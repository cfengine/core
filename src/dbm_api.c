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

int OpenDB(char *filename, CF_DB **dbp)
{
#ifdef TCDB
  return TCDB_OpenDB(filename, dbp);
#elif defined QDB  // FIXME
  return QDB_OpenDB(filename, dbp);
#else
  return BDB_OpenDB(filename, dbp);
#endif
}

/*****************************************************************************/

int CloseDB(CF_DB *dbp)
{
#ifdef TCDB
  return TCDB_CloseDB(dbp);;
#elif defined QDB  // FIXME
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
#elif defined QDB  // FIXME
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
#elif defined QDB  // FIXME
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
  return RevealDB(dbp,key,result,rsize);
#elif defined QDB  // FIXME
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
#elif defined QDB  // FIXME
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
#elif defined QDB  // FIXME
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
#elif defined QDB  // FIXME
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
#elif defined QDB  // FIXME
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
#elif defined QDB  // FIXME
  return QDB_DeleteDBCursor(dbp,dbcp);
#else
  return BDB_DeleteDBCursor(dbp,dbcp);
#endif
}

/*****************************************************************************/

int ReadDB(CF_DB *dbp, char *key, void *dest, int destSz)
{
  return ReadComplexKeyDB(dbp, key, strlen(key), dest, destSz);
}

/*****************************************************************************/

int WriteDB(CF_DB *dbp, char *key, void *src, int srcSz)
{
  return WriteComplexKeyDB(dbp, key, strlen(key), src, srcSz);
}


/*****************************************************************************/

int DeleteDB(CF_DB *dbp, char *key)
/**
 * Delete a record (key,value pair)
 */
{
  return DeleteComplexKeyDB(dbp, key, strlen(key));
}

