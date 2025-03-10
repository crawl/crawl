#pragma once
// Used only in world_reacts()
void player_reacts();
void player_reacts_to_monsters();

void maybe_attune_regen_items(bool attune_regen, bool attune_mana_regen);

// Only function other than decrement_durations() which uses decrement_a_duration()
void extract_barbs(const char* endmsg);
int current_horror_level(); // XXX: move?
