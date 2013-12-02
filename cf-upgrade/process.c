/*
   Copyright (C) CFEngine AS

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

#include <alloc-mini.h>
#include <process.h>
#include <log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>
#include <time.h>
#include <libgen.h>

int run_process_finish(const char *command, char **args, char **envp)
{
    if (!command || !args || !envp)
    {
        return -1;
    }
    /* Execute the command */
    execve(command, args, envp);
    /* If we reach here, then we have already failed */
    log_entry(LogCritical, "Temporary copy failed to run, aborting");
    return -1;
}

int run_process_wait(const char *command, char **args, char **envp)
{
    if (!command || !args || !envp)
    {
        return RUN_PROCESS_FAILURE_VALUE;
    }
    /* Redirect the output */
    int fd = -1;
    char *filename = basename(xstrdup(command));
    time_t now_seconds = time(NULL);
    struct tm *now_tm = gmtime(&now_seconds);
    char *filenamelog = xmalloc(strlen(filename) +
                                strlen("-YYYYMMDD-HHMMSS") +
                                strlen(".log") + 1);
    sprintf(filenamelog, "%s-%04d%02d%02d-%02d%02d%02d.log", filename,
            now_tm->tm_year + 1900, now_tm->tm_mon, now_tm->tm_mday,
            now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec);
    fd = open(filenamelog, O_CREAT, S_IRWXU|S_IRGRP|S_IROTH);
    if (fd < 0)
    {
        return RUN_PROCESS_FAILURE_VALUE;
    }
    int exit_status = 0;
    pid_t child = fork();
    if (child < 0)
    {
        close (fd);
        unlink (filenamelog);
        log_entry(LogCritical, "Could not fork child process: %s", command);
        return RUN_PROCESS_FAILURE_VALUE;
    }
    else if (child == 0)
    {
        /* Child */
        dup2(fd, STDOUT_FILENO);
        /* Finally execute the command */
        execve(command, args, envp);
        /* If we reach here, the we failed */
        log_entry(LogCritical, "Could not execute helper process %s", command);
        exit(-1);
    }
    else
    {
        /* Parent */
        int status = -1;
        free(filenamelog);
        waitpid(child, &status, 0);
        if (WIFEXITED(status))
        {
            exit_status = WEXITSTATUS(status);
        }
        close (fd);
    }
    return exit_status;
}
