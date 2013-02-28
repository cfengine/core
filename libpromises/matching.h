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

#ifndef CFENGINE_MATCHING_H
#define CFENGINE_MATCHING_H

#include "cf3.defs.h"

bool ValidateRegEx(const char *regex);
int FullTextMatch(const char *regptr, const char *cmpptr);
char *ExtractFirstReference(const char *regexp, const char *teststring);        /* Not thread-safe */
int BlockTextMatch(const char *regexp, const char *teststring, int *s, int *e);
int IsRegexItemIn(EvalContext *ctx, Item *list, char *regex);
int IsPathRegex(char *str);
int IsRegex(char *str);
int MatchRlistItem(Rlist *listofregex, const char *teststring);
void EscapeRegexChars(char *str, char *strEsc, int strEscSz);
void EscapeSpecialChars(char *str, char *strEsc, int strEscSz, char *noEscseq, char *noEsclist);
char *EscapeChar(char *str, int strSz, char esc);
void AnchorRegex(const char *regex, char *out, int outSz);
int MatchPolicy(const char *needle, const char *haystack, Attributes a, const Promise *pp);

#endif // MATCHING_H
