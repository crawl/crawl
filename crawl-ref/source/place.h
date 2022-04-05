/**
 * @file
 * @brief Place related functions.
**/

#pragma once

#include "enum.h"

//////////////////////////////////////////////////////////////////////
// Places and names
//

// Prepositional form of branch level name. For example, "in the
// Abyss" or "on level 3 of the Main Dungeon".
string prep_branch_level_name(level_id id = level_id::current());

// Get some kind of "absolute depth" given the depth in the branch.
// It's mostly the difficulty of a given level and the quality of items
// you can find.
int absdungeon_depth(branch_type branch, int depth);

bool single_level_branch(branch_type branch);

bool branch_allows_followers(branch_type branch);

vector<level_id> all_dungeon_ids();

bool is_level_on_stack(level_id id);
