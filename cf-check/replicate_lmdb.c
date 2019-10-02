#include <logging.h>
#include <diagnose.h>
#include <replicate_lmdb.h>
#include <string_lib.h>

#ifndef LMDB
int replicate_lmdb(const char *s_file, const char *d_file)
{
    Log(LOG_LEVEL_ERR, "Database replication only available for LMDB");
    return 1;
}

#else

#include <lmdb.h>

typedef struct {
    const char *s_file;
    const char *d_file;
    MDB_txn *s_txn;
    MDB_txn *d_txn;
} LMDBReplicationInfo;

static void report_mdb_error(const char *db_file, const char *op, int rc)
{
    Log(LOG_LEVEL_ERR, "%s: %s error(%d): %s\n",
        db_file, op, rc, mdb_strerror(rc));
}

static void HandleSrcLMDBCorruption(MDB_env *env, const char *msg)
{
    LMDBReplicationInfo *info = mdb_env_get_userctx(env);
    Log(LOG_LEVEL_ERR, "Corruption in the source DB '%s' detected! %s",
        info->s_file, msg);
    mdb_env_set_assert(env, (MDB_assert_func*) NULL);
    if (info->d_txn != NULL)
    {
        mdb_txn_commit(info->d_txn);
    }
    if (info->s_txn != NULL)
    {
        mdb_txn_abort(info->s_txn);
    }

    /* remove files that LMDB creates behind our back */
    char *garbage_file = StringFormat("%s-lock", info->d_file);
    unlink(garbage_file);
    free(garbage_file);

    exit(CF_CHECK_LMDB_CORRUPT_PAGE);
}

static void HandleDstLMDBCorruption(MDB_env *env, const char *msg)
{
    LMDBReplicationInfo *info = mdb_env_get_userctx(env);
    Log(LOG_LEVEL_ERR, "Corruption in the new DB '%s' detected! %s",
        info->d_file, msg);
    mdb_env_set_assert(env, (MDB_assert_func*) NULL);
    if (info->d_txn != NULL)
    {
        mdb_txn_abort(info->d_txn);
    }
    if (info->s_txn != NULL)
    {
        mdb_txn_abort(info->s_txn);
    }

    /* remove files that LMDB creates behind our back */
    char *garbage_file = StringFormat("%s-lock", info->d_file);
    unlink(garbage_file);
    free(garbage_file);

    /* This should actually never happen -- how can there be a corruption in a
     * freshly created LMDB where we are just inserting data? Hence,
     * UNKNOWN error. */
    exit(CF_CHECK_UNKNOWN);
}

/**
 * Replicate an LMDB file by reading it's entries and writing them into a new LMDB file.
 *
 * @return  CFCheckCode code
 * @WARNING This function can exit the calling process in case of an LMDB
 *          operation failure.
 */
int replicate_lmdb(const char *s_file, const char *d_file)
{
    MDB_env *s_env = NULL;
    MDB_txn *s_txn = NULL;
    MDB_dbi s_dbi;
    bool close_s_dbi = false;
    MDB_cursor *s_cursor = NULL;
    MDB_env *d_env = NULL;
    MDB_txn *d_txn = NULL;
    MDB_dbi d_dbi;
    bool close_d_dbi = false;
    MDB_cursor *d_cursor = NULL;
    LMDBReplicationInfo info = { s_file, d_file, NULL, NULL };

    int ret = 0;
    int rc;

    Log(LOG_LEVEL_INFO, "Replicating '%s' into '%s'", s_file, d_file);

    rc = mdb_env_create(&s_env);
    if (rc != 0)
    {
        ret = rc;
        report_mdb_error(s_file, "mdb_env_create", rc);
        goto cleanup;
    }
    mdb_env_set_userctx(s_env, &info);
    mdb_env_set_assert(s_env, (MDB_assert_func*) HandleSrcLMDBCorruption);

    /* MDB_NOTLS -- Don't use Thread-Local Storage
     * Should allow us to use multiple transactions from the same thread (only
     * one read-write). */
    rc = mdb_env_open(s_env, s_file, MDB_NOSUBDIR | MDB_RDONLY | MDB_NOTLS, 0600);
    if (rc != 0)
    {
        ret = rc;
        report_mdb_error(s_file, "mdb_env_open", rc);
        goto cleanup;
    }

    rc = mdb_txn_begin(s_env, NULL, MDB_RDONLY, &s_txn);
    if (rc != 0)
    {
        ret = rc;
        report_mdb_error(s_file, "mdb_txn_begin", rc);
        goto cleanup;
    }
    info.s_txn = s_txn;

    rc = mdb_dbi_open(s_txn, NULL, 0, &s_dbi);
    if (rc != 0)
    {
        ret = rc;
        report_mdb_error(s_file, "mdb_dbi_open", rc);
        goto cleanup;
    }
    else
    {
        close_s_dbi = true;
    }

    rc = mdb_cursor_open(s_txn, s_dbi, &s_cursor);
    if (rc != 0)
    {
        ret = rc;
        report_mdb_error(s_file, "mdb_cursor_open", rc);
        goto cleanup;
    }

    rc = mdb_env_create(&d_env);
    if (rc != 0)
    {
        ret = rc;
        report_mdb_error(d_file, "mdb_env_create", rc);
        goto cleanup;
    }
    mdb_env_set_userctx(d_env, &info);
    mdb_env_set_assert(d_env, (MDB_assert_func*) HandleDstLMDBCorruption);

    rc = mdb_env_open(d_env, d_file, MDB_NOSUBDIR | MDB_NOTLS, 0600);
    if (rc != 0)
    {
        ret = rc;
        report_mdb_error(d_file, "mdb_env_open", rc);
        goto cleanup;
    }

    rc = mdb_txn_begin(d_env, NULL, 0, &d_txn);
    if (rc != 0)
    {
        ret = rc;
        report_mdb_error(d_file, "mdb_txn_begin", rc);
        goto cleanup;
    }
    info.d_txn = d_txn;

    rc = mdb_dbi_open(d_txn, NULL, MDB_CREATE, &d_dbi);
    if (rc != 0)
    {
        ret = rc;
        report_mdb_error(d_file, "mdb_dbi_open", rc);
        goto cleanup;
    }
    else
    {
        close_d_dbi = true;
    }

    rc = mdb_cursor_open(d_txn, d_dbi, &d_cursor);
    if (rc != 0)
    {
        ret = rc;
        report_mdb_error(d_file, "mdb_cursor_open", rc);
        goto cleanup;
    }

    rc = MDB_SUCCESS;
    while (rc == MDB_SUCCESS)
    {
        MDB_val key, data;

        rc = mdb_cursor_get(s_cursor, &key, &data, MDB_NEXT);
        /* MDB_NOTFOUND => no more data */
        if ((rc != MDB_SUCCESS) && (rc != MDB_NOTFOUND))
        {
            report_mdb_error(s_file, "mdb_cursor_get", rc);
            ret = rc;
        }
        if (rc == MDB_SUCCESS)
        {
            rc = mdb_put(d_txn, d_dbi, &key, &data, 0);
            if (rc != MDB_SUCCESS)
            {
                report_mdb_error(d_file, "mdb_put", rc);
                ret = rc;
            }
        }
    }
    mdb_txn_commit(d_txn);
    d_txn = NULL;
    info.d_txn = NULL;

  cleanup:
    if (s_cursor != NULL)
    {
        mdb_cursor_close(s_cursor);
    }
    if (close_s_dbi)
    {
        mdb_close(s_env, s_dbi);
    }
    if (s_txn != NULL)
    {
        mdb_txn_abort(s_txn);
    }
    if (s_env != NULL)
    {
        mdb_env_close(s_env);
    }

    if (d_cursor != NULL)
    {
        mdb_cursor_close(d_cursor);
    }
    if (close_d_dbi)
    {
        mdb_close(d_env, d_dbi);
    }
    if (d_txn != NULL)
    {
        mdb_txn_abort(d_txn);
    }
    if (d_env != NULL)
    {
        mdb_env_close(d_env);
    }

    /* remove files that LMDB creates behind our back */
    char *garbage_file = StringFormat("%s-lock", d_file);
    unlink(garbage_file);
    free(garbage_file);

    ret = lmdb_errno_to_cf_check_code(ret);
    return ret;
}
#endif  /* LMDB */
