/*
 * File:      rng.cc
 * Summary:   Random number generator wrapping.
 */

#include "AppHdr.h"

#include "rng.h"

#include "mt19937ar.h"
#include "syscalls.h"

#ifdef USE_MORE_SECURE_SEED

#ifdef UNIX
// for times()
#include <sys/times.h>
#endif

#undef rename
// for getpid()
#include <sys/types.h>
#include <unistd.h>

#endif

#ifdef MORE_HARDENED_PRNG
#include "sha256.h"
#endif

void seed_rng(unsigned long* seed_key, size_t num_keys)
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

void seed_rng(long seed)
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
    unsigned long seed = time(NULL);
#ifdef USE_MORE_SECURE_SEED

    /* (at least) 256-bit wide seed */
    unsigned long seed_key[8];

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
unsigned long random_int(void)
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
