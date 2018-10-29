/*
   Copyright 2018 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#include <attributes.h>

#include <promises.h>
#include <policy.h>
#include <conversion.h>
#include <logging.h>
#include <chflags.h>
#include <audit.h>

#define CF_DEFINECLASSES "classes"
#define CF_TRANSACTION   "action"

static FilePerms GetPermissionConstraints(const EvalContext *ctx, const Promise *pp);

void ClearFilesAttributes(Attributes *whom)
{
    UidListDestroy(whom->perms.owners);
    GidListDestroy(whom->perms.groups);
}

Attributes GetFilesAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

// default for file copy

    attr.havedepthsearch = PromiseGetConstraintAsBoolean(ctx, "depth_search", pp);
    attr.haveselect = PromiseGetConstraintAsBoolean(ctx, "file_select", pp);
    attr.haverename = PromiseGetConstraintAsBoolean(ctx, "rename", pp);
    attr.havedelete = PromiseGetConstraintAsBoolean(ctx, "delete", pp);
    attr.haveperms = PromiseGetConstraintAsBoolean(ctx, "perms", pp);
    attr.havechange = PromiseGetConstraintAsBoolean(ctx, "changes", pp);
    attr.havecopy = PromiseGetConstraintAsBoolean(ctx, "copy_from", pp);
    attr.havelink = PromiseGetConstraintAsBoolean(ctx, "link_from", pp);

    attr.edit_template = PromiseGetConstraintAsRval(pp, "edit_template", RVAL_TYPE_SCALAR);
    attr.edit_template_string = PromiseGetConstraintAsRval(pp, "edit_template_string", RVAL_TYPE_SCALAR);
    attr.template_method = PromiseGetConstraintAsRval(pp, "template_method", RVAL_TYPE_SCALAR);
    attr.template_data = PromiseGetConstraintAsRval(pp, "template_data", RVAL_TYPE_CONTAINER);

    if (!attr.template_method )
    {
        attr.template_method = "cfengine";
    }

    attr.haveeditline = PromiseBundleOrBodyConstraintExists(ctx, "edit_line", pp);
    attr.haveeditxml = PromiseBundleOrBodyConstraintExists(ctx, "edit_xml", pp);
    attr.haveedit = (attr.haveeditline) || (attr.haveeditxml) || (attr.edit_template) || (attr.edit_template_string);

/* Files, specialist */

    attr.repository = PromiseGetConstraintAsRval(pp, "repository", RVAL_TYPE_SCALAR);
    attr.create = PromiseGetConstraintAsBoolean(ctx, "create", pp);
    attr.touch = PromiseGetConstraintAsBoolean(ctx, "touch", pp);
    attr.transformer = PromiseGetConstraintAsRval(pp, "transformer", RVAL_TYPE_SCALAR);
    attr.move_obstructions = PromiseGetConstraintAsBoolean(ctx, "move_obstructions", pp);
    attr.pathtype = PromiseGetConstraintAsRval(pp, "pathtype", RVAL_TYPE_SCALAR);
    attr.file_type = PromiseGetConstraintAsRval(pp, "file_type", RVAL_TYPE_SCALAR);

    attr.acl = GetAclConstraints(ctx, pp);
    attr.perms = GetPermissionConstraints(ctx, pp);
    attr.select = GetSelectConstraints(ctx, pp);
    attr.delete = GetDeleteConstraints(ctx, pp);
    attr.rename = GetRenameConstraints(ctx, pp);
    attr.change = GetChangeMgtConstraints(ctx, pp);
    attr.copy = GetCopyConstraints(ctx, pp);
    attr.link = GetLinkConstraints(ctx, pp);
    attr.edits = GetEditDefaults(ctx, pp);

    if (attr.edit_template || attr.edit_template_string)
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
    attr.havebundle = PromiseBundleOrBodyConstraintExists(ctx, "service_bundle", pp);

    return attr;
}

/*******************************************************************/

Attributes GetPackageAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.transaction = GetTransactionConstraints(ctx, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);
    attr.packages = GetPackageConstraints(ctx, pp);
    attr.new_packages = GetNewPackageConstraints(ctx, pp);
    return attr;
}

/*******************************************************************/

static User GetUserConstraints(const EvalContext *ctx, const Promise *pp)
{
    User u;
    char *value;

    value = PromiseGetConstraintAsRval(pp, "policy", RVAL_TYPE_SCALAR);
    u.policy = UserStateFromString(value);

    u.uid = PromiseGetConstraintAsRval(pp, "uid", RVAL_TYPE_SCALAR);

    value = PromiseGetConstraintAsRval(pp, "format", RVAL_TYPE_SCALAR);
    u.password_format = PasswordFormatFromString(value);
    u.password = PromiseGetConstraintAsRval(pp, "data", RVAL_TYPE_SCALAR);
    u.description = PromiseGetConstraintAsRval(pp, "description", RVAL_TYPE_SCALAR);

    u.group_primary = PromiseGetConstraintAsRval(pp, "group_primary", RVAL_TYPE_SCALAR);
    u.home_dir = PromiseGetConstraintAsRval(pp, "home_dir", RVAL_TYPE_SCALAR);
    u.shell = PromiseGetConstraintAsRval(pp, "shell", RVAL_TYPE_SCALAR);

    u.groups_secondary = PromiseGetConstraintAsList(ctx, "groups_secondary", pp);

    const Constraint *cp = PromiseGetImmediateConstraint(pp, "groups_secondary");
    u.groups_secondary_given = (cp != NULL);

    if (value && ((u.policy) == USER_STATE_NONE))
    {
        Log(LOG_LEVEL_ERR, "Unsupported user policy '%s' in users promise", value);
        PromiseRef(LOG_LEVEL_ERR, pp);
    }

    return u;
}

Attributes GetUserAttributes(const EvalContext *ctx, const Promise *pp)
{
    Attributes attr = { {0} };

    attr.havebundle = PromiseBundleOrBodyConstraintExists(ctx, "home_bundle", pp);

    attr.inherit = PromiseGetConstraintAsBoolean(ctx, "home_bundle_inherit", pp);

    attr.transaction = GetTransactionConstraints(ctx, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);
    attr.users = GetUserConstraints(ctx, pp);
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
    Attributes a = { {0} };

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

    attr.args = PromiseGetConstraintAsRval(pp, "args", RVAL_TYPE_SCALAR);
    attr.arglist = PromiseGetConstraintAsList(ctx, "arglist", pp);
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
    Attributes attr = { {0} };

    attr.signals = PromiseGetConstraintAsList(ctx, "signals", pp);
    attr.process_stop = PromiseGetConstraintAsRval(pp, "process_stop", RVAL_TYPE_SCALAR);
    attr.haveprocess_count = PromiseGetConstraintAsBoolean(ctx, "process_count", pp);
    attr.haveselect = PromiseGetConstraintAsBoolean(ctx, "process_select", pp);
    attr.restart_class = PromiseGetConstraintAsRval(pp, "restart_class", RVAL_TYPE_SCALAR);

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

    attr.havebundle = PromiseBundleOrBodyConstraintExists(ctx, "usebundle", pp);

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

    s.service_type = PromiseGetConstraintAsRval(pp, "service_type", RVAL_TYPE_SCALAR);
    s.service_policy = PromiseGetConstraintAsRval(pp, "service_policy", RVAL_TYPE_SCALAR);
    s.service_autostart_policy = PromiseGetConstraintAsRval(pp, "service_autostart_policy", RVAL_TYPE_SCALAR);
    s.service_args = PromiseGetConstraintAsRval(pp, "service_args", RVAL_TYPE_SCALAR);
    s.service_depend = PromiseGetConstraintAsList(ctx, "service_dependencies", pp);
    s.service_depend_chain = PromiseGetConstraintAsRval(pp, "service_dependence_chain", RVAL_TYPE_SCALAR);

    return s;
}

/*******************************************************************/

Environments GetEnvironmentsConstraints(const EvalContext *ctx, const Promise *pp)
{
    Environments e;

    e.cpus = PromiseGetConstraintAsInt(ctx, "env_cpus", pp);
    e.memory = PromiseGetConstraintAsInt(ctx, "env_memory", pp);
    e.disk = PromiseGetConstraintAsInt(ctx, "env_disk", pp);
    e.baseline = PromiseGetConstraintAsRval(pp, "env_baseline", RVAL_TYPE_SCALAR);
    e.spec = PromiseGetConstraintAsRval(pp, "env_spec", RVAL_TYPE_SCALAR);
    e.host = PromiseGetConstraintAsRval(pp, "environment_host", RVAL_TYPE_SCALAR);

    e.addresses = PromiseGetConstraintAsList(ctx, "env_addresses", pp);
    e.name = PromiseGetConstraintAsRval(pp, "env_name", RVAL_TYPE_SCALAR);
    e.type = PromiseGetConstraintAsRval(pp, "environment_type", RVAL_TYPE_SCALAR);
    e.state = EnvironmentStateFromString(PromiseGetConstraintAsRval(pp, "environment_state", RVAL_TYPE_SCALAR));

    return e;
}

/*******************************************************************/

ExecContain GetExecContainConstraints(const EvalContext *ctx, const Promise *pp)
{
    ExecContain e;

    e.shelltype = ShellTypeFromString(PromiseGetConstraintAsRval(pp, "useshell", RVAL_TYPE_SCALAR));
    e.umask = PromiseGetConstraintAsOctal(ctx, "umask", pp);
    e.owner = PromiseGetConstraintAsUid(ctx, "exec_owner", pp);
    e.group = PromiseGetConstraintAsGid(ctx, "exec_group", pp);
    e.preview = PromiseGetConstraintAsBoolean(ctx, "preview", pp);
    if (PromiseBundleOrBodyConstraintExists(ctx, "no_output", pp))
    {
        e.nooutput = PromiseGetConstraintAsBoolean(ctx, "no_output", pp);
    }
    else
    {
        e.nooutput = PromiseGetConstraintAsBoolean(ctx, "module", pp);
    }
    e.timeout = PromiseGetConstraintAsInt(ctx, "exec_timeout", pp);
    e.chroot = PromiseGetConstraintAsRval(pp, "chroot", RVAL_TYPE_SCALAR);
    e.chdir = PromiseGetConstraintAsRval(pp, "chdir", RVAL_TYPE_SCALAR);

    return e;
}

/*******************************************************************/

DirectoryRecursion GetRecursionConstraints(const EvalContext *ctx, const Promise *pp)
{
    DirectoryRecursion r;

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

    ac.acl_method = AclMethodFromString(PromiseGetConstraintAsRval(pp, "acl_method", RVAL_TYPE_SCALAR));
    ac.acl_type = AclTypeFromString(PromiseGetConstraintAsRval(pp, "acl_type", RVAL_TYPE_SCALAR));
    ac.acl_default = AclDefaultFromString(PromiseGetConstraintAsRval(pp, "acl_default", RVAL_TYPE_SCALAR));
    if (ac.acl_default == ACL_DEFAULT_NONE)
    {
        /* Deprecated attribute. */
        ac.acl_default = AclDefaultFromString(PromiseGetConstraintAsRval(pp, "acl_directory_inherit", RVAL_TYPE_SCALAR));
    }
    ac.acl_entries = PromiseGetConstraintAsList(ctx, "aces", pp);
    ac.acl_default_entries = PromiseGetConstraintAsList(ctx, "specify_default_aces", pp);
    if (ac.acl_default_entries == NULL)
    {
        /* Deprecated attribute. */
        ac.acl_default_entries = PromiseGetConstraintAsList(ctx, "specify_inherit_aces", pp);
    }
    ac.acl_inherit = AclInheritFromString(PromiseGetConstraintAsRval(pp, "acl_inherit", RVAL_TYPE_SCALAR));
    return ac;
}

/*******************************************************************/

static FilePerms GetPermissionConstraints(const EvalContext *ctx, const Promise *pp)
{
    FilePerms p;
    char *value;
    Rlist *list;

    value = PromiseGetConstraintAsRval(pp, "mode", RVAL_TYPE_SCALAR);

    p.plus = CF_SAMEMODE;
    p.minus = CF_SAMEMODE;

    if (!ParseModeString(value, &p.plus, &p.minus))
    {
        Log(LOG_LEVEL_ERR, "Problem validating a mode string");
        PromiseRef(LOG_LEVEL_ERR, pp);
    }

    list = PromiseGetConstraintAsList(ctx, "bsdflags", pp);

    p.plus_flags = 0;
    p.minus_flags = 0;

    if (list && (!ParseFlagString(list, &p.plus_flags, &p.minus_flags)))
    {
        Log(LOG_LEVEL_ERR, "Problem validating a BSD flag string");
        PromiseRef(LOG_LEVEL_ERR, pp);
    }

    p.owners = Rlist2UidList((Rlist *) PromiseGetConstraintAsRval(pp, "owners", RVAL_TYPE_LIST), pp);
    p.groups = Rlist2GidList((Rlist *) PromiseGetConstraintAsRval(pp, "groups", RVAL_TYPE_LIST), pp);

    p.findertype = PromiseGetConstraintAsRval(pp, "findertype", RVAL_TYPE_SCALAR);
    p.rxdirs = PromiseGetConstraintAsBoolean(ctx, "rxdirs", pp);

// The default should be true

    if (!PromiseGetConstraintAsRval(pp, "rxdirs", RVAL_TYPE_SCALAR))
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

    s.name = (Rlist *) PromiseGetConstraintAsRval(pp, "leaf_name", RVAL_TYPE_LIST);
    s.path = (Rlist *) PromiseGetConstraintAsRval(pp, "path_name", RVAL_TYPE_LIST);
    s.filetypes = (Rlist *) PromiseGetConstraintAsRval(pp, "file_types", RVAL_TYPE_LIST);
    s.issymlinkto = (Rlist *) PromiseGetConstraintAsRval(pp, "issymlinkto", RVAL_TYPE_LIST);

    s.perms = PromiseGetConstraintAsList(ctx, "search_mode", pp);

    for (rp = s.perms; rp != NULL; rp = rp->next)
    {
        plus = 0;
        minus = 0;
        value = RlistScalarValue(rp);

        if (!ParseModeString(value, &plus, &minus))
        {
            Log(LOG_LEVEL_ERR, "Problem validating a mode string");
            PromiseRef(LOG_LEVEL_ERR, pp);
        }
    }

    s.bsdflags = PromiseGetConstraintAsList(ctx, "search_bsdflags", pp);

    fplus = 0;
    fminus = 0;

    if (!ParseFlagString(s.bsdflags, &fplus, &fminus))
    {
        Log(LOG_LEVEL_ERR, "Problem validating a BSD flag string");
        PromiseRef(LOG_LEVEL_ERR, pp);
    }

    if ((s.name) || (s.path) || (s.filetypes) || (s.issymlinkto) || (s.perms) || (s.bsdflags))
    {
        entries = true;
    }

    s.owners = (Rlist *) PromiseGetConstraintAsRval(pp, "search_owners", RVAL_TYPE_LIST);
    s.groups = (Rlist *) PromiseGetConstraintAsRval(pp, "search_groups", RVAL_TYPE_LIST);

    value = PromiseGetConstraintAsRval(pp, "search_size", RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, (long *) &s.min_size, (long *) &s.max_size))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }

    value = PromiseGetConstraintAsRval(pp, "ctime", RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, (long *) &s.min_ctime, (long *) &s.max_ctime))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }

    value = PromiseGetConstraintAsRval(pp, "atime", RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, (long *) &s.min_atime, (long *) &s.max_atime))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = PromiseGetConstraintAsRval(pp, "mtime", RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, (long *) &s.min_mtime, (long *) &s.max_mtime))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }

    s.exec_regex = PromiseGetConstraintAsRval(pp, "exec_regex", RVAL_TYPE_SCALAR);
    s.exec_program = PromiseGetConstraintAsRval(pp, "exec_program", RVAL_TYPE_SCALAR);

    if ((s.owners) || (s.min_size) || (s.exec_regex) || (s.exec_program))
    {
        entries = true;
    }

    if ((s.result = PromiseGetConstraintAsRval(pp, "file_result", RVAL_TYPE_SCALAR)) == NULL)
    {
        if (!entries)
        {
            Log(LOG_LEVEL_ERR, "file_select body missing its a file_result return value");
        }
    }

    return s;
}

/*******************************************************************/

LogLevel ActionAttributeLogLevelFromString(const char *log_level)
{
    if (!log_level)
    {
        return LOG_LEVEL_ERR;
    }

    if (strcmp("inform", log_level) == 0)
    {
        return LOG_LEVEL_INFO;
    }
    else if (strcmp("verbose", log_level) == 0)
    {
        return LOG_LEVEL_VERBOSE;
    }
    else
    {
        return LOG_LEVEL_ERR;
    }
}

TransactionContext GetTransactionConstraints(const EvalContext *ctx, const Promise *pp)
{
    TransactionContext t;
    char *value;

    value = PromiseGetConstraintAsRval(pp, "action_policy", RVAL_TYPE_SCALAR);

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
    t.expireafter = PromiseGetConstraintAsInt(ctx, "expireafter", pp);

    /* Warn if promise locking was used with a promise that doesn't support it.
     * XXX: EvalContextGetPass() takes 'EvalContext *' instead of 'const EvalContext *'*/
    if ((strcmp("access", pp->parent_promise_type->name) == 0 ||
         strcmp("classes", pp->parent_promise_type->name) == 0 ||
         strcmp("defaults", pp->parent_promise_type->name) == 0 ||
         strcmp("meta", pp->parent_promise_type->name) == 0 ||
         strcmp("roles", pp->parent_promise_type->name) == 0 ||
         strcmp("vars", pp->parent_promise_type->name) == 0))
    {
        if (t.ifelapsed != CF_NOINT)
        {
            Log(LOG_LEVEL_WARNING,
                "ifelapsed attribute specified in action body for %s promise '%s',"
                " but %s promises do not support promise locking",
                pp->parent_promise_type->name, pp->promiser,
                pp->parent_promise_type->name);
        }
        if (t.expireafter != CF_NOINT)
        {
            Log(LOG_LEVEL_WARNING,
                "expireafter attribute specified in action body for %s promise '%s',"
                " but %s promises do not support promise locking",
                pp->parent_promise_type->name, pp->promiser,
                pp->parent_promise_type->name);
        }
    }

    if (t.ifelapsed == CF_NOINT)
    {
        t.ifelapsed = VIFELAPSED;
    }

    if (t.expireafter == CF_NOINT)
    {
        t.expireafter = VEXPIREAFTER;
    }

    t.audit = PromiseGetConstraintAsBoolean(ctx, "audit", pp);
    t.log_string = PromiseGetConstraintAsRval(pp, "log_string", RVAL_TYPE_SCALAR);
    t.log_priority = SyslogPriorityFromString(PromiseGetConstraintAsRval(pp, "log_priority", RVAL_TYPE_SCALAR));

    t.log_kept = PromiseGetConstraintAsRval(pp, "log_kept", RVAL_TYPE_SCALAR);
    t.log_repaired = PromiseGetConstraintAsRval(pp, "log_repaired", RVAL_TYPE_SCALAR);
    t.log_failed = PromiseGetConstraintAsRval(pp, "log_failed", RVAL_TYPE_SCALAR);

    value = PromiseGetConstraintAsRval(pp, "log_level", RVAL_TYPE_SCALAR);
    t.log_level = ActionAttributeLogLevelFromString(value);

    value = PromiseGetConstraintAsRval(pp, "report_level", RVAL_TYPE_SCALAR);
    t.report_level = ActionAttributeLogLevelFromString(value);

    t.measure_id = PromiseGetConstraintAsRval(pp, "measurement_class", RVAL_TYPE_SCALAR);

    return t;
}

/*******************************************************************/

DefineClasses GetClassDefinitionConstraints(const EvalContext *ctx, const Promise *pp)
{
    DefineClasses c;
    char *pt = NULL;

    {
        const char *context_scope = PromiseGetConstraintAsRval(pp, "scope", RVAL_TYPE_SCALAR);
        c.scope = ContextScopeFromString(context_scope);
    }
    c.change = (Rlist *) PromiseGetConstraintAsList(ctx, "promise_repaired", pp);
    c.failure = (Rlist *) PromiseGetConstraintAsList(ctx, "repair_failed", pp);
    c.denied = (Rlist *) PromiseGetConstraintAsList(ctx, "repair_denied", pp);
    c.timeout = (Rlist *) PromiseGetConstraintAsList(ctx, "repair_timeout", pp);
    c.kept = (Rlist *) PromiseGetConstraintAsList(ctx, "promise_kept", pp);

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

    pt = PromiseGetConstraintAsRval(pp, "timer_policy", RVAL_TYPE_SCALAR);

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

    value = PromiseGetConstraintAsRval(pp, "dirlinks", RVAL_TYPE_SCALAR);

    if (value && (strcmp(value, "keep") == 0))
    {
        f.dirlinks = TIDY_LINK_KEEP;
    }
    else
    {
        f.dirlinks = TIDY_LINK_DELETE;
    }

    f.rmdirs = PromiseGetConstraintAsBoolean(ctx, "rmdirs", pp);
    return f;
}

/*******************************************************************/

FileRename GetRenameConstraints(const EvalContext *ctx, const Promise *pp)
{
    FileRename r;
    char *value;

    value = PromiseGetConstraintAsRval(pp, "disable_mode", RVAL_TYPE_SCALAR);

    if (!ParseModeString(value, &r.plus, &r.minus))
    {
        Log(LOG_LEVEL_ERR, "Problem validating a mode string");
        PromiseRef(LOG_LEVEL_ERR, pp);
    }

    r.disable = PromiseGetConstraintAsBoolean(ctx, "disable", pp);
    r.disable_suffix = PromiseGetConstraintAsRval(pp, "disable_suffix", RVAL_TYPE_SCALAR);
    r.newname = PromiseGetConstraintAsRval(pp, "newname", RVAL_TYPE_SCALAR);
    r.rotate = PromiseGetConstraintAsInt(ctx, "rotate", pp);

    return r;
}

/*******************************************************************/

ENTERPRISE_FUNC_0ARG_DEFINE_STUB(HashMethod, GetBestFileChangeHashMethod)
{
    return HASH_METHOD_BEST;
}

FileChange GetChangeMgtConstraints(const EvalContext *ctx, const Promise *pp)
{
    FileChange c;
    char *value;

    value = PromiseGetConstraintAsRval(pp, "hash", RVAL_TYPE_SCALAR);

    if (value && (strcmp(value, "best") == 0))
    {
        c.hash = GetBestFileChangeHashMethod();
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
        Log(LOG_LEVEL_ERR, "FIPS mode is enabled, and md5 is not an approved algorithm");
        PromiseRef(LOG_LEVEL_ERR, pp);
    }

    value = PromiseGetConstraintAsRval(pp, "report_changes", RVAL_TYPE_SCALAR);

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

    if (PromiseGetConstraintAsRval(pp, "update_hashes", RVAL_TYPE_SCALAR))
    {
        c.update = PromiseGetConstraintAsBoolean(ctx, "update_hashes", pp);
    }
    else
    {
        c.update = GetChecksumUpdatesDefault(ctx);
    }

    c.report_diffs = PromiseGetConstraintAsBoolean(ctx, "report_diffs", pp);
    return c;
}

/*******************************************************************/

FileCopy GetCopyConstraints(const EvalContext *ctx, const Promise *pp)
{
    FileCopy f;
    long min, max;
    const char *value;

    f.source = PromiseGetConstraintAsRval(pp, "source", RVAL_TYPE_SCALAR);
    f.servers = PromiseGetConstraintAsList(ctx, "servers", pp);

    value = PromiseGetConstraintAsRval(pp, "compare", RVAL_TYPE_SCALAR);
    if (value == NULL)
    {
        value = DEFAULT_COPYTYPE;
    }
    f.compare = FileComparatorFromString(value);

    value = PromiseGetConstraintAsRval(pp, "link_type", RVAL_TYPE_SCALAR);
    f.link_type = FileLinkTypeFromString(value);

    char *protocol_version = PromiseGetConstraintAsRval(pp, "protocol_version",
                                                        RVAL_TYPE_SCALAR);

    /* Default is undefined, which leaves the choice to body common. */
    f.protocol_version = CF_PROTOCOL_UNDEFINED;
    if (protocol_version != NULL)
    {
        if (strcmp(protocol_version, "1") == 0 ||
            strcmp(protocol_version, "classic") == 0)
        {
            f.protocol_version = CF_PROTOCOL_CLASSIC;
        }
        else if (strcmp(protocol_version, "2") == 0 ||
                 strcmp(protocol_version, "latest") == 0)
        {
            f.protocol_version = CF_PROTOCOL_TLS;
        }
    }

    f.port = PromiseGetConstraintAsRval(pp, "portnumber", RVAL_TYPE_SCALAR);
    f.timeout = (short) PromiseGetConstraintAsInt(ctx, "timeout", pp);
    f.link_instead = PromiseGetConstraintAsList(ctx, "linkcopy_patterns", pp);
    f.copy_links = PromiseGetConstraintAsList(ctx, "copylink_patterns", pp);

    value = PromiseGetConstraintAsRval(pp, "copy_backup", RVAL_TYPE_SCALAR);

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

    value = PromiseGetConstraintAsRval(pp, "copy_size", RVAL_TYPE_SCALAR);
    if (!IntegerRangeFromString(value, &min, &max))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }

    f.min_size = (size_t) min;
    f.max_size = (size_t) max;

    f.trustkey = PromiseGetConstraintAsBoolean(ctx, "trustkey", pp);
    f.encrypt = PromiseGetConstraintAsBoolean(ctx, "encrypt", pp);
    f.verify = PromiseGetConstraintAsBoolean(ctx, "verify", pp);
    f.purge = PromiseGetConstraintAsBoolean(ctx, "purge", pp);
    f.missing_ok = PromiseGetConstraintAsBoolean(ctx, "missing_ok", pp);
    f.destination = NULL;

    return f;
}

/*******************************************************************/

FileLink GetLinkConstraints(const EvalContext *ctx, const Promise *pp)
{
    FileLink f;
    char *value;

    f.source = PromiseGetConstraintAsRval(pp, "source", RVAL_TYPE_SCALAR);
    value = PromiseGetConstraintAsRval(pp, "link_type", RVAL_TYPE_SCALAR);
    f.link_type = FileLinkTypeFromString(value);
    f.copy_patterns = PromiseGetConstraintAsList(ctx, "copy_patterns", pp);

    value = PromiseGetConstraintAsRval(pp, "when_no_source", RVAL_TYPE_SCALAR);

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

    value = PromiseGetConstraintAsRval(pp, "when_linking_children", RVAL_TYPE_SCALAR);

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

    if (e.maxfilesize == CF_NOINT)
    {
        e.maxfilesize = EDITFILESIZE;
    }

    value = PromiseGetConstraintAsRval(pp, "edit_backup", RVAL_TYPE_SCALAR);

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
        const char *context_scope = PromiseGetConstraintAsRval(pp, "scope", RVAL_TYPE_SCALAR);
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
    Packages p = {0};

    bool has_package_method =
            PromiseBundleOrBodyConstraintExists(ctx, "package_method", pp);
    bool has_generic_package_method = false;

    if (!has_package_method)
    {
        /* Check if we have generic package_method. */
        const Policy *policy = PolicyFromPromise(pp);
        Seq *bodies_and_args = EvalContextResolveBodyExpression(ctx, policy, "generic", "package_method");; // at position 0 we'll have the body, then its rval, then the same for each of its inherit_from parents
        if (bodies_and_args != NULL &&
            SeqLength(bodies_and_args) > 0)
        {
            const Body *bp = SeqAt(bodies_and_args, 0); // guaranteed to be non-NULL
            CopyBodyConstraintsToPromise((EvalContext*)ctx, (Promise*)pp, bp);
            has_generic_package_method = true;
        }
    }


    p.package_version = PromiseGetConstraintAsRval(pp, "package_version", RVAL_TYPE_SCALAR);
    p.package_architectures = PromiseGetConstraintAsList(ctx, "package_architectures", pp);
    p.package_select = PackageVersionComparatorFromString(PromiseGetConstraintAsRval(pp, "package_select", RVAL_TYPE_SCALAR));
    p.package_policy = PackageActionFromString(PromiseGetConstraintAsRval(pp, "package_policy", RVAL_TYPE_SCALAR));

    if (p.package_version == NULL && p.package_architectures == NULL &&
        p.package_select == PACKAGE_VERSION_COMPARATOR_NONE &&
        p.package_policy == PACKAGE_ACTION_NONE)
    {
        p.is_empty = true;
    }
    else
    {
        p.is_empty = false;
    }

    if (p.package_policy == PACKAGE_ACTION_NONE)        // Default action => package add
    {
        p.package_policy = PACKAGE_ACTION_ADD;
    }

    p.has_package_method = has_package_method | has_generic_package_method;

    /* body package_method constraints */
    p.package_add_command = PromiseGetConstraintAsRval(pp, "package_add_command", RVAL_TYPE_SCALAR);
    p.package_arch_regex = PromiseGetConstraintAsRval(pp, "package_arch_regex", RVAL_TYPE_SCALAR);
    p.package_changes = PackageActionPolicyFromString(PromiseGetConstraintAsRval(pp, "package_changes", RVAL_TYPE_SCALAR));
    p.package_delete_command = PromiseGetConstraintAsRval(pp, "package_delete_command", RVAL_TYPE_SCALAR);
    p.package_delete_convention = PromiseGetConstraintAsRval(pp, "package_delete_convention", RVAL_TYPE_SCALAR);
    p.package_file_repositories = PromiseGetConstraintAsList(ctx, "package_file_repositories", pp);
    p.package_installed_regex = PromiseGetConstraintAsRval(pp, "package_installed_regex", RVAL_TYPE_SCALAR);
    p.package_default_arch_command = PromiseGetConstraintAsRval(pp, "package_default_arch_command", RVAL_TYPE_SCALAR);
    p.package_list_arch_regex = PromiseGetConstraintAsRval(pp, "package_list_arch_regex", RVAL_TYPE_SCALAR);
    p.package_list_command = PromiseGetConstraintAsRval(pp, "package_list_command", RVAL_TYPE_SCALAR);
    p.package_name_regex = PromiseGetConstraintAsRval(pp, "package_name_regex", RVAL_TYPE_SCALAR);
    p.package_list_update_command = PromiseGetConstraintAsRval(pp, "package_list_update_command", RVAL_TYPE_SCALAR);
    p.package_list_update_ifelapsed = PromiseGetConstraintAsInt(ctx, "package_list_update_ifelapsed", pp);
    p.package_list_version_regex = PromiseGetConstraintAsRval(pp, "package_list_version_regex", RVAL_TYPE_SCALAR);
    p.package_name_convention = PromiseGetConstraintAsRval(pp, "package_name_convention", RVAL_TYPE_SCALAR);
    p.package_patch_name_regex = PromiseGetConstraintAsRval(pp, "package_patch_name_regex", RVAL_TYPE_SCALAR);
    p.package_noverify_regex = PromiseGetConstraintAsRval(pp, "package_noverify_regex", RVAL_TYPE_SCALAR);
    p.package_noverify_returncode = PromiseGetConstraintAsInt(ctx, "package_noverify_returncode", pp);
    p.package_patch_arch_regex = PromiseGetConstraintAsRval(pp, "package_patch_arch_regex", RVAL_TYPE_SCALAR);
    p.package_patch_command = PromiseGetConstraintAsRval(pp, "package_patch_command", RVAL_TYPE_SCALAR);
    p.package_patch_installed_regex = PromiseGetConstraintAsRval(pp, "package_patch_installed_regex", RVAL_TYPE_SCALAR);
    p.package_patch_list_command = PromiseGetConstraintAsRval(pp, "package_patch_list_command", RVAL_TYPE_SCALAR);
    p.package_list_name_regex = PromiseGetConstraintAsRval(pp, "package_list_name_regex", RVAL_TYPE_SCALAR);
    p.package_patch_version_regex = PromiseGetConstraintAsRval(pp, "package_patch_version_regex", RVAL_TYPE_SCALAR);
    p.package_update_command = PromiseGetConstraintAsRval(pp, "package_update_command", RVAL_TYPE_SCALAR);
    p.package_verify_command = PromiseGetConstraintAsRval(pp, "package_verify_command", RVAL_TYPE_SCALAR);
    p.package_version_regex = PromiseGetConstraintAsRval(pp, "package_version_regex", RVAL_TYPE_SCALAR);
    p.package_multiline_start = PromiseGetConstraintAsRval(pp, "package_multiline_start", RVAL_TYPE_SCALAR);
    if (PromiseGetConstraint(pp, "package_commands_useshell") == NULL)
    {
        p.package_commands_useshell = true;
    }
    else
    {
        p.package_commands_useshell = PromiseGetConstraintAsBoolean(ctx, "package_commands_useshell", pp);
    }
    p.package_version_less_command = PromiseGetConstraintAsRval(pp, "package_version_less_command", RVAL_TYPE_SCALAR);
    p.package_version_equal_command = PromiseGetConstraintAsRval(pp, "package_version_equal_command", RVAL_TYPE_SCALAR);

    return p;
}

/*******************************************************************/

static const char *new_packages_actions[] =
{
    "absent",
    "present",
    NULL
};

NewPackages GetNewPackageConstraints(const EvalContext *ctx, const Promise *pp)
{
    NewPackages p = {0};
    NewPackages empty = {0};

    p.package_version = PromiseGetConstraintAsRval(pp, "version", RVAL_TYPE_SCALAR);
    p.package_architecture = PromiseGetConstraintAsRval(pp, "architecture", RVAL_TYPE_SCALAR);
    p.package_options = PromiseGetConstraintAsList(ctx, "options", pp);

    p.is_empty = (memcmp(&p, &empty, sizeof(NewPackages)) == 0);
    p.package_policy = GetNewPackagePolicy(PromiseGetConstraintAsRval(pp, "policy", RVAL_TYPE_SCALAR),
                                           new_packages_actions);

    /* We can have only policy specified in new package promise definition. */
    if (p.package_policy != NEW_PACKAGE_ACTION_NONE)
    {
        p.is_empty = false;
    }

    /* If we have promise package manager specified.
     * IMPORTANT: this must be done after is_empty flag is set as we can have
     * some default options for new package promise specified and still use
     * old promise inside policy. */
    char *local_promise_manager =
            PromiseGetConstraintAsRval(pp, "package_module_name", RVAL_TYPE_SCALAR);
    if (local_promise_manager)
    {
        p.module_body = GetPackageModuleFromContext(ctx, local_promise_manager);
    }
    else
    {
        p.module_body = GetDefaultPackageModuleFromContext(ctx);
    }
    p.package_inventory = GetDefaultInventoryFromContext(ctx);

    /* If global options are not override by promise specific ones. */
    if (!p.package_options && p.module_body)
    {
        p.package_options = p.module_body->options;
    }

    return p;
}

/*******************************************************************/

ProcessSelect GetProcessFilterConstraints(const EvalContext *ctx, const Promise *pp)
{
    ProcessSelect p;
    char *value;
    int entries = 0;

    p.owner = PromiseGetConstraintAsList(ctx, "process_owner", pp);

    value = PromiseGetConstraintAsRval(pp, "pid", RVAL_TYPE_SCALAR);

    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_pid, &p.max_pid))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = PromiseGetConstraintAsRval(pp, "ppid", RVAL_TYPE_SCALAR);

    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_ppid, &p.max_ppid))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = PromiseGetConstraintAsRval(pp, "pgid", RVAL_TYPE_SCALAR);

    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_pgid, &p.max_pgid))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = PromiseGetConstraintAsRval(pp, "rsize", RVAL_TYPE_SCALAR);

    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_rsize, &p.max_rsize))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = PromiseGetConstraintAsRval(pp, "vsize", RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_vsize, &p.max_vsize))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = PromiseGetConstraintAsRval(pp, "ttime_range", RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, (long *) &p.min_ttime, (long *) &p.max_ttime))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = PromiseGetConstraintAsRval(pp, "stime_range", RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, (long *) &p.min_stime, (long *) &p.max_stime))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }

    p.status = PromiseGetConstraintAsRval(pp, "status", RVAL_TYPE_SCALAR);
    p.command = PromiseGetConstraintAsRval(pp, "command", RVAL_TYPE_SCALAR);
    p.tty = PromiseGetConstraintAsRval(pp, "tty", RVAL_TYPE_SCALAR);

    value = PromiseGetConstraintAsRval(pp, "priority", RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_pri, &p.max_pri))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }
    value = PromiseGetConstraintAsRval(pp, "threads", RVAL_TYPE_SCALAR);
    if (value)
    {
        entries++;
    }

    if (!IntegerRangeFromString(value, &p.min_thread, &p.max_thread))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
        FatalError(ctx, "Could not make sense of integer range [%s]", value);
    }

    if ((p.owner) || (p.status) || (p.command) || (p.tty))
    {
        entries = true;
    }

    if ((p.process_result = PromiseGetConstraintAsRval(pp, "process_result", RVAL_TYPE_SCALAR)) == NULL)
    {
        if (entries)
        {
            Log(LOG_LEVEL_ERR, "process_select body missing its a process_result return value");
        }
    }

    return p;
}

/*******************************************************************/

ProcessCount GetMatchesConstraints(const EvalContext *ctx, const Promise *pp)
{
    ProcessCount p;
    char *value;

    value = PromiseGetConstraintAsRval(pp, "match_range", RVAL_TYPE_SCALAR);
    if (!IntegerRangeFromString(value, &p.min_range, &p.max_range))
    {
        PromiseRef(LOG_LEVEL_ERR, pp);
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
    attr.location = GetLocationAttributes(pp);

    attr.sourcetype = PromiseGetConstraintAsRval(pp, "insert_type", RVAL_TYPE_SCALAR);
    attr.expandvars = PromiseGetConstraintAsBoolean(ctx, "expand_scalars", pp);

    attr.haveinsertselect = PromiseGetConstraintAsBoolean(ctx, "insert_select", pp);
    attr.line_select = GetInsertSelectConstraints(ctx, pp);

    attr.insert_match = PromiseGetConstraintAsList(ctx, "whitespace_policy", pp);

/* Common ("included") */

    attr.haveregion = PromiseGetConstraintAsBoolean(ctx, "select_region", pp);
    attr.region = GetRegionConstraints(ctx, pp);

    attr.xml = GetXmlConstraints(pp);

    attr.havetrans = PromiseGetConstraintAsBoolean(ctx, CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(ctx, pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(ctx, CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    return attr;
}

/*******************************************************************/

EditLocation GetLocationAttributes(const Promise *pp)
{
    EditLocation e;
    char *value;

    e.line_matching = PromiseGetConstraintAsRval(pp, "select_line_matching", RVAL_TYPE_SCALAR);

    value = PromiseGetConstraintAsRval(pp, "before_after", RVAL_TYPE_SCALAR);

    if (value && (strcmp(value, "before") == 0))
    {
        e.before_after = EDIT_ORDER_BEFORE;
    }
    else
    {
        e.before_after = EDIT_ORDER_AFTER;
    }

    e.first_last = PromiseGetConstraintAsRval(pp, "first_last", RVAL_TYPE_SCALAR);
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

    attr.xml = GetXmlConstraints(pp);

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
    attr.replace = GetReplaceConstraints(pp);

    attr.havecolumn = PromiseGetConstraintAsBoolean(ctx, "replace_with", pp);

    /* common */

    attr.haveregion = PromiseGetConstraintAsBoolean(ctx, "select_region", pp);
    attr.region = GetRegionConstraints(ctx, pp);

    attr.xml = GetXmlConstraints(pp);

    attr.havetrans = PromiseGetConstraintAsBoolean(ctx, CF_TRANSACTION, pp);
    attr.transaction = GetTransactionConstraints(ctx, pp);

    attr.haveclasses = PromiseGetConstraintAsBoolean(ctx, CF_DEFINECLASSES, pp);
    attr.classes = GetClassDefinitionConstraints(ctx, pp);

    return attr;
}

/*******************************************************************/

EditXml GetXmlConstraints(const Promise *pp)
{
    EditXml x;

    x.havebuildxpath = ((x.build_xpath = PromiseGetConstraintAsRval(pp, "build_xpath", RVAL_TYPE_SCALAR)) != NULL);
    x.haveselectxpath = ((x.select_xpath = PromiseGetConstraintAsRval(pp, "select_xpath", RVAL_TYPE_SCALAR)) != NULL);
    x.haveattributevalue = ((x.attribute_value = PromiseGetConstraintAsRval(pp, "attribute_value", RVAL_TYPE_SCALAR)) != NULL);

    return x;
}

/*******************************************************************/

EditRegion GetRegionConstraints(const EvalContext *ctx, const Promise *pp)
{
    EditRegion e;

    e.select_start = PromiseGetConstraintAsRval(pp, "select_start", RVAL_TYPE_SCALAR);
    e.select_end = PromiseGetConstraintAsRval(pp, "select_end", RVAL_TYPE_SCALAR);
    e.include_start = PromiseGetConstraintAsBoolean(ctx, "include_start_delimiter", pp);
    e.include_end = PromiseGetConstraintAsBoolean(ctx, "include_end_delimiter", pp);

    // set the value based on body agent control
    char *local_select_end = PromiseGetConstraintAsRval(pp,  "select_end_match_eof", RVAL_TYPE_SCALAR);
    if (local_select_end != NULL)
    {
        if (strcmp(local_select_end, "true") == 0)
        {
            e.select_end_match_eof = true;
        }
        else
        {
            e.select_end_match_eof = false;
        }
    }
    else
    {
        e.select_end_match_eof = EvalContextGetSelectEndMatchEof(ctx);
    }
    return e;
}

/*******************************************************************/

EditReplace GetReplaceConstraints(const Promise *pp)
{
    EditReplace r;

    r.replace_value = PromiseGetConstraintAsRval(pp, "replace_value", RVAL_TYPE_SCALAR);
    r.occurrences = PromiseGetConstraintAsRval(pp, "occurrences", RVAL_TYPE_SCALAR);

    return r;
}

/*******************************************************************/

EditColumn GetColumnConstraints(const EvalContext *ctx, const Promise *pp)
{
    EditColumn c;
    char *value;

    c.column_separator = PromiseGetConstraintAsRval(pp, "field_separator", RVAL_TYPE_SCALAR);
    c.select_column = PromiseGetConstraintAsInt(ctx, "select_field", pp);

    if (((c.select_column) != CF_NOINT) && (PromiseGetConstraintAsBoolean(ctx, "start_fields_from_zero", pp)))
    {
        c.select_column++;
    }

    value = PromiseGetConstraintAsRval(pp, "value_separator", RVAL_TYPE_SCALAR);

    if (value)
    {
        c.value_separator = *value;
    }
    else
    {
        c.value_separator = '\0';
    }

    c.column_value = PromiseGetConstraintAsRval(pp, "field_value", RVAL_TYPE_SCALAR);
    c.column_operation = PromiseGetConstraintAsRval(pp, "field_operation", RVAL_TYPE_SCALAR);
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

    m.mount_type = PromiseGetConstraintAsRval(pp, "mount_type", RVAL_TYPE_SCALAR);
    m.mount_source = PromiseGetConstraintAsRval(pp, "mount_source", RVAL_TYPE_SCALAR);
    m.mount_server = PromiseGetConstraintAsRval(pp, "mount_server", RVAL_TYPE_SCALAR);
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
    value = PromiseGetConstraintAsRval(pp, "freespace", RVAL_TYPE_SCALAR);

    v.freespace = (long) IntFromString(value);
    value = PromiseGetConstraintAsRval(pp, "sensible_size", RVAL_TYPE_SCALAR);
    v.sensible_size = (int) IntFromString(value);
    value = PromiseGetConstraintAsRval(pp, "sensible_count", RVAL_TYPE_SCALAR);
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

 r.result = PromiseGetConstraintAsRval(pp, "bundle_return_value_index", RVAL_TYPE_SCALAR);

    if (PromiseGetConstraintAsRval(pp, "lastseen", RVAL_TYPE_SCALAR))
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
    r.filename = PromiseGetConstraintAsRval(pp, "file_to_print", RVAL_TYPE_SCALAR);
    r.numlines = PromiseGetConstraintAsInt(ctx, "number_of_lines", pp);

    if (r.numlines == CF_NOINT)
    {
        r.numlines = 5;
    }

    r.showstate = PromiseGetConstraintAsList(ctx, "showstate", pp);

    r.friend_pattern = PromiseGetConstraintAsRval(pp, "friend_pattern", RVAL_TYPE_SCALAR);

    r.to_file = PromiseGetConstraintAsRval(pp, "report_to_file", RVAL_TYPE_SCALAR);

    if ((r.result) && ((r.haveprintfile) || (r.filename) || (r.showstate) || (r.to_file) || (r.lastseen)))
    {
        Log(LOG_LEVEL_ERR, "bundle_return_value promise for '%s' in bundle '%s' with too many constraints (ignored)", pp->promiser, PromiseGetBundle(pp)->name);
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

    m.stream_type = PromiseGetConstraintAsRval(pp, "stream_type", RVAL_TYPE_SCALAR);

    value = PromiseGetConstraintAsRval(pp, "data_type", RVAL_TYPE_SCALAR);
    m.data_type = DataTypeFromString(value);

    if (m.data_type == CF_DATA_TYPE_NONE)
    {
        m.data_type = CF_DATA_TYPE_STRING;
    }

    m.history_type = PromiseGetConstraintAsRval(pp, "history_type", RVAL_TYPE_SCALAR);
    m.select_line_matching = PromiseGetConstraintAsRval(pp, "select_line_matching", RVAL_TYPE_SCALAR);
    m.select_line_number = PromiseGetConstraintAsInt(ctx, "select_line_number", pp);
    m.policy = MeasurePolicyFromString(PromiseGetConstraintAsRval(pp, "select_multiline_policy", RVAL_TYPE_SCALAR));

    m.extraction_regex = PromiseGetConstraintAsRval(pp, "extraction_regex", RVAL_TYPE_SCALAR);
    m.units = PromiseGetConstraintAsRval(pp, "units", RVAL_TYPE_SCALAR);
    m.growing = PromiseGetConstraintAsBoolean(ctx, "track_growing_file", pp);
    return m;
}

/*******************************************************************/

Database GetDatabaseConstraints(const EvalContext *ctx, const Promise *pp)
{
    Database d;
    char *value;

    d.db_server_owner = PromiseGetConstraintAsRval(pp, "db_server_owner", RVAL_TYPE_SCALAR);
    d.db_server_password = PromiseGetConstraintAsRval(pp, "db_server_password", RVAL_TYPE_SCALAR);
    d.db_server_host = PromiseGetConstraintAsRval(pp, "db_server_host", RVAL_TYPE_SCALAR);
    d.db_connect_db = PromiseGetConstraintAsRval(pp, "db_server_connection_db", RVAL_TYPE_SCALAR);
    d.type = PromiseGetConstraintAsRval(pp, "database_type", RVAL_TYPE_SCALAR);
    d.server = PromiseGetConstraintAsRval(pp, "database_server", RVAL_TYPE_SCALAR);
    d.columns = PromiseGetConstraintAsList(ctx, "database_columns", pp);
    d.rows = PromiseGetConstraintAsList(ctx, "database_rows", pp);
    d.operation = PromiseGetConstraintAsRval(pp, "database_operation", RVAL_TYPE_SCALAR);
    d.exclude = PromiseGetConstraintAsList(ctx, "registry_exclude", pp);

    value = PromiseGetConstraintAsRval(pp, "db_server_type", RVAL_TYPE_SCALAR);
    d.db_server_type = DatabaseTypeFromString(value);

    if (value && ((d.db_server_type) == DATABASE_TYPE_NONE))
    {
        Log(LOG_LEVEL_ERR, "Unsupported database type '%s' in databases promise", value);
        PromiseRef(LOG_LEVEL_ERR, pp);
    }

    return d;
}
