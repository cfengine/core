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

#ifndef CFENGINE_CF3_DEFS_H
#define CFENGINE_CF3_DEFS_H

#include "platform.h"
#include "compiler.h"

#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#include <libxml/xpathInternals.h>
#endif

/*******************************************************************/
/* Preprocessor tricks                                             */
/*******************************************************************/

/* Convert integer constant to string */
#define STRINGIFY__INTERNAL_MACRO(x) #x
#define TOSTRING(x) STRINGIFY__INTERNAL_MACRO(x)

/*******************************************************************/
/* Various defines                                                 */
/*******************************************************************/

#define CF_BUFSIZE 4096
/* max size of plaintext in one transaction, see
   net.c:SendTransaction(), leave space for encryption padding
   (assuming max 64*8 = 512-bit cipher block size)*/
#define CF_BILLION 1000000000L
#define CF_EXPANDSIZE (2*CF_BUFSIZE)
#define CF_BUFFERMARGIN 128
#define CF_BLOWFISHSIZE 16
#define CF_SMALLBUF 128
#define CF_MAXVARSIZE 1024
#define CF_MAXSIDSIZE 2048      /* Windows only: Max size (bytes) of security identifiers */
#define CF_NONCELEN (CF_BUFSIZE/16)
#define CF_MAXLINKSIZE 256
#define CF_MAX_IP_LEN 64        /* numerical ip length */
#define CF_PROCCOLS 16
#define CF_HASHTABLESIZE 8192
#define CF_MACROALPHABET 61     /* a-z, A-Z plus a bit */
#define CF_ALPHABETSIZE 256
#define CF_SAMEMODE 7777
#define CF_SAME_OWNER ((uid_t)-1)
#define CF_UNKNOWN_OWNER ((uid_t)-2)
#define CF_SAME_GROUP ((gid_t)-1)
#define CF_UNKNOWN_GROUP ((gid_t)-2)
#define CF_INFINITY ((int)999999999)
#define SOCKET_INVALID -1
#define CF_MONDAY_MORNING 345600

#define MINUTES_PER_HOUR 60
#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR (60 * SECONDS_PER_MINUTE)
#define SECONDS_PER_DAY (24 * SECONDS_PER_HOUR)
#define SECONDS_PER_WEEK (7 * SECONDS_PER_DAY)

/* Long-term monitoring constants */

#define HOURS_PER_SHIFT 6
#define SECONDS_PER_SHIFT (HOURS_PER_SHIFT * SECONDS_PER_HOUR)
#define SHIFTS_PER_DAY 4
#define SHIFTS_PER_WEEK (4*7)

#define CF_INDEX_FIELD_LEN 7
#define CF_INDEX_OFFSET  CF_INDEX_FIELD_LEN+1

#define MAXIP4CHARLEN 16
#define MAX_MONTH_NAME 9

#define MAX_DIGEST_BYTES (512 / 8)  /* SHA-512 */
#define MAX_DIGEST_HEX (MAX_DIGEST_BYTES * 2)

#define CF_EDIT_IFELAPSED 3     /* NOTE: If doing copy template then edit working copy,
                                   the edit ifelapsed must not be higher than
                                   the copy ifelapsed. This will make the working
                                   copy equal to the copied template file - not the
                                   copied + edited file. */


/*******************************************************************/

#define CF_FILECHANGE     "file_change.log"
#define CF_FILECHANGE_NEW "file_changes.log"
#define CF_PROMISE_LOG    "promise_summary.log"

#define CF_ENV_FILE      "env_data"

#define CF_SAVED ".cfsaved"
#define CF_EDITED ".cfedited"
#define CF_NEW ".cfnew"
#define CFD_TERMINATOR "---cfXen/gine/cfXen/gine---"
#define CFD_TRUE "CFD_TRUE"
#define CFD_FALSE "CFD_FALSE"
#define CF_ANYCLASS "any"
#define CF_RSA_PROTO_OFFSET 24
#define CF_PROTO_OFFSET 16
#define CF_INBAND_OFFSET 8
#define CF_SMALL_OFFSET 2

/* digest sizes */
#define CF_MD5_LEN 16
#define CF_SHA_LEN 20
#define CF_SHA1_LEN 20
#define CF_BEST_LEN 0
#define CF_CRYPT_LEN 64
#define CF_SHA224_LEN 28
#define CF_SHA256_LEN 32
#define CF_SHA384_LEN 48
#define CF_SHA512_LEN 64

#define CF_DONE 't'
#define CF_MORE 'm'
#define CF_NS ':'   // namespace character separator

/*****************************************************************************/

/* Auditing key */

#define CF_NOP      'n'
#define CF_CHG      'c'
#define CF_WARN     'w'         /* something wrong but nothing done */
#define CF_FAIL     'f'
#define CF_DENIED   'd'
#define CF_TIMEX    't'
#define CF_INTERPT  'i'
#define CF_REGULAR  'r'
#define CF_REPORT   'R'
#define CF_UNKNOWN  'u'

/*****************************************************************************/

#define CF_FAILEDSTR "BAD: Unspecified server refusal (see verbose server output)"
#define CF_CHANGEDSTR1 "BAD: File changed "     /* Split this so it cannot be recognized */
#define CF_CHANGEDSTR2 "while copying"

#define CF_START_DOMAIN "undefined.domain"

#define CF_GRAINS   64
#define ATTR        20
#define CF_NETATTR   7          /* icmp udp dns tcpsyn tcpfin tcpack */
#define CF_MEASURE_INTERVAL (5.0*60.0)
#define CF_SHIFT_INTERVAL (6*3600)

#define CF_OBSERVABLES 100

/* Output control defines */

#define CfDebug   if (DEBUG) printf

#include "statistics.h"

typedef struct
{
    time_t t;
    QPoint Q;
} Event;

typedef struct
{
    time_t last_seen;
    QPoint Q[CF_OBSERVABLES];
} Averages;

/******************************************************************/

typedef struct
{
    pid_t pid;
    time_t time;
} LockData;

/*****************************************************************************/

#ifdef __MINGW32__
# define NULLFILE "nul"
# define EXEC_SUFFIX ".exe"
#else
# define NULLFILE "/dev/null"
# define EXEC_SUFFIX ""
#endif /* !__MINGW32__ */

#define CF_WORDSIZE 8           /* Number of bytes in a word */

/*******************************************************************/

typedef enum
{
    FILE_TYPE_REGULAR,
    FILE_TYPE_LINK,
    FILE_TYPE_DIR,
    FILE_TYPE_FIFO,
    FILE_TYPE_BLOCK,
    FILE_TYPE_CHAR,
    FILE_TYPE_SOCK
} FileType;

/*******************************************************************/

typedef struct Stat_ Stat;

struct Stat_
{
    char *cf_filename;          /* What file are we statting? */
    char *cf_server;            /* Which server did this come from? */
    FileType cf_type;           /* enum filetype */
    mode_t cf_lmode;            /* Mode of link, if link */
    mode_t cf_mode;             /* Mode of remote file, not link */
    uid_t cf_uid;               /* User ID of the file's owner */
    gid_t cf_gid;               /* Group ID of the file's group */
    off_t cf_size;              /* File size in bytes */
    time_t cf_atime;            /* Time of last access */
    time_t cf_mtime;            /* Time of last data modification */
    time_t cf_ctime;            /* Time of last file status change */
    char cf_makeholes;          /* what we need to know from blksize and blks */
    char *cf_readlink;          /* link value or NULL */
    int cf_failed;              /* stat returned -1 */
    int cf_nlink;               /* Number of hard links */
    int cf_ino;                 /* inode number on server */
    dev_t cf_dev;               /* device number */
    Stat *next;
};

/*******************************************************************/

typedef struct Item_ Item;

// Indexed itemlist
typedef struct
{
    Item *list[CF_ALPHABETSIZE];
} AlphaList;

/*******************************************************************/

enum cfsizes
{
    cfabs,
    cfpercent
};

/*******************************************************************/

typedef enum
{
    CONTEXT_STATE_POLICY_RESET,                    /* Policy when trying to add already defined persistent states */
    CONTEXT_STATE_POLICY_PRESERVE
} ContextStatePolicy;

/*******************************************************************/

typedef enum
{
    PLATFORM_CONTEXT_UNKNOWN,
    PLATFORM_CONTEXT_HP,
    PLATFORM_CONTEXT_AIX,
    PLATFORM_CONTEXT_LINUX,
    PLATFORM_CONTEXT_SOLARIS,
    PLATFORM_CONTEXT_FREEBSD,
    PLATFORM_CONTEXT_NETBSD,
    PLATFORM_CONTEXT_CRAYOS,
    PLATFORM_CONTEXT_WINDOWS_NT,
    PLATFORM_CONTEXT_SYSTEMV,
    PLATFORM_CONTEXT_OPENBSD,
    PLATFORM_CONTEXT_CFSCO,
    PLATFORM_CONTEXT_DARWIN,
    PLATFORM_CONTEXT_QNX,
    PLATFORM_CONTEXT_DRAGONFLY,
    PLATFORM_CONTEXT_MINGW,
    PLATFORM_CONTEXT_VMWARE,
    PLATFORM_CONTEXT_MAX
} PlatformContext;

enum observables
{
    ob_users,
    ob_rootprocs,
    ob_otherprocs,
    ob_diskfree,
    ob_loadavg,
    ob_netbiosns_in,
    ob_netbiosns_out,
    ob_netbiosdgm_in,
    ob_netbiosdgm_out,
    ob_netbiosssn_in,
    ob_netbiosssn_out,
    ob_imap_in,
    ob_imap_out,
    ob_cfengine_in,
    ob_cfengine_out,
    ob_nfsd_in,
    ob_nfsd_out,
    ob_smtp_in,
    ob_smtp_out,
    ob_www_in,
    ob_www_out,
    ob_ftp_in,
    ob_ftp_out,
    ob_ssh_in,
    ob_ssh_out,
    ob_wwws_in,
    ob_wwws_out,
    ob_icmp_in,
    ob_icmp_out,
    ob_udp_in,
    ob_udp_out,
    ob_dns_in,
    ob_dns_out,
    ob_tcpsyn_in,
    ob_tcpsyn_out,
    ob_tcpack_in,
    ob_tcpack_out,
    ob_tcpfin_in,
    ob_tcpfin_out,
    ob_tcpmisc_in,
    ob_tcpmisc_out,
    ob_webaccess,
    ob_weberrors,
    ob_syslog,
    ob_messages,
    ob_temp0,
    ob_temp1,
    ob_temp2,
    ob_temp3,
    ob_cpuall,
    ob_cpu0,
    ob_cpu1,
    ob_cpu2,
    ob_cpu3,
    ob_microsoft_ds_in,
    ob_microsoft_ds_out,
    ob_www_alt_in,
    ob_www_alt_out,
    ob_imaps_in,
    ob_imaps_out,
    ob_ldap_in,
    ob_ldap_out,
    ob_ldaps_in,
    ob_ldaps_out,
    ob_mongo_in,
    ob_mongo_out,
    ob_mysql_in,
    ob_mysql_out,
    ob_postgresql_in,
    ob_postgresql_out,
    ob_ipp_in,
    ob_ipp_out,
    ob_spare
};

/*******************************************************************/

typedef struct
{
    int sd;
    int trust;                  /* true if key being accepted on trust */
    int authenticated;
    int protoversion;
    int family;                 /* AF_INET or AF_INET6 */
    char username[CF_SMALLBUF];
    char localip[CF_MAX_IP_LEN];
    char remoteip[CF_MAX_IP_LEN];
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    unsigned char *session_key;
    char encryption_type;
    short error;
} AgentConnection;

/*******************************************************************/

typedef struct CompressedArray_ CompressedArray;

/*******************************************************************/

typedef struct Audit_ Audit;

struct Audit_
{
    char *version;
    char *filename;
    char *date;
    unsigned char digest[EVP_MAX_MD_SIZE + 1];
    Audit *next;
};

/*******************************************************************/

typedef struct UidList_ UidList;

struct UidList_
{
#ifdef __MINGW32__  // TODO: remove uid for NT ?
    char sid[CF_MAXSIDSIZE];    /* Invalid sid indicates unset */
#endif /* __MINGW32__ */
    uid_t uid;
    char *uidname;              /* when uid is -2 */
    UidList *next;
};

/*******************************************************************/

typedef struct GidList_ GidList;

struct GidList_
{
    gid_t gid;
    char *gidname;              /* when gid is -2 */
    GidList *next;
};

/*******************************************************************/

typedef struct Auth_ Auth;

struct Auth_
{
    char *path;
    Item *accesslist;
    Item *maproot;              /* which hosts should have root read access */
    int encrypt;                /* which files HAVE to be transmitted securely */
    int literal;
    int classpattern;
    int variable;
    Auth *next;
};

/*******************************************************************/
/* Checksum database structures                                    */
/*******************************************************************/

typedef struct
{
    unsigned char mess_digest[EVP_MAX_MD_SIZE + 1];     /* Content digest */
    unsigned char attr_digest[EVP_MAX_MD_SIZE + 1];     /* Attribute digest */
} ChecksumValue;

/*******************************************************************/
/* File path manipulation primitives                               */
/*******************************************************************/

/* Defined maximum length of a filename. */

/* File node separator (cygwin can use \ or / but prefer \ for communicating
 * with native windows commands). */

#ifdef _WIN32
# define IsFileSep(c) ((c) == '\\' || (c) == '/')
#else
# define IsFileSep(c) ((c) == '/')
#endif

/*************************************************************************/
/* Fundamental (meta) types                                              */
/*************************************************************************/

#define CF_STACK  'k'

#define CF_MAPPEDLIST '#'

#define CF_UNDEFINED -1
#define CF_NODOUBLE -123.45
#define CF_NOINT    -678L
#define CF_UNDEFINED_ITEM (void *)0x1234
#define CF_VARARGS 99
#define CF_UNKNOWN_IP "location unknown"

#define DEFAULTMODE ((mode_t)0755)

#define CF_MAX_NESTING 10
#define CF_MAX_REPLACE 20
#define CF_DONEPASSES  4

#define CFPULSETIME 60

/*************************************************************************/
/* Parsing and syntax tree structures                                    */
/*************************************************************************/

#define CF_DEFINECLASSES "classes"
#define CF_TRANSACTION   "action"

extern const int CF3_MODULES;

/*************************************************************************/

typedef struct Policy_ Policy;
typedef struct Bundle_ Bundle;
typedef struct Body_ Body;
typedef struct Promise_ Promise;
typedef struct SubType_ SubType;
typedef struct FnCall_ FnCall;

/*************************************************************************/
/* Abstract datatypes                                                    */
/*************************************************************************/

typedef enum
{
    DATA_TYPE_STRING,
    DATA_TYPE_INT,
    DATA_TYPE_REAL,
    DATA_TYPE_STRING_LIST,
    DATA_TYPE_INT_LIST,
    DATA_TYPE_REAL_LIST,
    DATA_TYPE_OPTION,
    DATA_TYPE_OPTION_LIST,
    DATA_TYPE_BODY,
    DATA_TYPE_BUNDLE,
    DATA_TYPE_CONTEXT,
    DATA_TYPE_CONTEXT_LIST,
    DATA_TYPE_INT_RANGE,
    DATA_TYPE_REAL_RANGE,
    DATA_TYPE_COUNTER,
    DATA_TYPE_NONE
} DataType;

/*************************************************************************/

#define CF_COMMONC  "common"
#define CF_AGENTC   "agent"
#define CF_SERVERC  "server"
#define CF_MONITORC "monitor"
#define CF_EXECC    "executor"
#define CF_RUNC     "runagent"
#define CF_KEYGEN   "keygenerator"
#define CF_HUBC     "hub"
#define CF_GENDOC   "gendoc"

typedef enum
{
    AGENT_TYPE_COMMON,
    AGENT_TYPE_AGENT,
    AGENT_TYPE_SERVER,
    AGENT_TYPE_MONITOR,
    AGENT_TYPE_EXECUTOR,
    AGENT_TYPE_RUNAGENT,
    AGENT_TYPE_KEYGEN,
    AGENT_TYPE_HUB,
    AGENT_TYPE_GENDOC,
    AGENT_TYPE_NOAGENT
} AgentType;

/*************************************************************************/

typedef enum
{
    COMMON_CONTROL_BUNDLESEQUENCE,
    COMMON_CONTROL_GOALPATTERNS,
    COMMON_CONTROL_IGNORE_MISSING_BUNDLES,
    COMMON_CONTROL_IGNORE_MISSING_INPUTS,
    COMMON_CONTROL_INPUTS,
    COMMON_CONTROL_VERSION,
    COMMON_CONTROL_LASTSEEN_EXPIRE_AFTER,
    COMMON_CONTROL_OUTPUT_PREFIX,
    COMMON_CONTROL_DOMAIN,
    COMMON_CONTROL_REQUIRE_COMMENTS,
    COMMON_CONTROL_LICENSES,
    COMMON_CONTROL_SITE_CLASSES,
    COMMON_CONTROL_SYSLOG_HOST,
    COMMON_CONTROL_SYSLOG_PORT,
    COMMON_CONTROL_FIPS_MODE,
    COMMON_CONTROL_NONE
} CommonControl;

/*************************************************************************/

typedef enum
{
    AGENT_CONTROL_ABORTCLASSES,
    AGENT_CONTROL_ABORTBUNDLECLASSES,
    AGENT_CONTROL_ADDCLASSES,
    AGENT_CONTROL_AGENTACCESS,
    AGENT_CONTROL_AGENTFACILITY,
    AGENT_CONTROL_ALLCLASSESREPORT,
    AGENT_CONTROL_ALWAYSVALIDATE,
    AGENT_CONTROL_AUDITING,
    AGENT_CONTROL_BINARYPADDINGCHAR,
    AGENT_CONTROL_BINDTOINTERFACE,
    AGENT_CONTROL_HASHUPDATES,
    AGENT_CONTROL_CHILDLIBPATH,
    AGENT_CONTROL_CHECKSUM_ALERT_TIME,
    AGENT_CONTROL_DEFAULTCOPYTYPE,
    AGENT_CONTROL_DRYRUN,
    AGENT_CONTROL_EDITBINARYFILESIZE,
    AGENT_CONTROL_EDITFILESIZE,
    AGENT_CONTROL_ENVIRONMENT,
    AGENT_CONTROL_EXCLAMATION,
    AGENT_CONTROL_EXPIREAFTER,
    AGENT_CONTROL_FSINGLECOPY,
    AGENT_CONTROL_FAUTODEFINE,
    AGENT_CONTROL_HOSTNAMEKEYS,
    AGENT_CONTROL_IFELAPSED,
    AGENT_CONTROL_INFORM,
    AGENT_CONTROL_INTERMITTENCY,
    AGENT_CONTROL_MAX_CHILDREN,
    AGENT_CONTROL_MAXCONNECTIONS,
    AGENT_CONTROL_MOUNTFILESYSTEMS,
    AGENT_CONTROL_NONALPHANUMFILES,
    AGENT_CONTROL_REPCHAR,
    AGENT_CONTROL_REFRESH_PROCESSES,
    AGENT_CONTROL_REPOSITORY,
    AGENT_CONTROL_SECUREINPUT,
    AGENT_CONTROL_SENSIBLECOUNT,
    AGENT_CONTROL_SENSIBLESIZE,
    AGENT_CONTROL_SKIPIDENTIFY,
    AGENT_CONTROL_SUSPICIOUSNAMES,
    AGENT_CONTROL_SYSLOG,
    AGENT_CONTROL_TRACK_VALUE,
    AGENT_CONTROL_TIMEZONE,
    AGENT_CONTROL_TIMEOUT,
    AGENT_CONTROL_VERBOSE,
    AGENT_CONTROL_NONE
} AgentControl;

/*************************************************************************/

typedef enum
{
    EXEC_CONTROL_SPLAYTIME,
    EXEC_CONTROL_MAILFROM,
    EXEC_CONTROL_MAILTO,
    EXEC_CONTROL_SMTPSERVER,
    EXEC_CONTROL_MAILMAXLINES,
    EXEC_CONTROL_SCHEDULE,
    EXEC_CONTROL_EXECUTORFACILITY,
    EXEC_CONTROL_EXECCOMMAND,
    EXEC_CONTROL_AGENT_EXPIREAFTER,
    EXEC_CONTROL_NONE
} ExecControl;

typedef enum
{
    OUTPUT_LEVEL_INFORM,
    OUTPUT_LEVEL_VERBOSE,
    OUTPUT_LEVEL_ERROR,
    OUTPUT_LEVEL_LOG,
    OUTPUT_LEVEL_REPORTING,
    OUTPUT_LEVEL_CMDOUT,
    OUTPUT_LEVEL_NONE
} OutputLevel;

typedef enum
{
    EDIT_ORDER_BEFORE,
    EDIT_ORDER_AFTER
} EditOrder;

/*************************************************************************/
/* Syntax module range/pattern constants for type validation             */
/*************************************************************************/

#define CF_BUNDLE  (void*)1234  /* any non-null value, not used */

#define CF_HIGHINIT 99999L
#define CF_LOWINIT -999999L

#define CF_SIGNALRANGE "hup,int,trap,kill,pipe,cont,abrt,stop,quit,term,child,usr1,usr2,bus,segv"
#define CF_BOOL      "true,false,yes,no,on,off"
#define CF_LINKRANGE "symlink,hardlink,relative,absolute"
#define CF_TIMERANGE "0,2147483647"
#define CF_VALRANGE  "0,99999999999"
#define CF_INTRANGE  "-99999999999,9999999999"
#define CF_INTLISTRANGE  "[-0-9_$(){}\\[\\].]+"
#define CF_REALRANGE "-9.99999E100,9.99999E100"
#define CF_CHARRANGE "^.$"
#define CF_NULL_VALUE "cf_null"

#define CF_MODERANGE   "[0-7augorwxst,+-]+"
#define CF_BSDFLAGRANGE "[+-]*[(arch|archived|nodump|opaque|sappnd|sappend|schg|schange|simmutable|sunlnk|sunlink|uappnd|uappend|uchg|uchange|uimmutable|uunlnk|uunlink)]+"
#define CF_CLASSRANGE  "[a-zA-Z0-9_!&@@$|.()\\[\\]{}:]+"
#define CF_IDRANGE     "[a-zA-Z0-9_$(){}\\[\\].:]+"
#define CF_USERRANGE   "[a-zA-Z0-9_$.-]+"
#define CF_IPRANGE     "[a-zA-Z0-9_$(){}.:-]+"
#define CF_FNCALLRANGE "[a-zA-Z0-9_(){}.$@]+"
#define CF_NAKEDLRANGE "@[(][a-zA-Z0-9]+[)]"
#define CF_ANYSTRING   ".*"

#ifndef __MINGW32__
# define CF_ABSPATHRANGE   "\042?(/.*)"
#else
// can start with e.g. c:\... or "c:\...  |  unix (for Cygwin-style paths)
# define CF_ABSPATHRANGE   "\042?(([a-zA-Z]:\\\\.*)|(/.*))"
#endif

/* Any non-empty string can be an absolute path under Unix */
#define CF_PATHRANGE ".+"

#define CF_LOGRANGE    "stdout|udp_syslog|(\042?[a-zA-Z]:\\\\.*)|(/.*)"

#define CF_FACILITY "LOG_USER,LOG_DAEMON,LOG_LOCAL0,LOG_LOCAL1,LOG_LOCAL2,LOG_LOCAL3,LOG_LOCAL4,LOG_LOCAL5,LOG_LOCAL6,LOG_LOCAL7"

// Put this here now for caching efficiency

#define SOFTWARE_PACKAGES_CACHE "software_packages.csv"

/*************************************************************************/

typedef enum
{
    RVAL_TYPE_SCALAR = 's',
    RVAL_TYPE_LIST = 'l',
    RVAL_TYPE_FNCALL = 'f',
    RVAL_TYPE_ASSOC = 'a',
    RVAL_TYPE_NOPROMISEE = 'X' // TODO: must be another hack
} RvalType;

typedef struct
{
    void *item;
    RvalType type;
} Rval;

typedef struct Rlist_ Rlist;

typedef enum
{
    REPORT_OUTPUT_TYPE_TEXT,
    REPORT_OUTPUT_TYPE_KNOWLEDGE,

    REPORT_OUTPUT_TYPE_MAX
} ReportOutputType;

typedef struct ReportContext_ ReportContext;

/*************************************************************************/

typedef struct
{
    const char *lval;
    const DataType dtype;
    const void *range;          /* either char or BodySyntax * */
    const char *description;
    const char *default_value;
} BodySyntax;

/*************************************************************************/

typedef struct
{
    const char *bundle_type;
    const char *subtype;
    const BodySyntax *bs;
} SubTypeSyntax;

/*************************************************************************/

typedef struct FnCallResult_ FnCallResult;

typedef struct
{
    const char *pattern;
    DataType dtype;
    const char *description;
} FnCallArg;

typedef struct
{
    const char *name;
    DataType dtype;
    const FnCallArg *args;
              FnCallResult(*impl) (FnCall *, Rlist *);
    const char *description;
    bool varargs;
} FnCallType;

/*************************************************************************/

#define UNKNOWN_FUNCTION -1

typedef struct Constraint_ Constraint;

typedef struct
{
    char *filename;
    Item *file_start;
    Item *file_classes;
    int num_edits;
    int empty_first;
#ifdef HAVE_LIBXML2
    xmlDocPtr xmldoc;
#endif

} EditContext;

/*******************************************************************/
/* Variable processing                                             */
/*******************************************************************/

typedef struct AssocHashTable_ AssocHashTable;

/* $(bundlevar) $(scope.name) */
typedef struct Scope_
{
    char *scope;                /* Name of scope */
    AssocHashTable *hashtable;
    struct Scope_ *next;
} Scope;

/*******************************************************************/
/* Return value signalling                                         */
/*******************************************************************/

typedef enum FnCallStatus
{
    FNCALL_SUCCESS,
    FNCALL_FAILURE,
} FnCallStatus;

/* from builtin functions */
struct FnCallResult_
{
    FnCallStatus status;
    Rval rval;
};

/*******************************************************************/
/* Return value signalling                                         */
/*******************************************************************/

typedef enum
{
    INTERVAL_HOURLY,
    INTERVAL_DAILY,
    INTERVAL_NONE
} Interval;

typedef enum
{
    FILE_COMPARATOR_ATIME,
    FILE_COMPARATOR_MTIME,
    FILE_COMPARATOR_CTIME,
    FILE_COMPARATOR_CHECKSUM,
    FILE_COMPARATOR_HASH,
    FILE_COMPARATOR_BINARY,
    FILE_COMPARATOR_EXISTS,
    FILE_COMPARATOR_NONE
} FileComparator;

typedef enum
{
    FILE_LINK_TYPE_SYMLINK,
    FILE_LINK_TYPE_HARDLINK,
    FILE_LINK_TYPE_RELATIVE,
    FILE_LINK_TYPE_ABSOLUTE,
    FILE_LINK_TYPE_NONE
} FileLinkType;

enum cfopaction
{
    cfa_fix,
    cfa_warn,
};

typedef enum
{
    BACKUP_OPTION_BACKUP,
    BACKUP_OPTION_NO_BACKUP,
    BACKUP_OPTION_TIMESTAMP,
    BACKUP_OPTION_ROTATE,
    BACKUP_OPTION_REPOSITORY_STORE             /* for internal use only */
} BackupOption;

enum cftidylinks
{
    cfa_linkdelete,
    cfa_linkkeep
};

typedef enum
{
    HASH_METHOD_MD5,
    HASH_METHOD_SHA224,
    HASH_METHOD_SHA256,
    HASH_METHOD_SHA384,
    HASH_METHOD_SHA512,
    HASH_METHOD_SHA1,
    HASH_METHOD_SHA,
    HASH_METHOD_BEST,
    HASH_METHOD_CRYPT,
    HASH_METHOD_NONE
} HashMethod;

enum cfnofile
{
    cfa_force,
    cfa_delete,
    cfa_skip
};

enum cflinkchildren
{
    cfa_override,
    cfa_onlynonexisting
};

typedef enum
{
    FILE_CHANGE_REPORT_NONE,
    FILE_CHANGE_REPORT_CONTENT_CHANGE,
    FILE_CHANGE_REPORT_STATS_CHANGE,
    FILE_CHANGE_REPORT_ALL
} FileChangeReport;

enum signalnames
{
    cfa_hup,
    cfa_int,
    cfa_trap,
    cfa_kill,
    cfa_pipe,
    cfa_cont,
    cfa_abrt,
    cfa_stop,
    cfa_quit,
    cfa_term,
    cfa_child,
    cfa_usr1,
    cfa_usr2,
    cfa_bus,
    cfa_segv
};

typedef enum
{
    PACKAGE_ACTION_ADD,
    PACKAGE_ACTION_DELETE,
    PACKAGE_ACTION_REINSTALL,
    PACKAGE_ACTION_UPDATE,
    PACKAGE_ACTION_ADDUPDATE,
    PACKAGE_ACTION_PATCH,
    PACKAGE_ACTION_VERIFY,
    PACKAGE_ACTION_NONE
} PackageAction;

typedef enum
{
    PACKAGE_VERSION_COMPARATOR_EQ,
    PACKAGE_VERSION_COMPARATOR_NEQ,
    PACKAGE_VERSION_COMPARATOR_GT,
    PACKAGE_VERSION_COMPARATOR_LT,
    PACKAGE_VERSION_COMPARATOR_GE,
    PACKAGE_VERSION_COMPARATOR_LE,
    PACKAGE_VERSION_COMPARATOR_NONE
} PackageVersionComparator;

typedef enum
{
    PACKAGE_ACTION_POLICY_INDIVIDUAL,
    PACKAGE_ACTION_POLICY_BULK,
    PACKAGE_ACTION_POLICY_NONE
} PackageActionPolicy;

/*
Adding new mutex:
- add declaration here,
- define in cf3globals.c.
*/

extern pthread_mutex_t *cft_system;
extern pthread_mutex_t *cft_count;
extern pthread_mutex_t *cft_getaddr;
extern pthread_mutex_t *cft_lock;
extern pthread_mutex_t *cft_output;
extern pthread_mutex_t *cft_dbhandle;
extern pthread_mutex_t *cft_policy;
extern pthread_mutex_t *cft_report;
extern pthread_mutex_t *cft_vscope;
extern pthread_mutex_t *cft_server_keyseen;
extern pthread_mutex_t *cft_server_children;

/************************************************************************************/

typedef enum
{
    PROMISE_STATE_REPAIRED = 'r',
    PROMISE_STATE_NOTKEPT = 'n',
    PROMISE_STATE_KEPT = 'c',
    PROMISE_STATE_ANY = 'x'
} PromiseState;

/************************************************************************************/

typedef enum
{
    LAST_SEEN_DIRECTION_INCOMING = '-',
    LAST_SEEN_DIRECTION_OUTGOING = '+'
} LastSeenDirection;

/************************************************************************************/

typedef enum
{
    FILE_STATE_NEW,
    FILE_STATE_REMOVED,
    FILE_STATE_CONTENT_CHANGED,
    FILE_STATE_STATS_CHANGED
} FileState;

/************************************************************************************/

typedef enum
{
    ACL_METHOD_APPEND,
    ACL_METHOD_OVERWRITE,
    ACL_METHOD_NONE
} AclMethod;

typedef enum
{
    ACL_TYPE_GENERIC,
    ACL_TYPE_POSIX,
    ACL_TYPE_NTFS_,
    ACL_TYPE_NONE
} AclType;

typedef enum
{
    ACL_INHERITANCE_NO_CHANGE,
    ACL_INHERITANCE_SPECIFY,
    ACL_INHERITANCE_PARENT,
    ACL_INHERITANCE_CLEAR,
    ACL_INHERITANCE_NONE
} AclInheritance;

typedef struct
{
    AclMethod acl_method;
    AclType acl_type;
    AclInheritance acl_directory_inherit;
    Rlist *acl_entries;
    Rlist *acl_inherit_entries;
} Acl;

typedef enum
{
    INHERIT_ACCESS_ONLY,
    INHERIT_DEFAULT_ONLY,
    INHERIT_ACCESS_AND_DEFAULT
}
inherit_t;

typedef enum
{
    INSERT_MATCH_TYPE_IGNORE_LEADING,
    INSERT_MATCH_TYPE_IGNORE_TRAILING,
    INSERT_MATCH_TYPE_IGNORE_EMBEDDED,
    INSERT_MATCH_TYPE_EXACT
} InsertMatchType;

/*************************************************************************/
/* Runtime constraint structures                                         */
/*************************************************************************/

#define OVECCOUNT 30

/*******************************************************************/

typedef struct
{
    char *name;
    RSA *key;
    char *address;
    time_t timestamp;
} KeyBinding;

/*************************************************************************/

typedef struct
{
    char *last;
    char *lock;
    char *log;
} CfLock;

/*************************************************************************/

typedef struct
{
    char *host;
    char *source;
    char *mounton;
    char *options;
    int unmount;
} Mount;

/*************************************************************************/

typedef struct
{
    int travlinks;
    int rmdeadlinks;
    int depth;
    int xdev;
    int include_basedir;
    Rlist *include_dirs;
    Rlist *exclude_dirs;
} Recursion;

/*************************************************************************/

typedef struct
{
    enum cfopaction action;
    int ifelapsed;
    int expireafter;
    int background;
    char *log_string;
    char *log_kept;
    char *log_repaired;
    char *log_failed;
    int log_priority;
    char *measure_id;
    double value_kept;
    double value_notkept;
    double value_repaired;
    int audit;
    OutputLevel report_level;
    OutputLevel log_level;
} TransactionContext;

/*************************************************************************/

typedef struct
{
    Rlist *change;
    Rlist *failure;
    Rlist *denied;
    Rlist *timeout;
    Rlist *kept;
    Rlist *interrupt;
    int persist;
    ContextStatePolicy timer;
    Rlist *del_change;
    Rlist *del_kept;
    Rlist *del_notkept;
    Rlist *retcode_kept;
    Rlist *retcode_repaired;
    Rlist *retcode_failed;
} DefineClasses;

/*************************************************************************/
/* SQL Database connectors                                               */
/*************************************************************************/

typedef enum
{
    DATABASE_TYPE_MYSQL,
    DATABASE_TYPE_POSTGRES,
    DATABASE_TYPE_NONE
} DatabaseType;

/*************************************************************************/
/* Threading container                                                   */
/*************************************************************************/

typedef struct
{
    AgentType agent;
    char *scopeid;
    Promise *pp;
    void *fnptr;
} PromiseThread;

/*************************************************************************/
/* Package promises                                                      */
/*************************************************************************/

typedef struct PackageItem_ PackageItem;
typedef struct PackageManager_ PackageManager;

struct PackageManager_
{
    char *manager;
    PackageAction action;
    PackageActionPolicy policy;
    PackageItem *pack_list;
    PackageItem *patch_list;
    PackageItem *patch_avail;
    PackageManager *next;
};

/*************************************************************************/

struct PackageItem_
{
    char *name;
    char *version;
    char *arch;
    Promise *pp;
    PackageItem *next;
};

/*************************************************************************/
/* Files                                                                 */
/*************************************************************************/

typedef struct
{
    char *source;
    char *destination;
    FileComparator compare;
    FileLinkType link_type;
    Rlist *servers;
    Rlist *link_instead;
    Rlist *copy_links;
    BackupOption backup;
    int stealth;
    int preserve;
    int collapse;
    int check_root;
    int type_check;
    int force_update;
    int force_ipv4;
    size_t min_size;            /* Safety margin not search criterion */
    size_t max_size;
    int trustkey;
    int encrypt;
    int verify;
    int purge;
    short portnumber;
    short timeout;
} FileCopy;

/*************************************************************************/

typedef struct
{
    unsigned int expires;
    ContextStatePolicy policy;
} CfState;

/*************************************************************************/

typedef struct
{
    mode_t plus;
    mode_t minus;
    UidList *owners;
    GidList *groups;
    char *findertype;
    u_long plus_flags;          /* for *BSD chflags */
    u_long minus_flags;         /* for *BSD chflags */
    int rxdirs;
} FilePerms;

/*************************************************************************/

typedef struct
{
    Rlist *name;
    Rlist *path;
    Rlist *perms;
    Rlist *bsdflags;
    Rlist *owners;
    Rlist *groups;
    long max_size;
    long min_size;
    time_t max_ctime;
    time_t min_ctime;
    time_t max_mtime;
    time_t min_mtime;
    time_t max_atime;
    time_t min_atime;
    char *exec_regex;
    char *exec_program;
    Rlist *filetypes;
    Rlist *issymlinkto;
    char *result;
} FileSelect;

/*************************************************************************/

typedef struct
{
    enum cftidylinks dirlinks;
    int rmdirs;
} FileDelete;

/*************************************************************************/

typedef struct
{
    char *newname;
    char *disable_suffix;
    int disable;
    int rotate;
    mode_t plus;
    mode_t minus;
} FileRename;

/*************************************************************************/

typedef struct
{
    HashMethod hash;
    FileChangeReport report_changes;
    int report_diffs;
    int update;
} FileChange;

/*************************************************************************/

typedef struct
{
    char *source;
    FileLinkType link_type;
    Rlist *copy_patterns;
    enum cfnofile when_no_file;
    enum cflinkchildren when_linking_children;
    int link_children;
} FileLink;

/*************************************************************************/

typedef struct
{
    int useshell;
    mode_t umask;
    uid_t owner;
    gid_t group;
    char *chdir;
    char *chroot;
    int preview;
    int nooutput;
    int timeout;
} ExecContain;

/*************************************************************************/

typedef struct
{
    long min_range;
    long max_range;
    Rlist *in_range_define;
    Rlist *out_of_range_define;
} ProcessCount;

/*************************************************************************/

typedef struct
{
    Rlist *owner;
    long min_pid;
    long max_pid;
    long min_ppid;
    long max_ppid;
    long min_pgid;
    long max_pgid;
    long min_rsize;
    long max_rsize;
    long min_vsize;
    long max_vsize;
    time_t min_ttime;
    time_t max_ttime;
    time_t min_stime;
    time_t max_stime;
    long min_pri;
    long max_pri;
    long min_thread;
    long max_thread;
    char *status;
    char *command;
    char *tty;
    char *process_result;
} ProcessSelect;

/*************************************************************************/

typedef struct
{
    Constraint *expression;
    int nconstraints;
    int persistent;
} ContextConstraint;

/*************************************************************************/

typedef struct
{
    BackupOption backup;
    int empty_before_use;
    int maxfilesize;
    int joinlines;
    int rotate;
    int inherit;
} EditDefaults;

/*************************************************************************/

typedef struct
{
    Rlist *startwith_from_list;
    Rlist *not_startwith_from_list;
    Rlist *match_from_list;
    Rlist *not_match_from_list;
    Rlist *contains_from_list;
    Rlist *not_contains_from_list;
} LineSelect;

typedef struct
{
    char *build_xpath;
    char *select_xpath;
    char *attribute_value;
    int havebuildxpath;
    int haveselectxpath;
    int haveattributevalue;
} EditXml;

typedef struct
{
    char *line_matching;
    EditOrder before_after;
    char *first_last;
} EditLocation;

typedef struct
{
    char *select_start;
    char *select_end;
    int include_start;
    int include_end;
} EditRegion;

typedef struct
{
    char *column_separator;
    int select_column;
    char value_separator;
    char *column_value;
    char *column_operation;
    int extend_columns;
    int blanks_ok;
} EditColumn;

typedef struct
{
    char *replace_value;
    char *occurrences;
} EditReplace;

/*************************************************************************/

typedef struct
{
    char *mount_type;
    char *mount_source;
    char *mount_server;
    Rlist *mount_options;
    int editfstab;
    int unmount;
} StorageMount;

typedef struct
{
    int check_foreign;
    long freespace;
    int sensible_size;
    int sensible_count;
    int scan_arrivals;
} StorageVolume;

/*************************************************************************/

typedef struct
{
    int haveprintfile;
    int havelastseen;
    int lastseen;
    char *result;
    double intermittency;
    char *friend_pattern;
    char *filename;
    char *to_file;
    int numlines;
    Rlist *showstate;
} Report;

/*************************************************************************/

typedef struct
{
    PackageAction package_policy;
    int have_package_methods;
    char *package_version;
    Rlist *package_architectures;
    PackageVersionComparator package_select;
    PackageActionPolicy package_changes;
    Rlist *package_file_repositories;

    char *package_default_arch_command;

    char *package_list_command;
    char *package_list_version_regex;
    char *package_list_name_regex;
    char *package_list_arch_regex;
    char *package_patch_list_command;

    char *package_patch_version_regex;
    char *package_patch_name_regex;
    char *package_patch_arch_regex;
    char *package_patch_installed_regex;

    char *package_list_update_command;
    int package_list_update_ifelapsed;

    char *package_version_regex;
    char *package_name_regex;
    char *package_arch_regex;
    char *package_installed_regex;

    char *package_add_command;
    char *package_delete_command;
    char *package_update_command;
    char *package_patch_command;
    char *package_verify_command;
    char *package_noverify_regex;
    char *package_name_convention;
    char *package_delete_convention;

    bool package_commands_useshell;

    char *package_multiline_start;

    char *package_version_less_command;
    char *package_version_equal_command;

    int package_noverify_returncode;
} Packages;

/*************************************************************************/

typedef enum
{
    MEASURE_POLICY_AVERAGE,
    MEASURE_POLICY_SUM,
    MEASURE_POLICY_FIRST,
    MEASURE_POLICY_LAST,
    MEASURE_POLICY_NONE
} MeasurePolicy;

typedef struct
{
    char *stream_type;
    DataType data_type;
    MeasurePolicy policy;
    char *history_type;
    char *select_line_matching;
    int select_line_number;
    char *extraction_regex;
    char *units;
    int growing;
} Measurement;

/*************************************************************************/

typedef struct
{
    char *ipv4_address;
    char *ipv4_netmask;
} TcpIp;

/*************************************************************************/

typedef struct
{
    char *db_server_owner;
    char *db_server_password;
    char *db_server_host;
    char *db_connect_db;
    DatabaseType db_server_type;
    char *server;
    char *type;
    char *operation;
    Rlist *columns;
    Rlist *rows;
    Rlist *exclude;
} Database;

/*************************************************************************/

typedef enum
{
    SERVICE_POLICY_START,
    SERVICE_POLICY_STOP,
    SERVICE_POLICY_DISABLE,
    SERVICE_POLICY_RESTART,
    SERVICE_POLICY_RELOAD,
    SERVICE_POLICY_NONE
} ServicePolicy;

typedef struct
{
    Rlist *service_depend;
    char *service_type;
    char *service_args;
    ServicePolicy service_policy;
    char *service_autostart_policy;
    char *service_depend_chain;
    FnCall *service_method;
} Services;

/*************************************************************************/

typedef struct
{
    char *level;
    char *promiser_type;
} Outputs;

/*************************************************************************/

typedef enum
{
    ENVIRONMENT_STATE_CREATE,
    ENVIRONMENT_STATE_DELETE,
    ENVIRONMENT_STATE_RUNNING,
    ENVIRONMENT_STATE_SUSPENDED,
    ENVIRONMENT_STATE_DOWN,
    ENVIRONMENT_STATE_NONE
} EnvironmentState;

typedef struct
{
    int cpus;
    int memory;
    int disk;
    char *baseline;
    char *spec;
    Rlist *addresses;
    char *name;
    char *host;
    char *type;
    EnvironmentState state;
} Environments;

/* This is huge, but the simplification of logic is huge too
    so we leave it to the compiler to optimize */

typedef struct
{
    Outputs output;
    FileSelect select;
    FilePerms perms;
    FileCopy copy;
    FileDelete delete;
    FileRename rename;
    FileChange change;
    FileLink link;
    EditDefaults edits;
    Packages packages;
    ContextConstraint context;
    Measurement measure;
    Acl acl;
    Database database;
    Services service;
    Environments env;
    char *transformer;
    char *pathtype;
    char *repository;
    char *template;
    int touch;
    int create;
    int move_obstructions;
    int inherit;

    Recursion recursion;
    TransactionContext transaction;
    DefineClasses classes;

    ExecContain contain;
    char *args;
    int module;

    Rlist *signals;
    char *process_stop;
    char *restart_class;
    ProcessCount process_count;
    ProcessSelect process_select;

    Report report;
    StorageMount mount;
    StorageVolume volume;

    TcpIp tcpip;
    int havedepthsearch;
    int haveselect;
    int haverename;
    int havedelete;
    int haveperms;
    int havechange;
    int havecopy;
    int havelink;
    int haveeditline;
    int haveeditxml;
    int haveedit;
    int havecontain;
    int haveclasses;
    int havetrans;
    int haveprocess_count;
    int havemount;
    int havevolume;
    int havebundle;
    int havetcpip;
    int havepackages;

    /* editline */

    EditRegion region;
    EditLocation location;
    EditColumn column;
    EditReplace replace;
    EditXml xml;
    int haveregion;
    int havelocation;
    int havecolumn;
    int havereplace;
    int haveinsertselect;
    int havedeleteselect;
    LineSelect line_select;
    char *sourcetype;
    int expandvars;
    int not_matching;
    Rlist *insert_match;
} Attributes;

/*************************************************************************/
/* common macros                                                         */
/*************************************************************************/

#define NULL_OR_EMPTY(str) ((str == NULL) || (str[0] == '\0'))
#define BEGINSWITH(str,start) (strncmp(str,start,strlen(start)) == 0)

#include "dbm_api.h"
#include "prototypes3.h"
#include "alloc.h"
#include "cf3.extern.h"

extern const BodySyntax CF_COMMON_BODIES[];
extern const BodySyntax CF_VARBODY[];
extern const SubTypeSyntax *CF_ALL_SUBTYPES[];
extern const BodySyntax CFG_CONTROLBODY[];
extern const FnCallType CF_FNCALL_TYPES[];
extern const SubTypeSyntax CF_ALL_BODIES[];
extern const BodySyntax CFH_CONTROLBODY[];
extern const SubTypeSyntax CF_COMMON_SUBTYPES[];
extern const BodySyntax CF_CLASSBODY[];
extern const BodySyntax CFA_CONTROLBODY[];
extern const BodySyntax CFEX_CONTROLBODY[];

#endif

