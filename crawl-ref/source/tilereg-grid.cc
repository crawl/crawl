/*
 *  File:       tilereg-grid.cc
 *
 *  Created by: ennewalker on Sat Jan 5 01:33:53 2008 UTC
 */

#include "AppHdr.h"

#ifdef USE_TILE

#include "tilereg-grid.h"

#include "libutil.h"
#include "tiledef-main.h"
#include "tilefont.h"

InventoryTile::InventoryTile()
{
    tile     = 0;
    idx      = -1;
    quantity = -1;
    key      = 0;
    flag     = 0;
    special  = 0;
}

bool InventoryTile::empty() const
{
    return (idx == -1);
}

GridRegion::GridRegion(const TileRegionInit &init) :
    TileRegion(init),
    m_flavour(NULL),
    m_cursor(NO_CURSOR),
    m_buf(init.im)
{
}

GridRegion::~GridRegion()
{
    delete[] m_flavour;
    m_flavour = NULL;
}

void GridRegion::clear()
{
    m_items.clear();
    m_buf.clear();
}

void GridRegion::on_resize()
{
    delete[] m_flavour;
    if (mx * my <= 0)
        return;

    m_flavour = new unsigned char[mx * my];
    for (int i = 0; i < mx * my; ++i)
        m_flavour[i] = random2((unsigned char)~0);
}

unsigned int GridRegion::cursor_index() const
{
    ASSERT(m_cursor != NO_CURSOR);
    return (m_cursor.x + m_cursor.y * mx);
}

void GridRegion::place_cursor(const coord_def &cursor)
{
    if (m_cursor != NO_CURSOR && cursor_index() < m_items.size())
    {
        m_items[cursor_index()].flag &= ~TILEI_FLAG_CURSOR;
        m_dirty = true;
    }

    if (m_cursor != cursor)
        you.last_clicked_item = -1;

    m_cursor = cursor;

    if (m_cursor == NO_CURSOR || cursor_index() >= m_items.size())
        return;

    // Add cursor to new location
    m_items[cursor_index()].flag |= TILEI_FLAG_CURSOR;
    m_dirty = true;
}

void GridRegion::draw_desc(const char *desc)
{
    ASSERT(m_tag_font);
    ASSERT(desc);

    // Always draw the description in the inventory header. (jpeg)
    int x = sx + ox + dx / 2;
    int y = sy + oy;

    const coord_def min_pos(sx, sy - dy);
    const coord_def max_pos(ex, ey);

    m_tag_font->render_string(x, y, desc, min_pos, max_pos, WHITE, false, 200);
}

bool GridRegion::place_cursor(MouseEvent &event, unsigned int &item_idx)
{
    int cx, cy;
    if (!mouse_pos(event.px, event.py, cx, cy))
    {
        place_cursor(NO_CURSOR);
        return (false);
    }

    const coord_def cursor(cx, cy);
    place_cursor(cursor);

    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND)
        return (false);

    if (event.event != MouseEvent::PRESS)
        return (false);

    item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return (false);

    return (true);
}

void GridRegion::add_quad_char(char c, int x, int y, int ofs_x, int ofs_y)
{
    int num = c - '0';
    ASSERT(num >= 0 && num <= 9);
    int idx = TILE_NUM0 + num;

    m_buf.add_main_tile(idx, x, y, ofs_x, ofs_y);
}

void GridRegion::render()
{
    if (m_dirty)
    {
        m_buf.clear();

        // Ensure the cursor has been placed.
        place_cursor(m_cursor);

        pack_buffers();
        m_dirty = false;
    }

#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering GridRegion\n");
#endif
    set_transform();
    m_buf.draw();

    draw_tag();
}

void GridRegion::draw_number(int x, int y, int num)
{
    // If you have that many, who cares.
    if (num > 999)
        num = 999;

    const int offset_amount = TILE_X/4;
    int offset_x = 3;
    int offset_y = 1;

    int help = num;
    int c100 = help/100;
    help -= c100*100;

    if (c100)
    {
        add_quad_char('0' + c100, x, y, offset_x, offset_y);
        offset_x += offset_amount;
    }

    int c10 = help/10;
    if (c10 || c100)
    {
        add_quad_char('0' + c10, x, y, offset_x, offset_y);
        offset_x += offset_amount;
    }

    int c1 = help % 10;
    add_quad_char('0' + c1, x, y, offset_x, offset_y);
}

#endif
