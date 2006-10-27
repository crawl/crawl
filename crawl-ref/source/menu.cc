/*
 *  File:       menu.cc
 *  Summary:    Menus and associated malarkey.
 *  Written by: Darshan Shaligram
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */
#include <cctype>
#include "AppHdr.h"
#include "menu.h"
#include "macro.h"
#include "view.h"
#include "initfile.h"

Menu::Menu( int _flags )
 :  f_selitem(NULL),
    f_drawitem(NULL),
    f_keyfilter(NULL),
    title(NULL),
    flags(_flags),
    first_entry(0),
    y_offset(0),
    pagesize(0),
    max_pagesize(0),
    more("-more-", true),
    items(),
    sel(),
    select_filter(),
    highlighter(new MenuHighlighter),
    num(-1),
    lastch(0),
    alive(false)
{
    set_flags(flags);
}

Menu::~Menu()
{
    for (int i = 0, count = items.size(); i < count; ++i)
        delete items[i];
    delete title;
    delete highlighter;
}

void Menu::clear()
{
    for (int i = 0, count = items.size(); i < count; ++i)
        delete items[i];
    items.clear();
}

void Menu::set_maxpagesize(int max)
{
    max_pagesize = max;
}

void Menu::set_flags(int new_flags, bool use_options)
{
    flags = new_flags;
    if (use_options)
    {
        if (Options.easy_exit_menu)
            flags |= MF_EASY_EXIT;
    }
}

void Menu::set_more(const formatted_string &fs)
{
    more = fs;
}

void Menu::set_highlighter( MenuHighlighter *mh )
{
    if (highlighter != mh)
        delete highlighter;
    highlighter = mh;
}

void Menu::set_title( MenuEntry *e )
{
    if (title != e)
        delete title;

    title = e;
    title->level = MEL_TITLE;
}

void Menu::add_entry( MenuEntry *entry )
{
    items.push_back( entry );
}

void Menu::reset()
{
    first_entry = 0;
}

std::vector<MenuEntry *> Menu::show(bool reuse_selections)
{
    if (reuse_selections)
        get_selected(&sel);
    else
        deselect_all(false);

    // Lose lines for the title + room for -more- line.
    pagesize = get_number_of_lines() - !!title - 1;
    if (max_pagesize > 0 && pagesize > max_pagesize)
        pagesize = max_pagesize;

#ifdef DOS_TERM
    char buffer[4600];

    gettext(1, 1, 80, 25, buffer);
    window(1, 1, 80, 25);
#endif

    do_menu();
    
#ifdef DOS_TERM    
    puttext(1, 1, 80, 25, buffer);
#endif

    return (sel);
}

void Menu::do_menu()
{
    draw_menu();

    alive = true;
    while (alive)
    {
        int keyin = getchm(c_getch);
        
        if (!process_key( keyin ))
            return;
    }
}

bool Menu::is_set(int flag) const
{
    return (flags & flag) == flag;
}

bool Menu::process_key( int keyin )
{
    if (items.size() == 0)
        return false;

    bool nav = false, repaint = false;

    if (f_keyfilter)
        keyin = (*f_keyfilter)(keyin);

    switch (keyin)
    {
    case 0:
        return true;
    case CK_ENTER:
        return false;
    case CK_ESCAPE:
        sel.clear();
        lastch = keyin;
        return false;
    case ' ': case CK_PGDN: case '>': case '\'':
        nav = true;
        repaint = page_down();
        if (!repaint && !is_set(MF_EASY_EXIT) && !is_set(MF_NOWRAP))
        {
            repaint = first_entry != 0;
            first_entry = 0;
        }
        break;
    case CK_PGUP: case '<': case ';':
        nav = true;
        repaint = page_up();
        break;
    case CK_UP:
        nav = true;
        repaint = line_up();
        break;
    case CK_DOWN:
        nav = true;
        repaint = line_down();
        break;
    default:
        lastch = keyin;

        // If no selection at all is allowed, exit now.
        if (!(flags & (MF_SINGLESELECT | MF_MULTISELECT)))
            return false;

        if (!is_set(MF_NO_SELECT_QTY) && isdigit( keyin ))
        {
            if (num > 999)
                num = -1;
            num = (num == -1)? keyin - '0' :
                               num * 10 + keyin - '0';
        }
        
        select_items( keyin, num );
        get_selected( &sel );
        if (sel.size() == 1 && (flags & MF_SINGLESELECT))
            return false;
        draw_select_count( sel.size() );

        if (flags & MF_ANYPRINTABLE && !isdigit( keyin ))
            return false;

        break;
    }

    if (!isdigit( keyin ))
        num = -1;

    if (nav)
    {
        if (repaint)
            draw_menu();
        // Easy exit should not kill the menu if there are selected items.
        else if (sel.empty() && is_set(MF_EASY_EXIT))
        {
            sel.clear();
            return false;
        }
    }
    return true;
}

bool Menu::draw_title_suffix( const std::string &s, bool titlefirst )
{
    int oldx = wherex(), oldy = wherey();

    if (titlefirst)
        draw_title();

    int x = wherex();
    if (x > get_number_of_cols() || x < 1)
    {
        gotoxy(oldx, oldy);
        return false;
    }

    // Note: 1 <= x <= get_number_of_cols(); we have no fear of overflow.
    unsigned avail_width = get_number_of_cols() - x + 1;
    std::string towrite = s.length() > avail_width? s.substr(0, avail_width) :
                          s.length() == avail_width? s :
                                s + std::string(avail_width - s.length(), ' ');

    cprintf(towrite.c_str());
    
    gotoxy( oldx, oldy );
    return true;
}
    
void Menu::draw_select_count( int count, bool force )
{
    if (!force && !is_set(MF_MULTISELECT))
        return;

    if (f_selitem)
    {
        draw_title_suffix( f_selitem( &sel ) );
    }
    else
    {
        char buf[100] = "";
        if (count)
            snprintf(buf, sizeof buf, "  (%d item%s)  ", count, 
                    (count > 1? "s" : ""));

        draw_title_suffix( buf );
    }
}

std::vector<MenuEntry*> Menu::selected_entries() const
{
    std::vector<MenuEntry*> selection;
    get_selected(&selection);
    return (selection);
}

void Menu::get_selected( std::vector<MenuEntry*> *selected ) const
{
    selected->clear();
    
    for (int i = 0, count = items.size(); i < count; ++i)
        if (items[i]->selected())
            selected->push_back( items[i] );
}

void Menu::deselect_all(bool update_view)
{
    for (int i = 0, count = items.size(); i < count; ++i)
    {
        if (items[i]->level == MEL_ITEM)
        {
            items[i]->select(0);
            if (update_view)
                draw_item(i);
        }
    }
}

bool Menu::is_hotkey(int i, int key)
{
    int end = first_entry + pagesize;
    if (end > (int) items.size()) end = items.size();

    bool ishotkey = is_set(MF_SINGLESELECT)?
                        items[i]->is_primary_hotkey(key)
                      : items[i]->is_hotkey(key);

    return !is_set(MF_SELECT_BY_PAGE)? ishotkey
        : ishotkey && i >= first_entry && i < end;
}

void Menu::select_items( int key, int qty )
{
    int x = wherex(), y = wherey();

    if (key == ',' || key == '*')
        select_index( -1, qty );
    else if (key == '-')
        select_index( -1, 0 );
    else
    {
        int final = items.size();
        bool selected = false;
        
        // Process all items, in case user hits hotkey for an
        // item not on the current page.

        // We have to use some hackery to handle items that share
        // the same hotkey (as for pickup when there's a stack of
        // >52 items).  If there are duplicate hotkeys, the items
        // are usually separated by at least a page, so we should
        // only select the item on the current page. This is why we
        // use two loops, and check to see if we've matched an item
        // by its primary hotkey (which is assumed to always be
        // hotkeys[0]), in which case, we stop selecting further
        // items.
        for (int i = first_entry; i < final; ++i)
        {
            if (is_hotkey( i, key ))
            {
                select_index( i, qty );
                if (items[i]->hotkeys[0] == key)
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
                if (is_hotkey( i, key ))
                {
                    select_index( i, qty );
                    if (items[i]->hotkeys[0] == key)
                    {
                        selected = true;
                        break;
                    }
                }
            }
        }
    }
    gotoxy( x, y );
}

bool Menu::is_selectable(int item) const
{
    if (select_filter.empty()) return true;
    
    std::string text = items[item]->get_text();
    for (int i = 0, count = select_filter.size(); i < count; ++i)
    {
        if (select_filter[i].matches(text))
            return true;
    }
    return false;
}

void Menu::select_index( int index, int qty )
{
    int si = index == -1? first_entry : index;

    if (index == -1)
    {
        if (flags & MF_MULTISELECT)
        {
            for (int i = 0, count = items.size(); i < count; ++i)
            {
                if (items[i]->level != MEL_ITEM) continue;
                if (is_hotkey(i, items[i]->hotkeys[0]) && is_selectable(i))
                {
                    items[i]->select( qty );
                    draw_item( i );
                }
            }
        }
    }
    else if (items[si]->level == MEL_SUBTITLE && (flags & MF_MULTISELECT))
    {
        for (int i = si + 1, count = items.size(); i < count; ++i)
        {
            if (items[i]->level != MEL_ITEM)
                break;
            if (is_hotkey(i, items[i]->hotkeys[0]))
            {
                items[i]->select( qty );
                draw_item( i );
            }
        }
    }
    else if (items[si]->level == MEL_ITEM && 
            (flags & (MF_SINGLESELECT | MF_MULTISELECT))) 
    {
        items[si]->select( qty );
        draw_item( si );
    }
}

void Menu::draw_menu()
{
    clrscr();

    draw_title();
    draw_select_count( sel.size() );
    y_offset = 1 + !!title;

    int end = first_entry + pagesize;
    if (end > (int) items.size()) end = items.size();
    
    for (int i = first_entry; i < end; ++i)
    {
        draw_item( i );
    }
    if (end < (int) items.size() || is_set(MF_ALWAYS_SHOW_MORE))
    {
        gotoxy( 1, y_offset + pagesize );
        more.display();
    }
}

void Menu::update_title()
{
    int x = wherex(), y = wherey();
    draw_title();
    gotoxy(x, y);
}

int Menu::item_colour(int, const MenuEntry *entry) const
{
    int icol = -1;
    if (highlighter)
        icol = highlighter->entry_colour(entry);

    return (icol == -1? entry->colour : icol);
}

void Menu::draw_title()
{
    if (title)
    {
        gotoxy(1, 1);
        write_title();
    }
}

void Menu::write_title()
{
    textattr( item_colour(-1, title) );
    cprintf("%s", title->get_text().c_str());

    const int x = wherex(), y = wherey();
    cprintf("%-*s", get_number_of_cols() - x, "");
    gotoxy(x, y);
}

bool Menu::in_page(int index) const
{
    return (index >= first_entry && index < first_entry + pagesize);
}

void Menu::draw_item( int index ) const
{
    if (!in_page(index))
        return;
    gotoxy( 1, y_offset + index - first_entry );

    draw_item(index, items[index]);
}

void Menu::draw_item(int index, const MenuEntry *me) const
{
    if (f_drawitem)
        (*f_drawitem)(index, me);
    else
        draw_stock_item(index, me);
}

void Menu::draw_stock_item(int index, const MenuEntry *me) const
{
    textattr( item_colour(index, items[index]) );
    cprintf( "%s", items[index]->get_text().c_str() );
}

bool Menu::page_down()
{
    int old_first = first_entry;

    if ((int) items.size() > first_entry + pagesize)
    {
        first_entry += pagesize;
        //if (first_entry + pagesize > (int) items.size())
        //    first_entry = items.size() - pagesize;

        if (old_first != first_entry)
            return true;
    }
    return false;
}

bool Menu::page_up()
{
    int old_first = first_entry;

    if (first_entry > 0)
    {
        if ((first_entry -= pagesize) < 0)
            first_entry = 0;
        if (old_first != first_entry)
            return true;
    }
    return false;
}

bool Menu::line_down()
{
    if (first_entry + pagesize < (int) items.size())
    {
        ++first_entry;
        return true;
    }
    return false;
}

bool Menu::line_up()
{
    if (first_entry > 0)
    {
        --first_entry;
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////
// slider_menu

slider_menu::slider_menu(int fl)
    : Menu(fl), less(), starty(1), endy(get_number_of_lines()),
      selected(0)
{
    less.textcolor(DARKGREY);
    less.cprintf("<---- More");
    more.clear();
    more.textcolor(DARKGREY);
    more.cprintf("More ---->");
}

void slider_menu::set_limits(int y1, int y2)
{
    starty = y1;
    endy   = y2;
}

bool slider_menu::process_key(int key)
{
    // Some of this key processing should really be in a user-passed-in function
    // If we ever need to use slider_menu elsewhere, we should factor it out.
    if (key == CK_ESCAPE || key == '\t')
    {
        int old = selected;
        selected = -1;
        draw_item(old);
        sel.clear();
        lastch = key;
        return (false);
    }

    if (selected == 0 && 
            (key == CK_UP || key == CK_PGUP || key == '<' || key == ';') &&
            Menu::is_set(MF_EASY_EXIT))
    {
        int old = selected;
        selected = -1;
        draw_item(old);
        return (false);
    }

    return Menu::process_key(key);
}

void slider_menu::display()
{
    // We lose two lines for each of the --More prompts
    pagesize = endy - starty - 1 - !!title;
    selected = -1;
    draw_menu();
}

std::vector<MenuEntry *> slider_menu::show()
{
    cursor_control coff(false);

    sel.clear();

    // We lose two lines for each of the --More prompts
    pagesize = endy - starty - 1 - !!title;

    if (selected == -1)
        selected = 0;

    do_menu();

    if (selected >= 0 && selected <= (int) items.size())
        sel.push_back(items[selected]);
    return (sel);
}

const MenuEntry *slider_menu::selected_entry() const
{
    if (selected >= 0 && selected <= (int) items.size())
        return (items[selected]);

    return (NULL);
}

void slider_menu::draw_stock_item(int index, const MenuEntry *me) const
{
    Menu::draw_stock_item(index, me);
    cprintf("%-*s", get_number_of_cols() - wherex() + 1, "");
}

int slider_menu::item_colour(int index, const MenuEntry *me) const
{
    int colour = Menu::item_colour(index, me);
    if (index == selected && selected != -1)
    {
#if defined(WIN32CONSOLE) || defined(DOS)
        colour = dos_brand(colour, CHATTR_REVERSE);
#else
        colour |= COLFLAG_REVERSE;
#endif
    }
    return (colour);
}

void slider_menu::draw_menu()
{
    gotoxy(1, starty);
    write_title();
    y_offset = starty + !!title + 1;

    int end = first_entry + pagesize;
    if (end > (int) items.size()) end = items.size();

    // We're using get_number_of_cols() - 1 because we want to avoid line wrap
    // on DOS (the conio.h functions go batshit if that happens).
    gotoxy(1, y_offset - 1);
    if (first_entry > 0)
        less.display();
    else
    {
        textattr(LIGHTGREY);
        cprintf("%-*s", get_number_of_cols() - 1, "");
    }
    
    for (int i = first_entry; i < end; ++i)
        draw_item( i );

    textattr(LIGHTGREY);
    for (int i = end; i < first_entry + pagesize; ++i)
    {
        gotoxy(1, y_offset + i - first_entry);
        cprintf("%-*s", get_number_of_cols() - 1, "");
    }   

    gotoxy( 1, y_offset + pagesize );
    if (end < (int) items.size() || is_set(MF_ALWAYS_SHOW_MORE))
        more.display();
    else
    {
        textattr(LIGHTGREY);
        cprintf("%-*s", get_number_of_cols() - 1, "");
    }
}

void slider_menu::select_items(int, int)
{
    // Ignored.
}

bool slider_menu::is_set(int flag) const
{
    if (flag == MF_EASY_EXIT)
        return (false);
    return Menu::is_set(flag);
}

bool slider_menu::fix_entry()
{
    if (selected < 0 || selected >= (int) items.size())
        return (false);

    const int oldfirst = first_entry;
    if (selected < first_entry)
        first_entry = selected;
    else if (selected >= first_entry + pagesize)
    {
        first_entry = selected - pagesize + 1;
        if (first_entry < 0)
            first_entry = 0;
    }

    return (first_entry != oldfirst);
}

void slider_menu::new_selection(int nsel)
{
    if (nsel < 0)
        nsel = 0;
    if (nsel >= (int) items.size())
        nsel = items.size() - 1;

    const int old = selected;
    selected = nsel;
    if (old != selected && nsel >= first_entry && nsel < first_entry + pagesize)
    {
        draw_item(old);
        draw_item(selected);
    }
}

bool slider_menu::page_down()
{
    new_selection( selected + pagesize );
    return fix_entry();
}

bool slider_menu::page_up()
{
    new_selection( selected - pagesize );
    return fix_entry();
}

bool slider_menu::line_down()
{
    new_selection( selected + 1 );
    return fix_entry();
}

bool slider_menu::line_up()
{
    new_selection( selected - 1 );
    return fix_entry();
}

/////////////////////////////////////////////////////////////////
// Menu colouring
//

int menu_colour(const std::string &text)
{
    for (int i = 0, size = Options.menu_colour_mappings.size(); i < size; ++i)
    {
        colour_mapping &cm = Options.menu_colour_mappings[i];
        if (cm.pattern.matches(text))
            return (cm.colour);
    }
    return (-1);
}

int MenuHighlighter::entry_colour(const MenuEntry *entry) const
{
    return (::menu_colour(entry->get_text()));
}


////////////////////////////////////////////////////////////////////
// formatted_string
//

formatted_string::formatted_string(const std::string &s, bool init_style)
    : ops()
{
    if (init_style)
        ops.push_back(LIGHTGREY);
    ops.push_back(s);
}

int formatted_string::get_colour(const std::string &tag)
{
    if (tag == "h")
        return (YELLOW);

    if (tag == "w")
        return (WHITE);

    const int colour = str_to_colour(tag);
    return (colour != -1? colour : LIGHTGREY);
}

formatted_string formatted_string::parse_string(
        const std::string &s,
        bool  eol_ends_format,
        bool (*process)(const std::string &tag))
{
    // FIXME This is a lame mess, just good enough for the task on hand
    // (keyboard help).
    formatted_string fs;
    std::string::size_type tag    = std::string::npos;
    std::string::size_type length = s.length();

    std::string currs;
    int curr_colour = LIGHTGREY;
    bool masked = false;
    
    for (tag = 0; tag < length; ++tag)
    {
        bool invert_colour = false;
        std::string::size_type endpos = std::string::npos;

        if (s[tag] != '<' || tag >= length - 2)
        {
            if (!masked)
                currs += s[tag];
            continue;
        }

        // Is this a << escape?
        if (s[tag + 1] == '<')
        {
            if (!masked)
                currs += s[tag];
            tag++;
            continue;
        }

        endpos = s.find('>', tag + 1);
        // No closing >?
        if (endpos == std::string::npos)
        {
            if (!masked)
                currs += s[tag];
            continue;
        }

        std::string tagtext = s.substr(tag + 1, endpos - tag - 1);
        if (tagtext.empty() || tagtext == "/")
        {
            if (!masked)
                currs += s[tag];
            continue;
        }

        if (tagtext[0] == '/')
        {
            invert_colour = true;
            tagtext = tagtext.substr(1);
            tag++;
        }

        if (tagtext[0] == '?')
        {
            if (tagtext.length() == 1)
                masked = false;
            else if (process && !process(tagtext.substr(1)))
                masked = true;

            tag += tagtext.length() + 1; 
            continue;
        }

        const int new_colour = invert_colour? LIGHTGREY : get_colour(tagtext);
        if (new_colour != curr_colour)
        {
            fs.cprintf(currs);
            currs.clear();
            fs.textcolor( curr_colour = new_colour );
        }
        tag += tagtext.length() + 1;
    }
    if (currs.length())
        fs.cprintf(currs);

    if (eol_ends_format && curr_colour != LIGHTGREY)
        fs.textcolor(LIGHTGREY);

    return (fs);
}

formatted_string::operator std::string() const
{
    std::string s;
    for (unsigned i = 0, size = ops.size(); i < size; ++i)
    {
        if (ops[i] == FSOP_TEXT)
            s += ops[i].text;
    }
    return (s);
}

const formatted_string &
formatted_string::operator += (const formatted_string &other)
{
    ops.insert(ops.end(), other.ops.begin(), other.ops.end());
    return (*this);
}

std::string::size_type formatted_string::length() const
{
    // Just add up the individual string lengths.
    std::string::size_type len = 0;
    for (unsigned i = 0, size = ops.size(); i < size; ++i)
        if (ops[i] == FSOP_TEXT)
            len += ops[i].text.length();
    return (len);
}

inline void cap(int &i, int max)
{
    if (i < 0 && -i <= max)
        i += max;
    if (i >= max)
        i = max - 1;
    if (i < 0)
        i = 0;
}

std::string formatted_string::tostring(int s, int e) const
{
    std::string st;
    
    int size = ops.size();
    cap(s, size);    
    cap(e, size);
    
    for (int i = s; i <= e && i < size; ++i)
    {
        if (ops[i] == FSOP_TEXT)
            st += ops[i].text;
    }
    return st;
}

void formatted_string::display(int s, int e) const
{
    int size = ops.size();
    if (!size)
        return ;

    cap(s, size);    
    cap(e, size);
    
    for (int i = s; i <= e && i < size; ++i)
        ops[i].display();
}

void formatted_string::gotoxy(int x, int y)
{
    ops.push_back( fs_op(x, y) );
}

void formatted_string::movexy(int x, int y)
{
    ops.push_back( fs_op(x, y, true) );
}

void formatted_string::textcolor(int color)
{
    ops.push_back(color);
}

void formatted_string::clear()
{
    ops.clear();
}

void formatted_string::cprintf(const char *s, ...)
{
    char buf[1000];
    va_list args;
    va_start(args, s);
    vsnprintf(buf, sizeof buf, s, args);
    va_end(args);

    cprintf(std::string(buf));
}

void formatted_string::cprintf(const std::string &s)
{
    ops.push_back(s);
}

void formatted_string::fs_op::display() const
{
    switch (type)
    {
    case FSOP_CURSOR:
    {
        int cx = x, cy = y;
        if (relative)
        {
            cx += wherex();
            cy += wherey();
        }
        else
        {
            if (cx == -1)
                cx = wherex();
            if (cy == -1)
                cy = wherey();
        }
        ::gotoxy(cx, cy);
        break;
    }
    case FSOP_COLOUR:
        ::textattr(x);
        break;
    case FSOP_TEXT:
        ::cprintf("%s", text.c_str());
        break;
    }
}

///////////////////////////////////////////////////////////////////////
// column_composer

column_composer::column_composer(int cols, ...)
    : ncols(cols), pagesize(0), columns()
{
    ASSERT(cols > 0);

    va_list args;
    va_start(args, cols);

    columns.push_back( column(1) );
    int lastcol = 1;
    for (int i = 1; i < cols; ++i)
    {
        int nextcol = va_arg(args, int);
        ASSERT(nextcol > lastcol);

        lastcol = nextcol;
        columns.push_back( column(nextcol) );
    }

    va_end(args);
}

void column_composer::set_pagesize(int ps)
{
    pagesize = ps;
}

void column_composer::clear()
{
    flines.clear();
}

void column_composer::add_formatted(
        int ncol,
        const std::string &s,
        bool add_separator,
        bool eol_ends_format,
        bool (*tfilt)(const std::string &),
        int  margin)
{
    ASSERT(ncol >= 0 && ncol < (int) columns.size());

    column &col = columns[ncol];
    std::vector<std::string> segs = split_string("\n", s, false, true);

    std::vector<formatted_string> newlines;
    // Add a blank line if necessary. Blank lines will not
    // be added at page boundaries.
    if (add_separator && col.lines && !segs.empty()
            && (!pagesize || col.lines % pagesize))
        newlines.push_back(formatted_string());

    for (unsigned i = 0, size = segs.size(); i < size; ++i)
    {
        newlines.push_back(
                formatted_string::parse_string(
                    segs[i],
                    eol_ends_format,
                    tfilt));
    }

    strip_blank_lines(newlines);

    compose_formatted_column(
            newlines, 
            col.lines, 
            margin == -1? col.margin : margin);

    col.lines += newlines.size();

    strip_blank_lines(flines);
}

std::vector<formatted_string> column_composer::formatted_lines() const
{
    return (flines);
}

void column_composer::strip_blank_lines(std::vector<formatted_string> &fs) const
{
    for (int i = fs.size() - 1; i >= 0; --i)
    {
        if (fs[i].length() == 0)
            fs.erase( fs.begin() + i );
        else
            break;
    }
}

void column_composer::compose_formatted_column(
        const std::vector<formatted_string> &lines,
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
            int xdelta = margin - flines[f].length() - 1;
            if (xdelta > 0)
                flines[f].cprintf("%-*s", xdelta, "");
        }
        flines[f] += lines[i];
    }
}
