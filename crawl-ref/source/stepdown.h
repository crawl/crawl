/**
 * @file
 * @brief Stepdown functions.
**/

#pragma once

// stepdowns

enum rounding_type
{
    ROUND_DOWN,
    ROUND_CLOSE,
    ROUND_RANDOM
};

double stepdown(double value, double step);
int stepdown(int value, int step, rounding_type = ROUND_CLOSE, int max = 0);
int stepdown_value(int base_value, int stepping, int first_step,
                   int /*last_step*/, int ceiling_value);
