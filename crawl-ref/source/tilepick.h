/*
 *  File:       tilepick.h
 *  Summary:    Look-up functions for dungeon and item tiles.
 */

#ifndef TILEPICK_H
#define TILEPICK_H

#ifdef USE_TILE

#include "tiledef_defines.h"

struct bolt;
struct cloud_struct;
class coord_def;
class item_def;
class monsters;

// Tile index lookup from Crawl data.
tileidx_t tileidx_feature(dungeon_feature_type feat, const coord_def &gc);

tileidx_t tileidx_monster_base(const monsters *mon, bool detected = false);
tileidx_t tileidx_monster(const monsters *mon, bool detected = false);

tileidx_t tileidx_item(const item_def &item);
tileidx_t tileidx_item_throw(const item_def &item, int dx, int dy);

tileidx_t tileidx_cloud(const cloud_struct &cl);
tileidx_t tileidx_bolt(const bolt &bolt);
tileidx_t tileidx_zap(int colour);
tileidx_t tileidx_spell(spell_type spell);

tileidx_t tileidx_known_brand(const item_def &item);
tileidx_t tileidx_corpse_brand(const item_def &item);

tileidx_t get_clean_map_idx(tileidx_t tile_idx);
tileidx_t tileidx_unseen_terrain(const coord_def &gc, int what);
tileidx_t tileidx_unseen_flag(const coord_def &gc);
void tileidx_unseen(tileidx_t *fg, tileidx_t *bg, screen_buffer_t ch,
                    const coord_def& gc);


// Return the level of enchantment as an int.  None is 0, Randart is 4.
int enchant_to_int(const item_def &item);
// If tile has variations, select among them based upon the enchant of item.
tileidx_t tileidx_enchant_equ(const item_def &item, tileidx_t tile);


#endif
#endif
