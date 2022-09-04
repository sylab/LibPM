#ifndef CONT_H
#define CONT_H

//#include <assert.h>

//#include "settings.h"

#include "page_alloc.h"
#include "fixptr.h"
#include "closure.h"

#define CPOINT_IN_PROGRESS_BIT 0
#define CFLAG_CPOINT_IN_PROGRESS    (1 << CPOINT_IN_PROGRESS_BIT)

struct container {
    unsigned int id;
    struct page_allocator *pg_allocator;
    struct {
        struct slab_dir *maddr;
        size_t laddr;
    } current_slab;
    struct {
        struct slab_dir *maddr;
        size_t laddr;
    } snapshot_slab;
    unsigned char flags;
    //STAILQ_HEAD(ptrat_list, ptrat) ptrat_head; ///< keep all ptrs from pointerat to be added at cpoint
};

struct container *container_init();
void *container_palloc(unsigned int cid, unsigned int size);
void container_cpoint(unsigned int cid);
struct container* container_restore(unsigned int cid);

size_t container_setroot(unsigned int cid, void *maddr);
void *container_getroot(unsigned int cid);

void container_pprint();

#endif /* end of include guard: CONT_H */
