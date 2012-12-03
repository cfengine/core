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

#include "cf3.defs.h"

/*****************************************************************************/
/* flags                                                                     */
/*****************************************************************************/

int SHOWREPORTS = false;
int SHOW_PARSE_TREE = false;
int USE_GCC_BRIEF_FORMAT = false;

/*****************************************************************************/
/* operational state                                                         */
/*****************************************************************************/

int VERBOSE = false;
int INFORM = false;
int PARSING = false;
int CFPARANOID = false;
int REQUIRE_COMMENTS = CF_UNDEFINED;
int LOOKUP = false;
int IGNORE_MISSING_INPUTS = false;
int IGNORE_MISSING_BUNDLES = false;
int FIPS_MODE = false;
int ALWAYS_VALIDATE = false;
bool ALLCLASSESREPORT = false;

struct utsname VSYSNAME;

int XML = false;

int CFA_MAXTHREADS = 10;
int CFA_BACKGROUND = 0;
int CFA_BACKGROUND_LIMIT = 1;
int AM_BACKGROUND_PROCESS = false;
int CF_PERSISTENCE = 10;

const char *THIS_BUNDLE = NULL;

AgentType THIS_AGENT_TYPE;
time_t PROMISETIME = 0;

int LICENSES = 0;
int AM_NOVA = false;
int AM_PHP_MODULE = false;
char EXPIRY[CF_SMALLBUF] = { 0 };
char LICENSE_COMPANY[CF_SMALLBUF] = { 0 };

// These are used to measure graph complexity in know/agent

int CSV = false;
int CF_TOPICS = 0;              // objects
int CF_OCCUR = 0;               // objects
int CF_EDGES = 0;               // links or promises between them

Rlist *MOUNTEDFSLIST = NULL;
PromiseIdent *PROMISE_ID_LIST = NULL;
Item *PROCESSTABLE = NULL;
Item *PROCESSREFRESH = NULL;
Item *ROTATED = NULL;
Item *FSTABLIST = NULL;
Item *DONELIST = NULL;

char *CBUNDLESEQUENCE_STR;

int EDIT_MODEL = false;
int CF_MOUNTALL = false;
int FSTAB_EDITS;
int BOOTSTRAP = false;

/*****************************************************************************/
/* Measurements                                                              */
/*****************************************************************************/

double METER_KEPT[meter_endmark];
double METER_REPAIRED[meter_endmark];

/*****************************************************************************/
/* Internal data structures                                                  */
/*****************************************************************************/

Scope *VSCOPE = NULL;
Rlist *VINPUTLIST = NULL;
Rlist *BODYPARTS = NULL;
Rlist *SUBBUNDLES = NULL;
Rlist *ACCESSLIST = NULL;

Rlist *SINGLE_COPY_LIST = NULL;
Rlist *AUTO_DEFINE_LIST = NULL;
Rlist *SINGLE_COPY_CACHE = NULL;
Rlist *CF_STCK = NULL;

Item *EDIT_ANCHORS = NULL;

int CF_STCKFRAME = 0;
int LASTSEENEXPIREAFTER = SECONDS_PER_WEEK;

char POLICY_SERVER[CF_BUFSIZE] = { 0 };

char WEBDRIVER[CF_MAXVARSIZE] = { 0 };
char BANNER[2 * CF_BUFSIZE] = { 0 };
char FOOTER[CF_BUFSIZE] = { 0 };
char STYLESHEET[CF_BUFSIZE] = { 0 };

/*****************************************************************************/
/* Windows version constants                                                 */
/*****************************************************************************/

unsigned int WINVER_MAJOR = 0;
unsigned int WINVER_MINOR = 0;
unsigned int WINVER_BUILD = 0;

/*****************************************************************************/
/* Compatability infrastructure                                              */
/*****************************************************************************/

double FORGETRATE = 0.7;

int IGNORELOCK = false;
int DONTDO = false;
int DEBUG = false;
int AUDIT = false;

char VFQNAME[CF_MAXVARSIZE] = { 0 };
char VUQNAME[CF_MAXVARSIZE] = { 0 };
char VDOMAIN[CF_MAXVARSIZE] = { 0 };

char VYEAR[5] = { 0 };
char VDAY[3] = { 0 };
char VMONTH[4] = { 0 };
char VSHIFT[12] = { 0 };

char PADCHAR = ' ';
char PURGE = 'n';

int ERRORCOUNT = 0;
char VPREFIX[CF_MAXVARSIZE] = { 0 };
char VINPUTFILE[CF_BUFSIZE] = { 0 };

char CONTEXTID[CF_MAXVARSIZE] = { 0 };
char CFPUBKEYFILE[CF_BUFSIZE] = { 0 };
char CFPRIVKEYFILE[CF_BUFSIZE] = { 0 };
char CFWORKDIR[CF_BUFSIZE] = { 0 };

char *DEFAULT_COPYTYPE = NULL;

RSA *PRIVKEY = NULL, *PUBKEY = NULL;
char PUBKEY_DIGEST[CF_MAXVARSIZE] = { 0 };

#if defined(HAVE_PTHREAD)

pthread_mutex_t MUTEXES[] =
{
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
    PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP,
};

pthread_mutex_t *cft_system = &MUTEXES[0];
pthread_mutex_t *cft_lock = &MUTEXES[1];
pthread_mutex_t *cft_count = &MUTEXES[2];
pthread_mutex_t *cft_output = &MUTEXES[3];
pthread_mutex_t *cft_dbhandle = &MUTEXES[4];
pthread_mutex_t *cft_policy = &MUTEXES[5];
pthread_mutex_t *cft_getaddr = &MUTEXES[6];
pthread_mutex_t *cft_report = &MUTEXES[7];
pthread_mutex_t *cft_vscope = &MUTEXES[8];
pthread_mutex_t *cft_server_keyseen = &MUTEXES[9];
pthread_mutex_t *cft_server_children = &MUTEXES[10];
#endif

char VIPADDRESS[18] = { 0 };

Item *IPADDRESSES = NULL;

/*******************************************************************/
/* Context Management                                              */
/*******************************************************************/

// see env_context.c

Rlist *GOALS = NULL;

/*******************************************************************/
/*                                                                 */
/* Checksums                                                       */
/*                                                                 */
/*******************************************************************/

enum cfhashes CF_DEFAULT_DIGEST;
int CF_DEFAULT_DIGEST_LEN;

/***********************************************************/

Audit *AUDITPTR;
Audit *VAUDIT = NULL;

char CFLOCK[CF_BUFSIZE] = { 0 };
char CFLOG[CF_BUFSIZE] = { 0 };
char CFLAST[CF_BUFSIZE] = { 0 };

time_t CFSTARTTIME;
time_t CFINITSTARTTIME;
char STR_CFENGINEPORT[16] = { 0 };

unsigned short SHORT_CFENGINEPORT;
time_t CONNTIMEOUT = 30;        /* seconds */
pid_t ALARM_PID = -1;
int EDITFILESIZE = 10000;
int VIFELAPSED = 1;
int VEXPIREAFTER = 120;
char BINDINTERFACE[CF_BUFSIZE] = { 0 };

int MINUSF = false;
int EXCLAIM = true;

mode_t DEFAULTMODE = (mode_t) 0755;

Item *VSETUIDLIST = NULL;
enum classes VSYSTEMHARDCLASS;
