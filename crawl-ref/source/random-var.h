#pragma once

typedef int (*weight_func)(int val);

/*
 * It is assumed that start <= val < end are tight bounds, i.e.,
 * that start and end-1 have positive weights. This could be
 * changed if a reason turns up.
 */

class random_var
{
    int start, end;           // can take values with start <= val < end
    vector<int> weights;
    int total;                // sum of weights

public:
    explicit random_var(int c);
    random_var(int s, int e, weight_func w_ = nullptr);
    random_var(int s, int e, vector<int> ws);

    const random_var& operator=(const random_var& other);

    int weight(int val) const;
    int roll() const;        // evaluate the random variable

    double expected() const; // expected value
    int min() const;
    int max() const;

protected:
    void init_weights(weight_func w);
    void init();
    int roll2val(int r) const;
};

random_var operator+(const random_var& x, const random_var& y);
random_var negate(const random_var& x);
random_var operator-(const random_var& x, const random_var& y);
const random_var& operator+=(random_var& x, const random_var& y);
const random_var& operator-=(random_var& x, const random_var& y);
random_var operator/(const random_var& x, int d);
random_var operator*(const random_var& x, int d);
random_var div_rand_round(const random_var& x, int d);

namespace rv
{
    random_var max(const random_var& x, const random_var& y);
    random_var min(const random_var& x, const random_var& y);
    random_var roll_dice(int d, int n);
    random_var random2(int n);
}
