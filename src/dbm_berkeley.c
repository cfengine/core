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
/* File: dbm_berkeley.c                                                      */
/*                                                                           */
/*****************************************************************************/

/*
 * Implementation of the Cfengine DBM API using Berkeley DB.
 */

#include "cf3.defs.h"
#include "cf3.extern.h"

#ifdef BDB

static DBT *BDB_NewDBKey(char *name);
static DBT *BDB_NewDBComplexKey(char *key,int size);
static void BDB_DeleteDBKey(DBT *key);
static DBT *BDB_NewDBValue(const void *ptr,int size);
static void BDB_DeleteDBValue(DBT *value);

/*****************************************************************************/

int BDB_OpenDB(char *filename,DB **dbp)

{
int ret;

if ((ret = db_create(dbp,NULL,0)) != 0)
   {
   CfOut(cf_error, "",
         "BDB_OpenDB: Couldn't get database environment for %s: %s\n",
         filename, db_strerror(ret));
   return false;
   }

#ifdef CF_OLD_DB
if ((ret = ((*dbp)->open)(*dbp,filename,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((ret = ((*dbp)->open)(*dbp,NULL,filename,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
   CfOut(cf_error, "",
         "BDB_OpenDB: Couldn't open database %s: %s\n",
         filename, db_strerror(ret));
   return false;
   }

return true;
}

/*****************************************************************************/

int BDB_CloseDB(DB *dbp)

{
int ret;
if ((ret = dbp->close(dbp, 0)) == 0)
   {
   return true;
   }
else
   {
   CfOut(cf_error, "",
         "BDB_CloseDB: Unable to close database: %s\n", db_strerror(ret));
   return false;
   }
}

/*****************************************************************************/

int BDB_ValueSizeDB(DB *dbp, char *key)

{
DBT *db_key, value;
int retv;
int ret;

db_key = BDB_NewDBKey(key);
memset(&value,0,sizeof(DBT));

if ((ret = dbp->get(dbp,NULL,db_key,&value,0)) == 0)
   {
   retv = value.size;
   }
else
   {
   retv = -1;

   if (ret == DB_NOTFOUND || ret == DB_KEYEMPTY)
      {
      Debug("Key %s does not exist in database.\n", key);
      }
   else
      {
      CfOut(cf_error, "",
            "BDB_ValueSizeDB: Error trying to read database: %s\n",
            db_strerror(ret));
      }
   }

BDB_DeleteDBKey(db_key);

return retv;
}

/*****************************************************************************/

int BDB_ReadComplexKeyDB(DB *dbp,char *name,int keysize,void *ptr,int size)

{
DBT *key,value;
int ret;
bool retval = false;

key = BDB_NewDBValue(name,keysize);
memset(&value,0,sizeof(DBT));

if ((ret = dbp->get(dbp,NULL,key,&value,0)) == 0)
   {
   memset(ptr,0,size);

   if (value.data)
      {
      if (size < value.size)
         {
         memcpy(ptr,value.data,size);
         }
      else
         {
         memcpy(ptr,value.data,value.size);
         }

      Debug("READ %s\n",name);

      retval = true;
      }
   }
else
   {
   if (ret == DB_NOTFOUND || ret == DB_KEYEMPTY)
      {
      Debug("Key %.*s does not exist in database", keysize, name);
      }
   else
      {
      CfOut(cf_error, "",
            "BDB_ReadComplexKeyDB: Error trying to read database: %s\n",
            db_strerror(ret));
      }
   }

BDB_DeleteDBValue(key);
return retval;
}

/*****************************************************************************/

int BDB_RevealDB(DB *dbp,char *name,void **result,int *rsize)

{
DBT *key,value;
int ret;
bool retval = false;

key = BDB_NewDBKey(name);
memset(&value,0,sizeof(DBT));

if ((ret = dbp->get(dbp,NULL,key,&value,0)) == 0)
   {
   if (value.data)
      {
      *rsize = value.size;
      *result = value.data;
      retval = true;
      }
   }
else
   {
   if (ret == DB_NOTFOUND || ret == DB_KEYEMPTY)
      {
      Debug("Key %s does not exist in database", name);
      }
   else
      {
      CfOut(cf_error, "", "Error trying to read database: %s\n",
            db_strerror(ret));
      }
   }

BDB_DeleteDBKey(key);
return retval;
}


/*****************************************************************************/

int BDB_WriteComplexKeyDB(DB *dbp,char *name,int keysize,const void *ptr,int size)

{
DBT *key,*value;
int ret;

key = BDB_NewDBValue(name,keysize);
value = BDB_NewDBValue(ptr,size);

if ((ret = dbp->put(dbp,NULL,key,value,0)) == 0)
   {
   Debug("WriteDB => %s\n",name);

   BDB_DeleteDBValue(key);
   BDB_DeleteDBValue(value);
   return true;
   }
else
   {
   CfOut(cf_error, "",
         "BDB_WriteComplexKeyDB: Error trying to write database: %s\n",
         db_strerror(ret));

   BDB_DeleteDBKey(key);
   BDB_DeleteDBValue(value);
   return false;
   }
}

/*****************************************************************************/

int BDB_DeleteComplexKeyDB(DB *dbp,char *name,int size)

{
DBT *key;
int ret;

key = BDB_NewDBValue(name,size);

if ((ret = dbp->del(dbp,NULL,key,0)) == 0)
   {
   BDB_DeleteDBKey(key);
   return true;
   }
else
   {
   if (ret == DB_NOTFOUND || ret == DB_KEYEMPTY)
      {
      Debug("Trying to remove from database non-existing key %.*s\n",
            size, name);
      }
   else
      {
      CfOut(cf_error, "", "BDB_DeleteComplexKeyDB: "
            "Unable to remove key %.*s from database: %s\n",
            size, name, db_strerror(ret));
      }

   BDB_DeleteDBKey(key);
   return false;
   }
}

/*****************************************************************************/

int BDB_NewDBCursor(CF_DB *dbp,CF_DBC **dbcpp)

{
int ret;

if ((ret = dbp->cursor(dbp,NULL,dbcpp,0)) == 0)
   {
   return true;
   }
else
   {
   CfOut(cf_error, "",
         "BDB_NewDBCursor: Error establishing cursor for hash database: %s\n",
         db_strerror(ret));
   return false;
   }
}

/*****************************************************************************/

int BDB_NextDB(CF_DB *dbp,CF_DBC *dbcp,char **key,int *ksize,void **value,int *vsize)

{
DBT dbvalue,dbkey;
int ret;

memset(&dbkey,0,sizeof(DBT));
memset(&dbvalue,0,sizeof(DBT));

ret = dbcp->c_get(dbcp,&dbkey,&dbvalue,DB_NEXT);

*ksize = dbkey.size;
*vsize = dbvalue.size;
*key = dbkey.data;
*value = dbvalue.data;

if (ret == 0)
   {
   return true;
   }
else
   {
   if (ret != DB_NOTFOUND && ret != DB_KEYEMPTY)
      {
      CfOut(cf_error, "", "BDB_NextDB: Unable to read database: %s\n",
            db_strerror(ret));
      }
   return false;
   }
}

/*****************************************************************************/

int BDB_DeleteDBCursor(CF_DB *dbp,CF_DBC *dbcp)

{
int ret;
if ((ret = dbcp->c_close(dbcp)) == 0)
   {
   return true;
   }
else
   {
   CfOut(cf_error, "", "BDB_DeleteDBCursor: Unable to close cursor: %s\n",
         db_strerror(ret));
   return false;
   }
}

/*****************************************************************************/
/* Level 2                                                                   */
/*****************************************************************************/

static DBT *BDB_NewDBKey(char *name)

{
char *dbkey;
DBT *key;

if ((dbkey = malloc(strlen(name)+1)) == NULL)
   {
   FatalError("NewChecksumKey malloc error");
   }

if ((key = (DBT *)malloc(sizeof(DBT))) == NULL)
   {
   FatalError("DBT  malloc error");
   }

memset(key,0,sizeof(DBT));
memset(dbkey,0,strlen(name)+1);

strncpy(dbkey,name,strlen(name));

key->data = (void *)dbkey;
key->size = strlen(name)+1;

return key;
}

/*****************************************************************************/

static void BDB_DeleteDBKey(DBT *key)

{
free((char *)key->data);
free((char *)key);
}

/*****************************************************************************/

static DBT *BDB_NewDBValue(const void *ptr,int size)

{
void *val;
DBT *value;

if ((val = (void *)malloc(size)) == NULL)
   {
   FatalError("BDB_NewDBKey malloc error");
   }

if ((value = (DBT *) malloc(sizeof(DBT))) == NULL)
   {
   FatalError("DBT Value malloc error");
   }

memset(value,0,sizeof(DBT));
memcpy(val,ptr,size);

value->data = val;
value->size = size;

return value;
}

/*****************************************************************************/

static void BDB_DeleteDBValue(DBT *value)

{
free((char *)value->data);
free((char *)value);
}

#endif
