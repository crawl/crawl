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
tileidx_t tileidx_cloud(const cloud_struct &cl);


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

#endif
#endif
