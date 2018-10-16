/*
   Copyright 2018 Northern.tech AS

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

#include <audit.h>
#include <misc_lib.h>
#include <conversion.h>
#include <logging.h>
#include <string_lib.h>
#include <cleanup.h>

int PR_KEPT = 0; /* GLOBAL_X */
int PR_REPAIRED = 0; /* GLOBAL_X */
int PR_NOTKEPT = 0; /* GLOBAL_X */

static bool END_AUDIT_REQUIRED = false; /* GLOBAL_X */

void BeginAudit()
{
    END_AUDIT_REQUIRED = true;
}

void UpdatePromiseCounters(PromiseResult status)
{
    switch (status)
    {
    case PROMISE_RESULT_CHANGE:
        PR_REPAIRED++;
        break;

    case PROMISE_RESULT_NOOP:
        PR_KEPT++;
        break;

    case PROMISE_RESULT_WARN:
    case PROMISE_RESULT_TIMEOUT:
    case PROMISE_RESULT_FAIL:
    case PROMISE_RESULT_DENIED:
    case PROMISE_RESULT_INTERRUPTED:
        PR_NOTKEPT++;
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

    double total = (double) (PR_KEPT + PR_NOTKEPT + PR_REPAIRED) / 100.0;

    const char *version = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_VERSION);
    if (!version)
    {
        version = "(not specified)";
    }

    if (total == 0)
    {
        Log(LOG_LEVEL_VERBOSE, "Outcome of version '%s', no checks were scheduled", version);
        return;
    }
    else
    {
        LogTotalCompliance(version, background_tasks);
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
        Log(LOG_LEVEL_ERR, "Fatal CFEngine error: %s", buf);
    }

    EndAudit(ctx, 0);
    /* calling abort would bypass cleanup handlers and trigger subtle bugs */
    DoCleanupAndExit(EXIT_FAILURE);
}
