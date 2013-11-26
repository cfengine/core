/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

#include <verify_classes.h>

#include <attributes.h>
#include <matching.h>
#include <files_names.h>
#include <fncall.h>
#include <rlist.h>
#include <expand.h>
#include <promises.h>
#include <conversion.h>
#include <logic_expressions.h>
#include <string_lib.h>


static int EvalClassExpression(EvalContext *ctx, Constraint *cp, Promise *pp);
static bool ValidClassName(const char *str);


PromiseResult VerifyClassPromise(EvalContext *ctx, Promise *pp, ARG_UNUSED void *param)
{
    assert(param == NULL);

    Attributes a;

    a = GetClassContextAttributes(ctx, pp);

    if (!FullTextMatch(ctx, "[a-zA-Z0-9_]+", pp->promiser))
    {
        Log(LOG_LEVEL_VERBOSE, "Class identifier '%s' contains illegal characters - canonifying", pp->promiser);
        snprintf(pp->promiser, strlen(pp->promiser) + 1, "%s", CanonifyName(pp->promiser));
    }

    if (a.context.nconstraints == 0)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "No constraints for class promise '%s'", pp->promiser);
        return PROMISE_RESULT_FAIL;
    }

    if (a.context.nconstraints > 1)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Irreconcilable constraints in classes for '%s'", pp->promiser);
        return PROMISE_RESULT_FAIL;
    }

    bool global_class = false;
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
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "Attempted to name a class '%s', which is an illegal class identifier", pp->promiser);
            return PROMISE_RESULT_FAIL;
        }
        else
        {
            if (global_class)
            {
                Log(LOG_LEVEL_VERBOSE, "Adding global class '%s'", pp->promiser);
                EvalContextClassPut(ctx, PromiseGetNamespace(pp), pp->promiser, true, CONTEXT_SCOPE_NAMESPACE, "goal=state,source=promise");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Adding local bundle class '%s'", pp->promiser);
                EvalContextClassPut(ctx, PromiseGetNamespace(pp), pp->promiser, true, CONTEXT_SCOPE_BUNDLE, "goal=state,source=promise");
            }

            if (a.context.persistent > 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Adding persistent class '%s'. (%d minutes)", pp->promiser,
                      a.context.persistent);
                EvalContextHeapPersistentSave(pp->promiser, PromiseGetNamespace(pp), a.context.persistent, CONTEXT_STATE_POLICY_RESET);
            }

            Rlist *promise_meta = PromiseGetConstraintAsList(ctx, "meta", pp);
            if (promise_meta)
            {
                StringSet *class_meta = EvalContextClassTags(ctx, PromiseGetNamespace(pp), pp->promiser);
                for (const Rlist *rp = promise_meta; rp; rp = rp->next)
                {
                    StringSetAdd(class_meta, xstrdup(RlistScalarValue(rp)));
                }
            }

            return PROMISE_RESULT_CHANGE;
        }
    }

    return PROMISE_RESULT_NOOP;
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
        Log(LOG_LEVEL_ERR, "EvalClassExpression internal diagnostic discovered an ill-formed condition");
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
            Log(LOG_LEVEL_VERBOSE, " ?> Cancelling cached persistent class %s", pp->promiser);
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
            rval = EvaluateFinalRval(ctx, NULL, "this", rp->val, true, pp);
            RvalDestroy(rp->val);
            rp->val = rval;
        }
        break;

    default:

        rval = ExpandPrivateRval(ctx, NULL, "this", cp->rval);
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
            Log(LOG_LEVEL_ERR, "No classes to select on RHS");
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }

        snprintf(splay, CF_MAXVARSIZE, "%s+%s+%ju", VFQNAME, VIPADDRESS, (uintmax_t)getuid());
        hash = (double) StringHash(splay, 0, CF_HASHTABLESIZE);
        n = (int) (total * hash / (double) CF_HASHTABLESIZE);

        for (rp = (Rlist *) cp->rval.item, i = 0; rp != NULL; rp = rp->next, i++)
        {
            if (i == n)
            {
                EvalContextClassPut(ctx, PromiseGetNamespace(pp), RlistScalarValue(rp), true, CONTEXT_SCOPE_NAMESPACE, "goal=select_servers,source=promise");
                return true;
            }
        }
    }

/* If we get here, anything remaining on the RHS must be a clist */

    if (cp->rval.type != RVAL_TYPE_LIST)
    {
        Log(LOG_LEVEL_ERR, "RHS of promise body attribute '%s' is not a list", cp->lval);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return true;
    }

// Class distributions

    if (strcmp(cp->lval, "dist") == 0)
    {
        for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
        {
            result = IntFromString(RlistScalarValue(rp));

            if (result < 0)
            {
                Log(LOG_LEVEL_ERR, "Non-positive integer in class distribution");
                PromiseRef(LOG_LEVEL_ERR, pp);
                return false;
            }

            total += result;
        }

        if (total == 0)
        {
            Log(LOG_LEVEL_ERR, "An empty distribution was specified on RHS");
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        }

        double fluct = drand48();
        double cum = 0.0;

        for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
        {
            double prob = ((double) IntFromString(RlistScalarValue(rp))) / ((double) total);
            cum += prob;

            if (fluct < cum)
            {
                break;
            }
        }

        snprintf(buffer, CF_MAXVARSIZE - 1, "%s_%s", pp->promiser, RlistScalarValue(rp));
        /* FIXME: figure why explicit mark and get rid of it */
        EvalContextMarkPromiseDone(ctx, pp);

        if (strcmp(PromiseGetBundle(pp)->type, "common") == 0)
        {
            EvalContextClassPut(ctx, PromiseGetNamespace(pp), buffer, true, CONTEXT_SCOPE_NAMESPACE, "goal=state,source=promise");
        }
        else
        {
            EvalContextClassPut(ctx, PromiseGetNamespace(pp), buffer, true, CONTEXT_SCOPE_BUNDLE, "goal=state,source=promise");
        }

        return true;
    }

    /* and/or/xor expressions */

    for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
    {
        if (rp->val.type != RVAL_TYPE_SCALAR)
        {
            return false;
        }

        result = IsDefinedClass(ctx, RlistScalarValue(rp), PromiseGetNamespace(pp));

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
