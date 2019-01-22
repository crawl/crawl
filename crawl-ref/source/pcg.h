#pragma once
class CrawlVector;

class PcgRNG
{
    public:
        PcgRNG();
        PcgRNG(uint64_t init_key[], int key_length);
        PcgRNG(const CrawlVector &v);
        CrawlVector to_vector();
        uint32_t get_uint32();
        uint64_t get_uint64();
        uint32_t operator()() { return get_uint32(); }

        typedef uint32_t result_type;
        static constexpr uint32_t min() { return 0; }
        static constexpr uint32_t max() { return 0xffffffffU; }

        uint64_t get_state() { return state_; };
        uint64_t get_inc() { return inc_; };

    private:
        uint64_t state_;
        uint64_t inc_;
};
