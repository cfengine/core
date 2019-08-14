/*
   Copyright 2019 Northern.tech AS

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

#ifndef CFENGINE_STRING_LIB_H
#define CFENGINE_STRING_LIB_H

#include <platform.h>
#include <compiler.h>
#include <sequence.h>
#include <pcre.h>


typedef struct
{
    const char *data;
    size_t len;
} StringRef;


#define NULL_OR_EMPTY(str)                      \
    ((str == NULL) || (str[0] == '\0'))

#define NOT_NULL_AND_EMPTY(str)                 \
    ((str != NULL) && (str[0] == '\0'))

#define STARTSWITH(str,start)                   \
    (strncmp(str,start,strlen(start)) == 0)

#define SAFENULL(str)                           \
    (str != NULL ? str : "(null)")

#ifndef EMPTY_STRING_TO_NULL
#define EMPTY_STRING_TO_NULL(string) ((SafeStringLength(string) != 0)? string : NULL)
#endif
#ifndef NULL_TO_EMPTY_STRING
#define NULL_TO_EMPTY_STRING(string) (string? string : "")
#endif

#define STRINGIFY__INTERNAL_MACRO(x) #x
#define TOSTRING(x) STRINGIFY__INTERNAL_MACRO(x)

unsigned int StringHash        (const char *str, unsigned int seed);
unsigned int StringHash_untyped(const void *str, unsigned int seed);

char ToLower(char ch);
char ToUpper(char ch);
void ToUpperStrInplace(char *str);
void ToLowerStrInplace(char *str);

int StringToLong(const char *str, long *value_out) FUNC_WARN_UNUSED_RESULT;
void LogStringToLongError(const char *str_attempted, const char *id, int error_code);
long StringToLongDefaultOnError(const char *str, long default_return);
long StringToLongExitOnError(const char *str);
long StringToLongUnsafe(const char *str); // Deprecated, do not use

char *StringFromLong(long number);
double StringToDouble(const char *str);
char *StringFromDouble(double number);
char *NULLStringToEmpty(char *str);

bool StringIsNumeric(const char *name);
bool StringIsPrintable(const char *name);
bool EmptyString(const char *s);

size_t StringBytesToHex(char *dst, size_t dst_size,
                        const unsigned char *src_bytes, size_t src_len);

char *SafeStringDuplicate(const char *str);
char *SafeStringNDuplicate(const char *str, size_t size);
int SafeStringLength(const char *str);

int  StringSafeCompare(const char *a, const char *b);
bool StringSafeEqual  (const char *a, const char *b);

int  StringSafeCompareN(const char *a, const char *b, size_t n);
bool StringSafeEqualN  (const char *a, const char *b, size_t n);

int  StringSafeCompare_IgnoreCase(const char *a, const char *b);
bool StringSafeEqual_IgnoreCase  (const char *a, const char *b);

int  StringSafeCompareN_IgnoreCase(const char *a, const char *b, size_t n);
bool StringSafeEqualN_IgnoreCase  (const char *a, const char *b, size_t n);

bool StringSafeEqual_untyped(const void *a, const void *b);

char *StringConcatenate(size_t count, const char *first, ...);
char *StringSubstring(const char *source, size_t source_len, int start, int len);

/* Allocates the result */
char *SearchAndReplace(const char *source, const char *search, const char *replace);

ssize_t StringReplace(char *buf, size_t buf_size, const char *find, const char *replace);

bool IsStrIn(const char *str, const char *const strs[]);
bool IsStrCaseIn(const char *str, const char *const strs[]);

size_t StringCountTokens(const char *str, size_t len, const char *seps);
StringRef StringGetToken(const char *str, size_t len, size_t index, const char *seps);

char **String2StringArray(const char *str, char separator);
void FreeStringArray(char **strs);

int CountChar(const char *string, char sp);
void ReplaceChar(char *in, char *out, int outSz, char from, char to);
void ReplaceTrailingChar(char *str, char from, char to);
char *EscapeCharCopy(const char *str, char to_escape, char escape_with);

char *ScanPastChars(char *scanpast, char *input);

/**
 * @brief Strips the newline character off a string, in place
 * @param str The string to strip
 * @param max_length Maximum length of input string
 * @return 0 if successful, -1 if the input string was longer than allowed (max_length).
 */
int StripTrailingNewline(char *str, size_t max_length);

/**
 * @brief Remove trailing spaces
 * @param str
 * @param max_length Maximum length of input string
 * @return 0 if successful, -1 if Chop was called on a string that seemed to have no terminator
 */
int Chop(char *str, size_t max_length);

char *TrimWhitespace(char *s);
size_t TrimCSVLineCRLF(char *data);
size_t TrimCSVLineCRLFStrict(char *data);

/**
 * @brief Check if a string ends with the given suffix
 * @param str
 * @param suffix
 * @param case_fold whether the comparison is case-insensitive
 * @return True if suffix matches
 */
bool StringEndsWithCase(const char *str, const char *suffix, const bool case_fold);

/**
 * @brief Check if a string ends with the given suffix
 * @param str
 * @param suffix
 * @return True if suffix matches
 */
bool StringEndsWith(const char *str, const char *suffix);

/**
 * @brief Check if a string starts with the given prefix
 * @param str
 * @param prefix
 * @return True if prefix matches
 */
bool StringStartsWith(const char *str, const char *prefix);

/**
 * @brief Format string like vsprintf and return formatted string allocated
 * on heap as a return value.
 */
char *StringVFormat(const char *fmt, va_list ap);

/**
 * @brief Format string like sprintf and return formatted string allocated on
 * heap as a return value.
 *
 * @param format Formatting string

 * @return formatted string (on heap) or NULL in case of error. errno is set in
 * the latter case (see errno codes for sprintf).
 */
char *StringFormat(const char *fmt, ...) FUNC_ATTR_PRINTF(1, 2);

/**
 * @brief Copy a string from `from` to `to` (a buffer of at least buf_size)
 *
 * The destination (`to`) is guaranteed to be NUL terminated,
 * `to[buf_size-1]` will always be '\0'. If the string is shorter
 * the additional trailing bytes are also zeroed.
 *
 * The source (`from`) must either be NUL terminated or big enough
 * to read buf_size characters, even though only buf_size - 1 of them
 * end up in the output buffer.
 *
 * The return value is equal to `strlen(to)`, for large data sizes this
 * is useful since it doesn't require a second pass through the data.
 * The return value should be checked, if it is >= buf_size, it means
 * the string was truncated because it was too long. Expressed in another
 * way, the return value is the number of bytes read from the input (`from`)
 * excluding the terminating `\0` byte (up to buf_size characters will be
 * read).
 *
 * @note `from` and `to` must not overlap
 * @warning Regardless of `strlen(from)`, `to` must be at least `buf_size` big
 * @param[in] from String to copy from, up to buf_size bytes are read
 * @param[out] to Output buffer (minimum buf_size), always '\0' terminated
 * @param[in] buf_size Maximum buffer size to write (including '\0' byte)
 * @return String length of `to`, or `buf_size` in case of overflow
 */
size_t StringCopy(const char *from, char *to, size_t buf_size);

void *memcchr(const void *buf, int c, size_t buf_size);

bool StringNotMatchingSetCapped(const char *isp, int limit,
                      const char *exclude, char *obuf);

/**
 * @brief Appends src to dst, but will not exceed n bytes in dst, including the terminating null.
 * @param dst Destination string.
 * @param src Source string.
 * @param n Total size of dst buffer. The string will be truncated if this is exceeded.
 * @return True if append was successful, false if the operation caused an overflow.
 */
bool StringAppend(char *dst, const char *src, size_t n);

char *StringCanonify(char *dst, const char *src);
bool PathAppend(char *path, size_t path_size, const char *leaf, char sep);

void StrCat(char *dst, size_t dst_size, size_t *dst_len,
            const char *src, size_t src_len);
void StrCatDelim(char *dst, size_t dst_size, size_t *dst_len,
                 const char *src, char sep);

void CanonifyNameInPlace(char *str);

#endif
