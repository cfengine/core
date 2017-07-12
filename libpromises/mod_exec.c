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

#include <mod_exec.h>

#include <syntax.h>

static const ConstraintSyntax contain_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewOption("useshell", "noshell,useshell,powershell," CF_BOOL, "noshell/useshell/powershell embed the command in the given shell environment. Default value: noshell", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("umask", "", "The umask value for the child process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("exec_owner", "", "The user name or id under which to run the process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("exec_group", "", "The group name or id under which to run the process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("exec_timeout", "1,3600", "Timeout in seconds for command completion", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("chdir", CF_ABSPATHRANGE, "Directory for setting current/base directory for the process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("chroot", CF_ABSPATHRANGE, "Directory of root sandbox for process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("preview", "true/false preview command when running in dry-run mode (with -n). Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("no_output", "true/false discard all output from the command. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax contain_body = BodySyntaxNew("contain", contain_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax commands_constraints[] =
{
    ConstraintSyntaxNewString("args", "", "Alternative string of arguments for the command (concatenated with promiser string and 'arglist' attribute)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("arglist", CF_ANYSTRING, "Alternative string list of arguments for the command (concatenated with promiser string and 'args' attribute)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("contain", &contain_body, "Containment options for the execution process", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("module", "true/false whether to expect the cfengine module protocol. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_EXEC_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "commands", commands_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
