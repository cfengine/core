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

#include <vars.h>

#include <conversion.h>
#include <expand.h>
#include <scope.h>
#include <matching.h>
#include <hashes.h>
#include <unix.h>
#include <misc_lib.h>
#include <rlist.h>
#include <policy.h>
#include <eval_context.h>

static int IsCf3Scalar(char *str);

/*******************************************************************/

bool RlistIsUnresolved(const Rlist *list)
{
    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        // JSON data container values are never expanded, except with the
        // data_expand() function which see.
        if (rp->val.type == RVAL_TYPE_CONTAINER)
        {
            continue;
        }

        if (rp->val.type != RVAL_TYPE_SCALAR)
        {
            return true;
        }

        if (IsCf3Scalar(RlistScalarValue(rp)))
        {
            if (strstr(RlistScalarValue(rp), "$(this)") || strstr(RlistScalarValue(rp), "${this}") ||
                strstr(RlistScalarValue(rp), "$(this.k)") || strstr(RlistScalarValue(rp), "${this.k}") ||
                strstr(RlistScalarValue(rp), "$(this.k[1])") || strstr(RlistScalarValue(rp), "${this.k[1]}") ||
                strstr(RlistScalarValue(rp), "$(this.v)") || strstr(RlistScalarValue(rp), "${this.v}"))
            {
                // We should allow this in function args for substitution in maplist() etc
                // We should allow this.k and this.k[1] and this.v in function args for substitution in maparray() etc
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

        snprintf(output, CF_BUFSIZE, "Broken scalar variable syntax or bracket mismatch in '%s'", str);
        yyerror(output);
        return false;
    }

    return vars;
}

/* Extract everything up to the dollar sign. */
size_t ExtractScalarPrefix(Buffer *out, const char *str, size_t len)
{
    assert(str);
    if (len == 0)
    {
        return 0;
    }

    const char *dollar_point = NULL;
    for (size_t i = 0; i < (len - 1); i++)
    {
        if (str[i] == '$')
        {
            if (str[i + 1] == '(' || str[i + 1] == '{')
            {
                dollar_point = str + i;
                break;
            }
        }
    }

    if (!dollar_point)
    {
        BufferAppend(out, str, len);
        return len;
    }
    else if (dollar_point > str)
    {
        size_t prefix_len = dollar_point - str;
        if (prefix_len > 0)
        {
            BufferAppend(out, str, prefix_len);
        }
        return prefix_len;
    }
    return 0;
}

static const char *ReferenceEnd(const char *str, size_t len)
{
    assert(len > 1);
    assert(str[0] == '$');
    assert(str[1] == '{' || str[1] == '(');

#define MAX_VARIABLE_REFERENCE_LEVELS 10
    char stack[MAX_VARIABLE_REFERENCE_LEVELS] = { 0, str[1], 0 };
    int level = 1;

    for (size_t i = 2; i < len; i++)
    {
        switch (str[i])
        {
        case '{':
        case '(':
            if (level < MAX_VARIABLE_REFERENCE_LEVELS - 1)
            {
                level++;
                stack[level] = str[i];
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Stack overflow in variable reference parsing. More than %d levels", MAX_VARIABLE_REFERENCE_LEVELS);
                return NULL;
            }
            break;

        case '}':
            if (stack[level] != '{')
            {
                Log(LOG_LEVEL_ERR, "Variable reference bracket mismatch '%.*s'",
                    (int) len, str);
                return NULL;
            }
            level--;
            break;
        case ')':
            if (stack[level] != '(')
            {
                Log(LOG_LEVEL_ERR, "Variable reference bracket mismatch '%.*s'",
                    (int) len, str);
                return NULL;
            }
            level--;
            break;
        }

        if (level == 0)
        {
            return str + i;
        }
    }

    return NULL;
}

/**
 * Extract variable inside dollar-paren.
 * @param extract_inner ignore opening dollar-paren and closing paren.
 */
bool ExtractScalarReference(Buffer *out, const char *str, size_t len, bool extract_inner)
{
    if (len <= 1)
    {
        return false;
    }

    const char *dollar_point = memchr(str, '$', len);
    if (!dollar_point || (dollar_point - str) == len)
    {
        return false;
    }
    else
    {
        const char *close_point = NULL;
        {
            size_t remaining = len - (dollar_point - str);
            if (*(dollar_point + 1) == '{' || *(dollar_point + 1) == '(')
            {
                close_point = ReferenceEnd(dollar_point, remaining);
            }
            else
            {
                return ExtractScalarReference(out, dollar_point + 1,
                                              remaining - 1, extract_inner);
            }
        }

        if (!close_point)
        {
            Log(LOG_LEVEL_ERR, "Variable reference close mismatch '%.*s'",
                (int) len, str);
            return false;
        }

        size_t outer_len = close_point - dollar_point + 1;
        if (outer_len <= 3)
        {
            Log(LOG_LEVEL_ERR, "Empty variable reference close mismatch '%.*s'",
                (int) len, str);
            return false;
        }

        if (extract_inner)
        {
            BufferAppend(out, dollar_point + 2, outer_len - 3);
        }
        else
        {
            BufferAppend(out, dollar_point, outer_len);
        }
        return true;
    }
}

/*********************************************************************/

bool IsQualifiedVariable(const char *var)
{
    int isarraykey = false;

    for (const char *sp = var; *sp != '\0'; sp++)
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
