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

#include "cf3.defs.h"
#include "mod_files.h"

static const BodySyntax CF_LOCATION_BODY[] =
{
    {"before_after", cf_opts, "before,after", "Menu option, point cursor before of after matched line", "after"},
    {"first_last", cf_opts, "first,last", "Menu option, choose first or last occurrence of match in file", "last"},
    {"select_line_matching", cf_str, CF_ANYSTRING, "Regular expression for matching file line location"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_EDITCOL_BODY[] =
{
    {"allow_blank_fields", cf_opts, CF_BOOL, "true/false allow blank fields in a line (do not purge)", "false"},
    {"extend_fields", cf_opts, CF_BOOL, "true/false add new fields at end of line if necessary to complete edit",
     "false"},
    {"field_operation", cf_opts, "prepend,append,alphanum,delete,set", "Menu option policy for editing subfields",
     "none"},
    {"field_separator", cf_str, CF_ANYSTRING, "The regular expression used to separate fields in a line", "none"},
    {"field_value", cf_str, CF_ANYSTRING, "Set field value to a fixed value"},
    {"select_field", cf_int, "0,99999999", "Integer index of the field required 0..n (default starts from 1)"},
    {"start_fields_from_zero", cf_opts, CF_BOOL, "If set, the default field numbering starts from 0"},
    {"value_separator", cf_str, CF_CHARRANGE, "Character separator for subfields inside the selected field", "none"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_REPLACEWITH_BODY[] =
{
    {"occurrences", cf_opts, "all,first",
     "Menu option to replace all occurrences or just first (NB the latter is non-convergent)", "all"},
    {"replace_value", cf_str, CF_ANYSTRING, "Value used to replace regular expression matches in search"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_EDSCOPE_BODY[] =
{
    {"include_start_delimiter", cf_opts, CF_BOOL, "Whether to include the section delimiter", "false"},
    {"include_end_delimiter", cf_opts, CF_BOOL, "Whether to include the section delimiter", "false"},
    {"select_start", cf_str, CF_ANYSTRING, "Regular expression matching start of edit region"},
    {"select_end", cf_str, CF_ANYSTRING, "Regular expression matches end of edit region from start"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_DELETESELECT_BODY[] =
{
    {"delete_if_startwith_from_list", cf_slist, CF_ANYSTRING, "Delete line if it starts with a string in the list"},
    {"delete_if_not_startwith_from_list", cf_slist, CF_ANYSTRING,
     "Delete line if it DOES NOT start with a string in the list"},
    {"delete_if_match_from_list", cf_slist, CF_ANYSTRING, "Delete line if it fully matches a regex in the list"},
    {"delete_if_not_match_from_list", cf_slist, CF_ANYSTRING,
     "Delete line if it DOES NOT fully match a regex in the list"},
    {"delete_if_contains_from_list", cf_slist, CF_ANYSTRING,
     "Delete line if a regex in the list match a line fragment"},
    {"delete_if_not_contains_from_list", cf_slist, CF_ANYSTRING,
     "Delete line if a regex in the list DOES NOT match a line fragment"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_INSERTSELECT_BODY[] =
{
    {"insert_if_startwith_from_list", cf_slist, CF_ANYSTRING, "Insert line if it starts with a string in the list"},
    {"insert_if_not_startwith_from_list", cf_slist, CF_ANYSTRING,
     "Insert line if it DOES NOT start with a string in the list"},
    {"insert_if_match_from_list", cf_slist, CF_ANYSTRING, "Insert line if it fully matches a regex in the list"},
    {"insert_if_not_match_from_list", cf_slist, CF_ANYSTRING,
     "Insert line if it DOES NOT fully match a regex in the list"},
    {"insert_if_contains_from_list", cf_slist, CF_ANYSTRING,
     "Insert line if a regex in the list match a line fragment"},
    {"insert_if_not_contains_from_list", cf_slist, CF_ANYSTRING,
     "Insert line if a regex in the list DOES NOT match a line fragment"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_INSERTLINES_BODIES[] =
{
    {"expand_scalars", cf_opts, CF_BOOL, "Expand any unexpanded variables", "false"},
    {"insert_type", cf_opts, "literal,string,file,file_preserve_block,preserve_block", "Type of object the promiser string refers to",
     "literal"},
    {"insert_select", cf_body, CF_INSERTSELECT_BODY, "Insert only if lines pass filter criteria"},
    {"location", cf_body, CF_LOCATION_BODY, "Specify where in a file an insertion will be made"},
    {"whitespace_policy", cf_olist, "ignore_leading,ignore_trailing,ignore_embedded,exact_match",
     "Criteria for matching and recognizing existing lines"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_DELETELINES_BODIES[] =
{
    {"delete_select", cf_body, CF_DELETESELECT_BODY, "Delete only if lines pass filter criteria"},
    {"not_matching", cf_opts, CF_BOOL, "true/false negate match criterion", "false"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_COLUMN_BODIES[] =
{
    {"edit_field", cf_body, CF_EDITCOL_BODY, "Edit line-based file as matrix of fields"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_REPLACE_BODIES[] =
{
    {"replace_with", cf_body, CF_REPLACEWITH_BODY, "Search-replace pattern"},
    {NULL, cf_notype, NULL, NULL}
};

const BodySyntax CF_COMMON_EDITBODIES[] =
{
    {"select_region", cf_body, CF_EDSCOPE_BODY, "Limit edits to a demarked region of the file"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_ACL_BODY[] =
{
    {"aces", cf_slist,
     "((user|group):[^:]+:[-=+,rwx()dtTabBpcoD]*(:(allow|deny))?)|((all|mask):[-=+,rwx()]*(:(allow|deny))?)",
     "Native settings for access control entry"},
    {"acl_directory_inherit", cf_opts, "nochange,parent,specify,clear",
     "Access control list type for the affected file system"},
    {"acl_method", cf_opts, "append,overwrite", "Editing method for access control list"},
    {"acl_type", cf_opts, "generic,posix,ntfs", "Access control list type for the affected file system"},
    {"specify_inherit_aces", cf_slist,
     "((user|group):[^:]+:[-=+,rwx()dtTabBpcoD]*(:(allow|deny))?)|((all|mask):[-=+,rwx()]*(:(allow|deny))?)",
     "Native settings for access control entry"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_CHANGEMGT_BODY[] =
{
    {"hash", cf_opts, "md5,sha1,sha224,sha256,sha384,sha512,best", "Hash files for change detection"},
    {"report_changes", cf_opts, "all,stats,content,none", "Specify criteria for change warnings"},
    {"update_hashes", cf_opts, CF_BOOL, "Update hash values immediately after change warning"},
    {"report_diffs", cf_opts, CF_BOOL,
     "Generate reports summarizing the major differences between individual text files"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_RECURSION_BODY[] =
{
    {"depth", cf_int, CF_VALRANGE, "Maximum depth level for search"},
    {"exclude_dirs", cf_slist, ".*", "List of regexes of directory names NOT to include in depth search"},
    {"include_basedir", cf_opts, CF_BOOL, "true/false include the start/root dir of the search results"},
    {"include_dirs", cf_slist, ".*", "List of regexes of directory names to include in depth search"},
    {"rmdeadlinks", cf_opts, CF_BOOL, "true/false remove links that point to nowhere", "false"},
    {"traverse_links", cf_opts, CF_BOOL, "true/false traverse symbolic links to directories", "false"},
    {"xdev", cf_opts, CF_BOOL, "true/false exclude directories that are on different devices", "false"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_EDITS_BODY[] =
{
    {"edit_backup", cf_opts, "true,false,timestamp,rotate", "Menu option for backup policy on edit changes", "true"},
    {"empty_file_before_editing", cf_opts, CF_BOOL,
     "Baseline memory model of file to zero/empty before commencing promised edits", "false"},
    {"inherit", cf_opts, CF_BOOL, "If true this causes the sub-bundle to inherit the private classes of its parent"},
    {"max_file_size", cf_int, CF_VALRANGE, "Do not edit files bigger than this number of bytes"},
    {"recognize_join", cf_opts, CF_BOOL, "Join together lines that end with a backslash, up to 4kB limit", "false"},
    {"rotate", cf_int, "0,99", "How many backups to store if 'rotate' edit_backup strategy is selected. Defaults to 1"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_TIDY_BODY[] =
{
    {"dirlinks", cf_opts, "delete,tidy,keep",
     "Menu option policy for dealing with symbolic links to directories during deletion"},
    {"rmdirs", cf_opts, CF_BOOL, "true/false whether to delete empty directories during recursive deletion"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_RENAME_BODY[] =
{
    {"disable", cf_opts, CF_BOOL, "true/false automatically rename and remove permissions", "false"},
    {"disable_mode", cf_str, CF_MODERANGE, "The permissions to set when a file is disabled"},
    {"disable_suffix", cf_str, "", "The suffix to add to files when disabling (.cfdisabled)"},
    {"newname", cf_str, "", "The desired name for the current file"},
    {"rotate", cf_int, "0,99", "Maximum number of file rotations to keep"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_ACCESS_BODIES[] =
{
    {"bsdflags", cf_slist, CF_BSDFLAGRANGE, "List of menu options for bsd file system flags to set"},
    {"groups", cf_slist, CF_USERRANGE, "List of acceptable groups of group ids, first is change target"},
    {"mode", cf_str, CF_MODERANGE, "File permissions (like posix chmod)"},
    {"owners", cf_slist, CF_USERRANGE, "List of acceptable owners or user ids, first is change target"},
    {"rxdirs", cf_opts, CF_BOOL, "true/false add execute flag for directories if read flag is set"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_FILEFILTER_BODY[] =
{
    {"leaf_name", cf_slist, "", "List of regexes that match an acceptable name"},
    {"path_name", cf_slist, CF_ABSPATHRANGE, "List of pathnames to match acceptable target"},
    {"search_mode", cf_slist, CF_MODERANGE, "A list of mode masks for acceptable file permissions"},
    {"search_size", cf_irange, "0,inf", "Integer range of file sizes"},
    {"search_owners", cf_slist, "", "List of acceptable user names or ids for the file, or regexes to match"},
    {"search_groups", cf_slist, "", "List of acceptable group names or ids for the file, or regexes to match"},
    {"search_bsdflags", cf_slist, CF_BSDFLAGRANGE, "String of flags for bsd file system flags expected set"},
    {"ctime", cf_irange, CF_TIMERANGE, "Range of change times (ctime) for acceptable files"},
    {"mtime", cf_irange, CF_TIMERANGE, "Range of modification times (mtime) for acceptable files"},
    {"atime", cf_irange, CF_TIMERANGE, "Range of access times (atime) for acceptable files"},
    {"exec_regex", cf_str, CF_ANYSTRING,
     "Matches file if this regular expression matches any full line returned by the command"},
    {"exec_program", cf_str, CF_ABSPATHRANGE, "Execute this command on each file and match if the exit status is zero"},
    {"file_types", cf_olist, "plain,reg,symlink,dir,socket,fifo,door,char,block",
     "List of acceptable file types from menu choices"},
    {"issymlinkto", cf_slist, "", "List of regular expressions to match file objects"},
    {"file_result", cf_str,
     "[!*(leaf_name|path_name|file_types|mode|size|owner|group|atime|ctime|mtime|issymlinkto|exec_regex|exec_program|bsdflags)[|&.]*]*",
     "Logical expression combining classes defined by file search criteria"},
    {NULL, cf_notype, NULL, NULL}
};

/* Copy and link are really the same body and should have
   non-overlapping patterns so that they are XOR but it's
   okay that some names overlap (like source) as there is
   no ambiguity in XOR */

static const BodySyntax CF_LINKTO_BODY[] =
{
    {"copy_patterns", cf_slist, "", "A set of patterns that should be copied and synchronized instead of linked"},
    {"link_children", cf_opts, CF_BOOL, "true/false whether to link all directory's children to source originals",
     "false"},
    {"link_type", cf_opts, CF_LINKRANGE, "The type of link used to alias the file", "symlink"},
    {"source", cf_str, CF_PATHRANGE, "The source file to which the link should point"},
    {"when_linking_children", cf_opts, "override_file,if_no_such_file",
     "Policy for overriding existing files when linking directories of children"},
    {"when_no_source", cf_opts, "force,delete,nop", "Behaviour when the source file to link to does not exist", "nop"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_COPYFROM_BODY[] =
{
    /* We use CF_PATHRANGE due to collision with LINKTO_BODY and a bug lurking in
     * a verification stage -- this attribute gets picked instead of another
     * 'source'
     */
    {"source", cf_str, CF_PATHRANGE, "Reference source file from which to copy"},
    {"servers", cf_slist, "[A-Za-z0-9_.:-]+", "List of servers in order of preference from which to copy"},
    {"collapse_destination_dir", cf_opts, CF_BOOL,
     "true/false Place files in subdirectories into the root destination directory during copy"},
    {"compare", cf_opts, "atime,mtime,ctime,digest,hash,exists,binary",
     "Menu option policy for comparing source and image file attributes", "mtime or ctime differs"},
    {"copy_backup", cf_opts, "true,false,timestamp", "Menu option policy for file backup/version control", "true"},
    {"encrypt", cf_opts, CF_BOOL, "true/false use encrypted data stream to connect to remote host", "false"},
    {"check_root", cf_opts, CF_BOOL, "true/false check permissions on the root directory when depth_search"},
    {"copylink_patterns", cf_slist, "", "List of patterns matching files that should be copied instead of linked"},
    {"copy_size", cf_irange, "0,inf", "Integer range of file sizes that may be copied", "any size range"},
    {"findertype", cf_opts, "MacOSX", "Menu option for default finder type on MacOSX"},
    {"linkcopy_patterns", cf_slist, "", "List of patterns matching files that should be replaced with symbolic links"},
    {"link_type", cf_opts, CF_LINKRANGE, "Menu option for type of links to use when copying", "symlink"},
    {"force_update", cf_opts, CF_BOOL, "true/false force copy update always", "false"},
    {"force_ipv4", cf_opts, CF_BOOL, "true/false force use of ipv4 on ipv6 enabled network", "false"},
    {"portnumber", cf_int, "1024,99999", "Port number to connect to on server host"},
    {"preserve", cf_opts, CF_BOOL, "true/false whether to preserve file permissions on copied file", "false"},
    {"purge", cf_opts, CF_BOOL,
     "true/false purge files on client that do not match files on server when a depth_search is used", "false"},
    {"stealth", cf_opts, CF_BOOL, "true/false whether to preserve time stamps on copied file", "false"},
    {"timeout", cf_int, "1,3600", "Connection timeout, seconds"},
    {"trustkey", cf_opts, CF_BOOL, "true/false trust public keys from remote server if previously unknown", "false"},
    {"type_check", cf_opts, CF_BOOL, "true/false compare file types before copying and require match"},
    {"verify", cf_opts, CF_BOOL, "true/false verify transferred file by hashing after copy (resource penalty)",
     "false"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_FILES_BODIES[] =
{
    {"acl", cf_body, CF_ACL_BODY, "Criteria for access control lists on file"},
    {"changes", cf_body, CF_CHANGEMGT_BODY, "Criteria for change management"},
    {"copy_from", cf_body, CF_COPYFROM_BODY, "Criteria for copying file from a source"},
    {"create", cf_opts, CF_BOOL, "true/false whether to create non-existing file", "false"},
    {"delete", cf_body, CF_TIDY_BODY, "Criteria for deleting files"},
    {"depth_search", cf_body, CF_RECURSION_BODY, "Criteria for file depth searches"},
    {"edit_defaults", cf_body, CF_EDITS_BODY, "Default promise details for file edits"},
    {"edit_line", cf_bundle, CF_BUNDLE, "Line editing model for file"},
    {"edit_template", cf_str, CF_ABSPATHRANGE, "The name of a special CFEngine template file to expand"},
    {"edit_xml", cf_bundle, CF_BUNDLE, "XML editing model for file"},
    {"file_select", cf_body, CF_FILEFILTER_BODY, "Choose which files select in a search"},
    {"link_from", cf_body, CF_LINKTO_BODY, "Criteria for linking file from a source"},
    {"move_obstructions", cf_opts, CF_BOOL, "true/false whether to move obstructions to file-object creation", "false"},
    {"pathtype", cf_opts, "literal,regex,guess", "Menu option for interpreting promiser file object"},
    {"perms", cf_body, CF_ACCESS_BODIES, "Criteria for setting permissions on a file"},
    {"rename", cf_body, CF_RENAME_BODY, "Criteria for renaming files"},
    {"repository", cf_str, CF_ABSPATHRANGE, "Name of a repository for versioning"},
    {"touch", cf_opts, CF_BOOL, "true/false whether to touch time stamps on file"},
    {"transformer", cf_str, CF_ABSPATHRANGE,
     "Command (with full path) used to transform current file (no shell wrapper used)"},
    {NULL, cf_notype, NULL, NULL}
};

// edit_xml body syntax
const BodySyntax CF_COMMON_XMLBODIES[] =
{
    {"build_xpath", cf_str, "", "Build an XPath within the XML file"},
    {"select_xpath", cf_str, "", "Select the XPath node in the XML file to edit"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_INSERTTAGS_BODIES[] =
{
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_DELETETAGS_BODIES[] =
{
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_INSERTATTRIBUTES_BODIES[] =
{
    {"attribute_value", cf_str, "", "Value of the attribute to be inserted into the XPath node of the XML file"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_DELETEATTRIBUTES_BODIES[] =
{
    {NULL, cf_notype, NULL, NULL}
};


// Master Syntax for Files 

const SubTypeSyntax CF_FILES_SUBTYPES[] =
{
    /* Body lists belonging to "files:" type in Agent */

    {"agent", "files", CF_FILES_BODIES},

    /* Body lists belonging to th edit_line sub-bundle of files: */

    {"edit_line", "*", CF_COMMON_EDITBODIES},
    {"edit_line", "delete_lines", CF_DELETELINES_BODIES},
    {"edit_line", "insert_lines", CF_INSERTLINES_BODIES},
    {"edit_line", "field_edits", CF_COLUMN_BODIES},
    {"edit_line", "replace_patterns", CF_REPLACE_BODIES},

    {"edit_xml", "*", CF_COMMON_XMLBODIES},
    {"edit_xml", "build_xpath", CF_INSERTTAGS_BODIES},
    {"edit_xml", "delete_tree", CF_DELETETAGS_BODIES},
    {"edit_xml", "insert_tree", CF_INSERTTAGS_BODIES},
    {"edit_xml", "delete_attribute", CF_DELETEATTRIBUTES_BODIES},
    {"edit_xml", "set_attribute", CF_INSERTATTRIBUTES_BODIES},
    {"edit_xml", "delete_text", CF_DELETETAGS_BODIES},
    {"edit_xml", "set_text", CF_INSERTTAGS_BODIES},
    {"edit_xml", "insert_text", CF_INSERTTAGS_BODIES},

    {NULL, NULL, NULL},
};
