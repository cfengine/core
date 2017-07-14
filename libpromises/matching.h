/*
   Copyright 2017 Northern.tech AS

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

#ifndef CFENGINE_MATCHING_H
#define CFENGINE_MATCHING_H

#include <cf3.defs.h>

int IsRegex(const char *str); /* Pure */
int IsRegexItemIn(const EvalContext *ctx, const Item *list, const char *regex); /* Uses context */

char *ExtractFirstReference(const char *regexp, const char *teststring); /* Pure, not thread-safe */

int IsPathRegex(const char *str); /* Pure */
bool HasRegexMetaChars(const char *string);
void EscapeRegexChars(char *str, char *strEsc, int strEscSz); /* Pure */
void EscapeSpecialChars(const char *str, char *strEsc, int strEscSz, char *noEscseq, char *noEsclist); /* Pure */
size_t EscapeRegexCharsLen(const char *str); /* Pure */
char *EscapeChar(char *str, int strSz, char esc); /* Pure */
void AnchorRegex(const char *regex, char *out, int outSz); /* Pure */

/**
   result is malloced
 */
char *AnchorRegexNew(const char *regex);

#endif // MATCHING_H
