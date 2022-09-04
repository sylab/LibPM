#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#include "distro.h"

double unirand()
{
    static int init = 0;
    if (!init) {
        srand (time(NULL));
        init++;
    }
    return rand()/(double)RAND_MAX;
}

static int rand_access_type(double treshold)
{
    return (unirand() < treshold) ? WRITE : READ;
}

int getnext_50rw()
{
    return rand_access_type(0.5);
}

int getnext_95rw()
{
    return rand_access_type(0.05);
}

uint64_t getnext_seq(uint64_t max)
{
    static uint64_t itr = 0;
    return itr++ % max;
}

uint64_t getnext_zipfian(uint64_t max)
{
    //TODO: finish this
    return 0;
}

#ifdef TEST

int main(int argc, const char *argv[])
{
    int i;
    char *access[] = { "READ", "WRITE" };
    
    for (i = 0; i < 30; i++) {
        //printf("%s\n", getnext_50rw() == READ ? access[READ] : access[WRITE]);
        //printf("%s\n", getnext_95rw() == READ ? access[READ] : access[WRITE]);
        printf("%lu\n", getnext_seq(13));
    }
    
    return 0;
}

#endif
