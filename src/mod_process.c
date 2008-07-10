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
   {"match_range",cf_irange,CF_VALRANGE},
   {"in_range_define",cf_slist,""},
   {"out_of_range_define",cf_slist,""},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/

struct BodySyntax CF_PROCFILTER_BODY[] =
   {
   {"process_owner",cf_slist,""},
   {"pid",cf_irange,CF_VALRANGE},
   {"ppid",cf_irange,CF_VALRANGE},
   {"pgid",cf_irange,CF_VALRANGE},
   {"rsize",cf_irange,CF_VALRANGE},
   {"vsize",cf_irange,CF_VALRANGE},
   {"status",cf_str,""},
   {"ttime_range",cf_irange,CF_TIMERANGE},
   {"stime_range",cf_irange,CF_TIMERANGE},
   {"command",cf_str,""},
   {"tty",cf_str,""},
   {"priority",cf_irange,"-20,+20"},
   {"threads",cf_irange,CF_VALRANGE},
   {"process_result",cf_str,"[(process_owner|pid|ppid||pgid|rsize|vsize|status|command|ttime|stime|tty|priority|threads)[|&!.]*]*"},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/

/* This is the primary set of constraints for an exec object */

struct BodySyntax CF_PROCESS_BODIES[] =
   {
   {"signals",cf_olist,CF_SIGNALRANGE},
   {"process_stop",cf_str,CF_PATHRANGE},
   {"process_count",cf_body,CF_MATCHCLASS_BODY},
   {"process_select",cf_body,CF_PROCFILTER_BODY},
   {"restart_class",cf_str,CF_IDRANGE},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_PROCESS_SUBTYPES[] =
  {
  {"agent","processes",CF_PROCESS_BODIES},
  {NULL,NULL,NULL},
  };

