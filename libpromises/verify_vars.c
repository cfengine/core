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

#include <verify_vars.h>

#include <actuator.h>
#include <attributes.h>
#include <string_lib.h>
#include <buffer.h>
#include <misc_lib.h>
#include <fncall.h>
#include <rlist.h>
#include <conversion.h>
#include <expand.h>
#include <scope.h>
#include <promises.h>
#include <vars.h>
#include <matching.h>
#include <syntax.h>

typedef struct
{
    bool should_converge;
    bool ok_redefine;
    bool drop_undefined;
    Constraint *cp_save; // e.g. string => "foo"
} ConvergeVariableOptions;


static ConvergeVariableOptions CollectConvergeVariableOptions(EvalContext *ctx, const Promise *pp, bool allow_redefine);
static bool Epimenides(EvalContext *ctx, const char *ns, const char *scope, const char *var, Rval rval, int level);
static int CompareRval(Rval rval1, Rval rval2);


PromiseResult VerifyVarPromise(EvalContext *ctx, const Promise *pp, bool allow_duplicates)
{
    ConvergeVariableOptions opts = CollectConvergeVariableOptions(ctx, pp, allow_duplicates);
    if (!opts.should_converge)
    {
        return PROMISE_RESULT_NOOP;
    }

    //More consideration needs to be given to using these
    //a.transaction = GetTransactionConstraints(pp);
    Attributes a = { {0} };
    a.classes = GetClassDefinitionConstraints(ctx, pp);

    VarRef *ref = VarRefParseFromBundle(pp->promiser, PromiseGetBundle(pp));
    if (strcmp("meta", pp->parent_promise_type->name) == 0)
    {
        VarRefSetMeta(ref, true);
    }

    Rval existing_var_rval;
    DataType existing_var_type = DATA_TYPE_NONE;

    if (!IsExpandable(pp->promiser))
    {
        EvalContextVariableGet(ctx, ref, &existing_var_rval, &existing_var_type);
    }

    PromiseResult result = PROMISE_RESULT_NOOP;

    Rval rval = opts.cp_save->rval;

    if (rval.item != NULL)
    {
        DataType data_type = DataTypeFromString(opts.cp_save->lval);

        FnCall *fp = (FnCall *) rval.item;

        if (opts.cp_save->rval.type == RVAL_TYPE_FNCALL)
        {
            if (existing_var_type != DATA_TYPE_NONE)
            {
                // Already did this
                VarRefDestroy(ref);
                return result;
            }

            FnCallResult res = FnCallEvaluate(ctx, fp, pp);

            if (res.status == FNCALL_FAILURE)
            {
                /* We do not assign variables to failed fn calls */
                RvalDestroy(res.rval);
                VarRefDestroy(ref);
                return result;
            }
            else
            {
                rval = res.rval;
            }
        }
        else
        {
            Buffer *conv = BufferNew();

            if (strcmp(opts.cp_save->lval, "int") == 0)
            {
                int result = BufferPrintf(conv, "%ld", IntFromString(opts.cp_save->rval.item));
                if (result < 0)
                {
                    /*
                     * Even though there will be no problems with memory allocation, there
                     * might be other problems.
                     */
                    UnexpectedError("Problems writing to buffer");
                    VarRefDestroy(ref);
                    BufferDestroy(&conv);
                    return result;
                }
                rval = RvalNew(BufferData(conv), opts.cp_save->rval.type);
            }
            else if (strcmp(opts.cp_save->lval, "real") == 0)
            {
                int result = -1;
                double real_value = 0.0;
                if (DoubleFromString(opts.cp_save->rval.item, &real_value))
                {
                    result = BufferPrintf(conv, "%lf", real_value);
                }
                else
                {
                    result = BufferPrintf(conv, "(double conversion error)");
                }

                if (result < 0)
                {
                    /*
                     * Even though there will be no problems with memory allocation, there
                     * might be other problems.
                     */
                    UnexpectedError("Problems writing to buffer");
                    VarRefDestroy(ref);
                    BufferDestroy(&conv);
                    return result;
                }
                rval = RvalCopy((Rval) {(char *)BufferData(conv), opts.cp_save->rval.type});
            }
            else
            {
                rval = RvalCopy(opts.cp_save->rval);
            }

            if (rval.type == RVAL_TYPE_LIST)
            {
                Rlist *rval_list = RvalRlistValue(rval);
                RlistFlatten(ctx, &rval_list);
                rval.item = rval_list;
            }

            BufferDestroy(&conv);
        }

        if (Epimenides(ctx, PromiseGetBundle(pp)->ns, PromiseGetBundle(pp)->name, pp->promiser, rval, 0))
        {
            Log(LOG_LEVEL_ERR, "Variable '%s' contains itself indirectly - an unkeepable promise", pp->promiser);
            exit(1);
        }
        else
        {
            /* See if the variable needs recursively expanding again */

            Rval returnval = EvaluateFinalRval(ctx, ref->ns, ref->scope, rval, true, pp);

            RvalDestroy(rval);

            // freed before function exit
            rval = returnval;
        }

        if (existing_var_type != DATA_TYPE_NONE)
        {
            if (opts.ok_redefine)    /* only on second iteration, else we ignore broken promises */
            {
                EvalContextVariableRemove(ctx, ref);
            }
            else if ((THIS_AGENT_TYPE == AGENT_TYPE_COMMON) && (CompareRval(existing_var_rval, rval) == false))
            {
                switch (rval.type)
                {
                case RVAL_TYPE_SCALAR:
                    Log(LOG_LEVEL_VERBOSE, "Redefinition of a constant scalar '%s', was '%s' now '%s'",
                          pp->promiser, RvalScalarValue(existing_var_rval), RvalScalarValue(rval));
                    PromiseRef(LOG_LEVEL_VERBOSE, pp);
                    break;

                case RVAL_TYPE_LIST:
                    {
                    Log(LOG_LEVEL_VERBOSE, "Redefinition of a constant list '%s'", pp->promiser);
                        Writer *w = StringWriter();
                        RlistWrite(w, existing_var_rval.item);
                        char *oldstr = StringWriterClose(w);
                        Log(LOG_LEVEL_VERBOSE, "Old value '%s'", oldstr);
                        free(oldstr);

                        w = StringWriter();
                        RlistWrite(w, rval.item);
                        char *newstr = StringWriterClose(w);
                        Log(LOG_LEVEL_VERBOSE, " New value '%s'", newstr);
                        free(newstr);
                        PromiseRef(LOG_LEVEL_VERBOSE, pp);
                    }
                    break;

                case RVAL_TYPE_CONTAINER:
                case RVAL_TYPE_FNCALL:
                case RVAL_TYPE_NOPROMISEE:
                    break;
                }
            }
        }

        if (IsCf3VarString(pp->promiser))
        {
            // Unexpanded variables, we don't do anything with
            RvalDestroy(rval);
            VarRefDestroy(ref);
            return result;
        }

        if (!FullTextMatch(ctx, "[a-zA-Z0-9_\200-\377.]+(\\[.+\\])*", pp->promiser))
        {
            Log(LOG_LEVEL_ERR, "Variable identifier contains illegal characters");
            PromiseRef(LOG_LEVEL_ERR, pp);
            RvalDestroy(rval);
            VarRefDestroy(ref);
            return result;
        }

        if (opts.drop_undefined && rval.type == RVAL_TYPE_LIST)
        {
            for (Rlist *rp = rval.item; rp != NULL; rp = rp->next)
            {
                if (IsNakedVar(RlistScalarValue(rp), '@'))
                {
                    free(rp->val.item);
                    rp->val.item = xstrdup(CF_NULL_VALUE);
                }
            }
        }

        if (ref->num_indices > 0)
        {
            if (data_type == DATA_TYPE_CONTAINER)
            {
                char *lval_str = VarRefToString(ref, true);
                Log(LOG_LEVEL_ERR, "Cannot assign a container to an indexed variable name '%s'. Should be assigned to '%s' instead",
                    lval_str, ref->lval);
                free(lval_str);
                VarRefDestroy(ref);
                RvalDestroy(rval);
                return result;
            }
            else
            {
                DataType existing_type = DATA_TYPE_NONE;
                VarRef *base_ref = VarRefCopyIndexless(ref);
                if (EvalContextVariableGet(ctx, ref, NULL, &existing_type) && existing_type == DATA_TYPE_CONTAINER)
                {
                    char *lval_str = VarRefToString(ref, true);
                    char *base_ref_str = VarRefToString(base_ref, true);
                    Log(LOG_LEVEL_ERR, "Cannot assign value to indexed variable name '%s', because a container is already assigned to the base name '%s'",
                        lval_str, base_ref_str);
                    free(lval_str);
                    free(base_ref_str);
                    VarRefDestroy(base_ref);
                    VarRefDestroy(ref);
                    RvalDestroy(rval);
                    return result;
                }
                VarRefDestroy(base_ref);
            }
        }

        if (!EvalContextVariablePut(ctx, ref, rval.item, DataTypeFromString(opts.cp_save->lval), "goal=state,source=promise"))
        {
            Log(LOG_LEVEL_VERBOSE, "Unable to converge %s.%s value (possibly empty or infinite regression)", ref->scope, pp->promiser);
            PromiseRef(LOG_LEVEL_VERBOSE, pp);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }
        else
        {
            Rlist *promise_meta = PromiseGetConstraintAsList(ctx, "meta", pp);
            if (promise_meta)
            {
                StringSet *class_meta = EvalContextVariableTags(ctx, ref);
                Buffer *print;
                for (const Rlist *rp = promise_meta; rp; rp = rp->next)
                {
                    StringSetAdd(class_meta, xstrdup(RlistScalarValue(rp)));
                    print = StringSetToBuffer(class_meta, ',');
                    Log(LOG_LEVEL_INFO, "Added tag %s to class %s, tags now [%s]", RlistScalarValue(rp), pp->promiser, BufferData(print));
                    BufferDestroy(&print);
                }
            }

            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Variable %s has no promised value", pp->promiser);
        Log(LOG_LEVEL_ERR, "Rule from %s at/before line %zu", PromiseGetBundle(pp)->source_path, opts.cp_save->offset.line);
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }

    /*
     * FIXME: Variable promise are exempt from normal evaluation logic still, so
     * they are not pushed to evaluation stack before being evaluated. Due to
     * this reason, we cannot call cfPS here to set classes, as it will error
     * out with ProgrammingError.
     *
     * In order to support 'classes' body for variables as well, we call
     * ClassAuditLog explicitly.
     */
    ClassAuditLog(ctx, pp, a, result);

    VarRefDestroy(ref);
    RvalDestroy(rval);

    return result;
}

// FIX: this function is a mixture of Equal/Compare (boolean/diff).
// somebody is bound to misuse this at some point
static int CompareRlist(Rlist *list1, Rlist *list2)
{
    Rlist *rp1, *rp2;

    for (rp1 = list1, rp2 = list2; rp1 != NULL && rp2 != NULL; rp1 = rp1->next, rp2 = rp2->next)
    {
        if (rp1->val.item && rp2->val.item)
        {
            Rlist *rc1, *rc2;

            if (rp1->val.type == RVAL_TYPE_FNCALL || rp2->val.type == RVAL_TYPE_FNCALL)
            {
                return -1;      // inconclusive
            }

            rc1 = rp1;
            rc2 = rp2;

            // Check for list nesting with { fncall(), "x" ... }

            if (rp1->val.type == RVAL_TYPE_LIST)
            {
                rc1 = rp1->val.item;
            }

            if (rp2->val.type == RVAL_TYPE_LIST)
            {
                rc2 = rp2->val.item;
            }

            if (IsCf3VarString(rc1->val.item) || IsCf3VarString(rp2->val.item))
            {
                return -1;      // inconclusive
            }

            if (strcmp(rc1->val.item, rc2->val.item) != 0)
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    return true;
}

static int CompareRval(Rval rval1, Rval rval2)
{
    if (rval1.type != rval2.type)
    {
        return -1;
    }

    switch (rval1.type)
    {
    case RVAL_TYPE_SCALAR:

        if (IsCf3VarString((char *) rval1.item) || IsCf3VarString((char *) rval2.item))
        {
            return -1;          // inconclusive
        }

        if (strcmp(rval1.item, rval2.item) != 0)
        {
            return false;
        }

        break;

    case RVAL_TYPE_LIST:
        return CompareRlist(rval1.item, rval2.item);

    case RVAL_TYPE_FNCALL:
        return -1;

    default:
        return -1;
    }

    return true;
}

static bool Epimenides(EvalContext *ctx, const char *ns, const char *scope, const char *var, Rval rval, int level)
{
    Rlist *rp, *list;
    char exp[CF_EXPANDSIZE];

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:

        if (StringContainsVar(rval.item, var))
        {
            Log(LOG_LEVEL_ERR, "Scalar variable '%s' contains itself (non-convergent) '%s'", var, (char *) rval.item);
            return true;
        }

        if (IsCf3VarString(rval.item))
        {
            ExpandScalar(ctx, ns, scope, rval.item, exp);

            if (strcmp(exp, (const char *) rval.item) == 0)
            {
                return false;
            }

            if (level > 3)
            {
                return false;
            }

            if (Epimenides(ctx, ns, scope, var, (Rval) {exp, RVAL_TYPE_SCALAR}, level + 1))
            {
                return true;
            }
        }

        break;

    case RVAL_TYPE_LIST:
        list = (Rlist *) rval.item;

        for (rp = list; rp != NULL; rp = rp->next)
        {
            if (Epimenides(ctx, ns, scope, var, rp->val, level))
            {
                return true;
            }
        }
        break;

    case RVAL_TYPE_CONTAINER:
    case RVAL_TYPE_FNCALL:
    case RVAL_TYPE_NOPROMISEE:
        return false;
    }

    return false;
}

/**
 * @brief Collects variable constraints controlling how the promise should be converged
 */
static ConvergeVariableOptions CollectConvergeVariableOptions(EvalContext *ctx, const Promise *pp, bool allow_redefine)
{
    ConvergeVariableOptions opts = { 0 };
    opts.should_converge = false;
    opts.drop_undefined = false;
    opts.ok_redefine = allow_redefine;
    opts.cp_save = NULL;

    if (EvalContextPromiseIsDone(ctx, pp))
    {
        return opts;
    }

    if (!IsDefinedClass(ctx, pp->classes, PromiseGetNamespace(pp)))
    {
        return opts;
    }

    int num_values = 0;
    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (strcmp(cp->lval, "comment") == 0)
        {
            continue;
        }

        if (cp->rval.item == NULL)
        {
            continue;
        }

        if (strcmp(cp->lval, "ifvarclass") == 0)
        {
            Rval res;

            switch (cp->rval.type)
            {
            case RVAL_TYPE_SCALAR:

                if (!IsDefinedClass(ctx, cp->rval.item, PromiseGetNamespace(pp)))
                {
                    return opts;
                }

                break;

            case RVAL_TYPE_FNCALL:
                {
                    bool excluded = false;

                    /* eval it: e.g. ifvarclass => not("a_class") */

                    res = FnCallEvaluate(ctx, cp->rval.item, pp).rval;

                    /* Don't continue unless function was evaluated properly */
                    if (res.type != RVAL_TYPE_SCALAR)
                    {
                        RvalDestroy(res);
                        return opts;
                    }

                    excluded = !IsDefinedClass(ctx, res.item, PromiseGetNamespace(pp));

                    RvalDestroy(res);

                    if (excluded)
                    {
                        return opts;
                    }
                }
                break;

            default:
                Log(LOG_LEVEL_ERR, "Invalid ifvarclass type '%c': should be string or function", cp->rval.type);
                continue;
            }

            continue;
        }

        if (strcmp(cp->lval, "policy") == 0)
        {
            if (strcmp(cp->rval.item, "ifdefined") == 0)
            {
                opts.drop_undefined = true;
                opts.ok_redefine = false;
            }
            else if (strcmp(cp->rval.item, "constant") == 0)
            {
                opts.ok_redefine = false;
            }
            else
            {
                opts.ok_redefine |= true;
            }
        }
        else if (DataTypeFromString(cp->lval) != DATA_TYPE_NONE)
        {
            num_values++;
            opts.cp_save = cp;
        }
    }

    if (opts.cp_save == NULL)
    {
        Log(LOG_LEVEL_WARNING, "Variable body for '%s' seems incomplete", pp->promiser);
        PromiseRef(LOG_LEVEL_INFO, pp);
        return opts;
    }

    if (num_values > 2)
    {
        Log(LOG_LEVEL_ERR, "Variable '%s' breaks its own promise with multiple values (code %d)", pp->promiser, num_values);
        PromiseRef(LOG_LEVEL_ERR, pp);
        return opts;
    }

    opts.should_converge = true;
    return opts;
}
