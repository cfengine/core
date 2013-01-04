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

#ifndef CFENGINE_UNIX_H
#define CFENGINE_UNIX_H

#include "cf3.defs.h"

#include "assoc.h"

void ProcessSignalTerminate(pid_t pid);

/*
 * Do not modify returned Rval, its contents may be constant and statically
 * allocated.
 */
enum cfdatatype GetVariable(const char *scope, const char *lval, Rval *returnv);

void DeleteVariable(const char *scope, const char *id);
bool StringContainsVar(const char *s, const char *v);
int DefinedVariable(char *name);
int IsCf3VarString(const char *str);
int BooleanControl(const char *scope, const char *name);
const char *ExtractInnerCf3VarString(const char *str, char *substr);
const char *ExtractOuterCf3VarString(const char *str, char *substr);
int UnresolvedVariables(CfAssoc *ap, char rtype);
int UnresolvedArgs(Rlist *args);
int IsQualifiedVariable(char *var);
int IsCfList(char *type);
int AddVariableHash(const char *scope, const char *lval, Rval rval, enum cfdatatype dtype, const char *fname, int no);
void DeRefListsInHashtable(char *scope, Rlist *list, Rlist *reflist);

#endif
