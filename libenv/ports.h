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

#ifndef CFENGINE_UNIX_PORT_H
#define CFENGINE_UNIX_PORT_H

#include <cf3.defs.h>
#include <buffer.h>

typedef enum { cfn_new, cfn_old } cf_netstat_type;
typedef enum { cfn_udp4, cfn_udp6, cfn_tcp4, cfn_tcp6} cf_packet_type;
typedef enum { cfn_listen, cfn_not_listen} cf_port_state; // TODO: add more?

typedef void (*PortProcessorFn) (const cf_netstat_type netstat_type, const cf_packet_type packet_type, const cf_port_state port_state,
                                 const char *netstat_line,
                                 const char *local_addr, const char *local_port,
                                 const char *remote_addr, const char *remote_port,
                                 void *callback_context);

Buffer* PortsGetNetstatCommand(const PlatformContext platform);
void PortsFindListening(const PlatformContext platform, PortProcessorFn port_processor, void *callback_context);

#endif
