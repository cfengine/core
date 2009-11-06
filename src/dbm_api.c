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
#ifdef QDB  // FIXME
return QDB_OpenDB(filename, dbp);
#else
return BDB_OpenDB(filename, dbp);
#endif
}

/*****************************************************************************/

int CloseDB(CF_DB *dbp)
{
#ifdef QDB  // FIXME
return QDB_CloseDB(dbp);
#else
return BDB_CloseDB(dbp);
#endif
}

/*****************************************************************************/

int ValueSizeDB(CF_DB *dbp, char *key)
/* Returns size of value corresponding to key, or -1 on not found or error */
{
#ifdef QDB  // FIXME
return QDB_ValueSizeDB(dbp, key);
#else
return BDB_ValueSizeDB(dbp, key);
#endif
}

/*****************************************************************************/

int ReadDB(CF_DB *dbp, char *key, void *dest, int destSz)
{
#ifdef QDB  // FIXME
return QDB_ReadDB(dbp, key, dest, destSz);
#else
return BDB_ReadDB(dbp, key, dest, destSz);
#endif
}

/*****************************************************************************/

int WriteDB(CF_DB *dbp, char *key, void *src, int srcSz)
{
#ifdef QDB  // FIXME
return QDB_WriteDB(dbp, key, src, srcSz);
#else
return BDB_WriteDB(dbp, key, src, srcSz);
#endif
}

/*****************************************************************************/

int DeleteDB(CF_DB *dbp, char *key)
/**
 * Delete a record (key,value pair)
 */
{
#ifdef QDB  // FIXME
return QDB_DeleteDB(dbp, key);
#else
return BDB_DeleteDB(dbp, key);
#endif
}
