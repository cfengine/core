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

static SyntaxTypeMatch CheckParseString(const char *lv, const char *s, const char *range);
static SyntaxTypeMatch CheckParseInt(const char *lv, const char *s, const char *range);
static SyntaxTypeMatch CheckParseReal(const char *lv, const char *s, const char *range);
static SyntaxTypeMatch CheckParseRealRange(const char *lval, const char *s, const char *range);
static SyntaxTypeMatch CheckParseIntRange(const char *lval, const char *s, const char *range);
static SyntaxTypeMatch CheckParseOpts(const char *lv, const char *s, const char *range);
static SyntaxTypeMatch CheckFnCallType(const char *lval, const char *s, DataType dtype, const char *range);

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

DataType ExpectedDataType(const char *lvalname)
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

/****************************************************************************/
/* Level 1                                                                  */
/****************************************************************************/

const char *SyntaxTypeMatchToString(SyntaxTypeMatch result)
{
    assert(result < SYNTAX_TYPE_MATCH_MAX);

    static const char *msgs[SYNTAX_TYPE_MATCH_MAX] =
    {
        [SYNTAX_TYPE_MATCH_OK] = "OK",

        [SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED] = "Cannot check unexpanded value",
        [SYNTAX_TYPE_MATCH_ERROR_RANGE_BRACKETED] = "Real range specification should not be enclosed in brackets - just \"a,b\"",
        [SYNTAX_TYPE_MATCH_ERROR_RANGE_MULTIPLE_ITEMS] = "Range format specifier should be of form \"a,b\" but got multiple items",
        [SYNTAX_TYPE_MATCH_ERROR_GOT_SCALAR] = "Attampted to give a scalar to a non-scalar type",
        [SYNTAX_TYPE_MATCH_ERROR_GOT_LIST] = "Attampted to give a list to a non-list type",

        [SYNTAX_TYPE_MATCH_ERROR_STRING_UNIX_PERMISSION] = "Error parsing Unix permission string",

        [SYNTAX_TYPE_MATCH_ERROR_SCALAR_OUT_OF_RANGE] = "Scalar value is out of range",

        [SYNTAX_TYPE_MATCH_ERROR_INT_PARSE] = "Cannot parse value as integer",
        [SYNTAX_TYPE_MATCH_ERROR_INT_OUT_OF_RANGE] = "Integer is out of range",

        [SYNTAX_TYPE_MATCH_ERROR_REAL_INF] = "Keyword \"inf\" has an integer value, cannot be used as real",
        [SYNTAX_TYPE_MATCH_ERROR_REAL_OUT_OF_RANGE] = "Real value is out of range",

        [SYNTAX_TYPE_MATCH_ERROR_OPTS_OUT_OF_RANGE] = "Selection is out of bounds",

        [SYNTAX_TYPE_MATCH_ERROR_FNCALL_RETURN_TYPE] = "Function does not return the required type",
        [SYNTAX_TYPE_MATCH_ERROR_FNCALL_UNKNOWN] = "Unknown function",

        [SYNTAX_TYPE_MATCH_ERROR_CONTEXT_OUT_OF_RANGE] = "Context string is invalid/out of range"
    };

    return msgs[result];
}

SyntaxTypeMatch CheckConstraintTypeMatch(const char *lval, Rval rval, DataType dt, const char *range, int level)
{
    Rlist *rp;
    Item *checklist;

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
                return SYNTAX_TYPE_MATCH_ERROR_GOT_SCALAR;
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
            return SYNTAX_TYPE_MATCH_ERROR_GOT_LIST;
        }

        for (rp = (Rlist *) rval.item; rp != NULL; rp = rp->next)
        {
            SyntaxTypeMatch err = CheckConstraintTypeMatch(lval, (Rval) {rp->item, rp->type}, dt, range, 1);
            switch (err)
            {
            case SYNTAX_TYPE_MATCH_OK:
            case SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED:
                break;

            default:
                return err;
            }
        }

        return SYNTAX_TYPE_MATCH_OK;

    case RVAL_TYPE_FNCALL:

        /* Fn-like objects are assumed to be parameterized bundles in these... */

        checklist = SplitString("bundlesequence,edit_line,edit_xml,usebundle,service_bundle", ',');

        if (!IsItemIn(checklist, lval))
        {
            SyntaxTypeMatch err = CheckFnCallType(lval, ((FnCall *) rval.item)->name, dt, range);
            DeleteItemList(checklist);
            return err;
        }

        DeleteItemList(checklist);
        return SYNTAX_TYPE_MATCH_OK;

    default:
        break;
    }

/* If we get here, we have a literal scalar type */

    switch (dt)
    {
    case DATA_TYPE_STRING:
    case DATA_TYPE_STRING_LIST:
        return CheckParseString(lval, (const char *) rval.item, range);

    case DATA_TYPE_INT:
    case DATA_TYPE_INT_LIST:
        return CheckParseInt(lval, (const char *) rval.item, range);

    case DATA_TYPE_REAL:
    case DATA_TYPE_REAL_LIST:
        return CheckParseReal(lval, (const char *) rval.item, range);

    case DATA_TYPE_BODY:
    case DATA_TYPE_BUNDLE:
        CfDebug("Nothing to check for body reference\n");
        break;

    case DATA_TYPE_OPTION:
    case DATA_TYPE_OPTION_LIST:
        return CheckParseOpts(lval, (const char *) rval.item, range);

    case DATA_TYPE_CONTEXT:
    case DATA_TYPE_CONTEXT_LIST:
        return CheckParseContext((const char *) rval.item, range);

    case DATA_TYPE_INT_RANGE:
        return CheckParseIntRange(lval, (const char *) rval.item, range);

    case DATA_TYPE_REAL_RANGE:
        return CheckParseRealRange(lval, (char *) rval.item, range);

    default:
        ProgrammingError("Unknown (unhandled) datatype for lval = %s (CheckConstraintTypeMatch)\n", lval);
        break;
    }

    CfDebug("end CheckConstraintTypeMatch---------\n");
    return SYNTAX_TYPE_MATCH_OK;
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

static SyntaxTypeMatch CheckParseString(const char *lval, const char *s, const char *range)
{
    CfDebug("\nCheckParseString(%s => %s/%s)\n", lval, s, range);

    if (s == NULL)
    {
        return SYNTAX_TYPE_MATCH_OK;
    }

    if (strlen(range) == 0)
    {
        return SYNTAX_TYPE_MATCH_OK;
    }

    if (IsNakedVar(s, '@') || IsNakedVar(s, '$'))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }

/* Deal with complex strings as special cases */

    if (strcmp(lval, "mode") == 0 || strcmp(lval, "search_mode") == 0)
    {
        mode_t plus, minus;

        if (!ParseModeString(s, &plus, &minus))
        {
            return SYNTAX_TYPE_MATCH_ERROR_STRING_UNIX_PERMISSION;
        }
    }

    if (FullTextMatch(range, s))
    {
        return SYNTAX_TYPE_MATCH_OK;
    }

    if (IsCf3VarString(s))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }
    else
    {
        return SYNTAX_TYPE_MATCH_ERROR_SCALAR_OUT_OF_RANGE;
    }

    return SYNTAX_TYPE_MATCH_OK;
}

/****************************************************************************/

SyntaxTypeMatch CheckParseContext(const char *context, const char *range)
{
    if (strlen(range) == 0)
    {
        return SYNTAX_TYPE_MATCH_OK;
    }

    if (FullTextMatch(range, context))
    {
        return SYNTAX_TYPE_MATCH_OK;
    }

    return SYNTAX_TYPE_MATCH_ERROR_CONTEXT_OUT_OF_RANGE;
}

/****************************************************************************/

static SyntaxTypeMatch CheckParseInt(const char *lval, const char *s, const char *range)
{
    Item *split;
    int n;
    long max = CF_LOWINIT, min = CF_HIGHINIT, val;

/* Numeric types are registered by range separated by comma str "min,max" */
    CfDebug("\nCheckParseInt(%s => %s/%s)\n", lval, s, range);

    split = SplitString(range, ',');

    if ((n = ListLen(split)) != 2)
    {
        ProgrammingError("INTERN: format specifier for int rvalues is not ok for lval %s - got %d items", lval, n);
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
        ProgrammingError("INTERN: could not parse format specifier for int rvalues for lval %s", lval);
    }

    if (IsCf3VarString(s))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }

    val = IntFromString(s);

    if (val == CF_NOINT)
    {
        return SYNTAX_TYPE_MATCH_ERROR_INT_PARSE;
    }

    if (val > max || val < min)
    {
        return SYNTAX_TYPE_MATCH_ERROR_INT_OUT_OF_RANGE;
    }

    CfDebug("CheckParseInt - syntax verified\n\n");

    return SYNTAX_TYPE_MATCH_OK;
}

/****************************************************************************/

static SyntaxTypeMatch CheckParseIntRange(const char *lval, const char *s, const char *range)
{
    Item *split, *ip, *rangep;
    int n;
    long max = CF_LOWINIT, min = CF_HIGHINIT, val;

/* Numeric types are registered by range separated by comma str "min,max" */
    CfDebug("\nCheckParseIntRange(%s => %s/%s)\n", lval, s, range);

    if (*s == '[' || *s == '(')
    {
        return SYNTAX_TYPE_MATCH_ERROR_RANGE_BRACKETED;
    }

    split = SplitString(range, ',');

    if ((n = ListLen(split)) != 2)
    {
        ProgrammingError("Format specifier %s for irange rvalues is not ok for lval %s - got %d items", range, lval, n);
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
        ProgrammingError("Could not parse irange format specifier for int rvalues for lval %s", lval);
    }

    if (IsCf3VarString(s))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }

    rangep = SplitString(s, ',');

    if ((n = ListLen(rangep)) != 2)
    {
        return SYNTAX_TYPE_MATCH_ERROR_RANGE_MULTIPLE_ITEMS;
    }

    for (ip = rangep; ip != NULL; ip = ip->next)
    {
        val = IntFromString(ip->name);

        if (val > max || val < min)
        {
            return SYNTAX_TYPE_MATCH_ERROR_INT_OUT_OF_RANGE;
        }
    }

    DeleteItemList(rangep);

    return SYNTAX_TYPE_MATCH_OK;
}

/****************************************************************************/

static SyntaxTypeMatch CheckParseReal(const char *lval, const char *s, const char *range)
{
    Item *split;
    double max = (double) CF_LOWINIT, min = (double) CF_HIGHINIT, val;
    int n;

    CfDebug("\nCheckParseReal(%s => %s/%s)\n", lval, s, range);

    if (strcmp(s, "inf") == 0)
    {
        return SYNTAX_TYPE_MATCH_ERROR_REAL_INF;
    }

    if (IsCf3VarString(s))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }

/* Numeric types are registered by range separated by comma str "min,max" */

    split = SplitString(range, ',');

    if ((n = ListLen(split)) != 2)
    {
        ProgrammingError("Format specifier for real rvalues is not ok for lval %s - %d items", lval, n);
    }

    sscanf(split->name, "%lf", &min);
    sscanf(split->next->name, "%lf", &max);
    DeleteItemList(split);

    if (min == CF_HIGHINIT || max == CF_LOWINIT)
    {
        ProgrammingError("Could not parse format specifier for int rvalues for lval %s", lval);
    }

    val = DoubleFromString(s);

    if (val > max || val < min)
    {
        return SYNTAX_TYPE_MATCH_ERROR_REAL_OUT_OF_RANGE;
    }

    return SYNTAX_TYPE_MATCH_OK;
}

/****************************************************************************/

static SyntaxTypeMatch CheckParseRealRange(const char *lval, const char *s, const char *range)
{
    Item *split, *rangep, *ip;
    double max = (double) CF_LOWINIT, min = (double) CF_HIGHINIT, val;
    int n;

    CfDebug("\nCheckParseRealRange(%s => %s/%s)\n", lval, s, range);

    if (*s == '[' || *s == '(')
    {
        return SYNTAX_TYPE_MATCH_ERROR_RANGE_BRACKETED;
    }

    if (strcmp(s, "inf") == 0)
    {
        return SYNTAX_TYPE_MATCH_ERROR_REAL_INF;
    }

    if (IsCf3VarString(s))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }

/* Numeric types are registered by range separated by comma str "min,max" */

    split = SplitString(range, ',');

    if ((n = ListLen(split)) != 2)
    {
        ProgrammingError("Format specifier for real rvalues is not ok for lval %s - %d items", lval, n);
    }

    sscanf(split->name, "%lf", &min);
    sscanf(split->next->name, "%lf", &max);
    DeleteItemList(split);

    if (min == CF_HIGHINIT || max == CF_LOWINIT)
    {
        ProgrammingError("Could not parse format specifier for int rvalues for lval %s", lval);
    }

    rangep = SplitString(s, ',');

    if ((n = ListLen(rangep)) != 2)
    {
        return SYNTAX_TYPE_MATCH_ERROR_RANGE_MULTIPLE_ITEMS;
    }

    for (ip = rangep; ip != NULL; ip = ip->next)
    {
        val = DoubleFromString(ip->name);

        if (val > max || val < min)
        {
            return SYNTAX_TYPE_MATCH_ERROR_REAL_OUT_OF_RANGE;
        }
    }

    DeleteItemList(rangep);

    return SYNTAX_TYPE_MATCH_OK;
}

/****************************************************************************/

static SyntaxTypeMatch CheckParseOpts(const char *lval, const char *s, const char *range)
{
    Item *split;

/* List/menu types are separated by comma str "a,b,c,..." */

    CfDebug("\nCheckParseOpts(%s => %s/%s)\n", lval, s, range);

    if (IsNakedVar(s, '@') || IsNakedVar(s, '$'))
    {
        return SYNTAX_TYPE_MATCH_ERROR_UNEXPANDED;
    }

    split = SplitString(range, ',');

    if (!IsItemIn(split, s))
    {
        DeleteItemList(split);
        return SYNTAX_TYPE_MATCH_ERROR_OPTS_OUT_OF_RANGE;
    }

    DeleteItemList(split);

    return SYNTAX_TYPE_MATCH_OK;
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

static SyntaxTypeMatch CheckFnCallType(const char *lval, const char *s, DataType dtype, const char *range)
{
    DataType dt;
    const FnCallType *fn;

    CfDebug("CheckFnCallType(%s => %s/%s)\n", lval, s, range);

    fn = FnCallTypeGet(s);

    if (fn)
    {
        dt = fn->dtype;

        if (dtype != dt)
        {
            /* Ok to allow fn calls of correct element-type in lists */

            if (dt == DATA_TYPE_STRING && dtype == DATA_TYPE_STRING_LIST)
            {
                return SYNTAX_TYPE_MATCH_OK;
            }

            if (dt == DATA_TYPE_INT && dtype == DATA_TYPE_INT_LIST)
            {
                return SYNTAX_TYPE_MATCH_OK;
            }

            if (dt == DATA_TYPE_REAL && dtype == DATA_TYPE_REAL_LIST)
            {
                return SYNTAX_TYPE_MATCH_OK;
            }

            if (dt == DATA_TYPE_OPTION && dtype == DATA_TYPE_OPTION_LIST)
            {
                return SYNTAX_TYPE_MATCH_OK;
            }

            if (dt == DATA_TYPE_CONTEXT && dtype == DATA_TYPE_CONTEXT_LIST)
            {
                return SYNTAX_TYPE_MATCH_OK;
            }

            return SYNTAX_TYPE_MATCH_ERROR_FNCALL_RETURN_TYPE;
        }
        else
        {
            return SYNTAX_TYPE_MATCH_OK;
        }
    }
    else
    {
        return SYNTAX_TYPE_MATCH_ERROR_FNCALL_UNKNOWN;
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
