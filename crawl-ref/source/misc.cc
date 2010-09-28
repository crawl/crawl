/*
 *  File:       misc.cc
 *  Summary:    Misc functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "misc.h"
#include "notes.h"

#include <string.h>
#include <algorithm>

#if !defined(__IBMCPP__) && !defined(TARGET_COMPILER_VC)
#include <unistd.h>
#endif

#ifdef TARGET_COMPILER_MINGW
#include <io.h>
#endif

#include <cstdlib>
#include <cstdio>
#include <cmath>

#include "externs.h"
#include "options.h"
#include "misc.h"

#include "abyss.h"
#include "areas.h"
#include "clua.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgn-shoals.h"
#include "directn.h"
#include "dgnevent.h"
#include "directn.h"
#include "env.h"
#include "exercise.h"
#include "fight.h"
#include "files.h"
#include "flood_find.h"
#include "fprop.h"
#include "food.h"
#include "ghost.h"
#include "godabil.h"
#include "hiscores.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "kills.h"
#include "libutil.h"
#include "macro.h"
#include "makeitem.h"
#include "mapmark.h"
#include "message.h"
#include "mon-place.h"
#include "mon-pathfind.h"
#include "mon-info.h"
#include "mon-iter.h"
#include "mon-util.h"
#include "mon-stuff.h"
#include "ouch.h"
#include "player.h"
#include "player-stats.h"
#include "random.h"
#include "religion.h"
#include "godconduct.h"
#include "shopping.h"
#include "skills.h"
#include "skills2.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "hints.h"
#include "view.h"
#include "viewgeom.h"
#include "shout.h"
#include "xom.h"

static void _create_monster_hide(const item_def corpse)
{
    // kiku_receive_corpses() creates corpses that are easily scummed
    // for hides.  We prevent this by setting "DoNotDropHide" as an item
    // property of corpses it creates.
    if (corpse.props.exists("DoNotDropHide"))
        return;

    int mons_class = corpse.plus;

    int o = get_item_slot();
    if (o == NON_ITEM)
        return;

    item_def& item = mitm[o];

    // These values are common to all: {dlb}
    item.base_type = OBJ_ARMOUR;
    item.quantity  = 1;
    item.plus      = 0;
    item.plus2     = 0;
    item.special   = 0;
    item.flags     = 0;
    item.colour    = mons_class_colour(mons_class);

    // These values cannot be set by a reasonable formula: {dlb}
    switch (mons_class)
    {
    case MONS_DRAGON:         item.sub_type = ARM_DRAGON_HIDE;         break;
    case MONS_TROLL:          item.sub_type = ARM_TROLL_HIDE;          break;
    case MONS_ICE_DRAGON:     item.sub_type = ARM_ICE_DRAGON_HIDE;     break;
    case MONS_STEAM_DRAGON:   item.sub_type = ARM_STEAM_DRAGON_HIDE;   break;
    case MONS_MOTTLED_DRAGON: item.sub_type = ARM_MOTTLED_DRAGON_HIDE; break;
    case MONS_STORM_DRAGON:   item.sub_type = ARM_STORM_DRAGON_HIDE;   break;
    case MONS_GOLDEN_DRAGON:  item.sub_type = ARM_GOLD_DRAGON_HIDE;    break;
    case MONS_SWAMP_DRAGON:   item.sub_type = ARM_SWAMP_DRAGON_HIDE;   break;

    case MONS_SHEEP:
    case MONS_YAK:
    default:
        item.sub_type = ARM_ANIMAL_SKIN;
        break;
    }

    monster_type montype = static_cast<monster_type>(corpse.orig_monnum - 1);
    if (!invalid_monster_type(montype) && mons_is_unique(montype))
        item.inscription = mons_type_name(montype, DESC_PLAIN);

    move_item_to_grid(&o, you.pos());
}

int get_max_corpse_chunks(int mons_class)
{
    return (mons_weight(mons_class) / 150);
}

void turn_corpse_into_skeleton(item_def &item)
{
    ASSERT(item.base_type == OBJ_CORPSES && item.sub_type == CORPSE_BODY);

    // Some monsters' corpses lack the structure to leave skeletons
    // behind.
    if (!mons_skeleton(item.plus))
        return;

    item.sub_type = CORPSE_SKELETON;
    item.special  = FRESHEST_CORPSE; // reset rotting counter
    item.colour   = LIGHTGREY;
}

void turn_corpse_into_chunks(item_def &item, bool bloodspatter,
                             bool make_hide)
{
    ASSERT(item.base_type == OBJ_CORPSES && item.sub_type == CORPSE_BODY);
    const monster_type montype = static_cast<monster_type>(item.plus);
    const int max_chunks = get_max_corpse_chunks(item.plus);

    // Only fresh corpses bleed enough to colour the ground.
    if (bloodspatter && !food_is_rotten(item))
        bleed_onto_floor(you.pos(), montype, max_chunks, true);

    item.base_type = OBJ_FOOD;
    item.sub_type  = FOOD_CHUNK;
    item.quantity  = 1 + random2(max_chunks);
    item.quantity  = stepdown_value(item.quantity, 4, 4, 12, 12);

    if (you.species != SP_VAMPIRE)
        item.flags &= ~(ISFLAG_THROWN | ISFLAG_DROPPED);

    // Happens after the corpse has been butchered.
    if (make_hide && monster_descriptor(item.plus, MDSC_LEAVES_HIDE)
        && !one_chance_in(3))
    {
        _create_monster_hide(item);
    }
}

void turn_corpse_into_skeleton_and_chunks(item_def &item)
{
    item_def chunks = item;

    if (mons_skeleton(item.plus))
        turn_corpse_into_skeleton(item);

    int o = get_item_slot();
    if (o != NON_ITEM)
    {
        turn_corpse_into_chunks(chunks);
        copy_item_to_grid(chunks, you.pos());
    }
}

// Initialise blood potions with a vector of timers.
void init_stack_blood_potions(item_def &stack, int age)
{
    ASSERT(is_blood_potion(stack));

    CrawlHashTable &props = stack.props;
    props.clear(); // sanity measure
    props["timer"].new_vector(SV_INT, SFLAG_CONST_TYPE);
    CrawlVector &timer = props["timer"].get_vector();

    if (age == -1)
    {
        if (stack.sub_type == POT_BLOOD)
            age = 2500;
        else // coagulated blood
            age = 500;
    }
    // For a newly created stack, all potions use the same timer.
    const int max_age = you.num_turns + age;
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

// Sort a CrawlVector<int>, should probably be done properly with templates.
static void _int_sort(CrawlVector &vec)
{
    std::vector<int> help;
    while (!vec.empty())
    {
        help.push_back(vec[vec.size()-1].get_int());
        vec.pop_back();
    }

    std::sort(help.begin(), help.end());

    while (!help.empty())
    {
        vec.push_back(help[help.size()-1]);
        help.pop_back();
    }
}

static void _compare_blood_quantity(item_def &stack, int timer_size)
{
    if (timer_size != stack.quantity)
    {
        mprf(MSGCH_WARN,
             "ERROR: blood potion quantity (%d) doesn't match timer (%d)",
             stack.quantity, timer_size);

        // sanity measure
        stack.quantity = timer_size;
    }
}

void maybe_coagulate_blood_potions_floor(int obj)
{
    item_def &blood = mitm[obj];
    ASSERT(blood.defined());
    ASSERT(is_blood_potion(blood));

    CrawlHashTable &props = blood.props;
    if (!props.exists("timer"))
        init_stack_blood_potions(blood, blood.special);

    ASSERT(props.exists("timer"));
    CrawlVector &timer = props["timer"].get_vector();
    ASSERT(!timer.empty());
    _compare_blood_quantity(blood, timer.size());

    // blood.sub_type could be POT_BLOOD or POT_BLOOD_COAGULATED
    // -> need different handling
    int rot_limit  = you.num_turns;
    int coag_limit = you.num_turns + 500; // check 500 turns later

    // First count whether coagulating is even necessary.
    int rot_count  = 0;
    int coag_count = 0;
    std::vector<int> age_timer;
    int current;
    while (!timer.empty())
    {
        current = timer[timer.size()-1].get_int();
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
        // Nothing to be done.
        return;

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

    if (!blood.held_by_monster())
    {
        // Now that coagulating is necessary, check square for
        // !coagulated blood.
        ASSERT(blood.pos.x >= 0 && blood.pos.y >= 0);
        for (stack_iterator si(blood.pos); si; ++si)
        {
            if (si->base_type == OBJ_POTIONS
                && si->sub_type == POT_BLOOD_COAGULATED)
            {
                // Merge with existing stack.
                CrawlHashTable &props2 = si->props;
                if (!props2.exists("timer"))
                    init_stack_blood_potions(*si, si->special);

                ASSERT(props2.exists("timer"));
                CrawlVector &timer2 = props2["timer"].get_vector();
                ASSERT(timer2.size() == si->quantity);

                // Update timer -> push(pop).
                while (!age_timer.empty())
                {
                    const int val = age_timer.back();
                    age_timer.pop_back();
                    timer2.push_back(val);
                }
                _int_sort(timer2);
                inc_mitm_item_quantity(si.link(), coag_count);
                ASSERT(timer2.size() == si->quantity);
                dec_mitm_item_quantity(obj, rot_count + coag_count);
                return;
            }
        }
    }
    // If we got here, nothing was found! (Or it's in a monster's
    // inventory.)

    // Entire stack is gone, rotted or coagulated.
    // -> Change potions to coagulated type.
    if (rot_count + coag_count == blood.quantity)
    {
        ASSERT(timer.empty());

        // Update subtype.
        blood.sub_type = POT_BLOOD_COAGULATED;
        item_colour(blood);

        // Re-fill vector.
        int val;
        while (!age_timer.empty())
        {
            val = age_timer[age_timer.size() - 1];
            age_timer.pop_back();
            timer.push_back(val);
        }
        dec_mitm_item_quantity(obj, rot_count);
        _compare_blood_quantity(blood, timer.size());
        return;
    }

    // Else, create a new stack of potions.
    int o = get_item_slot(20);
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
    props_new["timer"].new_vector(SV_INT, SFLAG_CONST_TYPE);
    CrawlVector &timer_new = props_new["timer"].get_vector();

    int val;
    while (!age_timer.empty())
    {
        val = age_timer[age_timer.size() - 1];
        age_timer.pop_back();
        timer_new.push_back(val);
    }
    ASSERT(timer_new.size() == coag_count);
    props_new.assert_validity();

    if (blood.held_by_monster())
        move_item_to_grid(&o, blood.holding_monster()->pos());
    else
        move_item_to_grid(&o, blood.pos);

    dec_mitm_item_quantity(obj, rot_count + coag_count);
    _compare_blood_quantity(blood, timer.size());
}

static std::string _get_desc_quantity(const int quant, const int total)
{
    if (total == quant)
        return "Your";
    else if (quant == 1)
        return "One of your";
    else if (quant == 2)
        return "Two of your";
    else if (quant >= (total * 3) / 4)
        return "Most of your";
    else
        return "Some of your";
}

// Prints messages for blood potions coagulating in inventory (coagulate = true)
// or whenever potions are cursed into potions of decay (coagulate = false).
static void _potion_stack_changed_message(item_def &potion, int num_changed,
                                          std::string verb)
{
    ASSERT(num_changed > 0);

    verb = replace_all(verb, "%s", num_changed == 1 ? "s" : "");
    mprf(MSGCH_ROTTEN_MEAT, "%s %s %s.",
         _get_desc_quantity(num_changed, potion.quantity).c_str(),
         potion.name(DESC_PLAIN, false).c_str(),
         verb.c_str());
}

// Returns true if "equipment weighs less" message needed.
// Also handles coagulation messages.
bool maybe_coagulate_blood_potions_inv(item_def &blood)
{
    ASSERT(blood.defined());
    ASSERT(is_blood_potion(blood));

    CrawlHashTable &props = blood.props;
    if (!props.exists("timer"))
        init_stack_blood_potions(blood, blood.special);

    ASSERT(props.exists("timer"));
    CrawlVector &timer = props["timer"].get_vector();
    _compare_blood_quantity(blood, timer.size());
    ASSERT(!timer.empty());

    // blood.sub_type could be POT_BLOOD or POT_BLOOD_COAGULATED
    // -> need different handling
    int rot_limit  = you.num_turns;
    int coag_limit = you.num_turns + 500; // check 500 turns later

    // First count whether coagulating is even necessary.
    int rot_count  = 0;
    int coag_count = 0;
    std::vector<int> age_timer;
    int current;
    const int size = timer.size();
    for (int i = 0; i < size; i++)
    {
        current = timer[timer.size()-1].get_int();
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

    const bool knew_coag  = (get_ident_type(OBJ_POTIONS, POT_BLOOD_COAGULATED)
                                == ID_KNOWN_TYPE);

    if (!coag_count) // Some potions rotted away, but none coagulated.
    {
        // Only coagulated blood can rot.
        ASSERT(blood.sub_type == POT_BLOOD_COAGULATED);
        _potion_stack_changed_message(blood, rot_count, "rot%s away");
        blood.quantity -= rot_count;

        if (!knew_coag)
        {
            set_ident_type( OBJ_POTIONS, POT_BLOOD_COAGULATED, ID_KNOWN_TYPE );
            if (blood.quantity >= 1)
                mpr(blood.name(DESC_INVENTORY).c_str());
        }

        if (blood.quantity < 1)
        {
            if (you.equip[EQ_WEAPON] == blood.link)
                you.equip[EQ_WEAPON] = -1;

            destroy_item(blood);
        }
        else
            _compare_blood_quantity(blood, timer.size());

        return (true);
    }

    // Coagulated blood cannot coagulate any further...
    ASSERT(blood.sub_type == POT_BLOOD);

    const bool knew_blood = get_ident_type(OBJ_POTIONS, POT_BLOOD)
                                == ID_KNOWN_TYPE;

    _potion_stack_changed_message(blood, coag_count, "coagulate%s");

    request_autoinscribe();

    // Identify both blood and coagulated blood, if necessary.
    if (!knew_blood)
        set_ident_type( OBJ_POTIONS, POT_BLOOD, ID_KNOWN_TYPE );

    if (!knew_coag)
        set_ident_type( OBJ_POTIONS, POT_BLOOD_COAGULATED, ID_KNOWN_TYPE );

    // Now that coagulating is necessary, check inventory for !coagulated blood.
    for (int m = 0; m < ENDOFPACK; m++)
    {
        if (!you.inv[m].defined())
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
                _compare_blood_quantity(blood, timer.size());
                if (!knew_blood)
                    mpr(blood.name(DESC_INVENTORY).c_str());
            }

            // Update timer -> push(pop).
            int val;
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
            _int_sort(timer2);

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
        int val;
        while (!age_timer.empty())
        {
            val = age_timer[age_timer.size() - 1];
            age_timer.pop_back();
            timer.push_back(val);
        }
        blood.quantity -= rot_count;
        // Stack still exists because of coag_count.
        _compare_blood_quantity(blood, timer.size());

        if (!knew_coag)
            mpr(blood.name(DESC_INVENTORY).c_str());

        return (rot_count > 0);
    }

    // Else, create new stack in inventory.
    int freeslot = find_free_slot(blood);
    if (freeslot >= 0 && freeslot < ENDOFPACK)
    {
        item_def &item   = you.inv[freeslot];
        item.clear();

        item.link        = freeslot;
        item.slot        = index_to_letter(item.link);
        item.base_type   = OBJ_POTIONS;
        item.sub_type    = POT_BLOOD_COAGULATED;
        item.quantity    = coag_count;
        item.plus        = 0;
        item.plus2       = 0;
        item.special     = 0;
        item.flags       = (ISFLAG_KNOW_TYPE & ISFLAG_BEEN_IN_INV);
        item.pos.set(-1, -1);
        item_colour(item);

        CrawlHashTable &props_new = item.props;
        props_new["timer"].new_vector(SV_INT, SFLAG_CONST_TYPE);
        CrawlVector &timer_new = props_new["timer"].get_vector();

        int val;
        while (!age_timer.empty())
        {
            val = age_timer[age_timer.size() - 1];
            age_timer.pop_back();
            timer_new.push_back(val);
        }

        ASSERT(timer_new.size() == coag_count);
        props_new.assert_validity();

        blood.quantity -= coag_count + rot_count;
        _compare_blood_quantity(blood, timer.size());

        if (!knew_blood)
            mpr(blood.name(DESC_INVENTORY).c_str());
        if (!knew_coag)
            mpr(item.name(DESC_INVENTORY).c_str());

        return (rot_count > 0);
    }

    // No space in inventory, check floor.
    int o = igrd(you.pos());
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
            int val;
            while (!age_timer.empty())
            {
                val = age_timer[age_timer.size() - 1];
                age_timer.pop_back();
                timer2.push_back(val);
            }
            _int_sort(timer2);

            inc_mitm_item_quantity(o, coag_count);
            ASSERT(timer2.size() == mitm[o].quantity);
            dec_inv_item_quantity(blood.link, rot_count + coag_count);
            _compare_blood_quantity(blood, timer.size());
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
    mitm[o].flags     = (ISFLAG_KNOW_TYPE & ISFLAG_BEEN_IN_INV);
    item_colour(mitm[o]);

    CrawlHashTable &props_new = mitm[o].props;
    props_new["timer"].new_vector(SV_INT, SFLAG_CONST_TYPE);
    CrawlVector &timer_new = props_new["timer"].get_vector();

    int val;
    while (!age_timer.empty())
    {
        val = age_timer[age_timer.size() - 1];
        age_timer.pop_back();
        timer_new.push_back(val);
    }

    ASSERT(timer_new.size() == coag_count);
    props_new.assert_validity();
    move_item_to_grid(&o, you.pos());

    blood.quantity -= rot_count + coag_count;
    if (blood.quantity < 1)
    {
        if (you.equip[EQ_WEAPON] == blood.link)
            you.equip[EQ_WEAPON] = -1;

        destroy_item(blood);
    }
    else
    {
        _compare_blood_quantity(blood, timer.size());
        if (!knew_blood)
            mpr(blood.name(DESC_INVENTORY).c_str());
    }
    return (true);
}

// Removes the oldest timer of a stack of blood potions.
// Mostly used for (q)uaff, (f)ire, and Evaporate.
int remove_oldest_blood_potion(item_def &stack)
{
    ASSERT(stack.defined());
    ASSERT(is_blood_potion(stack));

    CrawlHashTable &props = stack.props;
    if (!props.exists("timer"))
        init_stack_blood_potions(stack, stack.special);
    ASSERT(props.exists("timer"));
    CrawlVector &timer = props["timer"].get_vector();
    ASSERT(!timer.empty());

    // Assuming already sorted, and first (oldest) potion valid.
    const int val = timer[timer.size() - 1].get_int();
    timer.pop_back();

    // The quantity will be decreased elsewhere.
    return (val);
}

// Used whenever copies of blood potions have to be cleaned up.
void remove_newest_blood_potion(item_def &stack, int quant)
{
    ASSERT(stack.defined());
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
    _int_sort(timer);
}

void merge_blood_potion_stacks(item_def &source, item_def &dest, int quant)
{
    if (!source.defined() || !dest.defined())
        return;

    ASSERT(quant > 0 && quant <= source.quantity);
    ASSERT(is_blood_potion(source) && is_blood_potion(dest));

    CrawlHashTable &props = source.props;
    if (!props.exists("timer"))
        init_stack_blood_potions(source, source.special);
    ASSERT(props.exists("timer"));
    CrawlVector &timer = props["timer"].get_vector();
    ASSERT(!timer.empty());

    CrawlHashTable &props2 = dest.props;
    if (!props2.exists("timer"))
        init_stack_blood_potions(dest, dest.special);
    ASSERT(props2.exists("timer"));
    CrawlVector &timer2 = props2["timer"].get_vector();

    // Update timer -> push(pop).
    for (int i = 0; i < quant; i++)
    {
        timer2.push_back(timer[timer.size() - 1].get_int());
        timer.pop_back();
    }

    // Re-sort timer.
    _int_sort(timer2);
}

bool check_blood_corpses_on_ground()
{
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->base_type == OBJ_CORPSES && si->sub_type == CORPSE_BODY
            && !food_is_rotten(*si)
            && mons_has_blood(si->plus))
        {
            return (true);
        }
    }
    return (false);
}

// Deliberately don't check for rottenness here, so this check
// can also be used to verify whether you *could* have bottled
// a now rotten corpse.
bool can_bottle_blood_from_corpse(int mons_class)
{
    if (you.species != SP_VAMPIRE || you.experience_level < 6
        || !mons_has_blood(mons_class))
    {
        return (false);
    }

    int chunk_type = mons_corpse_effect(mons_class);
    if (chunk_type == CE_CLEAN || chunk_type == CE_CONTAMINATED)
        return (true);

    return (false);
}

int num_blood_potions_from_corpse(int mons_class, int chunk_type)
{
    if (chunk_type == -1)
        chunk_type = mons_corpse_effect(mons_class);

    // Max. amount is about one third of the max. amount for chunks.
    const int max_chunks = get_max_corpse_chunks(mons_class);

    // Max. amount is about one third of the max. amount for chunks.
    int pot_quantity = max_chunks / 3;
    pot_quantity = stepdown_value(pot_quantity, 2, 2, 6, 6);

    // Halve number of potions obtained from contaminated chunk type corpses.
    if (chunk_type == CE_CONTAMINATED)
        pot_quantity /= 2;

    if (pot_quantity < 1)
        pot_quantity = 1;

    return (pot_quantity);
}

// If autopickup is active, the potions are auto-picked up after creation.
void turn_corpse_into_blood_potions(item_def &item)
{
    ASSERT(item.base_type == OBJ_CORPSES);
    ASSERT(!food_is_rotten(item));

    item_def corpse = item;
    const int mons_class = corpse.plus;

    ASSERT(can_bottle_blood_from_corpse(mons_class));

    item.base_type = OBJ_POTIONS;
    item.sub_type  = POT_BLOOD;
    item_colour(item);
    item.flags    &= ~(ISFLAG_THROWN | ISFLAG_DROPPED);

    item.quantity = num_blood_potions_from_corpse(mons_class);

    // Initialise timer depending on corpse age:
    // almost rotting: age = 100 --> potion timer =  500 --> will coagulate soon
    // freshly killed: age = 200 --> potion timer = 2500 --> fresh !blood
    init_stack_blood_potions(item, (item.special - 100) * 20 + 500);

    // Happens after the blood has been bottled.
    if (monster_descriptor(mons_class, MDSC_LEAVES_HIDE) && !one_chance_in(3))
        _create_monster_hide(corpse);
}

void turn_corpse_into_skeleton_and_blood_potions(item_def &item)
{
    item_def blood_potions = item;

    if (mons_skeleton(item.plus))
        turn_corpse_into_skeleton(item);

    int o = get_item_slot();
    if (o != NON_ITEM)
    {
        turn_corpse_into_blood_potions(blood_potions);
        copy_item_to_grid(blood_potions, you.pos());
    }
}

// A variation of the mummy curse:
// Instead of trashing the entire stack, split the stack and only turn part
// of it into POT_DECAY.
void split_potions_into_decay( int obj, int amount, bool need_msg )
{
    ASSERT(obj != -1);
    item_def &potion = you.inv[obj];

    ASSERT(potion.defined());
    ASSERT(potion.base_type == OBJ_POTIONS);
    ASSERT(amount > 0);
    ASSERT(amount <= potion.quantity);

    // Output decay message.
    if (need_msg && get_ident_type(OBJ_POTIONS, POT_DECAY) == ID_KNOWN_TYPE)
        _potion_stack_changed_message(potion, amount, "decay%s");

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
        && !you.inv[freeslot].defined())
    {
        item_def &item   = you.inv[freeslot];
        item.link        = freeslot;
        item.slot        = index_to_letter(item.link);
        item.base_type   = OBJ_POTIONS;
        item.sub_type    = POT_DECAY;
        item.quantity    = amount;
        // Keep description as it was.
        item.plus        = potion.plus;
        item.plus2       = 0;
        item.special     = 0;
        item.flags       = 0;
        item.colour      = potion.colour;
        item.inscription.clear();
        item.pos.set(-1, -1);

        you.inv[obj].quantity -= amount;
        return;
    }
    // Okay, inventory is full.

    // Check whether we can merge with an existing stack on the floor.
    for (stack_iterator si(you.pos()); si; ++si)
    {
        if (si->base_type == OBJ_POTIONS && si->sub_type == POT_DECAY)
        {
            dec_inv_item_quantity(obj, amount);
            inc_mitm_item_quantity(si->index(), amount);
            return;
        }
    }

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

    copy_item_to_grid(potion2, you.pos());

    // Is decreased even if the decay stack goes splat.
    dec_inv_item_quantity(obj, amount);
}

static bool allow_bleeding_on_square(const coord_def& where)
{
    // No bleeding onto sanctuary ground, please.
    // Also not necessary if already covered in blood.
    if (is_bloodcovered(where) || is_sanctuary(where))
        return (false);

    // No spattering into lava or water.
    if (grd(where) >= DNGN_LAVA && grd(where) < DNGN_FLOOR)
        return (false);

    // No spattering into fountains (other than blood).
    if (grd(where) == DNGN_FOUNTAIN_BLUE
        || grd(where) == DNGN_FOUNTAIN_SPARKLING)
    {
        return (false);
    }

    // The good gods like to keep their altars pristine.
    if (is_good_god(feat_altar_god(grd(where))))
        return (false);

    return (true);
}

bool maybe_bloodify_square(const coord_def& where)
{
    if (!allow_bleeding_on_square(where))
        return (false);

    env.pgrid(where) |= FPROP_BLOODY;
    return(true);
}

static void _maybe_bloodify_square(const coord_def& where, int amount,
                                   bool spatter = false,
                                   bool smell_alert = true)
{
    if (amount < 1)
        return;

    bool may_bleed = allow_bleeding_on_square(where);
    if (!spatter && !may_bleed)
        return;

    if (x_chance_in_y(amount, 20))
    {
        dprf("might bleed now; square: (%d, %d); amount = %d",
             where.x, where.y, amount);
        if (may_bleed)
        {
            env.pgrid(where) |= FPROP_BLOODY;

            if (smell_alert && in_bounds(where))
                blood_smell(12, where);
        }

        if (spatter)
        {
            // Smaller chance of spattering surrounding squares.
            for (adjacent_iterator ai(where); ai; ++ai)
            {
                // Spattering onto walls etc. less likely.
                if (grd(*ai) < DNGN_MINMOVE && !one_chance_in(3))
                    continue;

                _maybe_bloodify_square(*ai, amount/15);
            }
        }
    }
}

// Currently flavour only: colour ground (and possibly adjacent squares) red.
// "damage" depends on damage taken (or hitpoints, if damage higher),
// or, for sacrifices, on the number of chunks possible to get out of a corpse.
void bleed_onto_floor(const coord_def& where, monster_type montype,
                      int damage, bool spatter, bool smell_alert)
{
    ASSERT(in_bounds(where));

    if (montype == MONS_PLAYER && !you.can_bleed())
        return;

    if (montype != NUM_MONSTERS && montype != MONS_PLAYER)
    {
        monster m;
        m.type = montype;
        if (!m.can_bleed())
            return;
    }

    _maybe_bloodify_square(where, damage, spatter, smell_alert);
}

void blood_spray(const coord_def& origin, monster_type montype, int level)
{
    los_def ld(origin, opc_solid);

    ld.update();

    int tries = 0;
    for (int i = 0; i < level; ++i)
    {
        // Blood drops are small and light and suffer a lot of wind
        // resistance.
        int range = random2(8) + 1;

        while (tries < 5000)
        {
            ++tries;

            coord_def bloody = origin;
            bloody.x += random_range(-range, range);
            bloody.y += random_range(-range, range);

            if (in_bounds(bloody) && ld.see_cell(bloody))
            {
                if (feat_is_solid(grd(bloody)) && coinflip())
                    continue;

                bleed_onto_floor(bloody, montype, 99);
                break;
            }
        }
    }
}

static void _spatter_neighbours(const coord_def& where, int chance)
{
    for (adjacent_iterator ai(where, false); ai; ++ai)
    {
        if (!allow_bleeding_on_square(*ai))
            continue;

        if (grd(*ai) < DNGN_MINMOVE && !one_chance_in(3))
            continue;

        if (one_chance_in(chance))
        {
            env.pgrid(*ai) |= FPROP_BLOODY;
            _spatter_neighbours(*ai, chance+1);
        }
    }
}

void generate_random_blood_spatter_on_level(const map_mask *susceptible_area)
{
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

    for (int i = 0; i < max_cluster; ++i)
    {
        const coord_def c = random_in_bounds();

        if (susceptible_area && !(*susceptible_area)(c))
            continue;

        // startprob is used to initialise the chance for neighbours
        // being spattered, which will be decreased by 1 per recursion
        // round. We then use one_chance_in(chance) to determine
        // whether to spatter a given grid or not. Thus, startprob = 1
        // means that initially all surrounding grids will be
        // spattered (3x3), and the _higher_ startprob the _lower_ the
        // overall chance for spattering and the _smaller_ the
        // bloodshed area.
        const int startprob = min_prob + random2(max_prob);

        maybe_bloodify_square(c);

        _spatter_neighbours(c, startprob);
    }
}

void search_around(bool only_adjacent)
{
    ASSERT(!crawl_state.game_is_arena());

    // Traps and doors stepdown skill:
    // skill/(2x-1) for squares at distance x
    int max_dist = (you.skills[SK_TRAPS_DOORS] + 1) / 2;
    if (max_dist > 5)
        max_dist = 5;
    if (only_adjacent && max_dist > 1 || max_dist < 1)
        max_dist = 1;

    for (radius_iterator ri(you.pos(), max_dist); ri; ++ri )
    {
        // Must have LOS, with no translucent walls in the way.
        if (you.see_cell_no_trans(*ri))
        {
            // Maybe we want distance() instead of feat_distance()?
            int dist = grid_distance(*ri, you.pos());

            // Don't exclude own square; may be levitating.
            // XXX: Currently, levitating over a trap will always detect it.
            if (dist == 0)
                ++dist;

            // Making this harsher by removing the old +1...
            int effective = you.skills[SK_TRAPS_DOORS] / (2*dist - 1);

            if (grd(*ri) == DNGN_SECRET_DOOR && x_chance_in_y(effective+1, 17))
            {
                mpr("You found a secret door!");
                reveal_secret_door(*ri);
                practise(EX_FOUND_SECRET_DOOR);
            }
            else if (grd(*ri) == DNGN_UNDISCOVERED_TRAP
                     && x_chance_in_y(effective + 1, 17))
            {
                trap_def* ptrap = find_trap(*ri);
                if (ptrap)
                {
                    ptrap->reveal();
                    mpr("You found a trap!");
                    learned_something_new(HINT_SEEN_TRAP, *ri);
                    practise(EX_TRAP_FOUND);
                }
                else
                {
                    // Maybe we shouldn't kill the trap for debugging
                    // purposes - oh well.
                    grd(*ri) = DNGN_FLOOR;
                    dprf("You found a buggy trap! It vanishes!");
                }
            }
        }
    }
}

void merfolk_start_swimming(bool stepped)
{
    if (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE)
        untransform(false, true); // We're already entering the water.

    if (stepped)
        mpr("Your legs become a tail as you enter the water.");
    else
        mpr("Your legs become a tail as you dive into the water.");

    remove_one_equip(EQ_BOOTS);
    you.redraw_evasion = true;

#ifdef USE_TILE
    init_player_doll();
#endif
}

void merfolk_stop_swimming()
{
    unmeld_one_equip(EQ_BOOTS);
    you.redraw_evasion = true;

#ifdef USE_TILE
    init_player_doll();
#endif
}

// Update the trackers after the player changed level.
void trackers_init_new_level(bool transit)
{
    travel_init_new_level();
    if (transit)
        stash_init_new_level();
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
    ASSERT(!crawl_state.game_is_arena());

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

bool go_berserk(bool intentional, bool potion)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!you.can_go_berserk(intentional, potion))
        return (false);

    if (stasis_blocks_effect(true,
                             true,
                             "%s thrums violently and saps your rage.",
                             3,
                             "%s vibrates violently and saps your rage."))
    {
        return (false);
    }

    if (Hints.hints_left)
        Hints.hints_berserk_counter++;

    // Che prevents hasting the player if the berserk was unintentional.
    // Otherwise, the haste would be forgiven "just this once" every time.
    const bool che_blocks_haste =
        you.religion == GOD_CHEIBRIADOS && !intentional;

    mpr("A red film seems to cover your vision as you go berserk!");

    if (che_blocks_haste)
        simple_god_message(" protects you from inadvertent hurry.");
    else
        mpr("You feel yourself moving faster!");

    mpr("You feel mighty!");

    // Cutting the duration in half since berserk causes haste and hasted
    // actions have half the usual delay. This keeps player turns
    // approximately consistent withe previous versions. -cao
    int berserk_duration = (20 + random2avg(19,2)) / 2;

    you.increase_duration(DUR_BERSERKER, berserk_duration);

    calc_hp();
    you.hp = you.hp * 3 / 2;

    deflate_hp(you.hp_max, false);

    if (!you.duration[DUR_MIGHT])
        notify_stat_change(STAT_STR, 5, true, "going berserk");

    you.increase_duration(DUR_MIGHT, berserk_duration);

    if (!che_blocks_haste)
    {
        // doubling the duration here since haste_player already cuts input
        // durations in half
        haste_player(berserk_duration * 2);

        did_god_conduct(DID_HASTY, 8, intentional);
    }

    if (you.berserk_penalty != NO_BERSERK_PENALTY)
        you.berserk_penalty = 0;

    return (true);
}

// Returns true if the monster has a path to the player, or it has to be
// assumed that this is the case.
static bool _mons_has_path_to_player(const monster* mon, bool want_move = false)
{
    // Don't consider sleeping monsters safe, in case the player would
    // rather retreat and try another path for maximum stabbing chances.
    // TODO: This doesn't cover monsters encaged in glass.
    if (mon->asleep())
        return (true);

    if (mons_is_stationary(mon))
    {
        int dist = grid_distance(you.pos(), mon->pos());
        if (want_move)
            dist--;
        if (dist >= 2)
            return (false);
    }

    // Short-cut if it's already adjacent.
    if (grid_distance(mon->pos(), you.pos()) <= 1)
        return (true);

    // If the monster is awake and knows a path towards the player
    // (even though the player cannot know this) treat it as unsafe.
    if (mon->travel_target == MTRAV_PLAYER)
        return (true);

    if (mon->travel_target == MTRAV_KNOWN_UNREACHABLE)
        return (false);

    // Try to find a path from monster to player, using the map as it's
    // known to the player and assuming unknown terrain to be traversable.
    monster_pathfind mp;
    const int range = mons_tracking_range(mon);
    // Use a large safety margin.  x4 should be ok.
    if (range > 0)
        mp.set_range(range * 4);

    if (mp.init_pathfind(mon, you.pos(), true, false, true))
        return (true);

    // Now we know the monster cannot possibly reach the player.
    mon->travel_target = MTRAV_KNOWN_UNREACHABLE;

    return (false);
}

bool mons_can_hurt_player(const monster* mon, const bool want_move)
{
    // FIXME: This takes into account whether the player knows the map!
    //        It should, for the purposes of i_feel_safe. [rob]
    // It also always returns true for sleeping monsters, but that's okay
    // for its current purposes. (Travel interruptions and tension.)
    if (_mons_has_path_to_player(mon, want_move))
        return (true);

    // The monster need only see you to hurt you.
    if (mons_has_los_attack(mon))
        return (true);

    // Even if the monster can not actually reach the player it might
    // still use some ranged form of attack.
    if (you.see_cell_no_trans(mon->pos())
        && (mons_has_ranged_attack(mon)
            || mons_has_ranged_ability(mon)
            || mons_has_ranged_spell(mon)))
    {
        return (true);
    }

    return (false);
}

bool mons_is_safe(const monster* mon, const bool want_move,
                  const bool consider_user_options)
{
    if (mons_is_unknown_mimic(mon))
        return (true);

    int  dist    = grid_distance(you.pos(), mon->pos());

    bool is_safe = (mon->wont_attack()
                    || mons_class_flag(mon->type, M_NO_EXP_GAIN)
                       && mon->type != MONS_KRAKEN_TENTACLE
                    || mon->pacified() && dist > 1
                    || mon->type == MONS_BALLISTOMYCETE && mon->number == 0
#ifdef WIZARD
                    // Wizmode skill setting enforces hiddenness.
                    || you.skills[SK_STEALTH] > 27 && dist > 2
#endif
                       // Only seen through glass walls or within water?
                       // Assuming that there are no water-only/lava-only
                       // monsters capable of throwing or zapping wands.
                    || !mons_can_hurt_player(mon, want_move));

#ifdef CLUA_BINDINGS
    if (consider_user_options)
    {
        bool moving = (!you.delay_queue.empty()
                          && delay_is_run(you.delay_queue.front().type)
                          && you.delay_queue.front().type != DELAY_REST
                       || you.running < RMODE_NOT_RUNNING
                       || want_move);

        bool result = is_safe;

        monster_info mi(mon, MILEV_SKIP_SAFE);
        if (clua.callfn("ch_mon_is_safe", "Ibbd>b",
                        &mi, is_safe, moving, dist,
                        &result))
        {
            is_safe = result;
        }
    }
#endif

    return (is_safe);
}

// Return all nearby monsters in range (default: LOS) that the player
// is able to recognise as being monsters (i.e. no unknown mimics or
// submerged creatures.)
//
// want_move       (??) Somehow affects what monsters are considered dangerous
// just_check      Return zero or one monsters only
// dangerous_only  Return only "dangerous" monsters
// require_visible Require that monsters be visible to the player
// range           search radius (defaults: LOS)
//
std::vector<monster* > get_nearby_monsters(bool want_move,
                                           bool just_check,
                                           bool dangerous_only,
                                           bool consider_user_options,
                                           bool require_visible,
                                           int range)
{
    ASSERT(!crawl_state.game_is_arena());

    if (range == -1)
        range = LOS_RADIUS;

    std::vector<monster* > mons;

    // Sweep every visible square within range.
    for (radius_iterator ri(you.pos(), range); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            if (mon->alive()
                && (!require_visible || mon->visible_to(&you))
                && !mon->submerged()
                && !mons_is_unknown_mimic(mon)
                && (!dangerous_only || !mons_is_safe(mon, want_move,
                                                     consider_user_options)))
            {
                mons.push_back(mon);
                if (just_check) // stop once you find one
                    break;
            }
        }
    }
    return mons;
}

static bool _exposed_monsters_nearby(bool want_move)
{
    const int radius = want_move ? 2 : 1;
    for (radius_iterator ri(you.pos(), radius); ri; ++ri)
        if (env.map_knowledge(*ri).flags & MAP_INVISIBLE_MONSTER)
            return (true);
    return (false);
}

bool i_feel_safe(bool announce, bool want_move, bool just_monsters, int range)
{
    if (!just_monsters)
    {
        // check clouds
        if (in_bounds(you.pos()) && env.cgrid(you.pos()) != EMPTY_CLOUD)
        {
            const int cloudidx = env.cgrid(you.pos());
            const cloud_type type = env.cloud[cloudidx].type;

            if (is_damaging_cloud(type, want_move))
            {
                if (announce)
                {
                    mprf(MSGCH_WARN, "You're standing in a cloud of %s!",
                         cloud_name_at_index(cloudidx).c_str());
                }
                return (false);
            }
        }

        // No monster will attack you inside a sanctuary,
        // so presence of monsters won't matter -- until it starts shrinking...
        if (is_sanctuary(you.pos()) && env.sanctuary_time >= 5)
            return (true);
    }

    // Monster check.
    std::vector<monster* > visible =
        get_nearby_monsters(want_move, !announce, true, true, true, range);

    // Announce the presence of monsters (Eidolos).
    std::string msg;
    if (visible.size() == 1)
    {
        const monster& m = *visible[0];
        const std::string monname = mons_is_mimic(m.type)
                                  ? "A mimic"
                                  : m.name(DESC_CAP_A);
        msg = make_stringf("%s is nearby!", monname.c_str());
    }
    else if (visible.size() > 1)
        msg = "There are monsters nearby!";
    else if (_exposed_monsters_nearby(want_move))
        msg = "There is a strange disturbance nearby!";
    else
        return (true);

    if (announce)
        mpr(msg, MSGCH_WARN);

    return (false);
}

bool there_are_monsters_nearby(bool dangerous_only, bool require_visible,
                               bool consider_user_options)
{
    return (!get_nearby_monsters(false, true, dangerous_only,
                                 consider_user_options,
                                 require_visible).empty());
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

    for (monster_iterator mi(you.get_los()); mi; ++mi)
    {
        if (!mi->friendly())
        {
            const int xp = exper_value(*mi);
            const double log_xp = log((double)xp);
            sum += log_xp;
            if (xp > highest_xp)
            {
                highest_xp = xp;
                *highest   = log_xp;
            }
            if (!you.can_see(*mi))
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

static void _drop_tomb(const coord_def& pos, bool premature)
{
    int count = 0;

    monster* mon = monster_at(pos);
    // Don't wander on duty!
    if (mon)
        mon->behaviour = BEH_SEEK;

    bool seen_change = false;
    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        if (grd(*ai) == DNGN_ROCK_WALL)
        {
            grd(*ai) = DNGN_FLOOR;
            set_terrain_changed(*ai);
            count++;
            if (you.see_cell(*ai))
                seen_change = true;
        }
    }

    if (count)
    {
        if (seen_change)
            mprf("The walls disappear%s!",
                 premature ? " prematurely" : "");
        else
        {
            if (!silenced(you.pos()))
                mpr("You hear a deep rumble.", MSGCH_SOUND);
            else
                mpr("You feel the ground shudder.");
        }
    }
}

void timeout_tombs(int duration)
{
    if (!duration)
        return;

    std::vector<map_marker*> markers = env.markers.get_all(MAT_TOMB);

    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_marker *mark = markers[i];
        if (mark->get_type() != MAT_TOMB)
            continue;

        map_tomb_marker *cmark = dynamic_cast<map_tomb_marker*>(mark);
        cmark->duration -= duration;

        // Tombs without monsters in them disappear early.
        monster* mon_entombed = monster_at(cmark->pos);
        if (cmark->duration <= 0 || !mon_entombed)
        {
            _drop_tomb(cmark->pos, !mon_entombed);

            monster* mon_src =
                !invalid_monster_index(cmark->source) ? &menv[cmark->source]
                                                      : NULL;
            monster* mon_targ =
                !invalid_monster_index(cmark->target) ? &menv[cmark->target]
                                                      : NULL;

            // Zin's Imprison ability.
            if (cmark->source == -GOD_ZIN && mon_targ
                && mon_targ == mon_entombed)
            {
                zin_recite_to_single_monster(mon_targ->pos(), true);
            }
            // A monster's Tomb of Doroklohe spell.
            else if (mon_src
                && mon_src == mon_entombed)
            {
                mon_src->lose_energy(EUT_SPELL);
            }

            env.markers.remove(cmark);
        }
    }
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
    // Don't apply if if the feature doesn't want it.
    if (testbits(env.pgrid(c), FPROP_NO_CLOUD_GEN))
        return;
    if (grid == DNGN_LAVA)
        check_place_cloud(CLOUD_BLACK_SMOKE, c, random_range(4, 8), KC_OTHER);
    else if (grid == DNGN_SHALLOW_WATER)
        check_place_cloud(CLOUD_MIST,        c, random_range(2, 5), KC_OTHER);
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
    shoals_apply_tides(div_rand_round(you.time_taken, BASELINE_DELAY),
                       false, true);
    timeout_tombs(you.time_taken);
}

coord_def pick_adjacent_free_square(const coord_def& p)
{
    int num_ok = 0;
    coord_def result(-1, -1);

    for (adjacent_iterator ai(p); ai; ++ai)
        if (grd(*ai) == DNGN_FLOOR && monster_at(*ai) == NULL)
            if (one_chance_in(++num_ok))
                result = *ai;

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

void reveal_secret_door(const coord_def& p)
{
    ASSERT(grd(p) == DNGN_SECRET_DOOR);

    const std::string door_open_prompt =
        env.markers.property_at(p, MAT_ANY, "door_open_prompt");

    // Former secret doors become known but are still hidden to monsters
    // until opened. Transparent secret doors are opened to not change
    // LOS, unless they have a warning prompt.
    dungeon_feature_type door = grid_secret_door_appearance(p);
    if (feat_is_opaque(door) || !door_open_prompt.empty())
        grd(p) = DNGN_DETECTED_SECRET_DOOR;
    else
        grd(p) = DNGN_OPEN_DOOR;

    set_terrain_changed(p);
    viewwindow();
    learned_something_new(HINT_FOUND_SECRET_DOOR, p);
}

// A feeble attempt at Nethack-like completeness for cute messages.
std::string your_hand(bool plural)
{
    std::string result;

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    default:
        mpr("ERROR: unknown transformation in your_hand() (misc.cc)",
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
    case TRAN_PIG:
        result = "front leg";
        break;
    case TRAN_DRAGON:
    case TRAN_BAT:
        result = "foreclaw";
        break;
    case TRAN_BLADE_HANDS:
        result = "scythe-like blade";
        break;
    }

    if (plural)
        result += 's';

    return result;
}

bool stop_attack_prompt(const monster* mon, bool beam_attack,
                        coord_def beam_target, bool autohit_first)
{
    ASSERT(!crawl_state.game_is_arena());

    if (you.confused() || !you.can_see(mon))
        return (false);

    bool retval = false;
    bool prompt = false;

    const bool mon_target    = (beam_target == mon->pos());
    const bool inSanctuary   = (is_sanctuary(you.pos())
                                || is_sanctuary(mon->pos()));
    const bool wontAttack    = mon->wont_attack();
    const bool isFriendly    = mon->friendly();
    const bool isNeutral     = mon->neutral();
    const bool isUnchivalric = is_unchivalric_attack(&you, mon);
    const bool isHoly        = mon->is_holy()
                                   && (mon->attitude != ATT_HOSTILE
                                       || testbits(mon->flags, MF_NO_REWARD)
                                       || testbits(mon->flags, MF_WAS_NEUTRAL));

    if (isFriendly)
    {
        // Listed in the form: "your rat", "Blork the orc".
        std::string mon_name = mon->name(DESC_PLAIN);
        mon_name = std::string("your ") +
                   (you.religion == GOD_OKAWARU ? "ally the " : "") +
                   mon_name;
        std::string verb = "";
        bool need_mon_name = true;
        if (beam_attack)
        {
            verb = "fire ";
            if (mon_target)
                verb += "at ";
            else if (you.pos() < beam_target && beam_target < mon->pos()
                     || you.pos() > beam_target && beam_target > mon->pos())
            {
                if (autohit_first)
                    return (false);

                verb += "in " + apostrophise(mon_name) + " direction";
                need_mon_name = false;
            }
            else
                verb += "through ";
        }
        else
            verb = "attack ";

        if (need_mon_name)
            verb += mon_name;

        snprintf(info, INFO_SIZE, "Really %s%s?",
                 verb.c_str(),
                 (inSanctuary) ? ", despite your sanctuary" : "");

        prompt = true;
    }
    else if (inSanctuary || wontAttack
             || (you.religion == GOD_JIYVA && mons_is_slime(mon)
                 && !mon->is_shapeshifter())
             || (isNeutral || isHoly) && is_good_god(you.religion)
             || isUnchivalric
                && you.religion == GOD_SHINING_ONE
                && !tso_unchivalric_attack_safe_monster(mon))
    {
        snprintf(info, INFO_SIZE, "Really %s the %s%s%s%s%s?",
                 (beam_attack) ? (mon_target) ? "fire at"
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

bool is_orckind(const actor *act)
{
    if (mons_genus(act->mons_species()) == MONS_ORC)
        return (true);

    if (act->atype() == ACT_MONSTER)
    {
        const monster* mon = act->as_monster();
        if (mons_is_zombified(mon)
            && mons_genus(mon->base_monster) == MONS_ORC)
        {
            return (true);
        }
        if (mons_is_ghost_demon(mon->type)
            && mon->ghost->species == SP_HILL_ORC)
        {
            return (true);
        }
    }

    return (false);
}

bool is_dragonkind(const actor *act)
{
    if (mons_genus(act->mons_species()) == MONS_DRAGON
        || mons_genus(act->mons_species()) == MONS_DRACONIAN)
    {
        return (true);
    }

    if (act->atype() == ACT_PLAYER)
        return (you.attribute[ATTR_TRANSFORMATION] == TRAN_DRAGON);

    // Else the actor is a monster.
    const monster* mon = act->as_monster();

    if (mon->type == MONS_SERPENT_OF_HELL)
        return (true);

    if (mons_is_zombified(mon)
        && (mons_genus(mon->base_monster) == MONS_DRAGON
            || mons_genus(mon->base_monster) == MONS_DRACONIAN))
    {
        return (true);
    }

    if (mons_is_ghost_demon(mon->type)
        && species_genus(mon->ghost->species) == GENPC_DRACONIAN)
    {
        return (true);
    }

    return (false);
}

// Make the player swap positions with a given monster.
void swap_with_monster(monster* mon_to_swap)
{
    monster& mon(*mon_to_swap);
    ASSERT(mon.alive());
    const coord_def newpos = mon.pos();

    // Be nice: no swapping into uninhabitable environments.
    if (!you.is_habitable(newpos) || !mon.is_habitable(you.pos()))
    {
        mpr("You spin around.");
        return;
    }

    const bool mon_caught = mon.caught();
    const bool you_caught = you.attribute[ATTR_HELD];

    // If it was submerged, it surfaces first.
    mon.del_ench(ENCH_SUBMERGED);

    mprf("You swap places with %s.", mon.name(DESC_NOCAP_THE).c_str());

    // Pick the monster up.
    mgrd(newpos) = NON_MONSTER;
    mon.moveto(you.pos());

    // Plunk it down.
    mgrd(mon.pos()) = mon_to_swap->mindex();

    if (you_caught)
    {
        check_net_will_hold_monster(&mon);
        if (!mon_caught)
            you.attribute[ATTR_HELD] = 0;
    }

    // Move you to its previous location.
    move_player_to_grid(newpos, false, true);

    if (mon_caught)
    {
        if (you.body_size(PSIZE_BODY) >= SIZE_GIANT)
        {
            mpr("The net rips apart!");
            you.attribute[ATTR_HELD] = 0;
            int net = get_trapping_net(you.pos());
            if (net != NON_ITEM)
                destroy_item(net);
        }
        else
        {
            you.attribute[ATTR_HELD] = 10;
            mpr("You become entangled in the net!");

            // Xom thinks this is hilarious if you trap yourself this way.
            if (you_caught)
                xom_is_stimulated(16);
            else
                xom_is_stimulated(255);
        }

        if (!you_caught)
            mon.del_ench(ENCH_HELD, true);
    }
}

// AutoID an equipped ring of teleport.
// Code copied from fire/ice in spl-cast.cc
void maybe_id_ring_TC()
{
    if (you.duration[DUR_CONTROL_TELEPORT]
        || player_mutation_level(MUT_TELEPORT_CONTROL))
    {
        return;
    }

    int num_unknown = 0;
    for (int i = EQ_LEFT_RING; i <= EQ_RIGHT_RING; ++i)
    {
        if (player_wearing_slot(i)
            && !item_ident(you.inv[you.equip[i]], ISFLAG_KNOW_PROPERTIES))
        {
            ++num_unknown;
        }
    }

    if (num_unknown != 1)
        return;

    for (int i = EQ_LEFT_RING; i <= EQ_RIGHT_RING; ++i)
        if (player_wearing_slot(i))
        {
            item_def& ring = you.inv[you.equip[i]];
            if (!item_ident(ring, ISFLAG_KNOW_PROPERTIES)
                && ring.sub_type == RING_TELEPORT_CONTROL)
            {
                set_ident_type( ring.base_type, ring.sub_type, ID_KNOWN_TYPE );
                set_ident_flags(ring, ISFLAG_KNOW_PROPERTIES);
                mprf("You are wearing: %s",
                     ring.name(DESC_INVENTORY_EQUIP).c_str());
            }
        }
}

void make_fake_undead(monster* mon, monster_type undead)
{
    mon->flags |= MF_FAKE_UNDEAD;

    if (!mons_class_can_regenerate(undead))
        mon->flags |= MF_NO_REGEN;
}
