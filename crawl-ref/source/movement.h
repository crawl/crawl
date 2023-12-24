/**
 * @file
 * @brief Movement, open-close door commands, movement effects.
**/

#pragma once

void player_did_deliberate_movement(bool rampaging = false);
void remove_ice_movement();
bool cancel_harmful_move(bool physically = true, bool rampaging = false);
void apply_noxious_bog(const coord_def old_pos);
bool apply_cloud_trail(const coord_def old_pos);
bool cancel_confused_move(bool stationary);
void open_door_action(coord_def move = {0,0});
void close_door_action(coord_def move);
bool prompt_dangerous_portal(dungeon_feature_type ftype);
bool prompt_descent_shortcut(dungeon_feature_type ftype);
monster *get_rampage_target(coord_def move);
void move_player_action(coord_def move);
