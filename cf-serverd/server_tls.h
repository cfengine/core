/*
   Copyright 2019 Northern.tech AS

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


#ifndef CFENGINE_SERVER_TLS_H
#define CFENGINE_SERVER_TLS_H


#include <platform.h>

#include <cf3.defs.h>                              /* EvalContext */
#include <cfnet.h>                                 /* ConnectionInfo */
#include <server.h>                                /* ServerConnectionState */

typedef enum
{
    PROTOCOL_COMMAND_EXEC = 0,
    PROTOCOL_COMMAND_GET,
    PROTOCOL_COMMAND_OPENDIR,
    PROTOCOL_COMMAND_SYNCH,
    PROTOCOL_COMMAND_MD5,
    PROTOCOL_COMMAND_VERSION,
    PROTOCOL_COMMAND_VAR,
    PROTOCOL_COMMAND_CONTEXT,
    PROTOCOL_COMMAND_QUERY,
    PROTOCOL_COMMAND_CALL_ME_BACK,
    PROTOCOL_COMMAND_BAD
} ProtocolCommandNew;

static const char *const PROTOCOL_NEW[PROTOCOL_COMMAND_BAD + 1] =
{
    "EXEC",
    "GET",
    "OPENDIR",
    "SYNCH",
    "MD5",
    "VERSION",
    "VAR",
    "CONTEXT",
    "QUERY",
    "SCALLBACK",
    NULL
};

bool ServerTLSInitialize(RSA *priv_key, RSA *pub_key, SSL_CTX **ctx);
void ServerTLSDeInitialize(RSA **priv_key, RSA **pub_key, SSL_CTX **ctx);
bool ServerTLSPeek(ConnectionInfo *conn_info);
bool BasicServerTLSSessionEstablish(ServerConnectionState *conn, SSL_CTX *ssl_ctx);
bool ServerTLSSessionEstablish(ServerConnectionState *conn, SSL_CTX *ssl_ctx);
bool BusyWithNewProtocol(EvalContext *ctx, ServerConnectionState *conn);
bool ServerSendWelcome(const ServerConnectionState *conn);
bool ServerIdentificationDialog(ConnectionInfo *conn_info,
                                char *username, size_t username_size);
ProtocolCommandNew GetCommandNew(char *str);

#endif  /* CFENGINE_SERVER_TLS_H */
