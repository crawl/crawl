#include "AppHdr.h"

#include "random-var.h"

#include "random.h"

// TODO: the design of this class, together with c++ operator eval order
// indeterminacy, is more or less a bad idea with the goal of deterministic
// seeding. It possibly should be removed.

random_var::random_var(int c)
    : start(c), end(c+1)
{
    weights.push_back(1);
    init();
}

random_var::random_var(int s, int e, weight_func w)
    : start(s), end(e)
{
    init_weights(w);
    init();
}

random_var::random_var(int s, int e, vector<int> ws)
    : start(s), end(e), weights(ws)
{
    ASSERT(weights.size() == static_cast<unsigned int>(end - start));
    init();
}

const random_var& random_var::operator=(const random_var& other)
{
    start = other.start;
    end = other.end;
    total = other.total;
    weights = other.weights;
    return *this;
}

int random_var::weight(int val) const
{
    if (val < start || val >= end)
        return 0;
    return weights[val - start];
}

void random_var::init_weights(weight_func w)
{
    ASSERT(weights.empty());
    for (int v = start; v < end; ++v)
        weights.push_back(w ? (*w)(v) : 1);
}

void random_var::init()
{
    int64_t sum = 0;
    for (int v = start; v < end; ++v)
        sum += weight(v);

    if (sum <= (int64_t) INT_MAX)
        total = (int) sum;
    else
    {
        // The total is too big: rescale all entries by 256.
        ASSERT(sum <= 256 * (int64_t) INT_MAX);

        total = 0;

        const int length = weights.size();
        int first_nonzero = -1;
        int last_nonzero = -1;
        for (int i = 0; i < length; ++i)
        {
            weights[i] /= 256;
            total += weights[i];

            if (first_nonzero < 0 && weights[i] > 0)
                first_nonzero = i;
            if (weights[i] > 0)
                last_nonzero = i;
        }
        ASSERT(first_nonzero >= 0 && last_nonzero >= 0);

        // Weights that rounded to zero should be dropped.
        weights.erase(weights.begin() + last_nonzero + 1, weights.end());
        weights.erase(weights.begin(), weights.begin() + first_nonzero);
        start += first_nonzero;
        end -= (length - last_nonzero - 1);
    }

    ASSERT(total > 0);
    ASSERT(weight(start) > 0);
    ASSERT(weight(end - 1) > 0);
}

int random_var::roll2val(int r) const
{
    ASSERT(0 <= r);
    ASSERT(r < total);
    int v = start;
    int w = weight(v);
    while (r >= w)
    {
        v++;
        w += weight(v);
    }
    return v;
}

int random_var::roll() const
{
    return roll2val(random2(total));
}

int random_var::max() const
{
    return end - 1;
}

int random_var::min() const
{
    return start;
}

double random_var::expected() const
{
    double ev = 0;
    for (int i = start; i < end; ++i)
        ev += i * weight(i) / (double)total;
    return ev;
}

//////////////////////////////////

random_var operator+(const random_var& x, const random_var& y)
{
    int start = x.min();
    start += y.min(); // force a sequence point
    int end = x.max();
    end += y.max() + 1; // force a sequence point
    vector<int> weights(end - start, 0);

    for (int vx = x.min(); vx <= x.max(); ++vx)
        for (int vy = y.min(); vy <= y.max(); ++vy)
            weights[vx + vy - start] += x.weight(vx) * y.weight(vy);

    return random_var(start, end, weights);
}

random_var negate(const random_var& x)
{
    const int start = -x.max();
    const int end = -x.min() + 1;
    vector<int> weights(end - start, 0);

    for (int v = x.min(); v <= x.max(); ++v)
        weights[-v - start] = x.weight(v);

    return random_var(start, end, weights);
}

random_var operator-(const random_var& x, const random_var& y)
{
    return x + ::negate(y);
}

const random_var& operator+=(random_var& x, const random_var& y)
{
    x = x + y;
    return x;
}

const random_var& operator-=(random_var& x, const random_var& y)
{
    x = x - y;
    return x;
}

random_var operator/(const random_var& x, int d)
{
    const int start = x.min() / d;
    const int end = x.max() / d + 1;
    vector<int> weights(end - start, 0);

    for (int v = x.min(); v <= x.max(); ++v)
        weights[v / d - start] += x.weight(v);

    return random_var(start, end, weights);
}

random_var operator*(const random_var& x, int d)
{
    const int start = x.min() * d;
    const int end = x.max() * d + 1;
    vector<int> weights(end - start, 0);

    for (int v = x.min(); v <= x.max(); ++v)
        weights[v * d - start] = x.weight(v);

    return random_var(start, end, weights);
}

random_var div_rand_round(const random_var& x, int d)
{
    // The rest is much simpler if we can assume d is positive.
    if (d < 0)
        return ::negate(div_rand_round(x, -d));
    ASSERT(d != 0);

    // Round start down and end up, not both towards zero.
    const int x_min1 = x.min(); // force sequence points...
    const int x_min2 = x.min();
    const int x_max1 = x.max();
    const int x_max2 = x.max();
    const int start = (x_min1 - (x_min2 < 0 ? d - 1 : 0)) / d;
    const int end   = (x_max1 + (x_max2 > 0 ? d - 1 : 0)) / d + 1;
    vector<int> weights(end - start, 0);

    for (int v = x.min(); v <= x.max(); ++v)
    {
        const int rem = abs(v % d);
        weights[v / d - start] += x.weight(v) * (d - rem);
        if (rem != 0) // guarantees sgn(v) != 0 too
            weights[v / d + sgn(v) - start] += x.weight(v) * rem;
    }

    return random_var(start, end, weights);
}

random_var rv::max(const random_var& x, const random_var& y)
{
    const int x_min = x.min(); // force sequence points...
    const int y_min = y.min();
    const int x_max = x.max();
    const int y_max = y.max();
    const int start = ::max(x_min, y_min);
    const int end = ::max(x_max, y_max) + 1;
    vector<int> weights(end - start, 0);

    for (int vx = x.min(); vx <= x.max(); ++vx)
        for (int vy = y.min(); vy <= y.max(); ++vy)
            weights[::max(vx, vy) - start] += x.weight(vx) * y.weight(vy);

    return random_var(start, end, weights);
}

random_var rv::min(const random_var& x, const random_var& y)
{
    const int x_min = x.min(); // force sequence points...
    const int y_min = y.min();
    const int x_max = x.max();
    const int y_max = y.max();
    const int start = ::min(x_min, y_min);
    const int end = ::min(x_max, y_max) + 1;
    vector<int> weights(end - start, 0);

    for (int vx = x.min(); vx <= x.max(); ++vx)
        for (int vy = y.min(); vy <= y.max(); ++vy)
            weights[::min(vx, vy) - start] += x.weight(vx) * y.weight(vy);

    return random_var(start, end, weights);
}

random_var rv::roll_dice(int d, int n)
{
    if (n <= 0)
        return random_var(0);
    random_var x(0);
    for (int i = 0; i < d; ++i)
        x += random_var(1, n+1);
    return x;
}

random_var rv::random2(int n)
{
    return random_var(0, std::max(n, 1));
}
