#ifndef ATOMICS_H
#define ATOMICS_H

#include "stats.h"

#define atomic_set_flag(bitarray, flag) do { \
    ((bitarray) |= (flag)); \
    volatile void *addr = &(bitarray); \
    flush(addr, 1); \
} while (0)

#define atomic_clear_flag(bitarray, flag) do { \
    ((bitarray) &= ~(flag)); \
    volatile void *addr = &(bitarray); \
    flush(addr, 1); \
} while (0)

#define atomic_set(ptr, val) do { \
    volatile uint64_t *addr = ptr; \
    *addr = val; \
    flush(addr, 1); \
} while (0)

#define simflush_fence(addr) do {\
    STATS_INC_FLUSH(); \
    unsigned long long tmp = 0; \
    asm volatile ("mov %0, %1\n\t" \
                  "movnti %1, %0\n\t" \
                  "sfence" : /* no output */ : "m" (addr), "r" (tmp)); \
} while(0)

#define simflush(addr) do {\
    STATS_INC_FLUSH(); \
    unsigned long long tmp = 0; \
    asm volatile ("mov %0, %1\n\t" \
                  "movnti %1, %0" : /* no output */ : "m" (addr), "r" (tmp)); \
} while (0)

#define flush(addr, fence) do {\
    STATS_INC_FLUSH(); \
    asm volatile ("clflush %0" : /* no output */ : "m" (addr)); \
    if (fence) \
        asm volatile ("sfence"); \
} while(0)

void *pmemcpy(void *dest, const void *src, size_t n);
void flush_memsegment(const void *src, size_t n, int fence);

#endif /* end of include guard: ATOMICS_H */
