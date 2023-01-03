/*
  Copyright 2023 Northern.tech AS

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

#include <mon.h>
#include <systype.h>
#include <item_lib.h>
#include <files_names.h>
#include <files_interfaces.h>
#include <files_lib.h>
#include <file_lib.h> // SetUmask()
#include <pipes.h>
#include <known_dirs.h>

#ifdef __linux__
#include <proc_net_parsing.h>
#endif

/* Globals */

Item *ALL_INCOMING = NULL;
Item *MON_UDP4 = NULL, *MON_UDP6 = NULL, *MON_TCP4 = NULL, *MON_TCP6 = NULL;

typedef enum {
    cfn_udp4 = 0,
    cfn_udp6,
    cfn_tcp4,
    cfn_tcp6,
    cfn_unknown,
} SocketType;

/*******************************************************************/
/* Anomaly                                                         */
/*******************************************************************/

typedef struct
{
    uint32_t port;
    char *portnr;
    char *name;
    enum observables in;
    enum observables out;
} Sock;

static const Sock ECGSOCKS[] =     /* extended to map old to new using enum */
{
    {137, "137", "netbiosns", ob_netbiosns_in, ob_netbiosns_out},
    {138, "138", "netbiosdgm", ob_netbiosdgm_in, ob_netbiosdgm_out},
    {139, "139", "netbiosssn", ob_netbiosssn_in, ob_netbiosssn_out},
    {445, "445", "microsoft_ds", ob_microsoft_ds_in, ob_microsoft_ds_out},
    {5308, "5308", "cfengine", ob_cfengine_in, ob_cfengine_out},
    {2049, "2049", "nfsd", ob_nfsd_in, ob_nfsd_out},
    {25, "25", "smtp", ob_smtp_in, ob_smtp_out},
    {80, "80", "www", ob_www_in, ob_www_out},
    {8080, "8080", "www-alt", ob_www_alt_in, ob_www_alt_out},
    {21, "21", "ftp", ob_ftp_in, ob_ftp_out},
    {22, "22", "ssh", ob_ssh_in, ob_ssh_out},
    {433, "443", "wwws", ob_wwws_in, ob_wwws_out},
    {143, "143", "imap", ob_imap_in, ob_imap_out},
    {993, "993", "imaps", ob_imaps_in, ob_imaps_out},
    {389, "389", "ldap", ob_ldap_in, ob_ldap_out},
    {636, "636", "ldaps", ob_ldaps_in, ob_ldaps_out},
    {27017, "27017", "mongo", ob_mongo_in, ob_mongo_out},
    {3306, "3306", "mysql", ob_mysql_in, ob_mysql_out},
    {5432, "5432", "postgresql", ob_postgresql_in, ob_postgresql_out},
    {631, "631", "ipp", ob_ipp_in, ob_ipp_out},
};
#define ATTR (sizeof(ECGSOCKS) / sizeof(ECGSOCKS[0]))

static const char *const VNETSTAT[] =
{
    [PLATFORM_CONTEXT_UNKNOWN] = "-",
    [PLATFORM_CONTEXT_OPENVZ] = "/bin/netstat",          /* virt_host_vz_vzps */
    [PLATFORM_CONTEXT_HP] = "/usr/bin/netstat",          /* hpux */
    [PLATFORM_CONTEXT_AIX] = "/usr/bin/netstat",         /* aix */
    [PLATFORM_CONTEXT_LINUX] = "/bin/netstat",           /* linux */
    [PLATFORM_CONTEXT_BUSYBOX] = "/bin/netstat",         /* linux */
    [PLATFORM_CONTEXT_SOLARIS] = "/usr/bin/netstat",     /* solaris */
    [PLATFORM_CONTEXT_SUN_SOLARIS] = "/usr/bin/netstat", /* solaris */
    [PLATFORM_CONTEXT_FREEBSD] = "/usr/bin/netstat",     /* freebsd */
    [PLATFORM_CONTEXT_NETBSD] = "/usr/bin/netstat",      /* netbsd */
    [PLATFORM_CONTEXT_CRAYOS] = "/usr/ucb/netstat",      /* cray */
    [PLATFORM_CONTEXT_WINDOWS_NT] = "/cygdrive/c/WINNT/System32/netstat", /* CygWin */
    [PLATFORM_CONTEXT_SYSTEMV] = "/usr/bin/netstat",     /* Unixware */
    [PLATFORM_CONTEXT_OPENBSD] = "/usr/bin/netstat",     /* openbsd */
    [PLATFORM_CONTEXT_CFSCO] = "/usr/bin/netstat",       /* sco */
    [PLATFORM_CONTEXT_DARWIN] = "/usr/sbin/netstat",     /* darwin */
    [PLATFORM_CONTEXT_QNX] = "/usr/bin/netstat",         /* qnx */
    [PLATFORM_CONTEXT_DRAGONFLY] = "/usr/bin/netstat",   /* dragonfly */
    [PLATFORM_CONTEXT_MINGW] = "mingw-invalid",              /* mingw */
    [PLATFORM_CONTEXT_VMWARE] = "/usr/bin/netstat",          /* vmware */
    [PLATFORM_CONTEXT_ANDROID] = "/system/xbin/netstat", /* android */
};

/* Implementation */

void MonNetworkInit(void)
{

    DeleteItemList(MON_TCP4);
    DeleteItemList(MON_TCP6);
    DeleteItemList(MON_UDP4);
    DeleteItemList(MON_UDP6);

    MON_UDP4 = MON_UDP6 = MON_TCP4 = MON_TCP6 = NULL;

    char vbuff[CF_BUFSIZE];
    const char* const statedir = GetStateDir();

    const char* const file_stems[] = { "cf_incoming", "cf_outgoing" };
    const size_t num_files = sizeof(file_stems) / sizeof(char*);

    for (size_t i = 0; i < ATTR; i++)
    {
        for (size_t j = 0; j < num_files; j++)
        {
            snprintf(vbuff, CF_BUFSIZE, "%s/%s.%s",
                     statedir, file_stems[j], ECGSOCKS[i].name);

            MapName(vbuff);
            CreateEmptyFile(vbuff);
        }
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

static void SaveNetworkData(Item * const *in, Item * const *out);
static void GetNetworkDataFromNetstat(FILE *fp, double *cf_this, Item **in, Item **out);
#ifdef __linux__
static bool GetNetworkDataFromProcNet(double *cf_this, Item **in, Item **out);
#endif

static inline void ResetNetworkData()
{
    DeleteItemList(ALL_INCOMING);
    ALL_INCOMING = NULL;

    DeleteItemList(MON_TCP4);
    DeleteItemList(MON_TCP6);
    DeleteItemList(MON_UDP4);
    DeleteItemList(MON_UDP6);
    MON_UDP4 = MON_UDP6 = MON_TCP4 = MON_TCP6 = NULL;
}

void MonNetworkGatherData(double *cf_this)
{
    ResetNetworkData();

    Item *in[ATTR] = {0};
    Item *out[ATTR] = {0};

#ifdef __linux__
    /* On Linux, prefer parsing data from /proc/net with our custom code (more
     * efficient), but fall back to netstat if proc parsing fails. */
    if ((access("/proc/net/tcp", R_OK) != 0) || !GetNetworkDataFromProcNet(cf_this, in, out))
#endif
    {
        char comm[PATH_MAX + 4] = {0}; /* path to the binary + " -an" */
        strncpy(comm, VNETSTAT[VSYSTEMHARDCLASS], (sizeof(comm) - 1));

        if (!FileCanOpen(comm, "r"))
        {
            Log(LOG_LEVEL_VERBOSE,
                "Cannot open '%s', aborting gathering of network data (monitoring)",
                comm);
            return;
        }

        strncat(comm, " -an", sizeof(comm) - 1);

        FILE *pp;
        if ((pp = cf_popen(comm, "r", true)) == NULL)
        {
            Log(LOG_LEVEL_VERBOSE,
                "Opening '%s' failed, aborting gathering of network data (monitoring)",
                comm);
            return;
        }

        GetNetworkDataFromNetstat(pp, cf_this, in, out);
        cf_pclose(pp);
    }

    /* Now save the state for ShowState()
       the state is not smaller than the last or at least 40 minutes
       older. This mirrors the persistence of the maxima classes */
    SaveNetworkData(in, out);
}

#ifdef __linux__
static inline void SaveSocketInfo(const char *local_addr,
                                  uint32_t local_port,
                                  uint32_t remote_port,
                                  SocketState state,
                                  SocketType type,
                                  const char *socket_info,
                                  double *cf_this,
                                  Item **in, Item **out)
{
    Log(LOG_LEVEL_DEBUG, "Saving socket info '%s:%d:%d [%d, %d]",
        local_addr, local_port, remote_port, state, type);

    if (state == SOCK_STATE_LISTEN)
    {
        char port_str[CF_MAX_PORT_LEN];
        snprintf(port_str, sizeof(port_str), "%d", local_port);

        IdempPrependItem(&ALL_INCOMING, port_str, NULL);

        switch (type)
        {
        case cfn_tcp4:
            IdempPrependItem(&MON_TCP4, port_str, local_addr);
            break;
        case cfn_tcp6:
            IdempPrependItem(&MON_TCP6, port_str, local_addr);
            break;
        case cfn_udp4:
            IdempPrependItem(&MON_UDP4, port_str, local_addr);
            break;
        case cfn_udp6:
            IdempPrependItem(&MON_UDP6, port_str, local_addr);
            break;
        default:
            debug_abort_if_reached();
            break;
        }
    }
    for (size_t i = 0; i < ATTR; i++)
    {
        if (local_port == ECGSOCKS[i].port)
        {
            cf_this[ECGSOCKS[i].in]++;
            AppendItem(&in[i], socket_info, "");

        }

        if (remote_port == ECGSOCKS[i].port)
        {
            cf_this[ECGSOCKS[i].out]++;
            AppendItem(&out[i], socket_info, "");

        }
    }
}

static inline bool GetNetworkDataFromProcNetTCP(char local_addr[INET_ADDRSTRLEN],
                                                char remote_addr[INET_ADDRSTRLEN],
                                                char **buff, size_t *buff_size, Seq *lines,
                                                double *cf_this, Item **in, Item **out)
{
    FILE *fp = fopen("/proc/net/tcp", "r");
    if (fp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to open /proc/net/tcp for reading");
        return false;
    }
    /* Read the header */
    ssize_t ret = CfReadLine(buff, buff_size, fp);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/tcp");
        fclose(fp);
        return false;
    }

    /* Read real data */
    ret = CfReadLines(buff, buff_size, fp, lines);
    if (ret < 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/tcp");
        fclose(fp);
        return false;
    }
    Log(LOG_LEVEL_VERBOSE, "Read %zu lines from /proc/net/tcp", ret);

    uint32_t l_port, r_port;
    SocketState state;
    for (size_t i = 0; i < (size_t) ret; i++)
    {
        char *line = SeqAt(lines,i);
        if (ParseIPv4SocketInfo(line, local_addr, &l_port, remote_addr, &r_port, &state))
        {
            SaveSocketInfo(local_addr, l_port, r_port,
                           state, cfn_tcp4,
                           line, cf_this, in, out);
        }
    }
    fclose(fp);
    SeqClear(lines);
    return true;
}

static inline bool GetNetworkDataFromProcNetTCP6(char local_addr6[INET6_ADDRSTRLEN],
                                                 char remote_addr6[INET6_ADDRSTRLEN],
                                                 char **buff, size_t *buff_size, Seq *lines,
                                                 double *cf_this, Item **in, Item **out)
{
    FILE *fp = fopen("/proc/net/tcp6", "r");
    if (fp == NULL)
    {
        /* IPv6 may be completely disabled in kernel so this should be handled gracefully. */
        Log(LOG_LEVEL_VERBOSE, "Failed to read data from /proc/net/tcp6");
        return true;
    }
    ssize_t ret = CfReadLine(buff, buff_size, fp);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/tcp6");
        fclose(fp);
        return false;
    }

    /* Read real data */
    ret = CfReadLines(buff, buff_size, fp, lines);
    if (ret < 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/tcp6");
        fclose(fp);
        return false;
    }
    Log(LOG_LEVEL_VERBOSE, "Read %zu lines from /proc/net/tcp6", ret);

    uint32_t l_port, r_port;
    SocketState state;
    for (size_t i = 0; i < (size_t) ret; i++)
    {
        char *line = SeqAt(lines,i);
        if (ParseIPv6SocketInfo(line, local_addr6, &l_port, remote_addr6, &r_port, &state))
        {
            SaveSocketInfo(local_addr6, l_port, r_port,
                           state, cfn_tcp6,
                           line, cf_this, in, out);
        }
    }
    fclose(fp);
    SeqClear(lines);
    return true;
}

static inline bool GetNetworkDataFromProcNetUDP(char local_addr[INET_ADDRSTRLEN],
                                                char remote_addr[INET_ADDRSTRLEN],
                                                char **buff, size_t *buff_size, Seq *lines,
                                                double *cf_this, Item **in, Item **out)
{
    FILE *fp = fopen("/proc/net/udp", "r");
    if (fp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Failed to open /proc/net/udp for reading");
        return false;
    }
    /* Read the header */
    ssize_t ret = CfReadLine(buff, buff_size, fp);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/udp");
        fclose(fp);
        return false;
    }

    /* Read real data */
    ret = CfReadLines(buff, buff_size, fp, lines);
    if (ret < 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/udp");
        fclose(fp);
        return false;
    }
    Log(LOG_LEVEL_VERBOSE, "Read %zu lines from /proc/net/udp", ret);

    uint32_t l_port, r_port;
    SocketState state;
    for (size_t i = 0; i < (size_t) ret; i++)
    {
        char *line = SeqAt(lines,i);
        if (ParseIPv4SocketInfo(line, local_addr, &l_port, remote_addr, &r_port, &state))
        {
            SaveSocketInfo(local_addr, l_port, r_port,
                           state, cfn_udp4,
                           line, cf_this, in, out);
        }
    }
    fclose(fp);
    SeqClear(lines);
    return true;
}

static inline bool GetNetworkDataFromProcNetUDP6(char local_addr6[INET6_ADDRSTRLEN],
                                                 char remote_addr6[INET6_ADDRSTRLEN],
                                                 char **buff, size_t *buff_size, Seq *lines,
                                                 double *cf_this, Item **in, Item **out)
{
    FILE *fp = fopen("/proc/net/udp6", "r");
    if (fp == NULL)
    {
        /* IPv6 may be completely disabled in kernel so this should be handled gracefully. */
        Log(LOG_LEVEL_VERBOSE, "Failed to read data from /proc/net/udp6");
        return true;
    }
    ssize_t ret = CfReadLine(buff, buff_size, fp);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/udp6");
        fclose(fp);
        return false;
    }

    /* Read real data */
    ret = CfReadLines(buff, buff_size, fp, lines);
    if (ret < 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to read data from /proc/net/udp6");
        fclose(fp);
        return false;
    }
    Log(LOG_LEVEL_VERBOSE, "Read %zu lines from /proc/net/udp6", ret);

    uint32_t l_port, r_port;
    SocketState state;
    for (size_t i = 0; i < (size_t) ret; i++)
    {
        char *line = SeqAt(lines,i);
        if (ParseIPv6SocketInfo(line, local_addr6, &l_port, remote_addr6, &r_port, &state))
        {
            SaveSocketInfo(local_addr6, l_port, r_port,
                           state, cfn_udp6,
                           line, cf_this, in, out);
        }
    }
    fclose(fp);
    SeqClear(lines);
    return true;
}

static bool GetNetworkDataFromProcNet(double *cf_this, Item **in, Item **out)
{
    bool result = true;
    Seq *lines = SeqNew(64, free);

    size_t buff_size = 256;
    char *buff = xmalloc(buff_size);

    {
        char local_addr[INET_ADDRSTRLEN];
        char remote_addr[INET_ADDRSTRLEN];
        if (!GetNetworkDataFromProcNetTCP(local_addr, remote_addr,
                                          &buff, &buff_size, lines,
                                          cf_this, in, out))
        {
            SeqDestroy(lines);
            free(buff);
            return false;
        }
        SeqClear(lines);
        if (!GetNetworkDataFromProcNetUDP(local_addr, remote_addr,
                                          &buff, &buff_size, lines,
                                          cf_this, in, out))
        {
            SeqDestroy(lines);
            free(buff);
            return false;
        }
        SeqClear(lines);
    }

    {
        char local_addr6[INET6_ADDRSTRLEN];
        char remote_addr6[INET6_ADDRSTRLEN];
        if (!GetNetworkDataFromProcNetTCP6(local_addr6, remote_addr6,
                                           &buff, &buff_size, lines,
                                           cf_this, in, out))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to get IPv6 TCP sockets information");
        }
        SeqClear(lines);
        if (!GetNetworkDataFromProcNetUDP6(local_addr6, remote_addr6,
                                           &buff, &buff_size, lines,
                                           cf_this, in, out))
        {
            Log(LOG_LEVEL_VERBOSE, "Failed to get IPv6 UDP sockets information");
        }
    }

    SeqDestroy(lines);
    free(buff);
    return result;
}
#endif  /* __linux__ */

static void GetNetworkDataFromNetstat(FILE *fp, double *cf_this, Item **in, Item **out)
{
    enum cf_netstat_type { cfn_new, cfn_old } type = cfn_new;
    SocketType packet = cfn_tcp4;

    size_t vbuff_size = CF_BUFSIZE;
    char *vbuff = xmalloc(vbuff_size);
    for (;;)
    {
        char local[CF_MAX_IP_LEN + CF_MAX_PORT_LEN] = {0};
        char remote[CF_MAX_IP_LEN + CF_MAX_PORT_LEN] = {0};

        ssize_t res = CfReadLine(&vbuff, &vbuff_size, fp);
        if (res == -1)
        {
            if (!feof(fp))
            {
                Log(LOG_LEVEL_DEBUG,
                    "Error occured while reading data from 'netstat' "
                    "(CfReadLine/getline returned -1 but no EOF found)");
                free(vbuff);
                return;
            }
            else
            {
                break;
            }
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

        char *sp;
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

        for (size_t i = 0; i < ATTR; i++)
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
    free(vbuff);
}

static void SaveNetworkData(Item * const *in, Item * const *out)
{
    const char* const statedir = GetStateDir();

    char vbuff[CF_BUFSIZE];
    for (size_t i = 0; i < ATTR; i++)
    {
        struct stat statbuf;
        time_t now = time(NULL);

        Log(LOG_LEVEL_DEBUG, "save incoming '%s'", ECGSOCKS[i].name);

        snprintf(vbuff, CF_MAXVARSIZE, "%s%ccf_incoming.%s", statedir, FILE_SEPARATOR, ECGSOCKS[i].name);

        if (stat(vbuff, &statbuf) != -1)
        {
            if (ItemListSize(in[i]) < (size_t) statbuf.st_size &&
                now < statbuf.st_mtime + 40 * 60)
            {
                Log(LOG_LEVEL_VERBOSE, "New state '%s' is smaller, retaining old for 40 mins longer", ECGSOCKS[i].name);
                DeleteItemList(in[i]);
                continue;
            }
        }

        SetNetworkEntropyClasses(CanonifyName(ECGSOCKS[i].name), "in", in[i]);
        const mode_t old_umask = SetUmask(0077);
        RawSaveItemList(in[i], vbuff, NewLineMode_Unix);
        RestoreUmask(old_umask);
        DeleteItemList(in[i]);
        Log(LOG_LEVEL_DEBUG, "Saved in netstat data in '%s'", vbuff);
    }

    for (size_t i = 0; i < ATTR; i++)
    {
        struct stat statbuf;
        time_t now = time(NULL);

        Log(LOG_LEVEL_DEBUG, "save outgoing '%s'", ECGSOCKS[i].name);
        snprintf(vbuff, CF_MAXVARSIZE, "%s%ccf_outgoing.%s", statedir, FILE_SEPARATOR, ECGSOCKS[i].name);

        if (stat(vbuff, &statbuf) != -1)
        {
            if (ItemListSize(out[i]) < (size_t) statbuf.st_size &&
                now < statbuf.st_mtime + 40 * 60)
            {
                Log(LOG_LEVEL_VERBOSE, "New state '%s' is smaller, retaining old for 40 mins longer", ECGSOCKS[i].name);
                DeleteItemList(out[i]);
                continue;
            }
        }

        SetNetworkEntropyClasses(CanonifyName(ECGSOCKS[i].name), "out", out[i]);
        const mode_t old_umask = SetUmask(0077);
        RawSaveItemList(out[i], vbuff, NewLineMode_Unix);
        RestoreUmask(old_umask);
        Log(LOG_LEVEL_DEBUG, "Saved out netstat data in '%s'", vbuff);
        DeleteItemList(out[i]);
    }
}
