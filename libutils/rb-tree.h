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
