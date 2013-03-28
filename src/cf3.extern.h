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
/* File: cf3.extern.h                                                        */
/*                                                                           */
/* Created: Thu Aug  2 12:51:18 2007                                         */
/*                                                                           */
/*****************************************************************************/

#ifndef CFENGINE_CF3_EXTERN_H
#define CFENGINE_CF3_EXTERN_H

/* See variables in cf3globals.c and syntax.c */

extern struct Topic *TOPICHASH[CF_HASHTABLESIZE];
extern struct PromiseParser P;
extern int REQUIRE_COMMENTS;
extern int FIPS_MODE;
extern char POLICY_SERVER[CF_BUFSIZE];
extern int ALWAYS_VALIDATE;
extern int LICENSES;
extern int AM_NOVA;
extern int AM_CONSTELLATION;
extern char EXPIRY[32];
extern char LICENSE_COMPANY[CF_SMALLBUF];
extern int IGNORE_MISSING_INPUTS;
extern int IGNORE_MISSING_BUNDLES;
extern char WEBDRIVER[CF_MAXVARSIZE];
extern char DOCROOT[CF_MAXVARSIZE];
extern char BANNER[2*CF_BUFSIZE];
extern char FOOTER[CF_BUFSIZE];
extern char STYLESHEET[CF_BUFSIZE];
extern int CF_TOPICS;
extern int CF_OCCUR;
extern int CF_EDGES;
extern int KEYTTL;
extern struct Rlist *SERVER_KEYSEEN;
extern enum cfhashes CF_DEFAULT_DIGEST;
extern int CF_DEFAULT_DIGEST_LEN;
extern struct Item *EDIT_ANCHORS;

extern struct Bundle *BUNDLES;
extern struct Body *BODIES;
extern struct Scope *VSCOPE;
extern struct Audit *AUDITPTR;
extern struct Audit *VAUDIT; 
extern struct Rlist *VINPUTLIST;
extern struct Rlist *BODYPARTS;
extern struct Rlist *SUBBUNDLES;
extern struct Rlist *SINGLE_COPY_LIST;
extern struct Rlist *AUTO_DEFINE_LIST;
extern struct Rlist *SINGLE_COPY_CACHE;
extern struct Rlist *ACCESSLIST;
extern struct PromiseIdent *PROMISE_ID_LIST;
extern struct Item *DONELIST;
extern struct Rlist *CBUNDLESEQUENCE;
extern char *CBUNDLESEQUENCE_STR;
extern struct Item *ROTATED;
extern double FORGETRATE;
extern struct Rlist *GOALS;
extern struct Rlist *GOALCATEGORIES;

extern struct Rlist *CF_STCK;
extern int EDIT_MODEL;
extern int CF_STCKFRAME;
extern int CFA_BACKGROUND;
extern int CFA_BACKGROUND_LIMIT;
extern int AM_BACKGROUND_PROCESS;
extern int CF_PERSISTENCE;
extern int LOOKUP;
extern int BOOTSTRAP;
extern int XML;
extern FILE *FREPORT_HTML;
extern FILE *FREPORT_TXT;
extern FILE *FKNOW;
extern struct FnCallStatus FNCALL_STATUS;
extern int CSV;

extern struct SubTypeSyntax CF_NOSTYPE;
extern char *CF_DATATYPES[];
extern char *CF_AGENTTYPES[];
extern char HASHDB[CF_BUFSIZE];
extern int FSTAB_EDITS;
extern char GRAPHDIR[CF_BUFSIZE];

extern int CFA_MAXTHREADS;
extern char *THIS_BUNDLE;
extern char THIS_AGENT[CF_MAXVARSIZE];
extern enum cfagenttype THIS_AGENT_TYPE;
extern char *ARGV0;
extern int INSTALL_SKIP;
extern int SHOWREPORTS;
extern char SYSLOGHOST[CF_MAXVARSIZE];
extern unsigned short SYSLOGPORT;
extern time_t PROMISETIME;
extern time_t CF_LOCKHORIZON;
extern int ABORTBUNDLE;
extern struct Item *ABORTBUNDLEHEAP;
extern int LASTSEENEXPIREAFTER;
extern char *DEFAULT_COPYTYPE;
extern struct Rlist *SERVERLIST;
extern struct Item *PROCESSTABLE;
extern struct Item *PROCESSREFRESH;
extern struct Item *FSTABLIST;
extern struct Rlist *MOUNTEDFSLIST;

extern int CF_MOUNTALL;
extern int CF_SAVEFSTAB;

extern const char *DAY_TEXT[];
extern const char *MONTH_TEXT[];
extern const char *SHIFT_TEXT[];

#if defined(NT) && !defined(__CYGWIN__)
#  define FILE_SEPARATOR '\\'
#  define FILE_SEPARATOR_STR "\\"
# else
#  define FILE_SEPARATOR '/'
#  define FILE_SEPARATOR_STR "/"
#endif

extern char *BASIC_REPORTS[cfrep_unknown][2];
extern char *CDP_REPORTS[cdp_unknown][2];

extern char SQL_DATABASE[CF_MAXVARSIZE];
extern char SQL_OWNER[CF_MAXVARSIZE];
extern char SQL_PASSWD[CF_MAXVARSIZE];
extern char SQL_SERVER[CF_MAXVARSIZE];
extern char SQL_CONNECT_NAME[CF_MAXVARSIZE];
extern enum cfdbtype SQL_TYPE;

extern double VAL_KEPT;
extern double VAL_REPAIRED;
extern double VAL_NOTKEPT;

extern double METER_KEPT[meter_endmark];
extern double METER_REPAIRED[meter_endmark];
extern double Q_MEAN;
extern double Q_SIGMA;

/***********************************************************/
/* SYNTAX MODULES                                          */
/***********************************************************/

#ifndef CF3_MOD_COMMON
extern struct SubTypeSyntax CF_COMMON_SUBTYPES[];
extern struct BodySyntax CF_BODY_TRANSACTION[];
extern struct BodySyntax CF_VARBODY[];
extern struct BodySyntax CF_CLASSBODY[];
extern struct BodySyntax CFG_CONTROLBODY[];
extern struct BodySyntax CFH_CONTROLBODY[];
extern struct BodySyntax CFA_CONTROLBODY[];
extern struct BodySyntax CFS_CONTROLBODY[];
extern struct BodySyntax CFE_CONTROLBODY[];
extern struct BodySyntax CFR_CONTROLBODY[];
extern struct BodySyntax CFK_CONTROLBODY[];
extern struct BodySyntax CFEX_CONTROLBODY[];
extern struct BodySyntax CF_TRIGGER_BODY[];

extern struct BodySyntax CF_TRANSACTION_BODY[];
extern struct BodySyntax CF_DEFINECLASS_BODY[];
extern struct BodySyntax CF_COMMON_BODIES[];

extern struct SubTypeSyntax *CF_ALL_SUBTYPES[];
extern struct SubTypeSyntax CF_ALL_BODIES[];
#endif

#ifndef CF3_MOD_ENVIRON
extern struct BodySyntax CF_RESOURCE_BODY[];
extern struct BodySyntax CF_DESIGNATION_BODY[];
extern struct BodySyntax CF_ENVIRON_BODIES[];
extern struct SubTypeSyntax CF_ENVIRONMENT_SUBTYPES[];
#endif

#ifndef CF3_MOD_OUTPUTS
extern struct BodySyntax CF_OUTPUTS_BODIES[];
extern struct SubTypeSyntax CF_OUTPUTS_SUBTYPES[];
#endif

#ifndef CF3_MOD_FUNCTIONS
extern struct FnCallType CF_FNCALL_TYPES[];
#endif

#ifndef CF3_MOD_ACCESS
extern struct BodySyntax CF_REMACCESS_BODIES[];
extern struct SubTypeSyntax CF_REMACCESS_SUBTYPES[];
#endif

#ifndef CF_MOD_INTERFACES
extern struct BodySyntax CF_TCPIP_BODY[];
extern struct BodySyntax CF_INTERFACES_BODIES[];
extern struct SubTypeSyntax CF_INTERFACES_SUBTYPES[];
#endif

#ifndef CF3_MOD_STORAGE
extern struct BodySyntax CF_STORAGE_BODIES[];
extern struct SubTypeSyntax CF_STORAGE_SUBTYPES[];
extern struct BodySyntax CF_MOUNT_BODY[];
extern struct BodySyntax CF_CHECKVOL_BODY[];
#endif

#ifndef CF3_MOD_DATABASES
extern struct BodySyntax CF_DATABASES_BODIES[];
extern struct SubTypeSyntax CF_DATABASES_SUBTYPES[];
extern struct BodySyntax CF_SQLSERVER_BODY[];
#endif

#ifndef CF3_MOD_KNOWLEGDE
extern struct SubTypeSyntax CF_KNOWLEDGE_SUBTYPES[];
extern struct BodySyntax CF_TOPICS_BODIES[];
extern struct BodySyntax CF_OCCUR_BODIES[];
extern struct BodySyntax CF_INFER_BODIES[];
extern struct BodySyntax CF_INFERENCE_BODY[];
extern struct BodySyntax CF_RELATE_BODY[];
#endif

#ifndef CF3_MOD_PACKAGES
extern struct SubTypeSyntax CF_PACKAGES_SUBTYPES[];
extern struct BodySyntax CF_PACKAGES_BODIES[];
extern struct BodySyntax CF_EXISTS_BODY[];
#endif

#ifndef CF3_MOD_REPORT
extern struct SubTypeSyntax CF_REPORT_SUBTYPES[];
extern struct BodySyntax CF_REPORT_BODIES[];
extern struct BodySyntax CF_PRINTFILE_BODY[];
#endif


#ifndef CF3_MOD_FILES
extern struct BodySyntax CF_COMMON_EDITBODIES[];
extern struct SubTypeSyntax CF_FILES_SUBTYPES[];
extern struct BodySyntax CF_APPEND_REPL_BODIES[];
extern struct BodySyntax CF_FILES_BODIES[];
extern struct BodySyntax CF_COPYFROM_BODY[];
extern struct BodySyntax CF_LINKTO_BODY[];
extern struct BodySyntax CF_FILEFILTER_BODY[];
extern struct BodySyntax CF_CHANGEMGT_BODY[];
extern struct BodySyntax CF_TIDY_BODY[];
extern struct BodySyntax CF_RENAME_BODY[];
extern struct BodySyntax CF_RECURSION_BODY[];
#endif

#ifndef CF3_MOD_EXEC
extern struct SubTypeSyntax CF_EXEC_SUBTYPES[];
#endif

#ifndef CF3_MOD_METHODS
extern struct SubTypeSyntax CF_METHOD_SUBTYPES[];
#endif

#ifndef CF3_MOD_PROCESS
extern struct SubTypeSyntax CF_PROCESS_SUBTYPES[];
extern struct BodySyntax CF_MATCHCLASS_BODY[];
extern struct BodySyntax CF_PROCFILTER_BODY[];
extern struct BodySyntax CF_PROCESS_BODIES[];
#endif

#ifndef CF3_MOD_PROCESS
extern struct SubTypeSyntax CF_MEASUREMENT_SUBTYPES[];
extern struct BodySyntax CF_MEASURE_BODIES[];
#endif

#ifndef CF3_MOD_SERVICES
extern struct SubTypeSyntax CF_SERVICES_SUBTYPES[];
extern struct BodySyntax CF_SERVMETHOD_BODY[];
extern struct BodySyntax CF_SERVICES_BODIES[];
#endif

#endif
