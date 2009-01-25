/* 
   Copyright (C) 2008 - Cfengine AS

   This file is part of Cfengine 3 - written and maintained by Cfengine AS.
 
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version. 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

*/

/*****************************************************************************/
/*                                                                           */
/* File: mod_interfaces.c                                                    */
/*                                                                           */
/*****************************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

/*****************************************************************************/

#define CF3_MOD_PACKAGES

struct BodySyntax CF_PKGMETHOD_BODY[] =
   {
   {"package_use_shell",cf_opts,CF_BOOL,"Whether to use a shell wrapper when interfacing with package manager"},
   {"package_aggregation",cf_opts,CF_BOOL,"Whether to group packages into a single aggregate command"},
   {"package_repositories",cf_slist,"","A list of machine-local directories to search for packages"},
   {"package_verify",cf_opts,CF_BOOL,"Whether to attempt verification of matching installed packages (resource heavy)"},
   {"package_verify_failure_class",cf_str,CF_IDRANGE,"Whether to attempt verification of matching installed packages (resource heavy)"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/

struct BodySyntax CF_PKGSELECT_BODY[] =
   {
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/

/* This is the primary set of constraints for an interfaces object */

struct BodySyntax CF_PACKAGES_BODIES[] =
   {
   {"package_list",cf_slist,"","List of packages to review"},
   {"package_policy",cf_opts,"insert|delete|replace","Criteria for package installation/upgrade on the current system"},
   {"package_methods",cf_body,CF_PKGMETHOD_BODY,"Criteria for installation and verification"},
   {"package_select",cf_body,CF_PKGSELECT_BODY,"Criteria for selecting version"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/
/* This is the point of entry from mod_common.c                */
/***************************************************************/

struct SubTypeSyntax CF_PACKAGES_SUBTYPES[] =
  {
  {"agent","packages",CF_PACKAGES_BODIES},
  {NULL,NULL,NULL},
  };

