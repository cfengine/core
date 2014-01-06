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

#include <communication.h>
#include <connection_info.h>
#include <alloc.h>                                      /* xmalloc,... */
#include <logging.h>                                    /* Log */
#include <misc_lib.h>                                   /* ProgrammingError */
#include <buffer.h>                                     /* Buffer */
#include <ip_address.h>                                 /* IPAddress */

AgentConnection *NewAgentConn(const char *server_name, int partial)
{
    AgentConnection *conn = xcalloc(1, sizeof(AgentConnection));
    if (partial)
    {
        conn->conn_info = NULL;
    }
    else
    {
        ConnectionInfo *info = ConnectionInfoNew();
        conn->conn_info = info;
    }
    conn->family = AF_UNSPEC;
    conn->trust = false;
    conn->encryption_type = 'c';
    conn->this_server = xstrdup(server_name);
    conn->authenticated = false;
    return conn;
}

void DeleteAgentConn(AgentConnection *conn, int partial)
{
    Stat *sp = conn->cache;

    while (sp != NULL)
    {
        Stat *sps = sp;
        sp = sp->next;
        free(sps);
    }

    if (!partial)
    {
        ConnectionInfoDestroy(&conn->conn_info);
    }

    if (conn->session_key)
    {
        free(conn->session_key);
    }
    if (conn->this_server)
    {
        free(conn->this_server);
    }

    *conn = (AgentConnection) {0};
    free(conn);
}

/*
 * Needed due to unix_iface.c, but can be done better. Flagging for general cleanup if I have time.
 */
int IsIPV6Address(char *name)
{
    if (!name)
    {
        return false;
    }
    Buffer *buffer = BufferNewFrom(name, strlen(name));
    if (!buffer)
    {
        return false;
    }
    IPAddress *ip_address = NULL;
    bool is_ip = false;
    is_ip = IPAddressIsIPAddress(buffer, &ip_address);
    if (!is_ip)
    {
        BufferDestroy(&buffer);
        return false;
    }
    if (IPAddressType(ip_address) != IP_ADDRESS_TYPE_IPV6)
    {
        BufferDestroy(&buffer);
        IPAddressDestroy(&ip_address);
        return false;
    }
    BufferDestroy(&buffer);
    IPAddressDestroy(&ip_address);
    return true;
}

/*******************************************************************/

/*
 * XXX: This function is no longer used anywhere. Safe to remove?
 */
int IsIPV4Address(char *name)
{
    if (!name)
    {
        return false;
    }
    Buffer *buffer = BufferNewFrom(name, strlen(name));
    if (!buffer)
    {
        return false;
    }
    IPAddress *ip_address = NULL;
    bool is_ip = false;
    is_ip = IPAddressIsIPAddress(buffer, &ip_address);
    if (!is_ip)
    {
        BufferDestroy(&buffer);
        return false;
    }
    if (IPAddressType(ip_address) != IP_ADDRESS_TYPE_IPV4)
    {
        BufferDestroy(&buffer);
        IPAddressDestroy(&ip_address);
        return false;
    }
    BufferDestroy(&buffer);
    IPAddressDestroy(&ip_address);
    return true;
}

/*****************************************************************************/

/**
 * @brief DNS lookup of hostname, store the address as string into dst of size
 * dst_size.
 * @return -1 in case of unresolvable hostname or other error.
 */
/* XXX: This is somewhat of a kludge. Doesn't check dst_size for <=32 or <=128.
 * Needs to test dst_size then set ai_family explicit. query is just hugely bad
 * because AF_UNSPEC will prevent error if set IPv4 but only has IPv6 DNS and
 * vice versa. Also likely what's causing the IPv4/IPv6 address reversals.
 */
int Hostname2IPString(char *dst, const char *hostname, size_t dst_size)
{
    int ret;
    struct addrinfo *response, *ap;
    struct addrinfo query = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    if (dst_size < CF_MAX_IP_LEN)
    {
        ProgrammingError("Hostname2IPString got %zu, needs at least"
                         " %d length buffer for IPv6 portability!",
                         dst_size, CF_MAX_IP_LEN);
    }

    ret = getaddrinfo(hostname, NULL, &query, &response);
    if ((ret) != 0)
    {
        Log(LOG_LEVEL_INFO,
        	/* XXX: Bad practice. Should be checking host and port separate. -prj */
            "Unable to lookup hostname '%s' or cfengine service. (getaddrinfo: %s)",
            hostname, gai_strerror(ret));
        return -1;
    }

    for (ap = response; ap != NULL; ap = ap->ai_next)
    {
        /* No lookup, just convert numeric IP to string. */
    	/* XXX: Needs ai_family explicit. -prj */
        int ret2 = getnameinfo(ap->ai_addr, ap->ai_addrlen,
                               dst, dst_size, NULL, 0, NI_NUMERICHOST);
        if (ret2 == 0)
        {
            freeaddrinfo(response);
            return 0;                                           /* Success */
        }
    }
    freeaddrinfo(response);

    Log(LOG_LEVEL_ERR,
        "Hostname2IPString: ERROR even though getaddrinfo returned success!");
    return -1;
}

/*****************************************************************************/

/**
 * @brief Reverse DNS lookup of ipaddr, store the address as string into dst
 * of size dst_size.
 * @return -1 in case of unresolvable IP address or other error.
 */
int IPString2Hostname(char *dst, const char *ipaddr, size_t dst_size)
{
    int ret;
    struct addrinfo *response;

    /* XXX: This is obviously a kludge. Doesn't check dst_size for <=32 or <=128.
     * Needs to test dst_size then set ai_family explicit. query is just hugely bad
     * because AF_UNSPEC will prevent error if set IPv4 but only has IPv6 DNS and
     * vice versa. Also likely what's causing the IPv4/IPv6 address reversals.
     * Not sure where AF_UNSPEC is coming from in this function context though.. most likely server-functions?
     */

    /* First convert ipaddr string to struct sockaddr, with no DNS query. */
    struct addrinfo query = {
        .ai_flags = AI_NUMERICHOST
    };

    ret = getaddrinfo(ipaddr, NULL, &query, &response);
    if (ret != 0)
    {
        Log(LOG_LEVEL_ERR,
            "Unable to convert IP address '%s'. (getaddrinfo: %s)",
              ipaddr, gai_strerror(ret));
        return -1;
    }

    /* response should only have one reply, so no need to iterate over the
     * response struct addrinfo. */

    /* Reverse DNS lookup. NI_NAMEREQD forces an error if not resolvable. */
    ret = getnameinfo(response->ai_addr, response->ai_addrlen,
                      dst, dst_size, NULL, 0, NI_NAMEREQD);
    if (ret != 0)
    {
        Log(LOG_LEVEL_INFO,
            "Couldn't reverse resolve '%s'. (getaddrinfo: %s)",
            ipaddr, gai_strerror(ret));
        freeaddrinfo(response);
        return -1;
    }

    freeaddrinfo(response);
    return 0;                                                   /* Success */
}

/*****************************************************************************/

/* XXX: Wow, this is super broken. [MAXIP4CHARLEN]?! Uh. */
int GetMyHostInfo(char nameBuf[MAXHOSTNAMELEN], char ipBuf[MAXIP4CHARLEN])
{
    char *ip;
    struct hostent *hostinfo;

    if (gethostname(nameBuf, MAXHOSTNAMELEN) == 0)
    {
        if ((hostinfo = gethostbyname(nameBuf)) != NULL)
        {
            ip = inet_ntoa(*(struct in_addr *) *hostinfo->h_addr_list);
            strncpy(ipBuf, ip, MAXIP4CHARLEN - 1);
            ipBuf[MAXIP4CHARLEN - 1] = '\0';
            return true;
        }
        else
        {
            Log(LOG_LEVEL_ERR, "Could not get host entry for local host. (gethostbyname: %s)", GetErrorStr());
        }
    }
    else
    {
        Log(LOG_LEVEL_ERR, "Could not get host name. (gethostname: %s)", GetErrorStr());
    }

    return false;
}

/*****************************************************************************/

unsigned short SocketFamily(int sd)
{
   struct sockaddr sa = {0};
   socklen_t len = sizeof(sa);

   if (getsockname(sd, &sa, &len) == -1)
   {
       Log(LOG_LEVEL_ERR, "Could not get socket family. (getsockname: %s)", GetErrorStr());
   }

   return sa.sa_family;
}
