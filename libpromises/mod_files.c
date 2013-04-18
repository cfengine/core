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

#include "mod_files.h"

#include "syntax.h"

static const ConstraintSyntax CF_LOCATION_BODY[] =
{
    ConstraintSyntaxNewOption("before_after", "before,after", "Menu option, point cursor before of after matched line", "after"),
    ConstraintSyntaxNewOption("first_last", "first,last", "Menu option, choose first or last occurrence of match in file", "last"),
    ConstraintSyntaxNewString("select_line_matching", CF_ANYSTRING, "Regular expression for matching file line location", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_EDITCOL_BODY[] =
{
    ConstraintSyntaxNewBool("allow_blank_fields", "true/false allow blank fields in a line (do not purge)", "false"),
    ConstraintSyntaxNewBool("extend_fields", "true/false add new fields at end of line if necessary to complete edit", "false"),
    ConstraintSyntaxNewOption("field_operation", "prepend,append,alphanum,delete,set", "Menu option policy for editing subfields", "none"),
    ConstraintSyntaxNewString("field_separator", CF_ANYSTRING, "The regular expression used to separate fields in a line", "none"),
    ConstraintSyntaxNewString("field_value", CF_ANYSTRING, "Set field value to a fixed value", NULL),
    ConstraintSyntaxNewInt("select_field", "0,99999999", "Integer index of the field required 0..n (default starts from 1)", NULL),
    ConstraintSyntaxNewBool("start_fields_from_zero", "If set, the default field numbering starts from 0", NULL),
    ConstraintSyntaxNewString("value_separator", CF_CHARRANGE, "Character separator for subfields inside the selected field", "none"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_REPLACEWITH_BODY[] =
{
    ConstraintSyntaxNewOption("occurrences", "all,first", "Menu option to replace all occurrences or just first (NB the latter is non-convergent)", "all"),
    ConstraintSyntaxNewString("replace_value", CF_ANYSTRING, "Value used to replace regular expression matches in search", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_EDSCOPE_BODY[] =
{
    ConstraintSyntaxNewBool("include_start_delimiter", "Whether to include the section delimiter", "false"),
    ConstraintSyntaxNewBool("include_end_delimiter", "Whether to include the section delimiter", "false"),
    ConstraintSyntaxNewString("select_start", CF_ANYSTRING, "Regular expression matching start of edit region", NULL),
    ConstraintSyntaxNewString("select_end", CF_ANYSTRING, "Regular expression matches end of edit region from start", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_DELETESELECT_BODY[] =
{
    ConstraintSyntaxNewStringList("delete_if_startwith_from_list", CF_ANYSTRING, "Delete line if it starts with a string in the list"),
    ConstraintSyntaxNewStringList("delete_if_not_startwith_from_list", CF_ANYSTRING, "Delete line if it DOES NOT start with a string in the list"),
    ConstraintSyntaxNewStringList("delete_if_match_from_list", CF_ANYSTRING, "Delete line if it fully matches a regex in the list"),
    ConstraintSyntaxNewStringList("delete_if_not_match_from_list", CF_ANYSTRING,"Delete line if it DOES NOT fully match a regex in the list"),
    ConstraintSyntaxNewStringList("delete_if_contains_from_list", CF_ANYSTRING, "Delete line if a regex in the list match a line fragment"),
    ConstraintSyntaxNewStringList("delete_if_not_contains_from_list", CF_ANYSTRING,"Delete line if a regex in the list DOES NOT match a line fragment"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_INSERTSELECT_BODY[] =
{
    ConstraintSyntaxNewStringList("insert_if_startwith_from_list", CF_ANYSTRING, "Insert line if it starts with a string in the list"),
    ConstraintSyntaxNewStringList("insert_if_not_startwith_from_list", CF_ANYSTRING,"Insert line if it DOES NOT start with a string in the list"),
    ConstraintSyntaxNewStringList("insert_if_match_from_list", CF_ANYSTRING, "Insert line if it fully matches a regex in the list"),
    ConstraintSyntaxNewStringList("insert_if_not_match_from_list", CF_ANYSTRING,"Insert line if it DOES NOT fully match a regex in the list"),
    ConstraintSyntaxNewStringList("insert_if_contains_from_list", CF_ANYSTRING,"Insert line if a regex in the list match a line fragment"),
    ConstraintSyntaxNewStringList("insert_if_not_contains_from_list", CF_ANYSTRING, "Insert line if a regex in the list DOES NOT match a line fragment"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_INSERTLINES_BODIES[] =
{
    ConstraintSyntaxNewBool("expand_scalars", "Expand any unexpanded variables", "false"),
    ConstraintSyntaxNewOption("insert_type", "literal,string,file,file_preserve_block,preserve_block", "Type of object the promiser string refers to", "literal"),
    ConstraintSyntaxNewBody("insert_select", CF_INSERTSELECT_BODY, "Insert only if lines pass filter criteria"),
    ConstraintSyntaxNewBody("location", CF_LOCATION_BODY, "Specify where in a file an insertion will be made"),
    ConstraintSyntaxNewOptionList("whitespace_policy", "ignore_leading,ignore_trailing,ignore_embedded,exact_match", "Criteria for matching and recognizing existing lines"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_DELETELINES_BODIES[] =
{
    ConstraintSyntaxNewBody("delete_select", CF_DELETESELECT_BODY, "Delete only if lines pass filter criteria"),
    ConstraintSyntaxNewBool("not_matching", "true/false negate match criterion", "false"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_COLUMN_BODIES[] =
{
    ConstraintSyntaxNewBody("edit_field", CF_EDITCOL_BODY, "Edit line-based file as matrix of fields"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_REPLACE_BODIES[] =
{
    ConstraintSyntaxNewBody("replace_with", CF_REPLACEWITH_BODY, "Search-replace pattern"),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CF_COMMON_EDITBODIES[] =
{
    ConstraintSyntaxNewBody("select_region", CF_EDSCOPE_BODY, "Limit edits to a demarked region of the file"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_ACL_BODY[] =
{
    ConstraintSyntaxNewStringList("aces", "((user|group):[^:]+:[-=+,rwx()dtTabBpcoD]*(:(allow|deny))?)|((all|mask):[-=+,rwx()]*(:(allow|deny))?)", "Native settings for access control entry"),
    ConstraintSyntaxNewOption("acl_directory_inherit", "nochange,parent,specify,clear", "Access control list type for the affected file system", NULL),
    ConstraintSyntaxNewOption("acl_method", "append,overwrite", "Editing method for access control list", NULL),
    ConstraintSyntaxNewOption("acl_type", "generic,posix,ntfs", "Access control list type for the affected file system", NULL),
    ConstraintSyntaxNewStringList("specify_inherit_aces", "((user|group):[^:]+:[-=+,rwx()dtTabBpcoD]*(:(allow|deny))?)|((all|mask):[-=+,rwx()]*(:(allow|deny))?)", "Native settings for access control entry"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_CHANGEMGT_BODY[] =
{
    ConstraintSyntaxNewOption("hash", "md5,sha1,sha224,sha256,sha384,sha512,best", "Hash files for change detection", NULL),
    ConstraintSyntaxNewOption("report_changes", "all,stats,content,none", "Specify criteria for change warnings", NULL),
    ConstraintSyntaxNewBool("update_hashes", "Update hash values immediately after change warning", NULL),
    ConstraintSyntaxNewBool("report_diffs","Generate reports summarizing the major differences between individual text files", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_RECURSION_BODY[] =
{
    ConstraintSyntaxNewInt("depth", CF_VALRANGE, "Maximum depth level for search", NULL),
    ConstraintSyntaxNewStringList("exclude_dirs", ".*", "List of regexes of directory names NOT to include in depth search"),
    ConstraintSyntaxNewBool("include_basedir", "true/false include the start/root dir of the search results", NULL),
    ConstraintSyntaxNewStringList("include_dirs", ".*", "List of regexes of directory names to include in depth search"),
    ConstraintSyntaxNewBool("rmdeadlinks", "true/false remove links that point to nowhere", "false"),
    ConstraintSyntaxNewBool("traverse_links", "true/false traverse symbolic links to directories", "false"),
    ConstraintSyntaxNewBool("xdev", "true/false exclude directories that are on different devices", "false"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_EDITS_BODY[] =
{
    ConstraintSyntaxNewOption("edit_backup", "true,false,timestamp,rotate", "Menu option for backup policy on edit changes", "true"),
    ConstraintSyntaxNewBool("empty_file_before_editing", "Baseline memory model of file to zero/empty before commencing promised edits", "false"),
    ConstraintSyntaxNewBool("inherit", "If true this causes the sub-bundle to inherit the private classes of its parent", NULL),
    ConstraintSyntaxNewInt("max_file_size", CF_VALRANGE, "Do not edit files bigger than this number of bytes", NULL),
    ConstraintSyntaxNewBool("recognize_join", "Join together lines that end with a backslash, up to 4kB limit", "false"),
    ConstraintSyntaxNewInt("rotate", "0,99", "How many backups to store if 'rotate' edit_backup strategy is selected. Defaults to 1", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_TIDY_BODY[] =
{
    ConstraintSyntaxNewOption("dirlinks", "delete,tidy,keep", "Menu option policy for dealing with symbolic links to directories during deletion", NULL),
    ConstraintSyntaxNewBool("rmdirs", "true/false whether to delete empty directories during recursive deletion", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_RENAME_BODY[] =
{
    ConstraintSyntaxNewBool("disable", "true/false automatically rename and remove permissions", "false"),
    ConstraintSyntaxNewString("disable_mode", CF_MODERANGE, "The permissions to set when a file is disabled", NULL),
    ConstraintSyntaxNewString("disable_suffix", "", "The suffix to add to files when disabling (.cfdisabled)", NULL),
    ConstraintSyntaxNewString("newname", "", "The desired name for the current file", NULL),
    ConstraintSyntaxNewInt("rotate", "0,99", "Maximum number of file rotations to keep", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_ACCESS_BODIES[] =
{
    ConstraintSyntaxNewStringList("bsdflags", CF_BSDFLAGRANGE, "List of menu options for bsd file system flags to set"),
    ConstraintSyntaxNewStringList("groups", CF_USERRANGE, "List of acceptable groups of group ids, first is change target"),
    ConstraintSyntaxNewString("mode", CF_MODERANGE, "File permissions (like posix chmod)", NULL),
    ConstraintSyntaxNewStringList("owners", CF_USERRANGE, "List of acceptable owners or user ids, first is change target"),
    ConstraintSyntaxNewBool("rxdirs", "true/false add execute flag for directories if read flag is set", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_FILEFILTER_BODY[] =
{
    ConstraintSyntaxNewStringList("leaf_name", "", "List of regexes that match an acceptable name"),
    ConstraintSyntaxNewStringList("path_name", CF_ABSPATHRANGE, "List of pathnames to match acceptable target"),
    ConstraintSyntaxNewStringList("search_mode", CF_MODERANGE, "A list of mode masks for acceptable file permissions"),
    ConstraintSyntaxNewIntRange("search_size", "0,inf", "Integer range of file sizes", NULL),
    ConstraintSyntaxNewStringList("search_owners", "", "List of acceptable user names or ids for the file, or regexes to match"),
    ConstraintSyntaxNewStringList("search_groups", "", "List of acceptable group names or ids for the file, or regexes to match"),
    ConstraintSyntaxNewStringList("search_bsdflags", CF_BSDFLAGRANGE, "String of flags for bsd file system flags expected set"),
    ConstraintSyntaxNewIntRange("ctime", CF_TIMERANGE, "Range of change times (ctime) for acceptable files", NULL),
    ConstraintSyntaxNewIntRange("mtime", CF_TIMERANGE, "Range of modification times (mtime) for acceptable files", NULL),
    ConstraintSyntaxNewIntRange("atime", CF_TIMERANGE, "Range of access times (atime) for acceptable files", NULL),
    ConstraintSyntaxNewString("exec_regex", CF_ANYSTRING, "Matches file if this regular expression matches any full line returned by the command", NULL),
    ConstraintSyntaxNewString("exec_program", CF_ABSPATHRANGE, "Execute this command on each file and match if the exit status is zero", NULL),
    ConstraintSyntaxNewOptionList("file_types", "plain,reg,symlink,dir,socket,fifo,door,char,block", "List of acceptable file types from menu choices"),
    ConstraintSyntaxNewStringList("issymlinkto", "", "List of regular expressions to match file objects"),
    ConstraintSyntaxNewString("file_result", "[!*(leaf_name|path_name|file_types|mode|size|owner|group|atime|ctime|mtime|issymlinkto|exec_regex|exec_program|bsdflags)[|&.]*]*",
                        "Logical expression combining classes defined by file search criteria", NULL),
    ConstraintSyntaxNewNull()
};

/* Copy and link are really the same body and should have
   non-overlapping patterns so that they are XOR but it's
   okay that some names overlap (like source) as there is
   no ambiguity in XOR */

static const ConstraintSyntax CF_LINKTO_BODY[] =
{
    ConstraintSyntaxNewStringList("copy_patterns", "", "A set of patterns that should be copied and synchronized instead of linked"),
    ConstraintSyntaxNewBool("link_children", "true/false whether to link all directory's children to source originals", "false"),
    ConstraintSyntaxNewOption("link_type", CF_LINKRANGE, "The type of link used to alias the file", "symlink"),
    ConstraintSyntaxNewString("source", CF_PATHRANGE, "The source file to which the link should point", NULL),
    ConstraintSyntaxNewOption("when_linking_children", "override_file,if_no_such_file", "Policy for overriding existing files when linking directories of children", NULL),
    ConstraintSyntaxNewOption("when_no_source", "force,delete,nop", "Behaviour when the source file to link to does not exist", "nop"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_COPYFROM_BODY[] =
{
    /* We use CF_PATHRANGE due to collision with LINKTO_BODY and a bug lurking in
     * a verification stage -- this attribute gets picked instead of another
     * 'source'
     */
    ConstraintSyntaxNewString("source", CF_PATHRANGE, "Reference source file from which to copy", NULL),
    ConstraintSyntaxNewStringList("servers", "[A-Za-z0-9_.:-]+", "List of servers in order of preference from which to copy"),
    ConstraintSyntaxNewBool("collapse_destination_dir", "true/false Place files in subdirectories into the root destination directory during copy", NULL),
    ConstraintSyntaxNewOption("compare", "atime,mtime,ctime,digest,hash,exists,binary", "Menu option policy for comparing source and image file attributes", "mtime or ctime differs"),
    ConstraintSyntaxNewOption("copy_backup", "true,false,timestamp", "Menu option policy for file backup/version control", "true"),
    ConstraintSyntaxNewBool("encrypt", "true/false use encrypted data stream to connect to remote host", "false"),
    ConstraintSyntaxNewBool("check_root", "true/false check permissions on the root directory when depth_search", NULL),
    ConstraintSyntaxNewStringList("copylink_patterns", "", "List of patterns matching files that should be copied instead of linked"),
    ConstraintSyntaxNewIntRange("copy_size", "0,inf", "Integer range of file sizes that may be copied", "any size range"),
    ConstraintSyntaxNewOption("findertype", "MacOSX", "Menu option for default finder type on MacOSX", NULL),
    ConstraintSyntaxNewStringList("linkcopy_patterns", "", "List of patterns matching files that should be replaced with symbolic links"),
    ConstraintSyntaxNewOption("link_type", CF_LINKRANGE, "Menu option for type of links to use when copying", "symlink"),
    ConstraintSyntaxNewBool("force_update", "true/false force copy update always", "false"),
    ConstraintSyntaxNewBool("force_ipv4", "true/false force use of ipv4 on ipv6 enabled network", "false"),
    ConstraintSyntaxNewInt("portnumber", "1024,99999", "Port number to connect to on server host", NULL),
    ConstraintSyntaxNewBool("preserve", "true/false whether to preserve file permissions on copied file", "false"),
    ConstraintSyntaxNewBool("purge", "true/false purge files on client that do not match files on server when a depth_search is used", "false"),
    ConstraintSyntaxNewBool("stealth", "true/false whether to preserve time stamps on copied file", "false"),
    ConstraintSyntaxNewInt("timeout", "1,3600", "Connection timeout, seconds", NULL),
    ConstraintSyntaxNewBool("trustkey", "true/false trust public keys from remote server if previously unknown", "false"),
    ConstraintSyntaxNewBool("type_check", "true/false compare file types before copying and require match", NULL),
    ConstraintSyntaxNewBool("verify", "true/false verify transferred file by hashing after copy (resource penalty)", "false"),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_FILES_BODIES[] =
{
    ConstraintSyntaxNewBody("acl", CF_ACL_BODY, "Criteria for access control lists on file"),
    ConstraintSyntaxNewBody("changes", CF_CHANGEMGT_BODY, "Criteria for change management"),
    ConstraintSyntaxNewBody("copy_from", CF_COPYFROM_BODY, "Criteria for copying file from a source"),
    ConstraintSyntaxNewBool("create", "true/false whether to create non-existing file", "false"),
    ConstraintSyntaxNewBody("delete", CF_TIDY_BODY, "Criteria for deleting files"),
    ConstraintSyntaxNewBody("depth_search", CF_RECURSION_BODY, "Criteria for file depth searches"),
    ConstraintSyntaxNewBody("edit_defaults", CF_EDITS_BODY, "Default promise details for file edits"),
    ConstraintSyntaxNewBundle("edit_line", "Line editing model for file"),
    ConstraintSyntaxNewString("edit_template", CF_ABSPATHRANGE, "The name of a special CFEngine template file to expand", NULL),
    ConstraintSyntaxNewBundle("edit_xml", "XML editing model for file"),
    ConstraintSyntaxNewBody("file_select", CF_FILEFILTER_BODY, "Choose which files select in a search"),
    ConstraintSyntaxNewBody("link_from", CF_LINKTO_BODY, "Criteria for linking file from a source"),
    ConstraintSyntaxNewBool("move_obstructions", "true/false whether to move obstructions to file-object creation", "false"),
    ConstraintSyntaxNewOption("pathtype", "literal,regex,guess", "Menu option for interpreting promiser file object", NULL),
    ConstraintSyntaxNewBody("perms", CF_ACCESS_BODIES, "Criteria for setting permissions on a file"),
    ConstraintSyntaxNewBody("rename", CF_RENAME_BODY, "Criteria for renaming files"),
    ConstraintSyntaxNewString("repository", CF_ABSPATHRANGE, "Name of a repository for versioning", NULL),
    ConstraintSyntaxNewBool("touch", "true/false whether to touch time stamps on file", NULL),
    ConstraintSyntaxNewString("transformer", CF_ABSPATHRANGE, "Command (with full path) used to transform current file (no shell wrapper used)", NULL),
    ConstraintSyntaxNewNull()
};

// edit_xml body syntax
const ConstraintSyntax CF_COMMON_XMLBODIES[] =
{
    ConstraintSyntaxNewString("build_xpath", "", "Build an XPath within the XML file", NULL),
    ConstraintSyntaxNewString("select_xpath", "", "Select the XPath node in the XML file to edit", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_INSERTTAGS_BODIES[] =
{
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_DELETETAGS_BODIES[] =
{
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_INSERTATTRIBUTES_BODIES[] =
{
    ConstraintSyntaxNewString("attribute_value", "", "Value of the attribute to be inserted into the XPath node of the XML file", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_DELETEATTRIBUTES_BODIES[] =
{
    ConstraintSyntaxNewNull()
};


// Master Syntax for Files 

const PromiseTypeSyntax CF_FILES_PROMISE_TYPES[] =
{
    /* Body lists belonging to "files:" type in Agent */

    PromiseTypeSyntaxNew("agent", "files", ConstraintSetSyntaxNew(CF_FILES_BODIES, NULL)),

    /* Body lists belonging to th edit_line sub-bundle of files: */

    PromiseTypeSyntaxNew("edit_line", "*", ConstraintSetSyntaxNew(CF_COMMON_EDITBODIES, NULL)),
    PromiseTypeSyntaxNew("edit_line", "delete_lines", ConstraintSetSyntaxNew(CF_DELETELINES_BODIES, NULL)),
    PromiseTypeSyntaxNew("edit_line", "insert_lines", ConstraintSetSyntaxNew(CF_INSERTLINES_BODIES, NULL)),
    PromiseTypeSyntaxNew("edit_line", "field_edits", ConstraintSetSyntaxNew(CF_COLUMN_BODIES, NULL)),
    PromiseTypeSyntaxNew("edit_line", "replace_patterns", ConstraintSetSyntaxNew(CF_REPLACE_BODIES, NULL)),

    PromiseTypeSyntaxNew("edit_xml", "*", ConstraintSetSyntaxNew(CF_COMMON_XMLBODIES, NULL)),
    PromiseTypeSyntaxNew("edit_xml", "build_xpath", ConstraintSetSyntaxNew(CF_INSERTTAGS_BODIES, NULL)),
    PromiseTypeSyntaxNew("edit_xml", "delete_tree", ConstraintSetSyntaxNew(CF_DELETETAGS_BODIES, NULL)),
    PromiseTypeSyntaxNew("edit_xml", "insert_tree", ConstraintSetSyntaxNew(CF_INSERTTAGS_BODIES, NULL)),
    PromiseTypeSyntaxNew("edit_xml", "delete_attribute", ConstraintSetSyntaxNew(CF_DELETEATTRIBUTES_BODIES, NULL)),
    PromiseTypeSyntaxNew("edit_xml", "set_attribute", ConstraintSetSyntaxNew(CF_INSERTATTRIBUTES_BODIES, NULL)),
    PromiseTypeSyntaxNew("edit_xml", "delete_text", ConstraintSetSyntaxNew(CF_DELETETAGS_BODIES, NULL)),
    PromiseTypeSyntaxNew("edit_xml", "set_text", ConstraintSetSyntaxNew(CF_INSERTTAGS_BODIES, NULL)),
    PromiseTypeSyntaxNew("edit_xml", "insert_text", ConstraintSetSyntaxNew(CF_INSERTTAGS_BODIES, NULL)),

    PromiseTypeSyntaxNewNull(),
};
