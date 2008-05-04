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

struct BodySyntax CF_REPLACE_BODIES[] =
   {
   {"with",cf_str,CF_ANYSTRING},
   {"which",cf_str,CF_ANYSTRING},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_CHANGEMGT_BODY[] =
   {
   {"hash",cf_opts,"md5,sha1"},
   {"update",cf_opts,CF_BOOL},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_TIDY_BODY[] =
   {
   {"age",cf_irange,"0,inf"},
   {"size",cf_irange,"0,inf"},
   {"age_type",cf_opts, "mtime,ctime,mtime,atime"},
   {"dirlinks",cf_opts,"delete,keep,tidy"},
   {"rmdirs",cf_opts,"yes,no,true,false,sub"},
   {"links",cf_opts,"stop,keep,traverse,tidy"},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_RENAME_BODY[] =
   {
   {"newname",cf_str,"filename"},
   {"type",cf_opts,"plain,file,link"},
   {"rotate",cf_int,"0,99"},
   {"size",cf_irange,"0,inf"},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_APPEND_BODIES[] =
   {
   {"data",cf_str,CF_ANYSTRING},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_ACCESS_BODIES[] =
   {
   {"mode",cf_str,"[0-7ugorwx,+-]*"},
   {"owner",cf_slist,".*"},
   {"group",cf_slist,".*"},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_FILEFILTER_BODY[] =
   {
   {"name",cf_slist,""},
   {"path",cf_slist,CF_PATHRANGE},
   {"mode",cf_str,""},
   {"size",cf_irange,""},
   {"owner",cf_slist,""},
   {"group",cf_slist,""},
   {"ctime",cf_irange,CF_TIMERANGE},
   {"mtime",cf_irange,CF_TIMERANGE},
   {"atime",cf_irange,CF_TIMERANGE},
   {"exec_regex",cf_str,""},
   {"filetypes",cf_olist,"reg,link,dir,socket,fifo,door,char,block"},
   {"issymlinkto",cf_slist,""},
   {"filetype",cf_str,"[(plain|link|dir|socket|fifo|door|char|block)[|]*]*"},
   {"result",cf_str,"[(name|path|filetype|mode|size|owner|group|atime|ctime|mtime|issymlinkto|exec_regex)[|&!.]*]*"},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

/* Copy and link are really the same body and should have
   non-overlapping patterns so that they are XOR */
       

struct BodySyntax CF_LINKTO_BODY[] =
   {
   {"link_type",cf_opts,"symbolic,absolute,abs,hard,relative,rel"},
   {"copy_patterns",cf_str,""},
   {"deadlinks",cf_opts,"kill,force"},
   {"when_no_file",cf_opts,"force,kill"},
   {NULL,cf_notype,NULL}
   };

/**************************************************************/

struct BodySyntax CF_COPYFROM_BODY[] =
   {
   {"source",cf_str,""},
   {"servers",cf_slist,""},
   {"action",cf_str,""},
   {"backup",cf_str,""},
   {"repository",cf_str,CF_PATHRANGE},
   {"stealth",cf_opts,CF_BOOL},
   {"preserve",cf_opts,CF_BOOL},
   {"linkpattern",cf_str,""},
   {"xdev",cf_opts,CF_BOOL},
   {"compare",cf_opts,"atime,mtime,ctime,checksum"},
   {"linktype",cf_opts,"absolute,relative,hard"},
   {"typecheck",cf_opts,CF_BOOL},
   {"forceupdate",cf_opts,CF_BOOL},
   {"forcedirs",cf_opts,CF_BOOL},
   {"forceipv4",cf_opts,CF_BOOL},
   {"size",cf_int,"0,inf"},
   {"trigger",cf_slist,""},
   {"trustkey",cf_opts,CF_BOOL},
   {"encrypt",cf_opts,CF_BOOL},
   {"verify",cf_opts,CF_BOOL},
   {"purge",cf_opts,CF_BOOL},
   {"findertype",cf_opts,"MacOSX"},
   {NULL,cf_notype,NULL}
   };

/***************************************************************/

/* This is the primary set of constraints for a file object */

struct BodySyntax CF_FILES_BODIES[] =
   {
   {"file_select",cf_body,CF_FILEFILTER_BODY},
   {"copyfrom",cf_body,CF_COPYFROM_BODY},
   {"linkto",cf_body,CF_LINKTO_BODY},
   {"perms",cf_body,CF_ACCESS_BODIES},
   {"changes",cf_body,CF_CHANGEMGT_BODY},
   {"delete",cf_body,CF_TIDY_BODY},
   {"rename",cf_body,CF_RENAME_BODY},
   {"repository",cf_str,"/.*"},
   {"edit_line",cf_body,CF_BUNDLE},
   {"edit_xml",cf_body,CF_BUNDLE},
   {"acl",cf_body,NULL},
   {"recurse",cf_int,"0,inf"},
   {"touch",cf_opts,CF_BOOL},
   {"create",cf_opts,CF_BOOL},
   {"action",cf_opts,"fix,warn"},
   {"pathtype",cf_opts,"literal,regex"},
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
     
  {"edit_line","append",CF_APPEND_BODIES},
  {"edit_line","replace",CF_REPLACE_BODIES},
  {NULL,NULL,NULL},
  };

