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

#include "cf3.defs.h"

#include "sysinfo.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "monitoring.h"
#include "item_lib.h"
#include "cfstream.h"
#include "communication.h"
#include "pipes.h"

/* Constants */

#define CF_TCPDUMP_COMM "/usr/sbin/tcpdump -t -n -v"

static const int SLEEPTIME = 2.5 * 60;  /* Should be a fraction of 5 minutes */

/* Global variables */

static bool TCPDUMP;
static bool TCPPAUSE;
static FILE *TCPPIPE;

static Item *NETIN_DIST[CF_NETATTR];
static Item *NETOUT_DIST[CF_NETATTR];

/* Prototypes */

static void Sniff(long iteration, double *cf_this);
static void AnalyzeArrival(long iteration, char *arrival, double *cf_this);

/* Implementation */

void MonNetworkSnifferSniff(long iteration, double *cf_this)
{
    if (TCPDUMP)
    {
        Sniff(iteration, cf_this);
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

        if (cfstat(buffer, &statbuf) != -1)
        {
            if ((TCPPIPE = cf_popen(CF_TCPDUMP_COMM, "r")) == NULL)
            {
                TCPDUMP = false;
            }

            /* Skip first banner */
            fgets(tcpbuffer, CF_BUFSIZE - 1, TCPPIPE);
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
    CfDebug("use tcpdump = %d\n", TCPDUMP);
}

/******************************************************************************/

static void CfenvTimeOut(int signum)
{
    alarm(0);
    TCPPAUSE = true;
    CfOut(cf_verbose, "", "Time out\n");
}

/******************************************************************************/

static void Sniff(long iteration, double *cf_this)
{
    char tcpbuffer[CF_BUFSIZE];

    CfOut(cf_verbose, "", "Reading from tcpdump...\n");
    memset(tcpbuffer, 0, CF_BUFSIZE);
    signal(SIGALRM, CfenvTimeOut);
    alarm(SLEEPTIME);
    TCPPAUSE = false;

    while (!feof(TCPPIPE))
    {
        if (TCPPAUSE)
        {
            break;
        }

        fgets(tcpbuffer, CF_BUFSIZE - 1, TCPPIPE);

        if (TCPPAUSE)
        {
            break;
        }

        if (strstr(tcpbuffer, "tcpdump:"))      /* Error message protect sleeptime */
        {
            CfDebug("Error - (%s)\n", tcpbuffer);
            alarm(0);
            TCPDUMP = false;
            break;
        }

        AnalyzeArrival(iteration, tcpbuffer, cf_this);
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

/******************************************************************************/

/* This coarsely classifies TCP dump data */

static void AnalyzeArrival(long iteration, char *arrival, double *cf_this)
{
    char src[CF_BUFSIZE], dest[CF_BUFSIZE], flag = '.', *arr;
    int isme_dest, isme_src;

    src[0] = dest[0] = '\0';

    if (strstr(arrival, "listening"))
    {
        return;
    }

    Chop(arrival);

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
        sscanf(arr, "%s %*c %s %c ", src, dest, &flag);
        DePort(src);
        DePort(dest);
        isme_dest = IsInterfaceAddress(dest);
        isme_src = IsInterfaceAddress(src);

        switch (flag)
        {
        case 'S':
            CfDebug("%ld: TCP new connection from %s to %s - i am %s\n", iteration, src, dest, VIPADDRESS);
            if (isme_dest)
            {
                cf_this[ob_tcpsyn_in]++;
                IncrementCounter(&(NETIN_DIST[tcpsyn]), src);
            }
            else if (isme_src)
            {
                cf_this[ob_tcpsyn_out]++;
                IncrementCounter(&(NETOUT_DIST[tcpsyn]), dest);
            }
            break;

        case 'F':
            CfDebug("%ld: TCP end connection from %s to %s\n", iteration, src, dest);
            if (isme_dest)
            {
                cf_this[ob_tcpfin_in]++;
                IncrementCounter(&(NETIN_DIST[tcpfin]), src);
            }
            else if (isme_src)
            {
                cf_this[ob_tcpfin_out]++;
                IncrementCounter(&(NETOUT_DIST[tcpfin]), dest);
            }
            break;

        default:
            CfDebug("%ld: TCP established from %s to %s\n", iteration, src, dest);

            if (isme_dest)
            {
                cf_this[ob_tcpack_in]++;
                IncrementCounter(&(NETIN_DIST[tcpack]), src);
            }
            else if (isme_src)
            {
                cf_this[ob_tcpack_out]++;
                IncrementCounter(&(NETOUT_DIST[tcpack]), dest);
            }
            break;
        }
    }
    else if (strstr(arrival, ".53"))
    {
        sscanf(arr, "%s %*c %s %c ", src, dest, &flag);
        DePort(src);
        DePort(dest);
        isme_dest = IsInterfaceAddress(dest);
        isme_src = IsInterfaceAddress(src);

        CfDebug("%ld: DNS packet from %s to %s\n", iteration, src, dest);
        if (isme_dest)
        {
            cf_this[ob_dns_in]++;
            IncrementCounter(&(NETIN_DIST[dns]), src);
        }
        else if (isme_src)
        {
            cf_this[ob_dns_out]++;
            IncrementCounter(&(NETOUT_DIST[tcpack]), dest);
        }
    }
    else if (strstr(arrival, "proto UDP"))
    {
        sscanf(arr, "%s %*c %s %c ", src, dest, &flag);
        DePort(src);
        DePort(dest);
        isme_dest = IsInterfaceAddress(dest);
        isme_src = IsInterfaceAddress(src);

        CfDebug("%ld: UDP packet from %s to %s\n", iteration, src, dest);
        if (isme_dest)
        {
            cf_this[ob_udp_in]++;
            IncrementCounter(&(NETIN_DIST[udp]), src);
        }
        else if (isme_src)
        {
            cf_this[ob_udp_out]++;
            IncrementCounter(&(NETOUT_DIST[udp]), dest);
        }
    }
    else if (strstr(arrival, "proto ICMP"))
    {
        sscanf(arr, "%s %*c %s %c ", src, dest, &flag);
        DePort(src);
        DePort(dest);
        isme_dest = IsInterfaceAddress(dest);
        isme_src = IsInterfaceAddress(src);

        CfDebug("%ld: ICMP packet from %s to %s\n", iteration, src, dest);

        if (isme_dest)
        {
            cf_this[ob_icmp_in]++;
            IncrementCounter(&(NETIN_DIST[icmp]), src);
        }
        else if (isme_src)
        {
            cf_this[ob_icmp_out]++;
            IncrementCounter(&(NETOUT_DIST[icmp]), src);
        }
    }
    else
    {
        CfDebug("%ld: Miscellaneous undirected packet (%.100s)\n", iteration, arrival);

        cf_this[ob_tcpmisc_in]++;

        /* Here we don't know what source will be, but .... */

        sscanf(arrival, "%s", src);

        if (!isdigit((int) *src))
        {
            CfDebug("Assuming continuation line...\n");
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
        }
        IncrementCounter(&(NETIN_DIST[tcpmisc]), dest);
    }
}

/******************************************************************************/

static void SaveTCPEntropyData(Item *list, int i, char *inout)
{
    Item *ip;
    FILE *fp;
    char filename[CF_BUFSIZE];

    CfOut(cf_verbose, "", "TCP Save %s\n", TCPNAMES[i]);

    if (list == NULL)
    {
        CfOut(cf_verbose, "", "No %s-%s events\n", TCPNAMES[i], inout);
        return;
    }

    if (strncmp(inout, "in", 2) == 0)
    {
        snprintf(filename, CF_BUFSIZE - 1, "%s/state/cf_incoming.%s", CFWORKDIR, TCPNAMES[i]);
    }
    else
    {
        snprintf(filename, CF_BUFSIZE - 1, "%s/state/cf_outgoing.%s", CFWORKDIR, TCPNAMES[i]);
    }

    CfOut(cf_verbose, "", "TCP Save %s\n", filename);

    if ((fp = fopen(filename, "w")) == NULL)
    {
        CfOut(cf_verbose, "", "Unable to write datafile %s\n", filename);
        return;
    }

    for (ip = list; ip != NULL; ip = ip->next)
    {
        fprintf(fp, "%d %s\n", ip->counter, ip->name);
    }

    fclose(fp);
}

/******************************************************************************/

void MonNetworkSnifferGatherData(double *cf_this)
{
    int i;
    char vbuff[CF_BUFSIZE];

    for (i = 0; i < CF_NETATTR; i++)
    {
        struct stat statbuf;
        double entropy;
        time_t now = time(NULL);

        CfDebug("save incoming %s\n", TCPNAMES[i]);
        snprintf(vbuff, CF_MAXVARSIZE, "%s/state/cf_incoming.%s", CFWORKDIR, TCPNAMES[i]);

        if (cfstat(vbuff, &statbuf) != -1)
        {
            if ((ByteSizeList(NETIN_DIST[i]) < statbuf.st_size) && (now < statbuf.st_mtime + 40 * 60))
            {
                CfOut(cf_verbose, "", "New state %s is smaller, retaining old for 40 mins longer\n", TCPNAMES[i]);
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

        CfDebug("save outgoing %s\n", TCPNAMES[i]);
        snprintf(vbuff, CF_MAXVARSIZE, "%s/state/cf_outgoing.%s", CFWORKDIR, TCPNAMES[i]);

        if (cfstat(vbuff, &statbuf) != -1)
        {
            if ((ByteSizeList(NETOUT_DIST[i]) < statbuf.st_size) && (now < statbuf.st_mtime + 40 * 60))
            {
                CfOut(cf_verbose, "", "New state %s is smaller, retaining old for 40 mins longer\n", TCPNAMES[i]);
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
