/**
 * @file
 * @brief Place related functions.
**/

#ifndef PLACE_H
#define PLACE_H

#include "enum.h"

//////////////////////////////////////////////////////////////////////
// Places and names
//
unsigned short get_packed_place();

unsigned short get_packed_place(branch_type branch, int subdepth);

branch_type place_branch(unsigned short place);
int place_depth(unsigned short place);

string short_place_name(unsigned short place);
string short_place_name(level_id id);
string place_name(unsigned short place, bool long_name = false,
                  bool include_number = true);

// Prepositional form of branch level name.  For example, "in the
// Abyss" or "on level 3 of the Main Dungeon".
string prep_branch_level_name(unsigned short packed_place);
string prep_branch_level_name();

// Get some kind of "absolute depth" given the depth in the branch.
// It's mostly the difficulty of a given level and the quality of items
// you can find.
int absdungeon_depth(branch_type branch, int depth);

bool single_level_branch(branch_type branch);

bool branch_allows_followers(branch_type branch);

vector<level_id> all_dungeon_ids();

bool is_level_on_stack(level_id id);

#endif
