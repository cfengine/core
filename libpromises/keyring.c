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

#include <cf3.defs.h>

#include <keyring.h>
#include <dir.h>
#include <file_lib.h>
#include <known_dirs.h>

/***************************************************************/

bool HostKeyAddressUnknown(const char *value)
{
    if (strcmp(value, "location unknown") == 0)
    {
        return true;
    }

// Is there some other non-ip string left over?

    if (!((strchr(value, '.')) || (strchr(value, ':'))))
    {
        return false;
    }

    return false;
}

/***************************************************************/

int RemovePublicKey(const char *id)
{
    Dir *dirh = NULL;
    int removed = 0;
    char keysdir[CF_BUFSIZE];
    const struct dirent *dirp;
    char suffix[CF_BUFSIZE];

    snprintf(keysdir, CF_BUFSIZE, "%s/ppkeys", GetWorkDir());

    MapName(keysdir);

    if ((dirh = DirOpen(keysdir)) == NULL)
    {
        if (errno == ENOENT)
        {
            return 0;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Unable to open keys directory at '%s'. (opendir: %s)", keysdir, GetErrorStr());
            return -1;
        }
    }

    snprintf(suffix, CF_BUFSIZE, "-%s.pub", id);

    while ((dirp = DirRead(dirh)) != NULL)
    {
        char *c = strstr(dirp->d_name, suffix);

        if (c && c[strlen(suffix)] == '\0')     /* dirp->d_name ends with suffix */
        {
            char keyfilename[CF_BUFSIZE];

            snprintf(keyfilename, CF_BUFSIZE, "%s/%s", keysdir, dirp->d_name);
            MapName(keyfilename);

            if (unlink(keyfilename) < 0)
            {
                if (errno != ENOENT)
                {
                    Log(LOG_LEVEL_ERR, "Unable to remove key file '%s'. (unlink: %s)", dirp->d_name, GetErrorStr());
                    DirClose(dirh);
                    return -1;
                }
            }
            else
            {
                removed++;
            }
        }
    }

    if (errno)
    {
        Log(LOG_LEVEL_ERR, "Unable to enumerate files in keys directory. (ReadDir: %s)", GetErrorStr());
        DirClose(dirh);
        return -1;
    }

    DirClose(dirh);
    return removed;
}
/***************************************************************/
