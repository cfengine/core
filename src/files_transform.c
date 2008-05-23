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
/* File: files_transform.c                                                   */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*******************************************************************/

struct FileAttr GetFileAttributes(struct Promise *pp)

{ struct FileAttr attr;

attr.havedepthsearch = GetBooleanConstraint("depth_search",pp->conlist);
attr.haveselect = GetBooleanConstraint("file_select",pp->conlist);
attr.haverename = GetBooleanConstraint("rename",pp->conlist);
attr.havedelete = GetBooleanConstraint("delete",pp->conlist);
attr.haveperms = GetBooleanConstraint("perms",pp->conlist);
attr.havechange = GetBooleanConstraint("changes",pp->conlist);
attr.havecopy = GetBooleanConstraint("copy_from",pp->conlist);
attr.havelink = GetBooleanConstraint("link_from",pp->conlist);
attr.haveeditline = GetBooleanConstraint("edit_line",pp->conlist);
attr.haveeditxml = GetBooleanConstraint("edit_xml",pp->conlist);
attr.haveedit = attr.haveeditline || attr.haveeditxml;

/* Files, specialist - see files_transform.c */

attr.repository = (char *)GetConstraint("repository",pp->conlist,CF_SCALAR);
attr.create = GetBooleanConstraint("create",pp->conlist);
attr.touch = GetBooleanConstraint("touch",pp->conlist);
attr.transformer = (char *)GetConstraint("transformer",pp->conlist,CF_SCALAR);
attr.move_obstructions = GetBooleanConstraint("move_obstruction",pp->conlist);

attr.perms = GetPermissionConstraints(pp);
attr.select = GetSelectConstraints(pp);
attr.delete = GetDeleteConstraints(pp);
attr.rename = GetRenameConstraints(pp);
attr.change = GetChangeMgtConstraints(pp);
attr.copy = GetCopyConstraints(pp);
attr.link = GetLinkConstraints(pp);

/* Files, multiple use */

attr.recursion = GetRecursionConstraints(pp);

/* Common ("included") */

attr.transaction = GetTransactionConstraints(pp);
attr.classes = GetClassDefinitionConstraints(pp);

if (DEBUG)
   {
   ShowAttributes(attr);
   }

return attr;
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

struct Recursion GetRecursionConstraints(struct Promise *pp)

{ struct Recursion r;
r.travlinks = GetBooleanConstraint("traverse_links",pp->conlist);
r.rmdeadlinks = GetBooleanConstraint("rmdeadlinks",pp->conlist);
r.depth = GetIntConstraint("depth",pp->conlist);
r.xdev = GetBooleanConstraint("xdev",pp->conlist);
r.include_dirs = GetListConstraint("include_dirs",pp->conlist);
r.exclude_dirs = GetListConstraint("exclude_dirs",pp->conlist);
r.include_basedir = GetBooleanConstraint("include_basedir",pp->conlist);
return r;
}

/*******************************************************************/

struct FilePerms GetPermissionConstraints(struct Promise *pp)

{ struct FilePerms p;
  char *value;

value = (char *)GetConstraint("mode",pp->conlist,CF_SCALAR);
ParseModeString(value,&p.plus,&p.minus);
value = (char *)GetConstraint("bsdflags",pp->conlist,CF_SCALAR);
ParseFlagString(value,&p.plus_flags,&p.minus_flags);
p.owners = Rlist2UidList((struct Rlist *)GetConstraint("owner",pp->conlist,CF_LIST),pp);
p.groups = Rlist2GidList((struct Rlist *)GetConstraint("group",pp->conlist,CF_LIST),pp);
p.findertype = (char *)GetConstraint("findertype",pp->conlist,CF_SCALAR);
p.rxdirs = GetBooleanConstraint("rxdirs",pp->conlist);

return p;
}

/*******************************************************************/

struct FileSelect GetSelectConstraints(struct Promise *pp)

{ struct FileSelect s;
  char *value;
  
s.name = (struct Rlist *)GetConstraint("leaf_name",pp->conlist,CF_LIST);
s.path = (struct Rlist *)GetConstraint("path_name",pp->conlist,CF_LIST);
s.filetypes = (struct Rlist *)GetConstraint("file_types",pp->conlist,CF_LIST);
s.issymlinkto = (struct Rlist *)GetConstraint("issymlinkto",pp->conlist,CF_LIST);

value = (char *)GetConstraint("search_mode",pp->conlist,CF_SCALAR);
ParseModeString(value,&s.plus,&s.minus);
value = (char *)GetConstraint("search_bsdflags",pp->conlist,CF_SCALAR);
ParseFlagString(value,&s.plus_flags,&s.minus_flags);

s.owners = (struct Rlist *)GetConstraint("search_owner",pp->conlist,CF_LIST);
s.groups = (struct Rlist *)GetConstraint("search_group",pp->conlist,CF_LIST);

value = (char *)GetConstraint("search_size",pp->conlist,CF_SCALAR);
IntRange2Int(value,&s.min_size,&s.max_size,pp);
value = (char *)GetConstraint("ctime",pp->conlist,CF_SCALAR);
IntRange2Int(value,&s.min_ctime,&s.max_ctime,pp);
value = (char *)GetConstraint("atime",pp->conlist,CF_SCALAR);
IntRange2Int(value,&s.min_atime,&s.max_atime,pp);
value = (char *)GetConstraint("mtime",pp->conlist,CF_SCALAR);
IntRange2Int(value,&s.min_mtime,&s.max_mtime,pp);
s.exec_regex = (char *)GetConstraint("exec_regex",pp->conlist,CF_SCALAR);
s.exec_program = (char *)GetConstraint("exec_program",pp->conlist,CF_SCALAR);

if ((s.result = (char *)GetConstraint("file_result",pp->conlist,CF_SCALAR)) == NULL)
   {
   s.result = "leaf_name.path_name.file_types.owner.group.mode.ctime.mtime.atime.size.exec_regex.issymlinkto.exec_program";
   }

return s;
}

/*******************************************************************/

struct TransactionContext GetTransactionConstraints(struct Promise *pp)

{ struct TransactionContext t;
 char *value;

value = GetConstraint("action",pp->conlist,CF_SCALAR);

if (value && strcmp(value,"warn") == 0)
   {
   t.action = cfa_warn;
   }
else
   {
   t.action = cfa_fix; // default
   }

t.background = GetBooleanConstraint("background",pp->conlist);
t.ifelapsed = GetIntConstraint("ifelapsed",pp->conlist);

if (t.ifelapsed == CF_UNDEFINED)
   {
   t.ifelapsed = VIFELAPSED;
   }

t.expireafter = GetIntConstraint("expireafter",pp->conlist);

if (t.expireafter == CF_UNDEFINED)
   {
   t.expireafter = VEXPIREAFTER;
   }

t.log_string = GetConstraint("log_string",pp->conlist,CF_SCALAR);
t.log_level = GetConstraint("log_level",pp->conlist,CF_SCALAR);
t.audit = GetBooleanConstraint("audit",pp->conlist);

value = GetConstraint("report_level",pp->conlist,CF_SCALAR);

if (value && strcmp(value,"verbose") == 0)
   {
   t.report_level = cfverbose;   
   }
else if (value && strcmp(value,"error") == 0)
   {
   t.report_level = cferror;   
   }
else if (value && strcmp(value,"logonly") == 0)
   {
   t.report_level = cflogonly;   
   }
else if (value && strcmp(value,"loginform") == 0)
   {
   t.report_level = cfloginform;   
   }
else
   {
   t.report_level = cfsilent;   
   }

return t;
}

/*******************************************************************/

struct DefineClasses GetClassDefinitionConstraints(struct Promise *pp)

{ struct DefineClasses c;

c.change = (struct Rlist *)GetConstraint("change",pp->conlist,CF_LIST);
c.failure = (struct Rlist *)GetConstraint("failure",pp->conlist,CF_LIST);
c.denied = (struct Rlist *)GetConstraint("denied",pp->conlist,CF_LIST);
c.timeout = (struct Rlist *)GetConstraint("timeout",pp->conlist,CF_LIST);
c.interrupt = (struct Rlist *)GetConstraint("interrupt",pp->conlist,CF_LIST);

return c;
}

/*******************************************************************/

struct FileDelete GetDeleteConstraints(struct Promise *pp)

{ struct FileDelete f;
  char *value;

value = (char *)GetConstraint("dirlinks",pp->conlist,CF_SCALAR);

if (value && strcmp(value,"keep") == 0)
   {
   f.dirlinks = cfa_linkkeep;
   }
else
   {
   f.dirlinks = cfa_linkdelete;
   }

f.rmdirs = GetBooleanConstraint("rmdirs",pp->conlist);
return f;
}

/*******************************************************************/

struct FileRename GetRenameConstraints(struct Promise *pp)

{ struct FileRename r;
  char *value;

value = (char *)GetConstraint("mode",pp->conlist,CF_SCALAR);
ParseModeString(value,&r.plus,&r.minus);

r.disable = GetBooleanConstraint("disable",pp->conlist);
r.disable_suffix = (char *)GetConstraint("disable_suffix",pp->conlist,CF_SCALAR);
r.newname = (char *)GetConstraint("newname",pp->conlist,CF_SCALAR);
r.rotate = GetIntConstraint("rotate",pp->conlist);

return r;
}

/*******************************************************************/

struct FileChange GetChangeMgtConstraints(struct Promise *pp)

{ struct FileChange c;
  char *value;

value = (char *)GetConstraint("hash",pp->conlist,CF_SCALAR);

if (value && strcmp(value,"best") == 0)
   {
   c.hash = cf_besthash;
   }
else if (value && strcmp(value,"sha1") == 0)
   {
   c.hash = cf_sha1;
   }
else
   {
   c.hash = cf_md5;
   }

value = (char *)GetConstraint("reportt_changes",pp->conlist,CF_SCALAR);

if (value && strcmp(value,"content") == 0)
   {
   c.report_changes = cfa_contentchange;
   }
else
   {
   c.report_changes = cfa_noreport;
   }

c.update = GetBooleanConstraint("update",pp->conlist);

return c;
}

/*******************************************************************/

struct FileCopy GetCopyConstraints(struct Promise *pp)

{ struct FileCopy f;
  char *value;

f.source = (char *)GetConstraint("source",pp->conlist,CF_SCALAR);
value = (char *)GetConstraint("compare",pp->conlist,CF_SCALAR);
f.compare = String2Comparison(value);
value = (char *)GetConstraint("link_type",pp->conlist,CF_SCALAR);
f.link_type = String2LinkType(value);
f.servers = GetListConstraint("servers",pp->conlist);
f.portnumber = (short)GetIntConstraint("portnumber",pp->conlist);
f.link_instead = GetListConstraint("linkcopy_patterns",pp->conlist);
f.copy_links = GetListConstraint("copylink_patterns",pp->conlist);

value = (char *)GetConstraint("backup",pp->conlist,CF_SCALAR);

if (value && strcmp(value,"false") == 0)
   {
   f.backup = cfa_nobackup;
   }
else if (value && strcmp(value,"timestamp") == 0)
   {
   f.backup = cfa_timestamp;
   }
else
   {
   f.backup = cfa_backup;
   }
       
f.stealth = GetBooleanConstraint("stealth",pp->conlist);
f.preserve = GetBooleanConstraint("preserve",pp->conlist);
f.type_check = GetBooleanConstraint("type_check",pp->conlist);
f.force_update = GetBooleanConstraint("force_update",pp->conlist);
f.force_ipv4 = GetBooleanConstraint("force_ipv4",pp->conlist);
f.check_root = GetBooleanConstraint("check_root",pp->conlist);

value = (char *)GetConstraint("copy_size",pp->conlist,CF_SCALAR);
IntRange2Int(value,&f.min_size,&f.max_size,pp);

f.trustkey = GetBooleanConstraint("trustkey",pp->conlist);
f.encrypt = GetBooleanConstraint("encrypt",pp->conlist);
f.verify = GetBooleanConstraint("verify",pp->conlist);
f.purge = GetBooleanConstraint("purge",pp->conlist);

return f;
}

/*******************************************************************/

struct FileLink GetLinkConstraints(struct Promise *pp)

{ struct FileLink f;
  char *value;
  
f.source = (char *)GetConstraint("source",pp->conlist,CF_SCALAR);
value = (char *)GetConstraint("link_type",pp->conlist,CF_SCALAR);
f.link_type = String2LinkType(value);
f.copy_patterns = GetConstraint("copy_patterns",pp->conlist,CF_LIST);

value = (char *)GetConstraint("when_no_file",pp->conlist,CF_SCALAR);

if (value && strcmp(value,"force") == 0)
   {
   f.when_no_file = cfa_force;
   }
else if (value && strcmp(value,"delete") == 0)
   {
   f.when_no_file = cfa_delete;
   }
else
   {
   f.when_no_file = cfa_skip;
   }

value = (char *)GetConstraint("link_children",pp->conlist,CF_SCALAR);

if (value && strcmp(value,"override_existing") == 0)
   {
   f.link_children = cfa_override;
   }
else
   {
   f.link_children = cfa_onlynonexisting;
   }

return f;
}

/*******************************************************************/

void ShowAttributes(struct FileAttr a)

{
printf(".....................................................\n");
printf("File Attribute Set =\n\n");

if (a.havedepthsearch) printf(" * havedepthsearch\n");
if (a.haveselect) printf(" * haveselect\n");
if (a.haverename) printf(" * haverename\n");
if (a.havedelete) printf(" * havedelete\n");
if (a.haveperms) printf(" * haveperms\n");
if (a.havechange) printf(" * havechange\n");
if (a.havecopy) printf(" * havecopy\n");
if (a.havelink) printf(" * havelink\n");
if (a.haveedit) printf(" * haveedit\n");
if (a.create)  printf(" * havecreate\n");
if (a.touch)  printf(" * havetouch\n");
if (a.move_obstructions) printf(" * move_obstructions\n");

if (a.repository) printf(" * repository %s\n",a.repository);
if (a.transformer)printf(" * transformer %s\n",a.transformer);

/*
if (a.perms) printf(" * perms %o\n",a.perms.mode);
a.select = GetSelectConstraints(pp);
a.delete = GetDeleteConstraints(pp);
a.rename = GetRenameConstraints(pp);
a.change = GetChangeMgtConstraints(pp);
a.copy = GetCopyConstraints(pp);
a.link = GetLinkConstraints(pp);
a.recursion = GetRecursionConstraints(pp);

a.transaction = GetTransactionConstraints(pp);
a.classes = GetClassDefinitionConstraints(pp);
*/
printf(".....................................................\n\n");
}

