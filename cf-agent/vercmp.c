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
#include "vercmp.h"
#include "vercmp_internal.h"

/* SetNewScope / DeleteScope */
#include "scope.h"
/* ExpandScalar */
#include "expand.h"
#include "vars.h"
#include "cfstream.h"
#include "pipes.h"
#include "logging.h"

static VersionCmpResult InvertResult(VersionCmpResult result)
{
    if (result == VERCMP_ERROR)
    {
        return VERCMP_ERROR;
    }
    else
    {
        return !result;
    }
}

static VersionCmpResult AndResults(VersionCmpResult lhs, VersionCmpResult rhs)
{
    if ((lhs == VERCMP_ERROR) || (rhs == VERCMP_ERROR))
    {
        return VERCMP_ERROR;
    }
    else
    {
        return lhs && rhs;
    }
}

static VersionCmpResult RunCmpCommand(const char *command, const char *v1, const char *v2, Attributes a, Promise *pp)
{
    char expanded_command[CF_EXPANDSIZE];

    SetNewScope("cf_pack_context");
    NewScalar("cf_pack_context", "v1", v1, DATA_TYPE_STRING);
    NewScalar("cf_pack_context", "v2", v2, DATA_TYPE_STRING);
    ExpandScalar(command, expanded_command);
    DeleteScope("cf_pack_context");

    FILE *pfp = a.packages.package_commands_useshell ? cf_popen_sh(expanded_command, "w") : cf_popen(expanded_command, "w");

    if (pfp == NULL)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "cf_popen", pp, a, "Can not start package version comparison command: %s", expanded_command);
        return VERCMP_ERROR;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Executing %s", expanded_command);

    int retcode = cf_pclose(pfp);

    if (retcode == -1)
    {
        cfPS(OUTPUT_LEVEL_ERROR, CF_FAIL, "cf_pclose", pp, a, "Error during package version comparison command execution: %s",
            expanded_command);
        return VERCMP_ERROR;
    }

    return retcode == 0;
}

static VersionCmpResult CompareVersionsLess(const char *v1, const char *v2, Attributes a, Promise *pp)
{
    if (a.packages.package_version_less_command)
    {
        return RunCmpCommand(a.packages.package_version_less_command, v1, v2, a, pp);
    }
    else
    {
        return ComparePackageVersionsInternal(v1, v2, PACKAGE_VERSION_COMPARATOR_GT) ? VERCMP_MATCH : VERCMP_NO_MATCH;
    }
}

static VersionCmpResult CompareVersionsEqual(const char *v1, const char *v2, Attributes a, Promise *pp)
{
    if (a.packages.package_version_equal_command)
    {
        return RunCmpCommand(a.packages.package_version_equal_command, v1, v2, a, pp);
    }
    else if (a.packages.package_version_less_command)
    {
        /* emulate v1 == v2 by !(v1 < v2) && !(v2 < v1)  */
        return AndResults(InvertResult(CompareVersionsLess(v1, v2, a, pp)),
                          InvertResult(CompareVersionsLess(v2, v1, a, pp)));
    }
    else
    {
        /* Built-in fallback */
        return ComparePackageVersionsInternal(v1, v2, PACKAGE_VERSION_COMPARATOR_EQ);
    }
}

VersionCmpResult CompareVersions(const char *v1, const char *v2, Attributes a, Promise *pp)
{
    switch (a.packages.package_select)
    {
    case PACKAGE_VERSION_COMPARATOR_EQ:
    case PACKAGE_VERSION_COMPARATOR_NONE: return CompareVersionsEqual(v1, v2, a, pp);
    case PACKAGE_VERSION_COMPARATOR_NEQ: return InvertResult(CompareVersionsEqual(v1, v2, a, pp));
    case PACKAGE_VERSION_COMPARATOR_LT: return CompareVersionsLess(v1, v2, a, pp);
    case PACKAGE_VERSION_COMPARATOR_GT: return CompareVersionsLess(v2, v1, a, pp);
    case PACKAGE_VERSION_COMPARATOR_GE: return InvertResult(CompareVersionsLess(v1, v2, a, pp));
    case PACKAGE_VERSION_COMPARATOR_LE: return InvertResult(CompareVersionsLess(v2, v1, a, pp));
    default: FatalError("Unexpected comparison value: %d", a.packages.package_select);
    }
}
