#include "hash.h"

uint32_t DecodeFixed32(const char* ptr)
{
    // Load the raw bytes
    uint32_t result;
    result = ((uint32_t)(ptr[0]) | 
             (uint32_t)(ptr[1] << 8) |
             (uint32_t)(ptr[2] << 16) |
             (uint32_t)(ptr[3] << 24));
    return result;
}

uint32_t Hash(const char* data, size_t n, uint32_t seed)
{
    // Similar to murmur hash
    const uint32_t m = 0xc6a4a793;
    const uint32_t r = 24;
    const char* limit = data + n;
    uint32_t h = seed ^ (n * m);

    // Pick up four bytes at a time
    while (data + 4 <= limit) {
        uint32_t w = DecodeFixed32(data);
        data += 4;
        h += w;
        h *= m;
        h ^= (h >> 16);
    }

    // Pick up remaining bytes
    switch (limit - data) {
        case 3:
            h += data[2] << 16;
        case 2:
            h += data[1] << 8;
        case 1:
            h += data[0];
            h *= m;
            h ^= (h >> r);
            break;
    }
    return h;
}
