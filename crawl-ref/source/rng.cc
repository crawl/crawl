/**
 * @file
 * @brief Random number generator wrapping.
**/

#include "AppHdr.h"

#include "rng.h"

#include "endianness.h"
#include "asg.h"
#include "syscalls.h"

#ifdef UNIX
// for times()
#include <sys/times.h>
#endif

// for getpid()
#include <sys/types.h>
#ifndef TARGET_COMPILER_VC
# include <unistd.h>
#else
# include <process.h>
#endif

static void _seed_rng(uint32_t* seed_key, size_t num_keys)
{
    seed_asg(seed_key, num_keys);

    // Just in case something calls libc's rand(); currently nothing does.
    uint32_t oneseed = 0;
    for (size_t i = 0; i < num_keys; ++i)
        oneseed += seed_key[i];

    srand(oneseed);
}

void seed_rng(uint32_t seed)
{
    uint32_t sarg[1] = { seed };
    _seed_rng(sarg, 1);
}

void seed_rng()
{
    /* Use a 160-bit wide seed */
    uint32_t seed_key[5];
    read_urandom((char*)(&seed_key), sizeof(seed_key));

#ifdef UNIX
    struct tms buf;
    seed_key[0] += times(&buf);
#endif
    seed_key[1] += getpid();
    seed_key[2] += time(NULL);

    _seed_rng(seed_key, 5);
}

uint32_t random_int(void)
{
    return get_uint32();
}

void push_rng_state()
{
    push_asg_state();
}

void pop_rng_state()
{
    pop_asg_state();
}

//-----------------------------------------------------------------------------
// MurmurHash2, by Austin Appleby
uint32_t hash32(const void *data, int len)
{
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.
    const uint32_t m = 0x5bd1e995;

    // Initialize the hash to a 'random' value
    uint32_t h = len;

    const uint8_t *d = (const uint8_t*)data;
    // Mix 4 bytes at a time into the hash
    while (len >= 4)
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
    switch (len)
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

unsigned int hash_rand(int x, uint32_t seed, uint32_t id)
{
    if (x < 2)
        return 0;
    uint32_t data[2] = {seed, id};
    return hash32(data, 2 * sizeof(int32_t)) % x;
}
