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

#include "dir.h"
#include "dir_priv.h"
#include "item_lib.h"

/*********************************************************************/

Dir *OpenDirForPromise(const char *dirname, Attributes attr, Promise *pp)
{
    if ((attr.copy.servers == NULL) || (strcmp(attr.copy.servers->item, "localhost") == 0))
    {
        return OpenDirLocal(dirname);
    }
    else
    {
        /* -> client_code.c to talk to server */
        return OpenDirRemote(dirname, attr, pp);
    }
}

/*********************************************************************/

static const struct dirent *ReadDirRemote(Dir *dir)
{
    const char *ret = NULL;

    if (dir->listpos != NULL)
    {
        ret = dir->listpos->name;
        dir->listpos = dir->listpos->next;
    }

    return (struct dirent *) ret;
}

/*********************************************************************/

static void CloseDirRemote(Dir *dir)
{
    if (dir->list)
    {
        DeleteItemList(dir->list);
    }
    free(dir);
}

/*********************************************************************/

const struct dirent *ReadDir(Dir *dir)
{
    if (dir->list)
    {
        return ReadDirRemote(dir);
    }
    else if (dir->dirh)
    {
        return ReadDirLocal(dir);
    }
    else
    {
        FatalError("Dir passed has no list nor directory handle open");
    }
}

/*********************************************************************/

void CloseDir(Dir *dir)
{
    if (dir->dirh)
    {
        CloseDirLocal(dir);
    }
    else
    {
        CloseDirRemote(dir);
    }
}
