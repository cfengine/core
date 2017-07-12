/*
   Copyright 2017 Northern.tech AS

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
  versions of CFEngine, the applicable Commercial Open Source License
  (COSL) may apply to this file if you as a licensee so wish it. See
  included file COSL.txt.
*/
#ifndef CFENGINE_RB_TREE_H
#define CFENGINE_RB_TREE_H

#include <platform.h>

typedef struct RBTree_ RBTree;
typedef struct RBTreeIterator_ RBTreeIterator;

typedef void *RBTreeKeyCopyFn(const void *key);
typedef int RBTreeKeyCompareFn(const void *a, const void *b);
typedef void RBTreeKeyDestroyFn(void *key);
typedef void *RBTreeValueCopyFn(const void *key);
typedef int RBTreeValueCompareFn(const void *a, const void *b);
typedef void RBTreeValueDestroyFn(void *key);

typedef bool RBTreePredicate(const void *key, const void *value, void *user_data);

RBTree *RBTreeNew(RBTreeKeyCopyFn *key_copy,
                  RBTreeKeyCompareFn *key_compare,
                  RBTreeKeyDestroyFn *key_destroy,
                  RBTreeValueCopyFn *value_copy,
                  RBTreeValueCompareFn *value_compare,
                  RBTreeValueDestroyFn *value_destroy);

RBTree *RBTreeCopy(const RBTree *tree, RBTreePredicate *filter, void *user_data);

bool RBTreeEqual(const void *a, const void *b);
void RBTreeDestroy(void *rb_tree);

bool RBTreePut(RBTree *tree, const void *key, const void *value);
void *RBTreeGet(const RBTree *tree, const void *key);
bool RBTreeRemove(RBTree *tree, const void *key);
void RBTreeClear(RBTree *tree);
size_t RBTreeSize(const RBTree *tree);

RBTreeIterator *RBTreeIteratorNew(const RBTree *tree);
bool RBTreeIteratorNext(RBTreeIterator *iter, void **key, void **value);
void RBTreeIteratorDestroy(void *_rb_iter);

#endif
