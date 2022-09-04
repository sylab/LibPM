#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>

#include <cont.h>
#include <utils/queue.h>

const char * program_name;

#define NODES_COUNT 5
#define LIST_HEAD_IDX 0
#define NODE_DATA_SIZE  16

#define MOD_TYPE_INSERT 0
#define MOD_TYPE_DELETE 1
#define MOD_TYPE_UPDATE 2

#define VERBOSE  1
#define NVERBOSE 0

int NEXT_NODE_ID = 0;
#define GET_NEXT_NODE_ID()  (NEXT_NODE_ID++)

int get_next_rand_int(int max)
{
    int val;
    struct timeval t;
    unsigned int seed;
    if (max == 0)
        val = 0;
    else {
        gettimeofday(&t, NULL);
        seed = (unsigned int) (t.tv_usec >> 7);
        val = (rand_r(&seed) % max);
    }
    return val;
}

int get_next_mod_type()
{
    int val = get_next_rand_int(3000);
    if (val < 1000)
        return MOD_TYPE_INSERT;
    if (val < 2000)
        return MOD_TYPE_UPDATE;
    return MOD_TYPE_DELETE;
}

struct node {
    int id;
    char data[NODE_DATA_SIZE];
    SLIST_ENTRY(node) list;
};

struct list_head {
    int size;
    SLIST_HEAD(lhead, node) head;
};

struct list_head * cont_root_alloc(unsigned int cid)
{
    struct list_head *h;
#if 0
    h = malloc(sizeof(*h));
#else
    h = container_palloc(cid, sizeof(*h));
    container_setroot(cid, h);
    pointerat(cid, &h->head.slh_first);
    //container_cpoint(cid);
#endif
    SLIST_INIT(&h->head);
    h->size = 0;
    return h;
}

struct node *node_alloc(unsigned int cid)
{
    struct node *n;
#if 0
    n = malloc(sizeof(*n));
#else
    n = container_palloc(cid, sizeof(*n));
    pointerat(cid, &n->list.sle_next);
    //container_cpoint(cid);
#endif
    n->id = GET_NEXT_NODE_ID();
    memset(n->data, 'a' + (n->id % 26), NODE_DATA_SIZE);
    return n;
}

void list_insert(struct list_head *h, struct node *n, int node_index, int verbose)
{
    int i = node_index;
    struct node *itr = SLIST_FIRST(&h->head);
    
    while (i) {
        itr = SLIST_NEXT(itr, list);
        i--;
    }
    
    if (h->size == 0)
        SLIST_INSERT_HEAD(&h->head, n, list);
    else
        SLIST_INSERT_AFTER(itr, n, list);
    h->size++;

    if (verbose)
        printf("inserting node[ id: %d, data: '%s' ]\n", n->id, n->data);

}

void list_modify(struct list_head *h, int node_index, int verbose)
{
    int i = node_index;
    struct node *itr = SLIST_FIRST(&h->head);

    while (i) {
        itr = SLIST_NEXT(itr, list);
        i--;
    }
    
    sprintf(itr->data, "%s", "[modified]");
    
    if (verbose)
        printf("modifying node[ id: %d, data: '%s' ]\n", itr->id, itr->data);
}

void list_remove(struct list_head *h, int node_index, int verbose)
{
    int i = node_index;
    struct node *itr = SLIST_FIRST(&h->head);

    while (i) {
        itr = SLIST_NEXT(itr, list);
        i--;
    }
    
    SLIST_REMOVE(&h->head, itr, node, list);
    h->size--;
    
    if (verbose)
        printf("removing node[ id: %d, data: '%s' ]\n", itr->id, itr->data);
}

void print_list(struct list_head *h, const char *msg)
{
    struct node *itr;
    printf("%s\n", msg);
    SLIST_FOREACH(itr, &h->head, list) {
        printf("  node[ id: %d, data: '%s' ]\n", itr->id, itr->data);
    }
}

void create_list()
{
    struct container *cont;
    struct list_head *h;
    struct node *n;
    int i;

    cont = container_init();

    h = cont_root_alloc(cont->id);

    for (i = 0; i < NODES_COUNT; i++) {
        n = node_alloc(cont->id);
        SLIST_INSERT_HEAD(&h->head, n, list);
        h->size++;
    }

    print_list(h, "This is the contents of the list before cpoint");
    container_cpoint(cont->id);
    
    /* the changes made from this point on should not be restore */
    int num_opes = get_next_rand_int(10);
    printf("%d modifications will be performed\n", num_opes);
    while (num_opes) {
        int mod_type = get_next_mod_type();
        if (h->size == 0)
            mod_type = MOD_TYPE_INSERT;
        
        switch(mod_type) {
            case MOD_TYPE_INSERT:
                list_insert(h, node_alloc(cont->id), get_next_rand_int(h->size), VERBOSE);
                break;
            case MOD_TYPE_UPDATE:
                list_modify(h, get_next_rand_int(h->size), VERBOSE);
                break;
            case MOD_TYPE_DELETE:
                list_remove(h, get_next_rand_int(h->size), VERBOSE);
                break;
        }
        num_opes--;
        assert(num_opes);
    }
}

void restore_list()
{
    int cid = 0;
    struct container *cont;
    struct list_head *h;
    
    cont = container_restore(cid);
    h = container_getroot(cid);
    if (h)
        print_list(h, "This is the contents of the list after restoring");
}

void print_usage(FILE* stream, int exit_code)
{
    fprintf(stream, "Usage: %s options\n", program_name);
    fprintf(stream, 
            "   -h    --help        Display this usage information.\n"
            "   -c    --create      Create new list.\n"
            "   -r    --restore     Restore list.\n");
    exit(exit_code);
}

int main(int argc,  char *const * argv)
{

    int next_option;
    static struct option long_options[] = {
        { "create",     no_argument, 0, 'c' },
        { "restore",    no_argument, 0, 'r' },
        { 0,            0,           0,  0 }
    };

    program_name = argv[0];

    do {
        next_option = getopt_long(argc, argv, "cr", long_options, NULL);
        switch(next_option) {
            case 'h':
                print_usage(stdout, 0);

            case 'c': /* --create */
                create_list();
                break;

            case 'r': /* --restore */
                restore_list();
                break;

            case '?': /* user provided invalid option */
                print_usage(stderr, 1);

            case -1: /* done with options */
                break;

            default:
                abort();
        }
    } while (next_option != -1);

    return 0;
}
