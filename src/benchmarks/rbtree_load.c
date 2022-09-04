#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "rbtree.h"
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

void write_with_tpl(struct rbroot *root)
{
    uint64_t key;
    char *data;
    tpl_node *tn = tpl_map("UiA(Us)", &root->node_cnt, &root->node_size, &key, &data);
    tpl_pack(tn, 0);

    //resializing the nodes
    struct rbnode *itr = NULL;
    RB_FOREACH(itr, root_struct, &root->root) {
        key = itr->key;
        data = itr->data;
        tpl_pack(tn, 1);
    }

    //writing to file
    int fd = open(RBTREE_TPL_FILE, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
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
    struct rbroot *root;
    struct rbnode *node;
    uint64_t ten_percent = n / 10;
    uint64_t verbose_itr = ten_percent;

    if (backend_engine == BACKEND_PMLIB) {

        //init container
        struct container *cont = container_init();

        //allocate root of the RB Tree
        root = container_palloc(cont->id, sizeof(*root));
        root->node_size = node_size;
        RB_INIT(&root->root);
        pointerat(cont->id, &root->root.rbh_root);

        if (consistent)
            container_cpoint(cont->id);

        //allocate all nodes and insert them into tree
        for (uint64_t i = 0; i<n; i++) {
            if (alloc_with_malloc) {
                node = malloc(node_size);
                mallocat(node, node_size);
            } else
                node = container_palloc(cont->id, node_size);

            node->key = i;
            sprintf(node->data, "%lu", i);
            RB_INSERT(root_struct, &root->root, node);
            root->node_cnt++;

            pointerat(cont->id, &node->node.rbe_left);
            pointerat(cont->id, &node->node.rbe_right);
            pointerat(cont->id, &node->node.rbe_parent);

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

        //creating tree root
        root = malloc(sizeof(*root));
        root->node_size = node_size;
        RB_INIT(&root->root);

        //adding nodes to the tree
        for (uint64_t i = 0; i<n; i++) {
            node = malloc(sizeof(*node));
            node->key = i;
            sprintf(node->data, "%lu", i);

            RB_INSERT(root_struct, &root->root, node);
            root->node_cnt++;

            if (consistent) {
                write_with_tpl(root);

                if (i > verbose_itr) {
                    printf("Operations completed %lu out of %lu\n", i, n);
                    verbose_itr += ten_percent;
                }
            }
        }

        write_with_tpl(root);

#if 0
        //serializing the root
        uint64_t key;
        char *data;
        tpl_node *tn = tpl_map("UiA(Us)", &root->node_cnt, &root->node_size, &key, &data);
        tpl_pack(tn, 0);

        //resializing the nodes
        struct rbnode *itr = NULL;
        RB_FOREACH(itr, root_struct, &root->root) {
            key = itr->key;
            data = itr->data;
            tpl_pack(tn, 1);
        }

        //writing to file
        int fd = open("rbtree.tpl", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
        assert(fd != -1);
        tpl_dump(tn, TPL_FD, fd);
        fsync(fd);
        tpl_free(tn);
        close(fd);
#endif
    }
    TIMEDIFF_STOP("RBTree loaded");

    exit(EXIT_SUCCESS);
}
