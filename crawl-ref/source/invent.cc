/**
 * @file
 * @brief Functions for inventory related commands.
**/

#include "AppHdr.h"

#include "invent.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sstream>
#include <iomanip>

#include "externs.h"
#include "options.h"

#include "artefact.h"
#include "clua.h"
#include "colour.h"
#include "command.h"
#include "decks.h"
#include "describe.h"
#include "env.h"
#include "food.h"
#include "initfile.h"
#include "item_use.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "message.h"
#include "player.h"
#include "religion.h"
#include "shopping.h"
#include "showsymb.h"
#include "stuff.h"
#include "mon-util.h"
#include "state.h"
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

InvEntry::InvEntry(const item_def &i, bool show_bg)
    : MenuEntry("", MEL_ITEM), show_background(show_bg), item(&i)
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

bool InvEntry::is_item_cursed() const
{
    return item_ident(*item, ISFLAG_KNOW_CURSE) && item->cursed();
}

bool InvEntry::is_item_glowing() const
{
    return !item_ident(*item, ISFLAG_KNOW_TYPE)
           && (get_equip_desc(*item)
               || (is_artefact(*item)
                   && (item->base_type == OBJ_WEAPONS
                       || item->base_type == OBJ_ARMOUR
                       || item->base_type == OBJ_BOOKS)));
}

bool InvEntry::is_item_ego() const
{
    return item_ident(*item, ISFLAG_KNOW_TYPE) && !is_artefact(*item)
           && item->special != 0
           && (item->base_type == OBJ_WEAPONS
               || item->base_type == OBJ_MISSILES
               || item->base_type == OBJ_ARMOUR);
}

bool InvEntry::is_item_art() const
{
    return item_ident(*item, ISFLAG_KNOW_TYPE) && is_artefact(*item);
}

bool InvEntry::is_item_equipped() const
{
    if (item->link == -1 || item->pos.x != -1 || item->pos.y != -1)
        return false;

    for (int i = 0; i < NUM_EQUIP; i++)
        if (item->link == you.equip[i])
            return true;

    return false;
}

// Returns values < 0 for edible chunks (non-rotten except for Saprovores),
// 0 for non-chunks, and values > 0 for rotten chunks for non-Saprovores.
int InvEntry::item_freshness() const
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

    tstr << ' ' << static_cast<char>(hotkeys[0]);

    if (need_cursor)
        tstr << '[';
    else
        tstr << ' ';

    if (!selected_qty)
        tstr << '-';
    else if (selected_qty < quantity)
        tstr << '#';
    else
        tstr << '+';

    if (need_cursor)
        tstr << ']';
    else
        tstr << ' ';

    if (InvEntry::show_glyph)
        tstr << "(" << glyph_to_tagstr(get_item_glyph(item)) << ")" << " ";

    //For weights display we need to know maximum number of chars in each column
    //which fit in one line.
    //XXX There should be a better way to determine this, for now we simply
    //estimate it by the following heuristics {kittel}.
    unsigned max_chars_in_line = get_number_of_cols() - 2;
#ifdef USE_TILE_LOCAL
    if (Options.tile_menu_icons && Options.show_inventory_weights)
        max_chars_in_line = get_number_of_cols() * 4 / 9 - 2;
#endif

    int colour_tag_adjustment = 0;
    if (InvEntry::show_glyph)
    {
        // colour tags have to be taken into account for terminal width
        // calculations on the ^x screen (monsters/items/features in LOS)
        string colour_tag = colour_to_str(get_item_glyph(item).col);
        colour_tag_adjustment = colour_tag.size() * 2 + 5;
    }

    if (Options.show_inventory_weights)
    {
        max_chars_in_line -= 1;
        const int w_weight = 10; //length of " (999 aum)"
        int excess = strwidth(tstr.str()) - colour_tag_adjustment
                     + text.size() + w_weight - max_chars_in_line;
        if (excess > 0)
            tstr << text.substr(0, max<int>(0, text.size() - excess - 2)) << "..";
        else
            tstr << text;
    }
    else
        tstr << text;

    if (Options.show_inventory_weights)
    {
        const int mass = item_mass(*item) * item->quantity;
        // Note: If updating the " (%i aum)" format, remember to update
        // w_weight above.
        tstr << setw(max_chars_in_line - strwidth(tstr.str())
                     + colour_tag_adjustment)
             << right
             << make_stringf(" (%i aum)",
                             static_cast<int>(0.5 + BURDEN_TO_AUM * mass));
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
    case OBJ_FOOD:
        glyphs.push_back('%');
        break;
    case OBJ_BOOKS:
        glyphs.push_back('+');
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
    case OBJ_RODS:
        glyphs.push_back('\\');
        break;
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
    for (unsigned int k = 0; k < glyphs.size(); ++k)
        add_hotkey(glyphs[k]);

    // Hack to make rotten chunks answer to '&' as well.
    // Check for uselessness rather than inedibility to cover the spells
    // that use chunks.
    if (i.base_type == OBJ_FOOD && i.sub_type == FOOD_CHUNK
        && is_useless_item(i))
    {
        add_hotkey('&');
    }
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
    : Menu(mflags, "inventory", false), type(MT_INVLIST), pre_select(NULL),
      title_annotate(NULL)
{
#ifdef USE_TILE_LOCAL
    if (Options.tile_menu_icons)
#endif
        mdisplay->set_num_columns(2);

    InvEntry::set_show_cursor(false);
}

// Returns vector of item_def pointers to each item_def in the given
// vector. Note: make sure the original vector stays around for the lifetime
// of the use of the item pointers, or mayhem results!
vector<const item_def*>
InvMenu::xlat_itemvect(const vector<item_def> &v)
{
    vector<const item_def*> xlatitems;
    for (unsigned i = 0, size = v.size(); i < size; ++i)
        xlatitems.push_back(&v[i]);
    return xlatitems;
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

void InvMenu::set_title(const string &s)
{
    string stitle = s;
    if (stitle.empty())
    {
#ifdef USE_TILE_WEB
        // Webtiles handles menus specially, so disable the crt
        tiles_crt_control crt_enabled(false);
#endif

        // We're not printing anything yet, but this select the crt layer
        // so that get_number_of_cols returns the appropriate value.
        cgotoxy(1, 1);

        const int cap = carrying_capacity(BS_UNENCUMBERED);

        stitle = make_stringf(
            "Inventory: %.0f/%.0f aum (%d%%, %d/52 slots)",
            BURDEN_TO_AUM * you.burden,
            BURDEN_TO_AUM * cap,
            (you.burden * 100) / cap,
            inv_count());

        string prompt = "(_ for help)";
        stitle = stitle + string(max(0, get_number_of_cols() - strwidth(stitle)
                                        - strwidth(prompt)),
                                 ' ') + prompt;
    }

    set_title(new InvTitle(this, stitle, title_annotate));
}

static bool _has_melded_armour()
{
    for (int e = EQ_CLOAK; e <= EQ_BODY_ARMOUR; e++)
        if (you.melded[e])
            return true;
    return false;
}

static bool _has_tran_unwearable_armour()
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def &item(you.inv[i]);

        if (item.defined() && item.base_type == OBJ_ARMOUR
            && !you_tran_can_wear(item))
        {
            return true;
        }
    }
    return false;
}

static string _no_selectables_message(int item_selector)
{
    switch (item_selector)
    {
    case OSEL_ANY:
        return "You aren't carrying anything.";
    case OSEL_SCROLL_TARGET:
        return "You aren't carrying anything you could use a scroll on.";
    case OSEL_WIELD:
    case OBJ_WEAPONS:
        return "You aren't carrying any weapons.";
    case OBJ_ARMOUR:
    {
        if (_has_melded_armour())
            return "Your armour is currently melded into you.";
        else if (_has_tran_unwearable_armour())
            return "You aren't carrying any armour you can wear in your "
                   "current form.";
        else
            return "You aren't carrying any armour.";
    }
    case OSEL_UNIDENT:
        return "You don't have any unidentified items.";
    case OSEL_RECHARGE:
        return "You aren't carrying any rechargeable items.";
    case OSEL_ENCH_ARM:
        return "You aren't carrying any armour which can be enchanted further.";
    case OBJ_CORPSES:
    case OSEL_VAMP_EAT:
        return "You aren't carrying any corpses which you can drain.";
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
    case OSEL_BUTCHERY:
        return "You aren't carrying any sharp implements.";
    case OSEL_EVOKABLE:
        return "You aren't carrying any items that can be evoked.";
    case OSEL_FRUIT:
        return "You aren't carrying any fruit.";
    case OSEL_CURSED_WORN:
        return "None of your equipped items are cursed.";
    case OSEL_UNCURSED_WORN_ARMOUR:
        return "You aren't wearing any piece of uncursed armour.";
    case OSEL_UNCURSED_WORN_JEWELLERY:
        return "You aren't wearing any piece of uncursed jewellery.";
    case OSEL_BRANDABLE_WEAPON:
        return "You aren't carrying any weapons that can be branded.";
    }

    return "You aren't carrying any such object.";
}

void InvMenu::load_inv_items(int item_selector, int excluded_slot,
                             MenuEntry *(*procfn)(MenuEntry *me))
{
    vector<const item_def *> tobeshown;
    _get_inv_items_to_show(tobeshown, item_selector, excluded_slot);

    load_items(tobeshown, procfn);

    if (!item_count())
        set_title(_no_selectables_message(item_selector));
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
                tileset.push_back(tile_def(TILE_ITEM_SLOT_EQUIP_CURSED, TEX_DEFAULT));
            else
                tileset.push_back(tile_def(TILE_ITEM_SLOT_EQUIP, TEX_DEFAULT));
        }
        else if (item_known_cursed(*item))
            tileset.push_back(tile_def(TILE_ITEM_SLOT_CURSED, TEX_DEFAULT));

        tileset.push_back(tile_def(TILE_ITEM_SLOT, TEX_FEAT));
        tileidx_t base_item = tileidx_known_base_item(idx);
        if (base_item)
            tileset.push_back(tile_def(base_item, TEX_DEFAULT));
        tileset.push_back(tile_def(idx, TEX_DEFAULT));

        if (eq != EQ_NONE && you.melded[eq])
            tileset.push_back(tile_def(TILEI_MESH, TEX_ICONS));
    }
    else
    {
        // Do we want to display the floor type or is that too distracting?
        const coord_def c = item->held_by_monster()
            ? item->holding_monster()->pos()
            : item->pos;
        tileidx_t ch = 0;
        if (!show_background || c.x == 0)
        {
            // Store items.
            tileset.push_back(tile_def(TILE_ITEM_SLOT, TEX_FEAT));
        }
        else if (c != coord_def())
        {
            ch = tileidx_feature(c);
            if (ch == TILE_FLOOR_NORMAL)
                ch = env.tile_flv(c).floor;
            else if (ch == TILE_WALL_NORMAL)
                ch = env.tile_flv(c).wall;

            tileset.push_back(tile_def(ch, get_dngn_tex(ch)));
        }
        tileidx_t base_item = tileidx_known_base_item(idx);
        if (base_item)
            tileset.push_back(tile_def(base_item, TEX_DEFAULT));

        tileset.push_back(tile_def(idx, TEX_DEFAULT));

        if (ch != 0)
        {
            // Needs to be displayed so as to not give away mimics in shallow water.
            if (ch == TILE_DNGN_SHALLOW_WATER)
            {
                tileset.push_back(tile_def(TILEI_MASK_SHALLOW_WATER,
                                           TEX_ICONS));
            }
            else if (ch == TILE_DNGN_SHALLOW_WATER_MURKY)
            {
                tileset.push_back(tile_def(TILEI_MASK_SHALLOW_WATER_MURKY,
                                           TEX_ICONS));
            }
        }
    }
    if (item->base_type == OBJ_WEAPONS || item->base_type == OBJ_MISSILES
        || item->base_type == OBJ_ARMOUR || item->base_type == OBJ_RODS)
    {
        tileidx_t brand = tileidx_known_brand(*item);
        if (brand)
            tileset.push_back(tile_def(brand, TEX_DEFAULT));
    }
    else if (item->base_type == OBJ_CORPSES)
    {
        tileidx_t brand = tileidx_corpse_brand(*item);
        if (brand)
            tileset.push_back(tile_def(brand, TEX_DEFAULT));
    }

    return true;
}
#endif

bool InvMenu::is_selectable(int index) const
{
    if (type == MT_DROP)
    {
        InvEntry *item = dynamic_cast<InvEntry*>(items[index]);
        if (item->is_item_cursed() && item->is_item_equipped())
            return false;

        string text = item->get_text();

        if (text.find("!*") != string::npos || text.find("!d") != string::npos)
            return false;
    }

    return Menu::is_selectable(index);
}

bool InvMenu::allow_easy_exit() const
{
    return type == MT_KNOW || Menu::allow_easy_exit();
}

template <const string &(InvEntry::*method)() const>
static int compare_item_str(const InvEntry *a, const InvEntry *b)
{
    return (a->*method)().compare((b->*method)());
}

template <typename T, T (*proc)(const InvEntry *a)>
static int compare_item(const InvEntry *a, const InvEntry *b)
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
    for (item_sort_comparators::const_iterator i = cmps->begin();
         i != cmps->end(); ++i)
    {
        const int cmp = i->compare(a, b);
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
          { "curse",     compare_item<bool, &InvEntry::is_item_cursed> },
          { "glowing",   compare_item_rev<bool, &InvEntry::is_item_glowing> },
          { "ego",       compare_item_rev<bool, &InvEntry::is_item_ego> },
          { "art",       compare_item_rev<bool, &InvEntry::is_item_art> },
          { "equipped",  compare_item_rev<bool, &InvEntry::is_item_equipped> },
          { "identified",compare_item<bool, sort_item_identified> },
          { "charged",   compare_item<bool, sort_item_charged>},
          { "qty",       compare_item<int, sort_item_qty> },
          { "slot",      compare_item<int, sort_item_slot> },
          { "freshness", compare_item<int, &InvEntry::item_freshness> }
      };

    list.clear();
    vector<string> cmps = split_string(",", set);
    for (int i = 0, size = cmps.size(); i < size; ++i)
    {
        string s = cmps[i];
        if (s.empty())
            continue;

        const bool negated = s[0] == '>';
        if (s[0] == '<' || s[0] == '>')
            s = s.substr(1);

        for (unsigned ci = 0; ci < ARRAYSZ(cmp_map); ++ci)
            if (cmp_map[ci].cname == s)
            {
                list.push_back(item_comparator(cmp_map[ci].cmp, negated));
                break;
            }
    }

    if (list.empty())
    {
        list.push_back(
            item_comparator(compare_item_str<&InvEntry::get_fullname>));
    }
}

const menu_sort_condition *InvMenu::find_menu_sort_condition() const
{
    for (int i = Options.sort_menus.size() - 1; i >= 0; --i)
        if (Options.sort_menus[i].matches(type))
            return &Options.sort_menus[i];

    return NULL;
}

void InvMenu::sort_menu(vector<InvEntry*> &invitems,
                        const menu_sort_condition *cond)
{
    if (!cond || cond->sort == -1 || (int) invitems.size() < cond->sort)
        return;

    sort(invitems.begin(), invitems.end(), menu_entry_comparator(cond));
}

menu_letter InvMenu::load_items(const vector<const item_def*> &mitems,
                                MenuEntry *(*procfn)(MenuEntry *me),
                                menu_letter ckey, bool sort)
{
    FixedVector< int, NUM_OBJECT_CLASSES > inv_class(0);
    for (int i = 0, count = mitems.size(); i < count; ++i)
        inv_class[ mitems[i]->base_type ]++;

    vector<InvEntry*> items_in_class;
    const menu_sort_condition *cond = NULL;
    if (sort) cond = find_menu_sort_condition();

    for (int i = 0; i < NUM_OBJECT_CLASSES; ++i)
    {
        if (!inv_class[i])
            continue;

        if (type != MT_RUNES)
        {
            string subtitle = item_class_name(i);

            // Mention the class selection shortcuts.
            if (is_set(MF_MULTISELECT) && inv_class[i] > 1)
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
                    for (unsigned int k = 0; k < glyphs.size(); ++k)
                         subtitle += glyphs[k];
                    subtitle += "</w><blue>)";
                }
            }

            add_entry(new MenuEntry(subtitle, MEL_SUBTITLE));
        }
        items_in_class.clear();

        InvEntry *forced_first = NULL;
        for (int j = 0, count = mitems.size(); j < count; ++j)
        {
            if (mitems[j]->base_type != i)
                continue;
            if (mitems[j]->sub_type == get_max_subtype(mitems[j]->base_type))
                forced_first = new InvEntry(*mitems[j]);
            else
                items_in_class.push_back(new InvEntry(*mitems[j]));
        }

        sort_menu(items_in_class, cond);
        if (forced_first)
            items_in_class.insert(items_in_class.begin(),forced_first);

        for (unsigned int j = 0; j < items_in_class.size(); ++j)
        {
            InvEntry *ie = items_in_class[j];
            if (tag == "pickup")
                ie->tag = "pickup";
            // If there's no hotkey, provide one.
            if (ie->hotkeys[0] == ' ')
                ie->hotkeys[0] = ckey++;
            do_preselect(ie);

            add_entry(procfn? (*procfn)(ie) : ie);
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

    for (int i = 0, size = pre_select->size(); i < size; ++i)
        if (ie->item && ie->item == (*pre_select)[i].item)
        {
            ie->selected_qty = (*pre_select)[i].quantity;
            break;
        }
}

vector<SelItem> InvMenu::get_selitems() const
{
    vector<SelItem> selected_items;
    for (int i = 0, count = sel.size(); i < count; ++i)
    {
        InvEntry *inv = dynamic_cast<InvEntry*>(sel[i]);
        selected_items.push_back(
                SelItem(
                    inv->hotkeys[0],
                    inv->selected_qty,
                    inv->item));
    }
    return selected_items;
}

bool InvMenu::process_key(int key)
{
    if (key == CONTROL('W'))
    {
        Options.show_inventory_weights = !Options.show_inventory_weights;
        draw_menu();
        return true;
    }

    if (type == MT_KNOW)
    {
        bool resetting = (lastch == CONTROL('D'));
        if (resetting)
        {
            //return the menu title to its previous text.
            set_title(temp_title);
            update_title();
            num = -2;

            // Disarm ^D here, because process_key doesn't always set lastch.
            lastch = ' ';
        }
        else
            num = -1;

        switch (key)
        {
        case ',':
            return true;
        case '*':
            if (!resetting)
                break;
        case '^':
            key = ',';
            break;

        case '-':
        case '\\':
        case CK_ENTER:
        CASE_ESCAPE
            lastch = key;
            return false;

        case '_':
            show_known_menu_help();
#ifdef USE_TILE_WEB
            webtiles_update_scroll_pos();
#endif
            draw_menu();
            return true;

        case CONTROL('D'):
            // If we cannot select anything (e.g. on the unknown items
            // page), ignore Ctrl-D.  Likewise if the last key was
            // Ctrl-D (we have already disarmed Ctrl-D for the next
            // keypress by resetting lastch).
            if (flags & (MF_SINGLESELECT | MF_MULTISELECT) && !resetting)
            {
                lastch = CONTROL('D');
                temp_title = title->text;
                set_title("Select to reset item to default: ");
                update_title();
            }

            return true;
        }
    }

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
        return true;
    }

    return Menu::process_key(key);
}

unsigned char InvMenu::getkey() const
{
    unsigned char mkey = lastch;
    if (type == MT_KNOW && (mkey == 0 || mkey == CK_ENTER))
        return mkey;

    if (!isaalnum(mkey) && mkey != '$' && mkey != '-' && mkey != '?'
        && mkey != '*' && !key_is_escape(mkey) && mkey != '\\')
    {
        mkey = ' ';
    }
    return mkey;
}

//////////////////////////////////////////////////////////////////////////////

bool in_inventory(const item_def &i)
{
    return i.pos.x == -1 && i.pos.y == -1;
}

string item_class_name(int type, bool terse)
{
    if (terse)
    {
        // TODO: merge with base_type_string()
        switch (type)
        {
        case OBJ_GOLD:       return "gold";
        case OBJ_WEAPONS:    return "weapon";
        case OBJ_MISSILES:   return "missile";
        case OBJ_ARMOUR:     return "armour";
        case OBJ_WANDS:      return "wand";
        case OBJ_FOOD:       return "food";
        case OBJ_SCROLLS:    return "scroll";
        case OBJ_JEWELLERY:  return "jewellery";
        case OBJ_POTIONS:    return "potion";
        case OBJ_BOOKS:      return "book";
        case OBJ_STAVES:     return "magical staff";
        case OBJ_RODS:       return "rod";
        case OBJ_ORBS:       return "orb";
        case OBJ_MISCELLANY: return "misc";
        case OBJ_CORPSES:    return "carrion";
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
        case OBJ_RODS:       return "Rods";
        case OBJ_ORBS:       return "Orbs of Power";
        case OBJ_MISCELLANY: return "Miscellaneous";
        case OBJ_CORPSES:    return "Carrion";
        }
    }
    return "";
}

const char* item_slot_name(equipment_type type, bool terse)
{
    if (terse)
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
    else
    {
        switch (type)
        {
        case EQ_CLOAK:       return "Cloak";
        case EQ_HELMET:      return "Helmet";
        case EQ_GLOVES:      return "Gloves";
        case EQ_BOOTS:       return "Boots";
        case EQ_SHIELD:      return "Shield";
        case EQ_BODY_ARMOUR: return "Body";
        default:             return "";
        }
    }
}

vector<SelItem> select_items(const vector<const item_def*> &items,
                             const char *title, bool noselect,
                             menu_type mtype, invtitle_annotator titlefn)
{
    vector<SelItem> selected;
    if (!items.empty())
    {
        InvMenu menu;
        menu.set_type(mtype);
        menu.set_title_annotator(titlefn);
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

static bool _item_class_selected(const item_def &i, int selector)
{
    const int itype = i.base_type;
    if (selector == OSEL_ANY || selector == itype
                                && itype != OBJ_FOOD && itype != OBJ_ARMOUR)
    {
        return true;
    }

    switch (selector)
    {
    // Combined filter for valid unided scroll targets
    // TODO: If the player already ided one of the scrolls then we
    // could filter the list further. That results in the final scroll
    // being effectively auto-ided as soon as you hit the menu.
    case OSEL_SCROLL_TARGET:
        return !item_is_melded(i)
            // Any unidentified
            && (!fully_identified(i) || (is_deck(i) && !top_card_is_known(i))
                // Rechargeable
                || item_is_rechargeable(i, true)
                // Armour
                || is_enchantable_armour(i, true, true));

    case OBJ_ARMOUR:
        return itype == OBJ_ARMOUR && you_tran_can_wear(i);

    case OSEL_FRUIT:
        return is_fruit(i);

    case OSEL_WORN_ARMOUR:
        return itype == OBJ_ARMOUR && item_is_equipped(i);

    case OSEL_UNIDENT:
        return !fully_identified(i) || (is_deck(i) && !top_card_is_known(i));

    case OBJ_MISSILES:
        return itype == OBJ_MISSILES || itype == OBJ_WEAPONS;

    case OSEL_THROWABLE:
    {
        if (i.base_type != OBJ_WEAPONS && i.base_type != OBJ_MISSILES)
            return false;

        const launch_retval projected = is_launched(&you, you.weapon(), i);

        if (projected == LRET_FUMBLED)
            return false;

        return true;
    }
    case OBJ_WEAPONS:
    case OSEL_WIELD:
        return item_is_wieldable(i);

    case OSEL_BUTCHERY:
        return itype == OBJ_WEAPONS && can_cut_meat(i);

    case OBJ_SCROLLS:
        return itype == OBJ_SCROLLS
               || (itype == OBJ_BOOKS && i.sub_type != BOOK_MANUAL);

    case OSEL_RECHARGE:
        return item_is_rechargeable(i, true);

    case OSEL_EVOKABLE:
        return item_is_evokable(i, true, true, true);

    case OSEL_ENCH_ARM:
        return is_enchantable_armour(i, true, true);

    case OBJ_FOOD:
        return itype == OBJ_FOOD && !is_inedible(i);

    case OSEL_VAMP_EAT:
        return itype == OBJ_CORPSES && i.sub_type == CORPSE_BODY
               && !food_is_rotten(i) && mons_has_blood(i.mon_type);

    case OSEL_DRAW_DECK:
        return is_deck(i);

    case OSEL_EQUIP:
    {
        if (item_is_quivered(i))
            return true;

        for (int eq = 0; eq < NUM_EQUIP; eq++)
            if (you.equip[eq] == i.link)
                return true;

        return false;
    }

    case OSEL_CURSED_WORN:
        return i.cursed() && item_is_equipped(i)
               && (&i != you.weapon() || is_weapon(i));

    case OSEL_UNCURSED_WORN_ARMOUR:
        return !i.cursed() && item_is_equipped(i) && itype == OBJ_ARMOUR;

    case OSEL_UNCURSED_WORN_JEWELLERY:
        return !i.cursed() && item_is_equipped(i) && itype == OBJ_JEWELLERY;

    case OSEL_BRANDABLE_WEAPON:
        return is_brandable_weapon(i, true);

    default:
        return false;
    }
}

static bool _userdef_item_selected(const item_def &i, int selector)
{
#if defined(CLUA_BINDINGS)
    const char *luafn = selector == OSEL_WIELD ? "ch_item_wieldable"
                                               : NULL;
    return luafn && clua.callbooleanfn(false, luafn, "i", &i);
#else
    return false;
#endif
}

static bool _is_item_selected(const item_def &i, int selector)
{
    return _item_class_selected(i, selector)
           || _userdef_item_selected(i, selector);
}

static void _get_inv_items_to_show(vector<const item_def*> &v,
                                   int selector, int excluded_slot)
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].defined()
            && you.inv[i].link != excluded_slot
            && _is_item_selected(you.inv[i], selector))
        {
            v.push_back(&you.inv[i]);
        }
    }
}

bool any_items_to_select(int selector, bool msg, int excluded_slot)
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (you.inv[i].defined()
            && you.inv[i].link != excluded_slot
            && _is_item_selected(you.inv[i], selector))
        {
            return true;
        }
    }
    if (msg)
    {
        mprf(MSGCH_PROMPT, "%s",
             _no_selectables_message(selector).c_str());
    }
    return false;
}

// Use title = NULL for stock Inventory title
// type = MT_DROP allows the multidrop toggle
static unsigned char _invent_select(const char *title = NULL,
                                    menu_type type = MT_INVLIST,
                                    int item_selector = OSEL_ANY,
                                    int excluded_slot = -1,
                                    int flags = MF_NOSELECT,
                                    invtitle_annotator titlefn = NULL,
                                    vector<SelItem> *items = NULL,
                                    vector<text_pattern> *filter = NULL,
                                    Menu::selitem_tfn selitemfn = NULL,
                                    const vector<SelItem> *pre_select = NULL)
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

unsigned char get_invent(int invent_type)
{
    unsigned char select;
    int flags = MF_SINGLESELECT;
    if (you.dead || crawl_state.updating_scores)
        flags |= MF_EASY_EXIT;

    while (true)
    {
        select = _invent_select(NULL, MT_INVLIST, invent_type, -1, flags);

        if (isaalpha(select))
        {
            const int invidx = letter_to_index(select);
            if (you.inv[invidx].defined())
            {
                if (!describe_item(you.inv[invidx], true))
                    break;
            }
        }
        else
            break;
    }

    if (!crawl_state.doing_prev_cmd_again)
        redraw_screen();

    return select;
}

void browse_inventory()
{
    get_invent(OSEL_ANY);
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

// This function prompts the user for an item, handles the '?' and '*'
// listings, and returns the inventory slot to the caller (which if
// must_exist is true, as it is by default, will be an assigned item,
// with a positive quantity.
//
// It returns PROMPT_ABORT       if the player hits escape.
// It returns PROMPT_GOT_SPECIAL if the player hits the "other_valid_char".
//
// Note: This function never checks if the item is appropriate.
vector<SelItem> prompt_invent_items(
                        const char *prompt,
                        menu_type mtype,
                        int type_expect,
                        invtitle_annotator titlefn,
                        bool allow_auto_list,
                        bool allow_easy_quit,
                        const char other_valid_char,
                        vector<text_pattern> *select_filter,
                        Menu::selitem_tfn fn,
                        const vector<SelItem> *pre_select)
{
    unsigned char  keyin = 0;
    int            ret = PROMPT_ABORT;

    bool           need_redraw = false;
    bool           need_prompt = true;
    bool           need_getch  = true;
    bool           auto_list   = Options.auto_list && allow_auto_list;

    if (auto_list)
    {
        need_getch = false;
        keyin       = '?';
    }

    vector<SelItem> items;
    int count = -1;
    while (true)
    {
        if (need_redraw && !crawl_state.doing_prev_cmd_again)
        {
            redraw_screen();
            mesclr();
        }

        if (need_prompt)
        {
            mprf(MSGCH_PROMPT, "%s (<w>?</w> for menu, <w>Esc</w> to quit)",
                 prompt);
        }

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
            const int ch = _invent_select(prompt,
                                          mtype,
                                          keyin == '*' ? OSEL_ANY : type_expect,
                                          -1,
                                          selmode,
                                          titlefn, &items, select_filter, fn,
                                          pre_select);

            if ((selmode & MF_SINGLESELECT) || key_is_escape(ch))
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
                    mesclr();
                }

                for (unsigned int i = 0; i < items.size(); ++i)
                    items[i].slot = letter_to_index(items[i].slot);
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
                || (Options.easy_quit_item_prompts
                    && allow_easy_quit
                    && keyin == ' '))
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
        items.push_back(
            SelItem(ret, count,
                     ret != PROMPT_GOT_SPECIAL ? &you.inv[ret] : NULL));
    }
    return items;
}

static int _digit_to_index(char digit, operation_types oper)
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

// Checks if current item (to be removed) has a warning inscription
// and prompts the user for confirmation.
bool check_old_item_warning(const item_def& item,
                             operation_types oper)
{
    item_def old_item;
    string prompt;
    if (oper == OPER_WIELD) // can we safely unwield old item?
    {
        if (!you.weapon())
            return true;

        old_item = *you.weapon();
        if (!needs_handle_warning(old_item, OPER_WIELD))
            return true;

        prompt += "Really unwield ";
    }
    else if (oper == OPER_WEAR) // can we safely take off old item?
    {
        if (item.base_type != OBJ_ARMOUR)
            return true;

        equipment_type eq_slot = get_armour_slot(item);
        if (you.equip[eq_slot] == -1)
            return true;

        old_item = you.inv[you.equip[eq_slot]];

        if (!needs_handle_warning(old_item, OPER_TAKEOFF))
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
            if (!needs_handle_warning(old_item, OPER_TAKEOFF))
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
    return yesno(prompt.c_str(), false, 'n');
}

static string _operation_verb(operation_types oper)
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

static bool _nasty_stasis(const item_def &item, operation_types oper)
{
    return oper == OPER_PUTON
           && item.base_type == OBJ_JEWELLERY
           && item.sub_type == AMU_STASIS
           && (you.duration[DUR_HASTE] || you.duration[DUR_SLOW]
               || you.duration[DUR_TELEPORT] || you.duration[DUR_FINESSE]);
}

static bool _is_wielded(const item_def &item)
{
    int equip = you.equip[EQ_WEAPON];
    return equip != -1 && item.link == equip;
}

bool needs_handle_warning(const item_def &item, operation_types oper)
{
    if (_has_warning_inscription(item, oper))
        return true;

    if (!item_ident(item, ISFLAG_KNOW_TYPE))
        return false;

    if (oper == OPER_REMOVE
        && item.base_type == OBJ_JEWELLERY
        && item.sub_type == AMU_FAITH
        && !you_worship(GOD_NO_GOD))
    {
        return true;
    }

    if (_nasty_stasis(item, oper))
        return true;

    if (oper == OPER_WIELD // unwielding uses OPER_WIELD too
        && is_weapon(item))
    {
        if (get_weapon_brand(item) == SPWPN_DISTORTION
            && !you.duration[DUR_WEAPON_BRAND])
        {
            return true;
        }

        if (get_weapon_brand(item) == SPWPN_VAMPIRICISM
            && !you.is_undead && !crawl_state.game_is_zotdef()
            && !you_foodless())
        {
            return true;
        }

        if (item_known_cursed(item) && !_is_wielded(item))
            return true;

        if (is_artefact(item) && artefact_wpn_property(item, ARTP_MUTAGENIC))
            return true;
    }
    else if (oper == OPER_PUTON || oper == OPER_WEAR)
    {
        if (item_known_cursed(item))
            return true;
    }

    if (oper == OPER_PUTON || oper == OPER_WEAR || oper == OPER_TAKEOFF
        || oper == OPER_REMOVE)
    {
        if (is_artefact(item) && artefact_wpn_property(item, ARTP_MUTAGENIC))
            return true;
    }

    return false;
}

// Returns true if user OK'd it (or no warning), false otherwise.
bool check_warning_inscriptions(const item_def& item,
                                 operation_types oper)
{
    if (item.defined()
        && needs_handle_warning(item, oper))
    {
        // When it's about destroying an item, don't even ask.
        // If the player really wants to do that, they'll have
        // to remove the inscription.
        if (oper == OPER_DESTROY)
            return false;

        if (oper == OPER_WIELD)
        {
            // Can't use can_wield in item_use.cc because it wants
            // a non-const item_def.
            if (!you.can_wield(item))
                return true;
        }
        else if (oper == OPER_WEAR)
        {
            if (!can_wear_armour(item, false, false))
                return true;

            // Don't ask if item already worn.
            int equip = you.equip[get_armour_slot(item)];
            if (equip != -1 && item.link == equip)
                return check_old_item_warning(item, oper);
        }
        else if (oper == OPER_PUTON)
        {
            if (item.base_type != OBJ_JEWELLERY)
                return true;

            // Don't ask if item already worn.
            if (jewellery_is_amulet(item))
            {
                int equip = you.equip[EQ_AMULET];
                if (equip != -1 && item.link == equip)
                    return check_old_item_warning(item, oper);
            }
            else
            {
                for (int slots = EQ_LEFT_RING; slots < NUM_EQUIP; ++slots)
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

        string prompt = "Really " + _operation_verb(oper) + " ";
        prompt += (in_inventory(item) ? item.name(DESC_INVENTORY)
                                      : item.name(DESC_A));
        if (_nasty_stasis(item, oper))
            prompt += string(" while ")
                      + (you.duration[DUR_TELEPORT] ? "about to teleport" :
                         you.duration[DUR_SLOW] ? "slowed" : "hasted");
        prompt += "?";
        return yesno(prompt.c_str(), false, 'n')
               && check_old_item_warning(item, oper);
    }
    else
        return check_old_item_warning(item, oper);
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
int prompt_invent_item(const char *prompt,
                        menu_type mtype, int type_expect,
                        bool must_exist, bool allow_auto_list,
                        bool allow_easy_quit,
                        const char other_valid_char,
                        int excluded_slot,
                        int *const count,
                        operation_types oper,
                        bool allow_list_known)
{
    if (!any_items_to_select(type_expect, false, excluded_slot)
        && type_expect == OSEL_THROWABLE
        && (oper == OPER_FIRE || oper == OPER_QUIVER)
        && mtype == MT_INVLIST)
    {
        type_expect = OSEL_ANY;
    }

    if (!any_items_to_select(type_expect, false, excluded_slot)
        && type_expect != OSEL_WIELD
        && type_expect != OSEL_BUTCHERY && mtype == MT_INVLIST)
    {
        mprf(MSGCH_PROMPT, "%s",
             _no_selectables_message(type_expect).c_str());
        return PROMPT_NOTHING;
    }

    unsigned char  keyin = 0;
    int            ret = PROMPT_ABORT;

    bool           need_redraw = false;
    bool           need_prompt = true;
    bool           need_getch  = true;
    bool           auto_list   = Options.auto_list && allow_auto_list;

    if (auto_list)
    {
        need_getch = false;

        if (any_items_to_select(type_expect))
            keyin = '?';
        else
            keyin = '*';
    }

    while (true)
    {
        if (need_redraw && !crawl_state.doing_prev_cmd_again)
        {
            redraw_screen();
            mesclr();
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
            keyin = _invent_select(
                        prompt,
                        mtype,
                        keyin == '*' ? OSEL_ANY : type_expect,
                        excluded_slot,
                        MF_SINGLESELECT | MF_ANYPRINTABLE | MF_NO_SELECT_QTY
                            | MF_EASY_EXIT,
                        NULL,
                        &items);

            if (allow_list_known && keyin == '\\')
            {
                check_item_knowledge();
                keyin = '?';
            }

            need_prompt = false;
            need_getch  = false;

            // Don't redraw if we're just going to display another listing
            need_redraw = (keyin != '?' && keyin != '*')
                          && !(count && auto_list && isadigit(keyin));

            if (!items.empty())
            {
                if (count)
                    *count = items[0].quantity;

                if (!crawl_state.doing_prev_cmd_again)
                {
                    redraw_screen();
                    mesclr();
                }
            }
        }
        else if (count != NULL && isadigit(keyin))
        {
            // The "read in quantity" mode
            keyin = _get_invent_quant(keyin, *count);

            need_prompt = false;
            need_getch  = false;

            if (auto_list)
                need_redraw = true;
        }
        else if (count == NULL && isadigit(keyin))
        {
            // scan for our item
            int res = _digit_to_index(keyin, oper);
            if (res != -1)
            {
                ret = res;
                if (check_warning_inscriptions(you.inv[ret], oper))
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
            else if (check_warning_inscriptions(you.inv[ret], oper))
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

bool prompt_failed(int retval, string msg)
{
    if (retval != PROMPT_ABORT && retval != PROMPT_NOTHING)
        return false;

    if (msg.empty())
    {
        if (retval == PROMPT_ABORT)
            canned_msg(MSG_OK);
    }
    else
        mpr(msg.c_str(), MSGCH_PROMPT);

    crawl_state.cancel_cmd_repeat();

    return true;
}

// Most items are wieldable, but this function check for items that needs to be
// wielded to be used normally.
bool item_is_wieldable(const item_def &item)
{
    const int type = item.base_type;
    return is_weapon(item) || is_deck(item)
           || type == OBJ_MISCELLANY
              && item.sub_type == MISC_LANTERN_OF_SHADOWS;
}

/*
 * Return wether an item can be evoked.
 * @param item      The item to check
 * @param reach     Do weapons of reaching count?
 * @param known     When set it returns true for items of unknown type which
 *                  might be evokable.
 * @param all_wands When set, it returns true for empty wands.
 * @param msg       Whether we need to print a message.
 * @param equip     When disabled, ignore wield and meld requirements.
 */
bool item_is_evokable(const item_def &item, bool reach, bool known,
                      bool all_wands, bool msg, bool equip)
{
    const string error = item_is_melded(item)
            ? "Your " + item.name(DESC_QUALNAME) + " is melded into your body."
            : "That item can only be evoked when wielded.";

    if (is_unrandom_artefact(item))
    {
        const unrandart_entry* entry = get_unrand_entry(item.special);

        if (entry->evoke_func && item_type_known(item))
        {
            if (item_is_equipped(item) && !item_is_melded(item) || !equip)
                return true;

            if (msg)
                mpr(error);

            return false;
        }
        // Unrandart might still be evokable (e.g., reaching)
    }

    const bool wielded = !equip || you.equip[EQ_WEAPON] == item.link
                                   && !item_is_melded(item);

    switch (item.base_type)
    {
    case OBJ_WANDS:
        if (you.species == SP_FELID)
        {
            if (msg)
                mpr("You cannot grasp it well enough.");
            return false;
        }

        if (all_wands)
            return true;

        if (item.plus2 == ZAPCOUNT_EMPTY)
        {
            if (msg)
                mpr("This wand has no charges.");
            return false;
        }
        return true;

    case OBJ_WEAPONS:
        if ((!wielded || !reach) && !msg)
            return false;

        if (reach && weapon_reach(item) > 2 && item_type_known(item))
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

    case OBJ_RODS:
        if (!wielded)
        {
            if (msg)
                mpr(error);
            return false;
        }
        return true;

    case OBJ_STAVES:
        if (!known && !item_type_known(item)
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

    case OBJ_MISCELLANY:
        if (is_deck(item))
        {
            if (!wielded)
            {
                if (msg)
                    mpr(error);
                return false;
            }
            return true;
        }

        if (item.sub_type != MISC_LANTERN_OF_SHADOWS
#if TAG_MAJOR_VERSION == 34
            && item.sub_type != MISC_BUGGY_EBONY_CASKET
#endif
            && item.sub_type != MISC_RUNE_OF_ZOT)
        {
            return true;
        }
        // else fall through
    default:
        if (msg)
            mpr("That item cannot be evoked!");
        return false;
    }
}
