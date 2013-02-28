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

#include "pipes.h"

#include "cfstream.h"
#include "transaction.h"
#include "exec_tools.h"
#include "rlist.h"
#include "policy.h"

#ifndef __MINGW32__
static int CfSetuid(uid_t uid, gid_t gid);
#endif

int VerifyCommandRetcode(EvalContext *ctx, int retcode, int fallback, Attributes a, Promise *pp)
{
    char retcodeStr[128] = { 0 };
    int result = true;
    int matched = false;

    if ((a.classes.retcode_kept) || (a.classes.retcode_repaired) || (a.classes.retcode_failed))
    {

        snprintf(retcodeStr, sizeof(retcodeStr), "%d", retcode);

        if (RlistKeyIn(a.classes.retcode_kept, retcodeStr))
        {
            cfPS(ctx, OUTPUT_LEVEL_INFORM, CF_NOP, "", pp, a,
                 "-> Command related to promiser \"%s\" returned code defined as promise kept (%d)", pp->promiser,
                 retcode);
            result = true;
            matched = true;
        }

        if (RlistKeyIn(a.classes.retcode_repaired, retcodeStr))
        {
            cfPS(ctx, OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a,
                 "-> Command related to promiser \"%s\" returned code defined as promise repaired (%d)", pp->promiser,
                 retcode);
            result = true;
            matched = true;
        }

        if (RlistKeyIn(a.classes.retcode_failed, retcodeStr))
        {
            cfPS(ctx, OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a,
                 "!! Command related to promiser \"%s\" returned code defined as promise failed (%d)", pp->promiser,
                 retcode);
            result = false;
            matched = true;
        }

        if (!matched)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "",
                  "Command related to promiser \"%s\" returned code %d -- did not match any failed, repaired or kept lists",
                  pp->promiser, retcode);
        }

    }
    else if (fallback)          // default: 0 is success, != 0 is failure
    {
        if (retcode == 0)
        {
            cfPS(ctx, OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Finished command related to promiser \"%s\" -- succeeded",
                 pp->promiser);
            result = true;
        }
        else
        {
            cfPS(ctx, OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a,
                 " !! Finished command related to promiser \"%s\" -- an error occurred (returned %d)", pp->promiser,
                 retcode);
            result = false;
        }
    }

    return result;
}

#ifndef __MINGW32__

/*****************************************************************************/

static pid_t *CHILDREN;
static int MAX_FD = 128;               /* Max number of simultaneous pipes */

static int InitChildrenFD()
{
    if (!ThreadLock(cft_count))
    {
        return false;
    }
        
    if (CHILDREN == NULL)       /* first time */
    {
        CHILDREN = xcalloc(MAX_FD, sizeof(pid_t));
    }

    ThreadUnlock(cft_count);
    return true;
}

/*****************************************************************************/

static void CloseChildrenFD()
{
    ThreadLock(cft_count);
    int i;
    for (i = 0; i < MAX_FD; i++)
    {
        if (CHILDREN[i] > 0)
        {
            close(i);
        }
    }
    ThreadUnlock(cft_count);
}

/*****************************************************************************/

static void SetChildFD(int fd, pid_t pid)
{
    int new_fd = 0;

    if (fd >= MAX_FD)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
                "File descriptor %d of child %jd higher than MAX_FD, check for defunct children",
                fd, (intmax_t)pid);
        new_fd = fd + 32;
    }

    ThreadLock(cft_count);

    if (new_fd)
    {
        CHILDREN = xrealloc(CHILDREN, new_fd * sizeof(pid_t));
        MAX_FD = new_fd;
    }

    CHILDREN[fd] = pid;
    ThreadUnlock(cft_count);
}

/*****************************************************************************/

static pid_t CreatePipeAndFork(char *type, int *pd)
{
    pid_t pid = -1;

    if (((*type != 'r') && (*type != 'w')) || (type[1] != '\0'))
    {
        errno = EINVAL;
        return -1;
    }

    if (!InitChildrenFD())
    {
        return -1;
    }

    if (pipe(pd) < 0)           /* Create a pair of descriptors to this process */
    {
        return -1;
    }

    if ((pid = fork()) == -1)
    {
        close(pd[0]);
        close(pd[1]);
        return -1;
    }

    signal(SIGCHLD, SIG_DFL);

    ALARM_PID = (pid != 0 ? pid : -1);

    return pid;
}

/*****************************************************************************/

FILE *cf_popen(const char *command, char *type)
{
    int pd[2];
    char **argv;
    pid_t pid;
    FILE *pp = NULL;

    CfDebug("cf_popen(%s)\n", command);

    pid = CreatePipeAndFork(type, pd);
    if (pid == -1) {
        return NULL;
    }

    if (pid == 0)
    {
        switch (*type)
        {
        case 'r':

            close(pd[0]);       /* Don't need output from parent */

            if (pd[1] != 1)
            {
                dup2(pd[1], 1); /* Attach pp=pd[1] to our stdout */
                dup2(pd[1], 2); /* Merge stdout/stderr */
                close(pd[1]);
            }

            break;

        case 'w':

            close(pd[1]);

            if (pd[0] != 0)
            {
                dup2(pd[0], 0);
                close(pd[0]);
            }
        }

        CloseChildrenFD();

        argv = ArgSplitCommand(command);

        if (execv(argv[0], argv) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "execv", "Couldn't run %s", argv[0]);
        }

        _exit(1);
    }
    else
    {
        switch (*type)
        {
        case 'r':

            close(pd[1]);

            if ((pp = fdopen(pd[0], type)) == NULL)
            {
                cf_pwait(pid);
                return NULL;
            }
            break;

        case 'w':

            close(pd[0]);

            if ((pp = fdopen(pd[1], type)) == NULL)
            {
                cf_pwait(pid);
                return NULL;
            }
        }

        SetChildFD(fileno(pp), pid);
        return pp;
    }

    return NULL;                /* Cannot reach here */
}

/*****************************************************************************/

FILE *cf_popensetuid(const char *command, char *type, uid_t uid, gid_t gid, char *chdirv, char *chrootv, int background)
{
    int pd[2];
    char **argv;
    pid_t pid;
    FILE *pp = NULL;

    CfDebug("cf_popensetuid(%s,%s,%" PRIuMAX ",%" PRIuMAX ")\n", command, type, (uintmax_t)uid, (uintmax_t)gid);

    pid = CreatePipeAndFork(type, pd);
    if (pid == -1) {
        return NULL;
    }

    if (pid == 0)
    {
        switch (*type)
        {
        case 'r':

            close(pd[0]);       /* Don't need output from parent */

            if (pd[1] != 1)
            {
                dup2(pd[1], 1); /* Attach pp=pd[1] to our stdout */
                dup2(pd[1], 2); /* Merge stdout/stderr */
                close(pd[1]);
            }

            break;

        case 'w':

            close(pd[1]);

            if (pd[0] != 0)
            {
                dup2(pd[0], 0);
                close(pd[0]);
            }
        }

        CloseChildrenFD();

        argv = ArgSplitCommand(command);

        if (chrootv && (strlen(chrootv) != 0))
        {
            if (chroot(chrootv) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "chroot", "Couldn't chroot to %s\n", chrootv);
                ArgFree(argv);
                return NULL;
            }
        }

        if (chdirv && (strlen(chdirv) != 0))
        {
            if (chdir(chdirv) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "chdir", "Couldn't chdir to %s\n", chdirv);
                ArgFree(argv);
                return NULL;
            }
        }

        if (!CfSetuid(uid, gid))
        {
            _exit(1);
        }

        if (execv(argv[0], argv) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "execv", "Couldn't run %s", argv[0]);
        }

        _exit(1);
    }
    else
    {
        switch (*type)
        {
        case 'r':

            close(pd[1]);

            if ((pp = fdopen(pd[0], type)) == NULL)
            {
                cf_pwait(pid);
                return NULL;
            }
            break;

        case 'w':

            close(pd[0]);

            if ((pp = fdopen(pd[1], type)) == NULL)
            {
                cf_pwait(pid);
                return NULL;
            }
        }

        SetChildFD(fileno(pp), pid);
        return pp;
    }

    return NULL;                /* cannot reach here */
}

/*****************************************************************************/
/* Shell versions of commands - not recommended for security reasons         */
/*****************************************************************************/

FILE *cf_popen_sh(const char *command, char *type)
{
    int pd[2];
    pid_t pid;
    FILE *pp = NULL;

    CfDebug("cf_popen_sh(%s)\n", command);

    pid = CreatePipeAndFork(type, pd);
    if (pid == -1) {
        return NULL;
    }

    if (pid == 0)
    {
        switch (*type)
        {
        case 'r':

            close(pd[0]);       /* Don't need output from parent */

            if (pd[1] != 1)
            {
                dup2(pd[1], 1); /* Attach pp=pd[1] to our stdout */
                dup2(pd[1], 2); /* Merge stdout/stderr */
                close(pd[1]);
            }

            break;

        case 'w':

            close(pd[1]);

            if (pd[0] != 0)
            {
                dup2(pd[0], 0);
                close(pd[0]);
            }
        }

        CloseChildrenFD();

        execl(SHELL_PATH, "sh", "-c", command, NULL);
        _exit(1);
    }
    else
    {
        switch (*type)
        {
        case 'r':

            close(pd[1]);

            if ((pp = fdopen(pd[0], type)) == NULL)
            {
                cf_pwait(pid);
                return NULL;
            }
            break;

        case 'w':

            close(pd[0]);

            if ((pp = fdopen(pd[1], type)) == NULL)
            {
                cf_pwait(pid);
                return NULL;
            }
        }

        SetChildFD(fileno(pp), pid);
        return pp;
    }

    return NULL;
}

/******************************************************************************/

FILE *cf_popen_shsetuid(const char *command, char *type, uid_t uid, gid_t gid, char *chdirv, char *chrootv, int background)
{
    int pd[2];
    pid_t pid;
    FILE *pp = NULL;

    CfDebug("cf_popen_shsetuid(%s,%s,%" PRIuMAX ",%" PRIuMAX ")\n", command, type, (uintmax_t)uid, (uintmax_t)gid);

    pid = CreatePipeAndFork(type, pd);
    if (pid == -1) {
        return NULL;
    }

    if (pid == 0)
    {
        switch (*type)
        {
        case 'r':

            close(pd[0]);       /* Don't need output from parent */

            if (pd[1] != 1)
            {
                dup2(pd[1], 1); /* Attach pp=pd[1] to our stdout */
                dup2(pd[1], 2); /* Merge stdout/stderr */
                close(pd[1]);
            }

            break;

        case 'w':

            close(pd[1]);

            if (pd[0] != 0)
            {
                dup2(pd[0], 0);
                close(pd[0]);
            }
        }

        CloseChildrenFD();

        if (chrootv && (strlen(chrootv) != 0))
        {
            if (chroot(chrootv) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "chroot", "Couldn't chroot to %s\n", chrootv);
                return NULL;
            }
        }

        if (chdirv && (strlen(chdirv) != 0))
        {
            if (chdir(chdirv) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "chdir", "Couldn't chdir to %s\n", chdirv);
                return NULL;
            }
        }

        if (!CfSetuid(uid, gid))
        {
            _exit(1);
        }

        execl(SHELL_PATH, "sh", "-c", command, NULL);
        _exit(1);
    }
    else
    {
        switch (*type)
        {
        case 'r':

            close(pd[1]);

            if ((pp = fdopen(pd[0], type)) == NULL)
            {
                cf_pwait(pid);
                return NULL;
            }
            break;

        case 'w':

            close(pd[0]);

            if ((pp = fdopen(pd[1], type)) == NULL)
            {
                cf_pwait(pid);
                return NULL;
            }
        }

        SetChildFD(fileno(pp), pid);
        return pp;
    }

    return NULL;
}

int cf_pwait(pid_t pid)
{
    int status;

    CfDebug("cf_pwait - Waiting for process %" PRIdMAX "\n", (intmax_t)pid);

# ifdef HAVE_WAITPID

    while (waitpid(pid, &status, 0) < 0)
    {
        if (errno != EINTR)
        {
            return -1;
        }
    }

    if (!WIFEXITED(status))
    {
        return -1;
    }

    return WEXITSTATUS(status);

# else

    while ((wait_result = wait(&status)) != pid)
    {
        if (wait_result <= 0)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "wait", " !! Wait for child failed\n");
            return -1;
        }
    }

    if (WIFSIGNALED(status))
    {
        return -1;
    }

    if (!WIFEXITED(status))
    {
        return -1;
    }

    return (WEXITSTATUS(status));
# endif
}

/*******************************************************************/

int cf_pclose(FILE *pp)
{
    int fd;
    pid_t pid;

    CfDebug("cf_pclose(pp)\n");

    if (!ThreadLock(cft_count))
    {
        return -1;
    }

    if (CHILDREN == NULL)       /* popen hasn't been called */
    {
        ThreadUnlock(cft_count);
        return -1;
    }

    ThreadUnlock(cft_count);

    ALARM_PID = -1;
    fd = fileno(pp);

    if (fd >= MAX_FD)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              "File descriptor %d of child higher than MAX_FD in cf_pclose, check for defunct children", fd);
        pid = -1;
    }
    else
    {
        if ((pid = CHILDREN[fd]) == 0)
        {
            return -1;
        }

        ThreadLock(cft_count);
        CHILDREN[fd] = 0;
        ThreadUnlock(cft_count);
    }

    if (fclose(pp) == EOF)
    {
        return -1;
    }

    return cf_pwait(pid);
}

/*******************************************************************/

int cf_pclose_def(EvalContext *ctx, FILE *pfp, Attributes a, Promise *pp)
/**
 * Defines command failure/success with cfPS based on exit code.
 */
{
    int fd, status;
    pid_t pid;

    CfDebug("cf_pclose_def(pfp)\n");

    if (!ThreadLock(cft_count))
    {
        return -1;
    }

    if (CHILDREN == NULL)       /* popen hasn't been called */
    {
        ThreadUnlock(cft_count);
        return -1;
    }

    ThreadUnlock(cft_count);

    ALARM_PID = -1;
    fd = fileno(pfp);

    if (fd >= MAX_FD)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              "File descriptor %d of child higher than MAX_FD in cf_pclose_def, check for defunct children", fd);
        fclose(pfp);
        return -1;
    }

    if ((pid = CHILDREN[fd]) == 0)
    {
        return -1;
    }

    ThreadLock(cft_count);
    CHILDREN[fd] = 0;
    ThreadUnlock(cft_count);

    if (fclose(pfp) == EOF)
    {
        return -1;
    }

    CfDebug("cf_pclose_def - Waiting for process %" PRIdMAX "\n", (intmax_t)pid);

# ifdef HAVE_WAITPID

    while (waitpid(pid, &status, 0) < 0)
    {
        if (errno != EINTR)
        {
            return -1;
        }
    }

    if (!WIFEXITED(status))
    {
        cfPS(ctx, OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a, " !! Finished script \"%s\" - failed (abnormal termination)", pp->promiser);
        return -1;
    }

    VerifyCommandRetcode(ctx, WEXITSTATUS(status), true, a, pp);

    return status;

# else

    while ((wait_result = wait(&status)) != pid)
    {
        if (wait_result <= 0)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "wait", "Wait for child failed\n");
            return -1;
        }
    }

    if (WIFSIGNALED(status))
    {
        cfPS(ctx, OUTPUT_LEVEL_INFORM, CF_INTERPT, "", pp, a, " -> Finished script - interrupted %s\n", pp->promiser);
        return -1;
    }

    if (!WIFEXITED(status))
    {
        cfPS(ctx, OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a, " !! Finished script \"%s\" - failed (abnormal termination)", pp->promiser);
        return -1;
    }

    VerifyCommandRetcode(WEXITSTATUS(status), true, a, pp);

    return (WEXITSTATUS(status));
# endif
}

/*******************************************************************/

bool PipeToPid(pid_t *pid, FILE *pp)
{
    if (!ThreadLock(cft_count))
    {
        return false;
    }

    if (CHILDREN == NULL)       /* popen hasn't been called */
    {
        ThreadUnlock(cft_count);
        return false;
    }

    int fd = fileno(pp);
    *pid = CHILDREN[fd];

    ThreadUnlock(cft_count);

    return true;
}

/*******************************************************************/

static int CfSetuid(uid_t uid, gid_t gid)
{
    struct passwd *pw;

    if (gid != (gid_t) - 1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Changing gid to %ju\n", (uintmax_t)gid);

        if (setgid(gid) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "setgid", "Couldn't set gid to %ju\n", (uintmax_t)gid);
            return false;
        }

        /* Now eliminate any residual privileged groups */

        if ((pw = getpwuid(uid)) == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "getpwuid", "Unable to get login groups when dropping privilege to %jd", (uintmax_t)uid);
            return false;
        }

        if (initgroups(pw->pw_name, pw->pw_gid) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "initgroups", "Unable to set login groups when dropping privilege to %s=%ju", pw->pw_name,
                  (uintmax_t)uid);
            return false;
        }
    }

    if (uid != (uid_t) - 1)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Changing uid to %ju\n", (uintmax_t)uid);

        if (setuid(uid) == -1)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "setuid", "Couldn't set uid to %ju\n", (uintmax_t)uid);
            return false;
        }
    }

    return true;
}

#endif /* !__MINGW32__ */
