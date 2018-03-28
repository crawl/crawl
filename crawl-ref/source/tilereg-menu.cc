#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-menu.h"

#ifdef TOUCH_UI
#include "cio.h"
#endif
#include "menu.h"
#include "options.h"
#include "tilebuf.h"
#include "tilefont.h"

MenuRegion::MenuRegion(ImageManager *im, FontWrapper *entry) :
    num_pages(0), vis_item_first(-1), vis_item_last(-1), m_menu_display(nullptr),
    m_image(im), m_font_entry(entry), m_mouse_idx(-1), m_max_columns(1),
    m_buffer_dirty(false), m_layout_dirty(false), m_font_buf(entry)
{
    ASSERT(m_image);
    ASSERT(m_font_entry);

    dx = 1;
    dy = 1;

    m_entries.resize(128);
    for (auto& e : m_entries)
        e.valid = false;

    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].set_tex(&m_image->m_textures[i]);
}

void MenuRegion::set_num_columns(int columns)
{
    m_max_columns = max(1, columns);
}

int MenuRegion::mouse_entry(int x, int y)
{
    if (!m_entries[0].valid)
        return 0;
    place_entries();

    const int scroll_y = _get_layout_scroll_y();
    y -= scroll_y;

    for (int i = 0; i < (int)m_entries.size(); i++)
    {
        if (i < vis_item_first || i > vis_item_last)
            continue;
        const auto &entry = m_entries[i];
        if (!entry.valid)
            continue;
        if (x >= entry.sx && x <= entry.ex && y >= entry.sy && y <= entry.ey)
            return i;
    }

    return -1;
}

int MenuRegion::handle_mouse(MouseEvent &event)
{
    m_mouse_idx = -1;
    if (!m_entries[0].valid)
        return 0;
    const int scroll_y = _get_layout_scroll_y();

    int x, y;
    if (!mouse_pos(event.px, event.py, x, y))
#ifdef TOUCH_UI
        if ((int)event.py < oy && event.event == MouseEvent::PRESS
           && event.button == MouseEvent::LEFT)
        {
            return CK_TOUCH_DUMMY; // mouse click in title area
        }
        else
#endif
        return 0;

    if (event.event == MouseEvent::MOVE)
    {
        int old_idx = m_mouse_idx;
        m_mouse_idx = mouse_entry(x, y);
        if (old_idx == m_mouse_idx)
            return 0;

        const VColour mouse_colour(160, 160, 160, 255);

        m_line_buf.clear();
        if (!m_entries[m_mouse_idx].heading && m_entries[m_mouse_idx].key)
        {
            m_line_buf.add_square(m_entries[m_mouse_idx].sx-1,
                                  m_entries[m_mouse_idx].sy+scroll_y,
                                  m_entries[m_mouse_idx].ex+1,
                                  m_entries[m_mouse_idx].ey+1+scroll_y,
                                  mouse_colour);
        }

        return 0;
    }

    if (event.event == MouseEvent::PRESS)
    {
        switch (event.button)
        {
        case MouseEvent::LEFT:
        {
            int entry = mouse_entry(x, y);
            if (entry == -1)
#ifdef TOUCH_UI
                // if L-CLICK on --more--, do more action
                if (y > m_more_region_start && m_more.width() > 0)
                    return CK_TOUCH_DUMMY;
                else
#endif
                return 0;

            return m_entries[entry].key;
        }
        default:
            return 0;
        }
    }

    return 0;
}

void MenuRegion::place_entries()
{
    _do_layout(0, 0, mx);
    if (vis_item_first == -1)
        scroll_to_item(0);
    if (m_buffer_dirty)
    {
        _clear_buffers();
        _place_entries();
        m_buffer_dirty = false;
    }
}

static bool _has_hotkey_prefix(const string &s)
{
    // [enne] - Ugh, hack. Maybe MenuEntry could specify the
    // presence and length of this substring?
    bool let = (s[1] >= 'a' && s[1] <= 'z' || s[1] >= 'A' && s[1] <= 'Z');
    bool plus = (s[3] == '-' || s[3] == '+');
    return let && plus && s[0] == ' ' && s[2] == ' ' && s[4] == ' ';
}

void MenuRegion::_place_entries()
{
    const int tile_indent     = 20;
    const int text_indent     = _draw_tiles() ? 58 : 20;
    const VColour selected_colour(50, 50, 10, 255);

    m_font_buf.add(m_title, sx + ox, sy + oy);
    int more_height = (count_linebreaks(m_more)+1) * m_font_entry->char_height();
    m_font_buf.add(m_more, sx + ox, ey - oy - more_height);

    const int scroll_y = _get_layout_scroll_y();

    for (int index = 0; index < (int)m_entries.size(); ++index)
    {
        if (index < vis_item_first) continue;
        if (index > vis_item_last) break;

        MenuRegionEntry &entry = m_entries[index];
        if (!entry.valid)
            continue;
        if (entry.heading)
        {
            const int w = entry.ex - entry.sx, h = entry.ey - entry.sy;
            formatted_string split = m_font_entry->split(entry.text, w, h);
            // +5 and +8 give: top margin of 5px, line-text sep of 3px, bottom margin of 2px
            if (index < (int)m_entries.size()-1 && m_entries[index+1].valid
                    && !m_entries[index+1].heading)
            {
                m_div_line_buf.add(entry.sx, entry.sy+scroll_y+5,
                        mx-entry.sx, entry.sy+scroll_y+5, VColour(0, 64, 255, 70));
            }
            m_font_buf.add(split, entry.sx, entry.sy+scroll_y+8);
        }
        else
        {
            const int entry_height = entry.ey - entry.sy;

            const int ty = entry.sy + max(entry_height-32, 0)/2;
            for (const tile_def &tile : entry.tiles)
            {
                // NOTE: This is not perfect. Tiles will be drawn
                // sorted by texture first, e.g. you can never draw
                // a dungeon tile over a monster tile.
                TextureID tex  = tile.tex;
                m_tile_buf[tex].add(tile.tile, entry.sx, ty+scroll_y, 0, 0, false, tile.ymax, 1, 1);
            }

            int text_sx = entry.sx + text_indent - tile_indent, text_sy = entry.sy;
            text_sy += (entry_height - m_font_entry->char_height()) / 2;

            // Split off and render any hotkey prefix first
            formatted_string text;
            if (_has_hotkey_prefix(entry.text.tostring()))
            {
                formatted_string header = entry.text.chop(5);
                m_font_buf.add(header, text_sx, text_sy+scroll_y);
                text_sx += m_font_entry->string_width(header);
                text = entry.text;
                // remove hotkeys. As Enne said above, this is a monstrosity.
                for (int k = 0; k < 5; k++)
                    text.del_char();
            }
            else
                text += entry.text;

            // Line wrap and render the remaining text
            int w = entry.ex-text_sx;
            int h = m_font_entry->char_height() * 2;
            formatted_string split = m_font_entry->split(text, w, h);
            int string_height = m_font_entry->string_height(split);
            if (string_height == entry_height)
                text_sy = entry.sy;

            m_font_buf.add(split, text_sx, text_sy+scroll_y);
        }
        if (entry.selected)
        {
            m_shape_buf.add(entry.sx-1, entry.sy-1+scroll_y,
                    entry.ex+1, entry.ey+1+scroll_y, selected_colour);
        }
    }
}

void MenuRegion::_do_layout(const int left_offset, const int top_offset,
                                const int menu_width)
{
    if (!m_layout_dirty)
        return;
    m_layout_dirty = false;
    m_buffer_dirty = true;

    const bool draw_tiles = _draw_tiles();
    const int heading_indent  = 10;
    const int tile_indent     = 20;
    const int text_indent     = draw_tiles ? 58 : 20;
    const int max_tile_height = draw_tiles ? 32 : 0;
    const int entry_buffer    = 1;

    if (!draw_tiles)
        set_num_columns(1);
    const int max_columns = min(2, m_max_columns);
    int num_columns = 1;

col_retry:
    int column = -1; // an initial increment makes this 0
    const int column_width = menu_width / num_columns;
    const int text_height = m_font_entry->char_height();
    const int more_height = (count_linebreaks(m_more)+1) * m_font_entry->char_height();

    int row_height = 0;
    int height = top_offset;

    for (int index = 0; index < (int)m_entries.size(); ++index)
    {
        MenuRegionEntry &entry = m_entries[index];
        if (!entry.valid)
        {
            entry.sx = 0;
            entry.sy = 0;
            entry.ex = 0;
            entry.ey = 0;
            continue;
        }

        column = entry.heading ? 0 : (column+1) % num_columns;

        if (column == 0)
        {
            height += row_height + entry_buffer;
            row_height = 0;
        }

        int text_width  = m_font_entry->string_width(entry.text);

        if (entry.heading)
        {
            entry.sx = heading_indent + left_offset;
            entry.ex = entry.sx + text_width + left_offset;
            entry.sy = height;
            // 10px extra used for divider line and padding
            entry.ey = entry.sy + text_height + 10;

            // wrap titles to two lines if they don't fit
            if (draw_tiles && entry.ex > entry.sx + menu_width)
            {
                int w = menu_width;
                int h = m_font_entry->char_height() * 2;
                formatted_string split = m_font_entry->split(entry.text, w, h);
                int string_height = m_font_entry->string_height(split);

                entry.ex = entry.sx + w;
                entry.ey = entry.sy + string_height;
            }

            row_height = max(row_height, entry.ey - entry.sy);
            column = num_columns-1;
        }
        else
        {
            entry.sy = height;
            int entry_start = column * column_width + left_offset;
            int text_sx = text_indent + entry_start;

            entry.sx = entry.tiles.empty() ? text_sx : entry_start + tile_indent;
            // Force all entries to the same height, regardless of whethery
            // they have a tile: this ensures <=52 items / page
            int entry_height = max(max_tile_height, text_height);

            int text_sy = entry.sy;
            text_sy += (entry_height - m_font_entry->char_height()) / 2;
            // Split menu entries that don't fit into a single line into
            // two lines.
            if (draw_tiles && text_sx + text_width > entry_start + column_width - tile_indent)
            {
                formatted_string text;
                if (_has_hotkey_prefix(entry.text.tostring()))
                {
                    formatted_string header = entry.text.chop(5);
                    text_sx += m_font_entry->string_width(header);
                    text = entry.text;
                    // remove hotkeys. As Enne said above, this is a monstrosity.
                    for (int k = 0; k < 5; k++)
                        text.del_char();
                }
                else
                    text += entry.text;

                int w = entry_start + column_width - tile_indent - text_sx;
                int h = m_font_entry->char_height() * 2;
                formatted_string split = m_font_entry->split(text, w, h);
                int string_height = m_font_entry->string_height(split);

                entry_height = max(entry_height, string_height);
            }

            entry.ex = entry_start + column_width - tile_indent;
            entry.ey = entry.sy + entry_height;
            row_height = max(row_height, entry_height);
        }
    }

    // Limit page size to ensure <= 52 items visible
    const int min_row_height = max(max_tile_height, text_height) + entry_buffer;
    const int title_height = (count_linebreaks(m_title)+1) * m_font_entry->char_height();
    m_end_height = min(min_row_height*52/num_columns, my - more_height - title_height);
#ifdef TOUCH_UI
    m_more_region_start = m_end_height;
#endif

    // If there's not enough space to fit everything on a single screen, try
    // again with more columns (if we haven't hit the max no. of columns)
    if (height+row_height > my-more_height-title_height && num_columns < max_columns)
    {
        num_columns++;
        goto col_retry;
    }

    // Give each entry a page number
    int page = 1, page_top = top_offset;
    for (int index = 0; index < (int)m_entries.size(); ++index)
    {
        MenuRegionEntry &entry = m_entries[index];
        if (!entry.valid) continue;

        if (entry.ey > page_top + m_end_height)
        {
            page++;
            page_top = entry.sy;
        }
        entry.page = page;
    }
    num_pages = page;

    // update vis_item_last
    int f = vis_item_first;
    vis_item_first = -1;
    scroll_to_item(f);
}

void MenuRegion::get_visible_items_range(int &first, int &last)
{
    first = vis_item_first;
    last = vis_item_last;
}

void MenuRegion::get_page_info(int &cur, int &num)
{
    cur = m_entries[vis_item_first].page;
    num = num_pages;
}

bool MenuRegion::scroll_to_item(int f)
{
    if (f == vis_item_first)
        return false;

    _do_layout(0, 0, mx);

    const int old_vif = vis_item_first;

    // Handle trying to scroll off the end
    f = min(max(0, f), (int)m_entries.size()-1);
    while (f > 0 && !m_entries[f].valid) f--;

    // Move f to point to the first entry in this row
    while (f > 0 && m_entries[f-1].sy == m_entries[f].sy) f--;

    // Given first visible item = f, scan forward to find the last visible item
    int l;
    for (l = f; l < (int)m_entries.size()
            && m_entries[l].valid
            && m_entries[l].ey - m_entries[f].sy <= m_end_height; l++) {}
    --l;

    // If l == end of menu, move f backwards
    if (l == (int)m_entries.size()-1 || !m_entries[l+1].valid)
    {
        ASSERT(f == 0 || m_entries[f-1].sy < m_entries[f].sy);
        while (f > 0 && m_entries[l].ey - m_entries[f-1].sy <= m_end_height)
            f--;
    }

    vis_item_first = f;
    vis_item_last = l;
    return old_vif != f;
}

bool MenuRegion::scroll_page(int delta)
{
    ASSERT(delta == -1 || delta == 1);
    _do_layout(0, 0, mx);

    if (delta == 1) // Scroll down
        return scroll_to_item(vis_item_last+1);

    int f = vis_item_first;
    while (f > 0 && m_entries[vis_item_first].sy - m_entries[f-1].sy <= m_end_height)
        f--;
    return scroll_to_item(f);
}

bool MenuRegion::scroll_line(int delta)
{
    int cur = vis_item_first, idx = vis_item_first;
    while ((idx+delta >= 0 && idx+delta < (int)m_entries.size()) && m_entries[cur].sy == m_entries[idx].sy)
        idx += delta;
    return scroll_to_item(idx);
}

void MenuRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering MenuRegion\n");
#endif
    place_entries();

    set_transform();
    m_shape_buf.draw();
    m_line_buf.draw();
    m_div_line_buf.draw();
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].draw();
    m_font_buf.draw();
}

void MenuRegion::_clear_buffers()
{
    m_shape_buf.clear();
    m_line_buf.clear();
    m_div_line_buf.clear();
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].clear();
    m_font_buf.clear();
}

void MenuRegion::clear()
{
    _clear_buffers();

    m_more.clear();

    for (MenuRegionEntry &entry : m_entries)
        entry.valid = false;

    m_mouse_idx = -1;
}

void MenuRegion::set_entry(int idx, const string &str, int colour,
                           const MenuEntry *me, bool mark_selected)
{
    if (idx >= (int)m_entries.size())
    {
        int new_size = m_entries.size();
        while (idx >= new_size)
            new_size *= 2;
        m_entries.resize(new_size);

        // Quiet valgrind warning about unitialized memory.
        for (int i = idx + 1; i < new_size; i++)
            m_entries[i].valid = false;
    }

    MenuRegionEntry &e = m_entries[idx];
    e.valid = true;
    e.text.clear();
    e.text.textcolour(colour);
    e.text += formatted_string::parse_string(str);
    e.colour   = colour;
    e.heading  = (me->level == MEL_TITLE || me->level == MEL_SUBTITLE);
    e.selected = mark_selected ? me->selected() : false;
    e.key      = !me->hotkeys.empty() ? me->hotkeys[0] : 0;
    e.sx = e.sy = e.ex = e.ey = 0;
    e.tiles.clear();
    me->get_tiles(e.tiles);
    e.page = 0;

    m_layout_dirty = true;
}

void MenuRegion::set_menu_display(void *m)
{
    if (m_menu_display != m)
        vis_item_first = vis_item_last = -1;
    m_menu_display = m;
}

void MenuRegion::on_resize()
{
    // Probably needed, even though for me nothing went wrong when
    // I commented it out. (jpeg)
    m_layout_dirty = true;
}

void MenuRegion::set_title(const formatted_string &title)
{
    m_title.clear();
    m_title += title;
    m_layout_dirty = true;
}

void MenuRegion::set_more(const formatted_string &more)
{
    m_more.clear();
    m_more += more;
    m_layout_dirty = true;
}

bool MenuRegion::_draw_tiles() const
{
    if (!Options.tile_menu_icons)
        return false;
    for (const auto& entry : m_entries)
        if (entry.valid && !entry.heading && !entry.tiles.empty())
            return true;
    return false;
}

int MenuRegion::_get_layout_scroll_y() const
{
    const int title_height = (count_linebreaks(m_title)+1) * m_font_entry->char_height();
    return title_height - m_entries[vis_item_first].sy;
}

#endif
