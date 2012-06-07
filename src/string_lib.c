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

bool IsStrIn(const char *str, const char **strs)
{
    int i;

    for (i = 0; strs[i]; ++i)
    {
        if (strcmp(str, strs[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

bool IsStrCaseIn(const char *str, const char **strs)
{
    int i;

    for (i = 0; strs[i]; ++i)
    {
        if (strcasecmp(str, strs[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

char *Titleize(char *str)
{
    static char buffer[CF_BUFSIZE];
    int i;

    if (str == NULL)
    {
        return NULL;
    }

    strcpy(buffer, str);

    if (strlen(buffer) > 1)
    {
        for (i = 1; buffer[i] != '\0'; i++)
        {
            buffer[i] = ToLower(str[i]);
        }
    }

    *buffer = ToUpper(*buffer);

    return buffer;
}

int SubStrnCopyChr(char *to, const char *from, int len, char sep)
{
    char *sto = to;
    int count = 0;

    memset(to, 0, len);

    if (from == NULL)
    {
        return 0;
    }

    if (from && strlen(from) == 0)
    {
        return 0;
    }

    for (const char *sp = from; *sp != '\0'; sp++)
    {
        if (count > len - 1)
        {
            break;
        }

        if (*sp == '\\' && *(sp + 1) == sep)
        {
            *sto++ = *++sp;
        }
        else if (*sp == sep)
        {
            break;
        }
        else
        {
            *sto++ = *sp;
        }

        count++;
    }

    return count;
}

int CountChar(char *string, char sep)
{
    char *sp;
    int count = 0;

    if (string == NULL)
    {
        return 0;
    }

    if (string && strlen(string) == 0)
    {
        return 0;
    }

    for (sp = string; *sp != '\0'; sp++)
    {
        if (*sp == '\\' && *(sp + 1) == sep)
        {
            ++sp;
        }
        else if (*sp == sep)
        {
            count++;
        }
    }

    return count;
}

void ReplaceChar(char *in, char *out, int outSz, char from, char to)
/* Replaces all occurences of 'from' to 'to' in preallocated
 * string 'out'. */
{
    int len;
    int i;

    memset(out, 0, outSz);
    len = strlen(in);

    for (i = 0; (i < len) && (i < outSz - 1); i++)
    {
        if (in[i] == from)
        {
            out[i] = to;
        }
        else
        {
            out[i] = in[i];
        }
    }
}

int ReplaceStr(char *in, char *out, int outSz, char *from, char *to)
/* Replaces all occurences of strings 'from' to 'to' in preallocated
 * string 'out'. Returns true on success, false otherwise. */
{
    int inSz;
    int outCount;
    int inCount;
    int fromSz, toSz;

    memset(out, 0, outSz);

    inSz = strlen(in);
    fromSz = strlen(from);
    toSz = strlen(to);

    inCount = 0;
    outCount = 0;

    while ((inCount < inSz) && (outCount < outSz))
    {
        if (strncmp(in + inCount, from, fromSz) == 0)
        {
            if (outCount + toSz >= outSz)
            {
                return false;
            }

            strcat(out, to);

            inCount += fromSz;
            outCount += toSz;
        }
        else
        {
            out[outCount] = in[inCount];

            inCount++;
            outCount++;
        }
    }

    return true;
}

void ReplaceTrailingChar(char *str, char from, char to)
/* Replaces any unwanted last char in str. */
{
    int strLen;

    strLen = SafeStringLength(str);

    if (strLen == 0)
    {
        return;
    }

    if (str[strLen - 1] == from)
    {
        str[strLen - 1] = to;
    }
}

void ReplaceTrailingStr(char *str, char *from, char to)
/* Replaces any unwanted last chars in str. */
{
    int strLen;
    int fromLen;
    char *startCmp = NULL;

    strLen = strlen(str);
    fromLen = strlen(from);

    if (strLen == 0)
    {
        return;
    }

    startCmp = str + strLen - fromLen;

    if (strcmp(startCmp, from) == 0)
    {
        memset(startCmp, to, fromLen);
    }
}

char **String2StringArray(char *str, char separator)
/**
 * Parse CSVs into char **.
 * MEMORY NOTE: Caller must free return value with FreeStringArray().
 **/
{
    char *sp, *esp;
    int i = 0, len;

    if (str == NULL)
    {
        return NULL;
    }

    for (sp = str; *sp != '\0'; sp++)
    {
        if (*sp == separator)
        {
            i++;
        }
    }

    char **arr = (char **) xcalloc(i + 2, sizeof(char *));

    sp = str;
    i = 0;

    while (sp)
    {
        esp = strchr(sp, separator);

        if (esp)
        {
            len = esp - sp;
            esp++;
        }
        else
        {
            len = strlen(sp);
        }

        arr[i] = xcalloc(len + 1, sizeof(char));
        strncpy(arr[i], sp, len);

        sp = esp;
        i++;
    }

    return arr;
}

void FreeStringArray(char **strs)
{
    int i;

    if (strs == NULL)
    {
        return;
    }

    for (i = 0; strs[i] != NULL; i++)
    {
        free(strs[i]);
    }

    free(strs);
    strs = NULL;
}
