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
#include "mod_packages.h"

static const BodySyntax CF_PKGMETHOD_BODY[] =
{
    {"package_add_command", cf_str, CF_PATHRANGE, "Command to install a package to the system"},
    {"package_arch_regex", cf_str, "",
     "Regular expression with one backreference to extract package architecture string"},
    {"package_changes", cf_opts, "individual,bulk",
     "Menu option - whether to group packages into a single aggregate command"},
    {"package_delete_command", cf_str, CF_PATHRANGE, "Command to remove a package from the system"},
    {"package_delete_convention", cf_str, "",
     "This is how the package manager expects the package to be referred to in the deletion part of a package update, e.g. $(name)"},
    {"package_file_repositories", cf_slist, "", "A list of machine-local directories to search for packages"},
    {"package_installed_regex", cf_str, "", "Regular expression which matches packages that are already installed"},
    {"package_default_arch_command", cf_str, CF_ABSPATHRANGE, "Command to detect the default packages' architecture"},
    {"package_list_arch_regex", cf_str, "",
     "Regular expression with one backreference to extract package architecture string"},
    {"package_list_command", cf_str, CF_PATHRANGE, "Command to obtain a list of available packages"},
    {"package_list_name_regex", cf_str, "", "Regular expression with one backreference to extract package name string"},
    {"package_list_update_command", cf_str, "", "Command to update the list of available packages (if any)"},
    {"package_list_update_ifelapsed", cf_int, CF_INTRANGE,
     "The ifelapsed locking time in between updates of the package list"},
    {"package_list_version_regex", cf_str, "",
     "Regular expression with one backreference to extract package version string"},
    {"package_name_convention", cf_str, "",
     "This is how the package manager expects the package to be referred to, e.g. $(name).$(arch)"},
    {"package_name_regex", cf_str, "", "Regular expression with one backreference to extract package name string"},
    {"package_noverify_regex", cf_str, "", "Regular expression to match verification failure output"},
    {"package_noverify_returncode", cf_int, CF_INTRANGE, "Integer return code indicating package verification failure"},
    {"package_patch_arch_regex", cf_str, "",
     "Regular expression with one backreference to extract update architecture string"},
    {"package_patch_command", cf_str, CF_PATHRANGE,
     "Command to update to the latest patch release of an installed package"},
    {"package_patch_installed_regex", cf_str, "",
     "Regular expression which matches packages that are already installed"},
    {"package_patch_list_command", cf_str, CF_PATHRANGE, "Command to obtain a list of available patches or updates"},
    {"package_patch_name_regex", cf_str, "", "Regular expression with one backreference to extract update name string"},
    {"package_patch_version_regex", cf_str, "",
     "Regular expression with one backreference to extract update version string"},
    {"package_update_command", cf_str, CF_PATHRANGE,
     "Command to update to the latest version a currently installed package"},
    {"package_verify_command", cf_str, CF_PATHRANGE, "Command to verify the correctness of an installed package"},
    {"package_version_regex", cf_str, "",
     "Regular expression with one backreference to extract package version string"},
    {"package_multiline_start", cf_str, "",
     "Regular expression which matches the start of a new package in multiline output"},
    {"package_commands_useshell", cf_opts, CF_BOOL,
     "Whether to use shell for commands in this body", "true"},
    {"package_version_less_command", cf_str, CF_PATHRANGE, "Command to check whether first supplied package version is less than second one"},
    {"package_version_equal_command", cf_str, CF_PATHRANGE, "Command to check whether first supplied package version is equal to second one"},
    {NULL, cf_notype, NULL, NULL}
};

static const BodySyntax CF_PACKAGES_BODIES[] =
{
    {"package_architectures", cf_slist, "", "Select the architecture for package selection"},
    {"package_method", cf_body, CF_PKGMETHOD_BODY, "Criteria for installation and verification"},
    {"package_policy", cf_opts, "add,delete,reinstall,update,addupdate,patch,verify",
     "Criteria for package installation/upgrade on the current system", "verify"},
    {"package_select", cf_opts, ">,<,==,!=,>=,<=",
     "A criterion for first acceptable match relative to \"package_version\""},
    {"package_version", cf_str, "", "Version reference point for determining promised version"},
    {NULL, cf_notype, NULL, NULL}
};

const SubTypeSyntax CF_PACKAGES_SUBTYPES[] =
{
    {"agent", "packages", CF_PACKAGES_BODIES},
    {NULL, NULL, NULL},
};
