#include <sys/mman.h>
#include <signal.h>
#include <string.h>

#include "slabInt.h"
#include "cont.h"
#include "atomics.h"
#include "page_alloc.h"
#include "stats.h"
#include "out.h"

extern void (*Func_slab_entry_snapshot)(unsigned int cid, struct slab_entry *se);

void handle_memory_update(int sigid, siginfo_t *sig, void *unused)
{
    //TODO: add support for multiple containers
    unsigned int cid = 0;
    void *pgaddr;
    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    struct slab_entry *se;

    STATS_INC_FAULTS();
    LOG(20, "Fault at location %p", sig->si_addr);

    pgaddr = itop(ROUND_DWNPG(ptoi(sig->si_addr)));
    struct slab_entry key = SLAB_ENTRY_SEARCH_KEY(pgaddr, SE_SEARCH_NORMAL);
    se = RB_FIND(used_slab_entry_tree, &sd->sd_maddr_root, &key);
    if (se) {
        Func_slab_entry_snapshot(cid, se);
        //TODO: handle the case of allocation bigger than a page
        page_allocator_mprotect(cid, se->se_data.current.maddr, PAGE_SIZE, PA_PROT_READ | PA_PROT_WRITE);
    } else {
        LOG(5, "No slab_entry found for address %p", sig->si_addr);
        handle_error("Got SIGSEGV at address: 0x%lx\n", (long) sig->si_addr);
    }
}

void slab_entry_snapshot(unsigned int cid, struct slab_entry *se)
{
    size_t data_laddr;
    void *data_maddr = page_allocator_getpage(cid, &data_laddr, PA_PROT_WRITE);
    if (data_maddr == NULL)
        handle_error("failed to allocate memory for slab_entry (data page) snapshot\n");
    pmemcpy(data_maddr, se->se_data.current.maddr, ROUNDPG(se->se_size));

    STATS_INC_COWDATA();

    // keep track of snapshotted slab_entries. We free this vector on slab_cpoint!
    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    VECTOR_APPEND(&sd->sd_vector, se);

    // if there are pointers in this page, then we need to snapshot them as well
    if (se->se_ptr.snapshot.maddr != NULL) {
        size_t ptr_laddr;
        void *ptr_maddr = page_allocator_getpage(cid, &ptr_laddr, PA_PROT_WRITE);
        if (ptr_maddr == NULL)
            handle_error("failed to allocate memory for slab_entry (ptr page) snapshot\n");
        pmemcpy(ptr_maddr, se->se_ptr.snapshot.maddr, ROUNDPG(se->se_size));
        se->se_ptr.current.idx = se->se_ptr.snapshot.idx;
        atomic_set(&ptr_laddr, se->se_ptr.current.laddr);
        se->se_ptr.current.maddr = ptr_maddr;

        STATS_INC_COWMETA();
    }

    se->se_data.snapshot.maddr = data_maddr;
    atomic_set(&se->se_data.snapshot.laddr, data_laddr);
}

void slab_entry_copynswap(unsigned int cid, struct slab_entry *se)
{
    size_t data_laddr;
    void *data_maddr = page_allocator_getpage(cid, &data_laddr, PA_PROT_WRITE);

    if (data_maddr == NULL)
        handle_error("failed to allocate memory for slab_entry (data page) snapshot\n");

    // copy the contents of the snapshot page into the new current page (no clflush)
    memcpy(data_maddr, se->se_data.snapshot.maddr, ROUNDPG(se->se_size));

    se->se_data.snapshot.maddr = data_maddr;
    atomic_set(&se->se_data.current.laddr, data_laddr);

    // now we swap the mappings for current and snapshot pages
    page_allocator_swap_mappings(cid,
                                 data_maddr,                    // current address
                                 se->se_data.snapshot.laddr,    // snapshot offset
                                 se->se_data.snapshot.maddr,    // snapshot address
                                 data_laddr);                   // current offset

    STATS_INC_COWDATA();

    // keep track of snapshotted slab_entries. We free this vector on slab_cpoint!
    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    VECTOR_APPEND(&sd->sd_vector, se);

    // if there are pointers in this page, then we need to snapshot them as well
    if (se->se_ptr.snapshot.maddr != NULL) {
        assert(0 && "TODO: implement this later");
        size_t ptr_laddr;
        void *ptr_maddr = page_allocator_getpage(cid, &ptr_laddr, PA_PROT_WRITE);
        if (ptr_maddr == NULL)
            handle_error("failed to allocate memory for slab_entry (ptr page) snapshot\n");
        pmemcpy(ptr_maddr, se->se_ptr.snapshot.maddr, ROUNDPG(se->se_size));
        se->se_ptr.current.idx = se->se_ptr.snapshot.idx;
        atomic_set(&ptr_laddr, se->se_ptr.current.laddr);
        se->se_ptr.current.maddr = ptr_maddr;

        STATS_INC_COWMETA();
    }

}

int slab_bucket_snapshot(unsigned int cid, struct slab_bucket *sb)
{
    int bucket_was_snapshoted = 0;

    int si_id = sb->sb_id / SLAB_INNER_ENTRIES;
    int so_id = si_id / SLAB_OUTER_ENTRIES;
    int so_idx = si_id % SLAB_OUTER_ENTRIES;
    int sd_idx = so_id % SLAB_DIR_ENTRIES;

    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    struct slab_outer *so = sd->sd_current[sd_idx].maddr;
    struct slab_inner *si = so->so_current[so_idx].maddr;

    if (si->si_current[si_id].maddr == si->si_snapshot[si_id].maddr) {
        size_t snapshot_laddr;
        struct slab_bucket * sbs = (struct slab_bucket*) page_allocator_getpage(cid, &snapshot_laddr, PA_PROT_WRITE);
        if (sbs == NULL)
            handle_error("failed to allocate memory for slab_bucket snapshot\n");
        pmemcpy(sbs, sb, ROUNDPG(sizeof(*sb)));

        atomic_set(&si->si_snapshot[si_id].laddr, snapshot_laddr);

        si->si_snapshot[si_id].maddr = sbs;
        bucket_was_snapshoted = 1;

        STATS_INC_COWMETA();
    }

    return bucket_was_snapshoted;
}

int slab_bucket_copynswap(unsigned int cid, struct slab_bucket *sb)
{
    int bucket_was_snapshoted = 0;

    int si_id = sb->sb_id / SLAB_INNER_ENTRIES;
    int so_id = si_id / SLAB_OUTER_ENTRIES;
    int so_idx = si_id % SLAB_OUTER_ENTRIES;
    int sd_idx = so_id % SLAB_DIR_ENTRIES;

    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    struct slab_outer *so = sd->sd_current[sd_idx].maddr;
    struct slab_inner *si = so->so_current[so_idx].maddr;

    if (si->si_current[si_id].maddr == si->si_snapshot[si_id].maddr) {
        size_t snapshot_laddr;
        struct slab_bucket *sbs = (struct slab_bucket*) page_allocator_getpage(cid, &snapshot_laddr, PA_PROT_WRITE);
        if (sbs == NULL)
            handle_error("failed to allocate memory for slab_bucket snapshot\n");

        // copy the current snapshotted slab_bucket into the current slab_bucket
        memcpy(sbs, sb, ROUNDPG(sizeof(*sb)));

        // now we swap the mappins for current and snapshot pages
        page_allocator_swap_mappings(cid,
                                     si->si_current[si_id].maddr,   // current addr
                                     snapshot_laddr,                // snapshot offset
                                     sbs,                           // snapshot addr
                                     si->si_current[si_id].laddr);  // current offset

        atomic_set(&si->si_current[si_id].laddr, snapshot_laddr);
        si->si_snapshot[si_id].maddr = sbs;
        bucket_was_snapshoted = 1;

        STATS_INC_COWMETA();
    }

    return bucket_was_snapshoted;
}

struct slab_inner* slab_inner_snapshot(unsigned int cid, struct slab_inner *si, size_t *laddr)
{
    struct slab_inner *sis = NULL;
    size_t snapshot_laddr;

    STATS_INC_COWMETA();

    sis = (struct slab_inner*) page_allocator_getpage(cid, &snapshot_laddr, PA_PROT_WRITE);
    if (sis == NULL)
        handle_error("failed to allocate memory for slab_dir snapshot\n");
    pmemcpy(sis, si, ROUNDPG(sizeof(*si)));

    atomic_set(laddr, snapshot_laddr);

    return sis;
}

struct slab_outer* slab_outer_snapshot(unsigned int cid, struct slab_outer *so, size_t *laddr)
{
    struct slab_outer *sos = NULL; //funny
    size_t snapshot_laddr;

    STATS_INC_COWMETA();

    sos = (struct slab_outer*) page_allocator_getpage(cid, &snapshot_laddr, PA_PROT_WRITE);
    if (sos == NULL)
        handle_error("failed to allocate memory for slab_dir snapshot\n");
    pmemcpy(sos, so, ROUNDPG(sizeof(*so)));

    atomic_set(laddr, snapshot_laddr);

    return sos;
}

struct slab_dir* slab_dir_snapshot(unsigned int cid, struct slab_dir *sd, size_t *laddr)
{
    struct slab_dir *sds = NULL;
    size_t snapshot_laddr;

    STATS_INC_COWMETA();

    sds = (struct slab_dir*) page_allocator_getpage(cid, &snapshot_laddr, PA_PROT_WRITE);
    if (sds == NULL)
        handle_error("failed to allocate memory for slab_dir snapshot\n");
    pmemcpy(sds, sd, ROUNDPG(sizeof(*sd)));

    atomic_set(laddr, snapshot_laddr);

    return sds;
}

void slab_foreach_snapshot_entry(unsigned int cid,
                                 void (*fun)(struct slab_entry *se, void *param),
                                 void *param)
{
    struct slab_dir *sd = get_container(cid)->current_slab.maddr;
    struct slab_entry *se;
    for (int i = 0; i < VECTOR_SIZE(&sd->sd_vector); i++) {
        se = VECTOR_AT(&sd->sd_vector, i);
        fun(se, param);
    }
}
