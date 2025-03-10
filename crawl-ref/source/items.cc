/**
 * @file
 * @brief Misc (mostly) inventory related functions.
**/

#include "AppHdr.h"

#include "items.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional> // mem_fn
#include <limits>

#include "adjust.h"
#include "areas.h"
#include "arena.h"
#include "artefact.h"
#include "art-enum.h"
#include "beam.h"
#include "bitary.h"
#include "branch.h"
#include "cio.h"
#include "clua.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "corpse.h"
#include "database.h" // getRandNameString
#include "dbg-util.h"
#include "defines.h"
#include "delay.h"
#include "describe.h"
#include "dgn-event.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "env.h"
#include "god-passive.h"
#include "god-prayer.h"
#include "hints.h"
#include "hints.h"
#include "hiscores.h"
#include "invent.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "item-use.h"
#include "libutil.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "nearby-danger.h"
#include "notes.h"
#include "options.h"
#include "orb.h"
#include "output.h"
#include "place.h"
#include "player-equip.h"
#include "player.h"
#include "prompt.h"
#include "quiver.h"
#include "randbook.h"
#include "religion.h"
#include "shopping.h"
#include "showsymb.h"
#include "skills.h"
#include "slot-select-mode.h"
#include "sound.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "throw.h"
#include "tilepick.h"
#include "travel.h"
#include "viewchar.h"
#include "view.h"
#include "xom.h"
#include "zot.h" // bezotted, print_gem_warnings

static int _autopickup_subtype(const item_def &item);
static void _autoinscribe_item(item_def& item);
static void _autoinscribe_floor_items();
static void _autoinscribe_inventory();
static void _multidrop(vector<SelItem> tmp_items);
static bool _merge_items_into_inv(item_def &it, int quant_got,
                                  int &inv_slot, bool quiet);

static bool will_autopickup   = false;
static bool will_autoinscribe = false;

static inline string _autopickup_item_name(const item_def &item)
{
    return userdef_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item)
           + item_prefix(item, false) + " " + item.name(DESC_PLAIN);
}

// Used to be called "unlink_items", but all it really does is make
// sure item coordinates are correct to the stack they're in. -- bwr
void fix_item_coordinates()
{
    // Nails all items to the ground (i.e. sets x,y).
    for (int x = 0; x < GXM; x++)
        for (int y = 0; y < GYM; y++)
        {
            int i = env.igrid[x][y];

            while (i != NON_ITEM)
            {
                env.item[i].pos.x = x;
                env.item[i].pos.y = y;
                i = env.item[i].link;
            }
        }
}

// This function uses the items coordinates to relink all the env.igrid lists.
void link_items()
{
    // First, initialise env.igrid array.
    env.igrid.init(NON_ITEM);

    // Link all items on the grid, plus shop inventory,
    // but DON'T link the huge pile of monster items at (-2,-2).

    for (int i = 0; i < MAX_ITEMS; i++)
    {
        // Don't mess with monster held items, since the index of the holding
        // monster is stored in the link field.
        if (env.item[i].held_by_monster())
            continue;

        if (!env.item[i].defined())
        {
            // Item is not assigned. Ignore.
            env.item[i].link = NON_ITEM;
            continue;
        }

        bool move_below = item_is_stationary(env.item[i])
            && !item_is_stationary_net(env.item[i]);
        int movable_ind = -1;
        // Stationary item, find index at location
        if (move_below)
        {

            for (stack_iterator si(env.item[i].pos); si; ++si)
            {
                if (!item_is_stationary(*si) || item_is_stationary_net(*si))
                    movable_ind = si->index();
            }
        }
        // Link to top
        if (!move_below || movable_ind == -1)
        {
            env.item[i].link = env.igrid(env.item[i].pos);
            env.igrid(env.item[i].pos) = i;
        }
        // Link below movable items.
        else
        {
            env.item[i].link = env.item[movable_ind].link;
            env.item[movable_ind].link = i;
        }
    }
}

static bool _item_ok_to_clean(int item)
{
    // Never clean misc items, Orbs, gems, or runes.
    if (env.item[item].base_type == OBJ_MISCELLANY
        || item_is_collectible(env.item[item]))
    {
        return false;
    }

    return true;
}

static bool _item_preferred_to_clean(int item)
{
    // Preferably clean "normal" weapons and ammo
    if (env.item[item].base_type == OBJ_WEAPONS
        && env.item[item].plus <= 0
        && !is_artefact(env.item[item]))
    {
        return true;
    }

    if (env.item[item].base_type == OBJ_MISSILES
        && env.item[item].plus <= 0 && !env.item[item].net_placed // XXX: plus...?
        && !is_artefact(env.item[item]))
    {
        return true;
    }

    return false;
}

// Returns index number of first available space, or NON_ITEM for
// unsuccessful cleanup (should be exceedingly rare!)
static int _cull_items()
{
    crawl_state.cancel_cmd_repeat();

    // XXX: Not the prettiest of messages, but the player
    // deserves to know whenever this kicks in. -- bwr
    mprf(MSGCH_WARN, "Too many items on level, removing some.");

    // Rules:
    //  1. Don't cleanup anything nearby the player
    //  2. Don't cleanup shops
    //  3. Don't cleanup monster inventory
    //  4. Clean 15% of items
    //  5. never remove orbs or runes
    //  7. uniques weapons are moved to the abyss
    //  8. randarts are simply lost
    //  9. unrandarts are 'destroyed', but may be generated again
    // 10. Remove +0 weapons and ammo first, only removing others if this fails.

    int first_cleaned = NON_ITEM;

    // 2. Avoid shops by avoiding (0,5..9).
    // 3. Avoid monster inventory by iterating over the dungeon grid.

    // 10. Remove +0 weapons and ammo first, only removing others if this fails.
    // Loop twice. First iteration, get rid of uninteresting stuff. Second
    // iteration, get rid of anything non-essential
    for (int remove_all=0; remove_all<2 && first_cleaned==NON_ITEM; remove_all++)
    {
        for (rectangle_iterator ri(1); ri; ++ri)
        {
            if (grid_distance(you.pos(), *ri) <= 8)
                continue;

            for (stack_iterator si(*ri); si; ++si)
            {
                if (_item_ok_to_clean(si->index())
                    && (remove_all || _item_preferred_to_clean(si->index()))
                    && x_chance_in_y(15, 100))
                {
                    if (is_unrandom_artefact(*si))
                    {
                        // 7. Move uniques to abyss.
                        set_unique_item_status(*si, UNIQ_LOST_IN_ABYSS);
                    }

                    if (first_cleaned == NON_ITEM)
                        first_cleaned = si->index();

                    // POOF!
                    destroy_item(si->index());
                }
            }
        }
    }

    return first_cleaned;
}

/*---------------------------------------------------------------------*/
stack_iterator::stack_iterator(const coord_def& pos, bool accessible)
{
    cur_link = accessible ? you.visible_igrd(pos) : env.igrid(pos);
    if (cur_link != NON_ITEM)
        next_link = env.item[cur_link].link;
    else
        next_link = NON_ITEM;
}

stack_iterator::stack_iterator(int start_link)
{
    cur_link = start_link;
    if (cur_link != NON_ITEM)
        next_link = env.item[cur_link].link;
    else
        next_link = NON_ITEM;
}

stack_iterator::operator bool() const
{
    return cur_link != NON_ITEM;
}

item_def& stack_iterator::operator*() const
{
    ASSERT(cur_link != NON_ITEM);
    return env.item[cur_link];
}

item_def* stack_iterator::operator->() const
{
    ASSERT(cur_link != NON_ITEM);
    return &env.item[cur_link];
}

int stack_iterator::index() const
{
    return cur_link;
}

const stack_iterator& stack_iterator::operator ++ ()
{
    cur_link = next_link;
    if (cur_link != NON_ITEM)
        next_link = env.item[cur_link].link;
    return *this;
}

stack_iterator stack_iterator::operator++(int)
{
    const stack_iterator copy = *this;
    ++(*this);
    return copy;
}

bool stack_iterator::operator==(const stack_iterator& rhs) const
{
    return cur_link == rhs.cur_link;
}

mon_inv_iterator::mon_inv_iterator(monster& _mon)
    : mon(_mon)
{
    type = static_cast<mon_inv_type>(0);
    if (mon.inv[type] == NON_ITEM)
        ++*this;
}

mon_inv_iterator::operator bool() const
{
    return type < NUM_MONSTER_SLOTS;
}

item_def& mon_inv_iterator::operator*() const
{
    ASSERT(mon.inv[type] != NON_ITEM);
    return env.item[mon.inv[type]];
}

item_def* mon_inv_iterator::operator->() const
{
    ASSERT(mon.inv[type] != NON_ITEM);
    return &env.item[mon.inv[type]];
}

mon_inv_iterator& mon_inv_iterator::operator ++ ()
{
    do
    {
        type = static_cast<mon_inv_type>(type + 1);
    }
    while (*this && mon.inv[type] == NON_ITEM);

    return *this;
}

mon_inv_iterator mon_inv_iterator::operator++(int)
{
    const mon_inv_iterator copy = *this;
    ++(*this);
    return copy;
}

/**
 * Reduce quantity of an inventory item, do cleanup if item goes away.
 * @return  True if stack of items no longer exists, false otherwise.
*/
bool dec_inv_item_quantity(int obj, int amount)
{
    bool ret = false;

    if (you.inv[obj].quantity <= amount)
    {
        item_skills(you.inv[obj], you.skills_to_hide);

        you.inv[obj].base_type = OBJ_UNASSIGNED;
        you.inv[obj].quantity  = 0;
        you.inv[obj].props.clear();

        ret = true;

        // If we're repeating a command, the repetitions used up the
        // item stack being repeated on, so stop rather than move onto
        // the next stack.
        crawl_state.cancel_cmd_repeat();
        crawl_state.cancel_cmd_again();
        quiver::on_actions_changed();
    }
    else
        you.inv[obj].quantity -= amount;

    return ret;
}

// Reduce quantity of a monster/grid item, do cleanup if item goes away.
//
// Returns true if stack of items no longer exists.
bool dec_mitm_item_quantity(int obj, int amount, bool player_action)
{
    item_def &item = env.item[obj];
    if (amount > item.quantity)
        amount = item.quantity; // can't use min due to type mismatch

    if (item.quantity == amount)
    {
        destroy_item(obj);
        if (player_action)
        {
            // If we're repeating a command, the repetitions used up the
            // item stack being repeated on, so stop rather than move onto
            // the next stack.
            crawl_state.cancel_cmd_repeat();
            crawl_state.cancel_cmd_again();
        }
        return true;
    }

    item.quantity -= amount;
    return false;
}

void inc_inv_item_quantity(int obj, int amount)
{
    you.inv[obj].quantity += amount;
}

void inc_mitm_item_quantity(int obj, int amount)
{
    env.item[obj].quantity += amount;
    ASSERT(env.item[obj].defined());
}

void init_item(int item)
{
    if (item == NON_ITEM)
        return;

    env.item[item].clear();
}

// Returns an unused env.item slot, or NON_ITEM if none available.
// The reserve is the number of item slots to not check.
// Items may be culled if a reserve <= 10 is specified.
int get_mitm_slot(int reserve)
{
    ASSERT(reserve >= 0);

    if (crawl_state.game_is_arena())
        reserve = 0;

    int item = NON_ITEM;

    for (item = 0; item < (MAX_ITEMS - reserve); item++)
        if (!env.item[item].defined())
            break;

    if (item >= MAX_ITEMS - reserve)
    {
        if (crawl_state.game_is_arena())
        {
            item = arena_cull_items();
            // If arena_cull_items() can't free up any space then
            // _cull_items() won't be able to either, so give up.
            if (item == NON_ITEM)
                return NON_ITEM;
        }
        else
            item = (reserve <= 10) ? _cull_items() : NON_ITEM;

        if (item == NON_ITEM)
            return NON_ITEM;
    }

    ASSERT(item != NON_ITEM);

    init_item(item);

    return item;
}

void unlink_item(int dest)
{
    // Don't destroy non-items, may be called after an item has been
    // reduced to zero quantity however.
    if (dest == NON_ITEM || !env.item[dest].defined())
        return;

    monster* mons = env.item[dest].holding_monster();

    if (mons != nullptr)
    {
        for (mon_inv_iterator ii(*mons); ii; ++ii)
        {
            if (ii->index() == dest)
            {
                item_def& item = *ii;
                mons->inv[ii.slot()] = NON_ITEM;

                item.pos.reset();
                item.link = NON_ITEM;
                return;
            }
        }
        mprf(MSGCH_ERROR, "Item %s claims to be held by monster %s, but "
                          "it isn't in the monster's inventory.",
             env.item[dest].name(DESC_PLAIN, false, true).c_str(),
             mons->name(DESC_PLAIN, true).c_str());
        // Don't return so the debugging code can take a look at it.
    }
    // Unlinking a newly created item, or a a temporary one, or an item in
    // the player's inventory.
    else if (env.item[dest].pos.origin() || env.item[dest].pos == ITEM_IN_INVENTORY)
    {
        env.item[dest].pos.reset();
        env.item[dest].link = NON_ITEM;
        return;
    }
    else
    {
        // Linked item on map:
        //
        // Use the items (x,y) to access the list (env.igrid[x][y]) where
        // the item should be linked.

#if TAG_MAJOR_VERSION == 34
        if (env.item[dest].pos.x != 0 || env.item[dest].pos.y < 5)
#endif
        ASSERT_IN_BOUNDS(env.item[dest].pos);

        // First check the top:
        if (env.igrid(env.item[dest].pos) == dest)
        {
            // link env.igrid to the second item
            env.igrid(env.item[dest].pos) = env.item[dest].link;

            env.item[dest].pos.reset();
            env.item[dest].link = NON_ITEM;
            return;
        }

        // Okay, item is buried, find item that's on top of it.
        for (stack_iterator si(env.item[dest].pos); si; ++si)
        {
            // Find item linking to dest item.
            if (si->defined() && si->link == dest)
            {
                // unlink dest
                si->link = env.item[dest].link;
                env.item[dest].pos.reset();
                env.item[dest].link = NON_ITEM;
                return;
            }
        }
    }

#ifdef DEBUG
    // Okay, the sane ways are gone... let's warn the player:
    mprf(MSGCH_ERROR, "BUG WARNING: Problems unlinking item '%s', (%d, %d)!!!",
         env.item[dest].name(DESC_PLAIN).c_str(),
         env.item[dest].pos.x, env.item[dest].pos.y);

    // Okay, first we scan all items to see if we have something
    // linked to this item. We're not going to return if we find
    // such a case... instead, since things are already out of
    // alignment, let's assume there might be multiple links as well.
    bool linked = false;
    int  old_link = env.item[dest].link; // used to try linking the first

    // Clean the relevant parts of the object.
    env.item[dest].base_type = OBJ_UNASSIGNED;
    env.item[dest].quantity  = 0;
    env.item[dest].link      = NON_ITEM;
    env.item[dest].pos.reset();
    env.item[dest].props.clear();

    // Look through all items for links to this item.
    for (auto &item : env.item)
    {
        if (item.defined() && item.link == dest)
        {
            // unlink item
            item.link = old_link;

            if (!linked)
            {
                old_link = NON_ITEM;
                linked = true;
            }
        }
    }

    // Now check the grids to see if it's linked as a list top.
    for (int c = 2; c < (GXM - 1); c++)
        for (int cy = 2; cy < (GYM - 1); cy++)
        {
            if (env.igrid[c][cy] == dest)
            {
                env.igrid[c][cy] = old_link;

                if (!linked)
                {
                    old_link = NON_ITEM;  // cleaned after the first
                    linked = true;
                }
            }
        }

    // Okay, finally warn player if we didn't do anything.
    if (!linked)
        mprf(MSGCH_ERROR, "BUG WARNING: Item didn't seem to be linked at all.");
#endif
}

void destroy_item(item_def &item, bool never_created)
{
    if (!item.defined())
        return;

    if (never_created)
    {
        if (is_unrandom_artefact(item))
            set_unique_item_status(item, UNIQ_NOT_EXISTS);
    }

    item.clear();
}

void destroy_item(int dest, bool never_created)
{
    // Don't destroy non-items, but this function may be called upon
    // to remove items reduced to zero quantity, so we allow "invalid"
    // objects in.
    if (dest == NON_ITEM || !env.item[dest].defined())
        return;

    unlink_item(dest);
    destroy_item(env.item[dest], never_created);
}

static void _handle_gone_item(const item_def &item)
{
    if (player_in_branch(BRANCH_ABYSS)
        && item.orig_place.branch == BRANCH_ABYSS)
    {
        if (is_unrandom_artefact(item))
            set_unique_item_status(item, UNIQ_LOST_IN_ABYSS);
    }
}

void item_was_lost(const item_def &item)
{
    _handle_gone_item(item);
    xom_check_lost_item(item);
}

void item_was_destroyed(const item_def &item)
{
    _handle_gone_item(item);
    xom_check_destroyed_item(item);
}

void lose_item_stack(const coord_def& where)
{
    for (stack_iterator si(where); si; ++si)
    {
        if (si ->defined()) // FIXME is this check necessary?
        {
            item_was_lost(*si);
            si->clear();
        }
    }
    env.igrid(where) = NON_ITEM;
}

/**
 * How many movable items are there at a location?
 *
 * @param obj The item link for the location.
 * @return  The number of movable items at the location.
*/
int count_movable_items(int obj)
{
    int result = 0;

    for (stack_iterator si(obj); si; ++si)
    {
        if (!item_is_stationary(*si))
            ++result;
    }
    return result;
}

/**
 * Fill the given vector with the items on the given location link.
 *
 * @param[out] items A vector to hold the item_defs of the item.
 * @param[in] obj The location link; an index in env.item.
 * @param exclude_stationary If true, don't include stationary items.
*/
vector<item_def*> item_list_on_square(int obj)
{
    vector<item_def*> items;
    for (stack_iterator si(obj); si; ++si)
        items.push_back(& (*si));
    return items;
}

// no overloading by return type, so some ugly code duplication. (There may be
// cleverer template things to do.)
vector<const item_def*> const_item_list_on_square(int obj)
{
    vector<const item_def*> items;
    for (stack_iterator si(obj); si; ++si)
        items.push_back(& (*si));
    return items;
}


bool need_to_autopickup()
{
    return will_autopickup;
}

void request_autopickup(bool do_pickup)
{
    will_autopickup = do_pickup;
}

bool item_is_branded(const item_def& item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        return get_weapon_brand(item) != SPWPN_NORMAL;
    case OBJ_ARMOUR:
        return get_armour_ego_type(item) != SPARM_NORMAL;
    case OBJ_MISSILES:
        return get_ammo_brand(item) != SPMSL_NORMAL;
    default:
        return false;
    }
}

bool item_is_unusual(const item_def& item)
{
    const auto &patterns = Options.unusual_monster_items;
    const string name = item.name(DESC_A, false, false, true, false);

    return any_of(begin(patterns), end(patterns),
                  [&](const text_pattern &p) -> bool
                  { return p.matches(name); });
}

bool item_is_worth_listing(const item_def& item)
{
    if (item_is_unusual(item))
        return true;

    switch (item.base_type)
    {
    case OBJ_STAVES:
    case OBJ_WANDS:
        return true;
    case OBJ_WEAPONS:
        return is_unrandom_artefact(item)
               || get_weapon_brand(item) != SPWPN_NORMAL;
    default:
        return item_is_branded(item) || is_artefact(item);
    }
}

// 3 - unrandart, 2 - artefact, 1 - glowing/runed, 0 - mundane
static int _item_name_specialness(const item_def& item)
{
    if (is_unrandom_artefact(item) || is_xp_evoker(item))
        return 3;

    // You can tell something is an artefact, because it'll have a
    // description which rules out anything else.
    if (is_artefact(item))
        return 2;

    // All unknown jewellery is worth looking at.
    if (item.base_type == OBJ_JEWELLERY)
    {
        if (is_useless_item(item))
            return 0;

        return 1;
    }

    if (item.is_identified())
    {
        if (item_is_branded(item))
            return 1;
        return 0;
    }

    if (item.flags & ISFLAG_COSMETIC_MASK)
        return 1;

    return 0;
}

string item_message(vector<const item_def *> const &items)
{
    if (static_cast<int>(items.size()) >= Options.item_stack_summary_minimum)
    {
        string out_string;
        vector<unsigned int> item_chars;
        for (unsigned int i = 0; i < items.size() && i < 50; ++i)
        {
            cglyph_t g = get_item_glyph(*items[i]);
            item_chars.push_back(g.ch * 0x100 +
                                 (10 - _item_name_specialness(*(items[i]))));
        }
        sort(item_chars.begin(), item_chars.end());

        int cur_state = -1;
        string colour = "";
        for (unsigned int i = 0; i < item_chars.size(); ++i)
        {
            const int specialness = 10 - (item_chars[i] % 0x100);
            if (specialness != cur_state)
            {
                if (!colour.empty())
                    out_string += "</" + colour + ">";
                switch (specialness)
                {
                case 3: colour = "lightcyan"; break; // unrandart
                case 2: colour = "yellow";   break; // artefact
                case 1: colour = "white";    break; // glowing/runed
                case 0: colour = "darkgrey"; break; // mundane
                }
                if (!colour.empty())
                    out_string += "<" + colour + ">";
                cur_state = specialness;
            }

            out_string += stringize_glyph(item_chars[i] / 0x100);
            if (i + 1 < item_chars.size()
                && (item_chars[i] / 0x100) != (item_chars[i+1] / 0x100))
            {
                out_string += ' ';
            }
        }
        if (!colour.empty())
            out_string += "</" + colour + ">";

        return out_string;
    }

    vector<string> colour_names;
    for (const item_def *it : items)
        colour_names.push_back(menu_colour_item_name(*it, DESC_A));

    return join_strings(colour_names.begin(), colour_names.end(), "; ");
}

void item_check()
{
    describe_floor();
    origin_set(you.pos());

    ostream& strm = msg::streams(MSGCH_FLOOR_ITEMS);

    auto items = const_item_list_on_square(you.visible_igrd(you.pos()));

    if (items.empty())
        return;

    // Special case
    if (items.size() == 1)
    {
        const item_def& it(*items[0]);
        string name = menu_colour_item_name(it, DESC_A);
        strm << "You see here " << name << '.' << endl;
        return;
    }

    string desc_string = item_message(items);
    // Stack summary case
    if (static_cast<int>(items.size()) >= Options.item_stack_summary_minimum)
        mprf_nojoin(MSGCH_FLOOR_ITEMS, "Items here: %s.", desc_string.c_str());
    else if (items.size() <= msgwin_lines() - 1)
    {
        mpr_nojoin(MSGCH_FLOOR_ITEMS, "Things that are here:");
        mprf_nocap("%s", desc_string.c_str());
    }
    else
        strm << "There are many items here." << endl;

    if (items.size() > 2 && crawl_state.game_is_hints_tutorial())
    {
        // If there are 2 or more non-corpse items here, we might need
        // a hint.
        int count = 0;
        for (const item_def *it : items)
        {
            if (it->base_type == OBJ_CORPSES)
                continue;

            if (++count > 1)
            {
                learned_something_new(HINT_MULTI_PICKUP);
                break;
            }
        }
    }
}

bool item_def::is_identified() const
{
    if (flags & ISFLAG_IDENTIFIED)
        return true;

    if (item_type_known(*this))
        return true;

    return false;
}

void identify_item(item_def& item)
{
    if (item.is_identified())
        return;

    item.flags |= ISFLAG_IDENTIFIED;

    request_autoinscribe();

    if (!is_artefact(item) && !crawl_state.game_is_arena())
        identify_item_type(item.base_type, item.sub_type);

    if (in_inventory(item))
    {
        shopping_list.cull_identical_items(item);
        item_skills(item, you.skills_to_show);
    }

    if (notes_are_active()
        && is_interesting_item(item)
        && !(item.flags & (ISFLAG_NOTED_ID)))
    {
        // Make a note of it.
        take_note(Note(NOTE_ID_ITEM, 0, 0, item.name(DESC_A),
                       origin_desc(item)));

        // Sometimes (e.g. shops) you can ID an item before you get it;
        // don't note twice in those cases.
        item.flags |= (ISFLAG_NOTED_ID | ISFLAG_NOTED_GET);
    }
}

// Identify a wand or equipment item the player stepped on.
// Returns true if the player learned something.
static bool _id_floor_item(item_def &item)
{
    if (item.base_type == OBJ_WANDS)
    {
        if (!type_is_identified(item))
        {
            // If the player doesn't want unknown wands picked up, assume
            // they won't want this wand after it is identified.
            bool should_pickup = item_needs_autopickup(item);
            identify_item(item);
            if (!should_pickup)
                set_item_autopickup(item, AP_FORCE_OFF);
            return true;
        }
    }
    else if (item_type_is_equipment(item.base_type)
             || item.base_type == OBJ_TALISMANS)
    {
        if (item.is_identified())
            return false;

        // autopickup hack for previously-unknown items
        if (item_needs_autopickup(item))
            item.props[NEEDS_AUTOPICKUP_KEY] = true;
        identify_item(item);
        // but skip ones that we discover to be useless
        if (item.props.exists(NEEDS_AUTOPICKUP_KEY) && is_useless_item(item))
            item.props.erase(NEEDS_AUTOPICKUP_KEY);
        return true;
    }

    return false;
}

/// Auto-ID whatever stuff the player stands on.
void id_floor_items()
{
    for (stack_iterator si(you.pos()); si; ++si)
        _id_floor_item(*si);
}

void pickup_menu(int item_link)
{
    int n_did_pickup   = 0;

    // XX why is this const?
    auto items = const_item_list_on_square(item_link);
    ASSERT(items.size());

    string prompt = "Pick up what? " + slot_description() + " (_ for help)";

    if (items.size() == 1 && items[0]->quantity > 1)
        prompt = "Select pick up quantity by entering a number, then select the item";
    vector<SelItem> selected = select_items(items, prompt.c_str(), false,
                                            menu_type::pickup);
    if (selected.empty())
        canned_msg(MSG_OK);
    redraw_screen();
    update_screen();

    string pickup_warning;
    for (const SelItem &sel : selected)
    {
        // Moving the item might destroy it, in which case we can't
        // rely on the link.
        short next;
        for (int j = item_link; j != NON_ITEM; j = next)
        {
            next = env.item[j].link;
            if (&env.item[j] == sel.item)
            {
                if (j == item_link)
                    item_link = next;

                int num_to_take = sel.quantity;
                const bool take_all = (num_to_take == env.item[j].quantity);
                iflags_t oldflags = env.item[j].flags;
                clear_item_pickup_flags(env.item[j]);

                // If we cleared any flags on the items, but the pickup was
                // partial, reset the flags for the items that remain on the
                // floor.
                if (!move_item_to_inv(j, num_to_take))
                {
                    pickup_warning = "You can't carry that many items.";
                    if (env.item[j].defined())
                        env.item[j].flags = oldflags;
                }
                else
                {
                    // If we deliberately chose to take only part of a
                    // pile, we consider the rest to have been
                    // "dropped."
                    if (!take_all && env.item[j].defined())
                        env.item[j].flags |= ISFLAG_DROPPED;
                }
            }
        }
    }

    if (!pickup_warning.empty())
    {
        mpr(pickup_warning);
        learned_something_new(HINT_FULL_INVENTORY);
    }

    if (n_did_pickup)
        you.turn_is_over = true;
}

bool origin_known(const item_def &item)
{
    return item.orig_place != level_id();
}

void origin_reset(item_def &item)
{
    item.orig_place.clear();
    item.orig_monnum = 0;
}

// We have no idea where the player found this item.
void origin_set_unknown(item_def &item)
{
    if (!origin_known(item))
    {
        item.orig_place  = level_id(BRANCH_DUNGEON, 0);
        item.orig_monnum = 0;
    }
}

// This item is starting equipment.
void origin_set_startequip(item_def &item)
{
    if (!origin_known(item))
    {
        item.orig_place  = level_id(BRANCH_DUNGEON, 0);
        item.orig_monnum = -IT_SRC_START;
    }
}

void origin_set_monster(item_def &item, const monster* mons)
{
    if (!origin_known(item))
    {
        if (!item.orig_monnum)
            item.orig_monnum = mons->type;
        item.orig_place = level_id::current();
    }
}

void origin_purchased(item_def &item)
{
    // We don't need to check origin_known if it's a shop purchase
    item.orig_place  = level_id::current();
    item.orig_monnum = -IT_SRC_SHOP;
}

void origin_acquired(item_def &item, int agent)
{
    // We don't need to check origin_known if it's a divine gift
    item.orig_place  = level_id::current();
    item.orig_monnum = -agent;
}

static string _milestone_collectible(const item_def &item)
{
    return string("found ") + item.name(DESC_A) + ".";
}

void milestone_check(const item_def &item)
{
    if (item.base_type == OBJ_RUNES)
        mark_milestone("rune", _milestone_collectible(item));
    else if (item.base_type == OBJ_GEMS)
        mark_milestone("gem.found", _milestone_collectible(item));
    else if (item_is_orb(item))
        mark_milestone("orb", "found the Orb of Zot!");
}

static void _check_note_item(item_def &item)
{
    if (item.flags & (ISFLAG_NOTED_GET | ISFLAG_NOTED_ID))
        return;

    if (item_is_collectible(item) || is_artefact(item))
    {
        int v = item.base_type == OBJ_GEMS ? gem_time_left(item.sub_type) : 0;
        take_note(Note(NOTE_GET_ITEM, v, 0, item.name(DESC_A),
                       origin_desc(item)));
        item.flags |= ISFLAG_NOTED_GET;

        // If it's already fully identified when picked up, don't take
        // further notes.
        if (item.is_identified())
            item.flags |= ISFLAG_NOTED_ID;
        milestone_check(item);
    }
}

void origin_set(const coord_def& where)
{
    for (stack_iterator si(where); si; ++si)
    {
        if (origin_known(*si))
            continue;

        si->orig_place = level_id::current();
    }
}

static void _origin_freeze(item_def &item)
{
    if (!origin_known(item))
    {
        item.orig_place = level_id::current();
        _check_note_item(item);
    }
}

static string _origin_monster_name(const item_def &item)
{
    const monster_type monnum = static_cast<monster_type>(item.orig_monnum);
    if (monnum == MONS_PLAYER_GHOST)
        return "a player ghost";
    else if (monnum == MONS_PANDEMONIUM_LORD)
        return "a pandemonium lord";
    return mons_type_name(monnum, DESC_A);
}

static string _origin_place_desc(const item_def &item)
{
    return prep_branch_level_name(item.orig_place);
}

static bool _origin_is_special(const item_def &item)
{
    return item.orig_place == level_id(BRANCH_DUNGEON, 0);
}

bool origin_describable(const item_def &item)
{
    return origin_known(item)
           && !_origin_is_special(item)
           && !is_stackable_item(item)
           && item.quantity == 1
           && item.base_type != OBJ_CORPSES;
}

static string _article_it(const item_def &/*item*/)
{
    // "it" is always correct, since gloves and boots also come in pairs.
    return "it";
}

static bool _origin_is_original_equip(const item_def &item)
{
    return _origin_is_special(item) && item.orig_monnum == -IT_SRC_START;
}

/**
 * What god gifted this item to the player?
 *
 * @param item      The item in question.
 * @returns         The god that gifted this item to the player, if any;
 *                  else GOD_NO_GOD.
 */
god_type origin_as_god_gift(const item_def& item)
{
    const god_type ogod = static_cast<god_type>(-item.orig_monnum);
    if (ogod <= GOD_NO_GOD || ogod >= NUM_GODS)
        return GOD_NO_GOD;
    return ogod;
}

bool origin_is_acquirement(const item_def& item, item_source_type *type)
{
    item_source_type junk;
    if (type == nullptr)
        type = &junk;
    *type = IT_SRC_NONE;

    const int iorig = -item.orig_monnum;
#if TAG_MAJOR_VERSION == 34
// Copy pasting is bad but this will autoupdate on version bump
    if (iorig == AQ_CARD_GENIE)
    {
        *type = static_cast<item_source_type>(iorig);
        return true;
    }

#endif
    if (iorig == AQ_SCROLL || iorig == AQ_WIZMODE)
    {
        *type = static_cast<item_source_type>(iorig);
        return true;
    }

    return false;
}

string origin_desc(const item_def &item)
{
    if (!origin_describable(item))
        return "";

    if (_origin_is_original_equip(item))
        return "Original Equipment";

    string desc;
    if (item.orig_monnum)
    {
        if (item.orig_monnum < 0)
        {
            int iorig = -item.orig_monnum;
            switch (iorig)
            {
            case IT_SRC_SHOP:
                desc += "You bought " + _article_it(item) + " in a shop ";
                break;
            case IT_SRC_START:
                desc += "Buggy Original Equipment: ";
                break;
            case AQ_SCROLL:
                desc += "You acquired " + _article_it(item) + " ";
                break;
            case AQ_INVENTED:
                desc += "You invented it yourself ";
                break;
#if TAG_MAJOR_VERSION == 34
            case AQ_CARD_GENIE:
                desc += "You drew the Genie ";
                break;
#endif
            case AQ_WIZMODE:
                desc += "Your wizardly powers created "+ _article_it(item)+ " ";
                break;
            default:
                if (iorig > GOD_NO_GOD && iorig < NUM_GODS)
                {
                    desc += god_name(static_cast<god_type>(iorig))
                        + " gifted " + _article_it(item) + " to you ";
                }
                else
                {
                    // Bug really.
                    desc += "You stumbled upon " + _article_it(item) + " ";
                }
                break;
            }
        }
        else if (item.orig_monnum == MONS_DANCING_WEAPON)
            desc += "You subdued it ";
        else
        {
            desc += "You took " + _article_it(item) + " off "
                    + _origin_monster_name(item) + " ";
        }
    }
    else
        desc += "You found " + _article_it(item) + " ";

    desc += _origin_place_desc(item);
    return desc;
}

/**
 * Pickup a single item stack at the given location link
 *
 * @param link The location link
 * @param qty If 0, prompt for quantity of that item to pick up, if < 0,
 *            pick up the entire stack, otherwise pick up qty of the item.
 * @return  True if any item was picked up, false otherwise.
*/
bool pickup_single_item(int link, int qty)
{
    ASSERT(link != NON_ITEM);

    item_def* item = &env.item[link];
    if (item_is_stationary(env.item[link]))
    {
        mpr("You can't pick that up.");
        return false;
    }
    if (item->base_type == OBJ_GOLD && !qty && !i_feel_safe()
        && !yesno("Are you sure you want to pick up this pile of gold now?",
                  true, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }
    if (qty == 0 && item->quantity > 1 && item->base_type != OBJ_GOLD)
    {
        const string prompt
                = make_stringf("Pick up how many of %s (; or enter for all)? ",
                               item->name(DESC_THE, false,
                                          false, false).c_str());

        qty = prompt_for_quantity(prompt.c_str());
        if (qty == -1)
            qty = item->quantity;
        else if (qty == 0)
        {
            canned_msg(MSG_OK);
            return false;
        }
        else if (qty < item->quantity)
        {
            // Mark rest item as not eligible for autopickup.
            item->flags |= ISFLAG_DROPPED;
            item->flags &= ~ISFLAG_THROWN;
        }
    }

    if (qty < 1 || qty > item->quantity)
        qty = item->quantity;

    iflags_t oldflags = item->flags;
    clear_item_pickup_flags(*item);
    const bool pickup_succ = move_item_to_inv(link, qty);
    if (item->defined())
        item->flags = oldflags;

    if (!pickup_succ)
    {
        mpr("You can't carry that many items.");
        learned_something_new(HINT_FULL_INVENTORY);
        return false;
    }
    return true;
}

bool player_on_single_stack()
{
    int o = you.visible_igrd(you.pos());
    if (o == NON_ITEM)
        return false;
    else
        return env.item[o].link == NON_ITEM && env.item[o].quantity > 1;
}

/**
 * Do the pickup command.
 *
 * @param partial_quantity If true, prompt for a quantity to pick up when
 *                         picking up a single stack.
*/
void pickup(bool partial_quantity)
{
    int keyin = 'x';

    int o = you.visible_igrd(you.pos());
    const int num_items = count_movable_items(o);

    // Store last_pickup in case we need to restore it.
    // Then clear it to fill with items picked up.
    map<int,int> tmp_l_p = you.last_pickup;
    you.last_pickup.clear();

    if (o == NON_ITEM)
        mpr("There are no items here.");
    else if (num_items == 1) // just one movable item?
    {
        // Get the link to the movable item in the pile.
        while (item_is_stationary(env.item[o]))
            o = env.item[o].link;
        pickup_single_item(o, partial_quantity ? 0 : env.item[o].quantity);
    }
    else if (Options.pickup_menu_limit
             && num_items > (Options.pickup_menu_limit > 0
                             ? Options.pickup_menu_limit
                             : Options.item_stack_summary_minimum - 1))
    {
        pickup_menu(o);
    }
    else
    {
        int next;
        if (num_items == 0)
            mpr("There are no objects that can be picked up here.");
        else
            mpr("There are several objects here.");
        string pickup_warning;
        bool any_selectable = false;
        while (o != NON_ITEM)
        {
            // Must save this because pickup can destroy the item.
            next = env.item[o].link;

            if (item_is_stationary(env.item[o]))
            {
                o = next;
                continue;
            }
            any_selectable = true;

            if (keyin != 'a')
            {
                string prompt = "Pick up %s? ((y)es/(n)o/(a)ll/(m)enu/*?g,/q)";

                mprf(MSGCH_PROMPT, prompt.c_str(),
                     menu_colour_item_name(env.item[o], DESC_A).c_str());

                mouse_control mc(MOUSE_MODE_YESNO);
                keyin = getch_ck();
            }

            if (keyin == '*' || keyin == '?' || keyin == ',' || keyin == 'g'
                || keyin == 'm' || keyin == CK_MOUSE_CLICK)
            {
                pickup_menu(o);
                break;
            }

            if (keyin == 'q' || key_is_escape(keyin))
            {
                canned_msg(MSG_OK);
                break;
            }

            if (keyin == 'y' || keyin == 'a')
            {
                int num_to_take = env.item[o].quantity;
                const iflags_t old_flags(env.item[o].flags);
                clear_item_pickup_flags(env.item[o]);

                // attempt to actually pick up the object.
                if (!move_item_to_inv(o, num_to_take))
                {
                    pickup_warning = "You can't carry that many items.";
                    env.item[o].flags = old_flags;
                }
            }

            o = next;

            if (o == NON_ITEM && keyin != 'y' && keyin != 'a')
                canned_msg(MSG_OK);
        }

        // If there were no selectable items (all corpses, for example),
        // list them.
        if (!any_selectable)
        {
            for (stack_iterator si(you.pos(), true); si; ++si)
                mprf_nocap("%s", menu_colour_item_name(*si, DESC_A).c_str());
        }

        if (!pickup_warning.empty())
            mpr(pickup_warning);
    }
    if (you.last_pickup.empty())
        you.last_pickup = tmp_l_p;
}

bool is_stackable_item(const item_def &item)
{
    if (!item.defined())
        return false;

    switch (item.base_type)
    {
        case OBJ_MISSILES:
        case OBJ_SCROLLS:
        case OBJ_POTIONS:
        case OBJ_GOLD:
#if TAG_MAJOR_VERSION == 34
        case OBJ_FOOD:
#endif
            return true;
        case OBJ_MISCELLANY:
            return item.sub_type == MISC_ZIGGURAT
                    || item.sub_type == MISC_SHOP_VOUCHER;
        default:
            break;
    }
    return false;
}

bool items_similar(const item_def &item1, const item_def &item2)
{
    // Base and sub-types must always be the same to stack.
    if (item1.base_type != item2.base_type || item1.sub_type != item2.sub_type)
        return false;

    if (item1.base_type == OBJ_GOLD || item_is_collectible(item1))
        return true;

    if (is_artefact(item1) != is_artefact(item2))
        return false;
    else if (is_artefact(item1)
             && get_artefact_name(item1) != get_artefact_name(item2))
    {
        return false;
    }

    // Missiles with different egos shouldn't merge.
    if (item1.base_type == OBJ_MISSILES && item1.brand != item2.brand)
        return false;

    // Don't merge trapping nets with other nets.
    if (item1.is_type(OBJ_MISSILES, MI_THROWING_NET)
        && item1.net_placed != item2.net_placed)
    {
        return false;
    }

#define NO_MERGE_FLAGS (ISFLAG_MIMIC | ISFLAG_SUMMONED)
    if ((item1.flags & NO_MERGE_FLAGS) != (item2.flags & NO_MERGE_FLAGS))
        return false;

    return true;
}

bool items_stack(const item_def &item1, const item_def &item2)
{
    // Both items must be stackable.
    if (!is_stackable_item(item1) || !is_stackable_item(item2)
        || static_cast<long long>(item1.quantity) + item2.quantity
            > numeric_limits<decltype(item1.quantity)>::max())
    {
        return false;
    }

    return items_similar(item1, item2)
        // Don't leak information when checking if an "(unknown)" shop item
        // matches an unidentified item in inventory.
        && item1.is_identified() == item2.is_identified();
}

static int _userdef_find_free_slot(const item_def &i)
{
    int slot = -1;
    if (!clua.callfn("c_assign_invletter", "i>d", &i, &slot))
        return -1;

    return slot;
}

int find_free_slot(const item_def &i)
{
#define slotisfree(s) \
            ((s) >= 0 && (s) < ENDOFPACK && !you.inv[s].defined())

    bool searchforward = false;
    // If we're doing Lua, see if there's a Lua function that can give
    // us a free slot.
    int slot = _userdef_find_free_slot(i);
    if (slot == -2 || Options.assign_item_slot == SS_FORWARD)
        searchforward = true;

    if (slotisfree(slot))
        return slot;

    // See if the item remembers where it's been. Lua code can play with
    // this field so be extra careful.
    if (isaalpha(i.slot))
        slot = letter_to_index(i.slot);

    if (slotisfree(slot))
        return slot;

    FixedBitVector<ENDOFPACK> disliked;
    if (i.base_type == OBJ_POTIONS)
        disliked.set('y' - 'a');

    if (!searchforward)
    {
        // This is the new default free slot search. We look for the last
        // available slot that does not leave a gap in the inventory.
        for (slot = ENDOFPACK - 1; slot >= 0; --slot)
        {
            if (you.inv[slot].defined())
            {
                if (slot + 1 < ENDOFPACK && !you.inv[slot + 1].defined()
                    && !disliked[slot + 1])
                {
                    return slot + 1;
                }
            }
            else
            {
                if (slot + 1 < ENDOFPACK && you.inv[slot + 1].defined()
                    && !disliked[slot])
                {
                    return slot;
                }
            }
        }
    }

    // Either searchforward is true, or search backwards failed and
    // we re-try searching the opposite direction.

    int badslot = -1;
    // Return first free slot
    for (slot = 0; slot < ENDOFPACK; ++slot)
        if (!you.inv[slot].defined())
        {
            if (disliked[slot])
                badslot = slot;
            else
                return slot;
        }

    // If the least preferred slot is the only choice, so be it.
    return badslot;
#undef slotisfree
}

static void _got_item(item_def& item)
{
    seen_item(item);
    shopping_list.cull_identical_items(item);
    item.flags |= ISFLAG_HANDLED;

    if (item.props.exists(NEEDS_AUTOPICKUP_KEY))
        item.props.erase(NEEDS_AUTOPICKUP_KEY);
}

void get_gold(const item_def& item, int quant, bool quiet)
{
    you.attribute[ATTR_GOLD_FOUND] += quant;

    if (you_worship(GOD_ZIN))
        quant -= zin_tithe(item, quant, quiet);
    if (quant <= 0)
        return;
    you.add_gold(quant);

    if (!quiet)
    {
        const string gain = quant != you.gold
                            ? make_stringf(" (gained %d)", quant)
                            : "";

        mprf("You now have %d gold piece%s%s.",
             you.gold, you.gold != 1 ? "s" : "", gain.c_str());
        learned_something_new(HINT_SEEN_GOLD);
    }
}

void note_inscribe_item(item_def &item)
{
    _autoinscribe_item(item);
    _origin_freeze(item);
    _check_note_item(item);
}

static bool _put_item_in_inv(item_def& it, int quant_got, bool quiet, bool& put_in_inv)
{
    put_in_inv = false;
    if (item_is_stationary(it))
    {
        if (!quiet)
            mpr("You can't pick that up.");
        // Fake a successful pickup (return 1), so we can continue to
        // pick up anything else that might be on this square.
        return true;
    }

    // sanity
    if (quant_got > it.quantity || quant_got <= 0)
        quant_got = it.quantity;

    // attempt to put the item into your inventory.
    int inv_slot;
    if (_merge_items_into_inv(it, quant_got, inv_slot, quiet))
    {
        put_in_inv = true;

        // cleanup items that ended up in an inventory slot (not gold, etc)
        if (inv_slot != -1)
            _got_item(you.inv[inv_slot]);
        else if (it.base_type == OBJ_BOOKS)
            _got_item(it);
        _check_note_item(inv_slot == -1 ? it : you.inv[inv_slot]);
        return true;
    }

    return false;
}


// Currently only used for moving shop items into inventory, since they are
// not in env.item. This doesn't work with partial pickup, because that requires
// an env.item slot...
bool move_item_to_inv(item_def& item, bool quiet)
{
    bool junk;
    return _put_item_in_inv(item, item.quantity, quiet, junk);
}

/**
 * Move the given item and quantity to the player's inventory.
 *
 * @param obj The item index in env.item.
 * @param quant_got The quantity of this item to move.
 * @param quiet If true, most messages notifying the player of item pickup (or
 *              item pickup failure) aren't printed.
 * @returns false if items failed to be picked up because of a full inventory,
 *          true otherwise (even if nothing was picked up).
*/
bool move_item_to_inv(int obj, int quant_got, bool quiet)
{
    item_def &it = env.item[obj];
    const coord_def old_item_pos = it.pos;

    bool actually_went_in = false;
    const bool keep_going = _put_item_in_inv(it, quant_got, quiet, actually_went_in);

    if ((item_is_collectible(it) || in_bounds(old_item_pos))
        && actually_went_in)
    {
        dungeon_events.fire_position_event(dgn_event(DET_ITEM_PICKUP,
                                                     you.pos(), 0, obj,
                                                     -1),
                                           you.pos());

        // XXX: Waiting until now to decrement the quantity may give Windows
        // tiles players the opportunity to close the window and duplicate the
        // item (a variant of bug #6486). However, we can't decrement the
        // quantity before firing the position event, because the latter needs
        // the object's index.
        dec_mitm_item_quantity(obj, quant_got);

        you.turn_is_over = true;
    }

    return keep_going;
}

static void _get_book(item_def& it)
{
    if (it.sub_type != BOOK_MANUAL)
    {
        if (you.has_mutation(MUT_INNATE_CASTER))
        {
            mprf("%s burns to shimmering ash in your grasp.",
                 it.name(DESC_THE).c_str());
            return;
        }
        mprf("You pick up %s and begin reading...", it.name(DESC_A).c_str());

        if (!library_add_spells(spells_in_book(it)))
            mpr("Unfortunately, you learned nothing new.");

        taken_new_item(it.base_type);

        return;
    }
    // This is mainly for save compat: if a manual generated somehow that is not
    // id'd, the following message is completely useless
    identify_item(it);
    const skill_type sk = static_cast<skill_type>(it.plus);

    if (is_useless_skill(sk))
    {
        mprf("You pick up %s. Unfortunately, it's quite useless to you.",
             it.name(DESC_A).c_str());
        return;
    }

    if (you.skills[sk] >= MAX_SKILL_LEVEL)
    {
        mprf("You pick up %s, but it has nothing more to teach you.",
             it.name(DESC_A).c_str());
        return;
    }

    if (you.skill_manual_points[sk])
        mprf("You pick up another %s and continue studying.", it.name(DESC_PLAIN).c_str());
    else
        mprf("You pick up %s and begin studying.", it.name(DESC_A).c_str());
    you.skill_manual_points[sk] += it.skill_points;
    you.skills_to_show.insert(sk);
}

static void _get_voucher(item_def& it)
{
    if (it.sub_type != MISC_SHOP_VOUCHER)
        return;

    you.attribute[ATTR_VOUCHER]++;

    taken_new_item(it.base_type);
}

// Adds all books in the player's inventory to library.
// Declared here for use by tags to load old saves.
void add_held_books_to_library()
{
    for (item_def& it : you.inv)
    {
        if (it.base_type == OBJ_BOOKS)
        {
            _get_book(it);
            destroy_item(it);
        }
    }
}

static bool _got_all_pan_runes()
{
    for (int rune = RUNE_DEMONIC; rune <= RUNE_GLOORX_VLOQ; ++rune)
        if (!you.runes[rune])
            return false;
    return true;
}

/**
 * Place a rune into the player's inventory.
 *
 * @param it      The item (rune) to pick up.
 * @param quiet   Whether to suppress (most?) messages.
 */
static void _get_rune(const item_def& it, bool quiet)
{
    if (!you.runes[it.sub_type])
        you.runes.set(it.sub_type);

    if (!quiet)
    {
        flash_view_delay(UA_PICKUP, rune_colour(it.sub_type), 300);
        mprf("You pick up the %s rune and feel its power.",
             rune_type_name(it.sub_type));
        int nrunes = runes_in_pack();
        if (nrunes >= you.obtainable_runes)
            mpr("You have collected all the runes! Now go and win!");
        else if (nrunes == ZOT_ENTRY_RUNES)
        {
            // might be inappropriate in new Sprints, please change it then
            mprf("%d runes! That's enough to enter the realm of Zot.",
                 nrunes);
        }
        else if (nrunes > 1)
        {
            if (player_in_branch(BRANCH_PANDEMONIUM) && _got_all_pan_runes())
                mpr("You've emptied out Pandemonium! Nothing left here but demons.");
            mprf("You now have %d runes.", nrunes);
        }

        mpr("Press } to see all the runes you have collected.");
    }

    if (it.sub_type == RUNE_ABYSSAL)
        mpr("You feel the abyssal rune guiding you out of this place.");
}

static bool _is_disabled_gem(gem_type gem)
{
    switch (gem)
    {
#if TAG_MAJOR_VERSION == 34
    case GEM_ORC:
        return true;
#endif
    default:
        return false;
    }
}

static bool _is_ungenerated_gem(gem_type gem)
{
    branch_type br = branch_for_gem(gem);

    return !brentry[br].is_valid() && is_random_subbranch(br);
}

static bool _got_all_gems()
{
    for (int gem = GEM_DUNGEON; gem < NUM_GEM_TYPES; ++gem)
    {
        if (_is_disabled_gem(static_cast<gem_type>(gem)))
            continue;
        if (_is_ungenerated_gem(static_cast<gem_type>(gem)))
            continue;
        if (!you.gems_found[static_cast<gem_type>(gem)])
            return false;
    }
    return true;
}

static void _get_gem(const item_def& it, bool quiet)
{
    you.gems_found.set(it.sub_type);
    if (quiet)
        return;

    flash_view_delay(UA_PICKUP, it.gem_colour(), 300);
    // XXX: consider customizing this message per-gem
    mprf("You pick up %s and feel its impossibly delicate weight in your %s.",
         it.name(DESC_THE).c_str(), you.hand_name(true).c_str());
    if (_got_all_gems())
    {
        mprf("You've found all the gems! Together, they sparkle an otherworldly %s!",
             getSpeakString("misc_colour").c_str());
    }
    mpr("Press } and ! to see all the gems you have collected.");
    print_gem_warnings(it.sub_type, 0);
}

/**
 * Place the Orb of Zot into the player's inventory.
 */
static void _get_orb()
{
    run_animation(ANIMATION_ORB, UA_PICKUP);

    mprf(MSGCH_ORB, "You pick up the Orb of Zot!");

    if (bezotted())
        mpr("Zot can harm you no longer.");

    env.orb_pos = you.pos(); // can be wrong in wizmode
    orb_pickup_noise(you.pos(), 30);

    start_orb_run(CHAPTER_ESCAPING, "Now all you have to do is get back out "
                                    "of the dungeon!");

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_METEORAN)
        update_vision_range();
#endif
}

/**
 * Attempt to merge a stackable item into an existing stack in the player's
 * inventory.
 *
 * @param it[in]            The stack to merge.
 * @param quant_got         The quantity of this item to place.
 * @param inv_slot[out]     The inventory slot the item was placed in. -1 if
 * not placed.
 * @param quiet             Whether to suppress pickup messages.
 */
static bool _merge_stackable_item_into_inv(const item_def &it, int quant_got,
                                           int &inv_slot, bool quiet)
{
    for (inv_slot = 0; inv_slot < ENDOFPACK; inv_slot++)
    {
        if (!items_stack(you.inv[inv_slot], it))
            continue;

        // If the object on the ground is inscribed, but not
        // the one in inventory, then the inventory object
        // picks up the other's inscription.
        if (!(it.inscription).empty()
            && you.inv[inv_slot].inscription.empty())
        {
            you.inv[inv_slot].inscription = it.inscription;
        }

        inc_inv_item_quantity(inv_slot, quant_got);
        you.last_pickup[inv_slot] = quant_got;

        if (!quiet)
        {
#ifdef USE_SOUND
            parse_sound(PICKUP_SOUND);
#endif
            mprf_nocap("%s (gained %d)",
                        menu_colour_item_name(you.inv[inv_slot],
                                                    DESC_INVENTORY).c_str(),
                        quant_got);
        }
        auto_assign_item_slot(you.inv[inv_slot]);

        return true;
    }

    inv_slot = -1;
    return false;
}

static bool _merge_evokers(const item_def &it, int &inv_slot, bool quiet)
{
    for (inv_slot = 0; inv_slot < ENDOFPACK; inv_slot++)
    {
        if (you.inv[inv_slot].base_type != OBJ_MISCELLANY
            || you.inv[inv_slot].sub_type != it.sub_type)
        {
            continue;
        }

        bool can_improve = evoker_plus(it.sub_type) < MAX_EVOKER_ENCHANT;
        if (!can_improve)
        {
            if (!quiet)
            {
                mprf("%s cannot be improved any further.",
                     you.inv[inv_slot].name(DESC_YOUR).c_str());
            }
            return true;
        }

        evoker_plus(it.sub_type)++;
        if (evoker_plus(it.sub_type) == MAX_EVOKER_ENCHANT)
            set_item_autopickup(it, AP_FORCE_OFF);

        if (!quiet)
        {
#ifdef USE_SOUND
            parse_sound(PICKUP_SOUND);
#endif
            mprf_nocap("%s (improved by +1).",
                        menu_colour_item_name(you.inv[inv_slot],
                                                    DESC_INVENTORY).c_str());
        }

        return true;
    }

    inv_slot = -1;
    return false;
}


/**
 * Attempt to merge a wands charges into an existing wand of the same type in
 * inventory.
 *
 * @param it[in]            The wand to merge.
 * @param inv_slot[out]     The inventory slot the wand was placed in. -1 if
 * not placed.
 * @param quiet             Whether to suppress pickup messages.
 */
static bool _merge_wand_charges(const item_def &it, int &inv_slot, bool quiet)
{
    for (inv_slot = 0; inv_slot < ENDOFPACK; inv_slot++)
    {
        if (you.inv[inv_slot].base_type != OBJ_WANDS
            || you.inv[inv_slot].sub_type != it.sub_type)
        {
            continue;
        }

        you.inv[inv_slot].charges += it.charges;

        if (!quiet)
        {
#ifdef USE_SOUND
            parse_sound(PICKUP_SOUND);
#endif
            mprf_nocap("%s (gained %d charge%s)",
                        menu_colour_item_name(you.inv[inv_slot],
                                                    DESC_INVENTORY).c_str(),
                        it.charges, it.charges == 1 ? "" : "s");
        }

        return true;
    }

    inv_slot = -1;
    return false;
}

/**
 * Maybe move an item to the slot given by the item_slot option.
 *
 * @param[in] item the item to be checked. Note that any references to this
 *                 item will be invalidated by the swap_inv_slots call!
 * @returns the new location of the item if it was moved, null otherwise.
 */
item_def *auto_assign_item_slot(item_def& item)
{
    if (!item.defined())
        return nullptr;
    if (!in_inventory(item))
        return nullptr;

    int newslot = -1;
    bool overwrite = true;
    // check to see whether we've chosen an automatic label:
    for (auto& mapping : Options.auto_item_letters)
    {
        // `matches` has a validity check
        if (!mapping.first.matches(item.name(DESC_QUALNAME))
            && !mapping.first.matches(item_prefix(item)
                                      + " " + item.name(DESC_A)))
        {
            continue;
        }
        for (char i : mapping.second)
        {
            if (i == '+')
                overwrite = true;
            else if (i == '-')
                overwrite = false;
            else if (isaalpha(i))
            {
                const int index = letter_to_index(i);
                auto &iitem = you.inv[index];

                // Slot is empty, or overwrite is on and the rule doesn't
                // match the item already there.
                if (!iitem.defined()
                    || overwrite
                       && !mapping.first.matches(iitem.name(DESC_QUALNAME))
                       && !mapping.first.matches(item_prefix(iitem)
                                                 + " " + iitem.name(DESC_A)))
                {
                    newslot = index;
                    break;
                }
            }
        }
        if (newslot != -1 && newslot != item.link)
        {
            swap_inv_slots(item.link, newslot, you.num_turns);
            return &you.inv[newslot];
        }
    }
    return nullptr;
}

/**
 * Move the given item and quantity to a free slot in the player's inventory.
 * If the item_slot option tells us to put it in a specific slot, it will move
 * there and push out the item that was in it before instead.
 *
 * @param it[in]          The item to be placed into the player's inventory.
 * @param quant_got       The quantity of this item to place.
 * @param quiet           Suppresses pickup messages.
 * @return                The inventory slot the item was placed in.
 */
static int _place_item_in_free_slot(item_def &it, int quant_got,
                                    bool quiet)
{
    int freeslot = find_free_slot(it);
    ASSERT_RANGE(freeslot, 0, ENDOFPACK);
    ASSERT(!you.inv[freeslot].defined());

    item_def &item = you.inv[freeslot];
    // Copy item.
    item          = it;
    item.link     = freeslot;
    item.quantity = quant_got;
    item.slot     = index_to_letter(item.link);
    item.pos = ITEM_IN_INVENTORY;
    // Remove "unobtainable" as it was just proven false.
    item.flags &= ~ISFLAG_UNOBTAINABLE;

    ash_id_item(item);
    maybe_identify_base_type(item);

    note_inscribe_item(item);

    if (crawl_state.game_is_hints())
        taken_new_item(item.base_type);

    you.last_pickup[item.link] = quant_got;
    quiver::on_item_pickup(freeslot);
    quiver::on_actions_changed();
    item_skills(item, you.skills_to_show);

    if (const item_def* newitem = auto_assign_item_slot(item))
        return newitem->link;
    else if (!quiet)
    {
#ifdef USE_SOUND
        parse_sound(PICKUP_SOUND);
#endif
        mprf_nocap("%s", menu_colour_item_name(item, DESC_INVENTORY).c_str());
    }

    return item.link;
}

/**
 * Move the given item and quantity to the player's inventory.
 * DOES NOT change the original item; the caller must handle any cleanup!
 *
 * @param it[in]          The item to be placed into the player's inventory.
 * @param quant_got       The quantity of this item to place.
 * @param inv_slot[out]   The inventory slot the item was placed in. -1 if
 * not placed.
 * @param quiet If true, most messages notifying the player of item pickup (or
 *              item pickup failure) aren't printed.
 * @return Whether something was successfully picked up.
 */
static bool _merge_items_into_inv(item_def &it, int quant_got,
                                  int &inv_slot, bool quiet)
{
    inv_slot = -1;

    // sanity
    if (quant_got > it.quantity || quant_got <= 0)
        quant_got = it.quantity;

    // Gold has no mass, so we handle it first.
    if (it.base_type == OBJ_GOLD)
    {
        get_gold(it, quant_got, quiet);
        return true;
    }
    if (it.base_type == OBJ_BOOKS)
    {
        _get_book(it);
        return true;
    }
    if (it.base_type == OBJ_MISCELLANY && it.sub_type == MISC_SHOP_VOUCHER)
    {
        _get_voucher(it);
        return true;
    }
    // Runes and gems are also massless.
    if (it.base_type == OBJ_RUNES)
    {
        _get_rune(it, quiet);
        return true;
    }
    if (it.base_type == OBJ_GEMS)
    {
        _get_gem(it, quiet);
        return true;
    }
    // The Orb is also handled specially.
    if (item_is_orb(it))
    {
        _get_orb();
        return true;
    }

    // attempt to merge into an existing stack, if possible
    if (is_stackable_item(it)
        && _merge_stackable_item_into_inv(it, quant_got, inv_slot, quiet))
    {
        return true;
    }

    // attempt to merge into an existing stack, if possible
    if (it.base_type == OBJ_WANDS
        && _merge_wand_charges(it, inv_slot, quiet))
    {
        quant_got = 1;
        return true;
    }

    // attempt to merge into an existing stack, if possible
    if (is_xp_evoker(it)
        && _merge_evokers(it, inv_slot, quiet))
    {
        quant_got = 1;
        return true;
    }

    // Can't combine, check for slot space.
    if (inv_count() >= ENDOFPACK)
        return false;

    inv_slot = _place_item_in_free_slot(it, quant_got, quiet);
    return true;
}

void mark_items_non_pickup_at(const coord_def &pos)
{
    int item = env.igrid(pos);
    while (item != NON_ITEM)
    {
        env.item[item].flags |= ISFLAG_DROPPED;
        env.item[item].flags &= ~ISFLAG_THROWN;
        // remove any force-pickup autoinscription, otherwise the full
        // inventory pickup check gets stuck in a loop
        env.item[item].inscription = replace_all(
                                        env.item[item].inscription, "=g", "");
        item = env.item[item].link;
    }
}

void clear_item_pickup_flags(item_def &item)
{
    item.flags &= ~(ISFLAG_THROWN | ISFLAG_DROPPED | ISFLAG_NO_PICKUP);
}

// Moves env.item[obj] to p... will modify the value of obj to
// be the index of the final object (possibly different).
//
// Done this way in the hopes that it will be obvious from
// calling code that "obj" is possibly modified.
//
// Returns false on error or level full - cases where you
// keep the item.
bool move_item_to_grid(int *const obj, const coord_def& p, bool silent)
{
    ASSERT_IN_BOUNDS(p);

    int& ob(*obj);

    // Must be a valid reference to a valid object.
    if (ob == NON_ITEM || !env.item[ob].defined())
        return false;

    item_def& item(env.item[ob]);
    bool move_below = item_is_stationary(item) && !item_is_stationary_net(item);

    if (!silenced(p) && !silent)
        feat_splash_noise(env.grid(p));

    if (feat_destroys_items(env.grid(p)))
    {
        item_was_destroyed(item);
        destroy_item(ob);
        ob = NON_ITEM;

        return true;
    }

    // If it's a stackable type or stationary item that's not a net...
    int movable_ind = -1;
    if (move_below || is_stackable_item(item))
    {
        // Look for similar item to stack:
        for (stack_iterator si(p); si; ++si)
        {
            // Check if item already linked here -- don't want to unlink it.
            if (ob == si->index())
                return false;

            if (items_stack(item, *si))
            {
                // Add quantity to item already here, and dispose
                // of obj, while returning the found item. -- bwr
                inc_mitm_item_quantity(si->index(), item.quantity);
                destroy_item(ob);
                ob = si->index();
                gozag_move_gold_to_top(p);
                if (you.see_cell(p))
                {
                    // XXX: Is it actually necessary to identify when the
                    // new item merged with a stack?
                    ash_id_item(*si);
                    maybe_identify_base_type(*si);
                }
                return true;
            }
            if (move_below
                && (!item_is_stationary(*si) || item_is_stationary_net(*si)))
            {
                movable_ind = si->index();
            }
        }
    }
    else
        ASSERT(item.quantity == 1);

    ASSERT(ob != NON_ITEM);

    // Need to actually move object, so first unlink from old position.
    unlink_item(ob);

    // Move item to coord.
    item.pos = p;

    // Need to move this stationary item to the position in the pile
    // below the lowest non-stationary, non-net item.
    if (move_below && movable_ind >= 0)
    {
        item.link = env.item[movable_ind].link;
        env.item[movable_ind].link = item.index();
    }
    // Movable item or no movable items in pile, link item to top of list.
    else
    {
        item.link = env.igrid(p);
        env.igrid(p) = ob;
    }

    if (item_is_orb(item))
        env.orb_pos = p;

    if (item.base_type != OBJ_GOLD)
        gozag_move_gold_to_top(p);

    if (you.see_cell(p))
    {
        ash_id_item(item);
        maybe_identify_base_type(item);
    }

    if (p == you.pos() && _id_floor_item(item))
        mprf("You see here %s.", item.name(DESC_A).c_str());

    return true;
}

void move_item_stack_to_grid(const coord_def& from, const coord_def& to)
{
    if (env.igrid(from) == NON_ITEM)
        return;

    // Tell all items in stack what the new coordinate is.
    for (stack_iterator si(from); si; ++si)
    {
        si->pos = to;

        // Link last of the stack to the top of the old stack.
        if (si->link == NON_ITEM && env.igrid(to) != NON_ITEM)
        {
            si->link = env.igrid(to);
            break;
        }
    }

    env.igrid(to) = env.igrid(from);
    env.igrid(from) = NON_ITEM;
}

// Returns the mitm index of the item. If the item was copied but destroyed,
// returns -1. If there was no space to copy it, returns NON_ITEM.
int copy_item_to_grid(const item_def &item, const coord_def& p,
                        int quant_drop, bool mark_dropped, bool silent)
{
    ASSERT_IN_BOUNDS(p);

    if (quant_drop == 0)
        return NON_ITEM;

    if (!silenced(p) && !silent)
        feat_splash_noise(env.grid(p));

    if (feat_destroys_items(env.grid(p)))
    {
        item_was_destroyed(item);
        return -1;
    }

    // default quant_drop == -1 => drop all
    if (quant_drop < 0)
        quant_drop = item.quantity;

    // Loop through items at current location.
    if (is_stackable_item(item))
    {
        for (stack_iterator si(p); si; ++si)
        {
            if (items_stack(item, *si))
            {
                item_def copy = item;
                inc_mitm_item_quantity(si->index(), quant_drop);

                if (mark_dropped)
                {
                    // If the items on the floor already have a nonzero slot,
                    // leave it as such, otherwise set the slot.
                    if (!si->slot)
                        si->slot = index_to_letter(item.link);

                    si->flags |= ISFLAG_DROPPED;
                    si->flags &= ~ISFLAG_THROWN;
                }
                return si->index();
            }
        }
    }

    // Item not found in current stack, add new item to top.
    int new_item_idx = get_mitm_slot(10);
    if (new_item_idx == NON_ITEM)
        return NON_ITEM;
    item_def& new_item = env.item[new_item_idx];

    // Copy item.
    new_item = item;

    // Set quantity, and set the item as unlinked.
    new_item.quantity = quant_drop;
    new_item.pos.reset();
    new_item.link = NON_ITEM;

    if (mark_dropped)
    {
        new_item.slot   = index_to_letter(item.link);
        new_item.flags |= ISFLAG_DROPPED;
        new_item.flags &= ~ISFLAG_THROWN;
        origin_set_unknown(new_item);
    }

    move_item_to_grid(&new_item_idx, p, true);

    return new_item_idx;
}

coord_def item_pos(const item_def &item)
{
    coord_def pos = item.pos;
    if (pos == ITEM_IN_MONSTER_INVENTORY)
    {
        if (const monster *mon = item.holding_monster())
            pos = mon->pos();
        else
            die("item held by an invalid monster");
    }
    else if (pos == ITEM_IN_INVENTORY)
        pos = you.pos();
    return pos;
}

/**
 * Move the top item of a stack to a new location.
 *
 * @param pos the location of the stack
 * @param dest the position to be moved to
 * @return whether there were any items at pos to be moved.
 */
bool move_top_item(const coord_def &pos, const coord_def &dest)
{
    int item = env.igrid(pos);
    if (item == NON_ITEM)
        return false;

    dungeon_events.fire_position_event(
        dgn_event(DET_ITEM_MOVED, pos, 0, item, -1, dest), pos);

    // Now move the item to its new position...
    move_item_to_grid(&item, dest);

    return true;
}

const item_def* top_item_at(const coord_def& where)
{
    const int link = you.visible_igrd(where);
    return (link == NON_ITEM) ? nullptr : &env.item[link];
}

static bool _check_dangerous_drop(const item_def & item)
{
    if (!feat_eliminates_items(env.grid(you.pos())))
        return true;

    string prompt = "Are you sure you want to drop " + item.name(DESC_THE)
                  + " into "
                  + feature_description_at(you.pos(), false, DESC_A) + "? "
                  + "You won't be able to retrieve "
                  + (item.quantity == 1 ? "it." : "them.");

    // don't interrupt delays; this might do something strange to macros
    // that trigger it, but the main way drops interact with delays is
    // through multidrop and armour delays
    if (yesno(prompt.c_str(), true, 'n', true, false))
        return true;

    canned_msg(MSG_OK);
    return false;
}


/**
 * Drop an item, possibly starting up a delay to do so.
 *
 * @param item_dropped the inventory index of the item to drop
 * @param quant_drop the number of items to drop, -1 to drop the whole stack.
 *
 * @return True if we took time, either because we dropped the item
 *         or because we took a preliminary step (removing a ring, etc.).
 */
bool drop_item(int item_dropped, int quant_drop)
{
    item_def &item = you.inv[item_dropped];

    if (quant_drop < 0 || quant_drop > item.quantity)
        quant_drop = item.quantity;

    if (!_check_dangerous_drop(item))
        return false;

    if (item_is_equipped(item))
    {
        if (item.base_type == OBJ_GIZMOS)
        {
            mpr("That is permanently installed in your exoskeleton.");
            return false;
        }

        const bool is_wpn = is_weapon(item);
        if (!Options.easy_unequip && !is_wpn)
        {
            mprf(MSGCH_PROMPT, "You will have to take that off first.");
            return false;
        }

        if (try_unequip_item(item))
        {
            // The delay handles the case where the item disappeared.
            start_delay<DropItemDelay>(1, item);
            // We didn't actually finish yet, but try_unequip_item either took
            // time or queued up delays, so return true.
            return true;
        }
        else
            return false;
    }

    ASSERT(item.defined());

    if (Options.drop_disables_autopickup
        && !is_artefact(item)
        && item.base_type != OBJ_MISSILES)
    {
        set_item_autopickup(item, AP_FORCE_OFF);
    }

    if (copy_item_to_grid(item, you.pos(), quant_drop, true, true) == NON_ITEM)
    {
        mpr("Too many items on this level, not dropping the item.");
        return false;
    }

    mprf("You drop %s.", quant_name(item, quant_drop, DESC_A).c_str());

    // If you drop an item in as a merfolk, it is below the water line and
    // makes no noise falling.
    if (!you.swimming())
        feat_splash_noise(env.grid(you.pos()));

    dec_inv_item_quantity(item_dropped, quant_drop);

    you.turn_is_over = true;

    you.last_pickup.erase(item_dropped);
    if (you.last_unequip == item.link)
        you.last_unequip = -1;

    return true;
}

void drop_last()
{
    vector<SelItem> items_to_drop;

    for (const auto &entry : you.last_pickup)
    {
        const item_def* item = &you.inv[entry.first];
        if (item->quantity > 0)
            items_to_drop.emplace_back(entry.first, entry.second, item);
    }

    if (items_to_drop.empty())
        mpr("No item to drop.");
    else
    {
        you.last_pickup.clear();
        _multidrop(items_to_drop);
    }
}

mon_inv_type get_mon_equip_slot(const monster* mon, const item_def &item)
{
    ASSERT(mon->alive());

    mon_inv_type slot = item_to_mslot(item);

    if (slot == NUM_MONSTER_SLOTS)
        return NUM_MONSTER_SLOTS;

    if (mon->mslot_item(slot) == &item)
        return slot;

    if (slot == MSLOT_WEAPON && mon->mslot_item(MSLOT_ALT_WEAPON) == &item)
        return MSLOT_ALT_WEAPON;

    return NUM_MONSTER_SLOTS;
}

// This has to be of static storage class, so that the value isn't lost when a
// MultidropDelay is interrupted.
static vector<SelItem> items_for_multidrop;

// Arrange items that have been selected for multidrop so that
// equipped items are dropped after other items.
static bool _drop_item_order(const SelItem &first, const SelItem &second)
{
    if (item_is_equipped(you.inv[first.slot]))
        return false;
    else if (item_is_equipped(you.inv[second.slot]))
        return true;
    else
        return first.slot < second.slot;
}

void set_item_autopickup(const item_def &item, autopickup_level_type ap)
{
    you.force_autopickup[item.base_type][_autopickup_subtype(item)] = ap;
}

int item_autopickup_level(const item_def &item)
{
    return you.force_autopickup[item.base_type][_autopickup_subtype(item)];
}

static void _disable_autopickup_for_starred_items(vector<SelItem> &items)
{
    int autopickup_remove_count = 0;
    const item_def *last_touched_item;
    for (SelItem &si : items)
    {
        if (si.has_star && item_autopickup_level(si.item[0]) != AP_FORCE_OFF)
        {
            last_touched_item = si.item;
            ++autopickup_remove_count;
            set_item_autopickup(*last_touched_item, AP_FORCE_OFF);
        }
    }
    if (autopickup_remove_count == 1)
    {
        mprf("Autopickup disabled for %s.",
             pluralise(last_touched_item->name(DESC_DBNAME)).c_str());
    }
    else if (autopickup_remove_count > 1)
        mprf("Autopickup disabled for %d items.", autopickup_remove_count);
}

/**
 * Prompts the user for an item to drop.
 */
void drop()
{
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    vector<SelItem> tmp_items;

    tmp_items = prompt_drop_items(items_for_multidrop);

    if (tmp_items.empty())
    {
        canned_msg(MSG_OK);
        return;
    }

    _disable_autopickup_for_starred_items(tmp_items);
    _multidrop(tmp_items);
}

static void _multidrop(vector<SelItem> tmp_items)
{
    // Sort the dropped items so that we drop unequipped items first.
    sort(tmp_items.begin(), tmp_items.end(), _drop_item_order);

    // If the user answers "no" to an item an with a warning inscription,
    // then remove it from the list of items to drop by not copying it
    // over to items_for_multidrop.
    items_for_multidrop.clear();
    for (SelItem& si : tmp_items)
    {
        const int item_quant = si.item->quantity;
        ASSERT(item_quant > 0);

        // EVIL HACK: Fix item quantity to match the quantity we will drop,
        // in order to prevent misleading messages when dropping
        // 15 of 25 arrows inscribed with {!d}.
        if (si.quantity && si.quantity != item_quant)
            const_cast<item_def*>(si.item)->quantity = si.quantity;

        // Check if we can add it to the multidrop list.
        bool warning_ok = check_warning_inscriptions(*(si.item), OPER_DROP);

        // Restore the item quantity if we mangled it.
        if (item_quant != si.item->quantity)
            const_cast<item_def*>(si.item)->quantity = item_quant;

        if (warning_ok)
            items_for_multidrop.push_back(si);
    }

    if (items_for_multidrop.empty()) // no items.
    {
        canned_msg(MSG_OK);
        return;
    }

    if (items_for_multidrop.size() == 1) // only one item
    {
        drop_item(items_for_multidrop[0].slot, items_for_multidrop[0].quantity);
        items_for_multidrop.clear();
    }
    else
        start_delay<MultidropDelay>(items_for_multidrop.size(), items_for_multidrop);
}

static void _autoinscribe_item(item_def& item)
{
    // If there's an inscription already, do nothing - except
    // for automatically generated inscriptions
    if (!item.inscription.empty())
        return;
    const string old_inscription = item.inscription;
    item.inscription.clear();

    string iname = _autopickup_item_name(item);

    for (const auto &ai_entry : Options.autoinscriptions)
    {
        if (ai_entry.first.matches(iname))
        {
            // Don't autoinscribe dropped items on ground with
            // "=g". If the item matches a rule which adds "=g",
            // "=g" got added to it before it was dropped, and
            // then the user explicitly removed it because they
            // don't want to autopickup it again.
            string str = ai_entry.second;
            if ((item.flags & ISFLAG_DROPPED) && !in_inventory(item))
                str = replace_all(str, "=g", "");

            // Note that this might cause the item inscription to
            // pass 80 characters.
            item.inscription += str;
        }
    }
    if (!old_inscription.empty())
    {
        if (item.inscription.empty())
            item.inscription = old_inscription;
        else
            item.inscription = old_inscription + ", " + item.inscription;
    }
}

static void _autoinscribe_floor_items()
{
    for (stack_iterator si(you.pos()); si; ++si)
        _autoinscribe_item(*si);
}

static void _autoinscribe_inventory()
{
    for (auto &item : you.inv)
        if (item.defined())
            _autoinscribe_item(item);
}

bool need_to_autoinscribe()
{
    return will_autoinscribe;
}

void request_autoinscribe(bool do_inscribe)
{
    will_autoinscribe = do_inscribe;
}

void autoinscribe()
{
    _autoinscribe_floor_items();
    _autoinscribe_inventory();

    will_autoinscribe = false;
}

/**
 * Resolve an item's subtype to an appropriate value for autopickup.
 * @param item The item to check.
 * @returns The actual sub_type for items that either have an identified
 * sub_type or where the sub_type is always known. Otherwise the value of the
 * max subtype.
*/
static int _autopickup_subtype(const item_def &item)
{
    // Sensed items.
    if (item.base_type >= NUM_OBJECT_CLASSES)
        return MAX_SUBTYPES - 1;

    const int max_type = get_max_subtype(item.base_type);

    // item_defs of unknown subtype.
    if (max_type > 0 && item.sub_type >= max_type)
        return max_type;

    // Only where item_type_known() refers to the subtype (as opposed to the
    // brand, for example) do we have to check it. For weapons etc. we always
    // know the subtype.
    switch (item.base_type)
    {
    case OBJ_JEWELLERY:
        return item_type_known(item) ? item.sub_type
                    : jewellery_is_amulet(item) ? max_type : NUM_RINGS;
    case OBJ_WANDS:
    case OBJ_SCROLLS:
    case OBJ_POTIONS:
    case OBJ_STAVES:
        return item_type_known(item) ? item.sub_type : max_type;
    case OBJ_BOOKS:
        if (item.sub_type == BOOK_MANUAL)
            return item.sub_type;
        else
            return 0;
#if TAG_MAJOR_VERSION == 34
    case OBJ_RODS:
#endif
    case OBJ_GOLD:
    case OBJ_RUNES:
    case OBJ_GEMS:
        return max_type;
    default:
        return item.sub_type;
    }
}

static bool _is_option_autopickup(const item_def &item, bool ignore_force)
{
    if (item.base_type < NUM_OBJECT_CLASSES)
    {
        const int force = item_autopickup_level(item);
        if (!ignore_force && force != AP_FORCE_NONE)
            return force == AP_FORCE_ON;
    }
    else
        return false;

    // the special-cased gold here is because this call can become very heavy
    // for gozag players under extreme circumstances
    const string iname = item.base_type == OBJ_GOLD
                                                ? "{gold}"
                                                : _autopickup_item_name(item);

    maybe_bool res = clua.callmaybefn("ch_force_autopickup", "is",
                                      &item, iname.c_str());
    if (!clua.error.empty())
    {
        mprf(MSGCH_ERROR, "ch_force_autopickup failed: %s",
             clua.error.c_str());
    }

    if (res.is_bool())
        return bool(res);

    // Check for initial settings
    for (const pair<text_pattern, bool>& option : Options.force_autopickup)
        if (option.first.matches(iname))
            return option.second;

    return Options.autopickups[item.base_type];
}

/** Is the item something that we should try to autopickup?
 *
 * @param ignore_force If true, ignore force_autopickup settings from the
 *                     \ menu (default false).
 * @return True if the object should be picked up.
 */
bool item_needs_autopickup(const item_def &item, bool ignore_force)
{
    if (crawl_state.game_is_arena())
        return false;

    if (in_inventory(item))
        return false;

    if (item_is_stationary(item))
        return false;

    if (item.inscription.find("=g") != string::npos)
        return true;

    if ((item.flags & ISFLAG_THROWN) && Options.pickup_thrown)
        return true;

    if (item.flags & ISFLAG_DROPPED)
        return false;

    if (item.props.exists(NEEDS_AUTOPICKUP_KEY))
        return true;

    return _is_option_autopickup(item, ignore_force);
}

bool can_autopickup()
{
    // [ds] Checking for autopickups == 0 is a bad idea because
    // autopickup is still possible with inscriptions and
    // pickup_thrown.
    if (Options.autopickup_on <= 0)
        return false;

    if (!i_feel_safe())
        return false;

    return true;
}

typedef bool (*item_comparer)(const item_def& pickup_item,
                              const item_def& inv_item);

static bool _identical_types(const item_def& pickup_item,
                             const item_def& inv_item)
{
    return pickup_item.is_type(inv_item.base_type, inv_item.sub_type);
}

static bool _similar_equip(const item_def& pickup_item,
                           const item_def& inv_item)
{
    const equipment_slot inv_slot = get_item_slot(inv_item);

    if (inv_slot == SLOT_UNUSED)
        return false;

    if (get_item_slot(pickup_item) != inv_slot)
        return false;

    // Just filling the same slot is enough for armour to be considered
    // similar.
    if (pickup_item.base_type == OBJ_ARMOUR)
        return true;

    return item_attack_skill(pickup_item) == item_attack_skill(inv_item)
           && get_damage_type(pickup_item) == get_damage_type(inv_item);
}

static bool _similar_wands(const item_def& pickup_item,
                           const item_def& inv_item)
{
    if (inv_item.base_type != OBJ_WANDS)
        return false;

    if (pickup_item.sub_type != inv_item.sub_type)
        return false;
#if TAG_MAJOR_VERSION == 34
    // Not similar if wand in inventory is empty.
    return !is_known_empty_wand(inv_item);
#else
    return true;
#endif
}

static bool _similar_jewellery(const item_def& pickup_item,
                               const item_def& inv_item)
{
    if (inv_item.base_type != OBJ_JEWELLERY)
        return false;

    if (pickup_item.sub_type != inv_item.sub_type)
        return false;

    // For jewellery of the same sub-type, unidentified jewellery is
    // always considered similar, as is identified jewellery whose
    // effect doesn't stack.
    return !jewellery_is_amulet(inv_item)
           && !ring_has_stackable_effect(inv_item);
}

static bool _item_different_than_inv(const item_def& pickup_item,
                                     item_comparer comparer)
{
    return none_of(begin(you.inv), end(you.inv),
                   [&] (const item_def &inv_item) -> bool
                   {
                       return inv_item.defined()
                           && comparer(pickup_item, inv_item);
                   });
}

static bool _interesting_explore_pickup(const item_def& item)
{
    if (!(Options.explore_stop & ES_GREEDY_PICKUP_MASK))
        return false;

    if (item.base_type == OBJ_GOLD)
        return Options.explore_stop & ES_GREEDY_PICKUP_GOLD;

    if ((Options.explore_stop & ES_GREEDY_PICKUP_THROWN)
        && (item.flags & ISFLAG_THROWN))
    {
        return true;
    }

    vector<text_pattern> &ignores = Options.explore_stop_pickup_ignore;
    if (!ignores.empty())
    {
        const string name = item.name(DESC_PLAIN);

        for (const text_pattern &pat : ignores)
            if (pat.matches(name))
                return false;
    }

    if (!(Options.explore_stop & ES_GREEDY_PICKUP_SMART))
        return true;
    // "Smart" code follows.

    // If ES_GREEDY_PICKUP_THROWN isn't set, then smart greedy pickup
    // will ignore thrown items.
    if (item.flags & ISFLAG_THROWN)
        return false;

    if (is_artefact(item))
        return true;

    // Possbible ego items.
    if (!item.is_identified() && (item.flags & ISFLAG_COSMETIC_MASK))
        return true;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_MISSILES:
    case OBJ_ARMOUR:
        // Ego items.
        if (item.brand != 0)
            return true;
        break;

    default:
        break;
    }

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_ARMOUR:
        return _item_different_than_inv(item, _similar_equip);

    case OBJ_WANDS:
        return _item_different_than_inv(item, _similar_wands);

    case OBJ_JEWELLERY:
        return _item_different_than_inv(item, _similar_jewellery);

    case OBJ_TALISMANS:
    case OBJ_MISCELLANY:
    case OBJ_SCROLLS:
    case OBJ_POTIONS:
    case OBJ_STAVES:
        // Item is boring only if there's an identical one in inventory.
        return _item_different_than_inv(item, _identical_types);

    case OBJ_BOOKS:
        // Books always start out unidentified.
        return true;

    case OBJ_RUNES:
    case OBJ_GEMS:
    case OBJ_ORBS:
        // Always interesting.
        return true;

    default:
        break;
    }

    return false;
}

static void _do_autopickup()
{
    bool did_pickup     = false;
    int  n_did_pickup   = 0;
    int  n_tried_pickup = 0;

    // Store last_pickup in case we need to restore it.
    // Then clear it to fill with items picked up.
    map<int,int> tmp_l_p = you.last_pickup;
    you.last_pickup.clear();

    int o = you.visible_igrd(you.pos());

    string pickup_warning;
    while (o != NON_ITEM)
    {
        const int next = env.item[o].link;
        item_def& mi = env.item[o];

        if (item_needs_autopickup(mi))
        {
            // Do this before it's picked up, otherwise the picked up
            // item will be in inventory and _interesting_explore_pickup()
            // will always return false.
            const bool interesting_pickup
                = _interesting_explore_pickup(mi);

            const iflags_t iflags(mi.flags);
            if ((iflags & ISFLAG_THROWN))
                learned_something_new(HINT_AUTOPICKUP_THROWN);

            clear_item_pickup_flags(mi);

            const bool pickup_result = move_item_to_inv(o, mi.quantity);

            if (pickup_result)
            {
                did_pickup = true;
                if (interesting_pickup)
                    n_did_pickup++;
            }
            else
            {
                n_tried_pickup++;
                pickup_warning = "Your pack is full.";
                mi.flags = iflags;
            }
        }
        o = next;
    }

    if (!pickup_warning.empty())
        mpr(pickup_warning);

    if (did_pickup)
        you.turn_is_over = true;

    if (you.last_pickup.empty())
        you.last_pickup = tmp_l_p;

    item_check();

    explore_pickup_event(n_did_pickup, n_tried_pickup);
}

void autopickup(bool forced)
{
    _autoinscribe_floor_items();

    will_autopickup = false;
    // pick up things when forced (by input ;;), or when you feel save
    if (forced || can_autopickup())
        _do_autopickup();
    else
        item_check();
}

int inv_count()
{
    return count_if(begin(you.inv), end(you.inv), mem_fn(&item_def::defined));
}

// sub_type == -1 means look for any item of the class
item_def *find_floor_item(object_class_type cls, int sub_type)
{
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
            for (stack_iterator si(coord_def(x,y)); si; ++si)
                if (si->defined()
                    && (si->is_type(cls, sub_type)
                        || si->base_type == cls && sub_type == -1)
                    && !(si->flags & ISFLAG_MIMIC))
                {
                    return &*si;
                }

    return nullptr;
}

int item_on_floor(const item_def &item, const coord_def& where)
{
    // Check if the item is on the floor and reachable.
    for (int link = env.igrid(where); link != NON_ITEM; link = env.item[link].link)
        if (&env.item[link] == &item)
            return link;

    return NON_ITEM;
}

int get_max_subtype(object_class_type base_type)
{
    static int max_subtype[] =
    {
        NUM_WEAPONS,
        NUM_MISSILES,
        NUM_ARMOURS,
        NUM_WANDS,
#if TAG_MAJOR_VERSION == 34
        NUM_FOODS,
#endif
        NUM_SCROLLS,
        NUM_JEWELLERY,
        NUM_POTIONS,
        NUM_BOOKS,
        NUM_STAVES,
        1,              // Orbs         -- only one
        NUM_MISCELLANY,
        -1,              // corpses     -- handled specially
        1,              // gold         -- handled specially
#if TAG_MAJOR_VERSION == 34
        NUM_RODS,
#endif
        NUM_RUNE_TYPES,
        NUM_TALISMANS,
        NUM_GEM_TYPES,
        1,
    };
    COMPILE_CHECK(ARRAYSZ(max_subtype) == NUM_OBJECT_CLASSES);

    ASSERT_RANGE(base_type, 0, NUM_OBJECT_CLASSES);

    return max_subtype[base_type];
}

equipment_slot item_equip_slot(const item_def& item)
{
    if (!item.defined() || !in_inventory(item))
        return SLOT_UNUSED;

    return you.equipment.find_equipped_slot(item);
}

// Includes melded items.
bool item_is_equipped(const item_def &item, bool quiver_too)
{
    return item_equip_slot(item) != SLOT_UNUSED
           || quiver_too && you.quiver_action.item_is_quivered(item);
}

bool item_is_melded(const item_def& item)
{
    return you.equipment.is_melded(item);
}

////////////////////////////////////////////////////////////////////////
// item_def functions.

bool item_def::has_spells() const
{
    return item_is_spellbook(*this);
}

bool item_def::cursed() const
{
    return flags & ISFLAG_CURSED;
}

int item_def::index() const
{
    return this - env.item.buffer();
}

bool valid_item_index(int i)
{
    return i >= 0 && i < MAX_ITEMS;
}

int item_def::armour_rating() const
{
    if (!defined() || base_type != OBJ_ARMOUR)
        return 0;

    return property(*this, PARM_AC) + plus;
}

monster* item_def::holding_monster() const
{
    if (pos != ITEM_IN_MONSTER_INVENTORY)
        return nullptr;
    const int midx = link - NON_ITEM - 1;
    if (invalid_monster_index(midx))
        return nullptr;

    return &env.mons[midx];
}

void item_def::set_holding_monster(const monster& mon)
{
    pos = ITEM_IN_MONSTER_INVENTORY;
    link = NON_ITEM + 1 + mon.mindex();
}

// Note: should not check env.mons, since it may be called by link_items() from
// tags.cc before monsters are unmarshalled.
bool item_def::held_by_monster() const
{
    return pos == ITEM_IN_MONSTER_INVENTORY
             && !invalid_monster_index(link - NON_ITEM - 1);
}

// Note:  This function is to isolate all the checks to see if
//        an item is valid (often just checking the quantity).
//
//        It shouldn't be used a a substitute for those cases
//        which actually want to check the quantity (as the
//        rules for unused objects might change).
bool item_def::defined() const
{
    return base_type != OBJ_UNASSIGNED && quantity > 0;
}
/**
 * Has this item's appearance been initialized?
 */
bool item_def::appearance_initialized() const
{
    return rnd != 0 || is_unrandom_artefact(*this);
}


/**
 * Assuming this item is a randart weapon/armour, what colour is it?
 */
colour_t item_def::randart_colour() const
{
    ASSERT(is_artefact(*this));
    static colour_t colours[] = { YELLOW, LIGHTGREEN, LIGHTRED, LIGHTMAGENTA };
    return colours[rnd % ARRAYSZ(colours)];
}

/**
 * Assuming this item is a weapon, what colour is it?
 */
colour_t item_def::weapon_colour() const
{
    ASSERT(base_type == OBJ_WEAPONS);

    // random artefact
    if (is_artefact(*this))
        return randart_colour();

    if (is_demonic(*this))
        return LIGHTRED;

    switch (item_attack_skill(*this))
    {
        case SK_RANGED_WEAPONS:
            return BLUE;
        case SK_THROWING:
            return WHITE;
        case SK_SHORT_BLADES:
            return CYAN;
        case SK_LONG_BLADES:
            return LIGHTCYAN;
        case SK_AXES:
            return MAGENTA;
        case SK_MACES_FLAILS:
            return LIGHTGREY;
        case SK_POLEARMS:
            return RED;
        case SK_STAVES:
            return GREEN;
        default:
            die("Unknown weapon attack skill %d", item_attack_skill(*this));
            // XXX: give more info!
    }
}

/**
 * Assuming this item is a missile (stone/arrow/bolt/etc), what colour is it?
 */
colour_t item_def::missile_colour() const
{
    ASSERT(base_type == OBJ_MISSILES);

    // TODO: move this into item-prop.cc
    switch (sub_type)
    {
        case MI_STONE:
            return BROWN;
        case MI_LARGE_ROCK:
            return LIGHTGREY;
#if TAG_MAJOR_VERSION == 34
        case MI_NEEDLE:
#endif
        case MI_ARROW:         // removed as an item, but don't crash
        case MI_BOLT:          // removed as an item, but don't crash
        case MI_SLING_BULLET:  // removed as an item, but don't crash
        case MI_SLUG:          // never existed as an item
        case MI_DART:
            return WHITE;
        case MI_JAVELIN:
            return RED;
        case MI_THROWING_NET:
            return MAGENTA;
        case MI_BOOMERANG:
            return GREEN;
        case NUM_SPECIAL_MISSILES:
        default:
            die("invalid missile type");
    }
}

/**
 * Assuming this item is a piece of armour, what colour is it?
 */
colour_t item_def::armour_colour() const
{
    ASSERT(base_type == OBJ_ARMOUR);

    if (is_artefact(*this))
        return randart_colour();

    if (armour_type_is_hide((armour_type)sub_type))
        return mons_class_colour(monster_for_hide((armour_type)sub_type));


    // TODO: move (some of?) this into item-prop.cc
    switch (sub_type)
    {
        case ARM_CLOAK:
        case ARM_SCARF:
        case ARM_CRYSTAL_PLATE_ARMOUR:
            return WHITE;
        case ARM_BARDING:
            return GREEN;
        case ARM_ROBE:
        case ARM_ANIMAL_SKIN:
            return RED;
#if TAG_MAJOR_VERSION == 34
        case ARM_CAP:
#endif
        case ARM_HAT:
        case ARM_HELMET:
            return MAGENTA;
        case ARM_BOOTS:
            return BLUE;
        case ARM_GLOVES:
            return LIGHTBLUE;
        case ARM_LEATHER_ARMOUR:
            return BROWN;
        case ARM_KITE_SHIELD:
        case ARM_TOWER_SHIELD:
        case ARM_BUCKLER:
            return CYAN;
        case ARM_ORB:
            return LIGHTGREY;
        default:
            return LIGHTCYAN;
    }
}

/**
 * Assuming this item is a wand, what colour is it?
 */
colour_t item_def::wand_colour() const
{
    ASSERT(base_type == OBJ_WANDS);

    // this is very odd... a linleyism?
    // TODO: associate colours directly with names
    // (use an array of [name, colour] tuples/structs)
    switch (subtype_rnd % NDSC_WAND_PRI)
    {
        case 0:         //"iron wand"
            return CYAN;
        case 1:         //"brass wand"
        case 5:         //"gold wand"
            return YELLOW;
        case 3:         //"wooden wand"
        case 4:         //"copper wand"
        case 7:         //"bronze wand"
            return BROWN;
        case 6:         //"silver wand"
            return WHITE;
        case 11:        //"fluorescent wand"
            return LIGHTGREEN;
        case 2:         //"bone wand"
        case 8:         //"ivory wand"
        case 9:         //"glass wand"
        case 10:        //"lead wand"
        default:
            return LIGHTGREY;
    }
}

/**
 * Assuming this item is a potion, what colour is it?
 */
colour_t item_def::potion_colour() const
{
    ASSERT(base_type == OBJ_POTIONS);

    // TODO: associate colours directly with names
    // (use an array of [name, colour] tuples/structs)
    static const COLOURS potion_colours[] =
    {
#if TAG_MAJOR_VERSION == 34
        // clear
        LIGHTGREY,
#endif
        // blue, black, silvery, cyan, purple, orange
        BLUE, LIGHTGREY, WHITE, CYAN, MAGENTA, LIGHTRED,
        // inky, red, yellow, green, brown, pink, white
        BLUE, RED, YELLOW, GREEN, BROWN, LIGHTMAGENTA, WHITE,
        // emerald, grey, pink, copper, gold, dark, puce
        LIGHTGREEN, LIGHTGREY, LIGHTRED, YELLOW, YELLOW, BROWN, BROWN,
        // amethyst, sapphire
        MAGENTA, BLUE
    };
    COMPILE_CHECK(ARRAYSZ(potion_colours) == NDSC_POT_PRI);
    return potion_colours[subtype_rnd % NDSC_POT_PRI];
}

/**
 * Assuming this item is a ring, what colour is it?
 */
colour_t item_def::ring_colour() const
{
    ASSERT(!jewellery_is_amulet(*this));
    // jewellery_is_amulet asserts base type

    // TODO: associate colours directly with names
    // (use an array of [name, colour] tuples/structs)
    switch (subtype_rnd % NDSC_JEWEL_PRI)
    {
        case 1:                 // "silver ring"
        case 8:                 // "granite ring"
        case 9:                 // "ivory ring"
        case 15:                // "bone ring"
        case 16:                // "diamond ring"
        case 20:                // "opal ring"
        case 21:                // "pearl ring"
        case 26:                // "onyx ring"
            return LIGHTGREY;
        case 3:                 // "iron ring"
        case 4:                 // "steel ring"
        case 13:                // "glass ring"
            return CYAN;
        case 5:                 // "tourmaline ring"
        case 22:                // "coral ring"
            return MAGENTA;
        case 10:                // "ruby ring"
        case 19:                // "garnet ring"
            return RED;
        case 11:                // "marble ring"
        case 12:                // "jade ring"
        case 14:                // "agate ring"
        case 17:                // "emerald ring"
        case 18:                // "peridot ring"
            return GREEN;
        case 23:                // "sapphire ring"
        case 24:                // "cabochon ring"
        case 28:                // "moonstone ring"
            return BLUE;
        case 0:                 // "wooden ring"
        case 2:                 // "golden ring"
        case 6:                 // "brass ring"
        case 7:                 // "copper ring"
        case 25:                // "gilded ring"
        case 27:                // "bronze ring"
        default:
            return BROWN;
    }
}

/**
 * Assuming this item is an amulet, what colour is it?
 */
colour_t item_def::amulet_colour() const
{
    ASSERT(jewellery_is_amulet(*this));
    // jewellery_is_amulet asserts base type

    // TODO: associate colours directly with names
    // (use an array of [name, colour] tuples/structs)
    switch (subtype_rnd % NDSC_JEWEL_PRI)
    {
        case 1:             // "zirconium amulet"
        case 10:            // "bone amulet"
        case 11:            // "platinum amulet"
        case 16:            // "pearl amulet"
        case 20:            // "diamond amulet"
            return LIGHTGREY;
        case 0:             // "sapphire amulet"
        case 17:            // "blue amulet"
        case 26:            // "lapis lazuli amulet"
            return BLUE;
        case 3:             // "emerald amulet"
        case 12:            // "jade amulet"
        case 18:            // "peridot amulet"
        case 21:            // "malachite amulet"
        case 25:            // "soapstone amulet"
        case 28:            // "beryl amulet"
            return GREEN;
        case 4:             // "garnet amulet"
        case 8:             // "ruby amulet"
        case 19:            // "jasper amulet"
        case 15:            // "cameo amulet"
            return RED;
        case 22:            // "steel amulet"
        case 24:            // "silver amulet"
        case 27:            // "filigree amulet"
            return CYAN;
        case 13:            // "fluorescent amulet"
        case 14:            // "amethyst amulet"
        case 23:            // "cabochon amulet"
            return MAGENTA;
        case 2:             // "golden amulet"
        case 5:             // "bronze amulet"
        case 6:             // "brass amulet"
        case 7:             // "copper amulet"
        case 9:             // "citrine amulet"
        default:
            return BROWN;
    }
}

/**
 * Assuming this is a piece of jewellery (ring or amulet), what colour is it?
 */
colour_t item_def::jewellery_colour() const
{
    ASSERT(base_type == OBJ_JEWELLERY);

    //randarts are bright, normal jewellery is dark
    if (is_random_artefact(*this))
        return LIGHTGREEN + (rnd % (WHITE - LIGHTGREEN + 1));

    if (jewellery_is_amulet(*this))
        return amulet_colour();
    return ring_colour();
}

/**
 * Assuming this item is a book, what colour is it?
 */
colour_t item_def::book_colour() const
{
    ASSERT(base_type == OBJ_BOOKS);

    if (sub_type == BOOK_MANUAL)
        return WHITE;

    switch (rnd % NDSC_BOOK_PRI)
    {
        case 0:
            return BROWN;
        case 1:
            return CYAN;
        case 2:
            return LIGHTGREY;
        case 3:
        case 4:
        default:
        {
            /// everything but BLACK, WHITE (manuals), and DARKGREY (useless)
            static const colour_t colours[] = {
                BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHTGREY, LIGHTBLUE,
                LIGHTGREEN, LIGHTCYAN, LIGHTRED, LIGHTMAGENTA, YELLOW
            };
            return colours[rnd % ARRAYSZ(colours)];
        }
    }
}

/**
 * Assuming this item is a Rune of Zot, what colour is it?
 */
colour_t item_def::rune_colour() const
{
    switch (sub_type)
    {
        case RUNE_DIS:                      // iron
            return ETC_IRON;

        case RUNE_COCYTUS:                  // icy
            return ETC_ICE;

        case RUNE_TARTARUS:                 // bone
        case RUNE_SPIDER:
            return ETC_BONE;

        case RUNE_SLIME:                    // slimy
            return ETC_SLIME;

        case RUNE_SNAKE:                    // serpentine
            return ETC_POISON;

        case RUNE_ELF:                      // elven
            return ETC_ELVEN;

        case RUNE_VAULTS:                   // silver
            return ETC_SILVER;

        case RUNE_TOMB:                     // golden
            return ETC_GOLD;

        case RUNE_SWAMP:                    // decaying
            return ETC_DECAY;

        case RUNE_SHOALS:                   // barnacled
            return ETC_WATER;

        case RUNE_DEMONIC:                  // random Pandemonium lords
        {
            static const element_type types[] =
            {ETC_EARTH, ETC_ELECTRICITY, ETC_ENCHANT, ETC_HEAL, ETC_BLOOD,
             ETC_DEATH, ETC_UNHOLY, ETC_VEHUMET, ETC_BEOGH, ETC_CRYSTAL,
             ETC_SMOKE, ETC_DWARVEN, ETC_ORCISH, ETC_FLASH};

            return types[rnd % ARRAYSZ(types)];
        }

        case RUNE_ABYSSAL:
            return ETC_RANDOM;

        case RUNE_MNOLEG:                   // glowing
            return ETC_MUTAGENIC;

        case RUNE_LOM_LOBON:                // magical
            return ETC_MAGIC;

        case RUNE_CEREBOV:                  // fiery
            return ETC_FIRE;

        case RUNE_GEHENNA:                  // obsidian
        case RUNE_GLOORX_VLOQ:              // dark
        default:
            return ETC_DARK;
    }
}

/**
 * Assuming this item is a gem, what colour is it?
 */
colour_t item_def::gem_colour() const
{
    switch (sub_type)
    {
    default:
    case GEM_DUNGEON: return LIGHTGREY;
#if TAG_MAJOR_VERSION == 34
    case GEM_ORC:     return ETC_GOLD;
#endif
    case GEM_ELF:     return ETC_ELVEN;
    case GEM_LAIR:    return GREEN;

    case GEM_SWAMP:   return ETC_DECAY;
    case GEM_SHOALS:  return ETC_ENCHANT;
    case GEM_SNAKE:   return ETC_POISON;
    case GEM_SPIDER:  return WHITE;

    case GEM_SLIME:   return ETC_AIR;
    case GEM_VAULTS:  return ETC_STEEL;
    case GEM_CRYPT:   return ETC_BONE;
    case GEM_TOMB:    return ETC_AWOKEN_FOREST; // enh
    case GEM_DEPTHS:  return ETC_DITHMENOS;
    case GEM_ZOT:     return ETC_RANDOM; // dubious
    }
}

static colour_t _zigfig_colour()
{
    const int zigs = you.zigs_completed;
    return zigs >= 27 ? ETC_JEWEL :
           zigs >=  9 ? ETC_FLASH :
           zigs >=  3 ? ETC_MAGIC :
           zigs >=  1 ? ETC_SHIMMER_BLUE :
                        ETC_BONE;
}
/**
 * Assuming this item is a talisman, what colour is it?
 */
colour_t item_def::talisman_colour() const
{
    ASSERT(base_type == OBJ_TALISMANS);

    switch (sub_type)
    {
    case TALISMAN_BEAST:
        return YELLOW; // brown taken by staves
    case TALISMAN_FLUX:
        return CYAN; // could maybe swap this and death
    case TALISMAN_MAW:
        return ETC_BLOOD;
    case TALISMAN_SERPENT:
        return ETC_POISON;
    case TALISMAN_BLADE:
        return ETC_IRON;
    case TALISMAN_STATUE:
        return ETC_EARTH;
    case TALISMAN_DRAGON:
        return ETC_FIRE;
    case TALISMAN_VAMPIRE:
        return LIGHTMAGENTA;
    case TALISMAN_DEATH:
        return MAGENTA;
    case TALISMAN_STORM:
        return ETC_ELECTRICITY;
    default:
        return LIGHTGREEN;
    }
}

/**
 * Assuming this item is a misc (non-wand evocable) item, what colour is it?
 */
colour_t item_def::miscellany_colour() const
{
    ASSERT(base_type == OBJ_MISCELLANY);

    switch (sub_type)
    {
#if TAG_MAJOR_VERSION == 34
        case MISC_FAN_OF_GALES:
            return CYAN;
        case MISC_BOTTLED_EFREET:
            return RED;
#endif
        case MISC_PHANTOM_MIRROR:
            return RED;
#if TAG_MAJOR_VERSION == 34
        case MISC_STONE_OF_TREMORS:
            return BROWN;
#endif
        case MISC_LIGHTNING_ROD:
            return LIGHTGREY;
        case MISC_PHIAL_OF_FLOODS:
            return LIGHTBLUE;
        case MISC_BOX_OF_BEASTS:
            return LIGHTGREEN; // ugh, but we're out of other options
#if TAG_MAJOR_VERSION == 34
        case MISC_CRYSTAL_BALL_OF_ENERGY:
            return LIGHTCYAN;
#endif
        case MISC_HORN_OF_GERYON:
            return LIGHTRED;
        case MISC_SACK_OF_SPIDERS:
            return WHITE;
        case MISC_GRAVITAMBOURINE:
            return LIGHTMAGENTA;
#if TAG_MAJOR_VERSION == 34
        case MISC_LAMP_OF_FIRE:
            return YELLOW;
        case MISC_BUGGY_LANTERN_OF_SHADOWS:
        case MISC_BUGGY_EBONY_CASKET:
            return DARKGREY;
#endif
        case MISC_TIN_OF_TREMORSTONES:
            return BROWN;
        case MISC_CONDENSER_VANE:
            return WHITE;
#if TAG_MAJOR_VERSION == 34
        case MISC_XOMS_CHESSBOARD:
            return ETC_RANDOM;
#endif
        case MISC_QUAD_DAMAGE:
            return ETC_DARK;
        case MISC_ZIGGURAT:
            return _zigfig_colour();
        default:
            return LIGHTGREEN;
    }
}

/**
 * Assuming this item is a corpse, what colour is it?
 */
colour_t item_def::corpse_colour() const
{
    ASSERT(base_type == OBJ_CORPSES);

    switch (sub_type)
    {
        case CORPSE_SKELETON:
            return LIGHTGREY;
        case CORPSE_BODY:
        {
            const colour_t class_colour = mons_class_colour(mon_type);
#if TAG_MAJOR_VERSION == 34
            if (class_colour == COLOUR_UNDEF)
                return LIGHTRED;
#else
            ASSERT(class_colour != COLOUR_UNDEF);
#endif
            return class_colour;
        }
        default:
            die("Unknown corpse type: %d", sub_type); // XXX: add more info
    }
}

/**
 * What colour is this item?
 *
 * @return The colour that the item should be displayed as.
 *         Used for console glyphs.
 */
colour_t item_def::get_colour() const
{
    // props take first priority
    if (props.exists(FORCED_ITEM_COLOUR_KEY))
    {
        const colour_t colour = props[FORCED_ITEM_COLOUR_KEY].get_int();
        ASSERT(colour);
        return colour;
    }

    // unrands get to override everything else (wrt colour)
    if (is_unrandom_artefact(*this))
    {
        const unrandart_entry *unrand = get_unrand_entry(
                                            find_unrandart_index(*this));
        ASSERT(unrand);
        ASSERT(unrand->colour);
        return unrand->colour;
    }

    switch (base_type)
    {
        case OBJ_WEAPONS:
            return weapon_colour();
        case OBJ_MISSILES:
            return missile_colour();
        case OBJ_ARMOUR:
            return armour_colour();
        case OBJ_WANDS:
            return wand_colour();
        case OBJ_POTIONS:
            return potion_colour();
#if TAG_MAJOR_VERSION == 34
        case OBJ_FOOD:
            return LIGHTRED;
#endif
        case OBJ_JEWELLERY:
            return jewellery_colour();
        case OBJ_SCROLLS:
            return LIGHTGREY;
        case OBJ_BOOKS:
            return book_colour();
#if TAG_MAJOR_VERSION == 34
        case OBJ_RODS:
            return YELLOW;
#endif
        case OBJ_STAVES:
            return BROWN;
        case OBJ_ORBS:
            return ETC_MUTAGENIC;
        case OBJ_CORPSES:
            return corpse_colour();
        case OBJ_MISCELLANY:
            return miscellany_colour();
        case OBJ_TALISMANS:
            return talisman_colour();
        case OBJ_GOLD:
            return YELLOW;
        case OBJ_RUNES:
            return rune_colour();
        case OBJ_GEMS:
            return gem_colour();
        case OBJ_DETECTED:
            return Options.detected_item_colour;
        case NUM_OBJECT_CLASSES:
        case OBJ_UNASSIGNED:
        case OBJ_RANDOM: // not sure what to do with these three
        default:
            return LIGHTGREY;
    }
}

bool item_type_has_unidentified(object_class_type base_type)
{
    return base_type == OBJ_WANDS
        || base_type == OBJ_SCROLLS
        || base_type == OBJ_JEWELLERY
        || base_type == OBJ_POTIONS
        || base_type == OBJ_BOOKS
        || base_type == OBJ_STAVES
#if TAG_MAJOR_VERSION == 34
        || base_type == OBJ_RODS
#endif
        ;
}

// Checks whether the item is actually a good one.
// TODO: check brands, etc.
bool item_def::is_valid(bool iinfo, bool error) const
{
    auto channel = error ? MSGCH_ERROR : MSGCH_DIAGNOSTICS;
    if (base_type == OBJ_DETECTED)
    {
        if (!iinfo)
            mprf(channel, "weird detected item");
        return iinfo;
    }
    else if (!defined())
    {
        mprf(channel, "undefined item");
        return false;
    }
    const int max_sub = get_max_subtype(base_type);
    if (max_sub != -1 && sub_type >= max_sub)
    {
        if (!iinfo || sub_type > max_sub || !item_type_has_unidentified(base_type))
        {
            if (!iinfo)
                mprf(channel, "weird item subtype and no info");
            if (sub_type > max_sub)
                mprf(channel, "huge item subtype");
            if (!item_type_has_unidentified(base_type))
                mprf(channel, "unided item of a type that can't be");
            return false;
        }
    }
    if (get_colour() == 0)
    {
        mprf(channel, "item color invalid"); // 0 = BLACK and so invisible
        return false;
    }
    if (!appearance_initialized())
    {
        mprf(channel, "item has uninitialized rnd");
        return false; // no items with uninitialized rnd
    }
    return true;
}

// The Orb of Zot, gems, and unique runes are considered critical.
bool item_def::is_critical() const
{
    if (!defined())
        return false;

    if (base_type == OBJ_ORBS || base_type == OBJ_GEMS)
        return true;

    return item_is_unique_rune(*this);
}

// Is item something that no one would usually bother enchanting?
bool item_def::is_mundane() const
{
    switch (base_type)
    {
    case OBJ_WEAPONS:
        if (sub_type == WPN_CLUB
            || is_giant_club_type(sub_type))
        {
            return true;
        }
        break;

    case OBJ_ARMOUR:
        if (sub_type == ARM_ANIMAL_SKIN)
            return true;
        break;

    default:
        break;
    }

    return false;
}

static void _rune_from_specs(const char* _specs, item_def &item)
{
    char specs[80];

    if (strstr(_specs, "rune of zot"))
    {
        strlcpy(specs, _specs, min(strlen(_specs) - strlen(" of zot") + 1,
                                   sizeof(specs)));
    }
    else
        strlcpy(specs, _specs, sizeof(specs));

    if (strlen(specs) > 4)
    {
        for (int i = 0; i < NUM_RUNE_TYPES; ++i)
        {
            item.sub_type = i;

            if (lowercase_string(item.name(DESC_PLAIN)).find(specs) != string::npos)
                return;
        }
    }

    while (true)
    {
        string line;
        for (int i = 0; i < NUM_RUNE_TYPES; i++)
        {
            line += make_stringf("[%c] %-10s ", i + 'a', rune_type_name(i));
            if (i % 5 == 4 || i == NUM_RUNE_TYPES - 1)
            {
                mprf(MSGCH_PROMPT, "%s", line.c_str());
                line.clear();
            }
        }
        mprf(MSGCH_PROMPT, "Which rune (ESC to exit)? ");

        int keyin = toalower(get_ch());

        if (key_is_escape(keyin) || keyin == ' '
            || keyin == '\r' || keyin == '\n')
        {
            canned_msg(MSG_OK);
            item.base_type = OBJ_UNASSIGNED;
            return;
        }

        if (keyin < 'a' || keyin >= 'a' + NUM_RUNE_TYPES)
            continue;

        item.sub_type = keyin - 'a';

        return;
    }
}

static bool _book_from_spell(const char* specs, item_def &item)
{
    spell_type type = spell_by_name(specs, true);

    if (type == SPELL_NO_SPELL)
        return false;

    for (int i = 0; i < NUM_BOOKS; ++i)
    {
        const auto bt = static_cast<book_type>(i);
        if (!book_exists(bt))
            continue;
        for (spell_type sp : spellbook_template(bt))
        {
            if (sp == type)
            {
                item.sub_type = i;
                return true;
            }
        }
    }

    return false;
}

bool get_item_by_name(item_def *item, const char* specs,
                      object_class_type class_wanted, bool create_for_real)
{
    // used only for wizmode and item lookup

    int            type_wanted    = -1;
    int            special_wanted = 0;

    // In order to get the sub-type, we'll fill out the base type...
    // then we're going to iterate over all possible subtype values
    // and see if we get a winner. -- bwr
    item->base_type = class_wanted;
    item->sub_type  = 0;
    item->plus      = 0;
    item->plus2     = 0;
    item->special   = 0;
    item->flags     = 0;
    item->quantity  = 1;
    item->flags    |= ISFLAG_IDENTIFIED;

    if (class_wanted == OBJ_RUNES && strstr(specs, "rune"))
    {
        _rune_from_specs(specs, *item);

        // Rune creation cancelled, clean up item->
        if (item->base_type == OBJ_UNASSIGNED)
            return false;
    }

    if (!item->sub_type)
    {
        type_wanted = -1;
        size_t best_index  = 10000;

        for (const auto i : all_item_subtypes(item->base_type))
        {
            item->sub_type = i;
            size_t pos = lowercase_string(item->name(DESC_PLAIN)).find(specs);
            if (pos != string::npos)
            {
                // Earliest match is the winner.
                if (pos < best_index)
                {
                    if (create_for_real)
                        mpr(item->name(DESC_PLAIN));
                    type_wanted = i;
                    best_index = pos;
                }
            }
        }

        if (type_wanted != -1)
        {
            item->sub_type = type_wanted;
            if (!create_for_real)
                return true;
        }
        else
        {
            switch (class_wanted)
            {
            case OBJ_BOOKS:
                // Try if we get a match against a spell.
                if (_book_from_spell(specs, *item))
                    type_wanted = item->sub_type;
                break;

            // Search for a matching unrandart.
            case OBJ_WEAPONS:
            case OBJ_ARMOUR:
            case OBJ_JEWELLERY:
            {
                for (int unrand = 0; unrand < NUM_UNRANDARTS; ++unrand)
                {
                    int index = unrand + UNRAND_START;
                    const unrandart_entry* entry = get_unrand_entry(index);
                    unwind_var<unique_item_status_type> status(you.unique_items[unrand], UNIQ_NOT_EXISTS);
                    unwind_var<uint8_t> octo(you.octopus_king_rings, 0x0); // easier to do unconditionally

                    size_t pos = lowercase_string(entry->name).find(specs);
                    if (pos != string::npos && entry->base_type == class_wanted)
                    {
                        make_item_unrandart(*item, index);
                        if (create_for_real)
                        {
                            mprf("%s (%s)", entry->name,
                                 debug_art_val_str(*item).c_str());
                        }
                        return true;
                    }
                }

                // Reset base type to class_wanted, if nothing found.
                item->base_type = class_wanted;
                item->sub_type  = 0;
                break;
            }

            default:
                break;
            }
        }

        if (type_wanted == -1)
        {
            // ds -- If specs is a valid int, try using that.
            //       Since zero is atoi's copout, the wizard
            //       must enter (subtype + 1).
            if (!(type_wanted = atoi(specs)))
                return false;

            type_wanted--;

            item->sub_type = type_wanted;
        }
    }

    if (!create_for_real)
        return true;

    switch (item->base_type)
    {
    case OBJ_MISSILES:
        item->quantity = 30;
        // intentional fall-through
    case OBJ_WEAPONS:
    case OBJ_ARMOUR:
    {
        char buf[80];
        msgwin_get_line_autohist("What ego type? ", buf, sizeof(buf));

        if (buf[0] != '\0')
        {
            string buf_lwr = lowercase_string(buf);
            special_wanted = 0;
            size_t best_index = 10000;
            const int brand_index = max(static_cast<int>(NUM_SPECIAL_WEAPONS),
                                        static_cast<int>(NUM_SPECIAL_ARMOURS));

            for (int i = SPWPN_NORMAL + 1; i < brand_index; ++i)
            {
                item->brand = i;
                size_t pos = lowercase_string(item->name(DESC_PLAIN)).find(buf_lwr);
                if (pos != string::npos)
                {
                    // earliest match is the winner
                    if (pos < best_index)
                    {
                        if (create_for_real)
                            mpr(item->name(DESC_PLAIN));
                        special_wanted = i;
                        best_index = pos;
                    }
                }
            }

            item->brand = special_wanted;
        }
        break;
    }

    case OBJ_BOOKS:
        if (item->sub_type == BOOK_MANUAL)
        {
            skill_type skill =
                    debug_prompt_for_skill("A manual for which skill? ");

            if (skill != SK_NONE)
                item->skill = skill;
            else
            {
                mpr("Sorry, no books on that skill today.");
                item->skill = SK_FIGHTING; // Was probably that anyway.
            }
            item->skill_points = random_range(2000, 3000);
        }
        else if (type_wanted == BOOK_RANDART_THEME)
            build_themed_book(*item, capped_spell_filter(20));
        else if (type_wanted == BOOK_RANDART_LEVEL)
        {
            int level = random_range(1, 9);
            make_book_level_randart(*item, level);
        }
        break;

    case OBJ_WANDS:
        item->plus = wand_charge_value(item->sub_type);
        break;

    case OBJ_POTIONS:
        item->quantity = 12;
        break;

    case OBJ_SCROLLS:
        item->quantity = 12;
        break;

    case OBJ_JEWELLERY:
        if (jewellery_is_amulet(*item))
            break;

        switch (item->sub_type)
        {
        case RING_SLAYING:
        case RING_PROTECTION:
        case RING_EVASION:
        case RING_STRENGTH:
        case RING_DEXTERITY:
        case RING_INTELLIGENCE:
            item->plus = 5;
        default:
            break;
        }

    default:
        break;
    }

    item_set_appearance(*item);

    return true;
}

static bool _locate_manual_by_exact_name(item_def &item, const char *lc_name)
{
    // lookup manual names. We need to line up pluses with names to get this
    // right; in contrast to the regular strategy for exact name lookup, this
    // call just checks the item name cache directly.

    // preconditions: we already have a manual, just need to find the right
    // plus value
    if (item.base_type != OBJ_BOOKS || item.sub_type != BOOK_MANUAL)
        return false;

    // XX can/should any other name lookups be done via the item name cache?
    // any mismatch between the name cache and the exact name lookup will lead
    // to errors when querying by glyph.
    auto item_kind = item_kind_by_name(lc_name);
    if (item_kind.base_type == OBJ_UNASSIGNED // not found (here for clarity, 2nd disjunct covers this)
        || item_kind.base_type != item.base_type // not a book
        || item_kind.sub_type != item.sub_type) // not a manual
    {
        return false;
    }
    item.plus = item_kind.plus;
    return true;
}

bool get_item_by_exact_name(item_def &item, const char* name)
{
    item.clear();
    item.quantity = 1;
    item.flags |= ISFLAG_IDENTIFIED;

    string name_lc = lowercase_string(string(name));

    // XX could we just use the item name cache instead of iterating through
    // every name?
    for (int i = 0; i < NUM_OBJECT_CLASSES; ++i)
    {
        if (i == OBJ_RUNES) // runes aren't shown in ?/I
            continue;

        item.base_type = static_cast<object_class_type>(i);
        item.sub_type = 0;

        if (!item.sub_type)
        {
            for (const auto j : all_item_subtypes(item.base_type))
            {
                item.sub_type = j;
                if (lowercase_string(item.name(DESC_DBNAME)) == name_lc)
                    return true;
                // if it's a manual, we also need to find the plus value:
                if (_locate_manual_by_exact_name(item, name_lc.c_str()))
                    return true;
            }
        }
    }
    return false;
}

void move_items(const coord_def r, const coord_def p)
{
    ASSERT_IN_BOUNDS(r);
    ASSERT_IN_BOUNDS(p);

    int it = env.igrid(r);

    if (it == NON_ITEM)
        return;

    while (it != NON_ITEM)
    {
        env.item[it].pos.x = p.x;
        env.item[it].pos.y = p.y;
        if (env.item[it].link == NON_ITEM)
        {
            // Link to the stack on the target grid p,
            // or NON_ITEM, if empty.
            env.item[it].link = env.igrid(p);
            break;
        }
        it = env.item[it].link;
    }

    // Move entire stack over to p.
    env.igrid(p) = env.igrid(r);
    env.igrid(r) = NON_ITEM;
}

int runes_in_pack()
{
    return static_cast<int>(you.runes.count());
}

/// Includes destroyed gems.
int gems_found()
{
    return static_cast<int>(you.gems_found.count());
}

int gems_lost()
{
    int count = 0;
    for (int i = 0; i < NUM_GEM_TYPES; i++)
        if (you.gems_found[i] && you.gems_shattered[i])
            count += 1;
    return count;
}

int gems_held_intact()
{
    return gems_found() - gems_lost();
}

object_class_type get_random_item_mimic_type()
{
   return random_choose(OBJ_GOLD, OBJ_WEAPONS, OBJ_ARMOUR, OBJ_SCROLLS,
                        OBJ_POTIONS, OBJ_BOOKS, OBJ_STAVES,
                        OBJ_MISCELLANY, OBJ_TALISMANS, OBJ_JEWELLERY);
}

/**
 * How many types of identifiable item exist in the same category as the
 * given item? (E.g., wands, scrolls, rings, etc.)
 *
 * @param item      The item in question.
 * @return          The number of item enums in the same category.
 *                  If the item isn't in a category of identifiable items,
 *                  returns 0.
 */
static int _items_in_category(const item_def &item)
{
    switch (item.base_type)
    {
        case OBJ_WANDS:
            return NUM_WANDS;
        case OBJ_STAVES:
            return NUM_STAVES;
        case OBJ_POTIONS:
            return NUM_POTIONS;
        case OBJ_SCROLLS:
            return NUM_SCROLLS;
        case OBJ_JEWELLERY:
            if (jewellery_is_amulet(item.sub_type))
                return NUM_JEWELLERY - AMU_FIRST_AMULET;
            return NUM_RINGS - RING_FIRST_RING;
        default:
            return 0;
    }
}

/**
 * What's the first enum for the given item's category?
 *
 * @param item  The item in question.
 * @return      The enum value for the first item of the given type.
 */
static int _get_item_base(const item_def &item)
{
    if (item.base_type != OBJ_JEWELLERY)
        return 0; // XXX: dubious
    if (jewellery_is_amulet(item))
        return AMU_FIRST_AMULET;
    return RING_FIRST_RING;
}

/**
 * Autoidentify the (given) last item in its category.
 *
 8 @param item  The item to identify.
 */
static void _identify_last_item(item_def &item)
{
    if (!in_inventory(item) && item_needs_autopickup(item)
        && (item.base_type == OBJ_STAVES
            || item.base_type == OBJ_JEWELLERY))
    {
        item.props[NEEDS_AUTOPICKUP_KEY] = true;
    }

    identify_item(item);

    if (item.props.exists(NEEDS_AUTOPICKUP_KEY) && is_useless_item(item))
        item.props.erase(NEEDS_AUTOPICKUP_KEY);

    const string class_name = item.base_type == OBJ_JEWELLERY ?
                                    item_base_name(item) :
                                    item_class_name(item.base_type, true);
    mprf("You have identified the last %s.", class_name.c_str());

    if (in_inventory(item))
    {
        mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
        auto_assign_item_slot(item);
    }
}


/**
 * Check to see if there's only one unidentified subtype left in the given
 * item's object type. If so, automatically identify it. Also mark item sets
 * known, if appropriate.
 *
 * @param item  The item in question.
 * @return      Whether the item was identified.
 */
bool maybe_identify_base_type(item_def &item)
{
    if (is_artefact(item))
        return false;

    maybe_mark_set_known(item.base_type, item.sub_type);

    if (type_is_identified(item))
        return false;

    const int item_count = _items_in_category(item);
    if (!item_count)
        return false;

    // What's the enum of the first item in the category?
    const int item_base = _get_item_base(item);

    int ident_count = 0;

    for (int i = item_base; i < item_count + item_base; i++)
    {
        const bool identified = you.type_ids[item.base_type][i]
                             || item_known_excluded_from_set(item.base_type, i);
        ident_count += identified ? 1 : 0;
    }

    if (ident_count < item_count - 1)
        return false;

    _identify_last_item(item);
    return true;
}

void name_weapon(item_def &item)
{
    string name = getRandMonNameString("steelspirit");
    if (name == "RANDGEN")
        name = make_name();
    item.props[WEAPON_NAME_KEY] = name;

    if (!item.inscription.empty())
        item.inscription += ", ";
    item.inscription += name;
}

string get_weapon_name(const item_def &item, bool full_name)
{
    const string it_name = item.name(DESC_YOUR, false, false, false);

    // Artefacts have names already.
    if (is_artefact(item))
        return it_name;

    ASSERT(item.props.exists(WEAPON_NAME_KEY));

    const string name = item.props[WEAPON_NAME_KEY].get_string();

    // For non-artefacts, get the names we gave them.
    if (!full_name)
        return name;

    return it_name + " \"" + name + "\"";
}

void maybe_name_weapon(item_def &item, bool silent)
{
    const bool has_own_name = is_artefact(item);
    const bool new_name = has_own_name
                          || !item.props.exists(WEAPON_NAME_KEY);

    if (new_name && !has_own_name)
        name_weapon(item);

    if (silent)
        return;

    string full_name = get_weapon_name(item, true);

    // TODO: variant messages? (in the database?)
    mprf("You welcome %s%s into your grasp.", full_name.c_str(),
         new_name ? "" : " back");
}

void say_farewell_to_weapon(const item_def &item)
{
    string name = get_weapon_name(item, false);

    // TODO: variant messages? (in the database?)
    mprf("You whisper farewell to %s.", name.c_str());
}

// If there are more than one net on this square
// split off one of them for checking/setting values.
void maybe_split_nets(item_def &item, const coord_def& where)
{
    if (item.quantity == 1)
    {
        set_net_stationary(item);
        return;
    }

    item_def it;

    it.base_type = item.base_type;
    it.sub_type  = item.sub_type;
    it.net_durability      = item.net_durability;
    it.net_placed  = item.net_placed;
    it.flags     = item.flags;
    it.special   = item.special;
    it.quantity  = --item.quantity;
    item_colour(it);

    item.quantity = 1;
    set_net_stationary(item);

    copy_item_to_grid(it, where);
}
