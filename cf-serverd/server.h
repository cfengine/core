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

#ifndef CFENGINE_SERVER_H
#define CFENGINE_SERVER_H


#include <cf3.defs.h>
#include <cfnet.h>                                       /* AgentConnection */

#include <generic_agent.h>


//*******************************************************************
// TYPES
//*******************************************************************

typedef struct Auth_ Auth;

/* Access rights for a path, literal, context (classpattern), variable */
/* LEGACY CODE the new struct is paths_acl etc. */
struct Auth_
{
    char *path;
    int literal;
    int classpattern;
    int variable;

    Item *accesslist;        /* which hosts -- IP or hostnames */
    Item *maproot;           /* which hosts should have root read access */
    int encrypt;             /* which files HAVE to be transmitted securely */

    Auth *next;
};

typedef struct
{
    Item *connectionlist;             /* List of currently open connections */

    /* body server control options */
    Item *nonattackerlist;                            /* "allowconnects" */
    Item *attackerlist;                               /* "denyconnects" */
    Item *allowuserlist;                              /* "allowusers" */
    Item *multiconnlist;                              /* "allowallconnects" */
    Item *trustkeylist;                               /* "trustkeysfrom" */
    Item *allowlegacyconnects;
    char *allowciphers;
    char *allowtlsversion;

    /* ACL for resource_type "path". */
    Auth *admit;
    Auth *admittail;

    Auth *deny;
    Auth *denytail;

    /* ACL for resource_types "literal", "query", "context", "variable". */
    Auth *varadmit;
    Auth *varadmittail;

    Auth *vardeny;
    Auth *vardenytail;

    Auth *roles;
    Auth *rolestail;

    int logconns;

    /* bundle server access_rules: shortcut for ACL entries, which expands to
     * the ACL entry when seen in client requests. */
    StringMap *path_shortcuts;

} ServerAccess;

/* TODO rename to IncomingConnection */
struct ServerConnectionState_
{
    ConnectionInfo *conn_info;
    /* TODO sockaddr_storage, even though we can keep the text as cache. */
    char ipaddr[CF_MAX_IP_LEN];
    char revdns[MAXHOSTNAMELEN];          /* only populated in new protocol */

#ifdef __MINGW32__
    /* We avoid dynamically allocated buffers due to potential memory leaks,
     * but this is still too big at 2K! */
    char sid[CF_MAXSIDSIZE];
#endif
    uid_t uid;

    /* TODO move username, hostname etc to a new struct identity. */
    char username[CF_MAXVARSIZE];

    /* TODO the following are useless with the new protocol */
    char hostname[CF_MAXVARSIZE]; /* hostname is copied from client-supplied CAUTH command */
    int user_data_set;
    int rsa_auth;
    int maproot;
    unsigned char *session_key;
    char encryption_type;

    /* TODO pass it through function arguments, EvalContext has nothing to do
     * with connection-specific data. */
    EvalContext *ctx;
};

typedef struct
{
    ServerConnectionState *conn;
    int encrypt;
    int buf_size;
    char *replybuff;
    char *replyfile;
} ServerFileGetState;


/* Used in cf-serverd-functions.c. */
void ServerEntryPoint(EvalContext *ctx, const char *ipaddr, ConnectionInfo *info);


AgentConnection *ExtractCallBackChannel(ServerConnectionState *conn);

//*******************************************************************
// STATE
//*******************************************************************

#define CLOCK_DRIFT 3600


extern int ACTIVE_THREADS;
extern int CFD_MAXPROCESSES;
extern bool DENYBADCLOCKS;
extern int MAXTRIES;
extern bool LOGENCRYPT;
extern int COLLECT_INTERVAL;
extern bool SERVER_LISTEN;
extern ServerAccess SV;
extern char CFRUNCOMMAND[CF_MAXVARSIZE];
extern bool NEED_REVERSE_LOOKUP;

#endif
