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

/* Minimum time between two logs of a persisting failure to check the file,
 * since the file is checked on every poll */
#define CHECK_ERROR_LOG_INTERVAL_SECS 60

typedef struct
{
    char *path;
    time_t last_checked;      /* Time of the last successful check, 0 if none yet */
    time_t last_error_logged; /* Time the last error was logged, 0 if no errors */

    /* What the last successful check observed */
    bool last_exists;
} FileWatcherState;


void *FileWatcherStateNew(const Rlist *args)
{
    assert(args != NULL);
    const char *path = RlistScalarValue(args);

    FileWatcherState *fws = (FileWatcherState *) xcalloc(1, sizeof(FileWatcherState));
    fws->path = xstrdup(path);

    return fws;
}

void FileWatcherStateDestroy(void *state)
{
    FileWatcherState *fws = state;
    if (fws != NULL)
    {
        free(fws->path);
        free(fws);
    }
}

// =========== code shared by the file event types ===========

/* Logs the failure of operation op (with errno err) on the file of the
 * watcher. A persisting failure is logged at most once every
 * CHECK_ERROR_LOG_INTERVAL_SECS. */
static void LogCheckError(FileWatcherState *fws, const char *op, int err)
{
    assert(fws != NULL);

    const time_t now = time(NULL);
    if (fws->last_error_logged == 0 ||
        now - fws->last_error_logged >= CHECK_ERROR_LOG_INTERVAL_SECS)
    {
        Log(LOG_LEVEL_ERR, "Unable to %s '%s' while checking for file events: %s",
            op, fws->path, GetErrorStrFromCode(err));
        fws->last_error_logged = now;
    }
}

/* Checks whether the file of the watcher exists, with lstat(). On success,
 * returns true and sets *exists, as well as *sb if the file exists and sb is
 * not NULL. Only ENOENT/ENOTDIR mean the file doesn't exist. On any other
 * lstat() failure (e.g. EACCES, ESTALE), whether the file exists is unknown,
 * so the failure is logged (see LogCheckError()) and false is returned.
 *
 * Doesn't follow symlinks, e.g. a dangling symlink exists */
static bool FileLstat(FileWatcherState *fws, bool *exists, struct stat *sb)
{
    assert(fws != NULL);
    assert(exists != NULL);

    struct stat buf;
    if (lstat(fws->path, (sb != NULL) ? sb : &buf) == 0)
    {
        *exists = true;
        fws->last_error_logged = 0;
        return true;
    }

    if (errno == ENOENT || errno == ENOTDIR)
    {
        *exists = false;
        fws->last_error_logged = 0;
        return true;
    }

    LogCheckError(fws, "lstat", errno);
    return false;
}

/* Records a successful check, at which the file existed or not and the event
 * (e.g. "deletion") happened or not, and returns its result. The first
 * successful check is only the reference for the next ones, so it can't
 * fire. */
static WatcherCheckResult FileWatcherRecordObservation(FileWatcherState *fws, bool exists,
                                                       bool happened, const char *event)
{
    assert(fws != NULL);
    assert(event != NULL);

    const time_t now = time(NULL);
    happened = happened && (fws->last_checked != 0);

    if (happened)
    {
        char last_checked_str[26];
        const char *last_checked_ts = cf_strtimestamp_local(fws->last_checked, last_checked_str);

        char now_str[26];
        const char *now_ts = cf_strtimestamp_local(now, now_str);

        Log(LOG_LEVEL_VERBOSE,
            "Observed %s of file '%s' at %s (last checked at %s)",
            event,
            fws->path,
            (now_ts != NULL) ? now_ts : "unknown time",
            (last_checked_ts != NULL) ? last_checked_ts : "unknown time");
    }

    fws->last_checked = now;
    fws->last_exists = exists;
    return happened ? WATCHER_CHECK_EVENT : WATCHER_CHECK_NO_EVENT;
}

// =========== code for the file_deleted event ===========

WatcherCheckResult FileWatcherCheckFileDeleted(void *state)
{
    assert(state != NULL);
    FileWatcherState *fws = state;

    bool exists;
    if (!FileLstat(fws, &exists, NULL))
    {
        /* Unknown state, keep the last observation as is */
        return WATCHER_CHECK_ERROR;
    }

    bool deleted = fws->last_exists && !exists;
    return FileWatcherRecordObservation(fws, exists, deleted, "deletion");
}

