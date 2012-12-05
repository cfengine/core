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

#ifndef CFENGINE_EXPAND_H
#define CFENGINE_EXPAND_H

#include "cf3.defs.h"

#include "reporting.h"

void ExpandPromise(AgentType ag, const char *scopeid, Promise *pp, void *fnptr, const ReportContext *report_context);
void ExpandPromiseAndDo(AgentType ag, const char *scope, Promise *p, Rlist *scalarvars, Rlist *listvars,
                        void (*fnptr) (), const ReportContext *report_context);
Rval ExpandDanglers(const char *scope, Rval rval, const Promise *pp);
void MapIteratorsFromRval(const char *scope, Rlist **los, Rlist **lol, Rval rval, const Promise *pp);

int IsExpandable(const char *str);
int ExpandScalar(const char *string, char buffer[CF_EXPANDSIZE]);
Rval ExpandBundleReference(const char *scopeid, Rval rval);
FnCall *ExpandFnCall(const char *contextid, FnCall *f, int expandnaked);
Rval ExpandPrivateRval(const char *contextid, Rval rval);
Rlist *ExpandList(const char *scopeid, const Rlist *list, int expandnaked);
Rval EvaluateFinalRval(const char *scopeid, Rval rval, int forcelist, const Promise *pp);
int IsNakedVar(const char *str, char vtype);
void GetNaked(char *s1, const char *s2);
void ConvergeVarHashPromise(char *scope, const Promise *pp, int checkdup);
int ExpandPrivateScalar(const char *contextid, const char *string, char buffer[CF_EXPANDSIZE]);

#endif
