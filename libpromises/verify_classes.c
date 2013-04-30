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

#include "verify_classes.h"

#include "attributes.h"
#include "matching.h"
#include "logging_old.h"
#include "files_names.h"
#include "fncall.h"
#include "rlist.h"
#include "expand.h"
#include "promises.h"
#include "hashes.h"
#include "conversion.h"
#include "logic_expressions.h"

#include <assert.h>


static int EvalClassExpression(EvalContext *ctx, Constraint *cp, Promise *pp);
static bool ValidClassName(const char *str);


void VerifyClassPromise(EvalContext *ctx, Promise *pp, ARG_UNUSED void *param)
{
    assert(param == NULL);

    Attributes a;

    a = GetClassContextAttributes(ctx, pp);

    if (!FullTextMatch("[a-zA-Z0-9_]+", pp->promiser))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Class identifier \"%s\" contains illegal characters - canonifying", pp->promiser);
        snprintf(pp->promiser, strlen(pp->promiser) + 1, "%s", CanonifyName(pp->promiser));
    }

    if (a.context.nconstraints == 0)
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_FAIL, "", pp, a, "No constraints for class promise %s", pp->promiser);
        return;
    }

    if (a.context.nconstraints > 1)
    {
        cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_FAIL, "", pp, a, "Irreconcilable constraints in classes for %s", pp->promiser);
        return;
    }

    bool global_class;
    if (a.context.persistent > 0) /* Persistent classes are always global */
    {
        global_class = true;
    }
    else if (a.context.scope == CONTEXT_SCOPE_NONE)
    {
        /* If there is no explicit scope, common bundles define global classes, other bundles define local classes */
        if (strcmp(PromiseGetBundle(pp)->type, "common") == 0)
        {
            global_class = true;
        }
        else
        {
            global_class = false;
        }
    }
    else if (a.context.scope == CONTEXT_SCOPE_NAMESPACE)
    {
        global_class = true;
    }
    else if (a.context.scope == CONTEXT_SCOPE_BUNDLE)
    {
        global_class = false;
    }

    if (EvalClassExpression(ctx, a.context.expression, pp))
    {
        if (!ValidClassName(pp->promiser))
        {
            cfPS(ctx, OUTPUT_LEVEL_ERROR, PROMISE_RESULT_FAIL, "", pp, a,
                 " !! Attempted to name a class \"%s\", which is an illegal class identifier", pp->promiser);
        }
        else
        {
            if (global_class)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> defining additional global class %s\n", pp->promiser);
                EvalContextHeapAddSoft(ctx, pp->promiser, PromiseGetNamespace(pp));
            }
            else
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> defining explicit local bundle class %s\n", pp->promiser);
                EvalContextStackFrameAddSoft(ctx, pp->promiser);
            }

            if (a.context.persistent > 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> defining explicit persistent class %s (%d mins)\n", pp->promiser,
                      a.context.persistent);
                EvalContextHeapPersistentSave(pp->promiser, PromiseGetNamespace(pp), a.context.persistent, CONTEXT_STATE_POLICY_RESET);
            }
        }
    }
}

static int EvalClassExpression(EvalContext *ctx, Constraint *cp, Promise *pp)
{
    int result_and = true;
    int result_or = false;
    int result_xor = 0;
    int result = 0, total = 0;
    char buffer[CF_MAXVARSIZE];
    Rlist *rp;
    FnCall *fp;
    Rval rval;

    if (cp == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! EvalClassExpression internal diagnostic discovered an ill-formed condition");
    }

    if (!IsDefinedClass(ctx, pp->classes, PromiseGetNamespace(pp)))
    {
        return false;
    }

    if (EvalContextPromiseIsDone(ctx, pp))
    {
        return false;
    }

    if (IsDefinedClass(ctx, pp->promiser, PromiseGetNamespace(pp)))
    {
        if (PromiseGetConstraintAsInt(ctx, "persistence", pp) == 0)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " ?> Cancelling cached persistent class %s", pp->promiser);
            EvalContextHeapPersistentRemove(pp->promiser);
        }
        return false;
    }

    switch (cp->rval.type)
    {
    case RVAL_TYPE_FNCALL:

        fp = (FnCall *) cp->rval.item;  /* Special expansion of functions for control, best effort only */
        FnCallResult res = FnCallEvaluate(ctx, fp, pp);

        FnCallDestroy(fp);
        cp->rval = res.rval;
        break;

    case RVAL_TYPE_LIST:
        for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
        {
            rval = EvaluateFinalRval(ctx, "this", (Rval) {rp->item, rp->type}, true, pp);
            RvalDestroy((Rval) {rp->item, rp->type});
            rp->item = rval.item;
            rp->type = rval.type;
        }
        break;

    default:

        rval = ExpandPrivateRval(ctx, "this", cp->rval);
        RvalDestroy(cp->rval);
        cp->rval = rval;
        break;
    }

    if (strcmp(cp->lval, "expression") == 0)
    {
        if (cp->rval.type != RVAL_TYPE_SCALAR)
        {
            return false;
        }

        if (IsDefinedClass(ctx, (char *) cp->rval.item, PromiseGetNamespace(pp)))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    if (strcmp(cp->lval, "not") == 0)
    {
        if (cp->rval.type != RVAL_TYPE_SCALAR)
        {
            return false;
        }

        if (IsDefinedClass(ctx, (char *) cp->rval.item, PromiseGetNamespace(pp)))
        {
            return false;
        }
        else
        {
            return true;
        }
    }

// Class selection

    if (strcmp(cp->lval, "select_class") == 0)
    {
        char splay[CF_MAXVARSIZE];
        int i, n;
        double hash;

        total = 0;

        for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
        {
            total++;
        }

        if (total == 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! No classes to select on RHS");
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            return false;
        }

        snprintf(splay, CF_MAXVARSIZE, "%s+%s+%ju", VFQNAME, VIPADDRESS, (uintmax_t)getuid());
        hash = (double) OatHash(splay, CF_HASHTABLESIZE);
        n = (int) (total * hash / (double) CF_HASHTABLESIZE);

        for (rp = (Rlist *) cp->rval.item, i = 0; rp != NULL; rp = rp->next, i++)
        {
            if (i == n)
            {
                EvalContextHeapAddSoft(ctx, rp->item, PromiseGetNamespace(pp));
                return true;
            }
        }
    }

/* If we get here, anything remaining on the RHS must be a clist */

    if (cp->rval.type != RVAL_TYPE_LIST)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! RHS of promise body attribute \"%s\" is not a list\n", cp->lval);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        return true;
    }

// Class distributions

    if (strcmp(cp->lval, "dist") == 0)
    {
        for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
        {
            result = IntFromString(rp->item);

            if (result < 0)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! Non-positive integer in class distribution");
                PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                return false;
            }

            total += result;
        }

        if (total == 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! An empty distribution was specified on RHS");
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            return false;
        }

        double fluct = drand48();
        double cum = 0.0;

        for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
        {
            double prob = ((double) IntFromString(rp->item)) / ((double) total);
            cum += prob;

            if (fluct < cum)
            {
                break;
            }
        }

        snprintf(buffer, CF_MAXVARSIZE - 1, "%s_%s", pp->promiser, (char *) rp->item);
        /* FIXME: figure why explicit mark and get rid of it */
        EvalContextMarkPromiseDone(ctx, pp);

        if (strcmp(PromiseGetBundle(pp)->type, "common") == 0)
        {
            EvalContextHeapAddSoft(ctx, buffer, PromiseGetNamespace(pp));
        }
        else
        {
            EvalContextStackFrameAddSoft(ctx, buffer);
        }

        CfDebug(" ?? \'Strategy\' distribution class interval -> %s\n", buffer);
        return true;
    }

    /* and/or/xor expressions */

    for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
    {
        if (rp->type != RVAL_TYPE_SCALAR)
        {
            return false;
        }

        result = IsDefinedClass(ctx, (char *) (rp->item), PromiseGetNamespace(pp));

        result_and = result_and && result;
        result_or = result_or || result;
        result_xor ^= result;
    }

// Class combinations

    if (strcmp(cp->lval, "or") == 0)
    {
        return result_or;
    }

    if (strcmp(cp->lval, "xor") == 0)
    {
        return (result_xor == 1) ? true : false;
    }

    if (strcmp(cp->lval, "and") == 0)
    {
        return result_and;
    }

    return false;
}

static bool ValidClassName(const char *str)
{
    ParseResult res = ParseExpression(str, 0, strlen(str));

    if (res.result)
    {
        FreeExpression(res.result);
    }

    return res.result && res.position == strlen(str);
}
