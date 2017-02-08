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
#include <sysinfo.h>

#include <files_names.h>
#include <eval_context.h>
#include <item_lib.h>
#include <pipes.h>
#include <misc_lib.h>
#include <communication.h>
#include <string_lib.h>
#include <regex.h>                                       /* StringMatchFull */
#include <files_interfaces.h>
#include <files_names.h>
#include <known_dirs.h>
#include <ip_address.h>

#ifdef HAVE_SYS_JAIL_H
# include <sys/jail.h>
#endif

#ifdef HAVE_GETIFADDRS
# include <ifaddrs.h>
# ifdef HAVE_NET_IF_DL_H
#   include <net/if_dl.h>
# endif
#endif

#ifdef HAVE_NET_IF_ARP_H
# include <net/if_arp.h>
#endif

#define CF_IFREQ 2048           /* Reportedly the largest size that does not segfault 32/64 bit */
#define CF_IGNORE_INTERFACES "ignore_interfaces.rx"

#ifndef __MINGW32__

# if defined(HAVE_STRUCT_SOCKADDR_SA_LEN) && !defined(__NetBSD__)
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

#ifdef _AIX
#include <sys/ndd_var.h>
#include <sys/kinfo.h>
static int aix_get_mac_addr(const char *device_name, uint8_t mac[6]);
#endif

static void FindV6InterfacesInfo(EvalContext *ctx);
static bool IgnoreJailInterface(int ifaceidx, struct sockaddr_in *inaddr);
static bool IgnoreInterface(char *name);
static void InitIgnoreInterfaces(void);

static Rlist *IGNORE_INTERFACES = NULL; /* GLOBAL_E */

typedef void (*ProcPostProcessFn)(void *ctx, void *json);


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

static void GetMacAddress(EvalContext *ctx, int fd, struct ifreq *ifr, struct ifreq *ifp, Rlist **interfaces,
                          Rlist **hardware)
{
    char name[CF_MAXVARSIZE];

    snprintf(name, sizeof(name), "hardware_mac[%s]", CanonifyName(ifp->ifr_name));

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

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, name, hw_mac, CF_DATA_TYPE_STRING, "source=agent");
    RlistAppend(hardware, hw_mac, RVAL_TYPE_SCALAR);
    RlistAppend(interfaces, ifp->ifr_name, RVAL_TYPE_SCALAR);

    snprintf(name, sizeof(name), "mac_%s", CanonifyName(hw_mac));
    EvalContextClassPutHard(ctx, name, "inventory,attribute_name=none,source=agent");

# elif defined(HAVE_GETIFADDRS) && !defined(__sun)
    char hw_mac[CF_MAXVARSIZE];
    char *m;
    struct ifaddrs *ifaddr, *ifa;
    struct sockaddr_dl *sdl;

    if (getifaddrs(&ifaddr) == -1)
    {
        Log(LOG_LEVEL_ERR, "!! Could not get interface %s addresses",
          ifp->ifr_name);

        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, name, "mac_unknown", CF_DATA_TYPE_STRING, "source=agent");
        EvalContextClassPutHard(ctx, "mac_unknown", "source=agent");
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

                EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, name, hw_mac, CF_DATA_TYPE_STRING, "source=agent");
                RlistAppend(hardware, hw_mac, RVAL_TYPE_SCALAR);
                RlistAppend(interfaces, ifa->ifa_name, RVAL_TYPE_SCALAR);

                snprintf(name, sizeof(name), "mac_%s", CanonifyName(hw_mac));
                EvalContextClassPutHard(ctx, name, "source=agent");
            }
        }

    }
    freeifaddrs(ifaddr);
    
# elif defined(_AIX) && !defined(HAVE_GETIFADDRS)
    char hw_mac[CF_MAXVARSIZE];
    char mac[CF_MAXVARSIZE];
    
    if (aix_get_mac_addr(ifp->ifr_name, mac) == 0)
    {
        sprintf(hw_mac, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
	       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, name, hw_mac, CF_DATA_TYPE_STRING, "source=agent");
        RlistAppend(hardware, hw_mac, RVAL_TYPE_SCALAR);
        RlistAppend(interfaces, ifp->ifr_name, RVAL_TYPE_SCALAR);

        snprintf(name, CF_MAXVARSIZE, "mac_%s", CanonifyName(hw_mac));
        EvalContextClassPutHard(ctx, name, "inventory,attribute_name=none,source=agent");
    }
    else
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, name, "mac_unknown", CF_DATA_TYPE_STRING, "source=agent");
        EvalContextClassPutHard(ctx, "mac_unknown", "source=agent");
    }

# elif defined(SIOCGARP)

    struct arpreq arpreq;

    ((struct sockaddr_in *) &arpreq.arp_pa)->sin_addr.s_addr =
        ((struct sockaddr_in *) &ifp->ifr_addr)->sin_addr.s_addr;

    if (ioctl(fd, SIOCGARP, &arpreq) == -1)
    {
        // ENXIO happens if there is no MAC address assigned, which is not that
        // uncommon.
        LogLevel log_level =
            (errno == ENXIO) ? LOG_LEVEL_VERBOSE : LOG_LEVEL_ERR;
        Log(log_level,
            "Could not get interface '%s' addresses (ioctl(SIOCGARP): %s)",
            ifp->ifr_name, GetErrorStr());
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, name,
                                      "mac_unknown", CF_DATA_TYPE_STRING,
                                      "source=agent");
        EvalContextClassPutHard(ctx, "mac_unknown", "source=agent");
        return;
    }

    char hw_mac[CF_MAXVARSIZE];

    snprintf(hw_mac, sizeof(hw_mac), "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
             (unsigned char) arpreq.arp_ha.sa_data[0],
             (unsigned char) arpreq.arp_ha.sa_data[1],
             (unsigned char) arpreq.arp_ha.sa_data[2],
             (unsigned char) arpreq.arp_ha.sa_data[3],
             (unsigned char) arpreq.arp_ha.sa_data[4],
             (unsigned char) arpreq.arp_ha.sa_data[5]);

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, name,
                                  hw_mac, CF_DATA_TYPE_STRING,
                                  "source=agent");
    RlistAppend(hardware, hw_mac, RVAL_TYPE_SCALAR);
    RlistAppend(interfaces, ifp->ifr_name, RVAL_TYPE_SCALAR);

    snprintf(name, sizeof(name), "mac_%s", CanonifyName(hw_mac));
    EvalContextClassPutHard(ctx, name,
                            "inventory,attribute_name=none,source=agent");

# else
    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, name,
                                  "mac_unknown", CF_DATA_TYPE_STRING,
                                  "source=agent");
    EvalContextClassPutHard(ctx, "mac_unknown", "source=agent");
# endif
}

/******************************************************************/

static void GetInterfaceFlags(EvalContext *ctx, struct ifreq *ifr, Rlist **flags)
{
    char name[CF_MAXVARSIZE];
    char buffer[CF_BUFSIZE] = "";
    char *fp = NULL;

    snprintf(name, sizeof(name), "interface_flags[%s]", ifr->ifr_name);

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
      EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, name, fp, CF_DATA_TYPE_STRING, "source=agent");
      RlistAppend(flags, fp, RVAL_TYPE_SCALAR);
    }
}

/******************************************************************/

void GetInterfacesInfo(EvalContext *ctx)
{
    bool address_set = false;
    int fd, len, i, j;
    struct ifreq ifbuf[CF_IFREQ], ifr, *ifp;
    struct ifconf list;
    struct sockaddr_in *sin;
    char *sp, workbuf[CF_BUFSIZE];
    char ip[CF_MAXVARSIZE];
    char name[CF_MAXVARSIZE];
    Rlist *interfaces = NULL, *hardware = NULL, *flags = NULL, *ips = NULL;

    /* This function may be called many times, while interfaces come and go */
    /* TODO cache results for non-daemon processes? */
    EvalContextDeleteIpAddresses(ctx);

    memset(ifbuf, 0, sizeof(ifbuf));

    InitIgnoreInterfaces();

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        Log(LOG_LEVEL_ERR, "Couldn't open socket. (socket: %s)", GetErrorStr());
        exit(EXIT_FAILURE);
    }

    list.ifc_len = sizeof(ifbuf);
    list.ifc_req = ifbuf;

    /* WARNING: *BSD use unsigned long as second argument to ioctl() while
     * POSIX specifies *signed* int. Using the largest possible signed type is
     * the best strategy.*/
#ifdef SIOCGIFCONF
    intmax_t request = SIOCGIFCONF;
#else
    intmax_t request = OSIOCGIFCONF;
#endif
    int ret = ioctl(fd, request, &list);
    if (ret == -1)
    {
        Log(LOG_LEVEL_ERR,
            "Couldn't get interfaces (ioctl(SIOCGIFCONF): %s)",
            GetErrorStr());
        exit(EXIT_FAILURE);
    }

    if (list.ifc_len < (int) sizeof(struct ifreq))
    {
        Log(LOG_LEVEL_VERBOSE,
            "Interface list returned is too small (%d bytes), "
            "assuming no interfaces present", list.ifc_len);
        list.ifc_len = 0;
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

        /* Skip network interfaces listed in ignore_interfaces.rx */

        if (IgnoreInterface(ifp->ifr_name))
        {
            continue;
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
            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "interface", last_name, CF_DATA_TYPE_STRING, "source=agent");
        }

        snprintf(workbuf, sizeof(workbuf), "net_iface_%s", CanonifyName(ifp->ifr_name));
        EvalContextClassPutHard(ctx, workbuf, "source=agent");

        /* TODO IPv6 should be handled transparently */
        if (ifp->ifr_addr.sa_family == AF_INET)
        {
            strlcpy(ifr.ifr_name, ifp->ifr_name, sizeof(ifp->ifr_name));

            if (ioctl(fd, SIOCGIFFLAGS, &ifr) == -1)
            {
                Log(LOG_LEVEL_ERR, "No such network device. (ioctl: %s)", GetErrorStr());
                continue;
            }
            else
            {
              GetInterfaceFlags(ctx, &ifr, &flags);
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
                EvalContextClassPutHard(ctx, txtaddr, "inventory,attribute_name=none,source=agent");

                if (strcmp(txtaddr, "0.0.0.0") == 0)
                {
                    /* TODO remove, interface address can't be 0.0.0.0 and
                     * even then DNS is not a safe way to set a variable... */
                    Log(LOG_LEVEL_VERBOSE, "Cannot discover hardware IP, using DNS value");
                    assert(sizeof(ip) >= sizeof(VIPADDRESS) + sizeof("ipv4_"));
                    strcpy(ip, "ipv4_");
                    strcat(ip, VIPADDRESS);
                    EvalContextAddIpAddress(ctx, VIPADDRESS, NULL); // we don't know the interface
                    RlistAppendScalar(&ips, VIPADDRESS);

                    for (sp = ip + strlen(ip) - 1; (sp > ip); sp--)
                    {
                        if (*sp == '.')
                        {
                            *sp = '\0';
                            EvalContextClassPutHard(ctx, ip, "inventory,attribute_name=none,source=agent");
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
                            EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, name, ip, CF_DATA_TYPE_STRING, "source=agent");
                        }
                    }
                    continue;
                }

                assert(sizeof(ip) >= sizeof(txtaddr) + sizeof("ipv4_"));
                strcpy(ip, "ipv4_");
                strcat(ip, txtaddr);
                EvalContextClassPutHard(ctx, ip, "inventory,attribute_name=none,source=agent");

                /* VIPADDRESS has already been set to the DNS address of
                 * VFQNAME by GetNameInfo3() during initialisation. Here we
                 * reset VIPADDRESS to the address of the first non-loopback
                 * interface. */
                if (!address_set && !(ifr.ifr_flags & IFF_LOOPBACK))
                {
                    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "ipv4", txtaddr, CF_DATA_TYPE_STRING, "inventory,source=agent,attribute_name=none");

                    strcpy(VIPADDRESS, txtaddr);
                    Log(LOG_LEVEL_VERBOSE, "IP address of host set to %s",
                        VIPADDRESS);
                    address_set = true;
                }

                EvalContextAddIpAddress(ctx, txtaddr, CanonifyName(ifp->ifr_name));
                RlistAppendScalar(&ips, txtaddr);

                for (sp = ip + strlen(ip) - 1; (sp > ip); sp--)
                {
                    if (*sp == '.')
                    {
                        *sp = '\0';
                        EvalContextClassPutHard(ctx, ip, "inventory,attribute_name=none,source=agent");
                    }
                }

                // Set the IPv4 on interface array

                strcpy(ip, txtaddr);

                snprintf(name, sizeof(name), "ipv4[%s]", CanonifyName(ifp->ifr_name));

                EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, name, ip, CF_DATA_TYPE_STRING, "source=agent");

                // generate the reverse mapping
                snprintf(name, sizeof(name), "ip2iface[%s]", txtaddr);

                EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, name, CanonifyName(ifp->ifr_name), CF_DATA_TYPE_STRING, "source=agent");

                i = 3;

                for (sp = ip + strlen(ip) - 1; (sp > ip); sp--)
                {
                    if (*sp == '.')
                    {
                        *sp = '\0';

                        snprintf(name, sizeof(name), "ipv4_%d[%s]", i--, CanonifyName(ifp->ifr_name));

                        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, name, ip, CF_DATA_TYPE_STRING, "source=agent");
                    }
                }
            }

            // Set the hardware/mac address array
            GetMacAddress(ctx, fd, &ifr, ifp, &interfaces, &hardware);
        }
    }

    close(fd);

    if (interfaces)
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "interfaces", interfaces, CF_DATA_TYPE_STRING_LIST,
                                      "inventory,source=agent,attribute_name=Interfaces");
    }
    if (hardware)
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "hardware_addresses", hardware, CF_DATA_TYPE_STRING_LIST,
                                      "inventory,source=agent,attribute_name=MAC addresses");
    }
    if (flags)
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "hardware_flags", flags, CF_DATA_TYPE_STRING_LIST,
                                      "source=agent");
    }
    if (ips)
    {
        EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "ip_addresses", ips, CF_DATA_TYPE_STRING_LIST,
                                      "source=agent");
    }

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
    if ((!FileCanOpen("/sbin/ifconfig", "r") || ((pp = cf_popen("/sbin/ifconfig -a", "r", true)) == NULL)) &&
        (!FileCanOpen("/bin/ifconfig", "r") || ((pp = cf_popen("/bin/ifconfig -a", "r", true)) == NULL)))
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
                    EvalContextAddIpAddress(ctx, ip->name, NULL); // interface unknown
                    EvalContextClassPutHard(ctx, ip->name, "inventory,attribute_name=none,source=agent");
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
    char filename[CF_BUFSIZE],regex[256];

    snprintf(filename, sizeof(filename), "%s%c%s", GetInputDir(), FILE_SEPARATOR, CF_IGNORE_INTERFACES);

    if ((fin = fopen(filename,"r")) == NULL)
    {
        Log(LOG_LEVEL_VERBOSE, "No interface exception file %s",filename);
        return;
    }
    
    while (!feof(fin))
    {
        regex[0] = '\0';
        int scanCount = fscanf(fin,"%255s",regex);

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
        /* FIXME: review this strcmp. Moved out from StringMatch */
        if (!strcmp(RlistScalarValue(rp), name)
            || StringMatchFull(RlistScalarValue(rp), name))
        {
            Log(LOG_LEVEL_VERBOSE, "Ignoring interface '%s' because it matches '%s'",name,CF_IGNORE_INTERFACES);
            return true;
        }    
    }

    return false;
}

#ifdef _AIX
static int aix_get_mac_addr(const char *device_name, uint8_t mac[6])
{
    size_t ksize;
    struct kinfo_ndd *ndd;
    int count, i;

    ksize = getkerninfo(KINFO_NDD, 0, 0, 0);
    if (ksize == 0) 
    {
        errno = ENOSYS;
        return -1;
    }

    ndd = (struct kinfo_ndd *)xmalloc(ksize);
    if (ndd == NULL) 
    {
        errno = ENOMEM;
        return -1;
    }

    if (getkerninfo(KINFO_NDD, ndd, &ksize, 0) == -1) 
    {
        errno = ENOSYS;
        return -1;
    }

    count= ksize/sizeof(struct kinfo_ndd);
    for (i=0;i<count;i++) 
    {
        if ((ndd[i].ndd_type == NDD_ETHER || 
            ndd[i].ndd_type == NDD_ISO88023) &&
            ndd[i].ndd_addrlen == 6 &&
            (strcmp(ndd[i].ndd_alias, device_name) == 0 ||
            strcmp(ndd[i].ndd_name, device_name == 0))) 
        {
            memcpy(mac, ndd[i].ndd_addr, 6);
            free(ndd);
            return 0;
        }
    }
    free(ndd);
    errno = ENOENT;
    return -1;
}
#endif /* _AIX */

// TODO: perhaps rename and move these to json.c and ip_address.c?  Or even let JsonElements store IPAddress structs?
static IPAddress* ParsedIPAddressHex(const char *data)
{
    IPAddress *ip = NULL;
    Buffer *buffer = BufferNewFrom(data, strlen(data));
    if (buffer != NULL)
    {
        ip = IPAddressNewHex(buffer);
        BufferDestroy(buffer);
    }

    return ip;
}

static void JsonRewriteParsedIPAddress(JsonElement* element, const char* raw_key, const char *new_key, const bool as_map)
{
    IPAddress *addr = ParsedIPAddressHex(JsonObjectGetAsString(element, raw_key));
    if (addr != NULL)
    {
        Buffer *buf = IPAddressGetAddress(addr);
        if (buf != NULL)
        {
            JsonObjectRemoveKey(element, raw_key);
            if (as_map)
            {
                JsonElement *ip = JsonObjectCreate(2);
                JsonObjectAppendString(ip, "address", BufferData(buf));
                BufferPrintf(buf, "%d", IPAddressGetPort(addr));
                JsonObjectAppendString(ip, "port", BufferData(buf));
                JsonObjectAppendElement(element, new_key, ip);
            }
            else
            {
                JsonObjectAppendString(element, new_key, BufferData(buf));
            }

            BufferDestroy(buf);
        }

        IPAddressDestroy(&addr);
    }
}

static long JsonExtractParsedNumber(JsonElement* element, const char* raw_key, const char *new_key, const bool hex_mode, const bool keep_number)
{
    long num = 0;

    if (sscanf(JsonObjectGetAsString(element, raw_key),
               hex_mode ? "%lx" : "%ld",
               &num)
        == 1)
    {
        if (!keep_number)
        {
            JsonObjectRemoveKey(element, raw_key);
        }

        if (new_key != NULL)
        {
            JsonObjectAppendInteger(element, new_key, num);
        }
    }

    return num;
}

/*******************************************************************/

static ProcPostProcessFn NetworkingRoutesPostProcessInfo(void *passed_ctx, void *json)
{
# if defined (__linux__)
    EvalContext *ctx = passed_ctx;
    JsonElement *route = json;

    JsonRewriteParsedIPAddress(route, "raw_dest", "dest", false);
    JsonRewriteParsedIPAddress(route, "raw_gw", "gateway", false);
    JsonRewriteParsedIPAddress(route, "raw_mask", "mask", false);

    // TODO: check that the metric and the others are decimal (ipv6_route uses hex for metric and others maybe)
    JsonExtractParsedNumber(route, "metric", "metric", false, false);
    JsonExtractParsedNumber(route, "mtu", "mtu", false, false);
    JsonExtractParsedNumber(route, "refcnt", "refcnt", false, false);
    JsonExtractParsedNumber(route, "use", "use", false, false);
    JsonExtractParsedNumber(route, "window", "window", false, false);
    JsonExtractParsedNumber(route, "irtt", "irtt", false, false);

    JsonElement *decoded_flags = JsonArrayCreate(3);
    long num_flags = JsonExtractParsedNumber(route, "raw_flags", NULL, true, false);

    bool is_up = (num_flags & RTF_UP);
    bool is_gw = (num_flags & RTF_GATEWAY);
    bool is_host = (num_flags & RTF_HOST);
    bool is_default_route = (strcmp(JsonObjectGetAsString(route, "dest"), "0.0.0.0") == 0);

    const char* gw_type = is_gw ? "gateway":"local";

    // These flags are always included on Linux in platform.h
    JsonArrayAppendString(decoded_flags, is_up ? "up":"down");
    JsonArrayAppendString(decoded_flags, is_host ? "host":"net");
    JsonArrayAppendString(decoded_flags, is_default_route ? "default" : "not_default");
    JsonArrayAppendString(decoded_flags, gw_type);
    JsonObjectAppendElement(route, "flags", decoded_flags);
    JsonObjectAppendBool(route, "active_default_gateway", is_default_route && is_up && is_gw);

    if (is_up && is_gw)
    {
        Buffer *formatter = BufferNew();
        BufferPrintf(formatter, "ipv4_gw_%s", JsonObjectGetAsString(route, "gateway"));
        EvalContextClassPutHard(ctx, BufferData(formatter), "inventory,networking,/proc,source=agent,attribute_name=none,procfs");
        BufferDestroy(formatter);
    }
# endif
    return NULL;
}

static ProcPostProcessFn NetworkingIPv6RoutesPostProcessInfo(ARG_UNUSED void *passed_ctx, void *json)
{
# if defined (__linux__)
    JsonElement *route = json;

    JsonRewriteParsedIPAddress(route, "raw_dest", "dest", false);
    JsonRewriteParsedIPAddress(route, "raw_next_hop", "next_hop", false);
    JsonRewriteParsedIPAddress(route, "raw_source", "dest", false);

    JsonExtractParsedNumber(route, "raw_metric", "metric", true, false);
    JsonExtractParsedNumber(route, "refcnt", "refcnt", false, false);
    JsonExtractParsedNumber(route, "use", "use", false, false);

    JsonElement *decoded_flags = JsonArrayCreate(3);
    long num_flags = JsonExtractParsedNumber(route, "raw_flags", NULL, true, false);

    bool is_up = (num_flags & RTF_UP);
    bool is_gw = (num_flags & RTF_GATEWAY);
    bool is_host = (num_flags & RTF_HOST);

    const char* gw_type = is_gw ? "gateway":"local";

    // These flags are always included on Linux in platform.h
    JsonArrayAppendString(decoded_flags, is_up ? "up":"down");
    JsonArrayAppendString(decoded_flags, is_host ? "host":"net");
    JsonArrayAppendString(decoded_flags, gw_type);
    JsonObjectAppendElement(route, "flags", decoded_flags);

    // TODO: figure out if we can grab any default gateway info here
    // like we do with IPv4 routes

# endif
    return NULL;
}

static ProcPostProcessFn NetworkingIPv6AddressesPostProcessInfo(ARG_UNUSED void *passed_ctx, void *json)
{
    JsonElement *entry = json;

    JsonRewriteParsedIPAddress(entry, "raw_address", "address", false);

    JsonExtractParsedNumber(entry, "raw_device_number", "device_number", true, false);
    JsonExtractParsedNumber(entry, "raw_prefix_length", "prefix_length", true, false);
    JsonExtractParsedNumber(entry, "raw_scope", "scope", true, false);
    return NULL;
}

/*******************************************************************/

static const char* GetPortStateString(int state)
{
# if defined (__linux__)
    switch (state)
    {
    case TCP_ESTABLISHED: return "ESTABLISHED";
    case TCP_SYN_SENT:    return "SYN_SENT";
    case TCP_SYN_RECV:    return "SYN_RECV";
    case TCP_FIN_WAIT1:   return "FIN_WAIT1";
    case TCP_FIN_WAIT2:   return "FIN_WAIT2";
    case TCP_TIME_WAIT:   return "TIME_WAIT";
    case TCP_CLOSE:       return "CLOSE";
    case TCP_CLOSE_WAIT:  return "CLOSE_WAIT";
    case TCP_LAST_ACK:    return "LAST_ACK";
    case TCP_LISTEN:      return "LISTEN";
    case TCP_CLOSING:     return "CLOSING";
    }

# endif
    return "UNKNOWN";
}

// used in evalfunction.c but defined here so
// JsonRewriteParsedIPAddress() etc. can stay local
ProcPostProcessFn NetworkingPortsPostProcessInfo(ARG_UNUSED void *passed_ctx, void *json)
{
    JsonElement *conn = json;

    if (conn != NULL)
    {
        JsonRewriteParsedIPAddress(conn, "raw_local", "local", true);
        JsonRewriteParsedIPAddress(conn, "raw_remote", "remote", true);

        long num_state = JsonExtractParsedNumber(conn, "raw_state", "temp_state", false, false);

        if (JsonObjectGetAsString(conn, "temp_state") != NULL)
        {
            JsonObjectRemoveKey(conn, "temp_state");
            JsonObjectAppendString(conn, "state", GetPortStateString(num_state));
        }
    }

    return NULL;
}

/*******************************************************************/

static JsonElement* GetNetworkingStatsInfo(const char *filename)
{
    JsonElement *stats = NULL;
    assert(filename);

    FILE *fin = safe_fopen(filename, "rt");
    if (fin)
    {
        Log(LOG_LEVEL_VERBOSE, "Reading netstat info from %s", filename);
        size_t header_line_size = CF_BUFSIZE;
        char *header_line = xmalloc(header_line_size);
        stats = JsonObjectCreate(2);

        while (CfReadLine(&header_line, &header_line_size, fin) != -1)
        {
            char* colon_ptr = strchr(header_line, ':');
            if (colon_ptr != NULL &&
                colon_ptr+2 < header_line + strlen(header_line))
            {
                JsonElement *stat = JsonObjectCreate(3);
                Buffer *type = BufferNewFrom(header_line, colon_ptr - header_line);
                size_t type_length = BufferSize(type);
                Rlist *info = RlistFromSplitString(colon_ptr+2, ' ');
                size_t line_size = CF_BUFSIZE;
                char *line = xmalloc(line_size);
                if (CfReadLine(&line, &line_size, fin) != -1)
                {
                    if (strlen(line) > type_length+2)
                    {
                        Rlist *data = RlistFromSplitString(line+type_length+2, ' ');
                        for (const Rlist *rp = info, *rdp = data;
                             rp != NULL && rdp != NULL;
                             rp = rp->next, rdp = rdp->next)
                        {
                            JsonObjectAppendString(stat, RlistScalarValue(rp), RlistScalarValue(rdp));
                        }
                        RlistDestroy(data);
                    }
                }

                JsonObjectAppendElement(stats, BufferData(type), stat);

                free(line);
                RlistDestroy(info);
                BufferDestroy(type);
            }

        }

        free(header_line);

        fclose(fin);
    }

    return stats;
}

/*******************************************************************/

// always returns the parsed data. If the key is not NULL, also
// creates a sys.KEY variable.

JsonElement* GetProcFileInfo(EvalContext *ctx, const char* filename, const char* key, const char* extracted_key, ProcPostProcessFn post, const char* regex)
{
    JsonElement *info = NULL;
    bool extract_key_mode = (extracted_key != NULL);

    FILE *fin = safe_fopen(filename, "rt");
    if (fin)
    {
        Log(LOG_LEVEL_VERBOSE, "Reading %s info from %s", key, filename);

        pcre *pattern = NULL;
        {
            const char *errorstr;
            int erroffset;
            pattern = pcre_compile(regex, PCRE_MULTILINE | PCRE_DOTALL,
                                   &errorstr, &erroffset, NULL);
        }

        if (pattern != NULL)
        {
            size_t line_size = CF_BUFSIZE;
            char *line = xmalloc(line_size);

            info = extract_key_mode ? JsonObjectCreate(10) : JsonArrayCreate(10);

            while (CfReadLine(&line, &line_size, fin) != -1)
            {
                JsonElement *item = StringCaptureData(pattern, regex, line);

                if (item != NULL)
                {
                    if (post != NULL)
                    {
                        (*post)(ctx, item);
                    }

                    if (extract_key_mode)
                    {
                        if (JsonObjectGetAsString(item, extracted_key) == NULL)
                        {
                            Log(LOG_LEVEL_ERR, "While parsing %s, looked to extract key %s but couldn't find it in line %s", filename, extracted_key, line);
                        }
                        else
                        {
                            Log(LOG_LEVEL_DEBUG, "While parsing %s, got key %s from line %s", filename, JsonObjectGetAsString(item, extracted_key), line);
                            JsonObjectAppendElement(info, JsonObjectGetAsString(item, extracted_key), item);
                        }
                    }
                    else
                    {
                        JsonArrayAppendElement(info, item);
                    }
                }
            }

            free(line);

            if (key != NULL)
            {
                Buffer *varname = BufferNew();
                BufferPrintf(varname, "%s", key);
                EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, BufferData(varname), info, CF_DATA_TYPE_CONTAINER,
                                              "inventory,networking,/proc,source=agent,attribute_name=none,procfs");
                BufferDestroy(varname);
            }

            pcre_free(pattern);
        }

        fclose(fin);
    }

    return info;
}

/*******************************************************************/

const char* GetNetworkingProcdir()
{
    const char *procdir = getenv("CFENGINE_TEST_OVERRIDE_PROCDIR");
    if (procdir == NULL)
    {
        procdir = "";
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Overriding /proc location to be %s", procdir);
    }

    return procdir;
}

void GetNetworkingInfo(EvalContext *ctx)
{
    const char *procdir = GetNetworkingProcdir();

    Buffer *pbuf = BufferNew();

    JsonElement *inet = JsonObjectCreate(2);

    BufferPrintf(pbuf, "%s/proc/net/netstat", procdir);
    JsonElement *inet_stats = GetNetworkingStatsInfo(BufferData(pbuf));

    if (inet_stats != NULL)
    {
        JsonObjectAppendElement(inet, "stats", inet_stats);
    }

    BufferPrintf(pbuf, "%s/proc/net/route", procdir);
    JsonElement *routes = GetProcFileInfo(ctx, BufferData(pbuf),  NULL, NULL, (ProcPostProcessFn) &NetworkingRoutesPostProcessInfo,
                    // format: Iface	Destination	Gateway 	Flags	RefCnt	Use	Metric	Mask		MTU	Window	IRTT
                    //         eth0	00000000	0102A8C0	0003	0	0	1024	00000000	0	0	0 
                    "^(?<interface>\\S+)\\t(?<raw_dest>[[:xdigit:]]+)\\t(?<raw_gw>[[:xdigit:]]+)\\t(?<raw_flags>[[:xdigit:]]+)\\t(?<refcnt>\\d+)\\t(?<use>\\d+)\\t(?<metric>[[:xdigit:]]+)\\t(?<raw_mask>[[:xdigit:]]+)\\t(?<mtu>\\d+)\\t(?<window>\\d+)\\t(?<irtt>[[:xdigit:]]+)");

    if (routes != NULL &&
        JsonGetElementType(routes) == JSON_ELEMENT_TYPE_CONTAINER)
    {
        JsonObjectAppendElement(inet, "routes", routes);

        JsonIterator iter = JsonIteratorInit(routes);
        const JsonElement *default_route = NULL;
        long lowest_metric = 0;
        const JsonElement *route = NULL;
        while ((route = JsonIteratorNextValue(&iter)))
        {
            JsonElement *active = JsonObjectGet(route, "active_default_gateway");
            if (active != NULL &&
                JsonGetElementType(active) == JSON_ELEMENT_TYPE_PRIMITIVE &&
                JsonGetPrimitiveType(active) == JSON_PRIMITIVE_TYPE_BOOL &&
                JsonPrimitiveGetAsBool(active))
            {
                JsonElement *metric = JsonObjectGet(route, "metric");
                if (metric != NULL &&
                    JsonGetElementType(metric) == JSON_ELEMENT_TYPE_PRIMITIVE &&
                    JsonGetPrimitiveType(metric) == JSON_PRIMITIVE_TYPE_INTEGER &&
                    (default_route == NULL ||
                     JsonPrimitiveGetAsInteger(metric) < lowest_metric))
                {
                    default_route = route;
                }
            }
        }

        if (default_route != NULL)
        {
            JsonObjectAppendString(inet, "default_gateway", JsonObjectGetAsString(default_route, "gateway"));
            JsonObjectAppendElement(inet, "default_route", JsonCopy(default_route));
        }
    }

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "inet", inet, CF_DATA_TYPE_CONTAINER,
                                  "inventory,networking,/proc,source=agent,attribute_name=none,procfs");
    JsonDestroy(inet);

    JsonElement *inet6 = JsonObjectCreate(3);

    BufferPrintf(pbuf, "%s/proc/net/snmp6", procdir);
    JsonElement *inet6_stats = GetProcFileInfo(ctx, BufferData(pbuf), NULL, NULL, NULL,
                                               "^\\s*(?<key>\\S+)\\s+(?<value>\\d+)");

    if (inet6_stats != NULL)
    {
        // map the key to the value (as a number) in the "stats" map
        JsonElement *rewrite = JsonObjectCreate(JsonLength(inet6_stats));
        JsonIterator iter = JsonIteratorInit(inet6_stats);
        const JsonElement *stat = NULL;
        while ((stat = JsonIteratorNextValue(&iter)))
        {
            long num = 0;
            const char* key = JsonObjectGetAsString(stat, "key");
            const char* value = JsonObjectGetAsString(stat, "value");
            if (key && value &&
                sscanf(value, "%ld", &num) == 1)
            {
                JsonObjectAppendInteger(rewrite, key, num);
            }
        }

        JsonObjectAppendElement(inet6, "stats", rewrite);
        JsonDestroy(inet6_stats);
    }

    BufferPrintf(pbuf, "%s/proc/net/ipv6_route", procdir);
    JsonElement *inet6_routes = GetProcFileInfo(ctx, BufferData(pbuf),  NULL, NULL, (ProcPostProcessFn) &NetworkingIPv6RoutesPostProcessInfo,
                    // format: dest                    dest_prefix source                source_prefix next_hop                         metric   refcnt   use      flags        interface
                    //         fe800000000000000000000000000000 40 00000000000000000000000000000000 00 00000000000000000000000000000000 00000100 00000000 00000000 00000001     eth0
                    "^(?<raw_dest>[[:xdigit:]]+)\\s+(?<dest_prefix>[[:xdigit:]]+)\\s+"
                    "(?<raw_source>[[:xdigit:]]+)\\s+(?<source_prefix>[[:xdigit:]]+)\\s+"
                    "(?<raw_next_hop>[[:xdigit:]]+)\\s+(?<raw_metric>[[:xdigit:]]+)\\s+"
                    "(?<refcnt>\\d+)\\s+(?<use>\\d+)\\s+"
                    "(?<raw_flags>[[:xdigit:]]+)\\s+(?<interface>\\S+)");

    if (inet6_routes != NULL)
    {
        JsonObjectAppendElement(inet6, "routes", inet6_routes);
    }

    BufferPrintf(pbuf, "%s/proc/net/if_inet6", procdir);
    JsonElement *inet6_addresses = GetProcFileInfo(ctx, BufferData(pbuf),  NULL, "interface", (ProcPostProcessFn) &NetworkingIPv6AddressesPostProcessInfo,
                    // format: address device_number prefix_length scope flags interface_name
                    // 00000000000000000000000000000001 01 80 10 80       lo
                    // fe80000000000000004249fffebdd7b4 04 40 20 80  docker0
                    // fe80000000000000c27cd1fffe3eada6 02 40 20 80   enp4s0
                    "^(?<raw_address>[[:xdigit:]]+)\\s+(?<raw_device_number>[[:xdigit:]]+)\\s+"
                    "(?<raw_prefix_length>[[:xdigit:]]+)\\s+(?<raw_scope>[[:xdigit:]]+)\\s+"
                    "(?<raw_flags>[[:xdigit:]]+)\\s+(?<interface>\\S+)");

    if (inet6_addresses != NULL)
    {
        JsonObjectAppendElement(inet6, "addresses", inet6_addresses);
    }

    EvalContextVariablePutSpecial(ctx, SPECIAL_SCOPE_SYS, "inet6", inet6, CF_DATA_TYPE_CONTAINER,
                                  "inventory,networking,/proc,source=agent,attribute_name=none,procfs");
    JsonDestroy(inet6);

    // Inter-|   Receive                                                |  Transmit
    //  face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
    //   eth0: 74850544807 75236137    0    0    0     0          0   1108775 63111535625 74696758    0    0    0     0       0          0

    BufferPrintf(pbuf, "%s/proc/net/dev", procdir);
    JsonElement *interfaces_data =
    GetProcFileInfo(ctx, BufferData(pbuf), "interfaces_data", "device", NULL,
                    "^\\s*(?<device>[^:]+)\\s*:\\s*"
                    // All of the below are just decimal digits separated by spaces
                    "(?<receive_bytes>\\d+)\\s+"
                    "(?<receive_packets>\\d+)\\s+"
                    "(?<receive_errors>\\d+)\\s+"
                    "(?<receive_drop>\\d+)\\s+"
                    "(?<receive_fifo>\\d+)\\s+"
                    "(?<receive_frame>\\d+)\\s+"
                    "(?<receive_compressed>\\d+)\\s+"
                    "(?<receive_multicast>\\d+)\\s+"
                    "(?<transmit_bytes>\\d+)\\s+"
                    "(?<transmit_packets>\\d+)\\s+"
                    "(?<transmit_errors>\\d+)\\s+"
                    "(?<transmit_drop>\\d+)\\s+"
                    "(?<transmit_fifo>\\d+)\\s+"
                    "(?<transmit_frame>\\d+)\\s+"
                    "(?<transmit_compressed>\\d+)\\s+"
                    "(?<transmit_multicast>\\d+)");
    JsonDestroy(interfaces_data);
    BufferDestroy(pbuf);
}

JsonElement* GetNetworkingConnections(EvalContext *ctx)
{
    const char *procdir = GetNetworkingProcdir();
    JsonElement *json = JsonObjectCreate(5);
    const char* ports_regex = "^\\s*\\d+:\\s+(?<raw_local>[0-9A-F:]+)\\s+(?<raw_remote>[0-9A-F:]+)\\s+(?<raw_state>[0-9]+)";

    JsonElement *data = NULL;
    Buffer *pbuf = BufferNew();

    BufferPrintf(pbuf, "%s/proc/net/tcp", procdir);
    data = GetProcFileInfo(ctx, BufferData(pbuf), NULL, NULL, (ProcPostProcessFn) &NetworkingPortsPostProcessInfo, ports_regex);
    if (data != NULL)
    {
        JsonObjectAppendElement(json, "tcp", data);
    }

    BufferPrintf(pbuf, "%s/proc/net/tcp6", procdir);
    data = GetProcFileInfo(ctx, BufferData(pbuf), NULL, NULL, (ProcPostProcessFn) &NetworkingPortsPostProcessInfo, ports_regex);
    if (data != NULL)
    {
        JsonObjectAppendElement(json, "tcp6", data);
    }

    BufferPrintf(pbuf, "%s/proc/net/udp", procdir);
    data = GetProcFileInfo(ctx, BufferData(pbuf), NULL, NULL, (ProcPostProcessFn) &NetworkingPortsPostProcessInfo, ports_regex);
    if (data != NULL)
    {
        JsonObjectAppendElement(json, "udp", data);
    }

    BufferPrintf(pbuf, "%s/proc/net/udp6", procdir);
    data = GetProcFileInfo(ctx, BufferData(pbuf), NULL, NULL, (ProcPostProcessFn) &NetworkingPortsPostProcessInfo, ports_regex);
    if (data != NULL)
    {
        JsonObjectAppendElement(json, "udp6", data);
    }

    if (JsonLength(json) < 1)
    {
        // nothing was collected, this is a failure
        JsonDestroy(json);
        return NULL;
    }

    return json;
}

#endif /* !__MINGW32__ */
