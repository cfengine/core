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
   {"before_after",cf_opts,"before,after","Menu option, point cursor before of after matched line"},
   {"first_last",cf_opts,"first,last","Menu option, choose first or last occurrence of match in file"},
   {"select_line_matching",cf_str,CF_ANYSTRING,"Regular expression for matching file line location"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_EDITCOL_BODY[] =
   {
   {"allow_blank_fields",cf_opts,CF_BOOL,"true/false allow blank fields in a line (do not purge)"},
   {"extend_fields",cf_opts,CF_BOOL,"true/false add new fields at end of line if necessary to complete edit"},
   {"field_operation",cf_opts,"prepend,append,alphanum,delete,set","Menu option policy for editing subfields"},
   {"field_separator",cf_str,CF_ANYSTRING,"The regular expression used to separate fields in a line"},
   {"field_value",cf_str,CF_ANYSTRING,"Set field value to a fixed value"},
   {"select_field",cf_int,CF_VALRANGE,"Integer index of the field required 1..n"},
   {"value_separator",cf_str,CF_CHARRANGE,"Character separator for subfields inside the selected field"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_REPLACEWITH_BODY[] =
   {
   {"occurrences",cf_opts,"all,first","Menu option to replace all occurrences or just first (NB the latter is non-convergent)"},
   {"replace_value",cf_str,CF_ANYSTRING,"Value used to replace regular expression matches in search"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_EDSCOPE_BODY[] =
   {
   {"select_start",cf_str,CF_ANYSTRING,"Regular expression matching start of edit region"},
   {"select_end",cf_str,CF_ANYSTRING,"Regular expression matches end of edit region from start"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_DELETESELECT_BODY[] =
   {
   {"delete_if_startwith_from_list",cf_slist,CF_ANYSTRING,"Delete line if it starts with a string in the list"},
   {"delete_if_not_startwith_from_list",cf_slist,CF_ANYSTRING,"Delete line if it DOES NOT start with a string in the list"},
   {"delete_if_match_from_list",cf_slist,CF_ANYSTRING,"Delete line if it fully matches a regex in the list"},
   {"delete_if_not_match_from_list",cf_slist,CF_ANYSTRING,"Delete line if it DOES NOT fully match a regex in the list"},
   {"delete_if_contains_from_list",cf_slist,CF_ANYSTRING,"Delete line if a regex in the list match a line fragment"},
   {"delete_if_not_contains_from_list",cf_slist,CF_ANYSTRING,"Delete line if a regex in the list DOES NOT match a line fragment"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_INSERTSELECT_BODY[] =
   {
   {"insert_if_startwith_from_list",cf_slist,CF_ANYSTRING,"Insert line if it starts with a string in the list"},
   {"insert_if_not_startwith_from_list",cf_slist,CF_ANYSTRING,"Insert line if it DOES NOT start with a string in the list"},
   {"insert_if_match_from_list",cf_slist,CF_ANYSTRING,"Insert line if it fully matches a regex in the list"},
   {"insert_if_not_match_from_list",cf_slist,CF_ANYSTRING,"Insert line if it DOES NOT fully match a regex in the list"},
   {"insert_if_contains_from_list",cf_slist,CF_ANYSTRING,"Insert line if a regex in the list match a line fragment"},
   {"insert_if_not_contains_from_list",cf_slist,CF_ANYSTRING,"Insert line if a regex in the list DOES NOT match a line fragment"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_INSERTLINES_BODIES[] =
   {
   {"expand_scalars",cf_opts,CF_BOOL,"Expand any unexpanded variables"},
   {"insert_type",cf_opts,"literal,string,file","Type of object the promiser string refers to (default literal)"},
   {"insert_select",cf_body,CF_INSERTSELECT_BODY,"Insert only if lines pass filter criteria"},
   {"location",cf_body,CF_LOCATION_BODY,"Specify where in a file an insertion will be made"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_DELETELINES_BODIES[] =
   {
   {"delete_select",cf_body,CF_DELETESELECT_BODY,"Delete only if lines pass filter criteria"},
   {"not_matching",cf_opts,CF_BOOL,"true/false negate match criterion"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_COLUMN_BODIES[] =
   {
   {"edit_field",cf_body,CF_EDITCOL_BODY,"Edit line-based file as matrix of fields"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_REPLACE_BODIES[] =
   {
   {"replace_with",cf_body,CF_REPLACEWITH_BODY,"Search-replace pattern"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/
/* Common to all edit_line promises                           */
/**************************************************************/

struct BodySyntax CF_COMMON_EDITBODIES[] =
   {
   {"select_region",cf_body,CF_EDSCOPE_BODY,"Limit edits to a demarked region of the file"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/
/* Main files                                                 */
/**************************************************************/

struct BodySyntax CF_ACL_BODY[] =
   {
   {"aces",cf_slist,"((user|group):[^:]+:[-=+,rwx()]*(:(allow|deny))?)|((all|mask):[-=+,rwx()]*(:(allow|deny))?)","Native settings for access control entry"},
   {"acl_directory_inherit",cf_opts,"parent,specify,none","Access control list type for the affected file system"},
   {"acl_method",cf_opts,"append,overwrite","Editing method for access control list"},
   {"acl_type",cf_opts,"generic,posix,ntfs","Access control list type for the affected file system"},
   {"specify_inherit_aces",cf_slist,"((user|group):[^:]+:[-=+,rwx()]*(:(allow|deny))?)|((all|mask):[-=+,rwx()]*(:(allow|deny))?)","Native settings for access control entry"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_CHANGEMGT_BODY[] =
   {
   {"hash",cf_opts,"md5,sha1,best","Hash files for change detection"},
   {"report_changes",cf_opts,"all,stats,content,none","Specify criteria for change warnings"},
   {"update_hashes",cf_opts,CF_BOOL,"Update hash values immediately after change warning"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_RECURSION_BODY[] =
   {
   {"depth",cf_int,CF_VALRANGE,"Maximum depth level for search"},
   {"exclude_dirs",cf_slist,".*","List of regexes of directory names NOT to include in depth search"},
   {"include_basedir",cf_opts,CF_BOOL,"true/false include the start/root dir of the search results"},
   {"include_dirs",cf_slist,".*","List of regexes of directory names to include in depth search"},
   {"rmdeadlinks",cf_opts,CF_BOOL,"true/false remove links that point to nowhere"},
   {"traverse_links",cf_opts,CF_BOOL,"true/false traverse symbolic links to directories (false)"},
   {"xdev",cf_opts,CF_BOOL,"true/false exclude directories that are on different devices"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_EDITS_BODY[] =
   {
   {"edit_backup",cf_opts,"true,false,timestamp,rotate","Menu option for backup policy on edit changes"},
   {"empty_file_before_editing",cf_opts,CF_BOOL,"Baseline memory model of file to zero/empty before commencing promised edits"},
   {"max_file_size",cf_int,CF_VALRANGE,"Do not edit files bigger than this number of bytes"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_TIDY_BODY[] =
   {
   {"dirlinks",cf_opts,"delete,tidy,keep","Menu option policy for dealing with symbolic links to directories during deletion"},
   {"rmdirs",cf_opts,CF_BOOL,"true/false whether to delete empty directories during recursive deletion"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_RENAME_BODY[] =
   {
   {"disable",cf_opts,CF_BOOL,"true/false automatically rename and remove permissions"},
   {"disable_mode",cf_str,CF_MODERANGE,"The permissions to set when a file is disabled"},
   {"disable_suffix",cf_str,"","The suffix to add to files when disabling (.cfdisabled)"},
   {"newname",cf_str,"","The desired name for the current file"},
   {"rotate",cf_int,"0,99","Maximum number of file rotations to keep"},
   {NULL,cf_notype,NULL,NULL}
   };


/**************************************************************/

struct BodySyntax CF_ACCESS_BODIES[] =
   {
   {"bsdflags",cf_slist,CF_BSDFLAGRANGE,"List of menu options for bsd file system flags to set"},
   {"groups",cf_slist,CF_USERRANGE,"List of acceptable groups of group ids, first is change target"},
   {"mode",cf_str,CF_MODERANGE,"File permissions (like posix chmod)"},
   {"owners",cf_slist,CF_USERRANGE,"List of acceptable owners or user ids, first is change target"},
   {"rxdirs",cf_opts,CF_BOOL,"true/false add execute flag for directories if read flag is set"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_FILEFILTER_BODY[] =
   {
   {"leaf_name",cf_slist,"","List of regexes that match an acceptable name"},
   {"path_name",cf_slist,CF_PATHRANGE,"List of pathnames to match acceptable target"},
   {"search_mode",cf_slist,CF_MODERANGE,"A list of mode masks for acceptable file permissions"},
   {"search_size",cf_irange,"0,inf","Integer range of file sizes"},
   {"search_owners",cf_slist,"","List of acceptable user names or ids for the file, or regexes to match"},
   {"search_groups",cf_slist,"","List of acceptable group names or ids for the file, or regexes to match"},
   {"search_bsdflags",cf_slist,CF_BSDFLAGRANGE,"String of flags for bsd file system flags expected set"},
   {"ctime",cf_irange,CF_TIMERANGE,"Range of change times (ctime) for acceptable files"},
   {"mtime",cf_irange,CF_TIMERANGE,"Range of modification times (mtime) for acceptable files"},
   {"atime",cf_irange,CF_TIMERANGE,"Range of access times (atime) for acceptable files"},
   {"exec_regex",cf_str,CF_PATHRANGE,"Matches file if this regular expression matches any full line returned by the command"},
   {"exec_program",cf_str,CF_PATHRANGE,"Execute this command on each file and match if the exit status is zero"},
   {"file_types",cf_olist,"plain,reg,symlink,dir,socket,fifo,door,char,block","List of acceptable file types from menu choices"},
   {"issymlinkto",cf_slist,"","List of regular expressions to match file objects"},
   {"file_result",cf_str,"[(leaf_name|path_name|file_types|mode|size|owner|group|atime|ctime|mtime|issymlinkto|exec_regex|exec_program|bsdflags)[|&!.]*]*","Logical expression combining classes defined by file search criteria"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

/* Copy and link are really the same body and should have
   non-overlapping patterns so that they are XOR but it's
   okay that some names overlap (like source) as there is
   no ambiguity in XOR */

struct BodySyntax CF_LINKTO_BODY[] =
   {
   {"copy_patterns",cf_slist,"","A set of patterns that should be copied ansd synchronized instead of linked"},
   {"link_children",cf_opts,CF_BOOL,"true/false whether to link all directory's children to source originals"},
   {"link_type",cf_opts,CF_LINKRANGE,"The type of link used to alias the file"},
   {"source",cf_str,"","The source file to which the link should point"},
   {"when_linking_children",cf_opts,"override_file,if_no_such_file","Policy for overriding existing files when linking directories of children"},
   {"when_no_source",cf_opts,"force,delete,nop","Behaviour when the source file to link to does not exist"},
   {NULL,cf_notype,NULL,NULL}
   };

/**************************************************************/

struct BodySyntax CF_COPYFROM_BODY[] =
   {
   {"source",cf_str,CF_PATHRANGE,"Reference source file from which to copy"},
   {"servers",cf_slist,"[A-Za-z0-9_.:-]+","List of servers in order of preference from which to copy"},
   {"collapse_destination_dir",cf_opts,CF_BOOL,"true/false Place files in subdirectories into the root destination directory during copy"},
   {"compare",cf_opts,"atime,mtime,ctime,digest,hash","Menu option policy for comparing source and image file attributes"},
   {"copy_backup",cf_opts,"true,false,timestamp","Menu option policy for file backup/version control"},
   {"encrypt",cf_opts,CF_BOOL,"true/false use encrypted data stream to connect to remote host"},
   {"check_root",cf_opts,CF_BOOL,"true/false check permissions on the root directory when depth_search"},
   {"copylink_patterns",cf_slist,"","List of patterns matching files that should be linked instead of copied"},
   {"copy_size",cf_irange,"0,inf","Integer range of file sizes that may be copied"},
   {"findertype",cf_opts,"MacOSX","Menu option for default finder type on MacOSX"},
   {"linkcopy_patterns",cf_slist,"","List of patterns matching symbolic links that should be replaced with copies"},
   {"link_type",cf_opts,CF_LINKRANGE,"Menu option for type of links to use when copying"},
   {"force_update",cf_opts,CF_BOOL,"true/false force copy update always"},
   {"force_ipv4",cf_opts,CF_BOOL,"true/false force use of ipv4 on ipv6 enabled network"},
   {"portnumber",cf_int,"1024,99999","Port number to connect to on server host"},
   {"preserve",cf_opts,CF_BOOL,"true/false whether to preserve file permissions on copied file"},
   {"purge",cf_opts,CF_BOOL,"true/false purge files on client that do not match files on server when depth_search"},
   {"stealth",cf_opts,CF_BOOL,"true/false whether to preserve time stamps on copied file"},
   {"trustkey",cf_opts,CF_BOOL,"true/false trust public keys from remote server if previously unknown"},
   {"type_check",cf_opts,CF_BOOL,"true/false compare file types before copying and require match"},
   {"verify",cf_opts,CF_BOOL,"true/false verify transferred file by hashing after copy (resource penalty)"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/

/* This is the primary set of constraints for a file object */

struct BodySyntax CF_FILES_BODIES[] =
   {
   {"acl",cf_body,CF_ACL_BODY,"Criteria for access control lists on file"},
   {"changes",cf_body,CF_CHANGEMGT_BODY,"Criteria for change management"},
   {"copy_from",cf_body,CF_COPYFROM_BODY,"Criteria for copying file from a source"},
   {"create",cf_opts,CF_BOOL,"true/false whether to create non-existing file"},
   {"delete",cf_body,CF_TIDY_BODY,"Criteria for deleting files"},
   {"depth_search",cf_body,CF_RECURSION_BODY,"Criteria for file depth searches"},
   {"edit_line",cf_bundle,CF_BUNDLE,"Line editing model for file"},
   {"edit_xml",cf_bundle,CF_BUNDLE,"XML editing model for file"},
   {"edit_defaults",cf_body,CF_EDITS_BODY,"Default promise details for file edits"},
   {"file_select",cf_body,CF_FILEFILTER_BODY,"Choose which files select in a search"},
   {"link_from",cf_body,CF_LINKTO_BODY,"Criteria for linking file from a source"},
   {"move_obstructions",cf_opts,CF_BOOL,"true/false whether to move obstructions to file-object creation"},
   {"pathtype",cf_opts,"literal,regex","Menu option for interpreting promiser file object"},
   {"perms",cf_body,CF_ACCESS_BODIES,"Criteria for setting permissions on a file"},
   {"rename",cf_body,CF_RENAME_BODY,"Criteria for renaming files"},
   {"repository",cf_str,CF_PATHRANGE,"Name of a repository for versioning"},
   {"touch",cf_opts,CF_BOOL,"true/false whether to touch time stamps on file"},
   {"transformer",cf_str,CF_PATHRANGE,"Command (with full path) used to transform current file (no shell wrapper used)"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_FILES_SUBTYPES[] =
  {
  /* Body lists belonging to "files:" type in Agent */
      
  {"agent","files",CF_FILES_BODIES},

  /* Body lists belonging to th edit_line sub-bundle of files: */
     
  {"edit_line","delete_lines",CF_DELETELINES_BODIES},
  {"edit_line","insert_lines",CF_INSERTLINES_BODIES},
  {"edit_line","field_edits",CF_COLUMN_BODIES},
  {"edit_line","replace_patterns",CF_REPLACE_BODIES},
  {NULL,NULL,NULL},
  };

