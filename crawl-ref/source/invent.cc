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
#include <sstream>
#include <iomanip>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "clua.h"
#include "describe.h"
#include "food.h"
#include "initfile.h"
#include "itemprop.h"
#include "items.h"
#include "macro.h"
#include "message.h"
#include "player.h"
#include "shopping.h"
#include "stuff.h"
#include "view.h"
#include "menu.h"
#include "randart.h"

#include "tiles.h"

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

    if ( in_inventory(i) && i.base_type != OBJ_GOLD )
    {
        // We need to do this in order to get the 'wielded' annotation.
        // We then toss out the first four characters, which look
        // like this: "a - ". Ow. FIXME.
        text = i.name(DESC_INVENTORY_EQUIP, false).substr(4);
    }
    else
        text = i.name(DESC_NOCAP_A, false);

    if (i.base_type != OBJ_GOLD && in_inventory(i))
        add_hotkey(index_to_letter( i.link ));
    else
        add_hotkey(' ');        // dummy hotkey

    add_class_hotkeys(i);

    quantity = i.quantity;
}

const std::string &InvEntry::get_basename() const
{
    if (basename.empty())
        basename = item->name(DESC_BASENAME);
    return (basename);
}

const std::string &InvEntry::get_qualname() const
{
    if (qualname.empty())
        qualname = item->name(DESC_QUALNAME);
    return (qualname);
}

const std::string &InvEntry::get_fullname() const
{
    return (text);
}

const bool InvEntry::is_item_cursed() const
{
    return (item_ident(*item, ISFLAG_KNOW_CURSE) && item_cursed(*item));
}

const bool InvEntry::is_item_glowing() const
{
    return (!item_ident(*item, ISFLAG_KNOW_TYPE)
            && (get_equip_desc(*item)
                || (is_artefact(*item)
                    && (item->base_type == OBJ_WEAPONS
                        || item->base_type == OBJ_MISSILES
                        || item->base_type == OBJ_ARMOUR))));
}

const bool InvEntry::is_item_ego() const
{
    return (item_ident(*item, ISFLAG_KNOW_TYPE) && !is_artefact(*item)
            && item->special != 0
            && (item->base_type == OBJ_WEAPONS
                || item->base_type == OBJ_MISSILES
                || item->base_type == OBJ_ARMOUR));
}

const bool InvEntry::is_item_art() const
{
    return (item_ident(*item, ISFLAG_KNOW_TYPE) && is_artefact(*item));
}

const bool InvEntry::is_item_equipped() const
{
    if (item->link == -1 || item->x != -1 || item->y != -1)
        return(false);

    for (int i = 0; i < NUM_EQUIP; i++)
        if (item->link == you.equip[i])
            return (true);

    return (false);
}

// returns values < 0 for edible chunks (non-rotten except for Saprovores),
// 0 for non-chunks, and values > 0 for rotten chunks for non-Saprovores
const int InvEntry::item_freshness() const
{
    if (item->base_type != OBJ_FOOD || item->sub_type != FOOD_CHUNK)
        return 0;

    int freshness = item->special;

    if (freshness >= 100 || you.mutation[MUT_SAPROVOROUS])
        freshness -= 300;

    // Ensure that chunk freshness is never zero, since zero means
    // that the item isn't a chunk.
    if (freshness >= 0) // possibly rotten chunks
        freshness++;

    return freshness;
}

std::string InvEntry::get_text() const
{
    std::ostringstream tstr;

    tstr << ' ' << static_cast<char>(hotkeys[0]) << ' ';
    if ( !selected_qty )
        tstr << '-';
    else if ( selected_qty < quantity )
        tstr << '#';
    else
        tstr << '+';
    tstr << ' ' << text;
    if (InvEntry::show_prices)
    {
        const int value = item_value(*item, show_prices);
        if (value > 0)
            tstr << " (" << value << " gold)";
    }

    if ( Options.show_inventory_weights )
    {
        const int mass = item_mass(*item) * item->quantity;
        tstr << std::setw(get_number_of_cols() - tstr.str().length() - 1)
             << std::right
             << make_stringf("(%.1f aum)", BURDEN_TO_AUM * mass);
    }
    return tstr.str();
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

void InvEntry::set_show_prices(bool doshow)
{
    show_prices = doshow;
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
        std::ostringstream default_title;
        const int cap = carrying_capacity(BS_UNENCUMBERED);

        default_title << make_stringf(
            "Inventory: %.0f/%.0f aum (%d%%, %d/52 slots)",
            BURDEN_TO_AUM * you.burden,
            BURDEN_TO_AUM * cap,
            (you.burden * 100) / cap, 
            inv_count() );

        default_title << std::setw(get_number_of_cols() - default_title.str().length() - 1)
                      << std::right
                      << "Press item letter to examine.";

        stitle = default_title.str();
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
        case OSEL_UNIDENT:
            s = "You don't have any unidentified items.";
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

void InvMenu::draw_stock_item(int index, const MenuEntry *me) const
{
#ifdef USE_TILE
    const InvEntry *ie = dynamic_cast<const InvEntry*>(me);
    if (ie && me->quantity > 0)
    {
        int draw_quantity = ie->quantity > 1 ? ie->quantity : -1;
        bool is_item_on_floor = true;
        bool is_item_tried = false;
        char c = 0;
        int idx = -1;
        if (ie->item)
        {
            is_item_on_floor = ie->item->x != -1;
            is_item_tried = item_type_tried(*ie->item);
            c = ie->hotkeys.size() > 0 ? ie->hotkeys[0] : 0;
            idx = (is_item_on_floor ? ie->item->index() :
                letter_to_index(c));
        }

        TileDrawOneItem(REGION_INV2, get_entry_index(ie), c,
            idx, tileidx_item(*ie->item), draw_quantity,
            is_item_on_floor, ie->selected(), ie->is_item_equipped(),
            is_item_tried, ie->is_item_cursed());
    }
#endif

    Menu::draw_stock_item(index, me);
}

template <std::string (*proc)(const InvEntry *a)>
int compare_item_str(const InvEntry *a, const InvEntry *b)
{
    return (proc(a).compare(proc(b)));
}

template <typename T, T (*proc)(const InvEntry *a)>
int compare_item(const InvEntry *a, const InvEntry *b)
{
    return (int(proc(a)) - int(proc(b)));
}

// C++ needs anonymous subs already!
std::string sort_item_basename(const InvEntry *a)
{
    return a->get_basename();
}
std::string sort_item_qualname(const InvEntry *a)
{
    return a->get_qualname();
}
std::string sort_item_fullname(const InvEntry *a)
{
    return a->get_fullname();
}
int sort_item_qty(const InvEntry *a)
{
    return a->quantity;
}
int sort_item_slot(const InvEntry *a)
{
    return a->item->link;
}

int sort_item_freshness(const InvEntry *a)
{
    return a->item_freshness();
}

bool sort_item_curse(const InvEntry *a)
{
    return a->is_item_cursed();
}

bool sort_item_glowing(const InvEntry *a)
{
    return !a->is_item_glowing();
}

bool sort_item_ego(const InvEntry *a)
{
    return !a->is_item_ego();
}

bool sort_item_art(const InvEntry *a)
{
    return !a->is_item_art();
}

bool sort_item_equipped(const InvEntry *a)
{
    return !a->is_item_equipped();
}

static bool compare_invmenu_items(const InvEntry *a, const InvEntry *b,
                                  const item_sort_comparators *cmps)
{
    for (item_sort_comparators::const_iterator i = cmps->begin();
         i != cmps->end();
         ++i)
    {
        const int cmp = i->compare(a, b);
        if (cmp)
            return (cmp < 0);
    }
    return (false);
}

struct menu_entry_comparator
{
    const menu_sort_condition *cond;
    
    menu_entry_comparator(const menu_sort_condition *c)
        : cond(c)
    {
    }

    bool operator () (const MenuEntry* a, const MenuEntry* b) const
    {
        const InvEntry *ia = dynamic_cast<const InvEntry *>(a);
        const InvEntry *ib = dynamic_cast<const InvEntry *>(b);
        return compare_invmenu_items(ia, ib, &cond->cmp);
    }
};

void init_item_sort_comparators(item_sort_comparators &list,
                                const std::string &set)
{
    static struct
    {
        const std::string cname;
        item_sort_fn cmp;
    } cmp_map[]  =
      {
          { "basename",  compare_item_str<sort_item_basename> },
          { "qualname",  compare_item_str<sort_item_qualname> },
          { "fullname",  compare_item_str<sort_item_fullname> },
          { "curse",     compare_item<bool, sort_item_curse> },
          { "glowing",   compare_item<bool, sort_item_glowing> },
          { "ego",       compare_item<bool, sort_item_ego> },
          { "art",       compare_item<bool, sort_item_art> },
          { "equipped",  compare_item<bool, sort_item_equipped> },
          { "qty",       compare_item<int, sort_item_qty> },
          { "slot",      compare_item<int, sort_item_slot> },
          { "freshness", compare_item<int, sort_item_freshness> }
      };

    list.clear();
    std::vector<std::string> cmps = split_string(",", set);
    for (int i = 0, size = cmps.size(); i < size; ++i)
    {
        std::string s = cmps[i];
        if (s.empty())
            continue;
        
        const bool negated = s[0] == '>';
        if (s[0] == '<' || s[0] == '>')
            s = s.substr(1);

        for (unsigned ci = 0; ci < ARRAYSIZE(cmp_map); ++ci)
            if (cmp_map[ci].cname == s)
            {
                list.push_back( item_comparator( cmp_map[ci].cmp, negated ) );
                break;
            }
    }

    if (list.empty())
        list.push_back(
            item_comparator(compare_item_str<sort_item_fullname>));
}

const menu_sort_condition *InvMenu::find_menu_sort_condition() const
{
    for (int i = 0, size = Options.sort_menus.size(); i < size; ++i)
        if (Options.sort_menus[i].matches(type))
            return &Options.sort_menus[i];
    return (NULL);
}

void InvMenu::sort_menu(std::vector<InvEntry*> &invitems,
                        const menu_sort_condition *cond)
{
    if (!cond || cond->sort == -1 || (int) invitems.size() < cond->sort)
        return;
    
    std::sort( invitems.begin(), invitems.end(), menu_entry_comparator(cond) );
}

void InvMenu::load_items(const std::vector<const item_def*> &mitems,
                         MenuEntry *(*procfn)(MenuEntry *me))
{
    FixedVector< int, NUM_OBJECT_CLASSES > inv_class(0);
    for (int i = 0, count = mitems.size(); i < count; ++i)
        inv_class[ mitems[i]->base_type ]++;

    menu_letter ckey;
    std::vector<InvEntry*> items_in_class;

    const menu_sort_condition *cond = find_menu_sort_condition();

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

        sort_menu(items_in_class, cond);

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

    // Don't make a menu so tall that we recycle hotkeys on the same page.
    if (mitems.size() > 52)
        set_maxpagesize(52);
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
    unsigned char select;
    
    while (true) {
       select = invent_select(NULL, MT_INVLIST, invent_type,
                                         MF_SINGLESELECT);
       if ( isalpha(select) )
       {
           const int invidx = letter_to_index(select);
           if ( is_valid_item(you.inv[invidx]) )
               describe_item( you.inv[invidx], true );
       }
       else break;
    }
    redraw_screen();
    return select;
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
        case OBJ_JEWELLERY:  return ("jewellery");
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
                                   const char *title, bool noselect,
                                   menu_type mtype )
{
    std::vector<SelItem> selected;
    if (!items.empty())
    {
        InvMenu menu;
        menu.set_type(mtype);
        menu.set_title(title);
        menu.load_items(items);
        menu.set_flags(noselect ? MF_NOSELECT | MF_SHOW_PAGENUMBERS :
                       MF_MULTISELECT | MF_ALLOW_FILTER | MF_SHOW_PAGENUMBERS);
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
    case OSEL_UNIDENT:
        return !fully_identified(i);
    case OBJ_MISSILES:
        return (itype == OBJ_MISSILES || itype == OBJ_WEAPONS);
    case OBJ_WEAPONS:
    case OSEL_WIELD:
        return (itype == OBJ_WEAPONS || itype == OBJ_STAVES 
                || itype == OBJ_MISCELLANY);
    case OSEL_MEMORISE:
        return (itype == OBJ_BOOKS && i.sub_type != BOOK_MANUAL
                && (i.sub_type != BOOK_DESTRUCTION || !item_type_known(i)));
    case OBJ_SCROLLS:
        return (itype == OBJ_SCROLLS || itype == OBJ_BOOKS);
    case OSEL_RECHARGE:
        return (item_is_rechargable(i, true));
    case OSEL_ENCH_ARM:
        return (is_enchantable_armour(i, true));
    case OSEL_VAMP_EAT:
        return (itype == OBJ_CORPSES && i.sub_type == CORPSE_BODY
                && !food_is_rotten(i));
    case OSEL_EQUIP:
        for (int eq = 0; eq < NUM_EQUIP; eq++)
        {
             if (you.equip[eq] == i.link)
                 return (true);
        }
        // fall through
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
                        MF_MULTISELECT | MF_ALLOW_FILTER;
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
    
    const char iletter = static_cast<char>(oper);

    for ( int i = 0; i < ENDOFPACK; ++i ) {
        if (is_valid_item(you.inv[i])) {
            const std::string& r(you.inv[i].inscription);
            /* note that r.size() is unsigned */
            for ( unsigned int j = 0; j + 2 < r.size(); ++j ) {
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

bool has_warning_inscription(const item_def& item,
                             operation_types oper)
{
    const char iletter = static_cast<char>(oper);
    
    const std::string& r(item.inscription);
    for ( unsigned int i = 0; i + 1 < r.size(); ++i )
        if (r[i] == '!' && (r[i+1] == iletter || r[i+1] == '*'))
            return true;
    return false;
}

// checks if current item (to be removed) has a warning inscription
// and prompts the user for confirmation
static bool check_old_item_warning( const item_def& item,
                                    operation_types oper )
{
    item_def old_item;
    std::string prompt = "";
    if (oper == OPER_WIELD) // can we safely unwield old item?
    {
        if (you.equip[EQ_WEAPON] == -1)
            return (true);
            
        old_item = you.inv[you.equip[EQ_WEAPON]];
        if (!has_warning_inscription(old_item, OPER_WIELD))
            return (true);
            
        prompt += "Really unwield ";
    }
    else if (oper == OPER_WEAR) // can we safely take off old item?
    {
        if (item.base_type != OBJ_ARMOUR)
            return (true);

        equipment_type eq_slot = get_armour_slot(item);
        if (you.equip[eq_slot] == -1)
            return (true);

        old_item = you.inv[you.equip[eq_slot]];

        if (!has_warning_inscription(old_item, OPER_TAKEOFF))
            return (true);
            
        prompt += "Really take off ";
    }
    else if (oper == OPER_PUTON) // can we safely remove old item?
    {
        if (item.base_type != OBJ_JEWELLERY)
            return (true);
            
        if (jewellery_is_amulet(item))
        {
            if (you.equip[EQ_AMULET] == -1)
                return (true);

            old_item = you.inv[you.equip[EQ_AMULET]];
            if (!has_warning_inscription(old_item, OPER_TAKEOFF))
                return (true);
                
            prompt += "Really remove ";
        }
        else // rings handled in prompt_ring_to_remove
            return (true);
    }
    else // anything else doesn't have a counterpart
        return (true);

    // now ask
    prompt += old_item.name(DESC_INVENTORY);
    prompt += '?';
    return yesno(prompt.c_str(), false, 'n');
}

static std::string operation_verb(operation_types oper)
{
    switch (oper)
    {
    case OPER_WIELD:          return "wield";
    case OPER_QUAFF:          return "quaff";
    case OPER_DROP:           return "drop";
    case OPER_EAT:            return (you.species == SP_VAMPIRE ?
                                      "drain" : "eat");
    case OPER_TAKEOFF:        return "take off";
    case OPER_WEAR:           return "wear";
    case OPER_PUTON:          return "put on";
    case OPER_REMOVE:         return "remove";
    case OPER_READ:           return "read";
    case OPER_MEMORISE:       return "memorise from";
    case OPER_ZAP:            return "zap";
    case OPER_THROW:          return "throw";
    case OPER_EXAMINE:        return "examine";
    case OPER_FIRE:           return "fire";
    case OPER_PRAY:           return "sacrifice";
    case OPER_EVOKE:          return "evoke";
    case OPER_ANY:
    default:
        return "choose";
    }
}

/* return true if user OK'd it (or no warning), false otherwise */ 
bool check_warning_inscriptions( const item_def& item,
                                 operation_types oper )
{
    if (is_valid_item( item ) && has_warning_inscription(item, oper) )
    {
        if (oper == OPER_WEAR)
        {
            if (item.base_type != OBJ_ARMOUR)
                return (true);

            // don't ask if item already worn
            int equip = you.equip[get_armour_slot(item)];
            if (equip != -1 && item.link == equip)
                return (check_old_item_warning(item, oper));
        }
        else if (oper == OPER_PUTON)
        {
            if (item.base_type != OBJ_JEWELLERY)
                return (true);

            // don't ask if item already worn
            int equip = -1;
            if (jewellery_is_amulet(item))
                equip = you.equip[EQ_AMULET];
            else
            {
                equip = you.equip[EQ_LEFT_RING];
                if (equip != -1 && item.link == equip)
                    return (check_old_item_warning(item, oper));
                // or maybe the other ring?
                equip = you.equip[EQ_RIGHT_RING];
            }
                
            if (equip != -1 && item.link == equip)
                return (check_old_item_warning(item, oper));
        }
        
        std::string prompt = "Really " + operation_verb(oper) + " ";
        prompt += item.name(DESC_INVENTORY);
        prompt += "?";
        return (yesno(prompt.c_str(), false, 'n')
                && check_old_item_warning(item, oper));
    }
    else
        return (check_old_item_warning(item, oper));
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
                        MF_SINGLESELECT | MF_ANYPRINTABLE | MF_NO_SELECT_QTY
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
