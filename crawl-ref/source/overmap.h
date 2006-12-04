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

void seen_notable_thing( int which_thing, int x, int y );
void display_overmap();
void unnotice_labyrinth_portal();
void unnotice_altar();
std::string overview_description_string();
void get_matching_features(
    const base_pattern &pattern, std::vector<stash_search_result> &results);

#endif
