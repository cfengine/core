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
    {"in_range_define", cf_slist, "", "List of classes to define if the matches are in range"},
    {"match_range", cf_irange, CF_VALRANGE, "Integer range for acceptable number of matches for this process"},
    {"out_of_range_define", cf_slist, "", "List of classes to define if the matches are out of range"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_PROCFILTER_BODY[] =
{
    {"command", cf_str, "", "Regular expression matching the command/cmd field of a process"},
    {"pid", cf_irange, CF_VALRANGE, "Range of integers matching the process id of a process"},
    {"pgid", cf_irange, CF_VALRANGE, "Range of integers matching the parent group id of a process"},
    {"ppid", cf_irange, CF_VALRANGE, "Range of integers matching the parent process id of a process"},
    {"priority", cf_irange, "-20,+20", "Range of integers matching the priority field (PRI/NI) of a process"},
    {"process_owner", cf_slist, "", "List of regexes matching the user of a process"},
    {"process_result", cf_str,
     "[(process_owner|pid|ppid||pgid|rsize|vsize|status|command|ttime|stime|tty|priority|threads)[|&!.]*]*",
     "Boolean class expression returning the logical combination of classes set by a process selection test"},
    {"rsize", cf_irange, CF_VALRANGE, "Range of integers matching the resident memory size of a process, in kilobytes"},
    {"status", cf_str, "", "Regular expression matching the status field of a process"},
    {"stime_range", cf_irange, CF_TIMERANGE, "Range of integers matching the start time of a process"},
    {"ttime_range", cf_irange, CF_TIMERANGE, "Range of integers matching the total elapsed time of a process"},
    {"tty", cf_str, "", "Regular expression matching the tty field of a process"},
    {"threads", cf_irange, CF_VALRANGE, "Range of integers matching the threads (NLWP) field of a process"},
    {"vsize", cf_irange, CF_VALRANGE, "Range of integers matching the virtual memory size of a process, in kilobytes"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_PROCESS_BODIES[] =
{
    {"process_count", cf_body, CF_MATCHCLASS_BODY,
     "Criteria for constraining the number of processes matching other criteria"},
    {"process_select", cf_body, CF_PROCFILTER_BODY, "Criteria for matching processes in the system process table"},
    {"process_stop", cf_str, CF_ABSPATHRANGE, "A command used to stop a running process"},
    {"restart_class", cf_str, CF_IDRANGE,
     "A class to be defined globally if the process is not running, so that a command: rule can be referred to restart the process"},
    {"signals", cf_olist, CF_SIGNALRANGE, "A list of menu options representing signals to be sent to a process"},
    {NULL, cf_notype, NULL, NULL}
};

const SubTypeSyntax CF_PROCESS_SUBTYPES[] =
{
    {"agent", "processes", CF_PROCESS_BODIES},
    {NULL, NULL, NULL},
};
