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

#include <alloc.h>
#include <refcount.h>
#include <misc_lib.h>
#include <platform.h>

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
        // Destroying a refcount which has more than one user is a bug, but we let it
        // pass in production code (memory leak).
        assert((*ref)->user_count <= 1);
        if ((*ref)->user_count > 1)
            return;
        if ((*ref)->users)
            free((*ref)->users);
        free(*ref);
        *ref = NULL;
    }
}

void RefCountAttach(RefCount *ref, void *owner)
{
    if (!ref || !owner)
    {
        ProgrammingError("Either refcount or owner is NULL (or both)");
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
}

void RefCountDetach(RefCount *ref, void *owner)
{
    if (!ref || !owner)
    {
        ProgrammingError("Either refcount or owner is NULL (or both)");
    }
    if (ref->user_count <= 1)
    {
        /*
         * Semantics: If 1 that means that we are the only users, if 0 nobody is using it.
         * In either case it is safe to destroy the refcount.
         */
        return;
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
                return;
            }
            free(p);
            break;
        }
    }
    if (!found)
    {
        ProgrammingError("The object is not attached to the RefCount object");
    }
    ref->user_count--;
}

bool RefCountIsShared(RefCount *ref)
{
    return ref && (ref->user_count > 1);
}

bool RefCountIsEqual(RefCount *a, RefCount *b)
{
    return (a && b) && (a == b);
}
