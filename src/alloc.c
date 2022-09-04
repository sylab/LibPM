#include "slabInt.h"
#include "cont.h"
#include "stats.h"

int slab_entry_full(struct slab_entry *se)
{
    bitstr_t *bitmap = SLAB_ENTRY_CBITMAP(se);
    int capacity = SLAB_ENTRY_CAPACITY(se);
    int count = 0;

    for (int i = 0; i < capacity; i++) {
        count += bit_test(bitmap, i) ? 1 : 0;
    }

    return (count == capacity);
}

static void *slab_entry_alloc_mem(unsigned int cid, struct slab_entry *se)
{
    bitstr_t *bitmap = SLAB_ENTRY_CBITMAP(se);
    int bm_capacity = SLAB_ENTRY_CAPACITY(se);
    int data_offset = SLAB_ENTRY_DATAOFFSET(se);
    void *maddr = NULL;
    int idx;

    assert(se->se_data.current.maddr != NULL && "Invalid pointer to data (NULL)");

    bit_ffc(bitmap, bm_capacity, &idx);
    if (idx != -1) {
        maddr = se->se_data.current.maddr + data_offset + (idx * se->se_size);
        bit_set(bitmap, idx);
    }

    return maddr;
}


static struct slab_entry *get_free_slab_entry(unsigned int cid)
{
    struct container *cont;
    struct slab_dir *sd;
    struct slab_entry *se;
    struct slab_outer *so;
    struct slab_inner *si;
    struct slab_bucket *sb;
    size_t sb_laddr;

    cont = get_container(cid);
    sd = cont->current_slab.maddr;

    se = STAILQ_FIRST(&sd->sd_free_list);
    if (se != NULL) {
        goto use_free_entry;
    }

    so = sd->sd_current[sd->sd_index - 1].maddr;
    si = so->so_current[so->so_index - 1].maddr;

    if (SLAB_INNER_FULL(si)) {
        if (SLAB_OUTER_FULL(so)) {

            if (cont->current_slab.maddr == cont->snapshot_slab.maddr)
                cont->snapshot_slab.maddr = slab_dir_snapshot(cid, sd, &cont->snapshot_slab.laddr);

            if (SLAB_DIR_FULL(sd))
                handle_error("Slab directory is full. Can not allocate more memory.\n");

            size_t so_laddr;
            so = slab_outer_init(cid, &so_laddr);
            sd->sd_current[sd->sd_index].maddr = so;
            sd->sd_current[sd->sd_index].laddr = so_laddr;
            sd->sd_snapshot[sd->sd_index].maddr = so;
            sd->sd_snapshot[sd->sd_index].laddr = so_laddr;
            sd->sd_index++;
        } else {

            if (sd->sd_current[so->so_index - 1].maddr == sd->sd_snapshot[sd->sd_index - 1].maddr)
                sd->sd_snapshot[sd->sd_index - 1].maddr =
                    slab_outer_snapshot(cid, so, &sd->sd_snapshot[sd->sd_index - 1].laddr);
        }

        size_t si_laddr;
        si = slab_inner_init(cid, &si_laddr);
        so->so_current[so->so_index].maddr = si;
        so->so_current[so->so_index].laddr = si_laddr;
        so->so_snapshot[so->so_index].maddr = si;
        so->so_snapshot[so->so_index].laddr = si_laddr;
        so->so_index++;
    } else {
        if (so->so_current[so->so_index - 1].maddr == so->so_snapshot[so->so_index - 1].maddr)
            so->so_snapshot[so->so_index - 1].maddr =
                slab_inner_snapshot(cid, si, &so->so_snapshot[so->so_index - 1].laddr);
    }

    sb = slab_bucket_init(cid, &sb_laddr);
    si->si_current[si->si_index].maddr = sb;
    si->si_current[si->si_index].laddr = sb_laddr;
    si->si_snapshot[si->si_index].maddr = sb;
    si->si_snapshot[si->si_index].laddr = sb_laddr;
    si->si_index++;

    // we try again. now we should not fail!
    se = STAILQ_FIRST(&sd->sd_free_list);
    if (!se) {
        handle_error("failed to get an slab_entry even after initializing a new slab_bucket!\n");
    }

use_free_entry:
    STAILQ_REMOVE_HEAD(&sd->sd_free_list, se_list);
    return se;
}

extern int (*Func_slab_bucket_snapshot)(unsigned int cid, struct slab_bucket *sb);

void *slab_palloc(unsigned int cid, unsigned int size)
{
    void *maddr = NULL;
    struct slab_dir *sd;
    struct container *cont;
    struct slab_entry *se;
    struct slab_entry_size *es;
    struct slab_bucket *sb;
    int size8 = ROUND8(size);

    STATS_INC_PALLOCATIONS();

    if (size > PAGE_SIZE)
        handle_error("allocations bigger than 4KB are not implemented yet.\n");

    cont = get_container(cid);
    sd = cont->current_slab.maddr;

    struct slab_entry_size key = { .es_size = size8 };
    es = RB_FIND(sizes_slab_entry_tree, &sd->sd_size_root, &key);
    if (es) {
        se = STAILQ_FIRST(&es->es_list);
        //TODO: consider caching the size to avoid calling slab_entry_full again. It's slow!
        if (slab_entry_full(se)) {
            se = get_free_slab_entry(cid);
            STAILQ_INSERT_HEAD(&es->es_list, se, se_list);
        }
    } else {
        es = slab_entry_size_init(size8);
        RB_INSERT(sizes_slab_entry_tree, &sd->sd_size_root, es);

        se = get_free_slab_entry(cid);
        STAILQ_INSERT_HEAD(&es->es_list, se, se_list);
    }

    sb = (struct slab_bucket*) ROUND_DWNPG(ptoi(se));
    sb->sb_has_snapshot = Func_slab_bucket_snapshot(cid, sb);

    if (!SLAB_ENTRY_IS_INIT(se)) {
        slab_entry_init(cid, se, size8);
        RB_INSERT(used_slab_entry_tree, &sd->sd_maddr_root, se);
    }

    maddr = slab_entry_alloc_mem(cid, se);
    if (slab_entry_full(se)) {
        STAILQ_INSERT_TAIL(&es->es_list, se, se_list);
    }

    return maddr;
}

