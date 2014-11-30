#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-grid.h"

#include "libutil.h"
#include "random.h"
#include "tiledef-icons.h"
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
    return idx == -1;
}

GridRegion::GridRegion(const TileRegionInit &init) :
    TileRegion(init),
    m_flavour(nullptr),
    m_cursor(NO_CURSOR),
    m_last_clicked_item(-1),
    m_grid_page(0),
    m_buf(init.im)
{
}

GridRegion::~GridRegion()
{
    delete[] m_flavour;
    m_flavour = nullptr;
}

void GridRegion::clear()
{
    m_items.clear();
    m_buf.clear();
}

void GridRegion::on_resize()
{
    if (m_flavour)
    {
        delete[] m_flavour;
        m_flavour = nullptr;
    }

    if (mx * my <= 0)
        return;

    m_flavour = new unsigned char[mx * my];
    for (int i = 0; i < mx * my; ++i)
        m_flavour[i] = random2((unsigned char)~0);
}

unsigned int GridRegion::cursor_index() const
{
    ASSERT(m_cursor != NO_CURSOR);
    return m_cursor.x + m_cursor.y * mx + m_grid_page*mx*my - m_grid_page*2;
}

void GridRegion::place_cursor(const coord_def &cursor)
{
    if (m_cursor != NO_CURSOR && cursor_index() < m_items.size())
    {
        m_items[cursor_index()].flag &= ~TILEI_FLAG_CURSOR;
        m_dirty = true;
    }

    if (m_cursor != cursor)
        m_last_clicked_item = -1;

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

    // ... unless we're using the touch UI, in which case: put at bottom
    if (tiles.is_using_small_layout())
        y = wy;

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
        return false;
    }

    const coord_def cursor(cx, cy);
    place_cursor(cursor);

    if (mouse_control::current_mode() != MOUSE_MODE_COMMAND
        && !tiles.get_map_display())
    {
        return false;
    }

    if (event.event != MouseEvent::PRESS)
        return false;

    item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return false;

    return true;
}

int GridRegion::add_quad_char(char c, int x, int y,
                              int ofs_x, int ofs_y)
{
    int num = c - '0';
    ASSERT_RANGE(num, 0, 9 + 1);
    tileidx_t idx = TILEI_NUM0 + num;

    m_buf.add_icons_tile(idx, x, y, ofs_x, ofs_y);
    return tile_icons_info(idx).width;
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
    if (num > 9999)
        num = 9999;

    int offset_x = num > 1999 ? 0 : 3;
    int offset_y = 1;

    int help = num;
    int c1000 = help/1000;
    help -= c1000*1000;

    if (c1000)
        offset_x += add_quad_char('0' + c1000, x, y, offset_x, offset_y);

    int c100 = help/100;
    help -= c100*100;

    if (c100 || c1000)
        offset_x += add_quad_char('0' + c100, x, y, offset_x, offset_y);

    int c10 = help/10;
    if (c10 || c100 || c1000)
        offset_x += add_quad_char('0' + c10, x, y, offset_x, offset_y);

    int c1 = help % 10;
    add_quad_char('0' + c1, x, y, offset_x, offset_y);
}

#endif
