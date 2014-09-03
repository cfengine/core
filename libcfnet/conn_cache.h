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

AgentConnection *ConnCache_FindIdle(const char *server, const char *port,
                                    ConnectionFlags flags);
void ConnCache_MarkNotBusy(AgentConnection *conn);
void ConnCache_Add(AgentConnection *conn, enum ConnCacheStatus status);
void ConnCache_IsBusy(AgentConnection *conn);


#endif
