#ifndef __MENU_H__
#define __MENU_H__

#include <string>
#include <vector>
#include <algorithm>
#include "AppHdr.h"
#include "externs.h"
#include "defines.h"
#include "libutil.h"

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

class formatted_string
{
public:
    formatted_string() : ops() { }
    formatted_string(const std::string &s, bool init_style = false);

    operator std::string() const;
    void display(int start = 0, int end = -1) const;
    std::string tostring(int start = 0, int end = -1) const;

    void cprintf(const char *s, ...);
    void cprintf(const std::string &s);
    void gotoxy(int x, int y);
    void movexy(int delta_x, int delta_y);
    void textcolor(int color);

    std::string::size_type length() const;

    const formatted_string &operator += (const formatted_string &other);

public:
    static formatted_string parse_string(
            const std::string &s,
            bool  eol_ends_format = true,
            bool (*process_tag)(const std::string &tag) = NULL );

private:
    enum fs_op_type
    {
        FSOP_COLOUR,
        FSOP_CURSOR,
        FSOP_TEXT
    };

    static int get_colour(const std::string &tag);

private:
    struct fs_op
    {
        fs_op_type type;
        int x, y;
        bool relative;
        std::string text;
        
        fs_op(int color)
            : type(FSOP_COLOUR), x(color), y(-1), relative(false), text()
        {
        }
        
        fs_op(int cx, int cy, bool rel = false)
            : type(FSOP_CURSOR), x(cx), y(cy), relative(rel), text()
        {
        }
        
        fs_op(const std::string &s)
            : type(FSOP_TEXT), x(-1), y(-1), relative(false), text(s)
        {
        }

        operator fs_op_type () const
        {
            return type;
        }
        void display() const;
    };

    std::vector<fs_op> ops;
};

struct item_def;
struct MenuEntry
{
    std::string text;
    int quantity, selected_qty;
    int colour;
    std::vector<int> hotkeys;
    MenuEntryLevel level;
    void *data;

    MenuEntry( const std::string &txt = std::string(),
               MenuEntryLevel lev = MEL_ITEM,
               int qty = 0,
               int hotk = 0 ) :
        text(txt), quantity(qty), selected_qty(0), colour(-1),
        hotkeys(), level(lev), data(NULL)
    {
        colour = lev == MEL_ITEM?       LIGHTGREY :
                 lev == MEL_SUBTITLE?   BLUE  :
                                        WHITE;
        if (hotk)
            hotkeys.push_back( hotk );
    }
    virtual ~MenuEntry() { }

    bool operator<( const MenuEntry& rhs ) const {
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
        return hotkeys.size()? hotkeys[0] == key : false;
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
        return std::string(level == MEL_SUBTITLE? " " : "") + text;
    }

    virtual bool selected() const
    {
        return selected_qty > 0 && quantity;
    }

    void select( int qty = -1 )
    {
        if (selected())
            selected_qty = 0;
        else if (quantity)
            selected_qty = qty == -1? quantity : qty;
    }
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

    MF_EASY_EXIT        = 0x1000
};

///////////////////////////////////////////////////////////////////////
// NOTE
// As a general contract, any pointers you pass to Menu methods are OWNED BY
// THE MENU, and will be deleted by the Menu on destruction. So any pointers
// you pass in MUST be allocated with new, or Crawl will crash.

#define NUMBUFSIZ 10
class Menu
{
public:
    Menu( int flags = MF_MULTISELECT );
    virtual ~Menu();

    // Remove all items from the Menu, leave title intact.
    void clear();

    // Sets menu flags to new_flags. If use_options is true, game options may
    // override options.
    void set_flags(int new_flags, bool use_options = true);
    int  get_flags() const        { return flags; }
    bool is_set( int flag ) const;
    
    bool draw_title_suffix( const std::string &s, bool titlefirst = true );
    void update_title();

    // Sets a replacement for the --more-- string.
    void set_more(const formatted_string &more);
    
    void set_highlighter( MenuHighlighter *h );
    void set_title( MenuEntry *e );
    void add_entry( MenuEntry *entry );
    void get_selected( std::vector<MenuEntry*> *sel ) const;

    void set_maxpagesize(int max);

    void set_select_filter( std::vector<text_pattern> filter )
    {
        select_filter = filter;
    }

    unsigned char getkey() const { return lastch; }

    void reset();
    std::vector<MenuEntry *> show(bool reuse_selections = false);
    std::vector<MenuEntry *> selected_entries() const;

    size_t item_count() const    { return items.size(); }

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

protected:
    void do_menu();
    virtual void draw_select_count(int count, bool force = false);
    virtual void draw_item( int index ) const;
    virtual void draw_item(int index, const MenuEntry *me) const;

    virtual void draw_title();
    void draw_menu();
    bool page_down();
    bool line_down();
    bool page_up();
    bool line_up();

    bool in_page(int index) const;

    void deselect_all(bool update_view = true);
    void select_items( int key, int qty = -1 );
    void select_index( int index, int qty = -1 );

    bool is_hotkey(int index, int key );
    bool is_selectable(int index) const;

    int item_colour(const MenuEntry *me) const;
    
    virtual bool process_key( int keyin );
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

int menu_colour(const std::string &itemtext);

#endif
