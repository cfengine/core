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

#ifndef CFENGINE_RLIST_H
#define CFENGINE_RLIST_H

#include "cf3.defs.h"
#include "writer.h"
#include "json.h"

struct Rlist_
{
    void *item;
    RvalType type;
    Rlist *state_ptr;           /* Points to "current" state/element of sub-list */
    Rlist *next;
};

RvalType DataTypeToRvalType(DataType datatype);

char *RvalScalarValue(Rval rval);
FnCall *RvalFnCallValue(Rval rval);
Rlist *RvalRlistValue(Rval rval);
Rval RvalCopy(Rval rval);
void RvalDestroy(Rval rval);
JsonElement *RvalToJson(Rval rval);
void RvalShow(FILE *fp, Rval rval);
void RvalWrite(Writer *writer, Rval rval);

void RlistPrintToWriter(const Rlist *list, Writer *w);
void RvalPrintToWriter(Rval rval, Writer *w);

Rlist *RlistCopy(const Rlist *list);
void RlistDestroy(Rlist *list);
void RlistDestroyEntry(Rlist **liststart, Rlist *entry);
char *RlistScalarValue(const Rlist *rlist);
FnCall *RlistFnCallValue(const Rlist *rlist);
Rlist *RlistRlistValue(const Rlist *rlist);
Rlist *RlistParseShown(char *string);
Rlist *RlistParseString(char *string, int *n);
bool RlistIsStringIn(const Rlist *list, const char *s);
bool RlistIsIntIn(const Rlist *list, int i);
Rlist *RlistKeyIn(Rlist *list, const char *key);
int RlistLen(const Rlist *start);
bool RlistIsInListOfRegex(const Rlist *list, const char *str);

Rlist *RlistAppendAlien(Rlist **start, void *item);
Rlist *RlistPrependAlien(Rlist **start, void *item);

Rlist *RlistPrependScalarIdemp(Rlist **start, const char *scalar);
Rlist *RlistPrependScalar(Rlist **start, const char *scalar);
Rlist *RlistAppendScalarIdemp(Rlist **start, const char *scalar);
Rlist *RlistAppendScalar(Rlist **start, const char *scalar);

Rlist *RlistAppendIdemp(Rlist **start, void *item, RvalType type);
Rlist *RlistPrepend(Rlist **start, const void *item, RvalType type);
Rlist *RlistAppend(Rlist **start, const void *item, RvalType type);

Rlist *RlistAppendFnCall(Rlist **start, const FnCall *fn);

Rlist *RlistFromSplitString(const char *string, char sep);
Rlist *RlistFromSplitRegex(const char *string, const char *regex, int max, int purge);
void RlistShow(FILE *fp, const Rlist *list);
void RlistWrite(Writer *writer, const Rlist *list);
Rlist *RlistLast(Rlist *start);
void RlistFilter(Rlist **list, bool (*KeepPredicate)(void *item, void *predicate_data), void *predicate_user_data, void (*DestroyItem)(void *item));
void RlistReverse(Rlist **list);

/**
 * @brief Flattens an Rlist by expanding naked scalar list-variable members. Flattening is only one-level deep.
 */
void RlistFlatten(EvalContext *ctx, Rlist **list);

#endif
