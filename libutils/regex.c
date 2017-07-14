/*
   Copyright 2017 Northern.tech AS

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


#include <platform.h>
#include <regex.h>

#include <alloc.h>
#include <logging.h>
#include <string_lib.h>

#include <buffer.h>

#define STRING_MATCH_OVECCOUNT 30


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

// Returns a Sequence with Buffer elements.

// If return_names is set, the even positions will be the name or
// number of the capturing group, followed by the captured data in the
// odd positions (so for N captures you can expect 2N elements in the
// Sequence).

// If return_names is not set, only the captured data is returned (so
// for N captures you can expect N elements in the Sequence).
Seq *StringMatchCapturesWithPrecompiledRegex(const pcre *pattern, const char *str, const bool return_names)
{
    int captures;
    int res = pcre_fullinfo(pattern, NULL, PCRE_INFO_CAPTURECOUNT, &captures);
    if (res != 0)
    {
        return NULL;
    }

    // Get the table of named captures.
    unsigned char *name_table = NULL; // Doesn't have to be freed as per docs.
    int namecount = 0;
    int name_entry_size = 0;
    unsigned char *tabptr;

    pcre_fullinfo(pattern, NULL, PCRE_INFO_NAMECOUNT, &namecount);

    const bool have_named_captures = (namecount > 0 && return_names);

    if (have_named_captures)
    {
        pcre_fullinfo(pattern, NULL, PCRE_INFO_NAMETABLE, &name_table);
        pcre_fullinfo(pattern, NULL, PCRE_INFO_NAMEENTRYSIZE, &name_entry_size);
    }

    int *ovector = xmalloc(sizeof(int) * (captures + 1) * 3);

    int result = pcre_exec(pattern, NULL, str, strlen(str),
                           0, 0, ovector, (captures + 1) * 3);

    if (result <= 0)
    {
        free(ovector);
        return NULL;
    }

    Seq *ret = SeqNew(captures + 1, BufferDestroy);
    for (int i = 0; i <= captures; ++i)
    {
        Buffer *capture = NULL;

        if (have_named_captures)
        {
            // The overhead of doing a nested name scan is negligible.
            tabptr = name_table;
            for (int namepos = 0; namepos < namecount; namepos++)
            {
                int n = (tabptr[0] << 8) | tabptr[1];
                if (n == i) // We found the position
                {
                    capture = BufferNewFrom(tabptr + 2, name_entry_size - 3);
                    break;
                }
                tabptr += name_entry_size;
            }
        }

        if (return_names)
        {
            if (capture == NULL)
            {
                capture = BufferNew();
                BufferAppendF(capture, "%zd", i);
            }

            SeqAppend(ret, capture);
        }

        Buffer *data = BufferNewFrom(str + ovector[2*i],
                                     ovector[2*i + 1] - ovector[2 * i]);
        Log(LOG_LEVEL_DEBUG, "StringMatchCaptures: return_names = %d, have_named_captures = %d, offset %d, name '%s', data '%s'", return_names, have_named_captures, i, capture == NULL ? "no_name" : BufferData(capture), BufferData(data));
        SeqAppend(ret, data);
    }

    free(ovector);
    return ret;
}

// Returns a Sequence with Buffer elements.

// If return_names is set, the even positions will be the name or
// number of the capturing group, followed by the captured data in the
// odd positions (so for N captures you can expect 2N elements in the
// Sequence).

// If return_names is not set, only the captured data is returned (so
// for N captures you can expect N elements in the Sequence).

Seq *StringMatchCaptures(const char *regex, const char *str, const bool return_names)
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

    if (pattern == NULL)
    {
        return NULL;
    }

    Seq *ret = StringMatchCapturesWithPrecompiledRegex(pattern, str, return_names);
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

/*
 * This is a fast partial match function. It checks that the compiled rx matches
 * anywhere inside teststring. It does not allocate or free rx!
 */
bool RegexPartialMatch(const pcre *rx, const char *teststring)
{
    int ovector[STRING_MATCH_OVECCOUNT];
    int rc = pcre_exec(rx, NULL, teststring, strlen(teststring), 0, 0, ovector, STRING_MATCH_OVECCOUNT);

    return rc >= 0;
}
