/*
 *  File:       tilereg-mon.cc
 */

#include "AppHdr.h"

#ifdef USE_TILE
#include <algorithm>

#include "tilereg-mon.h"

#include "cio.h"
#include "directn.h"
#include "env.h"
#include "libutil.h"
#include "monster.h"
#include "stuff.h"
#include "tiledef-dngn.h"
#include "tiledef-player.h"
#include "tilereg-dgn.h"
#include "viewgeom.h"

MonsterRegion::MonsterRegion(const TileRegionInit &init) : GridRegion(init)
{
}

void MonsterRegion::update()
{
    m_items.clear();
    m_mon_info.clear();
    m_dirty = true;

    const size_t max_mons = mx * my;

    if (max_mons == 0)
        return;

    get_monster_info(m_mon_info);
    std::sort(m_mon_info.begin(), m_mon_info.end(),
              monster_info::less_than_wrapper);

    unsigned int num_mons = std::min(max_mons, m_mon_info.size());
    for (size_t i = 0; i < num_mons; ++i)
    {
        InventoryTile desc;
        desc.idx = i;

        m_items.push_back(desc);
    }
}

int MonsterRegion::handle_mouse(MouseEvent &event)
{
    int cx, cy;
    if (!mouse_pos(event.px, event.py, cx, cy))
    {
        place_cursor(NO_CURSOR);
        return (0);
    }

    const coord_def cursor(cx, cy);
    place_cursor(cursor);

    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return (0);


    unsigned int item_idx = cursor_index();
    const monsters *mon = get_monster(item_idx);
    if (!mon)
        return (0);

    const coord_def &gc = mon->position;
    tiles.place_cursor(CURSOR_MOUSE, gc);

    if (event.event != MouseEvent::PRESS)
        return (0);

    if (event.button == MouseEvent::LEFT)
    {
        you.last_clicked_item = item_idx;
        return (tile_click_cell(gc, event.mod));
    }
    else if (event.button == MouseEvent::RIGHT)
    {
        full_describe_square(gc);
        redraw_screen();
        return (CK_MOUSE_CMD);
    }

    return (0);
}

bool MonsterRegion::update_tip_text(std::string &tip)
{
    if (m_cursor == NO_CURSOR)
        return (false);

    unsigned int item_idx = cursor_index();
    const monsters *mon = get_monster(item_idx);
    if (!mon)
        return (false);

    return (tile_dungeon_tip(mon->position, tip));
}

bool MonsterRegion::update_tab_tip_text(std::string &tip, bool active)
{
    return (false);
}

bool MonsterRegion::update_alt_text(std::string &alt)
{
    if (m_cursor == NO_CURSOR)
        return (false);

    unsigned int item_idx = cursor_index();
    if (you.last_clicked_item >= 0
        && item_idx == (unsigned int) you.last_clicked_item)
    {
        return (false);
    }

    const monsters *mon = get_monster(item_idx);
    if (!mon)
        return (false);

    const coord_def &gc = mon->position;

    describe_info inf;
    if (!you.see_cell(gc))
        return (false);

    get_square_desc(gc, inf, true, false);

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);

    proc.get_string(alt);

    return (true);
}

const monsters *MonsterRegion::get_monster(unsigned int idx) const
{
    if (idx >= m_items.size())
        return (NULL);

    const InventoryTile &item = m_items[idx];
    if (item.idx >= static_cast<int>(m_mon_info.size()))
        return (NULL);

    return (m_mon_info[item.idx].m_mon);
}

void MonsterRegion::pack_buffers()
{
    const int num_floor = tile_dngn_count(env.tile_default.floor);

    unsigned int i = 0;
    for (int y = 0; y < my; y++)
    {
        for (int x = 0; x < mx; x++)
        {
            bool cursor = (i < m_items.size()) ?
                (m_items[i].flag & TILEI_FLAG_CURSOR) : false;

            const monsters *mon = get_monster(i++);
            if (mon)
            {
                const coord_def gc = mon->position;
                const coord_def ep = view2show(grid2view(gc));

                if (crawl_view.in_grid_los(gc))
                {
                    packed_cell cell;
                    cell.fg = env.tile_fg[ep.x][ep.y];
                    cell.bg = env.tile_bg[ep.x][ep.y];
                    cell.flv = env.tile_flv(gc);
                    tile_apply_properties(gc, &cell.fg, &cell.bg);

                    m_buf.add(cell, x, y);

                    if (cursor)
                        m_buf.add_main_tile(TILE_CURSOR, x, y);
                    continue;
                }
            }

            // Fill the rest of the space with out of sight floor tiles.
            int tileidx = env.tile_default.floor + m_flavour[i] % num_floor;
            m_buf.add_dngn_tile(tileidx, x, y);
            m_buf.add_main_tile(TILE_MESH, x, y);
        }
    }
}

void MonsterRegion::draw_tag()
{
    if (m_cursor == NO_CURSOR)
        return;

    int curs_index = cursor_index();
    if (curs_index >= (int)m_items.size())
        return;
    int idx = m_items[curs_index].idx;
    if (idx == -1)
        return;

    const monsters *mon = get_monster(idx);
    if (!mon)
        return;

    std::string desc = mon->name(DESC_CAP_A);
    draw_desc(desc.c_str());
}

void MonsterRegion::activate()
{
}

#endif
