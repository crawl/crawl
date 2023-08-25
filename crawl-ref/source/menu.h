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
                const string &tag = "",
                bool strict=true);

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
    bool indent_no_hotkeys;
    string preface_format;
    void *data;
    function<bool(const MenuEntry&)> on_select;

#ifdef USE_TILE
    vector<tile_def> tiles;
#endif

public:
    MenuEntry(const string &txt = string(),
               MenuEntryLevel lev = MEL_ITEM,
               int qty  = 0,
               int hotk = 0) :
        text(txt), quantity(qty), selected_qty(0), colour(-1),
        hotkeys(), level(lev),
        indent_no_hotkeys(false),
        data(nullptr),
        on_select(nullptr),
        m_enabled(true)
    {
        colour = (lev == MEL_ITEM     ?  MENU_ITEM_STOCK_COLOUR :
                  lev == MEL_SUBTITLE ?  BLUE  :
                                         WHITE);
        if (hotk)
            hotkeys.push_back(hotk);
    }

    // n.b. select code requires that the qty value be >0 (TODO, why)
    MenuEntry(const string &txt, int hotk,
                        function<bool(const MenuEntry&)> action)
        : MenuEntry(txt, MEL_ITEM, 1, hotk)
    {
        on_select = action;
    }

    virtual ~MenuEntry() { }

    bool is_enabled() const { return m_enabled; }
    void set_enabled(bool b) { m_enabled = b; }

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
        return is_enabled()
            && find(hotkeys.begin(), hotkeys.end(), key) != hotkeys.end();
    }

    int hotkeys_count() const
    {
        return is_enabled() ? static_cast<int>(hotkeys.size()) : 0;
    }

    bool is_primary_hotkey(int key) const
    {
        return hotkeys_count() && hotkeys.size() && hotkeys[0] == key;
    }

    virtual string get_text() const;
    void wrap_text(int width=MIN_COLS);

    virtual int highlight_colour(bool /*unused in superclass*/ = false) const
    {
        return menu_colour(get_text(), "", tag);
    }

    virtual bool selected() const;
    virtual void select(int qty = -1);

    virtual string get_filter_text() const
    {
        return get_text();
    }

    virtual bool get_tiles(vector<tile_def>& tileset) const;

    virtual void add_tile(tile_def tile);

protected:
    virtual string _get_text_preface() const;
    bool m_enabled;
};

class ToggleableMenuEntry : public MenuEntry
{
public:
    string alt_text;

    ToggleableMenuEntry(const string &txt = string(),
                        const string &alt_txt = string(),
                        MenuEntryLevel lev = MEL_ITEM,
                        int qty = 0, int hotk = 0) :
        MenuEntry(txt, lev, qty, hotk), alt_text(alt_txt) {}

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

// if you update this, update mf in enums.js
enum MenuFlag
{
    MF_NOSELECT         = 0x00001,   ///< No selection is permitted
    MF_SINGLESELECT     = 0x00002,   ///< Select just one item
    MF_MULTISELECT      = 0x00004,   ///< Select multiple items
    MF_SELECT_QTY       = 0x00008,   ///< Allow partial selections by quantity
    MF_ANYPRINTABLE     = 0x00010,   ///< Any printable character is valid, and
                                     ///< closes the menu.
    MF_SELECT_BY_PAGE   = 0x00020,   ///< Allow selections to occur only on
                                     ///< currently-visible page.
    MF_INIT_HOVER       = 0x00040,   ///< Show the hover on initial display
    MF_WRAP             = 0x00080,   ///< Paging past the end will wrap back.
    MF_ALLOW_FILTER     = 0x00100,   ///< Control-F will ask for regex and
                                     ///< select the appropriate items.
    MF_ALLOW_FORMATTING = 0x00200,   ///< Parse index for formatted-string
    MF_TOGGLE_ACTION    = 0x00400,   ///< ToggleableMenu toggles action as well
    MF_NO_WRAP_ROWS     = 0x00800,   ///< For menus used as tables (eg. ability)
    MF_START_AT_END     = 0x01000,   ///< Scroll to end of list
    MF_SECONDARY_SCROLL = 0x02000,   ///< Secondary hotkeys scroll, rather than select
    MF_QUIET_SELECT     = 0x04000,   ///< No selection box and no count.

    MF_USE_TWO_COLUMNS  = 0x08000,   ///< Use two columns for long menus
    MF_UNCANCEL         = 0x10000,   ///< Menu is uncancellable
    MF_SPECIAL_MINUS    = 0x20000,   ///< '-' isn't PGUP or clear multiselect
    MF_ARROWS_SELECT    = 0x40000,   ///< arrow keys select, rather than scroll
    MF_SHOW_EMPTY       = 0x80000,   ///< don't auto-exit empty menus
    MF_GRID_LAYOUT     = 0x100000,   ///< use a grid-style layout, filling by width first
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

#define MENU_SELECT_CLEAR 0
#define MENU_SELECT_INVERT -1
#define MENU_SELECT_ALL -2

class Menu
{
    friend class UIMenu;
    friend class UIMenuPopup;
public:
    Menu(int flags = MF_MULTISELECT, const string& tagname = "", KeymapContext kmc = KMC_MENU);

    virtual ~Menu();

    // Remove all items from the Menu, leave title intact.
    virtual void clear();

    virtual void set_flags(int new_flags);
    int  get_flags() const        { return flags; }
    virtual bool is_set(int flag) const;
    void set_tag(const string& t) { tag = t; }

    bool minus_is_command() const;
    // Sets a replacement for the default -more- string.
    void set_more(const formatted_string &more);
    void set_more(const string s);
    // Shows a stock message about scrolling the menu instead of -more-
    void set_more();
    const formatted_string &get_more() const { return more; }
    virtual string get_keyhelp(bool scrollable) const;
    void set_min_col_width(int w);

    void set_highlighter(MenuHighlighter *h);
    void set_title(MenuEntry *e, bool first = true, bool indent = false);
    void set_title(const string &t, bool first = true, bool indent = false);
    void add_entry(MenuEntry *entry);
    void add_entry(unique_ptr<MenuEntry> entry)
    {
        add_entry(entry.release());
    }
    void get_selected(vector<MenuEntry*> *sel) const;

    void set_select_filter(vector<text_pattern> filter)
    {
        select_filter = filter;
    }

    bool ui_is_initialized() const;

    virtual void update_menu(bool update_entries = false);
    virtual void set_hovered(int index, bool force=false);
    bool set_scroll(int index);
    bool in_page(int index, bool strict=false) const;
    bool snap_in_page(int index);
    int get_first_visible(bool skip_init_headers=false, int col=-1) const;
    bool item_visible(int index);
    MenuEntry *get_cur_title() const;

    virtual int getkey() const { return lastch; }

    void reset();
    virtual vector<MenuEntry *> show(bool reuse_selections = false);
    vector<MenuEntry *> selected_entries() const;

    size_t item_count(bool include_headers=true) const;

    // Get entry index, skipping quantity 0 entries. Returns -1 if not found.
    int get_entry_index(const MenuEntry *e) const;

    virtual int item_colour(const MenuEntry *me) const;

    typedef string (*selitem_tfn)(const vector<MenuEntry*> *sel);

    selitem_tfn      f_selitem;
    function<int(int)> f_keyfilter;
    function<bool(const MenuEntry&)> on_single_selection;
    function<bool(const MenuEntry&)> on_examine;
    function<bool()> on_show;

    enum cycle  { CYCLE_NONE, CYCLE_TOGGLE, CYCLE_CYCLE } action_cycle;
    enum action { ACT_EXECUTE, ACT_EXAMINE, ACT_MISC, ACT_NUM } menu_action;
    void cycle_hover(bool reverse=false, bool preserve_row=false, bool preserve_col=false);
    virtual bool page_down();
    virtual bool line_down();
    virtual bool page_up();
    virtual bool line_up();
    virtual bool cycle_headers(bool forward=true);
    virtual bool cycle_mode(bool forward=true);

    bool title_prompt(char linebuf[], int bufsz, const char* prompt, string help_tag="");

    virtual bool process_key(int keyin);
    virtual bool skip_process_command(int keyin);
    virtual command_type get_command(int keyin);
    virtual bool process_command(command_type cmd);

#ifdef USE_TILE_WEB
    void webtiles_write_menu(bool replace = false) const;
    void webtiles_scroll(int first, int hover);
    void webtiles_handle_item_request(int start, int end);
#endif
protected:
    string _title_prompt_help_tag;

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
    bool more_needs_init;
    bool remap_numpad;

    int last_hovered;
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
    void webtiles_update_scroll_pos(bool force=false) const;

    virtual void webtiles_write_title() const;
    virtual void webtiles_write_item(const MenuEntry *me) const;

    bool _webtiles_title_changed;
    formatted_string _webtiles_title;
#endif

    virtual formatted_string calc_title();
    void update_more();
    pair<int,int> get_header_block(int index) const;
    int next_block_from(int index, bool forward, bool wrap) const;

    virtual int pre_process(int key);

    void deselect_all(bool update_view = true);
    virtual void select_items(int key, int qty = MENU_SELECT_INVERT); // XX why is the default invert?
    virtual void select_item_index(int idx, int qty = MENU_SELECT_INVERT);
    void select_index(int index, int qty = -1);
    virtual bool examine_index(int i);
    bool examine_by_key(int keyin);
    int hotkey_to_index(int key, bool primary_only);
    pair<int,int> hotkey_range(int key);
    bool process_selection();

    bool is_hotkey(int index, int key);
    virtual bool is_selectable(int index) const;

    virtual string help_key() const { return ""; }

    virtual void update_title();
    bool filter_with_regex(const char *re);
};

/// Allows toggling by specific keys.
class ToggleableMenu : public Menu
{
public:
    ToggleableMenu(int _flags = MF_MULTISELECT)
        : Menu(_flags) {}
    void add_toggle_key(int newkey) { toggle_keys.push_back(newkey); }
    void add_toggle_from_command(command_type cmd);
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

int linebreak_string(string& s, int maxcol, bool indent = false, int force_indent=-1);
string get_linebreak_string(const string& s, int maxcol);
formatted_string pad_more_with(formatted_string s,
                                    const formatted_string &pad, int min_width=MIN_COLS);
string pad_more_with(const string &s, const string &pad, int min_width=MIN_COLS);
string pad_more_with_esc(const string &s);
string menu_keyhelp_cmd(command_type cmd);
string menu_keyhelp_select_keys();
string hyphenated_hotkey_letters(int how_many, char first);
