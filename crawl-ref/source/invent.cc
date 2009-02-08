/*
 *  File:       invent.cc
 *  Summary:    Functions for inventory related commands.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

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
#include "decks.h"
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
#include "mon-util.h"
#include "randart.h"
#include "state.h"

#include "tiles.h"

///////////////////////////////////////////////////////////////////////////////
// Inventory menu shenanigans

static void _get_inv_items_to_show( std::vector<const item_def*> &v,
                                    int selector, int excluded_slot = -1);

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
                        || item->base_type == OBJ_ARMOUR
                        || item->base_type == OBJ_BOOKS))));
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
    if (item->link == -1 || item->pos.x != -1 || item->pos.y != -1)
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

    if (freshness >= 100 || player_mutation_level(MUT_SAPROVOROUS))
        freshness -= 300;

    // Ensure that chunk freshness is never zero, since zero means
    // that the item isn't a chunk.
    if (freshness >= 0) // possibly rotten chunks
        freshness++;

    return freshness;
}

void InvEntry::select(int qty)
{
    if ( item && item->quantity < qty )
        qty = item->quantity;
    MenuEntry::select(qty);
}

std::string InvEntry::get_filter_text() const
{
    return (filtering_item_prefix(*item) + " " + get_text());
}

std::string InvEntry::get_text() const
{
    std::ostringstream tstr;

    tstr << ' ' << static_cast<char>(hotkeys[0]) << ' ';
    if (!selected_qty)
        tstr << '-';
    else if (selected_qty < quantity)
        tstr << '#';
    else
        tstr << '+';

    if (InvEntry::show_glyph)
    {
        unsigned glyph_char;
        unsigned short glyph_col;
        get_item_glyph( item, &glyph_char, &glyph_col );

        const std::string col_string = colour_to_str(glyph_col);
        const std::string prefix = " (<" + col_string + ">"
                                   + static_cast<char>(glyph_char)
                                   + "</" + col_string + ">)";
        tstr << prefix;
    }
    tstr << ' ' << text;

    if (InvEntry::show_prices)
    {
        const int value = item_value(*item, show_prices);
        if (value > 0)
            tstr << " (" << value << " gold)";
    }

    if (Options.show_inventory_weights)
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
        add_hotkey(i.sub_type >= AMU_RAGE ? '"' : '=');
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

bool InvEntry::show_glyph = false;
void InvEntry::set_show_glyph(bool doshow)
{
    show_glyph = doshow;
}

InvMenu::InvMenu(int mflags)
    : Menu(mflags, "inventory", false), type(MT_INVLIST), pre_select(NULL),
      title_annotate(NULL)
{
    mdisplay->set_num_columns(2);
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

static bool _has_tran_unwearable_armour(bool &melded)
{
    bool unwearable = false;
         melded     = false;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def &item(you.inv[i]);

        if (is_valid_item(item) && item.base_type == OBJ_ARMOUR
            && !you_tran_can_wear(item))
        {
            unwearable = true;
            if (item_is_equipped(item))
            {
                melded = true;
                return (true);
            }
        }
    }
    return (unwearable);
}

static std::string _no_selectables_message(int item_selector)
{
    switch (item_selector)
    {
    case OSEL_ANY:
        return("You aren't carrying anything.");
    case OSEL_WIELD:
    case OBJ_WEAPONS:
        return("You aren't carrying any weapons.");
    case OBJ_ARMOUR:
    {
        bool melded = false;
        if (_has_tran_unwearable_armour(melded))
        {
            if (melded)
                return ("Your armour is currently melded into you.");
            else
                return("You aren't carrying any armour you can wear in your "
                       "current form.");
        }
        else
            return("You aren't carrying any armour.");
    }
    case OSEL_UNIDENT:
        return("You don't have any unidentified items.");
    case OSEL_MEMORISE:
        return("You aren't carrying any spellbooks.");
    case OSEL_RECHARGE:
        return("You aren't carrying any rechargeable items.");
    case OSEL_ENCH_ARM:
        return("You aren't carrying any armour which can be enchanted "
               "further.");
    case OBJ_CORPSES:
    case OSEL_VAMP_EAT:
        return("You aren't carrying any corpses which you can drain.");
    case OSEL_DRAW_DECK:
        return("You aren't carrying any decks from which to draw.");
    case OBJ_FOOD:
        return("You aren't carrying any food.");
    case OBJ_POTIONS:
        return("You aren't carrying any potions.");
    case OBJ_SCROLLS:
    case OBJ_BOOKS:
        return("You aren't carrying any books or scrolls.");
    case OBJ_WANDS:
        return("You aren't carrying any wands.");
    case OSEL_THROWABLE:
        return("You aren't carrying any items that might be thrown or fired.");
    case OSEL_BUTCHERY:
        return("You aren't carrying any sharp implements.");
    }

    return("You aren't carrying any such object.");
}

void InvMenu::load_inv_items(int item_selector, int excluded_slot,
                             MenuEntry *(*procfn)(MenuEntry *me))
{
    std::vector<const item_def *> tobeshown;
    _get_inv_items_to_show(tobeshown, item_selector, excluded_slot);

    load_items(tobeshown, procfn);

    if (!item_count())
        set_title(_no_selectables_message(item_selector));
    else
        set_title("");
}

#ifdef USE_TILE
bool InvEntry::get_tiles(std::vector<tile_def>& tileset) const
{
    if (quantity <= 0)
        return (false);

    int idx = tileidx_item(*item);
    if (!idx)
        return (false);

    tileset.push_back(tile_def(TILE_ITEM_SLOT, TEX_DUNGEON));
    tileset.push_back(tile_def(idx, TEX_DEFAULT));

    return (true);
}
#endif

bool InvMenu::is_selectable(int index) const
{
    if (type == MT_DROP)
    {
        InvEntry *item = dynamic_cast<InvEntry*>(items[index]);
        if (item->is_item_cursed() && item->is_item_equipped())
            return (false);

        std::string text = item->get_text();

        if (text.find("!*") != std::string::npos
            || text.find("!d") != std::string::npos)
        {
            return (false);
        }
    }

    return Menu::is_selectable(index);
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

bool sort_item_identified(const InvEntry *a)
{
    return !item_type_known(*(a->item));
}

static bool _compare_invmenu_items(const InvEntry *a, const InvEntry *b,
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
        return _compare_invmenu_items(ia, ib, &cond->cmp);
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
          { "identified",compare_item<bool, sort_item_identified> },
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

        for (unsigned ci = 0; ci < ARRAYSZ(cmp_map); ++ci)
            if (cmp_map[ci].cname == s)
            {
                list.push_back( item_comparator( cmp_map[ci].cmp, negated ) );
                break;
            }
    }

    if (list.empty())
    {
        list.push_back(
            item_comparator(compare_item_str<sort_item_fullname>));
    }
}

const menu_sort_condition *InvMenu::find_menu_sort_condition() const
{
    for (int i = Options.sort_menus.size() - 1; i >= 0; --i)
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
        if (!inv_class[i])
            continue;

        add_entry( new MenuEntry( item_class_name(i), MEL_SUBTITLE ) );
        items_in_class.clear();

        for (int j = 0, count = mitems.size(); j < count; ++j)
        {
            if (mitems[j]->base_type != i)
                continue;
            items_in_class.push_back( new InvEntry(*mitems[j]) );
        }

        sort_menu(items_in_class, cond);

        for (unsigned int j = 0; j < items_in_class.size(); ++j)
        {
            InvEntry *ie = items_in_class[j];
            if (this->tag == "pickup")
                ie->tag = "pickup";
            // If there's no hotkey, provide one.
            if (ie->hotkeys[0] == ' ')
                ie->hotkeys[0] = ckey++;
            do_preselect(ie);

            add_entry( procfn? (*procfn)(ie) : ie );
        }
    }

    // Don't make a menu so tall that we recycle hotkeys on the same page.
    if (mitems.size() > 52 && (max_pagesize > 52 || max_pagesize == 0))
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
        int newflag = is_set(MF_MULTISELECT) ? MF_SINGLESELECT | MF_ANYPRINTABLE
                                             : MF_MULTISELECT;

        flags &= ~(MF_SINGLESELECT | MF_MULTISELECT | MF_ANYPRINTABLE);
        flags |= newflag;

        deselect_all();
        sel.clear();
        draw_select_count(0, true);
        return (true);
    }
    return Menu::process_key( key );
}

unsigned char InvMenu::getkey() const
{
    unsigned char mkey = lastch;
    if (!isalnum(mkey) && mkey != '$' && mkey != '-' && mkey != '?'
        && mkey != '*' && mkey != ESCAPE)
    {
        mkey = ' ';
    }
    return (mkey);
}

//////////////////////////////////////////////////////////////////////////////

bool in_inventory( const item_def &i )
{
    return i.pos.x == -1 && i.pos.y == -1;
}

unsigned char get_invent( int invent_type )
{
    unsigned char select;

    while (true)
    {
        select = invent_select(NULL, MT_INVLIST, invent_type, -1,
                               MF_SINGLESELECT);

        if (isalpha(select))
        {
            const int invidx = letter_to_index(select);
            if ( is_valid_item(you.inv[invidx]) )
               describe_item( you.inv[invidx], true );
        }
        else
            break;
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
        if (mtype == MT_PICKUP)
            menu.set_tag("pickup");
        menu.load_items(items);
        int new_flags = noselect ? MF_NOSELECT
                                 : MF_MULTISELECT | MF_ALLOW_FILTER;
        new_flags |= MF_SHOW_PAGENUMBERS;
        menu.set_flags(new_flags);
        menu.show();
        selected = menu.get_selitems();
    }
    return (selected);
}

static bool _item_class_selected(const item_def &i, int selector)
{
    const int itype = i.base_type;
    if (selector == OSEL_ANY || selector == itype && itype != OBJ_ARMOUR)
        return (true);

    switch (selector)
    {
    case OBJ_ARMOUR:
        return (itype == OBJ_ARMOUR && you_tran_can_wear(i));

    case OSEL_UNIDENT:
        return !fully_identified(i);

    case OBJ_MISSILES:
        return (itype == OBJ_MISSILES || itype == OBJ_WEAPONS);

    case OSEL_THROWABLE:
    {
        if (i.base_type != OBJ_WEAPONS && i.base_type != OBJ_MISSILES)
            return (false);

        const launch_retval projected = is_launched(&you, you.weapon(), i);

        if (projected == LRET_FUMBLED)
            return (false);

        return (true);
    }
    case OBJ_WEAPONS:
    case OSEL_WIELD:
        return (itype == OBJ_WEAPONS || itype == OBJ_STAVES
                || itype == OBJ_MISCELLANY);

    case OSEL_BUTCHERY:
        return (itype == OBJ_WEAPONS && can_cut_meat(i));

    case OSEL_MEMORISE:
        return (itype == OBJ_BOOKS && i.sub_type != BOOK_MANUAL
                && (i.sub_type != BOOK_DESTRUCTION || !item_type_known(i)));

    case OBJ_SCROLLS:
        return (itype == OBJ_SCROLLS || itype == OBJ_BOOKS);

    case OSEL_RECHARGE:
        return (item_is_rechargeable(i, true, true));

    case OSEL_ENCH_ARM:
        return (is_enchantable_armour(i, true, true));

    case OSEL_VAMP_EAT:
        return (itype == OBJ_CORPSES && i.sub_type == CORPSE_BODY
                && !food_is_rotten(i) && mons_has_blood(i.plus));

    case OSEL_DRAW_DECK:
        return (is_deck(i));

    case OSEL_EQUIP:
        for (int eq = 0; eq < NUM_EQUIP; eq++)
        {
             if (you.equip[eq] == i.link)
                 return (true);
        }
        return (false);

    default:
        return (false);
    }
}

static bool _userdef_item_selected(const item_def &i, int selector)
{
#if defined(CLUA_BINDINGS)
    const char *luafn = selector == OSEL_WIELD ? "ch_item_wieldable"
                                               : NULL;
    return (luafn && clua.callbooleanfn(false, luafn, "u", &i));
#else
    return (false);
#endif
}

static bool _is_item_selected(const item_def &i, int selector)
{
    return (_item_class_selected(i, selector)
            || _userdef_item_selected(i, selector));
}

static void _get_inv_items_to_show(std::vector<const item_def*> &v,
                                   int selector, int excluded_slot)
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item(you.inv[i])
            && you.inv[i].slot != excluded_slot
            && _is_item_selected(you.inv[i], selector))
        {
            v.push_back( &you.inv[i] );
        }
    }
}

static bool _any_items_to_select(int selector)
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item(you.inv[i])
            && _is_item_selected(you.inv[i], selector))
        {
            return (true);
        }
    }
    return (false);
}

unsigned char invent_select( const char *title,
                             menu_type type,
                             int item_selector,
                             int excluded_slot,
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
    menu.load_inv_items(item_selector, excluded_slot);
    menu.set_type(type);

    // Don't override title if there are no items.
    if (title && menu.item_count())
        menu.set_title(title);

    menu.show(true);

    if (items)
        *items = menu.get_selitems();

    return (menu.getkey());
}

void browse_inventory( bool show_price )
{
    InvShowPrices show_item_prices(show_price);
    get_invent(OSEL_ANY);
}

// Reads in digits for a count and apprends then to val, the
// return value is the character that stopped the reading.
static unsigned char _get_invent_quant( unsigned char keyin, int &quant )
{
    quant = keyin - '0';

    while (true)
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
    while (true)
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
            int selmode = Options.drop_mode == DM_SINGLE
                          && (!pre_select || pre_select->empty()) ?
                        MF_SINGLESELECT | MF_EASY_EXIT | MF_ANYPRINTABLE :
                        MF_MULTISELECT | MF_ALLOW_FILTER;

            // The "view inventory listing" mode.
            int ch = invent_select( prompt,
                                    mtype,
                                    keyin == '*' ? OSEL_ANY : type_expect,
                                    -1,
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
            keyin = _get_invent_quant( keyin, count );

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
            // We've got a character we don't understand...
            canned_msg( MSG_HUH );
        }
        else
        {
            // We're going to loop back up, so don't draw another prompt.
            need_prompt = false;
        }
    }

    if (ret != PROMPT_ABORT)
    {
        items.push_back(
            SelItem( ret, count,
                     ret != PROMPT_GOT_SPECIAL ? &you.inv[ret] : NULL ) );
    }
    return items;
}

static int _digit_to_index( char digit, operation_types oper )
{

    const char iletter = static_cast<char>(oper);

    for ( int i = 0; i < ENDOFPACK; ++i )
    {
        if (is_valid_item(you.inv[i]))
        {
            const std::string& r(you.inv[i].inscription);
            // Note that r.size() is unsigned.
            for ( unsigned int j = 0; j + 2 < r.size(); ++j )
            {
                if ( r[j] == '@'
                     && (r[j+1] == iletter || r[j+1] == '*')
                     && r[j+2] == digit )
                {
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
    {
        if (r[i] == '!')
        {
            if (r[i+1] == iletter || r[i+1] == '*')
                return (true);
            else if (oper == OPER_ZAP && r[i+1] == 'z') // for the 0.3.4. keys
                return (true);
        }
    }

    return (false);
}

// Checks if current item (to be removed) has a warning inscription
// and prompts the user for confirmation.
bool check_old_item_warning( const item_def& item,
                             operation_types oper )
{
    item_def old_item;
    std::string prompt;
    if (oper == OPER_WIELD) // can we safely unwield old item?
    {
        if (!you.weapon())
            return (true);

        old_item = *you.weapon();
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

static std::string _operation_verb(operation_types oper)
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
    case OPER_EXAMINE:        return "examine";
    case OPER_FIRE:           return "fire";
    case OPER_PRAY:           return "sacrifice";
    case OPER_EVOKE:          return "evoke";
    case OPER_DESTROY:        return "destroy";
    case OPER_QUIVER:         return "quiver";
    case OPER_ANY:
    default:
        return "choose";
    }
}

// Returns true if user OK'd it (or no warning), false otherwise.
bool check_warning_inscriptions( const item_def& item,
                                 operation_types oper )
{
    if (is_valid_item( item ) && has_warning_inscription(item, oper) )
    {
        // When it's about destroying an item, don't even ask.
        // If the player really wants to do that, they'll have
        // to remove the inscription.
        if (oper == OPER_DESTROY)
            return (false);

        if (oper == OPER_WEAR)
        {
            if (item.base_type != OBJ_ARMOUR)
                return (true);

            // Don't ask if item already worn.
            int equip = you.equip[get_armour_slot(item)];
            if (equip != -1 && item.link == equip)
                return (check_old_item_warning(item, oper));
        }
        else if (oper == OPER_PUTON)
        {
            if (item.base_type != OBJ_JEWELLERY)
                return (true);

            // Don't ask if item already worn.
            int equip = -1;
            if (jewellery_is_amulet(item))
                equip = you.equip[EQ_AMULET];
            else
            {
                equip = you.equip[EQ_LEFT_RING];
                if (equip != -1 && item.link == equip)
                    return (check_old_item_warning(item, oper));

                // Or maybe the other ring?
                equip = you.equip[EQ_RIGHT_RING];
            }

            if (equip != -1 && item.link == equip)
                return (check_old_item_warning(item, oper));
        }

        std::string prompt = "Really " + _operation_verb(oper) + " ";
        prompt += (in_inventory(item) ? item.name(DESC_INVENTORY)
                                      : item.name(DESC_NOCAP_A));
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
// It returns PROMPT_NOTHING     if there are no matching items.
//
// Note: This function never checks if the item is appropriate.
int prompt_invent_item( const char *prompt,
                        menu_type mtype, int type_expect,
                        bool must_exist, bool allow_auto_list,
                        bool allow_easy_quit,
                        const char other_valid_char,
                        int excluded_slot,
                        int *const count,
                        operation_types oper )
{
    if (!_any_items_to_select(type_expect) && type_expect == OSEL_THROWABLE
        && oper == OPER_FIRE && mtype == MT_INVLIST)
    {
        type_expect = OSEL_ANY;
    }

    if (!_any_items_to_select(type_expect) && type_expect != OSEL_WIELD
        && type_expect != OSEL_BUTCHERY && mtype == MT_INVLIST)
    {
        mprf(MSGCH_PROMPT, "%s",
             _no_selectables_message(type_expect).c_str());
        return (PROMPT_NOTHING);
    }

    unsigned char  keyin = 0;
    int            ret = PROMPT_ABORT;

    bool           need_redraw = false;
    bool           need_prompt = true;
    bool           need_getch  = true;
    bool           auto_list   = Options.auto_list && allow_auto_list;

    if (auto_list)
    {
        need_prompt = need_getch = false;

        if (_any_items_to_select(type_expect))
            keyin = '?';
        else
            keyin = '*';
    }

    while (true)
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
                        keyin == '*' ? OSEL_ANY : type_expect,
                        excluded_slot,
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
            keyin = _get_invent_quant( keyin, *count );

            need_prompt = false;
            need_getch  = false;
        }
        else if ( count == NULL && isdigit( keyin ) )
        {
            /* scan for our item */
            int res = _digit_to_index( keyin, oper );
            if ( res != -1 )
            {
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
            else if ( check_warning_inscriptions( you.inv[ret], oper ) )
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

    return (ret);
}

bool prompt_failed(int retval, std::string msg)
{
    if (retval != PROMPT_ABORT && retval != PROMPT_NOTHING)
        return (false);

    if (msg.empty())
    {
        if (retval == PROMPT_ABORT)
            canned_msg(MSG_OK);
    }
    else
        mprf(MSGCH_PROMPT, msg.c_str());

    crawl_state.cancel_cmd_repeat();

    return (true);
}
