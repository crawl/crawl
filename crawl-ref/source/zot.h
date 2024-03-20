/**
 * @file
 * @brief Zot clock functions.
**/

#pragma once

// How many auts does the clock roll back every time the player
// enters a new floor?
const int ZOT_CLOCK_PER_FLOOR = 6000 * BASELINE_DELAY;
// After how many auts without visiting new floors does the player die instantly?
const int MAX_ZOT_CLOCK = 27000 * BASELINE_DELAY;

bool bezotted();
bool bezotted_in(branch_type br);
bool should_fear_zot();
bool zot_immune();
bool zot_clock_active();
int bezotting_level();
int bezotting_level_in(branch_type br);
int turns_until_zot();
int turns_until_zot_in(branch_type br);
void decr_zot_clock(bool extra_life = false);
void incr_zot_clock();
void set_turns_until_zot(int turns_left);

bool gem_clock_active();
void incr_gem_clock();
void print_gem_warnings(int gem, int old_time_taken);
void maybe_break_floor_gem();
void shatter_floor_gem(bool quiet = false);
int gem_time_left(int gem);
string gem_status();
