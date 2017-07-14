/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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


#include <platform.h>
#include <conn_cache.h>

#include <cfnet.h>                                     /* AgentConnection */
#include <client_code.h>                               /* DisconnectServer */
#include <sequence.h>                                  /* Seq */
#include <mutex.h>                                     /* ThreadLock */
#include <communication.h>                             /* Hostname2IPString */
#include <misc_lib.h>                                  /* CF_ASSERT */


/**
   Global cache for connections to servers, currently only used in cf-agent.

   @note THREAD-SAFETY: yes this connection cache *is* thread-safe, but is
         extremely slow if used intensely from multiple threads. It needs to
         be redesigned from scratch for that, not a priority for
         single-threaded cf-agent!
*/


typedef struct
{
    AgentConnection *conn;
    enum ConnCacheStatus status; /* TODO unify with conn->conn_info->status */
} ConnCache_entry;


static pthread_mutex_t cft_conncache = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

static Seq *conn_cache = NULL;


void ConnCache_Init()
{
    ThreadLock(&cft_conncache);

    assert(conn_cache == NULL);
    conn_cache = SeqNew(100, free);

    ThreadUnlock(&cft_conncache);
}

void ConnCache_Destroy()
{
    ThreadLock(&cft_conncache);

    for (size_t i = 0; i < SeqLength(conn_cache); i++)
    {
        ConnCache_entry *svp = SeqAt(conn_cache, i);

        CF_ASSERT(svp != NULL,
                  "Destroy: NULL ConnCache_entry!");
        CF_ASSERT(svp->conn != NULL,
                  "Destroy: NULL connection in ConnCache_entry!");

        DisconnectServer(svp->conn);
    }

    SeqDestroy(conn_cache);
    conn_cache = NULL;

    ThreadUnlock(&cft_conncache);
}

AgentConnection *ConnCache_FindIdleMarkBusy(const char *server,
                                            const char *port,
                                            ConnectionFlags flags)
{
    ThreadLock(&cft_conncache);

    AgentConnection *ret_conn = NULL;
    for (size_t i = 0; i < SeqLength(conn_cache); i++)
    {
        ConnCache_entry *svp = SeqAt(conn_cache, i);

        CF_ASSERT(svp != NULL,
                  "FindIdle: NULL ConnCache_entry!");
        CF_ASSERT(svp->conn != NULL,
                  "FindIdle: NULL connection in ConnCache_entry!");


        if (strcmp(server,  svp->conn->this_server) == 0 &&
            ConnectionFlagsEqual(&flags, &svp->conn->flags) &&
            (port == svp->conn->this_port
             ||
             (port != NULL && svp->conn->this_port != NULL &&
              strcmp(port,    svp->conn->this_port) == 0)))
        {
            if (svp->status == CONNCACHE_STATUS_BUSY)
            {
                Log(LOG_LEVEL_DEBUG, "FindIdle:"
                    " connection to '%s' seems to be busy.",
                    server);
            }
            else if (svp->status == CONNCACHE_STATUS_OFFLINE)
            {
                Log(LOG_LEVEL_DEBUG, "FindIdle:"
                    " connection to '%s' is marked as offline.",
                    server);
            }
            else if (svp->conn->conn_info->sd >= 0)
            {
                assert(svp->status == CONNCACHE_STATUS_IDLE);

                Log(LOG_LEVEL_VERBOSE, "FindIdle:"
                    " found connection to '%s' already open and ready.",
                    server);

                svp->status = CONNCACHE_STATUS_BUSY;
                ret_conn = svp->conn;
                break;
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "FindIdle:"
                    " connection to '%s' has invalid socket descriptor %d!",
                    server, svp->conn->conn_info->sd);
            }
        }
    }

    ThreadUnlock(&cft_conncache);

    if (ret_conn == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "FindIdle:"
            " no existing connection to '%s' is established.", server);
    }

    return ret_conn;
}

void ConnCache_MarkNotBusy(AgentConnection *conn)
{
    Log(LOG_LEVEL_DEBUG, "Searching for specific busy connection to: %s",
        conn->this_server);

    ThreadLock(&cft_conncache);

    bool found = false;
    for (size_t i = 0; i < SeqLength(conn_cache); i++)
    {
        ConnCache_entry *svp = SeqAt(conn_cache, i);

        CF_ASSERT(svp != NULL,
                  "MarkNotBusy: NULL ConnCache_entry!");
        CF_ASSERT(svp->conn != NULL,
                  "MarkNotBusy: NULL connection in ConnCache_entry!");

        if (svp->conn == conn)
        {
            /* There might be many connections to the same server, some busy
             * some not. But here we're searching by the address of the
             * AgentConnection object. There can be only one. */
            CF_ASSERT(svp->status == CONNCACHE_STATUS_BUSY,
                      "MarkNotBusy: status is not busy, it is %d!",
                      svp->status);

            svp->status = CONNCACHE_STATUS_IDLE;
            found = true;
            break;
        }
    }

    ThreadUnlock(&cft_conncache);

    if (!found)
    {
        ProgrammingError("MarkNotBusy: No busy connection found!");
    }

    Log(LOG_LEVEL_DEBUG, "Busy connection just became free");
}

/* First time we open a connection, so store it. */
void ConnCache_Add(AgentConnection *conn, enum ConnCacheStatus status)
{
    ConnCache_entry *svp = xmalloc(sizeof(*svp));
    svp->status = status;
    svp->conn = conn;

    ThreadLock(&cft_conncache);
    SeqAppend(conn_cache, svp);
    ThreadUnlock(&cft_conncache);
}
