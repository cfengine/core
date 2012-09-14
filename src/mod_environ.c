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
#include "mod_environ.h"

static const BodySyntax CF_RESOURCE_BODY[] =
{
    {"env_cpus", cf_int, CF_VALRANGE, "Number of virtual CPUs in the environment"},
    {"env_memory", cf_int, CF_VALRANGE, "Amount of primary storage (RAM) in the virtual environment (KB)"},
    {"env_disk", cf_int, CF_VALRANGE, "Amount of secondary storage (DISK) in the virtual environment (MB)"},
    {"env_baseline", cf_str, CF_ABSPATHRANGE, "The path to an image with which to baseline the virtual environment"},
    {"env_spec", cf_str, CF_ANYSTRING,
     "A string containing a technology specific set of promises for the virtual instance"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_DESIGNATION_BODY[] =
{
    {"env_addresses", cf_slist, "", "The IP addresses of the environment's network interfaces"},
    {"env_name", cf_str, "", "The hostname of the virtual environment"},
    {"env_network", cf_str, "", "The hostname of the virtual network"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_ENVIRON_BODIES[] =
{
    {"environment_host", cf_str, "[a-zA-Z0-9_]+",
     "A class indicating which physical node will execute this guest machine"},
    {"environment_interface", cf_body, CF_DESIGNATION_BODY, "Virtual environment outward identity and location"},
    {"environment_resources", cf_body, CF_RESOURCE_BODY, "Virtual environment resource description"},
    {"environment_state", cf_opts, "create,delete,running,suspended,down",
     "The desired dynamical state of the specified environment"},
    {"environment_type", cf_opts, "xen,kvm,esx,vbox,test,xen_net,kvm_net,esx_net,test_net,zone,ec2,eucalyptus",
     "Virtual environment type"},
    {NULL, cf_notype, NULL, NULL}
};

const SubTypeSyntax CF_ENVIRONMENT_SUBTYPES[] =
{
    {"agent", "guest_environments", CF_ENVIRON_BODIES},
    {NULL, NULL, NULL},
};
