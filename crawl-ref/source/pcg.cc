/* This class implements a 128-bit pseudo-random number generator.
 * Despite a reduction in state space, it still passes TestU01 BigCrush.
 *
 * get_uint32 is derived from M.E. O'Neill's minimal PCG implementation.
 * That function (c) 2014 M.E. O'Neill / pcg-random.org
 * Licensed under Apache License 2.0
 */

#include "AppHdr.h"

#include "pcg.h"

namespace rng
{
    uint32_t
    PcgRNG::get_uint32()
    {
        count_++;
        uint64_t oldstate = state_;
        // Advance internal state. Use the 'official' multiplier. Don't change
        // this without carefully consulting official sources, as not all
        // multipliers are ok: see
        // http://www.pcg-random.org/posts/critiquing-pcg-streams.html
        state_ = oldstate * static_cast<uint64_t>(6364136223846793005ULL)
                                                                + (inc_|1);
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

    // Initialization values only.
    // don't generate unseeded versions of this. But if you must, go get the
    // constants from the official PCG implementation...
    PcgRNG::PcgRNG()
        : state_(static_cast<uint64_t>(0)),
          inc_(static_cast<uint64_t>(1)),
          count_(0)
    { }

    /**
     * Seed a PCG RNG instance based on a seed (`init_state`) and a sequence/
     * stream id. Both of these can be any value, but the highest bit of
     * `sequence` will be discarded.
     *
     * @param init_state the seed value to initialize PCG state with.
     * @param sequence the sequence number/stream id.
     */
    PcgRNG::PcgRNG(uint64_t init_state, uint64_t sequence)
        : PcgRNG()
    {
        inc_ = sequence << 1u | 1u; // any odd value is ok
        get_uint32();
        state_ += init_state;
        get_uint32();
        count_ = 0;
    }

    PcgRNG::PcgRNG(uint64_t init_state)
        : PcgRNG(init_state, static_cast<uint64_t>(2305843009213693951ULL))
                             // Largest Mersenne prime under 64-bits.
    { }

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
}
