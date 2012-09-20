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

/*****************************************************************************/
/*                                                                           */
/* File: files_editxml.c                                                     */
/*                                                                           */
/* Created: Thu Aug  9 10:16:53 2012                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"

#include "env_context.h"
#include "constraints.h"
#include "promises.h"
#include "files_names.h"
#include "vars.h"
#include "item_lib.h"
#include "sort.h"
#include "conversion.h"
#include "reporting.h"
#include "expand.h"
#include "scope.h"


/*****************************************************************************/

enum editxmltypesequence
{
    elx_vars,
    elx_classes,
    elx_delete,
    elx_insert,
    elx_none
};

char *EDITXMLTYPESEQUENCE[] =
{
    "vars",
    "classes",
    "delete_tree",
    "insert_tree",
    //"replace_patterns",
    "reports",
    NULL
};

static void EditXmlClassBanner(enum editxmltypesequence type);
static void KeepEditXmlPromise(Promise *pp);
static void VerifyTreeInsertions(Promise *pp);
static void VerifyTreeDeletions(Promise *pp);


/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

int ScheduleEditXmlOperations(char *filename, Bundle *bp, Attributes a, Promise *parentp,
                               const ReportContext *report_context)
{
    enum editxmltypesequence type;
    SubType *sp;
    Promise *pp;
    char lockname[CF_BUFSIZE];
    const char *bp_stack = THIS_BUNDLE;
    CfLock thislock;
    int pass;

    snprintf(lockname, CF_BUFSIZE - 1, "masterfilelock-%s", filename);
    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, parentp, true);

    if (thislock.lock == NULL)
    {
        return false;
    }

    NewScope("edit");
    NewScalar("edit", "filename", filename, cf_str);

/* Reset the done state for every call here, since bundle is reusable */

    for (type = 0; EDITXMLTYPESEQUENCE[type] != NULL; type++)
    {
        if ((sp = GetSubTypeForBundle(EDITXMLTYPESEQUENCE[type], bp)) == NULL)
        {
            continue;
        }

        for (pp = sp->promiselist; pp != NULL; pp = pp->next)
        {
            pp->donep = false;
        }
    }

    for (pass = 1; pass < CF_DONEPASSES; pass++)
    {
        for (type = 0; EDITXMLTYPESEQUENCE[type] != NULL; type++)
        {
            EditXmlClassBanner(type);

            if ((sp = GetSubTypeForBundle(EDITXMLTYPESEQUENCE[type], bp)) == NULL)
            {
                continue;
            }

            BannerSubSubType(bp->name, sp->name);
            THIS_BUNDLE = bp->name;
            SetScope(bp->name);

            for (pp = sp->promiselist; pp != NULL; pp = pp->next)
            {
                pp->edcontext = parentp->edcontext;
                pp->this_server = filename;
                pp->donep = &(pp->done);

                ExpandPromise(cf_agent, bp->name, pp, KeepEditXmlPromise, report_context);

                if (Abort())
                {
                    THIS_BUNDLE = bp_stack;
                    DeleteScope("edit");
                    YieldCurrentLock(thislock);
                    return false;
                }
            }
        }
    }

    DeleteScope("edit");
    SetScope(parentp->bundle);
    THIS_BUNDLE = bp_stack;
    YieldCurrentLock(thislock);
    return true;
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static void EditXmlClassBanner(enum editxmltypesequence type)
{
    if (type != elx_delete)     /* Just parsed all local classes */
    {
        return;
    }

    CfOut(cf_verbose, "", "     ??  Private class context\n");

    AlphaListIterator i = AlphaListIteratorInit(&VADDCLASSES);

    for (const Item *ip = AlphaListIteratorNext(&i); ip != NULL; ip = AlphaListIteratorNext(&i))
    {
        CfOut(cf_verbose, "", "     ??       %s\n", ip->name);
    }

    CfOut(cf_verbose, "", "\n");
}

/***************************************************************************/

static void KeepEditXmlPromise(Promise *pp)
{
    char *sp = NULL;

    if (!IsDefinedClass(pp->classes))
    {
        CfOut(cf_verbose, "", "\n");
        CfOut(cf_verbose, "", "   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . \n");
        CfOut(cf_verbose, "", "   Skipping whole next edit promise, as context %s is not relevant\n", pp->classes);
        CfOut(cf_verbose, "", "   .  .  .  .  .  .  .  .  .  .  .  .  .  .  . \n");
        return;
    }

    if (VarClassExcluded(pp, &sp))
    {
        CfOut(cf_verbose, "", "\n");
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        CfOut(cf_verbose, "", "Skipping whole next edit promise (%s), as var-context %s is not relevant\n",
              pp->promiser, sp);
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        return;
    }

    PromiseBanner(pp);

    if (strcmp("classes", pp->agentsubtype) == 0)
    {
        KeepClassContextPromise(pp);
        return;
    }

    if (strcmp("delete_tree", pp->agentsubtype) == 0)
    {
        VerifyTreeDeletions(pp);
        return;
    }

    if (strcmp("insert_tree", pp->agentsubtype) == 0)
    {
        VerifyTreeInsertions(pp);
        return;
    }

    if (strcmp("reports", pp->agentsubtype) == 0)
    {
        VerifyReportPromise(pp);
        return;
    }
}

/***************************************************************************/
/* Level                                                                   */
/***************************************************************************/

static void VerifyTreeInsertions(Promise *pp)
{
}

/***************************************************************************/

static void VerifyTreeDeletions(Promise *pp)
{
}
