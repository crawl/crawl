/**
 * @file
 * @brief Misc (mostly) inventory related functions.
**/

#include "AppHdr.h"

#include "items.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "areas.h"
#include "arena.h"
#include "artefact.h"
#include "art-enum.h"
#include "beam.h"
#include "bitary.h"
#include "cio.h"
#include "clua.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "dactions.h"
#include "dbg-util.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "dgnevent.h"
#include "directn.h"
#include "dungeon.h"
#include "effects.h"
#include "env.h"
#include "food.h"
#include "godpassive.h"
#include "godprayer.h"
#include "godwrath.h"
#include "hints.h"
#include "hints.h"
#include "hiscores.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "libutil.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-ench.h"
#include "notes.h"
#include "options.h"
#include "orb.h"
#include "output.h"
#include "place.h"
#include "player-equip.h"
#include "player.h"
#include "prompt.h"
#include "quiver.h"
#include "religion.h"
#include "rot.h"
#include "shopping.h"
#include "showsymb.h"
#include "spl-book.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "throw.h"
#include "travel.h"
#include "unwind.h"
#include "viewchar.h"
#include "view.h"
#include "xom.h"

static void _autoinscribe_item(item_def& item);
static void _autoinscribe_floor_items();
static void _autoinscribe_inventory();
static void _multidrop(vector<SelItem> tmp_items);

static bool will_autopickup   = false;
static bool will_autoinscribe = false;

static inline string _autopickup_item_name(const item_def &item)
{
    return userdef_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item, true)
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
            int i = igrd[x][y];

            while (i != NON_ITEM)
            {
                mitm[i].pos.x = x;
                mitm[i].pos.y = y;
                i = mitm[i].link;
            }
        }
}

// This function uses the items coordinates to relink all the igrd lists.
void link_items()
{
    // First, initialise igrd array.
    igrd.init(NON_ITEM);

    // Link all items on the grid, plus shop inventory,
    // but DON'T link the huge pile of monster items at (-2,-2).

    for (int i = 0; i < MAX_ITEMS; i++)
    {
        // Don't mess with monster held items, since the index of the holding
        // monster is stored in the link field.
        if (mitm[i].held_by_monster())
            continue;

        if (!mitm[i].defined())
        {
            // Item is not assigned.  Ignore.
            mitm[i].link = NON_ITEM;
            continue;
        }

        bool move_below = item_is_stationary(mitm[i])
            && !item_is_stationary_net(mitm[i]);
        int movable_ind = -1;
        // Stationary item, find index at location
        if (move_below)
        {

            for (stack_iterator si(mitm[i].pos); si; ++si)
            {
                if (!item_is_stationary(*si) || item_is_stationary_net(*si))
                    movable_ind = si->index();
            }
        }
        // Link to top
        if (!move_below || movable_ind == -1)
        {
            mitm[i].link = igrd(mitm[i].pos);
            igrd(mitm[i].pos) = i;
        }
        // Link below movable items.
        else
        {
            mitm[i].link = mitm[movable_ind].link;
            mitm[movable_ind].link = i;
        }
    }
}

static bool _item_ok_to_clean(int item)
{
    // Never clean food or Orbs.
    if (mitm[item].base_type == OBJ_FOOD || item_is_orb(mitm[item]))
        return false;

    // Never clean runes.
    if (mitm[item].base_type == OBJ_MISCELLANY
        && mitm[item].sub_type == MISC_RUNE_OF_ZOT)
    {
        return false;
    }

    return true;
}

static bool _item_preferred_to_clean(int item)
{
    // Preferably clean "normal" weapons and ammo
    if (mitm[item].base_type == OBJ_WEAPONS
        && mitm[item].plus <= 0
        && !is_artefact(mitm[item]))
    {
        return true;
    }

    if (mitm[item].base_type == OBJ_MISSILES
        && mitm[item].plus <= 0 && !mitm[item].net_placed // XXX: plus...?
        && !is_artefact(mitm[item]))
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
    //  5. never remove food, orbs, runes
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
            if (distance2(you.pos(), *ri) <= dist_range(9))
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
    cur_link = accessible ? you.visible_igrd(pos) : igrd(pos);
    if (cur_link != NON_ITEM)
        next_link = mitm[cur_link].link;
    else
        next_link = NON_ITEM;
}

stack_iterator::stack_iterator(int start_link)
{
    cur_link = start_link;
    if (cur_link != NON_ITEM)
        next_link = mitm[cur_link].link;
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
    return mitm[cur_link];
}

item_def* stack_iterator::operator->() const
{
    ASSERT(cur_link != NON_ITEM);
    return &mitm[cur_link];
}

int stack_iterator::link() const
{
    return cur_link;
}

const stack_iterator& stack_iterator::operator ++ ()
{
    cur_link = next_link;
    if (cur_link != NON_ITEM)
        next_link = mitm[cur_link].link;
    return *this;
}

stack_iterator stack_iterator::operator++(int dummy)
{
    const stack_iterator copy = *this;
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

    if (you.equip[EQ_WEAPON] == obj)
        you.wield_change = true;

    you.m_quiver->on_inv_quantity_changed(obj, amount);

    if (you.inv[obj].quantity <= amount)
    {
        for (int i = 0; i < NUM_EQUIP; i++)
        {
            if (you.equip[i] == obj)
            {
                if (i == EQ_WEAPON)
                {
                    unwield_item();
                    canned_msg(MSG_EMPTY_HANDED_NOW);
                }
                you.equip[i] = -1;
            }
        }

        item_skills(you.inv[obj], you.stop_train);

        you.inv[obj].base_type = OBJ_UNASSIGNED;
        you.inv[obj].quantity  = 0;
        you.inv[obj].props.clear();

        ret = true;

        // If we're repeating a command, the repetitions used up the
        // item stack being repeated on, so stop rather than move onto
        // the next stack.
        crawl_state.cancel_cmd_repeat();
        crawl_state.cancel_cmd_again();
    }
    else
        you.inv[obj].quantity -= amount;

    return ret;
}

// Reduce quantity of a monster/grid item, do cleanup if item goes away.
//
// Returns true if stack of items no longer exists.
bool dec_mitm_item_quantity(int obj, int amount)
{
    item_def &item = mitm[obj];
    if (amount > item.quantity)
        amount = item.quantity; // can't use min due to type mismatch

    // when removing gold from a stack, make it lose its gozag-aura
    if (item.base_type == OBJ_GOLD && item.special > 0 && amount)
    {
        item.special = 0;
        invalidate_agrid(true);
        you.redraw_armour_class = true;
        you.redraw_evasion = true;
    }

    if (item.quantity == amount)
    {
        destroy_item(obj);
        // If we're repeating a command, the repetitions used up the
        // item stack being repeated on, so stop rather than move onto
        // the next stack.
        crawl_state.cancel_cmd_repeat();
        crawl_state.cancel_cmd_again();
        return true;
    }

    item.quantity -= amount;
    return false;
}

void inc_inv_item_quantity(int obj, int amount)
{
    if (you.equip[EQ_WEAPON] == obj)
        you.wield_change = true;

    you.m_quiver->on_inv_quantity_changed(obj, amount);
    you.inv[obj].quantity += amount;
}

void inc_mitm_item_quantity(int obj, int amount)
{
    mitm[obj].quantity += amount;
}

void init_item(int item)
{
    if (item == NON_ITEM)
        return;

    mitm[item].clear();
}

// Returns an unused mitm slot, or NON_ITEM if none available.
// The reserve is the number of item slots to not check.
// Items may be culled if a reserve <= 10 is specified.
int get_mitm_slot(int reserve)
{
    ASSERT(reserve >= 0);

    if (crawl_state.game_is_arena())
        reserve = 0;

    int item = NON_ITEM;

    for (item = 0; item < (MAX_ITEMS - reserve); item++)
        if (!mitm[item].defined())
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
    if (dest == NON_ITEM || !mitm[dest].defined())
        return;

    monster* mons = mitm[dest].holding_monster();

    if (mons != NULL)
    {
        for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
        {
            if (mons->inv[i] == dest)
            {
                mons->inv[i] = NON_ITEM;

                mitm[dest].pos.reset();
                mitm[dest].link = NON_ITEM;
                return;
            }
        }
        mprf(MSGCH_ERROR, "Item %s claims to be held by monster %s, but "
                          "it isn't in the monster's inventory.",
             mitm[dest].name(DESC_PLAIN, false, true).c_str(),
             mons->name(DESC_PLAIN, true).c_str());
        // Don't return so the debugging code can take a look at it.
    }
    // Unlinking a newly created item, or a a temporary one, or an item in
    // the player's inventory.
    else if (mitm[dest].pos.origin() || mitm[dest].pos.equals(-1, -1))
    {
        mitm[dest].pos.reset();
        mitm[dest].link = NON_ITEM;
        return;
    }
    else
    {
        if (!is_shop_item(mitm[dest]))
            ASSERT_IN_BOUNDS(mitm[dest].pos);

        // Linked item on map:
        //
        // Use the items (x,y) to access the list (igrd[x][y]) where
        // the item should be linked.

        // First check the top:
        if (igrd(mitm[dest].pos) == dest)
        {
            // link igrd to the second item
            igrd(mitm[dest].pos) = mitm[dest].link;

            mitm[dest].pos.reset();
            mitm[dest].link = NON_ITEM;
            return;
        }

        // Okay, item is buried, find item that's on top of it.
        for (stack_iterator si(mitm[dest].pos); si; ++si)
        {
            // Find item linking to dest item.
            if (si->defined() && si->link == dest)
            {
                // unlink dest
                si->link = mitm[dest].link;
                mitm[dest].pos.reset();
                mitm[dest].link = NON_ITEM;
                return;
            }
        }
    }

#ifdef DEBUG
    // Okay, the sane ways are gone... let's warn the player:
    mprf(MSGCH_ERROR, "BUG WARNING: Problems unlinking item '%s', (%d, %d)!!!",
         mitm[dest].name(DESC_PLAIN).c_str(),
         mitm[dest].pos.x, mitm[dest].pos.y);

    // Okay, first we scan all items to see if we have something
    // linked to this item.  We're not going to return if we find
    // such a case... instead, since things are already out of
    // alignment, let's assume there might be multiple links as well.
    bool linked = false;
    int  old_link = mitm[dest].link; // used to try linking the first

    // Clean the relevant parts of the object.
    mitm[dest].base_type = OBJ_UNASSIGNED;
    mitm[dest].quantity  = 0;
    mitm[dest].link      = NON_ITEM;
    mitm[dest].pos.reset();
    mitm[dest].props.clear();

    // Look through all items for links to this item.
    for (int c = 0; c < MAX_ITEMS; c++)
    {
        if (mitm[c].defined() && mitm[c].link == dest)
        {
            // unlink item
            mitm[c].link = old_link;

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
            if (igrd[c][cy] == dest)
            {
                igrd[c][cy] = old_link;

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
    if (dest == NON_ITEM || !mitm[dest].defined())
        return;

    unlink_item(dest);
    destroy_item(mitm[dest], never_created);
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
    if (item.props.exists("destroy_xp"))
        gain_exp(item.props["destroy_xp"].get_int());
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
    igrd(where) = NON_ITEM;
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
 * @param[in] obj The location link; an index in mitm.
 * @param exclude_stationary If true, don't include stationary items.
*/
void item_list_on_square(vector<const item_def*>& items, int obj)
{
    for (stack_iterator si(obj); si; ++si)
        items.push_back(& (*si));
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

// 2 - artefact, 1 - glowing/runed, 0 - mundane
static int _item_name_specialness(const item_def& item)
{
    if (item.base_type != OBJ_WEAPONS && item.base_type != OBJ_ARMOUR
        && item.base_type != OBJ_MISSILES && item.base_type != OBJ_JEWELLERY)
    {
        return 0;
    }

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

    if (item_type_known(item))
    {
        if (item_is_branded(item))
            return 1;
        return 0;
    }

    if (item.flags & ISFLAG_COSMETIC_MASK)
        return 1;

    return 0;
}

static void _maybe_give_corpse_hint(const item_def item)
{
    if (!crawl_state.game_is_hints_tutorial())
        return;

    if (item.base_type == OBJ_CORPSES && item.sub_type == CORPSE_BODY
        && you.has_spell(SPELL_ANIMATE_SKELETON))
    {
        learned_something_new(HINT_ANIMATE_CORPSE_SKELETON);
    }
}

void item_check()
{
    describe_floor();
    origin_set(you.pos());

    ostream& strm = msg::streams(MSGCH_FLOOR_ITEMS);

    vector<const item_def*> items;

    item_list_on_square(items, you.visible_igrd(you.pos()));

    if (items.empty())
        return;

    if (items.size() == 1)
    {
        item_def it(*items[0]);
        string name = get_menu_colour_prefix_tags(it, DESC_A);
        strm << "You see here " << name << '.' << endl;
        _maybe_give_corpse_hint(it);
        return;
    }

    bool done_init_line = false;

    if (static_cast<int>(items.size()) >= Options.item_stack_summary_minimum)
    {
        vector<unsigned int> item_chars;
        for (unsigned int i = 0; i < items.size() && i < 50; ++i)
        {
            cglyph_t g = get_item_glyph(items[i]);
            item_chars.push_back(g.ch * 0x100 +
                                 (10 - _item_name_specialness(*(items[i]))));
        }
        sort(item_chars.begin(), item_chars.end());

        string out_string = "Items here: ";
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
        mpr_nojoin(MSGCH_FLOOR_ITEMS, out_string);
        done_init_line = true;
    }

    if (items.size() <= msgwin_lines() - 1)
    {
        if (!done_init_line)
            mpr_nojoin(MSGCH_FLOOR_ITEMS, "Things that are here:");
        for (unsigned int i = 0; i < items.size(); ++i)
        {
            item_def it(*items[i]);
            mprf_nocap("%s", get_menu_colour_prefix_tags(it, DESC_A).c_str());
            _maybe_give_corpse_hint(it);
        }
    }
    else if (!done_init_line)
        strm << "There are many items here." << endl;

    if (items.size() > 2 && crawl_state.game_is_hints_tutorial())
    {
        // If there are 2 or more non-corpse items here, we might need
        // a hint.
        int count = 0;
        for (unsigned int i = 0; i < items.size(); ++i)
        {
            item_def it(*items[i]);
            if (it.base_type == OBJ_CORPSES)
                continue;

            if (++count > 1)
            {
                learned_something_new(HINT_MULTI_PICKUP);
                break;
            }
        }
    }
}

void pickup_menu(int item_link)
{
    int n_did_pickup   = 0;
    int n_tried_pickup = 0;

    vector<const item_def*> items;
    item_list_on_square(items, item_link);
    ASSERT(items.size());

#ifdef TOUCH_UI
    string prompt = "Pick up what? (<Enter> or tap header to pick up)";
#else
    string prompt = "Pick up what? (_ for help)";
#endif
    if (items.size() == 1 && items[0]->quantity > 1)
        prompt = "Select pick up quantity by entering a number, then select the item";
    vector<SelItem> selected = select_items(items, prompt.c_str(), false,
                                            MT_PICKUP);
    if (selected.empty())
        canned_msg(MSG_OK);
    redraw_screen();

    string pickup_warning;
    for (int i = 0, count = selected.size(); i < count; ++i)
    {
        // Moving the item might destroy it, in which case we can't
        // rely on the link.
        short next;
        for (int j = item_link; j != NON_ITEM; j = next)
        {
            next = mitm[j].link;
            if (&mitm[j] == selected[i].item)
            {
                if (j == item_link)
                    item_link = next;

                int num_to_take = selected[i].quantity;
                const bool take_all = (num_to_take == mitm[j].quantity);
                iflags_t oldflags = mitm[j].flags;
                clear_item_pickup_flags(mitm[j]);

                // If we cleared any flags on the items, but the pickup was
                // partial, reset the flags for the items that remain on the
                // floor.
                if (!move_item_to_inv(j, num_to_take))
                {
                    n_tried_pickup++;
                    pickup_warning = "You can't carry that many items.";
                    if (mitm[j].defined())
                        mitm[j].flags = oldflags;
                }
                else
                {
                    n_did_pickup++;
                    // If we deliberately chose to take only part of a
                    // pile, we consider the rest to have been
                    // "dropped."
                    if (!take_all && mitm[j].defined())
                        mitm[j].flags |= ISFLAG_DROPPED;
                }
            }
        }
    }

    if (!pickup_warning.empty())
    {
        mpr(pickup_warning.c_str());
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

void origin_set_inventory(void (*oset)(item_def &item))
{
    for (int i = 0; i < ENDOFPACK; ++i)
        if (you.inv[i].defined())
            oset(you.inv[i]);
}

static string _milestone_rune(const item_def &item)
{
    return string("found ") + item.name(DESC_A) + ".";
}

static void _milestone_check(const item_def &item)
{
    if (item_is_rune(item))
        mark_milestone("rune", _milestone_rune(item));
    else if (item_is_orb(item))
        mark_milestone("orb", "found the Orb of Zot!");
}

static void _check_note_item(item_def &item)
{
    if (item.flags & (ISFLAG_NOTED_GET | ISFLAG_NOTED_ID))
        return;

    if (item_is_rune(item) || item_is_orb(item) || is_artefact(item))
    {
        take_note(Note(NOTE_GET_ITEM, 0, 0, item.name(DESC_A).c_str(),
                       origin_desc(item).c_str()));
        item.flags |= ISFLAG_NOTED_GET;

        // If it's already fully identified when picked up, don't take
        // further notes.
        if (fully_identified(item))
            item.flags |= ISFLAG_NOTED_ID;
        _milestone_check(item);
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

static void _origin_freeze(item_def &item, const coord_def& where)
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
           && item.base_type != OBJ_CORPSES
           && (item.base_type != OBJ_FOOD || item.sub_type != FOOD_CHUNK);
}

static string _article_it(const item_def &item)
{
    // "it" is always correct, since gloves and boots also come in pairs.
    return "it";
}

static bool _origin_is_original_equip(const item_def &item)
{
    return _origin_is_special(item) && item.orig_monnum == -IT_SRC_START;
}

bool origin_is_god_gift(const item_def& item, god_type *god)
{
    god_type ogod = static_cast<god_type>(-item.orig_monnum);
    if (ogod <= GOD_NO_GOD || ogod >= NUM_GODS)
        ogod = GOD_NO_GOD;

    if (god)
        *god = ogod;
    return ogod;
}

bool origin_is_acquirement(const item_def& item, item_source_type *type)
{
    item_source_type junk;
    if (type == NULL)
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

    item_def* item = &mitm[link];
    if (item_is_stationary(mitm[link]))
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
        return mitm[o].link == NON_ITEM && mitm[o].quantity > 1;
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
    else if (you.form == TRAN_ICE_BEAST && grd(you.pos()) == DNGN_DEEP_WATER)
        mpr("You can't reach the bottom while floating on water.");
    // just one movable item?
    else if (num_items == 1)
    {
        // Get the link to the movable item in the pile.
        while (item_is_stationary(mitm[o]))
            o = mitm[o].link;
        pickup_single_item(o, partial_quantity ? 0 : mitm[o].quantity);
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
            next = mitm[o].link;

            if (item_is_stationary(mitm[o]))
            {
                o = next;
                continue;
            }
            any_selectable = true;

            if (keyin != 'a')
            {
                string prompt = "Pick up %s? ((y)es/(n)o/(a)ll/(m)enu/*?g,/q)";

                mprf(MSGCH_PROMPT, prompt.c_str(),
                     get_menu_colour_prefix_tags(mitm[o],
                                                 DESC_A).c_str());

                mouse_control mc(MOUSE_MODE_YESNO);
                keyin = getchk();
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
                int num_to_take = mitm[o].quantity;
                const iflags_t old_flags(mitm[o].flags);
                clear_item_pickup_flags(mitm[o]);

                // attempt to actually pick up the object.
                if (!move_item_to_inv(o, num_to_take))
                {
                    pickup_warning = "You can't carry that many items.";
                    mitm[o].flags = old_flags;
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
            for (stack_iterator si(you.pos(), true); si; si++)
                mprf_nocap("%s", get_menu_colour_prefix_tags(*si, DESC_A).c_str());
        }

        if (!pickup_warning.empty())
            mpr(pickup_warning.c_str());
    }
    if (you.last_pickup.empty())
        you.last_pickup = tmp_l_p;
}

bool is_stackable_item(const item_def &item)
{
    if (!item.defined())
        return false;

    if (item.base_type == OBJ_MISSILES
        || item.base_type == OBJ_FOOD
        || item.base_type == OBJ_SCROLLS
        || item.base_type == OBJ_POTIONS
        || item.base_type == OBJ_GOLD)
    {
        return true;
    }

    if (item.base_type == OBJ_MISCELLANY
        && item.sub_type == MISC_PHANTOM_MIRROR)
    {
        return true;
    }

    return false;
}

bool items_similar(const item_def &item1, const item_def &item2)
{
    // Base and sub-types must always be the same to stack.
    if (item1.base_type != item2.base_type || item1.sub_type != item2.sub_type)
        return false;

    if (item1.base_type == OBJ_GOLD || item_is_rune(item1))
        return true;

    if (is_artefact(item1) != is_artefact(item2))
        return false;
    else if (is_artefact(item1)
             && get_artefact_name(item1) != get_artefact_name(item2))
    {
        return false;
    }

    // Missiles with different egos shouldn't merge.
    if (item1.base_type == OBJ_MISSILES && item1.special != item2.special)
        return false;

    if (item1.base_type == OBJ_FOOD && item2.sub_type == FOOD_CHUNK
        && determine_chunk_effect(item1, true) !=
           determine_chunk_effect(item2, true))
    {
        return false;
    }


#define NO_MERGE_FLAGS (ISFLAG_MIMIC | ISFLAG_SUMMONED)
    if ((item1.flags & NO_MERGE_FLAGS) != (item2.flags & NO_MERGE_FLAGS))
        return false;

    // The inscriptions can differ if one of them is blank, but if they
    // are differing non-blank inscriptions then don't stack.
    if (item1.inscription != item2.inscription
        && !item1.inscription.empty() && !item2.inscription.empty())
    {
        return false;
    }

    return true;
}

bool items_stack(const item_def &item1, const item_def &item2)
{
    // Both items must be stackable.
    if (!is_stackable_item(item1) || !is_stackable_item(item2)
        || static_cast<int>(item1.quantity) + item2.quantity > 32767)
    {
        COMPILE_CHECK(sizeof(item1.quantity) == 2); // can be relaxed otherwise

        return false;
    }

    return items_similar(item1, item2);
}

/**
 * Handles special cases involved in merging a specified number of items from
 * one stack into another.
 * Assumes that it's being called before the destination stack is incremented -
 * bugginess will occur if this order is reversed.
 * DOES NOT modify the original stack - the caller must handle any cleanup!
 *
 * @param source    The source from which items are being drawn.
 * @param dest      The stack into which items are being placed.
 * @param quant     The number of items to be added to the destination stack.
 * Defaults to the entirety of the source stack.
 */
void merge_item_stacks(const item_def &source, item_def &dest, int quant)
{
    if (quant == -1)
        quant = source.quantity;

    ASSERT_RANGE(quant, 0 + 1, source.quantity + 1);

    if (is_perishable_stack(source) && is_perishable_stack(dest))
        merge_perishable_stacks(source, dest, quant);
    if (source.base_type == OBJ_GOLD) // Gozag
        dest.special = max(source.special, dest.special);
}

static int _userdef_find_free_slot(const item_def &i)
{
#ifdef CLUA_BINDINGS
    int slot = -1;
    if (!clua.callfn("c_assign_invletter", "u>d", &i, &slot))
        return -1;

    return slot;
#else
    return -1;
#endif
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
    if (i.base_type == OBJ_FOOD)
        disliked.set('e' - 'a'), disliked.set('y' - 'a');
    else if (i.base_type == OBJ_POTIONS)
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
            if (disliked[slot])
                badslot = slot;
            else
                return slot;

    // If the least preferred slot is the only choice, so be it.
    return badslot;
#undef slotisfree
}

static void _got_item(item_def& item)
{
    seen_item(item);
    shopping_list.cull_identical_items(item);

    if (item.props.exists("needs_autopickup"))
        item.props.erase("needs_autopickup");
}

void get_gold(const item_def& item, int quant, bool quiet)
{
    you.attribute[ATTR_GOLD_FOUND] += quant;

    if (player_under_penance(GOD_GOZAG)
        && item.base_type == OBJ_GOLD) // no taxing Curse of Gozag gold
    {
        int tax = div_rand_round(quant, 10);
        if (tax)
        {
            if (!quiet)
            {
                if (tax >= quant)
                {
                    simple_god_message(" claims this gold.",
                                       GOD_GOZAG);
                }
                else
                {
                    simple_god_message(" claims a portion of the gold.",
                                       GOD_GOZAG);
                }
            }
            int penance = div_rand_round(tax, 10);
            if (penance)
                dec_penance(GOD_GOZAG, penance);

            quant -= tax;
            if (quant <= 0)
                return;
        }
    }

    if (you_worship(GOD_ZIN) && !(item.flags & ISFLAG_THROWN))
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
    _origin_freeze(item, you.pos());
    _check_note_item(item);
}

/**
 * Move the given item and quantity to the player's inventory.
 *
 * @param obj The item index in mitm.
 * @param quant_got The quantity of this item to move.
 * @param quiet If true, most messages notifying the player of item pickup (or
 *              item pickup failure) aren't printed.
 * @return  Whether items were successfully picked up. May return true even on
 * failure in cases where pickup can continue; e.g. when trying to pickup
 * stationary objects, or the orb in zot defense. (I.e., when the cause of
 * failure was not a full inventory.)
*/
bool move_item_to_inv(int obj, int quant_got, bool quiet)
{
    item_def &it = mitm[obj];

    if (item_is_stationary(it))
    {
        if (!quiet)
            mpr("You can't pick that up.");
        // Fake a successful pickup (return 1), so we can continue to
        // pick up anything else that might be on this square.
        return true;
    }

    if (it.base_type == OBJ_ORBS && crawl_state.game_is_zotdef()
        && runes_in_pack() < 15)
    {
        mpr("You must possess at least fifteen runes to touch the sacred Orb which you defend.");
        return true;
    }

    // sanity
    if (quant_got > it.quantity || quant_got <= 0)
        quant_got = it.quantity;

    const coord_def old_item_pos = it.pos;
    // attempt to put the item into your inventory.
    char inv_slot;
    if (merge_items_into_inv(it, quant_got, inv_slot, quiet))
    {
        // if you succeeded, actually reduce the number in the original stack
        if (is_perishable_stack(it) && quant_got != it.quantity)
            for (int i = 0; i < quant_got; i++)
                remove_oldest_perishable_item(it);

        // cleanup items that ended up in an inventory slot (not gold, etc)
        if (inv_slot != -1)
        {
            _got_item(you.inv[inv_slot]);
            _check_note_item(you.inv[inv_slot]);
        }
        else
            _check_note_item(it);

        if (item_is_rune(it) || item_is_orb(it) || in_bounds(old_item_pos))
        {
            dungeon_events.fire_position_event(dgn_event(DET_ITEM_PICKUP,
                                                         you.pos(), 0, obj,
                                                         -1),
                                               you.pos());
        }

        // XXX: Waiting until now to decrement the quantity may give Windows
        // tiles players the opportunity to close the window and duplicate the
        // item (a variant of bug #6486). However, we can't decrement the
        // quantity before firing the position event, because the latter needs
        // the object's index.
        dec_mitm_item_quantity(obj, quant_got);

        you.turn_is_over = true;
        return true;
    }

    return false;
}

/**
 * Place a rune into the player's inventory.
 *
 * @param it      The item (rune) to pick up.
 * @param quiet   Whether to suppress (most?) messages.
 */
static void _get_rune(const item_def& it, bool quiet)
{
    if (!you.runes[it.plus])
        you.runes.set(it.plus);

    if (!quiet)
    {
        flash_view_delay(UA_PICKUP, rune_colour(it.plus), 300);
        mprf("You pick up the %s rune and feel its power.",
             rune_type_name(it.plus));
        int nrunes = runes_in_pack();
        if (nrunes >= you.obtainable_runes)
            mpr("You have collected all the runes! Now go and win!");
        else if (nrunes == NUMBER_OF_RUNES_NEEDED
                 && !crawl_state.game_is_zotdef())
        {
            // might be inappropriate in new Sprints, please change it then
            mprf("%d runes! That's enough to enter the realm of Zot.",
                 nrunes);
        }
        else if (nrunes > 1)
            mprf("You now have %d runes.", nrunes);

        mpr("Press } to see all the runes you have collected.");
    }

    if (it.plus == RUNE_ABYSSAL)
        mpr("You feel the abyssal rune guiding you out of this place.");

    if (it.plus == RUNE_TOMB)
        add_daction(DACT_TOMB_CTELE);

    if (it.plus >= RUNE_DIS && it.plus <= RUNE_TARTARUS)
        unset_level_flags(LFLAG_NO_TELE_CONTROL);
}

/**
 * Place the Orb of Zot into the player's inventory.
 *
 * @param it      The ORB!
 * @param quiet   Unused.
 */
static void _get_orb(const item_def &it, bool quiet)
{
    run_animation(ANIMATION_ORB, UA_PICKUP);

    mprf(MSGCH_ORB, "You pick up the Orb of Zot!");
    you.char_direction = GDT_ASCENDING;

    env.orb_pos = you.pos(); // can be wrong in wizmode
    orb_pickup_noise(you.pos(), 30);

    mprf(MSGCH_WARN, "The lords of Pandemonium are not amused. Beware!");

    if (you_worship(GOD_CHEIBRIADOS))
        simple_god_message(" tells them not to hurry.");

    mprf(MSGCH_ORB, "Now all you have to do is get back out of the dungeon!");

    xom_is_stimulated(200, XM_INTRIGUED);
    invalidate_agrid(true);
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
                                           char &inv_slot, bool quiet)
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

        merge_item_stacks(it, you.inv[inv_slot], quant_got);
        inc_inv_item_quantity(inv_slot, quant_got);
        you.last_pickup[inv_slot] = quant_got;

        if (!quiet)
        {
            mprf_nocap("%s (gained %d)",
                        get_menu_colour_prefix_tags(you.inv[inv_slot],
                                                    DESC_INVENTORY).c_str(),
                        quant_got);
        }

        return true;
    }

    inv_slot = -1;
    return false;
}

/**
 * Move the given item and quantity to a free slot in the player's inventory.
 *
 * @param it[in]          The item to be placed into the player's inventory.
 * @param quant_got       The quantity of this item to place.
 * @param quiet           Suppresses pickup messages.
 * @return                The inventory slot the item was placed in.
 */
static int _place_item_in_free_slot(const item_def &it, int quant_got,
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
    item.pos.set(-1, -1);
    // Remove "unobtainable" as it was just proven false.
    item.flags &= ~ISFLAG_UNOBTAINABLE;

    god_id_item(item);
    if (item.base_type == OBJ_WANDS)
        set_ident_type(item, ID_KNOWN_TYPE);
    maybe_identify_base_type(item);
    if (item.base_type == OBJ_BOOKS)
        maybe_id_book(item, true);

    note_inscribe_item(item);

    // avoid blood potion timer/stack size mismatch
    if (is_perishable_stack(it) && quant_got != it.quantity)
        remove_newest_perishable_item(item);

    if (!quiet)
    {
        mprf_nocap("%s", get_menu_colour_prefix_tags(you.inv[freeslot],
                                                     DESC_INVENTORY).c_str());
    }
    if (crawl_state.game_is_hints())
    {
        taken_new_item(item.base_type);
        if (is_artefact(item) || get_equip_desc(item) != ISFLAG_NO_DESC)
            learned_something_new(HINT_SEEN_RANDART);
    }

    you.m_quiver->on_inv_quantity_changed(freeslot, quant_got);
    you.last_pickup[item.link] = quant_got;
    item_skills(item, you.start_train);

    return freeslot;
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
bool merge_items_into_inv(const item_def &it, int quant_got, char &inv_slot,
                          bool quiet)
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

    // Runes are also massless.
    if (item_is_rune(it))
    {
        _get_rune(it, quiet);
        return true;
    }
    // The Orb is also handled specially.
    if (item_is_orb(it))
    {
        _get_orb(it, quiet);
        return true;
    }

    // BEWARE... THE CURSE OF GOZAG!
    if (player_under_penance(GOD_GOZAG))
    {
        const int goldified_count = gozag_goldify(it, quant_got, quiet);
        quant_got -= goldified_count;
        if (quant_got <= 0)
            return true;
    }

    // attempt to merge into an existing stack, if possible
    if (is_stackable_item(it)
        && _merge_stackable_item_into_inv(it, quant_got, inv_slot, quiet))
    {
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
    int item = igrd(pos);
    while (item != NON_ITEM)
    {
        mitm[item].flags |= ISFLAG_DROPPED;
        mitm[item].flags &= ~ISFLAG_THROWN;
        item = mitm[item].link;
    }
}

void mark_items_non_visit_at(const coord_def &pos)
{
    int item = igrd(pos);
    while (item != NON_ITEM)
    {
        if (god_likes_item(you.religion, mitm[item]))
            mitm[item].flags |= ISFLAG_DROPPED;
        item = mitm[item].link;
    }
}

void clear_item_pickup_flags(item_def &item)
{
    item.flags &= ~(ISFLAG_THROWN | ISFLAG_DROPPED | ISFLAG_NO_PICKUP);
}

// Move gold to the the top of a pile if following Gozag.
static void _gozag_move_gold_to_top(const coord_def p)
{
    if (you_worship(GOD_GOZAG))
    {
        for (int gold = igrd(p); gold != NON_ITEM;
             gold = mitm[gold].link)
        {
            if (mitm[gold].base_type == OBJ_GOLD)
            {
                unlink_item(gold);
                move_item_to_grid(&gold, p, true);
                break;
            }
        }
    }
}

// Moves mitm[obj] to p... will modify the value of obj to
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
    if (ob == NON_ITEM || !mitm[ob].defined())
        return false;

    item_def& item(mitm[ob]);
    bool move_below = item_is_stationary(item) && !item_is_stationary_net(item);

    if (feat_destroys_item(grd(p), mitm[ob], !silenced(p) && !silent))
    {
        item_was_destroyed(item);
        destroy_item(ob);
        ob = NON_ITEM;

        return true;
    }

    if (you.see_cell(p))
    {
        god_id_item(item);
        maybe_identify_base_type(item);
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
                merge_item_stacks(item, *si);
                inc_mitm_item_quantity(si->index(), item.quantity);
                destroy_item(ob);
                ob = si->index();
                _gozag_move_gold_to_top(p);
                return true;
            }
            if (move_below
                && (!item_is_stationary(*si) || item_is_stationary_net(*si)))
            {
                movable_ind = si->index();
            }
        }
    }
    // Non-stackable item that's been fudge-stacked (monster throwing weapons).
    // Explode the stack when dropped. We have to special case chunks, ew.
    else if (item.quantity > 1
             && (item.base_type != OBJ_FOOD
                 || item.sub_type != FOOD_CHUNK))
    {
        while (item.quantity > 1)
        {
            // If we can't copy the items out, we lose the surplus.
            if (copy_item_to_grid(item, p, 1, false, true))
                --item.quantity;
            else
                item.quantity = 1;
        }
    }

    ASSERT(ob != NON_ITEM);

    // Need to actually move object, so first unlink from old position.
    unlink_item(ob);

    // Move item to coord.
    item.pos = p;

    // Need to move this stationary item to the position in the pile
    // below the lowest non-stationary, non-net item.
    if (move_below && movable_ind >= 0)
    {
        item.link = mitm[movable_ind].link;
        mitm[movable_ind].link = item.index();
    }
    // Movable item or no movable items in pile, link item to top of list.
    else
    {
        item.link = igrd(p);
        igrd(p) = ob;
    }

    if (item_is_orb(item))
        env.orb_pos = p;

    if (item.base_type != OBJ_GOLD)
        _gozag_move_gold_to_top(p);

    return true;
}

void move_item_stack_to_grid(const coord_def& from, const coord_def& to)
{
    if (igrd(from) == NON_ITEM)
        return;

    // Tell all items in stack what the new coordinate is.
    for (stack_iterator si(from); si; ++si)
    {
        si->pos = to;

        // Link last of the stack to the top of the old stack.
        if (si->link == NON_ITEM && igrd(to) != NON_ITEM)
        {
            si->link = igrd(to);
            break;
        }
    }

    igrd(to) = igrd(from);
    igrd(from) = NON_ITEM;
}

// Returns false if no items could be dropped.
bool copy_item_to_grid(const item_def &item, const coord_def& p,
                        int quant_drop, bool mark_dropped, bool silent)
{
    ASSERT_IN_BOUNDS(p);

    if (quant_drop == 0)
        return false;

    if (feat_destroys_item(grd(p), item, !silenced(p) && !silent))
    {
        if (item_is_spellbook(item))
            destroy_spellbook(item);

        item_was_destroyed(item);

        return true;
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
                merge_item_stacks(copy, *si, quant_drop);
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
                return true;
            }
        }
    }

    // Item not found in current stack, add new item to top.
    int new_item_idx = get_mitm_slot(10);
    if (new_item_idx == NON_ITEM)
        return false;
    item_def& new_item = mitm[new_item_idx];

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
    if (is_perishable_stack(item) && item.quantity != quant_drop)
    {
        // In the case of a partial drop, since only the oldest items have
        // been dropped, remove the newest ones.
        remove_newest_perishable_item(new_item);
    }

    return true;
}

coord_def item_pos(const item_def &item)
{
    coord_def pos = item.pos;
    if (pos.equals(-2, -2))
        if (const monster *mon = item.holding_monster())
            pos = mon->pos();
        else
            die("item held by an invalid monster");
    else if (pos.equals(-1, -1))
        pos = you.pos();
    return pos;
}

//---------------------------------------------------------------
//
// move_top_item -- moves the top item of a stack to a new
// location.
//
//---------------------------------------------------------------
bool move_top_item(const coord_def &pos, const coord_def &dest)
{
    int item = igrd(pos);
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
    return (link == NON_ITEM) ? NULL : &mitm[link];
}

bool multiple_items_at(const coord_def& where)
{
    int found_count = 0;

    for (stack_iterator si(where); si && found_count < 2; ++si)
        ++found_count;

    return found_count > 1;
}

bool drop_item(int item_dropped, int quant_drop)
{
    if (quant_drop < 0 || quant_drop > you.inv[item_dropped].quantity)
        quant_drop = you.inv[item_dropped].quantity;

    if (item_dropped == you.equip[EQ_LEFT_RING]
     || item_dropped == you.equip[EQ_RIGHT_RING]
     || item_dropped == you.equip[EQ_AMULET]
     || item_dropped == you.equip[EQ_RING_ONE]
     || item_dropped == you.equip[EQ_RING_TWO]
     || item_dropped == you.equip[EQ_RING_THREE]
     || item_dropped == you.equip[EQ_RING_FOUR]
     || item_dropped == you.equip[EQ_RING_FIVE]
     || item_dropped == you.equip[EQ_RING_SIX]
     || item_dropped == you.equip[EQ_RING_SEVEN]
     || item_dropped == you.equip[EQ_RING_EIGHT]
     || item_dropped == you.equip[EQ_RING_AMULET])
    {
        if (!Options.easy_unequip)
        {
            mpr("You will have to take that off first.");
            return false;
        }

        if (remove_ring(item_dropped, true))
            start_delay(DELAY_DROP_ITEM, 1, item_dropped, 1);

        return false;
    }

    if (item_dropped == you.equip[EQ_WEAPON]
        && you.inv[item_dropped].base_type == OBJ_WEAPONS
        && you.inv[item_dropped].cursed())
    {
        mprf("%s is stuck to you!", you.inv[item_dropped].name(DESC_THE).c_str());
        return false;
    }

    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_ARMOUR; i++)
    {
        if (item_dropped == you.equip[i] && you.equip[i] != -1)
        {
            if (!Options.easy_unequip)
                mpr("You will have to take that off first.");
            else if (check_warning_inscriptions(you.inv[item_dropped],
                                                OPER_TAKEOFF))
            {
                // If we take off the item, cue up the item being dropped
                if (takeoff_armour(item_dropped))
                    start_delay(DELAY_DROP_ITEM, 1, item_dropped, 1);
            }

            // Regardless, we want to return here because either we're
            // aborting the drop, or the drop is delayed until after
            // the armour is removed. -- bwr
            return false;
        }
    }

    // [ds] easy_unequip does not apply to weapons.
    //
    // Unwield needs to be done before copy in order to clear things
    // like temporary brands. -- bwr
    if (item_dropped == you.equip[EQ_WEAPON]
        && quant_drop >= you.inv[item_dropped].quantity)
    {
        int old_time = you.time_taken;
        if (!wield_weapon(true, SLOT_BARE_HANDS))
            return false;
        // Dropping a wielded item isn't any faster.
        you.time_taken = old_time;
    }

    const dungeon_feature_type my_grid = grd(you.pos());

    if (!copy_item_to_grid(you.inv[item_dropped],
                            you.pos(), quant_drop, true, true))
    {
        mpr("Too many items on this level, not dropping the item.");
        return false;
    }

    mprf("You drop %s.",
         quant_name(you.inv[item_dropped], quant_drop, DESC_A).c_str());

    bool quiet = silenced(you.pos());

    // If you drop an item in as a merfolk, it is below the water line and
    // makes no noise falling.
    if (you.swimming())
        quiet = true;

    feat_destroys_item(my_grid, you.inv[item_dropped], !quiet);

    if (is_perishable_stack(you.inv[item_dropped])
        && you.inv[item_dropped].quantity != quant_drop)
    {
        // Oldest potions have been dropped.
        for (int i = 0; i < quant_drop; i++)
            remove_oldest_perishable_item(you.inv[item_dropped]);
    }

    dec_inv_item_quantity(item_dropped, quant_drop);
    you.turn_is_over = true;
    you.time_taken = div_rand_round(you.time_taken * 8, 10);

    you.last_pickup.erase(item_dropped);

    return true;
}

void drop_last()
{
    vector<SelItem> items_to_drop;

    for (map<int,int>::iterator it = you.last_pickup.begin();
        it != you.last_pickup.end(); ++it)
    {
        const item_def* item = &you.inv[it->first];
        if (item->quantity > 0)
            items_to_drop.push_back(SelItem(it->first, it->second, item));
    }

    if (items_to_drop.empty())
        mpr("No item to drop.");
    else
    {
        you.last_pickup.clear();
        _multidrop(items_to_drop);
    }
}

int get_equip_slot(const item_def *item)
{
    int worn = -1;
    if (item && in_inventory(*item))
    {
        for (int i = 0; i < NUM_EQUIP; ++i)
        {
            if (you.equip[i] == item->link)
            {
                worn = i;
                break;
            }
        }
    }
    return worn;
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

static string _drop_selitem_text(const vector<MenuEntry*> *s)
{
    bool extraturns = false;

    if (s->empty())
        return "";

    for (int i = 0, size = s->size(); i < size; ++i)
    {
        const item_def *item = static_cast<item_def *>((*s)[i]->data);
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

vector<SelItem> items_for_multidrop;

// Arrange items that have been selected for multidrop so that
// equipped items are dropped after other items, and equipped items
// are dropped in the same order as their EQ_ slots are numbered.
static bool _drop_item_order(const SelItem &first, const SelItem &second)
{
    const item_def &i1 = you.inv[first.slot];
    const item_def &i2 = you.inv[second.slot];

    const int slot1 = get_equip_slot(&i1),
              slot2 = get_equip_slot(&i2);

    if (slot1 != -1 && slot2 != -1)
        return slot1 < slot2;
    else if (slot1 != -1 && slot2 == -1)
        return false;
    else if (slot2 != -1 && slot1 == -1)
        return true;

    return first.slot < second.slot;
}

//---------------------------------------------------------------
//
// drop
//
// Prompts the user for an item to drop
//
//---------------------------------------------------------------
void drop()
{
    if (inv_count() < 1 && you.gold == 0)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    vector<SelItem> tmp_items;
#ifdef TOUCH_UI
    tmp_items = prompt_invent_items("Drop what? (<Enter> or tap header to drop)",
                                    MT_DROP,
#else
    tmp_items = prompt_invent_items("Drop what? (_ for help)", MT_DROP,
#endif
                                     -1, NULL, true, true, 0,
                                     &Options.drop_filter, _drop_selitem_text,
                                     &items_for_multidrop);

    if (tmp_items.empty())
    {
        canned_msg(MSG_OK);
        return;
    }

    _multidrop(tmp_items);
}

static void _multidrop(vector<SelItem> tmp_items)
{
    // Sort the dropped items so we don't see weird behaviour when
    // dropping a worn robe before a cloak (old behaviour: remove
    // cloak, remove robe, wear cloak, drop robe, remove cloak, drop
    // cloak).
    sort(tmp_items.begin(), tmp_items.end(), _drop_item_order);

    // If the user answers "no" to an item an with a warning inscription,
    // then remove it from the list of items to drop by not copying it
    // over to items_for_multidrop.
    items_for_multidrop.clear();
    for (unsigned int i = 0; i < tmp_items.size(); ++i)
    {
        SelItem& si(tmp_items[i]);

        const int item_quant = si.item->quantity;

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
        start_delay(DELAY_MULTIDROP, items_for_multidrop.size());
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

    for (unsigned i = 0; i < Options.autoinscriptions.size(); ++i)
    {
        if (Options.autoinscriptions[i].first.matches(iname))
        {
            // Don't autoinscribe dropped items on ground with
            // "=g".  If the item matches a rule which adds "=g",
            // "=g" got added to it before it was dropped, and
            // then the user explicitly removed it because they
            // don't want to autopickup it again.
            string str = Options.autoinscriptions[i].second;
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
    for (int i = 0; i < ENDOFPACK; i++)
        if (you.inv[i].defined())
            _autoinscribe_item(you.inv[i]);
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

static int _autopickup_subtype(const item_def &item)
{
    // Sensed items.
    if (item.base_type >= NUM_OBJECT_CLASSES)
        return MAX_SUBTYPES - 1;

    const int max_type = get_max_subtype(item.base_type);

    // item_infos of unknown subtype.
    if (max_type > 0 && item.sub_type >= max_type)
        return max_type;

    // Only where item_type_known() refers to the subtype (as opposed to the
    // brand, for example) do we have to check it.  For weapons etc. we always
    // know the subtype.
    switch (item.base_type)
    {
    case OBJ_WANDS:
    case OBJ_SCROLLS:
    case OBJ_JEWELLERY:
    case OBJ_POTIONS:
    case OBJ_STAVES:
        return item_type_known(item) ? item.sub_type : max_type;
    case OBJ_MISCELLANY:
        return (item.sub_type == MISC_RUNE_OF_ZOT
                || item.sub_type == MISC_PHANTOM_MIRROR)
               ? item.sub_type : max_type;
    case OBJ_BOOKS:
        if (item.sub_type == BOOK_MANUAL || item_type_known(item))
            return item.sub_type;
        else
            return max_type;
    case OBJ_RODS:
    case OBJ_GOLD:
        return max_type;
    default:
        return item.sub_type;
    }
}

static bool _is_option_autopickup(const item_def &item)
{
    string iname = _autopickup_item_name(item);

    if (item.base_type < NUM_OBJECT_CLASSES)
    {
        const int force = you.force_autopickup[item.base_type][_autopickup_subtype(item)];
        if (force != 0)
            return force == 1;
    }
    else
        return false;

#ifdef CLUA_BINDINGS
    bool res = clua.callbooleanfn(false, "ch_force_autopickup", "is",
                                        &item, iname.c_str());
    if (!clua.error.empty())
    {
        mprf(MSGCH_ERROR, "ch_force_autopickup failed: %s",
             clua.error.c_str());
    }

    if (res)
        return true;

    res = clua.callbooleanfn(false, "ch_deny_autopickup", "is",
                             &item, iname.c_str());
    if (!clua.error.empty())
    {
        mprf(MSGCH_ERROR, "ch_deny_autopickup failed: %s",
             clua.error.c_str());
    }

    if (res)
        return false;
#endif

    //Check for initial settings
    for (int i = 0; i < (int)Options.force_autopickup.size(); ++i)
        if (Options.force_autopickup[i].first.matches(iname))
            return Options.force_autopickup[i].second;

    return Options.autopickups[item.base_type];
}

bool item_needs_autopickup(const item_def &item)
{
    if (item_is_stationary(item))
        return false;

    if (item.inscription.find("=g") != string::npos)
        return true;

    if ((item.flags & ISFLAG_THROWN) && Options.pickup_thrown)
        return true;

    if (item.flags & ISFLAG_DROPPED)
        return false;

    if (item.props.exists("needs_autopickup"))
        return true;

    return _is_option_autopickup(item);
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
    return pickup_item.base_type == inv_item.base_type
           && pickup_item.sub_type == inv_item.sub_type;
}

static bool _edible_food(const item_def& pickup_item,
                         const item_def& inv_item)
{
    return inv_item.base_type == OBJ_FOOD && !is_inedible(inv_item);
}

static bool _similar_equip(const item_def& pickup_item,
                           const item_def& inv_item)
{
    const equipment_type inv_slot = get_item_slot(inv_item);

    if (inv_slot == EQ_NONE)
        return false;

    // If it's an unequipped cursed item the player might be looking
    // for a replacement.
    if (item_known_cursed(inv_item) && !item_is_equipped(inv_item))
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

    // Not similar if wand in inventory is known to be empty.
    return !is_known_empty_wand(inv_item);
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
    return !item_type_known(inv_item)
           || (!jewellery_is_amulet(inv_item)
               && !ring_has_stackable_effect(inv_item));
}

static bool _item_different_than_inv(const item_def& pickup_item,
                                     item_comparer comparer)
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        const item_def& inv_item(you.inv[i]);

        if (!inv_item.defined())
            continue;

        if ((*comparer)(pickup_item, inv_item))
            return false;
    }

    return true;
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

        for (unsigned int i = 0; i < ignores.size(); i++)
            if (ignores[i].matches(name))
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
    if (!item_type_known(item) & (item.flags & ISFLAG_COSMETIC_MASK))
        return true;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_MISSILES:
    case OBJ_ARMOUR:
        // Ego items.
        if (item.special != 0)
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

    case OBJ_FOOD:
        if (you_worship(GOD_FEDHAS) && is_fruit(item))
            return true;

        if (is_inedible(item))
            return false;

        // Interesting if we don't have any other edible food.
        return _item_different_than_inv(item, _edible_food);

    case OBJ_RODS:
        // Rods are always interesting, even if you already have one of
        // the same type, since each rod has its own mana.
        return true;

        // Intentional fall-through.
    case OBJ_MISCELLANY:
        // Runes are always interesting.
        if (item_is_rune(item))
            return true;

        // Decks always start out unidentified.
        if (is_deck(item))
            return true;

        // Intentional fall-through.
    case OBJ_SCROLLS:
    case OBJ_POTIONS:
    case OBJ_STAVES:
        // Item is boring only if there's an identical one in inventory.
        return _item_different_than_inv(item, _identical_types);

    case OBJ_BOOKS:
        // Books always start out unidentified.
        return true;

    case OBJ_ORBS:
        // Orb is always interesting.
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

    will_autopickup = false;

    if (!can_autopickup())
    {
        item_check();
        return;
    }

    // Store last_pickup in case we need to restore it.
    // Then clear it to fill with items picked up.
    map<int,int> tmp_l_p = you.last_pickup;
    you.last_pickup.clear();

    int o = you.visible_igrd(you.pos());

    string pickup_warning;
    while (o != NON_ITEM)
    {
        const int next = mitm[o].link;

        if (item_needs_autopickup(mitm[o]))
        {
            // Do this before it's picked up, otherwise the picked up
            // item will be in inventory and _interesting_explore_pickup()
            // will always return false.
            const bool interesting_pickup
                = _interesting_explore_pickup(mitm[o]);

            const iflags_t iflags(mitm[o].flags);
            if ((iflags & ISFLAG_THROWN))
                learned_something_new(HINT_AUTOPICKUP_THROWN);

            clear_item_pickup_flags(mitm[o]);

            const bool pickup_result = move_item_to_inv(o, mitm[o].quantity);
            if (mitm[o].base_type == OBJ_FOOD
                && mitm[o].sub_type == FOOD_CHUNK)
            {
                mitm[o].flags |= ISFLAG_DROPPED;
            }

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
                mitm[o].flags = iflags;
            }
        }

        o = next;
    }

    if (!pickup_warning.empty())
        mpr(pickup_warning.c_str());

    if (did_pickup)
        you.turn_is_over = true;

    if (you.last_pickup.empty())
        you.last_pickup = tmp_l_p;

    item_check();

    explore_pickup_event(n_did_pickup, n_tried_pickup);
}

void autopickup()
{
    _autoinscribe_floor_items();
    _do_autopickup();
}

int inv_count()
{
    int count = 0;

    for (int i = 0; i < ENDOFPACK; i++)
        if (you.inv[i].defined())
            count++;

    return count;
}

item_def *find_floor_item(object_class_type cls, int sub_type)
{
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
            for (stack_iterator si(coord_def(x,y)); si; ++si)
                if (si->defined()
                    && si->base_type == cls && si->sub_type == sub_type
                    && !(si->flags & ISFLAG_MIMIC))
                {
                    return &*si;
                }

    return NULL;
}

int item_on_floor(const item_def &item, const coord_def& where)
{
    // Check if the item is on the floor and reachable.
    for (int link = igrd(where); link != NON_ITEM; link = mitm[link].link)
        if (&mitm[link] == &item)
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
        NUM_FOODS,
        NUM_SCROLLS,
        NUM_JEWELLERY,
        NUM_POTIONS,
        NUM_BOOKS,
        NUM_STAVES,
        1,              // Orbs         -- only one
        NUM_MISCELLANY,
        -1,              // corpses     -- handled specially
        1,              // gold         -- handled specially
        NUM_RODS,
    };
    COMPILE_CHECK(ARRAYSZ(max_subtype) == NUM_OBJECT_CLASSES);

    ASSERT_RANGE(base_type, 0, NUM_OBJECT_CLASSES);

    return max_subtype[base_type];
}

equipment_type item_equip_slot(const item_def& item)
{
    if (!in_inventory(item))
        return EQ_NONE;

    for (int i = 0; i < NUM_EQUIP; i++)
        if (item.link == you.equip[i])
            return static_cast<equipment_type>(i);

    return EQ_NONE;
}

// Includes melded items.
bool item_is_equipped(const item_def &item, bool quiver_too)
{
    return item_equip_slot(item) != EQ_NONE
           || quiver_too && item_is_quivered(item);
}

bool item_is_melded(const item_def& item)
{
    equipment_type eq = item_equip_slot(item);
    return eq != EQ_NONE && you.melded[eq];
}

////////////////////////////////////////////////////////////////////////
// item_def functions.

bool item_def::has_spells() const
{
    return (item_is_spellbook(*this) || base_type == OBJ_RODS)
           && item_type_known(*this);
}

int item_def::book_number() const
{
    return base_type == OBJ_BOOKS ? sub_type
                                  : -1;
}

bool item_def::cursed() const
{
    return flags & ISFLAG_CURSED;
}

bool item_def::launched_by(const item_def &launcher) const
{
    if (base_type != OBJ_MISSILES)
        return false;
    const missile_type mt = fires_ammo_type(launcher);
    return sub_type == mt || (mt == MI_STONE && sub_type == MI_SLING_BULLET);
}

zap_type item_def::zap() const
{
    if (base_type != OBJ_WANDS)
        return ZAP_DEBUGGING_RAY;

    zap_type result = ZAP_DEBUGGING_RAY;
    wand_type wand_sub_type = static_cast<wand_type>(sub_type);

    if (wand_sub_type == WAND_RANDOM_EFFECTS)
    {
        while (wand_sub_type == WAND_RANDOM_EFFECTS
               || wand_sub_type == WAND_HEAL_WOUNDS)
        {
            wand_sub_type = static_cast<wand_type>(random2(NUM_WANDS));
        }
    }

    switch (wand_sub_type)
    {
    case WAND_FLAME:           result = ZAP_THROW_FLAME;     break;
    case WAND_FROST:           result = ZAP_THROW_FROST;     break;
    case WAND_SLOWING:         result = ZAP_SLOW;            break;
    case WAND_HASTING:         result = ZAP_HASTE;           break;
    case WAND_MAGIC_DARTS:     result = ZAP_MAGIC_DART;      break;
    case WAND_HEAL_WOUNDS:     result = ZAP_MAJOR_HEALING;   break;
    case WAND_PARALYSIS:       result = ZAP_PARALYSE;        break;
    case WAND_FIRE:            result = ZAP_BOLT_OF_FIRE;    break;
    case WAND_COLD:            result = ZAP_BOLT_OF_COLD;    break;
    case WAND_CONFUSION:       result = ZAP_CONFUSE;         break;
    case WAND_INVISIBILITY:    result = ZAP_INVISIBILITY;    break;
    case WAND_DIGGING:         result = ZAP_DIG;             break;
    case WAND_FIREBALL:        result = ZAP_FIREBALL;        break;
    case WAND_TELEPORTATION:   result = ZAP_TELEPORT_OTHER;  break;
    case WAND_LIGHTNING:       result = ZAP_LIGHTNING_BOLT;  break;
    case WAND_POLYMORPH:       result = ZAP_POLYMORPH;       break;
    case WAND_ENSLAVEMENT:     result = ZAP_ENSLAVEMENT;     break;
    case WAND_DRAINING:        result = ZAP_BOLT_OF_DRAINING; break;
    case WAND_DISINTEGRATION:  result = ZAP_DISINTEGRATE;    break;
    case WAND_RANDOM_EFFECTS:  /* impossible */
    case NUM_WANDS: break;
    }
    return result;
}

int item_def::index() const
{
    return this - mitm.buffer();
}

int item_def::armour_rating() const
{
    if (!defined() || base_type != OBJ_ARMOUR)
        return 0;

    return property(*this, PARM_AC) + plus;
}

monster* item_def::holding_monster() const
{
    if (!pos.equals(-2, -2))
        return NULL;
    const int midx = link - NON_ITEM - 1;
    if (invalid_monster_index(midx))
        return NULL;

    return &menv[midx];
}

void item_def::set_holding_monster(int midx)
{
    ASSERT(midx != NON_MONSTER);
    pos.set(-2, -2);
    link = NON_ITEM + 1 + midx;
}

bool item_def::held_by_monster() const
{
    return pos.equals(-2, -2) && !invalid_monster_index(link - NON_ITEM - 1);
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
        case SK_BOWS:
            return BLUE;
        case SK_CROSSBOWS:
            return LIGHTBLUE;
        case SK_THROWING:
            return WHITE;
        case SK_SLINGS:
            return BROWN;
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

    // TODO: move this into itemprop.cc
    switch (sub_type)
    {
        case MI_STONE:
            return BROWN;
#if TAG_MAJOR_VERSION == 34
        case MI_DART:
#endif
        case MI_SLING_BULLET:
            return CYAN;
        case MI_LARGE_ROCK:
            return LIGHTGREY;
        case MI_ARROW:
            return BLUE;
        case MI_NEEDLE:
            return WHITE;
        case MI_BOLT:
            return LIGHTBLUE;
        case MI_JAVELIN:
            return RED;
        case MI_THROWING_NET:
            return MAGENTA;
        case MI_TOMAHAWK:
            return GREEN;
        case NUM_SPECIAL_MISSILES:
        case NUM_REAL_SPECIAL_MISSILES:
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

    // TODO: move (some of?) this into itemprop.cc
    switch (sub_type)
    {
        case ARM_FIRE_DRAGON_HIDE:
        case ARM_FIRE_DRAGON_ARMOUR:
            return mons_class_colour(MONS_FIRE_DRAGON);
        case ARM_TROLL_HIDE:
        case ARM_TROLL_LEATHER_ARMOUR:
            return mons_class_colour(MONS_TROLL);
        case ARM_ICE_DRAGON_HIDE:
        case ARM_ICE_DRAGON_ARMOUR:
            return mons_class_colour(MONS_ICE_DRAGON);
        case ARM_STEAM_DRAGON_HIDE:
        case ARM_STEAM_DRAGON_ARMOUR:
            return mons_class_colour(MONS_STEAM_DRAGON);
        case ARM_MOTTLED_DRAGON_HIDE:
        case ARM_MOTTLED_DRAGON_ARMOUR:
            return mons_class_colour(MONS_MOTTLED_DRAGON);
        case ARM_STORM_DRAGON_HIDE:
        case ARM_STORM_DRAGON_ARMOUR:
            return mons_class_colour(MONS_STORM_DRAGON);
        case ARM_GOLD_DRAGON_HIDE:
        case ARM_GOLD_DRAGON_ARMOUR:
            return mons_class_colour(MONS_GOLDEN_DRAGON);
        case ARM_SWAMP_DRAGON_HIDE:
        case ARM_SWAMP_DRAGON_ARMOUR:
            return mons_class_colour(MONS_SWAMP_DRAGON);
        case ARM_PEARL_DRAGON_HIDE:
        case ARM_PEARL_DRAGON_ARMOUR:
            return mons_class_colour(MONS_PEARL_DRAGON);
        case ARM_SHADOW_DRAGON_HIDE:
        case ARM_SHADOW_DRAGON_ARMOUR:
            return mons_class_colour(MONS_SHADOW_DRAGON);
        case ARM_QUICKSILVER_DRAGON_HIDE:
        case ARM_QUICKSILVER_DRAGON_ARMOUR:
            return mons_class_colour(MONS_QUICKSILVER_DRAGON);
        case ARM_CLOAK:
            return WHITE;
        case ARM_NAGA_BARDING:
        case ARM_CENTAUR_BARDING:
            return GREEN;
        case ARM_ROBE:
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
        case ARM_ANIMAL_SKIN:
            return LIGHTGREY;
        case ARM_CRYSTAL_PLATE_ARMOUR:
            return WHITE;
        case ARM_SHIELD:
        case ARM_LARGE_SHIELD:
        case ARM_BUCKLER:
            return CYAN;
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
    switch (special % NDSC_WAND_PRI)
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
 * Assuming this item is a piece of food, what colour is it?
 */
colour_t item_def::food_colour() const
{
    ASSERT(base_type == OBJ_FOOD);

    switch (sub_type)
    {
        case FOOD_ROYAL_JELLY:
        case FOOD_PIZZA:
            return YELLOW;
        case FOOD_FRUIT:
            return LIGHTGREEN;
        case FOOD_CHUNK:
        {
            const colour_t class_colour = mons_class_colour(mon_type);
            if (class_colour == BLACK)
                return LIGHTRED;
            return class_colour;
        }
        case FOOD_BEEF_JERKY:
        case FOOD_BREAD_RATION:
        case FOOD_MEAT_RATION:
        default:
            return BROWN;
    }
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
        case 0:             // "zirconium amulet"
        case 9:             // "ivory amulet"
        case 10:            // "bone amulet"
        case 11:            // "platinum amulet"
        case 16:            // "pearl amulet"
        case 20:            // "diamond amulet"
        case 24:            // "silver amulet"
            return LIGHTGREY;
        case 1:             // "sapphire amulet"
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
        case 23:            // "cabochon amulet"
        case 27:            // "filigree amulet"
            return CYAN;
        case 13:            // "fluorescent amulet"
        case 14:            // "crystal amulet"
            return MAGENTA;
        case 2:             // "golden amulet"
        case 5:             // "bronze amulet"
        case 6:             // "brass amulet"
        case 7:             // "copper amulet"
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
    ASSERT(item_is_rune(*this));

    switch (rune_enum)
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

            // This one is hardly unique, but colour isn't used for
            // stacking, so we don't have to worry too much about this.
            // - bwr
        case RUNE_DEMONIC:                  // random Pandemonium lords
        {
            static const element_type types[] =
            {ETC_EARTH, ETC_ELECTRICITY, ETC_ENCHANT, ETC_HEAL, ETC_BLOOD,
             ETC_DEATH, ETC_UNHOLY, ETC_VEHUMET, ETC_BEOGH, ETC_CRYSTAL,
             ETC_SMOKE, ETC_DWARVEN, ETC_ORCISH, ETC_FLASH, ETC_KRAKEN};

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
 * Assuming this item is a miscellaneous item (evocations item or a rune), what
 * colour is it?
 */
colour_t item_def::miscellany_colour() const
{
    ASSERT(base_type == OBJ_MISCELLANY);

    if (is_deck(*this) || sub_type == MISC_DECK_UNKNOWN)
        return deck_rarity_to_colour(deck_rarity);

    if (item_is_rune(*this))
        return rune_colour();

    switch (sub_type)
    {
        case MISC_LANTERN_OF_SHADOWS:
            return BLUE;
        case MISC_FAN_OF_GALES:
            return CYAN;
#if TAG_MAJOR_VERSION == 34
        case MISC_BOTTLED_EFREET:
            return RED;
#endif
        case MISC_PHANTOM_MIRROR:
            return RED;
        case MISC_STONE_OF_TREMORS:
            return BROWN;
        case MISC_DISC_OF_STORMS:
            return LIGHTGREY;
        case MISC_PHIAL_OF_FLOODS:
            return LIGHTBLUE;
        case MISC_BOX_OF_BEASTS:
            return LIGHTGREEN; // ugh, but we're out of other options
        case MISC_CRYSTAL_BALL_OF_ENERGY:
            return LIGHTCYAN;
        case MISC_HORN_OF_GERYON:
            return LIGHTRED;
        case MISC_LAMP_OF_FIRE:
            return YELLOW;
        case MISC_SACK_OF_SPIDERS:
            return WHITE;
#if TAG_MAJOR_VERSION == 34
        case MISC_BUGGY_EBONY_CASKET:
            return DARKGREY;
#endif
        case MISC_QUAD_DAMAGE:
            return ETC_DARK;
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
            if (class_colour == BLACK)
                return LIGHTRED;
#else
            ASSERT(class_colour != BLACK);
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
    // ...except for un-ID'd random-appearance artefacts (Misfortune)
    if (is_unrandom_artefact(*this) && !is_randapp_artefact(*this))
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
        case OBJ_FOOD:
            return food_colour();
        case OBJ_JEWELLERY:
            return jewellery_colour();
        case OBJ_SCROLLS:
            return LIGHTGREY;
        case OBJ_BOOKS:
            return book_colour();
        case OBJ_RODS:
            return YELLOW;
        case OBJ_STAVES:
            return BROWN;
        case OBJ_ORBS:
            return ETC_MUTAGENIC;
        case OBJ_CORPSES:
            return corpse_colour();
        case OBJ_MISCELLANY:
            return miscellany_colour();
        case OBJ_GOLD:
            return YELLOW;
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
        || base_type == OBJ_RODS
        || base_type == OBJ_MISCELLANY;
}

// Checks whether the item is actually a good one.
// TODO: check brands, etc.
bool item_def::is_valid(bool iinfo) const
{
    if (base_type == OBJ_DETECTED)
    {
        if (!iinfo)
            dprf("weird detected item");
        return iinfo;
    }
    else if (!defined())
    {
        dprf("undefined");
        return false;
    }
    const int max_sub = get_max_subtype(base_type);
    if (max_sub != -1 && sub_type >= max_sub)
    {
        if (!iinfo || sub_type > max_sub || !item_type_has_unidentified(base_type))
        {
            if (!iinfo)
                dprf("weird subtype and no info");
            if (sub_type > max_sub)
                dprf("huge subtype");
            if (!item_type_has_unidentified(base_type))
                dprf("unided item of a type that can't be");
            return false;
        }
    }
    if (get_colour() == 0)
    {
        dprf("black item");
        return false; // No black items.
    }
    if (!appearance_initialized())
    {
        dprf("no rnd");
        return false; // no items with uninitialized rnd
    }
    return true;
}

// The Orb of Zot and unique runes are considered critical.
bool item_def::is_critical() const
{
    if (!defined())
        return false;

    if (base_type == OBJ_ORBS)
        return true;

    return base_type == OBJ_MISCELLANY
           && sub_type == MISC_RUNE_OF_ZOT
           && plus != RUNE_DEMONIC
           && plus != RUNE_ABYSSAL;
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

// Does the item cause autoexplore to visit it?
// Excludes visited items (dropped flag) and ?RC for Ash.
bool item_def::is_greedy_sacrificeable() const
{
    if (!god_likes_items(you.religion, true))
        return false;

    if (flags & (ISFLAG_DROPPED | ISFLAG_THROWN)
        || item_needs_autopickup(*this)
        || item_is_stationary_net(*this)
        || inscription.find("!p") != string::npos
        || inscription.find("=p") != string::npos
        || inscription.find("!*") != string::npos
        || inscription.find("!D") != string::npos)
    {
        return false;
    }

    return god_likes_item(you.religion, *this);
}

static void _rune_from_specs(const char* _specs, item_def &item)
{
    char specs[80];

    item.sub_type = MISC_RUNE_OF_ZOT;

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
            item.plus = i;

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

        item.plus = keyin - 'a';

        return;
    }
}

static void _deck_from_specs(const char* _specs, item_def &item)
{
    string specs    = _specs;
    string type_str = "";

    trim_string(specs);

    if (specs.find(" of ") != string::npos)
    {
        type_str = specs.substr(specs.find(" of ") + 4);

        if (type_str.find("card") != string::npos
            || type_str.find("deck") != string::npos)
        {
            type_str = "";
        }

        trim_string(type_str);
    }

    misc_item_type types[] =
    {
        MISC_DECK_OF_ESCAPE,
        MISC_DECK_OF_DESTRUCTION,
        MISC_DECK_OF_SUMMONING,
        MISC_DECK_OF_WONDERS,
        MISC_DECK_OF_PUNISHMENT,
        MISC_DECK_OF_WAR,
        MISC_DECK_OF_CHANGES,
        MISC_DECK_OF_DEFENCE,
        MISC_DECK_UNKNOWN,
    };

    item.special  = DECK_RARITY_COMMON;
    item.sub_type = MISC_DECK_UNKNOWN;

    if (!type_str.empty())
    {
        for (int i = 0; types[i] != MISC_DECK_UNKNOWN; ++i)
        {
            item.sub_type = types[i];
            item.plus     = 1;
            init_deck(item);
            // Remove "plain " from front.
            string name = item.name(DESC_PLAIN).substr(6);
            item.props.clear();

            if (name.find(type_str) != string::npos)
                break;
        }
    }

    if (item.sub_type == MISC_DECK_UNKNOWN)
    {
        while (true)
        {
            mprf(MSGCH_PROMPT,
"[a] escape     [b] destruction [c] summoning [d] wonders");
            mprf(MSGCH_PROMPT,
"[e] punishment [f] war         [g] changes  [h] defence");
            mpr("Which deck (ESC to exit)? ");

            const int keyin = toalower(get_ch());

            if (key_is_escape(keyin) || keyin == ' '
                || keyin == '\r' || keyin == '\n')
            {
                canned_msg(MSG_OK);
                item.base_type = OBJ_UNASSIGNED;
                return;
            }

            if (keyin < 'a' || keyin > 'h')
                continue;

            item.sub_type = types[keyin - 'a'];
            break;
        }
    }

    const char* rarities[] =
    {
        "plain",
        "ornate",
        "legendary",
        NULL
    };

    int rarity_val = -1;

    for (int i = 0; rarities[i] != NULL; ++i)
        if (specs.find(rarities[i]) != string::npos)
        {
            rarity_val = i;
            break;
        }

    if (rarity_val == -1)
    {
        while (true)
        {
            mprf(MSGCH_PROMPT, "[a] plain [b] ornate [c] legendary? (ESC to exit)");

            int keyin = toalower(get_ch());

            if (key_is_escape(keyin) || keyin == ' '
                || keyin == '\r' || keyin == '\n')
            {
                canned_msg(MSG_OK);
                item.base_type = OBJ_UNASSIGNED;
                return;
            }

            switch (keyin)
            {
            case 'p': keyin = 'a'; break;
            case 'o': keyin = 'b'; break;
            case 'l': keyin = 'c'; break;
            }

            if (keyin < 'a' || keyin > 'c')
                continue;

            rarity_val = keyin - 'a';
            break;
        }
    }

    const deck_rarity_type rarity =
        static_cast<deck_rarity_type>(DECK_RARITY_COMMON + rarity_val);
    item.special = rarity;

    int num = prompt_for_int("How many cards? ", false);

    if (num <= 0)
    {
        canned_msg(MSG_OK);
        item.base_type = OBJ_UNASSIGNED;
        return;
    }

    item.plus = num;

    init_deck(item);
}

static void _rune_or_deck_from_specs(const char* specs, item_def &item)
{
    if (strstr(specs, "rune"))
        _rune_from_specs(specs, item);
    else if (strstr(specs, "deck") || strstr(specs, "card"))
        _deck_from_specs(specs, item);
}

static bool _book_from_spell(const char* specs, item_def &item)
{
    spell_type type = spell_by_name(specs, true);

    if (type == SPELL_NO_SPELL)
        return false;

    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
        for (int j = 0; j < 8; ++j)
            if (which_spell_in_book(i, j) == type)
            {
                item.sub_type = i;
                return true;
            }

    return false;
}

bool get_item_by_name(item_def *item, char* specs,
                      object_class_type class_wanted, bool create_for_real)
{
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
    // Don't use set_ident_flags(), to avoid getting a spurious ID note.
    item->flags    |= ISFLAG_IDENT_MASK;

    if (class_wanted == OBJ_MISCELLANY)
    {
        // Leaves object unmodified if it wasn't a rune or deck.
        _rune_or_deck_from_specs(specs, *item);

        if (item->base_type == OBJ_UNASSIGNED)
        {
            // Rune or deck creation cancelled, clean up item->
            return false;
        }
    }

    if (!item->sub_type)
    {
        type_wanted = -1;
        size_t best_index  = 10000;

        for (int i = 0; i < get_max_subtype(item->base_type); ++i)
        {
            item->sub_type = i;
            size_t pos = lowercase_string(item->name(DESC_PLAIN)).find(specs);
            if (pos != string::npos)
            {
                // Earliest match is the winner.
                if (pos < best_index)
                {
                    if (create_for_real)
                        mpr(item->name(DESC_PLAIN).c_str());
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

            for (int i = SPWPN_NORMAL + 1; i < SPWPN_DEBUG_RANDART; ++i)
            {
                item->special = i;
                size_t pos = lowercase_string(item->name(DESC_PLAIN)).find(buf_lwr);
                if (pos != string::npos)
                {
                    // earliest match is the winner
                    if (pos < best_index)
                    {
                        if (create_for_real)
                            mpr(item->name(DESC_PLAIN).c_str());
                        special_wanted = i;
                        best_index = pos;
                    }
                }
            }

            item->special = special_wanted;
        }
        break;
    }

    case OBJ_BOOKS:
        if (item->sub_type == BOOK_MANUAL)
        {
            skill_type skill =
                    debug_prompt_for_skill("A manual for which skill? ");

            if (skill != SK_NONE)
            {
                item->skill        = skill;
                item->skill_points = random_range(2000, 3000);
            }
            else
                mpr("Sorry, no books on that skill today.");
        }
        else if (type_wanted == BOOK_RANDART_THEME)
            make_book_theme_randart(*item, 0, 0, 5 + coinflip(), 20);
        else if (type_wanted == BOOK_RANDART_LEVEL)
        {
            int level = random_range(1, 9);
            make_book_level_randart(*item, level);
        }
        break;

    case OBJ_WANDS:
        item->plus = 24;
        break;

    case OBJ_RODS:
        item->charges    = MAX_ROD_CHARGE * ROD_CHARGE_MULT;
        item->charge_cap = MAX_ROD_CHARGE * ROD_CHARGE_MULT;
        init_rod_mp(*item);
        break;

    case OBJ_MISCELLANY:
        if (item->sub_type == MISC_BOX_OF_BEASTS
            || item->sub_type == MISC_SACK_OF_SPIDERS)
        {
            item->charges = 50;
        }
        break;

    case OBJ_POTIONS:
        item->quantity = 12;
        if (is_blood_potion(*item))
        {
            const char* prompt;
            prompt = "# turns away from rotting? "
                     "[ENTER for fully fresh] ";
            int age = prompt_for_int(prompt, false);

            if (age <= 0)
                age = -1;
            init_perishable_stack(*item, age);
        }
        break;

    case OBJ_FOOD:
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

// Returns the position of the Orb on the floor, or
// coord_def() if not present
coord_def orb_position()
{
    item_def* orb = find_floor_item(OBJ_ORBS, ORB_ZOT);
    return orb ? orb->pos: coord_def();
}

void move_items(const coord_def r, const coord_def p)
{
    ASSERT_IN_BOUNDS(r);
    ASSERT_IN_BOUNDS(p);

    int it = igrd(r);

    if (it == NON_ITEM)
        return;

    while (it != NON_ITEM)
    {
        mitm[it].pos.x = p.x;
        mitm[it].pos.y = p.y;
        if (mitm[it].link == NON_ITEM)
        {
            // Link to the stack on the target grid p,
            // or NON_ITEM, if empty.
            mitm[it].link = igrd(p);
            break;
        }
        it = mitm[it].link;
    }

    // Move entire stack over to p.
    igrd(p) = igrd(r);
    igrd(r) = NON_ITEM;
}

// erase everything the player doesn't know
item_info get_item_info(const item_def& item)
{
    item_info ii;

    ii.base_type = item.base_type;
    ii.quantity = item.quantity;
    ii.inscription = item.inscription;
    ii.flags = item.flags & (0
            | ISFLAG_IDENT_MASK | ISFLAG_SEEN_CURSED
            | ISFLAG_ARTEFACT_MASK | ISFLAG_DROPPED | ISFLAG_THROWN
            | ISFLAG_COSMETIC_MASK);

    if (in_inventory(item))
    {
        ii.link = item.link;
        ii.slot = item.slot;
        ii.pos = coord_def(-1, -1);
    }
    else
        ii.pos = item.pos;

    ii.rnd = item.rnd; // XXX: may (?) leak cosmetic (?) info...?
    if (ii.rnd == 0)
        ii.rnd = 1; // don't leave "uninitialized" item infos around

    // keep god number
    if (item.orig_monnum < 0)
        ii.orig_monnum = item.orig_monnum;

    if (is_unrandom_artefact(item))
    {
        if (!is_randapp_artefact(item))
        {
            // Unrandart index
            // Since the appearance of unrandarts is fixed anyway, this
            // is not an information leak.
            ii.special = item.special;
        }
        else
        {
            // Disguise as a normal randart
            ii.flags &= ~ISFLAG_UNRANDART;
            ii.flags |= ISFLAG_RANDART;
        }
    }

    switch (item.base_type)
    {
    case OBJ_MISSILES:
        if (item_ident(ii, ISFLAG_KNOW_PLUSES))
            ii.net_placed = item.net_placed;
        // intentional fall-through
    case OBJ_WEAPONS:
        ii.sub_type = item.sub_type;
        if (item_ident(ii, ISFLAG_KNOW_PLUSES))
            ii.plus = item.plus;
        if (item_type_known(item))
            ii.brand = item.brand;
        break;
    case OBJ_ARMOUR:
        ii.sub_type = item.sub_type;
        if (item_ident(ii, ISFLAG_KNOW_PLUSES))
            ii.plus = item.plus;
        if (item_type_known(item))
            ii.brand = item.brand;
        break;
    case OBJ_WANDS:
        if (item_type_known(item))
            ii.sub_type = item.sub_type;
        else
            ii.sub_type = NUM_WANDS;
        ii.subtype_rnd = item.subtype_rnd;
        if (item_ident(ii, ISFLAG_KNOW_PLUSES))
            ii.charges = item.charges;
        ii.used_count = item.used_count; // num zapped/recharged or empty
        break;
    case OBJ_POTIONS:
        if (item_type_known(item))
            ii.sub_type = item.sub_type;
        else
            ii.sub_type = NUM_POTIONS;
        ii.subtype_rnd = item.subtype_rnd;
        break;
    case OBJ_FOOD:
        ii.sub_type = item.sub_type;
        if (ii.sub_type == FOOD_CHUNK)
        {
            ii.mon_type = item.mon_type;
            ii.freshness = 100;
        }
        break;
    case OBJ_CORPSES:
        ii.sub_type = item.sub_type;
        ii.mon_type = item.mon_type;
        ii.freshness = 100;
        break;
    case OBJ_SCROLLS:
        if (item_type_known(item))
            ii.sub_type = item.sub_type;
        else
            ii.sub_type = NUM_SCROLLS;
        ii.subtype_rnd = item.subtype_rnd;    // name seed
        break;
    case OBJ_JEWELLERY:
        if (item_type_known(item))
            ii.sub_type = item.sub_type;
        else
            ii.sub_type = jewellery_is_amulet(item) ? NUM_JEWELLERY : NUM_RINGS;
        if (item_ident(ii, ISFLAG_KNOW_PLUSES))
            ii.plus = item.plus;   // str/dex/int/ac/ev ring plus
        ii.subtype_rnd = item.subtype_rnd;
        break;
    case OBJ_BOOKS:
        if (item_type_known(item) || !item_is_spellbook(item))
            ii.sub_type = item.sub_type;
        else
            ii.sub_type = NUM_BOOKS;
        ii.subtype_rnd = item.subtype_rnd;
        if (item.sub_type == BOOK_MANUAL && item_type_known(item))
            ii.skill = item.skill; // manual skill
        break;
    case OBJ_RODS:
        if (item_type_known(item))
        {
            ii.sub_type = item.sub_type;
            if (item_ident(ii, ISFLAG_KNOW_PLUSES))
            {
                ii.rod_plus = item.rod_plus;
                ii.charges = item.charges;
                ii.charge_cap = item.charge_cap;
            }
        }
        else
            ii.sub_type = NUM_RODS;
        break;
    case OBJ_STAVES:
        ii.sub_type = item_type_known(item) ? item.sub_type : NUM_STAVES;
        ii.subtype_rnd = item.subtype_rnd;
        break;
    case OBJ_MISCELLANY:
        if (item_type_known(item))
            ii.sub_type = item.sub_type;
        else
        {
            if (item.sub_type >= MISC_DECK_OF_ESCAPE
                 && item.sub_type <= MISC_DECK_OF_DEFENCE)
            {
                // Needs to be changed if we add other miscellaneous items
                // that can be non-identified.
                ii.sub_type = MISC_DECK_UNKNOWN;
            }
            else
                ii.sub_type = item.sub_type;
        }

        if (ii.sub_type == MISC_RUNE_OF_ZOT)
            ii.rune_enum = item.rune_enum; // which rune

        if (ii.sub_type == MISC_DECK_UNKNOWN)
            ii.deck_rarity = item.deck_rarity;

        // Preserve inert/charged state but not the actual numbers.
        if (is_xp_evoker(item))
            ii.evoker_debt = !!item.evoker_debt;

        if (is_deck(item))
        {
            ii.deck_rarity = item.deck_rarity;

            const int num_cards = cards_in_deck(item);
            CrawlVector info_cards (SV_BYTE);
            CrawlVector info_card_flags (SV_BYTE);
            bool found_unmarked = false;
            for (int i = 0; i < num_cards; ++i)
            {
                uint8_t flags;
                const card_type card = get_card_and_flags(item, -i-1, flags);
                if (flags & CFLAG_MARKED)
                {
                    info_cards.push_back((char)card);
                    info_card_flags.push_back((char)flags);
                }
                else if (!found_unmarked)
                {
                    // special card to tell at which point cards are no longer
                    // continuous
                    info_cards.push_back((char)0);
                    info_card_flags.push_back((char)0);
                    found_unmarked = true;
                }
            }
            // TODO: this leaks both whether the seen cards are still there
            // and their order: the representation needs to be fixed

            // The above comment seems obsolete now that Mark Four is gone.
            for (int i = 0; i < num_cards; ++i)
            {
                uint8_t flags;
                const card_type card = get_card_and_flags(item, -i-1, flags);
                if (flags & CFLAG_SEEN && !(flags & CFLAG_MARKED))
                {
                    info_cards.push_back((char)card);
                    info_card_flags.push_back((char)flags);
                }
            }
            ii.props["cards"] = info_cards;
            ii.props["card_flags"] = info_card_flags;
        }
        break;
    case OBJ_ORBS:
    case OBJ_GOLD:
    default:
        ii.sub_type = item.sub_type;
    }

    if (item_ident(item, ISFLAG_KNOW_CURSE))
        ii.flags |= (item.flags & ISFLAG_CURSED);

    if (item_type_known(item))
    {
        ii.flags |= ISFLAG_KNOW_TYPE;

        if (item.props.exists(ARTEFACT_NAME_KEY))
            ii.props[ARTEFACT_NAME_KEY] = item.props[ARTEFACT_NAME_KEY];
    }
    else if (item_type_tried(item))
        ii.flags |= ISFLAG_TRIED;

    const char* copy_props[] = {ARTEFACT_APPEAR_KEY, KNOWN_PROPS_KEY,
                                CORPSE_NAME_KEY, CORPSE_NAME_TYPE_KEY,
                                "drawn_cards", "item_tile", "item_tile_name",
                                "worn_tile", "worn_tile_name",
                                "needs_autopickup", FORCED_ITEM_COLOUR_KEY};
    for (unsigned i = 0; i < ARRAYSZ(copy_props); ++i)
    {
        if (item.props.exists(copy_props[i]))
            ii.props[copy_props[i]] = item.props[copy_props[i]];
    }

    const char* copy_ident_props[] = {"spell_list"};
    if (item_ident(item, ISFLAG_KNOW_TYPE))
    {
        for (unsigned i = 0; i < ARRAYSZ(copy_ident_props); ++i)
        {
            if (item.props.exists(copy_ident_props[i]))
                ii.props[copy_ident_props[i]] = item.props[copy_ident_props[i]];
        }
    }

    if (item.props.exists(ARTEFACT_PROPS_KEY))
    {
        CrawlVector props = item.props[ARTEFACT_PROPS_KEY].get_vector();
        const CrawlVector &known = item.props[KNOWN_PROPS_KEY].get_vector();

        if (!item_ident(item, ISFLAG_KNOW_PROPERTIES))
        {
            for (unsigned i = 0; i < props.size(); ++i)
            {
                if (i >= known.size() || !known[i].get_bool())
                    props[i] = (short)0;
            }
        }

        ii.props[ARTEFACT_PROPS_KEY] = props;
    }

    return ii;
}

int runes_in_pack()
{
    return static_cast<int>(you.runes.count());
}

static const object_class_type _mimic_item_classes[] =
{
    OBJ_GOLD,
    OBJ_WEAPONS,
    OBJ_ARMOUR,
    OBJ_SCROLLS,
    OBJ_POTIONS,
    OBJ_BOOKS,
    OBJ_STAVES,
    OBJ_RODS,
    OBJ_FOOD,
    OBJ_MISCELLANY,
    OBJ_JEWELLERY,
};

object_class_type get_random_item_mimic_type()
{
    return _mimic_item_classes[random2(ARRAYSZ(_mimic_item_classes))];
}

object_class_type get_item_mimic_type()
{
    clear_messages();
    map<char, object_class_type> choices;
    char letter = 'a';
    for (unsigned int i = 0; i < ARRAYSZ(_mimic_item_classes); ++i)
    {
        mprf("[%c] %s ", letter,
             item_class_name(_mimic_item_classes[i], true).c_str());
        choices[letter++] = _mimic_item_classes[i];
    }
    mprf("[%c] random", letter);
    choices[letter] = OBJ_RANDOM;
    mprf(MSGCH_PROMPT, "\nWhat kind of item mimic? ");
    const int keyin = toalower(get_ch());

    if (!choices.count(keyin))
        return OBJ_UNASSIGNED;
    else if (choices[keyin] == OBJ_RANDOM)
        return get_random_item_mimic_type();
    else
        return choices[keyin];
}

bool is_valid_mimic_item(const item_def &item)
{
    // Nothing important.
    if (item_is_orb(item) || item_is_horn_of_geryon(item) || item_is_rune(item))
        return false;

    for (unsigned int i = 0; i < ARRAYSZ(_mimic_item_classes); ++i)
        if (item.base_type == _mimic_item_classes[i])
            return true;
    return false;
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
        item.props["needs_autopickup"] = true;
    }

    set_ident_type(item, ID_KNOWN_TYPE);

    if (item.props.exists("needs_autopickup") && is_useless_item(item))
        item.props.erase("needs_autopickup");

    const string class_name = item.base_type == OBJ_JEWELLERY ?
                                    item_base_name(item) :
                                    item_class_name(item.base_type, true);
    mprf("You have identified the last %s.", class_name.c_str());

    if (in_inventory(item))
        mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
}


/**
 * Check to see if there's only one unidentified subtype left in the given
 * item's object type. If so, automatically identify it.
 *
 * @param item  The item in question.
 * @return      Whether the item was identified.
 */
bool maybe_identify_base_type(item_def &item)
{
    if (is_artefact(item))
        return false;
    if (get_ident_type(item) == ID_KNOWN_TYPE)
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
                                == ID_KNOWN_TYPE;
        ident_count += identified ? 1 : 0;
    }

    if (ident_count < item_count - 1)
        return false;

    _identify_last_item(item);
    return true;
}
