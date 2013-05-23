/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "verify_processes.h"

#include "processes_select.h"
#include "env_context.h"
#include "promises.h"
#include "vars.h"
#include "item_lib.h"
#include "conversion.h"
#include "matching.h"
#include "attributes.h"
#include "locks.h"
#include "exec_tools.h"
#include "rlist.h"
#include "policy.h"
#include "scope.h"
#include "ornaments.h"

static void VerifyProcesses(EvalContext *ctx, Attributes a, Promise *pp);
static int ProcessSanityChecks(Attributes a, Promise *pp);
static void VerifyProcessOp(EvalContext *ctx, Item *procdata, Attributes a, Promise *pp);
static int FindPidMatches(Item *procdata, Item **killlist, Attributes a, const char *promiser);

void VerifyProcessesPromise(EvalContext *ctx, Promise *pp)
{
    Attributes a = { {0} };

    a = GetProcessAttributes(ctx, pp);
    ProcessSanityChecks(a, pp);

    VerifyProcesses(ctx, a, pp);
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static int ProcessSanityChecks(Attributes a, Promise *pp)
{
    int promised_zero, ret = true;

    promised_zero = ((a.process_count.min_range == 0) && (a.process_count.max_range == 0));

    if (a.restart_class)
    {
        if ((RlistIsStringIn(a.signals, "term")) || (RlistIsStringIn(a.signals, "kill")))
        {
            Log(LOG_LEVEL_WARNING, "Promise '%s' kills then restarts - never strictly converges",
                  pp->promiser);
            PromiseRef(LOG_LEVEL_INFO, pp);
        }

        if (a.haveprocess_count)
        {
            Log(LOG_LEVEL_ERR,
                  "process_count and restart_class should not be used in the same promise as this makes no sense");
            PromiseRef(LOG_LEVEL_INFO, pp);
            ret = false;
        }
    }

    if (promised_zero && (a.restart_class))
    {
        Log(LOG_LEVEL_ERR, "Promise constraint conflicts - '%s' processes cannot have zero count if restarted",
              pp->promiser);
        PromiseRef(LOG_LEVEL_ERR, pp);
        ret = false;
    }

    if ((a.haveselect) && (!a.process_select.process_result))
    {
        Log(LOG_LEVEL_ERR, "Process select constraint body promised no result (check body definition)");
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }

    return ret;
}

/*****************************************************************************/

static void VerifyProcesses(EvalContext *ctx, Attributes a, Promise *pp)
{
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    if (a.restart_class)
    {
        snprintf(lockname, CF_BUFSIZE - 1, "proc-%s-%s", pp->promiser, a.restart_class);
    }
    else
    {
        snprintf(lockname, CF_BUFSIZE - 1, "proc-%s-norestart", pp->promiser);
    }

    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, false);

    if (thislock.lock == NULL)
    {
        return;
    }

    ScopeDeleteSpecial("this", "promiser");
    ScopeNewSpecial(ctx, "this", "promiser", pp->promiser, DATA_TYPE_STRING);
    PromiseBanner(pp);
    VerifyProcessOp(ctx, PROCESSTABLE, a, pp);
    ScopeDeleteSpecial("this", "promiser");

    YieldCurrentLock(thislock);
}

static void VerifyProcessOp(EvalContext *ctx, Item *procdata, Attributes a, Promise *pp)
{
    int matches = 0, do_signals = true, out_of_range, killed = 0, need_to_restart = true;
    Item *killlist = NULL;

    matches = FindPidMatches(procdata, &killlist, a, pp->promiser);

/* promise based on number of matches */

    if (a.process_count.min_range != CF_NOINT)  /* if a range is specified */
    {
        if ((matches < a.process_count.min_range) || (matches > a.process_count.max_range))
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Process count for '%s' was out of promised range (%d found)", pp->promiser, matches);
            for (const Rlist *rp = a.process_count.out_of_range_define; rp != NULL; rp = rp->next)
            {
                if (!EvalContextHeapContainsSoft(ctx, rp->item))
                {
                    EvalContextHeapAddSoft(ctx, rp->item, PromiseGetNamespace(pp));
                }
            }
            out_of_range = true;
        }
        else
        {
            for (const Rlist *rp = a.process_count.in_range_define; rp != NULL; rp = rp->next)
            {
                if (!EvalContextHeapContainsSoft(ctx, rp->item))
                {
                    EvalContextHeapAddSoft(ctx, rp->item, PromiseGetNamespace(pp));
                }
            }
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Process promise for '%s' is kept", pp->promiser);
            out_of_range = false;
        }
    }
    else
    {
        out_of_range = true;
    }

    if (!out_of_range)
    {
        return;
    }

    if (a.transaction.action == cfa_warn)
    {
        do_signals = false;
    }
    else
    {
        do_signals = true;
    }

/* signal/kill promises for existing matches */

    if (do_signals && (matches > 0))
    {
        if (a.process_stop != NULL)
        {
            if (DONTDO)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a,
                     "Need to keep process-stop promise for '%s', but only a warning is promised", pp->promiser);
            }
            else
            {
                if (IsExecutable(CommandArg0(a.process_stop)))
                {
                    ShellCommandReturnsZero(a.process_stop, SHELL_TYPE_NONE);
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_FAIL, pp, a,
                         "Process promise to stop '%s' could not be kept because '%s' the stop operator failed",
                         pp->promiser, a.process_stop);
                    DeleteItemList(killlist);
                    return;
                }
            }
        }

        killed = DoAllSignals(ctx, killlist, a, pp);
    }

/* delegated promise to restart killed or non-existent entries */

    need_to_restart = (a.restart_class != NULL) && (killed || (matches == 0));

    DeleteItemList(killlist);

    if (!need_to_restart)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "No restart promised for %s", pp->promiser);
        return;
    }
    else
    {
        if (a.transaction.action == cfa_warn)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a,
                 "Need to keep restart promise for '%s', but only a warning is promised", pp->promiser);
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Making a one-time restart promise for '%s'", pp->promiser);
            EvalContextHeapAddSoft(ctx, a.restart_class, PromiseGetNamespace(pp));
        }
    }
}

#ifndef __MINGW32__
int DoAllSignals(EvalContext *ctx, Item *siglist, Attributes a, Promise *pp)
{
    Item *ip;
    Rlist *rp;
    pid_t pid;
    int killed = false;

    if (siglist == NULL)
    {
        return 0;
    }

    if (a.signals == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "No signals to send for '%s'", pp->promiser);
        return 0;
    }

    for (ip = siglist; ip != NULL; ip = ip->next)
    {
        pid = ip->counter;

        for (rp = a.signals; rp != NULL; rp = rp->next)
        {
            int signal = SignalFromString(rp->item);

            if (!DONTDO)
            {
                if ((signal == SIGKILL) || (signal == SIGTERM))
                {
                    killed = true;
                }

                if (kill((pid_t) pid, signal) < 0)
                {
                    cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_FAIL, pp, a,
                         "Couldn't send promised signal '%s' (%d) to pid %jd (might be dead). (kill: %s)", RlistScalarValue(rp),
                         signal, (intmax_t)pid, GetErrorStr());
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Signalled '%s' (%d) to process %jd (%s)",
                         RlistScalarValue(rp), signal, (intmax_t)pid, ip->name);
                }
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Need to keep signal promise '%s' in process entry '%s'",
                      RlistScalarValue(rp), ip->name);
            }
        }
    }

    return killed;
}
#endif

static int FindPidMatches(Item *procdata, Item **killlist, Attributes a, const char *promiser)
{
    int matches = 0;
    pid_t cfengine_pid = getpid();

    Item *matched = SelectProcesses(procdata, promiser, a.process_select, a.haveselect);

    for (Item *ip = matched; ip != NULL; ip = ip->next)
    {
        if (a.transaction.action == cfa_warn)
        {
            Log(LOG_LEVEL_ERR, "Matched '%s'", ip->name);
        }
        else
        {
            Log(LOG_LEVEL_INFO, "Matched '%s'", ip->name);
        }

        pid_t pid = ip->counter;

        if (pid == 1)
        {
            if ((RlistLen(a.signals) == 1) && (RlistIsStringIn(a.signals, "hup")))
            {
                Log(LOG_LEVEL_VERBOSE, "Okay to send only HUP to init");
            }
            else
            {
                continue;
            }
        }

        if ((pid < 4) && (a.signals))
        {
            Log(LOG_LEVEL_VERBOSE, "Will not signal or restart processes 0,1,2,3 (occurred while looking for %s)",
                  promiser);
            continue;
        }

        bool promised_zero = (a.process_count.min_range == 0) && (a.process_count.max_range == 0);

        if ((a.transaction.action == cfa_warn) && promised_zero)
        {
            Log(LOG_LEVEL_ERR, "Process alert '%s'", procdata->name);     /* legend */
            Log(LOG_LEVEL_ERR, "Process alert '%s'", ip->name);
            continue;
        }

        if ((pid == cfengine_pid) && (a.signals))
        {
            Log(LOG_LEVEL_VERBOSE, "cf-agent will not signal itself!");
            continue;
        }

        PrependItem(killlist, ip->name, "");
        (*killlist)->counter = pid;
        matches++;
    }

    DeleteItemList(matched);

    return matches;
}
