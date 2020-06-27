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
 #include "rltiles/tiledef-dngn.h"
 #include "rltiles/tiledef-icons.h"
 #include "rltiles/tiledef-main.h"
 #include "rltiles/tiledef-player.h"
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
#ifdef USE_TILE_LOCAL
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
    virtual bool on_event(const Event& event) override;
    int get_num_columns() const { return m_num_columns; };
    void set_num_columns(int n) {
        m_num_columns = n;
        _invalidate_sizereq();
        _queue_allocation();
    };
#endif

    void update_item(int index);
    void update_items();

    void is_visible_item_range(int *vis_min, int *vis_max);
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

void UIMenu::is_visible_item_range(int *vis_min, int *vis_max)
{
    const int viewport_height = m_menu->m_ui.scroller->get_region().height;
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
#else
    UNUSED(index);
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
    GLW_3VF t = {(float)m_region.x, (float)m_region.y, 0}, s = {1, 1, 1};
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
    is_visible_item_range(&vis_min, &vis_max);
    const int scroll = m_menu->m_ui.scroller->get_scroll();

    for (int i = vis_min; i < vis_max; i++)
    {
        const MenuEntry *me = m_menu->items[i];
        int y = i - vis_min + 1;
        cgotoxy(m_region.x+1, m_region.y+scroll+y);
        const int col = m_menu->item_colour(me);
        textcolour(col);
        const bool needs_cursor = (m_menu->get_cursor() == i && m_menu->is_set(MF_MULTISELECT));

        if (m_menu->get_flags() & MF_ALLOW_FORMATTING)
        {
            formatted_string s = formatted_string::parse_string(
                me->get_text(needs_cursor), col);
            s.chop(m_region.width).display();
        }
        else
        {
            string text = me->get_text(needs_cursor);
            text = chop_string(text, m_region.width);
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
        const int em = Options.tile_font_crt_size;
        int max_menu_width = min(93*em, m_nat_column_width * m_num_columns);
        return {0, max_menu_width};
    }
    else
    {
        do_layout(prosp_width, m_num_columns);
        return {0, m_height};
    }
#else
    UNUSED(prosp_width);
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
        m_wrapped_size = Size(-1);
        wrap_text_to_size(m_region.width, m_region.height);
    };
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

    int max_height = m_menu->m_ui.popup->get_max_child_size().height;
    max_height -= m_menu->m_ui.title->get_region().height;
    max_height -= m_menu->m_ui.title->get_margin().bottom;
    int viewport_height = m_menu->m_ui.scroller->get_region().height;

#ifdef USE_TILE_LOCAL
    int menu_w = m_menu->m_ui.menu->get_region().width;
    m_menu->m_ui.menu->do_layout(menu_w, 1);
    int m_height = m_menu->m_ui.menu->m_height;

    int more_height = m_menu->m_ui.more->get_region().height;
    // switch number of columns
    int num_cols = m_menu->m_ui.menu->get_num_columns();
    if (m_menu->m_ui.menu->m_draw_tiles && m_menu->is_set(MF_USE_TWO_COLUMNS)
        && !Options.tile_single_column_menus)
    {
        if ((num_cols == 1 && m_height+more_height > max_height)
         || (num_cols == 2 && m_height+more_height <= max_height))
        {
            m_menu->m_ui.menu->set_num_columns(3 - num_cols);
            ui::restart_layout();
        }
    }
    m_menu->m_ui.menu->do_layout(menu_w, num_cols);
#endif

#ifndef USE_TILE_LOCAL
    int menu_height = m_menu->m_ui.menu->get_region().height;

    // change more visibility
    bool can_toggle_more = !m_menu->is_set(MF_ALWAYS_SHOW_MORE)
        && !m_menu->m_ui.more->get_text().ops.empty();
    if (can_toggle_more)
    {
        bool more_visible = m_menu->m_ui.more->is_visible();
        if (more_visible ? menu_height <= max_height : menu_height > max_height)
        {
            m_menu->m_ui.more->set_visible(!more_visible);
            _invalidate_sizereq();
            m_menu->m_ui.more->_queue_allocation();
            ui::restart_layout();
        }
    }

    if (m_menu->m_keyhelp_more && m_menu->m_ui.more->is_visible())
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
    m_menu->m_ui.scroller->max_size().height = max_viewport_height;
    if (max_viewport_height < viewport_height)
    {
        m_menu->m_ui.scroller->_invalidate_sizereq();
        m_menu->m_ui.scroller->_queue_allocation();
        ui::restart_layout();
    }
}

void UIMenu::_allocate_region()
{
#ifndef USE_TILE_LOCAL
    // XXX: is this needed?
    m_height = m_menu->items.size();
#else
    do_layout(m_region.width, m_num_columns);
    update_hovered_entry();
    pack_buffers();
#endif
}

#ifdef USE_TILE_LOCAL
void UIMenu::update_hovered_entry()
{
    const int x = m_mouse_x - m_region.x,
              y = m_mouse_y - m_region.y;
    int vis_min, vis_max;
    is_visible_item_range(&vis_min, &vis_max);

    for (int i = vis_min; i < vis_max; ++i)
    {
        const auto& entry = item_info[i];
        if (entry.heading)
            continue;
        const auto me = m_menu->items[i];
        if (me->hotkeys.size() == 0)
            continue;
        const int w = m_region.width / m_num_columns;
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

bool UIMenu::on_event(const Event& ev)
{
    if (Widget::on_event(ev))
        return true;

    if (ev.type() != Event::Type::MouseMove
     && ev.type() != Event::Type::MouseDown
     && ev.type() != Event::Type::MouseUp
     && ev.type() != Event::Type::MouseEnter
     && ev.type() != Event::Type::MouseLeave)
    {
        return false;
    }

    auto event = static_cast<const MouseEvent&>(ev);

    m_mouse_x = event.x();
    m_mouse_y = event.y();

    if (event.type() == Event::Type::MouseEnter)
    {
        do_layout(m_region.width, m_num_columns);
        update_hovered_entry();
        pack_buffers();
        _expose();
        return false;
    }

    if (event.type() == Event::Type::MouseLeave)
    {
        wm->set_mouse_cursor(MOUSE_CURSOR_ARROW);
        m_mouse_x = -1;
        m_mouse_y = -1;
        m_mouse_pressed = false;
        m_mouse_idx = -1;
        do_layout(m_region.width, m_num_columns);
        pack_buffers();
        _expose();
        return false;
    }

    if (event.type() == Event::Type::MouseMove)
    {
        do_layout(m_region.width, m_num_columns);
        update_hovered_entry();
        pack_buffers();
        _expose();
        return true;
    }

    int key = -1;
    if (event.type() ==  Event::Type::MouseDown
        && event.button() == MouseEvent::Button::Left)
    {
        m_mouse_pressed = true;
        _queue_allocation();
    }
    else if (event.type() == Event::Type::MouseUp
            && event.button() == MouseEvent::Button::Left
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
        wm_keyboard_event wm_ev = {0};
        wm_ev.keysym.sym = key;
        KeyEvent key_ev(Event::Type::KeyDown, wm_ev);
        m_menu->m_ui.popup->on_event(key_ev);
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

    const int col_width = m_region.width / m_num_columns;

    int vis_min, vis_max;
    is_visible_item_range(&vis_min, &vis_max);

    for (int i = vis_min; i < vis_max; ++i)
    {
        const auto& entry = item_info[i];
        const auto me = m_menu->items[i];
        const int entry_x = entry.column * col_width;
        const int entry_ex = entry_x + col_width;
        const int entry_h = row_heights[entry.row+1] - row_heights[entry.row];

        if (entry.heading)
        {
            formatted_string split = m_font_entry->split(entry.text, m_region.width, entry_h);
            // see corresponding section in do_layout()
            int line_y = entry.y  + (i == 0 ? 0 : 5) + item_pad;
            if (i < (int)item_info.size()-1 && !item_info[i+1].heading)
            {
                m_div_line_buf.add_square(entry.x, line_y,
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
                const auto tex = get_tile_texture(tile.tile);
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
    m_ui.more->set_visible(false);
    m_ui.vbox = make_shared<Box>(Widget::VERT);
    m_ui.vbox->set_cross_alignment(Widget::STRETCH);

    m_ui.vbox->add_child(m_ui.title);
#ifdef USE_TILE_LOCAL
    m_ui.vbox->add_child(m_ui.scroller);
#else
    auto scroller_wrap = make_shared<Box>(Widget::VERT, Box::Expand::EXPAND_V);
    scroller_wrap->set_cross_alignment(Widget::STRETCH);
    scroller_wrap->add_child(m_ui.scroller);
    m_ui.vbox->add_child(scroller_wrap);
#endif
    m_ui.vbox->add_child(m_ui.more);
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

void Menu::set_flags(int new_flags)
{
    flags = new_flags;

#ifdef DEBUG
    int sel_flag = flags & (MF_NOSELECT | MF_SINGLESELECT | MF_MULTISELECT);
    ASSERT(sel_flag == MF_NOSELECT || sel_flag == MF_SINGLESELECT || sel_flag == MF_MULTISELECT);
#endif
}

bool Menu::minus_is_pageup() const
{
    return !is_set(MF_MULTISELECT) && !is_set(MF_SPECIAL_MINUS);
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
    string pageup_keys = minus_is_pageup() ? "<w>-</w>|<w><<</w>" : "<w><<</w>";
    more = formatted_string::parse_string(
        "<lightgrey>[<w>+</w>|<w>></w>|<w>Space</w>]: page down        "
        "[" + pageup_keys + "]: page up        "
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

    return sel;
}

void Menu::do_menu()
{
    bool done = false;
    m_ui.popup = make_shared<UIMenuPopup>(m_ui.vbox, this);

    m_ui.popup->on_keydown_event([this, &done](const KeyEvent& ev) {
        if (m_filter)
        {
            int key = m_filter->putkey(ev.key());
            if (key != -1)
            {
                delete m_filter;
                m_filter = nullptr;
            }
            update_title();
            return true;
        }
        done = !process_key(ev.key());
        return true;
    });
#ifdef TOUCH_UI
    auto menu_wrap_click = [this, &done](const MouseEvent& ev) {
        if (!m_filter && ev.button() == MouseEvent::Button::Left)
        {
            done = !process_key(CK_TOUCH_DUMMY);
            return true;
        }
        return false;
    };
    m_ui.title->on_mousedown_event(menu_wrap_click);
    m_ui.more->on_mousedown_event(menu_wrap_click);
#endif

    update_menu();
    ui::push_layout(m_ui.popup, m_kmc);

#ifdef USE_TILE_WEB
    tiles.push_menu(this);
    _webtiles_title_changed = false;
    m_ui.popup->on_layout_pop([](){ tiles.pop_menu(); });
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
    UNUSED(prompt);
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
    case ' ': case CK_PGDN: case '>': case '+':
    case CK_MOUSE_B1:
    case CK_MOUSE_CLICK:
        if (!page_down() && is_set(MF_WRAP))
            m_ui.scroller->set_scroll(0);
        break;
    case CK_PGUP: case '<':
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
bool MenuEntry::get_tiles(vector<tile_def>& /*tileset*/) const { return false; }
#endif

void MenuEntry::add_tile(tile_def tile)
{
#ifdef USE_TILE
    tiles.push_back(tile);
#else
    UNUSED(tile);
#endif
}

#ifdef USE_TILE
PlayerMenuEntry::PlayerMenuEntry(const string &str) :
    MenuEntry(str, MEL_ITEM, 1)
{
    quantity = 1;
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

    tileset.emplace_back(ch);

    if (m->attitude == ATT_FRIENDLY)
        tileset.emplace_back(TILE_HALO_FRIENDLY);
    else if (m->attitude == ATT_GOOD_NEUTRAL || m->attitude == ATT_STRICT_NEUTRAL)
        tileset.emplace_back(TILE_HALO_GD_NEUTRAL);
    else if (m->neutral())
        tileset.emplace_back(TILE_HALO_NEUTRAL);
    else
        switch (m->threat)
        {
        case MTHRT_TRIVIAL:
            if (Options.tile_show_threat_levels.find("trivial") != string::npos)
                tileset.emplace_back(TILE_THREAT_TRIVIAL);
            break;
        case MTHRT_EASY:
            if (Options.tile_show_threat_levels.find("easy") != string::npos)
                tileset.emplace_back(TILE_THREAT_EASY);
            break;
        case MTHRT_TOUGH:
            if (Options.tile_show_threat_levels.find("tough") != string::npos)
                tileset.emplace_back(TILE_THREAT_TOUGH);
            break;
        case MTHRT_NASTY:
            if (Options.tile_show_threat_levels.find("nasty") != string::npos)
                tileset.emplace_back(TILE_THREAT_NASTY);
            break;
        default:
            break;
        }

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

        tileset.emplace_back(tileidx_item(item));
        tileset.emplace_back(TILEI_ANIMATED_WEAPON);
    }
    else if (mons_is_draconian(m->type))
    {
        tileset.emplace_back(tileidx_draco_base(*m));
        const tileidx_t job = tileidx_draco_job(*m);
        if (job)
            tileset.emplace_back(job);
    }
    else if (mons_is_demonspawn(m->type))
    {
        tileset.emplace_back(tileidx_demonspawn_base(*m));
        const tileidx_t job = tileidx_demonspawn_job(*m);
        if (job)
            tileset.emplace_back(job);
    }
    else
    {
        tileidx_t idx = tileidx_monster(*m) & TILE_FLAG_MASK;
        tileset.emplace_back(idx);
    }

    // A fake monster might not have its ghost member set up properly.
    if (!fake && m->ground_level())
    {
        if (ch == TILE_DNGN_LAVA)
            tileset.emplace_back(TILEI_MASK_LAVA);
        else if (ch == TILE_DNGN_SHALLOW_WATER)
            tileset.emplace_back(TILEI_MASK_SHALLOW_WATER);
        else if (ch == TILE_DNGN_DEEP_WATER)
            tileset.emplace_back(TILEI_MASK_DEEP_WATER);
        else if (ch == TILE_DNGN_SHALLOW_WATER_MURKY)
            tileset.emplace_back(TILEI_MASK_SHALLOW_WATER_MURKY);
        else if (ch == TILE_DNGN_DEEP_WATER_MURKY)
            tileset.emplace_back(TILEI_MASK_DEEP_WATER_MURKY);
    }

    string damage_desc;
    mon_dam_level_type damage_level = m->dam;

    switch (damage_level)
    {
    case MDAM_DEAD:
    case MDAM_ALMOST_DEAD:
        tileset.emplace_back(TILEI_MDAM_ALMOST_DEAD);
        break;
    case MDAM_SEVERELY_DAMAGED:
        tileset.emplace_back(TILEI_MDAM_SEVERELY_DAMAGED);
        break;
    case MDAM_HEAVILY_DAMAGED:
        tileset.emplace_back(TILEI_MDAM_HEAVILY_DAMAGED);
        break;
    case MDAM_MODERATELY_DAMAGED:
        tileset.emplace_back(TILEI_MDAM_MODERATELY_DAMAGED);
        break;
    case MDAM_LIGHTLY_DAMAGED:
        tileset.emplace_back(TILEI_MDAM_LIGHTLY_DAMAGED);
        break;
    case MDAM_OKAY:
    default:
        // no flag for okay.
        break;
    }

    if (m->attitude == ATT_FRIENDLY)
        tileset.emplace_back(TILEI_FRIENDLY);
    else if (m->attitude == ATT_GOOD_NEUTRAL || m->attitude == ATT_STRICT_NEUTRAL)
        tileset.emplace_back(TILEI_GOOD_NEUTRAL);
    else if (m->neutral())
        tileset.emplace_back(TILEI_NEUTRAL);
    else if (m->is(MB_FLEEING))
        tileset.emplace_back(TILEI_FLEEING);
    else if (m->is(MB_STABBABLE))
        tileset.emplace_back(TILEI_STAB_BRAND);
    else if (m->is(MB_DISTRACTED))
        tileset.emplace_back(TILEI_MAY_STAB_BRAND);

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
    tileset.emplace_back(tile);

    if (in_bounds(pos) && is_unknown_stair(pos))
        tileset.emplace_back(TILEI_NEW_STAIR);

    if (in_bounds(pos) && is_unknown_transporter(pos))
        tileset.emplace_back(TILEI_NEW_TRANSPORTER);

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

        tileset.emplace_back(idx, ymax);
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
#else
    UNUSED(update_entries);
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
    m_ui.more->set_visible(show_more);

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

        fs.textcolour(col);

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
    const bool tile_indent = m_indent_title && Options.tile_menu_icons;
    m_ui.title->set_margin_for_sdl(0, UIMenu::item_pad+UIMenu::pad_right, 10,
            UIMenu::item_pad + (tile_indent ? 38 : 0));
    m_ui.more->set_margin_for_sdl(10, UIMenu::item_pad+UIMenu::pad_right, 0, 0);
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
    int vph = m_ui.menu->get_region().height;
    int vpy = m_ui.scroller->get_scroll();
    return (vpy < y1 && y1 < vpy+vph) || (vpy < y2 && y2 < vpy+vph);
}

bool Menu::page_down()
{
    int dy = m_ui.scroller->get_region().height;
    int y = m_ui.scroller->get_scroll();
    bool at_bottom = y+dy >= m_ui.menu->get_region().height;
    m_ui.scroller->set_scroll(y+dy);
#ifndef USE_TILE_LOCAL
    if (!at_bottom)
        m_ui.menu->set_showable_height(y+dy+dy);
#endif
    return !at_bottom;
}

bool Menu::page_up()
{
    int dy = m_ui.scroller->get_region().height;
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
        int dy = m_ui.scroller->get_region().height;
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
        webtiles_write_item(items[i]);

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
        ui::force_render();
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
        webtiles_write_item(items[i]);

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
            tiles.json_write_int("tex", get_tile_texture(tile.tile));

            if (tile.ymax != TILE_Y)
                tiles.json_write_int("ymax", tile.ymax);

            tiles.json_close_object();
        }

        tiles.json_close_array();
    }
}

void Menu::webtiles_write_item(const MenuEntry* me) const
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
