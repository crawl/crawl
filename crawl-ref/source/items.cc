/*
 *  File:       items.cc
 *  Summary:    Misc (mostly) inventory related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "items.h"
#include "cio.h"
#include "clua.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "arena.h"
#include "beam.h"
#include "branch.h"
#include "cloud.h"
#include "debug.h"
#include "delay.h"
#include "dgnevent.h"
#include "directn.h"
#include "effects.h"
#include "food.h"
#include "hiscores.h"
#include "invent.h"
#include "it_use2.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mstuff2.h"
#include "mon-util.h"
#include "mutation.h"
#include "notes.h"
#include "overmap.h"
#include "place.h"
#include "player.h"
#include "quiver.h"
#include "randart.h"
#include "religion.h"
#include "shopping.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "stash.h"
#include "tiles.h"
#include "state.h"
#include "terrain.h"
#include "transfor.h"
#include "tutorial.h"
#include "view.h"
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
        if (held_by_monster(mitm[i]))
            continue;

        if (!is_valid_item(mitm[i]))
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
    if (mitm[item].base_type == OBJ_FOOD || mitm[item].base_type == OBJ_ORBS)
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
    mpr( "Too many items on level, removing some.", MSGCH_WARN );

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
    for ( rectangle_iterator ri(1); ri; ++ri )
    {
        if ( grid_distance( you.pos(), *ri ) <= 9 )
            continue;

        for ( stack_iterator si(*ri); si; ++si )
        {
            if (_item_ok_to_clean(si->index()) && x_chance_in_y(15, 100))
            {
                if (is_fixed_artefact(*si))
                {
                    // 7. Move uniques to abyss.
                    set_unique_item_status( OBJ_WEAPONS, si->special,
                                            UNIQ_LOST_IN_ABYSS );
                }
                else if (is_unrandom_artefact(*si))
                {
                    // 9. Unmark unrandart.
                    const int z = find_unrandart_index(*si);
                    if (z != -1)
                        set_unrandart_exist(z, false);
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

// Note:  This function is to isolate all the checks to see if
//        an item is valid (often just checking the quantity).
//
//        It shouldn't be used a a substitute for those cases
//        which actually want to check the quantity (as the
//        rules for unused objects might change).
bool is_valid_item( const item_def &item )
{
    return (item.base_type != OBJ_UNASSIGNED && item.quantity > 0);
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

    if ( !suppress_burden )
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

    if (player_in_branch(BRANCH_HALL_OF_ZOT) && is_rune(mitm[obj]))
        you.attribute[ATTR_RUNES_IN_ZOT] -= amount;

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

    if ( !suppress_burden )
        burden_change();
}

void inc_mitm_item_quantity( int obj, int amount )
{
    if (player_in_branch(BRANCH_HALL_OF_ZOT) && is_rune(mitm[obj]))
        you.attribute[ATTR_RUNES_IN_ZOT] += amount;

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

    if (crawl_state.arena)
        reserve = 0;

    int item = NON_ITEM;

    for (item = 0; item < (MAX_ITEMS - reserve); item++)
        if (!is_valid_item( mitm[item] ))
            break;

    if (item >= MAX_ITEMS - reserve)
    {
        if (crawl_state.arena)
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
    if (dest == NON_ITEM || !is_valid_item( mitm[dest] ))
        return;

    monsters* monster = holding_monster(mitm[dest]);

    if (monster != NULL)
    {
        for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
        {
            if (monster->inv[i] == dest)
            {
                monster->inv[i] = NON_ITEM;

                mitm[dest].pos.reset();
                mitm[dest].link = NON_ITEM;
                return;
            }
        }
        mprf(MSGCH_ERROR, "Item %s claims to be held by monster %s, but "
                          "it isn't in the monster's inventory.",
             mitm[dest].name(DESC_PLAIN, false, true).c_str(),
             monster->name(DESC_PLAIN, true).c_str());
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
            if (is_valid_item(*si) && si->link == dest)
            {
                // unlink dest
                si->link = mitm[dest].link;
                mitm[dest].pos.reset();
                mitm[dest].link = NON_ITEM;
                return;
            }
        }
    }

#if DEBUG
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
        if (is_valid_item( mitm[c] ) && mitm[c].link == dest)
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
        mpr("BUG WARNING: Item didn't seem to be linked at all.",
            MSGCH_ERROR);
#endif
}                               // end unlink_item()

void destroy_item( item_def &item, bool never_created )
{
    if (!is_valid_item( item ))
        return;

    if (never_created)
    {
        if (is_fixed_artefact(item))
        {
            set_unique_item_status(item.base_type, item.special,
                                   UNIQ_NOT_EXISTS);
        }
        else if (is_unrandom_artefact(item))
        {
            const int unrand = find_unrandart_index(item);
            if (unrand != -1)
                set_unrandart_exist( unrand, false );
        }
    }

    item.clear();
}

void destroy_item( int dest, bool never_created )
{
    // Don't destroy non-items, but this function may be called upon
    // to remove items reduced to zero quantity, so we allow "invalid"
    // objects in.
    if (dest == NON_ITEM || !is_valid_item( mitm[dest] ))
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
        if (item.base_type == OBJ_ORBS)
        {
            set_unique_item_status(OBJ_ORBS, item.sub_type,
                                   UNIQ_LOST_IN_ABYSS);
        }
        else if (is_fixed_artefact(item))
        {
            set_unique_item_status(OBJ_WEAPONS, item.special,
                                   UNIQ_LOST_IN_ABYSS);
        }
    }

    if (is_rune(item))
    {
        if ((item.flags & ISFLAG_BEEN_IN_INV))
        {
            if (is_unique_rune(item))
                you.attribute[ATTR_UNIQUE_RUNES] -= item.quantity;
            else if (item.plus == RUNE_ABYSSAL)
                you.attribute[ATTR_ABYSSAL_RUNES] -= item.quantity;
            else
                you.attribute[ATTR_DEMONIC_RUNES] -= item.quantity;
        }

        if (player_in_branch(BRANCH_HALL_OF_ZOT) && !in_inventory(item))
            you.attribute[ATTR_RUNES_IN_ZOT] -= item.quantity;
    }
}

void item_was_lost(const item_def &item)
{
    _handle_gone_item( item );
    xom_check_lost_item( item );
}

void item_was_destroyed(const item_def &item, int cause)
{
    _handle_gone_item( item );
    xom_check_destroyed_item( item, cause );
}

void lose_item_stack( const coord_def& where )
{
    for ( stack_iterator si(where); si; ++si )
    {
        if (is_valid_item( *si )) // FIXME is this check necessary?
        {
            item_was_lost(*si);
            si->clear();
        }
    }
    igrd(where) = NON_ITEM;
}

void destroy_item_stack( int x, int y, int cause )
{
    for ( stack_iterator si(coord_def(x,y)); si; ++si )
    {
        if (is_valid_item( *si )) // FIXME is this check necessary?
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

    for ( stack_iterator si(obj); si; ++si )
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
    for ( stack_iterator si(obj); si; ++si )
    {
        // Add them to the items list if they qualify.
        if ( !have_nonsquelched || !_invisible_to_player(*si) )
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

    // Missiles don't get name descriptors.
    if (item.base_type == OBJ_MISSILES)
        return 0;

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
    if ( is_artefact(item) )
        return 2;

    return 0;
}

void item_check(bool verbose)
{
    describe_floor();
    origin_set(you.pos());

    std::ostream& strm = msg::streams(MSGCH_FLOOR_ITEMS);

    std::vector<const item_def*> items;

    item_list_on_square( items, igrd(you.pos()), true );

    if (items.empty())
    {
        if (verbose)
            strm << "There are no items here." << std::endl;
        return;
    }

    if (items.size() == 1 )
    {
        item_def it(*items[0]);
        std::string name = get_message_colour_tags(it, DESC_NOCAP_A,
                                                   MSGCH_FLOOR_ITEMS);
        strm << "You see here " << name << '.' << std::endl;
        return;
    }

    bool done_init_line = false;

    if (static_cast<int>(items.size()) >= Options.item_stack_summary_minimum)
    {
        std::vector<unsigned short int> item_chars;
        for (unsigned int i = 0; i < items.size() && i < 50; ++i)
        {
            unsigned glyph_char;
            unsigned short glyph_col;
            get_item_glyph( items[i], &glyph_char, &glyph_col );
            item_chars.push_back( glyph_char * 0x100 +
                                  (10 - item_name_specialness(*(items[i]))) );
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

            out_string += static_cast<unsigned char>(item_chars[i] / 0x100);
            if (i + 1 < item_chars.size()
                && (item_chars[i] / 0x100) != (item_chars[i+1] / 0x100))
            {
                out_string += ' ';
            }
        }
        formatted_mpr(formatted_string::parse_string(out_string),
                      MSGCH_FLOOR_ITEMS);
        done_init_line = true;
    }

    if (verbose || static_cast<int>(items.size() + 1) < crawl_view.msgsz.y)
    {
        if (!done_init_line)
            strm << "Things that are here:" << std::endl;
        for (unsigned int i = 0; i < items.size(); ++i)
        {
            item_def it(*items[i]);
            std::string name = get_message_colour_tags(it, DESC_NOCAP_A,
                                                       MSGCH_FLOOR_ITEMS);
            strm << name << std::endl;
        }
    }
    else if (!done_init_line)
        strm << "There are many items here." << std::endl;

    if (items.size() > 5)
        learned_something_new(TUT_MULTI_PICKUP);
}

static void _pickup_menu(int item_link)
{
    int n_did_pickup   = 0;
    int n_tried_pickup = 0;

    std::vector<const item_def*> items;
    item_list_on_square( items, item_link, false );

    std::vector<SelItem> selected =
        select_items( items, "Select items to pick up" );
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
                unsigned long oldflags = mitm[j].flags;
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

                    if (is_valid_item(mitm[j]))
                        mitm[j].flags = oldflags;
                }
                else
                {
                    n_did_pickup++;
                    // If we deliberately chose to take only part of a
                    // pile, we consider the rest to have been
                    // "dropped."
                    if (!take_all && is_valid_item(mitm[j]))
                        mitm[j].flags |= ISFLAG_DROPPED;
                }
            }
        }

    if (!pickup_warning.empty())
    {
        mpr(pickup_warning.c_str());
        learned_something_new(TUT_HEAVY_LOAD);
    }

    if (n_did_pickup)
        you.turn_is_over = true;
}

bool origin_known(const item_def &item)
{
    return (item.orig_place != 0);
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

void _origin_set_portal_vault(item_def &item)
{
    if (you.level_type != LEVEL_PORTAL_VAULT)
        return;

    item.props[PORTAL_VAULT_ORIGIN_KEY] = you.level_type_origin;
}

void origin_set_monster(item_def &item, const monsters *monster)
{
    if (!origin_known(item))
    {
        if (!item.orig_monnum)
            item.orig_monnum = monster->type + 1;
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
        if (is_valid_item(you.inv[i]))
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

#ifdef DGL_MILESTONES
static std::string _milestone_rune(const item_def &item)
{
    return std::string("found ") + item.name(DESC_NOCAP_A) + ".";
}

static void _milestone_check(const item_def &item)
{
    if (item.base_type == OBJ_MISCELLANY
        && item.sub_type == MISC_RUNE_OF_ZOT)
    {
        mark_milestone("rune", _milestone_rune(item));
    }
    else if (item.base_type == OBJ_ORBS && item.sub_type == ORB_ZOT)
    {
        mark_milestone("orb", "found the Orb of Zot!");
    }
}
#endif // DGL_MILESTONES

static void _check_note_item(item_def &item)
{
    if (item.flags & (ISFLAG_NOTED_GET | ISFLAG_NOTED_ID))
        return;

    if (is_rune(item) || item.base_type == OBJ_ORBS || is_artefact(item))
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

#ifdef DGL_MILESTONES
        _milestone_check(*si);
#endif
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

#ifdef DGL_MILESTONES
        _milestone_check(item);
#endif
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

bool is_rune(const item_def &item)
{
    return (item.base_type == OBJ_MISCELLANY
            && item.sub_type == MISC_RUNE_OF_ZOT);
}

bool is_unique_rune(const item_def &item)
{
    return (item.base_type == OBJ_MISCELLANY
            && item.sub_type == MISC_RUNE_OF_ZOT
            && item.plus != RUNE_DEMONIC
            && item.plus != RUNE_ABYSSAL);
}

bool origin_describable(const item_def &item)
{
    return (origin_known(item)
            && (item.orig_place != 0xFFFFU || item.orig_monnum == -1)
            && (!is_stackable_item(item) || is_rune(item))
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

    unsigned long oldflags = mitm[link].flags;
    mitm[link].flags &= ~(ISFLAG_THROWN | ISFLAG_DROPPED);
    int num = move_item_to_player( link, qty );
    if (is_valid_item(mitm[link]))
        mitm[link].flags = oldflags;

    if (num == -1)
    {
        mpr("You can't carry that many items.");
        learned_something_new(TUT_HEAVY_LOAD);
        return (false);
    }
    else if (num == 0)
    {
        mpr("You can't carry that much weight.");
        learned_something_new(TUT_HEAVY_LOAD);
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

    int o = igrd(you.pos());
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
        while ( o != NON_ITEM )
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
                                     "y/n/a/*?g,/q)";

                mprf(MSGCH_PROMPT, prompt.c_str(),
                     get_message_colour_tags(mitm[o], DESC_NOCAP_A,
                                             MSGCH_PROMPT).c_str());

                mouse_control mc(MOUSE_MODE_MORE);
                keyin = getch();
            }

            if (keyin == '*' || keyin == '?' || keyin == ',' || keyin == 'g'
                || keyin == CK_MOUSE_CLICK)
            {
                _pickup_menu(o);
                break;
            }

            if (keyin == 'q' || keyin == ESCAPE)
                break;

            if (keyin == 'y' || keyin == 'a')
            {
                int num_to_take = mitm[o].quantity;
                const unsigned long old_flags(mitm[o].flags);
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
    }
}                               // end pickup()

bool is_stackable_item( const item_def &item )
{
    if (!is_valid_item( item ))
        return (false);

    if (item.base_type == OBJ_MISSILES
        || (item.base_type == OBJ_FOOD && item.sub_type != FOOD_CHUNK)
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

unsigned long ident_flags(const item_def &item)
{
    const unsigned long identmask = full_ident_mask(item);
    unsigned long flags = item.flags & identmask;

    if ((identmask & ISFLAG_KNOW_TYPE) && item_type_known(item))
        flags |= ISFLAG_KNOW_TYPE;

    return (flags);
}

bool items_similar(const item_def &item1, const item_def &item2)
{
    // Base and sub-types must always be the same to stack.
    if (item1.base_type != item2.base_type || item1.sub_type != item2.sub_type)
        return (false);

    if (item1.base_type == OBJ_GOLD)
        return (true);

    // These classes also require pluses and special.
    if (item1.base_type == OBJ_WEAPONS         // only throwing weapons
        || item1.base_type == OBJ_MISSILES
        || item1.base_type == OBJ_MISCELLANY)  // only runes
    {
        if (item1.plus != item2.plus
            || item1.plus2 != item2.plus2
            || item1.special != item2.special)
        {
            return (false);
        }
    }

    // Check the ID flags.
    if (ident_flags(item1) != ident_flags(item2))
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
                  bool force_merge )
{
    // Both items must be stackable.
    if (!force_merge
        && (!is_stackable_item( item1 ) || !is_stackable_item( item2 )))
    {
        return (false);
    }

    return items_similar(item1, item2);
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
            ((s) >= 0 && (s) < ENDOFPACK && !is_valid_item(you.inv[s]))

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

    if (!searchforward)
    {
        // This is the new default free slot search. We look for the last
        // available slot that does not leave a gap in the inventory.
        bool accept_empty = false;
        for (slot = ENDOFPACK - 1; slot >= 0; --slot)
        {
            if (is_valid_item(you.inv[slot]))
            {
                if (!accept_empty && slot + 1 < ENDOFPACK
                    && !is_valid_item(you.inv[slot + 1]))
                {
                    return (slot + 1);
                }
                accept_empty = true;
            }
            else if (accept_empty)
            {
                return slot;
            }
        }
    }

    // Either searchforward is true, or search backwards failed and
    // we re-try searching the oposite direction.

    // Return first free slot
    for (slot = 0; slot < ENDOFPACK; ++slot)
        if (!is_valid_item(you.inv[slot]))
            return slot;

    return (-1);
#undef slotisfree
}

static void _got_item(item_def& item, int quant)
{
    if (!is_rune(item))
        return;

    // Picking up the rune for the first time.
    if (!(item.flags & ISFLAG_BEEN_IN_INV))
    {
        if (is_unique_rune(item))
            you.attribute[ATTR_UNIQUE_RUNES] += quant;
        else if (item.plus == RUNE_ABYSSAL)
            you.attribute[ATTR_ABYSSAL_RUNES] += quant;
        else
            you.attribute[ATTR_DEMONIC_RUNES] += quant;
    }

    item.flags |= ISFLAG_BEEN_IN_INV;
    _check_note_item(item);
}

// Returns quantity of items moved into player's inventory and -1 if
// the player's inventory is full.
int move_item_to_player( int obj, int quant_got, bool quiet,
                         bool ignore_burden )
{
    if (item_is_stationary(mitm[obj]))
    {
        mpr("You cannot pick up the net that holds you!");
        // Fake a successful pickup (return 1), so we can continue to pick up
        // anything that might be on this square.
        return (1);
    }

    int retval = quant_got;

    // Gold has no mass, so we handle it first.
    if (mitm[obj].base_type == OBJ_GOLD)
    {
        you.attribute[ATTR_GOLD_FOUND] += quant_got;
        you.gold += quant_got;
        dec_mitm_item_quantity( obj, quant_got );

        if (!quiet)
        {
            mprf("You now have %d gold piece%s.",
                 you.gold, you.gold > 1 ? "s" : "");
        }

        learned_something_new(TUT_SEEN_GOLD);

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
            if (items_stack( you.inv[m], mitm[obj] ))
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

                merge_item_stacks(mitm[obj], you.inv[m], quant_got);

                inc_inv_item_quantity( m, quant_got );
                dec_mitm_item_quantity( obj, quant_got );
                burden_change();

                _got_item(mitm[obj], quant_got);

                if (!quiet)
                    mpr( you.inv[m].name(DESC_INVENTORY).c_str() );

                you.turn_is_over = true;

                return (retval);
            }
        }
    }

    // can't combine, check for slot space
    if (inv_count() >= ENDOFPACK)
        return (-1);

    if (!quiet && partial_pickup)
        mpr("You can only carry some of what is here.");

    int freeslot = find_free_slot(mitm[obj]);
    if (freeslot < 0 || freeslot >= ENDOFPACK
        || is_valid_item(you.inv[freeslot]))
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

    _autoinscribe_item( item );

    _origin_freeze(item, you.pos());
    _check_note_item(item);

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
        mpr( you.inv[freeslot].name(DESC_INVENTORY).c_str() );

    if (Options.tutorial_left)
    {
        taken_new_item(item.base_type);
        if (is_artefact(item) || get_equip_desc( item ) != ISFLAG_NO_DESC)
            learned_something_new(TUT_SEEN_RANDART);
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
}                               // end move_item_to_player()

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
bool move_item_to_grid( int *const obj, const coord_def& p )
{
    ASSERT(in_bounds(p));

    int& ob(*obj);
    // Must be a valid reference to a valid object.
    if (ob == NON_ITEM || !is_valid_item( mitm[ob] ))
        return (false);

    item_def& item(mitm[ob]);

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
            if (copy_item_to_grid(item, p, 1, false))
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

    if (is_rune(item))
    {
        if (player_in_branch(BRANCH_HALL_OF_ZOT))
            you.attribute[ATTR_RUNES_IN_ZOT] += item.quantity;
    }
    else if (item.base_type == OBJ_ORBS && you.level_type == LEVEL_DUNGEON)
    {
        set_branch_flags(BFLAG_HAS_ORB);
    }

//     if (see_grid(p))
//        StashTrack.update_stash(p);

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


// Returns quantity dropped.
bool copy_item_to_grid( const item_def &item, const coord_def& p,
                        int quant_drop, bool mark_dropped )
{
    ASSERT(in_bounds(p));

    if (quant_drop == 0)
        return (false);

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

    move_item_to_grid( &new_item_idx, p );
    if (is_blood_potion(item)
        && item.quantity != quant_drop) // partial drop only
    {
        // Since only the oldest potions have been dropped,
        // remove the newest ones.
        remove_newest_blood_potion(new_item);
    }

    return (true);
}                               // end copy_item_to_grid()


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
        && item_cursed( you.inv[item_dropped] ))
    {
        mpr("That object is stuck to you!");
        return (false);
    }

    for (int i = EQ_CLOAK; i <= EQ_BODY_ARMOUR; i++)
    {
        if (item_dropped == you.equip[i] && you.equip[i] != -1)
        {
            if (!Options.easy_unequip)
            {
                mpr("You will have to take that off first.");
            }
            else
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
        if (!unwield_item())
            return (false);
        canned_msg( MSG_EMPTY_HANDED );
    }

    const dungeon_feature_type my_grid = grd(you.pos());

    if (!grid_destroys_items(my_grid)
        && !copy_item_to_grid( you.inv[item_dropped],
                               you.pos(), quant_drop, true ))
    {
        mpr("Too many items on this level, not dropping the item.");
        return (false);
    }

    mprf("You drop %s.",
         quant_name(you.inv[item_dropped], quant_drop, DESC_NOCAP_A).c_str());

    if (grid_destroys_items(my_grid))
    {
        if ( !silenced(you.pos()) )
            mprf(MSGCH_SOUND, grid_item_destruction_message(my_grid));

        item_was_destroyed(you.inv[item_dropped], NON_MONSTER);
    }
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
        && grid_altar_god(grd(you.pos())) == you.religion)
    {
        offer_items();
    }

    return (true);
}

static std::string _drop_menu_invstatus(const Menu *menu)
{
    char buf[100];
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

        snprintf(buf, sizeof buf, ">%.0f", newweight * BURDEN_TO_AUM);
        s_newweight = buf;
    }

    snprintf(buf, sizeof buf, "(Inv: %.0f%s/%.0f aum)",
             you.burden * BURDEN_TO_AUM,
             s_newweight.c_str(),
             cap * BURDEN_TO_AUM);
    return (buf);
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

mon_inv_type get_mon_equip_slot(const monsters* mon, const item_def &item)
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
    char buf[130];
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

    snprintf( buf, sizeof buf, " (%lu%s turn%s)",
                (unsigned long) (s->size()),
                extraturns? "+" : "",
                s->size() > 1? "s" : "" );
    return buf;
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
    tmp_items = prompt_invent_items( "Drop what?", MT_DROP, -1,
                                     _drop_menu_title, true, true, 0,
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
        if ( si.quantity && si.quantity != item_quant )
            const_cast<item_def*>(si.item)->quantity = si.quantity;

        // Check if we can add it to the multidrop list.
        bool warning_ok = check_warning_inscriptions(*(si.item), OPER_DROP);

        // Restore the item quantity if we mangled it.
        if ( item_quant != si.item->quantity )
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
    if ( !item.inscription.empty() && item.inscription != "god gift")
        return;
    const std::string old_inscription = item.inscription;
    item.inscription.clear();

    std::string iname = _autopickup_item_name(item);

    for ( unsigned i = 0; i < Options.autoinscriptions.size(); ++i )
    {
        if ( Options.autoinscriptions[i].first.matches(iname) )
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
    if ( !old_inscription.empty() )
    {
        if ( item.inscription.empty() )
            item.inscription = old_inscription;
        else
            item.inscription = old_inscription + ", " + item.inscription;
    }
}

static void _autoinscribe_floor_items()
{
    for ( stack_iterator si(you.pos()); si; ++si )
        _autoinscribe_item( *si );
}

static void _autoinscribe_inventory()
{
    for (int i = 0; i < ENDOFPACK; i++)
        if (is_valid_item(you.inv[i]))
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
    if (clua.callbooleanfn(false, "ch_force_autopickup", "us",
                           &item, iname.c_str()))
    {
        return (true);
    }
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

    if ((item.flags & ISFLAG_DROPPED) && !Options.pickup_dropped) {
        return (false);
    }

    std::string itemname;
    return _is_option_autopickup(item, itemname);
}

bool can_autopickup()
{
    // [ds] Checking for autopickups == 0 is a bad idea because
    // autopickup is still possible with inscriptions and
    // pickup_thrown.
    if (!Options.autopickup_on)
        return (false);

    if (you.flight_mode() == FL_LEVITATE)
        return (false);

    if (!i_feel_safe())
        return (false);

    return (true);
}

static void _do_autopickup()
{
    int n_did_pickup   = 0;
    int n_tried_pickup = 0;

    will_autopickup = false;

    if (!can_autopickup())
    {
        item_check(false);
        return;
    }

    int o = igrd(you.pos());

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

            const unsigned long iflags(mitm[o].flags);
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
                n_did_pickup++;
        }

        o = next;
    }

    if (!pickup_warning.empty())
        mpr(pickup_warning.c_str());

    if (n_did_pickup)
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
        if (is_valid_item( you.inv[i] ))
            count++;

    return count;
}

item_def *find_floor_item(object_class_type cls, int sub_type)
{
    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
            for ( stack_iterator si(coord_def(x,y)); si; ++si )
                if (is_valid_item( *si)
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

        if (base_type == OBJ_BOOKS && i == BOOK_MANUAL)
        {
            for (int j = 0; j < NUM_SKILLS; ++j)
            {
                if (!skill_name(j))
                    continue;

                item.plus = j;

                if (name == lowercase_string(item.name(DESC_PLAIN)))
                {
                    type_wanted = i;
                    i = ntypes;
                    break;
                }
            }
        }
        else if (name == lowercase_string(item.name(DESC_PLAIN)))
        {
            type_wanted = i;
            break;
        }
    }

    if (type_wanted != -1)
        item.sub_type = type_wanted;
    else
        item.sub_type = OBJ_RANDOM;

    return (item.sub_type != OBJ_RANDOM);
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
        0,              // Orbs         -- only one, handled specially
        NUM_MISCELLANY,
        0,              // corpses      -- handled specially
        0,              // gold         -- handled specially
        0,              // "gemstones"  -- no items of type
    };

    if (base_type == OBJ_UNASSIGNED)
    {
        for (unsigned i = 0; i < ARRAYSZ(max_subtype); ++i)
        {
            if (max_subtype[i] == 0)
                continue;

            if (_find_subtype_by_name(item, static_cast<object_class_type>(i),
                                      max_subtype[i], name))
            {
                break;
            }
        }
    }
    else
        _find_subtype_by_name(item, base_type, max_subtype[base_type], name);

    return (item);
}

bool item_is_equipped(const item_def &item, bool quiver_too)
{
    if (!in_inventory(item))
        return (false);

    for (int i = 0; i < NUM_EQUIP; i++)
        if (item.link == you.equip[i])
            return (true);

    if (quiver_too && item.link == you.m_quiver->get_fire_item())
        return (true);

    return (false);
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
            base_type == OBJ_STAVES ? sub_type + NUM_BOOKS - STAFF_FIRST_ROD
                                    : -1);
}

bool item_def::cursed() const
{
    return (item_cursed(*this));
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
    if (!is_valid_item(*this) || base_type != OBJ_ARMOUR)
        return (0);

    return (property(*this, PARM_AC) + plus);
}
