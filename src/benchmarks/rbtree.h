#ifndef RBTREE_H
#define RBTREE_H

#include <stdint.h>
#include <stdlib.h>

#include "tree.h"
//#define RBTREE_TPL_FILE "/mnt/pmfs/rbtree.tpl"
#define RBTREE_TPL_FILE "/tmp/rbtree.tpl"

struct rbnode {
    RB_ENTRY(rbnode) node;
    uint64_t key;
    char data[1];
};

struct rbroot {
    uint64_t node_cnt;
    int node_size;
    RB_HEAD(root_struct, rbnode) root;
};

struct rbsettings {
    void* (*alloc)(size_t);
    void (*free)(void *);
    int node_size;
};

#define rbnode_datalen(nodesize) ((nodesize) - sizeof(struct rbnode))

void rbsettings_set(void *(*_alloc)(size_t), void (*_free)(void*), size_t);
struct rbnode *rbnode_alloc(size_t k);

void rbroot_print(struct rbroot *root);

RB_PROTOTYPE(root_struct, rbnode, node, rbnode_cpm);

#endif /* end of include guard: RBTREE_H */
