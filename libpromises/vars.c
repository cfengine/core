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

#include "vars.h"

#include "conversion.h"
#include "expand.h"
#include "scope.h"
#include "matching.h"
#include "hashes.h"
#include "unix.h"
#include "misc_lib.h"
#include "rlist.h"
#include "policy.h"

static int IsCf3Scalar(char *str);

void LoadSystemConstants(EvalContext *ctx)
{
    ScopeNewSpecialScalar(ctx, "const", "dollar", "$", DATA_TYPE_STRING);
    ScopeNewSpecialScalar(ctx, "const", "n", "\n", DATA_TYPE_STRING);
    ScopeNewSpecialScalar(ctx, "const", "r", "\r", DATA_TYPE_STRING);
    ScopeNewSpecialScalar(ctx, "const", "t", "\t", DATA_TYPE_STRING);
    ScopeNewSpecialScalar(ctx, "const", "endl", "\n", DATA_TYPE_STRING);
/* NewScalar("const","0","\0",cf_str);  - this cannot work */

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
            if (strstr(rp->item, "$(this)") || strstr(rp->item, "${this}") ||
                strstr(rp->item, "$(this.k)") || strstr(rp->item, "${this.k}") ||
                strstr(rp->item, "$(this.v)") || strstr(rp->item, "${this.v}"))
            {
                // We should allow this in function args for substitution in maplist() etc
                // We should allow this.k and this.v in function args for substitution in maparray() etc
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

    return vars;
}

/*********************************************************************/

static int IsCf3Scalar(char *str)
{
    char *sp;
    char left = 'x', right = 'x';
    int dollar = false;
    int bracks = 0, vars = 0;

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

    return vars;
}

/*******************************************************************/

const char *ExtractInnerCf3VarString(const char *str, char *substr)
{
    const char *sp;
    int bracks = 1;

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
            if (isalnum((int) *sp) || strchr("_[]$.:-# ", *sp))
            {
            }
            else
            {
                Log(LOG_LEVEL_DEBUG, "Illegal character found '%c'", *sp);
                Log(LOG_LEVEL_DEBUG, "Illegal character somewhere in variable '%s' or nested expansion", str);
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

            Log(LOG_LEVEL_DEBUG, "Returning substring value '%s'", substr);
            return substr;
        }
    }

    if (bracks != 0)
    {
        char output[CF_BUFSIZE];

        if (strlen(substr) > 0)
        {
            snprintf(output, CF_BUFSIZE, "Broken variable syntax or bracket mismatch - inner '%s/%s'", str, substr);
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
