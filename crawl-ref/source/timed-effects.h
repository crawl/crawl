/**
 * @file
 * @brief Gametime related functions.
**/

#pragma once

void update_level(int elapsedTime);
monster* update_monster(monster& mon, int turns);
void handle_time();

void timeout_tombs(int duration);

int count_malign_gateways();
void timeout_malign_gateways(int duration);

void timeout_terrain_changes(int duration, bool force = false);

void setup_environment_effects();

// Lava smokes, swamp water mists.
void run_environment_effects();
int speed_to_duration(int speed);

// How many auts does the clock roll back every time the player
// enters a new floor?
static const int ZOT_CLOCK_PER_FLOOR = 6000 * BASELINE_DELAY;
// After how many auts without visiting new floors does the player die instantly?
static const int MAX_ZOT_CLOCK = 27000 * BASELINE_DELAY;

bool bezotted();
bool bezotted_in(branch_type br);
bool zot_clock_active();
int bezotting_level();
int turns_until_zot();
void decr_zot_clock();
void incr_zot_clock();
void set_turns_until_zot(int turns_left);
