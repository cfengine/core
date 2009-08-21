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
/* File: files_transform.c                                                   */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*******************************************************************/

struct Attributes GetFilesAttributes(struct Promise *pp)

{ static struct Attributes attr;

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
attr.move_obstructions = GetBooleanConstraint("move_obstructions",pp->conlist);
attr.pathtype = (char *)GetConstraint("pathtype",pp->conlist,CF_SCALAR);

attr.acl = GetAclConstraints(pp);
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

if (attr.haverename || attr.havedelete || attr.haveperms || attr.havechange ||
    attr.havecopy || attr.havelink || attr.haveedit || attr.create || attr.touch || attr.transformer)
   {
   }
else
   {
   if (THIS_AGENT_TYPE == cf_common)
      {
      cfPS(cf_error,CF_WARN,"",pp,attr," !! files promise makes no intention about system state");
      }
   }

return attr;
}

/*******************************************************************/

struct Attributes GetReportsAttributes(struct Promise *pp)

{ static struct Attributes attr;

attr.transaction = GetTransactionConstraints(pp);
attr.classes = GetClassDefinitionConstraints(pp);

attr.report = GetReportConstraints(pp);
return attr;
}

/*******************************************************************/

struct Attributes GetPackageAttributes(struct Promise *pp)

{ static struct Attributes attr;

attr.transaction = GetTransactionConstraints(pp);
attr.classes = GetClassDefinitionConstraints(pp);
attr.packages = GetPackageConstraints(pp);
return attr;
}

/*******************************************************************/

struct Attributes GetDatabaseAttributes(struct Promise *pp)

{ static struct Attributes attr;

attr.transaction = GetTransactionConstraints(pp);
attr.classes = GetClassDefinitionConstraints(pp);
attr.database = GetDatabaseConstraints(pp);
return attr;
}

/*******************************************************************/

struct Attributes GetClassContextAttributes(struct Promise *pp)

{ static struct Attributes a;

a.transaction = GetTransactionConstraints(pp);
a.classes = GetClassDefinitionConstraints(pp);
a.context = GetContextConstraints(pp);

return a;
}

/*******************************************************************/

struct Attributes GetExecAttributes(struct Promise *pp)

{ static struct Attributes attr;

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

{ static struct Attributes attr;

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

struct Attributes GetStorageAttributes(struct Promise *pp)

{ static struct Attributes attr;
 
attr.mount = GetMountConstraints(pp);
attr.volume = GetVolumeConstraints(pp);
attr.havevolume = GetBooleanConstraint("volume",pp->conlist);
attr.havemount = GetBooleanConstraint("mount",pp->conlist);

/* Common ("included") */

attr.edits.maxfilesize = EDITFILESIZE;

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp->conlist);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp->conlist);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetMethodAttributes(struct Promise *pp)

{ static struct Attributes attr;
 
attr.havebundle = GetBundleConstraint("usebundle",pp->conlist);

/* Common ("included") */

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp->conlist);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp->conlist);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetInterfacesAttributes(struct Promise *pp)

{ static struct Attributes attr;
 
attr.havetcpip = GetBundleConstraint("usebundle",pp->conlist);
attr.tcpip = GetTCPIPAttributes(pp);
    
/* Common ("included") */

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp->conlist);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp->conlist);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetTopicsAttributes(struct Promise *pp)

{ static struct Attributes attr;

attr.fwd_name = GetConstraint("forward_relationship",pp->conlist,CF_SCALAR);
attr.bwd_name = GetConstraint("backward_relationship",pp->conlist,CF_SCALAR);
attr.associates = GetListConstraint("associates",pp->conlist);
return attr;
}

/*******************************************************************/

struct Attributes GetOccurrenceAttributes(struct Promise *pp)

{ static struct Attributes attr;
  char *value;

attr.represents = GetListConstraint("represents",pp->conlist);
attr.rep_type = GetConstraint("representation",pp->conlist,CF_SCALAR);
attr.web_root = GetConstraint("web_root",pp->conlist,CF_SCALAR);
attr.path_root = GetConstraint("path_root",pp->conlist,CF_SCALAR);

return attr;
}

/*******************************************************************/

struct Attributes GetMeasurementAttributes(struct Promise *pp)

{ static struct Attributes attr;

attr.measure = GetMeasurementConstraint(pp);
    
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

{ static struct ExecContain e;
 
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

{ static struct Recursion r;
 
r.travlinks = GetBooleanConstraint("traverse_links",pp->conlist);
r.rmdeadlinks = GetBooleanConstraint("rmdeadlinks",pp->conlist);
r.depth = GetIntConstraint("depth",pp->conlist);

if (r.depth == CF_NOINT)
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

struct CfACL GetAclConstraints(struct Promise *pp)

{ static struct CfACL ac;

ac.acl_method = Str2AclMethod(GetConstraint("acl_method",pp->conlist,CF_SCALAR));
ac.acl_type = Str2AclType(GetConstraint("acl_type",pp->conlist,CF_SCALAR));
ac.acl_directory_inherit = Str2AclInherit(GetConstraint("acl_directory_inherit",pp->conlist,CF_SCALAR));
ac.acl_entries = GetListConstraint("aces",pp->conlist);
ac.acl_inherit_entries = GetListConstraint("specify_inherit_aces",pp->conlist);
return ac;
}

/*******************************************************************/

struct FilePerms GetPermissionConstraints(struct Promise *pp)

{ static struct FilePerms p;
  char *value;
  struct Rlist *list;
                
value = (char *)GetConstraint("mode",pp->conlist,CF_SCALAR);

p.plus = 0;
p.minus = 0;

if (!ParseModeString(value,&p.plus,&p.minus))
   {
   CfOut(cf_error,"","Problem validating a mode string");
   PromiseRef(cf_error,pp);
   }

list = GetListConstraint("bsdflags",pp->conlist);

if (list && !ParseFlagString(list,&p.plus_flags,&p.minus_flags))
   {
   CfOut(cf_error,"","Problem validating a BSD flag string");
   PromiseRef(cf_error,pp);
   }

p.owners = Rlist2UidList((struct Rlist *)GetConstraint("owners",pp->conlist,CF_LIST),pp);
p.groups = Rlist2GidList((struct Rlist *)GetConstraint("groups",pp->conlist,CF_LIST),pp);
p.findertype = (char *)GetConstraint("findertype",pp->conlist,CF_SCALAR);
p.rxdirs = GetBooleanConstraint("rxdirs",pp->conlist);

// The default should be true

if (!GetConstraint("rxdirs",pp->conlist,CF_SCALAR))
   {
   p.rxdirs = true;
   }

return p;
}

/*******************************************************************/

struct FileSelect GetSelectConstraints(struct Promise *pp)

{ static struct FileSelect s;
  char *value;
  struct Rlist *rp;
  mode_t plus,minus;
  u_long fplus,fminus;
  int entries = false;
  
s.name = (struct Rlist *)GetConstraint("leaf_name",pp->conlist,CF_LIST);
s.path = (struct Rlist *)GetConstraint("path_name",pp->conlist,CF_LIST);
s.filetypes = (struct Rlist *)GetConstraint("file_types",pp->conlist,CF_LIST);
s.issymlinkto = (struct Rlist *)GetConstraint("issymlinkto",pp->conlist,CF_LIST);

s.perms = GetListConstraint("search_mode",pp->conlist);

for  (rp = s.perms; rp != NULL; rp=rp->next)
   {
   plus = 0;
   minus = 0;
   value = (char *)rp->item;
   
   if (!ParseModeString(value,&plus,&minus))
      {
      CfOut(cf_error,"","Problem validating a mode string");
      PromiseRef(cf_error,pp);
      }
   }

s.bsdflags = GetListConstraint("search_bsdflags",pp->conlist);

fplus = 0;
fminus = 0;

if (!ParseFlagString(s.bsdflags,&fplus,&fminus))
   {
   CfOut(cf_error,"","Problem validating a BSD flag string");
   PromiseRef(cf_error,pp);
   }

if (s.name||s.path||s.filetypes||s.issymlinkto||s.perms||s.bsdflags)
   {
   entries = true;
   }

s.owners = (struct Rlist *)GetConstraint("search_owners",pp->conlist,CF_LIST);
s.groups = (struct Rlist *)GetConstraint("search_groups",pp->conlist,CF_LIST);

value = (char *)GetConstraint("search_size",pp->conlist,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,(long *)&s.min_size,(long *)&s.max_size,pp);
value = (char *)GetConstraint("ctime",pp->conlist,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,(long *)&s.min_ctime,(long *)&s.max_ctime,pp);
value = (char *)GetConstraint("atime",pp->conlist,CF_SCALAR);
if (value)
   {
   entries++;
   }
IntRange2Int(value,(long *)&s.min_atime,(long *)&s.max_atime,pp);
value = (char *)GetConstraint("mtime",pp->conlist,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,(long *)&s.min_mtime,(long *)&s.max_mtime,pp);

s.exec_regex = (char *)GetConstraint("exec_regex",pp->conlist,CF_SCALAR);
s.exec_program = (char *)GetConstraint("exec_program",pp->conlist,CF_SCALAR);

if (s.owners||s.min_size||s.exec_regex||s.exec_program)
   {
   entries = true;
   }

if ((s.result = (char *)GetConstraint("file_result",pp->conlist,CF_SCALAR)) == NULL)
   {
   if (!entries)
      {
      CfOut(cf_error,""," !! file_select body missing its a file_result return value");
      }
   }

return s;
}

/*******************************************************************/

struct TransactionContext GetTransactionConstraints(struct Promise *pp)

{ static struct TransactionContext t;
 char *value;

value = GetConstraint("action_policy",pp->conlist,CF_SCALAR);

if (value && ((strcmp(value,"warn") == 0)||(strcmp(value,"nop") == 0)))
   {
   t.action = cfa_warn;
   }
else
   {
   t.action = cfa_fix; // default
   }

t.background = GetBooleanConstraint("background",pp->conlist);
t.ifelapsed = GetIntConstraint("ifelapsed",pp->conlist);

if (t.ifelapsed == CF_NOINT)
   {
   t.ifelapsed = VIFELAPSED;
   }

t.expireafter = GetIntConstraint("expireafter",pp->conlist);

if (t.expireafter == CF_NOINT)
   {
   t.expireafter = VEXPIREAFTER;
   }

t.audit = GetBooleanConstraint("audit",pp->conlist);
t.log_string = GetConstraint("log_string",pp->conlist,CF_SCALAR);

t.log_kept = GetConstraint("log_kept",pp->conlist,CF_SCALAR);
t.log_repaired = GetConstraint("log_repaired",pp->conlist,CF_SCALAR);
t.log_failed = GetConstraint("log_failed",pp->conlist,CF_SCALAR);

value = GetConstraint("log_level",pp->conlist,CF_SCALAR);
t.log_level = String2ReportLevel(value);

value = GetConstraint("report_level",pp->conlist,CF_SCALAR);
t.report_level = String2ReportLevel(value);

t.measure_id = GetConstraint("measurement_class",pp->conlist,CF_SCALAR);

return t;
}

/*******************************************************************/

struct DefineClasses GetClassDefinitionConstraints(struct Promise *pp)

{ static struct DefineClasses c;
 char *pt = NULL;

c.change = (struct Rlist *)GetListConstraint("promise_repaired",pp->conlist);
c.failure = (struct Rlist *)GetListConstraint("repair_failed",pp->conlist);
c.denied = (struct Rlist *)GetListConstraint("reapir_denied",pp->conlist);
c.timeout = (struct Rlist *)GetListConstraint("repair_timeout",pp->conlist);
c.kept = (struct Rlist *)GetListConstraint("promise_kept",pp->conlist);
c.interrupt = (struct Rlist *)GetListConstraint("on_interrupt",pp->conlist);

c.persist = GetIntConstraint("persist_time",pp->conlist);

if (c.persist == CF_NOINT)
   {
   c.persist = 0;
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

{ static struct FileDelete f;
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

{ static struct FileRename r;
  char *value;

value = (char *)GetConstraint("disable_mode",pp->conlist,CF_SCALAR);

if (!ParseModeString(value,&r.plus,&r.minus))
   {
   CfOut(cf_error,"","Problem validating a mode string");
   PromiseRef(cf_error,pp);
   }

r.disable = GetBooleanConstraint("disable",pp->conlist);
r.disable_suffix = (char *)GetConstraint("disable_suffix",pp->conlist,CF_SCALAR);
r.newname = (char *)GetConstraint("newname",pp->conlist,CF_SCALAR);
r.rotate = GetIntConstraint("rotate",pp->conlist);

return r;
}

/*******************************************************************/

struct FileChange GetChangeMgtConstraints(struct Promise *pp)

{ static struct FileChange c;
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

value = (char *)GetConstraint("report_changes",pp->conlist,CF_SCALAR);

if (value && strcmp(value,"content") == 0)
   {
   c.report_changes = cfa_contentchange;
   }
else if (value && strcmp(value,"stats") == 0)
   {
   c.report_changes = cfa_statschange;
   }
else if (value && strcmp(value,"all") == 0)
   {
   c.report_changes = cfa_allchanges;
   }
else
   {
   c.report_changes = cfa_noreport;
   }

if (GetConstraint("update_hashes",pp->conlist,CF_SCALAR))
   {
   c.update = GetBooleanConstraint("update_hashes",pp->conlist);
   }
else
   {
   c.update = CHECKSUMUPDATES;
   }


return c;
}

/*******************************************************************/

struct FileCopy GetCopyConstraints(struct Promise *pp)

{ static struct FileCopy f;
  char *value;
  long min,max;

f.source = (char *)GetConstraint("source",pp->conlist,CF_SCALAR);

value = (char *)GetConstraint("compare",pp->conlist,CF_SCALAR);

if (value == NULL)
   {
   value = DEFAULT_COPYTYPE;
   }

f.compare = String2Comparison(value);

value = (char *)GetConstraint("link_type",pp->conlist,CF_SCALAR);

f.link_type = String2LinkType(value);
f.servers = GetListConstraint("servers",pp->conlist);
f.portnumber = (short)GetIntConstraint("portnumber",pp->conlist);
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
f.collapse = GetBooleanConstraint("collapse_destination_dir",pp->conlist);
f.preserve = GetBooleanConstraint("preserve",pp->conlist);
f.type_check = GetBooleanConstraint("type_check",pp->conlist);
f.force_update = GetBooleanConstraint("force_update",pp->conlist);
f.force_ipv4 = GetBooleanConstraint("force_ipv4",pp->conlist);
f.check_root = GetBooleanConstraint("check_root",pp->conlist);

value = (char *)GetConstraint("copy_size",pp->conlist,CF_SCALAR);
IntRange2Int(value,&min,&max,pp);

f.min_size = (size_t) min;
f.max_size = (size_t) max;

f.trustkey = GetBooleanConstraint("trustkey",pp->conlist);
f.encrypt = GetBooleanConstraint("encrypt",pp->conlist);
f.verify = GetBooleanConstraint("verify",pp->conlist);
f.purge = GetBooleanConstraint("purge",pp->conlist);
f.destination = NULL;
return f;
}

/*******************************************************************/

struct FileLink GetLinkConstraints(struct Promise *pp)

{ static struct FileLink f;
  char *value;
  
f.source = (char *)GetConstraint("source",pp->conlist,CF_SCALAR);
value = (char *)GetConstraint("link_type",pp->conlist,CF_SCALAR);
f.link_type = String2LinkType(value);
f.copy_patterns = GetListConstraint("copy_patterns",pp->conlist);

value = (char *)GetConstraint("when_no_source",pp->conlist,CF_SCALAR);

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

value = (char *)GetConstraint("when_linking_children",pp->conlist,CF_SCALAR);

if (value && strcmp(value,"override_file") == 0)
   {
   f.when_linking_children = cfa_override;
   }
else
   {
   f.when_linking_children = cfa_onlynonexisting;
   }

f.link_children = GetBooleanConstraint("link_children",pp->conlist);

return f;
}

/*******************************************************************/

struct EditDefaults GetEditDefaults(struct Promise *pp)

{ static struct EditDefaults e;
  char *value;

e.maxfilesize = GetIntConstraint("max_file_size",pp->conlist);

if (e.maxfilesize == CF_NOINT || e.maxfilesize == 0)
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

{ static struct Context a;
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

struct Packages GetPackageConstraints(struct Promise *pp)

{ static struct Packages p;
  enum package_actions action;
  enum version_cmp operator;
  enum action_policy change_policy;
  char *value;

p.have_package_methods = GetBooleanConstraint("havepackage_method",pp->conlist);
p.package_version = (char *)GetConstraint("package_version",pp->conlist,CF_SCALAR);
p.package_architectures = GetListConstraint("package_architectures",pp->conlist);

action = Str2PackageAction((char *)GetConstraint("package_policy",pp->conlist,CF_SCALAR));
p.package_policy = action;
  
operator = Str2PackageSelect((char *)GetConstraint("package_select",pp->conlist,CF_SCALAR));
p.package_select = operator;
change_policy = Str2ActionPolicy((char *)GetConstraint("package_changes",pp->conlist,CF_SCALAR));
p.package_changes = change_policy;

p.package_file_repositories = GetListConstraint("package_file_repositories",pp->conlist);


p.package_patch_list_command = (char *)GetConstraint("package_patch_list_command",pp->conlist,CF_SCALAR);
p.package_patch_name_regex = (char *)GetConstraint("package_patch_name_regex",pp->conlist,CF_SCALAR);
p.package_patch_arch_regex = (char *)GetConstraint("package_patch_arch_regex",pp->conlist,CF_SCALAR);
p.package_patch_version_regex = (char *)GetConstraint("package_patch_version_regex",pp->conlist,CF_SCALAR);
p.package_patch_installed_regex = (char *)GetConstraint("package_patch_installed_regex",pp->conlist,CF_SCALAR);

p.package_list_command = (char *)GetConstraint("package_list_command",pp->conlist,CF_SCALAR);
p.package_list_version_regex = (char *)GetConstraint("package_list_version_regex",pp->conlist,CF_SCALAR);
p.package_list_name_regex = (char *)GetConstraint("package_list_name_regex",pp->conlist,CF_SCALAR);
p.package_list_arch_regex = (char *)GetConstraint("package_list_arch_regex",pp->conlist,CF_SCALAR);

p.package_installed_regex = (char *)GetConstraint("package_installed_regex",pp->conlist,CF_SCALAR);

p.package_version_regex = (char *)GetConstraint("package_version_regex",pp->conlist,CF_SCALAR);
p.package_name_regex = (char *)GetConstraint("package_name_regex",pp->conlist,CF_SCALAR);
p.package_arch_regex = (char *)GetConstraint("package_arch_regex",pp->conlist,CF_SCALAR);


p.package_add_command = (char *)GetConstraint("package_add_command",pp->conlist,CF_SCALAR);
p.package_delete_command = (char *)GetConstraint("package_delete_command",pp->conlist,CF_SCALAR);
p.package_update_command = (char *)GetConstraint("package_update_command",pp->conlist,CF_SCALAR);
p.package_patch_command = (char *)GetConstraint("package_patch_command",pp->conlist,CF_SCALAR);
p.package_verify_command = (char *)GetConstraint("package_verify_command",pp->conlist,CF_SCALAR);
p.package_noverify_regex = (char *)GetConstraint("package_noverify_regex",pp->conlist,CF_SCALAR);
p.package_noverify_returncode = GetIntConstraint("package_noverify_returncode",pp->conlist);

p.package_name_convention = (char *)GetConstraint("package_name_convention",pp->conlist,CF_SCALAR);
return p;
}

/*******************************************************************/

struct ProcessSelect GetProcessFilterConstraints(struct Promise *pp)

{ static struct ProcessSelect p;
  char *value;
  int entries = 0;
   
p.owner = GetListConstraint("process_owner",pp->conlist);

value = (char *)GetConstraint("pid",pp->conlist,CF_SCALAR);

if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_pid,&p.max_pid,pp);
value = (char *)GetConstraint("ppid",pp->conlist,CF_SCALAR);

if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_ppid,&p.max_ppid,pp);
value = (char *)GetConstraint("pgid",pp->conlist,CF_SCALAR);

if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_pgid,&p.max_pgid,pp);
value = (char *)GetConstraint("rsize",pp->conlist,CF_SCALAR);

if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_rsize,&p.max_rsize,pp);
value = (char *)GetConstraint("vsize",pp->conlist,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_rsize,&p.max_rsize,pp);
value = (char *)GetConstraint("ttime_range",pp->conlist,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,(long *)&p.min_ttime,(long *)&p.max_ttime,pp);
value = (char *)GetConstraint("stime_range",pp->conlist,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,(long *)&p.min_stime,(long *)&p.max_stime,pp);

p.status = (char *)GetConstraint("status",pp->conlist,CF_SCALAR);
p.command = (char *)GetConstraint("command",pp->conlist,CF_SCALAR);
p.tty = (char *)GetConstraint("tty",pp->conlist,CF_SCALAR);

value = (char *)GetConstraint("priority",pp->conlist,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_pri,&p.max_pri,pp);
value = (char *)GetConstraint("threads",pp->conlist,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_thread,&p.max_thread,pp);

if (p.owner||p.status||p.command||p.tty)
   {
   entries = true;
   }

if ((p.process_result = (char *)GetConstraint("process_result",pp->conlist,CF_SCALAR)) == NULL)
   {
   if (entries)
      {
      CfOut(cf_error,""," !! process_select body missing its a process_result return value");
      }
   }

return p;
}


/*******************************************************************/

struct ProcessCount GetMatchesConstraints(struct Promise *pp)

{ static struct ProcessCount p;
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

{ static struct Attributes attr;

attr.havelocation = GetBooleanConstraint("location",pp->conlist);
attr.location = GetLocationAttributes(pp);

attr.sourcetype = GetConstraint("insert_type",pp->conlist,CF_SCALAR);
attr.expandvars = GetBooleanConstraint("expand_scalars",pp->conlist);

attr.haveinsertselect = GetBooleanConstraint("insert_select",pp->conlist);
attr.line_select = GetInsertSelectConstraints(pp);

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

{ static struct EditLocation e;
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

{ static struct Attributes attr;

attr.not_matching = GetBooleanConstraint("not_matching",pp->conlist);

attr.havedeleteselect = GetBooleanConstraint("delete_select",pp->conlist);
attr.line_select = GetDeleteSelectConstraints(pp);

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

{ static struct Attributes attr;

attr.havecolumn = GetBooleanConstraint("edit_field",pp->conlist);
attr.column = GetColumnConstraints(pp);

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

{ static struct Attributes attr;

attr.havereplace = GetBooleanConstraint("replace_patterns",pp->conlist);
attr.replace = GetReplaceConstraints(pp);

attr.havecolumn = GetBooleanConstraint("replace_with",pp->conlist);


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

{ static struct EditRegion e;

e.select_start = GetConstraint("select_start",pp->conlist,CF_SCALAR);
e.select_end = GetConstraint("select_end",pp->conlist,CF_SCALAR);
 
return e;
}

/*******************************************************************/

struct EditReplace GetReplaceConstraints(struct Promise *pp)

{ static struct EditReplace r;

r.replace_value = GetConstraint("replace_value",pp->conlist,CF_SCALAR);
r.occurrences = GetConstraint("occurrences",pp->conlist,CF_SCALAR);
 
return r;
}

/*******************************************************************/

struct EditColumn GetColumnConstraints(struct Promise *pp)

{ static struct EditColumn c;
 char *value;

c.column_separator = GetConstraint("field_separator",pp->conlist,CF_SCALAR);
c.select_column = GetIntConstraint("select_field",pp->conlist);
value = GetConstraint("value_separator",pp->conlist,CF_SCALAR);

if (value)
   {
   c.value_separator = *value;
   }
else
   {
   c.value_separator = '\0';
   }

c.column_value = GetConstraint("field_value",pp->conlist,CF_SCALAR);
c.column_operation = GetConstraint("field_operation",pp->conlist,CF_SCALAR);
c.extend_columns = GetBooleanConstraint("extend_fields",pp->conlist);
c.blanks_ok = GetBooleanConstraint("allow_blank_fields",pp->conlist);
return c;
}

/*******************************************************************/
/* Storage                                                         */
/*******************************************************************/

struct StorageMount GetMountConstraints(struct Promise *pp)

{ static struct StorageMount m;

m.mount_type = GetConstraint("mount_type",pp->conlist,CF_SCALAR);
m.mount_source = GetConstraint("mount_source",pp->conlist,CF_SCALAR);
m.mount_server = GetConstraint("mount_server",pp->conlist,CF_SCALAR);
m.mount_options = GetListConstraint("mount_options",pp->conlist);
m.editfstab = GetBooleanConstraint("edit_fstab",pp->conlist);
m.unmount = GetBooleanConstraint("unmount",pp->conlist);

return m;
}

/*******************************************************************/

struct StorageVolume GetVolumeConstraints(struct Promise *pp)

{ static struct StorageVolume v;
  char *value;

v.check_foreign = GetBooleanConstraint("check_foreign",pp->conlist);
value = GetConstraint("freespace",pp->conlist,CF_SCALAR);

v.freespace = (int) Str2Int(value);
value = GetConstraint("sensible_size",pp->conlist,CF_SCALAR);
v.sensible_size = (int) Str2Int(value);
value = GetConstraint("sensible_count",pp->conlist,CF_SCALAR);
v.sensible_count = (int) Str2Int(value);
v.scan_arrivals = GetBooleanConstraint("scan_arrivals",pp->conlist);

return v;
}

/*******************************************************************/

struct CfTcpIp GetTCPIPAttributes(struct Promise *pp)

{ static struct CfTcpIp t;

t.ipv4_address = GetConstraint("ipv4_address",pp->conlist,CF_SCALAR);
t.ipv4_netmask = GetConstraint("ipv4_netmask",pp->conlist,CF_SCALAR);

return t;
}

/*******************************************************************/

struct Report GetReportConstraints(struct Promise *pp)

{ static struct Report r;

if (GetConstraint("lastseen",pp->conlist,CF_SCALAR))
   {
   r.havelastseen = true;
   r.lastseen = GetIntConstraint("lastseen",pp->conlist);
   
   if (r.lastseen == CF_NOINT)
      {
      r.lastseen = 0;
      }
   }
else
   {
   r.havelastseen = false;
   r.lastseen = 0;
   }

r.intermittency = GetRealConstraint("intermittency",pp->conlist);

if (r.intermittency == CF_NODOUBLE)
   {
   r.intermittency = 0;
   }

r.haveprintfile = GetBooleanConstraint("printfile",pp->conlist);
r.filename = (char *)GetConstraint("file_to_print",pp->conlist,CF_SCALAR);
r.numlines = GetIntConstraint("num_lines",pp->conlist);

if (r.numlines == CF_NOINT)
   {
   r.numlines = 5;
   }

r.showstate = GetListConstraint("showstate",pp->conlist);

r.friend_pattern = GetConstraint("friend_pattern",pp->conlist,CF_SCALAR);

r.to_file = GetConstraint("report_to_file",pp->conlist,CF_SCALAR);

return r;
}

/*******************************************************************/

struct LineSelect GetInsertSelectConstraints(struct Promise *pp)

{ static struct LineSelect s;

s.startwith_from_list = GetListConstraint("insert_if_startwith_from_list",pp->conlist);
s.not_startwith_from_list = GetListConstraint("insert_if_not_startwith_from_list",pp->conlist);
s.match_from_list = GetListConstraint("insert_if_match_from_list",pp->conlist);
s.not_match_from_list = GetListConstraint("insert_if_not_match_from_list",pp->conlist);
s.contains_from_list = GetListConstraint("insert_if_contains_from_list",pp->conlist);
s.not_contains_from_list = GetListConstraint("insert_if_not_contains_from_list",pp->conlist);

return s;
}

/*******************************************************************/

struct LineSelect GetDeleteSelectConstraints(struct Promise *pp)

{ static struct LineSelect s;

s.startwith_from_list = GetListConstraint("delete_if_startwith_from_list",pp->conlist);
s.not_startwith_from_list = GetListConstraint("delete_if_not_startwith_from_list",pp->conlist);
s.match_from_list = GetListConstraint("delete_if_match_from_list",pp->conlist);
s.not_match_from_list = GetListConstraint("delete_if_not_match_from_list",pp->conlist);
s.contains_from_list = GetListConstraint("delete_if_contains_from_list",pp->conlist);
s.not_contains_from_list = GetListConstraint("delete_if_not_contains_from_list",pp->conlist);

return s;
}

/*******************************************************************/

struct Measurement GetMeasurementConstraint(struct Promise *pp)

{ static struct Measurement m;
  char *value;
 
m.stream_type = GetConstraint("stream_type",pp->conlist,CF_SCALAR);

value = GetConstraint("data_type",pp->conlist,CF_SCALAR);
m.data_type = Typename2Datatype(value);

if (m.data_type == cf_notype)
   {
   m.data_type = cf_str;
   }

m.history_type = GetConstraint("history_type",pp->conlist,CF_SCALAR);
m.select_line_matching = GetConstraint("select_line_matching",pp->conlist,CF_SCALAR);
m.select_line_number = GetIntConstraint("select_line_number",pp->conlist);
    
m.extraction_regex = GetConstraint("extraction_regex",pp->conlist,CF_SCALAR);
m.units = GetConstraint("units",pp->conlist,CF_SCALAR);
return m;
}

/*******************************************************************/

struct CfDatabase GetDatabaseConstraints(struct Promise *pp)

{ static struct CfDatabase d;
  char *value;

d.db_server_owner = GetConstraint("db_server_owner",pp->conlist,CF_SCALAR);
d.db_server_password = GetConstraint("db_server_password",pp->conlist,CF_SCALAR);
d.db_server_host = GetConstraint("db_server_host",pp->conlist,CF_SCALAR);
d.db_connect_db = GetConstraint("db_server_connection_db",pp->conlist,CF_SCALAR);
d.type = GetConstraint("database_type",pp->conlist,CF_SCALAR);
d.server = GetConstraint("database_server",pp->conlist,CF_SCALAR);
d.columns = GetListConstraint("database_columns",pp->conlist);
d.rows = GetListConstraint("database_rows",pp->conlist);
d.operation = GetConstraint("database_operation",pp->conlist,CF_SCALAR);
d.exclude = GetListConstraint("registry_exclude",pp->conlist);

value = GetConstraint("db_server_type",pp->conlist,CF_SCALAR);
d.db_server_type = Str2dbType(value);

if (value && d.db_server_type == cfd_notype)
   {
   CfOut(cf_error,"","Unsupported database type \"%s\" in databases promise",value);
   PromiseRef(cf_error,pp);
   }

return d;
}
