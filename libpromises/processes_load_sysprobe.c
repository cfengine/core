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

bool LoadProcessTable()
{
    JsonElement *proctable;

    if (PROCTABLE)
    {
        Log(LOG_LEVEL_VERBOSE, "Reusing cached process table");
        return true;
    }

    void *procd;
    if ((procd = OpenProcDir()) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Unable to load process information");
        return false;
    }

    proctable = JsonObjectCreate(NPROC_GUESS);

    JsonElement *pdata;
    const char *pidstr;
    while ((pdata = ReadProcDir(procd)) != NULL) {
        pidstr = JsonObjectGetAsString(pdata, JPROC_KEY_PID);
        JsonObjectAppendObject(proctable, pidstr, pdata);
    }

    CloseProcDir(procd);

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
