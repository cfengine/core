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
#include "policy.h"
#include "conversion.h"
#include "cfstream.h"
#include "chflags.h"

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

    attr.havedepthsearch = PromiseGetConstraintAsBoolean("depth_search", pp);
    attr.haveselect = PromiseGetConstraintAsBoolean("file_select", pp);
    attr.haverename = PromiseGetConstraintAsBoolean("rename", pp);
    attr.havedelete = PromiseGetConstraintAsBoolean("delete", pp);
    attr.haveperms = PromiseGetConstraintAsBoolean("perms", pp);
    attr.havechange = PromiseGetConstraintAsBoolean("changes", pp);
    attr.havecopy = PromiseGetConstraintAsBoolean("copy_from", pp);
    attr.havelink = PromiseGetConstraintAsBoolean("link_from", pp);

    attr.template = (char *)ConstraintGetRvalValue("edit_template", pp, RVAL_TYPE_SCALAR);
    attr.haveeditline = PromiseBundleConstraintExists("edit_line", pp);
    attr.haveeditxml = PromiseBundleConstraintExists("edit_xml", pp);
    attr.haveedit = (attr.haveeditline) || (attr.haveeditxml) || (attr.template);

/* Files, specialist */

    attr.repository = (char *) ConstraintGetRvalValue("repository", pp, RVAL_TYPE_SCALAR);
    attr.create = PromiseGetConstraintAsBoolean("create", pp);
    attr.touch = PromiseGetConstraintAsBoolean("touch", pp);
    attr.transformer = (char *) ConstraintGetRvalValue("transformer", pp, RVAL_TYPE_SCALAR);
    attr.move_obstructions = PromiseGetConstraintAsBoolean("move_obstructions", pp);
    attr.pathtype = (char *) ConstraintGetRvalValue("pathtype", pp, RVAL_TYPE_SCALAR);

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

    attr.havetrans = PromiseGetConstraintAsBoolean(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);
    attr.haveclasses = PromiseGetConstraintAsBoolean(CF_DEFINECLASSES, pp);
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
            cfPS(OUTPUT_LEVEL_ERROR, CF_WARN, "", pp, attr, " !! files promise makes no intention about system state");
        }
    }

    if ((THIS_AGENT_TYPE == AGENT_TYPE_COMMON) && (attr.create) && (attr.havecopy))
    {
        if (((attr.copy.compare) != (FILE_COMPARATOR_CHECKSUM)) && ((attr.copy.compare) != FILE_COMPARATOR_HASH))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "",
                  " !! Promise constraint conflicts - %s file will never be copied as created file is always newer",
                  pp->promiser);
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        }
        else
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "",
                  " !! Promise constraint conflicts - %s file cannot strictly both be created empty and copied from a source file.",
                  pp->promiser);
        }
    }

    if ((THIS_AGENT_TYPE == AGENT_TYPE_COMMON) && (attr.create) && (attr.havelink))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Promise constraint conflicts - %s cannot be created and linked at the same time",
              pp->promiser);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    }

    return attr;
}

/*******************************************************************/

Attributes GetOutputsAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    attr.output.promiser_type = ConstraintGetRvalValue("promiser_type", pp, RVAL_TYPE_SCALAR);
    attr.output.level = ConstraintGetRvalValue("output_level", pp, RVAL_TYPE_SCALAR);
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
    attr.havebundle = PromiseBundleConstraintExists("service_bundle", pp);

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
    attr.havecontain = PromiseGetConstraintAsBoolean("contain", pp);

    attr.args = ConstraintGetRvalValue("args", pp, RVAL_TYPE_SCALAR);
    attr.module = PromiseGetConstraintAsBoolean("module", pp);

/* Common ("included") */

    attr.havetrans = PromiseGetConstraintAsBoolean(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetProcessAttributes(const Promise *pp)
{
    static Attributes attr = { {0} };

    attr.signals = PromiseGetConstraintAsList("signals", pp);
    attr.process_stop = (char *) ConstraintGetRvalValue("process_stop", pp, RVAL_TYPE_SCALAR);
    attr.haveprocess_count = PromiseGetConstraintAsBoolean("process_count", pp);
    attr.haveselect = PromiseGetConstraintAsBoolean("process_select", pp);
    attr.restart_class = (char *) ConstraintGetRvalValue("restart_class", pp, RVAL_TYPE_SCALAR);

    attr.process_count = GetMatchesConstraints(pp);
    attr.process_select = GetProcessFilterConstraints(pp);

/* Common ("included") */

    attr.havetrans = PromiseGetConstraintAsBoolean(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetStorageAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.mount = GetMountConstraints(pp);
    attr.volume = GetVolumeConstraints(pp);
    attr.havevolume = PromiseGetConstraintAsBoolean("volume", pp);
    attr.havemount = PromiseGetConstraintAsBoolean("mount", pp);

/* Common ("included") */

    if (attr.edits.maxfilesize <= 0)
    {
        attr.edits.maxfilesize = EDITFILESIZE;
    }

    attr.havetrans = PromiseGetConstraintAsBoolean(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetMethodAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havebundle = PromiseBundleConstraintExists("usebundle", pp);

    attr.inherit = PromiseGetConstraintAsBoolean("inherit", pp);

/* Common ("included") */

    attr.havetrans = PromiseGetConstraintAsBoolean(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetInterfacesAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havetcpip = PromiseBundleConstraintExists("usebundle", pp);
    attr.tcpip = GetTCPIPAttributes(pp);

/* Common ("included") */

    attr.havetrans = PromiseGetConstraintAsBoolean(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetMeasurementAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.measure = GetMeasurementConstraint(pp);

/* Common ("included") */

    attr.havetrans = PromiseGetConstraintAsBoolean(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

Services GetServicesConstraints(const Promise *pp)
{
    Services s;

    s.service_type = ConstraintGetRvalValue("service_type", pp, RVAL_TYPE_SCALAR);
    s.service_policy = ServicePolicyFromString(ConstraintGetRvalValue("service_policy", pp, RVAL_TYPE_SCALAR));
    s.service_autostart_policy = ConstraintGetRvalValue("service_autostart_policy", pp, RVAL_TYPE_SCALAR);
    s.service_args = ConstraintGetRvalValue("service_args", pp, RVAL_TYPE_SCALAR);
    s.service_depend = PromiseGetConstraintAsList("service_dependencies", pp);
    s.service_depend_chain = ConstraintGetRvalValue("service_dependence_chain", pp, RVAL_TYPE_SCALAR);

    return s;
}

/*******************************************************************/

Environments GetEnvironmentsConstraints(const Promise *pp)
{
    Environments e;

    e.cpus = PromiseGetConstraintAsInt("env_cpus", pp);
    e.memory = PromiseGetConstraintAsInt("env_memory", pp);
    e.disk = PromiseGetConstraintAsInt("env_disk", pp);
    e.baseline = ConstraintGetRvalValue("env_baseline", pp, RVAL_TYPE_SCALAR);
    e.spec = ConstraintGetRvalValue("env_spec", pp, RVAL_TYPE_SCALAR);
    e.host = ConstraintGetRvalValue("environment_host", pp, RVAL_TYPE_SCALAR);

    e.addresses = PromiseGetConstraintAsList("env_addresses", pp);
    e.name = ConstraintGetRvalValue("env_name", pp, RVAL_TYPE_SCALAR);
    e.type = ConstraintGetRvalValue("environment_type", pp, RVAL_TYPE_SCALAR);
    e.state = EnvironmentStateFromString(ConstraintGetRvalValue("environment_state", pp, RVAL_TYPE_SCALAR));

    return e;
}

/*******************************************************************/

ExecContain GetExecContainConstraints(const Promise *pp)
{
    ExecContain e;

    e.useshell = PromiseGetConstraintAsBoolean("useshell", pp);
    e.umask = PromiseGetConstraintAsOctal("umask", pp);
    e.owner = PromiseGetConstraintAsUid("exec_owner", pp);
    e.group = PromiseGetConstraintAsGid("exec_group", pp);
    e.preview = PromiseGetConstraintAsBoolean("preview", pp);
    e.nooutput = PromiseGetConstraintAsBoolean("no_output", pp);
    e.timeout = PromiseGetConstraintAsInt("exec_timeout", pp);
    e.chroot = ConstraintGetRvalValue("chroot", pp, RVAL_TYPE_SCALAR);
    e.chdir = ConstraintGetRvalValue("chdir", pp, RVAL_TYPE_SCALAR);

    return e;
}

/*******************************************************************/

Recursion GetRecursionConstraints(const Promise *pp)
{
    Recursion r;

    r.travlinks = PromiseGetConstraintAsBoolean("traverse_links", pp);
    r.rmdeadlinks = PromiseGetConstraintAsBoolean("rmdeadlinks", pp);
    r.depth = PromiseGetConstraintAsInt("depth", pp);

    if (r.depth == CF_NOINT)
    {
        r.depth = 0;
    }

    r.xdev = PromiseGetConstraintAsBoolean("xdev", pp);
    r.include_dirs = PromiseGetConstraintAsList("include_dirs", pp);
    r.exclude_dirs = PromiseGetConstraintAsList("exclude_dirs", pp);
    r.include_basedir = PromiseGetConstraintAsBoolean("include_basedir", pp);
    return r;
}

/*******************************************************************/

Acl GetAclConstraints(const Promise *pp)
{
    Acl ac;

    ac.acl_method = AclMethodFromString(ConstraintGetRvalValue("acl_method", pp, RVAL_TYPE_SCALAR));
    ac.acl_type = AclTypeFromString(ConstraintGetRvalValue("acl_type", pp, RVAL_TYPE_SCALAR));
    ac.acl_directory_inherit = AclInheritanceFromString(ConstraintGetRvalValue("acl_directory_inherit", pp, RVAL_TYPE_SCALAR));
    ac.acl_entries = PromiseGetConstraintAsList("aces", pp);
    ac.acl_inherit_entries = PromiseGetConstraintAsList("specify_inherit_aces", pp);
    return ac;
}

/*******************************************************************/

FilePerms GetPermissionConstraints(const Promise *pp)
{
    FilePerms p;
    char *value;
    Rlist *list;

    value = (char *) ConstraintGetRvalValue("mode", pp, RVAL_TYPE_SCALAR);

    p.plus = CF_SAMEMODE;
    p.minus = CF_SAMEMODE;

    if (!ParseModeString(value, &p.plus, &p.minus))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Problem validating a mode string");
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    }

    list = PromiseGetConstraintAsList("bsdflags", pp);

    p.plus_flags = 0;
    p.minus_flags = 0;

    if (list && (!ParseFlagString(list, &p.plus_flags, &p.minus_flags)))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Problem validating a BSD flag string");
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    }

#ifdef __MINGW32__
    p.owners = NovaWin_Rlist2SidList((Rlist *) GetConstraintValue("owners", pp, RVAL_TYPE_LIST));
#else /* !__MINGW32__ */
    p.owners = Rlist2UidList((Rlist *) ConstraintGetRvalValue("owners", pp, RVAL_TYPE_LIST), pp);
    p.groups = Rlist2GidList((Rlist *) ConstraintGetRvalValue("groups", pp, RVAL_TYPE_LIST), pp);
#endif /* !__MINGW32__ */

    p.findertype = (char *) ConstraintGetRvalValue("findertype", pp, RVAL_TYPE_SCALAR);
    p.rxdirs = PromiseGetConstraintAsBoolean("rxdirs", pp);

// The default should be true

    if (!ConstraintGetRvalValue("rxdirs", pp, RVAL_TYPE_SCALAR))
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

    s.name = (Rlist *) ConstraintGetRvalValue("leaf_name", pp, RVAL_TYPE_LIST);
    s.path = (Rlist *) ConstraintGetRvalValue("path_name", pp, RVAL_TYPE_LIST);
    s.filetypes = (Rlist *) ConstraintGetRvalValue("file_types", pp, RVAL_TYPE_LIST);
    s.issymlinkto = (Rlist *) ConstraintGetRvalValue("issymlinkto", pp, RVAL_TYPE_LIST);

    s.perms = PromiseGetConstraintAsList("search_mode", pp);

    for (rp = s.perms; rp != NULL; rp = rp->next)
    {
        plus = 0;
        minus = 0;
        value = (char *) rp->item;

        if (!ParseModeString(value, &plus, &minus))
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Problem validating a mode string");
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        }
    }

    s.bsdflags = PromiseGetConstraintAsList("search_bsdflags", pp);

    fplus = 0;
    fminus = 0;

    if (!ParseFlagString(s.bsdflags, &fplus, &fminus))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Problem validating a BSD flag string");
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    }

    if ((s.name) || (s.path) || (s.filetypes) || (s.issymlinkto) || (s.perms) || (s.bsdflags))
    {
        entries = true;
    }

    s.owners = (Rlist *) ConstraintGetRvalValue("search_owners", pp, RVAL_TYPE_LIST);
    s.groups = (Rlist *) ConstraintGetRvalValue("search_groups", pp, RVAL_TYPE_LIST);

    value = (char *) ConstraintGetRvalValue("search_size", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, (long *) &s.min_size, (long *) &s.max_size, pp);

    value = (char *) ConstraintGetRvalValue("ctime", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, (long *) &s.min_ctime, (long *) &s.max_ctime, pp);
    value = (char *) ConstraintGetRvalValue("atime", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }
    IntRange2Int(value, (long *) &s.min_atime, (long *) &s.max_atime, pp);
    value = (char *) ConstraintGetRvalValue("mtime", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, (long *) &s.min_mtime, (long *) &s.max_mtime, pp);

    s.exec_regex = (char *) ConstraintGetRvalValue("exec_regex", pp, RVAL_TYPE_SCALAR);
    s.exec_program = (char *) ConstraintGetRvalValue("exec_program", pp, RVAL_TYPE_SCALAR);

    if ((s.owners) || (s.min_size) || (s.exec_regex) || (s.exec_program))
    {
        entries = true;
    }

    if ((s.result = (char *) ConstraintGetRvalValue("file_result", pp, RVAL_TYPE_SCALAR)) == NULL)
    {
        if (!entries)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! file_select body missing its a file_result return value");
        }
    }

    return s;
}

/*******************************************************************/

TransactionContext GetTransactionConstraints(const Promise *pp)
{
    TransactionContext t;
    char *value;

    value = ConstraintGetRvalValue("action_policy", pp, RVAL_TYPE_SCALAR);

    if (value && ((strcmp(value, "warn") == 0) || (strcmp(value, "nop") == 0)))
    {
        t.action = cfa_warn;
    }
    else
    {
        t.action = cfa_fix;     // default
    }

    t.background = PromiseGetConstraintAsBoolean("background", pp);
    t.ifelapsed = PromiseGetConstraintAsInt("ifelapsed", pp);

    if (t.ifelapsed == CF_NOINT)
    {
        t.ifelapsed = VIFELAPSED;
    }

    t.expireafter = PromiseGetConstraintAsInt("expireafter", pp);

    if (t.expireafter == CF_NOINT)
    {
        t.expireafter = VEXPIREAFTER;
    }

    t.audit = PromiseGetConstraintAsBoolean("audit", pp);
    t.log_string = ConstraintGetRvalValue("log_string", pp, RVAL_TYPE_SCALAR);
    t.log_priority = SyslogPriorityFromString(ConstraintGetRvalValue("log_priority", pp, RVAL_TYPE_SCALAR));

    t.log_kept = ConstraintGetRvalValue("log_kept", pp, RVAL_TYPE_SCALAR);
    t.log_repaired = ConstraintGetRvalValue("log_repaired", pp, RVAL_TYPE_SCALAR);
    t.log_failed = ConstraintGetRvalValue("log_failed", pp, RVAL_TYPE_SCALAR);

    if ((t.value_kept = PromiseGetConstraintAsReal("value_kept", pp)) == CF_NODOUBLE)
    {
        t.value_kept = 1.0;
    }

    if ((t.value_repaired = PromiseGetConstraintAsReal("value_repaired", pp)) == CF_NODOUBLE)
    {
        t.value_repaired = 0.5;
    }

    if ((t.value_notkept = PromiseGetConstraintAsReal("value_notkept", pp)) == CF_NODOUBLE)
    {
        t.value_notkept = -1.0;
    }

    value = ConstraintGetRvalValue("log_level", pp, RVAL_TYPE_SCALAR);
    t.log_level = OutputLevelFromString(value);

    value = ConstraintGetRvalValue("report_level", pp, RVAL_TYPE_SCALAR);
    t.report_level = OutputLevelFromString(value);

    t.measure_id = ConstraintGetRvalValue("measurement_class", pp, RVAL_TYPE_SCALAR);

    return t;
}

/*******************************************************************/

DefineClasses GetClassDefinitionConstraints(const Promise *pp)
{
    DefineClasses c;
    char *pt = NULL;

    c.change = (Rlist *) PromiseGetConstraintAsList("promise_repaired", pp);
    c.failure = (Rlist *) PromiseGetConstraintAsList("repair_failed", pp);
    c.denied = (Rlist *) PromiseGetConstraintAsList("repair_denied", pp);
    c.timeout = (Rlist *) PromiseGetConstraintAsList("repair_timeout", pp);
    c.kept = (Rlist *) PromiseGetConstraintAsList("promise_kept", pp);
    c.interrupt = (Rlist *) PromiseGetConstraintAsList("on_interrupt", pp);

    c.del_change = (Rlist *) PromiseGetConstraintAsList("cancel_repaired", pp);
    c.del_kept = (Rlist *) PromiseGetConstraintAsList("cancel_kept", pp);
    c.del_notkept = (Rlist *) PromiseGetConstraintAsList("cancel_notkept", pp);

    c.retcode_kept = (Rlist *) PromiseGetConstraintAsList("kept_returncodes", pp);
    c.retcode_repaired = (Rlist *) PromiseGetConstraintAsList("repaired_returncodes", pp);
    c.retcode_failed = (Rlist *) PromiseGetConstraintAsList("failed_returncodes", pp);

    c.persist = PromiseGetConstraintAsInt("persist_time", pp);

    if (c.persist == CF_NOINT)
    {
        c.persist = 0;
    }

    pt = ConstraintGetRvalValue("timer_policy", pp, RVAL_TYPE_SCALAR);

    if (pt && (strncmp(pt, "abs", 3) == 0))
    {
        c.timer = CONTEXT_STATE_POLICY_PRESERVE;
    }
    else
    {
        c.timer = CONTEXT_STATE_POLICY_RESET;
    }

    return c;
}

/*******************************************************************/

FileDelete GetDeleteConstraints(const Promise *pp)
{
    FileDelete f;
    char *value;

    value = (char *) ConstraintGetRvalValue("dirlinks", pp, RVAL_TYPE_SCALAR);

    if (value && (strcmp(value, "keep") == 0))
    {
        f.dirlinks = cfa_linkkeep;
    }
    else
    {
        f.dirlinks = cfa_linkdelete;
    }

    f.rmdirs = PromiseGetConstraintAsBoolean("rmdirs", pp);
    return f;
}

/*******************************************************************/

FileRename GetRenameConstraints(const Promise *pp)
{
    FileRename r;
    char *value;

    value = (char *) ConstraintGetRvalValue("disable_mode", pp, RVAL_TYPE_SCALAR);

    if (!ParseModeString(value, &r.plus, &r.minus))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Problem validating a mode string");
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    }

    r.disable = PromiseGetConstraintAsBoolean("disable", pp);
    r.disable_suffix = (char *) ConstraintGetRvalValue("disable_suffix", pp, RVAL_TYPE_SCALAR);
    r.newname = (char *) ConstraintGetRvalValue("newname", pp, RVAL_TYPE_SCALAR);
    r.rotate = PromiseGetConstraintAsInt("rotate", pp);

    return r;
}

/*******************************************************************/

FileChange GetChangeMgtConstraints(const Promise *pp)
{
    FileChange c;
    char *value;

    value = (char *) ConstraintGetRvalValue("hash", pp, RVAL_TYPE_SCALAR);

    if (value && (strcmp(value, "best") == 0))
    {
#ifdef HAVE_NOVA
        c.hash = HASH_METHOD_SHA512;
#else
        c.hash = HASH_METHOD_BEST;
#endif
    }
    else if (value && (strcmp(value, "md5") == 0))
    {
        c.hash = HASH_METHOD_MD5;
    }
    else if (value && (strcmp(value, "sha1") == 0))
    {
        c.hash = HASH_METHOD_SHA1;
    }
    else if (value && (strcmp(value, "sha256") == 0))
    {
        c.hash = HASH_METHOD_SHA256;
    }
    else if (value && (strcmp(value, "sha384") == 0))
    {
        c.hash = HASH_METHOD_SHA384;
    }
    else if (value && (strcmp(value, "sha512") == 0))
    {
        c.hash = HASH_METHOD_SHA512;
    }
    else
    {
        c.hash = CF_DEFAULT_DIGEST;
    }

    if (FIPS_MODE && (c.hash == HASH_METHOD_MD5))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! FIPS mode is enabled, and md5 is not an approved algorithm");
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    }

    value = (char *) ConstraintGetRvalValue("report_changes", pp, RVAL_TYPE_SCALAR);

    if (value && (strcmp(value, "content") == 0))
    {
        c.report_changes = FILE_CHANGE_REPORT_CONTENT_CHANGE;
    }
    else if (value && (strcmp(value, "stats") == 0))
    {
        c.report_changes = FILE_CHANGE_REPORT_STATS_CHANGE;
    }
    else if (value && (strcmp(value, "all") == 0))
    {
        c.report_changes = FILE_CHANGE_REPORT_ALL;
    }
    else
    {
        c.report_changes = FILE_CHANGE_REPORT_NONE;
    }

    if (ConstraintGetRvalValue("update_hashes", pp, RVAL_TYPE_SCALAR))
    {
        c.update = PromiseGetConstraintAsBoolean("update_hashes", pp);
    }
    else
    {
        c.update = CHECKSUMUPDATES;
    }

    c.report_diffs = PromiseGetConstraintAsBoolean("report_diffs", pp);
    return c;
}

/*******************************************************************/

FileCopy GetCopyConstraints(const Promise *pp)
{
    FileCopy f;
    char *value;
    long min, max;

    f.source = (char *) ConstraintGetRvalValue("source", pp, RVAL_TYPE_SCALAR);

    value = (char *) ConstraintGetRvalValue("compare", pp, RVAL_TYPE_SCALAR);

    if (value == NULL)
    {
        value = DEFAULT_COPYTYPE;
    }

    f.compare = FileComparatorFromString(value);

    value = (char *) ConstraintGetRvalValue("link_type", pp, RVAL_TYPE_SCALAR);

    f.link_type = FileLinkTypeFromString(value);
    f.servers = PromiseGetConstraintAsList("servers", pp);
    f.portnumber = (short) PromiseGetConstraintAsInt("portnumber", pp);
    f.timeout = (short) PromiseGetConstraintAsInt("timeout", pp);
    f.link_instead = PromiseGetConstraintAsList("linkcopy_patterns", pp);
    f.copy_links = PromiseGetConstraintAsList("copylink_patterns", pp);

    value = (char *) ConstraintGetRvalValue("copy_backup", pp, RVAL_TYPE_SCALAR);

    if (value && (strcmp(value, "false") == 0))
    {
        f.backup = BACKUP_OPTION_NO_BACKUP;
    }
    else if (value && (strcmp(value, "timestamp") == 0))
    {
        f.backup = BACKUP_OPTION_TIMESTAMP;
    }
    else
    {
        f.backup = BACKUP_OPTION_BACKUP;
    }

    f.stealth = PromiseGetConstraintAsBoolean("stealth", pp);
    f.collapse = PromiseGetConstraintAsBoolean("collapse_destination_dir", pp);
    f.preserve = PromiseGetConstraintAsBoolean("preserve", pp);
    f.type_check = PromiseGetConstraintAsBoolean("type_check", pp);
    f.force_update = PromiseGetConstraintAsBoolean("force_update", pp);
    f.force_ipv4 = PromiseGetConstraintAsBoolean("force_ipv4", pp);
    f.check_root = PromiseGetConstraintAsBoolean("check_root", pp);

    value = (char *) ConstraintGetRvalValue("copy_size", pp, RVAL_TYPE_SCALAR);
    IntRange2Int(value, &min, &max, pp);

    f.min_size = (size_t) min;
    f.max_size = (size_t) max;

    f.trustkey = PromiseGetConstraintAsBoolean("trustkey", pp);
    f.encrypt = PromiseGetConstraintAsBoolean("encrypt", pp);
    f.verify = PromiseGetConstraintAsBoolean("verify", pp);
    f.purge = PromiseGetConstraintAsBoolean("purge", pp);
    f.destination = NULL;

    return f;
}

/*******************************************************************/

FileLink GetLinkConstraints(const Promise *pp)
{
    FileLink f;
    char *value;

    f.source = (char *) ConstraintGetRvalValue("source", pp, RVAL_TYPE_SCALAR);
    value = (char *) ConstraintGetRvalValue("link_type", pp, RVAL_TYPE_SCALAR);
    f.link_type = FileLinkTypeFromString(value);
    f.copy_patterns = PromiseGetConstraintAsList("copy_patterns", pp);

    value = (char *) ConstraintGetRvalValue("when_no_source", pp, RVAL_TYPE_SCALAR);

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

    value = (char *) ConstraintGetRvalValue("when_linking_children", pp, RVAL_TYPE_SCALAR);

    if (value && (strcmp(value, "override_file") == 0))
    {
        f.when_linking_children = cfa_override;
    }
    else
    {
        f.when_linking_children = cfa_onlynonexisting;
    }

    f.link_children = PromiseGetConstraintAsBoolean("link_children", pp);

    return f;
}

/*******************************************************************/

EditDefaults GetEditDefaults(const Promise *pp)
{
    EditDefaults e = { 0 };
    char *value;

    e.maxfilesize = PromiseGetConstraintAsInt("max_file_size", pp);

    if ((e.maxfilesize == CF_NOINT) || (e.maxfilesize == 0))
    {
        e.maxfilesize = EDITFILESIZE;
    }

    value = (char *) ConstraintGetRvalValue("edit_backup", pp, RVAL_TYPE_SCALAR);

    if (value && (strcmp(value, "false") == 0))
    {
        e.backup = BACKUP_OPTION_NO_BACKUP;
    }
    else if (value && (strcmp(value, "timestamp") == 0))
    {
        e.backup = BACKUP_OPTION_TIMESTAMP;
    }
    else if (value && (strcmp(value, "rotate") == 0))
    {
        e.backup = BACKUP_OPTION_ROTATE;
        e.rotate = PromiseGetConstraintAsInt("rotate", pp);
    }
    else
    {
        e.backup = BACKUP_OPTION_BACKUP;
    }

    e.empty_before_use = PromiseGetConstraintAsBoolean("empty_file_before_editing", pp); 

    e.joinlines = PromiseGetConstraintAsBoolean("recognize_join", pp);

    e.inherit = PromiseGetConstraintAsBoolean("inherit", pp);

    return e;
}

/*******************************************************************/

ContextConstraint GetContextConstraints(const Promise *pp)
{
    ContextConstraint a;

    a.nconstraints = 0;
    a.expression = NULL;
    a.persistent = PromiseGetConstraintAsInt("persistence", pp);

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        for (int k = 0; CF_CLASSBODY[k].lval != NULL; k++)
        {
            if (strcmp(cp->lval, "persistence") == 0)
            {
                continue;
            }

            if (strcmp(cp->lval, CF_CLASSBODY[k].lval) == 0)
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
    PackageAction action;
    PackageVersionComparator operator;
    PackageActionPolicy change_policy;

    p.have_package_methods = PromiseGetConstraintAsBoolean("havepackage_method", pp);
    p.package_version = (char *) ConstraintGetRvalValue("package_version", pp, RVAL_TYPE_SCALAR);
    p.package_architectures = PromiseGetConstraintAsList("package_architectures", pp);

    action = PackageActionFromString((char *) ConstraintGetRvalValue("package_policy", pp, RVAL_TYPE_SCALAR));
    p.package_policy = action;

    if (p.package_policy == PACKAGE_ACTION_NONE)        // Default action => package add
    {
        p.package_policy = PACKAGE_ACTION_ADD;
    }

    operator = PackageVersionComparatorFromString((char *) ConstraintGetRvalValue("package_select", pp, RVAL_TYPE_SCALAR));

    p.package_select = operator;
    change_policy = PackageActionPolicyFromString((char *) ConstraintGetRvalValue("package_changes", pp, RVAL_TYPE_SCALAR));
    p.package_changes = change_policy;

    p.package_file_repositories = PromiseGetConstraintAsList("package_file_repositories", pp);

    p.package_default_arch_command = (char *) ConstraintGetRvalValue("package_default_arch_command", pp, RVAL_TYPE_SCALAR);

    p.package_patch_list_command = (char *) ConstraintGetRvalValue("package_patch_list_command", pp, RVAL_TYPE_SCALAR);
    p.package_patch_name_regex = (char *) ConstraintGetRvalValue("package_patch_name_regex", pp, RVAL_TYPE_SCALAR);
    p.package_patch_arch_regex = (char *) ConstraintGetRvalValue("package_patch_arch_regex", pp, RVAL_TYPE_SCALAR);
    p.package_patch_version_regex = (char *) ConstraintGetRvalValue("package_patch_version_regex", pp, RVAL_TYPE_SCALAR);
    p.package_patch_installed_regex = (char *) ConstraintGetRvalValue("package_patch_installed_regex", pp, RVAL_TYPE_SCALAR);

    p.package_list_update_command = (char *) ConstraintGetRvalValue("package_list_update_command", pp, RVAL_TYPE_SCALAR);
    p.package_list_update_ifelapsed = PromiseGetConstraintAsInt("package_list_update_ifelapsed", pp);
    p.package_list_command = (char *) ConstraintGetRvalValue("package_list_command", pp, RVAL_TYPE_SCALAR);
    p.package_list_version_regex = (char *) ConstraintGetRvalValue("package_list_version_regex", pp, RVAL_TYPE_SCALAR);
    p.package_list_name_regex = (char *) ConstraintGetRvalValue("package_list_name_regex", pp, RVAL_TYPE_SCALAR);
    p.package_list_arch_regex = (char *) ConstraintGetRvalValue("package_list_arch_regex", pp, RVAL_TYPE_SCALAR);

    p.package_installed_regex = (char *) ConstraintGetRvalValue("package_installed_regex", pp, RVAL_TYPE_SCALAR);

    p.package_version_regex = (char *) ConstraintGetRvalValue("package_version_regex", pp, RVAL_TYPE_SCALAR);
    p.package_name_regex = (char *) ConstraintGetRvalValue("package_name_regex", pp, RVAL_TYPE_SCALAR);
    p.package_arch_regex = (char *) ConstraintGetRvalValue("package_arch_regex", pp, RVAL_TYPE_SCALAR);

    p.package_add_command = (char *) ConstraintGetRvalValue("package_add_command", pp, RVAL_TYPE_SCALAR);
    p.package_delete_command = (char *) ConstraintGetRvalValue("package_delete_command", pp, RVAL_TYPE_SCALAR);
    p.package_update_command = (char *) ConstraintGetRvalValue("package_update_command", pp, RVAL_TYPE_SCALAR);
    p.package_patch_command = (char *) ConstraintGetRvalValue("package_patch_command", pp, RVAL_TYPE_SCALAR);
    p.package_verify_command = (char *) ConstraintGetRvalValue("package_verify_command", pp, RVAL_TYPE_SCALAR);
    p.package_noverify_regex = (char *) ConstraintGetRvalValue("package_noverify_regex", pp, RVAL_TYPE_SCALAR);
    p.package_noverify_returncode = PromiseGetConstraintAsInt("package_noverify_returncode", pp);

    if (PromiseGetConstraint(pp, "package_commands_useshell") == NULL)
    {
        p.package_commands_useshell = true;
    }
    else
    {
        p.package_commands_useshell = PromiseGetConstraintAsBoolean("package_commands_useshell", pp);
    }

    p.package_name_convention = (char *) ConstraintGetRvalValue("package_name_convention", pp, RVAL_TYPE_SCALAR);
    p.package_delete_convention = (char *) ConstraintGetRvalValue("package_delete_convention", pp, RVAL_TYPE_SCALAR);

    p.package_multiline_start = (char *) ConstraintGetRvalValue("package_multiline_start", pp, RVAL_TYPE_SCALAR);

    p.package_version_equal_command = ConstraintGetRvalValue("package_version_equal_command", pp, RVAL_TYPE_SCALAR);
    p.package_version_less_command = ConstraintGetRvalValue("package_version_less_command", pp, RVAL_TYPE_SCALAR);

    return p;
}

/*******************************************************************/

ProcessSelect GetProcessFilterConstraints(const Promise *pp)
{
    ProcessSelect p;
    char *value;
    int entries = 0;

    p.owner = PromiseGetConstraintAsList("process_owner", pp);

    value = (char *) ConstraintGetRvalValue("pid", pp, RVAL_TYPE_SCALAR);

    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_pid, &p.max_pid, pp);
    value = (char *) ConstraintGetRvalValue("ppid", pp, RVAL_TYPE_SCALAR);

    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_ppid, &p.max_ppid, pp);
    value = (char *) ConstraintGetRvalValue("pgid", pp, RVAL_TYPE_SCALAR);

    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_pgid, &p.max_pgid, pp);
    value = (char *) ConstraintGetRvalValue("rsize", pp, RVAL_TYPE_SCALAR);

    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_rsize, &p.max_rsize, pp);
    value = (char *) ConstraintGetRvalValue("vsize", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_vsize, &p.max_vsize, pp);
    value = (char *) ConstraintGetRvalValue("ttime_range", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, (long *) &p.min_ttime, (long *) &p.max_ttime, pp);
    value = (char *) ConstraintGetRvalValue("stime_range", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, (long *) &p.min_stime, (long *) &p.max_stime, pp);

    p.status = (char *) ConstraintGetRvalValue("status", pp, RVAL_TYPE_SCALAR);
    p.command = (char *) ConstraintGetRvalValue("command", pp, RVAL_TYPE_SCALAR);
    p.tty = (char *) ConstraintGetRvalValue("tty", pp, RVAL_TYPE_SCALAR);

    value = (char *) ConstraintGetRvalValue("priority", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_pri, &p.max_pri, pp);
    value = (char *) ConstraintGetRvalValue("threads", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    IntRange2Int(value, &p.min_thread, &p.max_thread, pp);

    if ((p.owner) || (p.status) || (p.command) || (p.tty))
    {
        entries = true;
    }

    if ((p.process_result = (char *) ConstraintGetRvalValue("process_result", pp, RVAL_TYPE_SCALAR)) == NULL)
    {
        if (entries)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! process_select body missing its a process_result return value");
        }
    }

    return p;
}

/*******************************************************************/

ProcessCount GetMatchesConstraints(const Promise *pp)
{
    ProcessCount p;
    char *value;

    value = (char *) ConstraintGetRvalValue("match_range", pp, RVAL_TYPE_SCALAR);
    IntRange2Int(value, &p.min_range, &p.max_range, pp);
    p.in_range_define = PromiseGetConstraintAsList("in_range_define", pp);
    p.out_of_range_define = PromiseGetConstraintAsList("out_of_range_define", pp);

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

    attr.havelocation = PromiseGetConstraintAsBoolean("location", pp);
    attr.location = GetLocationAttributes(pp);

    attr.sourcetype = ConstraintGetRvalValue("insert_type", pp, RVAL_TYPE_SCALAR);
    attr.expandvars = PromiseGetConstraintAsBoolean("expand_scalars", pp);

    attr.haveinsertselect = PromiseGetConstraintAsBoolean("insert_select", pp);
    attr.line_select = GetInsertSelectConstraints(pp);

    attr.insert_match = PromiseGetConstraintAsList("whitespace_policy", pp);

/* Common ("included") */

    attr.haveregion = PromiseGetConstraintAsBoolean("select_region", pp);
    attr.region = GetRegionConstraints(pp);

    attr.xml = GetXmlConstraints(pp);

    attr.havetrans = PromiseGetConstraintAsBoolean(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

EditLocation GetLocationAttributes(const Promise *pp)
{
    EditLocation e;
    char *value;

    e.line_matching = ConstraintGetRvalValue("select_line_matching", pp, RVAL_TYPE_SCALAR);

    value = ConstraintGetRvalValue("before_after", pp, RVAL_TYPE_SCALAR);

    if (value && (strcmp(value, "before") == 0))
    {
        e.before_after = EDIT_ORDER_BEFORE;
    }
    else
    {
        e.before_after = EDIT_ORDER_AFTER;
    }

    e.first_last = ConstraintGetRvalValue("first_last", pp, RVAL_TYPE_SCALAR);
    return e;
}

/*******************************************************************/

Attributes GetDeletionAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.not_matching = PromiseGetConstraintAsBoolean("not_matching", pp);

    attr.havedeleteselect = PromiseGetConstraintAsBoolean("delete_select", pp);
    attr.line_select = GetDeleteSelectConstraints(pp);

    /* common */

    attr.haveregion = PromiseGetConstraintAsBoolean("select_region", pp);
    attr.region = GetRegionConstraints(pp);

    attr.xml = GetXmlConstraints(pp);

    attr.havetrans = PromiseGetConstraintAsBoolean(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetColumnAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havecolumn = PromiseGetConstraintAsBoolean("edit_field", pp);
    attr.column = GetColumnConstraints(pp);

    /* common */

    attr.haveregion = PromiseGetConstraintAsBoolean("select_region", pp);
    attr.region = GetRegionConstraints(pp);

    attr.havetrans = PromiseGetConstraintAsBoolean(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

Attributes GetReplaceAttributes(const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havereplace = PromiseGetConstraintAsBoolean("replace_patterns", pp);
    attr.replace = GetReplaceConstraints(pp);

    attr.havecolumn = PromiseGetConstraintAsBoolean("replace_with", pp);

    /* common */

    attr.haveregion = PromiseGetConstraintAsBoolean("select_region", pp);
    attr.region = GetRegionConstraints(pp);

    attr.xml = GetXmlConstraints(pp);

    attr.havetrans = PromiseGetConstraintAsBoolean(CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(pp);

    return attr;
}

/*******************************************************************/

EditXml GetXmlConstraints(const Promise *pp)
{
    EditXml x;

    x.havebuildxpath = ((x.build_xpath = ConstraintGetRvalValue("build_xpath", pp, RVAL_TYPE_SCALAR)) != NULL);
    x.haveselectxpath = ((x.select_xpath = ConstraintGetRvalValue("select_xpath", pp, RVAL_TYPE_SCALAR)) != NULL);
    x.haveattributevalue = ((x.attribute_value = ConstraintGetRvalValue("attribute_value", pp, RVAL_TYPE_SCALAR)) != NULL);

    return x;
}

/*******************************************************************/

EditRegion GetRegionConstraints(const Promise *pp)
{
    EditRegion e;

    e.select_start = ConstraintGetRvalValue("select_start", pp, RVAL_TYPE_SCALAR);
    e.select_end = ConstraintGetRvalValue("select_end", pp, RVAL_TYPE_SCALAR);
    e.include_start = PromiseGetConstraintAsBoolean("include_start_delimiter", pp);
    e.include_end = PromiseGetConstraintAsBoolean("include_end_delimiter", pp);
    return e;
}

/*******************************************************************/

EditReplace GetReplaceConstraints(const Promise *pp)
{
    EditReplace r;

    r.replace_value = ConstraintGetRvalValue("replace_value", pp, RVAL_TYPE_SCALAR);
    r.occurrences = ConstraintGetRvalValue("occurrences", pp, RVAL_TYPE_SCALAR);

    return r;
}

/*******************************************************************/

EditColumn GetColumnConstraints(const Promise *pp)
{
    EditColumn c;
    char *value;

    c.column_separator = ConstraintGetRvalValue("field_separator", pp, RVAL_TYPE_SCALAR);
    c.select_column = PromiseGetConstraintAsInt("select_field", pp);

    if (((c.select_column) != CF_NOINT) && (PromiseGetConstraintAsBoolean("start_fields_from_zero", pp)))
    {
        c.select_column++;
    }

    value = ConstraintGetRvalValue("value_separator", pp, RVAL_TYPE_SCALAR);

    if (value)
    {
        c.value_separator = *value;
    }
    else
    {
        c.value_separator = '\0';
    }

    c.column_value = ConstraintGetRvalValue("field_value", pp, RVAL_TYPE_SCALAR);
    c.column_operation = ConstraintGetRvalValue("field_operation", pp, RVAL_TYPE_SCALAR);
    c.extend_columns = PromiseGetConstraintAsBoolean("extend_fields", pp);
    c.blanks_ok = PromiseGetConstraintAsBoolean("allow_blank_fields", pp);
    return c;
}

/*******************************************************************/
/* Storage                                                         */
/*******************************************************************/

StorageMount GetMountConstraints(const Promise *pp)
{
    StorageMount m;

    m.mount_type = ConstraintGetRvalValue("mount_type", pp, RVAL_TYPE_SCALAR);
    m.mount_source = ConstraintGetRvalValue("mount_source", pp, RVAL_TYPE_SCALAR);
    m.mount_server = ConstraintGetRvalValue("mount_server", pp, RVAL_TYPE_SCALAR);
    m.mount_options = PromiseGetConstraintAsList("mount_options", pp);
    m.editfstab = PromiseGetConstraintAsBoolean("edit_fstab", pp);
    m.unmount = PromiseGetConstraintAsBoolean("unmount", pp);

    return m;
}

/*******************************************************************/

StorageVolume GetVolumeConstraints(const Promise *pp)
{
    StorageVolume v;
    char *value;

    v.check_foreign = PromiseGetConstraintAsBoolean("check_foreign", pp);
    value = ConstraintGetRvalValue("freespace", pp, RVAL_TYPE_SCALAR);

    v.freespace = (long) IntFromString(value);
    value = ConstraintGetRvalValue("sensible_size", pp, RVAL_TYPE_SCALAR);
    v.sensible_size = (int) IntFromString(value);
    value = ConstraintGetRvalValue("sensible_count", pp, RVAL_TYPE_SCALAR);
    v.sensible_count = (int) IntFromString(value);
    v.scan_arrivals = PromiseGetConstraintAsBoolean("scan_arrivals", pp);

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

    t.ipv4_address = ConstraintGetRvalValue("ipv4_address", pp, RVAL_TYPE_SCALAR);
    t.ipv4_netmask = ConstraintGetRvalValue("ipv4_netmask", pp, RVAL_TYPE_SCALAR);

    return t;
}

/*******************************************************************/

Report GetReportConstraints(const Promise *pp)
{
 Report r = {0};
 
 r.result = ConstraintGetRvalValue("bundle_return_value_index", pp, RVAL_TYPE_SCALAR);
    
    if (ConstraintGetRvalValue("lastseen", pp, RVAL_TYPE_SCALAR))
    {
        r.havelastseen = true;
        r.lastseen = PromiseGetConstraintAsInt("lastseen", pp);

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

    r.intermittency = PromiseGetConstraintAsReal("intermittency", pp);

    if (r.intermittency == CF_NODOUBLE)
    {
        r.intermittency = 0;
    }

    r.haveprintfile = PromiseGetConstraintAsBoolean("printfile", pp);
    r.filename = (char *) ConstraintGetRvalValue("file_to_print", pp, RVAL_TYPE_SCALAR);
    r.numlines = PromiseGetConstraintAsInt("number_of_lines", pp);

    if (r.numlines == CF_NOINT)
    {
        r.numlines = 5;
    }

    r.showstate = PromiseGetConstraintAsList("showstate", pp);

    r.friend_pattern = ConstraintGetRvalValue("friend_pattern", pp, RVAL_TYPE_SCALAR);

    r.to_file = ConstraintGetRvalValue("report_to_file", pp, RVAL_TYPE_SCALAR);

    if ((r.result) && ((r.haveprintfile) || (r.filename) || (r.showstate) || (r.to_file) || (r.lastseen)))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! bundle_return_value promise for \"%s\" in bundle \"%s\" with too many constraints (ignored)", pp->promiser, pp->bundle);
    }
    
    return r;
}

/*******************************************************************/

LineSelect GetInsertSelectConstraints(const Promise *pp)
{
    LineSelect s;

    s.startwith_from_list = PromiseGetConstraintAsList("insert_if_startwith_from_list", pp);
    s.not_startwith_from_list = PromiseGetConstraintAsList("insert_if_not_startwith_from_list", pp);
    s.match_from_list = PromiseGetConstraintAsList("insert_if_match_from_list", pp);
    s.not_match_from_list = PromiseGetConstraintAsList("insert_if_not_match_from_list", pp);
    s.contains_from_list = PromiseGetConstraintAsList("insert_if_contains_from_list", pp);
    s.not_contains_from_list = PromiseGetConstraintAsList("insert_if_not_contains_from_list", pp);

    return s;
}

/*******************************************************************/

LineSelect GetDeleteSelectConstraints(const Promise *pp)
{
    LineSelect s;

    s.startwith_from_list = PromiseGetConstraintAsList("delete_if_startwith_from_list", pp);
    s.not_startwith_from_list = PromiseGetConstraintAsList("delete_if_not_startwith_from_list", pp);
    s.match_from_list = PromiseGetConstraintAsList("delete_if_match_from_list", pp);
    s.not_match_from_list = PromiseGetConstraintAsList("delete_if_not_match_from_list", pp);
    s.contains_from_list = PromiseGetConstraintAsList("delete_if_contains_from_list", pp);
    s.not_contains_from_list = PromiseGetConstraintAsList("delete_if_not_contains_from_list", pp);

    return s;
}

/*******************************************************************/

Measurement GetMeasurementConstraint(const Promise *pp)
{
    Measurement m;
    char *value;

    m.stream_type = ConstraintGetRvalValue("stream_type", pp, RVAL_TYPE_SCALAR);

    value = ConstraintGetRvalValue("data_type", pp, RVAL_TYPE_SCALAR);
    m.data_type = DataTypeFromString(value);

    if (m.data_type == DATA_TYPE_NONE)
    {
        m.data_type = DATA_TYPE_STRING;
    }

    m.history_type = ConstraintGetRvalValue("history_type", pp, RVAL_TYPE_SCALAR);
    m.select_line_matching = ConstraintGetRvalValue("select_line_matching", pp, RVAL_TYPE_SCALAR);
    m.select_line_number = PromiseGetConstraintAsInt("select_line_number", pp);
    m.policy = MeasurePolicyFromString(ConstraintGetRvalValue("select_multiline_policy", pp, RVAL_TYPE_SCALAR));
    
    m.extraction_regex = ConstraintGetRvalValue("extraction_regex", pp, RVAL_TYPE_SCALAR);
    m.units = ConstraintGetRvalValue("units", pp, RVAL_TYPE_SCALAR);
    m.growing = PromiseGetConstraintAsBoolean("track_growing_file", pp);
    return m;
}

/*******************************************************************/

Database GetDatabaseConstraints(const Promise *pp)
{
    Database d;
    char *value;

    d.db_server_owner = ConstraintGetRvalValue("db_server_owner", pp, RVAL_TYPE_SCALAR);
    d.db_server_password = ConstraintGetRvalValue("db_server_password", pp, RVAL_TYPE_SCALAR);
    d.db_server_host = ConstraintGetRvalValue("db_server_host", pp, RVAL_TYPE_SCALAR);
    d.db_connect_db = ConstraintGetRvalValue("db_server_connection_db", pp, RVAL_TYPE_SCALAR);
    d.type = ConstraintGetRvalValue("database_type", pp, RVAL_TYPE_SCALAR);
    d.server = ConstraintGetRvalValue("database_server", pp, RVAL_TYPE_SCALAR);
    d.columns = PromiseGetConstraintAsList("database_columns", pp);
    d.rows = PromiseGetConstraintAsList("database_rows", pp);
    d.operation = ConstraintGetRvalValue("database_operation", pp, RVAL_TYPE_SCALAR);
    d.exclude = PromiseGetConstraintAsList("registry_exclude", pp);

    value = ConstraintGetRvalValue("db_server_type", pp, RVAL_TYPE_SCALAR);
    d.db_server_type = DatabaseTypeFromString(value);

    if (value && ((d.db_server_type) == DATABASE_TYPE_NONE))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unsupported database type \"%s\" in databases promise", value);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    }

    return d;
}
