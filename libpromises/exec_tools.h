/*
  Copyright 2024 Northern.tech AS

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

#ifndef CFENGINE_EXEC_TOOLS_H
#define CFENGINE_EXEC_TOOLS_H

#include <platform.h>
#include <cf3.defs.h>
#include <pipes.h> // OutputSelect

bool IsExecutable(const char *file);
bool ShellCommandReturnsZero(const char *command, ShellType shell);
bool GetExecOutput(const char *command, char **buffer, size_t *buffer_size, ShellType shell, OutputSelect output_select, int *ret_out);
void ActAsDaemon();
void ArgGetExecutableAndArgs(const char *comm, char **exec, char **args);

/**
 * Create a argument list as expected by execv(3)
 * @param command is split on spaces, unless quoted
 * @param arglist is not split (can have embedded spaces)
 * @return null-terminated argument list
 */
char **ArgSplitCommand(const char *command, const Seq *arglist);

void ArgFree(char **args);

#endif
