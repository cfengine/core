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

for (i = 0; CLASSATTRIBUTES[i][0] != '\0'; i++)
   {
   if (WildMatch(CLASSATTRIBUTES[i][0],ToLowerStr(VSYSNAME.sysname)))
      {
      if (WildMatch(CLASSATTRIBUTES[i][1],VSYSNAME.machine))
         {
         if (WildMatch(CLASSATTRIBUTES[i][2],VSYSNAME.release))
            {
            if (UNDERSCORE_CLASSES)
               {
               snprintf(workbuf,CF_BUFSIZE,"_%s",CLASSTEXT[i]);
               AddClassToHeap(workbuf);
               }
            else
               {
               AddClassToHeap(CLASSTEXT[i]);
               }
            found = true;
            VSYSTEMHARDCLASS = (enum classes) i;
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

if ((sp = malloc(strlen(VSYSNAME.nodename)+1)) == NULL)
   {
   FatalError("malloc failure in initialize()");
   }

strcpy(sp,VSYSNAME.nodename);
SetDomainName(sp);
strncpy(VUQNAME,sp,MAXHOSTNAMELEN);  /* Default assume non-qualified kernel name, correct below */
      
for (sp2=sp; *sp2 != '\0'; sp2++)  /* Truncate fully qualified name */
   {
   if (*sp2 == '.')
      {
      *sp2 = '\0';
      Debug("Truncating fully qualified hostname %s to %s\n",VSYSNAME.nodename,sp);
      strncpy(VUQNAME,sp,MAXHOSTNAMELEN);
      break;
      }
   }

VDEFAULTBINSERVER.name = sp;

AddClassToHeap(CanonifyName(sp));

free(sp); /* Release the ressource */

 
if ((tloc = time((time_t *)NULL)) == -1)
   {
   printf("Couldn't read system clock\n");
   }

if (VERBOSE || DEBUG || D2 || D3)
   {
   if (UNDERSCORE_CLASSES)
      {
      snprintf(workbuf,CF_BUFSIZE,"_%s",CLASSTEXT[i]);
      }
   else
      {
      snprintf(workbuf,CF_BUFSIZE,"%s",CLASSTEXT[i]);
      }

   printf ("Cfengine - \n%s\n%s\n\n",VERSION,COPYRIGHT);

   printf ("------------------------------------------------------------------------\n\n");
   printf ("Host name is: %s\n",VSYSNAME.nodename);
   printf ("Operating System Type is %s\n",VSYSNAME.sysname);
   printf ("Operating System Release is %s\n",VSYSNAME.release);
   printf ("Architecture = %s\n\n\n",VSYSNAME.machine);
   printf ("Using internal soft-class %s for host %s\n\n",workbuf,CLASSTEXT[VSYSTEMHARDCLASS]);
   printf ("The time is now %s\n\n",ctime(&tloc));
   printf ("------------------------------------------------------------------------\n\n");
   }

sprintf(workbuf,"%d_bit",sizeof(long)*8);
AddClassToHeap(workbuf);
Verbose("Additional hard class defined as: %s\n",CanonifyName(workbuf));

snprintf(workbuf,CF_BUFSIZE,"%s_%s",VSYSNAME.sysname,VSYSNAME.release);
AddClassToHeap(CanonifyName(workbuf));

#ifdef IRIX
/* Get something like `irix64_6_5_19m' defined as well as
   `irix64_6_5'.  Just copying the latter into VSYSNAME.release
   wouldn't be backwards-compatible.  */
snprintf(workbuf,CF_BUFSIZE,"%s_%s",VSYSNAME.sysname,real_version);
AddClassToHeap(CanonifyName(workbuf));
#endif

AddClassToHeap(CanonifyName(VSYSNAME.machine));
Verbose("Additional hard class defined as: %s\n",CanonifyName(workbuf));

snprintf(workbuf,CF_BUFSIZE,"%s_%s",VSYSNAME.sysname,VSYSNAME.machine);
AddClassToHeap(CanonifyName(workbuf));
Verbose("Additional hard class defined as: %s\n",CanonifyName(workbuf));

snprintf(workbuf,CF_BUFSIZE,"%s_%s_%s",VSYSNAME.sysname,VSYSNAME.machine,VSYSNAME.release);
AddClassToHeap(CanonifyName(workbuf));
Verbose("Additional hard class defined as: %s\n",CanonifyName(workbuf));

#ifdef HAVE_SYSINFO
#ifdef SI_ARCHITECTURE
sz = sysinfo(SI_ARCHITECTURE,workbuf,CF_BUFSIZE);
if (sz == -1)
  {
  Verbose("cfengine internal: sysinfo returned -1\n");
  }
else
  {
  AddClassToHeap(CanonifyName(workbuf));
  Verbose("Additional hard class defined as: %s\n",workbuf);
  }
#endif
#ifdef SI_PLATFORM
sz = sysinfo(SI_PLATFORM,workbuf,CF_BUFSIZE);
if (sz == -1)
  {
  Verbose("cfengine internal: sysinfo returned -1\n");
  }
else
  {
  AddClassToHeap(CanonifyName(workbuf));
  Verbose("Additional hard class defined as: %s\n",workbuf);
  }
#endif
#endif

snprintf(workbuf,CF_BUFSIZE,"%s_%s_%s_%s",VSYSNAME.sysname,VSYSNAME.machine,VSYSNAME.release,VSYSNAME.version);

if (strlen(workbuf) < CF_MAXVARSIZE-2)
   {
   VARCH = strdup(CanonifyName(workbuf));
   }
else
   {
   Verbose("cfengine internal: $(arch) overflows CF_MAXVARSIZE! Truncating\n");
   VARCH = strdup(CanonifyName(VSYSNAME.sysname));
   }

snprintf(workbuf,CF_BUFSIZE,"%s_%s",VSYSNAME.sysname,VSYSNAME.machine);

VARCH2 = strdup(CanonifyName(workbuf));
 
AddClassToHeap(VARCH);

Verbose("Additional hard class defined as: %s\n",VARCH);

if (! found)
   {
   CfLog(cferror,"Cfengine: I don't understand what architecture this is!","");
   }

strcpy(workbuf,"compiled_on_"); 
strcat(workbuf,CanonifyName(AUTOCONF_SYSNAME));
AddClassToHeap(CanonifyName(workbuf));
Verbose("\nGNU autoconf class from compile time: %s\n\n",workbuf);

/* Get IP address from nameserver */

if ((hp = gethostbyname(VSYSNAME.nodename)) == NULL)
   {
   return;
   }
else
   {
   memset(&cin,0,sizeof(cin));
   cin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
   Verbose("Address given by nameserver: %s\n",inet_ntoa(cin.sin_addr));
   strcpy(VIPADDRESS,inet_ntoa(cin.sin_addr));
   
   for (i=0; hp->h_aliases[i]!= NULL; i++)
      {
      Debug("Adding alias %s..\n",hp->h_aliases[i]);
      AddClassToHeap(CanonifyName(hp->h_aliases[i])); 
      }
   }
}

/*********************************************************************/

void GetInterfaceInfo3(void)

{ int fd,len,i,j;
  struct ifreq ifbuf[CF_IFREQ],ifr, *ifp;
  struct ifconf list;
  struct sockaddr_in *sin;
  struct hostent *hp;
  char *sp, workbuf[CF_BUFSIZE];
  char ip[CF_MAXVARSIZE];
  char name[CF_MAXVARSIZE];
            

Debug("GetInterfaceInfo3()\n");

if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
   {
   CfLog(cferror,"Couldn't open socket","socket");
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
   CfLog(cferror,"Couldn't get interfaces - old kernel? Try setting CF_IFREQ to 1024","ioctl");
   exit(1);
   }

for (j = 0,len = 0,ifp = list.ifc_req; len < list.ifc_len; len+=SIZEOF_IFREQ(*ifp),j++,ifp=(struct ifreq *)((char *)ifp+SIZEOF_IFREQ(*ifp)))
   {
   if (ifp->ifr_addr.sa_family == 0)
       {
       continue;
       }

   Verbose("Interface %d: %s\n", j+1, ifp->ifr_name);

   if (UNDERSCORE_CLASSES)
      {
      snprintf(workbuf, CF_BUFSIZE, "_net_iface_%s", CanonifyName(ifp->ifr_name));
      }
   else
      {
      snprintf(workbuf, CF_BUFSIZE, "net_iface_%s", CanonifyName(ifp->ifr_name));
      }

   AddClassToHeap(workbuf);
   
   if (ifp->ifr_addr.sa_family == AF_INET)
      {
      strncpy(ifr.ifr_name,ifp->ifr_name,sizeof(ifp->ifr_name));
      
      if (ioctl(fd,SIOCGIFFLAGS,&ifr) == -1)
         {
         CfLog(cferror,"No such network device","ioctl");
         close(fd);
         return;
         }

      if ((ifr.ifr_flags & IFF_BROADCAST) && !(ifr.ifr_flags & IFF_LOOPBACK))
         {
         sin=(struct sockaddr_in *)&ifp->ifr_addr;
   
         if ((hp = gethostbyaddr((char *)&(sin->sin_addr.s_addr),sizeof(sin->sin_addr.s_addr),AF_INET)) == NULL)
            {
            Debug("Host information for %s not found\n", inet_ntoa(sin->sin_addr));
            }
         else
            {
            if (hp->h_name != NULL)
               {
               Debug("Adding hostip %s..\n",inet_ntoa(sin->sin_addr));
               AddClassToHeap(CanonifyName(inet_ntoa(sin->sin_addr)));
               Debug("Adding hostname %s..\n",hp->h_name);
               AddClassToHeap(CanonifyName(hp->h_name));

               if (hp->h_aliases != NULL)
                  {
                  for (i=0; hp->h_aliases[i] != NULL; i++)
                     {
                     Debug("Adding alias %s..\n",hp->h_aliases[i]);
                     AddClassToHeap(CanonifyName(hp->h_aliases[i]));
                     }
                  }
               }
            }
         
         /* Old style compat */
         strcpy(ip,inet_ntoa(sin->sin_addr));
         AppendItem(&IPADDRESSES,ip,"");
         
         for (sp = ip+strlen(ip)-1; *sp != '.'; sp--)
            {
            }
         *sp = '\0';
         AddClassToHeap(CanonifyName(ip));
         
            
         /* New style classes */
         strcpy(ip,"ipv4_");
         strcat(ip,inet_ntoa(sin->sin_addr));
         AddClassToHeap(CanonifyName(ip));

         for (sp = ip+strlen(ip)-1; (sp > ip); sp--)
            {
            if (*sp == '.')
               {
               *sp = '\0';
               AddClassToHeap(CanonifyName(ip));
               }
            }

         /* Matching variables */

         strcpy(ip,inet_ntoa(sin->sin_addr));
         snprintf(name,CF_MAXVARSIZE-1,"ipv4[%s]",CanonifyName(ifp->ifr_name));
         NewScalar(CONTEXTID,name,ip,cf_str);

         i = 3;
         
         for (sp = ip+strlen(ip)-1; (sp > ip); sp--)
            {
            if (*sp == '.')
               {
               *sp = '\0';
               snprintf(name,CF_MAXVARSIZE-1,"ipv4_%d[%s]",i--,CanonifyName(ifp->ifr_name));
               NewScalar(CONTEXTID,name,ip,cf_str);
               }
            }         
         }
      }
   }
 
close(fd);
}
