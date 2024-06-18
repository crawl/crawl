#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-mon.h"

#include <algorithm>

#include "cio.h"
#include "directn.h"
#include "env.h"
#include "tile-env.h"
#include "libutil.h"
#include "monster.h"
#include "output.h"
#include "describe.h"
#include "tile-inventory-flags.h"
#include "rltiles/tiledef-dngn.h"
#include "rltiles/tiledef-icons.h"
#include "rltiles/tiledef-player.h"
#include "tilepick.h"
#include "tilereg-dgn.h"
#include "tiles-build-specific.h"
#include "tileview.h"
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
    sort(m_mon_info.begin(), m_mon_info.end(),
              monster_info::less_than_wrapper);

    unsigned int num_mons = min(max_mons, m_mon_info.size());
    for (size_t i = 0; i < num_mons; ++i)
    {
        InventoryTile desc;
        desc.idx = i;

        m_items.push_back(desc);
    }
}

bool MonsterRegion::_update_cursor(wm_mouse_event &event)
{
    int cx, cy;
    if (!mouse_pos(event.px, event.py, cx, cy))
    {
        place_cursor(NO_CURSOR);
        return false;
    }

    const coord_def cursor(cx, cy);
    place_cursor(cursor);
    return true;
}

int MonsterRegion::handle_mouse(wm_mouse_event &event)
{
    if (!_update_cursor(event))
        return 0;

    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return 0;

    unsigned int item_idx = cursor_index();
    const monster_info* mon = get_monster(item_idx);
    if (!mon)
        return 0;

    const coord_def &gc = mon->pos;
    tiles.place_cursor(CURSOR_MOUSE, gc);

    if (event.event != wm_mouse_event::PRESS)
        return 0;

    if (event.button == wm_mouse_event::LEFT)
    {
        m_last_clicked_item = item_idx;
        return tile_click_cell(gc, event.mod);
    }
    else if (event.button == wm_mouse_event::RIGHT)
    {
        // Does it really make sense to describe the square including the items
        // on it when clicking the monster panel?
        const command_type cmd = (command_type)(CMD_DESCRIBE_SQUARE_MIN +
            (gc.y - Y_BOUND_1) * X_WIDTH + (gc.x - X_BOUND_1));
        ASSERT(cmd >= CMD_DESCRIBE_SQUARE_MIN
            && cmd <= CMD_DESCRIBE_SQUARE_MAX);
        return encode_command_as_key(cmd);
    }

    return 0;
}

bool MonsterRegion::handle_mouse_for_targeting(wm_mouse_event &event)
{
    if (!_update_cursor(event))
        return false;

    unsigned int item_idx = cursor_index();
    const monster_info* mon = get_monster(item_idx);
    if (!mon)
        return false;

    const coord_def &gc = mon->pos;
    targeting_mouse_move(gc);
    if (event.event == wm_mouse_event::PRESS
        && event.button == wm_mouse_event::LEFT)
    {
        targeting_mouse_select(gc);
    }
    return false;
}

bool MonsterRegion::update_tip_text(string &tip)
{
    if (m_cursor == NO_CURSOR)
        return false;

    unsigned int item_idx = cursor_index();
    const monster_info* mon = get_monster(item_idx);
    if (!mon)
        return false;

    return tile_dungeon_tip(mon->pos, tip);
}

bool MonsterRegion::update_tab_tip_text(string &/*tip*/, bool /*active*/)
{
    return false;
}

bool MonsterRegion::update_alt_text(string &alt)
{
    if (m_cursor == NO_CURSOR)
        return false;

    unsigned int item_idx = cursor_index();
    if (m_last_clicked_item >= 0
        && item_idx == (unsigned int) m_last_clicked_item)
    {
        return false;
    }

    const monster_info* mon = get_monster(item_idx);
    if (!mon)
        return false;

    const coord_def &gc = mon->pos;

    describe_info inf;
    if (!you.see_cell(gc))
        return false;

    get_square_desc(gc, inf);

    alt = process_description(inf);
    return true;
}

const monster_info* MonsterRegion::get_monster(unsigned int idx) const
{
    if (idx >= m_items.size())
        return nullptr;

    const InventoryTile &item = m_items[idx];
    if (item.idx >= static_cast<int>(m_mon_info.size()))
        return nullptr;

    return &(m_mon_info[item.idx]);
}

void MonsterRegion::pack_buffers()
{
    update();
    const int num_floor = tile_dngn_count(tile_env.default_flavour.floor);

    unsigned int i = 0;
    for (int y = 0; y < my; y++)
    {
        for (int x = 0; x < mx; x++)
        {
            bool cursor = (i < m_items.size()) ?
                (m_items[i].flag & TILEI_FLAG_CURSOR) : false;

            const monster_info* mon = get_monster(i++);
            if (mon)
            {
                const coord_def gc = mon->pos;
                const coord_def ep = grid2show(gc);

                if (crawl_view.in_los_bounds_g(gc))
                {
                    packed_cell cell;
                    cell.fg = tile_env.fg(ep);
                    cell.bg = tile_env.bg(ep);
                    cell.flv = tile_env.flv(gc);
                    cell.icons = tile_env.icons[ep];
                    tile_apply_properties(gc, cell);

                    m_buf.add(cell, x, y);

                    if (cursor)
                        m_buf.add_icons_tile(TILEI_CURSOR, x, y);
                    continue;
                }
            }

            // Fill the rest of the space with out of sight floor tiles.
            if (!tiles.is_using_small_layout())
            {
                int tileidx = tile_env.default_flavour.floor + i % num_floor;
                m_buf.add_dngn_tile(tileidx, x, y);
                m_buf.add_icons_tile(TILEI_MESH, x, y);
            }
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

    const monster_info* mon = get_monster(idx);
    if (!mon)
        return;

    string desc = mon->proper_name(DESC_A);
    draw_desc(desc.c_str());
}

void MonsterRegion::activate()
{
}

#endif
