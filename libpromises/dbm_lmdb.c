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

/*
 * Implementation using LMDB API.
 */

#include <cf3.defs.h>
#include <dbm_priv.h>
#include <logging.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <file_lib.h>
#include <known_dirs.h>
#include <bootstrap.h>

#ifdef LMDB

#include <lmdb.h>
#include <repair.h>
#include <mutex.h>
#include <time.h>               /* time() */

// Shared between threads.
struct DBPriv_
{
    MDB_env *env;
    MDB_dbi dbi;
    // Used to keep track of transactions.
    // We set this to the transaction address when a thread creates a
    // transaction, and back to 0x0 when it is destroyed.
    pthread_key_t txn_key;
};

// Not shared between threads.
typedef struct DBTxn_
{
    MDB_txn *txn;
    // Whether txn is a read/write (true) or read-only (false) transaction.
    bool rw_txn;
    bool cursor_open;
} DBTxn;

struct DBCursorPriv_
{
    DBPriv *db;
    MDB_cursor *mc;
    MDB_val delkey;
    void *curkv;
    bool pending_delete;
};

static int DB_MAX_READERS = -1;

/******************************************************************************/

static int GetReadTransaction(DBPriv *db, DBTxn **txn)
{
    DBTxn *db_txn = pthread_getspecific(db->txn_key);
    int rc = MDB_SUCCESS;

    if (!db_txn)
    {
        db_txn = xcalloc(1, sizeof(DBTxn));
        pthread_setspecific(db->txn_key, db_txn);
    }

    if (!db_txn->txn)
    {
        rc = mdb_txn_begin(db->env, NULL, MDB_RDONLY, &db_txn->txn);
        if (rc != MDB_SUCCESS)
        {
            Log(LOG_LEVEL_ERR, "Unable to open read transaction in '%s': %s",
                (char *) mdb_env_get_userctx(db->env), mdb_strerror(rc));
        }
    }

    *txn = db_txn;

    return rc;
}

static int GetWriteTransaction(DBPriv *db, DBTxn **txn)
{
    DBTxn *db_txn = pthread_getspecific(db->txn_key);
    int rc = MDB_SUCCESS;

    if (!db_txn)
    {
        db_txn = xcalloc(1, sizeof(DBTxn));
        pthread_setspecific(db->txn_key, db_txn);
    }

    if (db_txn->txn && !db_txn->rw_txn)
    {
        rc = mdb_txn_commit(db_txn->txn);
        if (rc != MDB_SUCCESS)
        {
            Log(LOG_LEVEL_ERR, "Unable to close read-only transaction in '%s': %s",
                (char *) mdb_env_get_userctx(db->env), mdb_strerror(rc));
        }
        db_txn->txn = NULL;
    }

    if (!db_txn->txn)
    {
        rc = mdb_txn_begin(db->env, NULL, 0, &db_txn->txn);
        if (rc == MDB_SUCCESS)
        {
            db_txn->rw_txn = true;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Unable to open write transaction in '%s': %s",
                (char *) mdb_env_get_userctx(db->env), mdb_strerror(rc));
        }
    }

    *txn = db_txn;

    return rc;
}

static void AbortTransaction(DBPriv *db)
{
    DBTxn *db_txn = pthread_getspecific(db->txn_key);
    if (db_txn != NULL)
    {
        if (db_txn->txn != NULL)
        {
            mdb_txn_abort(db_txn->txn);
        }

        pthread_setspecific(db->txn_key, NULL);
        free(db_txn);
    }
}

static void DestroyTransaction(void *ptr)
{
    DBTxn *db_txn = (DBTxn *)ptr;
    UnexpectedError("Transaction object still exists when terminating thread");
    if (db_txn->txn)
    {
        UnexpectedError("Transaction still open when terminating thread!");
        mdb_txn_abort(db_txn->txn);
    }
    free(db_txn);
}

const char *DBPrivGetFileExtension(void)
{
    return "lmdb";
}

#ifndef LMDB_MAXSIZE
#define LMDB_MAXSIZE    104857600
#endif

void DBPrivSetMaximumConcurrentTransactions(int max_txn)
{
    DB_MAX_READERS = max_txn;
}

static int LmdbEnvOpen(MDB_env *env, const char *path, unsigned int flags, mdb_mode_t mode)
{
    /* There is a race condition in LMDB that will fail to open the database
     * environment if another process is opening it at the exact same time. This
     * condition is signaled by returning ENOENT, which we should never get
     * otherwise. This can lead to error messages on a heavily loaded machine,
     * so try to open it again after allowing other threads to finish their
     * opening process. */
    int attempts = 5;
    while (attempts-- > 0)
    {
        int rc = mdb_env_open(env, path, flags, mode);
        if (rc != ENOENT)
        {
            return rc;
        }

#if HAVE_DECL_SCHED_YIELD && defined(HAVE_SCHED_YIELD)
        // Not required for this to work, but makes it less likely that the race
        // condition will persist.
        sched_yield();
#endif
    }

    // Return EBUSY for an error message slightly more related to reality.
    return EBUSY;
}

/**
 * @warning Expects @fd_stamp to be locked.
 */
static bool RepairedAfterOpen(const char *lmdb_file, int fd_tstamp)
{
    time_t repaired_tstamp = -1;
    ssize_t n_read = read(fd_tstamp, &repaired_tstamp, sizeof(time_t));
    lseek(fd_tstamp, 0, SEEK_SET);
    if (n_read == 0)
    {
        /* EOF (empty file) => never repaired */
        Log(LOG_LEVEL_VERBOSE, "DB '%s' never repaired before", lmdb_file);
    }
    else if (n_read < sizeof(time_t))
    {
        /* error */
        Log(LOG_LEVEL_ERR, "Failed to read the timestamp of repair of the '%s' DB",
            lmdb_file);
    }
    else
    {
        /* read the timestamp => Check if the LMDB file was repaired after
         * we opened it last time. Or, IOW, if this is a new corruption or
         * an already-handled one. */
        DBHandle *handle = GetDBHandleFromFilename(lmdb_file);
        if (repaired_tstamp > GetDBOpenTimestamp(handle))
        {
            return true;
        }
    }
    return false;
}

static void HandleLMDBCorruption(MDB_env *env, const char *msg)
{
    const char *lmdb_file = mdb_env_get_userctx(env);
    Log(LOG_LEVEL_CRIT, "Corruption in the '%s' DB detected! %s",
        lmdb_file, msg);

    /* Freeze the DB ASAP. This also makes the call to exit() safe regarding
     * this particular DB because exit handlers will ignore it. */
    DBHandle *handle = GetDBHandleFromFilename(lmdb_file);
    FreezeDB(handle);

#ifdef __MINGW32__
    /* Not much we can do on Windows because there is no fork() and file locking
     * is also not so nice. */
    Log(LOG_LEVEL_WARNING, "Removing the corrupted DB file '%s'",
        lmdb_file);
    if (unlink(lmdb_file) != 0)
    {
        Log(LOG_LEVEL_CRIT, "Failed to remove the corrupted DB file '%s'",
            lmdb_file);
        exit(EC_CORRUPTION_REPAIR_FAILED);
    }
    exit(EC_CORRUPTION_REPAIRED);
#else
    /* Try to handle the corruption gracefully by repairing the LMDB file
     * (replacing it with a new LMDB file with all the data we managed to read
     * from the corrupted one). */

    /* To avoid two processes acting on the same corrupted file at once, file
     * locks are involved. Looking at OpenDBInstance() and DBPathLock()
     * in libpromises/db_api.c might also be useful.*/

    /* Only allow one thread at a time to handle DB corruption. File locks are
     * *process* specific so threads could step on each others toes. */
    ThreadLock(cft_db_corruption_lock);

    char *tstamp_file = StringFormat("%s.repaired", lmdb_file);
    char *db_lock_file = StringFormat("%s.lock", lmdb_file);

    int fd_tstamp = safe_open(tstamp_file, O_CREAT|O_RDWR);
    if (fd_tstamp == -1)
    {
        Log(LOG_LEVEL_CRIT, "Failed to open the '%s' DB repair timestamp file",
            lmdb_file);
        ThreadUnlock(cft_db_corruption_lock);
        free(db_lock_file);
        free(tstamp_file);

        exit(EC_CORRUPTION_REPAIR_FAILED);
    }
    int fd_db_lock = safe_open(db_lock_file, O_CREAT|O_RDWR);
    if (fd_db_lock == -1)
    {
        Log(LOG_LEVEL_CRIT, "Failed to open the '%s' DB lock file",
            lmdb_file);
        ThreadUnlock(cft_db_corruption_lock);
        close(fd_tstamp);
        free(db_lock_file);
        free(tstamp_file);

        exit(EC_CORRUPTION_REPAIR_FAILED);
    }

    int ret;
    bool handle_corruption = true;

    /* Make sure we are not holding the DB's lock (potentially needed by some
     * other process for the repair) to avoid deadlocks. */
    if (ExclusiveLockFileCheck(fd_db_lock))
    {
        Log(LOG_LEVEL_DEBUG, "Releasing lock on the '%s' DB", lmdb_file);
        ExclusiveUnlockFile(fd_db_lock); /* closes fd_db_lock (TODO: fix) */
        fd_db_lock = safe_open(db_lock_file, O_CREAT|O_RDWR);
    }

    ret = SharedLockFile(fd_tstamp, true);
    if (ret == 0)
    {
        if (RepairedAfterOpen(lmdb_file, fd_tstamp))
        {
            /* The corruption has already been handled. This process should
             * just die because we have no way to return to the point where
             * it would just open the new (repaired) LMDB file. */
            handle_corruption = false;
        }
        SharedUnlockFile(fd_tstamp); /* closes fd_tstamp (TODO: fix) */
        fd_tstamp = safe_open(tstamp_file, O_CREAT|O_RDWR);
    }
    else
    {
        /* should never happen (we tried to wait), but if it does, just log an
         * error and keep going */
        Log(LOG_LEVEL_ERR,
            "Failed to get shared lock for the repair timestamp of the '%s' DB",
            lmdb_file);
    }

    if (!handle_corruption)
    {
        /* Just clean after ourselves and terminate the process. */
        ThreadUnlock(cft_db_corruption_lock);
        close(fd_db_lock);
        close(fd_tstamp);
        free(db_lock_file);
        free(tstamp_file);

        exit(EC_CORRUPTION_REPAIRED);
    }

    /* HERE is a window for some other process to do the repair between when we
     * checked the timestamp using the shared lock above and the attempt to get
     * the exclusive lock right below. However, this is detected by checking the
     * contents of the timestamp file again below, while holding the EXCLUSIVE
     * lock. */

    ret = ExclusiveLockFile(fd_tstamp, true);
    if (ret == -1)
    {
        /* should never happen (we tried to wait), but if it does, just
         * terminate because doing the repair without the lock could be
         * disasterous */
        Log(LOG_LEVEL_ERR,
            "Failed to get shared lock for the repair timestamp of the '%s' DB",
            lmdb_file);

        ThreadUnlock(cft_db_corruption_lock);
        close(fd_db_lock);
        close(fd_tstamp);
        free(db_lock_file);
        free(tstamp_file);

        exit(EC_CORRUPTION_REPAIR_FAILED);
    }

    /* Cleared to resolve the corruption. */

    /* 1. Acquire the lock for the DB to prevent more processes trying to use
     *    it while it is corrupted (wait till the lock is available). */
    while (ExclusiveLockFile(fd_db_lock, false) == -1)
    {
        /* busy wait to do the logging */
        Log(LOG_LEVEL_INFO, "Waiting for the lock on the '%s' DB",
            lmdb_file);
        sleep(1);
    }

    /* 2. Check the last repair timestamp again (see the big "HERE..." comment
     *    above) */
    if (RepairedAfterOpen(lmdb_file, fd_tstamp))
    {
        /* Some other process repaired the DB since we checked last time,
         * nothing more to do here. */
        ThreadUnlock(cft_db_corruption_lock);
        close(fd_db_lock);      /* releases locks */
        close(fd_tstamp);       /* releases locks */
        free(db_lock_file);
        free(tstamp_file);

        exit(EC_CORRUPTION_REPAIRED);
    }

    /* 3. Repair the DB or at least move it out of the way. */
    /* repair_lmdb_file() forks so it is safe (file locks are not
     * inherited). */
    ret = repair_lmdb_file(lmdb_file);
    bool repair_successful = (ret == 0);
    if (repair_successful)
    {
        Log(LOG_LEVEL_NOTICE, "DB '%s' successfully repaired",
            lmdb_file);
    }
    else
    {
        Log(LOG_LEVEL_CRIT, "Failed to repair DB '%s', removing it", lmdb_file);
        if (unlink(lmdb_file) != 0)
        {
            Log(LOG_LEVEL_CRIT, "Failed to remove DB '%s'", lmdb_file);
        }
        else
        {
            /* We at least moved the file out of the way. */
            repair_successful = true;
        }
    }

    /* 4. Record the timestamp of the last repair. */
    if (repair_successful)
    {
        time_t this_timestamp = time(NULL);
        ssize_t n_written = write(fd_tstamp, &this_timestamp, sizeof(time_t));
        if (n_written != sizeof(time_t))
        {
            Log(LOG_LEVEL_ERR, "Failed to write the timestamp of repair of the '%s' DB",
                lmdb_file);
            /* should never happen, but if it does, keep moving */
        }
    }

    /* 5. Make the repaired DB available for others. Also release the locks
     *    in the opposite order in which they were acquired to avoid
     *    deadlocks. */
    if (ExclusiveUnlockFile(fd_db_lock) != 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to release the acquired lock for '%s'",
            db_lock_file);
    }

    /* 6. Signal that the repair is done (also closes fd_tstamp). */
    if (ExclusiveUnlockFile(fd_tstamp) != 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to release the acquired lock for '%s'",
            tstamp_file);
    }

    ThreadUnlock(cft_db_corruption_lock);
    free(db_lock_file);
    free(tstamp_file);
    /* fd_db_lock and fd_tstamp are already closed by the calls to
     * ExclusiveUnlockFile above. */

    if (repair_successful)
    {
        exit(EC_CORRUPTION_REPAIRED);
    }
    else
    {
        exit(EC_CORRUPTION_REPAIR_FAILED);
    }
#endif  /* __MINGW32__ */
}

DBPriv *DBPrivOpenDB(const char *dbpath, dbid id)
{
    DBPriv *db = xcalloc(1, sizeof(DBPriv));
    MDB_txn *txn = NULL;
    int rc;

    rc = pthread_key_create(&db->txn_key, &DestroyTransaction);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not create transaction key. (pthread_key_create: '%s')",
            GetErrorStrFromCode(rc));
        free(db);
        return NULL;
    }

    rc = mdb_env_create(&db->env);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not create handle for database %s: %s",
              dbpath, mdb_strerror(rc));
        goto err;
    }
    rc = mdb_env_set_userctx(db->env, xstrdup(dbpath));
    if (rc != MDB_SUCCESS)
    {
        Log(LOG_LEVEL_WARNING, "Could not store DB file path (%s) in the DB context",
            dbpath);
    }
    rc = mdb_env_set_assert(db->env, (MDB_assert_func*) HandleLMDBCorruption);
    if (rc != MDB_SUCCESS)
    {
        Log(LOG_LEVEL_WARNING, "Could not set the corruption handler for '%s'",
            dbpath);
    }
    rc = mdb_env_set_mapsize(db->env, LMDB_MAXSIZE);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not set mapsize for database %s: %s",
              dbpath, mdb_strerror(rc));
        goto err;
    }
    if (DB_MAX_READERS > 0)
    {
        rc = mdb_env_set_maxreaders(db->env, DB_MAX_READERS);
        if (rc)
        {
            Log(LOG_LEVEL_ERR, "Could not set maxreaders for database %s: %s",
                dbpath, mdb_strerror(rc));
            goto err;
        }
    }

    unsigned int open_flags = MDB_NOSUBDIR;
    if (id == dbid_locks
        || (GetAmPolicyHub() && id == dbid_lastseen))
    {
        open_flags |= MDB_NOSYNC;
    }

#ifdef __hpux
    /*
     * On HP-UX, a unified file cache was not introduced until version 11.31.
     * This means that on 11.23 there are separate file caches for mmap()'ed
     * files and open()'ed files. When these two are mixed, changes made using
     * one mode won't be immediately seen by the other mode, which is an
     * assumption LMDB is relying on. The MDB_WRITEMAP flag causes LMDB to use
     * mmap() only, so that we stay within one file cache.
     */
    open_flags |= MDB_WRITEMAP;
#endif

    rc = LmdbEnvOpen(db->env, dbpath, open_flags, 0644);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not open database %s: %s",
              dbpath, mdb_strerror(rc));
        goto err;
    }
    if (DB_MAX_READERS > 0)
    {
        int max_readers;
        rc = mdb_env_get_maxreaders(db->env, &max_readers);
        if (rc)
        {
            Log(LOG_LEVEL_ERR, "Could not get maxreaders for database %s: %s",
                dbpath, mdb_strerror(rc));
            goto err;
        }
        if (max_readers < DB_MAX_READERS)
        {
            // LMDB will only reinitialize maxreaders if no database handles are
            // open, including in other processes, which is how we might end up
            // here.
            Log(LOG_LEVEL_VERBOSE, "Failed to set LMDB max reader limit on database '%s', "
                "consider restarting CFEngine",
                dbpath);
        }
    }
    rc = mdb_txn_begin(db->env, NULL, MDB_RDONLY, &txn);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not open database txn %s: %s",
              dbpath, mdb_strerror(rc));
        goto err;
    }
    rc = mdb_open(txn, NULL, 0, &db->dbi);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not open database dbi %s: %s",
              dbpath, mdb_strerror(rc));
        mdb_txn_abort(txn);
        goto err;
    }
    rc = mdb_txn_commit(txn);
    if (rc)
    {
        Log(LOG_LEVEL_ERR, "Could not commit database dbi %s: %s",
              dbpath, mdb_strerror(rc));
        goto err;
    }

    return db;

err:
    if (db->env)
    {
        mdb_env_close(db->env);
    }
    pthread_key_delete(db->txn_key);
    free(db);
    if (rc == MDB_INVALID)
    {
        return DB_PRIV_DATABASE_BROKEN;
    }
    return NULL;
}

void DBPrivCloseDB(DBPriv *db)
{
    /* Abort LMDB transaction of the current thread. There should only be some
     * transaction open when the signal handler or atexit() hook is called. */
    AbortTransaction(db);

    char *db_path = mdb_env_get_userctx(db->env);
    if (db_path)
    {
        free(db_path);
    }
    if (db->env)
    {
        mdb_env_close(db->env);
    }

    pthread_key_delete(db->txn_key);
    free(db);
}

#define EMPTY_DB 0

bool DBPrivClean(DBPriv *db)
{
    DBTxn *txn;
    int rc = GetWriteTransaction(db, &txn);
    
    if (rc != MDB_SUCCESS)
    {
        Log(LOG_LEVEL_ERR, "Unable to get write transaction for '%s': %s",
            (char *) mdb_env_get_userctx(db->env), mdb_strerror(rc));
        return false;
    }
    
    assert(!txn->cursor_open);
    
    return mdb_drop(txn->txn, db->dbi, EMPTY_DB);
}

void DBPrivCommit(DBPriv *db)
{
    DBTxn *db_txn = pthread_getspecific(db->txn_key);
    if (db_txn && db_txn->txn)
    {
        assert(!db_txn->cursor_open);
        int rc = mdb_txn_commit(db_txn->txn);
        if (rc != MDB_SUCCESS)
        {
            Log(LOG_LEVEL_ERR, "Could not commit database transaction to '%s': %s",
                (char *) mdb_env_get_userctx(db->env), mdb_strerror(rc));
        }
    }
    pthread_setspecific(db->txn_key, NULL);
    free(db_txn);
}

bool DBPrivHasKey(DBPriv *db, const void *key, int key_size)
{
    MDB_val mkey, data;
    DBTxn *txn;
    int rc;
    // FIXME: distinguish between "entry not found" and "error occurred"

    rc = GetReadTransaction(db, &txn);
    if (rc == MDB_SUCCESS)
    {
        assert(!txn->cursor_open);
        mkey.mv_data = (void *)key;
        mkey.mv_size = key_size;
        rc = mdb_get(txn->txn, db->dbi, &mkey, &data);
        if (rc && rc != MDB_NOTFOUND)
        {
            Log(LOG_LEVEL_ERR, "Could not read database entry from '%s': %s",
                (char *) mdb_env_get_userctx(db->env), mdb_strerror(rc));
            AbortTransaction(db);
        }
    }

    return rc == MDB_SUCCESS;
}

int DBPrivGetValueSize(DBPriv *db, const void *key, int key_size)
{
    MDB_val mkey, data;
    DBTxn *txn;
    int rc;

    data.mv_size = 0;

    rc = GetReadTransaction(db, &txn);
    if (rc == MDB_SUCCESS)
    {
        assert(!txn->cursor_open);
        mkey.mv_data = (void *)key;
        mkey.mv_size = key_size;
        rc = mdb_get(txn->txn, db->dbi, &mkey, &data);
        if (rc && rc != MDB_NOTFOUND)
        {
            Log(LOG_LEVEL_ERR, "Could not read database entry from '%s': %s",
                (char *) mdb_env_get_userctx(db->env), mdb_strerror(rc));
            AbortTransaction(db);
        }
    }

    return data.mv_size;
}

bool DBPrivRead(DBPriv *db, const void *key, int key_size, void *dest, int dest_size)
{
    MDB_val mkey, data;
    DBTxn *txn;
    int rc;
    bool ret = false;

    rc = GetReadTransaction(db, &txn);
    if (rc == MDB_SUCCESS)
    {
        assert(!txn->cursor_open);
        mkey.mv_data = (void *)key;
        mkey.mv_size = key_size;
        rc = mdb_get(txn->txn, db->dbi, &mkey, &data);
        if (rc == MDB_SUCCESS)
        {
            if (dest_size > data.mv_size)
            {
                dest_size = data.mv_size;
            }
            memcpy(dest, data.mv_data, dest_size);
            ret = true;
        }
        else if (rc != MDB_NOTFOUND)
        {
            Log(LOG_LEVEL_ERR, "Could not read database entry from '%s': %s",
                (char *) mdb_env_get_userctx(db->env), mdb_strerror(rc));
            AbortTransaction(db);
        }
    }
    return ret;
}

bool DBPrivWrite(DBPriv *db, const void *key, int key_size, const void *value, int value_size)
{
    MDB_val mkey, data;
    DBTxn *txn;
    int rc = GetWriteTransaction(db, &txn);
    if (rc == MDB_SUCCESS)
    {
        assert(!txn->cursor_open);
        mkey.mv_data = (void *)key;
        mkey.mv_size = key_size;
        data.mv_data = (void *)value;
        data.mv_size = value_size;
        rc = mdb_put(txn->txn, db->dbi, &mkey, &data, 0);
        if (rc != MDB_SUCCESS)
        {
            Log(LOG_LEVEL_ERR, "Could not write database entry to '%s': %s",
                (char *) mdb_env_get_userctx(db->env), mdb_strerror(rc));
            AbortTransaction(db);
        }
    }
    return rc == MDB_SUCCESS;
}

bool DBPrivDelete(DBPriv *db, const void *key, int key_size)
{
    MDB_val mkey;
    DBTxn *txn;
    int rc = GetWriteTransaction(db, &txn);
    if (rc == MDB_SUCCESS)
    {
        assert(!txn->cursor_open);
        mkey.mv_data = (void *)key;
        mkey.mv_size = key_size;
        rc = mdb_del(txn->txn, db->dbi, &mkey, NULL);
        if (rc == MDB_NOTFOUND)
        {
            Log(LOG_LEVEL_DEBUG, "Entry not found in '%s': %s",
                (char *) mdb_env_get_userctx(db->env), mdb_strerror(rc));
        }
        else if (rc != MDB_SUCCESS)
        {
            Log(LOG_LEVEL_ERR, "Could not delete from '%s': %s",
                (char *) mdb_env_get_userctx(db->env), mdb_strerror(rc));
            AbortTransaction(db);
        }
    }
    return rc == MDB_SUCCESS;
}

DBCursorPriv *DBPrivOpenCursor(DBPriv *db)
{
    DBCursorPriv *cursor = NULL;
    DBTxn *txn;
    int rc;
    MDB_cursor *mc;

    rc = GetWriteTransaction(db, &txn);
    if (rc == MDB_SUCCESS)
    {
        assert(!txn->cursor_open);
        rc = mdb_cursor_open(txn->txn, db->dbi, &mc);
        if (rc == MDB_SUCCESS)
        {
            cursor = xcalloc(1, sizeof(DBCursorPriv));
            cursor->db = db;
            cursor->mc = mc;
            txn->cursor_open = true;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Could not open cursor in '%s': %s",
                (char *) mdb_env_get_userctx(db->env), mdb_strerror(rc));
            AbortTransaction(db);
        }
        /* txn remains with cursor */
    }

    return cursor;
}

bool DBPrivAdvanceCursor(DBCursorPriv *cursor, void **key, int *key_size,
                     void **value, int *value_size)
{
    MDB_val mkey, data;
    int rc;
    bool retval = false;

    if (cursor->curkv)
    {
        free(cursor->curkv);
        cursor->curkv = NULL;
    }
    if ((rc = mdb_cursor_get(cursor->mc, &mkey, &data, MDB_NEXT)) == MDB_SUCCESS)
    {
        // Align second buffer to 64-bit boundary, to avoid alignment errors on
        // certain platforms.
        size_t keybuf_size = mkey.mv_size;
        if (keybuf_size & 0x7)
        {
            keybuf_size += 8 - (keybuf_size % 8);
        }
        cursor->curkv = xmalloc(keybuf_size + data.mv_size);
        memcpy(cursor->curkv, mkey.mv_data, mkey.mv_size);
        *key = cursor->curkv;
        *key_size = mkey.mv_size;
        *value_size = data.mv_size;
        memcpy((char *)cursor->curkv+keybuf_size, data.mv_data, data.mv_size);
        *value = (char *)cursor->curkv + keybuf_size;
        retval = true;
    }
    else if (rc != MDB_NOTFOUND)
    {
        Log(LOG_LEVEL_ERR, "Could not advance cursor in '%s': %s",
            (char *) mdb_env_get_userctx(cursor->db->env), mdb_strerror(rc));
    }
    if (cursor->pending_delete)
    {
        int r2;
        /* Position on key to delete */
        r2 = mdb_cursor_get(cursor->mc, &cursor->delkey, NULL, MDB_SET);
        if (r2 == MDB_SUCCESS)
        {
            r2 = mdb_cursor_del(cursor->mc, 0);
        }
        /* Reposition the cursor if it was valid before */
        if (rc == MDB_SUCCESS)
        {
            mkey.mv_data = *key;
            rc = mdb_cursor_get(cursor->mc, &mkey, NULL, MDB_SET);
        }
        cursor->pending_delete = false;
    }
    return retval;
}

bool DBPrivDeleteCursorEntry(DBCursorPriv *cursor)
{
    int rc = mdb_cursor_get(cursor->mc, &cursor->delkey, NULL, MDB_GET_CURRENT);
    if (rc == MDB_SUCCESS)
    {
        cursor->pending_delete = true;
    }
    return rc == MDB_SUCCESS;
}

bool DBPrivWriteCursorEntry(DBCursorPriv *cursor, const void *value, int value_size)
{
    MDB_val data;
    int rc;

    cursor->pending_delete = false;
    data.mv_data = (void *)value;
    data.mv_size = value_size;

    if (cursor->curkv)
    {
        MDB_val curkey;
        curkey.mv_data = cursor->curkv;
        curkey.mv_size = sizeof(cursor->curkv);

        if ((rc = mdb_cursor_put(cursor->mc, &curkey, &data, MDB_CURRENT)) != MDB_SUCCESS)
        {
            Log(LOG_LEVEL_ERR, "Could not write cursor entry to '%s': %s",
                (char *) mdb_env_get_userctx(cursor->db->env), mdb_strerror(rc));
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Could not write cursor entry to '%s': cannot find current key",
            (char *) mdb_env_get_userctx(cursor->db->env));
        rc = MDB_INVALID;
    }
    return rc == MDB_SUCCESS;
}

void DBPrivCloseCursor(DBCursorPriv *cursor)
{
    DBTxn *txn;
    int rc = GetWriteTransaction(cursor->db, &txn);
    CF_ASSERT(rc == MDB_SUCCESS, "Could not get write transaction");
    CF_ASSERT(txn->cursor_open, "Transaction not open");
    txn->cursor_open = false;

    if (cursor->curkv)
    {
        free(cursor->curkv);
    }

    if (cursor->pending_delete)
    {
        mdb_cursor_del(cursor->mc, 0);
    }

    mdb_cursor_close(cursor->mc);
    free(cursor);
}

char *DBPrivDiagnose(const char *dbpath)
{
    return StringFormat("Unable to diagnose LMDB file (not implemented) for '%s'", dbpath);
}
#endif
