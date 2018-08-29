/*
   Copyright 2018 Northern.tech AS

   This file is part of CFEngine 3 - written and maintained by Northern.tech AS.

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

#include <mod_packages.h>

#include <syntax.h>

static const ConstraintSyntax package_method_constraints[] =
{
    CONSTRAINT_SYNTAX_GLOBAL,
    ConstraintSyntaxNewString("package_add_command", CF_PATHRANGE, "Command to install a package to the system", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_arch_regex", "", "Regular expression with one backreference to extract package architecture string", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("package_changes", "individual,bulk", "Menu option - whether to group packages into a single aggregate command", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_delete_command", CF_PATHRANGE, "Command to remove a package from the system", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_delete_convention", "", "This is how the package manager expects the package to be referred to in the deletion part of a package update, e.g. $(name)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("package_file_repositories", "", "A list of machine-local directories to search for packages", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_installed_regex", "", "Regular expression which matches packages that are already installed", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_default_arch_command", CF_ABSPATHRANGE, "Command to detect the default packages' architecture", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_list_arch_regex", "", "Regular expression with one backreference to extract package architecture string", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_list_command", CF_PATHRANGE, "Command to obtain a list of available packages", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_list_name_regex", "", "Regular expression with one backreference to extract package name string", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_list_update_command", "", "Command to update the list of available packages (if any)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("package_list_update_ifelapsed", CF_INTRANGE, "The ifelapsed locking time in between updates of the package list", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_list_version_regex", "", "Regular expression with one backreference to extract package version string", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_name_convention", "", "This is how the package manager expects the package to be referred to, e.g. $(name).$(arch)", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_name_regex", "", "Regular expression with one backreference to extract package name string", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_noverify_regex", "", "Regular expression to match verification failure output", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("package_noverify_returncode", CF_INTRANGE, "Integer return code indicating package verification failure", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_patch_arch_regex", "", "Regular expression with one backreference to extract update architecture string", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_patch_command", CF_PATHRANGE, "Command to update to the latest patch release of an installed package", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_patch_installed_regex", "", "Regular expression which matches packages that are already installed", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_patch_list_command", CF_PATHRANGE, "Command to obtain a list of available patches or updates", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_patch_name_regex", "", "Regular expression with one backreference to extract update name string", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_patch_version_regex", "", "Regular expression with one backreference to extract update version string", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_update_command", CF_PATHRANGE, "Command to update to the latest version a currently installed package", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_verify_command", CF_PATHRANGE, "Command to verify the correctness of an installed package", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_version_regex", "", "Regular expression with one backreference to extract package version string", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_multiline_start", "", "Regular expression which matches the start of a new package in multiline output", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBool("package_commands_useshell", "Whether to use shell for commands in this body. Default value: true", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_version_less_command", CF_PATHRANGE, "Command to check whether first supplied package version is less than second one", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_version_equal_command", CF_PATHRANGE, "Command to check whether first supplied package version is equal to second one", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

static const BodySyntax package_method_body = BodySyntaxNew("package_method", package_method_constraints, NULL, SYNTAX_STATUS_NORMAL);


static const ConstraintSyntax package_module_constraints[] =
{
    ConstraintSyntaxNewInt("query_installed_ifelapsed", CF_INTRANGE, "The ifelapsed locking time in between updates of the installed package list", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewInt("query_updates_ifelapsed", CF_INTRANGE, "The ifelapsed locking time in between updates of the available updates list", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("default_options", "", "Default options passed to package manager wrapper", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("interpreter", "", "Path to the interpreter to run the package manager wrapper with", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};
static const BodySyntax package_module_body = BodySyntaxNew("package_module", package_module_constraints, NULL, SYNTAX_STATUS_NORMAL);

static const ConstraintSyntax packages_constraints[] =
{
    ConstraintSyntaxNewStringList("package_architectures", "", "Select the architecture for package selection", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("package_method", &package_method_body, "Criteria for installation and verification", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("package_policy", "add,delete,reinstall,update,addupdate,patch,verify", "Criteria for package installation/upgrade on the current system. Default value: verify", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("package_select", ">,<,==,!=,>=,<=", "A criterion for first acceptable match relative to \"package_version\"", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("package_version", "", "Version reference point for determining promised version", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewOption("policy", "absent,present", "Criteria for package installation/upgrade on the current system", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewBody("package_module", &package_module_body, "Package manager used for package related operations", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("version", "", "Version reference point for determining promised version", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewString("architecture", "", "Architecture type of package", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("options", "", "Additional options passed to package manager", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewStringList("additional_packages", "", "Additional packages processed with given package promiser", SYNTAX_STATUS_NORMAL),
    ConstraintSyntaxNewNull()
};

const PromiseTypeSyntax CF_PACKAGES_PROMISE_TYPES[] =
{
    PromiseTypeSyntaxNew("agent", "packages", packages_constraints, NULL, SYNTAX_STATUS_NORMAL),
    PromiseTypeSyntaxNewNull()
};
