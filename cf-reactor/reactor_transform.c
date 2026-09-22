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

// Temporary implementation of KeepEventsPromise that doesn't reflect how we have defined when bodies:
// 1. We only expect one constraint, "file_deleted"
// 2. The original idea was that we could "or" constraints that are inside the same when body
// TODO: adapt this function to the original design
static void KeepEventsPromise(EvalContext *ctx, const Promise *pp)
{
    const char *key = pp->promiser;

    const char *path = PromiseGetConstraintAsRval(pp, "file_deleted", RVAL_TYPE_SCALAR);
    if (path == NULL)
    {
        Log(LOG_LEVEL_WARNING,
            "Reactor events promise '%s' must specify exactly one file to watch in its 'when' body, ignoring",
            key);
        return;
    }

    Constraint *then_constraint = PromiseGetConstraint(pp, "then");

    // TODO: Call WatcherRegister. We have here all the information we need (event type, state, events promiser)
    Log(LOG_LEVEL_INFO, "Registering a file_deleted watcher with key '%s', on file '%s'", key, path);
    WatcherRegister(key, EVENT_FILE_DELETED, FileWatcherPayloadNew(path), then_constraint->rval, 1);
}

static PromiseResult KeepReactorPromise(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert(param == NULL);
    PromiseBanner(ctx, pp);

    if (strcmp(PromiseGetPromiseType(pp), "vars") == 0 ||
        strcmp(PromiseGetPromiseType(pp), "meta") == 0)
    {
        return VerifyVarPromise(ctx, pp, NULL);
    }

    if (strcmp(PromiseGetPromiseType(pp), "classes") == 0)
    {
        return VerifyClassPromise(ctx, pp, NULL);
    }

    if (strcmp(PromiseGetPromiseType(pp), "events") == 0)
    {
        KeepEventsPromise(ctx, pp);
        return PROMISE_RESULT_NOOP;
    }

    return PROMISE_RESULT_NOOP;
}

static void EvaluateReactorBundle(EvalContext *ctx, const Bundle *bp)
{
    EvalContextStackPushBundleFrame(ctx, bp, NULL, false, NULL);

    for (int i = 0; REACTOR_TYPESEQUENCE[i] != NULL; i++)
    {
        const BundleSection *sp = BundleGetSection(bp, REACTOR_TYPESEQUENCE[i]);
        if (!sp || SeqLength(sp->promises) == 0)
        {
            Log(LOG_LEVEL_DEBUG, "No promise type %s in bundle %s",
                REACTOR_TYPESEQUENCE[i], bp->name);
            continue;
        }

        EvalContextStackPushBundleSectionFrame(ctx, sp);
        for (size_t j = 0; j < SeqLength(sp->promises); j++)
        {
            Promise *pp = SeqAt(sp->promises, j);
            ExpandPromise(ctx, pp, KeepReactorPromise, NULL);
        }
        EvalContextStackPopFrame(ctx);
    }

    EvalContextStackPopFrame(ctx);
}

void KeepReactorPromises(EvalContext *ctx, const Policy *policy)
{
    WatcherRegistryClear();

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);
        if (strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_REACTOR]) != 0)
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
