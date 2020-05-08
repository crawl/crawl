#include "AppHdr.h"

#include "random.h"

#include <cinttypes>
#include <cmath>
#if defined(UNIX) || defined(TARGET_COMPILER_MINGW)
# include <unistd.h>
#else
# include <process.h>
#endif

#include "pcg.h"
#include "syscalls.h"
#include "branch-type.h"
#include "state.h"
#include "store.h"
#include "options.h"

namespace rng
{
    // global/persistent rng state that will be saved
    static FixedVector<PcgRNG, rng::NUM_RNGS> _global_state;

    // temporary rng state
    static rng_type _generator = rng::GAMEPLAY;
    // TODO: once we have c++17, convert to type optional<PcgRNG>
    static PcgRNG * _sub_generator = nullptr;

    CrawlVector generators_to_vector()
    {
        CrawlVector store;
        for (PcgRNG& rng : _global_state)
            store.push_back(rng.to_vector()); // TODO is this ok memory-wise?
        return store;
    }

    vector<uint64_t> get_states()
    {
        // this doesn't return the internal state per se, but it returns the count
        // of 32 bit integers that have been drawn, which amounts to the same thing.
        // This isn't saved, though, so it's mainly useful for debugging within a
        // session.
        vector<uint64_t> result;
        for (PcgRNG& rng : _global_state)
            result.push_back(rng.get_count());
        return result;
    }

    void load_generators(const CrawlVector &v)
    {
        // as-is, decreasing the number of rngs (e.g. by removing a branch) will
        // break save compatibility.
        ASSERT(v.size() <= _global_state.size());
        for (int i = 0; i < v.size(); i++)
        {
            CrawlVector state = v[i].get_vector();
            _global_state[i] = PcgRNG(state);
        }
    }

    generator::generator(rng_type g) : previous(_generator)
    {
        ASSERT(g != rng::SUB_GENERATOR);
        _generator = g;
    }

    rng_type get_branch_generator(const branch_type b)
    {
        return static_cast<rng_type>(rng::LEVELGEN + static_cast<int>(b));
    }

    generator::generator(branch_type b) : previous(_generator)
    {
        _generator = get_branch_generator(b);
    }

    generator::~generator()
    {
        _generator = previous;
    }

    subgenerator::subgenerator(uint64_t seed, uint64_t sequence)
        : current(seed, sequence),
          previous(_sub_generator),
          previous_main(_generator)
    {
        _generator = rng::SUB_GENERATOR;
        _sub_generator = &current;
    }

    subgenerator::~subgenerator()
    {
        _generator = previous_main;
        _sub_generator = previous;
    }

    subgenerator::subgenerator(uint64_t seed)
        : subgenerator(seed, get_uint64())
    { }

    // call the 1-arg constructor so as to ensure a sequence point between the
    // two get_uint64 calls.
    subgenerator::subgenerator()
        : subgenerator(get_uint64())
    { }

    PcgRNG *get_generator(rng_type r)
    {
        UNUSED(r);
        ASSERT(_generator != ASSERT_NO_RNG);
        if (_generator == SUB_GENERATOR)
            return _sub_generator;
        else
            return &_global_state[_generator];
    }

    PcgRNG &current_generator()
    {
        PcgRNG *ret = get_generator(_generator);
        ASSERT(ret);
        return *ret;
    }

    uint32_t get_uint32()
    {
        return current_generator().get_uint32();
    }

    uint32_t peek_uint32()
    {
        PcgRNG tmp = current_generator(); // make a copy
        return tmp.get_uint32();
    }

    uint64_t get_uint64()
    {
        return current_generator().get_uint64();
    }

    uint64_t peek_uint64()
    {
        PcgRNG tmp = current_generator(); // make a copy
        return tmp.get_uint64();
    }

    static void _do_seeding(PcgRNG &master)
    {
        // TODO: don't initialize gameplay/ui rng?
        // Use the just seeded RNG to initialize the rest.
        for (PcgRNG& rng : _global_state)
        {
            uint64_t init_state = master.get_uint64();
            uint64_t seq = master.get_uint64();
            rng = PcgRNG(init_state, seq);
        }
    }

    void seed(uint64_t seed)
    {
        // use the default stream
        PcgRNG master = PcgRNG(seed);
        _do_seeding(master);
    }

    void seed()
    {
        // seed both state and sequence from system randomness.
        uint64_t seed_key[2];
        bool seeded = read_urandom((char*)(&seed_key), sizeof(seed_key));
        ASSERT(seeded);
        PcgRNG master = PcgRNG(seed_key[0], seed_key[1]);
        _do_seeding(master);
    }

    /**
     * Reset RNG to Options seed, and if that seed is 0, generate a new one.
     */
    void reset()
    {
        crawl_state.seed = Options.seed;
        while (!crawl_state.seed) // 0 = random seed
        {
            rng::seed(); // reset entirely via read_urandom
            crawl_state.seed = get_uint64();
        }
        dprf("Setting game seed to %" PRIu64, crawl_state.seed);
        you.game_seed = crawl_state.seed;
        rng::seed(crawl_state.seed);
    }
}

// TODO: probably this could all be in the rng namespace

// [low, high]
int random_range(int low, int high)
{
    ASSERT(low <= high);
    return low + random2(high - low + 1);
}

// [low, high]
int random_range(int low, int high, int nrolls)
{
    ASSERT(nrolls > 0);
    const int roll = random2avg(high - low + 1, nrolls);
    return low + roll;
}

// [0, max)
int random2(int max)
{
    if (max <= 1)
        return 0;

    return rng::current_generator().get_bounded_uint32(max);
}

// [0, max), separate RNG state
int ui_random(int max)
{
    rng::generator ui(rng::UI);

    return random2(max);
}

// [0, 1]
bool coinflip()
{
    return static_cast<bool>(random2(2));
}

// Returns random2(x) if random_factor is true, otherwise the mean.
// [0, x)
int maybe_random2(int x, bool random_factor)
{
    if (x <= 1)
        return 0;
    if (random_factor)
        return random2(x);
    else
        return x / 2;
}

// [0, ceil(nom/denom)]
int maybe_random_div(int nom, int denom, bool random_factor)
{
    if (nom <= 0)
        return 0;
    if (random_factor)
        return random2(nom + denom) / denom;
    else
        return nom / 2 / denom;
}

// [num, num*size]
int maybe_roll_dice(int num, int size, bool random)
{
    if (random)
        return roll_dice(num, size);
    else
        return (num + num * size) / 2;
}

// [num, num*size]
int roll_dice(int num, int size)
{
    int ret = 0;

    // If num <= 0 or size <= 0, then we'll just return the default
    // value of zero. This is good behaviour in that it will be
    // appropriate for calculated values that might be passed in.
    if (num > 0 && size > 0)
    {
        ret += num;     // since random2() is zero based

        for (int i = 0; i < num; i++)
            ret += random2(size);
    }

    return ret;
}

int dice_def::roll() const
{
    return roll_dice(num, size);
}

dice_def calc_dice(int num_dice, int max_damage)
{
    dice_def ret(num_dice, 0);

    if (num_dice <= 1)
    {
        ret.num  = 1;
        ret.size = max_damage;
    }
    else if (max_damage <= num_dice)
    {
        ret.num  = max_damage;
        ret.size = 1;
    }
    else
        ret.size = div_rand_round(max_damage, num_dice);

    return ret;
}

// Calculates num/den and randomly adds one based on the remainder.
// [floor(num/den), ceil(num/den)]
int div_rand_round(int num, int den)
{
    int rem = num % den;
    if (rem)
        return num / den + (random2(den) < rem);
    else
        return num / den;
}

// Converts a double to an integer by randomly rounding.
// Currently does not handle negative inputs.
int rand_round(double x)
{
    ASSERT(x >= 0);
    return int(x) + decimal_chance(fmod(x, 1.0));
}

int div_round_up(int num, int den)
{
    return num / den + (num % den != 0);
}

// random2avg() returns same mean value as random2() but with a lower variance
// never use with rolls < 2 as that would be silly - use random2() instead {dlb}
// [0, max)
int random2avg(int max, int rolls)
{
    int sum = random2(max);

    for (int i = 0; i < (rolls - 1); i++)
        sum += random2(max + 1);

    return sum / rolls;
}

// biased_random2() takes values in the same range [0, max) as random2() but
// with mean value (max - 1)/(n + 1) biased towards the bottom end.
// This can be thought of as the smallest of n _distinct_ random integers
// chosen in [0, max + n - 1).
// Never use with n < 2.
int biased_random2(int max, int n)
{
    for (int i = 0; i < max; i++)
        if (x_chance_in_y(n, n + max - 1 - i))
            return i;
    return 0;
}

// originally designed to randomise evasion -
// values are slightly lowered near (max) and
// approach an upper limit somewhere near (limit/2)
// [0, max]
int random2limit(int max, int limit)
{
    int sum = 0;

    if (max < 1)
        return 0;

    for (int i = 0; i < max; i++)
        if (random2(limit) >= i)
            sum++;

    return sum;
}

/** Sample from a binomial distribution.
 *
 * This is the number of successes in a sequence of independent trials with
 * fixed probability.
 *
 * @param n_trials The number of trials.
 * @param trial_prob The numerator of the probability of success of each trial.
 *                   If greater than scale, the probability is 1.0.
 * @param scale The denominator of trial_prob, default 100.
 * @return the number of successes, range [0, n_trials]
 */
int binomial(unsigned n_trials, unsigned trial_prob, unsigned scale)
{
    int count = 0;
    for (unsigned i = 0; i < n_trials; ++i)
        if (::x_chance_in_y(trial_prob, scale))
            count++;

    return count;
}

// range [0, 1.0)
// This uses a technique described by Saito and Matsumoto at
// MCQMC'08. Given that the IEEE floating point numbers are
// uniformly distributed over [1,2), we generate a number in
// this range and then offset it onto the range [0,1). The
// choice of bits (masking v. shifting) is arbitrary and
// should be immaterial for high quality generators.
double random_real()
{
    static const uint64_t UPPER_BITS = 0x3FF0000000000000ULL;
    static const uint64_t LOWER_MASK = 0x000FFFFFFFFFFFFFULL;
    const uint64_t value = UPPER_BITS | (rng::get_uint64() & LOWER_MASK);
    double result;
    // Portable memory transmutation. The union trick almost always
    // works, but this is safer.
    memcpy(&result, &value, sizeof(value));
    return result - 1.0;
}

// Roll n_trials, return true if at least one succeeded. n_trials might be
// not integer.
// [0, 1]
bool bernoulli(double n_trials, double trial_prob)
{
    if (n_trials <= 0 || trial_prob <= 0)
        return false;
    return !decimal_chance(pow(1 - trial_prob, n_trials));
}

bool one_chance_in(int a_million)
{
    return random2(a_million) == 0;
}

bool x_chance_in_y(int x, int y)
{
    if (x <= 0)
        return false;

    if (x >= y)
        return true;

    return random2(y) < x;
}

// [val - lowfuzz, val + highfuzz]
int fuzz_value(int val, int lowfuzz, int highfuzz, int naverage)
{
    const int lfuzz = lowfuzz * val / 100,
        hfuzz = highfuzz * val / 100;
    return val + random2avg(lfuzz + hfuzz + 1, naverage) - lfuzz;
}

bool decimal_chance(double chance)
{
    return random_real() < chance;
}

// This is used when the front-end randomness is inconclusive. There are
// never more than two possibilities, which simplifies things.
bool defer_rand::x_chance_in_y_contd(int x, int y, int index)
{
    if (x <= 0)
        return false;

    if (x >= y)
        return true;

    do
    {
        if (index == int(bits.size()))
            bits.push_back(rng::get_uint32());

        uint64_t expn_rand_1 = uint64_t(bits[index++]) * y;
        uint64_t expn_rand_2 = expn_rand_1 + y;
        uint64_t expn_minimum_fail = uint64_t(x) << 32;

        if (expn_minimum_fail <= expn_rand_1)
            return false;

        if (expn_rand_2 <= expn_minimum_fail)
            return true;

        // y = expn_rand_2 - expn_rand_1;  no-op
        x = expn_minimum_fail - expn_rand_1;
    } while (1);
}

int defer_rand::random2(int maxp1)
{
    if (maxp1 <= 1)
        return 0;

    if (bits.empty())
        bits.push_back(rng::get_uint32());

    uint64_t expn_rand_1 = uint64_t(bits[0]) * maxp1;
    uint64_t expn_rand_2 = expn_rand_1 + maxp1;

    int val1 = int(expn_rand_1 >> 32);
    int val2 = int(expn_rand_2 >> 32);

    if (val2 == val1)
        return val1;

    // val2 == val1 + 1
    uint64_t expn_thresh = uint64_t(val2) << 32;

    return x_chance_in_y_contd(int(expn_thresh - expn_rand_1),
                               maxp1, 1)
         ? val1 : val2;
}

defer_rand& defer_rand::operator[](int i)
{
    return children[i];
}

int defer_rand::random_range(int low, int high)
{
    ASSERT(low <= high);
    return low + random2(high - low + 1);
}

int defer_rand::random2avg(int max, int rolls)
{
    int sum = (*this)[0].random2(max);

    for (int i = 0; i < (rolls - 1); i++)
        sum += (*this)[i+1].random2(max + 1);

    return sum / rolls;
}
