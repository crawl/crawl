/**
 * @file
 * @brief Gametime related functions.
**/

#pragma once

// After roughly how many turns without visiting new levels does the player
// die instantly?
static const int MAX_ZOT_CLOCK = 6000;
// Roughly how many turns before the end of the clock does the player become
// bezotted?
static const int BEZOTTING_THRESHOLD = 500;

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

bool zot_clock_active();
bool bezotted();
int bezotting_level();
void decr_zot_clock();
