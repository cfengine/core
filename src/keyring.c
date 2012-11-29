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

#include "cf3.defs.h"

#include "keyring.h"
#include "dir.h"
#include "cfstream.h"

/***************************************************************/

bool HostKeyAddressUnknown(const char *value)
{
    if (strcmp(value, CF_UNKNOWN_IP) == 0)
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

    snprintf(keysdir, CF_BUFSIZE, "%s/ppkeys", CFWORKDIR);
    MapName(keysdir);

    if ((dirh = OpenDirLocal(keysdir)) == NULL)
    {
        if (errno == ENOENT)
        {
            return 0;
        }
        else
        {
            CfOut(cf_error, "opendir", "Unable to open keys directory");
            return -1;
        }
    }

    snprintf(suffix, CF_BUFSIZE, "-%s.pub", id);

    while ((dirp = ReadDir(dirh)) != NULL)
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
                    CfOut(cf_error, "unlink", "Unable to remove key file %s", dirp->d_name);
                    CloseDir(dirh);
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
        CfOut(cf_error, "ReadDir", "Unable to enumerate files in keys directory");
        CloseDir(dirh);
        return -1;
    }

    CloseDir(dirh);
    return removed;
}
/***************************************************************/
