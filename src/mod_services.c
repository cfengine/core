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
#include "mod_services.h"

static const BodySyntax CF_SERVMETHOD_BODY[] =
{
    {"service_args", cf_str, "", "Parameters for starting the service as command"},
    {"service_autostart_policy", cf_opts, "none,boot_time,on_demand",
     "Should the service be started automatically by the OS"},
    {"service_bundle", cf_bundle, CF_BUNDLE,
     "A bundle reference with two arguments (service_name,args) used if the service type is generic"},
    {"service_dependence_chain", cf_opts, "ignore,start_parent_services,stop_child_services,all_related",
     "How to handle dependencies and dependent services"},
    {"service_type", cf_opts, "windows,generic", "Service abstraction type"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_SERVICES_BODIES[] =
{
    {"service_policy", cf_opts, "start,stop,disable,restart,reload", "Policy for cfengine service status"},
    {"service_dependencies", cf_slist, CF_IDRANGE, "A list of services on which the named service abstraction depends"},
    {"service_method", cf_body, CF_SERVMETHOD_BODY, "Details of promise body for the service abtraction feature"},
    {NULL, cf_notype, NULL, NULL}
};

const SubTypeSyntax CF_SERVICES_SUBTYPES[] =
{
    {"agent", "services", CF_SERVICES_BODIES},
    {NULL, NULL, NULL},
};
