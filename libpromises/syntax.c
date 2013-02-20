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

#include "syntax.h"

#include "json.h"
#include "files_names.h"
#include "mod_files.h"
#include "item_lib.h"
#include "conversion.h"
#include "reporting.h"
#include "expand.h"
#include "matching.h"
#include "vars.h"
#include "fncall.h"
#include "string_lib.h"
#include "logging.h"
#include "misc_lib.h"
#include "rlist.h"

#include <assert.h>

static int CheckParseString(const char *lv, const char *s, const char *range);
static void CheckParseInt(const char *lv, const char *s, const char *range);
static void CheckParseReal(const char *lv, const char *s, const char *range);
static void CheckParseRealRange(const char *lval, const char *s, const char *range);
static void CheckParseIntRange(const char *lval, const char *s, const char *range);
static void CheckParseOpts(const char *lv, const char *s, const char *range);
static void CheckFnCallType(const char *lval, const char *s, DataType dtype, const char *range);


/*********************************************************/

SubTypeSyntax SubTypeSyntaxLookup(const char *bundle_type, const char *subtype_name)
{
    for (int i = 0; i < CF3_MODULES; i++)
    {
        const SubTypeSyntax *syntax = NULL;

        if ((syntax = CF_ALL_SUBTYPES[i]) == NULL)
        {
            continue;
        }

        for (int j = 0; syntax[j].bundle_type != NULL; j++)
        {
            if (StringSafeEqual(subtype_name, syntax[j].subtype) &&
                    (StringSafeEqual(bundle_type, syntax[j].bundle_type) ||
                     StringSafeEqual("*", syntax[j].bundle_type)))
            {
                return syntax[j];
            }
        }
    }

    return (SubTypeSyntax) { NULL, NULL, NULL };
}

/****************************************************************************/

DataType ExpectedDataType(char *lvalname)
{
    int i, j, k, l;
    const BodySyntax *bs, *bs2;
    const SubTypeSyntax *ss;

    for (i = 0; i < CF3_MODULES; i++)
    {
        if ((ss = CF_ALL_SUBTYPES[i]) == NULL)
        {
            continue;
        }

        for (j = 0; ss[j].subtype != NULL; j++)
        {
            if ((bs = ss[j].bs) == NULL)
            {
                continue;
            }

            for (k = 0; bs[k].range != NULL; k++)
            {
                if (strcmp(lvalname, bs[k].lval) == 0)
                {
                    return bs[k].dtype;
                }
            }

            for (k = 0; bs[k].range != NULL; k++)
            {
                if (bs[k].dtype == DATA_TYPE_BODY)
                {
                    bs2 = (const BodySyntax *) (bs[k].range);

                    if (bs2 == NULL || bs2 == (void *) CF_BUNDLE)
                    {
                        continue;
                    }

                    for (l = 0; bs2[l].dtype != DATA_TYPE_NONE; l++)
                    {
                        if (strcmp(lvalname, bs2[l].lval) == 0)
                        {
                            return bs2[l].dtype;
                        }
                    }
                }
            }

        }
    }

    return DATA_TYPE_NONE;
}

/*********************************************************/

void CheckConstraint(char *type, char *ns, char *name, char *lval, Rval rval, SubTypeSyntax ss)
{
    int lmatch = false;
    int i, l, allowed = false;
    const BodySyntax *bs;
    char output[CF_BUFSIZE];

    CfDebug("CheckConstraint(%s,%s,", type, lval);

    if (DEBUG)
    {
        RvalShow(stdout, rval);
    }

    CfDebug(")\n");

    if (ss.subtype != NULL)     /* In a bundle */
    {
        if (strcmp(ss.subtype, type) == 0)
        {
            CfDebug("Found type %s's body syntax\n", type);

            bs = ss.bs;

            for (l = 0; bs[l].lval != NULL; l++)
            {
                CfDebug("CMP-bundle # (%s,%s)\n", lval, bs[l].lval);

                if (strcmp(lval, bs[l].lval) == 0)
                {
                    /* If we get here we have found the lval and it is valid
                       for this subtype */

                    lmatch = true;
                    CfDebug("Matched syntatically correct bundle (lval,rval) item = (%s) to its rval\n", lval);

                    if (bs[l].dtype == DATA_TYPE_BODY)
                    {
                        CfDebug("Constraint syntax ok, but definition of body is elsewhere %s=%c\n", lval, rval.type);
                        return;
                    }
                    else if (bs[l].dtype == DATA_TYPE_BUNDLE)
                    {
                        CfDebug("Constraint syntax ok, but definition of relevant bundle is elsewhere %s=%c\n", lval,
                                rval.type);
                        return;
                    }
                    else
                    {
                        CheckConstraintTypeMatch(lval, rval, bs[l].dtype, (char *) (bs[l].range), 0);
                        return;
                    }
                }
            }
        }
    }

/* Now check the functional modules - extra level of indirection
   Note that we only check body attributes relative to promise type.
   We can enter any promise types in any bundle, but only recognized
   types will be dealt with. */

    for (i = 0; CF_COMMON_BODIES[i].lval != NULL; i++)
    {
        CfDebug("CMP-common # %s,%s\n", lval, CF_COMMON_BODIES[i].lval);

        if (strcmp(lval, CF_COMMON_BODIES[i].lval) == 0)
        {
            CfDebug("Found a match for lval %s in the common constraint attributes\n", lval);
            return;
        }
    }

    for (i = 0; CF_COMMON_EDITBODIES[i].lval != NULL; i++)
    {
        CfDebug("CMP-common # %s,%s\n", lval, CF_COMMON_EDITBODIES[i].lval);

        if (strcmp(lval, CF_COMMON_EDITBODIES[i].lval) == 0)
        {
            CfDebug("Found a match for lval %s in the common edit_line constraint attributes\n", lval);
            return;
        }
    }

    for (i = 0; CF_COMMON_XMLBODIES[i].lval != NULL; i++)
    {
        CfDebug("CMP-common # %s,%s\n", lval, CF_COMMON_XMLBODIES[i].lval);

        if (strcmp(lval, CF_COMMON_XMLBODIES[i].lval) == 0)
        {
            CfDebug("Found a match for lval %s in the common edit_xml constraint attributes\n", lval);
            return;
        }
    }

    
// Now check if it is in the common list...

    if (!lmatch || !allowed)
    {
        snprintf(output, CF_BUFSIZE, "Constraint lvalue \'%s\' is not allowed in bundle category \'%s\'", lval, type);
        ReportError(output);
    }
}

/******************************************************************************************/

int LvalWantsBody(char *stype, char *lval)
{
    int i, j, l;
    const SubTypeSyntax *ss;
    const BodySyntax *bs;

    for (i = 0; i < CF3_MODULES; i++)
    {
        if ((ss = CF_ALL_SUBTYPES[i]) == NULL)
        {
            continue;
        }

        for (j = 0; ss[j].subtype != NULL; j++)
        {
            if ((bs = ss[j].bs) == NULL)
            {
                continue;
            }

            if (strcmp(ss[j].subtype, stype) != 0)
            {
                continue;
            }

            for (l = 0; bs[l].range != NULL; l++)
            {
                if (strcmp(bs[l].lval, lval) == 0)
                {
                    if (bs[l].dtype == DATA_TYPE_BODY)
                    {
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }
            }
        }
    }

    return false;
}

/******************************************************************************************/

void CheckSelection(char *type, char *name, char *lval, Rval rval)
{
    int lmatch = false;
    int i, j, k, l;
    const SubTypeSyntax *ss;
    const BodySyntax *bs, *bs2;
    char output[CF_BUFSIZE];

    CfDebug("CheckSelection(%s,%s,", type, lval);

    if (DEBUG)
    {
        RvalShow(stdout, rval);
    }

    CfDebug(")\n");

/* Check internal control bodies etc */

    for (i = 0; CF_ALL_BODIES[i].subtype != NULL; i++)
    {
        if (strcmp(CF_ALL_BODIES[i].subtype, name) == 0 && strcmp(type, CF_ALL_BODIES[i].bundle_type) == 0)
        {
            CfDebug("Found matching a body matching (%s,%s)\n", type, name);

            bs = CF_ALL_BODIES[i].bs;

            for (l = 0; bs[l].lval != NULL; l++)
            {
                if (strcmp(lval, bs[l].lval) == 0)
                {
                    CfDebug("Matched syntatically correct body (lval) item = (%s)\n", lval);

                    if (bs[l].dtype == DATA_TYPE_BODY)
                    {
                        CfDebug("Constraint syntax ok, but definition of body is elsewhere\n");
                        return;
                    }
                    else if (bs[l].dtype == DATA_TYPE_BUNDLE)
                    {
                        CfDebug("Constraint syntax ok, but definition of bundle is elsewhere\n");
                        return;
                    }
                    else
                    {
                        CheckConstraintTypeMatch(lval, rval, bs[l].dtype, (char *) (bs[l].range), 0);
                        return;
                    }
                }
            }

        }
    }

/* Now check the functional modules - extra level of indirection */

    for (i = 0; i < CF3_MODULES; i++)
    {
        CfDebug("Trying function module %d for matching lval %s\n", i, lval);

        if ((ss = CF_ALL_SUBTYPES[i]) == NULL)
        {
            continue;
        }

        for (j = 0; ss[j].subtype != NULL; j++)
        {
            if ((bs = ss[j].bs) == NULL)
            {
                continue;
            }

            CfDebug("\nExamining subtype %s\n", ss[j].subtype);

            for (l = 0; bs[l].range != NULL; l++)
            {
                if (bs[l].dtype == DATA_TYPE_BODY)
                {
                    bs2 = (const BodySyntax *) (bs[l].range);

                    if (bs2 == NULL || bs2 == (void *) CF_BUNDLE)
                    {
                        continue;
                    }

                    for (k = 0; bs2[k].dtype != DATA_TYPE_NONE; k++)
                    {
                        /* Either module defined or common */

                        if (strcmp(ss[j].subtype, type) == 0 && strcmp(ss[j].subtype, "*") != 0)
                        {
                            snprintf(output, CF_BUFSIZE, "lval %s belongs to promise type \'%s:\' but this is '\%s\'\n",
                                     lval, ss[j].subtype, type);
                            ReportError(output);
                            return;
                        }

                        if (strcmp(lval, bs2[k].lval) == 0)
                        {
                            CfDebug("Matched\n");
                            CheckConstraintTypeMatch(lval, rval, bs2[k].dtype, (char *) (bs2[k].range), 0);
                            return;
                        }
                    }
                }
            }
        }
    }

    if (!lmatch)
    {
        snprintf(output, CF_BUFSIZE, "Constraint lvalue \"%s\" is not allowed in \'%s\' constraint body", lval, type);
        ReportError(output);
    }
}

/****************************************************************************/
/* Level 1                                                                  */
/****************************************************************************/

void CheckConstraintTypeMatch(const char *lval, Rval rval, DataType dt, const char *range, int level)
{
    Rlist *rp;
    Item *checklist;
    char output[CF_BUFSIZE];

    if (rval.item == NULL)
    {
        return;
    }

    CfDebug(" ------------------------------------------------\n");

    if (dt == DATA_TYPE_BUNDLE || dt == DATA_TYPE_BODY)
    {
        CfDebug(" - Checking inline constraint/arg %s[%s] => mappedval (bundle/body)\n", lval, CF_DATATYPES[dt]);
    }
    else
    {
        CfDebug(" - Checking inline constraint/arg %s[%s] => mappedval (%c) %s\n", lval, CF_DATATYPES[dt], rval.type,
                range);
    }
    CfDebug(" ------------------------------------------------\n");

/* Get type of lval */

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        switch (dt)
        {
        case DATA_TYPE_STRING_LIST:
        case DATA_TYPE_INT_LIST:
        case DATA_TYPE_REAL_LIST:
        case DATA_TYPE_CONTEXT_LIST:
        case DATA_TYPE_OPTION_LIST:
            if (level == 0)
            {
                snprintf(output, CF_BUFSIZE, " !! Type mismatch -- rhs is a scalar, but lhs (%s) is not a scalar type",
                         CF_DATATYPES[dt]);
                ReportError(output);
            }
            break;
        default:
            /* Only lists are incompatible with scalars */
            break;
        }
        break;

    case RVAL_TYPE_LIST:

        switch (dt)
        {
        case DATA_TYPE_STRING_LIST:
        case DATA_TYPE_INT_LIST:
        case DATA_TYPE_REAL_LIST:
        case DATA_TYPE_CONTEXT_LIST:
        case DATA_TYPE_OPTION_LIST:
            break;
        default:
            snprintf(output, CF_BUFSIZE, "!! Type mismatch -- rhs is a list, but lhs (%s) is not a list type",
                     CF_DATATYPES[dt]);
            ReportError(output);
            break;
        }

        for (rp = (Rlist *) rval.item; rp != NULL; rp = rp->next)
        {
            CheckConstraintTypeMatch(lval, (Rval) {rp->item, rp->type}, dt, range, 1);
        }

        return;

    case RVAL_TYPE_FNCALL:

        /* Fn-like objects are assumed to be parameterized bundles in these... */

        checklist = SplitString("bundlesequence,edit_line,edit_xml,usebundle,service_bundle", ',');

        if (!IsItemIn(checklist, lval))
        {
            CheckFnCallType(lval, ((FnCall *) rval.item)->name, dt, range);
        }

        DeleteItemList(checklist);
        return;

    default:
        break;
    }

/* If we get here, we have a literal scalar type */

    switch (dt)
    {
    case DATA_TYPE_STRING:
    case DATA_TYPE_STRING_LIST:
        CheckParseString(lval, (const char *) rval.item, range);
        break;

    case DATA_TYPE_INT:
    case DATA_TYPE_INT_LIST:
        CheckParseInt(lval, (const char *) rval.item, range);
        break;

    case DATA_TYPE_REAL:
    case DATA_TYPE_REAL_LIST:
        CheckParseReal(lval, (const char *) rval.item, range);
        break;

    case DATA_TYPE_BODY:
    case DATA_TYPE_BUNDLE:
        CfDebug("Nothing to check for body reference\n");
        break;

    case DATA_TYPE_OPTION:
    case DATA_TYPE_OPTION_LIST:
        CheckParseOpts(lval, (const char *) rval.item, range);
        break;

    case DATA_TYPE_CONTEXT:
    case DATA_TYPE_CONTEXT_LIST:
        CheckParseClass(lval, (const char *) rval.item, range);
        break;

    case DATA_TYPE_INT_RANGE:
        CheckParseIntRange(lval, (const char *) rval.item, range);
        break;

    case DATA_TYPE_REAL_RANGE:
        CheckParseRealRange(lval, (char *) rval.item, range);
        break;

    default:
        ProgrammingError("Unknown (unhandled) datatype for lval = %s (CheckConstraintTypeMatch)\n", lval);
        break;
    }

    CfDebug("end CheckConstraintTypeMatch---------\n");
}

/****************************************************************************/

DataType StringDataType(const char *scopeid, const char *string)
{
    DataType dtype;
    Rval rval;
    int islist = false;
    char var[CF_BUFSIZE];

    CfDebug("StringDataType(%s)\n", string);

/*-------------------------------------------------------
What happens if we embed vars in a literal string
       "$(list)withending" - a list?
       "$(list1)$(list2)"  - not a simple list
Disallow these manual concatenations as ambiguous.
Demand this syntax to work around

vars:

   "listvar" slist => EmbellishList("prefix$(list)suffix");
---------------------------------------------------------*/

    var[0] = '\0';

    if (*string == '$')
    {
        if (ExtractInnerCf3VarString(string, var))
        {
            if ((dtype = GetVariable(scopeid, var, &rval)) != DATA_TYPE_NONE)
            {
                if (rval.type == RVAL_TYPE_LIST)
                {
                    if (!islist)
                    {
                        islist = true;
                    }
                    else
                    {
                        islist = false;
                    }
                }
            }

            if (strlen(var) == strlen(string))
            {
                /* Can trust variables own datatype  */
                return dtype;
            }
            else
            {
                /* Must force non-pure substitution to be generic type CF_SCALAR.cf_str */
                return DATA_TYPE_STRING;
            }
        }
    }

    return DATA_TYPE_STRING;
}

/****************************************************************************/
/* Level 1                                                                  */
/****************************************************************************/

static int CheckParseString(const char *lval, const char *s, const char *range)
{
    char output[CF_BUFSIZE];

    CfDebug("\nCheckParseString(%s => %s/%s)\n", lval, s, range);

    if (s == NULL)
    {
        return true;
    }

    if (strlen(range) == 0)
    {
        return true;
    }

    if (IsNakedVar(s, '@') || IsNakedVar(s, '$'))
    {
        CfDebug("Validation: Unable to verify variable expansion of %s at this stage\n", s);
        return false;
    }

/* Deal with complex strings as special cases */

    if (strcmp(lval, "mode") == 0 || strcmp(lval, "search_mode") == 0)
    {
        mode_t plus, minus;

        if (!ParseModeString(s, &plus, &minus))
        {
            snprintf(output, CF_BUFSIZE, "Error parsing Unix permission string %s)", s);
            ReportError(output);
            return false;
        }
    }

    if (FullTextMatch(range, s))
    {
        return true;
    }

    if (IsCf3VarString(s))
    {
        CfDebug("Validation: Unable to verify syntax of %s due to variable expansion at this stage\n", s);
    }
    else
    {
        snprintf(output, CF_BUFSIZE,
                 "Scalar item in %s => { %s } in rvalue is out of bounds (value should match pattern %s)", lval, s,
                 range);
        ReportError(output);
        return false;
    }

    return true;
}

/****************************************************************************/

int CheckParseClass(const char *lval, const char *s, const char *range)
{
    char output[CF_BUFSIZE];

    if (s == NULL)
    {
        return false;
    }

    CfDebug("\nCheckParseClass(%s => %s/%s)\n", lval, s, range);

    if (strlen(range) == 0)
    {
        return true;
    }

    if (FullTextMatch(range, s))
    {
        return true;
    }

    snprintf(output, CF_BUFSIZE, "Class item on rhs of lval \'%s\' given as { %s } is out of bounds (should match %s)",
             lval, s, range);
    ReportError(output);
    return false;
}

/****************************************************************************/

static void CheckParseInt(const char *lval, const char *s, const char *range)
{
    Item *split;
    int n;
    long max = CF_LOWINIT, min = CF_HIGHINIT, val;
    char output[CF_BUFSIZE];

/* Numeric types are registered by range separated by comma str "min,max" */
    CfDebug("\nCheckParseInt(%s => %s/%s)\n", lval, s, range);

    if (s == NULL)
    {
        return;
    }

    split = SplitString(range, ',');

    if ((n = ListLen(split)) != 2)
    {
        FatalError("INTERN: format specifier for int rvalues is not ok for lval %s - got %d items", lval, n);
    }

    sscanf(split->name, "%ld", &min);

    if (strcmp(split->next->name, "inf") == 0)
    {
        max = CF_INFINITY;
    }
    else
    {
        sscanf(split->next->name, "%ld", &max);
    }

    DeleteItemList(split);

    if (min == CF_HIGHINIT || max == CF_LOWINIT)
    {
        FatalError("INTERN: could not parse format specifier for int rvalues for lval %s", lval);
    }

    if (IsCf3VarString(s))
    {
        CfDebug("Validation: Unable to verify syntax of int \'%s\' due to variable expansion at this stage\n", s);
        return;
    }

    val = Str2Int(s);

    if (val == CF_NOINT)
    {
        snprintf(output, CF_BUFSIZE, "Int item on rhs of lval \'%s\' given as \'%s\' could not be parsed", lval, s);
        ReportError(output);
        return;
    }

    if (val > max || val < min)
    {
        snprintf(output, CF_BUFSIZE,
                 "Int item on rhs of lval \'%s\' given as {%s => %ld} is out of bounds (should be in [%s])", lval, s,
                 val, range);
        ReportError(output);
        return;
    }

    CfDebug("CheckParseInt - syntax verified\n\n");
}

/****************************************************************************/

static void CheckParseIntRange(const char *lval, const char *s, const char *range)
{
    Item *split, *ip, *rangep;
    int n;
    long max = CF_LOWINIT, min = CF_HIGHINIT, val;
    char output[CF_BUFSIZE];

    if (s == NULL)
    {
        return;
    }

/* Numeric types are registered by range separated by comma str "min,max" */
    CfDebug("\nCheckParseIntRange(%s => %s/%s)\n", lval, s, range);

    if (*s == '[' || *s == '(')
    {
        ReportError("Range specification should not be enclosed in brackets - just \"a,b\"");
        return;
    }

    split = SplitString(range, ',');

    if ((n = ListLen(split)) != 2)
    {
        FatalError("INTERN:format specifier %s for irange rvalues is not ok for lval %s - got %d items", range, lval,
                   n);
    }

    sscanf(split->name, "%ld", &min);

    if (strcmp(split->next->name, "inf") == 0)
    {
        max = CF_INFINITY;
    }
    else
    {
        sscanf(split->next->name, "%ld", &max);
    }

    DeleteItemList(split);

    if (min == CF_HIGHINIT || max == CF_LOWINIT)
    {
        FatalError("INTERN: could not parse irange format specifier for int rvalues for lval %s", lval);
    }

    if (IsCf3VarString(s))
    {
        CfDebug("Validation: Unable to verify syntax of int \'%s\' due to variable expansion at this stage\n", s);
        return;
    }

    rangep = SplitString(s, ',');

    if ((n = ListLen(rangep)) != 2)
    {
        snprintf(output, CF_BUFSIZE,
                 "Int range format specifier for lval %s should be of form \"a,b\" but got %d items", lval, n);
        ReportError(output);
        DeleteItemList(rangep);
        return;
    }

    for (ip = rangep; ip != NULL; ip = ip->next)
    {
        val = Str2Int(ip->name);

        if (val > max || val < min)
        {
            snprintf(output, CF_BUFSIZE,
                     "Int range item on rhs of lval \'%s\' given as {%s => %ld} is out of bounds (should be in [%s])",
                     lval, s, val, range);
            ReportError(output);
            DeleteItemList(rangep);
            return;
        }
    }

    DeleteItemList(rangep);

    CfDebug("CheckParseIntRange - syntax verified\n\n");
}

/****************************************************************************/

static void CheckParseReal(const char *lval, const char *s, const char *range)
{
    Item *split;
    double max = (double) CF_LOWINIT, min = (double) CF_HIGHINIT, val;
    int n;
    char output[CF_BUFSIZE];

    CfDebug("\nCheckParseReal(%s => %s/%s)\n", lval, s, range);

    if (s == NULL)
    {
        return;
    }

    if (strcmp(s, "inf") == 0)
    {
        ReportError("keyword \"inf\" has an integer value, cannot be used as real");
        return;
    }

    if (IsCf3VarString(s))
    {
        CfDebug("Validation: Unable to verify syntax of real %s due to variable expansion at this stage\n", s);
        return;
    }

/* Numeric types are registered by range separated by comma str "min,max" */

    split = SplitString(range, ',');

    if ((n = ListLen(split)) != 2)
    {
        FatalError("INTERN:format specifier for real rvalues is not ok for lval %s - %d items", lval, n);
    }

    sscanf(split->name, "%lf", &min);
    sscanf(split->next->name, "%lf", &max);
    DeleteItemList(split);

    if (min == CF_HIGHINIT || max == CF_LOWINIT)
    {
        FatalError("INTERN:could not parse format specifier for int rvalues for lval %s", lval);
    }

    val = Str2Double(s);

    if (val > max || val < min)
    {
        snprintf(output, CF_BUFSIZE,
                 "Real item on rhs of lval \'%s\' give as {%s => %.3lf} is out of bounds (should be in [%s])", lval, s,
                 val, range);
        ReportError(output);
    }

    CfDebug("CheckParseReal - syntax verified\n\n");
}

/****************************************************************************/

static void CheckParseRealRange(const char *lval, const char *s, const char *range)
{
    Item *split, *rangep, *ip;
    double max = (double) CF_LOWINIT, min = (double) CF_HIGHINIT, val;
    int n;
    char output[CF_BUFSIZE];

    if (s == NULL)
    {
        return;
    }

    CfDebug("\nCheckParseRealRange(%s => %s/%s)\n", lval, s, range);

    if (*s == '[' || *s == '(')
    {
        ReportError("Range specification should not be enclosed in brackets - just \"a,b\"");
        return;
    }

    if (strcmp(s, "inf") == 0)
    {
        ReportError("keyword \"inf\" has an integer value, cannot be used as real");
        return;
    }

    if (IsCf3VarString(s))
    {
        CfDebug("Validation: Unable to verify syntax of real %s due to variable expansion at this stage\n", s);
        return;
    }

/* Numeric types are registered by range separated by comma str "min,max" */

    split = SplitString(range, ',');

    if ((n = ListLen(split)) != 2)
    {
        FatalError("INTERN:format specifier for real rvalues is not ok for lval %s - %d items", lval, n);
    }

    sscanf(split->name, "%lf", &min);
    sscanf(split->next->name, "%lf", &max);
    DeleteItemList(split);

    if (min == CF_HIGHINIT || max == CF_LOWINIT)
    {
        FatalError("INTERN:could not parse format specifier for int rvalues for lval %s", lval);
    }

    rangep = SplitString(s, ',');

    if ((n = ListLen(rangep)) != 2)
    {
        snprintf(output, CF_BUFSIZE,
                 "Real range format specifier in lval %s should be of form \"a,b\" but got %d items", lval, n);
        ReportError(output);
        DeleteItemList(rangep);
        return;
    }

    for (ip = rangep; ip != NULL; ip = ip->next)
    {
        val = Str2Double(ip->name);

        if (val > max || val < min)
        {
            snprintf(output, CF_BUFSIZE,
                     "Real range item on rhs of lval \'%s\' give as {%s => %.3lf} is out of bounds (should be in [%s])",
                     lval, s, val, range);
            ReportError(output);
        }
    }

    DeleteItemList(rangep);

    CfDebug("CheckParseRealRange - syntax verified\n\n");
}

/****************************************************************************/

static void CheckParseOpts(const char *lval, const char *s, const char *range)
{
    Item *split;
    int err = false;
    char output[CF_BUFSIZE];

/* List/menu types are separated by comma str "a,b,c,..." */

    CfDebug("\nCheckParseOpts(%s => %s/%s)\n", lval, s, range);

    if (s == NULL)
    {
        return;
    }

    if (IsNakedVar(s, '@') || IsNakedVar(s, '$'))
    {
        CfDebug("Validation: Unable to verify variable expansion of %s at this stage\n", s);
        return;
    }

    split = SplitString(range, ',');

    if (!IsItemIn(split, s))
    {
        snprintf(output, CF_BUFSIZE,
                 "Selection on rhs of lval \'%s\' given as \'%s\' is out of bounds, should be in [%s]", lval, s, range);
        ReportError(output);
        err = true;
    }

    DeleteItemList(split);

    if (!err)
    {
        CfDebug("CheckParseOpts - syntax verified\n\n");
    }
}

/****************************************************************************/

int CheckParseVariableName(const char *name)
{
    const char *reserved[] = { "promiser", "handle", "promise_filename", "promise_linenumber", "this", NULL };
    char scopeid[CF_MAXVARSIZE], vlval[CF_MAXVARSIZE];
    int count = 0, level = 0;

    if (IsStrIn(name, reserved))
    {
        return false;
    }

    scopeid[0] = '\0';

    if (strchr(name, '.'))
    {
        for (const char *sp = name; *sp != '\0'; sp++)
        {
            switch (*sp)
            {
            case '.':
                if (++count > 1 && level != 1)
                {
                    return false;
                }
                break;

            case '[':
                level++;
                break;

            case ']':
                level--;
                break;

            default:
                break;
            }

            if (level > 1)
            {
                yyerror("Too many levels of [] reserved for array use");
                return false;
            }

        }

        if (count == 1)
        {
            sscanf(name, "%[^.].%s", scopeid, vlval);

            if (strlen(scopeid) == 0 || strlen(vlval) == 0)
            {
                return false;
            }
        }
    }

    return true;
}

/****************************************************************************/

bool IsDataType(const char *s)
{
    return strcmp(s, "string") == 0 || strcmp(s, "slist") == 0 ||
        strcmp(s, "int") == 0 || strcmp(s, "ilist") == 0 || strcmp(s, "real") == 0 || strcmp(s, "rlist") == 0;
}

/****************************************************************************/

static void CheckFnCallType(const char *lval, const char *s, DataType dtype, const char *range)
{
    DataType dt;
    char output[CF_BUFSIZE];
    const FnCallType *fn;

    CfDebug("CheckFnCallType(%s => %s/%s)\n", lval, s, range);

    if (s == NULL)
    {
        return;
    }

    fn = FnCallTypeGet(s);

    if (fn)
    {
        dt = fn->dtype;

        if (dtype != dt)
        {
            /* Ok to allow fn calls of correct element-type in lists */

            if (dt == DATA_TYPE_STRING && dtype == DATA_TYPE_STRING_LIST)
            {
                return;
            }

            if (dt == DATA_TYPE_INT && dtype == DATA_TYPE_INT_LIST)
            {
                return;
            }

            if (dt == DATA_TYPE_REAL && dtype == DATA_TYPE_REAL_LIST)
            {
                return;
            }

            if (dt == DATA_TYPE_OPTION && dtype == DATA_TYPE_OPTION_LIST)
            {
                return;
            }

            if (dt == DATA_TYPE_CONTEXT && dtype == DATA_TYPE_CONTEXT_LIST)
            {
                return;
            }

            snprintf(output, CF_BUFSIZE, "function %s() returns type %s but lhs requires %s", s, CF_DATATYPES[dt],
                     CF_DATATYPES[dtype]);
            ReportError(output);
            return;
        }
        else
        {
            return;
        }
    }
    else
    {
        snprintf(output, CF_BUFSIZE, "Unknown built-in function %s()", s);
        ReportError(output);
    }
}

/****************************************************************************/

static char *PCREStringToJsonString(const char *pcre)
{
    const char *src = pcre;
    char *dst = NULL;
    char *json = xcalloc((2 * strlen(pcre)) + 1, sizeof(char));

    for (dst = json; *src != '\0'; src++)
    {
        if (*src == '\"')
        {
            dst += sprintf(dst, "\\\"");
        }
        else if (*src == '\'')
        {
            dst += sprintf(dst, "\\\'");
        }
        else if (*src == '\\')
        {
            dst += sprintf(dst, "\\\\");
        }
        else
        {
            *dst = *src;
            dst++;
        }
    }

    *dst = '\0';

    return json;
}

/****************************************************************************/

static JsonElement *ExportAttributesSyntaxAsJson(const BodySyntax attributes[])
{
    JsonElement *json = JsonObjectCreate(10);
    int i = 0;

    if (attributes == NULL)
    {
        return json;
    }

    for (i = 0; attributes[i].lval != NULL; i++)
    {
        if (attributes[i].range == CF_BUNDLE)
        {
            /* TODO: must handle edit_line somehow */
            continue;
        }
        else if (attributes[i].dtype == DATA_TYPE_BODY)
        {
            JsonElement *json_attributes = ExportAttributesSyntaxAsJson((const BodySyntax *) attributes[i].range);

            JsonObjectAppendObject(json, attributes[i].lval, json_attributes);
        }
        else
        {
            JsonElement *attribute = JsonObjectCreate(10);

            JsonObjectAppendString(attribute, "datatype", CF_DATATYPES[attributes[i].dtype]);

            if (strlen(attributes[i].range) == 0)
            {
                JsonObjectAppendString(attribute, "pcre-range", ".*");
            }
            else if (attributes[i].dtype == DATA_TYPE_OPTION || attributes[i].dtype == DATA_TYPE_OPTION_LIST)
            {
                JsonElement *options = JsonArrayCreate(10);
                char options_buffer[CF_BUFSIZE];
                char *option = NULL;

                strcpy(options_buffer, attributes[i].range);
                for (option = strtok(options_buffer, ","); option != NULL; option = strtok(NULL, ","))
                {
                    JsonArrayAppendString(options, option);
                }

                JsonObjectAppendArray(attribute, "pcre-range", options);
            }
            else
            {
                char *pcre_range = PCREStringToJsonString(attributes[i].range);

                JsonObjectAppendString(attribute, "pcre-range", pcre_range);
            }

            JsonObjectAppendObject(json, attributes[i].lval, attribute);
        }
    }

    return json;
}

/****************************************************************************/

static JsonElement *ExportBundleTypeSyntaxAsJson(const char *bundle_type)
{
    JsonElement *json = JsonObjectCreate(10);
    const SubTypeSyntax *st;
    int i = 0, j = 0;

    for (i = 0; i < CF3_MODULES; i++)
    {
        st = CF_ALL_SUBTYPES[i];

        for (j = 0; st[j].bundle_type != NULL; j++)
        {
            if (strcmp(bundle_type, st[j].bundle_type) == 0 || strcmp("*", st[j].bundle_type) == 0)
            {
                JsonElement *attributes = ExportAttributesSyntaxAsJson(st[j].bs);

                JsonObjectAppendObject(json, st[j].subtype, attributes);
            }
        }
    }

    return json;
}

/****************************************************************************/

static JsonElement *ExportControlBodiesSyntaxAsJson()
{
    JsonElement *control_bodies = JsonObjectCreate(10);
    int i = 0;

    for (i = 0; CF_ALL_BODIES[i].bundle_type != NULL; i++)
    {
        JsonElement *attributes = ExportAttributesSyntaxAsJson(CF_ALL_BODIES[i].bs);

        JsonObjectAppendObject(control_bodies, CF_ALL_BODIES[i].bundle_type, attributes);
    }

    return control_bodies;
}

/****************************************************************************/

void SyntaxPrintAsJson(Writer *writer)
{
    JsonElement *syntax_tree = JsonObjectCreate(10);

    {
        JsonElement *control_bodies = ExportControlBodiesSyntaxAsJson();

        JsonObjectAppendObject(syntax_tree, "control-bodies", control_bodies);
    }

    {
        JsonElement *bundle_types = JsonObjectCreate(10);
        int i = 0;

        for (i = 0; CF_ALL_BODIES[i].bundle_type != NULL; i++)
        {
            JsonElement *bundle_type = ExportBundleTypeSyntaxAsJson(CF_ALL_BODIES[i].bundle_type);

            JsonObjectAppendObject(bundle_types, CF_ALL_BODIES[i].bundle_type, bundle_type);
        }

        JsonObjectAppendObject(syntax_tree, "bundle-types", bundle_types);
    }

    JsonElementPrint(writer, syntax_tree, 0);
    JsonElementDestroy(syntax_tree);
}
