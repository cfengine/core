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
/* File: dbm_api.c                                                           */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"


static int DoOpenDB(char *filename, CF_DB **dbp);
static int DoCloseDB(CF_DB *dbp);
static int SaveDBHandle(CF_DB *dbp);
static int RemoveDBHandle(CF_DB *dbp);
static int GetDBHandle(CF_DB **dbp);


CF_DB *OPENDB[MAX_OPENDB] = {0};


int OpenDB(char *filename, CF_DB **dbp)
{
int res;

Debug("OpenDB(%s)\n", filename);

res = DoOpenDB(filename, dbp);

// record open DBs if successful
if(res)
  {
  if(!SaveDBHandle(*dbp))
    {
    FatalError("OpenDB: Could not save DB handle");
    }
  }

return res;
}

/*****************************************************************************/

static int DoOpenDB(char *filename, CF_DB **dbp)
{
#ifdef TCDB
return TCDB_OpenDB(filename, dbp);
#elif defined QDB
return QDB_OpenDB(filename, dbp);
#else
return BDB_OpenDB(filename, dbp);
#endif
}

/*****************************************************************************/

int CloseDB(CF_DB *dbp)
{
int res;

res = DoCloseDB(dbp);

if (res)
   {
   if (!RemoveDBHandle(dbp))
      {
      FatalError("CloseDB: Could not find DB handle in open pool\n");
      }
   }

return res;
}

/*****************************************************************************/

static int DoCloseDB(CF_DB *dbp)
{
#ifdef TCDB
return TCDB_CloseDB(dbp);;
#elif defined QDB
return QDB_CloseDB(dbp);
#else
return BDB_CloseDB(dbp);
#endif
}

/*****************************************************************************/

int ValueSizeDB(CF_DB *dbp, char *key)
/* Returns size of value corresponding to key, or -1 on not found or error */
{
#ifdef TCDB
return TCDB_ValueSizeDB(dbp, key);
#elif defined QDB
return QDB_ValueSizeDB(dbp, key);
#else
return BDB_ValueSizeDB(dbp, key);
#endif
}

/*****************************************************************************/

int ReadComplexKeyDB(CF_DB *dbp, char *key, int keySz,void *dest, int destSz)
{
#ifdef TCDB
return TCDB_ReadComplexKeyDB(dbp, key, keySz, dest, destSz);
#elif defined QDB
return QDB_ReadComplexKeyDB(dbp, key, keySz, dest, destSz);
#else
return BDB_ReadComplexKeyDB(dbp, key, keySz, dest, destSz);
#endif
}

/*****************************************************************************/

int RevealDB(CF_DB *dbp, char *key, void **result, int *rsize)
/* Allocates memory for result, which is later freed automatically (on
   next call to this function or db close) */
{
#ifdef TCDB
return TCDB_RevealDB(dbp,key,result,rsize);
#elif defined QDB
return QDB_RevealDB(dbp,key,result,rsize);
#else
return BDB_RevealDB(dbp,key,result,rsize);
#endif
}

/*****************************************************************************/

int WriteComplexKeyDB(CF_DB *dbp, char *key, int keySz, void *src, int srcSz)
{
#ifdef TCDB
return TCDB_WriteComplexKeyDB(dbp, key, keySz, src, srcSz);
#elif defined QDB
return QDB_WriteComplexKeyDB(dbp, key, keySz, src, srcSz);
#else
return BDB_WriteComplexKeyDB(dbp, key, keySz, src, srcSz);
#endif
}

/*****************************************************************************/

int DeleteComplexKeyDB(CF_DB *dbp, char *key, int size)
/**
 * Delete a record (key,value pair)
 */
{
#ifdef TCDB
return TCDB_DeleteComplexKeyDB(dbp,key,size);
#elif defined QDB
return QDB_DeleteComplexKeyDB(dbp,key,size);
#else
return BDB_DeleteComplexKeyDB(dbp,key,size);
#endif
}

/*****************************************************************************/

int NewDBCursor(CF_DB *dbp,CF_DBC **dbcp)

{
#ifdef TCDB
return TCDB_NewDBCursor(dbp,dbcp);
#elif defined QDB
return QDB_NewDBCursor(dbp,dbcp);
#else
return BDB_NewDBCursor(dbp,dbcp);
#endif
}

/*****************************************************************************/

int NextDB(CF_DB *dbp,CF_DBC *dbcp,char **key,int *ksize,void **value,int *vsize)
{
#ifdef TCDB
return TCDB_NextDB(dbp,dbcp,key,ksize,value,vsize);
#elif defined QDB
return QDB_NextDB(dbp,dbcp,key,ksize,value,vsize);
#else
return BDB_NextDB(dbp,dbcp,key,ksize,value,vsize);
#endif
}

/*****************************************************************************/

int DeleteDBCursor(CF_DB *dbp,CF_DBC *dbcp)
{
#ifdef TCDB
return TCDB_DeleteDBCursor(dbp,dbcp);
#elif defined QDB
return QDB_DeleteDBCursor(dbp,dbcp);
#else
return BDB_DeleteDBCursor(dbp,dbcp);
#endif
}

/*****************************************************************************/

int ReadDB(CF_DB *dbp, char *key, void *dest, int destSz)

{
return ReadComplexKeyDB(dbp,key,strlen(key)+1,dest,destSz);
}

/*****************************************************************************/

int WriteDB(CF_DB *dbp, char *key, void *src, int srcSz)

{
return WriteComplexKeyDB(dbp,key,strlen(key)+1,src,srcSz);
}


/*****************************************************************************/

int DeleteDB(CF_DB *dbp, char *key)

{
return DeleteComplexKeyDB(dbp,key,strlen(key)+1);
}

/*****************************************************************************/

void CloseAllDB(void)
/* Closes all open DB handles */
{
  CF_DB *dbp = NULL;
  int i = 0;

  Debug("CloseAllDB()\n");
  
  while(true)
    {
    if(!GetDBHandle(&dbp))
      {
      FatalError("CloseAllDB: Could not pop next DB handle");
      }
  
    if(dbp == NULL)
      {
      break;
      }

    if(!CloseDB(dbp))
      {
      CfOut(cf_error, "", "!! CloseAllDB: Could not close DB with this handle");
      }

      i++;
    }
  
  Debug("Closed %d open DB handles\n", i);
}

/*****************************************************************************/

static int SaveDBHandle(CF_DB *dbp)
{
  int i;
  
  if (!ThreadLock(cft_dbhandle))
    {
      return false;
    }

  // find first free slot
  i = 0;
  while(OPENDB[i] != NULL)
    {
      i++;
      if(i == MAX_OPENDB)
	{
	  ThreadUnlock(cft_dbhandle);
	  CfOut(cf_error,"","!! Too many open databases");
	  return false;
	}
    }
  
  OPENDB[i] = dbp;

  ThreadUnlock(cft_dbhandle);
  return true;

}

/*****************************************************************************/

static int RemoveDBHandle(CF_DB *dbp)
/* Remove a specific DB handle */
{
  int i;

  if (!ThreadLock(cft_dbhandle))
    {
      return false;
    }

  i = 0;

  while(OPENDB[i] != dbp)
    {
      i++;
      if(i == MAX_OPENDB)
	{
	  ThreadUnlock(cft_dbhandle);
	  CfOut(cf_error,"","!! Database handle was not found");
	  return false;
	}
    }

  // free slot
  OPENDB[i] = NULL;

  ThreadUnlock(cft_dbhandle);
  return true;
}

/*****************************************************************************/

static int GetDBHandle(CF_DB **dbp)
/* Return the first unused DB handle in the parameter - NULL if empty */
{
  int i;

  if (!ThreadLock(cft_dbhandle))
    {
      return false;
    }

  i = 0;
  
  while(OPENDB[i] == NULL)
    {
      i++;
      if(i == MAX_OPENDB)
	{
	  ThreadUnlock(cft_dbhandle);
	  *dbp = NULL;
	  return true;
	}
    }

  *dbp = OPENDB[i];

  ThreadUnlock(cft_dbhandle);
  return true;
}
