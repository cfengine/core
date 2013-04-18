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

#include "cf3.defs.h"

#include "dbm_api.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "item_lib.h"
#include "vars.h"
#include "sort.h"
#include "attributes.h"
#include "logging.h"
#include "communication.h"
#include "locks.h"
#include "string_lib.h"
#include "misc_lib.h"
#include "policy.h"
#include "scope.h"
#include "ornaments.h"
#include "env_context.h"

static void PrintFile(EvalContext *ctx, Attributes a, Promise *pp);
static void ReportToFile(const char *logfile, const char *message);

void VerifyReportPromise(EvalContext *ctx, Promise *pp)
{
    Attributes a = { {0} };
    CfLock thislock;
    char unique_name[CF_EXPANDSIZE];

    a = GetReportsAttributes(ctx, pp);

    snprintf(unique_name, CF_EXPANDSIZE - 1, "%s_%zu", pp->promiser, pp->offset.line);
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

        ScopeNewScalar(ctx, (VarRef) { NULL, PromiseGetBundle(pp)->name, unique_name }, pp->promiser, DATA_TYPE_STRING);
        return;
    }
       
    // Now do regular human reports
    
    if (thislock.lock == NULL)
    {
        return;
    }

    PromiseBanner(pp);

    if (a.transaction.action == cfa_warn)
    {
        cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_WARN, "", pp, a, "Need to repair reports promise: %s", pp->promiser);
        YieldCurrentLock(thislock);
        return;
    }

    cfPS(ctx, OUTPUT_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, "", pp, a, "Report: %s", pp->promiser);

    if (a.report.to_file)
    {
        ReportToFile(a.report.to_file, pp->promiser);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_REPORTING, "", "R: %s", pp->promiser);
    }

    if (a.report.haveprintfile)
    {
        PrintFile(ctx, a, pp);
    }

    if (a.report.showstate)
    {
        /* Do nothing. Deprecated. */
    }

    if (a.report.havelastseen)
    {
        /* Do nothing. Deprecated. */
    }

    YieldCurrentLock(thislock);
}

static void ReportToFile(const char *logfile, const char *message)
{
    FILE *fp = fopen(logfile, "a");
    if (fp == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "fopen", "Could not open log file %s\n", logfile);
        printf("%s\n", message);
    }
    else
    {
        fprintf(fp, "%s\n", message);
        fclose(fp);
    }
}

static void PrintFile(EvalContext *ctx, Attributes a, Promise *pp)
{
    FILE *fp;
    char buffer[CF_BUFSIZE];
    int lines = 0;

    if (a.report.filename == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Printfile promise was incomplete, with no filename.\n");
        return;
    }

    if ((fp = fopen(a.report.filename, "r")) == NULL)
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_INTERRUPTED, "fopen", pp, a, " !! Printing of file %s was not possible.\n", a.report.filename);
        return;
    }

    while ((lines < a.report.numlines))
    {
        if (fgets(buffer, CF_BUFSIZE, fp) == NULL)
        {
            if (ferror(fp))
            {
                UnexpectedError("Failed to read line from stream");
                break;
            }
            else /* feof */
            {
                break;
            }
        }
        CfOut(OUTPUT_LEVEL_ERROR, "", "R: %s", buffer);
        lines++;
    }

    fclose(fp);
}
