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

#include "verify_methods.h"

#include "env_context.h"
#include "vars.h"
#include "expand.h"
#include "files_names.h"
#include "scope.h"
#include "hashes.h"
#include "unix.h"
#include "attributes.h"
#include "cfstream.h"
#include "transaction.h"
#include "logging.h"
#include "verify_outputs.h"
#include "generic_agent.h" // HashVariables
#include "fncall.h"
#include "rlist.h"

static void GetReturnValue(EvalContext *ctx, char *scope, Promise *pp);
    
/*****************************************************************************/

void VerifyMethodsPromise(EvalContext *ctx, Promise *pp, const ReportContext *report_context)
{
    Attributes a = { {0} };

    a = GetMethodAttributes(ctx, pp);

    VerifyMethod(ctx, "usebundle", a, pp, report_context);
    ScopeDeleteScalar("this", "promiser");
}

/*****************************************************************************/

int VerifyMethod(EvalContext *ctx, char *attrname, Attributes a, Promise *pp, const ReportContext *report_context)
{
    Bundle *bp;
    void *vp;
    FnCall *fp;
    char method_name[CF_EXPANDSIZE], qualified_method[CF_BUFSIZE], *method_deref;
    Rlist *params = NULL;
    int retval = false;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    if (a.havebundle)
    {
        if ((vp = ConstraintGetRvalValue(ctx, attrname, pp, RVAL_TYPE_FNCALL)))
        {
            fp = (FnCall *) vp;
            ExpandScalar(fp->name, method_name);
            params = fp->args;
        }
        else if ((vp = ConstraintGetRvalValue(ctx, attrname, pp, RVAL_TYPE_SCALAR)))
        {
            ExpandScalar((char *) vp, method_name);
            params = NULL;
        }
        else
        {
            return false;
        }
    }

    GetLockName(lockname, "method", pp->promiser, params);

    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, false);

    if (thislock.lock == NULL)
    {
        return false;
    }

    PromiseBanner(ctx, pp);

    if (strncmp(method_name,"default:",strlen("default:")) == 0) // CF_NS == ':'
    {
        method_deref = strchr(method_name, CF_NS) + 1;
    }
    else if ((strchr(method_name, CF_NS) == NULL) && (strcmp(pp->ns, "default") != 0))
    {
        snprintf(qualified_method, CF_BUFSIZE, "%s%c%s", pp->ns, CF_NS, method_name);
        method_deref = qualified_method;
    }
    else
    {
         method_deref = method_name;
    }
    
    bp = PolicyGetBundle(PolicyFromPromise(pp), NULL, "agent", method_deref);
    if (!bp)
    {
        bp = PolicyGetBundle(PolicyFromPromise(pp), NULL, "common", method_deref);
    }

    if (bp)
    {
        const char *bp_stack = THIS_BUNDLE;

        BannerSubBundle(bp, params);

        ScopeDelete(bp->name);
        ScopeNew(bp->name);
        HashVariables(ctx, PolicyFromPromise(pp), bp->name, report_context);

        char ns[CF_BUFSIZE];
        snprintf(ns,CF_BUFSIZE,"%s_meta",method_name);
        ScopeNew(ns);
        SetBundleOutputs(bp->name);

        ScopeAugment(ctx, method_deref, pp->ns, bp->args, params);

        THIS_BUNDLE = bp->name;

        EvalContextStackPushFrame(ctx, a.inherit);

        retval = ScheduleAgentOperations(ctx, bp, report_context);

        GetReturnValue(ctx, bp->name, pp);
        ResetBundleOutputs(bp->name);

        EvalContextStackPopFrame(ctx);

        THIS_BUNDLE = bp_stack;

        switch (retval)
        {
        case CF_FAIL:
            cfPS(ctx, OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a, " !! Method failed in some repairs or aborted\n");
            break;

        case CF_CHG:
            cfPS(ctx, OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, " !! Method invoked repairs\n");
            break;

        default:
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Method verified\n");
            break;

        }

        for (const Rlist *rp = bp->args; rp; rp = rp->next)
        {
            const char *lval = rp->item;
            ScopeDeleteScalar(bp->name, lval);
        }
    }
    else
    {
        if (IsCf3VarString(method_name))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "",
                  " !! A variable seems to have been used for the name of the method. In this case, the promiser also needs to contain the unique name of the method");
        }
        if (bp && (bp->name))
        {
            cfPS(ctx, OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, " !! Method \"%s\" was used but was not defined!\n", bp->name);
        }
        else
        {
            cfPS(ctx, OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
                 " !! A method attempted to use a bundle \"%s\" that was apparently not defined!\n", method_name);
        }
    }

    
    YieldCurrentLock(thislock);
    return retval;
}

/***********************************************************************/

static void GetReturnValue(EvalContext *ctx, char *scope, Promise *pp)
{
    char *result = ConstraintGetRvalValue(ctx, "useresult", pp, RVAL_TYPE_SCALAR);

    if (result)
    {
        AssocHashTableIterator i;
        CfAssoc *assoc;
        char newname[CF_BUFSIZE];                 
        Scope *ptr;
        char index[CF_MAXVARSIZE], match[CF_MAXVARSIZE];    

        if ((ptr = ScopeGet(scope)) == NULL)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", " !! useresult was specified but the method returned no data");
            return;
        }
    
        i = HashIteratorInit(ptr->hashtable);
    
        while ((assoc = HashIteratorNext(&i)))
        {
            snprintf(match, CF_MAXVARSIZE - 1, "last-result[");

            if (strncmp(match, assoc->lval, strlen(match)) == 0)
            {
                char *sp;
          
                index[0] = '\0';
                sscanf(assoc->lval + strlen(match), "%127[^\n]", index);
                if ((sp = strchr(index, ']')))
                {
                    *sp = '\0';
                }
                else
                {
                    index[strlen(index) - 1] = '\0';
                }
          
                if (strlen(index) > 0)
                {
                    snprintf(newname, CF_BUFSIZE, "%s[%s]", result, index);
                }
                else
                {
                    snprintf(newname, CF_BUFSIZE, "%s", result);
                }

                ScopeNewScalar(pp->bundle, newname, assoc->rval.item, DATA_TYPE_STRING);           
            }
        }
        
    }
    
}

