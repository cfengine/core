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
#include "logging.h"
#include "chflags.h"
#include "audit.h"

#ifdef HAVE_NOVA
#include "cf.nova.h"
#endif

static int CHECKSUMUPDATES;

/*******************************************************************/

void SetChecksumUpdates(bool enabled)
{
    CHECKSUMUPDATES = enabled;
}

/*******************************************************************/

Attributes GetFilesAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    memset(&attr, 0, sizeof(attr));

// default for file copy

    attr.havedepthsearch = PromiseGetConstraintAsBoolean(ctx, "depth_search", pp);
    attr.haveselect = PromiseGetConstraintAsBoolean(ctx, "file_select", pp);
    attr.haverename = PromiseGetConstraintAsBoolean(ctx, "rename", pp);
    attr.havedelete = PromiseGetConstraintAsBoolean(ctx, "delete", pp);
    attr.haveperms = PromiseGetConstraintAsBoolean(ctx, "perms", pp);
    attr.havechange = PromiseGetConstraintAsBoolean(ctx, "changes", pp);
    attr.havecopy = PromiseGetConstraintAsBoolean(ctx, "copy_from", pp);
    attr.havelink = PromiseGetConstraintAsBoolean(ctx, "link_from", pp);

    attr.template = (char *)ConstraintGetRvalValue(ctx, "edit_template", pp, RVAL_TYPE_SCALAR);
    attr.haveeditline = PromiseBundleConstraintExists(ctx, "edit_line", pp);
    attr.haveeditxml = PromiseBundleConstraintExists(ctx, "edit_xml", pp);
    attr.haveedit = (attr.haveeditline) || (attr.haveeditxml) || (attr.template);

/* Files, specialist */

    attr.repository = (char *) ConstraintGetRvalValue(ctx, "repository", pp, RVAL_TYPE_SCALAR);
    attr.create = PromiseGetConstraintAsBoolean(ctx, "create", pp);
    attr.touch = PromiseGetConstraintAsBoolean(ctx, "touch", pp);
    attr.transformer = (char *) ConstraintGetRvalValue(ctx, "transformer", pp, RVAL_TYPE_SCALAR);
    attr.move_obstructions = PromiseGetConstraintAsBoolean(ctx, "move_obstructions", pp);
    attr.pathtype = (char *) ConstraintGetRvalValue(ctx, "pathtype", pp, RVAL_TYPE_SCALAR);

    attr.acl = GetAclConstraints(ctx, pp);
    attr.perms = GetPermissionConstraints(ctx, pp);
    attr.select = GetSelectConstraints(ctx, pp);
    attr.delete = GetDeleteConstraints(ctx, pp);
    attr.rename = GetRenameConstraints(ctx, pp);
    attr.change = GetChangeMgtConstraints(ctx, pp);
    attr.copy = GetCopyConstraints(ctx, pp);
    attr.link = GetLinkConstraints(ctx, pp);
    attr.edits = GetEditDefaults(ctx, pp);

    if (attr.template)
       {
       attr.edits.empty_before_use = true;
       attr.edits.inherit = true;
       }

/* Files, multiple use */

    attr.recursion = GetRecursionConstraints(ctx, pp);

/* Common ("included") */

    attr.havetrans = PromiseGetConstraintAsBoolean(ctx, CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(ctx, pp);
    attr.haveclasses = PromiseGetConstraintAsBoolean(ctx, CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    return attr;
}

/*******************************************************************/

Attributes GetOutputsAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(ctx, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    attr.output.promiser_type = ConstraintGetRvalValue(ctx, "promiser_type", pp, RVAL_TYPE_SCALAR);
    attr.output.level = ConstraintGetRvalValue(ctx, "output_level", pp, RVAL_TYPE_SCALAR);
    return attr;
}

/*******************************************************************/

Attributes GetReportsAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(ctx, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    attr.report = GetReportConstraints(ctx, pp);
    return attr;
}

/*******************************************************************/

Attributes GetEnvironmentsAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(ctx, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);
    attr.env = GetEnvironmentsConstraints(ctx, pp);

    return attr;
}

/*******************************************************************/

Attributes GetServicesAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(ctx, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);
    attr.service = GetServicesConstraints(ctx, pp);
    attr.havebundle = PromiseBundleConstraintExists(ctx, "service_bundle", pp);

    return attr;
}

/*******************************************************************/

Attributes GetPackageAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(ctx, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);
    attr.packages = GetPackageConstraints(ctx, pp);
    return attr;
}

/*******************************************************************/

Attributes GetDatabaseAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(ctx, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);
    attr.database = GetDatabaseConstraints(ctx, pp);
    return attr;
}

/*******************************************************************/

Attributes GetClassContextAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes a = { {0} };;

    a.transaction = GetTransactionConstraints(ctx, pp);
    a.classes = GetClassDefinitionConstraints(ctx, pp);
    a.context = GetContextConstraints(ctx, pp);

    return a;
}

/*******************************************************************/

Attributes GetExecAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.contain = GetExecContainConstraints(ctx, pp);
    attr.havecontain = PromiseGetConstraintAsBoolean(ctx, "contain", pp);

    attr.args = ConstraintGetRvalValue(ctx, "args", pp, RVAL_TYPE_SCALAR);
    attr.module = PromiseGetConstraintAsBoolean(ctx, "module", pp);

/* Common ("included") */

    attr.havetrans = PromiseGetConstraintAsBoolean(ctx, CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(ctx, pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(ctx, CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    return attr;
}

/*******************************************************************/

Attributes GetProcessAttributes(const EvalContext *ctx, const Promise *pp)
{
    static Attributes attr = { {0} };

    attr.signals = PromiseGetConstraintAsList(ctx, "signals", pp);
    attr.process_stop = (char *) ConstraintGetRvalValue(ctx, "process_stop", pp, RVAL_TYPE_SCALAR);
    attr.haveprocess_count = PromiseGetConstraintAsBoolean(ctx, "process_count", pp);
    attr.haveselect = PromiseGetConstraintAsBoolean(ctx, "process_select", pp);
    attr.restart_class = (char *) ConstraintGetRvalValue(ctx, "restart_class", pp, RVAL_TYPE_SCALAR);

    attr.process_count = GetMatchesConstraints(ctx, pp);
    attr.process_select = GetProcessFilterConstraints(ctx, pp);

/* Common ("included") */

    attr.havetrans = PromiseGetConstraintAsBoolean(ctx, CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(ctx, pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(ctx, CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    return attr;
}

/*******************************************************************/

Attributes GetStorageAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.mount = GetMountConstraints(ctx, pp);
    attr.volume = GetVolumeConstraints(ctx, pp);
    attr.havevolume = PromiseGetConstraintAsBoolean(ctx, "volume", pp);
    attr.havemount = PromiseGetConstraintAsBoolean(ctx, "mount", pp);

/* Common ("included") */

    if (attr.edits.maxfilesize <= 0)
    {
        attr.edits.maxfilesize = EDITFILESIZE;
    }

    attr.havetrans = PromiseGetConstraintAsBoolean(ctx, CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(ctx, pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(ctx, CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    return attr;
}

/*******************************************************************/

Attributes GetMethodAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havebundle = PromiseBundleConstraintExists(ctx, "usebundle", pp);

    attr.inherit = PromiseGetConstraintAsBoolean(ctx, "inherit", pp);

/* Common ("included") */

    attr.havetrans = PromiseGetConstraintAsBoolean(ctx, CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(ctx, pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(ctx, CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    return attr;
}

Attributes GetMeasurementAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.measure = GetMeasurementConstraint(ctx, pp);

/* Common ("included") */

    attr.havetrans = PromiseGetConstraintAsBoolean(ctx, CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(ctx, pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(ctx, CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    return attr;
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

Services GetServicesConstraints(const EvalContext *ctx, const Promise *pp)
{
    Services s;

    s.service_type = ConstraintGetRvalValue(ctx, "service_type", pp, RVAL_TYPE_SCALAR);
    s.service_policy = ServicePolicyFromString(ConstraintGetRvalValue(ctx, "service_policy", pp, RVAL_TYPE_SCALAR));
    s.service_autostart_policy = ConstraintGetRvalValue(ctx, "service_autostart_policy", pp, RVAL_TYPE_SCALAR);
    s.service_args = ConstraintGetRvalValue(ctx, "service_args", pp, RVAL_TYPE_SCALAR);
    s.service_depend = PromiseGetConstraintAsList(ctx, "service_dependencies", pp);
    s.service_depend_chain = ConstraintGetRvalValue(ctx, "service_dependence_chain", pp, RVAL_TYPE_SCALAR);

    return s;
}

/*******************************************************************/

Environments GetEnvironmentsConstraints(const EvalContext *ctx, const Promise *pp)
{
    Environments e;

    e.cpus = PromiseGetConstraintAsInt(ctx, "env_cpus", pp);
    e.memory = PromiseGetConstraintAsInt(ctx, "env_memory", pp);
    e.disk = PromiseGetConstraintAsInt(ctx, "env_disk", pp);
    e.baseline = ConstraintGetRvalValue(ctx, "env_baseline", pp, RVAL_TYPE_SCALAR);
    e.spec = ConstraintGetRvalValue(ctx, "env_spec", pp, RVAL_TYPE_SCALAR);
    e.host = ConstraintGetRvalValue(ctx, "environment_host", pp, RVAL_TYPE_SCALAR);

    e.addresses = PromiseGetConstraintAsList(ctx, "env_addresses", pp);
    e.name = ConstraintGetRvalValue(ctx, "env_name", pp, RVAL_TYPE_SCALAR);
    e.type = ConstraintGetRvalValue(ctx, "environment_type", pp, RVAL_TYPE_SCALAR);
    e.state = EnvironmentStateFromString(ConstraintGetRvalValue(ctx, "environment_state", pp, RVAL_TYPE_SCALAR));

    return e;
}

/*******************************************************************/

ExecContain GetExecContainConstraints(const EvalContext *ctx, const Promise *pp)
{
    ExecContain e;

    e.useshell = PromiseGetConstraintAsBoolean(ctx, "useshell", pp);
    e.umask = PromiseGetConstraintAsOctal(ctx, "umask", pp);
    e.owner = PromiseGetConstraintAsUid(ctx, "exec_owner", pp);
    e.group = PromiseGetConstraintAsGid(ctx, "exec_group", pp);
    e.preview = PromiseGetConstraintAsBoolean(ctx, "preview", pp);
    e.nooutput = PromiseGetConstraintAsBoolean(ctx, "no_output", pp);
    e.timeout = PromiseGetConstraintAsInt(ctx, "exec_timeout", pp);
    e.chroot = ConstraintGetRvalValue(ctx, "chroot", pp, RVAL_TYPE_SCALAR);
    e.chdir = ConstraintGetRvalValue(ctx, "chdir", pp, RVAL_TYPE_SCALAR);

    return e;
}

/*******************************************************************/

Recursion GetRecursionConstraints(const EvalContext *ctx, const Promise *pp)
{
    Recursion r;

    r.travlinks = PromiseGetConstraintAsBoolean(ctx, "traverse_links", pp);
    r.rmdeadlinks = PromiseGetConstraintAsBoolean(ctx, "rmdeadlinks", pp);
    r.depth = PromiseGetConstraintAsInt(ctx, "depth", pp);

    if (r.depth == CF_NOINT)
    {
        r.depth = 0;
    }

    r.xdev = PromiseGetConstraintAsBoolean(ctx, "xdev", pp);
    r.include_dirs = PromiseGetConstraintAsList(ctx, "include_dirs", pp);
    r.exclude_dirs = PromiseGetConstraintAsList(ctx, "exclude_dirs", pp);
    r.include_basedir = PromiseGetConstraintAsBoolean(ctx, "include_basedir", pp);
    return r;
}

/*******************************************************************/

Acl GetAclConstraints(const EvalContext *ctx, const Promise *pp)
{
    Acl ac;

    ac.acl_method = AclMethodFromString(ConstraintGetRvalValue(ctx, "acl_method", pp, RVAL_TYPE_SCALAR));
    ac.acl_type = AclTypeFromString(ConstraintGetRvalValue(ctx, "acl_type", pp, RVAL_TYPE_SCALAR));
    ac.acl_directory_inherit = AclInheritanceFromString(ConstraintGetRvalValue(ctx, "acl_directory_inherit", pp, RVAL_TYPE_SCALAR));
    ac.acl_entries = PromiseGetConstraintAsList(ctx, "aces", pp);
    ac.acl_inherit_entries = PromiseGetConstraintAsList(ctx, "specify_inherit_aces", pp);
    return ac;
}

/*******************************************************************/

FilePerms GetPermissionConstraints(const EvalContext *ctx, const Promise *pp)
{
    FilePerms p;
    char *value;
    Rlist *list;

    value = (char *) ConstraintGetRvalValue(ctx, "mode", pp, RVAL_TYPE_SCALAR);

    p.plus = CF_SAMEMODE;
    p.minus = CF_SAMEMODE;

    if (!ParseModeString(value, &p.plus, &p.minus))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Problem validating a mode string");
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    }

    list = PromiseGetConstraintAsList(ctx, "bsdflags", pp);

    p.plus_flags = 0;
    p.minus_flags = 0;

    if (list && (!ParseFlagString(list, &p.plus_flags, &p.minus_flags)))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Problem validating a BSD flag string");
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    }

#ifdef __MINGW32__
    p.owners = NovaWin_Rlist2SidList((Rlist *) ConstraintGetRvalValue(ctx, "owners", pp, RVAL_TYPE_LIST));
#else /* !__MINGW32__ */
    p.owners = Rlist2UidList((Rlist *) ConstraintGetRvalValue(ctx, "owners", pp, RVAL_TYPE_LIST), pp);
    p.groups = Rlist2GidList((Rlist *) ConstraintGetRvalValue(ctx, "groups", pp, RVAL_TYPE_LIST), pp);
#endif /* !__MINGW32__ */

    p.findertype = (char *) ConstraintGetRvalValue(ctx, "findertype", pp, RVAL_TYPE_SCALAR);
    p.rxdirs = PromiseGetConstraintAsBoolean(ctx, "rxdirs", pp);

// The default should be true

    if (!ConstraintGetRvalValue(ctx, "rxdirs", pp, RVAL_TYPE_SCALAR))
    {
        p.rxdirs = true;
    }

    return p;
}

/*******************************************************************/

FileSelect GetSelectConstraints(const EvalContext *ctx, const Promise *pp)
{
    FileSelect s;
    char *value;
    Rlist *rp;
    mode_t plus, minus;
    u_long fplus, fminus;
    int entries = false;

    s.name = (Rlist *) ConstraintGetRvalValue(ctx, "leaf_name", pp, RVAL_TYPE_LIST);
    s.path = (Rlist *) ConstraintGetRvalValue(ctx, "path_name", pp, RVAL_TYPE_LIST);
    s.filetypes = (Rlist *) ConstraintGetRvalValue(ctx, "file_types", pp, RVAL_TYPE_LIST);
    s.issymlinkto = (Rlist *) ConstraintGetRvalValue(ctx, "issymlinkto", pp, RVAL_TYPE_LIST);

    s.perms = PromiseGetConstraintAsList(ctx, "search_mode", pp);

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

    s.bsdflags = PromiseGetConstraintAsList(ctx, "search_bsdflags", pp);

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

    s.owners = (Rlist *) ConstraintGetRvalValue(ctx, "search_owners", pp, RVAL_TYPE_LIST);
    s.groups = (Rlist *) ConstraintGetRvalValue(ctx, "search_groups", pp, RVAL_TYPE_LIST);

    value = (char *) ConstraintGetRvalValue(ctx, "search_size", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, (long *) &s.min_size, (long *) &s.max_size))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }

    value = (char *) ConstraintGetRvalValue(ctx, "ctime", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, (long *) &s.min_ctime, (long *) &s.max_ctime))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }

    value = (char *) ConstraintGetRvalValue(ctx, "atime", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, (long *) &s.min_atime, (long *) &s.max_atime))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = (char *) ConstraintGetRvalValue(ctx, "mtime", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, (long *) &s.min_mtime, (long *) &s.max_mtime))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }

    s.exec_regex = (char *) ConstraintGetRvalValue(ctx, "exec_regex", pp, RVAL_TYPE_SCALAR);
    s.exec_program = (char *) ConstraintGetRvalValue(ctx, "exec_program", pp, RVAL_TYPE_SCALAR);

    if ((s.owners) || (s.min_size) || (s.exec_regex) || (s.exec_program))
    {
        entries = true;
    }

    if ((s.result = (char *) ConstraintGetRvalValue(ctx, "file_result", pp, RVAL_TYPE_SCALAR)) == NULL)
    {
        if (!entries)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! file_select body missing its a file_result return value");
        }
    }

    return s;
}

/*******************************************************************/

TransactionContext GetTransactionConstraints(const EvalContext *ctx, const Promise *pp)
{
    TransactionContext t;
    char *value;

    value = ConstraintGetRvalValue(ctx, "action_policy", pp, RVAL_TYPE_SCALAR);

    if (value && ((strcmp(value, "warn") == 0) || (strcmp(value, "nop") == 0)))
    {
        t.action = cfa_warn;
    }
    else
    {
        t.action = cfa_fix;     // default
    }

    t.background = PromiseGetConstraintAsBoolean(ctx, "background", pp);
    t.ifelapsed = PromiseGetConstraintAsInt(ctx, "ifelapsed", pp);

    if (t.ifelapsed == CF_NOINT)
    {
        t.ifelapsed = VIFELAPSED;
    }

    t.expireafter = PromiseGetConstraintAsInt(ctx, "expireafter", pp);

    if (t.expireafter == CF_NOINT)
    {
        t.expireafter = VEXPIREAFTER;
    }

    t.audit = PromiseGetConstraintAsBoolean(ctx, "audit", pp);
    t.log_string = ConstraintGetRvalValue(ctx, "log_string", pp, RVAL_TYPE_SCALAR);
    t.log_priority = SyslogPriorityFromString(ConstraintGetRvalValue(ctx, "log_priority", pp, RVAL_TYPE_SCALAR));

    t.log_kept = ConstraintGetRvalValue(ctx, "log_kept", pp, RVAL_TYPE_SCALAR);
    t.log_repaired = ConstraintGetRvalValue(ctx, "log_repaired", pp, RVAL_TYPE_SCALAR);
    t.log_failed = ConstraintGetRvalValue(ctx, "log_failed", pp, RVAL_TYPE_SCALAR);

    if (!PromiseGetConstraintAsReal(ctx, "value_kept", pp, &t.value_kept))
    {
        t.value_kept = 1.0;
    }

    if (!PromiseGetConstraintAsReal(ctx, "value_repaired", pp, &t.value_repaired))
    {
        t.value_repaired = 0.5;
    }

    if (!PromiseGetConstraintAsReal(ctx, "value_notkept", pp, &t.value_notkept))
    {
        t.value_notkept = -1.0;
    }

    value = ConstraintGetRvalValue(ctx, "log_level", pp, RVAL_TYPE_SCALAR);
    t.log_level = OutputLevelFromString(value);

    value = ConstraintGetRvalValue(ctx, "report_level", pp, RVAL_TYPE_SCALAR);
    t.report_level = OutputLevelFromString(value);

    t.measure_id = ConstraintGetRvalValue(ctx, "measurement_class", pp, RVAL_TYPE_SCALAR);

    return t;
}

/*******************************************************************/

DefineClasses GetClassDefinitionConstraints(const EvalContext *ctx, const Promise *pp)
{
    DefineClasses c;
    char *pt = NULL;

    {
        const char *context_scope = ConstraintGetRvalValue(ctx, "scope", pp, RVAL_TYPE_SCALAR);
        c.scope = ContextScopeFromString(context_scope);
    }
    c.change = (Rlist *) PromiseGetConstraintAsList(ctx, "promise_repaired", pp);
    c.failure = (Rlist *) PromiseGetConstraintAsList(ctx, "repair_failed", pp);
    c.denied = (Rlist *) PromiseGetConstraintAsList(ctx, "repair_denied", pp);
    c.timeout = (Rlist *) PromiseGetConstraintAsList(ctx, "repair_timeout", pp);
    c.kept = (Rlist *) PromiseGetConstraintAsList(ctx, "promise_kept", pp);
    c.interrupt = (Rlist *) PromiseGetConstraintAsList(ctx, "on_interrupt", pp);

    c.del_change = (Rlist *) PromiseGetConstraintAsList(ctx, "cancel_repaired", pp);
    c.del_kept = (Rlist *) PromiseGetConstraintAsList(ctx, "cancel_kept", pp);
    c.del_notkept = (Rlist *) PromiseGetConstraintAsList(ctx, "cancel_notkept", pp);

    c.retcode_kept = (Rlist *) PromiseGetConstraintAsList(ctx, "kept_returncodes", pp);
    c.retcode_repaired = (Rlist *) PromiseGetConstraintAsList(ctx, "repaired_returncodes", pp);
    c.retcode_failed = (Rlist *) PromiseGetConstraintAsList(ctx, "failed_returncodes", pp);

    c.persist = PromiseGetConstraintAsInt(ctx, "persist_time", pp);

    if (c.persist == CF_NOINT)
    {
        c.persist = 0;
    }

    pt = ConstraintGetRvalValue(ctx, "timer_policy", pp, RVAL_TYPE_SCALAR);

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

FileDelete GetDeleteConstraints(const EvalContext *ctx, const Promise *pp)
{
    FileDelete f;
    char *value;

    value = (char *) ConstraintGetRvalValue(ctx, "dirlinks", pp, RVAL_TYPE_SCALAR);

    if (value && (strcmp(value, "keep") == 0))
    {
        f.dirlinks = cfa_linkkeep;
    }
    else
    {
        f.dirlinks = cfa_linkdelete;
    }

    f.rmdirs = PromiseGetConstraintAsBoolean(ctx, "rmdirs", pp);
    return f;
}

/*******************************************************************/

FileRename GetRenameConstraints(const EvalContext *ctx, const Promise *pp)
{
    FileRename r;
    char *value;

    value = (char *) ConstraintGetRvalValue(ctx, "disable_mode", pp, RVAL_TYPE_SCALAR);

    if (!ParseModeString(value, &r.plus, &r.minus))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Problem validating a mode string");
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    }

    r.disable = PromiseGetConstraintAsBoolean(ctx, "disable", pp);
    r.disable_suffix = (char *) ConstraintGetRvalValue(ctx, "disable_suffix", pp, RVAL_TYPE_SCALAR);
    r.newname = (char *) ConstraintGetRvalValue(ctx, "newname", pp, RVAL_TYPE_SCALAR);
    r.rotate = PromiseGetConstraintAsInt(ctx, "rotate", pp);

    return r;
}

/*******************************************************************/

FileChange GetChangeMgtConstraints(const EvalContext *ctx, const Promise *pp)
{
    FileChange c;
    char *value;

    value = (char *) ConstraintGetRvalValue(ctx, "hash", pp, RVAL_TYPE_SCALAR);

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

    value = (char *) ConstraintGetRvalValue(ctx, "report_changes", pp, RVAL_TYPE_SCALAR);

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

    if (ConstraintGetRvalValue(ctx, "update_hashes", pp, RVAL_TYPE_SCALAR))
    {
        c.update = PromiseGetConstraintAsBoolean(ctx, "update_hashes", pp);
    }
    else
    {
        c.update = CHECKSUMUPDATES;
    }

    c.report_diffs = PromiseGetConstraintAsBoolean(ctx, "report_diffs", pp);
    return c;
}

/*******************************************************************/

FileCopy GetCopyConstraints(const EvalContext *ctx, const Promise *pp)
{
    FileCopy f;
    char *value;
    long min, max;

    f.source = (char *) ConstraintGetRvalValue(ctx, "source", pp, RVAL_TYPE_SCALAR);

    value = (char *) ConstraintGetRvalValue(ctx, "compare", pp, RVAL_TYPE_SCALAR);

    if (value == NULL)
    {
        value = DEFAULT_COPYTYPE;
    }

    f.compare = FileComparatorFromString(value);

    value = (char *) ConstraintGetRvalValue(ctx, "link_type", pp, RVAL_TYPE_SCALAR);

    f.link_type = FileLinkTypeFromString(value);
    f.servers = PromiseGetConstraintAsList(ctx, "servers", pp);
    f.portnumber = (short) PromiseGetConstraintAsInt(ctx, "portnumber", pp);
    f.timeout = (short) PromiseGetConstraintAsInt(ctx, "timeout", pp);
    f.link_instead = PromiseGetConstraintAsList(ctx, "linkcopy_patterns", pp);
    f.copy_links = PromiseGetConstraintAsList(ctx, "copylink_patterns", pp);

    value = (char *) ConstraintGetRvalValue(ctx, "copy_backup", pp, RVAL_TYPE_SCALAR);

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

    f.stealth = PromiseGetConstraintAsBoolean(ctx, "stealth", pp);
    f.collapse = PromiseGetConstraintAsBoolean(ctx, "collapse_destination_dir", pp);
    f.preserve = PromiseGetConstraintAsBoolean(ctx, "preserve", pp);
    f.type_check = PromiseGetConstraintAsBoolean(ctx, "type_check", pp);
    f.force_update = PromiseGetConstraintAsBoolean(ctx, "force_update", pp);
    f.force_ipv4 = PromiseGetConstraintAsBoolean(ctx, "force_ipv4", pp);
    f.check_root = PromiseGetConstraintAsBoolean(ctx, "check_root", pp);

    value = (char *) ConstraintGetRvalValue(ctx, "copy_size", pp, RVAL_TYPE_SCALAR);
    if (!IntegerRangeFromString(value, &min, &max))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }

    f.min_size = (size_t) min;
    f.max_size = (size_t) max;

    f.trustkey = PromiseGetConstraintAsBoolean(ctx, "trustkey", pp);
    f.encrypt = PromiseGetConstraintAsBoolean(ctx, "encrypt", pp);
    f.verify = PromiseGetConstraintAsBoolean(ctx, "verify", pp);
    f.purge = PromiseGetConstraintAsBoolean(ctx, "purge", pp);
    f.destination = NULL;

    return f;
}

/*******************************************************************/

FileLink GetLinkConstraints(const EvalContext *ctx, const Promise *pp)
{
    FileLink f;
    char *value;

    f.source = (char *) ConstraintGetRvalValue(ctx, "source", pp, RVAL_TYPE_SCALAR);
    value = (char *) ConstraintGetRvalValue(ctx, "link_type", pp, RVAL_TYPE_SCALAR);
    f.link_type = FileLinkTypeFromString(value);
    f.copy_patterns = PromiseGetConstraintAsList(ctx, "copy_patterns", pp);

    value = (char *) ConstraintGetRvalValue(ctx, "when_no_source", pp, RVAL_TYPE_SCALAR);

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

    value = (char *) ConstraintGetRvalValue(ctx, "when_linking_children", pp, RVAL_TYPE_SCALAR);

    if (value && (strcmp(value, "override_file") == 0))
    {
        f.when_linking_children = cfa_override;
    }
    else
    {
        f.when_linking_children = cfa_onlynonexisting;
    }

    f.link_children = PromiseGetConstraintAsBoolean(ctx, "link_children", pp);

    return f;
}

/*******************************************************************/

EditDefaults GetEditDefaults(const EvalContext *ctx, const Promise *pp)
{
    EditDefaults e = { 0 };
    char *value;

    e.maxfilesize = PromiseGetConstraintAsInt(ctx, "max_file_size", pp);

    if ((e.maxfilesize == CF_NOINT) || (e.maxfilesize == 0))
    {
        e.maxfilesize = EDITFILESIZE;
    }

    value = (char *) ConstraintGetRvalValue(ctx, "edit_backup", pp, RVAL_TYPE_SCALAR);

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
        e.rotate = PromiseGetConstraintAsInt(ctx, "rotate", pp);
    }
    else
    {
        e.backup = BACKUP_OPTION_BACKUP;
    }

    e.empty_before_use = PromiseGetConstraintAsBoolean(ctx, "empty_file_before_editing", pp);

    e.joinlines = PromiseGetConstraintAsBoolean(ctx, "recognize_join", pp);

    e.inherit = PromiseGetConstraintAsBoolean(ctx, "inherit", pp);

    return e;
}

/*******************************************************************/

ContextConstraint GetContextConstraints(const EvalContext *ctx, const Promise *pp)
{
    ContextConstraint a;

    a.nconstraints = 0;
    a.expression = NULL;
    a.persistent = PromiseGetConstraintAsInt(ctx, "persistence", pp);

    {
        const char *context_scope = ConstraintGetRvalValue(ctx, "scope", pp, RVAL_TYPE_SCALAR);
        a.scope = ContextScopeFromString(context_scope);
    }

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        for (int k = 0; CF_CLASSBODY[k].lval != NULL; k++)
        {
            if (strcmp(cp->lval, "persistence") == 0 || strcmp(cp->lval, "scope") == 0)
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

Packages GetPackageConstraints(const EvalContext *ctx, const Promise *pp)
{
    Packages p;
    PackageAction action;
    PackageVersionComparator operator;
    PackageActionPolicy change_policy;

    p.have_package_methods = PromiseGetConstraintAsBoolean(ctx, "havepackage_method", pp);
    p.package_version = (char *) ConstraintGetRvalValue(ctx, "package_version", pp, RVAL_TYPE_SCALAR);
    p.package_architectures = PromiseGetConstraintAsList(ctx, "package_architectures", pp);

    action = PackageActionFromString((char *) ConstraintGetRvalValue(ctx, "package_policy", pp, RVAL_TYPE_SCALAR));
    p.package_policy = action;

    if (p.package_policy == PACKAGE_ACTION_NONE)        // Default action => package add
    {
        p.package_policy = PACKAGE_ACTION_ADD;
    }

    operator = PackageVersionComparatorFromString((char *) ConstraintGetRvalValue(ctx, "package_select", pp, RVAL_TYPE_SCALAR));

    p.package_select = operator;
    change_policy = PackageActionPolicyFromString((char *) ConstraintGetRvalValue(ctx, "package_changes", pp, RVAL_TYPE_SCALAR));
    p.package_changes = change_policy;

    p.package_file_repositories = PromiseGetConstraintAsList(ctx, "package_file_repositories", pp);

    p.package_default_arch_command = (char *) ConstraintGetRvalValue(ctx, "package_default_arch_command", pp, RVAL_TYPE_SCALAR);

    p.package_patch_list_command = (char *) ConstraintGetRvalValue(ctx, "package_patch_list_command", pp, RVAL_TYPE_SCALAR);
    p.package_patch_name_regex = (char *) ConstraintGetRvalValue(ctx, "package_patch_name_regex", pp, RVAL_TYPE_SCALAR);
    p.package_patch_arch_regex = (char *) ConstraintGetRvalValue(ctx, "package_patch_arch_regex", pp, RVAL_TYPE_SCALAR);
    p.package_patch_version_regex = (char *) ConstraintGetRvalValue(ctx, "package_patch_version_regex", pp, RVAL_TYPE_SCALAR);
    p.package_patch_installed_regex = (char *) ConstraintGetRvalValue(ctx, "package_patch_installed_regex", pp, RVAL_TYPE_SCALAR);

    p.package_list_update_command = (char *) ConstraintGetRvalValue(ctx, "package_list_update_command", pp, RVAL_TYPE_SCALAR);
    p.package_list_update_ifelapsed = PromiseGetConstraintAsInt(ctx, "package_list_update_ifelapsed", pp);
    p.package_list_command = (char *) ConstraintGetRvalValue(ctx, "package_list_command", pp, RVAL_TYPE_SCALAR);
    p.package_list_version_regex = (char *) ConstraintGetRvalValue(ctx, "package_list_version_regex", pp, RVAL_TYPE_SCALAR);
    p.package_list_name_regex = (char *) ConstraintGetRvalValue(ctx, "package_list_name_regex", pp, RVAL_TYPE_SCALAR);
    p.package_list_arch_regex = (char *) ConstraintGetRvalValue(ctx, "package_list_arch_regex", pp, RVAL_TYPE_SCALAR);

    p.package_installed_regex = (char *) ConstraintGetRvalValue(ctx, "package_installed_regex", pp, RVAL_TYPE_SCALAR);

    p.package_version_regex = (char *) ConstraintGetRvalValue(ctx, "package_version_regex", pp, RVAL_TYPE_SCALAR);
    p.package_name_regex = (char *) ConstraintGetRvalValue(ctx, "package_name_regex", pp, RVAL_TYPE_SCALAR);
    p.package_arch_regex = (char *) ConstraintGetRvalValue(ctx, "package_arch_regex", pp, RVAL_TYPE_SCALAR);

    p.package_add_command = (char *) ConstraintGetRvalValue(ctx, "package_add_command", pp, RVAL_TYPE_SCALAR);
    p.package_delete_command = (char *) ConstraintGetRvalValue(ctx, "package_delete_command", pp, RVAL_TYPE_SCALAR);
    p.package_update_command = (char *) ConstraintGetRvalValue(ctx, "package_update_command", pp, RVAL_TYPE_SCALAR);
    p.package_patch_command = (char *) ConstraintGetRvalValue(ctx, "package_patch_command", pp, RVAL_TYPE_SCALAR);
    p.package_verify_command = (char *) ConstraintGetRvalValue(ctx, "package_verify_command", pp, RVAL_TYPE_SCALAR);
    p.package_noverify_regex = (char *) ConstraintGetRvalValue(ctx, "package_noverify_regex", pp, RVAL_TYPE_SCALAR);
    p.package_noverify_returncode = PromiseGetConstraintAsInt(ctx, "package_noverify_returncode", pp);

    if (PromiseGetConstraint(ctx, pp, "package_commands_useshell") == NULL)
    {
        p.package_commands_useshell = true;
    }
    else
    {
        p.package_commands_useshell = PromiseGetConstraintAsBoolean(ctx, "package_commands_useshell", pp);
    }

    p.package_name_convention = (char *) ConstraintGetRvalValue(ctx, "package_name_convention", pp, RVAL_TYPE_SCALAR);
    p.package_delete_convention = (char *) ConstraintGetRvalValue(ctx, "package_delete_convention", pp, RVAL_TYPE_SCALAR);

    p.package_multiline_start = (char *) ConstraintGetRvalValue(ctx, "package_multiline_start", pp, RVAL_TYPE_SCALAR);

    p.package_version_equal_command = ConstraintGetRvalValue(ctx, "package_version_equal_command", pp, RVAL_TYPE_SCALAR);
    p.package_version_less_command = ConstraintGetRvalValue(ctx, "package_version_less_command", pp, RVAL_TYPE_SCALAR);

    return p;
}

/*******************************************************************/

ProcessSelect GetProcessFilterConstraints(const EvalContext *ctx, const Promise *pp)
{
    ProcessSelect p;
    char *value;
    int entries = 0;

    p.owner = PromiseGetConstraintAsList(ctx, "process_owner", pp);

    value = (char *) ConstraintGetRvalValue(ctx, "pid", pp, RVAL_TYPE_SCALAR);

    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_pid, &p.max_pid))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = (char *) ConstraintGetRvalValue(ctx, "ppid", pp, RVAL_TYPE_SCALAR);

    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_ppid, &p.max_ppid))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = (char *) ConstraintGetRvalValue(ctx, "pgid", pp, RVAL_TYPE_SCALAR);

    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_pgid, &p.max_pgid))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = (char *) ConstraintGetRvalValue(ctx, "rsize", pp, RVAL_TYPE_SCALAR);

    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_rsize, &p.max_rsize))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = (char *) ConstraintGetRvalValue(ctx, "vsize", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_vsize, &p.max_vsize))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = (char *) ConstraintGetRvalValue(ctx, "ttime_range", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, (long *) &p.min_ttime, (long *) &p.max_ttime))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = (char *) ConstraintGetRvalValue(ctx, "stime_range", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, (long *) &p.min_stime, (long *) &p.max_stime))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }

    p.status = (char *) ConstraintGetRvalValue(ctx, "status", pp, RVAL_TYPE_SCALAR);
    p.command = (char *) ConstraintGetRvalValue(ctx, "command", pp, RVAL_TYPE_SCALAR);
    p.tty = (char *) ConstraintGetRvalValue(ctx, "tty", pp, RVAL_TYPE_SCALAR);

    value = (char *) ConstraintGetRvalValue(ctx, "priority", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_pri, &p.max_pri))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = (char *) ConstraintGetRvalValue(ctx, "threads", pp, RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_thread, &p.max_thread))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }

    if ((p.owner) || (p.status) || (p.command) || (p.tty))
    {
        entries = true;
    }

    if ((p.process_result = (char *) ConstraintGetRvalValue(ctx, "process_result", pp, RVAL_TYPE_SCALAR)) == NULL)
    {
        if (entries)
        {
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! process_select body missing its a process_result return value");
        }
    }

    return p;
}

/*******************************************************************/

ProcessCount GetMatchesConstraints(const EvalContext *ctx, const Promise *pp)
{
    ProcessCount p;
    char *value;

    value = (char *) ConstraintGetRvalValue(ctx, "match_range", pp, RVAL_TYPE_SCALAR);
    if (!IntegerRangeFromString(value, &p.min_range, &p.max_range))
    {
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    p.in_range_define = PromiseGetConstraintAsList(ctx, "in_range_define", pp);
    p.out_of_range_define = PromiseGetConstraintAsList(ctx, "out_of_range_define", pp);

    return p;
}

Attributes GetInsertionAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havelocation = PromiseGetConstraintAsBoolean(ctx, "location", pp);
    attr.location = GetLocationAttributes(ctx, pp);

    attr.sourcetype = ConstraintGetRvalValue(ctx, "insert_type", pp, RVAL_TYPE_SCALAR);
    attr.expandvars = PromiseGetConstraintAsBoolean(ctx, "expand_scalars", pp);

    attr.haveinsertselect = PromiseGetConstraintAsBoolean(ctx, "insert_select", pp);
    attr.line_select = GetInsertSelectConstraints(ctx, pp);

    attr.insert_match = PromiseGetConstraintAsList(ctx, "whitespace_policy", pp);

/* Common ("included") */

    attr.haveregion = PromiseGetConstraintAsBoolean(ctx, "select_region", pp);
    attr.region = GetRegionConstraints(ctx, pp);

    attr.xml = GetXmlConstraints(ctx, pp);

    attr.havetrans = PromiseGetConstraintAsBoolean(ctx, CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(ctx, pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(ctx, CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    return attr;
}

/*******************************************************************/

EditLocation GetLocationAttributes(const EvalContext *ctx, const Promise *pp)
{
    EditLocation e;
    char *value;

    e.line_matching = ConstraintGetRvalValue(ctx, "select_line_matching", pp, RVAL_TYPE_SCALAR);

    value = ConstraintGetRvalValue(ctx, "before_after", pp, RVAL_TYPE_SCALAR);

    if (value && (strcmp(value, "before") == 0))
    {
        e.before_after = EDIT_ORDER_BEFORE;
    }
    else
    {
        e.before_after = EDIT_ORDER_AFTER;
    }

    e.first_last = ConstraintGetRvalValue(ctx, "first_last", pp, RVAL_TYPE_SCALAR);
    return e;
}

/*******************************************************************/

Attributes GetDeletionAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.not_matching = PromiseGetConstraintAsBoolean(ctx, "not_matching", pp);

    attr.havedeleteselect = PromiseGetConstraintAsBoolean(ctx, "delete_select", pp);
    attr.line_select = GetDeleteSelectConstraints(ctx, pp);

    /* common */

    attr.haveregion = PromiseGetConstraintAsBoolean(ctx, "select_region", pp);
    attr.region = GetRegionConstraints(ctx, pp);

    attr.xml = GetXmlConstraints(ctx, pp);

    attr.havetrans = PromiseGetConstraintAsBoolean(ctx, CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(ctx, pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(ctx, CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    return attr;
}

/*******************************************************************/

Attributes GetColumnAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havecolumn = PromiseGetConstraintAsBoolean(ctx, "edit_field", pp);
    attr.column = GetColumnConstraints(ctx, pp);

    /* common */

    attr.haveregion = PromiseGetConstraintAsBoolean(ctx, "select_region", pp);
    attr.region = GetRegionConstraints(ctx, pp);

    attr.havetrans = PromiseGetConstraintAsBoolean(ctx, CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(ctx, pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(ctx, CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    return attr;
}

/*******************************************************************/

Attributes GetReplaceAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havereplace = PromiseGetConstraintAsBoolean(ctx, "replace_patterns", pp);
    attr.replace = GetReplaceConstraints(ctx, pp);

    attr.havecolumn = PromiseGetConstraintAsBoolean(ctx, "replace_with", pp);

    /* common */

    attr.haveregion = PromiseGetConstraintAsBoolean(ctx, "select_region", pp);
    attr.region = GetRegionConstraints(ctx, pp);

    attr.xml = GetXmlConstraints(ctx, pp);

    attr.havetrans = PromiseGetConstraintAsBoolean(ctx, CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(ctx, pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(ctx, CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    return attr;
}

/*******************************************************************/

EditXml GetXmlConstraints(const EvalContext *ctx, const Promise *pp)
{
    EditXml x;

    x.havebuildxpath = ((x.build_xpath = ConstraintGetRvalValue(ctx, "build_xpath", pp, RVAL_TYPE_SCALAR)) != NULL);
    x.haveselectxpath = ((x.select_xpath = ConstraintGetRvalValue(ctx, "select_xpath", pp, RVAL_TYPE_SCALAR)) != NULL);
    x.haveattributevalue = ((x.attribute_value = ConstraintGetRvalValue(ctx, "attribute_value", pp, RVAL_TYPE_SCALAR)) != NULL);

    return x;
}

/*******************************************************************/

EditRegion GetRegionConstraints(const EvalContext *ctx, const Promise *pp)
{
    EditRegion e;

    e.select_start = ConstraintGetRvalValue(ctx, "select_start", pp, RVAL_TYPE_SCALAR);
    e.select_end = ConstraintGetRvalValue(ctx, "select_end", pp, RVAL_TYPE_SCALAR);
    e.include_start = PromiseGetConstraintAsBoolean(ctx, "include_start_delimiter", pp);
    e.include_end = PromiseGetConstraintAsBoolean(ctx, "include_end_delimiter", pp);
    return e;
}

/*******************************************************************/

EditReplace GetReplaceConstraints(const EvalContext *ctx, const Promise *pp)
{
    EditReplace r;

    r.replace_value = ConstraintGetRvalValue(ctx, "replace_value", pp, RVAL_TYPE_SCALAR);
    r.occurrences = ConstraintGetRvalValue(ctx, "occurrences", pp, RVAL_TYPE_SCALAR);

    return r;
}

/*******************************************************************/

EditColumn GetColumnConstraints(const EvalContext *ctx, const Promise *pp)
{
    EditColumn c;
    char *value;

    c.column_separator = ConstraintGetRvalValue(ctx, "field_separator", pp, RVAL_TYPE_SCALAR);
    c.select_column = PromiseGetConstraintAsInt(ctx, "select_field", pp);

    if (((c.select_column) != CF_NOINT) && (PromiseGetConstraintAsBoolean(ctx, "start_fields_from_zero", pp)))
    {
        c.select_column++;
    }

    value = ConstraintGetRvalValue(ctx, "value_separator", pp, RVAL_TYPE_SCALAR);

    if (value)
    {
        c.value_separator = *value;
    }
    else
    {
        c.value_separator = '\0';
    }

    c.column_value = ConstraintGetRvalValue(ctx, "field_value", pp, RVAL_TYPE_SCALAR);
    c.column_operation = ConstraintGetRvalValue(ctx, "field_operation", pp, RVAL_TYPE_SCALAR);
    c.extend_columns = PromiseGetConstraintAsBoolean(ctx, "extend_fields", pp);
    c.blanks_ok = PromiseGetConstraintAsBoolean(ctx, "allow_blank_fields", pp);
    return c;
}

/*******************************************************************/
/* Storage                                                         */
/*******************************************************************/

StorageMount GetMountConstraints(const EvalContext *ctx, const Promise *pp)
{
    StorageMount m;

    m.mount_type = ConstraintGetRvalValue(ctx, "mount_type", pp, RVAL_TYPE_SCALAR);
    m.mount_source = ConstraintGetRvalValue(ctx, "mount_source", pp, RVAL_TYPE_SCALAR);
    m.mount_server = ConstraintGetRvalValue(ctx, "mount_server", pp, RVAL_TYPE_SCALAR);
    m.mount_options = PromiseGetConstraintAsList(ctx, "mount_options", pp);
    m.editfstab = PromiseGetConstraintAsBoolean(ctx, "edit_fstab", pp);
    m.unmount = PromiseGetConstraintAsBoolean(ctx, "unmount", pp);

    return m;
}

/*******************************************************************/

StorageVolume GetVolumeConstraints(const EvalContext *ctx, const Promise *pp)
{
    StorageVolume v;
    char *value;

    v.check_foreign = PromiseGetConstraintAsBoolean(ctx, "check_foreign", pp);
    value = ConstraintGetRvalValue(ctx, "freespace", pp, RVAL_TYPE_SCALAR);

    v.freespace = (long) IntFromString(value);
    value = ConstraintGetRvalValue(ctx, "sensible_size", pp, RVAL_TYPE_SCALAR);
    v.sensible_size = (int) IntFromString(value);
    value = ConstraintGetRvalValue(ctx, "sensible_count", pp, RVAL_TYPE_SCALAR);
    v.sensible_count = (int) IntFromString(value);
    v.scan_arrivals = PromiseGetConstraintAsBoolean(ctx, "scan_arrivals", pp);

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

Report GetReportConstraints(const EvalContext *ctx, const Promise *pp)
{
 Report r = {0};
 
 r.result = ConstraintGetRvalValue(ctx, "bundle_return_value_index", pp, RVAL_TYPE_SCALAR);
    
    if (ConstraintGetRvalValue(ctx, "lastseen", pp, RVAL_TYPE_SCALAR))
    {
        r.havelastseen = true;
        r.lastseen = PromiseGetConstraintAsInt(ctx, "lastseen", pp);

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

    if (!PromiseGetConstraintAsReal(ctx, "intermittency", pp, &r.intermittency))
    {
        r.intermittency = 0;
    }

    r.haveprintfile = PromiseGetConstraintAsBoolean(ctx, "printfile", pp);
    r.filename = (char *) ConstraintGetRvalValue(ctx, "file_to_print", pp, RVAL_TYPE_SCALAR);
    r.numlines = PromiseGetConstraintAsInt(ctx, "number_of_lines", pp);

    if (r.numlines == CF_NOINT)
    {
        r.numlines = 5;
    }

    r.showstate = PromiseGetConstraintAsList(ctx, "showstate", pp);

    r.friend_pattern = ConstraintGetRvalValue(ctx, "friend_pattern", pp, RVAL_TYPE_SCALAR);

    r.to_file = ConstraintGetRvalValue(ctx, "report_to_file", pp, RVAL_TYPE_SCALAR);

    if ((r.result) && ((r.haveprintfile) || (r.filename) || (r.showstate) || (r.to_file) || (r.lastseen)))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! bundle_return_value promise for \"%s\" in bundle \"%s\" with too many constraints (ignored)", pp->promiser, PromiseGetBundle(pp)->name);
    }
    
    return r;
}

/*******************************************************************/

LineSelect GetInsertSelectConstraints(const EvalContext *ctx, const Promise *pp)
{
    LineSelect s;

    s.startwith_from_list = PromiseGetConstraintAsList(ctx, "insert_if_startwith_from_list", pp);
    s.not_startwith_from_list = PromiseGetConstraintAsList(ctx, "insert_if_not_startwith_from_list", pp);
    s.match_from_list = PromiseGetConstraintAsList(ctx, "insert_if_match_from_list", pp);
    s.not_match_from_list = PromiseGetConstraintAsList(ctx, "insert_if_not_match_from_list", pp);
    s.contains_from_list = PromiseGetConstraintAsList(ctx, "insert_if_contains_from_list", pp);
    s.not_contains_from_list = PromiseGetConstraintAsList(ctx, "insert_if_not_contains_from_list", pp);

    return s;
}

/*******************************************************************/

LineSelect GetDeleteSelectConstraints(const EvalContext *ctx, const Promise *pp)
{
    LineSelect s;

    s.startwith_from_list = PromiseGetConstraintAsList(ctx, "delete_if_startwith_from_list", pp);
    s.not_startwith_from_list = PromiseGetConstraintAsList(ctx, "delete_if_not_startwith_from_list", pp);
    s.match_from_list = PromiseGetConstraintAsList(ctx, "delete_if_match_from_list", pp);
    s.not_match_from_list = PromiseGetConstraintAsList(ctx, "delete_if_not_match_from_list", pp);
    s.contains_from_list = PromiseGetConstraintAsList(ctx, "delete_if_contains_from_list", pp);
    s.not_contains_from_list = PromiseGetConstraintAsList(ctx, "delete_if_not_contains_from_list", pp);

    return s;
}

/*******************************************************************/

Measurement GetMeasurementConstraint(const EvalContext *ctx, const Promise *pp)
{
    Measurement m;
    char *value;

    m.stream_type = ConstraintGetRvalValue(ctx, "stream_type", pp, RVAL_TYPE_SCALAR);

    value = ConstraintGetRvalValue(ctx, "data_type", pp, RVAL_TYPE_SCALAR);
    m.data_type = DataTypeFromString(value);

    if (m.data_type == DATA_TYPE_NONE)
    {
        m.data_type = DATA_TYPE_STRING;
    }

    m.history_type = ConstraintGetRvalValue(ctx, "history_type", pp, RVAL_TYPE_SCALAR);
    m.select_line_matching = ConstraintGetRvalValue(ctx, "select_line_matching", pp, RVAL_TYPE_SCALAR);
    m.select_line_number = PromiseGetConstraintAsInt(ctx, "select_line_number", pp);
    m.policy = MeasurePolicyFromString(ConstraintGetRvalValue(ctx, "select_multiline_policy", pp, RVAL_TYPE_SCALAR));
    
    m.extraction_regex = ConstraintGetRvalValue(ctx, "extraction_regex", pp, RVAL_TYPE_SCALAR);
    m.units = ConstraintGetRvalValue(ctx, "units", pp, RVAL_TYPE_SCALAR);
    m.growing = PromiseGetConstraintAsBoolean(ctx, "track_growing_file", pp);
    return m;
}

/*******************************************************************/

Database GetDatabaseConstraints(const EvalContext *ctx, const Promise *pp)
{
    Database d;
    char *value;

    d.db_server_owner = ConstraintGetRvalValue(ctx, "db_server_owner", pp, RVAL_TYPE_SCALAR);
    d.db_server_password = ConstraintGetRvalValue(ctx, "db_server_password", pp, RVAL_TYPE_SCALAR);
    d.db_server_host = ConstraintGetRvalValue(ctx, "db_server_host", pp, RVAL_TYPE_SCALAR);
    d.db_connect_db = ConstraintGetRvalValue(ctx, "db_server_connection_db", pp, RVAL_TYPE_SCALAR);
    d.type = ConstraintGetRvalValue(ctx, "database_type", pp, RVAL_TYPE_SCALAR);
    d.server = ConstraintGetRvalValue(ctx, "database_server", pp, RVAL_TYPE_SCALAR);
    d.columns = PromiseGetConstraintAsList(ctx, "database_columns", pp);
    d.rows = PromiseGetConstraintAsList(ctx, "database_rows", pp);
    d.operation = ConstraintGetRvalValue(ctx, "database_operation", pp, RVAL_TYPE_SCALAR);
    d.exclude = PromiseGetConstraintAsList(ctx, "registry_exclude", pp);

    value = ConstraintGetRvalValue(ctx, "db_server_type", pp, RVAL_TYPE_SCALAR);
    d.db_server_type = DatabaseTypeFromString(value);

    if (value && ((d.db_server_type) == DATABASE_TYPE_NONE))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Unsupported database type \"%s\" in databases promise", value);
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
    }

    return d;
}
