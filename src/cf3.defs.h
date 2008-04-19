/*****************************************************************************/
/*                                                                           */
/* File: cf3.defs.h                                                          */
/*                                                                           */
/* Created: Thu Aug  2 11:09:47 2007                                         */
/*                                                                           */
/* Author:                                           >                       */
/*                                                                           */
/* Revision: $Id$                                                            */
/*                                                                           */
/* Description:                                                              */
/*                                                                           */
/*****************************************************************************/

/* These files are hard links to the cfengine 2 sources */

#include "cf.defs.h"
#include "cf.extern.h"

#undef VERSION

#include "conf.h"

/*************************************************************************/
/* Fundamental (meta) types                                              */
/*************************************************************************/

#define CF_SCALAR 's'
#define CF_LIST   'l'
#define CF_FNCALL 'f'
#define CF_STACK  'k'

#define CF_NOPROMISEE 'X'

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

enum cfexcontrol
   {
   cfex_mailfrom,
   cfex_mailto,
   cfex_smtpserver,
   cfex_mailmaxlines,
   cfex_schedule,
   cfex_notype,
   };

/*************************************************************************/
/* Syntax module range/pattern constants for type validation             */
/*************************************************************************/

#define CF_BOOL "true,false,yes,no,on,off"
#define CF_BUNDLE  (void*)1234           /* any non-null value, not used */

#define CF_HIGHINIT 99999
#define CF_LOWINIT -999999

#define CF_VALRANGE  "0,999999"
#define CF_INTRANGE  "-9999999,999999"
#define CF_REALRANGE "-9.99999E100,9.99999E100"

#define CF_CLASSRANGE "[a-zA-Z0-9_!&|.()]+"
#define CF_IDRANGE    "[a-zA-Z0-9_]+"
#define CF_FNCALLRANGE "[a-zA-Z0-9_().$@]+"
#define CF_ANYSTRING  ".*"
#define CF_PATHRANGE  "[/\\].*"
#define CF_TIMERANGE "0,4026531839"


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
   cfn_readstringlist,
   cfn_readintlist,
   cfn_readreallist,
   cfn_irange,
   cfn_rrange,
   cfn_date,
   cfn_ago,
   cfn_now,
   cfn_persiststate,
   cfn_erasestate,
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
   char *promiser;
   void *promisee;              /* Can be a general rval */
   char  petype;                /* rtype of promisee - list or scalar recipient? */
   int   lineno;
   char *bundle;
   char  done;
   struct Audit *audit;
   struct Constraint *conlist;
   struct Promise *next;
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
/* Output options                                                  */
/*******************************************************************/

struct CfOutput
   {
   FILE *stream;
   //graphfile
   // graph context - or adjacency matrix?
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

#include "prototypes3.h"
