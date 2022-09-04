#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cont.h>

#define PAGE_SIZE   4096
#define BIG_SIZE    ((PAGE_SIZE / 2) + 1)
#define MID_SIZE    (PAGE_SIZE / 3)
#define SMALL_SIZE  (PAGE_SIZE / 8)
#define SMALL_CNT   (2 * PAGE_SIZE / SMALL_SIZE) // this is 16 small allocations
#define BIG_CNT     (10)

void container_pprint();

void test_slab_entry_alloc(unsigned int cid)
{
    void *big1, *big2;
    void *small[SMALL_CNT];
    int i;

    big1 = container_palloc(cid, BIG_SIZE);
    memset(big1, 'A', BIG_SIZE);
    big2 = container_palloc(cid, BIG_SIZE);
    memset(big2, 'B', BIG_SIZE);

    printf("big1: %p\n", big1);
    printf("big2: %p\n", big2);

    for (i = 0; i < SMALL_CNT; i++) {
        small[i] = container_palloc(cid, SMALL_SIZE);
        memset(small[i], 'a' + (i % 26), SMALL_SIZE);
        printf("small[%i]: %p\n", i, small[i]);
    }

}

void test_slab_dir_alloc(unsigned int cid)
{
    void *big[BIG_CNT];
    int i;

    for (i = 0; i < BIG_CNT; i++) {
        big[i] = container_palloc(cid, BIG_SIZE);
        memset(big[i], 'A' + (i % 26), BIG_SIZE);
        printf("big[%i]: %p\n", i, big[i]);
    }

}

void test_cpoint(unsigned int cid)
{
    void *small[SMALL_CNT];
    int i;

    for (i = 0; i < SMALL_CNT; i++) {
        small[i] = container_palloc(cid, SMALL_SIZE);
        memset(small[i], 'A' + (i % 26), SMALL_SIZE);
        printf("small[%i]: %p\n", i, small[i]);

        container_cpoint(cid);
        container_pprint();
    }
}

void test_pointerat(unsigned int cid)
{
    struct clique {
        struct clique *n1;
        struct clique *n2;
        int size;
        char data[1];
    } *a, *b, *c;

    a = (struct clique*) container_palloc(cid, sizeof(*a) + SMALL_SIZE - 1);
    a->size = SMALL_SIZE;
    memset(a->data, 'A', a->size);

    b = (struct clique*) container_palloc(cid, sizeof(*b) + MID_SIZE - 1);
    b->size = MID_SIZE;
    memset(b->data, 'B', b->size);

    c = (struct clique*) container_palloc(cid, sizeof(*c) + BIG_SIZE - 1);
    c->size = BIG_SIZE;
    memset(c->data, 'A', c->size);

    a->n1 = b;
    pointerat(cid, (void**)&a->n1);
    a->n2 = c;
    pointerat(cid, (void**)&a->n2);
    b->n1 = a;
    pointerat(cid, (void**)&b->n1);
    b->n2 = c;
    pointerat(cid, (void**)&b->n2);
    c->n1 = a;
    pointerat(cid, (void**)&c->n1);
    c->n2 = b;
    pointerat(cid, (void**)&c->n2);

    container_setroot(cid, a);
}

int main(int argc, const char *argv[])
{
    struct container *cont;
    cont = container_init();

    //test_slab_dir_alloc(cont->id);
    test_cpoint(cont->id);

    //test_pointerat(cont->id);
    //test_slab_entry_alloc(cont->id);
    //container_cpoint(cont->id);
    //test_slab_entry_alloc(cont->id);

    return 0;
}
