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

#ifndef CFENGINE_DBM_SQLITE3_H
#define CFENGINE_DBM_SQLITE3_H

#include "cf3.defs.h"
#include "cf3.extern.h"

int SQLite3_OpenDB(char *filename,sqlite3 **dbp);
int SQLite3_CloseDB(sqlite3 *dbp);
int SQLite3_ValueSizeDB(sqlite3 *dbp, char *key);
int SQLite3_ReadComplexKeyDB(sqlite3 *dbp,char *name,int keysize,void *ptr,int size);
int SQLite3_RevealDB(sqlite3 *dbp,char *name,void **result,int *rsize);
int SQLite3_WriteComplexKeyDB(sqlite3 *dbp,char *name,int keysize, const void *ptr,int size);
int SQLite3_DeleteComplexKeyDB(sqlite3 *dbp,char *name,int size);
int SQLite3_NewDBCursor(sqlite3 *dbp,sqlite3_stmt **dbcp);
int SQLite3_NextDB(sqlite3 *dbp,sqlite3_stmt *dbcp,char **key,int *ksize,void **value,int *vsize);
int SQLite3_DeleteDBCursor(sqlite3 *dbp,sqlite3_stmt *dbcp);
void SQLite3_OpenDBTransaction(sqlite3 *dbp);
void SQLite3_CommitDBTransaction(sqlite3 *dbp);

#endif
