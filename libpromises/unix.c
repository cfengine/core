/*
   Copyright 2017 Northern.tech AS

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

#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#ifndef __MINGW32__

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
    struct passwd *user_ptr;

    memset(userName, 0, userNameLen);
    user_ptr = getpwuid(getuid());

    if (user_ptr == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not get user name of current process, using 'UNKNOWN'. (getpwuid: %s)", GetErrorStr());
        strlcpy(userName, "UNKNOWN", userNameLen);
        return false;
    }

    strlcpy(userName, user_ptr->pw_name, userNameLen);
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
