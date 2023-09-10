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

void maybe_show_binding_sigil_duration_warning();
void timeout_binding_sigils();

void timeout_terrain_changes(int duration, bool force = false);

void setup_environment_effects();

// Lava smokes, swamp water mists.
void run_environment_effects();
int speed_to_duration(int speed);
