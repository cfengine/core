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

#include "cf3.defs.h"
#include "cf3.extern.h"
#include "writer.h"

#include <assert.h>

#ifdef HAVE_PCRE_H
# include <pcre.h>
#endif

#ifdef HAVE_PCRE_PCRE_H
# include <pcre/pcre.h>
#endif


char ToLower(char ch)
{
    if (isupper((int) ch))
    {
        return (ch - 'A' + 'a');
    }
    else
    {
        return (ch);
    }
}

/*********************************************************************/

char ToUpper(char ch)
{
    if (isdigit((int) ch) || ispunct((int) ch))
    {
        return (ch);
    }

    if (isupper((int) ch))
    {
        return (ch);
    }
    else
    {
        return (ch - 'a' + 'A');
    }
}

/*********************************************************************/

void ToUpperStrInplace(char *str)
{
    for (; *str != '\0'; str++)
    {
        *str = ToUpper(*str);
    }
}

/*********************************************************************/

char *ToUpperStr(const char *str)
{
    static char buffer[CF_BUFSIZE];

    if (strlen(str) >= CF_BUFSIZE)
    {
        FatalError("String too long in ToUpperStr: %s", str);
    }

    strlcpy(buffer, str, CF_BUFSIZE);
    ToUpperStrInplace(buffer);

    return buffer;
}

/*********************************************************************/

void ToLowerStrInplace(char *str)
{
    for (; *str != '\0'; str++)
    {
        *str = ToLower(*str);
    }
}

/*********************************************************************/

char *ToLowerStr(const char *str)
{
    static char buffer[CF_BUFSIZE];

    if (strlen(str) >= CF_BUFSIZE - 1)
    {
        FatalError("String too long in ToLowerStr: %s", str);
    }

    strlcpy(buffer, str, CF_BUFSIZE);

    ToLowerStrInplace(buffer);

    return buffer;
}

/*********************************************************************/

char *SafeStringDuplicate(const char *str)
{
    if (str == NULL)
    {
        return NULL;
    }

    return xstrdup(str);
}

/*********************************************************************/

int SafeStringLength(const char *str)
{
    if (str == NULL)
    {
        return 0;
    }

    return strlen(str);
}

int StringSafeCompare(const char *a, const char *b)
{
    if (a == b)
    {
        return 0;
    }

    if (a == NULL || b == NULL)
    {
        return -1;
    }

    return strcmp(a, b);
}

bool StringSafeEqual(const char *a, const char *b)
{
    if (a == b)
    {
        return true;
    }

    if (a == NULL || b == NULL)
    {
        return false;
    }

    return strcmp(a, b) == 0;
}

/*******************************************************************/

int StripListSep(char *strList, char *outBuf, int outBufSz)
{
    memset(outBuf, 0, outBufSz);

    if (NULL_OR_EMPTY(strList))
    {
        return false;
    }

    if (strList[0] != '{')
    {
        return false;
    }

    snprintf(outBuf, outBufSz, "%s", strList + 1);

    if (outBuf[strlen(outBuf) - 1] == '}')
    {
        outBuf[strlen(outBuf) - 1] = '\0';
    }

    return true;
}

/*******************************************************************/

/** Takes a string-parsed list "{'el1','el2','el3',..}" and writes
 ** "el1" or "el2" etc. based on index (starting on 0) in outBuf.
 ** returns true on success, false otherwise.
 **/

int GetStringListElement(char *strList, int index, char *outBuf, int outBufSz)
{
    char *sp, *elStart = strList, *elEnd;
    int elNum = 0;
    int minBuf;

    memset(outBuf, 0, outBufSz);

    if (NULL_OR_EMPTY(strList))
    {
        return false;
    }

    if (strList[0] != '{')
    {
        return false;
    }

    for (sp = strList; *sp != '\0'; sp++)
    {
        if ((sp[0] == '{' || sp[0] == ',') && sp[1] == '\'')
        {
            elStart = sp + 2;
        }

        else if ((sp[0] == '\'') && (sp[1] == ',' || sp[1] == '}'))
        {
            elEnd = sp;

            if (elNum == index)
            {
                if (elEnd - elStart < outBufSz)
                {
                    minBuf = elEnd - elStart;
                }
                else
                {
                    minBuf = outBufSz - 1;
                }

                strncpy(outBuf, elStart, minBuf);

                break;
            }

            elNum++;
        }
    }

    return true;
}

/*********************************************************************/

char *SearchAndReplace(const char *source, const char *search, const char *replace)
{
    const char *source_ptr = source;

    if (source == NULL || search == NULL || replace == NULL)
    {
        FatalError("Programming error: NULL argument is passed to SearchAndReplace");
    }

    if (strcmp(search, "") == 0)
    {
        return xstrdup(source);
    }

    Writer *w = StringWriter();

    for (;;)
    {
        const char *found_ptr = strstr(source_ptr, search);

        if (found_ptr == NULL)
        {
            WriterWrite(w, source_ptr);
            return StringWriterClose(w);
        }

        WriterWriteLen(w, source_ptr, found_ptr - source_ptr);
        WriterWrite(w, replace);

        source_ptr += found_ptr - source_ptr + strlen(search);
    }
}

/*********************************************************************/

char *StringConcatenate(const char *a, size_t a_len, const char *b, size_t b_len)
{
    char *result = xcalloc(a_len + b_len + 1, sizeof(char));

    strncat(result, a, a_len);
    strncat(result, b, b_len);
    return result;
}

/*********************************************************************/

char *StringSubstring(const char *source, size_t source_len, int start, int len)
{
    size_t end = -1;

    if (len == 0)
    {
        return SafeStringDuplicate("");
    }
    else if (len < 0)
    {
        end = source_len + len - 1;
    }
    else
    {
        end = start + len - 1;
    }

    end = MIN(end, source_len - 1);

    if (start < 0)
    {
        start = source_len + start;
    }

    if (start >= end)
    {
        return NULL;
    }

    char *result = xcalloc(end - start + 2, sizeof(char));

    strncpy(result, source + start, end - start + 1);
    return result;
}

/*********************************************************************/

bool IsNumber(const char *s)
{
    for (; *s; s++)
    {
        if (!isdigit(*s))
        {
            return false;
        }
    }

    return true;
}

/*********************************************************************/

long StringToLong(const char *str)
{
    assert(str);

    char *end;
    long result = strtol(str, &end, 10);

    assert(!*end && "Failed to convert string to long");

    return result;
}

/*********************************************************************/

double StringToDouble(const char *str)
{
    assert(str);

    char *end;
    double result = strtod(str, &end);

    assert(!*end && "Failed to convert string to double");

    return result;
}

/*********************************************************************/

char *NULLStringToEmpty(char *str)
{
    if(!str)
    {
        return "";
    }

    return str;
}

static bool StringMatchInternal(const char *regex, const char *str, int *start, int *end)
{
    assert(regex);
    assert(str);

    if (strcmp(regex, str) == 0)
    {
        if (start)
        {
            *start = 0;
        }
        if (end)
        {
            *end = strlen(str);
        }

        return true;
    }

    pcre *pattern = NULL;
    {
        const char *errorstr;
        int erroffset;
        pattern = pcre_compile(regex, PCRE_MULTILINE | PCRE_DOTALL, &errorstr, &erroffset, NULL);
    }
    assert(pattern);

    if (pattern == NULL)
    {
        return false;
    }

    int ovector[OVECCOUNT] = { 0 };
    int result = pcre_exec(pattern, NULL, str, strlen(str), 0, 0, ovector, OVECCOUNT);

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

    free(pattern);

    return result >= 0;
}

bool StringMatch(const char *regex, const char *str)
{
    return StringMatchInternal(regex, str, NULL, NULL);
}

bool StringMatchFull(const char *regex, const char *str)
{
    int start = 0, end = 0;

    if (StringMatchInternal(regex, str, &start, &end))
    {
        return start == 0 && end == strlen(str);
    }
    else
    {
        return false;
    }
}
