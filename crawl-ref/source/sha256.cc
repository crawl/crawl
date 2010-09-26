/*
   SHA256 hardening of PRNG written by Mikko Juola. This hashes MT-generated values (in 256 bit
   blocks) with SHA256 and uses the results as random values (in 32 bit blocks).

   sha256_genrand() generates cryptographically secure random numbers.

*/

#include "AppHdr.h"

typedef uint32_t u32;

#include "mt19937ar.h"

#ifdef MORE_HARDENED_PRNG

#include <stack>

#include <cstring>
#include <cstdio>
#include <cstdlib>

typedef struct
{
    char output[32];
} sha256state;

const u32 h[] = { 0x6a09e667,
                  0xbb67ae85,
                  0x3c6ef372,
                  0xa54ff53a,
                  0x510e527f,
                  0x9b05688c,
                  0x1f83d9ab,
                  0x5be0cd19 };

const u32 k[] = {
   0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
   0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
   0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
   0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
   0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
   0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
   0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
   0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

#define LSHIFT(value, bits) ( ((value) << (bits)) & 0xfffffffe )
#define RSHIFT(value, bits) ( ((value) >> (bits)) & 0x7fffffff )

#define LROTATE(value, bits) ( LSHIFT(value, bits) | RSHIFT(value, (sizeof(value) << 3) - (bits)) )
#define RROTATE(value, bits) ( RSHIFT(value, bits) | LSHIFT(value, (sizeof(value) << 3) - (bits)) )

#define STORE64H(x, y)                                                                     \
   { (y)[0] = (unsigned char)(((x)>>56)&255); (y)[1] = (unsigned char)(((x)>>48)&255);     \
     (y)[2] = (unsigned char)(((x)>>40)&255); (y)[3] = (unsigned char)(((x)>>32)&255);     \
     (y)[4] = (unsigned char)(((x)>>24)&255); (y)[5] = (unsigned char)(((x)>>16)&255);     \
     (y)[6] = (unsigned char)(((x)>>8)&255); (y)[7] = (unsigned char)((x)&255); }

#define STORE32H(x, y)                                                                     \
     { (y)[0] = (unsigned char)(((x)>>24)&255); (y)[1] = (unsigned char)(((x)>>16)&255);   \
       (y)[2] = (unsigned char)(((x)>>8)&255); (y)[3] = (unsigned char)((x)&255); }

#define LOAD32H(x, y)                            \
     { x = ((unsigned long)((y)[0] & 255)<<24) | \
           ((unsigned long)((y)[1] & 255)<<16) | \
           ((unsigned long)((y)[2] & 255)<<8)  | \
           ((unsigned long)((y)[3] & 255)); }

void sha256chunk(const char* chunk, sha256state* ss);

// Only first 64 bytes of in_msg are used, if in_msg_len is greater than that.
// No padding is done. Result is undefined if in_msg and out_msg overlap.
void sha256(const char* in_msg, size_t in_msg_len, char* out_msg)
{
    sha256state* ss = (sha256state*) out_msg;
    for (int i1 = 0; i1 < 8; i1++)
        STORE32H(h[i1], &ss->output[i1 << 2]);

    if (in_msg_len < 64)
    {
        char chunk[64];
        memset(chunk, 0, 64);
        memcpy(chunk, in_msg, in_msg_len);
        sha256chunk(chunk, ss);
        return;
    }

    sha256chunk(in_msg, ss);
}

void sha256chunk(const char* chunk, sha256state* ss)
{
    u32 chunk_out[8];
    u32 w[64];
    u32 s0, s1, maj, t1, t2, ch;
    size_t i1;

    for (i1 = 0; i1 < 16; i1 ++)
        LOAD32H(w[i1], &chunk[i1 << 2]);

    for (i1 = 16; i1 < 64; i1++)
    {
        s0 = RROTATE(w[i1-15], 7) ^ RROTATE(w[i1-15], 18) ^ RSHIFT(w[i1-15], 3);
        s1 = RROTATE(w[i1-2], 17) ^ RROTATE(w[i1-2],  19) ^ RSHIFT(w[i1-2], 10);
        w[i1] = w[i1-16] + s0 + w[i1-7] + s1;
    }

    for (i1 = 0; i1 < 8; i1++)
        LOAD32H(chunk_out[i1], &ss->output[i1 << 2]);

    for (i1 = 0; i1 < 64; i1++)
    {
        s0 = RROTATE(chunk_out[0], 2) ^ RROTATE(chunk_out[0], 13) ^ RROTATE(chunk_out[0], 22);
        maj = (chunk_out[0] & chunk_out[1]) ^ (chunk_out[0] & chunk_out[2]) ^ (chunk_out[1] & chunk_out[2]);
        t2 = s0 + maj;
        s1 = RROTATE(chunk_out[4], 6) ^ RROTATE(chunk_out[4], 11) ^ RROTATE(chunk_out[4], 25);
        ch = (chunk_out[4] & chunk_out[5]) ^ ((~chunk_out[4]) & chunk_out[6]);
        t1 = chunk_out[7] + s1 + ch + k[i1] + w[i1];

        chunk_out[7] = chunk_out[6];
        chunk_out[6] = chunk_out[5];
        chunk_out[5] = chunk_out[4];
        chunk_out[4] = chunk_out[3] + t1;
        chunk_out[3] = chunk_out[2];
        chunk_out[2] = chunk_out[1];
        chunk_out[1] = chunk_out[0];
        chunk_out[0] = t1 + t2;
    }

    for (i1 = 0; i1 < 8; i1++)
    {
        u32 temp;
        LOAD32H(temp, &ss->output[i1 << 2]);
        temp += chunk_out[i1];
        STORE32H(temp, &ss->output[i1 << 2]);
    }
}

struct sha256mt_state
{
    // 256 bits
    u32 mt_sha256_block[8], mt_block[8];
    u32 mt_block_index;

    sha256mt_state()
    {
        mt_block_index = 0;
    }
};

sha256mt_state effective_state;

std::stack<sha256mt_state> sha256mt_state_stack;

void reset_sha256_state()
{
    effective_state.mt_block_index = 0;
}

void push_sha256_state()
{
    sha256mt_state_stack.push(effective_state);
    push_mt_state();
}

void pop_sha256_state()
{
    if (sha256mt_state_stack.empty())
        return;

    effective_state = sha256mt_state_stack.top();

    sha256mt_state_stack.pop();
    pop_mt_state();
}

unsigned long sha256_genrand()
{
    u32 &mt_block_index = effective_state.mt_block_index;
    u32 *mt_sha256_block = effective_state.mt_sha256_block;
    u32 *mt_block = effective_state.mt_block;

    // Needs some hashing
    if (!(mt_block_index % 8))
    {
        mt_block_index = 0;

        mt_block[0] = genrand_int32();
        mt_block[1] = genrand_int32();
        mt_block[2] = genrand_int32();
        mt_block[3] = genrand_int32();
        mt_block[4] = genrand_int32();
        mt_block[5] = genrand_int32();
        mt_block[6] = genrand_int32();
        mt_block[7] = genrand_int32();

        // This kind of casting from char to 32-bit values gives different
        // results on different endianess platforms but we are talking
        // about random numbers here so let's leave it simple.
        sha256((char*) mt_block, 32, (char*) mt_sha256_block);
    }

    return mt_sha256_block[mt_block_index++];
}
#else // MORE_HARDENED_PRNG
// Stub these to MT functions
void push_sha256_state()
{
    push_mt_state();
}

void pop_sha256_state()
{
    pop_mt_state();
}

void reset_sha256_state()
{
}

unsigned long sha256_genrand()
{
    return genrand_int32();
}
#endif

#ifdef NEVER
/*
Simple correctness test, should print this:
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855

(the hash of an empty string, on unix systems try writing sha256sum and ctrl-d
*/
#include <cstdio>
#include <cstdlib>
int main(int argc, char* argv[])
{
    char msg[1];
    msg[0] = 0x80;

    char sha256out[32];
    memset(sha256out, 0, 32);
    sha256(msg, 1, sha256out);

    for (int i1 = 0; i1 < 32; i1++)
        printf("%x", (unsigned char) sha256out[i1]);
    printf("\n");
    return 0;
}
#endif

#ifdef SPEEDTEST
/* Generates 100000000 MT-generated, SHA256 hashed 32-bit random numbers if
   there are no arguments.
   Generates 100000000 MT-generated 32-bit random numbers if argument is '1'
*/

/* That's hundred million */
#define NUMBERS 100000000

#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <time.h>
int main(int argc, char* argv[])
{
    bool sha256test = true;
    if (argc > 1 && argv[1][0] == '1')
        sha256test = false;

    init_genrand(time(0));

    if (sha256test)
    {
        for (unsigned int i1 = 0; i1 < NUMBERS; i1++)
            sha256_genrand();
        return 0;
    }

    for (unsigned int i1 = 0; i1 < NUMBERS; i1++)
        genrand_int32();

    return 0;
}
#endif

#ifdef DIEHARD
/* When run, just outputs binary 4-byte random values. Useful for diehard tests */
/* If MORE_HARDENED_PRNG is not defined, it will use MT directly instead (because
   sha256 is not even compiled without that */
int main(int argc, char* argv[])
{
    init_genrand(time(0));

    while (true)
    {
        u32 value = sha256_genrand();
        fwrite(&value, sizeof(u32), 1, stdout);
    }

    return 0;
}
#endif
