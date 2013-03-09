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
AgentConnection *NewServerConnection(EvalContext *ctx, Attributes attr, Promise *pp, int *err);
void DisconnectServer(AgentConnection *conn);
int cf_remote_stat(EvalContext *ctx, char *file, struct stat *buf, char *stattype, Attributes attr, Promise *pp);
void DeleteClientCache(Attributes attr, Promise *pp);
int CompareHashNet(EvalContext *ctx, char *file1, char *file2, Attributes attr, Promise *pp);
int CopyRegularFileNet(EvalContext *ctx, char *source, char *new, off_t size, Attributes attr, Promise *pp);
int EncryptCopyRegularFileNet(EvalContext *ctx, char *source, char *new, off_t size, Attributes attr, Promise *pp);
int ServerConnect(EvalContext *ctx, AgentConnection *conn, char *host, Attributes attr, Promise *pp);

#endif
