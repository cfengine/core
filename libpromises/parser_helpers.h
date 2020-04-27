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

// This header contains types and (inline) functions used
// when parsing policy. It is used in a few files related
// to parsing, for example cf3lex.l (flex generated lexer),
// cf3parse.y (yacc generated parser) and syntax.c
// It doesn't really contain the grammar, or syntax
// description just some helper functions, to enable those.
// The intention is to move a lot of logic (C code) out of
// the lex and yacc files to make them easier to understand
// and maintain.

// It could maybe be combined with parser_state.h, but
// there is a distinction, this file does not include anything
// about the state of the parser

#ifndef CF_PARSER_HELPERS_H
#define CF_PARSER_HELPERS_H

#include <condition_macros.h> // debug_abort_if_reached()

// Blocks are the top level elements of a policy file
// (excluding macros). 

// Currently there are 2 types of blocks; bundle and body
typedef enum
{
  PARSER_BLOCK_BUNDLE = 1,
  PARSER_BLOCK_BODY = 2,
} ParserBlock;

static inline const char *ParserBlockString(ParserBlock b)
{
    switch (b)
    {
    case PARSER_BLOCK_BUNDLE:
      return "bundle";
    case PARSER_BLOCK_BODY:
      return "body";
    default:
      break;
    }
    debug_abort_if_reached();
    return "ERROR";
}

#endif // CF_PARSER_HELPERS_H
