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

struct Attributes GetFilesAttributes(struct Promise *pp)

{ struct Attributes attr;

attr.havedepthsearch = GetBooleanConstraint("depth_search",pp->conlist);
attr.haveselect = GetBooleanConstraint("file_select",pp->conlist);
attr.haverename = GetBooleanConstraint("rename",pp->conlist);
attr.havedelete = GetBooleanConstraint("delete",pp->conlist);
attr.haveperms = GetBooleanConstraint("perms",pp->conlist);
attr.havechange = GetBooleanConstraint("changes",pp->conlist);
attr.havecopy = GetBooleanConstraint("copy_from",pp->conlist);
attr.havelink = GetBooleanConstraint("link_from",pp->conlist);
attr.haveeditline = GetBundleConstraint("edit_line",pp->conlist);
attr.haveeditxml = GetBundleConstraint("edit_xml",pp->conlist);
attr.haveedit = attr.haveeditline || attr.haveeditxml;

/* Files, specialist */

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
attr.edits = GetEditDefaults(pp);

/* Files, multiple use */

attr.recursion = GetRecursionConstraints(pp);

/* Common ("included") */

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp->conlist);
attr.transaction = GetTransactionConstraints(pp);
attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp->conlist);
attr.classes = GetClassDefinitionConstraints(pp);

if (DEBUG)
   {
   ShowAttributes(attr);
   }

return attr;
}

/*******************************************************************/

struct Attributes GetReportsAttributes(struct Promise *pp)

{ struct Attributes attr;

attr.transaction = GetTransactionConstraints(pp);
attr.classes = GetClassDefinitionConstraints(pp);
return attr;
}

/*******************************************************************/

struct Attributes GetClassContextAttributes(struct Promise *pp)

{ struct Attributes a;

a.transaction = GetTransactionConstraints(pp);
a.classes = GetClassDefinitionConstraints(pp);
a.context = GetContextConstraints(pp);

return a;
}

/*******************************************************************/

struct Attributes GetExecAttributes(struct Promise *pp)

{ struct Attributes attr;

attr.contain = GetExecContainConstraints(pp);
attr.havecontain = GetBooleanConstraint("contain",pp->conlist);

attr.args = GetConstraint("args",pp->conlist,CF_SCALAR);
attr.module = GetBooleanConstraint("module",pp->conlist);

/* Common ("included") */

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp->conlist);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp->conlist);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}


/*******************************************************************/

struct Attributes GetProcessAttributes(struct Promise *pp)

{ struct Attributes attr;

attr.signals = GetListConstraint("signals",pp->conlist);
attr.process_stop = (char *)GetConstraint("process_stop",pp->conlist,CF_SCALAR);
attr.haveprocess_count = GetBooleanConstraint("process_count",pp->conlist);
attr.haveselect = GetBooleanConstraint("process_select",pp->conlist);
attr.restart_class = (char *)GetConstraint("restart_class",pp->conlist,CF_SCALAR);

attr.process_count = GetMatchesConstraints(pp);
attr.process_select = GetProcessFilterConstraints(pp);

/* Common ("included") */

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp->conlist);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp->conlist);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

struct ExecContain GetExecContainConstraints(struct Promise *pp)

{ struct ExecContain e;
 
e.useshell = GetBooleanConstraint("useshell",pp->conlist);
e.umask = GetOctalConstraint("umask",pp->conlist);
e.owner = GetUidConstraint("exec_owner",pp->conlist,pp);
e.group = GetGidConstraint("exec_group",pp->conlist,pp);
e.preview = GetBooleanConstraint("preview",pp->conlist);
e.nooutput = GetBooleanConstraint("no_output",pp->conlist);
e.timeout = GetBooleanConstraint("exec_timeout",pp->conlist);
e.chroot = GetConstraint("chroot",pp->conlist,CF_SCALAR);
e.chdir = GetConstraint("chdir",pp->conlist,CF_SCALAR);

return e;
}

/*******************************************************************/

struct Recursion GetRecursionConstraints(struct Promise *pp)

{ struct Recursion r;
r.travlinks = GetBooleanConstraint("traverse_links",pp->conlist);
r.rmdeadlinks = GetBooleanConstraint("rmdeadlinks",pp->conlist);
r.depth = GetIntConstraint("depth",pp->conlist);

if (r.depth == CF_UNDEFINED)
   {
   r.depth = 0;
   }

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

p.plus = 0;
p.minus = 0;

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

s.plus = 0;
s.minus = 0;
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

t.audit = GetBooleanConstraint("audit",pp->conlist);
t.log_string = GetConstraint("log_string",pp->conlist,CF_SCALAR);

value = GetConstraint("log_level",pp->conlist,CF_SCALAR);
t.log_level = String2ReportLevel(value);

value = GetConstraint("report_level",pp->conlist,CF_SCALAR);
t.report_level = String2ReportLevel(value);

return t;
}

/*******************************************************************/

struct DefineClasses GetClassDefinitionConstraints(struct Promise *pp)

{ struct DefineClasses c;
 char *pt = NULL;

c.change = (struct Rlist *)GetListConstraint("on_change",pp->conlist);
c.failure = (struct Rlist *)GetListConstraint("on_failure",pp->conlist);
c.denied = (struct Rlist *)GetListConstraint("on_denied",pp->conlist);
c.timeout = (struct Rlist *)GetListConstraint("on_timeout",pp->conlist);
c.interrupt = (struct Rlist *)GetListConstraint("on_interrupt",pp->conlist);

c.persist = GetIntConstraint("persist_time",pp->conlist);

if (c.persist == CF_UNDEFINED)
   {
   c.persist = 20;
   }

pt = GetConstraint("timer_policy",pp->conlist,CF_SCALAR);

if (pt && strncmp(pt,"abs",3) == 0)
   {
   c.timer = cfpreserve;
   }
else
   {
   c.timer = cfreset;
   }

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

if (f.portnumber == CF_UNDEFINED)
   {
   f.portnumber = 5308;
   }

f.link_instead = GetListConstraint("linkcopy_patterns",pp->conlist);
f.copy_links = GetListConstraint("copylink_patterns",pp->conlist);

value = (char *)GetConstraint("copy_backup",pp->conlist,CF_SCALAR);

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

struct EditDefaults GetEditDefaults(struct Promise *pp)

{ struct EditDefaults e;
  char *value;

e.maxfilesize = GetIntConstraint("max_file_size",pp->conlist);

if (e.maxfilesize == CF_UNDEFINED)
   {
   e.maxfilesize = EDITFILESIZE;
   }

value = (char *)GetConstraint("edit_backup",pp->conlist,CF_SCALAR);

if (value && strcmp(value,"false") == 0)
   {
   e.backup = cfa_nobackup;
   }
else if (value && strcmp(value,"timestamp") == 0)
   {
   e.backup = cfa_timestamp;
   }
else
   {
   e.backup = cfa_backup;
   }

e.empty_before_use = GetBooleanConstraint("empty_file_before_editing",pp->conlist);

return e;
}

/*******************************************************************/

struct Context GetContextConstraints(struct Promise *pp)

{ struct Context a;
  struct Constraint *cp;
  int i;

a.broken = -1;
a.expression = NULL; 

for (cp = pp->conlist; cp != NULL; cp=cp->next)
   {
   for (i = 0; CF_CLASSBODY[i].lval != NULL; i++)
      {
      if (strcmp(cp->lval,CF_CLASSBODY[i].lval) == 0)
         {
         a.expression = cp;
         a.broken++;
         }
      }
   }

return a;
}

/*******************************************************************/

struct ProcessSelect GetProcessFilterConstraints(struct Promise *pp)

{ struct ProcessSelect p;
  char *value;
 
p.owner = GetListConstraint("process_owner",pp->conlist);

value = (char *)GetConstraint("pid",pp->conlist,CF_SCALAR);
IntRange2Int(value,&p.min_pid,&p.max_pid,pp);
value = (char *)GetConstraint("ppid",pp->conlist,CF_SCALAR);
IntRange2Int(value,&p.min_ppid,&p.max_ppid,pp);
value = (char *)GetConstraint("pgid",pp->conlist,CF_SCALAR);
IntRange2Int(value,&p.min_pgid,&p.max_pgid,pp);
value = (char *)GetConstraint("rsize",pp->conlist,CF_SCALAR);
IntRange2Int(value,&p.min_rsize,&p.max_rsize,pp);
value = (char *)GetConstraint("vsize",pp->conlist,CF_SCALAR);
IntRange2Int(value,&p.min_rsize,&p.max_rsize,pp);
value = (char *)GetConstraint("ttime_range",pp->conlist,CF_SCALAR);
IntRange2Int(value,&p.min_ttime,&p.max_ttime,pp);
value = (char *)GetConstraint("stime_range",pp->conlist,CF_SCALAR);
IntRange2Int(value,&p.min_stime,&p.max_stime,pp);

p.status = (char *)GetConstraint("status",pp->conlist,CF_SCALAR);
p.command = (char *)GetConstraint("command",pp->conlist,CF_SCALAR);
p.tty = (char *)GetConstraint("tty",pp->conlist,CF_SCALAR);

value = (char *)GetConstraint("priority",pp->conlist,CF_SCALAR);
IntRange2Int(value,&p.min_pri,&p.max_pri,pp);
value = (char *)GetConstraint("threads",pp->conlist,CF_SCALAR);
IntRange2Int(value,&p.min_thread,&p.max_thread,pp);

p.process_result = (char *)GetConstraint("process_result",pp->conlist,CF_SCALAR);
return p;
}


/*******************************************************************/

struct ProcessCount GetMatchesConstraints(struct Promise *pp)

{ struct ProcessCount p;
  char *value;

value = (char *)GetConstraint("match_range",pp->conlist,CF_SCALAR);
IntRange2Int(value,&p.min_range,&p.max_range,pp);
p.in_range_define = GetListConstraint("in_range_define",pp->conlist);
p.out_of_range_define = GetListConstraint("out_of_range_define",pp->conlist);

return p;
}

/*******************************************************************/


void ShowAttributes(struct Attributes a)

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

/*******************************************************************/
/* Edit sub-bundles have their own attributes                      */
/*******************************************************************/

struct Attributes GetInsertionAttributes(struct Promise *pp)

{ struct Attributes attr;

attr.havelocation = GetBooleanConstraint("location",pp->conlist);
attr.location = GetLocationAttributes(pp);

attr.sourcetype = GetConstraint("source_type",pp->conlist,CF_SCALAR);
attr.expandvars = GetBooleanConstraint("expand_vars",pp->conlist);
                                                
/* Common ("included") */

attr.haveregion = GetBooleanConstraint("select_region",pp->conlist);
attr.region = GetRegionConstraints(pp);

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp->conlist);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp->conlist);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct EditLocation GetLocationAttributes(struct Promise *pp)

{ struct EditLocation e;
 char *value;

e.line_matching = GetConstraint("select_line_matching",pp->conlist,CF_SCALAR);;
value = GetConstraint("before_after",pp->conlist,CF_SCALAR);;

if (value && strcmp(value,"before") == 0)
   {
   e.before_after = cfe_before;
   }
else
   {
   e.before_after = cfe_after;
   }

e.first_last = GetConstraint("first_last",pp->conlist,CF_SCALAR);;
return e;
}

/*******************************************************************/

struct Attributes GetDeletionAttributes(struct Promise *pp)

{ struct Attributes attr;


 /* common */

attr.haveregion = GetBooleanConstraint("select_region",pp->conlist);
attr.region = GetRegionConstraints(pp);

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp->conlist);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp->conlist);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetColumnAttributes(struct Promise *pp)

{ struct Attributes attr;

attr.havecolumn = GetBooleanConstraint("edit_column",pp->conlist);

 /* common */

attr.haveregion = GetBooleanConstraint("select_region",pp->conlist);
attr.region = GetRegionConstraints(pp);

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp->conlist);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp->conlist);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetReplaceAttributes(struct Promise *pp)

{ struct Attributes attr;

attr.havereplace = GetBooleanConstraint("replace_pattern",pp->conlist);
attr.replace = GetReplaceConstraints(pp);

 /* common */

attr.haveregion = GetBooleanConstraint("select_region",pp->conlist);
attr.region = GetRegionConstraints(pp);

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp->conlist);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp->conlist);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct EditRegion GetRegionConstraints(struct Promise *pp)

{ struct EditRegion e;

e.select_start = GetConstraint("select_start",pp->conlist,CF_SCALAR);
e.select_end = GetConstraint("select_end",pp->conlist,CF_SCALAR);
 
return e;
}

/*******************************************************************/

struct EditReplace GetReplaceConstraints(struct Promise *pp)

{ struct EditReplace r;

r.replace_value = GetConstraint("replace_value",pp->conlist,CF_SCALAR);
r.occurrences = GetConstraint("occurrences",pp->conlist,CF_SCALAR);
 
return r;
}
