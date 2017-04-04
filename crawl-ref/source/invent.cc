/**
 * @file
 * @brief Functions for inventory related commands.
**/

#include "AppHdr.h"

#include "invent.h"

#include <algorithm> // any_of
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "artefact.h"
#include "colour.h"
#include "command.h"
#include "decks.h"
#include "describe.h"
#include "env.h"
#include "food.h"
#include "god-item.h"
#include "god-passive.h"
#include "initfile.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "options.h"
#include "output.h"
#include "prompt.h"
#include "religion.h"
#include "showsymb.h"
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#include "throw.h"
#ifdef USE_TILE
 #include "tiledef-icons.h"
 #include "tiledef-main.h"
 #include "tiledef-dngn.h"
 #include "tilepick.h"
#endif

///////////////////////////////////////////////////////////////////////////////
// Inventory menu shenanigans

static void _get_inv_items_to_show(vector<const item_def*> &v,
                                   int selector, int excluded_slot = -1);

InvTitle::InvTitle(Menu *mn, const string &title, invtitle_annotator tfn)
    : MenuEntry(title, MEL_TITLE)
{
    m       = mn;
    titlefn = tfn;
}

string InvTitle::get_text(const bool) const
{
    return titlefn ? titlefn(m, MenuEntry::get_text())
                   : MenuEntry::get_text();
}

InvEntry::InvEntry(const item_def &i)
    : MenuEntry("", MEL_ITEM), item(&i)
{
    data = const_cast<item_def *>(item);

    if (in_inventory(i) && i.base_type != OBJ_GOLD)
    {
        // We need to do this in order to get the 'wielded' annotation.
        // We then toss out the first four characters, which look
        // like this: "a - ". Ow. FIXME.
        text = i.name(DESC_INVENTORY_EQUIP, false).substr(4);
    }
    else
        text = i.name(DESC_A, false);

    if (item_is_stationary_net(i))
    {
        text += make_stringf(" (holding %s)",
                             net_holdee(i)->name(DESC_A).c_str());
    }

    if (i.base_type != OBJ_GOLD && in_inventory(i))
        add_hotkey(index_to_letter(i.link));
    else
        add_hotkey(' ');        // dummy hotkey

    add_class_hotkeys(i);

    quantity = i.quantity;
}

const string &InvEntry::get_basename() const
{
    if (basename.empty())
        basename = item->name(DESC_BASENAME);
    return basename;
}

const string &InvEntry::get_qualname() const
{
    if (qualname.empty())
        qualname = item->name(DESC_QUALNAME);
    return qualname;
}

const string &InvEntry::get_fullname() const
{
    return text;
}

const string &InvEntry::get_dbname() const
{
    if (dbname.empty())
        dbname = item->name(DESC_DBNAME);
    return dbname;
}

bool InvEntry::is_cursed() const
{
    return item_ident(*item, ISFLAG_KNOW_CURSE) && item->cursed();
}

bool InvEntry::is_glowing() const
{
    return !item_ident(*item, ISFLAG_KNOW_TYPE)
           && (get_equip_desc(*item)
               || (is_artefact(*item)
                   && (item->base_type == OBJ_WEAPONS
                       || item->base_type == OBJ_ARMOUR
                       || item->base_type == OBJ_BOOKS)));
}

bool InvEntry::is_ego() const
{
    return item_ident(*item, ISFLAG_KNOW_TYPE) && !is_artefact(*item)
           && item->brand != 0
           && (item->base_type == OBJ_WEAPONS
               || item->base_type == OBJ_MISSILES
               || item->base_type == OBJ_ARMOUR);
}

bool InvEntry::is_art() const
{
    return item_ident(*item, ISFLAG_KNOW_TYPE) && is_artefact(*item);
}

bool InvEntry::is_equipped() const
{
    if (item->link == -1 || item->pos != ITEM_IN_INVENTORY)
        return false;

    for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; i++)
        if (item->link == you.equip[i])
            return true;

    return false;
}

void InvEntry::select(int qty)
{
    if (item && item->quantity < qty)
        qty = item->quantity;

    MenuEntry::select(qty);
}

string InvEntry::get_filter_text() const
{
    return item_prefix(*item) + " " + get_text();
}

string InvEntry::get_text(bool need_cursor) const
{
    need_cursor = need_cursor && show_cursor;

    ostringstream tstr;

    const bool nosel = hotkeys.empty();
    const char key = nosel ? ' ' : static_cast<char>(hotkeys[0]);

    tstr << ' ' << key;

    if (!nosel || tag == "pickup")
    {
        if (need_cursor)
            tstr << '[';
        else
            tstr << ' ';

        if (nosel)
            tstr << ' ';
        else if (!selected_qty)
            tstr << '-';
        else if (selected_qty < quantity)
            tstr << '#';
        else
            tstr << '+';

        if (need_cursor)
            tstr << ']';
        else
            tstr << ' ';
    }
    if (InvEntry::show_glyph)
        tstr << "(" << glyph_to_tagstr(get_item_glyph(*item)) << ")" << " ";

    tstr << text;
    const string str = tstr.str();

    if (printed_width(str) > get_number_of_cols())
        return chop_tagged_string(str, get_number_of_cols() - 2) + "..";
    else
        return str;
}

void get_class_hotkeys(const int type, vector<char> &glyphs)
{
    switch (type)
    {
    case OBJ_GOLD:
        glyphs.push_back('$');
        break;
    case OBJ_MISSILES:
        glyphs.push_back('(');
        break;
    case OBJ_WEAPONS:
        glyphs.push_back(')');
        break;
    case OBJ_ARMOUR:
        glyphs.push_back('[');
        break;
    case OBJ_WANDS:
        glyphs.push_back('/');
        break;
    case OBJ_FOOD:
        glyphs.push_back('%');
        break;
    case OBJ_BOOKS:
        glyphs.push_back(':');
        break;
    case OBJ_SCROLLS:
        glyphs.push_back('?');
        break;
    case OBJ_JEWELLERY:
        glyphs.push_back('"');
        glyphs.push_back('=');
        break;
    case OBJ_POTIONS:
        glyphs.push_back('!');
        break;
    case OBJ_STAVES:
        glyphs.push_back('|');
        break;
#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:
        glyphs.push_back('\\');
        break;
#endif
    case OBJ_MISCELLANY:
        glyphs.push_back('}');
        break;
    case OBJ_CORPSES:
        glyphs.push_back('&');
        break;
    default:
        break;
    }
}

void InvEntry::add_class_hotkeys(const item_def &i)
{
    const int type = i.base_type;
    if (type == OBJ_JEWELLERY)
    {
        add_hotkey(i.sub_type >= AMU_FIRST_AMULET ? '"' : '=');
        return;
    }

    vector<char> glyphs;
    get_class_hotkeys(type, glyphs);
    for (char gly : glyphs)
        add_hotkey(gly);
}

bool InvEntry::show_cursor = false;
void InvEntry::set_show_cursor(bool doshow)
{
    show_cursor = doshow;
}

bool InvEntry::show_glyph = false;
void InvEntry::set_show_glyph(bool doshow)
{
    show_glyph = doshow;
}

InvMenu::InvMenu(int mflags)
    : Menu(mflags, "inventory", false), type(MT_INVLIST), pre_select(nullptr),
      title_annotate(nullptr)
{
#ifdef USE_TILE_LOCAL
    if (Options.tile_menu_icons)
#endif
        mdisplay->set_num_columns(2);

    InvEntry::set_show_cursor(false);
}

void InvMenu::set_type(menu_type t)
{
    type = t;
}

void InvMenu::set_title_annotator(invtitle_annotator afn)
{
    title_annotate = afn;
}

void InvMenu::set_title(MenuEntry *t, bool first)
{
    Menu::set_title(t, first);
}

void InvMenu::set_preselect(const vector<SelItem> *pre)
{
    pre_select = pre;
}

string slot_description()
{
    return make_stringf("%d/%d slots", inv_count(), ENDOFPACK);
}

void InvMenu::set_title(const string &s)
{
    set_title(new InvTitle(this, s.empty() ? "Inventory: " + slot_description()
                                           : s,
                           title_annotate));
}

static bool _has_melded_armour()
{
    for (int e = EQ_CLOAK; e <= EQ_BODY_ARMOUR; e++)
        if (you.melded[e])
            return true;
    return false;
}

static bool _has_temp_unwearable_armour()
{
    for (const auto &item : you.inv)
    {
        if (item.defined() && item.base_type == OBJ_ARMOUR
            && can_wear_armour(item, false, true)
            && !can_wear_armour(item, false, false))
        {
            return true;
        }
    }
    return false;
}

static bool _has_hand_evokable()
{
    for (const auto &item : you.inv)
    {
        if (item.defined()
            && item_is_evokable(item, true, true, true, false, false)
            && !item_is_evokable(item, true, true, true, false, true))
        {
            return true;
        }
    }
    return false;
}

/**
 * What message should the player be given when they look for items matching
 * the given selector and don't find any?
 *
 * @param selector      The given type of object_selector.
 * @return              A message such as "You aren't carrying any weapons."
 *                      "Your armour is currently melded into you.", etc.
 */
string no_selectables_message(int item_selector)
{
    switch (item_selector)
    {
    case OSEL_ANY:
        return "You aren't carrying anything.";
    case OSEL_WIELD:
    case OBJ_WEAPONS:
        return "You aren't carrying any weapons.";
    case OSEL_BLESSABLE_WEAPON:
        return "You aren't carrying any weapons that can be blessed.";
    case OBJ_ARMOUR:
    {
        if (_has_melded_armour())
            return "Your armour is currently melded into you.";
        else if (_has_temp_unwearable_armour())
            return "You aren't carrying any currently wearable armour.";
        else
            return "You aren't carrying any wearable armour.";
    }
    case OSEL_UNIDENT:
        return "You don't have any unidentified items.";
    case OSEL_RECHARGE:
    case OSEL_SUPERCHARGE:
        return "You aren't carrying any rechargeable items.";
    case OSEL_ENCHANTABLE_ARMOUR:
        return "You aren't carrying any armour which can be enchanted further.";
    case OBJ_CORPSES:
        return "You don't have any corpses.";
    case OSEL_DRAW_DECK:
        return "You aren't carrying any decks from which to draw.";
    case OBJ_FOOD:
        return "You aren't carrying any food.";
    case OBJ_POTIONS:
        return "You aren't carrying any potions.";
    case OBJ_SCROLLS:
    case OBJ_BOOKS:
        return "You aren't carrying any spellbooks or scrolls.";
    case OBJ_WANDS:
        return "You aren't carrying any wands.";
    case OBJ_JEWELLERY:
        return "You aren't carrying any pieces of jewellery.";
    case OSEL_THROWABLE:
        return "You aren't carrying any items that might be thrown or fired.";
    case OSEL_EVOKABLE:
        if (_has_hand_evokable())
            return "You aren't carrying any items that you can evoke without wielding.";
        else
            return "You aren't carrying any items that you can evoke.";
    case OSEL_CURSED_WORN:
        return "None of your equipped items are cursed.";
#if TAG_MAJOR_VERSION == 34
    case OSEL_UNCURSED_WORN_ARMOUR:
        return "You aren't wearing any piece of uncursed armour.";
    case OSEL_UNCURSED_WORN_JEWELLERY:
        return "You aren't wearing any piece of uncursed jewellery.";
#endif
    case OSEL_BRANDABLE_WEAPON:
        return "You aren't carrying any weapons that can be branded.";
    case OSEL_ENCHANTABLE_WEAPON:
        return "You aren't carrying any weapons that can be enchanted.";
    case OSEL_BEOGH_GIFT:
        return "You aren't carrying anything you can give to a follower.";
    case OSEL_CURSABLE:
        return "You don't have any cursable items.";
    }

    return "You aren't carrying any such object.";
}

void InvMenu::load_inv_items(int item_selector, int excluded_slot,
                             function<MenuEntry* (MenuEntry*)> procfn)
{
    vector<const item_def *> tobeshown;
    _get_inv_items_to_show(tobeshown, item_selector, excluded_slot);

    load_items(tobeshown, procfn);

    if (!item_count())
        set_title(no_selectables_message(item_selector));
    else
        set_title("");
}

#ifdef USE_TILE
bool InvEntry::get_tiles(vector<tile_def>& tileset) const
{
    if (!Options.tile_menu_icons)
        return false;

    if (quantity <= 0)
        return false;

    tileidx_t idx = tileidx_item(get_item_info(*item));
    if (!idx)
        return false;

    if (in_inventory(*item))
    {
        const equipment_type eq = item_equip_slot(*item);
        if (eq != EQ_NONE)
        {
            if (item_known_cursed(*item))
                tileset.emplace_back(TILE_ITEM_SLOT_EQUIP_CURSED, TEX_DEFAULT);
            else
                tileset.emplace_back(TILE_ITEM_SLOT_EQUIP, TEX_DEFAULT);
        }
        else if (item_known_cursed(*item))
            tileset.emplace_back(TILE_ITEM_SLOT_CURSED, TEX_DEFAULT);

        tileidx_t base_item = tileidx_known_base_item(idx);
        if (base_item)
            tileset.emplace_back(base_item, TEX_DEFAULT);
        tileset.emplace_back(idx, TEX_DEFAULT);

        if (eq != EQ_NONE && you.melded[eq])
            tileset.emplace_back(TILEI_MESH, TEX_ICONS);
    }
    else
    {
        // Do we want to display the floor type or is that too distracting?
        const coord_def c = item->held_by_monster()
            ? item->holding_monster()->pos()
            : item->pos;
        tileidx_t ch = 0;
        if (c != coord_def() && show_background)
        {
            ch = tileidx_feature(c);
            if (ch == TILE_FLOOR_NORMAL)
                ch = env.tile_flv(c).floor;
            else if (ch == TILE_WALL_NORMAL)
                ch = env.tile_flv(c).wall;

            tileset.emplace_back(ch, get_dngn_tex(ch));
        }
        tileidx_t base_item = tileidx_known_base_item(idx);
        if (base_item)
            tileset.emplace_back(base_item, TEX_DEFAULT);

        tileset.emplace_back(idx, TEX_DEFAULT);

        if (ch != 0)
        {
            // Needs to be displayed so as to not give away mimics in shallow water.
            if (ch == TILE_DNGN_SHALLOW_WATER)
                tileset.emplace_back(TILEI_MASK_SHALLOW_WATER, TEX_ICONS);
            else if (ch == TILE_DNGN_SHALLOW_WATER_MURKY)
                tileset.emplace_back(TILEI_MASK_SHALLOW_WATER_MURKY, TEX_ICONS);
        }
    }
    if (item->base_type == OBJ_WEAPONS || item->base_type == OBJ_MISSILES
        || item->base_type == OBJ_ARMOUR
#if TAG_MAJOR_VERSION == 34
        || item->base_type == OBJ_RODS
#endif
       )
    {
        tileidx_t brand = tileidx_known_brand(*item);
        if (brand)
            tileset.emplace_back(brand, TEX_DEFAULT);
    }
    else if (item->base_type == OBJ_CORPSES)
    {
        tileidx_t brand = tileidx_corpse_brand(*item);
        if (brand)
            tileset.emplace_back(brand, TEX_DEFAULT);
    }

    return true;
}
#endif

bool InvMenu::is_selectable(int index) const
{
    if (type == MT_DROP)
    {
        InvEntry *item = dynamic_cast<InvEntry*>(items[index]);
        if (item->is_cursed() && item->is_equipped())
            return false;

        string text = item->get_text();

        if (text.find("!*") != string::npos || text.find("!d") != string::npos)
            return false;
    }

    return Menu::is_selectable(index);
}

template <const string &(InvEntry::*method)() const>
static int compare_item_str(const InvEntry *a, const InvEntry *b)
{
    return (a->*method)().compare((b->*method)());
}

// Would call this just compare_item, but MSVC mistakenly thinks the next
// one is a specialization rather than an overload.
template <typename T, T (*proc)(const InvEntry *a)>
static int compare_item_fn(const InvEntry *a, const InvEntry *b)
{
    return int(proc(a)) - int(proc(b));
}

template <typename T, T (InvEntry::*method)() const>
static int compare_item(const InvEntry *a, const InvEntry *b)
{
    return int((a->*method)()) - int((b->*method)());
}

template <typename T, T (InvEntry::*method)() const>
static int compare_item_rev(const InvEntry *a, const InvEntry *b)
{
    return int((b->*method)()) - int((a->*method)());
}

template <item_sort_fn cmp>
static int compare_reverse(const InvEntry *a, const InvEntry *b)
{
    return -cmp(a, b);
}

// We need C++11 already!
// Some prototypes to prevent warnings; we can't make these static because
// they're used as template parameters.
int sort_item_qty(const InvEntry *a);
int sort_item_slot(const InvEntry *a);
bool sort_item_identified(const InvEntry *a);
bool sort_item_charged(const InvEntry *a);

int sort_item_qty(const InvEntry *a)
{
    return a->quantity;
}
int sort_item_slot(const InvEntry *a)
{
    return a->item->link;
}

bool sort_item_identified(const InvEntry *a)
{
    return !item_type_known(*(a->item));
}

bool sort_item_charged(const InvEntry *a)
{
    return a->item->base_type != OBJ_WANDS
           || !item_is_evokable(*(a->item), false, true);
}

static bool _compare_invmenu_items(const InvEntry *a, const InvEntry *b,
                                   const item_sort_comparators *cmps)
{
    for (const auto &comparator : *cmps)
    {
        const int cmp = comparator.compare(a, b);
        if (cmp)
            return cmp < 0;
    }
    return a->item->link < b->item->link;
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

void init_item_sort_comparators(item_sort_comparators &list, const string &set)
{
    static struct
    {
        const string cname;
        item_sort_fn cmp;
    } cmp_map[]  =
      {
          { "basename",  compare_item_str<&InvEntry::get_basename> },
          { "qualname",  compare_item_str<&InvEntry::get_qualname> },
          { "fullname",  compare_item_str<&InvEntry::get_fullname> },
          { "dbname",    compare_item_str<&InvEntry::get_dbname> },
          { "curse",     compare_item<bool, &InvEntry::is_cursed> },
          { "glowing",   compare_item_rev<bool, &InvEntry::is_glowing> },
          { "ego",       compare_item_rev<bool, &InvEntry::is_ego> },
          { "art",       compare_item_rev<bool, &InvEntry::is_art> },
          { "equipped",  compare_item_rev<bool, &InvEntry::is_equipped> },
          { "identified",compare_item_fn<bool, sort_item_identified> },
          { "charged",   compare_item_fn<bool, sort_item_charged>},
          { "qty",       compare_item_fn<int, sort_item_qty> },
          { "slot",      compare_item_fn<int, sort_item_slot> },
      };

    list.clear();
    for (string s : split_string(",", set))
    {
        if (s.empty())
            continue;

        const bool negated = s[0] == '>';
        if (s[0] == '<' || s[0] == '>')
            s = s.substr(1);

        for (const auto &ci : cmp_map)
            if (ci.cname == s)
            {
                list.emplace_back(ci.cmp, negated);
                break;
            }
    }

    if (list.empty())
        list.emplace_back(compare_item_str<&InvEntry::get_fullname>);
}

const menu_sort_condition *InvMenu::find_menu_sort_condition() const
{
    for (int i = Options.sort_menus.size() - 1; i >= 0; --i)
        if (Options.sort_menus[i].matches(type))
            return &Options.sort_menus[i];

    return nullptr;
}

void InvMenu::sort_menu(vector<InvEntry*> &invitems,
                        const menu_sort_condition *cond)
{
    if (!cond || cond->sort == -1 || (int) invitems.size() < cond->sort)
        return;

    sort(invitems.begin(), invitems.end(), menu_entry_comparator(cond));
}

FixedVector<int, NUM_OBJECT_CLASSES> inv_order(
    OBJ_WEAPONS,
    OBJ_MISSILES,
    OBJ_ARMOUR,
    OBJ_STAVES,
#if TAG_MAJOR_VERSION == 34
    OBJ_RODS,
#endif
    OBJ_JEWELLERY,
    OBJ_WANDS,
    OBJ_SCROLLS,
    OBJ_POTIONS,
    OBJ_BOOKS,
    OBJ_MISCELLANY,
    OBJ_FOOD,
    // These four can't actually be in your inventory.
    OBJ_CORPSES,
    OBJ_RUNES,
    OBJ_ORBS,
    OBJ_GOLD);

menu_letter InvMenu::load_items(const vector<item_def>& mitems,
                                function<MenuEntry* (MenuEntry*)> procfn,
                                menu_letter ckey, bool sort)
{
    vector<const item_def*> xlatitems;
    for (const item_def &item : mitems)
        xlatitems.push_back(&item);
    return load_items(xlatitems, procfn, ckey, sort);
}

menu_letter InvMenu::load_items(const vector<const item_def*> &mitems,
                                function<MenuEntry* (MenuEntry*)> procfn,
                                menu_letter ckey, bool sort)
{
    FixedVector< int, NUM_OBJECT_CLASSES > inv_class(0);
    for (const item_def * const mitem : mitems)
        inv_class[mitem->base_type]++;

    vector<InvEntry*> items_in_class;
    const menu_sort_condition *cond = nullptr;
    if (sort) cond = find_menu_sort_condition();

    for (int obj = 0; obj < NUM_OBJECT_CLASSES; ++obj)
    {
        int i = inv_order[obj];

        if (!inv_class[i])
            continue;

        string subtitle = item_class_name(i);

        // Mention the class selection shortcuts.
        if (is_set(MF_MULTISELECT))
        {
            vector<char> glyphs;
            get_class_hotkeys(i, glyphs);
            if (!glyphs.empty())
            {
                // longest string
                const string str = "Magical Staves ";
                subtitle += string(strwidth(str) - strwidth(subtitle),
                                   ' ');
                subtitle += "(select all with <w>";
                for (char gly : glyphs)
                    subtitle += gly;
                subtitle += "</w><blue>)";
            }
        }
        add_entry(new MenuEntry(subtitle, MEL_SUBTITLE));

        items_in_class.clear();

        InvEntry *forced_first = nullptr;
        for (const item_def * const mitem : mitems)
        {
            if (mitem->base_type != i)
                continue;

            InvEntry * const ie = new InvEntry(*mitem);
            if (mitem->sub_type == get_max_subtype(mitem->base_type))
                forced_first = ie;
            else
                items_in_class.push_back(ie);
        }

        sort_menu(items_in_class, cond);
        if (forced_first)
            items_in_class.insert(items_in_class.begin(),forced_first);

        for (InvEntry *ie : items_in_class)
        {
            if (tag == "pickup")
            {
                if (ie->item && item_is_stationary(*ie->item))
                    ie->tag = "nopickup";
                else
                    ie->tag = "pickup";
            }
            if (get_flags() & MF_NOSELECT)
                ie->hotkeys.clear();
            // If there's no hotkey, provide one.
            else if (ie->hotkeys[0] == ' ')
            {
                if (ie->tag == "nopickup")
                    ie->hotkeys.clear();
                else
                    ie->hotkeys[0] = ckey++;
            }
            do_preselect(ie);

            add_entry(procfn ? procfn(ie) : ie);
        }
    }

    // Don't make a menu so tall that we recycle hotkeys on the same page.
    if (mitems.size() > 52 && (max_pagesize > 52 || max_pagesize == 0))
        set_maxpagesize(52);

    return ckey;
}

void InvMenu::do_preselect(InvEntry *ie)
{
    if (!pre_select || pre_select->empty())
        return;

    for (const SelItem &presel : *pre_select)
        if (ie->item && ie->item == presel.item)
        {
            ie->selected_qty = presel.quantity;
            break;
        }
}

vector<SelItem> InvMenu::get_selitems() const
{
    vector<SelItem> selected_items;
    for (MenuEntry *me : sel)
    {
        InvEntry *inv = dynamic_cast<InvEntry*>(me);
        selected_items.emplace_back(inv->hotkeys[0], inv->selected_qty,
                                    inv->item);
    }
    return selected_items;
}

string InvMenu::help_key() const
{
    return type == MT_DROP || type == MT_PICKUP ? "pick-up" : "";
}

int InvMenu::getkey() const
{
    auto mkey = lastch;
    if (type == MT_KNOW && (mkey == 0 || mkey == CK_ENTER))
        return mkey;

    if (!isaalnum(mkey) && mkey != '$' && mkey != '-' && mkey != '?'
        && mkey != '*' && !key_is_escape(mkey) && mkey != '\\'
        && mkey != ',')
    {
        mkey = ' ';
    }
    return mkey;
}

//////////////////////////////////////////////////////////////////////////////

bool in_inventory(const item_def &i)
{
    return i.pos == ITEM_IN_INVENTORY;
}

const char *item_class_name(int type, bool terse)
{
    if (terse)
    {
        switch (type)
        {
        case OBJ_STAVES:     return "magical staff";
        case OBJ_MISCELLANY: return "misc";
        default:             return base_type_string((object_class_type) type);
        }
    }
    else
    {
        switch (type)
        {
        case OBJ_GOLD:       return "Gold";
        case OBJ_WEAPONS:    return "Hand Weapons";
        case OBJ_MISSILES:   return "Missiles";
        case OBJ_ARMOUR:     return "Armour";
        case OBJ_WANDS:      return "Wands";
        case OBJ_FOOD:       return "Comestibles";
        case OBJ_SCROLLS:    return "Scrolls";
        case OBJ_JEWELLERY:  return "Jewellery";
        case OBJ_POTIONS:    return "Potions";
        case OBJ_BOOKS:      return "Books";
        case OBJ_STAVES:     return "Magical Staves";
#if TAG_MAJOR_VERSION == 34
        case OBJ_RODS:       return "Rods";
#endif
        case OBJ_ORBS:       return "Orbs of Power";
        case OBJ_MISCELLANY: return "Miscellaneous";
        case OBJ_CORPSES:    return "Carrion";
        case OBJ_RUNES:      return "Runes of Zot";
        }
    }
    return "";
}

const char* item_slot_name(equipment_type type)
{
    switch (type)
    {
    case EQ_CLOAK:       return "cloak";
    case EQ_HELMET:      return "helmet";
    case EQ_GLOVES:      return "gloves";
    case EQ_BOOTS:       return "boots";
    case EQ_SHIELD:      return "shield";
    case EQ_BODY_ARMOUR: return "body";
    default:             return "";
    }
}

vector<SelItem> select_items(const vector<const item_def*> &items,
                             const char *title, bool noselect,
                             menu_type mtype,
                             invtitle_annotator titlefn)
{
    vector<SelItem> selected;
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

        if (mtype == MT_SELONE)
        {
            new_flags |= MF_SINGLESELECT;
            new_flags &= ~MF_MULTISELECT;
        }

        new_flags |= MF_SHOW_PAGENUMBERS | MF_ALLOW_FORMATTING;
        menu.set_flags(new_flags);
        menu.show();
        selected = menu.get_selitems();
    }
    return selected;
}

bool item_is_selected(const item_def &i, int selector)
{
    const object_class_type itype = i.base_type;
    if (selector == OSEL_ANY || selector == itype
                                && itype != OBJ_FOOD && itype != OBJ_ARMOUR)
    {
        return true;
    }

    switch (selector)
    {
    case OBJ_ARMOUR:
        return itype == OBJ_ARMOUR && can_wear_armour(i, false, false);

    case OSEL_WORN_ARMOUR:
        return itype == OBJ_ARMOUR && item_is_equipped(i);

    case OSEL_UNIDENT:
        return !fully_identified(i) && itype != OBJ_BOOKS;

    case OBJ_MISSILES:
        return itype == OBJ_MISSILES || itype == OBJ_WEAPONS;

    case OSEL_THROWABLE:
    {
        if (you_worship(GOD_TROG) && item_is_spellbook(i))
            return true;

        if (itype != OBJ_WEAPONS && itype != OBJ_MISSILES)
            return false;

        const launch_retval projected = is_launched(&you, you.weapon(), i);

        if (projected == LRET_FUMBLED)
            return false;

        return true;
    }
    case OBJ_WEAPONS:
    case OSEL_WIELD:
        return item_is_wieldable(i);

    case OBJ_SCROLLS:
        return itype == OBJ_SCROLLS
               || (itype == OBJ_BOOKS && i.sub_type != BOOK_MANUAL);

    case OSEL_RECHARGE:
    case OSEL_SUPERCHARGE:
        return item_is_rechargeable(i, selector != OSEL_SUPERCHARGE);

    case OSEL_EVOKABLE:
        return item_is_evokable(i, true, true, true);

    case OSEL_ENCHANTABLE_ARMOUR:
        return is_enchantable_armour(i, true);

    case OBJ_FOOD:
        return itype == OBJ_FOOD && !is_inedible(i);

    case OSEL_DRAW_DECK:
        return is_deck(i);

    case OSEL_CURSED_WORN:
        return i.cursed() && item_is_equipped(i)
               && (&i != you.weapon() || is_weapon(i));

#if TAG_MAJOR_VERSION == 34
    case OSEL_UNCURSED_WORN_ARMOUR:
        return !i.cursed() && item_is_equipped(i) && itype == OBJ_ARMOUR;

    case OSEL_UNCURSED_WORN_JEWELLERY:
        return !i.cursed() && item_is_equipped(i) && itype == OBJ_JEWELLERY;
#endif

    case OSEL_BRANDABLE_WEAPON:
        return is_brandable_weapon(i, true);

    case OSEL_ENCHANTABLE_WEAPON:
        return itype == OBJ_WEAPONS
               && !is_artefact(i)
               && (!item_ident(i, ISFLAG_KNOW_PLUSES)
                   || i.plus < MAX_WPN_ENCHANT);

    case OSEL_BLESSABLE_WEAPON:
        return is_brandable_weapon(i, you_worship(GOD_SHINING_ONE), true);

    case OSEL_BEOGH_GIFT:
        return (itype == OBJ_WEAPONS
                || is_shield(i)
                || itype == OBJ_ARMOUR
                   && get_armour_slot(i) == EQ_BODY_ARMOUR)
                && !item_is_equipped(i);

    case OSEL_CURSABLE:
        return item_is_cursable(i);

    default:
        return false;
    }
}

static void _get_inv_items_to_show(vector<const item_def*> &v,
                                   int selector, int excluded_slot)
{
    for (const auto &item : you.inv)
    {
        if (item.defined()
            && item.link != excluded_slot
            && item_is_selected(item, selector))
        {
            v.push_back(&item);
        }
    }
}


/**
 * Does the player have any items of the given type?
 *
 * @param selector          A object_selector.
 * @param excluded_slot     An item slot to ignore.
 * @param inspect_floor     If true, also check the floor where the player is
 *                          standing.
 *
 * @return                  Whether there are any items matching the given
 *                          selector in the player's inventory.
 */
bool any_items_of_type(int selector, int excluded_slot, bool inspect_floor)
{
    bool ret = any_of(begin(you.inv), end(you.inv),
                  [=] (const item_def &item) -> bool
                  {
                      return item.defined() && item.link != excluded_slot
                          && item_is_selected(item, selector);
                  });
    if (!ret && inspect_floor)
    {
        auto item_floor = item_list_on_square(you.visible_igrd(you.pos()));
        ret = any_of(begin(item_floor), end(item_floor),
                      [=] (const item_def* item) -> bool
                      {
                          return item->defined()
                                    && item_is_selected(*item, selector);
                      });
    }
    return ret;
}

// Use title = nullptr for stock Inventory title
// type = MT_DROP allows the multidrop toggle
static unsigned char _invent_select(const char *title = nullptr,
                                    menu_type type = MT_INVLIST,
                                    int item_selector = OSEL_ANY,
                                    int excluded_slot = -1,
                                    int flags = MF_NOSELECT,
                                    invtitle_annotator titlefn = nullptr,
                                    vector<SelItem> *items = nullptr,
                                    vector<text_pattern> *filter = nullptr,
                                    Menu::selitem_tfn selitemfn = nullptr,
                                    const vector<SelItem> *pre_select = nullptr)
{
    InvMenu menu(flags | MF_ALLOW_FORMATTING);

    menu.set_preselect(pre_select);
    menu.set_title_annotator(titlefn);
    menu.f_selitem = selitemfn;
    if (filter)
        menu.set_select_filter(*filter);
    menu.load_inv_items(item_selector, excluded_slot);
    menu.set_type(type);

    // Don't override title if there are no items.
    if (title && menu.item_count())
        menu.set_title(title);

    menu.show(true);

    if (items)
        *items = menu.get_selitems();

    return menu.getkey();
}

void display_inventory()
{
    int flags = MF_SINGLESELECT;
    if (you.pending_revival || crawl_state.updating_scores)
        flags |= MF_EASY_EXIT;

    while (true)
    {
        unsigned char select =
            _invent_select(nullptr, MT_INVLIST, OSEL_ANY, -1, flags);

        if (isaalpha(select))
        {
            const int invidx = letter_to_index(select);
            if (you.inv[invidx].defined())
            {
                if (!describe_item(you.inv[invidx]))
                    break;
            }
        }
        else
            break;
    }

    if (!crawl_state.doing_prev_cmd_again)
        redraw_screen();
}

// Reads in digits for a count and apprends then to val, the
// return value is the character that stopped the reading.
static unsigned char _get_invent_quant(unsigned char keyin, int &quant)
{
    quant = keyin - '0';

    while (true)
    {
        keyin = get_ch();

        if (!isadigit(keyin))
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

    return keyin;
}

static string _drop_selitem_text(const vector<MenuEntry*> *s)
{
    bool extraturns = false;

    if (s->empty())
        return "";

    for (MenuEntry *entry : *s)
    {
        const item_def *item = static_cast<item_def *>(entry->data);
        const int eq = get_equip_slot(item);
        if (eq > EQ_WEAPON && eq < NUM_EQUIP)
        {
            extraturns = true;
            break;
        }
    }

    return make_stringf(" (%u%s turn%s)",
                        (unsigned int)s->size(),
                        extraturns? "+" : "",
                        s->size() > 1? "s" : "");
}

/**
 * Prompt the player to select zero or more items to drop.
 * TODO: deduplicate/merge with prompt_invent_item().
 *
 * @param   Items already selected to drop.
 * @return  The total set of items the player's chosen to drop.
 */
vector<SelItem> prompt_drop_items(const vector<SelItem> &preselected_items)
{
    const string prompt = "Drop what? " + slot_description()
#ifdef TOUCH_UI
                          + " (<Enter> or tap header to drop)"
#else
                          + " (_ for help)"
#endif
    ;

    unsigned char  keyin = '?';
    int            ret = PROMPT_ABORT;

    bool           need_redraw = false;
    bool           need_prompt = true;
    bool           need_getch  = false;

    vector<SelItem> items;
    int count = -1;
    while (true)
    {
        if (need_redraw && !crawl_state.doing_prev_cmd_again)
        {
            redraw_screen();
            clear_messages();
        }

        if (need_prompt)
        {
            mprf(MSGCH_PROMPT, "%s (<w>?</w> for menu, <w>Esc</w> to quit)",
                 prompt.c_str());
        }

        if (need_getch)
            keyin = get_ch();

        need_redraw = false;
        need_prompt = true;
        need_getch  = true;

        // Note:  We handle any "special" character first, so that
        //        it can be used to override the others.
        if (keyin == '?' || keyin == '*' || keyin == ',')
        {
            // The "view inventory listing" mode.
            const int ch = _invent_select(prompt.c_str(),
                                          MT_DROP,
                                          OSEL_ANY,
                                          -1,
                                          MF_MULTISELECT | MF_ALLOW_FILTER,
                                          nullptr, &items,
                                          &Options.drop_filter,
                                          _drop_selitem_text,
                                          &preselected_items);

            if (key_is_escape(ch))
            {
                keyin       = ch;
                need_prompt = false;
                need_getch  = false;
            }
            else
            {
                keyin       = 0;
                need_prompt = true;
                need_getch  = true;
            }

            if (!items.empty())
            {
                if (!crawl_state.doing_prev_cmd_again)
                {
                    redraw_screen();
                    clear_messages();
                }

                for (SelItem &sel : items)
                    sel.slot = letter_to_index(sel.slot);
                return items;
            }

            need_redraw = !(keyin == '?' || keyin == '*'
                            || keyin == ',' || keyin == '+');
        }
        else if (isadigit(keyin))
        {
            // The "read in quantity" mode
            keyin = _get_invent_quant(keyin, count);

            need_prompt = false;
            need_getch  = false;
        }
        else if (key_is_escape(keyin)
                || (Options.easy_quit_item_prompts && keyin == ' '))
        {
            ret = PROMPT_ABORT;
            break;
        }
        else if (isaalpha(keyin))
        {
            ret = letter_to_index(keyin);

            if (!you.inv[ret].defined())
                mpr("You don't have any such object.");
            else
                break;
        }
        else if (!isspace(keyin))
        {
            // We've got a character we don't understand...
            canned_msg(MSG_HUH);
        }
        else
        {
            // We're going to loop back up, so don't draw another prompt.
            need_prompt = false;
        }
    }

    if (ret != PROMPT_ABORT)
    {
        items.emplace_back(ret, count,
                           ret != PROMPT_GOT_SPECIAL ? &you.inv[ret] : nullptr);
    }
    return items;
}

int digit_inscription_to_inv_index(char digit, operation_types oper)
{
    const char iletter = static_cast<char>(oper);

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (you.inv[i].defined())
        {
            const string& r(you.inv[i].inscription);
            // Note that r.size() is unsigned.
            for (unsigned int j = 0; j + 2 < r.size(); ++j)
            {
                if (r[j] == '@'
                     && (r[j+1] == iletter || r[j+1] == '*')
                     && r[j+2] == digit)
                {
                    return i;
                }
            }
        }
    }
    return -1;
}

static bool _has_warning_inscription(const item_def& item,
                             operation_types oper)
{
    const char iletter = static_cast<char>(oper);

    const string& r(item.inscription);
    for (unsigned int i = 0; i + 1 < r.size(); ++i)
    {
        if (r[i] == '!')
        {
            if (r[i+1] == iletter || r[i+1] == '*')
                return true;
            else if (oper == OPER_ZAP && r[i+1] == 'z') // for the 0.3.4. keys
                return true;
            else if (oper == OPER_EVOKE
                     && (r[i+1] == 'V' || toalower(r[i+1]) == 'z'))
            {
                return true;
            }
        }
    }

    return false;
}

// In order to equip this item, we may need to remove an old item in the
// corresponding slot which has a warning inscription. If this is the case,
// prompt the user for confirmation.
bool check_old_item_warning(const item_def& item,
                             operation_types oper)
{
    item_def old_item;
    string prompt;
    bool penance = false;
    if (oper == OPER_WIELD) // can we safely unwield old item?
    {
        if (!you.weapon())
            return true;

        int equip = you.equip[EQ_WEAPON];
        if (equip == -1 || item.link == equip)
            return true;

        old_item = *you.weapon();
        if (!needs_handle_warning(old_item, OPER_WIELD, penance))
            return true;

        prompt += "Really unwield ";
    }
    else if (oper == OPER_WEAR) // can we safely take off old item?
    {
        if (item.base_type != OBJ_ARMOUR)
            return true;

        equipment_type eq_slot = get_armour_slot(item);
        int equip = you.equip[eq_slot];
        if (equip == -1 || item.link == equip)
            return true;

        old_item = you.inv[you.equip[eq_slot]];

        if (!needs_handle_warning(old_item, OPER_TAKEOFF, penance))
            return true;

        prompt += "Really take off ";
    }
    else if (oper == OPER_PUTON) // can we safely remove old item?
    {
        if (item.base_type != OBJ_JEWELLERY)
            return true;

        if (jewellery_is_amulet(item))
        {
            int equip = you.equip[EQ_AMULET];
            if (equip == -1 || item.link == equip)
                return true;

            old_item = you.inv[equip];
            if (!needs_handle_warning(old_item, OPER_TAKEOFF, penance))
                return true;

            prompt += "Really remove ";
        }
        else // rings handled in prompt_ring_to_remove
            return true;
    }
    else // anything else doesn't have a counterpart
        return true;

    // now ask
    prompt += old_item.name(DESC_INVENTORY);
    prompt += "?";
    if (penance)
        prompt += " This could place you under penance!";
    return yesno(prompt.c_str(), false, 'n');
}

static string _operation_verb(operation_types oper)
{
    switch (oper)
    {
    case OPER_WIELD:          return "wield";
    case OPER_QUAFF:          return "quaff";
    case OPER_DROP:           return "drop";
    case OPER_EAT:            return you.species == SP_VAMPIRE ?
                                      "drain" : "eat";
    case OPER_TAKEOFF:        return "take off";
    case OPER_WEAR:           return "wear";
    case OPER_PUTON:          return "put on";
    case OPER_REMOVE:         return "remove";
    case OPER_READ:           return "read";
    case OPER_MEMORISE:       return "memorise from";
    case OPER_ZAP:            return "zap";
    case OPER_FIRE:           return "fire";
    case OPER_EVOKE:          return "evoke";
    case OPER_DESTROY:        return "destroy";
    case OPER_QUIVER:         return "quiver";
    case OPER_ANY:
    default:
        return "choose";
    }
}

static bool _is_wielded(const item_def &item)
{
    int equip = you.equip[EQ_WEAPON];
    return equip != -1 && item.link == equip;
}

static bool _is_known_no_tele_item(const item_def &item)
{
    if (!is_artefact(item))
        return false;

    bool known;
    return artefact_property(item, ARTP_PREVENT_TELEPORTATION, known) && known;
}

bool needs_notele_warning(const item_def &item, operation_types oper)
{
    return (oper == OPER_PUTON || oper == OPER_WEAR
                || oper == OPER_WIELD && !_is_wielded(item))
                && (_is_known_no_tele_item(item) && you.duration[DUR_TELEPORT]);
}

bool needs_handle_warning(const item_def &item, operation_types oper,
                          bool &penance)
{
    if (_has_warning_inscription(item, oper))
        return true;

    // Curses first.
    if (item_known_cursed(item)
        && (oper == OPER_WIELD && is_weapon(item) && !_is_wielded(item)
            || oper == OPER_PUTON || oper == OPER_WEAR))
    {
        return true;
    }

    // The consequences of evokables are generally known unless it's a deck
    // and you don't know what kind of a deck it is.
    if (item.base_type == OBJ_MISCELLANY && !is_deck(item)
        && oper == OPER_EVOKE && god_hates_item(item))
    {
        penance = true;
        return true;
    }

    // You know that mutagenic chunks mutate you.
    if (oper == OPER_EAT && you_worship(GOD_ZIN) && god_hates_item(item))
    {
        penance = true;
        return true;
    }

    // Everything else depends on knowing the item subtype/brand.
    if (!item_type_known(item))
        return false;

    if (oper == OPER_REMOVE
        && item.is_type(OBJ_JEWELLERY, AMU_FAITH)
        && !(you_worship(GOD_RU) && you.piety >= piety_breakpoint(5))
        && !you_worship(GOD_GOZAG)
        && !you_worship(GOD_NO_GOD)
        && !you_worship(GOD_XOM))
    {
        return true;
    }

    if (oper == OPER_REMOVE && item.is_type(OBJ_JEWELLERY, AMU_HARM))
        return true;

    if (needs_notele_warning(item, oper))
        return true;

    if (oper == OPER_ATTACK && god_hates_item(item)
        && !you_worship(GOD_PAKELLAS))
    {
        penance = true;
        return true;
    }

    if (oper == OPER_WIELD // unwielding uses OPER_WIELD too
        && is_weapon(item))
    {
        if (get_weapon_brand(item) == SPWPN_DISTORTION
            && !have_passive(passive_t::safe_distortion))
        {
            return true;
        }

        if (get_weapon_brand(item) == SPWPN_VAMPIRISM
            && you.undead_state() == US_ALIVE
            && !you_foodless()
            // Don't prompt if you aren't wielding it and you can't.
            && (you.hunger_state >= HS_FULL || _is_wielded(item)))
        {
            return true;
        }

        if (is_artefact(item) && artefact_property(item, ARTP_CONTAM))
        {
            if (_is_wielded(item) && you_worship(GOD_ZIN))
                penance = true;
            return true;
        }

        if (is_artefact(item) && (artefact_property(item, ARTP_DRAIN)
                                  || artefact_property(item, ARTP_FRAGILE)))
        {
            return true;
        }
    }

    if (oper == OPER_PUTON || oper == OPER_WEAR || oper == OPER_TAKEOFF
        || oper == OPER_REMOVE)
    {
        if (is_artefact(item) && artefact_property(item, ARTP_CONTAM))
        {
            if ((oper == OPER_TAKEOFF || oper == OPER_REMOVE)
                 && you_worship(GOD_ZIN))
            {
                penance = true;
            }
            return true;
        }

        if (is_artefact(item) && (artefact_property(item, ARTP_DRAIN)
                                  || artefact_property(item, ARTP_FRAGILE)))
        {
            return true;
        }
    }

    if (oper == OPER_EVOKE && god_hates_item(item))
    {
        penance = true;
        return true;
    }

    return false;
}

// If there are warning inscriptions associated with performing this operation
// on this item, prompt the user for confirmation. Return true if all prompts
// are OK'd.
bool check_warning_inscriptions(const item_def& item,
                                 operation_types oper)
{
    bool penance = false;
    if (item.defined()
        && needs_handle_warning(item, oper, penance))
    {
        // When it's about destroying an item, don't even ask.
        // If the player really wants to do that, they'll have
        // to remove the inscription.
        if (oper == OPER_DESTROY)
            return false;

        // Common pattern for wield/wear/put:
        // - if the player isn't capable of equipping it, return true
        //   immediately. No point warning, since the op is impossible.
        // - if the item is already worn, treat this as the corresponding
        //   unequip operation
        if (oper == OPER_WIELD)
        {
            // Can't use can_wield in item-use.cc because it wants
            // a non-const item_def.
            if (!you.can_wield(item))
                return true;

            int equip = you.equip[EQ_WEAPON];
            if (equip != -1 && item.link == equip)
                return check_old_item_warning(item, oper);
        }
        else if (oper == OPER_WEAR)
        {
            if (!can_wear_armour(item, false, false))
                return true;

            int equip = you.equip[get_armour_slot(item)];
            if (equip != -1 && item.link == equip)
                return check_old_item_warning(item, oper);
        }
        else if (oper == OPER_PUTON)
        {
            if (item.base_type != OBJ_JEWELLERY)
                return true;

            if (jewellery_is_amulet(item))
            {
                int equip = you.equip[EQ_AMULET];
                if (equip != -1 && item.link == equip)
                    return check_old_item_warning(item, oper);
            }
            else
            {
                for (int slots = EQ_FIRST_JEWELLERY; slots <= EQ_LAST_JEWELLERY; ++slots)
                {
                    if (slots == EQ_AMULET)
                        continue;

                    int equip = you.equip[slots];
                    if (equip != -1 && item.link == equip)
                        return check_old_item_warning(item, oper);
                }
            }
        }
        else if (oper == OPER_REMOVE || oper == OPER_TAKEOFF)
        {
            // Don't ask if it will fail anyway.
            if (item.cursed())
                return true;
        }

        // XXX: duplicates a check in delay.cc:_finish_delay()
        string prompt = "Really " + _operation_verb(oper) + " ";
        prompt += (in_inventory(item) ? item.name(DESC_INVENTORY)
                                      : item.name(DESC_A));
        if (needs_notele_warning(item, oper)
            && item_ident(item, ISFLAG_KNOW_TYPE))
        {
            prompt += " while about to teleport";
        }
        prompt += "?";
        if (penance)
            prompt += " This could place you under penance!";
        return yesno(prompt.c_str(), false, 'n')
               && check_old_item_warning(item, oper);
    }
    else
        return check_old_item_warning(item, oper);
}

/**
 * Prompts the user for an item.
 *
 * Prompts the user for an item, handling the '?' and '*' listings,
 * and returns the inventory slot to the caller (which if
 * must_exist is true (the default) will be an assigned item), with
 * a positive quantity.
 *
 * Note: Does not check if the item is appropriate.
 *
 * @param prompt           The question to ask the user.
 * @param mtype            The menu type.
 * @param type_expect      The object_class_type or object_selector for
 *                         items to be listed.
 * @param oper             The operation_type that will be used on the result.
 *                         Modifies some checks, including applicability of
 *                         warning inscriptions.
 * @param flags            See comments on invent_prompt_flags.
 * @param other_valid_char A character that, if not '\0', will cause
 *                         PROMPT_GOT_SPECIAL to be returned when pressed.
 *
 * @return  the inventory slot of an item or one of the following special values
 *          - PROMPT_ABORT:       if the player hits escape.
 *          - PROMPT_GOT_SPECIAL: if the player hits the "other_valid_char".
 *          - PROMPT_NOTHING:     if there are no matching items.
 */
int prompt_invent_item(const char *prompt,
                       menu_type mtype, int type_expect,
                       operation_types oper,
                       invent_prompt_flags flags,
                       const char other_valid_char)
{
    const bool do_warning = !(flags & invprompt_flag::no_warning);
    const bool allow_list_known = !(flags & invprompt_flag::hide_known);
    const bool must_exist = !(flags & invprompt_flag::unthings_ok);
    const bool auto_list = !(flags & invprompt_flag::manual_list);
    const bool allow_easy_quit = !(flags & invprompt_flag::escape_only);

    if (!any_items_of_type(type_expect)
        && type_expect == OSEL_THROWABLE
        && (oper == OPER_FIRE || oper == OPER_QUIVER)
        && mtype == MT_INVLIST)
    {
        type_expect = OSEL_ANY;
    }

    if (!any_items_of_type(type_expect) && type_expect != OSEL_WIELD)
    {
        mprf(MSGCH_PROMPT, "%s",
             no_selectables_message(type_expect).c_str());
        return PROMPT_NOTHING;
    }

    unsigned char  keyin = 0;
    int            ret = PROMPT_ABORT;

    int current_type_expected = type_expect;
    bool           need_redraw = false;
    bool           need_prompt = true;
    bool           need_getch  = true;

    if (auto_list)
    {
        need_getch = false;

        if (any_items_of_type(type_expect))
            keyin = '?';
        else
            keyin = '*';
    }

    while (true)
    {
        if (need_redraw && !crawl_state.doing_prev_cmd_again)
        {
            redraw_screen();
            clear_messages();
        }

        if (need_prompt)
        {
            mprf(MSGCH_PROMPT, "%s (<w>?</w> for menu, <w>Esc</w> to quit)",
                 prompt);
        }
        else
            flush_prev_message();

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
            vector< SelItem > items;
            current_type_expected = keyin == '*' ? OSEL_ANY : type_expect;
            keyin = _invent_select(
                        prompt,
                        mtype,
                        current_type_expected,
                        -1,
                        MF_SINGLESELECT | MF_ANYPRINTABLE | MF_NO_SELECT_QTY
                            | MF_EASY_EXIT,
                        nullptr,
                        &items);

            if (allow_list_known && keyin == '\\')
            {
                check_item_knowledge();
                keyin = '?';
            }

            need_prompt = false;
            need_getch  = false;

            // Don't redraw if we're just going to display another listing
            need_redraw = keyin != '?' && keyin != '*';

            if (!items.empty())
            {
                if (!crawl_state.doing_prev_cmd_again)
                {
                    redraw_screen();
                    clear_messages();
                }
            }
        }
        else if (isadigit(keyin))
        {
            // scan for our item
            int res = digit_inscription_to_inv_index(keyin, oper);
            if (res != -1)
            {
                ret = res;
                if (!do_warning || check_warning_inscriptions(you.inv[ret], oper))
                    break;
            }
        }
        else if (key_is_escape(keyin)
                 || (Options.easy_quit_item_prompts
                     && allow_easy_quit && keyin == ' '))
        {
            ret = PROMPT_ABORT;
            break;
        }
        else if (allow_list_known && keyin == '\\')
        {
            check_item_knowledge();
            keyin = '?';
            need_getch = false;
        }
        else if (isaalpha(keyin))
        {
            ret = letter_to_index(keyin);

            if (must_exist && !you.inv[ret].defined())
                mpr("You don't have any such object.");
            else if (must_exist && !item_is_selected(you.inv[ret],
                                                     current_type_expected))
            {
                mpr("That's the wrong kind of item! (Use * to select it.)");
            }
            else if (!do_warning || check_warning_inscriptions(you.inv[ret], oper))
                break;
        }
        else if (!isspace(keyin))
        {
            // We've got a character we don't understand...
            canned_msg(MSG_HUH);
        }
        else
        {
            // We're going to loop back up, so don't draw another prompt.
            need_prompt = false;
        }
    }

    return ret;
}

bool prompt_failed(int retval)
{
    if (retval != PROMPT_ABORT && retval != PROMPT_NOTHING)
        return false;

    if (retval == PROMPT_ABORT)
        canned_msg(MSG_OK);

    crawl_state.cancel_cmd_repeat();

    return true;
}

// Most items are wieldable, but this function check for items that needs to be
// wielded to be used normally.
bool item_is_wieldable(const item_def &item)
{
    return is_weapon(item) && you.species != SP_FELID;
}

/**
 * Return whether an item can be evoked.
 *
 * @param item      The item to check
 * @param reach     Do weapons of reaching count?
 * @param known     When set, return true for items of unknown type which
 *                  might be evokable.
 * @param all_wands When set, return true for empty wands.
 * @param msg       Whether we need to print a message.
 * @param equip     When false, ignore wield and meld requirements.
 */
bool item_is_evokable(const item_def &item, bool reach, bool known,
                      bool all_wands, bool msg, bool equip)
{
    const string error = item_is_melded(item)
            ? "Your " + item.name(DESC_QUALNAME) + " is melded into your body."
            : "That item can only be evoked when wielded.";

    const bool no_evocables = player_mutation_level(MUT_NO_ARTIFICE);
    const char* const no_evocable_error = "You cannot evoke magical items.";

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.unrand_idx);

        if (entry->evoke_func && item_type_known(item))
        {
            if (no_evocables)
            {
                if (msg)
                    mpr(no_evocable_error);
                return false;
            }

            if (item_is_equipped(item) && !item_is_melded(item) || !equip)
                return true;

            if (msg)
                mpr(error);

            return false;
        }
        // Unrandart might still be evokable (e.g., reaching)
    }

    if (no_evocables
        && item.base_type != OBJ_WEAPONS // reaching is ok.
        && !(item.base_type == OBJ_MISCELLANY
             && (item.sub_type == MISC_ZIGGURAT
                 || is_deck(item)))) // decks and zigfigs are OK.
    {
        // the rest are forbidden under sac evocables.
        if (msg)
            mpr(no_evocable_error);
        return false;
    }

    const bool wielded = !equip || you.equip[EQ_WEAPON] == item.link
                                   && !item_is_melded(item);

    switch (item.base_type)
    {
    case OBJ_WANDS:
        if (all_wands)
            return true;

        if (item.used_count == ZAPCOUNT_EMPTY)
        {
            if (msg)
                mpr("This wand has no charges.");
            return false;
        }
        return true;

    case OBJ_WEAPONS:
        if ((!wielded || !reach) && !msg)
            return false;

        if (reach && weapon_reach(item) > REACH_NONE && item_type_known(item))
        {
            if (!wielded)
            {
                if (msg)
                    mpr(error);
                return false;
            }
            return true;
        }

        if (msg)
            mpr("That item cannot be evoked!");
        return false;

    case OBJ_STAVES:
        if (known && !item_type_known(item)
            || item.sub_type == STAFF_ENERGY
               && item_type_known(item))
        {
            if (!wielded)
            {
                if (msg)
                    mpr(error);
                return false;
            }
            return true;
        }
        if (msg)
            mpr("That item cannot be evoked!");
        return false;

#if TAG_MAJOR_VERSION == 34
    case OBJ_MISCELLANY:
        if (item.sub_type != MISC_BUGGY_LANTERN_OF_SHADOWS
            && item.sub_type != MISC_BUGGY_EBONY_CASKET
            )
        {
            return true;
        }
#endif
        // removed items fallthrough to failure

    default:
        if (msg)
            mpr("That item cannot be evoked!");
        return false;
    }
}

/**
 * What xp-charged evocable items is the player currently devoting XP to, if
 * any?
 *
 * @param[out] evokers  A vector, to be filled with a list of the elemental
 *                      evokers that the player is currently charging.
 *                      (Only one of a type is charged at a time.)
 */
void list_charging_evokers(FixedVector<item_def*, NUM_MISCELLANY> &evokers)
{
    for (auto &item : you.inv)
    {
        // can't charge non-evokers, or evokers that are full
        if (!is_xp_evoker(item) || evoker_debt(item.sub_type) == 0)
            continue;

        evokers[item.sub_type] = &item;
    }
}

void identify_inventory()
{
    for (auto &item : you.inv)
    {
        if (item.defined())
        {
            set_ident_type(item, true);
            set_ident_flags(item, ISFLAG_IDENT_MASK);
        }
    }
}
