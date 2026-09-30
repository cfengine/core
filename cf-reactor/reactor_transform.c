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
#include <file_watcher.h>
#include <agent_operations.h>   // ScheduleAgentOperations()

/* Promise types evaluated within `bundle reactor NAME { ... }`. */
static const char *const REACTOR_TYPESEQUENCE[] =
{
    "meta",
    "vars",
    "classes",
    "events",
    NULL
};

/* Resolves the agent or common bundle referred to by an events promise's
 * 'then' attribute, either `then => "name"` or `then => name(args)`. If args is
 * not NULL, it is set to the bundle arguments (or NULL if there are none).
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

/* Identifies the watcher of one iteration of an events promise */
static char *EventsPromiseKey(const Promise *pp)
{
    const Bundle *bp = PromiseGetBundle(pp);
    return StringFormat("%s:%s:%s", bp->ns, bp->name, pp->promiser);
}

// Temporary limitations:
// - 'when' body: only a single constraint, 'file_deleted', is supported (no OR-ing of constraints yet)
// - 'then': only a single bundle is supported, not a list of bundles yet
// TODO: support multiple 'when' constraints and a list of 'then' bundles
static PromiseResult KeepEventsPromise(EvalContext *ctx, const Promise *pp)
{
    assert(pp != NULL);

    char *key = EventsPromiseKey(pp);

    const char *path = PromiseGetConstraintAsRval(pp, "file_deleted", RVAL_TYPE_SCALAR);
    if (path == NULL)
    {
        Log(LOG_LEVEL_WARNING,
            "Reactor events promise '%s' must specify exactly one file to watch in its 'when' body, ignoring",
            key);
        free(key);
        return PROMISE_RESULT_FAIL;
    }

    const Constraint *then_constraint = PromiseGetConstraint(pp, "then");
    if (then_constraint == NULL)
    {
        Log(LOG_LEVEL_ERR, "Reactor events promise '%s' does not specify a 'then' bundle, ignoring", key);
        free(key);
        return PROMISE_RESULT_FAIL;
    }

    if (ResolveThenBundle(ctx, pp, then_constraint->rval, NULL) == NULL)
    {
        free(key);
        return PROMISE_RESULT_FAIL;
    }

    // register watcher
    Log(LOG_LEVEL_INFO, "Registering a file_deleted watcher with key '%s', on file '%s'", key, path);
    bool kept = WatcherRegister(key, EVENT_FILE_DELETED, FileWatcherStateNew(path), pp->org_pp, 1);
    free(key);

    return (kept) ? PROMISE_RESULT_NOOP : PROMISE_RESULT_FAIL;
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
        return KeepEventsPromise(ctx, pp);
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

/* Runs the bundle referred to by the 'then' attribute of the (expanded)
 * events promise. */
static PromiseResult RunThenBundle(EvalContext *ctx, const Promise *pp)
{
    assert(pp != NULL);

    const Constraint *then_constraint = PromiseGetConstraint(pp, "then");
    if (then_constraint == NULL)
    {
        Log(LOG_LEVEL_ERR, "Reactor events promise '%s' does not specify a 'then' bundle", pp->promiser);
        return PROMISE_RESULT_FAIL;
    }

    const Rlist *args = NULL;
    const Bundle *bp = ResolveThenBundle(ctx, pp, then_constraint->rval, &args);
    if (bp == NULL)
    {
        return PROMISE_RESULT_FAIL;
    }


    BundleBanner(bp, args);
    EvalContextSetBundleArgs(ctx, args);
    EvalContextStackPushBundleFrame(ctx, bp, args, false, NULL);

    PromiseResult result = ScheduleAgentOperations(ctx, bp);

    EvalContextStackPopFrame(ctx);
    EvalContextSetBundleArgs(ctx, NULL);
    EndBundleBanner(bp);

    if (EvalAborted(ctx))
    {
        Log(LOG_LEVEL_ERR, "Eval aborted");
    }

    return result;
}

typedef struct
{
    const char *key;
} ReactorEventParam;

/* Promise actuator for an events promise whose watcher fired. Only acts on
 * the iteration the watcher was registered for. */
static PromiseResult KeepEventsPromiseOnEvent(EvalContext *ctx, const Promise *pp, void *param)
{
    assert(pp != NULL);
    assert(param != NULL);
    const ReactorEventParam *event = param;

    char *key = EventsPromiseKey(pp);
    const bool is_event_iteration = StringEqual(key, event->key);
    free(key);
    if (!is_event_iteration)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    return RunThenBundle(ctx, pp);
}

void HandleReactorEvent(EvalContext *ctx, const Promise *pp, const char *key)
{
    assert(pp != NULL);
    assert(key != NULL);

    ReactorEventParam event = { .key = key };

    EvalContextStackPushBundleFrame(ctx, PromiseGetBundle(pp), NULL, false, NULL);
    EvalContextStackPushBundleSectionFrame(ctx, pp->parent_section);
    ExpandPromise(ctx, pp, KeepEventsPromiseOnEvent, &event);
    EvalContextStackPopFrame(ctx);
    EvalContextStackPopFrame(ctx);
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
}
