#include <stdio.h>
#include "stats.h"

struct global_stats GLOBAL_STATS = {0};

#define PT_TEMPLATE "Transaction %lu {\n" \
    "   cont_grow: %lu\n" \
    "   cow_data_pg: %lu\n" \
    "   cow_meta_pg: %lu\n" \
    "   faults: %lu\n" \
    "   alloc_cont_pg: %lu\n" \
    "   free_cont_pg: %lu\n" \
    "   pallocations: %lu\n" \
    "   cpu_cache_flushes: %lu\n" \
    "   memprotects: %lu\n" \
    "   se_init: %lu\n" \
    "   sb_init: %lu\n" \
    "   so_init: %lu\n" \
    "   si_init: %lu\n" \
    "   sd_init: %lu\n" \
    "}"

char *stats_pt_report()
{
#ifdef STATS_ENABLED
    static char msg[512];
    sprintf(msg, PT_TEMPLATE,
        GLOBAL_STATS.general.transactions,
        GLOBAL_STATS.per_transaction.cont_grow,
        GLOBAL_STATS.per_transaction.cow_data_pg,
        GLOBAL_STATS.per_transaction.cow_meta_pg,
        GLOBAL_STATS.per_transaction.faults,
        GLOBAL_STATS.per_transaction.alloc_cont_pg,
        GLOBAL_STATS.per_transaction.free_cont_pg,
        GLOBAL_STATS.per_transaction.pallocations,
        GLOBAL_STATS.per_transaction.cpu_cache_flushes,
        GLOBAL_STATS.per_transaction.memprotects,
        GLOBAL_STATS.per_transaction.se_init,
        GLOBAL_STATS.per_transaction.sb_init,
        GLOBAL_STATS.per_transaction.so_init,
        GLOBAL_STATS.per_transaction.si_init,
        GLOBAL_STATS.per_transaction.sd_init
    );
    return msg;
#else
    return "Stats are not enabled!";
#endif
}

#define GENERAL_TEMPLATE "General {\n" \
    "   transactions: %lu\n" \
    "   cont_grow: %lu\n" \
    "   cow_data_pg: %lu\n" \
    "   cow_meta_pg: %lu\n" \
    "   faults: %lu\n" \
    "   alloc_cont_pg: %lu\n" \
    "   free_cont_pg: %lu\n" \
    "   pallocations: %lu\n" \
    "   cpu_cache_flushes: %lu\n" \
    "   memprotects: %lu\n" \
    "   se_init: %lu\n" \
    "   sb_init: %lu\n" \
    "   so_init: %lu\n" \
    "   si_init: %lu\n" \
    "   sd_init: %lu\n" \
    "}"

char *stats_general_report()
{
#ifdef STATS_ENABLED
    static char msg[512];
    sprintf(msg, GENERAL_TEMPLATE,
        GLOBAL_STATS.general.transactions,
        GLOBAL_STATS.general.cont_grow,
        GLOBAL_STATS.general.cow_data_pg,
        GLOBAL_STATS.general.cow_meta_pg,
        GLOBAL_STATS.general.faults,
        GLOBAL_STATS.general.alloc_cont_pg,
        GLOBAL_STATS.general.free_cont_pg,
        GLOBAL_STATS.general.pallocations,
        GLOBAL_STATS.general.cpu_cache_flushes,
        GLOBAL_STATS.general.memprotects,
        GLOBAL_STATS.general.se_init,
        GLOBAL_STATS.general.sb_init,
        GLOBAL_STATS.general.so_init,
        GLOBAL_STATS.general.si_init,
        GLOBAL_STATS.general.sd_init
    );
    return msg;
#else
    return "Stats are not enabled!";
#endif
}
