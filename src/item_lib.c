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
#include "addr_lib.h"
#include "item_lib.h"

/*******************************************************************/

int PrintItemList(char *buffer, int bufsize, Item *list)
{
    Item *ip;

    StartJoin(buffer, "{", bufsize);

    for (ip = list; ip != NULL; ip = ip->next)
    {
        if (!JoinSilent(buffer, "'", bufsize))
        {
            EndJoin(buffer, "...TRUNCATED'}", bufsize);
            return false;
        }

        if (!Join(buffer,ip->name,bufsize))
        {
            EndJoin(buffer, "...TRUNCATED'}", bufsize);
            return false;
        }

        if (!JoinSilent(buffer, "'", bufsize))
        {
            EndJoin(buffer, "...TRUNCATED'}", bufsize);
            return false;
        }

        if (ip->next != NULL)
        {
            if (!JoinSilent(buffer, ",", bufsize))
            {
                EndJoin(buffer, "...TRUNCATED}", bufsize);
                return false;
            }
        }
    }

    EndJoin(buffer, "}", bufsize);

    return true;
}

/*********************************************************************/

int ItemListSize(Item *list)
{
    int size = 0;
    Item *ip;

    for (ip = list; ip != NULL; ip = ip->next)
    {
        if (ip->name)
        {
            size += strlen(ip->name);
        }
    }

    return size;
}

/*********************************************************************/

Item *ReturnItemIn(Item *list, const char *item)
{
    Item *ptr;

    if ((item == NULL) || (strlen(item) == 0))
    {
        return NULL;
    }

    for (ptr = list; ptr != NULL; ptr = ptr->next)
    {
        if (strcmp(ptr->name, item) == 0)
        {
            return ptr;
        }
    }

    return NULL;
}

/*********************************************************************/

Item *ReturnItemInClass(Item *list, char *item, char *classes)
{
    Item *ptr;

    if ((item == NULL) || (strlen(item) == 0))
    {
        return NULL;
    }

    for (ptr = list; ptr != NULL; ptr = ptr->next)
    {
        if (strcmp(ptr->name, item) == 0 && strcmp(ptr->classes, classes) == 0)
        {
            return ptr;
        }
    }

    return NULL;
}

/*********************************************************************/

Item *ReturnItemAtIndex(Item *list, int index)
{
    Item *ptr;
    int i = 0;

    for (ptr = list; ptr != NULL; ptr = ptr->next)
    {

        if (i == index)
        {
            return ptr;
        }

        i++;
    }

    return NULL;
}

/*********************************************************************/

int GetItemIndex(Item *list, char *item)
/*
 * Returns index of first occurence of item.
 */
{
    Item *ptr;
    int i = 0;

    if ((item == NULL) || (strlen(item) == 0))
    {
        return -1;
    }

    for (ptr = list; ptr != NULL; ptr = ptr->next)
    {
        if (strcmp(ptr->name, item) == 0)
        {
            return i;
        }

        i++;
    }

    return -1;
}

/*********************************************************************/

bool IsItemIn(Item *list, const char *item)
{
    Item *ptr;

    if ((item == NULL) || (strlen(item) == 0))
    {
        return true;
    }

    for (ptr = list; ptr != NULL; ptr = ptr->next)
    {
        if (strcmp(ptr->name, item) == 0)
        {
            return true;
        }
    }

    return false;
}

/*********************************************************************/

Item *EndOfList(Item *start)
{
    Item *ip, *prev = CF_UNDEFINED_ITEM;

    for (ip = start; ip != NULL; ip = ip->next)
    {
        prev = ip;
    }

    return prev;
}

/*********************************************************************/

int IsItemInRegion(char *item, Item *begin_ptr, Item *end_ptr, Attributes a, Promise *pp)
{
    Item *ip;

    for (ip = begin_ptr; (ip != end_ptr && ip != NULL); ip = ip->next)
    {
        if (MatchPolicy(item, ip->name, a, pp))
        {
            return true;
        }
    }

    return false;
}

/*********************************************************************/

Item *IdempPrependItem(Item **liststart, const char *itemstring, const char *classes)
{
    Item *ip;

    ip = ReturnItemIn(*liststart, itemstring);

    if (ip)
    {
        return ip;
    }

    PrependItem(liststart, itemstring, classes);
    return *liststart;
}

/*********************************************************************/

Item *IdempPrependItemClass(Item **liststart, char *itemstring, char *classes)
{
    Item *ip;

    ip = ReturnItemInClass(*liststart, itemstring, classes);

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
    Item *ip;

    if ((ip = ReturnItemIn(*liststart, itemstring)))
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
    Item *ip;

    ip = xcalloc(1, sizeof(Item));

    ip->name = xstrdup(itemstring);
    ip->next = *liststart;
    if (classes != NULL)
    {
        ip->classes = xstrdup(classes);
    }

    *liststart = ip;

    return *liststart;
}

/*********************************************************************/

void PrependFullItem(Item **liststart, char *itemstring, char *classes, int counter, time_t t)
{
    Item *ip;

    ip = xcalloc(1, sizeof(Item));

    ip->name = xstrdup(itemstring);
    ip->next = *liststart;
    ip->counter = counter;
    ip->time = t;
    *liststart = ip;

    if (classes != NULL)
    {
        ip->classes = xstrdup(classes);
    }
}

/*********************************************************************/

void AppendItem(Item **liststart, const char *itemstring, const char *classes)
{
    Item *lp;
    Item *ip = xcalloc(1, sizeof(Item));

    ip->name = xstrdup(itemstring);

    if (*liststart == NULL)
    {
        *liststart = ip;
    }
    else
    {
        for (lp = *liststart; lp->next != NULL; lp = lp->next)
        {
        }

        lp->next = ip;
    }

    if (classes)
    {
        ip->classes = xstrdup(classes); /* unused now */
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

int ListLen(Item *list)
{
    int count = 0;
    Item *ip;

    CfDebug("Check ListLen\n");

    for (ip = list; ip != NULL; ip = ip->next)
    {
        count++;
    }

    return count;
}

/***************************************************************************/

void CopyList(Item **dest, Item *source)
/* Copy or concat lists */
{
    Item *ip;

    if (*dest != NULL)
    {
        FatalError("CopyList - list not initialized");
    }

    if (source == NULL)
    {
        return;
    }

    for (ip = source; ip != NULL; ip = ip->next)
    {
        AppendItem(dest, ip->name, ip->classes);
    }
}

/*********************************************************************/

Item *ConcatLists(Item *list1, Item *list2)
/* Notes: * Refrain from freeing list2 after using ConcatLists
          * list1 must have at least one element in it */
{
    Item *endOfList1;

    if (list1 == NULL)
    {
        FatalError("ConcatLists: first argument must have at least one element");
    }

    for (endOfList1 = list1; endOfList1->next != NULL; endOfList1 = endOfList1->next)
    {
    }

    endOfList1->next = list2;
    return list1;
}

/***************************************************************************/
/* Search                                                                  */
/***************************************************************************/

int SelectItemMatching(Item *start, char *regex, Item *begin_ptr, Item *end_ptr, Item **match, Item **prev, char *fl)
{
    Item *ip;
    int ret = false;

    *match = CF_UNDEFINED_ITEM;
    *prev = CF_UNDEFINED_ITEM;

    if (regex == NULL)
    {
        return false;
    }

    if (fl && (strcmp(fl, "first") == 0))
    {
        if (SelectNextItemMatching(regex, begin_ptr, end_ptr, match, prev))
        {
            ret = true;
        }
    }
    else
    {
        if (SelectLastItemMatching(regex, begin_ptr, end_ptr, match, prev))
        {
            ret = true;
        }
    }

    if (*match != CF_UNDEFINED_ITEM && *prev == CF_UNDEFINED_ITEM)
    {
        for (ip = start; ip != NULL && ip != *match; ip = ip->next)
        {
            *prev = ip;
        }
    }

    return ret;
}

/*********************************************************************/

int SelectNextItemMatching(char *regexp, Item *begin, Item *end, Item **match, Item **prev)
{
    Item *ip, *ip_prev = CF_UNDEFINED_ITEM;

    *match = CF_UNDEFINED_ITEM;
    *prev = CF_UNDEFINED_ITEM;

    for (ip = begin; ip != end; ip = ip->next)
    {
        if (ip->name == NULL)
        {
            continue;
        }

        if (FullTextMatch(regexp, ip->name))
        {
            *match = ip;
            *prev = ip_prev;
            return true;
        }

        ip_prev = ip;
    }

    return false;
}

/*********************************************************************/

int SelectLastItemMatching(char *regexp, Item *begin, Item *end, Item **match, Item **prev)
{
    Item *ip, *ip_last = NULL, *ip_prev = CF_UNDEFINED_ITEM;

    *match = CF_UNDEFINED_ITEM;
    *prev = CF_UNDEFINED_ITEM;

    for (ip = begin; ip != end; ip = ip->next)
    {
        if (ip->name == NULL)
        {
            continue;
        }

        if (FullTextMatch(regexp, ip->name))
        {
            *prev = ip_prev;
            ip_last = ip;
        }

        ip_prev = ip;
    }

    if (ip_last)
    {
        *match = ip_last;
        return true;
    }

    return false;
}

/*********************************************************************/

int MatchRegion(char *chunk, Item *start, Item *begin, Item *end)
/*
  Match a region in between the selection delimiters. It is
  called after SelectRegion. The end delimiter will be visible
  here so we have to check for it. Can handle multi-line chunks
*/
{
    Item *ip = begin;
    char *sp, buf[CF_BUFSIZE];
    int lines = 0;

    for (sp = chunk; sp <= chunk + strlen(chunk); sp++)
    {
        memset(buf, 0, CF_BUFSIZE);
        sscanf(sp, "%[^\n]", buf);
        sp += strlen(buf);

        if (ip == NULL)
        {
            return false;
        }

        if (!FullTextMatch(buf, ip->name))
        {
            return false;
        }

        lines++;

        // We have to manually exclude the marked terminator

        if (ip == end)
        {
            return false;
        }

        // Now see if there is more

        if (ip->next)
        {
            ip = ip->next;
        }
        else                    // if the region runs out before the end
        {
            if (++sp <= chunk + strlen(chunk))
            {
                return false;
            }

            break;
        }
    }

    return lines;
}

/*********************************************************************/
/* Level                                                             */
/*********************************************************************/

void InsertAfter(Item **filestart, Item *ptr, char *string)
{
    Item *ip;

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

    ip = xcalloc(1, sizeof(Item));

    ip->next = ptr->next;
    ptr->next = ip;
    ip->name = xstrdup(string);
    ip->classes = NULL;
}

/*********************************************************************/

int NeighbourItemMatches(Item *file_start, Item *location, char *string, enum cfeditorder pos, Attributes a,
                         Promise *pp)
{
    Item *ip;

/* Look for a line matching proposed insert before or after location */

    for (ip = file_start; ip != NULL; ip = ip->next)
    {
        if (pos == cfe_before)
        {
            if (ip->next && ip->next == location)
            {
                if (MatchPolicy(string, ip->name, a, pp))
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
        }

        if (pos == cfe_after)
        {
            if (ip == location)
            {
                if (ip->next && MatchPolicy(string, ip->next->name, a, pp))
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

    return false;
}

/*********************************************************************/

Item *SplitString(const char *string, char sep)
 /* Splits a string containing a separator like : 
    into a linked list of separate items, */
{
    Item *liststart = NULL;
    const char *sp;
    char before[CF_BUFSIZE];
    int i = 0;

    CfDebug("SplitString([%s],%c=%d)\n", string, sep, sep);

    for (sp = string; (*sp != '\0'); sp++, i++)
    {
        before[i] = *sp;

        if (*sp == sep)
        {
            /* Check the listsep is not escaped */

            if ((sp > string) && (*(sp - 1) != '\\'))
            {
                before[i] = '\0';
                AppendItem(&liststart, before, NULL);
                i = -1;
            }
            else if ((sp > string) && (*(sp - 1) == '\\'))
            {
                i--;
                before[i] = sep;
            }
            else
            {
                before[i] = '\0';
                AppendItem(&liststart, before, NULL);
                i = -1;
            }
        }
    }

    before[i] = '\0';
    AppendItem(&liststart, before, "");

    return liststart;
}

/*********************************************************************/

Item *SplitStringAsItemList(char *string, char sep)
 /* Splits a string containing a separator like : 
    into a linked list of separate items, */
{
    Item *liststart = NULL;
    char format[9], *sp;
    char node[CF_MAXVARSIZE];

    CfDebug("SplitStringAsItemList(%s,%c)\n", string, sep);

    sprintf(format, "%%255[^%c]", sep); /* set format string to search */

    for (sp = string; *sp != '\0'; sp++)
    {
        memset(node, 0, CF_MAXVARSIZE);
        sscanf(sp, format, node);

        if (strlen(node) == 0)
        {
            continue;
        }

        sp += strlen(node) - 1;

        AppendItem(&liststart, node, NULL);

        if (*sp == '\0')
        {
            break;
        }
    }

    return liststart;
}

/*********************************************************************/

char *ItemList2CSV(Item *list)
{
    Item *ip;
    int len = 0;
    char *s;

    for (ip = list; ip != NULL; ip = ip->next)
    {
        len += strlen(ip->name) + 1;
    }

    s = xmalloc(len + 1);
    *s = '\0';

    for (ip = list; ip != NULL; ip = ip->next)
    {
        strcat(s, ip->name);

        if (ip->next)
        {
            strcat(s, ",");
        }
    }

    return s;
}

/*********************************************************************/
/* Basic operations                                                  */
/*********************************************************************/

void IncrementItemListCounter(Item *list, char *item)
{
    Item *ptr;

    if ((item == NULL) || (strlen(item) == 0))
    {
        return;
    }

    for (ptr = list; ptr != NULL; ptr = ptr->next)
    {
        if (strcmp(ptr->name, item) == 0)
        {
            ptr->counter++;
            return;
        }
    }
}

/*********************************************************************/

void SetItemListCounter(Item *list, char *item, int value)
{
    Item *ptr;

    if ((item == NULL) || (strlen(item) == 0))
    {
        return;
    }

    for (ptr = list; ptr != NULL; ptr = ptr->next)
    {
        if (strcmp(ptr->name, item) == 0)
        {
            ptr->counter = value;
            return;
        }
    }
}

/*********************************************************************/

int IsMatchItemIn(Item *list, char *item)
/* Solve for possible regex/fuzzy models unified */
{
    Item *ptr;

    if ((item == NULL) || (strlen(item) == 0))
    {
        return true;
    }

    for (ptr = list; ptr != NULL; ptr = ptr->next)
    {
        if (FuzzySetMatch(ptr->name, item) == 0)
        {
            return (true);
        }

        if (IsRegex(ptr->name))
        {
            if (FullTextMatch(ptr->name, item))
            {
                return (true);
            }
        }
    }

    return (false);
}

/*********************************************************************/

void DeleteItemList(Item *item) /* delete starting from item */
{
    Item *ip, *next;

    for (ip = item; ip != NULL; ip = next)
    {
        next = ip->next;        // save before free

        if (ip->name != NULL)
        {
            free(ip->name);
        }

        if (ip->classes != NULL)
        {
            free(ip->classes);
        }

        free((char *) ip);
    }
}

/*********************************************************************/

void DeleteItem(Item **liststart, Item *item)
{
    Item *ip, *sp;

    if (item != NULL)
    {
        if (item->name != NULL)
        {
            free(item->name);
        }

        if (item->classes != NULL)
        {
            free(item->classes);
        }

        sp = item->next;

        if (item == *liststart)
        {
            *liststart = sp;
        }
        else
        {
            for (ip = *liststart; ip != NULL && ip->next != item && ip->next != NULL; ip = ip->next)
            {
            }

            if (ip != NULL)
            {
                ip->next = sp;
            }
        }

        free((char *) item);
    }
}

/*********************************************************************/

void DebugListItemList(Item *liststart)
{
    Item *ptr;

    for (ptr = liststart; ptr != NULL; ptr = ptr->next)
    {
        if (ptr->classes)
        {
            printf("CFDEBUG: %s::[%s]\n", ptr->classes, ptr->name);
        }
        else
        {
            printf("CFDEBUG: [%s]\n", ptr->name);
        }
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

int DeleteItemGeneral(Item **list, const char *string, enum matchtypes type)
{
    Item *ip, *last = NULL;
    int match = 0;

    if (list == NULL)
    {
        return false;
    }

    for (ip = *list; ip != NULL; ip = ip->next)
    {
        if (ip->name == NULL)
        {
            continue;
        }

        switch (type)
        {
        case NOTliteralStart:
            match = (strncmp(ip->name, string, strlen(string)) != 0);
            break;
        case literalStart:
            match = (strncmp(ip->name, string, strlen(string)) == 0);
            break;
        case NOTliteralComplete:
            match = (strcmp(ip->name, string) != 0);
            break;
        case literalComplete:
            match = (strcmp(ip->name, string) == 0);
            break;
        case NOTliteralSomewhere:
            match = (strstr(ip->name, string) == NULL);
            break;
        case literalSomewhere:
            match = (strstr(ip->name, string) != NULL);
            break;
        case NOTregexComplete:
        case regexComplete:
            /* To fix a bug on some implementations where rx gets emptied */
            match = FullTextMatch(string, ip->name);

            if (type == NOTregexComplete)
            {
                match = !match;
            }
            break;
        }

        if (match)
        {
            if (ip == *list)
            {
                free((*list)->name);
                if (ip->classes != NULL)
                {
                    free(ip->classes);
                }
                *list = ip->next;
                free((char *) ip);
                return true;
            }
            else
            {
                if (ip != NULL)
                {
                    if (last != NULL)
                    {
                        last->next = ip->next;
                    }

                    free(ip->name);
                    if (ip->classes != NULL)
                    {
                        free(ip->classes);
                    }
                    free((char *) ip);
                }

                return true;
            }

        }
        last = ip;
    }

    return false;
}

/*********************************************************************/

int DeleteItemStarting(Item **list, char *string)       /* delete 1st item starting with string */
{
    return DeleteItemGeneral(list, string, literalStart);
}

/*********************************************************************/

int DeleteItemNotStarting(Item **list, char *string)    /* delete 1st item starting with string */
{
    return DeleteItemGeneral(list, string, NOTliteralStart);
}

/*********************************************************************/

int DeleteItemLiteral(Item **list, const char *string)  /* delete 1st item which is string */
{
    return DeleteItemGeneral(list, string, literalComplete);
}

/*********************************************************************/

int DeleteItemMatching(Item **list, char *string)       /* delete 1st item fully matching regex */
{
    return DeleteItemGeneral(list, string, regexComplete);
}

/*********************************************************************/

int DeleteItemNotMatching(Item **list, char *string)    /* delete 1st item fully matching regex */
{
    return DeleteItemGeneral(list, string, NOTregexComplete);
}

/*********************************************************************/

int DeleteItemContaining(Item **list, char *string)     /* delete first item containing string */
{
    return DeleteItemGeneral(list, string, literalSomewhere);
}

/*********************************************************************/

int DeleteItemNotContaining(Item **list, char *string)  /* delete first item containing string */
{
    return DeleteItemGeneral(list, string, NOTliteralSomewhere);
}

/*********************************************************************/

int ByteSizeList(const Item *list)
{
    int count = 0;
    const Item *ip;

    for (ip = list; ip; ip = ip->next)
    {
        count += strlen(ip->name);
    }

    return count;
}
