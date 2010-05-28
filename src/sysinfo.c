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
  char *components[] = { "cf-twin", "cf-agent", "cf-serverd", "cf-monitord", "cf-know",
                         "cf-report", "cf-key", "cf-runagent", "cf-execd",
                         "cf-promises", NULL };
  int have_component[11];
  struct stat sb;
  char name[CF_MAXVARSIZE],quoteName[CF_MAXVARSIZE],shortname[CF_MAXVARSIZE];


Debug("GetNameInfo()\n");

if (VSYSTEMHARDCLASS != unused1)
   {
   CfOut(cf_verbose,"","Already know our hard classes...\n");
   /* Already have our name - so avoid memory leaks by recomputing */
   return;
   }

VFQNAME[0] = VUQNAME[0] = '\0';

if (uname(&VSYSNAME) == -1)
   {
   CfOut(cf_error, "uname", "!!! Couldn't get kernel name info!");
   memset(&VSYSNAME, 0, sizeof(VSYSNAME));
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
   NewClass(VFQNAME);
   strcpy(VUQNAME,VSYSNAME.nodename);
   NewClass(VUQNAME);
   }
else
   {
   int n = 0;
   strcpy(VFQNAME,VSYSNAME.nodename);
   NewClass(VFQNAME);

   while(VSYSNAME.nodename[n++] != '.' && VSYSNAME.nodename[n] != '\0')
      {
      }

   strncpy(VUQNAME,VSYSNAME.nodename,n);

   if (VUQNAME[n-1] == '.')
      {
      VUQNAME[n-1] = '\0';
      }
   else
      {
      VUQNAME[n] = '\0';
      }

   NewClass(VUQNAME);
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
CfOut(cf_verbose,"","The time is now %s\n\n",cf_ctime(&tloc));
CfOut(cf_verbose,"","------------------------------------------------------------------------\n\n");

snprintf(workbuf,CF_MAXVARSIZE,"%s",cf_ctime(&tloc));
Chop(workbuf);
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
NewScalar("sys","exports",VEXPORTS[VSYSTEMHARDCLASS],cf_str);
NewScalar("sys","expires",EXPIRY,cf_str);
NewScalar("sys","cf_version",VERSION,cf_str);
#ifdef HAVE_LIBCFNOVA
NewScalar("sys","nova_version",Nova_GetVersion(),cf_str);
#endif

for (i = 0; components[i] != NULL; i++)
   {
   snprintf(shortname,CF_MAXVARSIZE-1,"%s",CanonifyName(components[i]));

   if (VSYSTEMHARDCLASS == mingw || VSYSTEMHARDCLASS == cfnt)
      {
      // twin has own dir, and is named agent
      if(i == 0)
	{
	snprintf(name,CF_MAXVARSIZE-1,"%s%cbin-twin%ccf-agent.exe",CFWORKDIR,FILE_SEPARATOR,FILE_SEPARATOR);
	}
      else
	{
        snprintf(name,CF_MAXVARSIZE-1,"%s%cbin%c%s.exe",CFWORKDIR,FILE_SEPARATOR,FILE_SEPARATOR,components[i]);
	}
      }
   else
      {
      snprintf(name,CF_MAXVARSIZE-1,"%s%cbin%c%s",CFWORKDIR,FILE_SEPARATOR,FILE_SEPARATOR,components[i]);
      }

   have_component[i] = false;

   if (cfstat(name, &sb) != -1)
      {
      snprintf(quoteName, sizeof(quoteName), "\"%s\"", name);
      NewScalar("sys",shortname,quoteName,cf_str);
      have_component[i] = true;
      }
   }

// If no twin, fail over the agent

if (!have_component[0])
   {
   snprintf(shortname,CF_MAXVARSIZE-1,"%s",CanonifyName(components[0]));
   
   if (VSYSTEMHARDCLASS == mingw || VSYSTEMHARDCLASS == cfnt)
      {
      snprintf(name,CF_MAXVARSIZE-1,"%s%cbin%c%s.exe",CFWORKDIR,FILE_SEPARATOR,FILE_SEPARATOR,components[1]);
      }
   else
      {
      snprintf(name,CF_MAXVARSIZE-1,"%s%cbin%c%s",CFWORKDIR,FILE_SEPARATOR,FILE_SEPARATOR,components[1]);
      }

   if (cfstat(name, &sb) != -1)
      {
      snprintf(quoteName, sizeof(quoteName), "\"%s\"", name);
      NewScalar("sys",shortname,quoteName,cf_str);
      }
   }

/* Windows special directories */

#ifdef MINGW
if(NovaWin_GetWinDir(workbuf, sizeof(workbuf)))
  {
  NewScalar("sys","windir",workbuf,cf_str);
  }

if(NovaWin_GetSysDir(workbuf, sizeof(workbuf)))
  {
  NewScalar("sys","winsysdir",workbuf,cf_str);
  }

if(NovaWin_GetProgDir(workbuf, sizeof(workbuf)))
  {
  NewScalar("sys","winprogdir",workbuf,cf_str);
  }

# ifdef _WIN64
// only available on 64 bit windows systems
if(NovaWin_GetEnv("PROGRAMFILES(x86)", workbuf, sizeof(workbuf)))
  {
  NewScalar("sys","winprogdir86",workbuf,cf_str);
  }

# else  /* NOT _WIN64 */

NewScalar("sys","winprogdir86","",cf_str);

# endif

#else /* NOT MINGW */

// defs on Unix for manual-building purposes

NewScalar("sys","windir","/dev/null",cf_str);
NewScalar("sys","winsysdir","/dev/null",cf_str);
NewScalar("sys","winprogdir","/dev/null",cf_str);
NewScalar("sys","winprogdir86","/dev/null",cf_str);

#endif  /* NOT MINGW */

LoadSlowlyVaryingObservations();
EnterpriseContext();

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
NewClass(workbuf);

#ifdef IRIX
/* Get something like `irix64_6_5_19m' defined as well as
   `irix64_6_5'.  Just copying the latter into VSYSNAME.release
   wouldn't be backwards-compatible.  */
snprintf(workbuf,CF_BUFSIZE,"%s_%s",VSYSNAME.sysname,real_version);
NewClass(workbuf);
#endif

NewClass(VSYSNAME.machine);
CfOut(cf_verbose,"","Additional hard class defined as: %s\n",CanonifyName(workbuf));

snprintf(workbuf,CF_BUFSIZE,"%s_%s",VSYSNAME.sysname,VSYSNAME.machine);
NewClass(workbuf);
CfOut(cf_verbose,"","Additional hard class defined as: %s\n",CanonifyName(workbuf));

snprintf(workbuf,CF_BUFSIZE,"%s_%s_%s",VSYSNAME.sysname,VSYSNAME.machine,VSYSNAME.release);
NewClass(workbuf);
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
  NewClass(workbuf);
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
  NewClass(workbuf);
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
free(sp);

if (! found)
   {
   CfOut(cf_error,"","Cfengine: I don't understand what architecture this is!");
   }

strcpy(workbuf,"compiled_on_");
strcat(workbuf,CanonifyName(AUTOCONF_SYSNAME));
NewClass(workbuf);
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

   for (i = 0; hp->h_aliases[i]!= NULL; i++)
      {
      Debug("Adding alias %s..\n",hp->h_aliases[i]);
      NewClass(hp->h_aliases[i]);
      }
   }
}

/*********************************************************************/

void CfGetInterfaceInfo(enum cfagenttype ag)

#ifdef MINGW
{
NovaWin_GetInterfaceInfo();
}
#else
{
Unix_GetInterfaceInfo(ag);
Unix_FindV6InterfaceInfo();
}
#endif  /* NOT MINGW */

/*********************************************************************/

void Get3Environment()

{ char env[CF_BUFSIZE],class[CF_BUFSIZE],name[CF_MAXVARSIZE],value[CF_MAXVARSIZE];
  FILE *fp;
  struct stat statbuf;
  time_t now = time(NULL);

CfOut(cf_verbose,"","Looking for environment from cf-monitor...\n");

snprintf(env,CF_BUFSIZE,"%s/state/%s",CFWORKDIR,CF_ENV_FILE);
MapName(env);

if (cfstat(env,&statbuf) == -1)
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

snprintf(value,CF_MAXVARSIZE-1,"%s",cf_ctime(&statbuf.st_mtime));
Chop(value);

DeleteVariable("mon","env_time");
NewScalar("mon","env_time",value,cf_str);

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

      if (THIS_AGENT_TYPE != cf_executor)
         {
         DeleteVariable("mon",name);
         NewScalar("mon",name,value,cf_str);         
         Debug(" -> Setting new monitoring scalar %s => %s",name,value);
         }
      }
   else
      {
      NewClass(class);
      }
   }

fclose(fp);
CfOut(cf_verbose,"","Environment data loaded\n\n");
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
NewClass(buffer);
NewClass(ToLowerStr(buffer));

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
            NewClass(ptr+1);
            }
         else
            {
            Debug("Domain rejected\n");
            }
         }
      }
   }

NewClass(VDOMAIN);
}

/*******************************************************************/

void OSClasses()

{ struct stat statbuf;
  char vbuff[CF_BUFSIZE],class[CF_BUFSIZE];
  char *sp;
  int i = 0;
  struct passwd *pw;

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

if (cfstat("/etc/mandriva-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a mandriva system.\n");
   NewClass("Mandrake");
   NewClass("Mandriva");
   Linux_New_Mandriva_Version();
   }

else if (cfstat("/etc/mandrake-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a mandrake system.\n");
   NewClass("Mandrake");
   Linux_Old_Mandriva_Version();
   }

else if (cfstat("/etc/fedora-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a fedora system.\n");
   NewClass("redhat");
   NewClass("fedora");
   Linux_Fedora_Version();
   }

else if (cfstat("/etc/redhat-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a redhat system.\n");
   NewClass("redhat");
   Linux_Redhat_Version();
   }

if (cfstat("/etc/generic-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a sun cobalt system.\n");
   NewClass("SunCobalt");
   }

if (cfstat("/etc/SuSE-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a SuSE system.\n");
   NewClass("SuSE");
   Linux_Suse_Version();
   }

#define SLACKWARE_ANCIENT_VERSION_FILENAME "/etc/slackware-release"
#define SLACKWARE_VERSION_FILENAME "/etc/slackware-version"
if (cfstat(SLACKWARE_VERSION_FILENAME,&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a slackware system.\n");
   NewClass("slackware");
   Linux_Slackware_Version(SLACKWARE_VERSION_FILENAME);
   }
else if (cfstat(SLACKWARE_ANCIENT_VERSION_FILENAME,&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be an ancient slackware system.\n");
   NewClass("slackware");
   Linux_Slackware_Version(SLACKWARE_ANCIENT_VERSION_FILENAME);
   }

if (cfstat("/etc/generic-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a sun cobalt system.\n");
   NewClass("SunCobalt");
   }

if (cfstat("/etc/debian_version",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a debian system.\n");
   NewClass("debian");
   Linux_Debian_Version();
   }

if (cfstat("/usr/bin/aptitude",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This system seems to have the aptitude package system\n");
   NewClass("have_aptitude");
   }

if (cfstat("/etc/UnitedLinux-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a UnitedLinux system.\n");
   NewClass("UnitedLinux");
   }

if (cfstat("/etc/gentoo-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a gentoo system.\n");
   NewClass("gentoo");
   }

Lsb_Version();

#else

strncpy(vbuff,VSYSNAME.release,CF_MAXVARSIZE);

for (sp = vbuff; *sp != '\0'; sp++)
   {
   if (*sp == '-')
      {
      *sp = '\0';
      break;
      }
   }

snprintf(class,CF_BUFSIZE,"%s_%s",VSYSNAME.sysname,vbuff);
NewScalar("sys","flavour",class,cf_str);
NewScalar("sys","flavor",class,cf_str);

#endif

if (cfstat("/proc/vmware/version",&statbuf) != -1 ||
    cfstat("/etc/vmware-release",&statbuf) != -1)
   {
   CfOut(cf_verbose,"","This appears to be a VMware Server ESX system.\n");
   NewClass("VMware");
   VM_Version();
   }
else if (cfstat("/etc/vmware",&statbuf) != -1)
   {
   if (S_ISDIR(statbuf.st_mode))
      {
      CfOut(cf_verbose,"","This appears to be a VMware xSX system.\n");
      NewClass("VMware");
      VM_Version();
      }
   }

if (cfstat("/proc/xen/capabilities",&statbuf) != -1)
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


#ifdef CFCYG

for (sp = VSYSNAME.sysname; *sp != '\0'; sp++)
   {
   if (*sp == '-')
      {
      sp++;
      if (strncmp(sp,"5.0",3) == 0)
         {
         CfOut(cf_verbose,"","This appears to be Windows 2000\n");
         NewClass("Win2000");
         }

      if (strncmp(sp,"5.1",3) == 0)
         {
         CfOut(cf_verbose,"","This appears to be Windows XP\n");
         NewClass("WinXP");
         }

      if (strncmp(sp,"5.2",3) == 0)
         {
         CfOut(cf_verbose,"","This appears to be Windows Server 2003\n");
         NewClass("WinServer2003");
         }

      if (strncmp(sp,"6.1",3) == 0)
         {
         CfOut(cf_verbose,"","This appears to be Windows Vista\n");
         NewClass("WinVista");
         }

      if (strncmp(sp,"6.3",3) == 0)
         {
         CfOut(cf_verbose,"","This appears to be Windows Server 2008\n");
         NewClass("WinServer2008");
         }
      }
   }

NewScalar("sys","crontab","",cf_str);

#endif  /* CFCYG */

#ifdef MINGW
NewClass(VSYSNAME.version);  // code name - e.g. Windows Vista
NewClass(VSYSNAME.release);  // service pack number - e.g. Service Pack 3

if (strstr(VSYSNAME.sysname, "workstation"))
   {
   NewClass("WinWorkstation");
   }
else if(strstr(VSYSNAME.sysname, "server"))
   {
   NewClass("WinServer");
   }
else if(strstr(VSYSNAME.sysname, "domain controller"))
   {
   NewClass("DomainController");
   NewClass("WinServer");
   }
else
   {
   NewClass("unknown_ostype");
   }

NewScalar("sys","flavour","windows",cf_str);
NewScalar("sys","flavor","windows",cf_str);

#endif  /* MINGW */


#ifndef NT
if ((pw = getpwuid(getuid())) == NULL)
   {
   CfOut(cf_error,"getpwuid"," !! Unable to get username for uid %d",getuid);
   }
else
   {
   if (IsDefinedClass("SuSE"))
      {
      snprintf(vbuff,CF_BUFSIZE,"/var/spool/cron/tabs/%s",pw->pw_name);
      }
   else
      {
      snprintf(vbuff,CF_BUFSIZE,"/var/spool/cron/crontabs/%s",pw->pw_name);
      }
   }

NewScalar("sys","crontab",vbuff,cf_str);
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
 char *vendor="";
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

if (!strncmp(relstring, FEDORA_ID, strlen(FEDORA_ID)))
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

if (release == NULL)
   {
   CfOut(cf_verbose,"","Could not find a numeric OS release in %s\n",FEDORA_REL_FILENAME);
   return 2;
   }
else
   {
   release += strlen(RELEASE_FLAG);

   if (sscanf(release, "%d", &major) != 0)
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
if (!strncmp(relstring, REDHAT_ES_ID, strlen(REDHAT_ES_ID)))
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
if (release == NULL)
   {
   CfOut(cf_verbose,"","Could not find a numeric OS release in %s\n", RH_REL_FILENAME);
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
#define SUSE_SLED_ID  "suse linux enterprise desktop"
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
      else
         {
         snprintf(vbuf,CF_BUFSIZE,"%s %d ",SUSE_SLED_ID,version);
         Debug("Checking for suse [%s]\n",vbuf);
         
         if (!strncmp(relstring, vbuf, strlen(vbuf)))
            {
            snprintf(classbuf,CF_MAXVARSIZE,"SLED%d",version);
            NewClass(classbuf);
            }
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

if (major != -1 && minor != -1)
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

int Linux_Old_Mandriva_Version(void)

{
/* We are looking for one of the following strings... */
#define MANDRAKE_ID "Linux Mandrake"
#define MANDRAKE_REV_ID "Mandrake Linux"
#define MANDRAKE_10_1_ID "Mandrakelinux"

#define MANDRAKE_REL_FILENAME "/etc/mandrake-release"

 FILE *fp;
 char relstring[CF_MAXVARSIZE];
 char *vendor=NULL;

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

return Linux_Mandriva_Version_Real(MANDRAKE_REL_FILENAME, relstring, vendor);
}

int Linux_New_Mandriva_Version(void)

{
/* We are looking for the following strings... */
#define MANDRIVA_ID "Mandriva Linux"

#define MANDRIVA_REL_FILENAME "/etc/mandriva-release"

 FILE *fp;
 char relstring[CF_MAXVARSIZE];
 char *vendor=NULL;

if ((fp = fopen(MANDRIVA_REL_FILENAME,"r")) == NULL)
   {
   return 1;
   }

fgets(relstring, sizeof(relstring), fp);
fclose(fp);

CfOut(cf_verbose,"","Looking for Mandriva linux info in \"%s\"\n",relstring);

if(!strncmp(relstring, MANDRIVA_ID, strlen(MANDRIVA_ID)))
   {
   vendor = "mandriva";
   }
else
   {
   CfOut(cf_verbose,"","Could not identify OS distro from %s\n", MANDRIVA_REL_FILENAME);
   return 2;
   }

return Linux_Mandriva_Version_Real(MANDRIVA_REL_FILENAME, relstring, vendor);

}

int Linux_Mandriva_Version_Real(char *filename, char *relstring, char *vendor)

{
 char *release=NULL;
 char classbuf[CF_MAXVARSIZE];
 int major = -1;
 char strmajor[CF_MAXVARSIZE];
 int minor = -1;
 char strminor[CF_MAXVARSIZE];

#define RELEASE_FLAG "release "
release = strstr(relstring, RELEASE_FLAG);
if(release == NULL)
   {
   CfOut(cf_verbose,"","Could not find a numeric OS release in %s\n",filename);
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
      CfOut(cf_verbose,"","Could not break down release version numbers in %s\n",filename);
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

int Lsb_Version(void)

{ char vbuff[CF_BUFSIZE];
  char codename[CF_MAXVARSIZE];
  char distrib[CF_MAXVARSIZE];
  char release[CF_MAXVARSIZE];
  char classname[CF_MAXVARSIZE];;
  char *ret;
  int major = 0;
  int minor = 0;
  
if ((ret = Lsb_Release("--id")) != NULL)
   {
   strncpy(distrib,ret,CF_MAXVARSIZE);
   snprintf(classname, CF_MAXVARSIZE, "%s",ret);
   NewClass(classname);

   if ((ret = Lsb_Release("--codename")) != NULL)
      {
      strncpy(codename,ret,CF_MAXVARSIZE);
      snprintf(classname, CF_MAXVARSIZE, "%s_%s", distrib,codename);
      NewClass(classname);
      }

   if ((ret  = Lsb_Release("--release")) != NULL)
      {
      strncpy(release,ret,CF_MAXVARSIZE);
      
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

   NewScalar("sys","flavour",classname,cf_str);
   NewScalar("sys","flavor",classname,cf_str);
   return 0;
   }
else
   {
   CfOut(cf_verbose,""," !! No LSB information available on this Linux host - you should install the LSB packages");
   return -1;
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
   CfReadLine(buffer,CF_BUFSIZE,fp);
   Chop(buffer);
   if (sscanf(buffer,"VMware ESX Server %d.%d.%d",&major,&minor,&bug) > 0)
      {
      snprintf(classbuf,CF_BUFSIZE,"VMware ESX Server %d",major);
      NewClass(classbuf);
      snprintf(classbuf,CF_BUFSIZE,"VMware ESX Server %d.%d",major,minor);
      NewClass(classbuf);
      snprintf(classbuf,CF_BUFSIZE,"VMware ESX Server %d.%d.%d",major,minor,bug);
      NewClass(classbuf);
      sufficient = 1;
      }
   else if (sscanf(buffer,"VMware ESX Server %s",version) > 0)
      {
      snprintf(classbuf,CF_BUFSIZE,"VMware ESX Server %s",version);
      NewClass(classbuf);
      sufficient = 1;
      }
   fclose(fp);
   }

/* Fall back to checking for other files */

if (sufficient < 1 && ((fp = fopen("/etc/vmware-release","r")) != NULL) ||
    (fp = fopen("/etc/issue","r")) != NULL)
   {
   CfReadLine(buffer,CF_BUFSIZE,fp);
   Chop(buffer);
   NewClass(buffer);

   /* Strip off the release code name e.g. "(Dali)" */
   if ((sp = strchr(buffer,'(')) != NULL)
      {
      *sp = 0;
      Chop(buffer);
      NewClass(buffer);
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
      CfReadLine(buffer,CF_BUFSIZE,fp);
      if (strstr(buffer,"control_d"))
         {
         NewClass("xen_dom0");
         sufficient = 1;
         }
      }

   if (!sufficient)
      {
      NewClass("xen_domu_pv");
      sufficient = 1;
      }

   fclose(fp);
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

void *Lsb_Release(char *key)

{ char vbuff[CF_BUFSIZE];
  char info[CF_MAXVARSIZE];
  FILE *pp;
  struct stat sb;

snprintf(vbuff,CF_BUFSIZE, "/usr/bin/lsb_release");

if (cfstat(vbuff,&sb) == -1)
   {
   CfOut(cf_verbose,"","LSB probe \"%s\" doesn't exist",vbuff);
   return NULL;
   }

snprintf(vbuff,CF_BUFSIZE, "/usr/bin/lsb_release %s",key);

if ((pp = cf_popen(vbuff, "r")) == NULL)
   {
   return NULL;
   }

memset(info,0,CF_MAXVARSIZE);

if (CfReadLine(vbuff,CF_BUFSIZE,pp))
   {
   sscanf(vbuff,"%*[^:]: %255s",info);
   }

cf_pclose(pp);
return ToLowerStr(info);
}

/******************************************************************/

int GetCurrentUserName(char *userName, int userNameLen)
{
#ifdef MINGW
return NovaWin_GetCurrentUserName(userName, userNameLen);
#else
return Unix_GetCurrentUserName(userName, userNameLen);
#endif
}

/******************************************************************/

#ifndef MINGW

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
   int skip = false;
   
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

   if (UNDERSCORE_CLASSES)
      {
      snprintf(workbuf,CF_BUFSIZE, "_net_iface_%s", CanonifyName(ifp->ifr_name));
      }
   else
      {
      snprintf(workbuf,CF_BUFSIZE, "net_iface_%s", CanonifyName(ifp->ifr_name));
      }

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

         AppendItem(&IPADDRESSES,VIPADDRESS,"");

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
            NewClass(ip->name);
            }
         }

      DeleteItemList(list);
      }
   }

cf_pclose(pp);
}

/******************************************************************/

char *GetHome(uid_t uid)

{
struct passwd *mpw = getpwuid(uid);
return mpw->pw_dir;
}

#endif  /* NOT MINGW */
