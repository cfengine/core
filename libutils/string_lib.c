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

#include <platform.h>
#include <string_lib.h>

#include <openssl/buffer.h>                                 /* BUF_MEM */
#include <openssl/bio.h>                                    /* BIO_* */
#include <openssl/evp.h>                                    /* BIO_f_base64 */

#include <alloc.h>
#include <writer.h>
#include <misc_lib.h>
#include <logging.h>

char *StringVFormat(const char *fmt, va_list ap)
{
    char *value;
    int ret = xvasprintf(&value, fmt, ap);
    if (ret < 0)
    {
        return NULL;
    }
    else
    {
        return value;
    }
}

char *StringFormat(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *res = StringVFormat(fmt, ap);
    va_end(ap);
    return res;
}

unsigned int StringHash(const char *str, unsigned int seed, unsigned int max)
{
    unsigned const char *p = str;
    unsigned int h = seed;
    size_t len = strlen(str);

    for (size_t i = 0; i < len; i++)
    {
        h += p[i];
        h += (h << 10);
        h ^= (h >> 6);
    }

    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);

    return (h & (max - 1));
}


char ToLower(char ch)
{
    if (isupper((unsigned char) ch))
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
    if ((isdigit((unsigned char) ch)) || (ispunct((unsigned char) ch)))
    {
        return (ch);
    }

    if (isupper((unsigned char) ch))
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

void ToLowerStrInplace(char *str)
{
    for (; *str != '\0'; str++)
    {
        *str = ToLower(*str);
    }
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

char *SafeStringNDuplicate(const char *str, size_t size)
{
    if (str == NULL)
    {
        return NULL;
    }

    return xstrndup(str, size);
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
    if (a && b)
    {
        return strcmp(a, b);
    }
    if (a == NULL)
    {
        return -1;
    }
    assert(b == NULL);
    return +1;
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

/*********************************************************************/

char *SearchAndReplace(const char *source, const char *search, const char *replace)
{
    const char *source_ptr = source;

    if ((source == NULL) || (search == NULL) || (replace == NULL))
    {
        ProgrammingError("Programming error: NULL argument is passed to SearchAndReplace");
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

/**
   @TODO create a new one for already allocated arrays:
       bool StringConcat(dest, dest_size, ...)
   where the end of the va_args list is signified by a NULL sentinel element.
 */

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

    memcpy(result, source + start, end - start + 1);
    return result;
}

/*********************************************************************/

bool StringIsNumeric(const char *s)
{
    for (; *s; s++)
    {
        if (!isdigit((unsigned char)*s))
        {
            return false;
        }
    }

    return true;
}

bool StringIsPrintable(const char *s)
{
    for (; *s; s++)
    {
        if (!isprint((unsigned char)*s))
        {
            return false;
        }
    }

    return true;
}

bool EmptyString(const char *s)
{
    const char *sp;

    for (sp = s; *sp != '\0'; sp++)
    {
        if (!isspace((unsigned char)*sp))
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

char *StringFromDouble(double number)
{
    return StringFormat("%.2f", number);
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

/**
 * @NOTE this function always '\0'-terminates the destination string #dst.
 * @return length of written string #dst.
 */
size_t StringBytesToHex(char *dst, size_t dst_size,
                        const unsigned char *src_bytes, size_t src_len)
{
    static const char *const hex_chars = "0123456789abcdef";

    size_t i = 0;
    while ((i < src_len) &&
           (i*2 + 2 < dst_size))               /* room for 2 more hex chars */
    {
        dst[2*i]     = hex_chars[(src_bytes[i] >> 4) & 0xf];
        dst[2*i + 1] = hex_chars[src_bytes[i] & 0xf];
        i++;
    }

    assert(2*i < dst_size);
    dst[2*i] = '\0';

    return 2*i;
}

bool IsStrIn(const char *str, const char *const strs[])
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

bool IsStrCaseIn(const char *str, const char *const strs[])
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
/* Replaces all occurrences of 'from' to 'to' in preallocated
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

/* TODO replace with StringReplace. This one is pretty slow, calls strncmp
 * O(n) times even if string matches nowhere. */
bool ReplaceStr(const char *in, char *out, int outSz, const char *from, const char *to)
/* Replaces all occurrences of strings 'from' to 'to' in preallocated
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

static StringRef StringRefNull(void)
{
    return (StringRef) { .data = NULL, .len = 0 };
}

size_t StringCountTokens(const char *str, size_t len, const char *seps)
{
    size_t num_tokens = 0;
    bool in_token = false;

    for (size_t i = 0; i < len; i++)
    {
        if (strchr(seps, str[i]))
        {
            in_token = false;
        }
        else
        {
            if (!in_token)
            {
                num_tokens++;
            }
            in_token = true;
        }
    }

    return num_tokens;
}

static StringRef StringNextToken(const char *str, size_t len, const char *seps)
{
    size_t start = 0;
    bool found = false;
    for (size_t i = 0; i < len; i++)
    {
        if (strchr(seps, str[i]))
        {
            if (found)
            {
                assert(i > 0);
                return (StringRef) { .data = str + start, .len = i - start };
            }
        }
        else
        {
            if (!found)
            {
                found = true;
                start = i;
            }
        }
    }

    if (found)
    {
        return (StringRef) { .data = str + start, .len = len - start };
    }
    else
    {
        return StringRefNull();
    }
}

StringRef StringGetToken(const char *str, size_t len, size_t index, const char *seps)
{
    StringRef ref = StringNextToken(str, len, seps);
    for (size_t i = 0; i < index; i++)
    {
        if (!ref.data)
        {
            return ref;
        }

        len = len - (ref.data - str + ref.len);
        str = ref.data + ref.len;

        ref = StringNextToken(str, len, seps);
    }

    return ref;
}

char **String2StringArray(const char *str, char separator)
/**
 * Parse CSVs into char **.
 * MEMORY NOTE: Caller must free return value with FreeStringArray().
 **/
{
    int i = 0, len;

    if (str == NULL)
    {
        return NULL;
    }

    for (const char *sp = str; *sp != '\0'; sp++)
    {
        if (*sp == separator)
        {
            i++;
        }
    }

    char **arr = (char **) xcalloc(i + 2, sizeof(char *));

    const char *sp = str;
    i = 0;

    while (sp)
    {
        const char *esp = strchr(sp, separator);

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
        memcpy(arr[i], sp, len);

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
        strs[i] = NULL;
    }

    free(strs);
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

int StringInArray(char **array, char *string)
{
    for (int i = 0; array[i] != NULL; i++)
    {
        if (strcmp(string, array[i]) == 0)
        {
            return true;
        }
    }

    return false;
}

char *ScanPastChars(char *scanpast, char *input)
{
    char *pos = input;

    while ((*pos != '\0') && (strchr(scanpast, *pos)))
    {
        pos++;
    }

    return pos;
}

int StripTrailingNewline(char *str, size_t max_length)
{
    if (str)
    {
        size_t i = strnlen(str, max_length + 1);
        if (i > max_length) /* See off-by-one comment below */
        {
            return -1;
        }

        while (i > 0 && str[i - 1] == '\n')
        {
            i--;
        }
        assert(str[i] == '\0' || str[i] == '\n');
        str[i] = '\0';
    }
    return 0;
}

/**
 * Removes trailing whitespace from a string.
 *
 * @WARNING Off-by-one quirk in max_length.
 *
 * Both StripTrailngNewline() and Chop() have long allowed callers to
 * pass (effectively) the strlen(str) rather than the size of memory
 * (which is at least strlen() + 1) in the buffer.  The present
 * incarnation thus reads max_length as max-strlen(), not as size of
 * the buffer.  It may be sensible to review all callers and change so
 * that max_length is the buffer size instead.
 *
 * TODO change Chop() to accept str_len parameter. It goes without saying that
 * the callers should call it as:    Chop(s, strlen(s));
 */

int Chop(char *str, size_t max_length)
{
    if (str)
    {
        size_t i = strnlen(str, max_length + 1);
        if (i > max_length) /* See off-by-one comment above */
        {
            /* Given that many callers don't even check Chop's return value,
             * we should NULL-terminate the string here. TODO. */
            return -1;
        }

        while (i > 0 && isspace((unsigned char)str[i-1]))
        {
            i--;
        }
        assert(str[i] == '\0' || isspace((unsigned char)str[i]));
        str[i] = '\0';
    }
    return 0;
}

bool StringEndsWithCase(const char *str, const char *suffix, const bool case_fold)
{
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len)
    {
        return false;
    }

    for (size_t i = 0; i < suffix_len; i++)
    {
        char a = str[str_len - i - 1];
        char b = suffix[suffix_len - i - 1];
        if (case_fold)
        {
            a = ToLower(a);
            b = ToLower(b);
        }

        if (a != b)
        {
            return false;
        }
    }

    return true;
}

bool StringEndsWith(const char *str, const char *suffix)
{
    return StringEndsWithCase(str, suffix, false);
}

bool StringStartsWith(const char *str, const char *prefix)
{
    int str_len = strlen(str);
    int prefix_len = strlen(prefix);

    if (prefix_len > str_len)
    {
        return false;
    }

    for (int i = 0; i < prefix_len; i++)
    {
        if (str[i] != prefix[i])
        {
            return false;
        }
    }
    return true;
}

void *MemSpan(const void *mem, char c, size_t n)
{
    const char *end = mem + n;
    for (; (char*)mem < end; ++mem)
    {
        if (*((char *)mem) != c)
        {
            return (char *)mem;
        }
    }

    return (char *)mem;
}

void *MemSpanInverse(const void *mem, char c, size_t n)
{
    const char *end = mem + n;
    for (; (char*)mem < end; ++mem)
    {
        if (*((char*)mem) == c)
        {
            return (char *)mem;
        }
    }

    return (char *)mem;
}

/*
 * @brief extract info from input string given two types of constraints:
 *        - length of the extracted string is bounded
 *        - extracted string should stop at first element of an exclude list
 *
 * @param[in] isp     : the string to scan
 * @param[in] limit   : size limit on the output string (including '\0')
 * @param[in] exclude : characters to be excluded from output buffer
 * @param[out] obuf   : the output buffer
 * @retval    true if string was capped, false if not
 */
bool StringNotMatchingSetCapped(const char *isp, int limit,
                      const char *exclude, char *obuf)
{
    size_t l = strcspn(isp, exclude);

    if (l < limit-1)
    {
        memcpy(obuf, isp, l);
        obuf[l]='\0';
        return false;
    }
    else
    {
        memcpy(obuf, isp, limit-1);
        obuf[limit-1]='\0';
        return true;
    }
}

bool StringAppend(char *dst, const char *src, size_t n)
{
    int i, j;
    n--;
    for (i = 0; i < n && dst[i]; i++)
    {
    }
    for (j = 0; i < n && src[j]; i++, j++)
    {
        dst[i] = src[j];
    }
    dst[i] = '\0';
    return (i < n || !src[j]);
}

bool StringAppendPromise(char *dst, const char *src, size_t n)
{
    int i, j;
    n--;
    for (i = 0; i < n && dst[i]; i++)
    {
    }
    for (j = 0; i < n && src[j]; i++, j++)
    {
        const char ch = src[j];
        switch (ch)
        {
        case '*':
            dst[i] = ':';
            break;

        case '#':
            dst[i] = '.';
            break;

        default:
            dst[i] = ch;
            break;
        }
    }
    dst[i] = '\0';
    return (i < n || !src[j]);
}

bool StringAppendAbbreviatedPromise(char *dst, const char *src, size_t n, const size_t max_fragment)
{
    /* check if `src` contains a new line (may happen for "insert_lines") */
    const char *const nl = strchr(src, '\n');
    if (NULL == nl)
    {
        return StringAppendPromise(dst, src, n);
    }
    else
    {
        /* `src` contains a newline: abbreviate it by taking the first and last few characters */
        static const char sep[] = "...";
        char abbr[sizeof(sep) + 2 * max_fragment];
        const int head = (nl > src + max_fragment) ? max_fragment : (nl - src);
        const char * last_line = strrchr(src, '\n') + 1;
        assert(last_line); /* not max_fragmentULL, we know we have at least one '\n' */
        const int tail = strlen(last_line);
        if (tail > max_fragment)
        {
            last_line += tail - max_fragment;
        }
        memcpy(abbr, src, head);
        strcpy(abbr + head, sep);
        strcat(abbr, last_line);
        return StringAppendPromise(dst, abbr, n);
    }
}

/**
 *  Canonify #src into #dst.
 *
 *  @WARNING you must make sure sizeof(dst) is adequate!
 *
 *  @return always #dst.
 */
char *StringCanonify(char *dst, const char *src)
{
    while (*src != '\0')
    {
        if (isalnum((unsigned char) *src))
        {
            *dst = *src;
        }
        else
        {
            *dst = '_';
        }

        src++;
        dst++;
    }
    *dst = '\0';

    return dst;
}

/**
 * Append #src to #dst, delimiting with #sep if not at the beginning.
 *
 * @param #dst_len In-out parameter. If not NULL it is taken into account as
 *                 the current length of #dst, and updated as such.
 *
 * @retval false if it doesn't fit, in which case nothing gets appended and
 *               #dst remains as is.
 */
bool StringAppendDelimited(char *dst, size_t *dst_len, size_t dst_size,
                           const char *src, char sep)
{
    size_t len     = (dst_len != NULL) ? *dst_len : strlen(dst);
    size_t src_len = strlen(src);

    if (len + src_len + 1 >= dst_size)
    {
        return false;
    }

    /* Append a separator if dst is not empty. */
    if (len > 0)
    {
        dst[len] = sep;
        len++;
    }

    memcpy(&dst[len], src, src_len);
    len += src_len;
    dst[len] = '\0';

    if (dst_len != NULL)
    {
        *dst_len = len;
    }

    return true;
}

/**
 *  Append #sep if not already there, and then append #leaf.
 */
bool PathAppend(char *path, size_t path_size, const char *leaf, char sep)
{
    size_t path_len = strlen(path);
    size_t leaf_len = strlen(leaf);

    if (path_len + 1 + leaf_len >= path_size)
    {
        return false;
    }

    path[path_len] = sep;
    memcpy(&path[path_len + 1], leaf, leaf_len + 1);

    return true;
}
