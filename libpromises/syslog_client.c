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

#include "syslog_client.h"

#include "logging.h"

static char SYSLOG_HOST[CF_BUFSIZE] = "localhost";
static uint16_t SYSLOG_PORT = 514;
int FACILITY;

void SetSyslogFacility(int facility)
{
    FACILITY = facility;
}

void SetSyslogHost(const char *host)
{
    strlcpy(SYSLOG_HOST, host, CF_BUFSIZE);
}

void SetSyslogPort(uint16_t port)
{
    SYSLOG_PORT = port;
}

void RemoteSysLog(int log_priority, const char *log_string)
{
    int sd, rfc3164_len = 1024;
    char message[CF_BUFSIZE];
    time_t now = time(NULL);
    int pri = log_priority | FACILITY;

    int err;
    struct addrinfo query, *response, *ap;
    char strport[CF_MAXVARSIZE];

    snprintf(strport, CF_MAXVARSIZE - 1, "%u", (unsigned) SYSLOG_PORT);
    memset(&query, 0, sizeof(query));
    query.ai_family = AF_UNSPEC;
    query.ai_socktype = SOCK_DGRAM;

    if ((err = getaddrinfo(SYSLOG_HOST, strport, &query, &response)) != 0)
    {
        CfOut(OUTPUT_LEVEL_INFORM, "",
              "Unable to find syslog_host or service: (%s/%s) %s",
              SYSLOG_HOST, strport, gai_strerror(err));
        return;
    }

    for (ap = response; ap != NULL; ap = ap->ai_next)
    {
        /* No DNS lookup, just convert IP address to string. */
        char txtaddr[CF_MAX_IP_LEN] = "";
        getnameinfo(ap->ai_addr, ap->ai_addrlen,
                    txtaddr, sizeof(txtaddr),
                    NULL, 0, NI_NUMERICHOST);
        CfOut(OUTPUT_LEVEL_VERBOSE, "",
              " -> Connect to syslog %s = %s on port %s\n",
              SYSLOG_HOST, txtaddr, strport);

        if ((sd = socket(ap->ai_family, ap->ai_socktype, IPPROTO_UDP)) == -1)
        {
            CfOut(OUTPUT_LEVEL_INFORM, "socket", "Couldn't open a socket");
            continue;
        }
        else
        {
            char timebuffer[26];

            snprintf(message, rfc3164_len, "<%u>%.15s %s %s",
                     pri, cf_strtimestamp_local(now, timebuffer) + 4,
                     VFQNAME, log_string);
            err = sendto(sd, message, strlen(message),
                         0, ap->ai_addr, ap->ai_addrlen);
            if (err == -1)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "sendto",
                      " -> Couldn't send \"%s\" to syslog server \"%s\"\n",
                      message, SYSLOG_HOST);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "",
                      " -> Syslog message: \"%s\" to server \"%s\"\n",
                      message, SYSLOG_HOST);
            }
            close(sd);
        }
    }

    freeaddrinfo(response);
}
