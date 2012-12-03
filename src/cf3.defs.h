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
#include "rlist.h"
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

#ifdef MINGW
# define NULLFILE "nul"
# define EXEC_SUFFIX ".exe"
#else
# define NULLFILE "/dev/null"
# define EXEC_SUFFIX ""
#endif /* NOT MINGW */

#define CF_AUDIT_COMMENT 128
#define CF_AUDIT_VERSION 64
#define CF_AUDIT_DATE    32

/* key includes operation and date */
typedef struct
{
    char operator[CF_AUDIT_COMMENT];
    char comment[CF_AUDIT_COMMENT];
    char filename[CF_AUDIT_COMMENT];
    char bundle[CF_AUDIT_VERSION];      /* not used in cf2 */
    char version[CF_AUDIT_VERSION];
    char date[CF_AUDIT_DATE];
    short line_number;
    char status;
} AuditLog;

/*******************************************************************/
/* Client server defines                                           */
/*******************************************************************/

enum PROTOS
{
    cfd_exec,
    cfd_auth,
    cfd_get,
    cfd_opendir,
    cfd_synch,
    cfd_classes,
    cfd_md5,
    cfd_smd5,
    cfd_cauth,
    cfd_sauth,
    cfd_ssynch,
    cfd_sget,
    cfd_version,
    cfd_sopendir,
    cfd_var,
    cfd_svar,
    cfd_context,
    cfd_scontext,
    cfd_squery,
    cfd_call_me_back,
    cfd_bad
};

#define CF_WORDSIZE 8           /* Number of bytes in a word */

/*******************************************************************/

enum cf_filetype
{
    cf_reg,
    cf_link,
    cf_dir,
    cf_fifo,
    cf_block,
    cf_char,
    cf_sock
};

/*******************************************************************/

enum roles
{
    cf_connect,
    cf_accept
};

/*******************************************************************/

typedef struct Stat_ Stat;

struct Stat_
{
    char *cf_filename;          /* What file are we statting? */
    char *cf_server;            /* Which server did this come from? */
    enum cf_filetype cf_type;   /* enum filetype */
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

enum statepolicy
{
    cfreset,                    /* Policy when trying to add already defined persistent states */
    cfpreserve
};

/*******************************************************************/

enum classes
{
    hard_class_unknown,
    hp,
    aix,
    linuxx,
    solaris,
    freebsd,
    netbsd,
    crayos,
    cfnt,
    unix_sv,
    openbsd,
    cfsco,
    darwin,
    qnx,
    dragonfly,
    mingw,
    vmware,
    HARD_CLASSES_MAX,
};

/*******************************************************************/

enum iptypes
{
    icmp,
    udp,
    dns,
    tcpsyn,
    tcpack,
    tcpfin,
    tcpmisc
};

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

typedef struct
{
    char *portnr;
    char *name;
    enum observables in;
    enum observables out;
} Sock;

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

struct CompressedArray_
{
    int key;
    char *value;
    CompressedArray *next;
};

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
/* Action /promise types                                           */
/*******************************************************************/

struct Item_
{
    char done;
    char *name;
    char *classes;
    int counter;
    time_t time;
    Item *next;
};

/*******************************************************************/

typedef struct UidList_ UidList;

struct UidList_
{
#ifdef MINGW                    // TODO: remove uid for NT ?
    char sid[CF_MAXSIDSIZE];    /* Invalid sid indicates unset */
#endif                          /* MINGW */
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

enum matchtypes
{
    literalStart,
    literalComplete,
    literalSomewhere,
    regexComplete,
    NOTliteralStart,
    NOTliteralComplete,
    NOTliteralSomewhere,
    NOTregexComplete
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

#ifdef NT
# define IsFileSep(c) ((c) == '\\' || (c) == '/')
#else
# define IsFileSep(c) ((c) == '/')
#endif

/*************************************************************************/
/* Fundamental (meta) types                                              */
/*************************************************************************/

#define CF_SCALAR 's'
#define CF_LIST   'l'
#define CF_FNCALL 'f'
#define CF_STACK  'k'
#define CF_ASSOC  'a'

#define CF_MAPPEDLIST '#'

#define CF_NOPROMISEE 'X'
#define CF_UNDEFINED -1
#define CF_NODOUBLE -123.45
#define CF_NOINT    -678L
#define CF_UNDEFINED_ITEM (void *)0x1234
#define CF_VARARGS 99
#define CF_UNKNOWN_IP "location unknown"

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

enum cfdatatype
{
    cf_str,
    cf_int,
    cf_real,
    cf_slist,
    cf_ilist,
    cf_rlist,
    cf_opts,
    cf_olist,
    cf_body,
    cf_bundle,
    cf_class,
    cf_clist,
    cf_irange,
    cf_rrange,
    cf_counter,
    cf_notype
};

enum cfx_formatindex
{
    cfb,
    cfe,
};

enum cfx_format
{
    cfx_head,
    cfx_bundle,
    cfx_block,
    cfx_blockheader,
    cfx_blockid,
    cfx_blocktype,
    cfx_args,
    cfx_promise,
    cfx_class,
    cfx_subtype,
    cfx_object,
    cfx_lval,
    cfx_rval,
    cfx_qstring,
    cfx_rlist,
    cfx_function,
    cfx_line,
};

/*************************************************************************/

#define CF_COMMONC  "common"
#define CF_AGENTC   "agent"
#define CF_SERVERC  "server"
#define CF_MONITORC "monitor"
#define CF_EXECC    "executor"
#define CF_KNOWC    "knowledge"
#define CF_RUNC     "runagent"
#define CF_REPORTC  "reporter"
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
    AGENT_TYPE_KNOW,
    AGENT_TYPE_REPORT,
    AGENT_TYPE_KEYGEN,
    AGENT_TYPE_HUB,
    AGENT_TYPE_GENDOC,
    AGENT_TYPE_NOAGENT
} AgentType;

enum typesequence
{
    kp_meta,
    kp_vars,
    kp_defaults,
    kp_classes,
    kp_outputs,
    kp_interfaces,
    kp_files,
    kp_packages,
    kp_environments,
    kp_methods,
    kp_processes,
    kp_services,
    kp_commands,
    kp_storage,
    kp_databases,
    kp_reports,
    kp_none
};

/*************************************************************************/

enum cfgcontrol
{
    cfg_bundlesequence,
    cfg_goalpatterns,
    cfg_ignore_missing_bundles,
    cfg_ignore_missing_inputs,
    cfg_inputs,
    cfg_version,
    cfg_lastseenexpireafter,
    cfg_output_prefix,
    cfg_domain,
    cfg_require_comments,
    cfg_licenses,
    cfg_site_classes,
    cfg_syslog_host,
    cfg_syslog_port,
    cfg_fips_mode,
    cfg_noagent
};

/*************************************************************************/

enum cfacontrol
{
    cfa_abortclasses,
    cfa_abortbundleclasses,
    cfa_addclasses,
    cfa_agentaccess,
    cfa_agentfacility,
    cfa_allclassesreport,
    cfa_alwaysvalidate,
    cfa_auditing,
    cfa_binarypaddingchar,
    cfa_bindtointerface,
    cfa_hashupdates,
    cfa_childlibpath,
    cfa_checksum_alert_time,
    cfa_defaultcopytype,
    cfa_dryrun,
    cfa_editbinaryfilesize,
    cfa_editfilesize,
    cfa_environment,
    cfa_exclamation,
    cfa_expireafter,
    cfa_fsinglecopy,
    cfa_fautodefine,
    cfa_hostnamekeys,
    cfa_ifelapsed,
    cfa_inform,
    cfa_intermittency,
    cfa_max_children,
    cfa_maxconnections,
    cfa_mountfilesystems,
    cfa_nonalphanumfiles,
    cfa_repchar,
    cfa_refresh_processes,
    cfa_repository,
    cfa_secureinput,
    cfa_sensiblecount,
    cfa_sensiblesize,
    cfa_skipidentify,
    cfa_suspiciousnames,
    cfa_syslog,
    cfa_track_value,
    cfa_timezone,
    cfa_timeout,
    cfa_verbose,
    cfa_notype,
};

/*************************************************************************/

enum cfexcontrol
{
    cfex_splaytime,
    cfex_mailfrom,
    cfex_mailto,
    cfex_smtpserver,
    cfex_mailmaxlines,
    cfex_schedule,
    cfex_executorfacility,
    cfex_execcommand,
    cfex_agent_expireafter,
    cfex_notype,
};

/*************************************************************************/

enum cfmcontrol
{
    cfm_forgetrate,
    cfm_monitorfacility,
    cfm_histograms,
    cfm_tcpdump,
    cfm_notype,
};

/*************************************************************************/

enum cfrcontrol
{
    cfr_hosts,
    cfr_portnumber,
    cfr_force_ipv4,
    cfr_trustkey,
    cfr_encrypt,
    cfr_background,
    cfr_maxchild,
    cfr_output_to_file,
    cfr_output_directory,
    cfr_timeout,
    cfr_notype
};

/*************************************************************************/

enum cfscontrol
{
    cfs_allowallconnects,
    cfs_allowconnects,
    cfs_allowusers,
    cfs_auditing,
    cfs_bindtointerface,
    cfs_cfruncommand,
    cfs_call_collect_interval,
    cfs_collect_window,
    cfs_denybadclocks,
    cfs_denyconnects,
    cfs_dynamicaddresses,
    cfs_hostnamekeys,
    cfs_keyttl,
    cfs_logallconnections,
    cfs_logencryptedtransfers,
    cfs_maxconnections,
    cfs_portnumber,
    cfs_serverfacility,
    cfs_skipverify,
    cfs_trustkeysfrom,
    cfs_listen,
    cfs_notype,
};

/*************************************************************************/

enum cfkcontrol
{
    cfk_builddir,
    cfk_docroot,
    cfk_genman,
    cfk_graph_dir,
    cfk_graph_output,
    cfk_htmlbanner,
    cfk_htmlfooter,
    cfk_tm_prefix,
    cfk_mandir,
    cfk_query_engine,
    cfk_query_output,
    cfk_sql_type,
    cfk_sql_database,
    cfk_sql_owner,
    cfk_sql_passwd,
    cfk_sql_server,
    cfk_sql_connect_db,
    cfk_stylesheet,
    cfk_views,
    cfk_notype
};

/*************************************************************************/

enum cfrecontrol
{
    cfre_aggregation_point,
    cfre_autoscale,
    cfre_builddir,
    cfre_csv,
    cfre_errorbars,
    cfre_htmlbanner,
    cfre_html_embed,
    cfre_htmlfooter,
    cfre_query_engine,
    cfre_reports,
    cfre_report_output,
    cfre_stylesheet,
    cfre_timestamps,
    cfre_notype
};

/*************************************************************************/

enum cfhcontrol
{
    cfh_export_zenoss,
    cfh_exclude_hosts,
    cfh_schedule,
    cfh_port,
    cfh_notype
};

/*************************************************************************/

enum cfsbundle
{
    cfs_access,
    cfs_nobtype
};

enum cfsrole
{
    cfs_authorize,
    cfs_nortype
};

enum cfspromises
{
    cfs_admit,
    cfs_deny,
    cfs_maproot,
    cfs_encrypted,
    cfs_noptype
};

enum cfreport
{
    cf_inform,
    cf_verbose,
    cf_error,
    cf_log,
    cf_reporting,
    cf_cmdout,
    cf_noreport
};

enum cfeditorder
{
    cfe_before,
    cfe_after
};

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

#ifndef MINGW
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
    REPORT_OUTPUT_TYPE_TEXT,
    REPORT_OUTPUT_TYPE_HTML,
    REPORT_OUTPUT_TYPE_KNOWLEDGE,

    REPORT_OUTPUT_TYPE_MAX
} ReportOutputType;

typedef struct
{
    Writer *report_writers[REPORT_OUTPUT_TYPE_MAX];
} ReportContext;


/*************************************************************************/

typedef struct
{
    const char *lval;
    const enum cfdatatype dtype;
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
    enum cfdatatype dtype;
    const char *description;
} FnCallArg;

typedef struct
{
    const char *name;
    enum cfdatatype dtype;
    const FnCallArg *args;
              FnCallResult(*impl) (FnCall *, Rlist *);
    const char *description;
    bool varargs;
} FnCallType;

/*************************************************************************/

#define UNKNOWN_FUNCTION -1

/*************************************************************************/

typedef struct
{
    size_t start;
    size_t end;
    size_t line;
    size_t context;
} SourceOffset;

struct Bundle_
{
    Policy *parent_policy;

    char *type;
    char *name;
    char *namespace;
    Rlist *args;
    SubType *subtypes;
    struct Bundle_ *next;

    char *source_path;
    SourceOffset offset;
};

/*************************************************************************/

typedef struct Constraint_ Constraint;

struct Body_
{
    Policy *parent_policy;

    char *type;
    char *name;
    char *namespace;
    Rlist *args;
    Constraint *conlist;
    Body *next;

    char *source_path;
    SourceOffset offset;
};

/*************************************************************************/

struct SubType_
{
    Bundle *parent_bundle;

    char *name;
    Promise *promiselist;
    SubType *next;

    SourceOffset offset;
};

/*************************************************************************/

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

/*************************************************************************/

struct Promise_
{
    SubType *parent_subtype;

    char *classes;
    char *ref;                  /* comment */
    char ref_alloc;
    char *promiser;
    Rval promisee;
    char *bundle;
    Audit *audit;
    Constraint *conlist;
    Promise *next;

    /* Runtime bus for private flags and work space */

    char *agentsubtype;         /* cache the promise subtype */
    char *bundletype;           /* cache the agent type */
    char *namespace;            /* cache the namespace */
    int done;                   /* this needs to be preserved across runs */
    int *donep;                 /* used by locks to mark as done */
    int makeholes;
    char *this_server;
    int has_subbundles;
    Stat *cache;
    AgentConnection *conn;
    CompressedArray *inode_cache;
    EditContext *edcontext;
    dev_t rootdevice;           /* for caching during work */
    const Promise *org_pp;            /* A ptr to the unexpanded raw promise */

    SourceOffset offset;
};

/*************************************************************************/

typedef struct PromiseIdent_
{
    char *handle;
    char *filename;
    char *classes;
    int line_number;
    struct PromiseIdent_ *next;
} PromiseIdent;

/*************************************************************************/
/* Rvalues and lists - basic workhorse structure                         */
/*************************************************************************/

/*
  In an OO language one would probably think of Rval as a parent class
  and CF_SCALAR, CF_LIST and CF_FNCALL as children. There is more or
  less a sub-type polymorphism going on in the code around these structures,
  but it is not a proper inheritance relationship as lists could
  contain functions which return lists or scalars etc..

*/

/*************************************************************************/

struct FnCall_
{
    char *name;
    Rlist *args;
    char *namespace;
};

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

/*
 * Disposable iterator over hash table. Does not require deinitialization.
 */
typedef struct HashIterator_
{
    AssocHashTable *hashtable;
    int pos;
} HashIterator;

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

enum cfinterval
{
    cfa_hourly,
    cfa_daily,
    cfa_nointerval
};

enum cfdatetemplate
{
    cfa_year,
    cfa_month,
    cfa_day,
    cfa_hour,
    cfa_min,
    cfa_sec
};

enum cfcomparison
{
    cfa_atime,
    cfa_mtime,
    cfa_ctime,
    cfa_checksum,
    cfa_hash,
    cfa_binary,
    cfa_exists,
    cfa_nocomparison
};

enum cflinktype
{
    cfa_symlink,
    cfa_hardlink,
    cfa_relative,
    cfa_absolute,
    cfa_notlinked
};

enum cfopaction
{
    cfa_fix,
    cfa_warn,
};

enum cfbackupoptions
{
    cfa_backup,
    cfa_nobackup,
    cfa_timestamp,
    cfa_rotate,
    cfa_repos_store             /* for internal use only */
};

enum cftidylinks
{
    cfa_linkdelete,
    cfa_linkkeep
};

enum cfhashes
{
    cf_md5,
    cf_sha224,
    cf_sha256,
    cf_sha384,
    cf_sha512,
    cf_sha1,
    cf_sha,
    cf_besthash,
    cf_crypt,
    cf_nohash
};

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

enum cfchanges
{
    cfa_noreport,
    cfa_contentchange,
    cfa_statschange,
    cfa_allchanges
};

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

enum representations
{
    cfk_url,
    cfk_web,
    cfk_file,
    cfk_db,
    cfk_literal,
    cfk_image,
    cfk_portal,
    cfk_none
};

enum package_actions
{
    cfa_addpack,
    cfa_deletepack,
    cfa_reinstall,
    cfa_update,
    cfa_addupdate,
    cfa_patch,
    cfa_verifypack,
    cfa_pa_none
};

enum version_cmp
{
    cfa_eq,
    cfa_neq,
    cfa_gt,
    cfa_lt,
    cfa_ge,
    cfa_le,
    cfa_cmp_none
};

enum action_policy
{
    cfa_individual,
    cfa_bulk,
    cfa_no_ppolicy
};

/*
Adding new mutex:
- add declaration here,
- define in cf3globals.c.
*/

#if defined(HAVE_PTHREAD)
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
#endif

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
    cf_file_new,
    cf_file_removed,
    cf_file_content_changed,
    cf_file_stats_changed
}FileState;

/************************************************************************************/

enum cf_acl_method
{
    cfacl_append,
    cfacl_overwrite,
    cfacl_nomethod
};

enum cf_acl_type
{
    cfacl_generic,
    cfacl_posix,
    cfacl_ntfs,
    cfacl_notype
};

enum cf_acl_inherit
{
    cfacl_nochange,
    cfacl_specify,
    cfacl_parent,
    cfacl_clear,
    cfacl_noinherit,
};

typedef struct
{
    enum cf_acl_method acl_method;
    enum cf_acl_type acl_type;
    enum cf_acl_inherit acl_directory_inherit;
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

enum insert_match
{
    cf_ignore_leading,
    cf_ignore_trailing,
    cf_ignore_embedded,
    cf_exact_match
};

enum monitord_rep
{
    mon_rep_mag,
    mon_rep_week,
    mon_rep_yr
};

enum software_rep
{
    sw_rep_installed,
    sw_rep_patch_avail,
    sw_rep_patch_installed
};

/*************************************************************************/

enum cfd_menu
{
    cfd_menu_delta,
    cfd_menu_full,
    cfd_collect_call,
    cfd_menu_error
};

/*************************************************************************/

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
    enum cfreport report_level;
    enum cfreport log_level;
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
    enum statepolicy timer;
    Rlist *del_change;
    Rlist *del_kept;
    Rlist *del_notkept;
    Rlist *retcode_kept;
    Rlist *retcode_repaired;
    Rlist *retcode_failed;
} DefineClasses;

/*************************************************************************/
/* Ontology                                                              */
/*************************************************************************/

typedef struct Topic_ Topic;
typedef struct TopicAssociation_ TopicAssociation;

struct Topic_
{
    int id;
    char *topic_context;
    char *topic_name;
    char *bundle;
    double evc;
    TopicAssociation *associations;
    Topic *next;
};

struct TopicAssociation_
{
    char *fwd_context;
    char *fwd_name;
    char *bwd_context;
    char *bwd_name;
    Item *associates;
    TopicAssociation *next;
};

typedef struct Occurrence_ Occurrence;

struct Occurrence_
{
    char *occurrence_context;
    char *locator;                 /* Promiser */
    char *bundle;
    enum representations rep_type;
    Rlist *represents;
    Rlist *about_topics;    
    Occurrence *next;
};

typedef struct Inference_ Inference;

struct Inference_
{
    char *inference;            // Promiser
    char *precedent;
    char *qualifier;
    Inference *next;
};

/*************************************************************************/
/* SQL Database connectors                                               */
/*************************************************************************/

enum cfdbtype
{
    cfd_mysql,
    cfd_postgres,
    cfd_notype
};

typedef struct
{
    int connected;
    int result;
    int row;
    unsigned int maxcolumns;
    unsigned int maxrows;
    int column;
    char **rowdata;
    char *blank;
    enum cfdbtype type;
    void *data;                 /* Generic pointer to RDBMS-specific data */
} CfdbConn;

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
    enum package_actions action;
    enum action_policy policy;
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
    enum cfcomparison compare;
    enum cflinktype link_type;
    Rlist *servers;
    Rlist *link_instead;
    Rlist *copy_links;
    enum cfbackupoptions backup;
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
    enum statepolicy policy;
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
    enum cfhashes hash;
    enum cfchanges report_changes;
    int report_diffs;
    int update;
} FileChange;

/*************************************************************************/

typedef struct
{
    char *source;
    enum cflinktype link_type;
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
    enum cfbackupoptions backup;
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
    enum cfeditorder before_after;
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
    enum package_actions package_policy;
    int have_package_methods;
    char *package_version;
    Rlist *package_architectures;
    enum version_cmp package_select;
    enum action_policy package_changes;
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

enum cfmeasurepolicy
{
    cfm_average,
    cfm_sum,
    cfm_first,
    cfm_last,
    cfm_nomeasure
};

typedef struct
{
    char *stream_type;
    enum cfdatatype data_type;
    enum cfmeasurepolicy policy;
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
    enum cfdbtype db_server_type;
    char *server;
    char *type;
    char *operation;
    Rlist *columns;
    Rlist *rows;
    Rlist *exclude;
} Database;

/*************************************************************************/

enum cf_srv_policy
{
    cfsrv_start,
    cfsrv_stop,
    cfsrv_disable,
    cfsrv_restart,
    cfsrv_reload,
    cfsrv_nostatus
};

typedef struct
{
    Rlist *service_depend;
    char *service_type;
    char *service_args;
    enum cf_srv_policy service_policy;
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

enum cfhypervisors
{
    cfv_virt_xen,
    cfv_virt_kvm,
    cfv_virt_esx,
    cfv_virt_vbox,
    cfv_virt_test,
    cfv_virt_xen_net,
    cfv_virt_kvm_net,
    cfv_virt_esx_net,
    cfv_virt_test_net,
    cfv_zone,
    cfv_ec2,
    cfv_eucalyptus,
    cfv_none
};

enum cfenvironment_state
{
    cfvs_create,
    cfvs_delete,
    cfvs_running,
    cfvs_suspended,
    cfvs_down,
    cfvs_none
};

/*************************************************************************/

enum cf_meter
{
    meter_compliance_week,
    meter_compliance_day,
    meter_compliance_hour,
    meter_perf_day,
    meter_other_day,
    meter_comms_hour,
    meter_anomalies_day,
    meter_compliance_week_user,
    meter_compliance_week_internal,
    meter_compliance_day_user,
    meter_compliance_day_internal,
    meter_compliance_hour_user,
    meter_compliance_hour_internal,
    meter_endmark
};

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
    enum cfenvironment_state state;
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

    /* knowledge */

    char *fwd_name;
    char *bwd_name;
    Rlist *precedents;
    Rlist *qualifiers;
    Rlist *associates;
    Rlist *represents;
    Rlist *about_topics;
    Rlist *synonyms;
    Rlist *general;
    char *rep_type;
} Attributes;

/*************************************************************************/
/* definitions for reporting                                            */
/*************************************************************************/

extern double METER_KEPT[meter_endmark];
extern double METER_REPAIRED[meter_endmark];

/*************************************************************************/
/* common macros                                                         */
/*************************************************************************/

#define NULL_OR_EMPTY(str) ((str == NULL) || (str[0] == '\0'))
#define BEGINSWITH(str,start) (strncmp(str,start,strlen(start)) == 0)

// classes not interesting in reports
#define IGNORECLASS(c)                                                         \
 (strncmp(c,"Min",3) == 0 || strncmp(c,"Hr",2) == 0 || strcmp(c,"Q1") == 0     \
  || strcmp(c,"Q2") == 0 || strcmp(c,"Q3") == 0 || strcmp(c,"Q4") == 0         \
  || strncmp(c,"GMT_Hr",6) == 0  || strncmp(c,"Yr",2) == 0                     \
  || strncmp(c,"Day",3) == 0 || strcmp(c,"license_expired") == 0               \
  || strcmp(c,"any") == 0 || strcmp(c,"from_cfexecd") == 0                     \
  || IsStrIn(c,MONTH_TEXT) || IsStrIn(c,DAY_TEXT)                  \
  || IsStrIn(c,SHIFT_TEXT)) || strncmp(c,"Lcycle",6) == 0

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

#ifdef HAVE_NOVA
# include <cf.nova.h>
#endif


#endif

