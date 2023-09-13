/* This file implements a 32-bit pseudo-random number generator with 64 bit
 * internal state: see http://www.pcg-random.org/.
 *
 * "PCG is a family of simple fast space-efficient statistically good
 * algorithms for random number generation. Unlike many general-purpose RNGs,
 * they are also hard to predict."
 *
 * TODO: should we eventually switch to just directly using the official c++
 * implementation?
 *
 * PcgRNG::PcgRNG() and get_uint32 are derived/modified from M.E. O'Neill's
 * minimal PCG implementation:
 *    https://github.com/imneme/pcg-c-basic/blob/master/pcg_basic.c
 * Original source is (c) 2014 Melissa O'Neill <oneill@pcg-random.org>
 * Licensed under Apache License 2.0
 *
 * get_bounded_uint32 is derived/modified from an implementation by Melissa
 * O'Neill of an algorithm by Daniel Lemire as part of a comparison of bounded
 * random functions:
 *    http://www.pcg-random.org/posts/bounded-rands.html
 *    https://github.com/imneme/bounded-rands/blob/master/bounded32.cpp
 * Original source is Copyright (c) 2018 Melissa E. O'Neill
 * Licensed under The MIT License
 */

#include "AppHdr.h"

#include "pcg.h"

namespace rng
{
    /**
     * Generate a uniformly distributed 32-bit random number.
     */
    uint32_t PcgRNG::get_uint32()
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

    /**
     * Generate a uniformly distributed number, r, where 0 <= r < range.
     * This uses a technique due to Daniel Lemire, with implementation and
     * additional tweaks from Melissa O'Neil. It's designed to avoid /,% for
     * small values of `range`.
     *
     * See:
     *  http://www.pcg-random.org/posts/bounded-rands.html
     *  https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
     *  https://arxiv.org/abs/1805.10941 or https://dl.acm.org/citation.cfm?id=3230636
     */
    uint32_t PcgRNG::get_bounded_uint32(uint32_t range)
    {
        uint32_t x = get_uint32();
        uint64_t m = uint64_t(x) * uint64_t(range);
        uint32_t l = uint32_t(m);
        if (l < range)
        {
            // TODO: will this generate warnings somewhere? the PCG c++
            // implementation has a different version of this step that may be
            // useful.
            uint32_t t = -range;

            if (t >= range)
            {
                t -= range;
                if (t >= range)
                    t %= range;
            }
            while (l < t)
            {
                x = get_uint32();
                m = uint64_t(x) * uint64_t(range);
                l = uint32_t(m);
            }
        }
        return m >> 32;
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
