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

#ifndef CFENGINE_PARSER_STATE_H
#define CFENGINE_PARSER_STATE_H

#include "cf3.defs.h"
#include "rlist.h"

extern struct ParserState P;

struct ParserState
{
    char *block;                // body/bundle
    char blocktype[CF_MAXVARSIZE];
    char blockid[CF_MAXVARSIZE];

    char filename[CF_MAXVARSIZE];
    int line_pos;
    int line_no;

    int arg_nesting;
    int list_nesting;

    char lval[CF_MAXVARSIZE];
    Rval rval;
    bool references_body;

    char *promiser;
    void *promisee;

    char currentid[CF_MAXVARSIZE];
    char currenttype[CF_MAXVARSIZE];
    char *currentstring;
    char *currentclasses;

    Policy *policy;

    Bundle *currentbundle;
    Body *currentbody;
    Promise *currentpromise;
    SubType *currentstype;
    Rlist *useargs;

    Rlist *currentRlist;

    char *currentfnid[CF_MAX_NESTING];
    Rlist *giveargs[CF_MAX_NESTING];
    FnCall *currentfncall[CF_MAX_NESTING];

    struct OffsetState
    {
        size_t current;
        size_t last_id;
        size_t last_string;
        size_t last_block_id;
        size_t last_subtype_id;
        size_t last_class_id;
    } offsets;
};

#endif
