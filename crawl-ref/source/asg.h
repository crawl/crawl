#ifndef __LIB_RNG_ASG_HEADER__
#define __LIB_RNG_ASG_HEADER__

#include <stack>
class AsgKISS
{
    public:
        AsgKISS();
        AsgKISS(uint32_t init_key[], int key_length);
        uint32_t get_uint32();
    private:
        uint32_t m_lcg, m_mwcm, m_mwcc, m_xorshift, m_lfsr;
};

uint32_t get_uint32();
void seed_asg(uint32_t[], int);
#endif
