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

#include "mod_environ.h"

#include "syntax.h"

static const ConstraintSyntax CF_RESOURCE_BODY[] =
{
    ConstraintSyntaxNewInt("env_cpus", CF_VALRANGE, "Number of virtual CPUs in the environment", NULL),
    ConstraintSyntaxNewInt("env_memory", CF_VALRANGE, "Amount of primary storage (RAM) in the virtual environment (KB)", NULL),
    ConstraintSyntaxNewInt("env_disk", CF_VALRANGE, "Amount of secondary storage (DISK) in the virtual environment (MB)", NULL),
    ConstraintSyntaxNewString("env_baseline", CF_ABSPATHRANGE, "The path to an image with which to baseline the virtual environment", NULL),
    ConstraintSyntaxNewString("env_spec", CF_ANYSTRING, "A string containing a technology specific set of promises for the virtual instance", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_DESIGNATION_BODY[] =
{
    ConstraintSyntaxNewStringList("env_addresses", "", "The IP addresses of the environment's network interfaces"),
    ConstraintSyntaxNewString("env_name", "", "The hostname of the virtual environment", NULL),
    ConstraintSyntaxNewString("env_network", "", "The hostname of the virtual network", NULL),
    ConstraintSyntaxNewNull()
};

static const ConstraintSyntax CF_ENVIRON_BODIES[] =
{
    ConstraintSyntaxNewString("environment_host", "[a-zA-Z0-9_]+", "A class indicating which physical node will execute this guest machine", NULL),
    ConstraintSyntaxNewBody("environment_interface", CF_DESIGNATION_BODY, "Virtual environment outward identity and location"),
    ConstraintSyntaxNewBody("environment_resources", CF_RESOURCE_BODY, "Virtual environment resource description"),
    ConstraintSyntaxNewOption("environment_state", "create,delete,running,suspended,down", "The desired dynamical state of the specified environment", NULL),
    ConstraintSyntaxNewOption("environment_type", "xen,kvm,esx,vbox,test,xen_net,kvm_net,esx_net,test_net,zone,ec2,eucalyptus", "Virtual environment type", NULL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_ENVIRONMENT_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "guest_environments", ConstraintSetSyntaxNew(CF_ENVIRON_BODIES, NULL)),
    PromiseTypeSyntaxNewNull()
};
