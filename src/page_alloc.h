#ifndef PAGE_ALLOC_H
#define PAGE_ALLOC_H

#include <stdlib.h>
#include <sys/mman.h>

#define PA_PROT_READ    PROT_READ
#define PA_PROT_WRITE   PROT_WRITE
#define PA_PROT_RNW     (PROT_READ|PROT_WRITE)

struct page_allocator_ops {
    const char *name;
    void* (*init)();
    int (*shutdown)(void *);
    void* (*alloc_page)(void*, size_t*, int);
    void* (*alloc_pages)(void*, size_t, size_t*, int);
    void (*free_pages)(void*, void*);
    void* (*map_page)(void*, size_t);
    void (*swap_page_mapping)(void*, void*, size_t, void*, size_t);
    void (*protect_page)(void*, size_t, int);
};

struct page_allocator {
    void *pa_handler;
    struct page_allocator_ops *pa_ops;
};

struct page_allocator *page_allocator_init(unsigned int cid);
int page_allocator_shutdown(unsigned int cid);

void *page_allocator_getpage(unsigned int cid, size_t *laddr, int flags);
void *page_allocator_getpages(unsigned int cid, int npages, size_t *laddr, int flags);
void page_allocator_freepages(unsigned int cid, void *maddr);
void *page_allocator_mappage(unsigned int cid, size_t laddr);
void page_allocator_init_complete(unsigned int cid);
void page_allocator_swap_mappings(unsigned int cid, void *xaddr, size_t ypgoff, void *yaddr, size_t xpgoff);
void page_allocator_mprotect(unsigned int cid, void *maddr, size_t size, int flags);

void page_allocator_mprotect_generic(void *maddr, size_t size, int flags);

#endif /* end of include guard: PAGE_ALLOC_H */
