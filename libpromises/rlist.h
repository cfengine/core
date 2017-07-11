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

#ifndef CFENGINE_RLIST_H
#define CFENGINE_RLIST_H

#include <cf3.defs.h>
#include <writer.h>
#include <json.h>

struct Rlist_
{
    Rval val;
    Rlist *next;
};

RvalType DataTypeToRvalType(DataType datatype);

char *RvalScalarValue(Rval rval);
FnCall *RvalFnCallValue(Rval rval);
Rlist *RvalRlistValue(Rval rval);
JsonElement *RvalContainerValue(Rval rval);

const char *RvalTypeToString(RvalType type);

Rval RvalNew(const void *item, RvalType type);
Rval RvalCopy(Rval rval);
void RvalDestroy(Rval rval);
JsonElement *RvalToJson(Rval rval);
char *RvalToString(Rval rval);
void RvalWrite(Writer *writer, Rval rval);
void RvalWriteParts(Writer *writer, const void* item, RvalType type);
unsigned RvalHash(Rval rval, unsigned seed, unsigned max);

Rlist *RlistCopy(const Rlist *list);
unsigned int RlistHash        (const Rlist *list, unsigned seed, unsigned max);
unsigned int RlistHash_untyped(const void *list, unsigned seed, unsigned max);
void RlistDestroy        (Rlist *list);
void RlistDestroy_untyped(void *rl);
void RlistDestroyEntry(Rlist **liststart, Rlist *entry);
char *RlistScalarValue(const Rlist *rlist);
FnCall *RlistFnCallValue(const Rlist *rlist);
Rlist *RlistRlistValue(const Rlist *rlist);
Rlist *RlistParseShown(const char *string);
Rlist *RlistParseString(const char *string);
Rlist *RlistKeyIn(Rlist *list, const char *key);
int RlistLen(const Rlist *start);
bool RlistMatchesRegexRlist(const Rlist *list, const Rlist *search);
bool RlistMatchesRegex(const Rlist *list, const char *str);
bool RlistIsInListOfRegex(const Rlist *list, const char *str);
bool RlistIsNullList(const Rlist *list);

Rlist *RlistAppendRval(Rlist **start, Rval rval);

Rlist *RlistPrependScalarIdemp(Rlist **start, const char *scalar);
Rlist *RlistAppendScalarIdemp(Rlist **start, const char *scalar);
Rlist *RlistAppendScalar(Rlist **start, const char *scalar);

Rlist *RlistPrepend(Rlist **start, const void *item, RvalType type);
Rlist *RlistAppend(Rlist **start, const void *item, RvalType type);

Rlist *RlistFromSplitString(const char *string, char sep);
Rlist *RlistFromSplitRegex(const char *string, const char *regex, size_t max_entries, bool allow_blanks);
Rlist *RlistFromRegexSplitNoOverflow(const char *string, const char *regex, int max);
void RlistWrite(Writer *writer, const Rlist *list);
Rlist *RlistLast(Rlist *start);
void RlistFilter(Rlist **list, bool (*KeepPredicate)(void *item, void *predicate_data), void *predicate_user_data, void (*DestroyItem)(void *item));
void RlistReverse(Rlist **list);
void RlistFlatten(EvalContext *ctx, Rlist **list);
bool RlistEqual        (const Rlist *list1, const Rlist *list2);
bool RlistEqual_untyped(const void *list1, const void *list2);


#endif
