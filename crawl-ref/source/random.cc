#include "AppHdr.h"

#include <stdint.h>
#include "random.h"

int random_range(int low, int high)
{
    ASSERT(low <= high);
    return (low + random2(high - low + 1));
}

int random_range(int low, int high, int nrolls)
{
    ASSERT(nrolls > 0);
    const int roll = random2avg(high - low + 1, nrolls);
    return low + roll;
}

// Chooses one of the numbers passed in at random. The list of numbers
// must be terminated with -1.
int random_choose(int first, ...)
{
    va_list args;
    va_start(args, first);

    int chosen = first, count = 1, nargs = 100;

    while (nargs-- > 0)
    {
        const int pick = va_arg(args, int);
        if (pick == -1)
            break;
        if (one_chance_in(++count))
            chosen = pick;
    }

    ASSERT(nargs > 0);

    va_end(args);
    return (chosen);
}

// Chooses one of the strings passed in at random. The list of strings
// must be terminated with NULL.
const char* random_choose_string(const char* first, ...)
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
    return (chosen);
}

int random_choose_weighted(int weight, int first, ...)
{
    va_list args;
    va_start(args, first);

    int chosen = first, cweight = weight, nargs = 100;

    while (nargs-- > 0)
    {
        const int nweight = va_arg(args, int);
        if (!nweight)
            break;

        const int choice = va_arg(args, int);
        if (random2(cweight += nweight) < nweight)
            chosen = choice;
    }

    ASSERT(nargs > 0);

    va_end(args);
    return (chosen);
}

#ifndef UINT32_MAX
#define UINT32_MAX ((uint32_t)(-1))
#endif

int random2(int max)
{
    if (max <= 1)
        return (0);

    uint32_t partn = UINT32_MAX / max;

    while (true)
    {
        uint32_t bits = random_int();
        uint32_t val  = bits / partn;

        if (val < (uint32_t)max)
            return ((int)val);
    }
}

bool coinflip(void)
{
    return (static_cast<bool>(random2(2)));
}

// Returns random2(x) if random_factor is true, otherwise the mean.
int maybe_random2(int x, bool random_factor)
{
    if (x <= 1)
        return (0);
    if (random_factor)
        return (random2(x));
    else
        return (x / 2);
}

int maybe_roll_dice(int num, int size, bool random)
{
    if (random)
        return (roll_dice(num, size));
    else
        return ((num + num * size) / 2);
}

int roll_dice(int num, int size)
{
    int ret = 0;
    int i;

    // If num <= 0 or size <= 0, then we'll just return the default
    // value of zero.  This is good behaviour in that it will be
    // appropriate for calculated values that might be passed in.
    if (num > 0 && size > 0)
    {
        ret += num;     // since random2() is zero based

        for (i = 0; i < num; i++)
            ret += random2(size);
    }

    return (ret);
}

int dice_def::roll() const
{
    return roll_dice(this->num, this->size);
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

    return (ret);
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
int div_rand_round(int num, int den)
{
    int rem = num % den;
    if (rem)
        return (num / den + (random2(den) < rem));
    else
        return (num / den);
}

int bestroll(int max, int rolls)
{
    int best = 0;

    for (int i = 0; i < rolls; i++)
    {
        int curr = random2(max);
        if (curr > best)
            best = curr;
    }

    return (best);
}

// random2avg() returns same mean value as random2() but with a lower variance
// never use with rolls < 2 as that would be silly - use random2() instead {dlb}
int random2avg(int max, int rolls)
{
    int sum = random2(max);

    for (int i = 0; i < (rolls - 1); i++)
        sum += random2(max + 1);

    return (sum / rolls);
}

// originally designed to randomise evasion -
// values are slightly lowered near (max) and
// approach an upper limit somewhere near (limit/2)
int random2limit(int max, int limit)
{
    int i;
    int sum = 0;

    if (max < 1)
        return (0);

    for (i = 0; i < max; i++)
        if (random2(limit) >= i)
            sum++;

    return (sum);
}

// Generate samples from a binomial distribution with n_trials and trial_prob
// probability of success per trial. trial_prob is a integer less than 100
// representing the % chancee of success.
// This just evaluates all n trials, there is probably an efficient way of
// doing this but I'm not much of a statistician. -CAO
int binomial_generator(unsigned n_trials, unsigned trial_prob)
{
    int count = 0;
    for (unsigned i = 0; i < n_trials; ++i)
        if (::x_chance_in_y(trial_prob, 100))
            count++;

    return count;
}

bool one_chance_in(int a_million)
{
    return (random2(a_million) == 0);
}

bool x_chance_in_y(int x, int y)
{
    if (x <= 0)
        return (false);

    if (x >= y)
        return (true);

    return (random2(y) < x);
}

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
        return (false);

    if (x >= y)
        return (true);

    do
    {
        if (index == int(bits.size()))
            bits.push_back(random_int());

        uint64_t expn_rand_1 = uint64_t(bits[index++]) * y;
        uint64_t expn_rand_2 = expn_rand_1 + y;
        uint64_t expn_minimum_fail = uint64_t(x) << 32;

        if (expn_minimum_fail <= expn_rand_1)
            return (false);

        if (expn_rand_2 <= expn_minimum_fail)
            return (true);

        // y = expn_rand_2 - expn_rand_1;  no-op
        x = expn_minimum_fail - expn_rand_1;
    } while(1);
}

int defer_rand::random2(int maxp1)
{
    if (maxp1 <= 1)
        return (0);

    if (bits.empty())
        bits.push_back(random_int());

    uint64_t expn_rand_1 = uint64_t(bits[0]) * maxp1;
    uint64_t expn_rand_2 = expn_rand_1 + maxp1;

    int val1 = int(expn_rand_1 >> 32);
    int val2 = int(expn_rand_2 >> 32);

    if (val2 == val1)
        return (val1);

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
