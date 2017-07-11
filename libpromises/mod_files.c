/*
   Copyright 2017 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by CFEngine AS.

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <mod_files.h>

#include <policy.h>
#include <syntax.h>

static const ConstraintSyntax location_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewOption("before_after", "before,after", "Menu option, point cursor before of after matched line. Default value: after", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("first_last", "first,last", "Menu option, choose first or last occurrence of match in file. Default value: last", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("select_line_matching", CF_ANYSTRING, "Regular expression for matching file line location", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax location_body = BodySyntaxNew("location", location_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax edit_field_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewBool("allow_blank_fields", "true/false allow blank fields in a line (do not purge). Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("extend_fields", "true/false add new fields at end of line if necessary to complete edit. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("field_operation", "prepend,append,alphanum,delete,set", "Menu option policy for editing subfields. Default value: none", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("field_separator", CF_ANYSTRING, "The regular expression used to separate fields in a line. Default value: none", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("field_value", CF_ANYSTRING, "Set field value to a fixed value", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("select_field", "0,99999999", "Integer index of the field required 0..n (default starts from 1)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("start_fields_from_zero", "If set, the default field numbering starts from 0", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("value_separator", CF_CHARRANGE, "Character separator for subfields inside the selected field. Default value: none", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax edit_field_body = BodySyntaxNew("edit_field", edit_field_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax replace_with_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewOption("occurrences", "all,first", "Menu option to replace all occurrences or just first (NB the latter is non-convergent). Default value: all", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("replace_value", CF_ANYSTRING, "Value used to replace regular expression matches in search", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax replace_with_body = BodySyntaxNew("replace_with", replace_with_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax select_region_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewBool("include_start_delimiter", "Whether to include the section delimiter. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("include_end_delimiter", "Whether to include the section delimiter. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("select_start", CF_ANYSTRING, "Regular expression matching start of edit region", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("select_end", CF_ANYSTRING, "Regular expression matches end of edit region from start", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("select_end_match_eof", "Whether to include EOF as end of the region. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax select_region_body = BodySyntaxNew("select_region", select_region_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax delete_select_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewStringList("delete_if_startwith_from_list", CF_ANYSTRING, "Delete line if it starts with a string in the list", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("delete_if_not_startwith_from_list", CF_ANYSTRING, "Delete line if it DOES NOT start with a string in the list", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("delete_if_match_from_list", CF_ANYSTRING, "Delete line if it fully matches a regex in the list", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("delete_if_not_match_from_list", CF_ANYSTRING,"Delete line if it DOES NOT fully match a regex in the list", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("delete_if_contains_from_list", CF_ANYSTRING, "Delete line if a regex in the list match a line fragment", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("delete_if_not_contains_from_list", CF_ANYSTRING,"Delete line if a regex in the list DOES NOT match a line fragment", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax delete_select_body = BodySyntaxNew("delete_select", delete_select_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax insert_select_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewStringList("insert_if_startwith_from_list", CF_ANYSTRING, "Insert line if it starts with a string in the list", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("insert_if_not_startwith_from_list", CF_ANYSTRING,"Insert line if it DOES NOT start with a string in the list", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("insert_if_match_from_list", CF_ANYSTRING, "Insert line if it fully matches a regex in the list", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("insert_if_not_match_from_list", CF_ANYSTRING,"Insert line if it DOES NOT fully match a regex in the list", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("insert_if_contains_from_list", CF_ANYSTRING,"Insert line if a regex in the list match a line fragment", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("insert_if_not_contains_from_list", CF_ANYSTRING, "Insert line if a regex in the list DOES NOT match a line fragment", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax insert_select_body = BodySyntaxNew("insert_select", insert_select_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax CF_INSERTLINES_BODIES[] =
{
    ConstraintSyntaxNewBool("expand_scalars", "Expand any unexpanded variables. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("insert_type", "literal,string,file,file_preserve_block,preserve_block,preserve_all_lines", "Type of object the promiser string refers to. Default value: literal", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("insert_select", &insert_select_body, "Insert only if lines pass filter criteria", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("location", &location_body, "Specify where in a file an insertion will be made", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOptionList("whitespace_policy", "ignore_leading,ignore_trailing,ignore_embedded,exact_match", "Criteria for matching and recognizing existing lines", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_DELETELINES_BODIES[] =
{
    ConstraintSyntaxNewBody("delete_select", &delete_select_body, "Delete only if lines pass filter criteria", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("not_matching", "true/false negate match criterion. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_COLUMN_BODIES[] =
{
    ConstraintSyntaxNewBody("edit_field", &edit_field_body, "Edit line-based file as matrix of fields", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_REPLACE_BODIES[] =
{
    ConstraintSyntaxNewBody("replace_with", &replace_with_body, "Search-replace pattern", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const ConstraintSyntax CF_COMMON_EDITBODIES[] =
{
    ConstraintSyntaxNewBody("select_region", &select_region_body, "Limit edits to a demarked region of the file", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static bool AclCheck(const Body *body, Seq *errors)
{
    bool success = true;

    if (BodyHasConstraint(body, "acl_directory_inherit")
        && BodyHasConstraint(body, "acl_default"))
    {
        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_BODY, body, "An acl body cannot have both acl_directory_inherit and acl_default. Please use acl_default only"));
        success = false;
    }
    if (BodyHasConstraint(body, "specify_inherit_aces")
        && BodyHasConstraint(body, "specify_default_aces"))
    {
        SeqAppend(errors, PolicyErrorNew(POLICY_ELEMENT_TYPE_BODY, body, "An acl body cannot have both specify_inherit_aces and specify_default_aces. Please use specify_default_aces only"));
        success = false;
    }

    return success;
}

static const ConstraintSyntax acl_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewStringList("aces", "((user|group):[^:]+:[-=+,rwx()dtTabBpcoD]*(:(allow|deny))?)|((all|mask):[-=+,rwx()]*(:(allow|deny))?)", "Native settings for access control entry", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("acl_directory_inherit", "nochange,parent,specify,clear", "Access control list type for the affected file system", SYNTAX_STATUS_DEPRECATED),
    ConstraintSyntaxNewOption("acl_default", "nochange,access,specify,clear", "How to apply default (inheritable) access control list", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("acl_method", "append,overwrite", "Editing method for access control list", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("acl_type", "generic,posix,ntfs", "Access control list type for the affected file system", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("specify_inherit_aces", "((user|group):[^:]+:[-=+,rwx()dtTabBpcoD]*(:(allow|deny))?)|((all|mask):[-=+,rwx()]*(:(allow|deny))?)", "Native settings for access control entry", SYNTAX_STATUS_DEPRECATED),
    ConstraintSyntaxNewStringList("specify_default_aces", "((user|group):[^:]+:[-=+,rwx()dtTabBpcoD]*(:(allow|deny))?)|((all|mask):[-=+,rwx()]*(:(allow|deny))?)", "Native settings for access control entry", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("acl_inherit", CF_BOOL ",nochange", "Whether the object inherits its ACL from the parent (Windows only)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax acl_body = BodySyntaxNew("acl", acl_constraints, &AclCheck, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax changes_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewOption("hash", "md5,sha1,sha224,sha256,sha384,sha512,best", "Hash files for change detection", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("report_changes", "all,stats,content,none", "Specify criteria for change warnings", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("update_hashes", "Update hash values immediately after change warning", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("report_diffs","Generate reports summarizing the major differences between individual text files", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax changes_body = BodySyntaxNew("changes", changes_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax depth_search_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewInt("depth", CF_VALRANGE, "Maximum depth level for search", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("exclude_dirs", ".*", "List of regexes of directory names NOT to include in depth search", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("include_basedir", "true/false include the start/root dir of the search results", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("include_dirs", ".*", "List of regexes of directory names to include in depth search", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("rmdeadlinks", "true/false remove links that point to nowhere. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("traverse_links", "true/false traverse symbolic links to directories. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("xdev", "true/false exclude directories that are on different devices. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax depth_search_body = BodySyntaxNew("depth_search", depth_search_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax edit_defaults_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewOption("edit_backup", "true,false,timestamp,rotate", "Menu option for backup policy on edit changes. Default value: true", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("empty_file_before_editing", "Baseline memory model of file to zero/empty before commencing promised edits. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("inherit", "If true this causes the sub-bundle to inherit the private classes of its parent", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("max_file_size", CF_VALRANGE, "Do not edit files bigger than this number of bytes", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("recognize_join", "Join together lines that end with a backslash, up to 4kB limit. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("rotate", "0,99", "How many backups to store if 'rotate' edit_backup strategy is selected. Default value: 1", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax edit_defaults_body = BodySyntaxNew("edit_defaults", edit_defaults_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax delete_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewOption("dirlinks", "delete,tidy,keep", "Menu option policy for dealing with symbolic links to directories during deletion", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("rmdirs", "true/false whether to delete empty directories during recursive deletion", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax delete_body = BodySyntaxNew("delete", delete_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax rename_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewBool("disable", "true/false automatically rename and remove permissions. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("disable_mode", CF_MODERANGE, "The permissions to set when a file is disabled", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("disable_suffix", "", "The suffix to add to files when disabling (.cfdisabled)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("newname", "", "The desired name for the current file", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("rotate", "0,99", "Maximum number of file rotations to keep", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax rename_body = BodySyntaxNew("rename", rename_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax perms_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewStringList("bsdflags", CF_BSDFLAGRANGE, "List of menu options for bsd file system flags to set", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("groups", CF_USERRANGE, "List of acceptable groups of group ids, first is change target", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("mode", CF_MODERANGE, "File permissions (like posix chmod)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("owners", CF_USERRANGE, "List of acceptable owners or user ids, first is change target", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("rxdirs", "true/false add execute flag for directories if read flag is set", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax perms_body = BodySyntaxNew("perms", perms_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax file_select_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewStringList("leaf_name", "", "List of regexes that match an acceptable name", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("path_name", CF_ABSPATHRANGE, "List of pathnames to match acceptable target", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("search_mode", CF_MODERANGE, "A list of mode masks for acceptable file permissions", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("search_size", "0,inf", "Integer range of file sizes", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("search_owners", "", "List of acceptable user names or ids for the file, or regexes to match", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("search_groups", "", "List of acceptable group names or ids for the file, or regexes to match", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("search_bsdflags", CF_BSDFLAGRANGE, "String of flags for bsd file system flags expected set", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("ctime", CF_TIMERANGE, "Range of change times (ctime) for acceptable files", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("mtime", CF_TIMERANGE, "Range of modification times (mtime) for acceptable files", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("atime", CF_TIMERANGE, "Range of access times (atime) for acceptable files", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("exec_regex", CF_ANYSTRING, "Matches file if this regular expression matches any full line returned by the command", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("exec_program", CF_ABSPATHRANGE, "Execute this command on each file and match if the exit status is zero", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOptionList("file_types", "plain,reg,symlink,dir,socket,fifo,door,char,block", "List of acceptable file types from menu choices", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("issymlinkto", "", "List of regular expressions to match file objects", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("file_result", "[!*(leaf_name|path_name|file_types|mode|size|owner|group|atime|ctime|mtime|issymlinkto|exec_regex|exec_program|bsdflags)[|&.]*]*",
                        "Logical expression combining classes defined by file search criteria", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax file_select_body = BodySyntaxNew("file_select", file_select_constraints, NULL, SYNTAX_STATUS_NORMAL);

/* Copy and link are really the same body and should have
   non-overlapping patterns so that they are XOR but it's
   okay that some names overlap (like source) as there is
   no ambiguity in XOR */

static const ConstraintSyntax link_from_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewStringList("copy_patterns", "", "A set of patterns that should be copied and synchronized instead of linked", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("link_children", "true/false whether to link all directory's children to source originals. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("link_type", CF_LINKRANGE, "The type of link used to alias the file. Default value: symlink", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("source", CF_PATHRANGE, "The source file to which the link should point", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("when_linking_children", "override_file,if_no_such_file", "Policy for overriding existing files when linking directories of children", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("when_no_source", "force,delete,nop", "Behaviour when the source file to link to does not exist. Default value: nop", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax link_from_body = BodySyntaxNew("link_from", link_from_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax copy_from_constraints[] =
{
    /* We use CF_PATHRANGE due to collision with LINKTO_BODY and a bug lurking in
     * a verification stage -- this attribute gets picked instead of another
     * 'source'
     */
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewString("source", CF_PATHRANGE, "Reference source file from which to copy", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("servers", "[A-Za-z0-9_.:\\-\\[\\]]+", "List of servers in order of preference from which to copy", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("collapse_destination_dir", "Copy files from subdirectories to the root destination directory. Default: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("compare", "atime,mtime,ctime,digest,hash,exists,binary", "Menu option policy for comparing source and image file attributes. Default: mtime or ctime differs", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("copy_backup", "true,false,timestamp", "Menu option policy for file backup/version control. Default value: true", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("encrypt", "true/false use encrypted data stream to connect to remote host. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("check_root", "true/false check permissions on the root directory when depth_search", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("copylink_patterns", "", "List of patterns matching files that should be copied instead of linked", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewIntRange("copy_size", "0,inf", "Integer range of file sizes that may be copied", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("findertype", "MacOSX", "Menu option for default finder type on MacOSX", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("linkcopy_patterns", "", "List of patterns matching files that should be replaced with symbolic links", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("link_type", CF_LINKRANGE, "Menu option for type of links to use when copying. Default value: symlink", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("force_update", "true/false force copy update always. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("force_ipv4", "true/false force use of ipv4 on ipv6 enabled network. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("portnumber", "", "Port number or service name to connect to on server host", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("preserve", "true/false whether to preserve file permissions on copied file. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("purge", "true/false purge files on client that do not match files on server when a depth_search is used. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("stealth", "true/false whether to preserve time stamps on copied file. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("timeout", "1,3600", "Connection timeout, seconds", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("trustkey", "true/false trust public keys from remote server if previously unknown. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("type_check", "true/false compare file types before copying and require match", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("verify", "true/false verify transferred file by hashing after copy (resource penalty). Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("protocol_version", "0,undefined,1,classic,2,latest", "CFEngine protocol version to use when connecting to the server. Default: undefined", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax copy_from_body = BodySyntaxNew("copy_from", copy_from_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax CF_FILES_BODIES[] =
{
    ConstraintSyntaxNewBody("acl", &acl_body, "Criteria for access control lists on file", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("changes", &changes_body, "Criteria for change management", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("copy_from", &copy_from_body, "Criteria for copying file from a source", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("create", "true/false whether to create non-existing file. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("delete", &delete_body, "Criteria for deleting files", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("depth_search", &depth_search_body, "Criteria for file depth searches", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("edit_defaults", &edit_defaults_body, "Default promise details for file edits", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBundle("edit_line", "Line editing model for file", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("edit_template", CF_ABSPATHRANGE, "The name of a special CFEngine template file to expand", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBundle("edit_xml", "XML editing model for file", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("file_select", &file_select_body, "Choose which files select in a search", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("file_type", "regular,fifo", "Type of file to create. Default value: regular", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("link_from", &link_from_body, "Criteria for linking file from a source", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("move_obstructions", "true/false whether to move obstructions to file-object creation. Default value: false", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("pathtype", "literal,regex,guess", "Menu option for interpreting promiser file object", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("perms", &perms_body, "Criteria for setting permissions on a file", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("rename", &rename_body, "Criteria for renaming files", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("repository", CF_ABSPATHRANGE, "Name of a repository for versioning", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("touch", "true/false whether to touch time stamps on file", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("transformer", CF_ABSPATHRANGE, "Command (with full path) used to transform current file (no shell wrapper used)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("template_method", "cfengine,mustache", "", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewContainer("template_data", "", SYNTAX_STATUS_NORMAL),

    ConstraintSyntaxNewNull()
};

// edit_xml body syntax
const ConstraintSyntax CF_COMMON_XMLBODIES[] =
{
    ConstraintSyntaxNewString("build_xpath", "", "Build an XPath within the XML file", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("select_xpath", "", "Select the XPath node in the XML file to edit", SYNTAX_STATUS_NORMAL),
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
    ConstraintSyntaxNewString("attribute_value", "", "Value of the attribute to be inserted into the XPath node of the XML file", SYNTAX_STATUS_NORMAL),
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

    PromiseTypeSyntaxNew("agent", "files", CF_FILES_BODIES, NULL, SYNTAX_STATUS_NORMAL),

    /* Body lists belonging to th edit_line sub-bundle of files: */

    PromiseTypeSyntaxNew("edit_line", "*", CF_COMMON_EDITBODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("edit_line", "delete_lines", CF_DELETELINES_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("edit_line", "insert_lines", CF_INSERTLINES_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("edit_line", "field_edits", CF_COLUMN_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("edit_line", "replace_patterns", CF_REPLACE_BODIES, NULL, SYNTAX_STATUS_NORMAL),

    PromiseTypeSyntaxNew("edit_xml", "*", CF_COMMON_XMLBODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("edit_xml", "build_xpath", CF_INSERTTAGS_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("edit_xml", "delete_tree", CF_DELETETAGS_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("edit_xml", "insert_tree", CF_INSERTTAGS_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("edit_xml", "delete_attribute", CF_DELETEATTRIBUTES_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("edit_xml", "set_attribute", CF_INSERTATTRIBUTES_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("edit_xml", "delete_text", CF_DELETETAGS_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("edit_xml", "set_text", CF_INSERTTAGS_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNew("edit_xml", "insert_text", CF_INSERTTAGS_BODIES, NULL, SYNTAX_STATUS_NORMAL),

    PromiseTypeSyntaxNewNull()
};
