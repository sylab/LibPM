#ifndef FIXPTR_H
#define FIXPTR_H

#include <settings.h>
#include <utils/macros.h>
#include <utils/queue.h>

//#define PPTR_DNODE_ENTRIES   2
#define PPTR_DNODE_ENTRIES ((PAGE_SIZE - sizeof(int)) / sizeof(struct pptr))
#define PPTR_DNODE_IS_FULL(p)    ((p)->pdn_index == PPTR_DNODE_ENTRIES)

//#define PPTR_INODE_ENTRIES  2
#define PPTR_INODE_ENTRIES ((PAGE_SIZE - sizeof(int) \
                                - sizeof(size_t) \
                                - sizeof(void*)) / \
                                (2 * (sizeof(void*) + sizeof(size_t))))
#define PPTR_INODE_IS_FULL(p)    ((p)->pin_index == PPTR_DNODE_ENTRIES)
//TODO: should this be an atomic operations?
#define PPTR_INODE_INCIDX(p)    ((p)->pin_index++)

#define PPTR_INODE_CIDXM(p, idx)  ((p)->pin_entries[(idx)].current.maddr)
#define PPTR_INODE_CIDXL(p, idx)  ((p)->pin_entries[(idx)].current.laddr)

#define PPTR_INODE_SIDXM(p, idx)  ((p)->pin_entries[(idx)].snapshot.maddr)
#define PPTR_INODE_SIDXL(p, idx)  ((p)->pin_entries[(idx)].snapshot.laddr)

#define PPTR_INODE_CMADDR(p)    ((p)->pin_entries[(p)->pin_current_idx].current.maddr)
#define PPTR_INODE_CLADDR(p)    ((p)->pin_entries[(p)->pin_current_idx].current.laddr)
#define PPTR_INODE_SMADDR(p)    ((p)->pin_entries[(p)->pin_current_idx].snapshot.maddr)
#define PPTR_INODE_SLADDR(p)    ((p)->pin_entries[(p)->pin_current_idx].snapshot.laddr)
#define PPTR_INODE_CLAST(p)      ((p)->pin_entries[PPTR_INODE_ENTRIES - 1].current.maddr)

struct pptr {
    uint16_t ploc_offset;
    uint32_t ploc_seid;
    uint16_t pval_offset;
    uint32_t pval_seid;
};

struct pptr_dnode {  ///< persistent data nodes (contain metadata for persistent pointers)
    int pdn_index;
    struct pptr pdn_entries[PPTR_DNODE_ENTRIES];
};

struct pptr_inode { ///< persistent pointer list indirection node
    int pin_current_idx;    ///< next empty link for current data nodes
    struct {
        struct pptr_inode *maddr;
        size_t laddr;
    } pin_next;
    struct {
        struct {
            struct pptr_dnode *maddr;
            size_t laddr;
        } current;
        struct {
            struct pptr_dnode *maddr;
            size_t laddr;
        } snapshot;
    } pin_entries[PPTR_INODE_ENTRIES]; ///< pointers to data nodes
};

struct pptr_head {  ///< persistent pointer list head
    struct {
        struct pptr_inode *maddr;
        size_t laddr;   ///< updating this values must be atomic
    } ph_first;
    struct {
        struct pptr_inode *maddr;
    } ph_last;
};

void pptr_init(unsigned int cid, struct pptr_head *head);
void pptr_append(unsigned int cid, struct pptr_head *head,
                 unsigned int ploc_seid, uint16_t ploc_offset,
                 unsigned int pval_seid, uint16_t pval_offset);
void pptr_cpoint(unsigned int cid, struct pptr_head *head);
void pptr_map(unsigned int cid, struct pptr_head *head, int type);
void pptr_print(struct pptr_head *head); ///< for debugging

#endif /* end of include guard: FIXPTR_H */
