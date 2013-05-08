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
#include "ip_address.h"

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

static bool ItemNameLess(void *lhs, void *rhs, ARG_UNUSED void *ctx)
{
    return strcmp(((Item*)lhs)->name, ((Item*)rhs)->name) < 0;
}

static bool ItemClassesLess(void *lhs, void *rhs, ARG_UNUSED void *ctx)
{
    return strcmp(((Item*)lhs)->classes, ((Item*)rhs)->classes) < 0;
}

static bool ItemCounterMore(void *lhs, void *rhs, ARG_UNUSED void *ctx)
{
    return ((Item*)lhs)->counter > ((Item*)rhs)->counter;
}

static bool ItemTimeMore(void *lhs, void *rhs, ARG_UNUSED void *ctx)
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

static bool RlistItemLess(void *lhs, void *rhs, ARG_UNUSED void *ctx)
{
    return strcmp(((Rlist*)lhs)->item, ((Rlist*)rhs)->item) < 0;
}

static bool RlistItemIntLess(void *lhs, void *rhs, ARG_UNUSED void *ctx)
{
    char remainder[CF_BUFSIZE];
    long left;
    long right;
    int matched_left = sscanf(((Rlist*)lhs)->item, "%ld%s", &left, remainder);
    int matched_right = sscanf(((Rlist*)rhs)->item, "%ld%s", &right, remainder);

    if (matched_left && matched_right)
    {
        return left - right < 0;
    }

    if (matched_left)
    {
        return false;
    }

    if (matched_right)
    {
        return true;
    }

    // neither item matched
    return RlistItemLess(lhs, rhs, ctx);
}

static bool RlistItemRealLess(void *lhs, void *rhs, ARG_UNUSED void *ctx)
{
    char remainder[CF_BUFSIZE];
    double left;
    double right;
    int matched_left = sscanf(((Rlist*)lhs)->item, "%lf%s", &left, remainder);
    int matched_right = sscanf(((Rlist*)rhs)->item, "%lf%s", &right, remainder);

    if (matched_left && matched_right)
    {
        return left - right < 0;
    }

    // drop back to integer comparison if either number could not be parsed as a double
    return RlistItemIntLess(lhs, rhs, ctx);
}

static bool RlistItemIPLess(void *lhs, void *rhs, ARG_UNUSED void *ctx)
{
    char *left_item = ((Rlist*)lhs)->item;
    char *right_item = ((Rlist*)rhs)->item;

    Buffer *left_buffer = BufferNewFrom(left_item, strlen(left_item));
    Buffer *right_buffer = BufferNewFrom(right_item, strlen(right_item));

    IPAddress *left = IPAddressNew(left_buffer);
    IPAddress *right = IPAddressNew(right_buffer);

    bool matched_left = left != NULL;
    bool matched_right = right != NULL;

    BufferDestroy(&left_buffer);
    BufferDestroy(&right_buffer);

    if (matched_left && matched_right)
    {
        int difference = IPAddressCompare(left, right);
        IPAddressDestroy(&left);
        IPAddressDestroy(&right);
        if (difference != 0) return difference < 0;
    }

    IPAddressDestroy(&left);
    IPAddressDestroy(&right);

    if (matched_left)
    {
        return false;
    }

    if (matched_right)
    {
        return true;
    }

    // neither item matched
    return RlistItemLess(lhs, rhs, ctx);
}

static long ParseEtherAddress(const char* input, unsigned char *addr)
{
    if (strlen(input) > 12)
    {
        return sscanf(input, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
                      &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);
    }

    return sscanf(input, "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx", 
                  &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);
}

static bool RlistItemMACLess(void *lhs, void *rhs, ARG_UNUSED void *ctx)
{
    int bytes = 6;
    unsigned char left[bytes], right[bytes];
    int matched_left = 6 == ParseEtherAddress(((Rlist*)lhs)->item, left);
    int matched_right = 6 == ParseEtherAddress(((Rlist*)rhs)->item, right);

    if (matched_left && matched_right)
    {
        int difference = memcmp(left, right, bytes);
        if (difference != 0) return difference < 0;
    }

    if (matched_left)
    {
        return false;
    }

    if (matched_right)
    {
        return true;
    }

    // neither item matched
    return RlistItemLess(lhs, rhs, ctx);
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

Rlist *IntSortRListNames(Rlist *list)
{
    return Sort(list, &RlistItemIntLess, &RlistGetNext, &RlistPutNext, NULL);
}

Rlist *RealSortRListNames(Rlist *list)
{
    return Sort(list, &RlistItemRealLess, &RlistGetNext, &RlistPutNext, NULL);
}

Rlist *IPSortRListNames(Rlist *list)
{
    return Sort(list, &RlistItemIPLess, &RlistGetNext, &RlistPutNext, NULL);
}

Rlist *MACSortRListNames(Rlist *list)
{
    return Sort(list, &RlistItemMACLess, &RlistGetNext, &RlistPutNext, NULL);
}
