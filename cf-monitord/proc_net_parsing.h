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

#ifndef _CFE_PROC_NET_PARSING_H_
#define _CFE_PROC_NET_PARSING_H_

#include <arpa/inet.h>

/* Taken from https://git.kernel.org/pub/scm/network/iproute2/iproute2.git/tree/misc/ss.c#n221 */
typedef enum {
    SOCK_STATE_UNKNOWN = 0,
    SOCK_STATE_ESTAB,
    SOCK_STATE_SYN_SENT,
    SOCK_STATE_SYN_RECV,
    SOCK_STATE_FIN_WAIT1,
    SOCK_STATE_FIN_WAIT2,
    SOCK_STATE_TIME_WAIT,
    SOCK_STATE_UNCONN,
    SOCK_STATE_CLOSE_WAIT,
    SOCK_STATE_LAST_ACK,
    SOCK_STATE_LISTEN,
    SOCK_STATE_CLOSING,
} SocketState;

#ifdef __linux__
bool ParseIPv4SocketInfo(const char *line,
                         char local_addr[INET_ADDRSTRLEN], uint32_t *local_port,
                         char remote_addr[INET_ADDRSTRLEN], uint32_t *remote_port,
                         SocketState *state);

bool ParseIPv6SocketInfo(const char *line,
                         char local_addr[INET6_ADDRSTRLEN], uint32_t *local_port,
                         char remote_addr[INET6_ADDRSTRLEN], uint32_t *remote_port,
                         SocketState *state);
#else  /* __linux__ */
bool ParseIPv4SocketInfo(const char *line,
                         char local_addr[INET_ADDRSTRLEN], uint32_t *local_port,
                         char remote_addr[INET_ADDRSTRLEN], uint32_t *remote_port,
                         SocketState *state) __attribute__((error ("Only supported on Linux")));
bool ParseIPv6SocketInfo(const char *line,
                         char local_addr[INET6_ADDRSTRLEN], uint32_t *local_port,
                         char remote_addr[INET6_ADDRSTRLEN], uint32_t *remote_port,
                         SocketState *state) __attribute__((error ("Only supported on Linux")));
#endif  /* __linux__ */

#endif  /* _CFE_PROC_NET_PARSING_H_ */
