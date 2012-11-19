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
#include "files_lib.h"

#include "env_context.h"
#include "promises.h"
#include "vars.h"
#include "conversion.h"
#include "attributes.h"
#include "cfstream.h"

#ifndef HAVE_LIBVIRT

void NewEnvironmentsContext(void)
{
}

void DeleteEnvironmentsContext(void)
{
}

void VerifyEnvironmentsPromise(Promise *pp)
{
}

#endif

/*****************************************************************************/

#ifdef HAVE_LIBVIRT

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

virConnectPtr CFVC[cfv_none];

#define CF_MAX_CONCURRENT_ENVIRONMENTS 256

int CF_RUNNING[CF_MAX_CONCURRENT_ENVIRONMENTS];
char *CF_SUSPENDED[CF_MAX_CONCURRENT_ENVIRONMENTS];

/*****************************************************************************/


static int EnvironmentsSanityChecks(Attributes a, Promise *pp);
static void VerifyEnvironments(Attributes a, Promise *pp);
static void VerifyVirtDomain(char *uri, enum cfhypervisors envtype, Attributes a, Promise *pp);
static void VerifyVirtNetwork(char *uri, enum cfhypervisors envtype, Attributes a, Promise *pp);
static int CreateVirtDom(virConnectPtr vc, char *uri, Attributes a, Promise *pp);
static int DeleteVirt(virConnectPtr vc, char *uri, Attributes a, Promise *pp);
static int DeleteVirt(virConnectPtr vc, char *uri, Attributes a, Promise *pp);
static int RunningVirt(virConnectPtr vc, char *uri, Attributes a, Promise *pp);
static int SuspendedVirt(virConnectPtr vc, char *uri, Attributes a, Promise *pp);
static int DownVirt(virConnectPtr vc, char *uri, Attributes a, Promise *pp);
static int VerifyZone(Attributes a, Promise *pp);
static void EnvironmentErrorHandler(void);
static void ShowRunList(virConnectPtr vc);
static void ShowDormant(virConnectPtr vc);
static int CreateVirtNetwork(virConnectPtr vc, char **networks, Attributes a, Promise *pp);
static int DeleteVirtNetwork(virConnectPtr vc, char **networks, Attributes a, Promise *pp);

/*****************************************************************************/


void NewEnvironmentsContext(void)
{
    int i;

    for (i = 0; i < cfv_none; i++)
    {
        CFVC[i] = NULL;
    }
}

void DeleteEnvironmentsContext(void)
{
    int i;

    for (i = 0; i < cfv_none; i++)
    {
        if (CFVC[i] != NULL)
        {
            virConnectClose(CFVC[i]);
            CFVC[i] = NULL;
        }
    }
}

/*****************************************************************************/

void VerifyEnvironmentsPromise(Promise *pp)
{
    Attributes a = { {0} };
    CfLock thislock;
    Promise *pexp;

    a = GetEnvironmentsAttributes(pp);

    if (EnvironmentsSanityChecks(a, pp))
    {
        thislock = AcquireLock("virtual", VUQNAME, CFSTARTTIME, a, pp, false);

        if (thislock.lock == NULL)
        {
            return;
        }

        CF_OCCUR++;

        PromiseBanner(pp);
        NewScalar("this", "promiser", pp->promiser, cf_str);

        pexp = ExpandDeRefPromise("this", pp);
        VerifyEnvironments(a, pp);
        DeletePromise(pexp);
    }

    YieldCurrentLock(thislock);
}

/*****************************************************************************/

static int EnvironmentsSanityChecks(Attributes a, Promise *pp)
{
    if (a.env.spec)
    {
        if (a.env.cpus != CF_NOINT || a.env.memory != CF_NOINT || a.env.disk != CF_NOINT)
        {
            CfOut(cf_error, "", " !! Conflicting promise of both a spec and cpu/memory/disk resources");
            return false;
        }
    }

    if (a.env.host == NULL)
    {
        CfOut(cf_error, "", " !! No environment_host defined for environment promise");
        PromiseRef(cf_error, pp);
        return false;
    }

    switch (Str2Hypervisors(a.env.type))
    {
    case cfv_virt_xen_net:
    case cfv_virt_kvm_net:
    case cfv_virt_esx_net:
    case cfv_virt_test_net:
        if (a.env.cpus != CF_NOINT || a.env.memory != CF_NOINT || a.env.disk != CF_NOINT || a.env.name
            || a.env.addresses)
        {
            CfOut(cf_error, "", " !! Network environment promises computational resources (%d,%d,%d,%s)", a.env.cpus,
                  a.env.memory, a.env.disk, a.env.name);
            PromiseRef(cf_error, pp);
        }
        break;
    default:
        break;
    }

    return true;
}

/*****************************************************************************/

static void VerifyEnvironments(Attributes a, Promise *pp)
{
    char hyper_uri[CF_MAXVARSIZE];
    enum cfhypervisors envtype = cfv_none;

    switch (Str2Hypervisors(a.env.type))
    {
    case cfv_virt_xen:
    case cfv_virt_xen_net:
        snprintf(hyper_uri, CF_MAXVARSIZE - 1, "xen:///");
        envtype = cfv_virt_xen;
        break;

    case cfv_virt_kvm:
    case cfv_virt_kvm_net:
        snprintf(hyper_uri, CF_MAXVARSIZE - 1, "qemu:///session");
        envtype = cfv_virt_kvm;
        break;
    case cfv_virt_esx:
    case cfv_virt_esx_net:
        snprintf(hyper_uri, CF_MAXVARSIZE - 1, "esx://127.0.0.1");
        envtype = cfv_virt_esx;
        break;

    case cfv_virt_test:
    case cfv_virt_test_net:
        snprintf(hyper_uri, CF_MAXVARSIZE - 1, "test:///default");
        envtype = cfv_virt_test;
        break;

    case cfv_virt_vbox:
        snprintf(hyper_uri, CF_MAXVARSIZE - 1, "vbox:///session");
        envtype = cfv_virt_vbox;
        break;

    case cfv_zone:
        snprintf(hyper_uri, CF_MAXVARSIZE - 1, "solaris_zone");
        envtype = cfv_zone;
        break;

    default:
        CfOut(cf_error, "", " !! Environment type \"%s\" not currently supported", a.env.type);
        return;
        break;
    }

    CfOut(cf_verbose, "", " -> Selecting environment type \"%s\" -> \"%s\"", a.env.type, hyper_uri);

    if (!IsDefinedClass(a.env.host, NULL))
    {
        switch (a.env.state)
        {
        case cfvs_create:
        case cfvs_running:
            CfOut(cf_verbose, "",
                  " -> This host (\"%s\") is not the promised host for the environment (\"%s\"), so setting its intended state to \"down\"",
                  VFQNAME, a.env.host);
            a.env.state = cfvs_down;
            break;
        default:
            CfOut(cf_verbose, "",
                  " -> This is not the promised host for the environment, but it does not promise a run state, so take promise as valid");
        }
    }

    virInitialize();

    switch (VSYSTEMHARDCLASS)
    {
    case linuxx:

        switch (Str2Hypervisors(a.env.type))
        {
        case cfv_virt_xen:
        case cfv_virt_kvm:
        case cfv_virt_esx:
        case cfv_virt_vbox:
        case cfv_virt_test:
            VerifyVirtDomain(hyper_uri, envtype, a, pp);
            break;
        case cfv_virt_xen_net:
        case cfv_virt_kvm_net:
        case cfv_virt_esx_net:
        case cfv_virt_test_net:
            VerifyVirtNetwork(hyper_uri, envtype, a, pp);
            break;
        case cfv_ec2:
            break;
        case cfv_eucalyptus:
            break;
        default:
            break;
        }
        break;

    case darwin:
        switch (Str2Hypervisors(a.env.type))
        {
        case cfv_virt_vbox:
        case cfv_virt_test:
            VerifyVirtDomain(hyper_uri, envtype, a, pp);
            break;
        case cfv_virt_xen_net:
        case cfv_virt_kvm_net:
        case cfv_virt_esx_net:
        case cfv_virt_test_net:
            VerifyVirtNetwork(hyper_uri, envtype, a, pp);
            break;
        default:
            break;
        }
        break;

    case solaris:

        switch (Str2Hypervisors(a.env.type))
        {
        case cfv_zone:
            VerifyZone(a, pp);
            break;
        default:
            break;
        }

        break;

    default:
        CfOut(cf_verbose, "", " -> Unable to resolve an environment supervisor/monitor for this platform, aborting");
        break;
    }
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static void VerifyVirtDomain(char *uri, enum cfhypervisors envtype, Attributes a, Promise *pp)
{
    int num, i;

/* set up the library error handler */
    virSetErrorFunc(NULL, (void *) EnvironmentErrorHandler);

    if (CFVC[envtype] == NULL)
    {
        if ((CFVC[envtype] = virConnectOpenAuth(uri, virConnectAuthPtrDefault, 0)) == NULL)
        {
            CfOut(cf_error, "", " !! Failed to connect to virtualization monitor \"%s\"", uri);
            return;
        }
    }

    for (i = 0; i < CF_MAX_CONCURRENT_ENVIRONMENTS; i++)
    {
        CF_RUNNING[i] = -1;
        CF_SUSPENDED[i] = NULL;
    }

    num = virConnectListDomains(CFVC[envtype], CF_RUNNING, CF_MAX_CONCURRENT_ENVIRONMENTS);
    CfOut(cf_verbose, "", " -> Found %d running guest environments on this host (including enclosure)", num);
    ShowRunList(CFVC[envtype]);
    num = virConnectListDefinedDomains(CFVC[envtype], CF_SUSPENDED, CF_MAX_CONCURRENT_ENVIRONMENTS);
    CfOut(cf_verbose, "", " -> Found %d dormant guest environments on this host", num);
    ShowDormant(CFVC[envtype]);

    switch (a.env.state)
    {
    case cfvs_create:
        CreateVirtDom(CFVC[envtype], uri, a, pp);
        break;
    case cfvs_delete:
        DeleteVirt(CFVC[envtype], uri, a, pp);
        break;
    case cfvs_running:
        RunningVirt(CFVC[envtype], uri, a, pp);
        break;
    case cfvs_suspended:
        SuspendedVirt(CFVC[envtype], uri, a, pp);
        break;
    case cfvs_down:
        DownVirt(CFVC[envtype], uri, a, pp);
        break;
    default:
        CfOut(cf_inform, "", " !! No state specified for this environment");
        break;
    }
}

/*****************************************************************************/

static void VerifyVirtNetwork(char *uri, enum cfhypervisors envtype, Attributes a, Promise *pp)
{
    int num, i;
    char *networks[CF_MAX_CONCURRENT_ENVIRONMENTS];

    if (CFVC[envtype] == NULL)
    {
        if ((CFVC[envtype] = virConnectOpenAuth(uri, virConnectAuthPtrDefault, 0)) == NULL)
        {
            CfOut(cf_error, "", " !! Failed to connect to virtualization monitor \"%s\"", uri);
            return;
        }
    }

    for (i = 0; i < CF_MAX_CONCURRENT_ENVIRONMENTS; i++)
    {
        networks[i] = NULL;
    }

    num = virConnectListNetworks(CFVC[envtype], networks, CF_MAX_CONCURRENT_ENVIRONMENTS);

    CfOut(cf_verbose, "", " -> Detected %d active networks", num);

    switch (a.env.state)
    {
    case cfvs_create:
        CreateVirtNetwork(CFVC[envtype], networks, a, pp);
        break;

    case cfvs_delete:
        DeleteVirtNetwork(CFVC[envtype], networks, a, pp);
        break;

    default:
        CfOut(cf_inform, "", " !! No recogized state specified for this network environment");
        break;
    }
}

/*****************************************************************************/

static int VerifyZone(Attributes a, Promise *pp)
{
    return true;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static int CreateVirtDom(virConnectPtr vc, char *uri, Attributes a, Promise *pp)
{
    int alloc_file = false;
    char *xml_file;
    const char *name;
    char defaultxml[CF_MAXVARSIZE];
    virDomainPtr dom;
    int i;

    snprintf(defaultxml, CF_MAXVARSIZE - 1,
             "<domain type='test'>"
             "  <name>%s</name>"
             "  <memory>8388608</memory>"
             "  <currentMemory>2097152</currentMemory>"
             "  <vcpu>2</vcpu>" "  <os>" "    <type>hvm</type>" "  </os>" "</domain>", pp->promiser);

    for (i = 0; i < CF_MAX_CONCURRENT_ENVIRONMENTS; i++)
    {
        if (CF_RUNNING[i] > 0)
        {
            dom = virDomainLookupByID(vc, CF_RUNNING[i]);
            name = virDomainGetName(dom);

            if (name && strcmp(name, pp->promiser) == 0)
            {
                cfPS(cf_verbose, CF_NOP, "", pp, a, " -> Found a running environment called \"%s\" - promise kept\n",
                     name);
                return true;
            }

            virDomainFree(dom);
        }
    }

    for (i = 0; CF_SUSPENDED[i] != NULL; i++)
    {
        if (strcmp(CF_SUSPENDED[i], pp->promiser) == 0)
        {
            CfOut(cf_inform, "", " -> Found an existing, but suspended, environment id = %s, called \"%s\"\n",
                  CF_SUSPENDED[i], CF_SUSPENDED[i]);
        }
    }

    if(a.env.spec)
    {
        xml_file = xstrdup(a.env.spec);
        alloc_file = true;
    }
    else
    {
        CfOut(cf_verbose, "", "No spec file is promised, so reverting to default settings");
        xml_file = defaultxml;
    }

    if ((dom = virDomainCreateXML(vc, xml_file, 0)))
    {
        cfPS(cf_verbose, CF_CHG, "", pp, a, " -> Created a virtual domain \"%s\"\n", pp->promiser);

        if (a.env.cpus != CF_NOINT)
        {
            int maxcpus;

            if ((maxcpus = virConnectGetMaxVcpus(vc, virConnectGetType(vc))) == -1)
            {
                CfOut(cf_verbose, "", " !! Can't determine the available CPU resources");
            }
            else
            {
                if (a.env.cpus > maxcpus)
                {
                    CfOut(cf_inform, "",
                          " !! The promise to allocate %d CPUs in domain \"%s\" cannot be kept - only %d exist on the host",
                          a.env.cpus, pp->promiser, maxcpus);
                }
                else if (virDomainSetVcpus(dom, (unsigned int) a.env.cpus) == -1)
                {
                    CfOut(cf_inform, "", " -> Unable to adjust CPU count to %d", a.env.cpus);
                }
                else
                {
                    CfOut(cf_inform, "", " -> Verified that environment CPU count is now %d", a.env.cpus);
                }
            }
        }

        if (a.env.memory != CF_NOINT)
        {
            unsigned long maxmem;

            if ((maxmem = virDomainGetMaxMemory(dom)) == -1)
            {
                CfOut(cf_verbose, "", " !! Can't determine the available CPU resources");
            }
            else
            {
                if (virDomainSetMaxMemory(dom, (unsigned long) a.env.memory) == -1)
                {
                    CfOut(cf_inform, "", " !!! Unable to set the memory limit to %d", a.env.memory);
                }
                else
                {
                    CfOut(cf_inform, "", " -> Setting the memory limit to %d", a.env.memory);
                }

                if (virDomainSetMemory(dom, (unsigned long) a.env.memory) == -1)
                {
                    CfOut(cf_inform, "", " !!! Unable to set the current memory to %d", a.env.memory);
                }
            }
        }

        if (a.env.disk != CF_NOINT)
        {
            CfOut(cf_verbose, "", " -> Info: env_disk parameter is not currently supported on this platform");
        }

        virDomainFree(dom);
    }
    else
    {
        virErrorPtr vp;

        vp = virGetLastError();

        cfPS(cf_verbose, CF_FAIL, "", pp, a,
             " !! Failed to create a virtual domain \"%s\" - check spec for errors: %s", pp->promiser, vp->message);

        CfOut(cf_verbose, "", "Quoted spec file: %s", xml_file);
    }

    if (alloc_file)
    {
        free(xml_file);
    }

    return true;
}

/*****************************************************************************/

static int DeleteVirt(virConnectPtr vc, char *uri, Attributes a, Promise *pp)
{
    virDomainPtr dom;
    int ret = true;

    dom = virDomainLookupByName(vc, pp->promiser);

    if (dom)
    {
        if (virDomainDestroy(dom) == -1)
        {
            cfPS(cf_verbose, CF_FAIL, "", pp, a, " !! Failed to delete virtual domain \"%s\"\n", pp->promiser);
            ret = false;
        }
        else
        {
            cfPS(cf_verbose, CF_CHG, "", pp, a, " -> Deleted virtual domain \"%s\"\n", pp->promiser);
        }

        virDomainFree(dom);
    }
    else
    {
        cfPS(cf_verbose, CF_NOP, "", pp, a, " -> No such virtual domain called \"%s\" - promise kept\n", pp->promiser);
    }

    return ret;
}

/*****************************************************************************/

static int RunningVirt(virConnectPtr vc, char *uri, Attributes a, Promise *pp)
{
    virDomainPtr dom;
    virDomainInfo info;

    dom = virDomainLookupByName(vc, pp->promiser);

    if (dom)
    {
        if (virDomainGetInfo(dom, &info) == -1)
        {
            cfPS(cf_inform, CF_FAIL, "", pp, a, " !! Unable to probe virtual domain \"%s\"", pp->promiser);
            virDomainFree(dom);
            return false;
        }

        switch (info.state)
        {
        case VIR_DOMAIN_RUNNING:
            cfPS(cf_verbose, CF_NOP, "", pp, a, " -> Virtual domain \"%s\" running - promise kept\n", pp->promiser);
            break;

        case VIR_DOMAIN_BLOCKED:
            cfPS(cf_verbose, CF_NOP, "", pp, a,
                 " -> Virtual domain \"%s\" running but waiting for a resource - promise kept as far as possible\n",
                 pp->promiser);
            break;

        case VIR_DOMAIN_SHUTDOWN:
            cfPS(cf_verbose, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" is shutting down\n", pp->promiser);
            CfOut(cf_verbose, "",
                  " -> It is currently impossible to know whether it will reboot or not - deferring promise check until it has completed its shutdown");
            break;

        case VIR_DOMAIN_PAUSED:

            if (virDomainResume(dom) == -1)
            {
                cfPS(cf_verbose, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" failed to resume after suspension\n",
                     pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(cf_verbose, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" was suspended, resuming\n", pp->promiser);
            break;

        case VIR_DOMAIN_SHUTOFF:

            if (virDomainCreate(dom) == -1)
            {
                cfPS(cf_verbose, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" failed to resume after halting\n",
                     pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(cf_verbose, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" was inactive, booting...\n", pp->promiser);
            break;

        case VIR_DOMAIN_CRASHED:

            if (virDomainReboot(dom, 0) == -1)
            {
                cfPS(cf_verbose, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" has crashed and rebooting failed\n",
                     pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(cf_verbose, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" has crashed, rebooting...\n", pp->promiser);
            break;

        default:
            CfOut(cf_verbose, "", " !! Virtual domain \"%s\" is reported as having no state, whatever that means",
                  pp->promiser);
            break;
        }

        if (a.env.cpus > 0)
        {
            if (virDomainSetVcpus(dom, a.env.cpus) == -1)
            {
                CfOut(cf_inform, "", " !!! Unable to set the number of cpus to %d", a.env.cpus);
            }
            else
            {
                CfOut(cf_inform, "", " -> Setting the number of virtual cpus to %d", a.env.cpus);
            }
        }

        if (a.env.memory != CF_NOINT)
        {
            if (virDomainSetMaxMemory(dom, (unsigned long) a.env.memory) == -1)
            {
                CfOut(cf_inform, "", " !!! Unable to set the memory limit to %d", a.env.memory);
            }
            else
            {
                CfOut(cf_inform, "", " -> Setting the memory limit to %d", a.env.memory);
            }

            if (virDomainSetMemory(dom, (unsigned long) a.env.memory) == -1)
            {
                CfOut(cf_inform, "", " !!! Unable to set the current memory to %d", a.env.memory);
            }
        }

        if (a.env.disk != CF_NOINT)
        {
            CfOut(cf_verbose, "", " -> Info: env_disk parameter is not currently supported on this platform");
        }

        virDomainFree(dom);
    }
    else
    {
        CfOut(cf_verbose, "", " -> Virtual domain \"%s\" cannot be located, attempting to recreate", pp->promiser);
        CreateVirtDom(vc, uri, a, pp);
    }

    return true;
}

/*****************************************************************************/

static int SuspendedVirt(virConnectPtr vc, char *uri, Attributes a, Promise *pp)
{
    virDomainPtr dom;
    virDomainInfo info;

    dom = virDomainLookupByName(vc, pp->promiser);

    if (dom)
    {
        if (virDomainGetInfo(dom, &info) == -1)
        {
            cfPS(cf_inform, CF_FAIL, "", pp, a, " !! Unable to probe virtual domain \"%s\"", pp->promiser);
            virDomainFree(dom);
            return false;
        }

        switch (info.state)
        {
        case VIR_DOMAIN_BLOCKED:
        case VIR_DOMAIN_RUNNING:
            if (virDomainSuspend(dom) == -1)
            {
                cfPS(cf_verbose, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" failed to suspend!\n", pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(cf_verbose, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" running, suspending\n", pp->promiser);
            break;

        case VIR_DOMAIN_SHUTDOWN:
            cfPS(cf_verbose, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" is shutting down\n", pp->promiser);
            CfOut(cf_verbose, "",
                  " -> It is currently impossible to know whether it will reboot or not - deferring promise check until it has completed its shutdown");
            break;

        case VIR_DOMAIN_PAUSED:

            cfPS(cf_verbose, CF_NOP, "", pp, a, " -> Virtual domain \"%s\" is suspended - promise kept\n",
                 pp->promiser);
            break;

        case VIR_DOMAIN_SHUTOFF:

            cfPS(cf_verbose, CF_NOP, "", pp, a, " -> Virtual domain \"%s\" is down - promise kept\n", pp->promiser);
            break;

        case VIR_DOMAIN_CRASHED:

            if (virDomainSuspend(dom) == -1)
            {
                cfPS(cf_verbose, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" is crashed has failed to suspend!\n",
                     pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(cf_verbose, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" is in a crashed state, suspending\n",
                 pp->promiser);
            break;

        default:
            CfOut(cf_verbose, "", " !! Virtual domain \"%s\" is reported as having no state, whatever that means",
                  pp->promiser);
            break;
        }

        virDomainFree(dom);
    }
    else
    {
        cfPS(cf_verbose, CF_NOP, "", pp, a, " -> Virtual domain \"%s\" cannot be found - take promise as kept\n",
             pp->promiser);
    }

    return true;
}

/*****************************************************************************/

static int DownVirt(virConnectPtr vc, char *uri, Attributes a, Promise *pp)
{
    virDomainPtr dom;
    virDomainInfo info;

    dom = virDomainLookupByName(vc, pp->promiser);

    if (dom)
    {
        if (virDomainGetInfo(dom, &info) == -1)
        {
            cfPS(cf_inform, CF_FAIL, "", pp, a, " !! Unable to probe virtual domain \"%s\"", pp->promiser);
            virDomainFree(dom);
            return false;
        }

        switch (info.state)
        {
        case VIR_DOMAIN_BLOCKED:
        case VIR_DOMAIN_RUNNING:
            if (virDomainShutdown(dom) == -1)
            {
                cfPS(cf_verbose, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" failed to shutdown!\n",
                     pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(cf_verbose, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" running, terminating\n", pp->promiser);
            break;

        case VIR_DOMAIN_SHUTOFF:
        case VIR_DOMAIN_SHUTDOWN:
            cfPS(cf_verbose, CF_NOP, "", pp, a, " -> Virtual domain \"%s\" is down - promise kept\n", pp->promiser);
            break;

        case VIR_DOMAIN_PAUSED:
            cfPS(cf_verbose, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" is suspended - ignoring promise\n",
                 pp->promiser);
            break;

        case VIR_DOMAIN_CRASHED:

            if (virDomainSuspend(dom) == -1)
            {
                cfPS(cf_verbose, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" is crashed and failed to shutdown\n",
                     pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(cf_verbose, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" is in a crashed state, terminating\n",
                 pp->promiser);
            break;

        default:
            CfOut(cf_verbose, "", " !! Virtual domain \"%s\" is reported as having no state, whatever that means",
                  pp->promiser);
            break;
        }

        virDomainFree(dom);
    }
    else
    {
        cfPS(cf_verbose, CF_NOP, "", pp, a, " -> Virtual domain \"%s\" cannot be found - take promise as kept\n",
             pp->promiser);
    }

    return true;
}

/*****************************************************************************/

static int CreateVirtNetwork(virConnectPtr vc, char **networks, Attributes a, Promise *pp)
{
    virNetworkPtr network;
    char *xml_file;
    char defaultxml[CF_MAXVARSIZE];
    int i, found = false;

    snprintf(defaultxml, CF_MAXVARSIZE - 1,
             "<network>"
             "<name>%s</name>"
             "<bridge name=\"virbr0\" />"
             "<forward mode=\"nat\"/>"
             "<ip address=\"192.168.122.1\" netmask=\"255.255.255.0\">"
             "<dhcp>"
             "<range start=\"192.168.122.2\" end=\"192.168.122.254\" />" "</dhcp>" "</ip>" "</network>", pp->promiser);

    for (i = 0; networks[i] != NULL; i++)
    {
        CfOut(cf_verbose, "", " -> Discovered a running network \"%s\"", networks[i]);

        if (strcmp(networks[i], pp->promiser) == 0)
        {
            found = true;
        }
    }

    if (found)
    {
        cfPS(cf_verbose, CF_NOP, "", pp, a, " -> Network \"%s\" exists - promise kept\n", pp->promiser);
        return true;
    }

    if (a.env.spec)
    {
        xml_file = xstrdup(a.env.spec);
    }
    else
    {
        xml_file = xstrdup(defaultxml);
    }

    if ((network = virNetworkCreateXML(vc, xml_file)) == NULL)
    {
        cfPS(cf_error, CF_FAIL, "", pp, a, " !! Unable to create network \"%s\"\n", pp->promiser);
        free(xml_file);
        return false;
    }
    else
    {
        cfPS(cf_inform, CF_CHG, "", pp, a, " -> Created network \"%s\" - promise repaired\n", pp->promiser);
    }

    free(xml_file);

    virNetworkFree(network);
    return true;
}

/*****************************************************************************/

static int DeleteVirtNetwork(virConnectPtr vc, char **networks, Attributes a, Promise *pp)
{
    virNetworkPtr network;
    int ret = true;

    if ((network = virNetworkLookupByName(vc, pp->promiser)) == NULL)
    {
        cfPS(cf_verbose, CF_NOP, "", pp, a, " -> Couldn't find a network called \"%s\" - promise assumed kept\n",
             pp->promiser);
        return true;
    }

    if (virNetworkDestroy(network) == 0)
    {
        cfPS(cf_inform, CF_CHG, "", pp, a, " -> Deleted network \"%s\" - promise repaired\n", pp->promiser);
    }
    else
    {
        cfPS(cf_error, CF_FAIL, "", pp, a, " !! Network deletion of \"%s\" failed\n", pp->promiser);
        ret = false;
    }

    virNetworkFree(network);
    return ret;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static void EnvironmentErrorHandler()
{

}

/*****************************************************************************/

static void ShowRunList(virConnectPtr vc)
{
    int i;
    virDomainPtr dom;
    const char *name;

    for (i = 0; i < CF_MAX_CONCURRENT_ENVIRONMENTS; i++)
    {
        if (CF_RUNNING[i] > 0)
        {
            if ((dom = virDomainLookupByID(vc, CF_RUNNING[i])))
            {
                CfOut(cf_verbose, "", " -> Found a running virtual domain with id %d\n", CF_RUNNING[i]);
            }

            if ((name = virDomainGetName(dom)))
            {
                CfOut(cf_verbose, "", " ---> Found a running virtual domain called \"%s\"\n", name);
            }

            virDomainFree(dom);
        }
    }
}

/*****************************************************************************/

static void ShowDormant(virConnectPtr vc)
{
    int i;

    for (i = 0; CF_SUSPENDED[i] != NULL; i++)
    {
        CfOut(cf_verbose, "", " ---> Found a suspended, domain environment called \"%s\"\n", CF_SUSPENDED[i]);
    }
}

#endif
