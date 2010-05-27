/*
 *  File:       tiledgnbuf.h
 */

#ifdef USE_TILE
#ifndef TILEDGNBUF_H
#define TILEDGNBUF_H

#include "tilebuf.h"

struct packed_cell
{
    packed_cell() : num_dngn_overlay(0) {}

    unsigned int fg;
    unsigned int bg;
    tile_flavour flv;

    // For anything that requires multiple dungeon tiles (such as waves)
    // These tiles will be placed directly on top of the bg tile.
    enum { MAX_DNGN_OVERLAY = 20 };
    int num_dngn_overlay;
    FixedVector<int, MAX_DNGN_OVERLAY> dngn_overlay;
};

// For a given location, pack any waves (or ink) into the
// dungeon overlay of the passed in packed_cell.
void pack_waves(const coord_def &gc, packed_cell *cell);

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
    void add_dngn_tile(int tileidx, int x, int y);
    void add_main_tile(int tileidx, int x, int y);
    void add_main_tile(int tileidx, int x, int y, int ox, int oy);
    void add_spell_tile(int tileidx, int x, int y);

    void clear();
    void draw();

protected:
    void pack_background(int x, int y, const packed_cell &cell);
    void pack_foreground(int x, int y, const packed_cell &cell);

    void pack_mcache(mcache_entry *entry, int x, int y, bool submerged);
    void pack_player(int x, int y, bool submerged);
    void pack_doll(const dolls_data &doll, int x, int y,
                   bool submerged, bool ghost);

    TileBuffer m_buf_dngn;
    SubmergedTileBuffer m_buf_doll;
    SubmergedTileBuffer m_buf_main_trans;
    TileBuffer m_buf_main;
    TileBuffer m_buf_spells;
};

#endif
#endif
