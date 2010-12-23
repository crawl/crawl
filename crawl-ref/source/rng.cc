/*
 * File:      rng.cc
 * Summary:   Random number generator wrapping.
 */

#include "AppHdr.h"

#include "rng.h"

#include "mt19937ar.h"

#ifdef USE_MORE_SECURE_SEED

// for times()
#include <sys/times.h>

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

    struct tms  buf;
    seed += times(&buf) + getpid();
    seed_key[0] = seed;

    /* Try opening from various system provided (hopefully) CSPRNGs */
    FILE* seed_f = fopen("/dev/urandom", "rb");
    if (!seed_f)
        seed_f = fopen("/dev/random", "rb");
    if (!seed_f)
        seed_f = fopen("/dev/srandom", "rb");
    if (!seed_f)
        seed_f = fopen("/dev/arandom", "rb");
    if (seed_f)
    {
        fread(&seed_key[1], sizeof(unsigned long), 7, seed_f);
        fclose(seed_f);
    }

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
