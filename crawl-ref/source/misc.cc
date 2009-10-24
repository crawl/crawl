/*
 *  File:       misc.cc
 *  Summary:    Misc functions.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *   <4>   april2009     Cha    runes_in_pack no longer static
 *   <3>   11/14/99      cdl    evade with random40(ev) vice random2(ev)
 *   <2>    5/20/99      BWR    Multi-user support, new berserk code.
 *   <1>    -/--/--      LRH    Created
 */

#include "AppHdr.h"
#include "misc.h"
#include "notes.h"

#include <string.h>
#include <algorithm>

#if !defined(__IBMCPP__) && !defined(_MSC_VER)
#include <unistd.h>
#endif

#ifdef __MINGW32__
#include <io.h>
#endif

#include <cstdlib>
#include <cstdio>
#include <cmath>

#ifdef DOS
#include <conio.h>
#endif

#include "externs.h"
#include "misc.h"

#include "abyss.h"
#include "branch.h"
#include "chardump.h"
#include "clua.h"
#include "cloud.h"
#include "database.h"
#include "delay.h"
#include "directn.h"
#include "dgnevent.h"
#include "directn.h"
#include "dungeon.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "format.h"
#include "hiscores.h"
#include "it_use2.h"
#include "itemprop.h"
#include "items.h"
#include "lev-pand.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "mon-util.h"
#include "monstuff.h"
#include "ouch.h"
#include "output.h"
#include "overmap.h"
#include "place.h"
#include "player.h"
#include "religion.h"
#include "shopping.h"
#include "skills.h"
#include "skills2.h"
#include "spells3.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "tiles.h"
#include "terrain.h"
#include "transfor.h"
#include "traps.h"
#include "travel.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

static void create_monster_hide(int mons_class)
{
    int o = get_item_slot();
    if (o == NON_ITEM)
        return;

    mitm[o].quantity = 1;

    // These values are common to all: {dlb}
    mitm[o].base_type = OBJ_ARMOUR;
    mitm[o].plus      = 0;
    mitm[o].plus2     = 0;
    mitm[o].special   = 0;
    mitm[o].flags     = 0;
    mitm[o].colour    = mons_class_colour( mons_class );

    // These values cannot be set by a reasonable formula: {dlb}
    switch (mons_class)
    {
    case MONS_DRAGON:
        mitm[o].sub_type = ARM_DRAGON_HIDE;
        break;
    case MONS_TROLL:
        mitm[o].sub_type = ARM_TROLL_HIDE;
        break;
    case MONS_ICE_DRAGON:
        mitm[o].sub_type = ARM_ICE_DRAGON_HIDE;
        break;
    case MONS_STEAM_DRAGON:
        mitm[o].sub_type = ARM_STEAM_DRAGON_HIDE;
        break;
    case MONS_MOTTLED_DRAGON:
        mitm[o].sub_type = ARM_MOTTLED_DRAGON_HIDE;
        break;
    case MONS_STORM_DRAGON:
        mitm[o].sub_type = ARM_STORM_DRAGON_HIDE;
        break;
    case MONS_GOLDEN_DRAGON:
        mitm[o].sub_type = ARM_GOLD_DRAGON_HIDE;
        break;
    case MONS_SWAMP_DRAGON:
        mitm[o].sub_type = ARM_SWAMP_DRAGON_HIDE;
        break;
    case MONS_SHEEP:
    case MONS_YAK:
    default:
        mitm[o].sub_type = ARM_ANIMAL_SKIN;
        break;
    }

    move_item_to_grid( &o, you.x_pos, you.y_pos );
}

// Vampire draining corpses currently leaves them a time of 90, while the
// default time is 200. I'm not sure whether this is for balancing reasons
// or just an arbitrary difference. (jpeg)
void turn_corpse_into_skeleton(item_def &corpse, int time)
{
    ASSERT(corpse.base_type == OBJ_CORPSES && corpse.sub_type == CORPSE_BODY);

    // Some monsters' corpses lack the structure to leave skeletons behind.
    if (!mons_skeleton( corpse.plus ))
        return;

    // While it is possible to distinguish draconian corpses by colour, their
    // skeletons are indistinguishable.
    if (mons_genus(corpse.plus) == MONS_DRACONIAN
        && corpse.plus != MONS_DRACONIAN)
    {
        corpse.plus = MONS_DRACONIAN;
    }

    corpse.sub_type = CORPSE_SKELETON;
    corpse.special  = time;
    corpse.colour   = LIGHTGREY;
}

void turn_corpse_into_chunks( item_def &item )
{
    ASSERT( item.base_type == OBJ_CORPSES );

    const int mons_class = item.plus;
    const int max_chunks = mons_weight( mons_class ) / 150;

    // Only fresh corpses bleed enough to colour the ground.
    if (!food_is_rotten(item))
        bleed_onto_floor(you.x_pos, you.y_pos, mons_class, max_chunks, true);

    item.base_type = OBJ_FOOD;
    item.sub_type  = FOOD_CHUNK;
    item.quantity  = 1 + random2( max_chunks );
    item.quantity = stepdown_value( item.quantity, 4, 4, 12, 12 );

    if (you.species != SP_VAMPIRE)
        item.flags &= ~(ISFLAG_THROWN | ISFLAG_DROPPED);

    // Happens after the corpse has been butchered.
    if (monster_descriptor(mons_class, MDSC_LEAVES_HIDE) && !one_chance_in(3))
        create_monster_hide(mons_class);
}

// Initialize blood potions with a vector of timers.
void init_stack_blood_potions(item_def &stack, int age)
{
    ASSERT(is_blood_potion(stack));

    CrawlHashTable &props = stack.props;
    props.clear(); // sanity measure
    props.set_default_flags(SFLAG_CONST_TYPE);
    props["timer"].new_vector(SV_LONG);
    CrawlVector &timer = props["timer"].get_vector();

    if (age == -1)
    {
        if (stack.sub_type == POT_BLOOD)
            age = 2000;
        else // coagulated blood
            age = 500;
    }
    // For a newly created stack, all potions use the same timer.
    const long max_age = you.num_turns + age;
#ifdef DEBUG_BLOOD_POTIONS
    mprf(MSGCH_DIAGNOSTICS, "newly created stack will time out at turn %d",
                            max_age);
#endif
    for (int i = 0; i < stack.quantity; i++)
        timer.push_back(max_age);

    stack.special = 0;
    ASSERT(timer.size() == stack.quantity);
    props.assert_validity();
}

// Sort a CrawlVector<long>, should probably be done properly with templates.
static void _long_sort(CrawlVector &vec)
{
    std::vector<long> help;
    while (!vec.empty())
    {
        help.push_back(vec[vec.size()-1].get_long());
        vec.pop_back();
    }

    std::sort(help.begin(), help.end());

    while (!help.empty())
    {
        vec.push_back(help[help.size()-1]);
        help.pop_back();
    }
}

void maybe_coagulate_blood_potions_floor(int obj)
{
    item_def &blood = mitm[obj];
    ASSERT(is_valid_item(blood));
    ASSERT(is_blood_potion(blood));

    CrawlHashTable &props = blood.props;
    if (!props.exists("timer"))
        init_stack_blood_potions(blood, blood.special);

    ASSERT(props.exists("timer"));
    CrawlVector &timer = props["timer"].get_vector();
    ASSERT(!timer.empty());
    ASSERT(timer.size() == blood.quantity);

    // blood.sub_type could be POT_BLOOD or POT_BLOOD_COAGULATED
    // -> need different handling
    int rot_limit  = you.num_turns;
    int coag_limit = you.num_turns + 500; // check 500 turns later

    // First count whether coagulating is even necessary.
    int rot_count  = 0;
    int coag_count = 0;
    std::vector<long> age_timer;
    long current;
    while (!timer.empty())
    {
        current = timer[timer.size()-1].get_long();
        if (current > coag_limit
            || blood.sub_type == POT_BLOOD_COAGULATED && current > rot_limit)
        {
            // Still some time until rotting/coagulating.
            break;
        }

        timer.pop_back();
        if (current <= rot_limit)
            rot_count++;
        else if (blood.sub_type == POT_BLOOD && current <= coag_limit)
        {
            coag_count++;
            age_timer.push_back(current);
        }
    }

    if (!rot_count && !coag_count)
        return; // nothing to be done

#ifdef DEBUG_BLOOD_POTIONS
    mprf(MSGCH_DIAGNOSTICS, "in maybe_coagulate_blood_potions_FLOOR "
                            "(turns: %d)", you.num_turns);

    mprf(MSGCH_DIAGNOSTICS, "Something happened at pos (%d, %d)!",
                            blood.x, blood.y);
    mprf(MSGCH_DIAGNOSTICS, "coagulated: %d, rotted: %d, total: %d",
                            coag_count, rot_count, blood.quantity);
    more();
#endif

    if (!coag_count) // Some potions rotted away.
    {
        dec_mitm_item_quantity(obj, rot_count);
        // Timer is already up to date.
        return;
    }

    // Coagulated blood cannot coagulate any further...
    ASSERT(blood.sub_type == POT_BLOOD);

    // Now that coagulating is necessary, check square for !coagulated blood.
    ASSERT(blood.x >= 0 && blood.y >= 0);
    for (int o = igrd[blood.x][blood.y]; o != NON_ITEM; o = mitm[o].link)
    {
        if (mitm[o].base_type == OBJ_POTIONS
            && mitm[o].sub_type == POT_BLOOD_COAGULATED)
        {
            // Merge with existing stack.
            CrawlHashTable &props2 = mitm[o].props;
            if (!props2.exists("timer"))
                init_stack_blood_potions(mitm[o], mitm[o].special);

            ASSERT(props2.exists("timer"));
            CrawlVector &timer2 = props2["timer"].get_vector();
            ASSERT(timer2.size() == mitm[o].quantity);

            // Update timer -> push(pop).
            long val;
            while (!age_timer.empty())
            {
                val = age_timer[age_timer.size() - 1];
                age_timer.pop_back();
                timer2.push_back(val);
            }
            _long_sort(timer2);
            inc_mitm_item_quantity(o, coag_count);
            ASSERT(timer2.size() == mitm[o].quantity);
            dec_mitm_item_quantity(obj, rot_count + coag_count);
            return;
        }
    }
    // If we got here, nothing was found!

    // Entire stack is gone, rotted or coagulated.
    // -> Change potions to coagulated type.
    if (rot_count + coag_count == blood.quantity)
    {
        ASSERT(timer.empty());

        // Update subtype.
        blood.sub_type = POT_BLOOD_COAGULATED;
        item_colour(blood);

        // Re-fill vector.
        long val;
        while (!age_timer.empty())
        {
            val = age_timer[age_timer.size() - 1];
            age_timer.pop_back();
            timer.push_back(val);
        }
        dec_mitm_item_quantity(obj, rot_count);
        ASSERT(timer.size() == blood.quantity);
        return;
    }

    // Else, create a new stack of potions.
    int o = get_item_slot( 20 );
    if (o == NON_ITEM)
        return;

    item_def &item = mitm[o];
    item.base_type = OBJ_POTIONS;
    item.sub_type  = POT_BLOOD_COAGULATED;
    item.quantity  = coag_count;
    item.plus      = 0;
    item.plus2     = 0;
    item.special   = 0;
    item.flags     = 0;
    item_colour(item);

    CrawlHashTable &props_new = item.props;
    props_new.set_default_flags(SFLAG_CONST_TYPE);
    props_new["timer"].new_vector(SV_LONG);
    CrawlVector &timer_new = props_new["timer"].get_vector();

    long val;
    while (!age_timer.empty())
    {
        val = age_timer[age_timer.size() - 1];
        age_timer.pop_back();
        timer_new.push_back(val);
    }

    ASSERT(timer_new.size() == coag_count);
    props_new.assert_validity();
    move_item_to_grid( &o, blood.x, blood.y );

    dec_mitm_item_quantity(obj, rot_count + coag_count);
    ASSERT(timer.size() == blood.quantity);
}

// Prints messages for blood potions coagulating in inventory (coagulate = true)
// or whenever potions are cursed into potions of decay (coagulate = false).
static void _potion_stack_changed_message(item_def &potion, int num_changed,
                                          bool coagulate = true)
{
    ASSERT(num_changed > 0);
    if (coagulate)
        ASSERT(potion.sub_type == POT_BLOOD);

    std::string msg;
    if (potion.quantity == num_changed)
        msg = "Your ";
    else if (num_changed == 1)
        msg = "One of your ";
    else if (num_changed == 2)
        msg = "Two of your ";
    else if (num_changed >= (potion.quantity * 3) / 4)
        msg = "Most of your ";
    else
        msg = "Some of your ";

    msg += potion.name(DESC_PLAIN, false);

    if (coagulate)
        msg += " coagulate";
    else
        msg += " decay";

    if (num_changed == 1)
        msg += "s";
    msg += ".";

    mpr(msg.c_str(), MSGCH_ROTTEN_MEAT);
}

// Returns true if "equipment weighs less" message needed.
// Also handles coagulation messages.
bool maybe_coagulate_blood_potions_inv(item_def &blood)
{
    ASSERT(is_valid_item(blood));
    ASSERT(is_blood_potion(blood));

    CrawlHashTable &props = blood.props;
    if (!props.exists("timer"))
        init_stack_blood_potions(blood, blood.special);

    ASSERT(props.exists("timer"));
    CrawlVector &timer = props["timer"].get_vector();
    ASSERT(timer.size() == blood.quantity);
    ASSERT(!timer.empty());

    // blood.sub_type could be POT_BLOOD or POT_BLOOD_COAGULATED
    // -> need different handling
    int rot_limit  = you.num_turns;
    int coag_limit = you.num_turns + 500; // check 500 turns later

    // First count whether coagulating is even necessary.
    int rot_count  = 0;
    int coag_count = 0;
    std::vector<long> age_timer;
    long current;
    const int size = timer.size();
    for (int i = 0; i < size; i++)
    {
        current = timer[timer.size()-1].get_long();
        if (current > coag_limit
            || blood.sub_type == POT_BLOOD_COAGULATED && current > rot_limit)
        {
            // Still some time until rotting/coagulating.
            break;
        }

        timer.pop_back();
        if (current <= rot_limit)
            rot_count++;
        else if (blood.sub_type == POT_BLOOD && current <= coag_limit)
        {
            coag_count++;
            age_timer.push_back(current);
        }
    }

    if (!rot_count && !coag_count)
        return (false); // Nothing to be done.

#ifdef DEBUG_BLOOD_POTIONS
    mprf(MSGCH_DIAGNOSTICS, "in maybe_coagulate_blood_potions_INV "
                            "(turns: %d)", you.num_turns);

    mprf(MSGCH_DIAGNOSTICS, "coagulated: %d, rotted: %d, total: %d",
                            coag_count, rot_count, blood.quantity);
    more();
#endif

    // just in case
    you.wield_change  = true;
    you.redraw_quiver = true;

    if (!coag_count) // Some potions rotted away.
    {
        blood.quantity -= rot_count;
        if (blood.quantity < 1)
        {
            if (you.equip[EQ_WEAPON] == blood.link)
                you.equip[EQ_WEAPON] = -1;

            destroy_item(blood);
        }
        else
            ASSERT(blood.quantity == timer.size());

        return (true);
    }

    // Coagulated blood cannot coagulate any further...
    ASSERT(blood.sub_type == POT_BLOOD);

    bool knew_blood = get_ident_type(OBJ_POTIONS, POT_BLOOD) == ID_KNOWN_TYPE;
    bool knew_coag  = (get_ident_type(OBJ_POTIONS, POT_BLOOD_COAGULATED)
                           == ID_KNOWN_TYPE);

    _potion_stack_changed_message(blood, coag_count);

    // Identify both blood and coagulated blood, if necessary.
    if (!knew_blood)
        set_ident_type( OBJ_POTIONS, POT_BLOOD, ID_KNOWN_TYPE );

    if (!knew_coag)
        set_ident_type( OBJ_POTIONS, POT_BLOOD_COAGULATED, ID_KNOWN_TYPE );

    // Now that coagulating is necessary, check inventory for !coagulated blood.
    for (int m = 0; m < ENDOFPACK; m++)
    {
        if (!is_valid_item(you.inv[m]))
            continue;

        if (you.inv[m].base_type == OBJ_POTIONS
            && you.inv[m].sub_type == POT_BLOOD_COAGULATED)
        {
            CrawlHashTable &props2 = you.inv[m].props;
            if (!props2.exists("timer"))
                init_stack_blood_potions(you.inv[m], you.inv[m].special);

            ASSERT(props2.exists("timer"));
            CrawlVector &timer2 = props2["timer"].get_vector();

            blood.quantity -= coag_count + rot_count;
            if (blood.quantity < 1)
            {
                if (you.equip[EQ_WEAPON] == blood.link)
                    you.equip[EQ_WEAPON] = -1;

                destroy_item(blood);
            }
            else
            {
                ASSERT(timer.size() == blood.quantity);
                if (!knew_blood)
                    mpr(blood.name(DESC_INVENTORY).c_str());
            }

            // Update timer -> push(pop).
            long val;
            while (!age_timer.empty())
            {
                val = age_timer[age_timer.size() - 1];
                age_timer.pop_back();
                timer2.push_back(val);
            }

            you.inv[m].quantity += coag_count;
            ASSERT(timer2.size() == you.inv[m].quantity);
            if (!knew_coag)
                mpr(you.inv[m].name(DESC_INVENTORY).c_str());

            // re-sort timer
            _long_sort(timer2);

            return (rot_count > 0);
        }
    }

    // If entire stack has coagulated, simply change subtype.
    if (rot_count + coag_count == blood.quantity)
    {
        ASSERT(timer.empty());
        // Update subtype.
        blood.sub_type = POT_BLOOD_COAGULATED;
        item_colour(blood);

        // Re-fill vector.
        long val;
        while (!age_timer.empty())
        {
            val = age_timer[age_timer.size() - 1];
            age_timer.pop_back();
            timer.push_back(val);
        }
        blood.quantity -= rot_count;
        // Stack still exists because of coag_count.
        ASSERT(timer.size() == blood.quantity);

        if (!knew_coag)
            mpr(blood.name(DESC_INVENTORY).c_str());

        return (rot_count > 0);
    }

    // Else, create new stack in inventory.
    int freeslot = find_free_slot(blood);
    if (freeslot >= 0 && freeslot < ENDOFPACK
        && !is_valid_item(you.inv[freeslot]))
    {
        item_def &item   = you.inv[freeslot];
        item.link        = freeslot;
        item.slot        = index_to_letter(item.link);
        item.base_type   = OBJ_POTIONS;
        item.sub_type    = POT_BLOOD_COAGULATED;
        item.quantity    = coag_count;
        item.x           = -1;
        item.y           = -1;
        item.plus        = 0;
        item.plus2       = 0;
        item.special     = 0;
        item.flags       = 0;
        item.inscription = "";
        item_colour(item);

        CrawlHashTable &props_new = item.props;
        props_new.set_default_flags(SFLAG_CONST_TYPE);
        props_new["timer"].new_vector(SV_LONG);
        CrawlVector &timer_new = props_new["timer"].get_vector();

        long val;
        while (!age_timer.empty())
        {
            val = age_timer[age_timer.size() - 1];
            age_timer.pop_back();
            timer_new.push_back(val);
        }

        ASSERT(timer_new.size() == coag_count);
        props_new.assert_validity();

        blood.quantity -= coag_count + rot_count;
        ASSERT(timer.size() == blood.quantity);

        if (!knew_blood)
            mpr(blood.name(DESC_INVENTORY).c_str());
        if (!knew_coag)
            mpr(item.name(DESC_INVENTORY).c_str());

        return (rot_count > 0);
    }

    // No space in inventory, check floor.
    int o = igrd[you.x_pos][you.y_pos];
    while (o != NON_ITEM)
    {
        if (mitm[o].base_type == OBJ_POTIONS
            && mitm[o].sub_type == POT_BLOOD_COAGULATED)
        {
            // Merge with existing stack.
            CrawlHashTable &props2 = mitm[o].props;
            if (!props2.exists("timer"))
                init_stack_blood_potions(mitm[o], mitm[o].special);

            ASSERT(props2.exists("timer"));
            CrawlVector &timer2 = props2["timer"].get_vector();
            ASSERT(timer2.size() == mitm[o].quantity);

            // Update timer -> push(pop).
            long val;
            while (!age_timer.empty())
            {
                val = age_timer[age_timer.size() - 1];
                age_timer.pop_back();
                timer2.push_back(val);
            }
            _long_sort(timer2);

            inc_mitm_item_quantity(o, coag_count);
            ASSERT(timer2.size() == mitm[o].quantity);
            dec_inv_item_quantity(blood.link, rot_count + coag_count);
            ASSERT(timer.size() == blood.quantity);
            if (!knew_blood)
                mpr(blood.name(DESC_INVENTORY).c_str());

            return (true);
        }
        o = mitm[o].link;
    }
    // If we got here nothing was found!

    // Create a new stack of potions.
    o = get_item_slot();
    if (o == NON_ITEM)
        return (false);

    // These values are common to all: {dlb}
    mitm[o].base_type = OBJ_POTIONS;
    mitm[o].sub_type  = POT_BLOOD_COAGULATED;
    mitm[o].quantity  = coag_count;
    mitm[o].plus      = 0;
    mitm[o].plus2     = 0;
    mitm[o].special   = 0;
    mitm[o].flags     = ~(ISFLAG_THROWN | ISFLAG_DROPPED);
    item_colour(mitm[o]);

    CrawlHashTable &props_new = mitm[o].props;
    props_new.set_default_flags(SFLAG_CONST_TYPE);
    props_new["timer"].new_vector(SV_LONG);
    CrawlVector &timer_new = props_new["timer"].get_vector();

    long val;
    while (!age_timer.empty())
    {
        val = age_timer[age_timer.size() - 1];
        age_timer.pop_back();
        timer_new.push_back(val);
    }

    ASSERT(timer_new.size() == coag_count);
    props_new.assert_validity();
    move_item_to_grid( &o, you.x_pos, you.y_pos );

    blood.quantity -= rot_count + coag_count;
    if (blood.quantity < 1)
    {
        if (you.equip[EQ_WEAPON] == blood.link)
            you.equip[EQ_WEAPON] = -1;

        destroy_item(blood);
    }
    else
    {
        ASSERT(timer.size() == blood.quantity);
        if (!knew_blood)
            mpr(blood.name(DESC_INVENTORY).c_str());
    }
    return (true);
}

// Mostly used for (q)uaff, (f)ire, and Evaporate.
long remove_oldest_blood_potion(item_def &stack)
{
    ASSERT(is_valid_item(stack));
    ASSERT(is_blood_potion(stack));

    CrawlHashTable &props = stack.props;
    if (!props.exists("timer"))
        init_stack_blood_potions(stack, stack.special);
    ASSERT(props.exists("timer"));
    CrawlVector &timer = props["timer"].get_vector();
    ASSERT(!timer.empty());

    // Assuming already sorted, and first (oldest) potion valid.
    const long val = timer[timer.size() - 1].get_long();
    timer.pop_back();

    // The quantity will be decreased elsewhere.
    return val;
}

// Used whenever copies of blood potions have to be cleaned up.
void remove_newest_blood_potion(item_def &stack, int quant)
{
    ASSERT(is_valid_item(stack));
    ASSERT(is_blood_potion(stack));

    CrawlHashTable &props = stack.props;
    if (!props.exists("timer"))
        init_stack_blood_potions(stack, stack.special);
    ASSERT(props.exists("timer"));
    CrawlVector &timer = props["timer"].get_vector();
    ASSERT(!timer.empty());

    if (quant == -1)
        quant = timer.size() - stack.quantity;

    // Overwrite newest potions with oldest ones.
    int repeats = stack.quantity;
    if (repeats > quant)
        repeats = quant;

    for (int i = 0; i < repeats; i++)
    {
        timer[i] = timer[timer.size() - 1];
        timer.pop_back();
    }

    // Now remove remaining oldest potions...
    repeats = quant - repeats;
    for (int i = 0; i < repeats; i++)
        timer.pop_back();

    // ... and re-sort.
    _long_sort(timer);
}

// Called from copy_item_to_grid.
// NOTE: Quantities are set afterwards, so don't ASSERT for those.
void drop_blood_potions_stack(item_def &stack, int quant, int x, int y)
{
    if (!is_valid_item(stack))
        return;

    ASSERT(quant > 0 && quant <= stack.quantity);
    ASSERT(is_blood_potion(stack));

    CrawlHashTable &props = stack.props;
    if (!props.exists("timer"))
        init_stack_blood_potions(stack, stack.special);
    ASSERT(props.exists("timer"));
    CrawlVector &timer = props["timer"].get_vector();
    ASSERT(!timer.empty());

    // First check whether we can merge with an existing stack on the floor.
    int o = igrd[x][y];
    while (o != NON_ITEM)
    {
        if (mitm[o].base_type == OBJ_POTIONS
            && mitm[o].sub_type == stack.sub_type)
        {
            CrawlHashTable &props2 = mitm[o].props;
            if (!props2.exists("timer"))
                init_stack_blood_potions(mitm[o], mitm[o].special);
            ASSERT(props2.exists("timer"));
            CrawlVector &timer2 = props2["timer"].get_vector();

            // Update timer -> push(pop).
            for (int i = 0; i < quant; i++)
            {
                timer2.push_back(timer[timer.size() - 1].get_long());
                timer.pop_back();
            }

            // Re-sort timer.
            _long_sort(timer2);
            return;
        }
        o = mitm[o].link;
    }

    // If we got here nothing was found.
    // Stuff could have been destroyed or offered, either case we'll
    // have to reduce the timer vector anyway.
    while (!timer.empty() && quant-- > 0)
        timer.pop_back();
}

// Called from move_item_to_player.
// Quantities are set afterwards, so don't ASSERT for those.
void pick_up_blood_potions_stack(item_def &stack, int quant)
{
    ASSERT(quant > 0 && quant <= stack.quantity);
    if (!is_valid_item(stack))
        return;

    ASSERT(is_blood_potion(stack));

    CrawlHashTable &props = stack.props;
    if (!props.exists("timer"))
        init_stack_blood_potions(stack, stack.special);
    ASSERT(props.exists("timer"));
    CrawlVector &timer = props["timer"].get_vector();
    ASSERT(!timer.empty());

    // First check whether we can merge with an existing stack in inventory.
    for (int m = 0; m < ENDOFPACK; m++)
    {
        if (!is_valid_item(you.inv[m]))
            continue;

        if (you.inv[m].base_type == OBJ_POTIONS
            && you.inv[m].sub_type == stack.sub_type)
        {
            CrawlHashTable &props2 = you.inv[m].props;
            if (!props2.exists("timer"))
                init_stack_blood_potions(you.inv[m], you.inv[m].special);
            ASSERT(props2.exists("timer"));
            CrawlVector &timer2 = props2["timer"].get_vector();

            // Update timer -> push(pop).
            for (int i = 0; i < quant; i++)
            {
                timer2.push_back(timer[timer.size() - 1].get_long());
                timer.pop_back();
            }

            // Re-sort timer.
            _long_sort(timer2);
            return;
        }
    }
    // If we got here nothing was found. Huh?
}

// Deliberately don't check for rottenness here, so this check
// can also be used to verify whether you *could* have bottled
// a now rotten corpse.
bool can_bottle_blood_from_corpse(int mons_type)
{
    if (you.species != SP_VAMPIRE || you.experience_level < 6
        || !mons_has_blood(mons_type))
    {
        return (false);
    }

    int chunk_type = mons_corpse_effect( mons_type );
    if (chunk_type == CE_CLEAN || chunk_type == CE_CONTAMINATED)
        return (true);

    return (false);
}

// Maybe potions should automatically merge into those already on the floor,
// or the player's inventory.
void turn_corpse_into_blood_potions( item_def &item )
{
    ASSERT( item.base_type == OBJ_CORPSES );
    ASSERT( !food_is_rotten(item) );

    const int mons_class = item.plus;
    ASSERT( can_bottle_blood_from_corpse(mons_class) );

    item.base_type = OBJ_POTIONS;
    item.sub_type  = POT_BLOOD;
    item_colour(item);
    item.flags &= ~(ISFLAG_THROWN | ISFLAG_DROPPED);

    // Max. amount is about one third of the max. amount for chunks.
    const int max_chunks = mons_weight( mons_class ) / 150;
    item.quantity  = 1 + random2( max_chunks/3 );
    item.quantity  = stepdown_value( item.quantity, 2, 2, 6, 6 );

    // Lower number of potions obtained from contaminated chunk type corpses.
    if (mons_corpse_effect( mons_class ) == CE_CONTAMINATED)
    {
        item.quantity /= (random2(3) + 1);

        if (item.quantity < 1)
            item.quantity = 1;
    }

    // Initialize timer depending on corpse age:
    // almost rotting: age = 100 --> potion timer =  500 --> will coagulate soon
    // freshly killed: age = 200 --> potion timer = 2000 --> fresh !blood
    init_stack_blood_potions(item, (item.special - 100) * 15 + 500);

    // Happens after the blood has been bottled.
    if (monster_descriptor(mons_class, MDSC_LEAVES_HIDE) && !one_chance_in(3))
        create_monster_hide(mons_class);
}

// A variation of the mummy curse:
// Instead of trashing the entire stack, split the stack and only turn part
// of it into POT_DECAY.
void split_potions_into_decay( int obj, int amount, bool need_msg )
{
    ASSERT(obj != -1);
    item_def &potion = you.inv[obj];

    ASSERT(is_valid_item(potion));
    ASSERT(potion.base_type == OBJ_POTIONS);
    ASSERT(amount > 0);
    ASSERT(amount <= potion.quantity);

    // Output decay message.
    if (need_msg && get_ident_type(OBJ_POTIONS, POT_DECAY) == ID_KNOWN_TYPE)
        _potion_stack_changed_message(potion, amount, false);

    if (you.equip[EQ_WEAPON] == obj)
        you.wield_change = true;
    you.redraw_quiver = true;

    if (is_blood_potion(potion))
    {
        // We're being nice here, and only decay the *oldest* potions.
        for (int i = 0; i < amount; i++)
            remove_oldest_blood_potion(potion);
    }

    // Try to merge into existing stacks of decayed potions.
    for (int m = 0; m < ENDOFPACK; m++)
    {
        if (you.inv[m].base_type == OBJ_POTIONS
            && you.inv[m].sub_type == POT_DECAY)
        {
            if (potion.quantity == amount)
            {
                if (you.equip[EQ_WEAPON] == obj)
                    you.equip[EQ_WEAPON] = -1;

                destroy_item(potion);
            }
            else
                you.inv[obj].quantity -= amount;

            you.inv[m].quantity += amount;

            return;
        }
    }

    // Else, if entire stack affected just change subtype.
    if (amount == potion.quantity)
    {
        you.inv[obj].sub_type = POT_DECAY;
        unset_ident_flags( you.inv[obj], ISFLAG_IDENT_MASK ); // all different
        return;
    }

    // Else, create new stack in inventory.
    int freeslot = find_free_slot(you.inv[obj]);
    if (freeslot >= 0 && freeslot < ENDOFPACK
        && !is_valid_item(you.inv[freeslot]))
    {
        item_def &item   = you.inv[freeslot];
        item.link        = freeslot;
        item.slot        = index_to_letter(item.link);
        item.base_type   = OBJ_POTIONS;
        item.sub_type    = POT_DECAY;
        item.quantity    = amount;
        item.x           = -1;
        item.y           = -1;
        // Keep description as it was.
        item.plus        = potion.plus;
        item.plus2       = 0;
        item.special     = 0;
        item.flags       = 0;
        item.colour      = potion.colour;
        item.inscription = "";

        you.inv[obj].quantity -= amount;
        return;
    }
    // Okay, inventory is full.

    // Check whether we can merge with an existing stack on the floor.
    int o = igrd[you.x_pos][you.y_pos];
    while (o != NON_ITEM)
    {
        if (mitm[o].base_type == OBJ_POTIONS
            && mitm[o].sub_type == POT_DECAY)
        {
            dec_inv_item_quantity(obj, amount);
            inc_mitm_item_quantity(o, amount);
            return;
        }
        o = mitm[o].link;
    }

    // Only bother creating a distinct stack of potions
    // if it won't get destroyed right away.
    if (!grid_destroys_items(grd[you.x_pos][you.y_pos]))
    {
        item_def potion2;
        potion2.base_type = OBJ_POTIONS;
        potion2.sub_type  = POT_DECAY;
        // Keep description as it was.
        potion2.plus      = potion.plus;
        potion2.quantity  = amount;
        potion2.colour    = potion.colour;
        potion2.plus2     = 0;
        potion2.flags     = 0;
        potion2.special   = 0;

        copy_item_to_grid( potion2, you.x_pos, you.y_pos );
   }
   // Is decreased even if the decay stack goes splat.
   dec_inv_item_quantity(obj, amount);
}

// Checks whether the player or a monster is capable of bleeding.
bool victim_can_bleed(int montype)
{
    if (montype == -1) // player
    {
        if (you.is_undead && (you.species != SP_VAMPIRE
                              || you.hunger_state <= HS_SATIATED))
        {
            return (false);
        }

        int tran = you.attribute[ATTR_TRANSFORMATION];
        if (tran == TRAN_STATUE || tran == TRAN_ICE_BEAST
            || tran == TRAN_AIR || tran == TRAN_LICH
            || tran == TRAN_SPIDER) // Monster spiders don't bleed either.
        {
            return (false);
        }
        return (true);
    }

    // Now check monsters.
    return (mons_has_blood(montype));
}

static bool allow_bleeding_on_square(int x, int y)
{
    // No bleeding onto sanctuary ground, please.
    // Also not necessary if already covered in blood.
    if (env.map[x][y].property != FPROP_NONE)
        return (false);

    // No spattering into lava or water.
    if (grd[x][y] >= DNGN_LAVA && grd[x][y] < DNGN_FLOOR)
        return (false);

    // No spattering into fountains (other than blood).
    if (grd[x][y] == DNGN_FOUNTAIN_BLUE || grd[x][y] == DNGN_FOUNTAIN_SPARKLING)
        return (false);

    // The good gods like to keep their altars pristine.
    if (is_good_god(grid_altar_god(grd[x][y])))
        return (false);

    return (true);
}

static void maybe_bloodify_square(int x, int y, int amount, bool spatter = false)
{
    if (amount < 1)
        return;

    if (!spatter && !allow_bleeding_on_square(x,y))
        return;

    if (x_chance_in_y(amount, 20))
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS,
             "might bleed now; square: (%d, %d); amount = %d",
             x, y, amount);
#endif
        if (allow_bleeding_on_square(x,y))
            env.map[x][y].property = FPROP_BLOODY;

        // If old or new blood on square, the smell reaches further.
        if (env.map[x][y].property == FPROP_BLOODY)
            blood_smell(12, x, y);
        else // Still allow a lingering smell.
            blood_smell(7, x, y);

        if (spatter)
        {
            // Smaller chance of spattering surrounding squares.
            for (int i = -1; i <= 1; i++)
                for (int j = -1; j <= 1; j++)
                {
                    if (i == 0 && j == 0) // current square
                        continue;

                    // Spattering onto walls etc. less likely.
                    if (grd[x+i][y+j] < DNGN_MINMOVE && !one_chance_in(3))
                        continue;

                    maybe_bloodify_square(x+i, y+j, amount/15);
                }
        }
    }
}

// Currently flavour only: colour ground (and possibly adjacent squares) red.
// "damage" depends on damage taken (or hitpoints, if damage higher),
// or, for sacrifices, on the number of chunks possible to get out of a corpse.
void bleed_onto_floor(int x, int y, int montype, int damage, bool spatter)
{
    ASSERT(in_bounds(x,y));
    if (!victim_can_bleed(montype))
        return;

    maybe_bloodify_square(x, y, damage, spatter);
}

static void _spatter_neighbours(int cx, int cy, int chance)
{
    int posx, posy;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
        {
            posx = cx + x;
            posy = cy + y;
            if (!in_bounds(posx, posy))
                continue;

            if (!allow_bleeding_on_square(posx, posy))
                continue;

            if (grd[posx][posy] < DNGN_MINMOVE && !one_chance_in(3))
                continue;

            if (one_chance_in(chance))
            {
                env.map[posx][posy].property = FPROP_BLOODY;
                _spatter_neighbours(posx, posy, chance+1);
            }
        }
}

void generate_random_blood_spatter_on_level()
{
    int cx, cy;
    int startprob;

    // startprob is used to initialize the chance for neighbours being
    // spattered, which will be decreased by 1 per recursion round.
    // We then use one_chance_in(chance) to determine whether to spatter a
    // given grid or not. Thus, startprob = 1 means that initially all
    // surrounding grids will be spattered (3x3), and the _higher_ startprob
    // the _lower_ the overall chance for spattering and the _smaller_ the
    // bloodshed area.

    const int max_cluster = 7 + random2(9);

    // Lower chances for large bloodshed areas if we have many clusters,
    // but increase chances if we have few.
    // Chances for startprob are [1..3] for 7-9 clusters,
    //                       ... [1..4] for 10-12 clusters, and
    //                       ... [2..5] for 13-15 clusters.

    int min_prob = 1;
    int max_prob = 4;
    if (max_cluster < 10)
        max_prob--;
    else if (max_cluster > 12)
        min_prob++;

    for (int i = 0; i < max_cluster; i++)
    {
        cx = 10 + random2(GXM - 10);
        cy = 10 + random2(GYM - 10);
        startprob = min_prob + random2(max_prob);

        if (allow_bleeding_on_square(cx, cy))
            env.map[cx][cy].property = FPROP_BLOODY;
        _spatter_neighbours(cx, cy, startprob);
    }
}

void search_around( bool only_adjacent )
{
    int i;

    // Traps and doors stepdown skill:
    // skill/(2x-1) for squares at distance x
    int max_dist = (you.skills[SK_TRAPS_DOORS] + 1) / 2;
    if ( max_dist > 5 )
        max_dist = 5;
    if ( max_dist > 1 && only_adjacent )
        max_dist = 1;
    if ( max_dist < 1 )
        max_dist = 1;

    for (int srx = you.x_pos - max_dist; srx <= you.x_pos + max_dist; ++srx)
        for (int sry = you.y_pos - max_dist; sry <= you.y_pos + max_dist; ++sry)
        {
            // Must have LOS, with no translucent walls in the way.
            if (see_grid_no_trans(srx, sry))
            {
                // Maybe we want distance() instead of grid_distance()?
                int dist = grid_distance(srx, sry, you.x_pos, you.y_pos);

                // Don't exclude own square; may be levitating.
                // XXX: Currently, levitating over a trap will always detect it.
                if (dist == 0)
                    ++dist;

                // Making this harsher by removing the old +1...
                int effective = you.skills[SK_TRAPS_DOORS] / (2*dist - 1);

                if (grd[srx][sry] == DNGN_SECRET_DOOR
                    && x_chance_in_y(effective + 1, 17))
                {
                    mpr("You found a secret door!");
                    reveal_secret_door(srx, sry);
                    exercise(SK_TRAPS_DOORS, (coinflip() ? 2 : 1));
                }
                else if (grd[srx][sry] == DNGN_UNDISCOVERED_TRAP
                         && x_chance_in_y(effective + 1, 17))
                {
                    i = trap_at_xy(srx, sry);

                    if (i != -1)
                    {
                        grd[srx][sry] = trap_category(env.trap[i].type);
                        mpr("You found a trap!");
                        learned_something_new(TUT_SEEN_TRAP, srx, sry);

                        exercise(SK_TRAPS_DOORS, (coinflip() ? 2 : 1));
                    }
                    else
                    {
                        // Maybe we shouldn't kill the trap for debugging
                        // purposes - oh well.
                        grd[srx][sry] = DNGN_FLOOR;
#if DEBUG_DIAGNOSTICS
                        mpr("You found a buggy trap! It vanishes!",
                            MSGCH_DIAGNOSTICS);
#endif
                    }
                }
            }
        }

    return;
}

void curare_hits_player(int agent, int degree)
{
    const bool res_poison = player_res_poison();

    poison_player(degree);

    if (!player_res_asphyx())
    {
        int hurted = roll_dice(2, 6);
        // Note that the hurtage is halved by poison resistance.
        if (res_poison)
            hurted /= 2;

        if (hurted)
        {
            mpr("You feel difficulty breathing.");
            ouch( hurted, agent, KILLED_BY_CURARE, "curare-induced apnoea" );
        }
        potion_effect(POT_SLOWING, 2 + random2(4 + degree));
    }
}

void merfolk_start_swimming(void)
{
    if (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE)
        untransform();

    std::set<equipment_type> removed;
    removed.insert(EQ_BOOTS);

    // Perhaps a bit to easy for the player, but we allow merfolk
    // to slide out of heavy body armour freely when entering water,
    // rather than handling emcumbered swimming. -- bwr
    if (!player_light_armour())
        removed.insert(EQ_BODY_ARMOUR);

    remove_equipment(removed);
    you.redraw_evasion = true;
}

static void exit_stair_message(dungeon_feature_type stair, bool /* going_up */)
{
    if (grid_is_escape_hatch(stair))
        mpr("The hatch slams shut behind you.");
}

static void climb_message(dungeon_feature_type stair, bool going_up,
                          level_area_type old_level_type)
{
    if (old_level_type != LEVEL_DUNGEON)
        return;

    if (grid_is_portal(stair))
        mpr("The world spins around you as you enter the gateway.");
    else if (grid_is_escape_hatch(stair))
    {
        if (going_up)
            mpr("A mysterious force pulls you upwards.");
        else
        {
            mprf("You %s downwards.", you.flight_mode() == FL_FLY? "fly" :
                                     (player_is_airborne()? "float" : "slide"));
        }
    }
    else
    {
        mprf("You %s %swards.", you.flight_mode() == FL_FLY? "fly" :
                                (player_is_airborne()? "float" : "climb"),
                                going_up? "up": "down");
    }
}

static void leaving_level_now()
{
    // Note the name ahead of time because the events may cause markers
    // to be discarded.
    const std::string newtype =
        env.markers.property_at(you.pos(), MAT_ANY, "dst");

    dungeon_events.fire_position_event(DET_PLAYER_CLIMBS, you.pos());
    dungeon_events.fire_event(DET_LEAVING_LEVEL);

    you.level_type_name = newtype;
}

static void set_entry_cause(entry_cause_type default_cause,
                            level_area_type old_level_type)
{
    ASSERT(default_cause != NUM_ENTRY_CAUSE_TYPES);

    if (old_level_type == you.level_type && you.entry_cause != EC_UNKNOWN)
        return;

    if (crawl_state.is_god_acting())
    {
        if (crawl_state.is_god_retribution())
            you.entry_cause = EC_GOD_RETRIBUTION;
        else
            you.entry_cause = EC_GOD_ACT;

        you.entry_cause_god = crawl_state.which_god_acting();
    }
    else if (default_cause != EC_UNKNOWN)
    {
        you.entry_cause     = default_cause;
        you.entry_cause_god = GOD_NO_GOD;
    }
    else
    {
        you.entry_cause     = EC_SELF_EXPLICIT;
        you.entry_cause_god = GOD_NO_GOD;
    }
}

// // not static anymore
int runes_in_pack()
{
    int num_runes = 0;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item( you.inv[i] )
            && you.inv[i].base_type == OBJ_MISCELLANY
            && you.inv[i].sub_type == MISC_RUNE_OF_ZOT)
        {
            num_runes += you.inv[i].quantity;
        }
    }

    return num_runes;
}

bool check_annotation_exclusion_warning()
{
    coord_def pos(you.x_pos, you.y_pos);
    level_id  next_level_id = level_id::get_next_level_id(pos);

    crawl_state.level_annotation_shown = false;
    bool might_be_dangerous = false;

    if (level_annotation_has("!", next_level_id)
        && next_level_id != level_id::current()
        && next_level_id.level_type == LEVEL_DUNGEON)
    {
        mpr("Warning: level annotation for next level is:", MSGCH_PROMPT);
        mpr(get_level_annotation(next_level_id).c_str(), MSGCH_PLAIN, YELLOW);
        might_be_dangerous = true;
        crawl_state.level_annotation_shown = true;
    }
    else if (is_exclude_root(pos))
    {
        mpr("This staircase is marked as excluded!", MSGCH_WARN);
        might_be_dangerous = true;
    }

    if (might_be_dangerous
        && !yesno("Enter next level anyway?", true, 'n', true, false))
    {
        canned_msg(MSG_OK);
        interrupt_activity( AI_FORCE_INTERRUPT );
        crawl_state.level_annotation_shown = false;
        return (false);
    }
    return (true);
}

void up_stairs(dungeon_feature_type force_stair,
               entry_cause_type entry_cause)
{
    dungeon_feature_type stair_find = (force_stair ? force_stair
                                                   : grd[you.x_pos][you.y_pos]);
    const branch_type     old_where      = you.where_are_you;
    const level_area_type old_level_type = you.level_type;

    if (stair_find == DNGN_ENTER_SHOP)
    {
        shop();
        return;
    }

    // Probably still need this check here (teleportation) -- bwr
    if (grid_stair_direction(stair_find) != CMD_GO_UPSTAIRS)
    {
        if (stair_find == DNGN_STONE_ARCH)
            mpr("There is nothing on the other side of the stone arch.");
        else
            mpr("You can't go up here.");
        return;
    }

    level_id  old_level_id    = level_id::current();
    LevelInfo &old_level_info = travel_cache.get_level_info(old_level_id);

    // Since the overloaded message set turn_is_over, I'm assuming that
    // the overloaded character makes an attempt... so we're doing this
    // check before that one. -- bwr
    if (!player_is_airborne()
        && you.duration[DUR_CONF]
        && old_level_type == LEVEL_DUNGEON
        && stair_find >= DNGN_STONE_STAIRS_UP_I
        && stair_find <= DNGN_STONE_STAIRS_UP_III
        && random2(100) > you.dex)
    {
        mpr("In your confused state, you trip and fall back down the stairs.");

        ouch( roll_dice( 3 + you.burden_state, 5 ), 0,
              KILLED_BY_FALLING_DOWN_STAIRS );

        you.turn_is_over = true;
        return;
    }

    if (you.burden_state == BS_OVERLOADED
        && !grid_is_escape_hatch(stair_find))
    {
        mpr("You are carrying too much to climb upwards.");
        you.turn_is_over = true;
        return;
    }

    if (you.your_level == 0
        && !yesno("Are you sure you want to leave the Dungeon?", false, 'n'))
    {
        mpr("Alright, then stay!");
        return;
    }

    // Checks are done, the character is committed to moving between levels.
    leaving_level_now();

    int old_level  = you.your_level;

    // Interlevel travel data.
    const bool collect_travel_data = can_travel_interlevel();

    if (collect_travel_data)
        old_level_info.update();

    // Make sure we return to our main dungeon level... labyrinth entrances
    // in the abyss or pandemonium are a bit trouble (well the labyrinth does
    // provide a way out of those places, its really not that bad I suppose).
    if (level_type_exits_up(you.level_type))
        you.level_type = LEVEL_DUNGEON;

    you.your_level--;

    int i = 0;

    if (you.your_level < 0)
    {
        mpr("You have escaped!");

        for (i = 0; i < ENDOFPACK; i++)
        {
            if (is_valid_item( you.inv[i] )
                && you.inv[i].base_type == OBJ_ORBS)
            {
                ouch(INSTANT_DEATH, 0, KILLED_BY_WINNING);
            }
        }

        ouch(INSTANT_DEATH, 0, KILLED_BY_LEAVING);
    }

    you.prev_targ  = MHITNOT;
    if (you.pet_target != MHITYOU)
        you.pet_target = MHITNOT;

    you.prev_grd_targ = coord_def(0, 0);

    if (player_in_branch( BRANCH_VESTIBULE_OF_HELL ))
    {
        mpr("Thank you for visiting Hell. Please come again soon.");
        you.where_are_you = BRANCH_MAIN_DUNGEON;
        you.your_level = you.hell_exit;
        stair_find = DNGN_STONE_STAIRS_UP_I;
    }

    if (player_in_hell())
    {
        you.where_are_you = BRANCH_VESTIBULE_OF_HELL;
        you.your_level = 27;
    }

    // Did we take a branch stair?
    for (i = 0; i < NUM_BRANCHES; ++i)
    {
        if (branches[i].exit_stairs == stair_find)
        {
            you.where_are_you = branches[i].parent_branch;

            // If leaving a branch which wasn't generated in this
            // particular game (like the Swamp or Shoals), then
            // its startdepth is set to -1; compensate for that,
            // so we don't end up on "level -1".
            if (branches[i].startdepth == -1)
                you.your_level += 2;
            break;
        }
    }

    const dungeon_feature_type stair_taken = stair_find;

    if (you.flight_mode() == FL_FLY)
        mpr("You fly upwards.");
    else if (you.flight_mode() == FL_LEVITATE)
        mpr("You float upwards... And bob straight up to the ceiling!");
    else
        climb_message(stair_find, true, old_level_type);

    exit_stair_message(stair_find, true);

    if (old_where != you.where_are_you && you.level_type == LEVEL_DUNGEON)
    {
        mprf("Welcome back to %s!",
             branches[you.where_are_you].longname);
    }

    int stair_x = you.x_pos, stair_y = you.y_pos;

#ifdef USE_TILE
    const bool newlevel =
#endif
        load(stair_taken, LOAD_ENTER_LEVEL, old_level_type,
             old_level, old_where);

    set_entry_cause(entry_cause, old_level_type);
    entry_cause = you.entry_cause;

    you.turn_is_over = true;

    save_game_state();

    new_level();

    viewwindow(true, true);

    // Left Zot without enough runes to get back in (probably because
    // of dropping some runes within Zot), but need to get back in Zot
    // to get the Orb?  Xom finds that funny.
    if (stair_find == DNGN_RETURN_FROM_ZOT
        && branches[BRANCH_HALL_OF_ZOT].branch_flags & BFLAG_HAS_ORB)
    {
        int runes_avail = you.attribute[ATTR_UNIQUE_RUNES]
            + you.attribute[ATTR_DEMONIC_RUNES]
            + you.attribute[ATTR_ABYSSAL_RUNES]
            - you.attribute[ATTR_RUNES_IN_ZOT];

        if (runes_avail < NUMBER_OF_RUNES_NEEDED)
            xom_is_stimulated(255, "Xom snickers loudly.", true);
    }

    if (you.skills[SK_TRANSLOCATIONS] > 0 && !allow_control_teleport( true ))
        mpr( "You sense a powerful magical force warping space.", MSGCH_WARN );

    // Tell stash-tracker and travel that we've changed levels.
    trackers_init_new_level(true);

#ifdef USE_TILE
    TileNewLevel(newlevel);
#endif // USE_TILE

    if (collect_travel_data)
    {
        // Update stair information for the stairs we just ascended, and the
        // down stairs we're currently on.
        level_id  new_level_id    = level_id::current();

        if (can_travel_interlevel())
        {
            LevelInfo &new_level_info =
                        travel_cache.get_level_info(new_level_id);
            new_level_info.update();

            // First we update the old level's stair.
            level_pos lp;
            lp.id    = new_level_id;
            lp.pos.x = you.x_pos;
            lp.pos.y = you.y_pos;

            bool guess = false;
            // Ugly hack warning:
            // The stairs in the Vestibule of Hell exhibit special behaviour:
            // they always lead back to the dungeon level that the player
            // entered the Vestibule from. This means that we need to pretend
            // we don't know where the upstairs from the Vestibule go each time
            // we take it. If we don't, interlevel travel may try to use portals
            // to Hell as shortcuts between dungeon levels, which won't work,
            // and will confuse the dickens out of the player (well, it confused
            // the dickens out of me when it happened).
            if (new_level_id == BRANCH_MAIN_DUNGEON
                && old_level_id == BRANCH_VESTIBULE_OF_HELL)
            {
                old_level_info.clear_stairs(DNGN_EXIT_HELL);
            }
            else
            {
                old_level_info.update_stair(stair_x, stair_y, lp, guess);
            }

            // We *guess* that going up a staircase lands us on a downstair,
            // and that we can descend that downstair and get back to where we
            // came from. This assumption is guaranteed false when climbing out
            // of one of the branches of Hell.
            if (new_level_id != BRANCH_VESTIBULE_OF_HELL)
            {
                // Set the new level's stair, assuming arbitrarily that going
                // downstairs will land you on the same upstairs you took to
                // begin with (not necessarily true).
                lp.id = old_level_id;
                lp.pos.x = stair_x;
                lp.pos.y = stair_y;
                new_level_info.update_stair(you.x_pos, you.y_pos, lp, true);
            }
        }
    }
    request_autopickup();
}                               // end up_stairs()

void down_stairs( int old_level, dungeon_feature_type force_stair,
                  entry_cause_type entry_cause )
{
    int i;
    const level_area_type  old_level_type = you.level_type;
    const dungeon_feature_type stair_find =
        force_stair? force_stair : grd(you.pos());

    branch_type old_where = you.where_are_you;

    bool shaft = (!force_stair
                      && trap_type_at_xy(you.x_pos, you.y_pos) == TRAP_SHAFT
                  || force_stair == DNGN_TRAP_NATURAL);
    level_id shaft_dest;
    int      shaft_level = -1;

    // Probably still need this check here (teleportation) -- bwr
    if (grid_stair_direction(stair_find) != CMD_GO_DOWNSTAIRS && !shaft)
    {
        if (stair_find == DNGN_STONE_ARCH)
            mpr("There is nothing on the other side of the stone arch.");
        else
            mpr( "You can't go down here!" );
        return;
    }

    if (stair_find >= DNGN_ENTER_LABYRINTH
        && stair_find <= DNGN_ESCAPE_HATCH_DOWN
        && player_in_branch( BRANCH_VESTIBULE_OF_HELL ))
    {
        // Down stairs in vestibule are one-way!
        mpr("A mysterious force prevents you from descending the staircase.");
        return;
    }

    if (stair_find == DNGN_STONE_ARCH)
    {
        mpr("There is nothing on the other side of the stone arch.");
        return;
    }

    if (!force_stair && you.flight_mode() == FL_LEVITATE)
    {
        mpr("You're floating high up above the floor!");
        return;
    }

    if (shaft)
    {
        const bool known_trap = (grd(you.pos()) != DNGN_UNDISCOVERED_TRAP
                                 && !force_stair);

        if (!known_trap && !force_stair)
        {
            mpr("You can't go down here!");
            return;
        }

        if (you.flight_mode() == FL_LEVITATE && !force_stair)
        {
            if (known_trap)
                mpr("You can't fall through a shaft while levitating.");
            return;
        }

        if (!is_valid_shaft_level())
        {
            if (known_trap)
                mpr("Strange, the shaft doesn't seem to lead anywhere.");
            return;
        }

        shaft_dest = you.shaft_dest();
        if (shaft_dest == level_id::current())
        {
            if (known_trap)
                mpr("Strange, the shaft doesn't seem to lead anywhere.");
            return;
        }
        shaft_level = absdungeon_depth(shaft_dest.branch,
                                       shaft_dest.depth);

        if (you.flight_mode() != FL_FLY || force_stair)
            mpr("You fall through a shaft!");
    }

    // All checks are done, the player is on the move now.

    // Fire level-leaving trigger.
    leaving_level_now();

#ifdef DGL_MILESTONES
    if (!force_stair)
    {
        // Not entirely accurate - the player could die before
        // reaching the Abyss.
        if (grd[you.x_pos][you.y_pos] == DNGN_ENTER_ABYSS)
            mark_milestone("abyss.enter", "entered the Abyss!");
        else if (grd[you.x_pos][you.y_pos] == DNGN_EXIT_ABYSS
                 && you.char_direction != GDT_GAME_START)
        {
            mark_milestone("abyss.exit", "escaped from the Abyss!");
        }
    }
#endif

    // [ds] Descending into the Labyrinth increments your_level. Going
    // downstairs from a labyrinth implies that you've been banished (or been
    // sent to Pandemonium somehow). Decrementing your_level here is needed
    // to fix this buggy sequence: D:n -> Labyrinth -> Abyss -> D:(n+1).
    if (level_type_exits_up(you.level_type))
        you.your_level--;

    if (stair_find == DNGN_ENTER_ZOT)
    {
        const int num_runes = runes_in_pack();

        if (num_runes < NUMBER_OF_RUNES_NEEDED)
        {
            switch (NUMBER_OF_RUNES_NEEDED)
            {
            case 1:
                mpr("You need one more Rune to enter this place.");
                break;

            default:
                mprf( "You need at least %d Runes to enter this place.",
                      NUMBER_OF_RUNES_NEEDED );
            }
            return;
        }
    }

    // Interlevel travel data.
    bool collect_travel_data = can_travel_interlevel();

    level_id  old_level_id    = level_id::current();
    LevelInfo &old_level_info = travel_cache.get_level_info(old_level_id);
    int stair_x = you.x_pos, stair_y = you.y_pos;
    if (collect_travel_data)
        old_level_info.update();

    // Preserve abyss uniques now, since this Abyss level will be deleted.
    if (you.level_type == LEVEL_ABYSS)
        save_abyss_uniques();

    if (you.level_type != LEVEL_DUNGEON
        && (you.level_type != LEVEL_PANDEMONIUM
            || stair_find != DNGN_TRANSIT_PANDEMONIUM))
    {
        you.level_type = LEVEL_DUNGEON;
    }

    you.prev_targ  = MHITNOT;
    if (you.pet_target != MHITYOU)
        you.pet_target = MHITNOT;

    you.prev_grd_targ = coord_def(0, 0);

    if (stair_find == DNGN_ENTER_HELL)
    {
        you.where_are_you = BRANCH_VESTIBULE_OF_HELL;
        you.hell_exit = you.your_level;

        you.your_level = 26;
    }

    // Welcome message.
    // Try to find a branch stair.
    bool entered_branch = false;
    for (i = 0; i < NUM_BRANCHES; ++i)
    {
        if (branches[i].entry_stairs == stair_find)
        {
            entered_branch = true;
            you.where_are_you = branches[i].id;
            break;
        }
    }

    if (stair_find == DNGN_ENTER_LABYRINTH)
        dungeon_terrain_changed(you.pos(), DNGN_FLOOR);

    if (stair_find == DNGN_ENTER_LABYRINTH)
        you.level_type = LEVEL_LABYRINTH;
    else if (stair_find == DNGN_ENTER_ABYSS)
        you.level_type = LEVEL_ABYSS;
    else if (stair_find == DNGN_ENTER_PANDEMONIUM)
        you.level_type = LEVEL_PANDEMONIUM;
    else if (stair_find == DNGN_ENTER_PORTAL_VAULT)
        you.level_type = LEVEL_PORTAL_VAULT;

    // When going downstairs into a special level, delete any previous
    // instances of it.
    if (you.level_type != LEVEL_DUNGEON)
    {
        std::string lname = make_filename(you.your_name, you.your_level,
                                          you.where_are_you,
                                          you.level_type, false);
#if DEBUG_DIAGNOSTICS
        mprf( MSGCH_DIAGNOSTICS, "Deleting: %s", lname.c_str() );
#endif
        unlink(lname.c_str());
    }

    if (stair_find == DNGN_EXIT_ABYSS || stair_find == DNGN_EXIT_PANDEMONIUM)
    {
        mpr("You pass through the gate.");
        if (!you.wizard || !crawl_state.is_replaying_keys())
            more();
    }

    if (old_level_type != you.level_type && you.level_type == LEVEL_DUNGEON)
        mprf("Welcome back to %s!", branches[you.where_are_you].longname);

    if (!player_is_airborne()
        && you.duration[DUR_CONF]
        && !grid_is_escape_hatch(stair_find)
        && force_stair != DNGN_ENTER_ABYSS
        && random2(100) > you.dex)
    {
        std::string fall_where = "down the stairs";

        if (stair_find == DNGN_ENTER_ABYSS
            || stair_find == DNGN_ENTER_PANDEMONIUM
            || stair_find == DNGN_TRANSIT_PANDEMONIUM
            || stair_find == DNGN_ENTER_PORTAL_VAULT)
        {
            fall_where = "through the gate";
        }

        mprf("In your confused state, you trip and fall %s.",
             fall_where.c_str());

        // Nastier than when climbing stairs, but you'll aways get to
        // your destination. -- bwr
        ouch( roll_dice( 6 + you.burden_state, 10 ), 0,
              KILLED_BY_FALLING_DOWN_STAIRS );
    }

    if (shaft)
    {
        you.your_level    = shaft_level;
        you.where_are_you = shaft_dest.branch;
    }
    else if (you.level_type == LEVEL_DUNGEON)
        you.your_level++;

    dungeon_feature_type stair_taken = stair_find;

    if (you.level_type == LEVEL_ABYSS)
        stair_taken = DNGN_FLOOR;

    if (you.level_type == LEVEL_PANDEMONIUM)
        stair_taken = DNGN_TRANSIT_PANDEMONIUM;

    if (shaft)
        stair_taken = DNGN_ESCAPE_HATCH_DOWN;

    switch (you.level_type)
    {
    case LEVEL_LABYRINTH:
        if (you.species == SP_MINOTAUR)
            mpr("You feel right at home here.");
        else
            mpr("You enter a dark and forbidding labyrinth.");
        break;

    case LEVEL_ABYSS:
        if (!force_stair)
            mpr("You enter the Abyss!");

        mpr("To return, you must find a gate leading back.");
        learned_something_new(TUT_ABYSS);
        break;

    case LEVEL_PANDEMONIUM:
        if (old_level_type == LEVEL_PANDEMONIUM)
            mpr("You pass into a different region of Pandemonium.");
        else
        {
            mpr("You enter the halls of Pandemonium!");
            mpr("To return, you must find a gate leading back.");
        }
        break;

    default:
        if (shaft)
        {
            if (you.flight_mode() == FL_FLY && !force_stair)
                mpr("You dive down through the shaft.");
            handle_items_on_shaft(you.x_pos, you.y_pos, false);
        }
        else
            climb_message(stair_find, false, old_level_type);
        break;
    }

    if (!shaft)
        exit_stair_message(stair_find, false);

    if (entered_branch)
    {
        if (branches[you.where_are_you].entry_message)
            mpr(branches[you.where_are_you].entry_message);
        else
            mprf("Welcome to %s!", branches[you.where_are_you].longname);
    }

    if (stair_find == DNGN_ENTER_HELL)
    {
        mpr("Welcome to Hell!");
        mpr("Please enjoy your stay.");

        // Kill -more- prompt if we're traveling.
        if (!you.running && !force_stair)
            more();
    }

    const bool newlevel = load(stair_taken, LOAD_ENTER_LEVEL, old_level_type,
                               old_level, old_where);

    set_entry_cause(entry_cause, old_level_type);
    entry_cause = you.entry_cause;

    if (newlevel)
    {
        // When entering a new level, reset friendly_pickup to default.
        if (god_gives_permanent_followers(you.religion))
            you.friendly_pickup = Options.default_friendly_pickup;

        switch (you.level_type)
        {
        case LEVEL_DUNGEON:
            xom_is_stimulated(49);
            break;

        case LEVEL_PORTAL_VAULT:
            // Portal vaults aren't as interesting.
            xom_is_stimulated(25);
            break;

        case LEVEL_LABYRINTH:
            // Finding the way out of a labyrinth interests Xom,
            // but less so for Minotaurs.
            if (you.species == SP_MINOTAUR)
                xom_is_stimulated(49);
            else
                xom_is_stimulated(98);
            break;

        case LEVEL_ABYSS:
        case LEVEL_PANDEMONIUM:
        {
            // Paranoia
            if (old_level_type == you.level_type)
                break;

            PlaceInfo &place_info = you.get_place_info();
            generate_random_blood_spatter_on_level();

            // Entering voluntarily only stimulates Xom if you've never
            // been there before
            if ((place_info.num_visits == 1 && place_info.levels_seen == 1)
                || entry_cause != EC_SELF_EXPLICIT)
            {
                if (crawl_state.is_god_acting())
                    xom_is_stimulated(255);
                else if (entry_cause == EC_SELF_EXPLICIT)
                {
                    // Entering Pandemonium or the Abyss for the first
                    // time *voluntarily* stimulates Xom much more than
                    // entering a normal dungeon level for the first time.
                    xom_is_stimulated(128, XM_INTRIGUED);
                }
                else if (entry_cause == EC_SELF_RISKY)
                    xom_is_stimulated(128);
                else
                    xom_is_stimulated(255);
            }

            break;
        }

        default:
            ASSERT(false);
        }
    }

    unsigned char pc = 0;
    unsigned char pt = random2avg(28, 3);

    if (shaft)
    {
        you.your_level    = shaft_level;
        you.where_are_you = shaft_dest.branch;
    }
    else if (level_type_exits_up(you.level_type))
        you.your_level++;
    else if (level_type_exits_down(you.level_type)
             && !level_type_exits_down(old_level_type))
    {
        you.your_level--;
    }


    switch (you.level_type)
    {
    case LEVEL_ABYSS:
        grd[you.x_pos][you.y_pos] = DNGN_FLOOR;

        init_pandemonium();     // colours only

        if (player_in_hell())
        {
            you.where_are_you = BRANCH_MAIN_DUNGEON;
            you.your_level    = you.hell_exit - 1;
        }
        break;

    case LEVEL_PANDEMONIUM:
        if (old_level_type == LEVEL_PANDEMONIUM)
        {
            init_pandemonium();

            for (pc = 0; pc < pt; pc++)
                pandemonium_mons();
        }
        else
        {
            init_pandemonium();

            for (pc = 0; pc < pt; pc++)
                pandemonium_mons();

            if (player_in_hell())
            {
                you.where_are_you = BRANCH_MAIN_DUNGEON;
                you.hell_exit  = 26;
                you.your_level = 26;
            }
        }
        break;

    default:
        break;
    }

    you.turn_is_over = true;

    save_game_state();

    new_level();

    // Clear list of beholding monsters.
    if (you.duration[DUR_BEHELD])
    {
        you.beheld_by.clear();
        you.duration[DUR_BEHELD] = 0;
    }

    if (you.skills[SK_TRANSLOCATIONS] > 0 && !allow_control_teleport( true ))
        mpr( "You sense a powerful magical force warping space.", MSGCH_WARN );

    trackers_init_new_level(true);

#ifdef USE_TILE
    TileNewLevel(newlevel);
#endif // USE_TILE

    viewwindow(true, true);

    if (collect_travel_data)
    {
        // Update stair information for the stairs we just descended, and the
        // upstairs we're currently on.
        level_id  new_level_id    = level_id::current();

        if (can_travel_interlevel())
        {
            LevelInfo &new_level_info =
                            travel_cache.get_level_info(new_level_id);
            new_level_info.update();

            // First we update the old level's stair.
            level_pos lp;
            lp.id    = new_level_id;
            lp.pos.x = you.x_pos;
            lp.pos.y = you.y_pos;

            old_level_info.update_stair(stair_x, stair_y, lp);

            // Then the new level's stair, assuming arbitrarily that going
            // upstairs will land you on the same downstairs you took to begin
            // with (not necessarily true).
            lp.id = old_level_id;
            lp.pos.x = stair_x;
            lp.pos.y = stair_y;
            new_level_info.update_stair(you.x_pos, you.y_pos, lp, true);
        }
    }
    request_autopickup();
}                               // end down_stairs()

void trackers_init_new_level(bool transit)
{
    travel_init_new_level();
    if (transit)
        stash_init_new_level();
}

void new_level(void)
{
    take_note(Note(NOTE_DUNGEON_LEVEL_CHANGE));
    print_stats_level();
#ifdef DGL_WHEREIS
    whereis_record();
#endif
}

std::string weird_glowing_colour()
{
    return getMiscString("glowing_colour_name");
}

std::string weird_writing()
{
    return getMiscString("writing_name");
}

std::string weird_smell()
{
    return getMiscString("smell_name");
}

std::string weird_sound()
{
    return getMiscString("sound_name");
}

bool scramble(void)
{
    // Statues are too stiff and heavy to scramble out of the water.
    if (you.attribute[ATTR_TRANSFORMATION] == TRAN_STATUE)
        return (false);

    int max_carry = carrying_capacity();

    // When highly encumbered, scrambling out is hard to do.
    if ((max_carry / 2) + random2(max_carry / 2) <= you.burden)
        return (false);
    else
        return (true);
}

bool go_berserk(bool intentional)
{
    if (!you.can_go_berserk(intentional))
        return (false);

    if (Options.tutorial_left)
        Options.tut_berserk_counter++;

    mpr("A red film seems to cover your vision as you go berserk!");
    mpr("You feel yourself moving faster!");
    mpr("You feel mighty!");

    you.duration[DUR_BERSERKER] += 20 + random2avg(19, 2);

    calc_hp();
    you.hp *= 15;
    you.hp /= 10;

    deflate_hp(you.hp_max, false);

    if (!you.duration[DUR_MIGHT])
        modify_stat( STAT_STRENGTH, 5, true, "going berserk" );

    you.duration[DUR_MIGHT] += you.duration[DUR_BERSERKER];
    haste_player( you.duration[DUR_BERSERKER] );

    if (you.berserk_penalty != NO_BERSERK_PENALTY)
        you.berserk_penalty = 0;

    return (true);
}

bool mons_is_safe(const struct monsters *mon, bool want_move)
{
    int  dist    = grid_distance(you.x_pos, you.y_pos, mon->x, mon->y);

    bool is_safe = (mons_wont_attack(mon)
                    || mons_class_flag(mon->type, M_NO_EXP_GAIN)
#ifdef WIZARD
                    // Wizmode skill setting enforces hiddenness.
                    || you.skills[SK_STEALTH] > 27 && dist > 2
#endif
                       // Only seen through glass walls?
                    || !see_grid_no_trans(mon->x, mon->y)
                       && !mons_has_ranged_spell(mon)
                       && !mons_has_los_ability(mon->type));

#ifdef CLUA_BINDINGS
    bool moving = (!you.delay_queue.empty()
                       && is_run_delay(you.delay_queue.front().type)
                       && you.delay_queue.front().type != DELAY_REST
                   || you.running < RMODE_NOT_RUNNING
                   || want_move);

    bool result = is_safe;

    if (clua.callfn("ch_mon_is_safe", "Mbbd>b",
                    mon, is_safe, moving, dist,
                    &result))
    {
        is_safe = result;
    }
#endif

    return (is_safe);
}

// Return all monsters in range (default: LOS) that the player is able to see
// and recognize as being a monster.
//
// want_move       (??) Somehow affects what monsters are considered dangerous
// just_check      Return zero or one monsters only
// dangerous_only  Return only "dangerous" monsters
// range           search radius (defaults: LOS)
//
void get_playervisible_monsters(std::vector<monsters*> &mons,
                                bool want_move,
                                bool just_check,
                                bool dangerous_only,
                                int range)
{
    if (range == -1)
        range = LOS_RADIUS;

    // Sweep every square within range.
    for ( radius_iterator ri(you.pos(), range); ri; ++ri )
    {
        const unsigned short targ_monst = env.mgrid(*ri);
        if (targ_monst != NON_MONSTER)
        {
            if (see_grid(*ri))
            {
                monsters *mon = &env.mons[targ_monst];
                if (mon->alive()
                    && player_monster_visible(mon)
                    && !mons_is_submerged(mon)
                    && (!mons_is_mimic(mon->type) || mons_is_known_mimic(mon))
                    && (!dangerous_only || !mons_is_safe(mon, want_move)))
                {
                    mons.push_back(mon);
                    if (just_check) // stop once you find one
                        return;
                }
            }
        }
    }
}

bool i_feel_safe(bool announce, bool want_move, bool just_monsters, int range)
{
    if (!just_monsters)
    {
        // check clouds
        if (in_bounds(you.pos()) && env.cgrid(you.pos()) != EMPTY_CLOUD)
        {
            const cloud_type type = env.cloud[env.cgrid(you.pos())].type;

            if (is_damaging_cloud(type, false))
            {
                if (announce)
                {
                    mprf(MSGCH_WARN, "You're standing in a cloud of %s!",
                         cloud_name(type).c_str());
                }
                return (false);
            }
        }

        // No monster will attack you inside a sanctuary,
        // so presence of monsters won't matter -- until it starts shrinking...
        if (is_sanctuary(you.x_pos, you.y_pos) && env.sanctuary_time >= 5)
            return (true);
    }

    // Monster check.
    std::vector<monsters*> visible;
    get_playervisible_monsters(visible, want_move, !announce, true, range);

    // No monsters found.
    if (visible.empty())
        return (true);

    // Announce the presence of monsters (Eidolos).
    if (visible.size() == 1)
    {
        const monsters &m = *visible[0];
        if (announce)
        {
            std::string monname =
                (mons_is_mimic(m.type)) ? "a mimic" : m.name(DESC_NOCAP_A);

            mprf(MSGCH_WARN, "Not with %s in view!", monname.c_str());
        }
        else
            tutorial_first_monster(m);
    }
    else if (announce && visible.size() > 1)
    {
        mprf(MSGCH_WARN, "Not with these monsters around!");
    }

    return (false);
}

bool there_are_monsters_nearby()
{
    // Monster check.
    std::vector<monsters*> visible;
    get_playervisible_monsters(visible, false, true);

    return (!visible.empty());
}

static const char *shop_types[] = {
    "weapon",
    "armour",
    "antique weapon",
    "antique armour",
    "antiques",
    "jewellery",
    "wand",
    "book",
    "food",
    "distillery",
    "scroll",
    "general"
};

int str_to_shoptype(const std::string &s)
{
    if (s == "random" || s == "any")
        return (SHOP_RANDOM);

    for (unsigned i = 0; i < sizeof(shop_types) / sizeof (*shop_types); ++i)
    {
        if (s == shop_types[i])
            return (i);
    }
    return (-1);
}

// General threat = sum_of_logexpervalues_of_nearby_unfriendly_monsters.
// Highest threat = highest_logexpervalue_of_nearby_unfriendly_monsters.
static void monster_threat_values(double *general, double *highest,
                                  bool *invis)
{
    *invis = false;

    double sum = 0;
    int highest_xp = -1;

    monsters *monster = NULL;
    for (int it = 0; it < MAX_MONSTERS; it++)
    {
        monster = &menv[it];

        if (monster->alive() && mons_near(monster) && !mons_friendly(monster))
        {
            const int xp = exper_value(monster);
            const double log_xp = log((double)xp);
            sum += log_xp;
            if (xp > highest_xp)
            {
                highest_xp = xp;
                *highest   = log_xp;
            }
            if (!you.can_see(monster))
                *invis = true;
        }
    }

    *general = sum;
}

bool player_in_a_dangerous_place(bool *invis)
{
    bool junk;
    if (invis == NULL)
        invis = &junk;

    const double logexp = log((double)you.experience);
    double gen_threat = 0.0, hi_threat = 0.0;
    monster_threat_values(&gen_threat, &hi_threat, invis);

    return (gen_threat > logexp * 1.3 || hi_threat > logexp / 2);
}

////////////////////////////////////////////////////////////////////////////
// Living breathing dungeon stuff.
//

static std::vector<coord_def> sfx_seeds;

void setup_environment_effects()
{
    sfx_seeds.clear();

    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
    {
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        {
            if (!in_bounds(x, y))
                continue;

            const int grid = grd[x][y];
            if (grid == DNGN_LAVA
                    || (grid == DNGN_SHALLOW_WATER
                        && you.where_are_you == BRANCH_SWAMP))
            {
                const coord_def c(x, y);
                sfx_seeds.push_back(c);
            }
        }
    }
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "%u environment effect seeds", sfx_seeds.size());
#endif
}

static void apply_environment_effect(const coord_def &c)
{
    const dungeon_feature_type grid = grd(c);
    if (grid == DNGN_LAVA)
        check_place_cloud( CLOUD_BLACK_SMOKE,
                           c.x, c.y, random_range( 4, 8 ), KC_OTHER );
    else if (grid == DNGN_SHALLOW_WATER)
        check_place_cloud( CLOUD_MIST,
                           c.x, c.y, random_range( 2, 5 ), KC_OTHER );
}

static const int Base_Sfx_Chance = 5;
void run_environment_effects()
{
    if (!you.time_taken)
        return;

    dungeon_events.fire_event(DET_TURN_ELAPSED);

    // Each square in sfx_seeds has this chance of doing something special
    // per turn.
    const int sfx_chance = Base_Sfx_Chance * you.time_taken / 10;
    const int nseeds = sfx_seeds.size();

    // If there are a large number of seeds, speed things up by fudging the
    // numbers.
    if (nseeds > 50)
    {
        int nsels = div_rand_round( sfx_seeds.size() * sfx_chance, 100 );
        if (one_chance_in(5))
            nsels += random2(nsels * 3);

        for (int i = 0; i < nsels; ++i)
            apply_environment_effect( sfx_seeds[ random2(nseeds) ] );
    }
    else
    {
        for (int i = 0; i < nseeds; ++i)
        {
            if (random2(100) >= sfx_chance)
                continue;

            apply_environment_effect( sfx_seeds[i] );
        }
    }

    run_corruption_effects(you.time_taken);
}

coord_def pick_adjacent_free_square(const coord_def& p)
{
    int num_ok = 0;
    coord_def result(-1, -1);
    for ( int ux = p.x-1; ux <= p.x+1; ++ux )
    {
        for ( int uy = p.y-1; uy <= p.y+1; ++uy )
        {
            if ( ux == p.x && uy == p.y )
                continue;

            if ( in_bounds(ux, uy)
                 && grd[ux][uy] == DNGN_FLOOR
                 && mgrd[ux][uy] == NON_MONSTER )
            {
                ++num_ok;
                if ( one_chance_in(num_ok) )
                    result.set(ux, uy);
            }
        }
    }
    return result;
}

// Converts a movement speed to a duration. i.e., answers the
// question: if the monster is so fast, how much time has it spent in
// its last movement?
//
// If speed is 10 (normal),    one movement is a duration of 10.
// If speed is 1  (very slow), each movement is a duration of 100.
// If speed is 15 (50% faster than normal), each movement is a duration of
// 6.6667.
int speed_to_duration(int speed)
{
    if (speed < 1)
        speed = 10;
    else if (speed > 100)
        speed = 100;

    return div_rand_round(100, speed);
}

void reveal_secret_door(int x, int y)
{
    ASSERT(grd[x][y] == DNGN_SECRET_DOOR);

    dungeon_feature_type door = grid_secret_door_appearance(x, y);
    grd[x][y] = grid_is_opaque(door) ?
        DNGN_CLOSED_DOOR : DNGN_OPEN_DOOR;
    viewwindow(true, false);
    learned_something_new(TUT_SEEN_SECRET_DOOR, x, y);
}

// A feeble attempt at Nethack-like completeness for cute messages.
std::string your_hand(bool plural)
{
    std::string result;

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    default:
        mpr("ERROR: unknown transformation in your_hand() (spells4.cc)",
            MSGCH_ERROR);
    case TRAN_NONE:
    case TRAN_STATUE:
    case TRAN_LICH:
        result = (you.has_usable_claws()) ? "claw" : "hand";
        break;
    case TRAN_ICE_BEAST:
        result = "hand";
        break;
    case TRAN_SPIDER:
        result = "front leg";
        break;
    case TRAN_SERPENT_OF_HELL:
    case TRAN_DRAGON:
    case TRAN_BAT:
        result = "foreclaw";
        break;
    case TRAN_BLADE_HANDS:
        result = "scythe-like blade";
        break;
    case TRAN_AIR:
        result = "misty tendril";
        break;
    }

    if (plural)
        result += 's';

    return result;
}

bool stop_attack_prompt(const monsters *mon, bool beam_attack,
                        bool beam_target)
{
    if (you.confused())
        return (false);

    bool retval = false;
    bool prompt = false;

    const bool inSanctuary   = (is_sanctuary(you.x_pos, you.y_pos)
                                 || is_sanctuary(mon->x, mon->y));
    const bool wontAttack    = mons_wont_attack(mon);
    const bool isFriendly    = mons_friendly(mon);
    const bool isNeutral     = mons_neutral(mon);
    const bool isUnchivalric = is_unchivalric_attack(&you, mon, mon);
    const bool isHoly        = mons_is_holy(mon);

    if (isFriendly)
    {
        // Listed in the form: "your rat", "Blork the orc".
        snprintf(info, INFO_SIZE, "Really %s %s%s?",
                 (beam_attack) ? (beam_target) ? "fire at"
                                               : "fire through"
                               : "attack",
                 mon->name(DESC_NOCAP_THE).c_str(),
                 (inSanctuary) ? ", despite your sanctuary"
                               : "");
        prompt = true;
    }
    else if (inSanctuary || wontAttack
             || (isNeutral || isHoly) && is_good_god(you.religion)
             || isUnchivalric
                && you.religion == GOD_SHINING_ONE
                && !tso_unchivalric_attack_safe_monster(mon))
    {
        // "Really fire through the helpless neutral holy Daeva?"
        // was: "Really fire through this helpless neutral holy creature?"
        snprintf(info, INFO_SIZE, "Really %s the %s%s%s%s%s?",
                 (beam_attack) ? (beam_target) ? "fire at"
                                               : "fire through"
                               : "attack",
                 (isUnchivalric) ? "helpless "
                                 : "",
                 (isFriendly)    ? "friendly " :
                 (wontAttack)    ? "non-hostile " :
                 (isNeutral)     ? "neutral "
                                 : "",
                 (isHoly)        ? "holy "
                                 : "",
                 mon->name(DESC_PLAIN).c_str(),
                 (inSanctuary)   ? ", despite your sanctuary"
                                 : "");

        prompt = true;
    }

    if (prompt)
        retval = !yesno(info, false, 'n');

    return (retval);
}
