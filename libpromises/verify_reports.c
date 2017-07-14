/*
   Copyright 2017 Northern.tech AS

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

#include <cf3.defs.h>

#include <dbm_api.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <item_lib.h>
#include <vars.h>
#include <sort.h>
#include <attributes.h>
#include <communication.h>
#include <locks.h>
#include <logging.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <file_lib.h>
#include <policy.h>
#include <scope.h>
#include <ornaments.h>
#include <eval_context.h>
#include <actuator.h>

static bool PrintFile(const char *filename, size_t max_lines);
static void ReportToFile(const char *logfile, const char *message);
static void ReportToLog(const char *message);

PromiseResult VerifyReportPromise(EvalContext *ctx, const Promise *pp)
{
    CfLock thislock;
    char unique_name[CF_EXPANDSIZE];

    Attributes a = GetReportsAttributes(ctx, pp);

    // We let AcquireLock worry about making a unique name
    snprintf(unique_name, CF_EXPANDSIZE - 1, "%s", pp->promiser);
    thislock = AcquireLock(ctx, unique_name, VUQNAME, CFSTARTTIME, a.transaction, pp, false);

    // Handle return values before locks, as we always do this

    if (a.report.result)
    {
        // User-unwritable value last-result contains the useresult
        if (strlen(a.report.result) > 0)
        {
            snprintf(unique_name, CF_BUFSIZE, "last-result[%s]", a.report.result);
        }
        else
        {
            snprintf(unique_name, CF_BUFSIZE, "last-result");
        }

        VarRef *ref = VarRefParseFromBundle(unique_name, PromiseGetBundle(pp));
        EvalContextVariablePut(ctx, ref, pp->promiser, CF_DATA_TYPE_STRING, "source=bundle");
        VarRefDestroy(ref);

        if (thislock.lock)
        {
            YieldCurrentLock(thislock);
        }
        return PROMISE_RESULT_NOOP;
    }

    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    PromiseBanner(ctx, pp);

    if (a.transaction.action == cfa_warn)
    {
        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a, "Need to repair reports promise: %s", pp->promiser);
        YieldCurrentLock(thislock);
        return PROMISE_RESULT_WARN;
    }

    if (a.report.to_file)
    {
        ReportToFile(a.report.to_file, pp->promiser);
    }
    else
    {
        ReportToLog(pp->promiser);
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (a.report.haveprintfile)
    {
        if (!PrintFile(a.report.filename, a.report.numlines))
        {
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }
    }

    YieldCurrentLock(thislock);

    ClassAuditLog(ctx, pp, a, result);
    return result;
}

static void ReportToLog(const char *message)
{
    char *report_message;
    xasprintf(&report_message, "R: %s", message);

    fputs(report_message, stdout);
    fputc('\n', stdout);
    LogToSystemLog(report_message, LOG_LEVEL_NOTICE);

    free(report_message);
}

static void ReportToFile(const char *logfile, const char *message)
{
    FILE *fp = safe_fopen(logfile, "a");
    if (!fp)
    {
        Log(LOG_LEVEL_ERR, "Could not open log file '%s', message '%s'. (fopen: %s)", logfile, message, GetErrorStr());
    }
    else
    {
        fprintf(fp, "%s\n", message);
        fclose(fp);
    }
}

static bool PrintFile(const char *filename, size_t max_lines)
{
    if (!filename)
    {
        Log(LOG_LEVEL_VERBOSE, "Printfile promise was incomplete, with no filename.");
        return false;
    }

    FILE *fp = safe_fopen(filename, "r");
    if (!fp)
    {
        Log(LOG_LEVEL_ERR, "Printing of file '%s' was not possible. (fopen: %s)", filename, GetErrorStr());
        return false;
    }

    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);

    for (size_t i = 0; i < max_lines; i++)
    {
        if (CfReadLine(&line, &line_size, fp) == -1)
        {
            if (ferror(fp))
            {
                Log(LOG_LEVEL_ERR, "Failed to read line from stream, (getline: %s)", GetErrorStr());
                free(line);
                return false;
            }
            else
            {
                break;
            }
        }

        ReportToLog(line);
    }

    fclose(fp);
    free(line);

    return true;
}
