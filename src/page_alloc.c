#include "page_alloc.h"
#include "settings.h"
#include "macros.h"
#include "stats.h"
#include "out.h"

struct page_allocator *PAGE_ALLOCATORS[CONTAINER_CNT] = {0};

/* forward declarations of page allocator implementations */
struct page_allocator_ops *fixed_mapper_ops();
struct page_allocator_ops *nonlinear_mapper_ops();

static void set_page_allocator_ops(struct page_allocator *pa)
{
    char *ptr = NULL;
    int val = -1;

    ptr = getenv("PMLIB_USE_NLMAPPER");
    if (ptr) {
        val = atoi(ptr);
        if (val == 1) {
            pa->pa_ops = nonlinear_mapper_ops();
            goto out;
        }
    }

    // this is the default page allocator (PMLIB_USE_FMAPPER)
    pa->pa_ops = fixed_mapper_ops();

out:
    LOG(5, "%s has been set", pa->pa_ops->name);
}

struct page_allocator *page_allocator_init(unsigned int cid)
{
    struct page_allocator *pa;
    char *ptr;

    pa = calloc(1, sizeof(struct page_allocator));
    if (!pa)
        handle_error("failed to init page allocator\n");

    set_page_allocator_ops(pa);
    pa->pa_handler = pa->pa_ops->init();
    PAGE_ALLOCATORS[cid] = pa;

    return pa;
}

void *page_allocator_getpage(unsigned int cid, size_t *laddr, int flags)
{
    STATS_INC_ALLOCPG();
    struct page_allocator *pa;
    pa = get_page_allocator(cid);
    return pa->pa_ops->alloc_page(pa->pa_handler, laddr, flags);
}

void *page_allocator_getpages(unsigned int cid, int npages, size_t *laddr, int flags)
{
    struct page_allocator *pa;
    pa = get_page_allocator(cid);
    return pa->pa_ops->alloc_pages(pa->pa_handler, npages, laddr, flags);
}

void page_allocator_freepages(unsigned int cid, void *maddr)
{
    STATS_INC_FREEPG();
    struct page_allocator *pa;
    pa = get_page_allocator(cid);
    pa->pa_ops->free_pages(pa->pa_handler, maddr);
}

int page_allocator_shutdown(unsigned int cid)
{
    struct page_allocator *pa;
    pa = get_page_allocator(cid);
    return pa->pa_ops->shutdown(pa->pa_handler);
}

void *page_allocator_mappage(unsigned int cid, size_t laddr)
{
    struct page_allocator *pa;
    pa = get_page_allocator(cid);
    return pa->pa_ops->map_page(pa->pa_handler, laddr);
}

void page_allocator_swap_mappings(unsigned int cid, void *xaddr, size_t ypgoff, void *yaddr, size_t xpgoff)
{
    struct page_allocator *pa;
    pa = get_page_allocator(cid);
    pa->pa_ops->swap_page_mapping(pa->pa_handler, xaddr, ypgoff, yaddr, xpgoff);
}

void page_allocator_mprotect(unsigned int cid, void *maddr, size_t size, int flags)
{
    struct page_allocator *pa;
    pa = get_page_allocator(cid);
    pa->pa_ops->protect_page(maddr, size, flags);
}

void page_allocator_mprotect_generic(void *maddr, size_t size, int flags)
{
    STATS_INC_MPROTECT();

    LOG(50, "Protecting page at location %p with R:%d W:%d",
            maddr,
            flags & PA_PROT_READ ? 1 : 0,
            flags & PA_PROT_WRITE ? 1 : 0);

    if (mprotect(maddr, size, flags))
        handle_error("failed to memprotect page\n");
}
