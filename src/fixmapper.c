#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "page_alloc.h"
#include "settings.h"
#include "macros.h"
#include <queue.h>
#include "out.h"
#include "stats.h"

#define GIGABYTE    (1UL << 30)
#define GIGABYTE_MASK (GIGABYTE - 1)
#define ROUND2GB(x) (((x)+GIGABYTE_MASK)&~GIGABYTE_MASK)
#define TERABYTE    (1UL << 40)
#define PROCMAXLEN  2048

#define bytes2pgs(bytes) ((bytes) / PAGE_SIZE)

#define DEFAULT_MAPPING_PROT    (PA_PROT_READ)

char cont_file_name[128];

struct fixed_page {
    uint64_t pgno;
    int prot_flags;
    LIST_ENTRY(fixed_page) free;
};

struct fixed_page *fixed_page_alloc(uint64_t pgno)
{
    struct fixed_page *p = malloc(sizeof(*p));
    assert(p && "Failed to allocated memory for page");
    p->pgno = pgno;
    p->prot_flags = DEFAULT_MAPPING_PROT;
    return p;
}

void fixed_page_free(struct fixed_page *p)
{
    assert(p && "Trying to free NULL pointer");
    free(p);
    p = NULL;
}

struct fixed_mapper {
    int fd;             //file descriptor
    size_t file_size;   //file size in bytes
    void *start_addr;   //address at which the entire file is mapped
    struct {
        uint64_t size;
        LIST_HEAD(fixedpage_list_head, fixed_page) head;
        struct fixed_page *last;
    } free_list;                //keep track of all available pages
    struct fixed_page **index;  //keep track of all pages here
};

/*
 * map_hint -- (internal) use /proc to determine a hint address for mmap()
 *
 * This is a helper function for util_map().  It opens up /proc/self/maps
 * and looks for the first unused address in the process address space that is:
 * - greater or equal 1TB,
 * - large enough to hold range of given length,
 * - 1GB aligned.
 *
 * Asking for aligned address like this will allow the DAX code to use large
 * mappings.  It is not an error if mmap() ignores the hint and chooses
 * different address.
 */
void *map_hint(size_t len)
{
    FILE *fp;
    if ((fp = fopen("/proc/self/maps", "r")) == NULL) {
        handle_error("!/proc/self/maps");
    }

    char line[PROCMAXLEN];  /* for fgets() */
    char *lo = NULL;    /* beginning of current range in maps file */
    char *hi = NULL;    /* end of current range in maps file */
    char *raddr = (char *)TERABYTE; /* ignore regions below 1TB */

    while (fgets(line, PROCMAXLEN, fp) != NULL) {
        /* check for range line */
        if (sscanf(line, "%p-%p", &lo, &hi) == 2) {
            if (lo > raddr) {
                if (lo - raddr >= len) {
                    break;
                } else {
                    //region is too small
                }
            }

            if (hi > raddr) {
                /* check the next range align to 1GB */
                raddr = (char *)ROUND2GB((uintptr_t)hi);
            }

            if (raddr == 0) {
                break;
            }
        }
    }

    /*
     * Check for a case when this is the last unused range in the address
     * space, but is not large enough. (very unlikely)
     */
    if ((raddr != NULL) && (UINTPTR_MAX - (uintptr_t)raddr < len)) {
        raddr = NULL;
    }

    fclose(fp);

    return raddr;
}

static void update_mapping_prot(struct fixed_mapper *fm)
{
    size_t pgs = bytes2pgs(fm->file_size);
    struct fixed_page *p = NULL;
    void *addr = NULL;

    for (size_t i = 0; i < pgs; i++) {
        p = fm->index[i];
        addr = fm->start_addr + PAGE_SIZE * p->pgno;
        page_allocator_mprotect_generic(addr, PAGE_SIZE, p->prot_flags);
    }
}

static void fixed_mapper_grow_file(struct fixed_mapper *fm, uint64_t new_size)
{
    assert(fm->free_list.size == 0 && "growing file while with free pages");
    LOG(5, "Growing container file from %lu to %lu", fm->file_size, new_size);
    STATS_INC_CONTGROW();

    int rc = posix_fallocate(fm->fd, fm->file_size, new_size);
    switch (rc) {
        case 0: break;
        case EBADF  : handle_error("fd is not a valid file descriptor, or is not opened for writing.\n");
        case EFBIG  : handle_error("offset+len exceeds the maximum file size.\n");
        case EINVAL : handle_error("offset was less than 0, or len was less than or equal to 0.\n");
        case ENODEV : handle_error("fd does not refer to a regular file.\n");
        case ENOSPC : handle_error("There is not enough space left on the device containing the file referred to by fd.\n");
        case ESPIPE : handle_error("fd refers to a pipe.\n");
    }

    uint64_t current_size_pgs = bytes2pgs(fm->file_size);
    uint64_t new_size_pgs = bytes2pgs(new_size);
    struct fixed_page *p = NULL;

    fm->index = realloc(fm->index, sizeof(void*) * new_size_pgs);

    p = fm->index[current_size_pgs] = fixed_page_alloc(current_size_pgs);
    LIST_INSERT_HEAD(&fm->free_list.head, p, free);
    fm->free_list.last = p;
    fm->free_list.size++;
    current_size_pgs++;

    for (uint64_t i = current_size_pgs; i < new_size_pgs; i++) {
        p = fm->index[i] = fixed_page_alloc(i);
        LIST_INSERT_AFTER(fm->free_list.last, p, free);
        fm->free_list.size++;
        fm->free_list.last = p;
    }

    fm->file_size = new_size;
}

static void map_file(struct fixed_mapper *fm, int update_mappings)
{
    void *hint;

    if (fm->start_addr == 0)
        hint = map_hint(fm->file_size);
    else {
        hint = fm->start_addr;
        munmap(hint, fm->file_size);
    }
    assert(hint && "Could not find a big-enough region");

    void *addr = mmap(hint, fm->file_size, DEFAULT_MAPPING_PROT, MAP_SHARED, fm->fd, 0);
    assert(addr == hint && "Could not mapped at the hinted address");
    fm->start_addr = addr;

    if (update_mappings)
        update_mapping_prot(fm);
}

void *fixed_mapper_init()
{
    LOG(3, "Initializing fixed-mapper");
    int cid = 0;
    struct stat stat;
    struct fixed_mapper *fm = calloc(1, sizeof(*fm));
    char *ptr;

    assert(fm && "Failed to allocate memory");
    LIST_INIT(&fm->free_list.head);

    ptr = getenv("PMLIB_CONT_FILE");
    if (ptr) {
        LOG(3, "Setting container path to %s", ptr);
        sprintf(cont_file_name, "%s%d", ptr, cid);
    } else {
        sprintf(cont_file_name, "%s%d", FM_FILE_NAME_PREFIX, cid);
    }

    fm->fd = open(cont_file_name, O_CREAT | O_RDWR, 0645);
    if (fm->fd == -1)
        handle_error("fixed mapper failed to open file\n");

    fstat(fm->fd, &stat);
    fm->file_size = stat.st_size;

    if (fm->file_size == 0) {
        ptr = getenv("PMLIB_INIT_SIZE");
        uint64_t file_size = FM_FILE_SIZE;

        if (ptr) {
            file_size = atoll(ptr);
            file_size = MAX(ROUNDPG(file_size), PAGE_SIZE);
            LOG(3, "Container init size has been set to %lu", file_size);
        }

        fixed_mapper_grow_file(fm, file_size);
    }

    map_file(fm, /* dont update mappings */ 0);
    return (void*)fm;
}

int fixed_mapper_close(void *handler)
{
    struct fixed_mapper *h = (struct fixed_mapper*) handler;

    size_t size_pgs = bytes2pgs(h->file_size);
    for (size_t i = 0; i < size_pgs; ++i) {
        free(h->index[i]);
    }
    free(h->index);

    int ret = munmap(h->start_addr, h->file_size);
    assert(ret == 0 && "Failed to ummap the file");
    close(h->fd);
    return 0;
}

void *fixed_mapper_alloc_page(void *handler, size_t *laddr, int flags)
{
    struct fixed_mapper *h = (struct fixed_mapper*) handler;
    struct fixed_page *p;
    void *addr = NULL;

    if (h->free_list.size == 0) {
        fixed_mapper_grow_file(h, h->file_size * 2);
        map_file(h, /* update mappings */ 1);
    }

    p = LIST_FIRST(&h->free_list.head);
    LIST_REMOVE(p, free);
    h->free_list.size--;
    if (h->free_list.size == 0)
        h->free_list.last = NULL;

    addr = h->start_addr + (p->pgno * PAGE_SIZE);
    if (laddr)
        *laddr = p->pgno * PAGE_SIZE;

    if (flags & PA_PROT_WRITE)
        page_allocator_mprotect_generic(addr, PAGE_SIZE, flags);

    p->prot_flags = flags;

    return addr;
}

void *fixed_mapper_alloc_pages(void *handler, size_t n, size_t *laddr, int flags)
{
    handle_error("not implemnted yet");
    return NULL;
}

void fixed_mapper_freepages(void *handler, void *maddr)
{
    void *maddr_paligned = itop(ROUND_DWNPG(ptoi(maddr)));
    assert(maddr == maddr_paligned && "Address is not page aligned");

    struct fixed_mapper *h = (struct fixed_mapper*) handler;
    uint64_t pgno = (maddr - h->start_addr) / PAGE_SIZE;
    struct fixed_page *p = h->index[pgno];

    if (h->free_list.last)
        LIST_INSERT_AFTER(h->free_list.last, p, free);
    else
        LIST_INSERT_HEAD(&h->free_list.head, p, free);

    h->free_list.last = p;
    h->free_list.size++;
}

void *fixed_mapper_getaddress(void *handler, size_t laddr)
{
    struct fixed_mapper *h = (struct fixed_mapper*) handler;
    void *maddr = h->start_addr + laddr;
    return maddr;
}

void fixed_mapper_noope(void *handler)
{
    //nothing to do here!
}

void fixed_mapper_noswap(void *handler, void *xaddr, size_t ypgoff, void *yaddr, size_t xpgoff)
{
    handle_error("Fixed Mapper does not implement page mapping swap\n");
}

struct page_allocator_ops *fixed_mapper_ops()
{
    static struct page_allocator_ops ops = {
        .name = "Fixed Mapper Page Allocation",
        .init = fixed_mapper_init,
        .shutdown = fixed_mapper_close,
        .alloc_page = fixed_mapper_alloc_page,
        .alloc_pages = fixed_mapper_alloc_pages,
        .free_pages = fixed_mapper_freepages,
        .map_page = fixed_mapper_getaddress,
        .swap_page_mapping = fixed_mapper_noswap,
        .protect_page = page_allocator_mprotect_generic,
    };
    return &ops;
}

