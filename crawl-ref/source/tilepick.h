/**
 * @file
 * @brief Look-up functions for dungeon and item tiles.
**/

#ifndef TILEPICK_H
#define TILEPICK_H

#include "tiledef_defines.h"

struct bolt;
struct cloud_info;
struct coord_def;
struct item_def;
class monster;
struct monster_info;
struct show_type;

bool is_door_tile(tileidx_t tile);

// Tile index lookup from Crawl data.
tileidx_t tileidx_feature(const coord_def &gc);
tileidx_t tileidx_out_of_bounds(int branch);
void tileidx_out_of_los(tileidx_t *fg, tileidx_t *bg, tileidx_t *cloud, const coord_def& gc);

tileidx_t tileidx_monster(const monster_info& mon);
tileidx_t tileidx_draco_base(const monster_info& mon);
tileidx_t tileidx_draco_job(const monster_info& mon);

tileidx_t tileidx_item(const item_info &item);
tileidx_t tileidx_item_throw(const item_info &item, int dx, int dy);
tileidx_t tileidx_known_base_item(tileidx_t label);

tileidx_t tileidx_cloud(const cloud_info &cl, bool disturbance = false);
tileidx_t tileidx_bolt(const bolt &bolt);
tileidx_t vary_bolt_tile(tileidx_t tile, int dist);
tileidx_t tileidx_zap(int colour);
tileidx_t tileidx_spell(const spell_type spell);
tileidx_t tileidx_skill(const skill_type skill, int train);
tileidx_t tileidx_command(const command_type cmd);
tileidx_t tileidx_gametype(const game_type gtype);
tileidx_t tileidx_ability(const ability_type ability);

tileidx_t tileidx_known_brand(const item_def &item);
tileidx_t tileidx_corpse_brand(const item_def &item);

tileidx_t get_clean_map_idx(tileidx_t tile_idx, bool mon_only = false);
tileidx_t tileidx_unseen_flag(const coord_def &gc);

// Return the level of enchantment as an int.  None is 0, Randart is 4.
int enchant_to_int(const item_def &item);
// If tile has variations, select among them based upon the enchant of item.
tileidx_t tileidx_enchant_equ(const item_def &item, tileidx_t tile,
                              bool player = false);

#ifdef USE_TILE
void bind_item_tile(item_def &item);
#else
static inline void bind_item_tile(item_def &item) {}
#endif

// For a given fg/bg set of tile indices and a 1 character prefix,
// return index, flag, and tile name as a printable string.
string tile_debug_string(tileidx_t fg, tileidx_t bg, tileidx_t cloud, char prefix);

void tile_init_props(monster* mon);

#endif
