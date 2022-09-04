#include <stdlib.h>
#include <sys/mman.h>

#include "cont.h"
#include "out.h"
#include "settings.h"
#include "macros.h"
#include "slab.h"
#include "sfhandler.h"
#include "page_alloc.h"
#include "atomics.h"

/**
 * This array contains pointer to the actual containers. There is fixed numbers
 * of containers
 */
struct container * CONTAINERS[CONTAINER_CNT] = { NULL };

static int container_getid()
{
    int i;

    for (i = 0; i < CONTAINER_CNT; i++) {
        if (CONTAINERS[i] == NULL)
           return i;
    }

    return -1;
}

static void dont_compute_closure(unsigned int cid) { }

static void compute_closure(unsigned int cid)
{
    /*
     * TODO: with this implementation, we could get duplicates on the
     * persistent list of persistent pointers. This shold not affect
     * correctness, but it should be fixed!
     */
    LOG(10, "Computing closure");
    build_mallocat_tree();
    classify_pointers(cid);
    move_volatile_allocations(cid);
    fix_back_references();
}

static void (*Func_compute_closure)(unsigned int cid) = compute_closure;

/*
 * Called automatically by the run-time loader.
 */
__attribute__((constructor))
static void init(void)
{
    out_init(PMLIB_LOG_PREFIX, PMLIB_LOG_LEVEL_VAR, PMLIB_LOG_FILE_VAR,
            PMLIB_MAJOR_VERSION, PMLIB_MINOR_VERSION);
    LOG(3, NULL);

    register_sigsegv_handler();

    char *ptr = getenv("PMLIB_CLOSURE");
    if (ptr) {
        int val = atoi(ptr);
        if (val == 0) {
            Func_compute_closure = dont_compute_closure;
            LOG(3, "Container closure has been disabled");
        }
    } else {
        Func_compute_closure = compute_closure;
        LOG(3, "Container closure is enabled");
    }

    slab_init();
}

struct container *container_init()
{
    struct container *cont;
    struct page_allocator *pallocator;
    //TODO: handle the case of multiple containers
    int cid = container_getid();
    size_t cont_laddr;

    pallocator = page_allocator_init(cid);
    cont = page_allocator_getpage(cid, &cont_laddr, PA_PROT_WRITE);
    if (!cont)
        handle_error("failed to allocate memory for container\n");
    if (cont_laddr != CONTAINER_LIMA_ADDRESS)
        handle_error("failed to get the expected laddr for container\n");

    closure_init();

    CONTAINERS[cid] = cont;
    cont->id = cid;
    cont->pg_allocator = pallocator;
    cont->current_slab.maddr = slab_dir_init(cid, &cont->current_slab.laddr);
    cont->snapshot_slab.maddr = cont->current_slab.maddr;
    cont->snapshot_slab.laddr = cont->current_slab.laddr;

    return cont;
}

void *container_palloc(unsigned int cid, unsigned int size)
{
    return slab_palloc(cid, size);
}

static void container_compute_closure(unsigned int cid)
{
    Func_compute_closure(cid);
}

static void container_cpoint_aux(unsigned int cid, int type)
{
    struct container *cont;
    struct ptrat *pat, *pat_temp;
    cont = get_container(cid);

    //TODO: make sure that all changes to PM up to this point are durable
    atomic_set_flag(cont->flags, CFLAG_CPOINT_IN_PROGRESS);

    container_compute_closure(cid);

    slab_cpoint(cid, type);

    if (cont->current_slab.maddr != cont->snapshot_slab.maddr) {
        atomic_set(&cont->snapshot_slab.laddr, cont->current_slab.laddr);
        page_allocator_freepages(cont->id, cont->snapshot_slab.maddr);
        cont->snapshot_slab.maddr = cont->current_slab.maddr;
    }

    //TODO: make sure that all changes to PM up to this point are durable
    atomic_clear_flag(cont->flags, CFLAG_CPOINT_IN_PROGRESS);
}

void container_cpoint(unsigned int cid)
{
    LOG(10, "Starting a new checkpoint");

    container_cpoint_aux(cid, CPOINT_REGULAR);

    LOG(8, stats_pt_report());
    LOG(8, stats_general_report());
    STATS_RESET_TRANSACTION_COUNTERS();
}

struct container *container_restore(unsigned int cid)
{
    struct container *cont;
    struct page_allocator * pallocator;

    pallocator = page_allocator_init(cid);
    cont = page_allocator_mappage(cid, CONTAINER_LIMA_ADDRESS);
    CONTAINERS[cid] = cont;
    cont->pg_allocator = pallocator;

    closure_init();

    //TODO: make sure that all changes to PM up to this point are durable

    if (test_flag(cont->flags, CFLAG_CPOINT_IN_PROGRESS)) {
        cont->current_slab.maddr = slab_map(cid, cont->current_slab.laddr, CPOINT_INCOMPLETE);

        if (cont->current_slab.laddr != cont->snapshot_slab.laddr)
            cont->snapshot_slab.maddr = slab_map(cid, cont->snapshot_slab.laddr, CPOINT_INCOMPLETE);

        container_cpoint_aux(cid, CPOINT_RESTORE);

        //TODO: make sure that all changes to PM up to this point are durable
        atomic_clear_flag(cont->flags, CFLAG_CPOINT_IN_PROGRESS);
    } else {
        //TODO: think abuot what should be atomic here
        cont->current_slab.maddr = slab_map(cid, cont->snapshot_slab.laddr, CPOINT_COMPLETE);
        cont->snapshot_slab.maddr = cont->current_slab.maddr;
        cont->current_slab.laddr = cont->snapshot_slab.laddr;
    }

    slab_fixptrs(cid);
    slab_mprotect_datapgs(cid, PROT_READ);

    register_sigsegv_handler();
    return cont;
}

size_t container_setroot(unsigned int cid, void *maddr)
{
    return slab_setroot(cid, maddr);
}

void *container_getroot(unsigned int cid)
{
    return slab_getroot(cid);
}
