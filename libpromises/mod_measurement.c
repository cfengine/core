/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

#include <mod_measurement.h>

#include <syntax.h>

static const ConstraintSyntax match_value_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,

    /* Row models */
    ConstraintSyntaxNewString("select_line_matching", CF_ANYSTRING, "Regular expression for matching line location", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("select_line_number", CF_VALRANGE, "Read from the n-th line of the output (fixed format)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("extraction_regex", "", "Regular expression that should contain a single backreference for extracting a value", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("track_growing_file", "If true, cfengine remembers the position to which is last read when opening the file, and resets to the start if the file has since been truncated", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("select_multiline_policy", "average,sum,first,last", "Regular expression for matching line location", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax match_value_body = BodySyntaxNew("match_value", match_value_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax CF_MEASURE_BODIES[] =
{
    ConstraintSyntaxNewOption("stream_type", "pipe,file", "The datatype being collected.", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("data_type", "counter,int,real,string,slist", "The datatype being collected.", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("history_type", "weekly,scalar,static,log", "Whether the data can be seen as a time-series or just an isolated value", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("units", "", "The engineering dimensions of this value or a note about its intent used in plots", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("match_value", &match_value_body, "Criteria for extracting the measurement from a datastream", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_MEASUREMENT_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("monitor", "measurements", CF_MEASURE_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
