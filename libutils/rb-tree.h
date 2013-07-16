#ifndef CFENGINE_RB_TREE_H
#define CFENGINE_RB_TREE_H

#include "platform.h"

typedef struct RBTree_ RBTree;
typedef struct RBTreeIterator_ RBTreeIterator;

RBTree *RBTreeNew(void *(*KeyCopy)(const void *key),
                   int (*KeyCompare)(const void *a, const void *b),
                   void (*KeyDestroy)(void *key),
                   void *(*ValueCopy)(const void *key),
                   int (*ValueCompare)(const void *a, const void *b),
                   void (*ValueDestroy)(void *key));

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
