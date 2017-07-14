/*
   Copyright 2017 Northern.tech AS

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

#include <mod_environ.h>

#include <syntax.h>

static const ConstraintSyntax environment_resources_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewInt("env_cpus", CF_VALRANGE, "Number of virtual CPUs in the environment", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("env_memory", CF_VALRANGE, "Amount of primary storage (RAM) in the virtual environment (KB)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("env_disk", CF_VALRANGE, "Amount of secondary storage (DISK) in the virtual environment (MB)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("env_baseline", CF_ABSPATHRANGE, "The path to an image with which to baseline the virtual environment", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("env_spec", CF_ANYSTRING, "A string containing a technology specific set of promises for the virtual instance", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax environment_resources_body = BodySyntaxNew("environment_resources", environment_resources_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax environment_interface_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewStringList("env_addresses", "", "The IP addresses of the environment's network interfaces", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("env_name", "", "The hostname of the virtual environment", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("env_network", "", "The hostname of the virtual network", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax environment_interface_body = BodySyntaxNew("environment_interface", environment_interface_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax CF_ENVIRON_BODIES[] =
{
    ConstraintSyntaxNewString("environment_host", "[a-zA-Z0-9_]+", "A class indicating which physical node will execute this guest machine", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("environment_interface", &environment_interface_body, "Virtual environment outward identity and location", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("environment_resources", &environment_resources_body, "Virtual environment resource description", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("environment_state", "create,delete,running,suspended,down", "The desired dynamical state of the specified environment", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("environment_type", "xen,kvm,esx,vbox,test,xen_net,kvm_net,esx_net,test_net,zone,ec2,eucalyptus", "Virtual environment type", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_ENVIRONMENT_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "guest_environments", CF_ENVIRON_BODIES, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
