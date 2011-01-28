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

/*****************************************************************************/
/*                                                                           */
/* File: mod_services.c                                                      */
/*                                                                           */
/* Created: Fri Dec  4 09:36:59 2009                                         */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

#define CF3_MOD_SERVICES


struct BodySyntax CF_SERVMETHOD_BODY[] =
   {
   {"service_type",cf_opts,"windows,init,inetd,xinetd","Service abstraction type"},
   {"service_args",cf_str,"","Parameters for starting the service"},
   {"service_autostart_policy",cf_opts,"none,boot_time,on_demand","Should the service be started automatically by the OS"},
   {"service_dependence_chain",cf_opts,"ignore,start_parent_services,stop_child_services,all_related","How to handle dependencies and dependent services"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/

struct BodySyntax CF_SERVICES_BODIES[] =
   {
   {"service_policy",cf_opts,"start,stop,disable","Policy for cfengine service status"},
   {"service_dependencies",cf_slist,CF_IDRANGE,"A list of services on which the named service abstraction depends"},
   {"service_method",cf_body,CF_SERVMETHOD_BODY,"Details of promise body for the service abtraction feature"},
   {NULL,cf_notype,NULL,NULL}
   };


/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_SERVICES_SUBTYPES[] =
  {
  {"agent","services",CF_SERVICES_BODIES},
  {NULL,NULL,NULL},
  };










