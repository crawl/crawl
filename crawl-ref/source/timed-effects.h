/**
 * @file
 * @brief Gametime related functions.
**/

#pragma once

#include "terrain-change-type.h"

void update_level(int elapsedTime);
monster* update_monster(monster& mon, int turns);
void handle_time();

int count_malign_gateways(const actor& owner);

bool end_terrain_changes(terrain_change_type type, mid_t source_mid = MID_NOBODY);
bool end_terrain_changes(const actor& source, terrain_change_type type = NUM_TERRAIN_CHANGE_TYPES);

void end_enkindled_status();

void timeout_terrain_changes(int duration, bool force = false);

void update_mould_tracking(const coord_def& pos);
void setup_environment_effects();

// Lava smokes, swamp water mists.
void run_environment_effects();
int speed_to_duration(int speed);
