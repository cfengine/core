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

#include "mod_exec.h"

#include "syntax.h"

static const ConstraintSyntax CF_EXECCONTAIN_BODY[] =
{
    ConstraintSyntaxNewBool("useshell", "true/false embed the command in a shell environment", "false"),
    ConstraintSyntaxNewOption("umask", "0,77,22,27,72,077,022,027,072", "The umask value for the child process", NULL),
    ConstraintSyntaxNewString("exec_owner", "", "The user name or id under which to run the process", NULL),
    ConstraintSyntaxNewString("exec_group", "", "The group name or id under which to run the process", NULL),
    ConstraintSyntaxNewInt("exec_timeout", "1,3600", "Timeout in seconds for command completion", NULL),
    ConstraintSyntaxNewString("chdir", CF_ABSPATHRANGE, "Directory for setting current/base directory for the process", NULL),
    ConstraintSyntaxNewString("chroot", CF_ABSPATHRANGE, "Directory of root sandbox for process", NULL),
    ConstraintSyntaxNewBool("preview", "true/false preview command when running in dry-run mode (with -n)", "false"),
    ConstraintSyntaxNewBool("no_output", "true/false discard all output from the command", "false"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_EXEC_BODIES[] =
{
    ConstraintSyntaxNewString("args", "", "Alternative string of arguments for the command (concatenated with promiser string)", NULL),
    ConstraintSyntaxNewBody("contain", CF_EXECCONTAIN_BODY, "Containment options for the execution process"),
    ConstraintSyntaxNewBool("module", "true/false whether to expect the cfengine module protocol", "false"),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_EXEC_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "commands", ConstraintSetSyntaxNew(CF_EXEC_BODIES, NULL)),
    PromiseTypeSyntaxNewNull(),
};
