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

char ToLower(char ch);
char ToUpper(char ch);
char *ToUpperStr(const char *str);
void ToUpperStrInplace(char *str);
char *ToLowerStr(const char *str);
void ToLowerStrInplace(char *str);

long StringToLong(const char *str);
char *NULLStringToEmpty(char *str);

bool IsNumber(const char *name);

char *SafeStringDuplicate(const char *str);
int SafeStringLength(const char *str);
int StringSafeCompare(const char *a, const char *b);
bool StringSafeEqual(const char *a, const char *b);

char *StringConcatenate(const char *a, size_t a_len, const char *b, size_t b_len);
char *StringSubstring(const char *source, size_t source_len, int start, int len);

int GetStringListElement(char *strList, int index, char *outBuf, int outBufSz);
int StripListSep(char *strList, char *outBuf, int outBufSz);

/* Allocates the result */
char *SearchAndReplace(const char *source, const char *search, const char *replace);

bool StringMatch(const char *regex, const char *str);
bool StringMatchFull(const char *regex, const char *str);

#endif
