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
    if ((isdigit((int) ch)) || (ispunct((int) ch)))
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

    if ((a == NULL) || (b == NULL))
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

    if ((a == NULL) || (b == NULL))
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
        if (((sp[0] == '{') || (sp[0] == ',')) && (sp[1] == '\''))
        {
            elStart = sp + 2;
        }

        else if ((sp[0] == '\'') && ((sp[1] == ',') || (sp[1] == '}')))
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

    if ((source == NULL) || (search == NULL) || (replace == NULL))
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

char *StringConcatenate(size_t count, const char *first, ...)
{
    if (count < 1)
    {
        return NULL;
    }

    size_t total_length = first ? strlen(first) : 0;

    va_list args;
    va_start(args, first);
    for (size_t i = 1; i < count; i++)
    {
        const char *arg = va_arg(args, const char*);
        if (arg)
        {
            total_length += strlen(arg);
        }
    }
    va_end(args);

    char *result = xcalloc(total_length + 1, sizeof(char));
    if (first)
    {
        strcat(result, first);
    }

    va_start(args, first);
    for (size_t i = 1; i < count; i++)
    {
        const char *arg = va_arg(args, const char *);
        if (arg)
        {
            strcat(result, arg);
        }
    }
    va_end(args);

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
        if (!isdigit((int)*s))
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

char *StringFromLong(long number)
{
    char *str = xcalloc(32, sizeof(char));
    snprintf(str, 32, "%ld", number);
    return str;
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
        return (start == 0) && (end == strlen(str));
    }
    else
    {
        return false;
    }
}

char *StringEncodeBase64(const char *str, size_t len)
{
    assert(str);
    if (!str)
    {
        return NULL;
    }

    if (len == 0)
    {
        return xcalloc(1, sizeof(char));
    }

    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bio = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bio);
    BIO_write(b64, str, len);
    if (!BIO_flush(b64))
    {
        assert(false && "Unable to encode string to base64" && str);
        return NULL;
    }

    BUF_MEM *buffer = NULL;
    BIO_get_mem_ptr(b64, &buffer);
    char *out = xcalloc(1, buffer->length);
    memcpy(out, buffer->data, buffer->length - 1);
    out[buffer->length - 1] = '\0';

    BIO_free_all(b64);

    return out;
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

    if (from && (strlen(from) == 0))
    {
        return 0;
    }

    for (const char *sp = from; *sp != '\0'; sp++)
    {
        if (count > len - 1)
        {
            break;
        }

        if ((*sp == '\\') && (*(sp + 1) == sep))
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

int CountChar(const char *string, char sep)
{
    int count = 0;

    if (string == NULL)
    {
        return 0;
    }

    if (string && (strlen(string) == 0))
    {
        return 0;
    }

    for (const char *sp = string; *sp != '\0'; sp++)
    {
        if ((*sp == '\\') && (*(sp + 1) == sep))
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


char *EscapeCharCopy(const char *str, char to_escape, char escape_with)
/*
 * Escapes the 'to_escape'-chars found in str, by prefixing them with 'escape_with'.
 * Returns newly allocated string.
 */
{
    assert(str);

    int in_size = strlen(str);
    int out_size = in_size + CountChar(str, to_escape) + 1;

    char *out = xcalloc(1, out_size);

    const char *in_pos = str;
    char *out_pos = out;

    for(; *in_pos != '\0'; in_pos++, out_pos++)
    {
        if(*in_pos == to_escape)
        {
            *out_pos = escape_with;
            out_pos++;
        }
        *out_pos = *in_pos;
    }

    return out;
}
