/*
   Copyright (C) CFEngine AS

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

#ifndef CFENGINE_SERVER_H
#define CFENGINE_SERVER_H


#include <cf3.defs.h>
#include <cfnet.h>                                       /* AgentConnection */

#include <generic_agent.h>


//*******************************************************************
// TYPES
//*******************************************************************

typedef struct Auth_ Auth;

struct Auth_
{
    char *path;
    Item *accesslist;
    Item *maproot;              /* which hosts should have root read access */
    int encrypt;                /* which files HAVE to be transmitted securely */
    int literal;
    int classpattern;
    int variable;
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
    char *allowciphers;

    Auth *admit;
    Auth *admittop;

    Auth *deny;
    Auth *denytop;

    Auth *varadmit;
    Auth *varadmittop;

    Auth *vardeny;
    Auth *vardenytop;

    Auth *roles;
    Auth *rolestop;

    int logconns;
} ServerAccess;

/**
 * @member trust Whether we'll blindly trust any key from the host, depends on
 *               the "trustkeysfrom" option in body server control. Default
 *               false, check for setting it is in CheckStoreKey().
 */
struct ServerConnectionState_
{
    EvalContext *ctx;
    ConnectionInfo *conn_info;
    int synchronized;
    char hostname[CF_MAXVARSIZE];
    char username[CF_MAXVARSIZE];
#ifdef __MINGW32__
    char sid[CF_MAXSIDSIZE];    /* we avoid dynamically allocated buffers due to potential memory leaks */
#else
    uid_t uid;
#endif
    char ipaddr[CF_MAX_IP_LEN];
    char output[CF_BUFSIZE * 2];        /* Threadsafe output channel */
    /* TODO the following are useless with the new protocol */
    int id_verified;
    int rsa_auth;
    int maproot;
    unsigned char *session_key;
    char encryption_type;
};

typedef struct
{
    ServerConnectionState *connect;
    int encrypt;
    int buf_size;
    char *replybuff;
    char *replyfile;
} ServerFileGetState;


void ServerEntryPoint(EvalContext *ctx, char *ipaddr, ConnectionInfo *info);
void DeleteAuthList(Auth *ap);
void PurgeOldConnections(Item **list, time_t now);


AgentConnection *ExtractCallBackChannel(ServerConnectionState *conn);

//*******************************************************************
// STATE
//*******************************************************************

extern char CFRUNCOMMAND[];

#define CLOCK_DRIFT 3600

extern int ACTIVE_THREADS;

extern int CFD_MAXPROCESSES;
extern bool DENYBADCLOCKS;
extern int MAXTRIES;
extern bool LOGENCRYPT;
extern int COLLECT_INTERVAL;
extern bool SERVER_LISTEN;

extern ServerAccess SV;

extern char CFRUNCOMMAND[];

#endif
