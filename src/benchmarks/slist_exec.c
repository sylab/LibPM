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
#include "slist.h"
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

void read_node_data(struct slist_node *node, int node_size)
{
    int data_len = slist_datalen(node_size);
    int i;
    char c;
    for (i = 0; i < data_len; i++) {
        c = node->data[i];
    }
}

void write_node_data(struct slist_node *node, int node_size)
{
    int data_len = slist_datalen(node_size);
    int i;
    char c;
    for (i = 0; i < data_len; i++) {
        node->data[i] = (rand() % 26) + 'a';
    }
    node->data[data_len - 1] = '\0';
}

void write_with_tpl(struct slist_head *head, void *not_in_use)
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


void write_with_pmlib(struct slist_head *head, void *arg)
{
    int cid = ptoi(arg);
    container_cpoint(cid);
}

void workloadA(struct slist_head *head, uint64_t n, int consistent,
               void (*write_func)(struct slist_head *, void* ), void *write_args)
{
    int next_access;
    struct slist_node *node;
    uint64_t read_cnt = 0, writes_cnt = 0;

    TIMEDIFF_INIT();
    TIMEDIFF_START();

    uint64_t ten_percent = n / 10;
    uint64_t verbose_itr = ten_percent;
    uint64_t ope = 0;

    while (ope < n) {
        STAILQ_FOREACH(node, &head->head, node) {
            if (ope == n) break;
            next_access = getnext_50rw();
            ope++;
            if (next_access == READ) {
                read_node_data(node, head->node_size);
                read_cnt++;
            } else {
                write_node_data(node, head->node_size);
                writes_cnt++;

                if (consistent) {
                    write_func(head, write_args);

                    if (ope > verbose_itr) {
                        printf("Operations completed %lu out of %lu\n", ope, n);
                        verbose_itr += ten_percent;
                    }
                }
            }
        }
    }

    if (!consistent) {
        write_func(head, write_args);
    }

    TIMEDIFF_STOP("WorkloadA: 50/50 reads and writes");
    printf("reads: %lu, writes: %lu\n", read_cnt, writes_cnt);
}

void workloadB(struct slist_head *head, uint64_t n, int consistent,
               void (*write_func)(struct slist_head *, void* ), void *write_args)
{
    int next_access;
    struct slist_node *node;
    uint64_t read_cnt = 0, writes_cnt = 0;

    TIMEDIFF_INIT();
    TIMEDIFF_START();

    uint64_t ten_percent = n / 10;
    uint64_t verbose_itr = ten_percent;
    uint64_t ope = 0;

    while (ope < n) {
        STAILQ_FOREACH(node, &head->head, node) {
            if (ope == n) break;
            next_access = getnext_95rw();
            ope++;
            if (next_access == READ) {
                read_node_data(node, head->node_size);
                read_cnt++;
            } else {
                write_node_data(node, head->node_size);
                writes_cnt++;

                if (consistent) {
                    write_func(head, write_args);

                    if (ope > verbose_itr) {
                        printf("Operations completed %lu out of %lu\n", ope, n);
                        verbose_itr += ten_percent;
                    }
                }
            }
        }
    }

    if (!consistent) {
        write_func(head, write_args);
    }

    TIMEDIFF_STOP("WorkloadB: 95/5 reads and writes");
    printf("reads: %lu, writes: %lu\n", read_cnt, writes_cnt);
}

void workloadC(struct slist_head *head, uint64_t n, int consistent,
               void (*write_func)(struct slist_head *, void* ), void *write_args)
{
    struct slist_node *node;
    uint64_t read_cnt = 0, writes_cnt = 0;

    TIMEDIFF_INIT();
    TIMEDIFF_START();

    uint64_t ten_percent = n / 10;
    uint64_t verbose_itr = ten_percent;
    uint64_t ope = 0;

    while (ope < n) {
        STAILQ_FOREACH(node, &head->head, node) {
            if (ope == n) break;
            ope++;
            read_node_data(node, head->node_size);
            read_cnt++;
        }
    }

    TIMEDIFF_STOP("WorkloadC: Read only");
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

    struct slist_head *head = NULL;
    void (*write_func)(struct slist_head*, void*) = NULL;
    void *write_args = NULL;

    if (backend_engine == BACKEND_PMLIB) {
        struct container *cont = container_restore(0);
        head = container_getroot(cont->id);
        write_func = write_with_pmlib;
        write_args = itop(cont->id);
    } else /* backend_engine == BACKEND_TPL */ {
        head = malloc(sizeof(*head));
        assert(head && "Failed to allocate head");
        STAILQ_INIT(&head->head);

        char *data;
        tpl_node *tn = tpl_map("UiA(s)", &head->node_cnt, &head->node_size, &data);
        tpl_load(tn, TPL_FILE, SLIST_TPL_FILE);

        // unpacking head data
        tpl_unpack(tn, 0);
        printf("head {cnt: %lu, node_size: %d}\n", head->node_cnt, head->node_size);

        uint64_t cnt = 0;
        while (tpl_unpack(tn, 1) > 1) {
            struct slist_node *node = malloc(head->node_size);
            assert(node && "Failed to allocate node");
            memcpy(node->data, data, slist_datalen(head->node_size));
            STAILQ_INSERT_TAIL(&head->head, node, node);
            cnt++;
        }

        tpl_free(tn);

        if (cnt != head->node_cnt) {
            fprintf(stderr, "No all nodes were unpacked. We found %lu out of %lu.\n",
                    cnt, head->node_cnt);
            fprintf(stderr, "Setting node count to %lu.\n", cnt);
            head->node_cnt = cnt;
        }

        write_func = write_with_tpl;
    }

    switch (workload) {
        case 'a': workloadA(head, n, consistent, write_func, write_args); break;
        case 'b': workloadB(head, n, consistent, write_func, write_args); break;
        case 'c': workloadC(head, n, consistent, write_func, write_args); break;
        default: exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
