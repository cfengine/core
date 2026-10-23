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
#include <file_lib.h>           /* safe_fopen() */
#include <hash.h>               /* HashDigestFromId() */
#include <conversion.h>         /* IntFromString() */
#include <prototypes3.h>        /* cf_strtimestamp_local() */
#include <sys/stat.h>
#include <errno.h>
#include <openssl/evp.h>

/* Minimum time between two logs of a persisting failure to check the file,
 * since the file is checked on every poll */
#define CHECK_ERROR_LOG_INTERVAL_SECS 60

#define CONTENT_HASH_METHOD HASH_METHOD_SHA256

typedef struct
{
    char *path;
    time_t last_checked;      /* Time of the last successful check, 0 if none yet */
    time_t last_error_logged; /* Time the last error was logged, 0 if no errors */

    /* What the last successful check observed */
    bool last_exists;

    /* What the last successful check observed, depending on the event type */
    union
    {
        /* FileWatcherCheckFileModified() */
        struct timespec mtime;

        /* FileWatcherCheckFileContentModified() */
        struct
        {
            bool hashed;      /* Whether it was a regular file, that was hashed */
            unsigned char digest[EVP_MAX_MD_SIZE + 1];
        } content;

        /* FileWatcherCheckFilePermissionsChanged() */
        mode_t mode;

        /* FileWatcherCheckFileOwnerChanged() */
        struct
        {
            uid_t uid;
            gid_t gid;
        } owner;

        /* FileWatcherCheckFileSizeExceeds() */
        struct
        {
            off_t threshold;  /* Not observed, set when the state is created */
            off_t size;
        } file_size;
    } last;
} FileWatcherState;


void *FileWatcherStateNew(const Rlist *args)
{
    assert(args != NULL);
    const char *path = RlistScalarValue(args);

    FileWatcherState *fws = (FileWatcherState *) xcalloc(1, sizeof(FileWatcherState));
    fws->path = xstrdup(path);

    return fws;
}

void *FileWatcherSizeStateNew(const Rlist *args)
{
    assert(args != NULL && args->next != NULL);
    const char *path = RlistScalarValue(args);
    const long size_threshold = IntFromString(RlistScalarValue(args->next));

    FileWatcherState *fws = (FileWatcherState *) xcalloc(1, sizeof(FileWatcherState));
    fws->path = xstrdup(path);
    fws->last.file_size.threshold = size_threshold;

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

    if (exists && !fws->last_exists)
    {
        Log(LOG_LEVEL_VERBOSE, "File '%s' is present, watching for its deletion", fws->path);
    }

    bool deleted = fws->last_exists && !exists;
    return FileWatcherRecordObservation(fws, exists, deleted, "deletion");
}

// =========== code for the file_created event ===========

WatcherCheckResult FileWatcherCheckFileCreated(void *state)
{
    assert(state != NULL);
    FileWatcherState *fws = state;

    bool exists;
    if (!FileLstat(fws, &exists, NULL))
    {
        return WATCHER_CHECK_ERROR;
    }

    const bool created = !fws->last_exists && exists;
    return FileWatcherRecordObservation(fws, exists, created, "creation");
}

// =========== code for the file_modified event ===========

static struct timespec StatMtime(const struct stat *sb)
{
    assert(sb != NULL);

#if defined(HAVE_STRUCT_STAT_ST_MTIM)
    return sb->st_mtim;
#elif defined(HAVE_STRUCT_STAT_ST_MTIMESPEC)
    return sb->st_mtimespec;
#else
    return (struct timespec) { .tv_sec = sb->st_mtime, .tv_nsec = 0 };
#endif
}

WatcherCheckResult FileWatcherCheckFileModified(void *state)
{
    assert(state != NULL);
    FileWatcherState *fws = state;

    bool exists;
    struct stat sb;
    if (!FileLstat(fws, &exists, &sb))
    {
        return WATCHER_CHECK_ERROR;
    }

    const struct timespec mtime = exists ? StatMtime(&sb) : (struct timespec) { 0 };

    /* Only modifications of a file present at both checks */
    const bool modified = fws->last_exists && exists &&
        ((fws->last.mtime.tv_sec != mtime.tv_sec) ||
         (fws->last.mtime.tv_nsec != mtime.tv_nsec));

    fws->last.mtime = mtime;
    return FileWatcherRecordObservation(fws, exists, modified, "modification");
}

// =========== code for the file_content_modified event ===========

/* Hashes the content of the file into digest. Returns false and sets errno
 * on failure. */
static bool HashFileContent(const char *path, unsigned char digest[EVP_MAX_MD_SIZE + 1])
{
    assert(path != NULL);

    FILE *file = safe_fopen(path, "rb");
    if (file == NULL)
    {
        return false;
    }

    EVP_MD_CTX *context = EVP_MD_CTX_create();
    if (context == NULL ||
        EVP_DigestInit_ex(context, HashDigestFromId(CONTENT_HASH_METHOD), NULL) != 1)
    {
        EVP_MD_CTX_destroy(context);
        fclose(file);
        errno = ENOMEM;
        return false;
    }

    unsigned char buffer[4096];
    size_t read_count;
    while ((read_count = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        EVP_DigestUpdate(context, buffer, read_count);
    }

    const bool success = !ferror(file);
    const int read_errno = errno;
    fclose(file);

    memset(digest, 0, EVP_MAX_MD_SIZE + 1);
    EVP_DigestFinal_ex(context, digest, NULL);
    EVP_MD_CTX_destroy(context);

    errno = read_errno;
    return success;
}

WatcherCheckResult FileWatcherCheckFileContentModified(void *state)
{
    assert(state != NULL);
    FileWatcherState *fws = state;

    bool exists;
    struct stat sb;
    if (!FileLstat(fws, &exists, &sb))
    {
        return WATCHER_CHECK_ERROR;
    }

    bool hashed = false;
    unsigned char digest[EVP_MAX_MD_SIZE + 1] = { 0 };
    if (exists && S_ISREG(sb.st_mode))
    {
        if (HashFileContent(fws->path, digest))
        {
            hashed = true;
        }
        else if (errno != ENOENT && errno != ENOTDIR) /* Not deleted since lstat() */
        {
            LogCheckError(fws, "hash", errno);
            return WATCHER_CHECK_ERROR;
        }
    }

    /* Only content changes of a regular file present at both checks */
    const bool modified = fws->last.content.hashed && hashed &&
        (memcmp(fws->last.content.digest, digest, sizeof(digest)) != 0);

    fws->last.content.hashed = hashed;
    memcpy(fws->last.content.digest, digest, sizeof(digest));
    return FileWatcherRecordObservation(fws, exists, modified, "content modification");
}

// =========== code for the file_permissions_changed event ===========

WatcherCheckResult FileWatcherCheckFilePermissionsChanged(void *state)
{
    assert(state != NULL);
    FileWatcherState *fws = state;

    bool exists;
    struct stat sb;
    if (!FileLstat(fws, &exists, &sb))
    {
        return WATCHER_CHECK_ERROR;
    }

    const mode_t mode = exists ? sb.st_mode : 0;

    /* Only changes of a file present at both checks */
    const bool changed = fws->last_exists && exists &&
        ((fws->last.mode & 07777) != (mode & 07777));

    fws->last.mode = mode;
    return FileWatcherRecordObservation(fws, exists, changed, "permissions change");
}

// =========== code for the file_owner_changed event ===========

WatcherCheckResult FileWatcherCheckFileOwnerChanged(void *state)
{
    assert(state != NULL);
    FileWatcherState *fws = state;

    bool exists;
    struct stat sb;
    if (!FileLstat(fws, &exists, &sb))
    {
        return WATCHER_CHECK_ERROR;
    }

    const uid_t uid = exists ? sb.st_uid : 0;
    const gid_t gid = exists ? sb.st_gid : 0;

    /* Only changes of a file present at both checks */
    const bool changed = fws->last_exists && exists &&
        ((fws->last.owner.uid != uid) || (fws->last.owner.gid != gid));

    fws->last.owner.uid = uid;
    fws->last.owner.gid = gid;
    return FileWatcherRecordObservation(fws, exists, changed, "owner change");
}

// =========== code for the file_size_exceeds event ===========

WatcherCheckResult FileWatcherCheckFileSizeExceeds(void *state)
{
    assert(state != NULL);
    FileWatcherState *fws = state;

    bool exists;
    struct stat sb;
    if (!FileLstat(fws, &exists, &sb))
    {
        return WATCHER_CHECK_ERROR;
    }

    const off_t size = exists ? sb.st_size : 0;

    /* Only when the size grows above the threshold, including a file created
     * above it, not every time it is checked above it */
    const bool exceeded = fws->last_exists && (fws->last.file_size.size > fws->last.file_size.threshold);
    const bool exceeds = exists && (size > fws->last.file_size.threshold);

    fws->last.file_size.size = size;
    return FileWatcherRecordObservation(fws, exists, !exceeded && exceeds, "size exceeding threshold");
}
