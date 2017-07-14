/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#ifndef CFENGINE_CF3_EXTERN_H
#define CFENGINE_CF3_EXTERN_H


#include <cfnet.h>                          /* CF_MAX_IP_LEN */
#include <cf3.defs.h>                       /* CF_MAXVARSIZE,CF_OBSERVABLES */


/* See variables in cf3globals.c and syntax.c */

extern pid_t ALARM_PID;
extern RSA *PRIVKEY, *PUBKEY;

extern char BINDINTERFACE[CF_MAXVARSIZE];
extern time_t CONNTIMEOUT;

extern time_t CFSTARTTIME;

extern struct utsname VSYSNAME;
extern char VIPADDRESS[CF_MAX_IP_LEN];
extern char VPREFIX[1024];

extern char VDOMAIN[CF_MAXVARSIZE];
extern char VFQNAME[];
extern char VUQNAME[];

extern bool DONTDO;
extern bool MINUSF;

extern int EDITFILESIZE;
extern int VIFELAPSED;
extern int VEXPIREAFTER;

extern const char *const OBS[CF_OBSERVABLES][2];

extern bool FIPS_MODE;
extern HashMethod CF_DEFAULT_DIGEST;
extern int CF_DEFAULT_DIGEST_LEN;

extern int CF_PERSISTENCE;

extern const char *const CF_AGENTTYPES[];

extern int CFA_MAXTHREADS;
extern AgentType THIS_AGENT_TYPE;
extern long LASTSEENEXPIREAFTER;
extern const char *DEFAULT_COPYTYPE;

extern const char *const DAY_TEXT[];
extern const char *const MONTH_TEXT[];
extern const char *const SHIFT_TEXT[];

#endif
