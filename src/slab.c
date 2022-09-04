#include <assert.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>

#include "slab.h"
#include "slabInt.h"
#include "cont.h"
#include "page_alloc.h"
#include "macros.h"
#include "atomics.h"

#include "out.h"
#include "stats.h"

static unsigned int NEXT_SLAB_BUCKET_ID = 0;
static unsigned int NEXT_SLAB_ENTRY_ID = 0;

//TODO: this will need a lock
#define GET_NEXT_BUCKET_ID()            (NEXT_SLAB_BUCKET_ID++)
#define GET_NEXT_SLAB_ENTRY_ID()        (NEXT_SLAB_ENTRY_ID++)

#define OFFSET_SIZE_BITS    (sizeof(uint16_t) << 3)

#define PACK_CONT_ROOT(pv, seid, offset) do { \
    *(pv) = (uint64_t)(seid); \
    *(pv) = ((uint64_t)(*pv)) << OFFSET_SIZE_BITS; \
    *(pv) |= (uint64_t)(offset); \
} while(0)

#define UNPACK_CONT_ROOT(v, pseid, poffset) do { \
    *(poffset) = (uint16_t)v; \
    *(pseid) = (unsigned int)(v >> OFFSET_SIZE_BITS); \
} while(0)

RB_GENERATE(used_slab_entry_tree, slab_entry, se_splay, slab_entry_compare_by_maddr);
RB_GENERATE(sizes_slab_entry_tree, slab_entry_size, es_splay, slab_entry_size_compare_by_size);

/**
 * RB_PROTOTYPE does not allow us to use but one comparison function which
 * limit us to one type of search. To overcome this limitation, I we magic
 * values on other fields of a that tells us which type of comparison we need
 * to perform
 */
int slab_entry_compare_by_maddr(struct slab_entry *a, struct slab_entry *b)
{
    if (SLAB_ENTRY_SEARCH_TYPE(a) == SE_SEARCH_MADDR)
        return (a->se_data.current.maddr < b->se_data.current.maddr ?
                -1 :
                a->se_data.current.maddr >= b->se_data.current.maddr + PAGE_SIZE);
    else {
        return (a->se_data.current.maddr < b->se_data.current.maddr ?
                -1 :
                a->se_data.current.maddr > b->se_data.current.maddr);
    }

    return -1;
}

int slab_entry_size_compare_by_size(struct slab_entry_size *a, struct slab_entry_size *b)
{
    return (a->es_size < b->es_size ? -1 : a->es_size > b->es_size);
}

struct slab_entry_size *slab_entry_size_init(int size)
{
    struct slab_entry_size *es;
    es = calloc(1, sizeof(*es));
    es->es_size = size;
    STAILQ_INIT(&es->es_list);
    return es;
}

int slab_entry_init(unsigned int cid, struct slab_entry *se, int size)
{
    assert(se && "Invalid pointer");

    STATS_INC_SEINIT();

    se->se_data.current.maddr = page_allocator_getpage(cid, &se->se_data.current.laddr, PA_PROT_READ);
    se->se_data.snapshot.maddr = se->se_data.current.maddr;
    se->se_data.snapshot.laddr = se->se_data.current.laddr;
    se->se_ptr.current.idx = se->se_ptr.snapshot.idx = 0;
    se->se_ptr.current.maddr = se->se_ptr.snapshot.maddr = NULL;
    se->se_ptr.current.laddr = se->se_ptr.snapshot.laddr = 0;
    se->se_size = size;
    se->se_id = GET_NEXT_SLAB_ENTRY_ID();
    return 0;
}

struct slab_outer* slab_outer_init(unsigned int cid, size_t *laddr)
{
    struct slab_outer *so;
    STATS_INC_SOINIT();
    so = (struct slab_outer*) page_allocator_getpage(cid, laddr, PA_PROT_WRITE);
    return so;
}

struct slab_inner* slab_inner_init(unsigned int cid, size_t *laddr)
{
    struct slab_inner *si;
    STATS_INC_SIINIT();
    si = (struct slab_inner*) page_allocator_getpage(cid, laddr, PA_PROT_WRITE);
    return si;
}

struct slab_bucket* slab_bucket_init(unsigned int cid, size_t *laddr)
{
    struct slab_dir *sd;
    struct slab_bucket *sb;
    struct slab_entry *se;
    int i;

    STATS_INC_SBINIT();

    sd = get_container(cid)->current_slab.maddr;
    sb = (struct slab_bucket*)page_allocator_getpage(cid, laddr, PA_PROT_WRITE);
    memset(sb, 0, PAGE_SIZE);
    sb->sb_id = GET_NEXT_BUCKET_ID();

    for (i = 0; i < SLAB_BUCKET_ENTRIES; i++) {
        se = &sb->sb_entries[i];
        STAILQ_INSERT_TAIL(&sd->sd_free_list, se, se_list);
    }

    return sb;
}

struct slab_dir* slab_dir_init(unsigned int cid, size_t *laddr)
{
    struct slab_dir *sd = NULL;
    struct slab_outer *so = NULL;
    struct slab_inner *si = NULL;
    struct slab_bucket *sb = NULL;
    struct slab_entry *se = NULL;
    size_t so_laddr;
    size_t si_laddr;
    size_t sb_laddr;
    int i;

    sd = (struct slab_dir*) page_allocator_getpage(cid, laddr, PA_PROT_WRITE);
    so = slab_outer_init(cid, &so_laddr);
    si = slab_inner_init(cid, &si_laddr);
    sb = (struct slab_bucket*) page_allocator_getpage(cid, &sb_laddr, PA_PROT_WRITE);
    sb->sb_id = GET_NEXT_BUCKET_ID();

    if (sd == NULL || so == NULL || si == NULL || sb == NULL)
        handle_error("failed allocating memory for slab\n");

    sd->sd_current[sd->sd_index].maddr = so;
    sd->sd_current[sd->sd_index].laddr = so_laddr;
    sd->sd_snapshot[sd->sd_index].maddr = so;
    sd->sd_snapshot[sd->sd_index].laddr = so_laddr;
    sd->sd_index++;

    RB_INIT(&sd->sd_maddr_root);
    RB_INIT(&sd->sd_size_root);
    STAILQ_INIT(&sd->sd_free_list);

    so->so_current[so->so_index].maddr = si;
    so->so_current[so->so_index].laddr = si_laddr;
    so->so_snapshot[so->so_index].maddr = si;
    so->so_snapshot[so->so_index].laddr = si_laddr;
    so->so_index++;

    si->si_current[si->si_index].maddr = sb;
    si->si_current[si->si_index].laddr = sb_laddr;
    si->si_snapshot[si->si_index].maddr = sb;
    si->si_snapshot[si->si_index].laddr = sb_laddr;
    si->si_index++;

    for (i = 0; i < SLAB_BUCKET_ENTRIES; i++) {
        se = &sb->sb_entries[i];
        STAILQ_INSERT_TAIL(&sd->sd_free_list, se, se_list);
    }

    VECTOR_INIT(&sd->sd_vector);

    return sd;
}

void slab_mprotect_datapgs(unsigned int cid, int prot)
{
    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    struct slab_outer *so;
    struct slab_inner *si;
    struct slab_bucket *sb;
    struct slab_entry *se;
    int i, j, k, l;

    for (i = 0; i < sd->sd_index; i++) {
        so = sd->sd_current[i].maddr;
        for (j = 0; j < so->so_index; j++) {
            si = so->so_current[j].maddr;
            for (k = 0; k < si->si_index; k++) {
                sb = si->si_current[k].maddr;
                for (l = 0; l < SLAB_BUCKET_ENTRIES; l++) {
                    se = &sb->sb_entries[l];

                    if (!SLAB_ENTRY_IS_INIT(se))
                        continue;

                    //TODO: handle the case for allocation bigger than a page
                    page_allocator_mprotect(cid, se->se_data.current.maddr, PAGE_SIZE, prot);
                }
            }
        }
    }
}

static struct slab_entry *get_slab_entry_by_id(unsigned int cid, unsigned int seid)
{
    int sb_id = seid / SLAB_BUCKET_ENTRIES;
    int sb_idx = seid % SLAB_BUCKET_ENTRIES;
    int si_id = sb_id / SLAB_INNER_ENTRIES;
    int si_idx = sb_id % SLAB_INNER_ENTRIES;
    int so_id = si_id / SLAB_OUTER_ENTRIES;
    int so_idx = si_id % SLAB_OUTER_ENTRIES;
    int sd_idx = so_id % SLAB_DIR_ENTRIES;

    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    struct slab_outer *so = sd->sd_current[sd_idx].maddr;
    struct slab_inner *si = so->so_current[so_idx].maddr;
    struct slab_bucket *sb = si->si_current[si_idx].maddr;
    struct slab_entry *se = &sb->sb_entries[sb_idx];

    return se;
}

static void do_fixptrs(unsigned int cid)
{
    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    struct slab_entry *se_loc, *se_val;
    struct slab_outer *so;
    struct slab_inner *si;
    struct slab_bucket *sb;
    struct slab_ptr *sp;
    void **ptr_loc;

    for (int i = 0; i < sd->sd_index; i++) {
        so = sd->sd_current[i].maddr;
        for (int j = 0; j < so->so_index; j++) {
            si = so->so_current[j].maddr;
            for (int k = 0; k < si->si_index; k++) {
                sb = si->si_current[k].maddr;
                for (int l = 0; l < SLAB_BUCKET_ENTRIES; l++) {
                    se_loc = &sb->sb_entries[l];

                    if (!SLAB_ENTRY_IS_INIT(se_loc))
                        continue;

                    sp = se_loc->se_ptr.current.maddr;
                    for (int m = 0; m < se_loc->se_ptr.current.idx; m++) {
                        ptr_loc = se_loc->se_data.current.maddr + sp->ptrs[m].ploc_offset;

                        /* When the targe of the pointer is NULL, we use a special value for it */
                        if (sp->ptrs[m].pval_seid == SLAB_PTR_SEID_NULL &&
                                sp->ptrs[m].pval_offset == SLAB_PTR_OFFSET_NULL)
                            *ptr_loc = NULL;
                        else {
                            se_val = get_slab_entry_by_id(cid, sp->ptrs[m].pval_seid);
                            *ptr_loc = se_val->se_data.current.maddr + sp->ptrs[m].pval_offset;
                        }
                    }
                }
            }
        }
    }
}

static void dont_fixptrs(unsigned int cid) {}
static void (*Func_fixptrs)(unsigned int cid) = do_fixptrs;

void slab_fixptrs(unsigned int cid) { Func_fixptrs(cid); }

static void do_insert_pointer(unsigned int cid, void **ptr_loc)
{
    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    struct slab_entry *se_loc, *se_val = NULL;
    void *ptr_val = *ptr_loc;
    uint16_t ploc_offset = 0;
    uint16_t pval_offset = 0;

    assert(ptr_loc && "The location of the pointer must not be NULL");

    void *rounded_ptr_loc = itop(ROUND_DWNPG(ptoi(ptr_loc)));
    struct slab_entry key = SLAB_ENTRY_SEARCH_KEY(rounded_ptr_loc, SE_SEARCH_NORMAL);
    se_loc = RB_FIND(used_slab_entry_tree, &sd->sd_maddr_root, &key);
    if (se_loc) {
        ploc_offset = (ptoi(ptr_loc) - ROUND_DWNPG(ptoi(ptr_loc)));
        if (!se_loc->se_ptr.current.maddr) {
            se_loc->se_ptr.current.maddr = page_allocator_getpage(cid, &se_loc->se_ptr.current.laddr, PA_PROT_WRITE);
        }
        if (ptr_val != NULL) {
            key.se_data.current.maddr = itop(ROUND_DWNPG(ptoi(ptr_val)));
            se_val = RB_FIND(used_slab_entry_tree, &sd->sd_maddr_root, &key);
            if (se_val) {
                pval_offset = (ptoi(ptr_val) - ROUND_DWNPG(ptoi(ptr_val)));
                struct slab_ptr *sptr = se_loc->se_ptr.current.maddr;
                sptr->ptrs[se_loc->se_ptr.current.idx].ploc_offset = ploc_offset;
                sptr->ptrs[se_loc->se_ptr.current.idx].pval_seid = se_val->se_id;
                sptr->ptrs[se_loc->se_ptr.current.idx].pval_offset = pval_offset;
                se_loc->se_ptr.current.idx++;
            } else
                handle_error("failed to find the target slab_entry for the given pointer\n");
        } else {
            struct slab_ptr *sptr = se_loc->se_ptr.current.maddr;
            sptr->ptrs[se_loc->se_ptr.current.idx].ploc_offset = ploc_offset;
            sptr->ptrs[se_loc->se_ptr.current.idx].pval_seid = SLAB_PTR_SEID_NULL;
            sptr->ptrs[se_loc->se_ptr.current.idx].pval_offset = SLAB_PTR_OFFSET_NULL;
            se_loc->se_ptr.current.idx++;
        }
    } else
        handle_error("failed to find the slab entry for the given pointer location\n");
}

static void dont_insert_pointer(unsigned int cid, void **ptr_loc) {}

static void (*Func_insert_pointer)(unsigned int, void **) = do_insert_pointer;

void slab_insert_pointer(unsigned int cid, void **ptr_loc)
{
    Func_insert_pointer(cid, ptr_loc);
}

size_t slab_setroot(unsigned int cid, void *maddr)
{
    struct slab_entry *se;
    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    size_t ret = 0; /* error */
    uint64_t new_cont_root;

    void *rounded_maddr = itop(ROUND_DWNPG(ptoi(maddr)));
    struct slab_entry key = SLAB_ENTRY_SEARCH_KEY(rounded_maddr, SE_SEARCH_NORMAL);
    se = RB_FIND(used_slab_entry_tree, &sd->sd_maddr_root, &key);
    if (se) {
        //TODO: what should we do if there is a root already?
        PACK_CONT_ROOT(&new_cont_root, se->se_id, ROUND_DWNPG(ptoi(maddr)) - ptoi(maddr));
        atomic_set(&sd->sd_cont_root, new_cont_root);
        ret = 1;
    }

    return ret;
}

void *slab_getroot(unsigned int cid)
{
    struct slab_entry *se;
    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    void *ret = NULL;
    unsigned int root_seid;
    uint16_t root_offset;

    UNPACK_CONT_ROOT(sd->sd_cont_root, &root_seid, &root_offset);

    se = get_slab_entry_by_id(cid, root_seid);
    ret = se->se_data.current.maddr + root_offset;

    return ret;
}

struct slab_entry *slab_find(unsigned int cid, void *maddr)
{
    struct slab_entry *se;
    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    struct slab_entry key = SLAB_ENTRY_SEARCH_KEY(maddr, SE_SEARCH_MADDR);
    se = RB_FIND(used_slab_entry_tree, &sd->sd_maddr_root, &key);
    return se;
}

void (*Func_slab_entry_snapshot)(unsigned int cid, struct slab_entry *se) = slab_entry_snapshot;
int (*Func_slab_bucket_snapshot)(unsigned int cid, struct slab_bucket *sb) = slab_bucket_snapshot;

void slab_init()
{
    LOG(3, "Initializing slab");

    char *ptr = getenv("PMLIB_FIX_PTRS");
    if (ptr) {
        int val = atoi(ptr);
        if (val == 0) {
            Func_fixptrs = dont_fixptrs;
            Func_insert_pointer = dont_insert_pointer;
            LOG(3, "Pointer fixing has been disabled");
        }
    } else {
        Func_fixptrs = do_fixptrs;
        Func_insert_pointer = do_insert_pointer;
        LOG(3, "Pointer fixing is enabled");
    }

    ptr = getenv("PMLIB_USE_NLMAPPER");
    if (ptr) {
        int val = atoi(ptr);
        if (val == 1) {
            Func_slab_entry_snapshot = slab_entry_copynswap;
            Func_slab_bucket_snapshot = slab_bucket_copynswap;
            LOG(3, "Using slab_entry_copynswap");
            LOG(3, "Using slab_bucket_copynswap");
        }
    }
}
