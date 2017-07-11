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
#include <client_code.h>
#include <dir.h>
#include <abstract_dir.h>
#include <item_lib.h>
#include <rlist.h>

struct AbstractDir_
{
    /* Local directories */
    Dir *local_dir;

    /* Remote directories */
    Item *list;
    Item *listpos;
};

AbstractDir *AbstractDirOpen(const char *dirname, FileCopy fc, AgentConnection *conn)
{
    AbstractDir *d = xcalloc(1, sizeof(AbstractDir));
    if (conn == NULL)
    {
        d->local_dir = DirOpen(dirname);
        if (d->local_dir == NULL)
        {
            free(d);
            return NULL;
        }
    }
    else
    {
        assert(fc.servers && strcmp(RlistScalarValue(fc.servers), "localhost"));
        d->list = RemoteDirList(dirname, fc.encrypt, conn);
        if (d->list == NULL)
        {
            free(d);
            return NULL;
        }
        d->listpos = d->list;
    }
    return d;
}

static const struct dirent *RemoteDirRead(AbstractDir *dir)
{
    const struct dirent *ret = NULL;

    if (dir->listpos != NULL)
    {
        ret = (const struct dirent *)dir->listpos->name;
        dir->listpos = dir->listpos->next;
    }

    return ret;
}

const struct dirent *AbstractDirRead(AbstractDir *dir)
{
    if (dir->local_dir)
    {
        return DirRead(dir->local_dir);
    }
    else
    {
        return RemoteDirRead(dir);
    }
}

static void RemoteDirClose(AbstractDir *dir)
{
    if (dir->list)
    {
        DeleteItemList(dir->list);
    }
    free(dir);
}

void AbstractDirClose(AbstractDir *dir)
{
    if (dir->local_dir)
    {
        DirClose(dir->local_dir);
        free(dir);
    }
    else
    {
        RemoteDirClose(dir);
    }
}
