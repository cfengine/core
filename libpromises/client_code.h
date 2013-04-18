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

#ifndef CFENGINE_CLIENT_CODE_H
#define CFENGINE_CLIENT_CODE_H

#include "cf3.defs.h"

void DetermineCfenginePort(void);
/**
  @param err Set to 0 on success, -1 no server responce, -2 authentication failure.
  */
AgentConnection *NewServerConnection(FileCopy fc, bool background, int *err);
void DisconnectServer(AgentConnection *conn);
int cf_remote_stat(char *file, struct stat *buf, char *stattype, bool encrypt, AgentConnection *conn);
int CompareHashNet(char *file1, char *file2, bool encrypt, AgentConnection *conn);
int CopyRegularFileNet(char *source, char *new, off_t size, AgentConnection *conn);
int EncryptCopyRegularFileNet(char *source, char *new, off_t size, AgentConnection *conn);
int ServerConnect(AgentConnection *conn, const char *host, FileCopy fc);

Item *RemoteDirList(const char *dirname, bool encrypt, AgentConnection *conn);

const Stat *ClientCacheLookup(AgentConnection *conn, const char *server_name, const char *file_name);

#endif
