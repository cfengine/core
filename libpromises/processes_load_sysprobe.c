/*
   Copyright 2019 Northern.tech AS

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


#include <processes_select.h>
#include <process_unix_priv.h>
#include <process_lib.h>

#include <conversion.h>
#include <dir.h>


#define NPROC_GUESS 500

static JsonElement *PROCTABLE = NULL;


const char *GetProcessTableLegend(void)
{
    if (PROCTABLE)
    {
        return "<not using ps command>";
    }
    else
    {
        return "<Process table not loaded>";
    }
}


/*
 * LoadProcessTable() and subordinates
 *
 * ClearProcessTable()
 */

/* "/proc/nnn" is a process directory only if pure integer "nnn" */
static bool IsProcDir(const char *name)
{
    const char *p = name;

    while (*p)
    {
        if (!isdigit(*p)) {
            return false;
        }
        p++;
    }

    return true;
}


bool LoadProcessTable()
{
    JsonElement *proctable;
    Dir *dirh = NULL;
    const struct dirent *dirp;

    if (PROCTABLE)
    {
        Log(LOG_LEVEL_VERBOSE, "Reusing cached process table");
        return true;
    }

    if ((dirh = DirOpen(PROCDIR)) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to open %s directory'. (opendir: %s)", PROCDIR, GetErrorStr());
        return false;
    }

    proctable = JsonObjectCreate(NPROC_GUESS);

    pid_t  pid;
    JsonElement *pdata;
    while ((dirp = DirRead(dirh)) != NULL)
    {
        /*
         * Process next entry. Skip non-numeric names as being non-process.
         */
        if (! IsProcDir(dirp->d_name))
        {
            continue;
        }

        pid = atol(dirp->d_name);

        /* It ought to be a directory... */
        if (dirp->d_type != DT_DIR)
        {
            Log(LOG_LEVEL_ERR, "'%s/%s' not a directory\n", PROCDIR, dirp->d_name);
            continue;
        }

        pdata = LoadProcStat(pid);
        if (pdata == NULL)
        {
            Log(LOG_LEVEL_ERR, "failure creating 'stat' data for '%s'\n", dirp->d_name);
            continue;
        }

        JsonObjectAppendObject(proctable, dirp->d_name, pdata);

    }

    DirClose(dirh);

    PROCTABLE = proctable;

    return true;

}

/*
 * return a read-only pointer to the JSON version of the process table
 */
const JsonElement *FetchProcessTable()
{
    if (! LoadProcessTable())
    {
        Log(LOG_LEVEL_ERR, "Unable to load process table");
        return NULL;
    }

    return PROCTABLE;
}

void ClearProcessTable(void)
{
    JsonDestroy(PROCTABLE);
    PROCTABLE= NULL;
}


/*
 * Access points for individual processes.
 * Independent of complete PROCTABLE.
 * Initial use is mostly unit tests
 * although there are a couple of other uses.
 */

time_t GetProcessStartTime(pid_t pid)
{
    JsonElement *pdata;

    pdata = LoadProcStat(pid);
    if (pdata) {
        time_t t = IntFromString(JsonObjectGetAsString(pdata, JPROC_KEY_STARTTIME_BOOT));

        JsonDestroy(pdata);

        return t;
    }
    else
    {
        return PROCESS_START_TIME_UNKNOWN;
    }
}

ProcessState GetProcessState(pid_t pid)
{
    JsonElement *pdata;

    pdata = LoadProcStat(pid);
    if (pdata) {
        ProcessState pstate;

        const char *status = JsonObjectGetAsString(pdata, JPROC_KEY_PSTATE);
        switch (status[0])
        {
        case 'T':
            pstate = PROCESS_STATE_STOPPED;
            break;
        case 'Z':
            pstate = PROCESS_STATE_ZOMBIE;
            break;
        default:
            pstate = PROCESS_STATE_RUNNING;
            break;
        }

        JsonDestroy(pdata);

        return pstate;
    }
    else
    {
        return PROCESS_STATE_DOES_NOT_EXIST;
    }
}
