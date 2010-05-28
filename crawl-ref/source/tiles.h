/*
 *  File:       tiles.h
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#ifndef TILES_H
#define TILES_H

#ifdef USE_TILE

#include "tiledef_defines.h"

#include "beam.h"
#include "enum.h"
#include "player.h"
#include "show.h"

enum tag_version
{
    TILETAG_PRE_MCACHE = 71,
    TILETAG_CURRENT    = 72
};

struct demon_data
{
    demon_data() { head = body = wings = 0; }

    int head;
    int body;
    int wings;
};

//*tile1.cc: get data from core part and drives tile drawing codes

//**convert in-game data to tile index
int tileidx_feature(dungeon_feature_type feat, int gx, int gy);
int tileidx_player(int job);
void tileidx_unseen(unsigned int &fg, unsigned int &bg, int ch,
                    const coord_def& gc);
int tileidx_item(const item_def &item);
int tileidx_item_throw(const item_def &item, int dx, int dy);
int tileidx_bolt(const bolt &bolt);
int tileidx_zap(int colour);
int tile_idx_unseen_terrain(int x, int y, int what);
int tile_unseen_flag(const coord_def& gc);
int tileidx_monster_base(const monsters *mon, bool detected = false);
int tileidx_monster(const monsters *mon, bool detected = false);
int tileidx_spell(spell_type spell);

// Player tile related
int get_gender_from_tile(const int parts[]);
bool is_player_tile(const int tile, const int base_tile);
int tilep_species_to_base_tile(int sp = you.species,
                               int level = you.experience_level);
void tilep_draconian_init(int sp, int level, int &base, int &head, int &wing);
void tilep_race_default(int sp, int gender, int level, int *parts);
void tilep_job_default(int job, int gender, int *parts);
void tilep_calc_flags(const int parts[], int flag[]);

void tilep_part_to_str(int number, char *buf);
int  tilep_str_to_part(char *str);

void tilep_scan_parts(char *fbuf, dolls_data &doll, int species = you.species,
                      int level = you.experience_level);
void tilep_print_parts(char *fbuf, const dolls_data &doll);

int tilep_equ_weapon(const item_def &item);
int tilep_equ_shield(const item_def &item);
int tilep_equ_armour(const item_def &item);
int tilep_equ_cloak(const item_def &item);
int tilep_equ_helm(const item_def &item);
int tilep_equ_gloves(const item_def &item);
int tilep_equ_boots(const item_def &item);

// Tile display related
void tile_place_monster(int gx, int gy, int idx, bool foreground = true,
                        bool detected = false);
void tile_place_item(int x, int y, int idx);
void tile_place_item_marker(int x, int y, int idx);
void tile_place_cloud(int x, int y, cloud_struct cl);
void tile_place_ray(const coord_def& gc, bool in_range);
void tile_draw_rays(bool resetCount);
void tile_clear_buf();

void tile_apply_animations(screen_buffer_t bg, tile_flavour *flv);
void tile_apply_properties(const coord_def &gc, screen_buffer_t *fg,
                           screen_buffer_t *bg);

// Tile Inventory display
void tile_draw_inv(int flag = -1);
// Multiple pickup
void tile_pick_menu();

// Set the default type of walls and floors.
void tile_init_default_flavour();
// Get the default types of walls and floors
void tile_default_flv(level_area_type lev, branch_type br, tile_flavour &flv);
// Clear the per-cell wall and floor flavors.
void tile_clear_flavour();
// Initialise per-cell types of walls and floors using defaults.
void tile_init_flavour();
void tile_init_flavour(const coord_def &gc);

void tile_floor_halo(dungeon_feature_type target, int tile);

void tile_set_force_redraw_tiles(bool redraw);
void tile_set_force_redraw_inv(bool redraw);

tag_pref string2tag_pref(const char *opt);
const char *tag_pref2string(tag_pref pref);

/**************************************/
/* tile2.cc  image manipulation       */
/**************************************/
// init them all
void TileInit();

void TileResizeScreen(int x, int y);

// display tile cursor, returns old cursor value there
int TileDrawCursor(int x, int y, int flag);

void TileNewLevel(bool first_time);

int item_unid_type(const item_def &item);
int tile_known_brand(const item_def item);
int tile_corpse_brand(const item_def item);

int get_clean_map_idx(int tile_idx);

// Flags for drawing routines
enum tile_flags
{
    // Foreground flags
    TILE_FLAG_S_UNDER    = 0x00000800,
    TILE_FLAG_FLYING     = 0x00001000,
    TILE_FLAG_PET        = 0x00002000,
    TILE_FLAG_GD_NEUTRAL = 0x00004000,
    TILE_FLAG_NEUTRAL    = 0x00008000,
    TILE_FLAG_STAB       = 0x00010000,
    TILE_FLAG_MAY_STAB   = 0x00020000,
    TILE_FLAG_NET        = 0x00040000,
    TILE_FLAG_POISON     = 0x00080000,
    TILE_FLAG_ANIM_WEP   = 0x00100000,
    TILE_FLAG_FLAME      = 0x00200000,
    TILE_FLAG_BERSERK    = 0x00400000,

    // MDAM has 5 possibilities, so uses 3 bits.
    TILE_FLAG_MDAM_MASK  = 0x03800000,
    TILE_FLAG_MDAM_LIGHT = 0x00800000,
    TILE_FLAG_MDAM_MOD   = 0x01000000,
    TILE_FLAG_MDAM_HEAVY = 0x01800000,
    TILE_FLAG_MDAM_SEV   = 0x02000000,
    TILE_FLAG_MDAM_ADEAD = 0x02800000,

    // Demon difficulty has 5 possibilities, so uses 3 bits.
    TILE_FLAG_DEMON   = 0x34000000,
    TILE_FLAG_DEMON_5 = 0x04000000,
    TILE_FLAG_DEMON_4 = 0x10000000,
    TILE_FLAG_DEMON_3 = 0x14000000,
    TILE_FLAG_DEMON_2 = 0x20000000,
    TILE_FLAG_DEMON_1 = 0x24000000,

    // Background flags
    TILE_FLAG_RAY        = 0x00000800,
    TILE_FLAG_MM_UNSEEN  = 0x00001000,
    TILE_FLAG_UNSEEN     = 0x00002000,
    TILE_FLAG_CURSOR1    = 0x00004000,
    TILE_FLAG_CURSOR2    = 0x00008000,
    TILE_FLAG_CURSOR3    = 0x0000C000,
    TILE_FLAG_CURSOR     = 0x0000C000,
    TILE_FLAG_BLOOD      = 0x00010000,
    TILE_FLAG_HALO       = 0x00020000,
    TILE_FLAG_NEW_STAIR  = 0x00040000,
    TILE_FLAG_WAS_SECRET = 0x00080000,
    TILE_FLAG_TRAV_EXCL  = 0x00100000,
    TILE_FLAG_EXCL_CTR   = 0x00200000,
    TILE_FLAG_SANCTUARY  = 0x00400000,
    TILE_FLAG_TUT_CURSOR = 0x00800000,
    TILE_FLAG_RAY_OOR    = 0x01000000,
    TILE_FLAG_OOR        = 0x02000000,
    TILE_FLAG_WATER      = 0x04000000,
    TILE_FLAG_MOLD       = 0x08000000,
    TILE_FLAG_SILENCED   = 0x10000000,

    // General
    TILE_FLAG_MASK       = 0x000007FF
};

enum
{
    TILEI_FLAG_SELECT  = 0x0100,
    TILEI_FLAG_TRIED   = 0x0200,
    TILEI_FLAG_EQUIP   = 0x0400,
    TILEI_FLAG_FLOOR   = 0x0800,
    TILEI_FLAG_CURSE   = 0x1000,
    TILEI_FLAG_CURSOR  = 0x2000,
    TILEI_FLAG_MELDED  = 0x4000,
    TILEI_FLAG_INVALID = 0x8000
};

enum
{
    TILEP_GENDER_FEMALE = 0,
    TILEP_GENDER_MALE   = 1,
    TILEP_SHOW_EQUIP    = 0x1000
};

enum tile_player_flag_cut
{
    TILEP_FLAG_HIDE,
    TILEP_FLAG_NORMAL,
    TILEP_FLAG_CUT_CENTAUR,
    TILEP_FLAG_CUT_NAGA
};

// normal tile size in px
enum
{
    TILE_X = 32,
    TILE_Y = 32
};

// Don't change this without also modifying the data save/load routines.
enum
{
    NUM_MAX_DOLLS = 10
};

#endif
#endif
