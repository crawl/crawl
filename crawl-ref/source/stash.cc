/**
 * @file
 * @brief Classes tracking player stashes
**/

#include "AppHdr.h"

#include "stash.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

#include "chardump.h"
#include "clua.h"
#include "cluautil.h"
#include "command.h"
#include "coordit.h"
#include "describe.h"
#include "describe-spells.h"
#include "directn.h"
#include "env.h"
#include "feature.h"
#include "godpassive.h"
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h" // map_find
#include "menu.h"
#include "message.h"
#include "notes.h"
#include "output.h"
#include "religion.h"
#include "rot.h"
#include "spl-book.h"
#include "state.h"
#include "stringutil.h"
#include "syscalls.h"
#include "terrain.h"
#include "traps.h"
#include "travel.h"
#include "unicode.h"
#include "unwind.h"
#include "viewmap.h"

// Global
StashTracker StashTrack;

string userdef_annotate_item(const char *s, const item_def *item,
                             bool exclusive)
{
#ifdef CLUA_BINDINGS
    lua_stack_cleaner cleaner(clua);
    clua_push_item(clua, const_cast<item_def*>(item));
    if (!clua.callfn(s, 1, 1) && !clua.error.empty())
        mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
    string ann;
    if (lua_isstring(clua, -1))
        ann = luaL_checkstring(clua, -1);
    return ann;
#else
    return "";
#endif
}

string stash_annotate_item(const char *s, const item_def *item, bool exclusive)
{
    string text = userdef_annotate_item(s, item, exclusive);

    if (item->has_spells())
    {
        formatted_string fs;
        describe_spellset(item_spellset(*item), item, fs);
        text += "\n";
        text += fs.tostring();
    }

    // Include singular form (slice of pizza vs slices of pizza).
    if (item->quantity > 1)
    {
        text += " {";
        text += item->name(DESC_QUALNAME);
        text += "}";
    }

    // note that we can't add this in stash.lua (where most other annotations
    // are added) because that is shared between stash search annotations and
    // autopickup configuration annotations, and annotating an item based on
    // item_needs_autopickup while trying to decide if the item needs to be
    // autopickedup leads to infinite recursion
    if (Options.autopickup_search && item_needs_autopickup(*item))
        text += " {autopickup}";

    return text;
}

void maybe_update_stashes()
{
    if (!crawl_state.game_is_arena())
        StashTrack.update_visible_stashes();
}

bool is_stash(const coord_def& c)
{
    LevelStashes *ls = StashTrack.find_current_level();
    return ls && ls->find_stash(c);
}

string get_stash_desc(const coord_def& c)
{
    LevelStashes *ls = StashTrack.find_current_level();
    if (ls)
    {
        Stash *s = ls->find_stash(c);
        if (s)
        {
            const string desc = s->description();
            if (!desc.empty())
                return "[Stash: " + desc + "]";
        }
    }
    return "";
}

void describe_stash(const coord_def& c)
{
    string desc = get_stash_desc(c);
    if (!desc.empty())
        mprf(MSGCH_EXAMINE_FILTER, "%s", desc.c_str());
}

vector<item_def> Stash::get_items() const
{
    return items;
}

vector<item_def> item_list_in_stash(const coord_def& pos)
{
    vector<item_def> ret;

    LevelStashes *ls = StashTrack.find_current_level();
    if (ls)
    {
        Stash *s = ls->find_stash(pos);
        if (s)
            ret = s->get_items();
    }

    return ret;
}

static void _fully_identify_item(item_def *item)
{
    if (!item || !item->defined())
        return;

    set_ident_flags(*item, ISFLAG_IDENT_MASK);
    if (item->base_type != OBJ_WEAPONS)
        set_ident_type(*item, true);
}

// ----------------------------------------------------------------------
// Stash
// ----------------------------------------------------------------------

Stash::Stash(coord_def pos_) : items()
{
    // First, fix what square we're interested in
    if (pos_.origin())
        pos_ = you.pos();
    pos = pos_;

    update();
}

bool Stash::are_items_same(const item_def &a, const item_def &b, bool exact)
{
    const bool same = a.is_type(b.base_type, b.sub_type)
        // Ignore Gozag's gold flag, and rod charges.
        && (a.plus == b.plus || a.base_type == OBJ_GOLD && !exact
                             || a.base_type == OBJ_RODS && !exact)
        && a.plus2 == b.plus2
        && a.special == b.special
        && a.get_colour() == b.get_colour() // ????????
        && a.flags == b.flags
        && a.quantity == b.quantity;

    return same
           || (!exact && a.base_type == b.base_type
               && (a.base_type == OBJ_CORPSES
                   || (a.is_type(OBJ_FOOD, FOOD_CHUNK)
                       && b.sub_type == FOOD_CHUNK))
               && a.plus == b.plus);
}

bool Stash::unverified() const
{
    return !verified;
}

bool Stash::pickup_eligible() const
{
    for (const item_def &item : items)
        if (item_needs_autopickup(item))
            return true;

    return false;
}

bool Stash::needs_stop() const
{
    for (const item_def &item : items)
        if (!item_needs_autopickup(item))
            return true;

    return false;
}

bool Stash::is_boring_feature(dungeon_feature_type feature)
{
    // Count shops as boring features, because they are handled separately.
    return !is_notable_terrain(feature) || feature == DNGN_ENTER_SHOP;
}

static bool _grid_has_perceived_item(const coord_def& pos)
{
    return you.visible_igrd(pos) != NON_ITEM;
}

static bool _grid_has_perceived_multiple_items(const coord_def& pos)
{
    int count = 0;

    for (stack_iterator si(pos, true); si && count < 2; ++si)
        ++count;

    return count > 1;
}

bool Stash::unmark_trapping_nets()
{
    bool changed = false;
    for (auto &item : items)
        if (item_is_stationary_net(item))
            item.net_placed = false, changed = true;
    return changed;
}

void Stash::update()
{
    feat = grd(pos);
    trap = NUM_TRAPS;

    if (is_boring_feature(feat))
        feat = DNGN_FLOOR;

    if (feat_is_trap(feat))
    {
        trap = get_trap_type(pos);
        if (trap == TRAP_WEB)
            feat = DNGN_FLOOR, trap = TRAP_UNASSIGNED;
    }

    if (feat == DNGN_FLOOR)
        feat_desc = "";
    else
        feat_desc = feature_description_at(pos, false, DESC_A, false);

    // If this is your position, you know what's on this square
    if (pos == you.pos())
    {
        // Zap existing items
        items.clear();

        // Now, grab all items on that square and fill our vector
        for (stack_iterator si(pos, true); si; ++si)
            add_item(*si);

        verified = true;
    }
    // If this is not your position, the only thing we can do is verify that
    // what the player sees on the square is the first item in this vector.
    else
    {
        if (!_grid_has_perceived_item(pos))
        {
            items.clear();
            verified = true;
            return;
        }

        // There's something on this square. Take a squint at it.
        item_def *pitem = &mitm[you.visible_igrd(pos)];
        hints_first_item(*pitem);

        god_id_item(*pitem);
        maybe_identify_base_type(*pitem);
        const item_def& item = *pitem;

        if (!_grid_has_perceived_multiple_items(pos))
            items.clear();

        // We knew of nothing on this square, so we'll assume this is the
        // only item here, but mark it as unverified unless we can see nothing
        // under the item.
        if (items.empty())
        {
            if (!(item.flags & ISFLAG_UNOBTAINABLE))
                add_item(item);
            // Note that we could be lying here, since we can have
            // a verified falsehood (if there's a mimic.)
            verified = !_grid_has_perceived_multiple_items(pos);
            return;
        }

        // There's more than one item in this pile. Check to see if
        // the top item matches what we remember.
        const item_def &first = items[0];
        // Compare these items
        if (are_items_same(first, item))
        {
            // Replace the item to reflect seen recharging, etc.
            if (!are_items_same(first, item, true))
            {
                items.erase(items.begin());
                add_item(item, true);
            }
        }
        else
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
                    swap(items[i], items[0]);

                    // We don't set verified to true. If this stash was
                    // already unverified, it remains so.
                    return;
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
    if (is_shop_item(item))
        return false;
    return item.base_type == OBJ_CORPSES || item.is_type(OBJ_FOOD, FOOD_CHUNK);
}

static short _min_rot(const item_def &item)
{
    if (item.base_type == OBJ_FOOD)
        return 0;

    if (item.is_type(OBJ_CORPSES, CORPSE_SKELETON))
        return 0;

    if (!mons_skeleton(item.mon_type))
        return 0;
    else
        return -(FRESHEST_CORPSE);
}

// Returns the item name for a given item, with any appropriate
// stash-tracking pre/suffixes.
string Stash::stash_item_name(const item_def &item)
{
    string name = item.name(DESC_A);

    if (in_inventory(item))
    {
        name.insert(0, " (carried) ");
        return name;
    }

    if (!_is_rottable(item))
        return name;

    if (item.stash_freshness <= _min_rot(item))
    {
        name += " (gone by now)";
        return name;
    }

    // Skeletons show no signs of rotting before they're gone
    if (item.is_type(OBJ_CORPSES, CORPSE_SKELETON))
        return name;

    if (item.stash_freshness <= 0)
        name += " (skeletalised by now)";

    return name;
}

string Stash::description() const
{
    if (items.empty())
        return "";

    const item_def &item = items[0];
    string desc = stash_item_name(item);

    size_t sz = items.size();
    if (sz > 1)
    {
        char additionals[50];
        snprintf(additionals, sizeof additionals,
                " (...%u)",
                 (unsigned int) (sz - 1));
        desc += additionals;
    }
    return desc;
}

string Stash::feature_description() const
{
    return feat_desc;
}

vector<stash_search_result> Stash::matches_search(
    const string &prefix, const base_pattern &search) const
{
    vector<stash_search_result> results;
    if (empty())
        return results;

    for (const item_def &item : items)
    {
        const string s   = stash_item_name(item);
        const string ann = stash_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item);
        if (search.matches(prefix + " " + ann + " " + s)
            || is_dumpable_artefact(item) && search.matches(chardump_desc(item)))
        {
            stash_search_result res;
            res.match = s;
            res.item = item;
            results.push_back(res);
        }
    }

    if (results.empty() && feat != DNGN_FLOOR)
    {
        const string fdesc = feature_description();
        if (!fdesc.empty() && search.matches(fdesc))
        {
            stash_search_result res;
            res.match = fdesc;
            results.push_back(res);
        }
    }

    for (auto &res : results)
        res.pos.pos = pos;

    return results;
}

void Stash::_update_corpses(int rot_time)
{
    for (int i = items.size() - 1; i >= 0; i--)
    {
        item_def &item = items[i];

        if (!_is_rottable(item))
            continue;

        int new_rot = static_cast<int>(item.stash_freshness) - rot_time;

        if (new_rot <= _min_rot(item))
        {
            items.erase(items.begin() + i);
            continue;
        }
        item.stash_freshness = static_cast<short>(new_rot);
    }
}

void Stash::_update_identification()
{
    for (int i = items.size() - 1; i >= 0; i--)
    {
        god_id_item(items[i]);
        maybe_identify_base_type(items[i]);
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

    seen_item(item);

    if (!_is_rottable(item))
        return;

    // item.freshness remains unchanged in the stash, to show how fresh it
    // was when last seen. It's stash_freshness that's decayed over time.
    item_def &it = add_to_front ? items.front() : items.back();
    it.stash_freshness     = it.freshness;
}

void Stash::write(FILE *f, coord_def refpos, string place, bool identify) const
{
    if (items.empty() && verified)
        return;

    no_notes nx;

    fprintf(f, "(%d, %d%s%s)\n", pos.x - refpos.x, pos.y - refpos.y,
            place.empty() ? "" : ", ", OUTS(place));

    for (int i = 0; i < (int) items.size(); ++i)
    {
        item_def item = items[i];

        if (identify)
            _fully_identify_item(&item);

        string s = stash_item_name(item);

        string ann = userdef_annotate_item(STASH_LUA_DUMP_ANNOTATE, &item);

        if (!ann.empty())
        {
            trim_string(ann);
            ann = " " + ann;
        }

        fprintf(f, "  %s%s%s\n", OUTS(s), OUTS(ann),
            (!verified && (items.size() > 1 || i) ? " (still there?)" : ""));

        if (is_dumpable_artefact(item))
        {
            string desc = chardump_desc(item);

            // Kill leading and trailing whitespace
            desc.erase(desc.find_last_not_of(" \n\t") + 1);
            desc.erase(0, desc.find_first_not_of(" \n\t"));
            // If string is not-empty, pad out to a neat indent
            if (!desc.empty())
            {
                // Walk backwards and prepend indenting spaces to \n characters.
                for (int j = desc.length() - 1; j >= 0; --j)
                    if (desc[j] == '\n')
                        desc.insert(j + 1, " ");

                fprintf(f, "    %s\n", OUTS(desc));
            }
        }
    }

    if (items.size() <= 1 && !verified)
        fprintf(f, "  (unseen)\n");
}

void Stash::save(writer& outf) const
{
    // How many items on this square?
    marshallShort(outf, (short) items.size());

    marshallByte(outf, pos.x);
    marshallByte(outf, pos.y);

    marshallByte(outf, feat);
    marshallByte(outf, trap);

    marshallString(outf, feat_desc);

    marshallByte(outf, verified? 1 : 0);

    // And dump the items individually. We don't bother saving fields we're
    // not interested in (and don't anticipate being interested in).
    for (const item_def &item : items)
        marshallItem(outf, item, true);
}

void Stash::load(reader& inf)
{
    // How many items?
    int count = unmarshallShort(inf);

    pos.x = unmarshallByte(inf);
    pos.y = unmarshallByte(inf);

    feat =  static_cast<dungeon_feature_type>(unmarshallUByte(inf));
    trap =  static_cast<trap_type>(unmarshallUByte(inf));
    feat_desc = unmarshallString(inf);

    uint8_t flags = unmarshallUByte(inf);
    verified = (flags & 1) != 0;

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

ShopInfo::ShopInfo(const shop_struct& shop_)
    : shop(shop_)
{
}

string ShopInfo::shop_item_name(const item_def &it) const
{
    return make_stringf("%s%s (%d gold)",
                        Stash::stash_item_name(it).c_str(),
                        shop_item_unknown(it) ? " (unknown)" : "",
                        item_price(it, shop));
}

string ShopInfo::shop_item_desc(const item_def &it) const
{
    string desc;

    item_def& item(const_cast<item_def&>(it));
    unwind_var<iflags_t>(item.flags);

    if (shoptype_identifies_stock(shop.type))
        item.flags |= ISFLAG_IDENT_MASK;

    if (is_dumpable_artefact(item))
    {
        desc = chardump_desc(item);
        trim_string(desc);

        // Walk backwards and prepend indenting spaces to \n characters
        for (int i = desc.length() - 1; i >= 0; --i)
            if (desc[i] == '\n')
                desc.insert(i + 1, " ");
    }

    return desc;
}

void ShopInfo::show_menu(const level_pos& pos) const
{
    if (!is_visited())
        return;
    // ShopMenu shouldn't actually modify the shop, since it only does so if
    // you buy something.
    ::shop(const_cast<shop_struct&>(shop), pos);
}

vector<stash_search_result> ShopInfo::matches_search(
    const string &prefix, const base_pattern &search) const
{
    vector<stash_search_result> results;

    no_notes nx;

    const string shoptitle = shop_name(shop) + (shop.stock.empty() ? "*" : "");
    if (search.matches(shoptitle + " " + prefix + " {shop}"))
    {
        stash_search_result res;
        res.match = shoptitle;
        res.shop = this;
        res.pos.pos = shop.pos;
        results.push_back(res);
        // if the player is just searching for shops, don't show contents
        if (search.matches("shop"))
            return results;
    }

    for (const item_def &item : shop.stock)
    {
        const string sname = shop_item_name(item);
        const string ann   = stash_annotate_item(STASH_LUA_SEARCH_ANNOTATE,
                                                 &item, true);

        if (search.matches(prefix + " " + ann + " " + sname)
            || search.matches(shop_item_desc(item)))
        {
            stash_search_result res;
            res.match = sname;
            res.item = item;
            res.pos.pos = shop.pos;
            results.push_back(res);
        }
    }

    return results;
}

void ShopInfo::write(FILE *f, bool identify) const
{
    no_notes nx;
    fprintf(f, "[Shop] %s\n", OUTS(shop_name(shop)));
    if (!shop.stock.empty())
    {
        for (item_def item : shop.stock) // intentional copy
        {
            if (identify)
                _fully_identify_item(&item);

            fprintf(f, "  %s\n", OUTS(shop_item_name(item)));
            string desc = shop_item_desc(item);
            if (!desc.empty())
                fprintf(f, "    %s\n", OUTS(desc));
        }
    }
    else
        fprintf(f, "  (Shop contents are unknown)\n");
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

Stash *LevelStashes::find_stash(coord_def c)
{
    return map_find(m_stashes, c);
}

const Stash *LevelStashes::find_stash(coord_def c) const
{
    return map_find(m_stashes, c);
}

const ShopInfo *LevelStashes::find_shop(const coord_def& c) const
{
    for (const ShopInfo &shop : m_shops)
        if (shop.is_at(c))
            return &shop;

    return nullptr;
}

bool LevelStashes::shop_needs_visit(const coord_def& c) const
{
    const ShopInfo *shop = find_shop(c);
    return shop && !shop->is_visited();
}

bool LevelStashes::needs_visit(const coord_def& c, bool autopickup) const
{
    const Stash *s = find_stash(c);
    if (s && (s->unverified()
              || autopickup && s->pickup_eligible()))
    {
        return true;
    }
    return shop_needs_visit(c);
}

bool LevelStashes::needs_stop(const coord_def &c) const
{
    const Stash *s = find_stash(c);
    return s && s->unverified() && s->needs_stop();
}

ShopInfo &LevelStashes::get_shop(const coord_def& c)
{
    for (ShopInfo &shop : m_shops)
        if (shop.is_at(c))
            return shop;

    shop_struct shop = *shop_at(c);
    shop.stock.clear(); // You can't see it from afar.
    m_shops.emplace_back(shop);
    return m_shops.back();
}

// Updates the stash at p. Returns true if there was a stash at p, false
// otherwise.
bool LevelStashes::update_stash(const coord_def& c)
{
    Stash *s = find_stash(c);
    if (!s)
        return false;

    s->update();
    if (s->empty())
        kill_stash(*s);
    return true;
}

bool LevelStashes::unmark_trapping_nets(const coord_def &c)
{
    if (Stash *s = find_stash(c))
        return s->unmark_trapping_nets();
    else
        return false;
}

void LevelStashes::move_stash(const coord_def& from, const coord_def& to)
{
    ASSERT(from != to);

    Stash *s = find_stash(from);
    if (!s)
        return;

    coord_def old_pos = s->pos;
    s->pos = to;
    m_stashes[s->pos] = *s;
    m_stashes.erase(old_pos);
}

// Removes a Stash from the level.
void LevelStashes::kill_stash(const Stash &s)
{
    m_stashes.erase(s.pos);
}

void LevelStashes::add_stash(coord_def p)
{
    Stash *s = find_stash(p);
    if (s)
    {
        s->update();
        if (s->empty())
            kill_stash(*s);
    }
    else
    {
        Stash new_stash(p);
        if (!new_stash.empty())
            m_stashes[new_stash.pos] = new_stash;
    }
}

bool LevelStashes::is_current() const
{
    return m_place == level_id::current();
}

string LevelStashes::level_name() const
{
    return m_place.describe(true, true);
}

string LevelStashes::short_level_name() const
{
    return m_place.describe();
}

void LevelStashes::_waypoint_search(
        int n,
        vector<stash_search_result> &results) const
{
    level_pos waypoint = travel_cache.get_waypoint(n);
    if (!waypoint.is_valid() || waypoint.id != m_place)
        return;
    const Stash* stash = find_stash(waypoint.pos);
    if (!stash)
        return;
    vector<stash_search_result> new_results =
        stash->matches_search("", text_pattern(".*"));
    for (auto &res : new_results)
    {
        res.pos.id = m_place;
        results.push_back(res);
    }
}

void LevelStashes::get_matching_stashes(
        const base_pattern &search,
        vector<stash_search_result> &results) const
{
    string lplace = "{" + m_place.describe() + "}";

    // a single digit or * means we're searching for waypoints' content.
    const string s = search.tostring();
    if (s == "*")
    {
        for (int i = 0; i < TRAVEL_WAYPOINT_COUNT; ++i)
            _waypoint_search(i, results);
        return;
    }
    else if (s.size() == 1 && s[0] >= '0' && s[0] <= '9')
    {
        _waypoint_search(s[0] - '0', results);
        return;
    }

    for (const auto &entry : m_stashes)
    {
        vector<stash_search_result> new_results =
            entry.second.matches_search(lplace, search);
        for (auto &res : new_results)
        {
            res.pos.id = m_place;
            results.push_back(res);
        }
    }

    for (const ShopInfo &shop : m_shops)
    {
        vector<stash_search_result> new_results =
            shop.matches_search(lplace, search);
        for (auto &res : new_results)
        {
            res.pos.id = m_place;
            results.push_back(res);
        }
    }
}

void LevelStashes::_update_corpses(int rot_time)
{
    for (auto &entry : m_stashes)
        entry.second._update_corpses(rot_time);
}

void LevelStashes::_update_identification()
{
    for (auto &entry : m_stashes)
        entry.second._update_identification();
}

void LevelStashes::write(FILE *f, bool identify) const
{
    if (!has_stashes())
        return;

    // very unlikely level names will be localized, but hey
    fprintf(f, "%s\n", OUTS(level_name()));

    for (const ShopInfo &shop : m_shops)
        shop.write(f, identify);

    if (m_stashes.size())
    {
        const Stash &s = m_stashes.begin()->second;
        string levname = short_level_name();
        for (const auto &entry : m_stashes)
            entry.second.write(f, s.pos, levname, identify);
    }
    fprintf(f, "\n");
}

void LevelStashes::save(writer& outf) const
{
    // How many stashes on this level?
    marshallShort(outf, (short) m_stashes.size());

    m_place.save(outf);

    // And write the individual stashes
    for (const auto &entry : m_stashes)
        entry.second.save(outf);

    marshallShort(outf, (short) m_shops.size());
    for (const ShopInfo &shop : m_shops)
        shop.save(outf);
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
            m_stashes[s.pos] = s;
    }

    m_shops.clear();
    int shopc = unmarshallShort(inf);
    for (int i = 0; i < shopc; ++i)
    {
        m_shops.emplace_back();
        m_shops.back().load(inf);
    }
}

void LevelStashes::remove_shop(const coord_def& c)
{
    for (unsigned i = 0; i < m_shops.size(); ++i)
        if (m_shops[i].is_at(c))
        {
            m_shops.erase(m_shops.begin() + i);
            return;
        }
}

LevelStashes &StashTracker::get_current_level()
{
    return levels[level_id::current()];
}

LevelStashes *StashTracker::find_level(const level_id &id)
{
    return map_find(levels, id);
}

LevelStashes *StashTracker::find_current_level()
{
    return find_level(level_id::current());
}

bool StashTracker::update_stash(const coord_def& c)
{
    LevelStashes *lev = find_current_level();
    if (lev)
    {
        bool res = lev->update_stash(c);
        if (!lev->has_stashes())
            remove_level();
        return res;
    }
    return false;
}

void StashTracker::move_stash(const coord_def& from, const coord_def& to)
{
    if (LevelStashes *lev = find_current_level())
        lev->move_stash(from, to);
}

bool StashTracker::unmark_trapping_nets(const coord_def &c)
{
    if (LevelStashes *lev = find_current_level())
        return lev->unmark_trapping_nets(c);
    else
        return false;
}

void StashTracker::remove_level(const level_id &place)
{
    levels.erase(place);
}

void StashTracker::add_stash(coord_def p)
{
    LevelStashes &current = get_current_level();
    current.add_stash(p);

    if (!current.has_stashes())
        remove_level();
}

void StashTracker::dump(const char *filename, bool identify) const
{
    FILE *outf = fopen_u(filename, "w");
    if (outf)
    {
        write(outf, identify);
        fclose(outf);
    }
}

void StashTracker::write(FILE *f, bool identify) const
{
    fprintf(f, "%s\n\n", OUTS(you.your_name));
    if (!levels.size())
        fprintf(f, "  You have no stashes.\n");
    else
    {
        for (const auto &entry : levels)
            entry.second.write(f, identify);
    }
}

void StashTracker::save(writer& outf) const
{
    // Time of last corpse update.
    marshallInt(outf, last_corpse_update);

    // How many levels have we?
    marshallShort(outf, (short) levels.size());

    // And ask each level to write itself to the tag
    for (const auto &entry : levels)
        entry.second.save(outf);
}

void StashTracker::load(reader& inf)
{
    // Time of last corpse update.
    last_corpse_update = unmarshallInt(inf);

    int count = unmarshallShort(inf);

    levels.clear();
    for (int i = 0; i < count; ++i)
    {
        LevelStashes st;
        st.load(inf);
        if (st.has_stashes())
            levels[st.where()] = st;
    }
}

void StashTracker::update_visible_stashes()
{
    LevelStashes *lev = find_current_level();
    coord_def c;
    for (radius_iterator ri(you.pos(),
                            you.xray_vision ? LOS_NONE : LOS_DEFAULT); ri; ++ri)
    {
        const dungeon_feature_type feat = grd(*ri);

        if ((!lev || !lev->update_stash(*ri))
            && (_grid_has_perceived_item(*ri)
                || !Stash::is_boring_feature(feat)))
        {
            if (!lev)
                lev = &get_current_level();
            lev->add_stash(*ri);
        }

        if (feat == DNGN_ENTER_SHOP)
            get_shop(*ri);
    }

    if (lev && !lev->has_stashes())
        remove_level();
}

#define SEARCH_SPAM_THRESHOLD 400
static string lastsearch;
static input_history search_history(15);

string StashTracker::stash_search_prompt()
{
    vector<string> opts;
    if (!lastsearch.empty())
    {
        const string disp = replace_all(lastsearch, "<", "<<");
        opts.push_back(
            make_stringf("Enter for \"%s\"", disp.c_str()));
    }
    if (lastsearch != ".")
        opts.emplace_back("? for help");

    string prompt_qual =
        comma_separated_line(opts.begin(), opts.end(), ", or ", ", or ");

    if (!prompt_qual.empty())
        prompt_qual = " [" + prompt_qual + "]";

    return make_stringf("Search for what%s? ", prompt_qual.c_str());
}

void StashTracker::remove_shop(const level_pos &pos)
{
    LevelStashes *lev = find_level(pos.id);
    if (lev)
        lev->remove_shop(pos.pos);
}

class stash_search_reader : public line_reader
{
public:
    stash_search_reader(char *buf, size_t sz,
                        int wcol = get_number_of_cols())
        : line_reader(buf, sz, wcol)
    {
        set_input_history(&search_history);
#ifdef USE_TILE_WEB
        tag = "stash_search";
#endif
    }
protected:
    int process_key(int ch) override
    {
        if (ch == '?' && !pos)
        {
            *buffer = 0;
            return ch;
        }
        return line_reader::process_key(ch);
    }
};

// helper for search_stashes
static bool _compare_by_distance(const stash_search_result& lhs,
                                 const stash_search_result& rhs)
{
    if (lhs.player_distance != rhs.player_distance)
    {
        // Sort by increasing distance
        return lhs.player_distance < rhs.player_distance;
    }
    else if (lhs.player_distance == 0)
    {
        // If on the same level, sort by distance to player.
        const int lhs_dist = grid_distance(you.pos(), lhs.pos.pos);
        const int rhs_dist = grid_distance(you.pos(), rhs.pos.pos);
        if (lhs_dist != rhs_dist)
            return lhs_dist < rhs_dist;
    }

    if (lhs.match != rhs.match)
    {
        // Then by name.
        return lhs.match < rhs.match;
    }
    else
        return false;
}

// helper for search_stashes
static bool _compare_by_name(const stash_search_result& lhs,
                             const stash_search_result& rhs)
{
    if (lhs.match != rhs.match)
    {
        // Sort by name
        return lhs.match < rhs.match;
    }
    else if (lhs.player_distance != rhs.player_distance)
    {
        // Then sort by increasing distance
        return lhs.player_distance < rhs.player_distance;
    }
    else
        return false;
}

static vector<stash_search_result> _inventory_search(const base_pattern &search)
{
    vector<stash_search_result> results;
    for (const item_def &item : you.inv)
    {
        if (!item.defined())
            continue;

        const string s   = Stash::stash_item_name(item);
        const string ann = stash_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item);
        if (search.matches(ann + " " + s)
            || is_dumpable_artefact(item)
               && search.matches(chardump_desc(item)))
        {
            stash_search_result res;
            res.match = s;
            res.item = item;
            // Needs to not be equal to ITEM_IN_INVENTORY so the describe
            // menu doesn't think it can manipulate the item.
            res.item.pos = you.pos();
            res.in_inventory = true;
            res.pos = level_pos::current();
            results.push_back(res);
        }
    }

    return results;
}

void StashTracker::search_stashes()
{
    char buf[400];

    update_corpses();
    update_identification();

    stash_search_reader reader(buf, sizeof buf);

    bool validline = false;
    msgwin_prompt(stash_search_prompt());
    while (true)
    {
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
            break;
    }
    msgwin_reply(validline ? buf : "");

    clear_messages();
    if (!validline || (!*buf && lastsearch.empty()))
    {
        canned_msg(MSG_OK);
        return;
    }

    string csearch_literal = *buf? buf : lastsearch;
    string csearch = csearch_literal;

    bool curr_lev = (csearch[0] == '@' || csearch == ".");
    if (curr_lev)
    {
        csearch.erase(0, 1);
        if (csearch.length() == 0)
            csearch = ".";
    }

    base_pattern *search = nullptr;

    lua_text_pattern ltpat(csearch);
    text_pattern tpat(csearch, true);
    plaintext_pattern ptpat(csearch, true);

    if (lua_text_pattern::is_lua_pattern(csearch))
        search = &ltpat;
    else if (csearch[0] != '='
             && (csearch == "." || csearch == ".." || csearch[0] == '/'
                 || Options.regex_search))
    {
        if (csearch[0] == '/')
            csearch.erase(0, 1);
        tpat = csearch;
        search = &tpat;
    }
    else
    {
        if (csearch[0] == '=')
            csearch.erase(0, 1);
        ptpat = csearch;
        search = &ptpat;
    }

    if (!search->valid() && csearch != "*")
    {
        mprf(MSGCH_PLAIN, "Your search expression is invalid.");
        return ;
    }

    lastsearch = csearch_literal;

    vector<stash_search_result> results;
    if (!curr_lev)
        results = _inventory_search(*search);
    get_matching_stashes(*search, results, curr_lev);

    if (results.empty())
    {
        mprf(MSGCH_PLAIN, "Can't find anything matching that.");
        return;
    }

    if (results.size() > SEARCH_SPAM_THRESHOLD)
    {
        mprf(MSGCH_PLAIN, "Too many matches; use a more specific search.");
        return;
    }

    bool sort_by_dist = true;
    bool filter_useless = false;
    bool default_execute = true;
    while (true)
    {
        // Note that sort_by_dist and filter_useless can be modified by the
        // following call if requested by the user. Also, "results" will be
        // sorted by the call as appropriate:
        const bool again = display_search_results(results,
                                                  sort_by_dist,
                                                  filter_useless,
                                                  default_execute,
                                                  search,
                                                  csearch == "."
                                                  || csearch == "..");
        if (!again)
            break;
    }
}

void StashTracker::get_matching_stashes(
        const base_pattern &search,
        vector<stash_search_result> &results,
        bool curr_lev)
    const
{
    level_id curr = level_id::current();
    for (const auto &entry : levels)
    {
        if (curr_lev && curr != entry.first)
            continue;
        entry.second.get_matching_stashes(search, results);
        if (results.size() > SEARCH_SPAM_THRESHOLD)
            return;
    }

    for (stash_search_result &result : results)
    {
        int ldist = level_distance(curr, result.pos.id);
        if (ldist == -1)
            ldist = 1000;

        result.player_distance = ldist;
    }
}

class StashSearchMenu : public Menu
{
public:
    StashSearchMenu(const char* sort_style_,const char* filtered_)
        : Menu(),
          request_toggle_sort_method(false),
          request_toggle_filter_useless(false),
          sort_style(sort_style_),
          filtered(filtered_)
    { }

public:
    bool request_toggle_sort_method;
    bool request_toggle_filter_useless;
    const char* sort_style;
    const char* filtered;

protected:
    bool process_key(int key) override;
    void draw_title() override;
};

void StashSearchMenu::draw_title()
{
    if (title)
    {
        cgotoxy(1, 1);
        formatted_string fs = formatted_string(title->colour);
        fs.cprintf("%d %s%s",
                   title->quantity, title->text.c_str(),
                   title->quantity == 1 ? "" : "es");
        fs.display();

#ifdef USE_TILE_WEB
        webtiles_set_title(fs);
#endif

        draw_title_suffix(formatted_string::parse_string(make_stringf(
                 "<lightgrey>"
                 ": <w>%s</w> [toggle: <w>!</w>],"
                 " by <w>%s</w> [<w>/</w>],"
                 " <w>%s</w> useless [<w>=</w>]"
                 "</lightgrey>",
                 menu_action == ACT_EXECUTE ? "travel" : "view  ",
                 sort_style, filtered)), false);
    }
}

bool StashSearchMenu::process_key(int key)
{
    if (key == '/')
    {
        request_toggle_sort_method = true;
        return false;
    }
    else if (key == '=')
    {
        request_toggle_filter_useless = true;
        return false;
    }

    return Menu::process_key(key);
}

static void _stash_filter_useless(const vector<stash_search_result> &in,
                                  vector<stash_search_result> &out)
{
    // Creates search results vector with useless items filtered
    out.clear();
    out.reserve(in.size());
    for (const stash_search_result &res : in)
    {
        if (!res.item.defined() || !is_useless_item(res.item, false))
            out.push_back(res);
    }
}

// Returns true to request redisplay if display method was toggled
bool StashTracker::display_search_results(
    vector<stash_search_result> &results_in,
    bool& sort_by_dist,
    bool& filter_useless,
    bool& default_execute,
    base_pattern* search,
    bool nohl)
{
    if (results_in.empty())
        return false;

    vector<stash_search_result> * results;
    vector<stash_search_result> results_filtered;

    if (filter_useless)
    {
        _stash_filter_useless(results_in, results_filtered);
        results = &results_filtered;
    }
    else
        results = &results_in;

    if (sort_by_dist)
        stable_sort(results->begin(), results->end(), _compare_by_distance);
    else
        stable_sort(results->begin(), results->end(), _compare_by_name);

    StashSearchMenu stashmenu(sort_by_dist ? "dist" : "name",
                              filter_useless ? "hide" : "show");
    stashmenu.set_tag("stash");
    stashmenu.action_cycle = Menu::CYCLE_TOGGLE;
    stashmenu.menu_action  = default_execute ? Menu::ACT_EXECUTE : Menu::ACT_EXAMINE;
    string title = "match";

    MenuEntry *mtitle = new MenuEntry(title, MEL_TITLE);
    // Abuse of the quantity field.
    mtitle->quantity = results->size();
    stashmenu.set_title(mtitle);

    // Don't make a menu so tall that we recycle hotkeys on the same page.
    if (results->size() > 52
        && (stashmenu.maxpagesize() > 52 || stashmenu.maxpagesize() == 0))
    {
        stashmenu.set_maxpagesize(52);
    }

    menu_letter hotkey;
    for (stash_search_result &res : *results)
    {
        ostringstream matchtitle;
        if (!res.in_inventory)
        {
            if (const uint8_t waypoint = travel_cache.is_waypoint(res.pos))
                matchtitle << "(" << waypoint << ") ";
            matchtitle << "[" << res.pos.id.describe() << "] ";
        }

        matchtitle << res.match;

        MenuEntry *me = new MenuEntry(matchtitle.str(), MEL_ITEM, 1,
                                      res.in_inventory ? 0
                                                       : (int)hotkey);
        me->data = &res;

        if (res.shop && !res.shop->is_visited())
            me->colour = CYAN;

        if (res.item.defined())
        {
            const int itemcol = menu_colour(res.item.name(DESC_PLAIN).c_str(),
                                            item_prefix(res.item), "pickup");
            if (itemcol != -1)
                me->colour = itemcol;
        }

        stashmenu.add_entry(me);
        if (!res.in_inventory)
            ++hotkey;
    }

    stashmenu.set_flags(MF_SINGLESELECT | MF_ALLOW_FORMATTING);

    vector<MenuEntry*> sel;
    while (true)
    {
        sel = stashmenu.show();

        default_execute = stashmenu.menu_action == Menu::ACT_EXECUTE;
        if (stashmenu.request_toggle_sort_method)
        {
            sort_by_dist = !sort_by_dist;
            return true;
        }

        if (stashmenu.request_toggle_filter_useless)
        {
            filter_useless = !filter_useless;
            return true;
        }

        if (sel.size() == 1
            && stashmenu.menu_action == StashSearchMenu::ACT_EXAMINE)
        {
            stash_search_result *res =
                static_cast<stash_search_result *>(sel[0]->data);

            if (res->item.defined())
            {
                item_def it = res->item;
                describe_item(it,
                    [search, nohl](string& desc)
                    {
                        if (!nohl)
                            desc = search->match_location(desc).annotate_string("lightcyan");
                    });
            }
            else if (res->shop)
                res->shop->show_menu(res->pos);
            continue;
        }
        break;
    }

    redraw_screen();
    if (sel.size() == 1 && stashmenu.menu_action == Menu::ACT_EXECUTE)
    {
        const stash_search_result *res =
                static_cast<stash_search_result *>(sel[0]->data);
        level_pos lp = res->pos;
        if (show_map(lp, true, true, true))
            start_translevel_travel(lp);
        else
            return true;
    }
    return false;
}

void StashTracker::update_corpses()
{
    if (you.elapsed_time - last_corpse_update < ROT_TIME_FACTOR)
        return;

    const int rot_time =
        (you.elapsed_time - last_corpse_update) / ROT_TIME_FACTOR;

    last_corpse_update = you.elapsed_time;

    for (auto &entry : levels)
        entry.second._update_corpses(rot_time);
}

void StashTracker::update_identification()
{
    if (!have_passive(passive_t::identify_items))
        return;

    for (auto &entry : levels)
        entry.second._update_identification();
}

//////////////////////////////////////////////

ST_ItemIterator::ST_ItemIterator()
{
    m_stash_level_it = StashTrack.levels.begin();
    new_level();
    //(*this)++;
}

ST_ItemIterator::operator bool() const
{
    return m_item != nullptr;
}

const item_def& ST_ItemIterator::operator *() const
{
    return *m_item;
}

const item_def* ST_ItemIterator::operator->() const
{
    return m_item;
}

const level_id &ST_ItemIterator::place()
{
    return m_place;
}

const ShopInfo* ST_ItemIterator::shop()
{
    return m_shop;
}

unsigned        ST_ItemIterator::price()
{
    return m_price;
}

const ST_ItemIterator& ST_ItemIterator::operator ++ ()
{
    m_item = nullptr;
    m_shop = nullptr;

    const LevelStashes &ls = m_stash_level_it->second;

    if (m_stash_it == ls.m_stashes.end())
    {
        if (m_shop_it == ls.m_shops.end())
        {
            ++m_stash_level_it;
            if (m_stash_level_it == StashTrack.levels.end())
                return *this;

            new_level();
            return *this;
        }
        m_shop = &(*m_shop_it);

        if (m_shop_item_it != m_shop->shop.stock.end())
        {
            const item_def &item = *m_shop_item_it++;
            m_item  = &item;
            ASSERT(m_item->defined());
            m_price = item_price(item, m_shop->shop);
            return *this;
        }

        ++m_shop_it;
        if (m_shop_it != ls.m_shops.end())
            m_shop_item_it = m_shop_it->shop.stock.begin();

        ++(*this);
    }
    else
    {
        if (m_stash_item_it != m_stash_it->second.items.end())
        {
            m_item = &(*m_stash_item_it++);
            ASSERT(m_item->defined());
            return *this;
        }

        ++m_stash_it;
        if (m_stash_it == ls.m_stashes.end())
        {
            ++(*this);
            return *this;
        }

        m_stash_item_it = m_stash_it->second.items.begin();
        ++(*this);
    }

    return *this;
}

void ST_ItemIterator::new_level()
{
    m_item  = nullptr;
    m_shop  = nullptr;
    m_price = 0;

    if (m_stash_level_it == StashTrack.levels.end())
        return;

    const LevelStashes &ls = m_stash_level_it->second;

    m_place = ls.m_place;

    m_stash_it = ls.m_stashes.begin();
    if (m_stash_it != ls.m_stashes.end())
    {
        m_stash_item_it = m_stash_it->second.items.begin();
        if (m_stash_item_it != m_stash_it->second.items.end())
        {
            m_item = &(*m_stash_item_it++);
            ASSERT(m_item->defined());
        }
    }

    m_shop_it = ls.m_shops.begin();
    if (m_shop_it != ls.m_shops.end())
    {
        const ShopInfo &si = *m_shop_it;

        m_shop_item_it = si.shop.stock.begin();

        if (m_item == nullptr && m_shop_item_it != si.shop.stock.end())
        {
            const item_def &item = *m_shop_item_it++;
            m_item  = &item;
            ASSERT(m_item->defined());
            m_price = item_price(item, si.shop);
            m_shop  = &si;
        }
    }
}

ST_ItemIterator ST_ItemIterator::operator ++ (int)
{
    const ST_ItemIterator copy = *this;
    ++(*this);
    return copy;
}
