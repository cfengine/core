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
/* File: install.c                                                           */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

static void DeleteSubTypes(SubType *tp);

/*******************************************************************/

int RelevantBundle(char *agent, char *blocktype)
{
    Item *ip;

    if (strcmp(agent, CF_AGENTTYPES[cf_common]) == 0 || strcmp(CF_COMMONC, blocktype) == 0)
    {
        return true;
    }

/* Here are some additional bundle types handled by cfAgent */

    ip = SplitString("edit_line,edit_xml", ',');

    if (strcmp(agent, CF_AGENTTYPES[cf_agent]) == 0)
    {
        if (IsItemIn(ip, blocktype))
        {
            DeleteItemList(ip);
            return true;
        }
    }

    DeleteItemList(ip);
    return false;
}

/*******************************************************************/

Bundle *AppendBundle(Bundle **start, char *name, char *type, Rlist *args)
{
    Bundle *bp, *lp;

    CfDebug("Appending new bundle %s %s (", type, name);

    if (DEBUG)
    {
        ShowRlist(stdout, args);
    }
    CfDebug(")\n");

    CheckBundle(name, type);

    bp = xcalloc(1, sizeof(Bundle));

    if (*start == NULL)
    {
        *start = bp;
    }
    else
    {
        for (lp = *start; lp->next != NULL; lp = lp->next)
        {
        }

        lp->next = bp;
    }

    bp->name = xstrdup(name);
    bp->type = xstrdup(type);
    bp->args = CopyRlist(args);

    return bp;
}

/*******************************************************************/

Body *AppendBody(Body **start, char *name, char *type, Rlist *args)
{
    Body *bp, *lp;
    Rlist *rp;

    CfDebug("Appending new promise body %s %s(", type, name);

    CheckBody(name, type);

    for (rp = args; rp != NULL; rp = rp->next)
    {
        CfDebug("%s,", (char *) rp->item);
    }
    CfDebug(")\n");

    bp = xcalloc(1, sizeof(Body));

    if (*start == NULL)
    {
        *start = bp;
    }
    else
    {
        for (lp = *start; lp->next != NULL; lp = lp->next)
        {
        }

        lp->next = bp;
    }

    bp->name = xstrdup(name);
    bp->type = xstrdup(type);
    bp->args = CopyRlist(args);

    return bp;
}

/*******************************************************************/

SubType *AppendSubType(Bundle *bundle, char *typename)
{
    SubType *tp, *lp;

    CfDebug("Appending new type section %s\n", typename);

    if (bundle == NULL)
    {
        yyerror("Software error. Attempt to add a type without a bundle\n");
        FatalError("Stopped");
    }

    for (lp = bundle->subtypes; lp != NULL; lp = lp->next)
    {
        if (strcmp(lp->name, typename) == 0)
        {
            return lp;
        }
    }

    tp = xcalloc(1, sizeof(SubType));

    if (bundle->subtypes == NULL)
    {
        bundle->subtypes = tp;
    }
    else
    {
        for (lp = bundle->subtypes; lp->next != NULL; lp = lp->next)
        {
        }

        lp->next = tp;
    }

    tp->parent_bundle = bundle;
    tp->name = xstrdup(typename);

    return tp;
}

/*******************************************************************/

Promise *AppendPromise(SubType *type, char *promiser, Rval promisee, char *classes, char *bundle, char *bundletype)
{
    Promise *pp, *lp;
    char *sp = NULL, *spe = NULL;
    char output[CF_BUFSIZE];

    if (type == NULL)
    {
        yyerror("Software error. Attempt to add a promise without a type\n");
        FatalError("Stopped");
    }

/* Check here for broken promises - or later with more info? */

    CfDebug("Appending Promise from bundle %s %s if context %s\n", bundle, promiser, classes);

    pp = xcalloc(1, sizeof(Promise));

    sp = xstrdup(promiser);

    if (strlen(classes) > 0)
    {
        spe = xstrdup(classes);
    }
    else
    {
        spe = xstrdup("any");
    }

    if (strcmp(type->name, "classes") == 0 || strcmp(type->name, "vars") == 0)
    {
        if (isdigit(*promiser) && Str2Int(promiser) != CF_NOINT)
        {
            yyerror("Variable or class identifier is purely numerical, which is not allowed");
        }
    }

    if (strcmp(type->name, "vars") == 0)
    {
        if (!CheckParseVariableName(promiser))
        {
            snprintf(output, CF_BUFSIZE, "Use of a reserved or illegal variable name \"%s\" ", promiser);
            ReportError(output);
        }
    }

    if (type->promiselist == NULL)
    {
        type->promiselist = pp;
    }
    else
    {
        for (lp = type->promiselist; lp->next != NULL; lp = lp->next)
        {
        }

        lp->next = pp;
    }

    pp->parent_subtype = type;
    pp->audit = AUDITPTR;
    pp->bundle = xstrdup(bundle);
    pp->promiser = sp;
    pp->promisee = promisee;
    pp->classes = spe;
    pp->donep = &(pp->done);
    pp->has_subbundles = false;
    pp->org_pp = NULL;

    pp->bundletype = xstrdup(bundletype);       /* cache agent,common,server etc */
    pp->agentsubtype = type->name;      /* Cache the typename */
    pp->ref_alloc = 'n';

    return pp;
}

/*******************************************************************/
/* Cleanup                                                         */
/*******************************************************************/

void DeleteBundles(Bundle *bp)
{
    if (bp == NULL)
    {
        return;
    }

    if (bp->next != NULL)
    {
        DeleteBundles(bp->next);
    }

    if (bp->name != NULL)
    {
        free(bp->name);
    }

    if (bp->type != NULL)
    {
        free(bp->type);
    }

    DeleteRlist(bp->args);
    DeleteSubTypes(bp->subtypes);
    free(bp);
}

/*******************************************************************/

static void DeleteSubTypes(SubType *tp)
{
    if (tp == NULL)
    {
        return;
    }

    if (tp->next != NULL)
    {
        DeleteSubTypes(tp->next);
    }

    DeletePromises(tp->promiselist);

    if (tp->name != NULL)
    {
        free(tp->name);
    }

    free(tp);
}

/*******************************************************************/

void DeleteBodies(Body *bp)
{
    if (bp == NULL)
    {
        return;
    }

    if (bp->next != NULL)
    {
        DeleteBodies(bp->next);
    }

    if (bp->name != NULL)
    {
        free(bp->name);
    }

    if (bp->type != NULL)
    {
        free(bp->type);
    }

    DeleteRlist(bp->args);
    DeleteConstraintList(bp->conlist);
    free(bp);
}
