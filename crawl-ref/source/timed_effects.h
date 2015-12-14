/**
 * @file
 * @brief Gametime related functions.
**/

#ifndef TIME_H
#define TIME_H

void change_labyrinth(bool msg = false);

void update_level(int elapsedTime);
void handle_time();
void recharge_rods(int aut, bool floor_only);

void timeout_tombs(int duration);

int count_malign_gateways();
void timeout_malign_gateways(int duration);

void timeout_terrain_changes(int duration, bool force = false);

void setup_environment_effects();

// Lava smokes, swamp water mists.
void run_environment_effects();
int speed_to_duration(int speed);

#endif
