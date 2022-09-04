#ifndef HTABLE_H
#define HTABLE_H

#include <stdint.h>

struct htable;

struct htable* htable_init();
void htable_free(struct htable*);

void *htable_lookup(struct htable*, void *key);
int htable_insert(struct htable*, void *key, void *val);
void *htable_remove(struct htable*, void *key);

void htable_foreach(struct htable*, void (*fun)(void *key, void *val, void *param), void *param);
void htable_filter(struct htable*, int (*fun)(void *key, void *val, void *param), void *param);

#endif /* end of include guard: HTABLE_H */
