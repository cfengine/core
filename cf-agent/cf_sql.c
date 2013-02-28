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

#include "cf_sql.h"

#include "cfstream.h"

#ifdef HAVE_MYSQL_H
# include <mysql.h>
#elif defined(HAVE_MYSQL_MYSQL_H)
# include <mysql/mysql.h>
#endif

#ifdef HAVE_PGSQL_LIBPQ_FE_H
# include <pgsql/libpq-fe.h>
#elif defined(HAVE_LIBPQ_FE_H)
# include <libpq-fe.h>
#endif

/* Cfengine connectors for sql databases. Note that there are significant
   differences in db admin functions in the various implementations. e.g.
   sybase/mysql "use database, create database" not in postgres.
*/

/*****************************************************************************/

#ifdef HAVE_LIBMYSQLCLIENT

typedef struct
{
    MYSQL conn;
    MYSQL_RES *res;
} DbMysqlConn;

/*****************************************************************************/

static DbMysqlConn *CfConnectMysqlDB(const char *host, const char *user, const char *password, const char *database)
{
    DbMysqlConn *c;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> This is a MySQL database\n");

    c = xcalloc(1, sizeof(DbMysqlConn));

    mysql_init(&c->conn);

    if (!mysql_real_connect(&c->conn, host, user, password, database, 0, NULL, 0))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Failed to connect to existing MySQL database: %s\n", mysql_error(&c->conn));
        free(c);
        return NULL;
    }

    return c;
}

/*****************************************************************************/

static void CfCloseMysqlDB(DbMysqlConn *c)
{
    mysql_close(&c->conn);
    free(c);
}

/*****************************************************************************/

static void CfNewQueryMysqlDb(CfdbConn *c, const char *query)
{
    DbMysqlConn *mc = c->data;

    if (mysql_query(&mc->conn, query) != 0)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "MySQL query failed: %s, (%s)\n", query, mysql_error(&mc->conn));
    }
    else
    {
        mc->res = mysql_store_result(&mc->conn);

        if (mc->res)
        {
            c->result = true;
            c->maxcolumns = mysql_num_fields(mc->res);
            c->maxrows = mysql_num_rows(mc->res);
        }
    }
}

/*****************************************************************************/

static void CfFetchMysqlRow(CfdbConn *c)
{
    int i;
    MYSQL_ROW thisrow;
    DbMysqlConn *mc = c->data;

    if (c->maxrows > 0)
    {
        thisrow = mysql_fetch_row(mc->res);

        if (thisrow)
        {
            c->rowdata = xmalloc(sizeof(char *) * c->maxcolumns);

            for (i = 0; i < c->maxcolumns; i++)
            {
                c->rowdata[i] = (char *) thisrow[i];
            }
        }
        else
        {
            c->rowdata = NULL;
        }
    }
}

/*****************************************************************************/

static void CfDeleteMysqlQuery(CfdbConn *c)
{
    DbMysqlConn *mc = c->data;

    if (mc->res)
    {
        mysql_free_result(mc->res);
        mc->res = NULL;
    }
}

#else

static void *CfConnectMysqlDB(const char *host, const char *user, const char *password, const char *database)
{
    CfOut(OUTPUT_LEVEL_INFORM, "", "There is no MySQL support compiled into this version");
    return NULL;
}

static void CfCloseMysqlDB(void *c)
{
}

static void CfNewQueryMysqlDb(CfdbConn *c, const char *query)
{
}

static void CfFetchMysqlRow(CfdbConn *c)
{
}

static void CfDeleteMysqlQuery(CfdbConn *c)
{
}

#endif

#ifdef HAVE_LIBPQ

typedef struct
{
    PGconn *conn;
    PGresult *res;
} DbPostgresqlConn;

/*****************************************************************************/

static DbPostgresqlConn *CfConnectPostgresqlDB(const char *host,
                                               const char *user, const char *password, const char *database)
{
    DbPostgresqlConn *c;
    char format[CF_BUFSIZE];

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> This is a PotsgreSQL database\n");

    c = xcalloc(1, sizeof(DbPostgresqlConn));

    if (strcmp(host, "localhost") == 0)
    {
        /* Some authentication problem - ?? */
        if (database)
        {
            snprintf(format, CF_BUFSIZE - 1, "dbname=%s user=%s password=%s", database, user, password);
        }
        else
        {
            snprintf(format, CF_BUFSIZE - 1, "user=%s password=%s", user, password);
        }
    }
    else
    {
        if (database)
        {
            snprintf(format, CF_BUFSIZE - 1, "dbname=%s host=%s user=%s password=%s", database, host, user, password);
        }
        else
        {
            snprintf(format, CF_BUFSIZE - 1, "host=%s user=%s password=%s", host, user, password);
        }
    }

    c->conn = PQconnectdb(format);

    if (PQstatus(c->conn) == CONNECTION_BAD)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Failed to connect to existing PostgreSQL database: %s\n", PQerrorMessage(c->conn));
        free(c);
        return NULL;
    }

    return c;
}

/*****************************************************************************/

static void CfClosePostgresqlDb(DbPostgresqlConn *c)
{
    PQfinish(c->conn);
    free(c);
}

/*****************************************************************************/

static void CfNewQueryPostgresqlDb(CfdbConn *c, const char *query)
{
    DbPostgresqlConn *pc = c->data;

    pc->res = PQexec(pc->conn, query);

    if (PQresultStatus(pc->res) != PGRES_COMMAND_OK && PQresultStatus(pc->res) != PGRES_TUPLES_OK)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "", "PostgreSQL query failed: %s, %s\n", query, PQerrorMessage(pc->conn));
    }
    else
    {
        c->result = true;
        c->maxcolumns = PQnfields(pc->res);
        c->maxrows = PQntuples(pc->res);
    }
}

/*****************************************************************************/

static void CfFetchPostgresqlRow(CfdbConn *c)
{
    int i;
    DbPostgresqlConn *pc = c->data;

    if (c->row >= c->maxrows)
    {
        c->rowdata = NULL;
        return;
    }

    if (c->maxrows > 0)
    {
        c->rowdata = xmalloc(sizeof(char *) * c->maxcolumns);
    }

    for (i = 0; i < c->maxcolumns; i++)
    {
        c->rowdata[i] = PQgetvalue(pc->res, c->row, i);
    }
}

/*****************************************************************************/

static void CfDeletePostgresqlQuery(CfdbConn *c)
{
    DbPostgresqlConn *pc = c->data;

    PQclear(pc->res);
}

/*****************************************************************************/

#else

static void *CfConnectPostgresqlDB(const char *host, const char *user, const char *password, const char *database)
{
    CfOut(OUTPUT_LEVEL_INFORM, "", "There is no PostgreSQL support compiled into this version");
    return NULL;
}

static void CfClosePostgresqlDb(void *c)
{
}

static void CfNewQueryPostgresqlDb(CfdbConn *c, const char *query)
{
}

static void CfFetchPostgresqlRow(CfdbConn *c)
{
}

static void CfDeletePostgresqlQuery(CfdbConn *c)
{
}

#endif

/*****************************************************************************/

int CfConnectDB(CfdbConn *cfdb, DatabaseType dbtype, char *remotehost, char *dbuser, char *passwd, char *db)
{

    cfdb->connected = false;
    cfdb->type = dbtype;

/* If db == NULL, no database was specified, so we assume it has not been created yet. Need to
   open a generic database and create */

    if (db == NULL)
    {
        db = "no db specified";
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Connect to SQL database \"%s\" user=%s, host=%s (type=%d)\n", db, dbuser, remotehost,
          dbtype);

    switch (dbtype)
    {
    case DATABASE_TYPE_MYSQL:
        cfdb->data = CfConnectMysqlDB(remotehost, dbuser, passwd, db);
        break;

    case DATABASE_TYPE_POSTGRES:
        cfdb->data = CfConnectPostgresqlDB(remotehost, dbuser, passwd, db);
        break;

    default:
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "There is no SQL database selected");
        break;
    }

    if (cfdb->data)
        cfdb->connected = true;

    cfdb->blank = xstrdup("");
    return true;
}

/*****************************************************************************/

void CfCloseDB(CfdbConn *cfdb)
{
    if (!cfdb->connected)
    {
        return;
    }

    switch (cfdb->type)
    {
    case DATABASE_TYPE_MYSQL:
        CfCloseMysqlDB(cfdb->data);
        break;

    case DATABASE_TYPE_POSTGRES:
        CfClosePostgresqlDb(cfdb->data);
        break;

    default:
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "There is no SQL database selected");
        break;
    }

    cfdb->connected = false;
    free(cfdb->blank);
}

/*****************************************************************************/

void CfVoidQueryDB(CfdbConn *cfdb, char *query)
{
    if (!cfdb->connected)
    {
        return;
    }

/* If we don't need to retrieve table entries...*/
    CfNewQueryDB(cfdb, query);
    CfDeleteQuery(cfdb);
}

/*****************************************************************************/

void CfNewQueryDB(CfdbConn *cfdb, char *query)
{
    cfdb->result = false;
    cfdb->row = 0;
    cfdb->column = 0;
    cfdb->rowdata = NULL;
    cfdb->maxcolumns = 0;
    cfdb->maxrows = 0;

    CfDebug("Before Query succeeded: %s - %d,%d\n", query, cfdb->maxrows, cfdb->maxcolumns);

    switch (cfdb->type)
    {
    case DATABASE_TYPE_MYSQL:
        CfNewQueryMysqlDb(cfdb, query);
        break;

    case DATABASE_TYPE_POSTGRES:
        CfNewQueryPostgresqlDb(cfdb, query);
        break;

    default:
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "There is no SQL database selected");
        break;
    }

    CfDebug("Query succeeded: (%s) %d,%d\n", query, cfdb->maxrows, cfdb->maxcolumns);
}

/*****************************************************************************/

char **CfFetchRow(CfdbConn *cfdb)
{
    switch (cfdb->type)
    {
    case DATABASE_TYPE_MYSQL:
        CfFetchMysqlRow(cfdb);
        break;

    case DATABASE_TYPE_POSTGRES:
        CfFetchPostgresqlRow(cfdb);
        break;

    default:
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "There is no SQL database selected");
        break;
    }

    cfdb->row++;
    return cfdb->rowdata;
}

/*****************************************************************************/

char *CfFetchColumn(CfdbConn *cfdb, int col)
{
    if ((cfdb->rowdata) && (cfdb->rowdata[col]))
    {
        return cfdb->rowdata[col];
    }
    else
    {
        return NULL;
    }
}

/*****************************************************************************/

void CfDeleteQuery(CfdbConn *cfdb)
{
    switch (cfdb->type)
    {
    case DATABASE_TYPE_MYSQL:
        CfDeleteMysqlQuery(cfdb);
        break;

    case DATABASE_TYPE_POSTGRES:
        CfDeletePostgresqlQuery(cfdb);
        break;

    default:
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "There is no SQL database selected");
        break;
    }

    if (cfdb->rowdata)
    {
        free(cfdb->rowdata);
        cfdb->rowdata = NULL;
    }
}
