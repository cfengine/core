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

#include <unix.h>
#include <exec_tools.h>
#include <file_lib.h>

#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#ifndef __MINGW32__

/* Max size of the 'passwd' string in the getpwuid_r() function,
 * man:getpwuid_r(3) says that this value "Should be more than enough". */
#define GETPW_R_SIZE_MAX 16384

static bool IsProcessRunning(pid_t pid);

void ProcessSignalTerminate(pid_t pid)
{
    if(!IsProcessRunning(pid))
    {
        return;
    }


    if(kill(pid, SIGINT) == -1)
    {
        Log(LOG_LEVEL_ERR, "Could not send SIGINT to pid '%jd'. (kill: %s)",
            (intmax_t)pid, GetErrorStr());
    }

    sleep(1);


    if(kill(pid, SIGTERM) == -1)
    {
        Log(LOG_LEVEL_ERR, "Could not send SIGTERM to pid '%jd'. (kill: %s)",
            (intmax_t)pid, GetErrorStr());
    }

    sleep(5);


    if(kill(pid, SIGKILL) == -1)
    {
        Log(LOG_LEVEL_ERR, "Could not send SIGKILL to pid '%jd'. (kill: %s)",
            (intmax_t)pid, GetErrorStr());
    }

    sleep(1);
}

/*************************************************************/

static bool IsProcessRunning(pid_t pid)
{
    int res = kill(pid, 0);

    if(res == 0)
    {
        return true;
    }

    if(res == -1 && errno == ESRCH)
    {
        return false;
    }

    Log(LOG_LEVEL_ERR, "Failed checking for process existence. (kill: %s)", GetErrorStr());

    return false;
}

/*************************************************************/

int GetCurrentUserName(char *userName, int userNameLen)
{
    char buf[GETPW_R_SIZE_MAX] = {0};
    struct passwd pwd;
    struct passwd *result;

    memset(userName, 0, userNameLen);
    int ret = getpwuid_r(getuid(), &pwd, buf, GETPW_R_SIZE_MAX, &result);

    if (result == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not get user name of current process, using 'UNKNOWN'. (getpwuid: %s)",
            ret == 0 ? "not found" : GetErrorStrFromCode(ret));
        strlcpy(userName, "UNKNOWN", userNameLen);
        return false;
    }

    strlcpy(userName, result->pw_name, userNameLen);
    return true;
}

/*************************************************************/

int IsExecutable(const char *file)
{
    struct stat sb;
    gid_t grps[NGROUPS];
    int n;

    if (stat(file, &sb) == -1)
    {
        Log(LOG_LEVEL_ERR, "Proposed executable file '%s' doesn't exist", file);
        return false;
    }

    if (sb.st_mode & 02)
    {
        Log(LOG_LEVEL_ERR, "SECURITY ALERT: promised executable '%s' is world writable! ", file);
        Log(LOG_LEVEL_ERR, "SECURITY ALERT: CFEngine will not execute this - requires human inspection");
        return false;
    }

    if ((getuid() == sb.st_uid) || (getuid() == 0))
    {
        if (sb.st_mode & 0100)
        {
            return true;
        }
    }
    else if (getgid() == sb.st_gid)
    {
        if (sb.st_mode & 0010)
        {
            return true;
        }
    }
    else
    {
        if (sb.st_mode & 0001)
        {
            return true;
        }

        if ((n = getgroups(NGROUPS, grps)) > 0)
        {
            int i;

            for (i = 0; i < n; i++)
            {
                if (grps[i] == sb.st_gid)
                {
                    if (sb.st_mode & 0010)
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool ShellCommandReturnsZero(const char *command, ShellType shell)
{
    int status;
    pid_t pid;

    if (shell == SHELL_TYPE_POWERSHELL)
    {
        Log(LOG_LEVEL_ERR, "Powershell is only supported on Windows");
        return false;
    }

    if ((pid = fork()) < 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to fork new process: %s", command);
        return false;
    }
    else if (pid == 0)          /* child */
    {
        ALARM_PID = -1;

        if (shell == SHELL_TYPE_USE)
        {
            if (execl(SHELL_PATH, "sh", "-c", command, NULL) == -1)
            {
                Log(LOG_LEVEL_ERR, "Command '%s' failed. (execl: %s)", command, GetErrorStr());
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            char **argv = ArgSplitCommand(command);
            int devnull;

            if (LogGetGlobalLevel() < LOG_LEVEL_INFO)
            {
                if ((devnull = open("/dev/null", O_WRONLY)) == -1)
                {
                    Log(LOG_LEVEL_ERR, "Command '%s' failed. (open: %s)", command, GetErrorStr());
                    exit(EXIT_FAILURE);
                }

                if (dup2(devnull, STDOUT_FILENO) == -1 || dup2(devnull, STDERR_FILENO) == -1)
                {
                    Log(LOG_LEVEL_ERR, "Command '%s' failed. (dup2: %s)", command, GetErrorStr());
                    exit(EXIT_FAILURE);
                }

                close(devnull);
            }

            if (execv(argv[0], argv) == -1)
            {
                Log(LOG_LEVEL_ERR, "Command '%s' failed. (execv: %s)", argv[0], GetErrorStr());
                exit(EXIT_FAILURE);
            }
        }
    }
    else                        /* parent */
    {
        ALARM_PID = pid;

        while (waitpid(pid, &status, 0) < 0)
        {
            if (errno != EINTR)
            {
                return -1;
            }
        }

        return (WEXITSTATUS(status) == 0);
    }

    return false;
}

#endif /* !__MINGW32__ */
