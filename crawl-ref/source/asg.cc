/* This class implements a 160-bit pseudo-random number generator.
 * This generator combines David Jone's JKISS generator with an alternating
 * step generator utilizing a 32-bit Galois m_lfsr as a gate.
 * It passes all tests in TestU01 BigCrush.
 * This software is derived from LibRNG, a public domain pseudo-random number
 * library. (http://www.github.com/bhickey/librng)
 */

#include "AppHdr.h"
#include "asg.h"

static AsgKISS* asg_rng = new AsgKISS();

uint32_t
AsgKISS::get_uint32()
{
    m_lcg = (314527869 * m_lcg + 1234567);
    m_lfsr = (m_lfsr >> 1) ^ (-(int32_t)(m_lfsr & 1U) & 0xD0000001U);

    if (m_lfsr & 1)
    {
        m_xorshift ^= m_xorshift << 5;
        m_xorshift ^= m_xorshift >> 7;
        m_xorshift ^= m_xorshift << 22;
    }
    else
    {
        uint64_t t = 4294584393ULL * m_mwcm + m_mwcc;
        m_mwcc = t >> 32;
        m_mwcm = t;
    }

    return (m_lcg + m_mwcm + m_xorshift);
}

AsgKISS::AsgKISS()
{
    m_lcg = 12345678;
    m_mwcm = 43219876;
    m_mwcc = 6543217;
    m_xorshift = 987654321;
    m_lfsr = 76543210;
}

AsgKISS::AsgKISS(uint32_t init_key[], int key_length)
{
    m_lcg = (key_length > 0 ? init_key[0] : 12345678);
    m_mwcm = (key_length > 1 ? init_key[1] : 43219876);
    m_mwcc = (key_length > 2 ? init_key[2] : 6543217);
    m_xorshift = (key_length > 3 ? init_key[3] : 987654321);
    m_lfsr = (key_length > 4 ? init_key[4] : 7654321);

    // m_lfsr must not be set to  zero.
    if (!m_lfsr)
    {
        m_lfsr = 7654321;
        while (!(m_lfsr = get_uint32()));
    }
}

uint32_t get_uint32()
{
    return asg_rng->get_uint32();
}

void seed_asg(uint32_t seed_array[], int seed_len)
{
    delete asg_rng;
    asg_rng = new AsgKISS(seed_array, seed_len);
}
