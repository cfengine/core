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
#include "policy.h"

struct Constraint_
{
    PolicyElementType type;
    union {
        Promise *promise;
        Body *body;
    } parent;

    char *lval;
    Rval rval;

    char *classes;              /* only used within bodies */
    bool references_body;
    Audit *audit;

    SourceOffset offset;
    Constraint *next;
};

Constraint *ConstraintAppendToPromise(Promise *promise, const char *lval, Rval rval, const char *classes, bool references_body);
Constraint *ConstraintAppendToBody(Body *body, const char *lval, Rval rval, const char *classes, bool references_body);

Constraint *GetConstraint(const Promise *promise, const char *lval);
void DeleteConstraintList(Constraint *conlist);
void EditScalarConstraint(Constraint *conlist, const char *lval, const char *rval);
void *GetConstraintValue(const char *lval, const Promise *promise, char type);
int GetBooleanConstraint(const char *lval, const Promise *list);
int GetRawBooleanConstraint(const char *lval, const Constraint *list);
int GetIntConstraint(const char *lval, const Promise *list);
double GetRealConstraint(const char *lval, const Promise *list);
mode_t GetOctalConstraint(const char *lval, const Promise *list);
uid_t GetUidConstraint(const char *lval, const Promise *pp);
gid_t GetGidConstraint(char *lval, const Promise *pp);
Rlist *GetListConstraint(const char *lval, const Promise *list);
void ReCheckAllConstraints(Promise *pp);
int GetBundleConstraint(const char *lval, const Promise *list);
PromiseIdent *NewPromiseId(char *handle, Promise *pp);
void DeleteAllPromiseIds(void);

#endif
