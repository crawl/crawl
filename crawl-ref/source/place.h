/*
 *  File:       place.h
 *  Summary:    Place related functions.
 *  Written by: Linley Henzell
 */

#ifndef PLACE_H
#define PLACE_H

#include "enum.h"

//////////////////////////////////////////////////////////////////////
// Places and names
//
unsigned short get_packed_place();

unsigned short get_packed_place(branch_type branch, int subdepth,
                                 level_area_type level_type);

int place_branch(unsigned short place);
int place_depth(unsigned short place);
int place_type(unsigned short place);

std::string short_place_name(unsigned short place);
std::string short_place_name(level_id id);
std::string place_name(unsigned short place, bool long_name = false,
                        bool include_number = true);

// Prepositional form of branch level name.  For example, "in the
// Abyss" or "on level 3 of the Main Dungeon".
std::string prep_branch_level_name(unsigned short packed_place);
std::string prep_branch_level_name();

// Get displayable depth in the current branch, given the absolute
// depth.
int subdungeon_depth(branch_type branch, int depth);

// Get absolute depth given the displayable depth in the branch.
int absdungeon_depth(branch_type branch, int subdepth);

// Get displayable depth in the current branch.
int player_branch_depth();

bool single_level_branch(branch_type branch);

bool level_type_exits_up(level_area_type type);
bool level_type_exits_down(level_area_type type);
bool level_type_allows_followers(level_area_type type);
bool level_type_is_stash_trackable(level_area_type type);

std::vector<level_id> all_dungeon_ids();

#endif
