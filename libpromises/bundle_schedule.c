/*
  Copyright 2024 Northern.tech AS

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

#include <bundle_schedule.h>

#include <eval_context.h>
#include <expand.h>
#include <sequence.h>
#include <string_lib.h>
#include <instrumentation.h>
#include <ornaments.h>
#include <logging.h>

extern int PR_KEPT;
extern int PR_REPAIRED;
extern int PR_NOTKEPT;

/*********************************************************************/
/* Compliance comp                                                   */
/*********************************************************************/

static int NoteBundleCompliance(const Bundle *bundle, int save_pr_kept, int save_pr_repaired, int save_pr_notkept, struct timespec start)
{
    double delta_pr_kept, delta_pr_repaired, delta_pr_notkept;
    double bundle_compliance = 0.0;

    delta_pr_kept = (double) (PR_KEPT - save_pr_kept);
    delta_pr_notkept = (double) (PR_NOTKEPT - save_pr_notkept);
    delta_pr_repaired = (double) (PR_REPAIRED - save_pr_repaired);

    Log(LOG_LEVEL_VERBOSE, "A: ...................................................");
    Log(LOG_LEVEL_VERBOSE, "A: Bundle Accounting Summary for '%s' in namespace %s", bundle->name, bundle->ns);

    if (delta_pr_kept + delta_pr_notkept + delta_pr_repaired <= 0)
    {
        Log(LOG_LEVEL_VERBOSE, "A: Zero promises executed for bundle '%s'", bundle->name);
        Log(LOG_LEVEL_VERBOSE, "A: ...................................................");
        return PROMISE_RESULT_NOOP;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "A: Promises kept in '%s' = %.0lf", bundle->name, delta_pr_kept);
        Log(LOG_LEVEL_VERBOSE, "A: Promises not kept in '%s' = %.0lf", bundle->name, delta_pr_notkept);
        Log(LOG_LEVEL_VERBOSE, "A: Promises repaired in '%s' = %.0lf", bundle->name, delta_pr_repaired);

        bundle_compliance = (delta_pr_kept + delta_pr_repaired) / (delta_pr_kept + delta_pr_notkept + delta_pr_repaired);

        Log(LOG_LEVEL_VERBOSE, "A: Aggregate compliance (promises kept/repaired) for bundle '%s' = %.1lf%%",
          bundle->name, bundle_compliance * 100.0);

        if (LogGetGlobalLevel() >= LOG_LEVEL_INFO)
        {
            char name[CF_MAXVARSIZE];
            snprintf(name, CF_MAXVARSIZE, "%s:%s", bundle->ns, bundle->name);
            EndMeasure(name, start);
        }
        else
        {
            EndMeasure(NULL, start);
        }
        Log(LOG_LEVEL_VERBOSE, "A: ...................................................");
    }

    // return the worst case for the bundle status

    if (delta_pr_notkept > 0)
    {
        return PROMISE_RESULT_FAIL;
    }

    if (delta_pr_repaired > 0)
    {
        return PROMISE_RESULT_CHANGE;
    }

    return PROMISE_RESULT_NOOP;
}

/*********************************************************************/

PromiseResult ScheduleBundleOperationsNormalOrder(EvalContext *ctx, const Bundle *bp,
                                                   const char *const *type_sequence,
                                                   PromiseActuator *actuator,
                                                   BundleTypeContextFn *new_type_context,
                                                   BundleTypeContextCleanupFn *delete_type_context,
                                                   PromiseActuator *defaults_actuator)
{
    assert(bp != NULL);
    assert(type_sequence != NULL);
    assert(actuator != NULL);

    int save_pr_kept = PR_KEPT;
    int save_pr_repaired = PR_REPAIRED;
    int save_pr_notkept = PR_NOTKEPT;
    struct timespec start = BeginMeasure();

    PromiseResult result = PROMISE_RESULT_SKIPPED;

    for (int pass = 1; pass < CF_DONEPASSES; pass++)
    {
        // Evaluate built-in (non-custom) promise types, according to type sequence (normal order):
        for (TypeSequence type = 0; type_sequence[type] != NULL; type++)
        {
            const BundleSection *sp = BundleGetSection((Bundle *)bp, type_sequence[type]);

            if (!sp || SeqLength(sp->promises) == 0)
            {
                continue;
            }

            if (new_type_context != NULL)
            {
                new_type_context(type);
            }

            SpecialTypeBanner(type, pass);
            EvalContextStackPushBundleSectionFrame(ctx, sp);

            for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
            {
                Promise *pp = SeqAt(sp->promises, ppi);

                EvalContextSetPass(ctx, pass);

                PromiseResult promise_result = ExpandPromise(ctx, pp, actuator, NULL);
                result = PromiseResultUpdate(result, promise_result);

                if (EvalAborted(ctx) || BundleAbort(ctx))
                {
                    if (delete_type_context != NULL)
                    {
                        delete_type_context(ctx, type);
                    }
                    EvalContextStackPopFrame(ctx);
                    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept, start);
                    return result;
                }
            }

            if (delete_type_context != NULL)
            {
                delete_type_context(ctx, type);
            }
            EvalContextStackPopFrame(ctx);

            if (defaults_actuator != NULL && type == TYPE_SEQUENCE_CONTEXTS)
            {
                BundleResolve(ctx, bp);
                BundleResolvePromiseType(ctx, bp, "defaults", defaults_actuator);
            }
        }

        // Custom promises are evaluated at the end of an evaluation pass:
        const size_t sections = SeqLength(bp->custom_sections);
        for (size_t i = 0; i < sections; ++i)
        {
            BundleSection *section = SeqAt(bp->custom_sections, i);

            EvalContextStackPushBundleSectionFrame(ctx, section);

            const size_t promises = SeqLength(section->promises);
            for (size_t ppi = 0; ppi < promises; ppi++)
            {
                Promise *pp = SeqAt(section->promises, ppi);

                EvalContextSetPass(ctx, pass);

                PromiseResult promise_result = ExpandPromise(ctx, pp, actuator, NULL);
                result = PromiseResultUpdate(result, promise_result);

                if (EvalAborted(ctx) || BundleAbort(ctx))
                {
                    EvalContextStackPopFrame(ctx);
                    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept, start);
                    return result;
                }
            }
            EvalContextStackPopFrame(ctx);
        }
    }

    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept, start);
    return result;
}

PromiseResult ScheduleBundleOperationsTopDownOrder(EvalContext *ctx, const Bundle *bp, PromiseActuator *actuator)
{
    assert(bp != NULL);
    assert(actuator != NULL);

    int save_pr_kept = PR_KEPT;
    int save_pr_repaired = PR_REPAIRED;
    int save_pr_notkept = PR_NOTKEPT;
    struct timespec start = BeginMeasure();

    PromiseResult result = PROMISE_RESULT_SKIPPED;
    for (int pass = 1; pass < CF_DONEPASSES; pass++)
    {
        const char *last_promise_type = "";
        for (size_t ppi = 0; ppi < SeqLength(bp->all_promises); ppi++)
        {
            EvalContextSetPass(ctx, pass);
            Promise *pp = SeqAt(bp->all_promises, ppi);
            BundleSection *parent_section = pp->parent_section;

            if (!StringEqual(last_promise_type, parent_section->promise_type))
            {
                SpecialTypeBannerFromString(parent_section->promise_type, pass);
            }
            last_promise_type = parent_section->promise_type;

            EvalContextStackPushBundleSectionFrame(ctx, parent_section);

            PromiseResult promise_result = ExpandPromise(ctx, pp, actuator, NULL);
            result = PromiseResultUpdate(result, promise_result);
            if (EvalAborted(ctx) || BundleAbort(ctx))
            {
                EvalContextStackPopFrame(ctx);
                NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept, start);
                return result;
            }
            EvalContextStackPopFrame(ctx);
        }
    }

    NoteBundleCompliance(bp, save_pr_kept, save_pr_repaired, save_pr_notkept, start);
    return result;
}
