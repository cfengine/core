/*
   Copyright (C) CFEngine AS

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
#include <generic_agent.h> /* HashControls */
#include <file_lib.h>      /* IsDirReal */

#include "server_common.h"                         /* PreprocessRequestPath */
#include "server_access.h"
#include "strlist.h"


static void KeepContextBundles(EvalContext *ctx, const Policy *policy);
static PromiseResult KeepServerPromise(EvalContext *ctx, const Promise *pp, void *param);
static void InstallServerAuthPath(const char *path, Auth **list, Auth **listtail);
static void KeepServerRolePromise(EvalContext *ctx, const Promise *pp);
static void KeepPromiseBundles(EvalContext *ctx, const Policy *policy);
static void KeepControlPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config);
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
extern Item *CONNECTIONLIST;

/*******************************************************************/

static void KeepFileAccessPromise(const EvalContext *ctx, const Promise *pp);
static void KeepLiteralAccessPromise(EvalContext *ctx, const Promise *pp, char *type);
static void KeepQueryAccessPromise(EvalContext *ctx, const Promise *pp, char *type);

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/


void Summarize()
{
    Log(LOG_LEVEL_VERBOSE, " === BEGIN summary of access promises === ");

    size_t i, j;
    for (i = 0; i < paths_acl->len; i++)
    {
        Log(LOG_LEVEL_VERBOSE, "\tPath: %s",
            StrList_At(paths_acl->resource_names, i));
        const struct resource_acl *racl = &paths_acl->acls[i];

        for (j = 0; j < StrList_Len(racl->admit_ips); j++)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tadmit_ips: %s",
                StrList_At(racl->admit_ips, j));
        }
        for (j = 0; j < StrList_Len(racl->admit_hostnames); j++)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tadmit_hostnames: %s",
                StrList_At(racl->admit_hostnames, j));
        }
        for (j = 0; j < StrList_Len(racl->admit_keys); j++)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tadmit_keys: %s",
                StrList_At(racl->admit_keys, j));
        }
        for (j = 0; j < StrList_Len(racl->deny_ips); j++)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tdeny_ips: %s",
                StrList_At(racl->deny_ips, j));
        }
        for (j = 0; j < StrList_Len(racl->deny_hostnames); j++)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tdeny_hostnames: %s",
                StrList_At(racl->deny_hostnames, j));
        }
        for (j = 0; j < StrList_Len(racl->deny_keys); j++)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tdeny_keys: %s",
                StrList_At(racl->deny_keys, j));
        }
    }

    Auth *ptr;
    Item *ip, *ipr;

    Log(LOG_LEVEL_VERBOSE, "Granted access to paths for classic protocol:");

    for (ptr = SV.admit; ptr != NULL; ptr = ptr->next)
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
            Log(LOG_LEVEL_VERBOSE, "\t\tadmit hosts: %s", ip->name);
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Denied access to paths for classic protocol:");

    for (ptr = SV.deny; ptr != NULL; ptr = ptr->next)
    {
        /* Don't report empty entries. */
        if (ptr->accesslist != NULL)
        {
            Log(LOG_LEVEL_VERBOSE, "\tPath: %s", ptr->path);
        }

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            Log(LOG_LEVEL_VERBOSE, "\t\tdeny hosts: %s", ip->name);
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Granted access to literal/variable/query data :");

    for (ptr = SV.varadmit; ptr != NULL; ptr = ptr->next)
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

    Log(LOG_LEVEL_VERBOSE, "Denied access to literal/variable/query data:");

    for (ptr = SV.vardeny; ptr != NULL; ptr = ptr->next)
    {
        Log(LOG_LEVEL_VERBOSE, "Object %s", ptr->path);

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            Log(LOG_LEVEL_VERBOSE, "Deny '%s'", ip->name);
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Host IPs allowed connection access:");

    for (ip = SV.nonattackerlist; ip != NULL; ip = ip->next)
    {
        Log(LOG_LEVEL_VERBOSE, "IP '%s'", ip->name);
    }

    Log(LOG_LEVEL_VERBOSE, "Host IPs denied connection access:");

    for (ip = SV.attackerlist; ip != NULL; ip = ip->next)
    {
        Log(LOG_LEVEL_VERBOSE, "IP '%s'", ip->name);
    }

    Log(LOG_LEVEL_VERBOSE, "Host IPs allowed multiple connection access:");

    for (ip = SV.multiconnlist; ip != NULL; ip = ip->next)
    {
        Log(LOG_LEVEL_VERBOSE, "IP '%s'", ip->name);
    }

    Log(LOG_LEVEL_VERBOSE, "Host IPs whose keys we shall establish trust to:");

    for (ip = SV.trustkeylist; ip != NULL; ip = ip->next)
    {
        Log(LOG_LEVEL_VERBOSE, "IP '%s'", ip->name);
    }

    Log(LOG_LEVEL_VERBOSE, "Users from whom we accept connections:");

    for (ip = SV.allowuserlist; ip != NULL; ip = ip->next)
    {
        Log(LOG_LEVEL_VERBOSE, "USERS '%s'", ip->name);
    }

    Log(LOG_LEVEL_VERBOSE, " === END summary of access promises === ");
}

void KeepPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config)
{
    if (paths_acl != NULL || classes_acl != NULL || vars_acl != NULL ||
        literals_acl != NULL || query_acl != NULL || SV.path_shortcuts != NULL)
    {
        UnexpectedError("ACLs are not NULL - we are probably leaking memory!");
    }

    paths_acl     = calloc(1, sizeof(*paths_acl));
    classes_acl   = calloc(1, sizeof(*classes_acl));
    vars_acl      = calloc(1, sizeof(*vars_acl));
    literals_acl  = calloc(1, sizeof(*literals_acl));
    query_acl     = calloc(1, sizeof(*query_acl));
    SV.path_shortcuts = StringMapNew();

    if (paths_acl == NULL || classes_acl == NULL || vars_acl == NULL ||
        literals_acl == NULL || query_acl == NULL ||
        SV.path_shortcuts == NULL)
    {
        Log(LOG_LEVEL_CRIT, "calloc: %s", GetErrorStr());
        exit(255);
    }

    KeepContextBundles(ctx, policy);
    KeepControlPromises(ctx, policy, config);
    KeepPromiseBundles(ctx, policy);
}

/*******************************************************************/

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

            if (!value)
            {
                Log(LOG_LEVEL_ERR, "Unknown lval '%s' in server control body", cp->lval);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_SERVER_FACILITY].lval) == 0)
            {
                SetFacility(value);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_DENY_BAD_CLOCKS].lval) == 0)
            {
                DENYBADCLOCKS = BooleanFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting denybadclocks to '%s'", DENYBADCLOCKS ? "true" : "false");
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_LOG_ENCRYPTED_TRANSFERS].lval) == 0)
            {
                LOGENCRYPT = BooleanFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting logencrypt to '%s'", LOGENCRYPT ? "true" : "false");
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_LOG_ALL_CONNECTIONS].lval) == 0)
            {
                SV.logconns = BooleanFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting logconns to %d", SV.logconns);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_MAX_CONNECTIONS].lval) == 0)
            {
                CFD_MAXPROCESSES = (int) IntFromString(value);
                MAXTRIES = CFD_MAXPROCESSES / 3;
                Log(LOG_LEVEL_VERBOSE, "Setting maxconnections to %d", CFD_MAXPROCESSES);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_CALL_COLLECT_INTERVAL].lval) == 0)
            {
                COLLECT_INTERVAL = (int) 60 * IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting call_collect_interval to %d (seconds)", COLLECT_INTERVAL);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_LISTEN].lval) == 0)
            {
                SERVER_LISTEN = BooleanFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting server listen to '%s' ",
                      (SERVER_LISTEN)? "true":"false");
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_CALL_COLLECT_WINDOW].lval) == 0)
            {
                COLLECT_WINDOW = (int) IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting collect_window to %d (seconds)", COLLECT_INTERVAL);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_CF_RUN_COMMAND].lval) == 0)
            {
                strlcpy(CFRUNCOMMAND, value, sizeof(CFRUNCOMMAND));
                Log(LOG_LEVEL_VERBOSE, "Setting cfruncommand to '%s'", CFRUNCOMMAND);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_ALLOW_CONNECTS].lval) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Setting allowing connections from ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SV.nonattackerlist, RlistScalarValue(rp)))
                    {
                        AppendItem(&SV.nonattackerlist, RlistScalarValue(rp), cp->classes);
                    }
                }

                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_DENY_CONNECTS].lval) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Setting denying connections from ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SV.attackerlist, RlistScalarValue(rp)))
                    {
                        AppendItem(&SV.attackerlist, RlistScalarValue(rp), cp->classes);
                    }
                }

                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_SKIP_VERIFY].lval) == 0)
            {
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_ALLOW_ALL_CONNECTS].lval) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Setting allowing multiple connections from ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SV.multiconnlist, RlistScalarValue(rp)))
                    {
                        AppendItem(&SV.multiconnlist, RlistScalarValue(rp), cp->classes);
                    }
                }

                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_ALLOW_USERS].lval) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "SET Allowing users ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SV.allowuserlist, RlistScalarValue(rp)))
                    {
                        AppendItem(&SV.allowuserlist, RlistScalarValue(rp), cp->classes);
                    }
                }

                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_TRUST_KEYS_FROM].lval) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Setting trust keys from ...");

                for (const Rlist *rp = value; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SV.trustkeylist, RlistScalarValue(rp)))
                    {
                        AppendItem(&SV.trustkeylist, RlistScalarValue(rp), cp->classes);
                    }
                }

                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_PORT_NUMBER].lval) == 0)
            {
                CFENGINE_PORT = (short) IntFromString(value);
                Log(LOG_LEVEL_VERBOSE, "Setting default port number to %d", CFENGINE_PORT);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_BIND_TO_INTERFACE].lval) == 0)
            {
                strncpy(BINDINTERFACE, value, CF_BUFSIZE - 1);
                Log(LOG_LEVEL_VERBOSE, "Setting bindtointerface to '%s'", BINDINTERFACE);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_ALLOWCIPHERS].lval) == 0)
            {

                SV.allowciphers = xstrdup(value);
                Log(LOG_LEVEL_VERBOSE, "Setting allowciphers to '%s'", SV.allowciphers);
                continue;
            }
        }
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
}

/*********************************************************************/

static void KeepContextBundles(EvalContext *ctx, const Policy *policy)
{
/* Dial up the generic promise expansion with a callback */

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);

        if ((strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_SERVER]) == 0) || (strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_COMMON]) == 0))
        {
            if (RlistLen(bp->args) > 0)
            {
                Log(LOG_LEVEL_WARNING, "Cannot implicitly evaluate bundle '%s %s', as this bundle takes arguments.", bp->type, bp->name);
                continue;
            }

            BannerBundle(bp, NULL);

            for (size_t j = 0; j < SeqLength(bp->promise_types); j++)
            {
                PromiseType *sp = SeqAt(bp->promise_types, j);

                if ((strcmp(sp->name, "vars") != 0) && (strcmp(sp->name, "classes") != 0))
                {
                    continue;
                }

                BannerPromiseType(bp->name, sp->name, 0);

                EvalContextStackPushBundleFrame(ctx, bp, NULL, false);

                for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
                {
                    Promise *pp = SeqAt(sp->promises, ppi);
                    ExpandPromise(ctx, 0, pp, KeepServerPromise, NULL);
                }

                EvalContextStackPopFrame(ctx);
            }
        }
    }
}

/*********************************************************************/

static void KeepPromiseBundles(EvalContext *ctx, const Policy *policy)
{
/* Dial up the generic promise expansion with a callback */

    CleanReportBookFilterSet();

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);

        if ((strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_SERVER]) == 0) || (strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_COMMON]) == 0))
        {
            if (RlistLen(bp->args) > 0)
            {
                Log(LOG_LEVEL_WARNING, "Cannot implicitly evaluate bundle '%s %s', as this bundle takes arguments.", bp->type, bp->name);
                continue;
            }

            BannerBundle(bp, NULL);

            for (size_t j = 0; j < SeqLength(bp->promise_types); j++)
            {
                PromiseType *sp = SeqAt(bp->promise_types, j);

                if ((strcmp(sp->name, "access") != 0) && (strcmp(sp->name, "roles") != 0))
                {
                    continue;
                }

                BannerPromiseType(bp->name, sp->name, 0);

                EvalContextStackPushBundleFrame(ctx, bp, NULL, false);

                for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
                {
                    Promise *pp = SeqAt(sp->promises, ppi);
                    ExpandPromise(ctx, 0, pp, KeepServerPromise, NULL);
                }

                EvalContextStackPopFrame(ctx);
            }
        }
    }
}

static PromiseResult KeepServerPromise(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert(!param);

    if (!IsDefinedClass(ctx, pp->classes))
    {
        Log(LOG_LEVEL_VERBOSE, "Skipping whole promise, as context is %s", pp->classes);
        return PROMISE_RESULT_NOOP;
    }

    {
        char *cls = NULL;
        if (VarClassExcluded(ctx, pp, &cls))
        {
            if (LEGACY_OUTPUT)
            {
                Log(LOG_LEVEL_VERBOSE, "\n");
                Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
                Log(LOG_LEVEL_VERBOSE, "Skipping whole next promise (%s), as var-context %s is not relevant", pp->promiser, cls);
                Log(LOG_LEVEL_VERBOSE, ". . . . . . . . . . . . . . . . . . . . . . . . . . . . ");
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Skipping next promise '%s', as var-context '%s' is not relevant", pp->promiser, cls);
            }
            return PROMISE_RESULT_NOOP;
        }
    }

    if (strcmp(pp->parent_promise_type->name, "classes") == 0)
    {
        return VerifyClassPromise(ctx, pp, NULL);
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
            KeepQueryAccessPromise(ctx, pp, "query");
            KeepReportDataSelectAccessPromise(pp);
            return PROMISE_RESULT_NOOP;
        }
        else if (strcmp(resource_type, "context") == 0)
        {
            KeepLiteralAccessPromise(ctx, pp, "context");
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
    /* IPv4 or IPv6 subnet mask */
    else if (s[strspn(s, "0123456789abcdef.:/")] == '\0')
    {
        return ADMIT_TYPE_IP;
    }
    else
    {
        return ADMIT_TYPE_HOSTNAME;
    }
}

/**
 * Add access rules to the given ACL #acl according to the constraints in the
 * particular access promise.
 *
 * For legacy reasons (non-TLS connections), build also the #ap (access Auth)
 * and #dp (deny Auth).
 */
static void AccessPromise_AddAccessConstraints(const EvalContext *ctx,
                                               const Promise *pp,
                                               struct resource_acl *acl,
                                               Auth *ap, Auth *dp)
{
    Rlist *rp;

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
        case RVAL_TYPE_SCALAR:

            if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_IFENCRYPTED].lval) == 0)
            {
                ap->encrypt = true;
                continue;
            }
            if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_SHORTCUT].lval) == 0)
            {
                const char *shortcut = cp->rval.item;

                if (strchr(shortcut, FILE_SEPARATOR) != NULL)
                {
                    Log(LOG_LEVEL_ERR,
                        "slashes are forbidden in ACL shortcut: %s",
                        shortcut);
                    continue;
                }

                bool ret = StringMapHasKey(SV.path_shortcuts, shortcut);
                if (ret)
                {
                    Log(LOG_LEVEL_WARNING,
                        "Already existing shortcut for path '%s' was replaced",
                        pp->promiser);
                    continue;
                }

                StringMapInsert(SV.path_shortcuts,
                                xstrdup(shortcut), xstrdup(pp->promiser));

                Log(LOG_LEVEL_DEBUG, "Added shortcut '%s' for path: %s",
                    shortcut, pp->promiser);
                continue;
            }

            break;

        case RVAL_TYPE_LIST:

            for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
            {

                /* TODO keys, ips, hostnames are valid such strings. */

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_ADMITIPS].lval) == 0)
                {
                    ret = StrList_Append(&acl->admit_ips, RlistScalarValue(rp));
                    continue;
                }
                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_DENYIPS].lval) == 0)
                {
                    ret = StrList_Append(&acl->deny_ips, RlistScalarValue(rp));
                    continue;
                }
                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_ADMITHOSTNAMES].lval) == 0)
                {
                    ret = StrList_Append(&acl->admit_hostnames, RlistScalarValue(rp));
                    continue;
                }
                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_DENYHOSTNAMES].lval) == 0)
                {
                    ret = StrList_Append(&acl->deny_hostnames, RlistScalarValue(rp));
                    continue;
                }
                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_ADMITKEYS].lval) == 0)
                {
                    ret = StrList_Append(&acl->admit_keys, RlistScalarValue(rp));
                    continue;
                }
                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_DENYKEYS].lval) == 0)
                {
                    ret = StrList_Append(&acl->deny_keys, RlistScalarValue(rp));
                    continue;
                }

                /* Legacy stuff */

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_ADMIT].lval) == 0)
                {
                    switch (AdmitType(RlistScalarValue(rp)))
                    {
                    case ADMIT_TYPE_IP:
                        /* TODO convert IP string to binary representation. */
                        ret = StrList_Append(&acl->admit_ips, RlistScalarValue(rp));
                        break;
                    case ADMIT_TYPE_KEY:
                        ret = StrList_Append(&acl->admit_keys, RlistScalarValue(rp));
                        break;
                    case ADMIT_TYPE_HOSTNAME:
                        /* TODO clean up possible regex, if it starts with ".*"
                         * then store two entries: itself, and *dot*itself. */
                        ret = StrList_Append(&acl->admit_hostnames, RlistScalarValue(rp));
                        break;
                    default:
                        Log(LOG_LEVEL_WARNING,
                            "Access rule 'admit: %s' is not IP, hostname or key, ignoring",
                            RlistScalarValue(rp));
                    }

                    PrependItem(&(ap->accesslist), RlistScalarValue(rp), NULL);
                    continue;
                }
                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_DENY].lval) == 0)
                {
                    switch (AdmitType(RlistScalarValue(rp)))
                    {
                    case ADMIT_TYPE_IP:
                        ret = StrList_Append(&acl->deny_ips, RlistScalarValue(rp));
                        break;
                    case ADMIT_TYPE_KEY:
                        ret = StrList_Append(&acl->deny_keys, RlistScalarValue(rp));
                        break;
                    case ADMIT_TYPE_HOSTNAME:
                        ret = StrList_Append(&acl->deny_hostnames, RlistScalarValue(rp));
                        break;
                    default:
                        Log(LOG_LEVEL_WARNING,
                            "Access rule 'deny: %s' is not IP, hostname or key, ignoring",
                            RlistScalarValue(rp));
                    }

                    PrependItem(&(dp->accesslist), RlistScalarValue(rp), NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_MAPROOT].lval) == 0)
                {
                    PrependItem(&(ap->maproot), RlistScalarValue(rp), NULL);
                    continue;
                }
            }

            if (ret == (size_t) -1)
            {
                /* Should never happen, besides when allocation fails. */
                Log(LOG_LEVEL_CRIT, "StrList_Append: %s", GetErrorStr());
                exit(255);
            }

            break;

        default:
            UnexpectedError("Unknown constraint type!");
            break;
        }
    }

    StrList_Finalise(&acl->admit_ips);
    StrList_Sort(acl->admit_ips, string_Compare);

    StrList_Finalise(&acl->admit_hostnames);
    StrList_Sort(acl->admit_hostnames, string_CompareFromEnd);

    StrList_Finalise(&acl->admit_keys);
    StrList_Sort(acl->admit_keys, string_Compare);

    StrList_Finalise(&acl->deny_ips);
    StrList_Sort(acl->deny_ips, string_Compare);

    StrList_Finalise(&acl->deny_hostnames);
    StrList_Sort(acl->deny_hostnames, string_CompareFromEnd);

    StrList_Finalise(&acl->deny_keys);
    StrList_Sort(acl->deny_keys, string_Compare);
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
    int ret = PreprocessRequestPath(path, sizeof(path));

    if (ret == -1)
    {
        if (errno != ENOENT)                        /* something went wrong */
        {
            goto err_too_long;
        }
        else                      /* file does not exist, it doesn't matter */
        {
            Log(LOG_LEVEL_INFO,
                "Path does not exist, it's added as-is in access rules: %s",
                path);
            Log(LOG_LEVEL_INFO,
                "WARNING: that means that (not) having a trailing slash defines if it's a directory!");
        }
    }
    else                                 /* file exists, path canonicalised */
    {
        /* If it's a directory append trailing '/'. */
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
        exit(255);
    }

    /* Legacy code */
    if (path_len != 1)
    {
        DeleteSlash(path);
    }
    Auth *ap = GetOrCreateAuth(path, &SV.admit, &SV.admittail);
    Auth *dp = GetOrCreateAuth(path, &SV.deny, &SV.denytail);

    AccessPromise_AddAccessConstraints(ctx, pp, &paths_acl->acls[pos],
                                       ap, dp);
    return;

  err_too_long:
        Log(LOG_LEVEL_ERR, "Path '%s' in access_rules is too long, ignoring!",
            pp->promiser);
}

/*********************************************************************/

void KeepLiteralAccessPromise(EvalContext *ctx, const Promise *pp, char *type)
{
    Auth *ap, *dp;
    const char *handle = PromiseGetHandle(pp);

    if ((handle == NULL) && (strcmp(type,"literal") == 0))
    {
        Log(LOG_LEVEL_ERR, "Access to literal server data requires you to define a promise handle for reference");
        return;
    }

    if (strcmp(type, "literal") == 0)
    {
        Log(LOG_LEVEL_VERBOSE,"Looking at literal access promise '%s', type '%s'", pp->promiser, type);

        ap = GetOrCreateAuth(handle, &SV.varadmit, &SV.varadmittail);
        dp = GetOrCreateAuth(handle, &SV.vardeny, &SV.vardenytail);

        RegisterLiteralServerData(ctx, handle, pp);
        ap->literal = true;


        size_t pos = acl_SortedInsert(&literals_acl, handle);
        if (pos == (size_t) -1)
        {
            /* Should never happen, besides when allocation fails. */
            Log(LOG_LEVEL_CRIT, "acl_Insert: %s", GetErrorStr());
            exit(255);
        }

        AccessPromise_AddAccessConstraints(ctx, pp, &literals_acl->acls[pos],
                                           ap, dp);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE,"Looking at context/var access promise '%s', type '%s'", pp->promiser, type);

        ap = GetOrCreateAuth(pp->promiser, &SV.varadmit, &SV.varadmittail);
        dp = GetOrCreateAuth(pp->promiser, &SV.vardeny, &SV.vardenytail);

        if (strcmp(type, "context") == 0)
        {
            ap->classpattern = true;

            size_t pos = acl_SortedInsert(&classes_acl, pp->promiser);
            if (pos == (size_t) -1)
            {
                /* Should never happen, besides when allocation fails. */
                Log(LOG_LEVEL_CRIT, "acl_Insert: %s", GetErrorStr());
                exit(255);
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
                exit(255);
            }

            AccessPromise_AddAccessConstraints(ctx, pp, &vars_acl->acls[pos],
                                               ap, dp);
        }
    }
}

/*********************************************************************/

static void KeepQueryAccessPromise(EvalContext *ctx, const Promise *pp, char *type)
{
    Auth *ap, *dp;

    assert (strcmp(type, "query") == 0);

    ap = GetOrCreateAuth(pp->promiser, &SV.varadmit, &SV.varadmittail);
    dp = GetOrCreateAuth(pp->promiser, &SV.vardeny, &SV.vardenytail);

    RegisterLiteralServerData(ctx, pp->promiser, pp);

    ap->literal = true;

    size_t pos = acl_SortedInsert(&query_acl, pp->promiser);
    if (pos == (size_t) -1)
    {
        /* Should never happen, besides when allocation fails. */
        Log(LOG_LEVEL_CRIT, "acl_Insert: %s", GetErrorStr());
        exit(255);
    }

    AccessPromise_AddAccessConstraints(ctx, pp, &query_acl->acls[pos],
                                       ap, dp);
}

/*********************************************************************/


static void KeepServerRolePromise(EvalContext *ctx, const Promise *pp)
{
    Rlist *rp;
    Auth *ap;

    ap = GetOrCreateAuth(pp->promiser, &SV.roles, &SV.rolestail);

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (!IsDefinedClass(ctx, cp->classes))
        {
            continue;
        }

        switch (cp->rval.type)
        {
        case RVAL_TYPE_LIST:

            for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
            {
                /* This is for remote class activation by means of cf-runagent.*/
                if (strcmp(cp->lval, CF_REMROLE_BODIES[REMOTE_ROLE_AUTHORIZE].lval) == 0)
                {
                    PrependItem(&(ap->accesslist), RlistScalarValue(rp), NULL);
                    continue;
                }
            }
            break;

        case RVAL_TYPE_FNCALL:
            UnexpectedError("Constraint of type FNCALL is invalid in this context!");
            break;

        default:

            if ((strcmp(cp->lval, "comment") == 0) || (strcmp(cp->lval, "handle") == 0))
            {
            }
            else
            {
                Log(LOG_LEVEL_ERR, "Right-hand side of authorize promise for '%s' should be a list", pp->promiser);
            }
            break;
        }
    }
}

static void InstallServerAuthPath(const char *path, Auth **list, Auth **listtail)
{
    Auth *ptr;

    ptr = xcalloc(1, sizeof(Auth));

    if (*listtail == NULL)       /* Last element in the list */
    {
        assert(*list == NULL);
        *list = ptr;
    }
    else
    {
        (*listtail)->next = ptr;
    }

    char *path_dup = xstrdup(path);

#ifdef __MINGW32__
    int i;

    for (i = 0; path_dup[i] != '\0'; i++)
    {
        path_dup[i] = ToLower(path_dup[i]);
    }
#endif /* __MINGW32__ */

    ptr->path = path_dup;
    *listtail = ptr;
}

static Auth *GetAuthPath(const char *path, Auth *list)
{
    Auth *ap;

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

    for (ap = list; ap != NULL; ap = ap->next)
    {
        if (strcmp(ap->path, unslashed_path) == 0)
        {
            return ap;
        }
    }

    return NULL;
}
