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

#include "rlist.h"

#include "files_names.h"
#include "conversion.h"
#include "expand.h"
#include "matching.h"
#include "vars.h"
#include "cfstream.h"
#include "fncall.h"
#include "string_lib.h"
#include "transaction.h"
#include "logging.h"
#include "misc_lib.h"

#include <assert.h>

/*******************************************************************/

char *RlistScalarValue(const Rlist *rlist)
{
    if (rlist->type != RVAL_TYPE_SCALAR)
    {
        ProgrammingError("Internal error: Rlist value contains type %c instead of expected scalar", rlist->type);
    }

    return (char *) rlist->item;
}

/*******************************************************************/

FnCall *RlistFnCallValue(const Rlist *rlist)
{
    if (rlist->type != RVAL_TYPE_FNCALL)
    {
        ProgrammingError("Internal error: Rlist value contains type %c instead of expected FnCall", rlist->type);
    }

    return (FnCall *) rlist->item;
}

/*******************************************************************/

Rlist *RlistRlistValue(const Rlist *rlist)
{
    if (rlist->type != RVAL_TYPE_LIST)
    {
        ProgrammingError("Internal error: Rlist value contains type %c instead of expected List", rlist->type);
    }

    return (Rlist *) rlist->item;
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
        ProgrammingError("Internal error: Rval contains type %c instead of expected FnCall", rval.type);
    }

    return rval.item;
}

/*******************************************************************/

Rlist *RvalRlistValue(Rval rval)
{
    if (rval.type != RVAL_TYPE_LIST)
    {
        ProgrammingError("Internal error: Rval contain type %c instead of expected List", rval.type);
    }

    return rval.item;
}

/*******************************************************************/

Rlist *RlistKeyIn(Rlist *list, char *key)
{
    for (Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (rp->type != RVAL_TYPE_SCALAR)
        {
            continue;
        }

        if (strcmp((char *) rp->item, key) == 0)
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
        if (rp->type != RVAL_TYPE_SCALAR)
        {
            continue;
        }

        if (strcmp(s, rp->item) == 0)
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
        if (rp->type != RVAL_TYPE_SCALAR)
        {
            continue;
        }

        if (strcmp(s, rp->item) == 0)
        {
            return true;
        }
    }

    return false;
}

/*******************************************************************/

bool RlistIsInListOfRegex(const Rlist *list, const char *str)
{
    if (str == NULL || list == NULL)
    {
        return false;
    }

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (rp->type != RVAL_TYPE_SCALAR)
        {
            continue;
        }

        if (FullTextMatch(rp->item, str))
        {
            return true;
        }
    }

    return false;
}

/*******************************************************************/

Rval RvalCopy(Rval rval)
{
    Rlist *rp, *srp, *start = NULL;
    FnCall *fp;

    CfDebug("CopyRvalItem(%c)\n", rval.type);

    if (rval.item == NULL)
    {
        switch (rval.type)
        {
        case RVAL_TYPE_SCALAR:
            return (Rval) {xstrdup(""), RVAL_TYPE_SCALAR};

        case RVAL_TYPE_LIST:
            return (Rval) {NULL, RVAL_TYPE_LIST};

        default:
            break;
        }
    }

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        /* the rval is just a string */
        return (Rval) {xstrdup((char *) rval.item), RVAL_TYPE_SCALAR};

    case RVAL_TYPE_ASSOC:
        return (Rval) {CopyAssoc((CfAssoc *) rval.item), RVAL_TYPE_ASSOC };

    case RVAL_TYPE_FNCALL:
        /* the rval is a fncall */
        fp = (FnCall *) rval.item;
        return (Rval) {FnCallCopy(fp), RVAL_TYPE_FNCALL};

    case RVAL_TYPE_LIST:
        /* The rval is an embedded rlist (2d) */
        for (rp = (Rlist *) rval.item; rp != NULL; rp = rp->next)
        {
            char naked[CF_BUFSIZE] = "";

            if (IsNakedVar(rp->item, '@'))
            {
                GetNaked(naked, rp->item);

                Rval rv = { NULL, RVAL_TYPE_SCALAR };  /* FIXME: why it needs to be initialized? */
                if (GetVariable(CONTEXTID, naked, &rv) != DATA_TYPE_NONE)
                {
                    switch (rv.type)
                    {
                    case RVAL_TYPE_LIST:
                        for (srp = (Rlist *) rv.item; srp != NULL; srp = srp->next)
                        {
                            RlistAppend(&start, srp->item, srp->type);
                        }
                        break;

                    default:
                        RlistAppend(&start, rp->item, rp->type);
                        break;
                    }
                }
                else
                {
                    RlistAppend(&start, rp->item, rp->type);
                }
            }
            else
            {
                RlistAppend(&start, rp->item, rp->type);
            }
        }

        return (Rval) {start, RVAL_TYPE_LIST};

    default:
        break;
    }

    CfOut(OUTPUT_LEVEL_VERBOSE, "", "Unknown type %c in CopyRvalItem - should not happen", rval.type);
    return (Rval) {NULL, rval.type};
}

/*******************************************************************/

Rlist *RlistCopy(const Rlist *list)
{
    Rlist *start = NULL;

    CfDebug("CopyRlist()\n");

    if (list == NULL)
    {
        return NULL;
    }

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        RlistAppend(&start, rp->item, rp->type);        // allocates memory for objects
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

            if (rl->item != NULL)
            {
                RvalDestroy((Rval) {rl->item, rl->type});
            }

            free(rl);
        }
    }
}

/*******************************************************************/

Rlist *RlistAppendScalarIdemp(Rlist **start, void *item, char type)
{
    char *scalar = item;

    if (type != RVAL_TYPE_SCALAR)
    {
        ProgrammingError("Cannot append non-scalars to lists");
    }

    if (!RlistKeyIn(*start, (char *) item))
    {
        return RlistAppend(start, scalar, type);
    }
    else
    {
        return NULL;
    }
}

/*******************************************************************/

Rlist *RlistPrependScalarIdemp(Rlist **start, void *item, char type)
{
    char *scalar = item;

    if (type != RVAL_TYPE_SCALAR)
    {
        ProgrammingError("Cannot append non-scalars to lists");
    }

    if (!RlistKeyIn(*start, (char *) item))
    {
        return RlistPrepend(start, scalar, type);
    }
    else
    {
        return NULL;
    }
}

/*******************************************************************/

Rlist *RlistAppendIdemp(Rlist **start, void *item, char type)
{
    Rlist *rp, *ins = NULL;

    if (type == RVAL_TYPE_LIST)
    {
        for (rp = (Rlist *) item; rp != NULL; rp = rp->next)
        {
            ins = RlistAppendIdemp(start, rp->item, rp->type);
        }
        return ins;
    }

    if (!RlistKeyIn(*start, (char *) item))
    {
        return RlistAppend(start, (char *) item, type);
    }
    else
    {
        return NULL;
    }
}

/*******************************************************************/

Rlist *RlistAppendScalar(Rlist **start, void *item, char type)
{
    char *scalar = item;

    if (type != RVAL_TYPE_SCALAR)
    {
        ProgrammingError("Cannot append non-scalars to lists");
    }

    return RlistAppend(start, scalar, type);
}

/*******************************************************************/

Rlist *RlistPrependScalar(Rlist **start, void *item, char type)
{
    char *scalar = item;

    if (type != RVAL_TYPE_SCALAR)
    {
        ProgrammingError("Cannot append non-scalars to lists");
    }

    return RlistPrepend(start, scalar, type);
}

/*******************************************************************/

Rlist *RlistAppend(Rlist **start, const void *item, char type)
   /* Allocates new memory for objects - careful, could leak!  */
{
    Rlist *rp, *lp = *start;
    FnCall *fp;

    switch (type)
    {
    case RVAL_TYPE_SCALAR:
        CfDebug("Appending scalar to rval-list [%s]\n", (char *) item);
        break;

    case RVAL_TYPE_ASSOC:
        CfDebug("Appending assoc to rval-list [%s]\n", (char *) item);
        break;

    case RVAL_TYPE_FNCALL:
        CfDebug("Appending function to rval-list function call: ");
        fp = (FnCall *) item;
        if (DEBUG)
        {
            FnCallShow(stdout, fp);
        }
        CfDebug("\n");
        break;

    case RVAL_TYPE_LIST:
        CfDebug("Expanding and appending list object\n");

        for (rp = (Rlist *) item; rp != NULL; rp = rp->next)
        {
            lp = RlistAppend(start, rp->item, rp->type);
        }

        return lp;

    default:
        CfDebug("Cannot append %c to rval-list [%s]\n", type, (char *) item);
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

    rp->item = RvalCopy((Rval) {(void *) item, type}).item;
    rp->type = type;            /* scalar, builtin function */

    ThreadLock(cft_lock);

    if (type == RVAL_TYPE_LIST)
    {
        rp->state_ptr = rp->item;
    }
    else
    {
        rp->state_ptr = NULL;
    }

    rp->next = NULL;

    ThreadUnlock(cft_lock);

    return rp;
}

/*******************************************************************/

Rlist *RlistPrepend(Rlist **start, void *item, char type)
   /* heap memory for item must have already been allocated */
{
    Rlist *rp, *lp = *start;
    FnCall *fp;

    switch (type)
    {
    case RVAL_TYPE_SCALAR:
        CfDebug("Prepending scalar to rval-list [%s]\n", (char *) item);
        break;

    case RVAL_TYPE_LIST:

        CfDebug("Expanding and prepending list (ends up in reverse)\n");

        for (rp = (Rlist *) item; rp != NULL; rp = rp->next)
        {
            lp = RlistPrepend(start, rp->item, rp->type);
        }
        return lp;

    case RVAL_TYPE_FNCALL:
        CfDebug("Prepending function to rval-list function call: ");
        fp = (FnCall *) item;
        if (DEBUG)
        {
            FnCallShow(stdout, fp);
        }
        CfDebug("\n");
        break;
    default:
        CfDebug("Cannot prepend %c to rval-list [%s]\n", type, (char *) item);
        return NULL;
    }

    ThreadLock(cft_system);

    rp = xmalloc(sizeof(Rlist));

    ThreadUnlock(cft_system);

    rp->next = *start;
    rp->item = RvalCopy((Rval) {item, type}).item;
    rp->type = type;            /* scalar, builtin function */

    if (type == RVAL_TYPE_LIST)
    {
        rp->state_ptr = rp->item;
    }
    else
    {
        rp->state_ptr = NULL;
    }

    ThreadLock(cft_lock);
    *start = rp;
    ThreadUnlock(cft_lock);
    return rp;
}

/*******************************************************************/

Rlist *RlistAppendOrthog(Rlist **start, void *item, char type)
   /* Allocates new memory for objects - careful, could leak!  */
{
    Rlist *rp, *lp;
    CfAssoc *cp;

    CfDebug("OrthogAppendRlist\n");

    switch (type)
    {
    case RVAL_TYPE_LIST:
        CfDebug("Expanding and appending list object, orthogonally\n");
        break;
    default:
        CfDebug("Cannot append %c to rval-list [%s]\n", type, (char *) item);
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

// This is item is in fact a CfAssoc pointing to a list

    cp = (CfAssoc *) item;

// Note, we pad all iterators will a blank so the ptr arithmetic works
// else EndOfIteration will not see lists with only one element

    lp = RlistPrepend((Rlist **) &(cp->rval), CF_NULL_VALUE, RVAL_TYPE_SCALAR);
    rp->state_ptr = lp->next;   // Always skip the null value
    RlistAppend((Rlist **) &(cp->rval), CF_NULL_VALUE, RVAL_TYPE_SCALAR);

    rp->item = item;
    rp->type = RVAL_TYPE_LIST;
    rp->next = NULL;
    return rp;
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
        sscanf(rp->item, "%*[{ '\"]%255[^'\"]", value);
        RlistAppend(&newlist, value, RVAL_TYPE_SCALAR);
    }

    RlistDestroy(splitlist);
    return newlist;
}

/*******************************************************************/

void RlistShow(FILE *fp, const Rlist *list)
{
    fprintf(fp, " {");

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        fprintf(fp, "\'");
        RvalShow(fp, (Rval) {rp->item, rp->type});
        fprintf(fp, "\'");

        if (rp->next != NULL)
        {
            fprintf(fp, ",");
        }
    }
    fprintf(fp, "}");
}

/*******************************************************************/

int RlistPrint(char *buffer, int bufsize, const Rlist *list)
{
    StartJoin(buffer, "{", bufsize);

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (!JoinSilent(buffer, "'", bufsize))
        {
            EndJoin(buffer, "'}", bufsize);
            return false;
        }

        if (!RvalPrint(buffer, bufsize, (Rval) {rp->item, rp->type}))
        {
            EndJoin(buffer, "'}", bufsize);
            return false;
        }

        if (!JoinSilent(buffer, "'", bufsize))
        {
            EndJoin(buffer, "'}", bufsize);
            return false;
        }

        if (rp->next != NULL)
        {
            if (!JoinSilent(buffer, ",", bufsize))
            {
                EndJoin(buffer, "}", bufsize);
                return false;
            }
        }
    }

    EndJoin(buffer, "}", bufsize);

    return true;
}

/*******************************************************************/

static int PrintFnCall(char *buffer, int bufsize, const FnCall *fp)
{
    Rlist *rp;
    char work[CF_MAXVARSIZE];

    snprintf(buffer, bufsize, "%s(", fp->name);

    for (rp = fp->args; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case RVAL_TYPE_SCALAR:
            Join(buffer, (char *) rp->item, bufsize);
            break;

        case RVAL_TYPE_FNCALL:
            PrintFnCall(work, CF_MAXVARSIZE, (FnCall *) rp->item);
            Join(buffer, work, bufsize);
            break;

        default:
            break;
        }

        if (rp->next != NULL)
        {
            strcat(buffer, ",");
        }
    }

    strcat(buffer, ")");

    return strlen(buffer);
}

int RvalPrint(char *buffer, int bufsize, Rval rval)
{
    if (rval.item == NULL)
    {
        return 0;
    }

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        return JoinSilent(buffer, (const char *) rval.item, bufsize);
    case RVAL_TYPE_LIST:
        return RlistPrint(buffer, bufsize, (Rlist *) rval.item);
    case RVAL_TYPE_FNCALL:
        return PrintFnCall(buffer, bufsize, (FnCall *) rval.item);
    default:
        return 0;
    }
}

/*******************************************************************/

static JsonElement *FnCallToJson(const FnCall *fp)
{
    assert(fp);

    JsonElement *object = JsonObjectCreate(3);

    JsonObjectAppendString(object, "name", fp->name);
    JsonObjectAppendString(object, "type", "function-call");

    JsonElement *argsArray = JsonArrayCreate(5);

    for (Rlist *rp = fp->args; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case RVAL_TYPE_SCALAR:
            JsonArrayAppendString(argsArray, (const char *) rp->item);
            break;

        case RVAL_TYPE_FNCALL:
            JsonArrayAppendObject(argsArray, FnCallToJson((FnCall *) rp->item));
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
        switch (rp->type)
        {
        case RVAL_TYPE_SCALAR:
            JsonArrayAppendString(array, (const char *) rp->item);
            break;

        case RVAL_TYPE_LIST:
            JsonArrayAppendArray(array, RlistToJson((Rlist *) rp->item));
            break;

        case RVAL_TYPE_FNCALL:
            JsonArrayAppendObject(array, FnCallToJson((FnCall *) rp->item));
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
        return JsonStringCreate((const char *) rval.item);
    case RVAL_TYPE_LIST:
        return RlistToJson((Rlist *) rval.item);
    case RVAL_TYPE_FNCALL:
        return FnCallToJson((FnCall *) rval.item);
    default:
        assert(false && "Invalid rval type");
        return JsonStringCreate("");
    }
}

/*******************************************************************/

void RvalShow(FILE *fp, Rval rval)
{
    char buf[CF_BUFSIZE];

    if (rval.item == NULL)
    {
        return;
    }

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:
        EscapeQuotes((const char *) rval.item, buf, sizeof(buf));
        fprintf(fp, "%s", buf);
        break;

    case RVAL_TYPE_LIST:
        RlistShow(fp, (Rlist *) rval.item);
        break;

    case RVAL_TYPE_FNCALL:
        FnCallShow(fp, (FnCall *) rval.item);
        break;

    case RVAL_TYPE_NOPROMISEE:
        fprintf(fp, "(no-one)");
        break;

    default:
        break;
    }
}

/*******************************************************************/

void RvalDestroy(Rval rval)
{
    Rlist *clist, *next = NULL;

    CfDebug("DeleteRvalItem(%c)", rval.type);

    if (DEBUG)
    {
        RvalShow(stdout, rval);
    }

    CfDebug("\n");

    if (rval.item == NULL)
    {
        CfDebug("DeleteRval NULL\n");
        return;
    }

    switch (rval.type)
    {
    case RVAL_TYPE_SCALAR:

        ThreadLock(cft_lock);
        free((char *) rval.item);
        ThreadUnlock(cft_lock);
        break;

    case RVAL_TYPE_ASSOC:             /* What? */
        DeleteAssoc((CfAssoc *) rval.item);
        break;

    case RVAL_TYPE_LIST:

        /* rval is now a list whose first item is clist->item */

        for (clist = (Rlist *) rval.item; clist != NULL; clist = next)
        {

            next = clist->next;

            if (clist->item)
            {
                RvalDestroy((Rval) {clist->item, clist->type});
            }

            free(clist);
        }

        break;

    case RVAL_TYPE_FNCALL:

        FnCallDestroy((FnCall *) rval.item);
        break;

    default:
        CfDebug("Nothing to do\n");
        return;
    }
}

/*********************************************************************/

void RlistDestroyEntry(Rlist **liststart, Rlist *entry)
{
    Rlist *rp, *sp;

    if (entry != NULL)
    {
        if (entry->item != NULL)
        {
            free(entry->item);
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

Rlist *RlistAppendAlien(Rlist **start, void *item)
   /* Allocates new memory for objects - careful, could leak!  */
{
    Rlist *rp, *lp = *start;

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

    rp->item = item;
    rp->type = RVAL_TYPE_SCALAR;

    ThreadLock(cft_lock);

    rp->next = NULL;

    ThreadUnlock(cft_lock);
    return rp;
}

/*******************************************************************/

Rlist *RlistPrependAlien(Rlist **start, void *item)
   /* Allocates new memory for objects - careful, could leak!  */
{
    Rlist *rp;

    ThreadLock(cft_lock);

    rp = xmalloc(sizeof(Rlist));

    rp->next = *start;
    *start = rp;
    ThreadUnlock(cft_lock);

    rp->item = item;
    rp->type = RVAL_TYPE_SCALAR;
    return rp;
}

/*******************************************************************/
/* Stack                                                           */
/*******************************************************************/

/*
char *sp1 = xstrdup("String 1\n");
char *sp2 = xstrdup("String 2\n");
char *sp3 = xstrdup("String 3\n");

PushStack(&stack,(void *)sp1);
PopStack(&stack,(void *)&sp,sizeof(sp));
*/

void RlistPushStack(Rlist **liststart, void *item)
{
    Rlist *rp;

/* Have to keep track of types personally */

    rp = xmalloc(sizeof(Rlist));

    rp->next = *liststart;
    rp->item = item;
    rp->type = CF_STACK;
    *liststart = rp;
}

/*******************************************************************/

void RlistPopStack(Rlist **liststart, void **item, size_t size)
{
    Rlist *rp = *liststart;

    if (*liststart == NULL)
    {
        ProgrammingError("Attempt to pop from empty stack");
    }

    *item = rp->item;

    if (rp->next == NULL)       /* only one left */
    {
        *liststart = (void *) NULL;
    }
    else
    {
        *liststart = rp->next;
    }

    free((char *) rp);
}

/*******************************************************************/

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

    CfDebug("SplitStringAsRList(%s)\n", string);

    for (const char *sp = string; *sp != '\0'; sp++)
    {
        if (*sp == '\0' || sp > string + maxlen)
        {
            break;
        }

        memset(node, 0, CF_MAXVARSIZE);

        sp += SubStrnCopyChr(node, sp, CF_MAXVARSIZE, sep);

        RlistAppendScalar(&liststart, node, RVAL_TYPE_SCALAR);
    }

    return liststart;
}

/*******************************************************************/

Rlist *RlistFromSplitRegex(const char *string, const char *regex, int max, int blanks)
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

    CfDebug("\n\nSplit \"%s\" with regex \"%s\" (up to maxent %d)\n\n", string, regex, max);

    const char *sp = string;

    while ((count < max) && BlockTextMatch(regex, sp, &start, &end))
    {
        if (end == 0)
        {
            break;
        }

        memset(node, 0, CF_MAXVARSIZE);
        strncpy(node, sp, start);

        if (blanks || strlen(node) > 0)
        {
            RlistAppendScalar(&liststart, node, RVAL_TYPE_SCALAR);
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
            RlistAppendScalar(&liststart, node, RVAL_TYPE_SCALAR);
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

/*******************************************************************/

void RlistWrite(Writer *writer, const Rlist *list)
{
    WriterWrite(writer, " {");

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        WriterWriteChar(writer, '\'');
        RvalWrite(writer, (Rval) {rp->item, rp->type});
        WriterWriteChar(writer, '\'');

        if (rp->next != NULL)
        {
            WriterWriteChar(writer, ',');
        }
    }

    WriterWriteChar(writer, '}');
}

static void FnCallPrint(Writer *writer, const FnCall *call)
{
    for (const Rlist *rp = call->args; rp != NULL; rp = rp->next)
    {
        switch (rp->type)
        {
        case RVAL_TYPE_SCALAR:
            WriterWriteF(writer, "%s,", (const char *) rp->item);
            break;

        case RVAL_TYPE_FNCALL:
            FnCallPrint(writer, (FnCall *) rp->item);
            break;

        default:
            WriterWrite(writer, "(** Unknown argument **)\n");
            break;
        }
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
    {
        size_t buffer_size = (strlen((const char *) rval.item) * 2) + 1;
        char *buffer = xcalloc(buffer_size, sizeof(char));

        EscapeQuotes((const char *) rval.item, buffer, buffer_size);
        WriterWrite(writer, buffer);
        free(buffer);
    }
        break;

    case RVAL_TYPE_LIST:
        RlistWrite(writer, (Rlist *) rval.item);
        break;

    case RVAL_TYPE_FNCALL:
        FnCallPrint(writer, (FnCall *) rval.item);
        break;

    case RVAL_TYPE_NOPROMISEE:
        WriterWrite(writer, "(no-one)");
        break;

    case RVAL_TYPE_ASSOC:
        // TODO: do something here, but not handled previously
        break;
    }
}

void RlistFilter(Rlist **list, bool (*KeepPredicate)(void *, void *), void *predicate_user_data, void (*DestroyItem)(void *))
{
    assert(KeepPredicate);

    Rlist *start = *list;
    Rlist *prev = NULL;

    for (Rlist *rp = start; rp;)
    {
        if (!KeepPredicate(rp->item, predicate_user_data))
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
                DestroyItem(rp->item);
                rp->item = NULL;
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
