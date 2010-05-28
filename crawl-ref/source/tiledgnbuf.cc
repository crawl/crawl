/*
 *  File:       tiledgnbuf.cc
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "tiledgnbuf.h"

#include "cloud.h"
#include "coordit.h"
#include "env.h"
#include "tiledef-dngn.h"
#include "tiledef-player.h"
#include "tilemcache.h"

DungeonCellBuffer::DungeonCellBuffer(ImageManager *im) :
    m_buf_dngn(&im->m_textures[TEX_DUNGEON]),
    m_buf_doll(&im->m_textures[TEX_PLAYER], 17),
    m_buf_main_trans(&im->m_textures[TEX_DEFAULT], 17),
    m_buf_main(&im->m_textures[TEX_DEFAULT]),
    m_buf_spells(&im->m_textures[TEX_GUI])
{
}

static bool _in_water(const packed_cell &cell)
{
    return ((cell.bg & TILE_FLAG_WATER) && !(cell.fg & TILE_FLAG_FLYING));
}

static void _lichform_add_weapon(SubmergedTileBuffer &buf, int x, int y,
                                 bool in_water)
{
    const int item = you.equip[EQ_WEAPON];
    if (item == -1)
        return;

    const int wep = tilep_equ_weapon(you.inv[item]);
    if (!wep)
        return;

    buf.add(wep, x, y, 0, in_water, false, -1, 0);
}

void DungeonCellBuffer::add(const packed_cell &cell, int x, int y)
{
    pack_background(x, y, cell);

    const unsigned int fg_idx = cell.fg & TILE_FLAG_MASK;
    const bool in_water = _in_water(cell);

    if (fg_idx >= TILEP_MCACHE_START)
    {
        mcache_entry *entry = mcache.get(fg_idx);
        if (entry)
            pack_mcache(entry, x, y, in_water);
        else
            m_buf_doll.add(TILEP_MONS_UNKNOWN, x, y, 0, in_water, false);
    }
    else if (fg_idx == TILEP_PLAYER)
    {
        pack_player(x, y, in_water);
    }
    else if (fg_idx >= TILE_MAIN_MAX)
    {
        m_buf_doll.add(fg_idx, x, y, 0, in_water, false);
        if (fg_idx == TILEP_TRAN_LICH)
            _lichform_add_weapon(m_buf_doll, x, y, in_water);
    }

    pack_foreground(x, y, cell);

}

void DungeonCellBuffer::add_dngn_tile(int tileidx, int x, int y)
{
    m_buf_dngn.add(tileidx, x, y);
}

void DungeonCellBuffer::add_main_tile(int tileidx, int x, int y)
{
    m_buf_main.add(tileidx, x, y);
}

void DungeonCellBuffer::add_main_tile(int tileidx, int x, int y, int ox, int oy)
{
    m_buf_main.add(tileidx, x, y, ox, oy, false);
}

void DungeonCellBuffer::add_spell_tile(int tileidx, int x, int y)
{
    m_buf_spells.add(tileidx, x, y);
}

void DungeonCellBuffer::clear()
{
    m_buf_dngn.clear();
    m_buf_doll.clear();
    m_buf_main_trans.clear();
    m_buf_main.clear();
    m_buf_spells.clear();
}

void DungeonCellBuffer::draw()
{
    m_buf_dngn.draw();
    m_buf_doll.draw();
    m_buf_main_trans.draw();
    m_buf_main.draw();
    m_buf_spells.draw();
}

enum wave_type
{
    WV_NONE = 0,
    WV_SHALLOW,
    WV_DEEP
};

wave_type _get_wave_type(bool shallow)
{
    return (shallow ? WV_SHALLOW : WV_DEEP);
}

static void _pack_wave(int tileidx, packed_cell *cell)
{
    cell->dngn_overlay[cell->num_dngn_overlay++] = tileidx;
}

void pack_waves(const coord_def &gc, packed_cell *cell)
{
    if (!player_in_branch(BRANCH_SHOALS))
        return;

    // Add wave tiles on floor adjacent to shallow water.
    const dungeon_feature_type feat = env.map_knowledge(gc).feat();
    const bool feat_has_ink = (cloud_type_at(coord_def(gc)) == CLOUD_INK);

    if (feat == DNGN_DEEP_WATER && feat_has_ink)
    {
        _pack_wave(TILE_WAVE_INK_FULL, cell);
        return;
    }

    if (feat != DNGN_FLOOR && feat != DNGN_UNDISCOVERED_TRAP
        && feat != DNGN_SHALLOW_WATER && feat != DNGN_DEEP_WATER)
    {
        return;
    }

    const bool ink_only = (feat == DNGN_DEEP_WATER);

    wave_type north = WV_NONE, south = WV_NONE,
              east = WV_NONE, west = WV_NONE,
              ne = WV_NONE, nw = WV_NONE,
              se = WV_NONE, sw = WV_NONE;

    bool inkn = false, inks = false, inke = false, inkw = false,
         inkne = false, inknw = false, inkse = false, inksw = false;

    for (radius_iterator ri(gc, 1, true, false, true); ri; ++ri)
    {
        if (!is_terrain_seen(*ri) && !is_terrain_mapped(*ri))
            continue;

        const bool ink = (cloud_type_at(coord_def(*ri)) == CLOUD_INK);

        bool shallow = false;
        if (env.map_knowledge(*ri).feat() == DNGN_SHALLOW_WATER)
        {
            // Adjacent shallow water is only interesting for
            // floor cells.
            if (!ink && feat == DNGN_SHALLOW_WATER)
                continue;

            shallow = true;
        }
        else if (env.map_knowledge(*ri).feat() != DNGN_DEEP_WATER)
            continue;

        if (!ink_only)
        {
            if (ri->x == gc.x) // orthogonals
            {
                if (ri->y < gc.y)
                    north = _get_wave_type(shallow);
                else
                    south = _get_wave_type(shallow);
            }
            else if (ri->y == gc.y)
            {
                if (ri->x < gc.x)
                    west = _get_wave_type(shallow);
                else
                    east = _get_wave_type(shallow);
            }
            else // diagonals
            {
                if (ri->x < gc.x)
                {
                    if (ri->y < gc.y)
                        nw = _get_wave_type(shallow);
                    else
                        sw = _get_wave_type(shallow);
                }
                else
                {
                    if (ri->y < gc.y)
                        ne = _get_wave_type(shallow);
                    else
                        se = _get_wave_type(shallow);
                }
            }
        }
        if (!feat_has_ink && ink)
        {
            if (ri->x == gc.x) // orthogonals
            {
                if (ri->y < gc.y)
                    inkn = true;
                else
                    inks = true;
            }
            else if (ri->y == gc.y)
            {
                if (ri->x < gc.x)
                    inkw = true;
                else
                    inke = true;
            }
            else // diagonals
            {
                if (ri->x < gc.x)
                {
                    if (ri->y < gc.y)
                        inknw = true;
                    else
                        inksw = true;
                }
                else
                {
                    if (ri->y < gc.y)
                        inkne = true;
                    else
                        inkse = true;
                }
            }
        }
    }

    if (!ink_only)
    {
        // First check for shallow water.
        if (north == WV_SHALLOW)
            _pack_wave(TILE_WAVE_N, cell);
        if (south == WV_SHALLOW)
            _pack_wave(TILE_WAVE_S, cell);
        if (east == WV_SHALLOW)
            _pack_wave(TILE_WAVE_E, cell);
        if (west == WV_SHALLOW)
            _pack_wave(TILE_WAVE_W, cell);

        // Then check for deep water, overwriting shallow
        // corner waves, if necessary.
        if (north == WV_DEEP)
            _pack_wave(TILE_WAVE_DEEP_N, cell);
        if (south == WV_DEEP)
            _pack_wave(TILE_WAVE_DEEP_S, cell);
        if (east == WV_DEEP)
            _pack_wave(TILE_WAVE_DEEP_E, cell);
        if (west == WV_DEEP)
            _pack_wave(TILE_WAVE_DEEP_W, cell);

        if (ne == WV_SHALLOW && !north && !east)
            _pack_wave(TILE_WAVE_CORNER_NE, cell);
        else if (ne == WV_DEEP && north != WV_DEEP && east != WV_DEEP)
            _pack_wave(TILE_WAVE_DEEP_CORNER_NE, cell);
        if (nw == WV_SHALLOW && !north && !west)
            _pack_wave(TILE_WAVE_CORNER_NW, cell);
        else if (nw == WV_DEEP && north != WV_DEEP && west != WV_DEEP)
            _pack_wave(TILE_WAVE_DEEP_CORNER_NW, cell);
        if (se == WV_SHALLOW && !south && !east)
            _pack_wave(TILE_WAVE_CORNER_SE, cell);
        else if (se == WV_DEEP && south != WV_DEEP && east != WV_DEEP)
            _pack_wave(TILE_WAVE_DEEP_CORNER_SE, cell);
        if (sw == WV_SHALLOW && !south && !west)
            _pack_wave(TILE_WAVE_CORNER_SW, cell);
        else if (sw == WV_DEEP && south != WV_DEEP && west != WV_DEEP)
            _pack_wave(TILE_WAVE_DEEP_CORNER_SW, cell);
    }

    // Overlay with ink sheen, if necessary.
    if (feat_has_ink)
        _pack_wave(TILE_WAVE_INK_FULL, cell);
    else
    {
        if (inkn)
            _pack_wave(TILE_WAVE_INK_N, cell);
        if (inks)
            _pack_wave(TILE_WAVE_INK_S, cell);
        if (inke)
            _pack_wave(TILE_WAVE_INK_E, cell);
        if (inkw)
            _pack_wave(TILE_WAVE_INK_W, cell);
        if (inkne || inkn || inke)
            _pack_wave(TILE_WAVE_INK_CORNER_NE, cell);
        if (inknw || inkn || inkw)
            _pack_wave(TILE_WAVE_INK_CORNER_NW, cell);
        if (inkse || inks || inke)
            _pack_wave(TILE_WAVE_INK_CORNER_SE, cell);
        if (inksw || inks || inkw)
            _pack_wave(TILE_WAVE_INK_CORNER_SW, cell);
    }
}

void DungeonCellBuffer::pack_background(int x, int y, const packed_cell &cell)
{
    const unsigned int bg = cell.bg;
    const unsigned int bg_idx = cell.bg & TILE_FLAG_MASK;

    if (bg_idx >= TILE_DNGN_WAX_WALL)
    {
        m_buf_dngn.add(cell.flv.floor, x, y);
    }
    m_buf_dngn.add(bg_idx, x, y);

    if (bg_idx > TILE_DNGN_UNSEEN)
    {
        if (bg & TILE_FLAG_WAS_SECRET)
            m_buf_dngn.add(TILE_DNGN_DETECTED_SECRET_DOOR, x, y);

        if (bg & TILE_FLAG_BLOOD)
        {
            int offset = cell.flv.special % tile_dngn_count(TILE_BLOOD);
            m_buf_dngn.add(TILE_BLOOD + offset, x, y);
        }
        else if (bg & TILE_FLAG_MOLD)
        {
            int offset = cell.flv.special % tile_dngn_count(TILE_MOLD);
            m_buf_dngn.add(TILE_MOLD + offset, x, y);
        }

        for (int i = 0; i < cell.num_dngn_overlay; ++i)
            m_buf_dngn.add(cell.dngn_overlay[i], x, y);

        if (bg & TILE_FLAG_HALO)
            m_buf_dngn.add(TILE_HALO, x, y);

        if (!(bg & TILE_FLAG_UNSEEN))
        {
            if (bg & TILE_FLAG_SANCTUARY)
                m_buf_dngn.add(TILE_SANCTUARY, x, y);
            if (bg & TILE_FLAG_SILENCED)
                m_buf_dngn.add(TILE_SILENCED, x, y);

            // Apply the travel exclusion under the foreground if the cell is
            // visible.  It will be applied later if the cell is unseen.
            if (bg & TILE_FLAG_EXCL_CTR)
                m_buf_dngn.add(TILE_TRAVEL_EXCLUSION_CENTRE_BG, x, y);
            else if (bg & TILE_FLAG_TRAV_EXCL)
                m_buf_dngn.add(TILE_TRAVEL_EXCLUSION_BG, x, y);
        }

        if (bg & TILE_FLAG_RAY)
            m_buf_dngn.add(TILE_RAY, x, y);
        else if (bg & TILE_FLAG_RAY_OOR)
            m_buf_dngn.add(TILE_RAY_OUT_OF_RANGE, x, y);
    }
}

void DungeonCellBuffer::pack_foreground(int x, int y, const packed_cell &cell)
{
    const unsigned int fg = cell.fg;
    const unsigned int bg = cell.bg;
    const unsigned int fg_idx = cell.fg & TILE_FLAG_MASK;
    const bool in_water = _in_water(cell);

    if (fg_idx && fg_idx <= TILE_MAIN_MAX)
    {
        if (in_water)
            m_buf_main_trans.add(fg_idx, x, y, 0, true, false);
        else
            m_buf_main.add(fg_idx, x, y);
    }

    if (fg & TILE_FLAG_NET)
        m_buf_main.add(TILE_TRAP_NET, x, y);

    if (fg & TILE_FLAG_S_UNDER)
        m_buf_main.add(TILE_SOMETHING_UNDER, x, y);

    int status_shift = 0;
    if (fg & TILE_FLAG_BERSERK)
    {
        m_buf_main.add(TILE_BERSERK, x, y);
        status_shift += 10;
    }

    // Pet mark
    if (fg & TILE_FLAG_PET)
    {
        m_buf_main.add(TILE_HEART, x, y);
        status_shift += 10;
    }
    else if (fg & TILE_FLAG_GD_NEUTRAL)
    {
        m_buf_main.add(TILE_GOOD_NEUTRAL, x, y);
        status_shift += 8;
    }
    else if (fg & TILE_FLAG_NEUTRAL)
    {
        m_buf_main.add(TILE_NEUTRAL, x, y);
        status_shift += 8;
    }
    else if (fg & TILE_FLAG_STAB)
    {
        m_buf_main.add(TILE_STAB_BRAND, x, y);
        status_shift += 8;
    }
    else if (fg & TILE_FLAG_MAY_STAB)
    {
        m_buf_main.add(TILE_MAY_STAB_BRAND, x, y);
        status_shift += 5;
    }

    if (fg & TILE_FLAG_POISON)
    {
        m_buf_main.add(TILE_POISON, x, y, -status_shift, 0);
        status_shift += 5;
    }
    if (fg & TILE_FLAG_FLAME)
    {
        m_buf_main.add(TILE_FLAME, x, y, -status_shift, 0);
        status_shift += 5;
    }

    if (fg & TILE_FLAG_ANIM_WEP)
        m_buf_main.add(TILE_ANIMATED_WEAPON, x, y);

    if (bg & TILE_FLAG_UNSEEN && (bg != TILE_FLAG_UNSEEN || fg))
        m_buf_main.add(TILE_MESH, x, y);

    if (bg & TILE_FLAG_OOR && (bg != TILE_FLAG_OOR || fg))
        m_buf_main.add(TILE_OOR_MESH, x, y);

    if (bg & TILE_FLAG_MM_UNSEEN && (bg != TILE_FLAG_MM_UNSEEN || fg))
        m_buf_main.add(TILE_MAGIC_MAP_MESH, x, y);

    // Don't let the "new stair" icon cover up any existing icons, but
    // draw it otherwise.
    if (bg & TILE_FLAG_NEW_STAIR && status_shift == 0)
        m_buf_main.add(TILE_NEW_STAIR, x, y);

    if (bg & TILE_FLAG_EXCL_CTR && (bg & TILE_FLAG_UNSEEN))
        m_buf_main.add(TILE_TRAVEL_EXCLUSION_CENTRE_FG, x, y);
    else if (bg & TILE_FLAG_TRAV_EXCL && (bg & TILE_FLAG_UNSEEN))
        m_buf_main.add(TILE_TRAVEL_EXCLUSION_FG, x, y);

    // Tutorial cursor takes precedence over other cursors.
    if (bg & TILE_FLAG_TUT_CURSOR)
    {
        m_buf_main.add(TILE_TUTORIAL_CURSOR, x, y);
    }
    else if (bg & TILE_FLAG_CURSOR)
    {
        int type = ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR1) ?
            TILE_CURSOR : TILE_CURSOR2;

        if ((bg & TILE_FLAG_CURSOR) == TILE_FLAG_CURSOR3)
           type = TILE_CURSOR3;

        m_buf_main.add(type, x, y);
    }

    if (fg & TILE_FLAG_MDAM_MASK)
    {
        unsigned int mdam_flag = fg & TILE_FLAG_MDAM_MASK;
        if (mdam_flag == TILE_FLAG_MDAM_LIGHT)
            m_buf_main.add(TILE_MDAM_LIGHTLY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_MOD)
            m_buf_main.add(TILE_MDAM_MODERATELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_HEAVY)
            m_buf_main.add(TILE_MDAM_HEAVILY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_SEV)
            m_buf_main.add(TILE_MDAM_SEVERELY_DAMAGED, x, y);
        else if (mdam_flag == TILE_FLAG_MDAM_ADEAD)
            m_buf_main.add(TILE_MDAM_ALMOST_DEAD, x, y);
    }

    if (fg & TILE_FLAG_DEMON)
    {
        unsigned int demon_flag = fg & TILE_FLAG_DEMON;
        if (demon_flag == TILE_FLAG_DEMON_1)
            m_buf_main.add(TILE_DEMON_NUM1, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_2)
            m_buf_main.add(TILE_DEMON_NUM2, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_3)
            m_buf_main.add(TILE_DEMON_NUM3, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_4)
            m_buf_main.add(TILE_DEMON_NUM4, x, y);
        else if (demon_flag == TILE_FLAG_DEMON_5)
            m_buf_main.add(TILE_DEMON_NUM5, x, y);
    }
}

void DungeonCellBuffer::pack_player(int x, int y, bool submerged)
{
    dolls_data result = player_doll;
    fill_doll_equipment(result);
    pack_doll(result, x, y, submerged, false);
}

void DungeonCellBuffer::pack_doll(const dolls_data &doll, int x, int y,
                                  bool submerged, bool ghost)
{
    pack_doll_buf(m_buf_doll, doll, x, y, submerged, ghost);
}


void DungeonCellBuffer::pack_mcache(mcache_entry *entry, int x, int y,
                                    bool submerged)
{
    ASSERT(entry);

    bool trans = entry->transparent();

    const dolls_data *doll = entry->doll();
    if (doll)
        pack_doll(*doll, x, y, submerged, trans);

    tile_draw_info dinfo[3];
    unsigned int draw_info_count = entry->info(&dinfo[0]);
    ASSERT(draw_info_count <= sizeof(dinfo) / (sizeof(dinfo[0])));

    for (unsigned int i = 0; i < draw_info_count; i++)
    {
        m_buf_doll.add(dinfo[i].idx, x, y, 0, submerged, trans,
                       dinfo[i].ofs_x, dinfo[i].ofs_y);
    }
}

#endif
