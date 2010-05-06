/* cfengine for GNU
 
        Copyright (C) 1995
        Free Software Foundation, Inc.
 
   This file is part of GNU cfengine - written and maintained 
   by Cfengine AS, Dept of Computing and Engineering, Oslo College,
   Dept. of Theoretical physics, University of Oslo
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */
 

/*******************************************************************/
/*                                                                 */
/*  extern HEADER for cfengine                                     */
/*                                                                 */
/*******************************************************************/


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
# endif

extern pid_t ALARM_PID;
extern int INSTALLALL;
extern int ALL_SINGLECOPY;
extern int PASS;
extern RSA *PRIVKEY, *PUBKEY;
extern char BINDINTERFACE[CF_BUFSIZE];
extern struct sock ECGSOCKS[ATTR];
extern char *TCPNAMES[CF_NETATTR];
extern char **METHODARGV;
extern int GOTMETHODARGS;
extern struct Item *QUERYVARS;
extern struct Item *METHODRETURNVARS;
extern struct Item *METHODRETURNCLASSES;

extern struct Audit *AUDITPTR;
extern struct Audit *VAUDIT; 

extern int PR_KEPT;
extern int PR_REPAIRED;
extern int PR_NOTKEPT;

extern char METHODFILENAME[CF_BUFSIZE];
extern char *VMETHODPROTO[];
extern int METHODARGC;
extern char METHODREPLYTO[CF_BUFSIZE];
extern char METHODFOR[CF_BUFSIZE];
extern char METHODFORCE[CF_BUFSIZE];
extern char CONTEXTID[32];
extern char METHODNAME[CF_BUFSIZE];
extern char METHODMD5[CF_BUFSIZE];
extern char PADCHAR;
extern struct cfagent_connection *CONN;
extern int AUTHENTICATED;
extern struct Item *IPADDRESSES;

extern char PIDFILE[CF_BUFSIZE];
extern char  STR_CFENGINEPORT[16];
extern unsigned short SHORT_CFENGINEPORT;

extern char CFLOCK[CF_BUFSIZE];
extern char SAVELOCK[CF_BUFSIZE];
extern char CFLOG[CF_BUFSIZE];
extern char CFLAST[CF_BUFSIZE];
extern char LOCKDB[CF_BUFSIZE];
extern char EDITBUFF[CF_BUFSIZE];

extern char *tzname[2];
extern int CFSIGNATURE;
extern char CFDES1[8];
extern char CFDES2[8];
extern char CFDES3[8];

extern char CFPUBKEYFILE[CF_BUFSIZE];
extern char CFPRIVKEYFILE[CF_BUFSIZE];
extern char CFWORKDIR[CF_BUFSIZE];
extern char AVDB[CF_MAXVARSIZE];

extern dev_t ROOTDEVICE;
extern char *VPRECONFIG;
extern char *VRCFILE;

extern char *VARCH;
extern char *VARCH2;
extern char VYEAR[];
extern char VDAY[];
extern char VMONTH[];
extern char VHR[];
extern char VMINUTE[];
extern char VSEC[];
extern char VSHIFT[];
extern char VLIFECYCLE[];

extern char *ACTIONTEXT[];
extern char *ACTIONID[];
extern char *BUILTINS[];
extern char *CLASSTEXT[];
extern char *CLASSATTRIBUTES[CF_CLASSATTR][CF_ATTRDIM];
extern char *FILEACTIONTEXT[];
extern char *COMMATTRIBUTES[];
extern char VINPUTFILE[];
extern char *VCANONICALFILE;
extern char VCURRENTFILE[];
extern char VLOGFILE[];
extern char *CHDIR;
extern char *VSETUIDLOG;
extern FILE *VLOGFP;
extern CF_DB *AUDITDBP;
extern int AUDIT;
extern char VEDITABORT[];
extern char LISTSEPARATOR;
extern char REPOSCHAR;
extern char DISCOMP;
extern char USESHELL;
extern char PREVIEW;
extern char PURGE;
extern char CHECKSUM;
extern char COMPRESS;
extern int  CHECKSUMUPDATES;
extern int  DISABLESIZE;

extern char VLOGDIR[];
extern char VLOCKDIR[];

extern struct tm TM1;
extern struct tm TM2;

extern int ERRORCOUNT;
extern int NUMBEROFEDITS;
extern time_t CFSTARTTIME;
extern time_t CFINITSTARTTIME;
extern int CF_TIMEOUT;

extern struct utsname VSYSNAME;
extern int LINENUMBER;
extern mode_t DEFAULTMODE;
extern mode_t DEFAULTSYSTEMMODE;
extern int HAVEUID;
extern char *FINDERTYPE;
extern char *VUIDNAME;
extern char *VGIDNAME;
extern char CFSERVER[];
extern char *PROTOCOL[];
extern char VIPADDRESS[];
extern char VPREFIX[];
extern int VRECURSE;
extern int VAGE;
extern int RPCTIMEOUT;
extern char MOUNTMODE;
extern char DELETEDIR;
extern char DELETEFSTAB;
extern char FORCE;
extern char FORCEIPV4;
extern char FORCELINK;
extern char FORCEDIRS;
extern char STEALTH;
extern char PRESERVETIMES;
extern char TRUSTKEY;
extern char FORK;

extern int FULLENCRYPT;
extern int SKIPIDENTIFY;
extern int COMPATIBILITY_MODE;
extern int LINKSILENT;
extern int UPDATEONLY;
extern char  LINKTYPE;
extern char  AGETYPE;
extern char  COPYTYPE;
extern char  DEFAULTCOPYTYPE;
extern char  LINKDIRS;
extern char  LOGP;
extern char  INFORMP;
extern char  AUDITP;

extern char *FILTERNAME;
extern char *STRATEGYNAME;
extern char *CURRENTOBJECT;
extern char *CURRENTITEM;
extern char *GROUPBUFF;
extern char *ACTIONBUFF;
extern char *CLASSBUFF;
extern char ALLCLASSBUFFER[4*CF_BUFSIZE];
extern char CHROOT[CF_BUFSIZE];
extern char ELSECLASSBUFFER[CF_BUFSIZE];
extern char FAILOVERBUFFER[CF_BUFSIZE];
extern char *LINKFROM;
extern char *LINKTO;
extern char *MOUNTFROM;
extern char *MOUNTONTO;
extern char *MOUNTOPTS;
extern char *DESTINATION;
extern char *IMAGEACTION;

extern char *EXPR;
extern char *CURRENTAUTHPATH;
extern char *RESTART;
extern char *FILTERDATA;
extern char *STRATEGYDATA;
extern char *PKGVER;

extern int PROSIGNAL;
extern char  PROACTION;
extern char PROCOMP;
extern char IMGCOMP;

extern int IMGSIZE;


extern char *CHECKSUMDB;
extern char *COMPRESSCOMMAND;

extern char *HASH[CF_HASHTABLESIZE];

extern char VBUFF[CF_BUFSIZE];
extern char OUTPUT[CF_BUFSIZE*2];

extern char VFACULTY[CF_MAXVARSIZE];
extern char VDOMAIN[CF_MAXVARSIZE];
extern char VSYSADM[CF_MAXVARSIZE];
extern char VNETMASK[CF_MAXVARSIZE];
extern char VBROADCAST[CF_MAXVARSIZE];
extern char VMAILSERVER[CF_BUFSIZE];
extern struct Item *VTIMEZONE;
extern struct Item *VDEFAULTROUTE;
extern char VNFSTYPE[CF_MAXVARSIZE];
extern char *VREPOSITORY;
extern char *LOCALREPOS;
extern char VIFNAME[16];
extern char VIFNAMEOVERRIDE[16];
extern enum classes VSYSTEMHARDCLASS;
extern char VFQNAME[];
extern char VUQNAME[];
extern char LOGFILE[];

extern char *CMPSENSETEXT[];
extern char *CMPSENSEOPERAND[];

extern char NOABSPATH;
extern char CHKROOT;

extern struct Item *VEXCLUDECACHE;
extern struct Item *VSINGLECOPY;
extern struct Item *VAUTODEFINE;
extern struct Item *VEXCLUDECOPY;
extern struct Item *VEXCLUDELINK;
extern struct Item *VCOPYLINKS;
extern struct Item *VLINKCOPIES;
extern struct Item *VEXCLUDEPARSE;
extern struct Item *VCPLNPARSE;
extern struct Item *VINCLUDEPARSE;
extern struct Item *VIGNOREPARSE;
extern struct Item *VACLBUILD;
extern struct Item *VFILTERBUILD;
extern struct Item *VSTRATEGYBUILD;


extern struct Item *VMOUNTLIST;
extern struct Item *VHEAP;      /* Points to the base of the attribute heap */
extern struct Item *VNEGHEAP;
extern struct Item *VDELCLASSES;
extern struct Item *ABORTHEAP;

/* For packages: */
extern struct Package *VPKG;
extern struct Package *VPKGTOP;

/* HvB : Bas van der Vlies */
extern struct Mountables *VMOUNTABLES;  /* Points to the list of mountables */
extern struct Mountables *VMOUNTABLESTOP;

extern struct cfObject *VOBJTOP;
extern struct cfObject *VOBJ;


extern char *PARSEMETHODRETURNCLASSES;

extern struct Item *METHODARGS;
extern flag  CF_MOUNT_RO;                  /* mount directory readonly */

extern struct Item *VALERTS;
extern struct Item *VMOUNTED;
extern struct Tidy *VTIDY;               /* Points to the list of tidy specs */
extern struct Disk *VREQUIRED;              /* List of required file systems */
extern struct Disk *VREQUIREDTOP;
extern struct ShellComm *VSCRIPT;              /* List of scripts to execute */
extern struct ShellComm *VSCRIPTTOP;
extern struct ShellComm *VSCLI;
extern struct ShellComm *VSCLITOP;
extern struct Interface *VIFLIST;
extern struct Interface *VIFLISTTOP;
extern struct Mounted *MOUNTED;             /* Files systems already mounted */
extern struct Item VDEFAULTBINSERVER;
extern struct Item *VBINSERVERS;
extern struct Link *VLINK;
extern struct File *VFILE;
extern struct Item *VHOMESERVERS;
extern struct Item *VSETUIDLIST;
extern struct Disable *VDISABLELIST;
extern struct Disable *VDISABLETOP;
extern struct File *VMAKEPATH;
extern struct File *VMAKEPATHTOP;
extern struct Link *VCHLINK;
extern struct Item *VIGNORE;
extern struct Item *VHOMEPATLIST;
extern struct Item *EXTENSIONLIST;
extern struct Item *SUSPICIOUSLIST;
extern struct Item *SCHEDULE;
extern struct Item *SPOOLDIRLIST;
extern struct Item *NONATTACKERLIST;
extern struct Item *MULTICONNLIST;
extern struct Item *TRUSTKEYLIST;
extern struct Item *DHCPLIST;
extern struct Item *ALLOWUSERLIST;
extern struct Item *SKIPVERIFY;
extern struct Item *ATTACKERLIST;
extern struct Item *MOUNTOPTLIST;
extern struct Item *VRESOLVE;
extern struct MiscMount *VMISCMOUNT;
extern struct MiscMount *VMISCMOUNTTOP;
extern struct Item *VIMPORT;
extern struct Item *VACTIONSEQ;
extern struct Item *VACCESSLIST;
extern struct Item *VADDCLASSES;
extern struct Item *VALLADDCLASSES;
extern struct Rlist *PRIVCLASSHEAP;

extern struct Item *VJUSTACTIONS;
extern struct Item *VAVOIDACTIONS;
extern struct Edit *VEDITLIST;
extern struct Edit *VEDITLISTTOP;
extern struct Filter *VFILTERLIST;
extern struct Filter *VFILTERLISTTOP;
extern struct Strategy *VSTRATEGYLIST;
extern struct Strategy *VSTRATEGYLISTTOP;

extern struct CFACL  *VACLLIST;
extern struct CFACL  *VACLLISTTOP;
extern struct UnMount *VUNMOUNT;
extern struct UnMount *VUNMOUNTTOP;
extern struct Item *VCLASSDEFINE;
extern struct Image *VIMAGE;
extern struct Image *VIMAGETOP;
extern struct Method *VMETHODS;
extern struct Method *VMETHODSTOP;
extern struct Process *VPROCLIST;
extern struct Process *VPROCTOP;
extern struct Item *VSERVERLIST;
extern struct Item *VRPCPEERLIST;
extern struct Item *VREDEFINES;

extern struct Item *VREPOSLIST;

extern struct Auth *VADMIT;
extern struct Auth *VDENY;
extern struct Auth *VADMITTOP;
extern struct Auth *VDENYTOP;
extern struct Auth *VARADMIT;
extern struct Auth *VARADMITTOP;
extern struct Auth *VARDENY;
extern struct Auth *VARDENYTOP;

/* Associated variables which simplify logic */

extern struct Link *VLINKTOP;
extern struct Link *VCHLINKTOP;
extern struct Tidy *VTIDYTOP;
extern struct File *VFILETOP;

extern char *COPYRIGHT;

extern int DEBUG;
extern int D1;
extern int D2;
extern int D3;
extern int D4;

extern int PARSING;
extern int SHOWDB;
extern int ISCFENGINE;

extern int VERBOSE;
extern int EXCLAIM;
extern int INFORM;
extern int CHECK;

extern int PIFELAPSED;
extern int PEXPIREAFTER;
extern int LOGGING;
extern int INFORM_save;
extern int LOGGING_save;
extern int CFPARANOID;
extern int SHOWACTIONS;
extern int LOGTIDYHOMEFILES;

extern char TIDYDIRS;
extern int TRAVLINKS;
extern int DEADLINKS;
extern int PTRAVLINKS;
extern int DONTDO;
extern int IFCONF;
extern int PARSEONLY;
extern int GOTMOUNTINFO;
extern int NOMOUNTS;
extern int NOMODULES;
extern int NOPROCS;
extern int NOMETHODS;
extern int NOFILECHECK;
extern int NOTIDY;
extern int NOSCRIPTS;
extern int PRSYSADM;
extern int PRSCHEDULE;
extern int MOUNTCHECK;
extern int NOEDITS;
extern int KILLOLDLINKS;
extern int IGNORELOCK;
extern int NOPRECONFIG;
extern int WARNINGS;
extern int NONALPHAFILES;
extern int MINUSF;
extern int NOLINKS;
extern int ENFORCELINKS;
extern int NOCOPY;
extern int FORCENETCOPY;
extern int SILENT;
extern int EDITVERBOSE;
extern char IMAGEBACKUP;
extern int   TIDYSIZE;
extern int USEENVIRON;
extern int PROMATCHES;
extern int EDABORTMODE;
extern int NOPROCS;
extern int UNDERSCORE_CLASSES;
extern int NOHARDCLASSES;
extern int NOSPLAY;
extern int DONESPLAY;
extern char XDEV;
extern char RXDIRS;
extern char TYPECHECK;
extern char SCAN;

extern mode_t PLUSMASK;
extern mode_t MINUSMASK;

extern u_long PLUSFLAG;
extern u_long MINUSFLAG;

extern flag  ACTION_IS_LINK;
extern flag  ACTION_IS_LINKCHILDREN;
extern flag  MOUNT_ONTO;
extern flag  MOUNT_FROM;
extern flag  HAVE_RESTART;
extern flag  ACTIONPENDING;
extern flag  HOMECOPY;
extern char ENCRYPT;
extern char VERIFY;
extern char COMPATIBILITY;

extern char *VPSCOMM[];
extern char *VPSOPTS[];
extern char *VMOUNTCOMM[];
extern char *VMOUNTOPTS[];
extern char *VIFDEV[];
extern char *VETCSHELLS[];
extern char *VRESOLVCONF[];
extern char *VHOSTEQUIV[];
extern char *VFSTAB[];
extern char *VMAILDIR[];
extern char *VNETSTAT[];
extern char *VEXPORTS[];
extern char *VROUTE[];
extern char *VROUTEADDFMT[];
extern char *VROUTEDELFMT[];

extern char *ACTIONSEQTEXT[];
extern char *VEDITNAMES[];
extern char *VFILTERNAMES[];
extern char *VUNMOUNTCOMM[];
extern char *VRESOURCES[];
extern char *PKGMGRTEXT[];
extern char *PKGACTIONTEXT[];

extern int VTIMEOUT;
extern mode_t UMASK;

extern char *SIGNALS[];

extern char *tzname[2]; /* see man ctime */

extern int SENSIBLEFILECOUNT;
extern int SENSIBLEFSSIZE;
extern int EDITFILESIZE;
extern int EDITBINFILESIZE;
extern int VIFELAPSED;
extern int VEXPIREAFTER;
extern int VDEFAULTIFELAPSED;
extern int VDEFAULTEXPIREAFTER;
extern int AUTOCREATED;

extern enum fileactions FILEACTION;

extern enum cmpsense CMPSENSE;
extern enum pkgmgrs PKGMGR;
extern enum pkgmgrs DEFAULTPKGMGR;
extern enum pkgactions PKGACTION;

extern unsigned short PORTNUMBER;

extern int CURRENTLINENUMBER;
extern struct Item *CURRENTLINEPTR;

extern int EDITGROUPLEVEL;
extern int SEARCHREPLACELEVEL;
extern int FOREACHLEVEL;

extern char COMMENTSTART[], COMMENTEND[];

extern char *OBS[CF_OBSERVABLES][2];

extern char *CF_SCLICODES[CF_MAX_SCLICODES][2];


/* GNU REGEXP */

extern struct re_pattern_buffer *SEARCHPATTBUFF;
extern struct re_pattern_buffer *PATTBUFFER;

extern char *CF_DIGEST_TYPES[10][2];
extern int CF_DIGEST_SIZES[10];

/* Windows version constants */

extern unsigned int WINVER_MAJOR;
extern unsigned int WINVER_MINOR;
extern unsigned int WINVER_BUILD;
