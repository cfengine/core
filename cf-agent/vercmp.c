/*
   Copyright (C) CFEngine AS

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
  versions of CFEngine, the applicable Commerical Open Source License
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
#include "pipes.h"
#include "misc_lib.h"
#include "env_context.h"

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

static VersionCmpResult RunCmpCommand(EvalContext *ctx, const char *command, const char *v1, const char *v2, Attributes a, Promise *pp)
{
    char expanded_command[CF_EXPANDSIZE];

    {
        EvalContextVariablePut(ctx, (VarRef) { NULL, "cf_pack_context", "v1" }, (Rval) { v1, RVAL_TYPE_SCALAR }, CF_DATA_TYPE_STRING);
        EvalContextVariablePut(ctx, (VarRef) { NULL, "cf_pack_context", "v2" }, (Rval) { v2, RVAL_TYPE_SCALAR }, CF_DATA_TYPE_STRING);
        ExpandScalar(ctx, "cf_pack_context", command, expanded_command);

        ScopeClear("cf_pack_context");
    }

    FILE *pfp = a.packages.package_commands_useshell ? cf_popen_sh(expanded_command, "w") : cf_popen(expanded_command, "w", true);

    if (pfp == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Can not start package version comparison command '%s'. (cf_popen: %s)",
             expanded_command, GetErrorStr());
        return VERCMP_ERROR;
    }

    Log(LOG_LEVEL_VERBOSE, "Executing '%s'", expanded_command);

    int retcode = cf_pclose(pfp);

    if (retcode == -1)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Error during package version comparison command execution '%s'. (cf_pclose: %s)",
            expanded_command, GetErrorStr());
        return VERCMP_ERROR;
    }

    return retcode == 0;
}

static VersionCmpResult CompareVersionsLess(EvalContext *ctx, const char *v1, const char *v2, Attributes a, Promise *pp)
{
    if (a.packages.package_version_less_command)
    {
        return RunCmpCommand(ctx, a.packages.package_version_less_command, v1, v2, a, pp);
    }
    else
    {
        return ComparePackageVersionsInternal(v1, v2, PACKAGE_VERSION_COMPARATOR_LT);
    }
}

static VersionCmpResult CompareVersionsEqual(EvalContext *ctx, const char *v1, const char *v2, Attributes a, Promise *pp)
{
    if (a.packages.package_version_equal_command)
    {
        return RunCmpCommand(ctx, a.packages.package_version_equal_command, v1, v2, a, pp);
    }
    else if (a.packages.package_version_less_command)
    {
        /* emulate v1 == v2 by !(v1 < v2) && !(v2 < v1)  */
        return AndResults(InvertResult(CompareVersionsLess(ctx, v1, v2, a, pp)),
                          InvertResult(CompareVersionsLess(ctx, v2, v1, a, pp)));
    }
    else
    {
        /* Built-in fallback */
        return ComparePackageVersionsInternal(v1, v2, PACKAGE_VERSION_COMPARATOR_EQ);
    }
}

VersionCmpResult CompareVersions(EvalContext *ctx, const char *v1, const char *v2, Attributes a, Promise *pp)
{
    VersionCmpResult result;
    const char *cmp_operator = "";
    switch (a.packages.package_select)
    {
    case PACKAGE_VERSION_COMPARATOR_EQ:
    case PACKAGE_VERSION_COMPARATOR_NONE:
        result = CompareVersionsEqual(ctx, v1, v2, a, pp);
        cmp_operator = "==";
        break;
    case PACKAGE_VERSION_COMPARATOR_NEQ:
        result = InvertResult(CompareVersionsEqual(ctx, v1, v2, a, pp));
        cmp_operator = "!=";
        break;
    case PACKAGE_VERSION_COMPARATOR_LT:
        result = CompareVersionsLess(ctx, v1, v2, a, pp);
        cmp_operator = "<";
        break;
    case PACKAGE_VERSION_COMPARATOR_GT:
        result = CompareVersionsLess(ctx, v2, v1, a, pp);
        cmp_operator = ">";
        break;
    case PACKAGE_VERSION_COMPARATOR_GE:
        result = InvertResult(CompareVersionsLess(ctx, v1, v2, a, pp));
        cmp_operator = ">=";
        break;
    case PACKAGE_VERSION_COMPARATOR_LE:
        result = InvertResult(CompareVersionsLess(ctx, v2, v1, a, pp));
        cmp_operator = "<=";
        break;
    default:
        ProgrammingError("Unexpected comparison value: %d", a.packages.package_select);
        break;
    }

    const char *text_result;
    switch (result)
    {
    case VERCMP_NO_MATCH:
        text_result = "no";
        break;
    case VERCMP_MATCH:
        text_result = "yes";
        break;
    default:
        text_result = "Incompatible version format. Can't decide";
        break;
    }

    Log(LOG_LEVEL_VERBOSE, "Checking whether package version %s %s %s: %s", v1, cmp_operator, v2, text_result);

    return result;
}
