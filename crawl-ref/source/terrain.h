/**
 * @file
 * @brief Terrain related functions.
**/

#pragma once

#include <memory>

#include "command-type.h"
#include "enum.h"
#include "god-type.h"
#include "terrain-change-type.h"

class  actor;
struct coord_def;

typedef FixedArray<bool, GXM, GYM> map_mask_boolean;

// Precomputes slime wall neighbours for all squares on the map. Handy if
// you need to make a lot of slime wall checks (such as for travel).
class unwind_slime_wall_precomputer
{
public:
    unwind_slime_wall_precomputer(bool docompute = true);
    ~unwind_slime_wall_precomputer();
private:
    bool did_compute_mask;
};

actor* actor_at(const coord_def& c);

bool cell_is_solid(const coord_def &c);

bool feat_is_malign_gateway_suitable(dungeon_feature_type feat);
bool feat_is_wall(dungeon_feature_type feat);
bool feat_is_opaque(dungeon_feature_type feat);
bool feat_is_solid(dungeon_feature_type feat);
bool feat_has_solid_floor(dungeon_feature_type feat);
bool feat_has_dry_floor(dungeon_feature_type feat);
bool feat_is_door(dungeon_feature_type feat);
bool feat_is_closed_door(dungeon_feature_type feat);
bool feat_is_sealed(dungeon_feature_type feat);
bool feat_is_statuelike(dungeon_feature_type feat);
bool feat_is_permarock(dungeon_feature_type feat);
bool feat_can_wall_jump_against(dungeon_feature_type feat);
bool feat_is_diggable(dungeon_feature_type feat);

bool feat_is_stone_stair_down(dungeon_feature_type feat);
bool feat_is_stone_stair_up(dungeon_feature_type feat);
bool feat_is_stone_stair(dungeon_feature_type feat);
bool feat_is_staircase(dungeon_feature_type feat);
bool feat_is_escape_hatch(dungeon_feature_type feat);
bool feat_is_trap(dungeon_feature_type feat, bool undiscovered_too = false);
command_type feat_stair_direction(dungeon_feature_type feat);
bool feat_is_portal(dungeon_feature_type feat);
bool feat_is_tree(dungeon_feature_type feat);
bool feat_is_metal(dungeon_feature_type feat);

bool feat_is_stair(dungeon_feature_type feat);
bool feat_is_travelable_stair(dungeon_feature_type feat);
bool feat_is_gate(dungeon_feature_type feat);

string feat_preposition(dungeon_feature_type feat, bool active = false,
                        const actor* who = nullptr);
string stair_climb_verb(dungeon_feature_type feat);

bool feat_is_water(dungeon_feature_type feat);
bool feat_is_watery(dungeon_feature_type feat);
bool feat_is_lava(dungeon_feature_type feat);
god_type feat_altar_god(dungeon_feature_type feat);
dungeon_feature_type altar_for_god(god_type god);

bool feat_is_altar(dungeon_feature_type feat);
bool feat_is_player_altar(dungeon_feature_type grid);

bool feat_is_branch_entrance(dungeon_feature_type feat);
bool feat_is_branch_exit(dungeon_feature_type feat);
bool feat_is_portal_entrance(dungeon_feature_type feat);
bool feat_is_portal_exit(dungeon_feature_type feat);

bool feat_is_bidirectional_portal(dungeon_feature_type feat);
bool feat_is_fountain(dungeon_feature_type feat);
bool feat_is_reachable_past(dungeon_feature_type feat);

bool feat_is_critical(dungeon_feature_type feat);
bool feat_is_valid_border(dungeon_feature_type feat);
bool feat_is_mimicable(dungeon_feature_type feat, bool strict = true);
bool feat_is_shaftable(dungeon_feature_type feat);

int count_neighbours_with_func(const coord_def& c, bool (*checker)(dungeon_feature_type));

void find_connected_identical(const coord_def& d, set<coord_def>& out);
coord_def get_random_stair();

bool slime_wall_neighbour(const coord_def& c);
int count_adjacent_slime_walls(const coord_def &pos);
void slime_wall_damage(actor* act, int delay);

void get_door_description(int door_size, const char** adjective,
                          const char** noun);
void feat_splash_noise(dungeon_feature_type feat);
bool feat_destroys_items(dungeon_feature_type feat);
bool feat_eliminates_items(dungeon_feature_type feat);

// Terrain changed under 'pos', perform necessary effects.
void dungeon_terrain_changed(const coord_def &pos,
                             dungeon_feature_type feat = DNGN_UNSEEN,
                             bool preserve_features = false,
                             bool preserve_items = false,
                             bool temporary = false,
                             bool wizmode = false);

// Moves everything on the level at src to dst.
void dgn_move_entities_at(coord_def src,
                          coord_def dst,
                          bool move_player,
                          bool move_monster,
                          bool move_items);

bool swap_features(const coord_def &pos1, const coord_def &pos2,
                   bool swap_everything = false, bool announce = true);

bool slide_feature_over(const coord_def &src,
                        coord_def preferred_dest = coord_def(-1, -1),
                        bool announce = false);

void fall_into_a_pool(dungeon_feature_type terrain);

void                 init_feat_desc_cache();
dungeon_feature_type feat_by_desc(string desc);
const char* feat_type_name(dungeon_feature_type feat);

dungeon_feature_type dungeon_feature_by_name(const string &name);
vector<string> dungeon_feature_matches(const string &name);
const char *dungeon_feature_name(dungeon_feature_type rfeat);
void destroy_wall(const coord_def& p);
void set_terrain_changed(const coord_def c);
bool cell_is_clingable(const coord_def pos);
bool cell_can_cling_to(const coord_def& from, const coord_def to);
bool is_boring_terrain(dungeon_feature_type feat);

dungeon_feature_type orig_terrain(coord_def pos);
void temp_change_terrain(coord_def pos, dungeon_feature_type newfeat, int dur,
                         terrain_change_type type = TERRAIN_CHANGE_GENERIC,
                         const monster* mon = nullptr);
bool revert_terrain_change(coord_def pos, terrain_change_type ctype);
bool is_temp_terrain(coord_def pos);

bool plant_forbidden_at(const coord_def &p, bool connectivity_only = false);

