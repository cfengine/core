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

#include <cfnet.h>

#include <misc_lib.h>
#include <logging.h>                                         /* GetErrorStr */


int cf_closesocket(int sd)
{
    int res;

#ifdef __MINGW32__
    res = closesocket(sd);
    if (res == SOCKET_ERROR)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Failed to close socket (closesocket: %s)",
            GetErrorStrFromCode(WSAGetLastError()));
    }
#else
    res = close(sd);
    if (res == -1)
    {
        Log(LOG_LEVEL_VERBOSE,
            "Failed to close socket (close: %s)",
            GetErrorStr());
    }
#endif

    return res;
}

/* Return the port number in host byte order. */
uint16_t sockaddr_port(const void *sa)
{
    int family = ((struct sockaddr *) sa)->sa_family;
    uint16_t port;

    switch (family)
    {
    case AF_INET:
        port = ((struct sockaddr_in *) sa)->sin_port;
        break;

#ifdef HAVE_GETADDRINFO
    case AF_INET6:
        addr = ((struct sockaddr_in6 *) sa)->sin6_port;
        break;
#endif

    default:
        ProgrammingError("sockaddr_port: address family was %d", family);
    }

    return ntohs(port);
}

int sockaddr_AddrCompare(const void *sa1, const void *sa2)
{
    int sa1_family = ((struct sockaddr *) sa1)->sa_family;
    int sa2_family = ((struct sockaddr *) sa2)->sa_family;

    if ((sa1_family != AF_INET && sa1_family != AF_INET6) ||
        (sa2_family != AF_INET && sa2_family != AF_INET6))
    {
        ProgrammingError("sockaddr_AddrCompare: Unknown address families %d %d",
                         sa1_family, sa2_family);
    }

    if (sa1_family != sa2_family)
    {
        /* We consider any IPv4 address smaller than any IPv6 one. */
        return (sa1_family == AF_INET) ? -1 : 1;
    }

    int result;
    switch (sa1_family)
    {
    case AF_INET:
    {
        struct in_addr *addr1 = & ((struct sockaddr_in *) sa1)->sin_addr;
        struct in_addr *addr2 = & ((struct sockaddr_in *) sa2)->sin_addr;
        result = memcmp(addr1, addr2, sizeof(*addr1));
    }
    case AF_INET6:
    {
        struct in6_addr *addr1 = & ((struct sockaddr_in6 *) sa1)->sin6_addr;
        struct in6_addr *addr2 = & ((struct sockaddr_in6 *) sa2)->sin6_addr;
        result = memcmp(addr1, addr2, sizeof(*addr1));
    }
    default:
        assert(0);
        result = 0;
    }

    return result;
}


/* int sockaddr_AddrCompareMasked(const void *sa1, const void *sa2, int cidr_mask) */
/* { */
/*     assert(cidr_mask >= 0 && cidr_mask <= 128); */

/* } */
