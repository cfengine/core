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

#ifdef BDB  // FIXME

int BDB_OpenDB(char *filename,DB **dbp)
 
{ DB_ENV *dbenv = NULL;

if ((errno = db_create(dbp,dbenv,0)) != 0)
   {
   CfOut(cf_error,"db_open","Couldn't get database environment for %s\n",filename);
   return false;
   }

#ifdef CF_OLD_DB
if ((errno = ((*dbp)->open)(*dbp,filename,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
if ((errno = ((*dbp)->open)(*dbp,NULL,filename,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
   CfOut(cf_error,"db_open","Couldn't open database %s\n",filename);
   return false;
   }

return true;
}

/*****************************************************************************/

int BDB_CloseDB(DB *dbp)
{
  if(dbp == NULL)
    {
      return false;
    }

  return (dbp->close(dbp, 0) == 0);
}

/*****************************************************************************/

int BDB_ValueSizeDB(DB *dbp, char *key)
{
  DBT *db_key, value;
  int retv;

  if (dbp == NULL)
    {
      return -1;
    }
 
  db_key = BDB_NewDBKey(key);
  memset(&value,0,sizeof(DBT));

  if ((errno = dbp->get(dbp,NULL,db_key,&value,0)) == 0)
    {
      retv = value.size;
    }
  else
    {
      retv = -1;
    }
 
  BDB_DeleteDBKey(db_key);
 
  return retv;
}

/*****************************************************************************/

int BDB_ReadDB(DB *dbp,char *name,void *ptr,int size)

{ DBT *key,value;

if (dbp == NULL)
   {
   return false;
   }
 
key = BDB_NewDBKey(name);
memset(&value,0,sizeof(DBT));

if ((errno = dbp->get(dbp,NULL,key,&value,0)) == 0)
   {
   memset(ptr,0,size);
   
   if (value.data)
      {
      memcpy(ptr,value.data,size);
      }
   else
      {
      BDB_DeleteDBKey(key);
      return false;
      }
   
   Debug("READ %s\n",name);
   BDB_DeleteDBKey(key);
   return true;
   }
else
   {
   Debug("Database read failed: %s",db_strerror(errno));
   BDB_DeleteDBKey(key);
   return false;
   }
}

/*****************************************************************************/

int BDB_WriteDB(DB *dbp,char *name,void *ptr,int size)

{ DBT *key,*value;

if (dbp == NULL)
   {
   return false;
   }
 
key = BDB_NewDBKey(name); 
value = BDB_NewDBValue(ptr,size);

if ((errno = dbp->put(dbp,NULL,key,value,0)) != 0)
   {
   Debug("Database write failed: %s",db_strerror(errno));
   BDB_DeleteDBKey(key);
   BDB_DeleteDBValue(value);
   return false;
   }
else
   {
   Debug("WriteDB => %s\n",name);

   BDB_DeleteDBKey(key);
   BDB_DeleteDBValue(value);
   return true;
   }
}

/*****************************************************************************/

int BDB_DeleteDB(DB *dbp,char *name)

{ DBT *key;

if (dbp == NULL)
   {
   return false;
   }
 
key = BDB_NewDBKey(name);

if ((errno = dbp->del(dbp,NULL,key,0)) != 0)
   {
   Debug("Database deletion failed: %s",db_strerror(errno));
   BDB_DeleteDBKey(key);
   return false;
   }

BDB_DeleteDBKey(key);
Debug("DELETED DB %s\n",name);
return true;
}

/*****************************************************************************/
/* Level 2                                                                   */
/*****************************************************************************/

DBT *BDB_NewDBKey(char *name)

{ char *dbkey;
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

void BDB_DeleteDBKey(DBT *key)

{
free((char *)key->data);
free((char *)key);
}

/*****************************************************************************/

DBT *BDB_NewDBValue(void *ptr,int size)

{ void *val;
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
memset(val,0,size);
memcpy(val,ptr,size);

value->data = val;
value->size = size;

return value;
}

/*****************************************************************************/

void BDB_DeleteDBValue(DBT *value)

{
free((char *)value->data);
free((char *)value);
}

#endif // FIXME
