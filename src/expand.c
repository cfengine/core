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

/*********************************************************************/
/*                                                                   */
/*  Variable expansion in cf3                                        */
/*                                                                   */
/*********************************************************************/

#include "expand.h"

#include "env_context.h"
#include "constraints.h"
#include "promises.h"
#include "vars.h"
#include "syntax.h"
#include "files_names.h"
#include "conversion.h"
#include "reporting.h"
#include "scope.h"
#include "matching.h"
#include "unix.h"
#include "attributes.h"
#include "cfstream.h"
#include "fncall.h"
#include "args.h"

static void MapIteratorsFromScalar(const char *scope, Rlist **los, Rlist **lol, char *string, int level, const Promise *pp);
static int Epimenides(const char *var, Rval rval, int level);
static void RewriteInnerVarStringAsLocalCopyName(char *string);
static int CompareRlist(Rlist *list1, Rlist *list2);
static int CompareRval(Rval rval1, Rval rval2);
static void SetAnyMissingDefaults(Promise *pp);
static void CopyLocalizedIteratorsToThisScope(const char *scope, const Rlist *listvars);
static void CheckRecursion(const ReportContext *report_context, Promise *pp);
static void ParseServices(const ReportContext *report_context, Promise *pp);
/*

Expanding variables is easy -- expanding lists automagically requires
some thought. Remember that

promiser <=> CF_SCALAR
promisee <=> CF_LIST

Expanding all bodies in the constraint list, we have

lval <=> CF_LIST|CF_SCALAR

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

void ExpandPromise(AgentType agent, const char *scopeid, Promise *pp, void *fnptr,
                   const ReportContext *report_context)
{
    Rlist *listvars = NULL, *scalarvars = NULL;
    Constraint *cp;
    Promise *pcopy;

    CfDebug("****************************************************\n");
    CfDebug("* ExpandPromises (scope = %s )\n", scopeid);
    CfDebug("****************************************************\n\n");

// Set a default for packages here...general defaults that need to come before

//fix me wth a general function SetMissingDefaults

    SetAnyMissingDefaults(pp);

    DeleteScope("match");       /* in case we expand something expired accidentially */

    THIS_BUNDLE = scopeid;

    pcopy = DeRefCopyPromise(scopeid, pp);

    MapIteratorsFromRval(scopeid, &scalarvars, &listvars, (Rval) {pcopy->promiser, CF_SCALAR}, pp);

    if (pcopy->promisee.item != NULL)
    {
        MapIteratorsFromRval(scopeid, &scalarvars, &listvars, pp->promisee, pp);
    }

    for (cp = pcopy->conlist; cp != NULL; cp = cp->next)
    {
        MapIteratorsFromRval(scopeid, &scalarvars, &listvars, cp->rval, pp);
    }

    CopyLocalizedIteratorsToThisScope(scopeid, listvars);

    PushThisScope();
    ExpandPromiseAndDo(agent, scopeid, pcopy, scalarvars, listvars, fnptr, report_context);
    PopThisScope();

    DeletePromise(pcopy);
    DeleteRlist(scalarvars);
    DeleteRlist(listvars);
}

/*********************************************************************/

Rval ExpandDanglers(const char *scopeid, Rval rval, const Promise *pp)
{
    Rval final;

    /* If there is still work left to do, expand and replace alloc */

    switch (rval.rtype)
    {
    case CF_SCALAR:

        if (IsCf3VarString(rval.item))
        {
            final = EvaluateFinalRval(scopeid, rval, false, pp);
        }
        else
        {
            final = CopyRvalItem(rval);
        }
        break;

    default:
        final = CopyRvalItem(rval);
        break;
    }

    return final;
}

/*********************************************************************/

void MapIteratorsFromRval(const char *scopeid, Rlist **scalarvars, Rlist **listvars, Rval rval,
                          const Promise *pp)
{
    Rlist *rp;
    FnCall *fp;

    if (rval.item == NULL)
    {
        return;
    }

    switch (rval.rtype)
    {
    case CF_SCALAR:
        MapIteratorsFromScalar(scopeid, scalarvars, listvars, (char *) rval.item, 0, pp);
        break;

    case CF_LIST:
        for (rp = (Rlist *) rval.item; rp != NULL; rp = rp->next)
        {
            MapIteratorsFromRval(scopeid, scalarvars, listvars, (Rval) {rp->item, rp->type}, pp);
        }
        break;

    case CF_FNCALL:
        fp = (FnCall *) rval.item;

        for (rp = (Rlist *) fp->args; rp != NULL; rp = rp->next)
        {
            CfDebug("Looking at arg for function-like object %s()\n", fp->name);
            MapIteratorsFromRval(scopeid, scalarvars, listvars, (Rval) {rp->item, rp->type}, pp);
        }
        break;

    default:
        CfDebug("Unknown Rval type for scope %s", scopeid);
        break;
    }
}

/*********************************************************************/

static void MapIteratorsFromScalar(const char *scopeid, Rlist **scal, Rlist **its, char *string, int level,
                                   const Promise *pp)
{
    char *sp;
    Rval rval;
    char v[CF_BUFSIZE], var[CF_EXPANDSIZE], exp[CF_EXPANDSIZE], temp[CF_BUFSIZE], finalname[CF_BUFSIZE];

    CfDebug("MapIteratorsFromScalar(\"%s\", %d)\n", string, level);

    if (string == NULL)
    {
        return;
    }

    for (sp = string; (*sp != '\0'); sp++)
    {
        v[0] = '\0';
        var[0] = '\0';
        exp[0] = '\0';

        if (*sp == '$')
        {
            if (ExtractInnerCf3VarString(sp, v))
            {
                char absscope[CF_MAXVARSIZE];
                int qualified;

                // If a list is non-local, i.e. $(bundle.var), map it to local $(bundle#var)

                if (IsQualifiedVariable(v))
                {
                    strncpy(temp, v, CF_BUFSIZE - 1);
                    absscope[0] = '\0';
                    sscanf(temp, "%[^.].%s", absscope, v);
                    ExpandPrivateScalar(absscope, v, var);
                    snprintf(finalname, CF_MAXVARSIZE, "%s%c%s", absscope, CF_MAPPEDLIST, var);
                    qualified = true;
                }
                else
                {
                    strncpy(absscope, scopeid, CF_MAXVARSIZE - 1);
                    ExpandPrivateScalar(absscope, v, var);
                    strncpy(finalname, var, CF_BUFSIZE - 1);
                    qualified = false;
                }

                // Interlude for knowledge map creation add dependency

                RegisterBundleDependence(absscope, pp);

                // var is the expanded name of the variable in its native context
                // finalname will be the mapped name in the local context "this."

                if (GetVariable(absscope, var, &rval) != cf_notype)
                {
                    if (rval.rtype == CF_LIST)
                    {
                        ExpandScalar(finalname, exp);

                        if (qualified)
                        {
                            RewriteInnerVarStringAsLocalCopyName(sp);
                        }

                        /* embedded iterators should be incremented fastest,
                           so order list -- and MUST return de-scoped name
                           else list expansion cannot map var to this.name */

                        if (level > 0)
                        {
                            IdempPrependRScalar(its, exp, CF_SCALAR);
                        }
                        else
                        {
                            IdempAppendRScalar(its, exp, CF_SCALAR);
                        }
                    }
                    else if (rval.rtype == CF_SCALAR)
                    {
                        CfDebug("Scalar variable $(%s) found\n", var);
                        IdempAppendRScalar(scal, var, CF_SCALAR);
                    }
                }
                else
                {
                    CfDebug("Checking for nested vars, e.g. $(array[$(index)])....\n");

                    if (IsExpandable(var))
                    {
                        MapIteratorsFromScalar(scopeid, scal, its, var, level + 1, pp);

                        // Need to rewrite list references to nested variables in this level

                        if (strchr(var, CF_MAPPEDLIST))
                        {
                            RewriteInnerVarStringAsLocalCopyName(sp);
                        }
                    }
                }

                sp += strlen(var) - 1;
            }
        }
    }
}

/*********************************************************************/

int ExpandScalar(const char *string, char buffer[CF_EXPANDSIZE])
{
    return ExpandPrivateScalar(CONTEXTID, string, buffer);
}

/*********************************************************************/

Rlist *ExpandList(const char *scopeid, const Rlist *list, int expandnaked)
{
    Rlist *rp, *start = NULL;
    Rval returnval;
    char naked[CF_MAXVARSIZE];

    for (rp = (Rlist *) list; rp != NULL; rp = rp->next)
    {
        if (!expandnaked && (rp->type == CF_SCALAR) && IsNakedVar(rp->item, '@'))
        {
            returnval.item = xstrdup(rp->item);
            returnval.rtype = CF_SCALAR;
        }
        else if ((rp->type == CF_SCALAR) && IsNakedVar(rp->item, '@'))
        {
            GetNaked(naked, rp->item);

            if (GetVariable(scopeid, naked, &returnval) != cf_notype)
            {
                returnval = ExpandPrivateRval(scopeid, returnval);
            }
            else
            {
                returnval = ExpandPrivateRval(scopeid, (Rval) {rp->item, rp->type});
            }
        }
        else
        {
            returnval = ExpandPrivateRval(scopeid, (Rval) {rp->item, rp->type});
        }

        AppendRlist(&start, returnval.item, returnval.rtype);
        DeleteRvalItem(returnval);
    }

    return start;
}

/*********************************************************************/

Rval ExpandPrivateRval(const char *scopeid, Rval rval)
{
    char buffer[CF_EXPANDSIZE];
    FnCall *fp, *fpe;
    Rval returnval;

    CfDebug("ExpandPrivateRval(scope=%s,type=%c)\n", scopeid, rval.rtype);

/* Allocates new memory for the copy */

    returnval.item = NULL;
    returnval.rtype = CF_NOPROMISEE;

    switch (rval.rtype)
    {
    case CF_SCALAR:

        ExpandPrivateScalar(scopeid, (char *) rval.item, buffer);
        returnval.item = xstrdup(buffer);
        returnval.rtype = CF_SCALAR;
        break;

    case CF_LIST:

        returnval.item = ExpandList(scopeid, rval.item, true);
        returnval.rtype = CF_LIST;
        break;

    case CF_FNCALL:

        /* Note expand function does not mean evaluate function, must preserve type */
        fp = (FnCall *) rval.item;
        fpe = ExpandFnCall(scopeid, fp, true);
        returnval.item = fpe;
        returnval.rtype = CF_FNCALL;
        break;
    }

    return returnval;
}

/*********************************************************************/

Rval ExpandBundleReference(const char *scopeid, Rval rval)
{
    CfDebug("ExpandBundleReference(scope=%s,type=%c)\n", scopeid, rval.rtype);

/* Allocates new memory for the copy */

    switch (rval.rtype)
    {
    case CF_SCALAR:
    {
        char buffer[CF_EXPANDSIZE];

        ExpandPrivateScalar(scopeid, (char *) rval.item, buffer);
        return (Rval) {xstrdup(buffer), CF_SCALAR};
    }

    case CF_FNCALL:
    {
        /* Note expand function does not mean evaluate function, must preserve type */
        FnCall *fp = (FnCall *) rval.item;

        return (Rval) {ExpandFnCall(scopeid, fp, false), CF_FNCALL};
    }

    default:
        return (Rval) {NULL, CF_NOPROMISEE};
    }
}

/*********************************************************************/

static bool ExpandOverflow(const char *str1, const char *str2)
{
    int len = strlen(str2);

    if ((strlen(str1) + len) > (CF_EXPANDSIZE - CF_BUFFERMARGIN))
    {
        CfOut(cf_error, "",
              "Expansion overflow constructing string. Increase CF_EXPANDSIZE macro. Tried to add %s to %s\n", str2,
              str1);
        return true;
    }

    return false;
}

/*********************************************************************/

int ExpandPrivateScalar(const char *scopeid, const char *string, char buffer[CF_EXPANDSIZE])
{
    const char *sp;
    Rval rval;
    int varstring = false;
    char currentitem[CF_EXPANDSIZE], temp[CF_BUFSIZE], name[CF_MAXVARSIZE];
    int increment, returnval = true;

    buffer[0] = '\0';

    if (string == 0 || strlen(string) == 0)
    {
        return false;
    }

    CfDebug("\nExpandPrivateScalar(%s,%s)\n", scopeid, string);

    for (sp = string; /* No exit */ ; sp++)     /* check for varitems */
    {
        char var[CF_BUFSIZE];

        var[0] = '\0';

        increment = 0;

        if (*sp == '\0')
        {
            break;
        }

        currentitem[0] = '\0';

        sscanf(sp, "%[^$]", currentitem);

        if (ExpandOverflow(buffer, currentitem))
        {
            FatalError("Can't expand varstring");
        }

        strlcat(buffer, currentitem, CF_EXPANDSIZE);
        sp += strlen(currentitem);

        CfDebug("  Aggregate result |%s|, scanning at \"%s\" (current delta %s)\n", buffer, sp, currentitem);

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
            CfDebug("  Nested variables - %s\n", temp);
            ExpandPrivateScalar(scopeid, temp, currentitem);
        }
        else
        {
            CfDebug("  Delta - %s\n", temp);
            strncpy(currentitem, temp, CF_BUFSIZE - 1);
        }

        increment = strlen(var) - 1;

        switch (GetVariable(scopeid, currentitem, &rval))
        {
        case cf_str:
        case cf_int:
        case cf_real:

            if (ExpandOverflow(buffer, (char *) rval.item))
            {
                FatalError("Can't expand varstring");
            }

            strlcat(buffer, (char *) rval.item, CF_EXPANDSIZE);
            break;

        case cf_slist:
        case cf_ilist:
        case cf_rlist:
        case cf_notype:
            CfDebug("  Currently non existent or list variable $(%s)\n", currentitem);

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
            CfDebug("Returning Unknown Scalar (%s => %s)\n\n", string, buffer);
            return false;

        }

        sp += increment;
        currentitem[0] = '\0';
    }

    if (returnval)
    {
        CfDebug("Returning complete scalar expansion (%s => %s)\n\n", string, buffer);

        /* Can we be sure this is complete? What about recursion */
    }
    else
    {
        CfDebug("Returning partial / best effort scalar expansion (%s => %s)\n\n", string, buffer);
    }

    return returnval;
}

/*********************************************************************/

void ExpandPromiseAndDo(AgentType agent, const char *scopeid, Promise *pp, Rlist *scalarvars, Rlist *listvars,
                        void (*fnptr) (), const ReportContext *report_context)
{
    Rlist *lol = NULL;
    Promise *pexp;
    const int cf_null_cutoff = 5;
    char *handle = GetConstraintValue("handle", pp, CF_SCALAR), v[CF_MAXVARSIZE];
    int cutoff = 0;

    lol = NewIterationContext(scopeid, listvars);

    if (lol && EndOfIteration(lol))
    {
        DeleteIterationContext(lol);
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
        return;
    }

    do
    {
        char number[CF_SMALLBUF];

        /* Set scope "this" first to ensure list expansion ! */
        SetScope("this");
        DeRefListsInHashtable("this", listvars, lol);

        /* Allow $(this.handle) etc variables */

        if (handle)
        {
            char tmp[CF_EXPANDSIZE];
            // This ordering is necessary to get automated canonification
            ExpandScalar(handle,tmp);
            CanonifyNameInPlace(tmp);
            NewScalar("this", "handle", tmp, cf_str);
        }
        else
        {
            NewScalar("this", "handle", PromiseID(pp), cf_str);
        }

        if (pp->audit && pp->audit->filename)
        {
            NewScalar("this", "promise_filename", pp->audit->filename, cf_str);
            snprintf(number, CF_SMALLBUF, "%zu", pp->offset.line);
            NewScalar("this", "promise_linenumber", number, cf_str);
        }

        snprintf(v, CF_MAXVARSIZE, "%d", (int) getuid());
        NewScalar("this", "promiser_uid", v, cf_int);
        snprintf(v, CF_MAXVARSIZE, "%d", (int) getgid());
        NewScalar("this", "promiser_gid", v, cf_int);

        NewScalar("this", "bundle", pp->bundle, cf_str);
        NewScalar("this", "namespace", pp->namespace, cf_str);

        /* Must expand $(this.promiser) here for arg dereferencing in things
           like edit_line and methods, but we might have to
           adjust again later if the value changes  -- need to qualify this
           so we don't expand too early for some other promsies */

        if (pp->has_subbundles)
        {
            NewScalar("this", "promiser", pp->promiser, cf_str);
        }

        /* End special variables */

        pexp = ExpandDeRefPromise("this", pp);

        switch (agent)
        {
        case AGENT_TYPE_COMMON:
            ShowPromise(report_context, REPORT_OUTPUT_TYPE_TEXT, pexp, 6);
            ShowPromise(report_context, REPORT_OUTPUT_TYPE_HTML, pexp, 6);
            CheckRecursion(report_context, pexp);
            ReCheckAllConstraints(pexp);
            break;

        default:

            if (fnptr != NULL)
            {
                (*fnptr) (pexp);
            }
            break;
        }

        if (strcmp(pp->agentsubtype, "vars") == 0)
        {
            ConvergeVarHashPromise(pp->bundle, pexp, true);
        }

        if (strcmp(pp->agentsubtype, "meta") == 0)
           {
           char namespace[CF_BUFSIZE];
           snprintf(namespace,CF_BUFSIZE,"%s_meta",pp->bundle);
           ConvergeVarHashPromise(namespace, pp, true);
           }
        
        DeletePromise(pexp);

        /* End thread monitor */
    }
    while (IncrementIterationContext(lol));

    DeleteIterationContext(lol);
}

/*********************************************************************/

Rval EvaluateFinalRval(const char *scopeid, Rval rval, int forcelist, const Promise *pp)
{
    Rlist *rp;
    Rval returnval, newret;
    char naked[CF_MAXVARSIZE];
    FnCall *fp;

    CfDebug("EvaluateFinalRval -- type %c\n", rval.rtype);

    if ((rval.rtype == CF_SCALAR) && IsNakedVar(rval.item, '@'))        /* Treat lists specially here */
    {
        GetNaked(naked, rval.item);

        if (GetVariable(scopeid, naked, &returnval) == cf_notype || returnval.rtype != CF_LIST)
        {
            returnval = ExpandPrivateRval("this", rval);
        }
        else
        {
            returnval.item = ExpandList(scopeid, returnval.item, true);
            returnval.rtype = CF_LIST;
        }
    }
    else
    {
        if (forcelist)          /* We are replacing scalar @(name) with list */
        {
            returnval = ExpandPrivateRval(scopeid, rval);
        }
        else
        {
            if (IsBuiltinFnCall(rval))
            {
                returnval = CopyRvalItem(rval);
            }
            else
            {
                returnval = ExpandPrivateRval("this", rval);
            }
        }
    }

    switch (returnval.rtype)
    {
    case CF_SCALAR:
        break;

    case CF_LIST:
        for (rp = (Rlist *) returnval.item; rp != NULL; rp = rp->next)
        {
            if (rp->type == CF_FNCALL)
            {
                fp = (FnCall *) rp->item;
                FnCallResult res = EvaluateFunctionCall(fp, pp);

                DeleteFnCall(fp);
                rp->item = res.rval.item;
                rp->type = res.rval.rtype;
                CfDebug("Replacing function call with new type (%c)\n", rp->type);
            }
            else
            {
                Scope *ptr = GetScope("this");

                if (ptr != NULL)
                {
                    if (IsCf3VarString(rp->item))
                    {
                        newret = ExpandPrivateRval("this", (Rval) {rp->item, rp->type});
                        free(rp->item);
                        rp->item = newret.item;
                    }
                }
            }

            /* returnval unchanged */
        }
        break;

    case CF_FNCALL:

        // Also have to eval function now
        fp = (FnCall *) returnval.item;
        returnval = EvaluateFunctionCall(fp, pp).rval;
        DeleteFnCall(fp);
        break;

    default:
        returnval.item = NULL;
        returnval.rtype = CF_NOPROMISEE;
        break;
    }

    return returnval;
}

/*********************************************************************/

static void RewriteInnerVarStringAsLocalCopyName(char *string)
{
    char *sp;

    for (sp = string; *sp != '\0'; sp++)
    {
        if (*sp == '.')
        {
            *sp = CF_MAPPEDLIST;
            return;
        }
    }
}

/*********************************************************************/

static void CopyLocalizedIteratorsToThisScope(const char *scope, const Rlist *listvars)
{
    Rval retval;
    char format[CF_SMALLBUF];

    for (const Rlist *rp = listvars; rp != NULL; rp = rp->next)
    {
        // Add re-mapped variables to context "this", marked with scope . -> #

        if (strchr(rp->item, '#'))
        {
            char orgscope[CF_MAXVARSIZE], orgname[CF_MAXVARSIZE];

            snprintf(format, CF_SMALLBUF, "%%[^%c]%c%%s", CF_MAPPEDLIST, CF_MAPPEDLIST);

            sscanf(rp->item, format, orgscope, orgname);

            GetVariable(orgscope, orgname, &retval);

            NewList(scope, rp->item, CopyRvalItem((Rval) {retval.item, CF_LIST}).item, cf_slist);
        }
    }
}

/*********************************************************************/
/* Tools                                                             */
/*********************************************************************/

int IsExpandable(const char *str)
{
    const char *sp;
    char left = 'x', right = 'x';
    int dollar = false;
    int bracks = 0, vars = 0;

    CfDebug("IsExpandable(%s) - syntax verify\n", str);

    for (sp = str; *sp != '\0'; sp++)   /* check for varitems */
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
        CfDebug("If this is an expandable variable string then it contained syntax errors");
        return false;
    }

    CfDebug("Found %d variables in (%s)\n", vars, str);
    return vars;
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

    CfDebug("IsNakedVar(%s,%c)!!\n", str, vtype);
    return true;
}

/*********************************************************************/

void GetNaked(char *s2, const char *s1)
/* copy @(listname) -> listname */
{
    if (strlen(s1) < 4)
    {
        CfOut(cf_error, "", "Naked variable expected, but \"%s\" is malformed", s1);
        strncpy(s2, s1, CF_MAXVARSIZE - 1);
        return;
    }

    memset(s2, 0, CF_MAXVARSIZE);
    strncpy(s2, s1 + 2, strlen(s1) - 3);
}

/*********************************************************************/

static void SetAnyMissingDefaults(Promise *pp)
/* Some defaults have to be set here, if they involve body-name
   constraints as names need to be expanded before CopyDeRefPromise */
{
    if (strcmp(pp->agentsubtype, "packages") == 0)
    {
        if (GetConstraint(pp, "package_method") == NULL)
        {
            ConstraintAppendToPromise(pp, "package_method", (Rval) {"generic", CF_SCALAR}, "any", true);
        }
    }
}

/*********************************************************************/
/* General                                                           */
/*********************************************************************/

void ConvergeVarHashPromise(char *scope, const Promise *pp, int allow_redefine)
{
    Constraint *cp, *cp_save = NULL;
    Attributes a = { {0} };
    int i = 0, ok_redefine = false, drop_undefined = false;
    Rlist *rp;
    Rval retval;
    Rval rval = { NULL, 'x' };  /* FIXME: why this needs to be initialized? */

    if (pp->done)
    {
        return;
    }

    if (IsExcluded(pp->classes, pp->namespace))
    {
        return;
    }

    for (cp = pp->conlist; cp != NULL; cp = cp->next)
    {
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

            switch (cp->rval.rtype)
            {
            case CF_SCALAR:

                if (IsExcluded(cp->rval.item, pp->namespace))
                {
                    return;
                }

                break;

            case CF_FNCALL:
            {
                bool excluded = false;

                /* eval it: e.g. ifvarclass => not("a_class") */

                res = EvaluateFunctionCall(cp->rval.item, NULL).rval;

                /* Don't continue unless function was evaluated properly */
                if (res.rtype != CF_SCALAR)
                {
                    DeleteRvalItem(res);
                    return;
                }

                excluded = IsExcluded(res.item, pp->namespace);

                DeleteRvalItem(res);

                if (excluded)
                {
                    return;
                }
            }
                break;

            default:
                CfOut(cf_error, "", "!! Invalid ifvarclass type '%c': should be string or function", cp->rval.rtype);
                continue;
            }

            continue;
        }

        if (strcmp(cp->lval, "policy") == 0)
        {
            if (strcmp(cp->rval.item, "ifdefined") == 0)
            {
                drop_undefined = true;
                ok_redefine = false;
            }
            else if (strcmp(cp->rval.item, "constant") == 0)
            {
                ok_redefine = false;
            }
            else
            {
                ok_redefine = true;
            }
        }
        else if (IsDataType(cp->lval))
        {
            i++;
            rval.item = cp->rval.item;
            cp_save = cp;
        }
    }

    cp = cp_save;

    if (cp == NULL)
    {
        CfOut(cf_inform, "", "Warning: Variable body for \"%s\" seems incomplete", pp->promiser);
        PromiseRef(cf_inform, pp);
        return;
    }

    if (i > 2)
    {
        CfOut(cf_error, "", "Variable \"%s\" breaks its own promise with multiple values (code %d)", pp->promiser, i);
        PromiseRef(cf_error, pp);
        return;
    }

//More consideration needs to be given to using these
//a.transaction = GetTransactionConstraints(pp);
    a.classes = GetClassDefinitionConstraints(pp);

    enum cfdatatype existing_var = GetVariable(scope, pp->promiser, &retval);

    char qualified_scope[CF_MAXVARSIZE];

    if (strcmp(pp->namespace, "default") == 0)
       {
       strcpy(qualified_scope, scope);
       }
    else
       {
       if (strchr(scope, ':') == NULL)
          {
          snprintf(qualified_scope, CF_MAXVARSIZE, "%s:%s", pp->namespace, scope);
          }
       else
          {
          strcpy(qualified_scope, scope);
          }
       }

    if (rval.item != NULL)
    {
        FnCall *fp = (FnCall *) rval.item;

        if (cp->rval.rtype == CF_FNCALL)
        {
            if (existing_var != cf_notype)
               {
               // Already did this
               return;
               }

            FnCallResult res = EvaluateFunctionCall(fp, pp);

            if (res.status == FNCALL_FAILURE)
            {
                /* We do not assign variables to failed fn calls */
                DeleteRvalItem(res.rval);
                return;
            }
            else
            {
                rval = res.rval;
            }
        }
        else
        {
            char conv[CF_MAXVARSIZE];

            if (strcmp(cp->lval, "int") == 0)
            {
                snprintf(conv, CF_MAXVARSIZE, "%ld", Str2Int(cp->rval.item));
                rval = CopyRvalItem((Rval) {conv, cp->rval.rtype});
            }
            else if (strcmp(cp->lval, "real") == 0)
            {
                snprintf(conv, CF_MAXVARSIZE, "%lf", Str2Double(cp->rval.item));
                rval = CopyRvalItem((Rval) {conv, cp->rval.rtype});
            }
            else
            {
                rval = CopyRvalItem(cp->rval);
            }
        }

        if (Epimenides(pp->promiser, rval, 0))
        {
            CfOut(cf_error, "", "Variable \"%s\" contains itself indirectly - an unkeepable promise", pp->promiser);
            exit(1);
        }
        else
        {
            /* See if the variable needs recursively expanding again */

            Rval returnval = EvaluateFinalRval(qualified_scope, rval, true, pp);

            DeleteRvalItem(rval);

            // freed before function exit
            rval = returnval;
        }

        if (existing_var != cf_notype)
        {
            if (ok_redefine)    /* only on second iteration, else we ignore broken promises */
            {
                DeleteVariable(qualified_scope, pp->promiser);
            }
            else if ((THIS_AGENT_TYPE == AGENT_TYPE_COMMON) && (CompareRval(retval, rval) == false))
            {
                switch (rval.rtype)
                {
                    char valbuf[CF_BUFSIZE];

                case CF_SCALAR:
                    CfOut(cf_verbose, "", " !! Redefinition of a constant scalar \"%s\" (was %s now %s)",
                          pp->promiser, ScalarRvalValue(retval), ScalarRvalValue(rval));
                    PromiseRef(cf_verbose, pp);
                    break;
                case CF_LIST:
                    CfOut(cf_verbose, "", " !! Redefinition of a constant list \"%s\".", pp->promiser);
                    PrintRlist(valbuf, CF_BUFSIZE, retval.item);
                    CfOut(cf_verbose, "", "Old value: %s", valbuf);
                    PrintRlist(valbuf, CF_BUFSIZE, rval.item);
                    CfOut(cf_verbose, "", " New value: %s", valbuf);
                    PromiseRef(cf_verbose, pp);
                    break;
                }
            }
        }

        if (IsCf3VarString(pp->promiser))
        {
            // Unexpanded variables, we don't do anything with
            DeleteRvalItem(rval);
            return;
        }

        if (!FullTextMatch("[a-zA-Z0-9_\200-\377.]+(\\[.+\\])*", pp->promiser))
        {
            CfOut(cf_error, "", " !! Variable identifier contains illegal characters");
            PromiseRef(cf_error, pp);
            DeleteRvalItem(rval);
            return;
        }

        if (drop_undefined && rval.rtype == CF_LIST)
        {
            for (rp = rval.item; rp != NULL; rp = rp->next)
            {
                if (IsNakedVar(rp->item, '@'))
                {
                    free(rp->item);
                    rp->item = xstrdup(CF_NULL_VALUE);
                }
            }
        }

        if (!AddVariableHash(qualified_scope, pp->promiser, rval, Typename2Datatype(cp->lval),
                             cp->audit->filename, cp->offset.line))
        {
            CfOut(cf_verbose, "", "Unable to converge %s.%s value (possibly empty or infinite regression)\n", qualified_scope, pp->promiser);
            PromiseRef(cf_verbose, pp);
            cfPS(cf_noreport, CF_FAIL, "", pp, a, " !! Couldn't add variable %s", pp->promiser);
        }
        else
        {
            cfPS(cf_noreport, CF_CHG, "", pp, a, " -> Added variable %s", pp->promiser);
        }
    }
    else
    {
        CfOut(cf_error, "", " !! Variable %s has no promised value\n", pp->promiser);
        CfOut(cf_error, "", " !! Rule from %s at/before line %zu\n", cp->audit->filename, cp->offset.line);
        cfPS(cf_noreport, CF_FAIL, "", pp, a, " !! Couldn't add variable %s", pp->promiser);
    }

    DeleteRvalItem(rval);
}

/*********************************************************************/
/* Levels                                                            */
/*********************************************************************/

static int Epimenides(const char *var, Rval rval, int level)
{
    Rlist *rp, *list;
    char exp[CF_EXPANDSIZE];

    switch (rval.rtype)
    {
    case CF_SCALAR:

        if (StringContainsVar(rval.item, var))
        {
            CfOut(cf_error, "", "Scalar variable \"%s\" contains itself (non-convergent): %s", var, (char *) rval.item);
            return true;
        }

        if (IsCf3VarString(rval.item))
        {
            ExpandPrivateScalar(CONTEXTID, rval.item, exp);
            CfDebug("bling %d-%s: (look for %s) in \"%s\" => %s \n", level, CONTEXTID, var, (const char *) rval.item,
                    exp);

            if (level > 3)
            {
                return false;
            }

            if (Epimenides(var, (Rval) {exp, CF_SCALAR}, level + 1))
            {
                return true;
            }
        }

        break;

    case CF_LIST:
        list = (Rlist *) rval.item;

        for (rp = list; rp != NULL; rp = rp->next)
        {
            if (Epimenides(var, (Rval) {rp->item, rp->type}, level))
            {
                return true;
            }
        }
        break;

    default:
        return false;
    }

    return false;
}

/*******************************************************************/

static int CompareRval(Rval rval1, Rval rval2)
{
    if (rval1.rtype != rval2.rtype)
    {
        return -1;
    }

    switch (rval1.rtype)
    {
    case CF_SCALAR:

        if (IsCf3VarString((char *) rval1.item) || IsCf3VarString((char *) rval2.item))
        {
            return -1;          // inconclusive
        }

        if (strcmp(rval1.item, rval2.item) != 0)
        {
            return false;
        }

        break;

    case CF_LIST:
        return CompareRlist(rval1.item, rval2.item);

    case CF_FNCALL:
        return -1;
    }

    return true;
}

/*******************************************************************/

// FIX: this function is a mixture of Equal/Compare (boolean/diff).
// somebody is bound to misuse this at some point
static int CompareRlist(Rlist *list1, Rlist *list2)
{
    Rlist *rp1, *rp2;

    for (rp1 = list1, rp2 = list2; rp1 != NULL && rp2 != NULL; rp1 = rp1->next, rp2 = rp2->next)
    {
        if (rp1->item && rp2->item)
        {
            Rlist *rc1, *rc2;

            if (rp1->type == CF_FNCALL || rp2->type == CF_FNCALL)
            {
                return -1;      // inconclusive
            }

            rc1 = rp1;
            rc2 = rp2;

            // Check for list nesting with { fncall(), "x" ... }

            if (rp1->type == CF_LIST)
            {
                rc1 = rp1->item;
            }

            if (rp2->type == CF_LIST)
            {
                rc2 = rp2->item;
            }

            if (IsCf3VarString(rc1->item) || IsCf3VarString(rp2->item))
            {
                return -1;      // inconclusive
            }

            if (strcmp(rc1->item, rc2->item) != 0)
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

/*******************************************************************/

static void CheckRecursion(const ReportContext *report_context, Promise *pp)

{
    Constraint *cp;
    char *type;
    char *scope;
    Bundle *bp;
    FnCall *fp;
    SubType *sbp;
    Promise *ppsub;

    // Check for recursion of bundles so that knowledge map will reflect these cases

    if (strcmp("services", pp->agentsubtype) == 0)
       {
       ParseServices(report_context, pp);
       }

    for (cp = pp->conlist; cp != NULL; cp = cp->next)
    {        
        if (strcmp("usebundle", cp->lval) == 0)
        {
            type = "agent";
        }
        else  if (strcmp("edit_line", cp->lval) == 0 || strcmp("edit_xml", cp->lval) == 0)
        {
            type = cp->lval;
        }
        else
        {
            continue;
        }

        switch (cp->rval.rtype)
        {
           case CF_SCALAR:
               scope = (char *)cp->rval.item;
               break;

           case CF_FNCALL:
               fp = (FnCall *)cp->rval.item;
               scope = fp->name;
               break;

           default:
               continue;
        }

       if ((bp = GetBundle(PolicyFromPromise(pp), scope, type)))
       {
           for (sbp = bp->subtypes; sbp != NULL; sbp = sbp->next)
           {
               for (ppsub = sbp->promiselist; ppsub != NULL; ppsub = ppsub->next)
               {
                   ExpandPromise(AGENT_TYPE_COMMON, scope, ppsub, NULL, report_context);
               }
           }
       }
    }
}

/*****************************************************************************/

static void ParseServices(const ReportContext *report_context, Promise *pp)
{
    FnCall *default_bundle = NULL;
    Rlist *args = NULL;
    Attributes a = { {0} };

    a = GetServicesAttributes(pp);

    // Need to set up the default service pack to eliminate syntax, analogous to verify_services.c

    if (GetConstraintValue("service_bundle", pp, CF_SCALAR) == NULL)
    {
        switch (a.service.service_policy)
        {
        case cfsrv_start:
            AppendRlist(&args, pp->promiser, CF_SCALAR);
            AppendRlist(&args, "start", CF_SCALAR);
            break;

        case cfsrv_restart:
            AppendRlist(&args, pp->promiser, CF_SCALAR);
            AppendRlist(&args, "restart", CF_SCALAR);
            break;

        case cfsrv_reload:
            AppendRlist(&args, pp->promiser, CF_SCALAR);
            AppendRlist(&args, "restart", CF_SCALAR);
            break;

        case cfsrv_stop:
        case cfsrv_disable:
        default:
            AppendRlist(&args, pp->promiser, CF_SCALAR);
            AppendRlist(&args, "stop", CF_SCALAR);
            break;

        }

        default_bundle = NewFnCall("standard_services", args);

        ConstraintAppendToPromise(pp, "service_bundle", (Rval) {default_bundle, CF_FNCALL}, "any", false);
        a.havebundle = true;
    }

// Set $(this.service_policy) for flexible bundle adaptation

    switch (a.service.service_policy)
    {
    case cfsrv_start:
        NewScalar("this", "service_policy", "start", cf_str);
        break;

    case cfsrv_restart:
        NewScalar("this", "service_policy", "restart", cf_str);
        break;

    case cfsrv_reload:
        NewScalar("this", "service_policy", "reload", cf_str);
        break;
        
    case cfsrv_stop:
    case cfsrv_disable:
    default:
        NewScalar("this", "service_policy", "stop", cf_str);
        break;
    }

    if (default_bundle && GetBundle(PolicyFromPromise(pp), default_bundle->name, "agent") == NULL)
    {
        return;
    }

    SubType *sbp;
    Promise *ppsub;
    Bundle *bp;

    if ((bp = GetBundle(PolicyFromPromise(pp), default_bundle->name, "agent")))
       {
       MapBodyArgs(bp->name, args, bp->args);
           
       for (sbp = bp->subtypes; sbp != NULL; sbp = sbp->next)
          {
          for (ppsub = sbp->promiselist; ppsub != NULL; ppsub = ppsub->next)
             {
             ExpandPromise(AGENT_TYPE_COMMON, bp->name, ppsub, NULL, report_context);
             }
          }
       }
}
