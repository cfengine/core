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
#include "vars.h"
#include "attributes.h"
#include "cfstream.h"
#include "communication.h"
#include "transaction.h"
#include "string_lib.h"
#include "logging.h"
#include "misc_lib.h"
#include "policy.h"

static void PrintFile(Attributes a, Promise *pp);

/*******************************************************************/
/* Agent reporting                                                 */
/*******************************************************************/

void VerifyReportPromise(Promise *pp)
{
    Attributes a = { {0} };
    CfLock thislock;
    char unique_name[CF_EXPANDSIZE];

    a = GetReportsAttributes(pp);

    snprintf(unique_name, CF_EXPANDSIZE - 1, "%s_%zu", pp->promiser, pp->offset.line);
    thislock = AcquireLock(unique_name, VUQNAME, CFSTARTTIME, a, pp, false);

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

        NewScalar(pp->bundle, unique_name, pp->promiser, DATA_TYPE_STRING);
        return;
    }
       
    // Now do regular human reports
    
    if (thislock.lock == NULL)
    {
        return;
    }

    PromiseBanner(pp);

    cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, "Report: %s", pp->promiser);

    if (a.report.to_file)
    {
        CfFOut(a.report.to_file, OUTPUT_LEVEL_ERROR, "", "%s", pp->promiser);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_REPORTING, "", "R: %s", pp->promiser);
    }

    if (a.report.haveprintfile)
    {
        PrintFile(a, pp);
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

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

static void PrintFile(Attributes a, Promise *pp)
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
        cfPS(OUTPUT_LEVEL_ERROR, CF_INTERPT, "fopen", pp, a, " !! Printing of file %s was not possible.\n", a.report.filename);
        return;
    }

    while ((!feof(fp)) && (lines < a.report.numlines))
    {
        buffer[0] = '\0';
        if (fgets(buffer, CF_BUFSIZE, fp) == NULL)
        {
            if (strlen(buffer))
            {
                UnexpectedError("Failed to read line from stream");
            }
        }
        CfOut(OUTPUT_LEVEL_ERROR, "", "R: %s", buffer);
        lines++;
    }

    fclose(fp);
}
