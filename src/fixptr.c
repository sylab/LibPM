#include <string.h>

#include <fixptr.h>
#include <page_alloc.h>
#include <utils/macros.h>
#include <atomics.h>

void pptr_init(unsigned int cid, struct pptr_head *head)
{
    struct pptr_inode *inode;
    inode  = page_allocator_getpage(cid, &head->ph_first.laddr, PA_PROT_WRITE);
    if (!inode)
        handle_error("failed to allocate memory for the pptr_head\n");

    head->ph_first.maddr = inode;
    head->ph_last.maddr = inode;
    inode->pin_current_idx = 0;
}

static int pptr_snapshot_dnode(unsigned int cid, struct pptr_inode *inode, int idx)
{
    size_t current_laddr = PPTR_INODE_CIDXL(inode, idx);
    size_t snapshot_laddr = PPTR_INODE_SIDXL(inode, idx);
    int ret = 0;  //dnode

    if (current_laddr == snapshot_laddr && snapshot_laddr != 0) {
        void *snapshot_maddr = page_allocator_getpage(cid, &PPTR_INODE_SIDXL(inode, idx), PA_PROT_WRITE);
        if (!snapshot_maddr)
            handle_error("failed to allocate memory for snapshot (dnode)\n");
        pmemcpy(snapshot_maddr, PPTR_INODE_CIDXM(inode, idx), PAGE_SIZE);
        PPTR_INODE_SIDXM(inode, idx) = (struct pptr_dnode*) snapshot_maddr;
        ret = 1;
    }

    return ret;
}

static struct pptr_inode *
pptr_extend_list(unsigned int cid, struct pptr_head *head)
{
    struct pptr_inode * last_inode = head->ph_last.maddr;
    struct pptr_inode * new_last_inode;

    new_last_inode = page_allocator_getpage(cid, &last_inode->pin_next.laddr, PA_PROT_WRITE);
    if (!new_last_inode)
        handle_error("failed to allocate memory for a new inode\n");
    last_inode->pin_next.maddr = new_last_inode;
    head->ph_last.maddr = new_last_inode;

    return new_last_inode;
}

static inline int pptr_need_expanding(struct pptr_inode *inode)
{
    int ret = 0; //it's not full
    struct pptr_dnode *dnode = PPTR_INODE_CLAST(inode);
    if (dnode) {
        if (PPTR_DNODE_IS_FULL(dnode))
            ret = 1;
    }
    return ret;
}

/*
 * Returns a non-full dnode, creating one if necessary
 */
static struct pptr_dnode *
pptr_get_dnode(unsigned int cid, struct pptr_head * head, struct pptr_inode **pinode)
{
    struct pptr_inode *last_inode = head->ph_last.maddr;
    struct pptr_dnode *dnode;

    // do we need a new inode?
    if (pptr_need_expanding(last_inode))
        last_inode = pptr_extend_list(cid, head);

    // get an index in the current inode
    dnode = PPTR_INODE_CMADDR(last_inode);
    if (dnode && PPTR_DNODE_IS_FULL(dnode)) {
        last_inode->pin_current_idx++;
        dnode = PPTR_INODE_CMADDR(last_inode);
    }

    // initialize the dnode and snapshot it if necessary
    if (!dnode) {
        PPTR_INODE_CMADDR(last_inode) =
            page_allocator_getpage(cid, &PPTR_INODE_CLADDR(last_inode), PA_PROT_WRITE);
        dnode = PPTR_INODE_CMADDR(last_inode);
    }

    if (pinode)
        *pinode = last_inode;

    return dnode;
}

void pptr_append(unsigned int cid, struct pptr_head *head,
                 unsigned int ploc_seid, uint16_t ploc_offset,
                 unsigned int pval_seid, uint16_t pval_offset)
{
    struct pptr_inode *inode;
    struct pptr_dnode *dnode = pptr_get_dnode(cid, head, &inode);

    pptr_snapshot_dnode(cid, inode, inode->pin_current_idx);

    dnode->pdn_entries[dnode->pdn_index].ploc_seid = ploc_seid;
    dnode->pdn_entries[dnode->pdn_index].ploc_offset = ploc_offset;
    dnode->pdn_entries[dnode->pdn_index].pval_seid = pval_seid;
    dnode->pdn_entries[dnode->pdn_index].pval_offset = pval_offset;
    dnode->pdn_index++;
}

void pptr_cpoint(unsigned int cid, struct pptr_head *head)
{
    int inode_idx = 0;
    struct pptr_inode *inode;
    struct pptr_dnode *current_dnode;
    struct pptr_dnode *snapshot_dnode;

    for (inode = head->ph_first.maddr; inode != NULL; inode = inode->pin_next.maddr) {
        for (inode_idx = 0; inode_idx < PPTR_INODE_ENTRIES; inode_idx++) {
            current_dnode = PPTR_INODE_CIDXM(inode, inode_idx);
            if (!current_dnode)
                continue;
            snapshot_dnode = PPTR_INODE_SIDXM(inode, inode_idx);
            if (snapshot_dnode && snapshot_dnode != current_dnode)
                page_allocator_freepages(cid, snapshot_dnode);
            PPTR_INODE_SIDXL(inode, inode_idx) = PPTR_INODE_CIDXL(inode, inode_idx);
            PPTR_INODE_SIDXM(inode, inode_idx) = PPTR_INODE_CIDXM(inode, inode_idx);
        }
    }
}

void pptr_map(unsigned int cid, struct pptr_head *head, int type)
{
    int i = 0;
    struct pptr_inode *inode;
    struct pptr_inode *prev_inode;

    inode = page_allocator_mappage(cid, head->ph_first.laddr);
    head->ph_first.maddr = inode;
    while (inode->pin_next.laddr) {
        prev_inode = inode;
        inode = page_allocator_mappage(cid, inode->pin_next.laddr);
        prev_inode->pin_next.maddr = inode;
    }
    head->ph_last.maddr = inode;

    for (inode = head->ph_first.maddr; inode != NULL; inode = inode->pin_next.maddr) {
        for (i = 0; i < PPTR_INODE_ENTRIES; i++) {
            if (PPTR_INODE_CIDXL(inode, i)) {
                PPTR_INODE_CIDXM(inode, i) = page_allocator_mappage(cid, PPTR_INODE_CIDXL(inode, i));
                PPTR_INODE_SIDXM(inode, i) = PPTR_INODE_CIDXM(inode, i);
            }

            if (type == CPOINT_INCOMPLETE && PPTR_INODE_SIDXL(inode, i)) {
                PPTR_INODE_SIDXM(inode, i) = page_allocator_mappage(cid, PPTR_INODE_SIDXL(inode, i));
            }
        }
    }

    if (type == CPOINT_INCOMPLETE)
        pptr_cpoint(cid, head);
}

static void pptr_dnode_print(struct pptr_dnode *dnode, size_t laddr, int level, const char *label)
{
    int i;
    int step = 2;
    struct pptr *pptr;

    printf("%*s%s_dnode (maddr: %p, laddr: %zu) [index: %u]\n", level, "", label, dnode, laddr, dnode->pdn_index);
    for (i = 0; i < PPTR_DNODE_ENTRIES; i++) {
        pptr = &dnode->pdn_entries[i];
        if (pptr->ploc_offset) {
            printf("%*spptr [ loc_offset: %u, loc_seid: %u, val_offset: %u, val_seid: %u ] \n",
                    level + step, "", pptr->ploc_offset, pptr->ploc_seid, pptr->pval_offset, pptr->pval_seid);
        }
    }
}

static void pptr_inode_print(struct pptr_inode *inode, size_t laddr, int level)
{
    int i;
    int step = 2;

    printf("%*sinode (maddr: %p, laddr: %zu) [index: %u]\n", level, "", inode, laddr, inode->pin_current_idx);
    for (i = 0; i < PPTR_INODE_ENTRIES; i++) {
        if (PPTR_INODE_CIDXM(inode, i))
            pptr_dnode_print(PPTR_INODE_CIDXM(inode, i), PPTR_INODE_CIDXL(inode, i), level + step, "current");
        if (PPTR_INODE_SIDXM(inode, i))
            pptr_dnode_print(PPTR_INODE_SIDXM(inode, i), PPTR_INODE_SIDXL(inode, i), level + step, "snapshot");
    }
}

void pptr_print(struct pptr_head *head)
{
    int step = 2;
    struct pptr_inode *inode;
    size_t inode_laddr = head->ph_first.laddr;

    printf("head (%p) [first.maddr: %p, first.laddr: %zu, last.maddr: %p]\n",
            head, head->ph_first.maddr, head->ph_first.laddr, head->ph_last.maddr);
    for (inode = head->ph_first.maddr; inode != NULL; inode = inode->pin_next.maddr) {
        pptr_inode_print(inode, inode_laddr, step);
        inode_laddr = inode->pin_next.laddr;
    }
}

