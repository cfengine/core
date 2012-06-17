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

#ifndef CFENGINE_SYNTAX_H
#define CFENGINE_SYNTAX_H

#include "cf3.defs.h"
#include "writer.h"
#include <stdio.h>

int LvalWantsBody(char *stype, char *lval);
int CheckParseVariableName(char *name);
void CheckConstraint(char *type, char *ns, char *name, char *lval, Rval rval, SubTypeSyntax ss);
void CheckSelection(char *type, char *name, char *lval, Rval rval);
void CheckConstraintTypeMatch(const char *lval, Rval rval, enum cfdatatype dt, const char *range, int level);
int CheckParseClass(const char *lv, const char *s, const char *range);
enum cfdatatype StringDataType(const char *scopeid, const char *string);
enum cfdatatype ExpectedDataType(char *lvalname);
bool IsDataType(const char *s);
SubTypeSyntax SubTypeSyntaxLookup(const char *bundle_type, const char *subtype_name);

/* print a specification of the CFEngine language */
void SyntaxPrintAsJson(Writer *writer);

/* print a parse tree of the given policy (bundles, bodies) */
void PolicyPrintAsJson(Writer *writer, const char *filename, Bundle *bundles, Body *bodies);

/* print language elements using official formatting */
void BodyPrettyPrint(Writer *writer, Body *body);
void BundlePrettyPrint(Writer *writer, Bundle *bundle);

#endif
