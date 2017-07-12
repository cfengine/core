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

/* Low Level networking routines. */

#ifndef CFENGINE_NET_H
#define CFENGINE_NET_H


#include <cfnet.h>


extern uint32_t bwlimit_kbytes;


int SendTransaction(ConnectionInfo *conn_info, const char *buffer, int len, char status);
int ReceiveTransaction(ConnectionInfo *conn_info, char *buffer, int *more);

int SetReceiveTimeout(int fd, unsigned long ms);

int SocketConnect(const char *host, const char *port,
                  unsigned int connect_timeout, bool force_ipv4,
                  char *txtaddr, size_t txtaddr_size);

/**
 * @NOTE DO NOT USE THIS FUNCTION. The only reason it is non-static is because
 *       of a separate implementation for windows in Enterprise.
 */
bool TryConnect(int sd, unsigned long timeout_ms,
                const struct sockaddr *sa, socklen_t sa_len);


#endif
