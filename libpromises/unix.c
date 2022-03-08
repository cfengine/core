/*
  Copyright 2021 Northern.tech AS

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
#define GETGR_R_SIZE_MAX 16384  /* same for group name */

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
    if (kill(pid, 0) != 0)
    {
        /* can no longer send signals to the process => it's dead now */
        return;
    }

    if(kill(pid, SIGTERM) == -1)
    {
        Log(LOG_LEVEL_ERR, "Could not send SIGTERM to pid '%jd'. (kill: %s)",
            (intmax_t)pid, GetErrorStr());
    }

    sleep(5);
    if (kill(pid, 0) != 0)
    {
        /* can no longer send signals to the process => it's dead now */
        return;
    }

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

bool IsExecutable(const char *file)
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
                exit(EXIT_FAILURE); /* OK since this is a forked proc and no registered cleanup functions */
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
                    exit(EXIT_FAILURE); /* OK since this is a forked proc and no registered cleanup functions */
                }

                if (dup2(devnull, STDOUT_FILENO) == -1 || dup2(devnull, STDERR_FILENO) == -1)
                {
                    Log(LOG_LEVEL_ERR, "Command '%s' failed. (dup2: %s)", command, GetErrorStr());
                    exit(EXIT_FAILURE); /* OK since this is a forked proc and no registered cleanup functions */
                }

                close(devnull);
            }

            if (execv(argv[0], argv) == -1)
            {
                Log(LOG_LEVEL_ERR, "Command '%s' failed. (execv: %s)", argv[0], GetErrorStr());
                exit(EXIT_FAILURE); /* OK since this is a forked proc and no registered cleanup functions */
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
                return false;
            }
        }

        return (WEXITSTATUS(status) == 0);
    }

    return false;
}

/*************************************************************/

bool GetUserName(uid_t uid, char *user_name_buf, size_t buf_size, LogLevel error_log_level)
{
    char buf[GETPW_R_SIZE_MAX] = {0};
    struct passwd pwd;
    struct passwd *result;

    int ret = getpwuid_r(uid, &pwd, buf, GETPW_R_SIZE_MAX, &result);
    if (result == NULL)
    {
        Log(error_log_level, "Could not get user name for uid %ju, (getpwuid: %s)",
            (uintmax_t) uid, (ret == 0) ? "not found" : GetErrorStrFromCode(ret));
        return false;
    }

    ret = strlcpy(user_name_buf, result->pw_name, buf_size);
    assert(ret < buf_size);
    if (ret >= buf_size)
    {
        /* Should never happen, but if it does, it's definitely an error. */
        Log(LOG_LEVEL_ERR, "Failed to get user name for uid %ju (buffer too small)",
            (uintmax_t) uid);
        return false;
    }

    return true;
}

bool GetCurrentUserName(char *userName, int userNameLen)
{
    memset(userName, 0, userNameLen);
    bool success = GetUserName(getuid(), userName, userNameLen, LOG_LEVEL_ERR);
    if (!success)
    {
        strlcpy(userName, "UNKNOWN", userNameLen);
    }

    return success;
}

bool GetGroupName(gid_t gid, char *group_name_buf, size_t buf_size, LogLevel error_log_level)
{
    char buf[GETGR_R_SIZE_MAX] = {0};
    struct group grp;
    struct group *result;

    int ret = getgrgid_r(gid, &grp, buf, GETGR_R_SIZE_MAX, &result);
    if (result == NULL)
    {
        Log(error_log_level, "Could not get group name for gid %ju, (getgrgid: %s)",
            (uintmax_t) gid, (ret == 0) ? "not found" : GetErrorStrFromCode(ret));
        return false;
    }

    ret = strlcpy(group_name_buf, result->gr_name, buf_size);
    assert(ret < buf_size);
    if (ret >= buf_size)
    {
        /* Should never happen, but if it does, it's definitely an error. */
        Log(LOG_LEVEL_ERR, "Failed to get group name for gid %ju (buffer too small)",
            (uintmax_t) gid);
        return false;
    }

    return true;
}

bool GetUserID(const char *user_name, uid_t *uid, LogLevel error_log_level)
{
    char buf[GETPW_R_SIZE_MAX] = {0};
    struct passwd pwd;
    struct passwd *result;

    int ret = getpwnam_r(user_name, &pwd, buf, GETPW_R_SIZE_MAX, &result);
    if (result == NULL)
    {
        Log(error_log_level, "Could not get UID for user %s, (getpwnam: %s)",
            user_name, (ret == 0) ? "not found" : GetErrorStrFromCode(ret));
        return false;
    }

    *uid = result->pw_uid;

    return true;
}

bool GetGroupID(const char *group_name, gid_t *gid, LogLevel error_log_level)
{
    char buf[GETGR_R_SIZE_MAX] = {0};
    struct group grp;
    struct group *result;

    int ret = getgrnam_r(group_name, &grp, buf, GETGR_R_SIZE_MAX, &result);
    if (result == NULL)
    {
        Log(error_log_level, "Could not get GID for group '%s', (getgrnam: %s)",
            group_name, (ret == 0) ? "not found" : GetErrorStrFromCode(ret));
        return false;
    }

    *gid = result->gr_gid;

    return true;
}

#endif /* !__MINGW32__ */
