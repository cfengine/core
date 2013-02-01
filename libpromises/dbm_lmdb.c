/*

   Copyright (C) Howard Chu @ Symas Corp.

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

/*
 * Implementation using OpenLDAP Lightning MDB
 */

#include "cf3.defs.h"
#include "dbm_priv.h"
#include "dbm_lib.h"
#include "cfstream.h"

#ifdef LMDB

# include <lmdb.h>

struct DBPriv_
{
	MDB_env *env;
	MDB_dbi dbi;
};

struct DBCursorPriv_
{
    DBPriv *db;
	MDB_cursor *mc;
	MDB_val delkey;
	bool pending_delete;
};

/******************************************************************************/

const char *DBPrivGetFileExtension(void)
{
    return "lmdb";
}

#ifndef LMDB_MAXSIZE
#define LMDB_MAXSIZE	104857600
#endif

DBPriv *DBPrivOpenDB(const char *dbpath)
{
    DBPriv *db = xcalloc(1, sizeof(DBPriv));
	MDB_txn *txn = NULL;
	int rc;

	rc = mdb_env_create(&db->env);
	if (rc) {
        CfOut(cf_error, "", "!! Could not create handle for database %s: %s",
              dbpath, mdb_strerror(rc));
		goto err;
	}
	rc = mdb_env_set_mapsize(db->env, LMDB_MAXSIZE);
	if (rc) {
        CfOut(cf_error, "", "!! Could not set mapsize for database %s: %s",
              dbpath, mdb_strerror(rc));
		goto err;
	}
	rc = mdb_env_open(db->env, dbpath, MDB_NOSUBDIR, 0664);
	if (rc) {
        CfOut(cf_error, "", "!! Could not open database %s: %s",
              dbpath, mdb_strerror(rc));
		goto err;
	}
	rc = mdb_txn_begin(db->env, NULL, MDB_RDONLY, &txn);
	if (rc) {
        CfOut(cf_error, "", "!! Could not open database txn %s: %s",
              dbpath, mdb_strerror(rc));
		goto err;
	}
	rc = mdb_open(txn, NULL, 0, &db->dbi);
	if (rc) {
        CfOut(cf_error, "", "!! Could not open database dbi %s: %s",
              dbpath, mdb_strerror(rc));
		goto err;
	}
	rc = mdb_txn_commit(txn);
	if (rc) {
        CfOut(cf_error, "", "!! Could not commit database dbi %s: %s",
              dbpath, mdb_strerror(rc));
		goto err;
	}
	txn = NULL;

    return db;

err:
	if (db->env)
		mdb_env_close(db->env);
    free(db);
    return NULL;
}

void DBPrivCloseDB(DBPriv *db)
{
	if (db->env)
		mdb_env_close(db->env);
    free(db);
}

bool DBPrivHasKey(DBPriv *db, const void *key, int key_size)
{
	MDB_val mkey, data;
	MDB_txn *txn;
	int rc;
    // FIXME: distinguish between "entry not found" and "error occured"

	rc = mdb_txn_begin(db->env, NULL, MDB_RDONLY, &txn);
	if (rc == MDB_SUCCESS) {
		mkey.mv_data = (void *)key;
		mkey.mv_size = key_size;
		rc = mdb_get(txn, db->dbi, &mkey, &data);
		if (rc && rc != MDB_NOTFOUND) {
			CfOut(cf_error, "", "!! could not read: %s", mdb_strerror(rc));
		}
		mdb_txn_abort(txn);
	} else {
		CfOut(cf_error, "", "!! could not create read txn: %s", mdb_strerror(rc));
	}

    return rc == MDB_SUCCESS;
}

int DBPrivGetValueSize(DBPriv *db, const void *key, int key_size)
{
	MDB_val mkey, data;
	MDB_txn *txn;
	int rc;

	data.mv_size = 0;

	rc = mdb_txn_begin(db->env, NULL, MDB_RDONLY, &txn);
	if (rc == MDB_SUCCESS) {
		mkey.mv_data = (void *)key;
		mkey.mv_size = key_size;
		rc = mdb_get(txn, db->dbi, &mkey, &data);
		if (rc && rc != MDB_NOTFOUND) {
			CfOut(cf_error, "", "!! could not read: %s", mdb_strerror(rc));
		}
		mdb_txn_abort(txn);
	} else {
		CfOut(cf_error, "", "!! could not create read txn: %s", mdb_strerror(rc));
	}

    return data.mv_size;
}

bool DBPrivRead(DBPriv *db, const void *key, int key_size, void *dest, int dest_size)
{
	MDB_val mkey, data;
	MDB_txn *txn;
	int rc;
	bool ret = false;

	rc = mdb_txn_begin(db->env, NULL, MDB_RDONLY, &txn);
	if (rc == MDB_SUCCESS) {
		mkey.mv_data = (void *)key;
		mkey.mv_size = key_size;
		rc = mdb_get(txn, db->dbi, &mkey, &data);
		if (rc == MDB_SUCCESS) {
			if (dest_size > data.mv_size)
				dest_size = data.mv_size;
			memcpy(dest, data.mv_data, dest_size);
			ret = true;
		} else if (rc != MDB_NOTFOUND) {
			CfOut(cf_error, "", "!! could not read: %s", mdb_strerror(rc));
		}
		mdb_txn_abort(txn);
	} else {
		CfOut(cf_error, "", "!! could not create read txn: %s", mdb_strerror(rc));
	}
    return ret;
}

bool DBPrivWrite(DBPriv *db, const void *key, int key_size, const void *value, int value_size)
{
	MDB_val mkey, data;
	MDB_txn *txn;
	int rc;

	rc = mdb_txn_begin(db->env, NULL, 0, &txn);
	if (rc == MDB_SUCCESS) {
		mkey.mv_data = (void *)key;
		mkey.mv_size = key_size;
		data.mv_data = (void *)value;
		data.mv_size = value_size;
		rc = mdb_put(txn, db->dbi, &mkey, &data, 0);
		if (rc == MDB_SUCCESS) {
			rc = mdb_txn_commit(txn);
			if (rc) {
				CfOut(cf_error, "", "!! could not commit: %s", mdb_strerror(rc));
			}
		} else {
			CfOut(cf_error, "", "!! could not write: %s", mdb_strerror(rc));
			mdb_txn_abort(txn);
		}
	} else {
		CfOut(cf_error, "", "!! could not create write txn: %s", mdb_strerror(rc));
	}
    return rc == MDB_SUCCESS;
}

bool DBPrivDelete(DBPriv *db, const void *key, int key_size)
{
	MDB_val mkey;
	MDB_txn *txn;
	int rc;

	rc = mdb_txn_begin(db->env, NULL, 0, &txn);
	if (rc == MDB_SUCCESS) {
		mkey.mv_data = (void *)key;
		mkey.mv_size = key_size;
		rc = mdb_del(txn, db->dbi, &mkey, NULL);
		if (rc == MDB_SUCCESS) {
			rc = mdb_txn_commit(txn);
			if (rc) {
				CfOut(cf_error, "", "!! could not commit: %s", mdb_strerror(rc));
			}
		} else {
			CfOut(cf_error, "", "!! could not delete: %s", mdb_strerror(rc));
			mdb_txn_abort(txn);
		}
	} else {
		CfOut(cf_error, "", "!! could not create write txn: %s", mdb_strerror(rc));
	}
    return rc == MDB_SUCCESS;
}

DBCursorPriv *DBPrivOpenCursor(DBPriv *db)
{
    DBCursorPriv *cursor = NULL;
	MDB_txn *txn;
	MDB_cursor *mc;
	int rc;

	rc = mdb_txn_begin(db->env, NULL, 0, &txn);
	if (rc == MDB_SUCCESS) {
		rc = mdb_cursor_open(txn, db->dbi, &mc);
		if (rc == MDB_SUCCESS) {
			cursor = xcalloc(1, sizeof(DBCursorPriv));
			cursor->db = db;
			cursor->mc = mc;
		} else {
			CfOut(cf_error, "", "!! could not open cursor: %s", mdb_strerror(rc));
			mdb_txn_abort(txn);
		}
		/* txn remains with cursor */
	} else {
		CfOut(cf_error, "", "!! could not create cursor txn: %s", mdb_strerror(rc));
	}

    return cursor;
}

bool DBPrivAdvanceCursor(DBCursorPriv *cursor, void **key, int *key_size,
                     void **value, int *value_size)
{
	MDB_val mkey, data;
	int rc;
	bool retval = false;

	if ((rc = mdb_cursor_get(cursor->mc, &mkey, &data, MDB_NEXT)) == MDB_SUCCESS) {
		*key = mkey.mv_data;
		*key_size = mkey.mv_size;
		*value = data.mv_data;
		*value_size = data.mv_size;
		retval = true;
	} else if (rc != MDB_NOTFOUND) {
		CfOut(cf_error, "", "!! could not advance cursor: %s", mdb_strerror(rc));
	}
	if (cursor->pending_delete) {
		int r2;
		r2 = mdb_cursor_get(cursor->mc, &cursor->delkey, NULL, MDB_SET);
		if (r2 == MDB_SUCCESS)
			r2 = mdb_cursor_del(cursor->mc, 0);
		if (rc == MDB_SUCCESS)
			rc = mdb_cursor_get(cursor->mc, &mkey, &data, MDB_SET);
		cursor->pending_delete = false;
	}
    return retval;
}

bool DBPrivDeleteCursorEntry(DBCursorPriv *cursor)
{
	int rc = mdb_cursor_get(cursor->mc, &cursor->delkey, NULL, MDB_GET_CURRENT);
	if (rc == MDB_SUCCESS)
		cursor->pending_delete = true;
	return rc == MDB_SUCCESS;
}

bool DBPrivWriteCursorEntry(DBCursorPriv *cursor, const void *value, int value_size)
{
	MDB_val data;
	int rc;

	cursor->pending_delete = false;
	data.mv_data = (void *)value;
	data.mv_size = value_size;

    if ((rc = mdb_cursor_put(cursor->mc, NULL, &data, MDB_CURRENT)) != MDB_SUCCESS) {
		CfOut(cf_error, "", "!! could not write cursor entry: %s", mdb_strerror(rc));
	}
	return rc == MDB_SUCCESS;
}

void DBPrivCloseCursor(DBCursorPriv *cursor)
{
	MDB_txn *txn;
	int rc;

	if (cursor->pending_delete)
		mdb_cursor_del(cursor->mc, 0);

	txn = mdb_cursor_txn(cursor->mc);
	mdb_cursor_close(cursor->mc);
	rc = mdb_txn_commit(txn);
	if (rc) {
		CfOut(cf_error, "", "!! could not commit cursor txn: %s", mdb_strerror(rc));
	}
    free(cursor);
}

#endif
