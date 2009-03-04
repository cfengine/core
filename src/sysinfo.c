/* 
   Copyright (C) 2008 - Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version. 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

*/

/*****************************************************************************/
/*                                                                           */
/* File: sysinfo.c                                                           */
/*                                                                           */
/* Created: Sun Sep 30 14:14:47 2007                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#ifdef IRIX
#include <sys/syssgi.h>
#endif

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

/**********************************************************************/

void SetSignals()

{ int i;

 SIGNALS[SIGHUP] = strdup("SIGHUP");
 SIGNALS[SIGINT] = strdup("SIGINT");
 SIGNALS[SIGTRAP] = strdup("SIGTRAP");
 SIGNALS[SIGKILL] = strdup("SIGKILL");
 SIGNALS[SIGPIPE] = strdup("SIGPIPE");
 SIGNALS[SIGCONT] = strdup("SIGCONT");
 SIGNALS[SIGABRT] = strdup("SIGABRT");
 SIGNALS[SIGSTOP] = strdup("SIGSTOP");
 SIGNALS[SIGQUIT] = strdup("SIGQUIT");
 SIGNALS[SIGTERM] = strdup("SIGTERM");
 SIGNALS[SIGCHLD] = strdup("SIGCHLD");
 SIGNALS[SIGUSR1] = strdup("SIGUSR1");
 SIGNALS[SIGUSR2] = strdup("SIGUSR2");
 SIGNALS[SIGBUS] = strdup("SIGBUS");
 SIGNALS[SIGSEGV] = strdup("SIGSEGV");

 for (i = 0; i < highest_signal; i++)
    {
    if (SIGNALS[i] == NULL)
       {
       SIGNALS[i] = strdup("NOSIG");
       }
    }
}

/*******************************************************************/

void GetNameInfo3()

{ int i,found = false;
  char *sp,*sp2,workbuf[CF_BUFSIZE];
  time_t tloc;
  struct hostent *hp;
  struct sockaddr_in cin;
#ifdef AIX
  char real_version[_SYS_NMLN];
#endif
#ifdef IRIX
  char real_version[256]; /* see <sys/syssgi.h> */
#endif
#ifdef HAVE_SYSINFO
  long sz;
#endif

Debug("GetNameInfo()\n");
  
VFQNAME[0] = VUQNAME[0] = '\0';
  
if (uname(&VSYSNAME) == -1)
   {
   perror("uname ");
   FatalError("Uname couldn't get kernel name info!!\n");
   }

#ifdef AIX
snprintf(real_version,_SYS_NMLN,"%.80s.%.80s", VSYSNAME.version, VSYSNAME.release);
strncpy(VSYSNAME.release, real_version, _SYS_NMLN);
#elif defined IRIX
/* This gets us something like `6.5.19m' rather than just `6.5'.  */ 
 syssgi (SGI_RELEASE_NAME, 256, real_version);
#endif 

for (sp = VSYSNAME.sysname; *sp != '\0'; sp++)
   {
   *sp = ToLower(*sp);
   }

for (sp = VSYSNAME.machine; *sp != '\0'; sp++)
   {
   *sp = ToLower(*sp);
   }

#ifdef _AIX
switch (_system_configuration.architecture)
   {
   case POWER_RS:
      strncpy(VSYSNAME.machine, "power", _SYS_NMLN);
      break;
   case POWER_PC:
      strncpy(VSYSNAME.machine, "powerpc", _SYS_NMLN);
      break;
   case IA64:
      strncpy(VSYSNAME.machine, "ia64", _SYS_NMLN);
      break;
   }
#endif

for (i = 0; CLASSATTRIBUTES[i][0] != '\0'; i++)
   {
   if (FullTextMatch(CLASSATTRIBUTES[i][0],ToLowerStr(VSYSNAME.sysname)))
      {
      if (FullTextMatch(CLASSATTRIBUTES[i][1],VSYSNAME.machine))
         {
         if (FullTextMatch(CLASSATTRIBUTES[i][2],VSYSNAME.release))
            {
            if (UNDERSCORE_CLASSES)
               {
               snprintf(workbuf,CF_BUFSIZE,"_%s",CLASSTEXT[i]);
               NewClass(workbuf);
               }
            else
               {
               NewClass(CLASSTEXT[i]);
               }
            found = true;

            VSYSTEMHARDCLASS = (enum classes) i;
            NewScalar("sys","class",CLASSTEXT[i],cf_str);
            break;
            }
         }
      else
         {
         Debug2("Cfengine: I recognize %s but not %s\n",VSYSNAME.sysname,VSYSNAME.machine);
         continue;
         }
      }
   }

FindDomainName(VSYSNAME.nodename);

if (!StrStr(VSYSNAME.nodename,VDOMAIN))
   {
   snprintf(VFQNAME,CF_BUFSIZE,"%s.%s",VSYSNAME.nodename,ToLowerStr(VDOMAIN));
   NewClass(CanonifyName(VFQNAME));
   strcpy(VUQNAME,VSYSNAME.nodename);
   NewClass(CanonifyName(VUQNAME));
   }
else
   {
   int n = 0;
   strcpy(VFQNAME,VSYSNAME.nodename);
   NewClass(CanonifyName(VFQNAME));
   
   while(VSYSNAME.nodename[n++] != '.')
      {
      }
   
   strncpy(VUQNAME,VSYSNAME.nodename,n-1);
   NewClass(CanonifyName(VUQNAME));
   }
  
if ((tloc = time((time_t *)NULL)) == -1)
   {
   printf("Couldn't read system clock\n");
   }

if (UNDERSCORE_CLASSES)
   {
   snprintf(workbuf,CF_BUFSIZE,"_%s",CLASSTEXT[i]);
   }
else
   {
   snprintf(workbuf,CF_BUFSIZE,"%s",CLASSTEXT[i]);
   }

CfOut(cf_verbose,"","Cfengine - %s %s\n\n",VERSION,CF3COPYRIGHT);
CfOut(cf_verbose,"","------------------------------------------------------------------------\n\n");
CfOut(cf_verbose,"","Host name is: %s\n",VSYSNAME.nodename);
CfOut(cf_verbose,"","Operating System Type is %s\n",VSYSNAME.sysname);
CfOut(cf_verbose,"","Operating System Release is %s\n",VSYSNAME.release);
CfOut(cf_verbose,"","Architecture = %s\n\n\n",VSYSNAME.machine);
CfOut(cf_verbose,"","Using internal soft-class %s for host %s\n\n",workbuf,VSYSNAME.nodename);
CfOut(cf_verbose,"","The time is now %s\n\n",ctime(&tloc));
CfOut(cf_verbose,"","------------------------------------------------------------------------\n\n");

snprintf(workbuf,CF_MAXVARSIZE,"%s",ctime(&tloc));
NewScalar("sys","date",workbuf,cf_str);
NewScalar("sys","cdate",CanonifyName(workbuf),cf_str);
NewScalar("sys","host",VSYSNAME.nodename,cf_str);
NewScalar("sys","uqhost",VUQNAME,cf_str);
NewScalar("sys","fqhost",VFQNAME,cf_str);
NewScalar("sys","os",VSYSNAME.sysname,cf_str);
NewScalar("sys","release",VSYSNAME.release,cf_str);
NewScalar("sys","arch",VSYSNAME.machine,cf_str);
NewScalar("sys","workdir",CFWORKDIR,cf_str);
NewScalar("sys","fstab",VFSTAB[VSYSTEMHARDCLASS],cf_str);
NewScalar("sys","resolv",VRESOLVCONF[VSYSTEMHARDCLASS],cf_str);
NewScalar("sys","maildir",VMAILDIR[VSYSTEMHARDCLASS],cf_str);

if (strlen(VDOMAIN) > 0)
   {
   NewScalar("sys","domain",VDOMAIN,cf_str);
   }
else
   {
   NewScalar("sys","domain","undefined_domain",cf_str);
   NewClass("undefined_domain");
   }

sprintf(workbuf,"%d_bit",sizeof(long)*8);
NewClass(workbuf);
CfOut(cf_verbose,"","Additional hard class defined as: %s\n",CanonifyName(workbuf));

snprintf(workbuf,CF_BUFSIZE,"%s_%s",VSYSNAME.sysname,VSYSNAME.release);
NewClass(CanonifyName(workbuf));

#ifdef IRIX
/* Get something like `irix64_6_5_19m' defined as well as
   `irix64_6_5'.  Just copying the latter into VSYSNAME.release
   wouldn't be backwards-compatible.  */
snprintf(workbuf,CF_BUFSIZE,"%s_%s",VSYSNAME.sysname,real_version);
NewClass(CanonifyName(workbuf));
#endif

NewClass(CanonifyName(VSYSNAME.machine));
CfOut(cf_verbose,"","Additional hard class defined as: %s\n",CanonifyName(workbuf));

snprintf(workbuf,CF_BUFSIZE,"%s_%s",VSYSNAME.sysname,VSYSNAME.machine);
NewClass(CanonifyName(workbuf));
CfOut(cf_verbose,"","Additional hard class defined as: %s\n",CanonifyName(workbuf));

snprintf(workbuf,CF_BUFSIZE,"%s_%s_%s",VSYSNAME.sysname,VSYSNAME.machine,VSYSNAME.release);
NewClass(CanonifyName(workbuf));
CfOut(cf_verbose,"","Additional hard class defined as: %s\n",CanonifyName(workbuf));

#ifdef HAVE_SYSINFO
#ifdef SI_ARCHITECTURE
sz = sysinfo(SI_ARCHITECTURE,workbuf,CF_BUFSIZE);
if (sz == -1)
  {
  CfOut(cf_verbose,"","cfengine internal: sysinfo returned -1\n");
  }
else
  {
  NewClass(CanonifyName(workbuf));
  CfOut(cf_verbose,"","Additional hard class defined as: %s\n",workbuf);
  }
#endif
#ifdef SI_PLATFORM
sz = sysinfo(SI_PLATFORM,workbuf,CF_BUFSIZE);
if (sz == -1)
  {
  CfOut(cf_verbose,"","cfengine internal: sysinfo returned -1\n");
  }
else
  {
  NewClass(CanonifyName(workbuf));
  CfOut(cf_verbose,"","Additional hard class defined as: %s\n",workbuf);
  }
#endif
#endif

snprintf(workbuf,CF_BUFSIZE,"%s_%s_%s_%s",VSYSNAME.sysname,VSYSNAME.machine,VSYSNAME.release,VSYSNAME.version);

if (strlen(workbuf) > CF_MAXVARSIZE-2)
   {
   CfOut(cf_verbose,"","cfengine internal: $(arch) overflows CF_MAXVARSIZE! Truncating\n");
   }

sp = strdup(CanonifyName(workbuf));
NewScalar("sys","long_arch",sp,cf_str);
NewClass(sp);
free(sp);

snprintf(workbuf,CF_BUFSIZE,"%s_%s",VSYSNAME.sysname,VSYSNAME.machine);
sp = strdup(CanonifyName(workbuf));
NewScalar("sys","ostype",sp,cf_str);
NewClass(sp);

if (! found)
   {
   CfOut(cf_error,"","Cfengine: I don't understand what architecture this is!");
   }

strcpy(workbuf,"compiled_on_"); 
strcat(workbuf,CanonifyName(AUTOCONF_SYSNAME));
NewClass(CanonifyName(workbuf));
CfOut(cf_verbose,"","GNU autoconf class from compile time: %s",workbuf);

/* Get IP address from nameserver */

if ((hp = gethostbyname(VFQNAME)) == NULL)
   {
   CfOut(cf_verbose,"","Hostname lookup failed on node name \"%s\"\n",VSYSNAME.nodename);
   return;
   }
else
   {
   memset(&cin,0,sizeof(cin));
   cin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
   CfOut(cf_verbose,"","Address given by nameserver: %s\n",inet_ntoa(cin.sin_addr));
   strcpy(VIPADDRESS,inet_ntoa(cin.sin_addr));
   
   for (i=0; hp->h_aliases[i]!= NULL; i++)
      {
      Debug("Adding alias %s..\n",hp->h_aliases[i]);
      NewClass(CanonifyName(hp->h_aliases[i])); 
      }
   }
}

/*********************************************************************/

void GetInterfaceInfo3(void)

{ int fd,len,i,j,first_address,ipdefault = false;
  struct ifreq ifbuf[CF_IFREQ],ifr, *ifp;
  struct ifconf list;
  struct sockaddr_in *sin;
  struct hostent *hp;
  char *sp, workbuf[CF_BUFSIZE];
  char ip[CF_MAXVARSIZE];
  char name[CF_MAXVARSIZE];
  char last_name[CF_BUFSIZE];    

Debug("GetInterfaceInfo3()\n");

NewScalar("sys","interface",VIFDEV[VSYSTEMHARDCLASS],cf_str);

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

   if (strlen(ifp->ifr_name) == 0)
      {
      continue;
      }
   
   CfOut(cf_verbose,"","Interface %d: %s\n",j+1,ifp->ifr_name);
   
   if (strncmp(last_name,ifp->ifr_name,sizeof(ifp->ifr_name)) == 0)
      {
      first_address = false;
      }
   else
      {
      first_address = true;
      }
   
   strncpy(last_name,ifp->ifr_name,sizeof(ifp->ifr_name));

   if (UNDERSCORE_CLASSES)
      {
      snprintf(workbuf, CF_BUFSIZE, "_net_iface_%s", CanonifyName(ifp->ifr_name));
      }
   else
      {
      snprintf(workbuf, CF_BUFSIZE, "net_iface_%s", CanonifyName(ifp->ifr_name));
      }

   NewClass(workbuf);
   
   if (ifp->ifr_addr.sa_family == AF_INET)
      {
      strncpy(ifr.ifr_name,ifp->ifr_name,sizeof(ifp->ifr_name));
      
      if (ioctl(fd,SIOCGIFFLAGS,&ifr) == -1)
         {
         CfOut(cf_error,"ioctl","No such network device");
         close(fd);
         return;
         }

      if ((ifr.ifr_flags & IFF_BROADCAST) && !(ifr.ifr_flags & IFF_LOOPBACK))
         {
         sin=(struct sockaddr_in *)&ifp->ifr_addr;
   
         if ((hp = gethostbyaddr((char *)&(sin->sin_addr.s_addr),sizeof(sin->sin_addr.s_addr),AF_INET)) == NULL)
            {
            Debug("No hostinformation for %s not found\n", inet_ntoa(sin->sin_addr));
            }
         else
            {
            if (hp->h_name != NULL)
               {
               Debug("Adding hostip %s..\n",inet_ntoa(sin->sin_addr));
               NewClass(CanonifyName(inet_ntoa(sin->sin_addr)));
               Debug("Adding hostname %s..\n",hp->h_name);
               NewClass(CanonifyName(hp->h_name));

               if (hp->h_aliases != NULL)
                  {
                  for (i=0; hp->h_aliases[i] != NULL; i++)
                     {
                     CfOut(cf_verbose,"","Adding alias %s..\n",hp->h_aliases[i]);
                     NewClass(CanonifyName(hp->h_aliases[i]));
                     }
                  }
               }
            }
         
         if (!ipdefault)
            {
            ipdefault = true;
            strncpy(ip,"ipv4_",CF_MAXVARSIZE);
            strncat(ip,inet_ntoa(sin->sin_addr),CF_MAXVARSIZE-6);
            NewClass(CanonifyName(ip));
            NewScalar("sys","ipv4",inet_ntoa(sin->sin_addr),cf_str);
            strcpy(VIPADDRESS,inet_ntoa(sin->sin_addr));
   
            for (sp = ip+strlen(ip)-1; (sp > ip); sp--)
               {
               if (*sp == '.')
                  {
                  *sp = '\0';
                  NewClass(CanonifyName(ip));
                  }
               }
            }

         /* Matching variables */

         if (first_address)
            {
            strcpy(ip,inet_ntoa(sin->sin_addr));
            snprintf(name,CF_MAXVARSIZE-1,"ipv4[%s]",CanonifyName(ifp->ifr_name));
            NewScalar("sys",name,ip,cf_str);
            
            i = 3;
         
            for (sp = ip+strlen(ip)-1; (sp > ip); sp--)
               {
               if (*sp == '.')
                  {
                  *sp = '\0';
                  snprintf(name,CF_MAXVARSIZE-1,"ipv4_%d[%s]",i--,CanonifyName(ifp->ifr_name));
                  NewScalar("sys",name,ip,cf_str);
                  }
               }
            }
         }
      }
   }
 
close(fd);
}

/*******************************************************************/

void Get3Environment()

{ char env[CF_BUFSIZE],class[CF_BUFSIZE],name[CF_MAXVARSIZE],value[CF_MAXVARSIZE];
  FILE *fp;
  struct stat statbuf;
  time_t now = time(NULL);
  
CfOut(cf_verbose,"","Looking for environment from cf-monitor...\n");
snprintf(env,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_ENV_FILE);

if (stat(env,&statbuf) == -1)
   {
   CfOut(cf_verbose,"","Unable to detect environment from cfMonitord\n\n");
   return;
   }

if (statbuf.st_mtime < (now - 60*60))
   {
   CfOut(cf_verbose,"","Environment data are too old - discarding\n");
   unlink(env);
   return;
   }

snprintf(value,CF_MAXVARSIZE-1,"%s",ctime(&statbuf.st_mtime));
Chop(value);

DeleteVariable("sys","env_time");
NewScalar("sys","env_time",value,cf_str);

CfOut(cf_verbose,"","Loading environment...\n");
 
if ((fp = fopen(env,"r")) == NULL)
   {
   CfOut(cf_verbose,"","\nUnable to detect environment from cf-monitord\n\n");
   return;
   }

while (!feof(fp))
   {
   class[0] = '\0';
   name[0] = '\0';
   value[0] = '\0';

   fgets(class,CF_BUFSIZE-1,fp);

   if (feof(fp))
      {
      break;
      }

   if (strstr(class,"="))
      {
      sscanf(class,"%255[^=]=%255[^\n]",name,value);

      DeleteVariable("sys",name);
      NewScalar("sys",name,value,cf_str);
      }
   else
      {
      NewClass(class);
      }
   }
 
fclose(fp);
CfOut(cf_verbose,"","Environment data loaded\n\n"); 
}

/*********************************************************************/

void FindV6InterfaceInfo(void)

{ FILE *pp;
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
            NewClass(CanonifyName(ip->name));
            }
         }
      
      DeleteItemList(list);
      }
   }

cf_pclose(pp);
}


/*******************************************************************/

int IsInterfaceAddress(char *adr)

 /* Does this address belong to a local interface */

{ struct Item *ip;

for (ip = IPADDRESSES; ip != NULL; ip=ip->next)
   {
   if (StrnCmp(adr,ip->name,strlen(adr)) == 0)
      {
      Debug("Identifying (%s) as one of my interfaces\n",adr);
      return true;
      }
   }

Debug("(%s) is not one of my interfaces\n",adr); 
return false; 
}

/*********************************************************************/

void FindDomainName(char *hostname)

{ char fqn[CF_MAXVARSIZE];
  char *ptr;
  char buffer[CF_BUFSIZE];
 
strcpy(VFQNAME,hostname); /* By default VFQNAME = hostname (nodename) */

if (strstr(VFQNAME,".") == 0)
   {
   /* The nodename is not full qualified - try to find the FQDN hostname */

   if (gethostname(fqn, sizeof(fqn)) != -1)
      {
      struct hostent *hp;

      if (hp = gethostbyname(fqn))
         {
         if (strstr(hp->h_name,"."))
            {
            /* We find a FQDN hostname So we change the VFQNAME variable */
            strncpy(VFQNAME,hp->h_name,CF_MAXVARSIZE);
            VFQNAME[CF_MAXVARSIZE-1]= '\0'; 
            }
         }
      }
   }

strcpy(buffer,VFQNAME);
NewClass(CanonifyName(buffer));
NewClass(CanonifyName(ToLowerStr(buffer)));

if (strstr(VFQNAME,"."))
   {
   /* If VFQNAME is full qualified we can create VDOMAIN variable */
   ptr = strchr(VFQNAME, '.');
   strcpy(VDOMAIN, ++ptr);
   DeleteClass("undefined_domain");
   }

if (strstr(VFQNAME,".") == 0 && (strcmp(VDOMAIN,CF_START_DOMAIN) != 0))
   {
   strcat(VFQNAME,".");
   strcat(VFQNAME,VDOMAIN);
   }

if (strstr(VFQNAME,"."))
   {
   /* Add some domain hierarchy classes */
   for (ptr=VFQNAME; *ptr != '\0'; ptr++)
      {
      if (*ptr == '.')
         {
         if (*(ptr+1) != '\0')
            {
            Debug("Defining domain #%s#\n",(ptr+1));
            NewClass(CanonifyName(ptr+1));
            }
         else
            {
            Debug("Domain rejected\n");
            }      
         }
      }
   }

NewClass(CanonifyName(VDOMAIN));
}


/*******************************************************************/

void OSClasses()

{ struct stat statbuf;
  char vbuff[CF_BUFSIZE];
  char *sp;
  int i = 0;

NewClass("any");      /* This is a reserved word / wildcard */

snprintf(vbuff,CF_BUFSIZE,"cfengine_%s",CanonifyName(VERSION));
NewClass(vbuff);
 
for (sp = vbuff+strlen(vbuff); i < 2; sp--)
   {
   if (*sp == '_')
      {
      i++;
      *sp = '\0';
      NewClass(vbuff);
      }
   }

#ifdef LINUX

/* {Mandrake,Fedora} has a symlink at /etc/redhat-release pointing to
 * /etc/{mandrake,fedora}-release, so we else-if around that
 */

if (stat("/etc/mandrake-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a mandrake system.\n");
   NewClass("Mandrake");
   Linux_Mandrake_Version();
   }

else if (stat("/etc/fedora-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a fedora system.\n");
   NewClass("redhat");
   NewClass("fedora");
   Linux_Fedora_Version();
   }

else if (stat("/etc/redhat-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a redhat system.\n");
   NewClass("redhat");
   Linux_Redhat_Version();
   }

if (stat("/etc/generic-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a sun cobalt system.\n");
   NewClass("SunCobalt");
   }

if (stat("/etc/SuSE-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a SuSE system.\n");
   NewClass("SuSE");
   Linux_Suse_Version();
   }

#define SLACKWARE_ANCIENT_VERSION_FILENAME "/etc/slackware-release"
#define SLACKWARE_VERSION_FILENAME "/etc/slackware-version"
if (stat(SLACKWARE_VERSION_FILENAME,&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a slackware system.\n");
   NewClass("slackware");
   Linux_Slackware_Version(SLACKWARE_VERSION_FILENAME);
   }
else if (stat(SLACKWARE_ANCIENT_VERSION_FILENAME,&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be an ancient slackware system.\n");
   NewClass("slackware");
   Linux_Slackware_Version(SLACKWARE_ANCIENT_VERSION_FILENAME);
   }


if (stat("/etc/generic-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a sun cobalt system.\n");
   NewClass("SunCobalt");
   }
 
if (stat("/etc/debian_version",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a debian system.\n");
   NewClass("debian");
   Linux_Debian_Version();
   }

if (stat("/etc/UnitedLinux-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a UnitedLinux system.\n");
   NewClass("UnitedLinux");
   }

if (stat("/etc/gentoo-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a gentoo system.\n");
   NewClass("gentoo");
   }

Lsb_Version();

#endif

if (stat("/proc/vmware/version",&statbuf) != -1 ||
    stat("/etc/vmware-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a VMware Server ESX system.\n");
   NewClass("VMware");
   VM_Version();
   }
else if (stat("/etc/vmware",&statbuf) != -1)
   {
   if (S_ISDIR(statbuf.st_mode))
      {
      CfOut(cf_verbose,"","This appears to be a VMware xSX system.\n");
      NewClass("VMware");
      VM_Version();
      }
   }

if (stat("/proc/xen/capabilities",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a xen pv system.\n");
   NewClass("xen");
   Xen_Domain();
   }
#ifdef XEN_CPUID_SUPPORT
else if (Xen_Hv_Check())
   {
   CfOut(cf_verbose,"","This appears to be a xen hv system.\n");
   NewClass("xen");
   NewClass("xen_domu_hv");
   }
#endif

}

/*********************************************************************************/

int Linux_Fedora_Version(void)
{
#define FEDORA_ID "Fedora"

#define RELEASE_FLAG "release "

/* We are looking for one of the following strings...
 *
 * Fedora Core release 1 (Yarrow)
 * Fedora release 7 (Zodfoobar)
 */

#define FEDORA_REL_FILENAME "/etc/fedora-release"

FILE *fp;

/* The full string read in from fedora-release */
char relstring[CF_MAXVARSIZE];
char classbuf[CF_MAXVARSIZE];

/* Fedora */
char *vendor="";
/* Where the numerical release will be found */
char *release=NULL;

int major = -1;
char strmajor[CF_MAXVARSIZE];

/* Grab the first line from the file and then close it. */
 if ((fp = fopen(FEDORA_REL_FILENAME,"r")) == NULL)
    {
    return 1;
    }
 fgets(relstring, sizeof(relstring), fp);
 fclose(fp);
 
 CfOut(cf_verbose,"","Looking for fedora core linux info...\n");
 
 /* First, try to grok the vendor */
 if(!strncmp(relstring, FEDORA_ID, strlen(FEDORA_ID)))
    {
    vendor = "fedora";
    }
 else
    {
    CfOut(cf_verbose,"","Could not identify OS distro from %s\n", FEDORA_REL_FILENAME);
    return 2;
    }
 
 /* Now, grok the release.  We assume that all the strings will
  * have the word 'release' before the numerical release.
  */
 release = strstr(relstring, RELEASE_FLAG);
 if(release == NULL)
    {
    CfOut(cf_verbose,"","Could not find a numeric OS release in %s\n",
     FEDORA_REL_FILENAME);
    return 2;
    }
 else
    {
    release += strlen(RELEASE_FLAG);
    if (sscanf(release, "%d", &major) == 1)
       {
       sprintf(strmajor, "%d", major);
       }
    }
 
 if (major != -1 && (strcmp(vendor,"") != 0))
    {
    classbuf[0] = '\0';
    strcat(classbuf, vendor);
    NewClass(classbuf);
    strcat(classbuf, "_");
    strcat(classbuf, strmajor);
    NewClass(classbuf);
    }
 
 return 0;
}

/*********************************************************************************/

int Linux_Redhat_Version(void)
{
#define REDHAT_ID "Red Hat Linux"
#define REDHAT_AS_ID "Red Hat Enterprise Linux AS"
#define REDHAT_AS21_ID "Red Hat Linux Advanced Server"
#define REDHAT_ES_ID "Red Hat Enterprise Linux ES"
#define REDHAT_WS_ID "Red Hat Enterprise Linux WS"
#define REDHAT_C_ID "Red Hat Enterprise Linux Client"
#define REDHAT_S_ID "Red Hat Enterprise Linux Server"
#define MANDRAKE_ID "Linux Mandrake"
#define MANDRAKE_10_1_ID "Mandrakelinux"
#define WHITEBOX_ID "White Box Enterprise Linux"
#define CENTOS_ID "CentOS"
#define SCIENTIFIC_SL_ID "Scientific Linux SL"
#define SCIENTIFIC_CERN_ID "Scientific Linux CERN"
#define RELEASE_FLAG "release "

/* We are looking for one of the following strings...
 *
 * Red Hat Linux release 6.2 (Zoot)
 * Red Hat Linux Advanced Server release 2.1AS (Pensacola)
 * Red Hat Enterprise Linux AS release 3 (Taroon)
 * Red Hat Enterprise Linux WS release 3 (Taroon)
 * Red Hat Enterprise Linux Client release 5 (Tikanga)
 * Red Hat Enterprise Linux Server release 5 (Tikanga)
 * Linux Mandrake release 7.1 (helium)
 * Red Hat Enterprise Linux ES release 2.1 (Panama)
 * White Box Enterprise linux release 3.0 (Liberation)
 * Scientific Linux SL Release 4.0 (Beryllium)
 * CentOS release 4.0 (Final)
 */

#define RH_REL_FILENAME "/etc/redhat-release"

FILE *fp;

/* The full string read in from redhat-release */
char relstring[CF_MAXVARSIZE];
char classbuf[CF_MAXVARSIZE];

/* Red Hat, Mandrake */
char *vendor="";
/* as (Advanced Server, Enterprise) */
char *edition="";
/* Where the numerical release will be found */
char *release=NULL;
int i;
int major = -1;
char strmajor[CF_MAXVARSIZE];
int minor = -1;
char strminor[CF_MAXVARSIZE];

/* Grab the first line from the file and then close it. */
 if ((fp = fopen(RH_REL_FILENAME,"r")) == NULL)
    {
    return 1;
    }
 fgets(relstring, sizeof(relstring), fp);
 fclose(fp);
 
CfOut(cf_verbose,"","Looking for redhat linux info in \"%s\"\n",relstring);
 
 /* First, try to grok the vendor and the edition (if any) */
 if(!strncmp(relstring, REDHAT_ES_ID, strlen(REDHAT_ES_ID)))
    {
    vendor = "redhat";
    edition = "es";
    }
 else if(!strncmp(relstring, REDHAT_WS_ID, strlen(REDHAT_WS_ID)))
    {
    vendor = "redhat";
    edition = "ws";
    }
 else if(!strncmp(relstring, REDHAT_WS_ID, strlen(REDHAT_WS_ID)))
    {
    vendor = "redhat";
    edition = "ws";
    }
 else if(!strncmp(relstring, REDHAT_AS_ID, strlen(REDHAT_AS_ID)) ||
  !strncmp(relstring, REDHAT_AS21_ID, strlen(REDHAT_AS21_ID)))
    {
    vendor = "redhat";
    edition = "as";
    }
 else if(!strncmp(relstring, REDHAT_S_ID, strlen(REDHAT_S_ID)))
    {
    vendor = "redhat";
    edition = "s";
    }
 else if(!strncmp(relstring, REDHAT_C_ID, strlen(REDHAT_C_ID)))
    {
    vendor = "redhat";
    edition = "c";
    }
 else if(!strncmp(relstring, REDHAT_ID, strlen(REDHAT_ID)))
    {
    vendor = "redhat";
    }
 else if(!strncmp(relstring, MANDRAKE_ID, strlen(MANDRAKE_ID)))
    {
    vendor = "mandrake";
    }
 else if(!strncmp(relstring, MANDRAKE_10_1_ID, strlen(MANDRAKE_10_1_ID)))
    {
    vendor = "mandrake";
    }
 else if(!strncmp(relstring, WHITEBOX_ID, strlen(WHITEBOX_ID)))
    {
    vendor = "whitebox";
    }
 else if(!strncmp(relstring, SCIENTIFIC_SL_ID, strlen(SCIENTIFIC_SL_ID)))
    {
    vendor = "scientific";
    edition = "sl";
    }
 else if(!strncmp(relstring, SCIENTIFIC_CERN_ID, strlen(SCIENTIFIC_CERN_ID)))
    {
    vendor = "scientific";
    edition = "cern";
    }
 else if(!strncmp(relstring, CENTOS_ID, strlen(CENTOS_ID)))
    {
    vendor = "centos";
    }
 else
    {
    CfOut(cf_verbose,"","Could not identify OS distro from %s\n", RH_REL_FILENAME);
    return 2;
    }
 
 /* Now, grok the release.  For AS, we neglect the AS at the end of the
  * numerical release because we already figured out that it *is* AS
  * from the infomation above.  We assume that all the strings will
  * have the word 'release' before the numerical release.
  */

  /* Convert relstring to lowercase so that vendors like
     Scientific Linux don't fall through the cracks.
   */

 for (i = 0; i < strlen(relstring); i++)
    {
    relstring[i] = tolower(relstring[i]);
    }
 
 release = strstr(relstring, RELEASE_FLAG);
 if(release == NULL)
    {
    CfOut(cf_verbose,"","Could not find a numeric OS release in %s\n",
     RH_REL_FILENAME);
    return 2;
    }
 else
    {
    release += strlen(RELEASE_FLAG);
    if (sscanf(release, "%d.%d", &major, &minor) == 2)
       {
       sprintf(strmajor, "%d", major);
       sprintf(strminor, "%d", minor);
       }
    /* red hat 9 is *not* red hat 9.0. 
     * and same thing with RHEL AS 3
     */
    else if (sscanf(release, "%d", &major) == 1)
       {
       sprintf(strmajor, "%d", major);
       minor = -2;
       }
    }
 
 if (major != -1 && minor != -1 && (strcmp(vendor,"") != 0))
    {
    classbuf[0] = '\0';
    strcat(classbuf, vendor);
    NewClass(classbuf);
    strcat(classbuf, "_");
    
    if (strcmp(edition,"") != 0)
       {
       strcat(classbuf, edition);
       NewClass(classbuf);
       strcat(classbuf, "_");
       }
    
    strcat(classbuf, strmajor);
    NewClass(classbuf);
    if (minor != -2)
       {
       strcat(classbuf, "_");
       strcat(classbuf, strminor);
       NewClass(classbuf);
       }
    }
 return 0;
}

/******************************************************************/

int Linux_Suse_Version(void)
{
#define SUSE_REL_FILENAME "/etc/SuSE-release"
/* Check if it's a SuSE Enterprise version (all in lowercase) */
#define SUSE_SLES8_ID "suse sles-8"
#define SUSE_SLES_ID  "suse linux enterprise server"
#define SUSE_RELEASE_FLAG "linux "

/* The full string read in from SuSE-release */
char relstring[CF_MAXVARSIZE];
char classbuf[CF_MAXVARSIZE];
char vbuf[CF_BUFSIZE];

/* Where the numerical release will be found */
char *release=NULL;

int i,version;
int major = -1;
char strmajor[CF_MAXVARSIZE];
int minor = -1;
char strminor[CF_MAXVARSIZE];

FILE *fp;

 /* Grab the first line from the file and then close it. */

if ((fp = fopen(SUSE_REL_FILENAME,"r")) == NULL)
   {
   return 1;
   }

fgets(relstring, sizeof(relstring), fp);
Chop(relstring);
fclose(fp);
  
   /* Check if it's a SuSE Enterprise version  */

CfOut(cf_verbose,"","Looking for SuSE enterprise info in \"%s\"\n",relstring);
 
 /* Convert relstring to lowercase to handle rename of SuSE to 
  * SUSE with SUSE 10.0. 
  */

for (i = 0; i < strlen(relstring); i++)
   {
   relstring[i] = tolower(relstring[i]);
   }

   /* Check if it's a SuSE Enterprise version (all in lowercase) */

if (!strncmp(relstring, SUSE_SLES8_ID, strlen(SUSE_SLES8_ID)))
   {
   classbuf[0] = '\0';
   strcat(classbuf, "SLES8");
   NewClass(classbuf);
   }
else
   {
   for (version = 9; version < 13; version++)
      {
      snprintf(vbuf,CF_BUFSIZE,"%s %d ",SUSE_SLES_ID,version);
      Debug("Checking for suse [%s]\n",vbuf);
      
      if (!strncmp(relstring, vbuf, strlen(vbuf)))
         {
         snprintf(classbuf,CF_MAXVARSIZE,"SLES%d",version);
         NewClass(classbuf);
         }
      }
   }
    
 /* Determine release version. We assume that the version follows
  * the string "SuSE Linux" or "SUSE LINUX".
  */

release = strstr(relstring, SUSE_RELEASE_FLAG);

if (release == NULL)
   {
   CfOut(cf_verbose,"","Could not find a numeric OS release in %s\n",SUSE_REL_FILENAME);
   return 2;
   }
else
   {
   release += strlen(SUSE_RELEASE_FLAG);
   sscanf(release, "%d.%d", &major, &minor);
   sprintf(strmajor, "%d", major);
   sprintf(strminor, "%d", minor);
   }

if(major != -1 && minor != -1)
   {
   classbuf[0] = '\0';
   strcat(classbuf, "SuSE");
   NewClass(classbuf);
   strcat(classbuf, "_");
   strcat(classbuf, strmajor);
   NewClass(classbuf);
   strcat(classbuf, "_");
   strcat(classbuf, strminor);
   NewClass(classbuf);
   }

return 0;
}

/******************************************************************/

int Linux_Slackware_Version(char *filename)
{
int major = -1; 
int minor = -1; 
int release = -1;
char classname[CF_MAXVARSIZE] = "";
FILE *fp;

if ((fp = fopen(filename,"r")) == NULL)
   {
   return 1;
   }

CfOut(cf_verbose,"","Looking for Slackware version...\n");
switch (fscanf(fp, "Slackware %d.%d.%d", &major, &minor, &release))
    {
    case 3:
        CfOut(cf_verbose,"","This appears to be a Slackware %u.%u.%u system.", major, minor, release);
        snprintf(classname, CF_MAXVARSIZE, "slackware_%u_%u_%u", major, minor, release);
        NewClass(classname);
        /* Fall-through */
    case 2:
        CfOut(cf_verbose,"","This appears to be a Slackware %u.%u system.", major, minor);
        snprintf(classname, CF_MAXVARSIZE, "slackware_%u_%u", major, minor);
        NewClass(classname);
        /* Fall-through */
    case 1:
        CfOut(cf_verbose,"","This appears to be a Slackware %u system.", major);
        snprintf(classname, CF_MAXVARSIZE, "slackware_%u", major);
        NewClass(classname);
        break;
    case 0:
        CfOut(cf_verbose,"","No Slackware version number found.\n");
        fclose(fp);
        return 2;
    }
fclose(fp);
return 0;
}

/******************************************************************/

int Linux_Debian_Version(void)
{
#define DEBIAN_VERSION_FILENAME "/etc/debian_version"
int major = -1; 
int release = -1;
char classname[CF_MAXVARSIZE] = "";
FILE *fp;

if ((fp = fopen(DEBIAN_VERSION_FILENAME,"r")) == NULL)
   {
   return 1;
   }

CfOut(cf_verbose,"","Looking for Debian version...\n");
switch (fscanf(fp, "%d.%d", &major, &release))
    {
    case 2:
        CfOut(cf_verbose,"","This appears to be a Debian %u.%u system.", major, release);
        snprintf(classname, CF_MAXVARSIZE, "debian_%u_%u", major, release);
        NewClass(classname);
        /* Fall-through */
    case 1:
        CfOut(cf_verbose,"","This appears to be a Debian %u system.", major);
        snprintf(classname, CF_MAXVARSIZE, "debian_%u", major);
        NewClass(classname);
        break;
    case 0:
        CfOut(cf_verbose,"","No Debian version number found.\n");
        fclose(fp);
        return 2;
    }

fclose(fp);
return 0;
}

/******************************************************************/

int Linux_Mandrake_Version(void)
{

/* We are looking for one of the following strings... */
#define MANDRAKE_ID "Linux Mandrake"
#define MANDRAKE_REV_ID "Mandrake Linux"
#define MANDRAKE_10_1_ID "Mandrakelinux"

#define RELEASE_FLAG "release "
#define MANDRAKE_REL_FILENAME "/etc/mandrake-release"

FILE *fp;

/* The full string read in from mandrake-release */
char relstring[CF_MAXVARSIZE];
char classbuf[CF_MAXVARSIZE];

/* I've never seen Mandrake-Move or the other 'editions', so
   I'm not going to try and support them here.  Contributions welcome. */

/* Where the numerical release will be found */
char *release=NULL;
char *vendor=NULL;
int major = -1;
char strmajor[CF_MAXVARSIZE];
int minor = -1;
char strminor[CF_MAXVARSIZE];

/* Grab the first line from the file and then close it. */
 if ((fp = fopen(MANDRAKE_REL_FILENAME,"r")) == NULL)
    {
    return 1;
    }
 fgets(relstring, sizeof(relstring), fp);
 fclose(fp);

 CfOut(cf_verbose,"","Looking for Mandrake linux info in \"%s\"\n",relstring);

  /* Older Mandrakes had the 'Mandrake Linux' string in reverse order */
 if(!strncmp(relstring, MANDRAKE_ID, strlen(MANDRAKE_ID)))
    {
    vendor = "mandrake";
    }
 else if(!strncmp(relstring, MANDRAKE_REV_ID, strlen(MANDRAKE_REV_ID)))
    {
    vendor = "mandrake";
    }

 else if(!strncmp(relstring, MANDRAKE_10_1_ID, strlen(MANDRAKE_10_1_ID)))
    {
    vendor = "mandrake";
    }
 else
    {
    CfOut(cf_verbose,"","Could not identify OS distro from %s\n", MANDRAKE_REL_FILENAME);
    return 2;
    }

 /* Now, grok the release. We assume that all the strings will
  * have the word 'release' before the numerical release.
  */
 release = strstr(relstring, RELEASE_FLAG);
 if(release == NULL)
    {
    CfOut(cf_verbose,"","Could not find a numeric OS release in %s\n",MANDRAKE_REL_FILENAME);
    return 2;
    }
 else
    {
    release += strlen(RELEASE_FLAG);
    if (sscanf(release, "%d.%d", &major, &minor) == 2)
       {
       sprintf(strmajor, "%d", major);
       sprintf(strminor, "%d", minor);
       }
    else
       {
       CfOut(cf_verbose,"","Could not break down release version numbers in %s\n",MANDRAKE_REL_FILENAME);
       }
    }

 if (major != -1 && minor != -1 && strcmp(vendor, ""))
    {
    classbuf[0] = '\0';
    strcat(classbuf, vendor);
    NewClass(classbuf);
    strcat(classbuf, "_");
    strcat(classbuf, strmajor);
    NewClass(classbuf);
    if (minor != -2)
       {
       strcat(classbuf, "_");
       strcat(classbuf, strminor);
       NewClass(classbuf);
       }
    }
 return 0;
}

/******************************************************************/

void *Lsb_Release(const char *command, const char *key)

{ char vbuff[CF_BUFSIZE],*info = NULL;
  FILE *fp;

snprintf(vbuff, CF_BUFSIZE, "%s %s", command, key);
if ((fp = cf_popen(vbuff, "r")) == NULL)
   {
   return NULL;
   }

if (ReadLine(vbuff, CF_BUFSIZE, fp))
   {
   char * buffer = vbuff;
   strsep(&buffer, ":");
   
   while((*buffer != '\0') && isspace(*buffer))
      {
      buffer++;
      }
   
   info = buffer;
   while((*buffer != '\0') && !isspace(*buffer))
      {
      *buffer = tolower(*buffer++);
      }
   
   *buffer = '\0';
   info = strdup(info);
   }

cf_pclose(fp);
return info;
}


/******************************************************************/

int Lsb_Version(void)

{ char vbuff[CF_BUFSIZE];
 
#define LSB_RELEASE_COMMAND "lsb_release"

char classname[CF_MAXVARSIZE];
char *distrib  = NULL;
char *release  = NULL;
char *codename = NULL;
int major = 0;
int minor = 0;

char *path, *dir, *rest;
struct stat statbuf;

path = rest = strdup(getenv("PATH"));

if (strlen(path) == 0)
   {
   return 1;
   }

while (dir = strsep(&rest, ":"))
    {
    snprintf(vbuff, CF_BUFSIZE, "%s/" LSB_RELEASE_COMMAND, dir);
    if (stat(vbuff,&statbuf) != -1)
        {
        free(path);
        path = strdup(vbuff);

        CfOut(cf_verbose,"","This appears to be a LSB compliant system.\n");
        NewClass("lsb_compliant");
        break;
        }
    }

if (!dir)
   {
   free(path);
   return 1;
   }

if ((distrib  = Lsb_Release(path, "--id")) != NULL)
   {
   snprintf(classname, CF_MAXVARSIZE, "%s", distrib);
   NewClass(classname);
   
   if ((codename = Lsb_Release(path, "--codename")) != NULL)
      {
      snprintf(classname, CF_MAXVARSIZE, "%s_%s", distrib, codename);
      NewClass(classname);
      }
   
   if ((release  = Lsb_Release(path, "--release")) != NULL)
      {
      switch (sscanf(release, "%d.%d\n", &major, &minor))
         {
         case 2:
             snprintf(classname, CF_MAXVARSIZE, "%s_%u_%u", distrib, major, minor);
             NewClass(classname);
         case 1:
             snprintf(classname, CF_MAXVARSIZE, "%s_%u", distrib, major);
             NewClass(classname);
         }
      }

   free(path);
   return 0;
   }
else
   {
   free(path);
   return 2;
   }
}

/******************************************************************/

int VM_Version(void)

{ FILE *fp;
  char *sp,buffer[CF_BUFSIZE],classbuf[CF_BUFSIZE],version[CF_BUFSIZE];
  struct stat statbuf;
  int major,minor,bug;
  int len = 0;
  int sufficient = 0;

/* VMware Server ESX >= 3 has version info in /proc */
if ((fp = fopen("/proc/vmware/version","r")) != NULL)
   {
   ReadLine(buffer,CF_BUFSIZE,fp);
   Chop(buffer);
   if (sscanf(buffer,"VMware ESX Server %d.%d.%d",&major,&minor,&bug) > 0)
      {
      snprintf(classbuf,CF_BUFSIZE,"VMware ESX Server %d",major);
      NewClass(CanonifyName(classbuf));
      snprintf(classbuf,CF_BUFSIZE,"VMware ESX Server %d.%d",major,minor);
      NewClass(CanonifyName(classbuf));
      snprintf(classbuf,CF_BUFSIZE,"VMware ESX Server %d.%d.%d",major,minor,bug);
      NewClass(CanonifyName(classbuf));
      sufficient = 1;
      }
   else if (sscanf(buffer,"VMware ESX Server %s",version) > 0)
      {
      snprintf(classbuf,CF_BUFSIZE,"VMware ESX Server %s",version);
      NewClass(CanonifyName(classbuf));
      sufficient = 1;
      }
   fclose(fp);
   }

/* Fall back to checking for other files */
if (sufficient < 1 && ((fp = fopen("/etc/vmware-release","r")) != NULL) ||
    (fp = fopen("/etc/issue","r")) != NULL)
   {
   ReadLine(buffer,CF_BUFSIZE,fp);
   Chop(buffer);
   NewClass(CanonifyName(buffer));
   
   /* Strip off the release code name e.g. "(Dali)" */
   if ((sp = strchr(buffer,'(')) != NULL)
      {
      *sp = 0;
      Chop(buffer);
      NewClass(CanonifyName(buffer));
      }
   sufficient = 1;
   fclose(fp);
   }

return sufficient < 1 ? 1 : 0;
}

/******************************************************************/

int Xen_Domain(void)

{ FILE *fp;
  char buffer[CF_BUFSIZE];
  int sufficient = 0;

/* xen host will have "control_d" in /proc/xen/capabilities, xen guest will not */
  
if ((fp = fopen("/proc/xen/capabilities","r")) != NULL)
   {
   while (!feof(fp))
      {
      ReadLine(buffer,CF_BUFSIZE,fp);
      if (strstr(buffer,"control_d"))
         {
         NewClass("xen_dom0");
         sufficient = 1;
         }
      }
   if (sufficient < 1)
      {
      NewClass("xen_domu_pv");
      sufficient = 1;
      }
   }

return sufficient < 1 ? 1 : 0;
}

/******************************************************************/

#ifdef XEN_CPUID_SUPPORT
/* borrowed from Xen source/tools/libxc/xc_cpuid_x86.c */

void Xen_Cpuid(uint32_t idx, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
asm (
    /* %ebx register need to be saved before usage and restored thereafter
     * for PIC-compliant code on i386 */
#ifdef __i386__
    "push %%ebx; cpuid; mov %%ebx,%1; pop %%ebx"
#else
    "push %%rbx; cpuid; mov %%ebx,%1; pop %%rbx"
#endif
    : "=a" (*eax), "=r" (*ebx), "=c" (*ecx), "=d" (*edx)
    : "0" (idx), "2" (0)
    );
}

/******************************************************************/

int Xen_Hv_Check(void)

{ uint32_t eax, ebx, ecx, edx;
  char signature[13];

Xen_Cpuid(0x40000000, &eax, &ebx, &ecx, &edx);
*(uint32_t *)(signature + 0) = ebx;
*(uint32_t *)(signature + 4) = ecx;
*(uint32_t *)(signature + 8) = edx;
signature[12] = '\0';

if (strcmp("XenVMMXenVMM", signature) || (eax < 0x40000002))
   {
   return 0;
   }

Xen_Cpuid(0x40000001, &eax, &ebx, &ecx, &edx);
return 1;
}

#endif


/******************************************************************/
/* User info                                                      */
/******************************************************************/

char *GetHome(uid_t uid)

{
struct passwd *mpw = getpwuid(uid);
return mpw->pw_dir;
}
