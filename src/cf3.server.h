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

/*******************************************************************/
/*                                                                 */
/*  HEADER for cfServerd - Not generically includable!             */
/*                                                                 */
/*******************************************************************/

#ifndef CFENGINE_CF3_SERVER_H
#define CFENGINE_CF3_SERVER_H

#define connection 1
#define QUEUESIZE 50
#define CF_BUFEXT 128
#define CF_NOSIZE -1

/*******************************************************************/

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

/*******************************************************************/

typedef struct
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
#ifdef MINGW
    char sid[CF_MAXSIDSIZE];    /* we avoid dynamically allocated buffers due to potential memory leaks */
#else
    uid_t uid;
#endif
    char encryption_type;
    char ipaddr[CF_MAX_IP_LEN];
    char output[CF_BUFSIZE * 2];        /* Threadsafe output channel */
} ServerConnectionState;

/*******************************************************************/

typedef struct
{
    ServerConnectionState *connect;
    int encrypt;
    int buf_size;
    char *replybuff;
    char *replyfile;
} ServerFileGetState;

/**********************************************************************/

extern ServerAccess SV;

extern Auth *VADMIT;
extern Auth *VADMITTOP;
extern Auth *VDENY;
extern Auth *VDENYTOP;
extern Auth *VARADMIT;
extern Auth *VARADMITTOP;
extern Auth *VARDENY;
extern Auth *VARDENYTOP;

extern int LOGENCYPRT;

/**********************************************************************/

extern char CFRUNCOMMAND[];

#ifdef HAVE_NOVA
int Nova_ReturnQueryData(ServerConnectionState *conn, char *menu);
int Nova_AcceptCollectCall(ServerConnectionState *conn);
#endif

#ifdef HAVE_CONSTELLATION
int Constellation_ReturnRelayQueryData(ServerConnectionState *conn, char *query, char *sendbuffer);
void Constellation_RunQueries(Item *queries, Item **results_p);
#endif

void KeepPromises(Policy *policy);

void ServerEntryPoint(int sd_reply, char *ipaddr, ServerAccess sv);

void PurgeOldConnections(Item **list, time_t now);
void SpawnConnection(int sd_reply, char *ipaddr);
void *HandleConnection(ServerConnectionState *conn);
int BusyWithConnection(ServerConnectionState *conn);
int MatchClasses(ServerConnectionState *conn);
void DoExec(ServerConnectionState *conn, char *sendbuffer, char *args);
int GetCommand(char *str);
int VerifyConnection(ServerConnectionState *conn, char *buf);
void RefuseAccess(ServerConnectionState *conn, char *sendbuffer, int size, char *errormsg);
int AccessControl(const char *oldFilename, ServerConnectionState *conn, int encrypt, Auth *admit, Auth *deny);
int LiteralAccessControl(char *filename, ServerConnectionState *conn, int encrypt, Auth *admit, Auth *deny);
Item *ContextAccessControl(char *in, ServerConnectionState *conn, int encrypt, Auth *vadmit, Auth *vdeny);
void ReplyServerContext(ServerConnectionState *conn, char *sendbuffer, char *recvbuffer, int encrypted,
                               Item *classes);
int CheckStoreKey(ServerConnectionState *conn, RSA *key);
int StatFile(ServerConnectionState *conn, char *sendbuffer, char *filename);
void CfGetFile(ServerFileGetState *args);
void CfEncryptGetFile(ServerFileGetState *args);
void CompareLocalHash(ServerConnectionState *conn, char *sendbuffer, char *recvbuffer);
void GetServerLiteral(ServerConnectionState *conn, char *sendbuffer, char *recvbuffer, int encrypted);
int ReceiveCollectCall(ServerConnectionState *conn, char *sendbuffer);
void TryCollectCall(void);
int GetServerQuery(ServerConnectionState *conn, char *sendbuffer, char *recvbuffer);
int CfOpenDirectory(ServerConnectionState *conn, char *sendbuffer, char *oldDirname);
int CfSecOpenDirectory(ServerConnectionState *conn, char *sendbuffer, char *dirname);
void Terminate(int sd);
void DeleteAuthList(Auth *ap);
int AllowedUser(char *user);
int AuthorizeRoles(ServerConnectionState *conn, char *args);
int TransferRights(char *filename, int sd, ServerFileGetState *args, char *sendbuffer, struct stat *sb);
void AbortTransfer(int sd, char *sendbuffer, char *filename);
void FailedTransfer(int sd, char *sendbuffer, char *filename);
void ReplyNothing(ServerConnectionState *conn);
ServerConnectionState *NewConn(int sd);
void DeleteConn(ServerConnectionState *conn);
int cfscanf(char *in, int len1, int len2, char *out1, char *out2, char *out3);
int AuthenticationDialogue(ServerConnectionState *conn, char *buffer, int buffersize);
int SafeOpen(char *filename);
int OptionFound(char *args, char *pos, char *word);
AgentConnection *ExtractCallBackChannel(ServerConnectionState *conn);
#endif



