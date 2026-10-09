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
#include <prototypes3.h>        /* cf_strtimestamp_local() */
#include <sys/stat.h>
#include <errno.h>

// =========== code for EVENT_FILE_DELETED event type ===========

/* Minimum time between two logs of a persisting lstat() failure, since the
 * file is checked on every poll */
#define LSTAT_ERROR_LOG_INTERVAL_SECS 60

typedef struct
{
    char *path;
    time_t last_seen; /* Time the file was last observed present, 0 if absent */
    time_t lstat_error_logged; /* Time an lstat() failure was last logged, 0 if lstat() works */
} FileWatcherState;

/* Sets *deleted to whether the path of the watcher exists. Only
 * ENOENT/ENOTDIR mean the path genuinely doesn't exist. Any other lstat()
 * failure (e.g. EACCES, ESTALE) is a transient/permission error, in which case
 * we can't tell whether the file was deleted, so false is returned and
 * *deleted is left untouched. A persisting failure is logged at most once
 * every LSTAT_ERROR_LOG_INTERVAL_SECS.
 *
 * Doesn't follow symlinks, i.e. a dangling symlink is not considered deleted */
static bool FileDeleted(FileWatcherState *fws, bool *deleted)
{
    assert(fws != NULL);
    assert(deleted != NULL);

    struct stat sb;
    if (lstat(fws->path, &sb) == 0)
    {
        *deleted = false;
        fws->lstat_error_logged = 0;
        return true;
    }

    if (errno == ENOENT || errno == ENOTDIR)
    {
        *deleted = true;
        fws->lstat_error_logged = 0;
        return true;
    }

    const int lstat_errno = errno;
    const time_t now = time(NULL);
    if (fws->lstat_error_logged == 0 ||
        now - fws->lstat_error_logged >= LSTAT_ERROR_LOG_INTERVAL_SECS)
    {
        Log(LOG_LEVEL_ERR, "Unable to lstat '%s' while checking for file deletion: %s",
            fws->path, GetErrorStrFromCode(lstat_errno));
        fws->lstat_error_logged = now;
    }
    return false;
}

void *FileWatcherStateNew(const char *path)
{
    FileWatcherState *fws = (FileWatcherState *) xmalloc(sizeof(FileWatcherState));

    fws->path = xstrdup(path);
    fws->lstat_error_logged = 0;

    /* If the file's presence can't be determined, treat it as not yet seen */
    bool deleted;
    fws->last_seen = (FileDeleted(fws, &deleted) && !deleted) ? time(NULL) : 0;

    return (void *) fws;
}

WatcherCheckResult CheckFileDeleted(void *state)
{
    assert(state != NULL);
    FileWatcherState *fws = state;

    bool deleted_now;
    if (!FileDeleted(fws, &deleted_now))
    {
        /* Unknown state, keep last_seen as is */
        return WATCHER_CHECK_ERROR;
    }

    const time_t now = time(NULL);
    const bool deleted = (fws->last_seen != 0) && deleted_now;

    if (deleted)
    {
        char last_seen_str[26];
        const char *last_seen_ts = cf_strtimestamp_local(fws->last_seen, last_seen_str);

        char now_str[26];
        const char *now_ts = cf_strtimestamp_local(now, now_str);

        Log(LOG_LEVEL_VERBOSE,
            "Observed deletion of file '%s' at %s (last seen present at %s)",
            fws->path,
            (now_ts != NULL) ? now_ts : "unknown time",
            (last_seen_ts != NULL) ? last_seen_ts : "unknown time");
    }

    fws->last_seen = deleted_now ? 0 : now;
    return deleted ? WATCHER_CHECK_EVENT : WATCHER_CHECK_NO_EVENT;
}

void DestroyFileWatcherState(void *state)
{
    FileWatcherState *fws = state;
    if (fws != NULL)
    {
        free(fws->path);
        free(fws);
    }
}

