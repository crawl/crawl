/*
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include <cctype>
#include "menu.h"
#include "macro.h"
#include "view.h"

Menu::Menu( int _flags )
 :  f_selitem(NULL),
    f_drawitem(NULL),
    f_keyfilter(NULL),
    title(NULL),
    flags(_flags),
    first_entry(0),
    y_offset(0),
    pagesize(0),
    more("-more-", true),
    items(),
    sel(NULL),
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

std::vector<MenuEntry *> Menu::show()
{
    std::vector< MenuEntry * > selected;

    deselect_all(false);

    // Lose lines for the title + room for more.
    pagesize = get_number_of_lines() - !!title - 1;

#ifdef DOS_TERM
    char buffer[4600];

    gettext(1, 1, 80, 25, buffer);
    window(1, 1, 80, 25);
#endif

    do_menu( &selected );
    
#ifdef DOS_TERM    
    puttext(1, 1, 80, 25, buffer);
#endif

    return selected;
}

void Menu::do_menu( std::vector<MenuEntry*> *selected )
{
    sel = selected;
    draw_menu( selected );

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
        sel->clear();
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
        get_selected( sel );
        if (sel->size() == 1 && (flags & MF_SINGLESELECT))
            return false;
        draw_select_count( sel->size() );

        if (flags & MF_ANYPRINTABLE && !isdigit( keyin ))
            return false;

        break;
    }

    if (!isdigit( keyin ))
        num = -1;

    if (nav)
    {
        if (repaint)
            draw_menu( sel );
        // Easy exit should not kill the menu if there are selected items.
        else if (sel->empty() && is_set(MF_EASY_EXIT))
        {
            sel->clear();
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
    if (x > GXM || x < 1)
    {
        gotoxy(oldx, oldy);
        return false;
    }

    // Note: 1 <= x <= GXM; we have no fear of overflow.
    unsigned avail_width = GXM - x + 1;
    std::string towrite = s.length() > avail_width? s.substr(0, avail_width) :
                          s.length() == avail_width? s :
                                s + std::string(avail_width - s.length(), ' ');

    cprintf(towrite.c_str());
    
    gotoxy( oldx, oldy );
    return true;
}
    
void Menu::draw_select_count( int count )
{
    if (!is_set(MF_MULTISELECT))
        return;

    if (f_selitem)
    {
        draw_title_suffix( f_selitem( sel ) );
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

    return is_set(MF_SELECT_ANY_PAGE)? ishotkey
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

void Menu::draw_menu( std::vector< MenuEntry* > *selected )
{
    clrscr();

    draw_title();
    draw_select_count( selected->size() );
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

int Menu::item_colour(const MenuEntry *entry) const
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
        textcolor( item_colour(title) );
        cprintf( "%s", title->get_text().c_str() );
    }
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
    {
        textcolor( item_colour(items[index]) );
        cprintf( "%s", items[index]->get_text().c_str() );
    }
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
