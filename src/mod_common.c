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
/* File: mod_common.c                                                        */
/*                                                                           */
/* This is a root node in the syntax tree                                    */
/*                                                                           */
/*****************************************************************************/

#define CF3_MOD_COMMON

#include "cf3.defs.h"
#include "cf3.extern.h"

/*********************************************************/
/* FnCalls are lvalues in certain promise constraints    */
/*********************************************************/

/* see cf3.defs.h enum fncalltype */

struct FnCallType CF_FNCALL_TYPES[] = 
   {
   {"randomint",cf_int,2},
   {"getuid",cf_int,1},
   {"getgid",cf_int,1},
   {"execresult",cf_str,2},
   {"readtcp",cf_str,4},
   {"returnszero",cf_class,2},
   {"isnewerthan",cf_class,2},
   {"accessedbefore",cf_class,2},
   {"changedbefore",cf_class,2},
   {"fileexists",cf_class,1},
   {"isdir",cf_class,1},
   {"islink",cf_class,1},
   {"isplain",cf_class,1},
   {"iprange",cf_class,1},
   {"hostrange",cf_class,2},
   {"isvariable",cf_class,1},
   {"strcmp",cf_class,2},
   {"regcmp",cf_class,2},
   {"reglist",cf_class,2},
   {"regarray",cf_class,2},
   {"getindices",cf_slist,1},
   {"isgreaterthan",cf_class,2},
   {"islessthan",cf_class,2},
   {"userexists",cf_class,1},
   {"groupexists",cf_class,1},
   {"readfile",cf_str,2},
   {"readstringlist",cf_slist,5},
   {"readintlist",cf_ilist,5},
   {"readreallist",cf_rlist,5},
   {"irange",cf_irange,2},
   {"rrange",cf_rrange,2},
   {"on",cf_int,6},
   {"ago",cf_int,6},
   {"accumulated",cf_int,6},
   {"now",cf_int,0},
   {"readstringarray",cf_int,6},
   {"readintarray",cf_int,6},
   {"readrealarray",cf_int,6},
   {"classmatch",cf_class,1},
   {"hash",cf_str,2},
   {"usemodule",cf_class,2},
   {"selectservers",cf_int,6},
   {NULL,cf_notype}
   };

/*********************************************************/

struct BodySyntax CF_TRANSACTION_BODY[] =
   {
   {"action",cf_opts,"fix,warn,nop"},
   {"ifelapsed",cf_int,CF_VALRANGE},
   {"expireafter",cf_int,CF_VALRANGE},
   {"log_string",cf_str,""},
   {"log_level",cf_opts,"inform,verbose,error,log"},
   {"audit",cf_opts,CF_BOOL},
   {"background",cf_opts,CF_BOOL},
   {"report_level",cf_opts,"inform,verbose,error,log"},
   {NULL,cf_notype,NULL}
   };

/*********************************************************/

struct BodySyntax CF_DEFINECLASS_BODY[] =
   {
   {"promise_repaired",cf_slist,CF_IDRANGE},
   {"repair_failed",cf_slist,CF_IDRANGE},
   {"repair_denied",cf_slist,CF_IDRANGE},
   {"repair_timeout",cf_slist,CF_IDRANGE},
   {"promise_kept",cf_slist,CF_IDRANGE},
   {"persist_time",cf_int,CF_VALRANGE},
   {"timer_policy",cf_opts,"absolute,reset"},
   {NULL,cf_notype,NULL}
   };

/*********************************************************/

struct BodySyntax CF_VARBODY[] =
   {
   {"string",cf_str,""},
   {"int",cf_int,CF_INTRANGE},
   {"real",cf_real,CF_REALRANGE},
   {"slist",cf_slist,""},
   {"ilist",cf_ilist,CF_INTRANGE},
   {"rlist",cf_rlist,CF_REALRANGE},
   {"policy",cf_opts,"free,overridable,constant"},
   {NULL,cf_notype,NULL}
   };

/*********************************************************/

struct BodySyntax CF_CLASSBODY[] =
   {
   {"or",cf_clist,CF_CLASSRANGE}, 
   {"and",cf_clist,CF_CLASSRANGE},
   {"xor",cf_clist,CF_CLASSRANGE},
   {"dist",cf_rlist,CF_REALRANGE},
   {"expression",cf_class,CF_CLASSRANGE},
   {"not",cf_class,CF_CLASSRANGE},
   {NULL,cf_notype,NULL}
   };

/*********************************************************/
/* Control bodies                                        */
/*********************************************************/

struct BodySyntax CFG_CONTROLBODY[] =
   {
   {"bundlesequence",cf_slist,".*"},
   {"inputs",cf_slist,".*"},
   {"version",cf_str,"" },
   {"lastseenexpireafter",cf_int,CF_VALRANGE},
   {NULL,cf_notype,NULL}
   };

struct BodySyntax CFA_CONTROLBODY[] =
   {
   {"maxconnections",cf_int,CF_VALRANGE},
   {"abortclasses",cf_slist,".*"},
   {"abortbundleclasses",cf_slist,".*"},
   {"addclasses",cf_slist,".*"},
   {"agentaccess",cf_slist,".*"},
   {"agentfacility",cf_opts,CF_FACILITY},
   {"auditing",cf_opts,CF_BOOL},
   {"binarypaddingchar",cf_str,""},
   {"bindtointerface",cf_str,".*"},
   {"checksumpurge",cf_opts,CF_BOOL},
   {"checksumupdates",cf_opts,CF_BOOL},
   {"compresscommand",cf_str,".*"},
   {"childlibpath",cf_str,".*"},
   {"domain",cf_str,".*"},
   {"defaultcopytype",cf_opts,"mtime,atime,ctime,checksum,binary"},
   {"deletenonuserfiles",cf_opts,CF_BOOL},
   {"deletenonownerfiles",cf_opts,CF_BOOL},
   {"deletenonusermail",cf_opts,CF_BOOL},
   {"deletenonownermail",cf_opts,CF_BOOL},
   {"dryrun",cf_opts,CF_BOOL},
   {"editbinaryfilesize",cf_int,CF_VALRANGE},
   {"editfilesize",cf_int,CF_VALRANGE},
   {"emptyresolvconf",cf_opts,CF_BOOL},
   {"exclamation",cf_opts,CF_BOOL},
   {"expireafter",cf_int,CF_VALRANGE},
   {"files_single_copy",cf_slist,""},
   {"files_auto_define",cf_slist,""},
   {"fullencryption",cf_opts,CF_BOOL},
   {"hostnamekeys",cf_opts,CF_BOOL},
   {"ifelapsed",cf_int,CF_VALRANGE},
   {"inform",cf_opts,CF_BOOL},
   {"lastseen",cf_opts,CF_BOOL},
   {"lastseenexpireafter",cf_int,CF_VALRANGE},
   {"logtidyhomefiles",cf_opts,CF_BOOL},
   {"mountfilesystems",cf_opts,CF_BOOL},
   {"nonalphanumfiles",cf_opts,CF_BOOL},
   {"repchar",cf_str,"."},
   {"default_repository",cf_str,CF_PATHRANGE},
   {"secureinput",cf_opts,CF_BOOL},
   {"sensiblecount",cf_int,CF_VALRANGE},
   {"sensiblesize",cf_int,CF_VALRANGE},
   {"showactions",cf_opts,CF_BOOL},
   {"skipidentify",cf_slist,""},
   {"spooldirectories",cf_slist,CF_PATHRANGE},
   {"suspiciousnames",cf_slist,""},
   {"syslog",cf_opts,CF_BOOL},
   {"timezone",cf_str,""},
   {"default_timeout",cf_int,CF_VALRANGE},
   {"verbose",cf_opts,CF_BOOL},
   {"warnings",cf_opts,CF_BOOL},
   {"warnnonuserfiles",cf_opts,CF_BOOL},
   {"warnnonownerfiles",cf_opts,CF_BOOL},
   {"warnnonusermail",cf_opts,CF_BOOL},
   {"warnnonownermail",cf_opts,CF_BOOL},
   {NULL,cf_notype,NULL}
   };

struct BodySyntax CFS_CONTROLBODY[] =
   {
   {"cfruncommand",cf_str,CF_PATHRANGE},
   {"maxconnections",cf_int,CF_VALRANGE},
   {"denybadclocks",cf_opts,CF_BOOL},
   {"allowconnects",cf_slist,""},
   {"denyconnects",cf_slist,""},
   {"allowallconnects",cf_slist,""},
   {"trustkeysfrom",cf_slist,""},
   {"allowusers",cf_slist,""},
   {"dynamicaddresses",cf_slist,""},
   {"skipverify",cf_slist,""},
   {"logallconnections",cf_opts,CF_BOOL},
   {"logencryptedtransfers",cf_opts,CF_BOOL},
   {"hostnamekeys",cf_opts,CF_BOOL},
   {"checkident",cf_opts,CF_BOOL},
   {"auditing",cf_opts,CF_BOOL},
   {"bindtointerface",cf_str,""},
   {"serverfacility",cf_opts,CF_FACILITY},
   {NULL,cf_notype,NULL}
   };


struct BodySyntax CFM_CONTROLBODY[] =
   {
   {"threshold",cf_real,"0,1"},
   {"forgetrate",cf_real,"0,1"},
   {"monitorfacility",cf_opts,CF_FACILITY},
   {"histograms",cf_opts,CF_BOOL},
   {"tcpdump",cf_opts,CF_BOOL},
   {"tcpdumpcommand",cf_str,CF_PATHRANGE},
   {NULL,cf_notype,NULL}
   };

struct BodySyntax CFR_CONTROLBODY[] =
   {
   {"hosts",cf_slist,""},
   {"portnumber",cf_int,"1024,99999"},
   {"force_ipv4",cf_opts,CF_BOOL},
   {"trustkey",cf_opts,CF_BOOL},
   {"encrypt",cf_opts,CF_BOOL},
   {"background",cf_opts,CF_BOOL},
   {"maxchild",cf_int,CF_VALRANGE},
   {"output_to_file",cf_opts,CF_BOOL},
   {NULL,cf_notype,NULL}
   };

struct BodySyntax CFEX_CONTROLBODY[] = /* enum cfexcontrol */
   {
   {"splaytime",cf_int,CF_VALRANGE},
   {"mailfrom",cf_str,".*@.*"},
   {"mailto",cf_str,".*@.*"},
   {"smtpserver",cf_str,".*"},
   {"mailmaxlines",cf_int,"0,1000"},
   {"schedule",cf_slist,"Min.*"},
   {"executorfacility",cf_opts,CF_FACILITY},
   {"execcommand",cf_str,CF_PATHRANGE},
   {NULL,cf_notype,NULL}
   };

struct BodySyntax CFK_CONTROLBODY[] =
   {
   {"id_prefix",cf_str,".*"},
   {"build_directory",cf_str,".*"},
   {"sql_type",cf_opts,"mysql,postgress"},
   {"sql_database",cf_str,""},
   {"sql_owner",cf_str,""},
   {"sql_passwd",cf_str,""},
   {"sql_server",cf_str,""},
   {"query_output",cf_opts,"html,text"},
   {"query_engine",cf_str,""},
   {"style_sheet",cf_str,""},
   {"html_banner",cf_str,""},
   {"graph_output",cf_opts,CF_BOOL},
   {"graph_directory",cf_opts,CF_BOOL},
   {NULL,cf_notype,NULL}
   };

/*********************************************************/

/* This list is for checking free standing body lval => rval bindings */
    
struct SubTypeSyntax CF_ALL_BODIES[] =
   {
   {CF_COMMONC,"control",CFG_CONTROLBODY},
   {CF_AGENTC,"control",CFA_CONTROLBODY},
   {CF_SERVERC,"control",CFS_CONTROLBODY},
   {CF_MONITORC,"control",CFM_CONTROLBODY},
   {CF_RUNC,"control",CFR_CONTROLBODY},
   {CF_EXECC,"control",CFEX_CONTROLBODY},
   {CF_KNOWC,"control",CFK_CONTROLBODY},

   //  get others from modules e.g. "agent","files",CF_FILES_BODIES,

   {NULL,NULL,NULL}
   };



/*********************************************************/
/*                                                       */
/* Constraint values/types                               */
/*                                                       */
/*********************************************************/

 /* This is where we place lval => rval bindings that
    apply to more than one subtype, e.g. generic
    processing behavioural details */

struct BodySyntax CF_COMMON_BODIES[] =
   {
   {CF_TRANSACTION,cf_body,CF_TRANSACTION_BODY},
   {CF_DEFINECLASSES,cf_body,CF_DEFINECLASS_BODY},
   {"ifvarclass",cf_str,""},   /* execute only if expression true */
   {"comment",cf_str,""},      /* arbitrary annotation */
   {NULL,cf_notype,NULL}
   };

/*********************************************************/

 /* This is where we place promise subtypes that apply
    to more than one type of bundle, e.g. agent,server.. */

struct SubTypeSyntax CF_COMMON_SUBTYPES[] =
     {
     {"*","vars",CF_VARBODY},
     {"*","classes",CF_CLASSBODY},
     {"*","reports",CF_REPORT_BODIES},
     {"agent","*",CF_COMMON_BODIES},
     {"edit_line","*",CF_COMMON_EDITBODIES},
     {NULL,NULL,NULL}
     };

/*********************************************************/
/* THIS IS WHERE TO ATTACH SYNTAX MODULES                */
/*********************************************************/

/* Read in all parsable Bundle definitions */
/* REMEMBER TO REGISTER THESE IN cf3.extern.h */

struct SubTypeSyntax *CF_ALL_SUBTYPES[CF3_MODULES] =
   {
   CF_COMMON_SUBTYPES,     /* Add modules after this, mod_report.c is here */
   CF_FILES_SUBTYPES,      /* mod_files.c */
   CF_EXEC_SUBTYPES,       /* mod_exec.c */
   CF_PROCESS_SUBTYPES,    /* mod_process.c */
   CF_REMACCESS_SUBTYPES,  /* mod_access.c */
   CF_STORAGE_SUBTYPES,    /* mod_storage.c */
   CF_METHOD_SUBTYPES,     /* mod_methods.c */
   CF_INTERFACES_SUBTYPES, /* mod_interfaces.c */
   CF_PACKAGES_SUBTYPES,   /* mod_packages.c */
   CF_KNOWLEDGE_SUBTYPES,  /* mod_knowledge.c */
   
   /* update CF3_MODULES in cf3.defs.h */
   };
