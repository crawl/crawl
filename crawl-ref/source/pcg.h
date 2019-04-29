#pragma once
class CrawlVector;

namespace rng
{
    class PcgRNG
    {
    public:
        PcgRNG();
        PcgRNG(uint64_t init_state, uint64_t sequence);
        PcgRNG(uint64_t init_state);
        PcgRNG(const CrawlVector &v);
        CrawlVector to_vector();
        uint32_t get_uint32();
        uint32_t get_bounded_uint32(uint32_t bound);
        uint64_t get_uint64();
        uint32_t operator()() { return get_uint32(); }
        uint32_t operator()(uint32_t bound) { return get_bounded_uint32(bound); }

        typedef uint32_t result_type;
        static constexpr uint32_t min() { return 0; }
        static constexpr uint32_t max() { return 0xffffffffU; }

        uint64_t get_state() { return state_; };
        uint64_t get_inc() { return inc_; };
        uint64_t get_count() { return count_; };

    private:
        uint64_t state_;
        uint64_t inc_;
        uint64_t count_;
    };
}
