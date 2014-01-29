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

#include <expand.h>

#include <misc_lib.h>
#include <eval_context.h>
#include <policy.h>
#include <promises.h>
#include <vars.h>
#include <syntax.h>
#include <files_names.h>
#include <scope.h>
#include <matching.h>
#include <unix.h>
#include <attributes.h>
#include <fncall.h>
#include <iteration.h>
#include <audit.h>
#include <verify_vars.h>
#include <string_lib.h>
#include <conversion.h>
#include <verify_classes.h>


static PromiseResult ExpandPromiseAndDo(EvalContext *ctx,
                                        size_t pass,
                                        const Promise *pp,
                                        Rlist *lists,
                                        Rlist *containers,
                                        PromiseActuator *ActOnPromise,
                                        void *param);
static void ExpandAndMapIteratorsFromScalar(EvalContext *ctx, const Bundle *bundle, char *string, size_t length, int level,
                                            Rlist **scalars, Rlist **lists, Rlist **containers, Rlist **full_expansion);
static void CopyLocalizedReferencesToBundleScope(EvalContext *ctx, const Bundle *bundle, const Rlist *ref_names);

/*

Expanding variables is easy -- expanding lists automagically requires
some thought. Remember that

promiser <=> RVAL_TYPE_SCALAR
promisee <=> RVAL_TYPE_LIST

Expanding all bodies in the constraint list, we have

lval <=> RVAL_TYPE_LIST|RVAL_TYPE_SCALAR

Now the rule for variable substitution is that any list variable @(name)
substituted directly for a LIST is not iterated, but dropped into
place, i.e. in list-lvals and the promisee (since this would be
equivalent to a re-concatenation of the expanded separate promises)

Any list variable occuring within a scalar or in place of a scalar
is assumed to be iterated i.e. $(name).

To expand a promise, we build temporary hash tables. There are two
stages, to this - one is to create a promise copy including all of the
body templates and translate the parameters. This requires one round
of expansion with scopeid "body". Then we use this fully assembled promise
and expand vars and function calls.

To expand the variables in a promise we need to 

   -- first get all strings, also parameterized bodies, which
      could also be lists
                                                                     /
        //  MapIteratorsFromRval("scope",&lol,"ksajd$(one)$(two)...$(three)"); \/ 
        
   -- compile an ordered list of variables involved , with types -           /
      assume all are lists - these are never inside sub-bodies by design,  \/
      so all expansion data are in the promise itself
      can also be variables based on list items - derived like arrays x[i]

   -- Copy the promise to a temporary promise + constraint list, expanding one by one,   /
      then execute that                                                                \/

      -- In a sub-bundle, create a new context and make hashes of the the
      transferred variables in the temporary context

   -- bodies cannot contain iterators

   -- we've already checked types of lhs rhs, must match so an iterator
      can only be in a non-naked variable?
   
   -- form the outer loops to generate combinations

Note, we map the current context into a fluid context "this" that maps
every list into a scalar during iteration. Thus "this" never contains
lists. This presents a problem for absolute references like $(abs.var),
since these cannot be mapped into "this" without some magic.
   
**********************************************************************/

PromiseResult ExpandPromise(EvalContext *ctx, size_t pass, const Promise *pp, PromiseActuator *ActOnPromise, void *param)
{
    Rlist *lists = NULL;
    Rlist *scalars = NULL;
    Rlist *containers = NULL;

    Promise *pcopy = DeRefCopyPromise(ctx, pp);

    MapIteratorsFromRval(ctx, PromiseGetBundle(pp), (Rval) { pcopy->promiser, RVAL_TYPE_SCALAR }, &scalars, &lists, &containers);

    if (pcopy->promisee.item != NULL)
    {
        MapIteratorsFromRval(ctx, PromiseGetBundle(pp), pp->promisee, &scalars, &lists, &containers);
    }

    for (size_t i = 0; i < SeqLength(pcopy->conlist); i++)
    {
        Constraint *cp = SeqAt(pcopy->conlist, i);
        MapIteratorsFromRval(ctx, PromiseGetBundle(pp), cp->rval, &scalars, &lists, &containers);
    }

    CopyLocalizedReferencesToBundleScope(ctx, PromiseGetBundle(pp), lists);
    CopyLocalizedReferencesToBundleScope(ctx, PromiseGetBundle(pp), scalars);
    CopyLocalizedReferencesToBundleScope(ctx, PromiseGetBundle(pp), containers);

    PromiseResult result = ExpandPromiseAndDo(ctx, pass, pcopy, lists, containers, ActOnPromise, param);

    PromiseDestroy(pcopy);

    RlistDestroy(lists);
    RlistDestroy(scalars);
    RlistDestroy(containers);

    return result;
}

static PromiseResult ExpandPromiseAndDo(EvalContext *ctx, size_t pass, const Promise *pp, Rlist *lists, Rlist *containers, PromiseActuator *ActOnPromise, void *param)
{
    const char *handle = PromiseGetHandle(pp);

    EvalContextStackPushPromiseFrame(ctx, pp, true, pass);
    {
        char *stack_path = EvalContextStackPath(ctx);
        Log(LOG_LEVEL_VERBOSE, "%s/%s/'%s' Evaluating promise", stack_path, pp->parent_promise_type->name, pp->promiser);
        free(stack_path);
    }

    PromiseIterator *iter_ctx = NULL;
    size_t i = 0;
    PromiseResult result = PROMISE_RESULT_NOOP;
    Buffer *expbuf = BufferNew();
    for (iter_ctx = PromiseIteratorNew(ctx, pp, lists, containers); PromiseIteratorHasMore(iter_ctx); i++, PromiseIteratorNext(iter_ctx))
    {
        if (handle)
        {
            // This ordering is necessary to get automated canonification
            BufferZero(expbuf);
            ExpandScalar(ctx, NULL, "this", handle, expbuf);
            CanonifyNameInPlace(BufferGet(expbuf));
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "handle", BufferData(expbuf), DATA_TYPE_STRING, "source=promise");
        }
        else
        {
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "handle", PromiseID(pp), DATA_TYPE_STRING, "source=promise");
        }

        const Promise *pexp = EvalContextStackPushPromiseIterationFrame(ctx, i, iter_ctx);
        PromiseResult iteration_result = ActOnPromise(ctx, pexp, param);
        result = PromiseResultUpdate(result, iteration_result);

        if (strcmp(pp->parent_promise_type->name, "vars") == 0 || strcmp(pp->parent_promise_type->name, "meta") == 0)
        {
            VerifyVarPromise(ctx, pexp, true);
        }

        EvalContextStackPopFrame(ctx);
    }

    BufferDestroy(expbuf);
    PromiseIteratorDestroy(iter_ctx);
    EvalContextStackPopFrame(ctx);

    return result;
}


Rval ExpandDanglers(EvalContext *ctx, const char *ns, const char *scope, Rval rval, const Promise *pp)
{
    assert(ctx);
    assert(pp);

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        if (IsCf3VarString(RvalScalarValue(rval)))
        {
            return EvaluateFinalRval(ctx, PromiseGetPolicy(pp), ns, scope, rval, false, pp);
        }
        else
        {
            return RvalCopy(rval);
        }
        break;

    case RVAL_TYPE_LIST:
        {
            Rlist *result_list = RlistCopy(RvalRlistValue(rval));
            RlistFlatten(ctx, &result_list);
            return RvalNew(result_list, RVAL_TYPE_LIST);
        }
        break;

    case RVAL_TYPE_CONTAINER:
    case RVAL_TYPE_FNCALL:
    case RVAL_TYPE_NOPROMISEE:
        return RvalCopy(rval);
    }

    ProgrammingError("Unhandled Rval type");
}

/*********************************************************************/

void MapIteratorsFromRval(EvalContext *ctx, const Bundle *bundle, Rval rval, Rlist **scalars, Rlist **lists, Rlist **containers)
{
    assert(rval.item);
    if (rval.item == NULL)
    {
        return;
    }

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        {
            char *val = RvalScalarValue(rval);
            size_t val_len = strlen(val);
            ExpandAndMapIteratorsFromScalar(ctx, bundle, val, val_len, 0, scalars, lists, containers, NULL);
        }
        break;

    case RVAL_TYPE_LIST:
        for (const Rlist *rp = RvalRlistValue(rval); rp; rp = rp->next)
        {
            MapIteratorsFromRval(ctx, bundle, rp->val, scalars, lists, containers);
        }
        break;

    case RVAL_TYPE_FNCALL:
        for (const Rlist *rp = RvalFnCallValue(rval)->args; rp; rp = rp->next)
        {
            Log(LOG_LEVEL_DEBUG, "Looking at arg for function-like object '%s'", RvalFnCallValue(rval)->name);
            MapIteratorsFromRval(ctx, bundle, rp->val, scalars, lists, containers);
        }
        break;

    case RVAL_TYPE_CONTAINER:
    case RVAL_TYPE_NOPROMISEE:
        Log(LOG_LEVEL_DEBUG, "Unknown Rval type for scope '%s'", bundle->name);
        break;
    }
}

/*********************************************************************/

static void RlistConcatInto(Rlist **dest, const Rlist *src, const char *extension)
{
    assert(dest);

    size_t count = 0;
    for (const Rlist *rp = src; rp != NULL; rp = rp->next)
    {
        count++;
        char temp[CF_EXPANDSIZE] = "";
        snprintf(temp, CF_EXPANDSIZE, "%s%s", RlistScalarValue(rp), extension);
        RlistAppendScalarIdemp(dest, temp);
    }

    if (count == 0)
    {
        RlistAppendScalarIdemp(dest, extension);
    }
}

static void MangleVarRefString(char *ref_str, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (ref_str[i] == ':')
        {
            ref_str[i] = '*';
        }
        else if (ref_str[i] == '.')
        {
            ref_str[i] = '#';
        }
        else if (ref_str[i] == '\0' || ref_str[i] == '[')
        {
            return;
        }
    }
}

static void DeMangleVarRefString(char *ref_str, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (ref_str[i] == '*')
        {
            ref_str[i] = ':';
        }
        else if (ref_str[i] == '#')
        {
            ref_str[i] = '.';
        }
    }
}

static void ExpandAndMapIteratorsFromScalar(EvalContext *ctx, const Bundle *bundle, char *string, size_t length,
                                            int level, Rlist **scalars, Rlist **lists,
                                            Rlist **containers, Rlist **full_expansion)
{
    assert(string);
    if (!string)
    {
        return;
    }

    Buffer *value = BufferNew();

    for (size_t i = 0; i < length; i++)
    {
        const char *sp = string + i;

        Rlist *tmp_list = NULL;
        BufferZero(value);
        if (ExtractScalarPrefix(value, sp, length - i))
        {
            if (full_expansion)
            {
                RlistConcatInto(&tmp_list, *full_expansion, BufferData(value));
                RlistDestroy(*full_expansion);
                *full_expansion = tmp_list;
                tmp_list = NULL;
            }

            sp += BufferSize(value);
            i += BufferSize(value);

            BufferZero(value);

            if (i >= length)
            {
                break;
            }
        }

        if (*sp == '$')
        {
            BufferZero(value);
            ExtractScalarReference(value, sp, length - i, true);
            if (BufferSize(value) > 0)
            {
                Rlist *inner_expansion = NULL;
                Rlist *exp = NULL;
                int success = 0;

                VarRef *ref = VarRefParse(BufferData(value));

                int increment = BufferSize(value) - 1 + 3;

                // Handle any embedded variables
                char *substring = string + i + 2;
                ExpandAndMapIteratorsFromScalar(ctx, bundle, substring, BufferSize(value), level+1, scalars, lists, containers, &inner_expansion);

                for (exp = inner_expansion; exp != NULL; exp = exp->next)
                {
                    // If a list is non-local, i.e. $(bundle.var), map it to local $(bundle#var)

                    // NB without modifying variables as we map them, it's not
                    // possible to handle remote lists referenced by a variable
                    // scope. For example:
                    //  scope => "test."; var => "somelist"; $($(scope)$(var)) fails
                    //  varname => "test.somelist"; $($(varname)) also fails
                    // TODO Unless the consumer handles it?

                    const char *inner_ref_str = RlistScalarValue(exp);
                    VarRef *inner_ref = VarRefParseFromBundle(inner_ref_str, bundle);

                    // var is the expanded name of the variable in its native context
                    // finalname will be the mapped name in the local context "this."

                    DataType value_type = DATA_TYPE_NONE;
                    const void *value = EvalContextVariableGet(ctx, inner_ref, &value_type);
                    if (value)
                    {
                        char *mangled_inner_ref = xstrdup(inner_ref_str);
                        MangleVarRefString(mangled_inner_ref, strlen(mangled_inner_ref));

                        success++;
                        switch (DataTypeToRvalType(value_type))
                        {
                        case RVAL_TYPE_LIST:
                            if (level > 0)
                            {
                                RlistPrependScalarIdemp(lists, mangled_inner_ref);
                            }
                            else
                            {
                                RlistAppendScalarIdemp(lists, mangled_inner_ref);
                            }

                            if (full_expansion)
                            {
                                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                                {
                                    // append each slist item to each of full_expansion
                                    RlistConcatInto(&tmp_list, *full_expansion, RlistScalarValue(rp));
                                }
                            }
                            break;

                        case RVAL_TYPE_SCALAR:
                            RlistAppendScalarIdemp(scalars, mangled_inner_ref);

                            if (full_expansion)
                            {
                                // append the scalar value to each of full_expansion
                                RlistConcatInto(&tmp_list, *full_expansion, value);
                            }
                            break;

                        case RVAL_TYPE_CONTAINER:
                            if (level > 0)
                            {
                                RlistPrependScalarIdemp(containers, mangled_inner_ref);
                            }
                            else
                            {
                                RlistAppendScalarIdemp(containers, mangled_inner_ref);
                            }
                            break;

                        case RVAL_TYPE_FNCALL:
                        case RVAL_TYPE_NOPROMISEE:
                            break;
                        }

                        free(mangled_inner_ref);
                    }

                    VarRefDestroy(inner_ref);
                }
                RlistDestroy(inner_expansion);

                if (full_expansion)
                {
                    RlistDestroy(*full_expansion);
                    *full_expansion = tmp_list;
                    tmp_list = NULL;
                }

                // No need to map this.* even though it's technically qualified
                if (success && IsQualifiedVariable(BufferData(value)) && strcmp(ref->scope, "this") != 0)
                {
                    char *dotpos = strchr(substring, '.');
                    if (dotpos)
                    {
                        *dotpos = CF_MAPPEDLIST;    // replace '.' with '#'
                    }

                    if (strchr(BufferData(value), ':'))
                    {
                        char *colonpos = strchr(substring, ':');
                        if (colonpos)
                        {
                            *colonpos = '*';
                        }
                    }
                }

                VarRefDestroy(ref);

                sp += increment;
                i += increment;
            }
        }
    }

    BufferDestroy(value);
}

/*********************************************************************/

Rlist *ExpandList(EvalContext *ctx, const char *ns, const char *scope, const Rlist *list, int expandnaked)
{
    Rlist *start = NULL;
    Rval returnval;

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (!expandnaked && (rp->val.type == RVAL_TYPE_SCALAR) && IsNakedVar(RlistScalarValue(rp), '@'))
        {
            returnval = RvalNew(RlistScalarValue(rp), RVAL_TYPE_SCALAR);
        }
        else if ((rp->val.type == RVAL_TYPE_SCALAR) && IsNakedVar(RlistScalarValue(rp), '@'))
        {
            char naked[CF_MAXVARSIZE];
            GetNaked(naked, RlistScalarValue(rp));

            if (!IsExpandable(naked))
            {
                VarRef *ref = VarRefParseFromScope(naked, scope);

                DataType value_type = DATA_TYPE_NONE;
                const void *value = EvalContextVariableGet(ctx, ref, &value_type);
                if (value)
                {
                    returnval = ExpandPrivateRval(ctx, ns, scope, value, DataTypeToRvalType(value_type));
                }
                else
                {
                    returnval = ExpandPrivateRval(ctx, ns, scope, rp->val.item, rp->val.type);
                }

                VarRefDestroy(ref);
            }
            else
            {
                returnval = ExpandPrivateRval(ctx, ns, scope, rp->val.item, rp->val.type);
            }
        }
        else
        {
            returnval = ExpandPrivateRval(ctx, ns, scope, rp->val.item, rp->val.type);
        }

        RlistAppend(&start, returnval.item, returnval.type);
        RvalDestroy(returnval);
    }

    return start;
}

/*********************************************************************/

Rval ExpandPrivateRval(EvalContext *ctx, const char *ns, const char *scope, const void *rval_item, RvalType rval_type)
{
    Rval returnval;
    returnval.item = NULL;
    returnval.type = RVAL_TYPE_NOPROMISEE;

    switch (rval_type)
    {
    case RVAL_TYPE_SCALAR:
        {
            Buffer *buffer = BufferNew();
            ExpandScalar(ctx, ns, scope, rval_item, buffer);
            returnval = (Rval) { BufferClose(buffer),  RVAL_TYPE_SCALAR };
        }
        break;

    case RVAL_TYPE_LIST:
        returnval.item = ExpandList(ctx, ns, scope, rval_item, true);
        returnval.type = RVAL_TYPE_LIST;
        break;

    case RVAL_TYPE_FNCALL:
        returnval.item = ExpandFnCall(ctx, ns, scope, rval_item);
        returnval.type = RVAL_TYPE_FNCALL;
        break;

    case RVAL_TYPE_CONTAINER:
        returnval = RvalNew(JsonCopy(rval_item), RVAL_TYPE_CONTAINER);
        break;

    case RVAL_TYPE_NOPROMISEE:
        break;
    }

    return returnval;
}

/*********************************************************************/

Rval ExpandBundleReference(EvalContext *ctx, const char *ns, const char *scope, Rval rval)
{
    // Allocates new memory for the copy
    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        {
            Buffer *buffer = BufferNew();
            ExpandScalar(ctx, ns, scope, RvalScalarValue(rval), buffer);
            return (Rval) { BufferClose(buffer), RVAL_TYPE_SCALAR };
        }

    case RVAL_TYPE_FNCALL:
        return (Rval) {ExpandFnCall(ctx, ns, scope, RvalFnCallValue(rval)), RVAL_TYPE_FNCALL};

    case RVAL_TYPE_CONTAINER:
    case RVAL_TYPE_LIST:
    case RVAL_TYPE_NOPROMISEE:
         return RvalNew(NULL, RVAL_TYPE_NOPROMISEE);
    }

    assert(false);
    return RvalNew(NULL, RVAL_TYPE_NOPROMISEE);
}


bool ExpandScalar(const EvalContext *ctx, const char *ns, const char *scope, const char *string, Buffer *out)
{
    assert(string);

    bool returnval = true;

    if (strlen(string) == 0)
    {
        return false;
    }

    // TODO: cleanup, optimize this mess
    Buffer *var = BufferNew();
    Buffer *current_item = BufferNew();
    Buffer *temp = BufferNew();
    for (const char *sp = string; /* No exit */ ; sp++)     /* check for varitems */
    {
        char varstring = false;
        size_t increment = 0;

        if (*sp == '\0')
        {
            break;
        }

        BufferZero(current_item);
        ExtractScalarPrefix(current_item, sp, strlen(sp));

        BufferAppend(out, BufferData(current_item), BufferSize(current_item));
        sp += BufferSize(current_item);

        if (*sp == '\0')
        {
            break;
        }

        BufferZero(var);
        if (*sp == '$')
        {
            switch (*(sp + 1))
            {
            case '(':
                varstring = ')';
                ExtractScalarReference(var, sp, strlen(sp), false);
                if (BufferSize(var) == 0)
                {
                    BufferAppendChar(out, '$');
                    continue;
                }
                break;

            case '{':
                varstring = '}';
                ExtractScalarReference(var, sp, strlen(sp), false);
                if (BufferSize(var) == 0)
                {
                    BufferAppendChar(out, '$');
                    continue;
                }
                break;

            default:
                BufferAppendChar(out, '$');
                continue;
            }
        }

        BufferZero(current_item);
        {
            BufferZero(temp);
            ExtractScalarReference(temp, sp, strlen(sp), true);

            if (IsCf3VarString(BufferData(temp)))
            {
                ExpandScalar(ctx, ns, scope, BufferData(temp), current_item);
            }
            else
            {
                BufferAppend(current_item, BufferData(temp), BufferSize(temp));
            }
        }

        increment = BufferSize(var) - 1;

        char name[CF_MAXVARSIZE] = "";
        if (!IsExpandable(BufferData(current_item)))
        {
            DataType type = DATA_TYPE_NONE;
            const void *value = NULL;
            {
                VarRef *ref = VarRefParseFromNamespaceAndScope(BufferData(current_item), ns, scope, CF_NS, '.');
                value = EvalContextVariableGet(ctx, ref, &type);
                VarRefDestroy(ref);
            }

            if (value)
            {
                switch (type)
                {
                case DATA_TYPE_STRING:
                case DATA_TYPE_INT:
                case DATA_TYPE_REAL:
                    BufferAppend(out, value, strlen(value));
                    break;

                case DATA_TYPE_STRING_LIST:
                case DATA_TYPE_INT_LIST:
                case DATA_TYPE_REAL_LIST:
                case DATA_TYPE_NONE:
                    if (varstring == '}')
                    {
                        snprintf(name, CF_MAXVARSIZE, "${%s}", BufferData(current_item));
                    }
                    else
                    {
                        snprintf(name, CF_MAXVARSIZE, "$(%s)", BufferData(current_item));
                    }

                    BufferAppend(out, name, strlen(name));
                    returnval = false;
                    break;

                default:
                    Log(LOG_LEVEL_DEBUG, "Returning Unknown Scalar ('%s' => '%s')", string, BufferData(out));
                    BufferDestroy(var);
                    BufferDestroy(current_item);
                    BufferDestroy(temp);
                    return false;

                }
            }
            else
            {
                if (varstring == '}')
                {
                    snprintf(name, CF_MAXVARSIZE, "${%s}", BufferData(current_item));
                }
                else
                {
                    snprintf(name, CF_MAXVARSIZE, "$(%s)", BufferData(current_item));
                }

                BufferAppend(out, name, strlen(name));
                returnval = false;
            }
        }

        sp += increment;
        BufferZero(current_item);
    }

    BufferDestroy(var);
    BufferDestroy(current_item);
    BufferDestroy(temp);

    return returnval;
}

/*********************************************************************/


Rval EvaluateFinalRval(EvalContext *ctx, const Policy *policy, const char *ns, const char *scope,
                       Rval rval, bool forcelist, const Promise *pp)
{
    assert(ctx);
    assert(policy);

    Rval returnval, newret;
    if ((rval.type == RVAL_TYPE_SCALAR) && IsNakedVar(rval.item, '@'))        /* Treat lists specially here */
    {
        char naked[CF_MAXVARSIZE];
        GetNaked(naked, rval.item);

        if (!IsExpandable(naked))
        {
            VarRef *ref = VarRefParseFromScope(naked, scope);
            DataType value_type = DATA_TYPE_NONE;
            const void *value = EvalContextVariableGet(ctx, ref, &value_type);

            if (!value || DataTypeToRvalType(value_type) != RVAL_TYPE_LIST)
            {
                returnval = ExpandPrivateRval(ctx, NULL, "this", rval.item, rval.type);
            }
            else
            {
                returnval.item = ExpandList(ctx, ns, scope, value, true);
                returnval.type = RVAL_TYPE_LIST;
            }

            VarRefDestroy(ref);
        }
        else
        {
            returnval = ExpandPrivateRval(ctx, NULL, "this", rval.item, rval.type);
        }
    }
    else
    {
        if (forcelist)          /* We are replacing scalar @(name) with list */
        {
            returnval = ExpandPrivateRval(ctx, ns, scope, rval.item, rval.type);
        }
        else
        {
            if (FnCallIsBuiltIn(rval))
            {
                returnval = RvalCopy(rval);
            }
            else
            {
                returnval = ExpandPrivateRval(ctx, NULL, "this", rval.item, rval.type);
            }
        }
    }

    switch (returnval.type)
    {
    case RVAL_TYPE_SCALAR:
    case RVAL_TYPE_CONTAINER:
        break;

    case RVAL_TYPE_LIST:
        for (Rlist *rp = RvalRlistValue(returnval); rp; rp = rp->next)
        {
            if (rp->val.type == RVAL_TYPE_FNCALL)
            {
                FnCall *fp = RlistFnCallValue(rp);
                FnCallResult res = FnCallEvaluate(ctx, policy, fp, pp);

                FnCallDestroy(fp);
                rp->val = res.rval;
            }
            else
            {
                if (EvalContextStackCurrentPromise(ctx))
                {
                    if (IsCf3VarString(RlistScalarValue(rp)))
                    {
                        newret = ExpandPrivateRval(ctx, NULL, "this", rp->val.item, rp->val.type);
                        free(rp->val.item);
                        rp->val.item = newret.item;
                    }
                }
            }

            /* returnval unchanged */
        }
        break;

    case RVAL_TYPE_FNCALL:
        if (FnCallIsBuiltIn(returnval))
        {
            FnCall *fp = RvalFnCallValue(returnval);
            returnval = FnCallEvaluate(ctx, policy, fp, pp).rval;
            FnCallDestroy(fp);
        }
        break;

    default:
        returnval.item = NULL;
        returnval.type = RVAL_TYPE_NOPROMISEE;
        break;
    }

    return returnval;
}

/*********************************************************************/

static void CopyLocalizedReferencesToBundleScope(EvalContext *ctx, const Bundle *bundle, const Rlist *ref_names)
{
    for (const Rlist *rp = ref_names; rp != NULL; rp = rp->next)
    {
        const char *mangled = RlistScalarValue(rp);
        char *demangled = xstrdup(mangled);
        DeMangleVarRefString(demangled, strlen(demangled));

        if (strchr(RlistScalarValue(rp), CF_MAPPEDLIST))
        {
            VarRef *demangled_ref = VarRefParseFromBundle(demangled, bundle);

            DataType value_type;
            const void *value = EvalContextVariableGet(ctx, demangled_ref, &value_type);
            if (!value)
            {
                ProgrammingError("Couldn't find extracted variable '%s'", mangled);
            }

            VarRef *mangled_ref = VarRefParseFromBundle(mangled, bundle);

            switch (DataTypeToRvalType(value_type))
            {
            case RVAL_TYPE_LIST:
                {
                    Rlist *list = RlistCopy(value);
                    RlistFlatten(ctx, &list);

                    EvalContextVariablePut(ctx, mangled_ref, list, value_type, "source=agent");
                }
                break;

            case RVAL_TYPE_CONTAINER:
            case RVAL_TYPE_SCALAR:
                EvalContextVariablePut(ctx, mangled_ref, value, value_type, "source=agent");
                break;

            case RVAL_TYPE_FNCALL:
            case RVAL_TYPE_NOPROMISEE:
                ProgrammingError("Illegal rval type in switch %d", DataTypeToRvalType(value_type));
            }

            VarRefDestroy(mangled_ref);
            VarRefDestroy(demangled_ref);
        }

        free(demangled);
    }
}

static void ResolveCommonClassPromises(EvalContext *ctx, PromiseType *pt)
{
    assert(strcmp("classes", pt->name) == 0);
    assert(strcmp(pt->parent_bundle->type, "common") == 0);

    for (size_t i = 0; i < SeqLength(pt->promises); i++)
    {
        Promise *pp = SeqAt(pt->promises, i);

        char *sp = NULL;
        if (VarClassExcluded(ctx, pp, &sp))
        {
            if (LEGACY_OUTPUT)
            {
                Log(LOG_LEVEL_VERBOSE, "\n");
                Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
                Log(LOG_LEVEL_VERBOSE, "Skipping whole next promise (%s), as var-context %s is not relevant", pp->promiser, sp);
                Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Skipping next promise '%s', as var-context '%s' is not relevant", pp->promiser, sp);
            }
            continue;
        }

        ExpandPromise(ctx, 0, pp, VerifyClassPromise, NULL);
    }
}

static void ResolveVariablesPromises(EvalContext *ctx, PromiseType *pt)
{
    assert(strcmp("vars", pt->name) == 0);

    for (size_t i = 0; i < SeqLength(pt->promises); i++)
    {
        Promise *pp = SeqAt(pt->promises, i);
        EvalContextStackPushPromiseFrame(ctx, pp, false, 0);
        EvalContextStackPushPromiseIterationFrame(ctx, 0, NULL);
        VerifyVarPromise(ctx, pp, false);
        EvalContextStackPopFrame(ctx);
        EvalContextStackPopFrame(ctx);
    }
}

void BundleResolve(EvalContext *ctx, const Bundle *bundle)
{
    Log(LOG_LEVEL_DEBUG, "Resolving variables in bundle '%s' '%s'", bundle->type, bundle->name);

    for (size_t j = 0; j < SeqLength(bundle->promise_types); j++)
    {
        PromiseType *sp = SeqAt(bundle->promise_types, j);

        if (strcmp(bundle->type, "common") == 0 && strcmp(sp->name, "classes") == 0)
        {
            ResolveCommonClassPromises(ctx, sp);
        }
    }

    for (size_t j = 0; j < SeqLength(bundle->promise_types); j++)
    {
        PromiseType *sp = SeqAt(bundle->promise_types, j);

        if (strcmp(sp->name, "vars") == 0)
        {
            ResolveVariablesPromises(ctx, sp);
        }
    }

}

static void ResolveControlBody(EvalContext *ctx, GenericAgentConfig *config, const Body *control_body)
{
    const ConstraintSyntax *body_syntax = NULL;
    Rval returnval;

    assert(strcmp(control_body->name, "control") == 0);

    for (int i = 0; CONTROL_BODIES[i].constraints != NULL; i++)
    {
        body_syntax = CONTROL_BODIES[i].constraints;

        if (strcmp(control_body->type, CONTROL_BODIES[i].body_type) == 0)
        {
            break;
        }
    }

    if (body_syntax == NULL)
    {
        FatalError(ctx, "Unknown agent");
    }

    char scope[CF_BUFSIZE];
    snprintf(scope, CF_BUFSIZE, "%s_%s", control_body->name, control_body->type);
    Log(LOG_LEVEL_DEBUG, "Initiate control variable convergence for scope '%s'", scope);

    EvalContextStackPushBodyFrame(ctx, NULL, control_body, NULL);

    for (size_t i = 0; i < SeqLength(control_body->conlist); i++)
    {
        Constraint *cp = SeqAt(control_body->conlist, i);

        if (!IsDefinedClass(ctx, cp->classes))
        {
            continue;
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_BUNDLESEQUENCE].lval) == 0)
        {
            returnval = ExpandPrivateRval(ctx, NULL, scope, cp->rval.item, cp->rval.type);
        }
        else
        {
            returnval = EvaluateFinalRval(ctx, control_body->parent_policy, NULL, scope, cp->rval, true, NULL);
        }

        VarRef *ref = VarRefParseFromScope(cp->lval, scope);
        EvalContextVariableRemove(ctx, ref);

        if (!EvalContextVariablePut(ctx, ref, returnval.item, ConstraintSyntaxGetDataType(body_syntax, cp->lval), "source=promise"))
        {
            Log(LOG_LEVEL_ERR, "Rule from %s at/before line %zu", control_body->source_path, cp->offset.line);
        }

        VarRefDestroy(ref);

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_OUTPUT_PREFIX].lval) == 0)
        {
            strncpy(VPREFIX, returnval.item, CF_MAXVARSIZE);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_DOMAIN].lval) == 0)
        {
            strcpy(VDOMAIN, cp->rval.item);
            Log(LOG_LEVEL_VERBOSE, "SET domain = %s", VDOMAIN);
            EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_SYS, "domain");
            EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_SYS, "fqhost");
            snprintf(VFQNAME, CF_MAXVARSIZE, "%s.%s", VUQNAME, VDOMAIN);
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "fqhost", VFQNAME, DATA_TYPE_STRING, "inventory,source=agent,group=Host name");
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "domain", VDOMAIN, DATA_TYPE_STRING, "source=agent");
            EvalContextClassPutHard(ctx, VDOMAIN, "source=agent");
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_IGNORE_MISSING_INPUTS].lval) == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "SET ignore_missing_inputs %s", RvalScalarValue(cp->rval));
            config->ignore_missing_inputs = BooleanFromString(cp->rval.item);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_IGNORE_MISSING_BUNDLES].lval) == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "SET ignore_missing_bundles %s", RvalScalarValue(cp->rval));
            config->ignore_missing_bundles = BooleanFromString(cp->rval.item);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_CACHE_SYSTEM_FUNCTIONS].lval) == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "SET cache_system_functions %s", RvalScalarValue(cp->rval));
            bool cache_system_functions = BooleanFromString(RvalScalarValue(cp->rval));
            EvalContextSetEvalOption(ctx, EVAL_OPTION_CACHE_SYSTEM_FUNCTIONS,
                                     cache_system_functions);
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_GOALPATTERNS].lval) == 0)
        {
            /* Ignored */
            continue;
        }

        RvalDestroy(returnval);
    }

    EvalContextStackPopFrame(ctx);
}

void PolicyResolve(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config)
{
    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bundle = SeqAt(policy->bundles, i);
        if (strcmp("common", bundle->type) == 0)
        {
            EvalContextStackPushBundleFrame(ctx, bundle, NULL, false);
            BundleResolve(ctx, bundle);
            EvalContextStackPopFrame(ctx);
        }
    }

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bundle = SeqAt(policy->bundles, i);
        if (strcmp("common", bundle->type) != 0)
        {
            EvalContextStackPushBundleFrame(ctx, bundle, NULL, false);
            BundleResolve(ctx, bundle);
            EvalContextStackPopFrame(ctx);
        }
    }

    for (size_t i = 0; i < SeqLength(policy->bodies); i++)
    {
        Body *bdp = SeqAt(policy->bodies, i);

        if (strcmp(bdp->name, "control") == 0)
        {
            ResolveControlBody(ctx, config, bdp);
        }
    }
}

bool IsExpandable(const char *str)
{
    char left = 'x', right = 'x';
    int dollar = false;
    int bracks = 0, vars = 0;

    for (const char *sp = str; *sp != '\0'; sp++)   /* check for varitems */
    {
        switch (*sp)
        {
        case '$':
            if (*(sp + 1) == '{' || *(sp + 1) == '(')
            {
                dollar = true;
            }
            break;
        case '(':
        case '{':
            if (dollar)
            {
                left = *sp;
                bracks++;
            }
            break;
        case ')':
        case '}':
            if (dollar)
            {
                bracks--;
                right = *sp;
            }
            break;
        }

        if (left == '(' && right == ')' && dollar && (bracks == 0))
        {
            vars++;
            dollar = false;
        }

        if (left == '{' && right == '}' && dollar && (bracks == 0))
        {
            vars++;
            dollar = false;
        }
    }

    if (bracks != 0)
    {
        Log(LOG_LEVEL_DEBUG, "If this is an expandable variable string then it contained syntax errors");
        return false;
    }

    if (vars > 0)
    {
        Log(LOG_LEVEL_DEBUG,
            "Expanding variable '%s': found %d variables", str, vars);
    }
    return (vars > 0);
}

/*********************************************************************/

bool IsNakedVar(const char *str, char vtype)
{
    int count = 0;

    if (str == NULL || strlen(str) == 0)
    {
        return false;
    }

    char last = *(str + strlen(str) - 1);

    if (strlen(str) < 3)
    {
        return false;
    }

    if (*str != vtype)
    {
        return false;
    }

    switch (*(str + 1))
    {
    case '(':
        if (last != ')')
        {
            return false;
        }
        break;

    case '{':
        if (last != '}')
        {
            return false;
        }
        break;

    default:
        return false;
        break;
    }

    for (const char *sp = str; *sp != '\0'; sp++)
    {
        switch (*sp)
        {
        case '(':
        case '{':
        case '[':
            count++;
            break;
        case ')':
        case '}':
        case ']':
            count--;

            /* The last character must be the end of the variable */

            if (count == 0 && strlen(sp) > 1)
            {
                return false;
            }
            break;
        }
    }

    if (count != 0)
    {
        return false;
    }

    return true;
}

/*********************************************************************/

void GetNaked(char *s2, const char *s1)
/* copy @(listname) -> listname */
{
    if (strlen(s1) < 4)
    {
        Log(LOG_LEVEL_ERR, "Naked variable expected, but '%s' is malformed", s1);
        strncpy(s2, s1, CF_MAXVARSIZE - 1);
        return;
    }

    memset(s2, 0, CF_MAXVARSIZE);
    strncpy(s2, s1 + 2, strlen(s1) - 3);
}

/*********************************************************************/

bool IsVarList(const char *var)
{
    if ('@' != var[0])
    {
        return false;
    }
    /*
     * Minimum size for a list is 4:
     * '@' + '(' + name + ')'
     */
    if (strlen(var) < 4)
    {
        return false;
    }
    return true;
}

PromiseResult CommonEvalPromise(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert(param == NULL);

    if (SHOWREPORTS)
    {
        ShowPromise(pp);
    }

    PromiseRecheckAllConstraints(ctx, pp);

    return PROMISE_RESULT_NOOP;
}
