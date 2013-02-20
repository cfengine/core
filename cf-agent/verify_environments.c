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
#include "transaction.h"
#include "logging.h"
#include "policy.h"

#ifdef HAVE_LIBVIRT
/*****************************************************************************/

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

/*****************************************************************************/

enum cfhypervisors
{
    cfv_virt_xen,
    cfv_virt_kvm,
    cfv_virt_esx,
    cfv_virt_vbox,
    cfv_virt_test,
    cfv_virt_xen_net,
    cfv_virt_kvm_net,
    cfv_virt_esx_net,
    cfv_virt_test_net,
    cfv_zone,
    cfv_ec2,
    cfv_eucalyptus,
    cfv_none
};

/*****************************************************************************/

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
static enum cfhypervisors Str2Hypervisors(char *s);

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
        NewScalar("this", "promiser", pp->promiser, DATA_TYPE_STRING);

        pexp = ExpandDeRefPromise("this", pp);
        VerifyEnvironments(a, pp);
        PromiseDestroy(pexp);
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
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Conflicting promise of both a spec and cpu/memory/disk resources");
            return false;
        }
    }

    if (a.env.host == NULL)
    {
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! No environment_host defined for environment promise");
        PromiseRef(OUTPUT_LEVEL_ERROR, pp);
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
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Network environment promises computational resources (%d,%d,%d,%s)", a.env.cpus,
                  a.env.memory, a.env.disk, a.env.name);
            PromiseRef(OUTPUT_LEVEL_ERROR, pp);
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
        CfOut(OUTPUT_LEVEL_ERROR, "", " !! Environment type \"%s\" not currently supported", a.env.type);
        return;
        break;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Selecting environment type \"%s\" -> \"%s\"", a.env.type, hyper_uri);

    if (!IsDefinedClass(a.env.host, NULL))
    {
        switch (a.env.state)
        {
        case ENVIRONMENT_STATE_CREATE:
        case ENVIRONMENT_STATE_RUNNING:
            CfOut(OUTPUT_LEVEL_VERBOSE, "",
                  " -> This host (\"%s\") is not the promised host for the environment (\"%s\"), so setting its intended state to \"down\"",
                  VFQNAME, a.env.host);
            a.env.state = ENVIRONMENT_STATE_DOWN;
            break;
        default:
            CfOut(OUTPUT_LEVEL_VERBOSE, "",
                  " -> This is not the promised host for the environment, but it does not promise a run state, so take promise as valid");
        }
    }

    virInitialize();

#if defined(__linux__)
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
#elif defined(__APPLE__)
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
#elif defined(__sun)
    switch (Str2Hypervisors(a.env.type))
    {
    case cfv_zone:
        VerifyZone(a, pp);
        break;
    default:
        break;
    }
#else
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Unable to resolve an environment supervisor/monitor for this platform, aborting");
#endif
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
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Failed to connect to virtualization monitor \"%s\"", uri);
            return;
        }
    }

    for (i = 0; i < CF_MAX_CONCURRENT_ENVIRONMENTS; i++)
    {
        CF_RUNNING[i] = -1;
        CF_SUSPENDED[i] = NULL;
    }

    num = virConnectListDomains(CFVC[envtype], CF_RUNNING, CF_MAX_CONCURRENT_ENVIRONMENTS);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Found %d running guest environments on this host (including enclosure)", num);
    ShowRunList(CFVC[envtype]);
    num = virConnectListDefinedDomains(CFVC[envtype], CF_SUSPENDED, CF_MAX_CONCURRENT_ENVIRONMENTS);
    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Found %d dormant guest environments on this host", num);
    ShowDormant(CFVC[envtype]);

    switch (a.env.state)
    {
    case ENVIRONMENT_STATE_CREATE:
        CreateVirtDom(CFVC[envtype], uri, a, pp);
        break;
    case ENVIRONMENT_STATE_DELETE:
        DeleteVirt(CFVC[envtype], uri, a, pp);
        break;
    case ENVIRONMENT_STATE_RUNNING:
        RunningVirt(CFVC[envtype], uri, a, pp);
        break;
    case ENVIRONMENT_STATE_SUSPENDED:
        SuspendedVirt(CFVC[envtype], uri, a, pp);
        break;
    case ENVIRONMENT_STATE_DOWN:
        DownVirt(CFVC[envtype], uri, a, pp);
        break;
    default:
        CfOut(OUTPUT_LEVEL_INFORM, "", " !! No state specified for this environment");
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
            CfOut(OUTPUT_LEVEL_ERROR, "", " !! Failed to connect to virtualization monitor \"%s\"", uri);
            return;
        }
    }

    for (i = 0; i < CF_MAX_CONCURRENT_ENVIRONMENTS; i++)
    {
        networks[i] = NULL;
    }

    num = virConnectListNetworks(CFVC[envtype], networks, CF_MAX_CONCURRENT_ENVIRONMENTS);

    CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Detected %d active networks", num);

    switch (a.env.state)
    {
    case ENVIRONMENT_STATE_CREATE:
        CreateVirtNetwork(CFVC[envtype], networks, a, pp);
        break;

    case ENVIRONMENT_STATE_DELETE:
        DeleteVirtNetwork(CFVC[envtype], networks, a, pp);
        break;

    default:
        CfOut(OUTPUT_LEVEL_INFORM, "", " !! No recogized state specified for this network environment");
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
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Found a running environment called \"%s\" - promise kept\n",
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
            CfOut(OUTPUT_LEVEL_INFORM, "", " -> Found an existing, but suspended, environment id = %s, called \"%s\"\n",
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
        CfOut(OUTPUT_LEVEL_VERBOSE, "", "No spec file is promised, so reverting to default settings");
        xml_file = defaultxml;
    }

    if ((dom = virDomainCreateXML(vc, xml_file, 0)))
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Created a virtual domain \"%s\"\n", pp->promiser);

        if (a.env.cpus != CF_NOINT)
        {
            int maxcpus;

            if ((maxcpus = virConnectGetMaxVcpus(vc, virConnectGetType(vc))) == -1)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Can't determine the available CPU resources");
            }
            else
            {
                if (a.env.cpus > maxcpus)
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "",
                          " !! The promise to allocate %d CPUs in domain \"%s\" cannot be kept - only %d exist on the host",
                          a.env.cpus, pp->promiser, maxcpus);
                }
                else if (virDomainSetVcpus(dom, (unsigned int) a.env.cpus) == -1)
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "", " -> Unable to adjust CPU count to %d", a.env.cpus);
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "", " -> Verified that environment CPU count is now %d", a.env.cpus);
                }
            }
        }

        if (a.env.memory != CF_NOINT)
        {
            unsigned long maxmem;

            if ((maxmem = virDomainGetMaxMemory(dom)) == -1)
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Can't determine the available CPU resources");
            }
            else
            {
                if (virDomainSetMaxMemory(dom, (unsigned long) a.env.memory) == -1)
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "", " !!! Unable to set the memory limit to %d", a.env.memory);
                }
                else
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "", " -> Setting the memory limit to %d", a.env.memory);
                }

                if (virDomainSetMemory(dom, (unsigned long) a.env.memory) == -1)
                {
                    CfOut(OUTPUT_LEVEL_INFORM, "", " !!! Unable to set the current memory to %d", a.env.memory);
                }
            }
        }

        if (a.env.disk != CF_NOINT)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Info: env_disk parameter is not currently supported on this platform");
        }

        virDomainFree(dom);
    }
    else
    {
        virErrorPtr vp;

        vp = virGetLastError();

        cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a,
             " !! Failed to create a virtual domain \"%s\" - check spec for errors: %s", pp->promiser, vp->message);

        CfOut(OUTPUT_LEVEL_VERBOSE, "", "Quoted spec file: %s", xml_file);
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
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_FAIL, "", pp, a, " !! Failed to delete virtual domain \"%s\"\n", pp->promiser);
            ret = false;
        }
        else
        {
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Deleted virtual domain \"%s\"\n", pp->promiser);
        }

        virDomainFree(dom);
    }
    else
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> No such virtual domain called \"%s\" - promise kept\n", pp->promiser);
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
            cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a, " !! Unable to probe virtual domain \"%s\"", pp->promiser);
            virDomainFree(dom);
            return false;
        }

        switch (info.state)
        {
        case VIR_DOMAIN_RUNNING:
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Virtual domain \"%s\" running - promise kept\n", pp->promiser);
            break;

        case VIR_DOMAIN_BLOCKED:
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a,
                 " -> Virtual domain \"%s\" running but waiting for a resource - promise kept as far as possible\n",
                 pp->promiser);
            break;

        case VIR_DOMAIN_SHUTDOWN:
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" is shutting down\n", pp->promiser);
            CfOut(OUTPUT_LEVEL_VERBOSE, "",
                  " -> It is currently impossible to know whether it will reboot or not - deferring promise check until it has completed its shutdown");
            break;

        case VIR_DOMAIN_PAUSED:

            if (virDomainResume(dom) == -1)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" failed to resume after suspension\n",
                     pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" was suspended, resuming\n", pp->promiser);
            break;

        case VIR_DOMAIN_SHUTOFF:

            if (virDomainCreate(dom) == -1)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" failed to resume after halting\n",
                     pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" was inactive, booting...\n", pp->promiser);
            break;

        case VIR_DOMAIN_CRASHED:

            if (virDomainReboot(dom, 0) == -1)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" has crashed and rebooting failed\n",
                     pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" has crashed, rebooting...\n", pp->promiser);
            break;

        default:
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Virtual domain \"%s\" is reported as having no state, whatever that means",
                  pp->promiser);
            break;
        }

        if (a.env.cpus > 0)
        {
            if (virDomainSetVcpus(dom, a.env.cpus) == -1)
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", " !!! Unable to set the number of cpus to %d", a.env.cpus);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", " -> Setting the number of virtual cpus to %d", a.env.cpus);
            }
        }

        if (a.env.memory != CF_NOINT)
        {
            if (virDomainSetMaxMemory(dom, (unsigned long) a.env.memory) == -1)
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", " !!! Unable to set the memory limit to %d", a.env.memory);
            }
            else
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", " -> Setting the memory limit to %d", a.env.memory);
            }

            if (virDomainSetMemory(dom, (unsigned long) a.env.memory) == -1)
            {
                CfOut(OUTPUT_LEVEL_INFORM, "", " !!! Unable to set the current memory to %d", a.env.memory);
            }
        }

        if (a.env.disk != CF_NOINT)
        {
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Info: env_disk parameter is not currently supported on this platform");
        }

        virDomainFree(dom);
    }
    else
    {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Virtual domain \"%s\" cannot be located, attempting to recreate", pp->promiser);
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
            cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a, " !! Unable to probe virtual domain \"%s\"", pp->promiser);
            virDomainFree(dom);
            return false;
        }

        switch (info.state)
        {
        case VIR_DOMAIN_BLOCKED:
        case VIR_DOMAIN_RUNNING:
            if (virDomainSuspend(dom) == -1)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" failed to suspend!\n", pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" running, suspending\n", pp->promiser);
            break;

        case VIR_DOMAIN_SHUTDOWN:
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" is shutting down\n", pp->promiser);
            CfOut(OUTPUT_LEVEL_VERBOSE, "",
                  " -> It is currently impossible to know whether it will reboot or not - deferring promise check until it has completed its shutdown");
            break;

        case VIR_DOMAIN_PAUSED:

            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Virtual domain \"%s\" is suspended - promise kept\n",
                 pp->promiser);
            break;

        case VIR_DOMAIN_SHUTOFF:

            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Virtual domain \"%s\" is down - promise kept\n", pp->promiser);
            break;

        case VIR_DOMAIN_CRASHED:

            if (virDomainSuspend(dom) == -1)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" is crashed has failed to suspend!\n",
                     pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" is in a crashed state, suspending\n",
                 pp->promiser);
            break;

        default:
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Virtual domain \"%s\" is reported as having no state, whatever that means",
                  pp->promiser);
            break;
        }

        virDomainFree(dom);
    }
    else
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Virtual domain \"%s\" cannot be found - take promise as kept\n",
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
            cfPS(OUTPUT_LEVEL_INFORM, CF_FAIL, "", pp, a, " !! Unable to probe virtual domain \"%s\"", pp->promiser);
            virDomainFree(dom);
            return false;
        }

        switch (info.state)
        {
        case VIR_DOMAIN_BLOCKED:
        case VIR_DOMAIN_RUNNING:
            if (virDomainShutdown(dom) == -1)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" failed to shutdown!\n",
                     pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" running, terminating\n", pp->promiser);
            break;

        case VIR_DOMAIN_SHUTOFF:
        case VIR_DOMAIN_SHUTDOWN:
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Virtual domain \"%s\" is down - promise kept\n", pp->promiser);
            break;

        case VIR_DOMAIN_PAUSED:
            cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" is suspended - ignoring promise\n",
                 pp->promiser);
            break;

        case VIR_DOMAIN_CRASHED:

            if (virDomainSuspend(dom) == -1)
            {
                cfPS(OUTPUT_LEVEL_VERBOSE, CF_INTERPT, "", pp, a, " -> Virtual domain \"%s\" is crashed and failed to shutdown\n",
                     pp->promiser);
                virDomainFree(dom);
                return false;
            }

            cfPS(OUTPUT_LEVEL_VERBOSE, CF_CHG, "", pp, a, " -> Virtual domain \"%s\" is in a crashed state, terminating\n",
                 pp->promiser);
            break;

        default:
            CfOut(OUTPUT_LEVEL_VERBOSE, "", " !! Virtual domain \"%s\" is reported as having no state, whatever that means",
                  pp->promiser);
            break;
        }

        virDomainFree(dom);
    }
    else
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Virtual domain \"%s\" cannot be found - take promise as kept\n",
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
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Discovered a running network \"%s\"", networks[i]);

        if (strcmp(networks[i], pp->promiser) == 0)
        {
            found = true;
        }
    }

    if (found)
    {
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Network \"%s\" exists - promise kept\n", pp->promiser);
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
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, " !! Unable to create network \"%s\"\n", pp->promiser);
        free(xml_file);
        return false;
    }
    else
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, " -> Created network \"%s\" - promise repaired\n", pp->promiser);
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
        cfPS(OUTPUT_LEVEL_VERBOSE, CF_NOP, "", pp, a, " -> Couldn't find a network called \"%s\" - promise assumed kept\n",
             pp->promiser);
        return true;
    }

    if (virNetworkDestroy(network) == 0)
    {
        cfPS(OUTPUT_LEVEL_INFORM, CF_CHG, "", pp, a, " -> Deleted network \"%s\" - promise repaired\n", pp->promiser);
    }
    else
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "", pp, a, " !! Network deletion of \"%s\" failed\n", pp->promiser);
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
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " -> Found a running virtual domain with id %d\n", CF_RUNNING[i]);
            }

            if ((name = virDomainGetName(dom)))
            {
                CfOut(OUTPUT_LEVEL_VERBOSE, "", " ---> Found a running virtual domain called \"%s\"\n", name);
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
        CfOut(OUTPUT_LEVEL_VERBOSE, "", " ---> Found a suspended, domain environment called \"%s\"\n", CF_SUSPENDED[i]);
    }
}

/*****************************************************************************/

static enum cfhypervisors Str2Hypervisors(char *s)
{
    static char *names[] = { "xen", "kvm", "esx", "vbox", "test",
        "xen_net", "kvm_net", "esx_net", "test_net",
        "zone", "ec2", "eucalyptus", NULL
    };
    int i;

    if (s == NULL)
    {
        return cfv_virt_test;
    }

    for (i = 0; names[i] != NULL; i++)
    {
        if (s && (strcmp(s, names[i]) == 0))
        {
            return (enum cfhypervisors) i;
        }
    }

    return (enum cfhypervisors) i;
}
/*****************************************************************************/
#else

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
