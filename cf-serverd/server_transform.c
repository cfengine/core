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

#include "server_transform.h"

#include "server.h"

#include "env_context.h"
#include "files_names.h"
#include "mod_access.h"
#include "item_lib.h"
#include "conversion.h"
#include "reporting.h"
#include "expand.h"
#include "transaction.h"
#include "scope.h"
#include "vars.h"
#include "attributes.h"
#include "cfstream.h"
#include "communication.h"
#include "string_lib.h"
#include "rlist.h"

#include "generic_agent.h" // HashControls

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
    SERVER_CONTROL_NONE
} ServerControl;

static void KeepContextBundles(Policy *policy, const ReportContext *report_context);
static void KeepServerPromise(Promise *pp);
static void InstallServerAuthPath(char *path, Auth **list, Auth **listtop);
static void KeepServerRolePromise(Promise *pp);
static void KeepPromiseBundles(Policy *policy, const ReportContext *report_context);
static void KeepControlPromises(Policy *policy, GenericAgentConfig *config);

extern const BodySyntax CFS_CONTROLBODY[];
extern const BodySyntax CF_REMROLE_BODIES[];
extern int COLLECT_INTERVAL;
extern int COLLECT_WINDOW;
extern bool SERVER_LISTEN;

/*******************************************************************/
/* GLOBAL VARIABLES                                                */
/*******************************************************************/

extern int CLOCK_DRIFT;
extern int CFD_MAXPROCESSES;
extern int NO_FORK;
extern int CFD_INTERVAL;
extern int DENYBADCLOCKS;
extern int MAXTRIES;
extern int LOGCONNS;
extern int LOGENCRYPT;
extern Item *CONNECTIONLIST;
extern Auth *ROLES;
extern Auth *ROLESTOP;

/*******************************************************************/

void KeepFileAccessPromise(Promise *pp);
void KeepLiteralAccessPromise(Promise *pp, char *type);
void KeepQueryAccessPromise(Promise *pp, char *type);

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

void KeepPromises(Policy *policy, GenericAgentConfig *config, const ReportContext *report_context)
{
    KeepContextBundles(policy, report_context);
    KeepControlPromises(policy, config);
    KeepPromiseBundles(policy, report_context);
}

/*******************************************************************/

void Summarize()
{
    Auth *ptr;
    Item *ip, *ipr;

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Summarize control promises\n");

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Granted access to paths :\n");

    for (ptr = VADMIT; ptr != NULL; ptr = ptr->next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Path: %s (encrypt=%d)\n", ptr->path, ptr->encrypt);

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "   Admit: %s root=", ip->name);
            for (ipr = ptr->maproot; ipr != NULL; ipr = ipr->next)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "%s,", ipr->name);
            }
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Denied access to paths :\n");

    for (ptr = VDENY; ptr != NULL; ptr = ptr->next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Path: %s\n", ptr->path);

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "   Deny: %s\n", ip->name);
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Granted access to literal/variable/query data :\n");

    for (ptr = VARADMIT; ptr != NULL; ptr = ptr->next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "  Object: %s (encrypt=%d)\n", ptr->path, ptr->encrypt);

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "   Admit: %s root=", ip->name);
            for (ipr = ptr->maproot; ipr != NULL; ipr = ipr->next)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "%s,", ipr->name);
            }
        }
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Denied access to literal/variable/query data :\n");

    for (ptr = VARDENY; ptr != NULL; ptr = ptr->next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "  Object: %s\n", ptr->path);

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", "   Deny: %s\n", ip->name);
        }
    }

    
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Host IPs allowed connection access :\n");

    for (ip = SV.nonattackerlist; ip != NULL; ip = ip->next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " .... IP: %s\n", ip->name);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host IPs denied connection access :\n");

    for (ip = SV.attackerlist; ip != NULL; ip = ip->next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " .... IP: %s\n", ip->name);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host IPs allowed multiple connection access :\n");

    for (ip = SV.multiconnlist; ip != NULL; ip = ip->next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " .... IP: %s\n", ip->name);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host IPs from whom we shall accept public keys on trust :\n");

    for (ip = SV.trustkeylist; ip != NULL; ip = ip->next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " .... IP: %s\n", ip->name);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Users from whom we accept connections :\n");

    for (ip = SV.allowuserlist; ip != NULL; ip = ip->next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " .... USERS: %s\n", ip->name);
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Host IPs from NAT which we don't verify :\n");

    for (ip = SV.skipverify; ip != NULL; ip = ip->next)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " .... IP: %s\n", ip->name);
    }

}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

static void KeepControlPromises(Policy *policy, GenericAgentConfig *config)
{
    Rval retval;

    CFD_MAXPROCESSES = 30;
    MAXTRIES = 5;
    CFD_INTERVAL = 0;
    DENYBADCLOCKS = true;
    CFRUNCOMMAND[0] = '\0';
    SetChecksumUpdates(true);

/* Keep promised agent behaviour - control bodies */

    Banner("Server control promises..");

    HashControls(policy, config);

/* Now expand */

    Seq *constraints = ControlBodyConstraints(policy, AGENT_TYPE_SERVER);
    if (constraints)
    {
        for (size_t i = 0; i < SeqLength(constraints); i++)
        {
            Constraint *cp = SeqAt(constraints, i);

            if (IsExcluded(cp->classes, NULL))
            {
                continue;
            }

            if (GetVariable("control_server", cp->lval, &retval) == DATA_TYPE_NONE)
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "Unknown lval %s in server control body", cp->lval);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_SERVER_FACILITY].lval) == 0)
            {
                SetFacility(retval.item);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_DENY_BAD_CLOCKS].lval) == 0)
            {
                DENYBADCLOCKS = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET denybadclocks = %d\n", DENYBADCLOCKS);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_LOG_ENCRYPTED_TRANSFERS].lval) == 0)
            {
                LOGENCRYPT = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET LOGENCRYPT = %d\n", LOGENCRYPT);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_LOG_ALL_CONNECTIONS].lval) == 0)
            {
                SV.logconns = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET LOGCONNS = %d\n", LOGCONNS);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_MAX_CONNECTIONS].lval) == 0)
            {
                CFD_MAXPROCESSES = (int) IntFromString(retval.item);
                MAXTRIES = CFD_MAXPROCESSES / 3;
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET maxconnections = %d\n", CFD_MAXPROCESSES);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_CALL_COLLECT_INTERVAL].lval) == 0)
            {
                COLLECT_INTERVAL = (int) 60 * IntFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET call_collect_interval = %d (seconds)\n", COLLECT_INTERVAL);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_LISTEN].lval) == 0)
            {
                SERVER_LISTEN = BooleanFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET server listen = %s \n",
                      (SERVER_LISTEN)? "true":"false");
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_CALL_COLLECT_WINDOW].lval) == 0)
            {
                COLLECT_WINDOW = (int) IntFromString(retval.item);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET collect_window = %d (seconds)\n", COLLECT_INTERVAL);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_CF_RUN_COMMAND].lval) == 0)
            {
                strncpy(CFRUNCOMMAND, retval.item, CF_BUFSIZE - 1);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET cfruncommand = %s\n", CFRUNCOMMAND);
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_ALLOW_CONNECTS].lval) == 0)
            {
                Rlist *rp;

                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET Allowing connections from ...\n");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SV.nonattackerlist, rp->item))
                    {
                        AppendItem(&SV.nonattackerlist, rp->item, cp->classes);
                    }
                }

                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_DENY_CONNECTS].lval) == 0)
            {
                Rlist *rp;

                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET Denying connections from ...\n");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SV.attackerlist, rp->item))
                    {
                        AppendItem(&SV.attackerlist, rp->item, cp->classes);
                    }
                }

                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_SKIP_VERIFY].lval) == 0)
            {
                Rlist *rp;

                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET Skip verify connections from ...\n");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SV.skipverify, rp->item))
                    {
                        AppendItem(&SV.skipverify, rp->item, cp->classes);
                    }
                }

                continue;
            }


            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_ALLOW_ALL_CONNECTS].lval) == 0)
            {
                Rlist *rp;

                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET Allowing multiple connections from ...\n");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SV.multiconnlist, rp->item))
                    {
                        AppendItem(&SV.multiconnlist, rp->item, cp->classes);
                    }
                }

                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_ALLOW_USERS].lval) == 0)
            {
                Rlist *rp;

                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET Allowing users ...\n");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SV.allowuserlist, rp->item))
                    {
                        AppendItem(&SV.allowuserlist, rp->item, cp->classes);
                    }
                }

                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_TRUST_KEYS_FROM].lval) == 0)
            {
                Rlist *rp;

                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET Trust keys from ...\n");

                for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
                {
                    if (!IsItemIn(SV.trustkeylist, rp->item))
                    {
                        AppendItem(&SV.trustkeylist, rp->item, cp->classes);
                    }
                }

                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_PORT_NUMBER].lval) == 0)
            {
                SHORT_CFENGINEPORT = (short) IntFromString(retval.item);
                strncpy(STR_CFENGINEPORT, retval.item, 15);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET default portnumber = %u = %s = %s\n", (int) SHORT_CFENGINEPORT, STR_CFENGINEPORT,
                      RvalScalarValue(retval));
                SHORT_CFENGINEPORT = htons((short) IntFromString(retval.item));
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_KEY_TTL].lval) == 0)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "Ignoring deprecated option keycacheTTL");
                continue;
            }

            if (strcmp(cp->lval, CFS_CONTROLBODY[SERVER_CONTROL_BIND_TO_INTERFACE].lval) == 0)
            {
                strncpy(BINDINTERFACE, retval.item, CF_BUFSIZE - 1);
                CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET bindtointerface = %s\n", BINDINTERFACE);
                continue;
            }
        }
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[COMMON_CONTROL_SYSLOG_HOST].lval, &retval) != DATA_TYPE_NONE)
    {
        SetSyslogHost(Hostname2IPString(retval.item));
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[COMMON_CONTROL_SYSLOG_PORT].lval, &retval) != DATA_TYPE_NONE)
    {
        SetSyslogPort(IntFromString(retval.item));
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[COMMON_CONTROL_FIPS_MODE].lval, &retval) != DATA_TYPE_NONE)
    {
        FIPS_MODE = BooleanFromString(retval.item);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "SET FIPS_MODE = %d\n", FIPS_MODE);
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[COMMON_CONTROL_LASTSEEN_EXPIRE_AFTER].lval, &retval) != DATA_TYPE_NONE)
    {
        LASTSEENEXPIREAFTER = IntFromString(retval.item) * 60;
    }
}

/*********************************************************************/

static void KeepContextBundles(Policy *policy, const ReportContext *report_context)
{
    char *scope;

/* Dial up the generic promise expansion with a callback */

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);

        scope = bp->name;
        SetNewScope(bp->name);

        if ((strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_SERVER]) == 0) || (strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_COMMON]) == 0))
        {
            DeletePrivateClassContext();        // Each time we change bundle

            BannerBundle(bp, NULL);
            scope = bp->name;

            for (size_t j = 0; j < SeqLength(bp->subtypes); j++)
            {
                SubType *sp = SeqAt(bp->subtypes, j);

                if ((strcmp(sp->name, "vars") != 0) && (strcmp(sp->name, "classes") != 0))
                {
                    continue;
                }

                BannerSubType(scope, sp->name, 0);
                SetScope(scope);
                AugmentScope(scope, bp->ns, NULL, NULL);

                for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
                {
                    Promise *pp = SeqAt(sp->promises, ppi);
                    ExpandPromise(AGENT_TYPE_SERVER, scope, pp, KeepServerPromise, report_context);
                }
            }
        }
    }
}

/*********************************************************************/

static void KeepPromiseBundles(Policy *policy, const ReportContext *report_context)
{
    char *scope;

/* Dial up the generic promise expansion with a callback */

    for (size_t i = 0; i < SeqLength(policy->bundles); i++)
    {
        Bundle *bp = SeqAt(policy->bundles, i);

        scope = bp->name;
        SetNewScope(bp->name);

        if ((strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_SERVER]) == 0) || (strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_COMMON]) == 0))
        {
            DeletePrivateClassContext();        // Each time we change bundle

            BannerBundle(bp, NULL);
            scope = bp->name;

            for (size_t j = 0; j < SeqLength(bp->subtypes); j++)
            {
                SubType *sp = SeqAt(bp->subtypes, j);

                if ((strcmp(sp->name, "access") != 0) && (strcmp(sp->name, "roles") != 0))
                {
                    continue;
                }

                BannerSubType(scope, sp->name, 0);
                SetScope(scope);
                AugmentScope(scope, bp->ns, NULL, NULL);

                for (size_t ppi = 0; ppi < SeqLength(sp->promises); ppi++)
                {
                    Promise *pp = SeqAt(sp->promises, ppi);
                    ExpandPromise(AGENT_TYPE_SERVER, scope, pp, KeepServerPromise, report_context);
                }
            }
        }
    }
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

static void KeepServerPromise(Promise *pp)
{
    char *sp = NULL;

    if (!IsDefinedClass(pp->classes, pp->ns))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Skipping whole promise, as context is %s\n", pp->classes);
        return;
    }

    if (VarClassExcluded(pp, &sp))
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "\n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Skipping whole next promise (%s), as var-context %s is not relevant\n", pp->promiser,
              sp);
        CfOut(OUTPUT_LEVEL_VERBOSE, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        return;
    }

    if (strcmp(pp->agentsubtype, "classes") == 0)
    {
        KeepClassContextPromise(pp);
        return;
    }

    sp = (char *) ConstraintGetRvalValue("resource_type", pp, RVAL_TYPE_SCALAR);

    if ((strcmp(pp->agentsubtype, "access") == 0) && sp && (strcmp(sp, "literal") == 0))
    {
        KeepLiteralAccessPromise(pp, "literal");
        return;
    }

    if ((strcmp(pp->agentsubtype, "access") == 0) && sp && (strcmp(sp, "variable") == 0))
    {
        KeepLiteralAccessPromise(pp, "variable");
        return;
    }
    
    if ((strcmp(pp->agentsubtype, "access") == 0) && sp && (strcmp(sp, "query") == 0))
    {
        KeepQueryAccessPromise(pp, "query");
        return;
    }

    if ((strcmp(pp->agentsubtype, "access") == 0) && sp && (strcmp(sp, "context") == 0))
    {
        KeepLiteralAccessPromise(pp, "context");
        return;
    }

/* Default behaviour is file access */

    if (strcmp(pp->agentsubtype, "access") == 0)
    {
        KeepFileAccessPromise(pp);
        return;
    }

    if (strcmp(pp->agentsubtype, "roles") == 0)
    {
        KeepServerRolePromise(pp);
        return;
    }
}

/*********************************************************************/

void KeepFileAccessPromise(Promise *pp)
{
    Rlist *rp;
    Auth *ap, *dp;

    if (strlen(pp->promiser) != 1)
    {
        DeleteSlash(pp->promiser);
    }

    if (!GetAuthPath(pp->promiser, VADMIT))
    {
        InstallServerAuthPath(pp->promiser, &VADMIT, &VADMITTOP);
    }

    if (!GetAuthPath(pp->promiser, VDENY))
    {
        InstallServerAuthPath(pp->promiser, &VDENY, &VDENYTOP);
    }

    ap = GetAuthPath(pp->promiser, VADMIT);
    dp = GetAuthPath(pp->promiser, VDENY);

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (!IsDefinedClass(cp->classes, pp->ns))
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
                    PrependItem(&(ap->accesslist), rp->item, NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_DENY].lval) == 0)
                {
                    PrependItem(&(dp->accesslist), rp->item, NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_MAPROOT].lval) == 0)
                {
                    PrependItem(&(ap->maproot), rp->item, NULL);
                    continue;
                }
            }
            break;

        default:
            /* Shouldn't happen */
            break;
        }
    }
}

/*********************************************************************/

void KeepLiteralAccessPromise(Promise *pp, char *type)
{
    Rlist *rp;
    Auth *ap = NULL, *dp = NULL;
    char *handle = ConstraintGetRvalValue("handle", pp, RVAL_TYPE_SCALAR);

    if ((handle == NULL) && (strcmp(type,"literal") == 0))
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", "Access to literal server data requires you to define a promise handle for reference");
        return;
    }
    
    if (strcmp(type, "literal") == 0)
    {
        CfOut(OUTPUT_LEVEL_VERBOSE,""," -> Looking at literal access promise \"%s\", type %s",pp->promiser, type);

        if (!GetAuthPath(handle, VARADMIT))
        {
            InstallServerAuthPath(handle, &VARADMIT, &VARADMITTOP);
        }

        if (!GetAuthPath(handle, VARDENY))
        {
            InstallServerAuthPath(handle, &VARDENY, &VARDENYTOP);
        }

        RegisterLiteralServerData(handle, pp);
        ap = GetAuthPath(handle, VARADMIT);
        dp = GetAuthPath(handle, VARDENY);
        ap->literal = true;
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE,""," -> Looking at context/var access promise \"%s\", type %s",pp->promiser, type);

        if (!GetAuthPath(pp->promiser, VARADMIT))
        {
            InstallServerAuthPath(pp->promiser, &VARADMIT, &VARADMITTOP);
        }

        if (!GetAuthPath(pp->promiser, VARDENY))
        {
            InstallServerAuthPath(pp->promiser, &VARDENY, &VARDENYTOP);
        }


        if (strcmp(type, "context") == 0)
        {
            ap = GetAuthPath(pp->promiser, VARADMIT);
            dp = GetAuthPath(pp->promiser, VARDENY);
            ap->classpattern = true;
        }

        if (strcmp(type, "variable") == 0)
        {
            ap = GetAuthPath(pp->promiser, VARADMIT); // Allow the promiser (preferred) as well as handle as variable name
            dp = GetAuthPath(pp->promiser, VARDENY);
            ap->variable = true;
        }
    }
    
    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (!IsDefinedClass(cp->classes, pp->ns))
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
                    PrependItem(&(ap->accesslist), rp->item, NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_DENY].lval) == 0)
                {
                    PrependItem(&(dp->accesslist), rp->item, NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_MAPROOT].lval) == 0)
                {
                    PrependItem(&(ap->maproot), rp->item, NULL);
                    continue;
                }
            }
            break;

        default:
            /* Shouldn't happen */
            break;
        }
    }
}

/*********************************************************************/

void KeepQueryAccessPromise(Promise *pp, char *type)
{
    Rlist *rp;
    Auth *ap, *dp;

    if (!GetAuthPath(pp->promiser, VARADMIT))
    {
        InstallServerAuthPath(pp->promiser, &VARADMIT, &VARADMITTOP);
    }

    RegisterLiteralServerData(pp->promiser, pp);

    if (!GetAuthPath(pp->promiser, VARDENY))
    {
        InstallServerAuthPath(pp->promiser, &VARDENY, &VARDENYTOP);
    }

    ap = GetAuthPath(pp->promiser, VARADMIT);
    dp = GetAuthPath(pp->promiser, VARDENY);

    if (strcmp(type, "query") == 0)
    {
        ap->literal = true;
    }

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (!IsDefinedClass(cp->classes, pp->ns))
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
                    PrependItem(&(ap->accesslist), rp->item, NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_DENY].lval) == 0)
                {
                    PrependItem(&(dp->accesslist), rp->item, NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[REMOTE_ACCESS_MAPROOT].lval) == 0)
                {
                    PrependItem(&(ap->maproot), rp->item, NULL);
                    continue;
                }
            }
            break;

        default:
            /* Shouldn't happen */
            break;
        }
    }
}

/*********************************************************************/

static void KeepServerRolePromise(Promise *pp)
{
    Rlist *rp;
    Auth *ap;

    if (!GetAuthPath(pp->promiser, ROLES))
    {
        InstallServerAuthPath(pp->promiser, &ROLES, &ROLESTOP);
    }

    ap = GetAuthPath(pp->promiser, ROLES);

    for (size_t i = 0; i < SeqLength(pp->conlist); i++)
    {
        Constraint *cp = SeqAt(pp->conlist, i);

        if (!IsDefinedClass(cp->classes, pp->ns))
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
                    PrependItem(&(ap->accesslist), rp->item, NULL);
                    continue;
                }
            }
            break;

        case RVAL_TYPE_FNCALL:
            /* Shouldn't happen */
            break;

        default:

            if ((strcmp(cp->lval, "comment") == 0) || (strcmp(cp->lval, "handle") == 0))
            {
            }
            else
            {
                CfOut(OUTPUT_LEVEL_ERROR, "", "RHS of authorize promise for %s should be a list\n", pp->promiser);
            }
            break;
        }
    }
}

/***********************************************************************/
/* Level                                                               */
/***********************************************************************/

static void InstallServerAuthPath(char *path, Auth **list, Auth **listtop)
{
    Auth *ptr;

#ifdef __MINGW32__
    int i;

    for (i = 0; path[i] != '\0'; i++)
    {
        path[i] = ToLower(path[i]);
    }
#endif /* __MINGW32__ */

    ptr = xcalloc(1, sizeof(Auth));

    if (*listtop == NULL)       /* First element in the list */
    {
        *list = ptr;
    }
    else
    {
        (*listtop)->next = ptr;
    }

    ptr->path = xstrdup(path);
    *listtop = ptr;
}

/***********************************************************************/
/* Level                                                               */
/***********************************************************************/

Auth *GetAuthPath(char *path, Auth *list)
{
    Auth *ap;

#ifdef __MINGW32__
    int i;

    for (i = 0; path[i] != '\0'; i++)
    {
        path[i] = ToLower(path[i]);
    }
#endif /* __MINGW32__ */

    if (strlen(path) != 1)
    {
        DeleteSlash(path);
    }

    for (ap = list; ap != NULL; ap = ap->next)
    {
        if (strcmp(ap->path, path) == 0)
        {
            return ap;
        }
    }

    return NULL;
}

/***********************************************************************/
