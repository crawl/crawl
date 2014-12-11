#ifndef __LIB_RNG_ASG_HEADER__
#define __LIB_RNG_ASG_HEADER__

class AsgKISS
{
    public:
        AsgKISS();
        AsgKISS(uint32_t init_key[], int key_length);
        uint32_t get_uint32();
        uint32_t operator()() { return get_uint32(); }

        typedef uint32_t result_type;
        static constexpr uint32_t min() { return 0; }
        static constexpr uint32_t max() { return 0xffffffffU; }

        static AsgKISS &generator(int which = 0);
    private:
        uint32_t m_lcg, m_mwcm, m_mwcc, m_xorshift, m_lfsr;
};

uint32_t get_uint32(int generator = 0);
void seed_asg(uint32_t[], int);
#endif
