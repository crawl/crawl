/*
 *  File:       menu.h
 *  Summary:    Menus and associated malarkey.
 *  Written by: Darshan Shaligram
 */

#ifndef __MENU_H__
#define __MENU_H__

#include <string>
#include <vector>
#include <algorithm>
#include <time.h>
#include "externs.h"
#include "format.h"
#include "defines.h"
#include "libutil.h"

#ifdef USE_TILE
 #include "tilebuf.h"
 #include "tiledoll.h"
#endif

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
        this->operator++();
        return copy;
    }
};

struct item_def;
class Menu;

int menu_colour(const std::string &itemtext,
                const std::string &prefix = "",
                const std::string &tag = "");

class MenuEntry
{
public:
    std::string tag;
    std::string text;
    int quantity, selected_qty;
    int colour;
    std::vector<int> hotkeys;
    MenuEntryLevel level;
    void *data;

#ifdef USE_TILE
    std::vector<tile_def> tiles;
#endif

public:
    MenuEntry(const std::string &txt = std::string(),
               MenuEntryLevel lev = MEL_ITEM,
               int qty  = 0,
               int hotk = 0) :
        text(txt), quantity(qty), selected_qty(0), colour(-1),
        hotkeys(), level(lev), data(NULL)
    {
        colour = (lev == MEL_ITEM     ?  LIGHTGREY :
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
        return (hotkeys.size()? hotkeys[0] == key : false);
    }

    virtual std::string get_text(const bool unused = false) const
    {
        if (level == MEL_ITEM && hotkeys.size())
        {
            char buf[300];
            snprintf(buf, sizeof buf,
                    " %c - %s", hotkeys[0], text.c_str());
            return std::string(buf);
        }
        return text;
    }

    virtual int highlight_colour() const
    {
        return (menu_colour(get_text(), "", tag));
    }

    virtual bool selected() const
    {
        return (selected_qty > 0 && quantity);
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
            selected_qty = (qty == -1? quantity : qty);
    }

    virtual std::string get_filter_text() const
    {
        return get_text();
    }

#ifdef USE_TILE
    virtual bool get_tiles(std::vector<tile_def>& tileset) const;

    virtual void add_tile(tile_def tile);
#endif
};

class ToggleableMenuEntry : public MenuEntry
{
public:
    std::string alt_text;

    ToggleableMenuEntry(const std::string &txt = std::string(),
                         const std::string &alt_txt = std::string(),
                         MenuEntryLevel lev = MEL_ITEM,
                         int qty = 0, int hotk = 0) :
        MenuEntry(txt, lev, qty, hotk), alt_text(alt_txt) {}

    void toggle() { text.swap(alt_text); }
};

class MonsterMenuEntry : public MenuEntry
{
public:
    MonsterMenuEntry(const std::string &str, const monster* mon, int hotkey);

#ifdef USE_TILE
    virtual bool get_tiles(std::vector<tile_def>& tileset) const;
#endif
};

#ifdef USE_TILE
class PlayerMenuEntry : public MenuEntry
{
public:
    PlayerMenuEntry(const std::string &str);

    virtual bool get_tiles(std::vector<tile_def>& tileset) const;
};
#endif

class FeatureMenuEntry : public MenuEntry
{
public:
    coord_def            pos;
    dungeon_feature_type feat;

    FeatureMenuEntry(const std::string &str, const coord_def p, int hotkey);
    FeatureMenuEntry(const std::string &str, const dungeon_feature_type f,
                     int hotkey);

#ifdef USE_TILE
    virtual bool get_tiles(std::vector<tile_def>& tileset) const;
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
    MF_NOSELECT         = 0x0000,   // No selection is permitted
    MF_SINGLESELECT     = 0x0001,   // Select just one item
    MF_MULTISELECT      = 0x0002,   // Select multiple items
    MF_NO_SELECT_QTY    = 0x0004,   // Disallow partial selections
    MF_ANYPRINTABLE     = 0x0008,   // Any printable character is valid, and
                                    // closes the menu.
    MF_SELECT_BY_PAGE   = 0x0010,   // Allow selections to occur only on
                                    // currently visible page.

    MF_ALWAYS_SHOW_MORE = 0x0020,   // Always show the -more- footer
    MF_NOWRAP           = 0x0040,   // Paging past the end will not wrap back.

    MF_ALLOW_FILTER     = 0x0080,   // Control-F will ask for regex and
                                    // select the appropriate items.
    MF_ALLOW_FORMATTING = 0x0100,   // Parse index for formatted-string
    MF_SHOW_PAGENUMBERS = 0x0200,   // Show "(page X of Y)" when appropriate
    MF_EASY_EXIT        = 0x1000,
    MF_START_AT_END     = 0x2000,
};

class MenuDisplay
{
public:
    MenuDisplay(Menu *menu);
    virtual ~MenuDisplay() {}
    virtual void draw_stock_item(int index, const MenuEntry *me) = 0;
    virtual void set_offset(int lines) = 0;
    virtual void draw_more() = 0;
    virtual void set_num_columns(int columns) = 0;
protected:
    Menu *m_menu;
};

class MenuDisplayText : public MenuDisplay
{
public:
    MenuDisplayText(Menu *menu);
    virtual void draw_stock_item(int index, const MenuEntry *me);
    virtual void draw_more();
    virtual void set_offset(int lines) { m_starty = lines; }
    virtual void set_num_columns(int columns) {}
protected:
    int m_starty;
};

class MenuDisplayTile : public MenuDisplay
{
public:
    MenuDisplayTile(Menu *menu);
    virtual void draw_stock_item(int index, const MenuEntry *me);
    virtual void set_offset(int lines);
    virtual void draw_more();
    virtual void set_num_columns(int columns);
};

///////////////////////////////////////////////////////////////////////
// NOTE
// As a general contract, any pointers you pass to Menu methods are OWNED BY
// THE MENU, and will be deleted by the Menu on destruction. So any pointers
// you pass in MUST be allocated with new, or Crawl will crash.

#define NUMBUFSIZ 10

// FIXME: MenuEntry is a large object, and shouldn't be used for
// showing text files.

class Menu
{
public:
    Menu(int flags = MF_MULTISELECT, const std::string& tagname = "",
         bool text_only = true);

    // Initialises a Menu from a formatted_string as follows:
    //
    // 1) Splits the formatted_string on EOL.
    // 2) Picks the most recently used non-whitespace colour as the colour
    //    for the next line (so it can't do multiple colours on one line).
    // 3) Ignores all cursor movement ops in the formatted_string.
    //
    // These are limitations that should be fixed eventually.
    //
    Menu(const formatted_string &fs);

    virtual ~Menu();

    // Remove all items from the Menu, leave title intact.
    void clear();

    // Sets menu flags to new_flags. If use_options is true, game options may
    // override options.
    void set_flags(int new_flags, bool use_options = true);
    int  get_flags() const        { return flags; }
    virtual bool is_set(int flag) const;
    void set_tag(const std::string& t) { tag = t; }

    bool draw_title_suffix(const std::string &s, bool titlefirst = true);
    bool draw_title_suffix(const formatted_string &fs, bool titlefirst = true);
    void update_title();

    // Sets a replacement for the --more-- string.
    void set_more(const formatted_string &more);
    const formatted_string &get_more() const { return more; }

    void set_highlighter(MenuHighlighter *h);
    void set_title(MenuEntry *e, bool first = true);
    void add_entry(MenuEntry *entry);
    void get_selected(std::vector<MenuEntry*> *sel) const;
    virtual int get_cursor() const;

    void set_maxpagesize(int max);
    int maxpagesize() const { return max_pagesize; }

    void set_select_filter(std::vector<text_pattern> filter)
    {
        select_filter = filter;
    }

    unsigned char getkey() const { return lastch; }

    void reset();
    std::vector<MenuEntry *> show(bool reuse_selections = false);
    std::vector<MenuEntry *> selected_entries() const;

    size_t item_count() const    { return items.size(); }

    // Get entry index, skipping quantity 0 entries. Returns -1 if not found.
    int get_entry_index(const MenuEntry *e) const;

    virtual int item_colour(int index, const MenuEntry *me) const;
    int get_y_offset() const { return y_offset; }
    int get_pagesize() const { return pagesize; }
public:
    typedef std::string (*selitem_tfn)(const std::vector<MenuEntry*> *sel);
    typedef void (*drawitem_tfn)(int index, const MenuEntry *me);
    typedef int (*keyfilter_tfn)(int keyin);

    selitem_tfn      f_selitem;
    drawitem_tfn     f_drawitem;
    keyfilter_tfn    f_keyfilter;

    enum cycle  { CYCLE_NONE, CYCLE_TOGGLE, CYCLE_CYCLE } action_cycle;
    enum action { ACT_EXECUTE, ACT_EXAMINE, ACT_MISC, ACT_NUM } menu_action;

protected:
    MenuEntry *title;
    MenuEntry *title2;

    int flags;
    std::string tag;

    int first_entry, y_offset;
    int pagesize, max_pagesize;

    formatted_string more;

    std::vector<MenuEntry*>  items;
    std::vector<MenuEntry*>  sel;
    std::vector<text_pattern> select_filter;

    // Class that is queried to colour menu entries.
    MenuHighlighter *highlighter;

    int num;

    unsigned char lastch;

    bool alive;

    int last_selected;

    MenuDisplay *mdisplay;

protected:
    void check_add_formatted_line(int firstcol, int nextcol,
                                  std::string &line, bool check_eol);
    void do_menu();
    virtual void draw_select_count(int count, bool force = false);
    virtual void draw_item(int index) const;
    virtual void draw_index_item(int index, const MenuEntry *me) const;
    virtual void draw_stock_item(int index, const MenuEntry *me) const;

    virtual void draw_title();
    virtual void write_title();
    virtual void draw_menu();
    virtual bool page_down();
    virtual bool line_down();
    virtual bool page_up();
    virtual bool line_up();

    virtual int pre_process(int key);
    virtual int post_process(int key);

    bool in_page(int index) const;

    void deselect_all(bool update_view = true);
    virtual void select_items(int key, int qty = -1);
    void select_item_index(int idx, int qty, bool draw_cursor = true);
    void select_index(int index, int qty = -1);

    bool is_hotkey(int index, int key);
    virtual bool is_selectable(int index) const;

    virtual bool process_key(int keyin);
};

// Allows toggling by specific keys.
class ToggleableMenu : public Menu
{
public:
    ToggleableMenu(int _flags = MF_MULTISELECT, bool text_only = true)
        : Menu(_flags, "", text_only) {}
    void add_toggle_key(int newkey) { toggle_keys.push_back(newkey); }
protected:
    virtual int pre_process(int key);

    std::vector<int> toggle_keys;
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
            const std::string &tagged_text,
            bool  add_separator = true,
            bool  eol_ends_format = true,
            bool (*text_filter)(const std::string &tag) = NULL,
            int   margin = -1);

    std::vector<formatted_string> formatted_lines() const;

    void set_pagesize(int pagesize);

private:
    struct column;
    void compose_formatted_column(
            const std::vector<formatted_string> &lines,
            int start_col,
            int margin);
    void strip_blank_lines(std::vector<formatted_string> &) const;

private:
    struct column
    {
        int margin;
        int lines;

        column(int marg = 1) : margin(marg), lines(0) { }
    };

    int ncols, pagesize;
    std::vector<column> columns;
    std::vector<formatted_string> flines;
};

// This class is for when (some of) your items are formatted, and
// you want mostly a browser.
//
// If MF_NOSELECT, hotkeys jump to menu items.
// If MF_SINGLESELECT, hotkeys end the menu immediately.
// MF_MULTISELECT is not supported.
class formatted_scroller : public Menu
{
public:
    formatted_scroller();
    formatted_scroller(int flags, const std::string& s);
    virtual void add_item_formatted_string(const formatted_string& s,
                                           int hotkey = 0);
    virtual void add_item_string(const std::string& s, int hotkey = 0);
    virtual void add_text(const std::string& s, bool new_line = false);
    virtual bool jump_to_hotkey(int keyin);
    virtual ~formatted_scroller();
protected:
    virtual bool page_down();
    virtual bool line_down();
    virtual bool page_up();
    virtual bool line_up();

    virtual void draw_index_item(int index, const MenuEntry* me) const;
    virtual bool process_key(int keyin);
    bool jump_to(int linenum);
};

/**
 * Written by Janne "felirx" Lahdenpera
 * Abstract base class interface for all menu items to inherit from
 * each item should know how it's rendered.
 * rendering should only check the item bounds and screen bounds to prevent
 * assertion errors
 */
class MenuItem
{
public:
    MenuItem();
    virtual ~MenuItem();

    void set_id(int id) { m_item_id = id; }
    int get_id() const { return m_item_id; }

    virtual void set_bounds(const coord_def& min_coord, const coord_def& max_coord);
    virtual void set_bounds_no_multiply(const coord_def& min_coord,
                                         const coord_def& max_coord);
    virtual const coord_def& get_min_coord() const { return m_min_coord; }
    virtual const coord_def& get_max_coord() const { return m_max_coord; }

    virtual void set_description_text(const std::string& text) { m_description = text; }
    virtual const std::string& get_description_text() const { return m_description; }

    virtual void select(bool toggle);
    virtual void select(bool toggle, int value);
    virtual bool selected() const;
    virtual void allow_highlight(bool toggle);
    virtual bool can_be_highlighted() const;
    virtual void set_highlight_colour(COLORS colour);
    virtual COLORS get_highlight_colour() const;
    virtual void set_fg_colour(COLORS colour);
    virtual void set_bg_colour(COLORS colour);
    virtual COLORS get_fg_colour() const;
    virtual COLORS get_bg_colour() const;

    virtual void set_visible(bool flag);
    virtual bool is_visible() const;

    virtual void render() = 0;

    void add_hotkey(int key);
    const std::vector<int>& get_hotkeys() const;
protected:
    coord_def m_min_coord;
    coord_def m_max_coord;
    bool m_selected;
    bool m_allow_highlight;
    bool m_dirty;
    bool m_visible;
    std::vector<int> m_hotkeys;
    std::string m_description;

    COLORS m_fg_colour;
    COLORS m_highlight_colour;
    int m_bg_colour;

#ifdef USE_TILE
    // Holds the conversion values to translate unit values to pixel values
    unsigned int m_unit_width_pixels;
    unsigned int m_unit_height_pixels;
#endif

    int m_item_id;
};

/**
 * Basic Item with std::string unformatted text that can be selected
 */
class TextItem : public MenuItem
{
public:
    TextItem();
    virtual ~TextItem();

    virtual void set_bounds(const coord_def& min_coord, const coord_def& max_coord);
    virtual void set_bounds_no_multiply(const coord_def& min_coord,
                                         const coord_def& max_coord);

    virtual void render();

    void set_text(const std::string& text);
    const std::string& get_text() const;
protected:
    void _wrap_text();

    std::string m_text;
    std::string m_render_text;

#ifdef USE_TILE
    FontBuffer m_font_buf;
#endif
};

/**
 * Behaves the same as TextItem, expect selection has been overridden to always
 * return false
 */
class NoSelectTextItem : public TextItem
{
public:
    NoSelectTextItem();
    virtual ~NoSelectTextItem();
    virtual bool selected() const;
    virtual bool can_be_highlighted() const;
};

/**
 * Holds an arbitary number of tiles, currently rendered on top of each other
 */
#ifdef USE_TILE
class TextTileItem : public TextItem
{
public:
    TextTileItem();
    virtual ~TextTileItem();

    virtual void set_bounds(const coord_def& min_coord, const coord_def& max_coord);
    virtual void render();

    virtual void add_tile(tile_def tile);

protected:
    std::vector<tile_def> m_tiles;
    FixedVector<TileBuffer, TEX_MAX> m_tile_buf;
};

/**
 * Specialization of TextTileItem that knows how to pack a player doll
 * TODO: reform _pack_doll() since it currently holds duplicate code from
 * tilereg.cc pack_doll_buf()
 */
class SaveMenuItem : public TextTileItem
{
public:
    friend class TilesFramework;
    SaveMenuItem();
    virtual ~SaveMenuItem();

    virtual void render();

    void set_doll(dolls_data doll);

protected:
    void _pack_doll();
    dolls_data m_save_doll;
};
#endif

class PrecisionMenu;

/**
 * Abstract base class interface for all attachable objects
 * Objects are generally containers that hold MenuItems, however special
 * objects are also possible, for instance MenuDescriptor, MenuButton.
 * All objects should have an unique std::string name, although the uniquitity
 * is not enforced or checked right now.
 */
class MenuObject
{
public:
    enum InputReturnValue
    {
        INPUT_NO_ACTION,          // Nothing happened
        INPUT_SELECTED,           // Something got selected
        INPUT_DESELECTED,         // Something got deselected
        INPUT_END_MENU_SUCCESS,   // Call the menu to end
        INPUT_END_MENU_ABORT,     // Call the menu to clear all selections and end
        INPUT_ACTIVE_CHANGED,     // Mouse position or keyboard event changed active
        INPUT_FOCUS_RELEASE_UP,   // Selection went out of menu from top
        INPUT_FOCUS_RELEASE_DOWN, // Selection went out of menu from down
        INPUT_FOCUS_RELEASE_LEFT, // Selection went out of menu from left
        INPUT_FOCUS_RELEASE_RIGHT,// Selection went out of menu from right
        INPUT_FOCUS_LOST,         // Eg. the user is moving his mouse somewhere else
    };

    MenuObject();
    virtual ~MenuObject();

    void init(const coord_def& min_coord, const coord_def& max_coord,
              const std::string& name);
    const coord_def& get_min_coord() const { return m_min_coord; }
    const coord_def& get_max_coord() const { return m_max_coord; }
    const std::string& get_name() const { return m_object_name; }

    virtual void allow_focus(bool toggle);
    virtual bool can_be_focused();

    virtual InputReturnValue process_input(int key) = 0;
#ifdef USE_TILE
    virtual InputReturnValue handle_mouse(const MouseEvent& me) = 0;
#endif
    virtual void render() = 0;

    virtual void set_active_item(int ID) = 0;
    virtual void set_active_item(MenuItem* item) = 0;
    virtual void activate_first_item() = 0;
    virtual void activate_last_item() = 0;

    virtual void set_visible(bool flag);
    virtual bool is_visible() const;

    virtual std::vector<MenuItem*> get_selected_items();
    virtual bool select_item(int index) = 0;
    virtual bool select_item(MenuItem* item) = 0;
    virtual MenuItem* select_item_by_hotkey(int key);
    virtual void clear_selections();
    virtual MenuItem* get_active_item() = 0;

    virtual bool attach_item(MenuItem* item) = 0;

protected:
    enum Direction{
        UP,
        DOWN,
        LEFT,
        RIGHT,
    };
    virtual void _place_items() = 0;
    virtual bool _is_mouse_in_bounds(const coord_def& pos);
    virtual MenuItem* _find_item_by_mouse_coords(const coord_def& pos);
    virtual MenuItem* _find_item_by_direction(const MenuItem* start,
                                              MenuObject::Direction dir) = 0;

    bool m_dirty;
    bool m_allow_focus;
    bool m_visible;

    coord_def m_min_coord;
    coord_def m_max_coord;
    std::string m_object_name;
    // by default, entries are held in a vector
    // if you need a different behaviour, pleare override the
    // affected methods
    std::vector<MenuItem*> m_entries;
#ifdef USE_TILE
    // Holds the conversion values to translate unit values to pixel values
    unsigned int m_unit_width_pixels;
    unsigned int m_unit_height_pixels;
#endif
};

/**
 * Container object that holds MenuItems in a 2d plane.
 * There is no internal hierarchy structure inside the menu, thus the objects
 * are freely placed within the boundaries of the container
 */
class MenuFreeform : public MenuObject
{
public:
    MenuFreeform();
    virtual ~MenuFreeform();

    virtual InputReturnValue process_input(int key);
#ifdef USE_TILE
    virtual InputReturnValue handle_mouse(const MouseEvent& me);
#endif
    virtual void render();
    virtual MenuItem* get_active_item();
    virtual void set_active_item(int ID);
    virtual void set_active_item(MenuItem* item);
    virtual void activate_first_item();
    virtual void activate_last_item();

    virtual bool select_item(int index);
    virtual bool select_item(MenuItem* item);
    virtual bool attach_item(MenuItem* item);

protected:
    virtual void _place_items();
    virtual void _set_active_item_by_index(int index);
    virtual MenuItem* _find_item_by_direction(const MenuItem* start,
                                              MenuObject::Direction dir);

    // cursor position
    MenuItem* m_active_item;
};

/**
 * Container that can hold any number of objects in a scroll list style.
 * Only certain number of items are visible at the same time.
 * Navigating the list works with ARROW_UP and ARROW_DOWN keys.
 * Eventually it should also support scrollbars.
 */
class MenuScroller : public MenuObject
{
public:
    MenuScroller();
    virtual ~MenuScroller();

    virtual InputReturnValue process_input(int key);
#ifdef USE_TILE
    virtual InputReturnValue handle_mouse(const MouseEvent& me);
#endif
    virtual void render();
    virtual MenuItem* get_active_item();
    virtual void set_active_item(int ID);
    virtual void set_active_item(MenuItem* item);
    virtual void activate_first_item();
    virtual void activate_last_item();

    virtual bool select_item(int index);
    virtual bool select_item(MenuItem* item);
    virtual bool attach_item(MenuItem* item);
protected:
    virtual void _place_items();
    virtual void _set_active_item_by_index(int index);
    virtual MenuItem* _find_item_by_direction(int start_index,
                                              MenuObject::Direction dir);
    virtual MenuItem* _find_item_by_direction(const MenuItem* start,
                                              MenuObject::Direction dir) { return NULL; }

    int m_topmost_visible;
    int m_currently_active;
    int m_items_shown;
};

/**
 * Base class for various descriptor and highlighter objects
 * these should probably be attached last to the menu to be rendered last
 */
class MenuDescriptor : public MenuObject
{
public:
    MenuDescriptor(PrecisionMenu* parent);
    virtual ~MenuDescriptor();

    void init(const coord_def& min_coord, const coord_def& max_coord,
              const std::string& name);

    virtual InputReturnValue process_input(int key);
#ifdef USE_TILE
    virtual InputReturnValue handle_mouse(const MouseEvent& me);
#endif
    virtual void render();

    // these are not used, clear them
    virtual std::vector<MenuItem*> get_selected_items();
    virtual MenuItem* get_active_item() { return NULL; }
    virtual bool attach_item(MenuItem* item) { return false; }
    virtual void set_active_item(int index) {}
    virtual void set_active_item(MenuItem* item) {}
    virtual void activate_first_item() {}
    virtual void activate_last_item() {}

    virtual bool select_item(int index) { return false; }
    virtual bool select_item(MenuItem* item) { return false;}
    virtual MenuItem* select_item_by_hotkey(int key) { return NULL; }
    virtual void clear_selections() {}

    // Do not allow focus
    virtual void allow_focus(bool toggle) {}
    virtual bool can_be_focused() { return false; }

protected:
    virtual void _place_items();
    virtual MenuItem* _find_item_by_mouse_coords(const coord_def& pos) { return NULL; }
    virtual MenuItem* _find_item_by_direction(const MenuItem* start,
                                              MenuObject::Direction dir) { return NULL; }

    // Used to pull out currently active item
    PrecisionMenu* m_parent;
    MenuItem* m_active_item;
    NoSelectTextItem m_desc_item;
};

/**
 * Class for mouse over tooltips, does nothing if USE_TILE is not defined
 * TODO: actually implement render() and _place_items()
 */
class MenuTooltip : public MenuDescriptor
{
public:
    MenuTooltip(PrecisionMenu* parent);
    virtual ~MenuTooltip();

#ifdef USE_TILE
    virtual InputReturnValue handle_mouse(const MouseEvent& me);
#endif
    virtual void render();
protected:
    virtual void _place_items();

#ifdef USE_TILE
    ShapeBuffer m_background;
    FontBuffer m_font_buf;
#endif
};

/**
 * Highlighter object
 * TILES: It will create a colored rectangle around the currently active item
 * CONSOLE: It will muck with the Item background color, setting it to highlight
 *          colour, reverting the change when active changes.
 */
class BoxMenuHighlighter : public MenuObject
{
public:
    BoxMenuHighlighter(PrecisionMenu* parent);
    virtual ~BoxMenuHighlighter();

    virtual InputReturnValue process_input(int key);
#ifdef USE_TILE
    virtual InputReturnValue handle_mouse(const MouseEvent& me);
#endif
    virtual void render();

    // these are not used, clear them
    virtual std::vector<MenuItem*> get_selected_items();
    virtual MenuItem* get_active_item() { return NULL; }
    virtual bool attach_item(MenuItem* item) { return false; }
    virtual void set_active_item(int index) {}
    virtual void set_active_item(MenuItem* item) {}
    virtual void activate_first_item() {}
    virtual void activate_last_item() {}

    virtual bool select_item(int index) { return false; }
    virtual bool select_item(MenuItem* item) { return false;}
    virtual MenuItem* select_item_by_hotkey(int key) { return NULL; }
    virtual void clear_selections() {}

    // Do not allow focus
    virtual void allow_focus(bool toggle) {}
    virtual bool can_be_focused() { return false; }

protected:
    virtual void _place_items();
    virtual MenuItem* _find_item_by_mouse_coords(const coord_def& pos) { return NULL; }
    virtual MenuItem* _find_item_by_direction(const MenuItem* start,
                                              MenuObject::Direction dir) { return NULL; }

    // Used to pull out currently active item
    PrecisionMenu* m_parent;
    MenuItem* m_active_item;

#ifdef USE_TILE
    LineBuffer m_line_buf;
#else
    COLORS m_old_bg_colour;
#endif
};

class BlackWhiteHighlighter : public BoxMenuHighlighter
{
public:
    BlackWhiteHighlighter(PrecisionMenu* parent);
    virtual ~BlackWhiteHighlighter();

    virtual void render();
protected:
    virtual void _place_items();

#ifdef USE_TILE
    // Tiles does not seem to support background colors
    ShapeBuffer m_shape_buf;
#endif
    COLORS m_old_bg_colour;
    COLORS m_old_fg_colour;
};

/**
 * Base operations for a button to work
 * TODO: implement
 */
class MenuButton : public MenuObject
{
public:

protected:

};

/**
 * Inheritable root node of a menu that holds MenuObjects.
 * It is always full screen.
 * Everything attached to it it owns, thus deleting the memory when it exits the
 * scope.
 * TODO: add multiple paging support
 */
class PrecisionMenu
{
public:
    enum SelectType
    {
        PRECISION_SINGLESELECT,
        PRECISION_MULTISELECT,
    };

    PrecisionMenu();
    virtual ~PrecisionMenu();

    virtual void set_select_type(SelectType flag);
    virtual void clear();

    virtual void draw_menu();
    virtual bool process_key(int key);
#ifdef USE_TILE
    virtual int handle_mouse(const MouseEvent& me);
#endif

    // not const on purpose
    virtual std::vector<MenuItem*> get_selected_items();

    virtual void attach_object(MenuObject* item);
    virtual MenuObject* get_object_by_name(const std::string& search);
    virtual MenuItem* get_active_item();
    virtual void set_active_object(MenuObject* object);
protected:
    // These correspond to the Arrow keys when used for browsing the menus
    enum Direction{
        UP,
        DOWN,
        LEFT,
        RIGHT,
    };
    void _clear_selections();
    MenuObject* _find_object_by_direction(const MenuObject* start, Direction dir);

    std::vector<MenuObject*> m_attached_objects;
    MenuObject* m_active_object;

    SelectType m_select_type;
};

int linebreak_string(std::string& s, int wrapcol, int maxcol);
int linebreak_string2(std::string& s, int maxcol);
std::string get_linebreak_string(const std::string& s, int maxcol);

#endif
