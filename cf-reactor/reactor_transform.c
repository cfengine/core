/*
  Copyright 2026 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; version 3.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

  To the extent this program is licensed as part of the Enterprise
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <reactor_transform.h>

#include <logging.h>
#include <expand.h>
#include <fncall.h>
#include <ornaments.h>
#include <rlist.h>
#include <sequence.h>
#include <verify_classes.h>
#include <verify_vars.h>
#include <watcher.h>
#include <agent_operations.h>   // ScheduleAgentOperations()
#include <attributes.h>          // GetClassContextAttributes()
#include <conn_cache.h>          // ConnCache_Init(), ConnCache_Destroy()
#include <mod_custom.h>          // Initialize/FinalizeCustomPromises()
#include <timeout.h>             // SetReferenceTime()
#include <time_classes.h>        // UpdateTimeClasses()
#include <mod_reactor.h>         // REACTOR_INTERVAL_RANGE, REACTOR_FILE_DELETED
#include <syntax.h>              // CheckConstraintTypeMatch(), PromiseTypeSyntaxGet(), BodySyntaxGetConstraintSyntax()
#include <file_watcher.h>
#include <conversion.h>

/* Promise types evaluated within `bundle reactor NAME { ... }`. */
static const char *const REACTOR_TYPESEQUENCE[] =
{
    "meta",
    "vars",
    "classes",
    "events",
    NULL
};

/* Resolves the agent or common bundle referred to by an entry of an events
 * promise's 'then' attribute, either "name" or name(args). If args is not
 * NULL, it is set to the bundle arguments (or NULL if there are none).
 * Logs an error and returns NULL if the bundle is not found. */
static const Bundle *ResolveThenBundle(
    const EvalContext *ctx, const Promise *pp, Rval then_rval, const Rlist **args)
{
    assert(pp != NULL);

    const char *name = NULL;
    const Rlist *bundle_args = NULL;
    switch (then_rval.type)
    {
    case RVAL_TYPE_SCALAR:
        name = RvalScalarValue(then_rval);
        break;
    case RVAL_TYPE_FNCALL:
        name = RvalFnCallValue(then_rval)->name;
        bundle_args = RvalFnCallValue(then_rval)->args;
        break;
    default:
        break;
    }

    const Bundle *bp = NULL;
    if (name != NULL)
    {
        bp = EvalContextResolveBundleExpression(ctx, PromiseGetPolicy(pp), name, "agent");
        if (bp == NULL)
        {
            bp = EvalContextResolveBundleExpression(ctx, PromiseGetPolicy(pp), name, "common");
        }
    }
    if (bp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Reactor events promise '%s' refers to unknown bundle '%s'",
            pp->promiser, (name != NULL) ? name : "(invalid)");
        return NULL;
    }

    if (args != NULL)
    {
        *args = bundle_args;
    }
    return bp;
}

/* Records the outcome of an iteration of an events promise: defines the
 * classes of its 'classes' body, and logs it like cf-agent does for its
 * promises. */
static void RecordEventsPromiseOutcome(EvalContext *ctx, const Promise *pp, PromiseResult result)
{
    assert(pp != NULL);

    Attributes a = GetClassContextAttributes(ctx, pp);
    ClassAuditLog(ctx, pp, &a, result);
    EvalContextLogPromiseIterationOutcome(ctx, pp, result);
}

/* Returns the list of bundle references of the (expanded) 'then' attribute,
 * which is always a list after expansion, see ExpandBundleListReference().
 * Logs an error and returns NULL if there is no 'then' attribute or it is
 * empty. */
static const Rlist *GetThenBundles(const Promise *pp)
{
    assert(pp != NULL);

    const Constraint *then_constraint = PromiseGetConstraint(pp, "then");
    if (then_constraint == NULL)
    {
        Log(LOG_LEVEL_ERR, "Reactor events promise '%s' does not specify a 'then' bundle", pp->promiser);
        return NULL;
    }

    assert(then_constraint->rval.type == RVAL_TYPE_LIST);
    const Rlist *bundles = RvalRlistValue(then_constraint->rval);
    if (bundles == NULL)
    {
        Log(LOG_LEVEL_ERR, "Reactor events promise '%s' has an empty list of 'then' bundles", pp->promiser);
    }
    return bundles;
}

/*****************************************************************************/

/* A constraint of the 'when' body passed to the watcher of an event, with the
 * type of value it accepts */
typedef struct
{
    const char *name;
    RvalType type;
} EventArg;

typedef struct
{
    const char *name;
    RvalType type;              /* type of value the 'name' constraint accepts */
    const EventArg *extra_args; /* terminated by an EventArg with a NULL name */
    WatcherStateNewFn new_state;
    WatcherStateDestroyFn destroy_state;
    WatcherCheckFn check;
} EventType;

static const EventArg NO_EXTRA_ARGS[] =
{
    { NULL, RVAL_TYPE_NOPROMISEE }
};

#define EventTypeNew(name, type, args, new_state, destroy_state, check) \
    { name, type, args, new_state, destroy_state, check }
#define EventTypeNewNull() EventTypeNew(NULL, RVAL_TYPE_NOPROMISEE, NULL, NULL, NULL, NULL)

/* A file event, about the file at the path of its constraint */
#define FileEventTypeNew(name, check) \
    EventTypeNew(name, RVAL_TYPE_SCALAR, NO_EXTRA_ARGS, FileWatcherStateNew, FileWatcherStateDestroy, check)

static const EventType EVENT_TABLE[] =
{
    FileEventTypeNew(REACTOR_FILE_DELETED, FileWatcherCheckFileDeleted),
    EventTypeNewNull()
};

/* Appends the value of constraint con of the events promise to args if it is
 * of the given type, otherwise logs an error and returns false. */
static bool EventArgAppend(const Promise *pp, const Constraint *con, RvalType type, Rlist **args)
{
    assert(pp != NULL);
    assert(con != NULL);
    assert(args != NULL);

    if (con->rval.type != type)
    {
        Log(LOG_LEVEL_ERR,
            "Reactor events promise '%s' has an invalid '%s', expected a %s value, got a %s value",
            pp->promiser, con->lval, RvalTypeToString(type), RvalTypeToString(con->rval.type));
        return false;
    }
    RlistAppend(args, con->rval.item, con->rval.type);
    return true;
}

static const EventType *EventsPromiseGetFirstEvent(const Promise *pp, Rlist **args)
{
    assert(pp != NULL);
    assert(args != NULL);

    for (size_t i = 0; EVENT_TABLE[i].name != NULL; i++)
    {
        const Constraint *con = PromiseGetConstraint(pp, EVENT_TABLE[i].name);
        if (con != NULL)
        {
            if (!EventArgAppend(pp, con, EVENT_TABLE[i].type, args))
            {
                return NULL;
            }
            return &EVENT_TABLE[i];
        }
    }
    Log(LOG_LEVEL_ERR,
        "Reactor events promise '%s' does not specify an event to watch in its 'when' body",
        pp->promiser);
    return NULL;
}

#define DEFAULT_INTERVAL 1

/* Sets *interval to the seconds between two checks for the event of the
 * (expanded) events promise. The parser and cf-promises only check it if it
 * doesn't depend on variables or body arguments, so it is checked again here
 * the same way: logs an error and returns false if it is invalid. */
static bool EventsPromiseGetInterval(const Promise *pp, time_t *interval)
{
    assert(pp != NULL);
    assert(interval != NULL);

    const Constraint *interval_con = PromiseGetConstraint(pp, "interval");
    if (interval_con == NULL)
    {
        *interval = DEFAULT_INTERVAL;
        return true;
    }

    if (interval_con->rval.type != RVAL_TYPE_SCALAR)
    {
        Log(LOG_LEVEL_ERR,
            "Reactor events promise '%s' has an invalid interval, expected a number of seconds",
            pp->promiser);
        return false;
    }

    const char *value = RvalScalarValue(interval_con->rval);
    const SyntaxTypeMatch err = CheckConstraintTypeMatch(
        interval_con->lval, interval_con->rval, CF_DATA_TYPE_INT, REACTOR_INTERVAL_RANGE, 0);
    if (err != SYNTAX_TYPE_MATCH_OK)
    {
        Log(LOG_LEVEL_ERR,
            "Reactor events promise '%s' has an invalid interval '%s': %s",
            pp->promiser, value, SyntaxTypeMatchToString(err));
        return false;
    }

    *interval = (time_t) IntFromString(value);
    return true;
}

static bool ResolveWhenBody(const Promise *pp, WatcherOptions *opt)
{
    assert(pp != NULL);
    assert(opt != NULL);
    // Get the event and its arg
    Rlist *args = NULL;
    const EventType *event = EventsPromiseGetFirstEvent(pp, &args);

    if (event == NULL)
    {
        RlistDestroy(args);
        return false;
    }

    // If the event has additional args, add them to the list of args
    for (const EventArg *extra_arg = event->extra_args; extra_arg->name != NULL; extra_arg++)
    {
        const Constraint *con = PromiseGetConstraint(pp, extra_arg->name);
        if (con == NULL)
        {
            Log(LOG_LEVEL_ERR,
                "Constraint '%s' is required for event '%s' in events promise '%s'",
                extra_arg->name, event->name, pp->promiser);
            RlistDestroy(args);
            return false;
        }
        if (!EventArgAppend(pp, con, extra_arg->type, &args))
        {
            RlistDestroy(args);
            return false;
        }
    }

    // Check that other constraints are not defined in the promise, that
    // do not belong here
    const PromiseTypeSyntax *events_syntax = PromiseTypeSyntaxGet("reactor", "events");
    assert(events_syntax != NULL);
    const ConstraintSyntax *when_syntax =
        PromiseTypeSyntaxGetConstraintSyntax(events_syntax, "when")->range.body_type_syntax->constraints;

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        const Constraint *cp = SeqAt(pp->conlist, i);
        if (BodySyntaxGetConstraintSyntax(when_syntax, cp->lval) == NULL || /* not a 'when' constraint */
            StringEqual(cp->lval, event->name) ||
            StringEqual(cp->lval, "interval") ||
            StringEqual(cp->lval, "meta"))                                 /* also a promise attribute */
        {
            continue;
        }

        bool own_arg = false;
        for (const EventArg *extra_arg = event->extra_args; extra_arg->name != NULL; extra_arg++)
        {
            own_arg = own_arg || StringEqual(extra_arg->name, cp->lval);
        }
        if (!own_arg)
        {
            Log(LOG_LEVEL_ERR,
                "Reactor events promise '%s' cannot have both '%s' and '%s' in its 'when' body, only one event can be watched per promise",
                pp->promiser, event->name, cp->lval);
            RlistDestroy(args);
            return false;
        }
    }

    if (!EventsPromiseGetInterval(pp, &opt->interval))
    {
        RlistDestroy(args);
        return false;
    }

    opt->state = event->new_state(args);
    opt->check_callback = event->check;
    opt->destroy_state = event->destroy_state;
    RlistDestroy(args);
    return true;
}

/*****************************************************************************/

static PromiseResult KeepEventsPromise(EvalContext *ctx, const Promise *pp)
{
    assert(pp != NULL);

    const Rlist *then_bundles = GetThenBundles(pp);
    if (then_bundles == NULL)
    {
        return PROMISE_RESULT_FAIL;
    }

    for (const Rlist *rp = then_bundles; rp != NULL; rp = rp->next)
    {
        if (ResolveThenBundle(ctx, pp, rp->val, NULL) == NULL)
        {
            return PROMISE_RESULT_FAIL;
        }
    }

    WatcherOptions opt;
    if (!ResolveWhenBody(pp, &opt))
    {
        return PROMISE_RESULT_FAIL;
    }

    if (!WatcherRegister(pp, &opt))
    {
        return PROMISE_RESULT_FAIL;
    }

    return PROMISE_RESULT_NOOP;
}

static PromiseResult KeepReactorPromise(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert(param == NULL);
    PromiseBanner(ctx, pp);

    if (StringEqual(PromiseGetPromiseType(pp), "vars") ||
        StringEqual(PromiseGetPromiseType(pp), "meta"))
    {
        return VerifyVarPromise(ctx, pp, NULL);
    }

    if (StringEqual(PromiseGetPromiseType(pp), "classes"))
    {
        return VerifyClassPromise(ctx, pp, NULL);
    }

    if (StringEqual(PromiseGetPromiseType(pp), "events"))
    {
        PromiseResult result = KeepEventsPromise(ctx, pp);
        RecordEventsPromiseOutcome(ctx, pp, result);
        return result;
    }

    return PROMISE_RESULT_NOOP;
}

static void EvaluateReactorBundle(EvalContext *ctx, const Bundle *bp)
{
    assert(bp != NULL);
    EvalContextStackPushBundleFrame(ctx, bp, NULL, false, NULL);

    for (int i = 0; REACTOR_TYPESEQUENCE[i] != NULL; i++)
    {
        const BundleSection *sp = BundleGetSection(bp, REACTOR_TYPESEQUENCE[i]);
        if (sp == NULL || SeqLength(sp->promises) == 0)
        {
            Log(LOG_LEVEL_DEBUG, "No promise type %s in bundle %s",
                REACTOR_TYPESEQUENCE[i], bp->name);
            continue;
        }

        EvalContextStackPushBundleSectionFrame(ctx, sp);
        for (size_t j = 0; j < SeqLength(sp->promises); j++)
        {
            const Promise *pp = SeqAt(sp->promises, j);
            ExpandPromise(ctx, pp, KeepReactorPromise, NULL);
        }
        EvalContextStackPopFrame(ctx);
    }

    EvalContextStackPopFrame(ctx);
}

/* Runs the bundle referred to by an entry of the 'then' attribute of the
 * (expanded) events promise. */
static PromiseResult RunThenBundle(EvalContext *ctx, const Promise *pp, Rval then_rval)
{
    assert(pp != NULL);

    const Rlist *args = NULL;
    const Bundle *bp = ResolveThenBundle(ctx, pp, then_rval, &args);
    if (bp == NULL)
    {
        return PROMISE_RESULT_FAIL;
    }

    BundleBanner(bp, args);
    EvalContextSetBundleArgs(ctx, args);
    EvalContextStackPushBundleFrame(ctx, bp, args, false, NULL);

    /* Remote copy_from needs the connection cache and custom promise types
     * need the promise modules map. Set them up per run like in cf-agent so
     * that no connections or promise modules are kept between events. */
    ConnCache_Init();
    InitializeCustomPromises();
    int prev_ifelapsed = OverrideIfelapsed(0);
    PromiseResult result = ScheduleAgentOperations(ctx, bp);
    RestoreIfelapsed(prev_ifelapsed);
    FinalizeCustomPromises();
    ConnCache_Destroy();

    EvalContextStackPopFrame(ctx); /* bundle */
    EvalContextSetBundleArgs(ctx, NULL);
    EndBundleBanner(bp);

    return result;
}

/* Runs the bundles referred to by the 'then' attribute of the (expanded)
 * events promise in order, like a bundlesequence. */
static PromiseResult RunThenBundles(EvalContext *ctx, const Promise *pp)
{
    assert(pp != NULL);

    const Rlist *then_bundles = GetThenBundles(pp);
    if (then_bundles == NULL)
    {
        return PROMISE_RESULT_FAIL;
    }

    /* The promise lock cache makes each promise act at most once per
     * EvalContext, and the function cache keeps the results of functions like
     * execresult(), but cf-reactor keeps the same EvalContext across events.
     * Clear them so every event gets a fresh run of the bundles. */
    EvalContextPromiseLockCacheClear(ctx);
    EvalContextFunctionCacheClear(ctx);

    PromiseResult result = PROMISE_RESULT_NOOP;
    for (const Rlist *rp = then_bundles; rp != NULL; rp = rp->next)
    {
        result = PromiseResultUpdate(result, RunThenBundle(ctx, pp, rp->val));
    }
    return result;
}

typedef struct
{
    const char *promise_hash;
    WatcherCheckResult check;
} ReactorEventParam;

/* Promise actuator for an events promise whose watcher fired, or failed to
 * check for its event. Only acts on the iteration the watcher was registered
 * for. */
static PromiseResult KeepEventsPromiseOnEvent(EvalContext *ctx, const Promise *pp, void *param)
{
    assert(pp != NULL);
    assert(param != NULL);
    const ReactorEventParam *event = param;

    char promise_hash[CF_HOSTKEY_STRING_SIZE];
    WatcherPromiseHash(pp, promise_hash, sizeof(promise_hash));
    if (!StringEqual(promise_hash, event->promise_hash))
    {
        return PROMISE_RESULT_SKIPPED;
    }

    return RunThenBundles(ctx, pp);
}

void HandleReactorEvent(EvalContext *ctx, const Promise *pp, const char *promise_hash, WatcherCheckResult check)
{
    assert(pp != NULL);
    assert(promise_hash != NULL);

    /* cf-reactor keeps the same EvalContext across events. Like an agent run,
     * the bundle runs see the time classes of now and the persistent classes
     * that have not expired. The classes and variables they define, cancel or
     * change are undone once the event is handled, except the persistent
     * classes, which are kept in the state database. */
    UpdateTimeClasses(ctx, SetReferenceTime());
    ClassTable *classes = NULL;
    VariableTable *variables = NULL;
    EvalContextSnapshotTake(ctx, &classes, &variables);

    ReactorEventParam event = { .promise_hash = promise_hash, .check = check };

    EvalContextStackPushBundleFrame(ctx, PromiseGetBundle(pp), NULL, false, NULL);
    EvalContextStackPushBundleSectionFrame(ctx, pp->parent_section);
    ExpandPromise(ctx, pp, KeepEventsPromiseOnEvent, &event);
    EvalContextStackPopFrame(ctx); /* bundle section */
    EvalContextStackPopFrame(ctx); /* bundle */

    EvalContextSnapshotRestore(ctx, classes, variables);
}

void KeepReactorPromises(EvalContext *ctx, const Policy *policy)
{
    assert(policy != NULL);

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        const Bundle *bp = SeqAt(policy->bundles, i);
        if (!StringEqual(bp->type, CF_AGENTTYPES[AGENT_TYPE_REACTOR]))
        {
            continue;
        }

        if (RlistLen(bp->args) > 0)
        {
            Log(LOG_LEVEL_WARNING,
                "Cannot implicitly evaluate bundle '%s %s', as this bundle takes arguments.",
                bp->type, bp->name);
            continue;
        }

        EvaluateReactorBundle(ctx, bp);
    }

    /* Tag the persistent classes the evaluation defined 'source=persistent',
     * so that the snapshots taken on events tell them apart even once another
     * agent deleted them from the state database, see EvalContextSnapshotTake() */
    EvalContextHeapPersistentLoadAll(ctx);
}
