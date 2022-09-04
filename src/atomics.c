#include <string.h>
#include <atomics.h>
#include <settings.h>
#include <macros.h>

void flush_memsegment(const void *src, size_t n, int fence)
{
    void *low = itop(ROUND_DWNCL(ptoi(src)));
    void *high = itop(ROUND_UPCL(ptoi(src) + n));
    void *itr;
    for (itr = low; itr < high; itr+=CACHE_LINE_SIZE) {
        flush(itr, fence);
    }
}

/**
 * @dest is assumed to be in persistent memory
 */
void *pmemcpy(void *dest, const void *src, size_t n)
{
    void *ret = memcpy(dest, src, n);
    flush_memsegment(dest, n, 0);
    return ret;
}

