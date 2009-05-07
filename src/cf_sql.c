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

int CfConnectDB(CfdbConn *cfdb,enum cfdbtype dbtype,char *remotehost,char *dbuser, char *passwd, char *db)

{ char format[CF_BUFSIZE];

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
       
#ifdef HAVE_LIBMYSQLCLIENT

       CfOut(cf_verbose,""," -> This is a MySQL database\n");
       mysql_init(&(cfdb->my_conn));
       
       if (!mysql_real_connect(&(cfdb->my_conn),remotehost,dbuser,passwd,db,0,NULL,0))
          {
          CfOut(cf_error,"","Failed to connect to existing MYSQL database: %s\n",mysql_error(&(cfdb->my_conn)));
          return false;
          }

       cfdb->connected = true;
#else
       CfOut(cf_inform,"","There is no MySQL support compiled into this version");  
#endif
       break;
       
   case cfd_postgres:

#ifdef HAVE_LIBPQ

       CfOut(cf_verbose,""," -> This is a PotsgreSQL database\n");
       
       if (strcmp(remotehost,"localhost") == 0)
          {
          /* Some authentication problem - ?? */
          if (db)
             {
             snprintf(format,CF_BUFSIZE-1,"dbname=%s user=%s password=%s",db,dbuser,passwd);
             }
          else
             {
             snprintf(format,CF_BUFSIZE-1,"user=%s password=%s",dbuser,passwd);
             }
          }
       else
          {
          if (db)
             {
             snprintf(format,CF_BUFSIZE-1,"dbname=%s host=%s user=%s password=%s",db,remotehost,dbuser,passwd);
             }
          else
             {
             snprintf(format,CF_BUFSIZE-1,"host=%s user=%s password=%s",remotehost,dbuser,passwd);
             }
          }

       cfdb->pq_conn = PQconnectdb(format);
       
       if (PQstatus(cfdb->pq_conn) == CONNECTION_BAD)
          {
          CfOut(cf_error,"","Failed to connect to existing POSTGRES database: %s\n",PQerrorMessage(cfdb->pq_conn));
          return false;
          }

       cfdb->connected = true;
#else
       CfOut(cf_inform,"","There is no PostgreSQL support compiled into this version");  
#endif
       break;

   default:

       CfOut(cf_verbose,"","There is no SQL database selected");
       cfdb->connected = false;       
       break;
   }

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
       
#ifdef HAVE_LIBMYSQLCLIENT
       mysql_close(&(cfdb->my_conn));
#endif
       break;
       
   case cfd_postgres:

#ifdef HAVE_LIBPQ
       PQfinish(cfdb->pq_conn); 
#endif
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

{ int i,result;

cfdb->result = false;
cfdb->row = 0;
cfdb->column = 0;
cfdb->rowdata = NULL;
cfdb->maxcolumns = 0;
cfdb->maxrows = 0;

switch (cfdb->type)
   {
   case cfd_mysql:
       
#ifdef HAVE_LIBMYSQLCLIENT

       if (mysql_query(&(cfdb->my_conn),query) != 0)
          {
          CfOut(cf_inform,"","Mysql query failed: %s, (%s)\n",query,mysql_error(&(cfdb->my_conn)));
          }
       else
          {
          cfdb->my_res = mysql_store_result(&(cfdb->my_conn));

          if (cfdb->my_res)
             {
             cfdb->maxcolumns = mysql_num_fields(cfdb->my_res);
             cfdb->maxrows = mysql_num_rows(cfdb->my_res);
             }

          cfdb->result = true;
          }
#endif
       break;
       
   case cfd_postgres:

#ifdef HAVE_LIBPQ
       
       cfdb->pq_res = PQexec(cfdb->pq_conn,query);

       if (PQresultStatus(cfdb->pq_res) != PGRES_COMMAND_OK && PQresultStatus(cfdb->pq_res) != PGRES_TUPLES_OK)
          {
          CfOut(cf_inform,"","Postgres query failed: %s, %s\n",query,PQerrorMessage(cfdb->pq_conn));
          }
       else
          {
          cfdb->result = true;
          cfdb->maxcolumns = PQnfields(cfdb->pq_res);
          cfdb->maxrows = PQntuples(cfdb->pq_res);
          }
#endif
       break;

   default:
       CfOut(cf_verbose,"","There is no SQL database selected");
       break;
   }
}

/*****************************************************************************/

char **CfFetchRow(CfdbConn *cfdb)

{ int i;
#ifdef HAVE_LIBMYSQLCLIENT
  MYSQL_ROW thisrow;
#endif

switch (cfdb->type)
   {
   case cfd_mysql:
       
#ifdef HAVE_LIBMYSQLCLIENT
       
       if (cfdb->maxrows > 0)
          {
          thisrow = mysql_fetch_row(cfdb->my_res);

          if (thisrow)
             {
             cfdb->rowdata = malloc(sizeof(char *) * cfdb->maxcolumns);
             
             for (i = 0; i < cfdb->maxcolumns; i++)
                {
                cfdb->rowdata[i] = (char *)thisrow[i];
                }
             }
          else
             {
             cfdb->rowdata = NULL;
             }
          }

#endif
       break;
       
   case cfd_postgres:

#ifdef HAVE_LIBPQ

       if (cfdb->row >= cfdb->maxrows)
          {
          cfdb->row++;
          return NULL;
          }
       
       if (cfdb->maxrows > 0)
          {
          cfdb->rowdata = malloc(sizeof(char *) * cfdb->maxcolumns);
          }
       
       for (i = 0; i < cfdb->maxcolumns; i++)
          {
          cfdb->rowdata[i] = PQgetvalue(cfdb->pq_res,cfdb->row,i);
          }
#endif
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
       
#ifdef HAVE_LIBMYSQLCLIENT
       if (cfdb->result)
          {
          mysql_free_result(cfdb->my_res);
          }
#endif
       break;
       
   case cfd_postgres:

#ifdef HAVE_LIBPQ
       PQclear(cfdb->pq_res);  
#endif
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

{ static char result[CF_BUFSIZE];
 char *spf,*spt;

if (!cfdb->connected)
   {
   return query;
   }

memset(result,0,CF_BUFSIZE);
 
switch (cfdb->type)
   {
   case cfd_mysql:
       
#ifdef HAVE_LIBMYSQLCLIENT
       mysql_real_escape_string(&(cfdb->my_conn),result,query,strlen(query));
#endif
       break;
       
   case cfd_postgres:

#ifdef HAVE_LIBPQ
       PQescapeString (result,query,strlen(query));
#endif
       break;

   default:
       CfOut(cf_verbose,"","There is no SQL database selected");
       break;
   }




return result;
}

/* EOF */

