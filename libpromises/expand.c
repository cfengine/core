/*
   Copyright 2017 Northern.tech AS

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

#define CF_MAPPEDLIST '#'

static PromiseResult ExpandPromiseAndDo(EvalContext *ctx, const Promise *pp,
                                        Rlist *lists, Rlist *containers,
                                        PromiseActuator *ActOnPromise, void *param);
static void ExpandAndMapIteratorsFromScalar(EvalContext *ctx, const Bundle *bundle,
                                            char *string, size_t length, int level,
                                            Rlist **scalars, Rlist **lists,
                                            Rlist **containers,
                                            Rlist **full_expansion);
static void CopyLocalizedReferencesToBundleScope(EvalContext *ctx,
                                                 const Bundle *bundle,
                                                 const Rlist *ref_names);

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

Any list variable occurring within a scalar or in place of a scalar
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

PromiseResult ExpandPromise(EvalContext *ctx, const Promise *pp,
                            PromiseActuator *ActOnPromise, void *param)
{
    if (!IsDefinedClass(ctx, pp->classes))
    {
        return PROMISE_RESULT_SKIPPED;
    }

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

    PromiseResult result = ExpandPromiseAndDo(ctx, pcopy, lists, containers, ActOnPromise, param);

    PromiseDestroy(pcopy);

    RlistDestroy(lists);
    RlistDestroy(scalars);
    RlistDestroy(containers);

    return result;
}

static PromiseResult ExpandPromiseAndDo(EvalContext *ctx, const Promise *pp,
                                        Rlist *lists, Rlist *containers,
                                        PromiseActuator *ActOnPromise, void *param)
{
    const char *handle = PromiseGetHandle(pp);

    EvalContextStackPushPromiseFrame(ctx, pp, true);

    PromiseIterator *iter_ctx = NULL;
    size_t i = 0;
    PromiseResult result = PROMISE_RESULT_SKIPPED;
    Buffer *expbuf = BufferNew();
    iter_ctx = PromiseIteratorNew(ctx, pp, lists, containers);
    /*
     If any of the lists we iterate is null or contains only cf_null values,
     then skip the entire promise.
    */
    if (!NullIterators(iter_ctx))
    {
        do
        {
            if (handle)
            {
                // This ordering is necessary to get automated canonification
                BufferClear(expbuf);
                ExpandScalar(ctx, NULL, "this", handle, expbuf);
                CanonifyNameInPlace(BufferGet(expbuf));
                EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "handle", BufferData(expbuf), CF_DATA_TYPE_STRING, "source=promise");
            }
            else
            {
                EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "handle", PromiseID(pp), CF_DATA_TYPE_STRING, "source=promise");
            }

            const Promise *pexp = EvalContextStackPushPromiseIterationFrame(ctx, i, iter_ctx);
            if (!pexp)
            {
                // excluded
                result = PromiseResultUpdate(result, PROMISE_RESULT_SKIPPED);
                continue;
            }

            PromiseResult iteration_result = ActOnPromise(ctx, pexp, param);

            // Redmine#6484
            // Only during pre-evaluation ActOnPromise is set to be a pointer to
            // CommonEvalPromise. While doing CommonEvalPromise check all the
            // handles should be not collected and dependent promises should not
            // be notified.
            if (ActOnPromise != &CommonEvalPromise)
            {
                NotifyDependantPromises(ctx, pexp, iteration_result);
            }

            result = PromiseResultUpdate(result, iteration_result);

            if (strcmp(pp->parent_promise_type->name, "vars") == 0 || strcmp(pp->parent_promise_type->name, "meta") == 0)
            {
                VerifyVarPromise(ctx, pexp, true);
            }

            EvalContextStackPopFrame(ctx);
            ++i;
        } while (PromiseIteratorNext(iter_ctx));
    }

    BufferDestroy(expbuf);
    PromiseIteratorDestroy(iter_ctx);
    EvalContextStackPopFrame(ctx);

    return result;
}

void MapIteratorsFromRval(EvalContext *ctx, const Bundle *bundle, Rval rval,
                          Rlist **scalars, Rlist **lists, Rlist **containers)
{
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
        ExpandAndMapIteratorsFromScalar(ctx, bundle, RvalFnCallValue(rval)->name,
                                        strlen(RvalFnCallValue(rval)->name), 0, scalars, lists, containers, NULL);
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

static void RlistConcatInto(Rlist **dest,
                            const Rlist *src, const char *extension)
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
        else if (ref_str[i] == '[')
        {
            return;
        }
    }
}

static void ExpandAndMapIteratorsFromScalar(EvalContext *ctx,
                                            const Bundle *bundle,
                                            char *string, size_t length,
                                            int level,
                                            Rlist **scalars, Rlist **lists,
                                            Rlist **containers,
                                            Rlist **full_expansion)
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
        BufferClear(value);
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

            BufferClear(value);

            if (i >= length)
            {
                break;
            }
        }

        if (*sp == '$')
        {
            BufferClear(value);
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

                    DataType value_type = CF_DATA_TYPE_NONE;
                    const void *value = EvalContextVariableGet(ctx, inner_ref, &value_type);
                    if (value)
                    {
                        char *mangled_inner_ref = xstrdup(inner_ref_str);
                        MangleVarRefString(mangled_inner_ref, strlen(mangled_inner_ref));

                        success++;
                        switch (DataTypeToRvalType(value_type))
                        {
                        case RVAL_TYPE_LIST:
                            {
                                bool has_valid_entries = false;
                                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                                {
                                    has_valid_entries |= rp->val.item && (strcmp(rp->val.item, CF_NULL_VALUE) != 0);
                                    if (full_expansion)
                                    {
                                        // append each slist item to each of full_expansion
                                        RlistConcatInto(&tmp_list, *full_expansion, RlistScalarValue(rp));
                                    }
                                }
                                if (!has_valid_entries)
                                {
                                    Log(LOG_LEVEL_DEBUG, "Skipping empty list '%s' in iteration", mangled_inner_ref);
                                    break;
                                }
                                if (level > 0)
                                {
                                    RlistPrependScalarIdemp(lists, mangled_inner_ref);
                                }
                                else
                                {
                                    RlistAppendScalarIdemp(lists, mangled_inner_ref);
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

static Rval ExpandListEntry(EvalContext *ctx,
                            const char *ns, const char *scope,
                            int expandnaked, Rval entry)
{
    if (entry.type == RVAL_TYPE_SCALAR &&
        IsNakedVar(entry.item, '@'))
    {
        if (expandnaked)
        {
            char naked[CF_MAXVARSIZE];
            GetNaked(naked, entry.item);

            if (IsExpandable(naked))
            {
                Buffer *out = BufferNew();
                ExpandScalar(ctx, ns, scope, naked, out);
                strlcpy(naked, BufferData(out), CF_MAXVARSIZE);
                BufferDestroy(out);
            }

            if (!IsExpandable(naked))
            {
                VarRef *ref = VarRefParseFromScope(naked, scope);

                DataType value_type = CF_DATA_TYPE_NONE;
                const void *value = EvalContextVariableGet(ctx, ref, &value_type);
                VarRefDestroy(ref);

                if (value)
                {
                    return ExpandPrivateRval(ctx, ns, scope, value,
                                             DataTypeToRvalType(value_type));
                }
            }
        }
        else
        {
            return RvalNew(entry.item, RVAL_TYPE_SCALAR);
        }
    }

    return ExpandPrivateRval(ctx, ns, scope, entry.item, entry.type);
}

Rlist *ExpandList(EvalContext *ctx,
                  const char *ns, const char *scope,
                  const Rlist *list, int expandnaked)
{
    Rlist *start = NULL;

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        Rval returnval = ExpandListEntry(ctx, ns, scope, expandnaked, rp->val);
        RlistAppend(&start, returnval.item, returnval.type);
        RvalDestroy(returnval);
    }

    return start;
}

/*********************************************************************/

Rval ExpandPrivateRval(EvalContext *ctx,
                       const char *ns, const char *scope,
                       const void *rval_item, RvalType rval_type)
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
        returnval = RvalNew(rval_item, RVAL_TYPE_CONTAINER);
        break;

    case RVAL_TYPE_NOPROMISEE:
        break;
    }

    return returnval;
}

/*********************************************************************/

Rval ExpandBundleReference(EvalContext *ctx,
                           const char *ns, const char *scope,
                           Rval rval)
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

bool ExpandScalar(const EvalContext *ctx,
                  const char *ns, const char *scope, const char *string,
                  Buffer *out)
{
    assert(string);
    if (strlen(string) == 0)
    {
        return true;
    }

    bool fully_expanded = true;

    Buffer *current_item = BufferNew();
    for (const char *sp = string; *sp != '\0'; sp++)
    {
        BufferClear(current_item);
        ExtractScalarPrefix(current_item, sp, strlen(sp));

        BufferAppend(out, BufferData(current_item), BufferSize(current_item));
        sp += BufferSize(current_item);
        if (*sp == '\0')
        {
            break;
        }

        BufferClear(current_item);
        char varstring = sp[1];
        ExtractScalarReference(current_item,  sp, strlen(sp), true);
        sp += BufferSize(current_item) + 2;

        if (IsCf3VarString(BufferData(current_item)))
        {
            Buffer *temp = BufferCopy(current_item);
            BufferClear(current_item);
            ExpandScalar(ctx, ns, scope, BufferData(temp), current_item);
            BufferDestroy(temp);
        }

        if (!IsExpandable(BufferData(current_item)))
        {
            DataType type = CF_DATA_TYPE_NONE;
            const void *value = NULL;
            {
                VarRef *ref = VarRefParseFromNamespaceAndScope(BufferData(current_item), ns, scope, CF_NS, '.');
                value = EvalContextVariableGet(ctx, ref, &type);
                VarRefDestroy(ref);
            }

            switch (DataTypeToRvalType(type))
            {
            case RVAL_TYPE_SCALAR:
                if (value)
                {
                     BufferAppendString(out, value);
                     continue;
                }
                break;

            case RVAL_TYPE_CONTAINER:
                if (value && JsonGetElementType((JsonElement*)value) == JSON_ELEMENT_TYPE_PRIMITIVE)
                {
                    BufferAppendString(out, JsonPrimitiveGetAsString((JsonElement*)value));
                    continue;
                }
                break;

            default:
                // Discover if we are about to evaluate a promise with a cf_null-list
                if ((value && strcmp(RlistScalarValue(value), CF_NULL_VALUE) == 0)
                // or a list from a foreign bundle that can't be expanded because it is a null list there
                   || (type == CF_DATA_TYPE_NONE && !value && strchr(BufferData(current_item), CF_MAPPEDLIST)))
                {
                    BufferClear(out);
                    BufferAppendString(out, CF_NULL_VALUE); // mark as invalid - see ExpandDeRefPromise
                    BufferDestroy(current_item);
                    return false;
                }
                break;
            }
        }

        if (varstring == '{')
        {
            BufferAppendF(out, "${%s}", BufferData(current_item));
        }
        else
        {
            BufferAppendF(out, "$(%s)", BufferData(current_item));
        }
    }

    BufferDestroy(current_item);

    return fully_expanded;
}

/*********************************************************************/


Rval EvaluateFinalRval(EvalContext *ctx, const Policy *policy,
                       const char *ns, const char *scope,
                       Rval rval, bool forcelist, const Promise *pp)
{
    assert(ctx);
    assert(policy);

    Rval returnval;
    if (rval.type == RVAL_TYPE_SCALAR &&
        IsNakedVar(rval.item, '@'))
    {
        /* Treat lists specially here */
        char naked[CF_MAXVARSIZE];
        GetNaked(naked, rval.item);

        if (IsExpandable(naked))
        {
            returnval = ExpandPrivateRval(ctx, NULL, "this", rval.item, rval.type);
        }
        else
        {
            VarRef *ref = VarRefParseFromScope(naked, scope);
            DataType value_type = CF_DATA_TYPE_NONE;
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
    }
    else if (forcelist) /* We are replacing scalar @(name) with list */
    {
        returnval = ExpandPrivateRval(ctx, ns, scope, rval.item, rval.type);
    }
    else if (FnCallIsBuiltIn(rval))
    {
        returnval = RvalCopy(rval);
    }
    else
    {
        returnval = ExpandPrivateRval(ctx, NULL, "this", rval.item, rval.type);
    }

    switch (returnval.type)
    {
    case RVAL_TYPE_SCALAR:
    case RVAL_TYPE_CONTAINER:
        break;

    case RVAL_TYPE_LIST:
        for (Rlist *rp = RvalRlistValue(returnval); rp; rp = rp->next)
        {
            switch (rp->val.type)
            {
            case RVAL_TYPE_FNCALL:
            {
                FnCall *fp = RlistFnCallValue(rp);
                rp->val = FnCallEvaluate(ctx, policy, fp, pp).rval;
                FnCallDestroy(fp);
                break;
            }
            case RVAL_TYPE_SCALAR:
                if (EvalContextStackCurrentPromise(ctx) &&
                    IsCf3VarString(RlistScalarValue(rp)))
                {
                    void *prior = rp->val.item;
                    rp->val = ExpandPrivateRval(ctx, NULL, "this",
                                                prior, RVAL_TYPE_SCALAR);
                    free(prior);
                }
                /* else: returnval unchanged. */
                break;
            default:
                assert(!"Bad type for entry in Rlist");
            }
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
        assert(returnval.item == NULL); /* else we're leaking it */
        returnval.item = NULL;
        returnval.type = RVAL_TYPE_NOPROMISEE;
        break;
    }

    return returnval;
}

/*********************************************************************/

static void CopyLocalizedReferencesToBundleScope(EvalContext *ctx,
                                                 const Bundle *bundle,
                                                 const Rlist *ref_names)
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
                    RlistDestroy(list);
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

void BundleResolvePromiseType(EvalContext *ctx, const Bundle *bundle, const char *type, PromiseActuator *actuator)
{
    for (size_t j = 0; j < SeqLength(bundle->promise_types); j++)
    {
        PromiseType *pt = SeqAt(bundle->promise_types, j);

        if (strcmp(pt->name, type) == 0)
        {
            EvalContextStackPushPromiseTypeFrame(ctx, pt);
            for (size_t i = 0; i < SeqLength(pt->promises); i++)
            {
                Promise *pp = SeqAt(pt->promises, i);
                ExpandPromise(ctx, pp, actuator, NULL);
            }
            EvalContextStackPopFrame(ctx);
        }
    }
}

void BundleResolve(EvalContext *ctx, const Bundle *bundle)
{
    Log(LOG_LEVEL_DEBUG, "Resolving variables in bundle '%s' '%s'",
        bundle->type, bundle->name);

    if (strcmp(bundle->type, "common") == 0)
    {
        BundleResolvePromiseType(ctx, bundle, "classes", VerifyClassPromise);
    }
    BundleResolvePromiseType(ctx, bundle, "vars", (PromiseActuator*)VerifyVarPromise);
}

ProtocolVersion ProtocolVersionParse(const char *s)
{
    if (s == NULL ||
        strcmp(s, "0") == 0 ||
        strcmp(s, "undefined") == 0)
    {
        return CF_PROTOCOL_UNDEFINED;
    }
    if (strcmp(s, "1") == 0 ||
        strcmp(s, "classic") == 0)
    {
        return CF_PROTOCOL_CLASSIC;
    }
    else if (strcmp(s, "2") == 0)
    {
        return CF_PROTOCOL_TLS;
    }
    else if (strcmp(s, "latest") == 0)
    {
        return CF_PROTOCOL_LATEST;
    }
    else
    {
        return CF_PROTOCOL_UNDEFINED;
    }
}

/**
 * Evaluate the relevant control body, and set the
 * relevant fields in #ctx and #config.
 */
static void ResolveControlBody(EvalContext *ctx, GenericAgentConfig *config,
                               const Body *control_body)
{
    const char *filename = control_body->source_path;

    assert(CFG_CONTROLBODY[COMMON_CONTROL_MAX].lval == NULL);

    const ConstraintSyntax *body_syntax = NULL;
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
        FatalError(ctx, "Unknown control body: %s", control_body->type);
    }

    char *scope;
    assert(strcmp(control_body->name, "control") == 0);
    xasprintf(&scope, "control_%s", control_body->type);

    Log(LOG_LEVEL_DEBUG, "Initiate control variable convergence for scope '%s'", scope);

    EvalContextStackPushBodyFrame(ctx, NULL, control_body, NULL);

    for (size_t i = 0; i < SeqLength(control_body->conlist); i++)
    {
        const char *lval;
        Rval evaluated_rval;
        size_t lineno;

        /* Use nested scope to constrain cp. */
        {
            Constraint *cp = SeqAt(control_body->conlist, i);
            lval   = cp->lval;
            lineno = cp->offset.line;

            if (!IsDefinedClass(ctx, cp->classes))
            {
                continue;
            }

            if (strcmp(lval, CFG_CONTROLBODY[COMMON_CONTROL_BUNDLESEQUENCE].lval) == 0)
            {
                evaluated_rval = ExpandPrivateRval(ctx, NULL, scope,
                                                   cp->rval.item, cp->rval.type);
            }
            else
            {
                evaluated_rval = EvaluateFinalRval(ctx, control_body->parent_policy,
                                                   NULL, scope, cp->rval,
                                                   true, NULL);
            }

        } /* Close scope: assert we only use evaluated_rval, not cp->rval. */

        VarRef *ref = VarRefParseFromScope(lval, scope);
        EvalContextVariableRemove(ctx, ref);

        DataType rval_proper_datatype =
            ConstraintSyntaxGetDataType(body_syntax, lval);
        if (evaluated_rval.type != DataTypeToRvalType(rval_proper_datatype))
        {
            Log(LOG_LEVEL_ERR,
                "Attribute '%s' in %s:%zu is of wrong type, skipping",
                lval, filename, lineno);
            VarRefDestroy(ref);
            RvalDestroy(evaluated_rval);
            continue;
        }

        bool success = EvalContextVariablePut(
            ctx, ref, evaluated_rval.item, rval_proper_datatype,
            "source=promise");
        if (!success)
        {
            Log(LOG_LEVEL_ERR,
                "Attribute '%s' in %s:%zu can't be added, skipping",
                lval, filename, lineno);
            VarRefDestroy(ref);
            RvalDestroy(evaluated_rval);
            continue;
        }

        VarRefDestroy(ref);

        if (strcmp(lval, CFG_CONTROLBODY[COMMON_CONTROL_OUTPUT_PREFIX].lval) == 0)
        {
            strlcpy(VPREFIX, RvalScalarValue(evaluated_rval),
                    sizeof(VPREFIX));
        }

        if (strcmp(lval, CFG_CONTROLBODY[COMMON_CONTROL_DOMAIN].lval) == 0)
        {
            strlcpy(VDOMAIN, RvalScalarValue(evaluated_rval),
                    sizeof(VDOMAIN));
            Log(LOG_LEVEL_VERBOSE, "SET domain = %s", VDOMAIN);

            EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_SYS, "domain");
            EvalContextVariableRemoveSpecial(ctx, SPECIAL_SCOPE_SYS, "fqhost");
            snprintf(VFQNAME, CF_MAXVARSIZE, "%s.%s", VUQNAME, VDOMAIN);
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "fqhost",
                                          VFQNAME, CF_DATA_TYPE_STRING,
                                          "inventory,source=agent,attribute_name=Host name");
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "domain",
                                          VDOMAIN, CF_DATA_TYPE_STRING,
                                          "source=agent");
            EvalContextClassPutHard(ctx, VDOMAIN, "source=agent");
        }

        if (strcmp(lval, CFG_CONTROLBODY[COMMON_CONTROL_IGNORE_MISSING_INPUTS].lval) == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "SET ignore_missing_inputs %s",
                RvalScalarValue(evaluated_rval));
            config->ignore_missing_inputs = BooleanFromString(
                RvalScalarValue(evaluated_rval));
        }

        if (strcmp(lval, CFG_CONTROLBODY[COMMON_CONTROL_IGNORE_MISSING_BUNDLES].lval) == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "SET ignore_missing_bundles %s",
                RvalScalarValue(evaluated_rval));
            config->ignore_missing_bundles = BooleanFromString(
                RvalScalarValue(evaluated_rval));
        }

        if (strcmp(lval, CFG_CONTROLBODY[COMMON_CONTROL_CACHE_SYSTEM_FUNCTIONS].lval) == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "SET cache_system_functions %s",
                RvalScalarValue(evaluated_rval));
            bool cache_system_functions = BooleanFromString(
                RvalScalarValue(evaluated_rval));
            EvalContextSetEvalOption(ctx, EVAL_OPTION_CACHE_SYSTEM_FUNCTIONS,
                                     cache_system_functions);
        }

        if (strcmp(lval, CFG_CONTROLBODY[COMMON_CONTROL_PROTOCOL_VERSION].lval) == 0)
        {
            config->protocol_version = ProtocolVersionParse(
                RvalScalarValue(evaluated_rval));
            Log(LOG_LEVEL_VERBOSE, "SET common protocol_version: %s",
                PROTOCOL_VERSION_STRING[config->protocol_version]);
        }

        /* Those are package_inventory and package_module common control body options */
        if (strcmp(lval, CFG_CONTROLBODY[COMMON_CONTROL_PACKAGE_INVENTORY].lval) == 0)
        {
            AddDefaultInventoryToContext(ctx, RvalRlistValue(evaluated_rval));
            Log(LOG_LEVEL_VERBOSE, "SET common package_inventory list");
        }
        if (strcmp(lval, CFG_CONTROLBODY[COMMON_CONTROL_PACKAGE_MODULE].lval) == 0)
        {
            AddDefaultPackageModuleToContext(ctx, RvalScalarValue(evaluated_rval));
            Log(LOG_LEVEL_VERBOSE, "SET common package_module: %s",
                RvalScalarValue(evaluated_rval));
        }

        if (strcmp(lval, CFG_CONTROLBODY[COMMON_CONTROL_GOALPATTERNS].lval) == 0)
        {
            /* Ignored */
        }

        RvalDestroy(evaluated_rval);
    }

    EvalContextStackPopFrame(ctx);
    free(scope);
}

static void ResolvePackageManagerBody(EvalContext *ctx, const Body *pm_body)
{
    PackageModuleBody *new_manager = xcalloc(1, sizeof(PackageModuleBody));
    new_manager->name = SafeStringDuplicate(pm_body->name);

    for (size_t i = 0; i < SeqLength(pm_body->conlist); i++)
    {
        Constraint *cp = SeqAt(pm_body->conlist, i);

        Rval returnval = {0};

        if (IsDefinedClass(ctx, cp->classes))
        {
            returnval = ExpandPrivateRval(ctx, NULL, "body",
                                          cp->rval.item, cp->rval.type);
        }

        if (returnval.item == NULL || returnval.type == RVAL_TYPE_NOPROMISEE)
        {
            Log(LOG_LEVEL_VERBOSE, "have invalid constraint while resolving"
                    "package promise body: %s", cp->lval);

            RvalDestroy(returnval);
            continue;
        }

        if (strcmp(cp->lval, "query_installed_ifelapsed") == 0)
        {
            new_manager->installed_ifelapsed =
                    (int)IntFromString(RvalScalarValue(returnval));
        }
        else if (strcmp(cp->lval, "query_updates_ifelapsed") == 0)
        {
            new_manager->updates_ifelapsed =
                    (int)IntFromString(RvalScalarValue(returnval));
        }
        else if (strcmp(cp->lval, "default_options") == 0)
        {
            new_manager->options = RlistCopy(RvalRlistValue(returnval));
        }
        else
        {
            /* This should be handled by the parser. */
            assert(0);
        }
        RvalDestroy(returnval);
    }
    AddPackageModuleToContext(ctx, new_manager);
}

void PolicyResolve(EvalContext *ctx, const Policy *policy,
                   GenericAgentConfig *config)
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
        /* Collect all package managers data from policy as we don't know yet
         * which ones we will use. */
        else if (strcmp(bdp->type, "package_module") == 0)
        {
            ResolvePackageManagerBody(ctx, bdp);
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
        strlcpy(s2, s1, CF_MAXVARSIZE);
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

PromiseResult CommonEvalPromise(EvalContext *ctx, const Promise *pp,
                                ARG_UNUSED void *param)
{
    assert(param == NULL);

    if (SHOWREPORTS)
    {
        ShowPromise(pp);
    }

    PromiseRecheckAllConstraints(ctx, pp);

    return PROMISE_RESULT_NOOP;
}
