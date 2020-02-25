/*
  Copyright 2020 Northern.tech AS

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

#include <signals.h>
#include <signal_pipe.h>
#include <cleanup.h>
#include <known_dirs.h>         /* GetStateDir() */
#include <file_lib.h>           /* FILE_SEPARATOR */

static bool PENDING_TERMINATION = false; /* GLOBAL_X */

static bool RELOAD_CONFIG = false; /* GLOBAL_X */
/********************************************************************/

bool IsPendingTermination(void)
{
    return PENDING_TERMINATION;
}

bool ReloadConfigRequested(void)
{
    return RELOAD_CONFIG;
}

void ClearRequestReloadConfig()
{
    RELOAD_CONFIG = false;
}

/********************************************************************/
void HandleSignalsForAgent(int signum)
{
    switch (signum)
    {
    case SIGTERM:
    case SIGINT:
        /* TODO don't exit from the signal handler, just set a flag. Reason is
         * that all the cleanup() hooks we register are not reentrant. */
        DoCleanupAndExit(0);
    case SIGBUS:
        /* SIGBUS almost certainly means a violation of mmap() area boundaries
         * or some mis-aligned memory access. IOW, an LMDB corruption. */
        {
            char filename[PATH_MAX] = { 0 }; /* trying to avoid memory allocation */
            xsnprintf(filename, PATH_MAX, "%s%c%s",
                      GetStateDir(), FILE_SEPARATOR, CF_DB_REPAIR_TRIGGER);
            int fd = open(filename, O_CREAT|O_RDWR, CF_PERMS_DEFAULT);
            if (fd != -1)
            {
                close(fd);
            }

            /* avoid calling complex logging functions */
            fprintf(stdout, "process killed by SIGBUS\n");

            /* else: we tried, nothing more to do in the limited environment of a
             * signal handler */
            _exit(1);
        }
        break;
    case SIGUSR1:
        LogSetGlobalLevel(LOG_LEVEL_DEBUG);
        break;
    case SIGUSR2:
        LogSetGlobalLevel(LOG_LEVEL_NOTICE);
        break;
    default:
        /* No action */
        break;
    }

    SignalNotify(signum);

/* Reset the signal handler */
    signal(signum, HandleSignalsForAgent);
}

/********************************************************************/

void HandleSignalsForDaemon(int signum)
{
    switch (signum)
    {
    case SIGTERM:
    case SIGINT:
    case SIGSEGV:
    case SIGKILL:
        PENDING_TERMINATION = true;
        break;
    case SIGBUS:
        /* SIGBUS almost certainly means a violation of mmap() area boundaries
         * or some mis-aligned memory access. IOW, an LMDB corruption. */
        {
            char filename[PATH_MAX] = { 0 }; /* trying to avoid memory allocation */
            xsnprintf(filename, PATH_MAX, "%s%c%s",
                      GetStateDir(), FILE_SEPARATOR, CF_DB_REPAIR_TRIGGER);
            int fd = open(filename, O_CREAT|O_RDWR, CF_PERMS_DEFAULT);
            if (fd != -1)
            {
                close(fd);
            }

            /* avoid calling complex logging functions */
            fprintf(stdout, "process killed by SIGBUS\n");

            /* else: we tried, nothing more to do in the limited environment of a
             * signal handler */
            _exit(1);
        }
        break;
    case SIGUSR1:
        LogSetGlobalLevel(LOG_LEVEL_DEBUG);
        break;
    case SIGUSR2:
        LogSetGlobalLevel(LOG_LEVEL_NOTICE);
        break;
    case SIGHUP:
        RELOAD_CONFIG = true;
        break;
    case SIGPIPE:
    default:
        /* No action */
        break;
    }

    /* Notify processes that use the signal pipe (cf-serverd). */
    SignalNotify(signum);

    /* Reset the signal handler. */
    signal(signum, HandleSignalsForDaemon);
}
