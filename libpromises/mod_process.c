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

#include "mod_process.h"

#include "syntax.h"

static const ConstraintSyntax CF_MATCHCLASS_BODY[] =
{
    ConstraintSyntaxNewStringList("in_range_define", "", "List of classes to define if the matches are in range"),
    ConstraintSyntaxNewIntRange("match_range", CF_VALRANGE, "Integer range for acceptable number of matches for this process", NULL),
    ConstraintSyntaxNewStringList("out_of_range_define", "", "List of classes to define if the matches are out of range"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_PROCFILTER_BODY[] =
{
    ConstraintSyntaxNewString("command", "", "Regular expression matching the command/cmd field of a process", NULL),
    ConstraintSyntaxNewIntRange("pid", CF_VALRANGE, "Range of integers matching the process id of a process", NULL),
    ConstraintSyntaxNewIntRange("pgid", CF_VALRANGE, "Range of integers matching the parent group id of a process", NULL),
    ConstraintSyntaxNewIntRange("ppid", CF_VALRANGE, "Range of integers matching the parent process id of a process", NULL),
    ConstraintSyntaxNewIntRange("priority", "-20,+20", "Range of integers matching the priority field (PRI/NI) of a process", NULL),
    ConstraintSyntaxNewStringList("process_owner", "", "List of regexes matching the user of a process"),
    ConstraintSyntaxNewString("process_result",
     "[(process_owner|pid|ppid||pgid|rsize|vsize|status|command|ttime|stime|tty|priority|threads)[|&!.]*]*",
     "Boolean class expression returning the logical combination of classes set by a process selection test", NULL),
    ConstraintSyntaxNewIntRange("rsize", CF_VALRANGE, "Range of integers matching the resident memory size of a process, in kilobytes", NULL),
    ConstraintSyntaxNewString("status", "", "Regular expression matching the status field of a process", NULL),
    ConstraintSyntaxNewIntRange("stime_range", CF_TIMERANGE, "Range of integers matching the start time of a process", NULL),
    ConstraintSyntaxNewIntRange("ttime_range", CF_TIMERANGE, "Range of integers matching the total elapsed time of a process", NULL),
    ConstraintSyntaxNewString("tty", "", "Regular expression matching the tty field of a process", NULL),
    ConstraintSyntaxNewIntRange("threads", CF_VALRANGE, "Range of integers matching the threads (NLWP) field of a process", NULL),
    ConstraintSyntaxNewIntRange("vsize", CF_VALRANGE, "Range of integers matching the virtual memory size of a process, in kilobytes", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_PROCESS_BODIES[] =
{
    ConstraintSyntaxNewBody("process_count", CF_MATCHCLASS_BODY, "Criteria for constraining the number of processes matching other criteria"),
    ConstraintSyntaxNewBody("process_select", CF_PROCFILTER_BODY, "Criteria for matching processes in the system process table"),
    ConstraintSyntaxNewString("process_stop", CF_ABSPATHRANGE, "A command used to stop a running process", NULL),
    ConstraintSyntaxNewString("restart_class", CF_IDRANGE,
     "A class to be defined globally if the process is not running, so that a command: rule can be referred to restart the process", NULL),
    ConstraintSyntaxNewOptionList("signals", CF_SIGNALRANGE, "A list of menu options representing signals to be sent to a process"),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_PROCESS_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "processes", ConstraintSetSyntaxNew(CF_PROCESS_BODIES, NULL)),
    PromiseTypeSyntaxNewNull()
};
