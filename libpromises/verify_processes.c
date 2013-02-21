/*
   Copyright (C) Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.

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
  versions of Cfengine, the applicable Commerical Open Source License
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
#include "cfstream.h"
#include "transaction.h"
#include "exec_tools.h"
#include "logging.h"
#include "rlist.h"
#include "policy.h"

static void VerifyProcesses(Attributes a, Promise *pp);
static int ProcessSanityChecks(Attributes a, Promise *pp);
static void VerifyProcessOp(Item *procdata, Attributes a, Promise *pp);

#ifndef __MINGW32__
static int DoAllSignals(Item *siglist, Attributes a, Promise *pp);
#endif


/*****************************************************************************/

void VerifyProcessesPromise(Promise *pp)
{
    Attributes a = { {0} };

    a = GetProcessAttributes(pp);
    ProcessSanityChecks(a, pp);

    VerifyProcesses(a, pp);
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
            CfOut(OUTPUT_LEVEL_INFORM, "", " -> (warning) Promise %s kills then restarts - never strictly converges",
                  pp->promiser);
            PromiseRef(OUTPUT_LEVEL_INFORM, pp);
        }

        if (a.haveprocess_count)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "",
                  " !! process_count and restart_class should not be used in the same promise as this makes no sense");
            PromiseRef(OUTPUT_LEVEL_INFORM, pp);
            ret = false;
        }
    }

    if (promised_zero && (a.restart_class))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Promise constraint conflicts - %s processes cannot have zero count if restarted",
              pp->promiser);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        ret = false;
    }

    if ((a.haveselect) && (!a.process_select.process_result))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Process select constraint body promised no result (check body definition)");
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        return false;
    }

    return ret;
}

/*****************************************************************************/

static void VerifyProcesses(Attributes a, Promise *pp)
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

    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, false);

    if (thislock.lock == NULL)
    {
        return;
    }

    DeleteScalar("this", "promiser");
    NewScalar("this", "promiser", pp->promiser, DATA_TYPE_STRING);
    PromiseBanner(pp);
    VerifyProcessOp(PROCESSTABLE, a, pp);
    DeleteScalar("this", "promiser");

    YieldCurrentLock(thislock);
}

static void VerifyProcessOp(Item *procdata, Attributes a, Promise *pp)
{
    int matches = 0, do_signals = true, out_of_range, killed = 0, need_to_restart = true;
    Item *killlist = NULL;

    CfDebug("VerifyProcessOp\n");

    matches = FindPidMatches(procdata, &killlist, a, pp);

/* promise based on number of matches */

    if (a.process_count.min_range != CF_NOINT)  /* if a range is specified */
    {
        if ((matches < a.process_count.min_range) || (matches > a.process_count.max_range))
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " !! Process count for \'%s\' was out of promised range (%d found)\n", pp->promiser, matches);
            AddEphemeralClasses(a.process_count.out_of_range_define, pp->ns);
            out_of_range = true;
        }
        else
        {
            AddEphemeralClasses(a.process_count.in_range_define, pp->ns);
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Process promise for %s is kept", pp->promiser);
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
                cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a,
                     " -- Need to keep process-stop promise for %s, but only a warning is promised", pp->promiser);
            }
            else
            {
                if (IsExecutable(CommandArg0(a.process_stop)))
                {
                    ShellCommandReturnsZero(a.process_stop, false);
                }
                else
                {
                    cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
                         "Process promise to stop %s could not be kept because %s the stop operator failed",
                         pp->promiser, a.process_stop);
                    DeleteItemList(killlist);
                    return;
                }
            }
        }

        killed = DoAllSignals(killlist, a, pp);
    }

/* delegated promise to restart killed or non-existent entries */

    need_to_restart = (a.restart_class != NULL) && (killed || (matches == 0));

    DeleteItemList(killlist);

    if (!need_to_restart)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> No restart promised for %s\n", pp->promiser);
        return;
    }
    else
    {
        if (a.transaction.action == cfa_warn)
        {
            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a,
                 " -- Need to keep restart promise for %s, but only a warning is promised", pp->promiser);
        }
        else
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, " -> Making a one-time restart promise for %s", pp->promiser);
            NewClass(a.restart_class, pp->ns);
        }
    }
}

#ifndef __MINGW32__
static int DoAllSignals(Item *siglist, Attributes a, Promise *pp)
{
    Item *ip;
    Rlist *rp;
    pid_t pid;
    int killed = false;

    CfDebug("DoSignals(%s)\n", pp->promiser);

    if (siglist == NULL)
    {
        return 0;
    }

    if (a.signals == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> No signals to send for %s\n", pp->promiser);
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
                    cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "kill", pp, a,
                         " !! Couldn't send promised signal \'%s\' (%d) to pid %jd (might be dead)\n", RlistScalarValue(rp),
                         signal, (intmax_t)pid);
                }
                else
                {
                    cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, " -> Signalled '%s' (%d) to process %jd (%s)\n",
                         RlistScalarValue(rp), signal, (intmax_t)pid, ip->name);
                }
            }
            else
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " -> Need to keep signal promise \'%s\' in process entry %s",
                      RlistScalarValue(rp), ip->name);
            }
        }
    }

    return killed;
}
#endif
