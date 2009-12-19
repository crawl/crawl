/*
 *  File:       terrain.h
 *  Summary:    Terrain related functions.
 *  Written by: Linley Henzell
 */

#ifndef TERRAIN_H
#define TERRAIN_H

#include "enum.h"

class  actor;
struct coord_def;


actor* actor_at(const coord_def& c);

bool fall_into_a_pool( const coord_def& entry, bool allow_shift,
                       unsigned char terrain );

bool cell_is_solid(int x, int y);
bool cell_is_solid(const coord_def &c);

bool feat_is_wall(dungeon_feature_type feat);
bool feat_is_opaque(dungeon_feature_type feat);
bool feat_is_solid(dungeon_feature_type feat);
bool feat_is_door(dungeon_feature_type feat);
bool feat_is_closed_door(dungeon_feature_type feat);
bool feat_is_secret_door(dungeon_feature_type feat);
bool feat_is_statue_or_idol(dungeon_feature_type feat);
bool feat_is_rock(dungeon_feature_type feat);
bool feat_is_permarock(dungeon_feature_type feat);
bool feat_is_stone_stair(dungeon_feature_type feat);
bool feat_is_staircase(dungeon_feature_type feat);
bool feat_is_escape_hatch(dungeon_feature_type feat);
bool feat_is_trap(dungeon_feature_type feat, bool undiscovered_too = false);
command_type feat_stair_direction(dungeon_feature_type feat);
bool feat_sealable_portal(dungeon_feature_type feat);
bool feat_is_portal(dungeon_feature_type feat);

bool feat_is_stair(dungeon_feature_type feat);
bool feat_is_travelable_stair(dungeon_feature_type feat);
bool feat_is_escape_hatch(dungeon_feature_type feat);
bool feat_is_gate(dungeon_feature_type feat);

std::string feat_preposition(dungeon_feature_type feat, bool active = false,
                             const actor* who = NULL);
std::string stair_climb_verb(dungeon_feature_type feat);

bool feat_is_water(dungeon_feature_type feat);
bool feat_is_watery(dungeon_feature_type feat);
god_type feat_altar_god(dungeon_feature_type feat);
dungeon_feature_type altar_for_god(god_type god);
bool feat_is_altar(dungeon_feature_type feat);
bool feat_is_player_altar(dungeon_feature_type grid);

bool feat_is_branch_stairs(dungeon_feature_type feat);
void find_connected_identical(coord_def d, dungeon_feature_type ft,
                              std::set<coord_def>& out);
void find_connected_range(coord_def d, dungeon_feature_type ft_min,
                          dungeon_feature_type ft_max,
                          std::set<coord_def>& out);
void get_door_description(int door_size, const char** adjective, const char** noun);
dungeon_feature_type grid_secret_door_appearance(const coord_def &where);
dungeon_feature_type grid_appearance(const coord_def &gc);
bool feat_destroys_items(dungeon_feature_type feat);

const char *feat_item_destruction_message( dungeon_feature_type feat );

// Terrain changed under 'pos', perform necessary effects.
void dungeon_terrain_changed(const coord_def &pos,
                             dungeon_feature_type feat = DNGN_UNSEEN,
                             bool affect_player = true,
                             bool preserve_features = false,
                             bool preserve_items = false);

bool swap_features(const coord_def &pos1, const coord_def &pos2,
                   bool swap_everything = false, bool announce = true);

bool slide_feature_over(const coord_def &src,
                        coord_def prefered_dest = coord_def(-1, -1),
                        bool announce = true);

bool is_critical_feature(dungeon_feature_type feat);

void                 init_feat_desc_cache();
dungeon_feature_type feat_by_desc(std::string desc);

dungeon_feature_type dungeon_feature_by_name(const std::string &name);
std::vector<std::string> dungeon_feature_matches(const std::string &name);
const char *dungeon_feature_name(dungeon_feature_type rfeat);

#endif
