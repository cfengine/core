/*

   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_SQL_H
#define CFENGINE_SQL_H

#include <cf3.defs.h>

typedef struct
{
    int connected;
    int result;
    int row;
    unsigned int maxcolumns;
    unsigned int maxrows;
    int column;
    char **rowdata;
    char *blank;
    DatabaseType type;
    void *data;                 /* Generic pointer to RDBMS-specific data */
} CfdbConn;

int CfConnectDB(CfdbConn *cfdb, DatabaseType dbtype, char *remotehost, char *dbuser, char *passwd, char *db);
void CfCloseDB(CfdbConn *cfdb);
void CfVoidQueryDB(CfdbConn *cfdb, char *query);
void CfNewQueryDB(CfdbConn *cfdb, char *query);
char **CfFetchRow(CfdbConn *cfdb);
char *CfFetchColumn(CfdbConn *cfdb, int col);
void CfDeleteQuery(CfdbConn *cfdb);

#endif
