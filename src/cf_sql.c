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

/*****************************************************************************/
/*                                                                           */
/* File: cf_sql.c                                                            */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#ifdef HAVE_MYSQL_MYSQL_H
#include <mysql/mysql.h>
#endif

#ifdef HAVE_PGSQL_LIBPQ_FE_H
 #include <pgsql/libpq-fe.h>
#elif defined(HAVE_LIBPQ_FE_H)
 #include <libpq-fe.h>
#endif

/* Cfengine connectors for sql databases. Note that there are significant
   differences in db admin functions in the various implementations. e.g.
   sybase/mysql "use database, create database" not in postgres.


CfConnectDB(&cfdb,SQL_TYPE,SQL_SERVER,SQL_OWNER,SQL_PASSWD,SQL_DATABASE);

if (!cfdb.connected)
   {
   printf("Could not open sqldb\n");
   return;
   }

CfNewQueryDB(&cfdb,"SELECT * from topics");

while(CfFetchRow(&cfdb))
   {
   for (i = 0; i < cfdb.maxcolumns; i++)
      {
      printf("Row %d: %s\n",i,CfFetchColumn(&cfdb,i));
      }
   }

CfDeleteQuery(&cfdb);

CfCloseDB(&cfdb);
*/


/*****************************************************************************/

#ifdef HAVE_LIBMYSQLCLIENT

struct CfDbMysqlConn
{
    MYSQL conn;
    MYSQL_RES *res;
};

/*****************************************************************************/

static struct CfDbMysqlConn *CfConnectMysqlDB(const char *host,
                                              const char *user,
                                              const char *password,
                                              const char *database)
{
struct CfDbMysqlConn *c;

CfOut(cf_verbose,""," -> This is a MySQL database\n");

c = calloc(1, sizeof(struct CfDbMysqlConn));
if (!c)
   {
   CfOut(cf_error, "", "Failed to allocate memory to store MySQL database information");
   return NULL;
   }

mysql_init(&c->conn);

if (!mysql_real_connect(&c->conn, host, user, password,
                        database, 0, NULL, 0))
   {
   CfOut(cf_error,"","Failed to connect to existing MySQL database: %s\n",mysql_error(&c->conn));
   free(c);
   return NULL;
   }

return c;
}

/*****************************************************************************/

static void CfCloseMysqlDB(struct CfDbMysqlConn *c)
{
mysql_close(&c->conn);
free(c);
}

/*****************************************************************************/

static void CfNewQueryMysqlDb(CfdbConn *c, const char *query)
{
struct CfDbMysqlConn *mc = c->data;

if (mysql_query(&mc->conn, query) != 0)
   {
   CfOut(cf_inform,"","MySQL query failed: %s, (%s)\n",query,mysql_error(&mc->conn));
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
struct CfDbMysqlConn *mc = c->data;

if (c->maxrows > 0)
   {
   thisrow = mysql_fetch_row(mc->res);

   if (thisrow)
      {
      c->rowdata = malloc(sizeof(char *) * c->maxcolumns);

      for (i = 0; i < c->maxcolumns; i++)
         {
         c->rowdata[i] = (char *)thisrow[i];
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
struct CfDbMysqlConn *mc = c->data;

if (mc->res)
   {
   mysql_free_result(mc->res);
   mc->res = NULL;
   }
}

/*****************************************************************************/

static void CfEscapeMysqlSQL(CfdbConn *c, const char *query, char *result)
{
struct CfDbMysqlConn *mc = c->data;
mysql_real_escape_string(&mc->conn, result, query, strlen(query));
}

#else

static void *CfConnectMysqlDB(const char *host,
                              const char *user,
                              const char *password,
                              const char *database)

{
CfOut(cf_inform,"","There is no MySQL support compiled into this version");
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

static void CfEscapeMysqlSQL(CfdbConn *c, const char *query, char *result)
{
}

#endif

#ifdef HAVE_LIBPQ

struct CfDbPostgresqlConn
{
PGconn *conn;
PGresult *res;
};

/*****************************************************************************/

static struct CfDbPostgresqlConn *CfConnectPostgresqlDB(const char *host,
                                                        const char *user,
                                                        const char *password,
                                                        const char *database)
{
struct CfDbPostgresqlConn *c;
char format[CF_BUFSIZE];

CfOut(cf_verbose,""," -> This is a PotsgreSQL database\n");

c = calloc(1, sizeof(struct CfDbPostgresqlConn));
if (!c)
   {
   CfOut(cf_error, "", "Failed to allocate memory to store PostgreSQL database information");
   return NULL;
   }

if (strcmp(host,"localhost") == 0)
   {
   /* Some authentication problem - ?? */
   if (database)
      {
      snprintf(format,CF_BUFSIZE-1,"dbname=%s user=%s password=%s",database,user,password);
      }
   else
      {
      snprintf(format,CF_BUFSIZE-1,"user=%s password=%s",user,password);
      }
   }
else
   {
   if (database)
      {
      snprintf(format,CF_BUFSIZE-1,"dbname=%s host=%s user=%s password=%s",database,host,user,password);
      }
   else
      {
      snprintf(format,CF_BUFSIZE-1,"host=%s user=%s password=%s",host,user,password);
      }
   }

c->conn = PQconnectdb(format);

if (PQstatus(c->conn) == CONNECTION_BAD)
   {
   CfOut(cf_error,"","Failed to connect to existing PostgreSQL database: %s\n",PQerrorMessage(c->conn));
   free(c);
   return NULL;
   }

return c;
}

/*****************************************************************************/

static void CfClosePostgresqlDb(struct CfDbPostgresqlConn *c)
{
PQfinish(c->conn);
free(c);
}

/*****************************************************************************/

static void CfNewQueryPostgresqlDb(CfdbConn *c, const char *query)
{
struct CfDbPostgresqlConn *pc = c->data;

pc->res = PQexec(pc->conn, query);

if (PQresultStatus(pc->res) != PGRES_COMMAND_OK && PQresultStatus(pc->res) != PGRES_TUPLES_OK)
   {
   CfOut(cf_inform,"","PostgreSQL query failed: %s, %s\n",query,PQerrorMessage(pc->conn));
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
struct CfDbPostgresqlConn *pc = c->data;

if (c->row >= c->maxrows)
   {
   c->rowdata = NULL;
   return;
   }

if (c->maxrows > 0)
   {
   c->rowdata = malloc(sizeof(char *) * c->maxcolumns);
   }

for (i = 0; i < c->maxcolumns; i++)
   {
   c->rowdata[i] = PQgetvalue(pc->res,c->row,i);
   }
}

/*****************************************************************************/

static void CfDeletePostgresqlQuery(CfdbConn *c)
{
struct CfDbPostgresqlConn *pc = c->data;
PQclear(pc->res);
}

/*****************************************************************************/

static void CfEscapePostgresqlSQL(CfdbConn *c, const char *query, char *result)
{
PQescapeString(result, query, strlen(query));
}

#else

static void *CfConnectPostgresqlDB(const char *host,
                                   const char *user,
                                   const char *password,
                                   const char *database)
{
CfOut(cf_inform,"","There is no PostgreSQL support compiled into this version");
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

static void CfEscapePostgresqlSQL(CfdbConn *c, const char *query, char *result)
{
}

#endif

/*****************************************************************************/

int CfConnectDB(CfdbConn *cfdb,enum cfdbtype dbtype,char *remotehost,char *dbuser, char *passwd, char *db)

{

cfdb->connected = false;
cfdb->type = dbtype;

/* If db == NULL, no database was specified, so we assume it has not been created yet. Need to
   open a generic database and create */

if (db == NULL)
   {
   db = "no db specified";
   }

CfOut(cf_verbose,"","Connect to SQL database \"%s\" user=%s, host=%s (type=%d)\n",db,dbuser,remotehost,dbtype);

switch (dbtype)
   {
   case cfd_mysql:
       cfdb->data = CfConnectMysqlDB(remotehost, dbuser, passwd, db);
       break;

   case cfd_postgres:
       cfdb->data = CfConnectPostgresqlDB(remotehost, dbuser, passwd, db);
       break;

   default:
       CfOut(cf_verbose,"","There is no SQL database selected");
       break;
   }

if (cfdb->data)
    cfdb->connected = true;

cfdb->blank = strdup("");
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
   case cfd_mysql:
       CfCloseMysqlDB(cfdb->data);
       break;

   case cfd_postgres:
       CfClosePostgresqlDb(cfdb->data);
       break;

   default:
       CfOut(cf_verbose,"","There is no SQL database selected");
       break;
   }

cfdb->connected = false;
free(cfdb->blank);
}

/*****************************************************************************/

void CfVoidQueryDB(CfdbConn *cfdb,char *query)

{
if (!cfdb->connected)
   {
   return;
   }

/* If we don't need to retrieve table entries...*/
CfNewQueryDB(cfdb,query);
CfDeleteQuery(cfdb);
}

/*****************************************************************************/

void CfNewQueryDB(CfdbConn *cfdb,char *query)

{
cfdb->result = false;
cfdb->row = 0;
cfdb->column = 0;
cfdb->rowdata = NULL;
cfdb->maxcolumns = 0;
cfdb->maxrows = 0;

Debug("Before Query succeeded: %s - %d,%d\n",query,cfdb->maxrows,cfdb->maxcolumns);

switch (cfdb->type)
   {
   case cfd_mysql:
       CfNewQueryMysqlDb(cfdb, query);
       break;

   case cfd_postgres:
       CfNewQueryPostgresqlDb(cfdb, query);
       break;

   default:
       CfOut(cf_verbose,"","There is no SQL database selected");
       break;
   }

Debug("Query succeeded: (%s) %d,%d\n",query,cfdb->maxrows,cfdb->maxcolumns);
}

/*****************************************************************************/

char **CfFetchRow(CfdbConn *cfdb)

{
switch (cfdb->type)
   {
   case cfd_mysql:
       CfFetchMysqlRow(cfdb);
       break;

   case cfd_postgres:
       CfFetchPostgresqlRow(cfdb);
       break;

   default:
       CfOut(cf_verbose,"","There is no SQL database selected");
       break;
   }

cfdb->row++;
return cfdb->rowdata;
}

/*****************************************************************************/

char *CfFetchColumn(CfdbConn *cfdb,int col)

{
if (cfdb->rowdata && cfdb->rowdata[col])
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
   case cfd_mysql:
       CfDeleteMysqlQuery(cfdb);
       break;

   case cfd_postgres:
       CfDeletePostgresqlQuery(cfdb);
       break;

   default:
       CfOut(cf_verbose,"","There is no SQL database selected");
       break;
   }

if (cfdb->rowdata)
   {
   free(cfdb->rowdata);
   cfdb->rowdata = NULL;
   }
}

/*****************************************************************************/

char *EscapeSQL(CfdbConn *cfdb,char *query)

{
static char result[CF_BUFSIZE];

if (!cfdb->connected)
   {
   return query;
   }

memset(result,0,CF_BUFSIZE);

switch (cfdb->type)
   {
   case cfd_mysql:
       CfEscapeMysqlSQL(cfdb, query, result);
       break;

   case cfd_postgres:
       CfEscapePostgresqlSQL(cfdb, query, result);
       break;

   default:
       CfOut(cf_verbose,"","There is no SQL database selected");
       break;
   }

return result;
}


/*****************************************************************************/

void Debugcfdb(CfdbConn *cfdb)
{
printf("SIZE of CfdbConn: %d = %d\n",sizeof(CfdbConn),sizeof(*cfdb));
printf( "cfdb->result = %d\n",cfdb->result);
printf( "cfdb->row = %d\n",cfdb->row);
printf( "cfdb->column = %d\n",cfdb->column);
printf( "cfdb->maxcolumns = %d\n",cfdb->maxcolumns);
printf( "cfdb->maxrows = %d\n",cfdb->maxrows);
printf( "cfdb->type = %d\n",cfdb->type);
}

/*****************************************************************************/

int SizeCfSQLContainer()
{
return sizeof(CfdbConn);
}

/* EOF */


