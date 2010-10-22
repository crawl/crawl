/*
 *  File:       dgn-overview.h
 *  Summary:    "Overview" functionality
 *  Written by: Linley Henzell
 */


#ifndef OVERMAP_H
#define OVERMAP_H

#include "stash.h"
#include <vector>

void overview_clear();

void seen_notable_thing(dungeon_feature_type which_thing, const coord_def& pos);
bool move_notable_thing(const coord_def& orig, const coord_def& dest);
bool overview_knows_portal(dungeon_feature_type portal);
int  overview_knows_num_portals(dungeon_feature_type portal);
void display_overview();
bool unnotice_feature(const level_pos &pos);
std::string overview_description_string(bool display);

///////////////////////////////////////////////////////////
void set_level_annotation(std::string str,
                          level_id li = level_id::current());
void clear_level_annotation(level_id li = level_id::current());

void set_level_exclusion_annotation(std::string str,
                                    level_id li = level_id::current());
void clear_level_exclusion_annotation(level_id li = level_id::current());

std::string get_level_annotation(level_id li = level_id::current(),
                                 bool skip_excl = false);

std::string get_coloured_level_annotation(int col,
                                 level_id li = level_id::current(),
                                 bool skip_excl = false);

bool level_annotation_has(std::string str,
                          level_id li = level_id::current());

void annotate_level();

#endif
