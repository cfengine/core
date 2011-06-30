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
   {"check_foreign",cf_opts,CF_BOOL,"true/false verify storage that is mounted from a foreign system on this host"},
   {"freespace",cf_str,"[0-9]+[MBkKgGmb%]","Absolute or percentage minimum disk space that should be available before warning"},
   {"sensible_size",cf_int,CF_VALRANGE,"Minimum size in bytes that should be used on a sensible-looking storage device"},
   {"sensible_count",cf_int,CF_VALRANGE,"Minimum number of files that should be defined on a sensible-looking storage device"},
   {"scan_arrivals",cf_opts,CF_BOOL,"true/false generate pseudo-periodic disk change arrival distribution"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_MOUNT_BODY[] =
   {
   {"edit_fstab",cf_opts,CF_BOOL,"true/false add or remove entries to the file system table (\"fstab\")"},
   {"mount_type",cf_opts,"nfs,nfs2,nfs3,nfs4","Protocol type of remote file system"},
   {"mount_source",cf_str,CF_ABSPATHRANGE,"Path of remote file system to mount"},
   {"mount_server",cf_str,"","Hostname or IP or remote file system server"},
   {"mount_options",cf_slist,"","List of option strings to add to the file system table (\"fstab\")"},
   {"unmount",cf_opts,CF_BOOL,"true/false unmount a previously mounted filesystem"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/

struct BodySyntax CF_STORAGE_BODIES[] =
   {
   {"mount",cf_body,CF_MOUNT_BODY,"Criteria for mounting foreign file systems"},
   {"volume",cf_body,CF_CHECKVOL_BODY,"Criteria for monitoring/probing mounted volumes"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_STORAGE_SUBTYPES[] =
  {
  {"agent","storage",CF_STORAGE_BODIES},
  {NULL,NULL,NULL},
  };

