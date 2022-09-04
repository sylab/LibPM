#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>

//#include <fixptr.h>
#include <cont.h>
#include <slab.h> //only for debugging
#include <utils/queue.h> //only for debugging

const char * program_name;

#define PAGE_SIZE   4096
#define SLAB_ENTRY_COUNT 16

#define PACK_ONE_NODE_PER_SLABENTRY      (PAGE_SIZE >> 1)
#define PACK_TWO_NODES_PER_SLABENTRY     ((PAGE_SIZE / 2) - (sizeof(int) + sizeof(void*)) - 10)
#define PACK_FOUR_NODES_PER_SLABENTRY    ((PAGE_SIZE / 4) - (sizeof(int) + sizeof(void*)) - 10)

enum {ope_create_list, ope_create_list_consistent, ope_restore_list, ope_do_baseline};
char * OPE_TYPE_NAME(int type) {
    if (type == ope_create_list)
        return "softpm_create_list";
    if (type == ope_create_list_consistent)
        return "softpm_create_list_consistent";
    if (type == ope_restore_list)
        return "softpm_restore_list";
    if (type == ope_do_baseline)
        return "baseline";
    return NULL;
}

struct input_options {
    uint32_t num_nodes;
    uint32_t node_size;
    uint32_t ope_type;
    uint32_t consistent;
} OPTIONS = {
    .num_nodes = 0,
    .node_size = PACK_ONE_NODE_PER_SLABENTRY,
    .ope_type = ope_create_list,
    .consistent = 0
};

static inline long double time_diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return (long double)temp.tv_sec + (long double)temp.tv_nsec/1000000000;
}

struct node {
    int data;
    struct node *next;
    char _c[PACK_ONE_NODE_PER_SLABENTRY];
};

void print_status(const char *msg, int i)
{
    printf("iteration: %d, %s\n", i, msg);
    //printf("TREE_SIZE: %d\n", RB_TREE_SIZE);
    print_splay_tree();
    slab_entry_print(0);
}

void do_baseline(int nodes_count)
{
    struct node *nodes[nodes_count];
    int i;

    for (i = 0; i < nodes_count; i++) {
        nodes[i] = malloc(sizeof(struct node));
        nodes[i]->data = i;
    }
}


void create_list(int nodes_count)
{
    struct container *cont;
    struct node **nodes;
    int i;

    nodes = malloc(nodes_count * sizeof(nodes));

    cont = container_init();

    printf("This test will create %d nodes...\n", nodes_count);
    for (i = 0; i < nodes_count; i++) {
        nodes[i] = container_palloc(cont->id, sizeof(struct node));
        nodes[i]->data = i;

        //print_status("before cpoint", i);
        if (OPTIONS.consistent)
            container_cpoint(cont->id);
        //print_status("aftercpoint", i);

        //printf("Node %d has been created\n", i);
        //printf("----------------------------------------------\n");
    }

    for (i = 0; i < nodes_count - 1; i++) {
        nodes[i]->next = nodes[i+1];
        pointerat(cont->id, &nodes[i]->next);
    }
    nodes[i]->next = NULL;
    pointerat(cont->id, &nodes[i]->next);

    container_setroot(cont->id, nodes[0]);

    /*
    print_status("before cpoint", i);
    {
        struct ptrat *pat;
        int i=0;
        STAILQ_FOREACH(pat, &cont->ptrat_head, list) {
            printf("i: %d, ploc: %p\n", i++, pat->ptr_loc);
        }
    }
    */

    container_cpoint(cont->id);

    container_pprint();
    //print_status("aftercpoint", i);

    free(nodes);
}

void restore_list()
{
    int cid = 0;
    struct container *cont;
    struct node *n;

    printf("starting restore\n");
    cont = container_restore(cid);
    n = container_getroot(cid);
    printf("restore finished\n");

    container_pprint();

    while (n) {
        printf("n: %p next:%p data:%d\n", n, n->next, n->data);
        n = n->next;
    }

    container_pprint();
}

void print_usage(FILE* stream, int exit_code)
{
    fprintf(stream, "Usage: %s options\n", program_name);
    fprintf(stream,
            "   -h    --help        Display this usage information.\n"
            "   -c    --create      Create new list using softpm.\n"
            "   -r    --restore     Restore list store in a container.\n"
            "   -b    --baseline    Run baseline test (does not use softpm).\n"
            "   -p    --consistent  Create a cpoint after every update (only valid when used with -c).\n"
            "   -n    --nodes n     Create n nodes (set this param before --create).\n");
    exit(exit_code);
}

int main(int argc,  char *const * argv)
{

    int next_option;
    static struct option long_options[] = {
        { "help",       no_argument, 0, 'h' },
        { "create",     no_argument, 0, 'c' },
        { "restore",    no_argument, 0, 'r' },
        { "nodes",      required_argument, 0, 'n' },
        { "baseline",   no_argument, 0, 'b' },
        { "consistent", no_argument, 0, 'p' },
        { 0,            0,           0,  0 }
    };

    program_name = argv[0];

    struct timespec __t0, __t1;
    clock_gettime(CLOCK_MONOTONIC, &__t0);
    do {
        next_option = getopt_long(argc, argv, "hcrbpn:", long_options, NULL);
        switch(next_option) {
            case 'h':
                print_usage(stdout, 0);

            case 'n':
                OPTIONS.num_nodes = atoi(optarg);
                //nodes = atoi(optarg);
                break;

            case 'c': /* --create */
                OPTIONS.ope_type = ope_create_list;
                create_list(OPTIONS.num_nodes);
                break;

            case 'r': /* --restore */
                OPTIONS.ope_type = ope_restore_list;
                restore_list();
                break;

            case 'b': /* --baseline */
                OPTIONS.ope_type = ope_do_baseline;
                do_baseline(OPTIONS.num_nodes);
                break;

            case 'p': /* --consistent */
                OPTIONS.consistent = 1;
                OPTIONS.ope_type = ope_create_list_consistent;
                break;

            case '?': /* user provided invalid option */
                print_usage(stderr, 1);

            case -1: /* done with options */
                break;

            default:
                abort();
        }
    } while (next_option != -1);
    clock_gettime(CLOCK_MONOTONIC, &__t1);
    printf("ope_type: %s num_nodes: %d datasize: %u time: %.3Lf seconds\n",
            OPE_TYPE_NAME(OPTIONS.ope_type), OPTIONS.num_nodes, OPTIONS.node_size, time_diff(__t0, __t1));

    return 0;
}
