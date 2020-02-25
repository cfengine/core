/*
  Copyright 2020 Northern.tech AS

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

#ifndef CFENGINE_CLIENT_CODE_H
#define CFENGINE_CLIENT_CODE_H


#include <platform.h>
#include <sequence.h>
#include <openssl/rsa.h>

#include <communication.h>


bool cfnet_init(const char *tls_min_version, const char *ciphers,
                time_t start_time, const char *host);
void cfnet_shut(void);
bool cfnet_IsInitialized(void);
void DetermineCfenginePort(void);
bool SetCfenginePort(const char *port_str);

/**
  @param err Set to 0 on success, -1 no server response, -2 authentication failure.
  */
AgentConnection *ServerConnection(const char *server, const char *port,
                                  unsigned int connect_timeout,
                                  ConnectionFlags flags,
                                  RSA *privkey, RSA *pubkey,
                                  RecordSeen record_seen,
                                  GetHostRSAKeyByIP get_host_key_by_ip,
                                  int *err);
void DisconnectServer(AgentConnection *conn);

bool CompareHashNet(const char *file1, const char *file2, bool encrypt, AgentConnection *conn);
bool CopyRegularFileNet(const char *source, const char *dest, off_t size,
                        bool encrypt, AgentConnection *conn);
Seq *RemoteDirList(const char *dirname, bool encrypt, AgentConnection *conn);

int TLSConnectCallCollect(ConnectionInfo *conn_info, const char *username);


#endif
