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

#include <mod_environ.h>

#include <syntax.h>

static const ConstraintSyntax environment_resources_constraints[] =
{
    ConstraintSyntaxNewInt("env_cpus", CF_VALRANGE, "Number of virtual CPUs in the environment", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewInt("env_memory", CF_VALRANGE, "Amount of primary storage (RAM) in the virtual environment (KB)", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewInt("env_disk", CF_VALRANGE, "Amount of secondary storage (DISK) in the virtual environment (MB)", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("env_baseline", CF_ABSPATHRANGE, "The path to an image with which to baseline the virtual environment", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("env_spec", CF_ANYSTRING, "A string containing a technology specific set of promises for the virtual instance", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax guest_details_constraints[] =
{
    ConstraintSyntaxNewInt("guest_cpus", CF_VALRANGE, "Number of virtual CPUs in the environment", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("guest_memory", CF_VALRANGE, "Amount of primary storage (RAM) in the virtual environment (KB)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("guest_disk", CF_VALRANGE, "Amount of secondary storage (DISK) in the virtual environment (MB)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("guest_image_path", CF_ABSPATHRANGE, "The path to an image with which to initialize the virtual environment", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("guest_image_name", CF_ANYSTRING, "The name of the image which forms the initial state of the virtual environment", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("guest_libvirt_xml", CF_ANYSTRING, "A string containing a technology specific set of promises for the virtual instance", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("guest_addresses", "", "The IP addresses of the environment's network interfaces", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("guest_network", "", "The hostname of the virtual network", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("guest_type", "lxc,xen,kvm,esx,vbox,test,xen_net,kvm_net,esx_net,test_net,zone,ec2,eucalyptus,docker", "Guest environment type", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax environment_resources_body = BodySyntaxNew("environment_resources", environment_resources_constraints, NULL, SYNTAX_STATUS_REMOVED);
static const BodySyntax guest_details_body = BodySyntaxNew("guest_details", guest_details_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax environment_interface_constraints[] =
{
    ConstraintSyntaxNewStringList("env_addresses", "", "The IP addresses of the environment's network interfaces is deprecated", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("env_name", "", "The hostname of the virtual environment is deprecated - use the promiser", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewString("env_network", "", "The hostname of the virtual network is deprecated", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewNull()
};

static const BodySyntax environment_interface_body = BodySyntaxNew("environment_interface", environment_interface_constraints, NULL, SYNTAX_STATUS_REMOVED);

static const ConstraintSyntax CF_ENVIRON_BODIES[] =
{
    ConstraintSyntaxNewString("environment_host", "[a-zA-Z0-9_]+", "A class indicating which physical node will execute this guest machine (deprecated)", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewBody("environment_interface", &environment_interface_body, "Virtual environment outward identity and location (deprecated)", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewBody("environment_resources", &environment_resources_body, "Virtual environment resource description (deprecated)", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("environment_state", "create,delete,running,suspended,down", "The desired dynamical state of the specified environment", SYNTAX_STATUS_REMOVED),
    ConstraintSyntaxNewOption("environment_type", "lxc,xen,kvm,esx,vbox,test,xen_net,kvm_net,esx_net,test_net,zone,ec2,eucalyptus", "Virtual environment type (now called guest_type)", SYNTAX_STATUS_NORMAL),

    ConstraintSyntaxNewOption("guest_state", "create,delete,running,suspended,down", "The desired dynamical state of the specified environment", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("guest_details", &guest_details_body, "Virtual environment resource description", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_ENVIRONMENT_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "guest_environments", CF_ENVIRON_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
