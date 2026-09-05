/*
  Copyright 2026 Northern.tech AS

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

#include <file_watcher.h>
#include <watcher.h>
#include <logging.h>
#include <alloc.h>
#include <sys/stat.h>
#include <errno.h>

// =========== code for EVENT_FILE_DELETED event type ===========

typedef struct
{
    char *path;
    bool existed_last_check;
} FileWatcherPayload;

/* Only ENOENT/ENOTDIR mean the path genuinely doesn't exist. Any other
 * stat() failure (e.g. EACCES, ESTALE) is a transient/permission error, not
 * a deletion, so treat the file as still present rather than misreporting
 * it as deleted. */
static bool FileExists(const char *path)
{
    struct stat sb;
    if (stat(path, &sb) == 0)
    {
        return true;
    }

    if (errno == ENOENT || errno == ENOTDIR)
    {
        return false;
    }

    Log(LOG_LEVEL_ERR, "Unable to stat '%s' while checking for file deletion: %s", path, GetErrorStr());
    return true;
}

void *FileWatcherPayloadNew(const char *path)
{
    FileWatcherPayload *pl = (FileWatcherPayload *) xmalloc(sizeof(FileWatcherPayload));

    pl->path = xstrdup(path);
    pl->existed_last_check = FileExists(path);

    return (void *) pl;
}

bool CheckFileExists(void *payload)
{
    FileWatcherPayload *fwp = payload;

    bool exists_now = FileExists(fwp->path);
    bool deleted = fwp->existed_last_check && !exists_now;

    fwp->existed_last_check = exists_now;
    return deleted;
}

void DestroyFileWatcherPayload(void *payload)
{
    FileWatcherPayload *fwp = payload;
    free(fwp->path);
    free(fwp);
}

