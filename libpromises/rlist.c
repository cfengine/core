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

#include <rlist.h>

#include <files_names.h>
#include <conversion.h>
#include <expand.h>
#include <matching.h>
#include <scope.h>
#include <fncall.h>
#include <string_lib.h>
#include <mutex.h>
#include <misc_lib.h>
#include <assoc.h>
#include <env_context.h>


static Rlist *RlistPrependRval(Rlist **start, Rval rval);

RvalType DataTypeToRvalType(DataType datatype)
{
    switch (datatype)
    {
    case DATA_TYPE_BODY:
    case DATA_TYPE_BUNDLE:
    case DATA_TYPE_CONTEXT:
    case DATA_TYPE_COUNTER:
    case DATA_TYPE_INT:
    case DATA_TYPE_INT_RANGE:
    case DATA_TYPE_OPTION:
    case DATA_TYPE_REAL:
    case DATA_TYPE_REAL_RANGE:
    case DATA_TYPE_STRING:
        return RVAL_TYPE_SCALAR;

    case DATA_TYPE_CONTEXT_LIST:
    case DATA_TYPE_INT_LIST:
    case DATA_TYPE_OPTION_LIST:
    case DATA_TYPE_REAL_LIST:
    case DATA_TYPE_STRING_LIST:
        return RVAL_TYPE_LIST;

    case DATA_TYPE_CONTAINER:
        return RVAL_TYPE_CONTAINER;

    case DATA_TYPE_NONE:
        return RVAL_TYPE_NOPROMISEE;
    }

    ProgrammingError("DataTypeToRvalType, unhandled");
}

char *RlistScalarValue(const Rlist *rlist)
{
    if (rlist->val.type != RVAL_TYPE_SCALAR)
    {
        ProgrammingError("Rlist value contains type %c instead of expected scalar", rlist->val.type);
    }

    return rlist->val.item;
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

JsonElement *RvalContainerValue(Rval rval)
{
    if (rval.type != RVAL_TYPE_CONTAINER)
    {
        ProgrammingError("Rval contain type %c instead of expected container", rval.type);
    }

    return rval.item;
}

/*******************************************************************/

Rlist *RlistKeyIn(Rlist *list, const char *key)
{
    for (Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (rp->val.type != RVAL_TYPE_SCALAR)
        {
            continue;
        }

        if (strcmp(RlistScalarValue(rp), key) == 0)
        {
            return rp;
        }
    }

    return NULL;
}

/*******************************************************************/

bool RlistIsStringIn(const Rlist *list, const char *s)
{
    if (s == NULL || list == NULL)
    {
        return false;
    }

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (rp->val.type != RVAL_TYPE_SCALAR)
        {
            continue;
        }

        if (strcmp(s, RlistScalarValue(rp)) == 0)
        {
            return true;
        }
    }

    return false;
}

/*******************************************************************/

bool RlistIsIntIn(const Rlist *list, int i)
{
    char s[CF_SMALLBUF];

    snprintf(s, CF_SMALLBUF - 1, "%d", i);

    if (list == NULL)
    {
        return false;
    }

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (rp->val.type != RVAL_TYPE_SCALAR)
        {
            continue;
        }

        if (strcmp(s, RlistScalarValue(rp)) == 0)
        {
            return true;
        }
    }

    return false;
}

/*******************************************************************/

bool RlistIsInListOfRegex(EvalContext *ctx, const Rlist *list, const char *str)
{
    if (str == NULL || list == NULL)
    {
        return false;
    }

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (rp->val.type != RVAL_TYPE_SCALAR)
        {
            continue;
        }

        if (FullTextMatch(ctx, RlistScalarValue(rp), str))
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

    if (rval.item)
    {
        return ((Rval) {xstrdup((const char *) rval.item), RVAL_TYPE_SCALAR});
    }
    else
    {
        return ((Rval) {xstrdup(""), RVAL_TYPE_SCALAR});
    }
}

Rlist *RlistAppendRval(Rlist **start, Rval rval)
{
    Rlist *rp = xmalloc(sizeof(Rlist));

    if (*start == NULL)
    {
        *start = rp;
    }
    else
    {
        Rlist *lp = NULL;
        for (lp = *start; lp->next != NULL; lp = lp->next)
        {
        }

        lp->next = rp;
    }

    rp->val = rval;

    ThreadLock(cft_lock);

    rp->next = NULL;

    ThreadUnlock(cft_lock);

    return rp;
}

Rval RvalNew(const void *item, RvalType type)
{
    switch (type)
    {
    case RVAL_TYPE_SCALAR:
        return (Rval) { xstrdup(item), RVAL_TYPE_SCALAR };

    case RVAL_TYPE_FNCALL:
        return (Rval) { FnCallCopy(item), RVAL_TYPE_FNCALL };

    case RVAL_TYPE_LIST:
        return (Rval) { RlistCopy(item), RVAL_TYPE_LIST };

    case RVAL_TYPE_CONTAINER:
        return (Rval) { JsonCopy(item), RVAL_TYPE_CONTAINER };

    case RVAL_TYPE_NOPROMISEE:
        return ((Rval) {NULL, type});
    }

    assert(false);
    return ((Rval) { NULL, RVAL_TYPE_NOPROMISEE });
}

Rval RvalCopy(Rval rval)
{
    return RvalNew(rval.item, rval.type);
}

/*******************************************************************/

Rlist *RlistCopy(const Rlist *list)
{
    Rlist *start = NULL;

    if (list == NULL)
    {
        return NULL;
    }

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        RlistAppendRval(&start, RvalCopy(rp->val));
    }

    return start;
}

/*******************************************************************/

void RlistDestroy(Rlist *list)
/* Delete an rlist and all its references */
{
    Rlist *rl, *next;

    if (list != NULL)
    {
        for (rl = list; rl != NULL; rl = next)
        {
            next = rl->next;

            if (rl->val.item)
            {
                RvalDestroy(rl->val);
            }

            free(rl);
        }
    }
}

/*******************************************************************/

Rlist *RlistAppendScalarIdemp(Rlist **start, const char *scalar)
{
    if (!RlistKeyIn(*start, scalar))
    {
        return RlistAppendScalar(start, scalar);
    }
    else
    {
        return NULL;
    }
}

Rlist *RlistPrependScalarIdemp(Rlist **start, const char *scalar)
{
    if (!RlistKeyIn(*start, scalar))
    {
        return RlistPrepend(start, scalar, RVAL_TYPE_SCALAR);
    }
    else
    {
        return NULL;
    }
}

Rlist *RlistAppendScalar(Rlist **start, const char *scalar)
{
    return RlistAppendRval(start, RvalCopyScalar((Rval) { (char *)scalar, RVAL_TYPE_SCALAR }));
}

Rlist *RlistAppendFnCall(Rlist **start, const FnCall *fn)
{
    return RlistAppendRval(start, RvalNew(fn, RVAL_TYPE_FNCALL));
}

Rlist *RlistAppend(Rlist **start, const void *item, RvalType type)
{
    Rlist *rp, *lp = *start;

    switch (type)
    {
    case RVAL_TYPE_SCALAR:
        return RlistAppendScalar(start, item);

    case RVAL_TYPE_FNCALL:
        break;

    case RVAL_TYPE_LIST:
        for (rp = (Rlist *) item; rp != NULL; rp = rp->next)
        {
            lp = RlistAppendRval(start, RvalCopy(rp->val));
        }

        return lp;

    default:
        Log(LOG_LEVEL_DEBUG, "Cannot append %c to rval-list '%s'", type, (char *) item);
        return NULL;
    }

    rp = xmalloc(sizeof(Rlist));

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

    rp->val = RvalCopy((Rval) {(void *) item, type});

    ThreadLock(cft_lock);

    rp->next = NULL;

    ThreadUnlock(cft_lock);

    return rp;
}

/*******************************************************************/

static Rlist *RlistPrependRval(Rlist **start, Rval rval)
{
    Rlist *rp = xmalloc(sizeof(Rlist));

    rp->next = *start;
    rp->val = rval;
    ThreadLock(cft_lock);
    *start = rp;
    ThreadUnlock(cft_lock);
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

Rlist *RlistParseShown(char *string)
{
    Rlist *newlist = NULL, *splitlist, *rp;
    char value[CF_MAXVARSIZE];

/* Parse a string representation generated by ShowList and turn back into Rlist */

    splitlist = RlistFromSplitString(string, ',');

    for (rp = splitlist; rp != NULL; rp = rp->next)
    {
        sscanf(RlistScalarValue(rp), "%*[{ '\"]%255[^'\"]", value);
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

 @retval 0: successful >0: failed
 */
static int LaunchParsingMachine(char *str, Rlist **newlist)
{
    char *s = str;
    state current_state = ST_OPENED;
    int ret;

    char snatched[CF_MAXVARSIZE];
    snatched[0]='\0';
    char *sn = NULL;

    while (current_state != ST_CLOSED && *s)
    {
        switch(current_state) {
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
                    sn=snatched;
                    current_state = ST_ELM1;
                }
                else if (CLASS_START2(*s))
                {
                      sn=snatched; 
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
                    if (sn==NULL)
                    {
                        sn=snatched;
                    }
                    *sn='\0'; 
                    RlistAppendScalar(newlist, snatched);
                    sn=NULL;
                    current_state = ST_END1;
                }
                else if (CLASS_ANY2(*s))
                {
                    if (sn==NULL)
                    {
                        sn=snatched;
                    }
                    *sn=*s;
                    sn++; 
                    current_state = ST_ELM1;
                }
                s++;
                break;
            case ST_ELM2:
                if (CLASS_END2(*s))
                {
                    if (sn==NULL)
                    {
                        sn=snatched;
                    }
                    *sn='\0'; 
                    RlistAppendScalar(newlist, snatched);
                    sn=NULL; 
                    current_state = ST_END2;
                }
                else if (CLASS_ANY3(*s))
                {
                    if (sn==NULL) 
                    {
                        sn=snatched;
                    }
                    *sn=*s;
                    sn++;
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

    return 0;

clean:
    if (newlist)
    {
        RlistDestroy(*newlist);
    }
    return ret;
}

Rlist *RlistParseString(char *string)
{
    Rlist *newlist = NULL;
    int ret;

    ret = LaunchParsingMachine(string, &newlist);

    if (!ret)
    {
        return newlist;
    }
    else
    {
        return NULL;
    }
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
        ThreadLock(cft_lock);
        free(RvalScalarValue(rval));
        ThreadUnlock(cft_lock);
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
    Rlist *rp, *sp;

    if (entry != NULL)
    {
        if (entry->val.item)
        {
            free(entry->val.item);
        }

        sp = entry->next;

        if (entry == *liststart)
        {
            *liststart = sp;
        }
        else
        {
            for (rp = *liststart; rp->next != entry; rp = rp->next)
            {
            }

            rp->next = sp;
        }

        free((char *) entry);
    }
}

/*******************************************************************/

/*
 * Copies from <from> to <to>, reading up to <len> characters from <from>,
 * stopping at first <sep>.
 *
 * \<sep> is not counted as the separator, but copied to <to> as <sep>.
 * Any other escape sequences are not supported.
 */
static int SubStrnCopyChr(char *to, const char *from, int len, char sep)
{
    char *sto = to;
    int count = 0;

    memset(to, 0, len);

    if (from == NULL)
    {
        return 0;
    }

    if (from && (strlen(from) == 0))
    {
        return 0;
    }

    for (const char *sp = from; *sp != '\0'; sp++)
    {
        if (count > len - 1)
        {
            break;
        }

        if ((*sp == '\\') && (*(sp + 1) == sep))
        {
            *sto++ = *++sp;
        }
        else if (*sp == sep)
        {
            break;
        }
        else
        {
            *sto++ = *sp;
        }

        count++;
    }

    return count;
}

Rlist *RlistFromSplitString(const char *string, char sep)
 /* Splits a string containing a separator like "," 
    into a linked list of separate items, supports
    escaping separators, e.g. \, */
{
    if (string == NULL)
    {
        return NULL;
    }

    Rlist *liststart = NULL;
    char node[CF_MAXVARSIZE];
    int maxlen = strlen(string);

    for (const char *sp = string; *sp != '\0'; sp++)
    {
        if (*sp == '\0' || sp > string + maxlen)
        {
            break;
        }

        memset(node, 0, CF_MAXVARSIZE);

        sp += SubStrnCopyChr(node, sp, CF_MAXVARSIZE, sep);

        RlistAppendScalar(&liststart, node);
    }

    return liststart;
}

/*******************************************************************/

Rlist *RlistFromSplitRegex(EvalContext *ctx, const char *string, const char *regex, int max, int blanks)
 /* Splits a string containing a separator like "," 
    into a linked list of separate items, */
// NOTE: this has a bad side-effect of creating scope match and variables,
//       see RegExMatchSubString in matching.c - could leak memory
{
    Rlist *liststart = NULL;
    char node[CF_MAXVARSIZE];
    int start, end;
    int count = 0;

    if (string == NULL)
    {
        return NULL;
    }

    const char *sp = string;

    while ((count < max) && BlockTextMatch(ctx, regex, sp, &start, &end))
    {
        if (end == 0)
        {
            break;
        }

        memset(node, 0, CF_MAXVARSIZE);
        strncpy(node, sp, start);

        if (blanks || strlen(node) > 0)
        {
            RlistAppendScalar(&liststart, node);
            count++;
        }

        sp += end;
    }

    if (count < max)
    {
        memset(node, 0, CF_MAXVARSIZE);
        strncpy(node, sp, CF_MAXVARSIZE - 1);

        if ((blanks && sp != string) || strlen(node) > 0)
        {
            RlistAppendScalar(&liststart, node);
        }
    }

    return liststart;
}

Rlist *RlistLast(Rlist *start)
{
    if (start == NULL)
    {
        return NULL;
    }
    Rlist *rp;
    for (rp = start; rp->next; rp = rp->next);
    return rp;
}

void RlistFilter(Rlist **list, bool (*KeepPredicate)(void *, void *), void *predicate_user_data, void (*DestroyItem)(void *))
{
    assert(KeepPredicate);

    Rlist *start = *list;
    Rlist *prev = NULL;

    for (Rlist *rp = start; rp;)
    {
        if (!KeepPredicate(RlistScalarValue(rp), predicate_user_data))
        {
            if (prev)
            {
                prev->next = rp->next;
            }
            else
            {
                *list = rp->next;
            }

            if (DestroyItem)
            {
                DestroyItem(rp->val.item);
                rp->val.item = NULL;
            }

            Rlist *next = rp->next;
            rp->next = NULL;
            RlistDestroy(rp);
            rp = next;
        }
        else
        {
            prev = rp;
            rp = rp->next;
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

/* Human-readable serialization */

static void FnCallWrite(Writer *writer, const FnCall *call)
{
    WriterWrite(writer, call->name);
    WriterWriteChar(writer, '(');

    for (const Rlist *rp = call->args; rp != NULL; rp = rp->next)
    {
        switch (rp->val.type)
        {
        case RVAL_TYPE_SCALAR:
            WriterWrite(writer, RlistScalarValue(rp));
            break;

        case RVAL_TYPE_FNCALL:
            FnCallWrite(writer, RlistFnCallValue(rp));
            break;

        default:
            WriterWrite(writer, "(** Unknown argument **)\n");
            break;
        }

        if (rp->next != NULL)
        {
            WriterWriteChar(writer, ',');
        }
    }

    WriterWriteChar(writer, ')');
}

void RlistWrite(Writer *writer, const Rlist *list)
{
    WriterWrite(writer, " {");

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        WriterWriteChar(writer, '\'');
        RvalWrite(writer, rp->val);
        WriterWriteChar(writer, '\'');

        if (rp->next != NULL)
        {
            WriterWriteChar(writer, ',');
        }
    }

    WriterWriteChar(writer, '}');
}

/* Note: only single quotes are escaped, as they are used in RlistWrite to
   delimit strings. If double quotes would be escaped, they would be mangled by
   RlistParseShown */

static void ScalarWrite(Writer *w, const char *s)
{
    for (; *s; s++)
    {
        if (*s == '\'')
        {
            WriterWriteChar(w, '\\');
        }
        WriterWriteChar(w, *s);
    }
}

void RvalWrite(Writer *writer, Rval rval)
{
    if (rval.item == NULL)
    {
        return;
    }

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        ScalarWrite(writer, RvalScalarValue(rval));
        break;

    case RVAL_TYPE_LIST:
        RlistWrite(writer, RvalRlistValue(rval));
        break;

    case RVAL_TYPE_FNCALL:
        FnCallWrite(writer, RvalFnCallValue(rval));
        break;

    case RVAL_TYPE_NOPROMISEE:
        WriterWrite(writer, "(no-one)");
        break;

    case RVAL_TYPE_CONTAINER:
        JsonWrite(writer, RvalContainerValue(rval), 0);
        break;
    }
}

void RlistShow(FILE *fp, const Rlist *list)
{
    Writer *w = FileWriter(fp);
    RlistWrite(w, list);
    FileWriterDetach(w);
}

void RvalShow(FILE *fp, Rval rval)
{
    Writer *w = FileWriter(fp);
    RvalWrite(w, rval);
    FileWriterDetach(w);
}

unsigned RvalHash(Rval rval, unsigned seed, unsigned max)
{
    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        return StringHash(RvalScalarValue(rval), seed, max);
    case RVAL_TYPE_FNCALL:
        return FnCallHash(RvalFnCallValue(rval), seed, max);
    case RVAL_TYPE_LIST:
        return RlistHash(RvalRlistValue(rval), seed, max);
    case RVAL_TYPE_NOPROMISEE:
        return (seed + 1) % max;
    default:
        ProgrammingError("Unhandled case in switch: %d", rval.type);
    }
}

unsigned RlistHash(const Rlist *list, unsigned seed, unsigned max)
{
    unsigned hash = seed;
    for (const Rlist *rp = list; rp; rp = rp->next)
    {
        hash = RvalHash(rp->val, hash, max);
    }
    return hash;
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
            assert(false && "Unsupported item type in rlist");
            break;
        }
    }

    return array;
}

JsonElement *RvalToJson(Rval rval)
{
    assert(rval.item);

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

void RlistFlatten(EvalContext *ctx, Rlist **list)
{
    for (Rlist *rp = *list; rp != NULL;)
    {
        if (rp->val.type != RVAL_TYPE_SCALAR)
        {
            rp = rp->next;
            continue;
        }

        char naked[CF_BUFSIZE] = "";
        if (IsNakedVar(RlistScalarValue(rp), '@'))
        {
            GetNaked(naked, RlistScalarValue(rp));

            if (!IsExpandable(naked))
            {
                Rval rv;
                VarRef *ref = VarRefParse(naked);

                bool var_found = EvalContextVariableGet(ctx, ref, &rv, NULL);

                VarRefDestroy(ref);

                if (var_found)
                {
                    switch (rv.type)
                    {
                    case RVAL_TYPE_LIST:
                        for (const Rlist *srp = rv.item; srp != NULL; srp = srp->next)
                        {
                            RlistAppendRval(list, RvalCopy(srp->val));
                        }
                        Rlist *next = rp->next;
                        RlistDestroyEntry(list, rp);
                        rp = next;
                        continue;

                    default:
                        ProgrammingError("List variable does not resolve to a list");
                        RlistAppendRval(list, RvalCopy(rp->val));
                        break;
                    }
                }
            }
        }
        rp = rp->next;
    }
}
