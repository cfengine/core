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

#ifndef CFENGINE_DBM_API_H
#define CFENGINE_DBM_API_H

#include <map.h>

typedef enum
{
    dbid_classes,   // Deprecated
    dbid_variables, // Deprecated
    dbid_performance,
    dbid_checksums, // Deprecated
    dbid_filestats, // Deprecated
    dbid_changes,
    dbid_observations,
    dbid_state,
    dbid_lastseen,
    dbid_audit,
    dbid_locks,
    dbid_history,
    dbid_measure,
    dbid_static,
    dbid_scalars,
    dbid_windows_registry,
    dbid_cache,
    dbid_license,
    dbid_value,
    dbid_agent_execution,
    dbid_bundles,   // Deprecated
    dbid_packages_installed, //new package promise installed packages list
    dbid_packages_updates,   //new package promise list of available updates

    dbid_max
} dbid;

typedef struct DBHandle_ DBHandle;
typedef struct DBCursor_ DBCursor;

typedef DBHandle CF_DB;
typedef DBCursor CF_DBC;

void DBSetMaximumConcurrentTransactions(int max_txn);

bool OpenDB(CF_DB **dbp, dbid db);
bool OpenSubDB(DBHandle **dbp, dbid id, const char *sub_name);
bool CleanDB(DBHandle *handle);
void CloseDB(CF_DB *dbp);

bool HasKeyDB(CF_DB *dbp, const char *key, int key_size);
int ValueSizeDB(CF_DB *dbp, const char *key, int key_size);
bool ReadComplexKeyDB(CF_DB *dbp, const char *key, int key_size, void *dest, int destSz);
bool WriteComplexKeyDB(CF_DB *dbp, const char *key, int keySz, const void *src, int srcSz);
bool DeleteComplexKeyDB(CF_DB *dbp, const char *key, int size);
bool ReadDB(CF_DB *dbp, const char *key, void *dest, int destSz);
bool WriteDB(CF_DB *dbp, const char *key, const void *src, int srcSz);
bool DeleteDB(CF_DB *dbp, const char *key);

/*
 * Creating cursor locks the whole database, so keep the amount of work here to
 * minimum.
 *
 * Don't use WriteDB/DeleteDB while iterating database, it will result in
 * deadlock. Use cursor-specific operations instead. They work on the current
 * key.
 */
bool NewDBCursor(CF_DB *dbp, CF_DBC **dbcp);
bool NextDB(CF_DBC *dbcp, char **key, int *ksize, void **value, int *vsize);
bool DBCursorDeleteEntry(CF_DBC *cursor);
bool DBCursorWriteEntry(CF_DBC *cursor, const void *value, int value_size);
bool DeleteDBCursor(CF_DBC *dbcp);

char *DBIdToPath(dbid id);
char *DBIdToSubPath(dbid id, const char *subdb_name);

StringMap *LoadDatabaseToStringMap(dbid database_id);
#endif  /* NOT CFENGINE_DBM_API_H */
