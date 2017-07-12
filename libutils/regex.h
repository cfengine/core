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


#ifndef CFENGINE_REGEX_H
#define CFENGINE_REGEX_H


#include <platform.h>

#include <pcre_include.h>

#include <sequence.h>                                           /* Seq */

#define CFENGINE_REGEX_WHITESPACE_IN_CONTEXTS ".*[_A-Za-z0-9][ \\t]+[_A-Za-z0-9].*"

/* Try to use CompileRegex() and StringMatchWithPrecompiledRegex(). */
pcre *CompileRegex(const char *regex);
bool StringMatch(const char *regex, const char *str, int *start, int *end);
bool StringMatchWithPrecompiledRegex(pcre *regex, const char *str,
                                     int *start, int *end);
bool StringMatchFull(const char *regex, const char *str);
bool StringMatchFullWithPrecompiledRegex(pcre *regex, const char *str);
Seq *StringMatchCaptures(const char *regex, const char *str, const bool return_names);
Seq *StringMatchCapturesWithPrecompiledRegex(const pcre *pattern, const char *str, const bool return_names);
bool CompareStringOrRegex(const char *value, const char *compareTo, bool regex);

/* Does not free rx! */
bool RegexPartialMatch(const pcre *rx, const char *teststring);

#endif  /* CFENGINE_REGEX_H */
