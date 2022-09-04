#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cont.h>

#define PAGE_SIZE   4096
#define BIG_SIZE    2050
#define SMALL_SIZE  512
#define SMALL_CNT   (2 * PAGE_SIZE / SMALL_SIZE)
#define BIG_CNT     (10)

int main(int argc, const char *argv[])
{
    int cid = 0;
    struct container *cont;
    cont = container_restore(cid);
    return 0;
}
