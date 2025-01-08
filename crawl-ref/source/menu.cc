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
#include "tile-env.h"
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
 #include "tiledoll.h"
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

// Menu and subclasses: one of the main higher-level UI elements in dcss. The
// inventory screen is a prototypical use of this style of menu.
//
// Class hierarchy for Menu (many of these also subclass MenuEntry):
//
// Menu (menu.cc/h): possible to use directly, but rare, e.g. y/n prompts
// | \-ToggleableMenu (menu.cc/h): ability menu, various deck menus
// |   \-SpellMenu (spl-cast.cc): spell casting / spell view menu
// |
// |\--PromptMenu (prompt.h): menus that can show in both the message pane
//                            prompt channel, and as a regular menu
// |
// |\--InvMenu (invent.cc/h): most inventory/inv selection menus aside from
// |   |                      what the subclasses do. (Including take off
// |   |                      jewellery and remove armour.)
// |   |\--AcquireMenu (acquire.cc): acquirement
// |   |\--UseItemMenu (item-use.cc): scrolls, potions, put on jewellery, wear
// |   |                              armour, wield, scroll item selection
// |   |                              (e.g. brand, enchant)
// |   |\--KnownMenu (known-items.cc): known/unknown items
// |   |\--DescMenu (directn.cc): xv and ctrl-x menus, help lookup
// |   \---ShopMenu (shopping.cc): buying in shops
// |
// |\--ShoppingListMenu (shopping.cc): shopping lists (`$`)
// |\--StashSearchMenu (stash.cc): stash searches (ctrl-f)
// |\--ActionSelectMenu (quiver.cc): quiver action selection
// |\--SpellLibraryMenu (spl-book.cc): memorize menu
// |\--GameMenu (main.cc): game menu
// |\--MacroEditMenu (macro.cc): macro editing overall menu
// |\--MappingEditMenu (macro.cc): single macro mapping editing
// |\--LookupHelpMenu (lookup-help.cc): help lookup, e.g. `?/`
// |\--DescMenu (lookup-help.cc): submenu for choosing between search results
// |\--MutationMenu (mutation.cc): view info about mutations (`A`)
// \---StackFiveMenu (decks.cc): menu for the stack five ability
//
// Unfortunately, PrecisionMenu is something entirely different (used for the
// skill menu), as is OuterMenu (local builds main menu, character/background
// selection, hi scores). PrecisionMenu is deprecated.

using namespace ui;

class UIMenu : public Widget
{
    friend class UIMenuPopup;
public:
    UIMenu(Menu *menu) : m_menu(menu), m_num_columns(1)

#ifdef USE_TILE_LOCAL
    , m_font_entry(tiles.get_crt_font()), m_text_buf(m_font_entry)
#endif
    {
#ifdef USE_TILE_LOCAL
        // this seems ... non-ideal?? (pattern occurs in a few other places,
        // precision menu, playerdoll, ...)
        const ImageManager *image = tiles.get_image_manager();
        for (int i = 0; i < TEX_MAX; i++)
            m_tile_buf[i].set_tex(&image->get_texture(static_cast<TextureID>(i)));
#else
        expand_h = true;
#endif
    };
    ~UIMenu() {};

    virtual void _render() override;
    virtual SizeReq _get_preferred_size(Direction dim, int prosp_width) override;
    virtual void _allocate_region() override;
    int get_num_columns() const { return m_num_columns; };
    void set_num_columns(int n) {
        m_num_columns = n;
        _invalidate_sizereq();
        _queue_allocation();
    };
#ifdef USE_TILE_LOCAL
    virtual bool on_event(const Event& event) override;
#endif
    void set_min_col_width(int w) { m_min_col_width = w; } // XX min height?
    int get_min_col_width() { return m_min_col_width; }

    void set_initial_scroll(int i) { m_force_scroll = i; }
    int get_scroll_context() { return m_scroll_context + m_scroll_context / 2; }

    void update_item(int index);
    void update_items();

    void set_hovered_entry(int i);

    void is_visible_item_range(int *vis_min, int *vis_max);
    void get_item_region(int index, int *y1, int *y2);
    void get_item_gridpos(int index, int *row, int *col);

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
    int m_hover_idx = -1;
    int m_real_hover_idx = -1;
    int m_min_col_width = -1;

    int m_force_scroll = -1; // in rows, no pixels
    bool m_initial_hover_snap = false;
    int m_scroll_context = 0;

    int m_num_columns = 1;
    int m_nat_column_width; // set by do_layout()
    void do_layout(int mw, int num_columns, bool just_checking=false);
    struct MenuItemInfo {
        int x, y, row, column;
        formatted_string text;
#ifdef USE_TILE_LOCAL
        vector<tile_def> tiles;
#endif
        bool heading;
    };
    vector<MenuItemInfo> item_info;

#ifdef USE_TILE_LOCAL

    int get_max_viewport_height();

    vector<int> row_heights;

    bool m_mouse_pressed = false;
    int m_mouse_x = -1, m_mouse_y = -1;
    void update_hovered_entry(bool force=false);

    void pack_buffers();

    bool m_draw_tiles;
    FontWrapper *m_font_entry;
    ShapeBuffer m_shape_buf;
    LineBuffer m_line_buf, m_div_line_buf;
    FontBuffer m_text_buf;
    FixedVector<TileBuffer, TEX_MAX> m_tile_buf;

public:
    size_t shown_items() { return item_info.size(); }

    static constexpr int item_pad = 2;
    static constexpr int pad_right = 10;
#else
    static constexpr int pad_right = 2; // no vertical padding for console

    int m_shown_height {0};
#endif
};

void UIMenu::update_items()
{
    _invalidate_sizereq();

    item_info.resize(m_menu->items.size());
    do_layout(m_region.width, m_num_columns, true);
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

    int v_min = 0, v_max = item_info.size(), i;
#ifdef USE_TILE_LOCAL
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
    for (i = 0; i < (int)item_info.size(); ++i)
    {
        if (item_info[i].row >= scroll)
        {
            v_min = i;
            break;
        }
    }
    for (; i < (int)item_info.size(); ++i)
    {
        if (item_info[i].row >= scroll + viewport_height)
        {
            v_max = i;
            break;
        }
    }
#endif
    v_max = min(v_max, (int)m_menu->items.size()); // ??
    if (vis_min)
        *vis_min = v_min;
    if (vis_max)
        *vis_max = v_max;
}

void UIMenu::get_item_gridpos(int index, int *row, int *col)
{
    ASSERT_RANGE(index, 0, (int)m_menu->items.size());
    if (item_info.empty())
    {
        // no layout yet
        if (row)
            *row = index;
        if (col)
            *col = 0;
        return;
    }
    ASSERT_RANGE(index, 0, (int)item_info.size());
    // XX why not just expose MenuItemInfo
    if (row)
        *row = item_info[index].row;
    if (col)
        *col = item_info[index].column;
}

void UIMenu::get_item_region(int index, int *y1, int *y2)
{
    ASSERT_RANGE(index, 0, (int)m_menu->items.size());

    int row = -1;
    // use just the index in an uninitialized menu
    if (item_info.size() != m_menu->items.size())
        row = index;
    else
        row = item_info[index].row;
#ifdef USE_TILE_LOCAL
    if (static_cast<size_t>(row + 1) >= row_heights.size())
    {
        // call before UIMenu has been laid out
        if (y1)
            *y1 = -1;
        if (y2)
            *y2 = -1;
        return;
    }
    if (y1)
        *y1 = row_heights[row];
    if (y2)
        *y2 = row_heights[row+1];
#else
    if (y1)
        *y1 = row;
    if (y2)
        *y2 = row+1;
#endif
}

void UIMenu::update_item(int index)
{
    _invalidate_sizereq();
    _queue_allocation();

    ASSERT(index < static_cast<int>(m_menu->items.size()));
    const MenuEntry *me = m_menu->items[index];
    int colour = m_menu->item_colour(me);
    string text = me->get_text();

    item_info.resize(m_menu->items.size());

    auto& entry = item_info[index];
    entry.text.clear();
    entry.text.textcolour(colour);
    entry.text += formatted_string::parse_string(text);
    entry.heading = me->level == MEL_TITLE || me->level == MEL_SUBTITLE;
#ifdef USE_TILE_LOCAL
    entry.tiles.clear();
    me->get_tiles(entry.tiles);
#endif
}

#ifdef USE_TILE_LOCAL
static bool _has_hotkey_prefix(const string &s)
{
    // Don't read out of bounds!
    if (s.size() < 5)
        return false;

    // [enne] - Ugh, hack. Maybe MenuEntry could specify the
    // presence and length of this substring?
    bool let = (s[1] >= 'a' && s[1] <= 'z' || s[1] >= 'A' && s[1] <= 'Z');
    bool plus = (s[3] == '-' || s[3] == '+' || s[3] == '#');
    return let && plus && s[0] == ' ' && s[2] == ' ' && s[4] == ' ';
}
#endif

void UIMenu::do_layout(int mw, int num_columns, bool just_checking)
{
#ifdef USE_TILE_LOCAL
    const int min_column_width = m_min_col_width > 0 ? m_min_col_width : 400;
    const int max_column_width = mw / num_columns;
    const int text_height = m_font_entry->char_height();

    int column = -1; // an initial increment makes this 0
    int column_width = 0;
    int row_height = 0;
    int height = 0;
    int row_count = 0;

    // if the row heights are completely uninitialized, we should put something
    // in there
    just_checking = just_checking && !row_heights.empty();

    if (!just_checking)
    {
        row_heights.clear();
        row_heights.reserve(m_menu->items.size()+1);
    }

    for (size_t i = 0; i < m_menu->items.size(); ++i)
    {
        auto& entry = item_info[i];

        column = entry.heading ? 0 : (column+1) % num_columns;

        if (column == 0)
        {
            row_height += row_height == 0 ? 0 : 2*item_pad;
            m_scroll_context = max(m_scroll_context, row_height);
            height += row_height;
            if (!just_checking)
                row_heights.push_back(height);
            row_count++;
            row_height = 0;
        }

        const int text_width = m_font_entry->string_width(entry.text);

        if (!just_checking)
            entry.y = height;
        entry.row = row_count - 1;
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
            {
                if ((text_width > max_column_width-entry.x-pad_right))
                {
                    formatted_string text;
                    // TODO: refactor to use _get_text_preface
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
            }

            column_width = max(column_width, text_sx + text_width + pad_right);
            row_height = max(row_height, item_height);
        }
    }
    row_height += row_height == 0 ? 0 : 2*item_pad;
    m_scroll_context = max(m_scroll_context, row_height);
    height += row_height;
    if (!just_checking)
        row_heights.push_back(height);
    column_width += 2*item_pad;

    m_height = height;
    m_nat_column_width = max(min_column_width, min(column_width, max_column_width));
#else
    UNUSED(just_checking);
    // TODO: this code is not dissimilar to the tiles code, could they be
    // further unified?
    const int min_column_width = m_min_col_width > 0 ? m_min_col_width : 10;
    const int max_column_width = mw / num_columns;

    int row = -1;
    int column = -1; // an initial increment makes these 0
    int column_width = 0;

    for (size_t i = 0; i < m_menu->items.size(); ++i)
    {
        auto& entry = item_info[i];

        // headings occupy the entire row
        column = entry.heading ? 0 : (column+1) % num_columns;
        if (column == 0)
            row++;

        const int text_width = static_cast<int>(entry.text.width());

        entry.x = 0;
        entry.y = row;
        entry.row = row;
        entry.column = column;

        if (entry.heading)
            column = num_columns-1; // headings occupy the entire row, skip the rest
        else
        {
            // TODO(?): tiles will wrap menu entries that don't fit in a single
            // row, should that be implemented for console?
            column_width = max(column_width, text_width + pad_right);
        }
    }
    m_height = row + 1;
    // should this update the region??
    m_nat_column_width = max(min_column_width, min(column_width, max_column_width));
#endif
}

#ifdef USE_TILE_LOCAL
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
        int y = item_info[i].y - item_info[vis_min].y + 1;
        int x = item_info[i].x + item_info[i].column * m_nat_column_width;
        cgotoxy(m_region.x + x + 1, m_region.y + scroll + y);
        const int col = m_menu->item_colour(me);
        textcolour(col);

        // TODO: is this highlighting good enough for accessibility purposes?
        if (m_hover_idx == i)
            textbackground(default_hover_colour());
        // XX these will format the hover differently.
        // TODO: multiline entries
        if (m_menu->get_flags() & MF_ALLOW_FORMATTING)
        {
            formatted_string s = formatted_string::parse_string(
                me->get_text(), col);
            // headings always occupy full width
            if (item_info[i].heading)
                s.chop(m_region.width).display();
            else
                s.chop(m_nat_column_width).display();
        }
        else
        {
            string text = me->get_text();
            // headings always occupy full width
            if (item_info[i].heading)
                text = chop_string(text, m_region.width);
            else
                text = chop_string(text, m_nat_column_width);
            cprintf("%s", text.c_str());
        }
        textbackground(BLACK);
    }
#endif
}

SizeReq UIMenu::_get_preferred_size(Direction dim, int prosp_width)
{
#ifdef USE_TILE_LOCAL
    if (!dim)
    {
        do_layout(INT_MAX, m_num_columns, true);
        const int em = tiles.get_crt_font()->char_width();
        int max_menu_width = min(93*em, m_nat_column_width * m_num_columns);
        return {0, max_menu_width};
    }
    else
    {
        do_layout(prosp_width, m_num_columns, true);
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
    UIMenuMore() : using_template(false)
    {}
    virtual ~UIMenuMore() {}

    void set_text_immediately(const formatted_string &fs)
    {
        m_text.clear();
        m_text += fs;
        _expose();
        m_wrapped_size = Size(-1);
        // XX this doesn't handle height changes. I think it's partly a
        // sequencing issue: previous code will correctly invalidate on a
        // height change, but m_region isn't updated until later, so you get
        // the wrong wrapping behavior + a lack of reflowing until the next
        // update.
        wrap_text_to_size(m_region.width, m_region.height);
    }

    void set_more_template(const string &scroll, const string &noscroll)
    {
        // a string that can have `XXX` in it somewhere, to be replaced by a
        // scroll position
        // TODO: might be simpler just to call get_keyhelp directly?
        const bool diff = !using_template || text_for_scrollable != scroll
                                          || text_for_unscrollable != noscroll;
        if (diff)
        {
            text_for_scrollable = scroll;
            text_for_unscrollable = noscroll;
            _invalidate_sizereq();
            _queue_allocation();
        }

        if (!using_template)
        {
            using_template = true;
            set_from_template(true, 0);
        }
    }

    void set_from_template(bool scrollable, int scroll_percent)
    {
        if (!using_template)
            return;
        string more_template = scrollable
                                ? text_for_scrollable
                                : text_for_unscrollable;
        const string perc = scroll_percent <= 0 ? "top"
                            : scroll_percent >= 100 ? "bot"
                            : make_stringf("%2d%%", scroll_percent);

        more_template = replace_all(more_template, "XXX", perc);
        set_text_immediately(formatted_string::parse_string(more_template));
    }

#ifdef USE_TILE_WEB
    void webtiles_write_more()
    {
        // assumes an open object
        if (using_template)
        {
            tiles.json_write_string("more", text_for_scrollable);
            tiles.json_write_string("alt_more", text_for_unscrollable);
        }
        else
        {
            const string shown_more = get_text().to_colour_string(LIGHTGRAY);
            tiles.json_write_string("more", shown_more);
            tiles.json_write_string("alt_more", shown_more);
        }
    }
#endif

    bool using_template;

private:
    string text_for_scrollable;
    string text_for_unscrollable;
};

class UIMenuPopup : public ui::Popup
{
public:
    UIMenuPopup(shared_ptr<Widget> child, Menu *menu) : ui::Popup(child), m_menu(menu) {};
    virtual ~UIMenuPopup() {};

    int _calc_columns(int mw) const;
    virtual void _allocate_region() override;

private:
    Menu *m_menu;
};

/// What is the max number of columns that will fit into mw, given the current
/// items?
int UIMenuPopup::_calc_columns(int mw) const
{
    int min_column_width = m_menu->m_ui.menu->get_min_col_width();
    if (min_column_width <= 0)
#ifdef USE_TILE_LOCAL
        min_column_width = m_menu->m_ui.menu->m_font_entry->char_width() * 10;
#else
        min_column_width = 10;
#endif

#ifdef USE_TILE_LOCAL
    // TODO: some code overlap with do_layout
    int max_entry_text = min_column_width;
    for (size_t i = 0; i < m_menu->items.size(); ++i)
    {
        auto& entry = m_menu->m_ui.menu->item_info[i];
        const int text_width = entry.x
                    + m_menu->m_ui.menu->m_font_entry->string_width(entry.text)
                    + m_menu->m_ui.menu->pad_right;
        max_entry_text = max(max_entry_text, text_width);
    }
#else
    int max_entry_text = min_column_width;
    for (size_t i = 0; i < m_menu->items.size(); ++i)
    {
        max_entry_text = max(max_entry_text,
                static_cast<int>(m_menu->items[i]->get_text().size())
                + m_menu->m_ui.menu->pad_right);
    }
#endif

    // the 6 here is somewhat heuristic, but menus with more columns than
    // around this, in my testing, become harder to parse
    return max(1, min(6, mw / max_entry_text));
}

void UIMenuPopup::_allocate_region()
{
    Popup::_allocate_region();

    int max_height = m_menu->m_ui.popup->get_max_child_size().height;
    max_height -= m_menu->m_ui.title->get_region().height;
    max_height -= m_menu->m_ui.title->get_margin().bottom;
    max_height -= m_menu->m_ui.more->get_region().height;
    int viewport_height = m_menu->m_ui.scroller->get_region().height;
    int menu_w = m_menu->m_ui.menu->get_region().width;
    m_menu->m_ui.menu->do_layout(menu_w, 1);

    // see if we should try a two-column layout
    int m_height = m_menu->m_ui.menu->m_height;
    int more_height = m_menu->m_ui.more->get_region().height;
    int num_cols = m_menu->m_ui.menu->get_num_columns();
    if (m_menu->is_set(MF_GRID_LAYOUT))
    {
        const int max_columns = _calc_columns(menu_w);
        if (num_cols != max_columns)
        {
            // TODO: it might be good to horizontally shrink grid layout
            // menus a bit if the calculated column count would add too much
            // whitespace. But I'm not quite sure how to do this...
            m_menu->m_ui.menu->set_num_columns(max_columns);
            ui::restart_layout(); // NORETURN
        }
    }
    else if (m_menu->is_set(MF_USE_TWO_COLUMNS))
    {
        // XX should this be smarter about width for console?
        if ((num_cols == 1 && m_height+more_height > max_height)
         || (num_cols == 2 && m_height+more_height <= max_height))
        {
            m_menu->m_ui.menu->set_num_columns(3 - num_cols);
            ui::restart_layout(); // NORETURN
        }
    }
    m_menu->m_ui.menu->do_layout(menu_w, num_cols);

    if (m_menu->m_keyhelp_more)
    {
        // note: m_menu->m_ui.menu->get_region().height seems to be inaccurate
        // on console (ok on tiles) -- is this a problem?
        const int menu_height = m_menu->m_ui.menu->m_height;
        const int scroll_percent = (menu_height - viewport_height == 0)
                    ? 0
                    : m_menu->m_ui.scroller->get_scroll() * 100
                                            / (menu_height - viewport_height);
        m_menu->m_ui.more->set_from_template(menu_height > max_height, scroll_percent);
        // XX what if this changes the # of lines
    }

    // is the more invisible but has some text? This can happen either on
    // initial layout, or if the keyhelp changes depending on whether there's
    // a scrollbar.
    const bool more_visible = m_menu->m_ui.more->is_visible();
    const bool more_has_text = !m_menu->m_ui.more->get_text().ops.empty();
    if (more_visible != more_has_text)
    {
        // TODO: for some reason if this is initially set to false, on the
        // first render, toggling it to true doesn't work. (Workaround: always
        // start true.) I think it might be an issue with the more changing
        // height not working correctly?
        m_menu->m_ui.more->set_visible(!more_visible);
        _invalidate_sizereq();
        m_menu->m_ui.more->_queue_allocation();
        ui::restart_layout();
    }

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
    // Do some initial setup that requires higher-level calls but can't happen
    // until the menu entry heights are known
    if (m_force_scroll >= 0)
    {
        m_menu->set_scroll(m_force_scroll);
        m_force_scroll = -1;
    }
    if (!m_initial_hover_snap)
    {
        // TODO: because it is initial this does the minimum snap to ensure
        // the entry is visible, which can leave it the last on the page. This
        // is exactly what we want for things like pgdn, but it may not be
        // what a caller wants for an initial snap -- maybe do some extra
        // scrolling for this case? (A caller can manually set the scroll, but
        // this is tricky for some use cases because they won't know the menu
        // height.)
        if (m_menu->last_hovered >= 0)
            m_menu->snap_in_page(m_menu->last_hovered);
        m_initial_hover_snap = true;
    }

    do_layout(m_region.width, m_num_columns);
#ifdef USE_TILE_LOCAL
    if (!(m_menu->flags & MF_ARROWS_SELECT) || m_menu->last_hovered < 0)
        update_hovered_entry();
    else
        m_hover_idx = m_menu->last_hovered;
    pack_buffers();
#endif
}

void UIMenu::set_hovered_entry(int i)
{
    m_hover_idx = i;

#ifdef USE_TILE_LOCAL
    if (row_heights.size() > 0) // check for initial layout
        pack_buffers();
#endif
    _expose();
}

#ifdef USE_TILE_LOCAL
void UIMenu::update_hovered_entry(bool force)
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
        if (me->hotkeys_count() == 0 && !force)
            continue;
        const int w = m_region.width / m_num_columns;
        const int entry_x = entry.column * w;
        const int entry_h = row_heights[entry.row+1] - row_heights[entry.row];
        if (x >= entry_x && x < entry_x+w && y >= entry.y && y < entry.y+entry_h)
        {
            wm->set_mouse_cursor(MOUSE_CURSOR_POINTER);
            if (force && m_menu->last_hovered != i)
                m_menu->set_hovered(i, force); // give menu a chance to change state
            else if (me->hotkeys_count())
                m_hover_idx = i;
            m_real_hover_idx = i;
            return;
        }
    }
    wm->set_mouse_cursor(MOUSE_CURSOR_ARROW);
    if (!(m_menu->flags & MF_ARROWS_SELECT))
    {
        if (force)
            m_menu->set_hovered(-1, force);
        else
            m_hover_idx = -1;
        m_real_hover_idx = -1;
    }
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
        if (!(m_menu->flags & MF_ARROWS_SELECT) || m_menu->last_hovered < 0)
            update_hovered_entry(true);
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
        if (!(m_menu->is_set(MF_ARROWS_SELECT)))
            m_hover_idx = -1;
        m_real_hover_idx = -1;
        do_layout(m_region.width, m_num_columns);
        pack_buffers();
        _expose();
        return false;
    }

    if (event.type() == Event::Type::MouseMove)
    {
        do_layout(m_region.width, m_num_columns);
        update_hovered_entry(true);
        pack_buffers();
        _expose();
        return true;
    }

    int key = -1;
    if (event.type() ==  Event::Type::MouseDown
        && (event.button() == MouseEvent::Button::Left
            || event.button() == MouseEvent::Button::Right))
    {
        m_mouse_pressed = true;
        do_layout(m_region.width, m_num_columns);
        update_hovered_entry(true);
        pack_buffers();
        _expose();
    }
    else if (event.type() == Event::Type::MouseUp
            && (event.button() == MouseEvent::Button::Left
                || event.button() == MouseEvent::Button::Right)
            && m_mouse_pressed)
    {
        // use the "real" hover idx, a menu item that the mouse is currently
        // positioned over.
        int entry = m_real_hover_idx;
        if (entry != -1 && m_menu->items[entry]->hotkeys_count() > 0)
            key = event.button() == MouseEvent::Button::Left ? CK_MOUSE_B1 : CK_MOUSE_B2;

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

        if (!m_menu->is_set(MF_NOSELECT))
        {
            bool hovered = i == m_hover_idx
                && !entry.heading
                && me->hotkeys_count() > 0;

            if (me->selected() && !m_menu->is_set(MF_QUIET_SELECT))
            {
                // draw a highlighted background on selected entries (usually only
                // multiselect menus), overriding hover background
                m_shape_buf.add(entry_x, entry.y,
                        entry_ex, entry.y+entry_h, selected_colour);
            }
            else if (hovered)
            {
                // draw the regular hover background
                const VColour hover_bg = m_mouse_pressed ?
                    VColour(0, 0, 0, 255) : VColour(255, 255, 255, 25);
                m_shape_buf.add(entry_x, entry.y,
                        entry_ex, entry.y+entry_h, hover_bg);
            }

            // draw a line around any hovered entry
            if (hovered)
            {
                const VColour mouse_colour = m_mouse_pressed ?
                    VColour(34, 34, 34, 255) : VColour(255, 255, 255, 51);
                m_line_buf.add_square(entry_x + 1, entry.y + 1,
                        entry_x+col_width, entry.y+entry_h, mouse_colour);
            }
        }
    }
}
#endif

Menu::Menu(int _flags, const string& tagname, KeymapContext kmc)
  : f_selitem(nullptr), f_keyfilter(nullptr),
    action_cycle(CYCLE_NONE), menu_action(ACT_EXECUTE), title(nullptr),
    title2(nullptr), flags(_flags), tag(tagname),
    cur_page(1), items(), sel(),
    select_filter(), highlighter(new MenuHighlighter), num(-1), lastch(0),
    alive(false), more_needs_init(true), remap_numpad(true),
    last_hovered(-1), m_kmc(kmc), m_filter(nullptr)
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

    // just do minimal initialization for now, full default more initialization
    // happens on show
    set_more("");
    more_needs_init = true; // reset
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
}

void Menu::set_flags(int new_flags)
{
    flags = new_flags;

    // disable arrow controls depending on options, or for any noselect menu
    if (!Options.menu_arrow_control || !!(flags & MF_NOSELECT))
        flags = flags & (~(MF_ARROWS_SELECT | MF_INIT_HOVER));

#ifdef DEBUG
    int sel_flag = flags & (MF_NOSELECT | MF_SINGLESELECT | MF_MULTISELECT);
    ASSERT(sel_flag == MF_NOSELECT || sel_flag == MF_SINGLESELECT || sel_flag == MF_MULTISELECT);
#endif
}

bool Menu::minus_is_command() const
{
    // TODO: remove?
    return is_set(MF_MULTISELECT) || !is_set(MF_SPECIAL_MINUS);
}

void Menu::set_more(const formatted_string &fs)
{
    m_keyhelp_more = false;
    more_needs_init = false;
    more = fs;
    update_more();
}

void Menu::set_more(const string s)
{
    set_more(formatted_string::parse_string(s));
}

formatted_string pad_more_with(formatted_string s,
    const formatted_string &pad, int min_width)
{
    if (min_width <= 0)
        return s;
    // Needs to be done as a parsed formatted_string so that columns are
    // counted correctly.
    const auto lines = split_string("\n", s.tostring(), false, true);
    const int last_len = static_cast<int>(lines.back().size());
    const int pad_size = static_cast<int>(pad.tostring().size());
    if (last_len < (min_width - pad_size))
    {
        s += string(min_width - (last_len + pad_size), ' ');
        s += pad;
    }
    return s;
}

// assumes contiguous lettering
string hyphenated_hotkey_letters(int how_many, char first)
{
    how_many = min(how_many, 52);
    string s;
    if (how_many > 1)
    {
        // crawl uses A-Z second, but it is first in ascii
        int last = static_cast<int>(first) + how_many - 1;
        if (last > 'z')
            last = 'A' + last - ('z' + 1);
        s = make_stringf("[<w>%c</w>-<w>%c</w>]", first, static_cast<char>(last));
    }
    else
        s = make_stringf("[<w>%c</w>]", first);
    return s;
}

string pad_more_with(const string &s, const string &pad, int min_width)
{
    // TODO is there a way of compensating for the tile width on the left margin
    // of certain menus while adding a padded element?
    return pad_more_with(
        formatted_string::parse_string(s),
        formatted_string::parse_string(pad),
        min_width).to_colour_string(LIGHTGRAY);
}

static string _keyhelp_format_key(const string &s)
{
    return make_stringf("[<w>%s</w>]", s.c_str());
}

string menu_keyhelp_select_keys()
{
    const string up = command_to_string(CMD_MENU_UP);
    const string down = command_to_string(CMD_MENU_DOWN);
    return make_stringf("[<w>%s</w>|<w>%s</w>]", up.c_str(), down.c_str());
}

static string _command_to_string(command_type cmd)
{
    return replace_all(command_to_string(cmd), "<", "<<");
}

// standardized formatting for this
string pad_more_with_esc(const string &s)
{
    return pad_more_with(s, menu_keyhelp_cmd(CMD_MENU_EXIT) + " exit");
}

string menu_keyhelp_cmd(command_type cmd)
{
    if (cmd == CMD_MENU_PAGE_UP || cmd == CMD_MENU_PAGE_DOWN)
    {
        // always show < and > as secondary keys, if they are defined
        const string primary = _command_to_string(cmd);
        const string secondary_key = cmd == CMD_MENU_PAGE_UP ? "<<" : ">";
        string secondary;
        if (primary != secondary_key && key_to_command(secondary_key[0], KMC_MENU) == cmd)
            secondary = make_stringf("|<w>%s</w>", secondary_key.c_str());
        return make_stringf("[<w>%s</w>%s]", primary.c_str(), secondary.c_str());
    }
    else if (cmd == CMD_MENU_TOGGLE_SELECTED)
    {
        // TODO: fix once space is not hardcoded
        return make_stringf("[<w>%s</w>|<w>Space</w>]", _command_to_string(cmd).c_str());
    }
    else
        return _keyhelp_format_key(_command_to_string(cmd));
}

string Menu::get_keyhelp(bool scrollable) const
{
    // TODO: derive this from cmd key bindings

    // multiselect always shows a keyhelp, for singleselect it is blank unless
    // it can be scrolled
    if (!scrollable && !is_set(MF_MULTISELECT))
        return "";

    string navigation = "<lightgrey>";
    if (is_set(MF_ARROWS_SELECT))
        navigation += menu_keyhelp_select_keys() + " select  ";

    if (scrollable)
    {
        navigation +=
            menu_keyhelp_cmd(CMD_MENU_PAGE_DOWN) + " page down  "
            + menu_keyhelp_cmd(CMD_MENU_PAGE_UP) + " page up  ";
    }
    if (!is_set(MF_MULTISELECT))
        navigation += menu_keyhelp_cmd(CMD_MENU_EXIT) + " exit";
    navigation += "</lightgrey>";
    if (is_set(MF_MULTISELECT))
    {
        navigation = pad_more_with_esc(navigation);
        // XX this may not work perfectly with the way InvMenu handles
        // selection
        const auto chosen_count = selected_entries().size();
        navigation +=
                "\n<lightgrey>"
                "Letters toggle    ";
        if (is_set(MF_ARROWS_SELECT))
        {
            navigation += menu_keyhelp_cmd(CMD_MENU_TOGGLE_SELECTED)
                + " toggle selected    ";
        }
        if (chosen_count)
        {
            navigation += menu_keyhelp_cmd(CMD_MENU_ACCEPT_SELECTION)
                + make_stringf(
                    " accept (%zu chosen)"
                    "</lightgrey>",
                chosen_count);
        }
    }
    // XX this is present on non-scrolling multiselect keyhelps mostly for
    // aesthetic reasons, but maybe it could change with hover?
    // the `XXX` is replaced in the ui code with a position in the scroll.
    return pad_more_with(navigation, "<lightgrey>[<w>XXX</w>]</lightgrey>");
}

// use the class's built in keyhelp string
void Menu::set_more()
{
    m_keyhelp_more = true;
    more_needs_init = false;
    update_more();
}

void Menu::set_min_col_width(int w)
{
#ifdef USE_TILE_LOCAL
    // w is in chars
    m_ui.menu->set_min_col_width(w * tiles.get_crt_font()->char_width());
#else
    // n.b. this currently has no effect in webtiles unless the more is visible
    m_ui.menu->set_min_col_width(w);
#endif
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

void Menu::set_title(const string &t, bool first, bool indent)
{
    set_title(new MenuEntry(t, MEL_TITLE), first, indent);
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

    // no one has yet manually called set_more, so use a keyhelp more. We do it
    // here and this way instead of in the constructor so that a derived class
    // get_keyhelp is used if relevant -- this sets the height correctly.
    if (more_needs_init)
        set_more();

    if (reuse_selections)
        get_selected(&sel);
    else
        deselect_all(false);

    if (is_set(MF_START_AT_END))
    {
        m_ui.scroller->set_scroll(INT_MAX);
        if (is_set(MF_INIT_HOVER))
        {
            // MF_START_AT_END overrides a manually set initial hover
            set_hovered(static_cast<int>(items.size()) - 1);
            if (items[last_hovered]->level != MEL_ITEM)
                cycle_hover(true);
        }
    }
    else if (is_set(MF_INIT_HOVER)
        && (last_hovered < 0 || items[last_hovered]->level != MEL_ITEM)) // XX check hotkeys?
    {
        cycle_hover();
    }

    do_menu();

    return sel;
}

void Menu::do_menu()
{
    bool done = false;
    m_ui.popup = make_shared<UIMenuPopup>(m_ui.vbox, this);

    m_ui.popup->on_keydown_event([this, &done](const KeyEvent& ev) {
        // uses numpad number keys for navigation
        int key = remap_numpad ? numpad_to_regular(ev.key(), true) : ev.key();
        if (m_filter)
        {
            if (ev.key() == '?' && _title_prompt_help_tag.size())
            {
                // TODO: only useful for non-general prompts, is there another
                // help key that would be better?
                show_specific_help(_title_prompt_help_tag);
                return true;
            }

            key = m_filter->putkey(ev.key());

            if (key == CK_ESCAPE)
                m_filter->set_text("");

            if (key != -1)
            {
                lastch = key;
                delete m_filter;
                m_filter = nullptr;
            }
            update_title();
            return true;
        }
        done = !process_key(key);
        return true;
    });

    update_menu();
    ui::push_layout(m_ui.popup, m_kmc);

#ifdef USE_TILE_WEB
    tiles.push_menu(this);
    _webtiles_title_changed = false;
    m_ui.popup->on_layout_pop([](){ tiles.pop_menu(); });
#endif

    alive = true;
    if (on_show)
        done = !on_show();
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

bool Menu::is_set(int flag) const
{
    return (flags & flag) == flag;
}

int Menu::pre_process(int k)
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
                // currently, no menu uses this?
                get_selected(&sel);
                return false;
            }
        }
    }
    get_selected(&sel);
    return true;
}

bool Menu::title_prompt(char linebuf[], int bufsz, const char* prompt, string help_tag)
{
    bool validline;

    ASSERT(!m_filter);
    // XX show cursor in console
#ifdef USE_TILE_WEB
    tiles.json_open_object();
    tiles.json_write_string("msg", "title_prompt");
    tiles.json_write_string("prompt", prompt);
    tiles.json_close_object();
    tiles.finish_message();
#endif
    m_filter = new resumable_line_reader(linebuf, bufsz);
    m_filter->set_prompt(prompt);
    // ugly to use a member variable for this, maybe generalize to a feature
    // of line_reader?
    _title_prompt_help_tag = help_tag;
    update_title();
    do
    {
        ui::pump_events();
    }
    while (m_filter && !crawl_state.seen_hups);
    validline = linebuf[0];

    return validline;
}

bool Menu::cycle_mode(bool forward)
{
    switch (action_cycle)
    {
    case CYCLE_NONE:
        return false;
    case CYCLE_TOGGLE:
        // direction doesn't matter for this case
        ASSERT(menu_action != ACT_MISC);
        if (menu_action == ACT_EXECUTE)
            menu_action = ACT_EXAMINE;
        else
            menu_action = ACT_EXECUTE;
        break;
    case CYCLE_CYCLE:
        if (forward)
            menu_action = (action)((menu_action + 1) % ACT_NUM);
        else
            menu_action = (action)((menu_action + ACT_NUM - 1) % ACT_NUM);
        break;
    }
    sel.clear();
    update_title();
    update_more();
    return true;
}

/// Given some (possibly empty) selection, run any associated on_select or
/// on_single_selection triggers, and return whether the menu loop should
/// continue running. These triggers only happen for singleselect menus.
bool Menu::process_selection()
{
    // update selection
    get_selected(&sel);
    bool ret = sel.empty();
    // if it's non-empty, and we are in a singleselect menu, try to run any
    // associated on_select functions
    if (!!(flags & MF_SINGLESELECT) && !ret)
    {
        // singleselect: try running any on_select calls
        MenuEntry *item = sel[0];
        ret = false;
        if (item->on_select)
            ret = item->on_select(*item);
        else if (on_single_selection) // currently, no menus use both
            ret = on_single_selection(*item);
        // the UI for singleselect menus behaves oddly if anything is
        // selected when it runs, because select acts as a toggle. So
        // if the selection has been processed and the menu is
        // continuing, clear the selection.
        // TODO: this is a fairly clumsy api for menus that are
        // trying to *do* something.
        if (ret)
            deselect_all();
    }
    return ret;
}

bool Menu::process_command(command_type cmd)
{
    bool ret = true;

#ifdef USE_TILE_WEB
    const int old_vis_first = get_first_visible();
#endif

    // note -- MF_USE_TWO_COLUMNS doesn't guarantee two cols!
    // currently guaranteed to be false except on local tiles
    const bool multicol = m_ui.menu->get_num_columns() > 1;
    const int old_hover = last_hovered;

    switch (cmd)
    {
    case CMD_MENU_UP:
        if (is_set(MF_ARROWS_SELECT))
        {
            cycle_hover(true, false, true);
            if (last_hovered >= 0 && old_hover == last_hovered)
                line_up();
        }
        else
            line_up();
        break;
    case CMD_MENU_DOWN:
        if (is_set(MF_ARROWS_SELECT))
        {
            cycle_hover(false, false, true);
            if (last_hovered >= 0 && old_hover == last_hovered)
                line_down();
        }
        else
            line_down();
        break;
    case CMD_MENU_RIGHT:
        if (multicol && is_set(MF_ARROWS_SELECT))
            cycle_hover(false, true, false);
        else
            cycle_mode(true);
        break;
    case CMD_MENU_LEFT:
        if (multicol && is_set(MF_ARROWS_SELECT))
            cycle_hover(true, true, false);
        else
            cycle_mode(false);
        break;

    case CMD_MENU_LINE_UP:
        line_up();
        break;
    case CMD_MENU_LINE_DOWN:
        line_down();
        break;
    case CMD_MENU_PAGE_UP:
        page_up();
        break;
    case CMD_MENU_PAGE_DOWN:
        if (!page_down() && is_set(MF_WRAP))
            m_ui.scroller->set_scroll(0);
        break;
    case CMD_MENU_SCROLL_TO_TOP:
        m_ui.scroller->set_scroll(0);
        if (is_set(MF_ARROWS_SELECT) && items.size())
        {
            set_hovered(0);
            if (items[last_hovered]->level != MEL_ITEM)
                cycle_hover();
        }
        break;
    case CMD_MENU_SCROLL_TO_END:
        // setting this to INT_MAX when the last item is already visible does
        // unnecessary scrolling to force the last item to be exactly at the
        // end of the menu. (This has a weird interaction with page down.)
        // TODO: possibly should be fixed in ui.cc? But I don't understand that
        // code well enough
        if (items.size())
        {
            if (!in_page(static_cast<int>(items.size()) - 1, true))
                m_ui.scroller->set_scroll(INT_MAX);
            if (is_set(MF_ARROWS_SELECT))
            {
                set_hovered(static_cast<int>(items.size()) - 1);
                if (items[last_hovered]->level != MEL_ITEM)
                    cycle_hover(true);

            }
        }
        break;
    case CMD_MENU_SEARCH:
        if ((flags & MF_ALLOW_FILTER))
        {
            char linebuf[80] = "";

            const bool validline = title_prompt(linebuf, sizeof linebuf,
                                                "Select what (regex)?");

            // XX what is the use case for this exiting, should this set lastch
            ret = (validline && linebuf[0]) ? filter_with_regex(linebuf) : true;
        }
        break;
    case CMD_MENU_CYCLE_MODE:
        cycle_mode(true);
        break;
    case CMD_MENU_CYCLE_MODE_REVERSE:
        cycle_mode(false);
        break;
    case CMD_MENU_CYCLE_HEADERS:
        cycle_headers(true);
        break;
    case CMD_MENU_HELP:
        if (!help_key().empty())
            show_specific_help(help_key());
        break;
    case CMD_MENU_ACCEPT_SELECTION:
        ASSERT(is_set(MF_MULTISELECT));
        get_selected(&sel);
        ret = sel.empty(); // only exit if there is a selection, otherwise noop
        break;
    case CMD_MENU_SELECT:
        // select + accept. Note that this will work in multiselect mode to do
        // a quick accept if something is hovered, but is currently preempted by
        // CMD_MENU_ACCEPT_SELECTION above.
        if (is_set(MF_NOSELECT))
            break; // or crash?
        // try selecting a hovered item
        if (last_hovered >= 0)
            select_item_index(last_hovered, MENU_SELECT_ALL);
        ret = process_selection();
        break;

    case CMD_MENU_EXIT:
        sel.clear();
        lastch = CK_ESCAPE; // XX is this correct?
        ret = is_set(MF_UNCANCEL) && !crawl_state.seen_hups;
        break;
    case CMD_MENU_TOGGLE_SELECTED:
        // toggle the currently hovered item, if any. Noop if no hover.
        ASSERT(is_set(MF_MULTISELECT));
        if (last_hovered >= 0)
        {
            select_item_index(last_hovered, MENU_SELECT_INVERT);
            get_selected(&sel);
        }
        break;
    case CMD_MENU_SELECT_ALL: // Select all or apply filter if there is one.
        ASSERT(is_set(MF_MULTISELECT));
        select_index(-1, MENU_SELECT_ALL);
        break;
    case CMD_MENU_INVERT_SELECTION:
        ASSERT(is_set(MF_MULTISELECT));
        select_index(-1, MENU_SELECT_INVERT);
        get_selected(&sel);
        break;
    case CMD_MENU_CLEAR_SELECTION:
        ASSERT(is_set(MF_MULTISELECT));
        select_index(-1, MENU_SELECT_CLEAR); // XX is there a singleselect menu where this should work?
        break;

    case CMD_MENU_EXAMINE:
        if (last_hovered >= 0)
            ret = examine_index(last_hovered);
        break;

    default:
        break;
    }

    if (ret)
    {
        // is this overkill to always do?
        update_title();
        update_more();
    }

    if (cmd != CMD_NO_CMD)
        num = -1;

#ifdef USE_TILE_WEB
    if (old_vis_first != get_first_visible())
        webtiles_update_scroll_pos();
#endif

    return ret;
}

bool Menu::skip_process_command(int keyin)
{
    // TODO: autodetect if there is a menu item that uses a key that would
    // otherwise be bound?
    if (keyin == '-' && !minus_is_command())
        return true;
    return false;
}

static bool _cmd_converts_to_examine(command_type c)
{
    switch (c)
    {
    case CMD_MENU_SELECT:
    case CMD_MENU_ACCEPT_SELECTION:
    case CMD_MENU_TOGGLE_SELECTED:
        return true;
    default:
        return false;
    }
}

command_type Menu::get_command(int keyin)
{
    if (skip_process_command(keyin))
        return CMD_NO_CMD;

    // this is a `CASE_ESCAPE` case that the command mapping doesn't handle
    // -- is it needed?
    if (keyin == -1)
        return CMD_MENU_EXIT;

    // mouse clicks from UIMenu with a menu item selected
    if (keyin == CK_MOUSE_B1)
    {
        command_type cmd = is_set(MF_MULTISELECT)
                            ? CMD_MENU_TOGGLE_SELECTED : CMD_MENU_SELECT;
        if (menu_action == ACT_EXAMINE && _cmd_converts_to_examine(cmd))
            cmd = CMD_MENU_EXAMINE;

        return cmd;
    }
    else if (keyin == CK_MOUSE_B2)
        return CMD_MENU_EXAMINE;

    // for multiselect menus, we first check in the multiselect-specific keymap,
    // and if this doesn't find anything, the general menu keymap.
    if (is_set(MF_MULTISELECT))
    {
        // relies on subclasses to actually implement an examine behavior
        command_type cmd = key_to_command(keyin, KMC_MENU_MULTISELECT);
        if (menu_action == ACT_EXAMINE && _cmd_converts_to_examine(cmd))
            cmd = CMD_MENU_EXAMINE;

        if (cmd != CMD_NO_CMD)
            return cmd;
    }

    // n.b. this explicitly ignores m_kmc, which is intended only for keymap
    // purposes
    command_type cmd = key_to_command(keyin, KMC_MENU);

    // relies on subclasses to actually implement an examine behavior
    if (menu_action == ACT_EXAMINE && _cmd_converts_to_examine(cmd))
        cmd = CMD_MENU_EXAMINE;

    return cmd;
}

bool Menu::process_key(int keyin)
{
    // overall sequence given keyin:
    // 1. apply any keyfilter (currently: entirely unused!)
    // 2. apply any pre_process transformation
    // 3. try converting to a command (get_command, often overridden)
    //    a. if the key is skipped (usually by a subclass), return CMD_NO_CMD
    //    b. return the results of the key_to_command mapping
    // 4. if this got a command, run the command
    // 5. otherwise proceed to manual key handling on keyin
    //
    // n.b. superclasses can override process_key to preempt these steps as
    // well. Various special cases, e.g. digits for quantity menus
    //
    // webtiles complication alert: most of the menu navigation key handling
    // has a separate client-side implementation (for UI responsivenss), and
    // this function may not be called for arbitrary client-side keys!

    if (!is_set(MF_SHOW_EMPTY) && items.empty())
    {
        lastch = keyin;
        return false;
    }

    if (f_keyfilter)
        keyin = f_keyfilter(keyin);
    keyin = pre_process(keyin);

#ifdef USE_TILE_WEB
    const int old_vis_first = get_first_visible();
#endif
    if (keyin == ' '
        && !!(flags & MF_MULTISELECT) && !!(flags & MF_ARROWS_SELECT))
    {
        // XX allow customizing this mapping
        keyin = '.';
    }

    command_type cmd = CMD_NO_CMD;
    if (is_set(MF_SELECT_QTY) && !is_set(MF_NOSELECT) && isadigit(keyin))
    {
        // override cmd bindings for quantity digits
        if (num > 999)
            num = -1;
        num = (num == -1) ? keyin - '0' :
                            num * 10 + keyin - '0';
    }
    else
        cmd = get_command(keyin);

    if (cmd != CMD_NO_CMD)
    {
        lastch = keyin; // TODO: remove lastch
        return process_command(cmd);
    }

    switch (keyin)
    {
    case CK_NO_KEY:
    case CK_REDRAW:
    case CK_RESIZE:
        return true;
    case 0:
        return true;
    case CK_MOUSE_CLICK:
        // click event from ui.cc
        break;
    default:
        // Even if we do return early, lastch needs to be set first,
        // as it's sometimes checked when leaving a menu.
        lastch = keyin; // TODO: remove lastch?
        const int primary_index = hotkey_to_index(keyin, true);
        const int key_index = hotkey_to_index(keyin, false);

        // If no selection at all is allowed, exit now.
        if (!(flags & (MF_SINGLESELECT | MF_MULTISELECT)))
            return false;

        if (is_set(MF_SECONDARY_SCROLL) && primary_index < 0 && key_index >= 0)
        {
            auto snap_range = hotkey_range(keyin);
            snap_in_page(snap_range.second);
            set_hovered(snap_range.first);
#ifdef USE_TILE_WEB
            webtiles_update_scroll_pos(true);
#endif
            return true;
        }

        if (menu_action == ACT_EXAMINE && key_index >= 0)
            return examine_by_key(keyin);

        // Update either single or multiselect; noop if keyin isn't a hotkey.
        // Updates hover.
        select_items(keyin, num);
        get_selected(&sel);
        // we have received what should be a menu item hotkey. Activate that
        // menu item.
        if (is_set(MF_SINGLESELECT) && key_index >= 0)
        {
#ifdef USE_TILE_WEB
            webtiles_update_scroll_pos(true);
#endif
            return process_selection();
        }

        if (is_set(MF_ANYPRINTABLE)
            && (!isadigit(keyin) || !is_set(MF_SELECT_QTY)))
        {
            // TODO: should this behavior be made to coexist with multiselect?
            return false;
        }

        update_title();
        update_more();

        break;
    }

    // reset number state if anything other than setting a digit happened
    if (!isadigit(keyin))
        num = -1;

#ifdef USE_TILE_WEB
    // XX is handling this in process_command enough?
    if (old_vis_first != get_first_visible())
        webtiles_update_scroll_pos();
#endif

    return true;
}

string Menu::get_select_count_string(int) const
{
    string ret;
    if (f_selitem)
        ret = f_selitem(&sel);
    // count is shown in footer now

    return ret + string(max(12 - (int)ret.size(), 0), ' ');
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

int Menu::get_first_visible(bool skip_init_headers, int col) const
{
    int y = m_ui.scroller->get_scroll();
    for (int i = 0; i < static_cast<int>(items.size()); i++)
    {
        int item_y1;
        m_ui.menu->get_item_region(i, &item_y1, nullptr);
        if (item_y1 >= y)
        {
            if (skip_init_headers
                && (items[i]->level == MEL_TITLE
                    || items[i]->level == MEL_SUBTITLE))
            {
                // when using this to determine e.g. scroll position, it is
                // useful to ignore visible headers
                continue;
            }
            if (col >= 0)
            {
                int cur_col;
                m_ui.menu->get_item_gridpos(i, nullptr, &cur_col);
                if (cur_col != col)
                    continue;
            }
            return i;
        }
    }
    // returns 0 on empty menu -- callers should guard for this if relevant
    // (XX -1 might be better? but callers currently assume non-negative...)
    return items.size();
}

bool Menu::is_hotkey(int i, int key)
{
    bool ishotkey = items[i]->is_hotkey(key);
    return ishotkey && (!is_set(MF_SELECT_BY_PAGE) || in_page(i));
}

/// find the first item (if any) that has hotkey `key`.
int Menu::hotkey_to_index(int key, bool primary_only)
{
    // when called without a ui, just check from the beginning
    const int first_entry = ui_is_initialized() ? get_first_visible() : 0;
    const int final = items.size();

    // Process all items, in case user hits hotkey for an
    // item not on the current page.

    // We have to use some hackery to handle items that share
    // the same hotkey (as for pickup when there's a stack of
    // >52 items). If there are duplicate hotkeys, the items
    // are usually separated by at least a page, so we should
    // only select the item on the current page. We use only
    // one loop, but we look through the menu starting with the first
    // visible item, and check to see if we've matched an item
    // by its primary hotkey (hotkeys[0] for multiple-selection
    // menus), in which case we stop selecting further items. If
    // not, we loop around back to the beginning.
    for (int i = 0; i < final; ++i)
    {
        const int index = (i + first_entry) % final;
        if (is_hotkey(index, key)
            && (!primary_only || items[index]->hotkeys[0] == key))
        {
            return index;
        }
    }
    return -1;
}

pair<int,int> Menu::hotkey_range(int key)
{
    int first = -1;
    int last = -1;
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
        if (is_hotkey(i, key))
        {
            if (first < 0)
                first = i;
            last = i;
        }
    return make_pair(first, last);
}

void Menu::select_items(int key, int qty)
{
    const int index = hotkey_to_index(key, !is_set(MF_SINGLESELECT));
    if (index >= 0)
    {
        select_index(index, qty);
        // XX should this update hover generally? This approach is tailored
        // towards avoiding a weirdness with just the memorize menu...
        if (is_set(MF_MULTISELECT))
            set_hovered(index);
        return;
    }

    // no primary hotkeys found, check secondary hotkeys for multiselect
    if (is_set(MF_MULTISELECT))
    {
        auto snap_range = hotkey_range(key);
        if (snap_range.first >= 0)
        {
            for (int i = snap_range.first; i <= snap_range.second; ++i)
                if (is_hotkey(i, key))
                    select_index(i, qty);
            // try to ensure as much of the selection as possible is in
            // view by snapping twice
            snap_in_page(snap_range.second);
            set_hovered(snap_range.first);
#ifdef USE_TILE_WEB
            webtiles_update_scroll_pos(true);
#endif
        }
    }
}

bool Menu::examine_index(int i)
{
    ASSERT(i >= 0 && i < static_cast<int>(items.size()));
    if (on_examine)
        return on_examine(*items[i]);
    return true;
}

bool Menu::examine_by_key(int keyin)
{
    const int index = hotkey_to_index(keyin, !is_set(MF_SINGLESELECT));
    if (index >= 0)
    {
        set_hovered(index);
#ifdef USE_TILE_WEB
        webtiles_update_scroll_pos(true);
#endif
        return examine_index(index);
    }
    return true;
}

bool MenuEntry::selected() const
{
    return selected_qty > 0 && (quantity || on_select);
}

// -1: Invert (MENU_SELECT_INVERT; used only in multiselect)
// -2: Select all (MENU_SELECT_ALL)
// a menu can be selected either if it defines a quantity, or an on_select
// function.
// TODO: fix this mess
void MenuEntry::select(int qty)
{
    const int real_max = quantity == 0 && on_select ? 1 : quantity;
    if (qty == MENU_SELECT_ALL)
        selected_qty = real_max;
    else if (qty == MENU_SELECT_INVERT)
        selected_qty = selected() ? 0 : real_max;
    else
        selected_qty = min(qty, real_max);
}

string MenuEntry::_get_text_preface() const
{
    if (level == MEL_ITEM && hotkeys_count())
        return make_stringf(" %s - ", keycode_to_name(hotkeys[0]).c_str());
    else if (level == MEL_ITEM && indent_no_hotkeys)
        return "     ";
    else
        return "";
}

string MenuEntry::get_text() const
{
    return _get_text_preface() + text;
}

void MenuEntry::wrap_text(int width)
{
    // Warning: console menus cannot handle multiline regular entries, use for
    // the title only (TODO)
    const int indent
#ifdef USE_TILE_LOCAL
        = 0; // tiles does line-wrapping inside the text
#else
        = static_cast<int>(_get_text_preface().size());
    width -= indent;
#endif
    if (width <= 0)
        return;
    linebreak_string(text, width, true, indent);
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
        feat = env.grid(p);
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

    const bool    fake = m->props.exists(FAKE_MON_KEY);
    const coord_def c  = m->pos;
    tileidx_t       ch = TILE_FLOOR_NORMAL;

    if (!fake)
    {
        ch = tileidx_feature(c);
        if (ch == TILE_FLOOR_NORMAL)
            ch = tile_env.flv(c).floor;
        else if (ch == TILE_WALL_NORMAL)
            ch = tile_env.flv(c).wall;
    }

    tileset.emplace_back(ch);

    if (m->attitude == ATT_FRIENDLY)
        tileset.emplace_back(TILE_HALO_FRIENDLY);
    else if (m->attitude == ATT_GOOD_NEUTRAL)
        tileset.emplace_back(TILE_HALO_GD_NEUTRAL);
    else if (m->neutral())
        tileset.emplace_back(TILE_HALO_NEUTRAL);
    else if (Options.tile_show_threat_levels.find("unusual") != string::npos
             && m->has_unusual_items())
        tileset.emplace_back(TILE_THREAT_UNUSUAL);
    else if (m->type == MONS_PLAYER_GHOST)
        switch (m->threat)
        {
        case MTHRT_TRIVIAL:
            tileset.emplace_back(TILE_THREAT_GHOST_TRIVIAL);
            break;
        case MTHRT_EASY:
            tileset.emplace_back(TILE_THREAT_GHOST_EASY);
            break;
        case MTHRT_TOUGH:
            tileset.emplace_back(TILE_THREAT_GHOST_TOUGH);
            break;
        case MTHRT_NASTY:
            tileset.emplace_back(TILE_THREAT_GHOST_NASTY);
            break;
        default:
            break;
        }
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
        // other animated objects use regular monster tiles (TODO: animate
        // armour's seems broken in this menu)
        item_def item;

        if (!fake && m->inv[MSLOT_WEAPON])
            item = *m->inv[MSLOT_WEAPON];

        if (fake || !item.defined())
        {
            item.base_type = OBJ_WEAPONS;
            item.sub_type  = WPN_LONG_SWORD;
            item.quantity  = 1;
        }
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
    else if (m->attitude == ATT_GOOD_NEUTRAL)
        tileset.emplace_back(TILEI_GOOD_NEUTRAL);
    else if (m->neutral())
        tileset.emplace_back(TILEI_NEUTRAL);
    else if (m->is(MB_PARALYSED))
        tileset.emplace_back(TILEI_PARALYSED);
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

    pack_tilep_set(tileset, player.doll);

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

void Menu::select_item_index(int idx, int qty)
{
    items[idx]->select(qty);
    m_ui.menu->update_item(idx);
#ifdef USE_TILE_WEB
    webtiles_update_item(idx);
#endif
}

// index = -1, do special action depending on qty
//    qty =  0 (MENU_SELECT_CLEAR): clear selection
//    qty = -1 (MENU_SELECT_INVERT): invert selection
//    qty = -2 (MENU_SELECT_ALL): select all or apply filter
// TODO: refactor in a better way
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
                    && (qty != MENU_SELECT_ALL || is_selectable(i)))
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
        select_item_index(si, qty);
    }
}

size_t Menu::item_count(bool include_headers) const
{
    size_t count = items.size();
    if (!include_headers)
    {
        for (const auto &item : items)
            if (item->level != MEL_ITEM)
            {
                ASSERT(count > 0);
                count--;
            }
    }
    return count;
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
    // sanitize hover in case items have changed. The first check here handles
    // a mouse hover case that set_hovered will not.
    if (!is_set(MF_ARROWS_SELECT) && last_hovered >= static_cast<int>(items.size()))
        last_hovered = -1;
    if (last_hovered >= 0)
        set_hovered(last_hovered);

    if (!alive)
        return;
#ifdef USE_TILE_WEB
    if (update_entries)
    {
        tiles.json_open_object();
        tiles.json_write_string("msg", "update_menu");
        tiles.json_write_int("total_items", items.size());
        tiles.json_write_int("last_hovered", last_hovered);
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

    // used as a hacky way of enforcing a min width for tiles when a more is
    // visible. results in consistent popup widths. Only has an effect if the
    // min width is explicitly set.
    const int width =
#ifdef USE_TILE_LOCAL
            0;
#else
            m_ui.menu->get_min_col_width();
#endif
    if (m_keyhelp_more)
    {
        // this case does not use `this->more` at all
        m_ui.more->set_more_template(
            pad_more_with(get_keyhelp(true), "", width),
            pad_more_with(get_keyhelp(false), "", width));
        m_ui.more->_expose();

        // visibility is handled in lower-level UI code, but for some reason
        // only a toggle from visible to invisible works on initial render
        m_ui.more->set_visible(true);
    }
    else
    {
        formatted_string shown_more = more.ops.empty()
            ? more
            : pad_more_with(more, formatted_string(""), width);
        m_ui.more->set_text(shown_more);
        m_ui.more->using_template = false;
        m_ui.more->set_visible(!shown_more.ops.empty());
    }

#ifdef USE_TILE_WEB
    if (!alive)
        return;
    tiles.json_open_object();
    tiles.json_write_string("msg", "update_menu");
    m_ui.more->webtiles_write_more();
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

MenuEntry *Menu::get_cur_title() const
{
    const bool first = (action_cycle == CYCLE_NONE
                        || menu_action == ACT_EXECUTE);

    // if title2 is set, use it as an alt title; otherwise don't change
    // titles
    return first ? title
                 : title2 ? title2 : title;
}

void Menu::update_title()
{
    if (!title || crawl_state.doing_prev_cmd_again)
        return;

    formatted_string fs;

    if (m_filter)
    {
        fs = formatted_string::parse_string(
            m_filter->get_prompt().c_str())
            // apply formatting only to the prompt
            + " " + m_filter->get_text();
    }
    else
        fs = calc_title();

    if (fs.empty())
    {
        const auto *t = get_cur_title();
        ASSERT(t);
        auto col = item_colour(t);
        string text = t->get_text();

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
    m_ui.more->set_margin_for_sdl(10, UIMenu::item_pad+UIMenu::pad_right, 0,
            tile_indent ? UIMenu::item_pad + 38 : 0);
#endif
    m_ui.title->set_text(fs);
#ifdef USE_TILE_WEB
    webtiles_set_title(fs);
#endif
}

void Menu::set_hovered(int index, bool force)
{
    if (!force && !is_set(MF_ARROWS_SELECT))
    {
        snap_in_page(index);
        return;
    }
    // intentionally goes to -1 on size 0
    last_hovered = min(index, static_cast<int>(items.size()) - 1);
#ifdef USE_TILE_LOCAL
    // don't crash if this gets called on local tiles before the menu has been
    // displayed. If your initial hover isn't showing up on local tiles, it
    // may be because of this -- adjust the timing so it is set after
    // update_menu is called.
    if (m_ui.menu->shown_items() == 0)
        return;
#endif

    m_ui.menu->set_hovered_entry(last_hovered);
    if (last_hovered >= 0)
        snap_in_page(last_hovered);
}

bool Menu::in_page(int index, bool strict) const
{
    int y1, y2;
    m_ui.menu->get_item_region(index, &y1, &y2);
    int vph = m_ui.scroller->get_region().height;
    int vpy = m_ui.scroller->get_scroll();
    const bool upper_in = (vpy <= y1 && y1 <= vpy+vph);
    const bool lower_in = (vpy <= y2 && y2 <= vpy+vph);
    return strict ? (lower_in && upper_in) : (lower_in || upper_in);
}

bool Menu::set_scroll(int index)
{
    // ui not yet set up. Setting this value now will force a
    // `set_scroll` call with the same index on first render.
    if (!ui_is_initialized())
    {
        m_ui.menu->set_initial_scroll(index);
        return false;
    }

    // TODO: code duplication, maybe could be refactored into lower-level code
    const int vph = m_ui.scroller->get_region().height;
    ASSERT(vph > 0);
    if (index < 0 || index >= static_cast<int>(items.size()))
        return false;

    int y1, y2;
    m_ui.menu->get_item_region(index, &y1, &y2);

    // special case: the immediately preceding items are a header of some kind.
    // Most important visually when the first item in a menu is a header
    const int any_preceding_headers = get_header_block(index).first;
    if (any_preceding_headers != index)
        m_ui.menu->get_item_region(any_preceding_headers, &y1, nullptr);

    const int vpy = m_ui.scroller->get_scroll();
    m_ui.scroller->set_scroll(y1
#ifdef USE_TILE_LOCAL
            - UI_SCROLLER_SHADE_SIZE / 2
#endif
            );

#ifdef USE_TILE_WEB
    // XX this doesn't force update the server, should it ever?
    webtiles_update_scroll_pos();
#endif

    return vpy != y1;
}

bool Menu::ui_is_initialized() const
{
    // is this really the best way to do this?? I arrived at this by
    // generalizing some code that used this specific check, but maybe it
    // should be done in some more general fashion...
    return m_ui.scroller && m_ui.scroller->get_region().height > 0;
}

bool Menu::item_visible(int index)
{
    // ui not yet set up. Use `set_scroll` for this case, or a hover will
    // be automatically snapped.
    if (!ui_is_initialized())
        return false;
    // TODO: code duplication, maybe could be refactored into lower-level code
    const int vph = m_ui.scroller->get_region().height;
    ASSERT(vph > 0);
    if (index < 0 || index >= static_cast<int>(items.size()))
        return false;

    int y1, y2;
    m_ui.menu->get_item_region(index, &y1, &y2);

    // special case: the immediately preceding items are a header of some kind.
    // Most important visually when the first item in a menu is a header
    const int any_preceding_headers = get_header_block(index).first;
    if (any_preceding_headers != index)
        m_ui.menu->get_item_region(any_preceding_headers, &y1, nullptr);

    const int vpy = m_ui.scroller->get_scroll();

    // full visibility -- should this be partial visibility?
    return y1 >= vpy && y2 < vpy + vph;
}

/// Ensure that the item at index is visible in the scroller. This happens
/// relative to the current scroll.
bool Menu::snap_in_page(int index)
{
    // ui not yet set up. Use `set_scroll` for this case, or a hover will
    // be automatically snapped.
    if (!ui_is_initialized())
        return false;
    // TODO: code duplication, maybe could be refactored into lower-level code
    const int vph = m_ui.scroller->get_region().height;
    ASSERT(vph > 0);
    if (index < 0 || index >= static_cast<int>(items.size()))
        return false;

    int y1, y2;
    m_ui.menu->get_item_region(index, &y1, &y2);

    // special case: the immediately preceding items are a header of some kind.
    // Most important visually when the first item in a menu is a header
    const int any_preceding_headers = get_header_block(index).first;
    if (any_preceding_headers != index)
        m_ui.menu->get_item_region(any_preceding_headers, &y1, nullptr);

    const int vpy = m_ui.scroller->get_scroll();

#ifdef USE_TILE_LOCAL
    // on local tiles, when scrolling longer menus, the scroller will apply a
    // shade to the edge of the scroller. Compensate for this for non-end menu
    // items.
    const int shade = (index > 0 || index < static_cast<int>(items.size()) - 1)
        ? UI_SCROLLER_SHADE_SIZE / 2 : 0;
#else
    const int shade = 0;
#endif

    // the = for these is to apply the local tiles shade adjustment if necessary
    if (y1 <= vpy)
    {
        // scroll up
        m_ui.scroller->set_scroll(y1 - shade);
    }
    else if (y2 >= vpy + vph)
    {
        // scroll down
        m_ui.scroller->set_scroll(y2 - vph + shade);
    }
    else
        return false; // already in page
    return true;
}

bool Menu::page_down()
{
    int new_hover = -1;
    if (is_set(MF_ARROWS_SELECT) && last_hovered < 0 && items.size() > 0)
        last_hovered = 0;
    // preserve relative position

    int col = 0;
    if (last_hovered >= 0 && m_ui.menu)
        m_ui.menu->get_item_gridpos(last_hovered, nullptr, &col);

    if (last_hovered >= 0)
    {
        new_hover = in_page(last_hovered)
                        ? max(0, last_hovered - get_first_visible(true, col))
                        : 0;
    }
    int dy = m_ui.scroller->get_region().height - m_ui.menu->get_scroll_context();
    int y = m_ui.scroller->get_scroll();
    bool at_bottom = y+dy >= m_ui.menu->get_region().height;
    // don't scroll further if the last item is already visible
    // (TODO: I don't understand why this check is necessary, but without it,
    // you sometimes unpredictably end up with the last element on its own page)
    if (items.size() && !in_page(static_cast<int>(items.size()) - 1, true))
        m_ui.scroller->set_scroll(y+dy);

    if (new_hover >= 0)
    {
        // if pgdn wouldn't change the hover, move it to the last element
        if (is_set(MF_ARROWS_SELECT) && get_first_visible(true, col) + new_hover == last_hovered)
            set_hovered(items.size() - 1);
        else
            set_hovered(get_first_visible(true, col) + new_hover);
        if (items[last_hovered]->level != MEL_ITEM)
            cycle_hover(true); // reverse so we don't overshoot
    }

#ifndef USE_TILE_LOCAL
    if (!at_bottom)
        m_ui.menu->set_showable_height(y+dy+dy);
#endif
    return !at_bottom;
}

bool Menu::page_up()
{
    int new_hover = -1;
    if (is_set(MF_ARROWS_SELECT) && last_hovered < 0 && items.size() > 0)
        last_hovered = 0;

    if (last_hovered >= 0)
    {
        new_hover = in_page(last_hovered)
                        ? max(0, last_hovered - get_first_visible(true))
                        : 0;
    }

    int dy = m_ui.scroller->get_region().height - m_ui.menu->get_scroll_context();
    int y = m_ui.scroller->get_scroll();
    m_ui.scroller->set_scroll(y-dy);
    if (new_hover >= 0)
    {
        // if pgup wouldn't change the hover, select the first element
        if (is_set(MF_ARROWS_SELECT) && get_first_visible(true) + new_hover == last_hovered)
            new_hover = 0;
        set_hovered(get_first_visible(true) + new_hover);
        if (items[last_hovered]->level != MEL_ITEM)
            cycle_hover(); // forward so we don't overshoot
    }

#ifndef USE_TILE_LOCAL
    m_ui.menu->set_showable_height(y);
#endif
    return y > 0;
}

bool Menu::line_down()
{
    // check if we are already at the end.
    if (items.empty() || in_page(static_cast<int>(items.size()) - 1, true))
        return false;

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

void Menu::cycle_hover(bool reverse, bool preserve_row, bool preserve_col)
{
    if (!is_set(MF_ARROWS_SELECT))
        return;
    int items_tried = 0;
    const int max_items = is_set(MF_WRAP) ? items.size()
                        : reverse
                          ? last_hovered
                          : items.size() - max(last_hovered, 0);
    int new_hover = last_hovered;
    if (reverse && last_hovered < 0)
        new_hover = 0;
    int row = 0;
    int col = 0;
    if (last_hovered >= 0 && m_ui.menu)
        m_ui.menu->get_item_gridpos(last_hovered, &row, &col);
    bool found = false;
    while (items_tried < max_items)
    {
        new_hover = new_hover + (reverse ? -1 : 1);
        items_tried++;
        // try to find a non-heading to hover over
        const int sz = static_cast<int>(items.size());
        if (is_set(MF_WRAP) && sz > 0)
            new_hover = (new_hover + sz) % sz;
        new_hover = max(0, min(new_hover, sz - 1));

        int cur_row, cur_col;
        if (m_ui.menu && (preserve_row || preserve_col))
        {
            m_ui.menu->get_item_gridpos(new_hover, &cur_row, &cur_col);
            if (preserve_row && cur_row != row || preserve_col && cur_col != col)
                continue;
        }

        if (items[new_hover]->level == MEL_ITEM)
        {
            found = true;
            break;
        }
    }
    if (!found)
        return;

    set_hovered(new_hover);
#ifdef USE_TILE_WEB
    webtiles_update_scroll_pos();
#endif
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

/// Return a range for index that includes at least one non-header if possible,
/// and any sequence of adjacent headers. Used for display logic to try to
/// show headers before any selected item. Doesn't check for valid indices
pair<int, int> Menu::get_header_block(int index) const
{
    int first = index;
    int last = index;
    while (first >= 1 && items[first - 1]->level != MEL_ITEM)
        first--;
    // if index is a header, scan forward to look for a non-headed
    while (last + 1 < static_cast<int>(items.size()) && items[last]->level != MEL_ITEM)
        last++;
    return make_pair(first, last);
}

int Menu::next_block_from(int index, bool forward, bool wrap) const
{
    const auto cur_block = get_header_block(index);
    int next = forward ? cur_block.second + 1 : cur_block.first - 1;
    if (wrap)
        next = next % items.size();
    else
        next = max(min(next, static_cast<int>(items.size()) - 1), 0);
    return get_header_block(next).first;
}

bool Menu::cycle_headers(bool forward)
{
    if (items.size() == 0)
        return false;
    // XX this doesn't work quite right if called before the menu is displayed
    int start = is_set(MF_ARROWS_SELECT) ? max(last_hovered, 0)
                                         : get_first_visible();
    start = get_header_block(start).first;
    int cur = next_block_from(start, forward, true);
    while (cur != start)
    {
        if (items[cur]->level == MEL_SUBTITLE || items[cur]->level == MEL_TITLE)
        {
            if (!item_visible(cur) || !is_set(MF_ARROWS_SELECT))
                set_scroll(cur);
            if (is_set(MF_ARROWS_SELECT))
            {
                set_hovered(cur);
                cycle_hover(); // cycle to get a valid hover
            }
#ifdef USE_TILE_WEB
            // cycle_headers doesn't currently have a client-side
            // implementation, so force-send the server-side scroll
            webtiles_update_scroll_pos(true);
#endif
            return true;
        }
        cur = next_block_from(cur, forward, true);
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
    tiles.json_write_int("last_hovered", last_hovered);
    if (replace)
        tiles.json_write_int("replace", 1);

    webtiles_write_title();

    m_ui.more->webtiles_write_more();

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

void Menu::webtiles_scroll(int first, int hover)
{
    // catch and ignore stale scroll events
    if (first >= static_cast<int>(items.size()))
        return;

    int item_y;
    m_ui.menu->get_item_region(first, &item_y, nullptr);
    if (m_ui.scroller->get_scroll() != item_y)
    {
        m_ui.scroller->set_scroll(item_y);
        // The `set_hovered` call here will trigger a snap, which is a way
        // for the server to get out of sync with the client. We therefore
        // only call it if the hover has actually changed on the client side.
        // if (last_hovered != hover)
            set_hovered(hover);
        webtiles_update_scroll_pos();
        ui::force_render();
    }
}

void Menu::webtiles_handle_item_request(int start, int end)
{
    start = min(max(0, start), (int)items.size()-1);
    if (end < start)
        end = start;
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
        webtiles_write_item(items[i]);

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

void Menu::webtiles_update_scroll_pos(bool force) const
{
    tiles.json_open_object();
    tiles.json_write_string("msg", "menu_scroll");
    tiles.json_write_int("first", get_first_visible());
    tiles.json_write_int("last_hovered", last_hovered);
    tiles.json_write_bool("force", force);
    tiles.json_close_object();
    tiles.finish_message();
}

void Menu::webtiles_write_title() const
{
    // the title object only exists for backwards compatibility
    tiles.json_open_object("title");
    tiles.json_write_string("text", _webtiles_title.to_colour_string(LIGHTGRAY));
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

    webtiles_write_tiles(*me);

    tiles.json_close_object();
}
#endif // USE_TILE_WEB

/////////////////////////////////////////////////////////////////
// Menu colouring
//

int menu_colour(const string &text, const string &prefix, const string &tag, bool strict)
{
    const string tmp_text = prefix + text;

    for (const colour_mapping &cm : Options.menu_colour_mappings)
    {
        const bool match_any = !strict &&
            (cm.tag.empty() || cm.tag == "item" || cm.tag == "any");
        if ((match_any
                || cm.tag == tag || cm.tag == "inventory" && tag == "pickup")
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

int linebreak_string(string& s, int maxcol, bool indent, int force_indent)
{
    // [ds] Don't loop forever if the user is playing silly games with
    // their term size.
    if (maxcol < 1)
        return 0;

    int breakcount = 0;
    string res;

    while (!s.empty())
    {
        res += wordwrap_line(s, maxcol, true, indent, force_indent);
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

void ToggleableMenu::add_toggle_from_command(command_type cmd)
{
    // use this to align toggle with a command, e.g. CMD_MENU_CYCLE_MODE
    // XX do away with ToggleableMenu and just use Menu code for this?
    const vector<int> keys = command_to_keys(cmd);
    for (auto k : keys)
        add_toggle_key(k);
}

// TODO: do away with this somehow? Maybe this whole subclass isn't really
// needed?
int ToggleableMenu::pre_process(int key)
{
    if (find(toggle_keys.begin(), toggle_keys.end(), key) != toggle_keys.end())
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
        return 0;
    }
    return key;
}
