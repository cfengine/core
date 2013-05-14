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

#include "alloc.h"
#include "refcount.h"

void RefCountNew(RefCount **ref)
{
    if (!ref)
    {
        return;
    }
    *ref = (RefCount *)xmalloc(sizeof(RefCount));
    (*ref)->user_count = 0;
    (*ref)->users = NULL;
    (*ref)->last = NULL;
}

void RefCountDestroy(RefCount **ref)
{
    if (ref && *ref)
    {
        // Don't destroy the refCount if it is still in use by somebody else.
        if ((*ref)->user_count > 1)
            return;
        free(*ref);
        *ref = NULL;
    }
}

int RefCountAttach(RefCount *ref, void *owner)
{
    if (!ref || !owner)
    {
        return -1;
    }
    ref->user_count++;
    RefCountNode *node = (RefCountNode *)xmalloc(sizeof(RefCountNode));
    node->next = NULL;
    node->user = owner;
    if (ref->last)
    {
        ref->last->next = node;
        node->previous = ref->last;
    }
    else
    {
        ref->users = node;
        node->previous = NULL;
    }
    ref->last = node;
    return ref->user_count;
}

int RefCountDetach(RefCount *ref, void *owner)
{
    if (!ref || !owner)
    {
        return -1;
    }
    RefCountNode *p = NULL;
    int found = 0;
    for (p = ref->users; p; p = p->next)
    {
        if (p->user == owner)
        {
            found = 1;
            if (p->previous && p->next)
            {
                p->previous->next = p->next;
                p->next->previous = p->previous;
            }
            else if (p->previous && !p->next)
            {
                // Last node
                p->previous->next = NULL;
                ref->last = p->previous;
            }
            else if (!p->previous && p->next)
            {
                // First node
                ref->users = p->next;
                p->next->previous = NULL;
            }
            else
            {
                // Only one node, we cannot detach from ourselves.
                return 0;
            }
            free(p);
            break;
        }
    }
    if (!found)
    {
        return -1;
    }
    ref->user_count--;
    return ref->user_count;
}

int RefCountIsShared(RefCount *ref)
{
    if (!ref)
    {
        return 0;
    }
    if (ref->user_count == 0)
    {
        return 0;
    }
    return (ref->user_count != 1);
}

int RefCountIsEqual(RefCount *a, RefCount *b)
{
    if (a == b)
    {
        return 1;
    }
    if (a && b)
    {
        // Compare the inner elements
        if (a->user_count == b->user_count)
        {
            RefCountNode *na = a->users;
            RefCountNode *nb = b->users;
            int equal = 1;
            while (na && nb)
            {
                if (na->user != nb->user)
                {
                    equal = 0;
                    break;
                }
                na = na->next;
                nb = nb->next;
            }
            return equal;
        }
    }
    return 0;
}
