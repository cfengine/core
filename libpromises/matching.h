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

#ifndef CFENGINE_MATCHING_H
#define CFENGINE_MATCHING_H

#include "cf3.defs.h"

int FullTextMatch(const char *regptr, const char *cmpptr); /* Sets variables */
int BlockTextMatch(const char *regexp, const char *teststring, int *s, int *e); /* Sets variables */
int IsRegexItemIn(const EvalContext *ctx, Item *list, char *regex); /* Uses context, sets variables */
int MatchRlistItem(Rlist *listofregex, const char *teststring); /* Sets variables */
int MatchPolicy(const char *needle, const char *haystack, Rlist *insert_match, const Promise *pp); /* Sets variables */

char *ExtractFirstReference(const char *regexp, const char *teststring); /* Pure, not thread-safe */

bool ValidateRegEx(const char *regex); /* Pure */
int IsPathRegex(char *str); /* Pure */
int IsRegex(char *str); /* Pure */
void EscapeRegexChars(char *str, char *strEsc, int strEscSz); /* Pure */
void EscapeSpecialChars(char *str, char *strEsc, int strEscSz, char *noEscseq, char *noEsclist); /* Pure */
char *EscapeChar(char *str, int strSz, char esc); /* Pure */
void AnchorRegex(const char *regex, char *out, int outSz); /* Pure */

#endif // MATCHING_H
