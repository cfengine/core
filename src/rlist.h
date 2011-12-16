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

#ifndef CFENGINE_RLIST_H
#define CFENGINE_RLIST_H

#include "cf.defs.h"
#include "conf.h"

struct Rval
   {
   void *item;        /* (char *), (struct Rlist *), or (struct FnCall)  */
   char rtype;        /* Points to CF_SCALAR, CF_LIST, CF_FNCALL usually */
   };

struct Rlist
   {
   void *item;
   char type;
   struct Rlist *state_ptr; /* Points to "current" state/element of sub-list */
   struct Rlist *next;
   };

char *ScalarValue(struct Rlist *rlist);
struct FnCall *FnCallValue(struct Rlist *rlist);
struct Rlist *ListValue(struct Rlist *rlist);

int PrintRval(char *buffer,int bufsize, struct Rval rval);
int PrintRlist(char *buffer,int bufsize,struct Rlist *list);
struct Rlist *ParseShownRlist(char *string);
int IsStringIn(struct Rlist *list,char *s);
int IsIntIn(struct Rlist *list,int i);
struct Rlist *KeyInRlist(struct Rlist *list,char *key);
int RlistLen(struct Rlist *start);
void PopStack(struct Rlist **liststart, void **item,size_t size);
void PushStack(struct Rlist **liststart,void *item);
int IsInListOfRegex(struct Rlist *list,char *str);

struct Rval CopyRvalItem(struct Rval rval);
void DeleteRvalItem(struct Rval rval);
struct Rlist *CopyRlist(struct Rlist *list);
void DeleteRlist(struct Rlist *list);
void DeleteRlistEntry(struct Rlist **liststart,struct Rlist *entry);
struct Rlist *AppendRlistAlien(struct Rlist **start,void *item);
struct Rlist *PrependRlistAlien(struct Rlist **start,void *item);
struct Rlist *OrthogAppendRlist(struct Rlist **start,void *item, char type);
struct Rlist *IdempAppendRScalar(struct Rlist **start,void *item, char type);
struct Rlist *AppendRScalar(struct Rlist **start,void *item, char type);
struct Rlist *IdempAppendRlist(struct Rlist **start,void *item, char type);
struct Rlist *IdempPrependRScalar(struct Rlist **start,void *item, char type);
struct Rlist *PrependRScalar(struct Rlist **start,void *item, char type);
struct Rlist *PrependRlist(struct Rlist **start,void *item, char type);
struct Rlist *AppendRlist(struct Rlist **start,void *item, char type);
struct Rlist *PrependRlist(struct Rlist **start,void *item, char type);
struct Rlist *SplitStringAsRList(char *string,char sep);
struct Rlist *SplitRegexAsRList(char *string,char *regex,int max,int purge);
struct Rlist *SortRlist(struct Rlist *list, int (*CompareItems)());
struct Rlist *AlphaSortRListNames(struct Rlist *list);

struct Rlist *RlistAppendReference(struct Rlist **start,void *item, char type);

void ShowRlist(FILE *fp,struct Rlist *list);
void ShowRval(FILE *fp, struct Rval rval);

#endif
