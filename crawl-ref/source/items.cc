/*
 *  File:       items.cc
 *  Summary:    Misc (mostly) inventory related functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 * <9> 7/08/01   MV   Added messages for chunks/corpses rotting
 * <8> 8/07/99   BWR  Added Rune stacking
 * <7> 6/13/99   BWR  Added auto staff detection
 * <6> 6/12/99   BWR  Fixed time system.
 * <5> 6/9/99    DML  Autopickup
 * <4> 5/26/99   JDJ  Drop will attempt to take off armour.
 * <3> 5/21/99   BWR  Upped armour skill learning slightly.
 * <2> 5/20/99   BWR  Added assurance that against inventory count being wrong.
 * <1> -/--/--   LRH  Created
 */

#include "AppHdr.h"
#include "items.h"
#ifdef USE_TILE
 #include "cio.h"
#endif
#include "clua.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"

#include "beam.h"
#include "branch.h"
#include "cloud.h"
#include "debug.h"
#include "delay.h"
#include "dgnevent.h"
#include "direct.h"
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
#include "spl-book.h"
#include "spl-util.h"
#include "stuff.h"
#include "stash.h"
#include "tiles.h"
#include "state.h"
#include "terrain.h"
#include "transfor.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

static bool _invisible_to_player( const item_def& item );
static void _item_list_on_square( std::vector<const item_def*>& items,
                                  int obj, bool force_squelch = false );
static void _autoinscribe_item( item_def& item );
static void _autoinscribe_floor_items();
static void _autoinscribe_inventory();

static bool will_autopickup   = false;
static bool will_autoinscribe = false;

// Used to be called "unlink_items", but all it really does is make
// sure item coordinates are correct to the stack they're in. -- bwr
void fix_item_coordinates(void)
{
    // nails all items to the ground (i.e. sets x,y)
    for (int x = 0; x < GXM; x++)
    {
        for (int y = 0; y < GYM; y++)
        {
            int i = igrd[x][y];

            while (i != NON_ITEM)
            {
                mitm[i].x = x;
                mitm[i].y = y;
                i = mitm[i].link;
            }
        }
    }
}

// This function uses the items coordinates to relink all the igrd lists.
void link_items(void)
{
    // first, initialise igrd array
    igrd.init(NON_ITEM);

    // link all items on the grid, plus shop inventory,
    // DON'T link the huge pile of monster items at (0,0)

    for (int i = 0; i < MAX_ITEMS; i++)
    {
        if (!is_valid_item(mitm[i]) || (mitm[i].x == 0 && mitm[i].y == 0))
        {
            // item is not assigned, or is monster item.  ignore.
            mitm[i].link = NON_ITEM;
            continue;
        }

        // link to top
        mitm[i].link = igrd[ mitm[i].x ][ mitm[i].y ];
        igrd[ mitm[i].x ][ mitm[i].y ] = i;
    }
}                               // end link_items()

static bool _item_ok_to_clean(int item)
{
    // never clean food or Orbs
    if (mitm[item].base_type == OBJ_FOOD || mitm[item].base_type == OBJ_ORBS)
        return false;

    // never clean runes
    if (mitm[item].base_type == OBJ_MISCELLANY
        && mitm[item].sub_type == MISC_RUNE_OF_ZOT)
    {
        return false;
    }

    return true;
}

// returns index number of first available space, or NON_ITEM for
// unsuccessful cleanup (should be exceedingly rare!)
static int _cull_items(void)
{
    crawl_state.cancel_cmd_repeat();

    // XXX: Not the prettiest of messages, but the player
    // deserves to know whenever this kicks in. -- bwr
    mpr( "Too many items on level, removing some.", MSGCH_WARN );

    /* rules:
       1. Don't cleanup anything nearby the player
       2. Don't cleanup shops
       3. Don't cleanup monster inventory
       4. Clean 15% of items
       5. never remove food, orbs, runes
       7. uniques weapons are moved to the abyss
       8. randarts are simply lost
       9. unrandarts are 'destroyed', but may be generated again
    */

    int first_cleaned = NON_ITEM;

    // 2. avoid shops by avoiding (0,5..9)
    // 3. avoid monster inventory by iterating over the dungeon grid
    for (int x = 5; x < GXM; x++)
    {
        for (int y = 5; y < GYM; y++)
        {
            // 1. not near player!
            if (x > you.x_pos - 9 && x < you.x_pos + 9
                && y > you.y_pos - 9 && y < you.y_pos + 9)
            {
                continue;
            }

            int next;

            // iterate through the grids list of items:
            for (int item = igrd[x][y]; item != NON_ITEM; item = next)
            {
                next = mitm[item].link; // in case we can't get it later.

                if (_item_ok_to_clean(item) && random2(100) < 15)
                {
                    const item_def& obj(mitm[item]);
                    if (is_fixed_artefact(obj))
                    {
                        // 7. move uniques to abyss
                        set_unique_item_status( OBJ_WEAPONS, obj.special,
                                                UNIQ_LOST_IN_ABYSS );
                    }
                    else if (is_unrandom_artefact(obj))
                    {
                        // 9. unmark unrandart
                        const int z = find_unrandart_index(obj);
                        if (z != -1)
                            set_unrandart_exist(z, false);
                    }

                    // POOF!
                    destroy_item( item );
                    if (first_cleaned == NON_ITEM)
                        first_cleaned = item;
                }
            } // end for item

        } // end y
    } // end x

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
bool dec_inv_item_quantity( int obj, int amount )
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
        you.inv[obj].quantity = 0;
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

void inc_inv_item_quantity( int obj, int amount )
{
    if (you.equip[EQ_WEAPON] == obj)
        you.wield_change = true;
        
    you.m_quiver->on_inv_quantity_changed(obj, amount);
    you.inv[obj].quantity += amount;
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

    int item = NON_ITEM;

    for (item = 0; item < (MAX_ITEMS - reserve); item++)
    {
        if (!is_valid_item( mitm[item] ))
            break;
    }

    if (item >= MAX_ITEMS - reserve)
    {
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
    int c = 0;
    int cy = 0;

    // Don't destroy non-items, may be called after an item has been
    // reduced to zero quantity however.
    if (dest == NON_ITEM || !is_valid_item( mitm[dest] ))
        return;

    if (mitm[dest].x == 0 && mitm[dest].y == 0)
    {
        // (0,0) is where the monster items are (and they're unlinked by igrd),
        // although it also contains items that are not linked in yet.
        //
        // Check if a monster has it:
        for (c = 0; c < MAX_MONSTERS; c++)
        {
            struct monsters *monster = &menv[c];

            if (monster->type == -1)
                continue;

            for (cy = 0; cy < NUM_MONSTER_SLOTS; cy++)
            {
                if (monster->inv[cy] == dest)
                {
                    monster->inv[cy] = NON_ITEM;

                    mitm[dest].x = 0;
                    mitm[dest].y = 0;
                    mitm[dest].link = NON_ITEM;

                    // This causes problems when changing levels. -- bwr
                    // if (monster->type == MONS_DANCING_WEAPON)
                    //     monster_die(monster, KILL_RESET, 0);
                    return;
                }
            }
        }

        // Always return because this item might just be temporary.
        return;
    }
    else
    {
        // Linked item on map:
        //
        // Use the items (x,y) to access the list (igrd[x][y]) where
        // the item should be linked.

        // First check the top:
        if (igrd[ mitm[dest].x ][ mitm[dest].y ] == dest)
        {
            // link igrd to the second item
            igrd[ mitm[dest].x ][ mitm[dest].y ] = mitm[dest].link;

            mitm[dest].x = 0;
            mitm[dest].y = 0;
            mitm[dest].link = NON_ITEM;
            return;
        }

        // Okay, item is buried, find item that's on top of it:
        for (c = igrd[ mitm[dest].x ][ mitm[dest].y ]; c != NON_ITEM; c = mitm[c].link)
        {
            // find item linking to dest item
            if (is_valid_item( mitm[c] ) && mitm[c].link == dest)
            {
                // unlink dest
                mitm[c].link = mitm[dest].link;

                mitm[dest].x = 0;
                mitm[dest].y = 0;
                mitm[dest].link = NON_ITEM;
                return;
            }
        }
    }

#if DEBUG
    // Okay, the sane ways are gone... let's warn the player:
    mpr( "BUG WARNING: Problems unlinking item!!!", MSGCH_DANGER );

    // Okay, first we scan all items to see if we have something
    // linked to this item.  We're not going to return if we find
    // such a case... instead, since things are already out of
    // alignment, let's assume there might be multiple links as well.
    bool  linked = false;
    int   old_link = mitm[dest].link; // used to try linking the first

    // clean the relevant parts of the object:
    mitm[dest].base_type = OBJ_UNASSIGNED;
    mitm[dest].quantity = 0;
    mitm[dest].x = 0;
    mitm[dest].y = 0;
    mitm[dest].link = NON_ITEM;
    mitm[dest].props.clear();

    // Look through all items for links to this item.
    for (c = 0; c < MAX_ITEMS; c++)
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
    for (c = 2; c < (GXM - 1); c++)
    {
        for (cy = 2; cy < (GYM - 1); cy++)
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
    }


    // Okay, finally warn player if we didn't do anything.
    if (!linked)
        mpr("BUG WARNING: Item didn't seem to be linked at all.", MSGCH_DANGER);
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

    // paranoia, shouldn't be needed
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

        if (player_in_branch(BRANCH_HALL_OF_ZOT)
            && item.x != -1 && item.y != -1)
        {
            you.attribute[ATTR_RUNES_IN_ZOT] -= item.quantity;
        }
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

void lose_item_stack( int x, int y )
{
    int o = igrd[x][y];

    igrd[x][y] = NON_ITEM;

    while (o != NON_ITEM)
    {
        int next = mitm[o].link;

        if (is_valid_item( mitm[o] ))
        {
            item_was_lost(mitm[o]);

            mitm[o].base_type = OBJ_UNASSIGNED;
            mitm[o].quantity = 0;
            mitm[o].props.clear();
        }

        o = next;
    }
}

void destroy_item_stack( int x, int y, int cause )
{
    int o = igrd[x][y];

    igrd[x][y] = NON_ITEM;

    while (o != NON_ITEM)
    {
        int next = mitm[o].link;

        if (is_valid_item( mitm[o] ))
        {
            item_was_destroyed(mitm[o], cause);

            mitm[o].base_type = OBJ_UNASSIGNED;
            mitm[o].quantity = 0;
            mitm[o].props.clear();
        }

        o = next;
    }
}

static bool _invisible_to_player( const item_def& item )
{
    return strstr(item.inscription.c_str(), "=k") != 0;
}

static int count_nonsquelched_items( int obj )
{
    int result = 0;
    while ( obj != NON_ITEM )
    {
        if ( !_invisible_to_player(mitm[obj]) )
            ++result;
        obj = mitm[obj].link;
    }
    return result;
}

/* Fill items with the items on a square.
   Squelched items (marked with =k) are ignored, unless
   the square contains *only* squelched items, in which case they
   are included. If force_squelch is true, squelched items are
   never displayed.
 */
static void _item_list_on_square( std::vector<const item_def*>& items,
                                  int obj, bool force_squelch )
{

    const bool have_nonsquelched = (force_squelch ||
                                    count_nonsquelched_items(obj));

    /* loop through the items */
    while ( obj != NON_ITEM )
    {
        /* add them to the items list if they qualify */
        if ( !have_nonsquelched || !_invisible_to_player(mitm[obj]) )
        {
            items.push_back( &mitm[obj] );
        }
        obj = mitm[obj].link;
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
static int item_name_specialness(const item_def& item)
{
    // All jewellery is worth looking at.
    // And we can always tell from the name if it's an artefact.
    if (item.base_type == OBJ_JEWELLERY )
        return ( is_artefact(item) ? 2 : 1 );

    if (item.base_type != OBJ_WEAPONS && item.base_type != OBJ_ARMOUR &&
        item.base_type != OBJ_MISSILES)
        return 0;
    if (item_type_known(item))
    {
        if ( is_artefact(item) )
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

    if ( item.base_type == OBJ_MISSILES )
        return 0;               // missiles don't get name descriptors

    std::string itname = item.name(DESC_PLAIN, false, false, false);
    lowercase(itname);

    // FIXME Maybe we should replace this with a test of ISFLAG_COSMETIC_MASK?
    const bool item_runed = itname.find("runed ") != std::string::npos;
    const bool heav_runed = itname.find("heavily ") != std::string::npos;
    const bool item_glows = itname.find("glowing") != std::string::npos;

    if ( item_glows || (item_runed && !heav_runed) ||
         get_equip_desc(item) == ISFLAG_EMBROIDERED_SHINY )
    {
        return 1;
    }

    // You can tell artefacts, because they'll have a description which
    // rules out anything else.
    // XXX Fixedarts and unrandarts might upset the apple-cart, though.
    if ( is_artefact(item) )
        return 2;

    return 0;
}

void item_check(bool verbose)
{

    describe_floor();
    origin_set(you.x_pos, you.y_pos);

    std::ostream& strm = msg::streams(MSGCH_FLOOR_ITEMS);

    std::vector<const item_def*> items;

    _item_list_on_square( items, igrd[you.x_pos][you.y_pos], true );

    if (items.size() == 0)
    {
        if ( verbose )
            strm << "There are no items here." << std::endl;
        return;
    }

    if (items.size() == 1 )
    {
        strm << "You see here " << items[0]->name(DESC_NOCAP_A)
             << '.' << std::endl;
        return;
    }

    bool done_init_line = false;

    if (static_cast<int>(items.size()) >= Options.item_stack_summary_minimum)
    {
        std::vector<unsigned short int> item_chars;
        for ( unsigned int i = 0; i < items.size() && i < 50; ++i )
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
        for ( unsigned int i = 0; i < item_chars.size(); ++i )
        {
            const int specialness = 10 - (item_chars[i] % 0x100);
            if ( specialness != cur_state )
            {
                switch ( specialness )
                {
                case 2: out_string += "<yellow>"; break; // artefact
                case 1: out_string += "<white>"; break;  // glowing/runed
                case 0: out_string += "<darkgrey>"; break; // mundane
                }
                cur_state = specialness;
            }

            out_string += static_cast<unsigned char>(item_chars[i] / 0x100);
            if (i + 1 < item_chars.size() &&
                (item_chars[i] / 0x100) != (item_chars[i+1] / 0x100))
                out_string += ' ';
        }
        formatted_mpr(formatted_string::parse_string(out_string),
                      MSGCH_FLOOR_ITEMS);
        done_init_line = true;
    }

    if ( verbose || static_cast<int>(items.size() + 1) < crawl_view.msgsz.y )
    {
        if ( !done_init_line )
            strm << "Things that are here:" << std::endl;
        for ( unsigned int i = 0; i < items.size(); ++i )
            strm << items[i]->name(DESC_NOCAP_A) << std::endl;
    }
    else if ( !done_init_line )
        strm << "There are many items here." << std::endl;

    if ( items.size() > 5 )
        learned_something_new(TUT_MULTI_PICKUP);
}

static void _pickup_menu(int item_link)
{
    std::vector<const item_def*> items;
    _item_list_on_square( items, item_link, false );

    std::vector<SelItem> selected =
        select_items( items, "Select items to pick up" );
    redraw_screen();

    for (int i = 0, count = selected.size(); i < count; ++i)
    {
        for (int j = item_link; j != NON_ITEM; j = mitm[j].link)
        {
            if (&mitm[j] == selected[i].item)
            {
                if (j == item_link)
                    item_link = mitm[j].link;

                unsigned long oldflags = mitm[j].flags;
                mitm[j].flags &= ~(ISFLAG_THROWN | ISFLAG_DROPPED);
                int result = move_item_to_player( j, selected[i].quantity );

                // If we cleared any flags on the items, but the pickup was
                // partial, reset the flags for the items that remain on the
                // floor.
                if (is_valid_item(mitm[j]))
                    mitm[j].flags = oldflags;

                if (result == 0)
                {
                    mpr("You can't carry that much weight.");
                    learned_something_new(TUT_HEAVY_LOAD);
                    return;
                }
                else if (result == -1)
                {
                    mpr("You can't carry that many items.");
                    learned_something_new(TUT_HEAVY_LOAD);
                    return;
                }
                break;
            }
        }
    }
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
        item.orig_monnum = -1;
    }
}

void origin_set_monster(item_def &item, const monsters *monster)
{
    if (!origin_known(item))
    {
        if (!item.orig_monnum)
            item.orig_monnum = monster->type + 1;
        item.orig_place = get_packed_place();
    }
}

void origin_purchased(item_def &item)
{
    // We don't need to check origin_known if it's a shop purchase
    item.orig_place  = get_packed_place();
    // Hackiness
    item.orig_monnum = -1;
}

void origin_acquired(item_def &item, int agent)
{
    // We don't need to check origin_known if it's a divine gift
    item.orig_place  = get_packed_place();
    // Hackiness
    item.orig_monnum = -2 - agent;
}

void origin_set_inventory(void (*oset)(item_def &item))
{
    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (is_valid_item(you.inv[i]))
            oset(you.inv[i]);
    }
}

static int first_corpse_monnum(int x, int y)
{
    // We could look for a corpse on this square and assume that the
    // items belonged to it, but that is unsatisfactory.
    return (0);
}

#ifdef DGL_MILESTONES
static std::string milestone_rune(const item_def &item)
{
    return std::string("found ") + item.name(DESC_NOCAP_A) + ".";
}

static void milestone_check(const item_def &item)
{
    if (item.base_type == OBJ_MISCELLANY
        && item.sub_type == MISC_RUNE_OF_ZOT)
    {
        mark_milestone("rune", milestone_rune(item));
    }
    else if (item.base_type == OBJ_ORBS && item.sub_type == ORB_ZOT)
    {
        mark_milestone("orb", "found the Orb of Zot!");
    }
}
#endif // DGL_MILESTONES

static void check_note_item(item_def &item)
{
    if (!(item.flags & ISFLAG_NOTED_GET) && is_interesting_item(item))
    {
        take_note(Note(NOTE_GET_ITEM, 0, 0, item.name(DESC_NOCAP_A).c_str(),
                       origin_desc(item).c_str()));
        item.flags |= ISFLAG_NOTED_GET;

        // If it's already fully identified when picked up, don't take
        // further notes.
        if ( fully_identified(item) )
            item.flags |= ISFLAG_NOTED_ID;
    }
}

void origin_set(int x, int y)
{
    int monnum = first_corpse_monnum(x, y);
    unsigned short pplace = get_packed_place();
    for (int link = igrd[x][y]; link != NON_ITEM; link = mitm[link].link)
    {
        item_def &item = mitm[link];
        if (origin_known(item))
            continue;

        if (!item.orig_monnum)
            item.orig_monnum = static_cast<short>( monnum );
        item.orig_place  = pplace;
#ifdef DGL_MILESTONES
        milestone_check(item);
#endif
    }
}

void origin_set_monstercorpse(item_def &item, int x, int y)
{
    item.orig_monnum = first_corpse_monnum(x, y);
}

static void _origin_freeze(item_def &item, int x, int y)
{
    if (!origin_known(item))
    {
        if (!item.orig_monnum && x != -1 && y != -1)
            origin_set_monstercorpse(item, x, y);

        item.orig_place = get_packed_place();
        check_note_item(item);
#ifdef DGL_MILESTONES
        milestone_check(item);
#endif
    }
}

static std::string _origin_monster_name(const item_def &item)
{
    const int monnum = item.orig_monnum - 1;
    if (monnum == MONS_PLAYER_GHOST)
        return ("a player ghost");
    else if (monnum == MONS_PANDEMONIUM_DEMON)
        return ("a demon");
    return mons_type_name(monnum, DESC_NOCAP_A);
}

static std::string _origin_monster_desc(const item_def &item)
{
    return (_origin_monster_name(item));
}

static std::string _origin_place_desc(const item_def &item)
{
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
    return (item.orig_place == 0xFFFFU && item.orig_monnum == -1);
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
            int iorig = -item.orig_monnum - 2;
            switch (iorig)
            {
            case -1:
                desc += "You bought " + _article_it(item) + " in a shop ";
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
                    desc += god_name(static_cast<god_type>(iorig))
                        + " gifted " + _article_it(item) + " to you ";
                else
                    // Bug really.
                    desc += "You stumbled upon " + _article_it(item) + " ";
                break;
            }
        }
        else if (item.orig_monnum - 1 == MONS_DANCING_WEAPON)
            desc += "You subdued it ";
        else
            desc += "You took " + _article_it(item) + " off "
                    + _origin_monster_desc(item) + " ";
    }
    else
        desc += "You found " + _article_it(item) + " ";

    desc += _origin_place_desc(item);
    return (desc);
}

bool pickup_single_item(int link, int qty)
{
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_AIR
        && you.duration[DUR_TRANSFORMATION] > 0)
    {
        mpr("You can't pick up anything in this form!");
        return (false);
    }

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

    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_AIR
        && you.duration[DUR_TRANSFORMATION] > 0)
    {
        mpr("You can't pick up anything in this form!");
        return;
    }

    if (you.flight_mode() == FL_LEVITATE)
    {
        mpr("You can't reach the floor from up here.");
        return;
    }

    int o = igrd[you.x_pos][you.y_pos];
    const int num_nonsquelched = count_nonsquelched_items(o);

    if (o == NON_ITEM)
    {
        mpr("There are no items here.");
    }
    else if (mitm[o].link == NON_ITEM)      // just one item?
    {
        // deliberately allowing the player to pick up
        // a killed item here
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
        while ( o != NON_ITEM )
        {
            // must save this because pickup can destroy the item
            next = mitm[o].link;

            if ( num_nonsquelched && _invisible_to_player(mitm[o]) )
            {
                o = next;
                continue;
            }

            if (keyin != 'a')
            {
                mprf(MSGCH_PROMPT, "Pick up %s? (y/n/a/*?g,/q)",
                     mitm[o].name(DESC_NOCAP_A).c_str() );
#ifndef USE_TILE
                keyin = get_ch();
#else
                keyin = getch_ck();
#endif
            }

            if (keyin == '*' || keyin == '?' || keyin == ',' || keyin == 'g'
#ifdef USE_TILE
                || keyin == CK_MOUSE_B1
#endif
                )
            {
                _pickup_menu(o);
                break;
            }

            if (keyin == 'q' || keyin == ESCAPE)
                break;

            if (keyin == 'y' || keyin == 'a')
            {
                mitm[o].flags &= ~(ISFLAG_THROWN | ISFLAG_DROPPED);
                int result = move_item_to_player( o, mitm[o].quantity );

                if (result == 0)
                {
                    mpr("You can't carry that much weight.");
                    keyin = 'x';        // resets from 'a'
                }
                else if (result == -1)
                {
                    mpr("You can't carry that many items.");
                    break;
                }
            }

            o = next;
        }
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

bool items_stack( const item_def &item1, const item_def &item2,
                  bool force_merge )
{
    // both items must be stackable
    if (!force_merge
        && (!is_stackable_item( item1 ) || !is_stackable_item( item2 )))
    {
        return (false);
    }

    // base and sub-types must always be the same to stack
    if (item1.base_type != item2.base_type || item1.sub_type != item2.sub_type)
        return (false);

    ASSERT(force_merge || item1.base_type != OBJ_WEAPONS);

    if (item1.base_type == OBJ_GOLD)
        return (true);

    // These classes also require pluses and special
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

    // Check the ID flags
    if (ident_flags(item1) != ident_flags(item2))
        return false;

    // Check the non-ID flags, but ignore dropped, thrown, cosmetic,
    // and note flags. Also, whether item was in inventory before.
#define NON_IDENT_FLAGS ~(ISFLAG_IDENT_MASK | ISFLAG_COSMETIC_MASK | \
                          ISFLAG_DROPPED | ISFLAG_THROWN | \
                          ISFLAG_NOTED_ID | ISFLAG_NOTED_GET | \
                          ISFLAG_BEEN_IN_INV)

    if ((item1.flags & NON_IDENT_FLAGS) != (item2.flags & NON_IDENT_FLAGS))
    {
        return false;
    }

    if (item1.base_type == OBJ_POTIONS)
    {
        // Thanks to mummy cursing, we can have potions of decay
        // that don't look alike... so we don't stack potions
        // if either isn't identified and they look different.  -- bwr
        if (item1.plus != item2.plus
            && (!item_type_known(item1) || !item_type_known(item2)))
        {
            return false;
        }
    }

    // The inscriptions can differ if one of them is blank, but if they
    // are differing non-blank inscriptions then don't stack.
    if (item1.inscription != item2.inscription
        && item1.inscription != "" && item2.inscription != "")
    {
        return false;
    }

    return (true);
}

static int userdef_find_free_slot(const item_def &i)
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
    int slot = userdef_find_free_slot(i);
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
    for (slot = 0; slot < ENDOFPACK; ++slot) {
        if (!is_valid_item(you.inv[slot]))
            return slot;
    }

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
}

// Returns quantity of items moved into player's inventory and -1 if
// the player's inventory is full.
int move_item_to_player( int obj, int quant_got, bool quiet )
{
    if (item_is_stationary(mitm[obj]))
    {
        mpr("You cannot pick up the net that holds you!");
        return (1);
    }

    int retval = quant_got;

    // Gold has no mass, so we handle it first.
    if (mitm[obj].base_type == OBJ_GOLD)
    {
        you.gold += quant_got;
        dec_mitm_item_quantity( obj, quant_got );
        you.redraw_gold = 1;

        if (!quiet)
        {
            mprf("You pick up %d gold piece%s, bringing you to a "
                 "total of %d gold pieces.",
                 quant_got, (quant_got > 1) ? "s" : "", you.gold );
        }

        you.turn_is_over = true;
        return (retval);
    }

    const int unit_mass = item_mass( mitm[obj] );
    if (quant_got > mitm[obj].quantity || quant_got <= 0)
        quant_got = mitm[obj].quantity;

    const int imass = unit_mass * quant_got;

    bool partial_pickup = false;

    if (you.burden + imass > carrying_capacity())
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

                check_note_item(mitm[obj]);

                // If the object on the ground is inscribed, but not
                // the one in inventory, then the inventory object
                // picks up the other's inscription.
                if (mitm[obj].inscription != ""
                    && you.inv[m].inscription == "")
                {
                    you.inv[m].inscription = mitm[obj].inscription;
                }

                if (mitm[obj].base_type == OBJ_POTIONS
                    && (mitm[obj].sub_type == POT_BLOOD
                        || mitm[obj].sub_type == POT_BLOOD_COAGULATED))
                {
                    pick_up_blood_potions_stack(mitm[obj], quant_got);
                }
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
        // Something is terribly wrong
        return (-1);
    }

    coord_def pos(mitm[obj].x, mitm[obj].y);
    dungeon_events.fire_position_event(
        dgn_event(DET_ITEM_PICKUP, pos, 0, obj, -1), pos);

    item_def &item = you.inv[freeslot];
    // copy item
    item        = mitm[obj];
    item.x      = -1;
    item.y      = -1;
    item.link   = freeslot;

    if (!item.slot)
        item.slot = index_to_letter(item.link);

    _autoinscribe_item( item );

    _origin_freeze(item, you.x_pos, you.y_pos);
    check_note_item(item);

    item.quantity = quant_got;
    if (mitm[obj].base_type == OBJ_POTIONS
        && (mitm[obj].sub_type == POT_BLOOD
            || mitm[obj].sub_type == POT_BLOOD_COAGULATED))
    {
        if (quant_got != mitm[obj].quantity)
        {
            // remove oldest timers from original stack
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

// Moves mitm[obj] to (x,y)... will modify the value of obj to
// be the index of the final object (possibly different).
//
// Done this way in the hopes that it will be obvious from
// calling code that "obj" is possibly modified.
bool move_item_to_grid( int *const obj, int x, int y )
{
    // must be a valid reference to a valid object
    if (*obj == NON_ITEM || !is_valid_item( mitm[*obj] ))
        return (false);

    // If it's a stackable type...
    if (is_stackable_item( mitm[*obj] ))
    {
        // Look for similar item to stack:
        for (int i = igrd[x][y]; i != NON_ITEM; i = mitm[i].link)
        {
            // check if item already linked here -- don't want to unlink it
            if (*obj == i)
                return (false);

            if (items_stack( mitm[*obj], mitm[i] ))
            {
                // Add quantity to item already here, and dispose
                // of obj, while returning the found item. -- bwr
                inc_mitm_item_quantity( i, mitm[*obj].quantity );
                destroy_item( *obj );
                *obj = i;
                return (true);
            }
        }
    }
    // Non-stackable item that's been fudge-stacked (monster throwing weapons).
    // Explode the stack when dropped. We have to special case chunks, ew.
    else if (mitm[*obj].quantity > 1
             && (mitm[*obj].base_type != OBJ_FOOD
                 || mitm[*obj].sub_type != FOOD_CHUNK))
    {
        while (mitm[*obj].quantity > 1)
        {
            // If we can't copy the items out, we lose the surplus.
            if (copy_item_to_grid(mitm[*obj], x, y, 1, false))
                --mitm[*obj].quantity;
            else
                mitm[*obj].quantity = 1;
        }
    }

    ASSERT( *obj != NON_ITEM );

    // Need to actually move object, so first unlink from old position.
    unlink_item( *obj );

    // move item to coord:
    mitm[*obj].x = x;
    mitm[*obj].y = y;

    // link item to top of list.
    mitm[*obj].link = igrd[x][y];
    igrd[x][y] = *obj;

    if (is_rune(mitm[*obj]))
    {
        if (player_in_branch(BRANCH_HALL_OF_ZOT))
            you.attribute[ATTR_RUNES_IN_ZOT] += mitm[*obj].quantity;
    }
    else if (mitm[*obj].base_type == OBJ_ORBS
             && you.level_type == LEVEL_DUNGEON)
    {
        set_branch_flags(BFLAG_HAS_ORB);
    }

    return (true);
}

void move_item_stack_to_grid( int x, int y, int targ_x, int targ_y )
{
    // Tell all items in stack what the new coordinate is.
    for (int o = igrd[x][y]; o != NON_ITEM; o = mitm[o].link)
    {
        mitm[o].x = targ_x;
        mitm[o].y = targ_y;
    }

    igrd[targ_x][targ_y] = igrd[x][y];
    igrd[x][y] = NON_ITEM;
}


// returns quantity dropped
bool copy_item_to_grid( const item_def &item, int x_plos, int y_plos,
                        int quant_drop, bool mark_dropped )
{
    if (quant_drop == 0)
        return (false);

    // default quant_drop == -1 => drop all
    if (quant_drop < 0)
        quant_drop = item.quantity;

    // loop through items at current location
    if (is_stackable_item( item ))
    {
        for (int i = igrd[x_plos][y_plos]; i != NON_ITEM; i = mitm[i].link)
        {
            if (items_stack( item, mitm[i] ))
            {
                inc_mitm_item_quantity( i, quant_drop );

                if (item.base_type == OBJ_POTIONS
                    && (item.sub_type == POT_BLOOD
                        || item.sub_type == POT_BLOOD_COAGULATED))
                {
                    item_def help = item;
                    drop_blood_potions_stack(help, quant_drop, x_plos, y_plos);
                }

                // If the items on the floor already have a nonzero slot,
                // leave it as such, otherwise set the slot.
                if (mark_dropped && !mitm[i].slot)
                    mitm[i].slot = index_to_letter(item.link);
                return (true);
            }
        }
    }

    // item not found in current stack, add new item to top.
    int new_item = get_item_slot(10);
    if (new_item == NON_ITEM)
        return (false);

    // copy item
    mitm[new_item] = item;

    // set quantity, and set the item as unlinked
    mitm[new_item].quantity = quant_drop;
    mitm[new_item].x = 0;
    mitm[new_item].y = 0;
    mitm[new_item].link = NON_ITEM;

    if (mark_dropped)
    {
        mitm[new_item].slot = index_to_letter(item.link);
        mitm[new_item].flags |= ISFLAG_DROPPED;
        mitm[new_item].flags &= ~ISFLAG_THROWN;
        origin_set_unknown(mitm[new_item]);
    }

    move_item_to_grid( &new_item, x_plos, y_plos );
    if (item.base_type == OBJ_POTIONS
        && (item.sub_type == POT_BLOOD
            || item.sub_type == POT_BLOOD_COAGULATED)
        && item.quantity != quant_drop) // partial drop only
    {
        // since only the oldest potions have been dropped,
        // remove the newest ones
        remove_newest_blood_potion(mitm[new_item]);
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
    move_item_to_grid( &item, dest.x, dest.y );

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
        unwield_item();
        canned_msg( MSG_EMPTY_HANDED );
    }

    const dungeon_feature_type my_grid = grd[you.x_pos][you.y_pos];

    if ( !grid_destroys_items(my_grid)
         && !copy_item_to_grid( you.inv[item_dropped],
                                you.x_pos, you.y_pos, quant_drop, true ))
    {
        mpr( "Too many items on this level, not dropping the item." );
        return (false);
    }

    mprf("You drop %s.",
         quant_name(you.inv[item_dropped], quant_drop, DESC_NOCAP_A).c_str());

    if ( grid_destroys_items(my_grid) )
    {
        if( !silenced(you.pos()) )
            mprf(MSGCH_SOUND, grid_item_destruction_message(my_grid));

        item_was_destroyed(you.inv[item_dropped], NON_MONSTER);
    }
    else if (strstr(you.inv[item_dropped].inscription.c_str(), "=s") != 0)
        StashTrack.add_stash();

    if (you.inv[item_dropped].base_type == OBJ_POTIONS
        && (you.inv[item_dropped].sub_type == POT_BLOOD
            || you.inv[item_dropped].sub_type == POT_BLOOD_COAGULATED)
        && you.inv[item_dropped].quantity != quant_drop)
    {
        // oldest potions have been dropped
        for (int i = 0; i < quant_drop; i++)
            remove_oldest_blood_potion(you.inv[item_dropped]);
    }
    dec_inv_item_quantity( item_dropped, quant_drop );
    you.turn_is_over = true;

    if (try_offer
        && you.religion != GOD_NO_GOD
        && you.duration[DUR_PRAYER]
        && grid_altar_god(grd[you.x_pos][you.y_pos]) == you.religion)
    {
        offer_items();
    }

    return (true);
}

static std::string drop_menu_invstatus(const Menu *menu)
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

static std::string drop_menu_title(const Menu *menu, const std::string &oldt)
{
    std::string res = drop_menu_invstatus(menu) + " " + oldt;
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

static std::string drop_selitem_text( const std::vector<MenuEntry*> *s )
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
static bool drop_item_order(const SelItem &first, const SelItem &second)
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
void drop(void)
{
    if (inv_count() < 1 && you.gold == 0)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    std::vector<SelItem> tmp_items;
    tmp_items = prompt_invent_items( "Drop what?", MT_DROP, -1,
                                     drop_menu_title, true, true, 0,
                                     &Options.drop_filter, drop_selitem_text,
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
    std::sort( tmp_items.begin(), tmp_items.end(), drop_item_order );

    // If the user answers "no" to an item an with a warning inscription,
    // then remove it from the list of items to drop by not copying it
    // over to items_for_multidrop
    items_for_multidrop.clear();
    for ( unsigned int i = 0; i < tmp_items.size(); ++i )
        if ( check_warning_inscriptions( *(tmp_items[i].item), OPER_DROP))
            items_for_multidrop.push_back(tmp_items[i]);

    if ( items_for_multidrop.empty() ) // only one item
    {
        canned_msg( MSG_OK );
        return;
    }

    if ( items_for_multidrop.size() == 1 ) // only one item
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
    std::string iname = item.name(DESC_INVENTORY);

    /* if there's an inscription already, do nothing */
    if ( item.inscription.size() > 0 )
        return;

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
            if ((item.flags & ISFLAG_DROPPED)
                && (item.x != -1 || item.y != -1))
            {
                str = replace_all(str, "=g", "");
            }

            // Note that this might cause the item inscription to
            // pass 80 characters.
            item.inscription += str;
        }
    }
}

static void _autoinscribe_floor_items()
{
    int o, next;
    o = igrd[you.x_pos][you.y_pos];

    while (o != NON_ITEM)
    {
        next = mitm[o].link;
        _autoinscribe_item( mitm[o] );
        o = next;
    }
}

static void _autoinscribe_inventory()
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item(you.inv[i]))
            _autoinscribe_item( you.inv[i] );
    }
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

static inline std::string autopickup_item_name(const item_def &item)
{
    return userdef_annotate_item(STASH_LUA_SEARCH_ANNOTATE, &item, true)
        + item.name(DESC_PLAIN);
}

static bool is_denied_autopickup(const item_def &item, std::string &iname)
{
    if (iname.empty())
        iname = autopickup_item_name(item);
    for (unsigned i = 0, size = Options.never_pickup.size(); i < size; ++i)
    {
        if (Options.never_pickup[i].matches(iname))
            return (true);
    }
    return false;
}

static bool is_forced_autopickup(const item_def &item, std::string &iname)
{
    if (iname.empty())
        iname = autopickup_item_name(item);
    for (unsigned i = 0, size = Options.always_pickup.size(); i < size; ++i)
    {
        if (Options.always_pickup[i].matches(iname))
            return (true);
    }
    return false;
}

bool item_needs_autopickup(const item_def &item)
{
    if (item_is_stationary(item))
        return (false);

    if (strstr(item.inscription.c_str(), "=g") != 0)
        return (true);

    if ((item.flags & ISFLAG_THROWN) && Options.pickup_thrown)
        return (true);

    std::string itemname;
    return (((Options.autopickups & (1L << item.base_type))
#ifdef CLUA_BINDINGS
             && clua.callbooleanfn(true, "ch_autopickup", "u", &item)
#endif
             || is_forced_autopickup(item, itemname))
            && (Options.pickup_dropped || !(item.flags & ISFLAG_DROPPED))
            && !is_denied_autopickup(item, itemname));
}

bool can_autopickup()
{
    // [ds] Checking for autopickups == 0 is a bad idea because
    // autopickup is still possible with inscriptions and
    // pickup_thrown.
    if (!Options.autopickup_on)
        return (false);

    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_AIR
        && you.duration[DUR_TRANSFORMATION] > 0)
    {
        return (false);
    }

    if (you.flight_mode() == FL_LEVITATE)
        return (false);

    if ( !i_feel_safe() )
        return (false);

    return (true);
}

static void do_autopickup()
{
    //David Loewenstern 6/99
    int n_did_pickup = 0;
    int n_tried_pickup = 0;

    will_autopickup = false;

    if (!can_autopickup())
    {
        item_check(false);
        return;
    }

    int o = igrd[you.x_pos][you.y_pos];

    std::string pickup_warning;
    while (o != NON_ITEM)
    {
        const int next = mitm[o].link;

        if (item_needs_autopickup(mitm[o]))
        {
            int num_to_take = mitm[o].quantity;
            if ( Options.autopickup_no_burden && item_mass(mitm[o]) != 0)
            {
                int num_can_take =
                    (carrying_capacity(you.burden_state) - you.burden) /
                    item_mass(mitm[o]);

                if ( num_can_take < num_to_take )
                {
                    if (!n_tried_pickup)
                        mpr("You can't pick everything up without burdening "
                            "yourself.");
                    n_tried_pickup++;
                    num_to_take = num_can_take;
                }

                if ( num_can_take == 0 )
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
    do_autopickup();
}

int inv_count(void)
{
    int count=0;

    for(int i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item( you.inv[i] ))
            count++;
    }

    return count;
}

static bool find_subtype_by_name(item_def &item,
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
    set_ident_flags( item, ISFLAG_KNOW_TYPE | ISFLAG_KNOW_PROPERTIES );

    int type_wanted = -1;

    for (int i = 0; i < ntypes; i++)
    {
        item.sub_type = i;
        if (name == lowercase_string(item.name(DESC_PLAIN)))
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
        for (unsigned i = 0; i < sizeof(max_subtype) / sizeof(*max_subtype);
             ++i)
        {
            if (!max_subtype[i])
                continue;
            if (find_subtype_by_name(item, static_cast<object_class_type>(i),
                                     max_subtype[i], name))
                break;
        }
    }
    else
    {
        find_subtype_by_name(item, base_type, max_subtype[base_type], name);
    }

    return (item);
}

bool item_is_equipped(const item_def &item)
{
    if (item.x != -1 || item.y != -1)
        return (false);

    for (int i = 0; i < NUM_EQUIP; i++)
    {
        if (you.equip[i] == EQ_NONE)
            continue;

        item_def& eq(you.inv[you.equip[i]]);

        if (!is_valid_item(eq))
            continue;

        if (eq.slot == item.slot)
            return (true);
        else if (&eq == &item)
            return (true);
    }

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
    return (base_type == OBJ_BOOKS?   sub_type                      :
            base_type == OBJ_STAVES?  sub_type + 50 - STAFF_SMITING :
            -1);
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

int item_def::zap() const
{
    if (base_type != OBJ_WANDS)
        return ZAP_DEBUGGING_RAY;

    zap_type result;
    switch (sub_type)
    {
    case WAND_ENSLAVEMENT:    result = ZAP_ENSLAVEMENT;     break;
    case WAND_DRAINING:       result = ZAP_NEGATIVE_ENERGY; break;
    case WAND_DISINTEGRATION: result = ZAP_DISINTEGRATION;  break;

    case WAND_RANDOM_EFFECTS:
        result = static_cast<zap_type>(random2(16));
        if ( one_chance_in(20) )
            result = ZAP_NEGATIVE_ENERGY;
        if ( one_chance_in(17) )
            result = ZAP_ENSLAVEMENT;
        break;

    default:
        result = static_cast<zap_type>(sub_type);
        break;
    }
    return result;
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
