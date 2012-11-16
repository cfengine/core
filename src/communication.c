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

#include "cfstream.h"

/*********************************************************************/

AgentConnection *NewAgentConn()
{
    AgentConnection *ap;

    ap = xcalloc(1, sizeof(AgentConnection));

    CfDebug("New server connection...\n");
    ap->sd = SOCKET_INVALID;
    ap->family = AF_INET;
    ap->trust = false;
    ap->encryption_type = 'c';
    return ap;
};

/*********************************************************************/

void DeleteAgentConn(AgentConnection *ap)
{
    if (ap->session_key != NULL)
    {
        free(ap->session_key);
    }

    free(ap);
    ap = NULL;
}

/**********************************************************************/

void DePort(char *address)
{
    char *sp, *chop, *fc = NULL, *fd = NULL, *ld = NULL;
    int ccount = 0, dcount = 0;

/* Start looking for ethernet/ipv6 addresses */

    for (sp = address; *sp != '\0'; sp++)
    {
        if (*sp == ':')
        {
            if (!fc)
            {
                fc = sp;
            }
            ccount++;
        }

        if (*sp == '.')
        {
            if (!fd)
            {
                fd = sp;
            }

            ld = sp;

            dcount++;
        }
    }

    if (!fd)
    {
        /* This does not look like an IP address+port, maybe ethernet */
        return;
    }

    if (dcount == 4)
    {
        chop = ld;
    }
    else if ((dcount > 1) && (fc != NULL))
    {
        chop = fc;
    }
    else if ((ccount > 1) && (fd != NULL))
    {
        chop = fd;
    }
    else
    {
        /* Don't recognize address */
        return;
    }

    if (chop < address + strlen(address))
    {
        *chop = '\0';
    }

    return;
}

/*******************************************************************/

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

const char *Hostname2IPString(const char *hostname)
{
    static char ipbuffer[CF_SMALLBUF];

#if defined(HAVE_GETADDRINFO)
    int err;
    struct addrinfo query, *response, *ap;

    memset(&query, 0, sizeof(struct addrinfo));
    query.ai_family = AF_UNSPEC;
    query.ai_socktype = SOCK_STREAM;

    memset(ipbuffer, 0, CF_SMALLBUF - 1);

    if ((err = getaddrinfo(hostname, NULL, &query, &response)) != 0)
    {
        CfOut(cf_inform, "", "Unable to lookup hostname (%s) or cfengine service: %s", hostname, gai_strerror(err));
        return hostname;
    }

    for (ap = response; ap != NULL; ap = ap->ai_next)
    {
        strncpy(ipbuffer, sockaddr_ntop(ap->ai_addr), 64);
        CfDebug("Found address (%s) for host %s\n", ipbuffer, hostname);

        if (strlen(ipbuffer) == 0)
        {
            snprintf(ipbuffer, CF_SMALLBUF - 1, "Empty IP result for %s", hostname);
        }

        freeaddrinfo(response);
        return ipbuffer;
    }
#else
    struct hostent *hp;
    struct sockaddr_in cin;

    memset(&cin, 0, sizeof(cin));

    memset(ipbuffer, 0, CF_SMALLBUF - 1);

    if ((hp = gethostbyname(hostname)) != NULL)
    {
        cin.sin_addr.s_addr = ((struct in_addr *) (hp->h_addr))->s_addr;
        strncpy(ipbuffer, inet_ntoa(cin.sin_addr), CF_SMALLBUF - 1);
        CfOut(cf_verbose, "", "Found address (%s) for host %s\n", ipbuffer, hostname);
        return ipbuffer;
    }
#endif

    snprintf(ipbuffer, CF_SMALLBUF - 1, "Unknown IP %s", hostname);
    return ipbuffer;
}

/*****************************************************************************/

char *IPString2Hostname(const char *ipaddress)
{
    static char hostbuffer[MAXHOSTNAMELEN];

#if defined(HAVE_GETADDRINFO)
    int err;
    struct addrinfo query, *response, *ap;

    memset(&query, 0, sizeof(query));
    memset(&response, 0, sizeof(response));

    query.ai_flags = AI_CANONNAME;

    memset(hostbuffer, 0, MAXHOSTNAMELEN);

    if ((err = getaddrinfo(ipaddress, NULL, &query, &response)) != 0)
    {
        CfOut(cf_inform, "", "Unable to lookup IP address (%s): %s", ipaddress, gai_strerror(err));
        strlcpy(hostbuffer, ipaddress, MAXHOSTNAMELEN);
        return hostbuffer;
    }

    for (ap = response; ap != NULL; ap = ap->ai_next)
    {
        if ((err = getnameinfo(ap->ai_addr, ap->ai_addrlen, hostbuffer, MAXHOSTNAMELEN, 0, 0, 0)) != 0)
        {
            strlcpy(hostbuffer, ipaddress, MAXHOSTNAMELEN);
            freeaddrinfo(response);
            return hostbuffer;
        }

        CfDebug("Found address (%s) for host %s\n", hostbuffer, ipaddress);
        freeaddrinfo(response);
        return hostbuffer;
    }

    strlcpy(hostbuffer, ipaddress, MAXHOSTNAMELEN);

#else

    struct hostent *hp;
    struct in_addr iaddr;

    memset(hostbuffer, 0, MAXHOSTNAMELEN);

    if ((iaddr.s_addr = inet_addr(ipaddress)) != -1)
    {
        hp = gethostbyaddr((void *) &iaddr, sizeof(struct sockaddr_in), AF_INET);

        if ((hp == NULL) || (hp->h_name == NULL))
        {
            strcpy(hostbuffer, ipaddress);
            return hostbuffer;
        }

        strncpy(hostbuffer, hp->h_name, MAXHOSTNAMELEN - 1);
    }
    else
    {
        strcpy(hostbuffer, "(non registered IP)");
    }

#endif

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
            CfOut(cf_error, "gethostbyname", "!! Could not get host entry for local host");
        }
    }
    else
    {
        CfOut(cf_error, "gethostname", "!! Could not get host name");
    }

    return false;
}

/*****************************************************************************/

unsigned short SocketFamily(int sd)
{
   struct sockaddr sa;
   socklen_t len;
   getsockname(sd, &sa, &len);
   return sa.sa_family;
}
