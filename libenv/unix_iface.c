/*
   Copyright 2017 Northern.tech AS

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
#include <unix_iface.h>

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

#define IPV6_PREFIX "ipv6_"

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
                    EvalContextAddIpAddress(ctx, VIPADDRESS);
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

                EvalContextAddIpAddress(ctx, txtaddr);
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
                    char prefixed_ip[CF_MAX_IP_LEN + sizeof(IPV6_PREFIX)] = {0};
                    Log(LOG_LEVEL_VERBOSE, "Found IPv6 address %s", ip->name);
                    EvalContextAddIpAddress(ctx, ip->name);
                    EvalContextClassPutHard(ctx, ip->name, "inventory,attribute_name=none,source=agent");

                    xsnprintf(prefixed_ip, sizeof(prefixed_ip), IPV6_PREFIX"%s", ip->name);
                    EvalContextClassPutHard(ctx, prefixed_ip, "inventory,attribute_name=none,source=agent");
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

    snprintf(filename, sizeof(filename), "%s%cinputs%c%s", GetWorkDir(), FILE_SEPARATOR, FILE_SEPARATOR, CF_IGNORE_INTERFACES);

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

#endif /* !__MINGW32__ */
