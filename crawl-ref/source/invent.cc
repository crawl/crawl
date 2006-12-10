/*
 *  File:       invent.cc
 *  Summary:    Functions for inventory related commands.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <5>     10/9/99     BCR     Added wizard help screen
 *      <4>     10/1/99     BCR     Clarified help screen
 *      <3>     6/9/99      DML     Autopickup
 *      <2>     5/20/99     BWR     Extended screen lines support
 *      <1>     -/--/--     LRH     Created
 */

#include "AppHdr.h"
#include "invent.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "clua.h"
#include "itemname.h"
#include "items.h"
#include "macro.h"
#include "player.h"
#include "shopping.h"
#include "stuff.h"
#include "view.h"
#include "menu.h"

///////////////////////////////////////////////////////////////////////////////
// Inventory menu shenanigans

static void get_inv_items_to_show(
        std::vector<const item_def*> &v, int selector);

InvTitle::InvTitle( Menu *mn, const std::string &title,
          invtitle_annotator tfn ) 
    : MenuEntry( title, MEL_TITLE )
{
    m       = mn;
    titlefn = tfn;
}

std::string InvTitle::get_text() const
{
    return titlefn? titlefn( m, MenuEntry::get_text() ) :
                    MenuEntry::get_text();
}

InvEntry::InvEntry( const item_def &i ) : MenuEntry( "", MEL_ITEM ), item( &i )
{
    data = const_cast<item_def *>( item );
        
    char buf[ITEMNAME_SIZE];
    if (i.base_type == OBJ_GOLD)
        snprintf(buf, sizeof buf, "%d gold piece%s", i.quantity, 
                (i.quantity > 1? "s" : ""));
    else
        item_name(i, 
                in_inventory(i)? 
                    DESC_INVENTORY_EQUIP : DESC_NOCAP_A, buf, false);    
    text = buf;

    if (i.base_type != OBJ_GOLD && in_inventory(i))
    {
        // FIXME: This is HORRIBLE! We're skipping the inventory letter prefix 
        // which looks like this: "a - ".
        text = text.substr( 4 );
        add_hotkey(index_to_letter( i.link ));
    }
    else
    {
        // Dummy hotkey for gold or non-inventory items.
        add_hotkey(' ');
    }
    add_class_hotkeys(i);

    quantity = i.quantity;
}

std::string InvEntry::get_text() const
{
    char buf[ITEMNAME_SIZE];
    char suffix[ITEMNAME_SIZE] = "";

    if (InvEntry::show_prices)
    {
        int value = item_value(*item, temp_id, true);
        if (value > 0)
            snprintf(suffix, sizeof suffix,
                " (%d gold)", value);
    }
    snprintf( buf, sizeof buf,
            " %c %c %s%s",
            hotkeys[0],
            (!selected_qty? '-' : selected_qty < quantity? '#' : '+'),
            text.c_str(),
            suffix );
    return (buf);
}

void InvEntry::add_class_hotkeys(const item_def &i)
{
    switch (i.base_type)
    {
    case OBJ_GOLD:
        add_hotkey('$');
        break;
    case OBJ_MISSILES:
        add_hotkey('(');
        break;
    case OBJ_WEAPONS:
        add_hotkey(')');
        break;
    case OBJ_ARMOUR:
        add_hotkey('[');
        break;
    case OBJ_WANDS:
        add_hotkey('/');
        break;
    case OBJ_FOOD:
        add_hotkey('%');
        break;
    case OBJ_BOOKS:
        add_hotkey('+');
        add_hotkey(':');
        break;
    case OBJ_SCROLLS:
        add_hotkey('?');
        break;
    case OBJ_JEWELLERY:
        add_hotkey(i.sub_type >= AMU_RAGE? '"' : '='); 
        break;
    case OBJ_POTIONS:
        add_hotkey('!');
        break;
    case OBJ_STAVES:
        add_hotkey('\\');
        add_hotkey('|');
        break;
    case OBJ_MISCELLANY:
        add_hotkey('}');
        break;
    case OBJ_CORPSES:
        add_hotkey('&');
        break;
    default:
        break;
    }
}

bool InvEntry::show_prices = false;
char InvEntry::temp_id[4][50];

void InvEntry::set_show_prices(bool doshow)
{
    if ((show_prices = doshow))
    {
        memset(temp_id, 1, sizeof temp_id);
    }
}

InvShowPrices::InvShowPrices(bool doshow)
{
    InvEntry::set_show_prices(doshow);
}

InvShowPrices::~InvShowPrices()
{
    InvEntry::set_show_prices(false);
}

// Returns vector of item_def pointers to each item_def in the given
// vector. Note: make sure the original vector stays around for the lifetime
// of the use of the item pointers, or mayhem results!
std::vector<const item_def*>
InvMenu::xlat_itemvect(const std::vector<item_def> &v)
{
    std::vector<const item_def*> xlatitems;
    for (unsigned i = 0, size = v.size(); i < size; ++i)
        xlatitems.push_back( &v[i] );
    return (xlatitems);
}

void InvMenu::set_type(menu_type t)
{
    type = t;
}

void InvMenu::set_title_annotator(invtitle_annotator afn)
{
    title_annotate = afn;
}

void InvMenu::set_title(MenuEntry *t)
{
    Menu::set_title(t);
}

void InvMenu::set_preselect(const std::vector<SelItem> *pre)
{
    pre_select = pre;
}

void InvMenu::set_title(const std::string &s)
{
    std::string stitle = s;
    if (stitle.empty())
    {
        const int cap = carrying_capacity();

        char title_buf[200];
        snprintf( title_buf, sizeof title_buf,
                  "Inventory: %d.%d/%d.%d aum (%d%%, %d/52 slots)",
                    you.burden / 10, you.burden % 10, 
                    cap / 10, cap % 10,
                    (you.burden * 100) / cap, 
                    inv_count() );
        stitle = title_buf;
    }
    
    set_title(new InvTitle(this, stitle, title_annotate));
}

void InvMenu::load_inv_items(int item_selector,
                             MenuEntry *(*procfn)(MenuEntry *me))
{
    std::vector<const item_def *> tobeshown;
    get_inv_items_to_show(tobeshown, item_selector);

    load_items(tobeshown, procfn);
    if (!item_count())
    {
        std::string s;
        switch (item_selector)
        {
        case OSEL_ANY:
            s = "You aren't carrying anything.";
            break;
        case OSEL_WIELD:
        case OBJ_WEAPONS:
            s = "You aren't carrying any weapons.";
            break;
        default:
            s = "You aren't carrying any such object.";
            break;
        }
        set_title(s);
    }
    else
    {
        set_title("");
    }
}

static bool compare_menu_entries(const MenuEntry* a, const MenuEntry* b)
{
    return (*a < *b);
}

void InvMenu::load_items(const std::vector<const item_def*> &mitems,
                         MenuEntry *(*procfn)(MenuEntry *me))
{
    FixedVector< int, NUM_OBJECT_CLASSES > inv_class(0);
    for (int i = 0, count = mitems.size(); i < count; ++i)
        inv_class[ mitems[i]->base_type ]++;

    menu_letter ckey;
    std::vector<InvEntry*> items_in_class;

    for (int i = 0; i < NUM_OBJECT_CLASSES; ++i)
    {
        if (!inv_class[i]) continue;

        add_entry( new MenuEntry( item_class_name(i), MEL_SUBTITLE ) );
        items_in_class.clear();

        for (int j = 0, count = mitems.size(); j < count; ++j)
        {
            if (mitems[j]->base_type != i) continue;
            items_in_class.push_back( new InvEntry(*mitems[j]) );
        }

        if (Options.sort_menus != -1 &&
            (int)items_in_class.size() >= Options.sort_menus)
            std::sort( items_in_class.begin(), items_in_class.end(),
                       compare_menu_entries );

        for (unsigned int j = 0; j < items_in_class.size(); ++j)
        {
            InvEntry *ie = items_in_class[j];
            // If there's no hotkey, provide one.
            if (ie->hotkeys[0] == ' ')
                ie->hotkeys[0] = ckey++;
            do_preselect(ie);

            add_entry( procfn? (*procfn)(ie) : ie );
        }
    }
}

void InvMenu::do_preselect(InvEntry *ie)
{
    if (!pre_select || pre_select->empty())
        return;

    for (int i = 0, size = pre_select->size(); i < size; ++i)
        if (ie->item && ie->item == (*pre_select)[i].item)
        {
            ie->selected_qty = (*pre_select)[i].quantity;
            break;
        }
}

std::vector<SelItem> InvMenu::get_selitems() const
{
    std::vector<SelItem> selected_items;
    for (int i = 0, count = sel.size(); i < count; ++i)
    {
        InvEntry *inv = dynamic_cast<InvEntry*>(sel[i]);
        selected_items.push_back(
                SelItem(
                    inv->hotkeys[0], 
                    inv->selected_qty,
                    inv->item ) );
    }
    return (selected_items);
}

bool InvMenu::process_key( int key )
{
    if (items.size() 
            && type == MT_DROP
            && (key == CONTROL('D') || key == '@'))
    {
        int newflag =
            is_set(MF_MULTISELECT)? 
                MF_SINGLESELECT | MF_ANYPRINTABLE 
              : MF_MULTISELECT;

        flags &= ~(MF_SINGLESELECT | MF_MULTISELECT | MF_ANYPRINTABLE);
        flags |= newflag;

        deselect_all();
        sel.clear();
        draw_select_count(0, true);
        return true;
    }
    return Menu::process_key( key );
}

unsigned char InvMenu::getkey() const
{
    unsigned char mkey = lastch;
    if (!isalnum(mkey) && mkey != '$' && mkey != '-' && mkey != '?' 
            && mkey != '*' && mkey != ESCAPE)
        mkey = ' ';
    return (mkey);
}

//////////////////////////////////////////////////////////////////////////////

bool in_inventory( const item_def &i )
{
    return i.x == -1 && i.y == -1;
}

unsigned char get_invent( int invent_type )
{
    unsigned char nothing = invent_select(NULL, MT_INVLIST, invent_type);
    redraw_screen();
    return (nothing);
}                               // end get_invent()

std::string item_class_name( int type, bool terse )
{
    if (terse)
    {
        switch (type)
        {
        case OBJ_GOLD:       return ("gold");
        case OBJ_WEAPONS:    return ("weapon");
        case OBJ_MISSILES:   return ("missile");
        case OBJ_ARMOUR:     return ("armour");
        case OBJ_WANDS:      return ("wand");
        case OBJ_FOOD:       return ("food");
        case OBJ_UNKNOWN_I:  return ("?");
        case OBJ_SCROLLS:    return ("scroll");
        case OBJ_JEWELLERY:  return ("jewelry");
        case OBJ_POTIONS:    return ("potion");
        case OBJ_UNKNOWN_II: return ("?");
        case OBJ_BOOKS:      return ("book");
        case OBJ_STAVES:     return ("staff");
        case OBJ_ORBS:       return ("orb");
        case OBJ_MISCELLANY: return ("misc");
        case OBJ_CORPSES:    return ("carrion");
        }
    }
    else
    {
        switch (type)
        {
        case OBJ_GOLD:       return ("Gold");
        case OBJ_WEAPONS:    return ("Hand Weapons");
        case OBJ_MISSILES:   return ("Missiles");
        case OBJ_ARMOUR:     return ("Armour");
        case OBJ_WANDS:      return ("Magical Devices");
        case OBJ_FOOD:       return ("Comestibles");
        case OBJ_UNKNOWN_I:  return ("Books");
        case OBJ_SCROLLS:    return ("Scrolls");
        case OBJ_JEWELLERY:  return ("Jewellery");
        case OBJ_POTIONS:    return ("Potions");
        case OBJ_UNKNOWN_II: return ("Gems");
        case OBJ_BOOKS:      return ("Books");
        case OBJ_STAVES:     return ("Magical Staves and Rods");
        case OBJ_ORBS:       return ("Orbs of Power");
        case OBJ_MISCELLANY: return ("Miscellaneous");
        case OBJ_CORPSES:    return ("Carrion");
        }
    }
    return ("");
}

std::vector<SelItem> select_items( const std::vector<const item_def*> &items,
                                   const char *title, bool noselect )
{
    std::vector<SelItem> selected;
    if (!items.empty())
    {
        InvMenu menu;
        menu.set_title(title);
        menu.load_items(items);
        menu.set_flags(noselect ? MF_NOSELECT : MF_MULTISELECT);
        menu.show();
        selected = menu.get_selitems();
    }
    return (selected);
}

static bool item_class_selected(const item_def &i, int selector)
{
    const int itype = i.base_type;
    if (selector == OSEL_ANY || selector == itype)
        return (true);

    switch (selector)
    {
    case OBJ_MISSILES:
        return (itype == OBJ_MISSILES || itype == OBJ_WEAPONS);
    case OBJ_WEAPONS:
    case OSEL_WIELD:
        return (itype == OBJ_WEAPONS || itype == OBJ_STAVES 
                || itype == OBJ_MISCELLANY);
    case OBJ_SCROLLS:
        return (itype == OBJ_SCROLLS || itype == OBJ_BOOKS);
    default:
        return (false);
    }
}

static bool userdef_item_selected(const item_def &i, int selector)
{
#if defined(CLUA_BINDINGS)
    const char *luafn = selector == OSEL_WIELD? "ch_item_wieldable" :
                                                NULL;
    return (luafn && clua.callbooleanfn(false, luafn, "u", &i));
#else
    return (false);
#endif
}

static bool is_item_selected(const item_def &i, int selector)
{
    return item_class_selected(i, selector)
        || userdef_item_selected(i, selector);
}

static void get_inv_items_to_show(std::vector<const item_def*> &v, int selector)
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item(you.inv[i]) && is_item_selected(you.inv[i], selector))
            v.push_back( &you.inv[i] );
    }
}

unsigned char invent_select( 
                      const char *title,
                      menu_type type,
                      int item_selector,
                      int flags,
                      invtitle_annotator titlefn,
                      std::vector<SelItem> *items,
                      std::vector<text_pattern> *filter,
                      Menu::selitem_tfn selitemfn,
                      const std::vector<SelItem> *pre_select )
{
    InvMenu menu(flags);
    
    menu.set_preselect(pre_select);
    menu.set_title_annotator(titlefn);
    menu.f_selitem = selitemfn;
    if (filter)
        menu.set_select_filter( *filter );
    menu.load_inv_items(item_selector);
    menu.set_type(type);

    // Don't override title if there are no items.
    if (title && menu.item_count())
        menu.set_title(title);

    menu.show(true);

    if (items)
        *items = menu.get_selitems();

    return (menu.getkey());
}

unsigned char invent( int item_class_inv, bool show_price )
{
    InvShowPrices show_item_prices(show_price);
    return (invent_select(NULL, MT_INVLIST, item_class_inv));
}                               // end invent()

// Reads in digits for a count and apprends then to val, the
// return value is the character that stopped the reading.
static unsigned char get_invent_quant( unsigned char keyin, int &quant )
{
    quant = keyin - '0';

    for(;;)
    {
        keyin = get_ch();

        if (!isdigit( keyin ))
            break;

        quant *= 10;
        quant += (keyin - '0');

        if (quant > 9999999)
        {
            quant = 9999999;
            keyin = '\0';
            break;
        }
    }

    return (keyin);
}

// This function prompts the user for an item, handles the '?' and '*'
// listings, and returns the inventory slot to the caller (which if
// must_exist is true (the default) will be an assigned item, with
// a positive quantity.
//
// It returns PROMPT_ABORT       if the player hits escape.
// It returns PROMPT_GOT_SPECIAL if the player hits the "other_valid_char".
//
// Note: This function never checks if the item is appropriate.
std::vector<SelItem> prompt_invent_items(
                        const char *prompt,
                        menu_type mtype,
                        int type_expect,
                        invtitle_annotator titlefn,
                        bool allow_auto_list,
                        bool allow_easy_quit,
                        const char other_valid_char,
                        std::vector<text_pattern> *select_filter,
                        Menu::selitem_tfn fn,
                        const std::vector<SelItem> *pre_select )
{
    unsigned char  keyin = 0;
    int            ret = PROMPT_ABORT;

    bool           need_redraw = false;
    bool           need_prompt = true;
    bool           need_getch  = true;
    bool           auto_list   = Options.auto_list && allow_auto_list;

    if (auto_list)
    {
        need_prompt = need_getch = false;
        keyin       = '?';
    }

    std::vector<SelItem> items;
    int count = -1;
    for (;;)
    {
        if (need_redraw)
        {
            redraw_screen();
            mesclr( true );
        }

        if (need_prompt)
            mpr( prompt, MSGCH_PROMPT );

        if (need_getch)
            keyin = get_ch();

        need_redraw = false;
        need_prompt = true;
        need_getch  = true;

        // Note:  We handle any "special" character first, so that
        //        it can be used to override the others.
        if (other_valid_char != 0 && keyin == other_valid_char)
        {
            ret = PROMPT_GOT_SPECIAL;
            break;
        }
        else if (keyin == '?' || keyin == '*' || keyin == ',')
        {
            int selmode =
                Options.drop_mode == DM_SINGLE 
                    && (!pre_select || pre_select->empty())?
                        MF_SINGLESELECT | MF_EASY_EXIT | MF_ANYPRINTABLE :
                        MF_MULTISELECT;
            // The "view inventory listing" mode.
            int ch = invent_select(
                        prompt,
                        mtype,
                        keyin == '*'? OSEL_ANY : type_expect,
                        selmode,
                        titlefn, &items, select_filter, fn,
                        pre_select );

            if ((selmode & MF_SINGLESELECT) || ch == ESCAPE)
            {
                keyin       = ch;
                need_getch  = false;
            }
            else
            {
                keyin       = 0;
                need_getch  = true;
            }

            if (items.size())
            {
                redraw_screen();
                mesclr(true);

                for (unsigned int i = 0; i < items.size(); ++i)
                    items[i].slot = letter_to_index( items[i].slot );
                return (items);
            }

            need_redraw = !(keyin == '?' || keyin == '*' 
                            || keyin == ',' || keyin == '+');
            need_prompt = need_redraw;
        }
        else if (isdigit( keyin ))
        {
            // The "read in quantity" mode
            keyin = get_invent_quant( keyin, count );

            need_prompt = false;
            need_getch  = false;
        }
        else if (keyin == ESCAPE
                || (Options.easy_quit_item_prompts
                    && allow_easy_quit
                    && keyin == ' '))
        {
            ret = PROMPT_ABORT;
            break;
        }
        else if (isalpha( keyin )) 
        {
            ret = letter_to_index( keyin );

            if (!is_valid_item( you.inv[ret] ))
                mpr( "You do not have any such object." );
            else
                break;
        }
        else if (!isspace( keyin ))
        {
            // we've got a character we don't understand...
            canned_msg( MSG_HUH );
        }
        else
        {
            // We're going to loop back up, so don't draw another prompt.
            need_prompt = false;
        }
    }

    if (ret != PROMPT_ABORT)
        items.push_back(
            SelItem( ret, count,
                     ret != PROMPT_GOT_SPECIAL? &you.inv[ret] : NULL ) );
    return items;
}

static int digit_to_index( char digit, operation_types oper ) {
    
    int i;
    unsigned int j;
    char iletter = (char)(oper);

    for ( i = 0; i < ENDOFPACK; ++i ) {
        if (is_valid_item(you.inv[i])) {
	    const std::string& r(you.inv[i].inscription);
	    /* note that r.size() is unsigned */
	    for ( j = 0; j + 2 < r.size(); ++j ) {
		if ( r[j] == '@' &&
		     (r[j+1] == iletter || r[j+1] == '*') &&
		     r[j+2] == digit ) {
		    return i;
		}
	    }
	}
    }
    return -1;
}

static bool has_warning_inscription(const item_def& item,
                                    operation_types oper)
{
    char iletter = (char)(oper);
    unsigned int i;
    char name[ITEMNAME_SIZE];
    item_name(item, DESC_INVENTORY, name, false);
    
    const std::string& r(item.inscription);
    for ( i = 0; i + 1 < r.size(); ++i )
	if (r[i] == '!' && (r[i+1] == iletter || r[i+1] == '*'))
	    return true;
    return false;
}    

/* return true if user OK'd it (or no warning), false otherwise */ 
bool check_warning_inscriptions( const item_def& item,
                                 operation_types oper )
{
    char prompt[ITEMNAME_SIZE + 100];
    char name[ITEMNAME_SIZE];
    if ( has_warning_inscription(item, oper) )
    {
        snprintf(prompt, sizeof prompt, "Really choose %s?",
                 item_name(item, DESC_INVENTORY, name, false));
        return yesno(prompt, false, 'n');
    }
    else
        return true;
}

// This function prompts the user for an item, handles the '?' and '*'
// listings, and returns the inventory slot to the caller (which if
// must_exist is true (the default) will be an assigned item), with
// a positive quantity.
//
// It returns PROMPT_ABORT       if the player hits escape.
// It returns PROMPT_GOT_SPECIAL if the player hits the "other_valid_char".
//
// Note: This function never checks if the item is appropriate.
int prompt_invent_item( const char *prompt, 
                        menu_type mtype, int type_expect, 
                        bool must_exist, bool allow_auto_list,
                        bool allow_easy_quit,
                        const char other_valid_char,
                        int *const count,
                        operation_types oper )
{
    unsigned char  keyin = 0;
    int            ret = PROMPT_ABORT;

    bool           need_redraw = false;
    bool           need_prompt = true;
    bool           need_getch  = true;
    bool           auto_list   = Options.auto_list && allow_auto_list;

    if (auto_list)
    {
        need_prompt = need_getch = false;
        keyin       = '?';
    }

    for (;;)
    {
        if (need_redraw)
        {
            redraw_screen();
            mesclr( true );
        }

        if (need_prompt)
            mpr( prompt, MSGCH_PROMPT );

        if (need_getch)
            keyin = get_ch();

        need_redraw = false;
        need_prompt = true;
        need_getch  = true;

        // Note:  We handle any "special" character first, so that
        //        it can be used to override the others.
        if (other_valid_char != 0 && keyin == other_valid_char)
        {
            ret = PROMPT_GOT_SPECIAL;
            break;
        }
        else if (keyin == '?' || keyin == '*')
        {
            // The "view inventory listing" mode.
            std::vector< SelItem > items;
            keyin = invent_select(
                        prompt,
                        mtype,
                        keyin == '*'? OSEL_ANY : type_expect, 
                        MF_SINGLESELECT | MF_ANYPRINTABLE
                            | MF_EASY_EXIT, 
                        NULL, 
                        &items );


            need_getch  = false;

            // Don't redraw if we're just going to display another listing
            need_redraw = (keyin != '?' && keyin != '*');
            need_prompt = need_redraw;

            if (items.size())
            {
                if (count)
                    *count = items[0].quantity;
                redraw_screen();
                mesclr( true );
            }
        }
        else if (count != NULL && isdigit( keyin ))
        {
            // The "read in quantity" mode
            keyin = get_invent_quant( keyin, *count );

            need_prompt = false;
            need_getch  = false;
        }
        else if ( count == NULL && isdigit( keyin ) )
        {
            /* scan for our item */
            int res = digit_to_index( keyin, oper );
            if ( res != -1 ) {
                ret = res;
                if ( check_warning_inscriptions( you.inv[ret], oper ) )
                    break;
            }
        }
        else if (keyin == ESCAPE
                || (Options.easy_quit_item_prompts
                    && allow_easy_quit
                    && keyin == ' '))
        {
            ret = PROMPT_ABORT;
            break;
        }
        else if (isalpha( keyin )) 
        {
            ret = letter_to_index( keyin );

            if (must_exist && !is_valid_item( you.inv[ret] ))
                mpr( "You do not have any such object." );
            else {
                if ( check_warning_inscriptions( you.inv[ret], oper ) ) {
                    break;
                }
            }
        }
        else if (!isspace( keyin ))
        {
            // we've got a character we don't understand...
            canned_msg( MSG_HUH );
        }
        else
        {
            // We're going to loop back up, so don't draw another prompt.
            need_prompt = false;
        }
    }

    return (ret);
}
