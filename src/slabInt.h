#ifndef SLABINT_H_VRLG1ZSX
#define SLABINT_H_VRLG1ZSX

#include <stdio.h>

#include <tree.h>
#include <queue.h>
#include <bitstring.h>
#include <fixptr.h>
#include <vector.h>
#include "settings.h"

/* When the target of a persistent pointer is NULL, we use these values as its metadata */
#define SLAB_PTR_SEID_NULL      0xffffffff
#define SLAB_PTR_OFFSET_NULL    0xffff

/*
 * slab_ptr stores metadata about persistent pointers.
 * This metadata is use to fix the pointers after restoring a container
 */
#define SLAB_PTR_SIZE   (PAGE_SIZE / (sizeof(uint16_t) * 2 + sizeof(uint32_t)))
struct slab_ptr {
    struct {
        uint16_t ploc_offset;
        uint32_t pval_seid;
        uint16_t pval_offset;
    } ptrs[SLAB_PTR_SIZE];
};

#define SLAB_LARGE_ALLOC 512

/*
 * These macros return a pointer to the in-use bitmap.
 * Based on se_size, we use the bitmap defined in current/snapshot anonymous
 * struct or we place it on the first bytes of the data page.
 */
#define SLAB_ENTRY_CBITMAP(se) \
        ((se)->se_size >= SLAB_LARGE_ALLOC) ? \
        (se)->se_data.current.bitmap : \
        (se)->se_data.current.maddr

#define SLAB_ENTRY_SBITMAP(se) \
        ((se)->se_size >= SLAB_LARGE_ALLOC) ? \
        (se)->se_data.snapshot.bitmap : \
        (se)->se_data.snapshot.maddr

/*
 * This macro computes an approximate capacity of a data page
 */
#define SLAB_ENTRY_CAPACITY(se) \
        ((se)->se_size < SLAB_LARGE_ALLOC) ? \
            ((PAGE_SIZE - bitstr_size((se)->se_size)) / (se)->se_size) : \
            (PAGE_SIZE / (se)->se_size)

/*
 * Get the address of the first object in the data page
 */
#define SLAB_ENTRY_DATAOFFSET(se) \
        ((se)->se_size >= SLAB_LARGE_ALLOC) ? 0 : \
    bitstr_size(PAGE_SIZE / (se)->se_size)

/*
 * slab_entry describes data pages.
 * It contains pointers to both current and snapshot versions of a data page
 * along with other metaata needed to manage a data page
 */
#define SLAB_BM_SIZE    (PAGE_SIZE / 8)
struct slab_entry {
    unsigned int se_id;
    unsigned int se_size;        ///< size (bytes) of the persistent allocation
    struct {
        struct {
            void *maddr;
            size_t laddr;
            bitstr_t bitmap[1];
        } current;
        struct {
            void *maddr;
            size_t laddr;
            bitstr_t bitmap[1];
        } snapshot;
    } se_data;
    struct {
        struct {
            uint16_t idx;
            struct slab_ptr *maddr;
            size_t laddr;
        } current;
        struct {
            uint16_t idx;
            struct slab_ptr *maddr;
            size_t laddr;
        } snapshot;
    } se_ptr;
    //TODO: make these two index part of a union
    STAILQ_ENTRY(slab_entry) se_list; ///< indexed as a empty (uninitialized) entry
    RB_ENTRY(slab_entry) se_splay; ///< indexed as a non empty entry (separate trees for full and non-full entries)
};

/*
 * slab_entry_size is use to group slab_entry(s) container persistent obj(s) a
 * equal size.
 *
 * The slab_entry(s) are ordered by size and kept in a list where
 * the head of the list is checked first when allocation a new obj. If the head
 * is full, then all others slab_entry(s) are also full and a new one needs to
 * be created and init.
 */
struct slab_entry_size {
    unsigned int es_size;
    STAILQ_HEAD(list, slab_entry) es_list;
    RB_ENTRY(slab_entry_size) es_splay;
};

/*
 * SLAB_ENTRY_IS_INIT checks if the slab_entry pointed by pse is initialized
 * TODO: add consistency check to this macro
 */
#define SLAB_ENTRY_IS_INIT(pse) \
    ((pse)->se_size || (pse)->se_data.current.maddr)

#define SLAB_ENTRY_INDEX(pse) \
    ((ptoi((pse)) - ROUND_DWNPG(ptoi((pse)))) / sizeof(struct slab_entry))

/*
 * slab_bucket packs as many slab_entry(s) as possible wihtin a page
 */
#define SLAB_BUCKET_ENTRIES ((PAGE_SIZE - sizeof(int)) / sizeof(struct slab_entry))
struct slab_bucket {
    int sb_has_snapshot;
    unsigned int sb_id; ///< to calculate the index of the inner, outer, and dir for this bucket
    struct slab_entry sb_entries[SLAB_BUCKET_ENTRIES];
};

/*
 * slab_inner packs as many pointers to slab_bucket(s) as possiblw within a page
 */
#define SLAB_INNER_MEM      (2 * (sizeof(void*) + sizeof(size_t)))
#define SLAB_INNER_ENTRIES  ((PAGE_SIZE - sizeof(int)) / SLAB_INNER_MEM)
#define SLAB_INNER_FULL(p)  ((p)->si_index == SLAB_INNER_ENTRIES)

struct slab_inner {
    unsigned int si_index;
    struct {
        struct slab_bucket *maddr;
        size_t laddr;
    } si_current[SLAB_INNER_ENTRIES];
    struct {
        struct slab_bucket *maddr;
        size_t laddr;
    } si_snapshot[SLAB_INNER_ENTRIES];
};

/*
 * slab_outer packs as many pointers to slab_inner(s) as possible within a page
 */
#define SLAB_OUTER_MEM      (2 * (sizeof(void*) + sizeof(size_t)))
#define SLAB_OUTER_ENTRIES  ((PAGE_SIZE - sizeof(int)) / SLAB_OUTER_MEM)
#define SLAB_OUTER_FULL(p)  ((p)->so_index == SLAB_OUTER_ENTRIES)

struct slab_outer {
    unsigned int so_index;
    struct {
        struct slab_inner *maddr;
        size_t laddr;
    } so_current[SLAB_OUTER_ENTRIES];
    struct {
        struct slab_inner *maddr;
        size_t laddr;
    } so_snapshot[SLAB_OUTER_ENTRIES];
};

/*
 * slab_dir is the top level of the tree
 * It packs pointers to slab_outer(s) as well as metadata to index
 * slab_entry(s) and a free list.
 * This struct is adjusted to consume one page
 */
#define SLAB_DIR_MEMSIZE    (sizeof(int) +          /* sd_index */ \
                                3 * sizeof(void*) + /* sd_maddr_root */ \
                                3 * sizeof(void*) + /* sd_size_root */ \
                                2 * sizeof(void*) + /* sd_free_list */ \
                                sizeof(struct pptr_head) + /* sd_ptr_list_head */ \
                                sizeof(uint64_t))   /* sd_cont_root */
#define SLAB_DIR_ENTRIES    ((PAGE_SIZE - SLAB_DIR_MEMSIZE) / \
                                (2 * (sizeof(void*) + sizeof(size_t))))
#define SLAB_DIR_FULL(p)    ((p)->sd_index == SLAB_DIR_ENTRIES)

struct slab_dir {
    unsigned int sd_index;   ///< next available index in the arrays
    RB_HEAD(used_slab_entry_tree, slab_entry) sd_maddr_root;
    RB_HEAD(sizes_slab_entry_tree, slab_entry_size) sd_size_root;
    STAILQ_HEAD(slab_entry_free_list, slab_entry) sd_free_list;
    //struct pptr_head sd_ptr_list_head;
    uint64_t sd_cont_root;
    struct {
        struct slab_outer *maddr;
        size_t laddr;
    } sd_current[SLAB_DIR_ENTRIES];
    struct {
        struct slab_outer *maddr;
        size_t laddr;
    } sd_snapshot[SLAB_DIR_ENTRIES];
    VECTOR_DECL(se_vector, struct slab_entry*) sd_vector;
};

/* Generating function prototypes for rb-trees */
int slab_entry_compare_by_maddr(struct slab_entry *a, struct slab_entry *b);
RB_PROTOTYPE(used_slab_entry_tree, slab_entry, se_splay, slab_entry_compare_by_maddr);

int slab_entry_size_compare_by_size(struct slab_entry_size *a, struct slab_entry_size *b);
RB_PROTOTYPE(sizes_slab_entry_tree, slab_entry_size, es_splay, slab_entry_size_compare_by_size);

/* these macros are use to change the comparison function for the rb-trees */
#define SE_SEARCH_NORMAL  ((void*)1)
#define SE_SEARCH_MADDR   ((void*)2)
#define SLAB_ENTRY_SEARCH_KEY(key, type) \
    { .se_data.current.maddr = (key), .se_data.snapshot.maddr = type }
#define SLAB_ENTRY_SEARCH_TYPE(se) ((se)->se_data.snapshot.maddr)

/* utility functions */
struct slab_entry_size *slab_entry_size_init(int size);
struct slab_entry *slab_find(unsigned int cid, void *maddr);
int slab_entry_full(struct slab_entry *se);
struct slab_outer* slab_outer_init(unsigned int cid, size_t *laddr);
struct slab_inner* slab_inner_init(unsigned int cid, size_t *laddr);
struct slab_bucket* slab_bucket_init(unsigned int cid, size_t *laddr);
int slab_entry_init(unsigned int cid, struct slab_entry *se, int size);

/*
 * snapshot functions
 */
void slab_entry_snapshot(unsigned int cid, struct slab_entry *se);
int slab_bucket_snapshot(unsigned int cid, struct slab_bucket *sb);
struct slab_inner* slab_inner_snapshot(unsigned int cid, struct slab_inner *si, size_t *laddr);
struct slab_outer* slab_outer_snapshot(unsigned int cid, struct slab_outer *so, size_t *laddr);
struct slab_dir* slab_dir_snapshot(unsigned int cid, struct slab_dir *sd, size_t *laddr);
void slab_foreach_snapshot_entry(unsigned int cid, void (*fun)(struct slab_entry *se, void *param), void *param);
void slab_entry_copynswap(unsigned int cid, struct slab_entry *se);
int slab_bucket_copynswap(unsigned int cid, struct slab_bucket *sb);

/* only for debugging */
void print_splay_tree();
void slab_entry_print(unsigned int cid);
void slab_dir_pprint(struct slab_dir *sd, int level);

#endif /* end of include guard: SLABINT_H_VRLG1ZSX */
