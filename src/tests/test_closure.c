#include <stdio.h>
#include <stdlib.h>

#include <utils/macros.h>
#include <utils/vector.h>
#include <closure.h>
#include <cont.h>

#define ARRAY_SIZE 10
#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

extern struct htable *ht_pointerat;
extern struct htable *ht_mallocat;
extern VECTOR_DECL(struct_VECTOR_ptr, void*) VECTOR_ptr;

struct S {
    int f1;
    void *f2;
    void *f3;
};

void test_pointerat_mallocat()
{
    struct S array[] = {
        { .f1 = 1, .f2 = (void*)0x10, .f3 = (void*)0x11 },
        { .f1 = 2, .f2 = (void*)0x20, .f3 = (void*)0x21 },
        { .f1 = 3, .f2 = (void*)0x30, .f3 = (void*)0x31 },
        { .f1 = 4, .f2 = (void*)0x40, .f3 = (void*)0x41 },
        { .f1 = 5, .f2 = (void*)0x50, .f3 = (void*)0x51 },
        { .f1 = 6, .f2 = (void*)0x60, .f3 = (void*)0x61 }
    };

    unsigned int cid = 0;
    for (int i = 0; i < ARRAY_LEN(array); i++) {
        mallocat(&array[i], sizeof(struct S));
        pointerat(cid, &array[i].f2);
        pointerat(cid, &array[i].f3);
    }

    build_mallocat_tree();
    classify_pointers(cid);
    mallocat_pprint();
    htable_print(ht_pointerat);
}

void test_persistent_pointers()
{
    struct container *cont = container_init();
    struct S *s = container_palloc(cont->id, sizeof(*s) * 6);

    for (int i = 0; i < 6; i++) {
        pointerat(cont->id, &s[i].f2);
        pointerat(cont->id, &s[i].f3);
    }

    build_mallocat_tree();
    classify_pointers(cont->id);
    mallocat_pprint();
    htable_print(ht_pointerat);

    /*
    for (int i = 0; i < VECTOR_ptr.size; i++) {
        printf("persistent pointer at %p\n", VECTOR_ptr.buffer[i]);
    }
    */
}

void test_mallocat()
{
    void *arr[ARRAY_SIZE];

    uint64_t offset = 4096;
    uint64_t increment = 4096;

    for (int i = 0; i < ARRAY_SIZE; i++) {
        arr[i] = itop(offset + increment * i);
    }

    for (int i = 0; i < ARRAY_SIZE; i++) {
        mallocat(arr[i], i);
        //VECTOR_APPEND();
    }

    //inserting duplicate entries
    mallocat(arr[0], 10);
    mallocat(arr[2], 11);

    mallocat_pprint();
}

/*
 * In this test, we start with a list of 3 nodes. Then we create a container
 * with a node pointing to the second node of the volatile list, as depecited
 * here.
 *
 * n0 ----> n1 ----> n2 ----> NULL
 *          ^
 * cont     |
 * +--------|--+
 * | n3 ----+  |
 * +-----------+
 *
 * Then we issue a regular cpint, and we shuold end with 3 nodes in the
 * container and a volatile node pointing to the second node in the container.
 * Like this
 *
 * n0 -------+
 *           |
 * cont      |
 * +---------|---------------------+
 * | n3 ---> n1 ---> n2 ---> NULL  |
 * +-------------------------------+
 *
 */
void test_all()
{
    #define NODE_DATA_SIZE 100
    const int node_count = 3;

    struct Node {
        struct Node *next;
        char data[NODE_DATA_SIZE];
    } *array[node_count];

    // create an empy container
    struct container *cont = container_init();

    for (int i = 0; i < node_count; i++) {
        array[i] = malloc(sizeof(struct Node));
        sprintf(array[i]->data, "node: %d", i);

        // manual annotation of pointers and mallocs
        mallocat(array[i], sizeof(struct Node));
        pointerat(cont->id, &array[i]->next);
    }

    array[0]->next = array[1];
    array[1]->next = array[2];
    array[2]->next = NULL;

    // allocate a node in the container and make the node the container's root
    struct Node *head = container_palloc(cont->id, sizeof(*head));
    sprintf(head->data, "%s", "contianer head!");
    container_setroot(cont->id, head);
    pointerat(cont->id, &head->next);
    // make the node in the container point to the second node in the list
    head->next = array[1];

    container_cpoint(cont->id);

    printf("Printing list from the volatile head\n");
    struct Node *node = array[0];
    while (node) {
        printf("\tnode { addr: %p, data: '%s', next: %p }\n", node, node->data, node->next);
        node = node->next;
    }

    printf("Printing list from the container\n");
    node = head;
    while (node) {
        printf("\tnode { addr: %p, data: '%s', next: %p }\n", node, node->data, node->next);
        node = node->next;
    }

}

int main(int argc, const char *argv[])
{
    // init the closure internal data structures
    //closure_init();

    //test_mallocat();
    //test_pointerat_mallocat();
    //test_persistent_pointers();

    test_all();
    container_pprint();

    return 0;
}
