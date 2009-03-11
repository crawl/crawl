/*
 *  File:       menu.cc
 *  Summary:    Menus and associated malarkey.
 *  Written by: Darshan Shaligram
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include <cctype>

#include "cio.h"
#include "menu.h"
#include "macro.h"
#include "message.h"
#include "player.h"
#include "tiles.h"
#include "tutorial.h"
#include "view.h"
#include "initfile.h"

MenuDisplay::MenuDisplay(Menu *menu) : m_menu(menu)
{
    m_menu->set_maxpagesize(get_number_of_lines());
}

MenuDisplayText::MenuDisplayText(Menu *menu) : MenuDisplay(menu), m_starty(1)
{
}

void MenuDisplayText::draw_stock_item(int index, const MenuEntry *me)
{
    textattr(m_menu->item_colour(index, me));
    if (m_menu->get_flags() & MF_ALLOW_FORMATTING)
        formatted_string::parse_string(me->get_text()).display();
    else
    {
        std::string text = me->get_text();
        if ((int) text.length() > get_number_of_cols())
            text = text.substr(0, get_number_of_cols());
        cprintf("%s", text.c_str());
    }
}

void MenuDisplayText::draw_more()
{
    cgotoxy(1, m_menu->get_y_offset() + m_menu->get_pagesize() -
            count_linebreaks(m_menu->get_more()));
    m_menu->get_more().display();
}

#ifdef USE_TILE
MenuDisplayTile::MenuDisplayTile(Menu *menu) : MenuDisplay(menu)
{
    m_menu->set_maxpagesize(tiles.get_menu()->maxpagesize());
}

void MenuDisplayTile::draw_stock_item(int index, const MenuEntry *me)
{
    int colour = m_menu->item_colour(index, me);
    std::string text = me->get_text();
    tiles.get_menu()->set_entry(index, text, colour, me);
}

void MenuDisplayTile::set_offset(int lines)
{
    tiles.get_menu()->set_offset(lines);
    m_menu->set_maxpagesize(tiles.get_menu()->maxpagesize());
}

void MenuDisplayTile::draw_more()
{
    tiles.get_menu()->set_more(m_menu->get_more());
    m_menu->set_maxpagesize(tiles.get_menu()->maxpagesize());
}

void MenuDisplayTile::set_num_columns(int columns)
{
    tiles.get_menu()->set_num_columns(columns);
    m_menu->set_maxpagesize(tiles.get_menu()->maxpagesize());
}
#endif

Menu::Menu( int _flags, const std::string& tagname, bool text_only )
  : f_selitem(NULL), f_drawitem(NULL), f_keyfilter(NULL), allow_toggle(false),
    menu_action(ACT_EXAMINE), title(NULL), flags(_flags), tag(tagname),
    first_entry(0), y_offset(0), pagesize(0), max_pagesize(0),
    more("-more-", true), items(), sel(), select_filter(),
    highlighter(new MenuHighlighter), num(-1), lastch(0), alive(false),
    last_selected(-1)
{
#ifdef USE_TILE
    if (text_only)
        mdisplay = new MenuDisplayText(this);
    else
        mdisplay = new MenuDisplayTile(this);
#else
    mdisplay = new MenuDisplayText(this);
#endif
    mdisplay->set_num_columns(1);
    set_flags(flags);
}

Menu::Menu( const formatted_string &fs )
 : f_selitem(NULL), f_drawitem(NULL), f_keyfilter(NULL), title(NULL),

   // This is a text-viewer menu, init flags to be easy on the user.
   flags(MF_NOSELECT | MF_EASY_EXIT),

   tag(), first_entry(0), y_offset(0), pagesize(0),
   max_pagesize(0), more("-more-", true), items(), sel(),
   select_filter(), highlighter(new MenuHighlighter), num(-1),
   lastch(0), alive(false), last_selected(-1)
{
#ifdef USE_TILE
    mdisplay = new MenuDisplayText(this);
#else
    mdisplay = new MenuDisplayText(this);
#endif
    mdisplay->set_num_columns(1);

    int colour = LIGHTGREY;
    int last_text_colour = LIGHTGREY;
    std::string line;
    for (formatted_string::oplist::const_iterator i = fs.ops.begin();
         i != fs.ops.end(); ++i)
    {
        const formatted_string::fs_op &op(*i);
        switch (op.type)
        {
        case FSOP_COLOUR:
            colour = op.x;
            break;
        case FSOP_TEXT:
        {
            line += op.text;

            const std::string::size_type nonblankp =
                op.text.find_first_not_of(" \t\r\n");
            const bool nonblank = nonblankp != std::string::npos;
            const std::string::size_type eolp = op.text.find(EOL);
            const bool starts_with_eol =
                nonblank && eolp != std::string::npos
                && eolp < nonblankp;

            if (nonblank && !starts_with_eol)
                last_text_colour = colour;

            check_add_formatted_line(last_text_colour, colour, line, true);

            if (nonblank && starts_with_eol)
                last_text_colour = colour;
            break;
        }
        default:
            break;
        }
    }
    check_add_formatted_line(last_text_colour, colour, line, false);
}

void Menu::check_add_formatted_line(int firstcol, int nextcol,
                                    std::string &line, bool check_eol)
{
    if (line.empty())
        return;

    if (check_eol && line.find(EOL) == std::string::npos)
        return;

    std::vector<std::string> lines = split_string(EOL, line, false, true);
    int size = lines.size();

    // If we have stuff after EOL, leave that in the line variable and
    // don't add an entry for it, unless the caller told us not to
    // check EOL sanity.
    if (check_eol && !ends_with(line, EOL))
        line = lines[--size];
    else
        line.clear();

    for (int i = 0, col = firstcol; i < size; ++i, col = nextcol)
    {
        std::string &s(lines[i]);

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
    for (int i = 0, count = items.size(); i < count; ++i)
        delete items[i];
    delete title;
    delete highlighter;
    delete mdisplay;
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
    if (use_options && Options.easy_exit_menu)
        flags |= MF_EASY_EXIT;
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
    entry->tag = tag;
    items.push_back( entry );
}

void Menu::reset()
{
    first_entry = 0;
}

std::vector<MenuEntry *> Menu::show(bool reuse_selections)
{
    cursor_control cs(false);

    if (reuse_selections)
    {
        get_selected(&sel);
    }
    else
    {
        deselect_all(false);
        sel.clear();
    }

    // Reset offset to default.
    mdisplay->set_offset(1 + !!title);

    // Lose lines for the title + room for -more- line.
#ifdef USE_TILE
    pagesize = max_pagesize - !!title - 1;
#else
    pagesize = get_number_of_lines() - !!title - 1;
    if (max_pagesize > 0 && pagesize > max_pagesize)
        pagesize = max_pagesize;
#endif

    do_menu();

    return (sel);
}

void Menu::do_menu()
{
    draw_menu();

    alive = true;
    while (alive)
    {
#ifndef USE_TILE
        int keyin = getchm(KC_MENU, c_getch);
#else
        mouse_control mc(MOUSE_MODE_MORE);
        int keyin = getch();
#endif

        if (!process_key( keyin ))
            return;
    }
}

bool Menu::is_set(int flag) const
{
    return (flags & flag) == flag;
}

int Menu::pre_process(int k)
{
    return (k);
}

int Menu::post_process(int k)
{
    return (k);
}

bool Menu::process_key( int keyin )
{
    if (items.size() == 0)
    {
        lastch = keyin;
        return (false);
    }
    else if (allow_toggle && (keyin == '!' || keyin == '?'))
    {
        sel.clear();
        menu_action = (action)((menu_action+1) % ACT_NUM);
        update_title();
        return (true);
    }

    bool nav = false, repaint = false;

    if (f_keyfilter)
        keyin = (*f_keyfilter)(keyin);

    keyin = pre_process(keyin);

    switch (keyin)
    {
    case 0:
        return (true);
    case CK_ENTER:
        return (false);
    case CK_ESCAPE:
    case CK_MOUSE_B2:
    case CK_MOUSE_CMD:
        sel.clear();
        lastch = keyin;
        return (false);
    case ' ': case CK_PGDN: case '>': case '\'':
    case CK_MOUSE_B1:
    case CK_MOUSE_CLICK:
        nav = true;
        repaint = page_down();
        if (!repaint && !is_set(MF_EASY_EXIT) && !is_set(MF_NOWRAP))
        {
            repaint = (first_entry != 0);
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
    case CK_HOME:
        nav = true;
        repaint = (first_entry != 0);
        first_entry = 0;
        break;
    case CK_END:
    {
        nav = true;
        const int breakpoint = (items.size() + 1) - pagesize;
        if (first_entry < breakpoint)
        {
            first_entry = breakpoint;
            repaint = true;
        }
        break;
    }
    case CONTROL('F'):
    {
        if (!(flags & MF_ALLOW_FILTER))
            break;
        char linebuf[80];
        cgotoxy(1,1);
        clear_to_end_of_line();
        textcolor(WHITE);
        cprintf("Select what? (regex) ");
        textcolor(LIGHTGREY);
        bool validline = !cancelable_get_line(linebuf, sizeof linebuf, 80);
        if (validline && linebuf[0])
        {
            text_pattern tpat(linebuf, true);
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
                        return (false);
                    }
                }
            }
            get_selected(&sel);
        }
        nav = true;
        repaint = true;
        break;
    }
    case '.':
        if (last_selected != -1)
        {
            if ((first_entry + pagesize - last_selected) == 1)
            {
                page_down();
                nav = true;
            }

            select_index(last_selected + 1);
            get_selected(&sel);

            repaint = true;
        }
        break;

    default:
        keyin  = post_process(keyin);
        lastch = keyin;

        // If no selection at all is allowed, exit now.
        if (!(flags & (MF_SINGLESELECT | MF_MULTISELECT)))
            return (false);

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
            return (false);

        draw_select_count( sel.size() );

        if (flags & MF_ANYPRINTABLE
            && (!isdigit(keyin) || is_set(MF_NO_SELECT_QTY)))
        {
            return (false);
        }

        break;
    }

    if (last_selected != -1
        && (items.size() == ((unsigned int) last_selected + 1)
            || items[last_selected + 1] == NULL
            || items[last_selected + 1]->level != MEL_ITEM))
    {
        last_selected = -1;
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
            return (false);
        }
    }
    return (true);
}

bool Menu::draw_title_suffix( const std::string &s, bool titlefirst )
{
    int oldx = wherex(), oldy = wherey();

    if (titlefirst)
        draw_title();

    int x = wherex();
    if (x > get_number_of_cols() || x < 1)
    {
        cgotoxy(oldx, oldy);
        return (false);
    }

    // Note: 1 <= x <= get_number_of_cols(); we have no fear of overflow.
    unsigned avail_width = get_number_of_cols() - x + 1;
    std::string towrite = s.length() > avail_width? s.substr(0, avail_width) :
                          s.length() == avail_width? s :
                                s + std::string(avail_width - s.length(), ' ');

    cprintf("%s", towrite.c_str());

    cgotoxy( oldx, oldy );
    return (true);
}

bool Menu::draw_title_suffix( const formatted_string &fs, bool titlefirst )
{
    int oldx = wherex(), oldy = wherey();

    if (titlefirst)
        draw_title();

    int x = wherex();
    if (x > get_number_of_cols() || x < 1)
    {
        cgotoxy(oldx, oldy);
        return (false);
    }

    // Note: 1 <= x <= get_number_of_cols(); we have no fear of overflow.
    const unsigned int avail_width = get_number_of_cols() - x + 1;
    const unsigned int fs_length = fs.length();
    if (fs_length > avail_width)
    {
        formatted_string fs_trunc = fs.substr(0, avail_width);
        fs_trunc.display();
    }
    else
    {
        fs.display();
        if (fs_length < avail_width)
        {
            char fmt[20];
            sprintf(fmt, "%%%ds", avail_width-fs_length);
            cprintf(fmt, " ");
        }
    }

    cgotoxy( oldx, oldy );
    return (true);
}

void Menu::draw_select_count(int count, bool force)
{
    if (!force && !is_set(MF_MULTISELECT))
        return;

    if (f_selitem)
    {
        draw_title_suffix(f_selitem(&sel));
    }
    else
    {
        char buf[100] = "";
        if (count)
        {
            snprintf(buf, sizeof buf, "  (%d item%s)  ", count,
                    (count > 1? "s" : ""));
        }
        draw_title_suffix(buf);
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
    if (end > static_cast<int>(items.size())) end = items.size();

    bool ishotkey = (is_set(MF_SINGLESELECT) ? items[i]->is_primary_hotkey(key)
                                             : items[i]->is_hotkey(key));

    return !is_set(MF_SELECT_BY_PAGE) ? ishotkey
                                      : ishotkey && i >= first_entry && i < end;
}

void Menu::select_items(int key, int qty)
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
    cgotoxy( x, y );
}

#ifdef USE_TILE
bool MenuEntry::get_tiles(std::vector<tile_def>& tileset) const
{
    // Is this a monster?
    monsters *m = (monsters*)(data);
    if (!m)
        return (false);

    const coord_def c = m->pos();
    const dungeon_feature_type feat = grd(c);
    tileset.push_back(tile_def(tileidx_feature(feat, c.x, c.y), TEX_DUNGEON));
    tileset.push_back(tile_def(tileidx_monster_base(m), TEX_PLAYER));

    return (true);
}
#endif

bool Menu::is_selectable(int item) const
{
    if (select_filter.empty())
        return (true);

    std::string text = items[item]->get_filter_text();
    for (int i = 0, count = select_filter.size(); i < count; ++i)
        if (select_filter[i].matches(text))
            return (true);

    return (false);
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
                if (items[i]->level != MEL_ITEM
                    || items[i]->hotkeys.empty())
                {
                    continue;
                }
                if (is_hotkey(i, items[i]->hotkeys[0]) && is_selectable(i))
                {
                    last_selected = i;
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
            if (items[i]->level != MEL_ITEM
                || items[i]->hotkeys.empty())
            {
                continue;
            }
            if (is_hotkey(i, items[i]->hotkeys[0]))
            {
                last_selected = i;
                items[i]->select( qty );
                draw_item( i );
            }
        }
    }
    else if (items[si]->level == MEL_ITEM
             && (flags & (MF_SINGLESELECT | MF_MULTISELECT)))
    {
        last_selected = si;
        items[si]->select( qty );
        draw_item( si );
    }
}

int Menu::get_entry_index( const MenuEntry *e ) const
{
    int index = 0;
    for (unsigned int i = first_entry; i < items.size(); i++)
    {
        if (items[i] == e)
            return (index);

        if (items[i]->quantity != 0)
            index++;
    }

    return -1;
}

void Menu::draw_menu()
{
    clrscr();

    draw_title();
    draw_select_count( sel.size() );
    y_offset = 1 + !!title;

    mdisplay->set_offset(y_offset);

    int end = first_entry + pagesize;
    if (end > (int) items.size()) end = items.size();

    for (int i = first_entry; i < end; ++i)
        draw_item( i );

    if (end < (int) items.size() || is_set(MF_ALWAYS_SHOW_MORE))
        mdisplay->draw_more();
}

void Menu::update_title()
{
    int x = wherex(), y = wherey();
    draw_title();
    cgotoxy(x, y);
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
        cgotoxy(1, 1);
        write_title();
    }
}

void Menu::write_title()
{
    textattr( item_colour(-1, title) );

    cprintf("%s", title->get_text().c_str());
    if (flags & MF_SHOW_PAGENUMBERS)
    {
        // The total number of pages is well defined, but the current
        // page a bit less so. To make sense, we hack it so that your
        // current page is based on the first line you're seeing, *unless*
        // you're seeing the last item.
        int numpages = items.empty() ? 1 : ((items.size()-1) / pagesize + 1);
        int curpage = first_entry / pagesize + 1;
        if (in_page(items.size() - 1))
            curpage = numpages;
        cprintf(" (page %d of %d)", curpage, numpages);
    }

    const int x = wherex(), y = wherey();
    cprintf("%-*s", get_number_of_cols() - x, "");
    cgotoxy(x, y);
}

bool Menu::in_page(int index) const
{
    return (index >= first_entry && index < first_entry + pagesize);
}

void Menu::draw_item( int index ) const
{
    if (!in_page(index))
        return;
    cgotoxy( 1, y_offset + index - first_entry );

    draw_index_item(index, items[index]);
}

void Menu::draw_index_item(int index, const MenuEntry *me) const
{
    if (f_drawitem)
        (*f_drawitem)(index, me);
    else
        draw_stock_item(index, me);
}

void Menu::draw_stock_item(int index, const MenuEntry *me) const
{
    mdisplay->draw_stock_item(index, me);
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
            return (true);
    }
    return (false);
}

bool Menu::page_up()
{
    int old_first = first_entry;

    if (first_entry > 0)
    {
        if ((first_entry -= pagesize) < 0)
            first_entry = 0;

        if (old_first != first_entry)
            return (true);
    }
    return (false);
}

bool Menu::line_down()
{
    if (first_entry + pagesize < (int) items.size())
    {
        ++first_entry;
        return (true);
    }
    return (false);
}

bool Menu::line_up()
{
    if (first_entry > 0)
    {
        --first_entry;
        return (true);
    }
    return (false);
}

/////////////////////////////////////////////////////////////////
// slider_menu

slider_menu::slider_menu(int fl)
    : Menu(fl), less(), starty(1), endy(get_number_of_lines()),
      selected(0), need_less(true), need_more(true), oldselect(0),
      lastkey(0), search()
{
    less.textcolor(DARKGREY);
    less.cprintf("<---- More");
    more.clear();
    more.textcolor(DARKGREY);
    more.cprintf("More ---->");
}

void slider_menu::set_search(const std::string &s)
{
    search = s;
}

void slider_menu::set_limits(int y1, int y2)
{
    starty = y1;
    endy   = y2;
}

void slider_menu::select_search(const std::string &s)
{
    std::string srch = s;
    lowercase(srch);

    for (int i = 0, size = items.size(); i < size; ++i)
    {
        std::string text = items[i]->get_text();
        lowercase(text);

        std::string::size_type found = text.find(srch);
        if (found != std::string::npos
                && found == text.find_first_not_of(" "))
        {
            move_selection(i);
            break;
        }
    }
}

int slider_menu::post_process(int key)
{
    const time_t now = time(NULL);
    if (now - lastkey >= 2)
        search.clear();
    lastkey = now;
    select_search( search += key );
    return (key);
}

bool slider_menu::process_key(int key)
{
    // Some of this key processing should really be in a user-passed-in function
    // If we ever need to use slider_menu elsewhere, we should factor it out.
    if (key == CK_ESCAPE || key == '\t')
    {
        oldselect = selected;
        selected = -1;
        draw_item(oldselect);
        sel.clear();
        search.clear();
        lastch = key;
        return (false);
    }

    if (Menu::is_set(MF_NOWRAP) && selected == 0
        && (key == CK_UP || key == CK_PGUP || key == '<' || key == ';'))
    {
        oldselect = selected;
        selected = -1;
        draw_item(oldselect);
        search.clear();
        return (false);
    }

    return Menu::process_key(key);
}

void slider_menu::adjust_pagesizes(int recurse_depth)
{
    if (first_entry == 1 && selected == 1)
        first_entry = 0;

    need_less = !!first_entry;
    pagesize = endy - starty + 1 - !!title - need_less;
    const int nitems = items.size();
    need_more = first_entry + pagesize < nitems;
    if (need_more)
        pagesize--;

    if (selected != -1
            && (selected < first_entry || selected >= first_entry + pagesize)
            && recurse_depth > 0)
        fix_entry(recurse_depth - 1);

    calc_y_offset();
}

void slider_menu::display()
{
    adjust_pagesizes();

    if (selected != -1)
        oldselect = selected;
    selected = -1;
    draw_menu();
}

std::vector<MenuEntry *> slider_menu::show()
{
    cursor_control coff(false);

    sel.clear();

    adjust_pagesizes();

    if (selected == -1)
        selected = oldselect;

    if (!search.empty())
        select_search(search);

    fix_entry();
    do_menu();

    if (selected >= 0 && selected < (int) items.size())
        sel.push_back(items[selected]);
    return (sel);
}

const MenuEntry *slider_menu::selected_entry() const
{
    if (selected >= 0 && selected < (int) items.size())
        return (items[selected]);

    return (NULL);
}

void slider_menu::fill_line() const
{
    const int x = wherex(), maxx = get_number_of_cols();
    if (x < maxx)
        cprintf("%-*s", maxx - x, "");
}

void slider_menu::draw_stock_item(int index, const MenuEntry *me) const
{
    Menu::draw_stock_item(index, me);
    fill_line();
}

int slider_menu::item_colour(int index, const MenuEntry *me) const
{
    int colour = Menu::item_colour(index, me);
    if (index == selected && selected != -1)
    {
#if defined(WIN32CONSOLE) || defined(DOS)
        colour = dos_brand(colour, CHATTR_REVERSE);
#elif defined(USE_TILE)
        colour = (colour == WHITE ? YELLOW : WHITE);
#else
        colour |= COLFLAG_REVERSE;
#endif
    }
    return (colour);
}

void slider_menu::show_less()
{
    if (!need_less)
        return ;

    if (first_entry > 0)
        less.display();
    else
        textattr(LIGHTGREY);
    fill_line();
}

void slider_menu::show_more()
{
    if (!need_more)
        return ;
    const int end = entry_end();
    cgotoxy( 1, y_offset + pagesize );
    if (end < (int) items.size() || is_set(MF_ALWAYS_SHOW_MORE))
        mdisplay->draw_more();
    else
        textattr(LIGHTGREY);
    fill_line();
}

void slider_menu::calc_y_offset()
{
    y_offset = starty + !!title + need_less;
}

int slider_menu::entry_end() const
{
    int end = first_entry + pagesize;
    if (end > (int) items.size()) end = items.size();
    return (end);
}

void slider_menu::draw_menu()
{
    cgotoxy(1, starty);
    write_title();

    calc_y_offset();

    int end = entry_end();

    // We're using get_number_of_cols() - 1 because we want to avoid line wrap
    // on DOS (the conio.h functions go batshit if that happens).
    cgotoxy(1, y_offset - 1);

    show_less();

    mdisplay->set_offset(starty + 1);

    for (int i = first_entry; i < end; ++i)
        draw_item( i );

    textattr(LIGHTGREY);
    for (int i = end; i < first_entry + pagesize; ++i)
    {
        cgotoxy(1, y_offset + i - first_entry);
        cprintf("%-*s", get_number_of_cols() - 2, "");
    }

    show_more();
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

bool slider_menu::fix_entry(int recurse_depth)
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

    if (recurse_depth > 0)
        adjust_pagesizes(recurse_depth - 1);

    return (first_entry != oldfirst);
}

void slider_menu::new_selection(int nsel)
{
    if (nsel < 0)
    {
        if ( !is_set(MF_NOWRAP) )
        {
            do
            {
                nsel += items.size();
            }
            while ( nsel < 0 );
        }
        else
            nsel = 0;
    }
    if (nsel >= static_cast<int>(items.size()))
    {
        if ( !is_set(MF_NOWRAP) )
        {
            do
            {
                nsel -= items.size();
            }
            while ( nsel >= static_cast<int>(items.size()) );
        }
        else
            nsel = items.size() - 1;
    }

    const int old = selected;
    selected = nsel;
    if (old != selected && nsel >= first_entry && nsel < first_entry + pagesize)
    {
        draw_item(old);
        draw_item(selected);
    }
}

bool slider_menu::move_selection(int nsel)
{
    new_selection(nsel);
    return fix_entry();
}

bool slider_menu::page_down()
{
    search.clear();
    return move_selection( selected + pagesize );
}

bool slider_menu::page_up()
{
    search.clear();
    return move_selection( selected - pagesize );
}

bool slider_menu::line_down()
{
    search.clear();
    return move_selection( selected + 1 );
}

bool slider_menu::line_up()
{
    search.clear();
    return move_selection( selected - 1 );
}

/////////////////////////////////////////////////////////////////
// Menu colouring
//

int menu_colour(const std::string &text, const std::string &prefix,
                const std::string &tag)
{
    const std::string tmp_text = prefix + text;

    for (unsigned int i = 0; i < Options.menu_colour_mappings.size(); ++i)
    {
        const colour_mapping &cm = Options.menu_colour_mappings[i];
        if ((cm.tag.empty() || cm.tag == "any" || cm.tag == tag
               || cm.tag == "inventory" && tag == "pickup")
            && cm.pattern.matches(tmp_text) )
        {
            return (cm.colour);
        }
    }
    return (-1);
}

int MenuHighlighter::entry_colour(const MenuEntry *entry) const
{
    return entry->highlight_colour();
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

formatted_scroller::formatted_scroller() : Menu()
{
    set_highlighter(NULL);
}

formatted_scroller::formatted_scroller(int _flags, const std::string& s) :
    Menu(_flags)
{
    set_highlighter(NULL);
    add_text(s);
}

void formatted_scroller::add_text(const std::string& s)
{
    size_t eolpos = 0;
    while ( true )
    {
        const size_t newpos = s.find( "\n", eolpos );
        add_item_formatted_string(formatted_string::parse_string(std::string(s, eolpos, newpos-eolpos)));
        if ( newpos == std::string::npos )
            break;
        else
            eolpos = newpos + 1;
    }
}

void formatted_scroller::add_item_formatted_string(const formatted_string& fs,
                                                   int hotkey)
{
    MenuEntry* me = new MenuEntry;
    me->data = new formatted_string(fs);
    if (hotkey)
    {
        me->add_hotkey(hotkey);
        me->quantity = 1;
    }
    add_entry(me);
}

void formatted_scroller::add_item_string(const std::string& s, int hotkey)
{
    MenuEntry* me = new MenuEntry(s);
    if (hotkey)
        me->add_hotkey(hotkey);
    add_entry(me);
}

void formatted_scroller::draw_index_item(int index, const MenuEntry *me) const
{
    if (me->data == NULL)
        Menu::draw_index_item(index, me);
    else
        static_cast<formatted_string*>(me->data)->display();
}

formatted_scroller::~formatted_scroller()
{
    // Very important: this destructor is called *before* the base
    // (Menu) class destructor... which is as it should be.
    for (unsigned i = 0; i < items.size(); ++i)
        if (items[i]->data != NULL)
            delete static_cast<formatted_string*>(items[i]->data);
}

int linebreak_string( std::string& s, int wrapcol, int maxcol )
{
    size_t loc = 0;
    int xpos = 0;
    int breakcount = 0;
    while ( loc < s.size() )
    {
        if ( s[loc] == '<' )    // tag
        {
            // << escape
            if ( loc + 1 < s.size() && s[loc+1] == '<' )
            {
                ++xpos;
                loc += 2;
                // Um, we never break on <<. That's a feature. Right.
                continue;
            }
            // skip tag
            while ( loc < s.size() && s[loc] != '>' )
                ++loc;
            ++loc;
        }
        else
        {
            // user-forced newline
            if ( s[loc] == '\n' )
                xpos = 0;
            // soft linebreak
            else if ( s[loc] == ' ' && xpos > wrapcol )
            {
                s.replace(loc, 1, "\n");
                xpos = 0;
                ++breakcount;
            }
            // hard linebreak
            else if ( xpos > maxcol )
            {
                s.insert(loc, "\n");
                xpos = 0;
                ++breakcount;
            }
            // bog-standard
            else
                ++xpos;

            ++loc;
        }
    }
    return breakcount;
}

int linebreak_string2( std::string& s, int maxcol )
{
    size_t loc = 0;
    int xpos = 0, spaceloc = 0;
    int breakcount = 0;
    while ( loc < s.size() )
    {
        if ( s[loc] == '<' )    // tag
        {
            // << escape
            if ( loc + 1 < s.size() && s[loc+1] == '<' )
            {
                ++xpos;
                loc += 2;
                // Um, we never break on <<. That's a feature. Right.
                continue;
            }
            // skip tag
            while ( loc < s.size() && s[loc] != '>' )
                ++loc;
            ++loc;
        }
        else
        {
            // user-forced newline, or one we just stuffed in
            if ( s[loc] == '\n' )
            {
                xpos = 0;
                spaceloc = 0;
                ++loc;
                continue;
            }

            // force a wrap?
            if ( xpos >= maxcol )
            {
                if (spaceloc)
                {
                    loc = spaceloc;
                    s.replace(loc, 1, "\n");
                }
                else
                {
                    s.insert(loc, "\n");
                }
                ++breakcount;
                // reset pointers when we come around and see the \n
                continue;
            }

            // save possible linebreak location
            if ( s[loc] == ' ' && xpos > 0)
            {
                spaceloc = loc;
            }

            ++xpos;
            ++loc;
        }
    }
    return breakcount;
}

std::string get_linebreak_string(const std::string& s, int maxcol)
{
    std::string r = s;
    linebreak_string2(r, maxcol);
    return r;
}

// Takes a (possibly tagged) string, breaks it into lines and
// prints it into the given message channel.
void print_formatted_paragraph(std::string &s, msg_channel_type channel)
{
    int maxcol = get_number_of_cols();
    if (Options.delay_message_clear)
        --maxcol;

    linebreak_string2(s,maxcol);
    std::string text;

    size_t loc = 0, oldloc = 0;
    while ( loc < s.size() )
    {
        if (s[loc] == '\n')
        {
            text = s.substr(oldloc, loc-oldloc);
            formatted_message_history( text, channel );
            oldloc = ++loc;
        }
        loc++;
    }
    formatted_message_history( s.substr(oldloc, loc-oldloc), channel );
}

bool formatted_scroller::jump_to( int i )
{
    if (i == first_entry + 1)
        return (false);

    if (i == 0)
        first_entry = 0;
    else
        first_entry = i - 1;

    return (true);
}

// Don't scroll past MEL_TITLE entries
bool formatted_scroller::page_down()
{
    const int old_first = first_entry;

    if ((int) items.size() <= first_entry + pagesize)
        return (false);

    // If, when scrolling forward, we encounter a MEL_TITLE
    // somewhere in the newly displayed page, stop scrolling
    // just before it becomes visible
    int target;
    for (target = first_entry; target < first_entry + pagesize; ++target )
    {
        const int offset = target + pagesize;
        if (offset < (int)items.size() && items[offset]->level == MEL_TITLE)
            break;
    }
    first_entry = target;
    return (old_first != first_entry);
}

bool formatted_scroller::page_up()
{
    int old_first = first_entry;

    // If, when scrolling backward, we encounter a MEL_TITLE
    // somewhere in the newly displayed page, stop scrolling
    // just before it becomes visible

    if (items[first_entry]->level == MEL_TITLE)
        return (false);

    for ( int i = 0; i < pagesize; ++i )
    {
        if (first_entry == 0 || items[first_entry-1]->level == MEL_TITLE)
            break;

        --first_entry;
    }

    return (old_first != first_entry);
}

bool formatted_scroller::line_down()
{
    if (first_entry + pagesize < static_cast<int>(items.size()) &&
        items[first_entry + pagesize]->level != MEL_TITLE )
    {
        ++first_entry;
        return (true);
    }
    return (false);
}

bool formatted_scroller::line_up()
{
    if (first_entry > 0 && items[first_entry-1]->level != MEL_TITLE
        && items[first_entry]->level != MEL_TITLE)
    {
        --first_entry;
        return (true);
    }
    return (false);
}

bool formatted_scroller::jump_to_hotkey( int keyin )
{
    for (unsigned int i = 0; i < items.size(); ++i)
        if (items[i]->is_hotkey(keyin))
            return jump_to(i);

    return (false);
}


bool formatted_scroller::process_key( int keyin )
{
    lastch = keyin;

    if (f_keyfilter)
        keyin = (*f_keyfilter)(keyin);

    bool repaint = false;
    // Any key is assumed to be a movement key for now...
    bool moved = true;
    switch (keyin)
    {
    case 0:
        return (true);
    case -1:
    case CK_ESCAPE:
    case CK_MOUSE_CMD:
        return (false);
    case ' ': case '+': case '=': case CK_PGDN: case '>': case '\'':
    case CK_MOUSE_B5:
    case CK_MOUSE_CLICK:
        repaint = page_down();
        break;
    case '-': case CK_PGUP: case '<': case ';':
    case CK_MOUSE_B4:
        repaint = page_up();
        break;
    case CK_UP:
        repaint = line_up();
        break;
    case CK_DOWN:
    case CK_ENTER:
        repaint = line_down();
        break;
    case CK_HOME:
        repaint = jump_to(0);
        break;
    case CK_END:
    {
        const int breakpoint = (items.size() + 1) - pagesize;
        if (first_entry < breakpoint)
            repaint = jump_to(breakpoint);
        break;
    }
    default:
        if (is_set(MF_SINGLESELECT))
        {
            select_items( keyin, -1 );
            get_selected( &sel );
            if (sel.size() >= 1)
                return (false);
        }
        else
            repaint = jump_to_hotkey(keyin);

        break;
    }

    if (repaint)
        draw_menu();
    else if (moved && is_set(MF_EASY_EXIT))
        return (false);

    return (true);
}

int ToggleableMenu::pre_process(int key)
{
    if (std::find(toggle_keys.begin(), toggle_keys.end(), key) !=
            toggle_keys.end())
    {
        // Toggle all menu entries
        for (unsigned int i = 0; i < items.size(); ++i)
        {
            ToggleableMenuEntry* const p =
                dynamic_cast<ToggleableMenuEntry*>(items[i]);

            if (p)
                p->toggle();
        }

        // Toggle title
        ToggleableMenuEntry* const pt =
            dynamic_cast<ToggleableMenuEntry*>(title);

        if (pt)
            pt->toggle();

        // Redraw
        draw_menu();

        // Don't further process the key
        return 0;
    }
    return key;
}
