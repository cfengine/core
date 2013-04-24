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

#include "communication.h"


AgentConnection *NewAgentConn(const char *server_name)
{
    AgentConnection *conn = xcalloc(1, sizeof(AgentConnection));

    CfDebug("New server connection...\n");
    conn->sd = SOCKET_INVALID;
    conn->family = AF_INET;
    conn->trust = false;
    conn->encryption_type = 'c';
    conn->this_server = xstrdup(server_name);
    return conn;
};

void DeleteAgentConn(AgentConnection *conn)
{
    Stat *sp = conn->cache;

    while (sp != NULL)
    {
        Stat *sps = sp;
        sp = sp->next;
        free(sps);
    }

    free(conn->session_key);
    free(conn->this_server);
    free(conn);
}

int IsIPV6Address(char *name)
{
    char *sp;
    int count, max = 0;

    CfDebug("IsIPV6Address(%s)\n", name);

    if (name == NULL)
    {
        return false;
    }

    count = 0;

    for (sp = name; *sp != '\0'; sp++)
    {
        if (isalnum((int) *sp))
        {
            count++;
        }
        else if ((*sp != ':') && (*sp != '.'))
        {
            return false;
        }

        if (*sp == 'r')
        {
            return false;
        }

        if (count > max)
        {
            max = count;
        }
        else
        {
            count = 0;
        }
    }

    if (max <= 2)
    {
        CfDebug("Looks more like a MAC address");
        return false;
    }

    if (strstr(name, ":") == NULL)
    {
        return false;
    }

    if (strcasestr(name, "scope"))
    {
        return false;
    }

    return true;
}

/*******************************************************************/

int IsIPV4Address(char *name)
{
    char *sp;
    int count = 0;

    CfDebug("IsIPV4Address(%s)\n", name);

    if (name == NULL)
    {
        return false;
    }

    for (sp = name; *sp != '\0'; sp++)
    {
        if ((!isdigit((int) *sp)) && (*sp != '.'))
        {
            return false;
        }

        if (*sp == '.')
        {
            count++;
        }
    }

    if (count != 3)
    {
        return false;
    }

    return true;
}

/*****************************************************************************/

/* TODO thread-safe */
const char *Hostname2IPString(const char *hostname)
{
    int err;
    struct addrinfo query, *response, *ap;

    memset(&query, 0, sizeof(struct addrinfo));
    query.ai_family = AF_UNSPEC;
    query.ai_socktype = SOCK_STREAM;

    if ((err = getaddrinfo(hostname, NULL, &query, &response)) != 0)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "",
              "Unable to lookup hostname (%s) or cfengine service: %s",
              hostname, gai_strerror(err));
        return hostname;
    }

    static char ipbuffer[CF_MAX_IP_LEN] = {0};

    for (ap = response; ap != NULL; ap = ap->ai_next)
    {
        /* Convert numeric IP to string. */
        getnameinfo(ap->ai_addr, ap->ai_addrlen,
                    ipbuffer, sizeof(ipbuffer),
                    NULL, 0, NI_NUMERICHOST);
        CfDebug("Found address (%s) for host %s\n", ipbuffer, hostname);

        freeaddrinfo(response);
        return ipbuffer;
    }

    /* TODO return NULL? Must signify resolving failed? */
    snprintf(ipbuffer, sizeof(ipbuffer), "Unknown IP %s", hostname);
    return ipbuffer;
}

/*****************************************************************************/

char *IPString2Hostname(const char *ipaddress)
{
    static char hostbuffer[MAXHOSTNAMELEN];

    int err;
    struct addrinfo query, *response, *ap;

    memset(&query, 0, sizeof(query));
    memset(hostbuffer, 0, MAXHOSTNAMELEN);

    /* First convert ipaddress string to struct sockaddr, with no DNS query. */
    query.ai_flags = AI_NUMERICHOST;
    if ((err = getaddrinfo(ipaddress, NULL, &query, &response)) != 0)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "",
              "getaddrinfo: Unable to convert IP address (%s): %s",
              ipaddress, gai_strerror(err));
        strlcpy(hostbuffer, ipaddress, MAXHOSTNAMELEN);
        /* TODO return NULL? Must signify error somehow. */
        return hostbuffer;
    }

    for (ap = response; ap != NULL; ap = ap->ai_next)
    {
        /* Reverse DNS lookup. */
        if ((err = getnameinfo(ap->ai_addr, ap->ai_addrlen,
                               hostbuffer, MAXHOSTNAMELEN,
                               NULL, 0, 0)) != 0)
        {
            break;
        }
        CfDebug("Found address (%s) for host %s\n", hostbuffer, ipaddress);
        freeaddrinfo(response);
        return hostbuffer;
    }

    /* TODO return NULL to signify unsuccessful reverse query. */
    freeaddrinfo(response);
    strlcpy(hostbuffer, ipaddress, MAXHOSTNAMELEN);
    return hostbuffer;
}

/*****************************************************************************/

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
            CfOut(OUTPUT_LEVEL_ERROR, "gethostbyname", "!! Could not get host entry for local host");
        }
    }
    else
    {
        CfOut(OUTPUT_LEVEL_ERROR, "gethostname", "!! Could not get host name");
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
       CfOut(OUTPUT_LEVEL_ERROR, "getsockname", "!! Could not get socket family");
   }

   return sa.sa_family;
}
