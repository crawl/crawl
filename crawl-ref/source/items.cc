/*
 *  File:       items.cc
 *  Summary:    Misc (mostly) inventory related functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "items.h"
#include "cio.h"
#include "clua.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "externs.h"

#include "arena.h"
#include "artefact.h"
#include "beam.h"
#include "branch.h"
#include "coord.h"
#include "coordit.h"
#include "dbg-util.h"
#include "debug.h"
#include "decks.h"
#include "delay.h"
#include "dgnevent.h"
#include "directn.h"
#include "effects.h"
#include "env.h"
#include "food.h"
#include "godprayer.h"
#include "hiscores.h"
#include "invent.h"
#include "it_use2.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-util.h"
#include "mon-stuff.h"
#include "mutation.h"
#include "notes.h"
#include "options.h"
#include "place.h"
#include "player.h"
#include "quiver.h"
#include "religion.h"
#include "shopping.h"
#include "showsymb.h"
#include "spl-book.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "areas.h"
#include "stash.h"
#include "state.h"
#include "terrain.h"
#include "travel.h"
#include "hints.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "xom.h"

#define PORTAL_VAULT_ORIGIN_KEY "portal_vault_origin"

static bool _invisible_to_player( const item_def& item );
static void _autoinscribe_item( item_def& item );
static void _autoinscribe_floor_items();
static void _autoinscribe_inventory();

static inline std::string _autopickup_item_name(const item_def &item);

static bool will_autopickup   = false;
static bool will_autoinscribe = false;

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
void link_items(void)
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

        // link to top
        mitm[i].link = igrd( mitm[i].pos );
        igrd( mitm[i].pos ) = i;
    }
}

static bool _item_ok_to_clean(int item)
{
    // Never clean food or Orbs.
    if (mitm[item].base_type == OBJ_FOOD || item_is_orb(mitm[item]))
        return (false);

    // Never clean runes.
    if (mitm[item].base_type == OBJ_MISCELLANY
        && mitm[item].sub_type == MISC_RUNE_OF_ZOT)
    {
        return (false);
    }

    return (true);
}

// Returns index number of first available space, or NON_ITEM for
// unsuccessful cleanup (should be exceedingly rare!)
static int _cull_items(void)
{
    crawl_state.cancel_cmd_repeat();

    // XXX: Not the prettiest of messages, but the player
    // deserves to know whenever this kicks in. -- bwr
    mpr("Too many items on level, removing some.", MSGCH_WARN);

    // Rules:
    //  1. Don't cleanup anything nearby the player
    //  2. Don't cleanup shops
    //  3. Don't cleanup monster inventory
    //  4. Clean 15% of items
    //  5. never remove food, orbs, runes
    //  7. uniques weapons are moved to the abyss
    //  8. randarts are simply lost
    //  9. unrandarts are 'destroyed', but may be generated again

    int first_cleaned = NON_ITEM;

    // 2. Avoid shops by avoiding (0,5..9).
    // 3. Avoid monster inventory by iterating over the dungeon grid.
    for (rectangle_iterator ri(1); ri; ++ri)
    {
        if (grid_distance( you.pos(), *ri ) <= 9)
            continue;

        for (stack_iterator si(*ri); si; ++si)
        {
            if (_item_ok_to_clean(si->index()) && x_chance_in_y(15, 100))
            {
                if (is_unrandom_artefact(*si))
                {
                    // 7. Move uniques to abyss.
                    set_unique_item_status(*si, UNIQ_LOST_IN_ABYSS);
                }

                if (first_cleaned == NON_ITEM)
                    first_cleaned = si->index();

                // POOF!
                destroy_item( si->index() );
            }
        }
    }

    return (first_cleaned);
}

// Reduce quantity of an inventory item, do cleanup if item goes away.
//
// Returns true if stack of items no longer exists.
bool dec_inv_item_quantity( int obj, int amount, bool suppress_burden )
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
                    canned_msg( MSG_EMPTY_HANDED );
                }
                you.equip[i] = -1;
            }
        }

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
    {
        you.inv[obj].quantity -= amount;
    }

    if (!suppress_burden)
        burden_change();

    return (ret);
}

// Reduce quantity of a monster/grid item, do cleanup if item goes away.
//
// Returns true if stack of items no longer exists.
bool dec_mitm_item_quantity( int obj, int amount )
{
    if (mitm[obj].quantity <= amount)
        amount = mitm[obj].quantity;

    if (mitm[obj].quantity == amount)
    {
        destroy_item( obj );
        // If we're repeating a command, the repetitions used up the
        // item stack being repeated on, so stop rather than move onto
        // the next stack.
        crawl_state.cancel_cmd_repeat();
        crawl_state.cancel_cmd_again();
        return (true);
    }

    mitm[obj].quantity -= amount;

    return (false);
}

void inc_inv_item_quantity( int obj, int amount, bool suppress_burden )
{
    if (you.equip[EQ_WEAPON] == obj)
        you.wield_change = true;

    you.m_quiver->on_inv_quantity_changed(obj, amount);
    you.inv[obj].quantity += amount;

    if (!suppress_burden)
        burden_change();
}

void inc_mitm_item_quantity( int obj, int amount )
{
    mitm[obj].quantity += amount;
}

void init_item( int item )
{
    if (item == NON_ITEM)
        return;

    mitm[item].clear();
}

// Returns an unused mitm slot, or NON_ITEM if none available.
// The reserve is the number of item slots to not check.
// Items may be culled if a reserve <= 10 is specified.
int get_item_slot( int reserve )
{
    ASSERT( reserve >= 0 );

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
                return (NON_ITEM);
        }
        else
            item = (reserve <= 10) ? _cull_items() : NON_ITEM;

        if (item == NON_ITEM)
            return (NON_ITEM);
    }

    ASSERT( item != NON_ITEM );

    init_item( item );

    return (item);
}

void unlink_item( int dest )
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
        ASSERT(in_bounds(mitm[dest].pos) || is_shop_item(mitm[dest]));

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
         mitm[dest].pos.x, mitm[dest].pos.y );

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
    {
        mpr("BUG WARNING: Item didn't seem to be linked at all.",
            MSGCH_ERROR);
    }
#endif
}

void destroy_item( item_def &item, bool never_created )
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

void destroy_item( int dest, bool never_created )
{
    // Don't destroy non-items, but this function may be called upon
    // to remove items reduced to zero quantity, so we allow "invalid"
    // objects in.
    if (dest == NON_ITEM || !mitm[dest].defined())
        return;

    unlink_item( dest );
    destroy_item( mitm[dest], never_created );
}

static void _handle_gone_item(const item_def &item)
{
    if (you.level_type == LEVEL_ABYSS
        && place_type(item.orig_place) == LEVEL_ABYSS
        && !(item.flags & ISFLAG_BEEN_IN_INV))
    {
        if (is_unrandom_artefact(item))
            set_unique_item_status(item, UNIQ_LOST_IN_ABYSS);
    }

    if (item_is_rune(item))
    {
        if ((item.flags & ISFLAG_BEEN_IN_INV))
        {
            if (item_is_unique_rune(item))
                you.attribute[ATTR_UNIQUE_RUNES] -= item.quantity;
            else if (item.plus == RUNE_ABYSSAL)
                you.attribute[ATTR_ABYSSAL_RUNES] -= item.quantity;
            else
                you.attribute[ATTR_DEMONIC_RUNES] -= item.quantity;
        }
    }
}

void item_was_lost(const item_def &item)
{
    _handle_gone_item( item );
    xom_check_lost_item( item );
}

static void _note_item_destruction(const item_def &item)
{
    if (item_is_orb(item))
    {
        mprf(MSGCH_WARN, "A great rumbling fills the air... "
             "the Orb of Zot has been destroyed!");
        mark_milestone("orb.destroy", "destroyed the Orb of Zot");
    }
}

void item_was_destroyed(const item_def &item, int cause)
{
    _handle_gone_item( item );
    _note_item_destruction(item);
    xom_check_destroyed_item( item, cause );
}

void lose_item_stack( const coord_def& where )
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

void destroy_item_stack( int x, int y, int cause )
{
    for (stack_iterator si(coord_def(x,y)); si; ++si)
    {
        if (si ->defined()) // FIXME is this check necessary?
        {
            item_was_destroyed( *si, cause);
            si->clear();
        }
    }
    igrd[x][y] = NON_ITEM;
}

static bool _invisible_to_player( const item_def& item )
{
    return strstr(item.inscription.c_str(), "=k") != 0;
}

static int _count_nonsquelched_items( int obj )
{
    int result = 0;

    for (stack_iterator si(obj); si; ++si)
        if (!_invisible_to_player(*si))
            ++result;

    return result;
}

// Fill items with the items on a square.
// Squelched items (marked with =k) are ignored, unless
// the square contains *only* squelched items, in which case they
// are included. If force_squelch is true, squelched items are
// never displayed.
void item_list_on_square( std::vector<const item_def*>& items,
                          int obj, bool force_squelch )
{
    const bool have_nonsquelched = (force_squelch
                                    || _count_nonsquelched_items(obj));

    // Loop through the items.
    for (stack_iterator si(obj); si; ++si)
    {
        // Add them to the items list if they qualify.
        if (!have_nonsquelched || !_invisible_to_player(*si))
            items.push_back( & (*si) );
    }
}

bool need_to_autopickup()
{
    return will_autopickup;
}

void request_autopickup(bool do_pickup)
{
    will_autopickup = do_pickup;
}

// 2 - artefact, 1 - glowing/runed, 0 - mundane
int item_name_specialness(const item_def& item)
{
    // All jewellery is worth looking at.
    // And we can always tell from the name if it's an artefact.
    if (item.base_type == OBJ_JEWELLERY )
        return ( is_artefact(item) ? 2 : 1 );

    if (item.base_type != OBJ_WEAPONS && item.base_type != OBJ_ARMOUR
        && item.base_type != OBJ_MISSILES)
    {
        return 0;
    }
    if (item_type_known(item))
    {
        if (is_artefact(item))
            return 2;

        // XXX Unite with l_item_branded() in clua.cc
        bool branded = false;
        switch (item.base_type)
        {
        case OBJ_WEAPONS:
            branded = get_weapon_brand(item) != SPWPN_NORMAL;
            break;
        case OBJ_ARMOUR:
            branded = get_armour_ego_type(item) != SPARM_NORMAL;
            break;
        case OBJ_MISSILES:
            branded = get_ammo_brand(item) != SPMSL_NORMAL;
            break;
        default:
            break;
        }
        return ( branded ? 1 : 0 );
    }

    std::string itname = item.name(DESC_PLAIN, false, false, false);
    lowercase(itname);

    // FIXME Maybe we should replace this with a test of ISFLAG_COSMETIC_MASK?
    const bool item_runed = itname.find("runed ") != std::string::npos;
    const bool heav_runed = itname.find("heavily ") != std::string::npos;
    const bool item_glows = itname.find("glowing") != std::string::npos;

    if (item_glows || item_runed && !heav_runed
        || get_equip_desc(item) == ISFLAG_EMBROIDERED_SHINY)
    {
        return 1;
    }

    // You can tell something is an artefact, because it'll have a
    // description which rules out anything else.
    // XXX: Fixedarts and unrandarts might upset the apple-cart, though.
    if (is_artefact(item))
        return 2;

    return 0;
}

void item_check(bool verbose)
{
    describe_floor();
    origin_set(you.pos());

    std::ostream& strm = msg::streams(MSGCH_FLOOR_ITEMS);

    std::vector<const item_def*> items;

    item_list_on_square(items, you.visible_igrd(you.pos()), true);

    if (items.empty())
    {
        if (verbose)
            strm << "There are no items here." << std::endl;
        return;
    }

    if (items.size() == 1)
    {
        item_def it(*items[0]);
        std::string name = get_menu_colour_prefix_tags(it, DESC_NOCAP_A);
        strm << "You see here " << name << '.' << std::endl;
        return;
    }

    bool done_init_line = false;

    if (static_cast<int>(items.size()) >= Options.item_stack_summary_minimum)
    {
        std::vector<unsigned int> item_chars;
        for (unsigned int i = 0; i < items.size() && i < 50; ++i)
        {
            glyph g = get_item_glyph(items[i]);
            get_item_glyph(items[i]);
            item_chars.push_back(g.ch * 0x100 +
                                 (10 - item_name_specialness(*(items[i]))));
        }
        std::sort(item_chars.begin(), item_chars.end());

        std::string out_string = "Items here: ";
        int cur_state = -1;
        for (unsigned int i = 0; i < item_chars.size(); ++i)
        {
            const int specialness = 10 - (item_chars[i] % 0x100);
            if (specialness != cur_state)
            {
                switch (specialness)
                {
                case 2: out_string += "<yellow>";   break; // artefact
                case 1: out_string += "<white>";    break; // glowing/runed
                case 0: out_string += "<darkgrey>"; break; // mundane
                }
                cur_state = specialness;
            }

            out_string += stringize_glyph(item_chars[i] / 0x100);
            if (i + 1 < item_chars.size()
                && (item_chars[i] / 0x100) != (item_chars[i+1] / 0x100))
            {
                out_string += ' ';
            }
        }
        mprnojoin(out_string, MSGCH_FLOOR_ITEMS);
        done_init_line = true;
    }

    if (verbose || items.size() <= msgwin_lines() - 1)
    {
        if (!done_init_line)
            mprnojoin("Things that are here:", MSGCH_FLOOR_ITEMS);
        for (unsigned int i = 0; i < items.size(); ++i)
        {
            item_def it(*items[i]);
            std::string name = get_menu_colour_prefix_tags(it, DESC_NOCAP_A);
            strm << name << std::endl;
        }
    }
    else if (!done_init_line)
        strm << "There are many items here." << std::endl;

    if (items.size() > 5)
        learned_something_new(HINT_MULTI_PICKUP);
}

static void _pickup_menu(int item_link)
{
    int n_did_pickup   = 0;
    int n_tried_pickup = 0;

    std::vector<const item_def*> items;
    item_list_on_square( items, item_link, false );

    std::vector<SelItem> selected =
        select_items( items, "Select items to pick up or press _ for help" );
    redraw_screen();

    std::string pickup_warning;
    for (int i = 0, count = selected.size(); i < count; ++i)
        for (int j = item_link; j != NON_ITEM; j = mitm[j].link)
        {
            if (&mitm[j] == selected[i].item)
            {
                if (j == item_link)
                    item_link = mitm[j].link;

                int num_to_take = selected[i].quantity;
                const bool take_all = (num_to_take == mitm[j].quantity);
                iflags_t oldflags = mitm[j].flags;
                mitm[j].flags &= ~(ISFLAG_THROWN | ISFLAG_DROPPED);
                int result = move_item_to_player( j, num_to_take );

                // If we cleared any flags on the items, but the pickup was
                // partial, reset the flags for the items that remain on the
                // floor.
                if (result == 0 || result == -1)
                {
                    n_tried_pickup++;
                    if (result == 0)
                        pickup_warning = "You can't carry that much weight.";
                    else
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

    if (!pickup_warning.empty())
    {
        mpr(pickup_warning.c_str());
        learned_something_new(HINT_HEAVY_LOAD);
    }

    if (n_did_pickup)
        you.turn_is_over = true;
}

bool origin_known(const item_def &item)
{
    return (item.orig_place != 0);
}

void origin_reset(item_def &item)
{
    item.orig_place  = 0;
    item.orig_monnum = 0;
}

// We have no idea where the player found this item.
void origin_set_unknown(item_def &item)
{
    if (!origin_known(item))
    {
        item.orig_place  = 0xFFFF;
        item.orig_monnum = 0;
    }
}

// This item is starting equipment.
void origin_set_startequip(item_def &item)
{
    if (!origin_known(item))
    {
        item.orig_place  = 0xFFFF;
        item.orig_monnum = -IT_SRC_START;
    }
}

static void _origin_set_portal_vault(item_def &item)
{
    if (you.level_type != LEVEL_PORTAL_VAULT)
        return;

    item.props[PORTAL_VAULT_ORIGIN_KEY] = you.level_type_origin;
}

void origin_set_monster(item_def &item, const monster* mons)
{
    if (!origin_known(item))
    {
        if (!item.orig_monnum)
            item.orig_monnum = mons->type + 1;
        item.orig_place = get_packed_place();
        _origin_set_portal_vault(item);
    }
}

void origin_purchased(item_def &item)
{
    // We don't need to check origin_known if it's a shop purchase
    item.orig_place  = get_packed_place();
    _origin_set_portal_vault(item);
    item.orig_monnum = -IT_SRC_SHOP;
}

void origin_acquired(item_def &item, int agent)
{
    // We don't need to check origin_known if it's a divine gift
    item.orig_place  = get_packed_place();
    _origin_set_portal_vault(item);
    item.orig_monnum = -agent;
}

void origin_set_inventory(void (*oset)(item_def &item))
{
    for (int i = 0; i < ENDOFPACK; ++i)
        if (you.inv[i].defined())
            oset(you.inv[i]);
}

static int _first_corpse_monnum(const coord_def& where)
{
    // We could look for a corpse on this square and assume that the
    // items belonged to it, but that is unsatisfactory.

    // Actually, it would be easy to add the monster type to a corpse
    // (or to another item) by setting orig_monnum when the monster dies
    // (already done for unique monsters to get named zombies), but
    // personally, I rather like the way the player can't tell where an
    // item came from if he just finds it lying on the floor. (jpeg)
    return (0);
}

static std::string _milestone_rune(const item_def &item)
{
    return std::string("found ") + item.name(DESC_NOCAP_A) + ".";
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
        take_note(Note(NOTE_GET_ITEM, 0, 0, item.name(DESC_NOCAP_A).c_str(),
                       origin_desc(item).c_str()));
        item.flags |= ISFLAG_NOTED_GET;

        // If it's already fully identified when picked up, don't take
        // further notes.
        if (fully_identified(item))
            item.flags |= ISFLAG_NOTED_ID;
    }
}

void origin_set(const coord_def& where)
{
    int monnum = _first_corpse_monnum(where);
    unsigned short pplace = get_packed_place();
    for (stack_iterator si(where); si; ++si)
    {
        if (origin_known( *si ))
            continue;

        if (!si->orig_monnum)
            si->orig_monnum = static_cast<short>( monnum );
        si->orig_place  = pplace;
        _origin_set_portal_vault(*si);
        _milestone_check(*si);
    }
}

void origin_set_monstercorpse(item_def &item, const coord_def& where)
{
    item.orig_monnum = _first_corpse_monnum(where);
}

static void _origin_freeze(item_def &item, const coord_def& where)
{
    if (!origin_known(item))
    {
        if (!item.orig_monnum && where.x != -1 && where.y != -1)
            origin_set_monstercorpse(item, where);

        item.orig_place = get_packed_place();
        _origin_set_portal_vault(item);
        _check_note_item(item);
        _milestone_check(item);
    }
}

std::string origin_monster_name(const item_def &item)
{
    const int monnum = item.orig_monnum - 1;
    if (monnum == MONS_PLAYER_GHOST)
        return ("a player ghost");
    else if (monnum == MONS_PANDEMONIUM_DEMON)
        return ("a demon");
    return mons_type_name(monnum, DESC_NOCAP_A);
}

static std::string _origin_place_desc(const item_def &item)
{
    if (place_type(item.orig_place) == LEVEL_PORTAL_VAULT
        && item.props.exists(PORTAL_VAULT_ORIGIN_KEY))
    {
        return item.props[PORTAL_VAULT_ORIGIN_KEY].get_string();
    }

    return prep_branch_level_name(item.orig_place);
}

bool origin_describable(const item_def &item)
{
    return (origin_known(item)
            && (item.orig_place != 0xFFFFU || item.orig_monnum == -1)
            && (!is_stackable_item(item) || item_is_rune(item))
            && item.quantity == 1
            && item.base_type != OBJ_CORPSES
            && (item.base_type != OBJ_FOOD || item.sub_type != FOOD_CHUNK));
}

static std::string _article_it(const item_def &item)
{
    // "it" is always correct, since gloves and boots also come in pairs.
    return "it";
}

static bool _origin_is_original_equip(const item_def &item)
{
    return (item.orig_place == 0xFFFFU && item.orig_monnum == -IT_SRC_START);
}

bool origin_is_god_gift(const item_def& item, god_type *god)
{
    god_type junk;
    if (god == NULL)
        god = &junk;
    *god = GOD_NO_GOD;

    const int iorig = -item.orig_monnum;
    if (iorig > GOD_NO_GOD && iorig < NUM_GODS)
    {
        *god = static_cast<god_type>(iorig);
        return (true);
    }

    return (false);
}

bool origin_is_acquirement(const item_def& item, item_source_type *type)
{
    item_source_type junk;
    if (type == NULL)
        type = &junk;
    *type = IT_SRC_NONE;

    const int iorig = -item.orig_monnum;
    if (iorig == AQ_SCROLL || iorig == AQ_CARD_GENIE
        || iorig == AQ_WIZMODE)
    {
        *type = static_cast<item_source_type>(iorig);
        return (true);
    }

    return (false);
}

std::string origin_desc(const item_def &item)
{
    if (!origin_describable(item))
        return ("");

    if (_origin_is_original_equip(item))
        return "Original Equipment";

    std::string desc;
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
            case AQ_CARD_GENIE:
                desc += "You drew the Genie ";
                break;
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
        else if (item.orig_monnum - 1 == MONS_DANCING_WEAPON)
            desc += "You subdued it ";
        else
        {
            desc += "You took " + _article_it(item) + " off "
                    + origin_monster_name(item) + " ";
        }
    }
    else
        desc += "You found " + _article_it(item) + " ";

    desc += _origin_place_desc(item);
    return (desc);
}

bool pickup_single_item(int link, int qty)
{
    if (you.flight_mode() == FL_LEVITATE)
    {
        mpr("You can't reach the floor from up here.");
        return (false);
    }

    if (qty < 1 || qty > mitm[link].quantity)
        qty = mitm[link].quantity;

    iflags_t oldflags = mitm[link].flags;
    mitm[link].flags &= ~(ISFLAG_THROWN | ISFLAG_DROPPED);
    int num = move_item_to_player( link, qty );
    if (mitm[link].defined())
        mitm[link].flags = oldflags;

    if (num == -1)
    {
        mpr("You can't carry that many items.");
        learned_something_new(HINT_HEAVY_LOAD);
        return (false);
    }
    else if (num == 0)
    {
        mpr("You can't carry that much weight.");
        learned_something_new(HINT_HEAVY_LOAD);
        return (false);
    }

    return (true);
}

void pickup()
{
    int keyin = 'x';

    if (you.flight_mode() == FL_LEVITATE)
    {
        mpr("You can't reach the floor from up here.");
        return;
    }

    int o = you.visible_igrd(you.pos());
    const int num_nonsquelched = _count_nonsquelched_items(o);

    if (o == NON_ITEM)
    {
        mpr("There are no items here.");
    }
    else if (mitm[o].link == NON_ITEM)      // just one item?
    {
        // Deliberately allowing the player to pick up
        // a killed item here.
        pickup_single_item(o, mitm[o].quantity);
    }
    else if (Options.pickup_mode != -1
             && num_nonsquelched >= Options.pickup_mode)
    {
        _pickup_menu(o);
    }
    else
    {
        int next;
        mpr("There are several objects here.");
        std::string pickup_warning;
        while (o != NON_ITEM)
        {
            // Must save this because pickup can destroy the item.
            next = mitm[o].link;

            if (num_nonsquelched && _invisible_to_player(mitm[o]))
            {
                o = next;
                continue;
            }

            if (keyin != 'a')
            {
                std::string prompt = "Pick up %s? ("
#ifdef USE_TILE
                                     "Left-click to enter menu, or press "
#endif
                                     "y/n/a/*?g,/q/o)";

                mprf(MSGCH_PROMPT, prompt.c_str(),
                     get_menu_colour_prefix_tags(mitm[o],
                                                 DESC_NOCAP_A).c_str());

                mouse_control mc(MOUSE_MODE_MORE);
                keyin = getch();
            }

            if (keyin == '*' || keyin == '?' || keyin == ',' || keyin == 'g'
                || keyin == CK_MOUSE_CLICK)
            {
                _pickup_menu(o);
                break;
            }

            if (keyin == 'q' || key_is_escape(keyin) || keyin == 'o')
                break;

            if (keyin == 'y' || keyin == 'a')
            {
                int num_to_take = mitm[o].quantity;
                const iflags_t old_flags(mitm[o].flags);
                mitm[o].flags &= ~(ISFLAG_THROWN | ISFLAG_DROPPED);
                int result = move_item_to_player( o, num_to_take );

                if (result == 0 || result == -1)
                {
                    if (result == 0)
                        pickup_warning = "You can't carry that much weight.";
                    else
                        pickup_warning = "You can't carry that many items.";

                    mitm[o].flags = old_flags;
                }
            }

            o = next;
        }

        if (!pickup_warning.empty())
            mpr(pickup_warning.c_str());

        if (keyin == 'o')
            start_explore(Options.explore_greedy);
    }
}

bool is_stackable_item( const item_def &item )
{
    if (!item.defined())
        return (false);

    if (item.base_type == OBJ_MISSILES
        || item.base_type == OBJ_FOOD
        || item.base_type == OBJ_SCROLLS
        || item.base_type == OBJ_POTIONS
        || item.base_type == OBJ_UNKNOWN_II
        || item.base_type == OBJ_GOLD
        || (item.base_type == OBJ_MISCELLANY
            && item.sub_type == MISC_RUNE_OF_ZOT))
    {
        return (true);
    }

    return (false);
}

iflags_t ident_flags(const item_def &item)
{
    const iflags_t identmask = full_ident_mask(item);
    iflags_t flags = item.flags & identmask;

    if ((identmask & ISFLAG_KNOW_TYPE) && item_type_known(item))
        flags |= ISFLAG_KNOW_TYPE;

    return (flags);
}

bool items_similar(const item_def &item1, const item_def &item2, bool ignore_ident)
{
    // Base and sub-types must always be the same to stack.
    if (item1.base_type != item2.base_type || item1.sub_type != item2.sub_type)
        return (false);

    if (item1.base_type == OBJ_GOLD)
        return (true);

    // These classes also require pluses and special.
    if (item1.base_type == OBJ_WEAPONS         // only throwing weapons
        || item1.base_type == OBJ_MISSILES
        || item1.base_type == OBJ_MISCELLANY   // only runes
        || item1.base_type == OBJ_FOOD)        // chunks
    {
        if (item1.plus != item2.plus
            || item1.plus2 != item2.plus2
            || ((item1.base_type == OBJ_FOOD && item2.sub_type == FOOD_CHUNK) ?
                // Reject chunk merge if chunk ages differ by more than 5
                abs(item1.special - item2.special) > 5
                // Non-chunk item specials must match exactly.
                : item1.special != item2.special))
        {
            return (false);
        }
    }

    // Missiles need to be of the same brand, not just plusses.
    if (item1.base_type == OBJ_MISSILES
        && get_ammo_brand(item1) != get_ammo_brand(item2))
    {
        return (false);
    }

    // Check the ID flags.
    if (!ignore_ident && ident_flags(item1) != ident_flags(item2))
        return (false);

    // Check the non-ID flags, but ignore dropped, thrown, cosmetic,
    // and note flags. Also, whether item was in inventory before.
#define NON_IDENT_FLAGS ~(ISFLAG_IDENT_MASK | ISFLAG_COSMETIC_MASK | \
                          ISFLAG_DROPPED | ISFLAG_THROWN | \
                          ISFLAG_NOTED_ID | ISFLAG_NOTED_GET | \
                          ISFLAG_BEEN_IN_INV | ISFLAG_DROPPED_BY_ALLY)

    if ((item1.flags & NON_IDENT_FLAGS) != (item2.flags & NON_IDENT_FLAGS))
        return (false);

    if (item1.base_type == OBJ_POTIONS)
    {
        // Thanks to mummy cursing, we can have potions of decay
        // that don't look alike... so we don't stack potions
        // if either isn't identified and they look different.  -- bwr
        if (item1.plus != item2.plus
            && (!item_type_known(item1) || !item_type_known(item2)))
        {
            return (false);
        }
    }

    // The inscriptions can differ if one of them is blank, but if they
    // are differing non-blank inscriptions then don't stack.
    if (item1.inscription != item2.inscription
        && !item1.inscription.empty() && !item2.inscription.empty())
    {
        return (false);
    }

    return (true);
}

bool items_stack( const item_def &item1, const item_def &item2,
                  bool force_merge, bool ignore_ident )
{
    // Both items must be stackable.
    if (!force_merge
        && (!is_stackable_item( item1 ) || !is_stackable_item( item2 )))
    {
        return (false);
    }

    return items_similar(item1, item2, ignore_ident);
}

void merge_item_stacks(item_def &source, item_def &dest, int quant)
{
    if (quant == -1)
        quant = source.quantity;

    ASSERT(quant > 0 && quant <= source.quantity);

    if (is_blood_potion(source) && is_blood_potion(dest))
       merge_blood_potion_stacks(source, dest, quant);
}

static int _userdef_find_free_slot(const item_def &i)
{
#ifdef CLUA_BINDINGS
    int slot = -1;
    if (!clua.callfn("c_assign_invletter", "u>d", &i, &slot))
        return (-1);

    return (slot);
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
    if (i.slot >= 'a' && i.slot <= 'z'
        || i.slot >= 'A' && i.slot <= 'Z')
    {
        slot = letter_to_index(i.slot);
    }

    if (slotisfree(slot))
        return slot;

    int disliked = -1;
    if (i.base_type == OBJ_FOOD)
        disliked = 'e' - 'a';
    else if (i.base_type == OBJ_POTIONS)
        disliked = 'y' - 'a';

    if (!searchforward)
    {
        // This is the new default free slot search. We look for the last
        // available slot that does not leave a gap in the inventory.
        for (slot = ENDOFPACK - 1; slot >= 0; --slot)
        {
            if (you.inv[slot].defined())
            {
                if (slot + 1 < ENDOFPACK && !you.inv[slot + 1].defined()
                    && slot + 1 != disliked)
                {
                    return (slot + 1);
                }
            }
            else
            {
                if (slot + 1 < ENDOFPACK && you.inv[slot + 1].defined()
                    && slot != disliked)
                {
                    return (slot);
                }
            }
        }
    }

    // Either searchforward is true, or search backwards failed and
    // we re-try searching the oposite direction.

    // Return first free slot
    for (slot = 0; slot < ENDOFPACK; ++slot)
        if (slot != disliked && !you.inv[slot].defined())
            return slot;

    // If the least preferred slot is the only choice, so be it.
    if (disliked != -1 && !you.inv[disliked].defined())
        return disliked;

    return (-1);
#undef slotisfree
}

static void _got_item(item_def& item, int quant)
{
    seen_item(item);
    shopping_list.cull_identical_items(item);

    if (!item_is_rune(item))
        return;

    // Picking up the rune for the first time.
    if (!(item.flags & ISFLAG_BEEN_IN_INV))
    {
        if (item_is_unique_rune(item))
            you.attribute[ATTR_UNIQUE_RUNES] += quant;
        else if (item.plus == RUNE_ABYSSAL)
            you.attribute[ATTR_ABYSSAL_RUNES] += quant;
        else
            you.attribute[ATTR_DEMONIC_RUNES] += quant;

        if (you.religion == GOD_ASHENZARI)
        {
            mprf(MSGCH_GOD, "You learn the power of this rune.");
            // Important!  This should _not_ be scaled by bondage level, as
            // otherwise people would curse just before picking up.
            for (int i = 0; i < 10; i++)
                // do this in pieces because of the high piety taper
                gain_piety(1, 1);
        }
    }

    item.flags |= ISFLAG_BEEN_IN_INV;
    _check_note_item(item);
}

void note_inscribe_item(item_def &item)
{
    _autoinscribe_item(item);
    _origin_freeze(item, you.pos());
    you.attribute[ATTR_FRUIT_FOUND] |= item_fruit_mask(item);
    _check_note_item(item);
}

// Returns quantity of items moved into player's inventory and -1 if
// the player's inventory is full.
int move_item_to_player(int obj, int quant_got, bool quiet,
                        bool ignore_burden)
{
    if (item_is_stationary(mitm[obj]))
    {
        mpr("You cannot pick up the net that holds you!");
        // Fake a successful pickup (return 1), so we can continue to
        // pick up anything else that might be on this square.
        return (1);
    }

    int retval = quant_got;

    // Gold has no mass, so we handle it first.
    if (mitm[obj].base_type == OBJ_GOLD)
    {
        you.attribute[ATTR_GOLD_FOUND] += quant_got;
        you.add_gold(quant_got);
        dec_mitm_item_quantity(obj, quant_got);

        if (!quiet)
        {
            mprf("You now have %d gold piece%s.",
                 you.gold, you.gold != 1 ? "s" : "");
        }

        learned_something_new(HINT_SEEN_GOLD);

        you.turn_is_over = true;
        return (retval);
    }

    const int unit_mass = item_mass( mitm[obj] );
    if (quant_got > mitm[obj].quantity || quant_got <= 0)
        quant_got = mitm[obj].quantity;

    const int imass = unit_mass * quant_got;

    bool partial_pickup = false;

    if (!ignore_burden && (you.burden + imass > carrying_capacity()))
    {
        // calculate quantity we can actually pick up
        int part = (carrying_capacity() - you.burden) / unit_mass;

        if (part < 1)
            return (0);

        // only pickup 'part' items
        quant_got = part;
        partial_pickup = true;

        retval = part;
    }

    if (is_stackable_item( mitm[obj] ))
    {
        for (int m = 0; m < ENDOFPACK; m++)
        {
            if (items_stack( you.inv[m], mitm[obj], false, true ))
            {
                if (!quiet && partial_pickup)
                    mpr("You can only carry some of what is here.");

                _check_note_item(mitm[obj]);

                const bool floor_god_gift
                    = mitm[obj].inscription.find("god gift")
                      != std::string::npos;
                const bool inv_god_gift
                    = you.inv[m].inscription.find("god gift")
                      != std::string::npos;

                // If the object on the ground is inscribed, but not
                // the one in inventory, then the inventory object
                // picks up the other's inscription.
                if (!(mitm[obj].inscription).empty()
                    && you.inv[m].inscription.empty())
                {
                    you.inv[m].inscription = mitm[obj].inscription;
                }

                // Remove god gift inscription unless both items have it.
                if (floor_god_gift && !inv_god_gift
                    || inv_god_gift && !floor_god_gift)
                {
                    you.inv[m].inscription
                        = replace_all(you.inv[m].inscription,
                                      "god gift, ", "");
                    you.inv[m].inscription
                        = replace_all(you.inv[m].inscription,
                                      "god gift", "");
                }

                // If only one of the stacks is identified,
                // identify the other to a similar extent.
                if (ident_flags(mitm[obj]) != ident_flags(you.inv[m]))
                {
                    if (!quiet)
                        mpr("These items seem quite similar.");
                    mitm[obj].flags |=
                        ident_flags(you.inv[m]) & you.inv[m].flags;
                    you.inv[m].flags |=
                        ident_flags(mitm[obj]) & mitm[obj].flags;
                }

                merge_item_stacks(mitm[obj], you.inv[m], quant_got);

                inc_inv_item_quantity( m, quant_got );
                dec_mitm_item_quantity( obj, quant_got );
                burden_change();

                _got_item(mitm[obj], quant_got);

                if (!quiet)
                {
                    mpr(get_menu_colour_prefix_tags(you.inv[m],
                                                    DESC_INVENTORY).c_str());
                }
                you.turn_is_over = true;

                return (retval);
            }
        }
    }

    // Can't combine, check for slot space.
    if (inv_count() >= ENDOFPACK)
        return (-1);

    if (!quiet && partial_pickup)
        mpr("You can only carry some of what is here.");

    int freeslot = find_free_slot(mitm[obj]);
    if (freeslot < 0 || freeslot >= ENDOFPACK
        || you.inv[freeslot].defined())
    {
        // Something is terribly wrong.
        return (-1);
    }

    coord_def p = mitm[obj].pos;
    // If moving an item directly from a monster to the player without the
    // item having been on the grid, then it really isn't a position event.
    if (in_bounds(p))
    {
        dungeon_events.fire_position_event(
            dgn_event(DET_ITEM_PICKUP, p, 0, obj, -1), p);
    }
    item_def &item = you.inv[freeslot];
    // Copy item.
    item        = mitm[obj];
    item.link   = freeslot;
    item.pos.set(-1, -1);
    // Remove "dropped by ally" flag.
    item.flags &= ~(ISFLAG_DROPPED_BY_ALLY);

    if (!item.slot)
        item.slot = index_to_letter(item.link);

    note_inscribe_item(item);

    item.quantity = quant_got;
    if (is_blood_potion(mitm[obj]))
    {
        if (quant_got != mitm[obj].quantity)
        {
            // Remove oldest timers from original stack.
            for (int i = 0; i < quant_got; i++)
                remove_oldest_blood_potion(mitm[obj]);

            // ... and newest ones from picked up stack
            remove_newest_blood_potion(item);
        }
    }
    dec_mitm_item_quantity( obj, quant_got );
    you.m_quiver->on_inv_quantity_changed(freeslot, quant_got);
    burden_change();

    if (!quiet)
    {
        mpr(get_menu_colour_prefix_tags(you.inv[freeslot],
                                        DESC_INVENTORY).c_str());
    }
    if (Hints.hints_left)
    {
        taken_new_item(item.base_type);
        if (is_artefact(item) || get_equip_desc( item ) != ISFLAG_NO_DESC)
            learned_something_new(HINT_SEEN_RANDART);
    }

    if (item.base_type == OBJ_ORBS
        && you.char_direction == GDT_DESCENDING)
    {
        // Take a note!
        _check_note_item(item);

        if (!quiet)
            mpr("Now all you have to do is get back out of the dungeon!");
        you.char_direction = GDT_ASCENDING;
        xom_is_stimulated(255, XM_INTRIGUED);
    }

    if (item.base_type == OBJ_ORBS && you.level_type == LEVEL_DUNGEON)
        unset_branch_flags(BFLAG_HAS_ORB);

    _got_item(item, item.quantity);

    you.turn_is_over = true;

    return (retval);
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

// Moves mitm[obj] to p... will modify the value of obj to
// be the index of the final object (possibly different).
//
// Done this way in the hopes that it will be obvious from
// calling code that "obj" is possibly modified.
//
// Returns false on error or level full - cases where you
// keep the item.
bool move_item_to_grid( int *const obj, const coord_def& p, bool silent )
{
    ASSERT(in_bounds(p));

    int& ob(*obj);
    // Must be a valid reference to a valid object.
    if (ob == NON_ITEM || !mitm[ob].defined())
        return (false);

    item_def& item(mitm[ob]);

    if (feat_destroys_item(grd(p), mitm[ob], !silenced(p) && !silent))
    {
        item_was_destroyed(item, NON_MONSTER);
        destroy_item(ob);
        ob = NON_ITEM;

        return (true);
    }

    // If it's a stackable type...
    if (is_stackable_item( item ))
    {
        // Look for similar item to stack:
        for (stack_iterator si(p); si; ++si)
        {
            // Check if item already linked here -- don't want to unlink it.
            if (ob == si->index())
                return (false);

            if (items_stack( item, *si ))
            {
                // Add quantity to item already here, and dispose
                // of obj, while returning the found item. -- bwr
                inc_mitm_item_quantity( si->index(), item.quantity );
                merge_item_stacks(item, *si);
                destroy_item( ob );
                ob = si->index();
                return (true);
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

    ASSERT( ob != NON_ITEM );

    // Need to actually move object, so first unlink from old position.
    unlink_item( ob );

    // Move item to coord.
    item.pos = p;

    // Link item to top of list.
    item.link = igrd(p);
    igrd(p) = ob;

    if (item.base_type == OBJ_ORBS && you.level_type == LEVEL_DUNGEON)
        set_branch_flags(BFLAG_HAS_ORB);

    return (true);
}

void move_item_stack_to_grid( const coord_def& from, const coord_def& to )
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
bool copy_item_to_grid( const item_def &item, const coord_def& p,
                        int quant_drop, bool mark_dropped, bool silent )
{
    ASSERT(in_bounds(p));

    if (quant_drop == 0)
        return (false);

    if (feat_destroys_item(grd(p), item, !silenced(p) && !silent))
    {
        if (item.base_type == OBJ_BOOKS && item.sub_type != BOOK_MANUAL
            && item.sub_type != BOOK_DESTRUCTION)
        {
            destroy_spellbook(item);
        }

        item_was_destroyed(item, NON_MONSTER);

        return (true);
    }

    // default quant_drop == -1 => drop all
    if (quant_drop < 0)
        quant_drop = item.quantity;

    // Loop through items at current location.
    if (is_stackable_item( item ))
    {
        for (stack_iterator si(p); si; ++si)
        {
            if (items_stack( item, *si ))
            {
                inc_mitm_item_quantity( si->index(), quant_drop );
                item_def copy = item;
                merge_item_stacks(copy, *si, quant_drop);

                // If the items on the floor already have a nonzero slot,
                // leave it as such, otherwise set the slot.
                if (mark_dropped && !si->slot)
                    si->slot = index_to_letter(item.link);
                return (true);
            }
        }
    }

    // Item not found in current stack, add new item to top.
    int new_item_idx = get_item_slot(10);
    if (new_item_idx == NON_ITEM)
        return (false);
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

    move_item_to_grid( &new_item_idx, p, true );
    if (is_blood_potion(item)
        && item.quantity != quant_drop) // partial drop only
    {
        // Since only the oldest potions have been dropped,
        // remove the newest ones.
        remove_newest_blood_potion(new_item);
    }

    return (true);
}


//---------------------------------------------------------------
//
// move_top_item -- moves the top item of a stack to a new
// location.
//
//---------------------------------------------------------------
bool move_top_item( const coord_def &pos, const coord_def &dest )
{
    int item = igrd(pos);
    if (item == NON_ITEM)
        return (false);

    dungeon_events.fire_position_event(
        dgn_event(DET_ITEM_MOVED, pos, 0, item, -1, dest), pos);

    // Now move the item to its new possition...
    move_item_to_grid( &item, dest );

    return (true);
}

const item_def* top_item_at(const coord_def& where, bool allow_mimic_item)
{
    if (allow_mimic_item)
    {
        const monster* mon = monster_at(where);
        if (mon && mons_is_unknown_mimic(mon))
            return &get_mimic_item(mon);
    }

    const int link = you.visible_igrd(where);
    return (link == NON_ITEM) ? NULL : &mitm[link];
}

item_def *corpse_at(coord_def pos, int *num_corpses)
{
    item_def *corpse = NULL;
    if (num_corpses)
        *num_corpses = 0;
    // Determine how many corpses are available.
    for (stack_iterator si(pos, true); si; ++si)
    {
        if (item_is_corpse(*si))
        {
            if (!corpse)
            {
                corpse = &*si;
                if (!num_corpses)
                    return (corpse);
            }
            if (num_corpses)
                ++*num_corpses;
        }
    }
    return (corpse);
}

bool multiple_items_at(const coord_def& where, bool allow_mimic_item)
{
    int found_count = 0;

    if (allow_mimic_item)
    {
        const monster* mon = monster_at(where);
        if (mon && mons_is_unknown_mimic(mon))
            ++found_count;
    }

    for (stack_iterator si(where); si && found_count < 2; ++si)
        ++found_count;

    return (found_count > 1);
}

bool drop_item( int item_dropped, int quant_drop, bool try_offer )
{
    if (quant_drop < 0 || quant_drop > you.inv[item_dropped].quantity)
        quant_drop = you.inv[item_dropped].quantity;

    if (item_dropped == you.equip[EQ_LEFT_RING]
        || item_dropped == you.equip[EQ_RIGHT_RING]
        || item_dropped == you.equip[EQ_AMULET])
    {
        if (!Options.easy_unequip)
        {
            mpr("You will have to take that off first.");
            return (false);
        }

        if (remove_ring( item_dropped, true ))
            start_delay( DELAY_DROP_ITEM, 1, item_dropped, 1 );

        return (false);
    }

    if (item_dropped == you.equip[EQ_WEAPON]
        && you.inv[item_dropped].base_type == OBJ_WEAPONS
        && you.inv[item_dropped] .cursed())
    {
        mpr("That object is stuck to you!");
        return (false);
    }

    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_ARMOUR; i++)
    {
        if (item_dropped == you.equip[i] && you.equip[i] != -1)
        {
            if (!Options.easy_unequip)
            {
                mpr("You will have to take that off first.");
            }
            else if (check_warning_inscriptions(you.inv[item_dropped],
                                                OPER_TAKEOFF))
            {
                // If we take off the item, cue up the item being dropped
                if (takeoff_armour( item_dropped ))
                {
                    start_delay( DELAY_DROP_ITEM, 1, item_dropped, 1 );
                    you.turn_is_over = false; // turn happens later
                }
            }

            // Regardless, we want to return here because either we're
            // aborting the drop, or the drop is delayed until after
            // the armour is removed. -- bwr
            return (false);
        }
    }

    // [ds] easy_unequip does not apply to weapons.
    //
    // Unwield needs to be done before copy in order to clear things
    // like temporary brands. -- bwr
    if (item_dropped == you.equip[EQ_WEAPON]
        && quant_drop >= you.inv[item_dropped].quantity)
    {
        if (!wield_weapon(true, PROMPT_GOT_SPECIAL))
            return (false);
    }

    const dungeon_feature_type my_grid = grd(you.pos());

    if (!copy_item_to_grid( you.inv[item_dropped],
                            you.pos(), quant_drop, true, true ))
    {
        mpr("Too many items on this level, not dropping the item.");
        return (false);
    }

    mprf("You drop %s.",
         quant_name(you.inv[item_dropped], quant_drop, DESC_NOCAP_A).c_str());

    bool quiet = silenced(you.pos());

    // If you drop an item in as a merfolk, it is below the water line and
    // makes no noise falling.
    if (you.swimming())
        quiet = true;

    if (feat_destroys_item(my_grid, you.inv[item_dropped], !quiet))
        ;
    else if (strstr(you.inv[item_dropped].inscription.c_str(), "=s") != 0)
        StashTrack.add_stash();

    if (is_blood_potion(you.inv[item_dropped])
        && you.inv[item_dropped].quantity != quant_drop)
    {
        // Oldest potions have been dropped.
        for (int i = 0; i < quant_drop; i++)
            remove_oldest_blood_potion(you.inv[item_dropped]);
    }
    dec_inv_item_quantity( item_dropped, quant_drop );
    you.turn_is_over = true;

    if (try_offer
        && you.religion != GOD_NO_GOD
        && you.duration[DUR_PRAYER]
        && feat_altar_god(grd(you.pos())) == you.religion)
    {
        offer_items();
    }

    return (true);
}

static std::string _drop_menu_invstatus(const Menu *menu)
{
    const int cap = carrying_capacity(BS_UNENCUMBERED);

    std::string s_newweight;
    std::vector<MenuEntry*> se = menu->selected_entries();
    if (!se.empty())
    {
        int newweight = you.burden;
        for (int i = 0, size = se.size(); i < size; ++i)
        {
            const item_def *item = static_cast<item_def *>( se[i]->data );
            newweight -= item_mass(*item) * se[i]->selected_qty;
        }

        s_newweight = make_stringf(">%.0f", newweight * BURDEN_TO_AUM);
    }

    return (make_stringf("(Inv: %.0f%s/%.0f aum)",
             you.burden * BURDEN_TO_AUM,
             s_newweight.c_str(),
             cap * BURDEN_TO_AUM));
}

static std::string _drop_menu_title(const Menu *menu, const std::string &oldt)
{
    std::string res = _drop_menu_invstatus(menu) + " " + oldt;
    if (menu->is_set( MF_MULTISELECT ))
        res = "[Multidrop] " + res;

    return (res);
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

static std::string _drop_selitem_text( const std::vector<MenuEntry*> *s )
{
    bool extraturns = false;

    if (s->empty())
        return "";

    for (int i = 0, size = s->size(); i < size; ++i)
    {
        const item_def *item = static_cast<item_def *>( (*s)[i]->data );
        const int eq = get_equip_slot(item);
        if (eq > EQ_WEAPON && eq < NUM_EQUIP)
        {
            extraturns = true;
            break;
        }
    }

    return (make_stringf( " (%u%s turn%s)",
                s->size(),
                extraturns? "+" : "",
                s->size() > 1? "s" : "" ));
}

std::vector<SelItem> items_for_multidrop;

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
        return (slot1 < slot2);
    else if (slot1 != -1 && slot2 == -1)
        return (false);
    else if (slot2 != -1 && slot1 == -1)
        return (true);

    return (first.slot < second.slot);
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

    std::vector<SelItem> tmp_items;
    tmp_items = prompt_invent_items( "Drop what?  (Press _ for help.)", MT_DROP,
                                     -1, _drop_menu_title, true, true, 0,
                                     &Options.drop_filter, _drop_selitem_text,
                                     &items_for_multidrop );

    if (tmp_items.empty())
    {
        canned_msg( MSG_OK );
        return;
    }

    // Sort the dropped items so we don't see weird behaviour when
    // dropping a worn robe before a cloak (old behaviour: remove
    // cloak, remove robe, wear cloak, drop robe, remove cloak, drop
    // cloak).
    std::sort( tmp_items.begin(), tmp_items.end(), _drop_item_order );

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
        canned_msg( MSG_OK );
        return;
    }

    if (items_for_multidrop.size() == 1) // only one item
    {
        drop_item( items_for_multidrop[0].slot,
                   items_for_multidrop[0].quantity,
                   true );
        items_for_multidrop.clear();
        you.turn_is_over = true;
    }
    else
        start_delay( DELAY_MULTIDROP, items_for_multidrop.size() );
}

static void _autoinscribe_item( item_def& item )
{
    // If there's an inscription already, do nothing - except
    // for automatically generated inscriptions
    if (!item.inscription.empty() && item.inscription != "god gift")
        return;
    const std::string old_inscription = item.inscription;
    item.inscription.clear();

    std::string iname = _autopickup_item_name(item);

    for (unsigned i = 0; i < Options.autoinscriptions.size(); ++i)
    {
        if (Options.autoinscriptions[i].first.matches(iname))
        {
            // Don't autoinscribe dropped items on ground with
            // "=g".  If the item matches a rule which adds "=g",
            // "=g" got added to it before it was dropped, and
            // then the user explictly removed it because they
            // don't want to autopickup it again.
            std::string str = Options.autoinscriptions[i].second;
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
        _autoinscribe_item( *si );
}

static void _autoinscribe_inventory()
{
    for (int i = 0; i < ENDOFPACK; i++)
        if (you.inv[i].defined())
            _autoinscribe_item( you.inv[i] );
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

static inline std::string _autopickup_item_name(const item_def &item)
{
    return userdef_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item, true)
           + menu_colour_item_prefix(item, false) + " "
           + item.name(DESC_PLAIN);
}

static bool _is_option_autopickup(const item_def &item, std::string &iname)
{
    if (iname.empty())
        iname = _autopickup_item_name(item);

    for (unsigned i = 0, size = Options.force_autopickup.size(); i < size; ++i)
        if (Options.force_autopickup[i].first.matches(iname))
            return Options.force_autopickup[i].second;

#ifdef CLUA_BINDINGS
    bool res = clua.callbooleanfn(false, "ch_force_autopickup", "is",
                                        &item, iname.c_str());
    if (!clua.error.empty())
        mprf(MSGCH_ERROR, "ch_force_autopickup failed: %s",
             clua.error.c_str());

    if (res)
        return (true);

    res = clua.callbooleanfn(false, "ch_deny_autopickup", "is",
                             &item, iname.c_str());
    if (!clua.error.empty())
        mprf(MSGCH_ERROR, "ch_deny_autopickup failed: %s",
             clua.error.c_str());

    if (res)
        return (false);
#endif

    return (Options.autopickups & (1L << item.base_type));
}

bool item_needs_autopickup(const item_def &item)
{
    if (item_is_stationary(item))
        return (false);

    if (item.inscription.find("=g") != std::string::npos)
        return (true);

    if ((item.flags & ISFLAG_THROWN) && Options.pickup_thrown)
        return (true);

    if ((item.flags & ISFLAG_DROPPED) && !Options.pickup_dropped)
        return (false);

    std::string itemname;
    return _is_option_autopickup(item, itemname);
}

bool can_autopickup()
{
    // [ds] Checking for autopickups == 0 is a bad idea because
    // autopickup is still possible with inscriptions and
    // pickup_thrown.
    if (Options.autopickup_on <= 0)
        return (false);

    if (you.flight_mode() == FL_LEVITATE)
        return (false);

    if (!i_feel_safe())
        return (false);

    return (true);
}

typedef bool (*item_comparer)(const item_def& pickup_item,
                              const item_def& inv_item);

static bool _identical_types(const item_def& pickup_item,
                             const item_def& inv_item)
{
    return ((pickup_item.base_type == inv_item.base_type)
            && (pickup_item.sub_type == inv_item.sub_type));
}

static bool _edible_food(const item_def& pickup_item,
                         const item_def& inv_item)
{
    return (inv_item.base_type == OBJ_FOOD && !is_inedible(inv_item));
}

static bool _similar_equip(const item_def& pickup_item,
                           const item_def& inv_item)
{
    const equipment_type inv_slot = get_item_slot(inv_item);

    if (inv_slot == EQ_NONE)
        return (false);

    // If it's an unequipped cursed item the player might be looking
    // for a replacement.
    if (item_known_cursed(inv_item) && !item_is_equipped(inv_item))
        return (false);

    if (get_item_slot(pickup_item) != inv_slot)
        return (false);

    // Just filling the same slot is enough for armour to be considered
    // similar.
    if (pickup_item.base_type == OBJ_ARMOUR)
        return (true);

    // Launchers of the same type are similar.
    if ((pickup_item.sub_type >= WPN_BLOWGUN
         && pickup_item.sub_type <= WPN_LONGBOW)
             || pickup_item.sub_type == WPN_SLING)
    {
        return (pickup_item.sub_type != inv_item.sub_type);
    }

    return ((weapon_skill(pickup_item) == weapon_skill(inv_item))
            && (get_damage_type(pickup_item) == get_damage_type(inv_item)));
}

static bool _similar_wands(const item_def& pickup_item,
                           const item_def& inv_item)
{
    if (inv_item.base_type != OBJ_WANDS)
        return (false);

    if (pickup_item.sub_type != inv_item.sub_type)
        return (false);

    // Not similar if wand in inventory is known to be empty.
    return (inv_item.plus2 != ZAPCOUNT_EMPTY
            || (item_ident(inv_item, ISFLAG_KNOW_PLUSES)
                && inv_item.plus > 0));
}

static bool _similar_jewellery(const item_def& pickup_item,
                               const item_def& inv_item)
{
    if (inv_item.base_type != OBJ_JEWELLERY)
        return (false);

    if (pickup_item.sub_type != inv_item.sub_type)
        return (false);

    // For jewellery of the same sub-type, unidentified jewellery is
    // always considered similar, as is identified jewellery whose
    // effect doesn't stack.
    return (!item_type_known(inv_item)
            || (!jewellery_is_amulet(inv_item)
                && !ring_has_stackable_effect(inv_item)));
}

static bool _item_different_than_inv(const item_def& pickup_item,
                                     item_comparer comparer)
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        const item_def& inv_item(you.inv[i]);

        if (!inv_item.defined())
            continue;

        if ( (*comparer)(pickup_item, inv_item) )
            return (false);
    }

    return (true);
}

static bool _interesting_explore_pickup(const item_def& item)
{
    if (!(Options.explore_stop & ES_GREEDY_PICKUP_MASK))
        return (false);

    if (item.base_type == OBJ_GOLD)
        return (Options.explore_stop & ES_GREEDY_PICKUP_GOLD);

    if ((Options.explore_stop & ES_GREEDY_PICKUP_THROWN)
        && (item.flags & ISFLAG_THROWN))
    {
        return (true);
    }

    std::vector<text_pattern> &ignore = Options.explore_stop_pickup_ignore;
    if (!ignore.empty())
    {
        const std::string name = item.name(DESC_PLAIN);

        for (unsigned int i = 0; i < ignore.size(); i++)
            if (ignore[i].matches(name))
                return (false);
    }

    if (!(Options.explore_stop & ES_GREEDY_PICKUP_SMART))
        return (true);
    // "Smart" code follows.

    // If ES_GREEDY_PICKUP_THROWN isn't set, then smart greedy pickup
    // will ignore thrown items.
    if (item.flags & ISFLAG_THROWN)
        return (false);

    if (is_artefact(item))
        return (true);

    // Possbible ego items.
    if (!item_type_known(item) & (item.flags & ISFLAG_COSMETIC_MASK))
        return (true);

    switch(item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_MISSILES:
    case OBJ_ARMOUR:
        // Ego items.
        if (item.special != 0)
            return (true);
        break;

    default:
        break;
    }

    switch(item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_ARMOUR:
        return _item_different_than_inv(item, _similar_equip);

    case OBJ_WANDS:
        return _item_different_than_inv(item, _similar_wands);

    case OBJ_JEWELLERY:
        return _item_different_than_inv(item, _similar_jewellery);

    case OBJ_FOOD:
        if (you.religion == GOD_FEDHAS && is_fruit(item))
            return (true);

        if (is_inedible(item))
            return (false);

        // Interesting if we don't have any other edible food.
        return _item_different_than_inv(item, _edible_food);

    case OBJ_STAVES:
        // Rods are always interesting, even if you already have one of
        // the same type, since each rod has its own mana.
        if (item_is_rod(item))
            return (true);

        // Intentional fall-through.
    case OBJ_MISCELLANY:
        // Runes are always interesting.
        if (item_is_rune(item))
            return (true);

        // Decks always start out unidentified.
        if (is_deck(item))
            return (true);

        // Intentional fall-through.
    case OBJ_SCROLLS:
    case OBJ_POTIONS:
        // Item is boring only if there's an identical one in inventory.
        return _item_different_than_inv(item, _identical_types);

    case OBJ_BOOKS:
        // Books always start out unidentified.
        return (true);

    case OBJ_ORBS:
        // Orb is always interesting.
        return (true);

    default:
        break;
    }

    return (false);
}

static void _do_autopickup()
{
    bool did_pickup     = false;
    int  n_did_pickup   = 0;
    int  n_tried_pickup = 0;

    will_autopickup = false;

    if (!can_autopickup())
    {
        item_check(false);
        return;
    }

    int o = you.visible_igrd(you.pos());

    std::string pickup_warning;
    while (o != NON_ITEM)
    {
        const int next = mitm[o].link;

        if (item_needs_autopickup(mitm[o]))
        {
            int num_to_take = mitm[o].quantity;
            if (Options.autopickup_no_burden && item_mass(mitm[o]) != 0)
            {
                int num_can_take =
                    (carrying_capacity(you.burden_state) - you.burden) /
                        item_mass(mitm[o]);

                if (num_can_take < num_to_take)
                {
                    if (!n_tried_pickup)
                    {
                        mpr("You can't pick everything up without burdening "
                            "yourself.");
                    }
                    n_tried_pickup++;
                    num_to_take = num_can_take;
                }

                if (num_can_take == 0)
                {
                    o = next;
                    continue;
                }
            }

            // Do this before it's picked up, otherwise the picked up
            // item will be in inventory and _interesting_explore_pickup()
            // will always return false.
            const bool interesting_pickup
                = _interesting_explore_pickup(mitm[o]);

            const iflags_t iflags(mitm[o].flags);
            mitm[o].flags &= ~(ISFLAG_THROWN | ISFLAG_DROPPED);

            const int result = move_item_to_player(o, num_to_take);

            if (result == 0 || result == -1)
            {
                n_tried_pickup++;
                if (result == 0)
                    pickup_warning = "You can't carry any more.";
                else
                    pickup_warning = "Your pack is full.";
                mitm[o].flags = iflags;
            }
            else
            {
                did_pickup = true;
                if (interesting_pickup)
                    n_did_pickup++;
            }
        }

        o = next;
    }

    if (!pickup_warning.empty())
        mpr(pickup_warning.c_str());

    if (did_pickup)
        you.turn_is_over = true;

    item_check(false);

    explore_pickup_event(n_did_pickup, n_tried_pickup);
}

void autopickup()
{
    _autoinscribe_floor_items();
    _do_autopickup();
}

int inv_count(void)
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
                    && si->base_type == cls && si->sub_type == sub_type)
                {
                    return (& (*si));
                }

    return (NULL);
}

int item_on_floor(const item_def &item, const coord_def& where)
{
    // Check if the item is on the floor and reachable.
    for (int link = igrd(where); link != NON_ITEM; link = mitm[link].link)
        if (&mitm[link] == &item)
            return (link);

    return (NON_ITEM);
}

static bool _find_subtype_by_name(item_def &item,
                                  object_class_type base_type, int ntypes,
                                  const std::string &name)
{
    // In order to get the sub-type, we'll fill out the base type...
    // then we're going to iterate over all possible subtype values
    // and see if we get a winner. -- bwr

    item.base_type = base_type;
    item.sub_type  = OBJ_RANDOM;
    item.plus      = 0;
    item.plus2     = 0;
    item.special   = 0;
    item.flags     = 0;
    item.quantity  = 1;
    // Don't use set_ident_flags in order to avoid triggering notes.
    // FIXME - is this the proper solution?
    item.flags |= (ISFLAG_KNOW_TYPE | ISFLAG_KNOW_PROPERTIES);

    int type_wanted = -1;

    for (int i = 0; i < ntypes; i++)
    {
        item.sub_type = i;

        int npluses = 1;
        if (base_type == OBJ_BOOKS && i == BOOK_MANUAL)
            npluses = NUM_SKILLS;
        else if (base_type == OBJ_MISCELLANY && i == MISC_RUNE_OF_ZOT)
            npluses = NUM_RUNE_TYPES;

        for (int j = 0; j < npluses; ++j)
        {
            item.plus = j;

            if (name == lowercase_string(item.name(DESC_PLAIN)))
            {
                type_wanted = i;
                i = ntypes;
                break;
            }
        }
    }

    if (type_wanted != -1)
        item.sub_type = type_wanted;
    else
        item.sub_type = OBJ_RANDOM;

    return (item.sub_type != OBJ_RANDOM);
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
        0,              // unknown I
        NUM_SCROLLS,
        NUM_JEWELLERY,
        NUM_POTIONS,
        0,              // unknown II
        NUM_BOOKS,
        NUM_STAVES,
        1,              // Orbs         -- only one
        NUM_MISCELLANY,
        -1,              // corpses     -- handled specially
        1,              // gold         -- handled specially
        0,              // "gemstones"  -- no items of type
    };
    COMPILE_CHECK(sizeof(max_subtype)/sizeof(int) == NUM_OBJECT_CLASSES, c1);

    ASSERT(base_type < NUM_OBJECT_CLASSES);

    return (max_subtype[base_type]);
}

// Returns an incomplete item_def with base_type and sub_type set correctly
// for the given item name. If the name is not found, sets sub_type to
// OBJ_RANDOM.
item_def find_item_type(object_class_type base_type, std::string name)
{
    item_def item;
    item.base_type = OBJ_RANDOM;
    item.sub_type  = OBJ_RANDOM;
    lowercase(name);

    if (base_type == OBJ_RANDOM || base_type == OBJ_UNASSIGNED)
        base_type = OBJ_UNASSIGNED;

    if (base_type == OBJ_UNASSIGNED)
    {
        for (unsigned i = 0; i < NUM_OBJECT_CLASSES; ++i)
        {
            object_class_type cls = static_cast<object_class_type>(i);
            if (get_max_subtype(cls) == 0)
                continue;

            if (_find_subtype_by_name(item, cls, get_max_subtype(cls), name))
            {
                break;
            }
        }
    }
    else
    {
        _find_subtype_by_name(item, base_type,
                              get_max_subtype(base_type), name);
    }

    return (item);
}

equipment_type item_equip_slot(const item_def& item)
{
    if (!in_inventory(item))
        return (EQ_NONE);

    for (int i = 0; i < NUM_EQUIP; i++)
        if (item.link == you.equip[i])
            return (static_cast<equipment_type>(i));

    return (EQ_NONE);
}

// Includes melded items.
bool item_is_equipped(const item_def &item, bool quiver_too)
{
    return (item_equip_slot(item) != EQ_NONE
            || quiver_too && in_inventory(item)
               && item.link == you.m_quiver->get_fire_item());
}

bool item_is_melded(const item_def& item)
{
    equipment_type eq = item_equip_slot(item);
    return (eq != EQ_NONE && you.melded[eq]);
}

////////////////////////////////////////////////////////////////////////
// item_def functions.

bool item_def::has_spells() const
{
    return ((base_type == OBJ_BOOKS && item_type_known(*this)
               && sub_type != BOOK_DESTRUCTION
               && sub_type != BOOK_MANUAL)
            || count_staff_spells(*this, true) > 1);
}

int item_def::book_number() const
{
    return (base_type == OBJ_BOOKS  ? sub_type                             :
            base_type == OBJ_STAVES ? sub_type + NUM_BOOKS - STAFF_FIRST_ROD + 1
                                    : -1);
}

bool item_def::cursed() const
{
    return (flags & ISFLAG_CURSED);
}

bool item_def::launched_by(const item_def &launcher) const
{
    if (base_type != OBJ_MISSILES)
        return (false);
    const missile_type mt = fires_ammo_type(launcher);
    return (sub_type == mt || (mt == MI_STONE && sub_type == MI_SLING_BULLET));
}

zap_type item_def::zap() const
{
    if (base_type != OBJ_WANDS)
        return (ZAP_DEBUGGING_RAY);

    zap_type result = ZAP_DEBUGGING_RAY;
    switch (static_cast<wand_type>(sub_type))
    {
    case WAND_FLAME:           result = ZAP_FLAME;           break;
    case WAND_FROST:           result = ZAP_FROST;           break;
    case WAND_SLOWING:         result = ZAP_SLOWING;         break;
    case WAND_HASTING:         result = ZAP_HASTING;         break;
    case WAND_MAGIC_DARTS:     result = ZAP_MAGIC_DARTS;     break;
    case WAND_HEALING:         result = ZAP_HEALING;         break;
    case WAND_PARALYSIS:       result = ZAP_PARALYSIS;       break;
    case WAND_FIRE:            result = ZAP_FIRE;            break;
    case WAND_COLD:            result = ZAP_COLD;            break;
    case WAND_CONFUSION:       result = ZAP_CONFUSION;       break;
    case WAND_INVISIBILITY:    result = ZAP_INVISIBILITY;    break;
    case WAND_DIGGING:         result = ZAP_DIGGING;         break;
    case WAND_FIREBALL:        result = ZAP_FIREBALL;        break;
    case WAND_TELEPORTATION:   result = ZAP_TELEPORTATION;   break;
    case WAND_LIGHTNING:       result = ZAP_LIGHTNING;       break;
    case WAND_POLYMORPH_OTHER: result = ZAP_POLYMORPH_OTHER; break;
    case WAND_ENSLAVEMENT:     result = ZAP_ENSLAVEMENT;     break;
    case WAND_DRAINING:        result = ZAP_NEGATIVE_ENERGY; break;
    case WAND_DISINTEGRATION:  result = ZAP_DISINTEGRATION;  break;
    case WAND_RANDOM_EFFECTS:
        result = static_cast<zap_type>(random2(ZAP_LAST_RANDOM + 1));
        if (one_chance_in(20))
            result = ZAP_NEGATIVE_ENERGY;
        if (one_chance_in(17))
            result = ZAP_ENSLAVEMENT;
        break;

    case NUM_WANDS: break;
    }
    return (result);
}

int item_def::index() const
{
    return (this - mitm.buffer());
}

int item_def::armour_rating() const
{
    if (!this->defined() || base_type != OBJ_ARMOUR)
        return (0);

    return (property(*this, PARM_AC) + plus);
}

monster* item_def::holding_monster() const
{
    if (!pos.equals(-2, -2))
        return (NULL);
    const int midx = link - NON_ITEM - 1;
    if (invalid_monster_index(midx))
        return (NULL);

    return (&menv[midx]);
}

void item_def::set_holding_monster(int midx)
{
    ASSERT(midx != NON_MONSTER);
    pos.set(-2, -2);
    link = NON_ITEM + 1 + midx;
}

bool item_def::held_by_monster() const
{
    return (pos.equals(-2, -2) && !invalid_monster_index(link - NON_ITEM - 1));
}

// Note:  This function is to isolate all the checks to see if
//        an item is valid (often just checking the quantity).
//
//        It shouldn't be used a a substitute for those cases
//        which actually want to check the quantity (as the
//        rules for unused objects might change).
bool item_def::defined() const
{
    return (base_type != OBJ_UNASSIGNED && quantity > 0);
}

// Checks whether the item is actually a good one.
// TODO: check brands, etc.
bool item_def::is_valid() const
{
    if (!defined())
        return (false);
    const int max_sub = get_max_subtype(base_type);
    if (max_sub != -1 && sub_type >= max_sub)
        return (false);
    if (colour == 0)
        return (false); // No black items.
    return (true);
}

// The Orb of Zot and unique runes are considered critical.
bool item_def::is_critical() const
{
    if (!defined())
        return (false);

    if (base_type == OBJ_ORBS)
        return (true);

    return (base_type == OBJ_MISCELLANY
            && sub_type == MISC_RUNE_OF_ZOT
            && plus != RUNE_DEMONIC
            && plus != RUNE_ABYSSAL);
}

// Is item something that no one would usually bother enchanting?
bool item_def::is_mundane() const
{
    switch (base_type)
    {
    case OBJ_WEAPONS:
        if (sub_type == WPN_CLUB
            || sub_type == WPN_GIANT_CLUB
            || sub_type == WPN_GIANT_SPIKED_CLUB
            || sub_type == WPN_KNIFE)
        {
            return (true);
        }
        break;

    case OBJ_ARMOUR:
        if (sub_type == ARM_ANIMAL_SKIN)
            return (true);
        break;

    default:
        break;
    }

    return (false);
}

static void _rune_from_specs(const char* _specs, item_def &item)
{
    char specs[80];
    char obj_name[ ITEMNAME_SIZE ];

    item.sub_type = MISC_RUNE_OF_ZOT;

    if (strstr(_specs, "rune of zot"))
        strlcpy(specs, _specs, std::min(strlen(_specs) - strlen(" of zot") + 1,
                                        sizeof(specs)));
    else
        strlcpy(specs, _specs, sizeof(specs));

    if (strlen(specs) > 4)
    {
        for (int i = 0; i < NUM_RUNE_TYPES; ++i)
        {
            item.plus = i;

            strlcpy(obj_name, item.name(DESC_PLAIN).c_str(), sizeof(obj_name));

            if (strstr(strlwr(obj_name), specs))
                return;
        }
    }

    while (true)
    {
        mpr("[a] iron       [b] obsidian [c] icy      [d] bone     [e] slimy    [f] silver",
            MSGCH_PROMPT);
        mpr("[g] serpentine [h] elven    [i] golden   [j] decaying [k] barnacle [l] demonic",
            MSGCH_PROMPT);
        mpr("[m] abyssal    [n] glowing  [o] magical  [p] fiery    [q] dark     [r] gossamer",
            MSGCH_PROMPT);
        mpr("[s] mossy      [t] buggy",
            MSGCH_PROMPT);
        mpr("Which rune (ESC to exit)? ", MSGCH_PROMPT);

        int keyin = tolower( get_ch() );

        if (key_is_escape(keyin) || keyin == ' '
            || keyin == '\r' || keyin == '\n')
        {
            canned_msg( MSG_OK );
            item.base_type = OBJ_UNASSIGNED;
            return;
        }

        if (keyin < 'a' || keyin > 'r')
            continue;

        rune_type types[] = {
            RUNE_DIS,
            RUNE_GEHENNA,
            RUNE_COCYTUS,
            RUNE_TARTARUS,
            RUNE_SLIME_PITS,
            RUNE_VAULTS,
            RUNE_SNAKE_PIT,
            RUNE_ELVEN_HALLS,
            RUNE_TOMB,
            RUNE_SWAMP,
            RUNE_SHOALS,

            RUNE_DEMONIC,
            RUNE_ABYSSAL,

            RUNE_MNOLEG,
            RUNE_LOM_LOBON,
            RUNE_CEREBOV,
            RUNE_GLOORX_VLOQ,

            RUNE_SPIDER_NEST,
            RUNE_FOREST,
            NUM_RUNE_TYPES
        };

        item.plus = types[keyin - 'a'];

        return;
    }
}

static void _deck_from_specs(const char* _specs, item_def &item)
{
    std::string specs    = _specs;
    std::string type_str = "";

    trim_string(specs);

    if (specs.find(" of ") != std::string::npos)
    {
        type_str = specs.substr(specs.find(" of ") + 4);

        if (type_str.find("card") != std::string::npos
            || type_str.find("deck") != std::string::npos)
        {
            type_str = "";
        }

        trim_string(type_str);
    }

    misc_item_type types[] = {
        MISC_DECK_OF_ESCAPE,
        MISC_DECK_OF_DESTRUCTION,
        MISC_DECK_OF_DUNGEONS,
        MISC_DECK_OF_SUMMONING,
        MISC_DECK_OF_WONDERS,
        MISC_DECK_OF_PUNISHMENT,
        MISC_DECK_OF_WAR,
        MISC_DECK_OF_CHANGES,
        MISC_DECK_OF_DEFENCE,
        NUM_MISCELLANY
    };

    item.special  = DECK_RARITY_COMMON;
    item.sub_type = NUM_MISCELLANY;

    if (!type_str.empty())
    {
        for (int i = 0; types[i] != NUM_MISCELLANY; ++i)
        {
            item.sub_type = types[i];
            item.plus     = 1;
            init_deck(item);
            // Remove "plain " from front.
            std::string name = item.name(DESC_PLAIN).substr(6);
            item.props.clear();

            if (name.find(type_str) != std::string::npos)
                break;
        }
    }

    if (item.sub_type == NUM_MISCELLANY)
    {
        while (true)
        {
            mpr(
"[a] escape     [b] destruction [c] dungeons [d] summoning [e] wonders",
                MSGCH_PROMPT);
            mpr(
"[f] punishment [g] war         [h] changes  [i] defence",
                MSGCH_PROMPT);
            mpr("Which deck (ESC to exit)? ");

            const int keyin = tolower( get_ch() );

            if (key_is_escape(keyin) || keyin == ' '
                || keyin == '\r' || keyin == '\n')
            {
                canned_msg( MSG_OK );
                item.base_type = OBJ_UNASSIGNED;
                return;
            }

            if (keyin < 'a' || keyin > 'i')
                continue;

            item.sub_type = types[keyin - 'a'];
            break;
        }
    }

    const char* rarities[] = {
        "plain",
        "ornate",
        "legendary",
        NULL
    };

    int rarity_val = -1;

    for (int i = 0; rarities[i] != NULL; ++i)
        if (specs.find(rarities[i]) != std::string::npos)
        {
            rarity_val = i;
            break;
        }

    if (rarity_val == -1)
    {
        while (true)
        {
            mpr("[a] plain [b] ornate [c] legendary? (ESC to exit)",
                MSGCH_PROMPT);

            int keyin = tolower( get_ch() );

            if (key_is_escape(keyin) || keyin == ' '
                || keyin == '\r' || keyin == '\n')
            {
                canned_msg( MSG_OK );
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

    int num = debug_prompt_for_int("How many cards? ", false);

    if (num <= 0)
    {
        canned_msg( MSG_OK );
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
    else if (strstr(specs, "deck"))
        _deck_from_specs(specs, item);
}

static bool _book_from_spell(const char* specs, item_def &item)
{
    spell_type type = spell_by_name(specs, true);

    if (type == SPELL_NO_SPELL)
        return (false);

    for (int i = 0; i < NUM_FIXED_BOOKS; ++i)
        for (int j = 0; j < 8; ++j)
            if (which_spell_in_book(i, j) == type)
            {
                item.sub_type = i;
                return (true);
            }

    return (false);
}

bool get_item_by_name(item_def *item, char* specs,
                      object_class_type class_wanted, bool create_for_real)
{
    char           obj_name[ ITEMNAME_SIZE ];
    char*          ptr;
    int            best_index;
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
            // Rune or deck creation canceled, clean up item->
            return (false);
        }
    }

    if (!item->sub_type)
    {
        type_wanted = -1;
        best_index  = 10000;

        for (int i = 0; i < get_max_subtype(item->base_type); ++i)
        {
            item->sub_type = i;
            strlcpy(obj_name, item->name(DESC_PLAIN).c_str(), sizeof(obj_name));

            ptr = strstr( strlwr(obj_name), specs );
            if (ptr != NULL)
            {
                // Earliest match is the winner.
                if (ptr - obj_name < best_index)
                {
                    if (create_for_real)
                        mpr(obj_name);
                    type_wanted = i;
                    best_index = ptr - obj_name;
                }
            }
        }

        if (type_wanted != -1)
        {
            item->sub_type = type_wanted;
            if (!create_for_real)
                return (true);
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
                for (int unrand = 0; unrand < NO_UNRANDARTS; ++unrand)
                {
                    int index = unrand + UNRAND_START;
                    unrandart_entry* entry = get_unrand_entry(index);

                    strlcpy(obj_name, entry->name, sizeof(obj_name));

                    ptr = strstr( strlwr(obj_name), specs );
                    if (ptr != NULL && entry->base_type == class_wanted)
                    {
                        make_item_unrandart(*item, index);
                        if (create_for_real)
                        {
                            mprf("%s (%s)", entry->name,
                                 debug_art_val_str(*item).c_str());
                        }
                        return(true);
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
                return (false);

            type_wanted--;

            item->sub_type = type_wanted;
        }
    }

    if (!create_for_real)
        return (true);

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
            special_wanted = 0;
            best_index = 10000;

            for (int i = SPWPN_NORMAL + 1; i < SPWPN_DEBUG_RANDART; ++i)
            {
                item->special = i;
                strlcpy(obj_name, item->name(DESC_PLAIN).c_str(), sizeof(obj_name));

                ptr = strstr( strlwr(obj_name), strlwr(buf) );
                if (ptr != NULL)
                {
                    // earliest match is the winner
                    if (ptr - obj_name < best_index)
                    {
                        if (create_for_real)
                            mpr(obj_name);
                        special_wanted = i;
                        best_index = ptr - obj_name;
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
            special_wanted =
                    debug_prompt_for_skill( "A manual for which skill? " );

            if (special_wanted != -1)
            {
                item->plus  = special_wanted;
                item->plus2 = 3 + random2(15);
            }
            else
                mpr("Sorry, no books on that skill today.");
        }
        else if (type_wanted == BOOK_RANDART_THEME)
            make_book_theme_randart(*item, 0, 0, 5 + coinflip(), 20);
        else if (type_wanted == BOOK_RANDART_LEVEL)
        {
            int level = random_range(1, 9);
            int max_spells = 5 + level/3;
            make_book_level_randart(*item, level, max_spells);
        }
        break;

    case OBJ_WANDS:
        item->plus = 24;
        break;

    case OBJ_STAVES:
        if (item_is_rod(*item))
        {
            item->plus  = MAX_ROD_CHARGE * ROD_CHARGE_MULT;
            item->plus2 = MAX_ROD_CHARGE * ROD_CHARGE_MULT;
            init_rod_mp(*item);
        }
        break;

    case OBJ_MISCELLANY:
        if (!item_is_rune(*item) && !is_deck(*item))
            item->plus = 50;
        break;

    case OBJ_POTIONS:
        item->quantity = 12;
        if (is_blood_potion(*item))
        {
            const char* prompt;
            if (item->sub_type == POT_BLOOD)
            {
                prompt = "# turns away from coagulation? "
                         "[ENTER for fully fresh] ";
            }
            else
            {
                prompt = "# turns away from rotting? "
                         "[ENTER for fully fresh] ";
            }
            int age = debug_prompt_for_int(prompt, false);

            if (age <= 0)
                age = -1;
            else if (item->sub_type == POT_BLOOD)
                age += 500;

            init_stack_blood_potions(*item, age);
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
            item->plus2 = 5;
            // intentional fall-through
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

    return (true);
}

void move_items(const coord_def r, const coord_def p)
{
    ASSERT(in_bounds(r));
    ASSERT(in_bounds(p));

    int it = igrd(r);
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
    ii.colour = item.colour;
    ii.inscription = item.inscription;
    ii.flags = item.flags & (0
            | ISFLAG_IDENT_MASK | ISFLAG_BLESSED_WEAPON | ISFLAG_SEEN_CURSED
            | ISFLAG_ARTEFACT_MASK | ISFLAG_DROPPED | ISFLAG_THROWN
            | ISFLAG_COSMETIC_MASK | ISFLAG_RACIAL_MASK);

    if (in_inventory(item))
    {
        ii.link = item.link;
        ii.slot = item.slot;
        ii.pos = coord_def(-1, -1);
    }
    else
        ii.pos = grid2player(item.pos);

    // keep god number
    if (item.orig_monnum < 0)
        ii.orig_monnum = item.orig_monnum;

    switch (item.base_type)
    {
    case OBJ_WEAPONS:
    case OBJ_MISSILES:
        ii.sub_type = item.sub_type;
        if (item_ident(ii, ISFLAG_KNOW_PLUSES))
        {
            ii.plus = item.plus;
            ii.plus2 = item.plus2;
        }
        if (item_type_known(item))
            ii.special = item.special; // brand
        break;
    case OBJ_ARMOUR:
        ii.sub_type = item.sub_type;
        ii.plus2    = item.plus2;      // sub-subtype (gauntlets, etc)
        if (item_ident(ii, ISFLAG_KNOW_PLUSES))
            ii.plus = item.plus;
        if (item_type_known(item))
            ii.special = item.special; // brand
        break;
    case OBJ_WANDS:
        if (item_type_known(item))
            ii.sub_type = item.sub_type;
        ii.special = item.special; // appearance
        if (item_ident(ii, ISFLAG_KNOW_PLUSES))
            ii.plus = item.plus; // charges
        ii.plus2 = item.plus2; // num zapped/recharged or empty
        break;
    case OBJ_POTIONS:
        if (item_type_known(item))
            ii.sub_type = item.sub_type;
        ii.plus = item.plus; // appearance
        break;
    case OBJ_FOOD:
        ii.sub_type = item.sub_type;
        if (ii.sub_type == FOOD_CHUNK)
        {
            ii.plus = item.plus; // monster
            ii.special = food_is_rotten(item) ? 99 : 100;
        }
        break;
    case OBJ_CORPSES:
        ii.sub_type = item.sub_type;
        ii.plus = item.plus; // monster
        ii.special = food_is_rotten(item) ? 99 : 100;
        break;
    case OBJ_SCROLLS:
        if (item_type_known(item))
            ii.sub_type = item.sub_type;
        ii.special = item.special; // name seed, part 1
        ii.plus = item.plus;  // name seed, part 2
        break;
    case OBJ_JEWELLERY:
        if (item_type_known(item))
            ii.sub_type = item.sub_type;
        else
        {
            ii.sub_type = jewellery_is_amulet(item) ? AMU_FIRST_AMULET : RING_FIRST_RING;
            if (item_ident(ii, ISFLAG_KNOW_PLUSES))
                ii.plus = item.plus; // str/dex/int/ac/ev ring plus
        }
        ii.special = item.special; // appearance
        break;
    case OBJ_BOOKS:
        if (item_type_known(item) || item.sub_type == BOOK_MANUAL || item.sub_type == BOOK_DESTRUCTION)
            ii.sub_type = item.sub_type;
        ii.special = item.special; // appearance
        break;
    case OBJ_STAVES:
        if (item_type_known(item))
        {
            ii.sub_type = item.sub_type;
            if (item_ident(ii, ISFLAG_KNOW_PLUSES) && item.props.exists("rod_enchantment"))
                ii.props["rod_enchantment"] = item.props["rod_enchantment"];
        }
        else
            ii.sub_type = item_is_rod(item) ? STAFF_FIRST_ROD : 0;
        ii.special = item.special; // appearance
        break;
    case OBJ_MISCELLANY:
        if (item_type_known(item))
            ii.sub_type = item.sub_type;
        else
        {
            if (item.sub_type >= MISC_DECK_OF_ESCAPE && item.sub_type <= MISC_DECK_OF_DEFENCE)
                ii.sub_type = MISC_DECK_OF_ESCAPE;
            else if (item.sub_type >= MISC_CRYSTAL_BALL_OF_ENERGY && item.sub_type <= MISC_CRYSTAL_BALL_OF_SEEING)
                ii.sub_type = MISC_CRYSTAL_BALL_OF_FIXATION;
            else if (item.sub_type >= MISC_BOX_OF_BEASTS && item.sub_type <= MISC_EMPTY_EBONY_CASKET)
                ii.sub_type = MISC_BOX_OF_BEASTS;
            else
                ii.sub_type = item.sub_type;
        }

        if (ii.sub_type == MISC_RUNE_OF_ZOT)
            ii.plus = item.plus; // which rune

        if (ii.sub_type == MISC_DECK_OF_ESCAPE)
        {
            const int num_cards = cards_in_deck(item);
            CrawlVector info_cards;
            CrawlVector info_card_flags;
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
                    // special card to tell at which point cards are no longer continuous
                    info_cards.push_back((char)0);
                    info_card_flags.push_back((char)0);
                    found_unmarked = true;
                }
            }
            // TODO: this leaks both whether the seen cards are still there and their order: the representation needs to be fixed
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

    if (item_type_known(item)) {
        if (item.props.exists(ARTEFACT_NAME_KEY))
            ii.props[ARTEFACT_NAME_KEY] = item.props[ARTEFACT_NAME_KEY];
    }

    const char* copy_props[] = {ARTEFACT_APPEAR_KEY, KNOWN_PROPS_KEY, CORPSE_NAME_KEY, CORPSE_NAME_TYPE_KEY, "jewellery_tried", "drawn_cards"};
    for (unsigned i = 0; i < (sizeof(copy_props) / sizeof(copy_props[0])); ++i)
    {
        if (item.props.exists(copy_props[i]))
            ii.props[copy_props[i]] = item.props[copy_props[i]];
    }

    if (item.props.exists(ARTEFACT_PROPS_KEY))
    {
        CrawlVector props = item.props[ARTEFACT_PROPS_KEY].get_vector();
        const CrawlVector &known = item.props[KNOWN_PROPS_KEY].get_vector();

        for (unsigned i = 0; i < props.size(); ++i) {
            if (i >= known.size() || !known[i].get_bool())
                props[i] = (short)0;
        }

        ii.props[ARTEFACT_PROPS_KEY] = props;
    }

    return ii;
}
