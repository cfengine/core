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
#ifndef CFENGINE_CONN_CACHE_H
#define CFENGINE_CONN_CACHE_H


#include <cfnet.h>                                       /* AgentConnection */


enum ConnCacheStatus
{
    CONNCACHE_STATUS_IDLE = 0,
    CONNCACHE_STATUS_BUSY,
    CONNCACHE_STATUS_OFFLINE
};


void ConnCache_Init(void);
void ConnCache_Destroy(void);

AgentConnection *ConnCache_FindIdleMarkBusy(const char *server,
                                            const char *port,
                                            ConnectionFlags flags);
void ConnCache_MarkNotBusy(AgentConnection *conn);
void ConnCache_Add(AgentConnection *conn, enum ConnCacheStatus status);
void ConnCache_IsBusy(AgentConnection *conn);


#endif
