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

const char *command_string( int i );
const char *wizard_string( int i );

struct InvTitle : public MenuEntry
{
    Menu *m;
    std::string (*titlefn)( int menuflags, const std::string &oldt );
    
    InvTitle( Menu *mn, const char *title,
              std::string (*tfn)( int menuflags, const std::string &oldt ) ) 
        : MenuEntry( title )
    {
        m       = mn;
        titlefn = tfn;
    }

    std::string get_text() const
    {
        return titlefn? titlefn( m->get_flags(), MenuEntry::get_text() ) :
                        MenuEntry::get_text();
    }
};

class InvShowPrices;
class InvEntry : public MenuEntry
{
private:
    static bool show_prices;
    static char temp_id[4][50];
    static void set_show_prices(bool doshow);

    friend class InvShowPrices;
public:
    const item_def *item;

    InvEntry( const item_def &i ) : MenuEntry( "", MEL_ITEM ), item( &i )
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

        if (i.base_type != OBJ_GOLD)
        {
            if (in_inventory(i))
            {
                text = text.substr( 4 );        // Skip the inventory letter.
                add_hotkey(index_to_letter( i.link ));
            }
            else
                add_hotkey(' ');                // Dummy hotkey
        }
        else
        {
            // Dummy hotkey for gold.
            add_hotkey(' ');
        }
        add_class_hotkeys(i);

        quantity = i.quantity;
    }

    std::string get_text() const
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
                "%c %c %s%s",
                hotkeys[0],
                (!selected_qty? '-' : selected_qty < quantity? '#' : '+'),
                text.c_str(),
                suffix );
        return (buf);
    }
private:
    void add_class_hotkeys(const item_def &i)
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
};

bool InvEntry::show_prices = false;
char InvEntry::temp_id[4][50];

void InvEntry::set_show_prices(bool doshow)
{
    if ((show_prices = doshow))
    {
        memset(temp_id, 1, sizeof temp_id);
    }
}

class InvShowPrices {
public:
    InvShowPrices(bool doshow = true)
    {
        InvEntry::set_show_prices(doshow);
    }
    ~InvShowPrices()
    {
        InvEntry::set_show_prices(false);
    }
};

class InvMenu : public Menu
{
public:
    InvMenu(int mflags = MF_MULTISELECT) : Menu(mflags) { }
protected:
    bool process_key(int key);
};

bool InvMenu::process_key( int key )
{
    if (items.size() && (key == CONTROL('D') || key == '@'))
    {
        int newflag = 
            is_set(MF_MULTISELECT)? 
                MF_SINGLESELECT | MF_EASY_EXIT | MF_ANYPRINTABLE 
              : MF_MULTISELECT;

        flags &= ~(MF_SINGLESELECT | MF_MULTISELECT | 
                   MF_EASY_EXIT | MF_ANYPRINTABLE);
        flags |= newflag;

        deselect_all();
        sel->clear();
        draw_select_count(0);
        return true;
    }
    return Menu::process_key( key );
}

bool in_inventory( const item_def &i )
{
    return i.x == -1 && i.y == -1;
}

unsigned char get_invent( int invent_type )
{
    unsigned char nothing = invent_select( invent_type );

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
        case OBJ_UNKNOWN_I:  return ("book");
        case OBJ_SCROLLS:    return ("scroll");
        case OBJ_JEWELLERY:  return ("jewelry");
        case OBJ_POTIONS:    return ("potion");
        case OBJ_UNKNOWN_II: return ("gem");
        case OBJ_BOOKS:      return ("book");
        case OBJ_STAVES:     return ("stave");
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

class MenuEntryPtrComparer {
public:
    bool operator() (const MenuEntry* a, const MenuEntry* b ) const {
	return *a < *b;
    }
};

void populate_item_menu( Menu *menu, const std::vector<item_def> &items,
                         void (*callback)(MenuEntry *me) )
{
    FixedVector< int, NUM_OBJECT_CLASSES >  inv_class;
    for (int i = 0; i < NUM_OBJECT_CLASSES; ++i)
        inv_class[i] = 0;
    for (int i = 0, count = items.size(); i < count; ++i)
        inv_class[ items[i].base_type ]++;

    menu_letter ckey;
    std::vector<InvEntry*> items_in_class;

    for (int i = 0; i < NUM_OBJECT_CLASSES; ++i)
    {
        if (!inv_class[i]) continue;

        menu->add_entry( new MenuEntry( item_class_name(i), MEL_SUBTITLE ) );

	items_in_class.clear();

        for (int j = 0, count = items.size(); j < count; ++j)
        {
            if (items[j].base_type != i) continue;
	    items_in_class.push_back( new InvEntry(items[j]) );
	}

	std::sort( items_in_class.begin(), items_in_class.end(),
		   MenuEntryPtrComparer() );

        for (unsigned int j = 0; j < items_in_class.size(); ++j)
	{
            InvEntry *ie = items_in_class[j];
            ie->hotkeys[0] = ckey++;
	    callback(ie);

            menu->add_entry( ie );

        }

    }
}

std::vector<SelItem> select_items( std::vector<item_def*> &items, 
                                   const char *title, bool noselect )
{
    std::vector<SelItem> selected;

    if (items.empty())
        return selected;

    FixedVector< int, NUM_OBJECT_CLASSES >  inv_class;
    for (int i = 0; i < NUM_OBJECT_CLASSES; ++i)
        inv_class[i] = 0;
    for (int i = 0, count = items.size(); i < count; ++i)
        inv_class[ items[i]->base_type ]++;

    Menu menu;
    menu.set_title( new MenuEntry(title) );

    char ckey = 'a';

    std::vector<InvEntry*> items_in_class;

    for (int i = 0; i < NUM_OBJECT_CLASSES; ++i)
    {
        if (!inv_class[i]) continue;

        menu.add_entry( new MenuEntry( item_class_name(i), MEL_SUBTITLE ) );
	
	items_in_class.clear();

        for (int j = 0, count = items.size(); j < count; ++j)
        {
            if (items[j]->base_type != i) continue;
	    items_in_class.push_back( new InvEntry( *items[j]) );
	}
	/* sort the items inside a class */
	std::sort( items_in_class.begin(), items_in_class.end(),
		   MenuEntryPtrComparer() );

        for (unsigned int j = 0; j < items_in_class.size(); ++j)
	{
            InvEntry *ie = items_in_class[j];
            ie->hotkeys[0] = ckey;

            menu.add_entry( ie );

            ckey = ckey == 'z'? 'A' :
                   ckey == 'Z'? 'a' :
                                ckey + 1;
        }
    }
    menu.set_flags( noselect ? MF_NOSELECT :
		    (MF_MULTISELECT | MF_SELECT_ANY_PAGE) );
    std::vector< MenuEntry * > sel = menu.show();
    for (int i = 0, count = sel.size(); i < count; ++i)
    {
        InvEntry *inv = (InvEntry *) sel[i];
        selected.push_back( SelItem( inv->hotkeys[0], 
                                     inv->selected_qty,
                                     inv->item ) );
    }

    return selected;
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

static void get_inv_items_to_show(std::vector<item_def*> &v, int selector)
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item(you.inv[i]) && is_item_selected(you.inv[i], selector))
            v.push_back( &you.inv[i] );
    }
}

static void set_vectitem_classes(const std::vector<item_def*> &v,
                                 FixedVector<int, NUM_OBJECT_CLASSES> &fv)
{
    for (int i = 0; i < NUM_OBJECT_CLASSES; i++)
        fv[i] = 0;
    
    for (int i = 0, size = v.size(); i < size; i++)
        fv[ v[i]->base_type ]++;
}

unsigned char invent_select( 
                      int item_selector,
                      int flags,
                      std::string (*titlefn)( int menuflags, 
                                              const std::string &oldt ),
                      std::vector<SelItem> *items,
                      std::vector<text_pattern> *filter,
                      Menu::selitem_tfn selitemfn )
{
    InvMenu menu;
    
    menu.selitem_text = selitemfn;
    if (filter)
        menu.set_select_filter( *filter );

    FixedVector< int, NUM_OBJECT_CLASSES >  inv_class2;
    for (int i = 0; i < NUM_OBJECT_CLASSES; i++)
        inv_class2[i] = 0;

    int inv_count = 0;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].quantity)
        {
            inv_class2[ you.inv[i].base_type ]++;
            inv_count++;
        }
    }

    if (!inv_count)
    {
        menu.set_title( new MenuEntry( "You aren't carrying anything." ) );
        menu.show();
        return 0;
    }

    std::vector<item_def*> tobeshown;
    get_inv_items_to_show( tobeshown, item_selector );
    set_vectitem_classes( tobeshown, inv_class2 );

    if (tobeshown.size())
    {
        const int cap = carrying_capacity();

        char title_buf[200];
        snprintf( title_buf, sizeof title_buf,
                  "Inventory: %d.%d/%d.%d aum (%d%%, %d/52 slots)",
                    you.burden / 10, you.burden % 10, 
                    cap / 10, cap % 10,
                    (you.burden * 100) / cap, 
                    inv_count );
        menu.set_title( new InvTitle( &menu, title_buf, titlefn ) );

        for (int i = 0; i < 15; i++)
        {
            if (inv_class2[i] != 0)
            {
                std::string s = item_class_name(i);
                menu.add_entry( new MenuEntry( s, MEL_SUBTITLE ) );

                for (int j = 0, size = tobeshown.size(); j < size; ++j)
                {
                    if (tobeshown[j]->base_type == i)
                    {
                        menu.add_entry( new InvEntry( *tobeshown[j] ) );
                    }
                }               // end of j loop
            }                   // end of if inv_class2
        }                       // end of i loop.
    }
    else
    {
        std::string s;
        if (item_selector == -1)
            s = "You aren't carrying anything.";
        else
        {
            if (item_selector == OBJ_WEAPONS || item_selector == OSEL_WIELD)
                s = "You aren't carrying any weapons.";
            else if (item_selector == OBJ_MISSILES)
                s = "You aren't carrying any ammunition.";
            else
                s = "You aren't carrying any such object.";
        }
        menu.set_title( new MenuEntry( s ) );
    }

    menu.set_flags( flags );
    std::vector< MenuEntry * > sel = menu.show();
    if (items)
    {
        for (int i = 0, count = sel.size(); i < count; ++i)
            items->push_back( SelItem( sel[i]->hotkeys[0], 
                                       sel[i]->selected_qty ) );
    }

    unsigned char mkey = menu.getkey();
    if (!isalnum(mkey) && mkey != '$' && mkey != '-' && mkey != '?' 
            && mkey != '*')
        mkey = ' ';
    return mkey;
}

unsigned char invent( int item_class_inv, bool show_price )
{
    InvShowPrices show_item_prices(show_price);
    return (invent_select(item_class_inv));
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
                        int type_expect,
                        std::string (*titlefn)( int menuflags, 
                                                const std::string &oldt ),
                        bool allow_auto_list,
                        bool allow_easy_quit,
                        const char other_valid_char,
                        std::vector<text_pattern> *select_filter,
                        Menu::selitem_tfn fn )
{
    unsigned char  keyin = 0;
    int            ret = -1;

    bool           need_redraw = false;
    bool           need_prompt = true;
    bool           need_getch  = true;

    if (Options.auto_list && allow_auto_list)
    {
        need_getch  = false;
        need_redraw = false;
        need_prompt = false;
        keyin       = '?';
    }

    std::vector< SelItem > items;
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
        if (other_valid_char != '\0' && keyin == other_valid_char)
        {
            ret = PROMPT_GOT_SPECIAL;
            break;
        }
        else if (keyin == '?' || keyin == '*' || keyin == ',')
        {
            int selmode = Options.drop_mode == DM_SINGLE?
                MF_SINGLESELECT | MF_EASY_EXIT | MF_ANYPRINTABLE :
                MF_MULTISELECT;
            // The "view inventory listing" mode.
            int ch = invent_select(
                    keyin == '*'? -1 : type_expect,
                    selmode | MF_SELECT_ANY_PAGE,
                    titlefn, &items, select_filter, fn );

            if (selmode & MF_SINGLESELECT)
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
                mesclr( true );

                for (unsigned int i = 0; i < items.size(); ++i)
                    items[i].slot = letter_to_index( items[i].slot );
                return items;
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
    }

    if (ret != PROMPT_ABORT)
        items.push_back( SelItem( ret, count ) );
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

/* return true if user OK'd it (or no warning), false otherwise */ 
static bool check_warning_inscriptions( const item_def& item,
					operation_types oper )
{
    unsigned int i;
    char iletter = (char)(oper);
    char name[ITEMNAME_SIZE];
    char prompt[ITEMNAME_SIZE + 100];
    item_name(item, DESC_INVENTORY, name, false);
    strcpy( prompt, "Really choose ");
    strncat( prompt, name, ITEMNAME_SIZE );
    strcat( prompt, "?");
    
    const std::string& r(item.inscription);
    for ( i = 0; i + 1 < r.size(); ++i ) {
	if ( r[i] == '!' &&
	     (r[i+1] == iletter || r[i+1] == '*') ) {
	    
	    return yesno(prompt, false, 'n');
	}
    }
    return true;
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
int prompt_invent_item( const char *prompt, int type_expect, 
                        bool must_exist, bool allow_auto_list,
                        bool allow_easy_quit,
                        const char other_valid_char,
                        int *const count,
			operation_types oper )
{
    unsigned char  keyin = 0;
    int            ret = -1;

    bool           need_redraw = false;
    bool           need_prompt = true;
    bool           need_getch  = true;

    if (Options.auto_list && allow_auto_list)
    {
        std::vector< SelItem > items;

        // pretend the player has hit '?' and setup state.
        keyin = invent_select( type_expect, 
                                MF_SINGLESELECT | MF_ANYPRINTABLE
                                    | MF_EASY_EXIT,
                                NULL, &items );

        if (items.size())
        {
            if (count)
                *count = items[0].quantity;
            redraw_screen();
            mesclr( true );
            return letter_to_index( keyin );
        }

        need_getch = false;

        // Don't redraw if we're just going to display another listing
        need_redraw = (keyin != '?' && keyin != '*');

        // A prompt is nice for when we're moving to "count" mode.
        need_prompt = (count != NULL && isdigit( keyin ));
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
        if (other_valid_char != '\0' && keyin == other_valid_char)
        {
            ret = PROMPT_GOT_SPECIAL;
            break;
        }
        else if (keyin == '?' || keyin == '*')
        {
            // The "view inventory listing" mode.
            std::vector< SelItem > items;
            keyin = invent_select( keyin == '*'? OSEL_ANY : type_expect, 
                                MF_SINGLESELECT | MF_ANYPRINTABLE
                                    | MF_EASY_EXIT, 
                                NULL, 
                                &items );

            if (items.size())
            {
                if (count)
                    *count = items[0].quantity;
                redraw_screen();
                mesclr( true );
                return letter_to_index( keyin );
            }

            need_getch  = false;

            // Don't redraw if we're just going to display another listing
            need_redraw = (keyin != '?' && keyin != '*');

            // A prompt is nice for when we're moving to "count" mode.
            need_prompt = (count != NULL && isdigit( keyin ));
        }
        else if (count != NULL && isdigit( keyin ))
        {
            // The "read in quantity" mode
            keyin = get_invent_quant( keyin, *count );

            need_prompt = false;
            need_getch  = false;
        }
	/*** HP CHANGE ***/
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
    }

    return (ret);
}

void list_commands(bool wizard)
{
    const char *line;
    int j = 0;

#ifdef DOS_TERM
    char buffer[4800];

    window(1, 1, 80, 25);
    gettext(1, 1, 80, 25, buffer);
#endif

    clrscr();

    // BCR - Set to screen length - 1 to display the "more" string
    int moreLength = (get_number_of_lines() - 1) * 2;

    for (int i = 0; i < 500; i++)
    {
        if (wizard)
            line = wizard_string( i );
        else
            line = command_string( i );

        if (strlen( line ) != 0)
        {
            // BCR - If we've reached the end of the screen, clear
            if (j == moreLength)
            {
                gotoxy(2, j / 2 + 1);
                cprintf("More...");
                getch();
                clrscr();
                j = 0;
            }

            gotoxy( ((j % 2) ? 40 : 2), ((j / 2) + 1) );
            cprintf( line );

            j++;
        }
    }

    getch();

#ifdef DOS_TERM
    puttext(1, 1, 80, 25, buffer);
#endif

    return;
}                               // end list_commands()

const char *wizard_string( int i )
{
    UNUSED( i );

#ifdef WIZARD
    return((i ==  10) ? "a    : acquirement"                  :
           (i ==  13) ? "A    : set all skills to level"      :
           (i ==  15) ? "b    : controlled blink"             :
           (i ==  20) ? "B    : banish yourself to the Abyss" :
           (i ==  30) ? "g    : add a skill"                  :
           (i ==  35) ? "G    : remove all monsters"          :
           (i ==  40) ? "h/H  : heal yourself (super-Heal)"   :
           (i ==  50) ? "i/I  : identify/unidentify inventory":
           (i ==  70) ? "l    : make entrance to labyrinth"   :
           (i ==  80) ? "m/M  : create monster by number/name":
           (i ==  90) ? "o/%%  : create an object"            :
           (i == 100) ? "p    : make entrance to pandemonium" :
           (i == 110) ? "x    : gain an experience level"     :
           (i == 115) ? "r    : change character's species"   :
           (i == 120) ? "s    : gain 20000 skill points"      :
           (i == 130) ? "S    : set skill to level"           :
           (i == 140) ? "t    : tweak object properties"      :
           (i == 150) ? "X    : Receive a gift from Xom"      :
           (i == 160) ? "z/Z  : cast any spell by number/name":
           (i == 200) ? "$    : get 1000 gold"                :
           (i == 210) ? "</>  : create up/down staircase"     :
           (i == 220) ? "u/d  : shift up/down one level"      :
           (i == 230) ? "~/\"  : goto a level"                :
           (i == 240) ? "(    : create a feature"             :
           (i == 250) ? "]    : get a mutation"               :
           (i == 260) ? "[    : get a demonspawn mutation"    :
           (i == 270) ? ":    : find branch"                  :
           (i == 280) ? "{    : magic mapping"                :
           (i == 290) ? "^    : gain piety"                   :
           (i == 300) ? "_    : gain religion"                :
           (i == 310) ? "\'    : list items"                  :
           (i == 320) ? "?    : list wizard commands"         :
           (i == 330) ? "|    : acquire all unrand artefacts" :
           (i == 340) ? "+    : turn item into random artefact" :
           (i == 350) ? "=    : sum skill points"
                      : "");

#else
    return ("");
#endif
}                               // end wizard_string()

const char *command_string( int i )
{
    /*
     * BCR - Command printing, case statement
     * Note: The numbers in this case indicate the order in which the
     *       commands will be printed out.  Make sure none of these
     *       numbers is greater than 500, because that is the limit.
     *
     * Arranged alpha lower, alpha upper, punctuation, ctrl.
     *
     */

    return((i ==  10) ? "a    : use special ability"              :
           (i ==  20) ? "d(#) : drop (exact quantity of) items"   :
           (i ==  30) ? "e    : eat food"                         :
           (i ==  40) ? "f    : fire first available missile"     :
           (i ==  50) ? "i    : inventory listing"                :
           (i ==  55) ? "m    : check skills"                     :
           (i ==  60) ? "o/c  : open / close a door"              :
           (i ==  65) ? "p    : pray"                             :
           (i ==  70) ? "q    : quaff a potion"                   :
           (i ==  80) ? "r    : read a scroll or book"            :
           (i ==  90) ? "s    : search adjacent tiles"            :
           (i == 100) ? "t    : throw/shoot an item"              :
           (i == 110) ? "v    : view item description"            :
           (i == 120) ? "w    : wield an item"                    :
           (i == 130) ? "x    : examine visible surroundings"     :
           (i == 135) ? "z    : zap a wand"                       :
           (i == 140) ? "A    : list abilities/mutations"         :
           (i == 141) ? "C    : check experience"                 :
           (i == 142) ? "D    : dissect a corpse"                 :
           (i == 145) ? "E    : evoke power of wielded item"      :
           (i == 150) ? "M    : memorise a spell"                 :
           (i == 155) ? "O    : overview of the dungeon"          :
           (i == 160) ? "P/R  : put on / remove jewellery"        :
           (i == 165) ? "Q    : quit without saving"              :
           (i == 168) ? "S    : save game and exit"               :
           (i == 179) ? "V    : version information"              :
           (i == 200) ? "W/T  : wear / take off armour"           :
           (i == 210) ? "X    : examine level map"                :
           (i == 220) ? "Z    : cast a spell"                     :
           (i == 240) ? ",/g  : pick up items"                    :
           (i == 242) ? "./del: rest one turn"                    :
           (i == 250) ? "</>  : ascend / descend a staircase"     :
           (i == 270) ? ";    : examine occupied tile"            :
           (i == 280) ? "\\    : check item knowledge"            :
#ifdef WIZARD
           (i == 290) ? "&    : invoke your Wizardly powers"      :
#endif
           (i == 300) ? "+/-  : scroll up/down [level map only]"  :
           (i == 310) ? "!    : shout or command allies"          :
           (i == 325) ? "^    : describe religion"                :
           (i == 337) ? "@    : status"                           :
           (i == 340) ? "#    : dump character to file"           :
           (i == 350) ? "=    : reassign inventory/spell letters" :
           (i == 360) ? "\'    : wield item a, or switch to b"    :
	   (i == 370) ? ":    : make a note"                      :
#ifdef USE_MACROS
           (i == 380) ? "`    : add macro"                        :
           (i == 390) ? "~    : save macros"                      :
#endif
           (i == 400) ? "]    : display worn armour"              :
           (i == 410) ? "\"    : display worn jewellery"          :
	   (i == 415) ? "{    : inscribe an item"                 :
           (i == 420) ? "Ctrl-P : see old messages"               :
#ifdef PLAIN_TERM
           (i == 430) ? "Ctrl-R : Redraw screen"                  :
#endif
           (i == 440) ? "Ctrl-A : toggle autopickup"              :
	   (i == 445) ? "Ctrl-M : toggle autoprayer"              :
	   (i == 447) ? "Ctrl-T : toggle fizzle"                  :
           (i == 450) ? "Ctrl-X : Save game without query"        :

#ifdef ALLOW_DESTROY_ITEM_COMMAND
           (i == 451) ? "Ctrl-D : Destroy inventory item"         :
#endif
           (i == 453) ? "Ctrl-G : interlevel travel"              :
           (i == 455) ? "Ctrl-O : explore"                        :

#ifdef STASH_TRACKING
           (i == 456) ? "Ctrl-S : mark stash"                     :
           (i == 457) ? "Ctrl-E : forget stash"                   :
           (i == 458) ? "Ctrl-F : search stashes"                 :
#endif

           (i == 460) ? "Shift & DIR : long walk"                 :
           (i == 465) ? "/ DIR : long walk"                       :
           (i == 470) ? "Ctrl  & DIR : door; untrap; attack"      :
           (i == 475) ? "* DIR : door; untrap; attack"            :
           (i == 478) ? "Shift & 5 on keypad : rest 100 turns"
                      : "");
}                               // end command_string()
