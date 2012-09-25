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

#ifndef CFENGINE_SCOPE_H
#define CFENGINE_SCOPE_H

#include "cf3.defs.h"

void SetScope(char *id);
void SetNewScope(char *id);
void NewScope(const char *name);
void DeleteScope(char *name);
Scope *GetScope(const char *scope);
void CopyScope(const char *new_scopename, const char *old_scopename);
void DeleteAllScope(void);
void AugmentScope(char *scope, char *ns, Rlist *lvals, Rlist *rvals);
void DeleteFromScope(char *scope, Rlist *args);
void PushThisScope(void);
void PopThisScope(void);
void ShowScope(char *);

void SplitScopeName(const char *scope_name, char namespace_out[CF_MAXVARSIZE], char bundle_out[CF_MAXVARSIZE]);
void JoinScopeName(const char *ns, const char *bundle, char scope_out[CF_MAXVARSIZE]);

#endif
