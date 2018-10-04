/**
 * @file
 * @brief Movement, open-close door commands, movement effects.
**/

#pragma once

bool cancel_barbed_move();
void apply_barbs_damage();
void remove_ice_armour_movement();
bool cancel_confused_move(bool stationary);
void open_door_action(coord_def move = {0,0});
void close_door_action(coord_def move);
bool prompt_dangerous_portal(dungeon_feature_type ftype);
void move_player_action(coord_def move);
