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

#include "mod_report.h"

#include "syntax.h"

static const ConstraintSyntax printfile_constraints[] =
{
    ConstraintSyntaxNewString("file_to_print", CF_ABSPATHRANGE, "Path name to the file that is to be sent to standard output", NULL),
    ConstraintSyntaxNewInt("number_of_lines", CF_VALRANGE, "Integer maximum number of lines to print from selected file", NULL),
    ConstraintSyntaxNewNull()
};

static const BodyTypeSyntax printfile_body = BodyTypeSyntaxNew("printfile", printfile_constraints, NULL);

const const ConstraintSyntax CF_REPORT_BODIES[] =
{
    ConstraintSyntaxNewString("friend_pattern", "", "Regular expression to keep selected hosts from the friends report list", NULL),
    ConstraintSyntaxNewReal("intermittency", "0,1", "Real number threshold [0,1] of intermittency about current peers, report above", "false"),
    ConstraintSyntaxNewInt("lastseen", CF_VALRANGE, "Integer time threshold in hours since current peers were last seen, report absence", NULL),
    ConstraintSyntaxNewBody("printfile", &printfile_body, "Quote part of a file to standard output", NULL),
    ConstraintSyntaxNewString("report_to_file", CF_ABSPATHRANGE, "The path and filename to which output should be appended", NULL),
    ConstraintSyntaxNewString("bundle_return_value_index", CF_IDRANGE, "The promiser is to be interpreted as a literal value that the caller can accept as a result for this bundle, i.e. a return value with array index defined by this attribute.", NULL),
    ConstraintSyntaxNewStringList("showstate", "", "List of services about which status reports should be reported to standard output"),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_REPORT_PROMISE_TYPES[] =
{
    /* Body lists belonging to "reports:" type in Agent */

    PromiseTypeSyntaxNew("agent", "reports", CF_REPORT_BODIES, NULL),
    PromiseTypeSyntaxNewNull()
};
