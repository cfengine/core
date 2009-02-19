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



/* STUBs from cf2 - should this be rewritten? */



/*******************************************************************/
/*                                                                 */
/*  INET checking for cfengine                                     */
/*                                                                 */
/*  This is based on the action of "ifconfig" for IP protocols     */
/*  It assumes that we are on the internet and uses ioctl to get   */
/*  the necessary info from the device. Sanity checking is done... */
/*                                                                 */
/* Sockets are very poorly documented. The basic socket adress     */
/* struct sockaddr is a generic type. Specific socket addresses    */
/* must be specified depending on the family or protocol being     */
/* used. e.g. if you're using the internet inet protocol, then     */
/* the fmaily is AF_INT and the socket address type is sockadr_in  */
/* Although it is not obvious, the documentation assures us that   */
/* we can cast a pointer of one type into a pointer of the other:  */
/*                                                                 */
/* Here's an example                                               */
/*                                                                 */
/*   #include <netinet/in.h>                                       */
/*                                                                 */
/*        struct in_addr adr;                                      */
/* e.g.   adr.s_addr = inet_addr("129.240.22.34");                 */
/*        printf("addr is %s\n",inet_ntoa(adr));                   */
/*                                                                 */
/*                                                                 */
/* We have to do the following in order to convert                 */
/* a sockaddr struct into a sockaddr_in struct required by the     */
/* ifreq struct!! These calls have no right to work, but somehow   */
/* they do!                                                        */
/*                                                                 */
/* struct sockaddr_in sin;                                         */
/* sin.sin_addr.s_addr = inet_addr("129.240.22.34");               */
/*                                                                 */
/* IFR.ifr_addr = *((struct sockaddr *) &sin);                     */
/*                                                                 */
/* sin = *(struct sockaddr_in *) &IFR.ifr_addr;                    */
/*                                                                 */
/* printf("IP address: %s\n",inet_ntoa(sin.sin_addr));             */
/*                                                                 */
/*******************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN sizeof("255.255.255.255")
#endif

#if !defined(NT) && !defined(IRIX)

/* IRIX makes the routing stuff obsolete unless we do this */
# undef sgi

struct ifreq IFR;

char VNUMBROADCAST[256];

# define cfproto 0

# ifndef IPPROTO_IP     /* Old boxes, hpux 7 etc */
#  define IPPROTO_IP 0
# endif

# ifndef SIOCSIFBRDADDR
#  define SIOCSIFBRDADDR  SIOCGIFBRDADDR
# endif

/*******************************************************************/

void VerifyInterfacePromise(char *vifdev,char *vaddress,char *vnetmask,char *vbroadcast)

{ int sk, flags, metric, isnotsane = false;
 
CfOut(cf_verbose,"","Assumed interface name: %s %s %s\n",vifdev,vnetmask,vbroadcast);

if (!IsPrivileged())                            
   {
   CfOut(cf_error,"","Only root can configure network interfaces.\n");
   return;
   }

if (vnetmask && strlen(vnetmask))
   {
   CfOut(cf_error,"","No netmask is promised for interface %s\n",vifdev);
   return;
   }

if (vbroadcast && strlen(vbroadcast))
   {
   CfOut(cf_error,"","No broadcast address is promised for the interface - calculating default\n");
   return;
   }

strcpy(IFR.ifr_name,vifdev);
IFR.ifr_addr.sa_family = AF_INET;

if ((sk = socket(AF_INET,SOCK_DGRAM,IPPROTO_IP)) == -1)
   {
   CfOut(cf_error,"socket","Unable to open a socket to examine interface %s\n",vifdev);
   return;
   }

if (ioctl(sk,SIOCGIFFLAGS, (caddr_t) &IFR) == -1)   /* Get the device status flags */
   {
   CfOut(cf_error,"ioctl","Promised network device was not found\n");
   return;
   }

flags = IFR.ifr_flags;
strcpy(IFR.ifr_name,vifdev);                   /* copy this each time */
 
if (ioctl(sk,SIOCGIFMETRIC, (caddr_t) &IFR) == -1)  /* Get the routing priority */
   {
   CfOut(cf_error,"ioctl","Error examining the routing metric\n");
   return;
   }

metric = IFR.ifr_metric;

isnotsane = GetPromisedIfStatus(sk,vifdev,vaddress,vnetmask,vbroadcast);

if (!DONTDO && isnotsane)
   {
   SetPromisedIfStatus(sk,vifdev,vaddress,vnetmask,vbroadcast);
   GetPromisedIfStatus(sk,vifdev,vaddress,vnetmask,vbroadcast);
   }

close(sk);
}


/*******************************************************************/

int GetPromisedIfStatus(int sk,char *vifdev,char *vaddress,char *vnetmask,char *vbroadcast)

{ struct sockaddr_in *sin;
  struct sockaddr_in netmask;
  int insane = false;
  struct hostent *hp;
  struct in_addr inaddr;
  char vbuff[CF_BUFSIZE];

CfOut(cf_verbose,""," -> Checking interface promises on %s\n",vifdev);
  
if ((hp = gethostbyname(VSYSNAME.nodename)) == NULL)
   {
   CfOut(cf_error,"gethostbyname","Error looking up host");
   return false;
   }
else
   {
   memcpy(&inaddr,hp->h_addr,hp->h_length);
   CfOut(cf_verbose,""," -> Address reported by nameserver: %s\n",inet_ntoa(inaddr));
   }

strcpy(IFR.ifr_name,vifdev);

if (ioctl(sk,SIOCGIFADDR, (caddr_t) &IFR) == -1)   /* Get the device status flags */
   {
   return false;
   }

sin = (struct sockaddr_in *) &IFR.ifr_addr;

if (strlen(vaddress) > 0)
   {
   if (strcmp(vaddress,(char *)inet_ntoa(sin->sin_addr)) != 0)
      {
      CfOut(cf_error,"","Interface %s is configured with an address which differs from that promised\n",vifdev);
      insane = true;
      }
   }
 
if (strcmp((char *)inet_ntoa(*(struct in_addr *)(hp->h_addr)),(char *)inet_ntoa(sin->sin_addr)) != 0)
   {
   CfOut(cf_error,"","Interface %s is configured with an address which differs from that promised\n",vifdev);
   insane = true;
   }

if (ioctl(sk,SIOCGIFNETMASK, (caddr_t) &IFR) == -1) 
   {
   return false;
   }

netmask.sin_addr = ((struct sockaddr_in *) &IFR.ifr_addr)->sin_addr;

CfOut(cf_verbose,""," -> Found netmask: %s\n",inet_ntoa(netmask.sin_addr));

strcpy(vbuff,inet_ntoa(netmask.sin_addr));

if (strcmp(vbuff,vnetmask))
   {
   CfOut(cf_error,"","Interface %s is configured with a netmask which differs from that promised\n",vifdev);
   insane = true;
   }

if (ioctl(sk,SIOCGIFBRDADDR, (caddr_t) &IFR) == -1) 
   {
   return false;
   }

sin = (struct sockaddr_in *) &IFR.ifr_addr;
strcpy(vbuff,inet_ntoa(sin->sin_addr));

CfOut(cf_verbose,""," -> Found broadcast address: %s\n",inet_ntoa(sin->sin_addr));

GetDefaultBroadcastAddr(inet_ntoa(inaddr),vifdev,vnetmask,vbroadcast);

if (strcmp(vbuff,VNUMBROADCAST) != 0)
   {
   CfOut(cf_error,"","Interface %s is configured with a broadcast address which differs from that promised\n",vifdev);
   insane = true;
   }

return(insane);
}

/*******************************************************************/

void SetPromisedIfStatus(int sk,char *vifdev,char *vaddress,char *vnetmask,char *vbroadcast)

{ struct sockaddr_in *sin;
  struct sockaddr_in netmask, broadcast;

   /*********************************

   Don't try to set the address yet...

    if (ioctl(sk,SIOCSIFADDR, (caddr_t) &IFR) == -1) 
      {
      perror ("Can't set IP address");
      return;
      } 


 REWRITE THIS TO USE ifconfig / ipconfig

   **********************************/

/* set netmask */

CfOut(cf_verbose,""," -> Resetting interface...\n");

memset(&IFR, 0, sizeof(IFR));
strncpy(IFR.ifr_name,vifdev,sizeof(IFR.ifr_name)); 
netmask.sin_addr.s_addr = inet_network(vnetmask);
netmask.sin_family = AF_INET;
IFR.ifr_addr = *((struct sockaddr *) &netmask);

sin = (struct sockaddr_in *) &IFR.ifr_addr;

if (ioctl(sk,SIOCSIFNETMASK, (caddr_t) &IFR) < 0) 
   {
   //cfPS(cf_error,CF_FAIL,"ioctl",pp,a,"Failed to set netmask to %s\n",inet_ntoa(netmask.sin_addr));
   }
else
   {
   //cfPS(cf_inform,CF_CHG,"",pp,a,"Setting netmask to %s\n",inet_ntoa(netmask.sin_addr));
   }

/* broadcast addr */

strcpy(IFR.ifr_name,vifdev);
broadcast.sin_addr.s_addr = inet_addr(VNUMBROADCAST);
IFR.ifr_addr = *((struct sockaddr *) &broadcast);
sin = (struct sockaddr_in *) &IFR.ifr_addr;

CfOut(cf_verbose,"","Trying to set broad to %s = %s\n",VNUMBROADCAST,inet_ntoa(sin->sin_addr));
 
if (ioctl(sk,SIOCSIFBRDADDR, (caddr_t) &IFR) == -1) 
   {
   //cfPS(cf_error,CF_FAIL,"ioctl",pp,a,"Failed to set broadcast address\n");
   return;
   } 

/*
if ((void *)(sin->sin_addr.s_addr) == (void *)NULL)
   {
   cfPS(cf_inform,CF_FAIL,"ioctl",pp,a,"Set broadcast address failed\n");
   }
else
   {
   cfPS(cf_inform,CF_CHG,"ioctl",pp,a,"Set broadcast address\n");
   }
*/
}

/*****************************************************/

void GetDefaultBroadcastAddr(char *ipaddr,char *vifdev,char *vnetmask,char *vbroadcast)

{ unsigned int na,nb,nc,nd;
  unsigned int ia,ib,ic,id;
  unsigned int ba,bb,bc,bd;
  unsigned netmask,ip,broadcast;

sscanf(vnetmask,"%u.%u.%u.%u",&na,&nb,&nc,&nd);

netmask = nd + 256*nc + 256*256*nb + 256*256*256*na;

sscanf(ipaddr,"%u.%u.%u.%u",&ia,&ib,&ic,&id);

ip = id + 256*ic + 256*256*ib + 256*256*256*ia;

if (strcmp(vbroadcast,"zero") == 0)
   {
   broadcast = ip & netmask;
   }
else if (strcmp(vbroadcast,"one") == 0)
   {
   broadcast = ip | (~netmask);
   }
else
   {
   return;
   }

ba = broadcast / (256 * 256 * 256);
bb = (broadcast / (256 * 256)) % 256;
bc = broadcast / (256) % 256;
bd = broadcast % 256;
sprintf(VNUMBROADCAST,"%u.%u.%u.%u",ba,bb,bc,bd);
}

/****************************************************************/
/*                                                              */
/* Routing Tables:                                              */
/*                                                              */
/* To check that we have at least one static route entry to     */
/* the nearest gateway -- i.e. the wildcard entry for "default" */
/* we need some way of accessing the routing tables. There is   */
/* no elegant way of doing this, alas.                          */
/*                                                              */
/****************************************************************/

void SetPromisedDefaultRoute()

{ int sk, defaultokay = 1;
  struct sockaddr_in sindst,singw;
  char oldroute[INET_ADDRSTRLEN];
  char routefmt[CF_MAXVARSIZE];
  char vbuff[CF_BUFSIZE];

/* These OSes have these structs defined but use the route command */
# if defined DARWIN || defined FREEBSD || defined OPENBSD || defined SOLARIS
#  undef HAVE_RTENTRY
#  undef HAVE_ORTENTRY
# endif

# ifdef HAVE_ORTENTRY
   struct ortentry route;
# else
#  if HAVE_RTENTRY
   struct rtentry route;
#  endif
# endif

  FILE *pp;

CfOut(cf_verbose,"","Looking for a default route...\n");

if (!IsPrivileged())                            
   {
   CfOut(cf_inform,"","Only root can set a default route.");
   return;
   }

if (VDEFAULTROUTE == NULL)
   {
   CfOut(cf_verbose,"","cfengine: No default route is defined. Ignoring the routing tables.\n");
   return;
   }

if ((pp = cf_popen(VNETSTAT[VSYSTEMHARDCLASS],"r")) == NULL)
   {
   CfOut(cf_error,"cf_popen","Failed to open pipe from %s\n",VNETSTAT[VSYSTEMHARDCLASS]);
   return;
   }

while (!feof(pp))
   {
   ReadLine(vbuff,CF_BUFSIZE,pp);

   Debug("LINE: %s = %s?\n",vbuff,VDEFAULTROUTE->name);
   
   if ((strncmp(vbuff,"default",7) == 0)||(strncmp(vbuff,"0.0.0.0",7) == 0))
      {
      /* extract the default route */
      /* format: default|0.0.0.0 <whitespace> route <whitespace> etc */
      if ((sscanf(vbuff, "%*[default0. ]%s%*[ ]", &oldroute)) == 1)
        {
        if ((strncmp(VDEFAULTROUTE->name, oldroute, INET_ADDRSTRLEN)) == 0)
          {
          CfOut(cf_verbose,"","cfengine: default route is already set to %s\n",VDEFAULTROUTE->name);
          defaultokay = 1;
          break;
          }
        else
          {
          CfOut(cf_verbose,"","cfengine: default route is set to %s, but should be %s.\n",oldroute,VDEFAULTROUTE->name);
          defaultokay = 2;
          break;
          }
        }
      }
   else
      {
      Debug("No default route is yet registered\n");
      defaultokay = 0;
      }
   }

cf_pclose(pp);

if (defaultokay == 1)
   {
   CfOut(cf_verbose,"","Default route is set and agrees with conditional policy\n");
   return;
   }

if (defaultokay == 0)
   {
   NewClass("no_default_route");
   }

if (IsExcluded(VDEFAULTROUTE->classes))
   {
   CfOut(cf_verbose,"","cfengine: No default route is applicable. Ignoring the routing tables.\n");
   return;   
   }

CfOut(cf_error,"","The default route is incorrect, trying to correct\n");

if (strcmp(VROUTE[VSYSTEMHARDCLASS], "-") != 0)
   {
   Debug ("Using route shell commands to set default route\n");

   if (defaultokay == 2)
      {
      if (! DONTDO)
         {
         /* get the route command and the format for the delete argument */
         snprintf(routefmt,CF_MAXVARSIZE,"%s %s",VROUTE[VSYSTEMHARDCLASS],VROUTEDELFMT[VSYSTEMHARDCLASS]);
         snprintf(vbuff,CF_MAXVARSIZE,routefmt,"default",VDEFAULTROUTE->name);

         if (ShellCommandReturnsZero(vbuff,false))
            {
            CfOut(cf_inform,"Removing old default route %s",vbuff);
            }
         else
            {
            CfOut(cf_error,"","Error removing route");
            }
         }
      }
   
   if (! DONTDO)
      {
      snprintf(routefmt,CF_MAXVARSIZE,"%s %s",VROUTE[VSYSTEMHARDCLASS],VROUTEADDFMT[VSYSTEMHARDCLASS]);
      snprintf(vbuff,CF_MAXVARSIZE,routefmt,"default",VDEFAULTROUTE->name);

      if (ShellCommandReturnsZero(vbuff,false))
         {
         CfOut(cf_inform,"","Setting default route %s",vbuff);
         }
      else
         {
         CfOut(cf_error,"","Error setting route");
         }
      }
   return;
   }
else
   {
#if defined HAVE_RTENTRY || defined HAVE_ORTENTRY
   Debug ("Using route ioctl to set default route\n");
   if ((sk = socket(AF_INET,SOCK_RAW,0)) == -1)
      {
      CfOut(cf_error,"socket","System class: ", CLASSTEXT[VSYSTEMHARDCLASS]);
      }
   else
      {
      sindst.sin_family = AF_INET;
      singw.sin_family = AF_INET;

      sindst.sin_addr.s_addr = INADDR_ANY;
      singw.sin_addr.s_addr = inet_addr(VDEFAULTROUTE->name);

      route.rt_dst = *(struct sockaddr *)&sindst;      /* This disgusting method is necessary */
      route.rt_gateway = *(struct sockaddr *)&singw;
      route.rt_flags = RTF_GATEWAY;

      if (! DONTDO)
         {
         if (ioctl(sk,SIOCADDRT, (caddr_t) &route) == -1)   /* Get the device status flags */
            {
            CfOut(cf_error,"ioctly SIOCADDRT","Error setting route");
            }
         else
            {
            CfOut(cf_error,"","Setting default route to %s\n",VDEFAULTROUTE->name);
            }
         }
      }
#else

   /* Socket routing - don't really know how to do this yet */ 

   CfOut(cf_verbose,"","Sorry don't know how to do routing on this platform\n");
 
#endif
   }
}

#else /* NT or IRIX */

void VerifyInterfacePromise(vifdev,vaddress,vnetmask,vbroadcast)

char *vifdev,*vaddress,*vnetmask, *vbroadcast;

{
CfOut(cf_verbose,"","Network configuration is not implemented on this OS\n");
}

void SetPromisedDefaultRoute()

{
CfOut(cf_verbose,"","Setting default route is not implemented on this OS\n"); 
}

#endif
