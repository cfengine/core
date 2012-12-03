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

#include "cf3.defs.h"
#include "server.h"

#include "env_context.h"
#include "files_names.h"
#include "mod_access.h"
#include "constraints.h"
#include "item_lib.h"
#include "conversion.h"
#include "reporting.h"
#include "expand.h"
#include "transaction.h"
#include "scope.h"
#include "unix.h"
#include "attributes.h"
#include "cfstream.h"
#include "communication.h"
#include "string_lib.h"

static void KeepContextBundles(Policy *policy, const ReportContext *report_context);
static void KeepServerPromise(Promise *pp);
static void InstallServerAuthPath(char *path, Auth **list, Auth **listtop);
static void KeepServerRolePromise(Promise *pp);
static void KeepPromiseBundles(Policy *policy, const ReportContext *report_context);

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

void KeepPromises(Policy *policy, const ReportContext *report_context)
{
    KeepContextBundles(policy, report_context);
    KeepControlPromises(policy);
    KeepPromiseBundles(policy, report_context);
}

/*******************************************************************/

void Summarize()
{
    Auth *ptr;
    Item *ip, *ipr;

    CfOut(cf_verbose, "", "Summarize control promises\n");

    CfOut(cf_verbose, "", "Granted access to paths :\n");

    for (ptr = VADMIT; ptr != NULL; ptr = ptr->next)
    {
        CfOut(cf_verbose, "", "Path: %s (encrypt=%d)\n", ptr->path, ptr->encrypt);

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            CfOut(cf_verbose, "", "   Admit: %s root=", ip->name);
            for (ipr = ptr->maproot; ipr != NULL; ipr = ipr->next)
            {
                CfOut(cf_verbose, "", "%s,", ipr->name);
            }
        }
    }

    CfOut(cf_verbose, "", "Denied access to paths :\n");

    for (ptr = VDENY; ptr != NULL; ptr = ptr->next)
    {
        CfOut(cf_verbose, "", "Path: %s\n", ptr->path);

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            CfOut(cf_verbose, "", "   Deny: %s\n", ip->name);
        }
    }

    CfOut(cf_verbose, "", "Granted access to literal/variable/query data :\n");

    for (ptr = VARADMIT; ptr != NULL; ptr = ptr->next)
    {
        CfOut(cf_verbose, "", "  Object: %s (encrypt=%d)\n", ptr->path, ptr->encrypt);

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            CfOut(cf_verbose, "", "   Admit: %s root=", ip->name);
            for (ipr = ptr->maproot; ipr != NULL; ipr = ipr->next)
            {
                CfOut(cf_verbose, "", "%s,", ipr->name);
            }
        }
    }

    CfOut(cf_verbose, "", "Denied access to literal/variable/query data :\n");

    for (ptr = VARDENY; ptr != NULL; ptr = ptr->next)
    {
        CfOut(cf_verbose, "", "  Object: %s\n", ptr->path);

        for (ip = ptr->accesslist; ip != NULL; ip = ip->next)
        {
            CfOut(cf_verbose, "", "   Deny: %s\n", ip->name);
        }
    }

    
    CfOut(cf_verbose, "", " -> Host IPs allowed connection access :\n");

    for (ip = SV.nonattackerlist; ip != NULL; ip = ip->next)
    {
        CfOut(cf_verbose, "", " .... IP: %s\n", ip->name);
    }

    CfOut(cf_verbose, "", "Host IPs denied connection access :\n");

    for (ip = SV.attackerlist; ip != NULL; ip = ip->next)
    {
        CfOut(cf_verbose, "", " .... IP: %s\n", ip->name);
    }

    CfOut(cf_verbose, "", "Host IPs allowed multiple connection access :\n");

    for (ip = SV.multiconnlist; ip != NULL; ip = ip->next)
    {
        CfOut(cf_verbose, "", " .... IP: %s\n", ip->name);
    }

    CfOut(cf_verbose, "", "Host IPs from whom we shall accept public keys on trust :\n");

    for (ip = SV.trustkeylist; ip != NULL; ip = ip->next)
    {
        CfOut(cf_verbose, "", " .... IP: %s\n", ip->name);
    }

    CfOut(cf_verbose, "", "Users from whom we accept connections :\n");

    for (ip = SV.allowuserlist; ip != NULL; ip = ip->next)
    {
        CfOut(cf_verbose, "", " .... USERS: %s\n", ip->name);
    }

    CfOut(cf_verbose, "", "Host IPs from NAT which we don't verify :\n");

    for (ip = SV.skipverify; ip != NULL; ip = ip->next)
    {
        CfOut(cf_verbose, "", " .... IP: %s\n", ip->name);
    }

}

/*******************************************************************/
/* Level                                                           */
/*******************************************************************/

void KeepControlPromises(Policy *policy)
{
    Constraint *cp;
    Rval retval;

    CFD_MAXPROCESSES = 30;
    MAXTRIES = 5;
    CFD_INTERVAL = 0;
    DENYBADCLOCKS = true;
    CFRUNCOMMAND[0] = '\0';
    SetChecksumUpdates(true);

/* Keep promised agent behaviour - control bodies */

    Banner("Server control promises..");

    HashControls(policy);

/* Now expand */

    for (cp = ControlBodyConstraints(policy, AGENT_TYPE_SERVER); cp != NULL; cp = cp->next)
    {
        if (IsExcluded(cp->classes, NULL))
        {
            continue;
        }

        if (GetVariable("control_server", cp->lval, &retval) == cf_notype)
        {
            CfOut(cf_error, "", "Unknown lval %s in server control body", cp->lval);
            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_serverfacility].lval) == 0)
        {
            SetFacility(retval.item);
            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_denybadclocks].lval) == 0)
        {
            DENYBADCLOCKS = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET denybadclocks = %d\n", DENYBADCLOCKS);
            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_logencryptedtransfers].lval) == 0)
        {
            LOGENCRYPT = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET LOGENCRYPT = %d\n", LOGENCRYPT);
            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_logallconnections].lval) == 0)
        {
            SV.logconns = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET LOGCONNS = %d\n", LOGCONNS);
            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_maxconnections].lval) == 0)
        {
            CFD_MAXPROCESSES = (int) Str2Int(retval.item);
            MAXTRIES = CFD_MAXPROCESSES / 3;
            CfOut(cf_verbose, "", "SET maxconnections = %d\n", CFD_MAXPROCESSES);
            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_call_collect_interval].lval) == 0)
        {
            COLLECT_INTERVAL = (int) 60 * Str2Int(retval.item);
            CfOut(cf_verbose, "", "SET call_collect_interval = %d (seconds)\n", COLLECT_INTERVAL);
            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_listen].lval) == 0)
        {
            SERVER_LISTEN = GetBoolean(retval.item);
            CfOut(cf_verbose, "", "SET server listen = %s \n",
                  (SERVER_LISTEN)? "true":"false");
            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_collect_window].lval) == 0)
        {
            COLLECT_WINDOW = (int) Str2Int(retval.item);
            CfOut(cf_verbose, "", "SET collect_window = %d (seconds)\n", COLLECT_INTERVAL);
            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_cfruncommand].lval) == 0)
        {
            strncpy(CFRUNCOMMAND, retval.item, CF_BUFSIZE - 1);
            CfOut(cf_verbose, "", "SET cfruncommand = %s\n", CFRUNCOMMAND);
            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_allowconnects].lval) == 0)
        {
            Rlist *rp;

            CfOut(cf_verbose, "", "SET Allowing connections from ...\n");

            for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
            {
                if (!IsItemIn(SV.nonattackerlist, rp->item))
                {
                    AppendItem(&SV.nonattackerlist, rp->item, cp->classes);
                }
            }

            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_denyconnects].lval) == 0)
        {
            Rlist *rp;

            CfOut(cf_verbose, "", "SET Denying connections from ...\n");

            for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
            {
                if (!IsItemIn(SV.attackerlist, rp->item))
                {
                    AppendItem(&SV.attackerlist, rp->item, cp->classes);
                }
            }

            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_skipverify].lval) == 0)
        {
            Rlist *rp;

            CfOut(cf_verbose, "", "SET Skip verify connections from ...\n");

            for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
            {
                if (!IsItemIn(SV.skipverify, rp->item))
                {
                    AppendItem(&SV.skipverify, rp->item, cp->classes);
                }
            }

            continue;
        }


        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_allowallconnects].lval) == 0)
        {
            Rlist *rp;

            CfOut(cf_verbose, "", "SET Allowing multiple connections from ...\n");

            for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
            {
                if (!IsItemIn(SV.multiconnlist, rp->item))
                {
                    AppendItem(&SV.multiconnlist, rp->item, cp->classes);
                }
            }

            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_allowusers].lval) == 0)
        {
            Rlist *rp;

            CfOut(cf_verbose, "", "SET Allowing users ...\n");

            for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
            {
                if (!IsItemIn(SV.allowuserlist, rp->item))
                {
                    AppendItem(&SV.allowuserlist, rp->item, cp->classes);
                }
            }

            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_trustkeysfrom].lval) == 0)
        {
            Rlist *rp;

            CfOut(cf_verbose, "", "SET Trust keys from ...\n");

            for (rp = (Rlist *) retval.item; rp != NULL; rp = rp->next)
            {
                if (!IsItemIn(SV.trustkeylist, rp->item))
                {
                    AppendItem(&SV.trustkeylist, rp->item, cp->classes);
                }
            }

            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_portnumber].lval) == 0)
        {
            SHORT_CFENGINEPORT = (short) Str2Int(retval.item);
            strncpy(STR_CFENGINEPORT, retval.item, 15);
            CfOut(cf_verbose, "", "SET default portnumber = %u = %s = %s\n", (int) SHORT_CFENGINEPORT, STR_CFENGINEPORT,
                  ScalarRvalValue(retval));
            SHORT_CFENGINEPORT = htons((short) Str2Int(retval.item));
            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_keyttl].lval) == 0)
        {
            CfOut(cf_verbose, "", "Ignoring deprecated option keycacheTTL");
            continue;
        }

        if (strcmp(cp->lval, CFS_CONTROLBODY[cfs_bindtointerface].lval) == 0)
        {
            strncpy(BINDINTERFACE, retval.item, CF_BUFSIZE - 1);
            CfOut(cf_verbose, "", "SET bindtointerface = %s\n", BINDINTERFACE);
            continue;
        }
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[cfg_syslog_host].lval, &retval) != cf_notype)
    {
        SetSyslogHost(Hostname2IPString(retval.item));
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[cfg_syslog_port].lval, &retval) != cf_notype)
    {
        SetSyslogPort(Str2Int(retval.item));
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[cfg_fips_mode].lval, &retval) != cf_notype)
    {
        FIPS_MODE = GetBoolean(retval.item);
        CfOut(cf_verbose, "", "SET FIPS_MODE = %d\n", FIPS_MODE);
    }

    if (GetVariable("control_common", CFG_CONTROLBODY[cfg_lastseenexpireafter].lval, &retval) != cf_notype)
    {
        LASTSEENEXPIREAFTER = Str2Int(retval.item) * 60;
    }
}

/*********************************************************************/

static void KeepContextBundles(Policy *policy, const ReportContext *report_context)
{
    SubType *sp;
    Promise *pp;
    char *scope;

/* Dial up the generic promise expansion with a callback */

    for (Bundle *bp = policy->bundles; bp != NULL; bp = bp->next)       /* get schedule */
    {
        scope = bp->name;
        SetNewScope(bp->name);

        if ((strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_SERVER]) == 0) || (strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_COMMON]) == 0))
        {
            DeletePrivateClassContext();        // Each time we change bundle

            BannerBundle(bp, NULL);
            scope = bp->name;

            for (sp = bp->subtypes; sp != NULL; sp = sp->next)  /* get schedule */
            {
                if ((strcmp(sp->name, "vars") != 0) && (strcmp(sp->name, "classes") != 0))
                {
                    continue;
                }

                BannerSubType(scope, sp->name, 0);
                SetScope(scope);
                AugmentScope(scope, bp->namespace, NULL, NULL);

                for (pp = sp->promiselist; pp != NULL; pp = pp->next)
                {
                    ExpandPromise(AGENT_TYPE_SERVER, scope, pp, KeepServerPromise, report_context);
                }
            }
        }
    }
}

/*********************************************************************/

static void KeepPromiseBundles(Policy *policy, const ReportContext *report_context)
{
    SubType *sp;
    Promise *pp;
    char *scope;

/* Dial up the generic promise expansion with a callback */

    for (Bundle *bp = policy->bundles; bp != NULL; bp = bp->next)       /* get schedule */
    {
        scope = bp->name;
        SetNewScope(bp->name);

        if ((strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_SERVER]) == 0) || (strcmp(bp->type, CF_AGENTTYPES[AGENT_TYPE_COMMON]) == 0))
        {
            DeletePrivateClassContext();        // Each time we change bundle

            BannerBundle(bp, NULL);
            scope = bp->name;

            for (sp = bp->subtypes; sp != NULL; sp = sp->next)  /* get schedule */
            {
                if ((strcmp(sp->name, "access") != 0) && (strcmp(sp->name, "roles") != 0))
                {
                    continue;
                }

                BannerSubType(scope, sp->name, 0);
                SetScope(scope);
                AugmentScope(scope, bp->namespace, NULL, NULL);

                for (pp = sp->promiselist; pp != NULL; pp = pp->next)
                {
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

    if (!IsDefinedClass(pp->classes, pp->namespace))
    {
        CfOut(cf_verbose, "", "Skipping whole promise, as context is %s\n", pp->classes);
        return;
    }

    if (VarClassExcluded(pp, &sp))
    {
        CfOut(cf_verbose, "", "\n");
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        CfOut(cf_verbose, "", "Skipping whole next promise (%s), as var-context %s is not relevant\n", pp->promiser,
              sp);
        CfOut(cf_verbose, "", ". . . . . . . . . . . . . . . . . . . . . . . . . . . . \n");
        return;
    }

    if (strcmp(pp->agentsubtype, "classes") == 0)
    {
        KeepClassContextPromise(pp);
        return;
    }

    sp = (char *) GetConstraintValue("resource_type", pp, CF_SCALAR);

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
    Constraint *cp;
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

    for (cp = pp->conlist; cp != NULL; cp = cp->next)
    {
        if (!IsDefinedClass(cp->classes, pp->namespace))
        {
            continue;
        }

        switch (cp->rval.rtype)
        {
        case CF_SCALAR:

            if (strcmp(cp->lval, CF_REMACCESS_BODIES[cfs_encrypted].lval) == 0)
            {
                ap->encrypt = true;
            }

            break;

        case CF_LIST:

            for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
            {
                if (strcmp(cp->lval, CF_REMACCESS_BODIES[cfs_admit].lval) == 0)
                {
                    PrependItem(&(ap->accesslist), rp->item, NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[cfs_deny].lval) == 0)
                {
                    PrependItem(&(dp->accesslist), rp->item, NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[cfs_maproot].lval) == 0)
                {
                    PrependItem(&(ap->maproot), rp->item, NULL);
                    continue;
                }
            }
            break;

        case CF_FNCALL:
            /* Shouldn't happen */
            break;
        }
    }
}

/*********************************************************************/

void KeepLiteralAccessPromise(Promise *pp, char *type)
{
    Constraint *cp;
    Rlist *rp;
    Auth *ap = NULL, *dp = NULL;
    char *handle = GetConstraintValue("handle", pp, CF_SCALAR);

    if ((handle == NULL) && (strcmp(type,"literal") == 0))
    {
        CfOut(cf_error, "", "Access to literal server data requires you to define a promise handle for reference");
        return;
    }
    
    if (strcmp(type, "literal") == 0)
    {
        CfOut(cf_verbose,""," -> Looking at literal access promise \"%s\", type %s",pp->promiser, type);

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
        CfOut(cf_verbose,""," -> Looking at context/var access promise \"%s\", type %s",pp->promiser, type);

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
    
    for (cp = pp->conlist; cp != NULL; cp = cp->next)
    {
        if (!IsDefinedClass(cp->classes, pp->namespace))
        {
            continue;
        }

        switch (cp->rval.rtype)
        {
        case CF_SCALAR:

            if (strcmp(cp->lval, CF_REMACCESS_BODIES[cfs_encrypted].lval) == 0)
            {
                ap->encrypt = true;
            }

            break;

        case CF_LIST:

            for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
            {
                if (strcmp(cp->lval, CF_REMACCESS_BODIES[cfs_admit].lval) == 0)
                {
                    PrependItem(&(ap->accesslist), rp->item, NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[cfs_deny].lval) == 0)
                {
                    PrependItem(&(dp->accesslist), rp->item, NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[cfs_maproot].lval) == 0)
                {
                    PrependItem(&(ap->maproot), rp->item, NULL);
                    continue;
                }
            }
            break;

        case CF_FNCALL:
            /* Shouldn't happen */
            break;
        }
    }
}

/*********************************************************************/

void KeepQueryAccessPromise(Promise *pp, char *type)
{
    Constraint *cp;
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

    for (cp = pp->conlist; cp != NULL; cp = cp->next)
    {
        if (!IsDefinedClass(cp->classes, pp->namespace))
        {
            continue;
        }

        switch (cp->rval.rtype)
        {
        case CF_SCALAR:

            if (strcmp(cp->lval, CF_REMACCESS_BODIES[cfs_encrypted].lval) == 0)
            {
                ap->encrypt = true;
            }

            break;

        case CF_LIST:

            for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
            {
                if (strcmp(cp->lval, CF_REMACCESS_BODIES[cfs_admit].lval) == 0)
                {
                    PrependItem(&(ap->accesslist), rp->item, NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[cfs_deny].lval) == 0)
                {
                    PrependItem(&(dp->accesslist), rp->item, NULL);
                    continue;
                }

                if (strcmp(cp->lval, CF_REMACCESS_BODIES[cfs_maproot].lval) == 0)
                {
                    PrependItem(&(ap->maproot), rp->item, NULL);
                    continue;
                }
            }
            break;

        case CF_FNCALL:
            /* Shouldn't happen */
            break;
        }
    }
}

/*********************************************************************/

static void KeepServerRolePromise(Promise *pp)
{
    Constraint *cp;
    Rlist *rp;
    Auth *ap;

    if (!GetAuthPath(pp->promiser, ROLES))
    {
        InstallServerAuthPath(pp->promiser, &ROLES, &ROLESTOP);
    }

    ap = GetAuthPath(pp->promiser, ROLES);

    for (cp = pp->conlist; cp != NULL; cp = cp->next)
    {
        if (!IsDefinedClass(cp->classes, pp->namespace))
        {
            continue;
        }

        switch (cp->rval.rtype)
        {
        case CF_LIST:

            for (rp = (Rlist *) cp->rval.item; rp != NULL; rp = rp->next)
            {
                if (strcmp(cp->lval, CF_REMROLE_BODIES[cfs_authorize].lval) == 0)
                {
                    PrependItem(&(ap->accesslist), rp->item, NULL);
                    continue;
                }
            }
            break;

        case CF_FNCALL:
            /* Shouldn't happen */
            break;

        default:

            if ((strcmp(cp->lval, "comment") == 0) || (strcmp(cp->lval, "handle") == 0))
            {
            }
            else
            {
                CfOut(cf_error, "", "RHS of authorize promise for %s should be a list\n", pp->promiser);
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

#ifdef MINGW
    int i;

    for (i = 0; path[i] != '\0'; i++)
    {
        path[i] = ToLower(path[i]);
    }
#endif /* MINGW */

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

#ifdef MINGW
    int i;

    for (i = 0; path[i] != '\0'; i++)
    {
        path[i] = ToLower(path[i]);
    }
#endif /* MINGW */

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
