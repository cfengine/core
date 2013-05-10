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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "cfnet.h"

#include "misc_lib.h"


/* Convert IP address in src (which can be struct sockaddr_storage (best
 * choice for any IP version), struct sockaddr, struct sockaddr_in or struct
 * sockaddr_in6) to string dst.
 * Better use getnameinfo(NI_NUMERICHOST) if available. */
const char *sockaddr_ntop(const void *src, char *dst, socklen_t size)
{
    int family = ((struct sockaddr *) src)->sa_family;
    void *addr;

    switch (family)
    {
    case AF_INET:
        addr = & ((struct sockaddr_in *) src)->sin_addr.s_addr;
        break;

#ifdef HAVE_GETADDRINFO
    case AF_INET6:
        addr = & ((struct sockaddr_in6 *) src)->sin6_addr.s6_addr;
        break;
#endif

#ifdef AF_LOCAL
    case AF_LOCAL:
        strlcpy(dst, "127.0.0.1", sizeof("127.0.0.1"));
        break;
#endif

    default:
        ProgrammingError("Address family was %d", family);
    }

    const char *ret = inet_ntop(family, addr, dst, size);
    return ret;
}
