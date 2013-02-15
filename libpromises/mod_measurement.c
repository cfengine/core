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

#include "cf3.defs.h"

static const BodySyntax CF_MATCHVALUE_BODY[] =
{
    /* Row models */
    {"select_line_matching", DATA_TYPE_STRING, CF_ANYSTRING, "Regular expression for matching line location"},
    {"select_line_number", DATA_TYPE_INT, CF_VALRANGE, "Read from the n-th line of the output (fixed format)"},
    {"extraction_regex", DATA_TYPE_STRING, "",
     "Regular expression that should contain a single backreference for extracting a value"},
    {"track_growing_file", DATA_TYPE_OPTION, CF_BOOL,
     "If true, cfengine remembers the position to which is last read when opening the file, and resets to the start if the file has since been truncated"},
    {"select_multiline_policy", DATA_TYPE_OPTION, "average,sum,first,last", "Regular expression for matching line location"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

static const BodySyntax CF_MEASURE_BODIES[] =
{
    {"stream_type", DATA_TYPE_OPTION, "pipe,file", "The datatype being collected."},
    {"data_type", DATA_TYPE_OPTION, "counter,int,real,string,slist", "The datatype being collected."},
    {"history_type", DATA_TYPE_OPTION, "weekly,scalar,static,log",
     "Whether the data can be seen as a time-series or just an isolated value"},
    {"units", DATA_TYPE_STRING, "", "The engineering dimensions of this value or a note about its intent used in plots"},
    {"match_value", DATA_TYPE_BODY, CF_MATCHVALUE_BODY, "Criteria for extracting the measurement from a datastream"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const SubTypeSyntax CF_MEASUREMENT_SUBTYPES[] =
{
    {"monitor", "measurements", CF_MEASURE_BODIES},
    {NULL, NULL, NULL},
};
