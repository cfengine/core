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

/* Promise types evaluated within `bundle reactor NAME { ... }`. */
static const char *const REACTOR_TYPESEQUENCE[] =
{
    "meta",
    "vars",
    "classes",
    "events",
    NULL
};

/* Checks that the bundle referred to by an events promise's 'then' attribute,
 * either `then => "name"` or `then => name(args)`, is defined as an agent or
 * common bundle. Logs an error and returns false if it is not. */
static bool ThenBundleExists(const EvalContext *ctx, const Promise *pp, Rval then_rval, const char *key)
{
    const char *bundle_name = NULL;
    switch (then_rval.type)
    {
    case RVAL_TYPE_SCALAR:
        bundle_name = RvalScalarValue(then_rval);
        break;
    case RVAL_TYPE_FNCALL:
        bundle_name = RvalFnCallValue(then_rval)->name;
        break;
    default:
        break;
    }

    const Bundle *bundle = NULL;
    if (bundle_name != NULL)
    {
        bundle = EvalContextResolveBundleExpression(ctx, PromiseGetPolicy(pp), bundle_name, "agent");
        if (bundle == NULL)
        {
            bundle = EvalContextResolveBundleExpression(ctx, PromiseGetPolicy(pp), bundle_name, "common");
        }
    }
    if (bundle == NULL)
    {
        Log(LOG_LEVEL_ERR, "Reactor events promise '%s' refers to unknown bundle '%s', ignoring",
            key, (bundle_name != NULL) ? bundle_name : "(invalid)");
        return false;
    }

    return true;
}

// Temporary limitations:
// - 'when' body: only a single constraint, 'file_deleted', is supported (no OR-ing of constraints yet)
// - 'then': only a single bundle is supported, not a list of bundles yet
// TODO: support multiple 'when' constraints and a list of 'then' bundles
static PromiseResult KeepEventsPromise(EvalContext *ctx, const Promise *pp)
{
    assert(pp != NULL);

    const Bundle *bp = PromiseGetBundle(pp);
    char *key = StringFormat("%s:%s:%s", bp->ns, bp->name, pp->promiser);

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

    if (!ThenBundleExists(ctx, pp, then_constraint->rval, key))
    {
        free(key);
        return PROMISE_RESULT_FAIL;
    }

    // register watcher
    Log(LOG_LEVEL_INFO, "Registering a file_deleted watcher with key '%s', on file '%s'", key, path);
    bool kept = WatcherRegister(key, EVENT_FILE_DELETED, FileWatcherStateNew(path), then_constraint->rval, 1);
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

void KeepReactorPromises(EvalContext *ctx, const Policy *policy)
{
    assert(policy != NULL);
    WatcherRegistryClear();

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
