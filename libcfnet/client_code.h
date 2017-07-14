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

#ifndef CFENGINE_CLIENT_CODE_H
#define CFENGINE_CLIENT_CODE_H


#include <platform.h>
#include <item_lib.h>

#include <communication.h>


bool cfnet_init(const char *tls_min_version, const char *ciphers);
void cfnet_shut(void);
bool cfnet_IsInitialized(void);
void DetermineCfenginePort(void);
/**
  @param err Set to 0 on success, -1 no server response, -2 authentication failure.
  */
AgentConnection *ServerConnection(const char *server, const char *port,
                                  unsigned int connect_timeout,
                                  ConnectionFlags flags, int *err);
void DisconnectServer(AgentConnection *conn);

int CompareHashNet(const char *file1, const char *file2, bool encrypt, AgentConnection *conn);
int CopyRegularFileNet(const char *source, const char *dest, off_t size,
                       bool encrypt, AgentConnection *conn);
Item *RemoteDirList(const char *dirname, bool encrypt, AgentConnection *conn);

int TLSConnectCallCollect(ConnectionInfo *conn_info, const char *username);


#endif
