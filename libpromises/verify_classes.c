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
#include <string_lib.h>                                  /* StringHash */
#include <regex.h>                                       /* StringMatchFull */


static int EvalClassExpression(EvalContext *ctx, Constraint *cp, const Promise *pp);
static bool ValidClassName(const char *str);


PromiseResult VerifyClassPromise(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert(param == NULL);

    Attributes a = GetClassContextAttributes(ctx, pp);

    if (!StringMatchFull("[a-zA-Z0-9_]+", pp->promiser))
    {
        Log(LOG_LEVEL_VERBOSE, "Class identifier '%s' contains illegal characters - canonifying", pp->promiser);
        xsnprintf(pp->promiser, strlen(pp->promiser) + 1, "%s", CanonifyName(pp->promiser));
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
            char *tags = NULL;
            {
                Buffer *tag_buffer = BufferNew();
                BufferAppendString(tag_buffer, "source=promise");

                for (const Rlist *rp = PromiseGetConstraintAsList(ctx, "meta", pp); rp; rp = rp->next)
                {
                    BufferAppendChar(tag_buffer, ',');
                    BufferAppendString(tag_buffer, RlistScalarValue(rp));
                }

                tags = BufferClose(tag_buffer);
            }

            if (/* Persistent classes are always global: */
                a.context.persistent > 0 ||
                /* Namespace-scope is global: */
                a.context.scope == CONTEXT_SCOPE_NAMESPACE ||
                /* If there is no explicit scope, common bundles define global
                 * classes, other bundles define local classes: */
                (a.context.scope == CONTEXT_SCOPE_NONE &&
                 0 == strcmp(PromiseGetBundle(pp)->type, "common")))
            {
                Log(LOG_LEVEL_VERBOSE, "Adding global class '%s'", pp->promiser);
                EvalContextClassPutSoft(ctx, pp->promiser, CONTEXT_SCOPE_NAMESPACE, tags);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Adding local bundle class '%s'", pp->promiser);
                EvalContextClassPutSoft(ctx, pp->promiser, CONTEXT_SCOPE_BUNDLE, tags);
            }

            if (a.context.persistent > 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Adding persistent class '%s'. (%d minutes)", pp->promiser,
                      a.context.persistent);
                EvalContextHeapPersistentSave(ctx, pp->promiser, a.context.persistent,
                                              CONTEXT_STATE_POLICY_RESET, tags);
            }

            free(tags);

            return PROMISE_RESULT_NOOP;
        }
    }

    return PROMISE_RESULT_NOOP;
}

static int EvalClassExpression(EvalContext *ctx, Constraint *cp, const Promise *pp)
{
    assert(pp);

    int result_and = true;
    int result_or = false;
    int result_xor = 0;
    int result = 0, total = 0;
    char buffer[CF_MAXVARSIZE];
    Rlist *rp;

    if (cp == NULL) // ProgrammingError ?  We'll crash RSN anyway ...
    {
        Log(LOG_LEVEL_ERR, "EvalClassExpression internal diagnostic discovered an ill-formed condition");
    }

    if (!IsDefinedClass(ctx, pp->classes))
    {
        return false;
    }

    if (IsDefinedClass(ctx, pp->promiser))
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
        Rval rval;
        FnCall *fp;

    case RVAL_TYPE_FNCALL:
        fp = RvalFnCallValue(cp->rval);
        /* Special expansion of functions for control, best effort only: */
        FnCallResult res = FnCallEvaluate(ctx, PromiseGetPolicy(pp), fp, pp);

        FnCallDestroy(fp);
        cp->rval = res.rval;
        break;

    case RVAL_TYPE_LIST:
        for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
        {
            rval = EvaluateFinalRval(ctx, PromiseGetPolicy(pp), NULL, "this", rp->val, true, pp);
            RvalDestroy(rp->val);
            rp->val = rval;
        }
        break;

    default:
        rval = ExpandPrivateRval(ctx, NULL, "this", cp->rval.item, cp->rval.type);
        RvalDestroy(cp->rval);
        cp->rval = rval;
        break;
    }

    if (strcmp(cp->lval, "expression") == 0)
    {
        return (cp->rval.type == RVAL_TYPE_SCALAR &&
                IsDefinedClass(ctx, RvalScalarValue(cp->rval)));
    }

    if (strcmp(cp->lval, "not") == 0)
    {
        return (cp->rval.type == RVAL_TYPE_SCALAR &&
                !IsDefinedClass(ctx, RvalScalarValue(cp->rval)));
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
                EvalContextClassPutSoft(ctx, RlistScalarValue(rp), CONTEXT_SCOPE_NAMESPACE, "source=promise");
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

        if (strcmp(PromiseGetBundle(pp)->type, "common") == 0)
        {
            EvalContextClassPutSoft(ctx, buffer, CONTEXT_SCOPE_NAMESPACE, "source=promise");
        }
        else
        {
            EvalContextClassPutSoft(ctx, buffer, CONTEXT_SCOPE_BUNDLE, "source=promise");
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

        result = IsDefinedClass(ctx, RlistScalarValue(rp));

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
