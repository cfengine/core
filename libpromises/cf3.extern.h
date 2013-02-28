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

#ifndef CFENGINE_CF3_EXTERN_H
#define CFENGINE_CF3_EXTERN_H

/* See variables in cf3globals.c and syntax.c */

extern pid_t ALARM_PID;
extern RSA *PRIVKEY, *PUBKEY;
extern char PUBKEY_DIGEST[CF_MAXVARSIZE];
extern char BINDINTERFACE[CF_BUFSIZE];

extern Audit *AUDITPTR;
extern Audit *VAUDIT;

extern char CONTEXTID[];
extern Item *IPADDRESSES;

extern char STR_CFENGINEPORT[16];
extern unsigned short SHORT_CFENGINEPORT;
extern time_t CONNTIMEOUT;

extern char CFLOCK[CF_BUFSIZE];
extern char CFLOG[CF_BUFSIZE];
extern char CFLAST[CF_BUFSIZE];

extern char CFPUBKEYFILE[CF_BUFSIZE];
extern char CFPRIVKEYFILE[CF_BUFSIZE];
extern char CFWORKDIR[CF_BUFSIZE];

extern char VYEAR[];
extern char VDAY[];
extern char VMONTH[];
extern char VSHIFT[];

extern const char *CLASSTEXT[];
extern int AUDIT;

extern int ERRORCOUNT;
extern time_t CFSTARTTIME;
extern time_t CFINITSTARTTIME;

extern struct utsname VSYSNAME;
extern char VIPADDRESS[];
extern char VPREFIX[];

extern char VDOMAIN[CF_MAXVARSIZE];
extern PlatformContext VSYSTEMHARDCLASS;
extern char VFQNAME[];
extern char VUQNAME[];

extern int DEBUG;

extern int PARSING;

extern int VERBOSE;
extern int INFORM;

extern int DONTDO;
extern int IGNORELOCK;
extern bool MINUSF;

extern const char *VPSCOMM[];
extern const char *VPSOPTS[];
extern const char *VFSTAB[];

extern int EDITFILESIZE;
extern int VIFELAPSED;
extern int VEXPIREAFTER;

extern const char *OBS[CF_OBSERVABLES][2];

extern int FIPS_MODE;
extern char POLICY_SERVER[CF_BUFSIZE];
extern int LICENSES;
extern int AM_NOVA;
extern char EXPIRY[CF_SMALLBUF];
extern char LICENSE_COMPANY[CF_SMALLBUF];
extern int CF_TOPICS;
extern int CF_OCCUR;
extern HashMethod CF_DEFAULT_DIGEST;
extern int CF_DEFAULT_DIGEST_LEN;

extern Scope *VSCOPE;
extern Audit *AUDITPTR;
extern Audit *VAUDIT;
extern Item *DONELIST;
extern char *CBUNDLESEQUENCE_STR;
extern Item *ROTATED;

extern Rlist *CF_STCK;
extern int EDIT_MODEL;
extern int CF_STCKFRAME;
extern int AM_BACKGROUND_PROCESS;
extern int CF_PERSISTENCE;
extern int LOOKUP;
extern int BOOTSTRAP;

extern const char *CF_DATATYPES[];
extern const char *CF_AGENTTYPES[];

extern int CFA_MAXTHREADS;
extern const char *THIS_BUNDLE;
extern AgentType THIS_AGENT_TYPE;
extern int SHOWREPORTS;
extern time_t PROMISETIME;
#define CF_LOCKHORIZON ((time_t)(SECONDS_PER_WEEK * 4))
extern int LASTSEENEXPIREAFTER;
extern char *DEFAULT_COPYTYPE;
extern Item *PROCESSTABLE;

extern const char *DAY_TEXT[];
extern const char *MONTH_TEXT[];
extern const char *SHIFT_TEXT[];

#if defined(__MINGW32__)
# define FILE_SEPARATOR '\\'
# define FILE_SEPARATOR_STR "\\"
#else
# define FILE_SEPARATOR '/'
# define FILE_SEPARATOR_STR "/"
#endif

#endif
