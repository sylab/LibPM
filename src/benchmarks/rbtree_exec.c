#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <cont.h>
#include <timediff.h>
#include <macros.h>
#include "rbtree.h"
#include "distro.h"
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
            "  -c       Create a consistent point after every modification.\n");
    exit(exit_code);
}

void read_node_data(struct rbnode *node, int node_size)
{
    int data_len = rbnode_datalen(node_size);
    int i;
    char c;
    for (i = 0; i < data_len; i++) {
        c = node->data[i];
    }
}

void write_node_data(struct rbnode *node, int node_size)
{
    int data_len = rbnode_datalen(node_size);
    int i;
    char c;
    for (i = 0; i < data_len; i++) {
        node->data[i] = (rand() % 26) + 'a';
    }
    node->data[data_len - 1] = '\0';
}

void write_with_tpl(struct rbroot *root, void *not_in_use)
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
    int fd = open("rbtree.tpl", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
    assert(fd != -1);
    tpl_dump(tn, TPL_FD, fd);
    fsync(fd);
    tpl_free(tn);
    close(fd);
}


void write_with_pmlib(struct rbroot *root, void *arg)
{
    int cid = ptoi(arg);
    container_cpoint(cid);
}

void workloadA(struct rbroot *root, uint64_t n, int consistent,
               void (*write_func)(struct rbroot *, void* ), void *write_args)
{
    int next_access;
    uint64_t next_key;
    struct rbnode find, *node;
    uint64_t read_cnt = 0, writes_cnt = 0;

    TIMEDIFF_INIT();
    TIMEDIFF_START();

    uint64_t ten_percent = n / 10;
    uint64_t verbose_itr = ten_percent;
    for (uint64_t i = 0; i < n; i++) {
        next_key = getnext_seq(root->node_cnt);
        next_access = getnext_50rw();
        find.key = next_key;

        node = RB_FIND(root_struct, &root->root, &find);
        assert(node && "The node was not found");

        if (next_access == READ) {
            read_node_data(node, root->node_size);
            read_cnt++;
        } else {
            write_node_data(node, root->node_size);
            writes_cnt++;

            if (consistent) {
                write_func(root, write_args);

                if (i > verbose_itr) {
                    printf("Operations completed %lu out of %lu\n", i, n);
                    verbose_itr += ten_percent;
                }
            }
        }
    }

    if (!consistent) {
        write_func(root, write_args);
    }

    TIMEDIFF_STOP("WorkloadA: 50/50 reads and writes");
    printf("reads: %lu, writes: %lu\n", read_cnt, writes_cnt);
}

void workloadB(struct rbroot *root, uint64_t n, int consistent,
               void (*write_func)(struct rbroot *, void* ), void *write_args)
{
    int next_access;
    uint64_t next_key;
    struct rbnode find, *node;
    uint64_t read_cnt = 0, writes_cnt = 0;

    TIMEDIFF_INIT();
    TIMEDIFF_START();

    uint64_t ten_percent = n / 10;
    uint64_t verbose_itr = ten_percent;
    for (uint64_t i = 0; i < n; i++) {
        next_key = getnext_seq(root->node_cnt);
        next_access = getnext_95rw();
        find.key = next_key;

        node = RB_FIND(root_struct, &root->root, &find);
        assert(node && "The node was not found");

        if (next_access == READ) {
            read_node_data(node, root->node_size);
            read_cnt++;
        } else {
            write_node_data(node, root->node_size);
            writes_cnt++;

            if (consistent) {
                write_func(root, write_args);

                if (i > verbose_itr) {
                    printf("Operations completed %lu out of %lu\n", i, n);
                    verbose_itr += ten_percent;
                }
            }
        }
    }

    if (!consistent) {
        write_func(root, write_args);
    }

    TIMEDIFF_STOP("WorkloadB: 95/5 reads/write mix");
    printf("reads: %lu, writes: %lu\n", read_cnt, writes_cnt);
}

void workloadC(struct rbroot *root, uint64_t n, int consistent,
               void (*write_func)(struct rbroot *, void* ), void *write_args)
{
    int next_access;
    uint64_t next_key;
    struct rbnode find, *node;
    uint64_t read_cnt = 0, writes_cnt = 0;

    TIMEDIFF_INIT();
    TIMEDIFF_START();

    uint64_t ten_percent = n / 10;
    uint64_t verbose_itr = ten_percent;
    for (uint64_t i = 0; i < n; i++) {
        next_key = getnext_seq(root->node_cnt);
        next_access = READ;
        find.key = next_key;

        node = RB_FIND(root_struct, &root->root, &find);
        assert(node && "The node was not found");

        read_node_data(node, root->node_size);
        read_cnt++;
    }

    TIMEDIFF_STOP("Workload C: read only");
    printf("reads: %lu, writes: %lu\n", read_cnt, writes_cnt);
}

int detectWorkload(char *str)
{
    if (strcmp(str, "a") == 0) {
        return 'a';
    } else if (strcmp(str, "b") == 0) {
        return 'b';
    } else if (strcmp(str, "c") == 0) {
        return 'c';
    }
    return -1;
}

int main(int argc, char * const argv[])
{
    int opt;
    uint64_t n = 0;
    int consistent = 0;
    int backend_engine = 0;
    int workload = 0;
    program_name = argv[0];

    while ((opt = getopt(argc, argv, "hn:ctpw:")) != -1) {
        switch (opt) {
            case 'h': print_usage(stdout, EXIT_SUCCESS); break;
            case 'n': n = atoll(optarg); break;
            case 't': backend_engine = BACKEND_TPL; break;
            case 'p': backend_engine = BACKEND_PMLIB; break;
            case 'c': consistent = 1; break;
            case 'w': workload = detectWorkload(optarg); break;
            default: print_usage(stderr, EXIT_FAILURE);

        }
    }

    if ((backend_engine != BACKEND_PMLIB) && (backend_engine != BACKEND_TPL)) {
        fprintf(stderr, "Backend engine not specified.\n");
        exit(EXIT_FAILURE);
    }

    struct rbroot *root = NULL;
    void (*write_func)(struct rbroot*, void*) = NULL;
    void *write_args = NULL;

    if (backend_engine == BACKEND_PMLIB) {
        struct container *cont = container_restore(0);
        root = container_getroot(cont->id);
        write_func = write_with_pmlib;
        write_args = itop(cont->id);
    } else /* backend_engine == BACKEND_TPL */ {
        root = malloc(sizeof(*root));
        assert(root && "Failed to allocate root");
        RB_INIT(&root->root);

        uint64_t key;
        char *data;
        tpl_node *tn = tpl_map("UiA(Us)", &root->node_cnt, &root->node_size, &key, &data);
        tpl_load(tn, TPL_FILE, RBTREE_TPL_FILE);

        // unpacking root data
        tpl_unpack(tn, 0);
        printf("root {cnt: %lu, node_size: %d}\n", root->node_cnt, root->node_size);

        uint64_t cnt = 0;
        while (tpl_unpack(tn, 1) > 1) {
            struct rbnode *node = malloc(root->node_size);
            assert(node && "Failed to allocate node");
            node->key = key;
            memcpy(node->data, data, rbnode_datalen(root->node_size));
            RB_INSERT(root_struct, &root->root, node);
            cnt++;
        }

        tpl_free(tn);

        if (cnt != root->node_cnt) {
            fprintf(stderr, "No all nodes were unpacked. We found %lu out of %lu.\n",
                    cnt, root->node_cnt);
            fprintf(stderr, "Setting node count to %lu.\n", cnt);
            root->node_cnt = cnt;
        }

        write_func = write_with_tpl;
    }

    switch (workload) {
        case 'a': workloadA(root, n, consistent, write_func, write_args); break;
        case 'b': workloadB(root, n, consistent, write_func, write_args); break;
        case 'c': workloadC(root, n, consistent, write_func, write_args); break;
        default: exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
