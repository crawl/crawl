/*
 *  File:       tiledgnbuf.h
 */

#ifdef USE_TILE
#ifndef TILEDGNBUF_H
#define TILEDGNBUF_H

#include "tilebuf.h"

struct packed_cell
{
    // For anything that requires multiple dungeon tiles (such as waves)
    // These tiles will be placed directly on top of the bg tile.
    enum { MAX_DNGN_OVERLAY = 20 };
    int num_dngn_overlay;
    FixedVector<int, MAX_DNGN_OVERLAY> dngn_overlay;

    tileidx_t fg;
    tileidx_t bg;
    tile_flavour flv;

    bool is_bloody;
    bool is_silenced;
    bool is_haloed;
    bool is_moldy;
    bool is_sanctuary;
    bool is_liquefied;
    bool swamp_tree_water;

    packed_cell() : num_dngn_overlay(0), is_bloody(false), is_silenced(false),
                    is_haloed(false), is_moldy(false), is_sanctuary(false), 
                    is_liquefied(false), swamp_tree_water (false) {}

    packed_cell(const packed_cell* c) : num_dngn_overlay(c->num_dngn_overlay),
                                        fg(c->fg), bg(c->bg), flv(c->flv),
                                        is_bloody(c->is_bloody),
                                        is_silenced(c->is_silenced),
                                        is_haloed(c->is_haloed),
                                        is_moldy(c->is_moldy),
                                        is_sanctuary(c->is_sanctuary),
                                        is_liquefied(c->is_liquefied),
                                        swamp_tree_water(c->swamp_tree_water) {}

    void clear();
};

// For a given location, pack any waves/ink/wall shadow tiles
// that require knowledge of the surrounding env cells.
void pack_cell_overlays(const coord_def &gc, packed_cell *cell);

struct dolls_data;
class mcache_entry;
class ImageManager;

// A set of buffers that takes as input the foreground/background pair
// of tiles from tile_fg/tile_bg and populates a set of tile buffers
// so that the stack of tiles it represents can be easily drawn.
//
// This class should access as little global state as possible.
// If more information needs to be shared to draw, then either add
// a foreground or background flag, or add a member to packed_cell.
class DungeonCellBuffer
{
public:
    DungeonCellBuffer(ImageManager *im);

    void add(const packed_cell &cell, int x, int y);
    void add_dngn_tile(int tileidx, int x, int y, bool in_water = false);
    void add_main_tile(int tileidx, int x, int y);
    void add_main_tile(int tileidx, int x, int y, int ox, int oy);
    void add_spell_tile(int tileidx, int x, int y);
    void add_skill_tile(int tileidx, int x, int y);
    void add_command_tile(int tileidx, int x, int y);
    void add_icons_tile(int tileidx, int x, int y);
    void add_icons_tile(int tileidx, int x, int y, int ox, int oy);

    void clear();
    void draw();

protected:
    void add_blood_overlay(int x, int y, const packed_cell &cell);
    void pack_background(int x, int y, const packed_cell &cell);
    void pack_foreground(int x, int y, const packed_cell &cell);

    void pack_mcache(mcache_entry *entry, int x, int y, bool submerged);
    void pack_player(int x, int y, bool submerged);
    void pack_doll(const dolls_data &doll, int x, int y,
                   bool submerged, bool ghost);

    TileBuffer m_buf_floor;
    TileBuffer m_buf_wall;
    TileBuffer m_buf_feat;
    SubmergedTileBuffer m_buf_feat_trans;
    SubmergedTileBuffer m_buf_doll;
    SubmergedTileBuffer m_buf_main_trans;
    TileBuffer m_buf_main;
    TileBuffer m_buf_spells;
    TileBuffer m_buf_skills;
    TileBuffer m_buf_commands;
    TileBuffer m_buf_icons;
};

#endif
#endif
