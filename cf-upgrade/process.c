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

#include <platform.h>
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
#ifndef __MINGW32__
#include <sys/wait.h>
#endif // __MINGW32__ This covers both 32 and 64 bits MinGW
#include <time.h>
#include <libgen.h>

#ifdef __MINGW32__
/* Loaned from Enterprise extensions */
int isRunning(pid_t pid)
{
#if 0
    DWORD ret;
    HANDLE procHandle;

    procHandle = OpenProcess(SYNCHRONIZE, FALSE, pid);

    if (procHandle == NULL)
    {
        return false;
    }

    ret = WaitForSingleObject(procHandle, 0);
    CloseHandle(procHandle);

    return (ret == WAIT_TIMEOUT);
#endif
    return 0;
}
#endif // __MINGW32__ This covers both 32 and 64 bits MinGW

int run_process_finish(const char *command, char **args, char **envp)
{
    if (!command || !args || !envp)
    {
        return -1;
    }
    /* Execute the command */
#ifndef __MINGW32__
    execve(command, args, envp);
#endif
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
#if defined(S_IRGRP) && defined(S_IROTH)
    fd = open(filenamelog, O_CREAT, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
#else
    fd = open(filenamelog, O_CREAT, S_IWUSR|S_IRUSR);
#endif
    if (fd < 0)
    {
        return RUN_PROCESS_FAILURE_VALUE;
    }
    int exit_status = 0;
#ifndef __MINGW32__
    pid_t child = fork();
#else
    int child = 0;
#endif
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
#ifndef __MINGW32__
        /* Finally execute the command */
        execve(command, args, envp);
#endif
        /* If we reach here, the we failed */
        log_entry(LogCritical, "Could not execute helper process %s", command);
        exit(-1);
    }
    else
    {
        /* Parent */
        int status = -1;
        free(filenamelog);
#ifndef __MINGW32__
        /*
         * Due to differences between Unixes and Windows, this code can only
         * be compiled in Unix.
         * Basically this code waits for the child process to die and then
         * returns the status. In Windows we cannot do that because Windows does
         * not have that kind of information. And the only kind of information
         * that is available is by using their calls, which would mean deviating
         * from MinGW & friends.
         * This site contains more information:
         * http://www.interopcommunity.com/dictionary/waitpid-entry.php
         */
        waitpid(child, &status, 0);
        if (WIFEXITED(status))
        {
            exit_status = WEXITSTATUS(status);
        }
#else
       /*
        * There is a semantic problem here. Since we do not wait, the
        * child process might not be done when we return, which might present
        * some interesting challenges. The main challenge is synchronization,
        * which needs to be addressed to avoid major problems.
        * We address the synchronization problem by borrowing isRunning from
        * our enterprise extensions. It will return true as long as the process
        * is running. We top that with a loop counter and we abort after 10
        * minutes (10 * 60 seconds = 600).
        * There is one more problem that is not easy to solve. In Windows the
        * system does not keep track of exit statuses, therefore we do not know
        * what happened with our child process.
        * We assume that everything worked, after all, what could go wrong?
        */
        int counter = 0;
        while(isRunning(child))
        {
            sleep(1);
            ++counter;
            if (counter >= 600)
            {
                /* We tell our child process to die. */
#if 0
                kill(child, SIGKILL);
#endif
                exit_status = -1;
            }
        }
#endif // __MINGW32__ This covers both 32 and 64 bits MinGW.
        close (fd);
    }
    return exit_status;
}
