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

#include <cf3.defs.h>

#include <mon.h>
#include <item_lib.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <files_lib.h>
#include <pipes.h>
#include <ports.h>

/* Globals */

Item *ALL_INCOMING = NULL;
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

typedef struct
{
    double *cf_this;
    Item **in;
    Item **out;
} mon_network_port_processor_callback_context;

// this callback uses global variables
void MonNetworkPortProcessorFn (ARG_UNUSED const cf_netstat_type netstat_type, const cf_packet_type packet_type, const cf_port_state port_state,
                                const char *netstat_line,
                                const char *local_addr, const char *local_port,
                                ARG_UNUSED const char *remote_addr, const char *remote_port,
                                void *callback_context)
{
    mon_network_port_processor_callback_context *ctx = callback_context;
    double *cf_this = ctx->cf_this;
    Item **in = ctx->in;
    Item **out = ctx->out;

    if (cfn_listen == port_state)
    {
        // General bucket

        IdempPrependItem(&ALL_INCOMING, local_port, NULL);

        // Categories the incoming ports by packet types

        switch (packet_type)
        {
        case cfn_udp4:
            IdempPrependItem(&MON_UDP4, local_port, local_addr);
            break;
        case cfn_udp6:
            IdempPrependItem(&MON_UDP6, local_port, local_addr);
            break;
        case cfn_tcp4:
            IdempPrependItem(&MON_TCP4, local_port, local_addr);
            break;
        case cfn_tcp6:
            IdempPrependItem(&MON_TCP6, local_port, local_addr);
            break;
        default:
            break;
        }
    }


    // Now look for the specific vital signs to count frequencies
            
    for (int i = 0; i < ATTR; i++)
    {
        if (strcmp(local_port, ECGSOCKS[i].portnr) == 0)
        {
            cf_this[ECGSOCKS[i].in]++;
            AppendItem(&in[i], netstat_line, "");
        }

        if (strcmp(remote_port, ECGSOCKS[i].portnr) == 0)
        {
            cf_this[ECGSOCKS[i].out]++;
            AppendItem(&out[i], netstat_line, "");
        }
    }
}

void MonNetworkGatherData(double *cf_this)
{
    Item *in[ATTR], *out[ATTR];
    int i;
    char filename_buffer[CF_BUFSIZE];
    mon_network_port_processor_callback_context ctx = { cf_this, in, out };

    for (i = 0; i < ATTR; i++)
    {
        in[i] = out[i] = NULL;
    }

    DeleteItemList(ALL_INCOMING);
    ALL_INCOMING = NULL;

    PortsFindListening(VSYSTEMHARDCLASS, &MonNetworkPortProcessorFn, &ctx);

/* Now save the state for ShowState() 
   the state is not smaller than the last or at least 40 minutes
   older. This mirrors the persistence of the maxima classes */

    for (i = 0; i < ATTR; i++)
    {
        struct stat statbuf;
        time_t now = time(NULL);

        Log(LOG_LEVEL_DEBUG, "save incoming '%s'", ECGSOCKS[i].name);
        snprintf(filename_buffer, CF_MAXVARSIZE, "%s/state/cf_incoming.%s", CFWORKDIR, ECGSOCKS[i].name);
        if (stat(filename_buffer, &statbuf) != -1)
        {
            if ((ByteSizeList(in[i]) < statbuf.st_size) && (now < statbuf.st_mtime + 40 * 60))
            {
                Log(LOG_LEVEL_VERBOSE, "New state '%s' is smaller, retaining old for 40 mins longer", ECGSOCKS[i].name);
                DeleteItemList(in[i]);
                continue;
            }
        }

        SetNetworkEntropyClasses(CanonifyName(ECGSOCKS[i].name), "in", in[i]);
        RawSaveItemList(in[i], filename_buffer);
        DeleteItemList(in[i]);
        Log(LOG_LEVEL_DEBUG, "Saved in netstat data in '%s'", filename_buffer);
    }

    for (i = 0; i < ATTR; i++)
    {
        struct stat statbuf;
        time_t now = time(NULL);

        Log(LOG_LEVEL_DEBUG, "save outgoing '%s'", ECGSOCKS[i].name);
        snprintf(filename_buffer, CF_MAXVARSIZE, "%s/state/cf_outgoing.%s", CFWORKDIR, ECGSOCKS[i].name);

        if (stat(filename_buffer, &statbuf) != -1)
        {
            if ((ByteSizeList(out[i]) < statbuf.st_size) && (now < statbuf.st_mtime + 40 * 60))
            {
                Log(LOG_LEVEL_VERBOSE, "New state '%s' is smaller, retaining old for 40 mins longer", ECGSOCKS[i].name);
                DeleteItemList(out[i]);
                continue;
            }
        }

        SetNetworkEntropyClasses(CanonifyName(ECGSOCKS[i].name), "out", out[i]);
        RawSaveItemList(out[i], filename_buffer);
        Log(LOG_LEVEL_DEBUG, "Saved out netstat data in '%s'", filename_buffer);
        DeleteItemList(out[i]);
    }
}
