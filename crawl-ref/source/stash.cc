/*
 *  File:       stash.cc
 *  Summary:    Classes tracking player stashes
 *  Written by: Darshan Shaligram
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "branch.h"
#include "chardump.h"
#include "cio.h"
#include "clua.h"
#include "command.h"
#include "describe.h"
#include "directn.h"
#include "food.h"
#include "itemname.h"
#include "itemprop.h"
#include "files.h"
#include "invent.h"
#include "items.h"
#include "Kills.h"
#include "libutil.h"
#include "menu.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "monstuff.h"
#include "notes.h"
#include "overmap.h"
#include "place.h"
#include "randart.h"
#include "shopping.h"
#include "spl-book.h"
#include "stash.h"
#include "stuff.h"
#include "tags.h"
#include "terrain.h"
#include "traps.h"
#include "travel.h"
#include "tutorial.h"
#include "view.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <algorithm>

// Global
StashTracker StashTrack;

#define ST_MAJOR_VER ((unsigned char) 4)
#define ST_MINOR_VER ((unsigned char) 8)

void stash_init_new_level()
{
    // If there's an existing stash level for Pan, blow it away.
    StashTrack.remove_level( level_id(LEVEL_PANDEMONIUM) );
    StashTrack.remove_level( level_id(LEVEL_PORTAL_VAULT) );
}

std::string userdef_annotate_item(const char *s, const item_def *item,
                                  bool exclusive)
{
#ifdef CLUA_BINDINGS
    if (exclusive)
        lua_set_exclusive_item(item);
    std::string ann;
    if (!clua.callfn(s, "u>s", item, &ann) && !clua.error.empty())
        mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
    if (exclusive)
        lua_set_exclusive_item(NULL);
    return (ann);
#else
    return ("");
#endif
}

std::string stash_annotate_item(const char *s,
                                const item_def *item,
                                bool exclusive = false)
{
    std::string text = userdef_annotate_item(s, item, exclusive);
    if (item->base_type == OBJ_BOOKS
            && item_type_known(*item)
            && item->sub_type != BOOK_MANUAL
            && item->sub_type != BOOK_DESTRUCTION
        || count_staff_spells(*item, true) > 1)
    {
        formatted_string fs;
        item_def dup = *item;
        spellbook_contents(dup, item->base_type == OBJ_BOOKS ? RBOOK_READ_SPELL
                                                             : RBOOK_USE_STAFF,
                           &fs);
        text += EOL;
        text += fs.tostring(2, -2);
    }
    return text;
}

bool is_stash(int x, int y)
{
    LevelStashes *ls = StashTrack.find_current_level();
    if (ls)
    {
        Stash *s = ls->find_stash(x, y);
        return (s && s->enabled);
    }
    return (false);
}

void describe_stash(int x, int y)
{
    LevelStashes *ls = StashTrack.find_current_level();
    if (ls)
    {
        Stash *s = ls->find_stash(x, y);
        if (s)
        {
            const std::string desc = s->description();
            if (!desc.empty())
                mprf(MSGCH_EXAMINE_FILTER, "[Stash: %s]", desc.c_str());
        }
    }
}


std::vector<item_def> Stash::get_items() const
{
    return items;
}

std::vector<item_def> item_list_in_stash( coord_def pos )
{
    std::vector<item_def> ret;

    LevelStashes *ls = StashTrack.find_current_level();
    if (ls)
    {
        Stash *s = ls->find_stash(pos.x, pos.y);
        if (s)
            ret = s->get_items();
    }

    return ret;
}

static void _fully_identify_item(item_def *item)
{
    if (!item || !is_valid_item(*item))
        return;

    set_ident_flags( *item, ISFLAG_IDENT_MASK );
    if (item->base_type != OBJ_WEAPONS)
        set_ident_type( *item, ID_KNOWN_TYPE );
}

// ----------------------------------------------------------------------
// Stash
// ----------------------------------------------------------------------

bool Stash::aggressive_verify = true;
std::vector<item_def> Stash::filters;

Stash::Stash(int xp, int yp) : enabled(true), items()
{
    // First, fix what square we're interested in
    if (xp == -1)
    {
        xp = you.pos().x;
        yp = you.pos().y;
    }
    x = (unsigned char) xp;
    y = (unsigned char) yp;
    abspos = GXM * (int) y + x;

    update();
}

bool Stash::are_items_same(const item_def &a, const item_def &b)
{
    const bool same = a.base_type == b.base_type
        && a.sub_type == b.sub_type
        && a.plus == b.plus
        && a.plus2 == b.plus2
        && a.special == b.special
        && a.colour == b.colour
        && a.flags == b.flags
        && a.quantity == b.quantity;

    // Account for rotting meat when comparing items.
    return (same
            || (a.base_type == b.base_type
                && (a.base_type == OBJ_CORPSES
                    || (a.base_type == OBJ_FOOD && a.sub_type == FOOD_CHUNK
                        && b.sub_type == FOOD_CHUNK))
                && a.plus == b.plus));
}

void Stash::filter(const std::string &str)
{
    std::string base = str;

    unsigned char subc = 255;
    std::string   subs = "";
    std::string::size_type cpos = base.find(":", 0);
    if (cpos != std::string::npos)
    {
        subc = atoi(subs.c_str());

        base = base.substr(0, cpos);
    }

    const int base_num = atoi(base.c_str());
    if (base_num == 0 && base != "0" || subc == 0 && subs != "0")
    {
        item_types_pair pair = item_types_by_name(str);
        if (pair.base_type == OBJ_UNASSIGNED)
        {
            Options.report_error("Invalid stash filter '" + str + "'");
            return;
        }
        filter(pair.base_type, pair.sub_type);
    }
    else
    {
        const object_class_type basec =
            static_cast<object_class_type>(base_num);
        filter(basec, subc);
    }
}

void Stash::filter(object_class_type base, unsigned char sub)
{
    item_def item;
    item.base_type = base;
    item.sub_type  = sub;

    filters.push_back(item);
}

bool Stash::is_filtered(const item_def &item)
{
    for (int i = 0, count = filters.size(); i < count; ++i)
    {
        const item_def &filter = filters[i];
        if (item.base_type == filter.base_type
            && (filter.sub_type == 255
                || item.sub_type == filter.sub_type))
        {
            if (is_artefact(item))
                return (false);
            if (filter.sub_type != 255 && !item_type_known(item))
                return (false);
            return (true);
        }
    }
    return (false);
}

bool Stash::unverified() const
{
    return (!verified);
}

bool Stash::pickup_eligible() const
{
    for (int i = 0, size = items.size(); i < size; ++i)
        if (item_needs_autopickup(items[i]))
            return (true);

    return (false);
}

bool Stash::is_boring_feature(dungeon_feature_type feat)
{
    switch (feat)
    {
    // Discard spammy dungeon features.
    case DNGN_SHALLOW_WATER:
    case DNGN_DEEP_WATER:
    case DNGN_LAVA:
    case DNGN_OPEN_DOOR:
    case DNGN_STONE_STAIRS_DOWN_I:
    case DNGN_STONE_STAIRS_DOWN_II:
    case DNGN_STONE_STAIRS_DOWN_III:
    case DNGN_STONE_STAIRS_UP_I:
    case DNGN_STONE_STAIRS_UP_II:
    case DNGN_STONE_STAIRS_UP_III:
    case DNGN_ESCAPE_HATCH_DOWN:
    case DNGN_ESCAPE_HATCH_UP:
    case DNGN_ENTER_SHOP:
    case DNGN_ABANDONED_SHOP:
    case DNGN_UNDISCOVERED_TRAP:
        return (true);
    default:
        return (grid_is_solid(feat));
    }
}

static bool _grid_has_mimic_item(const coord_def& pos)
{
    const monsters *mon = monster_at(pos);
    return (mon && mons_is_unknown_mimic(mon));
}

static bool _grid_has_perceived_item(const coord_def& pos)
{
    return (igrd(pos) != NON_ITEM || _grid_has_mimic_item(pos));
}

static bool _grid_has_perceived_multiple_items(const coord_def& pos)
{
    int count = 0;

    if (_grid_has_mimic_item(pos))
        ++count;

    for (stack_iterator si(pos); si && count < 2; ++si)
        ++count;

    return (count > 1);
}

void Stash::update()
{
    coord_def p(x,y);
    feat = grd(p);
    trap = NUM_TRAPS;

    if (is_boring_feature(feat))
        feat = DNGN_FLOOR;

    if (grid_is_trap(feat))
        trap = get_trap_type(p);

    // If this is your position, you know what's on this square
    if (p == you.pos())
    {
        // Zap existing items
        items.clear();

        // Now, grab all items on that square and fill our vector
        for (stack_iterator si(p); si; ++si)
            if (!is_filtered(*si))
                add_item(*si);

        verified = true;
    }
    // If this is not your position, the only thing we can do is verify that
    // what the player sees on the square is the first item in this vector.
    else
    {
        if (!_grid_has_perceived_item(p))
        {
            items.clear();
            verified = true;
            return;
        }

        // There's something on this square. Take a squint at it.
        const item_def *pitem;
        item_def mimic_item;
        if (_grid_has_mimic_item(p))
        {
            get_mimic_item(monster_at(p), mimic_item);
            pitem = &mimic_item;
        }
        else
        {
            pitem = &mitm[igrd(p)];
        }

        const item_def& item = *pitem;

        tutorial_first_item(item);

        if (!_grid_has_perceived_multiple_items(p))
            items.clear();

        // We knew of nothing on this square, so we'll assume this is the
        // only item here, but mark it as unverified unless we can see nothing
        // under the item.
        if (items.size() == 0)
        {
            if (!is_filtered(item))
                add_item(item);
            // Note that we could be lying here, since we can have
            // a verified falsehood (if there's a mimic.)
            verified = !_grid_has_perceived_multiple_items(p);
            return;
        }

        // There's more than one item in this pile. As long as the top item is
        // not filtered, we can check to see if it matches what we think the
        // top item is.

        if (is_filtered(item))
            return;

        const item_def &first = items[0];
        // Compare these items
        if (!are_items_same(first, item))
        {
            if (aggressive_verify)
            {
                // See if 'item' matches any of the items we have. If it does,
                // we'll just make that the first item and leave 'verified'
                // unchanged.

                // Start from 1 because we've already checked items[0]
                for (int i = 1, count = items.size(); i < count; ++i)
                {
                    if (are_items_same(items[i], item))
                    {
                        // Found it. Swap it to the front of the vector.
                        std::swap(items[i], items[0]);

                        // We don't set verified to true. If this stash was
                        // already unverified, it remains so.
                        return;
                    }
                }
            }

            // If this is unverified, forget last item on stack. This isn't
            // terribly clever, but it prevents the vector swelling forever.
            if (!verified)
                items.pop_back();

            // Items are different. We'll put this item in the front of our
            // vector, and mark this as unverified
            add_item(item, true);
            verified = false;
        }
    }
}

static bool _is_rottable(const item_def &item)
{
    return (item.base_type == OBJ_CORPSES
            || (item.base_type == OBJ_FOOD && item.sub_type == FOOD_CHUNK));
}

static short _min_rot(const item_def &item)
{
    if (item.base_type == OBJ_FOOD)
        return 0;

    if (item.base_type == OBJ_CORPSES && item.sub_type == CORPSE_SKELETON)
        return 0;

    if (!mons_skeleton(item.plus))
        return 0;
    else
        return -200;
}

// Returns the item name for a given item, with any appropriate
// stash-tracking pre/suffixes.
std::string Stash::stash_item_name(const item_def &item)
{
    std::string name = item.name(DESC_NOCAP_A);

    if (!_is_rottable(item))
        return name;

    if (item.plus2 <= _min_rot(item))
    {
        name += " (gone by now)";
        return name;
    }

    // Skeletons show no signs of rotting before they're gone
    if (item.base_type == OBJ_CORPSES && item.sub_type == CORPSE_SKELETON)
        return name;

    // Item was already seen to be rotten
    if (item.special < 100)
        return name;

    if (item.plus2 <= 0)
        name += " (skeletalised by now)";
    else if (item.plus2 < 100)
        name += " (rotten by now)";

    return name;
}

class StashMenu : public InvMenu
{
public:
    StashMenu() : InvMenu(MF_SINGLESELECT), can_travel(false)
    {
        set_type(MT_PICKUP);
        set_tag("stash");       // override "inventory" tag
    }
    unsigned char getkey() const;
public:
    bool can_travel;
protected:
    void draw_title();
    bool process_key(int key);
};

void StashMenu::draw_title()
{
    if (title)
    {
        cgotoxy(1, 1);
        textcolor(title->colour);
        cprintf( "%s", title->text.c_str());
        if (title->quantity)
            cprintf(", %d item%s", title->quantity,
                                   title->quantity == 1? "" : "s");
        cprintf(")");
        if (can_travel)
            cprintf("  [ENTER: travel]");
    }
}

bool StashMenu::process_key(int key)
{
    if (key == CK_ENTER)
    {
        // Travel activates.
        lastch = 1;
        return (false);
    }
    return Menu::process_key(key);
}

unsigned char StashMenu::getkey() const
{
    return (lastch);
}

static MenuEntry *stash_menu_fixup(MenuEntry *me)
{
    const item_def *item = static_cast<const item_def *>( me->data );
    if (item->base_type == OBJ_GOLD)
    {
        me->quantity = 0;
        me->colour   = DARKGREY;
    }

    return (me);
}

bool Stash::show_menu(const std::string &prefix, bool can_travel) const
{
    StashMenu menu;

    MenuEntry *mtitle = new MenuEntry("Stash (" + prefix, MEL_TITLE);
    menu.can_travel   = can_travel;
    mtitle->quantity  = items.size();
    menu.set_title(mtitle);
    menu.load_items( InvMenu::xlat_itemvect(items), stash_menu_fixup);

    std::vector<MenuEntry*> sel;
    while (true)
    {
        sel = menu.show();
        if (menu.getkey() == 1)
            return (true);

        if (sel.size() != 1)
            break;

        item_def *item = static_cast<item_def *>( sel[0]->data );
        describe_item(*item);
    }
    return (false);
}

std::string Stash::description() const
{
    if (!enabled || items.empty())
        return ("");

    const item_def &item = items[0];
    std::string desc = stash_item_name(item);

    size_t sz = items.size();
    if (sz > 1)
    {
        char additionals[50];
        snprintf(additionals, sizeof additionals,
                " (...%ld)",
                 (unsigned long) (sz - 1));
        desc += additionals;
    }
    return (desc);
}

std::string Stash::feature_description() const
{
    if (feat == DNGN_FLOOR)
        return ("");

    return (::feature_description(feat, trap));
}

bool Stash::matches_search(const std::string &prefix,
                           const base_pattern &search,
                           stash_search_result &res) const
{
    if (!enabled || items.empty() && feat == DNGN_FLOOR)
        return (false);

    for (unsigned i = 0; i < items.size(); ++i)
    {
        const item_def &item = items[i];
        std::string s   = stash_item_name(item);
        std::string ann = stash_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item);
        if (search.matches(prefix + " " + ann + s))
        {
            if (!res.count++)
                res.match = s;
            res.matches += item.quantity;
            continue;
        }

        if (is_dumpable_artefact(item, false))
        {
            std::string desc =
                munge_description(get_item_description(item, false, true));

            if (search.matches(desc))
            {
                if (!res.count++)
                    res.match = s;
                res.matches += item.quantity;
            }
        }
    }

    if (!res.matches && feat != DNGN_FLOOR)
    {
        const std::string fdesc = feature_description();
        if (!fdesc.empty() && search.matches(fdesc))
        {
            res.match = fdesc;
            res.matches = 1;
        }
    }

    if (res.matches)
    {
        res.stash = this;
        // XXX pos.pos looks lame. Lameness is not solicited.
        res.pos.pos.x = x;
        res.pos.pos.y = y;
    }

    return !!res.matches;
}

void Stash::_update_corpses(long rot_time)
{
    for (int i = items.size() - 1; i >= 0; i--)
    {
        item_def &item = items[i];

        if (!_is_rottable(item))
            continue;

        long new_rot = static_cast<long>(item.plus2) - rot_time;

        if (new_rot <= _min_rot(item))
        {
            items.erase(items.begin() + i);
            continue;
        }
        item.plus2 = static_cast<short>(new_rot);
    }
}

void Stash::add_item(const item_def &item, bool add_to_front)
{
    if (_is_rottable(item))
        StashTrack.update_corpses();

    if (add_to_front)
        items.insert(items.begin(), item);
    else
        items.push_back(item);

    if (!_is_rottable(item))
        return;

    // item.special remains unchanged in the stash, to show how fresh it
    // was when last seen.  It's plus2 that's decayed over time.
    if (add_to_front)
    {
        item_def &it = items.front();
        it.plus2     = it.special;
    }
    else
    {
        item_def &it = items.back();
        it.plus2     = it.special;
    }
}

void Stash::write(std::ostream &os, int refx, int refy,
                  std::string place, bool identify)
    const
{
    if (!enabled || (items.size() == 0 && verified))
        return;

    bool note_status = notes_are_active();
    activate_notes(false);

    os << "(" << ((int) x - refx) << ", " << ((int) y - refy)
       << (place.length()? ", " + place : "")
       << ")"
       << std::endl;

    char buf[ITEMNAME_SIZE];
    for (int i = 0; i < (int) items.size(); ++i)
    {
        item_def item = items[i];

        if (identify)
            _fully_identify_item(&item);

        std::string s = stash_item_name(item);
        strncpy(buf, s.c_str(), sizeof buf);

        std::string ann = userdef_annotate_item(STASH_LUA_DUMP_ANNOTATE, &item);

        if (!ann.empty())
        {
            trim_string(ann);
            ann = " " + ann;
        }

        os << "  " << buf
           << (!ann.empty()? ann : std::string())
           << (!verified && (items.size() > 1 || i) ? " (still there?)" : "")
           << std::endl;

        if (is_dumpable_artefact(item, false))
        {
            std::string desc =
                munge_description(get_item_description(item, false, true));

            // Kill leading and trailing whitespace
            desc.erase(desc.find_last_not_of(" \n\t") + 1);
            desc.erase(0, desc.find_first_not_of(" \n\t"));
            // If string is not-empty, pad out to a neat indent
            if (desc.length())
            {
                // Walk backwards and prepend indenting spaces to \n characters.
                for (int j = desc.length() - 1; j >= 0; --j)
                    if (desc[j] == '\n')
                        desc.insert(j + 1, " ");

                os << "    " << desc << std::endl;
            }
        }
    }

    if (items.size() <= 1 && !verified)
        os << "  (unseen)" << std::endl;

    activate_notes(note_status);
}

void Stash::save(writer& outf) const
{
    // How many items on this square?
    marshallShort(outf, (short) items.size());

    marshallByte(outf, x);
    marshallByte(outf, y);

    marshallByte(outf, feat);
    marshallByte(outf, trap);

    // Note: Enabled save value is inverted logic, so that it defaults to true
    marshallByte(outf,
        (unsigned char) ((verified? 1 : 0) | (!enabled? 2 : 0)) );

    // And dump the items individually. We don't bother saving fields we're
    // not interested in (and don't anticipate being interested in).
    for (unsigned i = 0; i < items.size(); ++i)
        marshallItem(outf, items[i]);
}

void Stash::load(reader& inf)
{
    // How many items?
    int count = unmarshallShort(inf);

    x = unmarshallByte(inf);
    y = unmarshallByte(inf);

    feat =  static_cast<dungeon_feature_type>(
                static_cast<unsigned char>( unmarshallByte(inf) ));
    trap =  static_cast<trap_type>(
                static_cast<unsigned char>( unmarshallByte(inf) ));

    unsigned char flags = unmarshallByte(inf);
    verified = (flags & 1) != 0;

    // Note: Enabled save value is inverted so it defaults to true.
    enabled  = (flags & 2) == 0;

    abspos = GXM * (int) y + x;

    // Zap out item vector, in case it's in use (however unlikely)
    items.clear();
    // Read in the items
    for (int i = 0; i < count; ++i)
    {
        item_def item;
        unmarshallItem(inf, item);

        items.push_back(item);
    }
}

std::ostream &operator << (std::ostream &os, const Stash &s)
{
    s.write(os);
    return os;
}

ShopInfo::ShopInfo(int xp, int yp) : x(xp), y(yp), name(), shoptype(-1),
                                     visited(false), items()
{
    // Most of our initialization will be done externally; this class is really
    // a mildly glorified struct.
    const shop_struct *sh = get_shop(coord_def(x, y));
    if (sh)
        shoptype = sh->type;
}

void ShopInfo::add_item(const item_def &sitem, unsigned price)
{
    shop_item it;
    it.item  = sitem;
    it.price = price;
    items.push_back(it);
}

std::string ShopInfo::shop_item_name(const shop_item &si) const
{
    char shopitem[ITEMNAME_SIZE * 2];

    const unsigned long oldflags = si.item.flags;

    if (shoptype_identifies_stock(static_cast<shop_type>(this->shoptype)))
        const_cast<shop_item&>(si).item.flags |= ISFLAG_IDENT_MASK;

    const std::string itemname = Stash::stash_item_name(si.item);
    snprintf(shopitem, sizeof shopitem, "%s (%u gold)",
             itemname.c_str(), si.price);

    if (oldflags != si.item.flags)
        const_cast<shop_item&>(si).item.flags = oldflags;

    return shopitem;
}

std::string ShopInfo::shop_item_desc(const shop_item &si) const
{
    std::string desc;

    const unsigned long oldflags = si.item.flags;

    if (shoptype_identifies_stock(static_cast<shop_type>(this->shoptype)))
        const_cast<shop_item&>(si).item.flags |= ISFLAG_IDENT_MASK;

    if (is_dumpable_artefact(si.item, false))
    {
        desc = munge_description(get_item_description(si.item, false, true));
        trim_string(desc);

        // Walk backwards and prepend indenting spaces to \n characters
        for (int i = desc.length() - 1; i >= 0; --i)
            if (desc[i] == '\n')
                desc.insert(i + 1, " ");
    }

    if (oldflags != si.item.flags)
        const_cast<shop_item&>(si).item.flags = oldflags;

    return desc;
}

void ShopInfo::describe_shop_item(const shop_item &si) const
{
    const unsigned long oldflags = si.item.flags;

    if (shoptype_identifies_stock(static_cast<shop_type>(this->shoptype)))
        const_cast<shop_item&>(si).item.flags |= ISFLAG_IDENT_MASK;

    item_def it = static_cast<item_def>(si.item);
    describe_item( it );

    if ( oldflags != si.item.flags )
        const_cast<shop_item&>(si).item.flags = oldflags;
}

bool ShopInfo::show_menu(const std::string &place,
                         bool can_travel) const
{
    StashMenu menu;

    MenuEntry *mtitle = new MenuEntry(name + " (" + place, MEL_TITLE);
    menu.can_travel = can_travel;
    mtitle->quantity = items.size();
    menu.set_title(mtitle);

    menu_letter hotkey;
    if (items.empty())
    {
        MenuEntry *me = new MenuEntry(
                visited? "  (Shop is empty)" : "  (Shop contents are unknown)",
                MEL_ITEM,
                0,
                0);
        me->colour = DARKGREY;
        menu.add_entry(me);
    }
    else
    {
        for (int i = 0, count = items.size(); i < count; ++i)
        {
            MenuEntry *me = new MenuEntry(shop_item_name(items[i]),
                                          MEL_ITEM,
                                          1,
                                          hotkey++);
            me->data = const_cast<shop_item *>( &items[i] );
            menu.add_entry(me);
        }
    }

    std::vector<MenuEntry*> sel;
    while (true)
    {
        sel = menu.show();
        if (menu.getkey() == 1)
            return (true);

        if (sel.size() != 1)
            break;

        const shop_item *item = static_cast<const shop_item *>( sel[0]->data );
        describe_shop_item(*item);
    }
    return (false);
}

std::string ShopInfo::description() const
{
    return (name);
}

bool ShopInfo::matches_search(const std::string &prefix,
                              const base_pattern &search,
                              stash_search_result &res) const
{
    if (items.empty() && visited)
        return (false);

    bool note_status = notes_are_active();
    activate_notes(false);

    bool match = false;

    for (unsigned i = 0; i < items.size(); ++i)
    {
        std::string sname = shop_item_name(items[i]);
        std::string ann   = stash_annotate_item( STASH_LUA_SEARCH_ANNOTATE,
                                                  &items[i].item, true );

        bool thismatch = false;
        if (search.matches(prefix + " " + ann + sname))
            thismatch = true;
        else
        {
            std::string desc = shop_item_desc(items[i]);
            if (search.matches(desc))
                thismatch = true;
        }

        if (thismatch)
        {
            if (!res.count++)
                res.match = sname;
            res.matches++;
        }
    }

    if (!res.matches)
    {
        std::string shoptitle = prefix + " {shop} " + name;
        if (!visited && items.empty())
            shoptitle += "*";
        if (search.matches(shoptitle))
        {
            match = true;
            res.match = name;
        }
    }

    if (match || res.matches)
    {
        res.shop = this;
        res.pos.pos.x = x;
        res.pos.pos.y = y;
    }

    activate_notes(note_status);
    return (match || res.matches);
}

void ShopInfo::write(std::ostream &os, bool identify) const
{
    bool note_status = notes_are_active();
    activate_notes(false);
    os << "[Shop] " << name << std::endl;
    if (items.size() > 0)
    {
        for (unsigned i = 0; i < items.size(); ++i)
        {
            shop_item item = items[i];

            if (identify)
                _fully_identify_item(&item.item);

            os << "  " << shop_item_name(item) << std::endl;
            std::string desc = shop_item_desc(item);
            if (desc.length() > 0)
                os << "    " << desc << std::endl;
        }
    }
    else if (visited)
        os << "  (Shop is empty)" << std::endl;
    else
        os << "  (Shop contents are unknown)" << std::endl;

    activate_notes(note_status);
}

void ShopInfo::save(writer& outf) const
{
    marshallShort(outf, shoptype);

    int mangledx = (short) x;
    if (!visited)
        mangledx |= 1024;
    marshallShort(outf, mangledx);
    marshallShort(outf, (short) y);

    marshallShort(outf, (short) items.size());

    marshallString4(outf, name);

    for (unsigned i = 0; i < items.size(); ++i)
    {
        marshallItem(outf, items[i].item);
        marshallShort(outf, (short) items[i].price );
    }
}

void ShopInfo::load(reader& inf)
{
    shoptype = unmarshallShort(inf);

    x = unmarshallShort(inf);
    visited = !(x & 1024);
    x &= 0xFF;

    y = unmarshallShort(inf);

    int itemcount = unmarshallShort(inf);

    unmarshallString4(inf, name);
    for (int i = 0; i < itemcount; ++i)
    {
        shop_item item;
        unmarshallItem(inf, item.item);
        item.price = (unsigned) unmarshallShort(inf);
        items.push_back(item);
    }
}

std::ostream &operator << (std::ostream &os, const ShopInfo &s)
{
    s.write(os);
    return os;
}

LevelStashes::LevelStashes()
    : m_place(level_id::current()),
      m_stashes(),
      m_shops()
{
}

level_id LevelStashes::where() const
{
    return m_place;
}

Stash *LevelStashes::find_stash(int x, int y)
{
    if (x == -1 || y == -1)
    {
        x = you.pos().x;
        y = you.pos().y;
    }
    const int abspos = (GXM * y) + x;
    stashes_t::iterator st = m_stashes.find(abspos);
    return (st == m_stashes.end()? NULL : &st->second);
}

const Stash *LevelStashes::find_stash(int x, int y) const
{
    if (x == -1 || y == -1)
    {
        x = you.pos().x;
        y = you.pos().y;
    }
    const int abspos = (GXM * y) + x;
    stashes_t::const_iterator st = m_stashes.find(abspos);
    return (st == m_stashes.end()? NULL : &st->second);
}

const ShopInfo *LevelStashes::find_shop(int x, int y) const
{
    for (unsigned i = 0; i < m_shops.size(); ++i)
        if (m_shops[i].isAt(x, y))
            return (&m_shops[i]);

    return (NULL);
}

bool LevelStashes::shop_needs_visit(int x, int y) const
{
    const ShopInfo *shop = find_shop(x, y);
    return (shop && !shop->is_visited());
}

bool LevelStashes::needs_visit(int x, int y) const
{
    const Stash *s = find_stash(x, y);
    if (s && (s->unverified() || s->pickup_eligible()))
        return (true);

    return (shop_needs_visit(x, y));
}

ShopInfo &LevelStashes::get_shop(int x, int y)
{
    for (unsigned i = 0; i < m_shops.size(); ++i)
    {
        if (m_shops[i].isAt(x, y))
            return m_shops[i];
    }
    ShopInfo si(x, y);
    si.set_name(shop_name(coord_def(x, y)));
    m_shops.push_back(si);
    return get_shop(x, y);
}

// Updates the stash at (x,y). Returns true if there was a stash at (x,y), false
// otherwise.
bool LevelStashes::update_stash(int x, int y)
{
    Stash *s = find_stash(x, y);
    if (s)
    {
        s->update();
        if (s->empty())
            kill_stash(*s);
        return (true);
    }
    return (false);
}

// Removes a Stash from the level.
void LevelStashes::kill_stash(const Stash &s)
{
    m_stashes.erase(s.abs_pos());
}

void LevelStashes::no_stash(int x, int y)
{
    Stash *s = find_stash(x, y);
    bool en = false;
    if (s)
    {
        en = s->enabled = !s->enabled;
        s->update();
        if (s->empty())
            kill_stash(*s);
    }
    else
    {
        Stash newStash(x, y);
        newStash.enabled = false;

        m_stashes[ newStash.abs_pos() ] = newStash;
    }

    mpr(en? "I'll no longer ignore what I see on this square."
          : "Ok, I'll ignore what I see on this square.");
}

void LevelStashes::add_stash(int x, int y)
{
    Stash *s = find_stash(x, y);
    if (s)
    {
        s->update();
        if (s->empty())
            kill_stash(*s);
    }
    else
    {
        Stash new_stash(x, y);
        if (!new_stash.empty())
            m_stashes[ new_stash.abs_pos() ] = new_stash;
    }
}

bool LevelStashes::is_current() const
{
    return (m_place == level_id::current());
}

std::string LevelStashes::level_name() const
{
    return m_place.describe(true, true);
}

std::string LevelStashes::short_level_name() const
{
    return m_place.describe();
}

int LevelStashes::_num_enabled_stashes() const
{
    int rawcount = m_stashes.size();
    if (!rawcount)
        return (0);

    for (stashes_t::const_iterator iter = m_stashes.begin();
            iter != m_stashes.end(); iter++)
    {
        if (!iter->second.enabled)
            --rawcount;
    }
    return rawcount;
}

void LevelStashes::get_matching_stashes(
        const base_pattern &search,
        std::vector<stash_search_result> &results) const
{
    std::string lplace = "{" + m_place.describe() + "}";
    for (stashes_t::const_iterator iter = m_stashes.begin();
            iter != m_stashes.end(); iter++)
    {
        if (iter->second.enabled)
        {
            stash_search_result res;
            if (iter->second.matches_search(lplace, search, res))
            {
                res.pos.id = m_place;
                results.push_back(res);
            }
        }
    }

    for (unsigned i = 0; i < m_shops.size(); ++i)
    {
        stash_search_result res;
        if (m_shops[i].matches_search(lplace, search, res))
        {
            res.pos.id = m_place;
            results.push_back(res);
        }
    }
}

void LevelStashes::_update_corpses(long rot_time)
{
    for (stashes_t::iterator iter = m_stashes.begin();
            iter != m_stashes.end(); iter++)
    {
        iter->second._update_corpses(rot_time);
    }
}

void LevelStashes::write(std::ostream &os, bool identify) const
{
    if (visible_stash_count() == 0)
        return;

    os << level_name() << std::endl;

    for (unsigned i = 0; i < m_shops.size(); ++i)
        m_shops[i].write(os, identify);

    if (m_stashes.size())
    {
        const Stash &s = m_stashes.begin()->second;
        int refx = s.getX(), refy = s.getY();
        std::string levname = short_level_name();
        for (stashes_t::const_iterator iter = m_stashes.begin();
             iter != m_stashes.end(); iter++)
        {
            iter->second.write(os, refx, refy, levname, identify);
        }
    }
    os << std::endl;
}

void LevelStashes::save(writer& outf) const
{
    // How many stashes on this level?
    marshallShort(outf, (short) m_stashes.size());

    m_place.save(outf);

    // And write the individual stashes
    for (stashes_t::const_iterator iter = m_stashes.begin();
         iter != m_stashes.end(); iter++)
    {
        iter->second.save(outf);
    }

    marshallShort(outf, (short) m_shops.size());
    for (unsigned i = 0; i < m_shops.size(); ++i)
        m_shops[i].save(outf);
}

void LevelStashes::load(reader& inf)
{
    int size = unmarshallShort(inf);

    m_place.load(inf);

    m_stashes.clear();
    for (int i = 0; i < size; ++i)
    {
        Stash s;
        s.load(inf);
        if (!s.empty())
            m_stashes[ s.abs_pos() ] = s;
    }

    m_shops.clear();
    int shopc = unmarshallShort(inf);
    for (int i = 0; i < shopc; ++i)
    {
        ShopInfo si(0, 0);
        si.load(inf);
        m_shops.push_back(si);
    }
}

std::ostream &operator << (std::ostream &os, const LevelStashes &ls)
{
    ls.write(os);
    return os;
}

LevelStashes &StashTracker::get_current_level()
{
    return (levels[level_id::current()]);
}

LevelStashes *StashTracker::find_level(const level_id &id)
{
    stash_levels_t::iterator i = levels.find(id);
    return (i != levels.end()? &i->second : NULL);
}

LevelStashes *StashTracker::find_current_level()
{
    if (is_level_untrackable())
        return (NULL);

    return find_level(level_id::current());
}


bool StashTracker::update_stash(int x, int y)
{
    LevelStashes *lev = find_current_level();
    if (lev)
    {
        bool res = lev->update_stash(x, y);
        if (!lev->stash_count())
            remove_level();
        return (res);
    }
    return (false);
}

void StashTracker::remove_level(const level_id &place)
{
    levels.erase(place);
}

void StashTracker::no_stash(int x, int y)
{
    if (is_level_untrackable())
        return ;
    LevelStashes &current = get_current_level();
    current.no_stash(x, y);
    if (!current.stash_count())
        remove_level();
}

void StashTracker::add_stash(int x, int y, bool verbose)
{
    if (is_level_untrackable())
        return ;
    LevelStashes &current = get_current_level();
    current.add_stash(x, y);

    if (verbose)
    {
        Stash *s = current.find_stash(x, y);
        if (s && s->enabled)
            mpr("Added stash.");
    }

    if (!current.stash_count())
        remove_level();
}

void StashTracker::dump(const char *filename, bool identify) const
{
    std::ofstream outf(filename);
    if (outf)
    {
        write(outf, identify);
        outf.close();
    }
}

void StashTracker::write(std::ostream &os, bool identify) const
{
    os << you.your_name << std::endl << std::endl;
    if (!levels.size())
        os << "  You have no stashes." << std::endl;
    else
    {
        for (stash_levels_t::const_iterator iter = levels.begin();
             iter != levels.end(); iter++)
        {
            iter->second.write(os, identify);
        }
    }
}

void StashTracker::save(writer& outf) const
{
    // Write version info first - major + minor
    marshallByte(outf, ST_MAJOR_VER);
    marshallByte(outf, ST_MINOR_VER);

    // Time of last corpse update.
    marshallFloat(outf, (float) last_corpse_update);

    // How many levels have we?
    marshallShort(outf, (short) levels.size());

    // And ask each level to write itself to the tag
    stash_levels_t::const_iterator iter = levels.begin();
    for ( ; iter != levels.end(); iter++)
        iter->second.save(outf);
}

void StashTracker::load(reader& inf)
{
    // Check version. Compatibility isn't important, since stash-tracking
    // is non-critical.
    unsigned char major = unmarshallByte(inf),
                  minor = unmarshallByte(inf);
    if (major != ST_MAJOR_VER || minor != ST_MINOR_VER) return ;

    // Time of last corpse update.
    last_corpse_update = (double) unmarshallFloat(inf);

    int count = unmarshallShort(inf);

    levels.clear();
    for (int i = 0; i < count; ++i)
    {
        LevelStashes st;
        st.load(inf);
        if (st.stash_count())
            levels[st.where()] = st;
    }
}

void StashTracker::update_visible_stashes(
                                    StashTracker::stash_update_mode mode)
{
    if (is_level_untrackable())
        return;

    LevelStashes *lev = find_current_level();
    for (int cy = crawl_view.glos1.y; cy <= crawl_view.glos2.y; ++cy)
        for (int cx = crawl_view.glos1.x; cx <= crawl_view.glos2.x; ++cx)
        {
            if (!in_bounds(cx, cy) || !see_grid(cx, cy))
                continue;

            const dungeon_feature_type grid = grd[cx][cy];
            if ((!lev || !lev->update_stash(cx, cy))
                && mode == ST_AGGRESSIVE
                && (_grid_has_perceived_item(coord_def(cx,cy))
                    || !Stash::is_boring_feature(grid)))
            {
                if (!lev)
                    lev = &get_current_level();
                lev->add_stash(cx, cy);
            }

            if (grid == DNGN_ENTER_SHOP)
                get_shop(cx, cy);
        }

    if (lev && !lev->stash_count())
        remove_level();
}

#define SEARCH_SPAM_THRESHOLD 400
static std::string lastsearch;
static input_history search_history(15);

void StashTracker::show_stash_search_prompt()
{
    std::vector<std::string> opts;
    if (!lastsearch.empty())
        opts.push_back(
            make_stringf("Enter for \"%s\"", lastsearch.c_str()) );
    if (level_type_is_stash_trackable(you.level_type)
        && lastsearch != ".")
    {
        opts.push_back("? for help");
    }

    std::string prompt_qual =
        comma_separated_line(opts.begin(), opts.end(), ", or ", ", or ");

    if (!prompt_qual.empty())
        prompt_qual = " [" + prompt_qual + "]";

    mprf(MSGCH_PROMPT, "Search for what%s?", prompt_qual.c_str());
    // Push the cursor down to the next line. Newline on the prompt will not
    // do the trick on DOS.
    mpr("", MSGCH_PROMPT);
}

class stash_search_reader : public line_reader
{
public:
    stash_search_reader(char *buf, size_t sz,
                        int wcol = get_number_of_cols())
        : line_reader(buf, sz, wcol)
    {
    }
protected:
    int process_key(int ch)
    {
        if (ch == '?' && !pos)
        {
            *buffer = 0;
            return (ch);
        }
        return line_reader::process_key(ch);
    }
};

// helper for search_stashes
struct compare_by_distance
{
    bool operator()(const stash_search_result& lhs,
                    const stash_search_result& rhs)
    {
        if (lhs.player_distance != rhs.player_distance)
        {
            // Sort by increasing distance
            return (lhs.player_distance < rhs.player_distance);
        }
        else if (lhs.matches != rhs.matches)
        {
            // Then by decreasing number of matches
            return (lhs.matches > rhs.matches);
        }
        else
            return (false);
    }
};

// helper for search_stashes
struct compare_by_name
{
    bool operator()(const stash_search_result& lhs,
                    const stash_search_result& rhs)
    {
        if (lhs.match != rhs.match)
        {
            // Sort by name
            return (lhs.match < rhs.match);
        }
        else if (lhs.player_distance != rhs.player_distance)
        {
            // Then sort by increasing distance
            return (lhs.player_distance < rhs.player_distance);
        }
        else if (lhs.matches != rhs.matches)
        {
            // Then by decreasing number of matches
            return (lhs.matches > rhs.matches);
        }
        else
            return (false);
    }
};

void StashTracker::search_stashes()
{
    char buf[400];

    this->update_corpses();

    stash_search_reader reader(buf, sizeof buf);

    bool validline = false;
    while (true)
    {
        show_stash_search_prompt();

        int ret = reader.read_line();
        if (!ret)
        {
            validline = true;
            break;
        }
        else if (ret == '?')
        {
            show_stash_search_help();
            redraw_screen();
        }
        else
        {
            break;
        }
    }

    mesclr();
    if (!validline || (!*buf && !lastsearch.length()))
        return;

    std::string csearch = *buf? buf : lastsearch;
    std::string help = lastsearch;
    lastsearch = csearch;

    if (csearch == ".")
    {
        if (!level_type_is_stash_trackable(you.level_type))
        {
            mpr("Cannot track items on this level.");
            return;
        }
#if defined(REGEX_PCRE) || defined(REGEX_POSIX)
#define RE_ESCAPE "\\"
#else
#define RE_ESCAPE ""
#endif

        csearch = (RE_ESCAPE "{")
            + level_id::current().describe()
            + (RE_ESCAPE "}");
    }

    std::vector<stash_search_result> results;

    base_pattern *search = NULL;

    text_pattern tpat( csearch, true );
    search = &tpat;

    lua_text_pattern ltpat( csearch );

    if (lua_text_pattern::is_lua_pattern(csearch))
        search = &ltpat;

    if (!search->valid())
    {
        mpr("Your search expression is invalid.", MSGCH_PLAIN);
        lastsearch = help;
        return ;
    }

    get_matching_stashes(*search, results);

    if (results.empty())
    {
        mpr("Can't find anything matching that.", MSGCH_PLAIN);
        return;
    }

    if (results.size() > SEARCH_SPAM_THRESHOLD)
    {
        mpr("Too many matches; use a more specific search.", MSGCH_PLAIN);
        return;
    }

    bool sort_by_dist = true;
    while (true)
    {
        const char* sort_style;
        if (sort_by_dist)
        {
            std::sort(results.begin(), results.end(), compare_by_distance());
            sort_style = "by dist";
        }
        else
        {
            std::sort(results.begin(), results.end(), compare_by_name());
            sort_style = "by name";
        }

        const bool again = display_search_results(results, sort_style);
        if (!again)
            break;
        sort_by_dist = !sort_by_dist;
    }
}

void StashTracker::get_matching_stashes(
        const base_pattern &search,
        std::vector<stash_search_result> &results)
    const
{
    stash_levels_t::const_iterator iter = levels.begin();
    for ( ; iter != levels.end(); iter++)
    {
        iter->second.get_matching_stashes(search, results);
        if (results.size() > SEARCH_SPAM_THRESHOLD)
            return;
    }

    level_id curr = level_id::current();
    for (unsigned i = 0; i < results.size(); ++i)
    {
        int ldist = level_distance(curr, results[i].pos.id);
        if (ldist == -1)
            ldist = 1000;

        results[i].player_distance = ldist;
    }
}

class StashSearchMenu : public Menu
{
public:
    StashSearchMenu(const char* sort_style_)
        : Menu(), can_travel(true),
          request_toggle_sort_method(false),
//          allow_toggle(true), menu_action(ACT_EXECUTE),
          sort_style(sort_style_)
    { }

public:
    bool can_travel;
    bool request_toggle_sort_method;
    const char* sort_style;

protected:
    bool process_key(int key);
    void draw_title();
};

void StashSearchMenu::draw_title()
{
    if (title)
    {
        cgotoxy(1, 1);
        textcolor(title->colour);
        cprintf("%d %s%s, sorted %s",
                title->quantity, title->text.c_str(),
                title->quantity > 1? "es" : "",
                sort_style);

        char buf[200];
        snprintf(buf, 200,
                 "<lightgrey>  [<w>a-z</w>: %s  <w>?</w>/<w>!</w>: change action  <w>/</w>: change sort]",
                 menu_action == ACT_EXECUTE ? "travel" : "examine");

        draw_title_suffix(formatted_string::parse_string(buf), false);
    }
}


bool StashSearchMenu::process_key(int key)
{
    if (key == '/')
    {
        request_toggle_sort_method = true;
        return (false);
    }

    return Menu::process_key(key);
}

// Returns true to request redisplay with a different sort method
bool StashTracker::display_search_results(
    std::vector<stash_search_result> &results,
    const char* sort_style)
{
    if (results.empty())
        return (false);

    bool travelable = can_travel_interlevel();

    StashSearchMenu stashmenu(sort_style);
    stashmenu.set_tag("stash");
    stashmenu.can_travel = travelable;
    stashmenu.allow_toggle = true;
    stashmenu.menu_action = Menu::ACT_EXECUTE;
    std::string title = "match";

    MenuEntry *mtitle = new MenuEntry(title, MEL_TITLE);
    // Abuse of the quantity field.
    mtitle->quantity = results.size();
    stashmenu.set_title(mtitle);

    // Don't make a menu so tall that we recycle hotkeys on the same page.
    if (results.size() > 52
        && (stashmenu.maxpagesize() > 52 || stashmenu.maxpagesize() == 0))
    {
        stashmenu.set_maxpagesize(52);
    }

    menu_letter hotkey;
    for (unsigned i = 0; i < results.size(); ++i, ++hotkey)
    {
        stash_search_result &res = results[i];
        std::ostringstream matchtitle;
        matchtitle << "[" << short_place_name(res.pos.id) << "] "
                   << res.match;

        if (res.matches > 1 && res.count > 1)
            matchtitle << " (" << res.matches << ")";

        MenuEntry *me = new MenuEntry(matchtitle.str(), MEL_ITEM, 1, hotkey);
        me->data = &res;

        if (res.shop && !res.shop->is_visited())
            me->colour = CYAN;

        stashmenu.add_entry(me);
    }

    stashmenu.set_flags( MF_SINGLESELECT );

    std::vector<MenuEntry*> sel;
    while (true)
    {
        sel = stashmenu.show();

        if (stashmenu.request_toggle_sort_method)
            return (true);

        if (sel.size() == 1
            && stashmenu.menu_action == StashSearchMenu::ACT_EXAMINE)
        {
            stash_search_result *res =
                static_cast<stash_search_result *>(sel[0]->data);

            bool dotravel = false;
            if (res->shop)
            {
                dotravel = res->shop->show_menu(short_place_name(res->pos.id),
                                                can_travel_to(res->pos.id));
            }
            else if (res->stash)
            {
                dotravel = res->stash->show_menu(short_place_name(res->pos.id),
                                                 can_travel_to(res->pos.id));
            }

            if (dotravel && can_travel_to(res->pos.id))
            {
                redraw_screen();
                const travel_target lp = res->pos;
                start_translevel_travel(lp);
                return (false);
            }
            continue;
        }
        break;
    }

    redraw_screen();
    if (sel.size() == 1 && stashmenu.menu_action == Menu::ACT_EXECUTE)
    {
        const stash_search_result *res =
                static_cast<stash_search_result *>(sel[0]->data);
        const level_pos lp = res->pos;
        start_translevel_travel(lp);
    }
    return (false);
}

void StashTracker::update_corpses()
{
    if (you.elapsed_time - last_corpse_update < 20.0)
        return;

    const long rot_time = static_cast<long>((you.elapsed_time -
                                             last_corpse_update) / 20.0);

    last_corpse_update = you.elapsed_time;

    for (stash_levels_t::iterator iter = levels.begin();
            iter != levels.end(); iter++)
    {
        iter->second._update_corpses(rot_time);
    }
}
