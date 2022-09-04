#ifndef HASH_H
#define HASH_H

#include <stddef.h>
#include <stdint.h>

uint32_t Hash(const char* data, size_t n, uint32_t seed);

#endif /* end of include guard: HASH_H */
