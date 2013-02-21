/*******************************************************************/

/* The following sort functions are trivial rewrites of merge-sort
 * implementation by Simon Tatham
 * copyright 2001 Simon Tatham.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL SIMON TATHAM BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "cf3.defs.h"

#include "rlist.h"
#include "item_lib.h"

typedef bool (*LessFn)(void *lhs, void *rhs, void *ctx);
typedef void * (*GetNextElementFn)(void *element);
typedef void (*PutNextElementFn)(void *element, void *next);

static void *Sort(void *list, LessFn less, GetNextElementFn next, PutNextElementFn putnext, void *ctx)
{
    void *p, *q, *e, *tail;
    int insize, nmerges, psize, qsize, i;

    if (list == NULL)
    {
        return NULL;
    }

    insize = 1;

    while (true)
    {
        p = list;
        list = NULL;
        tail = NULL;

        nmerges = 0;            /* count number of merges we do in this pass */

        while (p)
        {
            nmerges++;          /* there exists a merge to be done */
            /* step `insize' places along from p */
            q = p;
            psize = 0;

            for (i = 0; i < insize; i++)
            {
                psize++;

                q = next(q);

                if (!q)
                {
                    break;
                }
            }

            /* if q hasn't fallen off end, we have two lists to merge */
            qsize = insize;

            /* now we have two lists; merge them */
            while ((psize > 0) || ((qsize > 0) && q))
            {
                /* decide whether next element of merge comes from p or q */
                if (psize == 0)
                {
                    /* p is empty; e must come from q. */
                    e = q;
                    q = next(q);
                    qsize--;
                }
                else if ((qsize == 0) || (!q))
                {
                    /* q is empty; e must come from p. */
                    e = p;
                    p = next(p);
                    psize--;
                }
                else if (less(p, q, ctx))
                {
                    /* First element of p is lower (or same);
                     * e must come from p. */
                    e = p;
                    p = next(p);
                    psize--;
                }
                else
                {
                    /* First element of q is lower; e must come from q. */
                    e = q;
                    q = next(q);
                    qsize--;
                }

                /* add the next element to the merged list */
                if (tail)
                {
                    putnext(tail, e);
                }
                else
                {
                    list = e;
                }

                tail = e;
            }

            /* now p has stepped `insize' places along, and q has too */
            p = q;
        }

        putnext(tail, NULL);

        /* If we have done only one merge, we're finished. */

        if (nmerges <= 1)       /* allow for nmerges==0, the empty list case */
        {
            return list;
        }

        /* Otherwise repeat, merging lists twice the size */
        insize *= 2;
    }
}

/* Item* callbacks */

static bool ItemNameLess(void *lhs, void *rhs, void *ctx)
{
    return strcmp(((Item*)lhs)->name, ((Item*)rhs)->name) < 0;
}

static bool ItemClassesLess(void *lhs, void *rhs, void *ctx)
{
    return strcmp(((Item*)lhs)->classes, ((Item*)rhs)->classes) < 0;
}

static bool ItemCounterMore(void *lhs, void *rhs, void *ctx)
{
    return ((Item*)lhs)->counter > ((Item*)rhs)->counter;
}

static bool ItemTimeMore(void *lhs, void *rhs, void *ctx)
{
    return ((Item*)lhs)->time > ((Item*)rhs)->time;
}

static void *ItemGetNext(void *element)
{
    return ((Item*)element)->next;
}

static void ItemPutNext(void *element, void *next)
{
    ((Item*)element)->next = (Item *)next;
}

/* Item* sorting */

Item *SortItemListNames(Item *list)
{
    return Sort(list, &ItemNameLess, &ItemGetNext, &ItemPutNext, NULL);
}

Item *SortItemListClasses(Item *list)
{
    return Sort(list, &ItemClassesLess, &ItemGetNext, &ItemPutNext, NULL);
}

Item *SortItemListCounters(Item *list)
{
    return Sort(list, &ItemCounterMore, &ItemGetNext, &ItemPutNext, NULL);
}

Item *SortItemListTimes(Item *list)
{
    return Sort(list, &ItemTimeMore, &ItemGetNext, &ItemPutNext, NULL);
}

/* Rlist* callbacks */

static bool RlistCustomItemLess(void *lhs_, void *rhs_, void *ctx)
{
    Rlist *lhs = lhs_;
    Rlist *rhs = rhs_;
    int (*cmp)() = ctx;

    return (*cmp)(lhs->item, rhs->item);
}

static bool RlistItemLess(void *lhs, void *rhs, void *ctx)
{
    return strcmp(((Rlist*)lhs)->item, ((Rlist*)rhs)->item) < 0;
}

static void *RlistGetNext(void *element)
{
    return ((Rlist*)element)->next;
}

static void RlistPutNext(void *element, void *next)
{
    ((Rlist*)element)->next = (Rlist *)next;
}

/* Rlist* sorting */

Rlist *SortRlist(Rlist *list, int (*CompareItems) ())
{
    return Sort(list, &RlistCustomItemLess, &RlistGetNext, &RlistPutNext, CompareItems);
}

Rlist *AlphaSortRListNames(Rlist *list)
{
    return Sort(list, &RlistItemLess, &RlistGetNext, &RlistPutNext, NULL);
}
