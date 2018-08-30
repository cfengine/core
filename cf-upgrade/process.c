/*
   Copyright 2018 Northern.tech AS

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
#include <time.h>
#include <libgen.h>
/* Different includes for Unix and Windows */
#ifndef __MINGW32__
#include <sys/wait.h>
#else
/*
 * According to the documentation, the access rights given to the caller
 * process (what would be the parent process in Unix) suffered a modification
 * in Windows 2008 and upwards. The ALL_ACCESS flag changed value, therefore
 * to keep backward compatibility we need to define the supported OS to the
 * lowest OS level we want to support.
 * If this is not defined, then we will have problems in Windows 2003.
 * Yes, this is the right value there is no WIN2003 flag.
 */
#define _WIN32_WINNT _WIN32_WINNT_WINXP
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#endif // __MINGW32__ This covers both 32 and 64 bits MinGW


#ifndef __MINGW32__
/* Unix implementation */
int private_run_process_replace(const char *command, char **args, char **envp)
{
    /* Execute the command */
    execve(command, args, envp);
    /* If we reach here, then we have already failed */
    log_entry(LogCritical, "Temporary copy failed to run, aborting");
    return -1;
}

/*
 * Due to differences between Unixes and Windows, this code can only
 * be compiled in Unix.
 * Basically this code waits for the child process to die and then
 * returns the status. In Windows we cannot do that because Windows does
 * not have that kind of information. And the only kind of information
 * that is available is by using their calls, therefore we implement this
 * differently for Windows.
 * This site contains more information:
 * http://www.interopcommunity.com/dictionary/waitpid-entry.php
 */
int private_run_process_wait(const char *command, char **args, char **envp)
{
    char *filename = basename(xstrdup(command));
    const time_t now_seconds = time(NULL);
    struct tm now;
    gmtime_r(&now_seconds, &now);
    size_t filenamelog_size = (strlen(filename) +
                               strlen("-YYYYMMDD-HHMMSS") +
                               strlen(".log") + 1);
    char *filenamelog = xmalloc(filenamelog_size);
    snprintf(filenamelog, filenamelog_size,
              "%s-%04d%02d%02d-%02d%02d%02d.log", filename,
              now.tm_year + 1900, now.tm_mon, now.tm_mday,
              now.tm_hour, now.tm_min, now.tm_sec);

    int exit_status = 0;
    pid_t child = fork();
    if (child < 0)
    {
        log_entry(LogCritical, "Could not fork child process: %s", command);
        return RUN_PROCESS_FAILURE_VALUE;
    }
    else if (child == 0)
    {
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
    }
    return exit_status;
}
#else
/*
 * Windows implementations.
 * The Windows implementations were taken from Microsoft's documentation and
 * modified accordingly to fit our purposes.
 */
static void args_to_command_line(char *command_line, char **args,
                                 unsigned long command_line_size)
{
    /*
     * Windows does not use an array for the command line arguments, but
     * a string. Therefore we need to revert the parsing we did before and
     * build the string.
     */

    /* TODO put arguments in quotes! */
    command_line[0] = '\0';
    char *arg;
    while ((arg = *args) != NULL)
    {
        strlcat(command_line, arg, command_line_size);
        /* Add a space before the next argument */
        strlcat(command_line, " ", command_line_size);

        args++;
    }
}

int private_run_process_replace(const char *command, char **args, char **envp)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    char command_line[32768];

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );
    ZeroMemory( command_line, sizeof(command_line) );

    args_to_command_line(command_line, args, sizeof(command_line));

    log_entry(LogVerbose,
              "Creating process with command line: %s", command_line);

    // Start the child process.
    if( !CreateProcess( command,   // No module name (use command line)
                        command_line,        // Command line
                        NULL,           // Process handle not inheritable
                        NULL,           // Thread handle not inheritable
                        FALSE,          // Set handle inheritance to FALSE
                        0,              // No creation flags
                        NULL,           // Use parent's environment block
                        NULL,           // Use parent's starting directory
                        &si,            // Pointer to STARTUPINFO structure
                        &pi )           // Pointer to PROCESS_INFORMATION structure
            )
    {
        log_entry(LogCritical, "Temporary copy failed to run, aborting");
        return -1;
    }
    /*
     * The fork-exec paradigm does not exist in Windows. Basically once a process
     * is created, there is no parent-child relationship as in Unix, the two
     * processes are independent. So, now we just need to exit the parent process
     * and hope for the best. Notice that if the process failed to start we
     * would have caught it in the if loop above.
     */
    exit(EXIT_SUCCESS);
}
/*
 * There is an interesting difference in the Windows way of doing things versus
 * the Unix way. On Windows, programs usually do not use STDOUT or STDERR because
 * they have other reporting mechanisms. In addition the installers we run
 * are run using the silent flags, so they do not disturb the user. Therefore
 * there is no need to redirect the output to a log file.
 */
int private_run_process_wait(const char *command, char **args, char **envp)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    char command_line[32768];

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );
    ZeroMemory( command_line, sizeof(command_line) );

    args_to_command_line(command_line, args, sizeof(command_line));

    log_entry(LogVerbose,
              "Creating process with command line: %s", command_line);

    // Start the child process.
    if( !CreateProcess( command,   // No module name (use command line)
                        command_line,        // Command line
                        NULL,           // Process handle not inheritable
                        NULL,           // Thread handle not inheritable
                        FALSE,          // Set handle inheritance to FALSE
                        0,              // No creation flags
                        NULL,           // Use parent's environment block
                        NULL,           // Use parent's starting directory
                        &si,            // Pointer to STARTUPINFO structure
                        &pi )           // Pointer to PROCESS_INFORMATION structure
    )
    {
        log_entry(LogCritical, "Could not create child process: %s", command);
        return RUN_PROCESS_FAILURE_VALUE;
    }

    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );

    /* Get exit status */
    DWORD exit_status = 0;
    if ( !GetExitCodeProcess( pi.hProcess, &exit_status) )
    {
        log_entry(LogCritical, "Could not get exit status from process: %s", command);
    }

    // Close process and thread handles.
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    return (int)exit_status;
}
#endif

int run_process_replace(const char *command, char **args, char **envp)
{
    if (!command || !args || !envp)
    {
        return -1;
    }
    return private_run_process_replace(command, args, envp);
}

int run_process_wait(const char *command, char **args, char **envp)
{
    if (!command || !args || !envp)
    {
        return RUN_PROCESS_FAILURE_VALUE;
    }
    return private_run_process_wait(command, args, envp);
}
