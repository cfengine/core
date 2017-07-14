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

#include <item_lib.h>

#include <files_names.h>
#include <addr_lib.h>
#include <matching.h>
#include <misc_lib.h>
#include <regex.h> /* StringMatchFull,CompileRegex,StringMatchWithPrecompiledRegex */
#include <file_lib.h>
#include <files_interfaces.h>

#ifdef CYCLE_DETECTION
/* While looping over entry lp in an Item list, advance slow half as
 * fast as lp; let n be the number of steps it has fallen behind; this
 * increases by one every second time round the loop.  If there's a
 * cycle of length M, lp shall run round and round it; once slow gets
 * into the loop, they shall be n % M steps apart; at most 2*M more
 * times round the loop and n % M shall be 0 so lp == slow.  If the
 * lead-in to the loop is of length L, this takes at most 2*(L+M)
 * turns round the loop to discover the cycle.  The added cost is O(1)
 * per time round the loop, so the typical O(list length) user doesn't
 * change order when this is enabled, albeit the constant of
 * proportionality is up.
 *
 * Note, however, that none of this works if you're messing with the
 * structure (e.g. reversing or deleting) of the list as you go.
 *
 * To use the macros: before the loop, declare and initialize your
 * loop variable; pass it as lp to CYCLE_DECLARE(), followed by two
 * names not in use in your code.  Then, in the body of the loop,
 * after advancing the loop variable, CYCLE_CHECK() the same three
 * parameters.  This is apt to require a while loop where you might
 * otherwise have used a for loop; you also need to make sure your
 * loop doesn't continue past the checking.  When you compile with
 * CYCLE_DETECTION defined, your function shall catch cycles, raising
 * a ProgrammingError() if it sees one.
 */
#define CYCLE_DECLARE(lp, slow, toggle) \
    const Item *slow = lp; bool toggle = false
#define CYCLE_VERIFY(lp, slow) if (!lp) { /* skip */ }              \
    else if (!slow) ProgrammingError("Loop-detector bug :-(");      \
    else if (lp == slow) ProgrammingError("Found loop in Item list")
#define CYCLE_CHECK(lp, slow, toggle) \
    CYCLE_VERIFY(lp, slow);                                     \
    if (toggle) { slow = slow->next; CYCLE_VERIFY(lp, slow); }  \
    toggle = !toggle
#else
#define CYCLE_DECLARE(lp, slow, toggle) /* skip */
#define CYCLE_CHECK(lp, slow, toggle) /* skip */
#endif

#ifndef NDEBUG
/* Only intended for use in assertions.  Note that its cost is O(list
 * length), so you don't want to call it inside a loop over the
 * list. */
static bool ItemIsInList(const Item *list, const Item *item)
{
    CYCLE_DECLARE(list, slow, toggle);
    while (list)
    {
        if (list == item)
        {
            return true;
        }
        list = list->next;
        CYCLE_CHECK(list, slow, toggle);
    }
    return false;
}
#endif /* NDEBUG */

/*******************************************************************/

Item *ReverseItemList(Item *list)
{
    /* TODO: cycle-detection, which is somewhat harder here, without
     * turning this into a quadratic-cost function, albeit only when
     * assert() is enabled.
     */
    Item *tail = NULL;
    while (list)
    {
        Item *here = list;
        list = here->next;
        /* assert(!ItemIsInList(here, list)); // quadratic cost */
        here->next = tail;
        tail = here;
    }
    return tail;
}

/*******************************************************************/

void PrintItemList(const Item *list, Writer *w)
{
    WriterWriteChar(w, '{');
    const Item *ip = list;
    CYCLE_DECLARE(ip, slow, toggle);

    while (ip != NULL)
    {
        if (ip != list)
        {
            WriterWriteChar(w, ',');
        }

        WriterWriteChar(w, '\'');
        WriterWrite(w, ip->name);
        WriterWriteChar(w, '\'');

        ip = ip->next;
        CYCLE_CHECK(ip, slow, toggle);
    }

    WriterWriteChar(w, '}');
}

/*********************************************************************/

int ItemListSize(const Item *list)
{
    int size = 0;
    const Item *ip = list;
    CYCLE_DECLARE(ip, slow, toggle);

    while (ip != NULL)
    {
        if (ip->name)
        {
            size += strlen(ip->name);
        }
        ip = ip->next;
        CYCLE_CHECK(ip, slow, toggle);
    }

    return size;
}

/*********************************************************************/

Item *ReturnItemIn(Item *list, const char *item)
{
    if (item == NULL || item[0] == '\0')
    {
        return NULL;
    }

    Item *ptr = list;
    CYCLE_DECLARE(ptr, slow, toggle);
    while (ptr != NULL)
    {
        if (strcmp(ptr->name, item) == 0)
        {
            return ptr;
        }
        ptr = ptr->next;
        CYCLE_CHECK(ptr, slow, toggle);
    }

    return NULL;
}

/*********************************************************************/

Item *ReturnItemInClass(Item *list, const char *item, const char *classes)
{
    if (item == NULL || item[0] == '\0')
    {
        return NULL;
    }

    Item *ptr = list;
    CYCLE_DECLARE(ptr, slow, toggle);
    while (ptr != NULL)
    {
        if (strcmp(ptr->name, item) == 0 &&
            strcmp(ptr->classes, classes) == 0)
        {
            return ptr;
        }
        ptr = ptr->next;
        CYCLE_CHECK(ptr, slow, toggle);
    }

    return NULL;
}

/*********************************************************************/

Item *ReturnItemAtIndex(Item *list, int index)
{
    Item *ptr = list;
    for (int i = 0; ptr != NULL && i < index; i++)
    {
        ptr = ptr->next;
    }

    return ptr;
}

/*********************************************************************/

bool IsItemIn(const Item *list, const char *item)
{
    if (item == NULL || item[0] == '\0')
    {
        return true;
    }

    const Item *ptr = list;
    CYCLE_DECLARE(ptr, slow, toggle);
    while (ptr != NULL)
    {
        if (strcmp(ptr->name, item) == 0)
        {
            return true;
        }
        ptr = ptr->next;
        CYCLE_CHECK(ptr, slow, toggle);
    }

    return false;
}

/*********************************************************************/
/* True precisely if the lists are of equal length and every entry of
 * the first appears in the second.  As long as each list is known to
 * have no duplication of its entries, this is equivalent to testing
 * they have the same set of entries (ignoring order).
 *
 * This is not, in general, the same as the lists being equal !  They
 * may have the same entries in different orders.  If the first list
 * has some duplicate entries, the second list can have some entries
 * not in the first, yet compare equal.  Two lists with the same set
 * of entries but with different multiplicities are equal or different
 * precisely if of equal length.
 */

bool ListsCompare(const Item *list1, const Item *list2)
{
    if (ListLen(list1) != ListLen(list2))
    {
        return false;
    }

    const Item *ptr = list1;
    CYCLE_DECLARE(ptr, slow, toggle);
    while (ptr != NULL)
    {
        if (IsItemIn(list2, ptr->name) == false)
        {
            return false;
        }
        ptr = ptr->next;
        CYCLE_CHECK(ptr, slow, toggle);
    }

    return true;
}

/**
 * Checks whether list1 is a subset of list2, i.e. every entry in list1 must
 * be found in list2.
 */
bool ListSubsetOfList(const Item *list1, const Item *list2)
{
    const Item *list1_ptr = list1;
    CYCLE_DECLARE(list1_ptr, slow, toggle);

    while (list1_ptr != NULL)
    {
        if (!IsItemIn(list2, list1_ptr->name))
        {
            return false;
        }

        list1_ptr = list1_ptr->next;
        CYCLE_CHECK(list1_ptr, slow, toggle);
    }

    return true;               /* all elements of list1 were found in list2 */
}

/*********************************************************************/

Item *EndOfList(Item *ip)
{
    Item *prev = CF_UNDEFINED_ITEM;

    CYCLE_DECLARE(ip, slow, toggle);
    while (ip != NULL)
    {
        prev = ip;
        ip = ip->next;
        CYCLE_CHECK(ip, slow, toggle);
    }

    return prev;
}

/*********************************************************************/

Item *IdempPrependItem(Item **liststart, const char *itemstring, const char *classes)
{
    Item *ip = ReturnItemIn(*liststart, itemstring);
    if (ip)
    {
        return ip;
    }

    PrependItem(liststart, itemstring, classes);
    return *liststart;
}

/*********************************************************************/

Item *IdempPrependItemClass(Item **liststart, const char *itemstring, const char *classes)
{
    Item *ip = ReturnItemInClass(*liststart, itemstring, classes);
    if (ip)                     // already exists
    {
        return ip;
    }

    PrependItem(liststart, itemstring, classes);
    return *liststart;
}

/*********************************************************************/

void IdempItemCount(Item **liststart, const char *itemstring, const char *classes)
{
    Item *ip = ReturnItemIn(*liststart, itemstring);

    if (ip)
    {
        ip->counter++;
    }
    else
    {
        PrependItem(liststart, itemstring, classes);
    }

// counter+1 is the histogram of occurrences
}

/*********************************************************************/

Item *PrependItem(Item **liststart, const char *itemstring, const char *classes)
{
    Item *ip = xcalloc(1, sizeof(Item));

    ip->name = xstrdup(itemstring);
    if (classes != NULL)
    {
        ip->classes = xstrdup(classes);
    }

    ip->next = *liststart;
    *liststart = ip;

    return ip;
}

/*********************************************************************/

void PrependFullItem(Item **liststart, const char *itemstring, const char *classes, int counter, time_t t)
{
    Item *ip = xcalloc(1, sizeof(Item));

    ip->name = xstrdup(itemstring);
    ip->next = *liststart;
    ip->counter = counter;
    ip->time = t;
    if (classes != NULL)
    {
        ip->classes = xstrdup(classes);
    }

    *liststart = ip;
}

/*********************************************************************/
/* Warning: doing this a lot incurs quadratic costs, as we have to run
 * to the end of the list each time.  If you're building long lists,
 * it is usually better to build the list with PrependItemList() and
 * then use ReverseItemList() to get the entries in the order you
 * wanted; for modest-sized n, 2*n < n*n, even after you've applied
 * different fixed scalings to the two sides.
 */

void AppendItem(Item **liststart, const char *itemstring, const char *classes)
{
    Item *ip = xcalloc(1, sizeof(Item));

    ip->name = xstrdup(itemstring);
    if (classes)
    {
        ip->classes = xstrdup(classes); /* unused now */
    }

    if (*liststart == NULL)
    {
        *liststart = ip;
    }
    else
    {
        Item *lp = EndOfList(*liststart);
        assert(lp != CF_UNDEFINED_ITEM);
        lp->next = ip;
    }
}

/*********************************************************************/

void PrependItemList(Item **liststart, const char *itemstring)
{
    Item *ip = xcalloc(1, sizeof(Item));
    ip->name = xstrdup(itemstring);

    ip->next = *liststart;
    *liststart = ip;
}

/*********************************************************************/

int ListLen(const Item *list)
{
    int count = 0;
    const Item *ip = list;
    CYCLE_DECLARE(ip, slow, toggle);

    while (ip != NULL)
    {
        count++;
        ip = ip->next;
        CYCLE_CHECK(ip, slow, toggle);
    }

    return count;
}

/***************************************************************************/

void CopyList(Item **dest, const Item *source)
/* Copy a list. */
{
    if (*dest != NULL)
    {
        ProgrammingError("CopyList - list not initialized");
    }

    if (source == NULL)
    {
        return;
    }

    const Item *ip = source;
    CYCLE_DECLARE(ip, slow, toggle);
    Item *backwards = NULL;
    while (ip != NULL)
    {
        PrependFullItem(&backwards, ip->name,
                        ip->classes, ip->counter, ip->time);
        ip = ip->next;
        CYCLE_CHECK(ip, slow, toggle);
    }
    *dest = ReverseItemList(backwards);
}

/*********************************************************************/

Item *ConcatLists(Item *list1, Item *list2)
/* Notes: * Refrain from freeing list2 after using ConcatLists
          * list1 must have at least one element in it */
{
    if (list1 == NULL)
    {
        ProgrammingError("ConcatLists: first argument must have at least one element");
    }
    Item *tail = EndOfList(list1);
    assert(tail != CF_UNDEFINED_ITEM);
    assert(tail->next == NULL);
    /* If any entry in list1 is in list2, so is tail; so this is a
     * sufficient check that we're not creating a loop: */
    assert(!ItemIsInList(list2, tail));
    tail->next = list2;
    return list1;
}

void InsertAfter(Item **filestart, Item *ptr, const char *string)
{
    if (*filestart == NULL || ptr == CF_UNDEFINED_ITEM)
    {
        AppendItem(filestart, string, NULL);
        return;
    }

    if (ptr == NULL)
    {
        AppendItem(filestart, string, NULL);
        return;
    }

    Item *ip = xcalloc(1, sizeof(Item));

    ip->next = ptr->next;
    ptr->next = ip;
    ip->name = xstrdup(string);
    ip->classes = NULL;
}

/**
 *  Splits a string containing a separator like ':' into a linked list of
 *  separate items,
 *
 *  @NOTE backslashes can be used to escape either the separator, or the
 *        backslash itself, e.g. "\:" or "\\" (ofcourse proper C strings need
 *        double backslash).
 *
 *  @NOTE separator can't be the backslash.
 */
Item *SplitString(const char *string, char sep)
{
    Item *liststart = NULL;

    size_t string_len = strlen(string);

    /* Temporary buffer for each chunk we append. This has the maximum
     * possible size we might possibly need. */
    char *buf = xmalloc(string_len + 1);
    size_t buf_len = 0;

    /* We scan the string for separator or backslash. */
    char sep2[3] = { sep, '\\', '\0' };
    size_t z = 0;

    while ((z = strcspn(string, sep2)) < string_len)
    {
        memcpy(&buf[buf_len], string, z);
        buf_len += z;

        if (string[z] == '\\')
        {
            /* Check next character after backslash, if it's backslash or
             * separator then skip backslash, append character to buffer. */
            if (string[z+1] == '\\' || string[z+1] == sep)
            {
                z++;
            }

            /* Append the single character to our buffer. */
            buf[buf_len] = string[z];
            buf_len++;

            /* SKIP, find next backslash or separator. */
            string     += z + 1;
            string_len -= z + 1;
            continue;
        }
        else                   /* separator was found, and it's not escaped */
        {
            assert(string[z] == sep);
            assert((z == 0) ||
                   (z > 0 && string[z-1] != '\\'));
        }

        /* Terminate buffer and add to Item list. */
        buf[buf_len] = '\0';
        PrependItem(&liststart, buf, NULL);

        /* Empty the buffer. */
        buf_len = 0;
        /* Start new search from after separator. */
        string     += z + 1;
        string_len -= z + 1;
    }

    memcpy(&buf[buf_len], string, z);
    buf_len += z;
    buf[buf_len] = '\0';
    PrependItem(&liststart, buf, NULL);

    free(buf);
    return ReverseItemList(liststart);
}

/*********************************************************************/

Item *SplitStringAsItemList(const char *string, char sep)
 /* Splits a string containing a separator like :
    into a linked list of separate items, */
{
    Item *liststart = NULL;
    char node[256];
    char format[] = "%255[^\0]";

    /* Overwrite format's internal \0 with sep: */
    format[strlen(format)] = sep;
    assert(strlen(format) + 1 == sizeof(format) || sep == '\0');

    for (const char *sp = string; *sp != '\0'; sp++)
    {
        if (sscanf(sp, format, node) == 1 &&
            node[0] != '\0')
        {
            sp += strlen(node) - 1;
            PrependItem(&liststart, node, NULL);
        }
    }

    return ReverseItemList(liststart);
}

/*********************************************************************/

/* NB: does not escape entries in list ! */
char *ItemList2CSV(const Item *list)
{
    /* After each entry, we need space for either a ',' (before the
     * next entry) or a final '\0'. */
    int len = ItemListSize(list) + ListLen(list);
    char *s = xmalloc(len);
    *s = '\0';

    /* No point cycle-checking; done while computing len. */
    for (const Item *ip = list; ip != NULL; ip = ip->next)
    {
        if (ip->name)
        {
            strcat(s, ip->name);
        }

        if (ip->next)
        {
            strcat(s, ",");
        }
    }
    assert(strlen(s) + 1 == len);

    return s;
}

/**
 * Write all strings in list to buffer #buf, separating them with
 * #separator. Watch out, no escaping happens.
 *
 * @return Final strlen(#buf), or #buf_size if string was truncated.
 *         Always '\0'-terminates #buf (within #buf_size).
 */
size_t ItemList2CSV_bound(const Item *list, char *buf, size_t buf_size,
                          char separator)
{
    size_t space = buf_size - 1; /* Reserve one for a '\0' */
    char *tail = buf; /* Point to end of what we've written. */
    const Item *ip = list;
    CYCLE_DECLARE(ip, slow, toggle);

    while (ip != NULL)
    {
        size_t len = strlen(ip->name);
        assert(buf + buf_size == tail + space + 1);

        if (space < len)                  /* We must truncate */
        {
            memcpy(tail, ip->name, space);
            tail[space] = '\0';   /* a.k.a. buf[buf_size - 1] */
            return buf_size;     /* This signifies truncation */
        }
        else
        {
            memcpy(tail, ip->name, len);
            tail += len;
            space -= len;
        }

        /* Output separator if list has more entries. */
        if (ip->next != NULL)
        {
            if (space > 0)
            {
                *tail = separator;
                tail++;
                space--;
            }
            else /* truncation */
            {
                *tail = '\0';
                return buf_size;
            }
        }

        ip = ip->next;
        CYCLE_CHECK(ip, slow, toggle);
    }

    *tail = '\0';
    return tail - buf;
}

/*********************************************************************/
/* Basic operations                                                  */
/*********************************************************************/

void IncrementItemListCounter(Item *list, const char *item)
{
    if (item == NULL || item[0] == '\0')
    {
        return;
    }

    Item *ptr = list;
    CYCLE_DECLARE(ptr, slow, toggle);
    while (ptr != NULL)
    {
        if (strcmp(ptr->name, item) == 0)
        {
            ptr->counter++;
            return;
        }
        ptr = ptr->next;
        CYCLE_CHECK(ptr, slow, toggle);
    }
}

/*********************************************************************/

void SetItemListCounter(Item *list, const char *item, int value)
{
    if (item == NULL || item[0] == '\0')
    {
        return;
    }

    Item *ptr = list;
    CYCLE_DECLARE(ptr, slow, toggle);
    while (ptr != NULL)
    {
        if (strcmp(ptr->name, item) == 0)
        {
            ptr->counter = value;
            return;
        }
        ptr = ptr->next;
        CYCLE_CHECK(ptr, slow, toggle);
    }
}

/*********************************************************************/

int IsMatchItemIn(const Item *list, const char *item)
/* Solve for possible regex/fuzzy models unified */
{
    if (item == NULL || item[0] == '\0')
    {
        return true;
    }

    const Item *ptr = list;
    CYCLE_DECLARE(ptr, slow, toggle);
    while (ptr != NULL)
    {
        if (FuzzySetMatch(ptr->name, item) == 0 ||
            (IsRegex(ptr->name) &&
             StringMatchFull(ptr->name, item)))
        {
            return true;
        }
        ptr = ptr->next;
        CYCLE_CHECK(ptr, slow, toggle);
    }

    return false;
}

/*********************************************************************/
/* Cycle-detection: you'll get a double free if there's a cycle. */

void DeleteItemList(Item *item) /* delete starting from item */
{
    while (item != NULL)
    {
        Item *here = item;
        item = here->next; /* before free()ing here */

        free(here->name);
        free(here->classes);
        free(here);
    }
}

/*********************************************************************/

void DeleteItem(Item **liststart, Item *item)
{
    if (item != NULL)
    {
        if (item == *liststart)
        {
            *liststart = item->next;
        }
        else
        {
            Item *ip = *liststart;
            CYCLE_DECLARE(ip, slow, toggle);
            while (ip && ip->next != item)
            {
                ip = ip->next;
                CYCLE_CHECK(ip, slow, toggle);
            }

            if (ip != NULL)
            {
                assert(ip->next == item);
                ip->next = item->next;
            }
        }

        free(item->name);
        free(item->classes);
        free(item);
    }
}

/*********************************************************************/

/* DeleteItem* function notes:
 * -They all take an item list and an item specification ("string" argument.)
 * -Some of them treat the item spec as a literal string, while others
 *  treat it as a regular expression.
 * -They all delete the first item meeting their criteria, as below.
 *  function   deletes item
 *  ------------------------------------------------------------------------
 *  DeleteItemStarting  start is literally equal to string item spec
 *  DeleteItemLiteral  literally equal to string item spec
 *  DeleteItemMatching  fully matched by regex item spec
 *  DeleteItemContaining containing string item spec
 */

/*********************************************************************/

int DeleteItemGeneral(Item **list, const char *string, ItemMatchType type)
{
    if (list == NULL)
    {
        return false;
    }

    pcre *rx = NULL;
    if (type == ITEM_MATCH_TYPE_REGEX_COMPLETE_NOT ||
        type == ITEM_MATCH_TYPE_REGEX_COMPLETE)
    {
        rx = CompileRegex(string);
        if (!rx)
        {
            return false;
        }
    }

    Item *ip = *list, *last = NULL;
    CYCLE_DECLARE(ip, slow, toggle);
    while (ip != NULL)
    {
        if (ip->name != NULL)
        {
            bool match = false, flip = false;
            switch (type)
            {
            case ITEM_MATCH_TYPE_LITERAL_START_NOT:
                flip = true; /* and fall through */
            case ITEM_MATCH_TYPE_LITERAL_START:
                match = (strncmp(ip->name, string, strlen(string)) == 0);
                break;

            case ITEM_MATCH_TYPE_LITERAL_COMPLETE_NOT:
                flip = true; /* and fall through */
            case ITEM_MATCH_TYPE_LITERAL_COMPLETE:
                match = (strcmp(ip->name, string) == 0);
                break;

            case ITEM_MATCH_TYPE_LITERAL_SOMEWHERE_NOT:
                flip = true; /* and fall through */
            case ITEM_MATCH_TYPE_LITERAL_SOMEWHERE:
                match = (strstr(ip->name, string) != NULL);
                break;

            case ITEM_MATCH_TYPE_REGEX_COMPLETE_NOT:
                flip = true; /* and fall through */
            case ITEM_MATCH_TYPE_REGEX_COMPLETE:
                match = StringMatchFullWithPrecompiledRegex(rx, ip->name);
                break;
            }
            if (flip)
            {
                match = !match;
            }

            if (match)
            {
                if (ip == *list)
                {
                    *list = ip->next;
                }
                else
                {
                    assert(ip != NULL);
                    assert(last != NULL);
                    assert(last->next == ip);
                    last->next = ip->next;
                }

                free(ip->name);
                free(ip->classes);
                free(ip);
                if (rx)
                {
                    pcre_free(rx);
                }

                return true;
            }
        }
        last = ip;
        ip = ip->next;
        CYCLE_CHECK(ip, slow, toggle);
    }

    if (rx)
    {
        pcre_free(rx);
    }

    return false;
}

/*********************************************************************/

int DeleteItemStarting(Item **list, const char *string)       /* delete 1st item starting with string */
{
    return DeleteItemGeneral(list, string, ITEM_MATCH_TYPE_LITERAL_START);
}

/*********************************************************************/

int DeleteItemNotStarting(Item **list, const char *string)    /* delete 1st item starting with string */
{
    return DeleteItemGeneral(list, string, ITEM_MATCH_TYPE_LITERAL_START_NOT);
}

/*********************************************************************/

int DeleteItemLiteral(Item **list, const char *string)  /* delete 1st item which is string */
{
    return DeleteItemGeneral(list, string, ITEM_MATCH_TYPE_LITERAL_COMPLETE);
}

/*********************************************************************/

int DeleteItemMatching(Item **list, const char *string)       /* delete 1st item fully matching regex */
{
    return DeleteItemGeneral(list, string, ITEM_MATCH_TYPE_REGEX_COMPLETE);
}

/*********************************************************************/

int DeleteItemNotMatching(Item **list, const char *string)    /* delete 1st item fully matching regex */
{
    return DeleteItemGeneral(list, string, ITEM_MATCH_TYPE_REGEX_COMPLETE_NOT);
}

/*********************************************************************/

int DeleteItemContaining(Item **list, const char *string)     /* delete first item containing string */
{
    return DeleteItemGeneral(list, string, ITEM_MATCH_TYPE_LITERAL_SOMEWHERE);
}

/*********************************************************************/

int DeleteItemNotContaining(Item **list, const char *string)  /* delete first item containing string */
{
    return DeleteItemGeneral(list, string, ITEM_MATCH_TYPE_LITERAL_SOMEWHERE_NOT);
}

/*********************************************************************/

bool RawSaveItemList(const Item *liststart, const char *filename, NewLineMode new_line_mode)
{
    char new[CF_BUFSIZE];
    strcpy(new, filename);
    strcat(new, CF_EDITED);
    unlink(new);                /* Just in case of races */

    FILE *fp = safe_fopen(new, (new_line_mode == NewLineMode_Native) ? "wt" : "w");
    if (fp == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't write file '%s'. (fopen: %s)", new, GetErrorStr());
        return false;
    }

    const Item *ip = liststart;
    CYCLE_DECLARE(ip, slow, toggle);
    while (ip != NULL)
    {
        fprintf(fp, "%s\n", ip->name);
        ip = ip->next;
        CYCLE_CHECK(ip, slow, toggle);
    }

    if (fclose(fp) == -1)
    {
        Log(LOG_LEVEL_ERR, "Unable to close file '%s' while writing. (fclose: %s)", new, GetErrorStr());
        return false;
    }

    if (rename(new, filename) == -1)
    {
        Log(LOG_LEVEL_INFO, "Error while renaming file '%s' to '%s'. (rename: %s)", new, filename, GetErrorStr());
        return false;
    }

    return true;
}

Item *RawLoadItemList(const char *filename)
{
    FILE *fp = safe_fopen(filename, "r");
    if (!fp)
    {
        return NULL;
    }

    size_t line_size = CF_BUFSIZE;
    char *line = xmalloc(line_size);

    Item *list = NULL;
    while (CfReadLine(&line, &line_size, fp) != -1)
    {
        PrependItem(&list, line, NULL);
    }

    free(line);

    if (!feof(fp))
    {
        Log(LOG_LEVEL_ERR, "Error while reading item list from file: %s", filename);
        DeleteItemList(list);
        list = NULL;
    }
    fclose(fp);

    return ReverseItemList(list);
}
