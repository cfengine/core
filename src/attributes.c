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

static void ShowAttributes(struct Attributes a);

/*******************************************************************/

struct Attributes GetFilesAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};
 
memset(&attr,0,sizeof(attr));

// default for file copy
 
attr.havedepthsearch = GetBooleanConstraint("depth_search",pp);
attr.haveselect = GetBooleanConstraint("file_select",pp);
attr.haverename = GetBooleanConstraint("rename",pp);
attr.havedelete = GetBooleanConstraint("delete",pp);
attr.haveperms = GetBooleanConstraint("perms",pp);
attr.havechange = GetBooleanConstraint("changes",pp);
attr.havecopy = GetBooleanConstraint("copy_from",pp);
attr.havelink = GetBooleanConstraint("link_from",pp);
attr.haveeditline = GetBundleConstraint("edit_line",pp);
attr.haveeditxml = GetBundleConstraint("edit_xml",pp);
attr.haveedit = attr.haveeditline || attr.haveeditxml;

/* Files, specialist */

attr.repository = (char *)GetConstraint("repository",pp,CF_SCALAR);
attr.create = GetBooleanConstraint("create",pp);
attr.touch = GetBooleanConstraint("touch",pp);
attr.transformer = (char *)GetConstraint("transformer",pp,CF_SCALAR);
attr.move_obstructions = GetBooleanConstraint("move_obstructions",pp);
attr.pathtype = (char *)GetConstraint("pathtype",pp,CF_SCALAR);

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

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp);
attr.transaction = GetTransactionConstraints(pp);
attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp);
attr.classes = GetClassDefinitionConstraints(pp);

if (DEBUG)
   {
   ShowAttributes(attr);
   }

if (attr.haverename || attr.havedelete || attr.haveperms || attr.havechange ||
    attr.havecopy || attr.havelink || attr.haveedit || attr.create || attr.touch ||
    attr.transformer || attr.acl.acl_entries)
   {
   }
else
   {
   if (THIS_AGENT_TYPE == cf_common)
      {
      cfPS(cf_error,CF_WARN,"",pp,attr," !! files promise makes no intention about system state");
      }
   }

if ((THIS_AGENT_TYPE == cf_common) && attr.create && attr.havecopy)
   {
   if (attr.copy.compare != cfa_checksum && attr.copy.compare != cfa_hash)
      {
      CfOut(cf_error,""," !! Promise constraint conflicts - %s file will never be copied as created file is always newer",pp->promiser);
      PromiseRef(cf_error,pp);
      }
   else
      {      
      CfOut(cf_verbose,""," !! Promise constraint conflicts - %s file cannot strictly both be created empty and copied from a source file.",pp->promiser);
      }
   }

if ((THIS_AGENT_TYPE == cf_common) && attr.create && attr.havelink)
   {
   CfOut(cf_error,""," !! Promise constraint conflicts - %s cannot be created and linked at the same time",pp->promiser);
   PromiseRef(cf_error,pp);
   }

return attr;
}

/*******************************************************************/

struct Attributes GetOutputsAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.transaction = GetTransactionConstraints(pp);
attr.classes = GetClassDefinitionConstraints(pp);

attr.output.promiser_type = GetConstraint("promiser_type",pp,CF_SCALAR);
attr.output.level = GetConstraint("output_level",pp,CF_SCALAR);
return attr;
}

/*******************************************************************/

struct Attributes GetReportsAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.transaction = GetTransactionConstraints(pp);
attr.classes = GetClassDefinitionConstraints(pp);

attr.report = GetReportConstraints(pp);
return attr;
}

/*******************************************************************/

struct Attributes GetEnvironmentsAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.transaction = GetTransactionConstraints(pp);
attr.classes = GetClassDefinitionConstraints(pp);
attr.env = GetEnvironmentsConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetServicesAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.transaction = GetTransactionConstraints(pp);
attr.classes = GetClassDefinitionConstraints(pp);
attr.service = GetServicesConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetPackageAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};
 
attr.transaction = GetTransactionConstraints(pp);
attr.classes = GetClassDefinitionConstraints(pp);
attr.packages = GetPackageConstraints(pp);
return attr;
}

/*******************************************************************/

struct Attributes GetDatabaseAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.transaction = GetTransactionConstraints(pp);
attr.classes = GetClassDefinitionConstraints(pp);
attr.database = GetDatabaseConstraints(pp);
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

{ struct Attributes attr = {{0}};

attr.contain = GetExecContainConstraints(pp);
attr.havecontain = GetBooleanConstraint("contain",pp);

attr.args = GetConstraint("args",pp,CF_SCALAR);
attr.module = GetBooleanConstraint("module",pp);

/* Common ("included") */

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetProcessAttributes(struct Promise *pp)

{ static struct Attributes attr = {{0}};

attr.signals = GetListConstraint("signals",pp);
attr.process_stop = (char *)GetConstraint("process_stop",pp,CF_SCALAR);
attr.haveprocess_count = GetBooleanConstraint("process_count",pp);
attr.haveselect = GetBooleanConstraint("process_select",pp);
attr.restart_class = (char *)GetConstraint("restart_class",pp,CF_SCALAR);

attr.process_count = GetMatchesConstraints(pp);
attr.process_select = GetProcessFilterConstraints(pp);

/* Common ("included") */

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetStorageAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.mount = GetMountConstraints(pp);
attr.volume = GetVolumeConstraints(pp);
attr.havevolume = GetBooleanConstraint("volume",pp);
attr.havemount = GetBooleanConstraint("mount",pp);

/* Common ("included") */

if (attr.edits.maxfilesize <= 0)
   {
   attr.edits.maxfilesize = EDITFILESIZE;
   }

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetMethodAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.havebundle = GetBundleConstraint("usebundle",pp);

/* Common ("included") */

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetInterfacesAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.havetcpip = GetBundleConstraint("usebundle",pp);
attr.tcpip = GetTCPIPAttributes(pp);
    
/* Common ("included") */

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetTopicsAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.fwd_name = GetConstraint("forward_relationship",pp,CF_SCALAR);
attr.bwd_name = GetConstraint("backward_relationship",pp,CF_SCALAR);
attr.associates = GetListConstraint("associates",pp);
attr.synonyms = GetListConstraint("synonyms",pp);
attr.general = GetListConstraint("generalizations",pp);
return attr;
}

/*******************************************************************/

struct Attributes GetThingsAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};
  struct Rlist *rp;
  char *cert = GetConstraint("certainty",pp,CF_SCALAR);
  enum knowledgecertainty certainty;

attr.synonyms = GetListConstraint("synonyms",pp);
attr.general = GetListConstraint("generalizations",pp);

if (cert && strcmp(cert,"possible") == 0)
   {
   certainty = cfk_possible;
   }
else if (cert && strcmp(cert,"uncertain") == 0)
   {
   certainty = cfk_uncertain;
   }
else
   {
   certainty = cfk_certain;
   }

// Select predefined physics

if (rp = GetListConstraint("is_part_of",pp))
   {
   switch (certainty)
      {
      case cfk_certain:
          attr.fwd_name = KM_PARTOF_CERT_F;
          attr.bwd_name = KM_PARTOF_CERT_B;
          break;
      case cfk_uncertain:
          attr.fwd_name = KM_PARTOF_UNCERT_F;
          attr.bwd_name = KM_PARTOF_UNCERT_B;
          break;
      case cfk_possible:
          attr.fwd_name = KM_PARTOF_POSS_F;
          attr.bwd_name = KM_PARTOF_POSS_B;
          break;
      }

   attr.associates = rp;
   }
else if (rp = GetListConstraint("determines",pp))
   {
   switch (certainty)
      {
      case cfk_certain:
          attr.fwd_name = KM_DETERMINES_CERT_F;
          attr.bwd_name = KM_DETERMINES_CERT_B;
          break;
      case cfk_uncertain:
          attr.fwd_name = KM_DETERMINES_UNCERT_F;
          attr.bwd_name = KM_DETERMINES_UNCERT_B;
          break;
      case cfk_possible:
          attr.fwd_name = KM_DETERMINES_POSS_F;
          attr.bwd_name = KM_DETERMINES_POSS_B;
          break;
      }

   attr.associates = rp;
   }
else if (rp = GetListConstraint("is_connected_to",pp))
   {
   switch (certainty)
      {
      case cfk_certain:
          attr.fwd_name = KM_CONNECTS_CERT_F;
          attr.bwd_name = KM_CONNECTS_CERT_B;
          break;
      case cfk_uncertain:
          attr.fwd_name = KM_CONNECTS_UNCERT_F;
          attr.bwd_name = KM_CONNECTS_UNCERT_B;
          break;
      case cfk_possible:
          attr.fwd_name = KM_CONNECTS_POSS_F;
          attr.bwd_name = KM_CONNECTS_POSS_B;
          break;
      }

   attr.associates = rp;
   }
else if (rp = GetListConstraint("uses",pp))
   {
   switch (certainty)
      {
      case cfk_certain:
          attr.fwd_name = KM_USES_CERT_F;
          attr.bwd_name = KM_USES_CERT_B;
          break;
      case cfk_uncertain:
          attr.fwd_name = KM_USES_UNCERT_F;
          attr.bwd_name = KM_USES_UNCERT_B;
          break;
      case cfk_possible:
          attr.fwd_name = KM_USES_POSS_F;
          attr.bwd_name = KM_USES_POSS_B;
          break;
      }

   attr.associates = rp;
   }
else if (rp = GetListConstraint("provides",pp))
   {
   switch (certainty)
      {
      case cfk_certain:
          attr.fwd_name = KM_PROVIDES_CERT_F;
          attr.bwd_name = KM_PROVIDES_CERT_B;   
          break;
      case cfk_uncertain:
          attr.fwd_name = KM_PROVIDES_UNCERT_F;
          attr.bwd_name = KM_PROVIDES_UNCERT_B;   
          break;
      case cfk_possible:
          attr.fwd_name = KM_PROVIDES_POSS_F;
          attr.bwd_name = KM_PROVIDES_POSS_B;   
          break;
      }

   attr.associates = rp;
   }
else if (rp = GetListConstraint("belongs_to",pp))
   {
   switch (certainty)
      {
      case cfk_certain:
          attr.fwd_name = KM_BELONGS_CERT_F;
          attr.bwd_name = KM_BELONGS_CERT_B;   
          break;
      case cfk_uncertain:
          attr.fwd_name = KM_BELONGS_UNCERT_F;
          attr.bwd_name = KM_BELONGS_UNCERT_B;   
          break;
      case cfk_possible:
          attr.fwd_name = KM_BELONGS_POSS_F;
          attr.bwd_name = KM_BELONGS_POSS_B;   
          break;
      }

   attr.associates = rp;
   }
else if (rp = GetListConstraint("affects",pp))
   {
   switch (certainty)
      {
      case cfk_certain:
          attr.fwd_name = KM_AFFECTS_CERT_F;
          attr.bwd_name = KM_AFFECTS_CERT_B;   
          break;
      case cfk_uncertain:
          attr.fwd_name = KM_AFFECTS_UNCERT_F;
          attr.bwd_name = KM_AFFECTS_UNCERT_B;   
          break;
      case cfk_possible:
          attr.fwd_name = KM_AFFECTS_POSS_F;
          attr.bwd_name = KM_AFFECTS_POSS_B;   
          break;
      }

   attr.associates = rp;
   }
else if (rp = GetListConstraint("causes",pp))
   {
   switch (certainty)
      {
      case cfk_certain:
          attr.fwd_name = KM_CAUSE_CERT_F;
          attr.bwd_name = KM_CAUSE_CERT_B;   
          break;
      case cfk_uncertain:
          attr.fwd_name = KM_CAUSE_UNCERT_F;
          attr.bwd_name = KM_CAUSE_UNCERT_B;   
          break;
      case cfk_possible:
          attr.fwd_name = KM_CAUSE_POSS_F;
          attr.bwd_name = KM_CAUSE_POSS_B;   
          break;
      }

   attr.associates = rp;
   }
else if (rp = GetListConstraint("caused_by",pp))
   {
   switch (certainty)
      {
      case cfk_certain:
          attr.bwd_name = KM_CAUSE_CERT_F;
          attr.fwd_name = KM_CAUSE_CERT_B;   
          break;
      case cfk_uncertain:
          attr.bwd_name = KM_CAUSE_UNCERT_F;
          attr.fwd_name = KM_CAUSE_UNCERT_B;   
          break;
      case cfk_possible:
          attr.bwd_name = KM_CAUSE_POSS_F;
          attr.fwd_name = KM_CAUSE_POSS_B;   
          break;
      }

   attr.associates = rp;
   }
else if (rp = GetListConstraint("needs",pp))
   {
   switch (certainty)
      {
      case cfk_certain:
          attr.fwd_name = KM_NEEDS_CERT_F;
          attr.bwd_name = KM_NEEDS_CERT_B;   
          break;
      case cfk_uncertain:
          attr.fwd_name = KM_NEEDS_UNCERT_F;
          attr.bwd_name = KM_NEEDS_UNCERT_B;   
          break;
      case cfk_possible:
          attr.fwd_name = KM_NEEDS_POSS_F;
          attr.bwd_name = KM_NEEDS_POSS_B;   
          break;
      }

   attr.associates = rp;
   }

return attr;
}

/*******************************************************************/

struct Attributes GetInferencesAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.precedents = GetListConstraint("precedents",pp);
attr.qualifiers = GetListConstraint("qualifers",pp);
return attr;
}

/*******************************************************************/

struct Attributes GetOccurrenceAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.represents = GetListConstraint("represents",pp);
attr.rep_type = GetConstraint("representation",pp,CF_SCALAR);
attr.web_root = GetConstraint("web_root",pp,CF_SCALAR);
attr.path_root = GetConstraint("path_root",pp,CF_SCALAR);

return attr;
}

/*******************************************************************/

struct Attributes GetMeasurementAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.measure = GetMeasurementConstraint(pp);
    
/* Common ("included") */

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

struct CfServices GetServicesConstraints(struct Promise *pp)

{ struct CfServices s;
 
s.service_type = GetConstraint("service_type",pp,CF_SCALAR);
s.service_policy = Str2ServicePolicy(GetConstraint("service_policy",pp,CF_SCALAR));
s.service_autostart_policy = GetConstraint("service_autostart_policy",pp,CF_SCALAR);
s.service_args = GetConstraint("service_args",pp,CF_SCALAR);
s.service_depend = GetListConstraint("service_dependencies",pp);
s.service_depend_chain = GetConstraint("service_dependence_chain",pp,CF_SCALAR);

return s;
}

/*******************************************************************/

struct CfEnvironments GetEnvironmentsConstraints(struct Promise *pp)

{ struct CfEnvironments e;

e.cpus = GetIntConstraint("env_cpus",pp);
e.memory = GetIntConstraint("env_memory",pp);
e.disk = GetIntConstraint("env_disk",pp);
e.baseline = GetConstraint("env_baseline",pp,CF_SCALAR);
e.specfile = GetConstraint("env_spec_file",pp,CF_SCALAR);
e.host = GetConstraint("environment_host",pp,CF_SCALAR);

e.addresses = GetListConstraint("env_addresses",pp);
e.name = GetConstraint("env_name",pp,CF_SCALAR);
e.type = GetConstraint("environment_type",pp,CF_SCALAR);
e.state = Str2EnvState(GetConstraint("environment_state",pp,CF_SCALAR));

return e;
}

/*******************************************************************/

struct ExecContain GetExecContainConstraints(struct Promise *pp)

{ struct ExecContain e;
 
e.useshell = GetBooleanConstraint("useshell",pp);
e.umask = GetOctalConstraint("umask",pp);
e.owner = GetUidConstraint("exec_owner",pp);
e.group = GetGidConstraint("exec_group",pp);
e.preview = GetBooleanConstraint("preview",pp);
e.nooutput = GetBooleanConstraint("no_output",pp);
e.timeout = GetIntConstraint("exec_timeout",pp);
e.chroot = GetConstraint("chroot",pp,CF_SCALAR);
e.chdir = GetConstraint("chdir",pp,CF_SCALAR);

return e;
}

/*******************************************************************/

struct Recursion GetRecursionConstraints(struct Promise *pp)

{ struct Recursion r;
 
r.travlinks = GetBooleanConstraint("traverse_links",pp);
r.rmdeadlinks = GetBooleanConstraint("rmdeadlinks",pp);
r.depth = GetIntConstraint("depth",pp);

if (r.depth == CF_NOINT)
   {
   r.depth = 0;
   }

r.xdev = GetBooleanConstraint("xdev",pp);
r.include_dirs = GetListConstraint("include_dirs",pp);
r.exclude_dirs = GetListConstraint("exclude_dirs",pp);
r.include_basedir = GetBooleanConstraint("include_basedir",pp);
return r;
}

/*******************************************************************/

struct CfACL GetAclConstraints(struct Promise *pp)

{ struct CfACL ac;

ac.acl_method = Str2AclMethod(GetConstraint("acl_method",pp,CF_SCALAR));
ac.acl_type = Str2AclType(GetConstraint("acl_type",pp,CF_SCALAR));
ac.acl_directory_inherit = Str2AclInherit(GetConstraint("acl_directory_inherit",pp,CF_SCALAR));
ac.acl_entries = GetListConstraint("aces",pp);
ac.acl_inherit_entries = GetListConstraint("specify_inherit_aces",pp);
return ac;
}

/*******************************************************************/

struct FilePerms GetPermissionConstraints(struct Promise *pp)

{ struct FilePerms p;
  char *value;
  struct Rlist *list;
                
value = (char *)GetConstraint("mode",pp,CF_SCALAR);

p.plus = CF_SAMEMODE;
p.minus = CF_SAMEMODE;

if (!ParseModeString(value,&p.plus,&p.minus))
   {
   CfOut(cf_error,"","Problem validating a mode string");
   PromiseRef(cf_error,pp);
   }

list = GetListConstraint("bsdflags",pp);

p.plus_flags = 0;
p.minus_flags = 0;

if (list && !ParseFlagString(list,&p.plus_flags,&p.minus_flags))
   {
   CfOut(cf_error,"","Problem validating a BSD flag string");
   PromiseRef(cf_error,pp);
   }

#ifdef MINGW
p.owners = NovaWin_Rlist2SidList((struct Rlist *)GetConstraint("owners",pp,CF_LIST),pp);
#else  /* NOT MINGW */
p.owners = Rlist2UidList((struct Rlist *)GetConstraint("owners",pp,CF_LIST),pp);
p.groups = Rlist2GidList((struct Rlist *)GetConstraint("groups",pp,CF_LIST),pp);
#endif  /* NOT MINGW */

p.findertype = (char *)GetConstraint("findertype",pp,CF_SCALAR);
p.rxdirs = GetBooleanConstraint("rxdirs",pp);

// The default should be true

if (!GetConstraint("rxdirs",pp,CF_SCALAR))
   {
   p.rxdirs = true;
   }

return p;
}

/*******************************************************************/

struct FileSelect GetSelectConstraints(struct Promise *pp)

{ struct FileSelect s;
  char *value;
  struct Rlist *rp;
  mode_t plus,minus;
  u_long fplus,fminus;
  int entries = false;
  
s.name = (struct Rlist *)GetConstraint("leaf_name",pp,CF_LIST);
s.path = (struct Rlist *)GetConstraint("path_name",pp,CF_LIST);
s.filetypes = (struct Rlist *)GetConstraint("file_types",pp,CF_LIST);
s.issymlinkto = (struct Rlist *)GetConstraint("issymlinkto",pp,CF_LIST);

s.perms = GetListConstraint("search_mode",pp);

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

s.bsdflags = GetListConstraint("search_bsdflags",pp);

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

s.owners = (struct Rlist *)GetConstraint("search_owners",pp,CF_LIST);
s.groups = (struct Rlist *)GetConstraint("search_groups",pp,CF_LIST);

value = (char *)GetConstraint("search_size",pp,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,(long *)&s.min_size,(long *)&s.max_size,pp);

value = (char *)GetConstraint("ctime",pp,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,(long *)&s.min_ctime,(long *)&s.max_ctime,pp);
value = (char *)GetConstraint("atime",pp,CF_SCALAR);
if (value)
   {
   entries++;
   }
IntRange2Int(value,(long *)&s.min_atime,(long *)&s.max_atime,pp);
value = (char *)GetConstraint("mtime",pp,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,(long *)&s.min_mtime,(long *)&s.max_mtime,pp);

s.exec_regex = (char *)GetConstraint("exec_regex",pp,CF_SCALAR);
s.exec_program = (char *)GetConstraint("exec_program",pp,CF_SCALAR);

if (s.owners||s.min_size||s.exec_regex||s.exec_program)
   {
   entries = true;
   }

if ((s.result = (char *)GetConstraint("file_result",pp,CF_SCALAR)) == NULL)
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

{ struct TransactionContext t;
 char *value;

value = GetConstraint("action_policy",pp,CF_SCALAR);

if (value && ((strcmp(value,"warn") == 0)||(strcmp(value,"nop") == 0)))
   {
   t.action = cfa_warn;
   }
else
   {
   t.action = cfa_fix; // default
   }

t.background = GetBooleanConstraint("background",pp);
t.ifelapsed = GetIntConstraint("ifelapsed",pp);

if (t.ifelapsed == CF_NOINT)
   {
   t.ifelapsed = VIFELAPSED;
   }

t.expireafter = GetIntConstraint("expireafter",pp);

if (t.expireafter == CF_NOINT)
   {
   t.expireafter = VEXPIREAFTER;
   }

t.audit = GetBooleanConstraint("audit",pp);
t.log_string = GetConstraint("log_string",pp,CF_SCALAR);
t.log_priority = SyslogPriority2Int(GetConstraint("log_priority",pp,CF_SCALAR));

t.log_kept = GetConstraint("log_kept",pp,CF_SCALAR);
t.log_repaired = GetConstraint("log_repaired",pp,CF_SCALAR);
t.log_failed = GetConstraint("log_failed",pp,CF_SCALAR);

if ((t.value_kept = GetRealConstraint("value_kept",pp)) == CF_NODOUBLE)
   {
   t.value_kept = 1.0;
   }

if ((t.value_repaired = GetRealConstraint("value_repaired",pp)) == CF_NODOUBLE)
   {
   t.value_repaired = 0.5;
   }

if ((t.value_notkept = GetRealConstraint("value_notkept",pp)) == CF_NODOUBLE)
   {
   t.value_notkept = -1.0;
   }

value = GetConstraint("log_level",pp,CF_SCALAR);
t.log_level = String2ReportLevel(value);

value = GetConstraint("report_level",pp,CF_SCALAR);
t.report_level = String2ReportLevel(value);

t.measure_id = GetConstraint("measurement_class",pp,CF_SCALAR);

return t;
}

/*******************************************************************/

struct DefineClasses GetClassDefinitionConstraints(struct Promise *pp)

{ struct DefineClasses c;
 char *pt = NULL;

c.change = (struct Rlist *)GetListConstraint("promise_repaired",pp);
c.failure = (struct Rlist *)GetListConstraint("repair_failed",pp);
c.denied = (struct Rlist *)GetListConstraint("repair_denied",pp);
c.timeout = (struct Rlist *)GetListConstraint("repair_timeout",pp);
c.kept = (struct Rlist *)GetListConstraint("promise_kept",pp);
c.interrupt = (struct Rlist *)GetListConstraint("on_interrupt",pp);

c.del_change = (struct Rlist *)GetListConstraint("cancel_repaired",pp);
c.del_kept = (struct Rlist *)GetListConstraint("cancel_kept",pp);
c.del_notkept = (struct Rlist *)GetListConstraint("cancel_notkept",pp);

c.retcode_kept = (struct Rlist *)GetListConstraint("kept_returncodes",pp);
c.retcode_repaired = (struct Rlist *)GetListConstraint("repaired_returncodes",pp);
c.retcode_failed = (struct Rlist *)GetListConstraint("failed_returncodes",pp);

c.persist = GetIntConstraint("persist_time",pp);

if (c.persist == CF_NOINT)
   {
   c.persist = 0;
   }

pt = GetConstraint("timer_policy",pp,CF_SCALAR);

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

value = (char *)GetConstraint("dirlinks",pp,CF_SCALAR);

if (value && strcmp(value,"keep") == 0)
   {
   f.dirlinks = cfa_linkkeep;
   }
else
   {
   f.dirlinks = cfa_linkdelete;
   }

f.rmdirs = GetBooleanConstraint("rmdirs",pp);
return f;
}

/*******************************************************************/

struct FileRename GetRenameConstraints(struct Promise *pp)

{ struct FileRename r;
  char *value;

value = (char *)GetConstraint("disable_mode",pp,CF_SCALAR);

if (!ParseModeString(value,&r.plus,&r.minus))
   {
   CfOut(cf_error,"","Problem validating a mode string");
   PromiseRef(cf_error,pp);
   }

r.disable = GetBooleanConstraint("disable",pp);
r.disable_suffix = (char *)GetConstraint("disable_suffix",pp,CF_SCALAR);
r.newname = (char *)GetConstraint("newname",pp,CF_SCALAR);
r.rotate = GetIntConstraint("rotate",pp);

return r;
}

/*******************************************************************/

struct FileChange GetChangeMgtConstraints(struct Promise *pp)

{ struct FileChange c;
  char *value;

value = (char *)GetConstraint("hash",pp,CF_SCALAR);

if (value && strcmp(value,"best") == 0)
   {
#ifdef HAVE_NOVA
   c.hash = cf_sha512;
#else
   c.hash = cf_besthash;
#endif   
   }
else if (value && strcmp(value,"md5") == 0)
   {
   c.hash = cf_md5;
   }
else if (value && strcmp(value,"sha1") == 0)
   {
   c.hash = cf_sha1;
   }
else if (value && strcmp(value,"sha256") == 0)
   {
   c.hash = cf_sha256;
   }
else if (value && strcmp(value,"sha384") == 0)
   {
   c.hash = cf_sha384;
   }
else if (value && strcmp(value,"sha512") == 0)
   {
   c.hash = cf_sha512;
   }
else
   {
   c.hash = CF_DEFAULT_DIGEST;
   }

if (FIPS_MODE && c.hash == cf_md5)
   {
   CfOut(cf_error,""," !! FIPS mode is enabled, and md5 is not an approved algorithm");
   PromiseRef(cf_error,pp);
   }

value = (char *)GetConstraint("report_changes",pp,CF_SCALAR);

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

if (GetConstraint("update_hashes",pp,CF_SCALAR))
   {
   c.update = GetBooleanConstraint("update_hashes",pp);
   }
else
   {
   c.update = CHECKSUMUPDATES;
   }

c.report_diffs = GetBooleanConstraint("report_diffs",pp);
return c;
}

/*******************************************************************/

struct FileCopy GetCopyConstraints(struct Promise *pp)

{ struct FileCopy f;
  char *value;
  long min,max;

f.source = (char *)GetConstraint("source",pp,CF_SCALAR);

value = (char *)GetConstraint("compare",pp,CF_SCALAR);

if (value == NULL)
   {
   value = DEFAULT_COPYTYPE;
   }

f.compare = String2Comparison(value);

value = (char *)GetConstraint("link_type",pp,CF_SCALAR);

f.link_type = String2LinkType(value);
f.servers = GetListConstraint("servers",pp);
f.portnumber = (short)GetIntConstraint("portnumber",pp);
f.timeout = (short)GetIntConstraint("timeout",pp);
f.link_instead = GetListConstraint("linkcopy_patterns",pp);
f.copy_links = GetListConstraint("copylink_patterns",pp);

value = (char *)GetConstraint("copy_backup",pp,CF_SCALAR);

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
       
f.stealth = GetBooleanConstraint("stealth",pp);
f.collapse = GetBooleanConstraint("collapse_destination_dir",pp);
f.preserve = GetBooleanConstraint("preserve",pp);
f.type_check = GetBooleanConstraint("type_check",pp);
f.force_update = GetBooleanConstraint("force_update",pp);
f.force_ipv4 = GetBooleanConstraint("force_ipv4",pp);
f.check_root = GetBooleanConstraint("check_root",pp);

value = (char *)GetConstraint("copy_size",pp,CF_SCALAR);
IntRange2Int(value,&min,&max,pp);

f.min_size = (size_t) min;
f.max_size = (size_t) max;

f.trustkey = GetBooleanConstraint("trustkey",pp);
f.encrypt = GetBooleanConstraint("encrypt",pp);
f.verify = GetBooleanConstraint("verify",pp);
f.purge = GetBooleanConstraint("purge",pp);
f.destination = NULL;

return f;
}

/*******************************************************************/

struct FileLink GetLinkConstraints(struct Promise *pp)

{ struct FileLink f;
  char *value;
  
f.source = (char *)GetConstraint("source",pp,CF_SCALAR);
value = (char *)GetConstraint("link_type",pp,CF_SCALAR);
f.link_type = String2LinkType(value);
f.copy_patterns = GetListConstraint("copy_patterns",pp);

value = (char *)GetConstraint("when_no_source",pp,CF_SCALAR);

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

value = (char *)GetConstraint("when_linking_children",pp,CF_SCALAR);

if (value && strcmp(value,"override_file") == 0)
   {
   f.when_linking_children = cfa_override;
   }
else
   {
   f.when_linking_children = cfa_onlynonexisting;
   }

f.link_children = GetBooleanConstraint("link_children",pp);

return f;
}

/*******************************************************************/

struct EditDefaults GetEditDefaults(struct Promise *pp)

{ struct EditDefaults e;
  char *value;

e.maxfilesize = GetIntConstraint("max_file_size",pp);

if (e.maxfilesize == CF_NOINT || e.maxfilesize == 0)
   {
   e.maxfilesize = EDITFILESIZE;
   }

value = (char *)GetConstraint("edit_backup",pp,CF_SCALAR);

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

e.empty_before_use = GetBooleanConstraint("empty_file_before_editing",pp);

e.joinlines = GetBooleanConstraint("recognize_join",pp);

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

struct Packages GetPackageConstraints(struct Promise *pp)

{ struct Packages p;
  enum package_actions action;
  enum version_cmp operator;
  enum action_policy change_policy;

p.have_package_methods = GetBooleanConstraint("havepackage_method",pp);
p.package_version = (char *)GetConstraint("package_version",pp,CF_SCALAR);
p.package_architectures = GetListConstraint("package_architectures",pp);

action = Str2PackageAction((char *)GetConstraint("package_policy",pp,CF_SCALAR));
p.package_policy = action;
  
operator = Str2PackageSelect((char *)GetConstraint("package_select",pp,CF_SCALAR));
p.package_select = operator;
change_policy = Str2ActionPolicy((char *)GetConstraint("package_changes",pp,CF_SCALAR));
p.package_changes = change_policy;

p.package_file_repositories = GetListConstraint("package_file_repositories",pp);


p.package_patch_list_command = (char *)GetConstraint("package_patch_list_command",pp,CF_SCALAR);
p.package_patch_name_regex = (char *)GetConstraint("package_patch_name_regex",pp,CF_SCALAR);
p.package_patch_arch_regex = (char *)GetConstraint("package_patch_arch_regex",pp,CF_SCALAR);
p.package_patch_version_regex = (char *)GetConstraint("package_patch_version_regex",pp,CF_SCALAR);
p.package_patch_installed_regex = (char *)GetConstraint("package_patch_installed_regex",pp,CF_SCALAR);

p.package_list_update_command = (char *)GetConstraint("package_list_update_command",pp,CF_SCALAR);
p.package_list_update_ifelapsed = GetIntConstraint("package_list_update_ifelapsed",pp);
p.package_list_command = (char *)GetConstraint("package_list_command",pp,CF_SCALAR);
p.package_list_version_regex = (char *)GetConstraint("package_list_version_regex",pp,CF_SCALAR);
p.package_list_name_regex = (char *)GetConstraint("package_list_name_regex",pp,CF_SCALAR);
p.package_list_arch_regex = (char *)GetConstraint("package_list_arch_regex",pp,CF_SCALAR);

p.package_installed_regex = (char *)GetConstraint("package_installed_regex",pp,CF_SCALAR);

p.package_version_regex = (char *)GetConstraint("package_version_regex",pp,CF_SCALAR);
p.package_name_regex = (char *)GetConstraint("package_name_regex",pp,CF_SCALAR);
p.package_arch_regex = (char *)GetConstraint("package_arch_regex",pp,CF_SCALAR);


p.package_add_command = (char *)GetConstraint("package_add_command",pp,CF_SCALAR);
p.package_delete_command = (char *)GetConstraint("package_delete_command",pp,CF_SCALAR);
p.package_update_command = (char *)GetConstraint("package_update_command",pp,CF_SCALAR);
p.package_patch_command = (char *)GetConstraint("package_patch_command",pp,CF_SCALAR);
p.package_verify_command = (char *)GetConstraint("package_verify_command",pp,CF_SCALAR);
p.package_noverify_regex = (char *)GetConstraint("package_noverify_regex",pp,CF_SCALAR);
p.package_noverify_returncode = GetIntConstraint("package_noverify_returncode",pp);

p.package_name_convention = (char *)GetConstraint("package_name_convention",pp,CF_SCALAR);
p.package_delete_convention = (char *)GetConstraint("package_delete_convention",pp,CF_SCALAR);

p.package_multiline_start = (char *)GetConstraint("package_multiline_start",pp,CF_SCALAR);
return p;
}

/*******************************************************************/

struct ProcessSelect GetProcessFilterConstraints(struct Promise *pp)

{ struct ProcessSelect p;
  char *value;
  int entries = 0;
   
p.owner = GetListConstraint("process_owner",pp);

value = (char *)GetConstraint("pid",pp,CF_SCALAR);

if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_pid,&p.max_pid,pp);
value = (char *)GetConstraint("ppid",pp,CF_SCALAR);

if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_ppid,&p.max_ppid,pp);
value = (char *)GetConstraint("pgid",pp,CF_SCALAR);

if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_pgid,&p.max_pgid,pp);
value = (char *)GetConstraint("rsize",pp,CF_SCALAR);

if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_rsize,&p.max_rsize,pp);
value = (char *)GetConstraint("vsize",pp,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_vsize,&p.max_vsize,pp);
value = (char *)GetConstraint("ttime_range",pp,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,(long *)&p.min_ttime,(long *)&p.max_ttime,pp);
value = (char *)GetConstraint("stime_range",pp,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,(long *)&p.min_stime,(long *)&p.max_stime,pp);

p.status = (char *)GetConstraint("status",pp,CF_SCALAR);
p.command = (char *)GetConstraint("command",pp,CF_SCALAR);
p.tty = (char *)GetConstraint("tty",pp,CF_SCALAR);

value = (char *)GetConstraint("priority",pp,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_pri,&p.max_pri,pp);
value = (char *)GetConstraint("threads",pp,CF_SCALAR);
if (value)
   {
   entries++;
   }

IntRange2Int(value,&p.min_thread,&p.max_thread,pp);

if (p.owner||p.status||p.command||p.tty)
   {
   entries = true;
   }

if ((p.process_result = (char *)GetConstraint("process_result",pp,CF_SCALAR)) == NULL)
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

{ struct ProcessCount p;
  char *value;

value = (char *)GetConstraint("match_range",pp,CF_SCALAR);
IntRange2Int(value,&p.min_range,&p.max_range,pp);
p.in_range_define = GetListConstraint("in_range_define",pp);
p.out_of_range_define = GetListConstraint("out_of_range_define",pp);

return p;
}

/*******************************************************************/

static void ShowAttributes(struct Attributes a)

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

{ struct Attributes attr = {{0}};

attr.havelocation = GetBooleanConstraint("location",pp);
attr.location = GetLocationAttributes(pp);

attr.sourcetype = GetConstraint("insert_type",pp,CF_SCALAR);
attr.expandvars = GetBooleanConstraint("expand_scalars",pp);

attr.haveinsertselect = GetBooleanConstraint("insert_select",pp);
attr.line_select = GetInsertSelectConstraints(pp);

attr.insert_match = GetListConstraint("whitespace_policy",pp);

/* Common ("included") */

attr.haveregion = GetBooleanConstraint("select_region",pp);
attr.region = GetRegionConstraints(pp);

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct EditLocation GetLocationAttributes(struct Promise *pp)

{ struct EditLocation e;
  char *value;

e.line_matching = GetConstraint("select_line_matching",pp,CF_SCALAR);

value = GetConstraint("before_after",pp,CF_SCALAR);

if (value && strcmp(value,"before") == 0)
   {
   e.before_after = cfe_before;
   }
else
   {
   e.before_after = cfe_after;
   }

e.first_last = GetConstraint("first_last",pp,CF_SCALAR);
return e;
}

/*******************************************************************/

struct Attributes GetDeletionAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.not_matching = GetBooleanConstraint("not_matching",pp);

attr.havedeleteselect = GetBooleanConstraint("delete_select",pp);
attr.line_select = GetDeleteSelectConstraints(pp);

 /* common */

attr.haveregion = GetBooleanConstraint("select_region",pp);
attr.region = GetRegionConstraints(pp);

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetColumnAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.havecolumn = GetBooleanConstraint("edit_field",pp);
attr.column = GetColumnConstraints(pp);

 /* common */

attr.haveregion = GetBooleanConstraint("select_region",pp);
attr.region = GetRegionConstraints(pp);

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct Attributes GetReplaceAttributes(struct Promise *pp)

{ struct Attributes attr = {{0}};

attr.havereplace = GetBooleanConstraint("replace_patterns",pp);
attr.replace = GetReplaceConstraints(pp);

attr.havecolumn = GetBooleanConstraint("replace_with",pp);


 /* common */

attr.haveregion = GetBooleanConstraint("select_region",pp);
attr.region = GetRegionConstraints(pp);

attr.havetrans = GetBooleanConstraint(CF_TRANSACTION,pp);
attr.transaction = GetTransactionConstraints(pp);

attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES,pp);
attr.classes = GetClassDefinitionConstraints(pp);

return attr;
}

/*******************************************************************/

struct EditRegion GetRegionConstraints(struct Promise *pp)

{ struct EditRegion e;

e.select_start = GetConstraint("select_start",pp,CF_SCALAR);
e.select_end = GetConstraint("select_end",pp,CF_SCALAR);
e.include_start = GetBooleanConstraint("include_start_delimiter",pp);
e.include_end = GetBooleanConstraint("include_end_delimiter",pp); 
return e;
}

/*******************************************************************/

struct EditReplace GetReplaceConstraints(struct Promise *pp)

{ struct EditReplace r;

r.replace_value = GetConstraint("replace_value",pp,CF_SCALAR);
r.occurrences = GetConstraint("occurrences",pp,CF_SCALAR);
 
return r;
}

/*******************************************************************/

struct EditColumn GetColumnConstraints(struct Promise *pp)

{ struct EditColumn c;
 char *value;

c.column_separator = GetConstraint("field_separator",pp,CF_SCALAR);
c.select_column = GetIntConstraint("select_field",pp);

if (c.select_column != CF_NOINT && GetBooleanConstraint("start_fields_from_zero",pp))
   {
   c.select_column++;
   }

value = GetConstraint("value_separator",pp,CF_SCALAR);

if (value)
   {
   c.value_separator = *value;
   }
else
   {
   c.value_separator = '\0';
   }

c.column_value = GetConstraint("field_value",pp,CF_SCALAR);
c.column_operation = GetConstraint("field_operation",pp,CF_SCALAR);
c.extend_columns = GetBooleanConstraint("extend_fields",pp);
c.blanks_ok = GetBooleanConstraint("allow_blank_fields",pp);
return c;
}

/*******************************************************************/
/* Storage                                                         */
/*******************************************************************/

struct StorageMount GetMountConstraints(struct Promise *pp)

{ struct StorageMount m;

m.mount_type = GetConstraint("mount_type",pp,CF_SCALAR);
m.mount_source = GetConstraint("mount_source",pp,CF_SCALAR);
m.mount_server = GetConstraint("mount_server",pp,CF_SCALAR);
m.mount_options = GetListConstraint("mount_options",pp);
m.editfstab = GetBooleanConstraint("edit_fstab",pp);
m.unmount = GetBooleanConstraint("unmount",pp);

return m;
}

/*******************************************************************/

struct StorageVolume GetVolumeConstraints(struct Promise *pp)

{ struct StorageVolume v;
  char *value;

v.check_foreign = GetBooleanConstraint("check_foreign",pp);
value = GetConstraint("freespace",pp,CF_SCALAR);

v.freespace = (long) Str2Int(value);
value = GetConstraint("sensible_size",pp,CF_SCALAR);
v.sensible_size = (int) Str2Int(value);
value = GetConstraint("sensible_count",pp,CF_SCALAR);
v.sensible_count = (int) Str2Int(value);
v.scan_arrivals = GetBooleanConstraint("scan_arrivals",pp);

// defaults
 if(v.sensible_size == CF_NOINT)
   {
     v.sensible_size = 1000;
   }
 
 if(v.sensible_count == CF_NOINT)
   {
     v.sensible_count = 2;
   }


return v;
}

/*******************************************************************/

struct CfTcpIp GetTCPIPAttributes(struct Promise *pp)

{ struct CfTcpIp t;

t.ipv4_address = GetConstraint("ipv4_address",pp,CF_SCALAR);
t.ipv4_netmask = GetConstraint("ipv4_netmask",pp,CF_SCALAR);

return t;
}

/*******************************************************************/

struct Report GetReportConstraints(struct Promise *pp)

{ struct Report r;

if (GetConstraint("lastseen",pp,CF_SCALAR))
   {
   r.havelastseen = true;
   r.lastseen = GetIntConstraint("lastseen",pp);
   
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

r.intermittency = GetRealConstraint("intermittency",pp);

if (r.intermittency == CF_NODOUBLE)
   {
   r.intermittency = 0;
   }

r.haveprintfile = GetBooleanConstraint("printfile",pp);
r.filename = (char *)GetConstraint("file_to_print",pp,CF_SCALAR);
r.numlines = GetIntConstraint("number_of_lines",pp);

if (r.numlines == CF_NOINT)
   {
   r.numlines = 5;
   }

r.showstate = GetListConstraint("showstate",pp);

r.friend_pattern = GetConstraint("friend_pattern",pp,CF_SCALAR);

r.to_file = GetConstraint("report_to_file",pp,CF_SCALAR);

return r;
}

/*******************************************************************/

struct LineSelect GetInsertSelectConstraints(struct Promise *pp)

{ struct LineSelect s;

s.startwith_from_list = GetListConstraint("insert_if_startwith_from_list",pp);
s.not_startwith_from_list = GetListConstraint("insert_if_not_startwith_from_list",pp);
s.match_from_list = GetListConstraint("insert_if_match_from_list",pp);
s.not_match_from_list = GetListConstraint("insert_if_not_match_from_list",pp);
s.contains_from_list = GetListConstraint("insert_if_contains_from_list",pp);
s.not_contains_from_list = GetListConstraint("insert_if_not_contains_from_list",pp);

return s;
}

/*******************************************************************/

struct LineSelect GetDeleteSelectConstraints(struct Promise *pp)

{ struct LineSelect s;

s.startwith_from_list = GetListConstraint("delete_if_startwith_from_list",pp);
s.not_startwith_from_list = GetListConstraint("delete_if_not_startwith_from_list",pp);
s.match_from_list = GetListConstraint("delete_if_match_from_list",pp);
s.not_match_from_list = GetListConstraint("delete_if_not_match_from_list",pp);
s.contains_from_list = GetListConstraint("delete_if_contains_from_list",pp);
s.not_contains_from_list = GetListConstraint("delete_if_not_contains_from_list",pp);

return s;
}

/*******************************************************************/

struct Measurement GetMeasurementConstraint(struct Promise *pp)

{ struct Measurement m;
  char *value;
 
m.stream_type = GetConstraint("stream_type",pp,CF_SCALAR);

value = GetConstraint("data_type",pp,CF_SCALAR);
m.data_type = Typename2Datatype(value);

if (m.data_type == cf_notype)
   {
   m.data_type = cf_str;
   }

m.history_type = GetConstraint("history_type",pp,CF_SCALAR);
m.select_line_matching = GetConstraint("select_line_matching",pp,CF_SCALAR);
m.select_line_number = GetIntConstraint("select_line_number",pp);
    
m.extraction_regex = GetConstraint("extraction_regex",pp,CF_SCALAR);
m.units = GetConstraint("units",pp,CF_SCALAR);
m.growing = GetBooleanConstraint("track_growing_file",pp);
return m;
}

/*******************************************************************/

struct CfDatabase GetDatabaseConstraints(struct Promise *pp)

{ struct CfDatabase d;
  char *value;

d.db_server_owner = GetConstraint("db_server_owner",pp,CF_SCALAR);
d.db_server_password = GetConstraint("db_server_password",pp,CF_SCALAR);
d.db_server_host = GetConstraint("db_server_host",pp,CF_SCALAR);
d.db_connect_db = GetConstraint("db_server_connection_db",pp,CF_SCALAR);
d.type = GetConstraint("database_type",pp,CF_SCALAR);
d.server = GetConstraint("database_server",pp,CF_SCALAR);
d.columns = GetListConstraint("database_columns",pp);
d.rows = GetListConstraint("database_rows",pp);
d.operation = GetConstraint("database_operation",pp,CF_SCALAR);
d.exclude = GetListConstraint("registry_exclude",pp);

value = GetConstraint("db_server_type",pp,CF_SCALAR);
d.db_server_type = Str2dbType(value);

if (value && d.db_server_type == cfd_notype)
   {
   CfOut(cf_error,"","Unsupported database type \"%s\" in databases promise",value);
   PromiseRef(cf_error,pp);
   }

return d;
}
