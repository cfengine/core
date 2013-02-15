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
#include "mod_process.h"

static const BodySyntax CF_MATCHCLASS_BODY[] =
{
    {"in_range_define", DATA_TYPE_STRING_LIST, "", "List of classes to define if the matches are in range"},
    {"match_range", DATA_TYPE_INT_RANGE, CF_VALRANGE, "Integer range for acceptable number of matches for this process"},
    {"out_of_range_define", DATA_TYPE_STRING_LIST, "", "List of classes to define if the matches are out of range"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

static const BodySyntax CF_PROCFILTER_BODY[] =
{
    {"command", DATA_TYPE_STRING, "", "Regular expression matching the command/cmd field of a process"},
    {"pid", DATA_TYPE_INT_RANGE, CF_VALRANGE, "Range of integers matching the process id of a process"},
    {"pgid", DATA_TYPE_INT_RANGE, CF_VALRANGE, "Range of integers matching the parent group id of a process"},
    {"ppid", DATA_TYPE_INT_RANGE, CF_VALRANGE, "Range of integers matching the parent process id of a process"},
    {"priority", DATA_TYPE_INT_RANGE, "-20,+20", "Range of integers matching the priority field (PRI/NI) of a process"},
    {"process_owner", DATA_TYPE_STRING_LIST, "", "List of regexes matching the user of a process"},
    {"process_result", DATA_TYPE_STRING,
     "[(process_owner|pid|ppid||pgid|rsize|vsize|status|command|ttime|stime|tty|priority|threads)[|&!.]*]*",
     "Boolean class expression returning the logical combination of classes set by a process selection test"},
    {"rsize", DATA_TYPE_INT_RANGE, CF_VALRANGE, "Range of integers matching the resident memory size of a process, in kilobytes"},
    {"status", DATA_TYPE_STRING, "", "Regular expression matching the status field of a process"},
    {"stime_range", DATA_TYPE_INT_RANGE, CF_TIMERANGE, "Range of integers matching the start time of a process"},
    {"ttime_range", DATA_TYPE_INT_RANGE, CF_TIMERANGE, "Range of integers matching the total elapsed time of a process"},
    {"tty", DATA_TYPE_STRING, "", "Regular expression matching the tty field of a process"},
    {"threads", DATA_TYPE_INT_RANGE, CF_VALRANGE, "Range of integers matching the threads (NLWP) field of a process"},
    {"vsize", DATA_TYPE_INT_RANGE, CF_VALRANGE, "Range of integers matching the virtual memory size of a process, in kilobytes"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

static const BodySyntax CF_PROCESS_BODIES[] =
{
    {"process_count", DATA_TYPE_BODY, CF_MATCHCLASS_BODY,
     "Criteria for constraining the number of processes matching other criteria"},
    {"process_select", DATA_TYPE_BODY, CF_PROCFILTER_BODY, "Criteria for matching processes in the system process table"},
    {"process_stop", DATA_TYPE_STRING, CF_ABSPATHRANGE, "A command used to stop a running process"},
    {"restart_class", DATA_TYPE_STRING, CF_IDRANGE,
     "A class to be defined globally if the process is not running, so that a command: rule can be referred to restart the process"},
    {"signals", DATA_TYPE_OPTION_LIST, CF_SIGNALRANGE, "A list of menu options representing signals to be sent to a process"},
    {NULL, DATA_TYPE_NONE, NULL, NULL}
};

const SubTypeSyntax CF_PROCESS_SUBTYPES[] =
{
    {"agent", "processes", CF_PROCESS_BODIES},
    {NULL, NULL, NULL},
};
