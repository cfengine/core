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

#ifndef CFENGINE_CONSTRAINTS_H
#define CFENGINE_CONSTRAINTS_H

#include "cf3.defs.h"

struct Constraint
   {
   char *lval;
   void *rval;    /* should point to either string, Rlist or FnCall */
   char type;     /* scalar, list, or function */
   char *classes; /* only used within bodies */
   int isbody;
   struct Audit *audit;
   struct Constraint *next;

   struct SourceOffset offset;
   };


struct Constraint *AppendConstraint(struct Constraint **conlist,char *lval, void *rval, char type,char *classes,int body);
struct Constraint *GetConstraint(struct Promise *promise, const char *lval);
void DeleteConstraintList(struct Constraint *conlist);
void EditScalarConstraint(struct Constraint *conlist,char *lval,char *rval);
void *GetConstraintValue(char *lval, struct Promise *promise, char type);
int GetBooleanConstraint(char *lval,struct Promise *list);
int GetRawBooleanConstraint(char *lval,struct Constraint *list);
int GetIntConstraint(char *lval,struct Promise *list);
double GetRealConstraint(char *lval,struct Promise *list);
mode_t GetOctalConstraint(char *lval,struct Promise *list);
uid_t GetUidConstraint(char *lval,struct Promise *pp);
gid_t GetGidConstraint(char *lval,struct Promise *pp);
struct Rlist *GetListConstraint(char *lval,struct Promise *list);
void ReCheckAllConstraints(struct Promise *pp);
int GetBundleConstraint(char *lval,struct Promise *list);
struct PromiseIdent *NewPromiseId(char *handle,struct Promise *pp);
void DeleteAllPromiseIds(void);


#endif
