/*
  Copyright 2021 Northern.tech AS

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

#include <verify_processes.h>

#include <actuator.h>
#include <processes_select.h>
#include <eval_context.h>
#include <promises.h>
#include <class.h>
#include <vars.h>
#include <class.h>
#include <item_lib.h>
#include <conversion.h>
#include <matching.h>
#include <attributes.h>
#include <locks.h>
#include <exec_tools.h>
#include <rlist.h>
#include <policy.h>
#include <scope.h>
#include <ornaments.h>

static PromiseResult VerifyProcesses(EvalContext *ctx, const Attributes *a, const Promise *pp);
static bool ProcessSanityChecks(const Attributes *a, const Promise *pp);
static PromiseResult VerifyProcessOp(EvalContext *ctx, const Attributes *a, const Promise *pp);
static int FindPidMatches(Item **killlist, const Attributes *a, const char *promiser);

PromiseResult VerifyProcessesPromise(EvalContext *ctx, const Promise *pp)
{
    Attributes a = GetProcessAttributes(ctx, pp);
    ProcessSanityChecks(&a, pp);

    return VerifyProcesses(ctx, &a, pp);
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static bool ProcessSanityChecks(const Attributes *a, const Promise *pp)
{
    assert(a != NULL);
    assert(pp != NULL);
    // Note that an undefined match_value results in process_count.min_range and max_range being set to CF_NOINT
    bool promised_zero = ((a->process_count.min_range == 0) && (a->process_count.max_range == 0));
    bool ret = true;

    if (a->restart_class)
    {
        if ((RlistKeyIn(a->signals, "term")) || (RlistKeyIn(a->signals, "kill")))
        {
            Log(LOG_LEVEL_WARNING, "Promise '%s' kills then restarts - never strictly converges",
                pp->promiser);
            PromiseRef(LOG_LEVEL_INFO, pp);
        }

        if (a->haveprocess_count)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Note both process_count and restart_class define classes. Please exercises caution that there is not a logic error. Review process_count match_range, in_range_define, and out_of_range_define with respect to restart_class.");
            PromiseRef(LOG_LEVEL_VERBOSE, pp);
            ret = false;
        }
    }

    if (promised_zero && (a->restart_class))
    {
        Log(LOG_LEVEL_ERR, "Promise constraint conflicts - '%s' processes cannot have zero count if restarted",
            pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        ret = false;
    }

    if ((a->haveselect) && (!a->process_select.process_result))
    {
        Log(LOG_LEVEL_ERR, "Process select constraint body promised no result (check body definition)");
        PromiseRef(LOG_LEVEL_ERR, pp);
        ret = false;
    }

    // CF_NOINT is the value assigned when match_range is not specified
    if ((a->haveprocess_count) && ((a->process_count.min_range == CF_NOINT) || (a->process_count.max_range == CF_NOINT)))
    {
        Log(LOG_LEVEL_ERR, "process_count used without match_range, this promise will not define a class based on a count of matching promises.");
        PromiseRef(LOG_LEVEL_ERR, pp);
        ret = false;
    }

    if ((a->haveprocess_count) && !((a->process_count.in_range_define) || (a->process_count.out_of_range_define)))
    {
        Log(LOG_LEVEL_ERR, "process_count body is insufficiently defined. process_count must specify at least one of in_range_define or out_of_range_define. ");
        PromiseRef(LOG_LEVEL_ERR, pp);
        ret = false;
    }


    return ret;
}

/*****************************************************************************/

static PromiseResult VerifyProcesses(EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    if (a->restart_class)
    {
        snprintf(lockname, CF_BUFSIZE - 1, "proc-%s-%s", pp->promiser, a->restart_class);
    }
    else
    {
        snprintf(lockname, CF_BUFSIZE - 1, "proc-%s-norestart", pp->promiser);
    }

    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME,
        a->transaction.ifelapsed, a->transaction.expireafter, pp, false);
    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    PromiseBanner(ctx, pp);
    PromiseResult result = VerifyProcessOp(ctx, a, pp);

    YieldCurrentLock(thislock);

    return result;
}

static void ProcessCountMaybeDefineClass(
    EvalContext *const ctx,
    const Rlist *const rp,
    const Promise *const pp,
    const char *const attribute_name)
{
    assert(ctx != NULL);
    assert(rp != NULL);
    assert(pp != NULL);
    assert(attribute_name != NULL && strlen(attribute_name) > 0);

    if (rp->val.type == RVAL_TYPE_FNCALL)
    {
        Log(LOG_LEVEL_ERR,
            "Body (process_count), for processes promise %s, has a failing function call (%s) in the %s attribute - cannot define the class",
            pp->promiser,
            RlistFnCallValue(rp)->name,
            attribute_name);
        return;
    }

    assert(rp->val.type == RVAL_TYPE_SCALAR);
    ClassRef ref = ClassRefParse(RlistScalarValue(rp));
    EvalContextClassPutSoft(
        ctx, RlistScalarValue(rp), CONTEXT_SCOPE_NAMESPACE, "source=promise");
    ClassRefDestroy(ref);
}

static PromiseResult VerifyProcessOp(EvalContext *ctx, const Attributes *a, const Promise *pp)
{
    assert(a != NULL);
    assert(pp != NULL);

    Item *matched_procs = NULL;
    int matches = FindPidMatches(&matched_procs, a, pp->promiser);

    /* promise based on number of matches */

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (a->process_count.min_range != CF_NOINT)  /* if a range is specified */
    {
        if ((matches < a->process_count.min_range) || (matches > a->process_count.max_range))
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Process count for '%s' was out of promised range (%d found)", pp->promiser, matches);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            for (const Rlist *rp = a->process_count.out_of_range_define; rp != NULL; rp = rp->next)
            {
                ProcessCountMaybeDefineClass(ctx, rp, pp, "out_of_range_define");
            }
        }
        else
        {
            for (const Rlist *rp = a->process_count.in_range_define; rp != NULL; rp = rp->next)
            {
                ProcessCountMaybeDefineClass(ctx, rp, pp, "in_range_define");
            }
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Process promise for '%s' is kept", pp->promiser);
            DeleteItemList(matched_procs);
            return result;
        }
    }

    bool do_signals = (a->transaction.action != cfa_warn);
    if (!do_signals)
    {
        result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
    }

    /* signal/kill promises for existing matches */

    bool killed = false;
    if (do_signals && (matches > 0))
    {
        if (a->process_stop != NULL)
        {
            if (DONTDO)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a,
                     "Need to keep process-stop promise for '%s', but only a warning is promised", pp->promiser);
                result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
            }
            else
            {
                if (IsExecutable(CommandArg0(a->process_stop)))
                {
                    Log(LOG_LEVEL_DEBUG, "Found process_stop command '%s' is executable.", a->process_stop);

                    if (ShellCommandReturnsZero(a->process_stop, SHELL_TYPE_NONE))
                    {
                        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a,
                             "Promise to stop '%s' repaired, '%s' returned zero",
                             pp->promiser, a->process_stop);
                        result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
                    }
                    else
                    {
                        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                             "Promise to stop '%s' failed, '%s' returned nonzero",
                             pp->promiser, a->process_stop);
                        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    }
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                         "Process promise to stop '%s' could not be kept because '%s' is not executable",
                         pp->promiser, a->process_stop);
                    result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
                    DeleteItemList(matched_procs);
                    return result;
                }
            }
        }

        killed = DoAllSignals(ctx, matched_procs, a, pp, &result);
    }
    DeleteItemList(matched_procs);

    /* delegated promise to restart killed or non-existent entries */

    bool need_to_restart = (a->restart_class != NULL) && (killed || (matches == 0));
    if (!need_to_restart)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "No restart promised for %s", pp->promiser);
        return result;
    }
    else
    {
        if (a->transaction.action == cfa_warn)
        {
            cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a,
                 "Need to keep restart promise for '%s', but only a warning is promised", pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_WARN);
        }
        else
        {
            PromiseResult status = killed ? PROMISE_RESULT_CHANGE : PROMISE_RESULT_NOOP;
            cfPS(ctx, LOG_LEVEL_VERBOSE, status, pp, a,
                 "C:     +  Global class: %s ", a->restart_class);
            result = PromiseResultUpdate(result, status);
            EvalContextClassPutSoft(ctx, a->restart_class, CONTEXT_SCOPE_NAMESPACE, "source=promise");
        }
    }

    return result;
}

#ifndef __MINGW32__
bool DoAllSignals(EvalContext *ctx, Item *procs_to_signal, const Attributes *a, const Promise *pp, PromiseResult *result)
{
    Item *ip;
    Rlist *rp;
    pid_t pid;
    bool killed = false;
    bool failure = false;

    if (procs_to_signal == NULL)
    {
        return false;
    }

    if (a->signals == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "No signals to send for '%s'", pp->promiser);
        return false;
    }

    for (ip = procs_to_signal; ip != NULL; ip = ip->next)
    {
        pid = ip->counter;

        for (rp = a->signals; rp != NULL; rp = rp->next)
        {
            char *spec = RlistScalarValue(rp);

            int secs;
            char s;
            char next;
            int ret = sscanf(spec, "%d%c%c", &secs, &s, &next);
            if (((ret == 1) && (secs >= 0)) ||
                ((ret == 2) && (s == 's') && (secs >= 0)))
            {
                /* Some number or some number plus 's' given, and nothing more */
                sleep(secs);
                continue;
            }
            /* else something else given => consider it a signal name */

            int signal = SignalFromString(spec);

            if (!DONTDO)
            {
                if ((signal == SIGKILL) || (signal == SIGTERM))
                {
                    killed = true;
                }

                if (kill(pid, signal) < 0)
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, a,
                         "Couldn't send promised signal '%s' (%d) to pid %jd (might be dead). (kill: %s)",
                         spec, signal, (intmax_t)pid, GetErrorStr());
                    failure = true;
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a,
                         "Signalled '%s' (%d) to process %jd (%s)",
                         spec, signal, (intmax_t) pid, ip->name);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                    failure = false;
                }
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Need to keep signal promise '%s' in process entry '%s'",
                    spec, ip->name);
            }
        }
    }

    if (failure)
    {
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
    }

    return killed;
}
#endif

static int FindPidMatches(Item **killlist, const Attributes *a, const char *promiser)
{
    int matches = 0;
    pid_t cfengine_pid = getpid();

    Item *matched = SelectProcesses(promiser, &(a->process_select), a->haveselect);

    for (Item *ip = matched; ip != NULL; ip = ip->next)
    {
        pid_t pid = ip->counter;

        if (a->signals) /* There are some processes we don't want to signal. */
        {
            if (pid == 1)
            {
                if (RlistLen(a->signals) == 1 && RlistKeyIn(a->signals, "hup"))
                {
                    Log(LOG_LEVEL_VERBOSE, "Okay to send only HUP to init");
                }
                else
                {
                    continue;
                }
            }
            else if (pid < 4)
            {
                Log(LOG_LEVEL_VERBOSE, "Will not signal or restart processes 0,1,2,3 (occurred while looking for %s)",
                    promiser);
                continue;
            }

            if (pid == cfengine_pid)
            {
                Log(LOG_LEVEL_VERBOSE, "cf-agent will not signal itself!");
                continue;
            }
        }

        bool promised_zero = (a->process_count.min_range == 0) && (a->process_count.max_range == 0);

        if ((a->transaction.action == cfa_warn) && promised_zero)
        {
            Log(LOG_LEVEL_WARNING, "Process alert '%s'", GetProcessTableLegend());
            Log(LOG_LEVEL_WARNING, "Process alert '%s'", ip->name);
            continue;
        }

        if (a->transaction.action == cfa_warn)
        {
            Log(LOG_LEVEL_WARNING, "Matched '%s'", ip->name);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Matched '%s'", ip->name);
        }

        PrependItem(killlist, ip->name, "");
        (*killlist)->counter = pid;
        matches++;
    }

    DeleteItemList(matched);

    return matches;
}
