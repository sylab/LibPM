#include <sys/mman.h>

#include "slabInt.h"
#include "cont.h"
#include "atomics.h"
#include "page_alloc.h"

static void slab_bucket_cpoint(unsigned int cid, struct slab_bucket *sb, int type)
{
    struct slab_entry *se;
    int i;

    for (i = 0; i < SLAB_BUCKET_ENTRIES; i++) {
        se = &sb->sb_entries[i];

        if (!SLAB_ENTRY_IS_INIT(se))
            continue;

        if (se->se_data.current.maddr != se->se_data.snapshot.maddr) {
            atomic_set(&se->se_data.snapshot.laddr, se->se_data.current.laddr);
            page_allocator_freepages(cid, se->se_data.snapshot.maddr);
            se->se_data.snapshot.maddr = se->se_data.current.maddr;
            //TODO: handle allocation bigger than one page
        }

        if (se->se_ptr.current.maddr != NULL) {
            if (se->se_ptr.snapshot.maddr != NULL &&
                    se->se_ptr.snapshot.maddr != se->se_ptr.current.maddr) {
                page_allocator_freepages(cid, se->se_ptr.snapshot.maddr);
            }
            atomic_set(&se->se_ptr.snapshot.laddr, se->se_ptr.current.laddr);
            se->se_ptr.snapshot.maddr = se->se_ptr.current.maddr;
        }

        if (type == CPOINT_REGULAR)
            page_allocator_mprotect(cid, se->se_data.current.maddr, PAGE_SIZE, PROT_READ);
    }

}

static void slab_inner_cpoint(unsigned int cid, struct slab_inner *si, int type)
{
    int i;
    struct slab_bucket *sb;

    for (i = 0; i < si->si_index; i++) {
        sb = si->si_current[i].maddr;
        if (sb->sb_has_snapshot) {
            slab_bucket_cpoint(cid, si->si_current[i].maddr, type);

            atomic_set(&si->si_snapshot[i].laddr, si->si_current[i].laddr);
            page_allocator_freepages(cid, si->si_snapshot[i].maddr);
            si->si_snapshot[i].maddr = si->si_current[i].maddr;

            sb->sb_has_snapshot = 0;
        }
    }
}

static void slab_outer_cpoint(unsigned int cid, struct slab_outer *so, int type)
{
    int i;

    for (i = 0; i < so->so_index; i++) {
        slab_inner_cpoint(cid, so->so_current[i].maddr, type);

        if (so->so_current[i].maddr != so->so_snapshot[i].maddr) {
            atomic_set(&so->so_snapshot[i].laddr, so->so_current[i].laddr);
            page_allocator_freepages(cid, so->so_snapshot[i].maddr);
            so->so_snapshot[i].maddr = so->so_current[i].maddr;
        }
    }
}

static void slab_dir_cpoint(unsigned int cid, struct slab_dir *sd, int type)
{
    int i;

    for (i = 0; i < sd->sd_index; i++) {
        slab_outer_cpoint(cid, sd->sd_current[i].maddr, type);

        if (sd->sd_current[i].maddr != sd->sd_snapshot[i].maddr) {
            atomic_set(&sd->sd_snapshot[i].laddr, sd->sd_current[i].laddr);
            page_allocator_freepages(cid, sd->sd_snapshot[i].maddr);
            sd->sd_snapshot[i].maddr = sd->sd_current[i].maddr;
        }
    }
}

void slab_cpoint(unsigned int cid, int type)
{
    struct slab_dir *sd;
    sd = get_container(cid)->current_slab.maddr;
    slab_dir_cpoint(cid, sd, type);

    //TODO: here we need to flush both data and metadata pages
    struct slab_entry *se = NULL;
    for (int i = 0; i < VECTOR_SIZE(&sd->sd_vector); ++i) {
        se = VECTOR_AT(&sd->sd_vector, i);
        flush_memsegment(se->se_data.current.maddr, PAGE_SIZE, 0);
    }
    VECTOR_FREE(&sd->sd_vector);
}
