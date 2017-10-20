/**
 * @file
 * @brief Stepdown functions.
**/

#pragma once

#include "fixedp.h"

// stepdowns

enum rounding_type
{
    ROUND_DOWN,
    ROUND_CLOSE,
    ROUND_RANDOM
};

double stepdown(double value, double step);

// this is templated mainly so it can be easily used with fixedps of varying
// scales.
template <class Num=fixedp<>> Num stepdown(const Num &value, const Num &step,
                                            const Num &max=static_cast<Num>(0))
{
    const double ret = stepdown(static_cast<double>(value),
                                                    static_cast<double>(step));
    const Num cast_ret = static_cast<Num>(ret);
    if (max > static_cast<Num>(0) && cast_ret > max)
        return max;

    // leaves rounding behavior to Num
    return cast_ret;
}

int stepdown(int value, int step, rounding_type = ROUND_CLOSE, int max = 0);
int stepdown_value(int base_value, int stepping, int first_step,
                   int /*last_step*/, int ceiling_value);
