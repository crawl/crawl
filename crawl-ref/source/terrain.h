/*
 *  File:       terrain.h
 *  Summary:    Terrain related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *               <1>     9/11/07        MPC             Split from misc.h
 */

#ifndef TERRAIN_H
#define TERRAIN_H

#include "enum.h"

struct coord_def;

// last updated 12may2000 {dlb}
/* ***********************************************************************
 * called from: acr
 * *********************************************************************** */
bool fall_into_a_pool( int entry_x, int entry_y, bool allow_shift, 
                       unsigned char terrain );


bool grid_is_wall(dungeon_feature_type grid);
bool grid_is_opaque(dungeon_feature_type grid);
bool grid_is_solid(dungeon_feature_type grid);
bool grid_is_solid(int x, int y);
bool grid_is_solid(const coord_def &c);
bool grid_is_rock(dungeon_feature_type grid);
bool grid_is_permarock(dungeon_feature_type grid);
bool grid_is_stone_stair(dungeon_feature_type grid);
bool grid_is_escape_hatch(dungeon_feature_type grid);
bool grid_is_trap(dungeon_feature_type grid);
command_type grid_stair_direction(dungeon_feature_type grid);
bool grid_sealable_portal(dungeon_feature_type grid);
bool grid_is_portal(dungeon_feature_type grid);

bool grid_is_water(dungeon_feature_type grid);
bool grid_is_watery(dungeon_feature_type grid);
god_type grid_altar_god( dungeon_feature_type grid );
dungeon_feature_type altar_for_god( god_type god );
bool grid_is_branch_stairs( dungeon_feature_type grid );
void find_connected_identical(coord_def d, dungeon_feature_type ft,
                               std::set<coord_def>& out);
void get_door_description(int door_size, const char** adjective, const char** noun);
dungeon_feature_type grid_secret_door_appearance( int gx, int gy );
bool grid_destroys_items( dungeon_feature_type grid );

const char *grid_item_destruction_message( dungeon_feature_type grid );

// Terrain changed under 'pos', perform necessary effects.
void dungeon_terrain_changed(const coord_def &pos,
                             dungeon_feature_type feat = DNGN_UNSEEN,
                             bool affect_player = true,
                             bool preserve_features = false,
                             bool preserve_items = false);

bool is_critical_feature(dungeon_feature_type feat);

void                 init_feat_desc_cache();
dungeon_feature_type feat_by_desc(std::string desc);
#endif
