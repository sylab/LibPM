#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#include <cont.h>
#include <utils/tree.h>
#include <utils/macros.h>

const char * program_name;

enum {ope_baseline, ope_softpm, ope_consistent};
char * OPE_TYPE_NAME(int type) {
    if (type == ope_baseline)
        return "baseline";
    if (type == ope_softpm)
        return "softpm";
    if (type == ope_consistent)
        return "consisten";
    return NULL;
}

struct input_options {
    uint32_t num_ope;
    uint32_t datasize;
    uint32_t ope_type;
} OPTIONS = { .num_ope = 0, .datasize = 128, .ope_type = ope_baseline };

static uint64_t NODE_KEY = 0;
#define GET_NEXT_NODE_KEY() (NODE_KEY++)

struct node {
    uint64_t key;
    RB_ENTRY(node) entry;
    uint32_t data_size;
    char data[0];
};

struct tree_handler {
    uint64_t size;
    RB_HEAD(tree_struct, node) root;
};

#define DATA_SIZE(n)   ((n) - sizeof(struct node))

int node_cmp(struct node *a, struct node *b)
{
    return (a->key < b->key ? -1 : a->key > b->key);
}

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

RB_GENERATE(tree_struct, node, entry, node_cmp);

void print_usage(FILE* stream, int exit_code)
{
    fprintf(stream, "Usage: %s options\n", program_name);
    fprintf(stream,
            "   -h    --help            Display this usage information.\n"
            "   -b    --baseline        Do not use softpm.\n"
            "   -p    --softpm          Allocate memory using Softpm and cpoint once (at the end).\n"
            "   -c    --consistent      Allocate memory with softpm and perform a cpoint after every update.\n"
            "   -n    --operations n    Perform n inserts.\n"
            "   -s    --datasize n      Make each node's data size equal to n bytes\n");
    exit(exit_code);
}

void do_baseline()
{
    struct tree_handler *r;
    struct node *n;
    int i;

    r = malloc(sizeof(*r));
    if (!r)
        handle_error("failed to allocate memory for the root");
    r->size = 0;
    RB_INIT(&r->root);

    for (i = 0; i < OPTIONS.num_ope; i++) {
        n = malloc(OPTIONS.datasize);
        if (!n)
            handle_error("failed to allocate memory\n");
        n->key = GET_NEXT_NODE_KEY();
        n->data_size = OPTIONS.datasize;
        RB_INSERT(tree_struct, &r->root, n);
        r->size++;
    }
}

void do_softpm(int consistent)
{
    struct container *cont;
    struct tree_handler *r;
    struct node *n;
    int i;
   
    /* extra lines of code */
    cont = container_init();
    /* end of extra lines of code */
    
    r = container_palloc(cont->id, sizeof(*r));
    
    /* extra lines of code */
    container_setroot(cont->id, r);
    pointerat(cont->id, (void**)&r->root.rbh_root);
    /* end of extra lines of code */
    
    for (i = 0; i < OPTIONS.num_ope; i++) {
        n = container_palloc(cont->id, OPTIONS.datasize);
        if (!n)
            handle_error("failed to allocate memory for node in contianer\n");

        /* extra lines of code */
        pointerat(cont->id, (void**)&n->entry.rbe_left);
        pointerat(cont->id, (void**)&n->entry.rbe_right);
        pointerat(cont->id, (void**)&n->entry.rbe_parent);
        /* end of extra lines of code */

        n->key = GET_NEXT_NODE_KEY();
        n->data_size = OPTIONS.datasize;
        RB_INSERT(tree_struct, &r->root, n);
        r->size++;

        if (consistent)
            container_cpoint(cont->id);
    }

    if (!consistent)
        container_cpoint(cont->id);
}

int main(int argc, char *const * argv)
{
    int next_option;
    static struct option long_options[] = {
        { "baseline",   no_argument,       0, 'b' },
        { "softpm",     no_argument,       0, 'p' },
        { "consistent", no_argument,       0, 'c' },
        { "operations", required_argument, 0, 'n' },
        { "datasize",   required_argument, 0, 's' },
        { 0,            0,                 0,  0 }
    };

    program_name = argv[0];

    do {
        next_option = getopt_long(argc, argv, "bpcn:s:", long_options, NULL);
        switch(next_option) {
            case 'h':
                print_usage(stdout, 0);

            case 'b':
                OPTIONS.ope_type = ope_baseline;
                break;

            case 'p':
                OPTIONS.ope_type = ope_softpm;
                break;

            case 'c':
                OPTIONS.ope_type = ope_consistent;
                break;

            case 'n':
                OPTIONS.num_ope = (uint64_t)atoll(optarg);
                break;

            case 's':
                OPTIONS.datasize = MAX((uint32_t)atoi(optarg), sizeof(struct node));
                break;

            case '?': /* user provided invalid option */
                print_usage(stderr, 1);

            case -1: /* done with options */
                break;

            default:
                abort();
        }
    } while (next_option != -1);

    struct timespec __t0, __t1;
    clock_gettime(CLOCK_MONOTONIC, &__t0);
    switch (OPTIONS.ope_type) {
        case ope_baseline:
            do_baseline();
            break;
        case ope_softpm:
            do_softpm(0);
            break;
        case ope_consistent:
            do_softpm(1);
            break;
    }
    clock_gettime(CLOCK_MONOTONIC, &__t1);
    printf("ope_type: %s num_ope: %u datasize: %d time: %.3Lf seconds\n", 
            OPE_TYPE_NAME(OPTIONS.ope_type), OPTIONS.num_ope, OPTIONS.datasize, time_diff(__t0, __t1));

    return 0;
}
