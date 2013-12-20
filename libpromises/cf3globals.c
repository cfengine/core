/*
   Copyright (C) CFEngine AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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

#include <cf3.defs.h>

/*****************************************************************************/
/* flags                                                                     */
/*****************************************************************************/

int SHOWREPORTS = false;

/*****************************************************************************/
/* operational state                                                         */
/*****************************************************************************/

bool FIPS_MODE = false;

struct utsname VSYSNAME;

int CFA_MAXTHREADS = 10;
int CF_PERSISTENCE = 10;

AgentType THIS_AGENT_TYPE;

Item *PROCESSTABLE = NULL;
Item *ROTATED = NULL;

/*****************************************************************************/
/* Internal data structures                                                  */
/*****************************************************************************/

int LASTSEENEXPIREAFTER = SECONDS_PER_WEEK;

char POLICY_SERVER[CF_MAX_IP_LEN] = { 0 };

/*****************************************************************************/
/* Compatability infrastructure                                              */
/*****************************************************************************/

int IGNORELOCK = false;
bool DONTDO = false;

char VFQNAME[CF_MAXVARSIZE] = { 0 };
char VUQNAME[CF_MAXVARSIZE] = { 0 };
char VDOMAIN[CF_MAXVARSIZE] = { 0 };

char VYEAR[5] = { 0 };
char VDAY[3] = { 0 };
char VMONTH[4] = { 0 };
char VSHIFT[12] = { 0 };

char CFWORKDIR[CF_BUFSIZE] = { 0 };

/*
  Default value for copytype attribute. Loaded by cf-agent from body control
*/
char *DEFAULT_COPYTYPE = NULL;

/*
  Keys for the agent. Loaded by GAInitialize (and hence every time policy is
  reloaded).

  Used in network protocol and leaked to language.
*/
RSA *PRIVKEY = NULL, *PUBKEY = NULL;

/*
  First IP address discovered by DetectEnvironment (hence reloaded every policy
  change).

  Used somewhere in cf-execd, superficially in old-style protocol handshake and
  sporadically in other situations.
*/
char VIPADDRESS[CF_MAX_IP_LEN] = { 0 };

/*
  Edition-time constant (MD5 for community, something else for Enterprise)

  Used as a default hash everywhere (not only in network protocol)
*/
HashMethod CF_DEFAULT_DIGEST;
int CF_DEFAULT_DIGEST_LEN;

/*
  Holds the "now" time captured at the moment of policy load (and in response to
  cf-runagent command to cf-serverd?!).

  Utilized everywhere "now" start time is needed
*/
time_t CFSTARTTIME;

/*
  Set in cf-serverd (from control body)/GenericAgentInitialize (defaults)

  Used in network code
*/
int CFENGINE_PORT;

/*
  Set in cf-agent/cf-runagent (from control body).

  Used as a timeout for socket operations in network code.
*/
time_t CONNTIMEOUT = 30;        /* seconds */

/*
  Internal detail of timeout operations. Due to historical reasons
  is defined here, not in libpromises/timeout.c
 */
pid_t ALARM_PID = -1;

/*
  Set in cf-agent (from control body).

  Used as a default value for maxfilesize attribute in policy
*/
int EDITFILESIZE = 10000;

/*
  Set in cf-agent (from control body) and GenericAgentInitialize.

  Used as a default value for ifelapsed attribute in policy.
*/
int VIFELAPSED = 1;

/*
  Set in cf-agent (from control body) and GenericAgentInitialize.

  Used as a default value for expireafter attribute in policy.
*/
int VEXPIREAFTER = 120;

/*
  Set in cf-agent/cf-serverd (from control body).

  Utilized in server/client code to bind sockets.
*/
char BINDINTERFACE[CF_BUFSIZE] = { 0 };

/*
  Set in cf-*.c:CheckOpts and GenericAgentConfigParseArguments.

  Utilized in generic_agent.c for
    - cf_promises_validated filename
    - GenericAgentCheckPolicy
    - GenericAgentLoadPolicy (ReadPolicyValidatedFile)
*/
bool MINUSF = false;

/* Set in libenv/sysinfo.c::DetectEnvironment (called every time environment
   reload is performed).

   Utilized all over the place, usually to look up OS-specific command/option to
   call external utility
*/
PlatformContext VSYSTEMHARDCLASS;
