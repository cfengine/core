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
/* File: verify_databases.c                                                  */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

void VerifyDatabasePromises(struct Promise *pp)

{ struct Attributes a;

if (pp->done)
   {
   return;
   }
 
PromiseBanner(pp);
 
a = GetDatabaseAttributes(pp);

if (!CheckDatabaseSanity(a,pp))
   {
   return;
   }

if (strcmp(a.database.type,"sql") == 0)
   {
   VerifySQLPromise(a,pp);
   return;
   }

if (strcmp(a.database.type,"ms_registry") == 0)
   {
   VerifyRegistryPromise(a,pp);
   return;
   }
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

void VerifySQLPromise(struct Attributes a,struct Promise *pp)

{ char database[CF_MAXVARSIZE],table[CF_MAXVARSIZE],query[CF_BUFSIZE];
  char *sp,sep = 'x';
  int count = 0, need_connector = false;
  CfdbConn cfdb;
  struct CfLock thislock;
  char lockname[CF_BUFSIZE];

snprintf(lockname,CF_BUFSIZE-1,"db-%s",pp->promiser);
 
thislock = AcquireLock(lockname,VUQNAME,CFSTARTTIME,a,pp);

if (thislock.lock == NULL)
   {
   return;
   }

database[0] = '\0';
table[0] = '\0';
  
for (sp = pp->promiser; *sp != '\0'; sp++)
   {
   if (IsIn(*sp,"./\\"))
      {
      count++;
      sep = *sp;
      strncpy(table,sp+1,CF_MAXVARSIZE-1);
      sscanf(pp->promiser,"%[^.\\/]",database);

      if (strlen(database) == 0)
         {
         cfPS(cf_error,CF_FAIL,"",pp,a,"SQL database promiser syntax should be of the form \"database.table\"");
         PromiseRef(cf_error,pp);
	 YieldCurrentLock(thislock);
         return;
         }
      }     
   }

if (count > 1)
   {
   cfPS(cf_error,CF_FAIL,"",pp,a,"SQL database promiser syntax should be of the form \"database.table\"");
   PromiseRef(cf_error,pp);
   }

if (strlen(database) == 0)
   {
   strncpy(database,pp->promiser,CF_MAXVARSIZE-1);
   }

if (strcmp(a.database.operation,"delete") == 0)
   {
   /* Just deal with one */
   strcpy(a.database.operation,"drop");
   }

/* Connect to the server */

CfConnectDB(&cfdb,a.database.db_server_type,a.database.db_server_host,a.database.db_server_owner,a.database.db_server_password,database);

if (!cfdb.connected)
   {
   /* If we haven't said create then db should already exist */
   
   if (a.database.operation && strcmp(a.database.operation,"create") != 0)
      {
      CfOut(cf_error,"","Could not connect an existing database %s - check server configuration?\n",database);
      PromiseRef(cf_error,pp);
      CfCloseDB(&cfdb);
      YieldCurrentLock(thislock);
      return;
      }
   }

/* Check change of existential constraints */

if (a.database.operation && strcmp(a.database.operation,"create") == 0)
   {
   CfConnectDB(&cfdb,a.database.db_server_type,a.database.db_server_host,a.database.db_server_owner,a.database.db_server_password,a.database.db_connect_db);
   
   if (!cfdb.connected)
      {
      CfOut(cf_error,"","Could not connect to the sql_db server for %s\n",database);
      return;
      }

   /* Don't drop the db if we really want to drop a table */
   
   if (strlen(table) == 0 || (strlen(table) > 0 && strcmp(a.database.operation,"drop") != 0))
      {      
      VerifyDatabasePromise(&cfdb,database,a,pp);
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

CfConnectDB(&cfdb,a.database.db_server_type,a.database.db_server_host,a.database.db_server_owner,a.database.db_server_password,database);

if (!cfdb.connected)
   {
   CfOut(cf_inform,"","Database %s is not connected\n",database);
   }
else
   {
   snprintf(query,CF_MAXVARSIZE-1,"%s.%s",database,table);
   
   if (CfVerifyTablePromise(&cfdb,query,a.database.columns,a,pp))
      {
      cfPS(cf_inform,CF_NOP,"",pp,a," -> Table \"%s\" is as promised",query);
      }
   else
      {
      cfPS(cf_inform,CF_FAIL,"",pp,a," -> Table \"%s\" is not as promised",query);
      }
   
/* Finally check any row constraints on this table */

   if (a.database.rows)
      {
      CfOut(cf_inform,""," !! Database row operations are not currently supported. Please contact cfengine with suggestions.");
      }
   
   CfCloseDB(&cfdb);
   }

YieldCurrentLock(thislock);
}

