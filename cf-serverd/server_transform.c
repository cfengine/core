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

#include <server_transform.h>

#include <server.h>

#include <misc_lib.h>
#include <eval_context.h>
#include <files_names.h>
#include <mod_common.h>
#include <mod_access.h>
#include <item_lib.h>
#include <conversion.h>
#include <ornaments.h>
#include <expand.h>
#include <scope.h>
#include <vars.h>
#include <attributes.h>
#include <communication.h>
#include <string_lib.h>
#include <rlist.h>
#include <cf-serverd-enterprise-stubs.h>
#include <syslog_client.h>
#include <verify_classes.h>
#include <verify_vars.h>
#include <generic_agent.h> /* HashControls */
#include <file_lib.h>      /* IsDirReal */
#include <matching.h>      /* IsRegex */
#include <net.h>
#include <client_code.h>
#include <cfnet.h>

#include "server_common.h"                         /* PreprocessRequestPath */
#include "server_access.h"
#include "strlist.h"
#include <cleanup.h>


static PromiseResult KeepServerPromise(EvalContext *ctx, const Promise *pp, void *param);
static void InstallServerAuthPath(const char *path, Auth **list, Auth **listtail);
static void KeepServerRolePromise(EvalContext *ctx, const Promise *pp);
static void KeepPromiseBundles(EvalContext *ctx, const Policy *policy);
static void KeepControlPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config);
static void KeepBundlesAccessPromise(EvalContext *ctx, const Promise *pp);
static Auth *GetAuthPath(const char *path, Auth *list);


extern int COLLECT_INTERVAL;
extern int COLLECT_WINDOW;
extern bool SERVER_LISTEN;


/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

extern int CFD_MAXPROCESSES;
extern int NO_FORK;
extern bool DENYBADCLOCKS;
extern int MAXTRIES;
extern bool LOGENCRYPT;

/*******************************************************************/

static void KeepFileAccessPromise(const EvalContext *ctx, const Promise *pp);
static void KeepLiteralAccessPromise(EvalContext *ctx, const Promise *pp, const char *type);
static void KeepQueryAccessPromise(EvalContext *ctx, const Promise *pp);

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/


void Summarize()
{
    Auth *ptr;
    Item *ip, *ipr;

    Log(LOG_LEVEL_VERBOSE, " === BEGIN summary of access promises === ");

    Log(LOG_LEVEL_VERBOSE,
        "Host IPs allowed connection access (allowconnects):");
    for (ip = SERVER_ACCESS.nonattackerlist; ip != NULL; ip = ip->next)
    {
        Log(LOG_LEVEL_VERBOSE, "\tIP: %s", ip->name);
    }

    Log(LOG_LEVEL_VERBOSE,
        "Host IPs denied connection access (denyconnects):");
    for (ip = SERVER_ACCESS.attackerlist; ip != NULL; ip = ip->next)
    {
        Log(LOG_LEVEL_VERBOSE, "\tIP: %s", ip->name);
    }

    Log(LOG_LEVEL_VERBOSE,
        "Host IPs allowed multiple connection access (allowallconnects):");
    for (ip = SERVER_ACCESS.multiconnlist; ip != NULL; ip = ip->next)
    {
        Log(LOG_LEVEL_VERBOSE, "\tIP: %s", ip->name);
    }

    Log(LOG_LEVEL_VERBOSE,
        "Host IPs whose keys we shall establish trust to (trustkeysfrom):");
    for (ip = SERVER_ACCESS.trustkeylist; ip != NULL; ip = ip->next)
    {
        Log(LOG_LEVEL_VERBOSE, "\tIP: %s", ip->name);
    }

    Log(LOG_LEVEL_VERBOSE,
        "Host IPs allowed legacy connections (allowlegacyconnects):");
    for (ip = SERVER_ACCESS.allowlegacyconnects; ip != NULL; ip = ip->next)
    {
        Log(LOG_LEVEL_VERBOSE, "\tIP: %s", ip->name);
    }

    Log(LOG_LEVEL_VERBOSE,
        "Users from whom we accept cf-runagent connections (allowusers):");
    for (ip = SERVER_ACCESS.allowuserlist; ip != NULL; ip = ip->next)
    {
        Log(LOG_LEVEL_VERBOSE, "\tUSER: %s", ip->name);
    }

    Log(LOG_LEVEL_VERBOSE, "Access control lists:");
    acl_Summarise(paths_acl, "Path");
    acl_Summarise(classes_acl, "Class");
    acl_Summarise(vars_acl, "Variable");
    acl_Summarise(literals_acl, "Literal");
    acl_Summarise(query_acl, "Query");
    acl_Summarise(roles_acl, "Role");
    acl_Summarise(bundles_acl, "Bundle");

    Log(LOG_LEVEL_VERBOSE,
        "Access control lists for the classic network protocol:");

    for (ptr = SERVER_ACCESS.admit; ptr != NULL; ptr = ptr->next)
    {
        /* Don't report empty entries. */
        if (ptr->maproot != NULL || ptr->accesslist != NULL)
        {
            Log(LOG_LEVEL_VERBOSE, "\tPath: %s", ptr->path);
        }

        for (ipr = ptr->maproot; ipr != NULL; ipr = ipr->next)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tmaproot user: %s,", ipr->name);
        }

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tadmit: %s", ip->name);
        }
    }

    for (ptr = SERVER_ACCESS.deny; ptr != NULL; ptr = ptr->next)
    {
        /* Don't report empty entries. */
        if (ptr->accesslist != NULL)
        {
            Log(LOG_LEVEL_VERBOSE, "\tPath: %s", ptr->path);
        }

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tdeny: %s", ip->name);
        }
    }

    for (ptr = SERVER_ACCESS.varadmit; ptr != NULL; ptr = ptr->next)
    {
        Log(LOG_LEVEL_VERBOSE, "Object: %s", ptr->path);

        for (ipr = ptr->maproot; ipr != NULL; ipr = ipr->next)
        {
            Log(LOG_LEVEL_VERBOSE, "%s,", ipr->name);
        }
        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            Log(LOG_LEVEL_VERBOSE, "Admit '%s' root=", ip->name);
        }
    }

    for (ptr = SERVER_ACCESS.vardeny; ptr != NULL; ptr = ptr->next)
    {
        Log(LOG_LEVEL_VERBOSE, "Object %s", ptr->path);

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            Log(LOG_LEVEL_VERBOSE, "Deny '%s'", ip->name);
        }
    }

    Log(LOG_LEVEL_VERBOSE, " === END summary of access promises === ");
}

void KeepPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config)
{
    if (paths_acl    != NULL || classes_acl != NULL || vars_acl    != NULL ||
        literals_acl != NULL || query_acl   != NULL || bundles_acl != NULL ||
        roles_acl    != NULL || SERVER_ACCESS.path_shortcuts != NULL)
    {
        UnexpectedError("ACLs are not NULL - we are probably leaking memory!");
    }

    paths_acl     = calloc(1, sizeof(*paths_acl));
    classes_acl   = calloc(1, sizeof(*classes_acl));
    vars_acl      = calloc(1, sizeof(*vars_acl));
    literals_acl  = calloc(1, sizeof(*literals_acl));
    query_acl     = calloc(1, sizeof(*query_acl));
    bundles_acl   = calloc(1, sizeof(*bundles_acl));
    roles_acl     = calloc(1, sizeof(*roles_acl));
    SERVER_ACCESS.path_shortcuts = StringMapNew();

    if (paths_acl    == NULL || classes_acl == NULL || vars_acl    == NULL ||
        literals_acl == NULL || query_acl   == NULL || bundles_acl == NULL ||
        roles_acl    == NULL || SERVER_ACCESS.path_shortcuts == NULL)
    {
        Log(LOG_LEVEL_CRIT, "calloc: %s", GetErrorStr());
        DoCleanupAndExit(255);
    }

    KeepControlPromises(ctx, policy, config);
    KeepPromiseBundles(ctx, policy);
}

/*******************************************************************/

static bool SetMaxOpenFiles(int n)
#ifdef HAVE_SYS_RESOURCE_H
{
    const struct rlimit lim = {
        .rlim_cur = n,
        .rlim_max = n,
    };
    int ret = setrlimit(RLIMIT_NOFILE, &lim);
    if (ret == -1)
    {
        Log(LOG_LEVEL_INFO, "Failed setting max open files limit"
            " (setrlimit(NOFILE, %d): %s)", n, GetErrorStr());
        Log(LOG_LEVEL_INFO,
            "Please ensure that 'nofile' ulimit is at least 2x maxconnections");
        return false;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Setting max open files rlimit to %d", n);
        return true;
    }
}
#else  /* MinGW */
{
    Log(LOG_LEVEL_VERBOSE,
        "Platform does not support setrlimit(NOFILE), max open files not set");
    return false;
}
#endif  /* HAVE_SYS_RESOURCE_H */

static void KeepControlPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config)
{
    CFD_MAXPROCESSES = 30;
    MAXTRIES = 5;
    DENYBADCLOCKS = true;
    CFRUNCOMMAND[0] = '\0';
    SetChecksumUpdatesDefault(ctx, true);

    /* Keep promised agent behaviour - control bodies */

    Banner("Server control promises..");

    PolicyResolve(ctx, policy, config);

    /* Now expand */

    Seq *constraints = ControlBodyConstraints(policy, AGENT_TYPE_SERVER);

#define IsControlBody(e) (strcmp(cp->lval, CFS_CONTROLBODY[e].lval) == 0)

    if (constraints)
    {
        for (size_t i = 0; i < SeqLength(constraints); i++)
        {
            Constraint *cp = SeqAt(constraints, i);

            if (!IsDefinedClass(ctx, cp->classes))
            {
                continue;
            }

            VarRef *ref = VarRefParseFromScope(cp->lval, "control_server");
            const void *value = EvalContextVariableGet(ctx, ref, NULL);
            VarRefDestroy(ref);

            if (IsControlBody(SERVER_CONTROL_SERVER_FACILITY))
            {
                SetFacility(value);
            }
            else if (IsControlBody(SERVER_CONTROL_DENY_BAD_CLOCKS))
            {
                DENYBADCLOCKS = BooleanFromString(value);
                Log(LOG_LEVEL_VERBOSE,
                    "Setting denybadclocks to '%s'",
                    DENYBADCLOCKS ? "true" : "false");
            }
            else if (IsControlBody(SERVER_CONTROL_LOG_ENCRYPTED_TRANSFERS))
            {
                LOGENCRYPT = BooleanFromString(value);
                Log(LOG_LEVEL_VERBOSE,
                    "Setting logencrypt to '%s'",
                    LOGENCRYPT ? "true" : "false");
            }
            else if (IsControlBody(SERVER_CONTROL_LOG_ALL_CONNECTIONS))
            {
                SERVER_ACCESS.logconns = BooleanFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting logconns to %d", SERVER_ACCESS.logconns);
            }
            else if (IsControlBody(SERVER_CONTROL_MAX_CONNECTIONS))
            {
                CFD_MAXPROCESSES = (int) IntFromString(value);

                /* Ease apoptosis limits. */
                MAXTRIES = CFD_MAXPROCESSES / 3;

                /* The handling of max_readers in LMDB is not ideal, but
                 * here is how it is right now: We know that both cf-serverd and
                 * cf-hub will access the lastseen database. Worst case every
                 * single thread and process will do it at the same time, and
                 * this has in fact been observed. So we add the maximum of
                 * those two values together to provide a safe ceiling. In
                 * addition, cf-agent can access the database occasionally as
                 * well, so add a few extra for that too. */
                Log(LOG_LEVEL_VERBOSE,
                    "Setting maxconnections to %d", CFD_MAXPROCESSES);
                DBSetMaximumConcurrentTransactions(CFD_MAXPROCESSES
                                                   + EnterpriseGetMaxCfHubProcesses() + 10);

                /* Set RLIMIT_NOFILE to be enough for all threads. */
                SetMaxOpenFiles(CFD_MAXPROCESSES * 2 + 10);
            }
            else if (IsControlBody(SERVER_CONTROL_CALL_COLLECT_INTERVAL))
            {
                COLLECT_INTERVAL = (int) 60 * IntFromString(value);
                Log(LOG_LEVEL_VERBOSE,
                    "Setting call_collect_interval to %d (seconds)",
                    COLLECT_INTERVAL);
            }
            else if (IsControlBody(SERVER_CONTROL_LISTEN))
            {
                SERVER_LISTEN = BooleanFromString(value);
                Log(LOG_LEVEL_VERBOSE,
                    "Setting server listen to '%s' ",
                    SERVER_LISTEN ? "true" : "false");
            }
            else if (IsControlBody(SERVER_CONTROL_CALL_COLLECT_WINDOW))
            {
                COLLECT_WINDOW = (int) IntFromString(value);
                Log(LOG_LEVEL_VERBOSE,
                    "Setting collect_window to %d (seconds)",
                    COLLECT_INTERVAL);
            }
            else if (IsControlBody(SERVER_CONTROL_CFRUNCOMMAND))
            {
                if (strlen(value) >= sizeof(CFRUNCOMMAND))
                {
                    Log(LOG_LEVEL_ERR,
                        "cfruncommand too long (>%zu), leaving empty",
                        sizeof(CFRUNCOMMAND));
                }
                else
                {
                    memcpy(CFRUNCOMMAND, value, strlen(value) + 1);
                    Log(LOG_LEVEL_VERBOSE, "Setting cfruncommand to: %s",
                        CFRUNCOMMAND);
                }
            }
            else if (IsControlBody(SERVER_CONTROL_ALLOW_CONNECTS))
            {
                Log(LOG_LEVEL_VERBOSE, "Setting allowing connections from ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SERVER_ACCESS.nonattackerlist, RlistScalarValue(rp)))
                    {
                        PrependItem(&SERVER_ACCESS.nonattackerlist, RlistScalarValue(rp), cp->classes);
                    }
                }
            }
            else if (IsControlBody(SERVER_CONTROL_DENY_CONNECTS))
            {
                Log(LOG_LEVEL_VERBOSE, "Setting denying connections from ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SERVER_ACCESS.attackerlist, RlistScalarValue(rp)))
                    {
                        PrependItem(&SERVER_ACCESS.attackerlist, RlistScalarValue(rp), cp->classes);
                    }
                }
            }
            else if (IsControlBody(SERVER_CONTROL_SKIP_VERIFY))
            {
                /* Skip. */
            }
            else if (IsControlBody(SERVER_CONTROL_ALLOW_ALL_CONNECTS))
            {
                Log(LOG_LEVEL_VERBOSE, "Setting allowing multiple connections from ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SERVER_ACCESS.multiconnlist, RlistScalarValue(rp)))
                    {
                        PrependItem(&SERVER_ACCESS.multiconnlist, RlistScalarValue(rp), cp->classes);
                    }
                }
            }
            else if (IsControlBody(SERVER_CONTROL_ALLOW_USERS))
            {
                Log(LOG_LEVEL_VERBOSE, "SET Allowing users ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SERVER_ACCESS.allowuserlist, RlistScalarValue(rp)))
                    {
                        PrependItem(&SERVER_ACCESS.allowuserlist, RlistScalarValue(rp), cp->classes);
                    }
                }
            }
            else if (IsControlBody(SERVER_CONTROL_TRUST_KEYS_FROM))
            {
                Log(LOG_LEVEL_VERBOSE, "Setting 'trustkeysfrom' ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SERVER_ACCESS.trustkeylist, RlistScalarValue(rp)))
                    {
                        PrependItem(&SERVER_ACCESS.trustkeylist, RlistScalarValue(rp), cp->classes);
                    }
                }
            }
            else if (IsControlBody(SERVER_CONTROL_ALLOWLEGACYCONNECTS))
            {
                Log(LOG_LEVEL_VERBOSE, "Setting 'allowlegacyconnects' ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SERVER_ACCESS.allowlegacyconnects, RlistScalarValue(rp)))
                    {
                        PrependItem(&SERVER_ACCESS.allowlegacyconnects, RlistScalarValue(rp), cp->classes);
                    }
                }
            }
            else if (IsControlBody(SERVER_CONTROL_PORT_NUMBER))
            {
                bool ret = SetCfenginePort(value);
                assert(ret);
            }
            else if (IsControlBody(SERVER_CONTROL_BIND_TO_INTERFACE))
            {
                SetBindInterface(value);
            }
            else if (IsControlBody(SERVER_CONTROL_ALLOWCIPHERS))
            {
                assert(SERVER_ACCESS.allowciphers == NULL);                /* no leak */
                SERVER_ACCESS.allowciphers = xstrdup(value);
                Log(LOG_LEVEL_VERBOSE, "Setting allowciphers to: %s",
                    SERVER_ACCESS.allowciphers);
            }
            else if (IsControlBody(SERVER_CONTROL_ALLOWTLSVERSION))
            {
                assert(SERVER_ACCESS.allowtlsversion == NULL);             /* no leak */
                SERVER_ACCESS.allowtlsversion = xstrdup(value);
                Log(LOG_LEVEL_VERBOSE, "Setting allowtlsversion to: %s",
                    SERVER_ACCESS.allowtlsversion);
            }
        }

#undef IsControlBody

    }

    const void *value = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_SYSLOG_HOST);
    if (value)
    {
        /* Don't resolve syslog_host now, better do it per log request. */
        if (!SetSyslogHost(value))
        {
            Log(LOG_LEVEL_ERR, "Failed to set syslog_host, '%s' too long", (const char *)value);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Setting syslog_host to '%s'", (const char *)value);
        }
    }

    value = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_SYSLOG_PORT);
    if (value)
    {
        SetSyslogPort(IntFromString(value));
    }

    value = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_FIPS_MODE);
    if (value)
    {
        FIPS_MODE = BooleanFromString(value);
        Log(LOG_LEVEL_VERBOSE, "Setting FIPS mode to to '%s'", FIPS_MODE ? "true" : "false");
    }

    value = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_LASTSEEN_EXPIRE_AFTER);
    if (value)
    {
        LASTSEENEXPIREAFTER = IntFromString(value) * 60;
    }

    value = EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_BWLIMIT);
    if (value)
    {
        double bval;
        if (DoubleFromString(value, &bval))
        {
            bwlimit_kbytes = (uint32_t) ( bval / 1000.0);
            Log(LOG_LEVEL_VERBOSE, "Setting rate limit to %d kBytes/sec", bwlimit_kbytes);
        }
    }

}

/*********************************************************************/

/* Sequence in which server promise types should be evaluated */
static const char *const SERVER_TYPESEQUENCE[] =
{
    "meta",
    "vars",
    "classes",
    "roles",
    "access",
    NULL
};

static const char *const COMMON_TYPESEQUENCE[] =
{
    "meta",
    "vars",
    "classes",
    "reports",
    NULL
};

/* Check if promise is NOT belonging to default server types 
 * (see SERVER_TYPESEQUENCE)*/
static bool IsPromiseTypeNotInTypeSequence(const char *promise_type,
                                           const char * const *seq)
{
    for (int type = 0; seq[type] != NULL; type++)
    {
        if (strcmp(promise_type, seq[type]) == 0)
        {
            return false;
        }
    }
    return true;
}

static void EvaluateBundle(EvalContext *ctx, const Bundle *bp, const char * const *seq)
{
    EvalContextStackPushBundleFrame(ctx, bp, NULL, false);

    for (int type = 0; seq[type] != NULL; type++)
    {
        const PromiseType *sp = BundleGetPromiseType((Bundle *)bp, seq[type]);

        /* Some promise types might not be there. */
        if (!sp || SeqLength(sp->promises) == 0)
        {
            Log(LOG_LEVEL_DEBUG, "No promise type %s in bundle %s",
                                 seq[type], bp->name);
            continue;
        }

        EvalContextStackPushPromiseTypeFrame(ctx, sp);
        for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
        {
            Promise *pp = SeqAt(sp->promises, ppi);
            ExpandPromise(ctx, pp, KeepServerPromise, NULL);
        }
        EvalContextStackPopFrame(ctx);
    }

    /* Check if we are having some other promise types which we
     * should evaluate. THIS IS ONLY FOR BACKWARD COMPATIBILITY! */
    for (size_t j = 0; j < SeqLength(bp->promise_types); j++)
    {
        PromiseType *sp = SeqAt(bp->promise_types, j);

        /* Skipping evaluation of promise as this was evaluated in
         * loop above. */
        if (!IsPromiseTypeNotInTypeSequence(sp->name, seq))
        {
            Log(LOG_LEVEL_DEBUG, "Skipping subsequent evaluation of "
                    "promise type %s in bundle %s", sp->name, bp->name);
            continue;
        }

        Log(LOG_LEVEL_WARNING, "Trying to evaluate unsupported/obsolete "
                    "promise type %s in %s bundle %s", sp->name, bp->type, bp->name);

        EvalContextStackPushPromiseTypeFrame(ctx, sp);
        for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
        {
            Promise *pp = SeqAt(sp->promises, ppi);
            ExpandPromise(ctx, pp, KeepServerPromise, NULL);
        }
        EvalContextStackPopFrame(ctx);

    }

    EvalContextStackPopFrame(ctx);
}

static void KeepPromiseBundles(EvalContext *ctx, const Policy *policy)
{
    /* Dial up the generic promise expansion with a callback */

    CleanReportBookFilterSet();

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);
        bool server_bundle = strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_SERVER]) == 0;
        bool common_bundle = strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_COMMON]) == 0;

        if (server_bundle || common_bundle)
        {
            if (RlistLen(bp->args) > 0)
            {
                Log(LOG_LEVEL_WARNING,
                    "Cannot implicitly evaluate bundle '%s %s', as this bundle takes arguments.",
                    bp->type, bp->name);
                continue;
            }
        }

        if (server_bundle)
        {
            EvaluateBundle(ctx, bp, SERVER_TYPESEQUENCE);
        }

        else if (common_bundle)
        {
            EvaluateBundle(ctx, bp, COMMON_TYPESEQUENCE);
        }
    }
}

static PromiseResult KeepServerPromise(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert(!param);
    PromiseBanner(ctx, pp);

    if (strcmp(pp->parent_promise_type->name, "vars") == 0)
    {
        return VerifyVarPromise(ctx, pp, NULL);
    }

    if (strcmp(pp->parent_promise_type->name, "classes") == 0)
    {
        return VerifyClassPromise(ctx, pp, NULL);
    }

    /* Warn if promise locking was used with a promise that doesn't support it.
     * That applies to both of the below promise types while 'vars' and
     * 'classes' handle this on their own. */
    int ifelapsed = PromiseGetConstraintAsInt(ctx, "ifelapsed", pp);
    if (ifelapsed != CF_NOINT)
    {
        Log(LOG_LEVEL_WARNING,
            "ifelapsed attribute specified in action body for %s promise '%s',"
            " but %s promises do not support promise locking",
            pp->parent_promise_type->name, pp->promiser,
            pp->parent_promise_type->name);
    }
    int expireafter = PromiseGetConstraintAsInt(ctx, "expireafter", pp);
    if (expireafter != CF_NOINT)
    {
        Log(LOG_LEVEL_WARNING,
            "expireafter attribute specified in action body for %s promise '%s',"
            " but %s promises do not support promise locking",
            pp->parent_promise_type->name, pp->promiser,
            pp->parent_promise_type->name);
    }

    if (strcmp(pp->parent_promise_type->name, "access") == 0)
    {
        const char *resource_type =
            PromiseGetConstraintAsRval(pp, "resource_type", RVAL_TYPE_SCALAR);

        /* Default resource_type in access_rules is "path" */
        if (resource_type == NULL ||
            strcmp(resource_type, "path") == 0)
        {
            KeepFileAccessPromise(ctx, pp);
            return PROMISE_RESULT_NOOP;
        }
        else if (strcmp(resource_type, "literal") == 0)
        {
            KeepLiteralAccessPromise(ctx, pp, "literal");
            return PROMISE_RESULT_NOOP;
        }
        else if (strcmp(resource_type, "variable") == 0)
        {
            KeepLiteralAccessPromise(ctx, pp, "variable");
            return PROMISE_RESULT_NOOP;
        }
        else if (strcmp(resource_type, "query") == 0)
        {
            KeepQueryAccessPromise(ctx, pp);
            KeepReportDataSelectAccessPromise(pp);
            return PROMISE_RESULT_NOOP;
        }
        else if (strcmp(resource_type, "context") == 0)
        {
            KeepLiteralAccessPromise(ctx, pp, "context");
            return PROMISE_RESULT_NOOP;
        }
        else if (strcmp(resource_type, "bundle") == 0)
        {
            KeepBundlesAccessPromise(ctx, pp);
            return PROMISE_RESULT_NOOP;
        }
    }
    else if (strcmp(pp->parent_promise_type->name, "roles") == 0)
    {
        KeepServerRolePromise(ctx, pp);
        return PROMISE_RESULT_NOOP;
    }

    return PROMISE_RESULT_NOOP;
}

/*********************************************************************/

enum admit_type
{
    ADMIT_TYPE_IP,
    ADMIT_TYPE_HOSTNAME,
    ADMIT_TYPE_KEY,
    ADMIT_TYPE_OTHER
};

/* Check if the given string is an IP subnet, a hostname, a key, or none of
 * the above. */
static enum admit_type AdmitType(const char *s)
{
    if (strncmp(s, "SHA=", strlen("SHA=")) == 0 ||
        strncmp(s, "MD5=", strlen("MD5=")) == 0)
    {
        return ADMIT_TYPE_KEY;
    }
    /* IPv4 or IPv6 subnet mask or regex. */
    /* TODO change this to "0123456789abcdef.:/", no regex allowed. */
    else if (s[strspn(s, "0123456789abcdef.:/[-]*()\\")] == '\0')
    {
        return ADMIT_TYPE_IP;
    }
    else
    {
        return ADMIT_TYPE_HOSTNAME;
    }
}

/**
 * Map old-style regex-or-hostname #host to new-style host-or-domain
 * and append it to #sl.
 *
 * Old-style ACLs could include regexes to be matched against host
 * names; but new-style ones only support sub-domain matching.  If the
 * old-style host regex looks like ".*\.sub\.domain\.tld" we can take
 * it in as ".sub.domain.tld"; otherwise, we can only really map exact
 * match hostnames.  However, we know some old policy (including our
 * own masterfiles) had cases of .*sub.domain.tld and it's possible
 * that someone might include a new-style .sub.domain.tld by mistake
 * in an (old-style) accept list; so cope with these cases, too.
 *
 * @param sl The string-list to which to add entries.
 * @param host The name-or-regex to add to the ACL.
 * @return An index at which an entry was added to the list (there may
 * be another), or -1 if nothing added.
 */
static size_t StrList_AppendRegexHostname(StrList **sl, const char *host)
{
    if (IsRegex(host))
    {
        if (host[strcspn(host, "({[|+?]})")] != '\0')
        {
            return -1; /* Not a regex we can sensibly massage; discard. */
        }
        bool skip[2] = { false, false }; /* { domain, host } passes below */
        const char *name = host;
        if (name[0] == '^') /* Was always implicit; but read as hint to intent. */
        {
            /* Default to skipping domain-form if anchored explicitly: */
            skip[0] = true; /* Over-ridden below if followed by .* of course. */
            name++;
        }
        if (StringStartsWith(name, ".*"))
        {
            skip[0] = false; /* Domain-form should match */
            name += 2;
        }
        if (StringStartsWith(name, "\\."))
        {
            /* Skip host-form, as the regex definitely wants something
             * before the given name. */
            skip[1] = true;
            name += 2;
        }
        if (strchr(name, '*') != NULL)
        {
            /* Can't handle a * later than the preamble. */
            return (size_t) -1;
        }

        if (name > host ||
            strchr(host, '\\') != NULL)
        {
            /* 2: leading '.' and final '\0' */
            char copy[2 + strlen(name)], *c = copy;
            c++[0] = '.'; /* For domain-form; and copy+1 gives host-form. */
            /* Now copy the rest of the name, de-regex-ifying as we go: */
            for (const char *p = name; p[0] != '\0'; p++)
            {
                if (p[0] == '\\')
                {
                    p++;
                    if (p[0] != '.')
                    {
                        /* Regex includes a non-dot escape */
                        return (size_t) -1;
                    }
                }
#if 0
                else if (p[0] == '.')
                {
                    /* In principle, this is a special character; but
                     * it may just be an unescaped dot, so let it be. */
                }
#endif
                c++[0] = p[0];
            }
            assert(c < copy + sizeof(copy));
            c[0] = '\0';

            /* Now, for host then domain, add entry if suitable */
            int pass = 2;
            size_t ret = -1;
            while (pass > 0)
            {
                pass--;
                if (!skip[pass]) /* pass 0 is domain, pass 1 is host */
                {
                    ret = StrList_Append(sl, copy + pass);
                }
            }
            return ret;
        }

        /* IsRegex() is true but we treat it just as a name! */
    }
    /* Just a simple host name. */

    return StrList_Append(sl, host);
}

bool NEED_REVERSE_LOOKUP = false;

static void TurnOnReverseLookups()
{
    if (!NEED_REVERSE_LOOKUP)
    {
        Log(LOG_LEVEL_INFO,
            "Found hostname admit/deny in access_rules, "
            "turning on reverse DNS lookups for every connection");
        NEED_REVERSE_LOOKUP = true;
    }

}

static size_t racl_SmartAppend(struct admitdeny_acl *ad, const char *entry)
{
    size_t ret;

    switch (AdmitType(entry))
    {

    case ADMIT_TYPE_IP:
        /* TODO convert IP string to binary representation. */
        ret = StrList_Append(&ad->ips, entry);
        break;

    case ADMIT_TYPE_KEY:
        ret = StrList_Append(&ad->keys, entry);
        break;

    case ADMIT_TYPE_HOSTNAME:
        ret = StrList_AppendRegexHostname(&ad->hostnames, entry);

        /* If any hostname rule got added,
         * turn on reverse DNS lookup in the new protocol. */
        if (ret != (size_t) -1)
        {
            TurnOnReverseLookups();
        }

        break;

    default:
        Log(LOG_LEVEL_WARNING,
            "Access rule 'admit: %s' is not IP, hostname or key, ignoring",
            entry);
        ret = (size_t) -1;
    }

    return ret;
}

/* Package hostname as regex, if needed.
 *
 * @param old The old Auth structure to which to add.
 * @param host The new acl_hostnames entry to add to it.
 */
static void NewHostToOldACL(Auth *old, const char *host)
{
    if (host[0] == '.') /* Domain - transform to regex: */
    {
        int extra = 2; /* For leading ".*" */
        const char *dot = host;

        do
        {
            do
            {
                dot++; /* Step over prior dot. */
            } while (dot[0] == '.'); /* Treat many dots as one. */
            extra++; /* For a backslash before the dot */
            dot = strchr(dot, '.');
        } while (dot);

        char regex[strlen(host) + extra], *dst = regex;
        dst++[0] = '.';
        dst++[0] = '*';

        dot = host;
        do
        {
            /* Insert literal dot. */
            assert(dot[0] == '.');
            dst++[0] = '\\';
            dst++[0] = '.';

            do /* Step over prior dot(s), as before. */
            {
                dot++;
            } while (dot[0] == '.');

            /* Identify next fragment: */
            const char *d = strchr(dot, '.');
            size_t len = d ? d - dot : strlen(dot);

            /* Copy fragment: */
            memcpy(dst, dot, len);
            dst += len;

            /* Advance: */
            dot = d;
        } while (dot);

        /* Terminate: */
        assert(dst < regex + sizeof(regex));
        dst[0] = '\0';

        /* Add to list: */
        PrependItem(&(old->accesslist), regex, NULL);
    }
    else
    {
        /* Simple host-name; just add it: */
        PrependItem(&(old->accesslist), host, NULL);
    }
}

/**
 * Add access rules to the given ACL #acl according to the constraints in the
 * particular access promise.
 *
 * For legacy reasons (non-TLS connections), build also the #ap (access Auth)
 * and #dp (deny Auth), if they are not NULL.
 */
static void AccessPromise_AddAccessConstraints(const EvalContext *ctx,
                                               const Promise *pp,
                                               struct resource_acl *racl,
                                               Auth *ap, Auth *dp)
{
    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        const Constraint *cp = SeqAt(pp->conlist, i);
        size_t ret = -2;

        if (!IsDefinedClass(ctx, cp->classes))
        {
            continue;
        }

        switch (cp->rval.type)
        {
#define IsAccessBody(e) (strcmp(cp->lval, CF_REMACCESS_BODIES[e].lval) == 0)

        case RVAL_TYPE_SCALAR:

            if (ap != NULL &&
                IsAccessBody(REMOTE_ACCESS_IFENCRYPTED))
            {
                ap->encrypt = BooleanFromString(cp->rval.item);
            }
            else if (IsAccessBody(REMOTE_ACCESS_SHORTCUT))
            {
                const char *shortcut = cp->rval.item;

                if (strchr(shortcut, FILE_SEPARATOR) != NULL)
                {
                    Log(LOG_LEVEL_ERR,
                        "slashes are forbidden in ACL shortcut: %s",
                        shortcut);
                }
                else if (StringMapHasKey(SERVER_ACCESS.path_shortcuts, shortcut))
                {
                    Log(LOG_LEVEL_WARNING,
                        "Already existing shortcut for path '%s' was replaced",
                        pp->promiser);
                }
                else
                {
                    StringMapInsert(SERVER_ACCESS.path_shortcuts,
                                    xstrdup(shortcut), xstrdup(pp->promiser));

                    Log(LOG_LEVEL_DEBUG, "Added shortcut '%s' for path: %s",
                        shortcut, pp->promiser);
                }
            }
            break;

        case RVAL_TYPE_LIST:

            for (const Rlist *rp = (const Rlist *) cp->rval.item;
                 rp != NULL; rp = rp->next)
            {
                /* TODO keys, ips, hostnames are valid such strings. */

                if (IsAccessBody(REMOTE_ACCESS_ADMITIPS))
                {
                    ret = StrList_Append(&racl->admit.ips, RlistScalarValue(rp));
                    if (ap != NULL)
                    {
                        PrependItem(&(ap->accesslist), RlistScalarValue(rp), NULL);
                    }
                }
                else if (IsAccessBody(REMOTE_ACCESS_DENYIPS))
                {
                    ret = StrList_Append(&racl->deny.ips, RlistScalarValue(rp));
                    if (dp != NULL)
                    {
                        PrependItem(&(dp->accesslist), RlistScalarValue(rp), NULL);
                    }
                }
                else if (IsAccessBody(REMOTE_ACCESS_ADMITHOSTNAMES))
                {
                    ret = StrList_Append(&racl->admit.hostnames, RlistScalarValue(rp));
                    /* If any hostname rule got added,
                     * turn on reverse DNS lookup in the new protocol. */
                    if (ret != (size_t) -1)
                    {
                        TurnOnReverseLookups();
                    }
                    if (ap != NULL)
                    {
                        NewHostToOldACL(ap, RlistScalarValue(rp));
                    }
                }
                else if (IsAccessBody(REMOTE_ACCESS_DENYHOSTNAMES))
                {
                    ret = StrList_Append(&racl->deny.hostnames, RlistScalarValue(rp));
                    /* If any hostname rule got added,
                     * turn on reverse DNS lookup in the new protocol. */
                    if (ret != (size_t) -1)
                    {
                        TurnOnReverseLookups();
                    }
                    if (dp != NULL)
                    {
                        NewHostToOldACL(dp, RlistScalarValue(rp));
                    }
                }
                else if (IsAccessBody(REMOTE_ACCESS_ADMITKEYS))
                {
                    ret = StrList_Append(&racl->admit.keys, RlistScalarValue(rp));
                }
                else if (IsAccessBody(REMOTE_ACCESS_DENYKEYS))
                {
                    ret = StrList_Append(&racl->deny.keys, RlistScalarValue(rp));
                }
                /* Legacy stuff */
                else if (IsAccessBody(REMOTE_ACCESS_ADMIT))
                {
                    ret = racl_SmartAppend(&racl->admit, RlistScalarValue(rp));
                    if (ap != NULL)
                    {
                        PrependItem(&(ap->accesslist), RlistScalarValue(rp), NULL);
                    }
                }
                else if (IsAccessBody(REMOTE_ACCESS_DENY))
                {
                    ret = racl_SmartAppend(&racl->deny, RlistScalarValue(rp));
                    if (dp != NULL)
                    {
                        PrependItem(&(dp->accesslist), RlistScalarValue(rp), NULL);
                    }
                }
                else if (ap != NULL && IsAccessBody(REMOTE_ACCESS_MAPROOT))
                {
                    PrependItem(&(ap->maproot), RlistScalarValue(rp), NULL);
                }
            }

            if (ret == (size_t) -1)
            {
                /* Should never happen, besides when allocation fails. */
                Log(LOG_LEVEL_CRIT, "StrList_Append: %s", GetErrorStr());
                DoCleanupAndExit(255);
            }

            break;

        default:
            UnexpectedError("Unknown constraint type!");
            break;

#undef IsAccessBody
        }
    }

    StrList_Finalise(&racl->admit.ips);
    StrList_Sort(racl->admit.ips, string_Compare);

    StrList_Finalise(&racl->admit.hostnames);
    StrList_Sort(racl->admit.hostnames, string_CompareFromEnd);

    StrList_Finalise(&racl->admit.keys);
    StrList_Sort(racl->admit.keys, string_Compare);

    StrList_Finalise(&racl->deny.ips);
    StrList_Sort(racl->deny.ips, string_Compare);

    StrList_Finalise(&racl->deny.hostnames);
    StrList_Sort(racl->deny.hostnames, string_CompareFromEnd);

    StrList_Finalise(&racl->deny.keys);
    StrList_Sort(racl->deny.keys, string_Compare);
}

/* It is allowed to have duplicate handles (paths or class names or variables
 * etc) in bundle server access_rules in policy files, but the lists here
 * should have unique entries. This, we make sure here. */
static Auth *GetOrCreateAuth(const char *handle, Auth **authchain, Auth **authchain_tail)
{
    Auth *a = GetAuthPath(handle, *authchain);

    if (!a)
    {
        InstallServerAuthPath(handle, authchain, authchain_tail);
        a = GetAuthPath(handle, *authchain);
    }

    return a;
}

static void KeepFileAccessPromise(const EvalContext *ctx, const Promise *pp)
{
    char path[PATH_MAX];
    size_t path_len = strlen(pp->promiser);
    if (path_len > sizeof(path) - 1)
    {
        goto err_too_long;
    }
    memcpy(path, pp->promiser, path_len + 1);

    /* Resolve symlinks and canonicalise access_rules path. */
    size_t ret2 = PreprocessRequestPath(path, sizeof(path));

    if (ret2 == (size_t) -1)
    {
        if (errno != ENOENT)                        /* something went wrong */
        {
            Log(LOG_LEVEL_ERR,
                "Failed to canonicalize path '%s' in access_rules, ignoring!",
                pp->promiser);
            return;
        }
        else                      /* file does not exist, it doesn't matter */
        {
            Log(LOG_LEVEL_INFO,
                "Path does not exist, it's added as-is in access rules: %s",
                path);
            Log(LOG_LEVEL_INFO,
                "WARNING: this means that (not) having a trailing slash defines if it's (not) a directory!");
            /* Legacy: convert trailing "/." to "/" */
            if (path_len >= 2 &&
                path[path_len - 1] == '.' &&
                path[path_len - 2] == '/')
            {
                path[path_len - 1] = '\0';
                path_len--;
            }
        }
    }
    else                                 /* file exists, path canonicalised */
    {
        /* If it's a directory append trailing '/' */
        path_len = ret2;
        int is_dir = IsDirReal(path);
        if (is_dir == 1 && path[path_len - 1] != FILE_SEPARATOR)
        {
            if (path_len + 2 > sizeof(path))
            {
                goto err_too_long;
            }
            PathAppendTrailingSlash(path, path_len);
            path_len++;
        }
    }

    size_t pos = acl_SortedInsert(&paths_acl, path);
    if (pos == (size_t) -1)
    {
        /* Should never happen, besides when allocation fails. */
        Log(LOG_LEVEL_CRIT, "acl_Insert: %s", GetErrorStr());
        DoCleanupAndExit(255);
    }

    /* Legacy code */
    if (path_len != 1)
    {
        DeleteSlash(path);
    }
    Auth *ap = GetOrCreateAuth(path, &SERVER_ACCESS.admit, &SERVER_ACCESS.admittail);
    Auth *dp = GetOrCreateAuth(path, &SERVER_ACCESS.deny, &SERVER_ACCESS.denytail);

    AccessPromise_AddAccessConstraints(ctx, pp, &paths_acl->acls[pos],
                                       ap, dp);
    return;

  err_too_long:
    Log(LOG_LEVEL_ERR,
        "Path '%s' in access_rules is too long (%zu > %d), ignoring!",
        pp->promiser, strlen(pp->promiser), PATH_MAX);
    return;
}

/*********************************************************************/

void KeepLiteralAccessPromise(EvalContext *ctx, const Promise *pp, const char *type)
{
    Auth *ap, *dp;
    const char *handle = PromiseGetHandle(pp);

    if (handle == NULL && strcmp(type, "literal") == 0)
    {
        Log(LOG_LEVEL_ERR, "Access to literal server data requires you to define a promise handle for reference");
        return;
    }

    if (strcmp(type, "literal") == 0)
    {
        Log(LOG_LEVEL_VERBOSE,"Looking at literal access promise '%s', type '%s'", pp->promiser, type);

        ap = GetOrCreateAuth(handle, &SERVER_ACCESS.varadmit, &SERVER_ACCESS.varadmittail);
        dp = GetOrCreateAuth(handle, &SERVER_ACCESS.vardeny, &SERVER_ACCESS.vardenytail);

        RegisterLiteralServerData(ctx, handle, pp);
        ap->literal = true;


        size_t pos = acl_SortedInsert(&literals_acl, handle);
        if (pos == (size_t) -1)
        {
            /* Should never happen, besides when allocation fails. */
            Log(LOG_LEVEL_CRIT, "acl_Insert: %s", GetErrorStr());
            DoCleanupAndExit(255);
        }

        AccessPromise_AddAccessConstraints(ctx, pp, &literals_acl->acls[pos],
                                           ap, dp);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE,"Looking at context/var access promise '%s', type '%s'", pp->promiser, type);

        ap = GetOrCreateAuth(pp->promiser, &SERVER_ACCESS.varadmit, &SERVER_ACCESS.varadmittail);
        dp = GetOrCreateAuth(pp->promiser, &SERVER_ACCESS.vardeny, &SERVER_ACCESS.vardenytail);

        if (strcmp(type, "context") == 0)
        {
            ap->classpattern = true;

            size_t pos = acl_SortedInsert(&classes_acl, pp->promiser);
            if (pos == (size_t) -1)
            {
                /* Should never happen, besides when allocation fails. */
                Log(LOG_LEVEL_CRIT, "acl_Insert: %s", GetErrorStr());
                DoCleanupAndExit(255);
            }

            AccessPromise_AddAccessConstraints(ctx, pp, &classes_acl->acls[pos],
                                               ap, dp);
        }
        else if (strcmp(type, "variable") == 0)
        {
            ap->variable = true;

            size_t pos = acl_SortedInsert(&vars_acl, pp->promiser);
            if (pos == (size_t) -1)
            {
                /* Should never happen, besides when allocation fails. */
                Log(LOG_LEVEL_CRIT, "acl_Insert: %s", GetErrorStr());
                DoCleanupAndExit(255);
            }

            AccessPromise_AddAccessConstraints(ctx, pp, &vars_acl->acls[pos],
                                               ap, dp);
        }
    }
}

/*********************************************************************/

static void KeepQueryAccessPromise(EvalContext *ctx, const Promise *pp)
{
    Auth *dp = GetOrCreateAuth(pp->promiser, &SERVER_ACCESS.vardeny, &SERVER_ACCESS.vardenytail),
        *ap = GetOrCreateAuth(pp->promiser, &SERVER_ACCESS.varadmit, &SERVER_ACCESS.varadmittail);

    RegisterLiteralServerData(ctx, pp->promiser, pp);
    ap->literal = true;

    size_t pos = acl_SortedInsert(&query_acl, pp->promiser);
    if (pos == (size_t) -1)
    {
        /* Should never happen, besides when allocation fails. */
        Log(LOG_LEVEL_CRIT, "acl_Insert: %s", GetErrorStr());
        DoCleanupAndExit(255);
    }

    AccessPromise_AddAccessConstraints(ctx, pp, &query_acl->acls[pos],
                                       ap, dp);
}

static void KeepBundlesAccessPromise(EvalContext *ctx, const Promise *pp)
{
    size_t pos = acl_SortedInsert(&bundles_acl, pp->promiser);
    if (pos == (size_t) -1)
    {
        /* Should never happen, besides when allocation fails. */
        Log(LOG_LEVEL_CRIT, "acl_Insert: %s", GetErrorStr());
        DoCleanupAndExit(255);
    }

    /* Last params are NULL because we don't have
     * old-school Auth type ACLs here. */
    AccessPromise_AddAccessConstraints(ctx, pp, &bundles_acl->acls[pos],
                                       NULL, NULL);
}
/*********************************************************************/

/**
 * The "roles" access promise is for remote class activation by means of
 * cf-runagent -D:
 *
 *     pp->promiser is a regex to match classes.
 *     pp->conlist  is an slist of usernames.
 */
static void KeepServerRolePromise(EvalContext *ctx, const Promise *pp)
{
    size_t pos = acl_SortedInsert(&roles_acl, pp->promiser);
    if (pos == (size_t) -1)
    {
        /* Should never happen, besides when allocation fails. */
        Log(LOG_LEVEL_CRIT, "acl_Insert: %s", GetErrorStr());
        DoCleanupAndExit(255);
    }

    size_t i = SeqLength(pp->conlist);
    while (i > 0)
    {
        i--;
        Constraint *cp = SeqAt(pp->conlist, i);
        char const * const authorizer =
            CF_REMROLE_BODIES[REMOTE_ROLE_AUTHORIZE].lval;

        if (strcmp(cp->lval, authorizer) == 0)
        {
            if (cp->rval.type != RVAL_TYPE_LIST)
            {
                Log(LOG_LEVEL_ERR,
                    "Right-hand side of authorize promise for '%s' should be a list",
                    pp->promiser);
            }
            else if (IsDefinedClass(ctx, cp->classes))
            {
                for (const Rlist *rp = cp->rval.item; rp != NULL; rp = rp->next)
                {
                    /* The "roles" access promise currently only supports
                     * listing usernames to admit access to, nothing more. */
                    struct resource_acl *racl = &roles_acl->acls[pos];
                    size_t zret = StrList_Append(&racl->admit.usernames,
                                                 RlistScalarValue(rp));
                    if (zret == (size_t) -1)
                    {
                        /* Should never happen, besides when allocation fails. */
                        Log(LOG_LEVEL_CRIT, "StrList_Append: %s", GetErrorStr());
                        DoCleanupAndExit(255);
                    }
                }
            }
        }
        else if (strcmp(cp->lval, "comment") != 0 &&
                 strcmp(cp->lval, "handle") != 0 &&
                 /* Are there other known list constraints ? if not, skip this: */
                 cp->rval.type != RVAL_TYPE_LIST)
        {
            Log(LOG_LEVEL_WARNING,
                "Unrecognised promise '%s' for %s",
                cp->lval, pp->promiser);
        }
    }
}

static void InstallServerAuthPath(const char *path, Auth **list, Auth **listtail)
{
    Auth **nextp = *listtail ? &((*listtail)->next) : list;
    assert(*nextp == NULL);
    *listtail = *nextp = xcalloc(1, sizeof(Auth));
    (*nextp)->path = xstrdup(path);

#ifdef __MINGW32__
    for (char *p = (*nextp)->path; *p != '\0'; p++)
    {
        *p = ToLower(*p);
    }
#endif /* __MINGW32__ */
}

static Auth *GetAuthPath(const char *path, Auth *list)
{
    size_t path_len = strlen(path);
    char unslashed_path[path_len + 1];
    memcpy(unslashed_path, path, path_len + 1);

#ifdef __MINGW32__
    ToLowerStrInplace(unslashed_path);
#endif

    if (path_len != 1)
    {
        DeleteSlash(unslashed_path);
    }

    for (Auth *ap = list; ap != NULL; ap = ap->next)
    {
        if (strcmp(ap->path, unslashed_path) == 0)
        {
            return ap;
        }
    }

    return NULL;
}
