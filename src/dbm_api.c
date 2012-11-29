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

#include "dbm_api.h"
#include "dbm_priv.h"
#include "dbm_lib.h"
#include "dbm_migration.h"
#include "atexit.h"
#include "cfstream.h"

#include <assert.h>

/******************************************************************************/

struct DBHandle_
{
    /* Filename of database file */
    char *filename;

    /* Actual database-specific data */
    DBPriv *priv;

    int refcount;

    /* This lock protects initialization of .priv element, and .refcount manipulation */
    pthread_mutex_t lock;
};

struct DBCursor_
{
    DBCursorPriv *cursor;
};

/******************************************************************************/

/*
 * This lock protects on-demand initialization of db_handles[i].lock and
 * db_handles[i].name.
 */
static pthread_mutex_t db_handles_lock = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

static DBHandle db_handles[dbid_max] = { { 0 } };

static pthread_once_t db_shutdown_once = PTHREAD_ONCE_INIT;

/******************************************************************************/

static const char *DB_PATHS[] = {
    [dbid_classes] = "cf_classes",
    [dbid_variables] = "state/cf_variables",
    [dbid_performance] = "performance",
    [dbid_checksums] = "checksum_digests",
    [dbid_filestats] = "stats",
    [dbid_observations] = "state/cf_observations",
    [dbid_state] = "state/cf_state",
    [dbid_lastseen] = "cf_lastseen",
    [dbid_audit] = "cf_audit",
    [dbid_locks] = "state/cf_lock",
    [dbid_history] = "state/history",
    [dbid_measure] = "state/nova_measures",
    [dbid_static] = "state/nova_static",
    [dbid_scalars] = "state/nova_pscalar",
    [dbid_promise_compliance] = "state/promise_compliance",
    [dbid_windows_registry] = "mswin",
    [dbid_cache] = "nova_cache",
    [dbid_license] = "nova_track",
    [dbid_value] = "nova_value",
    [dbid_agent_execution] = "nova_agent_execution",
    [dbid_bundles] = "bundles",
};

/******************************************************************************/

static char *DBIdToPath(dbid id)
{
    assert(DB_PATHS[id] != NULL);

    char *filename;
    if (xasprintf(&filename, "%s/%s.%s",
                  CFWORKDIR, DB_PATHS[id], DBPrivGetFileExtension()) == -1)
    {
        FatalError("Unable to construct database filename for file %s", DB_PATHS[id]);
    }

    char *native_filename = MapNameCopy(filename);
    free(filename);

    return native_filename;
}

static DBHandle *DBHandleGet(int id)
{
    assert(id >= 0 && id < dbid_max);

    pthread_mutex_lock(&db_handles_lock);

    if (db_handles[id].filename == NULL)
    {
        db_handles[id].filename = DBIdToPath(id);
        pthread_mutex_init(&db_handles[id].lock, NULL);
    }

    pthread_mutex_unlock(&db_handles_lock);

    return &db_handles[id];
}

/* Closes all open DB handles */
static void CloseAllDB(void)
{
    pthread_mutex_lock(&db_handles_lock);

    for (int i = 0; i < dbid_max; ++i)
    {
        if (db_handles[i].refcount != 0)
        {
            DBPrivCloseDB(db_handles[i].priv);
        }

        /*
         * CloseAllDB is called just before exit(3), but clean up
         * nevertheless.
         */
        db_handles[i].refcount = 0;

        if (db_handles[i].filename)
        {
            free(db_handles[i].filename);
            db_handles[i].filename = NULL;

            int ret = pthread_mutex_destroy(&db_handles[i].lock);
            if (ret != 0)
            {
                errno = ret;
                CfOut(cf_error, "pthread_mutex_destroy",
                      "Unable to close database %s", DB_PATHS[i]);
            }
        }
    }

    pthread_mutex_unlock(&db_handles_lock);
}

static void RegisterShutdownHandler(void)
{
    RegisterAtExitFunction(&CloseAllDB);
}

bool OpenDB(DBHandle **dbp, dbid id)
{
    DBHandle *handle = DBHandleGet(id);

    pthread_mutex_lock(&handle->lock);

    if (handle->refcount == 0)
    {
        int lock_fd = DBPathLock(handle->filename);

        if(lock_fd != -1)
        {
            handle->priv = DBPrivOpenDB(handle->filename);
            DBPathUnLock(lock_fd);
        }

        if (handle->priv)
        {
            if (!DBMigrate(handle, id))
            {
                DBPrivCloseDB(handle->priv);
                handle->priv = NULL;
            }
        }
    }

    if (handle->priv)
    {
        handle->refcount++;
        *dbp = handle;

        /* Only register shutdown handler if any database was opened
         * correctly. Otherwise this shutdown caller may be called too early,
         * and shutdown handler installed by the database library may end up
         * being called before CloseAllDB function */

        pthread_once(&db_shutdown_once, RegisterShutdownHandler);
    }
    else
    {
        *dbp = NULL;
    }

    pthread_mutex_unlock(&handle->lock);

    return *dbp != NULL;
}

void CloseDB(DBHandle *handle)
{
    pthread_mutex_lock(&handle->lock);

    if (handle->refcount < 1)
    {
        CfOut(cf_error, "", "Trying to close database %s which is not open", handle->filename);
    }
    else if (--handle->refcount == 0)
    {
        DBPrivCloseDB(handle->priv);
    }

    pthread_mutex_unlock(&handle->lock);
}

/*****************************************************************************/

bool ReadComplexKeyDB(DBHandle *handle, const char *key, int key_size,
                      void *dest, int dest_size)
{
    return DBPrivRead(handle->priv, key, key_size, dest, dest_size);
}

bool WriteComplexKeyDB(DBHandle *handle, const char *key, int key_size,
                       const void *value, int value_size)
{
    return DBPrivWrite(handle->priv, key, key_size, value, value_size);
}

bool DeleteComplexKeyDB(DBHandle *handle, const char *key, int key_size)
{
    return DBPrivDelete(handle->priv, key, key_size);
}

bool ReadDB(DBHandle *handle, const char *key, void *dest, int destSz)
{
    return DBPrivRead(handle->priv, key, strlen(key) + 1, dest, destSz);
}

bool WriteDB(DBHandle *handle, const char *key, const void *src, int srcSz)
{
    return DBPrivWrite(handle->priv, key, strlen(key) + 1, src, srcSz);
}

bool HasKeyDB(DBHandle *handle, const char *key, int key_size)
{
    return DBPrivHasKey(handle->priv, key, key_size);
}

int ValueSizeDB(DBHandle *handle, const char *key, int key_size)
{
    return DBPrivGetValueSize(handle->priv, key, key_size);
}

bool DeleteDB(DBHandle *handle, const char *key)
{
    return DBPrivDelete(handle->priv, key, strlen(key) + 1);
}

bool NewDBCursor(DBHandle *handle, DBCursor **cursor)
{
    DBCursorPriv *priv = DBPrivOpenCursor(handle->priv);
    if (!priv)
    {
        return false;
    }

    *cursor = xcalloc(1, sizeof(DBCursor));
    (*cursor)->cursor = priv;
    return true;
}

bool NextDB(DBHandle *handle, DBCursor *cursor, char **key, int *ksize,
            void **value, int *vsize)
{
    return DBPrivAdvanceCursor(cursor->cursor, (void **)key, ksize, value, vsize);
}

bool DBCursorDeleteEntry(DBCursor *cursor)
{
    return DBPrivDeleteCursorEntry(cursor->cursor);
}

bool DBCursorWriteEntry(DBCursor *cursor, const void *value, int value_size)
{
    return DBPrivWriteCursorEntry(cursor->cursor, value, value_size);
}

bool DeleteDBCursor(DBHandle *handle, DBCursor *cursor)
{
    DBPrivCloseCursor(cursor->cursor);
    free(cursor);
    return true;
}
