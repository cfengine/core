/*
  Copyright 2024 Northern.tech AS

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
#include <string_lib.h>


/* Pure, non-thread-safe */
static char *FirstBackReference(Regex *regex, const char *teststring)
{
    static char backreference[CF_BUFSIZE]; /* GLOBAL_R, no initialization needed */
    memset(backreference, 0, CF_BUFSIZE);

    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(regex, NULL);
    int result = pcre2_match(regex, (PCRE2_SPTR) teststring, PCRE2_ZERO_TERMINATED,
                             0, 0, match_data, NULL);
    /* pcre2_match() returns the highest capture group number + 1, i.e. 1 means
     * a match with 0 capture groups. 0 means the vector of offsets is small,
     * negative numbers are errors (incl. no match). */
    if (result > 0)
    {
        size_t *ovector = pcre2_get_ovector_pointer(match_data);
        /* ovector[0] and ovector[1] are for the start and end of the whole
         * match, the capture groups follow in [2] and [3], etc. */
        const char *backref_start = teststring + ovector[2];
        size_t backref_len = ovector[3] - ovector[2];
        if (backref_len < CF_MAXVARSIZE)
        {
            strncpy(backreference, backref_start, backref_len);
        }
    }

    pcre2_match_data_free(match_data);
    RegexDestroy(regex);
    return backreference;
}

char *ExtractFirstReference(const char *regexp, const char *teststring)
{
    char *backreference;

    if ((regexp == NULL) || (teststring == NULL))
    {
        return "";
    }

    Regex *rx = CompileRegex(regexp);
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

bool IsRegex(const char *str)
{
    const char *sp;
    bool ret = false;
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

bool IsPathRegex(const char *str)
{
    bool result = IsRegex(str);

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
                break;
            case '(':
                r++;
                break;
            case ')':
                r--;
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

bool IsRegexItemIn(const EvalContext *ctx, const Item *list, const char *regex)
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

void EscapeSpecialChars(const char *str, char *strEsc, size_t strEscSz, char *noEscSeq, char *noEscList)
{
    size_t strEscPos = 0;

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
                Log(LOG_LEVEL_ERR,
                      "EscapeSpecialChars: Output string truncated. in='%s' out='%s'",
                      str, strEsc);
                break;
            }

            strlcat(strEsc, noEscSeq, strEscSz);
            strEscPos += strlen(noEscSeq);
            sp += strlen(noEscSeq);
        }

        if (strchr(noEscList,*sp) != NULL)
        {
            // Found current char (*sp) in noEscList, do nothing
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

char *EscapeChar(char *str, size_t strSz, char esc)
{
    char strDup[CF_BUFSIZE];
    size_t strPos, strDupPos;

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
