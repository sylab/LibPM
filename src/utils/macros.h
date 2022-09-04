#ifndef MACROS_H
#define MACROS_H

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <bitstring.h>
#include <stdint.h>

#include "settings.h"

#define ULONG_PTR unsigned long int
#define CONTAINER_LIMA_ADDRESS 0    ///< container must be in the first page

#define ROUND8(x)       (((x)+7)&~7)
#define ROUND_DWN8(x)   ((x)&~7)

#define PAGE_MASK       (PAGE_SIZE - 1)
#define ROUNDPG(x)      (((x)+PAGE_MASK)&~PAGE_MASK)
#define ROUND_DWNPG(x)  ((x)&~PAGE_MASK)
#define PAGE_ROUND_DOWN(x) (((ULONG_PTR)(x)) & (~(PAGE_MASK)))

#define CACHE_LINE_SIZE_MASK    (CACHE_LINE_SIZE-1)
#define ROUND_UPCL(x)   (((x)+CACHE_LINE_SIZE_MASK)&~CACHE_LINE_SIZE_MASK)
#define ROUND_DWNCL(x)  ((x)&~CACHE_LINE_SIZE_MASK)

#define MIN(x,y)        ((x) < (y) ? x : y)
#define MAX(x,y)        ((x) > (y) ? x : y)
#define SWAP(TYPE,x,y)  {TYPE t=(x); (x)=(y); (y)=t;}

//TODO: make these macros assert if the id is invalid

extern struct container * CONTAINERS[CONTAINER_CNT];
#define get_container(id)   (CONTAINERS[(id)])
#define get_page_allocator(id) PAGE_ALLOCATORS[(id)]

#define bug()   assert(0)

#define handle_error(fmt, arg...) do { \
    fprintf(stderr, "[SOFTPM] %s:%s:%u: " fmt, __FILE__, __FUNCTION__, __LINE__, ##arg); \
    if (errno) \
        perror("[SOFTPM]"); \
    fflush(stderr); \
    bug(); \
} while (0)

#define set_page_allocator(name)    (& name ## _ops)

#define ptoi(p) (uint64_t)(p)
#define itop(i) (void*)((uint64_t)(i))

/*
static int bitmap_count_sets(bitstr_t *bm, int bm_size)
{
    int count = 0;
    int i;
    for (i = 0; i < bm_size; i++) {
        if (bit_test(bm, i))
            count++;
    }
    return count;
}
*/

#define test_flag(bitarray, flag)       ((bitarray) & (flag))
#define set_flag(bitarray, flag)        ((bitarray) |= (flag))
#define clear_flag(bitarray, flag)      ((bitarray) &= ~(flag))

#define CPOINT_COMPLETE     0
#define CPOINT_INCOMPLETE   1
#define CPOINT_RESTORE      2   ///< copint as part of the restore
#define CPOINT_REGULAR      3   ///< regular cpoint

#define NOT_CS_CONSISTENT(c,s) (MIN(c, 1) ^ MIN(s, 1))   ///< (c=0 and s=1) is not allowed

#endif /* end of include guard: MACROS_H */

