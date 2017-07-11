/*
   Copyright 2017 Northern.tech AS

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/

#include <cf3.defs.h>

#include <files_names.h>
#include <vercmp_internal.h>
#include <rlist.h>

static void ParsePackageVersion(char *version, Rlist **num, Rlist **sep);

VersionCmpResult ComparePackageVersionsInternal(const char *v1, const char *v2, PackageVersionComparator cmp)
{
    Rlist *rp_pr, *rp_in;

    int result = true;
    int break_loop = false;
    int cmp_result;
    VersionCmpResult version_matched = VERCMP_NO_MATCH;

    Rlist *numbers_pr = NULL, *separators_pr = NULL;
    Rlist *numbers_in = NULL, *separators_in = NULL;

    ParsePackageVersion(CanonifyChar(v1, ','), &numbers_pr, &separators_pr);
    ParsePackageVersion(CanonifyChar(v2, ','), &numbers_in, &separators_in);

/* If the format of the version string doesn't match, we're already doomed */

    Log(LOG_LEVEL_VERBOSE, "Check for compatible versioning model in (%s,%s)", v1, v2);

    for (rp_pr = separators_pr, rp_in = separators_in; (rp_pr != NULL) && (rp_in != NULL);
         rp_pr = rp_pr->next, rp_in = rp_in->next)
    {
        if (strcmp(RlistScalarValue(rp_pr), RlistScalarValue(rp_in)) != 0)
        {
            result = false;
            break;
        }

        if ((rp_pr->next == NULL) && (rp_in->next == NULL))
        {
            result = true;
            break;
        }
    }

    if (result)
    {
        Log(LOG_LEVEL_VERBOSE, "Verified that versioning models are compatible");
    }
    else
    {
        Log(LOG_LEVEL_VERBOSE, "Versioning models for (%s,%s) were incompatible", v1, v2);
        version_matched = VERCMP_ERROR;
    }

    int version_equal = (strcmp(v2, v1) == 0);

    if (result)
    {
        for (rp_pr = numbers_pr, rp_in = numbers_in; (rp_pr != NULL) && (rp_in != NULL);
             rp_pr = rp_pr->next, rp_in = rp_in->next)
        {
            cmp_result = strcmp(RlistScalarValue(rp_pr), RlistScalarValue(rp_in));

            switch (cmp)
            {
            case PACKAGE_VERSION_COMPARATOR_EQ:
            case PACKAGE_VERSION_COMPARATOR_NONE:
                if (version_equal)
                {
                    version_matched = VERCMP_MATCH;
                }
                break;
            case PACKAGE_VERSION_COMPARATOR_NEQ:
                if (!version_equal)
                {
                    version_matched = VERCMP_MATCH;
                }
                break;
            case PACKAGE_VERSION_COMPARATOR_GT:
                if (cmp_result > 0)
                {
                    version_matched = VERCMP_MATCH;
                }
                else if (cmp_result < 0)
                {
                    break_loop = true;
                }
                break;
            case PACKAGE_VERSION_COMPARATOR_LT:
                if (cmp_result < 0)
                {
                    version_matched = VERCMP_MATCH;
                }
                else if (cmp_result > 0)
                {
                    break_loop = true;
                }
                break;
            case PACKAGE_VERSION_COMPARATOR_GE:
                if ((cmp_result > 0) || version_equal)
                {
                    version_matched = VERCMP_MATCH;
                }
                else if (cmp_result < 0)
                {
                    break_loop = true;
                }
                break;
            case PACKAGE_VERSION_COMPARATOR_LE:
                if ((cmp_result < 0) || version_equal)
                {
                    version_matched = VERCMP_MATCH;
                }
                else if (cmp_result > 0)
                {
                    break_loop = true;
                }
                break;
            default:
                break;
            }

            if ((version_matched == VERCMP_MATCH) || break_loop)
            {
                rp_pr = NULL;
                rp_in = NULL;
                break;
            }
        }

        if (rp_pr != NULL)
        {
            if ((cmp == PACKAGE_VERSION_COMPARATOR_GT) || (cmp == PACKAGE_VERSION_COMPARATOR_GE))
            {
                version_matched = VERCMP_MATCH;
            }
        }
        if (rp_in != NULL)
        {
            if ((cmp == PACKAGE_VERSION_COMPARATOR_LT) || (cmp == PACKAGE_VERSION_COMPARATOR_LE))
            {
                version_matched = VERCMP_MATCH;
            }
        }
    }

    RlistDestroy(numbers_pr);
    RlistDestroy(numbers_in);
    RlistDestroy(separators_pr);
    RlistDestroy(separators_in);

    return version_matched;
}

static void ParsePackageVersion(char *version, Rlist **num, Rlist **sep)
{
    char *sp, numeral[30], separator[2];

    if (version == NULL)
    {
        return;
    }

    for (sp = version; *sp != '\0'; sp++)
    {
        memset(numeral, 0, 30);
        memset(separator, 0, 2);

        /* Split in 2's complement */

        sscanf(sp, "%29[0-9a-zA-Z]", numeral);
        sp += strlen(numeral);

        /* Append to end up with left->right (major->minor) comparison */

        RlistAppendScalar(num, numeral);

        if (*sp == '\0')
        {
            return;
        }

        sscanf(sp, "%1[^0-9a-zA-Z]", separator);
        RlistAppendScalar(sep, separator);
    }
}
