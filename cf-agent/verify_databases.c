/*
   Copyright (C) CFEngine AS

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
#include <rlist.h>
#include <item_lib.h>

static int CheckDatabaseSanity(Attributes a, const Promise *pp);
static PromiseResult VerifySQLPromise(EvalContext *ctx, Attributes a, const Promise *pp);
static int VerifyDatabasePromise(CfdbConn *cfdb, char *database, Attributes a);

static int VerifyTablePromise(EvalContext *ctx, CfdbConn *cfdb, char *db, char *table, Rlist *columns, Attributes a, const Promise *pp, PromiseResult *result);
Rlist *QueryTableColumns(CfdbConn *cfdb, char *db_name, char *table_name, Attributes a, const Promise *pp);
static void CreateQueryForSQLServers(DatabaseType type, char *query);
static void CreateTableColumns(EvalContext *ctx, CfdbConn *cfdb, char *table, Rlist *columns, PromiseResult *result, Attributes a, const Promise *pp);
static int TableContainerExists(CfdbConn *cfdb, char *name);
static Rlist *GetCurrentSQLTables(CfdbConn *cfdb);
static int ValidateRegistryPromiser(char *s, const Promise *pp);
static int CheckRegistrySanity(Attributes a, const Promise *pp);
static PromiseResult KeepSQLPromise(char *database, char *table, EvalContext *ctx, Attributes a, const Promise *pp);
static void ConvergeSQLSchemas(EvalContext *ctx, CfdbConn *cfdb, char *database, char *table_name, Rlist *schema, Rlist *columns, Attributes a, const Promise *pp, PromiseResult *result);
static bool CompareColumns(char *a, char *b);
static bool InTable(Rlist *table, char *entry);
static bool AddEntryToSchema(EvalContext *ctx, CfdbConn *cfdb, char *table, char *entry, PromiseResult *result, Attributes a, const Promise *pp);
static int VerifyRowContent(EvalContext *ctx, CfdbConn *cfdb, char *database, char *table_name, Rlist *columns, Attributes a, const Promise *pp, PromiseResult *result);
static void ParseRow(EvalContext *ctx, char *row, Item **targets, Item **where, Attributes a, const Promise *pp, PromiseResult *result);

/*****************************************************************************/

PromiseResult VerifyDatabasePromises(EvalContext *ctx, const Promise *pp)
{
    PromiseBanner(ctx, pp);

    Attributes a = GetDatabaseAttributes(ctx, pp);

    if (!CheckDatabaseSanity(a, pp))
    {
        return PROMISE_RESULT_FAIL;
    }

    if (a.database.type == NULL)
    {
        a.database.type = "sql";
    }

    if (a.database.operation == NULL)
    {
        a.database.operation = "create";
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
    char database[CF_MAXVARSIZE], table[CF_MAXVARSIZE];
    char *sp;
    int count = 0;
    CfLock thislock;
    char lockname[CF_BUFSIZE];
    PromiseResult result = PROMISE_RESULT_NOOP;

    snprintf(lockname, CF_BUFSIZE - 1, "db-%s", pp->promiser);

    thislock = AcquireLock(ctx, lockname, VUQNAME, CFSTARTTIME, a.transaction, pp, false);
    if (thislock.lock == NULL)
    {
        return PROMISE_RESULT_SKIPPED;
    }

    database[0] = '\0';
    table[0] = '\0';

    sscanf(pp->promiser, "%128[^/\\]%*c%128s", database, table);

    for (sp = pp->promiser; *sp != '\0'; sp++)
    {
        if (*sp == FILE_SEPARATOR)
        {
            count++;
        }
    }

    if (count > 1)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "SQL database promiser syntax should be of the form \"database/table\"");
        PromiseRef(LOG_LEVEL_ERR, pp);
        return PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }

    if (strlen(database) == 0)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "SQL database promiser syntax should be of the form \"database/table\"");
        PromiseRef(LOG_LEVEL_ERR, pp);
        YieldCurrentLock(thislock);
        return PROMISE_RESULT_FAIL;
    }

    if (strcmp(a.database.operation, "delete") == 0)
    {
        /* Just deal with one */
        strcpy(a.database.operation, "drop");
    }

    result = KeepSQLPromise(database, table, ctx, a, pp);

    YieldCurrentLock(thislock);
    return result;
}

/*****************************************************************************/

static int CheckDatabaseSanity(Attributes a, const Promise *pp)
{
    int retval = true;

    if ((a.database.type) && (strcmp(a.database.type, "ms_registry") == 0))
    {
        retval = CheckRegistrySanity(a, pp);
    }
    else if ((a.database.type) && (strcmp(a.database.type, "sql") != 0))
    {
        Log(LOG_LEVEL_ERR, "Unknown database type %s in database promise", a.database.type);
        retval = false;
    }
    else // default is SQL
    {
        if ((strchr(pp->promiser, '/') == NULL) && (strchr(pp->promiser, '\\') == NULL))
        {
            if (a.database.columns)
            {
                Log(LOG_LEVEL_ERR, "Row values promised for an SQL table, but only the root database was promised in %s", pp->promiser);
                retval = false;
            }

            if (a.database.rows)
            {
                Log(LOG_LEVEL_ERR, "Columns promised for an SQL table, but only the root database was promised in %s", pp->promiser);
                retval = false;
            }
        }

        if (a.database.type && strcmp(a.database.type, "sql") == 0 && a.database.db_server_type != DATABASE_TYPE_SQLITE)
        {
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
        }
    }

    if (a.database.operation == NULL)
    {
        a.database.operation = "create";
    }

    if ((a.database.operation) && ((strcmp(a.database.operation, "delete") == 0) || (strcmp(a.database.operation, "drop") == 0)))
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

/*****************************************************************************/
/* Registry                                                                  */
/*****************************************************************************/

static int CheckRegistrySanity(Attributes a, const Promise *pp)
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

/*****************************************************************************/

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
/* SQL                                                                       */
/*****************************************************************************/

static PromiseResult KeepSQLPromise(char *database, char *table, EvalContext *ctx, Attributes a, const Promise *pp)
{
    CfdbConn cfdb;
    PromiseResult result = PROMISE_RESULT_NOOP;

    CfConnectDB(&cfdb, a.database.db_server_type, a.database.db_server_host, a.database.db_server_owner,
                a.database.db_server_password, database, a.database.db_directory);

    if (!cfdb.connected)
    {
        /* If we haven't said create then db should already exist */

        if ((a.database.operation) && (strcmp(a.database.operation, "create") != 0))
        {
            Log(LOG_LEVEL_INFO, "Could not connect an existing database '%s' - check server configuration?", database);
            PromiseRef(LOG_LEVEL_INFO, pp);
            CfCloseDB(&cfdb);
            return PROMISE_RESULT_FAIL;
        }
    }

/* Check change of existential constraints - only applies to the servers, not embedded dbs which are files */

    if (a.database.db_server_type != DATABASE_TYPE_SQLITE)
    {
        if ((a.database.operation) && (strcmp(a.database.operation, "create") == 0))
        {
            CfConnectDB(&cfdb, a.database.db_server_type, a.database.db_server_host, a.database.db_server_owner,
                        a.database.db_server_password, a.database.db_connect_db,a. database.db_directory);

            if (!cfdb.connected)
            {
                Log(LOG_LEVEL_INFO, "Could not connect to the sql_db server for '%s'", database);
                return PROMISE_RESULT_FAIL;
            }

            VerifyDatabasePromise(&cfdb, database, a);

            /* Close the database here to commit the change - might have to reopen */

            CfCloseDB(&cfdb);
        }
    }

    /* Now check the structure of the named table, if any */

    if (strlen(table) == 0)
    {
        return result;
    }

    CfConnectDB(&cfdb, a.database.db_server_type, a.database.db_server_host, a.database.db_server_owner,
                a.database.db_server_password, database, a.database.db_directory);

    if (!cfdb.connected)
    {
        Log(LOG_LEVEL_INFO, "Database '%s' is not connected", database);
    }
    else
    {
        if (VerifyTablePromise(ctx, &cfdb, database, table, a.database.columns, a, pp, &result))
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_NOOP, pp, a, "Table '%s' is as promised", table);
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Table '%s' promise could not be kept", table);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }

/* Finally check any row constraints on this table */

        if (a.database.rows)
        {
            VerifyRowContent(ctx, &cfdb, database, table, a.database.columns, a, pp, &result);
        }
    }

    CfCloseDB(&cfdb);
    return result;
}

/*****************************************************************************/

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

    if (cfdb->type != DATABASE_TYPE_SQLITE)
    {
        CreateQueryForSQLServers(cfdb->type, query);

        CfNewQueryDB(cfdb, query);

        if (cfdb->maxcolumns < 1)
        {
            Log(LOG_LEVEL_INFO, "The schema did not promise the expected number of fields - got %d expected >= %d", cfdb->maxcolumns, 1);
            return false;
        }

        while (CfFetchRow(cfdb))
        {
            strncpy(name, CfFetchColumn(cfdb, 0), CF_MAXVARSIZE - 1);

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

    return true;
}

/*****************************************************************************/

static int VerifyTablePromise(EvalContext *ctx, CfdbConn *cfdb, char *database, char *table_name, Rlist *columns, Attributes a, const Promise *pp, PromiseResult *result)
{
    Log(LOG_LEVEL_VERBOSE,"Verifying promised table structure for '%s'", table_name);

    /* Verify the existence of the tables within the database */

    if (!TableContainerExists(cfdb, table_name))
    {
        Log(LOG_LEVEL_INFO, "The database did not contain the promised table '%s'", table_name);

        // Need to fill it with all of its tables

        if ((a.database.operation) && (strcmp(a.database.operation, "create") == 0))
        {
            CreateTableColumns(ctx, cfdb, table_name, columns, result, a, pp);
            return true;
        }

        return false;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "The database already contains promised table '%s' - verifying schema", table_name);
        Rlist *schema = QueryTableColumns(cfdb, database, table_name, a, pp);
        ConvergeSQLSchemas(ctx, cfdb, database, table_name, schema, columns, a, pp, result);
        return true;
    }
}

/*****************************************************************************/

static int VerifyRowContent(EvalContext *ctx, CfdbConn *cfdb, ARG_UNUSED char *database, char *table_name, ARG_UNUSED Rlist *columns, Attributes a, const Promise *pp, PromiseResult *result)
{
    int where_size = 0;
    int target_size = 0;
    int value_size = 0;

    Log(LOG_LEVEL_VERBOSE, "Verifying promised table content (rows) for '%s'\n", table_name);

    if (a.database.rows)
    {
        for (Rlist *rp = a.database.rows; rp != NULL; rp=rp->next)
        {
            Item *targets = NULL, *where= NULL;
            ParseRow(ctx, rp->val.item, &targets, &where, a, pp, result);

            for (Item *ip = targets; ip != NULL; ip=ip->next)
            {
                target_size += strlen(ip->name) + 3;
                value_size += strlen(ip->classes) + 3;
            }

            for (Item *ip = where; ip != NULL; ip=ip->next)
            {
                where_size += strlen(ip->name) + 5;
                where_size += strlen(ip->classes) + 5;
            }

            char *inserts = xmalloc(target_size);
            char *updates = xmalloc(target_size + value_size);
            char *wheres = xmalloc(where_size);
            char *where_inserts = xmalloc(where_size);
            char *where_values = xmalloc(where_size);
            char *values = xmalloc(value_size);

            inserts[0] = '\0';
            updates[0] = '\0';
            wheres[0] = '\0';
            where_inserts[0] = '\0';
            where_values[0] = '\0';
            values[0] = '\0';

            for (Item *ip = targets; ip != NULL; ip=ip->next)
            {
                strcat(inserts, ip->name);
                strcat(values, ip->classes);

                strcat(updates, ip->name);
                strcat(updates, "=");
                strcat(updates, ip->classes);

                if (ip->next)
                {
                    strcat(values, ",");
                    strcat(inserts, ",");
                    strcat(updates, ",");
                }
            }

            for (Item *ip = where; ip != NULL; ip=ip->next)
            {
                strcat(wheres, ip->name);
                strcat(wheres, "=");
                strcat(wheres, ip->classes);

                strcat(where_inserts, ip->name);
                strcat(where_values, ip->classes);

                if (ip->next)
                {
                    strcat(wheres, " and ");
                    strcat(where_inserts, ",");
                    strcat(where_values, ",");
                }
                else
                {
                    strcat(wheres, "");
                }
            }

            char *check_query = xmalloc(strlen("insert into values where()()")+target_size+value_size+where_size+strlen(table_name)+10);
            char *insert_query = xmalloc(strlen("insert into values where()()")+target_size+value_size+where_size+strlen(table_name)+10);
            char *update_query = xmalloc(strlen("insert into values where()()")+target_size+value_size+where_size+strlen(table_name)+10);

            sprintf(check_query, "select %s from %s where %s", inserts, table_name, wheres);
            sprintf(insert_query, "insert into %s (%s,%s) values(%s,%s)", table_name, inserts, where_inserts, values, where_values);
            sprintf(update_query, "update %s set %s where %s", table_name, updates, wheres);

            CfNewQueryDB(cfdb, check_query);

            bool update = false;
            bool insert = true;
            Item *ip;
            int i;

            if (CfFetchRow(cfdb)) // Check that all the columns match in the returned data
            {
                insert = false; // Can't sql-insert a new row if there one exists already

                for (ip = targets, i = 0; (ip != NULL) && (i < cfdb->maxcolumns); i++, ip=ip->next)
                {
                    char *match = CfFetchColumn(cfdb, i);

                    if (strncmp(ip->classes+1, match, strlen(ip->classes)-2) != 0) // strip off '..'
                    {
                        update = true; // Need to sql-update existing row
                        break;
                    }
                }

                // Everything is ok, nothing to do
            }

            CfDeleteQuery(cfdb);

            if (insert)
            {
                if ((!DONTDO) && ((a.transaction.action) != cfa_warn))
                {
                    CfVoidQueryDB(cfdb, insert_query);
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Adding promised row '%s' to database table '%s'", insert_query, pp->promiser);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a, "Need to add promised row '%s' to database table '%s' but only a warning was promised", insert_query, pp->promiser);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                }
            }

            if (update)
            {
                if ((!DONTDO) && ((a.transaction.action) != cfa_warn))
                {
                    CfVoidQueryDB(cfdb, update_query);
                    cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Updating promised row '%s' to database table '%s'", update_query, pp->promiser);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
                }
                else
                {
                    cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a, "Need to updating promised row '%s' to database table '%s' but only a warning was promised", update_query, pp->promiser);
                    *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
                }
            }

            free(wheres);
            free(where_inserts);
            free(where_values);
            free(inserts);
            free(updates);
            free(values);
            free(check_query);
            free(insert_query);
            free(update_query);
        }
    }

    return true;
}

/*****************************************************************************/

static void ParseRow(EvalContext *ctx, char *row, Item **targets, Item **where, Attributes a, const Promise *pp, PromiseResult *result)
{
    char item[CF_MAXVARSIZE], name[CF_MAXVARSIZE], operand[CF_MAXVARSIZE];
    char operator[CF_SMALLBUF];
    int skip;

    for (char *sp = row; *sp != '\0'; sp += skip)
    {
        if (*sp == ',' || *sp == ' ')
        {
            skip = 1;
            continue;
        }

        item[0] = '\0';
        name[0] = '\0';
        operator[0] = '\0';
        operand[0] = '\0';
        sscanf(sp, "%128[^,]", item);
        skip = strlen(item);

        sscanf(item, "%64[^=~ ] %3[^' ] %64s", name, operator, operand);

        if (strcmp(operator, "=>") == 0)
        {
            PrependItem(targets, name, operand);
        }
        else if (strcmp(operator, "=") == 0)
        {
            PrependItem(where, name, operand);
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_FAIL, pp, a, "Badly formatted row '%s' for database table '%s'", item, pp->promiser);
            *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        }
    }
}

/*****************************************************************************/

static void ConvergeSQLSchemas(EvalContext *ctx, CfdbConn *cfdb, ARG_UNUSED char *database, char *table_name, Rlist *schema, Rlist *policy, Attributes a, const Promise *pp, PromiseResult *result)
{
    for (Rlist *rpp = schema; rpp != NULL; rpp=rpp->next)
    {
        if (!InTable(policy, rpp->val.item))
        {
            Log(LOG_LEVEL_VERBOSE, "The schema has an unknown field: \"%s\"", (char *)rpp->val.item);
            //DeleteEntryFromSchema(rpp->val.item, cfdb);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Table %s already has field \"%s\"", table_name, (char *)rpp->val.item);
        }
    }

    for (Rlist *rpp = policy; rpp != NULL; rpp=rpp->next)
    {
        if (!InTable(schema, rpp->val.item))
        {
            Log(LOG_LEVEL_VERBOSE, "Table %s needs to add field \"%s\"", table_name, (char *)rpp->val.item);
            AddEntryToSchema(ctx, cfdb, table_name, rpp->val.item, result, a, pp);
        }
    }

}

/*****************************************************************************/

static bool InTable(Rlist *table, char *entry)
{
    for (Rlist *rp = table; rp != NULL; rp=rp->next)
    {
        if (CompareColumns(rp->val.item, entry))
        {
            return true;
        }
    }

    return false;
}

/*****************************************************************************/

static bool CompareColumns(char *a, char *b)
{
    char a1[CF_MAXVARSIZE] = {0}, a2[CF_MAXVARSIZE] = {0};
    char b1[CF_MAXVARSIZE] = {0}, b2[CF_MAXVARSIZE] = {0};

    sscanf(a, "%s %s", a1, a2);
    sscanf(b, "%s %s", b1, b2);
    ToLowerStrInplace(a1);
    ToLowerStrInplace(a2);
    ToLowerStrInplace(b1);
    ToLowerStrInplace(b2);

    if ((strcmp(a1,b1) == 0) && (strcmp(a2,b2) == 0))
    {
        return true;
    }

    return false;
}

/*****************************************************************************/

static bool AddEntryToSchema(EvalContext *ctx, CfdbConn *cfdb, char *table, char *entry, PromiseResult *result, Attributes a, const Promise *pp)
{
    char query[CF_BUFSIZE];

    if ((!DONTDO) && ((a.transaction.action) != cfa_warn))
    {
        snprintf(query, CF_MAXVARSIZE - 1, "ALTER TABLE %s ADD %s", table, entry);

        CfVoidQueryDB(cfdb, query);
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Adding promised column '%s' to database table '%s'", entry, table);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);
        return true;
    }
    else
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_WARN, pp, a, "Promised column '%s' missing from database table '%s' but only a warning was promised", entry, table);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
        return false;
    }
}

/*****************************************************************************/

static int TableContainerExists(CfdbConn *cfdb, char *name)
{
    Rlist *rp, *list = NULL;
    int match = false;

    list = GetCurrentSQLTables(cfdb);

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

static void CreateTableColumns(EvalContext *ctx, CfdbConn *cfdb, char *table, Rlist *columns, PromiseResult *result, Attributes a, const Promise *pp)
{
    int size = 0;

    if ((!DONTDO) && ((a.transaction.action) != cfa_warn))
    {
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Database/table '%s' doesn't seem to exist, creating", table);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_CHANGE);

        for (Rlist *rp = columns; rp != NULL; rp=rp->next)
        {
            size += strlen(rp->val.item) + 3;
        }

        char *query = xmalloc(size+strlen("create table")+strlen(table)+5);

        sprintf(query, "create table %s(", table);

        for (Rlist *rp = columns; rp != NULL; rp=rp->next)
        {
            char item[CF_MAXVARSIZE];
            snprintf(item, CF_MAXVARSIZE, "%s,", (char *)rp->val.item);
            strcat(query, item);
        }

        query[strlen(query)-1] = ')';

        CfVoidQueryDB(cfdb, query);
        free(query);
    }
    else
    {
        cfPS(ctx, LOG_LEVEL_WARNING, PROMISE_RESULT_WARN, pp, a, "Database.table '%s' doesn't seem to exist, but only a warning is promised", table);
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_WARN);
    }
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static Rlist *GetCurrentSQLTables(CfdbConn *cfdb)
{
    Rlist *list = NULL;
    char query[CF_MAXVARSIZE];

    switch (cfdb->type)
    {
    case DATABASE_TYPE_MYSQL:
        snprintf(query, CF_MAXVARSIZE - 1, "show tables");
        break;

    case DATABASE_TYPE_POSTGRES:
        /* This gibberish is the simplest thing I can find in postgres */

        snprintf(query, CF_MAXVARSIZE - 1,
                 "SELECT c.relname as \"Name\" FROM pg_catalog.pg_class c JOIN pg_catalog.pg_roles r ON r.oid = c.relowner LEFT JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace WHERE n.nspname = 'public'");
        break;

    case DATABASE_TYPE_SQLITE:
        snprintf(query, CF_MAXVARSIZE - 1, "SELECT name FROM sqlite_master WHERE type='table'");
        break;

    default:
        snprintf(query, CF_MAXVARSIZE, "NULL QUERY");
        break;
    }


    CfNewQueryDB(cfdb, query);

    if (cfdb->maxcolumns != 1)
    {
        Log(LOG_LEVEL_ERR, "Could not make sense of the columns");
        CfDeleteQuery(cfdb);
        return NULL;
    }

    while (CfFetchRow(cfdb))
    {
        char *sp = CfFetchColumn(cfdb, 0);
        RlistAppendScalar(&list, sp);
    }

    CfDeleteQuery(cfdb);

    return list;
}

/*****************************************************************************/

static void CreateQueryForSQLServers(DatabaseType type, char *query)
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

    case DATABASE_TYPE_SQLITE:
        // Doesn't apply, as each database is a standalone file
        break;

    default:
        snprintf(query, CF_MAXVARSIZE, "NULL QUERY");
        break;
    }
}

/*****************************************************************************/

Rlist *QueryTableColumns(CfdbConn *cfdb, char *db_name, char *table_name, ARG_UNUSED Attributes a, ARG_UNUSED const Promise *pp)
{
    char query[CF_BUFSIZE], row[CF_BUFSIZE];
    Rlist *schema = NULL;

    switch (cfdb->type)
    {
    case DATABASE_TYPE_MYSQL:
        //? who removed this?
        break;

    case DATABASE_TYPE_POSTGRES:
        snprintf(query, CF_MAXVARSIZE - 1, "SELECT column_name,data_type,character_maximum_length FROM information_schema.columns "
                 "WHERE table_name ='%s' AND table_schema = '%s'",
                 table_name, db_name);
        break;

    case DATABASE_TYPE_SQLITE:
        snprintf(query, CF_MAXVARSIZE - 1, "SELECT sql FROM sqlite_master WHERE tbl_name = '%s'",
                 table_name);
        break;

    default:
        Log(LOG_LEVEL_VERBOSE, "There is no SQL database selected");
        return NULL;
    }

    CfNewQueryDB(cfdb, query);

    if (cfdb->maxcolumns < 1)
    {
        Log(LOG_LEVEL_INFO, "The schema did not promise the expected number of fields - got %d expected >= %d", cfdb->maxcolumns, 1);
        return false;
    }

    switch (cfdb->type)
    {
    case DATABASE_TYPE_MYSQL:
        break;

    case DATABASE_TYPE_POSTGRES:
        break;

    case DATABASE_TYPE_SQLITE:

        if (CfFetchRow(cfdb))
        {
            char *sp, *value = CfFetchColumn(cfdb, 0);
            char *end = value + strlen(value);
            int balance = 0;

            sp = value + strlen("CREATE TABLE ") + strlen(table_name) + 1;

            while (sp < end)
            {
                sscanf(sp, "%[^,]", row);
                sp += strlen(row) + 1;


                for (char *spp = row; *spp != '\0'; spp++)
                {
                    switch (*spp)
                    {
                    case '(':
                        balance++;
                        break;
                    case ')':
                        balance--;
                        break;
                    default:
                        break;
                    }
                }

                if (balance && row[strlen(row)-1] == ')')
                {
                    row[strlen(row)-1] = '\0';
                }

                // now strip off '..'

                RlistAppendScalar(&schema, row);
                row[0] = '\0';
            }
        }
        break;

    default:
        return NULL; // Unreachable
    }
    return schema;
}
