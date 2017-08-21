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
    m_image(im), m_font_entry(entry), m_mouse_idx(-1),
    m_max_columns(1), m_dirty(false), m_font_buf(entry)
{
    ASSERT(m_image);
    ASSERT(m_font_entry);

    dx = 1;
    dy = 1;

    m_entries.resize(128);

    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].set_tex(&m_image->m_textures[i]);
}

void MenuRegion::set_num_columns(int columns)
{
    m_max_columns = max(1, columns);
}

int MenuRegion::mouse_entry(int x, int y)
{
    if (m_dirty)
        place_entries();

    for (unsigned int i = 0; i < m_entries.size(); i++)
    {
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
                                  m_entries[m_mouse_idx].sy,
                                  m_entries[m_mouse_idx].ex+1,
                                  m_entries[m_mouse_idx].ey+1,
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
    _clear_buffers();
    _place_entries(0, 0, mx);
}

void MenuRegion::_place_entries(const int left_offset, const int top_offset,
                                const int menu_width)
{
    m_dirty = false;

    const int heading_indent  = 10;
    const int tile_indent     = 20;
    const int text_indent     = (Options.tile_menu_icons ? 58 : 20);
    const int max_tile_height = (Options.tile_menu_icons ? 32 : 0);
    const int entry_buffer    = 1;
    const VColour selected_colour(50, 50, 10, 255);

    int column = 0;
    if (!Options.tile_menu_icons)
        set_num_columns(1);
    const int max_columns  = min(2, m_max_columns);
    const int column_width = menu_width / max_columns;

    int lines = count_linebreaks(m_more);
    int more_height = (lines + 1) * m_font_entry->char_height();
    m_font_buf.add(m_more, sx + ox, ey - oy - more_height);

    int height = top_offset;
    int end_height = my - more_height;

#ifdef TOUCH_UI
    m_more_region_start = end_height;
#endif

    const int max_entry_height = max((int)m_font_entry->char_height() * 2,
                                     max_tile_height);

    for (MenuRegionEntry &entry : m_entries)
    {
        if (!entry.valid)
        {
            entry.sx = 0;
            entry.sy = 0;
            entry.ex = 0;
            entry.ey = 0;
            continue;
        }

        if (height + max_entry_height > end_height && column <= max_columns)
        {
            height = top_offset;
            column++;
        }

        int text_width  = m_font_entry->string_width(entry.text);
        int text_height = m_font_entry->char_height();

        if (entry.heading)
        {
            entry.sx = heading_indent + column * column_width + left_offset;
            entry.ex = entry.sx + text_width + left_offset;
            entry.sy = height;
            entry.ey = entry.sy + text_height;

            // wrap titles to two lines if they don't fit
            if (Options.tile_menu_icons
                && entry.ex > entry.sx + column_width)
            {
                int w = column_width;
                int h = m_font_entry->char_height() * 2;
                formatted_string split = m_font_entry->split(entry.text, w, h);
                int string_height = m_font_entry->string_height(split);

                entry.ex = w;
                entry.ey = entry.sy + string_height;

                m_font_buf.add(split, entry.sx, entry.sy);
                height += string_height;
            }
            else
            {
                m_font_buf.add(entry.text, entry.sx, entry.sy);
                height += text_height;
            }
        }
        else
        {
            entry.sy = height;
            int entry_start = column * column_width + left_offset;
            int text_sx = text_indent + entry_start;

            int entry_height;

            if (!entry.tiles.empty())
            {
                entry.sx = entry_start + tile_indent;
                entry_height = max(max_tile_height, text_height);

                for (const tile_def &tile : entry.tiles)
                {
                    // NOTE: This is not perfect. Tiles will be drawn
                    // sorted by texture first, e.g. you can never draw
                    // a dungeon tile over a monster tile.
                    TextureID tex  = tile.tex;
                    m_tile_buf[tex].add_unscaled(tile.tile, entry.sx, entry.sy,
                                                 tile.ymax);
                }
            }
            else
            {
                entry.sx = text_sx;
                entry_height = text_height;
            }

            int text_sy = entry.sy;
            text_sy += (entry_height - m_font_entry->char_height()) / 2;
            // Split menu entries that don't fit into a single line into
            // two lines.
            if (Options.tile_menu_icons
                && text_sx + text_width > entry_start + column_width)
            {
                // [enne] - Ugh, hack. Maybe MenuEntry could specify the
                // presence and length of this substring?
                string unfm = entry.text.tostring();
                bool let = (unfm[1] >= 'a' && unfm[1] <= 'z'
                            || unfm[1] >= 'A' && unfm[1] <= 'Z');
                bool plus = (unfm[3] == '-' || unfm[3] == '+');

                formatted_string text;
                if (let && plus && unfm[0] == ' ' && unfm[2] == ' '
                    && unfm[4] == ' ')
                {
                    formatted_string header = entry.text.chop(5);
                    m_font_buf.add(header, text_sx, text_sy);
                    text_sx += m_font_entry->string_width(header);
                    text = entry.text;
                    // remove hotkeys. As Enne said above, this is a monstrosity.
                    for (int k = 0; k < 5; k++)
                        text.del_char();
                }
                else
                    text += entry.text;

                int w = entry_start + column_width - text_sx - tile_indent;
                int h = m_font_entry->char_height() * 2;
                formatted_string split = m_font_entry->split(text, w, h);

                int string_height = m_font_entry->string_height(split);
                if (string_height > entry_height)
                    text_sy = entry.sy;

                m_font_buf.add(split, text_sx, text_sy);

                entry_height = max(entry_height, string_height);
                entry.ex = entry_start + column_width - tile_indent;
            }
            else
            {
                entry.ex = entry_start + column_width - tile_indent;
                m_font_buf.add(entry.text, text_sx, text_sy);
            }

            entry.ey = entry.sy + entry_height;
            height = entry.ey;
        }
        if (entry.selected)
        {
            m_shape_buf.add(entry.sx-1, entry.sy-1,
                            entry.ex+1, entry.ey+1,
                            selected_colour);
        }

        height += entry_buffer;
    }
}

void MenuRegion::render()
{
#ifdef DEBUG_TILES_REDRAW
    cprintf("rendering MenuRegion\n");
#endif
    if (m_dirty)
        place_entries();

    set_transform();
    m_shape_buf.draw();
    m_line_buf.draw();
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].draw();
    m_font_buf.draw();
}

void MenuRegion::_clear_buffers()
{
    m_shape_buf.clear();
    m_line_buf.clear();
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

    m_dirty = true;
}

void MenuRegion::on_resize()
{
    // Probably needed, even though for me nothing went wrong when
    // I commented it out. (jpeg)
    m_dirty = true;
}

int MenuRegion::maxpagesize() const
{
    // TODO enne - this is a conservative guess.
    // It would be better to make menus use a dynamic number of items per page,
    // but it'd require a lot more refactoring of menu.cc to handle that.

    const int lines = count_linebreaks(m_more);
    const int more_height = (lines + 1) * m_font_entry->char_height();

    // Similar to the definition of max_entry_height in place_entries().
    // HACK: Increasing height by 1 to make sure all items actually fit
    //       on the page, though this introduces a few too many empty lines.
    const int div = (Options.tile_menu_icons ? 32
                                             : m_font_entry->char_height() + 1);

    const int pagesize = ((my - more_height) / div) * m_max_columns;

    // Upper limit for inventory menus. (jpeg)
    // Non-inventory menus only have one column and need
    // *really* big screens to cover more than 52 lines.
    if (pagesize > 52)
        return 52;
    return pagesize;
}

void MenuRegion::set_offset(int lines)
{
    oy = (lines - 1) * m_font_entry->char_height() + 4;
    my = wy - oy;
}

void MenuRegion::set_more(const formatted_string &more)
{
    m_more.clear();
    m_more += more;
}

#endif
