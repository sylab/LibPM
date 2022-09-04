#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <cont.h>

#define BIG_NODE    (PAGE_SIZE - 100)       //only one in a page
#define MID_NODE    ((PAGE_SIZE - 10) / 2)  //only two in a page
#define SMAL_NODE   ((PAGE_SIZE - 10) / 4)  //only four in a page

struct node {
    struct node *next;
    char data[1];
};

const char * program_name;

void create_list(unsigned int cid, unsigned int node_cnt, unsigned int freq)
{
    struct pptr_head head;
    unsigned int count = node_cnt;
    unsigned int i;

    pptr_init(cid, &head);

    for (i = 1; i <= count; i++) {
        pptr_append(0, &head, i, i, i*10, i*10);
        printf("aftern append %u ==============================\n", i);
        pptr_print(&head);
        //
        pptr_cpoint(0, &head);
        printf("aftern cpoint %u ==============================\n", i);
        pptr_print(&head);

        //if (freq > 0 && i % freq == 0)
        //    pptr_cpoint(0, &head);
    }

    //printf("list after intserting %u entries\n", count);
    //pptr_print(&head);

    //pptr_cpoint(0, &head);
    //pptr_print(&head);
    ////
    //pptr_append(0, &head, i, i, i*10, i*10);
    //////printf("list after final cpoint\n");
    //pptr_print(&head);

    //struct pptr *p;
    //pptr_for_each(&head, p) {
    //    printf("p: %p, __n: %p, __i: %d, loc: %u, val: %u\n",
    //            p, __n, __i, p->ploc_offset, p->pval_offset);
    //}
}

void restore_list(unsigned int cid)
{
    /* we need to set the laddr of the first inode so that we can restore the
     * list of persistent pointers
     */
    struct pptr_head head = {{0}};

    pptr_map(cid, &head, CPOINT_COMPLETE);
    printf("list after CPOINT_COMPLETE\n");
    pptr_print(&head);
}

void print_usage(FILE* stream, int exit_code)
{
    fprintf(stream, "Usage: %s options\n", program_name);
    fprintf(stream,
            "   -h    --help          Display this usage information.\n"
            "   -f    --cpoint-freq n Checkpoint after n operations (set this before -c).\n"
            "   -c    --create n      Create new list with n nodes.\n"
            "   -r    --restore       Restore list.\n");
    exit(exit_code);
}

/*
 * In this test, we allocate several objs of different size in the container
 * and verify that the objects are group up correctly.
 */
void allocation_test()
{
    struct container *cont = container_init();

    struct node *big_node = container_palloc(cont->id, BIG_NODE);
    struct node *mid_node1 = container_palloc(cont->id, MID_NODE);
    struct node *mid_node2 = container_palloc(cont->id, MID_NODE);
    struct node *smal_node1 = container_palloc(cont->id, SMAL_NODE);
    struct node *smal_node2 = container_palloc(cont->id, SMAL_NODE);
    struct node *smal_node3 = container_palloc(cont->id, SMAL_NODE);
    struct node *smal_node4 = container_palloc(cont->id, SMAL_NODE);

    big_node->next = mid_node1;
    mid_node1->next = mid_node2;
    mid_node2->next = smal_node1;
    smal_node1->next = smal_node2;
    smal_node2->next = smal_node3;
    smal_node3->next = smal_node4;
    smal_node4->next = NULL;

    pointerat(cont->id, &big_node->next);
    pointerat(cont->id, &mid_node1->next);
    pointerat(cont->id, &mid_node2->next);
    pointerat(cont->id, &smal_node1->next);
    pointerat(cont->id, &smal_node2->next);
    pointerat(cont->id, &smal_node3->next);
    pointerat(cont->id, &smal_node4->next);

    container_cpoint(cont->id);
    container_pprint();
}

/*
 * In this test we simply verify that multiple checkpoints does not break
 * the logic of persistent pointers.
 * NOTE: We make next pointers point to themself to avoif having null ptrs
 */
void multicheckpoint_test()
{
    struct container *cont = container_init();

    struct node *mid_node1 = container_palloc(cont->id, MID_NODE);
    pointerat(cont->id, &mid_node1->next);
    mid_node1->next = mid_node1;
    container_cpoint(cont->id);
    container_pprint();

    struct node *mid_node2 = container_palloc(cont->id, MID_NODE);
    pointerat(cont->id, &mid_node2->next);
    mid_node2->next = mid_node2;
    container_cpoint(cont->id);
    container_pprint();

    struct node *smal_node1 = container_palloc(cont->id, SMAL_NODE);
    pointerat(cont->id, &smal_node1->next);
    smal_node1->next = smal_node1;
    container_cpoint(cont->id);
    container_pprint();

    struct node *smal_node2 = container_palloc(cont->id, SMAL_NODE);
    pointerat(cont->id, &smal_node2->next);
    smal_node2->next = smal_node2;
    container_cpoint(cont->id);
    container_pprint();
}

int main(int argc,  char *const * argv)
{
    unsigned int cid = 0;
    unsigned int node_cnt = 0;
    unsigned int cpoint_freq = 0; // only at the end

    int next_option;
    static struct option long_options[] = {
        { "cpoint-freq", required_argument,  0, 'f' },
        { "create",      required_argument,  0, 'c' },
        { "restore",     no_argument,        0, 'r' },
        { 0,             0,                  0,  0 }
    };

    program_name = argv[0];

#if 0
    page_allocator_init(cid);

    do {
        next_option = getopt_long(argc, argv, "c:rf:h", long_options, NULL);
        switch(next_option) {
            case 'h':
                print_usage(stdout, 0);

            case 'f': /* the freq must be set before calling create_list */
                cpoint_freq = atoi(optarg);
                break;

            case 'c': /* --create */
                node_cnt = atoi(optarg);
                create_list(cid, node_cnt, cpoint_freq);
                break;

            case 'r': /* --restore */
                restore_list(cid);
                break;

            case '?': /* user provided invalid option */
                print_usage(stderr, 1);

            case -1: /* done with options */
                break;

            default:
                abort();
        }
    } while (next_option != -1);

    page_allocator_init_complete(cid);
#endif

    //NOTE: as of now, both test can not be run in one execution!
    //allocation_test();
    multicheckpoint_test();

    return 0;
}
