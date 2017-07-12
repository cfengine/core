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

/*
 * Implementation using QDBM
 */

#include <cf3.defs.h>

#include <dbm_api.h>
#include <dbm_priv.h>
#include <string_lib.h>

#ifdef QDB
# include <depot.h>

struct DBPriv_
{
    /*
     * This mutex controls the access to depot, which is not thread-aware
     */
    pthread_mutex_t lock;

    /*
     * This mutex prevents two cursors to be active on depot at same time, as
     * cursors are internal for QDBM. 'cursor_lock' is always taken before
     * 'lock' to avoid deadlocks.
     */
    pthread_mutex_t cursor_lock;

    DEPOT *depot;
};

struct DBCursorPriv_
{
    DBPriv *db;
    char *curkey;
    int curkey_size;
    char *curval;
};

/******************************************************************************/

static bool Lock(DBPriv *db)
{
    int ret = pthread_mutex_lock(&db->lock);
    if (ret != 0)
    {
        errno = ret;
        Log(LOG_LEVEL_ERR, "Unable to lock QDBM database. (pthread_mutex_lock: %s)", GetErrorStr());
        return false;
    }
    return true;
}

static void Unlock(DBPriv *db)
{
    int ret = pthread_mutex_unlock(&db->lock);
    if (ret != 0)
    {
        errno = ret;
        Log(LOG_LEVEL_ERR, "Unable to unlock QDBM database. (pthread_mutex_unlock: %s)", GetErrorStr());
    }
}

static bool LockCursor(DBPriv *db)
{
    int ret = pthread_mutex_lock(&db->cursor_lock);
    if (ret != 0)
    {
        errno = ret;
        Log(LOG_LEVEL_ERR, "Unable to obtain cursor lock for QDBM database. (pthread_mutex_lock: %s)", GetErrorStr());
        return false;
    }
    return true;
}

static void UnlockCursor(DBPriv *db)
{
    int ret = pthread_mutex_unlock(&db->cursor_lock);
    if (ret != 0)
    {
        errno = ret;
        Log(LOG_LEVEL_ERR, "Unable to release cursor lock for QDBM database. (pthread_mutex_unlock: %s)", GetErrorStr());
    }
}

const char *DBPrivGetFileExtension(void)
{
    return "qdbm";
}

void DBPrivSetMaximumConcurrentTransactions(ARG_UNUSED int max_txn)
{
}

DBPriv *DBPrivOpenDB(const char *filename, ARG_UNUSED dbid id)
{
    DBPriv *db = xcalloc(1, sizeof(DBPriv));

    pthread_mutex_init(&db->lock, NULL);
    pthread_mutex_init(&db->cursor_lock, NULL);

    db->depot = dpopen(filename, DP_OWRITER | DP_OCREAT, -1);

    if ((db->depot == NULL) && (dpecode == DP_EBROKEN))
    {
        Log(LOG_LEVEL_ERR, "Database '%s' is broken, trying to repair...", filename);

        if (dprepair(filename))
        {
            Log(LOG_LEVEL_INFO, "Successfully repaired database '%s'", filename);
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Failed to repair database '%s', recreating...", filename);
            return DB_PRIV_DATABASE_BROKEN;
        }

        db->depot = dpopen(filename, DP_OWRITER | DP_OCREAT, -1);
    }

    if (db->depot == NULL)
    {
        Log(LOG_LEVEL_ERR, "dpopen: Opening database '%s' failed. (dpopen: %s)",
              filename, dperrmsg(dpecode));
        pthread_mutex_destroy(&db->cursor_lock);
        pthread_mutex_destroy(&db->lock);
        free(db);
        return NULL;
    }

    return db;
}

void DBPrivCloseDB(DBPriv *db)
{
    int ret;

    if ((ret = pthread_mutex_destroy(&db->lock)) != 0)
    {
        errno = ret;
        Log(LOG_LEVEL_ERR, "Lock is still active during QDBM database handle close. (pthread_mutex_destroy: %s)", GetErrorStr());
    }

    if ((ret = pthread_mutex_destroy(&db->cursor_lock)) != 0)
    {
        errno = ret;
        Log(LOG_LEVEL_ERR, "Cursor lock is still active during QDBM database handle close. (pthread_mutex_destroy: %s)", GetErrorStr());
    }

    if (!dpclose(db->depot))
    {
        Log(LOG_LEVEL_ERR, "Unable to close QDBM database. (dpclose: %s)", dperrmsg(dpecode));
    }

    free(db);
}

void DBPrivCommit(ARG_UNUSED DBPriv *db)
{
}

bool DBPrivClean(DBPriv *db)
{
    if (!Lock(db))
    {
        return false;
    }
    
    if (!dpiterinit(db->depot))
    {
        Log(LOG_LEVEL_ERR, "Could not initialize QuickDB iterator. (dpiterinit: %s)", dperrmsg(dpecode));
        Unlock(db);
        return false;
    }
    
    char *key = NULL;
    while((key = dpiternext(db->depot, NULL)))
    {
        dpout(db->depot, key, -1);
    }
    
    Unlock(db);
    return true;
}

bool DBPrivRead(DBPriv *db, const void *key, int key_size, void *dest, int dest_size)
{
    if (!Lock(db))
    {
        return false;
    }

    if (dpgetwb(db->depot, key, key_size, 0, dest_size, dest) == -1)
    {
        // FIXME: distinguish between "entry not found" and "failure to read"

        Log(LOG_LEVEL_DEBUG, "QDBM DBPrivRead: Could not read '%s', (dpgetwb: %s)",
                (const char *)key, dperrmsg(dpecode));

        Unlock(db);
        return false;
    }

    Unlock(db);
    return true;
}

bool DBPrivWrite(DBPriv *db, const void *key, int key_size, const void *value, int value_size)
{
    if (!Lock(db))
    {
        return false;
    }

    if (!dpput(db->depot, key, key_size, value, value_size, DP_DOVER))
    {
        char *db_name = dpname(db->depot);
        Log(LOG_LEVEL_ERR, "Could not write key to DB '%s'. (dpput: %s)",
              db_name, dperrmsg(dpecode));
        free(db_name);
        Unlock(db);
        return false;
    }

    Unlock(db);
    return true;
}

bool DBPrivHasKey(DBPriv *db, const void *key, int key_size)
{
    if (!Lock(db))
    {
        return false;
    }

    int ret = dpvsiz(db->depot, key, key_size) != -1;

    Unlock(db);
    return ret;
}

int DBPrivGetValueSize(DBPriv *db, const void *key, int key_size)
{
    if (!Lock(db))
    {
        return false;
    }

    int ret = dpvsiz(db->depot, key, key_size);

    Unlock(db);
    return ret;
}

bool DBPrivDelete(DBPriv *db, const void *key, int key_size)
{
    if (!Lock(db))
    {
        return false;
    }

    /* dpout returns false both for error and if key is not found */
    if (!dpout(db->depot, key, key_size) && dpecode != DP_ENOITEM)
    {
        Unlock(db);
        return false;
    }

    Unlock(db);
    return true;
}

DBCursorPriv *DBPrivOpenCursor(DBPriv *db)
{
    DBCursorPriv *cursor = DBPrivOpenCursor(db);
    
    if (!cursor)
    {
        return false;
    }
    
    void *key;
    int key_size;
    void *value;
    int value_size;
    
    while ((DBPrivAdvanceCursor(cursor, &key, &key_size, &value, &value_size)))
    {
        DBPrivDeleteCursorEntry(cursor);
    }
    
    DBPrivCloseCursor(cursor);
    
    return true;
}

bool DBPrivAdvanceCursor(DBCursorPriv *cursor, void **key, int *ksize, void **value, int *vsize)
{
    if (!Lock(cursor->db))
    {
        return false;
    }

    free(cursor->curkey);
    free(cursor->curval);

    cursor->curkey = NULL;
    cursor->curval = NULL;

    *key = dpiternext(cursor->db->depot, ksize);

    if (*key == NULL)
    {
        /* Reached the end of database */
        Unlock(cursor->db);
        return false;
    }

    *value = dpget(cursor->db->depot, *key, *ksize, 0, -1, vsize);

    // keep pointers for later free
    cursor->curkey = *key;
    cursor->curkey_size = *ksize;
    cursor->curval = *value;

    Unlock(cursor->db);
    return true;
}

bool DBPrivDeleteCursorEntry(DBCursorPriv *cursor)
{
    return DBPrivDelete(cursor->db, cursor->curkey, cursor->curkey_size);
}

bool DBPrivWriteCursorEntry(DBCursorPriv *cursor, const void *value, int value_size)
{
    return DBPrivWrite(cursor->db, cursor->curkey, cursor->curkey_size, value, value_size);
}

void DBPrivCloseCursor(DBCursorPriv *cursor)
{
    DBPriv *db = cursor->db;

    /* FIXME: communicate the deadlock if happens */
    Lock(db);

    free(cursor->curkey);
    free(cursor->curval);
    free(cursor);

    Unlock(db);
    /* Cursor lock was obtained in DBPrivOpenCursor */
    UnlockCursor(db);
}

char *DBPrivDiagnose(const char *dbpath)
{
    return StringFormat("Unable to diagnose QuickDB file (not implemented) for '%s'", dbpath);
}

#endif
