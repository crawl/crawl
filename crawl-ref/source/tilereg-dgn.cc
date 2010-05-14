/*
 *  File:       tilereg-dgn.cc
 *
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "cio.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "menu.h"
#include "describe.h"
#include "invent.h"
#include "itemprop.h"
#include "jobs.h"
#include "libutil.h"
#include "macro.h"
#include "misc.h"
#include "mon-util.h"
#include "options.h"
#include "religion.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stash.h"
#include "stuff.h"
#include "terrain.h"
#include "tiledef-dngn.h"
#include "tiledef-main.h"
#include "tiledef-player.h"
#include "tilefont.h"
#include "tilemcache.h"
#include "tilereg-dgn.h"
#include "trap_def.h"
#include "traps.h"
#include "travel.h"
#include "viewgeom.h"

DungeonRegion::DungeonRegion(const TileRegionInit &init) :
    TileRegion(init),
    m_cx_to_gx(0),
    m_cy_to_gy(0),
    m_buf_dngn(&init.im->m_textures[TEX_DUNGEON]),
    m_buf_doll(&init.im->m_textures[TEX_PLAYER], 17),
    m_buf_main_trans(&init.im->m_textures[TEX_DEFAULT], 17),
    m_buf_main(&init.im->m_textures[TEX_DEFAULT])
{
    for (int i = 0; i < CURSOR_MAX; i++)
        m_cursor[i] = NO_CURSOR;
}

DungeonRegion::~DungeonRegion()
{
}

void DungeonRegion::load_dungeon(unsigned int* tileb, int cx_to_gx, int cy_to_gy)
{
    m_tileb.clear();
    m_dirty = true;

    if (!tileb)
        return;

    int len = 2 * crawl_view.viewsz.x * crawl_view.viewsz.y;
    m_tileb.resize(len);
    // TODO enne - move this function into dungeonregion
    tile_finish_dngn(tileb, cx_to_gx + mx/2, cy_to_gy + my/2);
    memcpy(&m_tileb[0], tileb, sizeof(unsigned int) * len);

    m_cx_to_gx = cx_to_gx;
    m_cy_to_gy = cy_to_gy;

    place_cursor(CURSOR_TUTORIAL, m_cursor[CURSOR_TUTORIAL]);
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

void DungeonRegion::pack_background(unsigned int bg, int x, int y)
{
    unsigned int bg_idx = bg & TILE_FLAG_MASK;

    if (bg_idx >= TILE_DNGN_WAX_WALL)
    {
        tile_flavour &flv = env.tile_flv[x + m_cx_to_gx][y + m_cy_to_gy];
        m_buf_dngn.add(flv.floor, x, y);
    }
    m_buf_dngn.add(bg_idx, x, y);

    if (bg_idx > TILE_DNGN_UNSEEN)
    {
        if (bg & TILE_FLAG_WAS_SECRET)
            m_buf_dngn.add(TILE_DNGN_DETECTED_SECRET_DOOR, x, y);

        if (bg & TILE_FLAG_BLOOD)
        {
            tile_flavour &flv = env.tile_flv[x + m_cx_to_gx][y + m_cy_to_gy];
            int offset = flv.special % tile_dngn_count(TILE_BLOOD);
            m_buf_dngn.add(TILE_BLOOD + offset, x, y);
        }
        else if (bg & TILE_FLAG_MOLD)
        {
            tile_flavour &flv = env.tile_flv[x + m_cx_to_gx][y + m_cy_to_gy];
            int offset = flv.special % tile_dngn_count(TILE_MOLD);
            m_buf_dngn.add(TILE_MOLD + offset, x, y);
        }

        if (player_in_branch(BRANCH_SHOALS))
        {
            // Add wave tiles on floor adjacent to shallow water.
            const coord_def pos = coord_def(x + m_cx_to_gx, y + m_cy_to_gy);
            const dungeon_feature_type feat = env.map_knowledge(pos).feat();
            const bool feat_has_ink
                            = (cloud_type_at(coord_def(pos)) == CLOUD_INK);

            if (feat == DNGN_DEEP_WATER && feat_has_ink)
            {
                m_buf_dngn.add(TILE_WAVE_INK_FULL, x, y);
            }
            else if (feat == DNGN_FLOOR || feat == DNGN_UNDISCOVERED_TRAP
                     || feat == DNGN_SHALLOW_WATER
                     || feat == DNGN_DEEP_WATER)
            {
                const bool ink_only = (feat == DNGN_DEEP_WATER);

                wave_type north = WV_NONE, south = WV_NONE,
                          east = WV_NONE, west = WV_NONE,
                          ne = WV_NONE, nw = WV_NONE,
                          se = WV_NONE, sw = WV_NONE;

                bool inkn = false, inks = false, inke = false, inkw = false,
                     inkne = false, inknw = false, inkse = false, inksw = false;

                for (radius_iterator ri(pos, 1, true, false, true); ri; ++ri)
                {
                    if (!is_terrain_seen(*ri) && !is_terrain_mapped(*ri))
                        continue;

                    const bool ink
                        = (cloud_type_at(coord_def(*ri)) == CLOUD_INK);

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
                        if (ri->x == pos.x) // orthogonals
                        {
                            if (ri->y < pos.y)
                                north = _get_wave_type(shallow);
                            else
                                south = _get_wave_type(shallow);
                        }
                        else if (ri->y == pos.y)
                        {
                            if (ri->x < pos.x)
                                west = _get_wave_type(shallow);
                            else
                                east = _get_wave_type(shallow);
                        }
                        else // diagonals
                        {
                            if (ri->x < pos.x)
                            {
                                if (ri->y < pos.y)
                                    nw = _get_wave_type(shallow);
                                else
                                    sw = _get_wave_type(shallow);
                            }
                            else
                            {
                                if (ri->y < pos.y)
                                    ne = _get_wave_type(shallow);
                                else
                                    se = _get_wave_type(shallow);
                            }
                        }
                    }
                    if (!feat_has_ink && ink)
                    {
                        if (ri->x == pos.x) // orthogonals
                        {
                            if (ri->y < pos.y)
                                inkn = true;
                            else
                                inks = true;
                        }
                        else if (ri->y == pos.y)
                        {
                            if (ri->x < pos.x)
                                inkw = true;
                            else
                                inke = true;
                        }
                        else // diagonals
                        {
                            if (ri->x < pos.x)
                            {
                                if (ri->y < pos.y)
                                    inknw = true;
                                else
                                    inksw = true;
                            }
                            else
                            {
                                if (ri->y < pos.y)
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
                        m_buf_dngn.add(TILE_WAVE_N, x, y);
                    if (south == WV_SHALLOW)
                        m_buf_dngn.add(TILE_WAVE_S, x, y);
                    if (east == WV_SHALLOW)
                        m_buf_dngn.add(TILE_WAVE_E, x, y);
                    if (west == WV_SHALLOW)
                        m_buf_dngn.add(TILE_WAVE_W, x, y);

                    // Then check for deep water, overwriting shallow
                    // corner waves, if necessary.
                    if (north == WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_N, x, y);
                    if (south == WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_S, x, y);
                    if (east == WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_E, x, y);
                    if (west == WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_W, x, y);

                    if (ne == WV_SHALLOW && !north && !east)
                        m_buf_dngn.add(TILE_WAVE_CORNER_NE, x, y);
                    else if (ne == WV_DEEP && north != WV_DEEP && east != WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_CORNER_NE, x, y);
                    if (nw == WV_SHALLOW && !north && !west)
                        m_buf_dngn.add(TILE_WAVE_CORNER_NW, x, y);
                    else if (nw == WV_DEEP && north != WV_DEEP && west != WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_CORNER_NW, x, y);
                    if (se == WV_SHALLOW && !south && !east)
                        m_buf_dngn.add(TILE_WAVE_CORNER_SE, x, y);
                    else if (se == WV_DEEP && south != WV_DEEP && east != WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_CORNER_SE, x, y);
                    if (sw == WV_SHALLOW && !south && !west)
                        m_buf_dngn.add(TILE_WAVE_CORNER_SW, x, y);
                    else if (sw == WV_DEEP && south != WV_DEEP && west != WV_DEEP)
                        m_buf_dngn.add(TILE_WAVE_DEEP_CORNER_SW, x, y);
                }

                // Overlay with ink sheen, if necessary.
                if (feat_has_ink)
                    m_buf_dngn.add(TILE_WAVE_INK_FULL, x, y);
                else
                {
                    if (inkn)
                        m_buf_dngn.add(TILE_WAVE_INK_N, x, y);
                    if (inks)
                        m_buf_dngn.add(TILE_WAVE_INK_S, x, y);
                    if (inke)
                        m_buf_dngn.add(TILE_WAVE_INK_E, x, y);
                    if (inkw)
                        m_buf_dngn.add(TILE_WAVE_INK_W, x, y);
                    if (inkne || inkn || inke)
                        m_buf_dngn.add(TILE_WAVE_INK_CORNER_NE, x, y);
                    if (inknw || inkn || inkw)
                        m_buf_dngn.add(TILE_WAVE_INK_CORNER_NW, x, y);
                    if (inkse || inks || inke)
                        m_buf_dngn.add(TILE_WAVE_INK_CORNER_SE, x, y);
                    if (inksw || inks || inkw)
                        m_buf_dngn.add(TILE_WAVE_INK_CORNER_SW, x, y);
                }
            }
        }

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

void DungeonRegion::pack_player(int x, int y, bool submerged)
{
    dolls_data result = player_doll;
    fill_doll_equipment(result);
    pack_doll(result, x, y, submerged, false);
}

void DungeonRegion::pack_doll(const dolls_data &doll, int x, int y, bool submerged, bool ghost)
{
    pack_doll_buf(m_buf_doll, doll, x, y, submerged, ghost);
}


void DungeonRegion::pack_mcache(mcache_entry *entry, int x, int y, bool submerged)
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

void DungeonRegion::pack_foreground(unsigned int bg, unsigned int fg, int x, int y)
{
    unsigned int fg_idx = fg & TILE_FLAG_MASK;

    bool in_water = (bg & TILE_FLAG_WATER) && !(fg & TILE_FLAG_FLYING);

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

void DungeonRegion::pack_cursor(cursor_type type, unsigned int tile)
{
    const coord_def &gc = m_cursor[type];
    if (gc == NO_CURSOR || !on_screen(gc))
        return;

    m_buf_main.add(tile, gc.x - m_cx_to_gx, gc.y - m_cy_to_gy);
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

void DungeonRegion::pack_buffers()
{
    m_buf_dngn.clear();
    m_buf_doll.clear();
    m_buf_main_trans.clear();
    m_buf_main.clear();

    if (m_tileb.empty())
        return;

    int tile = 0;
    for (int y = 0; y < crawl_view.viewsz.y; ++y)
        for (int x = 0; x < crawl_view.viewsz.x; ++x)
        {
            unsigned int bg = m_tileb[tile + 1];
            unsigned int fg = m_tileb[tile];
            unsigned int fg_idx = fg & TILE_FLAG_MASK;

            pack_background(bg, x, y);

            bool in_water = (bg & TILE_FLAG_WATER) && !(fg & TILE_FLAG_FLYING);
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

            pack_foreground(bg, fg, x, y);

            tile += 2;
        }

    pack_cursor(CURSOR_TUTORIAL, TILE_TUTORIAL_CURSOR);
    pack_cursor(CURSOR_MOUSE, you.see_cell(m_cursor[CURSOR_MOUSE]) ? TILE_CURSOR
                                                                   : TILE_CURSOR2);
    pack_cursor(CURSOR_MAP, TILE_CURSOR);

    if (m_cursor[CURSOR_TUTORIAL] != NO_CURSOR
        && on_screen(m_cursor[CURSOR_TUTORIAL]))
    {
        m_buf_main.add(TILE_TUTORIAL_CURSOR, m_cursor[CURSOR_TUTORIAL].x,
                                             m_cursor[CURSOR_TUTORIAL].y);
    }

    for (unsigned int i = 0; i < m_overlays.size(); i++)
    {
        // overlays must be from the main image and must be in LOS.
        if (!crawl_view.in_grid_los(m_overlays[i].gc))
            continue;

        int idx = m_overlays[i].idx;
        if (idx >= TILE_MAIN_MAX)
            continue;

        int x = m_overlays[i].gc.x - m_cx_to_gx;
        int y = m_overlays[i].gc.y - m_cy_to_gy;
        m_buf_main.add(idx, x, y);
    }
}

struct tag_def
{
    tag_def() { text = NULL; left = right = 0; }

    const char* text;
    char left, right;
    char type;
};

// #define DEBUG_TILES_REDRAW
void DungeonRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering DungeonRegion\n");
#endif
    if (m_dirty)
    {
        pack_buffers();
        m_dirty = false;
    }

    set_transform();
    m_buf_dngn.draw(NULL, NULL);
    m_buf_doll.draw();
    m_buf_main_trans.draw();
    m_buf_main.draw(NULL, NULL);

    draw_minibars();

    if (you.berserk())
    {
        ShapeBuffer buff;
        VColour red_film(130, 0, 0, 100);
        buff.add(0, 0, mx, my, red_film);
        buff.draw(NULL, NULL);
    }

    FixedArray<tag_def, ENV_SHOW_DIAMETER, ENV_SHOW_DIAMETER> tag_show;

    int total_tags = 0;

    for (int t = TAG_MAX - 1; t >= 0; t--)
    {
        for (unsigned int i = 0; i < m_tags[t].size(); i++)
        {
            if (!crawl_view.in_grid_los(m_tags[t][i].gc))
                continue;

            const coord_def ep = grid2show(m_tags[t][i].gc);

            if (tag_show(ep).text)
                continue;

            const char *str = m_tags[t][i].tag.c_str();

            int width    = m_tag_font->string_width(str);
            tag_def &def = tag_show(ep);

            const int buffer = 2;

            def.left  = -width / 2 - buffer;
            def.right =  width / 2 + buffer;
            def.text  = str;
            def.type  = t;

            total_tags++;
        }

        if (total_tags)
            break;
    }

    if (!total_tags)
        return;

    // Draw text tags.
    // TODO enne - be more intelligent about not covering stuff up
    for (int y = 0; y < ENV_SHOW_DIAMETER; y++)
        for (int x = 0; x < ENV_SHOW_DIAMETER; x++)
        {
            coord_def ep(x, y);
            tag_def &def = tag_show(ep);

            if (!def.text)
                continue;

            const coord_def gc = show2grid(ep);
            coord_def pc;
            to_screen_coords(gc, pc);
            // center this coord, which is at the top left of gc's cell
            pc.x += dx / 2;

            const coord_def min_pos(sx, sy);
            const coord_def max_pos(ex, ey);
            m_tag_font->render_string(pc.x, pc.y, def.text,
                                      min_pos, max_pos, WHITE, false);
        }
}

/**
 * Draws miniature health and magic points bars on top of the player tile.
 *
 * Drawing of either is governed by options tile_show_minihealthbar and
 * tile_show_minimagicbar. By default, both are on.
 *
 * Intended behaviour is to display both if either is not full. (Unless
 * the bar is toggled off by options.) --Eino & felirx
 */
void DungeonRegion::draw_minibars()
{
    if (Options.tile_show_minihealthbar && you.hp < you.hp_max
        || Options.tile_show_minimagicbar
           && you.magic_points < you.max_magic_points)
    {
        // Tiles are 32x32 pixels; 1/32 = 0.03125.
        // The bars are two pixels high each.
        const float bar_height = 0.0625;
        float healthbar_offset = 0.875;

        ShapeBuffer buff;

        if (!on_screen(you.pos()))
             return;

        // FIXME: to_screen_coords could be made into two versions: one
        // that gives coords by pixel (the current one), one that gives
        // them by grid.
        coord_def player_on_screen;
        to_screen_coords(you.pos(), player_on_screen);

        static const float tile_width  = wx / mx;
        static const float tile_height = wy / my;

        player_on_screen.x = player_on_screen.x / tile_width;
        player_on_screen.y = player_on_screen.y / tile_height;

        if (Options.tile_show_minimagicbar && you.max_magic_points > 0)
        {
            static const VColour magic(0, 0, 255, 255);      // blue
            static const VColour magic_spent(0, 0, 0, 255);  // black

            const float magic_divider = (float) you.magic_points
                                        / (float) you.max_magic_points;

            buff.add(player_on_screen.x,
                     player_on_screen.y + healthbar_offset + bar_height,
                     player_on_screen.x + magic_divider,
                     player_on_screen.y + 1,
                     magic);
            buff.add(player_on_screen.x + magic_divider,
                     player_on_screen.y + healthbar_offset + bar_height,
                     player_on_screen.x + 1,
                     player_on_screen.y + 1,
                     magic_spent);
        }
        else
            healthbar_offset += bar_height;

        if (Options.tile_show_minihealthbar)
        {
            const float min_hp = std::max(0, you.hp);
            const float health_divider = min_hp / (float) you.hp_max;

            const int hp_percent = (you.hp * 100) / you.hp_max;

            int hp_colour = GREEN;
            for (unsigned int i = 0; i < Options.hp_colour.size(); ++i)
                if (hp_percent <= Options.hp_colour[i].first)
                    hp_colour = Options.hp_colour[i].second;

            static const VColour healthy(   0, 255, 0, 255); // green
            static const VColour damaged( 255, 255, 0, 255); // yellow
            static const VColour wounded( 150,   0, 0, 255); // darkred
            static const VColour hp_spent(255,   0, 0, 255); // red

            buff.add(player_on_screen.x,
                     player_on_screen.y + healthbar_offset,
                     player_on_screen.x + health_divider,
                     player_on_screen.y + healthbar_offset + bar_height,
                     hp_colour == RED    ? wounded :
                     hp_colour == YELLOW ? damaged
                                         : healthy);

            buff.add(player_on_screen.x + health_divider,
                     player_on_screen.y + healthbar_offset,
                     player_on_screen.x + 1,
                     player_on_screen.y + healthbar_offset + bar_height,
                     hp_spent);
        }

        buff.draw(NULL, NULL);
    }
}

void DungeonRegion::clear()
{
    m_tileb.clear();
}

void DungeonRegion::on_resize()
{
    // TODO enne
}

// FIXME: If the player is targeted, the game asks the player to target
// something with the mouse, then targets the player anyway and treats
// mouse click as if it hadn't come during targeting (moves the player
// to the clicked cell, whatever).
static void _add_targeting_commands(const coord_def& pos)
{
    // Force targeting cursor back onto center to start off on a clean
    // slate.
    macro_buf_add_cmd(CMD_TARGET_FIND_YOU);

    const coord_def delta = pos - you.pos();

    command_type cmd;

    if (delta.x < 0)
        cmd = CMD_TARGET_LEFT;
    else
        cmd = CMD_TARGET_RIGHT;

    for (int i = 0; i < std::abs(delta.x); i++)
        macro_buf_add_cmd(cmd);

    if (delta.y < 0)
        cmd = CMD_TARGET_UP;
    else
        cmd = CMD_TARGET_DOWN;

    for (int i = 0; i < std::abs(delta.y); i++)
        macro_buf_add_cmd(cmd);

    macro_buf_add_cmd(CMD_TARGET_MOUSE_MOVE);
}

static const bool _is_appropriate_spell(spell_type spell,
                                        const actor* target)
{
    ASSERT(is_valid_spell(spell));

    const unsigned int flags    = get_spell_flags(spell);
    const bool         targeted = flags & SPFLAG_TARGETING_MASK;

    // We don't handle grid targeted spells yet.
    if (flags & SPFLAG_GRID)
        return (false);

    // Most spells are blocked by transparent walls.
    if (targeted && !you.see_cell_no_trans(target->pos()))
    {
        switch(spell)
        {
        case SPELL_HELLFIRE_BURST:
        case SPELL_SMITING:
        case SPELL_HAUNT:
        case SPELL_FIRE_STORM:
        case SPELL_AIRSTRIKE:
            break;

        default:
            return (false);
        }
    }

    const bool helpful = flags & SPFLAG_HELPFUL;

    if (target->atype() == ACT_PLAYER)
    {
        if (flags & SPFLAG_NOT_SELF)
            return (false);

        return ((flags & (SPFLAG_HELPFUL | SPFLAG_ESCAPE | SPFLAG_RECOVERY))
                || !targeted);
    }

    if (!targeted)
        return (false);

    if (flags & SPFLAG_NEUTRAL)
        return (false);

    bool friendly = target->as_monster()->wont_attack();

    return (friendly == helpful);
}

static const bool _is_appropriate_evokable(const item_def& item,
                                           const actor* target)
{
    if (!item_is_evokable(item, false, true))
        return (false);

    // Only wands for now.
    if (item.base_type != OBJ_WANDS)
        return (false);

    // Aren't yet any wands that can go through transparent walls.
    if (!you.see_cell_no_trans(target->pos()))
        return (false);

    // We don't know what it is, so it *might* be appropriate.
    if (!item_type_known(item))
        return (true);

    // Random effects are always (in)apropriate for all targets.
    if (item.sub_type == WAND_RANDOM_EFFECTS)
        return (true);

    spell_type spell = zap_type_to_spell(item.zap());
    if (spell == SPELL_TELEPORT_OTHER && target->atype() == ACT_PLAYER)
        spell = SPELL_TELEPORT_SELF;

    return (_is_appropriate_spell(spell, target));
}

static const bool _have_appropriate_evokable(const actor* target)
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def &item(you.inv[i]);

        if (!item.is_valid())
            continue;

        if (_is_appropriate_evokable(item, target))
            return (true);
   }

    return (false);
}

static item_def* _get_evokable_item(const actor* target)
{
    std::vector<const item_def*> list;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def &item(you.inv[i]);

        if (!item.is_valid())
            continue;

        if (_is_appropriate_evokable(item, target))
            list.push_back(&item);
    }

    ASSERT(!list.empty());

    InvMenu menu(MF_SINGLESELECT | MF_ANYPRINTABLE
                 | MF_ALLOW_FORMATTING | MF_SELECT_BY_PAGE);
    menu.set_type(MT_ANY);
    menu.set_title("Wand to zap?");
    menu.load_items(list);
    menu.show();
    std::vector<SelItem> sel = menu.get_selitems();

    update_screen();
    redraw_screen();

    if (sel.empty())
        return (NULL);

    return ( const_cast<item_def*>(sel[0].item) );
}

static bool _evoke_item_on_target(actor* target)
{
    item_def* item;
    {
        // Prevent the inventory letter from being recorded twice.
        pause_all_key_recorders pause;

        item = _get_evokable_item(target);
    }

    if (item == NULL)
        return (false);

    if (item->base_type == OBJ_WANDS)
    {
        if (item->plus2 == ZAPCOUNT_EMPTY
            || item_type_known(*item) && item->plus <= 0)
        {
            mpr("That wand is empty.");
            return (false);
        }
    }

    macro_buf_add_cmd(CMD_EVOKE);
    macro_buf_add(index_to_letter(item->link)); // Inventory letter.
    _add_targeting_commands(target->pos());
    return (true);
}

static bool _spell_in_range(spell_type spell, actor* target)
{
    if (!(get_spell_flags(spell) & SPFLAG_TARGETING_MASK))
        return (true);

    int range = calc_spell_range(spell);

    switch(spell)
    {
    case SPELL_EVAPORATE:
    case SPELL_MEPHITIC_CLOUD:
    case SPELL_FIREBALL:
    case SPELL_FREEZING_CLOUD:
    case SPELL_POISONOUS_CLOUD:
        // Increase range by one due to cloud radius.
        range++;
        break;
    default:
        break;
    }

    return (range >= grid_distance(you.pos(), target->pos()));
}

static actor* _spell_target = NULL;

static bool _spell_selector(spell_type spell)
{
    return (_is_appropriate_spell(spell, _spell_target));
}

// TODO: Cast spells which target a particular cell.
static bool _cast_spell_on_target(actor* target)
{
    ASSERT(_spell_target == NULL);
    _spell_target = target;

    int letter;
    {
        // Prevent the spell letter from being recorded twice.
        pause_all_key_recorders pause;

        letter = list_spells(true, false, -1, _spell_selector);
    }

    _spell_target = NULL;

    if (letter == 0)
        return (false);

    const spell_type spell = get_spell_by_letter(letter);

    ASSERT(is_valid_spell(spell));
    ASSERT(_is_appropriate_spell(spell, target));

    if (!_spell_in_range(spell, target))
    {
        mprf("%s is out of range for that spell.",
             target->name(DESC_CAP_THE).c_str());
        return (true);
    }

    if (spell_mana(spell) > you.magic_points)
    {
        mpr( "You don't have enough mana to cast that spell.");
        return (true);
    }

    int item_slot = -1;
    if (spell == SPELL_EVAPORATE)
    {
        const int pot = prompt_invent_item("Throw which potion?", MT_INVLIST,
                                           OBJ_POTIONS);

        if (prompt_failed(pot))
            return (false);
        else if (you.inv[pot].base_type != OBJ_POTIONS)
        {
            mpr("This spell works only on potions!");
            return (false);
        }
        item_slot = you.inv[pot].slot;
    }

    macro_buf_add_cmd(CMD_FORCE_CAST_SPELL);
    macro_buf_add(letter);
    if (item_slot != -1)
        macro_buf_add(item_slot);

    if (get_spell_flags(spell) & SPFLAG_TARGETING_MASK)
        _add_targeting_commands(target->pos());

    return (true);
}

static const bool _have_appropriate_spell(const actor* target)
{
    for (size_t i = 0; i < you.spells.size(); i++)
    {
        spell_type spell = you.spells[i];

        if (!is_valid_spell(spell))
            continue;

        if (_is_appropriate_spell(spell, target))
            return (true);
    }
    return (false);
}

static bool _handle_distant_monster(monsters* mon, MouseEvent &event)
{
    const coord_def gc = mon->pos();
    const bool shift = (event.mod & MOD_SHIFT);
    const bool ctrl  = (event.mod & MOD_CTRL);
    const bool alt   = (shift && ctrl || (event.mod & MOD_ALT));

    // Handle evoking items at monster.
    if (alt && _have_appropriate_evokable(mon))
        return _evoke_item_on_target(mon);

    // Handle firing quivered items.
    if (shift && !ctrl && you.m_quiver->get_fire_item() != -1)
    {
        macro_buf_add_cmd(CMD_FIRE);
        _add_targeting_commands(mon->pos());
        return (true);
    }

    // Handle casting spells at monster.
    if (ctrl && !shift && _have_appropriate_spell(mon))
        return _cast_spell_on_target(mon);

    // Handle weapons of reaching.
    if (!mon->wont_attack() && you.see_cell_no_trans(mon->pos()))
    {
        const item_def* weapon = you.weapon();
        const coord_def delta  = you.pos() - mon->pos();
        const int       x_dist = std::abs(delta.x);
        const int       y_dist = std::abs(delta.y);

        if (weapon && get_weapon_brand(*weapon) == SPWPN_REACHING
            && std::max(x_dist, y_dist) == 2)
        {
            macro_buf_add_cmd(CMD_EVOKE_WIELDED);
            _add_targeting_commands(mon->pos());
            return (true);
        }
    }

    return (false);
}

static bool _handle_zap_player(MouseEvent &event)
{
    const bool shift = (event.mod & MOD_SHIFT);
    const bool ctrl  = (event.mod & MOD_CTRL);
    const bool alt   = (shift && ctrl || (event.mod & MOD_ALT));

    if (alt && _have_appropriate_evokable(&you))
        return _evoke_item_on_target(&you);

    if (ctrl && _have_appropriate_spell(&you))
        return _cast_spell_on_target(&you);

    return (false);
}

int DungeonRegion::handle_mouse(MouseEvent &event)
{
    tiles.clear_text_tags(TAG_CELL_DESC);

    if (!inside(event.px, event.py))
        return 0;

    if (mouse_control::current_mode() == MOUSE_MODE_NORMAL
        && event.event == MouseEvent::PRESS
        && event.button == MouseEvent::LEFT)
    {
        you.last_clicked_grid = m_cursor[CURSOR_MOUSE];
        return CK_MOUSE_CLICK;
    }

    if (mouse_control::current_mode() == MOUSE_MODE_NORMAL
        || mouse_control::current_mode() == MOUSE_MODE_MACRO
        || mouse_control::current_mode() == MOUSE_MODE_MORE)
    {
        return 0;
    }

    int cx;
    int cy;

    bool on_map = mouse_pos(event.px, event.py, cx, cy);

    const coord_def gc(cx + m_cx_to_gx, cy + m_cy_to_gy);
    tiles.place_cursor(CURSOR_MOUSE, gc);

    if (event.event == MouseEvent::MOVE)
    {
        std::string desc = get_terse_square_desc(gc);
        // Suppress floor description
        if (desc == "floor")
            desc = "";

        if (you.see_cell(gc))
        {
            const int cloudidx = env.cgrid(gc);
            if (cloudidx != EMPTY_CLOUD)
            {
                std::string terrain_desc = desc;
                desc = cloud_name(cloudidx);

                if (!terrain_desc.empty())
                    desc += "\n" + terrain_desc;
            }
        }

        if (!desc.empty())
            tiles.add_text_tag(TAG_CELL_DESC, desc, gc);
    }

    if (!on_map)
        return 0;

    if (mouse_control::current_mode() == MOUSE_MODE_TARGET
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_PATH
        || mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR)
    {
        if (event.event == MouseEvent::MOVE)
        {
            return CK_MOUSE_MOVE;
        }
        else if (event.event == MouseEvent::PRESS
                 && event.button == MouseEvent::LEFT && on_screen(gc))
        {
            you.last_clicked_grid = m_cursor[CURSOR_MOUSE];
            return CK_MOUSE_CLICK;
        }

        return 0;
    }

    if (event.event != MouseEvent::PRESS)
        return 0;

    you.last_clicked_grid = m_cursor[CURSOR_MOUSE];

    if (you.pos() == gc)
    {
        switch (event.button)
        {
        case MouseEvent::LEFT:
        {
            if ((event.mod & (MOD_CTRL | MOD_ALT)))
            {
                if (_handle_zap_player(event))
                    return 0;
            }

            if (!(event.mod & MOD_SHIFT))
                return command_to_key(CMD_PICKUP);

            const dungeon_feature_type feat = grd(gc);
            switch (feat_stair_direction(feat))
            {
            case CMD_GO_DOWNSTAIRS:
            case CMD_GO_UPSTAIRS:
                return command_to_key(feat_stair_direction(feat));
            default:
                if (feat_is_altar(feat)
                    && player_can_join_god(feat_altar_god(feat)))
                {
                    return command_to_key(CMD_PRAY);
                }
                return 0;
            }
        }
        case MouseEvent::RIGHT:
            if (!(event.mod & MOD_SHIFT))
                return command_to_key(CMD_RESISTS_SCREEN); // Character overview.
            if (you.religion != GOD_NO_GOD)
                return command_to_key(CMD_DISPLAY_RELIGION); // Religion screen.

            // fall through...
        default:
            return 0;
        }

    }
    // else not on player...
    if (event.button == MouseEvent::RIGHT)
    {
        full_describe_square(gc);
        return CK_MOUSE_CMD;
    }

    if (event.button != MouseEvent::LEFT)
        return 0;

    monsters* mon = monster_at(gc);
    if (mon && you.can_see(mon))
    {
        if (_handle_distant_monster(mon, event))
            return (CK_MOUSE_CMD);
    }

    if ((event.mod & MOD_CTRL) && adjacent(you.pos(), gc))
        return click_travel(gc, event.mod & MOD_CTRL);

    // Don't move if we've tried to fire/cast/evoke when there's nothing
    // available.
    if (event.mod & (MOD_SHIFT | MOD_CTRL | MOD_ALT))
        return (CK_MOUSE_CMD);

    return click_travel(gc, event.mod & MOD_CTRL);
}

void DungeonRegion::to_screen_coords(const coord_def &gc, coord_def &pc) const
{
    int cx = gc.x - m_cx_to_gx;
    int cy = gc.y - m_cy_to_gy;

    pc.x = sx + ox + cx * dx;
    pc.y = sy + oy + cy * dy;
}

bool DungeonRegion::on_screen(const coord_def &gc) const
{
    int x = gc.x - m_cx_to_gx;
    int y = gc.y - m_cy_to_gy;

    return (x >= 0 && x < mx && y >= 0 && y < my);
}

// Returns the index into m_tileb for the foreground tile.
// This value may not be valid.  Check on_screen() first.
// Add one to the return value to get the background tile idx.
int DungeonRegion::get_buffer_index(const coord_def &gc)
{
    int x = gc.x - m_cx_to_gx;
    int y = gc.y - m_cy_to_gy;
    return 2 * (x + y * mx);
}

void DungeonRegion::place_cursor(cursor_type type, const coord_def &gc)
{
    coord_def result = gc;

    // If we're only looking for a direction, put the mouse
    // cursor next to the player to let them know that their
    // spell/wand will only go one square.
    if (mouse_control::current_mode() == MOUSE_MODE_TARGET_DIR
        && type == CURSOR_MOUSE && gc != NO_CURSOR)
    {
        coord_def delta = gc - you.pos();

        int ax = abs(delta.x);
        int ay = abs(delta.y);

        result = you.pos();
        if (1000 * ay < 414 * ax)
            result += (delta.x > 0) ? coord_def(1, 0) : coord_def(-1, 0);
        else if (1000 * ax < 414 * ay)
            result += (delta.y > 0) ? coord_def(0, 1) : coord_def(0, -1);
        else if (delta.x > 0)
            result += (delta.y > 0) ? coord_def(1, 1) : coord_def(1, -1);
        else if (delta.x < 0)
            result += (delta.y > 0) ? coord_def(-1, 1) : coord_def(-1, -1);
    }

    if (m_cursor[type] != result)
    {
        m_dirty = true;
        m_cursor[type] = result;
        if (type == CURSOR_MOUSE)
            you.last_clicked_grid = coord_def();
    }
}

bool DungeonRegion::update_tip_text(std::string& tip)
{
    // TODO enne - it would be really nice to use the tutorial
    // descriptions here for features, monsters, etc...
    // Unfortunately, that would require quite a bit of rewriting
    // and some parsing of formatting to get that to work.

    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return (false);

    if (m_cursor[CURSOR_MOUSE] == NO_CURSOR)
        return (false);
    if (!map_bounds(m_cursor[CURSOR_MOUSE]))
        return (false);

    const bool have_reach = you.weapon()
        && get_weapon_brand(*(you.weapon())) == SPWPN_REACHING;
    const int  attack_dist = have_reach ? 2 : 1;

    std::vector<command_type> cmd;
    if (m_cursor[CURSOR_MOUSE] == you.pos())
    {
        tip = you.your_name;
        tip += " (";
        tip += get_species_abbrev(you.species);
        tip += get_job_abbrev(you.char_class);
        tip += ")";

        if (you.visible_igrd(m_cursor[CURSOR_MOUSE]) != NON_ITEM)
        {
            tip += "\n[L-Click] Pick up items (%)";
            cmd.push_back(CMD_PICKUP);
        }

        const dungeon_feature_type feat = grd(m_cursor[CURSOR_MOUSE]);
        const command_type dir = feat_stair_direction(feat);
        if (dir != CMD_NO_CMD)
        {
            tip += "\n[Shift-L-Click] ";
            if (feat == DNGN_ENTER_SHOP)
                tip += "enter shop";
            else if (feat_is_gate(feat))
                tip += "enter gate";
            else
                tip += "use stairs";

            tip += " (%)";
            cmd.push_back(dir);
        }
        else if (feat_is_altar(feat) && player_can_join_god(feat_altar_god(feat)))
        {
            tip += "\n[Shift-L-Click] pray on altar (%)";
            cmd.push_back(CMD_PRAY);
        }

        // Character overview.
        tip += "\n[R-Click] Overview (%)";
        cmd.push_back(CMD_RESISTS_SCREEN);

        // Religion.
        if (you.religion != GOD_NO_GOD)
        {
            tip += "\n[Shift-R-Click] Religion (%)";
            cmd.push_back(CMD_DISPLAY_RELIGION);
        }
    }
    else if (abs(m_cursor[CURSOR_MOUSE].x - you.pos().x) <= attack_dist
             && abs(m_cursor[CURSOR_MOUSE].y - you.pos().y) <= attack_dist)
    {
        tip = "";

        if (!cell_is_solid(m_cursor[CURSOR_MOUSE]))
        {
            const monsters *mon = monster_at(m_cursor[CURSOR_MOUSE]);
            if (!mon || mon->friendly() || !mon->visible_to(&you))
                tip = "[L-Click] Move\n";
            else if (mon)
            {
                tip = mon->name(DESC_CAP_A);
                tip += "\n[L-Click] Attack\n";
            }
        }
    }
    else
    {
        if (i_feel_safe() && !cell_is_solid(m_cursor[CURSOR_MOUSE]))
            tip = "[L-Click] Travel\n";
    }

    if (m_cursor[CURSOR_MOUSE] != you.pos())
    {
        const monsters* mon = monster_at(m_cursor[CURSOR_MOUSE]);
        if (mon && you.can_see(mon))
        {
            if (you.see_cell_no_trans(mon->pos())
                && you.m_quiver->get_fire_item() != -1)
            {
                tip += "[Shift-L-Click] Fire (%)\n";
                cmd.push_back(CMD_FIRE);
            }
        }
    }

    const actor* target = actor_at(m_cursor[CURSOR_MOUSE]);
    if (target && you.can_see(target))
    {
        std::string str = "";

        if (_have_appropriate_spell(target))
        {
            str += "[Ctrl-L-Click] Cast spell (%)\n";
            cmd.push_back(CMD_CAST_SPELL);
        }

        if (_have_appropriate_evokable(target))
        {
            std::string key = "Alt";
#ifdef UNIX
            // On Unix systems the Alt key is already hogged by
            // the application window, at least when we're not
            // in fullscreen mode, so we use Ctrl-Shift instead.
            if (!tiles.is_fullscreen())
                key = "Ctrl-Shift";
#endif
            str += "[" + key + "-L-Click] Zap wand (%)\n";
            cmd.push_back(CMD_EVOKE);
        }

        if (!str.empty())
        {
            if (m_cursor[CURSOR_MOUSE] == you.pos())
                tip += "\n";

            tip += str;
        }
    }

    if (m_cursor[CURSOR_MOUSE] != you.pos())
        tip += "[R-Click] Describe";

    if (!target && adjacent(m_cursor[CURSOR_MOUSE], you.pos()))
    {
        trap_def *trap = find_trap(m_cursor[CURSOR_MOUSE]);
        if (trap && trap->is_known()
            && trap->category() == DNGN_TRAP_MECHANICAL)
        {
            tip += "\n[Ctrl-L-Click] Disarm";
        }
        else if (grd(m_cursor[CURSOR_MOUSE]) == DNGN_OPEN_DOOR)
            tip += "\n[Ctrl-L-Click] Close door (C)";
        else if (feat_is_closed_door(grd(m_cursor[CURSOR_MOUSE])))
            tip += "\n[Ctrl-L-Click] Open door (O)";
    }

    insert_commands(tip, cmd, false);

    return (true);
}

bool DungeonRegion::update_alt_text(std::string &alt)
{
    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return (false);

    const coord_def &gc = m_cursor[CURSOR_MOUSE];

    if (gc == NO_CURSOR)
        return (false);
    if (!map_bounds(gc))
        return (false);
    if (!is_terrain_seen(gc))
        return (false);
    if (you.last_clicked_grid == gc)
        return (false);

    describe_info inf;
    if (you.see_cell(gc))
    {
        get_square_desc(gc, inf, true, false);
        const int cloudidx = env.cgrid(gc);
        if (cloudidx != EMPTY_CLOUD)
        {
            inf.prefix = "There is a cloud of " + cloud_name(cloudidx)
                         + " here.\n\n";
        }
    }
    else if (grid_appearance(gc) != DNGN_FLOOR
             && !feat_is_wall(grid_appearance(gc)))
    {
        get_feature_desc(gc, inf);
    }
    else
    {
        // For plain floor, output the stash description.
        const std::string stash = get_stash_desc(gc);
        if (!stash.empty())
            inf.body << "\n" << stash;
    }

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);

    proc.get_string(alt);

    // Suppress floor description
    if (alt == "Floor.")
    {
        alt.clear();
        return (false);
    }
    return (true);
}

void DungeonRegion::clear_text_tags(text_tag_type type)
{
    m_tags[type].clear();
}

void DungeonRegion::add_text_tag(text_tag_type type, const std::string &tag,
                                 const coord_def &gc)
{
    TextTag t;
    t.tag = tag;
    t.gc  = gc;

    m_tags[type].push_back(t);
}

void DungeonRegion::add_overlay(const coord_def &gc, int idx)
{
    tile_overlay over;
    over.gc  = gc;
    over.idx = idx;

    m_overlays.push_back(over);
    m_dirty = true;
}

void DungeonRegion::clear_overlays()
{
    m_overlays.clear();
    m_dirty = true;
}

#endif
