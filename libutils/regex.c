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


#include <platform.h>
#include <regex.h>

#include <alloc.h>
#include <logging.h>
#include <string_lib.h>


#define STRING_MATCH_OVECCOUNT 30
#define NULL_OR_EMPTY(str) ((str == NULL) || (str[0] == '\0'))


pcre *CompileRegex(const char *regex)
{
    const char *errorstr;
    int erroffset;

    pcre *rx = pcre_compile(regex, PCRE_MULTILINE | PCRE_DOTALL,
                            &errorstr, &erroffset, NULL);

    if (!rx)
    {
        Log(LOG_LEVEL_ERR,
            "Regular expression error: pcre_compile() '%s' in expression '%s' (offset: %d)",
            errorstr, regex, erroffset);
    }

    return rx;
}

bool StringMatchWithPrecompiledRegex(pcre *regex, const char *str, int *start, int *end)
{
    assert(regex);
    assert(str);

    int ovector[STRING_MATCH_OVECCOUNT] = { 0 };
    int result = pcre_exec(regex, NULL, str, strlen(str),
                           0, 0, ovector, STRING_MATCH_OVECCOUNT);

    if (result)
    {
        if (start)
        {
            *start = ovector[0];
        }
        if (end)
        {
            *end = ovector[1];
        }
    }
    else
    {
        if (start)
        {
            *start = 0;
        }
        if (end)
        {
            *end = 0;
        }
    }

    return result >= 0;
}

bool StringMatch(const char *regex, const char *str, int *start, int *end)
{
    pcre *pattern = CompileRegex(regex);

    if (pattern == NULL)
    {
        return false;
    }

    bool ret = StringMatchWithPrecompiledRegex(pattern, str, start, end);

    pcre_free(pattern);
    return ret;

}

bool StringMatchFull(const char *regex, const char *str)
{
    pcre *pattern = CompileRegex(regex);

    if (pattern == NULL)
    {
        return false;
    }

    bool ret = StringMatchFullWithPrecompiledRegex(pattern, str);

    pcre_free(pattern);
    return ret;
}

bool StringMatchFullWithPrecompiledRegex(pcre *pattern, const char *str)
{
    int start = 0, end = 0;

    if (StringMatchWithPrecompiledRegex(pattern, str, &start, &end))
    {
        return (start == 0) && (end == strlen(str));
    }
    else
    {
        return false;
    }
}

Seq *StringMatchCaptures(const char *regex, const char *str)
{
    assert(regex);
    assert(str);

    pcre *pattern = NULL;
    {
        const char *errorstr;
        int erroffset;
        pattern = pcre_compile(regex, PCRE_MULTILINE | PCRE_DOTALL,
                               &errorstr, &erroffset, NULL);
    }
    assert(pattern);

    if (pattern == NULL)
    {
        return NULL;
    }

    int captures;
    int res = pcre_fullinfo(pattern, NULL, PCRE_INFO_CAPTURECOUNT, &captures);
    if (res != 0)
    {
        pcre_free(pattern);
        return NULL;
    }

    int *ovector = xmalloc(sizeof(int) * (captures + 1) * 3);

    int result = pcre_exec(pattern, NULL, str, strlen(str),
                           0, 0, ovector, (captures + 1) * 3);

    if (result <= 0)
    {
        free(ovector);
        pcre_free(pattern);
        return NULL;
    }

    Seq *ret = SeqNew(captures + 1, free);
    for (int i = 0; i <= captures; ++i)
    {
        SeqAppend(ret, xstrndup(str + ovector[2*i],
                                ovector[2*i + 1] - ovector[2 * i]));
    }
    free(ovector);
    pcre_free(pattern);
    return ret;
}

bool CompareStringOrRegex(const char *value, const char *compareTo, bool regex)
{
    if (regex)
    {
        if (!NULL_OR_EMPTY(compareTo) && !StringMatchFull(compareTo, value))
        {
            return false;
        }
    }
    else
    {
        if (!NULL_OR_EMPTY(compareTo)  && strcmp(compareTo, value) != 0)
        {
            return false;
        }
    }
    return true;
}
