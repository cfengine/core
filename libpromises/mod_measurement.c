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

#include "mod_measurement.h"

#include "syntax.h"

static const ConstraintSyntax CF_MATCHVALUE_BODY[] =
{
    /* Row models */
    ConstraintSyntaxNewString("select_line_matching", CF_ANYSTRING, "Regular expression for matching line location", NULL),
    ConstraintSyntaxNewInt("select_line_number", CF_VALRANGE, "Read from the n-th line of the output (fixed format)", NULL),
    ConstraintSyntaxNewString("extraction_regex", "",
     "Regular expression that should contain a single backreference for extracting a value", NULL),
    ConstraintSyntaxNewBool("track_growing_file", "If true, cfengine remembers the position to which is last read when opening the file, and resets to the start if the file has since been truncated", NULL),
    ConstraintSyntaxNewOption("select_multiline_policy", "average,sum,first,last", "Regular expression for matching line location", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_MEASURE_BODIES[] =
{
    ConstraintSyntaxNewOption("stream_type", "pipe,file", "The datatype being collected.", NULL),
    ConstraintSyntaxNewOption("data_type", "counter,int,real,string,slist", "The datatype being collected.", NULL),
    ConstraintSyntaxNewOption("history_type", "weekly,scalar,static,log", "Whether the data can be seen as a time-series or just an isolated value", NULL),
    ConstraintSyntaxNewString("units", "", "The engineering dimensions of this value or a note about its intent used in plots", NULL),
    ConstraintSyntaxNewBody("match_value", CF_MATCHVALUE_BODY, "Criteria for extracting the measurement from a datastream"),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_MEASUREMENT_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("monitor", "measurements", ConstraintSetSyntaxNew(CF_MEASURE_BODIES, NULL)),
    PromiseTypeSyntaxNewNull()
};
