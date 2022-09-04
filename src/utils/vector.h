#ifndef SIMPLE_VECTOR_H
#define SIMPLE_VECTOR_H

#include <assert.h>
#include <stdlib.h>
#include "macros.h"

#define INIT_SIZE   2

#define VECTOR_DECL(name, type) \
struct name { \
    int capacity; \
    int size; \
    typeof(type) *buffer; \
}

#define VECTOR_INITAT(p, c) do { \
    (p)->capacity = c; \
    (p)->size = 0; \
    int buffer_size = sizeof(typeof(*(p)->buffer)) * (p)->capacity; \
    (p)->buffer = malloc(buffer_size); \
    assert((p)->buffer && "failed to allocate memory for abuffer"); \
} while(0)

#define VECTOR_INIT(p)    VECTOR_INITAT(p, INIT_SIZE);

#define VECTOR_FREE(p) do { \
	free((p)->buffer); \
	(p)->buffer = 0; \
	(p)->capacity = 0; \
	(p)->size = 0; \
} while (0)

#define __VECTOR_DOUBLE(p) do { \
    (p)->capacity = MAX(INIT_SIZE, (p)->capacity *2); \
    int buffer_size = sizeof(typeof(*(p)->buffer)) * (p)->capacity; \
    (p)->buffer = realloc((p)->buffer, buffer_size); \
    assert((p)->buffer && "failed to reallocate memory for abuffer"); \
} while(0)

#define VECTOR_APPEND(p, item) do { \
    if ((p)->size == (p)->capacity) \
        __VECTOR_DOUBLE(p); \
    (p)->buffer[(p)->size++] = item; \
} while(0)

#define VECTOR_FOREACH(p, itr, i) \
    for (i = 0, itr = (p)->buffer[0]; \
            i < (p)->size; \
                itr = (p)->buffer[++i])

#define VECTOR_SIZE(p)     (p)->size
#define VECTOR_CAPACITY(p) (p)->capacity
#define VECTOR_AT(p, idx)  (p)->buffer[idx]
#define VECTOR_BUFFSIZE(p) (sizeof(typeof(*(p)->buffer)) * (p)->capacity)


//#define SIMPLE_VECTOR_TEST_MODE
#ifdef SIMPLE_VECTOR_TEST_MODE

// To use this test, rename the file to abuffer.c and run this command
// $ rm -f vector; clang -Wall -g vector.c -o vector; ./vector

#include <stdio.h>
#include <stdint.h>

int main(int argc, const char *argv[])
{
    VECTOR_DECL(int_array, void*) v;
    VECTOR_INIT(&v);

    for (int i = 0; i < 10; i++) {
        VECTOR_APPEND(&v, (void*)(&v + i));
        printf("%lu\n", VECTOR_BUFFSIZE(&v));
    }

    void *itr;
    int i;
    VECTOR_FOREACH(&v, itr, i) {
        printf("%p\n", itr);
    }

    VECTOR_FREE(&v);

    for (int i = 0; i < 10; i++) {
	VECTOR_APPEND(&v, (void*)(&v + i));
	printf("%lu\n", VECTOR_BUFFSIZE(&v));
    }

    VECTOR_FOREACH(&v, itr, i) {
	printf("%p\n", itr);
    }

    return 0;
}

#endif

#endif /* end of include guard: SIMPLE_VECTOR_H */
