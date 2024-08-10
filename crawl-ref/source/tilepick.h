/**
 * @file
 * @brief Look-up functions for dungeon and item tiles.
**/

#pragma once

#include "ability-type.h"
#include "command-type.h"
#include "game-type.h"
#include "trap-type.h"
#include "rltiles/tiledef_defines.h"
#include "job-type.h"
#include "species-type.h"

#define TILE_NUM_KEY "tile_num"

struct bolt;
struct cloud_info;
struct coord_def;
struct item_def;
class monster;
struct monster_info;
struct shop_struct;
struct show_type;

bool is_door_tile(tileidx_t tile);

TextureID get_tile_texture(tileidx_t idx);

// Tile index lookup from Crawl data.
tileidx_t tileidx_feature(const coord_def &gc);
tileidx_t tileidx_trap(trap_type type);
tileidx_t tileidx_shop(const shop_struct *shop);
tileidx_t tileidx_feature_base(dungeon_feature_type feat);
tileidx_t tileidx_out_of_bounds(int branch);
void tileidx_out_of_los(tileidx_t *fg, tileidx_t *bg, tileidx_t *cloud, const coord_def& gc);

tileidx_t tileidx_monster(const monster_info& mon);
tileidx_t tileidx_draco_base(const monster_info& mon);
tileidx_t tileidx_draco_job(const monster_info& mon);
tileidx_t tileidx_player_mons();
tileidx_t tileidx_player_shadow();
tileidx_t tileidx_tentacle(const monster_info& mon);

tileidx_t tileidx_item(const item_def &item);
tileidx_t tileidx_item_throw(const item_def &item, int dx, int dy);
tileidx_t tileidx_known_base_item(tileidx_t label);

tileidx_t tileidx_cloud(const cloud_info &cl);
tileidx_t tileidx_bolt(const bolt &bolt);
tileidx_t vary_bolt_tile(tileidx_t tile, int dist);
tileidx_t tileidx_zap(int colour);
tileidx_t tileidx_spell(const spell_type spell);
tileidx_t tileidx_skill(const skill_type skill, int train);
tileidx_t tileidx_command(const command_type cmd);
tileidx_t tileidx_gametype(const game_type gtype);
tileidx_t tileidx_ability(const ability_type ability);
tileidx_t tileidx_branch(const branch_type br);
tileidx_t tileidx_player_job(const job_type job, bool recommended);
tileidx_t tileidx_player_species(const species_type species, bool recommended);

tileidx_t tileidx_known_brand(const item_def &item);

tileidx_t tileidx_unseen_flag(const coord_def &gc);

set<tileidx_t> status_icons_for(const monster_info &mon);
set<tileidx_t> status_icons_for_player();

// Return the level of enchantment as an int. None is 0, Randart is 4.
int enchant_to_int(const item_def &item);
// If tile has variations, select among them based upon the enchant of item.
tileidx_t tileidx_enchant_equ(const item_def &item, tileidx_t tile,
                              bool player = false);

void bind_item_tile(item_def &item);

// For a given fg/bg set of tile indices and a 1 character prefix,
// return index, flag, and tile name as a printable string.
string tile_debug_string(tileidx_t fg, tileidx_t bg, char prefix);

void tile_init_props(monster* mon);
tileidx_t tileidx_monster_base(int type, int mon_id, bool in_water = false,
                               int colour = 0, int number = 4, int tile_num_prop = 0,
                               bool vary = true);
tileidx_t tileidx_mon_clamp(tileidx_t tile, int offset);
