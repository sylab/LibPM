#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#include <macros.h>
#include <page_alloc.h>
#include <settings.h>
#include <out.h>

#define COUNTER 16

/*
 * Here we test that we are reclaiming space after freeing pages
 */
void test_page_alloc_and_free(int cid)
{
    void *ptrs[COUNTER] = {0};
    int i;

    for (i = 0; i < COUNTER; i++) {
        ptrs[i] = page_allocator_getpage(cid,  NULL, PA_PROT_WRITE);
        printf("writing %p\n", ptrs[i]);
        memset(ptrs[i], 'a' + (i % 26), PAGE_SIZE);
    }

    printf("########################\n");

    for (i = 0; i < COUNTER; i += 2) {
        printf("freeing %p\n", ptrs[i]);
        page_allocator_freepages(cid, ptrs[i]);
    }

    printf("########################\n");

    for (i = 0; i < COUNTER; i += 2) {
        ptrs[i] = page_allocator_getpage(cid,  NULL, PA_PROT_WRITE);
        printf("re-writing %p\n", ptrs[i]);
        memset(ptrs[i], 'A' + (i % 26), PAGE_SIZE);
    }

}

void test_restore_file(int cid)
{
    int i;
    void *ptrs[COUNTER];

    for (i = 0; i < COUNTER; i += 2) {
        ptrs[i] = page_allocator_mappage(cid, i * PAGE_SIZE);
        //printf("iteration %d, file size in pages: %zu, list size: %lu, tree size: %lu\n",
        //        i, fm->file_size / PAGE_SIZE, fm->free_list_size, fm->alloc_tree_size);
        memset(ptrs[i], 'a' + (i % 26), PAGE_SIZE);
    }
}

// the size of the container file should be no more than 2x size_gb
void test_allocate_many_pgs(int cid, int size_gb)
{
    uint64_t page_in_gb = (1UL << 30) / 4096;
    for (uint64_t i = 0; i < (size_gb * page_in_gb); ++i) {
        page_allocator_getpage(cid, NULL, PA_PROT_WRITE);
    }
}

void print_memory_content(const char *str, size_t size)
{
    char curr;
    char prev = str[0];
    int cnt = 0;
    int prev_idx = 0;

    for (size_t i = 0; i < size; i++) {
        curr = str[i];
        if (curr != prev) {
            printf("%p '%c' <repeats %i times>\n", (void*)&str[prev_idx], prev, cnt);
            prev = curr;
            cnt = 1;
            prev_idx = i;
        } else {
            cnt++;
        }
    }
    printf("%p '%c' <repeats %i times>\n", (void*)&str[prev_idx], prev, cnt);
}

void test_swap_mappings(int cid)
{
    void *ptrs[COUNTER] = {0};
    size_t offsets[COUNTER] = {0};

    for (int i = 0; i < COUNTER; i++) {
        ptrs[i] = page_allocator_getpage(cid,  &offsets[i], PA_PROT_WRITE);
        memset(ptrs[i], 'a' + (i % 26), PAGE_SIZE);
    }

    printf("Memory before swapping\n");
    for (int i = 0; i < COUNTER; i++) {
        print_memory_content(ptrs[i], PAGE_SIZE);
    }

    // swap page 1 with 3
    page_allocator_swap_mappings(cid, ptrs[1], offsets[3], ptrs[3], offsets[1]);
    // restore linear mapping
    //page_allocator_swap_mappings(cid, ptrs[3], offsets[3], ptrs[1], offsets[1]);

    printf("Memory after swapping\n");
    for (int i = 0; i < COUNTER; i++) {
        print_memory_content(ptrs[i], PAGE_SIZE);
    }

    for (int i = 0; i < COUNTER; i++) {
        ptrs[i] = page_allocator_getpage(cid,  &offsets[i], PA_PROT_WRITE);
        memset(ptrs[i], 'a' + (i % 26), PAGE_SIZE);
    }
}

int main(int argc, const char *argv[])
{
    struct page_allocator *pa;
    char fname[128];
    int cid = 0;

    out_init(PMLIB_LOG_PREFIX, PMLIB_LOG_LEVEL_VAR, PMLIB_LOG_FILE_VAR,
            PMLIB_MAJOR_VERSION, PMLIB_MINOR_VERSION);
    LOG(3, NULL);

    sprintf(fname, "%s%d", FM_FILE_NAME_PREFIX, cid);
    if (unlink(fname)) {
        perror("failed to delete container");
    }

    pa = page_allocator_init(cid);
        test_page_alloc_and_free(cid);
    page_allocator_shutdown(cid);

    //pa = page_allocator_init(cid);
    //    test_swap_mappings(cid);
    //page_allocator_shutdown(cid);

    //pa = page_allocator_init(cid);
    //    test_restore_file(cid);
    //    page_allocator_init_complete(cid);
    //    //printf("file size in pages: %zu, list size: %lu, tree size: %lu\n",
    //    //        fm->file_size / PAGE_SIZE, fm->free_list_size, fm->alloc_tree_size);
    //page_allocator_shutdown(cid);

    //pa = page_allocator_init(cid);
    //    test_allocate_many_pgs(cid, 4 /* gb */);
    //page_allocator_shutdown(cid);

    return 0;
}
