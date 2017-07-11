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

#ifndef PROCESS_H
#define PROCESS_H

#define RUN_PROCESS_FAILURE_VALUE   -10
#define RUN_PROCESS_FAILURE(x) \
    (x == RUN_PROCESS_FAILURE_VALUE) ? true : false;
/**
  @brief Runs the specified process by replacing the current one.
  @param command Full path of the command to run.
  @param args Arguments for the program
  @param envp Environment to use.
  @return The exit status of the process or a negative value in case of error.
  @remarks This function does not return, unless there was an error.
  */
int run_process_replace(const char *command, char **args, char **envp);
/**
  @brief Runs the specified process and redirects the output to a file, waiting
  for the process to terminate.
  @param command Full path of the command to run.
  @param args Arguments for the program
  @param envp Environment to use.
  @return The exit status of the process or a negative value in case of error.
  @remarks Use RUN_PROCESS_FAILURE with the return value of this method to
  detect if the error was caused by the process or the function itself.
  */
int run_process_wait(const char *command, char **args, char **envp);

#endif // PROCESS_H
