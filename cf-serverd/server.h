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

#ifndef CFENGINE_SERVER_H
#define CFENGINE_SERVER_H

#include "cf3.defs.h"

#include "generic_agent.h"

//*******************************************************************
// TYPES
//*******************************************************************

typedef struct
{
   Item *nonattackerlist;
   Item *attackerlist;
   Item *connectionlist;
   Item *allowuserlist;
   Item *multiconnlist;
   Item *trustkeylist;
   Item *skipverify;
   int logconns;
} ServerAccess;

typedef struct ServerConnectionState
{
    int id_verified;
    int rsa_auth;
    int synchronized;
    int maproot;
    int trust;
    int sd_reply;
    unsigned char *session_key;
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    char hostname[CF_MAXVARSIZE];
    char username[CF_MAXVARSIZE];
#ifdef __MINGW32__
    char sid[CF_MAXSIDSIZE];    /* we avoid dynamically allocated buffers due to potential memory leaks */
#else
    uid_t uid;
#endif
    char encryption_type;
    char ipaddr[CF_MAX_IP_LEN];
    char output[CF_BUFSIZE * 2];        /* Threadsafe output channel */
} ServerConnectionState;

typedef struct
{
    ServerConnectionState *connect;
    int encrypt;
    int buf_size;
    char *replybuff;
    char *replyfile;
} ServerFileGetState;


#ifdef HAVE_NOVA

int Nova_ReturnQueryData(ServerConnectionState *conn, char *menu);

#endif

void KeepPromises(Policy *policy, GenericAgentConfig *config, const ReportContext *report_context);

void ServerEntryPoint(int sd_reply, char *ipaddr, ServerAccess sv);
void TryCollectCall(void);
int SetServerListenState(size_t queue_size);
void DeleteAuthList(Auth *ap);
void PurgeOldConnections(Item **list, time_t now);


AgentConnection *ExtractCallBackChannel(ServerConnectionState *conn);


//*******************************************************************
// STATE
//*******************************************************************

extern char CFRUNCOMMAND[];

extern int CLOCK_DRIFT;
extern int ACTIVE_THREADS;

extern int CFD_MAXPROCESSES;
extern int CFD_INTERVAL;
extern int DENYBADCLOCKS;
extern int MAXTRIES;
extern int LOGCONNS;
extern int LOGENCRYPT;
extern int COLLECT_INTERVAL;
extern bool SERVER_LISTEN;

extern Auth *ROLES;
extern Auth *ROLESTOP;

extern ServerAccess SV;

extern Auth *VADMIT;
extern Auth *VADMITTOP;
extern Auth *VDENY;
extern Auth *VDENYTOP;

extern Auth *VARADMIT;
extern Auth *VARADMITTOP;
extern Auth *VARDENY;
extern Auth *VARDENYTOP;

extern char CFRUNCOMMAND[];

#endif
