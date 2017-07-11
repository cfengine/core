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


#ifndef CFENGINE_SERVER_COMMON_H
#define CFENGINE_SERVER_COMMON_H


#define CF_BUFEXT 128


#include <platform.h>

#include <cf3.defs.h>                              /* EvalContext */
#include <server.h>                                /* ServerConnectionState */


void RefuseAccess(ServerConnectionState *conn, char *errmesg);
int AllowedUser(char *user);
/* Checks whatever user name contains characters we are considering to be invalid */
bool IsUserNameValid(const char *username);
int MatchClasses(EvalContext *ctx, ServerConnectionState *conn);
void Terminate(ConnectionInfo *connection);
void DoExec(const ServerConnectionState *conn, const char *args);
void CfGetFile(ServerFileGetState *args);
void CfEncryptGetFile(ServerFileGetState *args);
int StatFile(ServerConnectionState *conn, char *sendbuffer, char *ofilename);
void ReplyServerContext(ServerConnectionState *conn, int encrypted, Item *classes);
int CfOpenDirectory(ServerConnectionState *conn, char *sendbuffer, char *oldDirname);
int CfSecOpenDirectory(ServerConnectionState *conn, char *sendbuffer, char *dirname);
void GetServerLiteral(EvalContext *ctx, ServerConnectionState *conn, char *sendbuffer, char *recvbuffer, int encrypted);
int GetServerQuery(ServerConnectionState *conn, char *recvbuffer, int encrypted);
bool CompareLocalHash(const char *filename, const char digest[EVP_MAX_MD_SIZE + 1],
                      char sendbuffer[CF_BUFSIZE]);
Item *ListPersistentClasses(void);

bool PathRemoveTrailingSlash(char *s, size_t s_len);
bool PathAppendTrailingSlash(char *s, size_t s_len);
size_t ReplaceSpecialVariables(char *buf, size_t buf_size,
                               const char *find1, const char *repl1,
                               const char *find2, const char *repl2,
                               const char *find3, const char *repl3);
size_t ShortcutsExpand(char *path, size_t path_size,
                       const StringMap *shortcuts,
                       const char *ipaddr, const char *hostname,
                       const char *key);
size_t PreprocessRequestPath(char *reqpath, size_t reqpath_size);
void SetConnIdentity(ServerConnectionState *conn, const char *username);


#endif  /* CFENGINE_SERVER_COMMON_H */
