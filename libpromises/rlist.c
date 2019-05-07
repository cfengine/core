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

#include <rlist.h>

#include <files_names.h>
#include <conversion.h>
#include <expand.h>
#include <matching.h>
#include <scope.h>
#include <fncall.h>
#include <string_lib.h>                                       /* StringHash */
#include <regex.h>          /* StringMatchWithPrecompiledRegex,CompileRegex */
#include <misc_lib.h>
#include <assoc.h>
#include <eval_context.h>
#include <json.h>
#include <vars.h>                                         /* IsCf3VarString */


static Rlist *RlistPrependRval(Rlist **start, Rval rval);

RvalType DataTypeToRvalType(DataType datatype)
{
    switch (datatype)
    {
    case CF_DATA_TYPE_BODY:
    case CF_DATA_TYPE_BUNDLE:
    case CF_DATA_TYPE_CONTEXT:
    case CF_DATA_TYPE_COUNTER:
    case CF_DATA_TYPE_INT:
    case CF_DATA_TYPE_INT_RANGE:
    case CF_DATA_TYPE_OPTION:
    case CF_DATA_TYPE_REAL:
    case CF_DATA_TYPE_REAL_RANGE:
    case CF_DATA_TYPE_STRING:
        return RVAL_TYPE_SCALAR;

    case CF_DATA_TYPE_CONTEXT_LIST:
    case CF_DATA_TYPE_INT_LIST:
    case CF_DATA_TYPE_OPTION_LIST:
    case CF_DATA_TYPE_REAL_LIST:
    case CF_DATA_TYPE_STRING_LIST:
        return RVAL_TYPE_LIST;

    case CF_DATA_TYPE_CONTAINER:
        return RVAL_TYPE_CONTAINER;

    case CF_DATA_TYPE_NONE:
        return RVAL_TYPE_NOPROMISEE;
    }

    ProgrammingError("DataTypeToRvalType, unhandled");
}

bool RlistValueIsType(const Rlist *rlist, RvalType type)
{
    return (rlist != NULL &&
            rlist->val.type == type);
}

char *RlistScalarValue(const Rlist *rlist)
{
    if (rlist->val.type != RVAL_TYPE_SCALAR)
    {
        ProgrammingError("Rlist value contains type %c instead of expected scalar", rlist->val.type);
    }

    return rlist->val.item;
}

char *RlistScalarValueSafe(const Rlist *rlist)
{
    if (rlist->val.type != RVAL_TYPE_SCALAR)
    {
        return "[not printable]";
    }

    return RlistScalarValue(rlist);
}

/*******************************************************************/

FnCall *RlistFnCallValue(const Rlist *rlist)
{
    if (rlist->val.type != RVAL_TYPE_FNCALL)
    {
        ProgrammingError("Rlist value contains type %c instead of expected FnCall", rlist->val.type);
    }

    return rlist->val.item;
}

/*******************************************************************/

Rlist *RlistRlistValue(const Rlist *rlist)
{
    if (rlist->val.type != RVAL_TYPE_LIST)
    {
        ProgrammingError("Rlist value contains type %c instead of expected List", rlist->val.type);
    }

    return rlist->val.item;
}

/*******************************************************************/

char *RvalScalarValue(Rval rval)
{
    if (rval.type != RVAL_TYPE_SCALAR)
    {
        ProgrammingError("Internal error: Rval contains type %c instead of expected scalar", rval.type);
    }

    return rval.item;
}

/*******************************************************************/

FnCall *RvalFnCallValue(Rval rval)
{
    if (rval.type != RVAL_TYPE_FNCALL)
    {
        ProgrammingError("Rval contains type %c instead of expected FnCall", rval.type);
    }

    return rval.item;
}

/*******************************************************************/

Rlist *RvalRlistValue(Rval rval)
{
    if (rval.type != RVAL_TYPE_LIST)
    {
        ProgrammingError("Rval contain type %c instead of expected List", rval.type);
    }

    return rval.item;
}

/*******************************************************************/

JsonElement *RvalContainerValue(Rval rval)
{
    if (rval.type != RVAL_TYPE_CONTAINER)
    {
        ProgrammingError("Rval contain type %c instead of expected container", rval.type);
    }

    return rval.item;
}


const char *RvalTypeToString(RvalType type)
{
    switch (type)
    {
    case RVAL_TYPE_CONTAINER:
        return "data";
    case RVAL_TYPE_FNCALL:
        return "call";
    case RVAL_TYPE_LIST:
        return "list";
    case RVAL_TYPE_NOPROMISEE:
        return "null";
    case RVAL_TYPE_SCALAR:
        return "scalar";
    }

    assert(false && "never reach");
    return NULL;
}

Rlist *RlistKeyIn(Rlist *list, const char *key)
{
    for (Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (rp->val.type == RVAL_TYPE_SCALAR &&
            strcmp(RlistScalarValue(rp), key) == 0)
        {
            return rp;
        }
    }

    return NULL;
}

/*******************************************************************/

bool RlistMatchesRegexRlist(const Rlist *list, const Rlist *search)
/*
   Returns true if "list" contains all the regular expressions in
   "search".  Non-scalars in "list" and "search" are skipped.
*/
{
    for (const Rlist *rp = search; rp != NULL; rp = rp->next)
    {
        if (rp->val.type == RVAL_TYPE_SCALAR &&
            // check for the current element in the search list
            !RlistMatchesRegex(list, RlistScalarValue(search)))
        {
            return false;
        }
    }

    return true;
}

bool RlistMatchesRegex(const Rlist *list, const char *regex)
/*
   Returns true if any of the "list" of strings matches "regex".
   Non-scalars in "list" are skipped.
*/
{
    if (regex == NULL || list == NULL)
    {
        return false;
    }

    pcre *rx = CompileRegex(regex);
    if (!rx)
    {
        return false;
    }

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (rp->val.type == RVAL_TYPE_SCALAR &&
            StringMatchFullWithPrecompiledRegex(rx, RlistScalarValue(rp)))
        {
            pcre_free(rx);
            return true;
        }
    }

    pcre_free(rx);
    return false;
}

bool RlistIsNullList(const Rlist *list)
{
    return (list == NULL);
}

bool RlistIsInListOfRegex(const Rlist *list, const char *str)
/*
   Returns true if any of the "list" of regular expressions matches "str".
   Non-scalars in "list" are skipped.
*/
{
    if (str == NULL || list == NULL)
    {
        return false;
    }

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (rp->val.type == RVAL_TYPE_SCALAR &&
            StringMatchFull(RlistScalarValue(rp), str))
        {
            return true;
        }
    }

    return false;
}

/*******************************************************************/

static Rval RvalCopyScalar(Rval rval)
{
    assert(rval.type == RVAL_TYPE_SCALAR);
    const char * src = rval.item ? rval.item : "";

    return (Rval) {xstrdup(src), RVAL_TYPE_SCALAR};
}

Rlist *RlistAppendRval(Rlist **start, Rval rval)
{
    Rlist *rp = xmalloc(sizeof(Rlist));

    rp->val  = rval;
    rp->next = NULL;

    if (*start == NULL)
    {
        *start = rp;
    }
    else
    {
        Rlist *lp = *start;
        while (lp->next != NULL)
        {
            lp = lp->next;
        }

        lp->next = rp;
    }

    return rp;
}

/* Inserts an Rlist node with value #rval, right after the rlist node #node. */
void RlistInsertAfter(Rlist *node, Rval rval)
{
    assert(node != NULL);

    Rlist new_node = { .val  = rval,
                       .next = node->next };

    node->next = xmemdup(&new_node, sizeof(new_node));
}

Rval RvalNewRewriter(const void *item, RvalType type, JsonElement *map)
{
    switch (type)
    {
    case RVAL_TYPE_SCALAR:
        if (map != NULL && JsonLength(map) > 0 &&       // do we have a rewrite map?
            (strstr(item, "$(") || strstr(item, "${"))) // are there unresolved variable references?
        {
            // TODO: replace with BufferSearchAndReplace when the
            // string_replace code is merged.
            // Sorry about the CF_BUFSIZE ugliness.
            int max_size = 10*CF_BUFSIZE+1;
            char *buffer_from = xmalloc(max_size);
            char *buffer_to = xmalloc(max_size);

            Buffer *format = BufferNew();
            strncpy(buffer_from, item, max_size);

            for (int iteration = 0; iteration < 10; iteration++)
            {
                bool replacement_made = false;
                int var_start = -1;
                char closing_brace = 0;
                for (int c = 0; c < buffer_from[c]; c++)
                {
                    if (buffer_from[c] == '$')
                    {
                        if (buffer_from[c+1] == '(')
                        {
                            closing_brace = ')';
                        }
                        else if (buffer_from[c+1] == '{')
                        {
                            closing_brace = '}';
                        }

                        if (closing_brace)
                        {
                            c++;
                            var_start = c-1;
                        }
                    }
                    else if (var_start >= 0 && buffer_from[c] == closing_brace)
                    {
                        char saved = buffer_from[c];
                        buffer_from[c] = '\0';
                        const char *repl = JsonObjectGetAsString(map, buffer_from + var_start + 2);
                        buffer_from[c] = saved;

                        if (repl)
                        {
                            // Before the replacement.
                            memcpy(buffer_to, buffer_from, var_start);

                            // The actual replacement.
                            int repl_len = strlen(repl);
                            memcpy(buffer_to + var_start, repl, repl_len);

                            // The text after.
                            strlcpy(buffer_to + var_start + repl_len, buffer_from + c + 1, max_size - var_start - repl_len);

                            // Reset location to immediately after the replacement.
                            c = var_start + repl_len - 1;
                            var_start = -1;
                            strcpy(buffer_from, buffer_to);
                            closing_brace = 0;
                            replacement_made = true;
                        }
                    }
                }

                if (!replacement_made)
                {
                    break;
                }
            }

            char *ret = xstrdup(buffer_to);

            BufferDestroy(format);
            free(buffer_to);
            free(buffer_from);

            return (Rval) { ret, RVAL_TYPE_SCALAR };
        }
        else
        {
            return (Rval) { xstrdup(item), RVAL_TYPE_SCALAR };
        }

    case RVAL_TYPE_FNCALL:
        return (Rval) { FnCallCopyRewriter(item, map), RVAL_TYPE_FNCALL };

    case RVAL_TYPE_LIST:
        return (Rval) { RlistCopyRewriter(item, map), RVAL_TYPE_LIST };

    case RVAL_TYPE_CONTAINER:
        return (Rval) { JsonCopy(item), RVAL_TYPE_CONTAINER };

    case RVAL_TYPE_NOPROMISEE:
        return ((Rval) {NULL, type});
    }

    assert(false);
    return ((Rval) { NULL, RVAL_TYPE_NOPROMISEE });
}

Rval RvalNew(const void *item, RvalType type)
{
    return RvalNewRewriter(item, type, NULL);
}

Rval RvalCopyRewriter(Rval rval, JsonElement *map)
{
    return RvalNewRewriter(rval.item, rval.type, map);
}

Rval RvalCopy(Rval rval)
{
    return RvalNew(rval.item, rval.type);
}

/*******************************************************************/

Rlist *RlistCopyRewriter(const Rlist *rp, JsonElement *map)
{
    Rlist *start = NULL;

    while (rp != NULL)
    {
        RlistAppendRval(&start, RvalCopyRewriter(rp->val, map));
        rp = rp->next;
    }

    return start;
}

Rlist *RlistCopy(const Rlist *rp)
{
    return RlistCopyRewriter(rp, NULL);
}

/*******************************************************************/

void RlistDestroy(Rlist *rl)
/* Delete an rlist and all its references */
{
    while (rl != NULL)
    {
        Rlist *next = rl->next;

        if (rl->val.item)
        {
            RvalDestroy(rl->val);
        }

        free(rl);
        rl = next;
    }
}

void RlistDestroy_untyped(void *rl)
{
    RlistDestroy(rl);
}

/*******************************************************************/

Rlist *RlistAppendScalarIdemp(Rlist **start, const char *scalar)
{
    if (RlistKeyIn(*start, scalar))
    {
        return NULL;
    }

    return RlistAppendScalar(start, scalar);
}

Rlist *RlistPrependScalarIdemp(Rlist **start, const char *scalar)
{
    if (RlistKeyIn(*start, scalar))
    {
        return NULL;
    }

    return RlistPrepend(start, scalar, RVAL_TYPE_SCALAR);
}

Rlist *RlistAppendScalar(Rlist **start, const char *scalar)
{
    return RlistAppendRval(start, RvalCopyScalar((Rval) { (char *)scalar, RVAL_TYPE_SCALAR }));
}

// NOTE: Copies item, does NOT take ownership
Rlist *RlistAppend(Rlist **start, const void *item, RvalType type)
{
    return RlistAppendAllTypes(start, item, type, false);
}

// See fncall.c for the usage of allow_all_types.
Rlist *RlistAppendAllTypes(Rlist **start, const void *item, RvalType type, bool allow_all_types)
{
    Rlist *lp = *start;

    switch (type)
    {
    case RVAL_TYPE_SCALAR:
        return RlistAppendScalar(start, item);

    case RVAL_TYPE_FNCALL:
        break;

    case RVAL_TYPE_LIST:
        if (allow_all_types)
        {
            JsonElement* store = JsonArrayCreate(RlistLen(item));
            for (const Rlist *rp = item; rp; rp = rp->next)
            {
                JsonArrayAppendElement(store, RvalToJson(rp->val));
            }

            return RlistAppendRval(start, (Rval) { store, RVAL_TYPE_CONTAINER });
        }

        for (const Rlist *rp = item; rp; rp = rp->next)
        {
            lp = RlistAppendRval(start, RvalCopy(rp->val));
        }

        return lp;

    case RVAL_TYPE_CONTAINER:
        if (allow_all_types)
        {
            return RlistAppendRval(start, (Rval) { JsonCopy((JsonElement*) item), RVAL_TYPE_CONTAINER });
        }

        // note falls through!

    default:
        Log(LOG_LEVEL_DEBUG, "Cannot append %c to rval-list '%s'", type, (char *) item);
        return NULL;
    }

    Rlist *rp = xmalloc(sizeof(Rlist));

    rp->val  = RvalNew(item, type);
    rp->next = NULL;

    if (*start == NULL)
    {
        *start = rp;
    }
    else
    {
        for (lp = *start; lp->next != NULL; lp = lp->next)
        {
        }

        lp->next = rp;
    }

    return rp;
}

/*******************************************************************/

static Rlist *RlistPrependRval(Rlist **start, Rval rval)
{
    Rlist *rp = xmalloc(sizeof(Rlist));

    rp->next = *start;
    rp->val = rval;

    *start = rp;

    return rp;
}

Rlist *RlistPrepend(Rlist **start, const void *item, RvalType type)
{
    switch (type)
    {
    case RVAL_TYPE_LIST:
        {
            Rlist *lp = NULL;
            for (const Rlist *rp = item; rp; rp = rp->next)
            {
                lp = RlistPrependRval(start, RvalCopy(rp->val));
            }
            return lp;
        }

    case RVAL_TYPE_SCALAR:
    case RVAL_TYPE_FNCALL:
    case RVAL_TYPE_CONTAINER:
    case RVAL_TYPE_NOPROMISEE:
        return RlistPrependRval(start, RvalNew(item, type));
    }

    assert(false);
    return NULL;
}

/*******************************************************************/

int RlistLen(const Rlist *start)
{
    int count = 0;

    for (const Rlist *rp = start; rp != NULL; rp = rp->next)
    {
        count++;
    }

    return count;
}

/*******************************************************************/

Rlist *RlistParseShown(const char *string)
{
    Rlist *newlist = NULL, *splitlist, *rp;

/* Parse a string representation generated by ShowList and turn back into Rlist */

    splitlist = RlistFromSplitString(string, ',');

    for (rp = splitlist; rp != NULL; rp = rp->next)
    {
        char value[CF_MAXVARSIZE] = { 0 };
        sscanf(RlistScalarValue(rp), "%*[{ '\"]%255[^'\"}]", value);
        RlistAppendScalar(&newlist, value);
    }

    RlistDestroy(splitlist);
    return newlist;
}

/*******************************************************************/

typedef enum
{
    ST_OPENED,
    ST_PRECLOSED,
    ST_CLOSED,
    ST_IO,
    ST_ELM1,
    ST_ELM2,
    ST_END1,
    ST_END2,
    ST_SEP,
    ST_ERROR
} state;

#define CLASS_BLANK(x)  (((x)==' ')||((x)=='\t'))
#define CLASS_START1(x) (((x)=='\''))
#define CLASS_START2(x) (((x)=='"'))
#define CLASS_END1(x)   ((CLASS_START1(x)))
#define CLASS_END2(x)   ((CLASS_START2(x)))
#define CLASS_BRA1(x)   (((x)=='{'))
#define CLASS_BRA2(x)   (((x)=='}'))
#define CLASS_SEP(x)    (((x)==','))
#define CLASS_EOL(x)    (((x)=='\0'))

#define CLASS_ANY0(x) ((!CLASS_BLANK(x))&&(!CLASS_BRA1(x)))
#define CLASS_ANY1(x) ((!CLASS_BLANK(x))&&(!CLASS_START1(x))&&(!CLASS_START2(x)))
#define CLASS_ANY2(x) ((!CLASS_END1(x)))
#define CLASS_ANY3(x) ((!CLASS_END2(x)))
#define CLASS_ANY4(x) ((!CLASS_BLANK(x))&&(!CLASS_SEP(x))&&(!CLASS_BRA2(x)))
#define CLASS_ANY5(x) ((!CLASS_BLANK(x))&&(!CLASS_SEP(x))&&(!CLASS_BRA2(x)))
#define CLASS_ANY6(x) ((!CLASS_BLANK(x))&&(!CLASS_START2(x))&&(!CLASS_START2(x)))
#define CLASS_ANY7(x) ((!CLASS_BLANK(x))&&(!CLASS_EOL(x)))

/**
 @brief parse elements in a list passed through use_module

 @param[in] str: is the string to parse
 @param[out] newlist: rlist of elements found

 @retval 0: successful > 0: failed
 */
static int LaunchParsingMachine(const char *str, Rlist **newlist)
{
    const char *s = str;
    state current_state = ST_OPENED;
    int ret;

    Buffer *buf = BufferNewWithCapacity(CF_MAXVARSIZE);

    assert(newlist);

    while (current_state != ST_CLOSED && *s)
    {
        switch(current_state)
        {
            case ST_ERROR:
                Log(LOG_LEVEL_ERR, "Parsing error : Malformed string");
                ret = 1;
                goto clean;
            case ST_OPENED:
                if (CLASS_BLANK(*s))
                {
                    current_state = ST_OPENED;
                }
                else if (CLASS_BRA1(*s))
                {
                    current_state = ST_IO;
                }
                else if (CLASS_ANY0(*s))
                {
                    current_state = ST_ERROR;
                }
                s++;
                break;
            case ST_IO:
                if (CLASS_BLANK(*s))
                {
                    current_state = ST_IO;
                }
                else if (CLASS_START1(*s))
                {
                    BufferClear(buf);
                    current_state = ST_ELM1;
                }
                else if (CLASS_START2(*s))
                {
                    BufferClear(buf);
                    current_state = ST_ELM2;
                }
                else if (CLASS_ANY1(*s))
                {
                    current_state = ST_ERROR;
                }
                s++;
                break;
            case ST_ELM1:
                if (CLASS_END1(*s))
                {
                    RlistAppendScalar(newlist, BufferData(buf));
                    BufferClear(buf);
                    current_state = ST_END1;
                }
                else if (CLASS_ANY2(*s))
                {
                    BufferAppendChar(buf, *s);
                    current_state = ST_ELM1;
                }
                s++;
                break;
            case ST_ELM2:
                if (CLASS_END2(*s))
                {
                    RlistAppendScalar(newlist, BufferData(buf));
                    BufferClear(buf);
                    current_state = ST_END2;
                }
                else if (CLASS_ANY3(*s))
                {
                    BufferAppendChar(buf, *s);
                    current_state = ST_ELM2;
                }
                s++;
                break;
            case ST_END1:
                if (CLASS_SEP(*s))
                {
                    current_state = ST_SEP;
                }
                else if (CLASS_BRA2(*s))
                {
                    current_state = ST_PRECLOSED;
                }
                else if (CLASS_BLANK(*s))
                {
                    current_state = ST_END1;
                }
                else if (CLASS_ANY4(*s))
                {
                    current_state = ST_ERROR;
                }
                s++;
                break;
            case ST_END2:
                if (CLASS_SEP(*s))
                {
                    current_state = ST_SEP;
                }
                else if (CLASS_BRA2(*s))
                {
                    current_state = ST_PRECLOSED;
                }
                else if (CLASS_BLANK(*s))
                {
                    current_state = ST_END2;
                }
                else if (CLASS_ANY5(*s))
                {
                    current_state = ST_ERROR;
                }
                s++;
                break;
            case ST_SEP:
                if (CLASS_BLANK(*s))
                {
                    current_state = ST_SEP;
                }
                else if (CLASS_START1(*s))
                {
                    current_state = ST_ELM1;
                }
                else if (CLASS_START2(*s))
                {
                    current_state = ST_ELM2;
                }
                else if (CLASS_ANY6(*s))
                {
                    current_state = ST_ERROR;
                }
                s++;
                break;
            case ST_PRECLOSED:
                if (CLASS_BLANK(*s))
                {
                    current_state = ST_PRECLOSED;
                }
                else if (CLASS_EOL(*s))
                {
                    current_state = ST_CLOSED;
                }
                else if (CLASS_ANY7(*s))
                {
                    current_state = ST_ERROR;
                }
                s++;
                break;
            default:
                Log(LOG_LEVEL_ERR, "Parsing logic error: unknown state");
                ret = 2;
                goto clean;
                break;
        }
    }

    if (current_state != ST_CLOSED && current_state != ST_PRECLOSED )
    {
        Log(LOG_LEVEL_ERR, "Parsing error : Malformed string (unexpected end of input)");
        ret = 3;
        goto clean;
    }

    BufferDestroy(buf);
    return 0;

clean:
    BufferDestroy(buf);
    RlistDestroy(*newlist);
    assert(ret != 0);
    return ret;
}

Rlist *RlistParseString(const char *string)
{
    Rlist *newlist = NULL;
    if (LaunchParsingMachine(string, &newlist))
    {
        return NULL;
    }

    return newlist;
}

/*******************************************************************/

void RvalDestroy(Rval rval)
{
    if (rval.item == NULL)
    {
        return;
    }

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        free(RvalScalarValue(rval));
        return;

    case RVAL_TYPE_LIST:
        RlistDestroy(RvalRlistValue(rval));
        return;

    case RVAL_TYPE_FNCALL:
        FnCallDestroy(RvalFnCallValue(rval));
        break;

    case RVAL_TYPE_CONTAINER:
        JsonDestroy(RvalContainerValue(rval));
        break;

    case RVAL_TYPE_NOPROMISEE:
        return;
    }
}

/*********************************************************************/

void RlistDestroyEntry(Rlist **liststart, Rlist *entry)
{
    if (entry != NULL)
    {
        if (entry->val.item)
        {
            free(entry->val.item);
        }

        Rlist *sp = entry->next;

        if (entry == *liststart)
        {
            *liststart = sp;
        }
        else
        {
            Rlist *rp = *liststart;
            while (rp->next != entry)
            {
                rp = rp->next;
            }

            assert(rp && rp->next == entry);
            rp->next = sp;
        }

        free(entry);
    }
}

/*******************************************************************/

/* Copies a <sep>-delimited unit from <from> into a new entry in <to>.
 *
 * \<sep> is not counted as the separator, but copied to the new entry
 * as <sep>.  No other escape sequences are supported.
 *
 * Returns the number of bytes read out of <from>; this may be more
 * than the length of the new entry in <to>.  The new entry is
 * prepended; the caller can reverse <to> once built.
 */
static size_t SubStrnCopyChr(Rlist **to, const char *from, char sep, char lstrip)
{
    assert(from && from[0]);
    size_t offset = 0;

    while (lstrip != '\0' && from[0] == lstrip && from[0] != '\0')
    {
        /* Skip over all instances of the 'lstrip' character (e.g. ' ') if
         * specified */
        from++;
        offset++;
    }
    if (from[0] == '\0')
    {
        /* Reached the end already so there's nothing to add to the result list,
           just tell the caller how far they can move. */
        return offset;
    }

    const char *end = from;
    size_t escapes = 0;
    while (end && end[0] && end[0] != sep)
    {
        end = strchr(end, sep);
        assert(end == NULL || end[0] == sep);
        if (end && end > from && end[-1] == '\\')
        {
            escapes++;
            end++;
        }
    }

    size_t consume = (end == NULL) ? strlen(from) : (end - from);
    assert(consume >= escapes);
    char copy[1 + consume - escapes], *dst = copy;

    for (const char *src = from; src[0] != '\0' && src[0] != sep; src++)
    {
        if (src[0] == '\\' && src[1] == sep)
        {
            src++; /* Skip over the backslash so we copy the sep */
        }
        dst++[0] = src[0];
    }
    assert(dst + 1 == copy + sizeof(copy));
    *dst = '\0';

    /* Prepend to the list and reverse when done, costing O(len),
     * instead of appending, which costs O(len**2). */
    RlistPrependRval(to, RvalCopyScalar((Rval) { copy, RVAL_TYPE_SCALAR }));
    return offset + consume;
}

Rlist *RlistFromSplitString(const char *string, char sep)
/* Splits a string on a separator - e.g. "," - into a linked list of
 * separate items.  Supports escaping separators - e.g. "\," isn't a
 * separator, it contributes a simple "," in a list entry. */
{
    if (string == NULL || string[0] == '\0')
    {
        return NULL;
    }
    Rlist *liststart = NULL;

    for (const char *sp = string; *sp != '\0';)
    {
        sp += SubStrnCopyChr(&liststart, sp, sep, '\0');
        assert(sp - string <= strlen(string));
        if (*sp)
        {
            assert(*sp == sep && (sp == string || sp[-1] != '\\'));
            sp++;
        }
    }

    RlistReverse(&liststart);
    return liststart;
}

/**
 * Splits the given string into lines. On Windows, both \n and \r\n newlines are
 * detected. Escaped newlines are respected/ignored too.
 *
 * @param detect_crlf whether to try to detect and respect "\r\n" line endings
 * @return: an #Rlist where items are the individual lines **without** the
 *          trailing newline character(s)
 * @note: Free the result with RlistDestroy()
 * @warning: This function doesn't work properly if @string uses "\r\n" newlines
 *           and contains '\r' characters that are not part of any "\r\n"
 *           sequence because it first splits @string on '\r'.
 */
Rlist *RlistFromStringSplitLines(const char *string, bool detect_crlf)
{
    if (string == NULL || string[0] == '\0')
    {
        return NULL;
    }

    if (!detect_crlf || (strstr(string, "\r\n") == NULL))
    {
        return RlistFromSplitString(string, '\n');
    }

    /* else we split on '\r' just like RlistFromSplitString() above, but
     * strip leading '\n' in every chunk, thus effectively split on \r\n. See
     * the warning in the function's documentation.*/
    Rlist *liststart = NULL;

    for (const char *sp = string; *sp != '\0';)
    {
        sp += SubStrnCopyChr(&liststart, sp, '\r', '\n');
        assert(sp - string <= strlen(string));
        if (*sp)
        {
            assert(*sp == '\r' && (sp == string || sp[-1] != '\\'));
            sp++;
        }
    }

    RlistReverse(&liststart);
    return liststart;
}

/*******************************************************************/

Rlist *RlistFromSplitRegex(const char *string, const char *regex, size_t max_entries, bool allow_blanks)
{
    assert(string);
    if (!string)
    {
        return NULL;
    }

    const char *sp = string;
    size_t entry_count = 0;
    int start = 0;
    int end = 0;
    Rlist *result = NULL;
    Buffer *buffer = BufferNewWithCapacity(CF_MAXVARSIZE);

    pcre *rx = CompileRegex(regex);
    if (rx)
    {
        while ((entry_count < max_entries) &&
               StringMatchWithPrecompiledRegex(rx, sp, &start, &end))
        {
            if (end == 0)
            {
                break;
            }

            BufferClear(buffer);
            BufferAppend(buffer, sp, start);

            if (allow_blanks || BufferSize(buffer) > 0)
            {
                RlistAppendScalar(&result, BufferData(buffer));
                entry_count++;
            }

            sp += end;
        }

        pcre_free(rx);
    }

    if (entry_count < max_entries)
    {
        BufferClear(buffer);
        size_t remaining = strlen(sp);
        BufferAppend(buffer, sp, remaining);

        if ((allow_blanks && sp != string) || BufferSize(buffer) > 0)
        {
            RlistAppendScalar(&result, BufferData(buffer));
        }
    }

    BufferDestroy(buffer);

    return result;
}

/*******************************************************************/
/*
 * Splits string on regex, returns a list of (at most max) fragments.
 *
 * NOTE: in contrast with RlistFromSplitRegex() this one will produce at most max number of elements;
 *       last element will contain everything that lefts from original string (we use everything after
 *       the (max-1)-th separator as the final list element, including any separators that may be embedded in it)
 */
Rlist *RlistFromRegexSplitNoOverflow(const char *string, const char *regex, int max)
{
    Rlist *liststart = NULL;
    char node[CF_MAXVARSIZE];
    int start, end;
    int count = 0;

    assert(max > 0); // ensured by FnCallStringSplit() before calling us
    assert(string != NULL); // ensured by FnCallStringSplit() before calling us

    const char *sp = string;
    // We will avoid compiling regex multiple times.
    pcre *pattern = CompileRegex(regex);

    if (pattern == NULL)
    {
        Log(LOG_LEVEL_DEBUG, "Error compiling regex from '%s'", regex);
        return NULL;
    }

    while (count < max - 1 &&
           StringMatchWithPrecompiledRegex(pattern, sp, &start, &end))
    {
        size_t len = start;
        if (len >= CF_MAXVARSIZE)
        {
            len = CF_MAXVARSIZE - 1;
            Log(LOG_LEVEL_WARNING,
                "Segment in string_split() is %d bytes and will be truncated to %zu bytes",
                start,
                len);
        }
        memcpy(node, sp, len);
        node[len] = '\0';
        RlistAppendScalar(&liststart, node);
        count++;

        sp += end;
    }

    assert(count < max);
    RlistAppendScalar(&liststart, sp);

    pcre_free(pattern);

    return liststart;
}

Rlist *RlistLast(Rlist *start)
{
    if (start == NULL)
    {
        return NULL;
    }
    Rlist *rp = start;
    while (rp->next != NULL)
    {
        rp = rp->next;
    }
    return rp;
}

void RlistFilter(Rlist **list,
                 bool (*KeepPredicate)(void *, void *), void *predicate_user_data,
                 void (*DestroyItem)(void *))
{
    assert(KeepPredicate);
    Rlist *start = *list, *prev = NULL, *next;

    for (Rlist *rp = start; rp; rp = next)
    {
        next = rp->next;
        if (KeepPredicate(RlistScalarValue(rp), predicate_user_data))
        {
            prev = rp;
        }
        else
        {
            if (prev)
            {
                prev->next = next;
            }
            else
            {
                assert(rp == *list);
                *list = next;
            }

            if (DestroyItem)
            {
                DestroyItem(rp->val.item);
                rp->val.item = NULL;
            }

            rp->next = NULL;
            RlistDestroy(rp);
        }
    }
}

void RlistReverse(Rlist **list)
{
    Rlist *prev = NULL;
    while (*list)
    {
        Rlist *tmp = *list;
        *list = (*list)->next;
        tmp->next = prev;
        prev = tmp;
    }
    *list = prev;
}

void RlistWrite(Writer *writer, const Rlist *list)
{
    WriterWrite(writer, " {");

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        RvalWriteQuoted(writer, rp->val);

        if (rp->next != NULL)
        {
            WriterWriteChar(writer, ',');
        }
    }

    WriterWriteChar(writer, '}');
}

void ScalarWrite(Writer *writer, const char *s, bool quote)
{
    if (quote)
    {
        WriterWriteChar(writer, '"');
    }
    for (; *s; s++)
    {
        if (*s == '"')
        {
            WriterWriteChar(writer, '\\');
        }
        WriterWriteChar(writer, *s);
    }
    if (quote)
    {
        WriterWriteChar(writer, '"');
    }
}

static void RvalWriteParts(Writer *writer, const void* item, RvalType type, bool quote)
{
    if (item == NULL)
    {
        return;
    }

    switch (type)
    {
    case RVAL_TYPE_SCALAR:
        ScalarWrite(writer, item, quote);
        break;

    case RVAL_TYPE_LIST:
        RlistWrite(writer, item);
        break;

    case RVAL_TYPE_FNCALL:
        FnCallWrite(writer, item);
        break;

    case RVAL_TYPE_NOPROMISEE:
        WriterWrite(writer, "(no-one)");
        break;

    case RVAL_TYPE_CONTAINER:
        JsonWrite(writer, item, 0);
        break;
    }
}

void RvalWrite(Writer *writer, Rval rval)
{
    RvalWriteParts(writer, rval.item, rval.type, false);
}

void RvalWriteQuoted(Writer *writer, Rval rval)
{
    RvalWriteParts(writer, rval.item, rval.type, true);
}

char *RvalToString(Rval rval)
{
    Writer *w = StringWriter();
    RvalWrite(w, rval);
    return StringWriterClose(w);
}

char *RlistToString(const Rlist *rlist)
{
    Writer *w = StringWriter();
    RlistWrite(w, rlist);
    return StringWriterClose(w);
}

unsigned RvalHash(Rval rval, unsigned seed)
{
    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        return StringHash(RvalScalarValue(rval), seed);
    case RVAL_TYPE_FNCALL:
        return FnCallHash(RvalFnCallValue(rval), seed);
    case RVAL_TYPE_LIST:
        return RlistHash(RvalRlistValue(rval), seed);
    case RVAL_TYPE_NOPROMISEE:
        /* TODO modulus operation is biasing results. */
        return (seed + 1);
    default:
        ProgrammingError("Unhandled case in switch: %d", rval.type);
    }
}

unsigned int RlistHash(const Rlist *list, unsigned seed)
{
    unsigned hash = seed;
    for (const Rlist *rp = list; rp; rp = rp->next)
    {
        hash = RvalHash(rp->val, hash);
    }
    return hash;
}

unsigned int RlistHash_untyped(const void *list, unsigned seed)
{
    return RlistHash(list, seed);
}


static JsonElement *FnCallToJson(const FnCall *fp)
{
    assert(fp);

    JsonElement *object = JsonObjectCreate(3);

    JsonObjectAppendString(object, "name", fp->name);
    JsonObjectAppendString(object, "type", "function-call");

    JsonElement *argsArray = JsonArrayCreate(5);

    for (Rlist *rp = fp->args; rp != NULL; rp = rp->next)
    {
        switch (rp->val.type)
        {
        case RVAL_TYPE_SCALAR:
            JsonArrayAppendString(argsArray, RlistScalarValue(rp));
            break;

        case RVAL_TYPE_FNCALL:
            JsonArrayAppendObject(argsArray, FnCallToJson(RlistFnCallValue(rp)));
            break;

        default:
            assert(false && "Unknown argument type");
            break;
        }
    }
    JsonObjectAppendArray(object, "arguments", argsArray);

    return object;
}

static JsonElement *RlistToJson(Rlist *list)
{
    JsonElement *array = JsonArrayCreate(RlistLen(list));

    for (Rlist *rp = list; rp; rp = rp->next)
    {
        switch (rp->val.type)
        {
        case RVAL_TYPE_SCALAR:
            JsonArrayAppendString(array, RlistScalarValue(rp));
            break;

        case RVAL_TYPE_LIST:
            JsonArrayAppendArray(array, RlistToJson(RlistRlistValue(rp)));
            break;

        case RVAL_TYPE_FNCALL:
            JsonArrayAppendObject(array, FnCallToJson(RlistFnCallValue(rp)));
            break;

        default:
            ProgrammingError("Unsupported item type in rlist");
            break;
        }
    }

    return array;
}

JsonElement *RvalToJson(Rval rval)
{
    /* Only empty Rlist can be NULL. */
    assert(rval.item || rval.type == RVAL_TYPE_LIST);

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        return JsonStringCreate(RvalScalarValue(rval));
    case RVAL_TYPE_LIST:
        return RlistToJson(RvalRlistValue(rval));
    case RVAL_TYPE_FNCALL:
        return FnCallToJson(RvalFnCallValue(rval));
    case RVAL_TYPE_CONTAINER:
        return JsonCopy(RvalContainerValue(rval));
    case RVAL_TYPE_NOPROMISEE:
        assert(false);
        return JsonObjectCreate(1);
    }

    assert(false);
    return NULL;
}

/**
 * @brief Flattens an Rlist by expanding naked scalar list-variable
 *        members. Flattening is only one-level deep.
 */
void RlistFlatten(EvalContext *ctx, Rlist **list)
{
    Rlist *next;
    for (Rlist *rp = *list; rp != NULL; rp = next)
    {
        next = rp->next;

        if (rp->val.type == RVAL_TYPE_SCALAR      &&
            IsNakedVar(RlistScalarValue(rp), '@'))
        {
            char naked[CF_MAXVARSIZE];
            GetNaked(naked, RlistScalarValue(rp));

            /* Make sure there are no inner expansions to take place, like if
             * rp was "@{blah_$(blue)}".  */
            if (!IsExpandable(naked))
            {
                Log(LOG_LEVEL_DEBUG,
                    "Flattening slist: %s", RlistScalarValue(rp));

                VarRef *ref = VarRefParse(naked);
                DataType value_type;
                const void *value = EvalContextVariableGet(ctx, ref, &value_type);
                VarRefDestroy(ref);

                if (value_type == CF_DATA_TYPE_NONE)
                {
                    assert(value == NULL);
                    continue;                         /* undefined variable */
                }

                if (DataTypeToRvalType(value_type) != RVAL_TYPE_LIST)
                {
                    Log(LOG_LEVEL_WARNING,
                        "'%s' failed - variable is not list but %s",
                        RlistScalarValue(rp), DataTypeToString(value_type));
                    continue;
                }

                /* NOTE: Remember that value can be NULL as an empty Rlist. */

                /* at_node: just a mnemonic name for the
                            list node with @{blah}. */
                Rlist *at_node      = rp;
                Rlist *insert_after = at_node;
                for (const Rlist *rp2 = value; rp2 != NULL; rp2 = rp2->next)
                {
                    assert(insert_after != NULL);

                    RlistInsertAfter(insert_after, RvalCopy(rp2->val));
                    insert_after = insert_after->next;
                }

                /* Make sure we won't miss any element. */
                assert(insert_after->next == next);
                RlistDestroyEntry(list, at_node);   /* Delete @{blah} entry */

                char *list_s = RlistToString(*list);
                Log(LOG_LEVEL_DEBUG, "Flattened slist: %s", list_s);
                free(list_s);
            }
        }
    }
}

bool RlistEqual(const Rlist *list1, const Rlist *list2)
{
    const Rlist *rp1, *rp2;

    for (rp1 = list1, rp2 = list2; rp1 != NULL && rp2 != NULL; rp1 = rp1->next, rp2 = rp2->next)
    {
        if (rp1->val.item != NULL &&
            rp2->val.item != NULL)
        {
            if (rp1->val.type == RVAL_TYPE_FNCALL || rp2->val.type == RVAL_TYPE_FNCALL)
            {
                return false;      // inconclusive
            }

            const Rlist *rc1 = rp1;
            const Rlist *rc2 = rp2;

            // Check for list nesting with { fncall(), "x" ... }

            if (rp1->val.type == RVAL_TYPE_LIST)
            {
                rc1 = rp1->val.item;
            }

            if (rp2->val.type == RVAL_TYPE_LIST)
            {
                rc2 = rp2->val.item;
            }

            if (IsCf3VarString(rc1->val.item) || IsCf3VarString(rp2->val.item))
            {
                return false;      // inconclusive
            }

            if (strcmp(rc1->val.item, rc2->val.item) != 0)
            {
                return false;
            }
        }
        else if ((rp1->val.item != NULL && rp2->val.item == NULL) ||
                 (rp1->val.item == NULL && rp2->val.item != NULL))
        {
            return false;
        }
        else
        {
            assert(rp1->val.item == NULL && rp2->val.item == NULL);
        }
    }

    return true;
}

bool RlistEqual_untyped(const void *list1, const void *list2)
{
    return RlistEqual(list1, list2);
}

/*******************************************************************/

static void RlistAppendContainerPrimitive(Rlist **list, const JsonElement *primitive)
{
    assert(JsonGetElementType(primitive) == JSON_ELEMENT_TYPE_PRIMITIVE);

    switch (JsonGetPrimitiveType(primitive))
    {
    case JSON_PRIMITIVE_TYPE_BOOL:
        RlistAppendScalar(list, JsonPrimitiveGetAsBool(primitive) ? "true" : "false");
        break;
    case JSON_PRIMITIVE_TYPE_INTEGER:
        {
            char *str = StringFromLong(JsonPrimitiveGetAsInteger(primitive));
            RlistAppendScalar(list, str);
            free(str);
        }
        break;
    case JSON_PRIMITIVE_TYPE_REAL:
        {
            char *str = StringFromDouble(JsonPrimitiveGetAsReal(primitive));
            RlistAppendScalar(list, str);
            free(str);
        }
        break;
    case JSON_PRIMITIVE_TYPE_STRING:
        RlistAppendScalar(list, JsonPrimitiveGetAsString(primitive));
        break;

    case JSON_PRIMITIVE_TYPE_NULL:
        break;
    }
}

Rlist *RlistFromContainer(const JsonElement *container)
{
    Rlist *list = NULL;

    switch (JsonGetElementType(container))
    {
    case JSON_ELEMENT_TYPE_PRIMITIVE:
        RlistAppendContainerPrimitive(&list, container);
        break;

    case JSON_ELEMENT_TYPE_CONTAINER:
        {
            JsonIterator iter = JsonIteratorInit(container);
            const JsonElement *child;

            while (NULL != (child = JsonIteratorNextValue(&iter)))
            {
                if (JsonGetElementType(child) == JSON_ELEMENT_TYPE_PRIMITIVE)
                {
                    RlistAppendContainerPrimitive(&list, child);
                }
            }
        }
        break;
    }

    return list;
}
