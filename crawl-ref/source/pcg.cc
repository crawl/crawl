/* This class implements a 128-bit pseudo-random number generator.
 * Despite a reduction in state space, it still passes TestU01 BigCrush.
 *
 * get_uint32 is derived from M.E. O'Neill's minimal PCG implementation.
 * That function (c) 2014 M.E. O'Neill / pcg-random.org
 * Licensed under Apache License 2.0
 */

#include "AppHdr.h"

#include "pcg.h"

uint32_t
PcgRNG::get_uint32()
{
    count_++;
    uint64_t oldstate = state_;
    // Advance internal state
    state_ = oldstate * static_cast<uint64_t>(6364136223846793005ULL) + (inc_|1);
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
      // Choose base state arbitrarily. There's nothing up my sleeve.
    : state_(static_cast<uint64_t>(18446744073709551557ULL)), // Largest 64-bit prime.
      inc_(static_cast<uint64_t>(2305843009213693951ULL)),    // Largest Mersenne prime under 64-bits.
      count_(0)

{ }

PcgRNG::PcgRNG(uint64_t init_key[], int key_length)
    : PcgRNG()
{
    if (key_length > 0)
        state_ = init_key[0];
    if (key_length > 1)
        inc_ = init_key[1];
    else
        inc_ ^= get_uint32();
}

PcgRNG::PcgRNG(const CrawlVector &v)
    : PcgRNG()
{
    ASSERT(v.size() == 2);
    state_ = static_cast<uint64_t>(v[0].get_int64());
    inc_ = static_cast<uint64_t>(v[1].get_int64());
}

CrawlVector PcgRNG::to_vector()
{
    CrawlVector store;
    store.push_back(static_cast<int64_t>(state_));
    store.push_back(static_cast<int64_t>(inc_));
    return store;
}
