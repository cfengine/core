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

#include <cf3.defs.h>
#include <files_lib.h>
#include <files_interfaces.h>
#include <item_lib.h>
#include <actuator.h>
#include <eval_context.h>
#include <promises.h>
#include <vars.h>
#include <conversion.h>
#include <attributes.h>
#include <locks.h>
#include <policy.h>
#include <scope.h>
#include <ornaments.h>
#include <pipes.h>
#include <verify_environments.h>
#include <string_lib.h>
#include <regex.h>
#include <../libenv/sysinfo.h>

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
    cfv_virt_lxc,
    cfv_virt_test,
    cfv_virt_xen_net,
    cfv_virt_kvm_net,
    cfv_virt_esx_net,
    cfv_virt_test_net,
    cfv_zone,
    cfv_docker,
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

static int EnvironmentsSanityChecks(Attributes a, const Promise *pp);
static PromiseResult VerifyEnvironments(EvalContext *ctx, Attributes a, const Promise *pp);
static PromiseResult VerifyVirtDomain(EvalContext *ctx, char *uri, enum cfhypervisors envtype, Attributes a, const Promise *pp);
static PromiseResult VerifyVirtNetwork(EvalContext *ctx, char *uri, enum cfhypervisors envtype, Attributes a, const Promise *pp);
static PromiseResult VerifyDockerContainer(EvalContext *ctx, Attributes a, const Promise *pp);
static PromiseResult CreateVirtDom(EvalContext *ctx, virConnectPtr vc, Attributes a, const Promise *pp);
static PromiseResult DeleteVirt(EvalContext *ctx, virConnectPtr vc, Attributes a, const Promise *pp);
static PromiseResult RunningVirt(EvalContext *ctx, virConnectPtr vc, Attributes a, const Promise *pp);
static PromiseResult SuspendedVirt(EvalContext *ctx, virConnectPtr vc, Attributes a, const Promise *pp);
static PromiseResult DownVirt(EvalContext *ctx, virConnectPtr vc, Attributes a, const Promise *pp);
static void EnvironmentErrorHandler(void);
static void ShowRunList(virConnectPtr vc);
static void ShowDormant(void);
static PromiseResult CreateVirtNetwork(EvalContext *ctx, virConnectPtr vc, char **networks, Attributes a, const Promise *pp);
static PromiseResult DeleteVirtNetwork(EvalContext *ctx, virConnectPtr vc, Attributes a, const Promise *pp);
static enum cfhypervisors Str2Hypervisors(char *s);
static void VerifyDockerContainerRunning(EvalContext *ctx, Attributes a, const Promise *pp, DockerPS *containers, PromiseResult *result);
static void VerifyDockerContainerNotRunning(EvalContext *ctx, Attributes a, const Promise *pp, DockerPS *containers, PromiseResult *result);

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

PromiseResult VerifyEnvironmentsPromise(EvalContext *ctx, const Promise *pp)
{
    CfLock thislock;

    Attributes a = GetEnvironmentsAttributes(ctx, pp);

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (EnvironmentsSanityChecks(a, pp))
    {
        thislock = AcquireLock(ctx, "virtual", VUQNAME, CFSTARTTIME, a.transaction, pp, false);

        if (thislock.lock == NULL)
        {
            return PROMISE_RESULT_NOOP;
        }

        PromiseBanner(ctx, pp);

        bool excluded = false;
        Promise *pexp = ExpandDeRefPromise(ctx, pp, &excluded);
        if (excluded)
        {
            result = PROMISE_RESULT_SKIPPED;
        }
        else
        {
            result = VerifyEnvironments(ctx, a, pp);
        }

        PromiseDestroy(pexp);
    }

    YieldCurrentLock(thislock);

    return result;
}

/*****************************************************************************/

static int EnvironmentsSanityChecks(Attributes a, const Promise *pp)
{
    for (char *sp = pp->promiser; *sp != '\0'; sp++)
    {
        switch (*sp)
        {
        case ' ':
        case '&':
        case '"':
        case '#':
        case '/':
        case '\\':
        case '`':
            Log(LOG_LEVEL_ERR, "Avoid using character '%c' in host/container names", *sp);
            PromiseRef(LOG_LEVEL_ERR, pp);
            return false;
        default:
            break;
        }
    }

    if (a.env.spec)
    {
        if (a.env.cpus != CF_NOINT || a.env.memory != CF_NOINT || a.env.disk != CF_NOINT)
        {
            Log(LOG_LEVEL_ERR, "Conflicting promise of both a spec and cpu/memory/disk resources");
            return false;
        }
    }

    switch (Str2Hypervisors(a.env.type))
    {
    case cfv_virt_xen_net:
    case cfv_virt_kvm_net:
    case cfv_virt_esx_net:
    case cfv_virt_test_net:
        if (a.env.cpus != CF_NOINT || a.env.memory != CF_NOINT || a.env.disk != CF_NOINT || a.env.addresses)
        {
            Log(LOG_LEVEL_ERR, "Network environment promises computational resources (%d,%d,%d,%s)", a.env.cpus, a.env.memory, a.env.disk, pp->promiser);
            PromiseRef(LOG_LEVEL_ERR, pp);
        }
        break;
    default:
        break;
    }

    return true;
}

/*****************************************************************************/

static PromiseResult VerifyEnvironments(EvalContext *ctx, Attributes a, const Promise *pp)
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

    case cfv_virt_lxc:
        snprintf(hyper_uri, CF_MAXVARSIZE - 1, "lxc:///");
        envtype = cfv_virt_lxc;
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
        envtype = cfv_zone;
        break;

    case cfv_docker:
        envtype = cfv_docker;
        break;

    default:
        Log(LOG_LEVEL_ERR, "Environment type '%s' not currently supported", a.env.type);
        return PROMISE_RESULT_NOOP;
        break;
    }

    Log(LOG_LEVEL_VERBOSE, "Selecting environment type '%s' '%s'", a.env.type, hyper_uri);

    virInitialize();

    PromiseResult result = PROMISE_RESULT_NOOP;
#if defined(__linux__)
    switch (Str2Hypervisors(a.env.type))
    {
    case cfv_virt_xen:
    case cfv_virt_kvm:
    case cfv_virt_esx:
    case cfv_virt_vbox:
    case cfv_virt_lxc:
    case cfv_virt_test:
        result = PromiseResultUpdate(result, VerifyVirtDomain(ctx, hyper_uri, envtype, a, pp));
        break;
    case cfv_virt_xen_net:
    case cfv_virt_kvm_net:
    case cfv_virt_esx_net:
    case cfv_virt_test_net:
        result = PromiseResultUpdate(result, VerifyVirtNetwork(ctx, hyper_uri, envtype, a, pp));
        break;
    case cfv_ec2:
        break;
    case cfv_eucalyptus:
        break;
    case cfv_docker:
        result = PromiseResultUpdate(result, VerifyDockerContainer(ctx, a, pp));
        break;
    default:
        break;
    }
#elif defined(__APPLE__)
    switch (Str2Hypervisors(a.env.type))
    {
    case cfv_virt_vbox:
    case cfv_virt_test:
        result = PromiseResultUpdate(result, VerifyVirtDomain(ctx, hyper_uri, envtype, a, pp));
        break;
    case cfv_virt_xen_net:
    case cfv_virt_kvm_net:
    case cfv_virt_esx_net:
    case cfv_virt_test_net:
        result = PromiseResultUpdate(result, VerifyVirtNetwork(ctx, hyper_uri, envtype, a, pp));
        break;
    default:
        break;
    }
#elif defined(__sun)
    switch (Str2Hypervisors(a.env.type))
    {
    case cfv_zone:
        result = PromiseResultUpdate(result, VerifyZone(a, pp));
        break;
    default:
        break;
    }
#else
    Log(LOG_LEVEL_VERBOSE, "Unable to resolve an environment supervisor/monitor for this platform, aborting");
#endif
    return result;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static PromiseResult VerifyVirtDomain(EvalContext *ctx, char *uri, enum cfhypervisors envtype, Attributes a, const Promise *pp)
{
    int num, i;

/* set up the library error handler */
    virSetErrorFunc(NULL, (void *) EnvironmentErrorHandler);

    if (CFVC[envtype] == NULL)
    {
        if ((CFVC[envtype] = virConnectOpenAuth(uri, virConnectAuthPtrDefault, 0)) == NULL)
        {
            Log(LOG_LEVEL_ERR, "Failed to connect to virtualization monitor '%s'", uri);
            return PROMISE_RESULT_NOOP;
        }
    }

    for (i = 0; i < CF_MAX_CONCURRENT_ENVIRONMENTS; i++)
    {
        CF_RUNNING[i] = -1;
        CF_SUSPENDED[i] = NULL;
    }

    num = virConnectListDomains(CFVC[envtype], CF_RUNNING, CF_MAX_CONCURRENT_ENVIRONMENTS);
    Log(LOG_LEVEL_VERBOSE, "Found %d running guest environments on this host (including enclosure)", num);
    ShowRunList(CFVC[envtype]);
    num = virConnectListDefinedDomains(CFVC[envtype], CF_SUSPENDED, CF_MAX_CONCURRENT_ENVIRONMENTS);
    Log(LOG_LEVEL_VERBOSE, "Found %d dormant guest environments on this host", num);
    ShowDormant();

    PromiseResult result = PROMISE_RESULT_NOOP;
    switch (a.env.state)
    {
    case ENVIRONMENT_STATE_CREATE:
        result = PromiseResultUpdate(result, CreateVirtDom(ctx, CFVC[envtype], a, pp));
        break;
    case ENVIRONMENT_STATE_DELETE:
        result = PromiseResultUpdate(result, DeleteVirt(ctx, CFVC[envtype], a, pp));
        break;
    case ENVIRONMENT_STATE_RUNNING:
        result = PromiseResultUpdate(result, RunningVirt(ctx, CFVC[envtype], a, pp));
        break;
    case ENVIRONMENT_STATE_SUSPENDED:
        result = PromiseResultUpdate(result, SuspendedVirt(ctx, CFVC[envtype], a, pp));
        break;
    case ENVIRONMENT_STATE_DOWN:
        result = PromiseResultUpdate(result, DownVirt(ctx, CFVC[envtype], a, pp));
        break;
    default:
        Log(LOG_LEVEL_INFO, "No state specified for this environment");
        break;
    }

    return result;
}

/*****************************************************************************/

static PromiseResult VerifyVirtNetwork(EvalContext *ctx, char *uri, enum cfhypervisors envtype, Attributes a, const Promise *pp)
{
    int num, i;
    char *networks[CF_MAX_CONCURRENT_ENVIRONMENTS];

    if (CFVC[envtype] == NULL)
    {
        if ((CFVC[envtype] = virConnectOpenAuth(uri, virConnectAuthPtrDefault, 0)) == NULL)
        {
            Log(LOG_LEVEL_ERR, "Failed to connect to virtualization monitor '%s'", uri);
            return PROMISE_RESULT_NOOP;
        }
    }

    for (i = 0; i < CF_MAX_CONCURRENT_ENVIRONMENTS; i++)
    {
        networks[i] = NULL;
    }

    num = virConnectListNetworks(CFVC[envtype], networks, CF_MAX_CONCURRENT_ENVIRONMENTS);

    Log(LOG_LEVEL_VERBOSE, "Detected %d active networks", num);

    PromiseResult result = PROMISE_RESULT_NOOP;
    switch (a.env.state)
    {
    case ENVIRONMENT_STATE_CREATE:
        result = PromiseResultUpdate(result, CreateVirtNetwork(ctx, CFVC[envtype], networks, a, pp));
        break;

    case ENVIRONMENT_STATE_DELETE:
        result = PromiseResultUpdate(result, DeleteVirtNetwork(ctx, CFVC[envtype], a, pp));
        break;

    default:
        Log(LOG_LEVEL_INFO, "No recogized state specified for this network environment");
        break;
    }

    return result;
}

/*****************************************************************************/

static PromiseResult VerifyDockerContainer(EvalContext *ctx, Attributes a, const Promise *pp)
{
    PromiseResult result = PROMISE_RESULT_NOOP;
    struct stat sb;

    if (stat(DOCKER_COMMAND, &sb) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "No docker installation found: %s", DOCKER_COMMAND);
        return PROMISE_RESULT_FAIL;
    }

    DockerPS *active = QueryDockerProcessTable(&active);

    if (!active)
    {
        Log(LOG_LEVEL_VERBOSE, "Docker does not seem to be running");
        return PROMISE_RESULT_FAIL;
    }

    switch (a.env.state)
    {
    case ENVIRONMENT_STATE_RUNNING:
    case ENVIRONMENT_STATE_CREATE:
        VerifyDockerContainerRunning(ctx, a, pp, active, &result);
        break;

    case ENVIRONMENT_STATE_SUSPENDED:
    case ENVIRONMENT_STATE_DOWN:
    case ENVIRONMENT_STATE_DELETE:
        VerifyDockerContainerNotRunning(ctx, a, pp, active, &result);
        break;

    default:
        Log(LOG_LEVEL_INFO, "No recogized state specified for this network environment");
        break;
    }

    DeleteDockerPS(active);
    return result;
}

/*****************************************************************************/
/* Level                                                                     */
/*****************************************************************************/

static PromiseResult CreateVirtDom(EvalContext *ctx, virConnectPtr vc, Attributes a, const Promise *pp)
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
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Found a running environment called '%s' - promise kept", name);
                return PROMISE_RESULT_NOOP;
            }

            virDomainFree(dom);
        }
    }

    for (i = 0; CF_SUSPENDED[i] != NULL; i++)
    {
        if (strcmp(CF_SUSPENDED[i], pp->promiser) == 0)
        {
            Log(LOG_LEVEL_INFO, "Found an existing, but suspended, environment id = %s, called '%s'",
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
        Log(LOG_LEVEL_VERBOSE, "No spec file is promised, so reverting to default settings");
        xml_file = defaultxml;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if ((dom = virDomainCreateXML(vc, xml_file, 0)))
    {
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Created a virtual domain '%s'", pp->promiser);
        result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);

        if (a.env.cpus != CF_NOINT)
        {
            int maxcpus;

            if ((maxcpus = virConnectGetMaxVcpus(vc, virConnectGetType(vc))) == -1)
            {
                Log(LOG_LEVEL_VERBOSE, "Can't determine the available CPU resources");
            }
            else
            {
                if (a.env.cpus > maxcpus)
                {
                    Log(LOG_LEVEL_INFO,
                        "The promise to allocate %d CPUs in domain '%s' cannot be kept - only %d exist on the host",
                        a.env.cpus, pp->promiser, maxcpus);
                }
                else if (virDomainSetVcpus(dom, (unsigned int) a.env.cpus) == -1)
                {
                    Log(LOG_LEVEL_INFO, "Unable to adjust CPU count to %d", a.env.cpus);
                }
                else
                {
                    Log(LOG_LEVEL_INFO, "Verified that environment CPU count is now %d", a.env.cpus);
                }
            }
        }

        if (a.env.memory != CF_NOINT)
        {
            unsigned long maxmem;

            if ((maxmem = virDomainGetMaxMemory(dom)) == -1)
            {
                Log(LOG_LEVEL_VERBOSE, "Can't determine the available CPU resources");
            }
            else
            {
                if (virDomainSetMaxMemory(dom, (unsigned long) a.env.memory) == -1)
                {
                    Log(LOG_LEVEL_INFO, " Unable to set the memory limit to %d", a.env.memory);
                }
                else
                {
                    Log(LOG_LEVEL_INFO, "Setting the memory limit to %d", a.env.memory);
                }

                if (virDomainSetMemory(dom, (unsigned long) a.env.memory) == -1)
                {
                    Log(LOG_LEVEL_INFO, " Unable to set the current memory to %d", a.env.memory);
                }
            }
        }

        if (a.env.disk != CF_NOINT)
        {
            Log(LOG_LEVEL_VERBOSE, "Info: env_disk parameter is not currently supported on this platform");
        }

        virDomainFree(dom);
    }
    else
    {
        virErrorPtr vp;

        vp = virGetLastError();

        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a,
             "Failed to create a virtual domain '%s' - check spec for errors: '%s'", pp->promiser, vp->message);
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);

        Log(LOG_LEVEL_VERBOSE, "Quoted spec file: %s", xml_file);
    }

    if (alloc_file)
    {
        free(xml_file);
    }

    return result;
}

/*****************************************************************************/

static PromiseResult DeleteVirt(EvalContext *ctx, virConnectPtr vc, Attributes a, const Promise *pp)
{
    virDomainPtr dom;

    dom = virDomainLookupByName(vc, pp->promiser);

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (dom)
    {
        if (virDomainDestroy(dom) == -1)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Failed to delete virtual domain '%s'", pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Deleted virtual domain '%s'", pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
        }

        virDomainFree(dom);
    }
    else
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "No such virtual domain called '%s' - promise kept", pp->promiser);
    }

    return result;
}

/*****************************************************************************/

static PromiseResult RunningVirt(EvalContext *ctx, virConnectPtr vc, Attributes a, const Promise *pp)
{
    virDomainPtr dom;
    virDomainInfo info;

    dom = virDomainLookupByName(vc, pp->promiser);

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (dom)
    {
        if (virDomainGetInfo(dom, &info) == -1)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Unable to probe virtual domain '%s'", pp->promiser);
            virDomainFree(dom);
            return PROMISE_RESULT_FAIL;
        }

        switch (info.state)
        {
        case VIR_DOMAIN_RUNNING:
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Virtual domain '%s' running - promise kept", pp->promiser);
            break;

        case VIR_DOMAIN_BLOCKED:
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a,
                 "Virtual domain '%s' running but waiting for a resource - promise kept as far as possible",
                 pp->promiser);
            break;

        case VIR_DOMAIN_SHUTDOWN:
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Virtual domain '%s' is shutting down", pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
            Log(LOG_LEVEL_VERBOSE,
                "It is currently impossible to know whether it will reboot or not - deferring promise check until it has completed its shutdown");
            break;

        case VIR_DOMAIN_PAUSED:

            if (virDomainResume(dom) == -1)
            {
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Virtual domain '%s' failed to resume after suspension",
                     pp->promiser);
                virDomainFree(dom);
                result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                return result;
            }

            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Virtual domain '%s' was suspended, resuming", pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            break;

        case VIR_DOMAIN_SHUTOFF:

            if (virDomainCreate(dom) == -1)
            {
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Virtual domain '%s' failed to resume after halting",
                     pp->promiser);
                result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                virDomainFree(dom);
                return result;
            }

            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Virtual domain '%s' was inactive, booting...", pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            break;

        case VIR_DOMAIN_CRASHED:

            if (virDomainReboot(dom, 0) == -1)
            {
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Virtual domain '%s' has crashed and rebooting failed",
                     pp->promiser);
                result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                virDomainFree(dom);
                return result;
            }

            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Virtual domain '%s' has crashed, rebooting...", pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            break;

        default:
            Log(LOG_LEVEL_VERBOSE, "Virtual domain '%s' is reported as having no state, whatever that means",
                pp->promiser);
            break;
        }

        if (a.env.cpus > 0)
        {
            if (virDomainSetVcpus(dom, a.env.cpus) == -1)
            {
                Log(LOG_LEVEL_INFO, " Unable to set the number of cpus to %d", a.env.cpus);
            }
            else
            {
                Log(LOG_LEVEL_INFO, "Setting the number of virtual cpus to %d", a.env.cpus);
            }
        }

        if (a.env.memory != CF_NOINT)
        {
            if (virDomainSetMaxMemory(dom, (unsigned long) a.env.memory) == -1)
            {
                Log(LOG_LEVEL_INFO, " Unable to set the memory limit to %d", a.env.memory);
            }
            else
            {
                Log(LOG_LEVEL_INFO, "Setting the memory limit to %d", a.env.memory);
            }

            if (virDomainSetMemory(dom, (unsigned long) a.env.memory) == -1)
            {
                Log(LOG_LEVEL_INFO, " Unable to set the current memory to %d", a.env.memory);
            }
        }

        if (a.env.disk != CF_NOINT)
        {
            Log(LOG_LEVEL_VERBOSE, "Info: env_disk parameter is not currently supported on this platform");
        }

        virDomainFree(dom);
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Virtual domain '%s' cannot be located, attempting to recreate", pp->promiser);
        result = PromiseResultUpdate(result, CreateVirtDom(ctx, vc, a, pp));
    }

    return result;
}

/*****************************************************************************/

static PromiseResult SuspendedVirt(EvalContext *ctx, virConnectPtr vc, Attributes a, const Promise *pp)
{
    virDomainPtr dom;
    virDomainInfo info;

    dom = virDomainLookupByName(vc, pp->promiser);

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (dom)
    {
        if (virDomainGetInfo(dom, &info) == -1)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Unable to probe virtual domain '%s'", pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            virDomainFree(dom);
            return result;
        }

        switch (info.state)
        {
        case VIR_DOMAIN_BLOCKED:
        case VIR_DOMAIN_RUNNING:
            if (virDomainSuspend(dom) == -1)
            {
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Virtual domain '%s' failed to suspend", pp->promiser);
                result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                virDomainFree(dom);
                return result;
            }

            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Virtual domain '%s' running, suspending", pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            break;

        case VIR_DOMAIN_SHUTDOWN:
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Virtual domain '%s' is shutting down", pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
            Log(LOG_LEVEL_VERBOSE,
                "It is currently impossible to know whether it will reboot or not - deferring promise check until it has completed its shutdown");
            break;

        case VIR_DOMAIN_PAUSED:
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Virtual domain '%s' is suspended - promise kept",
                 pp->promiser);
            break;

        case VIR_DOMAIN_SHUTOFF:
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Virtual domain '%s' is down - promise kept", pp->promiser);
            break;

        case VIR_DOMAIN_CRASHED:

            if (virDomainSuspend(dom) == -1)
            {
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Virtual domain '%s' is crashed has failed to suspend",
                     pp->promiser);
                result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                virDomainFree(dom);
                return result;
            }

            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Virtual domain '%s' is in a crashed state, suspending",
                 pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            break;

        default:
            Log(LOG_LEVEL_VERBOSE, "Virtual domain '%s' is reported as having no state, whatever that means",
                pp->promiser);
            break;
        }

        virDomainFree(dom);
    }
    else
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Virtual domain '%s' cannot be found - take promise as kept",
             pp->promiser);
    }

    return result;
}

/*****************************************************************************/

static PromiseResult DownVirt(EvalContext *ctx, virConnectPtr vc, Attributes a, const Promise *pp)
{
    virDomainPtr dom;
    virDomainInfo info;

    dom = virDomainLookupByName(vc, pp->promiser);

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (dom)
    {
        if (virDomainGetInfo(dom, &info) == -1)
        {
            cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Unable to probe virtual domain '%s'", pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
            virDomainFree(dom);
            return result;
        }

        switch (info.state)
        {
        case VIR_DOMAIN_BLOCKED:
        case VIR_DOMAIN_RUNNING:
            if (virDomainShutdown(dom) == -1)
            {
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Virtual domain '%s' failed to shutdown",
                     pp->promiser);
                result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                virDomainFree(dom);
                return result;
            }

            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Virtual domain '%s' running, terminating", pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            break;

        case VIR_DOMAIN_SHUTOFF:
        case VIR_DOMAIN_SHUTDOWN:
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Virtual domain '%s' is down - promise kept", pp->promiser);
            break;

        case VIR_DOMAIN_PAUSED:
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Virtual domain '%s' is suspended - ignoring promise",
                 pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
            break;

        case VIR_DOMAIN_CRASHED:
            if (virDomainSuspend(dom) == -1)
            {
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_INTERRUPTED, pp, a, "Virtual domain '%s' is crashed and failed to shutdown",
                     pp->promiser);
                result = PromiseResultUpdate(result, PROMISE_RESULT_INTERRUPTED);
                virDomainFree(dom);
                return false;
            }

            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_CHANGE, pp, a, "Virtual domain '%s' is in a crashed state, terminating",
                 pp->promiser);
            result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
            break;

        default:
            Log(LOG_LEVEL_VERBOSE, "Virtual domain '%s' is reported as having no state, whatever that means",
                pp->promiser);
            break;
        }

        virDomainFree(dom);
    }
    else
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Virtual domain '%s' cannot be found - take promise as kept",
             pp->promiser);
    }

    return result;
}

/*****************************************************************************/

static PromiseResult CreateVirtNetwork(EvalContext *ctx, virConnectPtr vc, char **networks, Attributes a, const Promise *pp)
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
        Log(LOG_LEVEL_VERBOSE, "Discovered a running network '%s'", networks[i]);

        if (strcmp(networks[i], pp->promiser) == 0)
        {
            found = true;
        }
    }

    if (found)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Network '%s' exists - promise kept", pp->promiser);
        return PROMISE_RESULT_NOOP;
    }

    if (a.env.spec)
    {
        xml_file = xstrdup(a.env.spec);
    }
    else
    {
        xml_file = xstrdup(defaultxml);
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if ((network = virNetworkCreateXML(vc, xml_file)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Unable to create network '%s'", pp->promiser);
        free(xml_file);
        return PROMISE_RESULT_FAIL;
    }
    else
    {
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Created network '%s' - promise repaired", pp->promiser);
        result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
    }

    free(xml_file);

    virNetworkFree(network);
    return result;
}

/*****************************************************************************/

static PromiseResult DeleteVirtNetwork(EvalContext *ctx, virConnectPtr vc, Attributes a, const Promise *pp)
{
    virNetworkPtr network;

    if ((network = virNetworkLookupByName(vc, pp->promiser)) == NULL)
    {
        cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_NOOP, pp, a, "Couldn't find a network called '%s' - promise assumed kept",
             pp->promiser);
        return PROMISE_RESULT_NOOP;
    }

    PromiseResult result = PROMISE_RESULT_NOOP;
    if (virNetworkDestroy(network) == 0)
    {
        cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Deleted network '%s' - promise repaired", pp->promiser);
        result = PromiseResultUpdate(result, PROMISE_RESULT_CHANGE);
    }
    else
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Network deletion of '%s' failed", pp->promiser);
        result = PromiseResultUpdate(result, PROMISE_RESULT_FAIL);
    }

    virNetworkFree(network);
    return result;
}

/*****************************************************************************/

static void VerifyDockerContainerRunning(EvalContext *ctx, Attributes a, const Promise *pp, DockerPS *containers, PromiseResult *result)
{
    int found = false;

    for (DockerPS *ps = containers; ps != NULL; ps = ps->next)
    {
        if (strcmp(pp->promiser, ps->name) == 0)
        {
            found = true;

            if (strncmp(ps->image, a.env.image_name, strlen(a.env.image_name)) != 0)
            {
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_FAIL, pp, a, "A container exists with the name \"%s\" but seems to be based on the image %s instead of %s", ps->name, ps->image, a.env.image_name);
                *result = PROMISE_RESULT_FAIL;
                return;
            }
        }
    }

// Start the container

    if (!found)
    {
        char comm[CF_BUFSIZE], value[CF_BUFSIZE], address[CF_MAX_IP_LEN];

        Log(LOG_LEVEL_VERBOSE, "Removing any prevous docker instance '%s' name/container binding", pp->promiser);

        snprintf(comm, CF_BUFSIZE, "%s rm %s", DOCKER_COMMAND, pp->promiser);
        ExecEnvCommand(comm, value);

        snprintf(comm, CF_BUFSIZE, "%s run --name %s --hostname %s -d %s", DOCKER_COMMAND, pp->promiser, pp->promiser, a.env.image_name);

        if (!ExecEnvCommand(comm, value))
        {
            cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_FAIL, pp, a, "Failed to keep promise to start Docker instance %s from 'comm' (%s)", pp->promiser, value);
            *result = PROMISE_RESULT_FAIL;
        }
        else
        {
            cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Launched Docker container: %s", pp->promiser);
            *result = PROMISE_RESULT_CHANGE;
            GetDockerIPForContainer(value, address);
            if (strlen(address) == 0)
            {
                Log(LOG_LEVEL_VERBOSE, "Docker instance %s exited quickly (%s)", pp->promiser, comm);
            }
            else
            {
                Log(LOG_LEVEL_VERBOSE, "Promise to start Docker instance %s kept (%s)", pp->promiser, address);
            }

            // Set vars  docker_guest[promiser] => ip
            // Set has_running_docker_instances = number
        }
    }
}

/*****************************************************************************/

static void VerifyDockerContainerNotRunning(EvalContext *ctx, Attributes a, const Promise *pp, DockerPS *containers, PromiseResult *result)
{
    char comm[CF_BUFSIZE], error[CF_BUFSIZE];
    int found = false;

    for (DockerPS *ps = containers; ps != NULL; ps = ps->next)
    {
        if (StringMatchFull(pp->promiser, ps->name) && strncmp(ps->image, a.env.image_name, strlen(a.env.image_name)) == 0)
        {
            Log(LOG_LEVEL_VERBOSE, "Matched a Docker instance %s with same image (%s)", pp->promiser, a.env.image_name);

            found = true;

            Log(LOG_LEVEL_VERBOSE, "Attempting polite stop to docker instance %s with image (%s)", pp->promiser, a.env.image_name);
            snprintf(comm, CF_BUFSIZE, "%s stop %s", DOCKER_COMMAND, ps->id);

            if (!ExecEnvCommand(comm, error))
            {
                cfPS(ctx, LOG_LEVEL_VERBOSE, PROMISE_RESULT_FAIL, pp, a, "Failed to set keep promise to bring down Docker instance %s (%s)", ps->name, ps->id);
                *result = PROMISE_RESULT_FAIL;
            }
            else
            {
                cfPS(ctx, LOG_LEVEL_INFO, PROMISE_RESULT_CHANGE, pp, a, "Stopped Docker container: %s (%s)", ps->name, ps->id);
                *result = PROMISE_RESULT_CHANGE;
            }
        }
    }

    if (!found)
    {
        Log(LOG_LEVEL_VERBOSE, "Didn't match any Docker instance '%s' with same image (%s)", pp->promiser, a.env.image_name);
    }
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
                Log(LOG_LEVEL_VERBOSE, "Found a running virtual domain with id %d", CF_RUNNING[i]);
            }

            if ((name = virDomainGetName(dom)))
            {
                Log(LOG_LEVEL_VERBOSE, "Found a running virtual domain called '%s'", name);
            }

            virDomainFree(dom);
        }
    }
}

/*****************************************************************************/

static void ShowDormant(void)
{
    int i;

    for (i = 0; CF_SUSPENDED[i] != NULL; i++)
    {
        Log(LOG_LEVEL_VERBOSE, "Found a suspended, domain environment called '%s'", CF_SUSPENDED[i]);
    }
}

/*****************************************************************************/

static enum cfhypervisors Str2Hypervisors(char *s)
{
    static char *names[] = { "xen", "kvm", "esx", "vbox", "lxc", "test",
                             "xen_net", "kvm_net", "esx_net", "test_net",
                             "zone", "docker", "ec2", "eucalyptus", NULL
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

#else /* !HAVE_LIBVIRT */

void NewEnvironmentsContext(void)
{
}

void DeleteEnvironmentsContext(void)
{
}

PromiseResult VerifyEnvironmentsPromise(ARG_UNUSED EvalContext *ctx, ARG_UNUSED const Promise *pp)
{
    return PROMISE_RESULT_NOOP;
}

#endif
