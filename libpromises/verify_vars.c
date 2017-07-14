/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
#include <regex.h>      /* CompileRegex,StringMatchFullWithPrecompiledRegex */
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
#include <audit.h>

typedef struct
{
    bool should_converge;
    bool ok_redefine;
    bool drop_undefined;
    Constraint *cp_save; // e.g. string => "foo"
} ConvergeVariableOptions;


static ConvergeVariableOptions CollectConvergeVariableOptions(EvalContext *ctx, const Promise *pp, bool allow_redefine);
static bool Epimenides(EvalContext *ctx, const char *ns, const char *scope, const char *var, Rval rval, int level);
static bool CompareRval(const void *rval1_item, RvalType rval1_type, const void *rval2_item, RvalType rval2_type);

static bool IsValidVariableName(const char *var_name)
{
    /* TODO: remove at some point (global, leaked), but for now
     * this offers an attractive speedup. */
    static pcre *rx = NULL;
    if (!rx)
    {
        rx = CompileRegex("[a-zA-Z0-9_\200-\377.]+(\\[.+\\])*"); /* Known leak, see TODO. */
    }

    return StringMatchFullWithPrecompiledRegex(rx, var_name);
}

PromiseResult VerifyVarPromise(EvalContext *ctx, const Promise *pp, bool allow_duplicates)
{
    ConvergeVariableOptions opts = CollectConvergeVariableOptions(ctx, pp, allow_duplicates);
    if (!opts.should_converge)
    {
        return PROMISE_RESULT_NOOP;
    }

    Attributes a = { {0} };
    // More consideration needs to be given to using these
    //a.transaction = GetTransactionConstraints(pp);
    a.classes = GetClassDefinitionConstraints(ctx, pp);

    VarRef *ref = VarRefParseFromBundle(pp->promiser, PromiseGetBundle(pp));
    if (strcmp("meta", pp->parent_promise_type->name) == 0)
    {
        VarRefSetMeta(ref, true);
    }

    DataType existing_value_type = CF_DATA_TYPE_NONE;
    const void *const existing_value =
        IsExpandable(pp->promiser) ? NULL : EvalContextVariableGet(ctx, ref, &existing_value_type);

    PromiseResult result = PROMISE_RESULT_NOOP;
    Rval rval = opts.cp_save->rval;

    if (rval.item != NULL)
    {
        DataType data_type = DataTypeFromString(opts.cp_save->lval);

        if (opts.cp_save->rval.type == RVAL_TYPE_FNCALL)
        {
            FnCall *fp = RvalFnCallValue(rval);
            const FnCallType *fn = FnCallTypeGet(fp->name);
            if (!fn)
            {
                assert(false && "Canary: should have been caught before this point");
                FatalError(ctx, "While setting variable '%s' in bundle '%s', unknown function '%s'",
                           pp->promiser, PromiseGetBundle(pp)->name, fp->name);
            }

            if (fn->dtype != DataTypeFromString(opts.cp_save->lval))
            {
                FatalError(ctx, "While setting variable '%s' in bundle '%s', variable declared type '%s' but function '%s' returns type '%s'",
                           pp->promiser, PromiseGetBundle(pp)->name, opts.cp_save->lval,
                           fp->name, DataTypeToString(fn->dtype));
            }

            if (existing_value_type != CF_DATA_TYPE_NONE)
            {
                // Already did this
                VarRefDestroy(ref);
                return PROMISE_RESULT_NOOP;
            }

            FnCallResult res = FnCallEvaluate(ctx, PromiseGetPolicy(pp), fp, pp);

            if (res.status == FNCALL_FAILURE)
            {
                /* We do not assign variables to failed fn calls */
                RvalDestroy(res.rval);
                VarRefDestroy(ref);
                return PROMISE_RESULT_NOOP;
            }
            else
            {
                rval = res.rval;
            }
        }
        else
        {
            Buffer *conv = BufferNew();
            bool malformed = false, misprint = false;

            if (strcmp(opts.cp_save->lval, "int") == 0)
            {
                long int asint = IntFromString(opts.cp_save->rval.item);
                if (asint == CF_NOINT)
                {
                    malformed = true;
                }
                else if (0 > BufferPrintf(conv, "%ld", asint))
                {
                    misprint = true;
                }
                else
                {
                    rval = RvalNew(BufferData(conv), opts.cp_save->rval.type);
                }
            }
            else if (strcmp(opts.cp_save->lval, "real") == 0)
            {
                double real_value;
                if (!DoubleFromString(opts.cp_save->rval.item, &real_value))
                {
                    malformed = true;
                }
                else if (0 > BufferPrintf(conv, "%lf", real_value))
                {
                    misprint = true;
                }
                else
                {
                    rval = RvalNew(BufferData(conv), opts.cp_save->rval.type);
                }
            }
            else
            {
                rval = RvalCopy(opts.cp_save->rval);
            }
            BufferDestroy(conv);

            if (malformed)
            {
                /* Arises when opts->cp_save->rval.item isn't yet expanded. */
                /* Has already been logged by *FromString */
                VarRefDestroy(ref);
                return PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            }
            else if (misprint)
            {
                /* Even though no problems with memory allocation can
                 * get here, there might be other problems. */
                UnexpectedError("Problems writing to buffer");
                VarRefDestroy(ref);
                return PROMISE_RESULT_NOOP;
            }
            else if (rval.type == RVAL_TYPE_LIST)
            {
                Rlist *rval_list = RvalRlistValue(rval);
                RlistFlatten(ctx, &rval_list);
                rval.item = rval_list;
            }
        }

        if (Epimenides(ctx, PromiseGetBundle(pp)->ns, PromiseGetBundle(pp)->name, pp->promiser, rval, 0))
        {
            Log(LOG_LEVEL_ERR, "Variable '%s' contains itself indirectly - an unkeepable promise", pp->promiser);
            exit(EXIT_FAILURE);
        }
        else
        {
            /* See if the variable needs recursively expanding again */

            Rval returnval = EvaluateFinalRval(ctx, PromiseGetPolicy(pp), ref->ns, ref->scope, rval, true, pp);

            RvalDestroy(rval);

            // freed before function exit
            rval = returnval;
        }

        if (existing_value_type != CF_DATA_TYPE_NONE)
        {
            if (!opts.ok_redefine)    /* only on second iteration, else we ignore broken promises */
            {
                if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON &&
                     !CompareRval(existing_value, DataTypeToRvalType(existing_value_type),
                                  rval.item, rval.type))
                {
                    switch (rval.type)
                    {
                    case RVAL_TYPE_SCALAR:
                        Log(LOG_LEVEL_VERBOSE, "V: Redefinition of a constant scalar '%s', was '%s' now '%s'",
                            pp->promiser, (const char *)existing_value, RvalScalarValue(rval));
                        PromiseRef(LOG_LEVEL_VERBOSE, pp);
                        break;

                    case RVAL_TYPE_LIST:
                        {
                            Log(LOG_LEVEL_VERBOSE, "V: Redefinition of a constant list '%s'", pp->promiser);
                            Writer *w = StringWriter();
                            RlistWrite(w, existing_value);
                            char *oldstr = StringWriterClose(w);
                            Log(LOG_LEVEL_VERBOSE, "Old value '%s'", oldstr);
                            free(oldstr);

                            w = StringWriter();
                            RlistWrite(w, rval.item);
                            char *newstr = StringWriterClose(w);
                            Log(LOG_LEVEL_VERBOSE, "V: New value '%s'", newstr);
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

                RvalDestroy(rval);
                VarRefDestroy(ref);
                return result;
            }
        }

        if (IsCf3VarString(pp->promiser))
        {
            // Unexpanded variables, we don't do anything with
            RvalDestroy(rval);
            VarRefDestroy(ref);
            return result;
        }

        if (!IsValidVariableName(pp->promiser))
        {
            Log(LOG_LEVEL_ERR, "Variable identifier contains illegal characters");
            PromiseRef(LOG_LEVEL_ERR, pp);
            RvalDestroy(rval);
            VarRefDestroy(ref);
            return result;
        }

        if (rval.type == RVAL_TYPE_LIST)
        {
            if (opts.drop_undefined)
            {
                Rlist *stripped = RvalRlistValue(rval);
                Rlist *entry = stripped;
                while (entry)
                {
                    Rlist *delete_me = NULL;
                    if (IsNakedVar(RlistScalarValue(entry), '@'))
                    {
                        delete_me = entry;
                    }
                    entry = entry->next;
                    RlistDestroyEntry(&stripped, delete_me);
                }
                rval.item = stripped;
            }

            for (const Rlist *rp = RvalRlistValue(rval); rp; rp = rp->next)
            {
                switch (rp->val.type)
                {
                case RVAL_TYPE_SCALAR:
                    break;

                default:
                    // Cannot assign variable because value is a list containing a non-scalar item
                    VarRefDestroy(ref);
                    RvalDestroy(rval);
                    return result;
                }
            }
        }

        if (ref->num_indices > 0)
        {
            if (data_type == CF_DATA_TYPE_CONTAINER)
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
                DataType existing_type = CF_DATA_TYPE_NONE;
                VarRef *base_ref = VarRefCopyIndexless(ref);
                if (EvalContextVariableGet(ctx, ref, &existing_type) && existing_type == CF_DATA_TYPE_CONTAINER)
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


        DataType required_datatype = DataTypeFromString(opts.cp_save->lval);
        if (rval.type != DataTypeToRvalType(required_datatype))
        {
            char *ref_str = VarRefToString(ref, true);
            char *value_str = RvalToString(rval);
            Log(LOG_LEVEL_ERR, "Variable '%s' expected a variable of type '%s', but was given incompatible value '%s'",
                ref_str, DataTypeToString(required_datatype), value_str);
            PromiseRef(LOG_LEVEL_ERR, pp);
            free(ref_str);
            free(value_str);
            VarRefDestroy(ref);
            RvalDestroy(rval);
            return PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }

        if (!EvalContextVariablePut(ctx, ref, rval.item, required_datatype, "source=promise"))
        {
            Log(LOG_LEVEL_VERBOSE,
                "Unable to converge %s.%s value (possibly empty or infinite regression)",
                ref->scope, pp->promiser);
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
                    Log(LOG_LEVEL_DEBUG,
                        "Added tag %s to class %s, tags now [%s]",
                        RlistScalarValue(rp), pp->promiser, BufferData(print));
                    BufferDestroy(print);
                }
            }
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

static bool CompareRval(const void *rval1_item, RvalType rval1_type,
                        const void *rval2_item, RvalType rval2_type)
{
    if (rval1_type != rval2_type)
    {
        return -1;
    }

    switch (rval1_type)
    {
    case RVAL_TYPE_SCALAR:

        if (IsCf3VarString(rval1_item) || IsCf3VarString(rval2_item))
        {
            return -1;          // inconclusive
        }

        if (strcmp(rval1_item, rval2_item) != 0)
        {
            return false;
        }

        break;

    case RVAL_TYPE_LIST:
        return RlistEqual(rval1_item, rval2_item);

    case RVAL_TYPE_FNCALL:
        return -1;

    default:
        return -1;
    }

    return true;
}

static bool Epimenides(EvalContext *ctx, const char *ns, const char *scope, const char *var, Rval rval, int level)
{
    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:

        if (StringContainsVar(RvalScalarValue(rval), var))
        {
            Log(LOG_LEVEL_ERR, "Scalar variable '%s' contains itself (non-convergent) '%s'", var, RvalScalarValue(rval));
            return true;
        }

        if (IsCf3VarString(RvalScalarValue(rval)))
        {
            Buffer *exp = BufferNew();
            ExpandScalar(ctx, ns, scope, RvalScalarValue(rval), exp);

            if (strcmp(BufferData(exp), RvalScalarValue(rval)) == 0)
            {
                BufferDestroy(exp);
                return false;
            }

            if (level > 3)
            {
                BufferDestroy(exp);
                return false;
            }

            if (Epimenides(ctx, ns, scope, var, (Rval) { BufferGet(exp), RVAL_TYPE_SCALAR}, level + 1))
            {
                BufferDestroy(exp);
                return true;
            }

            BufferDestroy(exp);
        }

        break;

    case RVAL_TYPE_LIST:
        for (const Rlist *rp = RvalRlistValue(rval); rp != NULL; rp = rp->next)
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

    if (!IsDefinedClass(ctx, pp->classes))
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

        if (strcmp(cp->lval, "ifvarclass") == 0 || strcmp(cp->lval, "if") == 0)
        {
            switch (cp->rval.type)
            {
            case RVAL_TYPE_SCALAR:
                if (!IsDefinedClass(ctx, cp->rval.item))
                {
                    return opts;
                }

                break;

            case RVAL_TYPE_FNCALL:
            {
                bool excluded = false;

                /* eval it: e.g. ifvarclass => not("a_class") */

                Rval res = FnCallEvaluate(ctx, PromiseGetPolicy(pp), cp->rval.item, pp).rval;

                /* Don't continue unless function was evaluated properly */
                if (res.type != RVAL_TYPE_SCALAR)
                {
                    RvalDestroy(res);
                    return opts;
                }

                excluded = !IsDefinedClass(ctx, res.item);

                RvalDestroy(res);

                if (excluded)
                {
                    return opts;
                }
            }
            break;

            default:
                Log(LOG_LEVEL_ERR, "Invalid if/ifvarclass type '%c': should be string or function", cp->rval.type);
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

            opts.ok_redefine &= allow_redefine;
        }
        else if (DataTypeFromString(cp->lval) != CF_DATA_TYPE_NONE)
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
