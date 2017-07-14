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

#ifndef CFENGINE_REFCOUNT_H
#define CFENGINE_REFCOUNT_H

#include <platform.h>
/**
  @brief Simple reference count implementation.

  Reference counting helps to keep track of elements and avoid unnecessary duplication.
  In C we need to manually keep track of the users, while in C++ this is implicitly done
  by the "this" pointer. If we don't do that and we just count how many users we have,
  we risk multiples attach or detach. We need one way to find out who is connected to
  the refcount so we can act properly.
  */
struct RefCountNode {
    struct RefCountNode *next;
    struct RefCountNode *previous;
    void *user;
};
typedef struct RefCountNode RefCountNode;

struct RefCount {
    // Normally one unless we are shared.
    unsigned int user_count;
    RefCountNode *users;
    RefCountNode *last;
};
typedef struct RefCount RefCount;

/**
  @brief Initializes a refcount structure.
  @param ref RefCount structure to be initialized.
  */
void RefCountNew(RefCount **ref);
/**
  @brief Destroys a refcount structure.
  @param ref RefCount structure to be destroyed.
  */
void RefCountDestroy(RefCount **ref);
/**
  @brief Attaches a data structure to a given RefCount structure.
  Attaching refers to the fact that the container is using a data structure that might be shared by others.
  This should be called before using the data structure so everybody is aware of the new holder.
  @param ref RefCountr structure
  @param owner Data structure to be attached.
  */
void RefCountAttach(RefCount *ref, void *owner);
/**
  @brief Detaches a data structure from a given RefCount structure.
  Detaching should be called after the container has copied the data structure. As long as the container is
  still using the data structure it should not detach from the reference counting. Otherwise this might lead
  to undesired side effects.
  @param ref RefCountr structure
  @param owner Data structure to be detached.
  */
void RefCountDetach(RefCount *ref, void *owner);
/**
  @brief Simple check to see if a given data structure is shared.
  @param ref RefCount structure.
  @return True if shared, false otherwise.
  */
bool RefCountIsShared(RefCount *ref);
/**
  @brief Compares two RefCount structures.
  @param a
  @param b
  @return True if a and b point to the same object, false otherwise.
  @remarks This function is needed in order to speed up comparisons of complex
  data structures. If the RefCount objects of the two structures are the same,
  then most likely the structures are the same.
  */
bool RefCountIsEqual(RefCount *a, RefCount *b);

#endif // CFENGINE_REFCOUNT_H
