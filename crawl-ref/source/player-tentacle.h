/**
 * @file
 * @brief Player tentacle-related code.
**/

#pragma once

bool mons_tentacle_adjacent_of_player(const monster* child);
int move_child_tentacles_player(int want_delay);

bool destroy_tentacle_of_player();
bool destroy_tentacles_of_player();
int player_available_tentacles();
void player_create_tentacles(int tentacle_num);