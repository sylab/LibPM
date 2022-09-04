#include <utils/macros.h>
#include <utils/tree.h>
#include <utils/vector.h>
#include <assert.h>

#include "cont.h"
#include "closure.h"
#include "htable.h"
#include "slab.h"
#include "slabInt.h"
#include "atomics.h"

#define PTR_REBASE(base, ploc, newbase) ((newbase) + ((ploc) - (base)))

//TODO: keep track of some statistics
struct closure_stats {
    uint64_t mallocat;
    uint64_t pointerat;
};

/*
 * 1. init subsystem
 * 2. build tree of mallocat
 * 3. classify the pointers
 *      a. part of malloc
 *      b. persistent
 *      c. back reference
 * 4. walk the vector of persistent pointers and start moving allocations to PM
 * 5. walk the dirty slab_entries and check the target of their pptrs
 * 6. fix back-reference pointers, which are the ones left in the hashtable
 *
 */

struct mallocat_entry {
    void *addr; //< original volatile address of the allocation
    size_t size; //< size of the allocation
    void *paddr; //< if not NULL, it points to the new persistent address
    SPLAY_ENTRY(mallocat_entry) splay;
    VECTOR_DECL(mallocat_pointers, void*) ptrs;
};

struct htable *ht_pointerat = NULL;
struct htable *ht_mallocat = NULL;
VECTOR_DECL(struct_pptr_vec, void*) pptr_vec;
SPLAY_HEAD(mallocat_tree, mallocat_entry) mallocat_tree = SPLAY_INITIALIZER();

void pointerat_aux(unsigned int cid, void **ptr_loc)
{
#if 0 //old implementation
    struct container *cont = get_container(cid);
    struct ptrat * pat = malloc(sizeof(*pat));
    if (!pat)
        handle_error("failed to allocate memory for pointerat at");

    pat->ptr_loc = ptr_loc;
    STAILQ_INSERT_TAIL(&cont->ptrat_head, pat, list);
#endif
    htable_insert(ht_pointerat, ptr_loc, ptr_loc);
}

void mallocat(void* addr, size_t size)
{
#if 0
    struct mallocat_entry key = { .addr = addr };
    struct mallocat_entry *e;

    // check for duplicates
    e = SPLAY_FIND(mallocat_tree, &mallocat_tree, &key);
    if (e) {
    } else {
        e = mallocat_entry_alloc(addr, size);
        SPLAY_INSERT(mallocat_tree, &mallocat_tree, e);
    }
#endif
    assert(addr && "registering a failed memory allocation");
    htable_insert(ht_mallocat, addr, itop(size));
}

void freeat(void *addr)
{
    handle_error("not implemented yet");
}

int mallocat_entry_cpm(struct mallocat_entry *a, struct mallocat_entry *b)
{
    return (a->addr < b->addr) ?
            -1 :
            a->addr >= (b->addr + b->size);
}

SPLAY_PROTOTYPE(mallocat_tree, mallocat_entry, splay, mallocat_entry_cpm);
SPLAY_GENERATE(mallocat_tree, mallocat_entry, splay, mallocat_entry_cpm);

static struct mallocat_entry *mallocat_entry_alloc(void *addr, size_t size)
{
    struct mallocat_entry *e;
    e = malloc(sizeof(*e));
    assert(e && "failed to allocate mallocat_entry");

    e->addr = addr;
    e->size = size;
    e->paddr = NULL;
    VECTOR_INIT(&e->ptrs);

    return e;
}

static void init_mallocat_tree_callback(void *key, void *val, void *_not_use_)
{
    void *addr = key;
    size_t size = ptoi(val);
    struct mallocat_entry *e = mallocat_entry_alloc(addr, size);
    SPLAY_INSERT(mallocat_tree, &mallocat_tree, e);
}

/**
 * Create a mallocat_entry for every element of the mallocat hashtable.
 * The hashtable guarantees that there are not duplicate entries
 */
void build_mallocat_tree()
{
    htable_foreach(ht_mallocat, init_mallocat_tree_callback, NULL);
}

void empty_mallocat_tree()
{
    while (SPLAY_EMPTY(&mallocat_tree)) {
        struct mallocat_entry *root = SPLAY_ROOT(&mallocat_tree);
        SPLAY_REMOVE(mallocat_tree, &mallocat_tree, root);
    }
}

/*
 * We classify pointers in 3 groups based on its location:
 *  1. volatile: these are associated with mallocat structs
 *  2. persistent: these reside on persistent memory
 *  3. back-reference: these reside somewhere in volatile memory and point
 *      to memory that was moved from volatile to persistent. These remain
 *      in the hashtable.
 *  Pointers that are classified as 2. are removed from the hashtable.
 */
static int classify_pointers_callback(void *key, void *val, void *param)
{
    void **ptr_loc = (void**)key;
    uint32_t cid = (uint32_t)param;

    /*
     * first check on the volatile allocations, but don't remove because this
     * may be a back reference. This pointer can be remove once the allocation
     * it belongs to has been moved to the continaer
     */
    struct mallocat_entry k = { .addr = ptr_loc };
    struct mallocat_entry *e = SPLAY_FIND(mallocat_tree, &mallocat_tree, &k);
    if (e) {
        VECTOR_APPEND(&e->ptrs, ptr_loc);
        return 0;
    }

    /* second on the persistent data pages */
    struct slab_entry *se;
    if ((se = slab_find(cid, ptr_loc))) {
        /*
         * these ptrs don't have a persistent target yet, so they cannot be
         * to the slab_ptr just yet. We do that after we move all volatile
         * allocations
         */
        VECTOR_APPEND(&pptr_vec, ptr_loc);
        return 1;
    }

    /* most likely this is back reference pointer which needs fixing later */
    return 0;
}

void classify_pointers(unsigned int cid)
{
    htable_filter(ht_pointerat, classify_pointers_callback, itop(cid));
}

/*
 * Check the target of every persistent pointer and if it falls into a
 * registered volatile allocation, move the allocations to the container.
 *
 * Multiple persistent pointer may target the same allocation (e.g. mutile
 * objects pointing to the same object), but the allocation is moved only once
 * with the first of these pointers. Then all pointers are updated to point to
 * the new address in the container.
 *
 * If the allocation has pointers associated with it, then add the pointers to
 * the vector of persistent pointer to continue the discovery.
 */
static void persist_valloc(unsigned int cid, void **ptr_loc)
{
    struct mallocat_entry key = { .addr = *ptr_loc };
    struct mallocat_entry *e = SPLAY_FIND(mallocat_tree, &mallocat_tree, &key);

    if (e) {
        if (!e->paddr) {
            /* now we move this allocation to the container */
            e->paddr = container_palloc(cid, e->size);
            pmemcpy(e->paddr, e->addr, e->size);
            free(e->addr);

            /* now we move every pointer to the vector of persistent ptrs */
            void *ptr; int i;
            VECTOR_FOREACH(&e->ptrs, ptr, i) {
                VECTOR_APPEND(&pptr_vec, PTR_REBASE(e->addr, ptr, e->paddr));
                htable_remove(ht_pointerat, ptr);
            }
        }
        /* update the targe of the pointer */
        *ptr_loc = PTR_REBASE(e->addr, *ptr_loc, e->paddr);
    } else {
        /**
         * the target of this persistent pointer is not a registered
         * volatile allocation, so nothing needs to be done
         */
    }
}

static void move_volatile_allocations_callback(struct slab_entry *se_loc, void *param)
{
    struct slab_ptr *sp = se_loc->se_ptr.current.maddr;
    struct slab_entry *se_val;
    unsigned int cid = ptoi(param);

    for (int i = 0; i < se_loc->se_ptr.current.idx; i++) {
        void **ptr_loc = se_loc->se_data.current.maddr + sp->ptrs[i].ploc_offset;
        persist_valloc(cid, ptr_loc);
    }
}

/*
 * Check the target of every persistent ptr and if it falls within a registered
 * volatile allocation, move the allocation and its ptrs to the container
 *
 * There are two sets of persistent ptrs we need to check:
 *  a) recently registered persistent ptrs whose target are volatile. These
 *     reside in the pptr_vec data strcuture.
 *  b) current persistent ptrs in the slab. We don't check all of then, but just
 *     the ones that are in `dirty` slab_entries
 */
void move_volatile_allocations(unsigned int cid)
{
    void *ptr_loc;

    for (int i = 0; i < VECTOR_SIZE(&pptr_vec); i++) {
        ptr_loc = VECTOR_AT(&pptr_vec, i);
        persist_valloc(cid, ptr_loc);
        slab_insert_pointer(cid, ptr_loc);
    }
    VECTOR_FREE(&pptr_vec);

    slab_foreach_snapshot_entry(cid, move_volatile_allocations_callback, itop(cid));
}

static void fix_back_references_callback(void *key, void *val, void *_not_use_)
{
    void **ptr_loc = key;

    if (*ptr_loc == NULL)
        return;

    struct mallocat_entry k = { .addr = *ptr_loc };
    struct mallocat_entry *e = SPLAY_FIND(mallocat_tree, &mallocat_tree, &k);
    if (e) {
        *ptr_loc = PTR_REBASE(e->addr, *ptr_loc, e->paddr);
    } else {
        printf("back-ref ptr: %p with unknown target %p\n", ptr_loc, *ptr_loc);
    }
}

void fix_back_references()
{
    htable_foreach(ht_pointerat, fix_back_references_callback, NULL);
}

void closure_init()
{
    ht_mallocat = htable_init();
    ht_pointerat = htable_init();
    VECTOR_INIT(&pptr_vec);
}

void store_peristent_pointers(unsigned int cid)
{
    int i;
    void *ptr_loc;
    VECTOR_FOREACH(&pptr_vec, ptr_loc, i) {
        printf("storing pptr %p\n", ptr_loc);
        slab_insert_pointer(cid, ptr_loc);
    }
}

/* Print the contents of the tree with mallocat structs
 * only for debugging */
void mallocat_pprint()
{
    struct mallocat_entry *e;
    int i;
    void *itr;

    SPLAY_FOREACH(e, mallocat_tree, &mallocat_tree) {
        printf("mallocat { addr: %p, size: %zu, "
               "ptrs: { size: %d, capacity: %d, ptrs: [",
               e->addr, e->size, e->ptrs.size, e->ptrs.capacity);
        VECTOR_FOREACH(&e->ptrs, itr, i)
            printf("%p, ", itr);
        printf(" ] } }\n");
    }
}

