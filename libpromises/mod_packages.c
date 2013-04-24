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

#include "mod_packages.h"

#include "syntax.h"

static const ConstraintSyntax package_method_constraints[] =
{
    ConstraintSyntaxNewString("package_add_command", CF_PATHRANGE, "Command to install a package to the system", NULL),
    ConstraintSyntaxNewString("package_arch_regex", "", "Regular expression with one backreference to extract package architecture string", NULL),
    ConstraintSyntaxNewOption("package_changes", "individual,bulk", "Menu option - whether to group packages into a single aggregate command", NULL),
    ConstraintSyntaxNewString("package_delete_command", CF_PATHRANGE, "Command to remove a package from the system", NULL),
    ConstraintSyntaxNewString("package_delete_convention", "", "This is how the package manager expects the package to be referred to in the deletion part of a package update, e.g. $(name)", NULL),
    ConstraintSyntaxNewStringList("package_file_repositories", "", "A list of machine-local directories to search for packages"),
    ConstraintSyntaxNewString("package_installed_regex", "", "Regular expression which matches packages that are already installed", NULL),
    ConstraintSyntaxNewString("package_default_arch_command", CF_ABSPATHRANGE, "Command to detect the default packages' architecture", NULL),
    ConstraintSyntaxNewString("package_list_arch_regex", "", "Regular expression with one backreference to extract package architecture string", NULL),
    ConstraintSyntaxNewString("package_list_command", CF_PATHRANGE, "Command to obtain a list of available packages", NULL),
    ConstraintSyntaxNewString("package_list_name_regex", "", "Regular expression with one backreference to extract package name string", NULL),
    ConstraintSyntaxNewString("package_list_update_command", "", "Command to update the list of available packages (if any)", NULL),
    ConstraintSyntaxNewInt("package_list_update_ifelapsed", CF_INTRANGE, "The ifelapsed locking time in between updates of the package list", NULL),
    ConstraintSyntaxNewString("package_list_version_regex", "", "Regular expression with one backreference to extract package version string", NULL),
    ConstraintSyntaxNewString("package_name_convention", "", "This is how the package manager expects the package to be referred to, e.g. $(name).$(arch)", NULL),
    ConstraintSyntaxNewString("package_name_regex", "", "Regular expression with one backreference to extract package name string", NULL),
    ConstraintSyntaxNewString("package_noverify_regex", "", "Regular expression to match verification failure output", NULL),
    ConstraintSyntaxNewInt("package_noverify_returncode", CF_INTRANGE, "Integer return code indicating package verification failure", NULL),
    ConstraintSyntaxNewString("package_patch_arch_regex", "", "Regular expression with one backreference to extract update architecture string", NULL),
    ConstraintSyntaxNewString("package_patch_command", CF_PATHRANGE, "Command to update to the latest patch release of an installed package", NULL),
    ConstraintSyntaxNewString("package_patch_installed_regex", "", "Regular expression which matches packages that are already installed", NULL),
    ConstraintSyntaxNewString("package_patch_list_command", CF_PATHRANGE, "Command to obtain a list of available patches or updates", NULL),
    ConstraintSyntaxNewString("package_patch_name_regex", "", "Regular expression with one backreference to extract update name string", NULL),
    ConstraintSyntaxNewString("package_patch_version_regex", "", "Regular expression with one backreference to extract update version string", NULL),
    ConstraintSyntaxNewString("package_update_command", CF_PATHRANGE, "Command to update to the latest version a currently installed package", NULL),
    ConstraintSyntaxNewString("package_verify_command", CF_PATHRANGE, "Command to verify the correctness of an installed package", NULL),
    ConstraintSyntaxNewString("package_version_regex", "", "Regular expression with one backreference to extract package version string", NULL),
    ConstraintSyntaxNewString("package_multiline_start", "", "Regular expression which matches the start of a new package in multiline output", NULL),
    ConstraintSyntaxNewBool("package_commands_useshell", "Whether to use shell for commands in this body", "true"),
    ConstraintSyntaxNewString("package_version_less_command", CF_PATHRANGE, "Command to check whether first supplied package version is less than second one", NULL),
    ConstraintSyntaxNewString("package_version_equal_command", CF_PATHRANGE, "Command to check whether first supplied package version is equal to second one", NULL),
    ConstraintSyntaxNewNull()
};

static const BodyTypeSyntax package_method_body = BodyTypeSyntaxNew("package_method", package_method_constraints, NULL);

static const ConstraintSyntax packages_constraints[] =
{
    ConstraintSyntaxNewStringList("package_architectures", "", "Select the architecture for package selection"),
    ConstraintSyntaxNewBody("package_method", &package_method_body, "Criteria for installation and verification", NULL),
    ConstraintSyntaxNewOption("package_policy", "add,delete,reinstall,update,addupdate,patch,verify", "Criteria for package installation/upgrade on the current system", "verify"),
    ConstraintSyntaxNewOption("package_select", ">,<,==,!=,>=,<=", "A criterion for first acceptable match relative to \"package_version\"", NULL),
    ConstraintSyntaxNewString("package_version", "", "Version reference point for determining promised version", NULL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_PACKAGES_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "packages", packages_constraints, NULL),
    PromiseTypeSyntaxNewNull(),
};
