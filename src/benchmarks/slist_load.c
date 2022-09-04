#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "queue.h"
#include "slist.h"
#include <cont.h>
#include <timediff.h>
#include "tpl.h"

#define BACKEND_TPL     1
#define BACKEND_PMLIB   2

const char *program_name;

void print_usage(FILE *stream, int exit_code)
{
    fprintf(stream, "Usage: %s options\n", program_name);
    fprintf(stream,
            "  -h       Display usage.\n"
            "  -n x     Create x nodes.\n"
            "  -t       Use TPL for persistence.\n"
            "  -p       Use pmlib for persistence.\n"
            "  -m       Allocate memory with malloc.\n"
            "  -c       Create a consistent point after every modification.\n");
    exit(exit_code);
}

void write_with_tpl(struct slist_head *head)
{
    char *data;
    tpl_node *tn = tpl_map("UiA(s)", &head->node_cnt, &head->node_size, &data);
    tpl_pack(tn, 0);

    //resializing the nodes
    struct slist_node *itr = NULL;
    STAILQ_FOREACH(itr, &head->head, node) {
        data = itr->data;
        tpl_pack(tn, 1);
    }

    //writing to file
    int fd = open(SLIST_TPL_FILE, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
    assert(fd != -1);
    tpl_dump(tn, TPL_FD, fd);
    fsync(fd);
    tpl_free(tn);
    close(fd);
}

int main(int argc, char * const argv[])
{
    int opt;
    uint64_t n = 0;
    int consistent = 0;
    int backend_engine = 0;
    int node_size = 512;
    int alloc_with_malloc = 0;
    program_name = argv[0];
    TIMEDIFF_INIT();

    while ((opt = getopt(argc, argv, "hn:ctpm")) != -1) {
        switch (opt) {
            case 'h': print_usage(stdout, EXIT_SUCCESS); break;
            case 'n': n = atoll(optarg); break;
            case 't': backend_engine = BACKEND_TPL; break;
            case 'p': backend_engine = BACKEND_PMLIB; break;
            case 'c': consistent = 1; break;
            case 'm': alloc_with_malloc = 1; break;
            default: print_usage(stderr, EXIT_FAILURE);

        }
    }

    if ((backend_engine != BACKEND_PMLIB) && (backend_engine != BACKEND_TPL)) {
        fprintf(stderr, "Backend engine not specified.\n");
        exit(EXIT_FAILURE);
    }

    printf("backend: %s nodes: %lu consistent: %d\n",
            backend_engine == BACKEND_PMLIB ? "pmlib" : "tpl", n, consistent);

    TIMEDIFF_START();
    struct slist_head *head;
    struct slist_node *node;
    uint64_t ten_percent = n / 10;
    uint64_t verbose_itr = ten_percent;

    if (backend_engine == BACKEND_PMLIB) {

        //init container
        struct container *cont = container_init();

        //allocate head of the slist
        head = container_palloc(cont->id, sizeof(*head));
        head->node_size = node_size;
        STAILQ_INIT(&head->head);
        pointerat(cont->id, &head->head.stqh_first);
        pointerat(cont->id, &head->head.stqh_last);

        if (consistent)
            container_cpoint(cont->id);

        //allocate all nodes and insert them into tree
        for (uint64_t i = 0; i<n; i++) {

            if (alloc_with_malloc) {
                node = malloc(node_size);
                mallocat(node, node_size);
            } else
                node = container_palloc(cont->id, node_size);

            sprintf(node->data, "%lu", i);
            STAILQ_INSERT_TAIL(&head->head, node, node);
            pointerat(cont->id, &node->node.stqe_next);

            head->node_cnt++;

            if (consistent) {
                container_cpoint(cont->id);

                if (i > verbose_itr) {
                    printf("Operations completed %lu out of %lu\n", i, n);
                    verbose_itr += ten_percent;
                }
            }
        }

        if (!consistent)
            container_cpoint(cont->id);

    } else /* backend_engine == BACKEND_TPL */ {

        //creating list head
        head = malloc(sizeof(*head));
        head->node_size = node_size;
        STAILQ_INIT(&head->head);

        //adding nodes to the tree
        for (uint64_t i = 0; i<n; i++) {
            node = malloc(sizeof(*node));
            sprintf(node->data, "%lu", i);

            STAILQ_INSERT_TAIL(&head->head, node, node);
            head->node_cnt++;

            if (consistent) {
                write_with_tpl(head);

                if (i > verbose_itr) {
                    printf("Operations completed %lu out of %lu\n", i, n);
                    verbose_itr += ten_percent;
                }
            }
        }

        write_with_tpl(head);
    }
    TIMEDIFF_STOP("SLIST loaded");

    exit(EXIT_SUCCESS);
}
