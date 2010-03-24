/* 
   Copyright (C) 2008 - Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version. 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

*/

/*****************************************************************************/
/*                                                                           */
/* File: cf3.defs.h                                                          */
/*                                                                           */
/*****************************************************************************/

/* These files are hard links to the cfengine 2 sources */

#include "cf.defs.h"
#include "cf.extern.h"

#undef VERSION
#undef Verbose

#include "conf.h"

#ifdef HAVE_PCRE_H
#include <pcre.h>
#endif

#ifdef HAVE_PCRE_PCRE_H
#include <pcre/pcre.h>
#endif


#ifndef NGROUPS
# define NGROUPS 20
#endif

/*************************************************************************/
/* Fundamental (meta) types                                              */
/*************************************************************************/

#define CF3COPYRIGHT "(C) Cfengine AS 2008-"


#define LIC_DAY "1"
#define LIC_MONTH "July"
#define LIC_YEAR "2000"

#define CF_SCALAR 's'
#define CF_LIST   'l'
#define CF_FNCALL 'f'
#define CF_STACK  'k'

#define CF_NOPROMISEE 'X'
#define CF_UNDEFINED -1
#define CF_NODOUBLE -123.45
#define CF_NOINT    -678L
#define CF_ANYUSER  (uid_t)-1
#define CF_ANYGROUP (gid_t)-1
#define CF_UNDEFINED_ITEM (void *)0x1234
#define CF_VARARGS 99

#define CF_INBODY   1
#define CF_INBUNDLE 2

#define CF_MAX_NESTING 3
#define CF_DONEPASSES  4

#define CF_TIME_SIZE 32
#define CF_FIPS_SIZE 32

#define CFPULSETIME 60

/*************************************************************************/
/** Design criteria                                                      */
/*************************************************************************/

#define CF_DUNBAR_INTIMATE 6
#define CF_DUNBAR_WORK 30
#define CF_DUNBAR_KNOW 120

/*************************************************************************/
/* Parsing and syntax tree structures                                    */
/*************************************************************************/

#define CF_DEFINECLASSES "classes"
#define CF_TRANSACTION   "action"

#define CF3_MODULES 13 /* This value needs to be incremented when adding modules */

/*************************************************************************/

struct PromiseParser
   {
   char *block;                     /* body/bundle  */
   char *blocktype;
   char *blockid;

   char *filename;
   int line_pos;
   int line_no;

   int arg_nesting;
   int list_nesting;
      
   char *lval;
   void *rval;
   char rtype;
   int isbody;

   char *promiser;
   void *promisee;

   char *currentid;
   char *currentstring;
   char *currenttype;
   char *currentclasses;

   struct Bundle *currentbundle;
   struct Body *currentbody;
   struct Promise *currentpromise;
   struct SubType *currentstype;
   struct Rlist *useargs;
      
   struct RList *currentRlist;

   char *currentfnid[CF_MAX_NESTING];
   struct Rlist *giveargs[CF_MAX_NESTING];
   struct FnCall *currentfncall[CF_MAX_NESTING];
   };

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

enum cfagenttype
   {
   cf_common,
   cf_agent,
   cf_server,
   cf_monitor,
   cf_executor,
   cf_runagent,
   cf_know,
   cf_report,
   cf_keygen,
   cf_noagent
   };

/*************************************************************************/

enum cfgcontrol
   {
   cfg_bundlesequence,
   cfg_ignore_missing_bundles,
   cfg_ignore_missing_inputs,
   cfg_inputs,
   cfg_version,
   cfg_lastseenexpireafter,
   cfg_output_prefix,
   cfg_domain,
   cfg_require_comments,
   cfg_licenses,
   cfg_syslog_host,
   cfg_syslog_port,
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
   cfa_fullencryption,
   cfa_hostnamekeys,
   cfa_ifelapsed,
   cfa_inform,
   cfa_lastseen,
   cfa_intermittency,
   cfa_max_children,
   cfa_maxconnections,
   cfa_mountfilesystems,
   cfa_nonalphanumfiles,
   cfa_repchar,
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
   cfr_notype
   };

/*************************************************************************/

enum cfscontrol
   {
   cfs_cfruncommand,
   cfs_maxconnections,
   cfs_denybadclocks,
   cfs_allowconnects,
   cfs_denyconnects,
   cfs_allowallconnects,
   cfs_trustkeysfrom,
   cfs_allowusers,
   cfs_dynamicaddresses,
   cfs_skipverify,
   cfs_logallconnections,
   cfs_logencryptedtransfers,
   cfs_hostnamekeys,
   cfs_auditing,
   cfs_bindtointerface,
   cfs_serverfacility,
   cfs_portnumber,
   cfs_notype,
   };

/*************************************************************************/

enum cfkcontrol
   {
   cfk_tm_prefix,
   cfk_builddir,
   cfk_sql_type,
   cfk_sql_database,
   cfk_sql_owner,
   cfk_sql_passwd,
   cfk_sql_server,
   cfk_sql_connect_db,
   cfk_query_output,
   cfk_query_engine,
   cfk_stylesheet,
   cfk_htmlbanner,
   cfk_htmlfooter,
   cfk_graph_output,
   cfk_graph_dir,
   cfk_genman,
   cfk_mandir,
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

#define CF_BUNDLE  (void*)1234           /* any non-null value, not used */

#define CF_HIGHINIT 99999L
#define CF_LOWINIT -999999L

#define CF_SIGNALRANGE "hup,int,trap,kill,pipe,cont,abrt,stop,quit,term,child,usr1,usr2,bus,segv"
#define CF_BOOL      "true,false,yes,no,on,off"
#define CF_LINKRANGE "symlink,hardlink,relative,absolute,none"
#define CF_TIMERANGE "0,2147483647"
#define CF_VALRANGE  "0,99999999999"
#define CF_INTRANGE  "-99999999999,9999999999"
#define CF_REALRANGE "-9.99999E100,9.99999E100"
#define CF_CHARRANGE "^.$"

#define CF_MODERANGE   "[0-7augorwxst,+-]+"
#define CF_BSDFLAGRANGE "[+-]*[(arch|archived|nodump|opaque|sappnd|sappend|schg|schange|simmutable|sunlnk|sunlink|uappnd|uappend|uchg|uchange|uimmutable|uunlnk|uunlink)]+"
#define CF_CLASSRANGE  "[a-zA-Z0-9_!&@@$|.()]+"
#define CF_IDRANGE     "[a-zA-Z0-9_$()\\[\\].]+"
#define CF_USERRANGE   "[a-zA-Z0-9_$.-]+"
#define CF_IPRANGE     "[a-zA-Z0-9_$.:-]+"
#define CF_FNCALLRANGE "[a-zA-Z0-9_().$@]+"
#define CF_NAKEDLRANGE "@[(][a-zA-Z0-9]+[)]"
#define CF_ANYSTRING   ".*"
#define CF_PATHRANGE   "\042?(([a-zA-Z]:\\\\.*)|(/.*))"  // can start with e.g. c:\... or "c:\...  |  unix
#define CF_LOGRANGE    "stdout|udp_syslog|(\042?[a-zA-Z]:\\\\.*)|(/.*)"

#define CF_FACILITY "LOG_USER,LOG_DAEMON,LOG_LOCAL0,LOG_LOCAL1,LOG_LOCAL2,LOG_LOCAL3,LOG_LOCAL4,LOG_LOCAL5,LOG_LOCAL6,LOG_LOCAL7"

/*************************************************************************/

struct BodySyntax
   {
   char *lval;
   enum cfdatatype dtype;
   void *range;               /* either char or struct BodySyntax **/
   char *description;
   };

/*************************************************************************/

struct SubTypeSyntax
   {
   char *btype;
   char *subtype;
   struct BodySyntax *bs;
   };

/*************************************************************************/

struct FnCallType
   {
   char *name;
   enum cfdatatype dtype;
   int numargs;
   char *description;
   };

/*************************************************************************/

enum fncalltype
   {
   cfn_accessedbefore,
   cfn_accum,
   cfn_ago,
   cfn_canonify,
   cfn_changedbefore,
   cfn_classify,
   cfn_classmatch,
   cfn_countclassesmatching,
   cfn_countlinesmatching,
   cfn_diskfree,
   cfn_escape,
   cfn_execresult,
   cfn_fileexists,
   cfn_filesexist,
   cfn_getfields,
   cfn_getindices,
   cfn_getenv,
   cfn_getgid,
   cfn_getuid,
   cfn_grep,
   cfn_groupexists,
   cfn_hash,
   cfn_hashmatch,
   cfn_host2ip,
   cfn_hostinnetgroup,
   cfn_hostrange,
   cfn_hostsseen,
   cfn_iprange,
   cfn_irange,
   cfn_isdir,
   cfn_isgreaterthan,
   cfn_islessthan,
   cfn_islink,
   cfn_isnewerthan,
   cfn_isplain,
   cfn_isvariable,
   cfn_join,
   cfn_lastnode,
   cfn_ldaparray,
   cfn_ldaplist,
   cfn_ldapvalue,
   cfn_now,
   cfn_date,
   cfn_peers,
   cfn_peerleader,
   cfn_peerleaders,
   cfn_randomint,
   cfn_readfile,
   cfn_readintarray,
   cfn_readintlist,
   cfn_readrealarray,
   cfn_readreallist,
   cfn_readstringarray,
   cfn_readstringlist,   
   cfn_readtcp,
   cfn_regarray,
   cfn_regcmp,
   cfn_regextract,
   cfn_registryvalue,
   cfn_regline,
   cfn_reglist,
   cfn_regldap,
   cfn_remotescalar,
   cfn_remoteclassesmatching,
   cfn_returnszero,
   cfn_rrange,
   cfn_selectservers,
   cfn_splayclass,
   cfn_splitstring,
   cfn_strcmp,
   cfn_translatepath,
   cfn_usemodule,
   cfn_userexists,
   cfn_unknown,
   };

/*************************************************************************/

struct Bundle
   {
   char *type;
   char *name;
   struct Rlist *args;
   struct SubType *subtypes;
   struct Bundle *next;
   };

/*************************************************************************/

struct Body
   {
   char *type;
   char *name;
   struct Rlist *args;
   struct Constraint *conlist;
   struct Body *next;
   };

/*************************************************************************/

struct SubType
   {
   char *name;
   struct Promise *promiselist;
   struct SubType *next;
   };

/*************************************************************************/

struct edit_context
   {
   char *filename;
   struct Item *file_start;
   struct Item *file_classes;
   int num_edits;
   };

/*************************************************************************/

struct Promise
   {
   char *classes;
   char *ref;                   /* comment */
   char *promiser;
   void *promisee;              /* Can be a general rval */
   char  petype;                /* rtype of promisee - list or scalar recipient? */
   int   lineno;
   char *bundle;
   struct Audit *audit;
   struct Constraint *conlist;
   struct Promise *next;
      
    /* Runtime bus for private flags and work space */

   char  *agentsubtype;         /* cache the promise subtype */
   char  *bundletype;           /* cache the agent type */
   int    done;                 /* this needs to be preserved across runs */
   int   *donep;                /* used by locks to mark as done */
   int    makeholes;
   char  *this_server;
   struct cfstat *cache;      
   struct cfagent_connection *conn;
   struct CompressedArray *inode_cache;
   struct edit_context *edcontext;
   dev_t rootdevice;                          /* for caching during work*/
   };

/*************************************************************************/

struct PromiseIdent
   {
   char *handle;
   char *filename;
   int lineno;
   struct PromiseIdent *next;
   };

/*************************************************************************/

struct Constraint
   {
   char *lval;
   void *rval;    /* should point to either string, Rlist or FnCall */
   char type;     /* scalar, list, or function */
   char *classes; /* only used within bodies */
   int lineno;
   int isbody;
   struct Audit *audit;
   struct Constraint *next;
   };

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

struct Rval
   {
   void *item;        /* (char *), (struct Rlist *), or (struct FnCall)  */
   char rtype;        /* Points to CF_SCALAR, CF_LIST, CF_FNCALL usually */
   };

/*************************************************************************/

struct Rlist
   {
   void *item;
   char type;
   struct Rlist *state_ptr; /* Points to "current" state/element of sub-list */
   struct Rlist *next;
   };

/*************************************************************************/

struct FnCall
   {
   char *name;
   struct Rlist *args;
   int argc;
   };

/*******************************************************************/
/* Variable processing                                             */
/*******************************************************************/

struct Scope                         /* $(bundlevar) $(scope.name) */
   {
   char *scope;                                 /* Name of scope */
   struct CfAssoc *hashtable[CF_HASHTABLESIZE]; /* Variable heap  */
   struct Scope *next;
   };

/*******************************************************************/

struct CfAssoc        /* variable reference linkage , with metatype*/
   {
   char *lval;
   void *rval;
   char rtype;
   enum cfdatatype dtype;
   };

/*******************************************************************/
/* Return value signalling                                         */
/*******************************************************************/

#define FNCALL_NOP     -1
#define FNCALL_SUCCESS 0
#define FNCALL_FAILURE 1
#define FNCALL_ALERT   2

struct FnCallStatus  /* from builtin functions */
   {
   int status;
   char message[CF_BUFSIZE];
   char fncall_classes[CF_BUFSIZE]; /* set by functions in the form fncall_CLASS */
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
   cfa_repos_store/* for internal use only */
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

enum cf_thread_mutex
  {
  cft_system,
  cft_count,
  cft_getaddr,
  cft_lock,
  cft_output,
  cft_dbhandle,
  cft_no_tpolicy
  };

enum cf_status
  {
  cfn_repaired,
  cfn_notkept,
  cfn_nop
  };

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

struct CfACL
   {
   enum cf_acl_method acl_method;
   enum cf_acl_type acl_type;
   enum cf_acl_inherit acl_directory_inherit;
   struct Rlist *acl_entries;
   struct Rlist *acl_inherit_entries;
   };

typedef enum
  {
  INHERIT_ACCESS_ONLY,
  INHERIT_DEFAULT_ONLY,
  INHERIT_ACCESS_AND_DEFAULT
  }inherit_t;


/*************************************************************************/
/* Runtime constraint structures                                         */
/*************************************************************************/

#define OVECCOUNT 30

struct CfRegEx
{
#if defined HAVE_PCRE_H || defined HAVE_PCRE_PCRE_H
   pcre *rx;
   const char *err;
   int err_offset;
#else
   regex_t rx;
#endif
   int failed;
   char *regexp;
};

/*************************************************************************/

struct CfLock
   {
   char *last;
   char *lock;
   char *log;
   };

/*************************************************************************/

struct CfMount
   {
   char *host;
   char *source;
   char *mounton;
   char *options;
   int unmount;
   };

/*************************************************************************/

struct Recursion
   {
   int travlinks;
   int rmdeadlinks;
   int depth;
   int xdev;
   int include_basedir;
   struct Rlist *include_dirs;
   struct Rlist *exclude_dirs;
   };

/*************************************************************************/

struct TransactionContext
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
   int  audit;
   enum cfreport report_level;
   enum cfreport log_level;
   };

/*************************************************************************/

struct DefineClasses
   {
   struct Rlist *change;
   struct Rlist *failure;
   struct Rlist *denied;
   struct Rlist *timeout;
   struct Rlist *kept;
   struct Rlist *interrupt;
   int persist;
   enum statepolicy timer;
   struct Rlist *del_change;
   struct Rlist *del_kept;
   struct Rlist *del_notkept;
   };


/*************************************************************************/
/* Ontology                                                              */
/*************************************************************************/

struct Topic
   {
   char *topic_type;
   char *topic_name;
   char *topic_comment;
   struct Occurrence *occurrences;
   struct TopicAssociation *associations;
   struct Topic *next;
   };

struct TopicAssociation
   {
   char *assoc_type;
   char *fwd_name;
   char *bwd_name;
   struct Rlist *associates;
   char *associate_topic_type;
   struct TopicAssociation *next;
   };

struct Occurrence
   {
   char *locator; /* Promiser */
   enum representations rep_type;
   struct Rlist *represents; /* subtype represented by promiser */
   struct Occurrence *next;
   };


/*************************************************************************/
/* SQL Database connectors                                               */
/*************************************************************************/

#ifdef HAVE_MYSQL_MYSQL_H
#include <mysql/mysql.h>
#endif

#ifdef HAVE_PGSQL_LIBPQ_FE_H
#include <pgsql/libpq-fe.h>
#endif

#ifdef HAVE_LIBPQ_FE_H
#include <libpq-fe.h>
#endif

enum cfdbtype
   {
   cfd_mysql,
   cfd_postgres,
   cfd_notype
   };

typedef struct 
   {
#ifdef HAVE_MYSQL_MYSQL_H
   MYSQL my_conn;
   MYSQL_RES *my_res;
#endif
#if defined HAVE_PGSQL_LIBPQ_FE_H || defined HAVE_LIBPQ_FE_H
   PGconn *pq_conn;
   PGresult   *pq_res;
#endif
   int connected;
   int result;
   int row;
   int maxcolumns;
   int maxrows;
   int column;
   char **rowdata;
   char *blank;
   enum cfdbtype type;
   }
CfdbConn;

/*************************************************************************/
/* Threading container                                                   */
/*************************************************************************/

struct PromiseThread
   {
   enum cfagenttype agent;
   char *scopeid;
   struct Promise *pp;
   void *fnptr;
   };

/*************************************************************************/
/* Package promises                                                      */
/*************************************************************************/

struct CfPackageManager
   {
   char *manager;
   enum package_actions action;
   enum action_policy policy;
   struct CfPackageItem *pack_list;
   struct CfPackageItem *patch_list;
   struct CfPackageItem *patch_avail;
   struct CfPackageManager *next;
   };

/*************************************************************************/

struct CfPackageItem 
   {
   char *name;
   char *version;
   char *arch;
   struct Promise *pp;
   struct CfPackageItem *next;
   };

/*************************************************************************/
/* Files                                                                 */
/*************************************************************************/

struct FileCopy
   {
   char *source;
   char *destination;
   enum cfcomparison compare;
   enum cflinktype link_type;
   struct Rlist *servers;
   struct Rlist *link_instead;
   struct Rlist *copy_links;
   enum cfbackupoptions backup;
   int stealth;
   int preserve;
   int collapse;
   int check_root;
   int type_check;
   int force_update;
   int force_ipv4;
   size_t min_size;      /* Safety margin not search criterion */
   size_t max_size;
   int trustkey;
   int encrypt;
   int verify;
   int purge;
   short portnumber;
   };

struct ServerItem
   {
   char *server;
   struct cfagent_connection *conn;
   int busy;
   };

/*************************************************************************/

struct CfState
   {
   unsigned int expires;
   enum statepolicy policy;
   };

/*************************************************************************/

struct FilePerms
   {
   mode_t plus;
   mode_t minus;
   struct UidList *owners;
   struct GidList *groups;
   char  *findertype;
   u_long plus_flags;     /* for *BSD chflags */
   u_long minus_flags;    /* for *BSD chflags */
   int    rxdirs;
   };

/*************************************************************************/

struct FileSelect
   {
   struct Rlist *name;
   struct Rlist *path;
   struct Rlist *perms;
   struct Rlist *bsdflags;      
   struct Rlist *owners;
   struct Rlist *groups;
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
   struct Rlist *filetypes;
   struct Rlist *issymlinkto;
   char *result;
   };

/*************************************************************************/

struct FileDelete

   {
   enum cftidylinks dirlinks;
   int rmdirs;
   };

/*************************************************************************/

struct FileRename
   {
   char *newname;
   char *disable_suffix;
   int disable;
   int rotate;
   mode_t plus;
   mode_t minus;
   };

/*************************************************************************/

struct FileChange
   {
   enum cfhashes hash;
   enum cfchanges report_changes;
   int report_diffs;
   int update;
   };

/*************************************************************************/

struct FileLink
   {
   char *source;
   enum cflinktype link_type;
   struct Rlist *copy_patterns;
   enum cfnofile when_no_file;
   enum cflinkchildren when_linking_children;
   int link_children;   
   };

/*************************************************************************/

struct ExecContain
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
   };

/*************************************************************************/

struct ProcessCount
   {
   long min_range;
   long max_range;
   struct Rlist *in_range_define;
   struct Rlist *out_of_range_define;
   };

/*************************************************************************/

struct ProcessSelect
   {
   struct Rlist *owner;
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
   };

/*************************************************************************/

struct Context
   {
   struct Constraint *expression;
   int broken;
   };

/*************************************************************************/

struct EditDefaults
   {
   enum cfbackupoptions backup;
   int empty_before_use;
   int maxfilesize;
   };

/*************************************************************************/

struct LineSelect
   {
   struct Rlist *startwith_from_list;
   struct Rlist *not_startwith_from_list;
   struct Rlist *match_from_list;
   struct Rlist *not_match_from_list;
   struct Rlist *contains_from_list;
   struct Rlist *not_contains_from_list;
   };

struct EditLocation
   {
   char *line_matching;
   enum cfeditorder before_after;
   char *first_last;
   };

struct EditRegion
   {
   char *select_start;
   char *select_end;
   };

struct EditColumn
   {
   char *column_separator;
   int select_column;
   char value_separator;
   char *column_value;
   char *column_operation;
   int extend_columns;
   int blanks_ok;
   };

struct EditReplace
   {
   char *replace_value;
   char *occurrences;
   };

/*************************************************************************/

struct StorageMount
   {
   char *mount_type;
   char *mount_source;
   char *mount_server;
   struct Rlist *mount_options;
   int editfstab;
   int unmount;
   };

struct StorageVolume
   {
   int check_foreign;
   long freespace;
   int sensible_size;
   int sensible_count;
   int scan_arrivals;
   };

/*************************************************************************/

struct Report
   {
   int haveprintfile;
   int havelastseen;
   int lastseen;
   double intermittency;
   char *friend_pattern;
   char *filename;
   char *to_file;
   int numlines;
   struct Rlist *showstate;
   };

/*************************************************************************/

struct Packages
   {
   enum package_actions package_policy;
   int have_package_methods;
   char *package_version;
   struct Rlist *package_architectures;
   enum version_cmp package_select;
   enum action_policy package_changes;
   struct Rlist *package_file_repositories;

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

   char *package_multiline_start;
      
   int package_noverify_returncode;
   };

/*************************************************************************/

struct Measurement
   {
   char *stream_type;
   enum cfdatatype data_type;
   char *history_type;
   char *select_line_matching;
   int select_line_number;
   char *extraction_regex;
   char *units;
   int growing;
   };

/*************************************************************************/

struct CfTcpIp
   {
   char *ipv4_address;
   char *ipv4_netmask;
   };

/*************************************************************************/

struct CfDatabase
   {
   char *db_server_owner;
   char *db_server_password;
   char *db_server_host;
   char *db_connect_db;
   enum cfdbtype  db_server_type;
   char *server;
   char *type;
   char *operation;
   struct Rlist *columns;
   struct Rlist *rows;
   struct Rlist *exclude;
   };
    
/*************************************************************************/

enum cf_srv_policy
   {
   cfsrv_start,
   cfsrv_stop,
   cfsrv_disable,
   cfsrv_nostatus
   };

struct CfServices
   {
   struct Rlist *service_depend;
   char *service_type;
   char *service_args;
   enum cf_srv_policy service_policy;
   char *service_autostart_policy;
   char *service_depend_chain;
   };

/*************************************************************************/

 /* This is huge, but the simplification of logic is huge too
    so we leave it to the compiler to optimize */

struct Attributes
   {
   struct FileSelect select;
   struct FilePerms perms;
   struct FileCopy copy;
   struct FileDelete delete;
   struct FileRename rename;
   struct FileChange change;
   struct FileLink link;
   struct EditDefaults edits;
   struct Packages packages;
   struct Context context;
   struct Measurement measure;
   struct CfACL acl;
   struct CfDatabase database;
   struct CfServices service;
   char *transformer;
   char *pathtype;
   char *repository;
   int touch;
   int create;
   int move_obstructions;
      
   struct Recursion recursion;
   struct TransactionContext transaction;
   struct DefineClasses classes;

   struct ExecContain contain;
   char *args;
   int module;
   int exec_timeout;

   struct Rlist *signals;
   char *process_stop;
   char *restart_class;
   struct ProcessCount process_count;
   struct ProcessSelect process_select;

   struct Report report;
   struct StorageMount mount;
   struct StorageVolume volume;
      
   struct CfTcpIp tcpip;
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

   struct EditRegion region;
   struct EditLocation location;
   struct EditColumn column;
   struct EditReplace replace;
   int haveregion;
   int havelocation;
   int havecolumn;
   int havereplace;
   int haveinsertselect;
   int havedeleteselect;
   struct LineSelect line_select;
   char *sourcetype;
   int expandvars;
   int not_matching;

      /* knowledge */

   char *fwd_name;
   char *bwd_name;
   struct Rlist *associates;
   struct Rlist *represents;
   char *rep_type;
   char *path_root;
   char *web_root;
   };

enum cf_meter
{
meter_compliance_week,
meter_compliance_day,
meter_compliance_hour,
meter_patch_day,
meter_soft_day,
meter_comms_hour,
meter_anomalies_day,
meter_endmark
};

#include "prototypes3.h"

#ifdef HAVE_LIBCFNOVA
#include <cf.nova.h>
#endif
