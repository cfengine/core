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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_STRING_LIB_H
#define CFENGINE_STRING_LIB_H

#include "compiler.h"

char ToLower(char ch);
char ToUpper(char ch);
void ToUpperStrInplace(char *str);
void ToLowerStrInplace(char *str);

long StringToLong(const char *str);
char *StringFromLong(long number);
double StringToDouble(const char *str);
char *NULLStringToEmpty(char *str);

bool IsNumber(const char *name);
bool EmptyString(const char *s);

char *StringEncodeBase64(const char *str, size_t len);

char *SafeStringDuplicate(const char *str);
int SafeStringLength(const char *str);
int StringSafeCompare(const char *a, const char *b);
bool StringSafeEqual(const char *a, const char *b);

char *StringConcatenate(size_t count, const char *first, ...);
char *StringSubstring(const char *source, size_t source_len, int start, int len);

/* Allocates the result */
char *SearchAndReplace(const char *source, const char *search, const char *replace);

bool StringMatch(const char *regex, const char *str);
bool StringMatchFull(const char *regex, const char *str);

int ReplaceStr(char *in, char *out, int outSz, char *from, char *to);

bool IsStrIn(const char *str, const char **strs);
bool IsStrCaseIn(const char *str, const char **strs);

char **String2StringArray(char *str, char separator);
void FreeStringArray(char **strs);

int CountChar(const char *string, char sp);
void ReplaceChar(char *in, char *out, int outSz, char from, char to);
void ReplaceTrailingChar(char *str, char from, char to);
char *EscapeCharCopy(const char *str, char to_escape, char escape_with);

int StringInArray(char **array, char *string);
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
 * @brief Find the initial segment of memory (up to #n bytes) consisting of character #c.
 *
 * @return first byte which is not #c, or #mem + #n if all bytes in memory segment are #c
 */
void *MemSpan(const void *mem, char c, size_t n);

/**
 * @brief Find the initial segment of memory (up to #n bytes) consisting not of character #c.
 *
 * @return first byte which is #c, or #mem + #n if none of bytes in memory segment are #c
 */
void *MemSpanInverse(const void *mem, char c, size_t n);

bool CompareStringOrRegex(const char *value, const char *compareTo, bool regex);
bool StringNotMatchingSetCapped(const char *isp, int limit, 
                      const char *exclude, char *obuf);

#endif
