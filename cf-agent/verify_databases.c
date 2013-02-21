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

#include "verify_databases.h"

#include "promises.h"
#include "files_names.h"
#include "conversion.h"
#include "attributes.h"
#include "cfstream.h"
#include "string_lib.h"
#include "transaction.h"
#include "cf_sql.h"
#include "logging.h"
#include "rlist.h"
#include "policy.h"

static int CheckDatabaseSanity(Attributes a, Promise *pp);
static void VerifySQLPromise(Attributes a, Promise *pp);
static int VerifyDatabasePromise(CfdbConn *cfdb, char *database, Attributes a, Promise *pp);

static int ValidateSQLTableName(char *table_path, char *db, char *table);
static int VerifyTablePromise(CfdbConn *cfdb, char *table_path, Rlist *columns, Attributes a, Promise *pp);
static int ValidateSQLTableName(char *table_path, char *db, char *table);
static void QueryTableColumns(char *s, char *db, char *table);
static int NewSQLColumns(char *table, Rlist *columns, char ***name_table, char ***type_table, int **size_table,
                         int **done);
static void DeleteSQLColumns(char **name_table, char **type_table, int *size_table, int *done, int len);
static void CreateDBQuery(DatabaseType type, char *query);
static int CreateTableColumns(CfdbConn *cfdb, char *table, Rlist *columns, Attributes a, Promise *pp);
static int CheckSQLDataType(char *type, char *ref_type, Promise *pp);
static int TableExists(CfdbConn *cfdb, char *name);
static Rlist *GetSQLTables(CfdbConn *cfdb);
static void ListTables(int type, char *query);
static int ValidateRegistryPromiser(char *s, Attributes a, Promise *pp);
static int CheckRegistrySanity(Attributes a, Promise *pp);

/*****************************************************************************/

void VerifyDatabasePromises(Promise *pp)
{
    Attributes a = { {0} };

    if (pp->done)
    {
        return;
    }

    PromiseBanner(pp);

    a = GetDatabaseAttributes(pp);

    if (!CheckDatabaseSanity(a, pp))
    {
        return;
    }

    if (strcmp(a.database.type, "sql") == 0)
    {
        VerifySQLPromise(a, pp);
        return;
    }

    if (strcmp(a.database.type, "ms_registry") == 0)
    {
#if defined(__MINGW32__)
        VerifyRegistryPromise(a, pp);
#endif
        return;
    }
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static void VerifySQLPromise(Attributes a, Promise *pp)
{
    char database[CF_MAXVARSIZE], table[CF_MAXVARSIZE], query[CF_BUFSIZE];
    char *sp;
    int count = 0;
    CfdbConn cfdb;
    CfLock thislock;
    char lockname[CF_BUFSIZE];

    snprintf(lockname, CF_BUFSIZE - 1, "db-%s", pp->promiser);

    thislock = AcquireLock(lockname, VUQNAME, CFSTARTTIME, a, pp, false);

    if (thislock.lock == NULL)
    {
        return;
    }

    database[0] = '\0';
    table[0] = '\0';

    for (sp = pp->promiser; *sp != '\0'; sp++)
    {
        if (strchr("./\\", *sp))
        {
            count++;
            strncpy(table, sp + 1, CF_MAXVARSIZE - 1);
            sscanf(pp->promiser, "%[^.\\/]", database);

            if (strlen(database) == 0)
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
                     "SQL database promiser syntax should be of the form \"database.table\"");
                PromiseRef(OUTPUT_LEVEL_ERROR, pp);
                YieldCurrentLock(thislock);
                return;
            }
        }
    }

    if (count > 1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "SQL database promiser syntax should be of the form \"database.table\"");
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    }

    if (strlen(database) == 0)
    {
        strncpy(database, pp->promiser, CF_MAXVARSIZE - 1);
    }

    if (a.database.operation == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a ,
             "Missing database_operation in database promise");
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        YieldCurrentLock(thislock);
        return;
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
            CfOut(OUTPUT_LEVEL_ERROR, "", "Could not connect an existing database %s - check server configuration?\n", database);
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
            CfCloseDB(&cfdb);
            YieldCurrentLock(thislock);
            return;
        }
    }

/* Check change of existential constraints */

    if ((a.database.operation) && (strcmp(a.database.operation, "create") == 0))
    {
        CfConnectDB(&cfdb, a.database.db_server_type, a.database.db_server_host, a.database.db_server_owner,
                    a.database.db_server_password, a.database.db_connect_db);

        if (!cfdb.connected)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Could not connect to the sql_db server for %s\n", database);
            return;
        }

        /* Don't drop the db if we really want to drop a table */

        if ((strlen(table) == 0) || ((strlen(table) > 0) && (strcmp(a.database.operation, "drop") != 0)))
        {
            VerifyDatabasePromise(&cfdb, database, a, pp);
        }

        /* Close the database here to commit the change - might have to reopen */

        CfCloseDB(&cfdb);
    }

/* Now check the structure of the named table, if any */

    if (strlen(table) == 0)
    {
        YieldCurrentLock(thislock);
        return;
    }

    CfConnectDB(&cfdb, a.database.db_server_type, a.database.db_server_host, a.database.db_server_owner,
                a.database.db_server_password, database);

    if (!cfdb.connected)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Database %s is not connected\n", database);
    }
    else
    {
        snprintf(query, CF_MAXVARSIZE - 1, "%s.%s", database, table);

        if (VerifyTablePromise(&cfdb, query, a.database.columns, a, pp))
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_NOP, "", pp, a, " -> Table \"%s\" is as promised", query);
        }
        else
        {
            cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a, " -> Table \"%s\" is not as promised", query);
        }

/* Finally check any row constraints on this table */

        if (a.database.rows)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "",
                  " !! Database row operations are not currently supported. Please contact cfengine with suggestions.");
        }

        CfCloseDB(&cfdb);
    }

    YieldCurrentLock(thislock);
}

static int VerifyDatabasePromise(CfdbConn *cfdb, char *database, Attributes a, Promise *pp)
{
    char query[CF_BUFSIZE], name[CF_MAXVARSIZE];
    int found = false;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Verifying promised database");

    if (!cfdb->connected)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "Database %s is not connected\n", database);
        return false;
    }

    CreateDBQuery(cfdb->type, query);

    CfNewQueryDB(cfdb, query);

    if (cfdb->maxcolumns < 1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! The schema did not promise the expected number of fields - got %d expected >= %d\n",
              cfdb->maxcolumns, 1);
        return false;
    }

    while (CfFetchRow(cfdb))
    {
        strncpy(name, CfFetchColumn(cfdb, 0), CF_MAXVARSIZE - 1);

        CfOut(OUTPUT_LEVEL_VERBOSE, "", "      ? ... discovered a database called \"%s\"", name);

        if (strcmp(name, database) == 0)
        {
            found = true;
        }
    }

    if (found)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Database \"%s\" exists on this connection", database);
        return true;
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Database \"%s\" does not seem to exist on this connection", database);
    }

    if ((a.database.operation) && (strcmp(a.database.operation, "drop") == 0))
    {
        if (((a.transaction.action) != cfa_warn) && (!DONTDO))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Attempting to delete the database %s", database);
            snprintf(query, CF_MAXVARSIZE - 1, "drop database %s", database);
            CfVoidQueryDB(cfdb, query);
            return cfdb->result;
        }
        else
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Need to delete the database %s but only a warning was promised\n", database);
            return false;
        }
    }

    if ((a.database.operation) && (strcmp(a.database.operation, "create") == 0))
    {
        if (((a.transaction.action) != cfa_warn) && (!DONTDO))
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Attempting to create the database %s", database);
            snprintf(query, CF_MAXVARSIZE - 1, "create database %s", database);
            CfVoidQueryDB(cfdb, query);
            return cfdb->result;
        }
        else
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Need to create the database %s but only a warning was promised\n", database);
            return false;
        }
    }

    return false;
}

/*****************************************************************************/

static int CheckDatabaseSanity(Attributes a, Promise *pp)
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
                CfOut(OUTPUT_LEVEL_ERROR, "", "Row values promised for an SQL table, but only the root database was promised");
                retval = false;
            }

            if (a.database.rows)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Columns promised for an SQL table, but only the root database was promised");
                retval = false;
            }
        }

        if (a.database.db_server_host == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "No server host is promised for connecting to the SQL server");
            retval = false;
        }

        if (a.database.db_server_owner == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "No database login user is promised for connecting to the SQL server");
            retval = false;
        }

        if (a.database.db_server_password == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "No database authentication password is promised for connecting to the SQL server");
            retval = false;
        }

        for (rp = a.database.columns; rp != NULL; rp = rp->next)
        {
            commas = CountChar(rp->item, ',');

            if ((commas > 2) && (commas < 1))
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "SQL Column format should be NAME,TYPE[,SIZE]");
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
        if (pp->ref == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "",
                  "When specifying a delete/drop from an SQL database you must add a comment. Take a backup of the database before making this change. This is a highly destructive operation.");
            retval = false;
        }
    }

    return retval;
}

static int CheckRegistrySanity(Attributes a, Promise *pp)
{
    bool retval = true;

    ValidateRegistryPromiser(pp->promiser, a, pp);

    if ((a.database.operation) && (strcmp(a.database.operation, "create") == 0))
    {
        if (a.database.rows == NULL)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "No row values promised for the MS registry database");
        }

        if (a.database.columns != NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Columns are only used to delete promised values for the MS registry database");
            retval = false;
        }
    }

    if ((a.database.operation)
        && ((strcmp(a.database.operation, "delete") == 0) || (strcmp(a.database.operation, "drop") == 0)))
    {
        if (a.database.columns == NULL)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "", "No columns were promised deleted in the MS registry database");
        }

        if (a.database.rows != NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Rows cannot be deleted in the MS registry database, only entire columns");
            retval = false;
        }
    }

    for (Rlist *rp = a.database.rows; rp != NULL; rp = rp->next)
    {
        if (CountChar(RlistScalarValue(rp), ',') != 2)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Registry row format should be NAME,REG_SZ,VALUE, not \"%s\"", RlistScalarValue(rp));
            retval = false;
        }
    }

    for (Rlist *rp = a.database.columns; rp != NULL; rp = rp->next)
    {
        if (CountChar(rp->item, ',') > 0)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "MS registry column format should be NAME only in deletion");
            retval = false;
        }
    }

    return retval;
}

static int ValidateRegistryPromiser(char *key, Attributes a, Promise *pp)
{
    static char *valid[] = { "HKEY_CLASSES_ROOT", "HKEY_CURRENT_CONFIG",
        "HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE", "HKEY_USERS", NULL
    };
    char root_key[CF_MAXVARSIZE];
    char *sp;
    int i;

    /* First remove the root key */

    strncpy(root_key, key, CF_MAXVARSIZE - 1);
    sp = strchr(root_key, '\\');
    *sp = '\0';

    for (i = 0; valid[i] != NULL; i++)
    {
        if (strcmp(root_key, valid[i]) == 0)
        {
            return true;
        }
    }

    CfOut(OUTPUT_LEVEL_ERROR, "", "Non-editable registry prefix \"%s\"", root_key);
    PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    return false;
}

/*****************************************************************************/
/* Linker troubles require this code to be here in the main body             */
/*****************************************************************************/

static int VerifyTablePromise(CfdbConn *cfdb, char *table_path, Rlist *columns, Attributes a, Promise *pp)
{
    char name[CF_MAXVARSIZE], type[CF_MAXVARSIZE], query[CF_MAXVARSIZE], table[CF_MAXVARSIZE], db[CF_MAXVARSIZE];
    int i, count, size, no_of_cols, *size_table, *done, identified, retval = true;
    char **name_table, **type_table;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Verifying promised table structure for \"%s\"", table_path);

    if (!ValidateSQLTableName(table_path, db, table))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "",
              " !! The structure of the promiser did not match that for an SQL table, i.e. \"database.table\"\n");
        return false;
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Assuming database \"%s\" with table \"%s\"", db, table);
    }

/* Verify the existence of the tables within the database */

    if (!TableExists(cfdb, table))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! The database did not contain the promised table \"%s\"\n", table_path);

        if ((a.database.operation) && (strcmp(a.database.operation, "create") == 0))
        {
            if ((!DONTDO) && ((a.transaction.action) != cfa_warn))
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_CHG, "", pp, a, " -> Database.table %s doesn't seem to exist, creating\n",
                     table_path);
                return CreateTableColumns(cfdb, table, columns, a, pp);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", " -> Database.table %s doesn't seem to exist, but only a warning was promised\n",
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
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "Could not make sense of the columns");
        CfDeleteQuery(cfdb);
        return false;
    }

/* Assume that the Rlist has been validated and consists of a,b,c */

    count = 0;
    no_of_cols = RlistLen(columns);

    if (!NewSQLColumns(table, columns, &name_table, &type_table, &size_table, &done))
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, "Could not make sense of the columns");
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

        CfOut(OUTPUT_LEVEL_VERBOSE, "", "    ... discovered column (%s,%s,%d)", name, type, size);

        if (sizestr && (size == CF_NOINT))
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a,
                 " !! Integer size of SQL datatype could not be determined or was not specified - invalid promise.");
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
                    cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
                         " !! Promised column \"%s\" in database.table \"%s\" has a non-matching array size (%d != %d)",
                         name, table_path, size, size_table[i]);
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Promised column \"%s\" in database.table \"%s\" is as promised", name,
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
            cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
                 "Column \"%s\" found in database.table \"%s\" is not part of its promise.", name, table_path);

            if ((a.database.operation) && (strcmp(a.database.operation, "drop") == 0))
            {
                cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a,
                     "Cfengine will not promise to repair this, as the operation is potentially too destructive.");
                // Future allow deletion?
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
                CfOut(OUTPUT_LEVEL_ERROR, "", " !! Promised column \"%s\" missing from database table %s", name_table[i],
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
                    cfPS(OUTPUT_LEVEL_ERROR, CF_CHG, "", pp, a, " !! Adding promised column \"%s\" to database table %s",
                         name_table[i], table);
                    retval = true;
                }
                else
                {
                    cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, a,
                         " !! Promised column \"%s\" missing from database table %s but only a warning was promised",
                         name_table[i], table);
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
        if (strcmp(name, rp->item) == 0)
        {
            match = true;
        }
    }

    RlistDestroy(list);

    return match;
}

/*****************************************************************************/

static int CreateTableColumns(CfdbConn *cfdb, char *table, Rlist *columns, Attributes a, Promise *pp)
{
    char entry[CF_MAXVARSIZE], query[CF_BUFSIZE];
    int i, *size_table, *done;
    char **name_table, **type_table;
    int no_of_cols = RlistLen(columns);

    CfOut(OUTPUT_LEVEL_ERROR, "", " -> Trying to create table %s\n", table);

    if (!NewSQLColumns(table, columns, &name_table, &type_table, &size_table, &done))
    {
        return false;
    }

    if (no_of_cols > 0)
    {
        snprintf(query, CF_BUFSIZE - 1, "create table %s(", table);

        for (i = 0; i < no_of_cols; i++)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Forming column template %s %s %d\n", name_table[i], type_table[i],
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
        CfOut(OUTPUT_LEVEL_ERROR, "", "Could not make sense of the columns");
        CfDeleteQuery(cfdb);
        return NULL;
    }

    while (CfFetchRow(cfdb))
    {
        RlistPrependScalar(&list, CfFetchColumn(cfdb, 0));
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

    if (dot + back + fwd > true)
    {
        return false;
    }

    memset(db, 0, CF_MAXVARSIZE);
    strncpy(db, table_path, sp - table_path - 1);
    strncpy(table, sp, CF_MAXVARSIZE - 1);
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

        cols = RlistFromSplitString((char *) rp->item, ',');

        if (!cols)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "No columns promised for table \"%s\" - makes no sense", table);
            return false;
        }

        if (cols->item == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Malformed column promise for table \"%s\" - found not even a name", table);
            free(*name_table);
            free(*type_table);
            free(*size_table);
            free(*done);
            return false;
        }

        (*name_table)[i] = xstrdup((char *) cols->item);

        if (cols->next == NULL)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Malformed column \"%s\" promised for table \"%s\" - missing a type", (*name_table)[i],
                  table);
            free(*name_table);
            free(*type_table);
            free(*size_table);
            free(*done);
            return false;
        }

        (*type_table)[i] = xstrdup(cols->next->item);

        if (cols->next->next == NULL)
        {
            (*size_table)[i] = 0;
        }
        else
        {
            if (cols->next->next->item)
            {
                (*size_table)[i] = IntFromString(cols->next->next->item);
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

static int CheckSQLDataType(char *type, char *ref_type, Promise *pp)
{
    static char *aliases[3][2] =
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
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Promised column in database %s has a non-matching type (%s != %s)",
                      pp->promiser, ref_type, type);
            }
        }
        else
        {
            if (strcmp(type, ref_type) != 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Promised column in database %s has a non-matching type (%s != %s)",
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
