#ifndef CLOSURE_H
#define CLOSURE_H

#include <stdlib.h>
#include <utils/queue.h>

//old pointerat metadata representation
struct ptrat {  ///< store the location of persistent pointers in memory until the next cpoint
    void **ptr_loc;
    STAILQ_ENTRY(ptrat) list;
};

/* do not call this function directly */
void pointerat_aux(unsigned int cid, void **ptr_loc);

/* these functions must be called in this order */
void closure_init();
void build_mallocat_tree();
void classify_pointers(unsigned int cid);
void move_volatile_allocations(unsigned int cid);
void fix_back_references();
void store_peristent_pointers(unsigned int cid);

#define pointerat(cid, ptr_loc) do { \
    void **pp = (void**) ptr_loc; \
    pointerat_aux(cid, pp); \
} while (0)

void mallocat(void*, size_t);
void freeat(void*);

/* only for debugging */
void mallocat_pprint();

#endif /* end of include guard: CLOSURE_H */
