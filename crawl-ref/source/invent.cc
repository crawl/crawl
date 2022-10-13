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
#include "describe.h"
#include "env.h"
#include "evoke.h"
#include "god-item.h"
#include "god-passive.h"
#include "initfile.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "known-items.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "options.h"
#include "output.h"
#include "prompt.h"
#include "religion.h"
#include "rltiles/tiledef-dngn.h"
#include "rltiles/tiledef-icons.h"
#include "rltiles/tiledef-main.h"
#include "showsymb.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "throw.h"
#include "tilepick.h"
#include "tile-env.h"

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

string InvTitle::get_text() const
{
    return titlefn ? titlefn(m, MenuEntry::get_text())
                   : MenuEntry::get_text();
}

InvEntry::InvEntry(const item_def &i)
    : MenuEntry("", MEL_ITEM), item(&i), _has_star(false)
{
    indent_no_hotkeys = true;

    // Data is an inherited void *. When using InvEntry in menus
    // use the const item in this class whenever possible
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
        actor *trapped = actor_at(i.pos);
        text += make_stringf(" (holding %s)",
                            trapped ? trapped->name(DESC_A).c_str()
                                    : "nobody"); // buggy net, but don't crash
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
    return item->cursed();
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

bool InvEntry::has_star() const
{
    return _has_star;
}

string InvEntry::get_filter_text() const
{
    return item_prefix(*item, false) + " " + get_text();
}

string InvEntry::_get_text_preface() const
{
    ostringstream tstr;

    const bool nosel = hotkeys_count() == 0;
    if (nosel && tag != "pickup")
        return MenuEntry::_get_text_preface();
    const char key = nosel ? ' ' : static_cast<char>(hotkeys[0]);

    tstr << ' ' << key << ' ';

    if (nosel)
        tstr << ' '; // pickup only
    else if (!selected_qty)
        tstr << '-';
    else if (selected_qty < quantity)
        tstr << '#';
    else if (_has_star)
        tstr << '*';
    else
        tstr << '+';

    tstr << ' ';

    if (InvEntry::show_glyph)
        tstr << "(" << glyph_to_tagstr(get_item_glyph(*item)) << ")" << " ";

    if (InvEntry::show_coordinates && in_bounds(item->pos))
    {
        const coord_def relpos = item->pos - you.pos();
        tstr << "(" << relpos.x << ", " << -relpos.y << ")" << " ";
    }

    return tstr.str();
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

bool InvEntry::show_glyph = false;
void InvEntry::set_show_glyph(bool doshow)
{
    show_glyph = doshow;
}

bool InvEntry::show_coordinates = false;
void InvEntry::set_show_coordinates(bool doshow)
{
    show_coordinates = doshow;
}

InvMenu::InvMenu(int mflags)
    : Menu((mflags & MF_NOSELECT) ? mflags : (mflags | MF_ARROWS_SELECT),
                "inventory"),
        type(menu_type::invlist), pre_select(nullptr),
        title_annotate(nullptr), _mode_special_drop(false)
{
    menu_action = ACT_EXAMINE; // default
#ifdef USE_TILE_LOCAL
    if (Options.tile_menu_icons)
        set_flags(get_flags() | MF_USE_TWO_COLUMNS);
#endif
}

bool InvMenu::mode_special_drop() const
{
    return _mode_special_drop;
}

void InvMenu::set_type(menu_type t)
{
    type = t;
    menu_action = t == menu_type::describe ? ACT_EXAMINE : ACT_EXECUTE;
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

bool InvMenu::skip_process_command(int keyin)
{
    switch (keyin)
    {
    case '?':
    case '!':
        // item type shortcuts
        return true;
    }
    return Menu::skip_process_command(keyin);
}

int InvMenu::pre_process(int key)
{
    if (type == menu_type::drop && key == '\\')
    {
        _mode_special_drop = !_mode_special_drop;
        key = CK_NO_KEY;
    }
    else if (key == ';'
             && you.last_unequip != -1
             && (type == menu_type::drop || type == menu_type::invlist))
    {
        key = index_to_letter(you.last_unequip);
    }
    else if (key == '-')
        _mode_special_drop = false;
    return key;
}

static bool _item_is_permadrop_candidate(const item_def &item)
{
    // Known, non-artefact items of the types you see on the '\' menu proper.
    // (No disabling autopickup for "green fizzy potion", "+3 whip", etc.)
    if (item_type_unknown(item))
        return false;
    return item.base_type == OBJ_MISCELLANY
        || is_stackable_item(item)
        || item_type_has_ids(item.base_type);
}

void InvMenu::select_item_index(int idx, int qty)
{
    if (type != menu_type::drop)
        return Menu::select_item_index(idx, qty);

    InvEntry *ie = static_cast<InvEntry*>(items[idx]);

    bool should_toggle_star = _item_is_permadrop_candidate(ie->item[0])
        && (ie->has_star() || _mode_special_drop);

    if (should_toggle_star)
    {
        // Toggle starred items back to selected-but-not-starred in this mode
        // instead of turning them all the way off.
        qty = _mode_special_drop ? -2 : 0;
        ie->set_star(!ie->has_star());
    }
    Menu::select_item_index(idx, qty);
}

bool InvMenu::examine_index(int i)
{
    const bool do_actions = type == menu_type::describe;
    // not entirely sure if the bounds check is necessary
    auto ie = (i >= 0 && i < static_cast<int>(items.size()))
                    ? dynamic_cast<InvEntry *>(items[i])
                    : nullptr;

    // superclass behavior: do nothing unless on_examine is defined, in which
    // case call on_examine.
    if (!ie || on_examine)
        return Menu::examine_index(i);
    else if (type == menu_type::pickup)
    {
        // item is a floor item.
        auto desc_tgt = const_cast<item_def*>(ie->item);
        ASSERT(desc_tgt);
        return describe_item(*desc_tgt, nullptr, do_actions);
    }
    else if (ie->hotkeys.size())
    {
        // default behavior: examine inv item. You must override or use on_examine
        // if your items come from somewhere else, or this will cause crashes!
        unsigned char select = ie->hotkeys[0];
        const int invidx = letter_to_index(select);
        ASSERT(you.inv[invidx].defined());
        return describe_item(you.inv[invidx], nullptr, do_actions);
    }
    // nothing to describe, ignore
    return true;
}

void InvEntry::set_star(bool val)
{
    _has_star = val;
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
    case OSEL_ENCHANTABLE_ARMOUR:
        return "You aren't carrying any armour which can be enchanted further.";
    case OBJ_CORPSES:
        return "You don't have any corpses.";
    case OBJ_POTIONS:
        return "You aren't carrying any potions.";
    case OBJ_SCROLLS:
        return "You aren't carrying any scrolls.";
    case OBJ_BOOKS:
        return "You don't have any books.";
    case OBJ_WANDS:
        return "You aren't carrying any wands.";
    case OBJ_JEWELLERY:
        return "You aren't carrying any pieces of jewellery.";
    case OSEL_LAUNCHING:
        return "You aren't carrying any items that might be thrown or fired.";
    case OSEL_EVOKABLE:
        return "You aren't carrying any items that you can evoke.";
    case OSEL_CURSED_WORN:
        return "None of your equipped items are cursed.";
#if TAG_MAJOR_VERSION == 34
    case OSEL_UNCURSED_WORN_ARMOUR:
        return "You aren't wearing any piece of uncursed armour.";
    case OSEL_UNCURSED_WORN_JEWELLERY:
        return "You aren't wearing any piece of uncursed jewellery.";
#endif
    case OSEL_WORN_ARMOUR:
        return "You aren't wearing any pieces of armour.";
    case OSEL_BRANDABLE_WEAPON:
        return "You aren't carrying any weapons that can be branded.";
    case OSEL_ENCHANTABLE_WEAPON:
        return "You aren't carrying any weapons that can be enchanted.";
    case OSEL_BEOGH_GIFT:
        return "You aren't carrying anything you can give to a follower.";
    case OSEL_CURSABLE:
        return "You aren't wearing any cursable items.";
    case OSEL_UNCURSED_WORN_RINGS:
        return "You aren't wearing any uncursed rings.";
    case OSEL_QUIVER_ACTION:
    case OSEL_QUIVER_ACTION_FORCE:
        return "You don't have any quiverable items.";
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

bool get_tiles_for_item(const item_def &item, vector<tile_def>& tileset, bool show_background)
{
    tileidx_t idx = tileidx_item(get_item_known_info(item));
    if (!idx)
        return false;

    if (in_inventory(item))
    {
        const equipment_type eq = item_equip_slot(item);
        if (eq != EQ_NONE)
        {
            if (item.cursed())
                tileset.emplace_back(TILE_ITEM_SLOT_EQUIP_CURSED);
            else
                tileset.emplace_back(TILE_ITEM_SLOT_EQUIP);
        }
        else if (item.cursed())
            tileset.emplace_back(TILE_ITEM_SLOT_CURSED);

        tileidx_t base_item = tileidx_known_base_item(idx);
        if (base_item)
            tileset.emplace_back(base_item);
        tileset.emplace_back(idx);

        if (eq != EQ_NONE && you.melded[eq])
            tileset.emplace_back(TILEI_MESH);
    }
    else
    {
        // Do we want to display the floor type or is that too distracting?
        const coord_def c = item.held_by_monster()
            ? item.holding_monster()->pos()
            : item.pos;
        tileidx_t ch = 0;
        if (c != coord_def() && show_background && item.link != ITEM_IN_SHOP)
        {
            ch = tileidx_feature(c);
            if (ch == TILE_FLOOR_NORMAL)
                ch = tile_env.flv(c).floor;
            else if (ch == TILE_WALL_NORMAL)
                ch = tile_env.flv(c).wall;

            tileset.emplace_back(ch);
        }
        tileidx_t base_item = tileidx_known_base_item(idx);
        if (base_item)
            tileset.emplace_back(base_item);

        tileset.emplace_back(idx);

        if (ch != 0)
        {
            // Needs to be displayed so as to not give away mimics in shallow water.
            if (ch == TILE_DNGN_SHALLOW_WATER)
                tileset.emplace_back(TILEI_MASK_SHALLOW_WATER);
            else if (ch == TILE_DNGN_SHALLOW_WATER_MURKY)
                tileset.emplace_back(TILEI_MASK_SHALLOW_WATER_MURKY);
        }
    }
    if (item.base_type == OBJ_WEAPONS || item.base_type == OBJ_MISSILES
        || item.base_type == OBJ_ARMOUR
#if TAG_MAJOR_VERSION == 34
        || item.base_type == OBJ_RODS
#endif
       )
    {
        tileidx_t brand = tileidx_known_brand(item);
        if (brand)
            tileset.emplace_back(brand);
    }
    else if (item.base_type == OBJ_CORPSES)
    {
        tileidx_t brand = tileidx_corpse_brand(item);
        if (brand)
            tileset.emplace_back(brand);
    }

    return true;
}

#ifdef USE_TILE
bool InvEntry::get_tiles(vector<tile_def>& tileset) const
{
    if (!Options.tile_menu_icons)
        return false;

    // Runes + orb of zot have a special uncollected tile
    if (quantity <= 0 && (item->base_type != OBJ_RUNES && item->base_type != OBJ_ORBS))
        return false;

    return get_tiles_for_item(*item, tileset, show_background);
}
#else
bool InvEntry::get_tiles(vector<tile_def>& /*tileset*/) const { return false; }
#endif

bool InvMenu::is_selectable(int index) const
{
    if (type == menu_type::drop)
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
           || !item_is_evokable(*(a->item));
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
#if TAG_MAJOR_VERSION == 34
    OBJ_FOOD,
#endif
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
    if (sort)
        cond = find_menu_sort_condition();

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
                                    inv->item, inv->has_star());
    }
    return selected_items;
}

string InvMenu::help_key() const
{
    return type == menu_type::drop || type == menu_type::pickup ? "pick-up"
                                                                : "";
}

int InvMenu::getkey() const
{
    auto mkey = lastch;

    if (is_set(MF_ARROWS_SELECT) && mkey == CK_ENTER
        || mkey == CK_MOUSE_B1)
    {
        return mkey;
    }
    if (type == menu_type::know && mkey == 0) // ??
        return mkey;

    // this is sort of a mess. It seems to be converting a lot of keys to ' '
    // so that invprompt_flag::escape_only can work right, but it almost
    // certainly has other effects. Needless to say, it makes modifying key
    // handling in specific menus pretty annoying, but I don't dare touch it
    // right now.
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
#if TAG_MAJOR_VERSION == 34
        case OBJ_FOOD:       return "Comestibles";
#endif
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
                             menu_type mtype)
{
    vector<SelItem> selected;
    if (!items.empty())
    {
        InvMenu menu;
        menu.set_type(mtype);
        menu.set_title(title);
        if (mtype == menu_type::pickup)
            menu.set_tag("pickup");

        menu.load_items(items);
        int new_flags = noselect ? MF_NOSELECT
                            : MF_MULTISELECT | MF_ALLOW_FILTER;

        if (mtype == menu_type::sel_one)
        {
            new_flags |= MF_SINGLESELECT;
            new_flags &= ~MF_MULTISELECT;
        }

        if (!!(new_flags & MF_MULTISELECT))
            new_flags |= MF_SELECT_QTY;

        new_flags |= MF_ALLOW_FORMATTING | MF_ARROWS_SELECT;
        new_flags |= menu.get_flags() & MF_USE_TWO_COLUMNS;
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
                                && itype != OBJ_ARMOUR)
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

    case OSEL_LAUNCHING:
        return itype == OBJ_MISSILES
                        && is_launched(&you, i) != launch_retval::FUMBLED
                || itype == OBJ_WEAPONS && is_range_weapon(i)
                                        && item_is_equipped(i);

    case OBJ_WEAPONS:
    case OSEL_WIELD:
        return item_is_wieldable(i);

    case OSEL_EVOKABLE:
        // assumes valid link...would break with evoking from floor?
        return item_is_evokable(i);//evoke_check(i.link, true);

    case OSEL_ENCHANTABLE_ARMOUR:
        return is_enchantable_armour(i, true);

    case OSEL_CURSED_WORN:
        return i.cursed() && item_is_equipped(i);

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
        return is_brandable_weapon(i, you_worship(GOD_SHINING_ONE)
                    || you_worship(GOD_KIKUBAAQUDGHA), true);

    case OSEL_BEOGH_GIFT:
        return (itype == OBJ_WEAPONS
                || is_offhand(i)
                || itype == OBJ_ARMOUR
                   && get_armour_slot(i) == EQ_BODY_ARMOUR)
                && !item_is_equipped(i);

    case OSEL_CURSABLE:
        return item_is_equipped(i) && item_is_cursable(i);

    case OSEL_UNCURSED_WORN_RINGS:
        return !i.cursed() && item_is_equipped(i) && itype == OBJ_JEWELLERY
            && !jewellery_is_amulet(i);

    case OSEL_QUIVER_ACTION:
        if (in_inventory(i))
        {
            auto a = quiver::slot_to_action(i.link);
            // lots of things can be activated via the quiver, but don't have
            // a targeter -- ignore these.
            // However, we do want to allow selecting ammo/launchers under
            // confusion...
            // XX should the primary weapon be allowed here?
            return a->is_valid()
                && (a->is_targeted()
                    || you.confused() && item_is_selected(i, OSEL_LAUNCHING));
        }
        return false;
    case OSEL_QUIVER_ACTION_FORCE:
        return in_inventory(i) && quiver::slot_to_action(i.link, true)->is_valid();

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
// type = menu_type::drop allows the multidrop toggle
static int _invent_select(const char *title = nullptr,
                                    menu_type type = menu_type::invlist,
                                    int item_selector = OSEL_ANY,
                                    int excluded_slot = -1,
                                    int flags = MF_NOSELECT,
                                    invtitle_annotator titlefn = nullptr,
                                    vector<SelItem> *items = nullptr,
                                    vector<text_pattern> *filter = nullptr,
                                    Menu::selitem_tfn selitemfn = nullptr,
                                    const vector<SelItem> *pre_select = nullptr)
{
    InvMenu menu(flags | MF_ALLOW_FORMATTING | MF_INIT_HOVER);

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
    InvMenu menu(MF_SINGLESELECT | MF_ALLOW_FORMATTING | MF_SECONDARY_SCROLL);
    menu.load_inv_items(OSEL_ANY, -1);
    menu.set_type(menu_type::describe);

    menu.show(true);
    if (!crawl_state.doing_prev_cmd_again)
    {
        redraw_screen();
        update_screen();
    }
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

static string _drop_prompt(bool as_menu_title, bool menu_autopickup_mode)
{
    string prompt_base;

    if (as_menu_title && menu_autopickup_mode)
        prompt_base = "Drop (and turn off autopickup for) what? ";
    else if (as_menu_title)
        prompt_base = "Drop what?                               ";
    else
        prompt_base = "Drop what? ";
    return prompt_base + slot_description()
#ifdef TOUCH_UI
                          + " (<Enter> or tap header to drop)";
#else
                          + " (_ for help)";
#endif
}

static string _drop_menu_titlefn(const Menu *m, const string &)
{
    const InvMenu *invmenu = static_cast<const InvMenu *>(m);
    return _drop_prompt(true, invmenu->mode_special_drop());
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
    vector<SelItem> items;

    // multi-select some items to drop
    _invent_select("",
                      menu_type::drop,
                      OSEL_ANY,
                      -1,
                      MF_MULTISELECT | MF_ALLOW_FILTER | MF_SELECT_QTY,
                      _drop_menu_titlefn,
                      &items,
                      &Options.drop_filter,
                      _drop_selitem_text,
                      &preselected_items);

    for (SelItem &sel : items)
        sel.slot = letter_to_index(sel.slot);

    return items;
}

static bool item_matches_digit_inscription(item_def &item, char digit, operation_types oper)
{
    const string& r(item.inscription);
    const char iletter = static_cast<char>(oper);
    for (unsigned int j = 0; j + 2 < r.size(); ++j)
        if (r[j] == '@' && (r[j+1] == iletter || r[j+1] == '*') && r[j+2] == digit)
            return true;
    return false;
}

item_def *digit_inscription_to_item(char digit, operation_types oper)
{
    for (int i = 0; i < ENDOFPACK; ++i)
        if (you.inv[i].defined()
                && item_matches_digit_inscription(you.inv[i], digit, oper))
        {
            return &you.inv[i];
        }

    for (stack_iterator si(you.pos(), true); si; ++si)
        if (item_matches_digit_inscription(*si, digit, oper))
            return &*si;

    return nullptr;
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
                            operation_types oper,
                            bool check_melded)
{
    item_def old_item;
    string prompt;
    bool penance = false;
    if (oper == OPER_WIELD) // can we safely unwield old item?
    {
        if (!you.slot_item(EQ_WEAPON, check_melded))
            return true;

        int equip = you.equip[EQ_WEAPON];
        if (equip == -1 || item.link == equip)
            return true;

        old_item = *you.slot_item(EQ_WEAPON, check_melded);
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
    if (old_item.cursed())
        prompt += "and destroy ";
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

    return artefact_known_property(item, ARTP_PREVENT_TELEPORTATION);
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

    // Curses first. Warn if something would take off (i.e. destroy) the cursed item.
    if (item.cursed()
        && (oper == OPER_WIELD && is_weapon(item)
            || oper == OPER_PUTON || oper == OPER_WEAR
            || oper == OPER_TAKEOFF || oper == OPER_REMOVE))
    {
        return true;
    }

    // The consequences of evokables are generally known.
    if (item.base_type == OBJ_MISCELLANY
        && oper == OPER_EVOKE && god_hates_item(item))
    {
        penance = true;
        return true;
    }

    // Everything else depends on knowing the item subtype/brand.
    if (!item_type_known(item))
        return false;

    if (oper == OPER_REMOVE
        && item.is_type(OBJ_JEWELLERY, AMU_FAITH)
        && faith_has_penalty())
    {
        return true;
    }

    if (oper == OPER_PUTON
        && item.is_type(OBJ_JEWELLERY, AMU_FAITH)
        && faith_has_penalty())
    {
        return true;
    }

    if (needs_notele_warning(item, oper))
        return true;

    if (oper == OPER_ATTACK && god_hates_item(item))
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
 * @param type_out         Output: OSEL_ANY if the user was in `*`, type_expect
 *                         otherwise. Ignored if nullptr.
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
                       const char other_valid_char,
                       const char *view_all_prompt,
                       int *type_out)
{
    const bool do_warning = !(flags & invprompt_flag::no_warning);
    const bool allow_list_known = !(flags & invprompt_flag::hide_known);
    const bool must_exist = !(flags & invprompt_flag::unthings_ok);
    const bool auto_list = !(flags & invprompt_flag::manual_list);
    const bool allow_easy_quit = !(flags & invprompt_flag::escape_only);

    if (!any_items_of_type(type_expect) && type_expect != OSEL_WIELD
        && type_expect != OSEL_QUIVER_ACTION)
    {
        mprf(MSGCH_PROMPT, "%s",
             no_selectables_message(type_expect).c_str());
        return PROMPT_NOTHING;
    }

    int keyin = 0;
    int ret = PROMPT_ABORT;

    int current_type_expected = type_expect;
    bool need_redraw = false;
    bool need_prompt = true;
    bool need_getch  = true;

    if (auto_list)
    {
        need_prompt = false;
        need_getch = false;

        if (any_items_of_type(type_expect))
            keyin = '?';
        else
            keyin = '*';
    }

    if (keyin == '*')
        current_type_expected = OSEL_ANY;

    // ugh, why is this done manually
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
                 current_type_expected == OSEL_ANY && view_all_prompt
                 ? view_all_prompt : prompt);
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

        // TODO: it seems like for some uses of this function, `*` shouldn't
        // be allowed at all, e.g. evoke.
        if (keyin == '?' || keyin == '*')
        {
            // The "view inventory listing" mode.
            vector< SelItem > items;
            const auto last_keyin = keyin;
            current_type_expected = keyin == '*' ? OSEL_ANY : type_expect;
            int mflags = MF_SINGLESELECT | MF_ANYPRINTABLE | MF_SECONDARY_SCROLL;
            if (other_valid_char == '-')
                mflags |= MF_SPECIAL_MINUS;

            while (true)
            {
                keyin = _invent_select(
                    current_type_expected == OSEL_ANY && view_all_prompt
                        ? view_all_prompt : prompt,
                    mtype, current_type_expected, -1, mflags, nullptr, &items);

                if (allow_list_known && keyin == '\\')
                {
                    check_item_knowledge();
                    continue;
                }
                break;
            }

            // a continue at this point has need_prompt = need_getch = true

            // let `?` toggle the menu altogether. If this is a non-autolist,
            // return to the prompt, otherwise we will pass escape to the
            // abort handling below. (TODO: is this the right behavior?)
            // TODO: some versions of _invent_select, such as wield, never
            // return '?'. Is this a problem?
            if (keyin == '?' || key_is_escape(keyin) && !auto_list)
                continue;
            else if (keyin == '*')
            {
                // let `*` act as a toggle. This is a slightly wacky
                // implementation in that '?' as a toggle does something
                // entirely different...
                // need_prompt = view_all_prompt;
                need_prompt = need_getch = false;
                if (last_keyin == '*')
                    keyin = '?';
                else
                    keyin = '*';
                continue;
            }
            else if ((keyin == CK_ENTER || keyin == CK_MOUSE_B1) && items.size() > 0)
            {
                // hacky, but lets the inscription checks below trip
                // TODO: this code should not rely on keyin, it breaks cmd
                // bindings
                keyin = items[0].slot;
            }
            else if (other_valid_char != 0 && keyin == other_valid_char)
            {
                // need to handle overrides...ugly code duplication
                ret = PROMPT_GOT_SPECIAL;
                break;
            }
        }

        if (isadigit(keyin))
        {
            // scan for our item
            item_def *item = digit_inscription_to_item(keyin, oper);
            if (item && in_inventory(*item))
            {
                ret = item->link;
                if (!do_warning || check_warning_inscriptions(*item, oper))
                    break;
            }
        }
        else if (key_is_escape(keyin) || allow_easy_quit && keyin == ' ')
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
        else if (keyin == ';')
        {
            ret = you.last_unequip;
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
    if (type_out)
        *type_out = current_type_expected;

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
    return is_weapon(item) && !you.has_mutation(MUT_NO_GRASPING);
}

/// Does the item only serve to produce summons or allies?
static bool _item_ally_only(const item_def &item)
{
    if (item.base_type == OBJ_WANDS)
        return item.sub_type == WAND_CHARMING;
    else if (item.base_type == OBJ_MISCELLANY)
    {
        switch (item.sub_type)
        {
        case MISC_PHANTOM_MIRROR:
        case MISC_HORN_OF_GERYON:
        case MISC_BOX_OF_BEASTS:
            return true;
        default:
            return false;
        }
    }
    return false;
}

/**
 * Return whether an item can be evoked.
 *
 * @param item      The item to check
 * @param msg       Whether we need to print a message.
 */
bool item_is_evokable(const item_def &item, bool msg)
{
    // XX unify with evoke_check?
    const string error = item_is_melded(item)
            ? "Your " + item.name(DESC_QUALNAME) + " is melded into your body."
            : "That item can only be evoked when wielded.";

    const bool no_evocables = you.get_mutation_level(MUT_NO_ARTIFICE);
    const char* const no_evocable_error = "You cannot evoke magical items.";

    if (no_evocables
        && !(item.base_type == OBJ_MISCELLANY
             && item.sub_type == MISC_ZIGGURAT)) // zigfigs are OK.
    {
        // the rest are forbidden under sac evocables.
        if (msg)
            mpr(no_evocable_error);
        return false;
    }

    // TODO: check other summoning constraints here?
    if (_item_ally_only(item) && you.allies_forbidden())
    {
        if (msg)
            mpr("That item cannot be used by those who cannot gain allies!");
        return false;
    }

    switch (item.base_type)
    {
    case OBJ_WANDS:
        return true;

    case OBJ_MISCELLANY:
#if TAG_MAJOR_VERSION == 34
        if (item.sub_type != MISC_BUGGY_LANTERN_OF_SHADOWS
            && item.sub_type != MISC_BUGGY_EBONY_CASKET)
#endif
        {
            return true;
        }
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
