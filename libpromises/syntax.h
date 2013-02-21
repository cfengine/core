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

#include "sequence.h"
#include "writer.h"

#include <stdio.h>

typedef enum
{
    SYNTAX_TYPE_MATCH_OK,

    SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED,
    SYNTAX_TYPE_MATCH_ERROR_RANGE_BRACKETED,
    SYNTAX_TYPE_MATCH_ERROR_RANGE_MULTIPLE_ITEMS,
    SYNTAX_TYPE_MATCH_ERROR_GOT_SCALAR,
    SYNTAX_TYPE_MATCH_ERROR_GOT_LIST,

    SYNTAX_TYPE_MATCH_ERROR_SCALAR_OUT_OF_RANGE,

    SYNTAX_TYPE_MATCH_ERROR_STRING_UNIX_PERMISSION,

    SYNTAX_TYPE_MATCH_ERROR_INT_PARSE,
    SYNTAX_TYPE_MATCH_ERROR_INT_OUT_OF_RANGE,

    SYNTAX_TYPE_MATCH_ERROR_REAL_INF,
    SYNTAX_TYPE_MATCH_ERROR_REAL_OUT_OF_RANGE,

    SYNTAX_TYPE_MATCH_ERROR_OPTS_OUT_OF_RANGE,

    SYNTAX_TYPE_MATCH_ERROR_FNCALL_RETURN_TYPE,
    SYNTAX_TYPE_MATCH_ERROR_FNCALL_UNKNOWN,

    SYNTAX_TYPE_MATCH_ERROR_CONTEXT_OUT_OF_RANGE,

    SYNTAX_TYPE_MATCH_MAX
} SyntaxTypeMatch;

const char *SyntaxTypeMatchToString(SyntaxTypeMatch result);

int CheckParseVariableName(const char *name);
SyntaxTypeMatch CheckConstraintTypeMatch(const char *lval, Rval rval, DataType dt, const char *range, int level);
SyntaxTypeMatch CheckParseContext(const char *context, const char *range);
DataType StringDataType(const char *scopeid, const char *string);
DataType ExpectedDataType(const char *lvalname);
bool IsDataType(const char *s);
SubTypeSyntax SubTypeSyntaxLookup(const char *bundle_type, const char *subtype_name);

/* print a specification of the CFEngine language */
void SyntaxPrintAsJson(Writer *writer);

#endif
