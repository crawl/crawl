/**
 * @file
 * @brief Menus and associated malarkey.
**/

#include "AppHdr.h"

#include "menu.h"

#include <cctype>
#include <functional>

#include "cio.h"
#include "colour.h"
#include "command.h"
#include "coord.h"
#include "env.h"
#include "hints.h"
#include "invent.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#ifdef USE_TILE
 #include "mon-util.h"
#endif
#include "options.h"
#include "player.h"
#include "player-save-info.h"
#include "state.h"
#include "stringutil.h"
#ifdef USE_TILE
 #include "terrain.h"
#endif
#ifdef USE_TILE_LOCAL
 #include "tilebuf.h"
#endif
#ifdef USE_TILE
 #include "tile-flags.h"
 #include "tile-player-flag-cut.h"
 #include "tiledef-dngn.h"
 #include "tiledef-icons.h"
 #include "tiledef-main.h"
 #include "tiledef-player.h"
#endif
#ifdef USE_TILE_LOCAL
 #include "tilefont.h"
#endif
#ifdef USE_TILE
 #include "tilepick.h"
 #include "tilepick-p.h"
#endif
#ifdef USE_TILE_LOCAL
 #include "tilereg-crt.h"
#endif
#ifdef USE_TILE
 #include "travel.h"
#endif
#include "ui.h"
#include "unicode.h"
#include "unwind.h"
#if defined(USE_TILE_LOCAL) && defined(TOUCH_UI)
#include "windowmanager.h"
#endif

using namespace ui;

class UIMenu : public Widget
{
    friend class UIMenuPopup;
public:
    UIMenu(Menu *menu) : m_menu(menu)

#ifdef USE_TILE_LOCAL
    , m_num_columns(1), m_font_entry(tiles.get_crt_font()), m_text_buf(m_font_entry)
#endif
    {
#ifdef USE_TILE_LOCAL
        const ImageManager *m_image = tiles.get_image_manager();
        for (int i = 0; i < TEX_MAX; i++)
            m_tile_buf[i].set_tex(&m_image->m_textures[i]);
#else
        expand_h = true;
#endif
    };
    ~UIMenu() {};

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
#ifdef USE_TILE_LOCAL
    virtual bool on_event(const wm_event& event) override;
    int get_num_columns() const { return m_num_columns; };
    void set_num_columns(int n) {
        m_num_columns = n;
        _invalidate_sizereq();
        _queue_allocation();
    };
#endif

    void update_item(int index);
    void update_items();

    void get_visible_item_range(int *vis_min, int *vis_max);
    void get_item_region(int index, int *y1, int *y2);

#ifndef USE_TILE_LOCAL
    void set_showable_height(int h)
    {
        m_shown_height = h;
        _invalidate_sizereq();
    }
#endif

protected:
    Menu *m_menu;
    int m_height; // set by do_layout()

#ifdef USE_TILE_LOCAL
    void do_layout(int mw, int num_columns);

    int get_max_viewport_height();
    int m_nat_column_width; // set by do_layout()
    int m_num_columns = 1;

    struct MenuItemInfo {
        int x, y, row, column;
        formatted_string text;
        vector<tile_def> tiles;
        bool heading;
    };
    vector<MenuItemInfo> item_info;
    vector<int> row_heights;

    bool m_mouse_pressed = false;
    int m_mouse_idx = -1, m_mouse_x = -1, m_mouse_y = -1;
    void update_hovered_entry();

    void pack_buffers();

    bool m_draw_tiles;
    FontWrapper *m_font_entry;
    ShapeBuffer m_shape_buf;
    LineBuffer m_line_buf, m_div_line_buf;
    FontBuffer m_text_buf;
    FixedVector<TileBuffer, TEX_MAX> m_tile_buf;

public:
    static constexpr int item_pad = 2;
    static constexpr int pad_right = 10;
#else
    int m_shown_height {0};
#endif
};

void UIMenu::update_items()
{
    _invalidate_sizereq();

#ifdef USE_TILE_LOCAL
    item_info.resize(m_menu->items.size());
#endif
    for (unsigned int i = 0; i < m_menu->items.size(); ++i)
        update_item(i);

#ifdef USE_TILE_LOCAL
    // update m_draw_tiles
    m_draw_tiles = false;
    for (const auto& entry : item_info)
        if (!entry.heading && !entry.tiles.empty())
        {
            m_draw_tiles = Options.tile_menu_icons;
            break;
        }
#endif
}

void UIMenu::get_visible_item_range(int *vis_min, int *vis_max)
{
    const int viewport_height = m_menu->m_ui.scroller->get_region()[3];
    const int scroll = m_menu->m_ui.scroller->get_scroll();

#ifdef USE_TILE_LOCAL
    int v_min = 0, v_max = item_info.size(), i;
    for (i = 0; i < (int)item_info.size(); ++i)
    {
        if (row_heights[item_info[i].row + 1] > scroll)
        {
            v_min = i;
            break;
        }
    }
    for (; i < (int)item_info.size(); ++i)
    {
        if (row_heights[item_info[i].row] >= scroll + viewport_height)
        {
            v_max = i;
            break;
        }
    }
#else
    int v_min = scroll;
    int v_max = scroll + viewport_height;
#endif
    v_max = min(v_max, (int)m_menu->items.size());
    if (vis_min)
        *vis_min = v_min;
    if (vis_max)
        *vis_max = v_max;
}

void UIMenu::get_item_region(int index, int *y1, int *y2)
{
    ASSERT_RANGE(index, 0, (int)m_menu->items.size());
#ifdef USE_TILE_LOCAL
    int row = item_info[index].row;
    if (y1)
        *y1 = row_heights[row];
    if (y2)
        *y2 = row_heights[row+1];
#else
    if (y1)
        *y1 = index;
    if (y2)
        *y2 = index+1;
#endif
}

void UIMenu::update_item(int index)
{
    _invalidate_sizereq();
    _queue_allocation();
#ifdef USE_TILE_LOCAL
    const MenuEntry *me = m_menu->items[index];
    int colour = m_menu->item_colour(me);
    const bool needs_cursor = (m_menu->get_cursor() == index
                               && m_menu->is_set(MF_MULTISELECT));
    string text = me->get_text(needs_cursor);

    item_info.resize(m_menu->items.size());

    auto& entry = item_info[index];
    entry.text.clear();
    entry.text.textcolour(colour);
    entry.text += formatted_string::parse_string(text);
    entry.heading = me->level == MEL_TITLE || me->level == MEL_SUBTITLE;
    entry.tiles.clear();
    me->get_tiles(entry.tiles);
#endif
}

#ifdef USE_TILE_LOCAL
static bool _has_hotkey_prefix(const string &s)
{
    // [enne] - Ugh, hack. Maybe MenuEntry could specify the
    // presence and length of this substring?
    bool let = (s[1] >= 'a' && s[1] <= 'z' || s[1] >= 'A' && s[1] <= 'Z');
    bool plus = (s[3] == '-' || s[3] == '+' || s[3] == '#');
    return let && plus && s[0] == ' ' && s[2] == ' ' && s[4] == ' ';
}

void UIMenu::do_layout(int mw, int num_columns)
{
    const int min_column_width = 400;
    const int max_column_width = mw / num_columns;
    const int text_height = m_font_entry->char_height();

    int column = -1; // an initial increment makes this 0
    int column_width = 0;
    int row_height = 0;
    int height = 0;

    row_heights.clear();
    row_heights.reserve(m_menu->items.size()+1);

    for (size_t i = 0; i < m_menu->items.size(); ++i)
    {
        auto& entry = item_info[i];

        column = entry.heading ? 0 : (column+1) % num_columns;

        if (column == 0)
        {
            row_height += row_height == 0 ? 0 : 2*item_pad;
            height += row_height;
            row_heights.push_back(height);
            row_height = 0;
        }

        const int text_width = m_font_entry->string_width(entry.text);

        entry.y = height;
        entry.row = row_heights.size() - 1;
        entry.column = column;

        if (entry.heading)
        {
            entry.x = 0;
            // extra space here is used for divider line and padding; note that
            // we only want top padding if we're not the first item, since the
            // popup and the more already have padding.
            row_height = text_height + (i == 0 ? 5 : 10);

            // wrap titles to two lines if they don't fit
            if (m_draw_tiles && text_width > mw)
            {
                formatted_string split = m_font_entry->split(entry.text, mw, UINT_MAX);
                row_height = max(row_height, (int)m_font_entry->string_height(split));
            }
            column = num_columns-1;
        }
        else
        {
            const int text_indent = m_draw_tiles ? 38 : 0;

            entry.x = text_indent;
            int text_sx = text_indent;
            int item_height = max(text_height, !entry.tiles.empty() ? 32 : 0);

            // Split menu entries that don't fit into a single line into two lines.
            if (!m_menu->is_set(MF_NO_WRAP_ROWS))
            if ((text_width > max_column_width-entry.x-pad_right))
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

                int w = max_column_width - text_sx - pad_right;
                formatted_string split = m_font_entry->split(text, w, UINT_MAX);
                int string_height = m_font_entry->string_height(split);
                string_height = min(string_height, text_height*2);
                item_height = max(item_height, string_height);
            }

            column_width = max(column_width, text_sx + text_width + pad_right);
            row_height = max(row_height, item_height);
        }
    }
    row_height += row_height == 0 ? 0 : 2*item_pad;
    height += row_height;
    row_heights.push_back(height);
    column_width += 2*item_pad;

    m_height = height;
    m_nat_column_width = max(min_column_width, min(column_width, max_column_width));
}

int UIMenu::get_max_viewport_height()
{
    // Limit page size to ensure <= 52 items visible
    int max_viewport_height = INT_MAX;
    size_t a = 0, b = 0, num_items = 0;
    while (b < item_info.size())
    {
        if (num_items < 52)
            num_items += !item_info[b++].heading;
        else if (num_items == 52)
        {
            int item_h = row_heights[item_info[b].row] - row_heights[item_info[b-1].row];
            int delta = item_h + item_info[b-1].y - item_info[a].y;
            max_viewport_height = min(max_viewport_height, delta);
            do
            {
                num_items -= !item_info[a++].heading;
            }
            while (item_info[a].column != 0);
        }
    }
    return max_viewport_height;
}
#endif

void UIMenu::_render()
{
#ifdef USE_TILE_LOCAL
    GLW_3VF t = {(float)m_region[0], (float)m_region[1], 0}, s = {1, 1, 1};
    glmanager->set_transform(t, s);

    m_shape_buf.draw();
    m_div_line_buf.draw();
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].draw();
    m_text_buf.draw();
    m_line_buf.draw();

    glmanager->reset_transform();
#else

    int vis_min, vis_max;
    get_visible_item_range(&vis_min, &vis_max);
    const int scroll = m_menu->m_ui.scroller->get_scroll();

    for (int i = vis_min; i < vis_max; i++)
    {
        const MenuEntry *me = m_menu->items[i];
        int y = i - vis_min + 1;
        cgotoxy(m_region[0]+1, m_region[1]+scroll+y);
        const int col = m_menu->item_colour(me);
        textcolour(col);
        const bool needs_cursor = (m_menu->get_cursor() == i && m_menu->is_set(MF_MULTISELECT));

        if (m_menu->get_flags() & MF_ALLOW_FORMATTING)
        {
            formatted_string s = formatted_string::parse_string(
                me->get_text(needs_cursor), col);
            s.chop(m_region[2]).display();
        }
        else
        {
            string text = me->get_text(needs_cursor);
            text = chop_string(text, m_region[2]);
            cprintf("%s", text.c_str());
        }
    }
#endif
}

SizeReq UIMenu::_get_preferred_size(Direction dim, int prosp_width)
{
#ifdef USE_TILE_LOCAL
    if (!dim)
    {
        do_layout(INT_MAX, m_num_columns);
        int max_menu_width = min(1400, m_nat_column_width * m_num_columns);
        return {0, max_menu_width};
    }
    else
    {
        do_layout(prosp_width, m_num_columns);
        return {0, m_height};
    }
#else
    if (!dim)
        return {0, 80};
    else
        return {1, max({1, (int)m_menu->items.size(), m_shown_height})};
#endif
}

class UIMenuScroller : public Scroller
{
public:
    UIMenuScroller() : Scroller() {};
    virtual ~UIMenuScroller() {};
    virtual void _allocate_region() override {
        m_child->set_allocation_needed();
        Scroller::_allocate_region();
    };
};

class UIMenuMore : public Text
{
public:
    virtual ~UIMenuMore() {};
    void set_text_immediately(const formatted_string &fs)
    {
        m_text.clear();
        m_text += fs;
        _expose();
        m_wrapped_size = { -1, -1 };
        wrap_text_to_size(m_region[2], m_region[3]);
    };
};

class UIShowHide : public Bin
{
public:
    UIShowHide() : Bin() {};
    virtual ~UIShowHide() {};
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override
    {
        if (!m_visible)
            return { 0, 0 };
        return m_child->get_preferred_size(dim, prosp_width);
    }
    virtual void _allocate_region() override
    {
        if (m_visible)
            m_child->allocate_region(m_region);
    }
    virtual void _render() override
    {
        if (m_visible)
            m_child->render();
    }
    bool get_visible() const { return m_visible; }
    void set_visible(bool visible)
    {
        m_visible = visible;
        _invalidate_sizereq();
    }
protected:
    bool m_visible = false;
};

class UIMenuPopup : public ui::Popup
{
public:
    UIMenuPopup(shared_ptr<Widget> child, Menu *menu) : ui::Popup(child), m_menu(menu) {};
    virtual ~UIMenuPopup() {};

    virtual void _allocate_region() override;

private:
    Menu *m_menu;
};

void UIMenuPopup::_allocate_region()
{
    Popup::_allocate_region();

    int max_height = m_menu->m_ui.popup->get_max_child_size()[1];
    max_height -= m_menu->m_ui.title->get_region()[3];
    max_height -= m_menu->m_ui.title->margin[2];
    int viewport_height = m_menu->m_ui.scroller->get_region()[3];

#ifdef USE_TILE_LOCAL
    int menu_w = m_menu->m_ui.menu->get_region()[2];
    m_menu->m_ui.menu->do_layout(menu_w, 1);
    int m_height = m_menu->m_ui.menu->m_height;

    int more_height = m_menu->m_ui.more_bin->get_region()[3];
    // switch number of columns
    int num_cols = m_menu->m_ui.menu->get_num_columns();
    if (m_menu->m_ui.menu->m_draw_tiles && m_menu->is_set(MF_USE_TWO_COLUMNS)
        && !Options.tile_single_column_menus)
    {
        if ((num_cols == 1 && m_height+more_height > max_height)
         || (num_cols == 2 && m_height+more_height <= max_height))
        {
            m_menu->m_ui.menu->set_num_columns(3 - num_cols);
            throw RestartAllocation();
        }
    }
    m_menu->m_ui.menu->do_layout(menu_w, num_cols);
#endif

#ifndef USE_TILE_LOCAL
    int menu_height = m_menu->m_ui.menu->get_region()[3];

    // change more visibility
    bool can_toggle_more = !m_menu->is_set(MF_ALWAYS_SHOW_MORE)
        && !m_menu->m_ui.more->get_text().ops.empty();
    if (can_toggle_more)
    {
        bool more_visible = m_menu->m_ui.more_bin->get_visible();
        if (more_visible ? menu_height <= max_height : menu_height > max_height)
        {
            m_menu->m_ui.more_bin->set_visible(!more_visible);
            _invalidate_sizereq();
            m_menu->m_ui.more_bin->_queue_allocation();
            throw RestartAllocation();
        }
    }

    if (m_menu->m_keyhelp_more && m_menu->m_ui.more_bin->get_visible())
    {
        int scroll = m_menu->m_ui.scroller->get_scroll();
        int scroll_percent = scroll*100/(menu_height-viewport_height);
        string perc = scroll <= 0 ? "top"
            : scroll_percent >= 100 ? "bot"
            : make_stringf("%2d%%", scroll_percent);

        string scroll_more = m_menu->more.to_colour_string();
        scroll_more = replace_all(scroll_more, "XXX", perc);
        m_menu->m_ui.more->set_text_immediately(formatted_string::parse_string(scroll_more));
    }
#endif

    // adjust maximum height
#ifdef USE_TILE_LOCAL
    const int max_viewport_height = m_menu->m_ui.menu->get_max_viewport_height();
#else
    const int max_viewport_height = 52;
#endif
    m_menu->m_ui.scroller->max_size() = { INT_MAX, max_viewport_height };
    if (max_viewport_height < viewport_height)
    {
        m_menu->m_ui.scroller->_invalidate_sizereq();
        m_menu->m_ui.scroller->_queue_allocation();
        throw RestartAllocation();
    }
}

void UIMenu::_allocate_region()
{
#ifndef USE_TILE_LOCAL
    // XXX: is this needed?
    m_height = m_menu->items.size();
#else
    do_layout(m_region[2], m_num_columns);
    update_hovered_entry();
    pack_buffers();
#endif
}

#ifdef USE_TILE_LOCAL
void UIMenu::update_hovered_entry()
{
    const int x = m_mouse_x - m_region[0],
              y = m_mouse_y - m_region[1];
    int vis_min, vis_max;
    get_visible_item_range(&vis_min, &vis_max);

    for (int i = vis_min; i < vis_max; ++i)
    {
        const auto& entry = item_info[i];
        if (entry.heading)
            continue;
        const int w = m_region[2] / m_num_columns;
        const int entry_x = entry.column * w;
        const int entry_h = row_heights[entry.row+1] - row_heights[entry.row];
        if (x >= entry_x && x < entry_x+w && y >= entry.y && y < entry.y+entry_h)
        {
            wm->set_mouse_cursor(MOUSE_CURSOR_POINTER);
            m_mouse_idx = i;
            return;
        }
    }
    wm->set_mouse_cursor(MOUSE_CURSOR_ARROW);
    m_mouse_idx = -1;
}

bool UIMenu::on_event(const wm_event& event)
{
    if (Widget::on_event(event))
        return true;

    if (event.type != WME_MOUSEMOTION
     && event.type != WME_MOUSEBUTTONDOWN
     && event.type != WME_MOUSEBUTTONUP
     && event.type != WME_MOUSEENTER
     && event.type != WME_MOUSELEAVE)
    {
        return false;
    }

    m_mouse_x = event.mouse_event.px;
    m_mouse_y = event.mouse_event.py;

    if (event.type == WME_MOUSEENTER)
    {
        do_layout(m_region[2], m_num_columns);
        update_hovered_entry();
        pack_buffers();
        _expose();
        return false;
    }

    if (event.type == WME_MOUSELEAVE)
    {
        wm->set_mouse_cursor(MOUSE_CURSOR_ARROW);
        m_mouse_x = -1;
        m_mouse_y = -1;
        m_mouse_pressed = false;
        m_mouse_idx = -1;
        do_layout(m_region[2], m_num_columns);
        pack_buffers();
        _expose();
        return false;
    }

    if (event.type == WME_MOUSEMOTION)
    {
        do_layout(m_region[2], m_num_columns);
        update_hovered_entry();
        pack_buffers();
        _expose();
        return true;
    }

    int key = -1;
    if (event.type == WME_MOUSEBUTTONDOWN
            && event.mouse_event.button == MouseEvent::LEFT)
    {
        m_mouse_pressed = true;
        _queue_allocation();
    }
    else if (event.type == WME_MOUSEBUTTONUP
            && event.mouse_event.button == MouseEvent::LEFT
            && m_mouse_pressed)
    {
        int entry = m_mouse_idx;
        if (entry != -1 && m_menu->items[entry]->hotkeys.size() > 0)
            key = m_menu->items[entry]->hotkeys[0];
        m_mouse_pressed = false;
        _queue_allocation();
    }

    if (key != -1)
    {
        wm_event ev = {0};
        ev.type = WME_KEYDOWN;
        ev.key.keysym.sym = key;
        on_event(ev);
    }

    return true;
}

void UIMenu::pack_buffers()
{
    m_shape_buf.clear();
    m_div_line_buf.clear();
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].clear();
    m_text_buf.clear();
    m_line_buf.clear();

    const VColour selected_colour(50, 50, 10, 255);
    const VColour header_div_colour(64, 64, 64, 200);

    if (!item_info.size())
        return;

    const int col_width = m_region[2] / m_num_columns;

    int vis_min, vis_max;
    get_visible_item_range(&vis_min, &vis_max);

    for (int i = vis_min; i < vis_max; ++i)
    {
        const auto& entry = item_info[i];
        const auto me = m_menu->items[i];
        const int entry_x = entry.column * col_width;
        const int entry_ex = entry_x + col_width;
        const int entry_h = row_heights[entry.row+1] - row_heights[entry.row];

        if (entry.heading)
        {
            formatted_string split = m_font_entry->split(entry.text, m_region[2], entry_h);
            // see corresponding section in do_layout()
            int line_y = entry.y  + (i == 0 ? 0 : 5) + item_pad;
            if (i < (int)item_info.size()-1 && !item_info[i+1].heading)
            {
                m_div_line_buf.add(entry.x, line_y,
                        entry.x+m_num_columns*col_width, line_y, header_div_colour);
            }
            m_text_buf.add(split, entry.x, line_y+3);
        }
        else
        {
            const int ty = entry.y + max(entry_h-32, 0)/2;
            for (const tile_def &tile : entry.tiles)
            {
                // NOTE: This is not perfect. Tiles will be drawn
                // sorted by texture first, e.g. you can never draw
                // a dungeon tile over a monster tile.
                TextureID tex  = tile.tex;
                m_tile_buf[tex].add(tile.tile, entry_x + item_pad, ty, 0, 0, false, tile.ymax, 1, 1);
            }

            const int text_indent = m_draw_tiles ? 38 : 0;
            int text_sx = entry_x + text_indent + item_pad;
            int text_sy = entry.y + (entry_h - m_font_entry->char_height())/2;

            // Split off and render any hotkey prefix first
            formatted_string text;
            if (_has_hotkey_prefix(entry.text.tostring()))
            {
                formatted_string header = entry.text.chop(5);
                m_text_buf.add(header, text_sx, text_sy);
                text_sx += m_font_entry->string_width(header);
                text = entry.text;
                // remove hotkeys. As Enne said above, this is a monstrosity.
                for (int k = 0; k < 5; k++)
                    text.del_char();
            }
            else
                text += entry.text;

            // Line wrap and render the remaining text
            int w = entry_ex-text_sx - pad_right;
            int h = m_font_entry->char_height();
            h *= m_menu->is_set(MF_NO_WRAP_ROWS) ? 1 : 2;
            formatted_string split = m_font_entry->split(text, w, h);
            int string_height = m_font_entry->string_height(split);
            text_sy = entry.y + (entry_h - string_height)/2;

            m_text_buf.add(split, text_sx, text_sy);
        }

        bool hovered = i == m_mouse_idx && !entry.heading && me->hotkeys.size() > 0;

        if (me->selected() && !m_menu->is_set(MF_QUIET_SELECT))
        {
            m_shape_buf.add(entry_x, entry.y,
                    entry_ex, entry.y+entry_h, selected_colour);
        }
        else if (hovered)
        {
            const VColour hover_bg = m_mouse_pressed ?
                VColour(0, 0, 0, 255) : VColour(255, 255, 255, 25);
            m_shape_buf.add(entry_x, entry.y,
                    entry_ex, entry.y+entry_h, hover_bg);
        }
        if (hovered)
        {
            const VColour mouse_colour = m_mouse_pressed ?
                VColour(34, 34, 34, 255) : VColour(255, 255, 255, 51);
            m_line_buf.add_square(entry_x + 1, entry.y + 1,
                    entry_x+col_width, entry.y+entry_h, mouse_colour);
        }
    }
}
#endif

Menu::Menu(int _flags, const string& tagname, KeymapContext kmc)
  : f_selitem(nullptr), f_keyfilter(nullptr),
    action_cycle(CYCLE_NONE), menu_action(ACT_EXAMINE), title(nullptr),
    title2(nullptr), flags(_flags), tag(tagname),
    cur_page(1), items(), sel(),
    select_filter(), highlighter(new MenuHighlighter), num(-1), lastch(0),
    alive(false), last_selected(-1), m_kmc(kmc), m_filter(nullptr)
{
    m_ui.menu = make_shared<UIMenu>(this);
    m_ui.scroller = make_shared<UIMenuScroller>();
    m_ui.title = make_shared<Text>();
    m_ui.more = make_shared<UIMenuMore>();
    m_ui.more_bin = make_shared<UIShowHide>();
    m_ui.vbox = make_shared<Box>(Widget::VERT);
    m_ui.vbox->align_items = Widget::STRETCH;

    m_ui.title->set_margin_for_sdl({0, 0, 10, 0});
    m_ui.more->set_margin_for_sdl({10, 0, 0, 0});

    m_ui.vbox->add_child(m_ui.title);
#ifdef USE_TILE_LOCAL
    m_ui.vbox->add_child(m_ui.scroller);
#else
    auto scroller_wrap = make_shared<Box>(Widget::VERT, Box::Expand::EXPAND_V);
    scroller_wrap->align_items = Widget::STRETCH;
    scroller_wrap->add_child(m_ui.scroller);
    m_ui.vbox->add_child(scroller_wrap);
#endif
    m_ui.vbox->add_child(m_ui.more_bin);
    m_ui.more_bin->set_child(m_ui.more);
    m_ui.scroller->set_child(m_ui.menu);

    set_flags(flags);
    set_more();
}

void Menu::check_add_formatted_line(int firstcol, int nextcol,
                                    string &line, bool check_eol)
{
    if (line.empty())
        return;

    if (check_eol && line.find("\n") == string::npos)
        return;

    vector<string> lines = split_string("\n", line, false, true);
    int size = lines.size();

    // If we have stuff after EOL, leave that in the line variable and
    // don't add an entry for it, unless the caller told us not to
    // check EOL sanity.
    if (check_eol && !ends_with(line, "\n"))
        line = lines[--size];
    else
        line.clear();

    for (int i = 0, col = firstcol; i < size; ++i, col = nextcol)
    {
        string &s(lines[i]);

        trim_string_right(s);

        MenuEntry *me = new MenuEntry(s);
        me->colour = col;
        if (!title)
            set_title(me);
        else
            add_entry(me);
    }

    line.clear();
}

Menu::~Menu()
{
    deleteAll(items);
    delete title;
    if (title2)
        delete title2;
    delete highlighter;
}

void Menu::clear()
{
    deleteAll(items);
    m_ui.menu->_queue_allocation();
    last_selected = -1;
}

void Menu::set_flags(int new_flags, bool use_options)
{
    flags = new_flags;

#ifdef DEBUG
    int sel_flag = flags & (MF_NOSELECT | MF_SINGLESELECT | MF_MULTISELECT);
    ASSERT(sel_flag == MF_NOSELECT || sel_flag == MF_SINGLESELECT || sel_flag == MF_MULTISELECT);
#endif
}

void Menu::set_more(const formatted_string &fs)
{
    m_keyhelp_more = false;
    more = fs;
    update_more();
}

void Menu::set_more()
{
    m_keyhelp_more = true;
    more = formatted_string::parse_string(
        "<lightgrey>[<w>+</w>|<w>></w>|<w>Space</w>]: page down        "
        "[<w>-</w>|<w><<</w>]: page up        "
        "[<w>Esc</w>]: close        [<w>XXX</w>]</lightgrey>"
    );
    update_more();
}

void Menu::set_highlighter(MenuHighlighter *mh)
{
    if (highlighter != mh)
        delete highlighter;
    highlighter = mh;
}

void Menu::set_title(MenuEntry *e, bool first, bool indent)
{
    if (first)
    {
        if (title != e)
            delete title;

        title = e;
        title->level = MEL_TITLE;
    }
    else
    {
        title2 = e;
        title2->level = MEL_TITLE;
    }
    m_indent_title = indent;
    update_title();
}

void Menu::add_entry(MenuEntry *entry)
{
    entry->tag = tag;
    items.push_back(entry);
}

void Menu::reset()
{
    m_ui.scroller->set_scroll(0);
}

vector<MenuEntry *> Menu::show(bool reuse_selections)
{
    cursor_control cs(false);

    if (reuse_selections)
        get_selected(&sel);
    else
        deselect_all(false);

    if (is_set(MF_START_AT_END))
        m_ui.scroller->set_scroll(INT_MAX);

    do_menu();

#ifdef USE_TILE_WEB
    tiles.pop_menu();
#endif

    return sel;
}

void Menu::do_menu()
{
    bool done = false;
    m_ui.popup = make_shared<UIMenuPopup>(m_ui.vbox, this);

    m_ui.menu->on(Widget::slots.event, [this, &done](wm_event ev) {
        if (ev.type != WME_KEYDOWN)
            return false;
        if (m_filter)
        {
            int key = m_filter->putkey(ev.key.keysym.sym);
            if (key != -1)
            {
                delete m_filter;
                m_filter = nullptr;
            }
            update_title();
            return true;
        }
        done = !process_key(ev.key.keysym.sym);
        return true;
    });
#ifdef TOUCH_UI
    auto menu_wrap_click = [this, &done](const wm_event& ev) {
        if (!m_filter && ev.type == WME_MOUSEBUTTONDOWN
                && ev.mouse_event.button == MouseEvent::LEFT)
        {
            done = !process_key(CK_TOUCH_DUMMY);
            return true;
        }
        return false;
    };
    m_ui.title->on(Widget::slots.event, menu_wrap_click);
    m_ui.more_bin->on(Widget::slots.event, menu_wrap_click);
#endif

    update_menu();
    ui::push_layout(m_ui.popup, m_kmc);

#ifdef USE_TILE_WEB
    tiles.push_menu(this);
    _webtiles_title_changed = false;
#endif

    alive = true;
    while (alive && !done && !crawl_state.seen_hups)
    {
#ifdef USE_TILE_WEB
        if (_webtiles_title_changed)
        {
            webtiles_update_title();
            _webtiles_title_changed = false;
        }
#endif
        ui::pump_events();
    }
    alive = false;
    ui::pop_layout();
}

int Menu::get_cursor() const
{
    if (last_selected == -1)
        return -1;

    unsigned int last = last_selected % item_count();
    unsigned int next = (last_selected + 1) % item_count();

    // Items with no hotkeys are unselectable
    while (next != last && (items[next]->hotkeys.empty()
                            || items[next]->level != MEL_ITEM))
    {
        next = (next + 1) % item_count();
    }

    return next;
}

bool Menu::is_set(int flag) const
{
    return (flags & flag) == flag;
}

int Menu::pre_process(int k)
{
    return k;
}

int Menu::post_process(int k)
{
    return k;
}

bool Menu::filter_with_regex(const char *re)
{
    text_pattern tpat(re, true);
    for (unsigned int i = 0; i < items.size(); ++i)
    {
        if (items[i]->level == MEL_ITEM
            && tpat.matches(items[i]->get_text()))
        {
            select_index(i);
            if (flags & MF_SINGLESELECT)
            {
                // Return the first item found.
                get_selected(&sel);
                return false;
            }
        }
    }
    get_selected(&sel);
    return true;
}

bool Menu::title_prompt(char linebuf[], int bufsz, const char* prompt)
{
    bool validline;
#ifdef USE_TILE_WEB
    mouse_control mc(MOUSE_MODE_PROMPT);
    cgotoxy(1,1);
    clear_to_end_of_line();
    textcolour(WHITE);
    cprintf("%s", prompt);
    textcolour(LIGHTGREY);
    line_reader reader(linebuf, bufsz, get_number_of_cols());
    validline = !reader.read_line("");
#else
    ASSERT(!m_filter);
    m_filter = new resumable_line_reader(linebuf, bufsz);
    update_title();
    do
    {
        ui::pump_events();
    }
    while (m_filter && !crawl_state.seen_hups);
    validline = linebuf[0];
#endif

    return validline;
}

bool Menu::process_key(int keyin)
{
    if (items.empty())
    {
        lastch = keyin;
        return false;
    }
#ifdef TOUCH_UI
    else if (action_cycle == CYCLE_TOGGLE && (keyin == '!' || keyin == '?'
             || keyin == CK_TOUCH_DUMMY))
#else
    else if (action_cycle == CYCLE_TOGGLE && (keyin == '!' || keyin == '?'))
#endif
    {
        ASSERT(menu_action != ACT_MISC);
        if (menu_action == ACT_EXECUTE)
            menu_action = ACT_EXAMINE;
        else
            menu_action = ACT_EXECUTE;

        sel.clear();
        update_title();
        return true;
    }
#ifdef TOUCH_UI
    else if (action_cycle == CYCLE_CYCLE && (keyin == '!' || keyin == '?'
             || keyin == CK_TOUCH_DUMMY))
#else
    else if (action_cycle == CYCLE_CYCLE && (keyin == '!' || keyin == '?'))
#endif
    {
        menu_action = (action)((menu_action+1) % ACT_NUM);
        sel.clear();
        update_title();
        return true;
    }

    if (f_keyfilter)
        keyin = (*f_keyfilter)(keyin);
    keyin = pre_process(keyin);

#ifdef USE_TILE_WEB
    const int old_vis_first = get_first_visible();
#endif

    switch (keyin)
    {
    case CK_REDRAW:
        return true;
#ifndef TOUCH_UI
    case 0:
        return true;
#endif
    case CK_MOUSE_B2:
    case CK_MOUSE_CMD:
    CASE_ESCAPE
        sel.clear();
        lastch = keyin;
        return is_set(MF_UNCANCEL) && !crawl_state.seen_hups;
    case ' ': case CK_PGDN: case '>':
    case CK_MOUSE_B1:
    case CK_MOUSE_CLICK:
        if (!page_down() && is_set(MF_WRAP))
            m_ui.scroller->set_scroll(0);
        break;
    case CK_PGUP: case '<': case ';':
        page_up();
        break;
    case CK_UP:
        line_up();
        break;
    case CK_DOWN:
        line_down();
        break;
    case CK_HOME:
        m_ui.scroller->set_scroll(0);
        break;
    case CK_END:
        m_ui.scroller->set_scroll(INT_MAX);
        break;
    case CONTROL('F'):
        if ((flags & MF_ALLOW_FILTER))
        {
            char linebuf[80] = "";

            const bool validline = title_prompt(linebuf, sizeof linebuf,
                                                "Select what? (regex) ");

            return (validline && linebuf[0]) ? filter_with_regex(linebuf) : true;
        }
        break;
    case '.':
        if (last_selected == -1 && is_set(MF_MULTISELECT))
            last_selected = 0;

        if (last_selected != -1)
        {
            const int next = get_cursor();
            if (next != -1)
            {
                InvEntry::set_show_cursor(true);
                select_index(next, num);
                get_selected(&sel);
                update_title();
                if (get_cursor() < next)
                {
                    m_ui.scroller->set_scroll(0);
                    break;
                }
            }

            if (!in_page(last_selected))
                page_down();
        }
        break;

    case '\'':
        if (last_selected == -1 && is_set(MF_MULTISELECT))
            last_selected = 0;
        else
            last_selected = get_cursor();

        if (last_selected != -1)
        {
            InvEntry::set_show_cursor(true);
            const int it_count = item_count();
            if (last_selected < it_count
                && items[last_selected]->level == MEL_ITEM)
            {
                m_ui.menu->update_item(last_selected);
            }

            const int next_cursor = get_cursor();
            if (next_cursor != -1)
            {
                if (next_cursor < last_selected)
                    m_ui.scroller->set_scroll(0);
                else if (!in_page(last_selected))
                    page_down();
                else if (next_cursor < it_count
                         && items[next_cursor]->level == MEL_ITEM)
                {
                    m_ui.menu->update_item(next_cursor);
                }
            }
        }
        break;

    case '_':
        if (!help_key().empty())
            show_specific_help(help_key());
        break;

#ifdef TOUCH_UI
    case CK_TOUCH_DUMMY:  // mouse click in top/bottom region of menu
    case 0:               // do the same as <enter> key
        if (!(flags & MF_MULTISELECT)) // bail out if not a multi-select
            return true;
#endif
    case CK_ENTER:
        if (!(flags & MF_PRESELECTED) || !sel.empty())
            return false;
        // else fall through
    default:
        // Even if we do return early, lastch needs to be set first,
        // as it's sometimes checked when leaving a menu.
        keyin  = post_process(keyin);
        lastch = keyin;

        // If no selection at all is allowed, exit now.
        if (!(flags & (MF_SINGLESELECT | MF_MULTISELECT)))
            return false;

        if (!is_set(MF_NO_SELECT_QTY) && isadigit(keyin))
        {
            if (num > 999)
                num = -1;
            num = (num == -1) ? keyin - '0' :
                                num * 10 + keyin - '0';
        }

        select_items(keyin, num);
        get_selected(&sel);
        if (sel.size() == 1 && (flags & MF_SINGLESELECT))
        {
            if (!on_single_selection)
                return false;
            MenuEntry *item = sel[0];
            if (!on_single_selection(*item))
                return false;
            deselect_all();
            return true;
        }

        update_title();

        if (flags & MF_ANYPRINTABLE
            && (!isadigit(keyin) || is_set(MF_NO_SELECT_QTY)))
        {
            return false;
        }

        break;
    }

    if (last_selected != -1 && get_cursor() == -1)
        last_selected = -1;

    if (!isadigit(keyin))
        num = -1;

#ifdef USE_TILE_WEB
    if (old_vis_first != get_first_visible())
        webtiles_update_scroll_pos();
#endif

    return true;
}

string Menu::get_select_count_string(int count) const
{
    string ret;
    if (f_selitem)
        ret = f_selitem(&sel);
    else
    {
        char buf[100] = "";
        if (count)
        {
            snprintf(buf, sizeof buf, " (%d item%s)", count,
                    (count > 1 ? "s" : ""));
        }
        ret = string(buf);
    }
    return ret + string(max(12-(int)ret.size(), 0), ' ');
}

vector<MenuEntry*> Menu::selected_entries() const
{
    vector<MenuEntry*> selection;
    get_selected(&selection);
    return selection;
}

void Menu::get_selected(vector<MenuEntry*> *selected) const
{
    selected->clear();

    for (MenuEntry *item : items)
        if (item->selected())
            selected->push_back(item);
}

void Menu::deselect_all(bool update_view)
{
    for (int i = 0, count = items.size(); i < count; ++i)
    {
        if (items[i]->level == MEL_ITEM && items[i]->selected())
        {
            items[i]->select(0);
            if (update_view)
            {
                m_ui.menu->update_item(i);
#ifdef USE_TILE_WEB
                webtiles_update_item(i);
#endif
            }
        }
    }
    sel.clear();
}

int Menu::get_first_visible() const
{
    int y = m_ui.scroller->get_scroll();
    for (int i = 0; i < (int)items.size(); i++)
    {
        int item_y2;
        m_ui.menu->get_item_region(i, nullptr, &item_y2);
        if (item_y2 > y)
            return i;
    }
    return items.size();
}

bool Menu::is_hotkey(int i, int key)
{
    bool ishotkey = items[i]->is_hotkey(key);
    return ishotkey && (!is_set(MF_SELECT_BY_PAGE) || in_page(i));
}

void Menu::select_items(int key, int qty)
{
    if (key == ',') // Select all or apply filter if there is one.
        select_index(-1, -2);
    else if (key == '*') // Invert selection.
        select_index(-1, -1);
    else if (key == '-') // Clear selection.
        select_index(-1, 0);
    else
    {
        int first_entry = get_first_visible(), final = items.size();
        bool selected = false;

        // Process all items, in case user hits hotkey for an
        // item not on the current page.

        // We have to use some hackery to handle items that share
        // the same hotkey (as for pickup when there's a stack of
        // >52 items). If there are duplicate hotkeys, the items
        // are usually separated by at least a page, so we should
        // only select the item on the current page. This is why we
        // use two loops, and check to see if we've matched an item
        // by its primary hotkey (hotkeys[0] for multiple-selection
        // menus, any hotkey for single-selection menus), in which
        // case, we stop selecting further items.
        const bool check_preselected = (key == CK_ENTER);
        for (int i = first_entry; i < final; ++i)
        {
            if (check_preselected && items[i]->preselected)
            {
                select_index(i, qty);
                selected = true;
                break;
            }
            else if (is_hotkey(i, key))
            {
                select_index(i, qty);
                if (items[i]->hotkeys[0] == key || is_set(MF_SINGLESELECT))
                {
                    selected = true;
                    break;
                }
            }
        }

        if (!selected)
        {
            for (int i = 0; i < first_entry; ++i)
            {
                if (check_preselected && items[i]->preselected)
                {
                    select_index(i, qty);
                    break;
                }
                else if (is_hotkey(i, key))
                {
                    select_index(i, qty);
                    break;
                }
            }
        }
    }
}

MonsterMenuEntry::MonsterMenuEntry(const string &str, const monster_info* mon,
                                   int hotkey) :
    MenuEntry(str, MEL_ITEM, 1, hotkey)
{
    data = (void*)mon;
    quantity = 1;
}

FeatureMenuEntry::FeatureMenuEntry(const string &str, const coord_def p,
                                   int hotkey) :
    MenuEntry(str, MEL_ITEM, 1, hotkey)
{
    if (in_bounds(p))
        feat = grd(p);
    else
        feat = DNGN_UNSEEN;
    pos      = p;
    quantity = 1;
}

FeatureMenuEntry::FeatureMenuEntry(const string &str,
                                   const dungeon_feature_type f,
                                   int hotkey) :
    MenuEntry(str, MEL_ITEM, 1, hotkey)
{
    pos.reset();
    feat     = f;
    quantity = 1;
}

#ifdef USE_TILE
bool MenuEntry::get_tiles(vector<tile_def>& tileset) const
{
    if (!Options.tile_menu_icons || tiles.empty())
        return false;

    tileset.insert(end(tileset), begin(tiles), end(tiles));
    return true;
}
#else
bool MenuEntry::get_tiles(vector<tile_def>& tileset) const { return false; }
#endif

#ifdef USE_TILE
PlayerMenuEntry::PlayerMenuEntry(const string &str) :
    MenuEntry(str, MEL_ITEM, 1)
{
    quantity = 1;
}

void MenuEntry::add_tile(tile_def tile)
{
    tiles.push_back(tile);
}

bool MonsterMenuEntry::get_tiles(vector<tile_def>& tileset) const
{
    if (!Options.tile_menu_icons)
        return false;

    monster_info* m = (monster_info*)(data);
    if (!m)
        return false;

    MenuEntry::get_tiles(tileset);

    const bool    fake = m->props.exists("fake");
    const coord_def c  = m->pos;
    tileidx_t       ch = TILE_FLOOR_NORMAL;

    if (!fake)
    {
        ch = tileidx_feature(c);
        if (ch == TILE_FLOOR_NORMAL)
            ch = env.tile_flv(c).floor;
        else if (ch == TILE_WALL_NORMAL)
            ch = env.tile_flv(c).wall;
    }

    tileset.emplace_back(ch, get_dngn_tex(ch));

    if (m->attitude == ATT_FRIENDLY)
        tileset.emplace_back(TILE_HALO_FRIENDLY, TEX_FEAT);
    else if (m->attitude == ATT_GOOD_NEUTRAL || m->attitude == ATT_STRICT_NEUTRAL)
        tileset.emplace_back(TILE_HALO_GD_NEUTRAL, TEX_FEAT);
    else if (m->neutral())
        tileset.emplace_back(TILE_HALO_NEUTRAL, TEX_FEAT);

    if (m->type == MONS_DANCING_WEAPON)
    {
        // For fake dancing weapons, just use a generic long sword, since
        // fake monsters won't have a real item equipped.
        item_def item;
        if (fake)
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_LONG_SWORD;
            item.quantity  = 1;
        }
        else
            item = *m->inv[MSLOT_WEAPON];

        tileset.emplace_back(tileidx_item(item), TEX_DEFAULT);
        tileset.emplace_back(TILEI_ANIMATED_WEAPON, TEX_ICONS);
    }
    else if (mons_is_draconian(m->type))
    {
        tileset.emplace_back(tileidx_draco_base(*m), TEX_PLAYER);
        const tileidx_t job = tileidx_draco_job(*m);
        if (job)
            tileset.emplace_back(job, TEX_PLAYER);
    }
    else if (mons_is_demonspawn(m->type))
    {
        tileset.emplace_back(tileidx_demonspawn_base(*m), TEX_PLAYER);
        const tileidx_t job = tileidx_demonspawn_job(*m);
        if (job)
            tileset.emplace_back(job, TEX_PLAYER);
    }
    else
    {
        tileidx_t idx = tileidx_monster(*m) & TILE_FLAG_MASK;
        tileset.emplace_back(idx, TEX_PLAYER);
    }

    // A fake monster might not have its ghost member set up properly.
    if (!fake && m->ground_level())
    {
        if (ch == TILE_DNGN_LAVA)
            tileset.emplace_back(TILEI_MASK_LAVA, TEX_ICONS);
        else if (ch == TILE_DNGN_SHALLOW_WATER)
            tileset.emplace_back(TILEI_MASK_SHALLOW_WATER, TEX_ICONS);
        else if (ch == TILE_DNGN_DEEP_WATER)
            tileset.emplace_back(TILEI_MASK_DEEP_WATER, TEX_ICONS);
        else if (ch == TILE_DNGN_SHALLOW_WATER_MURKY)
            tileset.emplace_back(TILEI_MASK_SHALLOW_WATER_MURKY, TEX_ICONS);
        else if (ch == TILE_DNGN_DEEP_WATER_MURKY)
            tileset.emplace_back(TILEI_MASK_DEEP_WATER_MURKY, TEX_ICONS);
    }

    string damage_desc;
    mon_dam_level_type damage_level = m->dam;

    switch (damage_level)
    {
    case MDAM_DEAD:
    case MDAM_ALMOST_DEAD:
        tileset.emplace_back(TILEI_MDAM_ALMOST_DEAD, TEX_ICONS);
        break;
    case MDAM_SEVERELY_DAMAGED:
        tileset.emplace_back(TILEI_MDAM_SEVERELY_DAMAGED, TEX_ICONS);
        break;
    case MDAM_HEAVILY_DAMAGED:
        tileset.emplace_back(TILEI_MDAM_HEAVILY_DAMAGED, TEX_ICONS);
        break;
    case MDAM_MODERATELY_DAMAGED:
        tileset.emplace_back(TILEI_MDAM_MODERATELY_DAMAGED, TEX_ICONS);
        break;
    case MDAM_LIGHTLY_DAMAGED:
        tileset.emplace_back(TILEI_MDAM_LIGHTLY_DAMAGED, TEX_ICONS);
        break;
    case MDAM_OKAY:
    default:
        // no flag for okay.
        break;
    }

    if (m->attitude == ATT_FRIENDLY)
        tileset.emplace_back(TILEI_FRIENDLY, TEX_ICONS);
    else if (m->attitude == ATT_GOOD_NEUTRAL || m->attitude == ATT_STRICT_NEUTRAL)
        tileset.emplace_back(TILEI_GOOD_NEUTRAL, TEX_ICONS);
    else if (m->neutral())
        tileset.emplace_back(TILEI_NEUTRAL, TEX_ICONS);
    else if (m->is(MB_FLEEING))
        tileset.emplace_back(TILEI_FLEEING, TEX_ICONS);
    else if (m->is(MB_STABBABLE))
        tileset.emplace_back(TILEI_STAB_BRAND, TEX_ICONS);
    else if (m->is(MB_DISTRACTED))
        tileset.emplace_back(TILEI_MAY_STAB_BRAND, TEX_ICONS);

    return true;
}

bool FeatureMenuEntry::get_tiles(vector<tile_def>& tileset) const
{
    if (!Options.tile_menu_icons)
        return false;

    if (feat == DNGN_UNSEEN)
        return false;

    MenuEntry::get_tiles(tileset);

    tileidx_t tile = tileidx_feature(pos);
    tileset.emplace_back(tile, get_dngn_tex(tile));

    if (in_bounds(pos) && is_unknown_stair(pos))
        tileset.emplace_back(TILEI_NEW_STAIR, TEX_ICONS);

    if (in_bounds(pos) && is_unknown_transporter(pos))
        tileset.emplace_back(TILEI_NEW_TRANSPORTER, TEX_ICONS);

    return true;
}

bool PlayerMenuEntry::get_tiles(vector<tile_def>& tileset) const
{
    if (!Options.tile_menu_icons)
        return false;

    MenuEntry::get_tiles(tileset);

    const player_save_info &player = *static_cast<player_save_info*>(data);
    dolls_data equip_doll = player.doll;

    // FIXME: Implement this logic in one place in e.g. pack_doll_buf().
    int p_order[TILEP_PART_MAX] =
    {
        TILEP_PART_SHADOW,  //  0
        TILEP_PART_HALO,
        TILEP_PART_ENCH,
        TILEP_PART_DRCWING,
        TILEP_PART_CLOAK,
        TILEP_PART_BASE,    //  5
        TILEP_PART_BOOTS,
        TILEP_PART_LEG,
        TILEP_PART_BODY,
        TILEP_PART_ARM,
        TILEP_PART_HAIR,
        TILEP_PART_BEARD,
        TILEP_PART_DRCHEAD,  // 15
        TILEP_PART_HELM,
        TILEP_PART_HAND1,   // 10
        TILEP_PART_HAND2,
    };

    int flags[TILEP_PART_MAX];
    tilep_calc_flags(equip_doll, flags);

    // For skirts, boots go under the leg armour. For pants, they go over.
    if (equip_doll.parts[TILEP_PART_LEG] < TILEP_LEG_SKIRT_OFS)
    {
        p_order[6] = TILEP_PART_BOOTS;
        p_order[7] = TILEP_PART_LEG;
    }

    // Special case bardings from being cut off.
    bool is_naga = (equip_doll.parts[TILEP_PART_BASE] == TILEP_BASE_NAGA
                    || equip_doll.parts[TILEP_PART_BASE] == TILEP_BASE_NAGA + 1);
    if (equip_doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_NAGA_BARDING
        && equip_doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_NAGA_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_naga ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    bool is_cent = (equip_doll.parts[TILEP_PART_BASE] == TILEP_BASE_CENTAUR
                    || equip_doll.parts[TILEP_PART_BASE] == TILEP_BASE_CENTAUR + 1);
    if (equip_doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_CENTAUR_BARDING
        && equip_doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_CENTAUR_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_cent ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    for (int i = 0; i < TILEP_PART_MAX; ++i)
    {
        const int p   = p_order[i];
        const int idx = equip_doll.parts[p];
        if (idx == 0 || idx == TILEP_SHOW_EQUIP || flags[p] == TILEP_FLAG_HIDE)
            continue;

        ASSERT_RANGE(idx, TILE_MAIN_MAX, TILEP_PLAYER_MAX);

        int ymax = TILE_Y;

        if (flags[p] == TILEP_FLAG_CUT_CENTAUR
            || flags[p] == TILEP_FLAG_CUT_NAGA)
        {
            ymax = 18;
        }

        tileset.emplace_back(idx, TEX_PLAYER, ymax);
    }

    return true;
}
#endif

bool Menu::is_selectable(int item) const
{
    if (select_filter.empty())
        return true;

    string text = items[item]->get_filter_text();
    for (const text_pattern &pat : select_filter)
        if (pat.matches(text))
            return true;

    return false;
}

void Menu::select_item_index(int idx, int qty, bool draw_cursor)
{
    const int old_cursor = get_cursor();

    last_selected = idx;
    items[idx]->select(qty);
    m_ui.menu->update_item(idx);
#ifdef USE_TILE_WEB
    webtiles_update_item(idx);
#endif

    if (draw_cursor)
    {
        int it_count = items.size();

        const int new_cursor = get_cursor();
        if (old_cursor != -1 && old_cursor < it_count
            && items[old_cursor]->level == MEL_ITEM)
        {
            m_ui.menu->update_item(old_cursor);
        }
        if (new_cursor != -1 && new_cursor < it_count
            && items[new_cursor]->level == MEL_ITEM)
        {
            m_ui.menu->update_item(new_cursor);
        }
    }
}

void Menu::select_index(int index, int qty)
{
    int first_vis = get_first_visible();

    int si = index == -1 ? first_vis : index;

    if (index == -1)
    {
        if (flags & MF_MULTISELECT)
        {
            for (int i = 0, count = items.size(); i < count; ++i)
            {
                if (items[i]->level != MEL_ITEM
                    || items[i]->hotkeys.empty())
                {
                    continue;
                }
                if (is_hotkey(i, items[i]->hotkeys[0])
                    && (qty != -2 || is_selectable(i)))
                {
                    select_item_index(i, qty);
                }
            }
        }
    }
    else if (items[si]->level == MEL_SUBTITLE && (flags & MF_MULTISELECT))
    {
        for (int i = si + 1, count = items.size(); i < count; ++i)
        {
            if (items[i]->level != MEL_ITEM
                || items[i]->hotkeys.empty())
            {
                continue;
            }
            if (is_hotkey(i, items[i]->hotkeys[0]))
                select_item_index(i, qty);
        }
    }
    else if (items[si]->level == MEL_ITEM
             && (flags & (MF_SINGLESELECT | MF_MULTISELECT)))
    {
        select_item_index(si, qty, (flags & MF_MULTISELECT));
    }
}

int Menu::get_entry_index(const MenuEntry *e) const
{
    int index = 0;
    for (const auto &item : items)
    {
        if (item == e)
            return index;

        if (item->quantity != 0)
            index++;
    }

    return -1;
}

void Menu::update_menu(bool update_entries)
{
    m_ui.menu->update_items();
    update_title();

    if (!alive)
        return;
#ifdef USE_TILE_WEB
    if (update_entries)
    {
        tiles.json_open_object();
        tiles.json_write_string("msg", "update_menu");
        tiles.json_write_int("total_items", items.size());
        tiles.json_close_object();
        tiles.finish_message();
        if (items.size() > 0)
            webtiles_update_items(0, items.size() - 1);
    }
#endif
}

void Menu::update_more()
{
    if (crawl_state.doing_prev_cmd_again)
        return;
    m_ui.more->set_text(more);

    bool show_more = !more.ops.empty();
#ifdef USE_TILE_LOCAL
    show_more = show_more && !m_keyhelp_more;
#endif
    m_ui.more_bin->set_visible(show_more);

#ifdef USE_TILE_WEB
    if (!alive)
        return;
    tiles.json_open_object();
    tiles.json_write_string("msg", "update_menu");
    tiles.json_write_string("more",
            m_keyhelp_more ? "" : more.to_colour_string());
    tiles.json_close_object();
    tiles.finish_message();
#endif
}

int Menu::item_colour(const MenuEntry *entry) const
{
    int icol = -1;
    if (highlighter)
        icol = highlighter->entry_colour(entry);

    return icol == -1 ? entry->colour : icol;
}

formatted_string Menu::calc_title() { return formatted_string(); }

void Menu::update_title()
{
    if (!title || crawl_state.doing_prev_cmd_again)
        return;

    formatted_string fs;

    if (m_filter)
    {
        fs.textcolour(WHITE);
        fs.cprintf("Select what? (regex) %s", m_filter->get_text().c_str());
    } else
        fs = calc_title();

    if (fs.empty())
    {
        const bool first = (action_cycle == CYCLE_NONE
                            || menu_action == ACT_EXECUTE);
        if (!first)
            ASSERT(title2);

        auto col = item_colour(first ? title : title2);
        string text = (first ? title->get_text() : title2->get_text());

        fs = formatted_string(col);

        if (flags & MF_ALLOW_FORMATTING)
            fs += formatted_string::parse_string(text);
        else
            fs.cprintf("%s", text.c_str());
    }

    if (!is_set(MF_QUIET_SELECT) && is_set(MF_MULTISELECT))
        fs.cprintf("%s", get_select_count_string(sel.size()).c_str());

    if (m_indent_title)
    {
        formatted_string indented(" ");
        indented += fs;
        fs = indented;
    }

#ifdef USE_TILE_LOCAL
    m_ui.title->set_margin_for_sdl({0, 0, 10,
            UIMenu::item_pad + (m_indent_title ? 38 : 0)});
#endif
    m_ui.title->set_text(fs);
#ifdef USE_TILE_WEB
    webtiles_set_title(fs);
#endif
}

bool Menu::in_page(int index) const
{
    int y1, y2;
    m_ui.menu->get_item_region(index, &y1, &y2);
    int vph = m_ui.menu->get_region()[3];
    int vpy = m_ui.scroller->get_scroll();
    return (vpy < y1 && y1 < vpy+vph) || (vpy < y2 && y2 < vpy+vph);
}

bool Menu::page_down()
{
    int dy = m_ui.scroller->get_region()[3];
    int y = m_ui.scroller->get_scroll();
    bool at_bottom = y+dy >= m_ui.menu->get_region()[3];
    m_ui.scroller->set_scroll(y+dy);
#ifndef USE_TILE_LOCAL
    if (!at_bottom)
        m_ui.menu->set_showable_height(y+dy+dy);
#endif
    return !at_bottom;
}

bool Menu::page_up()
{
    int dy = m_ui.scroller->get_region()[3];
    int y = m_ui.scroller->get_scroll();
    m_ui.scroller->set_scroll(y-dy);
#ifndef USE_TILE_LOCAL
    m_ui.menu->set_showable_height(y);
#endif
    return y > 0;
}

bool Menu::line_down()
{
    int index = get_first_visible();
    int first_vis_y;
    m_ui.menu->get_item_region(index, &first_vis_y, nullptr);

    index++;
    while (index < (int)items.size())
    {
        int y;
        m_ui.menu->get_item_region(index, &y, nullptr);
        index++;
        if (y == first_vis_y)
            continue;
        m_ui.scroller->set_scroll(y);
        return true;
    }
    return false;
}

// XXX: doesn't do exactly what we want
bool Menu::line_up()
{
    int index = get_first_visible();
    if (index > 0)
    {
        int y;
        m_ui.menu->get_item_region(index-1, &y, nullptr);
        m_ui.scroller->set_scroll(y);
#ifndef USE_TILE_LOCAL
        int dy = m_ui.scroller->get_region()[3];
        m_ui.menu->set_showable_height(y+dy);
#endif
        return true;
    }
    return false;
}

#ifdef USE_TILE_WEB
void Menu::webtiles_write_menu(bool replace) const
{
    if (crawl_state.doing_prev_cmd_again)
        return;

    tiles.json_open_object();
    tiles.json_write_string("msg", "menu");
    tiles.json_write_bool("ui-centred", !crawl_state.need_save);
    tiles.json_write_string("tag", tag);
    tiles.json_write_int("flags", flags);
    if (replace)
        tiles.json_write_int("replace", 1);

    webtiles_write_title();

    tiles.json_write_string("more",
            m_keyhelp_more ? "" : more.to_colour_string());

    int count = items.size();
    int start = 0;
    int end = start + count;

    tiles.json_write_int("total_items", count);
    tiles.json_write_int("chunk_start", start);

    int first_entry = get_first_visible();
    if (first_entry != 0 && !is_set(MF_START_AT_END))
        tiles.json_write_int("jump_to", first_entry);

    tiles.json_open_array("items");

    for (int i = start; i < end; ++i)
        webtiles_write_item(i, items[i]);

    tiles.json_close_array();

    tiles.json_close_object();
}

void Menu::webtiles_scroll(int first)
{
    // catch and ignore stale scroll events
    if (first >= static_cast<int>(items.size()))
        return;

    int item_y;
    m_ui.menu->get_item_region(first, &item_y, nullptr);
    if (m_ui.scroller->get_scroll() != item_y)
    {
        m_ui.scroller->set_scroll(item_y);
        webtiles_update_scroll_pos();
        ui_force_render();
    }
}

void Menu::webtiles_handle_item_request(int start, int end)
{
    start = min(max(0, start), (int)items.size()-1);
    if (end < start) end = start;
    if (end >= (int)items.size())
        end = (int)items.size() - 1;

    tiles.json_open_object();
    tiles.json_write_string("msg", "update_menu_items");

    tiles.json_write_int("chunk_start", start);

    tiles.json_open_array("items");

    for (int i = start; i <= end; ++i)
        webtiles_write_item(i, items[i]);

    tiles.json_close_array();

    tiles.json_close_object();
    tiles.finish_message();
}

void Menu::webtiles_set_title(const formatted_string title_)
{
    if (title_.to_colour_string() != _webtiles_title.to_colour_string())
    {
        _webtiles_title_changed = true;
        _webtiles_title = title_;
    }
}

void Menu::webtiles_update_items(int start, int end) const
{
    ASSERT_RANGE(start, 0, (int) items.size());
    ASSERT_RANGE(end, start, (int) items.size());

    tiles.json_open_object();

    tiles.json_write_string("msg", "update_menu_items");
    tiles.json_write_int("chunk_start", start);

    tiles.json_open_array("items");

    for (int i = start; i <= end; ++i)
    {
        tiles.json_open_object();
        const MenuEntry* me = items[i];
        tiles.json_write_string("text", me->get_text());
        int col = item_colour(me);
        // previous colour field is deleted by client if new one not sent
        if (col != MENU_ITEM_STOCK_COLOUR)
            tiles.json_write_int("colour", col);
        webtiles_write_tiles(*me);
        tiles.json_close_object();
    }

    tiles.json_close_array();

    tiles.json_close_object();
    tiles.finish_message();
}


void Menu::webtiles_update_item(int index) const
{
    webtiles_update_items(index, index);
}

void Menu::webtiles_update_title() const
{
    tiles.json_open_object();
    tiles.json_write_string("msg", "update_menu");
    webtiles_write_title();
    tiles.json_close_object();
    tiles.finish_message();
}

void Menu::webtiles_update_scroll_pos() const
{
    tiles.json_open_object();
    tiles.json_write_string("msg", "menu_scroll");
    tiles.json_write_int("first", get_first_visible());
    tiles.json_close_object();
    tiles.finish_message();
}

void Menu::webtiles_write_title() const
{
    // the title object only exists for backwards compatibility
    tiles.json_open_object("title");
    tiles.json_write_string("text", _webtiles_title.to_colour_string());
    tiles.json_close_object("title");
}

void Menu::webtiles_write_tiles(const MenuEntry& me) const
{
    vector<tile_def> t;
    if (me.get_tiles(t) && !t.empty())
    {
        tiles.json_open_array("tiles");

        for (const tile_def &tile : t)
        {
            tiles.json_open_object();

            tiles.json_write_int("t", tile.tile);
            tiles.json_write_int("tex", tile.tex);

            if (tile.ymax != TILE_Y)
                tiles.json_write_int("ymax", tile.ymax);

            tiles.json_close_object();
        }

        tiles.json_close_array();
    }
}

void Menu::webtiles_write_item(int index, const MenuEntry* me) const
{
    tiles.json_open_object();

    if (me)
        tiles.json_write_string("text", me->get_text());
    else
    {
        tiles.json_write_string("text", "");
        tiles.json_close_object();
        return;
    }

    if (me->quantity)
        tiles.json_write_int("q", me->quantity);

    int col = item_colour(me);
    if (col != MENU_ITEM_STOCK_COLOUR)
        tiles.json_write_int("colour", col);

    if (!me->hotkeys.empty())
    {
        tiles.json_open_array("hotkeys");
        for (int hotkey : me->hotkeys)
            tiles.json_write_int(hotkey);
        tiles.json_close_array();
    }

    if (me->level != MEL_NONE)
        tiles.json_write_int("level", me->level);

    if (me->preselected)
        tiles.json_write_int("preselected", me->preselected);

    webtiles_write_tiles(*me);

    tiles.json_close_object();
}
#endif // USE_TILE_WEB

/////////////////////////////////////////////////////////////////
// Menu colouring
//

int menu_colour(const string &text, const string &prefix, const string &tag)
{
    const string tmp_text = prefix + text;

    for (const colour_mapping &cm : Options.menu_colour_mappings)
    {
        if ((cm.tag.empty() || cm.tag == "any" || cm.tag == tag
               || cm.tag == "inventory" && tag == "pickup")
            && cm.pattern.matches(tmp_text))
        {
            return cm.colour;
        }
    }
    return -1;
}

int MenuHighlighter::entry_colour(const MenuEntry *entry) const
{
    return entry->colour != MENU_ITEM_STOCK_COLOUR ? entry->colour
           : entry->highlight_colour();
}

///////////////////////////////////////////////////////////////////////
// column_composer

column_composer::column_composer(int cols, ...)
    : columns()
{
    ASSERT(cols > 0);

    va_list args;
    va_start(args, cols);

    columns.emplace_back(1);
    int lastcol = 1;
    for (int i = 1; i < cols; ++i)
    {
        int nextcol = va_arg(args, int);
        ASSERT(nextcol > lastcol);

        lastcol = nextcol;
        columns.emplace_back(nextcol);
    }

    va_end(args);
}

void column_composer::clear()
{
    flines.clear();
}

void column_composer::add_formatted(int ncol,
                                    const string &s,
                                    bool add_separator,
                                    int  margin)
{
    ASSERT_RANGE(ncol, 0, (int) columns.size());

    column &col = columns[ncol];
    vector<string> segs = split_string("\n", s, false, true);

    vector<formatted_string> newlines;
    // Add a blank line if necessary. Blank lines will not
    // be added at page boundaries.
    if (add_separator && col.lines && !segs.empty())
        newlines.emplace_back();

    for (const string &seg : segs)
        newlines.push_back(formatted_string::parse_string(seg));

    strip_blank_lines(newlines);

    compose_formatted_column(newlines,
                              col.lines,
                              margin == -1 ? col.margin : margin);

    col.lines += newlines.size();

    strip_blank_lines(flines);
}

vector<formatted_string> column_composer::formatted_lines() const
{
    return flines;
}

void column_composer::strip_blank_lines(vector<formatted_string> &fs) const
{
    for (int i = fs.size() - 1; i >= 0; --i)
    {
        if (fs[i].width() == 0)
            fs.erase(fs.begin() + i);
        else
            break;
    }
}

void column_composer::compose_formatted_column(
        const vector<formatted_string> &lines,
        int startline,
        int margin)
{
    if (flines.size() < startline + lines.size())
        flines.resize(startline + lines.size());

    for (unsigned i = 0, size = lines.size(); i < size; ++i)
    {
        int f = i + startline;
        if (margin > 1)
        {
            int xdelta = margin - flines[f].width() - 1;
            if (xdelta > 0)
                flines[f].cprintf("%-*s", xdelta, "");
        }
        flines[f] += lines[i];
    }
}

int linebreak_string(string& s, int maxcol, bool indent)
{
    // [ds] Don't loop forever if the user is playing silly games with
    // their term size.
    if (maxcol < 1)
        return 0;

    int breakcount = 0;
    string res;

    while (!s.empty())
    {
        res += wordwrap_line(s, maxcol, true, indent);
        if (!s.empty())
        {
            res += "\n";
            breakcount++;
        }
    }
    s = res;
    return breakcount;
}

string get_linebreak_string(const string& s, int maxcol)
{
    string r = s;
    linebreak_string(r, maxcol);
    return r;
}

int ToggleableMenu::pre_process(int key)
{
#ifdef TOUCH_UI
    if (find(toggle_keys.begin(), toggle_keys.end(), key) != toggle_keys.end()
        || key == CK_TOUCH_DUMMY)
#else
    if (find(toggle_keys.begin(), toggle_keys.end(), key) != toggle_keys.end())
#endif
    {
        // Toggle all menu entries
        for (MenuEntry *item : items)
            if (auto p = dynamic_cast<ToggleableMenuEntry*>(item))
                p->toggle();

        // Toggle title
        if (auto pt = dynamic_cast<ToggleableMenuEntry*>(title))
            pt->toggle();

        update_menu();

#ifdef USE_TILE_WEB
        webtiles_update_items(0, items.size() - 1);
#endif

        if (flags & MF_TOGGLE_ACTION)
        {
            if (menu_action == ACT_EXECUTE)
                menu_action = ACT_EXAMINE;
            else
                menu_action = ACT_EXECUTE;
        }

        // Don't further process the key
#ifdef TOUCH_UI
        return CK_TOUCH_DUMMY;
#else
        return 0;
#endif
    }
    return key;
}

/**
 * Performs regular rectangular AABB intersection between the given AABB
 * rectangle and a item in the menu_entries
 * <pre>
 * start(x,y)------------
 *           |          |
 *           ------------end(x,y)
 * </pre>
 */
static bool _AABB_intersection(const coord_def& item_start,
                              const coord_def& item_end,
                              const coord_def& aabb_start,
                              const coord_def& aabb_end)
{
    // Check for no overlap using equals on purpose to rule out entities
    // that only brush the bounding box
    if (aabb_start.x >= item_end.x)
        return false;
    if (aabb_end.x <= item_start.x)
        return false;
    if (aabb_start.y >= item_end.y)
        return false;
    if (aabb_end.y <= item_start.y)
        return false;
    // We have overlap
    return true;
}

PrecisionMenu::PrecisionMenu() : m_active_object(nullptr),
    m_select_type(PRECISION_SINGLESELECT)
{
}

PrecisionMenu::~PrecisionMenu()
{
    clear();
}

void PrecisionMenu::set_select_type(SelectType flag)
{
    m_select_type = flag;
}

/**
 * Frees all used memory
 */
void PrecisionMenu::clear()
{
    // release all the data reserved
    deleteAll(m_attached_objects);
}

/**
 * Processes user input.
 *
 * Returns:
 * true when a significant event happened, signaling that the player has made a
 * menu ending action like selecting an item in singleselect mode
 * false otherwise
 */
bool PrecisionMenu::process_key(int key)
{
    if (m_active_object == nullptr)
    {
        if (m_attached_objects.empty())
        {
            // nothing to process
            return true;
        }
        else
        {
            // pick the first object possible
            for (auto mobj : m_attached_objects)
            {
                if (mobj->can_be_focused())
                {
                    m_active_object = mobj;
                    break;
                }
            }
        }
    }

#ifdef TOUCH_UI
    if (key == CK_TOUCH_DUMMY)
        return true; // mouse click in title area, which wouldn't usually be handled
#endif
    // Handle CK_MOUSE_CLICK separately
    // This signifies a menu ending action
    if (key == CK_MOUSE_CLICK)
        return true;

    bool focus_find = false;
    PrecisionMenu::Direction focus_direction;
    MenuObject::InputReturnValue input_ret = m_active_object->process_input(key);
    switch (input_ret)
    {
    case MenuObject::INPUT_NO_ACTION:
        break;
    case MenuObject::INPUT_SELECTED:
        if (m_select_type == PRECISION_SINGLESELECT)
            return true;
        else
        {
            // TODO: Handle multiselect somehow
        }
        break;
    case MenuObject::INPUT_DESELECTED:
        break;
    case MenuObject::INPUT_END_MENU_SUCCESS:
        return true;
    case MenuObject::INPUT_END_MENU_ABORT:
        clear_selections();
        return true;
    case MenuObject::INPUT_ACTIVE_CHANGED:
        break;
    case MenuObject::INPUT_FOCUS_RELEASE_UP:
        focus_find = true;
        focus_direction = PrecisionMenu::UP;
        break;
    case MenuObject::INPUT_FOCUS_RELEASE_DOWN:
        focus_find = true;
        focus_direction = PrecisionMenu::DOWN;
        break;
    case MenuObject::INPUT_FOCUS_RELEASE_LEFT:
        focus_find = true;
        focus_direction = PrecisionMenu::LEFT;
        break;
    case MenuObject::INPUT_FOCUS_RELEASE_RIGHT:
        focus_find = true;
        focus_direction = PrecisionMenu::RIGHT;
        break;
    default:
        die("Malformed return value");
        break;
    }
    if (focus_find)
    {
        MenuObject* find_object = _find_object_by_direction(m_active_object,
                                                            focus_direction);
        if (find_object != nullptr)
        {
            m_active_object->set_active_item((MenuItem*)nullptr);
            m_active_object = find_object;
            if (focus_direction == PrecisionMenu::UP)
                m_active_object->activate_last_item();
            else
                m_active_object->activate_first_item();
        }
    }
    // Handle selection of other objects items hotkeys
    for (MenuObject *obj : m_attached_objects)
    {
        MenuItem* tmp = obj->select_item_by_hotkey(key);
        if (tmp != nullptr)
        {
            // was it a toggle?
            if (!tmp->selected())
                continue;
            // it was a selection
            if (m_select_type == PrecisionMenu::PRECISION_SINGLESELECT)
                return true;
        }
    }
    return false;
}

#ifdef USE_TILE_LOCAL
int PrecisionMenu::handle_mouse(const MouseEvent &me)
{
    // Feed input to each attached object that the mouse is over
    // The objects are responsible for processing the input
    // This includes, if applicable for instance checking if the mouse
    // is over the item or not
    for (MenuObject *obj : m_attached_objects)
    {
        const MenuObject::InputReturnValue input_return = obj->handle_mouse(me);

        switch (input_return)
        {
        case MenuObject::INPUT_SELECTED:
            m_active_object = obj;
            if (m_select_type == PRECISION_SINGLESELECT)
                return CK_MOUSE_CLICK;
            break;
        case MenuObject::INPUT_ACTIVE_CHANGED:
            // Set the active object to be this one
            m_active_object = obj;
            break;
        case MenuObject::INPUT_END_MENU_SUCCESS:
            // something got clicked that needs to signal the menu to end
            return CK_MOUSE_CLICK;
        case MenuObject::INPUT_END_MENU_ABORT:
            // XXX: For right-click we use CK_MOUSE_CMD to cancel out of the
            // menu, but these mouse-button->key mappings are not very sane.
            clear_selections();
            return CK_MOUSE_CMD;
        case MenuObject::INPUT_FOCUS_LOST:
            // The object lost its focus and is no longer the active one
            if (obj == m_active_object)
                m_active_object = nullptr;
        default:
            break;
        }
    }
    return 0;
}
#endif

void PrecisionMenu::clear_selections()
{
    for (MenuObject *obj : m_attached_objects)
        obj->clear_selections();
}

/**
 * Finds the closest rectangle to given entry start on a cardinal
 * direction from it.
 * If no entries are found, nullptr is returned.
 *
 * TODO: This is exact duplicate of MenuObject::_find_item_by_direction();
 * maybe somehow generalize it and detach it from class?
 */
MenuObject* PrecisionMenu::_find_object_by_direction(const MenuObject* start,
                                                   Direction dir)
{
    if (start == nullptr)
        return nullptr;

    coord_def aabb_start(0,0);
    coord_def aabb_end(0,0);

    // construct the aabb
    switch (dir)
    {
    case UP:
        aabb_start.x = start->get_min_coord().x;
        aabb_end.x = start->get_max_coord().x;
        aabb_start.y = 0; // top of screen
        aabb_end.y = start->get_min_coord().y;
        break;
    case DOWN:
        aabb_start.x = start->get_min_coord().x;
        aabb_end.x = start->get_max_coord().x;
        aabb_start.y = start->get_max_coord().y;
        // we choose an arbitrarily large number here, because
        // tiles saves entry coordinates in pixels, yet console saves them
        // in characters
        // basically, we want the AABB to be large enough to extend to the
        // bottom of the screen in every possible resolution
        aabb_end.y = 32767;
        break;
    case LEFT:
        aabb_start.x = 0; // left of screen
        aabb_end.x = start->get_min_coord().x;
        aabb_start.y = start->get_min_coord().y;
        aabb_end.y = start->get_max_coord().y;
        break;
    case RIGHT:
        aabb_start.x = start->get_max_coord().x;
        // we again want a value that is always larger then the width of screen
        aabb_end.x = 32767;
        aabb_start.y = start->get_min_coord().y;
        aabb_end.y = start->get_max_coord().y;
        break;
    default:
        die("Bad direction given");
    }

    // loop through the entries
    // save the currently closest to the index in a variable
    MenuObject* closest = nullptr;
    for (MenuObject *obj : m_attached_objects)
    {
        if (!obj->can_be_focused())
        {
            // this is a noselect entry, skip it
            continue;
        }

        if (!_AABB_intersection(obj->get_min_coord(), obj->get_max_coord(),
                                aabb_start, aabb_end))
        {
            continue; // does not intersect, continue loop
        }

        // intersects
        // check if it's closer than current
        if (closest == nullptr)
            closest = obj;

        switch (dir)
        {
        case UP:
            if (obj->get_min_coord().y > closest->get_min_coord().y)
                closest = obj;
            break;
        case DOWN:
            if (obj->get_min_coord().y < closest->get_min_coord().y)
                closest = obj;
            break;
        case LEFT:
            if (obj->get_min_coord().x > closest->get_min_coord().x)
                closest = obj;
            break;
        case RIGHT:
            if (obj->get_min_coord().x < closest->get_min_coord().x)
                closest = obj;
        }
    }
    // TODO handle special cases here, like pressing down on the last entry
    // to go the the first item in that line
    return closest;
}

vector<MenuItem*> PrecisionMenu::get_selected_items()
{
    vector<MenuItem*> ret_val;

    for (MenuObject *obj : m_attached_objects)
        for (MenuItem *item : obj->get_selected_items())
            ret_val.push_back(item);

    return ret_val;
}

void PrecisionMenu::attach_object(MenuObject* item)
{
    ASSERT(item != nullptr);
    m_attached_objects.push_back(item);
}

// Predicate for std::find_if
static bool _string_lookup(MenuObject* item, string lookup)
{
    return item->get_name().compare(lookup) == 0;
}

MenuObject* PrecisionMenu::get_object_by_name(const string &search)
{
    auto it = find_if(begin(m_attached_objects), end(m_attached_objects),
                      bind(_string_lookup, placeholders::_1, search));
    return it != m_attached_objects.end() ? *it : nullptr;
}

MenuItem* PrecisionMenu::get_active_item()
{
    if (m_active_object != nullptr)
        return m_active_object->get_active_item();
    return nullptr;
}

void PrecisionMenu::set_active_object(MenuObject* object)
{
    if (object == m_active_object)
        return;

    // is the object attached?
    auto find_val = find(m_attached_objects.begin(), m_attached_objects.end(),
                         object);
    if (find_val != m_attached_objects.end())
    {
        m_active_object = object;
        m_active_object->activate_first_item();
    }
}

void PrecisionMenu::draw_menu()
{
    for (MenuObject *obj : m_attached_objects)
        obj->render();
    // Render everything else here

    // Reset textcolour just in case
    textcolour(LIGHTGRAY);
}

MenuItem::MenuItem(): m_min_coord(0,0), m_max_coord(0,0), m_selected(false),
                      m_allow_highlight(true), m_dirty(false), m_visible(false),
                      m_link_left(nullptr), m_link_right(nullptr),
                      m_link_up(nullptr), m_link_down(nullptr), m_item_id(-1)
{
#ifdef USE_TILE_LOCAL
    m_unit_width_pixels = tiles.get_crt_font()->char_width();
    m_unit_height_pixels = tiles.get_crt_font()->char_height();
#endif

    set_fg_colour(LIGHTGRAY);
    set_bg_colour(BLACK);
    set_highlight_colour(BLACK);
}

MenuItem::~MenuItem()
{
}

#ifdef USE_TILE_LOCAL
void MenuItem::set_height(const int height)
{
    m_unit_height_pixels = height;
}
#endif

/**
 * Override this if you use eg funky different sized fonts, tiles etc
 */
void MenuItem::set_bounds(const coord_def& min_coord, const coord_def& max_coord)
{
#ifdef USE_TILE_LOCAL
    // these are saved in font dx / dy for mouse to work properly
    // remove 1 unit from all the entries because console starts at (1,1)
    // but tiles starts at (0,0)
    m_min_coord.x = (min_coord.x - 1) * m_unit_width_pixels;
    m_min_coord.y = (min_coord.y - 1) * m_unit_height_pixels;
    m_max_coord.x = (max_coord.x - 1) * m_unit_width_pixels;
    m_max_coord.y = (max_coord.y - 1) * m_unit_height_pixels;
#else
    m_min_coord = min_coord;
    m_max_coord = max_coord;
#endif
}

/**
 * This is handly if you are already working with existing multiplied
 * coordinates and modifying them
 */
void MenuItem::set_bounds_no_multiply(const coord_def& min_coord,
                                      const coord_def& max_coord)
{
    m_min_coord = min_coord;
    m_max_coord = max_coord;
}

void MenuItem::move(const coord_def& delta)
{
    m_min_coord += delta;
    m_max_coord += delta;
}

// By default, value does nothing. Override for Items needing it.
void MenuItem::select(bool toggle, int value)
{
    select(toggle);
}

void MenuItem::select(bool toggle)
{
    m_selected = toggle;
    m_dirty = true;
}

bool MenuItem::selected() const
{
    return m_selected;
}

void MenuItem::allow_highlight(bool toggle)
{
    m_allow_highlight = toggle;
    m_dirty = true;
}

bool MenuItem::can_be_highlighted() const
{
    return m_allow_highlight;
}

void MenuItem::set_highlight_colour(COLOURS colour)
{
    m_highlight_colour = colour;
    m_dirty = true;
}

COLOURS MenuItem::get_highlight_colour() const
{
    return m_highlight_colour;
}

void MenuItem::set_bg_colour(COLOURS colour)
{
    m_bg_colour = colour;
    m_dirty = true;
}

void MenuItem::set_fg_colour(COLOURS colour)
{
    m_fg_colour = colour;
    m_dirty = true;
}

COLOURS MenuItem::get_fg_colour() const
{
    return m_fg_colour;
}

COLOURS MenuItem::get_bg_colour() const
{
    return static_cast<COLOURS> (m_bg_colour);
}

void MenuItem::set_visible(bool flag)
{
    m_visible = flag;
}

bool MenuItem::is_visible() const
{
    return m_visible;
}

void MenuItem::add_hotkey(int key)
{
    m_hotkeys.push_back(key);
}

void MenuItem::clear_hotkeys()
{
    m_hotkeys.clear();
}

const vector<int>& MenuItem::get_hotkeys() const
{
    return m_hotkeys;
}

void MenuItem::set_link_left(MenuItem* item)
{
    m_link_left = item;
}

void MenuItem::set_link_right(MenuItem* item)
{
    m_link_right = item;
}

void MenuItem::set_link_up(MenuItem* item)
{
    m_link_up = item;
}

void MenuItem::set_link_down(MenuItem* item)
{
    m_link_down = item;
}

MenuItem* MenuItem::get_link_left() const
{
    return m_link_left;
}

MenuItem* MenuItem::get_link_right() const
{
    return m_link_right;
}

MenuItem* MenuItem::get_link_up() const
{
    return m_link_up;
}

MenuItem* MenuItem::get_link_down() const
{
    return m_link_down;
}

#ifdef USE_TILE_LOCAL
int MenuItem::get_vertical_offset() const
{
    return m_unit_height_pixels / 2 - tiles.get_crt_font()->char_height() / 2;
}
#endif

TextItem::TextItem()
#ifdef USE_TILE_LOCAL
                        : m_font_buf(tiles.get_crt_font())
#endif
{
}

TextItem::~TextItem()
{
}

/**
 * Rewrap the text if bounds changes
 */
void TextItem::set_bounds(const coord_def& min_coord, const coord_def& max_coord)
{
    MenuItem::set_bounds(min_coord, max_coord);
    _wrap_text();
    m_dirty = true;
}

/**
 * Rewrap the text if bounds changes
 */
void TextItem::set_bounds_no_multiply(const coord_def& min_coord,
                                      const coord_def& max_coord)
{
    MenuItem::set_bounds_no_multiply(min_coord, max_coord);
    _wrap_text();
    m_dirty = true;
}

void TextItem::render()
{
    if (!m_visible)
        return;

#ifdef USE_TILE_LOCAL
    if (m_dirty)
    {
        m_font_buf.clear();
        // TODO: handle m_bg_colour
        m_font_buf.add(m_render_text, term_colours[m_fg_colour],
                       m_min_coord.x, m_min_coord.y + get_vertical_offset());
        m_dirty = false;
    }
    m_font_buf.draw();
#else
    // Clean the drawing area first
    // clear_to_end_of_line does not work for us
    string white_space(m_max_coord.x - m_min_coord.x, ' ');
    textcolour(BLACK);
    for (int i = 0; i < (m_max_coord.y - m_min_coord.y); ++i)
    {
        cgotoxy(m_min_coord.x, m_min_coord.y + i);
        cprintf("%s", white_space.c_str());
    }

    // print each line separately, is there a cleaner solution?
    size_t newline_pos = 0;
    size_t endline_pos = 0;
    for (int i = 0; i < (m_max_coord.y - m_min_coord.y); ++i)
    {
        endline_pos = m_render_text.find('\n', newline_pos);
        cgotoxy(m_min_coord.x, m_min_coord.y + i);
        textcolour(m_fg_colour);
        textbackground(m_bg_colour);
        cprintf("%s", m_render_text.substr(newline_pos,
                endline_pos - newline_pos).c_str());
        if (endline_pos != string::npos)
            newline_pos = endline_pos + 1;
        else
            break;
    }
    // clear text background
    textbackground(BLACK);
#endif
}

void TextItem::set_text(const string& text)
{
    m_text = text;
    _wrap_text();
    m_dirty = true;
}

const string& TextItem::get_text() const
{
    return m_text;
}

/**
 * Wraps and chops the #m_text variable and saves the chopped
 * text to #m_render_text.
 * This is done to preserve the old text in case the text item
 * changes size and could fit more text.
 * Override if you use font with different sizes than CRTRegion font.
 */
void TextItem::_wrap_text()
{
    m_render_text = m_text; // preserve original copy intact
    int max_cols;
    int max_lines;
    max_cols = (m_max_coord.x - m_min_coord.x);
    max_lines = (m_max_coord.y - m_min_coord.y);
#ifdef USE_TILE_LOCAL
    // Tiles saves coordinates in pixels
    max_cols = max_cols / m_unit_width_pixels;
    max_lines = max_lines / m_unit_height_pixels;
#endif
    if (max_cols == 0 || max_lines == 0)
    {
        // escape and set render text to nothing
        m_render_text = "";
        return;
    }

    int num_linebreaks = linebreak_string(m_render_text, max_cols);
    if (num_linebreaks > max_lines)
    {
        size_t pos = 0;
        // find the max_line'th occurrence of '\n'
        for (int i = 0; i < max_lines; ++i)
            pos = m_render_text.find('\n', pos);

        // Chop of all the nonfitting text
        m_render_text = m_render_text.substr(pos);
    }
    // m_render_text now holds the fitting part of the text, ready for render!
}


EditableTextItem::EditableTextItem() : TextItem(),
                        editable(true), in_edit_mode(false), edit_width(-1),
                        tag("generic_text_box")
{
}

void EditableTextItem::set_editable(bool e, int width)
{
    editable = e;
    edit_width = width;
}

/**
 * A rudimentary textbox editing mode.
 *
 * This uses a line_reader to read some text at the location of the TextItem.
 * It does not do anything with the edit results! You will need to call this
 * function at the right point in the gui, and do something appropriate with
 * the results elsewhere.
 *
 * @param custom_prefill a string to populate the box; if null, this will use
 *                          the current text.
 * @param keyproc_fun an optional keyproc for the line_reader
  *                     (see lin_reader::set_keyproc).
 *
 * @return the result of the editing, including the string and the int
 *          returned by the line_reader.
 */
edit_result EditableTextItem::edit(const string *custom_prefill,
                                   const line_reader::keyproc keyproc_fun)
{
    char buf[80];

    if (!editable)
        return edit_result(string(m_text), 0);

    // this is needed because render will get called during the input loop.
    unwind_bool e_mode(in_edit_mode, true);

    int e_width;
    int box_width = m_max_coord.x - m_min_coord.x;
    if (edit_width <= 0)
        e_width = box_width;
    else
        e_width = edit_width;

    e_width = min(e_width, (int) sizeof buf - 1);

    // TODO: make width not dependent on prefill string
    string prefill = make_stringf("%-*s", e_width,
        custom_prefill ? custom_prefill->c_str() : m_text.c_str());

    strncpy(buf, prefill.c_str(), e_width);
    buf[e_width] = 0;

    mouse_control mc(MOUSE_MODE_PROMPT);

#ifdef USE_TILE_LOCAL
    m_line_buf.clear();
    m_line_buf.add_square(m_min_coord.x, m_min_coord.y,
                          m_max_coord.x, m_max_coord.y, term_colours[RED]);
    m_line_buf.draw();

    unwind_bool dirty(m_dirty, false);

    fontbuf_line_reader reader(buf, e_width+1, m_font_buf, 80);
    reader.set_location(coord_def(m_min_coord.x,
                                  m_min_coord.y + get_vertical_offset()));
#else
    line_reader reader(buf, e_width+1, 80);
    reader.set_location(m_min_coord);
#endif

    reader.set_edit_mode(EDIT_MODE_OVERWRITE);
    if (keyproc_fun)
        reader.set_keyproc(keyproc_fun);

#ifdef USE_TILE_WEB
    reader.set_prompt(prompt);
    reader.set_tag(tag);
#endif

    reader.set_colour(COLOUR_INHERIT, m_highlight_colour);
    int result = reader.read_line(false, true);

#ifdef USE_TILE_LOCAL
    m_line_buf.clear();
    m_line_buf.draw();
#endif

    return edit_result(string(buf), result);
}

void EditableTextItem::set_tag(string t)
{
    tag = t;
}

void EditableTextItem::set_prompt(string p)
{
    prompt = p;
}

bool EditableTextItem::selected() const
{
    return false;
}

bool EditableTextItem::can_be_highlighted() const
{
    // TODO: make this work better
    return false;
}

void EditableTextItem::render()
{
#ifdef USE_TILE_LOCAL
    if (in_edit_mode)
    {
        m_line_buf.add_square(m_min_coord.x, m_min_coord.y,
                              m_max_coord.x, m_max_coord.y,
                              term_colours[m_highlight_colour]);
        m_line_buf.draw();
        // this relies on m_font_buf being modified by the reader
        m_font_buf.draw();
    }
    else
    {
        m_line_buf.clear();
        m_line_buf.draw();
        TextItem::render();
    }
#else
    TextItem::render();
#endif //USE_TILE_LOCAL
}

NoSelectTextItem::NoSelectTextItem()
{
}

NoSelectTextItem::~NoSelectTextItem()
{
}

// Do not allow selection
bool NoSelectTextItem::selected() const
{
    return false;
}

// Do not allow highlight
bool NoSelectTextItem::can_be_highlighted() const
{
    return false;
}

void FormattedTextItem::render()
{
    if (!m_visible)
        return;

    if (m_max_coord.x == m_min_coord.x || m_max_coord.y == m_min_coord.y)
        return;

#ifdef USE_TILE_LOCAL
    if (m_dirty)
    {
        m_font_buf.clear();
        // FIXME: m_fg_colour doesn't work here while it works in console.
        textcolour(m_fg_colour);
        m_font_buf.add(formatted_string::parse_string(m_render_text,
                                                      m_fg_colour),
                       m_min_coord.x, m_min_coord.y + get_vertical_offset());
        m_dirty = false;
    }
    m_font_buf.draw();
#else
    // Clean the drawing area first
    // clear_to_end_of_line does not work for us
    ASSERT(m_max_coord.x > m_min_coord.x);
    ASSERT(m_max_coord.y > m_min_coord.y);
    string white_space(m_max_coord.x - m_min_coord.x, ' ');
    for (int i = 0; i < (m_max_coord.y - m_min_coord.y); ++i)
    {
        cgotoxy(m_min_coord.x, m_min_coord.y + i);
        cprintf("%s", white_space.c_str());
    }

    cgotoxy(m_min_coord.x, m_min_coord.y);
    textcolour(m_fg_colour);
    display_tagged_block(m_render_text);
#endif
}

#ifdef USE_TILE_LOCAL
TextTileItem::TextTileItem()
{
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].set_tex(&tiles.get_image_manager()->m_textures[i]);
    m_unit_height_pixels = max<int>(m_unit_height_pixels, TILE_Y);
}

TextTileItem::~TextTileItem()
{
}

void TextTileItem::add_tile(tile_def tile)
{
    m_tiles.push_back(tile);
    m_dirty = true;
}

void TextTileItem::set_bounds(const coord_def &min_coord, const coord_def &max_coord)
{
    // these are saved in font dx / dy for mouse to work properly
    // remove 1 unit from all the entries because console starts at (1,1)
    // but tiles starts at (0,0)
    m_min_coord.x = (min_coord.x - 1) * m_unit_width_pixels;
    m_max_coord.x = (max_coord.x - 1) * m_unit_width_pixels + 4;
    m_min_coord.y = (min_coord.y - 1) * m_unit_height_pixels;
    m_max_coord.y = (max_coord.y - 1) * m_unit_height_pixels + 4;
}

void TextTileItem::render()
{
    if (!m_visible)
        return;

    if (m_dirty)
    {
        m_font_buf.clear();
        for (int t = 0; t < TEX_MAX; t++)
            m_tile_buf[t].clear();
        for (const tile_def &tdef : m_tiles)
        {
            int tile      = tdef.tile;
            TextureID tex = tdef.tex;
            m_tile_buf[tex].add_unscaled(tile, m_min_coord.x + 2, m_min_coord.y + 2,
                                         tdef.ymax,
                                         (float)m_unit_height_pixels / TILE_Y);
        }
        // center the text
        // TODO wrap / chop the text
        const int tile_offset = m_tiles.empty() ? 0 : (m_unit_height_pixels + 6);
        m_font_buf.add(m_text, term_colours[m_fg_colour],
                       m_min_coord.x + 2 + tile_offset,
                       m_min_coord.y + 2 + get_vertical_offset());

        m_dirty = false;
    }

    m_font_buf.draw();
    for (int i = 0; i < TEX_MAX; i++)
        m_tile_buf[i].draw();
}

SaveMenuItem::SaveMenuItem()
{
}

SaveMenuItem::~SaveMenuItem()
{
    for (int t = 0; t < TEX_MAX; t++)
        m_tile_buf[t].clear();
}

void SaveMenuItem::render()
{
    if (!m_visible)
        return;

    TextTileItem::render();
}

void SaveMenuItem::set_doll(dolls_data doll)
{
    m_save_doll = doll;
    _pack_doll();
}

void SaveMenuItem::_pack_doll()
{
    m_tiles.clear();
    // FIXME: Implement this logic in one place in e.g. pack_doll_buf().
    int p_order[TILEP_PART_MAX] =
    {
        TILEP_PART_SHADOW,  //  0
        TILEP_PART_HALO,
        TILEP_PART_ENCH,
        TILEP_PART_DRCWING,
        TILEP_PART_CLOAK,
        TILEP_PART_BASE,    //  5
        TILEP_PART_BOOTS,
        TILEP_PART_LEG,
        TILEP_PART_BODY,
        TILEP_PART_ARM,
        TILEP_PART_HAIR,
        TILEP_PART_BEARD,
        TILEP_PART_DRCHEAD,  // 15
        TILEP_PART_HELM,
        TILEP_PART_HAND1,   // 10
        TILEP_PART_HAND2,
    };

    int flags[TILEP_PART_MAX];
    tilep_calc_flags(m_save_doll, flags);

    // For skirts, boots go under the leg armour. For pants, they go over.
    if (m_save_doll.parts[TILEP_PART_LEG] < TILEP_LEG_SKIRT_OFS)
    {
        p_order[6] = TILEP_PART_BOOTS;
        p_order[7] = TILEP_PART_LEG;
    }

    // Special case bardings from being cut off.
    bool is_naga = (m_save_doll.parts[TILEP_PART_BASE] == TILEP_BASE_NAGA
                    || m_save_doll.parts[TILEP_PART_BASE] == TILEP_BASE_NAGA + 1);
    if (m_save_doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_NAGA_BARDING
        && m_save_doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_NAGA_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_naga ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    bool is_cent = (m_save_doll.parts[TILEP_PART_BASE] == TILEP_BASE_CENTAUR
                    || m_save_doll.parts[TILEP_PART_BASE] == TILEP_BASE_CENTAUR + 1);
    if (m_save_doll.parts[TILEP_PART_BOOTS] >= TILEP_BOOTS_CENTAUR_BARDING
        && m_save_doll.parts[TILEP_PART_BOOTS] <= TILEP_BOOTS_CENTAUR_BARDING_RED)
    {
        flags[TILEP_PART_BOOTS] = is_cent ? TILEP_FLAG_NORMAL : TILEP_FLAG_HIDE;
    }

    for (int i = 0; i < TILEP_PART_MAX; ++i)
    {
        const int p   = p_order[i];
        const tileidx_t idx = m_save_doll.parts[p];
        if (idx == 0 || idx == TILEP_SHOW_EQUIP || flags[p] == TILEP_FLAG_HIDE)
            continue;

        ASSERT_RANGE(idx, TILE_MAIN_MAX, TILEP_PLAYER_MAX);

        int ymax = TILE_Y;

        if (flags[p] == TILEP_FLAG_CUT_CENTAUR
            || flags[p] == TILEP_FLAG_CUT_NAGA)
        {
            ymax = 18;
        }

        m_tiles.emplace_back(idx, TEX_PLAYER, ymax);
    }
}
#endif

MenuObject::MenuObject() : m_dirty(false), m_allow_focus(true), m_min_coord(0,0),
                           m_max_coord(0,0), m_object_name("unnamed object")
{
#ifdef USE_TILE_LOCAL
    m_unit_width_pixels = tiles.get_crt_font()->char_width();
    m_unit_height_pixels = tiles.get_crt_font()->char_height();
#endif
}

MenuObject::~MenuObject()
{
}

#ifdef USE_TILE_LOCAL
void MenuObject::set_height(const int height)
{
    m_unit_height_pixels = height;
}
#endif

void MenuObject::init(const coord_def& min_coord, const coord_def& max_coord,
                      const string& name)
{
#ifdef USE_TILE_LOCAL
    // these are saved in font dx / dy for mouse to work properly
    // remove 1 unit from all the entries because console starts at (1,1)
    // but tiles starts at (0,0)
    m_min_coord.x = (min_coord.x - 1) * m_unit_width_pixels;
    m_min_coord.y = (min_coord.y - 1) * m_unit_height_pixels;
    m_max_coord.x = (max_coord.x - 1) * m_unit_width_pixels;
    m_max_coord.y = (max_coord.y - 1) * m_unit_height_pixels;
#else
    m_min_coord = min_coord;
    m_max_coord = max_coord;
#endif
    m_object_name = name;
}

bool MenuObject::_is_mouse_in_bounds(const coord_def& pos)
{
    // Is the mouse in our bounds?
    if (m_min_coord.x > static_cast<int> (pos.x)
        || m_max_coord.x < static_cast<int> (pos.x)
        || m_min_coord.y > static_cast<int> (pos.y)
        || m_max_coord.y < static_cast<int> (pos.y))
    {
        return false;
    }
    return true;
}

MenuItem* MenuObject::_find_item_by_mouse_coords(const coord_def& pos)
{
    // Is the mouse even in bounds?
    if (!_is_mouse_in_bounds(pos))
        return nullptr;

    // Traverse
    for (MenuItem *item : m_entries)
    {
        if (!item->can_be_highlighted())
        {
            // this is a noselect entry, skip it
            continue;
        }
        if (!item->is_visible())
        {
            // this item is not visible, skip it
            continue;
        }
        if (pos.x >= item->get_min_coord().x
            && pos.x <= item->get_max_coord().x
            && pos.y >= item->get_min_coord().y
            && pos.y <= item->get_max_coord().y)
        {
            // We're inside
            return item;
        }
    }

    // nothing found
    return nullptr;
}

MenuItem* MenuObject::find_item_by_hotkey(int key)
{
    // browse through all the Entries
    for (MenuItem *item : m_entries)
        for (int hotkey : item->get_hotkeys())
            if (key == hotkey)
                return item;

    return nullptr;
}

MenuItem* MenuObject::select_item_by_hotkey(int key)
{
    MenuItem* item = find_item_by_hotkey(key);
    if (item)
        select_item(item);
    return item;
}

vector<MenuItem*> MenuObject::get_selected_items()
{
    vector<MenuItem *> result;
    for (MenuItem *item : m_entries)
        if (item->selected())
            result.push_back(item);

    return result;
}

void MenuObject::clear_selections()
{
    for (MenuItem *item : m_entries)
        item->select(false);
}

void MenuObject::allow_focus(bool toggle)
{
    m_allow_focus = toggle;
}

bool MenuObject::can_be_focused()
{
    if (m_entries.empty())
    {
        // Do not allow focusing empty containers by default
        return false;
    }
    return m_allow_focus;
}

void MenuObject::set_visible(bool flag)
{
    m_visible = flag;
}

bool MenuObject::is_visible() const
{
    return m_visible;
}

MenuFreeform::MenuFreeform(): m_active_item(nullptr), m_default_item(nullptr)
{
}

MenuFreeform::~MenuFreeform()
{
    deleteAll(m_entries);
}

void MenuFreeform::set_default_item(MenuItem* item)
{
    m_default_item = item;
}

void MenuFreeform::activate_default_item()
{
    m_active_item = m_default_item;
}

MenuObject::InputReturnValue MenuFreeform::process_input(int key)
{
    if (!m_allow_focus || !m_visible)
        return INPUT_NO_ACTION;

    if (m_active_item == nullptr)
    {
        if (m_entries.empty())
        {
            // nothing to process
            return MenuObject::INPUT_NO_ACTION;
        }
        else if (m_default_item == nullptr)
        {
            // pick the first item possible
            for (auto mentry : m_entries)
            {
                if (mentry->can_be_highlighted())
                {
                    m_active_item = mentry;
                    break;
                }
            }
        }
    }

    if (m_active_item == nullptr && m_default_item != nullptr)
    {
        switch (key)
        {
        case CK_UP:
        case CK_DOWN:
        case CK_LEFT:
        case CK_RIGHT:
        case CK_ENTER:
            set_active_item(m_default_item);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
    }

    MenuItem* find_entry = nullptr;
    switch (key)
    {
    case CK_ENTER:
        if (m_active_item == nullptr)
            return MenuObject::INPUT_NO_ACTION;

        select_item(m_active_item);
        if (m_active_item->selected())
            return MenuObject::INPUT_SELECTED;
        else
            return MenuObject::INPUT_DESELECTED;
        break;
    case CK_UP:
        find_entry = _find_item_by_direction(m_active_item, UP);
        if (find_entry != nullptr)
        {
            set_active_item(find_entry);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
        else
            return MenuObject::INPUT_FOCUS_RELEASE_UP;
        break;
    case CK_DOWN:
        find_entry = _find_item_by_direction(m_active_item, DOWN);
        if (find_entry != nullptr)
        {
            set_active_item(find_entry);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
        else
            return MenuObject::INPUT_FOCUS_RELEASE_DOWN;
        break;
    case CK_LEFT:
        find_entry = _find_item_by_direction(m_active_item, LEFT);
        if (find_entry != nullptr)
        {
            set_active_item(find_entry);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
        else
            return MenuObject::INPUT_FOCUS_RELEASE_LEFT;
        break;
    case CK_RIGHT:
        find_entry = _find_item_by_direction(m_active_item, RIGHT);
        if (find_entry != nullptr)
        {
            set_active_item(find_entry);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
        else
            return MenuObject::INPUT_FOCUS_RELEASE_RIGHT;
        break;
    default:
        find_entry = select_item_by_hotkey(key);
        if (find_entry != nullptr)
        {
            if (find_entry->selected())
                return MenuObject::INPUT_SELECTED;
            else
                return MenuObject::INPUT_DESELECTED;
        }
        break;
    }
    return MenuObject::INPUT_NO_ACTION;
}

#ifdef USE_TILE_LOCAL
MenuObject::InputReturnValue MenuFreeform::handle_mouse(const MouseEvent& me)
{
    if (!m_allow_focus || !m_visible)
        return INPUT_NO_ACTION;

    if (!_is_mouse_in_bounds(coord_def(me.px, me.py)))
    {
        if (m_active_item != nullptr)
        {
            _set_active_item_by_index(-1);
            return INPUT_FOCUS_LOST;
        }
        else
            return INPUT_NO_ACTION;
    }

    MenuItem* find_item = _find_item_by_mouse_coords(coord_def(me.px, me.py));

    if (find_item && find_item->handle_mouse(me))
        return MenuObject::INPUT_SELECTED; // The object handled the event
    else if (me.event == MouseEvent::MOVE)
    {
        if (find_item == nullptr)
        {
            if (m_active_item != nullptr)
            {
                _set_active_item_by_index(-1);
                return INPUT_NO_ACTION;
            }
        }
        else
        {
            if (m_active_item != find_item)
            {
                set_active_item(find_item);
                return INPUT_ACTIVE_CHANGED;
            }
        }
        return INPUT_NO_ACTION;
    }
    InputReturnValue ret = INPUT_NO_ACTION;
    if (me.event == MouseEvent::PRESS)
    {
        if (me.button == MouseEvent::LEFT)
        {
            if (find_item != nullptr)
            {
                select_item(find_item);
                if (find_item->selected())
                    ret = INPUT_SELECTED;
                else
                    ret = INPUT_DESELECTED;
            }
        }
        else if (me.button == MouseEvent::RIGHT)
            ret = INPUT_END_MENU_ABORT;
    }
    // all the other Mouse Events are uninteresting and are ignored
    return ret;
}
#endif

void MenuFreeform::render()
{
    if (!m_visible)
        return;

    if (m_dirty)
        _place_items();

    for (MenuItem *item : m_entries)
        item->render();
}

/**
 * Handle all the dirtyness here that the MenuItems themselves do not handle
 */
void MenuFreeform::_place_items()
{
    m_dirty = false;
}

MenuItem* MenuFreeform::get_active_item()
{
    return m_active_item;
}

// Predicate for std::find_if
static bool _id_comparison(MenuItem* item, int ID)
{
    return item->get_id() == ID;
}

/**
 * Sets item by ID
 * Clears active item if ID not found
 */
void MenuFreeform ::set_active_item(int ID)
{
    auto it = find_if(m_entries.begin(), m_entries.end(),
                      bind(_id_comparison, placeholders::_1, ID));

    m_active_item = (it != m_entries.end()) ? *it : nullptr;
    m_dirty = true;
}

/**
 * Sets active item based on index
 * This function is for internal use if object does not have ID set
 */
void MenuFreeform::_set_active_item_by_index(int index)
{
    if (index >= 0 && index < static_cast<int> (m_entries.size()))
    {
        if (m_entries.at(index)->can_be_highlighted())
        {
            m_active_item = m_entries.at(index);
            m_dirty = true;
            return;
        }
    }
    // Clear active selection
    m_active_item = nullptr;
    m_dirty = true;
}

void MenuFreeform::set_active_item(MenuItem* item)
{
    // Does item exist in the menu?
    auto it = find(m_entries.begin(), m_entries.end(), item);
    m_active_item = (it != end(m_entries) && item->can_be_highlighted())
                    ? item : nullptr;
    m_dirty = true;
}

void MenuFreeform::activate_first_item()
{
    if (!m_entries.empty())
    {
        // find the first activeable item
        for (int i = 0; i < static_cast<int> (m_entries.size()); ++i)
        {
            if (m_entries.at(i)->can_be_highlighted())
            {
                _set_active_item_by_index(i);
                break; // escape loop
            }
        }
    }
}

void MenuFreeform::activate_last_item()
{
    if (!m_entries.empty())
    {
        // find the last activeable item
        for (int i = m_entries.size() -1; i >= 0; --i)
        {
            if (m_entries.at(i)->can_be_highlighted())
            {
                _set_active_item_by_index(i);
                break; // escape loop
            }
        }
    }
}

bool MenuFreeform::select_item(int index)
{
    if (index >= 0 && index < static_cast<int> (m_entries.size()))
    {
        // Flip the selection flag
        m_entries.at(index)->select(!m_entries.at(index)->selected());
    }
    return m_entries.at(index)->selected();
}

bool MenuFreeform::select_item(MenuItem* item)
{
    ASSERT(item != nullptr);

    // Is the given item in menu?
    auto find_val = find(m_entries.begin(), m_entries.end(), item);
    if (find_val != m_entries.end())
    {
        // Flip the selection flag
        item->select(!item->selected());
    }
    return item->selected();
}

bool MenuFreeform::attach_item(MenuItem* item)
{
    // is the item inside boundaries?
    if (   item->get_min_coord().x < m_min_coord.x
        || item->get_min_coord().x > m_max_coord.x
        || item->get_min_coord().y < m_min_coord.y
        || item->get_min_coord().y > m_max_coord.y
        || item->get_max_coord().x < m_min_coord.x
        || item->get_max_coord().x > m_max_coord.x
        || item->get_max_coord().y < m_min_coord.y
        || item->get_max_coord().y > m_max_coord.y)
    {
        return false;
    }
    // It's inside boundaries

    m_entries.push_back(item);
    return true;
}

/**
 * Finds the closest rectangle to given entry begin_index on a caardinal
 * direction from it.
 * if no entries are found, -1 is returned
 */
MenuItem* MenuFreeform::_find_item_by_direction(const MenuItem* start,
                                                MenuObject::Direction dir)
{
    if (start == nullptr)
        return nullptr;

    coord_def aabb_start(0,0);
    coord_def aabb_end(0,0);

    // construct the aabb
    switch (dir)
    {
    case UP:
        if (start->get_link_up())
            return start->get_link_up();

        aabb_start.x = start->get_min_coord().x;
        aabb_end.x = start->get_max_coord().x;
        aabb_start.y = 0; // top of screen
        aabb_end.y = start->get_min_coord().y;
        break;
    case DOWN:
        if (start->get_link_down())
            return start->get_link_down();

        aabb_start.x = start->get_min_coord().x;
        aabb_end.x = start->get_max_coord().x;
        aabb_start.y = start->get_max_coord().y;
        // we choose an arbitrarily large number here, because
        // tiles saves entry coordinates in pixels, yet console saves them
        // in characters
        // basically, we want the AABB to be large enough to extend to the
        // bottom of the screen in every possible resolution
        aabb_end.y = 32767;
        break;
    case LEFT:
        if (start->get_link_left())
            return start->get_link_left();

        aabb_start.x = 0; // left of screen
        aabb_end.x = start->get_min_coord().x;
        aabb_start.y = start->get_min_coord().y;
        aabb_end.y = start->get_max_coord().y;
        break;
    case RIGHT:
        if (start->get_link_right())
            return start->get_link_right();

        aabb_start.x = start->get_max_coord().x;
        // we again want a value that is always larger then the width of screen
        aabb_end.x = 32767;
        aabb_start.y = start->get_min_coord().y;
        aabb_end.y = start->get_max_coord().y;
        break;
    default:
        die("Bad direction given");
    }

    // loop through the entries
    // save the currently closest to the index in a variable
    MenuItem* closest = nullptr;
    for (MenuItem *item : m_entries)
    {
        if (!item->can_be_highlighted())
        {
            // this is a noselect entry, skip it
            continue;
        }
        if (!item->is_visible())
        {
            // this item is not visible, skip it
            continue;
        }
        if (!_AABB_intersection(item->get_min_coord(), item->get_max_coord(),
                                aabb_start, aabb_end))
        {
            continue; // does not intersect, continue loop
        }

        // intersects
        // check if it's closer than current
        if (closest == nullptr)
            closest = item;

        switch (dir)
        {
        case UP:
            if (item->get_min_coord().y > closest->get_min_coord().y)
                closest = item;
            break;
        case DOWN:
            if (item->get_min_coord().y < closest->get_min_coord().y)
                closest = item;
            break;
        case LEFT:
            if (item->get_min_coord().x > closest->get_min_coord().x)
                closest = item;
            break;
        case RIGHT:
            if (item->get_min_coord().x < closest->get_min_coord().x)
                closest = item;
        }
    }
    // TODO handle special cases here, like pressing down on the last entry
    // to go the the first item in that line
    return closest;
}

MenuScroller::MenuScroller(): m_topmost_visible(0), m_currently_active(0),
                              m_items_shown(0)
{
#ifdef USE_TILE_LOCAL
    m_arrow_up = new TextTileItem();
    m_arrow_down = new TextTileItem();
    m_arrow_up->add_tile(tile_def(TILE_MI_ARROW0, TEX_DEFAULT));
    m_arrow_down->add_tile(tile_def(TILE_MI_ARROW4, TEX_DEFAULT));
#endif
}

MenuScroller::~MenuScroller()
{
#ifdef USE_TILE_LOCAL
    delete m_arrow_up;
    delete m_arrow_down;
#endif
    deleteAll(m_entries);
}

MenuObject::InputReturnValue MenuScroller::process_input(int key)
{
    if (!m_allow_focus || !m_visible)
        return INPUT_NO_ACTION;

    if (m_currently_active < 0)
    {
        if (m_entries.empty())
        {
            // nothing to process
            return MenuObject::INPUT_NO_ACTION;
        }
        else
        {
            // pick the first item possible
            for (unsigned int i = 0; i < m_entries.size(); ++i)
            {
                if (m_entries.at(i)->can_be_highlighted())
                {
                    _set_active_item_by_index(i);
                    break;
                }
            }
        }
    }

    MenuItem* find_entry = nullptr;
    switch (key)
    {
    case CK_ENTER:
        if (m_currently_active < 0)
            return MenuObject::INPUT_NO_ACTION;

        select_item(m_currently_active);
        if (get_active_item()->selected())
            return MenuObject::INPUT_SELECTED;
        else
            return MenuObject::INPUT_DESELECTED;
        break;
    case CK_UP:
    case CONTROL('K'):
    case CONTROL('P'):
        find_entry = _find_item_by_direction(m_currently_active, UP);
        if (find_entry != nullptr)
        {
            set_active_item(find_entry);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
        else
            return MenuObject::INPUT_FOCUS_RELEASE_UP;
        break;
    case CK_DOWN:
    case CONTROL('J'):
    case CONTROL('N'):
        find_entry = _find_item_by_direction(m_currently_active, DOWN);
        if (find_entry != nullptr)
        {
            set_active_item(find_entry);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
        else
            return MenuObject::INPUT_FOCUS_RELEASE_DOWN;
        break;
    case CK_LEFT:
        return MenuObject::INPUT_FOCUS_RELEASE_LEFT;
    case CK_RIGHT:
        return MenuObject::INPUT_FOCUS_RELEASE_RIGHT;
    case CK_PGUP:
        if (m_currently_active != m_topmost_visible)
        {
            set_active_item(m_topmost_visible);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
        else
        {
            if (m_currently_active == 0)
                return MenuObject::INPUT_FOCUS_RELEASE_UP;
            else
            {
                int new_active = m_currently_active - m_items_shown;
                if (new_active < 0)
                    new_active = 0;
                set_active_item(new_active);
                return MenuObject::INPUT_ACTIVE_CHANGED;
            }
        }
    case CK_PGDN:
    {
        int last_item_visible = m_topmost_visible + m_items_shown - 1;
        int last_menu_item = m_entries.size() - 1;
        if (last_item_visible > m_currently_active)
        {
            set_active_item(last_item_visible);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
        else if (m_currently_active == last_menu_item)
            return MenuObject::INPUT_FOCUS_RELEASE_DOWN;
        else
        {
            int new_active = m_currently_active + m_items_shown - 1;
            if (new_active > last_menu_item)
                new_active = last_menu_item;
            set_active_item(new_active);
            return MenuObject::INPUT_ACTIVE_CHANGED;
        }
    }
    default:
        find_entry = select_item_by_hotkey(key);
        if (find_entry != nullptr)
        {
            if (find_entry->selected())
                return MenuObject::INPUT_SELECTED;
            else
                return MenuObject::INPUT_DESELECTED;
        }
        break;
    }
    return MenuObject::INPUT_NO_ACTION;
}

#ifdef USE_TILE_LOCAL
MenuObject::InputReturnValue MenuScroller::handle_mouse(const MouseEvent &me)
{
    if (!m_allow_focus || !m_visible)
        return INPUT_NO_ACTION;

    if (!_is_mouse_in_bounds(coord_def(me.px, me.py)))
    {
        if (m_currently_active >= 0)
        {
            _set_active_item_by_index(-1);
            return INPUT_FOCUS_LOST;
        }
        else
            return INPUT_NO_ACTION;
    }

    MenuItem* find_item = nullptr;

    if (me.event == MouseEvent::MOVE)
    {
        find_item = _find_item_by_mouse_coords(coord_def(me.px, me.py));
        if (find_item == nullptr)
        {
            if (m_currently_active >= 0)
            {
                // Do not signal on cleared active events
                _set_active_item_by_index(-1);
                return INPUT_NO_ACTION;
            }
        }
        else
        {
            if (m_currently_active >= 0)
            {
                // prevent excess _place_item calls if the mouse over is already
                // active
                if (find_item != m_entries.at(m_currently_active))
                {
                    set_active_item(find_item);
                    return INPUT_ACTIVE_CHANGED;
                }
                else
                    return INPUT_NO_ACTION;
            }
            else
            {
                set_active_item(find_item);
                return INPUT_ACTIVE_CHANGED;
            }

        }
        return INPUT_NO_ACTION;
    }

    if (me.event == MouseEvent::PRESS && me.button == MouseEvent::LEFT)
    {
        find_item = _find_item_by_mouse_coords(coord_def(me.px,
                                                        me.py));
        if (find_item != nullptr)
        {
            select_item(find_item);
            if (find_item->selected())
                return INPUT_SELECTED;
            else
                return INPUT_DESELECTED;
        }
        else
        {
            // handle clicking on the scrollbar (top half of region => scroll up)
            if (static_cast<int>(me.py)-m_min_coord.y > (m_max_coord.y-m_min_coord.y)/2)
                return process_input(CK_DOWN);
            else
                return process_input(CK_UP);
        }
    }
    else if (me.event == MouseEvent::PRESS && me.button == MouseEvent::RIGHT)
        return INPUT_END_MENU_ABORT;
    // all the other Mouse Events are uninteresting and are ignored
    return INPUT_NO_ACTION;
}
#endif

void MenuScroller::render()
{
    if (!m_visible)
        return;

    if (m_dirty)
        _place_items();

    for (MenuItem *item : m_entries)
       item->render();

#ifdef USE_TILE_LOCAL
    // draw scrollbar
    m_arrow_up->render();
    m_arrow_down->render();
#endif
}

MenuItem* MenuScroller::get_active_item()
{
    if (m_currently_active >= 0
        && m_currently_active < static_cast<int> (m_entries.size()))
    {
        return m_entries.at(m_currently_active);
    }
    return nullptr;
}

void MenuScroller::set_active_item(int ID)
{
    if (m_currently_active >= 0)
    {
        if (m_entries.at(m_currently_active)->get_id() == ID)
        {
            // prevent useless _place_items
            return;
        }
    }

    auto it = find_if(m_entries.begin(), m_entries.end(),
                      bind(_id_comparison, placeholders::_1, ID));
    if (it != m_entries.end())
    {
        set_active_item(*it);
        return;
    }
    m_currently_active = 0;
    m_dirty = true;
    return;
}

void MenuScroller::_set_active_item_by_index(int index)
{
    // prevent useless _place_items
    if (index == m_currently_active)
        return;

    if (index >= 0 && index < static_cast<int> (m_entries.size()))
    {
        m_currently_active = index;
        if (m_currently_active < m_topmost_visible)
            m_topmost_visible = m_currently_active;
    }
    else
        m_currently_active = -1;
    m_dirty = true;
}

void MenuScroller::set_active_item(MenuItem* item)
{
    if (item == nullptr)
    {
        _set_active_item_by_index(-1);
        return;
    }

    for (int i = 0; i < static_cast<int> (m_entries.size()); ++i)
    {
        if (item == m_entries.at(i))
        {
            _set_active_item_by_index(i);
            return;
        }
    }
    _set_active_item_by_index(-1);
}

void MenuScroller::activate_first_item()
{
    if (!m_entries.empty())
    {
        // find the first activeable item
        for (int i = 0; i < static_cast<int> (m_entries.size()); ++i)
        {
            if (m_entries.at(i)->can_be_highlighted())
            {
                _set_active_item_by_index(i);
                break; // escape loop
            }
        }
    }
}

void MenuScroller::activate_last_item()
{
    if (!m_entries.empty())
    {
        // find the last activeable item
        for (int i = m_entries.size() -1; i >= 0; --i)
        {
            if (m_entries.at(i)->can_be_highlighted())
            {
                _set_active_item_by_index(i);
                break; // escape loop
            }
        }
    }
}

bool MenuScroller::select_item(int index)
{
    if (index >= 0 && index < static_cast<int> (m_entries.size()))
    {
        // Flip the flag
        m_entries.at(index)->select(!m_entries.at(index)->selected());
        return m_entries.at(index)->selected();
    }
    return false;
}

bool MenuScroller::select_item(MenuItem* item)
{
    ASSERT(item != nullptr);
    // Is the item in the menu?
    for (int i = 0; i < static_cast<int> (m_entries.size()); ++i)
    {
        if (item == m_entries.at(i))
            return select_item(i);
    }
    return false;
}

bool MenuScroller::attach_item(MenuItem* item)
{
    // _place_entries controls visibility, hide it until it's processed
    item->set_visible(false);
    m_entries.push_back(item);
    m_dirty = true;
    return true;
}

/**
 * Changes the bounds of the items that are to be visible
 * preserves user set item heigth
 * does not preserve width
 */
void MenuScroller::_place_items()
{
    m_dirty = false;
    m_items_shown = 0;

    int item_height = 0;
    int space_used = 0;
    int one_past_last = 0;
    const int space_available = m_max_coord.y - m_min_coord.y;
    coord_def min_coord(0,0);
    coord_def max_coord(0,0);

    // Hide all the items
    for (MenuItem *item : m_entries)
    {
        item->set_visible(false);
        item->allow_highlight(false);
    }

    // calculate how many entries we can fit
    for (one_past_last = m_topmost_visible;
         one_past_last < static_cast<int> (m_entries.size());
         ++one_past_last)
    {
        space_used += m_entries.at(one_past_last)->get_max_coord().y
                      - m_entries.at(one_past_last)->get_min_coord().y;
        if (space_used > space_available)
        {
            if (m_currently_active < 0)
                break; // all space allocated
            if (one_past_last > m_currently_active)
                break; // we included our active one, ok!
            else
            {
                // active one didn't fit, chop the first one and run the loop
                // again
                ++m_topmost_visible;
                one_past_last = m_topmost_visible - 1;
                space_used = 0;
                continue;
            }
        }
    }

    space_used = 0;
    int work_index = 0;

    for (work_index = m_topmost_visible; work_index < one_past_last;
         ++work_index)
    {
        min_coord = m_entries.at(work_index)->get_min_coord();
        max_coord = m_entries.at(work_index)->get_max_coord();
        item_height = max_coord.y - min_coord.y;

        min_coord.y = m_min_coord.y + space_used;
        max_coord.y = min_coord.y + item_height;
        min_coord.x = m_min_coord.x;
        max_coord.x = m_max_coord.x;
#ifdef USE_TILE_LOCAL
        // reserve one tile space for scrollbar
        max_coord.x -= 32;
#endif
        m_entries.at(work_index)->set_bounds_no_multiply(min_coord, max_coord);
        m_entries.at(work_index)->set_visible(true);
        m_entries.at(work_index)->allow_highlight(true);
        space_used += item_height;
        ++m_items_shown;
    }

#ifdef USE_TILE_LOCAL
    // arrows
    m_arrow_down->set_bounds_no_multiply(coord_def(m_max_coord.x-32,m_max_coord.y-32),coord_def(m_max_coord.x,m_max_coord.y));
    m_arrow_down->set_visible(m_topmost_visible + m_items_shown < (int)m_entries.size());

    m_arrow_up->set_bounds_no_multiply(coord_def(m_max_coord.x-32,m_min_coord.y),coord_def(m_max_coord.x,m_min_coord.y+32));
    m_arrow_up->set_visible(m_topmost_visible>0);
#endif
}

MenuItem* MenuScroller::_find_item_by_direction(int start_index,
                                          MenuObject::Direction dir)
{
    MenuItem* find_item = nullptr;
    switch (dir)
    {
    case UP:
        if ((start_index - 1) >= 0)
            find_item = m_entries.at(start_index - 1);
        break;
    case DOWN:
        if ((start_index + 1) < static_cast<int> (m_entries.size()))
            find_item = m_entries.at(start_index + 1);
        break;
    default:
        break;
    }
    return find_item;
}

MenuDescriptor::MenuDescriptor(PrecisionMenu* parent): m_parent(parent),
    m_active_item(nullptr), override_text("")
{
    ASSERT(m_parent != nullptr);
}

MenuDescriptor::~MenuDescriptor()
{
}

vector<MenuItem*> MenuDescriptor::get_selected_items()
{
    vector<MenuItem*> ret_val;
    return ret_val;
}

void MenuDescriptor::init(const coord_def& min_coord, const coord_def& max_coord,
                          const string& name)
{
    MenuObject::init(min_coord, max_coord, name);
    m_desc_item.set_bounds(min_coord, max_coord);
    m_desc_item.set_fg_colour(WHITE);
    m_desc_item.set_visible(true);
}

MenuObject::InputReturnValue MenuDescriptor::process_input(int key)
{
    // just in case we somehow end up processing input of this item
    return MenuObject::INPUT_NO_ACTION;
}

#ifdef USE_TILE_LOCAL
MenuObject::InputReturnValue MenuDescriptor::handle_mouse(const MouseEvent &me)
{
    if (me.event == MouseEvent::PRESS && me.button == MouseEvent::RIGHT)
        return INPUT_END_MENU_ABORT;
    // we have nothing interesting to do on mouse events because render()
    // always checks if the active has changed override for things like
    // tooltips
    return INPUT_NO_ACTION;
}
#endif

void MenuDescriptor::render()
{
    if (!m_visible)
        return;

    _place_items();

    m_desc_item.render();
}

/**
 * This allows an arbitrary string to show up in the descriptor, temporarily
 * overriding whatever is there. The override will last until the descriptor
 * next changes. Empty strings are not used.
 *
 * @param t a string to show in the MenuDescriptor. The empty string will clear
 *          any existing override.
 */
void MenuDescriptor::override_description(const string &t)
{
    override_text = t;
    render();
}


void MenuDescriptor::_place_items()
{
    MenuItem* tmp = m_parent->get_active_item();
    if (tmp != m_active_item)
    {
        // the active item has changed -- update
        m_active_item = tmp;
        override_text = "";
#ifndef USE_TILE_LOCAL
        textcolour(BLACK);
        textbackground(BLACK);
        for (int i = 0; i < m_desc_item.get_max_coord().y
                            - m_desc_item.get_min_coord().y; ++i)
        {
            cgotoxy(m_desc_item.get_min_coord().x,
                    m_desc_item.get_min_coord().y + i);
            clear_to_end_of_line();
        }
        textcolour(LIGHTGRAY);
#endif

        if (tmp == nullptr)
             m_desc_item.set_text("");
        else
            m_desc_item.set_text(m_active_item->get_description_text());
    }
    else if (override_text.size() > 0)
        m_desc_item.set_text(override_text);
}

BoxMenuHighlighter::BoxMenuHighlighter(PrecisionMenu *parent): m_parent(parent),
    m_active_item(nullptr)
{
    ASSERT(parent != nullptr);
}

BoxMenuHighlighter::~BoxMenuHighlighter()
{
}

vector<MenuItem*> BoxMenuHighlighter::get_selected_items()
{
    vector<MenuItem*> ret_val;
    return ret_val;
}

MenuObject::InputReturnValue BoxMenuHighlighter::process_input(int key)
{
    // just in case we somehow end up processing input of this item
    return MenuObject::INPUT_NO_ACTION;
}

#ifdef USE_TILE_LOCAL
MenuObject::InputReturnValue BoxMenuHighlighter::handle_mouse(const MouseEvent &me)
{
    // we have nothing interesting to do on mouse events because render()
    // always checks if the active has changed
    return MenuObject::INPUT_NO_ACTION;
}
#endif

void BoxMenuHighlighter::render()
{
    if (!m_visible)
        return;

    if (!m_visible)
        return;
    _place_items();
#ifdef USE_TILE_LOCAL
    m_line_buf.draw();
    m_shape_buf.draw();
#else
    if (m_active_item != nullptr)
        m_active_item->render();
#endif
}

void BoxMenuHighlighter::_place_items()
{
    MenuItem* tmp = m_parent->get_active_item();
    if (tmp == m_active_item)
        return;

#ifdef USE_TILE_LOCAL
    m_line_buf.clear();
    m_shape_buf.clear();
    if (tmp != nullptr)
    {
        const VColour& c = term_colours[tmp->get_highlight_colour()];
        const VColour bg_colour(c.r, c.g, c.b, 80);
        const VColour line_colour(c.r, c.g, c.b, 127);
        const coord_def tl = tmp->get_min_coord() + coord_def(1, 1);
        const coord_def br = tmp->get_max_coord();
        m_line_buf.add_square(tl.x, tl.y, br.x, br.y, line_colour);
        m_shape_buf.add(tl.x, tl.y, br.x, br.y, bg_colour);
    }
#else
    // we had an active item before
    if (m_active_item != nullptr)
    {
        // clear the background highlight trickery
        m_active_item->set_bg_colour(m_old_bg_colour);
        // redraw the old item
        m_active_item->render();
    }
    if (tmp != nullptr)
    {
        m_old_bg_colour = tmp->get_bg_colour();
        tmp->set_bg_colour(tmp->get_highlight_colour());
    }
#endif
    m_active_item = tmp;
}

BlackWhiteHighlighter::BlackWhiteHighlighter(PrecisionMenu* parent):
    BoxMenuHighlighter(parent)
{
    ASSERT(m_parent != nullptr);
}

BlackWhiteHighlighter::~BlackWhiteHighlighter()
{
}

void BlackWhiteHighlighter::render()
{
    if (!m_visible)
        return;

    _place_items();

    if (m_active_item != nullptr)
    {
#ifdef USE_TILE_LOCAL
        m_shape_buf.draw();
#endif
        m_active_item->render();
    }
}

void BlackWhiteHighlighter::_place_items()
{
    MenuItem* tmp = m_parent->get_active_item();
    if (tmp == m_active_item)
        return;

#ifdef USE_TILE_LOCAL
    m_shape_buf.clear();
#endif
    // we had an active item before
    if (m_active_item != nullptr)
    {
        // clear the highlight trickery
        m_active_item->set_fg_colour(m_old_fg_colour);
        m_active_item->set_bg_colour(m_old_bg_colour);
        // redraw the old item
        m_active_item->render();
    }
    if (tmp != nullptr)
    {
#ifdef USE_TILE_LOCAL
        m_shape_buf.add(tmp->get_min_coord().x, tmp->get_min_coord().y,
                        tmp->get_max_coord().x, tmp->get_max_coord().y,
                        term_colours[LIGHTGRAY]);
#endif
        m_old_bg_colour = tmp->get_bg_colour();
        m_old_fg_colour = tmp->get_fg_colour();
        tmp->set_bg_colour(LIGHTGRAY);
        tmp->set_fg_colour(BLACK);
    }
    m_active_item = tmp;
}
