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
#include <rb-tree.h>

#include <alloc.h>

#include <assert.h>

typedef struct RBNode_ RBNode;

struct RBNode_
{
    void *key;
    void *value;
    bool red;
    RBNode *parent;
    RBNode *left;
    RBNode *right;
};

struct RBTree_
{
    void *(*KeyCopy)(const void *key);
    int (*KeyCompare)(const void *a, const void *b);
    void (*KeyDestroy)(void *key);

    void *(*ValueCopy)(const void *key);
    int (*ValueCompare)(const void *a, const void *b);
    void (*ValueDestroy)(void *key);

    struct RBNode_ *root;
    struct RBNode_ *nil;
    size_t size;
};

struct RBTreeIterator_
{
    const RBTree *tree;
    RBNode *curr;
};

static void PutFix_(RBTree *tree, RBNode *z);
static RBNode *Next_(const RBTree *tree, const RBNode *node);
static void VerifyTree_(RBTree *tree);

static int PointerCompare_(const void *a, const void *b)
{
    return a - b;
}

static void NoopDestroy_(ARG_UNUSED void *a)
{
    return;
}

static void *NoopCopy_(const void *a)
{
    return (void *)a;
}

static RBNode *NodeNew_(RBTree *tree, RBNode *parent, bool red, const void *key, const void *value)
{
    RBNode *node = xmalloc(sizeof(RBNode));

    node->parent = parent;
    node->red = red;
    node->key = tree->KeyCopy(key);
    node->value = tree->ValueCopy(value);
    node->left = tree->nil;
    node->right = tree->nil;

    return node;
}

static void NodeDestroy_(RBTree *tree, RBNode *node)
{
    if (node)
    {
        tree->KeyDestroy(node->key);
        tree->ValueDestroy(node->value);
        free(node);
    }
}

static void Reset_(RBTree *tree)
{
    tree->nil->key = tree->nil->value = NULL;
    tree->nil->red = false;
    tree->nil->parent = tree->nil->left = tree->nil->right = tree->nil;

    tree->root->key = tree->root->value = NULL;
    tree->root->red = false;
    tree->root->parent = tree->root->left = tree->root->right = tree->nil;

    tree->size = 0;
}

RBTree *RBTreeNew(void *(*KeyCopy)(const void *key),
                  int (*KeyCompare)(const void *a, const void *b),
                  void (*KeyDestroy)(void *key),
                  void *(*ValueCopy)(const void *key),
                  int (*ValueCompare)(const void *a, const void *b),
                  void (*ValueDestroy)(void *key))
{
    assert(!(KeyCopy && KeyDestroy) || (KeyCopy && KeyDestroy));
    assert(!(ValueCopy && ValueDestroy) || (ValueCopy && ValueDestroy));

    RBTree *t = xmalloc(sizeof(RBTree));

    t->KeyCopy = KeyCopy ? KeyCopy : NoopCopy_;
    t->KeyCompare = KeyCompare ? KeyCompare : PointerCompare_;
    t->KeyDestroy = KeyDestroy ? KeyDestroy : NoopDestroy_;

    t->ValueCopy = ValueCopy ? ValueCopy : NoopCopy_;
    t->ValueCompare = ValueCompare ? ValueCompare : PointerCompare_;
    t->ValueDestroy = ValueDestroy ? ValueDestroy : NoopDestroy_;

    t->nil = xcalloc(1, sizeof(RBNode));
    t->root = xcalloc(1, sizeof(RBNode));

    Reset_(t);

    return t;
}

static void TreeDestroy_(RBTree *tree, RBNode *x)
{
    if (x != tree->nil)
    {
        TreeDestroy_(tree, x->left);
        TreeDestroy_(tree, x->right);
        NodeDestroy_(tree, x);
    }
}

RBTree *RBTreeCopy(const RBTree *tree, RBTreePredicate *filter, void *user_data)
{
    RBNode **nodes = xmalloc(tree->size * sizeof(RBNode *));
    size_t node_count = 0;

    {
        RBTreeIterator *iter = NULL;
        for (iter = RBTreeIteratorNew(tree); iter->curr != iter->tree->nil; iter->curr = Next_(iter->tree, iter->curr))
        {
            if (!filter || filter(iter->curr->key, iter->curr->value, user_data))
            {
                nodes[node_count] = iter->curr;
                node_count++;
            }
        }
        RBTreeIteratorDestroy(iter);
    }

    RBTree *copy = RBTreeNew(tree->KeyCopy, tree->KeyCompare, tree->KeyDestroy,
                             tree->ValueCopy, tree->ValueCompare, tree->ValueDestroy);

    RBNode *node = NULL;
    // [0, 1, 2, 3, 4]
    if ((node_count % 2) != 0)
    {
        node = nodes[node_count / 2];
        RBTreePut(copy, node->key, node->value);
        node_count--;
    }
    else
    {
        node = copy->root;
    }

    assert((node_count % 2) == 0);

    // [0, 1, 2, 3]
    for (size_t i = 0; i < (node_count / 2); i += 1)
    {
        node = nodes[(node_count / 2) + i];
        RBTreePut(copy, node->key, node->value);

        node = nodes[(node_count / 2) - i - 1];
        RBTreePut(copy, node->key, node->value);
    }

    free(nodes);

    VerifyTree_(copy);

    return copy;
}

bool RBTreeEqual(const void *_a, const void *_b)
{
    const RBTree *a = _a, *b = _b;

    if (a == b)
    {
        return true;
    }
    if (a == NULL || b == NULL)
    {
        return false;
    }
    if (a->KeyCompare != b->KeyCompare || a->ValueCompare != b->ValueCompare)
    {
        return false;
    }
    if (RBTreeSize(a) != RBTreeSize(b))
    {
        return false;
    }

    RBTreeIterator *it_a = RBTreeIteratorNew(a);
    RBTreeIterator *it_b = RBTreeIteratorNew(b);

    void *a_key, *a_val, *b_key, *b_val;
    while (RBTreeIteratorNext(it_a, &a_key, &a_val)
           && RBTreeIteratorNext(it_b, &b_key, &b_val))
    {
        if (a->KeyCompare(a_key, b_key) != 0
            || b->ValueCompare(a_val, b_val))
        {
            RBTreeIteratorDestroy(it_a);
            RBTreeIteratorDestroy(it_b);
            return false;
        }
    }

    RBTreeIteratorDestroy(it_a);
    RBTreeIteratorDestroy(it_b);
    return true;

}

void RBTreeDestroy(void *rb_tree)
{
    RBTree *tree = rb_tree;
    if (tree)
    {
        TreeDestroy_(tree, tree->root->left);
        free(tree->root);
        free(tree->nil);
        free(tree);
    }
}

static void RotateLeft_(RBTree *tree, RBNode *x)
{
    assert(!tree->nil->red);

    RBNode *y = x->right;
    x->right = y->left;

    if (y->left != tree->nil)
    {
        y->left->parent = x;
    }

    y->parent = x->parent;

    if (x == x->parent->left)
    {
        x->parent->left = y;
    }
    else
    {
        x->parent->right = y;
    }

    y->left = x;
    x->parent = y;

    assert(!tree->nil->red);
}

static void RotateRight_(RBTree *tree, RBNode *y)
{
    assert(!tree->nil->red);

    RBNode *x = y->left;
    y->left = x->right;

    if (x->right != tree->nil)
    {
        x->right->parent = y;
    }

    x->parent = y->parent;

    if (y == y->parent->left)
    {
        y->parent->left = x;
    }
    else
    {
        y->parent->right = x;
    }

    x->right = y;
    y->parent = x;

    assert(!tree->nil->red);
}

typedef struct
{
    bool replaced;
    RBNode *node;
} InsertResult_;

static void PutFix_(RBTree *tree, RBNode *z)
{
    while (z->parent->red)
    {
        if (z->parent == z->parent->parent->left)
        {
            RBNode *y = z->parent->parent->right;
            if (y->red)
            {
                // case 1
                z->parent->red = false;
                y->red = false;
                z->parent->parent->red = true;
                z = z->parent->parent;
            }
            else
            {
                if (z == z->parent->right)
                {
                    // case 2
                    z = z->parent;
                    RotateLeft_(tree, z);
                }

                // case 3
                z->parent->red = false;
                z->parent->parent->red = true;
                RotateRight_(tree, z->parent->parent);
            }
        }
        else
        {
            RBNode *y = z->parent->parent->left;
            if (y->red)
            {
                // case 1
                z->parent->red = false;
                y->red = false;
                z->parent->parent->red = true;
                z = z->parent->parent;
            }
            else
            {
                if (z == z->parent->left)
                {
                    // case 2
                    z = z->parent;
                    RotateRight_(tree, z);
                }

                // case 3
                z->parent->red = false;
                z->parent->parent->red = true;
                RotateLeft_(tree, z->parent->parent);
            }
        }
    }

    tree->root->left->red = false;

    assert(!tree->nil->red);
    assert(!tree->root->red);
}


bool RBTreePut(RBTree *tree, const void *key, const void *value)
{
    RBNode *y = tree->root;
    RBNode *x = tree->root->left;

    while (x != tree->nil)
    {
        y = x;
        int cmp = tree->KeyCompare(key, x->key);
        if (cmp == 0)
        {
            tree->KeyDestroy(x->key);
            x->key = tree->KeyCopy(key);
            tree->ValueDestroy(x->value);
            x->value = tree->ValueCopy(value);
            return true;
        }
        x = (cmp < 0) ? x->left : x->right;
    }

    RBNode *z = NodeNew_(tree, y, true, key, value);

    if (y == tree->root || tree->KeyCompare(z->key, y->key) < 0)
    {
        y->left = z;
    }
    else
    {
        y->right = z;
    }

    PutFix_(tree, z);
    tree->size++;

    return false;
}

static RBNode *Next_(const RBTree *tree, const RBNode *node)
{
    if (node->right != tree->nil)
    {
        RBNode *curr;
        for (curr = node->right; curr->left != tree->nil; curr = curr->left);
        return curr;
    }
    else
    {
        RBNode *curr;
        for (curr = node->parent; node == curr->right; node = curr, curr = curr->parent);
        return (curr != tree->root) ? curr : tree->nil;
    }
}

static RBNode *Get_(const RBTree *tree, const void *key)
{
    assert(!tree->nil->red);
    RBNode *curr = tree->root->left;

    while (curr != tree->nil)
    {
        int cmp = tree->KeyCompare(key, curr->key);
        if (cmp == 0)
        {
            return curr;
        }
        else if (cmp < 0)
        {
            curr = curr->left;
        }
        else
        {
            curr = curr->right;
        }
    }

    assert(!tree->nil->red);
    return curr;
}

void *RBTreeGet(const RBTree *tree, const void *key)
{
    RBNode *node = Get_(tree, key);
    return node != tree->nil ? node->value : NULL;
}


void RemoveFix_(RBTree *tree, RBNode *x)
{
    assert(!tree->nil->red);

    RBNode *root = tree->root->left;
    RBNode *w;

    while (x != root && !x->red)
    {
        if (x == x->parent->left)
        {
            w = x->parent->right;
            if (w->red)
            {
                w->red = false;
                x->parent->red = true;
                RotateLeft_(tree, x->parent);
                w = x->parent->right;
            }

            if (!w->left->red && !w->right->red)
            {
                w->red = true;
                x = x->parent;
            }
            else
            {
                if (!w->right->red)
                {
                    w->left->red = false;
                    w->red = true;
                    RotateRight_(tree, w);
                    w = x->parent->right;
                }
                w->red = x->parent->red;
                x->parent->red = false;
                w->right->red = false;
                RotateLeft_(tree, x->parent);
                x = root;
            }
        }
        else
        {
            w = x->parent->left;
            if (w->red)
            {
                w->red = false;
                x->parent->red = true;
                RotateRight_(tree, x->parent);
                w = x->parent->left;
            }

            if (!w->left->red && !w->right->red)
            {
                w->red = true;
                x = x->parent;
            }
            else
            {
                if (!w->left->red)
                {
                    w->right->red = false;
                    w->red = true;
                    RotateLeft_(tree, w);
                    w = x->parent->left;
                }

                w->red = x->parent->red;
                x->parent->red = false;
                w->left->red = false;
                RotateRight_(tree, x->parent);
                x = root;
            }
        }
    }

    x->red = false;
    assert(!tree->nil->red);
}

bool RBTreeRemove(RBTree *tree, const void *key)
{
    assert(!tree->nil->red);

    RBNode *z = Get_(tree, key);
    if (z == tree->nil)
    {
        return false;
    }

    RBNode *y = ((z->left == tree->nil) || (z->right == tree->nil)) ? z : Next_(tree, z);
    RBNode *x = (y->left == tree->nil) ? y->right : y->left;

    x->parent = y->parent;
    if (tree->root == x->parent)
    {
        tree->root->left = x;
    }
    else
    {
        if (y == y->parent->left)
        {
            y->parent->left = x;
        }
        else
        {
            y->parent->right = x;
        }
    }

    if (z != y)
    {
        assert(y != tree->nil);
        assert(!tree->nil->red);

        if (!y->red)
        {
            RemoveFix_(tree, x);
        }

        y->left = z->left;
        y->right = z->right;
        y->parent = z->parent;
        y->red = z->red;
        z->left->parent = y;
        z->right->parent = y;

        if (z == z->parent->left)
        {
            z->parent->left = y;
        }
        else
        {
            z->parent->right = y;
        }
        NodeDestroy_(tree, z);
    }
    else
    {
        if (!y->red)
        {
            RemoveFix_(tree, x);
        }
        NodeDestroy_(tree, y);
    }

    assert(!tree->nil->red);

    tree->size--;
    return true;
}

void ClearRecursive_(RBTree *tree, RBNode *node)
{
    if (node == tree->nil)
    {
        return;
    }

    ClearRecursive_(tree, node->left);
    ClearRecursive_(tree, node->right);

    NodeDestroy_(tree, node);
}

void RBTreeClear(RBTree *tree)
{
    assert(tree);

    ClearRecursive_(tree, tree->root);
    tree->root = xcalloc(1, sizeof(RBNode));

    Reset_(tree);
}

size_t RBTreeSize(const RBTree *tree)
{
    return tree->size;
}

RBTreeIterator *RBTreeIteratorNew(const RBTree *tree)
{
    RBTreeIterator *iter = xmalloc(sizeof(RBTreeIterator));

    iter->tree = tree;
    for (iter->curr = iter->tree->root; iter->curr->left != tree->nil; iter->curr = iter->curr->left);

    return iter;
}

bool Peek_(RBTreeIterator *iter, void **key, void **value)
{
    if (iter->tree->size == 0)
    {
        return false;
    }

    if (iter->curr == iter->tree->nil)
    {
        return false;
    }

    if (key)
    {
        *key = iter->curr->key;
    }

    if (value)
    {
        *value = iter->curr->value;
    }

    return true;
}

bool RBTreeIteratorNext(RBTreeIterator *iter, void **key, void **value)
{
    if (Peek_(iter, key, value))
    {
        iter->curr = Next_(iter->tree, iter->curr);
        return true;
    }
    else
    {
        return false;
    }
}

void RBTreeIteratorDestroy(void *_rb_iter)
{
    free(_rb_iter);
}

static void VerifyNode_(RBTree *tree, RBNode *node, int black_count, int *path_black_count)
{
    if (node->red)
    {
        assert(!node->left->red);
        assert(!node->right->red);
    }
    else
    {
        black_count++;
    }

    if (node == tree->nil)
    {
        assert(!node->red);
        if ((*path_black_count) == -1)
        {
            *path_black_count = black_count;
        }
        else
        {
            assert(black_count == *path_black_count);
        }
    }
    else
    {
        VerifyNode_(tree, node->left, black_count, path_black_count);
        VerifyNode_(tree, node->right, black_count, path_black_count);
    }
}

static void VerifyTree_(RBTree *tree)
{
    assert(!tree->root->red);
    assert(!tree->root->key);
    assert(!tree->root->value);

    assert(!tree->nil->red);
    assert(!tree->nil->key);
    assert(!tree->nil->value);

    int path_black_count = -1;
    VerifyNode_(tree, tree->root->left, 0, &path_black_count);
}
