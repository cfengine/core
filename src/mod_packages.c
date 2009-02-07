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
   {"package_changes",cf_opts,"individual,bulk","Menu option - whether to group packages into a single aggregate command"},
   {"package_file_repositories",cf_slist,"","A list of machine-local directories to search for packages"},
   {"package_list_command",cf_str,CF_PATHRANGE,"Command to obtain a list of installed packages"},
   {"package_list_version_regex",cf_str,"","Regular expression with one backreference to extract package version string" },
   {"package_list_name_regex",cf_str,"","Regular expression with one backreference to extract package name string" },
   {"package_list_arch_regex",cf_str,"","Regular expression with one backreference to extract package architecture string" },
   {"package_version_regex",cf_str,"","Regular expression with one backreference to extract package version string" },
   {"package_name_regex",cf_str,"","Regular expression with one backreference to extract package name string" },
   {"package_arch_regex",cf_str,"","Regular expression with one backreference to extract package architecture string" },
   {"package_installed_regex",cf_str,"","Regular expression which matches packages that are already installed"},
   {"package_add_command",cf_str,CF_PATHRANGE,"Command to install a package to the system"},
   {"package_delete_command",cf_str,CF_PATHRANGE,"Command to remove a package from the system"},
   {"package_update_command",cf_str,CF_PATHRANGE,"Command to update to the latest version a currently installed package"},
   {"package_patch_command",cf_str,CF_PATHRANGE,"Command to update to the latest patch release of an installed package"},
   {"package_verify_command",cf_str,CF_PATHRANGE,"Command to verify the correctness of an installed package"},
   {"package_noverify_regex",cf_str,"","Regular expression to match verification failure output"},
   {"package_noverify_returncode",cf_int,CF_INTRANGE,"Integer return code indicating package verification failure"},
   {"package_name_convention",cf_str,"","This is how the package manager expects the file to be referred to, e.g. $(name).$(arch)"},
   {NULL,cf_notype,NULL,NULL}
   };

/***************************************************************/

/* This is the primary set of constraints for an interfaces object */

struct BodySyntax CF_PACKAGES_BODIES[] =
   {
   {"package_policy",cf_opts,"add,delete,reinstall,update,patch,verify","Criteria for package installation/upgrade on the current system"},
   {"package_method",cf_body,CF_PKGMETHOD_BODY,"Criteria for installation and verification"},
   {"package_version",cf_str,"","Version reference point for determining promised version"},
   {"package_architectures",cf_slist,"","Select the architecture for package selection"},
   {"package_select",cf_opts,">,<,==,!=,>=,<=","A criterion for first acceptable match relative to \"package_version\""},
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

