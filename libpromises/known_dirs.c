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

#include <known_dirs.h>
#include <cf3.defs.h>
#include <file_lib.h>

#include <cf-windows-functions.h>

#if defined(__CYGWIN__)

static const char *GetDefaultWorkDir(void)
{
    return WORKDIR;
}

static const char *GetDefaultLogDir(void)
{
    return LOGDIR;
}

static const char *GetDefaultPidDir(void)
{
    return PIDDIR;
}

static const char *GetDefaultInputDir(void)
{
    return INPUTDIR;
}

static const char *GetDefaultMasterDir(void)
{
    return MASTERDIR;
}

#elif defined(__ANDROID__)

static const char *GetDefaultWorkDir(void)
{
    /* getpwuid() on Android returns /data, so use compile-time default instead */
    return WORKDIR;
}

static const char *GetDefaultLogDir(void)
{
    return LOGDIR;
}

static const char *GetDefaultPidDir(void)
{
    return PIDDIR;
}

static const char *GetDefaultInputDir(void)
{
    return INPUTDIR;
}

static const char *GetDefaultMasterDir(void)
{
    return MASTERDIR;
}

#elif !defined(__MINGW32__)

#define MAX_WORKDIR_LENGTH (CF_BUFSIZE / 2)

static const char *GetDefaultDir_helper(char dir[MAX_WORKDIR_LENGTH], const char *root_dir, const char *append_dir)
{
    if (getuid() > 0)
    {
        if (!*dir)
        {
            struct passwd *mpw = getpwuid(getuid());

            if ( append_dir == NULL )
            {
                if (snprintf(dir, MAX_WORKDIR_LENGTH, "%s/.cfagent", mpw->pw_dir) >= MAX_WORKDIR_LENGTH)
                {
                    return NULL;
                }
            }
            else
            {
                if (snprintf(dir, MAX_WORKDIR_LENGTH, "%s/.cfagent/%s", mpw->pw_dir, append_dir) >= MAX_WORKDIR_LENGTH)
                {
                    return NULL;
                }
            }
        }
        return dir;
    }
    else
    {
        return root_dir;
    }
}

static const char *GetDefaultWorkDir(void)
{
    static char workdir[MAX_WORKDIR_LENGTH] = ""; /* GLOBAL_C */
    return GetDefaultDir_helper(workdir, WORKDIR, NULL);
}

static const char *GetDefaultLogDir(void)
{
    static char logdir[MAX_WORKDIR_LENGTH] = ""; /* GLOBAL_C */
    return GetDefaultDir_helper(logdir, LOGDIR, NULL);
}

static const char *GetDefaultPidDir(void)
{
    static char piddir[MAX_WORKDIR_LENGTH] = ""; /* GLOBAL_C */
    return GetDefaultDir_helper(piddir, PIDDIR, NULL);
}

static const char *GetDefaultMasterDir(void)
{
    static char masterdir[MAX_WORKDIR_LENGTH] = ""; /* GLOBAL_C */
    return GetDefaultDir_helper(masterdir, MASTERDIR, "masterfiles");
}

static const char *GetDefaultInputDir(void)
{
    static char inputdir[MAX_WORKDIR_LENGTH] = ""; /* GLOBAL_C */
    return GetDefaultDir_helper(inputdir, INPUTDIR, "inputs");
}

#endif

const char *GetWorkDir(void)
{
    const char *workdir = getenv("CFENGINE_TEST_OVERRIDE_WORKDIR");

    return workdir == NULL ? GetDefaultWorkDir() : workdir;
}

const char *GetLogDir(void)
{
    const char *logdir = getenv("CFENGINE_TEST_OVERRIDE_WORKDIR");

    return logdir == NULL ? GetDefaultLogDir() : logdir;
}

const char *GetPidDir(void)
{
    const char *piddir = getenv("CFENGINE_TEST_OVERRIDE_WORKDIR");

    return piddir == NULL ? GetDefaultPidDir() : piddir;
}

const char *GetInputDir(void)
{
    const char *inputdir = getenv("CFENGINE_TEST_OVERRIDE_WORKDIR");

    if (inputdir != NULL) 
    {
        static char workbuf[CF_BUFSIZE];
        snprintf(workbuf, CF_BUFSIZE, "%s%cinputs", inputdir, FILE_SEPARATOR);
        return MapName(workbuf);
    }
    else if (strcmp(INPUTDIR, "default") == 0 )
    {
        static char workbuf[CF_BUFSIZE];
        snprintf(workbuf, CF_BUFSIZE, "%s%cinputs", GetWorkDir(), FILE_SEPARATOR);
        return MapName(workbuf);
    }
    else
    {
        return GetDefaultInputDir();
    }

}

const char *GetMasterDir(void)
{
    const char *masterdir = getenv("CFENGINE_TEST_OVERRIDE_WORKDIR");

    if (masterdir != NULL) 
    {
        static char workbuf[CF_BUFSIZE];
        snprintf(workbuf, CF_BUFSIZE, "%s%cmasterfiles", masterdir, FILE_SEPARATOR);
        return MapName(workbuf);
    }
    else if (strcmp(MASTERDIR, "default") == 0 )
    {
        static char workbuf[CF_BUFSIZE];
        snprintf(workbuf, CF_BUFSIZE, "%s%cmasterfiles", GetWorkDir(), FILE_SEPARATOR);
        return MapName(workbuf);
    }
    else
    {
        return GetDefaultMasterDir();
    }
}
