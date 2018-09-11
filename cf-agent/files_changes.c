/*
   Copyright 2018 Northern.tech AS

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

#include <files_changes.h>
#include <sequence.h>
#include <hash.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <file_lib.h>
#include <dbm_api.h>
#include <promises.h>
#include <actuator.h>
#include <eval_context.h>
#include <files_hashes.h>
#include <known_dirs.h>

/*
  The format of the changes database is as follows:

         Key:   |            Value:
  "D_<path>"    | "<basename>\0<basename>\0..." (SORTED!)
                |
  "H_<hash_key> | "<hash>\0"
                |
  "S_<path>     | "<struct stat>"

  Explanation:

  - The "D" entry contains all the filenames that have been recorded in that
    directory, stored as the basename.
  - The "H" entry records the hash of a file.
  - The "S" entry records the stat information of a file.
*/

#define CHANGES_HASH_STRING_LEN 7
#define CHANGES_HASH_FILE_NAME_OFFSET  CHANGES_HASH_STRING_LEN+1

typedef struct
{
    unsigned char mess_digest[EVP_MAX_MD_SIZE + 1];     /* Content digest */
} ChecksumValue;

static bool GetDirectoryListFromDatabase(CF_DB *db, const char * path, Seq *files);
static bool FileChangesSetDirectoryList(CF_DB *db, const char *path, const Seq *files);

/*
 * Key format:
 *
 * 7 bytes    hash name, \0 padded at right
 * 1 byte     \0
 * N bytes    pathname
 */
static char *NewIndexKey(char type, const char *name, int *size)
{
    char *chk_key;

// "H_" plus pathname plus index_str in one block + \0

    const size_t len = strlen(name);
    *size = len + CHANGES_HASH_FILE_NAME_OFFSET + 3;

    chk_key = xcalloc(1, *size);

// Data start after offset for index

    strlcpy(chk_key, "H_", 2);
    strlcpy(chk_key + 2, HashNameFromId(type), CHANGES_HASH_STRING_LEN);
    memcpy(chk_key + 2 + CHANGES_HASH_FILE_NAME_OFFSET, name, len);
    return chk_key;
}

static void DeleteIndexKey(char *key)
{
    free(key);
}

static ChecksumValue *NewHashValue(unsigned char digest[EVP_MAX_MD_SIZE + 1])
{
    ChecksumValue *chk_val;

    chk_val = xcalloc(1, sizeof(ChecksumValue));

    memcpy(chk_val->mess_digest, digest, EVP_MAX_MD_SIZE + 1);

    return chk_val;
}

static void DeleteHashValue(ChecksumValue *chk_val)
{
    free(chk_val);
}

static int ReadHash(CF_DB *dbp, HashMethod type, const char *name, unsigned char digest[EVP_MAX_MD_SIZE + 1])
{
    char *key;
    int size;
    ChecksumValue chk_val;

    key = NewIndexKey(type, name, &size);

    if (ReadComplexKeyDB(dbp, key, size, (void *) &chk_val, sizeof(ChecksumValue)))
    {
        memcpy(digest, chk_val.mess_digest, EVP_MAX_MD_SIZE + 1);
        DeleteIndexKey(key);
        return true;
    }
    else
    {
        DeleteIndexKey(key);
        return false;
    }
}

static int WriteHash(CF_DB *dbp, HashMethod type, const char *name, unsigned char digest[EVP_MAX_MD_SIZE + 1])
{
    char *key;
    ChecksumValue *value;
    int ret, keysize;

    key = NewIndexKey(type, name, &keysize);
    value = NewHashValue(digest);
    ret = WriteComplexKeyDB(dbp, key, keysize, value, sizeof(ChecksumValue));
    DeleteIndexKey(key);
    DeleteHashValue(value);
    return ret;
}

static void DeleteHash(CF_DB *dbp, HashMethod type, const char *name)
{
    int size;
    char *key;

    key = NewIndexKey(type, name, &size);
    DeleteComplexKeyDB(dbp, key, size);
    DeleteIndexKey(key);
}

static void AddMigratedFileToDirectoryList(CF_DB *changes_db, const char *file, const char *common_msg)
{
    // This is incredibly inefficient, since we add files to the list one by one,
    // but the migration only ever needs to be done once for each host.
    size_t file_len = strlen(file);
    char dir[file_len + 1];
    strcpy(dir, file);
    const char *basefile;
    char *last_slash = strrchr(dir, '/');

    if (last_slash == NULL)
    {
        Log(LOG_LEVEL_ERR, "%s: Invalid file entry: '%s'", common_msg, dir);
        return;
    }

    if (last_slash == dir)
    {
        // If we only have one slash, it is the root dir, so we need to have
        // dir be equal to "/". We cannot both have that, and let basefile
        // point to the the next component in dir (since the first character
        // will then be '\0'), so point to the original file buffer instead.
        dir[1] = '\0';
        basefile = file + 1;
    }
    else
    {
        basefile = last_slash + 1;
        *last_slash = '\0';
    }

    Seq *files = SeqNew(1, free);
    if (!GetDirectoryListFromDatabase(changes_db, dir, files))
    {
        Log(LOG_LEVEL_ERR, "%s: Not able to get directory index", common_msg);
        SeqDestroy(files);
        return;
    }

    if (SeqBinaryIndexOf(files, basefile, (SeqItemComparator)strcmp) == -1)
    {
        SeqAppend(files, xstrdup(basefile));
        SeqSort(files, (SeqItemComparator)strcmp, NULL);
        if (!FileChangesSetDirectoryList(changes_db, dir, files))
        {
            Log(LOG_LEVEL_ERR, "%s: Not able to update directory index", common_msg);
        }
    }

    SeqDestroy(files);
}

static bool MigrateOldChecksumDatabase(CF_DB *changes_db)
{
    CF_DB *old_db;

    const char *common_msg = "While converting old checksum database to new format";

    if (!OpenDB(&old_db, dbid_checksums))
    {
        Log(LOG_LEVEL_ERR, "%s: Could not open database.", common_msg);
        return false;
    }

    CF_DBC *cursor;
    if (!NewDBCursor(old_db, &cursor))
    {
        Log(LOG_LEVEL_ERR, "%s: Could not open database cursor.", common_msg);
        CloseDB(old_db);
        return false;
    }

    char *key;
    int ksize;
    char *value;
    int vsize;
    while (NextDB(cursor, &key, &ksize, (void **)&value, &vsize))
    {
        char new_key[ksize + 2];
        new_key[0] = 'H';
        new_key[1] = '_';
        memcpy(new_key + 2, key, ksize);
        if (!WriteComplexKeyDB(changes_db, new_key, sizeof(new_key), value, vsize))
        {
            Log(LOG_LEVEL_ERR, "%s: Could not write file checksum to database", common_msg);
            // Keep trying for other keys.
        }
        AddMigratedFileToDirectoryList(changes_db, key + CHANGES_HASH_FILE_NAME_OFFSET, common_msg);
    }

    DeleteDBCursor(cursor);
    CloseDB(old_db);

    return true;
}

static bool MigrateOldStatDatabase(CF_DB *changes_db)
{
    CF_DB *old_db;

    const char *common_msg = "While converting old filestat database to new format";

    if (!OpenDB(&old_db, dbid_filestats))
    {
        Log(LOG_LEVEL_ERR, "%s: Could not open database.", common_msg);
        return false;
    }

    CF_DBC *cursor;
    if (!NewDBCursor(old_db, &cursor))
    {
        Log(LOG_LEVEL_ERR, "%s: Could not open database cursor.", common_msg);
        CloseDB(old_db);
        return false;
    }

    char *key;
    int ksize;
    char *value;
    int vsize;
    while (NextDB(cursor, &key, &ksize, (void **)&value, &vsize))
    {
        char new_key[ksize + 2];
        new_key[0] = 'S';
        new_key[1] = '_';
        memcpy(new_key + 2, key, ksize);
        if (!WriteComplexKeyDB(changes_db, new_key, sizeof(new_key), value, vsize))
        {
            Log(LOG_LEVEL_ERR, "%s: Could not write filestat to database", common_msg);
            // Keep trying for other keys.
        }
        AddMigratedFileToDirectoryList(changes_db, key, common_msg);
    }

    DeleteDBCursor(cursor);
    CloseDB(old_db);

    return true;
}

static bool OpenChangesDB(CF_DB **db)
{
    if (!OpenDB(db, dbid_changes))
    {
        Log(LOG_LEVEL_ERR, "Could not open changes database");
        return false;
    }

    struct stat statbuf;
    char *old_checksums_db = DBIdToPath(dbid_checksums);
    char *old_filestats_db = DBIdToPath(dbid_filestats);

    if (stat(old_checksums_db, &statbuf) != -1)
    {
        Log(LOG_LEVEL_INFO, "Migrating checksum database");
        MigrateOldChecksumDatabase(*db);
        char migrated_db_name[PATH_MAX];
        snprintf(migrated_db_name, sizeof(migrated_db_name), "%s.cf-migrated", old_checksums_db);
        Log(LOG_LEVEL_INFO, "After checksum database migration: Renaming '%s' to '%s'",
            old_checksums_db, migrated_db_name);
        if (rename(old_checksums_db, migrated_db_name) != 0)
        {
            Log(LOG_LEVEL_ERR, "Could not rename '%s' to '%s'", old_checksums_db, migrated_db_name);
        }
    }

    if (stat(old_filestats_db, &statbuf) != -1)
    {
        Log(LOG_LEVEL_INFO, "Migrating filestat database");
        MigrateOldStatDatabase(*db);
        char migrated_db_name[PATH_MAX];
        snprintf(migrated_db_name, sizeof(migrated_db_name), "%s.cf-migrated", old_filestats_db);
        Log(LOG_LEVEL_INFO, "After filestat database migration: Renaming '%s' to '%s'",
            old_filestats_db, migrated_db_name);
        if (rename(old_filestats_db, migrated_db_name) != 0)
        {
            Log(LOG_LEVEL_ERR, "Could not rename '%s' to '%s'", old_filestats_db, migrated_db_name);
        }
    }

    free(old_checksums_db);
    free(old_filestats_db);

    return true;
}

static void RemoveAllFileTraces(CF_DB *db, const char *path)
{
    for (int c = 0; c < HASH_METHOD_NONE; c++)
    {
        DeleteHash(db, c, path);
    }
    char key[strlen(path) + 3];
    xsnprintf(key, sizeof(key), "S_%s", path);
    DeleteDB(db, key);
}

static bool GetDirectoryListFromDatabase(CF_DB *db, const char *path, Seq *files)
{
    char key[strlen(path) + 3];
    xsnprintf(key, sizeof(key), "D_%s", path);
    if (!HasKeyDB(db, key, sizeof(key)))
    {
        // Not an error, so successful, but seq remains unchanged.
        return true;
    }
    int size = ValueSizeDB(db, key, sizeof(key));
    if (size <= 0)
    {
        // Shouldn't happen, since we don't store empty lists, but play it safe
        // and return empty seq.
        return true;
    }

    char raw_entries[size];
    if (!ReadDB(db, key, raw_entries, size))
    {
        Log(LOG_LEVEL_ERR, "Could not read changes database entry");
        return false;
    }

    char *raw_entries_end = raw_entries + size;
    for (char *pos = raw_entries; pos < raw_entries_end;)
    {
        char *null_pos = memchr(pos, '\0', raw_entries_end - pos);
        if (!null_pos)
        {
            Log(LOG_LEVEL_ERR, "Unexpected end of value in changes database");
            return false;
        }

        SeqAppend(files, xstrdup(pos));
        pos = null_pos + 1;
    }

    return true;
}

bool FileChangesGetDirectoryList(const char *path, Seq *files)
{
    CF_DB *db;
    if (!OpenChangesDB(&db))
    {
        Log(LOG_LEVEL_ERR, "Could not open changes database");
        return false;
    }

    bool result = GetDirectoryListFromDatabase(db, path, files);
    CloseDB(db);
    return result;
}

static bool FileChangesSetDirectoryList(CF_DB *db, const char *path, const Seq *files)
{
    int size = 0;
    int no_files = SeqLength(files);

    char key[strlen(path) + 3];
    xsnprintf(key, sizeof(key), "D_%s", path);

    if (no_files == 0)
    {
        DeleteDB(db, key);
        return true;
    }

    for (int c = 0; c < no_files; c++)
    {
        size += strlen(SeqAt(files, c)) + 1;
    }

    char raw_entries[size];
    char *pos = raw_entries;
    for (int c = 0; c < no_files; c++)
    {
        strcpy(pos, SeqAt(files, c));
        pos += strlen(pos) + 1;
    }

    if (!WriteDB(db, key, raw_entries, size))
    {
        Log(LOG_LEVEL_ERR, "Could not write to changes database");
        return false;
    }

    return true;
}

/* Returns false if filename never seen before, and adds a checksum
   to the database. Returns true if hashes do not match and also
   updates database to the new value if update is true */
bool FileChangesCheckAndUpdateHash_impl(const char *filename,
                                        unsigned char digest[EVP_MAX_MD_SIZE + 1],
                                        HashMethod type,
                                        bool update,
                                        const Promise *pp,
                                        PromiseResult *result)
{
    const int size = HashSizeFromId(type);
    unsigned char dbdigest[EVP_MAX_MD_SIZE + 1];
    CF_DB *dbp;
    bool found;
    bool different;
    bool ret = false;

    if (!OpenChangesDB(&dbp))
    {
        Log(LOG_LEVEL_ERR, "Unable to open the hash database!");
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        return false;
    }

    if (ReadHash(dbp, type, filename, dbdigest))
    {
        found = true;
        different = (memcmp(digest, dbdigest, size) != 0);
        if (different)
        {
            Log(LOG_LEVEL_NOTICE, "Hash '%s' for '%s' changed!", HashNameFromId(type), filename);
            if (pp->comment)
            {
                Log(LOG_LEVEL_VERBOSE, "Preceding promise '%s'", pp->comment);
            }
        }
    }
    else
    {
        found = false;
        different = true;
    }

    if (different)
    {
        if ((!found || update) && !DONTDO)
        {
            const char *action = found ? "Updating" : "Storing";
            char buffer[CF_HOSTKEY_STRING_SIZE];
            Log(LOG_LEVEL_NOTICE, "%s %s hash for '%s' (%s)", action,
                HashNameFromId(type), filename,
                HashPrintSafe(buffer, sizeof(buffer), digest, type, true));
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);

            WriteHash(dbp, type, filename, digest);
            ret = found;
        }
        else
        {
            Log(LOG_LEVEL_NOTICE, "Hash for file '%s' changed", filename);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            ret = true;
        }
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "File hash for %s is correct", filename);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_NOOP);
        ret = false;
    }

    CloseDB(dbp);
    return ret;
}

bool FileChangesCheckAndUpdateHash(EvalContext *ctx,
                                   const char *filename,
                                   unsigned char digest[EVP_MAX_MD_SIZE + 1],
                                   HashMethod type,
                                   const Attributes *attr,
                                   const Promise *pp,
                                   PromiseResult *result)
{
    assert(attr != NULL);
    bool ret = FileChangesCheckAndUpdateHash_impl(filename, digest, type, attr->change.update, pp, result);
    // TODO: Move cfPS even further up the call stack.
    cfPS(ctx, LOG_LEVEL_DEBUG, *result, pp, *attr, "Updating promise status for files changes promise");
    return ret;
}

void FileChangesLogNewFile(const char *path, const Promise *pp)
{
    Log(LOG_LEVEL_NOTICE, "New file '%s' found", path);
    FileChangesLogChange(path, FILE_STATE_NEW, "New file found", pp);
}

// db_file_set should already be sorted.
void FileChangesCheckAndUpdateDirectory(const char *name, const Seq *file_set, const Seq *db_file_set,
                                        bool update, const Promise *pp, PromiseResult *result)
{
    CF_DB *db;
    if (!OpenChangesDB(&db))
    {
        Log(LOG_LEVEL_ERR, "Could not open changes database");
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        return;
    }

    Seq *disk_file_set = SeqSoftSort(file_set, (SeqItemComparator)strcmp, NULL);

    // We'll traverse the union of disk_file_set and db_file_set in merged order.

    int num_files = SeqLength(disk_file_set);
    int num_db_files = SeqLength(db_file_set);
    bool changed = false;
    for (int disk_pos = 0, db_pos = 0; disk_pos < num_files || db_pos < num_db_files;)
    {
        int compare_result;
        if (disk_pos >= num_files)
        {
            compare_result = 1;
        }
        else if (db_pos >= num_db_files)
        {
            compare_result = -1;
        }
        else
        {
            compare_result = strcmp(SeqAt(disk_file_set, disk_pos), SeqAt(db_file_set, db_pos));
        }

        if (compare_result < 0)
        {
            /*
              We would have called this here, but we assume that DepthSearch()
              has already done it for us. The reason is that calling it here
              produces a very unnatural order, with all stat and content
              changes, as well as all subdirectories, appearing in the log
              before the message about a new file. This is because we save the
              list for last and *then* compare it to the saved directory list,
              *after* traversing the tree. So we let DepthSearch() do it while
              traversing instead. Removed files will still be listed last.
            */
#if 0
            char *file = SeqAt(disk_file_set, disk_pos);
            char path[strlen(name) + strlen(file) + 2];
            xsnprintf(path, sizeof(path), "%s/%s", name, file);
            FileChangesLogNewFile(path, pp);
#endif

            changed = true;
            disk_pos++;
        }
        else if (compare_result > 0)
        {
            char *db_file = SeqAt(db_file_set, db_pos);
            char path[strlen(name) + strlen(db_file) + 2];
            xsnprintf(path, sizeof(path), "%s/%s", name, db_file);

            Log(LOG_LEVEL_NOTICE, "File '%s' no longer exists", path);
            FileChangesLogChange(path, FILE_STATE_REMOVED, "File removed", pp);

            RemoveAllFileTraces(db, path);

            changed = true;
            db_pos++;
        }
        else
        {
            // DB file entry and filesystem file entry matched.
            disk_pos++;
            db_pos++;
        }
    }

    PromiseResult newres;
    if (!changed)
    {
        newres = PROMISE_RESULT_NOOP;
    }
    else if (update && FileChangesSetDirectoryList(db, name, disk_file_set))
    {
        newres = PROMISE_RESULT_CHANGE;
    }
    else
    {
        newres = PROMISE_RESULT_FAIL;
    }
    *result = PromiseResultUpdate(*result, newres);

    SeqSoftDestroy(disk_file_set);
    CloseDB(db);
}

void FileChangesCheckAndUpdateStats(const char *file, struct stat *sb, bool update, const Promise *pp)
{
    struct stat cmpsb;
    CF_DB *dbp;

    if (!OpenChangesDB(&dbp))
    {
        return;
    }

    char key[strlen(file) + 3];
    xsnprintf(key, sizeof(key), "S_%s", file);

    if (!ReadDB(dbp, key, &cmpsb, sizeof(struct stat)))
    {
        if (!DONTDO)
        {
            if (!WriteDB(dbp, key, sb, sizeof(struct stat)))
            {
                Log(LOG_LEVEL_ERR, "Could not write stat information to database");
            }
            CloseDB(dbp);
            return;
        }
    }

    if (cmpsb.st_mode == sb->st_mode
        && cmpsb.st_uid == sb->st_uid
        && cmpsb.st_gid == sb->st_gid
        && cmpsb.st_dev == sb->st_dev
        && cmpsb.st_ino == sb->st_ino
        && cmpsb.st_mtime == sb->st_mtime)
    {
        CloseDB(dbp);
        return;
    }

    if (cmpsb.st_mode != sb->st_mode)
    {
        Log(LOG_LEVEL_NOTICE, "Permissions for '%s' changed %04jo -> %04jo",
                 file, (uintmax_t)cmpsb.st_mode, (uintmax_t)sb->st_mode);

        char msg_temp[CF_MAXVARSIZE];
        snprintf(msg_temp, sizeof(msg_temp), "Permission: %04jo -> %04jo",
                 (uintmax_t)cmpsb.st_mode, (uintmax_t)sb->st_mode);

        FileChangesLogChange(file, FILE_STATE_STATS_CHANGED, msg_temp, pp);
    }

    if (cmpsb.st_uid != sb->st_uid)
    {
        Log(LOG_LEVEL_NOTICE, "Owner for '%s' changed %ju -> %ju",
            file, (uintmax_t) cmpsb.st_uid, (uintmax_t) sb->st_uid);

        char msg_temp[CF_MAXVARSIZE];
        snprintf(msg_temp, sizeof(msg_temp), "Owner: %ju -> %ju",
                 (uintmax_t) cmpsb.st_uid, (uintmax_t) sb->st_uid);

        FileChangesLogChange(file, FILE_STATE_STATS_CHANGED, msg_temp, pp);
    }

    if (cmpsb.st_gid != sb->st_gid)
    {
        Log(LOG_LEVEL_NOTICE, "Group for '%s' changed %ju -> %ju",
            file, (uintmax_t) cmpsb.st_gid, (uintmax_t) sb->st_gid);

        char msg_temp[CF_MAXVARSIZE];
        snprintf(msg_temp, sizeof(msg_temp), "Group: %ju -> %ju",
                 (uintmax_t)cmpsb.st_gid, (uintmax_t)sb->st_gid);

        FileChangesLogChange(file, FILE_STATE_STATS_CHANGED, msg_temp, pp);
    }

    if (cmpsb.st_dev != sb->st_dev)
    {
        Log(LOG_LEVEL_NOTICE, "Device for '%s' changed %ju -> %ju",
            file, (uintmax_t) cmpsb.st_dev, (uintmax_t) sb->st_dev);

        char msg_temp[CF_MAXVARSIZE];
        snprintf(msg_temp, sizeof(msg_temp), "Device: %ju -> %ju",
                 (uintmax_t)cmpsb.st_dev, (uintmax_t)sb->st_dev);

        FileChangesLogChange(file, FILE_STATE_STATS_CHANGED, msg_temp, pp);
    }

    if (cmpsb.st_ino != sb->st_ino)
    {
        Log(LOG_LEVEL_NOTICE, "inode for '%s' changed %ju -> %ju",
            file, (uintmax_t) cmpsb.st_ino, (uintmax_t) sb->st_ino);
    }

    if (cmpsb.st_mtime != sb->st_mtime)
    {
        char from[CF_MAXVARSIZE];
        char to[CF_MAXVARSIZE];

        strcpy(from, ctime(&(cmpsb.st_mtime)));
        strcpy(to, ctime(&(sb->st_mtime)));
        Chop(from, CF_MAXVARSIZE);
        Chop(to, CF_MAXVARSIZE);
        Log(LOG_LEVEL_NOTICE, "Last modified time for '%s' changed '%s' -> '%s'", file, from, to);

        char msg_temp[CF_MAXVARSIZE];
        snprintf(msg_temp, sizeof(msg_temp), "Modified time: %s -> %s",
                 from, to);

        FileChangesLogChange(file, FILE_STATE_STATS_CHANGED, msg_temp, pp);
    }

    if (pp->comment)
    {
        Log(LOG_LEVEL_VERBOSE, "Preceding promise '%s'", pp->comment);
    }

    if (update && !DONTDO)
    {
        if (!DeleteDB(dbp, key) || !WriteDB(dbp, key, sb, sizeof(struct stat)))
        {
            Log(LOG_LEVEL_ERR, "Could not write stat information to database");
        }
    }

    CloseDB(dbp);
}

static char FileStateToChar(FileState status)
{
    switch(status)
    {
    case FILE_STATE_NEW:
        return 'N';

    case FILE_STATE_REMOVED:
        return 'R';

    case FILE_STATE_CONTENT_CHANGED:
        return 'C';

    case FILE_STATE_STATS_CHANGED:
        return 'S';

    default:
        ProgrammingError("Unhandled file status in switch: %d", status);
    }
}

void FileChangesLogChange(const char *file, FileState status, char *msg, const Promise *pp)
{
    FILE *fp;
    char fname[CF_BUFSIZE];
    time_t now = time(NULL);

/* This is inefficient but we don't want to lose any data */

    snprintf(fname, CF_BUFSIZE, "%s/%s", GetStateDir(), CF_FILECHANGE_NEW);
    MapName(fname);

#ifndef __MINGW32__
    struct stat sb;
    if (stat(fname, &sb) != -1)
    {
        if (sb.st_mode & (S_IWGRP | S_IWOTH))
        {
            Log(LOG_LEVEL_ERR, "File '%s' (owner %ju) was writable by others (security exception)", fname, (uintmax_t)sb.st_uid);
        }
    }
#endif /* !__MINGW32__ */

    fp = safe_fopen(fname, "a");
    if (fp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not write to the hash change log. (fopen: %s)", GetErrorStr());
        return;
    }

    const char *handle = PromiseID(pp);

    fprintf(fp, "%lld,%s,%s,%c,%s\n", (long long) now, handle, file, FileStateToChar(status), msg);
    fclose(fp);

    safe_chmod(fname, 0600);
}
