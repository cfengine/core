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

struct Constraint_
   {
   char *lval;
   Rval rval;
   char *classes; /* only used within bodies */
   int isbody;
   Audit *audit;
   Constraint *next;

   SourceOffset offset;
   };


Constraint *AppendConstraint(Constraint **conlist, char *lval, Rval rval, char *classes, int body);
Constraint *GetConstraint(Promise *promise, const char *lval);
void DeleteConstraintList(Constraint *conlist);
void EditScalarConstraint(Constraint *conlist,char *lval,char *rval);
void *GetConstraintValue(char *lval, Promise *promise, char type);
int GetBooleanConstraint(char *lval,Promise *list);
int GetRawBooleanConstraint(char *lval,Constraint *list);
int GetIntConstraint(char *lval,Promise *list);
double GetRealConstraint(char *lval,Promise *list);
mode_t GetOctalConstraint(char *lval,Promise *list);
uid_t GetUidConstraint(char *lval,Promise *pp);
gid_t GetGidConstraint(char *lval,Promise *pp);
Rlist *GetListConstraint(char *lval,Promise *list);
void ReCheckAllConstraints(Promise *pp);
int GetBundleConstraint(char *lval,Promise *list);
PromiseIdent *NewPromiseId(char *handle,Promise *pp);
void DeleteAllPromiseIds(void);


#endif
