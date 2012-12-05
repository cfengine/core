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

#include "attributes.h"

#include "promises.h"
#include "constraints.h"
#include "conversion.h"
#include "cfstream.h"

static void ShowAttributes(Attributes a);

/*******************************************************************/

static int CHECKSUMUPDATES;

/*******************************************************************/

void SetChecksumUpdates(bool enabled)
{
    CHECKSUMUPDATES = enabled;
}

/*******************************************************************/

Attributes GetFilesAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    memset(&attr, 0, sizeof(attr));

// default for file copy

    attr.havedepthsearch = GetBooleanConstraint("depth_search", pp);
    attr.haveselect = GetBooleanConstraint("file_select", pp);
    attr.haverename = GetBooleanConstraint("rename", pp);
    attr.havedelete = GetBooleanConstraint("delete", pp);
    attr.haveperms = GetBooleanConstraint("perms", pp);
    attr.havechange = GetBooleanConstraint("changes", pp);
    attr.havecopy = GetBooleanConstraint("copy_from", pp);
    attr.havelink = GetBooleanConstraint("link_from", pp);

    attr.template = (char *)GetConstraintValue("edit_template", pp, CF_SCALAR);
    attr.haveeditline = GetBundleConstraint("edit_line", pp);
    attr.haveeditxml = GetBundleConstraint("edit_xml", pp);
    attr.haveedit = (attr.haveeditline) || (attr.haveeditxml) || (attr.template);

/* Files, specialist */

    attr.repository = (char *) GetConstraintValue("repository", pp, CF_SCALAR);
    attr.create = GetBooleanConstraint("create", pp);
    attr.touch = GetBooleanConstraint("touch", pp);
    attr.transformer = (char *) GetConstraintValue("transformer", pp, CF_SCALAR);
    attr.move_obstructions = GetBooleanConstraint("move_obstructions", pp);
    attr.pathtype = (char *) GetConstraintValue("pathtype", pp, CF_SCALAR);

    attr.acl = GetAclConstraints(pp);
    attr.perms = GetPermissionConstraints(pp);
    attr.select = GetSelectConstraints(pp);
    attr.delete = GetDeleteConstraints(pp);
    attr.rename = GetRenameConstraints(pp);
    attr.change = GetChangeMgtConstraints(pp);
    attr.copy = GetCopyConstraints(pp);
    attr.link = GetLinkConstraints(pp);
    attr.edits = GetEditDefaults(pp);

    if (attr.template)
       {
       attr.edits.empty_before_use = true;
       attr.edits.inherit = true;
       }

/* Files, multiple use */

    attr.recursion = GetRecursionConstraints(pp);

/* Common ("included") */

    attr.havetrans = GetBooleanConstraint(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);
    attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    if (DEBUG)
    {
        ShowAttributes(attr);
    }

    if ((attr.haverename) || (attr.havedelete) || (attr.haveperms) || (attr.havechange) ||
        (attr.havecopy) || (attr.havelink) || (attr.haveedit) || (attr.create) || (attr.touch) ||
        (attr.transformer) || (attr.acl.acl_entries))
    {
    }
    else
    {
        if (THIS_AGENT_TYPE == AGENT_TYPE_COMMON)
        {
            cfPS(cf_error, CF_WARN, "", pp, attr, " !! files promise makes no intention about system state");
        }
    }

    if ((THIS_AGENT_TYPE == AGENT_TYPE_COMMON) && (attr.create) && (attr.havecopy))
    {
        if (((attr.copy.compare) != (cfa_checksum)) && ((attr.copy.compare) != cfa_hash))
        {
            CfOut(cf_error, "",
                  " !! Promise constraint conflicts - %s file will never be copied as created file is always newer",
                  pp->promiser);
            PromiseRef(cf_error, pp);
        }
        else
        {
            CfOut(cf_verbose, "",
                  " !! Promise constraint conflicts - %s file cannot strictly both be created empty and copied from a source file.",
                  pp->promiser);
        }
    }

    if ((THIS_AGENT_TYPE == AGENT_TYPE_COMMON) && (attr.create) && (attr.havelink))
    {
        CfOut(cf_error, "", " !! Promise constraint conflicts - %s cannot be created and linked at the same time",
              pp->promiser);
        PromiseRef(cf_error, pp);
    }

    return attr;
}

/*******************************************************************/

Attributes GetOutputsAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    attr.output.promiser_type = GetConstraintValue("promiser_type", pp, CF_SCALAR);
    attr.output.level = GetConstraintValue("output_level", pp, CF_SCALAR);
    return attr;
}

/*******************************************************************/

Attributes GetReportsAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    attr.report = GetReportConstraints(pp);
    return attr;
}

/*******************************************************************/

Attributes GetEnvironmentsAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(pp);
    attr.classes = GetClassDefinitionConstraints(pp);
    attr.env = GetEnvironmentsConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetServicesAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(pp);
    attr.classes = GetClassDefinitionConstraints(pp);
    attr.service = GetServicesConstraints(pp);
    attr.havebundle = GetBundleConstraint("service_bundle", pp);

    return attr;
}

/*******************************************************************/

Attributes GetPackageAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(pp);
    attr.classes = GetClassDefinitionConstraints(pp);
    attr.packages = GetPackageConstraints(pp);
    return attr;
}

/*******************************************************************/

Attributes GetDatabaseAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(pp);
    attr.classes = GetClassDefinitionConstraints(pp);
    attr.database = GetDatabaseConstraints(pp);
    return attr;
}

/*******************************************************************/

Attributes GetClassContextAttributes(const Promise *pp)
{
    Attributes a = { {0} };;

    a.transaction = GetTransactionConstraints(pp);
    a.classes = GetClassDefinitionConstraints(pp);
    a.context = GetContextConstraints(pp);

    return a;
}

/*******************************************************************/

Attributes GetExecAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.contain = GetExecContainConstraints(pp);
    attr.havecontain = GetBooleanConstraint("contain", pp);

    attr.args = GetConstraintValue("args", pp, CF_SCALAR);
    attr.module = GetBooleanConstraint("module", pp);

/* Common ("included") */

    attr.havetrans = GetBooleanConstraint(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetProcessAttributes(const Promise *pp)
{
    static Attributes attr = { {0} };

    attr.signals = GetListConstraint("signals", pp);
    attr.process_stop = (char *) GetConstraintValue("process_stop", pp, CF_SCALAR);
    attr.haveprocess_count = GetBooleanConstraint("process_count", pp);
    attr.haveselect = GetBooleanConstraint("process_select", pp);
    attr.restart_class = (char *) GetConstraintValue("restart_class", pp, CF_SCALAR);

    attr.process_count = GetMatchesConstraints(pp);
    attr.process_select = GetProcessFilterConstraints(pp);

/* Common ("included") */

    attr.havetrans = GetBooleanConstraint(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetStorageAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.mount = GetMountConstraints(pp);
    attr.volume = GetVolumeConstraints(pp);
    attr.havevolume = GetBooleanConstraint("volume", pp);
    attr.havemount = GetBooleanConstraint("mount", pp);

/* Common ("included") */

    if (attr.edits.maxfilesize <= 0)
    {
        attr.edits.maxfilesize = EDITFILESIZE;
    }

    attr.havetrans = GetBooleanConstraint(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetMethodAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havebundle = GetBundleConstraint("usebundle", pp);

    attr.inherit = GetBooleanConstraint("inherit", pp);

/* Common ("included") */

    attr.havetrans = GetBooleanConstraint(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetInterfacesAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havetcpip = GetBundleConstraint("usebundle", pp);
    attr.tcpip = GetTCPIPAttributes(pp);

/* Common ("included") */

    attr.havetrans = GetBooleanConstraint(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetTopicsAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.fwd_name = GetConstraintValue("forward_relationship", pp, CF_SCALAR);
    attr.bwd_name = GetConstraintValue("backward_relationship", pp, CF_SCALAR);
    attr.associates = GetListConstraint("associates", pp);
    attr.synonyms = GetListConstraint("synonyms", pp);
    attr.general = GetListConstraint("generalizations", pp);
    return attr;
}

/*******************************************************************/

Attributes GetInferencesAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.precedents = GetListConstraint("precedents", pp);
    attr.qualifiers = GetListConstraint("qualifers", pp);
    return attr;
}

/*******************************************************************/

Attributes GetOccurrenceAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.represents = GetListConstraint("represents", pp);
    attr.rep_type = GetConstraintValue("representation", pp, CF_SCALAR);
    attr.about_topics = GetListConstraint("about_topics", pp);
    return attr;
}

/*******************************************************************/

Attributes GetMeasurementAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.measure = GetMeasurementConstraint(pp);

/* Common ("included") */

    attr.havetrans = GetBooleanConstraint(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

Services GetServicesConstraints(const Promise *pp)
{
    Services s;

    s.service_type = GetConstraintValue("service_type", pp, CF_SCALAR);
    s.service_policy = Str2ServicePolicy(GetConstraintValue("service_policy", pp, CF_SCALAR));
    s.service_autostart_policy = GetConstraintValue("service_autostart_policy", pp, CF_SCALAR);
    s.service_args = GetConstraintValue("service_args", pp, CF_SCALAR);
    s.service_depend = GetListConstraint("service_dependencies", pp);
    s.service_depend_chain = GetConstraintValue("service_dependence_chain", pp, CF_SCALAR);

    return s;
}

/*******************************************************************/

Environments GetEnvironmentsConstraints(const Promise *pp)
{
    Environments e;

    e.cpus = GetIntConstraint("env_cpus", pp);
    e.memory = GetIntConstraint("env_memory", pp);
    e.disk = GetIntConstraint("env_disk", pp);
    e.baseline = GetConstraintValue("env_baseline", pp, CF_SCALAR);
    e.spec = GetConstraintValue("env_spec", pp, CF_SCALAR);
    e.host = GetConstraintValue("environment_host", pp, CF_SCALAR);

    e.addresses = GetListConstraint("env_addresses", pp);
    e.name = GetConstraintValue("env_name", pp, CF_SCALAR);
    e.type = GetConstraintValue("environment_type", pp, CF_SCALAR);
    e.state = Str2EnvState(GetConstraintValue("environment_state", pp, CF_SCALAR));

    return e;
}

/*******************************************************************/

ExecContain GetExecContainConstraints(const Promise *pp)
{
    ExecContain e;

    e.useshell = GetBooleanConstraint("useshell", pp);
    e.umask = GetOctalConstraint("umask", pp);
    e.owner = GetUidConstraint("exec_owner", pp);
    e.group = GetGidConstraint("exec_group", pp);
    e.preview = GetBooleanConstraint("preview", pp);
    e.nooutput = GetBooleanConstraint("no_output", pp);
    e.timeout = GetIntConstraint("exec_timeout", pp);
    e.chroot = GetConstraintValue("chroot", pp, CF_SCALAR);
    e.chdir = GetConstraintValue("chdir", pp, CF_SCALAR);

    return e;
}

/*******************************************************************/

Recursion GetRecursionConstraints(const Promise *pp)
{
    Recursion r;

    r.travlinks = GetBooleanConstraint("traverse_links", pp);
    r.rmdeadlinks = GetBooleanConstraint("rmdeadlinks", pp);
    r.depth = GetIntConstraint("depth", pp);

    if (r.depth == CF_NOINT)
    {
        r.depth = 0;
    }

    r.xdev = GetBooleanConstraint("xdev", pp);
    r.include_dirs = GetListConstraint("include_dirs", pp);
    r.exclude_dirs = GetListConstraint("exclude_dirs", pp);
    r.include_basedir = GetBooleanConstraint("include_basedir", pp);
    return r;
}

/*******************************************************************/

Acl GetAclConstraints(const Promise *pp)
{
    Acl ac;

    ac.acl_method = Str2AclMethod(GetConstraintValue("acl_method", pp, CF_SCALAR));
    ac.acl_type = Str2AclType(GetConstraintValue("acl_type", pp, CF_SCALAR));
    ac.acl_directory_inherit = Str2AclInherit(GetConstraintValue("acl_directory_inherit", pp, CF_SCALAR));
    ac.acl_entries = GetListConstraint("aces", pp);
    ac.acl_inherit_entries = GetListConstraint("specify_inherit_aces", pp);
    return ac;
}

/*******************************************************************/

FilePerms GetPermissionConstraints(const Promise *pp)
{
    FilePerms p;
    char *value;
    Rlist *list;

    value = (char *) GetConstraintValue("mode", pp, CF_SCALAR);

    p.plus = CF_SAMEMODE;
    p.minus = CF_SAMEMODE;

    if (!ParseModeString(value, &p.plus, &p.minus))
    {
        CfOut(cf_error, "", "Problem validating a mode string");
        PromiseRef(cf_error, pp);
    }

    list = GetListConstraint("bsdflags", pp);

    p.plus_flags = 0;
    p.minus_flags = 0;

    if (list && (!ParseFlagString(list, &p.plus_flags, &p.minus_flags)))
    {
        CfOut(cf_error, "", "Problem validating a BSD flag string");
        PromiseRef(cf_error, pp);
    }

#ifdef MINGW
    p.owners = NovaWin_Rlist2SidList((Rlist *) GetConstraintValue("owners", pp, CF_LIST));
#else /* NOT MINGW */
    p.owners = Rlist2UidList((Rlist *) GetConstraintValue("owners", pp, CF_LIST), pp);
    p.groups = Rlist2GidList((Rlist *) GetConstraintValue("groups", pp, CF_LIST), pp);
#endif /* NOT MINGW */

    p.findertype = (char *) GetConstraintValue("findertype", pp, CF_SCALAR);
    p.rxdirs = GetBooleanConstraint("rxdirs", pp);

// The default should be true

    if (!GetConstraintValue("rxdirs", pp, CF_SCALAR))
    {
        p.rxdirs = true;
    }

    return p;
}

/*******************************************************************/

FileSelect GetSelectConstraints(const Promise *pp)
{
    FileSelect s;
    char *value;
    Rlist *rp;
    mode_t plus, minus;
    u_long fplus, fminus;
    int entries = false;

    s.name = (Rlist *) GetConstraintValue("leaf_name", pp, CF_LIST);
    s.path = (Rlist *) GetConstraintValue("path_name", pp, CF_LIST);
    s.filetypes = (Rlist *) GetConstraintValue("file_types", pp, CF_LIST);
    s.issymlinkto = (Rlist *) GetConstraintValue("issymlinkto", pp, CF_LIST);

    s.perms = GetListConstraint("search_mode", pp);

    for (rp = s.perms; rp != NULL; rp = rp->next)
    {
        plus = 0;
        minus = 0;
        value = (char *) rp->item;

        if (!ParseModeString(value, &plus, &minus))
        {
            CfOut(cf_error, "", "Problem validating a mode string");
            PromiseRef(cf_error, pp);
        }
    }

    s.bsdflags = GetListConstraint("search_bsdflags", pp);

    fplus = 0;
    fminus = 0;

    if (!ParseFlagString(s.bsdflags, &fplus, &fminus))
    {
        CfOut(cf_error, "", "Problem validating a BSD flag string");
        PromiseRef(cf_error, pp);
    }

    if ((s.name) || (s.path) || (s.filetypes) || (s.issymlinkto) || (s.perms) || (s.bsdflags))
    {
        entries = true;
    }

    s.owners = (Rlist *) GetConstraintValue("search_owners", pp, CF_LIST);
    s.groups = (Rlist *) GetConstraintValue("search_groups", pp, CF_LIST);

    value = (char *) GetConstraintValue("search_size", pp, CF_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, (long *) &s.min_size, (long *) &s.max_size, pp);

    value = (char *) GetConstraintValue("ctime", pp, CF_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, (long *) &s.min_ctime, (long *) &s.max_ctime, pp);
    value = (char *) GetConstraintValue("atime", pp, CF_SCALAR);
    if (value)
    {
        entries++;
    }
    IntRange2Int(value, (long *) &s.min_atime, (long *) &s.max_atime, pp);
    value = (char *) GetConstraintValue("mtime", pp, CF_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, (long *) &s.min_mtime, (long *) &s.max_mtime, pp);

    s.exec_regex = (char *) GetConstraintValue("exec_regex", pp, CF_SCALAR);
    s.exec_program = (char *) GetConstraintValue("exec_program", pp, CF_SCALAR);

    if ((s.owners) || (s.min_size) || (s.exec_regex) || (s.exec_program))
    {
        entries = true;
    }

    if ((s.result = (char *) GetConstraintValue("file_result", pp, CF_SCALAR)) == NULL)
    {
        if (!entries)
        {
            CfOut(cf_error, "", " !! file_select body missing its a file_result return value");
        }
    }

    return s;
}

/*******************************************************************/

TransactionContext GetTransactionConstraints(const Promise *pp)
{
    TransactionContext t;
    char *value;

    value = GetConstraintValue("action_policy", pp, CF_SCALAR);

    if (value && ((strcmp(value, "warn") == 0) || (strcmp(value, "nop") == 0)))
    {
        t.action = cfa_warn;
    }
    else
    {
        t.action = cfa_fix;     // default
    }

    t.background = GetBooleanConstraint("background", pp);
    t.ifelapsed = GetIntConstraint("ifelapsed", pp);

    if (t.ifelapsed == CF_NOINT)
    {
        t.ifelapsed = VIFELAPSED;
    }

    t.expireafter = GetIntConstraint("expireafter", pp);

    if (t.expireafter == CF_NOINT)
    {
        t.expireafter = VEXPIREAFTER;
    }

    t.audit = GetBooleanConstraint("audit", pp);
    t.log_string = GetConstraintValue("log_string", pp, CF_SCALAR);
    t.log_priority = SyslogPriority2Int(GetConstraintValue("log_priority", pp, CF_SCALAR));

    t.log_kept = GetConstraintValue("log_kept", pp, CF_SCALAR);
    t.log_repaired = GetConstraintValue("log_repaired", pp, CF_SCALAR);
    t.log_failed = GetConstraintValue("log_failed", pp, CF_SCALAR);

    if ((t.value_kept = GetRealConstraint("value_kept", pp)) == CF_NODOUBLE)
    {
        t.value_kept = 1.0;
    }

    if ((t.value_repaired = GetRealConstraint("value_repaired", pp)) == CF_NODOUBLE)
    {
        t.value_repaired = 0.5;
    }

    if ((t.value_notkept = GetRealConstraint("value_notkept", pp)) == CF_NODOUBLE)
    {
        t.value_notkept = -1.0;
    }

    value = GetConstraintValue("log_level", pp, CF_SCALAR);
    t.log_level = String2ReportLevel(value);

    value = GetConstraintValue("report_level", pp, CF_SCALAR);
    t.report_level = String2ReportLevel(value);

    t.measure_id = GetConstraintValue("measurement_class", pp, CF_SCALAR);

    return t;
}

/*******************************************************************/

DefineClasses GetClassDefinitionConstraints(const Promise *pp)
{
    DefineClasses c;
    char *pt = NULL;

    c.change = (Rlist *) GetListConstraint("promise_repaired", pp);
    c.failure = (Rlist *) GetListConstraint("repair_failed", pp);
    c.denied = (Rlist *) GetListConstraint("repair_denied", pp);
    c.timeout = (Rlist *) GetListConstraint("repair_timeout", pp);
    c.kept = (Rlist *) GetListConstraint("promise_kept", pp);
    c.interrupt = (Rlist *) GetListConstraint("on_interrupt", pp);

    c.del_change = (Rlist *) GetListConstraint("cancel_repaired", pp);
    c.del_kept = (Rlist *) GetListConstraint("cancel_kept", pp);
    c.del_notkept = (Rlist *) GetListConstraint("cancel_notkept", pp);

    c.retcode_kept = (Rlist *) GetListConstraint("kept_returncodes", pp);
    c.retcode_repaired = (Rlist *) GetListConstraint("repaired_returncodes", pp);
    c.retcode_failed = (Rlist *) GetListConstraint("failed_returncodes", pp);

    c.persist = GetIntConstraint("persist_time", pp);

    if (c.persist == CF_NOINT)
    {
        c.persist = 0;
    }

    pt = GetConstraintValue("timer_policy", pp, CF_SCALAR);

    if (pt && (strncmp(pt, "abs", 3) == 0))
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

FileDelete GetDeleteConstraints(const Promise *pp)
{
    FileDelete f;
    char *value;

    value = (char *) GetConstraintValue("dirlinks", pp, CF_SCALAR);

    if (value && (strcmp(value, "keep") == 0))
    {
        f.dirlinks = cfa_linkkeep;
    }
    else
    {
        f.dirlinks = cfa_linkdelete;
    }

    f.rmdirs = GetBooleanConstraint("rmdirs", pp);
    return f;
}

/*******************************************************************/

FileRename GetRenameConstraints(const Promise *pp)
{
    FileRename r;
    char *value;

    value = (char *) GetConstraintValue("disable_mode", pp, CF_SCALAR);

    if (!ParseModeString(value, &r.plus, &r.minus))
    {
        CfOut(cf_error, "", "Problem validating a mode string");
        PromiseRef(cf_error, pp);
    }

    r.disable = GetBooleanConstraint("disable", pp);
    r.disable_suffix = (char *) GetConstraintValue("disable_suffix", pp, CF_SCALAR);
    r.newname = (char *) GetConstraintValue("newname", pp, CF_SCALAR);
    r.rotate = GetIntConstraint("rotate", pp);

    return r;
}

/*******************************************************************/

FileChange GetChangeMgtConstraints(const Promise *pp)
{
    FileChange c;
    char *value;

    value = (char *) GetConstraintValue("hash", pp, CF_SCALAR);

    if (value && (strcmp(value, "best") == 0))
    {
#ifdef HAVE_NOVA
        c.hash = cf_sha512;
#else
        c.hash = cf_besthash;
#endif
    }
    else if (value && (strcmp(value, "md5") == 0))
    {
        c.hash = cf_md5;
    }
    else if (value && (strcmp(value, "sha1") == 0))
    {
        c.hash = cf_sha1;
    }
    else if (value && (strcmp(value, "sha256") == 0))
    {
        c.hash = cf_sha256;
    }
    else if (value && (strcmp(value, "sha384") == 0))
    {
        c.hash = cf_sha384;
    }
    else if (value && (strcmp(value, "sha512") == 0))
    {
        c.hash = cf_sha512;
    }
    else
    {
        c.hash = CF_DEFAULT_DIGEST;
    }

    if (FIPS_MODE && (c.hash == cf_md5))
    {
        CfOut(cf_error, "", " !! FIPS mode is enabled, and md5 is not an approved algorithm");
        PromiseRef(cf_error, pp);
    }

    value = (char *) GetConstraintValue("report_changes", pp, CF_SCALAR);

    if (value && (strcmp(value, "content") == 0))
    {
        c.report_changes = cfa_contentchange;
    }
    else if (value && (strcmp(value, "stats") == 0))
    {
        c.report_changes = cfa_statschange;
    }
    else if (value && (strcmp(value, "all") == 0))
    {
        c.report_changes = cfa_allchanges;
    }
    else
    {
        c.report_changes = cfa_noreport;
    }

    if (GetConstraintValue("update_hashes", pp, CF_SCALAR))
    {
        c.update = GetBooleanConstraint("update_hashes", pp);
    }
    else
    {
        c.update = CHECKSUMUPDATES;
    }

    c.report_diffs = GetBooleanConstraint("report_diffs", pp);
    return c;
}

/*******************************************************************/

FileCopy GetCopyConstraints(const Promise *pp)
{
    FileCopy f;
    char *value;
    long min, max;

    f.source = (char *) GetConstraintValue("source", pp, CF_SCALAR);

    value = (char *) GetConstraintValue("compare", pp, CF_SCALAR);

    if (value == NULL)
    {
        value = DEFAULT_COPYTYPE;
    }

    f.compare = String2Comparison(value);

    value = (char *) GetConstraintValue("link_type", pp, CF_SCALAR);

    f.link_type = String2LinkType(value);
    f.servers = GetListConstraint("servers", pp);
    f.portnumber = (short) GetIntConstraint("portnumber", pp);
    f.timeout = (short) GetIntConstraint("timeout", pp);
    f.link_instead = GetListConstraint("linkcopy_patterns", pp);
    f.copy_links = GetListConstraint("copylink_patterns", pp);

    value = (char *) GetConstraintValue("copy_backup", pp, CF_SCALAR);

    if (value && (strcmp(value, "false") == 0))
    {
        f.backup = cfa_nobackup;
    }
    else if (value && (strcmp(value, "timestamp") == 0))
    {
        f.backup = cfa_timestamp;
    }
    else
    {
        f.backup = cfa_backup;
    }

    f.stealth = GetBooleanConstraint("stealth", pp);
    f.collapse = GetBooleanConstraint("collapse_destination_dir", pp);
    f.preserve = GetBooleanConstraint("preserve", pp);
    f.type_check = GetBooleanConstraint("type_check", pp);
    f.force_update = GetBooleanConstraint("force_update", pp);
    f.force_ipv4 = GetBooleanConstraint("force_ipv4", pp);
    f.check_root = GetBooleanConstraint("check_root", pp);

    value = (char *) GetConstraintValue("copy_size", pp, CF_SCALAR);
    IntRange2Int(value, &min, &max, pp);

    f.min_size = (size_t) min;
    f.max_size = (size_t) max;

    f.trustkey = GetBooleanConstraint("trustkey", pp);
    f.encrypt = GetBooleanConstraint("encrypt", pp);
    f.verify = GetBooleanConstraint("verify", pp);
    f.purge = GetBooleanConstraint("purge", pp);
    f.destination = NULL;

    return f;
}

/*******************************************************************/

FileLink GetLinkConstraints(const Promise *pp)
{
    FileLink f;
    char *value;

    f.source = (char *) GetConstraintValue("source", pp, CF_SCALAR);
    value = (char *) GetConstraintValue("link_type", pp, CF_SCALAR);
    f.link_type = String2LinkType(value);
    f.copy_patterns = GetListConstraint("copy_patterns", pp);

    value = (char *) GetConstraintValue("when_no_source", pp, CF_SCALAR);

    if (value && (strcmp(value, "force") == 0))
    {
        f.when_no_file = cfa_force;
    }
    else if (value && (strcmp(value, "delete") == 0))
    {
        f.when_no_file = cfa_delete;
    }
    else
    {
        f.when_no_file = cfa_skip;
    }

    value = (char *) GetConstraintValue("when_linking_children", pp, CF_SCALAR);

    if (value && (strcmp(value, "override_file") == 0))
    {
        f.when_linking_children = cfa_override;
    }
    else
    {
        f.when_linking_children = cfa_onlynonexisting;
    }

    f.link_children = GetBooleanConstraint("link_children", pp);

    return f;
}

/*******************************************************************/

EditDefaults GetEditDefaults(const Promise *pp)
{
    EditDefaults e = { 0 };
    char *value;

    e.maxfilesize = GetIntConstraint("max_file_size", pp);

    if ((e.maxfilesize == CF_NOINT) || (e.maxfilesize == 0))
    {
        e.maxfilesize = EDITFILESIZE;
    }

    value = (char *) GetConstraintValue("edit_backup", pp, CF_SCALAR);

    if (value && (strcmp(value, "false") == 0))
    {
        e.backup = cfa_nobackup;
    }
    else if (value && (strcmp(value, "timestamp") == 0))
    {
        e.backup = cfa_timestamp;
    }
    else if (value && (strcmp(value, "rotate") == 0))
    {
        e.backup = cfa_rotate;
        e.rotate = GetIntConstraint("rotate", pp);
    }
    else
    {
        e.backup = cfa_backup;
    }

    e.empty_before_use = GetBooleanConstraint("empty_file_before_editing", pp); 

    e.joinlines = GetBooleanConstraint("recognize_join", pp);

    e.inherit = GetBooleanConstraint("inherit", pp);

    return e;
}

/*******************************************************************/

ContextConstraint GetContextConstraints(const Promise *pp)
{
    ContextConstraint a;
    Constraint *cp;
    int i;

    a.nconstraints = 0;
    a.expression = NULL;
    a.persistent = GetIntConstraint("persistence", pp);

    for (cp = pp->conlist; cp != NULL; cp = cp->next)
    {
        for (i = 0; CF_CLASSBODY[i].lval != NULL; i++)
        {
            if (strcmp(cp->lval, "persistence") == 0)
            {
                continue;
            }

            if (strcmp(cp->lval, CF_CLASSBODY[i].lval) == 0)
            {
                a.expression = cp;
                a.nconstraints++;
            }
        }
    }

    return a;
}

/*******************************************************************/

Packages GetPackageConstraints(const Promise *pp)
{
    Packages p;
    enum package_actions action;
    enum version_cmp operator;
    enum action_policy change_policy;

    p.have_package_methods = GetBooleanConstraint("havepackage_method", pp);
    p.package_version = (char *) GetConstraintValue("package_version", pp, CF_SCALAR);
    p.package_architectures = GetListConstraint("package_architectures", pp);

    action = Str2PackageAction((char *) GetConstraintValue("package_policy", pp, CF_SCALAR));
    p.package_policy = action;

    if (p.package_policy == cfa_pa_none)        // Default action => package add
    {
        p.package_policy = cfa_addpack;
    }

    operator = Str2PackageSelect((char *) GetConstraintValue("package_select", pp, CF_SCALAR));

    p.package_select = operator;
    change_policy = Str2ActionPolicy((char *) GetConstraintValue("package_changes", pp, CF_SCALAR));
    p.package_changes = change_policy;

    p.package_file_repositories = GetListConstraint("package_file_repositories", pp);

    p.package_default_arch_command = (char *) GetConstraintValue("package_default_arch_command", pp, CF_SCALAR);

    p.package_patch_list_command = (char *) GetConstraintValue("package_patch_list_command", pp, CF_SCALAR);
    p.package_patch_name_regex = (char *) GetConstraintValue("package_patch_name_regex", pp, CF_SCALAR);
    p.package_patch_arch_regex = (char *) GetConstraintValue("package_patch_arch_regex", pp, CF_SCALAR);
    p.package_patch_version_regex = (char *) GetConstraintValue("package_patch_version_regex", pp, CF_SCALAR);
    p.package_patch_installed_regex = (char *) GetConstraintValue("package_patch_installed_regex", pp, CF_SCALAR);

    p.package_list_update_command = (char *) GetConstraintValue("package_list_update_command", pp, CF_SCALAR);
    p.package_list_update_ifelapsed = GetIntConstraint("package_list_update_ifelapsed", pp);
    p.package_list_command = (char *) GetConstraintValue("package_list_command", pp, CF_SCALAR);
    p.package_list_version_regex = (char *) GetConstraintValue("package_list_version_regex", pp, CF_SCALAR);
    p.package_list_name_regex = (char *) GetConstraintValue("package_list_name_regex", pp, CF_SCALAR);
    p.package_list_arch_regex = (char *) GetConstraintValue("package_list_arch_regex", pp, CF_SCALAR);

    p.package_installed_regex = (char *) GetConstraintValue("package_installed_regex", pp, CF_SCALAR);

    p.package_version_regex = (char *) GetConstraintValue("package_version_regex", pp, CF_SCALAR);
    p.package_name_regex = (char *) GetConstraintValue("package_name_regex", pp, CF_SCALAR);
    p.package_arch_regex = (char *) GetConstraintValue("package_arch_regex", pp, CF_SCALAR);

    p.package_add_command = (char *) GetConstraintValue("package_add_command", pp, CF_SCALAR);
    p.package_delete_command = (char *) GetConstraintValue("package_delete_command", pp, CF_SCALAR);
    p.package_update_command = (char *) GetConstraintValue("package_update_command", pp, CF_SCALAR);
    p.package_patch_command = (char *) GetConstraintValue("package_patch_command", pp, CF_SCALAR);
    p.package_verify_command = (char *) GetConstraintValue("package_verify_command", pp, CF_SCALAR);
    p.package_noverify_regex = (char *) GetConstraintValue("package_noverify_regex", pp, CF_SCALAR);
    p.package_noverify_returncode = GetIntConstraint("package_noverify_returncode", pp);

    if (GetConstraint(pp, "package_commands_useshell") == NULL)
    {
        p.package_commands_useshell = true;
    }
    else
    {
        p.package_commands_useshell = GetBooleanConstraint("package_commands_useshell", pp);
    }

    p.package_name_convention = (char *) GetConstraintValue("package_name_convention", pp, CF_SCALAR);
    p.package_delete_convention = (char *) GetConstraintValue("package_delete_convention", pp, CF_SCALAR);

    p.package_multiline_start = (char *) GetConstraintValue("package_multiline_start", pp, CF_SCALAR);

    p.package_version_equal_command = GetConstraintValue("package_version_equal_command", pp, CF_SCALAR);
    p.package_version_less_command = GetConstraintValue("package_version_less_command", pp, CF_SCALAR);

    return p;
}

/*******************************************************************/

ProcessSelect GetProcessFilterConstraints(const Promise *pp)
{
    ProcessSelect p;
    char *value;
    int entries = 0;

    p.owner = GetListConstraint("process_owner", pp);

    value = (char *) GetConstraintValue("pid", pp, CF_SCALAR);

    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_pid, &p.max_pid, pp);
    value = (char *) GetConstraintValue("ppid", pp, CF_SCALAR);

    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_ppid, &p.max_ppid, pp);
    value = (char *) GetConstraintValue("pgid", pp, CF_SCALAR);

    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_pgid, &p.max_pgid, pp);
    value = (char *) GetConstraintValue("rsize", pp, CF_SCALAR);

    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_rsize, &p.max_rsize, pp);
    value = (char *) GetConstraintValue("vsize", pp, CF_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_vsize, &p.max_vsize, pp);
    value = (char *) GetConstraintValue("ttime_range", pp, CF_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, (long *) &p.min_ttime, (long *) &p.max_ttime, pp);
    value = (char *) GetConstraintValue("stime_range", pp, CF_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, (long *) &p.min_stime, (long *) &p.max_stime, pp);

    p.status = (char *) GetConstraintValue("status", pp, CF_SCALAR);
    p.command = (char *) GetConstraintValue("command", pp, CF_SCALAR);
    p.tty = (char *) GetConstraintValue("tty", pp, CF_SCALAR);

    value = (char *) GetConstraintValue("priority", pp, CF_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_pri, &p.max_pri, pp);
    value = (char *) GetConstraintValue("threads", pp, CF_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_thread, &p.max_thread, pp);

    if ((p.owner) || (p.status) || (p.command) || (p.tty))
    {
        entries = true;
    }

    if ((p.process_result = (char *) GetConstraintValue("process_result", pp, CF_SCALAR)) == NULL)
    {
        if (entries)
        {
            CfOut(cf_error, "", " !! process_select body missing its a process_result return value");
        }
    }

    return p;
}

/*******************************************************************/

ProcessCount GetMatchesConstraints(const Promise *pp)
{
    ProcessCount p;
    char *value;

    value = (char *) GetConstraintValue("match_range", pp, CF_SCALAR);
    IntRange2Int(value, &p.min_range, &p.max_range, pp);
    p.in_range_define = GetListConstraint("in_range_define", pp);
    p.out_of_range_define = GetListConstraint("out_of_range_define", pp);

    return p;
}

/*******************************************************************/

static void ShowAttributes(Attributes a)
{
    printf(".....................................................\n");
    printf("File Attribute Set =\n\n");

    if (a.havedepthsearch)
        printf(" * havedepthsearch\n");
    if (a.haveselect)
        printf(" * haveselect\n");
    if (a.haverename)
        printf(" * haverename\n");
    if (a.havedelete)
        printf(" * havedelete\n");
    if (a.haveperms)
        printf(" * haveperms\n");
    if (a.havechange)
        printf(" * havechange\n");
    if (a.havecopy)
        printf(" * havecopy\n");
    if (a.havelink)
        printf(" * havelink\n");
    if (a.haveedit)
        printf(" * haveedit\n");
    if (a.create)
        printf(" * havecreate\n");
    if (a.touch)
        printf(" * havetouch\n");
    if (a.move_obstructions)
        printf(" * move_obstructions\n");

    if (a.repository)
        printf(" * repository %s\n", a.repository);
    if (a.transformer)
        printf(" * transformer %s\n", a.transformer);

    printf(".....................................................\n\n");
}

/*******************************************************************/
/* Edit sub-bundles have their own attributes                      */
/*******************************************************************/

Attributes GetInsertionAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havelocation = GetBooleanConstraint("location", pp);
    attr.location = GetLocationAttributes(pp);

    attr.sourcetype = GetConstraintValue("insert_type", pp, CF_SCALAR);
    attr.expandvars = GetBooleanConstraint("expand_scalars", pp);

    attr.haveinsertselect = GetBooleanConstraint("insert_select", pp);
    attr.line_select = GetInsertSelectConstraints(pp);

    attr.insert_match = GetListConstraint("whitespace_policy", pp);

/* Common ("included") */

    attr.haveregion = GetBooleanConstraint("select_region", pp);
    attr.region = GetRegionConstraints(pp);

    attr.xml = GetXmlConstraints(pp);

    attr.havetrans = GetBooleanConstraint(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

EditLocation GetLocationAttributes(const Promise *pp)
{
    EditLocation e;
    char *value;

    e.line_matching = GetConstraintValue("select_line_matching", pp, CF_SCALAR);

    value = GetConstraintValue("before_after", pp, CF_SCALAR);

    if (value && (strcmp(value, "before") == 0))
    {
        e.before_after = cfe_before;
    }
    else
    {
        e.before_after = cfe_after;
    }

    e.first_last = GetConstraintValue("first_last", pp, CF_SCALAR);
    return e;
}

/*******************************************************************/

Attributes GetDeletionAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.not_matching = GetBooleanConstraint("not_matching", pp);

    attr.havedeleteselect = GetBooleanConstraint("delete_select", pp);
    attr.line_select = GetDeleteSelectConstraints(pp);

    /* common */

    attr.haveregion = GetBooleanConstraint("select_region", pp);
    attr.region = GetRegionConstraints(pp);

    attr.xml = GetXmlConstraints(pp);

    attr.havetrans = GetBooleanConstraint(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetColumnAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havecolumn = GetBooleanConstraint("edit_field", pp);
    attr.column = GetColumnConstraints(pp);

    /* common */

    attr.haveregion = GetBooleanConstraint("select_region", pp);
    attr.region = GetRegionConstraints(pp);

    attr.havetrans = GetBooleanConstraint(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetReplaceAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havereplace = GetBooleanConstraint("replace_patterns", pp);
    attr.replace = GetReplaceConstraints(pp);

    attr.havecolumn = GetBooleanConstraint("replace_with", pp);

    /* common */

    attr.haveregion = GetBooleanConstraint("select_region", pp);
    attr.region = GetRegionConstraints(pp);

    attr.xml = GetXmlConstraints(pp);

    attr.havetrans = GetBooleanConstraint(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = GetBooleanConstraint(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

EditXml GetXmlConstraints(const Promise *pp)
{
    EditXml x;

    x.havebuildxpath = ((x.build_xpath = GetConstraintValue("build_xpath", pp, CF_SCALAR)) != NULL);
    x.haveselectxpath = ((x.select_xpath = GetConstraintValue("select_xpath", pp, CF_SCALAR)) != NULL);
    x.haveattributevalue = ((x.attribute_value = GetConstraintValue("attribute_value", pp, CF_SCALAR)) != NULL);

    return x;
}

/*******************************************************************/

EditRegion GetRegionConstraints(const Promise *pp)
{
    EditRegion e;

    e.select_start = GetConstraintValue("select_start", pp, CF_SCALAR);
    e.select_end = GetConstraintValue("select_end", pp, CF_SCALAR);
    e.include_start = GetBooleanConstraint("include_start_delimiter", pp);
    e.include_end = GetBooleanConstraint("include_end_delimiter", pp);
    return e;
}

/*******************************************************************/

EditReplace GetReplaceConstraints(const Promise *pp)
{
    EditReplace r;

    r.replace_value = GetConstraintValue("replace_value", pp, CF_SCALAR);
    r.occurrences = GetConstraintValue("occurrences", pp, CF_SCALAR);

    return r;
}

/*******************************************************************/

EditColumn GetColumnConstraints(const Promise *pp)
{
    EditColumn c;
    char *value;

    c.column_separator = GetConstraintValue("field_separator", pp, CF_SCALAR);
    c.select_column = GetIntConstraint("select_field", pp);

    if (((c.select_column) != CF_NOINT) && (GetBooleanConstraint("start_fields_from_zero", pp)))
    {
        c.select_column++;
    }

    value = GetConstraintValue("value_separator", pp, CF_SCALAR);

    if (value)
    {
        c.value_separator = *value;
    }
    else
    {
        c.value_separator = '\0';
    }

    c.column_value = GetConstraintValue("field_value", pp, CF_SCALAR);
    c.column_operation = GetConstraintValue("field_operation", pp, CF_SCALAR);
    c.extend_columns = GetBooleanConstraint("extend_fields", pp);
    c.blanks_ok = GetBooleanConstraint("allow_blank_fields", pp);
    return c;
}

/*******************************************************************/
/* Storage                                                         */
/*******************************************************************/

StorageMount GetMountConstraints(const Promise *pp)
{
    StorageMount m;

    m.mount_type = GetConstraintValue("mount_type", pp, CF_SCALAR);
    m.mount_source = GetConstraintValue("mount_source", pp, CF_SCALAR);
    m.mount_server = GetConstraintValue("mount_server", pp, CF_SCALAR);
    m.mount_options = GetListConstraint("mount_options", pp);
    m.editfstab = GetBooleanConstraint("edit_fstab", pp);
    m.unmount = GetBooleanConstraint("unmount", pp);

    return m;
}

/*******************************************************************/

StorageVolume GetVolumeConstraints(const Promise *pp)
{
    StorageVolume v;
    char *value;

    v.check_foreign = GetBooleanConstraint("check_foreign", pp);
    value = GetConstraintValue("freespace", pp, CF_SCALAR);

    v.freespace = (long) Str2Int(value);
    value = GetConstraintValue("sensible_size", pp, CF_SCALAR);
    v.sensible_size = (int) Str2Int(value);
    value = GetConstraintValue("sensible_count", pp, CF_SCALAR);
    v.sensible_count = (int) Str2Int(value);
    v.scan_arrivals = GetBooleanConstraint("scan_arrivals", pp);

// defaults
    if (v.sensible_size == CF_NOINT)
    {
        v.sensible_size = 1000;
    }

    if (v.sensible_count == CF_NOINT)
    {
        v.sensible_count = 2;
    }

    return v;
}

/*******************************************************************/

TcpIp GetTCPIPAttributes(const Promise *pp)
{
    TcpIp t;

    t.ipv4_address = GetConstraintValue("ipv4_address", pp, CF_SCALAR);
    t.ipv4_netmask = GetConstraintValue("ipv4_netmask", pp, CF_SCALAR);

    return t;
}

/*******************************************************************/

Report GetReportConstraints(const Promise *pp)
{
 Report r = {0};
 
 r.result = GetConstraintValue("bundle_return_value_index", pp, CF_SCALAR);
    
    if (GetConstraintValue("lastseen", pp, CF_SCALAR))
    {
        r.havelastseen = true;
        r.lastseen = GetIntConstraint("lastseen", pp);

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

    r.intermittency = GetRealConstraint("intermittency", pp);

    if (r.intermittency == CF_NODOUBLE)
    {
        r.intermittency = 0;
    }

    r.haveprintfile = GetBooleanConstraint("printfile", pp);
    r.filename = (char *) GetConstraintValue("file_to_print", pp, CF_SCALAR);
    r.numlines = GetIntConstraint("number_of_lines", pp);

    if (r.numlines == CF_NOINT)
    {
        r.numlines = 5;
    }

    r.showstate = GetListConstraint("showstate", pp);

    r.friend_pattern = GetConstraintValue("friend_pattern", pp, CF_SCALAR);

    r.to_file = GetConstraintValue("report_to_file", pp, CF_SCALAR);

    if ((r.result) && ((r.haveprintfile) || (r.filename) || (r.showstate) || (r.to_file) || (r.lastseen)))
    {
        CfOut(cf_error, "", " !! bundle_return_value promise for \"%s\" in bundle \"%s\" with too many constraints (ignored)", pp->promiser, pp->bundle);
    }
    
    return r;
}

/*******************************************************************/

LineSelect GetInsertSelectConstraints(const Promise *pp)
{
    LineSelect s;

    s.startwith_from_list = GetListConstraint("insert_if_startwith_from_list", pp);
    s.not_startwith_from_list = GetListConstraint("insert_if_not_startwith_from_list", pp);
    s.match_from_list = GetListConstraint("insert_if_match_from_list", pp);
    s.not_match_from_list = GetListConstraint("insert_if_not_match_from_list", pp);
    s.contains_from_list = GetListConstraint("insert_if_contains_from_list", pp);
    s.not_contains_from_list = GetListConstraint("insert_if_not_contains_from_list", pp);

    return s;
}

/*******************************************************************/

LineSelect GetDeleteSelectConstraints(const Promise *pp)
{
    LineSelect s;

    s.startwith_from_list = GetListConstraint("delete_if_startwith_from_list", pp);
    s.not_startwith_from_list = GetListConstraint("delete_if_not_startwith_from_list", pp);
    s.match_from_list = GetListConstraint("delete_if_match_from_list", pp);
    s.not_match_from_list = GetListConstraint("delete_if_not_match_from_list", pp);
    s.contains_from_list = GetListConstraint("delete_if_contains_from_list", pp);
    s.not_contains_from_list = GetListConstraint("delete_if_not_contains_from_list", pp);

    return s;
}

/*******************************************************************/

Measurement GetMeasurementConstraint(const Promise *pp)
{
    Measurement m;
    char *value;

    m.stream_type = GetConstraintValue("stream_type", pp, CF_SCALAR);

    value = GetConstraintValue("data_type", pp, CF_SCALAR);
    m.data_type = Typename2Datatype(value);

    if (m.data_type == cf_notype)
    {
        m.data_type = cf_str;
    }

    m.history_type = GetConstraintValue("history_type", pp, CF_SCALAR);
    m.select_line_matching = GetConstraintValue("select_line_matching", pp, CF_SCALAR);
    m.select_line_number = GetIntConstraint("select_line_number", pp);
    m.policy = MeasurePolicy2Value(GetConstraintValue("select_multiline_policy", pp, CF_SCALAR));
    
    m.extraction_regex = GetConstraintValue("extraction_regex", pp, CF_SCALAR);
    m.units = GetConstraintValue("units", pp, CF_SCALAR);
    m.growing = GetBooleanConstraint("track_growing_file", pp);
    return m;
}

/*******************************************************************/

Database GetDatabaseConstraints(const Promise *pp)
{
    Database d;
    char *value;

    d.db_server_owner = GetConstraintValue("db_server_owner", pp, CF_SCALAR);
    d.db_server_password = GetConstraintValue("db_server_password", pp, CF_SCALAR);
    d.db_server_host = GetConstraintValue("db_server_host", pp, CF_SCALAR);
    d.db_connect_db = GetConstraintValue("db_server_connection_db", pp, CF_SCALAR);
    d.type = GetConstraintValue("database_type", pp, CF_SCALAR);
    d.server = GetConstraintValue("database_server", pp, CF_SCALAR);
    d.columns = GetListConstraint("database_columns", pp);
    d.rows = GetListConstraint("database_rows", pp);
    d.operation = GetConstraintValue("database_operation", pp, CF_SCALAR);
    d.exclude = GetListConstraint("registry_exclude", pp);

    value = GetConstraintValue("db_server_type", pp, CF_SCALAR);
    d.db_server_type = Str2dbType(value);

    if (value && ((d.db_server_type) == cfd_notype))
    {
        CfOut(cf_error, "", "Unsupported database type \"%s\" in databases promise", value);
        PromiseRef(cf_error, pp);
    }

    return d;
}
