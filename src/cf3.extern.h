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
/* File: cf3.extern.h                                                        */
/*                                                                           */
/* Created: Thu Aug  2 12:51:18 2007                                         */
/*                                                                           */
/*****************************************************************************/

#ifndef CFENGINE_CF3_EXTERN_H
#define CFENGINE_CF3_EXTERN_H

/* See variables in cf3globals.c and syntax.c */

#include "../pub/getopt.h"

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

extern int EDITFILESIZE;
extern int VIFELAPSED;
extern int VEXPIREAFTER;

extern char *OBS[CF_OBSERVABLES][2];

extern char *CF_DIGEST_TYPES[10][2];
extern int CF_DIGEST_SIZES[10];

/* Windows version constants */

extern unsigned int WINVER_MAJOR;
extern unsigned int WINVER_MINOR;
extern unsigned int WINVER_BUILD;

extern struct Topic *TOPICHASH[CF_HASHTABLESIZE];
extern struct PromiseParser P;
extern int REQUIRE_COMMENTS;
extern int FIPS_MODE;
extern char POLICY_SERVER[CF_BUFSIZE];
extern int ALWAYS_VALIDATE;
extern bool ALLCLASSESREPORT;
extern int LICENSES;
extern int AM_NOVA;
extern int AM_CONSTELLATION;
extern char EXPIRY[CF_SMALLBUF];
extern char LICENSE_COMPANY[CF_SMALLBUF];
extern int IGNORE_MISSING_INPUTS;
extern int IGNORE_MISSING_BUNDLES;
extern char WEBDRIVER[CF_MAXVARSIZE];
extern char DOCROOT[CF_MAXVARSIZE];
extern char BANNER[2*CF_BUFSIZE];
extern char FOOTER[CF_BUFSIZE];
extern char STYLESHEET[CF_BUFSIZE];
extern int CF_TOPICS;
extern int CF_OCCUR;
extern int CF_EDGES;
extern int KEYTTL;
extern struct Rlist *SERVER_KEYSEEN;
extern enum cfhashes CF_DEFAULT_DIGEST;
extern int CF_DEFAULT_DIGEST_LEN;
extern struct Item *EDIT_ANCHORS;

extern struct Bundle *BUNDLES;
extern struct Body *BODIES;
extern struct Scope *VSCOPE;
extern struct Audit *AUDITPTR;
extern struct Audit *VAUDIT; 
extern struct Rlist *VINPUTLIST;
extern struct Rlist *BODYPARTS;
extern struct Rlist *SUBBUNDLES;
extern struct Rlist *SINGLE_COPY_LIST;
extern struct Rlist *AUTO_DEFINE_LIST;
extern struct Rlist *SINGLE_COPY_CACHE;
extern struct Rlist *ACCESSLIST;
extern struct PromiseIdent *PROMISE_ID_LIST;
extern struct Item *DONELIST;
extern struct Rlist *CBUNDLESEQUENCE;
extern char *CBUNDLESEQUENCE_STR;
extern struct Item *ROTATED;
extern double FORGETRATE;
extern struct Rlist *GOALS;
extern struct Rlist *GOALCATEGORIES;

extern struct Rlist *CF_STCK;
extern int EDIT_MODEL;
extern int CF_STCKFRAME;
extern int CFA_BACKGROUND;
extern int CFA_BACKGROUND_LIMIT;
extern int AM_BACKGROUND_PROCESS;
extern int CF_PERSISTENCE;
extern int LOOKUP;
extern int BOOTSTRAP;
extern int XML;
extern FILE *FREPORT_HTML;
extern FILE *FREPORT_TXT;
extern FILE *FKNOW;
extern struct FnCallStatus FNCALL_STATUS;
extern int CSV;

extern struct SubTypeSyntax CF_NOSTYPE;
extern char *CF_DATATYPES[];
extern char *CF_AGENTTYPES[];
extern char HASHDB[CF_BUFSIZE];
extern int FSTAB_EDITS;

extern int CFA_MAXTHREADS;
extern char *THIS_BUNDLE;
extern char THIS_AGENT[CF_MAXVARSIZE];
extern enum cfagenttype THIS_AGENT_TYPE;
extern int INSTALL_SKIP;
extern int SHOWREPORTS;
extern int SHOW_PARSE_TREE;
extern char SYSLOGHOST[CF_MAXVARSIZE];
extern unsigned short SYSLOGPORT;
extern time_t PROMISETIME;
extern time_t CF_LOCKHORIZON;
extern int ABORTBUNDLE;
extern struct Item *ABORTBUNDLEHEAP;
extern int LASTSEENEXPIREAFTER;
extern char *DEFAULT_COPYTYPE;
extern struct Rlist *SERVERLIST;
extern struct Item *PROCESSTABLE;
extern struct Item *PROCESSREFRESH;
extern struct Item *FSTABLIST;
extern struct Rlist *MOUNTEDFSLIST;

extern int CF_MOUNTALL;
extern int CF_SAVEFSTAB;

extern const char *DAY_TEXT[];
extern const char *MONTH_TEXT[];
extern const char *SHIFT_TEXT[];

#if defined(NT) && !defined(__CYGWIN__)
#  define FILE_SEPARATOR '\\'
#  define FILE_SEPARATOR_STR "\\"
# else
#  define FILE_SEPARATOR '/'
#  define FILE_SEPARATOR_STR "/"
#endif

extern char SQL_DATABASE[CF_MAXVARSIZE];
extern char SQL_OWNER[CF_MAXVARSIZE];
extern char SQL_PASSWD[CF_MAXVARSIZE];
extern char SQL_SERVER[CF_MAXVARSIZE];
extern char SQL_CONNECT_NAME[CF_MAXVARSIZE];
extern enum cfdbtype SQL_TYPE;

extern double VAL_KEPT;
extern double VAL_REPAIRED;
extern double VAL_NOTKEPT;

#endif
