#include <assert.h>
#include "rbtree.h"

#include <stdio.h>

struct rbsettings ope = {0};

void rbsettings_set(void *(*_alloc)(size_t), void (*_free)(void*), size_t _node_size)
{
    ope.alloc = _alloc;
    ope.free = _free;
    ope.node_size = _node_size;
}

struct rbnode *rbnode_alloc(size_t k)
{
    assert(ope.alloc && "The allocation function has not been specified");
    struct rbnode *node = ope.alloc(ope.node_size);
    assert(node && "Failed to allocate memory for memory");
    return node;
}

int rbnode_cpm(struct rbnode *a, struct rbnode *b)
{
    if (a->key < b->key)
        return -1;
    if (a->key > b->key)
        return 1;
    return 0;
}


void rbroot_print(struct rbroot *root)
{
    assert(root && "Invalid root pointer");
    struct rbnode *node;
    RB_FOREACH(node, root_struct, &root->root) {
        printf("node {key: '%lu', data: '%.10s...'} at %p\n", node->key, node->data, node);
    }
}

RB_GENERATE(root_struct, rbnode, node, rbnode_cpm);
