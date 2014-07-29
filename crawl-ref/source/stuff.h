/**
 * @file
 * @brief Misc stuff.
**/

#ifndef STUFF_H
#define STUFF_H

#include <map>
#include "player.h"

// time

string make_time_string(time_t abs_time, bool terse = false);
string make_file_time(time_t when);

// redraw

void set_redraw_status(uint64_t flags);

void redraw_screen();

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
                   int last_step, int ceiling_value);

#endif
