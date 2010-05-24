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
/* File: mod_environ.c                                                       */
/*                                                                           */
/*****************************************************************************/

/*

 This file can act as a template for adding functionality to cfengine 3.
 All functionality can be added by extending the main array

 CF_MOD_SUBTYPES[CF3_MODULES]

 and its array dimension, in mod_common, in the manner shown here.
 
*/

#define CF3_MOD_ENVIRON

#include "cf3.defs.h"
#include "cf3.extern.h"

 /***********************************************************/
 /* Read this module file backwards, as dependencies have   */
 /* to be defined first - these arrays declare pairs of     */
 /* constraints                                             */
 /*                                                         */
 /* lval => rval                                            */
 /*                                                         */
 /* in the form (lval,type,range)                           */
 /*                                                         */
 /* If the type is cf_body then the range is a pointer      */
 /* to another array of pairs, like in a body "sub-routine" */
 /*                                                         */
 /***********************************************************/

struct BodySyntax CF_RESOURCE_BODY[] =
   {
   {"env_cpus",cf_int,CF_VALRANGE,"Number of virtual CPUs in the environment"},
   {"env_memory",cf_int,CF_VALRANGE,"Amount of primary storage (RAM) in the virtual environment (KB)"},
   {"env_disk",cf_int,CF_VALRANGE,"Amount of secondary storage (DISK) in the virtual environment (MB)"},
   {"env_baseline",cf_str,CF_PATHRANGE,"The path to an image with which to baseline the virtual environment"},
   {"env_spec_file",cf_str,CF_PATHRANGE,"The path to a file containing a technology specific set of promises for the virtual instance"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/

struct BodySyntax CF_DESIGNATION_BODY[] =
   {
   {"env_addresses",cf_slist,"","The IP addresses of the environment's network interfaces"},
   {"env_name",cf_str,"","The hostname of the virtual environment"},
   {"env_network",cf_str,"","The hostname of the virtual network"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/

struct BodySyntax CF_ENVIRON_BODIES[] =
   {
   {"environment_host",cf_str,CF_IDRANGE,"The name of the virtual environment host (this must be promised uniquely)"},
   {"environment_interface",cf_body,CF_DESIGNATION_BODY,"Virtual environment outward identity and location"},
   {"environment_resources",cf_body,CF_RESOURCE_BODY,"Virtual environment resource description"},
   {"environment_state",cf_opts,"create,delete,running,suspended,down","The desired dynamical state of the specified environment"},
   {"environment_type",cf_opts,"xen,kvm,esx,test,xen_net,kvm_net,esx_net,test_net,zone,ec2,eucalyptus","Virtual environment type"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_ENVIRONMENT_SUBTYPES[] =
  {
  {"agent","environments",CF_ENVIRON_BODIES},
  {NULL,NULL,NULL},
  };

