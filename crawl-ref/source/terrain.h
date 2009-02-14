/*
 *  File:       terrain.h
 *  Summary:    Terrain related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef TERRAIN_H
#define TERRAIN_H

#include "enum.h"

class  actor;
struct coord_def;


actor* actor_at(const coord_def& c);

bool fall_into_a_pool( const coord_def& entry, bool allow_shift,
                       unsigned char terrain );


bool grid_is_wall(dungeon_feature_type grid);
bool grid_is_opaque(dungeon_feature_type grid);
bool grid_is_solid(dungeon_feature_type grid);
bool grid_is_solid(int x, int y);
bool grid_is_solid(const coord_def &c);
bool grid_is_rock(dungeon_feature_type grid);
bool grid_is_permarock(dungeon_feature_type grid);
bool grid_is_stone_stair(dungeon_feature_type grid);
bool grid_is_staircase(dungeon_feature_type grid);
bool grid_is_escape_hatch(dungeon_feature_type grid);
bool grid_is_trap(dungeon_feature_type grid, bool undiscovered_too = false);
command_type grid_stair_direction(dungeon_feature_type grid);
bool grid_sealable_portal(dungeon_feature_type grid);
bool grid_is_portal(dungeon_feature_type grid);

std::string grid_preposition(dungeon_feature_type grid, bool active = false,
                             const actor* who = NULL);
std::string stair_climb_verb(dungeon_feature_type grid);

bool grid_is_water(dungeon_feature_type grid);
bool grid_is_watery(dungeon_feature_type grid);
god_type grid_altar_god(dungeon_feature_type grid);
dungeon_feature_type altar_for_god(god_type god);
bool grid_is_branch_stairs(dungeon_feature_type grid);
void find_connected_identical(coord_def d, dungeon_feature_type ft,
                              std::set<coord_def>& out);
void find_connected_range(coord_def d, dungeon_feature_type ft_min,
                          dungeon_feature_type ft_max,
                          std::set<coord_def>& out);
void get_door_description(int door_size, const char** adjective, const char** noun);
dungeon_feature_type grid_secret_door_appearance(const coord_def &where);
dungeon_feature_type grid_appearance(const coord_def &gc);
dungeon_feature_type grid_appearance(feature_grid &gr, const coord_def &gc);
unsigned int show_appearance(const coord_def &ep);
bool grid_destroys_items(dungeon_feature_type grid);

const char *grid_item_destruction_message( dungeon_feature_type grid );

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
#endif
