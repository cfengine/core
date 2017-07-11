/*
   Copyright 2017 Northern.tech AS

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

#include <mod_services.h>

#include <syntax.h>

static const ConstraintSyntax service_method_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewString("service_args", "", "Parameters for starting the service as command", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("service_autostart_policy", "none,boot_time,on_demand", "Should the service be started automatically by the OS", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBundle("service_bundle", "A bundle reference with two arguments (service_name,args) used if the service type is generic", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("service_dependence_chain", "ignore,start_parent_services,stop_child_services,all_related", "How to handle dependencies and dependent services", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("service_type", "windows,generic", "Service abstraction type", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax service_method_body = BodySyntaxNew("service_method", service_method_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax services_constraints[] =
{
    ConstraintSyntaxNewOption("service_policy", "start,stop,enable,disable,restart,reload", "Policy for cfengine service status", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("service_dependencies", CF_IDRANGE, "A list of services on which the named service abstraction depends", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("service_method", &service_method_body, "Details of promise body for the service abtraction feature", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_SERVICES_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "services", services_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
