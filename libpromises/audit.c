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

#include "audit.h"
#include "misc_lib.h"
#include "conversion.h"
#include "logging_old.h"
#include "string_lib.h"

int PR_KEPT;
int PR_REPAIRED;
int PR_NOTKEPT;

static double VAL_KEPT;
static double VAL_REPAIRED;
static double VAL_NOTKEPT;

static bool END_AUDIT_REQUIRED = false;

#define CF_VALUE_LOG      "cf_value.log"

void BeginAudit()
{
    END_AUDIT_REQUIRED = true;
}

void UpdatePromiseCounters(PromiseResult status, TransactionContext tc)
{
    switch (status)
    {
    case PROMISE_RESULT_CHANGE:
        PR_REPAIRED++;
        VAL_REPAIRED += tc.value_repaired;
        break;

    case PROMISE_RESULT_NOOP:
        PR_KEPT++;
        VAL_KEPT += tc.value_kept;

    case PROMISE_RESULT_WARN:
    case PROMISE_RESULT_TIMEOUT:
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_DENIED:
    case PROMISE_RESULT_INTERRUPTED:
        PR_NOTKEPT++;
        VAL_NOTKEPT += tc.value_notkept;
        break;

    default:
        ProgrammingError("Unexpected status '%c' has been passed to UpdatePromiseCounters", status);
    }
}

void EndAudit(const EvalContext *ctx, int background_tasks)
{
    if (!END_AUDIT_REQUIRED)
    {
        return;
    }

    char *sp, string[CF_BUFSIZE];
    Rval retval = { 0 };

    {
        Rval track_value_rval = { 0 };
        bool track_value = false;
        if (EvalContextVariableGet(ctx, (VarRef) { NULL, "control_agent", CFA_CONTROLBODY[AGENT_CONTROL_TRACK_VALUE].lval }, &track_value_rval, NULL))
        {
            track_value = BooleanFromString(track_value_rval.item);
        }

        if (track_value)
        {
            FILE *fout;
            char name[CF_MAXVARSIZE], datestr[CF_MAXVARSIZE];
            time_t now = time(NULL);

            CfOut(OUTPUT_LEVEL_INFORM, "", " -> Recording promise valuations");

            snprintf(name, CF_MAXVARSIZE, "%s/state/%s", CFWORKDIR, CF_VALUE_LOG);
            snprintf(datestr, CF_MAXVARSIZE, "%s", ctime(&now));

            if ((fout = fopen(name, "a")) == NULL)
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", " !! Unable to write to the value log %s\n", name);
                return;
            }

            if (Chop(datestr, CF_EXPANDSIZE) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Chop was called on a string that seemed to have no terminator");
            }
            fprintf(fout, "%s,%.4lf,%.4lf,%.4lf\n", datestr, VAL_KEPT, VAL_REPAIRED, VAL_NOTKEPT);
            TrackValue(datestr, VAL_KEPT, VAL_REPAIRED, VAL_NOTKEPT);
            fclose(fout);
        }
    }

    double total = (double) (PR_KEPT + PR_NOTKEPT + PR_REPAIRED) / 100.0;

    if (EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_VERSION, &retval))
    {
        sp = (char *) retval.item;
    }
    else
    {
        sp = "(not specified)";
    }

    if (total == 0)
    {
        *string = '\0';
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Outcome of version %s: No checks were scheduled\n", sp);
        return;
    }
    else
    {
        LogTotalCompliance(sp, background_tasks);
    }
}

void FatalError(const EvalContext *ctx, char *s, ...)
{
    if (s)
    {
        va_list ap;
        char buf[CF_BUFSIZE] = "";

        va_start(ap, s);
        vsnprintf(buf, CF_BUFSIZE - 1, s, ap);
        va_end(ap);
        CfOut(OUTPUT_LEVEL_ERROR, "", "Fatal CFEngine error: %s", buf);
    }

    EndAudit(ctx, 0);
    exit(1);
}
