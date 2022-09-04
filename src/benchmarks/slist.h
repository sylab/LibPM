#ifndef SLIST_H
#define SLIST_H

#include "queue.h"

//#define SLIST_TPL_FILE "/mnt/pmfs/slist.tpl"
#define SLIST_TPL_FILE "/tmp/slist.tpl"

struct slist_node {
    STAILQ_ENTRY(slist_node) node;
    char data[1];
};

#define slist_datalen(nodesize) ((nodesize) - sizeof(struct slist_node))

struct slist_head {
    int node_size;
    uint64_t node_cnt;
    STAILQ_HEAD(struct_list_head, slist_node) head;
};

#endif /* end of include guard: SLIST_H */
