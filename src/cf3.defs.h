/* 
   Copyright (C) 2008 - Mark Burgess

   This file is part of Cfengine 3 - written and maintained by Mark Burgess.
 
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

/*************************************************************************/
/* Fundamental (meta) types                                              */
/*************************************************************************/

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

#define CF_INBODY   1
#define CF_INBUNDLE 2

#define CF_MAX_NESTING 3

/*************************************************************************/
/* Parsing and syntax tree structures                                    */
/*************************************************************************/

#define CF_DEFINECLASSES "classes"
#define CF_TRANSACTION   "transaction"

#define CF3_MODULES 7 /* This value needs to be incremented when adding modules */

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
   cf_notype
   };

/*************************************************************************/

#define CF_COMMONC  "common"
#define CF_AGENTC   "agent"
#define CF_SERVERC  "server"
#define CF_MONITORC "monitor"
#define CF_EXECC    "executor"
#define CF_KNOWC    "knowledge"
#define CF_RUNC     "runagent"

enum cfagenttype
   {
   cf_common,
   cf_agent,
   cf_server,
   cf_monitor,
   cf_executor,
   cf_runagent,
   cf_know,
   cf_noagent
   };

/*************************************************************************/

enum cfacontrol
   {
   cfa_maxconnections,
   cfa_abortclasses,
   cfa_addclasses,
   cfa_agentaccess,
   cfa_agentfacility,
   cfa_auditing,
   cfa_binarypaddingchar,
   cfa_bindtointerface,
   cfa_checksumpurge,
   cfa_checksumupdates,
   cfa_compresscommand,
   cfa_childlibpath,
   cfa_defaultcopytype,
   cfa_deletenonuserfiles,
   cfa_deletenonownerfiles,
   cfa_deletenonusermail,
   cfa_deletenonownermail,
   cfa_dryrun,
   cfa_editbinaryfilesize,
   cfa_editfilesize,
   cfa_emptyresolvconf,
   cfa_exclamation,
   cfa_expireafter,
   cfa_fsinglecopy,
   cfa_fautodefine,
   cfa_fullencryption,
   cfa_hostnamekeys,
   cfa_ifelapsed,
   cfa_inform,
   cfa_lastseen,
   cfa_lastseenexpireafter,
   cfa_logtidyhomefiles,
   cfa_nonalphanumfiles,
   cfa_repchar,
   cfa_repository,
   cfa_sensiblecount,
   cfa_sensiblesize,
   cfa_showactions,
   cfa_skipidentify,
   cfa_spooldirectories,
   cfa_suspiciousnames,
   cfa_syslog,
   cfa_timezone,
   cfa_timeout,
   cfa_verbose,
   cfa_warnings,
   cfa_warnnonuserfiles,
   cfa_warnnonownerfiles,
   cfa_warnnonusermail,
   cfa_warnnonownermail,
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
   cfex_execcommand,
   cfex_notype,
   };

/*************************************************************************/

enum cfmcontrol
   {
   cfm_threshold,
   cfm_forgetrate,
   cfm_monitorfacility,
   cfm_histograms,
   cfm_tcpdump,
   cfm_notype,
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
   cfs_checkident,
   cfs_auditing,
   cfs_bindtointerface,
   cfs_serverfacility,
   cfs_notype,
   };

enum cfsbundle
   {
   cfs_access,
   cfs_nobtype
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
   cf_noreport
   };

/*************************************************************************/
/* Syntax module range/pattern constants for type validation             */
/*************************************************************************/

#define CF_BUNDLE  (void*)1234           /* any non-null value, not used */

#define CF_HIGHINIT 99999L
#define CF_LOWINIT -999999L

#define CF_BOOL      "true,false,yes,no,on,off"
#define CF_LINKRANGE "symlink,hardlink,relative,absolute"
#define CF_TIMERANGE "0,4026531839"
#define CF_VALRANGE  "0,99999999999"
#define CF_INTRANGE  "-99999999999,9999999999"
#define CF_REALRANGE "-9.99999E100,9.99999E100"
#define CF_CHARRANGE "^.$"

#define CF_MODERANGE   "[0-7augorwxst,+-]+"
#define CF_CLASSRANGE  "[a-zA-Z0-9_!&|.()]+"
#define CF_IDRANGE     "[a-zA-Z0-9_]+"
#define CF_FNCALLRANGE "[a-zA-Z0-9_().$@]+"
#define CF_ANYSTRING   ".*"
#define CF_PATHRANGE   "[/\\].*"

#define CF_FACILITY "LOG_USER,LOG_DAEMON,LOG_LOCAL0,LOG_LOCAL1,LOG_LOCAL2,LOG_LOCAL3,LOG_LOCAL4,LOG_LOCAL5,LOG_LOCAL6,LOG_LOCAL7"

/*************************************************************************/

struct BodySyntax
   {
   char *lval;
   enum cfdatatype dtype;
   void *range;               /* either char or struct BodySyntax **/
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
   };

/*************************************************************************/

enum fncalltype
   {
   cfn_randomint,
   cfn_getuid,
   cfn_getgid,
   cfn_execresult,
   cfn_readtcp,
   cfn_returnszero,
   cfn_isnewerthan,
   cfn_accessedbefore,
   cfn_changedbefore,
   cfn_fileexists,
   cfn_isdir,
   cfn_islink,
   cfn_isplain,
   cfn_iprange,
   cfn_hostrange,
   cfn_isvariable,
   cfn_strcmp,
   cfn_regcmp,
   cfn_isgreaterthan,
   cfn_islessthan,
   cfn_userexists,
   cfn_groupexists,
   cfn_readfile,
   cfn_readstringlist,
   cfn_readintlist,
   cfn_readreallist,
   cfn_irange,
   cfn_rrange,
   cfn_date,
   cfn_ago,
   cfn_accum,
   cfn_now,
   cfn_persiststate,
   cfn_erasestate,
   cfn_readstringarray,
   cfn_readintarray,
   cfn_readrealarray,
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

struct Promise
   {
   char *classes;
   char *agentsubtype;          /* cache the enum for this agent, -1 if undefined */
   char *ref;                   /* cache comment */
   char *promiser;
   void *promisee;              /* Can be a general rval */
   char  petype;                /* rtype of promisee - list or scalar recipient? */
   int   lineno;
   char *bundle;
   struct Audit *audit;
   struct Constraint *conlist;
   struct Promise *next;
      
    /* Runtime bus for private flags and work space */

   int    done;                 /* this needs to be preserved across runs */
   int   *donep;
   int    makeholes;
   char  *this_server;
   struct cfstat *cache;      
   struct cfagent_connection *conn;
   struct CompressedArray *inode_cache;
   dev_t rootdevice;                          /* for caching during work*/
   };

/*************************************************************************/

struct Constraint
   {
   char *lval;
   void *rval;    /* should point to either string, Rlist or FnCall */
   char type;     /* scalar, list, or function */
   char *classes; /* only used within bodies */
   int lineno;
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
   char *classes;                               /* Private context classes */
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
   cfa_binary,
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
   cfa_contentchange
   };

/*************************************************************************/
/* Runtime constraint structures                                         */
/*************************************************************************/

struct CfLock
   {
   char *last;
   char *lock;
   char *log;
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
   int  audit;
   enum cfoutputlevel report_level;
   enum cfoutputlevel log_level;
   };

/*************************************************************************/

struct DefineClasses
   {
   struct Rlist *change;
   struct Rlist *failure;
   struct Rlist *denied;
   struct Rlist *timeout;
   struct Rlist *interrupt;
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
   mode_t plus;
   mode_t minus;
   struct Rlist *owners;
   struct Rlist *groups;
   u_long plus_flags;     /* for *BSD chflags */
   u_long minus_flags;    /* for *BSD chflags */
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
   int update;
   };

/*************************************************************************/

struct FileLink
   {
   char *source;
   enum cflinktype link_type;
   struct Rlist *copy_patterns;
   enum cfnofile when_no_file;
   enum cflinkchildren link_children;
   };

/*************************************************************************/

struct Attributes
   {
   struct FileSelect select;
   struct FilePerms perms;
   struct FileCopy copy;
   struct FileDelete delete;
   struct FileRename rename;
   struct FileChange change;
   struct FileLink link;
      
   struct Recursion recursion;
   struct TransactionContext transaction;
   struct DefineClasses classes;

   char *transformer;

   int touch;
   int create;
   int move_obstructions;
   char *repository;

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
   };

#include "prototypes3.h"
