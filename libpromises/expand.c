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


/**
 * VARIABLES AND PROMISE EXPANSION
 *
 * Expanding variables is easy -- expanding lists automagically requires
 * some thought. Remember that
 *
 * promiser <=> RVAL_TYPE_SCALAR
 * promisee <=> RVAL_TYPE_LIST
 *
 * For bodies we have
 *
 * lval <=> RVAL_TYPE_LIST | RVAL_TYPE_SCALAR
 *
 * Any list or container variable occurring within a scalar or in place of a
 * scalar is assumed to be iterated i.e. $(name). See comments in iteration.c.
 *
 * Any list variable @(name) is *not iterated*, but dropped into place (see
 * DeRefCopyPromise()).
 *
 * Please note that bodies cannot contain iterators.
 *
 * The full process of promise and variable expansion is mostly outlined in
 * ExpandPromise() and ExpandPromiseAndDo() and the basic steps are:
 *
 * + Skip everything if the class guard is not defined.
 *
 * + DeRefCopyPromise(): *Copy the promise* while expanding '@' slists and body
 *   arguments and handling body inheritance. This requires one round of
 *   expansion with scopeid "body".
 *
 * + Push promise frame
 *
 * + MapIteratorsFromRval(): Parse all strings (promiser-promisee-constraints),
 *   find all unexpanded variables, mangle them if needed (if they are
 *   namespaced/scoped), and *initialise the wheels* in the iteration engine
 *   (iterctx) to iterate over iterable variables (slists and containers). See
 *   comments in iteration.c for further details.
 *
 * + For every iteration:
 *
 *   - Push iteration frame
 *
 *   - EvalContextStackPushPromiseIterationFrame()->ExpandDeRefPromise(): Make
 *     another copy of the promise with all constraints evaluated and variables
 *     expanded.
 *
 *     -- NOTE: As a result all *functions are also evaluated*, even if they are
 *        not to be used immediately (for example promises that the actuator skips
 *        because of ifvarclass, see promises.c:ExpandDeRefPromise() ).
 *
 *        -- (TODO IS IT CORRECT?) In a sub-bundle, create a new context and make
 *           hashes of the the transferred variables in the temporary context
 *
 *   - Run the actuator (=act_on_promise= i.e. =VerifyWhateverPromise()=)
 *
 *   - Pop iteration frame
 *
 * + Pop promise frame
 *
 */


static void PutHandleVariable(EvalContext *ctx, const Promise *pp)
{
    char *handle_s;
    const char *existing_handle = PromiseGetHandle(pp);

    if (existing_handle != NULL)
    {
        // This ordering is necessary to get automated canonification
        handle_s = ExpandScalar(ctx, NULL, "this", existing_handle, NULL);
        CanonifyNameInPlace(handle_s);
    }
    else
    {
        handle_s = xstrdup(PromiseID(pp));                /* default handle */
    }

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS,
                                  "handle", handle_s,
                                  CF_DATA_TYPE_STRING, "source=promise");
    free(handle_s);
}

/**
 * Recursively go down the #rval and run PromiseIteratorPrepare() to take note
 * of all iterables and mangle all rvals than need to be mangled before
 * iterating.
 */
static void MapIteratorsFromRval(EvalContext *ctx,
                                 PromiseIterator *iterctx,
                                 Rval rval)
{
    switch (rval.type)
    {

    case RVAL_TYPE_SCALAR:
        PromiseIteratorPrepare(iterctx, ctx, RvalScalarValue(rval));
        break;

    case RVAL_TYPE_LIST:
        for (const Rlist *rp = RvalRlistValue(rval);
             rp != NULL; rp = rp->next)
        {
            MapIteratorsFromRval(ctx, iterctx, rp->val);
        }
        break;

    case RVAL_TYPE_FNCALL:
    {
        char *fn_name = RvalFnCallValue(rval)->name;

        /* Check function name. */
        PromiseIteratorPrepare(iterctx, ctx, fn_name);

        /* Check each of the function arguments. */
        /* EXCEPT on functions that use special variables: the mangled
         * variables would never be resolved if they contain inner special
         * variables (for example "$(bundle.A[$(this.k)])" and the returned
         * slist would contained mangled vars like "bundle#A[1]" which would
         * never resolve in future iterations. By skipping the iteration
         * engine for now, the function returns an slist with unmangled
         * entries, and the iteration engine works correctly on the next
         * pass! */
        if (strcmp(fn_name, "maplist") != 0 &&
            strcmp(fn_name, "mapdata") != 0 &&
            strcmp(fn_name, "maparray")!= 0)
        {
            for (Rlist *rp = RvalFnCallValue(rval)->args;
                 rp != NULL;  rp = rp->next)
            {
                MapIteratorsFromRval(ctx, iterctx, rp->val);
            }
        }
        break;
    }

    case RVAL_TYPE_CONTAINER:
    case RVAL_TYPE_NOPROMISEE:
        break;
    }
}

static PromiseResult ExpandPromiseAndDo(EvalContext *ctx, PromiseIterator *iterctx,
                                        PromiseActuator *act_on_promise, void *param)
{
    PromiseResult result = PROMISE_RESULT_SKIPPED;

    /* TODO this loop could be completely skipped for for non vars/classes if
     *      act_on_promise is CommonEvalPromise(). */
    while (PromiseIteratorNext(iterctx, ctx))
    {
        /*
         * ACTUAL WORK PART 1: Get a (another) copy of the promise.
         *
         * Basically this evaluates all constraints.  As a result it evaluates
         * all functions, even if they are not to be used immediately (for
         * example promises that the actuator skips because of ifvarclass).
         */
        const Promise *pexp =                           /* expanded promise */
            EvalContextStackPushPromiseIterationFrame(ctx, iterctx);
        if (pexp == NULL)                       /* is the promise excluded? */
        {
            result = PromiseResultUpdate(result, PROMISE_RESULT_SKIPPED);
            continue;
        }

        /* ACTUAL WORK PART 2: run the actuator */
        PromiseResult iteration_result = act_on_promise(ctx, pexp, param);

        /* iteration_result is always NOOP for PRE-EVAL. */
        result = PromiseResultUpdate(result, iteration_result);

        /* Redmine#6484: Do not store promise handles during PRE-EVAL, to
         *               avoid package promise always running. */
        if (act_on_promise != &CommonEvalPromise)
        {
            NotifyDependantPromises(ctx, pexp, iteration_result);
        }

        /* EVALUATE VARS PROMISES again, allowing redefinition of
         * variables. The theory behind this is that the "sampling rate" of
         * vars promise needs to be double than the rest. */
        if (strcmp(pexp->parent_promise_type->name, "vars") == 0 ||
            strcmp(pexp->parent_promise_type->name, "meta") == 0)
        {
            if (act_on_promise != &VerifyVarPromise)
            {
                VerifyVarPromise(ctx, pexp, NULL);
            }
        }

        /* Why do we push/pop an iteration frame, if all iterated variables
         * are Put() on the previous scope? */
        EvalContextStackPopFrame(ctx);
    }

    return result;
}

PromiseResult ExpandPromise(EvalContext *ctx, const Promise *pp,
                            PromiseActuator *act_on_promise, void *param)
{
    if (!IsDefinedClass(ctx, pp->classes))
    {
        return PROMISE_RESULT_SKIPPED;
    }

    /* 1. Copy the promise while expanding '@' slists and body arguments
     *    (including body inheritance). */
    Promise *pcopy = DeRefCopyPromise(ctx, pp);

    EvalContextStackPushPromiseFrame(ctx, pcopy, true);
    PromiseIterator *iterctx = PromiseIteratorNew(pcopy);

    /* 2. Parse all strings (promiser-promisee-constraints), find all
          unexpanded variables, mangle them if needed (if they are
          namespaced/scoped), and start the iteration engine (iterctx) to
          iterate over slists and containers. */

    MapIteratorsFromRval(ctx, iterctx,
                         (Rval) { pcopy->promiser, RVAL_TYPE_SCALAR });

    if (pcopy->promisee.item != NULL)
    {
        MapIteratorsFromRval(ctx, iterctx, pcopy->promisee);
    }

    for (size_t i = 0; i < SeqLength(pcopy->conlist); i++)
    {
        Constraint *cp = SeqAt(pcopy->conlist, i);
        MapIteratorsFromRval(ctx, iterctx, cp->rval);
    }

    /* 3. GO! */
    PutHandleVariable(ctx, pcopy);
    PromiseResult result = ExpandPromiseAndDo(ctx, iterctx,
                                              act_on_promise, param);

    EvalContextStackPopFrame(ctx);
    PromiseIteratorDestroy(iterctx);
    PromiseDestroy(pcopy);

    return result;
}


/*********************************************************************/
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
        returnval.item = ExpandScalar(ctx, ns, scope, rval_item, NULL);
        returnval.type = RVAL_TYPE_SCALAR;
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
                char *exp = ExpandScalar(ctx, ns, scope, naked, NULL);
                strlcpy(naked, exp, sizeof(naked));             /* TODO err */
                free(exp);
            }

            /* Check again, it might have changed. */
            if (!IsExpandable(naked))
            {
                VarRef *ref = VarRefParseFromScope(naked, scope);

                DataType value_type;
                const void *value = EvalContextVariableGet(ctx, ref, &value_type);
                VarRefDestroy(ref);

                if (value_type != CF_DATA_TYPE_NONE)     /* variable found? */
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

Rval ExpandBundleReference(EvalContext *ctx,
                           const char *ns, const char *scope,
                           Rval rval)
{
    // Allocates new memory for the copy
    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        return (Rval) { ExpandScalar(ctx, ns, scope, RvalScalarValue(rval), NULL),
                        RVAL_TYPE_SCALAR };

    case RVAL_TYPE_FNCALL:
        return (Rval) { ExpandFnCall(ctx, ns, scope, RvalFnCallValue(rval)),
                        RVAL_TYPE_FNCALL};

    case RVAL_TYPE_CONTAINER:
    case RVAL_TYPE_LIST:
    case RVAL_TYPE_NOPROMISEE:
         return RvalNew(NULL, RVAL_TYPE_NOPROMISEE);
    }

    assert(false);
    return RvalNew(NULL, RVAL_TYPE_NOPROMISEE);
}

/**
 * Expand a #string into Buffer #out, returning the pointer to the string
 * itself, inside the Buffer #out. If #out is NULL then the buffer will be
 * created and destroyed internally.
 *
 * @retval NULL something went wrong
 */
char *ExpandScalar(const EvalContext *ctx, const char *ns, const char *scope,
                   const char *string, Buffer *out)
{
    bool out_belongs_to_us = false;

    if (out == NULL)
    {
        out               = BufferNew();
        out_belongs_to_us = true;
    }

    assert(string != NULL);
    assert(out != NULL);
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
            VarRef *ref = VarRefParseFromNamespaceAndScope(
                BufferData(current_item),
                ns, scope, CF_NS, '.');
            DataType value_type;
            const void *value = EvalContextVariableGet(ctx, ref, &value_type);
            VarRefDestroy(ref);

            switch (DataTypeToRvalType(value_type))
            {
            case RVAL_TYPE_SCALAR:
                assert(value != NULL);
                BufferAppendString(out, value);
                continue;
                break;

            case RVAL_TYPE_CONTAINER:
            {
                assert(value != NULL);
                const JsonElement *jvalue = value;      /* instead of casts */
                if (JsonGetElementType(jvalue) == JSON_ELEMENT_TYPE_PRIMITIVE)
                {
                    BufferAppendString(out, JsonPrimitiveGetAsString(jvalue));
                    continue;
                }
                break;
            }
            default:
                /* TODO Log() */
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

    LogDebug(LOG_MOD_EXPAND, "ExpandScalar( %s : %s . %s )  =>  %s",
             SAFENULL(ns), SAFENULL(scope), string, BufferData(out));

    return out_belongs_to_us ? BufferClose(out) : BufferGet(out);
}

/*********************************************************************/

Rval EvaluateFinalRval(EvalContext *ctx, const Policy *policy,
                       const char *ns, const char *scope,
                       Rval rval, bool forcelist, const Promise *pp)
{
    assert(ctx);
    assert(policy);
    Rval returnval;

    /* Treat lists specially. */
    if (rval.type == RVAL_TYPE_SCALAR && IsNakedVar(rval.item, '@'))
    {
        char naked[CF_MAXVARSIZE];
        GetNaked(naked, rval.item);

        if (IsExpandable(naked))                /* example: @(blah_$(blue)) */
        {
            returnval = ExpandPrivateRval(ctx, NULL, "this", rval.item, rval.type);
        }
        else
        {
            VarRef *ref = VarRefParseFromScope(naked, scope);
            DataType value_type;
            const void *value = EvalContextVariableGet(ctx, ref, &value_type);
            VarRefDestroy(ref);

            if (DataTypeToRvalType(value_type) == RVAL_TYPE_LIST)
            {
                returnval.item = ExpandList(ctx, ns, scope, value, true);
                returnval.type = RVAL_TYPE_LIST;
            }
            else
            {
                returnval = ExpandPrivateRval(ctx, NULL, "this", rval.item, rval.type);
            }
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
    Log(LOG_LEVEL_DEBUG,
        "Resolving classes and variables in 'bundle %s %s'",
        bundle->type, bundle->name);

    /* PRE-EVAL: evaluate classes of common bundles. */
    if (strcmp(bundle->type, "common") == 0)
    {
        /* Necessary to parse vars *before* classes for cases like this:
         * 00_basics/04_bundles/dynamic_bundlesequence/dynamic_inputs_based_on_class_set_using_variable_file_control_extends_inputs.cf.sub
         *   --  see bundle "classify". */
        BundleResolvePromiseType(ctx, bundle, "vars", VerifyVarPromise);

        BundleResolvePromiseType(ctx, bundle, "classes", VerifyClassPromise);
    }

    /* Necessary to also parse vars *after* classes,
     * because "inputs" might be affected in cases like:
     * 00_basics/04_bundles/dynamic_bundlesequence/dynamic_inputs_based_on_list_variable_dependent_on_class.cf */
    BundleResolvePromiseType(ctx, bundle, "vars", VerifyVarPromise);
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
    /* PRE-EVAL: common bundles: classes,vars. */
    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bundle = SeqAt(policy->bundles, i);
        if (strcmp("common", bundle->type) == 0)
        {
            EvalContextStackPushBundleFrame(ctx, bundle, NULL, false);
            BundleResolve(ctx, bundle);            /* PRE-EVAL classes,vars */
            EvalContextStackPopFrame(ctx);
        }
    }

/*
 * HACK: yet another pre-eval pass here, WHY? TODO remove, but test fails:
 *       00_basics/03_bodies/dynamic_inputs_findfiles.cf
 */
#if 1

    /* PRE-EVAL: non-common bundles: only vars. */
    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bundle = SeqAt(policy->bundles, i);
        if (strcmp("common", bundle->type) != 0)
        {
            EvalContextStackPushBundleFrame(ctx, bundle, NULL, false);
            BundleResolve(ctx, bundle);                    /* PRE-EVAL vars */
            EvalContextStackPopFrame(ctx);
        }
    }

#endif

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

static char opposite(char c)
{
    switch (c)
    {
    case '(':  return ')';
    case '{':  return '}';
    default :  ProgrammingError("Was expecting '(' or '{' but got: '%c'", c);
    }
    return 0;
}

/**
 * Check if #str contains one and only one variable expansion of #vtype kind
 * (it's usually either '$' or '@'). It can contain nested expansions which
 * are not checked properly. Examples:
 *     true:  "$(whatever)", "${whatever}", "$(blah$(blue))"
 *     false: "$(blah)blue", "blah$(blue)", "$(blah)$(blue)", "$(blah}"
 */
bool IsNakedVar(const char *str, char vtype)
{
    size_t len = strlen(str);
    char last  = len > 0 ? str[len-1] : 0;

    if (len < 3
        || str[0] != vtype
        || (str[1] != '(' && str[1] != '{')
        || last != opposite(str[1]))
    {
        return false;
    }

    /* TODO check if nesting happens correctly? Is it needed? */
    size_t count = 0;
    for (const char *sp = str; *sp != '\0'; sp++)
    {
        switch (*sp)
        {
        case '(':
        case '{':
            count++;
            break;
        case ')':
        case '}':
            count--;

            /* Make sure the end of the variable is the last character. */
            if (count == 0 && sp[1] != '\0')
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

/**
 * Copy @(listname) -> listname.
 *
 * This function performs no validations, it is necessary to call the
 * validation functions before calling this function.
 *
 * @NOTE make sure sizeof(dst) >= sizeof(s)
 */
void GetNaked(char *dst, const char *s)
{
    size_t s_len = strlen(s);

    if (s_len < 4  ||  s_len + 3 >= CF_MAXVARSIZE)
    {
        Log(LOG_LEVEL_ERR,
            "@(variable) expected, but got malformed: %s", s);
        strlcpy(dst, s, CF_MAXVARSIZE);
        return;
    }

    memcpy(dst, &s[2], s_len - 3);
    dst[s_len - 3] = '\0';
}

/*********************************************************************/

/**
 * Checks if a variable is an @-list and returns true or false.
 */
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

    PromiseRecheckAllConstraints(ctx, pp);

    return PROMISE_RESULT_NOOP;
}
