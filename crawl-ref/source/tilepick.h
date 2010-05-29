#ifndef TILEPICK_H
#define TILEPICK_H

#ifdef USE_TILE

#include "tiledef_defines.h"

struct bolt;
struct cloud_struct;
class coord_def;
class dolls_data;
class item_def;
class monsters;
class tile_flavour;

// Tile index lookup from Crawl data.
tileidx_t tileidx_feature(dungeon_feature_type feat, const coord_def &gc);
tileidx_t tileidx_player(int job);
void tileidx_unseen(tileidx_t *fg, tileidx_t *bg, screen_buffer_t ch,
                    const coord_def& gc);
tileidx_t tileidx_item(const item_def &item);
tileidx_t tileidx_item_throw(const item_def &item, int dx, int dy);
tileidx_t tileidx_bolt(const bolt &bolt);
tileidx_t tileidx_zap(int colour);
tileidx_t tileidx_unseen_terrain(const coord_def &gc, int what);
tileidx_t tileidx_unseen_flag(const coord_def &gc);
tileidx_t tileidx_monster_base(const monsters *mon, bool detected = false);
tileidx_t tileidx_monster(const monsters *mon, bool detected = false);
tileidx_t tileidx_spell(spell_type spell);
tileidx_t tileidx_known_brand(const item_def &item);
tileidx_t tileidx_corpse_brand(const item_def &item);
tileidx_t get_clean_map_idx(tileidx_t tile_idx);


// Player equipment lookup
tileidx_t tilep_equ_weapon(const item_def &item);
tileidx_t tilep_equ_shield(const item_def &item);
tileidx_t tilep_equ_armour(const item_def &item);
tileidx_t tilep_equ_cloak(const item_def &item);
tileidx_t tilep_equ_helm(const item_def &item);
tileidx_t tilep_equ_gloves(const item_def &item);
tileidx_t tilep_equ_boots(const item_def &item);


// Player tile related
int get_gender_from_tile(const dolls_data &doll);
bool is_player_tile(tileidx_t tile, tileidx_t base_tile);
tileidx_t tilep_species_to_base_tile(int sp, int level);

void tilep_draconian_init(int sp, int level, tileidx_t *base,
                          tileidx_t *head, tileidx_t *wing);
void tilep_race_default(int sp, int gender, int level, dolls_data *doll);
void tilep_job_default(int job, int gender, dolls_data *doll);
void tilep_calc_flags(const dolls_data &data, int flag[]);
void tilep_part_to_str(int number, char *buf);
int  tilep_str_to_part(char *str);
void tilep_scan_parts(char *fbuf, dolls_data &doll, int species, int level);
void tilep_print_parts(char *fbuf, const dolls_data &doll);


// Tile display related
void tile_place_monster(const coord_def &gc, const monsters *mons,
                        bool foreground = true, bool detected = false);
void tile_place_item(const coord_def &gc, const item_def &item);
void tile_place_item_marker(const coord_def &gc, const item_def &item);
void tile_place_cloud(const coord_def &gc, const cloud_struct &cl);
void tile_place_ray(const coord_def &gc, bool in_range);
void tile_draw_rays(bool reset_count);

void tile_apply_animations(tileidx_t bg, tile_flavour *flv);
void tile_apply_properties(const coord_def &gc, tileidx_t *fg,
                           tileidx_t *bg);

// Tile flavour

// Set the default type of walls and floors.
void tile_init_default_flavour();
// Get the default types of walls and floors
void tile_default_flv(level_area_type lev, branch_type br, tile_flavour &flv);
// Clear the per-cell wall and floor flavors.
void tile_clear_flavour();
// Initialise per-cell types of walls and floors using defaults.
void tile_init_flavour();
void tile_init_flavour(const coord_def &gc);
void tile_floor_halo(dungeon_feature_type target, tileidx_t tile);


// Miscellaneous.
void TileNewLevel(bool first_time);

#endif
#endif
