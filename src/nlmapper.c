#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <queue.h>
#include <tree.h>
#include "out.h"
#include "stats.h"
#include "macros.h"
#include "page_alloc.h"

#define DEFAULT_MAPPING_PROT    (PA_PROT_READ)

/*
 * This implementattion of a page allocator uses non-linear mappins.
 *
 * It depends on the remap_file_pages system call which is not available in the
 * Linux kernel starting with version 4.0.
 */

#define DOUBLE_FILE_SIZE(p) ((p)->file_size * 2)
#define UPDATE_FREE_LIST        1
#define DONT_UPDATE_FREE_LIST   0

#define MARK_PAGE_AS_FREE(p) do { \
    (p)->is_free = 1; \
    (p)->is_use = 0; \
} while (0);

#define MARK_PAGE_AS_INUSE(p) do { \
    assert((p)->is_free && "Invalid page status"); \
    (p)->is_free = 0; \
    (p)->is_use = 1; \
} while (0);

static char cont_file_name[128];

struct nlm_page {
    size_t pgoff;
    void *addr;

    // flags
    unsigned int is_free : 1;
    unsigned int is_use : 1;
    unsigned int is_nonlinear : 1;

    union {
        LIST_ENTRY(nlm_page) free;
        RB_ENTRY(nlm_page) inuse;
    } index;
};

struct nlm {
    int fd;
    size_t file_size;
    size_t prev_file_size;
    void *start_addr;
    int is_nonlinear; //becomes true after the first swap
    int is_fully_mapped; // 1 when the entire file is mapped, 0 otherwise

    size_t pgcnt;
    struct {
        uint64_t size;
        LIST_HEAD(free_list_head, nlm_page) head;
        struct nlm_page *last;
    } free_list; // all free pages are here

    RB_HEAD(nlm_tree, nlm_page) root; // all in_use pages are here
};

// forward declaration
void *map_hint(size_t len);

int nlm_page_comare_addr(struct nlm_page *a, struct nlm_page *b)
{
    return (a->addr < b->addr ? -1 : a->addr > b->addr);
}
RB_GENERATE(nlm_tree, nlm_page, index.inuse, nlm_page_comare_addr);

/*
 * Add page @p to the free list
 * If update_in_used == 1, then the page comes from the in_use tree
 */
static void __nlm_free_page(struct nlm *nlm, struct nlm_page *p, int update_in_used)
{
    if (update_in_used) {
        assert(p->is_use && "Invalid page status");
        RB_REMOVE(nlm_tree, &nlm->root, p);
    }

    if (nlm->free_list.size == 0) {
        LIST_INSERT_HEAD(&nlm->free_list.head, p, index.free);
        assert(nlm->free_list.last == NULL && "Invalid last pointer");
    } else
        LIST_INSERT_AFTER(nlm->free_list.last, p, index.free);

    nlm->free_list.last = p;
    nlm->free_list.size++;
    MARK_PAGE_AS_FREE(p);
}

/*
 * Move page from the free list to the in_use tree and update metadata to
 * reflect this change.
 */
static void __nlm_use_page(struct nlm *nlm, struct nlm_page *p)
{
    assert(p->is_free && "invalid page status");
    LIST_REMOVE(p, index.free);
    nlm->free_list.size--;

    if (!nlm->free_list.size)
        nlm->free_list.last = NULL;

    RB_INSERT(nlm_tree, &nlm->root, p);
    MARK_PAGE_AS_INUSE(p);
}

/*
 * Return 1 if pg is swapped (non-linearly mapped)
 */
static inline int is_nonlinear(struct nlm *nlm, struct nlm_page *pg)
{
    return (pg->addr - nlm->start_addr) != pg->pgoff;
}

struct nlm_page *nlm_page_alloc(struct nlm* nlm, void *addr, size_t pgoff)
{
    struct nlm_page *p = calloc(1, sizeof(*p));
    assert(p && "Failed to allocated memory for page");
    p->pgoff = pgoff;
    p->addr = addr;
    nlm->pgcnt++;
    LOG(50, "Creating new page {add: %p, pgoff: %lu}", p->addr, p->pgoff);
    return p;
}

static void add_pages_to_free_list(struct nlm *nlm)
{
    LOG(5, "Updating nlmapping free list");

    size_t pgs = (nlm->file_size - nlm->prev_file_size) / PAGE_SIZE;
    struct nlm_page *p = NULL;

   for (uint64_t i = 0; i < pgs; i++) {
        void *iaddr = nlm->start_addr + nlm->prev_file_size + i * PAGE_SIZE;
        size_t ioffset = nlm->prev_file_size + i * PAGE_SIZE;
        p = nlm_page_alloc(nlm, iaddr, ioffset);
        __nlm_free_page(nlm, p, /*update_in_used=*/0);
        LOG(40, "New page in free list {addr: %p, offset: %lu} list_size: %lu",
                p->addr, p->pgoff, nlm->free_list.size);
    }
}

static void remap_entire_file(struct nlm *nlm, int update_free_list)
{
    void *hint;

    assert(nlm->is_fully_mapped == 0 && "Trying to map an fully mapped file");

    if (nlm->start_addr == 0)
        hint = map_hint(nlm->file_size);
    else {
        hint = nlm->start_addr;
        munmap(hint, nlm->file_size);
    }
    assert(hint && "Could not find a big-enough region");

    void *addr = mmap(hint, nlm->file_size, DEFAULT_MAPPING_PROT, MAP_SHARED, nlm->fd, 0);
    assert(addr == hint && "Could not mapped at the hinted address");

    if (!nlm->start_addr)
        nlm->start_addr = addr;

    if (update_free_list)
        add_pages_to_free_list(nlm);

    nlm->is_fully_mapped = 1;

    //TODO: here we need to recreate all the previously existing swaps
    // foreach swap_page:
    //      swap_mappings_as_before
}

static void map_appending(struct nlm *nlm, int update_free_list)
{
    assert(nlm->is_fully_mapped == 0 && "Trying to map an fully mapped file");
    void *hint;

    if (nlm->start_addr == 0)
        hint = map_hint(nlm->file_size);
    else {
        hint = nlm->start_addr + nlm->prev_file_size;
    }

    size_t length = nlm->file_size - nlm->prev_file_size;
    void *addr = mmap(hint, length, DEFAULT_MAPPING_PROT, MAP_SHARED, nlm->fd, nlm->prev_file_size);
    assert(addr == hint && "Could not mmap at the hinted address");

    if (!nlm->start_addr)
        nlm->start_addr = addr;

    if (update_free_list)
        add_pages_to_free_list(nlm);

    nlm->is_fully_mapped = 1;
}

void (*Func_map_file)(struct nlm *nlm, int update_free_list) = map_appending;

void nlm_grow_file(struct nlm *nlm, size_t new_size)
{
    assert(nlm->free_list.size == 0 && "growing file while with free pages");
    LOG(5, "Growing container file from %lu (pgs: %lu) to %lu (pgs: %lu)",
            nlm->file_size, nlm->file_size/PAGE_SIZE, new_size, new_size/PAGE_SIZE);
    STATS_INC_CONTGROW();

    int rc = posix_fallocate(nlm->fd, nlm->file_size, new_size);
    switch (rc) {
        case 0: break;
        case EBADF  : handle_error("fd is not a valid file descriptor, or is not opened for writing.\n");
        case EFBIG  : handle_error("offset+len exceeds the maximum file size.\n");
        case EINVAL : handle_error("offset was less than 0, or len was less than or equal to 0.\n");
        case ENODEV : handle_error("fd does not refer to a regular file.\n");
        case ENOSPC : handle_error("There is not enough space left on the device containing the file referred to by fd.\n");
        case ESPIPE : handle_error("fd refers to a pipe.\n");
    }

    nlm->prev_file_size = nlm->file_size;
    nlm->file_size = new_size;
    nlm->is_fully_mapped = 0;
}

void *nlm_alloc_pages(void *handler, size_t n, size_t *laddr, int flags)
{
    handle_error("not implemented yet");
    return NULL;
}

void *nlm_alloc_page(void *handler, size_t *laddr, int flags)
{
    struct nlm *h = (struct nlm*) handler;
    struct nlm_page *p;

    if (h->free_list.size == 0) {
        nlm_grow_file(h, DOUBLE_FILE_SIZE(h));
        Func_map_file(h, UPDATE_FREE_LIST);
    }

    p = LIST_FIRST(&h->free_list.head);
    __nlm_use_page(h, p);
    LOG(25, "Allocing new page {addr: %p, offset: %lu, pgno: %lu}",
            p->addr, p->pgoff, p->pgoff/PAGE_SIZE);

    if (laddr)
        *laddr = p->pgoff;

    if (flags & PA_PROT_WRITE)
        page_allocator_mprotect_generic(p->addr, PAGE_SIZE, flags);

    return p->addr;
}

void nlm_free_pages(void *handler, void *addr)
{
    struct nlm *h = (struct nlm*)handler;
    void *addr_paligned = itop(ROUND_DWNPG(ptoi(addr)));
    struct nlm_page *p = NULL;
    struct nlm_page key = { .addr = addr_paligned };
    p = RB_FIND(nlm_tree, &h->root, &key);
    if (p) {
        __nlm_free_page(h, p, /*update_in_used=*/ 1);
    } else {
        if (addr < h->start_addr || addr >= h->start_addr + h->file_size)
            handle_error("trying to free a page outside the map regions");
        else
            handle_error("page not found!");
    }
}

void *nlm_get_address(void *handler, size_t laddr)
{
    struct nlm *h = (struct nlm*) handler;
    void *maddr = h->start_addr + laddr;
    return maddr;
}

int nlm_close(void *handler)
{
    struct nlm *h = (struct nlm*) handler;
    struct nlm_page *p = NULL;

    assert(h->pgcnt == (h->file_size / PAGE_SIZE) && "Not all pages were allocated");

    while (!RB_EMPTY(&h->root)) {
        p = RB_ROOT(&h->root);
        assert(p->is_use && "invalid page status");
        RB_REMOVE(nlm_tree, &h->root, p);
        LOG(50, "Freeing page {addr: %p, pgoff: %lu, pgno: %lu}", p->addr, p->pgoff, p->pgoff/PAGE_SIZE);
        free(p);
        h->pgcnt--;
    }

    while (h->free_list.size) {
        p = LIST_FIRST(&h->free_list.head);
        assert(p->is_free && "invalid page status");
        LIST_REMOVE(p, index.free);
        LOG(50, "Freeing page {addr: %p, pgoff: %lu, pgno: %lu}", p->addr, p->pgoff, p->pgoff/PAGE_SIZE);
        free(p);
        h->free_list.size--;
        h->pgcnt--;
    }

    assert(h->pgcnt == 0 && "Failed to free all pages");

    return 0;
}

void *nlm_init()
{
    LOG(3, "Initializing nonlinear-mapper");
    int cid = 0;
    struct stat stat;
    struct nlm *nlm = calloc(1, sizeof(*nlm));
    char *ptr;

    assert(nlm && "Failed to allocate memory");
    LIST_INIT(&nlm->free_list.head);
    RB_INIT(&nlm->root);

    ptr = getenv("PMLIB_CONT_FILE");
    if (ptr) {
        LOG(3, "Setting container path to %s", ptr);
        sprintf(cont_file_name, "%s%d", ptr, cid);
    } else {
        sprintf(cont_file_name, "%s%d", FM_FILE_NAME_PREFIX, cid);
    }

    nlm->fd = open(cont_file_name, O_CREAT | O_RDWR, 0645);
    if (nlm->fd == -1)
        handle_error("fixed mapper failed to open file\n");

    fstat(nlm->fd, &stat);
    nlm->file_size = stat.st_size;

    if (nlm->file_size == 0) {
        ptr = getenv("PMLIB_INIT_SIZE");
        uint64_t file_size = FM_FILE_SIZE;

        if (ptr) {
            file_size = atoll(ptr);
            file_size = MAX(ROUNDPG(file_size), PAGE_SIZE);
            LOG(3, "Container init size has been set to %lu", file_size);
        }

        nlm_grow_file(nlm, file_size);
    }

    Func_map_file(nlm, UPDATE_FREE_LIST);
    LOG(3, "Mapping file for the first time at location %p", nlm->start_addr);
    return (void*)nlm;
}

void nlm_noope(void *handler) {}

/*
 * In-use pages are indexed by addr in a RB-Tree.
 * This functions copies all updates all the other metadata of the page so what
 * it's correct after a two pages have been swapped.
 *
 * NOTE: we cannot change the addr becuase it's the index of the RB-Tree
 */
static void update_swapped_pages(struct nlm *nlm, void *xaddr, void *yaddr)
{
    struct nlm_page *xpg = NULL;
    struct nlm_page *ypg = NULL;
    struct nlm_page xkey = { .addr = xaddr };
    xpg = RB_FIND(nlm_tree, &nlm->root, &xkey);
    if (xpg) {
        struct nlm_page ykey = { .addr = yaddr };
        ypg = RB_FIND(nlm_tree, &nlm->root, &ykey);
        if (ypg) {
            SWAP(size_t, xpg->pgoff, ypg->pgoff);
            xpg->is_nonlinear = is_nonlinear(nlm, xpg);
            ypg->is_nonlinear = is_nonlinear(nlm, ypg);
        }
    }
}

void nlm_swap_pages(void *handler, void *xaddr, size_t ypgoff, void *yaddr, size_t xpgoff)
{
    struct nlm *h = (struct nlm*) handler;
    int rc;

    rc = remap_file_pages(xaddr, PAGE_SIZE, /* prot = */0, ypgoff/PAGE_SIZE, MAP_SHARED);
    if (rc)
        handle_error("filed to remap pg_one");

    rc = remap_file_pages(yaddr, PAGE_SIZE, /* prot = */0, xpgoff/PAGE_SIZE, MAP_SHARED);
    if (rc)
        handle_error("filed to remap pg_one");

    if ((xaddr - h->start_addr) != xpgoff)
        h->is_nonlinear = 1;

    update_swapped_pages(h, xaddr, yaddr);

    LOG(25, "Page %lu (offset=%lu) is now mapped at address %p",
            xpgoff/PAGE_SIZE, xpgoff, yaddr);
    LOG(25, "Page %lu (offset=%lu) is now mapped at address %p",
            ypgoff/PAGE_SIZE, ypgoff, xaddr);
}

struct page_allocator_ops *nonlinear_mapper_ops()
{
    static struct page_allocator_ops ops = {
        .name = "Non-Linear Page Allocator",
        .init = nlm_init,
        .shutdown = nlm_close,
        .alloc_page = nlm_alloc_page,
        .alloc_pages = nlm_alloc_pages,
        .free_pages = nlm_free_pages,
        .map_page = nlm_get_address,
        .swap_page_mapping = nlm_swap_pages,
        .protect_page = page_allocator_mprotect_generic,
    };
    return &ops;
}

