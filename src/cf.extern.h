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

#ifndef CFENGINE_CF_EXTERN_H
#define CFENGINE_CF_EXTERN_H

#include "../pub/getopt.h"

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
extern pthread_mutex_t MUTEX_SYSCALL;
extern pthread_mutex_t MUTEX_LOCK;
extern pthread_attr_t PTHREADDEFAULTS;
extern pthread_mutex_t MUTEX_COUNT;
extern pthread_mutex_t MUTEX_OUTPUT;
extern pthread_mutex_t MUTEX_DBHANDLE;
extern pthread_mutex_t MUTEX_POLICY;
extern pthread_mutex_t MUTEX_GETADDR;
extern pthread_mutex_t MUTEX_DB_LASTSEEN;
extern pthread_mutex_t MUTEX_DB_REPORT;
extern pthread_mutex_t MUTEX_VSCOPE;
extern pthread_mutex_t MUTEX_SERVER_KEYSEEN;
extern pthread_mutex_t MUTEX_SERVER_CHILDREN;
# endif

extern pid_t ALARM_PID;
extern RSA *PRIVKEY, *PUBKEY;
extern char PUBKEY_DIGEST[CF_MAXVARSIZE];
extern char BINDINTERFACE[CF_BUFSIZE];
extern struct sock ECGSOCKS[ATTR];
extern char *TCPNAMES[CF_NETATTR];

extern struct Audit *AUDITPTR;
extern struct Audit *VAUDIT; 

extern int PR_KEPT;
extern int PR_REPAIRED;
extern int PR_NOTKEPT;

extern char CONTEXTID[32];
extern char PADCHAR;
extern struct Item *IPADDRESSES;

extern char PIDFILE[CF_BUFSIZE];
extern char  STR_CFENGINEPORT[16];
extern unsigned short SHORT_CFENGINEPORT;
extern time_t CONNTIMEOUT;
extern time_t RECVTIMEOUT;

extern char CFLOCK[CF_BUFSIZE];
extern char CFLOG[CF_BUFSIZE];
extern char CFLAST[CF_BUFSIZE];
extern char LOCKDB[CF_BUFSIZE];

extern int CFSIGNATURE; /*?*/

extern char CFPUBKEYFILE[CF_BUFSIZE];
extern char CFPRIVKEYFILE[CF_BUFSIZE];
extern char CFWORKDIR[CF_BUFSIZE];
extern char AVDB[CF_MAXVARSIZE];

extern char VYEAR[];
extern char VDAY[];
extern char VMONTH[];
extern char VSHIFT[];

extern char *CLASSTEXT[];
extern char *CLASSATTRIBUTES[CF_CLASSATTR][CF_ATTRDIM];
extern char VINPUTFILE[];
extern CF_DB *AUDITDBP;
extern int AUDIT;
extern char REPOSCHAR;
extern char PURGE;
extern int  CHECKSUMUPDATES;

extern int ERRORCOUNT;
extern time_t CFSTARTTIME;
extern time_t CFINITSTARTTIME;

extern struct utsname VSYSNAME;
extern mode_t DEFAULTMODE;
extern char *PROTOCOL[];
extern char VIPADDRESS[];
extern char VPREFIX[];
extern int VRECURSE;
extern int RPCTIMEOUT;

extern int SKIPIDENTIFY;
extern char  DEFAULTCOPYTYPE;

extern char VDOMAIN[CF_MAXVARSIZE];
extern char VMAILSERVER[CF_BUFSIZE];
extern struct Item *VDEFAULTROUTE;
extern char *VREPOSITORY;
extern enum classes VSYSTEMHARDCLASS;
extern char VFQNAME[];
extern char VUQNAME[];
extern char LOGFILE[];

extern struct Item *VNEGHEAP;
extern struct Item *VDELCLASSES;
extern struct Item *ABORTHEAP;

extern struct Mounted *MOUNTED;             /* Files systems already mounted */
extern struct Item *VSETUIDLIST;
extern struct Item *SUSPICIOUSLIST;
extern struct Item *SCHEDULE;
extern struct Item *NONATTACKERLIST;
extern struct Item *MULTICONNLIST;
extern struct Item *TRUSTKEYLIST;
extern struct Item *DHCPLIST;
extern struct Item *ALLOWUSERLIST;
extern struct Item *SKIPVERIFY;
extern struct Item *ATTACKERLIST;
extern struct AlphaList VHEAP; 
extern struct AlphaList VADDCLASSES;
extern struct Rlist *PRIVCLASSHEAP;

extern struct Item *VREPOSLIST;

extern struct Auth *VADMIT;
extern struct Auth *VDENY;
extern struct Auth *VADMITTOP;
extern struct Auth *VDENYTOP;
extern struct Auth *VARADMIT;
extern struct Auth *VARADMITTOP;
extern struct Auth *VARDENY;
extern struct Auth *VARDENYTOP;

extern int DEBUG;
extern int D1;
extern int D2;
extern int D3;
extern int D4;

extern int PARSING;

extern int VERBOSE;
extern int EXCLAIM;
extern int INFORM;

extern int LOGGING;
extern int CFPARANOID;

extern int DONTDO;
extern int IGNORELOCK;
extern int MINUSF;

extern int NOSPLAY;

extern char *VPSCOMM[];
extern char *VPSOPTS[];
extern char *VMOUNTCOMM[];
extern char *VMOUNTOPTS[];
extern char *VRESOLVCONF[];
extern char *VHOSTEQUIV[];
extern char *VFSTAB[];
extern char *VMAILDIR[];
extern char *VNETSTAT[];
extern char *VEXPORTS[];
extern char *VROUTE[];
extern char *VROUTEADDFMT[];
extern char *VROUTEDELFMT[];

extern char *VUNMOUNTCOMM[];

extern char *SIGNALS[];

#ifndef MINGW
extern char *tzname[2]; /* see man ctime */
#endif

extern int EDITFILESIZE;
extern int EDITBINFILESIZE;
extern int VIFELAPSED;
extern int VEXPIREAFTER;

extern char *OBS[CF_OBSERVABLES][2];

extern char *CF_DIGEST_TYPES[10][2];
extern int CF_DIGEST_SIZES[10];

/* Windows version constants */

extern unsigned int WINVER_MAJOR;
extern unsigned int WINVER_MINOR;
extern unsigned int WINVER_BUILD;

#endif
