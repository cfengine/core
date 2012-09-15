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

#ifndef CFENGINE_STRING_LIB_H
#define CFENGINE_STRING_LIB_H

#include "compiler.h"

char ToLower(char ch);
char ToUpper(char ch);
char *ToUpperStr(const char *str) FUNC_DEPRECATED;
void ToUpperStrInplace(char *str);
char *ToLowerStr(const char *str) FUNC_DEPRECATED;
void ToLowerStrInplace(char *str);

long StringToLong(const char *str);
char *StringFromLong(long number);
double StringToDouble(const char *str);
char *NULLStringToEmpty(char *str);

bool IsNumber(const char *name);

char *StringEncodeBase64(const char *str, size_t len);

char *SafeStringDuplicate(const char *str);
int SafeStringLength(const char *str);
int StringSafeCompare(const char *a, const char *b);
bool StringSafeEqual(const char *a, const char *b);

char *StringConcatenate(size_t count, const char *first, ...);
char *StringSubstring(const char *source, size_t source_len, int start, int len);

int GetStringListElement(char *strList, int index, char *outBuf, int outBufSz);
int StripListSep(char *strList, char *outBuf, int outBufSz);

/* Allocates the result */
char *SearchAndReplace(const char *source, const char *search, const char *replace);

bool StringMatch(const char *regex, const char *str);
bool StringMatchFull(const char *regex, const char *str);

int ReplaceStr(char *in, char *out, int outSz, char *from, char *to);

bool IsStrIn(const char *str, const char **strs);
bool IsStrCaseIn(const char *str, const char **strs);

char **String2StringArray(char *str, char separator);
void FreeStringArray(char **strs);

char *Titleize(char *str);

int SubStrnCopyChr(char *to, const char *from, int len, char sep);
int CountChar(const char *string, char sp);
void ReplaceChar(char *in, char *out, int outSz, char from, char to);
void ReplaceTrailingChar(char *str, char from, char to);
void ReplaceTrailingStr(char *str, char *from, char to);
char *EscapeCharCopy(const char *str, char to_escape, char escape_with);

#endif
