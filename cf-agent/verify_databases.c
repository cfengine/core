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

#include <verify_databases.h>

#include <actuator.h>
#include <promises.h>
#include <files_names.h>
#include <conversion.h>
#include <attributes.h>
#include <string_lib.h>
#include <locks.h>
#include <cf_sql.h>
#include <rlist.h>
#include <policy.h>
#include <cf-agent-enterprise-stubs.h>
#include <eval_context.h>
#include <ornaments.h>
#include <misc_lib.h>

static int CheckDatabaseSanity(Attributes a, const Promise *pp);
static PromiseResult VerifySQLPromise(EvalContext *ctx, Attributes a, const Promise *pp);
static int VerifyDatabasePromise(CfdbConn *cfdb, char *database, Attributes a);

static int ValidateSQLTableName(char *table_path, char *db, char *table);
static int VerifyTablePromise(EvalContext *ctx, CfdbConn *cfdb, char *table_path, Rlist *columns, Attributes a, const Promise *pp, PromiseResult *result);
static int ValidateSQLTableName(char *table_path, char *db, char *table);
static void QueryTableColumns(char *s, char *db, char *table);
static int NewSQLColumns(char *table, Rlist *columns, char ***name_table, char ***type_table, int **size_table,
                         int **done);
static void DeleteSQLColumns(char **name_table, char **type_table, int *size_table, int *done, int len);
static void CreateDBQuery(DatabaseType type, char *query);
static int CreateTableColumns(CfdbConn *cfdb, char *table, Rlist *columns);
static int CheckSQLDataType(char *type, char *ref_type, const Promise *pp);
static int TableExists(CfdbConn *cfdb, char *name);
static Rlist *GetSQLTables(CfdbConn *cfdb);
static void ListTables(int type, char *query);
static int ValidateRegistryPromiser(char *s, const Promise *pp);
static bool CheckRegistrySanity(Attributes a, const Promise *pp);

/*****************************************************************************/

PromiseResult VerifyDatabasePromises(EvalContext *ctx, const Promise *pp)
{
    PromiseBanner(ctx, pp);

    Attributes a = GetDatabaseAttributes(ctx, pp);

    if (!CheckDatabaseSanity(a, pp))
    {
        return PROMISE_RESULT_FAIL;
    }

    if (strcmp(a.database.type, "sql") == 0)
    {
        return VerifySQLPromise(ctx, a, pp);
    }
    else if (strcmp(a.database.type, "ms_registry") == 0)
    {
#if defined(__MINGW32__)
        return VerifyRegistryPromise(ctx, a, pp);
#endif
        return PROMISE_RESULT_NOOP;
    }
    else
    {
        ProgrammingError("Unknown database type '%s'", a.database.type);
    }
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static PromiseResult VerifySQLPromise(EvalContext *ctx, Attributes a, const Promise *pp)
{
    char database[CF_MAXVARSIZE], table[CF_MAXVARSIZE], query[CF_BUFSIZE];
    char *sp;
    int count = 0;
    CfdbConn cfdb;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    snprintf(lockname, CF_BUFSIZE - 1, "db-%s", pp->promiser);

    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, false);
    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    database[0] = '\0';
    table[0] = '\0';

    for (sp = pp->promiser; *sp != '\0'; sp++)
    {
        if (strchr("./\\", *sp))
        {
            count++;
            strlcpy(table, sp + 1, CF_MAXVARSIZE);
            sscanf(pp->promiser, "%[^.\\/]", database);

            if (strlen(database) == 0)
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                     "SQL database promiser syntax should be of the form \"database.table\"");
                PromiseRef(LOG_LEVEL_ERR, pp);
                YieldCurrentLock(thislock);
                return PROMISE_RESULT_FAIL;
            }
        }
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (count > 1)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "SQL database promiser syntax should be of the form \"database.table\"");
        PromiseRef(LOG_LEVEL_ERR, pp);
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }

    if (strlen(database) == 0)
    {
        strlcpy(database, pp->promiser, CF_MAXVARSIZE);
    }

    if (a.database.operation == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
             "Missing database_operation in database promise");
        PromiseRef(LOG_LEVEL_ERR, pp);
        YieldCurrentLock(thislock);
        return PROMISE_RESULT_FAIL;
    }

    if (strcmp(a.database.operation, "delete") == 0)
    {
        /* Just deal with one */
        strcpy(a.database.operation, "drop");
    }

/* Connect to the server */

    CfConnectDB(&cfdb, a.database.db_server_type, a.database.db_server_host, a.database.db_server_owner,
                a.database.db_server_password, database);

    if (!cfdb.connected)
    {
        /* If we haven't said create then db should already exist */

        if ((a.database.operation) && (strcmp(a.database.operation, "create") != 0))
        {
            Log(LOG_LEVEL_ERR, "Could not connect an existing database '%s' - check server configuration?", database);
            PromiseRef(LOG_LEVEL_ERR, pp);
            CfCloseDB(&cfdb);
            YieldCurrentLock(thislock);
            return PROMISE_RESULT_FAIL;
        }
    }

/* Check change of existential constraints */

    if ((a.database.operation) && (strcmp(a.database.operation, "create") == 0))
    {
        CfConnectDB(&cfdb, a.database.db_server_type, a.database.db_server_host, a.database.db_server_owner,
                    a.database.db_server_password, a.database.db_connect_db);

        if (!cfdb.connected)
        {
            Log(LOG_LEVEL_ERR, "Could not connect to the sql_db server for '%s'", database);
            return PROMISE_RESULT_FAIL;
        }

        /* Don't drop the db if we really want to drop a table */

        if ((strlen(table) == 0) || ((strlen(table) > 0) && (strcmp(a.database.operation, "drop") != 0)))
        {
            VerifyDatabasePromise(&cfdb, database, a);
        }

        /* Close the database here to commit the change - might have to reopen */

        CfCloseDB(&cfdb);
    }

/* Now check the structure of the named table, if any */

    if (strlen(table) == 0)
    {
        YieldCurrentLock(thislock);
        return result;
    }

    CfConnectDB(&cfdb, a.database.db_server_type, a.database.db_server_host, a.database.db_server_owner,
                a.database.db_server_password, database);

    if (!cfdb.connected)
    {
        Log(LOG_LEVEL_INFO, "Database '%s' is not connected", database);
    }
    else
    {
        snprintf(query, CF_MAXVARSIZE - 1, "%s.%s", database, table);

        if (VerifyTablePromise(ctx, &cfdb, query, a.database.columns, a, pp, &result))
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_NOOP, pp, a, "Table '%s' is as promised", query);
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Table '%s' is not as promised", query);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }

/* Finally check any row constraints on this table */

        if (a.database.rows)
        {
            Log(LOG_LEVEL_INFO,
                  "Database row operations are not currently supported. Please contact cfengine with suggestions.");
        }

        CfCloseDB(&cfdb);
    }

    YieldCurrentLock(thislock);

    return result;
}

static int VerifyDatabasePromise(CfdbConn *cfdb, char *database, Attributes a)
{
    char query[CF_BUFSIZE], name[CF_MAXVARSIZE];
    int found = false;

    Log(LOG_LEVEL_VERBOSE, "Verifying promised database");

    if (!cfdb->connected)
    {
        Log(LOG_LEVEL_INFO, "Database '%s' is not connected", database);
        return false;
    }

    CreateDBQuery(cfdb->type, query);

    CfNewQueryDB(cfdb, query);

    if (cfdb->maxcolumns < 1)
    {
        Log(LOG_LEVEL_ERR, "The schema did not promise the expected number of fields - got %d expected >= %d",
              cfdb->maxcolumns, 1);
        return false;
    }

    while (CfFetchRow(cfdb))
    {
        strlcpy(name, CfFetchColumn(cfdb, 0), CF_MAXVARSIZE);

        Log(LOG_LEVEL_VERBOSE, "Discovered a database called '%s'", name);

        if (strcmp(name, database) == 0)
        {
            found = true;
        }
    }

    if (found)
    {
        Log(LOG_LEVEL_VERBOSE, "Database '%s' exists on this connection", database);
        return true;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Database '%s' does not seem to exist on this connection", database);
    }

    if ((a.database.operation) && (strcmp(a.database.operation, "drop") == 0))
    {
        if (((a.transaction.action) != cfa_warn) && (!DONTDO))
        {
            Log(LOG_LEVEL_VERBOSE, "Attempting to delete the database '%s'", database);
            snprintf(query, CF_MAXVARSIZE - 1, "drop database %s", database);
            CfVoidQueryDB(cfdb, query);
            return cfdb->result;
        }
        else
        {
            Log(LOG_LEVEL_WARNING, "Need to delete the database '%s' but only a warning was promised", database);
            return false;
        }
    }

    if ((a.database.operation) && (strcmp(a.database.operation, "create") == 0))
    {
        if (((a.transaction.action) != cfa_warn) && (!DONTDO))
        {
            Log(LOG_LEVEL_VERBOSE, "Attempting to create the database '%s'", database);
            snprintf(query, CF_MAXVARSIZE - 1, "create database %s", database);
            CfVoidQueryDB(cfdb, query);
            return cfdb->result;
        }
        else
        {
            Log(LOG_LEVEL_WARNING, "Need to create the database '%s' but only a warning was promised", database);
            return false;
        }
    }

    return false;
}

/*****************************************************************************/

static int CheckDatabaseSanity(Attributes a, const Promise *pp)
{
    Rlist *rp;
    int retval = true, commas = 0;

    if ((a.database.type) && (strcmp(a.database.type, "ms_registry") == 0))
    {
        retval = CheckRegistrySanity(a, pp);
    }
    else if ((a.database.type) && (strcmp(a.database.type, "sql") == 0))
    {
        if ((strchr(pp->promiser, '.') == NULL) && (strchr(pp->promiser, '/') == NULL)
            && (strchr(pp->promiser, '\\') == NULL))
        {
            if (a.database.columns)
            {
                Log(LOG_LEVEL_ERR, "Row values promised for an SQL table, but only the root database was promised");
                retval = false;
            }

            if (a.database.rows)
            {
                Log(LOG_LEVEL_ERR, "Columns promised for an SQL table, but only the root database was promised");
                retval = false;
            }
        }

        if (a.database.db_server_host == NULL)
        {
            Log(LOG_LEVEL_ERR, "No server host is promised for connecting to the SQL server");
            retval = false;
        }

        if (a.database.db_server_owner == NULL)
        {
            Log(LOG_LEVEL_ERR, "No database login user is promised for connecting to the SQL server");
            retval = false;
        }

        if (a.database.db_server_password == NULL)
        {
            Log(LOG_LEVEL_ERR, "No database authentication password is promised for connecting to the SQL server");
            retval = false;
        }

        for (rp = a.database.columns; rp != NULL; rp = rp->next)
        {
            commas = CountChar(RlistScalarValue(rp), ',');

            if ((commas > 2) || (commas < 1))
            {
                Log(LOG_LEVEL_ERR, "SQL Column format should be NAME,TYPE[,SIZE]");
                retval = false;
            }
        }

    }

    if ((a.database.operation) && (strcmp(a.database.operation, "create") == 0))
    {
    }

    if ((a.database.operation)
        && ((strcmp(a.database.operation, "delete") == 0) || (strcmp(a.database.operation, "drop") == 0)))
    {
        if (pp->comment == NULL)
        {
            Log(LOG_LEVEL_ERR,
                  "When specifying a delete/drop from an SQL database you must add a comment. Take a backup of the database before making this change. This is a highly destructive operation.");
            retval = false;
        }
    }

    return retval;
}

static bool CheckRegistrySanity(Attributes a, const Promise *pp)
{
    bool retval = true;

    ValidateRegistryPromiser(pp->promiser, pp);

    if ((a.database.operation) && (strcmp(a.database.operation, "create") == 0))
    {
        if (a.database.rows == NULL)
        {
            Log(LOG_LEVEL_INFO, "No row values promised for the MS registry database");
        }

        if (a.database.columns != NULL)
        {
            Log(LOG_LEVEL_ERR, "Columns are only used to delete promised values for the MS registry database");
            retval = false;
        }
    }

    if ((a.database.operation)
        && ((strcmp(a.database.operation, "delete") == 0) || (strcmp(a.database.operation, "drop") == 0)))
    {
        if (a.database.columns == NULL)
        {
            Log(LOG_LEVEL_INFO, "No columns were promised deleted in the MS registry database");
        }

        if (a.database.rows != NULL)
        {
            Log(LOG_LEVEL_ERR, "Rows cannot be deleted in the MS registry database, only entire columns");
            retval = false;
        }
    }

    for (Rlist *rp = a.database.rows; rp != NULL; rp = rp->next)
    {
        if (CountChar(RlistScalarValue(rp), ',') != 2)
        {
            Log(LOG_LEVEL_ERR, "Registry row format should be NAME,REG_SZ,VALUE, not '%s'", RlistScalarValue(rp));
            retval = false;
        }
    }

    for (Rlist *rp = a.database.columns; rp != NULL; rp = rp->next)
    {
        if (CountChar(RlistScalarValue(rp), ',') > 0)
        {
            Log(LOG_LEVEL_ERR, "MS registry column format should be NAME only in deletion");
            retval = false;
        }
    }

    return retval;
}

static int ValidateRegistryPromiser(char *key, const Promise *pp)
{
    static char *const valid[] = { "HKEY_CLASSES_ROOT", "HKEY_CURRENT_CONFIG",
        "HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE", "HKEY_USERS", NULL
    };
    char root_key[CF_MAXVARSIZE];
    char *sp;
    int i;

    /* First remove the root key */

    strlcpy(root_key, key, CF_MAXVARSIZE );
    sp = strchr(root_key, '\\');
    if (sp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Cannot locate '\\' in '%s'", root_key);
        Log(LOG_LEVEL_ERR, "Failed validating registry promiser");
        PromiseRef(LOG_LEVEL_ERR, pp);
        return false;
    }
    *sp = '\0';

    for (i = 0; valid[i] != NULL; i++)
    {
        if (strcmp(root_key, valid[i]) == 0)
        {
            return true;
        }
    }

    Log(LOG_LEVEL_ERR, "Non-editable registry prefix '%s'", root_key);
    PromiseRef(LOG_LEVEL_ERR, pp);
    return false;
}

/*****************************************************************************/
/* Linker troubles require this code to be here in the main body             */
/*****************************************************************************/

static int VerifyTablePromise(EvalContext *ctx, CfdbConn *cfdb, char *table_path, Rlist *columns, Attributes a,
                              const Promise *pp, PromiseResult *result)
{
    char name[CF_MAXVARSIZE], type[CF_MAXVARSIZE], query[CF_MAXVARSIZE], table[CF_MAXVARSIZE], db[CF_MAXVARSIZE];
    int i, count, size, no_of_cols, *size_table, *done, identified, retval = true;
    char **name_table, **type_table;

    Log(LOG_LEVEL_VERBOSE, "Verifying promised table structure for '%s'", table_path);

    if (!ValidateSQLTableName(table_path, db, table))
    {
        Log(LOG_LEVEL_ERR,
            "The structure of the promiser did not match that for an SQL table, i.e. 'database.table'");
        return false;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Assuming database '%s' with table '%s'", db, table);
    }

/* Verify the existence of the tables within the database */

    if (!TableExists(cfdb, table))
    {
        Log(LOG_LEVEL_ERR, "The database did not contain the promised table '%s'", table_path);

        if ((a.database.operation) && (strcmp(a.database.operation, "create") == 0))
        {
            if ((!DONTDO) && ((a.transaction.action) != cfa_warn))
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_CHANGE, pp, a, "Database.table '%s' doesn't seem to exist, creating",
                     table_path);
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                return CreateTableColumns(cfdb, table, columns);
            }
            else
            {
                Log(LOG_LEVEL_WARNING, "Database.table '%s' doesn't seem to exist, but only a warning was promised",
                      table_path);
            }
        }

        return false;
    }

/* Get a list of the columns in the table */

    QueryTableColumns(query, db, table);
    CfNewQueryDB(cfdb, query);

    if (cfdb->maxcolumns != 3)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Could not make sense of the columns");
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        CfDeleteQuery(cfdb);
        return false;
    }

/* Assume that the Rlist has been validated and consists of a,b,c */

    count = 0;
    no_of_cols = RlistLen(columns);

    if (!NewSQLColumns(table, columns, &name_table, &type_table, &size_table, &done))
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Could not make sense of the columns");
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        return false;
    }

/* Obtain columns from the named table - if any */

    while (CfFetchRow(cfdb))
    {
        char *sizestr;

        name[0] = '\0';
        type[0] = '\0';
        size = CF_NOINT;

        strlcpy(name, CfFetchColumn(cfdb, 0), CF_MAXVARSIZE);
        strlcpy(type, CfFetchColumn(cfdb, 1), CF_MAXVARSIZE);
        ToLowerStrInplace(type);
        sizestr = CfFetchColumn(cfdb, 2);

        if (sizestr)
        {
            size = IntFromString(sizestr);
        }

        Log(LOG_LEVEL_VERBOSE, "Discovered database column (%s,%s,%d)", name, type, size);

        if (sizestr && (size == CF_NOINT))
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
                 "Integer size of SQL datatype could not be determined or was not specified - invalid promise.");
            DeleteSQLColumns(name_table, type_table, size_table, done, no_of_cols);
            CfDeleteQuery(cfdb);
            return false;
        }

        identified = false;

        for (i = 0; i < no_of_cols; i++)
        {
            if (done[i])
            {
                continue;
            }

            if (strcmp(name, name_table[i]) == 0)
            {
                CheckSQLDataType(type, type_table[i], pp);

                if (size != size_table[i])
                {
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                         "Promised column '%s' in database.table '%s' has a non-matching array size (%d != %d)",
                         name, table_path, size, size_table[i]);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
                }
                else
                {
                    Log(LOG_LEVEL_VERBOSE, "Promised column '%s' in database.table '%s' is as promised", name,
                          table_path);
                }

                count++;
                done[i] = true;
                identified = true;
                break;
            }
        }

        if (!identified)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                 "Column '%s' found in database.table '%s' is not part of its promise.", name, table_path);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);

            if ((a.database.operation) && (strcmp(a.database.operation, "drop") == 0))
            {
                cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
                     "CFEngine will not promise to repair this, as the operation is potentially too destructive.");
                // Future allow deletion?
                *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
            }

            retval = false;
        }
    }

    CfDeleteQuery(cfdb);

/* Now look for deviations - only if we have promised to create missing */

    if ((a.database.operation) && (strcmp(a.database.operation, "drop") == 0))
    {
        return retval;
    }

    if (count != no_of_cols)
    {
        for (i = 0; i < no_of_cols; i++)
        {
            if (!done[i])
            {
                Log(LOG_LEVEL_ERR, "Promised column '%s' missing from database table '%s'", name_table[i],
                      pp->promiser);

                if ((!DONTDO) && ((a.transaction.action) != cfa_warn))
                {
                    if (size_table[i] > 0)
                    {
                        snprintf(query, CF_MAXVARSIZE - 1, "ALTER TABLE %s ADD %s %s(%d)", table, name_table[i],
                                 type_table[i], size_table[i]);
                    }
                    else
                    {
                        snprintf(query, CF_MAXVARSIZE - 1, "ALTER TABLE %s ADD %s %s", table, name_table[i],
                                 type_table[i]);
                    }

                    CfVoidQueryDB(cfdb, query);
                    cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_CHANGE, pp, a, "Adding promised column '%s' to database table '%s'",
                         name_table[i], table);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                    retval = true;
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a,
                         "Promised column '%s' missing from database table '%s' but only a warning was promised",
                         name_table[i], table);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                    retval = false;
                }
            }
        }
    }

    DeleteSQLColumns(name_table, type_table, size_table, done, no_of_cols);

    return retval;
}

/*****************************************************************************/

static int TableExists(CfdbConn *cfdb, char *name)
{
    Rlist *rp, *list = NULL;
    int match = false;

    list = GetSQLTables(cfdb);

    for (rp = list; rp != NULL; rp = rp->next)
    {
        if (strcmp(name, RlistScalarValue(rp)) == 0)
        {
            match = true;
        }
    }

    RlistDestroy(list);

    return match;
}

/*****************************************************************************/

static int CreateTableColumns(CfdbConn *cfdb, char *table, Rlist *columns)
{
    char entry[CF_MAXVARSIZE], query[CF_BUFSIZE];
    int i, *size_table, *done;
    char **name_table, **type_table;
    int no_of_cols = RlistLen(columns);

    Log(LOG_LEVEL_ERR, "Trying to create table '%s'", table);

    if (!NewSQLColumns(table, columns, &name_table, &type_table, &size_table, &done))
    {
        return false;
    }

    if (no_of_cols > 0)
    {
        snprintf(query, CF_BUFSIZE - 1, "create table %s(", table);

        for (i = 0; i < no_of_cols; i++)
        {
            Log(LOG_LEVEL_VERBOSE, "Forming column template %s %s %d", name_table[i], type_table[i],
                  size_table[i]);;

            if (size_table[i] > 0)
            {
                snprintf(entry, CF_MAXVARSIZE - 1, "%s %s(%d)", name_table[i], type_table[i], size_table[i]);
            }
            else
            {
                snprintf(entry, CF_MAXVARSIZE - 1, "%s %s", name_table[i], type_table[i]);
            }

            strcat(query, entry);

            if (i < no_of_cols - 1)
            {
                strcat(query, ",");
            }
        }

        strcat(query, ")");
    }

    CfVoidQueryDB(cfdb, query);
    DeleteSQLColumns(name_table, type_table, size_table, done, no_of_cols);
    return true;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static Rlist *GetSQLTables(CfdbConn *cfdb)
{
    Rlist *list = NULL;
    char query[CF_MAXVARSIZE];

    ListTables(cfdb->type, query);

    CfNewQueryDB(cfdb, query);

    if (cfdb->maxcolumns != 1)
    {
        Log(LOG_LEVEL_ERR, "Could not make sense of the columns");
        CfDeleteQuery(cfdb);
        return NULL;
    }

    while (CfFetchRow(cfdb))
    {
        RlistPrepend(&list, CfFetchColumn(cfdb, 0), RVAL_TYPE_SCALAR);
    }

    CfDeleteQuery(cfdb);

    return list;
}

/*****************************************************************************/

static void CreateDBQuery(DatabaseType type, char *query)
{
    switch (type)
    {
    case DATABASE_TYPE_MYSQL:
        snprintf(query, CF_MAXVARSIZE - 1, "show databases");
        break;

    case DATABASE_TYPE_POSTGRES:
        /* This gibberish is the simplest thing I can find in postgres */

        snprintf(query, CF_MAXVARSIZE - 1, "SELECT pg_database.datname FROM pg_database");
        break;

    default:
        snprintf(query, CF_MAXVARSIZE, "NULL QUERY");
        break;
    }
}

/*****************************************************************************/

static int ValidateSQLTableName(char *table_path, char *db, char *table)
{
    char *sp;
    int dot = false, back = false, fwd = false;

/* Valid separators . / or \ only */

    if ((sp = strchr(table_path, '/')))
    {
        fwd = true;
        *sp = '.';
    }

    if ((sp = strchr(table_path, '\\')))
    {
        back = true;
        *sp = '.';
    }

    if ((sp = strchr(table_path, '.')))
    {
        dot = true;
        sp++;
    }

/* Should contain a single separator */

    if (dot + back + fwd != 1)
    {
        return false;
    }

    memset(db, 0, CF_MAXVARSIZE);
    strncpy(db, table_path, sp - table_path - 1);
    strlcpy(table, sp, CF_MAXVARSIZE);
    return true;
}

/*****************************************************************************/

static void QueryTableColumns(char *s, char *db, char *table)
{
    snprintf(s, CF_MAXVARSIZE - 1,
             "SELECT column_name,data_type,character_maximum_length FROM information_schema.columns WHERE table_name ='%s' AND table_schema = '%s'",
             table, db);
}

/*****************************************************************************/

static int NewSQLColumns(char *table, Rlist *columns, char ***name_table, char ***type_table, int **size_table,
                         int **done)
{
    int i, no_of_cols = RlistLen(columns);
    Rlist *cols, *rp;

    *name_table = (char **) xmalloc(sizeof(char *) * (no_of_cols + 1));
    *type_table = (char **) xmalloc(sizeof(char *) * (no_of_cols + 1));
    *size_table = (int *) xmalloc(sizeof(int) * (no_of_cols + 1));
    *done = (int *) xmalloc(sizeof(int) * (no_of_cols + 1));

    for (i = 0, rp = columns; rp != NULL; rp = rp->next, i++)
    {
        (*done)[i] = 0;

        cols = RlistFromSplitString(RlistScalarValue(rp), ',');

        if (!cols)
        {
            Log(LOG_LEVEL_ERR, "No columns promised for table '%s' - makes no sense", table);
            return false;
        }

        if (cols->val.item == NULL)
        {
            Log(LOG_LEVEL_ERR, "Malformed column promise for table '%s' - found not even a name", table);
            free(*name_table);
            free(*type_table);
            free(*size_table);
            free(*done);
            return false;
        }

        (*name_table)[i] = xstrdup(RlistScalarValue(cols));

        if (cols->next == NULL)
        {
            Log(LOG_LEVEL_ERR, "Malformed column '%s' promised for table '%s' - missing a type", (*name_table)[i],
                  table);
            free(*name_table);
            free(*type_table);
            free(*size_table);
            free(*done);
            return false;
        }

        (*type_table)[i] = xstrdup(RlistScalarValue(cols->next));

        if (cols->next->next == NULL)
        {
            (*size_table)[i] = 0;
        }
        else
        {
            if (cols->next->next->val.item)
            {
                (*size_table)[i] = IntFromString(RlistScalarValue(cols->next->next));
            }
            else
            {
                (*size_table)[i] = 0;
            }
        }

        RlistDestroy(cols);
    }

    return true;
}

/*****************************************************************************/

static void DeleteSQLColumns(char **name_table, char **type_table, int *size_table, int *done, int len)
{
    int i;

    if ((name_table == NULL) || (type_table == NULL) || (size_table == NULL))
    {
        return;
    }

    for (i = 0; i < len; i++)
    {
        if (name_table[i] != NULL)
        {
            free(name_table[i]);
        }

        if (type_table[i] != NULL)
        {
            free(type_table[i]);
        }
    }

    free(name_table);
    free(type_table);
    free(size_table);
    free(done);
}

/*****************************************************************************/

static int CheckSQLDataType(char *type, char *ref_type, const Promise *pp)
{
    static char *const aliases[3][2] =
        {
            {"varchar", "character@varying"},
            {"varchar", "character varying"},
            {NULL, NULL}
        };

    int i;

    for (i = 0; aliases[i][0] != NULL; i++)
    {
        if ((strcmp(ref_type, aliases[i][0]) == 0) || (strcmp(ref_type, aliases[i][1]) == 0)
            || (strcmp(type, aliases[i][0]) == 0) || (strcmp(type, aliases[i][1]) == 0))
        {
            if ((strcmp(type, ref_type) != 0) && (strcmp(aliases[i][0], ref_type) != 0))
            {
                Log(LOG_LEVEL_VERBOSE, "Promised column in database '%s' has a non-matching type (%s != %s)",
                      pp->promiser, ref_type, type);
            }
        }
        else
        {
            if (strcmp(type, ref_type) != 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Promised column in database '%s' has a non-matching type (%s != %s)",
                      pp->promiser, ref_type, type);
            }
        }
    }

    return true;
}

/*****************************************************************************/

static void ListTables(int type, char *query)
{
    switch (type)
    {
    case DATABASE_TYPE_MYSQL:
        snprintf(query, CF_MAXVARSIZE - 1, "show tables");
        break;

    case DATABASE_TYPE_POSTGRES:
        /* This gibberish is the simplest thing I can find in postgres */

        snprintf(query, CF_MAXVARSIZE - 1,
                 "SELECT c.relname as \"Name\" FROM pg_catalog.pg_class c JOIN pg_catalog.pg_roles r ON r.oid = c.relowner LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace WHERE n.nspname = 'public'");
        break;

    default:
        snprintf(query, CF_MAXVARSIZE, "NULL QUERY");
        break;
    }
}
