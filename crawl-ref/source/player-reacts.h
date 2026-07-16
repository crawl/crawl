#pragma once

void check_trapped();

// Used only in world_reacts()
void player_reacts();
void player_reacts_to_monsters();

// Also used in places where we skip world_reacts for instant turns.
void player_takes_instant_action();
void player_reacts_to_instant_action();

// Only function other than decrement_durations() which uses decrement_a_duration()
void extract_barbs(const char* endmsg);
int current_horror_level(); // XXX: move?
