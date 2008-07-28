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
/* File: mod_storage.c                                                       */
/*                                                                           */
/*****************************************************************************/

/*

 This file can act as a template for adding functionality to cfengine 3.
 All functionality can be added by extending the main array

 CF_MOD_SUBTYPES[CF3_MODULES]

 and its array dimension, in mod_common, in the manner shown here.
 
*/

#define CF3_MOD_STORAGE

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

struct BodySyntax CF_CHECKVOL_BODY[] =
   {
   {"check_foreign",cf_opts,CF_BOOL},
   {"freespace",cf_str,"[0-9]+[mb%]"},
   {"sensible_size",cf_int,CF_VALRANGE},
   {"sensible_count",cf_int,CF_VALRANGE},
   {"scan_arrivals",cf_opts,CF_BOOL},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_MOUNT_BODY[] =
   {
   {"mount_fs",cf_opts,"nfs"},
   {"mount_point",cf_str,CF_PATHRANGE},
   {"mount_server",cf_str,""},
   {"mount_options",cf_slist,""},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/

/* This is the primary set of constraints for a file object */

struct BodySyntax CF_STORAGE_BODIES[] =
   {
   {"mount",cf_body,CF_MOUNT_BODY},
   {"volume",cf_body,CF_CHECKVOL_BODY},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_STORAGE_SUBTYPES[] =
  {
  {"agent","storage",CF_STORAGE_BODIES},
  {NULL,NULL,NULL},
  };

