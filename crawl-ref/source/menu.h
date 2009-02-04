/*
 *  File:       menu.h
 *  Summary:    Menus and associated malarkey.
 *  Written by: Darshan Shaligram
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef __MENU_H__
#define __MENU_H__

#include <string>
#include <vector>
#include <algorithm>
#include <time.h>
#include "AppHdr.h"
#include "externs.h"
#include "format.h"
#include "defines.h"
#include "libutil.h"

class formatted_string;

enum MenuEntryLevel
{
    MEL_NONE = -1,
    MEL_TITLE,
    MEL_SUBTITLE,
    MEL_ITEM
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

struct MenuEntry
{
    std::string tag;
    std::string text;
    int quantity, selected_qty;
    int colour;
    std::vector<int> hotkeys;
    MenuEntryLevel level;
    void *data;

    MenuEntry( const std::string &txt = std::string(),
               MenuEntryLevel lev = MEL_ITEM,
               int qty  = 0,
               int hotk = 0 ) :
        text(txt), quantity(qty), selected_qty(0), colour(-1),
        hotkeys(), level(lev), data(NULL)
    {
        colour = (lev == MEL_ITEM     ?  LIGHTGREY :
                  lev == MEL_SUBTITLE ?  BLUE  :
                                         WHITE);
        if (hotk)
            hotkeys.push_back( hotk );
    }
    virtual ~MenuEntry() { }

    bool operator<( const MenuEntry& rhs ) const
    {
        return text < rhs.text;
    }

    void add_hotkey( int key )
    {
        if (key && !is_hotkey(key))
            hotkeys.push_back( key );
    }

    bool is_hotkey( int key ) const
    {
        return find( hotkeys.begin(), hotkeys.end(), key ) != hotkeys.end();
    }

    bool is_primary_hotkey( int key ) const
    {
        return (hotkeys.size()? hotkeys[0] == key : false);
    }

    virtual std::string get_text() const
    {
        if (level == MEL_ITEM && hotkeys.size())
        {
            char buf[300];
            snprintf(buf, sizeof buf,
                    "%c - %s", hotkeys[0], text.c_str());
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

    virtual void select( int qty = -1 )
    {
        if (selected())
            selected_qty = 0;
        else if (quantity)
            selected_qty = (qty == -1? quantity : qty);
    }

    virtual std::string get_filter_text() const
    {
        return get_text();
    }

#ifdef USE_TILE
    virtual bool get_tiles(std::vector<tile_def>& tileset) const
    {
        return (false);
    }
#endif
};

struct ToggleableMenuEntry : public MenuEntry
{
public:
    std::string alt_text;

    ToggleableMenuEntry( const std::string &txt = std::string(),
                         const std::string &alt_txt = std::string(),
                         MenuEntryLevel lev = MEL_ITEM,
                         int qty = 0, int hotk = 0 ) :
        MenuEntry(txt, lev, qty, hotk), alt_text(alt_txt) {}

    void toggle() { text.swap(alt_text); }
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
    MF_EASY_EXIT        = 0x1000
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

    // Initializes a Menu from a formatted_string as follows:
    //
    // 1) Splits the formatted_string on EOL (this is not necessarily \n).
    // 2) Picks the most recently used non-whitespace colour as the colour
    //    for the next line (so it can't do multiple colours on one line).
    // 3) Ignores all cursor movement ops in the formatted_string.
    //
    // These are limitations that should be fixed eventually.
    //
    Menu( const formatted_string &fs );

    virtual ~Menu();

    // Remove all items from the Menu, leave title intact.
    void clear();

    // Sets menu flags to new_flags. If use_options is true, game options may
    // override options.
    void set_flags(int new_flags, bool use_options = true);
    int  get_flags() const        { return flags; }
    virtual bool is_set( int flag ) const;
    void set_tag(const std::string& t) { tag = t; }

    bool draw_title_suffix( const std::string &s, bool titlefirst = true );
    bool draw_title_suffix( const formatted_string &fs, bool titlefirst = true );
    void update_title();

    // Sets a replacement for the --more-- string.
    void set_more(const formatted_string &more);
    const formatted_string &get_more() const { return more; }

    void set_highlighter( MenuHighlighter *h );
    void set_title( MenuEntry *e );
    void add_entry( MenuEntry *entry );
    void get_selected( std::vector<MenuEntry*> *sel ) const;

    void set_maxpagesize(int max);
    int maxpagesize() const { return max_pagesize; }

    void set_select_filter( std::vector<text_pattern> filter )
    {
        select_filter = filter;
    }

    unsigned char getkey() const { return lastch; }

    void reset();
    std::vector<MenuEntry *> show(bool reuse_selections = false);
    std::vector<MenuEntry *> selected_entries() const;

    size_t item_count() const    { return items.size(); }

    // Get entry index, skipping quantity 0 entries. Returns -1 if not found.
    int get_entry_index( const MenuEntry *e ) const;

    virtual int item_colour(int index, const MenuEntry *me) const;
    int get_y_offset() const { return y_offset; }
    int get_pagesize() const { return pagesize; }
public:
    typedef std::string (*selitem_tfn)( const std::vector<MenuEntry*> *sel );
    typedef void (*drawitem_tfn)(int index, const MenuEntry *me);
    typedef int (*keyfilter_tfn)(int keyin);

    selitem_tfn      f_selitem;
    drawitem_tfn     f_drawitem;
    keyfilter_tfn    f_keyfilter;

protected:
    MenuEntry *title;
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
    virtual void draw_item( int index ) const;
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
    virtual void select_items( int key, int qty = -1 );
    void select_index( int index, int qty = -1 );

    bool is_hotkey(int index, int key );
    virtual bool is_selectable(int index) const;

    virtual bool process_key( int keyin );
};

// Allows toggling by specific keys.
class ToggleableMenu : public Menu
{
public:
    ToggleableMenu( int _flags = MF_MULTISELECT ) : Menu(_flags) {}
    void add_toggle_key(int newkey) { toggle_keys.push_back(newkey); }
protected:
    virtual int pre_process(int key);

    std::vector<int> toggle_keys;
};


// Uses a sliding selector rather than hotkeyed selection.
class slider_menu : public Menu
{
public:
    // Multiselect would be awkward to implement.
    slider_menu(int flags = MF_SINGLESELECT | MF_NOWRAP);
    void display();
    std::vector<MenuEntry *> show();

    void set_search(const std::string &search);
    void set_limits(int starty, int endy);
    const MenuEntry *selected_entry() const;

protected:
    int item_colour(int index, const MenuEntry *me) const;
    void draw_stock_item(int index, const MenuEntry *me) const;
    void draw_menu();
    void show_less();
    void show_more();
    void calc_y_offset();
    void adjust_pagesizes(int rdepth = 3);
    int  entry_end() const;
    void fill_line() const;

    void new_selection(int nsel);
    bool move_selection(int nsel);

    bool page_down();
    bool line_down();
    bool page_up();
    bool line_up();

    bool is_set(int flag) const;
    void select_items( int key, int qty );
    bool fix_entry(int rdepth = 3);
    bool process_key( int keyin );

    int post_process(int key);

    void select_search(const std::string &search);

protected:
    formatted_string less;
    int starty, endy;
    int selected;
    bool need_less, need_more;
    int oldselect;
    time_t lastkey;
    std::string search;
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
    virtual void add_text(const std::string& s);
    virtual bool jump_to_hotkey( int keyin );
    virtual ~formatted_scroller();
protected:
    virtual bool page_down();
    virtual bool line_down();
    virtual bool page_up();
    virtual bool line_up();

    virtual void draw_index_item(int index, const MenuEntry* me) const;
    virtual bool process_key( int keyin );
    bool jump_to( int linenum );
};

int linebreak_string( std::string& s, int wrapcol, int maxcol );
int linebreak_string2( std::string& s, int maxcol );
void print_formatted_paragraph(std::string &s,
                               msg_channel_type channel = MSGCH_PLAIN);
std::string get_linebreak_string(const std::string& s, int maxcol);

#endif
