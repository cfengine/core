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

typedef struct
   {
   void *item;        /* (char *), (Rlist *), or (FnCall)  */
   char rtype;        /* Points to CF_SCALAR, CF_LIST, CF_FNCALL usually */
   } Rval;

typedef struct Rlist_ Rlist;

struct Rlist_
   {
   void *item;
   char type;
   Rlist *state_ptr; /* Points to "current" state/element of sub-list */
   Rlist *next;
   };

#include "cf.defs.h"
#include "conf.h"
#include "writer.h"

struct FnCall_;

char *ScalarValue(Rlist *rlist);
struct FnCall_ *FnCallValue(Rlist *rlist);
Rlist *ListValue(Rlist *rlist);

char *ScalarRvalValue(Rval rval);
struct FnCall_ *FnCallRvalValue(Rval rval);
Rlist *ListRvalValue(Rval rval);

int PrintRval(char *buffer,int bufsize, Rval rval);
int PrintRlist(char *buffer,int bufsize,Rlist *list);
Rlist *ParseShownRlist(char *string);
int IsStringIn(Rlist *list,char *s);
int IsIntIn(Rlist *list,int i);
Rlist *KeyInRlist(Rlist *list,char *key);
int RlistLen(Rlist *start);
void PopStack(Rlist **liststart, void **item,size_t size);
void PushStack(Rlist **liststart,void *item);
int IsInListOfRegex(Rlist *list,char *str);

Rval CopyRvalItem(Rval rval);
void DeleteRvalItem(Rval rval);
Rlist *CopyRlist(Rlist *list);
void DeleteRlist(Rlist *list);
void DeleteRlistEntry(Rlist **liststart,Rlist *entry);
Rlist *AppendRlistAlien(Rlist **start,void *item);
Rlist *PrependRlistAlien(Rlist **start,void *item);
Rlist *OrthogAppendRlist(Rlist **start,void *item, char type);
Rlist *IdempAppendRScalar(Rlist **start,void *item, char type);
Rlist *AppendRScalar(Rlist **start,void *item, char type);
Rlist *IdempAppendRlist(Rlist **start,void *item, char type);
Rlist *IdempPrependRScalar(Rlist **start,void *item, char type);
Rlist *PrependRScalar(Rlist **start,void *item, char type);
Rlist *PrependRlist(Rlist **start,void *item, char type);
Rlist *AppendRlist(Rlist **start,void *item, char type);
Rlist *PrependRlist(Rlist **start,void *item, char type);
Rlist *SplitStringAsRList(char *string,char sep);
Rlist *SplitRegexAsRList(char *string,char *regex,int max,int purge);
Rlist *SortRlist(Rlist *list, int (*CompareItems)());
Rlist *AlphaSortRListNames(Rlist *list);

Rlist *RlistAppendReference(Rlist **start,void *item, char type);

void ShowRlist(FILE *fp,Rlist *list);
void ShowRval(FILE *fp, Rval rval);

void RvalPrint(Writer *writer, Rval rval);

Rlist *RlistAt(Rlist *start, size_t index);

char *GetRlistScalar(Rlist *rp);

#endif
