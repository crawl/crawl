/**
 * @file
 * @brief Menus and associated malarkey.
**/

#pragma once

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "tiles-build-specific.h"
#include "cio.h"
#include "format.h"
#ifdef USE_TILE
 #include "tiledoll.h"
#endif
#ifdef USE_TILE_LOCAL
 #include "tilebuf.h"
#endif
#include "ui.h"

class formatted_string;

enum MenuEntryLevel
{
    MEL_NONE = -1,
    MEL_TITLE,
    MEL_SUBTITLE,
    MEL_ITEM,
};

struct menu_letter
{
    char letter;

    menu_letter() : letter('a') { }
    menu_letter(char c) : letter(c) { }

    operator char () const { return letter; }

    const menu_letter &operator ++ ()
    {
        letter = letter == 'z'? 'A' :
                 letter == 'Z'? 'a' :
                                letter + 1;
        return *this;
    }

    // dummy postfix argument unnamed to stop gcc from complaining
    menu_letter operator ++ (int)
    {
        menu_letter copy = *this;
        operator++();
        return copy;
    }
};

// XXX Use inheritance instead of duplicate code
struct menu_letter2
{
    char letter;

    menu_letter2() : letter('a') { }
    menu_letter2(char c) : letter(c) { }

    operator char () const { return letter; }
    const menu_letter2 &operator ++ ()
    {
        letter = letter == 'z'? '0' :
                 letter == '9'? 'a' :
                                letter + 1;
        return *this;
    }

    menu_letter2 operator ++ (int)
    {
        menu_letter2 copy = *this;
        operator++();
        return copy;
    }
};

struct item_def;
class Menu;

int menu_colour(const string &itemtext,
                const string &prefix = "",
                const string &tag = "");

const int MENU_ITEM_STOCK_COLOUR = LIGHTGREY;
class MenuEntry
{
public:
    string tag;
    string text;
    int quantity, selected_qty;
    colour_t colour;
    vector<int> hotkeys;
    MenuEntryLevel level;
    bool preselected;
    void *data;

#ifdef USE_TILE
    vector<tile_def> tiles;
#endif

public:
    MenuEntry(const string &txt = string(),
               MenuEntryLevel lev = MEL_ITEM,
               int qty  = 0,
               int hotk = 0,
               bool preselect = false) :
        text(txt), quantity(qty), selected_qty(0), colour(-1),
        hotkeys(), level(lev), preselected(preselect), data(nullptr)
    {
        colour = (lev == MEL_ITEM     ?  MENU_ITEM_STOCK_COLOUR :
                  lev == MEL_SUBTITLE ?  BLUE  :
                                         WHITE);
        if (hotk)
            hotkeys.push_back(hotk);
    }
    virtual ~MenuEntry() { }

    bool operator<(const MenuEntry& rhs) const
    {
        return text < rhs.text;
    }

    void add_hotkey(int key)
    {
        if (key && !is_hotkey(key))
            hotkeys.push_back(key);
    }

    bool is_hotkey(int key) const
    {
        return find(hotkeys.begin(), hotkeys.end(), key) != hotkeys.end();
    }

    bool is_primary_hotkey(int key) const
    {
        return hotkeys.size() && hotkeys[0] == key;
    }

    virtual string get_text(const bool unused = false) const
    {
        UNUSED(unused);
        if (level == MEL_ITEM && hotkeys.size())
        {
            char buf[300];
            snprintf(buf, sizeof buf,
                    " %c %c %s", hotkeys[0], preselected ? '+' : '-',
                                 text.c_str());
            return string(buf);
        }
        return text;
    }

    virtual int highlight_colour() const
    {
        return menu_colour(get_text(), "", tag);
    }

    virtual bool selected() const
    {
        return selected_qty > 0 && quantity;
    }

    // -1: Invert
    // -2: Select all
    virtual void select(int qty = -1)
    {
        if (qty == -2)
            selected_qty = quantity;
        else if (selected())
            selected_qty = 0;
        else if (quantity)
            selected_qty = (qty == -1 ? quantity : qty);
    }

    virtual string get_filter_text() const
    {
        return get_text();
    }

    virtual bool get_tiles(vector<tile_def>& tileset) const;

    virtual void add_tile(tile_def tile);
};

class ToggleableMenuEntry : public MenuEntry
{
public:
    string alt_text;

    ToggleableMenuEntry(const string &txt = string(),
                        const string &alt_txt = string(),
                        MenuEntryLevel lev = MEL_ITEM,
                        int qty = 0, int hotk = 0,
                        bool preselect = false) :
        MenuEntry(txt, lev, qty, hotk, preselect), alt_text(alt_txt) {}

    void toggle() { text.swap(alt_text); }
};

class MonsterMenuEntry : public MenuEntry
{
public:
    MonsterMenuEntry(const string &str, const monster_info* mon, int hotkey);

#ifdef USE_TILE
    virtual bool get_tiles(vector<tile_def>& tileset) const override;
#endif
};

#ifdef USE_TILE
class PlayerMenuEntry : public MenuEntry
{
public:
    PlayerMenuEntry(const string &str);

    virtual bool get_tiles(vector<tile_def>& tileset) const override;
};
#endif

class FeatureMenuEntry : public MenuEntry
{
public:
    coord_def            pos;
    dungeon_feature_type feat;

    FeatureMenuEntry(const string &str, const coord_def p, int hotkey);
    FeatureMenuEntry(const string &str, const dungeon_feature_type f,
                     int hotkey);

#ifdef USE_TILE
    virtual bool get_tiles(vector<tile_def>& tileset) const override;
#endif
};

class MenuHighlighter
{
public:
    virtual int entry_colour(const MenuEntry *entry) const;
    virtual ~MenuHighlighter() { }
};

enum MenuFlag
{
    MF_NOSELECT         = 0x00001,   ///< No selection is permitted
    MF_SINGLESELECT     = 0x00002,   ///< Select just one item
    MF_MULTISELECT      = 0x00004,   ///< Select multiple items
    MF_NO_SELECT_QTY    = 0x00008,   ///< Disallow partial selections
    MF_ANYPRINTABLE     = 0x00010,   ///< Any printable character is valid, and
                                     ///< closes the menu.
    MF_SELECT_BY_PAGE   = 0x00020,   ///< Allow selections to occur only on
                                     ///< currently-visible page.
    MF_ALWAYS_SHOW_MORE = 0x00040,   ///< Always show the -more- footer
    MF_WRAP             = 0x00080,   ///< Paging past the end will wrap back.
    MF_ALLOW_FILTER     = 0x00100,   ///< Control-F will ask for regex and
                                     ///< select the appropriate items.
    MF_ALLOW_FORMATTING = 0x00200,   ///< Parse index for formatted-string
    MF_TOGGLE_ACTION    = 0x00400,   ///< ToggleableMenu toggles action as well
    MF_NO_WRAP_ROWS     = 0x00800,   ///< For menus used as tables (eg. ability)
    MF_START_AT_END     = 0x01000,   ///< Scroll to end of list
    MF_PRESELECTED      = 0x02000,   ///< Has a preselected entry.
    MF_QUIET_SELECT     = 0x04000,   ///< No selection box and no count.

    MF_USE_TWO_COLUMNS  = 0x08000,   ///< Only valid for tiles menus
    MF_UNCANCEL         = 0x10000,   ///< Menu is uncancellable
    MF_SPECIAL_MINUS    = 0x20000,   ///< '-' isn't PGUP or clear multiselect
};

class UIMenu;
class UIMenuPopup;
class UIShowHide;
class UIMenuMore;

///////////////////////////////////////////////////////////////////////
// NOTE
// As a general contract, any pointers you pass to Menu methods are OWNED BY
// THE MENU, and will be deleted by the Menu on destruction. So any pointers
// you pass in MUST be allocated with new, or Crawl will crash.

#define NUMBUFSIZ 10

class Menu
{
    friend class UIMenu;
    friend class UIMenuPopup;
public:
    Menu(int flags = MF_MULTISELECT, const string& tagname = "", KeymapContext kmc = KMC_MENU);

    virtual ~Menu();

    // Remove all items from the Menu, leave title intact.
    void clear();

    virtual void set_flags(int new_flags);
    int  get_flags() const        { return flags; }
    virtual bool is_set(int flag) const;
    void set_tag(const string& t) { tag = t; }

    bool minus_is_pageup() const;
    // Sets a replacement for the default -more- string.
    void set_more(const formatted_string &more);
    // Shows a stock message about scrolling the menu instead of -more-
    void set_more();
    const formatted_string &get_more() const { return more; }

    void set_highlighter(MenuHighlighter *h);
    void set_title(MenuEntry *e, bool first = true, bool indent = false);
    void add_entry(MenuEntry *entry);
    void add_entry(unique_ptr<MenuEntry> entry)
    {
        add_entry(entry.release());
    }
    void get_selected(vector<MenuEntry*> *sel) const;
    virtual int get_cursor() const;

    void set_select_filter(vector<text_pattern> filter)
    {
        select_filter = filter;
    }

    void update_menu(bool update_entries = false);

    virtual int getkey() const { return lastch; }

    void reset();
    virtual vector<MenuEntry *> show(bool reuse_selections = false);
    vector<MenuEntry *> selected_entries() const;

    size_t item_count() const    { return items.size(); }

    // Get entry index, skipping quantity 0 entries. Returns -1 if not found.
    int get_entry_index(const MenuEntry *e) const;

    virtual int item_colour(const MenuEntry *me) const;

    typedef string (*selitem_tfn)(const vector<MenuEntry*> *sel);
    typedef int (*keyfilter_tfn)(int keyin);

    selitem_tfn      f_selitem;
    keyfilter_tfn    f_keyfilter;
    function<bool(const MenuEntry&)> on_single_selection;

    enum cycle  { CYCLE_NONE, CYCLE_TOGGLE, CYCLE_CYCLE } action_cycle;
    enum action { ACT_EXECUTE, ACT_EXAMINE, ACT_MISC, ACT_NUM } menu_action;

#ifdef USE_TILE_WEB
    void webtiles_write_menu(bool replace = false) const;
    void webtiles_scroll(int first);
    void webtiles_handle_item_request(int start, int end);
#endif
protected:
    MenuEntry *title;
    MenuEntry *title2;
    bool m_indent_title;

    int flags;
    string tag;

    int cur_page;
    int num_pages;

    formatted_string more;
    bool m_keyhelp_more;

    vector<MenuEntry*>  items;
    vector<MenuEntry*>  sel;
    vector<text_pattern> select_filter;

    // Class that is queried to colour menu entries.
    MenuHighlighter *highlighter;

    int num;

    int lastch;

    bool alive;

    int last_selected;
    KeymapContext m_kmc;

    resumable_line_reader *m_filter;

    struct {
        shared_ptr<ui::Popup> popup;
        shared_ptr<UIMenu> menu;
        shared_ptr<ui::Scroller> scroller;
        shared_ptr<ui::Text> title;
        shared_ptr<UIMenuMore> more;
        shared_ptr<UIShowHide> more_bin;
        shared_ptr<ui::Box> vbox;
    } m_ui;

protected:
    void check_add_formatted_line(int firstcol, int nextcol,
                                  string &line, bool check_eol);
    void do_menu();
    virtual string get_select_count_string(int count) const;

#ifdef USE_TILE_WEB
    void webtiles_set_title(const formatted_string title);

    void webtiles_write_tiles(const MenuEntry& me) const;
    void webtiles_update_items(int start, int end) const;
    void webtiles_update_item(int index) const;
    void webtiles_update_title() const;
    void webtiles_update_scroll_pos() const;

    virtual void webtiles_write_title() const;
    virtual void webtiles_write_item(const MenuEntry *me) const;

    bool _webtiles_title_changed;
    formatted_string _webtiles_title;
#endif

    virtual formatted_string calc_title();
    void update_more();
    virtual bool page_down();
    virtual bool line_down();
    virtual bool page_up();
    virtual bool line_up();

    virtual int pre_process(int key);
    virtual int post_process(int key);

    bool in_page(int index) const;
    int get_first_visible() const;

    void deselect_all(bool update_view = true);
    virtual void select_items(int key, int qty = -1);
    virtual void select_item_index(int idx, int qty, bool draw_cursor = true);
    void select_index(int index, int qty = -1);

    bool is_hotkey(int index, int key);
    virtual bool is_selectable(int index) const;

    bool title_prompt(char linebuf[], int bufsz, const char* prompt);

    virtual bool process_key(int keyin);

    virtual string help_key() const { return ""; }

    virtual void update_title();
protected:
    bool filter_with_regex(const char *re);
};

/// Allows toggling by specific keys.
class ToggleableMenu : public Menu
{
public:
    ToggleableMenu(int _flags = MF_MULTISELECT)
        : Menu(_flags) {}
    void add_toggle_key(int newkey) { toggle_keys.push_back(newkey); }
protected:
    virtual int pre_process(int key) override;

    vector<int> toggle_keys;
};

// This is only tangentially related to menus, but what the heck.
// Note, column margins start on 1, not 0.
class column_composer
{
public:
    // Number of columns and left margins for 2nd, 3rd, ... nth column.
    column_composer(int ncols, ...);

    void clear();
    void add_formatted(int ncol,
            const string &tagged_text,
            bool  add_separator = true,
            int   margin = -1);

    vector<formatted_string> formatted_lines() const;

private:
    struct column;
    void compose_formatted_column(
            const vector<formatted_string> &lines,
            int start_col,
            int margin);
    void strip_blank_lines(vector<formatted_string> &) const;

private:
    struct column
    {
        int margin;
        int lines;

        column(int marg = 1) : margin(marg), lines(0) { }
    };

    vector<column> columns;
    vector<formatted_string> flines;
};

int linebreak_string(string& s, int maxcol, bool indent = false);
string get_linebreak_string(const string& s, int maxcol);
