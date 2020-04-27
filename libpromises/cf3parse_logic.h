/*
  Copyright 2020 Northern.tech AS

  This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

/*
  This file is an attempt to clean up cf3parse.y, moving as much
  C code (logic) as possible out of it, so the actual grammar
  is more readable. It should only be included from cf3parse.y (!).

  The advantages of moving the C code out of the grammar are:
  * Separate overall grammar from noisy details
  * Less crazy indentation
  * Better editor support for auto complete / syntax highlighting
*/

#ifndef CF3_PARSE_LOGIC_H
#define CF3_PARSE_LOGIC_H

#include "cf3.defs.h"
#include "parser.h"
#include "parser_state.h"

#include "logging.h"
#include "fncall.h"
#include "rlist.h"
#include "item_lib.h"
#include "policy.h"
#include "mod_files.h"
#include "string_lib.h"
#include "logic_expressions.h"
#include "json-yaml.h"
#include "cleanup.h"

// FIX: remove
#include "syntax.h"

#include <assert.h>

int yylex(void);
extern char *yytext;

static int RelevantBundle(const char *agent, const char *blocktype);
static bool LvalWantsBody(char *stype, char *lval);
static SyntaxTypeMatch CheckSelection(
    const char *type, const char *name, const char *lval, Rval rval);
static SyntaxTypeMatch CheckConstraint(
    const char *type,
    const char *lval,
    Rval rval,
    const PromiseTypeSyntax *ss);
static void fatal_yyerror(const char *s);

static void ParseErrorColumnOffset(int column_offset, const char *s, ...)
    FUNC_ATTR_PRINTF(2, 3);
static void ParseError(const char *s, ...) FUNC_ATTR_PRINTF(1, 2);
static void ParseWarning(unsigned int warning, const char *s, ...)
    FUNC_ATTR_PRINTF(2, 3);

static void ValidateClassLiteral(const char *class_literal);

static bool INSTALL_SKIP = false;
static size_t CURRENT_BLOCKID_LINE = 0;
static size_t CURRENT_PROMISER_LINE = 0;

#define YYMALLOC xmalloc
#define P PARSER_STATE

#define ParserDebug(...) LogDebug(LOG_MOD_PARSER, __VA_ARGS__)

#endif // CF3_PARSE_LOGIC_H
