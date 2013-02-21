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

#include "vars.h"

#include "conversion.h"
#include "reporting.h"
#include "expand.h"
#include "scope.h"
#include "matching.h"
#include "hashes.h"
#include "unix.h"
#include "cfstream.h"
#include "logging.h"
#include "misc_lib.h"
#include "rlist.h"
#include "policy.h"

static int IsCf3Scalar(char *str);
static int CompareVariableValue(Rval rval, CfAssoc *ap);

void LoadSystemConstants()
{
    NewScalar("const", "dollar", "$", DATA_TYPE_STRING);
    NewScalar("const", "n", "\n", DATA_TYPE_STRING);
    NewScalar("const", "r", "\r", DATA_TYPE_STRING);
    NewScalar("const", "t", "\t", DATA_TYPE_STRING);
    NewScalar("const", "endl", "\n", DATA_TYPE_STRING);
/* NewScalar("const","0","\0",cf_str);  - this cannot work */

}

/*******************************************************************/
/* Variables                                                       */
/*******************************************************************/

void NewScalar(const char *scope, const char *lval, const char *rval, DataType dt)
{
    Rval rvald;
    Scope *ptr;

    CfDebug("NewScalar(%s,%s,%s)\n", scope, lval, rval);

    ptr = GetScope(scope);

    if (ptr == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "!! Attempt to add variable \"%s\" to non-existant scope \"%s\" - ignored", lval, scope);
        return;
    }

// Newscalar allocates memory through NewAssoc

    if (GetVariable(scope, lval, &rvald) != DATA_TYPE_NONE)
    {
        DeleteScalar(scope, lval);
    }

/*
 * We know AddVariableHash does not change passed Rval structure or its
 * contents, but we have no easy way to express it in C type system, hence cast.
 */
    AddVariableHash(scope, lval, (Rval) {(char *) rval, RVAL_TYPE_SCALAR }, dt, NULL, 0);
}

/*******************************************************************/

void DeleteScalar(const char *scope_name, const char *lval)
{
    Scope *scope = GetScope(scope_name);

    if (scope == NULL)
    {
        return;
    }

    if (HashDeleteElement(scope->hashtable, lval) == false)
    {
        CfDebug("Attempt to delete non-existent variable %s in scope %s\n", lval, scope_name);
    }
}

/*******************************************************************/

void NewList(const char *scope, const char *lval, void *rval, DataType dt)
{
    Rval rvald;

    if (GetVariable(scope, lval, &rvald) != DATA_TYPE_NONE)
    {
        DeleteVariable(scope, lval);
    }

    AddVariableHash(scope, lval, (Rval) {rval, RVAL_TYPE_LIST }, dt, NULL, 0);
}

/*******************************************************************/

DataType GetVariable(const char *scope, const char *lval, Rval *returnv)
{
    Scope *ptr = NULL;
    char scopeid[CF_MAXVARSIZE], vlval[CF_MAXVARSIZE], sval[CF_MAXVARSIZE];
    char expbuf[CF_EXPANDSIZE];
    CfAssoc *assoc;

    CfDebug("GetVariable(%s,%s) type=(to be determined)\n", scope, lval);

    if (lval == NULL)
    {
        *returnv = (Rval) {NULL, RVAL_TYPE_SCALAR };
        return DATA_TYPE_NONE;
    }

    if (!IsExpandable(lval))
    {
        strncpy(sval, lval, CF_MAXVARSIZE - 1);
    }
    else
    {
        if (ExpandScalar(lval, expbuf))
        {
            strncpy(sval, expbuf, CF_MAXVARSIZE - 1);
        }
        else
        {
            /* C type system does not allow us to express the fact that returned
               value may contain immutable string. */
            *returnv = (Rval) {(char *) lval, RVAL_TYPE_SCALAR };
            CfDebug("Couldn't expand array-like variable (%s) due to undefined dependencies\n", lval);
            return DATA_TYPE_NONE;
        }
    }

    if (IsQualifiedVariable(sval))
    {
        scopeid[0] = '\0';
        sscanf(sval, "%[^.].%s", scopeid, vlval);
        CfDebug("Variable identifier \"%s\" is prefixed with scope id \"%s\"\n", vlval, scopeid);
        ptr = GetScope(scopeid);
    }
    else
    {
        strlcpy(vlval, sval, sizeof(vlval));
        strlcpy(scopeid, scope, sizeof(scopeid));
    }

    if (ptr == NULL)
    {
        /* Assume current scope */
        strcpy(vlval, lval);
        ptr = GetScope(scopeid);
    }

    if (ptr == NULL)
    {
        CfDebug("Scope \"%s\" for variable \"%s\" does not seem to exist\n", scopeid, vlval);
        /* C type system does not allow us to express the fact that returned
           value may contain immutable string. */
        *returnv = (Rval) {(char *) lval, RVAL_TYPE_SCALAR };
        return DATA_TYPE_NONE;
    }

    CfDebug("GetVariable(%s,%s): using scope '%s' for variable '%s'\n", scopeid, vlval, ptr->scope, vlval);

    assoc = HashLookupElement(ptr->hashtable, vlval);

    if (assoc == NULL)
    {
        CfDebug("No such variable found %s.%s\n\n", scopeid, lval);
        /* C type system does not allow us to express the fact that returned
           value may contain immutable string. */


        *returnv = (Rval) {(char *) lval, RVAL_TYPE_SCALAR };
        return DATA_TYPE_NONE;

    }

    CfDebug("return final variable type=%s, value={\n", CF_DATATYPES[assoc->dtype]);

    if (DEBUG)
    {
        RvalShow(stdout, assoc->rval);
    }
    CfDebug("}\n");

    *returnv = assoc->rval;
    return assoc->dtype;
}

/*******************************************************************/

void DeleteVariable(const char *scope, const char *id)
{
    Scope *ptr = GetScope(scope);

    if (ptr == NULL)
    {
        return;
    }

    if (HashDeleteElement(ptr->hashtable, id) == false)
    {
        CfDebug("No variable matched %s\n", id);
    }
}

/*******************************************************************/

static int CompareVariableValue(Rval rval, CfAssoc *ap)
{
    const Rlist *list, *rp;

    if (ap == NULL || rval.item == NULL)
    {
        return 1;
    }

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        return strcmp(ap->rval.item, rval.item);

    case RVAL_TYPE_LIST:
        list = (const Rlist *) rval.item;

        for (rp = list; rp != NULL; rp = rp->next)
        {
            if (!CompareVariableValue((Rval) {rp->item, rp->type}, ap))
            {
                return -1;
            }
        }

        return 0;

    default:
        return 0;
    }

    return strcmp(ap->rval.item, rval.item);
}

/*******************************************************************/

int UnresolvedVariables(CfAssoc *ap, char rtype)
{
    Rlist *list, *rp;

    if (ap == NULL)
    {
        return false;
    }

    switch (rtype)
    {
    case RVAL_TYPE_SCALAR:
        return IsCf3VarString(ap->rval.item);

    case RVAL_TYPE_LIST:
        list = (Rlist *) ap->rval.item;

        for (rp = list; rp != NULL; rp = rp->next)
        {
            if (IsCf3VarString(rp->item))
            {
                return true;
            }
        }

        return false;

    default:
        return false;
    }
}

/*******************************************************************/

int UnresolvedArgs(Rlist *args)
{
    Rlist *rp;

    for (rp = args; rp != NULL; rp = rp->next)
    {
        if (rp->type != RVAL_TYPE_SCALAR)
        {
            return true;
        }

        if (IsCf3Scalar(rp->item))
        {
            if (strstr(rp->item, "$(this)") || strstr(rp->item, "${this}"))
            {
                // We should allow this in function args for substitution in maplist() etc
            }
            else
            {
                return true;
            }
        }
    }

    return false;
}

/******************************************************************/

bool StringContainsVar(const char *s, const char *v)
{
    int vlen = strlen(v);

    if (s == NULL)
    {
        return false;
    }

/* Look for ${v}, $(v), @{v}, $(v) */

    for (;;)
    {
        /* Look for next $ or @ */
        s = strpbrk(s, "$@");
        if (s == NULL)
        {
            return false;
        }
        /* If next symbol */
        if (*++s == '\0')
        {
            return false;
        }
        /* is { or ( */
        if (*s != '(' && *s != '{')
        {
            continue;
        }
        /* Then match the variable starting from next symbol */
        if (strncmp(s + 1, v, vlen) != 0)
        {
            continue;
        }
        /* And if it matched, match the closing bracket */
        if ((s[0] == '(' && s[vlen + 1] == ')') || (s[0] == '{' && s[vlen + 1] == '}'))
        {
            return true;
        }
    }
}

/*********************************************************************/

bool IsCf3VarString(const char *str)
{
    char left = 'x', right = 'x';
    int dollar = false;
    int bracks = 0, vars = 0;

    CfDebug("IsCf3VarString(%s) - syntax verify\n", str);

    if (str == NULL)
    {
        return false;
    }

    for (const char *sp = str; *sp != '\0'; sp++)   /* check for varitems */
    {
        switch (*sp)
        {
        case '$':
        case '@':
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

        /* Some chars cannot be in variable ids, e.g.
           $(/bin/cat file) is legal in bash */

        if (bracks > 0)
        {
            switch (*sp)
            {
            case '/':
                return false;
            }
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

    if (dollar && (bracks != 0))
    {
        char output[CF_BUFSIZE];

        snprintf(output, CF_BUFSIZE, "Broken variable syntax or bracket mismatch in string (%s)", str);
        yyerror(output);
        return false;
    }

    CfDebug("Found %d variables in (%s)\n", vars, str);
    return vars;
}

/*********************************************************************/

static int IsCf3Scalar(char *str)
{
    char *sp;
    char left = 'x', right = 'x';
    int dollar = false;
    int bracks = 0, vars = 0;

    CfDebug("IsCf3Scalar(%s) - syntax verify\n", str);

    if (str == NULL)
    {
        return false;
    }

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

        /* Some chars cannot be in variable ids, e.g.
           $(/bin/cat file) is legal in bash */

        if (bracks > 0)
        {
            switch (*sp)
            {
            case '/':
                return false;
            }
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

    if (dollar && (bracks != 0))
    {
        char output[CF_BUFSIZE];

        snprintf(output, CF_BUFSIZE, "Broken scalar variable syntax or bracket mismatch in \"%s\"", str);
        yyerror(output);
        return false;
    }

    CfDebug("Found %d variables in (%s)\n", vars, str);
    return vars;
}

/*******************************************************************/

int DefinedVariable(char *name)
{
    Rval rval;

    if (name == NULL)
    {
        return false;
    }

    if (GetVariable("this", name, &rval) == DATA_TYPE_NONE)
    {
        return false;
    }

    return true;
}

/*******************************************************************/

int BooleanControl(const char *scope, const char *name)
{
    Rval retval;

    if (name == NULL)
    {
        return false;
    }

    if (GetVariable(scope, name, &retval) != DATA_TYPE_NONE)
    {
        return BooleanFromString(retval.item);
    }

    return false;
}

/*******************************************************************/

const char *ExtractInnerCf3VarString(const char *str, char *substr)
{
    const char *sp;
    int bracks = 1;

    CfDebug("ExtractInnerVarString( %s ) - syntax verify\n", str);

    if (str == NULL || strlen(str) == 0)
    {
        return NULL;
    }

    memset(substr, 0, CF_BUFSIZE);

    if (*(str + 1) != '(' && *(str + 1) != '{')
    {
        return NULL;
    }

/* Start this from after the opening $( */

    for (sp = str + 2; *sp != '\0'; sp++)       /* check for varitems */
    {
        switch (*sp)
        {
        case '(':
        case '{':
            bracks++;
            break;
        case ')':
        case '}':
            bracks--;
            break;

        default:
            if (isalnum((int) *sp) || strchr("_[]$.:-", *sp))
            {
            }
            else
            {
                CfDebug("Illegal character found: '%c'\n", *sp);
                CfDebug("Illegal character somewhere in variable \"%s\" or nested expansion", str);
            }
        }

        if (bracks == 0)
        {
            strncpy(substr, str + 2, sp - str - 2);

            if (strlen(substr) == 0)
            {
                char output[CF_BUFSIZE];
                snprintf(output, CF_BUFSIZE, "Empty variable name in brackets: %s", str);
                yyerror(output);
                return NULL;
            }

            CfDebug("Returning substring value %s\n", substr);
            return substr;
        }
    }

    if (bracks != 0)
    {
        char output[CF_BUFSIZE];

        if (strlen(substr) > 0)
        {
            snprintf(output, CF_BUFSIZE, "Broken variable syntax or bracket mismatch - inner (%s/%s)", str, substr);
            yyerror(output);
        }
        return NULL;
    }

    return sp - 1;
}

/*********************************************************************/

const char *ExtractOuterCf3VarString(const char *str, char *substr)
  /* Should only by applied on str[0] == '$' */
{
    const char *sp;
    int dollar = false;
    int bracks = 0, onebrack = false;

    CfDebug("ExtractOuterVarString(\"%s\") - syntax verify\n", str);

    memset(substr, 0, CF_BUFSIZE);

    for (sp = str; *sp != '\0'; sp++)   /* check for varitems */
    {
        switch (*sp)
        {
        case '$':
            dollar = true;
            switch (*(sp + 1))
            {
            case '(':
            case '{':
                break;
            default:
                /* Stray dollar not a variable */
                return NULL;
            }
            break;
        case '(':
        case '{':
            bracks++;
            onebrack = true;
            break;
        case ')':
        case '}':
            bracks--;
            break;
        }

        if (dollar && (bracks == 0) && onebrack)
        {
            strncpy(substr, str, sp - str + 1);
            CfDebug("Extracted outer variable |%s|\n", substr);
            return substr;
        }
    }

    if (dollar == false)
    {
        return str;             /* This is not a variable */
    }

    if (bracks != 0)
    {
        char output[CF_BUFSIZE];

        snprintf(output, CF_BUFSIZE, "Broken variable syntax or bracket mismatch in - outer (%s/%s)", str, substr);
        yyerror(output);
        return NULL;
    }

/* Return pointer to first position in string (shouldn't happen)
   as long as we only call this function from the first $ position */

    return str;
}

/*********************************************************************/

int IsQualifiedVariable(char *var)
{
    int isarraykey = false;
    char *sp;

    for (sp = var; *sp != '\0'; sp++)
    {
        if (*sp == '[')
        {
            isarraykey = true;
        }

        if (isarraykey)
        {
            return false;
        }
        else
        {
            if (*sp == '.')
            {
                return true;
            }
        }
    }

    return false;
}

/*******************************************************************/

int AddVariableHash(const char *scope, const char *lval, Rval rval, DataType dtype, const char *fname,
                    int lineno)
{
    Scope *ptr;
    const Rlist *rp;
    CfAssoc *assoc;

    if (rval.type == RVAL_TYPE_SCALAR)
    {
        CfDebug("AddVariableHash(%s.%s=%s (%s) rtype=%c)\n", scope, lval, (const char *) rval.item, CF_DATATYPES[dtype],
                rval.type);
    }
    else
    {
        CfDebug("AddVariableHash(%s.%s=(list) (%s) rtype=%c)\n", scope, lval, CF_DATATYPES[dtype], rval.type);
    }

    if (lval == NULL || scope == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "scope.value = %s.%s", scope, lval);
        ProgrammingError("Bad variable or scope in a variable assignment, should not happen - forgotten to register a function call in fncall.c?");
    }

    if (rval.item == NULL)
    {
        CfDebug("No value to assignment - probably a parameter in an unused bundle/body\n");
        return false;
    }

    if (strlen(lval) > CF_MAXVARSIZE)
    {
        ReportError("variable lval too long");
        return false;
    }

/* If we are not expanding a body template, check for recursive singularities */

    if (strcmp(scope, "body") != 0)
    {
        switch (rval.type)
        {
        case RVAL_TYPE_SCALAR:

            if (StringContainsVar((char *) rval.item, lval))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Scalar variable %s.%s contains itself (non-convergent): %s", scope, lval,
                      (char *) rval.item);
                return false;
            }
            break;

        case RVAL_TYPE_LIST:

            for (rp = rval.item; rp != NULL; rp = rp->next)
            {
                if (StringContainsVar((char *) rp->item, lval))
                {
                    CfOut(OUTPUT_LEVEL_ERROR, "", "List variable %s contains itself (non-convergent)", lval);
                    return false;
                }
            }
            break;

        default:
            break;
        }
    }

    ptr = GetScope(scope);

    if (ptr == NULL)
    {
        return false;
    }

// Look for outstanding lists in variable rvals

    if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
    {
        Rlist *listvars = NULL, *scalarvars = NULL;

        if (strcmp(CONTEXTID, "this") != 0)
        {
            MapIteratorsFromRval(CONTEXTID, &scalarvars, &listvars, rval, NULL);

            if (listvars != NULL)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! Redefinition of variable \"%s\" (embedded list in RHS) in context \"%s\"",
                      lval, CONTEXTID);
            }

            RlistDestroy(scalarvars);
            RlistDestroy(listvars);
        }
    }

    assoc = HashLookupElement(ptr->hashtable, lval);

    if (assoc)
    {
        if (CompareVariableValue(rval, assoc) == 0)
        {
            /* Identical value, keep as is */
        }
        else
        {
            /* Different value, bark and replace */
            if (!UnresolvedVariables(assoc, rval.type))
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", " !! Duplicate selection of value for variable \"%s\" in scope %s", lval, ptr->scope);
                if (fname)
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "", " !! Rule from %s at/before line %d\n", fname, lineno);
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "", " !! in bundle parameterization\n");
                }
            }
            RvalDestroy(assoc->rval);
            assoc->rval = RvalCopy(rval);
            assoc->dtype = dtype;
            CfDebug("Stored \"%s\" in context %s\n", lval, scope);
        }
    }
    else
    {
        if (!HashInsertElement(ptr->hashtable, lval, rval, dtype))
        {
            ProgrammingError("Hash table is full");
        }
    }

    CfDebug("Added Variable %s in scope %s with value (omitted)\n", lval, scope);
    return true;
}

/*******************************************************************/

void DeRefListsInHashtable(char *scope, Rlist *namelist, Rlist *dereflist)
// Go through scope and for each variable in name-list, replace with a
// value from the deref "lol" (list of lists) clock
{
    int len;
    Scope *ptr;
    Rlist *rp;
    CfAssoc *cplist;
    AssocHashTableIterator i;
    CfAssoc *assoc;

    if ((len = RlistLen(namelist)) != RlistLen(dereflist))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Name list %d, dereflist %d\n", len, RlistLen(dereflist));
        ProgrammingError("Software Error DeRefLists... correlated lists not same length");
    }

    if (len == 0)
    {
        return;
    }

    ptr = GetScope(scope);
    i = HashIteratorInit(ptr->hashtable);

    while ((assoc = HashIteratorNext(&i)))
    {
        for (rp = dereflist; rp != NULL; rp = rp->next)
        {
            cplist = (CfAssoc *) rp->item;

            if (strcmp(cplist->lval, assoc->lval) == 0)
            {
                /* Link up temp hash to variable lol */

                if (rp->state_ptr == NULL || rp->state_ptr->type == RVAL_TYPE_FNCALL)
                {
                    /* Unexpanded function, or blank variable must be skipped. */
                    return;
                }

                if (rp->state_ptr)
                {
                    CfDebug("Rewriting expanded type for %s from %s to %s\n", assoc->lval, CF_DATATYPES[assoc->dtype],
                            (char *) rp->state_ptr->item);

                    // must first free existing rval in scope, then allocate new (should always be string)
                    RvalDestroy(assoc->rval);

                    // avoids double free - borrowing value from lol (freed in DeleteScope())
                    assoc->rval.item = xstrdup(rp->state_ptr->item);
                }

                switch (assoc->dtype)
                {
                case DATA_TYPE_STRING_LIST:
                    assoc->dtype = DATA_TYPE_STRING;
                    assoc->rval.type = RVAL_TYPE_SCALAR;
                    break;
                case DATA_TYPE_INT_LIST:
                    assoc->dtype = DATA_TYPE_INT;
                    assoc->rval.type = RVAL_TYPE_SCALAR;
                    break;
                case DATA_TYPE_REAL_LIST:
                    assoc->dtype = DATA_TYPE_REAL;
                    assoc->rval.type = RVAL_TYPE_SCALAR;
                    break;
                default:
                    /* Only lists need to be converted */
                    break;
                }

                CfDebug(" to %s\n", CF_DATATYPES[assoc->dtype]);
            }
        }
    }
}
