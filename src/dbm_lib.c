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

/*****************************************************************************/
/*                                                                           */
/* File: dbm_lib.c                                                           */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"
#include "dbm_lib.h"
#include <sys/file.h>

int DBPathLock(const char *filename)
{
    char *filename_lock;
    if (xasprintf(&filename_lock, "%s.lock", filename) == -1)
    {
        FatalError("Unable to construct lock database filename for file %s",
                   filename);
    }

    int fd = open(filename_lock, O_CREAT, 0666);

    free(filename_lock);

    if(fd == -1)
    {
        CfOut(cf_error, "flock", "!! Could not open db lock-file");
        return -1;
    }

    if(flock(fd, LOCK_EX|LOCK_NB) == -1)
    {
        close(fd);
        CfOut(cf_error, "flock", "!! Could not lock db lock-file");
        return -1;
    }

    return fd;
}

void DBPathUnLock(int fd)
{
    if(flock(fd, LOCK_UN) == -1)
    {
        CfOut(cf_error, "flock", "!! Could not unlock db lock-file");
    }

    if(close(fd) != 0)
    {
        CfOut(cf_error, "close", "!! Could not close db lock-file");
    }
}

void DBPathMoveBroken(const char *filename)
{
    char *filename_broken;
    if (xasprintf(&filename_broken, "%s.broken", filename) == -1)
    {
        FatalError("Unable to construct broken database filename for file %s", filename);
    }

    if(cf_rename(filename, filename_broken) != 0)
    {
        CfOut(cf_error, "", "!! Failed moving broken db out of the way");
    }

    free(filename_broken);
}
