#ifndef HASH_H
#define HASH_H

static uint64_t hash3(uint64_t x, uint64_t y, uint64_t z)
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

#endif
