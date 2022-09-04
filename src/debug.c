#include "slabInt.h"
#include "cont.h"

void print_splay_tree()
{
    struct slab_entry *se;
    struct slab_dir *sd = get_container(0)->current_slab.maddr;
    RB_FOREACH(se, used_slab_entry_tree, &sd->sd_maddr_root) {
        printf("tree's slab_entrys [maddr: %p]\n", se->se_data.current.maddr);
    }
}

void slab_entry_print(unsigned int cid)
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

                    printf("all slab_entrys [maddr: %p]\n", se->se_data.current.maddr);
                }
            }
        }
    }
}

void slab_ptr_pprint(struct slab_ptr *sp, int sp_idx, int level)
{
    int step = 2;
    for (int i = 0; i < sp_idx; i++) {
        printf("%*ssp [ploc_offset: %u, pval_seid: %u, pval_offset: %u]\n",
               level + step, "", sp->ptrs[i].ploc_offset, sp->ptrs[i].pval_seid, sp->ptrs[i].pval_offset);
    }
}

int bitmap_set_count(bitstr_t *bm, int size)
{
    int count = 0;
    for (int i = 0; i < size; ++i) {
        count += bit_test(bm, i) ? 1 : 0;
    }
    return count;
}

void slab_entry_pprint(struct slab_entry *se, int level)
{
    int step = 2;
    printf("%*sse (%p) [id: %u, size: %u]\n",
            level, "", se, se->se_id, se->se_size);

    printf("%*sdata_c [maddr: %p, laddr: %zu bm: %d (%d)]\n", level + step, "",
            se->se_data.current.maddr,
            se->se_data.current.laddr,
            bitmap_set_count(SLAB_ENTRY_CBITMAP(se), SLAB_ENTRY_CAPACITY(se)),
            SLAB_ENTRY_CAPACITY(se));
    if (se->se_data.current.laddr != se->se_data.snapshot.laddr || 1)
        printf("%*sdata_s [maddr: %p, laddr: %zu bm: %d (%d)]\n", level + step, "",
                se->se_data.snapshot.maddr,
                se->se_data.snapshot.laddr,
                bitmap_set_count(SLAB_ENTRY_SBITMAP(se), SLAB_ENTRY_CAPACITY(se)),
                SLAB_ENTRY_CAPACITY(se));

    printf("%*sptr_c [idx: %u, maddr: %p, laddr: %zu]\n",
            level + step, "", se->se_ptr.current.idx, se->se_ptr.current.maddr, se->se_ptr.current.laddr);
    slab_ptr_pprint(se->se_ptr.current.maddr, se->se_ptr.current.idx, level + step);
    if (se->se_ptr.current.laddr != se->se_ptr.snapshot.laddr || 1) {
        printf("%*sptr_s [idx: %u, maddr: %p, laddr: %zu]\n",
                level + step, "", se->se_ptr.snapshot.idx, se->se_ptr.snapshot.maddr, se->se_ptr.snapshot.laddr);
        slab_ptr_pprint(se->se_ptr.snapshot.maddr, se->se_ptr.snapshot.idx, level + step);
    }
}

void slab_bucket_pprint(struct slab_bucket *sb, int level)
{
    int step = 2;
    int i;
    printf("%*ssb (%p) [id: %u, capacity: %lu]\n",
            level, "", sb, sb->sb_id, SLAB_BUCKET_ENTRIES);
    for (i = 0; i < SLAB_BUCKET_ENTRIES; i++) {
        if (sb->sb_entries[i].se_size)
            slab_entry_pprint(&sb->sb_entries[i], level + step);
    }
}

void slab_inner_pprint(struct slab_inner *si, int level)
{
    int step = 2;
    int i;
    struct slab_bucket *sbc, *sbs;
    printf("%*ssi (%p) [index: %u, capacity: %lu]\n",
            level, "", si, si->si_index, SLAB_INNER_ENTRIES);
    for (i = 0; i < si->si_index; i++) {
        sbc = si->si_current[i].maddr;
        sbs = si->si_snapshot[i].maddr;
        slab_bucket_pprint(sbc, level + step);
        if (sbc != sbs)
            slab_bucket_pprint(sbs, level + step);
    }
}

void slab_outer_pprint(struct slab_outer *so, int level)
{
    int step = 2;
    int i;
    struct slab_inner *sic, *sis;
    printf("%*sso (%p) [index: %u, capacity: %lu]\n",
            level, "", so, so->so_index, SLAB_OUTER_ENTRIES);
    for (i = 0; i < so->so_index; i++) {
        sic = so->so_current[i].maddr;
        sis = so->so_snapshot[i].maddr;
        slab_inner_pprint(sic, level + step);
        if (sic != sis)
            slab_inner_pprint(sis, level + step);
    }
}

void slab_dir_pprint(struct slab_dir *sd, int level)
{
    int step = 2;
    int i;
    struct slab_outer *soc, *sos;
    printf("%*ssd (%p) [index: %u]\n", level, "", sd, sd->sd_index);
    for (i = 0; i < sd->sd_index; i++) {
        soc = sd->sd_current[i].maddr;
        sos = sd->sd_snapshot[i].maddr;
        slab_outer_pprint(soc, level + step);
        if (soc != sos)
            slab_outer_pprint(soc, level + step);
    }

    //pptr_print(&sd->sd_ptr_list_head);
}

/*
void slab_dir_phead_ptr(struct slab_dir *sd, int level)
{
    int step;
    int i;
    struct pptr_node *node;

    pptr_for_each(&sd->sd_ptr_list_head, node) {
        printf("%*spnode (%p) [index: %u]\n", level, "", node, node->pn_index);
    }
}
*/

void container_pprint()
{
    int step = 2;
    struct container *cont = CONTAINERS[0];
    printf("cont (%p) [id: %u]\n", cont, cont->id);
    slab_dir_pprint(cont->current_slab.maddr, step);
    if (cont->current_slab.maddr != cont->snapshot_slab.maddr)
        slab_dir_pprint(cont->snapshot_slab.maddr, step);
}

