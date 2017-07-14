/*
   Copyright 2017 Northern.tech AS

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

#include <cf3.defs.h>
#include <vercmp.h>
#include <vercmp_internal.h>

#include <actuator.h>
#include <scope.h>
#include <expand.h>
#include <vars.h>
#include <pipes.h>
#include <misc_lib.h>
#include <eval_context.h>

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

static VersionCmpResult RunCmpCommand(EvalContext *ctx, const char *command, const char *v1, const char *v2, Attributes a,
                                      const Promise *pp, PromiseResult *result)
{
    Buffer *expanded_command = BufferNew();
    {
        VarRef *ref_v1 = VarRefParseFromScope("v1", PACKAGES_CONTEXT);
        EvalContextVariablePut(ctx, ref_v1, v1, CF_DATA_TYPE_STRING, "source=promise");

        VarRef *ref_v2 = VarRefParseFromScope("v2", PACKAGES_CONTEXT);
        EvalContextVariablePut(ctx, ref_v2, v2, CF_DATA_TYPE_STRING, "source=promise");

        ExpandScalar(ctx, NULL, PACKAGES_CONTEXT, command, expanded_command);

        EvalContextVariableRemove(ctx, ref_v1);
        VarRefDestroy(ref_v1);

        EvalContextVariableRemove(ctx, ref_v2);
        VarRefDestroy(ref_v2);
    }

    FILE *pfp = a.packages.package_commands_useshell ? cf_popen_sh(BufferData(expanded_command), "w") : cf_popen(BufferData(expanded_command), "w", true);

    if (pfp == NULL)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Can not start package version comparison command '%s'. (cf_popen: %s)",
             BufferData(expanded_command), GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        BufferDestroy(expanded_command);
        return VERCMP_ERROR;
    }

    Log(LOG_LEVEL_VERBOSE, "Executing '%s'", BufferData(expanded_command));

    int retcode = cf_pclose(pfp);

    if (retcode == -1)
    {
        cfPS(ctx, LOG_LEVEL_ERR, PROMISE_RESULT_FAIL, pp, a, "Error during package version comparison command execution '%s'. (cf_pclose: %s)",
            BufferData(expanded_command), GetErrorStr());
        *result = PromiseResultUpdate(*result, PROMISE_RESULT_FAIL);
        BufferDestroy(expanded_command);
        return VERCMP_ERROR;
    }

    BufferDestroy(expanded_command);

    return retcode == 0;
}

static VersionCmpResult CompareVersionsLess(EvalContext *ctx, const char *v1, const char *v2, Attributes a,
                                            const Promise *pp, PromiseResult *result)
{
    if (a.packages.package_version_less_command)
    {
        return RunCmpCommand(ctx, a.packages.package_version_less_command, v1, v2, a, pp, result);
    }
    else
    {
        return ComparePackageVersionsInternal(v1, v2, PACKAGE_VERSION_COMPARATOR_LT);
    }
}

static VersionCmpResult CompareVersionsEqual(EvalContext *ctx, const char *v1, const char *v2, Attributes a,
                                             const Promise *pp, PromiseResult *result)
{
    if (a.packages.package_version_equal_command)
    {
        return RunCmpCommand(ctx, a.packages.package_version_equal_command, v1, v2, a, pp, result);
    }
    else if (a.packages.package_version_less_command)
    {
        /* emulate v1 == v2 by !(v1 < v2) && !(v2 < v1)  */
        return AndResults(InvertResult(CompareVersionsLess(ctx, v1, v2, a, pp, result)),
                          InvertResult(CompareVersionsLess(ctx, v2, v1, a, pp, result)));
    }
    else
    {
        /* Built-in fallback */
        return ComparePackageVersionsInternal(v1, v2, PACKAGE_VERSION_COMPARATOR_EQ);
    }
}

VersionCmpResult CompareVersions(EvalContext *ctx, const char *v1, const char *v2, Attributes a,
                                 const Promise *pp, PromiseResult *result)
{
    VersionCmpResult cmp_result;

    switch (a.packages.package_select)
    {
    case PACKAGE_VERSION_COMPARATOR_EQ:
    case PACKAGE_VERSION_COMPARATOR_NONE:
        cmp_result = CompareVersionsEqual(ctx, v1, v2, a, pp, result);
        break;
    case PACKAGE_VERSION_COMPARATOR_NEQ:
        cmp_result = InvertResult(CompareVersionsEqual(ctx, v1, v2, a, pp, result));
        break;
    case PACKAGE_VERSION_COMPARATOR_LT:
        cmp_result = CompareVersionsLess(ctx, v1, v2, a, pp, result);
        break;
    case PACKAGE_VERSION_COMPARATOR_GT:
        cmp_result = CompareVersionsLess(ctx, v2, v1, a, pp, result);
        break;
    case PACKAGE_VERSION_COMPARATOR_GE:
        cmp_result = InvertResult(CompareVersionsLess(ctx, v1, v2, a, pp, result));
        break;
    case PACKAGE_VERSION_COMPARATOR_LE:
        cmp_result = InvertResult(CompareVersionsLess(ctx, v2, v1, a, pp, result));
        break;
    default:
        ProgrammingError("Unexpected comparison value: %d", a.packages.package_select);
        break;
    }

    const char *text_result;
    switch (cmp_result)
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

    Log(LOG_LEVEL_VERBOSE, "CompareVersions: Checked whether package version %s %s %s: %s",
        v1, PackageVersionComparatorToString(a.packages.package_select), v2, text_result);

    return cmp_result;
}

const char* PackageVersionComparatorToString(const PackageVersionComparator pvc)
{
    switch (pvc)
    {
    case PACKAGE_VERSION_COMPARATOR_EQ:   return "==";
    case PACKAGE_VERSION_COMPARATOR_NONE: return "==";
    case PACKAGE_VERSION_COMPARATOR_NEQ:  return "!=";
    case PACKAGE_VERSION_COMPARATOR_LT:   return "<";
    case PACKAGE_VERSION_COMPARATOR_GT:   return ">";
    case PACKAGE_VERSION_COMPARATOR_GE:   return ">=";
    case PACKAGE_VERSION_COMPARATOR_LE:   return "<=";

    default:
        ProgrammingError("Unexpected PackageVersionComparator value: %d", pvc);
    }

    return NULL;
}
