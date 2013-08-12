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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <expand.h>

#include <misc_lib.h>
#include <env_context.h>
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
#include <args.h>
#include <iteration.h>
#include <audit.h>
#include <verify_vars.h>
#include <string_lib.h>
#include <conversion.h>
#include <verify_classes.h>


static void ExpandPromiseAndDo(EvalContext *ctx, const Promise *pp, Rlist *listvars,
                               PromiseActuator *ActOnPromise, void *param);
static void ExpandAndMapIteratorsFromScalar(EvalContext *ctx, const char *scope, Rlist **list_vars_out, Rlist **scalar_vars_out,
                                            Rlist **full_expansion, const char *string, size_t length, int level);
static void SetAnyMissingDefaults(EvalContext *ctx, Promise *pp);
static void CopyLocalizedIteratorsToBundleScope(EvalContext *ctx, const Bundle *bundle, const Rlist *listvars);
static void CopyLocalizedScalarsToBundleScope(EvalContext *ctx, const Bundle *bundle, const Rlist *scalars);
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

void ExpandPromise(EvalContext *ctx, Promise *pp, PromiseActuator *ActOnPromise, void *param)
{
    Rlist *listvars = NULL;
    Rlist *scalars = NULL;

    // Set a default for packages here...general defaults that need to come before
    //fix me wth a general function SetMissingDefaults
    SetAnyMissingDefaults(ctx, pp);

    Promise *pcopy = DeRefCopyPromise(ctx, pp);

    MapIteratorsFromRval(ctx, PromiseGetBundle(pp)->name, &listvars, &scalars, (Rval) { pcopy->promiser, RVAL_TYPE_SCALAR });

    if (pcopy->promisee.item != NULL)
    {
        MapIteratorsFromRval(ctx, PromiseGetBundle(pp)->name, &listvars, &scalars, pp->promisee);
    }

    for (size_t i = 0; i < SeqLength(pcopy->conlist); i++)
    {
        Constraint *cp = SeqAt(pcopy->conlist, i);
        MapIteratorsFromRval(ctx, PromiseGetBundle(pp)->name, &listvars, &scalars, cp->rval);
    }

    CopyLocalizedIteratorsToBundleScope(ctx, PromiseGetBundle(pp), listvars);
    CopyLocalizedScalarsToBundleScope(ctx, PromiseGetBundle(pp), scalars);

    ExpandPromiseAndDo(ctx, pcopy, listvars, ActOnPromise, param);

    PromiseDestroy(pcopy);
    RlistDestroy(listvars);
    RlistDestroy(scalars);
}

static void ExpandPromiseAndDo(EvalContext *ctx, const Promise *pp, Rlist *listvars, PromiseActuator *ActOnPromise, void *param)
{
    Rlist *lol = NULL;
    Promise *pexp;
    const int cf_null_cutoff = 5;
    const char *handle = PromiseGetHandle(pp);
    char v[CF_MAXVARSIZE];
    int cutoff = 0;

    EvalContextStackPushPromiseFrame(ctx, pp, true);

    lol = NewIterationContext(ctx, pp, listvars);

    if (lol && EndOfIteration(lol))
    {
        DeleteIterationContext(lol);
        EvalContextStackPopFrame(ctx);
        return;
    }

    while (NullIterators(lol))
    {
        IncrementIterationContext(lol);

        // In case a list is completely blank
        if (cutoff++ > cf_null_cutoff)
        {
            break;
        }
    }

    if (lol && EndOfIteration(lol))
    {
        DeleteIterationContext(lol);
        EvalContextStackPopFrame(ctx);
        return;
    }

    do
    {
        if (RlistLen(listvars) != RlistLen(lol))
        {
            ProgrammingError("Name list %d, dereflist %d", RlistLen(listvars), RlistLen(lol));
        }

        EvalContextStackPushPromiseIterationFrame(ctx, lol);
        char number[CF_SMALLBUF];

        /* Allow $(this.handle) etc variables */

        if (PromiseGetBundle(pp)->source_path)
        {
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promise_filename",PromiseGetBundle(pp)->source_path, DATA_TYPE_STRING);
            snprintf(number, CF_SMALLBUF, "%zu", pp->offset.line);
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promise_linenumber", number, DATA_TYPE_STRING);
        }

        snprintf(v, CF_MAXVARSIZE, "%d", (int) getuid());
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser_uid", v, DATA_TYPE_INT);
        snprintf(v, CF_MAXVARSIZE, "%d", (int) getgid());
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser_gid", v, DATA_TYPE_INT);

        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "bundle", PromiseGetBundle(pp)->name, DATA_TYPE_STRING);
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "namespace", PromiseGetNamespace(pp), DATA_TYPE_STRING);

        /* Must expand $(this.promiser) here for arg dereferencing in things
           like edit_line and methods, but we might have to
           adjust again later if the value changes  -- need to qualify this
           so we don't expand too early for some other promsies */

        if (pp->has_subbundles)
        {
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "promiser", pp->promiser, DATA_TYPE_STRING);
        }

        if (handle)
        {
            char tmp[CF_EXPANDSIZE];
            // This ordering is necessary to get automated canonification
            ExpandScalar(ctx, NULL, "this", handle, tmp);
            CanonifyNameInPlace(tmp);
            Log(LOG_LEVEL_DEBUG, "Expanded handle to '%s'", tmp);
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "handle", tmp, DATA_TYPE_STRING);
        }
        else
        {
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_THIS, "handle", PromiseID(pp), DATA_TYPE_STRING);
        }

        /* End special variables */

        pexp = ExpandDeRefPromise(ctx, pp);

        assert(ActOnPromise);
        ActOnPromise(ctx, pexp, param);

        if (strcmp(pp->parent_promise_type->name, "vars") == 0 || strcmp(pp->parent_promise_type->name, "meta") == 0)
        {
            VerifyVarPromise(ctx, pexp, true);
        }

        PromiseDestroy(pexp);

        EvalContextStackPopFrame(ctx);
        /* End thread monitor */
    }
    while (IncrementIterationContext(lol));

    DeleteIterationContext(lol);
    EvalContextStackPopFrame(ctx);
}


Rval ExpandDanglers(EvalContext *ctx, const char *ns, const char *scope, Rval rval, const Promise *pp)
{
    Rval final;

    /* If there is still work left to do, expand and replace alloc */

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:

        if (IsCf3VarString(rval.item))
        {
            final = EvaluateFinalRval(ctx, ns, scope, rval, false, pp);
        }
        else
        {
            final = RvalCopy(rval);
        }
        break;

    case RVAL_TYPE_LIST:
        final = RvalCopy(rval);
        {
            Rlist *final_list = RvalRlistValue(final);
            RlistFlatten(ctx, &final_list);
            final.item = final_list;
        }
        break;

    default:
        final = RvalCopy(rval);
        break;
    }

    return final;
}

/*********************************************************************/

void MapIteratorsFromRval(EvalContext *ctx, const char *scopeid, Rlist **listvars, Rlist **scalars, Rval rval)
{
    Rlist *rp;
    FnCall *fp;

    if (rval.item == NULL)
    {
        return;
    }

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        {
            char *val = (char *)rval.item;
            ExpandAndMapIteratorsFromScalar(ctx, scopeid, listvars, scalars, NULL, val, strlen(val), 0);
        }
        break;

    case RVAL_TYPE_LIST:
        for (rp = (Rlist *) rval.item; rp != NULL; rp = rp->next)
        {
            MapIteratorsFromRval(ctx, scopeid, listvars, scalars, (Rval) {rp->item, rp->type});
        }
        break;

    case RVAL_TYPE_FNCALL:
        fp = (FnCall *) rval.item;

        for (rp = (Rlist *) fp->args; rp != NULL; rp = rp->next)
        {
            Log(LOG_LEVEL_DEBUG, "Looking at arg for function-like object '%s'", fp->name);
            MapIteratorsFromRval(ctx, scopeid, listvars, scalars, (Rval) {rp->item, rp->type});
        }
        break;

    default:
        Log(LOG_LEVEL_DEBUG, "Unknown Rval type for scope '%s'", scopeid);
        break;
    }
}

/*********************************************************************/

static void RlistConcatInto(Rlist **dest, const Rlist *src, const char *extension)
{
    char temp[CF_EXPANDSIZE];
    const Rlist *it;
    int count = 0;

    if (!dest)
    {
        return;
    }

    for (it = src; it != NULL; it = it->next)
    {
        count++;
        snprintf(temp, CF_EXPANDSIZE, "%s%s", (char*)it->item, extension);
        RlistAppendScalarIdemp(dest, temp);
    }

    if (count == 0)
    {
        RlistAppendScalarIdemp(dest, extension);
    }
}

static void ExpandAndMapIteratorsFromScalar(EvalContext *ctx, const char *scopeid, Rlist **list_vars_out, Rlist **scalar_vars_out,
                                            Rlist **full_expansion, const char *string, size_t length, int level)
{
    char *sp;
    Rval rval;
    Rlist *tmp_list = NULL;
    char v[CF_BUFSIZE], buffer[CF_BUFSIZE];

    if (string == NULL)
    {
        return;
    }

    if (length >= CF_BUFSIZE)
    {
        ProgrammingError("ExpandAndMapIteratorsFromScalar called with invalid strlen");
    }

    strncpy(buffer, string, length);
    buffer[length] = '\0';

    for (sp = buffer; (*sp != '\0'); sp++)
    {
        v[0] = '\0';

        sscanf(sp, "%[^$]", v);

        if (full_expansion)
        {
            RlistConcatInto(&tmp_list, *full_expansion, v);
            RlistDestroy(*full_expansion);
            *full_expansion = tmp_list;
            tmp_list = NULL;
        }

        sp += strlen(v);

        if (*sp == '\0')
        {
            break;
        }

        if (*sp == '$')
        {
            if (ExtractInnerCf3VarString(sp, v))
            {
                Rlist *inner_expansion = NULL;
                Rlist *exp, *tmp;
                int success = 0;
                int increment;

                VarRef *ref = VarRefParseFromScope(v, scopeid);

                increment = strlen(v) - 1 + 3;

                // Handle any embedded variables
                const char *substring = string + (sp - buffer) + 2;
                ExpandAndMapIteratorsFromScalar(ctx, scopeid, list_vars_out, scalar_vars_out, &inner_expansion, substring, strlen(v), level+1);

                for (exp = inner_expansion; exp != NULL; exp = exp->next)
                {
                    // If a list is non-local, i.e. $(bundle.var), map it to local $(bundle#var)

                    // NB without modifying variables as we map them, it's not
                    // possible to handle remote lists referenced by a variable
                    // scope. For example:
                    //  scope => "test."; var => "somelist"; $($(scope)$(var)) fails
                    //  varname => "test.somelist"; $($(varname)) also fails
                    // TODO Unless the consumer handles it?

                    VarRef *inner_ref = VarRefParseFromScope(exp->item, scopeid);

                    // var is the expanded name of the variable in its native context
                    // finalname will be the mapped name in the local context "this."

                    if (EvalContextVariableGet(ctx, inner_ref, &rval, NULL))
                    {
                        char *mangled_inner_ref = IsQualifiedVariable(exp->item) ? VarRefMangle(inner_ref) : xstrdup(exp->item);

                        success++;
                        if (rval.type == RVAL_TYPE_LIST)
                        {
                            /* embedded iterators should be incremented fastest,
                               so order list -- and MUST return de-scoped name
                               else list expansion cannot map var to this.name */
                            if (level > 0)
                            {
                                RlistPrependScalarIdemp(list_vars_out, mangled_inner_ref);
                            }
                            else
                            {
                                RlistAppendScalarIdemp(list_vars_out, mangled_inner_ref);
                            }

                            if (full_expansion)
                            {
                                for (tmp = rval.item; tmp != NULL; tmp = tmp->next)
                                {
                                    // append each slist item to each of full_expansion
                                    RlistConcatInto(&tmp_list, *full_expansion, tmp->item);
                                }
                            }
                        }
                        else if (rval.type == RVAL_TYPE_SCALAR)
                        {
                            RlistAppendScalarIdemp(scalar_vars_out, mangled_inner_ref);

                            if (full_expansion)
                            {
                                // append the scalar value to each of full_expansion
                                RlistConcatInto(&tmp_list, *full_expansion, rval.item);
                            }
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
                if (success && IsQualifiedVariable(v) && strcmp(ref->scope, "this") != 0)
                {
                    char *dotpos = strchr(substring, '.');
                    if (dotpos)
                    {
                        *dotpos = CF_MAPPEDLIST;    // replace '.' with '#'
                    }

                    if (strchr(v, ':'))
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
            }
        }
    }
}

/*********************************************************************/

Rlist *ExpandList(EvalContext *ctx, const char *ns, const char *scope, const Rlist *list, int expandnaked)
{
    Rlist *rp, *start = NULL;
    Rval returnval;
    char naked[CF_MAXVARSIZE];

    for (rp = (Rlist *) list; rp != NULL; rp = rp->next)
    {
        if (!expandnaked && (rp->type == RVAL_TYPE_SCALAR) && IsNakedVar(rp->item, '@'))
        {
            returnval.item = xstrdup(rp->item);
            returnval.type = RVAL_TYPE_SCALAR;
        }
        else if ((rp->type == RVAL_TYPE_SCALAR) && IsNakedVar(rp->item, '@'))
        {
            GetNaked(naked, rp->item);

            if (!IsExpandable(naked))
            {
                VarRef *ref = VarRefParseFromScope(naked, scope);

                if (EvalContextVariableGet(ctx, ref, &returnval, NULL))
                {
                    returnval = ExpandPrivateRval(ctx, ns, scope, returnval);
                }
                else
                {
                    returnval = ExpandPrivateRval(ctx, ns, scope, (Rval) {rp->item, rp->type});
                }

                VarRefDestroy(ref);
            }
            else
            {
                returnval = ExpandPrivateRval(ctx, ns, scope, (Rval) {rp->item, rp->type});
            }
        }
        else
        {
            returnval = ExpandPrivateRval(ctx, ns, scope, (Rval) {rp->item, rp->type});
        }

        RlistAppend(&start, returnval.item, returnval.type);
        RvalDestroy(returnval);
    }

    return start;
}

/*********************************************************************/

Rval ExpandPrivateRval(EvalContext *ctx, const char *ns, const char *scope, Rval rval)
{
    char buffer[CF_EXPANDSIZE];
    FnCall *fp, *fpe;
    Rval returnval;

    // Allocates new memory for the copy
    returnval.item = NULL;
    returnval.type = RVAL_TYPE_NOPROMISEE;

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:

        ExpandScalar(ctx, ns, scope, (char *) rval.item, buffer);
        returnval.item = xstrdup(buffer);
        returnval.type = RVAL_TYPE_SCALAR;
        break;

    case RVAL_TYPE_LIST:

        returnval.item = ExpandList(ctx, ns, scope, rval.item, true);
        returnval.type = RVAL_TYPE_LIST;
        break;

    case RVAL_TYPE_FNCALL:

        /* Note expand function does not mean evaluate function, must preserve type */
        fp = (FnCall *) rval.item;
        fpe = ExpandFnCall(ctx, ns, scope, fp);
        returnval.item = fpe;
        returnval.type = RVAL_TYPE_FNCALL;
        break;

    default:
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
        char buffer[CF_EXPANDSIZE];

        ExpandScalar(ctx, ns, scope, (char *) rval.item, buffer);
        return (Rval) {xstrdup(buffer), RVAL_TYPE_SCALAR};
    }

    case RVAL_TYPE_FNCALL:
    {
        /* Note expand function does not mean evaluate function, must preserve type */
        FnCall *fp = (FnCall *) rval.item;

        return (Rval) {ExpandFnCall(ctx, ns, scope, fp), RVAL_TYPE_FNCALL};
    }

    default:
        return (Rval) {NULL, RVAL_TYPE_NOPROMISEE };
    }
}

/*********************************************************************/

static bool ExpandOverflow(const char *str1, const char *str2)
{
    int len = strlen(str2);

    if ((strlen(str1) + len) > (CF_EXPANDSIZE - CF_BUFFERMARGIN))
    {
        Log(LOG_LEVEL_ERR,
            "Expansion overflow constructing string. Increase CF_EXPANDSIZE macro. Tried to add '%s' to '%s'", str2,
              str1);
        return true;
    }

    return false;
}

/*********************************************************************/

bool ExpandScalar(const EvalContext *ctx, const char *ns, const char *scope, const char *string, char buffer[CF_EXPANDSIZE])
{
    Rval rval;
    int varstring = false;
    char currentitem[CF_EXPANDSIZE], temp[CF_BUFSIZE], name[CF_MAXVARSIZE];
    int increment, returnval = true;

    buffer[0] = '\0';

    if (string == 0 || strlen(string) == 0)
    {
        return false;
    }

    for (const char *sp = string; /* No exit */ ; sp++)     /* check for varitems */
    {
        char var[CF_BUFSIZE];

        var[0] = '\0';

        increment = 0;

        if (*sp == '\0')
        {
            break;
        }

        currentitem[0] = '\0';
        StringNotMatchingSetCapped(sp,CF_EXPANDSIZE,"$",currentitem);

        if (ExpandOverflow(buffer, currentitem))
        {
            FatalError(ctx, "Can't expand varstring");
        }

        strlcat(buffer, currentitem, CF_EXPANDSIZE);
        sp += strlen(currentitem);

        Log(LOG_LEVEL_DEBUG, "  Aggregate result '%s', scanning at '%s' (current delta '%s')", buffer, sp, currentitem);

        if (*sp == '\0')
        {
            break;
        }

        if (*sp == '$')
        {
            switch (*(sp + 1))
            {
            case '(':
                ExtractOuterCf3VarString(sp, var);
                varstring = ')';
                if (strlen(var) == 0)
                {
                    strlcat(buffer, "$", CF_EXPANDSIZE);
                    continue;
                }
                break;

            case '{':
                ExtractOuterCf3VarString(sp, var);
                varstring = '}';
                if (strlen(var) == 0)
                {
                    strlcat(buffer, "$", CF_EXPANDSIZE);
                    continue;
                }
                break;

            default:
                strlcat(buffer, "$", CF_EXPANDSIZE);
                continue;
            }
        }

        currentitem[0] = '\0';

        temp[0] = '\0';
        ExtractInnerCf3VarString(sp, temp);

        if (IsCf3VarString(temp))
        {
            Log(LOG_LEVEL_DEBUG, "Nested variables '%s'", temp);
            ExpandScalar(ctx, ns, scope, temp, currentitem);
        }
        else
        {
            Log(LOG_LEVEL_DEBUG, "Delta '%s'", temp);
            strncpy(currentitem, temp, CF_BUFSIZE - 1);
        }

        increment = strlen(var) - 1;

        if (!IsExpandable(currentitem))
        {
            DataType type = DATA_TYPE_NONE;
            bool variable_found = false;
            {
                VarRef *ref = VarRefParseFromNamespaceAndScope(currentitem, ns, scope, CF_NS, '.');
                variable_found = EvalContextVariableGet(ctx, ref, &rval, &type);
                VarRefDestroy(ref);
            }

            if (variable_found)
            {
                switch (type)
                {
                case DATA_TYPE_STRING:
                case DATA_TYPE_INT:
                case DATA_TYPE_REAL:

                    if (ExpandOverflow(buffer, (char *) rval.item))
                    {
                        FatalError(ctx, "Can't expand varstring");
                    }

                    strlcat(buffer, (char *) rval.item, CF_EXPANDSIZE);
                    break;

                case DATA_TYPE_STRING_LIST:
                case DATA_TYPE_INT_LIST:
                case DATA_TYPE_REAL_LIST:
                case DATA_TYPE_NONE:
                    if (type == DATA_TYPE_NONE)
                    {
                        Log(LOG_LEVEL_DEBUG,
                            "Can't expand inexistent variable '%s'", currentitem);
                    }
                    else
                    {
                        Log(LOG_LEVEL_DEBUG,
                            "Expecting scalar, can't expand list variable '%s'",
                            currentitem);
                    }

                    if (varstring == '}')
                    {
                        snprintf(name, CF_MAXVARSIZE, "${%s}", currentitem);
                    }
                    else
                    {
                        snprintf(name, CF_MAXVARSIZE, "$(%s)", currentitem);
                    }

                    strlcat(buffer, name, CF_EXPANDSIZE);
                    returnval = false;
                    break;

                default:
                    Log(LOG_LEVEL_DEBUG, "Returning Unknown Scalar ('%s' => '%s')", string, buffer);
                    return false;

                }
            }
            else
            {
                Log(LOG_LEVEL_DEBUG, "Currently non existent or list variable '%s'", currentitem);

                if (varstring == '}')
                {
                    snprintf(name, CF_MAXVARSIZE, "${%s}", currentitem);
                }
                else
                {
                    snprintf(name, CF_MAXVARSIZE, "$(%s)", currentitem);
                }

                strlcat(buffer, name, CF_EXPANDSIZE);
                returnval = false;
            }
        }

        sp += increment;
        currentitem[0] = '\0';
    }

    if (returnval)
    {
        Log(LOG_LEVEL_DEBUG, "Returning complete scalar expansion ('%s' => '%s')", string, buffer);

        /* Can we be sure this is complete? What about recursion */
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "Returning partial / best effort scalar expansion ('%s' => '%s')", string, buffer);
    }

    return returnval;
}

/*********************************************************************/


Rval EvaluateFinalRval(EvalContext *ctx, const char *ns, const char *scope, Rval rval, int forcelist, const Promise *pp)
{
    Rlist *rp;
    Rval returnval, newret;
    char naked[CF_MAXVARSIZE];
    FnCall *fp;

    if ((rval.type == RVAL_TYPE_SCALAR) && IsNakedVar(rval.item, '@'))        /* Treat lists specially here */
    {
        GetNaked(naked, rval.item);

        if (!IsExpandable(naked))
        {
            VarRef *ref = VarRefParseFromScope(naked, scope);

            if (!EvalContextVariableGet(ctx, ref, &returnval, NULL) || returnval.type != RVAL_TYPE_LIST)
            {
                returnval = ExpandPrivateRval(ctx, NULL, "this", rval);
            }
            else
            {
                returnval.item = ExpandList(ctx, ns, scope, returnval.item, true);
                returnval.type = RVAL_TYPE_LIST;
            }

            VarRefDestroy(ref);
        }
        else
        {
            returnval = ExpandPrivateRval(ctx, NULL, "this", rval);
        }
    }
    else
    {
        if (forcelist)          /* We are replacing scalar @(name) with list */
        {
            returnval = ExpandPrivateRval(ctx, ns, scope, rval);
        }
        else
        {
            if (FnCallIsBuiltIn(rval))
            {
                returnval = RvalCopy(rval);
            }
            else
            {
                returnval = ExpandPrivateRval(ctx, NULL, "this", rval);
            }
        }
    }

    switch (returnval.type)
    {
    case RVAL_TYPE_SCALAR:
        break;

    case RVAL_TYPE_LIST:
        for (rp = (Rlist *) returnval.item; rp != NULL; rp = rp->next)
        {
            if (rp->type == RVAL_TYPE_FNCALL)
            {
                fp = (FnCall *) rp->item;
                FnCallResult res = FnCallEvaluate(ctx, fp, pp);

                FnCallDestroy(fp);
                rp->item = res.rval.item;
                rp->type = res.rval.type;
            }
            else
            {
                if (EvalContextStackCurrentPromise(ctx))
                {
                    if (IsCf3VarString(rp->item))
                    {
                        newret = ExpandPrivateRval(ctx, NULL, "this", (Rval) {rp->item, rp->type});
                        free(rp->item);
                        rp->item = newret.item;
                    }
                }
            }

            /* returnval unchanged */
        }
        break;

    case RVAL_TYPE_FNCALL:

        // Also have to eval function now
        fp = (FnCall *) returnval.item;
        returnval = FnCallEvaluate(ctx, fp, pp).rval;
        FnCallDestroy(fp);
        break;

    default:
        returnval.item = NULL;
        returnval.type = RVAL_TYPE_NOPROMISEE;
        break;
    }

    return returnval;
}

/*********************************************************************/

static void CopyLocalizedIteratorsToBundleScope(EvalContext *ctx, const Bundle *bundle, const Rlist *listvars)
{
    for (const Rlist *rp = listvars; rp != NULL; rp = rp->next)
    {
        const char *mangled = rp->item;

        if (strchr(rp->item, CF_MAPPEDLIST))
        {
            VarRef *demangled_ref = VarRefDeMangle(mangled);

            Rval retval;
            if (EvalContextVariableGet(ctx, demangled_ref, &retval, NULL))
            {
                Rlist *list = RvalCopy((Rval) {retval.item, RVAL_TYPE_LIST}).item;
                RlistFlatten(ctx, &list);

                VarRef *mangled_ref = VarRefParseFromBundle(mangled, bundle);
                EvalContextVariablePut(ctx, mangled_ref, (Rval) { list, RVAL_TYPE_LIST }, DATA_TYPE_STRING_LIST);
                VarRefDestroy(mangled_ref);
            }

            VarRefDestroy(demangled_ref);
        }
    }
}

/*********************************************************************/

static void CopyLocalizedScalarsToBundleScope(EvalContext *ctx, const Bundle *bundle, const Rlist *scalars)
{
    for (const Rlist *rp = scalars; rp != NULL; rp = rp->next)
    {
        const char *mangled = rp->item;

        if (strchr(rp->item, CF_MAPPEDLIST))
        {
            VarRef *demangled_ref = VarRefDeMangle(mangled);

            Rval retval;
            if (EvalContextVariableGet(ctx, demangled_ref, &retval, NULL))
            {
                VarRef *mangled_ref = VarRefParseFromBundle(mangled, bundle);
                EvalContextVariablePut(ctx, mangled_ref, (Rval) { retval.item, RVAL_TYPE_SCALAR }, DATA_TYPE_STRING);
                VarRefDestroy(mangled_ref);
            }

            VarRefDestroy(demangled_ref);
        }
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

        ExpandPromise(ctx, pp, VerifyClassPromise, NULL);
    }
}

static void ResolveVariablesPromises(EvalContext *ctx, PromiseType *pt)
{
    assert(strcmp("vars", pt->name) == 0);

    for (size_t i = 0; i < SeqLength(pt->promises); i++)
    {
        Promise *pp = SeqAt(pt->promises, i);
        EvalContextStackPushPromiseFrame(ctx, pp, false);
        EvalContextStackPushPromiseIterationFrame(ctx, NULL);
        VerifyVarPromise(ctx, pp, false);
        EvalContextStackPopFrame(ctx);
        EvalContextStackPopFrame(ctx);
    }
}

void BundleResolve(EvalContext *ctx, Bundle *bundle)
{
    Log(LOG_LEVEL_VERBOSE, "Resolving variables in bundle '%s' '%s'", bundle->type, bundle->name);

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

    EvalContextStackPushBodyFrame(ctx, control_body, NULL);

    for (size_t i = 0; i < SeqLength(control_body->conlist); i++)
    {
        Constraint *cp = SeqAt(control_body->conlist, i);

        if (!IsDefinedClass(ctx, cp->classes, NULL))
        {
            continue;
        }

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_BUNDLESEQUENCE].lval) == 0)
        {
            returnval = ExpandPrivateRval(ctx, NULL, scope, cp->rval);
        }
        else
        {
            returnval = EvaluateFinalRval(ctx, NULL, scope, cp->rval, true, NULL);
        }

        VarRef *ref = VarRefParseFromScope(cp->lval, scope);
        EvalContextVariableRemove(ctx, ref);

        if (!EvalContextVariablePut(ctx, ref, returnval, ConstraintSyntaxGetDataType(body_syntax, cp->lval)))
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
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "fqhost", VFQNAME, DATA_TYPE_STRING);
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "domain", VDOMAIN, DATA_TYPE_STRING);
            EvalContextHeapAddHard(ctx, VDOMAIN);
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

        if (strcmp(cp->lval, CFG_CONTROLBODY[COMMON_CONTROL_GOALPATTERNS].lval) == 0)
        {
            /* Ignored */
            continue;
        }

        RvalDestroy(returnval);
    }

    EvalContextStackPopFrame(ctx);
}

void PolicyResolve(EvalContext *ctx, Policy *policy, GenericAgentConfig *config)
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

int IsNakedVar(const char *str, char vtype)
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

/*********************************************************************/

static void SetAnyMissingDefaults(EvalContext *ctx, Promise *pp)
/* Some defaults have to be set here, if they involve body-name
   constraints as names need to be expanded before CopyDeRefPromise */
{
    if (strcmp(pp->parent_promise_type->name, "packages") == 0)
    {
        if (PromiseGetConstraint(ctx, pp, "package_method") == NULL)
        {
            PromiseAppendConstraint(pp, "package_method", (Rval) {"generic", RVAL_TYPE_SCALAR}, "any", true);
        }
    }
}

void CommonEvalPromise(EvalContext *ctx, Promise *pp, ARG_UNUSED void *param)
{
    assert(param == NULL);

    ShowPromise(pp);
    PromiseRecheckAllConstraints(ctx, pp);
}
