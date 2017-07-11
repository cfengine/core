/*
   Copyright 2017 Northern.tech AS

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

#include <cf3.defs.h>

#include <mon.h>
#include <item_lib.h>
#include <files_interfaces.h>
#include <pipes.h>
#include <systype.h>
#include <known_dirs.h>

#include <cf-windows-functions.h>

/* Prototypes */

#ifndef __MINGW32__
static bool GatherProcessUsers(Item **userList, int *userListSz, int *numRootProcs, int *numOtherProcs);
#endif

/* Implementation */

void MonProcessesGatherData(double *cf_this)
{
    Item *userList = NULL;
    int numProcUsers = 0;
    int numRootProcs = 0;
    int numOtherProcs = 0;

    if (!GatherProcessUsers(&userList, &numProcUsers, &numRootProcs, &numOtherProcs))
    {
        return;
    }

    cf_this[ob_users] += numProcUsers;
    cf_this[ob_rootprocs] += numRootProcs;
    cf_this[ob_otherprocs] += numOtherProcs;

    char vbuff[CF_MAXVARSIZE];
    xsnprintf(vbuff, sizeof(vbuff), "%s/cf_users", GetStateDir());
    MapName(vbuff);

    RawSaveItemList(userList, vbuff, NewLineMode_Unix);
    DeleteItemList(userList);

    Log(LOG_LEVEL_VERBOSE, "(Users,root,other) = (%d,%d,%d)",
        (int) cf_this[ob_users], (int) cf_this[ob_rootprocs],
        (int) cf_this[ob_otherprocs]);
}

#ifndef __MINGW32__

static bool GatherProcessUsers(Item **userList, int *userListSz, int *numRootProcs, int *numOtherProcs)
{
    char pscomm[CF_BUFSIZE];
    xsnprintf(pscomm, sizeof(pscomm), "%s %s",
              VPSCOMM[VPSHARDCLASS], VPSOPTS[VPSHARDCLASS]);

    FILE *pp;
    if ((pp = cf_popen(pscomm, "r", true)) == NULL)
    {
        /* FIXME: no logging */
        return false;
    }

    size_t vbuff_size = CF_BUFSIZE;
    char *vbuff = xmalloc(vbuff_size);

    /* Ignore first line -- header */
    ssize_t res = CfReadLine(&vbuff, &vbuff_size, pp);
    if (res <= 0)
    {
        /* FIXME: no logging */
        cf_pclose(pp);
        free(vbuff);
        return false;
    }

    for (;;)
    {
        res = CfReadLine(&vbuff, &vbuff_size, pp);
        if (res == -1)
        {
            if (!feof(pp))
            {
                /* FIXME: no logging */
                cf_pclose(pp);
                free(vbuff);
                return false;
            }
            else
            {
                break;
            }
        }

        char user[64];
        int ret = sscanf(vbuff, " %63s ", user);

        /* CFE-1560: Skip the username if it starts with a digit, this means
         * that we are reading the PID! Happens on some platforms (e.g. AIX)
         * where zombie processes have an empty username field in ps. */
        if (ret != 1 || user[0] == '\0' || isdigit(user[0]))
        {
            continue;
        }

        if (!IsItemIn(*userList, user))
        {
            PrependItem(userList, user, NULL);
            (*userListSz)++;
        }

        if (strcmp(user, "root") == 0)
        {
            (*numRootProcs)++;
        }
        else
        {
            (*numOtherProcs)++;
        }
    }

    if (LogGetGlobalLevel() >= LOG_LEVEL_DEBUG)
    {
        char *s = ItemList2CSV(*userList);
        Log(LOG_LEVEL_DEBUG, "Users in the process table: (%s)", s);
        free(s);
    }
    cf_pclose(pp);
    free(vbuff);
    return true;
}

#endif
