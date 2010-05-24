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
/* File: cf3globals.c                                                        */
/*                                                                           */
/* Created: Thu Aug  2 11:08:10 2007                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"

/*****************************************************************************/
/* flags                                                                     */
/*****************************************************************************/

int SHOWREPORTS = false;

/*****************************************************************************/
/* operational state                                                         */
/*****************************************************************************/

int VERBOSE = false;
int INFORM = false;
int PARSING = false;
int CFPARANOID = false;
int REQUIRE_COMMENTS = CF_UNDEFINED;
int LOOKUP = false;
int VIEWS = true;
int IGNORE_MISSING_INPUTS = false;
int IGNORE_MISSING_BUNDLES = false;

struct utsname VSYSNAME;

FILE *FREPORT_HTML = NULL;
FILE *FREPORT_TXT = NULL;
FILE *FKNOW = NULL;
int XML = false;

struct FnCallStatus FNCALL_STATUS;

int CFA_MAXTHREADS = 10;
int CFA_BACKGROUND = 0;
int CFA_BACKGROUND_LIMIT = 1;
int AM_BACKGROUND_PROCESS = false;
int CF_PERSISTENCE = 10;

char *THIS_BUNDLE = NULL;
char THIS_AGENT[CF_MAXVARSIZE];
enum cfagenttype THIS_AGENT_TYPE;
char SYSLOGHOST[CF_MAXVARSIZE];
unsigned short SYSLOGPORT = 514;
int FACILITY = 0;
time_t PROMISETIME;

int LICENSES = 0;
char EXPIRY[32];
int INSTALL_SKIP = false;

// These are used to measure graph complexity in know/agent

int CF_NODES = 0; // objects
int CF_EDGES = 0; // links or promises between them

struct CfPackageManager *INSTALLED_PACKAGE_LISTS = NULL;
struct CfPackageManager *PACKAGE_SCHEDULE = NULL;
struct Rlist *MOUNTEDFSLIST = NULL;
struct Rlist *SERVERLIST = NULL;
struct PromiseIdent *PROMISE_ID_LIST = NULL;
struct Item *PROCESSTABLE = NULL;
struct Item *ROTATED = NULL;
struct Item *FSTABLIST = NULL;
struct Item *ABORTBUNDLEHEAP = NULL;
struct Item *DONELIST = NULL;
struct Rlist *CBUNDLESEQUENCE = NULL;

#ifdef HAVE_LIBVIRT
virConnectPtr CFVC[cfv_none];
#endif

int EDIT_MODEL = false;
int CF_MOUNTALL = false;
int CF_SAVEFSTAB = false;
int FSTAB_EDITS;
int ABORTBUNDLE = false;
int BOOTSTRAP = false;

char HASHDB[CF_BUFSIZE];

/*****************************************************************************/
/* Measurements                                                              */
/*****************************************************************************/

double METER_KEPT[meter_endmark];
double METER_REPAIRED[meter_endmark];

double Q_MEAN;
double Q_SIGMA;
double Q_MAX;
double Q_MIN;

/*****************************************************************************/
/* Internal data structures                                                  */
/*****************************************************************************/

struct PromiseParser P;
struct Bundle *BUNDLES = NULL;
struct Body *BODIES = NULL;
struct Scope *VSCOPE = NULL;
struct Rlist *VINPUTLIST = NULL;
struct Rlist *BODYPARTS = NULL;
struct Rlist *SUBBUNDLES = NULL;
struct Rlist *ACCESSLIST = NULL;

struct Rlist *SINGLE_COPY_LIST = NULL;
struct Rlist *AUTO_DEFINE_LIST = NULL;
struct Rlist *SINGLE_COPY_CACHE = NULL;
struct Rlist *CF_STCK = NULL;

struct Item *EDIT_ANCHORS = NULL;

int CF_STCKFRAME = 0;
int LASTSEENEXPIREAFTER = CF_WEEK;
int LASTSEEN = false;

struct Topic *TOPIC_MAP = NULL;

char POLICY_SERVER[CF_BUFSIZE];

char WEBDRIVER[CF_MAXVARSIZE];
char BANNER[2*CF_BUFSIZE];
char FOOTER[CF_BUFSIZE];
char STYLESHEET[CF_BUFSIZE];
char AGGREGATION[CF_BUFSIZE];

/*****************************************************************************/
/* Windows version constants                                                 */
/*****************************************************************************/

unsigned int WINVER_MAJOR = 0;
unsigned int WINVER_MINOR = 0;
unsigned int WINVER_BUILD = 0;


/*****************************************************************************/
/* Constants                                                                 */
/*****************************************************************************/

struct SubTypeSyntax CF_NOSTYPE = {NULL,NULL,NULL};

/*********************************************************************/
/* Object variables                                                  */
/*********************************************************************/

char *DAY_TEXT[] =
   {
   "Monday",
   "Tuesday",
   "Wednesday",
   "Thursday",
   "Friday",
   "Saturday",
   "Sunday"
   };

char *MONTH_TEXT[] =
   {
   "January",
   "February",
   "March",
   "April",
   "May",
   "June",
   "July",
   "August",
   "September",
   "October",
   "November",
   "December"
   };

char *SHIFT_TEXT[] =
   {
   "Night",
   "Morning",
   "Afternoon",
   "Evening"
   };

/*****************************************************************************/

char *CF_DATATYPES[] = /* see enum cfdatatype */
   {
   "string",
   "int",
   "real",
   "slist",
   "ilist",
   "rlist",
   "(menu option)",
   "(option list)",
   "(ext body)",
   "(ext bundle)",
   "class",
   "clist",
   "irange [int,int]",
   "rrange [real,real]",
   "counter",
   "<notype>",
   };

/*****************************************************************************/

char *CF_AGENTTYPES[] = /* see enum cfagenttype */
   {
   CF_COMMONC,
   CF_AGENTC,
   CF_SERVERC,
   CF_MONITORC,
   CF_EXECC,
   CF_RUNC,
   CF_KNOWC,
   CF_REPORTC,
   CF_KEYGEN,
   "<notype>",
   };

/*****************************************************************************/
/* Compatability infrastructure                                              */
/*****************************************************************************/

double FORGETRATE = 0.7;

int IGNORELOCK = false;
int DONTDO = false;
int DEBUG = false;
int D1 = false;
int D2 = false;
int AUDIT = false;
int LOGGING = false;

char  VFQNAME[CF_MAXVARSIZE];
char  VUQNAME[CF_MAXVARSIZE];
char  VDOMAIN[CF_MAXVARSIZE];

char  VYEAR[5];
char  VDAY[3];
char  VMONTH[4];
char  VHR[3];
char  VMINUTE[3];
char  VSEC[3];
char  VSHIFT[12];
char  VLIFECYCLE[12];

char PADCHAR = ' ';
char PURGE = 'n';

int ERRORCOUNT = 0;
char VPREFIX[CF_MAXVARSIZE];
char VINPUTFILE[CF_BUFSIZE];

char CONTEXTID[32];
char CFPUBKEYFILE[CF_BUFSIZE];
char CFPRIVKEYFILE[CF_BUFSIZE];
char AVDB[CF_MAXVARSIZE];
char CFWORKDIR[CF_BUFSIZE];
char PIDFILE[CF_BUFSIZE];

char *DEFAULT_COPYTYPE = NULL;

RSA *PRIVKEY = NULL, *PUBKEY = NULL;

pthread_attr_t PTHREADDEFAULTS;

#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
pthread_mutex_t MUTEX_SYSCALL = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
pthread_mutex_t MUTEX_LOCK = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
pthread_mutex_t MUTEX_COUNT = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
pthread_mutex_t MUTEX_OUTPUT = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
pthread_mutex_t MUTEX_DBHANDLE = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
pthread_mutex_t MUTEX_POLICY = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
pthread_mutex_t MUTEX_GETADDR = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
pthread_mutex_t MUTEX_DB_LASTSEEN = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
#else
# if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
pthread_mutex_t MUTEX_SYSCALL = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t MUTEX_LOCK = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t MUTEX_COUNT = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t MUTEX_OUTPUT = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t MUTEX_DBHANDLE = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t MUTEX_POLICY = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t MUTEX_GETADDR = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t MUTEX_DB_LASTSEEN = PTHREAD_MUTEX_INITIALIZER;
# endif
#endif

unsigned short PORTNUMBER = 0;
char VIPADDRESS[18];
int  CF_TIMEOUT = 10;
int  CFSIGNATURE = 0;

char *PROTOCOL[] =
   {
   "EXEC",
   "AUTH",  /* old protocol */
   "GET",
   "OPENDIR",
   "SYNCH",
   "CLASSES",
   "MD5",
   "SMD5",
   "CAUTH",
   "SAUTH",
   "SSYNCH",
   "SGET",
   "VERSION",
   "SOPENDIR",
   "VAR",
   "SVAR",
   "CONTEXT",
   "SCONTEXT",
   NULL
   };

struct Item *IPADDRESSES = NULL;
struct Item *VHEAP = NULL;
struct Item *VNEGHEAP = NULL;
struct Item *VDELCLASSES = NULL;
struct Item *VADDCLASSES=NULL;           /* Action sequence defs  */
struct Rlist *PRIVCLASSHEAP = NULL;

int PR_KEPT = 0;
int PR_REPAIRED = 0;
int PR_NOTKEPT = 0;

double VAL_KEPT = 0;
double VAL_REPAIRED = 0;
double VAL_NOTKEPT = 0;

char FILE_SEPARATOR;
char FILE_SEPARATOR_STR[2];

/*******************************************************************/
/*                                                                 */
/* Checksums                                                       */
/*                                                                 */
/*******************************************************************/

/* These string lengths should not exceed CF_MAXDIGESTNAMELEN
   characters for packing */

char *CF_DIGEST_TYPES[10][2] =
     {
     "md5","m",
     "sha224","c",
     "sha256","C",
     "sha384","h",
     "sha512","H",
     "sha1","S",
     "sha","s",   /* Should come last, since substring */
     "best","b",
     "crypt","o",
     NULL,NULL
     };

int CF_DIGEST_SIZES[10] =
     {
     CF_MD5_LEN,
     CF_SHA224_LEN,
     CF_SHA256_LEN,
     CF_SHA384_LEN,
     CF_SHA512_LEN,
     CF_SHA1_LEN,
     CF_SHA_LEN,
     CF_BEST_LEN,
     CF_CRYPT_LEN,
     0
     };

/***********************************************************/

struct Audit *AUDITPTR;
struct Audit *VAUDIT = NULL; 
FILE *VLOGFP = NULL; 
CF_DB  *AUDITDBP = NULL;

char GRAPHDIR[CF_BUFSIZE];
char CFLOCK[CF_BUFSIZE];
char SAVELOCK[CF_BUFSIZE]; 
char CFLOG[CF_BUFSIZE];
char CFLAST[CF_BUFSIZE]; 
char LOCKDB[CF_BUFSIZE];
char LOGFILE[CF_MAXVARSIZE];

char *SIGNALS[highest_signal];
char *VSETUIDLOG = NULL;

time_t CFSTARTTIME;
time_t CFINITSTARTTIME;
dev_t ROOTDEVICE = 0;
char  STR_CFENGINEPORT[16];
unsigned short SHORT_CFENGINEPORT;
int RPCTIMEOUT = 60;          /* seconds */
pid_t ALARM_PID = -1;
int SENSIBLEFILECOUNT = 2;
int SENSIBLEFSSIZE = 1000;
int SKIPIDENTIFY = false;
int ALL_SINGLECOPY = false;
int FULLENCRYPT = false;
int EDITFILESIZE = 10000;
int EDITBINFILESIZE = 100000;
int NOHARDCLASSES=false;
int VIFELAPSED = 1;
int VEXPIREAFTER = 120;
int UNDERSCORE_CLASSES=false;
int CHECKSUMUPDATES = false;
char BINDINTERFACE[CF_BUFSIZE];
int MINUSF = false;
int EXCLAIM = true;

mode_t DEFAULTMODE = (mode_t) 0755;

char *VREPOSITORY = NULL;
char REPOSCHAR = '_';

struct Item *VDEFAULTROUTE=NULL;
struct Item *VSETUIDLIST = NULL;
struct Item *SUSPICIOUSLIST = NULL;
enum classes VSYSTEMHARDCLASS = unused1;
int NONALPHAFILES = false;
struct Item *EXTENSIONLIST = NULL;
struct Item *SPOOLDIRLIST = NULL;
struct Item *NONATTACKERLIST = NULL;
struct Item *MULTICONNLIST = NULL;
struct Item *TRUSTKEYLIST = NULL;
struct Item *DHCPLIST = NULL;
struct Item *ALLOWUSERLIST = NULL;
struct Item *SKIPVERIFY = NULL;
struct Item *ATTACKERLIST = NULL;
struct Item *ABORTHEAP = NULL;

struct Item *VREPOSLIST=NULL;

 /*******************************************************************/
 /* Anomaly                                                         */
 /*******************************************************************/

struct sock ECGSOCKS[ATTR] = /* extended to map old to new using enum*/
   {
   {"137","netbiosns",ob_netbiosns_in,ob_netbiosns_out},
   {"138","netbiosdgm",ob_netbiosdgm_in,ob_netbiosdgm_out},
   {"139","netbiosssn",ob_netbiosssn_in,ob_netbiosssn_out},
   {"194","irc",ob_irc_in,ob_irc_out},
   {"5308","cfengine",ob_cfengine_in,ob_cfengine_out},
   {"2049","nfsd",ob_nfsd_in,ob_nfsd_out},
   {"25","smtp",ob_smtp_in,ob_smtp_out},
   {"80","www",ob_www_in,ob_www_out},
   {"21","ftp",ob_ftp_in,ob_ftp_out},
   {"22","ssh",ob_ssh_in,ob_ssh_out},
   {"443","wwws",ob_wwws_in,ob_wwws_out}
   };

char *TCPNAMES[CF_NETATTR] =
   {
   "icmp",
   "udp",
   "dns",
   "tcpsyn",
   "tcpack",
   "tcpfin",
   "misc"
   };

char *OBS[CF_OBSERVABLES][2] =
    {
    "users","Users logged in",
    "rootprocs","Privileged system processes",
    "otherprocs","Non-privileged process",
    "diskfree","Free disk on / partition",
    "loadavg","% kernel load utilization",
    "netbiosns_in","netbios name lookups (in)",
    "netbiosns_out","netbios name lookups (out)",
    "netbiosdgm_in","netbios name datagrams (in)",
    "netbiosdgm_out","netbios name datagrams (out)",
    "netbiosssn_in","netbios name sessions (in)",
    "netbiosssn_out","netbios name sessions (out)",
    "irc_in","IRC connections (in)",
    "irc_out","IRC connections (out)",
    "cfengine_in","cfengine connections (in)",
    "cfengine_out","cfengine connections (out)",
    "nfsd_in","nfs connections (in)",
    "nfsd_out","nfs connections (out)",
    "smtp_in","smtp connections (in)",
    "smtp_out","smtp connections (out)",
    "www_in","www connections (in)",
    "www_out","www connections (out)",
    "ftp_in","ftp connections (in)",
    "ftp_out","ftp connections (out)",
    "ssh_in","ssh connections (in)",
    "ssh_out","ssh connections (out)",
    "wwws_in","wwws connections (in)",
    "wwws_out","wwws connections (out)",
    "icmp_in","ICMP packets (in)",
    "icmp_out","ICMP packets (out)",
    "udp_in","UDP dgrams (in)",
    "udp_out","UDP dgrams (out)",
    "dns_in","DNS requests (in)",
    "dns_out","DNS requests (out)",
    "tcpsyn_in","TCP sessions (in)",
    "tcpsyn_out","TCP sessions (out)",
    "tcpack_in","TCP acks (in)",
    "tcpack_out","TCP acks (out)",
    "tcpfin_in","TCP finish (in)",
    "tcpfin_out","TCP finish (out)",
    "tcpmisc_in","TCP misc (in)",
    "tcpmisc_out","TCP misc (out)",
    "webaccess","Webserver hits",
    "weberrors","Webserver errors",
    "syslog","New log entries (Syslog)",
    "messages","New log entries (messages)",
    "temp0","CPU Temperature 0",
    "temp1","CPU Temperature 1",
    "temp2","CPU Temperature 2",
    "temp3","CPU Temperature 3",
    "cpu","%CPU utilization (all)",
    "cpu0","%CPU utilization 0",
    "cpu1","%CPU utilization 1",
    "cpu2","%CPU utilization 2",
    "cpu3","%CPU utilization 3",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    "spare","unused",
    };

char *UNITS[CF_OBSERVABLES];
time_t DATESTAMPS[CF_OBSERVABLES];
