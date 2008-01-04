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
/* File: mod_process.c                                                       */
/*                                                                           */
/*****************************************************************************/

/*

 This file can act as a template for adding functionality to cfengine 3.
 All functionality can be added by extending the main array

 CF_MOD_SUBTYPES[CF3_MODULES]

 and its array dimension, in mod_common, in the manner shown here.
 
*/

#define CF3_MOD_PROCESS

#include "cf3.defs.h"
#include "cf3.extern.h"

 /***********************************************************/
 /* Read this module file backwards, as dependencies have   */
 /* to be defined first - these arrays declare pairs of     */
 /* constraints                                             */
 /*                                                         */
 /* lval => rval                                            */
 /*                                                         */
 /* in the form (lval,type,range)                           */
 /*                                                         */
 /* If the type is cf_body then the range is a pointer      */
 /* to another array of pairs, like in a body "sub-routine" */
 /*                                                         */
 /***********************************************************/

struct BodySyntax CF_MATCHCLASS_BODY[] =
   {
   {"matches",cf_int,""},
   {"in_range_define",cf_slist,""},
   {"out_of_range_define",cf_slist,""},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/

struct BodySyntax CF_PROCFILTER_BODY[] =
   {
   {"owner",cf_slist,""},
   {"pid",cf_str,""},
   {"ppid",cf_str,""},
   {"pgid",cf_str,""},
   {"rsize",cf_str,""},
   {"vsize",cf_str,""},
   {"status",cf_str,""},
   {"ttime",cf_str,""},
   {"stime",cf_str,""},
   {"command",cf_str,""},
   {"tty",cf_str,""},
   {"priority",cf_str,""},
   {"threads",cf_str,""},
   {"result",cf_opts,"owner,pid,ppid,pgid,rsize,vsize,status,command,ttime,stime,tty,priority,threads"},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/

/* This is the primary set of constraints for an exec object */

struct BodySyntax CF_PROCESS_BODIES[] =
   {
   {"signals",cf_olist,"hup,int,trap,kill,pipe,cont,abrt,stop,quit,term,child,usr1,usr2,bus,segv"},
   {"number",cf_body,CF_MATCHCLASS_BODY},
   {"procfilter",cf_body,CF_PROCFILTER_BODY},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_PROCESS_SUBTYPES[] =
  {
  {"Agent","processes",CF_PROCESS_BODIES},
  };

