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
/* File: mod_files.c                                                         */
/*                                                                           */
/*****************************************************************************/

/*

 This file can act as a template for adding functionality to cfengine 3.
 All functionality can be added by extending the main array

 CF_MOD_SUBTYPES[CF3_MODULES]

 and its array dimension, in mod_common, in the manner shown here.
 
*/

#define CF3_MOD_FILES

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

/**************************************************************/
/* editing                                                    */
/**************************************************************/

struct BodySyntax CF_LOCATION_BODY[] =
   {
   {"line_location",cf_opts,"start,end"},
   {"after_line_matching",cf_opts,CF_ANYSTRING},
   {"before_line_matching",cf_opts,CF_ANYSTRING},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_EDITCOL_BODY[] =
   {
   {"column_separator",cf_str,CF_ANYSTRING},
   {"select_column",cf_int,CF_VALRANGE},
   {"value_separator",cf_str,CF_CHARRANGE},
   {"column_value",cf_str,CF_ANYSTRING},
   {"append_value",cf_str,CF_ANYSTRING},
   {"prepend_value",cf_str,CF_ANYSTRING},
   {"insert_value_after",cf_str,CF_ANYSTRING},
   {"insert_value_alphanum",cf_str,CF_ANYSTRING},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_REPLACEWITH_BODY[] =
   {
   {"replace_value",cf_str,CF_ANYSTRING},
   {"occurrences",cf_opts,"all,first,last"},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/
/**************************************************************/

struct BodySyntax CF_INSERTLINES_BODIES[] =
   {
   {"location",cf_body,CF_LOCATION_BODY},
   {"source_type",cf_opts,"literal,string,file"},
   {"expand_vars",cf_opts,CF_BOOL},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_DELETELINES_BODIES[] =
   {
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_COLUMN_BODIES[] =
   {
   {"edit_column",cf_body,CF_EDITCOL_BODY},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_REPLACE_BODIES[] =
   {
   {"replace_with",cf_body,CF_REPLACEWITH_BODY},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/
/* Main files                                                 */
/**************************************************************/

struct BodySyntax CF_ACL_BODY[] =
   {
   {"tobedecided",cf_str,CF_ANYSTRING},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_CHANGEMGT_BODY[] =
   {
   {"hash",cf_opts,"md5,sha1"},
   {"report_changes",cf_opts,"content,none"},
   {"update",cf_opts,CF_BOOL},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_RECURSION_BODY[] =
   {
   {"include_dirs",cf_slist,".*"},
   {"exclude_dirs",cf_slist,".*"},
   {"include_basedir",cf_opts,CF_BOOL},
   {"depth",cf_int,CF_VALRANGE},
   {"xdev",cf_opts,CF_BOOL},
   {"traverse_links",cf_opts,CF_BOOL},
   {"rmdeadlinks",cf_opts,CF_BOOL},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_EDITS_BODY[] =
   {
   {"edit_backup",cf_opts,"true,false,timestamp,rotate"},
   {"max_file_size",cf_int,CF_VALRANGE},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_TIDY_BODY[] =
   {
   {"dirlinks",cf_opts,"delete,tidy,keep"},
   {"rmdirs",cf_opts,CF_BOOL},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_RENAME_BODY[] =
   {
   {"newname",cf_str,""},
   {"disable_suffix",cf_str,""},
   {"disable",cf_opts,CF_BOOL},
   {"rotate",cf_int,"0,99"},
   {"rename_perms",cf_str,CF_MODERANGE},
   {NULL,cf_notype,NULL}
   };


/**************************************************************/

struct BodySyntax CF_ACCESS_BODIES[] =
   {
   {"mode",cf_str,CF_MODERANGE},
   {"owners",cf_slist,CF_IDRANGE},
   {"groups",cf_slist,CF_IDRANGE},
   {"rxdirs",cf_opts,CF_BOOL},
   {"bsdflags",cf_olist,"arch,archived,dump,opaque,sappnd,sappend,schg,schange,simmutable,sunlnk,sunlink,uappnd,uappend,uchg,uchange,uimmutable,uunlnk,uunlink"},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_FILEFILTER_BODY[] =
   {
   {"leaf_name",cf_slist,""},
   {"path_name",cf_slist,CF_PATHRANGE},
   {"search_mode",cf_str,CF_MODERANGE},
   {"search_size",cf_irange,"0,inf"},
   {"search_owners",cf_slist,CF_IDRANGE},
   {"search_groups",cf_slist,CF_IDRANGE},
   {"ctime",cf_irange,CF_TIMERANGE},
   {"mtime",cf_irange,CF_TIMERANGE},
   {"atime",cf_irange,CF_TIMERANGE},
   {"exec_regex",cf_str,""},
   {"exec_program",cf_str,""},
   {"filetypes",cf_olist,"plain,reg,symlink,dir,socket,fifo,door,char,block"},
   {"issymlinkto",cf_slist,""},
   {"file_result",cf_str,"[(leaf_name|path_name|file_types|mode|size|owner|group|atime|ctime|mtime|issymlinkto|exec_regex|exec_program)[|&!.]*]*"},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

/* Copy and link are really the same body and should have
   non-overlapping patterns so that they are XOR but it's
   okay that some names overlap (like source) as there is
   no ambiguity in XOR */

struct BodySyntax CF_LINKTO_BODY[] =
   {
   {"source",cf_str,""},
   {"link_type",cf_opts,CF_LINKRANGE},
   {"copy_patterns",cf_str,""},
   {"when_no_file",cf_opts,"force,delete,nop"},
   {"link_children",cf_opts,CF_BOOL},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_COPYFROM_BODY[] =
   {
   {"source",cf_str,""},
   {"servers",cf_slist,""},
   {"portnumber",cf_int,"1024,99999"},
   {"copy_backup",cf_opts,"true,false,timestamp"},
   {"stealth",cf_opts,CF_BOOL},
   {"preserve",cf_opts,CF_BOOL},
   {"linkcopy_patterns",cf_slist,""},
   {"copylink_patterns",cf_slist,""},
   {"compare",cf_opts,"atime,mtime,ctime,checksum"},
   {"link_type",cf_opts,CF_LINKRANGE},
   {"type_check",cf_opts,CF_BOOL},
   {"force_update",cf_opts,CF_BOOL},
   {"force_ipv4",cf_opts,CF_BOOL},
   {"copy_size",cf_irange,"0,inf"},
   {"trustkey",cf_opts,CF_BOOL},
   {"encrypt",cf_opts,CF_BOOL},
   {"verify",cf_opts,CF_BOOL},
   {"purge",cf_opts,CF_BOOL},
   {"check_root",cf_opts,CF_BOOL},
   {"findertype",cf_opts,"MacOSX"},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/

/* This is the primary set of constraints for a file object */

struct BodySyntax CF_FILES_BODIES[] =
   {
   {"file_select",cf_body,CF_FILEFILTER_BODY},
   {"copy_from",cf_body,CF_COPYFROM_BODY},
   {"link_from",cf_body,CF_LINKTO_BODY},
   {"perms",cf_body,CF_ACCESS_BODIES},
   {"changes",cf_body,CF_CHANGEMGT_BODY},
   {"delete",cf_body,CF_TIDY_BODY},
   {"rename",cf_body,CF_RENAME_BODY},
   {"repository",cf_str,CF_PATHRANGE},
   {"edit_line",cf_bundle,CF_BUNDLE},
   {"edit_xml",cf_bundle,CF_BUNDLE},
   {"edit_defaults",cf_body,CF_EDITS_BODY},
   {"depth_search",cf_body,CF_RECURSION_BODY},
   {"touch",cf_opts,CF_BOOL},
   {"create",cf_opts,CF_BOOL},
   {"move_obstructions",cf_opts,CF_BOOL},
   {"transformer",cf_str,CF_PATHRANGE},
   {"pathtype",cf_opts,"literal,regex"},
   {"acl",cf_body,CF_ACL_BODY},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_FILES_SUBTYPES[] =
  {
  /* Body lists belonging to "files:" type in Agent */
      
  {"agent","files",CF_FILES_BODIES},

  /* Body lists belonging to th edit_line sub-bundle of files: */
     
  {"edit_line","insert_lines",CF_INSERTLINES_BODIES},
  {"edit_line","column_edits",CF_COLUMN_BODIES},
  {"edit_line","replace_patterns",CF_REPLACE_BODIES},
  {"edit_line","delete_lines",CF_DELETELINES_BODIES},
  {NULL,NULL,NULL},
  };

