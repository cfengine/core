/*
   Copyright 2017 Northern.tech AS

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

#include <mod_process.h>

#include <syntax.h>

static const ConstraintSyntax process_count_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewStringList("in_range_define", "", "List of classes to define if the matches are in range", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("match_range", CF_VALRANGE, "Integer range for acceptable number of matches for this process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("out_of_range_define", "", "List of classes to define if the matches are out of range", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax process_count_body = BodySyntaxNew("process_count", process_count_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax process_select_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewString("command", "", "Regular expression matching the command/cmd field of a process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("pid", CF_VALRANGE, "Range of integers matching the process id of a process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("pgid", CF_VALRANGE, "Range of integers matching the parent group id of a process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("ppid", CF_VALRANGE, "Range of integers matching the parent process id of a process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("priority", "-20,+20", "Range of integers matching the priority field (PRI/NI) of a process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("process_owner", "", "List of regexes matching the user of a process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("process_result",
     "[(process_owner|pid|ppid||pgid|rsize|vsize|status|command|ttime|stime|tty|priority|threads)[|&!.]*]*",
     "Boolean class expression returning the logical combination of classes set by a process selection test", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("rsize", CF_VALRANGE, "Range of integers matching the resident memory size of a process, in kilobytes", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("status", "", "Regular expression matching the status field of a process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("stime_range", CF_TIMERANGE, "Range of integers matching the start time of a process", SYNTAX_STATUS_NORMAL),
    /* CF_VALRANGE insted of CF_TIMERANGE since ttime_range counts cumulative
     * time, not absolute time since the epoch. See cumulative() that has
     * maximum number of years 1000, that can easily surpass CF_TIMERANGE in
     * seconds. */
    ConstraintSyntaxNewIntRange("ttime_range", CF_VALRANGE, "Range of integers matching the total elapsed time of a process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("tty", "", "Regular expression matching the tty field of a process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("threads", CF_VALRANGE, "Range of integers matching the threads (NLWP) field of a process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("vsize", CF_VALRANGE, "Range of integers matching the virtual memory size of a process, in kilobytes", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax process_select_body = BodySyntaxNew("process_select", process_select_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax processes_constraints[] =
{
    ConstraintSyntaxNewBody("process_count", &process_count_body, "Criteria for constraining the number of processes matching other criteria", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("process_select", &process_select_body, "Criteria for matching processes in the system process table", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("process_stop", CF_ABSPATHRANGE, "A command used to stop a running process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("restart_class", CF_IDRANGE,
     "A class to be defined globally if the process is not running, so that a command: rule can be referred to restart the process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOptionList("signals", CF_SIGNALRANGE, "A list of menu options representing signals to be sent to a process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_PROCESS_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "processes", processes_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
