/**
 * @file
 * @brief Movement, open-close door commands, movement effects.
**/

#pragma once

bool mantis_leap_point(set<coord_def>& set_, set<coord_def>& coward_set_);
void apply_barbs_damage(bool rampaging = false);
void remove_water_hold();
bool apply_cloud_trail(const coord_def old_pos);
bool cancel_barbed_move(bool rampaging = false);
bool cancel_confused_move(bool stationary);
void open_door_action(coord_def move = {0,0});
void close_door_action(coord_def move);
bool prompt_dangerous_portal(dungeon_feature_type ftype);
void move_player_action(coord_def move);
