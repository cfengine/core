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

#include "files_names.h"
#include "conversion.h"
#include "expand.h"
#include "matching.h"
#include "unix.h"
#include "cfstream.h"
#include "fncall.h"
#include "string_lib.h"

#include <assert.h>

/*******************************************************************/

char *ScalarValue(const Rlist *rlist)
{
    if (rlist->type != CF_SCALAR)
    {
        FatalError("Internal error: Rlist value contains type %c instead of expected scalar", rlist->type);
    }

    return (char *) rlist->item;
}

/*******************************************************************/

FnCall *FnCallValue(const Rlist *rlist)
{
    if (rlist->type != CF_FNCALL)
    {
        FatalError("Internal error: Rlist value contains type %c instead of expected FnCall", rlist->type);
    }

    return (FnCall *) rlist->item;
}

/*******************************************************************/

Rlist *ListValue(const Rlist *rlist)
{
    if (rlist->type != CF_LIST)
    {
        FatalError("Internal error: Rlist value contains type %c instead of expected List", rlist->type);
    }

    return (Rlist *) rlist->item;
}

/*******************************************************************/

char *ScalarRvalValue(Rval rval)
{
    if (rval.rtype != CF_SCALAR)
    {
        FatalError("Internal error: Rval contains type %c instead of expected scalar", rval.rtype);
    }

    return rval.item;
}

/*******************************************************************/

FnCall *FnCallRvalValue(Rval rval)
{
    if (rval.rtype != CF_FNCALL)
    {
        FatalError("Internal error: Rval contains type %c instead of expected FnCall", rval.rtype);
    }

    return rval.item;
}

/*******************************************************************/

Rlist *ListRvalValue(Rval rval)
{
    if (rval.rtype != CF_LIST)
    {
        FatalError("Internal error: Rval contain type %c instead of expected List", rval.rtype);
    }

    return rval.item;
}

/*******************************************************************/

Rlist *KeyInRlist(Rlist *list, char *key)
{
    for (Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (rp->type != CF_SCALAR)
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

bool IsStringIn(const Rlist *list, const char *s)
{
    if (s == NULL || list == NULL)
    {
        return false;
    }

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (rp->type != CF_SCALAR)
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

bool IsIntIn(const Rlist *list, int i)
{
    char s[CF_SMALLBUF];

    snprintf(s, CF_SMALLBUF - 1, "%d", i);

    if (list == NULL)
    {
        return false;
    }

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (rp->type != CF_SCALAR)
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

bool IsInListOfRegex(const Rlist *list, const char *str)
{
    if (str == NULL || list == NULL)
    {
        return false;
    }

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        if (rp->type != CF_SCALAR)
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

Rval CopyRvalItem(Rval rval)
{
    Rlist *rp, *srp, *start = NULL;
    FnCall *fp;

    CfDebug("CopyRvalItem(%c)\n", rval.rtype);

    if (rval.item == NULL)
    {
        switch (rval.rtype)
        {
        case CF_SCALAR:
            return (Rval) {xstrdup(""), CF_SCALAR};

        case CF_LIST:
            return (Rval) {NULL, CF_LIST};
        }
    }

    switch (rval.rtype)
    {
    case CF_SCALAR:
        /* the rval is just a string */
        return (Rval) {xstrdup((char *) rval.item), CF_SCALAR};

    case CF_ASSOC:
        return (Rval) {CopyAssoc((CfAssoc *) rval.item), CF_ASSOC};

    case CF_FNCALL:
        /* the rval is a fncall */
        fp = (FnCall *) rval.item;
        return (Rval) {CopyFnCall(fp), CF_FNCALL};

    case CF_LIST:
        /* The rval is an embedded rlist (2d) */
        for (rp = (Rlist *) rval.item; rp != NULL; rp = rp->next)
        {
            char naked[CF_BUFSIZE] = "";

            if (IsNakedVar(rp->item, '@'))
            {
                GetNaked(naked, rp->item);

                Rval rv = { NULL, CF_SCALAR };  /* FIXME: why it needs to be initialized? */
                if (GetVariable(CONTEXTID, naked, &rv) != cf_notype)
                {
                    switch (rv.rtype)
                    {
                    case CF_LIST:
                        for (srp = (Rlist *) rv.item; srp != NULL; srp = srp->next)
                        {
                            AppendRlist(&start, srp->item, srp->type);
                        }
                        break;

                    default:
                        AppendRlist(&start, rp->item, rp->type);
                        break;
                    }
                }
                else
                {
                    AppendRlist(&start, rp->item, rp->type);
                }
            }
            else
            {
                AppendRlist(&start, rp->item, rp->type);
            }
        }

        return (Rval) {start, CF_LIST};
    }

    CfOut(cf_verbose, "", "Unknown type %c in CopyRvalItem - should not happen", rval.rtype);
    return (Rval) {NULL, rval.rtype};
}

/*******************************************************************/

Rlist *CopyRlist(const Rlist *list)
{
    Rlist *start = NULL;

    CfDebug("CopyRlist()\n");

    if (list == NULL)
    {
        return NULL;
    }

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        AppendRlist(&start, rp->item, rp->type);        // allocates memory for objects
    }

    return start;
}

/*******************************************************************/

void DeleteRlist(Rlist *list)
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
                DeleteRvalItem((Rval) {rl->item, rl->type});
            }

            free(rl);
        }
    }
}

/*******************************************************************/

Rlist *IdempAppendRScalar(Rlist **start, void *item, char type)
{
    char *scalar = item;

    if (type != CF_SCALAR)
    {
        FatalError("Cannot append non-scalars to lists");
    }

    if (!KeyInRlist(*start, (char *) item))
    {
        return AppendRlist(start, scalar, type);
    }
    else
    {
        return NULL;
    }
}

/*******************************************************************/

Rlist *IdempPrependRScalar(Rlist **start, void *item, char type)
{
    char *scalar = item;

    if (type != CF_SCALAR)
    {
        FatalError("Cannot append non-scalars to lists");
    }

    if (!KeyInRlist(*start, (char *) item))
    {
        return PrependRlist(start, scalar, type);
    }
    else
    {
        return NULL;
    }
}

/*******************************************************************/

Rlist *IdempAppendRlist(Rlist **start, void *item, char type)
{
    Rlist *rp, *ins = NULL;

    if (type == CF_LIST)
    {
        for (rp = (Rlist *) item; rp != NULL; rp = rp->next)
        {
            ins = IdempAppendRlist(start, rp->item, rp->type);
        }
        return ins;
    }

    if (!KeyInRlist(*start, (char *) item))
    {
        return AppendRlist(start, (char *) item, type);
    }
    else
    {
        return NULL;
    }
}

/*******************************************************************/

Rlist *AppendRScalar(Rlist **start, void *item, char type)
{
    char *scalar = item;

    if (type != CF_SCALAR)
    {
        FatalError("Cannot append non-scalars to lists");
    }

    return AppendRlist(start, scalar, type);
}

/*******************************************************************/

Rlist *PrependRScalar(Rlist **start, void *item, char type)
{
    char *scalar = item;

    if (type != CF_SCALAR)
    {
        FatalError("Cannot append non-scalars to lists");
    }

    return PrependRlist(start, scalar, type);
}

/*******************************************************************/

Rlist *AppendRlist(Rlist **start, const void *item, char type)
   /* Allocates new memory for objects - careful, could leak!  */
{
    Rlist *rp, *lp = *start;
    FnCall *fp;

    switch (type)
    {
    case CF_SCALAR:
        CfDebug("Appending scalar to rval-list [%s]\n", (char *) item);
        break;

    case CF_ASSOC:
        CfDebug("Appending assoc to rval-list [%s]\n", (char *) item);
        break;

    case CF_FNCALL:
        CfDebug("Appending function to rval-list function call: ");
        fp = (FnCall *) item;
        if (DEBUG)
        {
            ShowFnCall(stdout, fp);
        }
        CfDebug("\n");
        break;

    case CF_LIST:
        CfDebug("Expanding and appending list object\n");

        for (rp = (Rlist *) item; rp != NULL; rp = rp->next)
        {
            lp = AppendRlist(start, rp->item, rp->type);
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

    rp->item = CopyRvalItem((Rval) {(void *) item, type}).item;
    rp->type = type;            /* scalar, builtin function */

    ThreadLock(cft_lock);

    if (type == CF_LIST)
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

Rlist *PrependRlist(Rlist **start, void *item, char type)
   /* heap memory for item must have already been allocated */
{
    Rlist *rp, *lp = *start;
    FnCall *fp;

    switch (type)
    {
    case CF_SCALAR:
        CfDebug("Prepending scalar to rval-list [%s]\n", (char *) item);
        break;

    case CF_LIST:

        CfDebug("Expanding and prepending list (ends up in reverse)\n");

        for (rp = (Rlist *) item; rp != NULL; rp = rp->next)
        {
            lp = PrependRlist(start, rp->item, rp->type);
        }
        return lp;

    case CF_FNCALL:
        CfDebug("Prepending function to rval-list function call: ");
        fp = (FnCall *) item;
        if (DEBUG)
        {
            ShowFnCall(stdout, fp);
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
    rp->item = CopyRvalItem((Rval) {item, type}).item;
    rp->type = type;            /* scalar, builtin function */

    if (type == CF_LIST)
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

Rlist *OrthogAppendRlist(Rlist **start, void *item, char type)
   /* Allocates new memory for objects - careful, could leak!  */
{
    Rlist *rp, *lp;
    CfAssoc *cp;

    CfDebug("OrthogAppendRlist\n");

    switch (type)
    {
    case CF_LIST:
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

    lp = PrependRlist((Rlist **) &(cp->rval), CF_NULL_VALUE, CF_SCALAR);
    rp->state_ptr = lp->next;   // Always skip the null value
    AppendRlist((Rlist **) &(cp->rval), CF_NULL_VALUE, CF_SCALAR);

    rp->item = item;
    rp->type = CF_LIST;
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

Rlist *ParseShownRlist(char *string)
{
    Rlist *newlist = NULL, *splitlist, *rp;
    char value[CF_MAXVARSIZE];

/* Parse a string representation generated by ShowList and turn back into Rlist */

    splitlist = SplitStringAsRList(string, ',');

    for (rp = splitlist; rp != NULL; rp = rp->next)
    {
        sscanf(rp->item, "%*[{ '\"]%255[^'\"]", value);
        AppendRlist(&newlist, value, CF_SCALAR);
    }

    DeleteRlist(splitlist);
    return newlist;
}

/*******************************************************************/

void ShowRlist(FILE *fp, const Rlist *list)
{
    fprintf(fp, " {");

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        fprintf(fp, "\'");
        ShowRval(fp, (Rval) {rp->item, rp->type});
        fprintf(fp, "\'");

        if (rp->next != NULL)
        {
            fprintf(fp, ",");
        }
    }
    fprintf(fp, "}");
}

/*******************************************************************/

int PrintRlist(char *buffer, int bufsize, Rlist *list)
{
    Rlist *rp;

    StartJoin(buffer, "{", bufsize);

    for (rp = list; rp != NULL; rp = rp->next)
    {
        if (!JoinSilent(buffer, "'", bufsize))
        {
            EndJoin(buffer, "'}", bufsize);
            return false;
        }

        if (!PrintRval(buffer, bufsize, (Rval) {rp->item, rp->type}))
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

int PrintRval(char *buffer, int bufsize, Rval rval)
{
    if (rval.item == NULL)
    {
        return 0;
    }

    switch (rval.rtype)
    {
    case CF_SCALAR:
        return JoinSilent(buffer, (const char *) rval.item, bufsize);
    case CF_LIST:
        return PrintRlist(buffer, bufsize, (Rlist *) rval.item);
    case CF_FNCALL:
        return PrintFnCall(buffer, bufsize, (FnCall *) rval.item);
    default:
        return 0;
    }
}

/*******************************************************************/

static JsonElement *RlistToJson(Rlist *list)
{
    JsonElement *array = JsonArrayCreate(RlistLen(list));

    for (Rlist *rp = list; rp; rp = rp->next)
    {
        switch (rp->type)
        {
        case CF_SCALAR:
            JsonArrayAppendString(array, (const char *) rp->item);
            break;

        case CF_LIST:
            JsonArrayAppendArray(array, RlistToJson((Rlist *) rp->item));
            break;

        case CF_FNCALL:
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

    switch (rval.rtype)
    {
    case CF_SCALAR:
        return JsonStringCreate((const char *) rval.item);
    case CF_LIST:
        return RlistToJson((Rlist *) rval.item);
    case CF_FNCALL:
        return FnCallToJson((FnCall *) rval.item);
    default:
        assert(false && "Invalid rval type");
        return JsonStringCreate("");
    }
}

/*******************************************************************/

void ShowRval(FILE *fp, Rval rval)
{
    char buf[CF_BUFSIZE];

    if (rval.item == NULL)
    {
        return;
    }

    switch (rval.rtype)
    {
    case CF_SCALAR:
        EscapeQuotes((const char *) rval.item, buf, sizeof(buf));
        fprintf(fp, "%s", buf);
        break;

    case CF_LIST:
        ShowRlist(fp, (Rlist *) rval.item);
        break;

    case CF_FNCALL:
        ShowFnCall(fp, (FnCall *) rval.item);
        break;

    case CF_NOPROMISEE:
        fprintf(fp, "(no-one)");
        break;
    }
}

/*******************************************************************/

void DeleteRvalItem(Rval rval)
{
    Rlist *clist, *next = NULL;

    CfDebug("DeleteRvalItem(%c)", rval.rtype);

    if (DEBUG)
    {
        ShowRval(stdout, rval);
    }

    CfDebug("\n");

    if (rval.item == NULL)
    {
        CfDebug("DeleteRval NULL\n");
        return;
    }

    switch (rval.rtype)
    {
    case CF_SCALAR:

        ThreadLock(cft_lock);
        free((char *) rval.item);
        ThreadUnlock(cft_lock);
        break;

    case CF_ASSOC:             /* What? */

        DeleteAssoc((CfAssoc *) rval.item);
        break;

    case CF_LIST:

        /* rval is now a list whose first item is clist->item */

        for (clist = (Rlist *) rval.item; clist != NULL; clist = next)
        {

            next = clist->next;

            if (clist->item)
            {
                DeleteRvalItem((Rval) {clist->item, clist->type});
            }

            free(clist);
        }

        break;

    case CF_FNCALL:

        DeleteFnCall((FnCall *) rval.item);
        break;

    default:
        CfDebug("Nothing to do\n");
        return;
    }
}

/*********************************************************************/

void DeleteRlistEntry(Rlist **liststart, Rlist *entry)
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

Rlist *AppendRlistAlien(Rlist **start, void *item)
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
    rp->type = CF_SCALAR;

    ThreadLock(cft_lock);

    rp->next = NULL;

    ThreadUnlock(cft_lock);
    return rp;
}

/*******************************************************************/

Rlist *PrependRlistAlien(Rlist **start, void *item)
   /* Allocates new memory for objects - careful, could leak!  */
{
    Rlist *rp;

    ThreadLock(cft_lock);

    rp = xmalloc(sizeof(Rlist));

    rp->next = *start;
    *start = rp;
    ThreadUnlock(cft_lock);

    rp->item = item;
    rp->type = CF_SCALAR;
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

void PushStack(Rlist **liststart, void *item)
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

void PopStack(Rlist **liststart, void **item, size_t size)
{
    Rlist *rp = *liststart;

    if (*liststart == NULL)
    {
        FatalError("Attempt to pop from empty stack");
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

Rlist *SplitStringAsRList(const char *string, char sep)
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

        AppendRScalar(&liststart, node, CF_SCALAR);
    }

    return liststart;
}

/*******************************************************************/

Rlist *SplitRegexAsRList(const char *string, const char *regex, int max, int blanks)
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
            AppendRScalar(&liststart, node, CF_SCALAR);
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
            AppendRScalar(&liststart, node, CF_SCALAR);
        }
    }

    return liststart;
}

/*******************************************************************/

Rlist *RlistAppendReference(Rlist **start, void *item, char type)
{
    Rlist *rp = NULL, *lp = *start;

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
    rp->type = type;

    ThreadLock(cft_lock);

    rp->next = NULL;

    ThreadUnlock(cft_lock);
    return rp;
}

/*******************************************************************/

Rlist *RlistAt(Rlist *start, size_t index)
{
    for (Rlist *rp = start; rp != NULL; rp = rp->next)
    {
        if (index-- == 0)
        {
            return rp;
        }
    }

    return NULL;
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

void RlistPrint(Writer *writer, const Rlist *list)
{
    WriterWrite(writer, " {");

    for (const Rlist *rp = list; rp != NULL; rp = rp->next)
    {
        WriterWriteChar(writer, '\'');
        RvalPrint(writer, (Rval) {rp->item, rp->type});
        WriterWriteChar(writer, '\'');

        if (rp->next != NULL)
        {
            WriterWriteChar(writer, ',');
        }
    }

    WriterWriteChar(writer, '}');
}

void RvalPrint(Writer *writer, Rval rval)
{
    if (rval.item == NULL)
    {
        return;
    }

    switch (rval.rtype)
    {
    case CF_SCALAR:
    {
        size_t buffer_size = strlen((const char *) rval.item) * 2;
        char *buffer = xcalloc(buffer_size, sizeof(char));

        EscapeQuotes((const char *) rval.item, buffer, buffer_size);
        WriterWrite(writer, buffer);
        free(buffer);
    }
        break;

    case CF_LIST:
        RlistPrint(writer, (Rlist *) rval.item);
        break;

    case CF_FNCALL:
        FnCallPrint(writer, (FnCall *) rval.item);
        break;

    case CF_NOPROMISEE:
        WriterWrite(writer, "(no-one)");
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
            DeleteRlist(rp);
            rp = next;
        }
        else
        {
            prev = rp;
            rp = rp->next;
        }
    }
}
