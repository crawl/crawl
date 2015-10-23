/* This class implements a 128-bit pseudo-random number generator.
 * Despite a reduction in state space, it still passes TestU01 BigCrush.
 *
 * get_uint32 is derived from M.E. O'Neill's minimal PCG implementation.
 * That function (c) 2014 M.E. O'Neill / pcg-random.org
 * Licensed under Apache License 2.0
 */

#include "AppHdr.h"

#include "pcg.h"

static PcgRNG pcg_rng[2];

PcgRNG &PcgRNG::generator(int which)
{
    ASSERT(which >= 0);
    ASSERT((size_t) which < ARRAYSZ(pcg_rng));
    return pcg_rng[which];
}

uint32_t
PcgRNG::get_uint32()
{
    uint64_t oldstate = state_;
    // Advance internal state
    state_ = oldstate * 6364136223846793005ULL + (inc_|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

uint64_t
PcgRNG::get_uint64()
{
  return static_cast<uint64_t>(get_uint32()) << 32 | get_uint32();
}

PcgRNG::PcgRNG()
{
    // Choose base state arbitrarily. There's nothing up my sleeve.
    state_ = 18446744073709551557ULL; // Largest 64-bit prime
    inc_ =   2305843009213693951ULL; // Largest Mersenne prime under 64-bits
}

PcgRNG::PcgRNG(uint64_t init_key[], int key_length)
{

    if (key_length > 0)
        state_ = init_key[0];
    if (key_length > 1)
    {
        inc_ = init_key[1];
    }
    else
    {
        inc_ ^= get_uint32();      
    }
}

uint32_t get_uint32(int generator)
{
    return PcgRNG::generator(generator).get_uint32();
}

uint32_t get_uint64(int generator)
{
    return PcgRNG::generator(generator).get_uint64();
}

void seed_rng(uint64_t seed_array[], int seed_len)
{
    PcgRNG seeded(seed_array, seed_len);
    // Use the just seeded RNG to initialize the rest.
    for (size_t i = 0; i < ARRAYSZ(pcg_rng); ++i)
    {
        uint64_t key[2] = { seeded.get_uint64(), seeded.get_uint64() };
        pcg_rng[i] = PcgRNG(key, ARRAYSZ(key));
    }
}
