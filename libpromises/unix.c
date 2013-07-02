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
  versions of CFEngine, the applicable Commerical Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include "unix.h"

#include "env_context.h"
#include "vars.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "item_lib.h"
#include "conversion.h"
#include "matching.h"
#include "communication.h"
#include "pipes.h"
#include "exec_tools.h"
#include "misc_lib.h"
#include "rlist.h"
#include "scope.h"

#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#ifdef HAVE_SYS_JAIL_H
# include <sys/jail.h>
#endif

#ifdef HAVE_GETIFADDRS
# include <ifaddrs.h>
# ifdef HAVE_NET_IF_DL_H
#   include <net/if_dl.h>
# endif
#endif

#define CF_IFREQ 2048           /* Reportedly the largest size that does not segfault 32/64 bit */
#define CF_IGNORE_INTERFACES "ignore_interfaces.rx"

#ifndef __MINGW32__

# ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
#  ifdef _SIZEOF_ADDR_IFREQ
#   define SIZEOF_IFREQ(x) _SIZEOF_ADDR_IFREQ(x)
#  else
#   define SIZEOF_IFREQ(x) \
          ((x).ifr_addr.sa_len > sizeof(struct sockaddr) ? \
           (sizeof(struct ifreq) - sizeof(struct sockaddr) + \
            (x).ifr_addr.sa_len) : sizeof(struct ifreq))
#  endif
# else
#  define SIZEOF_IFREQ(x) sizeof(struct ifreq)
# endif


static bool IsProcessRunning(pid_t pid);
static void FindV6InterfacesInfo(EvalContext *ctx);
static bool IgnoreJailInterface(int ifaceidx, struct sockaddr_in *inaddr);
static bool IgnoreInterface(char *name);
static void InitIgnoreInterfaces(void);

static Rlist *IGNORE_INTERFACES = NULL;


void ProcessSignalTerminate(pid_t pid)
{
    if(!IsProcessRunning(pid))
    {
        return;
    }


    if(kill(pid, SIGINT) == -1)
    {
        Log(LOG_LEVEL_ERR, "Could not send SIGINT to pid '%" PRIdMAX "'. (kill: %s)",
            (intmax_t)pid, GetErrorStr());
    }

    sleep(1);


    if(kill(pid, SIGTERM) == -1)
    {
        Log(LOG_LEVEL_ERR, "Could not send SIGTERM to pid '%" PRIdMAX "'. (kill: %s)",
            (intmax_t)pid, GetErrorStr());
    }

    sleep(5);


    if(kill(pid, SIGKILL) == -1)
    {
        Log(LOG_LEVEL_ERR, "Could not send SIGKILL to pid '%" PRIdMAX "'. (kill: %s)",
            (intmax_t)pid, GetErrorStr());
    }

    sleep(1);
}

/*************************************************************/

static bool IsProcessRunning(pid_t pid)
{
    int res = kill(pid, 0);

    if(res == 0)
    {
        return true;
    }

    if(res == -1 && errno == ESRCH)
    {
        return false;
    }

    Log(LOG_LEVEL_ERR, "Failed checking for process existence. (kill: %s)", GetErrorStr());

    return false;
}

/*************************************************************/

int GetCurrentUserName(char *userName, int userNameLen)
{
    struct passwd *user_ptr;

    memset(userName, 0, userNameLen);
    user_ptr = getpwuid(getuid());

    if (user_ptr == NULL)
    {
        Log(LOG_LEVEL_ERR, "Could not get user name of current process, using 'UNKNOWN'. (getpwuid: %s)", GetErrorStr());
        strncpy(userName, "UNKNOWN", userNameLen - 1);
        return false;
    }

    strncpy(userName, user_ptr->pw_name, userNameLen - 1);
    return true;
}

/*************************************************************/

int IsExecutable(const char *file)
{
    struct stat sb;
    gid_t grps[NGROUPS];
    int n;

    if (stat(file, &sb) == -1)
    {
        Log(LOG_LEVEL_ERR, "Proposed executable file '%s' doesn't exist", file);
        return false;
    }

    if (sb.st_mode & 02)
    {
        Log(LOG_LEVEL_ERR, "SECURITY ALERT: promised executable '%s' is world writable! ", file);
        Log(LOG_LEVEL_ERR, "SECURITY ALERT: CFEngine will not execute this - requires human inspection");
        return false;
    }

    if ((getuid() == sb.st_uid) || (getuid() == 0))
    {
        if (sb.st_mode & 0100)
        {
            return true;
        }
    }
    else if (getgid() == sb.st_gid)
    {
        if (sb.st_mode & 0010)
        {
            return true;
        }
    }
    else
    {
        if (sb.st_mode & 0001)
        {
            return true;
        }

        if ((n = getgroups(NGROUPS, grps)) > 0)
        {
            int i;

            for (i = 0; i < n; i++)
            {
                if (grps[i] == sb.st_gid)
                {
                    if (sb.st_mode & 0010)
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool ShellCommandReturnsZero(const char *command, ShellType shell)
{
    int status;
    pid_t pid;

    if (shell == SHELL_TYPE_POWERSHELL)
    {
        Log(LOG_LEVEL_ERR, "Powershell is only supported on Windows");
        return false;
    }

    if ((pid = fork()) < 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to fork new process: %s", command);
        return false;
    }
    else if (pid == 0)          /* child */
    {
        ALARM_PID = -1;

        if (shell == SHELL_TYPE_USE)
        {
            if (execl(SHELL_PATH, "sh", "-c", command, NULL) == -1)
            {
                Log(LOG_LEVEL_ERR, "Command '%s' failed. (execl: %s)", command, GetErrorStr());
                exit(1);
            }
        }
        else
        {
            char **argv = ArgSplitCommand(command);

            if (execv(argv[0], argv) == -1)
            {
                Log(LOG_LEVEL_ERR, "Command '%s' failed. (execv: %s)", argv[0], GetErrorStr());
                exit(1);
            }
        }
    }
    else                        /* parent */
    {
        ALARM_PID = pid;

        while (waitpid(pid, &status, 0) < 0)
        {
            if (errno != EINTR)
            {
                return -1;
            }
        }

        return (WEXITSTATUS(status) == 0);
    }

    return false;
}

/*********************************************************************/


/******************************************************************/

static bool IgnoreJailInterface(
#if !defined(HAVE_JAIL_GET)
    ARG_UNUSED int ifaceidx, ARG_UNUSED struct sockaddr_in *inaddr
#else
    int ifaceidx, struct sockaddr_in *inaddr
#endif
    )
{
/* FreeBSD jails */
# ifdef HAVE_JAIL_GET
    struct iovec fbsd_jparams[4];
    struct in_addr fbsd_jia;
    int fbsd_lastjid = 0;

    *(const void **) &fbsd_jparams[0].iov_base = "lastjid";
    fbsd_jparams[0].iov_len = sizeof("lastjid");
    fbsd_jparams[1].iov_base = &fbsd_lastjid;
    fbsd_jparams[1].iov_len = sizeof(fbsd_lastjid);

    *(const void **) &fbsd_jparams[2].iov_base = "ip4.addr";
    fbsd_jparams[2].iov_len = sizeof("ip4.addr");
    fbsd_jparams[3].iov_len = sizeof(struct in_addr);
    fbsd_jparams[3].iov_base = &fbsd_jia;

    while ((fbsd_lastjid = jail_get(fbsd_jparams, 4, 0)) > 0)
    {
        if (fbsd_jia.s_addr == inaddr->sin_addr.s_addr)
        {
            Log(LOG_LEVEL_VERBOSE, "Interface %d belongs to a FreeBSD jail %s", ifaceidx, inet_ntoa(fbsd_jia));
            return true;
        }
    }
# endif

    return false;
}

/******************************************************************/

static void GetMacAddress(EvalContext *ctx, AgentType ag, int fd, struct ifreq *ifr, struct ifreq *ifp, Rlist **interfaces,
                          Rlist **hardware)
{
    char name[CF_MAXVARSIZE];

    if (ag != AGENT_TYPE_GENDOC)
    {
        snprintf(name, sizeof(name), "hardware_mac[%s]", ifp->ifr_name);
    }
    else
    {
        snprintf(name, sizeof(name), "hardware_mac[interface_name]");
    }

    // mac address on a loopback interface doesn't make sense
    if (ifr->ifr_flags & IFF_LOOPBACK)
    {
      return;
    }

# if defined(SIOCGIFHWADDR) && defined(HAVE_STRUCT_IFREQ_IFR_HWADDR)
    char hw_mac[CF_MAXVARSIZE];

    if ((ioctl(fd, SIOCGIFHWADDR, ifr) == -1))
    {
        Log(LOG_LEVEL_ERR, "Couldn't get mac address for '%s' interface. (ioctl: %s)", ifr->ifr_name, GetErrorStr());
        return;
    }
      
    snprintf(hw_mac, sizeof(hw_mac), "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
             (unsigned char) ifr->ifr_hwaddr.sa_data[0],
             (unsigned char) ifr->ifr_hwaddr.sa_data[1],
             (unsigned char) ifr->ifr_hwaddr.sa_data[2],
             (unsigned char) ifr->ifr_hwaddr.sa_data[3],
             (unsigned char) ifr->ifr_hwaddr.sa_data[4], 
             (unsigned char) ifr->ifr_hwaddr.sa_data[5]);

    ScopeNewSpecial(ctx, "sys", name, hw_mac, DATA_TYPE_STRING);
    RlistAppend(hardware, hw_mac, RVAL_TYPE_SCALAR);
    RlistAppend(interfaces, ifp->ifr_name, RVAL_TYPE_SCALAR);

    snprintf(name, sizeof(name), "mac_%s", CanonifyName(hw_mac));
    EvalContextHeapAddHard(ctx, name);

# elif defined(HAVE_GETIFADDRS)
    char hw_mac[CF_MAXVARSIZE];
    char *m;
    struct ifaddrs *ifaddr, *ifa;
    struct sockaddr_dl *sdl;

    if (getifaddrs(&ifaddr) == -1)
    {
        Log(LOG_LEVEL_ERR, "getifaddrs", "!! Could not get interface %s addresses",
          ifp->ifr_name);

        ScopeNewSpecial(ctx, "sys", name, "mac_unknown", DATA_TYPE_STRING);
        EvalContextHeapAddHard(ctx, "mac_unknown");
        return;
    }
    for (ifa = ifaddr; ifa != NULL; ifa=ifa->ifa_next)
    {
        if ( strcmp(ifa->ifa_name, ifp->ifr_name) == 0) 
        {
            if (ifa->ifa_addr->sa_family == AF_LINK) 
            {
                sdl = (struct sockaddr_dl *)ifa->ifa_addr;
                m = (char *) LLADDR(sdl);
                
                snprintf(hw_mac, sizeof(hw_mac), "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
                    (unsigned char) m[0],
                    (unsigned char) m[1],
                    (unsigned char) m[2],
                    (unsigned char) m[3],
                    (unsigned char) m[4],
                    (unsigned char) m[5]);

                ScopeNewSpecial(ctx, "sys", name, hw_mac, DATA_TYPE_STRING);
                RlistAppend(hardware, hw_mac, RVAL_TYPE_SCALAR);
                RlistAppend(interfaces, ifa->ifa_name, RVAL_TYPE_SCALAR);

                snprintf(name, sizeof(name), "mac_%s", CanonifyName(hw_mac));
                EvalContextHeapAddHard(ctx, name);
            }
        }

    }
    freeifaddrs(ifaddr);

# else
    ScopeNewSpecial(ctx, "sys", name, "mac_unknown", DATA_TYPE_STRING);
    EvalContextHeapAddHard(ctx, "mac_unknown");
# endif
}

/******************************************************************/

void GetInterfaceFlags(EvalContext *ctx, AgentType ag, struct ifreq *ifr, Rlist **flags)
{
    char name[CF_MAXVARSIZE];
    char buffer[CF_BUFSIZE] = "";
    char *fp = NULL;

    if (ag != AGENT_TYPE_GENDOC)
    {
        snprintf(name, sizeof(name), "interface_flags[%s]", ifr->ifr_name);
    }
    else
    {
        snprintf(name, sizeof(name), "interface_flags[interface_name]");
    }

    if (ifr->ifr_flags & IFF_UP) strcat(buffer, " up");
    if (ifr->ifr_flags & IFF_BROADCAST) strcat(buffer, " broadcast");
    if (ifr->ifr_flags & IFF_DEBUG) strcat(buffer, " debug");
    if (ifr->ifr_flags & IFF_LOOPBACK) strcat(buffer, " loopback");
    if (ifr->ifr_flags & IFF_POINTOPOINT) strcat(buffer, " pointopoint");

#ifdef IFF_NOTRAILERS
    if (ifr->ifr_flags & IFF_NOTRAILERS) strcat(buffer, " notrailers");
#endif

    if (ifr->ifr_flags & IFF_RUNNING) strcat(buffer, " running");
    if (ifr->ifr_flags & IFF_NOARP) strcat(buffer, " noarp");
    if (ifr->ifr_flags & IFF_PROMISC) strcat(buffer, " promisc");
    if (ifr->ifr_flags & IFF_ALLMULTI) strcat(buffer, " allmulti");
    if (ifr->ifr_flags & IFF_MULTICAST) strcat(buffer, " multicast");

    // If a least 1 flag is found
    if (strlen(buffer) > 1)
    {
      // Skip leading space
      fp = buffer + 1;
      ScopeNewSpecial(ctx, "sys", name, fp, DATA_TYPE_STRING);
      RlistAppend(flags, fp, RVAL_TYPE_SCALAR);
    }
}

/******************************************************************/

void GetInterfacesInfo(EvalContext *ctx, AgentType ag)
{
    bool address_set = false;
    int fd, len, i, j;
    struct ifreq ifbuf[CF_IFREQ], ifr, *ifp;
    struct ifconf list;
    struct sockaddr_in *sin;
    struct hostent *hp;
    char *sp, workbuf[CF_BUFSIZE];
    char ip[CF_MAXVARSIZE];
    char name[CF_MAXVARSIZE];
    Rlist *interfaces = NULL, *hardware = NULL, *flags = NULL, *ips = NULL;

    /* This function may be called many times, while interfaces come and go */
    /* TODO cache results for non-daemon processes? */
    DeleteItemList(IPADDRESSES);
    IPADDRESSES = NULL;

    memset(ifbuf, 0, sizeof(ifbuf));

    InitIgnoreInterfaces();

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open socket. (socket: %s)", GetErrorStr());
        exit(1);
    }

    list.ifc_len = sizeof(ifbuf);
    list.ifc_req = ifbuf;

# ifdef SIOCGIFCONF
    if (ioctl(fd, SIOCGIFCONF, &list) == -1 || (list.ifc_len < (sizeof(struct ifreq))))
# else
    if ((ioctl(fd, OSIOCGIFCONF, &list) == -1) || (list.ifc_len < (sizeof(struct ifreq))))
# endif
    {
        Log(LOG_LEVEL_ERR, "Couldn't get interfaces - old kernel? Try setting CF_IFREQ to 1024. (ioctl: %s)", GetErrorStr());
        exit(1);
    }

    char last_name[sizeof(ifp->ifr_name)] = "";

    for (j = 0, len = 0, ifp = list.ifc_req; len < list.ifc_len;
         len += SIZEOF_IFREQ(*ifp), j++, ifp = (struct ifreq *) ((char *) ifp + SIZEOF_IFREQ(*ifp)))
    {

        if (ifp->ifr_addr.sa_family == 0)
        {
            continue;
        }

        if ((ifp->ifr_name == NULL) || (strlen(ifp->ifr_name) == 0))
        {
            continue;
        }

        /* Skip virtual network interfaces for Linux, which seems to be a problem */

        if (IgnoreInterface(ifp->ifr_name))
        {
            continue;
        }

        if (strstr(ifp->ifr_name, ":"))
        {
#ifdef __linux__
            Log(LOG_LEVEL_VERBOSE, "Skipping apparent virtual interface %d: %s", j + 1, ifp->ifr_name);
            continue;
#endif
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Interface %d: %s", j + 1, ifp->ifr_name);
        }

        /* If interface name appears a second time in a row then it has more
           than one IP addresses (linux: ip addr add $IP dev $IF).
           But the variable is already added so don't set it again. */
        if (strcmp(last_name, ifp->ifr_name) != 0)
        {
            strcpy(last_name, ifp->ifr_name);
            ScopeNewSpecial(ctx, "sys", "interface", last_name, DATA_TYPE_STRING);
        }

        snprintf(workbuf, sizeof(workbuf), "net_iface_%s", CanonifyName(ifp->ifr_name));
        EvalContextHeapAddHard(ctx, workbuf);

        /* TODO IPv6 should be handled transparently */
        if (ifp->ifr_addr.sa_family == AF_INET)
        {
            strncpy(ifr.ifr_name, ifp->ifr_name, sizeof(ifp->ifr_name));

            if (ioctl(fd, SIOCGIFFLAGS, &ifr) == -1)
            {
                Log(LOG_LEVEL_ERR, "No such network device. (ioctl: %s)", GetErrorStr());
                continue;
            }
            else
            {
              GetInterfaceFlags(ctx, ag, &ifr, &flags);
            }

            if (ifr.ifr_flags & IFF_UP)
            {
                sin = (struct sockaddr_in *) &ifp->ifr_addr;

                if (IgnoreJailInterface(j + 1, sin))
                {
                    Log(LOG_LEVEL_VERBOSE, "Ignoring interface %d", j + 1);
                    continue;
                }

                /* No DNS lookup, just convert IP address to string. */
                char txtaddr[CF_MAX_IP_LEN] = "";
                assert(sizeof(VIPADDRESS) >= sizeof(txtaddr));

                getnameinfo((struct sockaddr *) sin, sizeof(*sin),
                            txtaddr, sizeof(txtaddr),
                            NULL, 0, NI_NUMERICHOST);

                Log(LOG_LEVEL_DEBUG, "Adding hostip '%s'", txtaddr);
                EvalContextHeapAddHard(ctx, txtaddr);

                if ((hp = gethostbyaddr((char *) &(sin->sin_addr.s_addr),
                                        sizeof(sin->sin_addr.s_addr), AF_INET))
                    == NULL)
                {
                    Log(LOG_LEVEL_DEBUG, "No hostinformation for '%s' found",
                        txtaddr);
                }
                else
                {
                    if (hp->h_name != NULL)
                    {
                        Log(LOG_LEVEL_DEBUG, "Adding hostname '%s'", hp->h_name);
                        EvalContextHeapAddHard(ctx, hp->h_name);

                        if (hp->h_aliases != NULL)
                        {
                            for (i = 0; hp->h_aliases[i] != NULL; i++)
                            {
                                Log(LOG_LEVEL_DEBUG, "Adding alias '%s'",
                                    hp->h_aliases[i]);
                                EvalContextHeapAddHard(ctx, hp->h_aliases[i]);
                            }
                        }
                    }
                }

                if (strcmp(txtaddr, "0.0.0.0") == 0)
                {
                    /* TODO remove, interface address can't be 0.0.0.0 and
                     * even then DNS is not a safe way to set a variable... */
                    Log(LOG_LEVEL_VERBOSE, "Cannot discover hardware IP, using DNS value");
                    assert(sizeof(ip) >= sizeof(VIPADDRESS) + sizeof("ipv4_"));
                    strcpy(ip, "ipv4_");
                    strcat(ip, VIPADDRESS);
                    AppendItem(&IPADDRESSES, VIPADDRESS, "");
                    RlistAppendScalar(&ips, VIPADDRESS);

                    for (sp = ip + strlen(ip) - 1; (sp > ip); sp--)
                    {
                        if (*sp == '.')
                        {
                            *sp = '\0';
                            EvalContextHeapAddHard(ctx, ip);
                        }
                    }

                    strcpy(ip, VIPADDRESS);
                    i = 3;

                    for (sp = ip + strlen(ip) - 1; (sp > ip); sp--)
                    {
                        if (*sp == '.')
                        {
                            *sp = '\0';
                            snprintf(name, sizeof(name), "ipv4_%d[%s]", i--, CanonifyName(VIPADDRESS));
                            ScopeNewSpecial(ctx, "sys", name, ip, DATA_TYPE_STRING);
                        }
                    }
                    continue;
                }

                assert(sizeof(ip) >= sizeof(txtaddr) + sizeof("ipv4_"));
                strcpy(ip, "ipv4_");
                strcat(ip, txtaddr);
                EvalContextHeapAddHard(ctx, ip);

                /* VIPADDRESS has already been set to the DNS address of
                 * VFQNAME by GetNameInfo3() during initialisation. Here we
                 * reset VIPADDRESS to the address of the first non-loopback
                 * interface. */
                if (!address_set && !(ifr.ifr_flags & IFF_LOOPBACK))
                {
                    ScopeNewSpecial(ctx, "sys", "ipv4", txtaddr, DATA_TYPE_STRING);

                    strcpy(VIPADDRESS, txtaddr);
                    Log(LOG_LEVEL_VERBOSE, "IP address of host set to %s",
                        VIPADDRESS);
                    address_set = true;
                }

                AppendItem(&IPADDRESSES, txtaddr, "");
                RlistAppendScalar(&ips, txtaddr);

                for (sp = ip + strlen(ip) - 1; (sp > ip); sp--)
                {
                    if (*sp == '.')
                    {
                        *sp = '\0';
                        EvalContextHeapAddHard(ctx, ip);
                    }
                }

                // Set the IPv4 on interface array

                strcpy(ip, txtaddr);

                if (ag != AGENT_TYPE_GENDOC)
                {
                    snprintf(name, sizeof(name), "ipv4[%s]", CanonifyName(ifp->ifr_name));
                }
                else
                {
                    snprintf(name, sizeof(name), "ipv4[interface_name]");
                }

                ScopeNewSpecial(ctx, "sys", name, ip, DATA_TYPE_STRING);

                i = 3;

                for (sp = ip + strlen(ip) - 1; (sp > ip); sp--)
                {
                    if (*sp == '.')
                    {
                        *sp = '\0';

                        if (ag != AGENT_TYPE_GENDOC)
                        {
                            snprintf(name, sizeof(name), "ipv4_%d[%s]", i--, CanonifyName(ifp->ifr_name));
                        }
                        else
                        {
                            snprintf(name, sizeof(name), "ipv4_%d[interface_name]", i--);
                        }

                        ScopeNewSpecial(ctx, "sys", name, ip, DATA_TYPE_STRING);
                    }
                }
            }

            // Set the hardware/mac address array
            GetMacAddress(ctx, ag, fd, &ifr, ifp, &interfaces, &hardware);
        }
    }

    close(fd);

    ScopeNewSpecial(ctx, "sys", "interfaces", interfaces, DATA_TYPE_STRING_LIST);
    ScopeNewSpecial(ctx, "sys", "hardware_addresses", hardware, DATA_TYPE_STRING_LIST);
    ScopeNewSpecial(ctx, "sys", "hardware_flags", flags, DATA_TYPE_STRING_LIST);
    ScopeNewSpecial(ctx, "sys", "ip_addresses", ips, DATA_TYPE_STRING_LIST);

    RlistDestroy(interfaces);
    RlistDestroy(hardware);
    RlistDestroy(flags);
    RlistDestroy(ips);

    FindV6InterfacesInfo(ctx);
}

/*******************************************************************/

static void FindV6InterfacesInfo(EvalContext *ctx)
{
    FILE *pp = NULL;
    char buffer[CF_BUFSIZE];

/* Whatever the manuals might say, you cannot get IPV6
   interface configuration from the ioctls. This seems
   to be implemented in a non standard way across OSes
   BSDi has done getifaddrs(), solaris 8 has a new ioctl, Stevens
   book shows the suggestion which has not been implemented...
*/

    Log(LOG_LEVEL_VERBOSE, "Trying to locate my IPv6 address");

#if defined(__CYGWIN__)
    /* NT cannot do this */
    return;
#elif defined(__hpux)
    if ((pp = cf_popen("/usr/sbin/ifconfig -a", "r", true)) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Could not find interface info");
        return;
    }
#elif defined(_AIX)
    if ((pp = cf_popen("/etc/ifconfig -a", "r", true)) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Could not find interface info");
        return;
    }
#else
    if ((pp = cf_popen("/sbin/ifconfig -a", "r", true)) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "Could not find interface info");
        return;
    }
#endif

/* Don't know the output format of ifconfig on all these .. hope for the best*/

    for(;;)
    {
        if (fgets(buffer, sizeof(buffer), pp) == NULL)
        {
            if (ferror(pp))
            {
                UnexpectedError("Failed to read line from stream");
                break;
            }
            else /* feof */
            {
                break;
            }
        }

        if (strcasestr(buffer, "inet6"))
        {
            Item *ip, *list = NULL;
            char *sp;

            list = SplitStringAsItemList(buffer, ' ');

            for (ip = list; ip != NULL; ip = ip->next)
            {
                for (sp = ip->name; *sp != '\0'; sp++)
                {
                    if (*sp == '/')     /* Remove CIDR mask */
                    {
                        *sp = '\0';
                    }
                }

                if ((IsIPV6Address(ip->name)) && ((strcmp(ip->name, "::1") != 0)))
                {
                    Log(LOG_LEVEL_VERBOSE, "Found IPv6 address %s", ip->name);
                    AppendItem(&IPADDRESSES, ip->name, "");
                    EvalContextHeapAddHard(ctx, ip->name);
                }
            }

            DeleteItemList(list);
        }
    }

    cf_pclose(pp);
}

/*******************************************************************/

static void InitIgnoreInterfaces()
{
    FILE *fin;
    char filename[CF_BUFSIZE],regex[CF_MAXVARSIZE];

    snprintf(filename, sizeof(filename), "%s%cinputs%c%s", CFWORKDIR, FILE_SEPARATOR, FILE_SEPARATOR, CF_IGNORE_INTERFACES);

    if ((fin = fopen(filename,"r")) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "No interface exception file %s",filename);
        return;
    }
    
    while (!feof(fin))
    {
        regex[0] = '\0';
        int scanCount = fscanf(fin,"%s",regex);

        if (scanCount != 0 && *regex != '\0')
        {
           RlistPrependScalarIdemp(&IGNORE_INTERFACES, regex);
        }
    }
 
    fclose(fin);
}

/*******************************************************************/

static bool IgnoreInterface(char *name)
{
    Rlist *rp;

    for (rp = IGNORE_INTERFACES; rp != NULL; rp=rp->next)
    {
        if (FullTextMatch(rp->item,name))
        {
            Log(LOG_LEVEL_VERBOSE, "Ignoring interface '%s' because it matches '%s'",name,CF_IGNORE_INTERFACES);
            return true;
        }    
    }

    return false;
}



#endif /* !__MINGW32__ */
