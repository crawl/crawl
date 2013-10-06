#include "AppHdr.h"

#include <math.h>
#include "asg.h"
#include "random.h"
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

void seed_rng(uint32_t seed)
{
    uint32_t sarg[1] = { seed };
    seed_asg(sarg, 1);
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

    seed_asg(seed_key, 5);
}

uint32_t random_int()
{
    return get_uint32();
}

// [low, high]
int random_range(int low, int high)
{
    ASSERT(low <= high);
    return (low + random2(high - low + 1));
}

// [low, high]
int random_range(int low, int high, int nrolls)
{
    ASSERT(nrolls > 0);
    const int roll = random2avg(high - low + 1, nrolls);
    return low + roll;
}

// Chooses one of the strings passed in at random. The list of strings
// must be terminated with NULL.  NULL is not -1, and 0 is popular
// value for enums, so we need to copy the function.
template <>
const char* random_choose<const char*>(const char* first, ...)
{
    va_list args;
    va_start(args, first);

    const char* chosen = first;
    int count = 1, nargs = 100;

    while (nargs-- > 0)
    {
        char* pick = va_arg(args, char*);
        if (pick == NULL)
            break;
        if (one_chance_in(++count))
            chosen = pick;
    }

    ASSERT(nargs > 0);

    va_end(args);
    return chosen;
}

#ifndef UINT32_MAX
#define UINT32_MAX ((uint32_t)(-1))
#endif

// [0, max)
int random2(int max)
{
    if (max <= 1)
        return 0;

    uint32_t partn = UINT32_MAX / max;

    while (true)
    {
        uint32_t bits = get_uint32();
        uint32_t val  = bits / partn;

        if (val < (uint32_t)max)
            return ((int)val);
    }
}

// [0, max), separate RNG state
int ui_random(int max)
{
    if (max <= 1)
        return 0;

    uint32_t partn = UINT32_MAX / max;

    while (true)
    {
        uint32_t bits = get_uint32();
        uint32_t val  = bits / partn;

        if (val < (uint32_t)max)
            return ((int)val);
    }
}

// [0, 1]
bool coinflip(void)
{
    return (static_cast<bool>(random2(2)));
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
        return (x / 2);
}

// [0, ceil(nom/denom)]
int maybe_random_div(int nom, int denom, bool random_factor)
{
    if (nom <= 0)
        return 0;
    if (random_factor)
        return (random2(nom + denom) / denom);
    else
        return (nom / 2 / denom);
}

// [num, num*size]
int maybe_roll_dice(int num, int size, bool random)
{
    if (random)
        return roll_dice(num, size);
    else
        return ((num + num * size) / 2);
}

// [num, num*size]
int roll_dice(int num, int size)
{
    int ret = 0;

    // If num <= 0 or size <= 0, then we'll just return the default
    // value of zero.  This is good behaviour in that it will be
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
    {
        // Divide the damage among the dice, and add one
        // occasionally to make up for the fractions. -- bwr
        ret.size  = max_damage / num_dice;
        ret.size += x_chance_in_y(max_damage % num_dice, num_dice);
    }

    return ret;
}

// Attempts to make missile weapons nicer to the player by reducing the
// extreme variance in damage done.
void scale_dice(dice_def &dice, int threshold)
{
    while (dice.size > threshold)
    {
        dice.num *= 2;
        // If it's an odd number, lose one; this is more than
        // compensated by the increase in number of dice.
        dice.size /= 2;
    }
}

// Calculates num/den and randomly adds one based on the remainder.
// [floor(num/den), ceil(num/den)]
int div_rand_round(int num, int den)
{
    int rem = num % den;
    if (rem)
        return (num / den + (random2(den) < rem));
    else
        return (num / den);
}

// [0, max)
int bestroll(int max, int rolls)
{
    int best = 0;

    for (int i = 0; i < rolls; i++)
    {
        int curr = random2(max);
        if (curr > best)
            best = curr;
    }

    return best;
}

// random2avg() returns same mean value as random2() but with a lower variance
// never use with rolls < 2 as that would be silly - use random2() instead {dlb}
// [0, max)
int random2avg(int max, int rolls)
{
    int sum = random2(max);

    for (int i = 0; i < (rolls - 1); i++)
        sum += random2(max + 1);

    return (sum / rolls);
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
    int i;
    int sum = 0;

    if (max < 1)
        return 0;

    for (i = 0; i < max; i++)
        if (random2(limit) >= i)
            sum++;

    return sum;
}

// Generate samples from a binomial distribution with n_trials and trial_prob
// probability of success per trial. trial_prob is an integer less than 100
// representing the % chance of success.
// This just evaluates all n trials, there is probably an efficient way of
// doing this but I'm not much of a statistician. -CAO
// [0, n_trials]
int binomial_generator(unsigned n_trials, unsigned trial_prob)
{
    int count = 0;
    for (unsigned i = 0; i < n_trials; ++i)
        if (::x_chance_in_y(trial_prob, 100))
            count++;

    return count;
}

// range [0, 1.0)
double random_real()
{
    return get_uint32() / 4294967296.0;
}

// range [0, 1.0]
double random_real_inc()
{
    return get_uint32() / 4294967295.0;
}

// range [0, 1.0], weighted to middle with multiple rolls
double random_real_avg(int rolls)
{
    ASSERT(rolls > 0);
    double sum = 0;

    for (int i = 0; i < rolls; i++)
        sum += random_real_inc();

    return (sum / (double)rolls);
}

// range [low, high], weighted to middle with multiple rolls
double random_range_real(double low, double high, int nrolls)
{
    const int roll = random_real_avg(nrolls) * (high - low);
    return low + roll;
}

// Roll n_trials, return true if at least one succeeded.  n_trials might be
// not integer.
// [0, 1]
bool bernoulli(double n_trials, double trial_prob)
{
    if (n_trials <= 0 || trial_prob <= 0)
        return false;
    return random_real() >= pow(1 - trial_prob, n_trials);
}

bool one_chance_in(int a_million)
{
    return (random2(a_million) == 0);
}

bool x_chance_in_y(int x, int y)
{
    if (x <= 0)
        return false;

    if (x >= y)
        return true;

    return (random2(y) < x);
}

// [val - lowfuzz, val + highfuzz]
int fuzz_value(int val, int lowfuzz, int highfuzz, int naverage)
{
    const int lfuzz = lowfuzz * val / 100,
        hfuzz = highfuzz * val / 100;
    return val + random2avg(lfuzz + hfuzz + 1, naverage) - lfuzz;
}

// This is used when the front-end randomness is inconclusive.  There are
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
            bits.push_back(get_uint32());

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
        bits.push_back(get_uint32());

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
    return (low + random2(high - low + 1));
}

int defer_rand::random2avg(int max, int rolls)
{
    int sum = (*this)[0].random2(max);

    for (int i = 0; i < (rolls - 1); i++)
        sum += (*this)[i+1].random2(max + 1);

    return (sum / rolls);
}
