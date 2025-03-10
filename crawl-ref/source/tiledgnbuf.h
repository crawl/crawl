#ifdef USE_TILE_LOCAL
#pragma once

#include "tilebuf.h"
#include "tilecell.h"

struct dolls_data;
class mcache_entry;

class ImageManager;
class FontWrapper;

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
    DungeonCellBuffer(const ImageManager *im);

    void add(const packed_cell &cell, int x, int y);
    void add_monster(const monster_info &mon, int x, int y);
    void add_dngn_tile(int tileidx, int x, int y, bool in_water = false);
    void add_main_tile(int tileidx, int x, int y);
    void add_special_tile(int tileidx, int x, int y, int ox, int oy);
    void add_spell_tile(int tileidx, int x, int y);
    void add_skill_tile(int tileidx, int x, int y);
    void add_command_tile(int tileidx, int x, int y);
    void add_icons_tile(int tileidx, int x, int y);
    void add_icons_tile(int tileidx, int x, int y, int ox, int oy);
    void add_glyph(const char32_t &g, const VColour &col, int x, int y);
    void add_glyph(const char32_t &g, const VColour &col, const VColour &bg, int x, int y);
    FontWrapper *get_glyph_font();

    void clear();
    void draw();
    void draw_glyphs();
    void draw_tiles();
    void draw_icons();
protected:
    void add_blood_overlay(int x, int y, const packed_cell &cell,
                           bool is_wall = false);
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
    FontBuffer m_buf_glyphs;
};

#endif
