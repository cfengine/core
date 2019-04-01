/*
   Copyright 2018 Northern.tech AS

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


#ifndef CFENGINE_CFNET_H
#define CFENGINE_CFNET_H


#include <platform.h>
#include <definitions.h>                        /* CF_BUFSIZE, CF_SMALLBUF */


/* Only set with DetermineCfenginePort() and from cf-serverd */
extern char CFENGINE_PORT_STR[16];                     /* GLOBAL_P GLOBAL_E */
extern int CFENGINE_PORT;                              /* GLOBAL_P GLOBAL_E */


#define CF_MAX_IP_LEN 64                    /* max IPv4/IPv6 address length */
#define CF_MAX_PORT_LEN 6
#define CF_MAX_HOST_LEN 256
// Since both have 1 extra for null byte, we don't need to add 1 for ':'
#define CF_MAX_SERVER_LEN (CF_MAX_HOST_LEN + CF_MAX_PORT_LEN)

#define CF_DONE 't'
#define CF_MORE 'm'
#define SOCKET_INVALID -1
#define MAXIP4CHARLEN 16
#define CF_RSA_PROTO_OFFSET 24
#define CF_PROTO_OFFSET 16
#define CF_INBAND_OFFSET 8


/**
  Available protocol versions. When connection is initialised ProtocolVersion
  is 0, i.e. undefined. It is after the call to ServerConnection() that
  protocol version is decided, according to body copy_from and body common
  control. All protocol numbers are numbered incrementally starting from 1.
 */
typedef enum
{
    CF_PROTOCOL_UNDEFINED = 0,
    CF_PROTOCOL_CLASSIC = 1,
    /* --- Greater versions use TLS as secure communications layer --- */
    CF_PROTOCOL_TLS = 2
} ProtocolVersion;

/* We use CF_PROTOCOL_LATEST as the default for new connections. */
#define CF_PROTOCOL_LATEST CF_PROTOCOL_TLS

static const char * const PROTOCOL_VERSION_STRING[CF_PROTOCOL_LATEST + 1] = {
    "undefined",
    "classic",
    "latest"
};

typedef struct
{
    ProtocolVersion protocol_version : 3;
    bool            cache_connection : 1;
    bool            force_ipv4       : 1;
    bool            trust_server     : 1;
    bool            off_the_record   : 1;
} ConnectionFlags;

static inline bool ConnectionFlagsEqual(const ConnectionFlags *f1,
                                        const ConnectionFlags *f2)
{
    if (f1->protocol_version == f2->protocol_version &&
        f1->cache_connection == f2->cache_connection &&
        f1->force_ipv4 == f2->force_ipv4 &&
        f1->trust_server == f2->trust_server &&
        f1->off_the_record == f2->off_the_record)
    {
        return true;
    }
    else
    {
        return false;
    }
}


#include "connection_info.h"                       /* needs ProtocolVersion */


/*
 * TLS support
 */
#define DEFAULT_TLS_TIMEOUT_SECONDS     5
#define DEFAULT_TLS_TIMEOUT_USECONDS    0
#define SET_DEFAULT_TLS_TIMEOUT(x) \
    x.tv_sec = DEFAULT_TLS_TIMEOUT_SECONDS; \
    x.tv_usec = DEFAULT_TLS_TIMEOUT_USECONDS
#define DEFAULT_TLS_TRIES 5

struct Stat_;              /* defined in stat_cache.h, typedef'ed to "Stat" */

typedef struct
{
    ConnectionInfo *conn_info;
    int authenticated;
    char username[CF_SMALLBUF];
    /* Unused for now... */
    /* char localip[CF_MAX_IP_LEN]; */
    char remoteip[CF_MAX_IP_LEN];
    unsigned char *session_key;
    char encryption_type;
    short error;
    struct Stat_ *cache;                          /* cache for remote STATs */

    /* The following consistutes the ID of a server host, mostly taken from
     * the copy_from connection attributes. */
    ConnectionFlags flags;
    char *this_server;
    char *this_port;
} AgentConnection;



/* misc.c */

void EnforceBwLimit(int tosend);
int cf_closesocket(int sd);


/* client_protocol.c */
void SetSkipIdentify(bool enabled);

/* net.c */
void SetBindInterface(const char *ip);

#endif
