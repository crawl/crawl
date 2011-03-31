/*
 * File:      rng.cc
 * Summary:   Random number generator wrapping.
 */

#include "AppHdr.h"

#include "rng.h"

#include "endianness.h"
#include "mt19937ar.h"
#include "syscalls.h"

#ifdef USE_MORE_SECURE_SEED

#ifdef UNIX
// for times()
#include <sys/times.h>
#endif

// for getpid()
#include <sys/types.h>
#include <unistd.h>

#endif

#ifdef MORE_HARDENED_PRNG
#include "sha256.h"
#endif

void seed_rng(uint32_t* seed_key, size_t num_keys)
{
    // MT19937 -- see mt19937ar.cc for details/licence
    init_by_array(seed_key, num_keys);

    // Reset the sha256 generator to get predictable random numbers in case
    // of a saved rng state.
#ifdef MORE_HARDENED_PRNG
    reset_sha256_state();
#endif

    // for std::random_shuffle()
    srand(seed_key[0]);
}

void seed_rng(uint32_t seed)
{
    // MT19937 -- see mt19937ar.cc for details/licence
    init_genrand(seed);

    // Reset the sha256 generator to get predictable random numbers in case
    // of a saved rng state.
#ifdef MORE_HARDENED_PRNG
    reset_sha256_state();
#endif

    // for std::random_shuffle()
    srand(seed);
}

void seed_rng()
{
    uint32_t seed = time(NULL);
#ifdef USE_MORE_SECURE_SEED

    /* (at least) 256-bit wide seed */
    uint32_t seed_key[8];

#ifdef UNIX
    struct tms  buf;
    seed += times(&buf);
#endif

    seed += getpid();
    seed_key[0] = seed;

    read_urandom((char*)(&seed_key[1]), sizeof(seed_key[0]) * 7);
    seed_rng(seed_key, 8);

#else
    seed_rng(seed);
#endif
}

// MT19937 -- see mt19937ar.cc for details
uint32_t random_int(void)
{
#ifndef MORE_HARDENED_PRNG
    return (genrand_int32());
#else
    return (sha256_genrand());
#endif
}

void push_rng_state()
{
#ifndef MORE_HARDENED_PRNG
    push_mt_state();
#else
    push_sha256_state();
#endif
}

void pop_rng_state()
{
#ifndef MORE_HARDENED_PRNG
    pop_mt_state();
#else
    pop_sha256_state();
#endif
}

//-----------------------------------------------------------------------------
// MurmurHash2, by Austin Appleby
uint32_t hash(const void *data, int len)
{
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.
    const uint32_t m = 0x5bd1e995;

    // Initialize the hash to a 'random' value
    uint32_t h = len;

    const uint8_t *d = (const uint8_t*)data;
    // Mix 4 bytes at a time into the hash
    while(len >= 4)
    {
        uint32_t k = htole32(*(uint32_t *)d);

        k *= m;
        k ^= k >> 24;
        k *= m;

        h *= m;
        h ^= k;

        d += 4;
        len -= 4;
    }

    // Handle the last few bytes of the input array
    switch(len)
    {
    case 3: h ^= (uint32_t)d[2] << 16;
    case 2: h ^= (uint32_t)d[1] << 8;
    case 1: h ^= (uint32_t)d[0];
            h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}
