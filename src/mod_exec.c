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
#include "mod_exec.h"

static const BodySyntax CF_EXECCONTAIN_BODY[] =
{
    {"useshell", cf_opts, CF_BOOL, "true/false embed the command in a shell environment", "false"},
    {"umask", cf_opts, "0,77,22,27,72,077,022,027,072", "The umask value for the child process"},
    {"exec_owner", cf_str, "", "The user name or id under which to run the process"},
    {"exec_group", cf_str, "", "The group name or id under which to run the process"},
    {"exec_timeout", cf_int, "1,3600", "Timeout in seconds for command completion"},
    {"chdir", cf_str, CF_ABSPATHRANGE, "Directory for setting current/base directory for the process"},
    {"chroot", cf_str, CF_ABSPATHRANGE, "Directory of root sandbox for process"},
    {"preview", cf_opts, CF_BOOL, "true/false preview command when running in dry-run mode (with -n)", "false"},
    {"no_output", cf_opts, CF_BOOL, "true/false discard all output from the command", "false"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_EXEC_BODIES[] =
{
    {"args", cf_str, "", "Alternative string of arguments for the command (concatenated with promiser string)"},
    {"contain", cf_body, CF_EXECCONTAIN_BODY, "Containment options for the execution process"},
    {"module", cf_opts, CF_BOOL, "true/false whether to expect the cfengine module protocol", "false"},
    {NULL, cf_notype, NULL, NULL}
};

const SubTypeSyntax CF_EXEC_SUBTYPES[] =
{
    {"agent", "commands", CF_EXEC_BODIES},
    {NULL, NULL, NULL},
};
