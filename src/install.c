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
#include "policy.h"
#include "syntax.h"
#include "item_lib.h"
#include "conversion.h"
#include "reporting.h"
#include "string_lib.h"

static void DeleteSubTypes(SubType *tp);

/*******************************************************************/

int RelevantBundle(const char *agent, const char *blocktype)
{
    Item *ip;

    if ((strcmp(agent, CF_AGENTTYPES[AGENT_TYPE_COMMON]) == 0) || (strcmp(CF_COMMONC, blocktype) == 0))
    {
        return true;
    }

/* Here are some additional bundle types handled by cfAgent */

    ip = SplitString("edit_line,edit_xml", ',');

    if (strcmp(agent, CF_AGENTTYPES[AGENT_TYPE_AGENT]) == 0)
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

Bundle *AppendBundle(Policy *policy, const char *name, const char *type, Rlist *args,
                     const char *source_path)
{
    CfDebug("Appending new bundle %s %s (", type, name);

    if (DEBUG)
    {
        ShowRlist(stdout, args);
    }
    CfDebug(")\n");

    Bundle *bundle = xcalloc(1, sizeof(Bundle));
    bundle->parent_policy = policy;

    if (policy->bundles == NULL)
    {
        policy->bundles = bundle;
    }
    else
    {
        Bundle *bp = NULL;
        for (bp = policy->bundles; bp->next; bp = bp->next)
        {
        }

        bp->next = bundle;
    }

    if (strcmp(policy->current_namespace,"default") == 0)
       {
       bundle->name = xstrdup(name);
       }
    else
       {
       char fqname[CF_BUFSIZE];
       snprintf(fqname,CF_BUFSIZE-1, "%s:%s",policy->current_namespace,name);
       bundle->name = xstrdup(fqname);
       }

    bundle->type = xstrdup(type);
    bundle->namespace = xstrdup(policy->current_namespace);
    bundle->args = CopyRlist(args);
    bundle->source_path = SafeStringDuplicate(source_path);

    return bundle;
}

/*******************************************************************/

Body *AppendBody(Policy *policy, const char *name, const char *type, Rlist *args,
                 const char *source_path)
{
    CfDebug("Appending new promise body %s %s(", type, name);

    for (const Rlist *rp = args; rp; rp = rp->next)
    {
        CfDebug("%s,", (char *) rp->item);
    }
    CfDebug(")\n");

    Body *body = xcalloc(1, sizeof(Body));
    body->parent_policy = policy;

    if (policy->bodies == NULL)
    {
        policy->bodies = body;
    }
    else
    {
        Body *bp = NULL;
        for (bp = policy->bodies; bp->next; bp = bp->next)
        {
        }

        bp->next = body;
    }

    if (strcmp(policy->current_namespace,"default") == 0)
       {
       body->name = xstrdup(name);
       }
    else
       {
       char fqname[CF_BUFSIZE];
       snprintf(fqname,CF_BUFSIZE-1, "%s:%s",policy->current_namespace,name);
       body->name = xstrdup(fqname);
       }

    body->type = xstrdup(type);
    body->namespace = xstrdup(policy->current_namespace);
    body->args = CopyRlist(args);
    body->source_path = SafeStringDuplicate(source_path);

    return body;
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

Promise *AppendPromise(SubType *type, char *promiser, Rval promisee, char *classes, char *bundle, char *bundletype, char *namespace)
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

    if ((strcmp(type->name, "classes") == 0) || (strcmp(type->name, "vars") == 0))
    {
        if ((isdigit((int)*promiser)) && (Str2Int(promiser) != CF_NOINT))
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
    pp->namespace = xstrdup(namespace);
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
