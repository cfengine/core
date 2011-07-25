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

/*****************************************************************************/
/*                                                                           */
/* File: unix.c                                                              */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#ifndef MINGW

#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
# ifdef _SIZEOF_ADDR_IFREQ
#  define SIZEOF_IFREQ(x) _SIZEOF_ADDR_IFREQ(x)
# else
#  define SIZEOF_IFREQ(x) \
          ((x).ifr_addr.sa_len > sizeof(struct sockaddr) ? \
           (sizeof(struct ifreq) - sizeof(struct sockaddr) + \
            (x).ifr_addr.sa_len) : sizeof(struct ifreq))
# endif
#else
# define SIZEOF_IFREQ(x) sizeof(struct ifreq)
#endif

/*****************************************************************************/
/* newly created, used in timeout.c and transaction.c */

int Unix_GracefulTerminate(pid_t pid)

{ int res;
 
if ((res = kill(pid,SIGINT)) == -1)
   {
   sleep(1);
   res = 0;
   
   if ((res = kill(pid,SIGTERM)) == -1)
      {   
      sleep(5);
      res = 0;
      
      if ((res = kill(pid,SIGKILL)) == -1)
         {
         sleep(1);
         }
      }
   }

return (res == 0);
}

/*************************************************************/

int Unix_GetCurrentUserName(char *userName, int userNameLen)
{
struct passwd *user_ptr;

memset(userName, 0, userNameLen);
user_ptr = getpwuid(getuid());

if(user_ptr == NULL)
  {
  CfOut(cf_error,"getpwuid","Could not get user name of current process, using \"UNKNOWN\"");
  strncpy(userName, "UNKNOWN", userNameLen - 1);
  return false;
  }

strncpy(userName, user_ptr->pw_name, userNameLen - 1);
return true;
}

/*************************************************************/

char *Unix_GetErrorStr(void)
{
return strerror(errno);
}

/*************************************************************/

/* from exec_tools.c */

int Unix_IsExecutable(const char *file)

{ struct stat sb;
  gid_t grps[NGROUPS];
  int i,n;

if (cfstat(file,&sb) == -1)
   {
   CfOut(cf_error,"","Proposed executable file \"%s\" doesn't exist",file);
   return false;
   }

if (sb.st_mode & 02)
   {
   CfOut(cf_error,""," !! SECURITY ALERT: promised executable \"%s\" is world writable! ",file);
   CfOut(cf_error,""," !! SECURITY ALERT: cfengine will not execute this - requires human inspection");
   return false;
   }

if (getuid() == sb.st_uid || getuid() == 0)
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
   
   if ((n = getgroups(NGROUPS,grps)) > 0)
      {
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

/*******************************************************************/

/* from exec_tools.c */

int Unix_ShellCommandReturnsZero(char *comm,int useshell)

{ int status, i, argc = 0;
  pid_t pid;
  char arg[CF_MAXSHELLARGS][CF_BUFSIZE];
  char **argv;
  char esc_command[CF_BUFSIZE];

if (!useshell)
   {
   /* Build argument array */

   for (i = 0; i < CF_MAXSHELLARGS; i++)
      {
      memset (arg[i],0,CF_BUFSIZE);
      }

   argc = ArgSplitCommand(comm,arg);

   if (argc == -1)
      {
      CfOut(cf_error,"","Too many arguments in %s\n",comm);
      return false;
      }
   }

if ((pid = fork()) < 0)
   {
   FatalError("Failed to fork new process");
   }
else if (pid == 0)                     /* child */
   {
   ALARM_PID = -1;

   if (useshell)
      {
      strncpy(esc_command,ShEscapeCommand(comm),CF_BUFSIZE-1);

      if (execl("/bin/sh","sh","-c",esc_command,NULL) == -1)
         {
         CfOut(cf_error,"execl","Command %s failed",esc_command);
         exit(1);
         }
      }
   else
      {      
      argv = (char **) malloc((argc+1)*sizeof(char *));

      if (argv == NULL)
         {
         FatalError("Out of memory");
         }

      for (i = 0; i < argc; i++)
         {
         argv[i] = arg[i];
         }

      argv[i] = (char *) NULL;

      if (execv(arg[0],argv) == -1)
         {
         CfOut(cf_error,"execv","Command %s failed (%d args)",argv[0],argc - 1);
         exit(1);
         }

      free((char *)argv);
      }
   }
else                                    /* parent */
   {
#ifndef HAVE_WAITPID
   pid_t wait_result;
#endif
   ALARM_PID = pid;

#ifdef HAVE_WAITPID
   
   while(waitpid(pid,&status,0) < 0)
      {
      if (errno != EINTR)
         {
         return -1;
         }
      }

   return (WEXITSTATUS(status) == 0);
   
#else
   
   while ((wait_result = wait(&status)) != pid)
      {
      if (wait_result <= 0)
         {
         CfOut(cf_inform,"wait"," !! Wait for child failed\n");
         return false;
         }
      }
   
   if (WIFSIGNALED(status))
      {
      return false;
      }
   
   if (! WIFEXITED(status))
      {
      return false;
      }
   
   return (WEXITSTATUS(status) == 0);
#endif
   }

return false;
}

/**********************************************************************************/

/* from verify_processes.c */

int Unix_DoAllSignals(struct Item *siglist,struct Attributes a,struct Promise *pp)

{ struct Item *ip;
  struct Rlist *rp;
  pid_t pid;
  int killed = false;

Debug("DoSignals(%s)\n",pp->promiser);
  
if (siglist == NULL)
   {
   return 0;
   }

if (a.signals == NULL)
   {
   CfOut(cf_verbose,""," -> No signals to send for %s\n",pp->promiser);
   return 0;
   }

for (ip = siglist; ip != NULL; ip=ip->next)
   {
   pid = ip->counter;
   
   for (rp = a.signals; rp != NULL; rp=rp->next)
      {
      int signal = Signal2Int(rp->item);
      
      if (!DONTDO)
         {         
         if (signal == SIGKILL || signal == SIGTERM)
            {
            killed = true;
            }
         
         if (kill((pid_t)pid,signal) < 0)
            {
            cfPS(cf_verbose,CF_FAIL,"kill",pp,a," !! Couldn't send promised signal \'%s\' (%d) to pid %d (might be dead)\n",rp->item,signal,pid);
            }
         else
            {
            cfPS(cf_inform,CF_CHG,"",pp,a," -> Signalled '%s' (%d) to process %d (%s)\n", rp->item, signal, pid, ip->name);
            }
         }
      else
         {
         CfOut(cf_error,""," -> Need to keep signal promise \'%s\' in process entry %s",rp->item,ip->name);
         }
      }
   }

return killed;
}

/*******************************************************************/

/* from verify_processes.c */

int Unix_LoadProcessTable(struct Item **procdata)

{ FILE *prp;
  char pscomm[CF_MAXLINKSIZE], vbuff[CF_BUFSIZE], *sp;
  struct Item *rootprocs = NULL;
  struct Item *otherprocs = NULL;
  const char *psopts = GetProcessOptions();

snprintf(pscomm,CF_MAXLINKSIZE,"%s %s",VPSCOMM[VSYSTEMHARDCLASS],psopts);

CfOut(cf_verbose,"","Observe process table with %s\n",pscomm); 
  
if ((prp = cf_popen(pscomm,"r")) == NULL)
   {
   CfOut(cf_error,"popen","Couldn't open the process list with command %s\n",pscomm);
   return false;
   }

while (!feof(prp))
   {
   memset(vbuff,0,CF_BUFSIZE);
   CfReadLine(vbuff,CF_BUFSIZE,prp);

   for (sp = vbuff+strlen(vbuff)-1; sp > vbuff && isspace(*sp); sp--)
      {
      *sp = '\0';
      }
   
   if (ForeignZone(vbuff))
      {
      continue;
      }

   AppendItem(procdata,vbuff,"");
   }

cf_pclose(prp);

/* Now save the data */

snprintf(vbuff,CF_MAXVARSIZE,"%s/state/cf_procs",CFWORKDIR);
RawSaveItemList(*procdata,vbuff);

CopyList(&rootprocs,*procdata);
CopyList(&otherprocs,*procdata);

while (DeleteItemNotContaining(&rootprocs,"root"))
   {
   }

while (DeleteItemContaining(&otherprocs,"root"))
   {
   }

if (otherprocs)
   {
   PrependItem(&rootprocs,otherprocs->name,NULL);
   }

snprintf(vbuff,CF_MAXVARSIZE,"%s/state/cf_rootprocs",CFWORKDIR);
RawSaveItemList(rootprocs,vbuff);
DeleteItemList(rootprocs);

snprintf(vbuff,CF_MAXVARSIZE,"%s/state/cf_otherprocs",CFWORKDIR);
RawSaveItemList(otherprocs,vbuff);
DeleteItemList(otherprocs);

return true;
}

/*********************************************************************/

/* from files_operators.c */

void Unix_CreateEmptyFile(char *name)

{ int tempfd;

if (unlink(name) == -1)
   {
   Debug("Pre-existing object %s could not be removed or was not there\n",name);
   }

if ((tempfd = open(name, O_CREAT|O_EXCL|O_WRONLY,0600)) < 0)
   {
   CfOut(cf_error,"open","Couldn't open a file %s\n",name);  
   }

close(tempfd);
}

/******************************************************************/

static bool IgnoreInterface(int ifaceidx, struct sockaddr_in *inaddr)
{
/* FreeBSD jails */
#ifdef HAVE_JAIL_GET
struct iovec fbsd_jparams[4];
struct in_addr fbsd_jia;
int fbsd_lastjid = 0;

*(const void **)&fbsd_jparams[0].iov_base = "lastjid";
fbsd_jparams[0].iov_len = sizeof("lastjid");
fbsd_jparams[1].iov_base = &fbsd_lastjid;
fbsd_jparams[1].iov_len = sizeof(fbsd_lastjid);

*(const void **)&fbsd_jparams[2].iov_base = "ip4.addr";
fbsd_jparams[2].iov_len = sizeof("ip4.addr");
fbsd_jparams[3].iov_len = sizeof(struct in_addr);
fbsd_jparams[3].iov_base = &fbsd_jia;

while ((fbsd_lastjid = jail_get(fbsd_jparams, 4, 0)) > 0)
   {
   if (fbsd_jia.s_addr == inaddr->sin_addr.s_addr)
      {
      CfOut(cf_verbose,"","Interface %d belongs to a FreeBSD jail %s\n",
            ifaceidx, inet_ntoa(fbsd_jia));
      return true;
      }
   }
#endif

return false;
}

/******************************************************************/

void Unix_GetInterfaceInfo(enum cfagenttype ag)

{ int fd,len,i,j,first_address = false,ipdefault = false;
  struct ifreq ifbuf[CF_IFREQ],ifr, *ifp;
  struct ifconf list;
  struct sockaddr_in *sin;
  struct hostent *hp;
  char *sp, workbuf[CF_BUFSIZE];
  char ip[CF_MAXVARSIZE];
  char name[CF_MAXVARSIZE];
  char last_name[CF_BUFSIZE];

Debug("Unix_GetInterfaceInfo()\n");

memset(ifbuf, 0, sizeof(ifbuf));

last_name[0] = '\0';

if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
   {
   CfOut(cf_error,"socket","Couldn't open socket");
   exit(1);
   }

list.ifc_len = sizeof(ifbuf);
list.ifc_req = ifbuf;

#ifdef SIOCGIFCONF
if (ioctl(fd, SIOCGIFCONF, &list) == -1 || (list.ifc_len < (sizeof(struct ifreq))))
#else
if (ioctl(fd, OSIOCGIFCONF, &list) == -1 || (list.ifc_len < (sizeof(struct ifreq))))
#endif
   {
   CfOut(cf_error,"ioctl","Couldn't get interfaces - old kernel? Try setting CF_IFREQ to 1024");
   exit(1);
   }

last_name[0] = '\0';

for (j = 0,len = 0,ifp = list.ifc_req; len < list.ifc_len; len+=SIZEOF_IFREQ(*ifp),j++,ifp=(struct ifreq *)((char *)ifp+SIZEOF_IFREQ(*ifp)))
   {

   if (ifp->ifr_addr.sa_family == 0)
      {
      continue;
      }

   if (ifp->ifr_name == NULL || strlen(ifp->ifr_name) == 0)
      {
      continue;
      }

   /* Skip virtual network interfaces for Linux, which seems to be a problem */
   
   if (strstr(ifp->ifr_name,":"))
      {
      if (VSYSTEMHARDCLASS == linuxx)
         {
         CfOut(cf_verbose,"","Skipping apparent virtual interface %d: %s\n",j+1,ifp->ifr_name);
         continue;
         }
      }
   else
      {
      CfOut(cf_verbose,"","Interface %d: %s\n",j+1,ifp->ifr_name);
      }

   // Ignore the loopback
   
   if (strcmp(ifp->ifr_name,"lo") == 0)
      {
      continue;
      }
   
   if (strncmp(last_name,ifp->ifr_name,sizeof(ifp->ifr_name)) == 0)
      {
      first_address = false;
      }
   else
      {
      strncpy(last_name,ifp->ifr_name,sizeof(ifp->ifr_name));

      if (!first_address)
         {
         NewScalar("sys","interface",last_name,cf_str);
         first_address = true;
         }
      }

   snprintf(workbuf,CF_BUFSIZE, "net_iface_%s", CanonifyName(ifp->ifr_name));

   NewClass(workbuf);

   if (ifp->ifr_addr.sa_family == AF_INET)
      {
      strncpy(ifr.ifr_name,ifp->ifr_name,sizeof(ifp->ifr_name));

      if (ioctl(fd,SIOCGIFFLAGS,&ifr) == -1)
         {
         CfOut(cf_error,"ioctl","No such network device");
         //close(fd);
         //return;
         continue;
         }

      if ((ifr.ifr_flags & IFF_BROADCAST) && !(ifr.ifr_flags & IFF_LOOPBACK))
         {
         sin=(struct sockaddr_in *)&ifp->ifr_addr;

         if (IgnoreInterface(j + 1, sin))
            {
            CfOut(cf_verbose, "", "Ignoring interface %d", j + 1);
            continue;
            }

         Debug("Adding hostip %s..\n",inet_ntoa(sin->sin_addr));
         NewClass(inet_ntoa(sin->sin_addr));

         if ((hp = gethostbyaddr((char *)&(sin->sin_addr.s_addr),sizeof(sin->sin_addr.s_addr),AF_INET)) == NULL)
            {
            Debug("No hostinformation for %s not found\n", inet_ntoa(sin->sin_addr));
            }
         else
            {
            if (hp->h_name != NULL)
               {
               Debug("Adding hostname %s..\n",hp->h_name);
               NewClass(hp->h_name);

               if (hp->h_aliases != NULL)
                  {
                  for (i=0; hp->h_aliases[i] != NULL; i++)
                     {
                     CfOut(cf_verbose,"","Adding alias %s..\n",hp->h_aliases[i]);
                     NewClass(hp->h_aliases[i]);
                     }
                  }
               }
            }

         if (strcmp(inet_ntoa(sin->sin_addr),"0.0.0.0") == 0)
            {
            // Maybe we need to do something windows specific here?
            CfOut(cf_verbose,""," !! Cannot discover hardware IP, using DNS value");
            strcpy(ip,"ipv4_");
            strcat(ip,VIPADDRESS);
            AppendItem(&IPADDRESSES,VIPADDRESS,"");

            for (sp = ip+strlen(ip)-1; (sp > ip); sp--)
               {
               if (*sp == '.')
                  {
                  *sp = '\0';
                  NewClass(ip);
                  }
               }

            strcpy(ip,VIPADDRESS);
            i = 3;

            for (sp = ip+strlen(ip)-1; (sp > ip); sp--)
               {
               if (*sp == '.')
                  {
                  *sp = '\0';
                  snprintf(name,CF_MAXVARSIZE-1,"ipv4_%d[%s]",i--,CanonifyName(VIPADDRESS));
                  NewScalar("sys",name,ip,cf_str);
                  }
               }
            //close(fd);
            //return;
            continue;
            }

         strncpy(ip,"ipv4_",CF_MAXVARSIZE);
         strncat(ip,inet_ntoa(sin->sin_addr),CF_MAXVARSIZE-6);
         NewClass(ip);         

         if (!ipdefault)
            {
            ipdefault = true;
            NewScalar("sys","ipv4",inet_ntoa(sin->sin_addr),cf_str);

            strcpy(VIPADDRESS,inet_ntoa(sin->sin_addr));
            }

         AppendItem(&IPADDRESSES,inet_ntoa(sin->sin_addr),"");

         for (sp = ip+strlen(ip)-1; (sp > ip); sp--)
            {
            if (*sp == '.')
               {
               *sp = '\0';
               NewClass(ip);
               }
            }

         strcpy(ip,inet_ntoa(sin->sin_addr));

         if (ag != cf_know)
            {
            snprintf(name,CF_MAXVARSIZE-1,"ipv4[%s]",CanonifyName(ifp->ifr_name));
            }
         else
            {
            snprintf(name,CF_MAXVARSIZE-1,"ipv4[interface_name]");
            }
         
         NewScalar("sys",name,ip,cf_str);

         i = 3;

         for (sp = ip+strlen(ip)-1; (sp > ip); sp--)
            {
            if (*sp == '.')
               {
               *sp = '\0';
               
               if (ag != cf_know)
                  {                  
                  snprintf(name,CF_MAXVARSIZE-1,"ipv4_%d[%s]",i--,CanonifyName(ifp->ifr_name));
                  }
               else
                  {
                  snprintf(name,CF_MAXVARSIZE-1,"ipv4_%d[interface_name]",i--);
                  }
               
               NewScalar("sys",name,ip,cf_str);
               }
            }
         }
      }
   }

close(fd);
}

/*******************************************************************/

void Unix_FindV6InterfaceInfo(void)

{ FILE *pp = NULL;
  char buffer[CF_BUFSIZE];

/* Whatever the manuals might say, you cannot get IPV6
   interface configuration from the ioctls. This seems
   to be implemented in a non standard way across OSes
   BSDi has done getifaddrs(), solaris 8 has a new ioctl, Stevens
   book shows the suggestion which has not been implemented...
*/

 CfOut(cf_verbose,"","Trying to locate my IPv6 address\n");

 switch (VSYSTEMHARDCLASS)
    {
    case cfnt:
        /* NT cannot do this */
        return;

    case irix:
    case irix4:
    case irix64:

        if ((pp = cf_popen("/usr/etc/ifconfig -a","r")) == NULL)
           {
           CfOut(cf_verbose,"","Could not find interface info\n");
           return;
           }

        break;

    case hp:

        if ((pp = cf_popen("/usr/sbin/ifconfig -a","r")) == NULL)
           {
           CfOut(cf_verbose,"","Could not find interface info\n");
           return;
           }

        break;

    case aix:

        if ((pp = cf_popen("/etc/ifconfig -a","r")) == NULL)
           {
           CfOut(cf_verbose,"","Could not find interface info\n");
           return;
           }

        break;

    default:

        if ((pp = cf_popen("/sbin/ifconfig -a","r")) == NULL)
           {
           CfOut(cf_verbose,"","Could not find interface info\n");
           return;
           }

    }

/* Don't know the output format of ifconfig on all these .. hope for the best*/

while (!feof(pp))
   {
   fgets(buffer,CF_BUFSIZE-1,pp);

   if (ferror(pp))  /* abortable */
      {
      break;
      }

   if (StrStr(buffer,"inet6"))
      {
      struct Item *ip,*list = NULL;
      char *sp;

      list = SplitStringAsItemList(buffer,' ');

      for (ip = list; ip != NULL; ip=ip->next)
         {
         for (sp = ip->name; *sp != '\0'; sp++)
            {
            if (*sp == '/')  /* Remove CIDR mask */
               {
               *sp = '\0';
               }
            }

         if (IsIPV6Address(ip->name) && (strcmp(ip->name,"::1") != 0))
            {
            CfOut(cf_verbose,"","Found IPv6 address %s\n",ip->name);
            AppendItem(&IPADDRESSES,ip->name,"");
            NewClass(ip->name);
            }
         }

      DeleteItemList(list);
      }
   }

cf_pclose(pp);
}


#endif  /* NOT MINGW */
