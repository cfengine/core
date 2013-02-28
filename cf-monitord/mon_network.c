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

#include "mon.h"
#include "item_lib.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "files_lib.h"
#include "cfstream.h"
#include "pipes.h"
#include "logging.h"

/* Globals */

Item *ALL_INCOMING;
Item *MON_UDP4 = NULL, *MON_UDP6 = NULL, *MON_TCP4 = NULL, *MON_TCP6 = NULL;

/*******************************************************************/
/* Anomaly                                                         */
/*******************************************************************/

typedef struct
{
    char *portnr;
    char *name;
    enum observables in;
    enum observables out;
} Sock;

static const Sock ECGSOCKS[ATTR] =     /* extended to map old to new using enum */
{
    {"137", "netbiosns", ob_netbiosns_in, ob_netbiosns_out},
    {"138", "netbiosdgm", ob_netbiosdgm_in, ob_netbiosdgm_out},
    {"139", "netbiosssn", ob_netbiosssn_in, ob_netbiosssn_out},
    {"445", "microsoft_ds", ob_microsoft_ds_in, ob_microsoft_ds_out},
    {"5308", "cfengine", ob_cfengine_in, ob_cfengine_out},
    {"2049", "nfsd", ob_nfsd_in, ob_nfsd_out},
    {"25", "smtp", ob_smtp_in, ob_smtp_out},
    {"80", "www", ob_www_in, ob_www_out},
    {"8080", "www-alt", ob_www_alt_in, ob_www_alt_out},
    {"21", "ftp", ob_ftp_in, ob_ftp_out},
    {"22", "ssh", ob_ssh_in, ob_ssh_out},
    {"443", "wwws", ob_wwws_in, ob_wwws_out},
    {"143", "imap", ob_imap_in, ob_imap_out},
    {"993", "imaps", ob_imaps_in, ob_imaps_out},
    {"389", "ldap", ob_ldap_in, ob_ldap_out},
    {"636", "ldaps", ob_ldaps_in, ob_ldaps_out},
    {"27017", "mongo", ob_mongo_in, ob_mongo_out},
    {"3306", "mysql", ob_mysql_in, ob_mysql_out},
    {"5432", "postgresql", ob_postgresql_in, ob_postgresql_out},
    {"631", "ipp", ob_ipp_in, ob_ipp_out},
};

static const char *VNETSTAT[PLATFORM_CONTEXT_MAX] =
{
    "-",
    "/usr/bin/netstat -rn",     /* hpux */
    "/usr/bin/netstat -rn",     /* aix */
    "/bin/netstat -rn",         /* linux */
    "/usr/bin/netstat -rn",     /* solaris */
    "/usr/bin/netstat -rn",     /* freebsd */
    "/usr/bin/netstat -rn",     /* netbsd */
    "/usr/ucb/netstat -rn",     /* cray */
    "/cygdrive/c/WINNT/System32/netstat",       /* NT */
    "/usr/bin/netstat -rn",     /* Unixware */
    "/usr/bin/netstat -rn",     /* openbsd */
    "/usr/bin/netstat -rn",     /* sco */
    "/usr/sbin/netstat -rn",    /* darwin */
    "/usr/bin/netstat -rn",     /* qnx */
    "/usr/bin/netstat -rn",     /* dragonfly */
    "mingw-invalid",            /* mingw */
    "/usr/bin/netstat",         /* vmware */
};

/* Implementation */

void MonNetworkInit(void)
{
 
    DeleteItemList(MON_TCP4);
    DeleteItemList(MON_TCP6);
    DeleteItemList(MON_UDP4);
    DeleteItemList(MON_UDP6);
 
    MON_UDP4 = MON_UDP6 = MON_TCP4 = MON_TCP6 = NULL;

    for (int i = 0; i < ATTR; i++)
    {
        char vbuff[CF_BUFSIZE];

        sprintf(vbuff, "%s/state/cf_incoming.%s", CFWORKDIR, ECGSOCKS[i].name);
        MapName(vbuff);
        CreateEmptyFile(vbuff);

        sprintf(vbuff, "%s/state/cf_outgoing.%s", CFWORKDIR, ECGSOCKS[i].name);
        MapName(vbuff);
        CreateEmptyFile(vbuff);
    }
}

/******************************************************************************/

static void SetNetworkEntropyClasses(const char *service, const char *direction, const Item *list)
{
    const Item *ip;
    Item *addresses = NULL;
    double entropy;

    for (ip = list; ip != NULL; ip = ip->next)
    {
        if (strlen(ip->name) > 0)
        {
            char local[CF_BUFSIZE];
            char remote[CF_BUFSIZE];
            char vbuff[CF_BUFSIZE];
            char *sp;

            if (strncmp(ip->name, "tcp", 3) == 0)
            {
                sscanf(ip->name, "%*s %*s %*s %s %s", local, remote);   /* linux-like */
            }
            else
            {
                sscanf(ip->name, "%s %s", local, remote);       /* solaris-like */
            }

            strncpy(vbuff, remote, CF_BUFSIZE - 1);
            vbuff[CF_BUFSIZE-1] = '\0';

            for (sp = vbuff + strlen(vbuff) - 1; isdigit((int) *sp) && (sp > vbuff); sp--)
            {
            }

            *sp = '\0';

            if (!IsItemIn(addresses, vbuff))
            {
                AppendItem(&addresses, vbuff, "");
            }

            IncrementItemListCounter(addresses, vbuff);
        }
    }

    entropy = MonEntropyCalculate(addresses);
    MonEntropyClassesSet(service, direction, entropy);
    DeleteItemList(addresses);
}

/******************************************************************************/

void MonNetworkGatherData(double *cf_this)
{
    FILE *pp;
    char local[CF_BUFSIZE], remote[CF_BUFSIZE], comm[CF_BUFSIZE];
    Item *in[ATTR], *out[ATTR];
    char *sp;
    int i;
    char vbuff[CF_BUFSIZE];
    enum cf_netstat_type { cfn_new, cfn_old } type = cfn_new;
    enum cf_packet_type { cfn_udp4, cfn_udp6, cfn_tcp4, cfn_tcp6} packet = cfn_tcp4;

    CfDebug("GatherSocketData()\n");

    for (i = 0; i < ATTR; i++)
    {
        in[i] = out[i] = NULL;
    }

    DeleteItemList(ALL_INCOMING);
    ALL_INCOMING = NULL;

    sscanf(VNETSTAT[VSYSTEMHARDCLASS], "%s", comm);

    strcat(comm, " -an");

    if ((pp = cf_popen(comm, "r")) == NULL)
    {
        return;
    }

    while (!feof(pp))
    {
        memset(local, 0, CF_BUFSIZE);
        memset(remote, 0, CF_BUFSIZE);

        if (CfReadLine(vbuff, CF_BUFSIZE, pp) == -1)
        {
            FatalError("Error in CfReadLine");
        }

        if (strstr(vbuff, "UNIX"))
        {
            break;
        }

        if (!((strstr(vbuff, ":")) || (strstr(vbuff, "."))))
        {
            continue;
        }

        /* Different formats here ... ugh.. pick a model */

        // If this is old style, we look for chapter headings, e.g. "TCP: IPv4"

        if ((strncmp(vbuff,"UDP:",4) == 0) && (strstr(vbuff+4,"6")))
        {
            packet = cfn_udp6;
            type = cfn_old;
            continue;
        }
        else if ((strncmp(vbuff,"TCP:",4) == 0) && (strstr(vbuff+4,"6")))
        {
            packet = cfn_tcp6;
            type = cfn_old;
            continue;
        }
        else if ((strncmp(vbuff,"UDP:",4) == 0) && (strstr(vbuff+4,"4")))
        {
            packet = cfn_udp4;
            type = cfn_old;
            continue;
        }
        else if ((strncmp(vbuff,"TCP:",4) == 0) && (strstr(vbuff+4,"4")))
        {
            packet = cfn_tcp4;
            type = cfn_old;
            continue;
        }

        // Line by line state in modern/linux output

        if (strncmp(vbuff,"udp6",4) == 0)
        {
            packet = cfn_udp6;
            type = cfn_new;
        }
        else if (strncmp(vbuff,"tcp6",4) == 0)
        {
            packet = cfn_tcp6;
            type = cfn_new;
        }
        else if (strncmp(vbuff,"udp",3) == 0)
        {
            packet = cfn_udp4;
            type = cfn_new;
        }
        else if (strncmp(vbuff,"tcp",3) == 0)
        {
            packet = cfn_tcp4;
            type = cfn_new;
        }

        // End extract type
        
        switch (type)
        {
        case cfn_new:  sscanf(vbuff, "%*s %*s %*s %s %s", local, remote);  /* linux-like */
            break;
            
        case cfn_old:
            sscanf(vbuff, "%s %s", local, remote);
            break;
        }

        if (strlen(local) == 0)
        {
            continue;
        }

        // Extract the port number from the end of the string

        for (sp = local + strlen(local); (*sp != '.') && (*sp != ':')  && (sp > local); sp--)
        {
        }

        *sp = '\0'; // Separate address from port number
        sp++;

        char *localport = sp;
        
        if (strstr(vbuff, "LISTEN"))
        {
            // General bucket

            IdempPrependItem(&ALL_INCOMING, sp, NULL);

            // Categories the incoming ports by packet types

            switch (packet)
            {
            case cfn_udp4:
                IdempPrependItem(&MON_UDP4, sp, local);
                break;
            case cfn_udp6:
                IdempPrependItem(&MON_UDP6, sp, local);
                break;
            case cfn_tcp4:
                IdempPrependItem(&MON_TCP4, localport, local);
                break;
            case cfn_tcp6:
                IdempPrependItem(&MON_TCP6, localport, local);
                break;
            default:
                break;
            }
        }


        // Now look at outgoing
        
        for (sp = remote + strlen(remote) - 1; (sp >= remote) && (isdigit((int) *sp)); sp--)
        {
        }

        sp++;
        char *remoteport = sp;

        // Now look for the specific vital signs to count frequencies
            
        for (i = 0; i < ATTR; i++)
        {
            if (strcmp(localport, ECGSOCKS[i].portnr) == 0)
            {
                cf_this[ECGSOCKS[i].in]++;
                AppendItem(&in[i], vbuff, "");

            }

            if (strcmp(remoteport, ECGSOCKS[i].portnr) == 0)
            {
                cf_this[ECGSOCKS[i].out]++;
                AppendItem(&out[i], vbuff, "");

            }
        }
    }

    cf_pclose(pp);

/* Now save the state for ShowState() 
   the state is not smaller than the last or at least 40 minutes
   older. This mirrors the persistence of the maxima classes */

    for (i = 0; i < ATTR; i++)
    {
        struct stat statbuf;
        time_t now = time(NULL);

        CfDebug("save incoming %s\n", ECGSOCKS[i].name);
        snprintf(vbuff, CF_MAXVARSIZE, "%s/state/cf_incoming.%s", CFWORKDIR, ECGSOCKS[i].name);
        if (cfstat(vbuff, &statbuf) != -1)
        {
            if ((ByteSizeList(in[i]) < statbuf.st_size) && (now < statbuf.st_mtime + 40 * 60))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "New state %s is smaller, retaining old for 40 mins longer\n", ECGSOCKS[i].name);
                DeleteItemList(in[i]);
                continue;
            }
        }

        SetNetworkEntropyClasses(CanonifyName(ECGSOCKS[i].name), "in", in[i]);
        RawSaveItemList(in[i], vbuff);
        DeleteItemList(in[i]);
        CfDebug("Saved in netstat data in %s\n", vbuff);
    }

    for (i = 0; i < ATTR; i++)
    {
        struct stat statbuf;
        time_t now = time(NULL);

        CfDebug("save outgoing %s\n", ECGSOCKS[i].name);
        snprintf(vbuff, CF_MAXVARSIZE, "%s/state/cf_outgoing.%s", CFWORKDIR, ECGSOCKS[i].name);

        if (cfstat(vbuff, &statbuf) != -1)
        {
            if ((ByteSizeList(out[i]) < statbuf.st_size) && (now < statbuf.st_mtime + 40 * 60))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "New state %s is smaller, retaining old for 40 mins longer\n", ECGSOCKS[i].name);
                DeleteItemList(out[i]);
                continue;
            }
        }

        SetNetworkEntropyClasses(CanonifyName(ECGSOCKS[i].name), "out", out[i]);
        RawSaveItemList(out[i], vbuff);
        CfDebug("Saved out netstat data in %s\n", vbuff);
        DeleteItemList(out[i]);
    }
}
