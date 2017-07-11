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

#include <matching.h>

#include <eval_context.h>
#include <vars.h>
#include <promises.h>
#include <item_lib.h>
#include <conversion.h>
#include <scope.h>
#include <misc_lib.h>
#include <rlist.h>
#include <regex.h>                          /* CompileRegex,StringMatchFull */

/* Pure, non-thread-safe */
static char *FirstBackReference(pcre *rx, const char *teststring)
{
    static char backreference[CF_BUFSIZE]; /* GLOBAL_R, no initialization needed */

    int ovector[OVECCOUNT], i, rc;

    memset(backreference, 0, CF_BUFSIZE);

    if ((rc = pcre_exec(rx, NULL, teststring, strlen(teststring), 0, 0, ovector, OVECCOUNT)) >= 0)
    {
        for (i = 1; i < rc; i++)        /* make backref vars $(1),$(2) etc */
        {
            const char *backref_start = teststring + ovector[i * 2];
            int backref_len = ovector[i * 2 + 1] - ovector[i * 2];

            if (backref_len < CF_MAXVARSIZE)
            {
                strncpy(backreference, backref_start, backref_len);
            }

            break;
        }
    }

    free(rx);

    return backreference;
}

char *ExtractFirstReference(const char *regexp, const char *teststring)
{
    char *backreference;

    pcre *rx;

    if ((regexp == NULL) || (teststring == NULL))
    {
        return "";
    }

    rx = CompileRegex(regexp);
    if (rx == NULL)
    {
        return "";
    }

    backreference = FirstBackReference(rx, teststring);

    if (strlen(backreference) == 0)
    {
        strlcpy(backreference, "CF_NOMATCH", CF_MAXVARSIZE);
    }

    return backreference;
}

int IsRegex(const char *str)
{
    const char *sp;
    int ret = false;
    enum { r_norm, r_norepeat, r_literal } special = r_norepeat;
    int bracket = 0;
    int paren = 0;

/* Try to see when something is intended as a regular expression */

    for (sp = str; *sp != '\0'; sp++)
    {
        if (special == r_literal)
        {
            special = r_norm;
            continue;
        }
        else if (*sp == '\\')
        {
            special = r_literal;
            continue;
        }
        else if (bracket && (*sp != ']'))
        {
            if (*sp == '[')
            {
                return false;
            }
            continue;
        }

        switch (*sp)
        {
        case '^':
            special = (sp == str) ? r_norepeat : r_norm;
            break;
        case '*':
        case '+':
            if (special == r_norepeat)
            {
                return false;
            }
            special = r_norepeat;
            ret = true;
            break;
        case '[':
            special = r_norm;
            bracket++;
            ret = true;
            break;
        case ']':
            if (bracket == 0)
            {
                return false;
            }
            bracket = 0;
            special = r_norm;
            break;
        case '(':
            special = r_norepeat;
            paren++;
            break;

        case ')':
            special = r_norm;
            paren--;
            if (paren < 0)
            {
                return false;
            }
            break;

        case '|':
            special = r_norepeat;
            if (paren > 0)
            {
                ret = true;
            }
            break;

        default:
            special = r_norm;
        }

    }

    if ((bracket != 0) || (paren != 0) || (special == r_literal))
    {
        return false;
    }
    else
    {
        return ret;
    }
}

int IsPathRegex(const char *str)
{
    int result = IsRegex(str);

    if (result)
    {
        int s = 0, r = 0; /* Count square and round brackets. */
        for (const char *sp = str; *sp != '\0'; sp++)
        {
            switch (*sp)
            {
            case '[':
                s++;
                break;
            case ']':
                s--;
                if (s % 2 == 0)
                {
                    result++;
                }
                break;
            case '(':
                r++;
                break;
            case ')':
                r--;
                if (r % 2 == 0)
                {
                    result++;
                }
                break;
            default:

                if (*sp == FILE_SEPARATOR && (r || s))
                {
                    Log(LOG_LEVEL_ERR,
                          "Path regular expression %s seems to use expressions containing the directory symbol %c", str,
                          FILE_SEPARATOR);
                    Log(LOG_LEVEL_ERR, "Use a work-around to avoid pathological behaviour");
                    return false;
                }
                break;
            }
        }
    }

    return result;
}

/* Checks whether item matches a list of wildcards */

int IsRegexItemIn(const EvalContext *ctx, const Item *list, const char *regex)
{
    for (const Item *ptr = list; ptr != NULL; ptr = ptr->next)
    {
        if (ctx != NULL && ptr->classes != NULL &&
            !IsDefinedClass(ctx, ptr->classes))
        {
            continue;
        }

        /* Cheap pre-test: */
        if (strcmp(regex, ptr->name) == 0)
        {
            return true;
        }

        /* Make it commutative */

        if (StringMatchFull(regex, ptr->name) || StringMatchFull(ptr->name, regex))
        {
            return true;
        }
    }

    return false;
}

/* Escapes non-alphanumeric chars, except sequence given in noEscSeq */

void EscapeSpecialChars(const char *str, char *strEsc, int strEscSz, char *noEscSeq, char *noEscList)
{
    int strEscPos = 0;

    if (noEscSeq == NULL)
    {
        noEscSeq = "";
    }

    if (noEscList == NULL)
    {
        noEscList = "";
    }

    memset(strEsc, 0, strEscSz);

    for (const char *sp = str; (*sp != '\0') && (strEscPos < strEscSz - 2); sp++)
    {
        if (strncmp(sp, noEscSeq, strlen(noEscSeq)) == 0)
        {
            if (strEscSz <= strEscPos + strlen(noEscSeq))
            {
                break;
            }

            strcat(strEsc, noEscSeq);
            strEscPos += strlen(noEscSeq);
            sp += strlen(noEscSeq);
        }

        if (strchr(noEscList,*sp))
        {
        }        
        else if ((*sp != '\0') && (!isalnum((int)*sp)))
        {
            strEsc[strEscPos++] = '\\';
        }

        strEsc[strEscPos++] = *sp;
    }
}

size_t EscapeRegexCharsLen(const char *str)
{
    size_t ret = 2;
    for (const char *sp = str; *sp != '\0'; sp++)
    {
        switch (*sp)
        {
            case '.':
            case '*':
                ret++;
                break;
            default:
                break;
        }

        ret++;
    }

    return ret;
}

void EscapeRegexChars(char *str, char *strEsc, int strEscSz)
{
    char *sp;
    int strEscPos = 0;

    memset(strEsc, 0, strEscSz);

    for (sp = str; (*sp != '\0') && (strEscPos < strEscSz - 2); sp++)
    {
        switch (*sp)
        {
        case '.':
        case '*':
            strEsc[strEscPos++] = '\\';
            break;
        default:
            break;
        }

        strEsc[strEscPos++] = *sp;
    }
}

/* Escapes characters esc in the string str of size strSz  */

char *EscapeChar(char *str, int strSz, char esc)
{
    char strDup[CF_BUFSIZE];
    int strPos, strDupPos;

    if (sizeof(strDup) < strSz)
    {
        ProgrammingError("Too large string passed to EscapeCharInplace()");
    }

    snprintf(strDup, sizeof(strDup), "%s", str);
    memset(str, 0, strSz);

    for (strPos = 0, strDupPos = 0; strPos < strSz - 2; strPos++, strDupPos++)
    {
        if (strDup[strDupPos] == esc)
        {
            str[strPos] = '\\';
            strPos++;
        }

        str[strPos] = strDup[strDupPos];
    }

    return str;
}

void AnchorRegex(const char *regex, char *out, int outSz)
{
    if (NULL_OR_EMPTY(regex))
    {
        memset(out, 0, outSz);
    }
    else
    {
        snprintf(out, outSz, "^(%s)$", regex);
    }
}

char *AnchorRegexNew(const char *regex)
{
    if (NULL_OR_EMPTY(regex))
    {
        return xstrdup("^$");
    }

    char *ret = NULL;
    xasprintf(&ret, "^(%s)$", regex);

    return ret;
}

bool HasRegexMetaChars(const char *string)
{
    if (!string)
    {
        return false;
    }

    if (string[strcspn(string, "\\^${}[]().*+?|<>-&")] == '\0') /* i.e. no metachars appear in string */
    {
        return false;
    }

    return true;
}
