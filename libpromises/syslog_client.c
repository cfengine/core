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

#include <syslog_client.h>
#include <printsize.h>

#include <cf3.defs.h>

/*
 * Set by cf-agent/cf-serverd from body agent/server control.
 */
static char SYSLOG_HOST[MAXHOSTNAMELEN] = "localhost";
/*
 * Set by cf-agent/cf-serverd from body agent/server control.
 */
static uint16_t SYSLOG_PORT = 514;
/*
 * Set by cf-agent/cf-serverd/cf-execd from body agent/exec/server control.
 */
static int SYSLOG_FACILITY = LOG_USER;


void SetSyslogFacility(int facility)
{
    SYSLOG_FACILITY = facility;
}

bool SetSyslogHost(const char *host)
{
    if (strlen(host) < sizeof(SYSLOG_HOST))
    {
        strcpy(SYSLOG_HOST, host);
        return true;
    }
    else
    {
        return false;
    }
}

void SetSyslogPort(uint16_t port)
{
    SYSLOG_PORT = port;
}

void RemoteSysLog(int log_priority, const char *log_string)
{
    time_t now = time(NULL);

    struct addrinfo query = { 0 }, *response = NULL;
    char strport[PRINTSIZE(unsigned)];
    xsnprintf(strport, sizeof(strport), "%u", (unsigned) SYSLOG_PORT);

    query.ai_family = AF_UNSPEC;
    query.ai_socktype = SOCK_DGRAM;

    int err = getaddrinfo(SYSLOG_HOST, strport, &query, &response);
    if (err != 0)
    {
        Log(LOG_LEVEL_INFO,
            "Unable to find syslog_host or service: (%s/%s) %s",
            SYSLOG_HOST, strport, gai_strerror(err));
        if (response != NULL)
        {
            freeaddrinfo(response);
        }
        return;
    }

    for (const struct addrinfo *ap = response; ap != NULL; ap = ap->ai_next)
    {
        /* No DNS lookup, just convert IP address to string. */
        char txtaddr[CF_MAX_IP_LEN] = "";
        getnameinfo(ap->ai_addr, ap->ai_addrlen,
                    txtaddr, sizeof(txtaddr),
                    NULL, 0, NI_NUMERICHOST);
        Log(LOG_LEVEL_VERBOSE,
            "Connect to syslog '%s' = '%s' on port '%s'",
            SYSLOG_HOST, txtaddr, strport);

        int sd = socket(ap->ai_family, ap->ai_socktype, IPPROTO_UDP);
        if (sd == -1)
        {
            Log(LOG_LEVEL_INFO, "Couldn't open a socket. (socket: %s)", GetErrorStr());
            continue;
        }
        else
        {
            const size_t rfc3164_len = 1024;
            char message[rfc3164_len];
            char timebuffer[26];
            pid_t pid = getpid();

            snprintf(message, sizeof(message), "<%i>%.15s %s %s[%d]: %s",
                     log_priority | SYSLOG_FACILITY,
                     cf_strtimestamp_local(now, timebuffer) + 4,
                     VFQNAME, VPREFIX, pid, log_string);
            err = sendto(sd, message, strlen(message),
                         0, ap->ai_addr, ap->ai_addrlen);
            if (err == -1)
            {
                Log(LOG_LEVEL_VERBOSE, "Couldn't send '%s' to syslog server '%s'. (sendto: %s)",
                    message, SYSLOG_HOST, GetErrorStr());
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Syslog message: '%s' to server '%s'", message, SYSLOG_HOST);
            }
            close(sd);
        }
    }

    freeaddrinfo(response);
}
