/*
   Copyright 2017 Northern.tech AS

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

#ifndef CFENGINE_DBM_PRIV_H
#define CFENGINE_DBM_PRIV_H

/* DBM implementation is supposed to define the following structures and
 * implement the following functions */

typedef struct DBPriv_ DBPriv;
typedef struct DBCursorPriv_ DBCursorPriv;

const char *DBPrivGetFileExtension(void);

#define DB_PRIV_DATABASE_BROKEN ((DBPriv *)-1)

void DBPrivSetMaximumConcurrentTransactions(int max_txn);

/*
 * These two functions will always be called with a per-database lock held.
 */

/*
 * Should return either
 * - NULL in case of generic error
 * - DB_PRIV_DATABASE_BROKEN in case database file is broken, need to be moved
      away and attempt to open database again should be performed.
 * - valid pointer to DBPriv * in case database was opened successfully.
 */
DBPriv *DBPrivOpenDB(const char *dbpath, dbid id);
void DBPrivCloseDB(DBPriv *hdbp);
void DBPrivCommit(DBPriv *hdbp);
bool DBPrivClean(DBPriv *hdbp);

bool DBPrivHasKey(DBPriv *db, const void *key, int key_size);
int DBPrivGetValueSize(DBPriv *db, const void *key, int key_size);

bool DBPrivRead(DBPriv *db, const void *key, int key_size,
            void *dest, int dest_size);

bool DBPrivWrite(DBPriv *db, const void *key, int key_size,
             const void *value, int value_size);

bool DBPrivDelete(DBPriv *db, const void *key, int key_size);


DBCursorPriv *DBPrivOpenCursor(DBPriv *db);
bool DBPrivAdvanceCursor(DBCursorPriv *cursor, void **key, int *key_size,
                     void **value, int *value_size);
bool DBPrivDeleteCursorEntry(DBCursorPriv *cursor);
bool DBPrivWriteCursorEntry(DBCursorPriv *cursor, const void *value, int value_size);
void DBPrivCloseCursor(DBCursorPriv *cursor);

/**
 * @brief Check a database file for consistency
 * @param dbpath Path to database file
 * @return NULL if successful, else an error string that must be freed
 */
char *DBPrivDiagnose(const char *dbpath);

#endif
