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
#include "cf3.extern.h"

/*
 * Implement/emulate key-value storage using SQLite3.
 *
 * TODO: Re-use compiled statements.
 */

#ifdef SQLITE3

#include "dbm_sqlite3.h"

static int SQLite3_PrepareSchema(sqlite3 *handle);

/*****************************************************************************/

int SQLite3_OpenDB(char *filename, sqlite3 **dbp)
{
*dbp = NULL;

int ret = sqlite3_open_v2(filename, dbp,
                          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
                          | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_SHAREDCACHE,
                          NULL);

if (*dbp == NULL)
   {
   FatalError("Unable to allocate memory for sqlite3 handle");
   }

if (ret != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_OpenDB: Unable to open database: %s", sqlite3_errmsg(*dbp));
   return false;
   }

return SQLite3_PrepareSchema(*dbp);
}

/*****************************************************************************/

const char *CREATE_TABLE = "CREATE TABLE IF NOT EXISTS data "
   "(key TEXT PRIMARY KEY, value TEXT NOT NULL)";

static int SQLite3_PrepareSchema(sqlite3 *dbp)
{
char *errmsg;

if (sqlite3_exec(dbp, CREATE_TABLE, NULL, NULL, &errmsg) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_PrepareSchema: Unable to create table in database: %s",
         errmsg);
   sqlite3_free(errmsg);
   return false;
   }

return true;
}

/*****************************************************************************/

int SQLite3_CloseDB(sqlite3 *dbp)
{
if (sqlite3_close(dbp) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_CloseDB: Unable to close database: %s", sqlite3_errmsg(dbp));
   return false;
   }

return true;
}

/*****************************************************************************/

const char *SELECT_SIZE = "SELECT LENGTH(value) FROM data WHERE key = ?1";

int SQLite3_ValueSizeDB(sqlite3 *dbp, char *key)
{
sqlite3_stmt *stmt = NULL;
int retval = -1;
int ret;

if (sqlite3_prepare_v2(dbp, SELECT_SIZE, -1, &stmt, NULL) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_ValueSizeDB: Unable to prepare statement: %s", sqlite3_errmsg(dbp));
   goto done;
   }

if (sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_ValueSizeDB: Unable to bind paremeter: %s", sqlite3_errmsg(dbp));
   goto done;
   }

ret = sqlite3_step(stmt);
if (ret == SQLITE_DONE)
   {
   Debug("Key %s does not exist in database.\n", key);
   }
else if (ret == SQLITE_ROW)
   {
   retval = sqlite3_column_int(stmt, 0);
   }
else
   {
   CfOut(cf_error, "", "SQLite3_ValueSizeDB: Unable to execute statement: %s", sqlite3_errmsg(dbp));
   }

done:
if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_ValueSizeDB: Unable to finalize statement: %s", sqlite3_errmsg(dbp));
   }

return retval;
}

/*****************************************************************************/

const char *SELECT_VALUE = "SELECT value FROM data WHERE key = ?1";

static int SQLite3_RevealComplexKeyDB(sqlite3 *dbp, char *key, int keysize, void **result, int *rsize)
{
sqlite3_stmt *stmt = NULL;
bool retval = false;
int ret;

if (sqlite3_prepare_v2(dbp, SELECT_VALUE, -1, &stmt, NULL) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_RevealComplexKeyDB: Unable to prepare statament: %s", sqlite3_errmsg(dbp));
   goto done;
   }

if (sqlite3_bind_text(stmt, 1, key, keysize, SQLITE_TRANSIENT) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_RevealComplexKeyDB: Unable to bind parameter: %s", sqlite3_errmsg(dbp));
   goto done;
   }

ret = sqlite3_step(stmt);
if (ret == SQLITE_DONE)
   {
   Debug("Key %.*s does not exist in database", keysize, key);
   }
else if (ret == SQLITE_ROW)
   {
   *result = (char*)sqlite3_column_text(stmt, 0);
   *rsize = sqlite3_column_bytes(stmt, 0);;
   retval = true;
   }
else
   {
   CfOut(cf_error, "", "SQLite3_RevealComplexKeyDB: Unable to execute statement: %s", sqlite3_errmsg(dbp));
   }

done:
if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_RevealComplexKeyDB: Unable to finalize statement: %s", sqlite3_errmsg(dbp));
   }

return retval;
}

/*****************************************************************************/

int SQLite3_ReadComplexKeyDB(sqlite3 *dbp, char *key, int keysize, void *ptr, int expectedsize)
{
void *retvalue;
int retsize;

if (SQLite3_RevealComplexKeyDB(dbp, key, keysize, &retvalue, &retsize))
   {
   if (expectedsize <= retsize)
      {
      memcpy(ptr, retvalue, expectedsize);
      }
   else if (expectedsize > retsize)
      {
      Debug("SQLite3_ReadComplexKeyDB: data for key %.s is larger than expected (%d > %d)",
            keysize, key, retsize, expectedsize);

      memcpy(ptr, retvalue, retsize);
      memset(ptr + retsize, 0, expectedsize - retsize);
      }
   return true;
   }
else
   {
   return false;
   }
}

/*****************************************************************************/

int SQLite3_RevealDB(sqlite3 *dbp, char *key, void **result, int *rsize)
{
return SQLite3_RevealComplexKeyDB(dbp, key, strlen(key), result, rsize);
}

/*****************************************************************************/

const char *INSERT_KEY_VALUE = "INSERT OR REPLACE INTO data VALUES (?1, ?2)";

int SQLite3_WriteComplexKeyDB(sqlite3 *dbp, char *key, int keysize, const void *ptr, int size)
{
sqlite3_stmt *stmt = NULL;
bool retval = false;

if (sqlite3_prepare_v2(dbp, INSERT_KEY_VALUE, -1, &stmt, NULL) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_WriteComplexKeyDB: Unable to prepare statement: %s",
         sqlite3_errmsg(dbp));
   goto done;
   }

if (sqlite3_bind_text(stmt, 1, key, keysize, SQLITE_TRANSIENT) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_WriteComplexKeyDB: Unable to bind parameter: %s",
         sqlite3_errmsg(dbp));
   goto done;
   }

if (sqlite3_bind_text(stmt, 2, ptr, size, SQLITE_TRANSIENT) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_WriteComplexKeyDB: Unable to bind parameter: %s",
         sqlite3_errmsg(dbp));
   goto done;
   }

if (sqlite3_step(stmt) == SQLITE_DONE)
   {
   retval = true;
   }
else
   {
   CfOut(cf_error, "", "SQLite3_WriteComplexKeyDB: Unable to execute statement: %s", sqlite3_errmsg(dbp));
   }

done:
if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_WriteComplexKeyDB: Unable to finalize statement: %s", sqlite3_errmsg(dbp));
   }

return retval;
}

/*****************************************************************************/

const char *DELETE_KEY = "DELETE FROM data WHERE key = ?1";

int SQLite3_DeleteComplexKeyDB(sqlite3 *dbp, char *key, int keysize)
{
sqlite3_stmt *stmt = NULL;
bool retval = false;

if (sqlite3_prepare_v2(dbp, DELETE_KEY, -1, &stmt, NULL) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_DeleteComplexKeyDB: Unable to prepare statement: %s",
         sqlite3_errmsg(dbp));
   goto done;
   }

if (sqlite3_bind_text(stmt, 1, key, keysize, SQLITE_TRANSIENT) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_DeleteComplexKeyDB: Unable to bind parameter: %s",
         sqlite3_errmsg(dbp));
   goto done;
   }

if (sqlite3_step(stmt) == SQLITE_DONE)
   {
   if (sqlite3_changes(dbp) == 0)
      {
      Debug("Key %.*s requested for deletion is not found in database", keysize, key);
      }
   retval = true;
   }
else
   {
   CfOut(cf_error, "", "SQLite3_DeleteComplexKeyDB: Unable to execute statement: %s", sqlite3_errmsg(dbp));
   }

done:
if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_DeleteComplexKeyDB: Unable to finalize statement: %s", sqlite3_errmsg(dbp));
   }

return retval;
}

/*****************************************************************************/

const char *SELECT_KEYS_VALUES = "SELECT key, value FROM data";

int SQLite3_NewDBCursor(sqlite3 *dbp, sqlite3_stmt **stmt)
{
if (sqlite3_prepare_v2(dbp, SELECT_KEYS_VALUES, -1, stmt, NULL) == SQLITE_OK)
   {
   return true;
   }

CfOut(cf_error, "", "SQLite3_DeleteComplexKeyDB: Unable to prepare statement: %s",
      sqlite3_errmsg(dbp));
return false;
}

/*****************************************************************************/

int SQLite3_NextDB(sqlite3 *dbp, sqlite3_stmt *stmt, char **key, int *ksize, void **value, int *vsize)
{
int ret = sqlite3_step(stmt);

if (ret == SQLITE_DONE)
   {
   return false;
   }

if (ret == SQLITE_ROW)
   {
   *key = (char*)sqlite3_column_text(stmt, 0);
   *ksize = sqlite3_column_bytes(stmt, 0);
   *value = (char*)sqlite3_column_text(stmt, 1);
   *vsize = sqlite3_column_bytes(stmt, 1);
   return true;
   }

CfOut(cf_error, "", "SQLite3_NextDB: Error trying to read database: %s\n", sqlite3_errmsg(dbp));
return false;
}

/*****************************************************************************/

int SQLite3_DeleteDBCursor(sqlite3 *dbp, sqlite3_stmt *stmt)
{
if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_DeleteDBCursor: Unable to finalize statement: %s", sqlite3_errmsg(dbp));
   return false;
   }

return true;
}

/*****************************************************************************/

static const char *OPEN_TRANSACTION="BEGIN TRANSACTION";

void SQLite3_OpenDBTransaction(sqlite3 *dbp)
{
char *errmsg;

if (sqlite3_exec(dbp, OPEN_TRANSACTION, NULL, NULL, &errmsg) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_OpenDBTransaction: Unable to begin transaction: %s",
         errmsg);
   sqlite3_free(errmsg);
   }
}

/*****************************************************************************/

static const char *COMMIT_TRANSACTION="COMMIT TRANSACTION";

void SQLite3_CommitDBTransaction(sqlite3 *dbp)
{
char *errmsg;

if (sqlite3_exec(dbp, COMMIT_TRANSACTION, NULL, NULL, &errmsg) != SQLITE_OK)
   {
   CfOut(cf_error, "", "SQLite3_CommitDBTransaction: Unable to commit: %s",
         errmsg);
   sqlite3_free(errmsg);
   }
}

#endif
