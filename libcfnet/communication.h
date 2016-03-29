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

#ifndef CFENGINE_COMMUNICATION_H
#define CFENGINE_COMMUNICATION_H

#include <cfnet.h>

/**
  @brief Allocates a new AgentConnection (stores a connection from Agent
         to Server).
  */
AgentConnection *NewAgentConn(const char *server, const char *port,
                              ConnectionFlags flags);
/**
  @brief Destroys an AgentConnection.
  @param ap AgentConnection structure.
  */
void DeleteAgentConn(AgentConnection *ap);
int IsIPV6Address(char *name);
int IsIPV4Address(char *name);
int Hostname2IPString(char *dst, const char *hostname, size_t dst_size);
int IPString2Hostname(char *dst, const char *ipaddr, size_t dst_size);
unsigned short SocketFamily(int sd);

#endif
