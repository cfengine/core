/*
   Copyright 2018 Northern.tech AS

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

#ifndef CFENGINE_CF3_DEFS_H
#define CFENGINE_CF3_DEFS_H

/* ALWAYS INCLUDE EITHER THIS FILE OR platform.h FIRST */


#include <platform.h>

#include <compiler.h>
#include <hash_method.h>                                      /* HashMethod */
#include <sequence.h>
#include <logging.h>

#include <definitions.h>                 /* CF_MAXVARSIZE, CF_BUFSIZE etc   */
#include <cfnet.h>                       /* ProtocolVersion, etc */
#include <misc_lib.h>                    /* xsnprintf, ProgrammingError etc */

/*******************************************************************/
/* Undef platform specific defines that pollute our namespace      */
/*******************************************************************/

#ifdef interface
#undef interface
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

#define CF_MAXFRAGMENT 19       /* abbreviate long promise names to 2*MAXFRAGMENT+3 */
#define CF_NONCELEN (CF_BUFSIZE/16)
#define CF_MAXLINKSIZE 256
#define CF_PROCCOLS 16
#define CF_MACROALPHABET 61     /* a-z, A-Z plus a bit */
#define CF_ALPHABETSIZE 256
#define CF_SAMEMODE 7777
/* CF_SAME_OWNER/GROUP should be -1; chown(-1) doesn't change ownership. */
#define CF_SAME_OWNER ((uid_t)-1)
#define CF_UNKNOWN_OWNER ((uid_t)-2)
#define CF_SAME_GROUP ((gid_t)-1)
#define CF_UNKNOWN_GROUP ((gid_t)-2)
#define CF_INFINITY ((int)999999999)
#define CF_MONDAY_MORNING 345600

#define MINUTES_PER_HOUR 60
#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR (60 * SECONDS_PER_MINUTE)
#define SECONDS_PER_DAY (24 * SECONDS_PER_HOUR)
#define SECONDS_PER_WEEK (7 * SECONDS_PER_DAY)
#define SECONDS_PER_YEAR (365 * SECONDS_PER_DAY)

/* Long-term monitoring constants */

#define HOURS_PER_SHIFT 6
#define SECONDS_PER_SHIFT (HOURS_PER_SHIFT * SECONDS_PER_HOUR)
#define SHIFTS_PER_DAY 4
#define SHIFTS_PER_WEEK (4*7)

#define MAX_MONTH_NAME 9

#define MAX_DIGEST_BYTES (512 / 8)  /* SHA-512 */
#define MAX_DIGEST_HEX (MAX_DIGEST_BYTES * 2)


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
#define CF_SMALL_OFFSET 2

#define CF_NS ':'   // namespace character separator

/* Mangled namespace and scope characters, in order for the iteration engine
 * to VariablePut() to THIS scope single elements of namespaced iterables
 * (slists/containers). See expand.c and iteration.c. */
#define CF_MANGLED_NS    '*'
#define CF_MANGLED_SCOPE '#'

/*****************************************************************************/

/* Auditing key */

typedef enum
{
    PROMISE_RESULT_SKIPPED,
    PROMISE_RESULT_NOOP = 'n',
    PROMISE_RESULT_CHANGE = 'c',
    PROMISE_RESULT_WARN = 'w', // something wrong but nothing done
    PROMISE_RESULT_FAIL = 'f',
    PROMISE_RESULT_DENIED = 'd',
    PROMISE_RESULT_TIMEOUT = 't',
    PROMISE_RESULT_INTERRUPTED = 'i'
} PromiseResult;

/*****************************************************************************/

#define CF_FAILEDSTR "BAD: Unspecified server refusal (see verbose server output)"
#define CF_CHANGEDSTR1 "BAD: File changed "     /* Split this so it cannot be recognized */
#define CF_CHANGEDSTR2 "while copying"

#define CF_START_DOMAIN "undefined.domain"

#define CF_GRAINS   64
#define CF_NETATTR   7          /* icmp udp dns tcpsyn tcpfin tcpack */
#define CF_MEASURE_INTERVAL (5.0*60.0)
#define CF_SHIFT_INTERVAL (6*3600)

#define CF_OBSERVABLES 100


typedef struct
{
    char *name;
    char *description;
    char *units;
    double expected_minimum;
    double expected_maximum;
    bool consolidable;
} MonitoringSlot;

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

#include <statistics.h>                                         /* QPoint */

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

// Please ensure the padding in this struct is initialized to 0
// LockData lock = { 0 }; // Will zero padding as well as members
// lock.pid = [...]
typedef struct
{
    pid_t pid;                  // 4 bytes
                                // 4 bytes padding
    time_t time;                // 8 bytes
    time_t process_start_time;  // 8 bytes
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

typedef struct Item_ Item;

/*******************************************************************/

typedef enum
{
    CF_SIZE_ABS,
    CF_SIZE_PERCENT
} CfSize;

/*******************************************************************/

typedef enum
{
    CONTEXT_STATE_POLICY_RESET,                    /* Policy when trying to add already defined persistent states */
    CONTEXT_STATE_POLICY_PRESERVE
} PersistentClassPolicy;

/*******************************************************************/

typedef struct UidList_ UidList;

// TODO: why do UIDs have their own list type? Clean up.
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

// TODO: why do UIDs have their own list type? Clean up.
struct GidList_
{
    gid_t gid;
    char *gidname;              /* when gid is -2 */
    GidList *next;
};

/*************************************************************************/
/* Fundamental (meta) types                                              */
/*************************************************************************/

#define CF_UNDEFINED -1
#define CF_NOINT    -678L
#define CF_UNDEFINED_ITEM (void *)0x1234

#define DEFAULTMODE ((mode_t)0755)

#define CF_DONEPASSES  4

#define CFPULSETIME 60

/*************************************************************************/
/* Parsing and syntax tree structures                                    */
/*************************************************************************/

extern const int CF3_MODULES;

/*************************************************************************/

typedef struct Policy_ Policy;
typedef struct Bundle_ Bundle;
typedef struct Body_ Body;
typedef struct Promise_ Promise;
typedef struct PromiseType_ PromiseType;
typedef struct FnCall_ FnCall;

/*************************************************************************/
/* Abstract datatypes                                                    */
/*************************************************************************/

typedef enum
{
    CF_DATA_TYPE_STRING,
    CF_DATA_TYPE_INT,
    CF_DATA_TYPE_REAL,
    CF_DATA_TYPE_STRING_LIST,
    CF_DATA_TYPE_INT_LIST,
    CF_DATA_TYPE_REAL_LIST,
    CF_DATA_TYPE_OPTION,
    CF_DATA_TYPE_OPTION_LIST,
    CF_DATA_TYPE_BODY,
    CF_DATA_TYPE_BUNDLE,
    CF_DATA_TYPE_CONTEXT,
    CF_DATA_TYPE_CONTEXT_LIST,
    CF_DATA_TYPE_INT_RANGE,
    CF_DATA_TYPE_REAL_RANGE,
    CF_DATA_TYPE_COUNTER,
    CF_DATA_TYPE_CONTAINER,
    CF_DATA_TYPE_NONE
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
    COMMON_CONTROL_BWLIMIT,
    COMMON_CONTROL_CACHE_SYSTEM_FUNCTIONS,
    COMMON_CONTROL_PROTOCOL_VERSION,
    COMMON_CONTROL_TLS_CIPHERS,
    COMMON_CONTROL_TLS_MIN_VERSION,
    COMMON_CONTROL_PACKAGE_INVENTORY,
    COMMON_CONTROL_PACKAGE_MODULE,
    COMMON_CONTROL_MAX
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
    AGENT_CONTROL_REPORTCLASSLOG,
    AGENT_CONTROL_SELECT_END_MATCH_EOF,
    AGENT_CONTROL_NONE
} AgentControl;

/*************************************************************************/

typedef enum
{
    EXEC_CONTROL_SPLAYTIME,
    EXEC_CONTROL_MAILFROM,
    EXEC_CONTROL_MAILTO,
    EXEC_CONTROL_MAILSUBJECT,
    EXEC_CONTROL_SMTPSERVER,
    EXEC_CONTROL_MAILMAXLINES,
    EXEC_CONTROL_MAILFILTER_INCLUDE,
    EXEC_CONTROL_MAILFILTER_EXCLUDE,
    EXEC_CONTROL_SCHEDULE,
    EXEC_CONTROL_EXECUTORFACILITY,
    EXEC_CONTROL_EXECCOMMAND,
    EXEC_CONTROL_AGENT_EXPIREAFTER,
    EXEC_CONTROL_NONE
} ExecControl;

typedef enum
{
    EDIT_ORDER_BEFORE,
    EDIT_ORDER_AFTER
} EditOrder;

/*************************************************************************/

typedef enum
{
    TYPE_SEQUENCE_META,
    TYPE_SEQUENCE_VARS,
    TYPE_SEQUENCE_DEFAULTS,
    TYPE_SEQUENCE_CONTEXTS,
    TYPE_SEQUENCE_USERS,
    TYPE_SEQUENCE_FILES,
    TYPE_SEQUENCE_PACKAGES,
    TYPE_SEQUENCE_ENVIRONMENTS,
    TYPE_SEQUENCE_METHODS,
    TYPE_SEQUENCE_PROCESSES,
    TYPE_SEQUENCE_SERVICES,
    TYPE_SEQUENCE_COMMANDS,
    TYPE_SEQUENCE_STORAGE,
    TYPE_SEQUENCE_DATABASES,
    TYPE_SEQUENCE_REPORTS,
    TYPE_SEQUENCE_NONE
} TypeSequence;

/*************************************************************************/
/* Syntax module range/pattern constants for type validation             */
/*************************************************************************/

#define CF_BUNDLE  (void*)1234  /* any non-null value, not used */

/* Initial values for max=low, min=high when determining a range, for
 * which we'll revise max upwards and in downwards if given meaningful
 * bounds for them.  Quite why they're these values, rather than 9999
 * (longest string of 9s to fit in an int16) or 999999999 (similar for
 * int32) remains a mystery. */
#define CF_HIGHINIT 999999L
#define CF_LOWINIT -999999L

#define CF_SIGNALRANGE "hup,int,trap,kill,pipe,cont,abrt,stop,quit,term,child,usr1,usr2,bus,segv"
#define CF_BOOL      "true,false,yes,no,on,off"
#define CF_LINKRANGE "symlink,hardlink,relative,absolute"
#define CF_TIMERANGE "0,2147483647" /* i.e. "0,0x7fffffff" */

/* Syntax checker accepts absurdly big numbers for backwards
 * compatibility. WARNING: internally they are stored as longs, possibly
 * being truncated to LONG_MAX within IntFromString(). */
#define CF_VALRANGE            "0,99999999999"
#define CF_INTRANGE  "-99999999999,99999999999"

#define CF_INTLISTRANGE  "[-0-9_$(){}\\[\\].]+"
#define CF_REALRANGE "-9.99999E100,9.99999E100"
#define CF_CHARRANGE "^.$"

#define CF_MODERANGE   "[0-7augorwxst,+-=]+"
#define CF_BSDFLAGRANGE "[+-]*[(arch|archived|nodump|opaque|sappnd|sappend|schg|schange|simmutable|sunlnk|sunlink|uappnd|uappend|uchg|uchange|uimmutable|uunlnk|uunlink)]+"
#define CF_CLASSRANGE  "[a-zA-Z0-9_!&@@$|.()\\[\\]{}:]+"
#define CF_IDRANGE     "[a-zA-Z0-9_$(){}\\[\\].:]+"
#define CF_USERRANGE   "[a-zA-Z0-9_$.-]+"
#define CF_IPRANGE     "[a-zA-Z0-9_$(){}.:-]+"
#define CF_FNCALLRANGE "[a-zA-Z0-9_(){}.$@]+"
#define CF_NAKEDLRANGE "@[(][a-zA-Z0-9_$(){}\\[\\].:]+[)]"
#define CF_ANYSTRING   ".*"

#define CF_KEYSTRING   "^(SHA|MD5)=[0123456789abcdef]*$"


#ifndef __MINGW32__
# define CF_ABSPATHRANGE   "\"?(/.*)"
#else
// can start with e.g. c:\... or "c:\...  |  unix (for Cygwin-style paths)
# define CF_ABSPATHRANGE   "\"?(([a-zA-Z]:\\\\.*)|(/.*))"
#endif

/* Any non-empty string can be an absolute path under Unix */
#define CF_PATHRANGE ".+"

// Put this here now for caching efficiency

#define SOFTWARE_PACKAGES_CACHE "software_packages.csv"
#define SOFTWARE_PATCHES_CACHE "software_patches_avail.csv"

#define PACKAGES_CONTEXT "cf_pack_context"
#define PACKAGES_CONTEXT_ANYVER "cf_pack_context_anyver"

/*************************************************************************/

typedef struct EvalContext_ EvalContext;

typedef enum
{
    RVAL_TYPE_SCALAR = 's',
    RVAL_TYPE_LIST = 'l',
    RVAL_TYPE_FNCALL = 'f',
    RVAL_TYPE_CONTAINER = 'c',
    RVAL_TYPE_NOPROMISEE = 'X' // TODO: must be another hack
} RvalType;

typedef struct
{
    void *item;
    RvalType type;
} Rval;

typedef struct Rlist_ Rlist;

typedef struct ConstraintSyntax_ ConstraintSyntax;
typedef struct BodySyntax_ BodySyntax;

/*
 * Promise types or bodies may optionally provide parse-tree check function, called after
 * parsing to do a preliminary syntax/semantic checking of unexpanded promises.
 *
 * This check function should populate #errors sequence with errors it finds and
 * return false in case it has found at least one error.
 *
 * If the check function has not found any errors, it should return true.
 */
typedef bool (*PromiseCheckFn)(const Promise *pp, Seq *errors);
typedef bool (*BodyCheckFn)(const Body *body, Seq *errors);

typedef enum
{
    SYNTAX_STATUS_NORMAL,
    SYNTAX_STATUS_DEPRECATED,
    SYNTAX_STATUS_REMOVED
} SyntaxStatus;

typedef enum
{
    FNCALL_CATEGORY_SYSTEM,
    FNCALL_CATEGORY_FILES,
    FNCALL_CATEGORY_IO,
    FNCALL_CATEGORY_COMM,
    FNCALL_CATEGORY_DATA,
    FNCALL_CATEGORY_UTILS,
    FNCALL_CATEGORY_INTERNAL
} FnCallCategory;

struct ConstraintSyntax_
{
    const char *lval;
    const DataType dtype;
    union
    {
        const char *validation_string;
        const BodySyntax *body_type_syntax;
    } range;
    const char *description;
    SyntaxStatus status;
};

struct BodySyntax_
{
    const char *body_type;
    const ConstraintSyntax *constraints;
    BodyCheckFn check_body;
    SyntaxStatus status;
};

typedef struct
{
    const char *bundle_type;
    const char *promise_type;
    const ConstraintSyntax *constraints;
    const PromiseCheckFn check_promise;
    SyntaxStatus status;
} PromiseTypeSyntax;

/*************************************************************************/

typedef struct Constraint_ Constraint;

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
    NEW_PACKAGE_ACTION_ABSENT,
    NEW_PACKAGE_ACTION_PRESENT,
    NEW_PACKAGE_ACTION_NONE
} NewPackageAction;

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
    ACL_DEFAULT_NO_CHANGE,
    ACL_DEFAULT_SPECIFY,
    ACL_DEFAULT_ACCESS,
    ACL_DEFAULT_CLEAR,
    ACL_DEFAULT_NONE
} AclDefault;

typedef enum
{
    ACL_INHERIT_FALSE,
    ACL_INHERIT_TRUE,
    ACL_INHERIT_NOCHANGE
} AclInherit;

typedef struct
{
    AclMethod acl_method;
    AclType acl_type;
    AclDefault acl_default;
    Rlist *acl_entries;
    Rlist *acl_default_entries;
    /* Only used on Windows */
    AclInherit acl_inherit;
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
    char *last;
    char *lock;
    bool is_dummy;
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
} DirectoryRecursion;

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
    int audit;
    LogLevel report_level;
    LogLevel log_level;
} TransactionContext;

/*************************************************************************/

typedef enum
{
    CONTEXT_SCOPE_NAMESPACE,
    CONTEXT_SCOPE_BUNDLE,
    CONTEXT_SCOPE_NONE
} ContextScope;

typedef struct
{
    ContextScope scope;
    Rlist *change;
    Rlist *failure;
    Rlist *denied;
    Rlist *timeout;
    Rlist *kept;
    int persist;
    PersistentClassPolicy timer;
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


typedef struct
{
    unsigned int expires;
    PersistentClassPolicy policy;
    char tags[]; // variable length, must be zero terminated
} PersistentClassInfo;

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
    enum
    {
        TIDY_LINK_DELETE,
        TIDY_LINK_KEEP
    } dirlinks;
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

typedef enum
{
    SHELL_TYPE_NONE,
    SHELL_TYPE_USE,
    SHELL_TYPE_POWERSHELL
} ShellType;

typedef struct
{
    ShellType shelltype;
    mode_t umask;
    uid_t owner;
    gid_t group;
    char *chdir;
    char *chroot;
    int preview;
    bool nooutput;
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
    ContextScope scope;
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
    bool select_end_match_eof;
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

    bool has_package_method;
    bool is_empty;
} Packages;

/*************************************************************************/

typedef struct
{
    char *name;
    int updates_ifelapsed;
    int installed_ifelapsed;
    Rlist *options;
    char *interpreter;
    char *module_path;
} PackageModuleBody;


typedef struct
{
    Rlist *control_package_inventory; /* list of all inventory used package managers
                                       * names taken from common control */
    char *control_package_module;    /* policy default package manager name */
    Seq *package_modules_bodies; /* list of all discovered in policy PackageManagerBody
                                   * bodies taken from common control */
} PackagePromiseContext;


typedef struct
{
    NewPackageAction package_policy;
    PackageModuleBody *module_body;
    Rlist *package_inventory;
    char *package_version;
    char *package_architecture;
    Rlist *package_options;

    bool is_empty;
} NewPackages;

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
    USER_STATE_PRESENT,
    USER_STATE_ABSENT,
    USER_STATE_LOCKED,
    USER_STATE_NONE
} UserState;

typedef enum
{
    PASSWORD_FORMAT_PLAINTEXT,
    PASSWORD_FORMAT_HASH,
    PASSWORD_FORMAT_NONE
} PasswordFormat;

typedef struct
{
    UserState policy;
    char *uid;
    PasswordFormat password_format;
    char *password;
    char *description;
    char *group_primary;
    Rlist *groups_secondary;
    bool   groups_secondary_given;
    char *home_dir;
    char *shell;
} User;

/*************************************************************************/

typedef struct
{
    Rlist *service_depend;
    char *service_type;
    char *service_args;
    char *service_policy;
    char *service_autostart_policy;
    char *service_depend_chain;
    FnCall *service_method;
} Services;

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

#include <json.h>

typedef struct
{
    const char *source;
    const char *port;                               /* port or service name */
    char *destination;
    FileComparator compare;
    FileLinkType link_type;
    Rlist *servers;
    Rlist *link_instead;
    Rlist *copy_links;
    BackupOption backup;
    int stealth;
    int preserve;
    int collapse;                               /* collapse_destination_dir */
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
    short timeout;
    ProtocolVersion protocol_version;
    bool missing_ok;
} FileCopy;

typedef struct
{
    FileSelect select;
    FilePerms perms;
    FileCopy copy;
    FileDelete delete;
    FileRename rename;
    FileChange change;
    FileLink link;
    EditDefaults edits;
    Packages packages;
    NewPackages new_packages;
    ContextConstraint context;
    Measurement measure;
    Acl acl;
    Database database;
    Services service;
    User users;
    Environments env;
    char *transformer;
    char *pathtype;
    char *file_type;
    char *repository;
    char *edit_template;
    char *edit_template_string;
    char *template_method;
    JsonElement *template_data;
    int touch;
    int create;
    int move_obstructions;
    int inherit;

    DirectoryRecursion recursion;
    TransactionContext transaction;
    DefineClasses classes;

    ExecContain contain;
    char *args;
    Rlist *arglist;
    int module;

    Rlist *signals;
    char *process_stop;
    char *restart_class;
    ProcessCount process_count;
    ProcessSelect process_select;

    Report report;
    StorageMount mount;
    StorageVolume volume;

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

#include <rlist.h>
#include <dbm_api.h>
#include <sequence.h>
#include <prototypes3.h>
#include <alloc.h>
#include <cf3.extern.h>

extern const ConstraintSyntax CF_COMMON_BODIES[];
extern const ConstraintSyntax CF_VARBODY[];
extern const PromiseTypeSyntax *const CF_ALL_PROMISE_TYPES[];
extern const ConstraintSyntax CFG_CONTROLBODY[];
extern const BodySyntax CONTROL_BODIES[];
extern const ConstraintSyntax CFH_CONTROLBODY[];
extern const PromiseTypeSyntax CF_COMMON_PROMISE_TYPES[];
extern const ConstraintSyntax CF_CLASSBODY[];
extern const ConstraintSyntax CFA_CONTROLBODY[];
extern const ConstraintSyntax CFEX_CONTROLBODY[];

typedef struct ServerConnectionState_ ServerConnectionState;

#endif
