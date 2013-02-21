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

#include "unix.h"

#include "env_context.h"
#include "vars.h"
#include "files_names.h"
#include "files_interfaces.h"
#include "item_lib.h"
#include "conversion.h"
#include "matching.h"
#include "cfstream.h"
#include "communication.h"
#include "pipes.h"
#include "logging.h"
#include "exec_tools.h"
#include "misc_lib.h"
#include "rlist.h"

#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#ifdef HAVE_SYS_JAIL_H
# include <sys/jail.h>
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

static void FindV6InterfacesInfo(void);

static bool IgnoreJailInterface(int ifaceidx, struct sockaddr_in *inaddr);
static bool IgnoreInterface(char *name);
static void InitIgnoreInterfaces(void);

static Rlist *IGNORE_INTERFACES = NULL;


/*****************************************************************************/
/* newly created, used in timeout.c and transaction.c */

int GracefulTerminate(pid_t pid)
{
    int res;

    if ((res = kill(pid, SIGINT)) == -1)
    {
        sleep(1);
        res = 0;

        if ((res = kill(pid, SIGTERM)) == -1)
        {
            sleep(5);
            res = 0;

            if ((res = kill(pid, SIGKILL)) == -1)
            {
                sleep(1);
            }
        }
    }

    return (res == 0);
}

/*************************************************************/

void ProcessSignalTerminate(pid_t pid)
{
    if(!IsProcessRunning(pid))
    {
        return;
    }


    if(kill(pid, SIGINT) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "kill", "!! Could not send SIGINT to pid %" PRIdMAX , (intmax_t)pid);
    }

    sleep(1);


    if(kill(pid, SIGTERM) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "kill", "!! Could not send SIGTERM to pid %" PRIdMAX , (intmax_t)pid);
    }

    sleep(5);


    if(kill(pid, SIGKILL) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "kill", "!! Could not send SIGKILL to pid %" PRIdMAX , (intmax_t)pid);
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

    CfOut(OUTPUT_LEVEL_ERROR, "kill", "!! Failed checking for process existence");

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
        CfOut(OUTPUT_LEVEL_ERROR, "getpwuid", "Could not get user name of current process, using \"UNKNOWN\"");
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

    if (cfstat(file, &sb) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Proposed executable file \"%s\" doesn't exist", file);
        return false;
    }

    if (sb.st_mode & 02)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! SECURITY ALERT: promised executable \"%s\" is world writable! ", file);
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! SECURITY ALERT: cfengine will not execute this - requires human inspection");
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

int ShellCommandReturnsZero(const char *comm, int useshell)
{
    int status;
    pid_t pid;

    if (!useshell)
    {
        /* Build argument array */

    }

    if ((pid = fork()) < 0)
    {
        FatalError("Failed to fork new process");
    }
    else if (pid == 0)          /* child */
    {
        ALARM_PID = -1;

        if (useshell)
        {
            if (execl(SHELL_PATH, "sh", "-c", comm, NULL) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "execl", "Command %s failed", comm);
                exit(1);
            }
        }
        else
        {
            char **argv = ArgSplitCommand(comm);

            if (execv(argv[0], argv) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "execv", "Command %s failed", argv[0]);
                exit(1);
            }
        }
    }
    else                        /* parent */
    {
# ifndef HAVE_WAITPID
        pid_t wait_result;
# endif
        ALARM_PID = pid;

# ifdef HAVE_WAITPID

        while (waitpid(pid, &status, 0) < 0)
        {
            if (errno != EINTR)
            {
                return -1;
            }
        }

        return (WEXITSTATUS(status) == 0);

# else

        while ((wait_result = wait(&status)) != pid)
        {
            if (wait_result <= 0)
            {
                CfOut(OUTPUT_LEVEL_INFORM, "wait", " !! Wait for child failed\n");
                return false;
            }
        }

        if (WIFSIGNALED(status))
        {
            return false;
        }

        if (!WIFEXITED(status))
        {
            return false;
        }

        return (WEXITSTATUS(status) == 0);
# endif
    }

    return false;
}

/*********************************************************************/


/******************************************************************/

static bool IgnoreJailInterface(int ifaceidx, struct sockaddr_in *inaddr)
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
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Interface %d belongs to a FreeBSD jail %s\n", ifaceidx, inet_ntoa(fbsd_jia));
            return true;
        }
    }
# endif

    return false;
}

/******************************************************************/

static void GetMacAddress(AgentType ag, int fd, struct ifreq *ifr, struct ifreq *ifp, Rlist **interfaces,
                          Rlist **hardware)
{
    char name[CF_MAXVARSIZE];

    if (ag != AGENT_TYPE_GENDOC)
    {
        snprintf(name, CF_MAXVARSIZE, "hardware_mac[%s]", ifp->ifr_name);
    }
    else
    {
        snprintf(name, CF_MAXVARSIZE, "hardware_mac[interface_name]");
    }

# if defined(SIOCGIFHWADDR) && defined(HAVE_STRUCT_IFREQ_IFR_HWADDR)
    char hw_mac[CF_MAXVARSIZE];

    
    ioctl(fd, SIOCGIFHWADDR, ifr);
    snprintf(hw_mac, CF_MAXVARSIZE - 1, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
             (unsigned char) ifr->ifr_hwaddr.sa_data[0],
             (unsigned char) ifr->ifr_hwaddr.sa_data[1],
             (unsigned char) ifr->ifr_hwaddr.sa_data[2],
             (unsigned char) ifr->ifr_hwaddr.sa_data[3],
             (unsigned char) ifr->ifr_hwaddr.sa_data[4], (unsigned char) ifr->ifr_hwaddr.sa_data[5]);

    NewScalar("sys", name, hw_mac, DATA_TYPE_STRING);
    RlistAppend(hardware, hw_mac, RVAL_TYPE_SCALAR);
    RlistAppend(interfaces, ifp->ifr_name, RVAL_TYPE_SCALAR);

    snprintf(name, CF_MAXVARSIZE, "mac_%s", CanonifyName(hw_mac));
    HardClass(name);
# else
    NewScalar("sys", name, "mac_unknown", DATA_TYPE_STRING);
    HardClass("mac_unknown");
# endif
}

/******************************************************************/

void GetInterfacesInfo(AgentType ag)
{
    int fd, len, i, j, first_address = false, ipdefault = false;
    struct ifreq ifbuf[CF_IFREQ], ifr, *ifp;
    struct ifconf list;
    struct sockaddr_in *sin;
    struct hostent *hp;
    char *sp, workbuf[CF_BUFSIZE];
    char ip[CF_MAXVARSIZE];
    char name[CF_MAXVARSIZE];
    char last_name[CF_BUFSIZE];
    Rlist *interfaces = NULL, *hardware = NULL, *ips = NULL;

    CfDebug("GetInterfacesInfo()\n");

    // Long-running processes may call this many times
    DeleteItemList(IPADDRESSES);
    IPADDRESSES = NULL;

    memset(ifbuf, 0, sizeof(ifbuf));

    InitIgnoreInterfaces();
    
    last_name[0] = '\0';

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "socket", "Couldn't open socket");
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
        CfOut(OUTPUT_LEVEL_ERROR, "ioctl", "Couldn't get interfaces - old kernel? Try setting CF_IFREQ to 1024");
        exit(1);
    }

    last_name[0] = '\0';

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
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Skipping apparent virtual interface %d: %s\n", j + 1, ifp->ifr_name);
            continue;
#endif
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "Interface %d: %s\n", j + 1, ifp->ifr_name);
        }

        // Ignore the loopback

        if (strcmp(ifp->ifr_name, "lo") == 0)
        {
            continue;
        }

        if (strncmp(last_name, ifp->ifr_name, sizeof(ifp->ifr_name)) == 0)
        {
            first_address = false;
        }
        else
        {
            strncpy(last_name, ifp->ifr_name, sizeof(ifp->ifr_name));

            if (!first_address)
            {
                NewScalar("sys", "interface", last_name, DATA_TYPE_STRING);
                first_address = true;
            }
        }

        snprintf(workbuf, CF_BUFSIZE, "net_iface_%s", CanonifyName(ifp->ifr_name));

        HardClass(workbuf);

        if (ifp->ifr_addr.sa_family == AF_INET)
        {
            strncpy(ifr.ifr_name, ifp->ifr_name, sizeof(ifp->ifr_name));

            if (ioctl(fd, SIOCGIFFLAGS, &ifr) == -1)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "ioctl", "No such network device");
                continue;
            }

            if ((ifr.ifr_flags & IFF_UP) && (!(ifr.ifr_flags & IFF_LOOPBACK)))
            {
                sin = (struct sockaddr_in *) &ifp->ifr_addr;

                if (IgnoreJailInterface(j + 1, sin))
                {
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Ignoring interface %d", j + 1);
                    continue;
                }

                CfDebug("Adding hostip %s..\n", inet_ntoa(sin->sin_addr));
                HardClass(inet_ntoa(sin->sin_addr));

                if ((hp =
                     gethostbyaddr((char *) &(sin->sin_addr.s_addr), sizeof(sin->sin_addr.s_addr), AF_INET)) == NULL)
                {
                    CfDebug("No hostinformation for %s found\n", inet_ntoa(sin->sin_addr));
                }
                else
                {
                    if (hp->h_name != NULL)
                    {
                        CfDebug("Adding hostname %s..\n", hp->h_name);
                        HardClass(hp->h_name);

                        if (hp->h_aliases != NULL)
                        {
                            for (i = 0; hp->h_aliases[i] != NULL; i++)
                            {
                                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Adding alias %s..\n", hp->h_aliases[i]);
                                HardClass(hp->h_aliases[i]);
                            }
                        }
                    }
                }

                if (strcmp(inet_ntoa(sin->sin_addr), "0.0.0.0") == 0)
                {
                    // Maybe we need to do something windows specific here?
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Cannot discover hardware IP, using DNS value");
                    strcpy(ip, "ipv4_");
                    strcat(ip, VIPADDRESS);
                    AppendItem(&IPADDRESSES, VIPADDRESS, "");
                    RlistAppend(&ips, VIPADDRESS, RVAL_TYPE_SCALAR);

                    for (sp = ip + strlen(ip) - 1; (sp > ip); sp--)
                    {
                        if (*sp == '.')
                        {
                            *sp = '\0';
                            HardClass(ip);
                        }
                    }

                    strcpy(ip, VIPADDRESS);
                    i = 3;

                    for (sp = ip + strlen(ip) - 1; (sp > ip); sp--)
                    {
                        if (*sp == '.')
                        {
                            *sp = '\0';
                            snprintf(name, CF_MAXVARSIZE - 1, "ipv4_%d[%s]", i--, CanonifyName(VIPADDRESS));
                            NewScalar("sys", name, ip, DATA_TYPE_STRING);
                        }
                    }
                    continue;
                }

                strncpy(ip, "ipv4_", CF_MAXVARSIZE);
                strncat(ip, inet_ntoa(sin->sin_addr), CF_MAXVARSIZE - 6);
                HardClass(ip);

                if (!ipdefault)
                {
                    ipdefault = true;
                    NewScalar("sys", "ipv4", inet_ntoa(sin->sin_addr), DATA_TYPE_STRING);

                    strcpy(VIPADDRESS, inet_ntoa(sin->sin_addr));
                }

                AppendItem(&IPADDRESSES, inet_ntoa(sin->sin_addr), "");
                RlistAppend(&ips, inet_ntoa(sin->sin_addr), RVAL_TYPE_SCALAR);

                for (sp = ip + strlen(ip) - 1; (sp > ip); sp--)
                {
                    if (*sp == '.')
                    {
                        *sp = '\0';
                        HardClass(ip);
                    }
                }

                // Set the IPv4 on interface array

                strcpy(ip, inet_ntoa(sin->sin_addr));

                if (ag != AGENT_TYPE_GENDOC)
                {
                    snprintf(name, CF_MAXVARSIZE - 1, "ipv4[%s]", CanonifyName(ifp->ifr_name));
                }
                else
                {
                    snprintf(name, CF_MAXVARSIZE - 1, "ipv4[interface_name]");
                }

                NewScalar("sys", name, ip, DATA_TYPE_STRING);

                i = 3;

                for (sp = ip + strlen(ip) - 1; (sp > ip); sp--)
                {
                    if (*sp == '.')
                    {
                        *sp = '\0';

                        if (ag != AGENT_TYPE_GENDOC)
                        {
                            snprintf(name, CF_MAXVARSIZE - 1, "ipv4_%d[%s]", i--, CanonifyName(ifp->ifr_name));
                        }
                        else
                        {
                            snprintf(name, CF_MAXVARSIZE - 1, "ipv4_%d[interface_name]", i--);
                        }

                        NewScalar("sys", name, ip, DATA_TYPE_STRING);
                    }
                }
            }

            // Set the hardware/mac address array
            GetMacAddress(ag, fd, &ifr, ifp, &interfaces, &hardware);
        }
    }

    close(fd);

    NewList("sys", "interfaces", interfaces, DATA_TYPE_STRING_LIST);
    NewList("sys", "hardware_addresses", hardware, DATA_TYPE_STRING_LIST);
    NewList("sys", "ip_addresses", ips, DATA_TYPE_STRING_LIST);

    RlistDestroy(interfaces);
    RlistDestroy(hardware);
    RlistDestroy(ips);

    FindV6InterfacesInfo();
}

/*******************************************************************/

static void FindV6InterfacesInfo(void)
{
    FILE *pp = NULL;
    char buffer[CF_BUFSIZE];

/* Whatever the manuals might say, you cannot get IPV6
   interface configuration from the ioctls. This seems
   to be implemented in a non standard way across OSes
   BSDi has done getifaddrs(), solaris 8 has a new ioctl, Stevens
   book shows the suggestion which has not been implemented...
*/

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Trying to locate my IPv6 address\n");

#if defined(__CYGWIN__)
    /* NT cannot do this */
    return;
#elif defined(__hpux)
    if ((pp = cf_popen("/usr/sbin/ifconfig -a", "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not find interface info\n");
        return;
    }
#elif defined(_AIX)
    if ((pp = cf_popen("/etc/ifconfig -a", "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not find interface info\n");
        return;
    }
#else
    if ((pp = cf_popen("/sbin/ifconfig -a", "r")) == NULL)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Could not find interface info\n");
        return;
    }
#endif

/* Don't know the output format of ifconfig on all these .. hope for the best*/

    while (!feof(pp))
    {
        buffer[0] = '\0';
        if (fgets(buffer, CF_BUFSIZE, pp) == NULL)
        {
            if (strlen(buffer))
            {
                UnexpectedError("Failed to read line from stream");
            }
        }

        if (ferror(pp))         /* abortable */
        {
            break;
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
                    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Found IPv6 address %s\n", ip->name);
                    AppendItem(&IPADDRESSES, ip->name, "");
                    HardClass(ip->name);
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
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> No interface exception file %s",filename);
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
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Ignoring interface \"%s\" because it matches %s",name,CF_IGNORE_INTERFACES);
            return true;
        }    
    }

    return false;
}



#endif /* !__MINGW32__ */
