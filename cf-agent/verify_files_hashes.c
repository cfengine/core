/*
   Copyright (C) CFEngine AS

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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "verify_files_hashes.h"

#include "rlist.h"
#include "policy.h"
#include "client_code.h"
#include "files_interfaces.h"
#include "files_lib.h"
#include "files_hashes.h"
#include "misc_lib.h"
#include "env_context.h"

/*
 * Key format:
 *
 * 7 bytes    hash name, \0 padded at right
 * 1 byte     \0
 * N bytes    filename
 */
static char *NewIndexKey(char type, char *name, int *size)
{
    char *chk_key;

// Filename plus index_str in one block + \0

    *size = strlen(name) + CF_INDEX_OFFSET + 1;

    chk_key = xcalloc(1, *size);

// Data start after offset for index

    strncpy(chk_key, FileHashName(type), CF_INDEX_FIELD_LEN);
    strncpy(chk_key + CF_INDEX_OFFSET, name, strlen(name));
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

/* memcpy(chk_val->attr_digest,attr,EVP_MAX_MD_SIZE+1); depricated */

    return chk_val;
}

static void DeleteHashValue(ChecksumValue *chk_val)
{
    free((char *) chk_val);
}

static int ReadHash(CF_DB *dbp, HashMethod type, char *name, unsigned char digest[EVP_MAX_MD_SIZE + 1])
{
    char *key;
    int size;
    ChecksumValue chk_val;

    key = NewIndexKey(type, name, &size);

    if (ReadComplexKeyDB(dbp, key, size, (void *) &chk_val, sizeof(ChecksumValue)))
    {
        memset(digest, 0, EVP_MAX_MD_SIZE + 1);
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

static int WriteHash(CF_DB *dbp, HashMethod type, char *name, unsigned char digest[EVP_MAX_MD_SIZE + 1])
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

static void DeleteHash(CF_DB *dbp, HashMethod type, char *name)
{
    int size;
    char *key;

    key = NewIndexKey(type, name, &size);
    DeleteComplexKeyDB(dbp, key, size);
    DeleteIndexKey(key);
}


/* Returns false if filename never seen before, and adds a checksum
   to the database. Returns true if hashes do not match and also potentially
   updates database to the new value */

int FileHashChanged(EvalContext *ctx, char *filename, unsigned char digest[EVP_MAX_MD_SIZE + 1], HashMethod type,
                    Attributes attr, Promise *pp)
{
    int i, size = 21;
    unsigned char dbdigest[EVP_MAX_MD_SIZE + 1];
    CF_DB *dbp;
    char buffer[EVP_MAX_MD_SIZE * 4];

    size = FileHashSize(type);

    if (!OpenDB(&dbp, dbid_checksums))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "Unable to open the hash database!");
        return false;
    }

    if (ReadHash(dbp, type, filename, dbdigest))
    {
        for (i = 0; i < size; i++)
        {
            if (digest[i] != dbdigest[i])
            {
                Log(LOG_LEVEL_ERR, "Hash '%s' for '%s' changed!", FileHashName(type), filename);

                if (pp->comment)
                {
                    Log(LOG_LEVEL_ERR, "Preceding promise: %s", pp->comment);
                }

                if (attr.change.update)
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_CHANGE, pp, attr, "Updating hash for %s to %s", filename,
                         HashPrintSafe(type, digest, buffer));

                    DeleteHash(dbp, type, filename);
                    WriteHash(dbp, type, filename, digest);
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, attr, "!! Hash for file \"%s\" changed", filename);
                }

                CloseDB(dbp);
                return true;
            }
        }

        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, attr, "File hash for %s is correct", filename);
        CloseDB(dbp);
        return false;
    }
    else
    {
        /* Key was not found, so install it */
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_CHANGE, pp, attr, "File '%s' was not in '%s' database - new file found", filename,
             FileHashName(type));
        Log(LOG_LEVEL_DEBUG, "Storing checksum for '%s' in database '%s'", filename, HashPrintSafe(type, digest, buffer));
        WriteHash(dbp, type, filename, digest);

        LogHashChange(filename, FILE_STATE_NEW, "New file found", pp);

        CloseDB(dbp);
        return false;
    }
}

int CompareFileHashes(char *file1, char *file2, struct stat *sstat, struct stat *dstat, FileCopy fc, AgentConnection *conn)
{
    unsigned char digest1[EVP_MAX_MD_SIZE + 1] = { 0 }, digest2[EVP_MAX_MD_SIZE + 1] = { 0 };
    int i;

    if (sstat->st_size != dstat->st_size)
    {
        Log(LOG_LEVEL_DEBUG, "File sizes differ, no need to compute checksum");
        return true;
    }

    if ((fc.servers == NULL) || (strcmp(fc.servers->item, "localhost") == 0))
    {
        HashFile(file1, digest1, CF_DEFAULT_DIGEST);
        HashFile(file2, digest2, CF_DEFAULT_DIGEST);

        for (i = 0; i < EVP_MAX_MD_SIZE; i++)
        {
            if (digest1[i] != digest2[i])
            {
                return true;
            }
        }

        Log(LOG_LEVEL_DEBUG, "Files were identical");
        return false;           /* only if files are identical */
    }
    else
    {
        return CompareHashNet(file1, file2, fc.encrypt, conn);  /* client.c */
    }
}

int CompareBinaryFiles(char *file1, char *file2, struct stat *sstat, struct stat *dstat, FileCopy fc, AgentConnection *conn)
{
    int fd1, fd2, bytes1, bytes2;
    char buff1[BUFSIZ], buff2[BUFSIZ];

    if (sstat->st_size != dstat->st_size)
    {
        Log(LOG_LEVEL_DEBUG, "File sizes differ, no need to compute checksum");
        return true;
    }

    if ((fc.servers == NULL) || (strcmp(fc.servers->item, "localhost") == 0))
    {
        fd1 = open(file1, O_RDONLY | O_BINARY, 0400);
        fd2 = open(file2, O_RDONLY | O_BINARY, 0400);

        do
        {
            bytes1 = read(fd1, buff1, BUFSIZ);
            bytes2 = read(fd2, buff2, BUFSIZ);

            if ((bytes1 != bytes2) || (memcmp(buff1, buff2, bytes1) != 0))
            {
                Log(LOG_LEVEL_VERBOSE, "Binary Comparison mismatch...");
                close(fd2);
                close(fd1);
                return true;
            }
        }
        while (bytes1 > 0);

        close(fd2);
        close(fd1);

        return false;           /* only if files are identical */
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "Using network checksum instead");
        return CompareHashNet(file1, file2, fc.encrypt, conn);  /* client.c */
    }
}

void PurgeHashes(EvalContext *ctx, char *path, Attributes attr, Promise *pp)
/* Go through the database and purge records about non-existent files */
{
    CF_DB *dbp;
    CF_DBC *dbcp;
    struct stat statbuf;
    int ksize, vsize;
    char *key;
    void *value;

    if (!OpenDB(&dbp,dbid_checksums))
    {
        return;
    }

    if (path)
    {
        if (stat(path, &statbuf) == -1)
        {
            DeleteDB(dbp, path);
        }
        CloseDB(dbp);
        return;
    }

/* Acquire a cursor for the database. */

    if (!NewDBCursor(dbp, &dbcp))
    {
        Log(LOG_LEVEL_INFO, "Unable to scan hash database");
        CloseDB(dbp);
        return;
    }

    /* Walk through the database and print out the key/data pairs. */

    while (NextDB(dbcp, &key, &ksize, &value, &vsize))
    {
        char *obj = (char *) key + CF_INDEX_OFFSET;

        if (stat(obj, &statbuf) == -1)
        {
            if (attr.change.update)
            {
                DBCursorDeleteEntry(dbcp);
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, attr, "ALERT: File %s no longer exists!", obj);
            }

            LogHashChange(obj, FILE_STATE_REMOVED, "File removed", pp);
        }

        memset(&key, 0, sizeof(key));
        memset(&value, 0, sizeof(value));
    }

    DeleteDBCursor(dbcp);
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

void LogHashChange(char *file, FileState status, char *msg, Promise *pp)
{
    FILE *fp;
    char fname[CF_BUFSIZE];
    time_t now = time(NULL);
    mode_t perm = 0600;
    static char prevFile[CF_MAXVARSIZE] = { 0 };

// we might get called twice..
    if (strcmp(file, prevFile) == 0)
    {
        return;
    }

    strlcpy(prevFile, file, CF_MAXVARSIZE);

/* This is inefficient but we don't want to lose any data */

    snprintf(fname, CF_BUFSIZE, "%s/state/%s", CFWORKDIR, CF_FILECHANGE_NEW);
    MapName(fname);

#ifndef __MINGW32__
    struct stat sb;
    if (stat(fname, &sb) != -1)
    {
        if (sb.st_mode & (S_IWGRP | S_IWOTH))
        {
            Log(LOG_LEVEL_ERR, "File %s (owner %ju) is writable by others (security exception)", fname, (uintmax_t)sb.st_uid);
        }
    }
#endif /* !__MINGW32__ */

    if ((fp = fopen(fname, "a")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not write to the hash change log. (fopen: %s)", GetErrorStr());
        return;
    }

    const char *handle = PromiseID(pp);

    fprintf(fp, "%ld,%s,%s,%c,%s\n", (long) now, handle, file, FileStateToChar(status), msg);
    fclose(fp);

    chmod(fname, perm);
}
