#ifndef STATS_H
#define STATS_H

#include "settings.h"
#include <stdint.h>

struct global_stats {
    struct {
        uint64_t cont_grow;
        uint64_t cow_data_pg;
        uint64_t cow_meta_pg;
        uint64_t faults;
        uint64_t alloc_cont_pg;
        uint64_t free_cont_pg;
        uint64_t pallocations;
        uint64_t cpu_cache_flushes;
        uint64_t memprotects;

        /* keeps track of when a new metadata node of the slab is init */
        uint64_t se_init;
        uint64_t sb_init;
        uint64_t so_init;
        uint64_t si_init;
        uint64_t sd_init;
    } per_transaction;

    struct {
        uint64_t transactions;

        uint64_t cont_grow;
        uint64_t cow_data_pg;
        uint64_t cow_meta_pg;
        uint64_t faults;
        uint64_t alloc_cont_pg;
        uint64_t free_cont_pg;
        uint64_t pallocations;
        uint64_t cpu_cache_flushes;
        uint64_t memprotects;

        /* keeps track of when a new metadata node of the slab is init */
        uint64_t se_init;
        uint64_t sb_init;
        uint64_t so_init;
        uint64_t si_init;
        uint64_t sd_init;
    } general;

    int init;
};

extern struct global_stats GLOBAL_STATS;

char *stats_pt_report();
char *stats_general_report();

#define STATS_IS_INIT() GLOBAL_STATS.init

#ifdef STATS_ENABLED

#define __INC_BOTH(name) do {\
    GLOBAL_STATS.per_transaction.name++; \
    GLOBAL_STATS.general.name++; \
} while(0)


/*
 * If this is the first fault of the transaction, then we also inc the
 * transaction counter
 */
#define STATS_INC_FAULTS() do { \
    if (GLOBAL_STATS.per_transaction.faults == 0) \
        GLOBAL_STATS.general.transactions++; \
    __INC_BOTH(faults); \
} while(0)

#define STATS_RESET_TRANSACTION_COUNTERS() do { \
    GLOBAL_STATS.per_transaction.cont_grow = 0; \
    GLOBAL_STATS.per_transaction.cow_data_pg = 0; \
    GLOBAL_STATS.per_transaction.cow_meta_pg = 0; \
    GLOBAL_STATS.per_transaction.faults = 0; \
    GLOBAL_STATS.per_transaction.alloc_cont_pg = 0; \
    GLOBAL_STATS.per_transaction.free_cont_pg = 0; \
    GLOBAL_STATS.per_transaction.pallocations = 0; \
    GLOBAL_STATS.per_transaction.cpu_cache_flushes = 0; \
    GLOBAL_STATS.per_transaction.memprotects = 0; \
} while (0)

#define STATS_INC_CONTGROW()        __INC_BOTH(cont_grow)
#define STATS_INC_PALLOCATIONS()    __INC_BOTH(pallocations)
#define STATS_INC_COWDATA()         __INC_BOTH(cow_data_pg)
#define STATS_INC_COWMETA()         __INC_BOTH(cow_meta_pg)
#define STATS_INC_FLUSH()           __INC_BOTH(cpu_cache_flushes)
#define STATS_INC_ALLOCPG()         __INC_BOTH(alloc_cont_pg)
#define STATS_INC_FREEPG()          __INC_BOTH(free_cont_pg)
#define STATS_INC_MPROTECT()        __INC_BOTH(memprotects)

/*
 * se_init gives us the number of data pages, while the sum of s[boid]_init
 * gives of the number of metadata pages
 */
#define STATS_INC_SEINIT()          __INC_BOTH(se_init)
#define STATS_INC_SBINIT()          __INC_BOTH(sb_init)
#define STATS_INC_SOINIT()          __INC_BOTH(so_init)
#define STATS_INC_SIINIT()          __INC_BOTH(si_init)
#define STATS_INC_SDINIT()          __INC_BOTH(sd_init)


#else /* STATS_ENABLED */

/* here we make all operations no-op */

#define STATS_INC_FAULTS()
#define STATS_RESET_TRANSACTION_COUNTERS()

#define STATS_INC_CONTGROW()
#define STATS_INC_PALLOCATIONS()
#define STATS_INC_COWDATA()
#define STATS_INC_COWMETA()
#define STATS_INC_FLUSH()
#define STATS_INC_ALLOCPG()
#define STATS_INC_FREEPG()
#define STATS_INC_MPROTECT()

#define STATS_INC_SEINIT()
#define STATS_INC_SBINIT()
#define STATS_INC_SOINIT()
#define STATS_INC_SIINIT()
#define STATS_INC_SDINIT()

#endif /* end of STATS_ENABLED */

#endif /* end of include guard: STATS_H */
