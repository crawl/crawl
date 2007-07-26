/*
 *  File:       overmap.cc
 *  Summary:    "Overmap" functionality
 *  Written by: Linley Henzell
 *
 *  Change History (most recent first):
 *
 *               <1>    --/--/--        LRH             Created
 */


#ifndef OVERMAP_H
#define OVERMAP_H

#include "stash.h"
#include <vector>

void seen_notable_thing( dungeon_feature_type which_thing, int x, int y );
bool overmap_knows_portal(dungeon_feature_type portal);
void display_overmap();
bool unnotice_feature(const level_pos &pos);
std::string overview_description_string();

#endif
