/*
   Copyright 2018 Northern.tech AS

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

#include <cf3.defs.h>

#include <files_names.h>
#include <files_interfaces.h>
#include <mon.h>
#include <item_lib.h>
#include <pipes.h>
#include <signals.h>
#include <string_lib.h>
#include <misc_lib.h>
#include <addr_lib.h>
#include <known_dirs.h>

typedef enum
{
    IP_TYPES_ICMP,
    IP_TYPES_UDP,
    IP_TYPES_DNS,
    IP_TYPES_TCP_SYN,
    IP_TYPES_TCP_ACK,
    IP_TYPES_TCP_FIN,
    IP_TYPES_TCP_MISC
} IPTypes;

/* Constants */

#define CF_TCPDUMP_COMM "/usr/sbin/tcpdump -t -n -v"

static const int SLEEPTIME = 2.5 * 60;  /* Should be a fraction of 5 minutes */

static const char *const TCPNAMES[CF_NETATTR] =
{
    "icmp",
    "udp",
    "dns",
    "tcpsyn",
    "tcpack",
    "tcpfin",
    "misc"
};

/* Global variables */

static bool TCPDUMP = false;
static bool TCPPAUSE = false;
static FILE *TCPPIPE = NULL;

static Item *NETIN_DIST[CF_NETATTR] = { NULL };
static Item *NETOUT_DIST[CF_NETATTR] = { NULL };

/* Prototypes */

static void Sniff(Item *ip_addresses, long iteration, double *cf_this);
static void AnalyzeArrival(Item *ip_addresses, long iteration, char *arrival, double *cf_this);
static void DePort(char *address);

/* Implementation */

void MonNetworkSnifferSniff(Item *ip_addresses, long iteration, double *cf_this)
{
    if (TCPDUMP)
    {
        Sniff(ip_addresses, iteration, cf_this);
    }
    else
    {
        sleep(SLEEPTIME);
    }
}

/******************************************************************************/

void MonNetworkSnifferOpen(void)
{
    char tcpbuffer[CF_BUFSIZE];

    if (TCPDUMP)
    {
        struct stat statbuf;
        char buffer[CF_MAXVARSIZE];

        sscanf(CF_TCPDUMP_COMM, "%s", buffer);

        if (stat(buffer, &statbuf) != -1)
        {
            if ((TCPPIPE = cf_popen(CF_TCPDUMP_COMM, "r", true)) == NULL)
            {
                TCPDUMP = false;
            }

            /* Skip first banner */
            if (fgets(tcpbuffer, sizeof(tcpbuffer), TCPPIPE) == NULL)
            {
                UnexpectedError("Failed to read output from '%s'", CF_TCPDUMP_COMM);
                cf_pclose(TCPPIPE);
                TCPPIPE = NULL;
                TCPDUMP = false;
            }
        }
        else
        {
            TCPDUMP = false;
        }
    }
}

/******************************************************************************/

void MonNetworkSnifferEnable(bool enable)
{
    TCPDUMP = enable;
    Log(LOG_LEVEL_DEBUG, "use tcpdump = %d", TCPDUMP);
}

/******************************************************************************/

static void CfenvTimeOut(ARG_UNUSED int signum)
{
    alarm(0);
    TCPPAUSE = true;
    Log(LOG_LEVEL_VERBOSE, "Time out");
}

/******************************************************************************/

static void Sniff(Item *ip_addresses, long iteration, double *cf_this)
{
    char tcpbuffer[CF_BUFSIZE];

    Log(LOG_LEVEL_VERBOSE, "Reading from tcpdump...");
    memset(tcpbuffer, 0, CF_BUFSIZE);
    signal(SIGALRM, CfenvTimeOut);
    alarm(SLEEPTIME);
    TCPPAUSE = false;

    while (!feof(TCPPIPE) && !IsPendingTermination())
    {
        if (TCPPAUSE)
        {
            break;
        }

        if (fgets(tcpbuffer, sizeof(tcpbuffer), TCPPIPE) == NULL)
        {
            UnexpectedError("Unable to read data from tcpdump; closing pipe");
            cf_pclose(TCPPIPE);
            TCPPIPE = NULL;
            TCPDUMP = false;
            break;
        }

        if (TCPPAUSE)
        {
            break;
        }

        if (strstr(tcpbuffer, "tcpdump:"))      /* Error message protect sleeptime */
        {
            Log(LOG_LEVEL_DEBUG, "Error - '%s'", tcpbuffer);
            alarm(0);
            TCPDUMP = false;
            break;
        }

        AnalyzeArrival(ip_addresses, iteration, tcpbuffer, cf_this);
    }

    signal(SIGALRM, SIG_DFL);
    TCPPAUSE = false;
    fflush(TCPPIPE);
}

/******************************************************************************/

static void IncrementCounter(Item **list, char *name)
{
    if (!IsItemIn(*list, name))
    {
        AppendItem(list, name, "");
    }

    IncrementItemListCounter(*list, name);
}

/* This coarsely classifies TCP dump data */

static void AnalyzeArrival(Item *ip_addresses, long iteration, char *arrival, double *cf_this)
{
    char src[CF_BUFSIZE], dest[CF_BUFSIZE], flag = '.', *arr;
    int isme_dest, isme_src;

    src[0] = dest[0] = '\0';

    if (strstr(arrival, "listening"))
    {
        return;
    }

    if (Chop(arrival, CF_EXPANDSIZE) == -1)
    {
        Log(LOG_LEVEL_ERR, "Chop was called on a string that seemed to have no terminator");
    }

/* Most hosts have only a few dominant services, so anomalies will
   show up even in the traffic without looking too closely. This
   will apply only to the main interface .. not multifaces

   New format in tcpdump

   IP (tos 0x10, ttl 64, id 14587, offset 0, flags [DF], proto TCP (6), length 692) 128.39.89.232.22 > 84.215.40.125.48022: P 1546432:1547072(640) ack 1969 win 1593 <nop,nop,timestamp 25662737 1631360>
   IP (tos 0x0, ttl 251, id 14109, offset 0, flags [DF], proto UDP (17), length 115) 84.208.20.110.53 > 192.168.1.103.32769: 45266 NXDomain 0/1/0 (87)
   arp who-has 192.168.1.1 tell 192.168.1.103
   arp reply 192.168.1.1 is-at 00:1d:7e:28:22:c6
   IP (tos 0x0, ttl 64, id 0, offset 0, flags [DF], proto ICMP (1), length 84) 192.168.1.103 > 128.39.89.10: ICMP echo request, id 48474, seq 1, length 64
   IP (tos 0x0, ttl 64, id 0, offset 0, flags [DF], proto ICMP (1), length 84) 192.168.1.103 > 128.39.89.10: ICMP echo request, id 48474, seq 2, length 64
*/

    for (arr = strstr(arrival, "length"); (arr != NULL) && (*arr != ')'); arr++)
    {
    }

    if (arr == NULL)
    {
        arr = arrival;
    }
    else
    {
        arr++;
    }
    if ((strstr(arrival, "proto TCP")) || (strstr(arrival, "ack")))
    {
        assert(sizeof(src) == CF_BUFSIZE);
        assert(sizeof(dest) == CF_BUFSIZE);
        assert(CF_BUFSIZE == 4096);
        sscanf(arr, "%4095s %*c %4095s %c ", src, dest, &flag);
        DePort(src);
        DePort(dest);
        isme_dest = IsInterfaceAddress(ip_addresses, dest);
        isme_src = IsInterfaceAddress(ip_addresses, src);

        switch (flag)
        {
        case 'S':
            Log(LOG_LEVEL_DEBUG, "%ld: TCP new connection from '%s' to '%s' - i am '%s'", iteration, src, dest, VIPADDRESS);
            if (isme_dest)
            {
                cf_this[ob_tcpsyn_in]++;
                IncrementCounter(&(NETIN_DIST[IP_TYPES_TCP_SYN]), src);
            }
            else if (isme_src)
            {
                cf_this[ob_tcpsyn_out]++;
                IncrementCounter(&(NETOUT_DIST[IP_TYPES_TCP_SYN]), dest);
            }
            break;

        case 'F':
            Log(LOG_LEVEL_DEBUG, "%ld: TCP end connection from '%s' to '%s'", iteration, src, dest);
            if (isme_dest)
            {
                cf_this[ob_tcpfin_in]++;
                IncrementCounter(&(NETIN_DIST[IP_TYPES_TCP_FIN]), src);
            }
            else if (isme_src)
            {
                cf_this[ob_tcpfin_out]++;
                IncrementCounter(&(NETOUT_DIST[IP_TYPES_TCP_FIN]), dest);
            }
            break;

        default:
            Log(LOG_LEVEL_DEBUG, "%ld: TCP established from '%s' to '%s'", iteration, src, dest);

            if (isme_dest)
            {
                cf_this[ob_tcpack_in]++;
                IncrementCounter(&(NETIN_DIST[IP_TYPES_TCP_ACK]), src);
            }
            else if (isme_src)
            {
                cf_this[ob_tcpack_out]++;
                IncrementCounter(&(NETOUT_DIST[IP_TYPES_TCP_ACK]), dest);
            }
            break;
        }
    }
    else if (strstr(arrival, ".53"))
    {
        assert(sizeof(src) == CF_BUFSIZE);
        assert(sizeof(dest) == CF_BUFSIZE);
        assert(CF_BUFSIZE == 4096);
        sscanf(arr, "%4095s %*c %4095s %c ", src, dest, &flag);
        DePort(src);
        DePort(dest);
        isme_dest = IsInterfaceAddress(ip_addresses, dest);
        isme_src = IsInterfaceAddress(ip_addresses, src);

        Log(LOG_LEVEL_DEBUG, "%ld: DNS packet from '%s' to '%s'", iteration, src, dest);
        if (isme_dest)
        {
            cf_this[ob_dns_in]++;
            IncrementCounter(&(NETIN_DIST[IP_TYPES_DNS]), src);
        }
        else if (isme_src)
        {
            cf_this[ob_dns_out]++;
            IncrementCounter(&(NETOUT_DIST[IP_TYPES_TCP_ACK]), dest);
        }
    }
    else if (strstr(arrival, "proto UDP"))
    {
        assert(sizeof(src) == CF_BUFSIZE);
        assert(sizeof(dest) == CF_BUFSIZE);
        assert(CF_BUFSIZE == 4096);
        sscanf(arr, "%4095s %*c %4095s %c ", src, dest, &flag);
        DePort(src);
        DePort(dest);
        isme_dest = IsInterfaceAddress(ip_addresses, dest);
        isme_src = IsInterfaceAddress(ip_addresses, src);

        Log(LOG_LEVEL_DEBUG, "%ld: UDP packet from '%s' to '%s'", iteration, src, dest);
        if (isme_dest)
        {
            cf_this[ob_udp_in]++;
            IncrementCounter(&(NETIN_DIST[IP_TYPES_UDP]), src);
        }
        else if (isme_src)
        {
            cf_this[ob_udp_out]++;
            IncrementCounter(&(NETOUT_DIST[IP_TYPES_UDP]), dest);
        }
    }
    else if (strstr(arrival, "proto ICMP"))
    {
        assert(sizeof(src) == CF_BUFSIZE);
        assert(sizeof(dest) == CF_BUFSIZE);
        assert(CF_BUFSIZE == 4096);
        sscanf(arr, "%4095s %*c %4095s %c ", src, dest, &flag);
        DePort(src);
        DePort(dest);
        isme_dest = IsInterfaceAddress(ip_addresses, dest);
        isme_src = IsInterfaceAddress(ip_addresses, src);

        Log(LOG_LEVEL_DEBUG, "%ld: ICMP packet from '%s' to '%s'", iteration, src, dest);

        if (isme_dest)
        {
            cf_this[ob_icmp_in]++;
            IncrementCounter(&(NETIN_DIST[IP_TYPES_ICMP]), src);
        }
        else if (isme_src)
        {
            cf_this[ob_icmp_out]++;
            IncrementCounter(&(NETOUT_DIST[IP_TYPES_ICMP]), src);
        }
    }
    else
    {
        Log(LOG_LEVEL_DEBUG, "%ld: Miscellaneous undirected packet (%.100s)", iteration, arrival);

        cf_this[ob_tcpmisc_in]++;

        /* Here we don't know what source will be, but .... */
        assert(sizeof(src) == CF_BUFSIZE);
        assert(CF_BUFSIZE == 4096);
        sscanf(arrival, "%4095s", src);

        if (!isdigit((int) *src))
        {
            Log(LOG_LEVEL_DEBUG, "Assuming continuation line...");
            return;
        }

        DePort(src);

        if (strstr(arrival, ".138"))
        {
            snprintf(dest, CF_BUFSIZE - 1, "%s NETBIOS", src);
        }
        else if (strstr(arrival, ".2049"))
        {
            snprintf(dest, CF_BUFSIZE - 1, "%s NFS", src);
        }
        else
        {
            strncpy(dest, src, 60);
            dest[60] = '\0';
        }
        IncrementCounter(&(NETIN_DIST[IP_TYPES_TCP_MISC]), dest);
    }
}

/******************************************************************************/

static void SaveTCPEntropyData(Item *list, int i, char *inout)
{
    Item *ip;
    FILE *fp;
    char filename[CF_BUFSIZE];

    Log(LOG_LEVEL_VERBOSE, "TCP Save '%s'", TCPNAMES[i]);

    if (list == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "No %s-%s events", TCPNAMES[i], inout);
        return;
    }

    if (strncmp(inout, "in", 2) == 0)
    {
        snprintf(filename, CF_BUFSIZE, "%s%ccf_incoming.%s", GetStateDir(), FILE_SEPARATOR, TCPNAMES[i]);
    }
    else
    {
        snprintf(filename, CF_BUFSIZE, "%s%ccf_outgoing.%s", GetStateDir(), FILE_SEPARATOR, TCPNAMES[i]);
    }

    Log(LOG_LEVEL_VERBOSE, "TCP Save '%s'", filename);

    if ((fp = fopen(filename, "w")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't save TCP entropy to '%s' (fopen: %s)", filename, GetErrorStr());
        return;
    }

    for (ip = list; ip != NULL; ip = ip->next)
    {
        fprintf(fp, "%d %s\n", ip->counter, ip->name);
    }

    fclose(fp);
}

/******************************************************************************/

void MonNetworkSnifferGatherData(void)
{
    int i;
    char vbuff[CF_BUFSIZE];

    const char* const statedir = GetStateDir();

    for (i = 0; i < CF_NETATTR; i++)
    {
        struct stat statbuf;
        double entropy;
        time_t now = time(NULL);

        Log(LOG_LEVEL_DEBUG, "save incoming '%s'", TCPNAMES[i]);
        snprintf(vbuff, CF_MAXVARSIZE, "%s%ccf_incoming.%s", statedir, FILE_SEPARATOR, TCPNAMES[i]);

        if (stat(vbuff, &statbuf) != -1)
        {
            if (ItemListSize(NETIN_DIST[i]) < statbuf.st_size &&
                now < statbuf.st_mtime + 40 * 60)
            {
                Log(LOG_LEVEL_VERBOSE, "New state %s is smaller, retaining old for 40 mins longer", TCPNAMES[i]);
                DeleteItemList(NETIN_DIST[i]);
                NETIN_DIST[i] = NULL;
                continue;
            }
        }

        SaveTCPEntropyData(NETIN_DIST[i], i, "in");

        entropy = MonEntropyCalculate(NETIN_DIST[i]);
        MonEntropyClassesSet(TCPNAMES[i], "in", entropy);
        DeleteItemList(NETIN_DIST[i]);
        NETIN_DIST[i] = NULL;
    }

    for (i = 0; i < CF_NETATTR; i++)
    {
        struct stat statbuf;
        double entropy;
        time_t now = time(NULL);

        Log(LOG_LEVEL_DEBUG, "save outgoing '%s'", TCPNAMES[i]);
        snprintf(vbuff, CF_MAXVARSIZE, "%s%ccf_outgoing.%s", statedir, FILE_SEPARATOR, TCPNAMES[i]);

        if (stat(vbuff, &statbuf) != -1)
        {
            if (ItemListSize(NETOUT_DIST[i]) < statbuf.st_size &&
                now < statbuf.st_mtime + 40 * 60)
            {
                Log(LOG_LEVEL_VERBOSE, "New state '%s' is smaller, retaining old for 40 mins longer", TCPNAMES[i]);
                DeleteItemList(NETOUT_DIST[i]);
                NETOUT_DIST[i] = NULL;
                continue;
            }
        }

        SaveTCPEntropyData(NETOUT_DIST[i], i, "out");

        entropy = MonEntropyCalculate(NETOUT_DIST[i]);
        MonEntropyClassesSet(TCPNAMES[i], "out", entropy);
        DeleteItemList(NETOUT_DIST[i]);
        NETOUT_DIST[i] = NULL;
    }
}

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
