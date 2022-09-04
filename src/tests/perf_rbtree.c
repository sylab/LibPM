#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cont.h>
#include "tpl.h"
#include "tree.h"
#include "timediff.h"

#define ACTION_WRITE    0
#define ACTION_READ     1

#define METHOD_SOFTPM   0
#define METHOD_PTL      1
#define METHOD_VANILLA  2

#define REGPTRS         1
#define UNREGPTRS       1
#define NOP             0

//#define ARRLEN          256     //must be bigger than 1
#define ARRLEN          ((4096 - 4*sizeof(void*)) / sizeof(uint64_t))

const char * program_name;
struct root;
int cid = 0;

struct settings {
    void (*write)(void);    //ptr to write function
    void (*read)(void);     //ptr to read function
    int method;             //use softpm, tpl, or vanilla 
    int action;             //read or write
    uint64_t n;                  //number of nodes
} SETTINGS = { 
    .write = NULL,
    .read = NULL,
    .method = -1,
    .action = -1,
    .n = -1
};

struct entry {
    uint64_t data[ARRLEN];
    RB_ENTRY(entry) node;
};

struct entry * entry_alloc(uint64_t data)
{
    struct entry *p;
    if (SETTINGS.method == METHOD_SOFTPM) {
        p = container_palloc(cid, sizeof(*p));
        pointerat(cid, &p->node.rbe_left);
        pointerat(cid, &p->node.rbe_right);
        pointerat(cid, &p->node.rbe_parent);
    } else {
        p = calloc(1, sizeof(*p));
    }
    p->data[0] = data;
    return p;
}

void entry_free(struct entry * pe) 
{
    if (SETTINGS.method == METHOD_SOFTPM) {
        //freeat(pe); TODO
        free(pe);
    } else {
        free(pe);
    }
}

int entry_cpm(struct entry *a, struct entry *b)
{
    if (a->data[0] < b->data[0])
        return -1;
    if (a->data[0] > b->data[0])
        return 1;
    return 0;
} 

RB_HEAD(rb_tree_test, entry);

struct root {
    //RB_HEAD(rb_tree_test, entry) root;
    struct rb_tree_test root;
} * proot;

RB_PROTOTYPE(rb_tree_test, entry, node, entry_cpm);
RB_GENERATE(rb_tree_test, entry, node, entry_cpm);

void print_usage(FILE* stream, int exit_code)
{
    fprintf(stream, "Usage: %s options\n", program_name);
    fprintf(stream, 
            "  -h  --help              Display usage.\n"
            "  -n  --nodes n           Creates a list of the given number of elements.\n"
            "  -s  --size n[pbkmg]     Alloc a total of n*factor bytes of memory.\n"
            "  -o  --output file       Write output to file.\n"
            "  -m  --modify            Run modify benchmark.\n"
            "  -a  --addrm             Run add/rm benchmark.\n"
            "  -p  --ptrdat            Run ptr/data benchmark.\n"
            "  -t  --threads n         Run threads benchmark with n pthreads.\n"
            "  -r  --read              Run reader to check correctness.\n"
            "  -i  --doincore          Change mode to SoftPM incore.\n"
           );
    exit(exit_code);
}

void tree_init()
{
    assert(ARRLEN > 1);

    if (SETTINGS.method == METHOD_SOFTPM) {
        struct container *cont;
        cont = container_init();
        cid = cont->id;

        proot = container_palloc(cid, sizeof(*proot));
        RB_INIT(&proot->root);
        pointerat(cid, &proot->root.rbh_root);
    } else {
        proot = malloc(sizeof(*proot));
        RB_INIT(&proot->root);
    }


    uint64_t i;
    struct entry * pe;
    for (i=SETTINGS.n; i>0; i--) {
        //pe = entry_alloc(i);
        pe = entry_alloc(rand());
        RB_INSERT(rb_tree_test, &proot->root, pe);
    }
}

void tree_free()
{
    //struct entry * pe;
    //while(LIST_EMPTY(&proot->root)) {
    //pe = LIST_FIRST(&proot->root);
    //LIST_REMOVE(pe, node);
    //RB_REMOVE
    //entry_free(pe);
    //}
}

void tree_print()
{
    struct entry * pe;
    RB_FOREACH(pe, rb_tree_test, &proot->root) {
        printf("%lu %lu\n", pe->data[0], pe->data[1]);
    }
}

/*
 * Insert n random items 
 */
void tree_insert(uint64_t n) 
{
    struct entry * pe;
    uint64_t i;

    for (i=0; i<n; i++) {
        pe = entry_alloc(rand());
        RB_INSERT(rb_tree_test, &proot->root, pe);
    }
}

/*
 * Delete n random items 
 */
void tree_delete(uint64_t n)
{
    uint64_t i = 0;
    struct entry * pe = NULL;
    struct entry * pe_tmp = NULL;
    pe = RB_MIN(rb_tree_test, &proot->root);

    while (i < n) {
        pe_tmp = RB_NEXT(rb_tree_test, &proot->root, pe);
        RB_REMOVE(rb_tree_test, &proot->root, pe);
        entry_free(pe);
        pe = pe_tmp;
        i++;
    }
}

/*
 * Modify n items of the tree
 */
void tree_modify(uint64_t n)
{
    uint64_t i = 0;
    struct entry * pe = NULL;
    pe = RB_MIN(rb_tree_test, &proot->root);

    while (i < n) {
        pe->data[1] = pe->data[0];
        pe = RB_NEXT(rb_tree_test, &proot->root, pe);
        i++;
    }
}

void helper(struct entry * pe, void * data)
{
    //NOTE: do nothing here for now
}

void tree_traverse(void (*fun)(struct entry * _pe, void * _data), void * data) 
{
    struct entry * pe;
    RB_FOREACH(pe, rb_tree_test, &proot->root) {
        fun(pe, data);
    }
}

void tree_operations(uint64_t n)
{
    long double init, mod, ins, rem, que; 

    TIMEDIFF_INIT();

    tree_init();

    TIMEDIFF_TAKE_VAL(SETTINGS.write(), init);

    printf("Op\tInit\tModify\tInsert\tRemove\tQuery\n");

    mod = ins = rem = que = 0.0;
    TIMEDIFF_TAKE_VAL(tree_traverse(helper, NULL); ; SETTINGS.write(), que);
    TIMEDIFF_TAKE_VAL(tree_modify(n); SETTINGS.write(), mod);
    TIMEDIFF_TAKE_VAL(tree_insert(n); SETTINGS.write(), ins);
    /*
    //tree_delete(n); SETTINGS.write();
    //TIMEDIFF_TAKE_VAL(tree_delete(n); SETTINGS.write(), rem);
    printf("%2d%%\t%.3Lf\t%.3Lf\t%.3Lf\t%.3Lf\t%.3Lf\n", 100*n/SETTINGS.n, init, mod, ins, rem, que);
    */
}

void tpl_write() 
{
    struct entry * pe = NULL;
    struct entry e;
    tpl_node * tn = tpl_map("A(i#)", &e, ARRLEN);

    RB_FOREACH(pe, rb_tree_test, &proot->root) {
        e = *pe;
        tpl_pack(tn, 1);
    }

    int fd = open("rbtree.tpl", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
    assert(fd != -1);
    tpl_dump(tn, TPL_FD, fd);
    fsync(fd);
    tpl_free(tn);
    close(fd);
}

void tpl_read()
{
    struct entry * pe = NULL;
    //struct entry * pe_tmp = NULL;   
    struct entry e;

    tpl_node * tn = tpl_map("A(i#)", &e, ARRLEN);
    tpl_load(tn, TPL_FILE, "rbtree.tpl");

    proot = malloc(sizeof(*proot));
    RB_INIT(&proot->root);

    while (tpl_unpack(tn, 1) > 0) {
        pe = entry_alloc(0);
        *pe = e;

        RB_INSERT(rb_tree_test, &proot->root, pe);

        //if (!pe_tmp) {
        //LIST_INSERT_HEAD(&proot->root, pe, node);
        //} else {
        //LIST_INSERT_AFTER(pe_tmp, pe, node);
        //}
        //pe_tmp = pe;
    }

    tpl_free(tn);
}

void softpm_write()
{
    container_cpoint(cid);
}

void softpm_read()
{
    struct container *cont;
    cont = container_restore(cid);
    proot = container_getroot(cid);
    //tree_print();
}

int main(int argc, char *argv[])
{
    int opt;
    int pert = 0;
    TIMEDIFF_INIT();

    program_name = argv[0];

    while ((opt = getopt(argc, argv, "hstrw:n:")) != -1) {
        switch (opt) {
            case 'h': print_usage(stdout, EXIT_SUCCESS); break;
            case 'n': SETTINGS.n = atol(optarg); break;
            case 's': 
                      SETTINGS.method = METHOD_SOFTPM; 
                      SETTINGS.write = &softpm_write;
                      SETTINGS.read = &softpm_read;
                      break;
            case 't': 
                      SETTINGS.method = METHOD_PTL; 
                      SETTINGS.write = &tpl_write;
                      SETTINGS.read = &tpl_read;
                      break;
            case 'w': 
                      SETTINGS.action = ACTION_WRITE; 
                      pert = atoi(optarg);
                      break;
            case 'r': SETTINGS.action = ACTION_READ; break;
            default: /* '?' */
                      print_usage(stderr, EXIT_FAILURE);
        }
    }

    printf("# method: %s action: %s\n",
            SETTINGS.method == METHOD_SOFTPM ? "softpm" : "tpl",
            SETTINGS.action == ACTION_WRITE ? "write" : "read" );

    if (SETTINGS.method == METHOD_SOFTPM) {
        //pdl_start();
    }

    if (SETTINGS.action == ACTION_WRITE) {
        tree_operations((int)((double)(SETTINGS.n*pert)/100.0));
    }

    if (SETTINGS.action == ACTION_READ) {
        TIMEDIFF_TAKE(SETTINGS.read(), "rbtree read:");
        //tree_print();
    }

    if (SETTINGS.method == METHOD_SOFTPM) 
        //pdl_shutdown();

    //TODO: free all memory here

    exit(EXIT_SUCCESS);
}

