#include "hash.h"
#include "htable.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct htable_entry {
    uint32_t hash;
    void *key;
    void *val;
    struct htable_entry *next_hash;
};

struct htable {
    uint32_t length;
    uint32_t elems;
    struct htable_entry **list;
};

struct htable_entry *htable_entry_alloc(void *key, void *val, uint32_t hash)
{
    struct htable_entry *e = malloc(sizeof(*e));
    assert(e && "failed to allocate memory for htable_entry");
    e->key = key;
    e->val = val;
    e->hash = hash;
    e->next_hash = NULL;
    return e;
}

static struct htable_entry **
htable_find_pointer(struct htable *ht, void *key, uint32_t hash)
{
    struct htable_entry **ptr = &ht->list[hash & (ht->length - 1)];
    while (*ptr != NULL && ((*ptr)->hash != hash || key != (*ptr)->key)) {
        ptr = &(*ptr)->next_hash;
    }
    return ptr;
}

static void htable_resize(struct htable *ht)
{
    uint32_t new_length = 4;
    while (new_length < ht->elems) {
        new_length *= 2;
    }

    struct htable_entry **new_list = malloc(sizeof(void*) * new_length);
    assert(new_list && "failed to resize the array on the hash table");
    memset(new_list, 0, sizeof(void*) * new_length);

    int count = 0;
    for (int i = 0; i < ht->length; i++) {
        struct htable_entry *e = ht->list[i];
        while (e != NULL) {
            struct htable_entry *next = e->next_hash;
            uint32_t hash = e->hash;
            struct htable_entry **ptr = &new_list[hash & (new_length - 1)];
            e->next_hash = *ptr;
            *ptr = e;
            e = next;
            count++;
        }
    }

    assert(ht->elems == count);
    free(ht->list);
    ht->list = new_list;
    ht->length = new_length;
}

struct htable* htable_init()
{
    struct htable *ht = malloc(sizeof(*ht));
    assert(ht && "failed to allocate htable");
    ht->length = 0;
    ht->elems = 0;
    ht->list = NULL;

    htable_resize(ht);
    return ht;
}

void htable_free(struct htable *ht)
{
    int count = 0;
    for (int i = 0; i < ht->length; i++) {
        struct htable_entry *e = ht->list[i];
        while (e != NULL) {
            struct htable_entry *next = e->next_hash;
            free(e);
            e = next;
            count++;
        }
    }

    assert(ht->elems == count);
    free(ht->list);
    free(ht);
}

int htable_insert(struct htable *ht, void *key, void *val)
{
    uint32_t hash = Hash((char*)&key, sizeof(void*), 0);
    struct htable_entry **ptr = htable_find_pointer(ht, key, hash);
    int ret = 0;

    if (*ptr == NULL) {
        struct htable_entry *new_entry = htable_entry_alloc(key, val, hash);
        *ptr = new_entry;
        ret = 1;

        ++ht->elems;
        if (ht->elems > ht->length)
            htable_resize(ht);
    } else {
        //NOTE: is val points to an allocated obj, it should be freed here!
        (*ptr)->val = val;
    }

    return ret;
}

void *htable_lookup(struct htable *ht, void *key)
{
    uint32_t hash = Hash((char*)&key, sizeof(void*), 0);
    struct htable_entry **ptr = htable_find_pointer(ht, key, hash);
    return (*ptr) != NULL ? (*ptr)->val : NULL;
}

void *htable_remove(struct htable *ht, void *key)
{
    int ret = 0;
    uint32_t hash = Hash((char*)&key, sizeof(void*), 0);
    struct htable_entry **ptr = htable_find_pointer(ht, key, hash);
    struct htable_entry *result = *ptr;
    if (result != NULL) {
        *ptr = result->next_hash;
        ht->elems--;
        return result->val;
    }
    return NULL;
}

/* only for debugging */
void htable_print(struct htable *ht)
{
    int count = 0;
    for (int i = 0; i < ht->length; i++) {
        struct htable_entry *e = ht->list[i];
        while (e != NULL) {
            printf("htable_entry { hash: %x, key: %p, val: %p }\n",
                    e->hash, e->key, e->val);
            e = e->next_hash;
            count++;
        }
    }

    assert(ht->elems == count);
}

void htable_foreach(struct htable *ht,
                    void (*fun)(void *key, void *val, void *param),
                    void *param)
{
    int count = 0;
    for (int i = 0; i < ht->length; i++) {
        struct htable_entry *e = ht->list[i];
        while (e != NULL) {
            fun(e->key, e->val, param);
            e = e->next_hash;
            count++;
        }
    }

    assert(ht->elems == count);
}

void htable_filter(struct htable *ht,
                    int (*fun)(void *key, void *val, void *param),
                    void *param)
{
    for (int i = 0; i < ht->length; i++) {
        struct htable_entry *e = ht->list[i];
        struct htable_entry **ptr = &ht->list[i];
        while (e != NULL) {
            if (fun(e->key, e->val, param)) {
                ht->elems--;
                *ptr = e->next_hash;
                free(e);
                e = *ptr;
            } else {
                ptr = &e->next_hash;
                e = e->next_hash;
            }
        }
    }
}
