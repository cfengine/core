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

#include <generic_agent.h> // HashControls


typedef enum
{
    REMOTE_ACCESS_ADMIT,
    REMOTE_ACCESS_DENY,
    REMOTE_ACCESS_MAPROOT,
    REMOTE_ACCESS_ENCRYPTED,
    REMOTE_ACCESS_NONE
} RemoteAccess;

typedef enum
{
    REMOTE_ROLE_AUTHORIZE,
    REMOTE_ROLE_NONE
} RemoteRole;

typedef enum
{
    SERVER_CONTROL_ALLOW_ALL_CONNECTS,
    SERVER_CONTROL_ALLOW_CONNECTS,
    SERVER_CONTROL_ALLOW_USERS,
    SERVER_CONTROL_AUDITING,
    SERVER_CONTROL_BIND_TO_INTERFACE,
    SERVER_CONTROL_CF_RUN_COMMAND,
    SERVER_CONTROL_CALL_COLLECT_INTERVAL,
    SERVER_CONTROL_CALL_COLLECT_WINDOW,
    SERVER_CONTROL_DENY_BAD_CLOCKS,
    SERVER_CONTROL_DENY_CONNECTS,
    SERVER_CONTROL_DYNAMIC_ADDRESSES,
    SERVER_CONTROL_HOSTNAME_KEYS,
    SERVER_CONTROL_KEY_TTL,
    SERVER_CONTROL_LOG_ALL_CONNECTIONS,
    SERVER_CONTROL_LOG_ENCRYPTED_TRANSFERS,
    SERVER_CONTROL_MAX_CONNECTIONS,
    SERVER_CONTROL_PORT_NUMBER,
    SERVER_CONTROL_SERVER_FACILITY,
    SERVER_CONTROL_SKIP_VERIFY,
    SERVER_CONTROL_TRUST_KEYS_FROM,
    SERVER_CONTROL_LISTEN,
    SERVER_CONTROL_ALLOWCIPHERS,
    SERVER_CONTROL_NONE
} ServerControl;

static void KeepContextBundles(EvalContext *ctx, const Policy *policy);
static PromiseResult KeepServerPromise(EvalContext *ctx, const Promise *pp, void *param);
static void InstallServerAuthPath(const char *path, Auth **list, Auth **listtop);
static void KeepServerRolePromise(EvalContext *ctx, const Promise *pp);
static void KeepPromiseBundles(EvalContext *ctx, const Policy *policy);
static void KeepControlPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config);
static Auth *GetAuthPath(const char *path, Auth *list);

extern const ConstraintSyntax CFS_CONTROLBODY[];
extern const ConstraintSyntax CF_REMROLE_BODIES[];
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

void KeepFileAccessPromise(EvalContext *ctx, const Promise *pp);
void KeepLiteralAccessPromise(EvalContext *ctx, const Promise *pp, char *type);
void KeepQueryAccessPromise(EvalContext *ctx, const Promise *pp, char *type);

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/


void KeepPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config)
{
    KeepContextBundles(ctx, policy);
    KeepControlPromises(ctx, policy, config);
    KeepPromiseBundles(ctx, policy);
}

/*******************************************************************/

void Summarize()
{
    Auth *ptr;
    Item *ip, *ipr;

    Log(LOG_LEVEL_VERBOSE, "Summarize control promises");

    Log(LOG_LEVEL_VERBOSE, "Granted access to paths :");

    for (ptr = SV.admit; ptr != NULL; ptr = ptr->next)
    {
        Log(LOG_LEVEL_VERBOSE, "Path '%s' (encrypt=%d)", ptr->path, ptr->encrypt);

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            Log(LOG_LEVEL_VERBOSE, "Admit: '%s' root=", ip->name);
            for (ipr = ptr->maproot; ipr != NULL; ipr = ipr->next)
            {
                Log(LOG_LEVEL_VERBOSE, "%s,", ipr->name);
            }
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Denied access to paths :");

    for (ptr = SV.deny; ptr != NULL; ptr = ptr->next)
    {
        Log(LOG_LEVEL_VERBOSE, "Path '%s'", ptr->path);

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            Log(LOG_LEVEL_VERBOSE, "Deny '%s'", ip->name);
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Granted access to literal/variable/query data :");

    for (ptr = SV.varadmit; ptr != NULL; ptr = ptr->next)
    {
        Log(LOG_LEVEL_VERBOSE, "Object: '%s' (encrypt=%d)", ptr->path, ptr->encrypt);

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            Log(LOG_LEVEL_VERBOSE, "Admit '%s' root=", ip->name);
            for (ipr = ptr->maproot; ipr != NULL; ipr = ipr->next)
            {
                Log(LOG_LEVEL_VERBOSE, "%s,", ipr->name);
            }
        }
    }

    Log(LOG_LEVEL_VERBOSE, "Denied access to literal/variable/query data:");

    for (ptr = SV.vardeny; ptr != NULL; ptr = ptr->next)
    {
        Log(LOG_LEVEL_VERBOSE, "Object '%s'", ptr->path);

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

    Log(LOG_LEVEL_VERBOSE, "Host IPs from whom we shall accept public keys on trust:");

    for (ip = SV.trustkeylist; ip != NULL; ip = ip->next)
    {
        Log(LOG_LEVEL_VERBOSE, "IP '%s'", ip->name);
    }

    Log(LOG_LEVEL_VERBOSE, "Users from whom we accept connections:");

    for (ip = SV.allowuserlist; ip != NULL; ip = ip->next)
    {
        Log(LOG_LEVEL_VERBOSE, "USERS '%s'", ip->name);
    }
}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

static void KeepControlPromises(EvalContext *ctx, const Policy *policy, GenericAgentConfig *config)
{
    Rval retval;

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

            if (!IsDefinedClass(ctx, cp->classes, NULL))
            {
                continue;
            }

            VarRef *ref = VarRefParseFromScope(cp->lval, "control_server");

            if (!EvalContextVariableGet(ctx, ref, &retval, NULL))
            {
                Log(LOG_LEVEL_ERR, "Unknown lval '%s' in server control body", cp->lval);
                VarRefDestroy(ref);
                continue;
            }

            VarRefDestroy(ref);

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_SERVER_FACILITY].lval) == 0)
            {
                SetFacility(retval.item);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_DENY_BAD_CLOCKS].lval) == 0)
            {
                DENYBADCLOCKS = BooleanFromString(retval.item);
                Log(LOG_LEVEL_VERBOSE, "Setting denybadclocks to '%s'", DENYBADCLOCKS ? "true" : "false");
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_LOG_ENCRYPTED_TRANSFERS].lval) == 0)
            {
                LOGENCRYPT = BooleanFromString(retval.item);
                Log(LOG_LEVEL_VERBOSE, "Setting logencrypt to '%s'", LOGENCRYPT ? "true" : "false");
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_LOG_ALL_CONNECTIONS].lval) == 0)
            {
                SV.logconns = BooleanFromString(retval.item);
                Log(LOG_LEVEL_VERBOSE, "Setting logconns to %d", SV.logconns);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_MAX_CONNECTIONS].lval) == 0)
            {
                CFD_MAXPROCESSES = (int) IntFromString(retval.item);
                MAXTRIES = CFD_MAXPROCESSES / 3;
                Log(LOG_LEVEL_VERBOSE, "Setting maxconnections to %d", CFD_MAXPROCESSES);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_CALL_COLLECT_INTERVAL].lval) == 0)
            {
                COLLECT_INTERVAL = (int) 60 * IntFromString(retval.item);
                Log(LOG_LEVEL_VERBOSE, "Setting call_collect_interval to %d (seconds)", COLLECT_INTERVAL);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_LISTEN].lval) == 0)
            {
                SERVER_LISTEN = BooleanFromString(retval.item);
                Log(LOG_LEVEL_VERBOSE, "Setting server listen to '%s' ",
                      (SERVER_LISTEN)? "true":"false");
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_CALL_COLLECT_WINDOW].lval) == 0)
            {
                COLLECT_WINDOW = (int) IntFromString(retval.item);
                Log(LOG_LEVEL_VERBOSE, "Setting collect_window to %d (seconds)", COLLECT_INTERVAL);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_CF_RUN_COMMAND].lval) == 0)
            {
                strncpy(CFRUNCOMMAND, retval.item, CF_BUFSIZE - 1);
                Log(LOG_LEVEL_VERBOSE, "Setting cfruncommand to '%s'", CFRUNCOMMAND);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_ALLOW_CONNECTS].lval) == 0)
            {
                Rlist *rp;

                Log(LOG_LEVEL_VERBOSE, "Setting allowing connections from ...");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
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
                Rlist *rp;

                Log(LOG_LEVEL_VERBOSE, "Setting denying connections from ...");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
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
                Rlist *rp;

                Log(LOG_LEVEL_VERBOSE, "Setting allowing multiple connections from ...");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
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
                Rlist *rp;

                Log(LOG_LEVEL_VERBOSE, "SET Allowing users ...");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
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
                Rlist *rp;

                Log(LOG_LEVEL_VERBOSE, "Setting trust keys from ...");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
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
                CFENGINE_PORT = (short) IntFromString(retval.item);
                Log(LOG_LEVEL_VERBOSE, "Setting default port number to %d", CFENGINE_PORT);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_BIND_TO_INTERFACE].lval) == 0)
            {
                strncpy(BINDINTERFACE, retval.item, CF_BUFSIZE - 1);
                Log(LOG_LEVEL_VERBOSE, "Setting bindtointerface to '%s'", BINDINTERFACE);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_ALLOWCIPHERS].lval) == 0)
            {

                SV.allowciphers = xstrdup(retval.item);
                Log(LOG_LEVEL_VERBOSE, "Setting allowciphers to '%s'", SV.allowciphers);
                continue;
            }
        }
    }

    if (EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_SYSLOG_HOST, &retval))
    {
        /* Don't resolve syslog_host now, better do it per log request. */
        if (!SetSyslogHost(retval.item))
        {
            Log(LOG_LEVEL_ERR, "Failed to set syslog_host, '%s' too long",
                  (char *) retval.item);
        }
        else
        {
            Log(LOG_LEVEL_VERBOSE, "Setting syslog_host to '%s'",
                  (char *) retval.item);
        }
    }

    if (EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_SYSLOG_PORT, &retval))
    {
        SetSyslogPort(IntFromString(retval.item));
    }

    if (EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_FIPS_MODE, &retval))
    {
        FIPS_MODE = BooleanFromString(retval.item);
        Log(LOG_LEVEL_VERBOSE, "Setting FIPS mode to to '%s'", FIPS_MODE ? "true" : "false");
    }

    if (EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_LASTSEEN_EXPIRE_AFTER, &retval))
    {
        LASTSEENEXPIREAFTER = IntFromString(retval.item) * 60;
    }

    if (EvalContextVariableControlCommonGet(ctx, COMMON_CONTROL_BWLIMIT, &retval))
    {
        double bval;
        if (DoubleFromString(retval.item, &bval))
        {
            bwlimit_kbytes = (uint32_t) ( bval / 1000.0);
            Log(LOG_LEVEL_VERBOSE, "Setting rate limit to %d kBytes/sec", bwlimit_kbytes);
        }
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
                    ExpandPromise(ctx, pp, KeepServerPromise, NULL);
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
                    ExpandPromise(ctx, pp, KeepServerPromise, NULL);
                }

                EvalContextStackPopFrame(ctx);
            }
        }
    }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static PromiseResult KeepServerPromise(EvalContext *ctx, const Promise *pp, ARG_UNUSED void *param)
{
    assert(!param);

    if (!IsDefinedClass(ctx, pp->classes, PromiseGetNamespace(pp)))
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

    const char *resource_type = PromiseGetConstraintAsRval(pp, "resource_type", RVAL_TYPE_SCALAR);
    if (resource_type && strcmp(pp->parent_promise_type->name, "access") == 0)
    {
        if (strcmp(resource_type, "literal") == 0)
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

    if (strcmp(pp->parent_promise_type->name, "access") == 0)
    {
        KeepFileAccessPromise(ctx, pp);
        return PROMISE_RESULT_NOOP;
    }
    else if (strcmp(pp->parent_promise_type->name, "roles") == 0)
    {
        KeepServerRolePromise(ctx, pp);
        return PROMISE_RESULT_NOOP;
    }

    return PROMISE_RESULT_NOOP;
}

/*********************************************************************/

void KeepFileAccessPromise(EvalContext *ctx, const Promise *pp)
{
    Rlist *rp;
    Auth *ap, *dp;

    if (strlen(pp->promiser) != 1)
    {
        DeleteSlash(pp->promiser);
    }

    if (!GetAuthPath(pp->promiser, SV.admit))
    {
        InstallServerAuthPath(pp->promiser, &SV.admit, &SV.admittop);
    }

    if (!GetAuthPath(pp->promiser, SV.deny))
    {
        InstallServerAuthPath(pp->promiser, &SV.deny, &SV.denytop);
    }

    ap = GetAuthPath(pp->promiser, SV.admit);
    dp = GetAuthPath(pp->promiser, SV.deny);

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (!IsDefinedClass(ctx, cp->classes, PromiseGetNamespace(pp)))
        {
            continue;
        }

        switch (cp->rval.type)
        {
        case RVAL_TYPE_SCALAR:

            if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_ENCRYPTED].lval) == 0)
            {
                ap->encrypt = true;
            }

            break;

        case RVAL_TYPE_LIST:

            for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
            {
                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_ADMIT].lval) == 0)
                {
                    PrependItem(&(ap->accesslist), RlistScalarValue(rp), NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_DENY].lval) == 0)
                {
                    PrependItem(&(dp->accesslist), RlistScalarValue(rp), NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_MAPROOT].lval) == 0)
                {
                    PrependItem(&(ap->maproot), RlistScalarValue(rp), NULL);
                    continue;
                }
            }
            break;

        default:
            UnexpectedError("Unknown constraint type!");
            break;
        }
    }
}

/*********************************************************************/

void KeepLiteralAccessPromise(EvalContext *ctx, const Promise *pp, char *type)
{
    Rlist *rp;
    Auth *ap = NULL, *dp = NULL;
    const char *handle = PromiseGetHandle(pp);

    if ((handle == NULL) && (strcmp(type,"literal") == 0))
    {
        Log(LOG_LEVEL_ERR, "Access to literal server data requires you to define a promise handle for reference");
        return;
    }
    
    if (strcmp(type, "literal") == 0)
    {
        Log(LOG_LEVEL_VERBOSE,"Looking at literal access promise '%s', type '%s'", pp->promiser, type);

        if (!GetAuthPath(handle, SV.varadmit))
        {
            InstallServerAuthPath(handle, &SV.varadmit, &SV.varadmittop);
        }

        if (!GetAuthPath(handle, SV.vardeny))
        {
            InstallServerAuthPath(handle, &SV.vardeny, &SV.vardenytop);
        }

        RegisterLiteralServerData(ctx, handle, pp);
        ap = GetAuthPath(handle, SV.varadmit);
        dp = GetAuthPath(handle, SV.vardeny);
        ap->literal = true;
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE,"Looking at context/var access promise '%s', type '%s'", pp->promiser, type);

        if (!GetAuthPath(pp->promiser, SV.varadmit))
        {
            InstallServerAuthPath(pp->promiser, &SV.varadmit, &SV.varadmittop);
        }

        if (!GetAuthPath(pp->promiser, SV.vardeny))
        {
            InstallServerAuthPath(pp->promiser, &SV.vardeny, &SV.vardenytop);
        }


        if (strcmp(type, "context") == 0)
        {
            ap = GetAuthPath(pp->promiser, SV.varadmit);
            dp = GetAuthPath(pp->promiser, SV.vardeny);
            ap->classpattern = true;
        }

        if (strcmp(type, "variable") == 0)
        {
            ap = GetAuthPath(pp->promiser, SV.varadmit); // Allow the promiser (preferred) as well as handle as variable name
            dp = GetAuthPath(pp->promiser, SV.vardeny);
            ap->variable = true;
        }
    }
    
    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (!IsDefinedClass(ctx, cp->classes, PromiseGetNamespace(pp)))
        {
            continue;
        }

        switch (cp->rval.type)
        {
        case RVAL_TYPE_SCALAR:

            if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_ENCRYPTED].lval) == 0)
            {
                ap->encrypt = true;
            }

            break;

        case RVAL_TYPE_LIST:

            for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
            {
                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_ADMIT].lval) == 0)
                {
                    PrependItem(&(ap->accesslist), RlistScalarValue(rp), NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_DENY].lval) == 0)
                {
                    PrependItem(&(dp->accesslist), RlistScalarValue(rp), NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_MAPROOT].lval) == 0)
                {
                    PrependItem(&(ap->maproot), RlistScalarValue(rp), NULL);
                    continue;
                }
            }
            break;

        default:
            UnexpectedError("Unknown constraint type!");
            break;
        }
    }
}

/*********************************************************************/

void KeepQueryAccessPromise(EvalContext *ctx, const Promise *pp, char *type)
{
    Rlist *rp;
    Auth *ap, *dp;

    if (!GetAuthPath(pp->promiser, SV.varadmit))
    {
        InstallServerAuthPath(pp->promiser, &SV.varadmit, &SV.varadmittop);
    }

    RegisterLiteralServerData(ctx, pp->promiser, pp);

    if (!GetAuthPath(pp->promiser, SV.vardeny))
    {
        InstallServerAuthPath(pp->promiser, &SV.vardeny, &SV.vardenytop);
    }

    ap = GetAuthPath(pp->promiser, SV.varadmit);
    dp = GetAuthPath(pp->promiser, SV.vardeny);

    if (strcmp(type, "query") == 0)
    {
        ap->literal = true;
    }

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (!IsDefinedClass(ctx, cp->classes, PromiseGetNamespace(pp)))
        {
            continue;
        }

        switch (cp->rval.type)
        {
        case RVAL_TYPE_SCALAR:

            if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_ENCRYPTED].lval) == 0)
            {
                ap->encrypt = true;
            }

            break;

        case RVAL_TYPE_LIST:

            for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
            {
                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_ADMIT].lval) == 0)
                {
                    PrependItem(&(ap->accesslist), RlistScalarValue(rp), NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_DENY].lval) == 0)
                {
                    PrependItem(&(dp->accesslist), RlistScalarValue(rp), NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_MAPROOT].lval) == 0)
                {
                    PrependItem(&(ap->maproot), RlistScalarValue(rp), NULL);
                    continue;
                }
            }
            break;

        default:
            UnexpectedError("Unknown constraint type!");
            break;
        }
    }
}

/*********************************************************************/

static void KeepServerRolePromise(EvalContext *ctx, const Promise *pp)
{
    Rlist *rp;
    Auth *ap;

    if (!GetAuthPath(pp->promiser, SV.roles))
    {
        InstallServerAuthPath(pp->promiser, &SV.roles, &SV.rolestop);
    }

    ap = GetAuthPath(pp->promiser, SV.roles);

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (!IsDefinedClass(ctx, cp->classes, PromiseGetNamespace(pp)))
        {
            continue;
        }

        switch (cp->rval.type)
        {
        case RVAL_TYPE_LIST:

            for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
            {
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

/***********************************************************************/
/* Level                                                               */
/***********************************************************************/

static void InstallServerAuthPath(const char *path, Auth **list, Auth **listtop)
{
    Auth *ptr;

    ptr = xcalloc(1, sizeof(Auth));

    if (*listtop == NULL)       /* First element in the list */
    {
        *list = ptr;
    }
    else
    {
        (*listtop)->next = ptr;
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
    *listtop = ptr;
}

/***********************************************************************/
/* Level                                                               */
/***********************************************************************/

static Auth *GetAuthPath(const char *path, Auth *list)
{
    Auth *ap;

    char *unslashed_path = xstrdup(path);

#ifdef __MINGW32__
    int i;

    for (i = 0; unslashed_path[i] != '\0'; i++)
    {
        unslashed_path[i] = ToLower(unslashed_path[i]);
    }
#endif /* __MINGW32__ */    

    if (strlen(unslashed_path) != 1)
    {
        DeleteSlash(unslashed_path);
    }

    for (ap = list; ap != NULL; ap = ap->next)
    {
        if (strcmp(ap->path, unslashed_path) == 0)
        {
            free(unslashed_path);
            return ap;
        }
    }

    free(unslashed_path);
    return NULL;
}

/***********************************************************************/
