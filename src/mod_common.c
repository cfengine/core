/* 

        Copyright (C) 1994-
        Free Software Foundation, Inc.

   This file is part of GNU cfengine - written and maintained 
   by Mark Burgess, Dept of Computing and Engineering, Oslo College,
   Dept. of Theoretical physics, University of Oslo
 
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
   {"isgreaterthan",cf_class,2},
   {"islessthan",cf_class,2},
   {"userexists",cf_class,1},
   {"groupexists",cf_class,1},
   {"readstringlist",cf_slist,4},
   {"readintlist",cf_ilist,4},
   {"readreallist",cf_rlist,4},
   {"irange",cf_irange,2},
   {"rrange",cf_rrange,2},
   {"ondate",cf_int,6},
   {"ago",cf_int,6},
   {"now",cf_int,0},
   {"persiststate",cf_class,3},
   {"erasestate",cf_class,1},
   {NULL,cf_notype}
   };

/*********************************************************/

struct BodySyntax CF_TRANSACTION_BODY[] =
   {
   {"action",cf_opts,"fix,warn,nop"},
   {"ifelapsed",cf_int,CF_VALRANGE},
   {"expireafter",cf_int,CF_VALRANGE},
   {"logstring",cf_str,""},
   {"loglevel",cf_str,""},
   {"audit",cf_opts,CF_BOOL},
   {"reportlevel",cf_opts,"inform,verbose,debug,logonly"},
   {NULL,cf_notype,NULL}
   };

/*********************************************************/

struct BodySyntax CF_DEFINECLASS_BODY[] =
   {
   {"success",cf_slist,CF_IDRANGE},
   {"reportsuccess",cf_str,CF_ANYSTRING},
   {"failure",cf_slist,CF_IDRANGE},
   {"reportfailure",cf_str,CF_ANYSTRING},
   {"alert",cf_slist,CF_IDRANGE},
   {"reportalert",cf_str,CF_ANYSTRING},
   {NULL,cf_notype,NULL}
   };

/*********************************************************/

struct BodySyntax CF_TRIGGER_BODY[] =
   {
   {"followup",cf_slist,CF_FNCALLRANGE},
   {"cleanup",cf_slist,CF_FNCALLRANGE},
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
   {"policy",cf_str,CF_IDRANGE},
   {"dist",cf_rlist,CF_REALRANGE},
   {"expression",cf_str,CF_CLASSRANGE},
   {NULL,cf_notype,NULL}
   };

/*********************************************************/

struct BodySyntax CF_REPORTBODY[] =
   {
   {NULL,cf_notype,NULL}
   };

/***************************************************************/

struct BodySyntax CF_SELECT_BODY[] =
  {
  {"include",cf_slist,".*"},
  {"exclude",cf_slist,".*"},
  {NULL,cf_notype,NULL}
  };

/*********************************************************/
/* Control bodies                                        */
/*********************************************************/

struct BodySyntax CFG_CONTROLBODY[] =
   {
   {"bundlesequence",cf_slist,".*"},
   {"inputs",cf_slist,".*"},
   {NULL,cf_notype,NULL}
   };

struct BodySyntax CFA_CONTROLBODY[] =
   {
   {"splaytime",cf_int,".*"},
   {"access",cf_slist,".*"},
   {NULL,cf_notype,NULL}
   };

struct BodySyntax CFS_CONTROLBODY[] =
   {
   {NULL,cf_notype,NULL}
   };


struct BodySyntax CFM_CONTROLBODY[] =
   {
   {"emailfrom",cf_str,".*"},
   {"emailto",cf_str,".*"},
   {"threshold",cf_real,"0,1"},
   {"forgetrate",cf_real,"0,1"},
   {NULL,cf_notype,NULL}
   };

struct BodySyntax CFR_CONTROLBODY[] =
   {
   {NULL,cf_notype,NULL}
   };

struct BodySyntax CFEX_CONTROLBODY[] =
   {
   {"schedule",cf_ilist,".*"},
   {NULL,cf_notype,NULL}
   };


/*********************************************************/

/* This list is for checking free standing body lval => rval bindings */
    
struct SubTypeSyntax CF_ALL_BODIES[] =
   {
   {"common","control",CFG_CONTROLBODY},
   {"agent","control",CFA_CONTROLBODY},
   {"server","control",CFS_CONTROLBODY},
   {"monitor","control",CFM_CONTROLBODY},

   //  get others from modules e.g. "agent","files",CF_FILES_BODIES,

   {NULL,NULL,NULL}
   };


/* REMEMBER TO REGISTER THESE IN cf3.extern.h */

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
   {"trigger",cf_body,CF_TRIGGER_BODY},
   {"name_select",cf_body,CF_SELECT_BODY},
   {NULL,cf_notype,NULL}
   };

/*********************************************************/

 /* This is where we place promise subtypes that apply
    to more than one type of bundle, e.g. agent,server.. */

struct SubTypeSyntax CF_COMMON_SUBTYPES[] =
     {
     {"*","vars",CF_VARBODY},
     {"*","classes",CF_CLASSBODY},
     {"*","report",CF_REPORTBODY},
     {"agent","*",CF_COMMON_BODIES},
     {NULL,NULL,NULL}
     };

/*********************************************************/
/* THIS IS WHERE TO ATTACH SYNTAX MODULES                */
/*********************************************************/

/* Read in all parsable Bundle definitions */

struct SubTypeSyntax *CF_ALL_SUBTYPES[CF3_MODULES] =
   {
   CF_COMMON_SUBTYPES, /* Add modules after this */
   CF_FILES_SUBTYPES,  /* mod_files.c */
   CF_EXEC_SUBTYPES,   /* mod_exec.c */
   CF_PROCESS_SUBTYPES,/* mod_process.c */
   
   /* update CF3_MODULES in cf3.defs.h */
   };
