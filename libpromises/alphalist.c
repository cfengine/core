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

#include "alphalist.h"

#include "item_lib.h"
#include "matching.h"

/*****************************************************************************/
/* This library creates a simple indexed array of lists for optimization of
   high entropy class searches.

 AlphaList al;
 Item *ip;
 int i;

 InitAlphaList(&al);
 PrependAlphaList(&al,"one");
 PrependAlphaList(&al,"two");
 PrependAlphaList(&al,"three");
 PrependAlphaList(&al,"onetwo");
 VERBOSE = 1;
 ShowAlphaList(al);
 exit(0); */

/*****************************************************************************/

void InitAlphaList(AlphaList *al)
{
    memset(al, 0, sizeof(AlphaList));
}

/*****************************************************************************/

void DeleteAlphaList(AlphaList *al)
{
    for (int i = 0; i < CF_ALPHABETSIZE; i++)
    {
        DeleteItemList(al->list[i]);
    }

    InitAlphaList(al);
}

/*****************************************************************************/

AlphaList *CopyAlphaListPointers(AlphaList *ap, const AlphaList *al)
{
    if (ap != NULL)
    {
        memcpy(ap, al, sizeof(AlphaList));
    }

    return ap;
}

/*****************************************************************************/

AlphaList *DupAlphaListPointers(AlphaList *ap, AlphaList *al)
{
    if (ap != NULL)
    {
        memcpy(ap, al, sizeof(AlphaList));
    }

    for (int i = 0; i < CF_ALPHABETSIZE; i++)
    {
        Item *tmp = NULL;
        if (al->list[i])
        {
            CopyList(&tmp, al->list[i]);
            al->list[i] = tmp;
        }
    }

    return ap;
}

/*****************************************************************************/

int InAlphaList(const AlphaList *al, const char *string)
{
    int i = (int) *string;

    return IsItemIn(al->list[i], string);
}

/*****************************************************************************/

int MatchInAlphaList(const AlphaList *al, const char *string)
{
    Item *ip;
    int i = (int) *string;

    if (isalnum(i) || *string == '_')
    {
        for (ip = al->list[i]; ip != NULL; ip = ip->next)
        {
            if (FullTextMatch(string, ip->name))
            {
                return true;
            }
        }
    }
    else
    {
        // We don't know what the correct hash is because the pattern in vague

        for (i = 0; i < CF_ALPHABETSIZE; i++)
        {
            for (ip = al->list[i]; ip != NULL; ip = ip->next)
            {
                if (FullTextMatch(string, ip->name))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

/*****************************************************************************/

void PrependAlphaList(AlphaList *al, const char *string)
{
    int i = (int) *string;

    al->list[i] = PrependItem(&(al->list[i]), string, NULL);
}

/*****************************************************************************/

void IdempPrependAlphaList(AlphaList *al, const char *string)
{
    if (!InAlphaList(al, string))
    {
        PrependAlphaList(al, string);
    }
}

/*****************************************************************************/

void DeleteFromAlphaList(AlphaList *al, const char *string)
{
    DeleteItemLiteral(&al->list[(int) *string], string);
}

/*****************************************************************************/

AlphaListIterator AlphaListIteratorInit(AlphaList *al)
{
    return (AlphaListIterator) {al, -1, NULL};
}

/*****************************************************************************/

const Item *AlphaListIteratorNext(AlphaListIterator *iterator)
{
    while (iterator->curitem == NULL)
    {
        if (++iterator->pos == CF_ALPHABETSIZE)
        {
            return NULL;
        }

        if (iterator->al->list[iterator->pos] != NULL)
        {
            iterator->curitem = iterator->al->list[iterator->pos];
        }
    }

    const Item *ret = iterator->curitem;

    iterator->curitem = ret->next;
    return ret;
}
