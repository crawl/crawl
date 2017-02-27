#pragma once

static inline uint64_t hash3(uint64_t x, uint64_t y, uint64_t z)
{
    // Some compilers choke on big unsigneds, need to give them in hex.
    uint64_t hash=0xcbf29ce484222325ULL; // 14695981039346656037
    #define FNV64 1099511628211ULL
    hash^=x;
    hash*=FNV64;
    hash^=y;
    hash*=FNV64;
    hash^=z;
    hash*=FNV64;
    x=hash ^ (hash >> 27) ^ (hash << 24) ^ (hash >> 48);
    return x;
}

uint32_t hash32(const void *data, int len) PURE;
unsigned int hash_rand(int x, uint32_t seed, uint32_t id = 0);

