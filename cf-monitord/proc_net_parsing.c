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

#include <stdio.h>
#include <arpa/inet.h>

#include <logging.h>
#include <proc_net_parsing.h>

/* Inspired by https://git.kernel.org/pub/scm/network/iproute2/iproute2.git/tree/misc/ss.c#n2404 */
static bool SplitProcNetLine(const char *line, char **local, char **remote, char **state)
{
    /* Example data:
     *   2: B400A8C0:9DAE 38420D1F:01BB 01 00000000:00000000 ... (IPv4)
     *   0: 00000000000000000000000001000000:0277 00000000000000000000000000000000:0000 0A 00000000:00000000 ... (IPv6)
     */
    char *p;

    if ((p = strchr(line, ':')) == NULL)
    {
        return false;
    }

    *local = p + 2;
    if ((p = strchr(*local, ':')) == NULL)
    {
        return false;
    }

    *remote = p + 6;
    if ((p = strchr(*remote, ':')) == NULL)
    {
        return false;
    }

    *state = p + 6;
    return true;
}

bool ParseIPv4SocketInfo(const char *line,
                         char local_addr[INET_ADDRSTRLEN], uint32_t *local_port,
                         char remote_addr[INET_ADDRSTRLEN], uint32_t *remote_port,
                         SocketState *state)
{
    char *local, *remote, *state_str;
    if (!SplitProcNetLine(line, &local, &remote, &state_str))
    {
        return false;
    }

    /* else */
    struct in_addr l_addr;
    if (sscanf(local, "%x:%x", &(l_addr.s_addr), local_port) != 2)
    {
        Log(LOG_LEVEL_ERR,"Failed to parse local addr:port from line '%s'\n", line);
        return false;
    }
    struct in_addr r_addr;
    if (sscanf(remote, "%x:%x", &(r_addr.s_addr), remote_port) != 2)
    {
        Log(LOG_LEVEL_ERR, "Failed to parse remote addr:port from line '%s'\n", line);
        return false;
    }

    if (sscanf(state_str, "%x", state) != 1)
    {
        Log(LOG_LEVEL_ERR, "Failed to parse state from line '%s'\n", line);
        return false;
    }

    if (inet_ntop(AF_INET, &l_addr, local_addr, INET_ADDRSTRLEN) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to get ASCII representation of local address");
        return false;
    }
    if (inet_ntop(AF_INET, &r_addr, remote_addr, INET_ADDRSTRLEN) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to get ASCII representation of remote address");
        return false;
    }
    return true;
}

bool ParseIPv6SocketInfo(const char *line,
                         char local_addr[INET6_ADDRSTRLEN], uint32_t *local_port,
                         char remote_addr[INET6_ADDRSTRLEN], uint32_t *remote_port,
                         SocketState *state)
{
    char *local, *remote, *state_str;
    if (!SplitProcNetLine(line, &local, &remote, &state_str))
    {
        return false;
    }

    /* else */
    struct in6_addr l_addr;
    uint32_t *addr_data = (uint32_t*) l_addr.s6_addr;
    if (sscanf(local, "%08x%08x%08x%08x:%x",
               addr_data, addr_data + 1, addr_data + 2, addr_data + 3,
               local_port) != 5)
    {
        Log(LOG_LEVEL_ERR,"Failed to parse local addr:port from line '%s'\n", line);
        return false;
    }
    struct in6_addr r_addr;
    addr_data = (uint32_t*) r_addr.s6_addr;
    if (sscanf(remote, "%08x%08x%08x%08x:%x",
               addr_data, addr_data + 1, addr_data + 2, addr_data + 3,
               remote_port) != 5)
    {
        Log(LOG_LEVEL_ERR, "Failed to parse remote addr:port from line '%s'\n", line);
        return false;
    }

    if (sscanf(state_str, "%x", state) != 1)
    {
        Log(LOG_LEVEL_ERR, "Failed to parse state from line '%s'\n", line);
        return false;
    }

    if (inet_ntop(AF_INET6, &l_addr, local_addr, INET6_ADDRSTRLEN) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to get ASCII representation of local address");
        return false;
    }
    if (inet_ntop(AF_INET6, &r_addr, remote_addr, INET6_ADDRSTRLEN) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to get ASCII representation of remote address");
        return false;
    }
    return true;
}
