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


/*****************************************************************************/
/* operational state                                                         */
/*****************************************************************************/

bool FIPS_MODE = false; /* GLOBAL_P */

struct utsname VSYSNAME; /* GLOBAL_E, initialized later */

int CFA_MAXTHREADS = 10; /* GLOBAL_P */
int CF_PERSISTENCE = 10; /* GLOBAL_P */

AgentType THIS_AGENT_TYPE; /* GLOBAL_C, initialized later */

/*****************************************************************************/
/* Internal data structures                                                  */
/*****************************************************************************/

long LASTSEENEXPIREAFTER = SECONDS_PER_WEEK; /* GLOBAL_P */

/*****************************************************************************/
/* Compatibility infrastructure                                              */
/*****************************************************************************/

bool DONTDO = false; /* GLOBAL_A */

/* NB! Check use before changing sizes */
char VFQNAME[CF_MAXVARSIZE] = ""; /* GLOBAL_E GLOBAL_P */
char VUQNAME[CF_MAXVARSIZE] = ""; /* GLOBAL_E */
char VDOMAIN[CF_MAXVARSIZE] = ""; /* GLOBAL_E GLOBAL_P */

/*
  Default value for copytype attribute. Loaded by cf-agent from body control
*/
const char *DEFAULT_COPYTYPE = NULL; /* GLOBAL_P */

/*
  Keys for the agent. Loaded by GAInitialize (and hence every time policy is
  reloaded).

  Used in network protocol and leaked to language.
*/
RSA *PRIVKEY = NULL, *PUBKEY = NULL; /* GLOBAL_X */

/*
  First IP address discovered by DetectEnvironment (hence reloaded every policy
  change).

  Used somewhere in cf-execd, superficially in old-style protocol handshake and
  sporadically in other situations.
*/
char VIPADDRESS[CF_MAX_IP_LEN] = ""; /* GLOBAL_E */

/*
  Edition-time constant (MD5 for community, something else for Enterprise)

  Used as a default hash everywhere (not only in network protocol)
*/
HashMethod CF_DEFAULT_DIGEST; /* GLOBAL_C, initialized later */
int CF_DEFAULT_DIGEST_LEN; /* GLOBAL_C, initialized later */

/*
  Holds the "now" time captured at the moment of policy (re)load.

  TODO: This variable should be internal to timeout.c, not exposed.
  It should only be set by SetStartTime() and read by GetStartTime().

  Utilized everywhere "now" start time is needed
*/
time_t CFSTARTTIME; /* GLOBAL_E, initialized later */

/*
  Set in cf-agent/cf-runagent (from control body).

  Used as a timeout for socket operations in network code.
*/
time_t CONNTIMEOUT = 30;        /* seconds */ /* GLOBAL_A GLOBAL_P */

/*
  Internal detail of timeout operations. Due to historical reasons
  is defined here, not in libpromises/timeout.c
 */
pid_t ALARM_PID = -1; /* GLOBAL_X */

/*
  Set in cf-agent (from control body).

  Used as a default value for maxfilesize attribute in policy
*/
int EDITFILESIZE = 100000; /* GLOBAL_P */

/*
  Set in cf-agent (from control body) and GenericAgentInitialize.

  Used as a default value for ifelapsed attribute in policy.
*/
int VIFELAPSED = 1; /* GLOBAL_P */

/*
  Set in cf-agent (from control body) and GenericAgentInitialize.

  Used as a default value for expireafter attribute in policy.
*/
int VEXPIREAFTER = 120; /* GLOBAL_P */

/*
  Set in cf-agent/cf-serverd (from control body).

  Utilized in server/client code to bind sockets.
*/
char BINDINTERFACE[CF_MAXVARSIZE]; /* GLOBAL_P */

/*
  Set in cf-*.c:CheckOpts and GenericAgentConfigParseArguments.

  Utilized in generic_agent.c for
    - cf_promises_validated filename
    - GenericAgentCheckPolicy
    - GenericAgentLoadPolicy (ReadPolicyValidatedFile)
*/
bool MINUSF = false; /* GLOBAL_A */
