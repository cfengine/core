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

#include "constraints.h"
#include "promises.h"
#include "vars.h"

static void FindFilePromiserObjects(Promise *pp, const ReportContext *report_context);

/*****************************************************************************/

void *FindAndVerifyFilesPromises(Promise *pp, const ReportContext *report_context)
{
    PromiseBanner(pp);
    FindFilePromiserObjects(pp, report_context);

    if (AM_BACKGROUND_PROCESS && !pp->done)
    {
        CfOut(cf_verbose, "", "Exiting backgrounded promise");
        PromiseRef(cf_verbose, pp);
        exit(0);
    }

    return (void *) NULL;
}

/*****************************************************************************/

static void FindFilePromiserObjects(Promise *pp, const ReportContext *report_context)
{
    char *val = GetConstraintValue("pathtype", pp, CF_SCALAR);
    int literal = GetBooleanConstraint("copy_from", pp) || ((val != NULL) && (strcmp(val, "literal") == 0));

/* Check if we are searching over a regular expression */

    if (literal)
    {
        // Prime the promiser temporarily, may override later
        NewScalar("this", "promiser", pp->promiser, cf_str);
        VerifyFilePromise(pp->promiser, pp, report_context);
    }
    else                        // Default is to expand regex paths
    {
        LocateFilePromiserGroup(pp->promiser, pp, VerifyFilePromise, report_context);
    }
}
