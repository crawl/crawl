/**
 * @file
 * @brief Misc functions.
**/

#include "AppHdr.h"

#include "misc.h"

#include <string.h>
#include <algorithm>

#if !defined(__IBMCPP__) && !defined(TARGET_COMPILER_VC)
#include <unistd.h>
#endif

#include <cstdlib>
#include <cstdio>
#include <cmath>

#include "externs.h"
#include "misc.h"

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "artefact.h"
#include "clua.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgn-shoals.h"
#include "dgnevent.h"
#include "env.h"
#include "feature.h"
#include "fight.h"
#include "files.h"
#include "fprop.h"
#include "food.h"
#include "ghost.h"
#include "godabil.h"
#include "godpassive.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "losglobal.h"
#include "makeitem.h"
#include "mapmark.h"
#include "message.h"
#include "mgen_data.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-pathfind.h"
#include "mon-info.h"
#include "mon-stuff.h"
#include "ng-setup.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "player-stats.h"
#include "random.h"
#include "religion.h"
#include "godconduct.h"
#include "shopping.h"
#include "skills.h"
#include "skills2.h"
#include "spl-clouds.h"
#include "state.h"
#include "stuff.h"
#include "target.h"
#include "terrain.h"
#include "tileview.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "hints.h"
#include "view.h"
#include "shout.h"
#include "xom.h"

static void _create_monster_hide(const item_def corpse)
{
    // kiku_receive_corpses() creates corpses that are easily scummed
    // for hides.  We prevent this by setting "never_hide" as an item
    // property of corpses it creates.
    if (corpse.props.exists("never_hide"))
        return;

    monster_type mons_class = corpse.mon_type;
    armour_type type;

    // These values cannot be set by a reasonable formula: {dlb}
    if (mons_genus(mons_class) == MONS_TROLL)
        mons_class = MONS_TROLL;
    switch (mons_class)
    {
    case MONS_FIRE_DRAGON:    type = ARM_FIRE_DRAGON_HIDE;    break;
    case MONS_TROLL:          type = ARM_TROLL_HIDE;          break;
    case MONS_ICE_DRAGON:     type = ARM_ICE_DRAGON_HIDE;     break;
    case MONS_STEAM_DRAGON:   type = ARM_STEAM_DRAGON_HIDE;   break;
    case MONS_MOTTLED_DRAGON: type = ARM_MOTTLED_DRAGON_HIDE; break;
    case MONS_STORM_DRAGON:   type = ARM_STORM_DRAGON_HIDE;   break;
    case MONS_GOLDEN_DRAGON:  type = ARM_GOLD_DRAGON_HIDE;    break;
    case MONS_SWAMP_DRAGON:   type = ARM_SWAMP_DRAGON_HIDE;   break;
    case MONS_PEARL_DRAGON:   type = ARM_PEARL_DRAGON_HIDE;   break;
    default:
        die("an unknown hide drop");
    }

    int o = items(0, OBJ_ARMOUR, type, true, 0, MAKE_ITEM_NO_RACE, 0, 0, -1,
                  true);
    if (o == NON_ITEM)
        return;
    item_def& item = mitm[o];

    do_uncurse_item(item, false);
    const monster_type montype =
        static_cast<monster_type>(corpse.orig_monnum);
    if (!invalid_monster_type(montype) && mons_is_unique(montype))
        item.inscription = mons_type_name(montype, DESC_PLAIN);

    const coord_def pos = item_pos(corpse);
    if (!pos.origin())
        move_item_to_grid(&o, pos);
}

void maybe_drop_monster_hide(const item_def corpse)
{
    if (mons_class_leaves_hide(corpse.mon_type) && !one_chance_in(3))
        _create_monster_hide(corpse);
}

int get_max_corpse_chunks(monster_type mons_class)
{
    return mons_weight(mons_class) / 150;
}

void turn_corpse_into_skeleton(item_def &item)
{
    ASSERT(item.base_type == OBJ_CORPSES);
    ASSERT(item.sub_type == CORPSE_BODY);

    // Some monsters' corpses lack the structure to leave skeletons
    // behind.
    if (!mons_skeleton(item.mon_type))
        return;

    item.sub_type = CORPSE_SKELETON;
    item.special  = FRESHEST_CORPSE; // reset rotting counter
    item.colour   = LIGHTGREY;
}

static void _maybe_bleed_monster_corpse(const item_def corpse)
{
    // Only fresh corpses bleed enough to colour the ground.
    if (!food_is_rotten(corpse))
    {
        const coord_def pos = item_pos(corpse);
        if (!pos.origin())
        {
            const int max_chunks = get_max_corpse_chunks(corpse.mon_type);
            bleed_onto_floor(pos, corpse.mon_type, max_chunks, true);
        }
    }
}

void turn_corpse_into_chunks(item_def &item, bool bloodspatter,
                             bool make_hide)
{
    ASSERT(item.base_type == OBJ_CORPSES);
    ASSERT(item.sub_type == CORPSE_BODY);
    const item_def corpse = item;
    const int max_chunks = get_max_corpse_chunks(item.mon_type);

    // Only fresh corpses bleed enough to colour the ground.
    if (bloodspatter)
        _maybe_bleed_monster_corpse(corpse);

    item.base_type = OBJ_FOOD;
    item.sub_type  = FOOD_CHUNK;
    item.quantity  = 1 + random2(max_chunks);
    item.quantity  = stepdown_value(item.quantity, 4, 4, 12, 12);

    bool wants_for_spells = you.has_spell(SPELL_SIMULACRUM)
                            || you.has_spell(SPELL_SUBLIMATION_OF_BLOOD);
    // Don't mark it as dropped if we are forcing autopickup of chunks.
    if (you.force_autopickup[OBJ_FOOD][FOOD_CHUNK] <= 0
        && is_bad_food(item) && !wants_for_spells)
    {
        item.flags |= ISFLAG_DROPPED;
    }
    else if (you.species != SP_VAMPIRE)
        clear_item_pickup_flags(item);

    // Happens after the corpse has been butchered.
    if (make_hide)
        maybe_drop_monster_hide(corpse);
}

static void _turn_corpse_into_skeleton_and_chunks(item_def &item, bool prefer_chunks)
{
    item_def copy = item;

    // Complicated logic, but unless we use the original, both could fail if
    // mitm[] is overstuffed.
    if (prefer_chunks)
    {
        turn_corpse_into_chunks(item);
        turn_corpse_into_skeleton(copy);
    }
    else
    {
        turn_corpse_into_chunks(copy);
        turn_corpse_into_skeleton(item);
    }

    copy_item_to_grid(copy, item_pos(item));
}

void butcher_corpse(item_def &item, maybe_bool skeleton, bool chunks)
{
    item_was_destroyed(item);
    if (!mons_skeleton(item.mon_type))
        skeleton = MB_FALSE;
    if (skeleton == MB_TRUE || skeleton == MB_MAYBE && one_chance_in(3))
    {
        if (chunks)
            _turn_corpse_into_skeleton_and_chunks(item, skeleton != MB_TRUE);
        else
        {
            _maybe_bleed_monster_corpse(item);
            maybe_drop_monster_hide(item);
            turn_corpse_into_skeleton(item);
        }
    }
    else
    {
        if (chunks)
            turn_corpse_into_chunks(item);
        else
        {
            _maybe_bleed_monster_corpse(item);
            maybe_drop_monster_hide(item);
            destroy_item(item.index());
        }
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
    vector<int> help;
    while (!vec.empty())
    {
        help.push_back(vec[vec.size()-1].get_int());
        vec.pop_back();
    }

    sort(help.begin(), help.end());

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

static void _init_coagulated_blood(item_def &stack, int count, item_def &old,
                                   vector<int> &age_timer)
{
    stack.base_type = OBJ_POTIONS;
    stack.sub_type  = POT_BLOOD_COAGULATED;
    stack.quantity  = count;
    stack.plus      = 0;
    stack.plus2     = 0;
    stack.special   = 0;
    stack.flags     = old.flags & (ISFLAG_DROPPED | ISFLAG_THROWN
                                   | ISFLAG_NO_PICKUP | ISFLAG_SUMMONED
                                   | ISFLAG_DROPPED_BY_ALLY);
    stack.inscription = old.inscription;
    item_colour(stack);

    CrawlHashTable &props_new = stack.props;
    props_new["timer"].new_vector(SV_INT, SFLAG_CONST_TYPE);
    CrawlVector &timer_new = props_new["timer"].get_vector();

    int val;
    while (!age_timer.empty())
    {
        val = age_timer[age_timer.size() - 1];
        age_timer.pop_back();
        timer_new.push_back(val);
    }
    ASSERT(timer_new.size() == count);
    props_new.assert_validity();
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
    vector<int> age_timer;
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
        return; // Nothing to be done.

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
        ASSERT(blood.pos.x >= 0);
        ASSERT(blood.pos.y >= 0);
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
    int o = get_mitm_slot(20);
    if (o == NON_ITEM)
        return;

    item_def &item = mitm[o];
    _init_coagulated_blood(item, coag_count, blood, age_timer);

    if (blood.held_by_monster())
        move_item_to_grid(&o, blood.holding_monster()->pos());
    else
        move_item_to_grid(&o, blood.pos);

    dec_mitm_item_quantity(obj, rot_count + coag_count);
    _compare_blood_quantity(blood, timer.size());
}

static string _get_desc_quantity(const int quant, const int total)
{
    if (total == quant)
        return "Your";
    else if (quant == 1)
        return "One of your";
    else if (quant == 2)
        return "Two of your";
    else if (quant >= total * 3 / 4)
        return "Most of your";
    else
        return "Some of your";
}

// Prints messages for blood potions coagulating or rotting in inventory.
static void _potion_stack_changed_message(item_def &potion, int num_changed,
                                          string verb)
{
    ASSERT(num_changed > 0);

    verb = replace_all(verb, "%s", num_changed == 1 ? "s" : "");
    mprf(MSGCH_ROTTEN_MEAT, "%s %s %s.",
         _get_desc_quantity(num_changed, potion.quantity).c_str(),
         potion.name(DESC_PLAIN, false).c_str(),
         verb.c_str());
}

// Returns true if the total number of potions in inventory decreased,
// in which case burden_change() will need to be called.
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
    vector<int> age_timer;
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
        return false; // Nothing to be done.

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

    if (!coag_count) // Some potions rotted away, but none coagulated.
    {
        // Only coagulated blood can rot.
        ASSERT(blood.sub_type == POT_BLOOD_COAGULATED);
        _potion_stack_changed_message(blood, rot_count, "rot%s away");
        bool destroyed = dec_inv_item_quantity(blood.link, rot_count);

        if (!destroyed)
            _compare_blood_quantity(blood, timer.size());

        return true;
    }

    // Coagulated blood cannot coagulate any further...
    ASSERT(blood.sub_type == POT_BLOOD);

    _potion_stack_changed_message(blood, coag_count, "coagulate%s");

    request_autoinscribe();

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
            if (!dec_inv_item_quantity(blood.link, coag_count + rot_count))
                _compare_blood_quantity(blood, timer.size());

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

            // re-sort timer
            _int_sort(timer2);

            return rot_count > 0;
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

        return rot_count > 0;
    }

    // Else, create new stack in inventory.
    int freeslot = find_free_slot(blood);
    if (freeslot >= 0 && freeslot < ENDOFPACK)
    {
        item_def &item   = you.inv[freeslot];
        item.clear();
        item.link        = freeslot;
        item.slot        = index_to_letter(item.link);
        item.pos.set(-1, -1);
        _init_coagulated_blood(item, coag_count, blood, age_timer);

        blood.quantity -= coag_count + rot_count;
        _compare_blood_quantity(blood, timer.size());

        return rot_count > 0;
    }

    mprf("You can't carry %s right now.", coag_count > 1 ? "them" : "it");

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

            return true;
        }
        o = mitm[o].link;
    }
    // If we got here nothing was found!

    // Create a new stack of potions.
    o = get_mitm_slot();
    if (o == NON_ITEM)
        return false;
    _init_coagulated_blood(mitm[o], coag_count, blood, age_timer);

    move_item_to_grid(&o, you.pos());

    if (!dec_inv_item_quantity(blood.link, coag_count + rot_count))
        _compare_blood_quantity(blood, timer.size());
    return true;
}

// Removes the oldest timer of a stack of blood potions.
// Mostly used for (q)uaff and (f)ire.
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
    return val;
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

    ASSERT_RANGE(quant, 1, source.quantity + 1);
    ASSERT(is_blood_potion(source));
    ASSERT(is_blood_potion(dest));

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
            && mons_has_blood(si->mon_type))
        {
            return true;
        }
    }
    return false;
}

// Deliberately don't check for rottenness here, so this check
// can also be used to verify whether you *could* have bottled
// a now rotten corpse.
bool can_bottle_blood_from_corpse(monster_type mons_class)
{
    if (you.species != SP_VAMPIRE || you.experience_level < 6
        || !mons_has_blood(mons_class))
    {
        return false;
    }

    int chunk_type = mons_corpse_effect(mons_class);
    if (chunk_type == CE_CLEAN || chunk_type == CE_CONTAMINATED)
        return true;

    return false;
}

int num_blood_potions_from_corpse(monster_type mons_class, int chunk_type)
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

    return pot_quantity;
}

// If autopickup is active, the potions are auto-picked up after creation.
void turn_corpse_into_blood_potions(item_def &item)
{
    ASSERT(item.base_type == OBJ_CORPSES);
    ASSERT(!food_is_rotten(item));

    const item_def corpse = item;
    const monster_type mons_class = corpse.mon_type;

    ASSERT(can_bottle_blood_from_corpse(mons_class));

    item.base_type = OBJ_POTIONS;
    item.sub_type  = POT_BLOOD;
    item_colour(item);
    clear_item_pickup_flags(item);

    item.quantity = num_blood_potions_from_corpse(mons_class);

    // Initialise timer depending on corpse age:
    // almost rotting: age = 100 --> potion timer =  500 --> will coagulate soon
    // freshly killed: age = 200 --> potion timer = 2500 --> fresh !blood
    init_stack_blood_potions(item, (item.special - 100) * 20 + 500);

    // Happens after the blood has been bottled.
    maybe_drop_monster_hide(corpse);
}

void turn_corpse_into_skeleton_and_blood_potions(item_def &item)
{
    item_def blood_potions = item;

    if (mons_skeleton(item.mon_type))
        turn_corpse_into_skeleton(item);

    int o = get_mitm_slot();
    if (o != NON_ITEM)
    {
        turn_corpse_into_blood_potions(blood_potions);
        copy_item_to_grid(blood_potions, you.pos());
    }
}

static bool allow_bleeding_on_square(const coord_def& where)
{
    // No bleeding onto sanctuary ground, please.
    // Also not necessary if already covered in blood.
    if (is_bloodcovered(where) || is_sanctuary(where))
        return false;

    // No spattering into lava or water.
    if (grd(where) >= DNGN_LAVA && grd(where) < DNGN_FLOOR)
        return false;

    // No spattering into fountains (other than blood).
    if (grd(where) == DNGN_FOUNTAIN_BLUE
        || grd(where) == DNGN_FOUNTAIN_SPARKLING)
    {
        return false;
    }

    // The good gods like to keep their altars pristine.
    if (is_good_god(feat_altar_god(grd(where))))
        return false;

    return true;
}

bool maybe_bloodify_square(const coord_def& where)
{
    if (!allow_bleeding_on_square(where))
        return false;

    env.pgrid(where) |= FPROP_BLOODY;
    return true;
}

/*
 * Rotate the wall blood splat tile, so that it is facing the source.
 *
 * Wall blood splat tiles are drawned with the blood dripping down. We need
 * the tile to be facing an orthogonal empty space for the effect to look
 * good. We choose the empty space closest to the source of the blood.
 *
 * @param where Coordinates of the wall where there is a blood splat.
 * @param from Coordinates of the source of the blood.
 * @param old_blood blood splats created at level generation are old and can
 * have some blood inscriptions. Only for south facing splats, so you don't
 * have to turn your head to read the inscriptions.
 */
static void _orient_wall_blood(const coord_def& where, coord_def from,
                               bool old_blood)
{
    if (!feat_is_wall(env.grid(where)))
        return;

    if (from == INVALID_COORD)
        from = where;

    coord_def closer = INVALID_COORD;
    int dist = INT_MAX;
    for (orth_adjacent_iterator ai(where); ai; ++ai)
    {
        if (in_bounds(*ai) && !cell_is_solid(*ai)
            && cell_see_cell(from, *ai, LOS_SOLID)
            && (distance2(*ai, from) < dist
                || distance2(*ai, from) == dist && coinflip()))
        {
            closer = *ai;
            dist = distance2(*ai, from);
        }
    }

    // If we didn't find anything, the wall is in a corner.
    // We don't want blood tile there.
    if (closer == INVALID_COORD)
    {
        env.pgrid(where) &= ~FPROP_BLOODY;
        return;
    }

    const coord_def diff = where - closer;
    if (diff == coord_def(1, 0))
        env.pgrid(where) |= FPROP_BLOOD_WEST;
    else if (diff == coord_def(0, 1))
        env.pgrid(where) |= FPROP_BLOOD_NORTH;
    else if (diff == coord_def(-1, 0))
        env.pgrid(where) |= FPROP_BLOOD_EAST;
    else if (old_blood && one_chance_in(10))
        env.pgrid(where) |= FPROP_OLD_BLOOD;
}

static void _maybe_bloodify_square(const coord_def& where, int amount,
                                   bool spatter = false,
                                   bool smell_alert = true,
                                   const coord_def& from = INVALID_COORD,
                                   const bool old_blood = false)
{
    if (amount < 1)
        return;

    bool may_bleed = allow_bleeding_on_square(where);
    if (!spatter && !may_bleed)
        return;

    bool ignite_blood = player_mutation_level(MUT_IGNITE_BLOOD)
                        && you.see_cell(where);

    if (ignite_blood)
        amount *= 2;

    if (x_chance_in_y(amount, 20))
    {
        dprf("might bleed now; square: (%d, %d); amount = %d",
             where.x, where.y, amount);
        if (may_bleed)
        {
            env.pgrid(where) |= FPROP_BLOODY;
            _orient_wall_blood(where, from, old_blood);

            if (smell_alert && in_bounds(where))
                blood_smell(12, where);

            if (ignite_blood
                && !cell_is_solid(where)
                && env.cgrid(where) == EMPTY_CLOUD)
            {
                place_cloud(CLOUD_FIRE, where, 5 + random2(6), &you);
            }
        }

        if (spatter)
        {
            // Smaller chance of spattering surrounding squares.
            for (adjacent_iterator ai(where); ai; ++ai)
            {
                _maybe_bloodify_square(*ai, amount/15, false, true, from,
                                       old_blood);
            }
        }
    }
}

// Currently flavour only: colour ground (and possibly adjacent squares) red.
// "damage" depends on damage taken (or hitpoints, if damage higher),
// or, for sacrifices, on the number of chunks possible to get out of a corpse.
void bleed_onto_floor(const coord_def& where, monster_type montype,
                      int damage, bool spatter, bool smell_alert,
                      const coord_def& from, const bool old_blood)
{
    ASSERT_IN_BOUNDS(where);

    if (montype == MONS_PLAYER && !you.can_bleed())
        return;

    if (montype != NUM_MONSTERS && montype != MONS_PLAYER)
    {
        monster m;
        m.type = montype;
        if (!m.can_bleed())
            return;
    }

    _maybe_bloodify_square(where, damage, spatter, smell_alert, from,
                           old_blood);
}

void blood_spray(const coord_def& origin, monster_type montype, int level)
{
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

            if (in_bounds(bloody) && cell_see_cell(origin, bloody, LOS_SOLID))
            {
                bleed_onto_floor(bloody, montype, 99, false, true, origin);
                break;
            }
        }
    }
}

static void _spatter_neighbours(const coord_def& where, int chance,
                                const coord_def& from = INVALID_COORD)
{
    for (adjacent_iterator ai(where, false); ai; ++ai)
    {
        if (!allow_bleeding_on_square(*ai))
            continue;

        if (one_chance_in(chance))
        {
            env.pgrid(*ai) |= FPROP_BLOODY;
            _orient_wall_blood(where, from, true);
            _spatter_neighbours(*ai, chance+1, from);
        }
    }
}

void generate_random_blood_spatter_on_level(const map_bitmask *susceptible_area)
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

void search_around()
{
    ASSERT(!crawl_state.game_is_arena());

    int base_skill = you.experience_level * 100 / 3;
    int skill = (2/(1+exp(-(base_skill+120)/325.0))-1) * 225
                + (base_skill/200.0) + 15;

    if (you_worship(GOD_ASHENZARI) && !player_under_penance())
        skill += you.piety * 2;

    int farskill = skill;
    if (int mut = you.mutation[MUT_BLURRY_VISION])
        farskill >>= mut;
    // Traps and doors stepdown skill:
    // skill/(2x-1) for squares at distance x
    int max_dist = div_rand_round(farskill, 32);
    if (max_dist > 5)
        max_dist = 5;
    if (max_dist < 1)
        max_dist = 1;

    for (radius_iterator ri(you.pos(), max_dist, C_ROUND, LOS_NO_TRANS); ri; ++ri)
    {
        if (grd(*ri) != DNGN_UNDISCOVERED_TRAP)
            continue;

        int dist = ri->range(you.pos());

        // Own square is not excluded; may be flying.
        // XXX: Currently, flying over a trap will always detect it.

        int effective = (dist <= 1) ? skill : farskill / (dist * 2 - 1);

        trap_def* ptrap = find_trap(*ri);
        if (!ptrap)
        {
            // Maybe we shouldn't kill the trap for debugging
            // purposes - oh well.
            grd(*ri) = DNGN_FLOOR;
            dprf("You found a buggy trap! It vanishes!");
            continue;
        }

        if (effective > ptrap->skill_rnd)
        {
            ptrap->reveal();
            mprf("You found %s trap!",
                 ptrap->name(DESC_A).c_str());
            learned_something_new(HINT_SEEN_TRAP, *ri);
        }
    }
}

void emergency_untransform()
{
    mpr("You quickly transform back into your natural form.");
    untransform(false, true); // We're already entering the water.

    if (you.species == SP_MERFOLK)
        merfolk_start_swimming(false);
}

void merfolk_start_swimming(bool stepped)
{
    if (you.fishtail)
        return;

    if (stepped)
        mpr("Your legs become a tail as you enter the water.");
    else
        mpr("Your legs become a tail as you dive into the water.");

    if (you.invisible())
        mpr("...but don't expect to remain undetected.");

    you.fishtail = true;
    remove_one_equip(EQ_BOOTS);
    you.redraw_evasion = true;

#ifdef USE_TILE
    init_player_doll();
#endif
}

void merfolk_stop_swimming()
{
    if (!you.fishtail)
        return;
    you.fishtail = false;
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
}

string weird_glowing_colour()
{
    return getMiscString("glowing_colour_name");
}

string weird_writing()
{
    return getMiscString("writing_name");
}

string weird_smell()
{
    return getMiscString("smell_name");
}

string weird_sound()
{
    return getMiscString("sound_name");
}

bool scramble(void)
{
    ASSERT(!crawl_state.game_is_arena());

    // Statues are too stiff and heavy to scramble out of the water.
    if (you.form == TRAN_STATUE || you.cannot_move())
        return false;

    const int max_carry = carrying_capacity();
    // When highly encumbered, scrambling out is hard to do.
    return you.burden < (max_carry / 2) + random2(max_carry / 2);
}

bool go_berserk(bool intentional, bool potion)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!you.can_go_berserk(intentional, potion))
        return false;

    if (stasis_blocks_effect(true,
                             true,
                             "%s thrums violently and saps your rage.",
                             3,
                             "%s vibrates violently and saps your rage."))
    {
        return false;
    }

    if (crawl_state.game_is_hints())
        Hints.hints_berserk_counter++;

    mpr("A red film seems to cover your vision as you go berserk!");

    if (you.duration[DUR_FINESSE] > 0)
    {
        you.duration[DUR_FINESSE] = 0; // Totally incompatible.
        mpr("Finesse? Hah! Time to rip out guts!");
    }

    if (you_worship(GOD_CHEIBRIADOS))
    {
        // Chei makes berserk not speed you up.
        // Unintentional would be forgiven "just this once" every time.
        // Intentional could work as normal, but that would require storing
        // whether you transgressed to start it -- so we just consider this
        // a part of your penance.
        if (intentional)
        {
            did_god_conduct(DID_HASTY, 8);
            simple_god_message(" forces you to slow down.");
        }
        else
            simple_god_message(" protects you from inadvertent hurry.");
    }
    else
        mpr("You feel yourself moving faster!");

    mpr("You feel mighty!");

    // Cutting the duration in half since berserk causes haste and hasted
    // actions have half the usual delay. This keeps player turns
    // approximately consistent withe previous versions. -cao
    // Only 1.5 now, but I'm keeping the reduction as a nerf. -1KB
    int berserk_duration = (20 + random2avg(19,2)) / 2;

    you.increase_duration(DUR_BERSERK, berserk_duration);

    calc_hp();
    you.hp = you.hp * 3 / 2;

    deflate_hp(you.hp_max, false);

    if (!you.duration[DUR_MIGHT])
        notify_stat_change(STAT_STR, 5, true, "going berserk");

    if (you.berserk_penalty != NO_BERSERK_PENALTY)
        you.berserk_penalty = 0;

    you.redraw_quiver = true; // Account for no firing.

    if (you.species == SP_LAVA_ORC)
    {
        mpr("You burn with rage!");
        // This will get sqrt'd later, so.
        you.temperature = TEMP_MAX;
    }

    if (player_equip_unrand(UNRAND_JIHAD))
        for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
            if (mi->friendly())
                mi->go_berserk(false);

    return true;
}

// HACK ALERT: In the following several functions, want_move is true if the
// player is travelling. If that is the case, things must be considered one
// square closer to the player, since we don't yet know where the player will
// be next turn.

// Returns true if the monster has a path to the player, or it has to be
// assumed that this is the case.
static bool _mons_has_path_to_player(const monster* mon, bool want_move = false)
{
    if (mon->is_stationary() && !mons_is_tentacle(mon->type))
    {
        int dist = grid_distance(you.pos(), mon->pos());
        if (want_move)
            dist--;
        if (dist >= 2)
            return false;
    }

    // Short-cut if it's already adjacent.
    if (grid_distance(mon->pos(), you.pos()) <= 1)
        return true;

    // If the monster is awake and knows a path towards the player
    // (even though the player cannot know this) treat it as unsafe.
    if (mon->travel_target == MTRAV_PLAYER)
        return true;

    if (mon->travel_target == MTRAV_KNOWN_UNREACHABLE)
        return false;

    // Try to find a path from monster to player, using the map as it's
    // known to the player and assuming unknown terrain to be traversable.
    monster_pathfind mp;
    const int range = mons_tracking_range(mon);
    // Use a large safety margin.  x4 should be ok.
    if (range > 0)
        mp.set_range(range * 4);

    if (mp.init_pathfind(mon, you.pos(), true, false, true))
        return true;

    // Now we know the monster cannot possibly reach the player.
    mon->travel_target = MTRAV_KNOWN_UNREACHABLE;

    return false;
}

bool mons_can_hurt_player(const monster* mon, const bool want_move)
{
    // FIXME: This takes into account whether the player knows the map!
    //        It should, for the purposes of i_feel_safe. [rob]
    // It also always returns true for sleeping monsters, but that's okay
    // for its current purposes. (Travel interruptions and tension.)
    if (_mons_has_path_to_player(mon, want_move))
        return true;

    // Even if the monster can not actually reach the player it might
    // still use some ranged form of attack.
    if (you.see_cell_no_trans(mon->pos())
        && mons_has_known_ranged_attack(mon))
    {
        return true;
    }

    return false;
}

// Returns true if a monster can be considered safe regardless
// of distance.
static bool _mons_is_always_safe(const monster *mon)
{
    return mon->wont_attack()
           || mon->type == MONS_BUTTERFLY
           || mon->withdrawn()
           || mon->type == MONS_BALLISTOMYCETE && mon->number == 0;
}

bool mons_is_safe(const monster* mon, const bool want_move,
                  const bool consider_user_options, bool check_dist)
{
    // Short-circuit plants, some vaults have tons of those.
    // Except for inactive ballistos, players may still want these.
    if (mons_is_firewood(mon) && mon->type != MONS_BALLISTOMYCETE)
        return true;

    int  dist    = grid_distance(you.pos(), mon->pos());

    bool is_safe = (_mons_is_always_safe(mon)
                    || check_dist
                       && (mon->pacified() && dist > 1
                           || crawl_state.disables[DIS_MON_SIGHT] && dist > 2
                           // Only seen through glass walls or within water?
                           // Assuming that there are no water-only/lava-only
                           // monsters capable of throwing or zapping wands.
                           || !mons_can_hurt_player(mon, want_move)));

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

    return is_safe;
}

// Return all nearby monsters in range (default: LOS) that the player
// is able to recognise as being monsters (i.e. no submerged creatures.)
//
// want_move       (??) Somehow affects what monsters are considered dangerous
// just_check      Return zero or one monsters only
// dangerous_only  Return only "dangerous" monsters
// require_visible Require that monsters be visible to the player
// range           search radius (defaults: LOS)
//
vector<monster* > get_nearby_monsters(bool want_move,
                                      bool just_check,
                                      bool dangerous_only,
                                      bool consider_user_options,
                                      bool require_visible,
                                      bool check_dist,
                                      int range)
{
    ASSERT(!crawl_state.game_is_arena());

    if (range == -1)
        range = LOS_RADIUS;

    vector<monster* > mons;

    // Sweep every visible square within range.
    for (radius_iterator ri(you.pos(), range, C_ROUND, LOS_DEFAULT); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            if (mon->alive()
                && (!require_visible || mon->visible_to(&you))
                && !mon->submerged()
                && (!dangerous_only || !mons_is_safe(mon, want_move,
                                                     consider_user_options,
                                                     check_dist)))
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
    const int radius = want_move ? 8 : 2;
    for (radius_iterator ri(you.pos(), radius, C_CIRCLE, LOS_DEFAULT); ri; ++ri)
        if (env.map_knowledge(*ri).flags & MAP_INVISIBLE_MONSTER)
            return true;
    return false;
}

bool i_feel_safe(bool announce, bool want_move, bool just_monsters,
                 bool check_dist, int range)
{
    if (!just_monsters)
    {
        // check clouds
        if (in_bounds(you.pos()) && env.cgrid(you.pos()) != EMPTY_CLOUD)
        {
            const int cloudidx = env.cgrid(you.pos());
            const cloud_type type = env.cloud[cloudidx].type;

            // Temporary immunity allows travelling through a cloud but not
            // resting in it.
            if (is_damaging_cloud(type, want_move))
            {
                if (announce)
                {
                    mprf(MSGCH_WARN, "You're standing in a cloud of %s!",
                         cloud_name_at_index(cloudidx).c_str());
                }
                return false;
            }
        }

        // No monster will attack you inside a sanctuary,
        // so presence of monsters won't matter -- until it starts shrinking...
        if (is_sanctuary(you.pos()) && env.sanctuary_time >= 5)
            return true;
    }

    // Monster check.
    vector<monster* > visible =
        get_nearby_monsters(want_move, !announce, true, true, true,
                            check_dist, range);

    // Announce the presence of monsters (Eidolos).
    string msg;
    if (visible.size() == 1)
    {
        const monster& m = *visible[0];
        const string monname = mons_is_mimic(m.type) ? "A mimic"
                                                     : m.name(DESC_A);
        msg = make_stringf("%s is nearby!", monname.c_str());
    }
    else if (visible.size() > 1)
        msg = "There are monsters nearby!";
    else if (_exposed_monsters_nearby(want_move))
        msg = "There is a strange disturbance nearby!";
    else
        return true;

    if (announce)
        mprf(MSGCH_WARN, "%s", msg.c_str());

    return false;
}

bool there_are_monsters_nearby(bool dangerous_only, bool require_visible,
                               bool consider_user_options)
{
    return !get_nearby_monsters(false, true, dangerous_only,
                                consider_user_options,
                                require_visible).empty();
}

static const char *shop_types[] =
{
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
    "general",
    "gadget"
};

int str_to_shoptype(const string &s)
{
    if (s == "random" || s == "any")
        return SHOP_RANDOM;

    for (unsigned i = 0; i < ARRAYSZ(shop_types); ++i)
    {
        if (s == shop_types[i])
            return i;
    }
    return -1;
}

void list_shop_types()
{
    mpr_nojoin(MSGCH_PLAIN, "Available shop types: ");
    for (unsigned i = 0; i < ARRAYSZ(shop_types); ++i)
        mprf_nocap("%s", shop_types[i]);
}

// General threat = sum_of_logexpervalues_of_nearby_unfriendly_monsters.
// Highest threat = highest_logexpervalue_of_nearby_unfriendly_monsters.
static void monster_threat_values(double *general, double *highest,
                                  bool *invis)
{
    *invis = false;

    double sum = 0;
    int highest_xp = -1;

    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (mi->friendly())
            continue;

        const int xp = exper_value(*mi);
        const double log_xp = log((double)xp);
        sum += log_xp;
        if (xp > highest_xp)
        {
            highest_xp = xp;
            *highest   = log_xp;
        }
        if (!mi->visible_to(&you))
            *invis = true;
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

    return gen_threat > logexp * 1.3 || hi_threat > logexp / 2;
}

static void _drop_tomb(const coord_def& pos, bool premature, bool zin)
{
    int count = 0;
    monster* mon = monster_at(pos);

    // Don't wander on duty!
    if (mon)
        mon->behaviour = BEH_SEEK;

    bool seen_change = false;
    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        // "Normal" tomb (card or monster spell)
        if (!zin && revert_terrain_change(*ai, TERRAIN_CHANGE_TOMB))
        {
            count++;
            if (you.see_cell(*ai))
                seen_change = true;
        }
        // Zin's Imprison.
        else if (zin && revert_terrain_change(*ai, TERRAIN_CHANGE_IMPRISON))
        {
            vector<map_marker*> markers = env.markers.get_markers_at(*ai);
            for (int i = 0, size = markers.size(); i < size; ++i)
            {
                map_marker *mark = markers[i];
                if (mark->property("feature_description")
                    == "a gleaming silver wall")
                {
                    env.markers.remove(mark);
                }
            }

            env.grid_colours(*ai) = 0;
            tile_clear_flavour(*ai);
            tile_init_flavour(*ai);
            count++;
            if (you.see_cell(*ai))
                seen_change = true;
        }
    }

    if (count)
    {
        if (seen_change && !zin)
            mprf("The walls disappear%s!", premature ? " prematurely" : "");
        else if (seen_change && zin)
        {
            mprf("Zin %s %s %s.",
                 (mon) ? "releases"
                       : "dismisses",
                 (mon) ? mon->name(DESC_THE).c_str()
                       : "the silver walls,",
                 (mon) ? make_stringf("from %s prison",
                             mon->pronoun(PRONOUN_POSSESSIVE).c_str()).c_str()
                       : "but there is nothing inside them");
        }
        else
        {
            if (!silenced(you.pos()))
                mprf(MSGCH_SOUND, "You hear a deep rumble.");
            else
                mpr("You feel the ground shudder.");
        }
    }
}

static vector<map_malign_gateway_marker*> _get_malign_gateways()
{
    vector<map_malign_gateway_marker*> mm_markers;

    vector<map_marker*> markers = env.markers.get_all(MAT_MALIGN);
    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_marker *mark = markers[i];
        if (mark->get_type() != MAT_MALIGN)
            continue;

        map_malign_gateway_marker *mmark = dynamic_cast<map_malign_gateway_marker*>(mark);

        mm_markers.push_back(mmark);
    }

    return mm_markers;
}

int count_malign_gateways()
{
    return _get_malign_gateways().size();
}

void timeout_malign_gateways(int duration)
{
    // Passing 0 should allow us to just touch the gateway and see
    // if it should decay. This, in theory, should resolve the one
    // turn delay between it timing out and being recastable. -due
    vector<map_malign_gateway_marker*> markers = _get_malign_gateways();

    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_malign_gateway_marker *mmark = markers[i];

        if (duration)
            mmark->duration -= duration;

        if (mmark->duration > 0)
            big_cloud(CLOUD_TLOC_ENERGY, 0, mmark->pos, 3+random2(10), 2+random2(5));
        else
        {
            monster* mons = monster_at(mmark->pos);
            if (mmark->monster_summoned && !mons)
            {
                // The marker hangs around until later.
                if (env.grid(mmark->pos) == DNGN_MALIGN_GATEWAY)
                    env.grid(mmark->pos) = DNGN_FLOOR;

                env.markers.remove(mmark);
            }
            else if (!mmark->monster_summoned && !mons)
            {
                bool is_player = mmark->is_player;
                actor* caster = 0;
                if (is_player)
                    caster = &you;

                mgen_data mg = mgen_data(MONS_ELDRITCH_TENTACLE,
                                         mmark->behaviour,
                                         caster,
                                         0,
                                         0,
                                         mmark->pos,
                                         MHITNOT,
                                         MG_FORCE_PLACE,
                                         mmark->god);
                if (!is_player)
                    mg.non_actor_summoner = mmark->summoner_string;

                if (monster *tentacle = create_monster(mg))
                {
                    tentacle->flags |= MF_NO_REWARD;
                    tentacle->add_ench(ENCH_PORTAL_TIMER);
                    mon_enchant kduration = mon_enchant(ENCH_PORTAL_PACIFIED, 4,
                        caster, (random2avg(mmark->power, 6)-random2(4))*10);
                    tentacle->props["base_position"].get_coord()
                                        = tentacle->pos();
                    tentacle->add_ench(kduration);

                    mmark->monster_summoned = true;
                }
            }
        }
    }
}

void timeout_tombs(int duration)
{
    if (!duration)
        return;

    vector<map_marker*> markers = env.markers.get_all(MAT_TOMB);

    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_marker *mark = markers[i];
        if (mark->get_type() != MAT_TOMB)
            continue;

        map_tomb_marker *cmark = dynamic_cast<map_tomb_marker*>(mark);
        cmark->duration -= duration;

        // Empty tombs disappear early.
        monster* mon_entombed = monster_at(cmark->pos);
        bool empty_tomb = !(mon_entombed || you.pos() == cmark->pos);
        bool zin = (cmark->source == -GOD_ZIN);

        if (cmark->duration <= 0 || empty_tomb)
        {
            _drop_tomb(cmark->pos, empty_tomb, zin);

            monster* mon_src =
                !invalid_monster_index(cmark->source) ? &menv[cmark->source]
                                                      : NULL;
            // A monster's Tomb of Doroklohe spell.
            if (mon_src
                && mon_src == mon_entombed)
            {
                mon_src->lose_energy(EUT_SPELL);
            }

            env.markers.remove(cmark);
        }
    }
}

void timeout_terrain_changes(int duration, bool force)
{
    if (!duration && !force)
        return;

    int num_seen[NUM_TERRAIN_CHANGE_TYPES] = {0};

    vector<map_marker*> markers = env.markers.get_all(MAT_TERRAIN_CHANGE);

    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_terrain_change_marker *marker =
                dynamic_cast<map_terrain_change_marker*>(markers[i]);

        if (marker->duration != INFINITE_DURATION)
            marker->duration -= duration;

        monster* mon_src = monster_by_mid(marker->mon_num);
        if (marker->duration <= 0
            || (marker->mon_num != 0
                && (!mon_src || !mon_src->alive() || mon_src->pacified())))
        {
            if (you.see_cell(marker->pos))
                num_seen[marker->change_type]++;
            revert_terrain_change(marker->pos, marker->change_type);
        }
    }

    if (num_seen[TERRAIN_CHANGE_DOOR_SEAL] > 1)
        mpr("The runic seals fade away.");
    else if (num_seen[TERRAIN_CHANGE_DOOR_SEAL] > 0)
        mpr("The runic seal fades away.");
}

void bring_to_safety()
{
    if (player_in_branch(BRANCH_ABYSS))
        return abyss_teleport(true);

    if (crawl_state.game_is_zotdef() && !orb_position().origin())
    {
        // In ZotDef, it's not the safety of your sorry butt that matters.
        for (distance_iterator di(env.orb_pos, true, false); di; ++di)
            if (!monster_at(*di)
                && !(env.pgrid(*di) & FPROP_NO_TELE_INTO))
            {
                you.moveto(*di);
                return;
            }
    }

    coord_def best_pos, pos;
    double min_threat = 1e38;
    int tries = 0;

    // Up to 100 valid spots, but don't lock up when there's none.  This can happen
    // on tiny Zig/portal rooms with a bad summon storm and you in cloud / over water.
    while (tries < 100000 && min_threat > 0)
    {
        pos.x = random2(GXM);
        pos.y = random2(GYM);
        if (!in_bounds(pos)
            || grd(pos) != DNGN_FLOOR
            || env.cgrid(pos) != EMPTY_CLOUD
            || monster_at(pos)
            || env.pgrid(pos) & FPROP_NO_TELE_INTO
            || crawl_state.game_is_sprint()
               && distance2(pos, you.pos()) > dist_range(10))
        {
            tries++;
            continue;
        }

        for (adjacent_iterator ai(pos); ai; ++ai)
            if (grd(*ai) == DNGN_SLIMY_WALL)
            {
                tries++;
                continue;
            }

        bool junk;
        double gen_threat = 0.0, hi_threat = 0.0;
        monster_threat_values(&gen_threat, &hi_threat, &junk);
        const double threat = gen_threat * hi_threat;

        if (threat < min_threat)
        {
            best_pos = pos;
            min_threat = threat;
        }
        tries += 1000;
    }

    if (min_threat != 1e38)
        you.moveto(best_pos);
}

// This includes ALL afflictions, unlike wizard/Xom revive.
void revive()
{
    adjust_level(-1);
    // Allow a spare after two levels (we just lost one); the exact value
    // doesn't matter here.
    you.attribute[ATTR_LIFE_GAINED] = 0;

    you.disease = 0;
    you.magic_contamination = 0;
    set_hunger(HUNGER_DEFAULT, true);
    restore_stat(STAT_ALL, 0, true);
    you.rotting = 0;

    you.attribute[ATTR_DELAYED_FIREBALL] = 0;
    clear_trapping_net();
    you.attribute[ATTR_DIVINE_VIGOUR] = 0;
    you.attribute[ATTR_DIVINE_STAMINA] = 0;
    you.attribute[ATTR_DIVINE_SHIELD] = 0;
    if (you.duration[DUR_WEAPON_BRAND])
        set_item_ego_type(*you.weapon(), OBJ_WEAPONS, SPWPN_NORMAL);
    if (you.form)
        untransform();
    you.clear_beholders();
    you.clear_fearmongers();
    you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = 0;
    you.attribute[ATTR_INVIS_UNCANCELLABLE] = 0;
    you.attribute[ATTR_FLIGHT_UNCANCELLABLE] = 0;
    you.attribute[ATTR_XP_DRAIN] = 0;
    if (you.duration[DUR_SCRYING])
        you.xray_vision = false;

    for (int dur = 0; dur < NUM_DURATIONS; dur++)
        if (dur != DUR_GOURMAND && dur != DUR_PIETY_POOL)
            you.duration[dur] = 0;

    // Stat death that wasn't cleared might be:
    // * permanent (focus card): our fix is spot on
    // * long-term (mutation): we induce some penalty, ok
    // * short-term (-stat item): could be done better...
    unfocus_stats();
    you.stat_zero.init(0);

    unrot_hp(9999);
    set_hp(9999);
    set_mp(9999);
    you.dead = false;

    // Remove silence.
    invalidate_agrid();

    if (you.hp_max <= 0)
    {
        you.lives = 0;
        mpr("You are too frail to live.");
        // possible only with an extreme abuse of Borgnjor's
        ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_DRAINING);
    }

    mpr("You rejoin the land of the living...");
    more();

    burden_change();
}

////////////////////////////////////////////////////////////////////////////
// Living breathing dungeon stuff.
//

static vector<coord_def> sfx_seeds;

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
                        && player_in_branch(BRANCH_SWAMP)))
            {
                const coord_def c(x, y);
                sfx_seeds.push_back(c);
            }
        }
    }
    dprf("%u environment effect seeds", (unsigned int)sfx_seeds.size());
}

static void apply_environment_effect(const coord_def &c)
{
    const dungeon_feature_type grid = grd(c);
    // Don't apply if if the feature doesn't want it.
    if (testbits(env.pgrid(c), FPROP_NO_CLOUD_GEN))
        return;
    if (grid == DNGN_LAVA)
        check_place_cloud(CLOUD_BLACK_SMOKE, c, random_range(4, 8), 0);
    else if (one_chance_in(3) && grid == DNGN_SHALLOW_WATER)
        check_place_cloud(CLOUD_MIST,        c, random_range(2, 5), 0);
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
        int nsels = div_rand_round(sfx_seeds.size() * sfx_chance, 100);
        if (one_chance_in(5))
            nsels += random2(nsels * 3);

        for (int i = 0; i < nsels; ++i)
            apply_environment_effect(sfx_seeds[ random2(nseeds) ]);
    }
    else
    {
        for (int i = 0; i < nseeds; ++i)
        {
            if (random2(100) >= sfx_chance)
                continue;

            apply_environment_effect(sfx_seeds[i]);
        }
    }

    run_corruption_effects(you.time_taken);
    shoals_apply_tides(div_rand_round(you.time_taken, BASELINE_DELAY),
                       false, true);
    timeout_tombs(you.time_taken);
    timeout_malign_gateways(you.time_taken);
    timeout_phoenix_markers(you.time_taken);
    timeout_terrain_changes(you.time_taken);
    run_cloud_spreaders(you.time_taken);
}

coord_def pick_adjacent_free_square(const coord_def& p)
{
    int num_ok = 0;
    coord_def result(-1, -1);

    for (adjacent_iterator ai(p); ai; ++ai)
    {
        if (grd(*ai) == DNGN_FLOOR && monster_at(*ai) == NULL
            && one_chance_in(++num_ok))
        {
            result = *ai;
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

bool bad_attack(const monster *mon, string& adj, string& suffix,
                coord_def attack_pos, bool check_landing_only)
{
    ASSERT(!crawl_state.game_is_arena());
    bool bad_landing = false;

    if (!you.can_see(mon))
        return false;

    if (attack_pos == coord_def(0, 0))
        attack_pos = you.pos();

    adj.clear();
    suffix.clear();

    if (!check_landing_only
        && (is_sanctuary(mon->pos()) || is_sanctuary(attack_pos)))
    {
        suffix = ", despite your sanctuary";
    }
    else if (check_landing_only && is_sanctuary(attack_pos))
    {
        suffix = ", when you might land in your sanctuary";
        bad_landing = true;
    }
    if (check_landing_only)
        return bad_landing;

    if (mon->friendly())
    {
        if (you_worship(GOD_OKAWARU))
        {
            adj = "your ally ";

            monster_info mi(mon, MILEV_NAME);
            if (!mi.is(MB_NAME_UNQUALIFIED))
                adj += "the ";
        }
        else
            adj = "your ";
        return true;
    }

    if (is_unchivalric_attack(&you, mon)
        && you_worship(GOD_SHINING_ONE)
        && !tso_unchivalric_attack_safe_monster(mon))
    {
        adj += "helpless ";
    }
    if (mon->neutral() && is_good_god(you.religion))
        adj += "neutral ";
    else if (mon->wont_attack())
        adj += "non-hostile ";

    if (you_worship(GOD_JIYVA) && mons_is_slime(mon)
        && !(mon->is_shapeshifter() && (mon->flags & MF_KNOWN_SHIFTER)))
    {
        return true;
    }
    return !adj.empty() || !suffix.empty();
}

bool stop_attack_prompt(const monster* mon, bool beam_attack,
                        coord_def beam_target, bool autohit_first,
                        bool *prompted, coord_def attack_pos,
                        bool check_landing_only)
{
    if (prompted)
        *prompted = false;

    if (crawl_state.disables[DIS_CONFIRMATIONS])
        return false;

    if (you.confused() || !you.can_see(mon))
        return false;

    string adj, suffix;
    if (!bad_attack(mon, adj, suffix, attack_pos, check_landing_only))
        return false;

    // Listed in the form: "your rat", "Blork the orc".
    string mon_name = mon->name(DESC_PLAIN);
    if (!mon_name.find("the ")) // no "your the royal jelly" nor "the the RJ"
        mon_name.erase(0, 4);
    if (adj.find("your"))
        adj = "the " + adj;
    mon_name = adj + mon_name;
    string verb;
    if (beam_attack)
    {
        verb = "fire ";
        if (beam_target == mon->pos())
            verb += "at ";
        else if (you.pos() < beam_target && beam_target < mon->pos()
                 || you.pos() > beam_target && beam_target > mon->pos())
        {
            if (autohit_first)
                return false;

            verb += "in " + apostrophise(mon_name) + " direction";
            mon_name = "";
        }
        else
            verb += "through ";
    }
    else
        verb = "attack ";

    snprintf(info, INFO_SIZE, "Really %s%s%s?",
             verb.c_str(), mon_name.c_str(), suffix.c_str());

    if (prompted)
        *prompted = true;

    if (yesno(info, false, 'n'))
        return false;
    else
    {
        canned_msg(MSG_OK);
        return true;
    }
}

bool stop_attack_prompt(targetter &hitfunc, string verb,
                        bool (*affects)(const actor *victim), bool *prompted)
{
    if (crawl_state.disables[DIS_CONFIRMATIONS])
        return false;

    if (crawl_state.which_god_acting() == GOD_XOM)
        return false;

    if (you.confused())
        return false;

    string adj, suffix;
    counted_monster_list victims;
    for (distance_iterator di(hitfunc.origin, false, true, LOS_RADIUS); di; ++di)
    {
        if (hitfunc.is_affected(*di) <= AFF_NO)
            continue;
        const monster* mon = monster_at(*di);
        if (!mon || !you.can_see(mon))
            continue;
        if (affects && !affects(mon))
            continue;
        string adjn, suffixn;
        if (bad_attack(mon, adjn, suffixn))
        {
            if (victims.empty()) // record the adjectives for the first listed
                adj = adjn, suffix = suffixn;
            victims.add(mon);
        }
    }

    if (victims.empty())
        return false;

    // Listed in the form: "your rat", "Blork the orc".
    string mon_name = victims.describe(DESC_PLAIN);
    if (!mon_name.find("the ")) // no "your the royal jelly" nor "the the RJ"
        mon_name.erase(0, 4);
    if (adj.find("your"))
        adj = "the " + adj;
    mon_name = adj + mon_name;

    snprintf(info, INFO_SIZE, "Really %s %s%s?",
             verb.c_str(), mon_name.c_str(), suffix.c_str());

    if (prompted)
        *prompted = true;
    if (yesno(info, false, 'n'))
        return false;
    else
    {
        canned_msg(MSG_OK);
        return true;
    }
}

bool is_dragonkind(const actor *act)
{
    if (mons_genus(act->mons_species()) == MONS_DRAGON
        || mons_genus(act->mons_species()) == MONS_DRAKE
        || mons_genus(act->mons_species()) == MONS_DRACONIAN)
    {
        return true;
    }

    if (act->is_player())
        return you.form == TRAN_DRAGON;

    // Else the actor is a monster.
    const monster* mon = act->as_monster();

    if (mons_is_zombified(mon)
        && (mons_genus(mon->base_monster) == MONS_DRAGON
            || mons_genus(mon->base_monster) == MONS_DRAKE
            || mons_genus(mon->base_monster) == MONS_DRACONIAN))
    {
        return true;
    }

    if (mons_is_ghost_demon(mon->type)
        && species_genus(mon->ghost->species) == GENPC_DRACONIAN)
    {
        return true;
    }

    return false;
}

// Make the player swap positions with a given monster.
void swap_with_monster(monster* mon_to_swap)
{
    monster& mon(*mon_to_swap);
    ASSERT(mon.alive());
    const coord_def newpos = mon.pos();

    if (stasis_blocks_effect(true, true, "%s emits a piercing whistle.",
                             20, "%s makes your neck tingle."))
    {
        return;
    }

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

    mprf("You swap places with %s.", mon.name(DESC_THE).c_str());

    // Pick the monster up.
    mgrd(newpos) = NON_MONSTER;
    mon.moveto(you.pos());

    // Plunk it down.
    mgrd(mon.pos()) = mon_to_swap->mindex();

    if (you_caught)
    {
        check_net_will_hold_monster(&mon);
        if (!mon_caught)
        {
            you.attribute[ATTR_HELD] = 0;
            you.redraw_quiver = true;
            you.redraw_evasion = true;
        }
    }

    // Move you to its previous location.
    move_player_to_grid(newpos, false, true);

    if (mon_caught)
    {
        if (you.body_size(PSIZE_BODY) >= SIZE_GIANT)
        {
            int net = get_trapping_net(you.pos());
            if (net != NON_ITEM)
                destroy_item(net);
            mprf("The %s rips apart!", (net == NON_ITEM) ? "web" : "net");
            you.attribute[ATTR_HELD] = 0;
            you.redraw_quiver = true;
            you.redraw_evasion = true;
        }
        else
        {
            you.attribute[ATTR_HELD] = 10;
            if (get_trapping_net(you.pos()) != NON_ITEM)
                mpr("You become entangled in the net!");
            else
                mpr("You get stuck in the web!");
            you.redraw_quiver = true; // Account for being in a net.
            // Xom thinks this is hilarious if you trap yourself this way.
            if (you_caught)
                xom_is_stimulated(12);
            else
                xom_is_stimulated(200);
        }

        if (!you_caught)
            mon.del_ench(ENCH_HELD, true);
    }
}

/**
 * Identify a worn piece of jewellery's type.
 */
void wear_id_type(item_def &item)
{
    if (item_type_known(item))
        return;
    set_ident_type(item.base_type, item.sub_type, ID_KNOWN_TYPE);
    set_ident_flags(item, ISFLAG_KNOW_TYPE);
    mprf("You are wearing: %s",
         item.name(DESC_INVENTORY_EQUIP).c_str());
}

static void _maybe_id_jewel(jewellery_type ring_type = NUM_JEWELLERY,
                            jewellery_type amulet_type = NUM_JEWELLERY,
                            artefact_prop_type artp = ARTP_NUM_PROPERTIES)
{
    for (int i = EQ_LEFT_RING; i < NUM_EQUIP; ++i)
    {
        if (player_wearing_slot(i))
        {
            item_def& item = you.inv[you.equip[i]];
            if (item.sub_type == ring_type || item.sub_type == amulet_type)
                wear_id_type(item);
            bool known;
            if (artp != ARTP_NUM_PROPERTIES && is_artefact(item)
                && artefact_wpn_property(item, artp, known)
                && !known)
            {
                artefact_wpn_learn_prop(item, artp);
                mprf("You are wearing: %s",
                     item.name(DESC_INVENTORY_EQUIP).c_str());
            }
        }
    }
}

void maybe_id_ring_hunger()
{
    _maybe_id_jewel(RING_HUNGER, NUM_JEWELLERY, ARTP_METABOLISM);
    _maybe_id_jewel(RING_SUSTENANCE, NUM_JEWELLERY, ARTP_METABOLISM);
}

void maybe_id_ring_see_invis()
{
    _maybe_id_jewel(RING_SEE_INVISIBLE, NUM_JEWELLERY, ARTP_EYESIGHT);
}

void maybe_id_clarity()
{
    _maybe_id_jewel(NUM_JEWELLERY, AMU_CLARITY, ARTP_CLARITY);
}

void maybe_id_resist(beam_type flavour)
{
    switch (flavour)
    {
    case BEAM_FIRE:
    case BEAM_LAVA:
        _maybe_id_jewel(RING_PROTECTION_FROM_FIRE, NUM_JEWELLERY, ARTP_FIRE);
        break;

    case BEAM_COLD:
    case BEAM_ICE:
        _maybe_id_jewel(RING_PROTECTION_FROM_COLD, NUM_JEWELLERY, ARTP_COLD);
        break;

    case BEAM_ELECTRICITY:
        _maybe_id_jewel(NUM_JEWELLERY, NUM_JEWELLERY, ARTP_ELECTRICITY);
        break;

    case BEAM_ACID:
        _maybe_id_jewel(NUM_JEWELLERY, AMU_RESIST_CORROSION);
        break;

    case BEAM_POISON:
    case BEAM_POISON_ARROW:
    case BEAM_MEPHITIC:
        _maybe_id_jewel(RING_POISON_RESISTANCE, NUM_JEWELLERY, ARTP_POISON);
        break;

    case BEAM_NEG:
        _maybe_id_jewel(RING_LIFE_PROTECTION, AMU_WARDING, ARTP_NEGATIVE_ENERGY);
        break;

    case BEAM_STEAM:
        // rF+ grants rSteam, all possibly unidentified sources of rSteam are rF
        _maybe_id_jewel(RING_PROTECTION_FROM_FIRE, NUM_JEWELLERY, ARTP_FIRE);
        break;

    case BEAM_MALMUTATE:
        _maybe_id_jewel(NUM_JEWELLERY, AMU_RESIST_MUTATION);
        break;

    default: ;
    }
}

void auto_id_inventory()
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def& item = you.inv[i];
        if (item.defined())
            god_id_item(item, false);
    }
}

// Reduce damage by AC.
// In most cases, we want AC to mostly stop weak attacks completely but affect
// strong ones less, but the regular formula is too hard to apply well to cases
// when damage is spread into many small chunks.
//
// Every point of damage is processed independently.  Every point of AC has
// an independent 1/81 chance of blocking that damage.
//
// AC 20 stops 22% of damage, AC 40 -- 39%, AC 80 -- 63%.
int apply_chunked_AC(int dam, int ac)
{
    double chance = pow(80.0/81, ac);
    uint64_t cr = chance * (((uint64_t)1) << 32);

    int hurt = 0;
    for (int i = 0; i < dam; i++)
        if (random_int() < cr)
            hurt++;

    return hurt;
}

void entered_malign_portal(actor* act)
{
    if (you.can_see(act))
    {
        mprf("The portal repels %s, its terrible forces doing untold damage!",
             act->is_player() ? "you" : act->name(DESC_THE).c_str());
    }

    act->blink(false);
    if (act->is_player())
        ouch(roll_dice(2, 4), NON_MONSTER, KILLED_BY_WILD_MAGIC, "a malign gateway");
    else
        act->hurt(NULL, roll_dice(2, 4));
}

void handle_real_time(time_t t)
{
    you.real_time += min<time_t>(t - you.last_keypress_time, IDLE_TIME_CLAMP);
    you.last_keypress_time = t;
}

string part_stack_string(const int num, const int total)
{
    if (num == total)
        return "Your";

    string ret  = uppercase_first(number_in_words(num));
           ret += " of your";

    return ret;
}

unsigned int breakpoint_rank(int val, const int breakpoints[],
                             unsigned int num_breakpoints)
{
    unsigned int result = 0;
    while (result < num_breakpoints && val >= breakpoints[result])
        ++result;

    return result;
}

void counted_monster_list::add(const monster* mons)
{
    const string name = mons->name(DESC_PLAIN);
    for (counted_list::iterator i = list.begin(); i != list.end(); ++i)
    {
        if (i->first->name(DESC_PLAIN) == name)
        {
            i->second++;
            return;
        }
    }
    list.push_back(counted_monster(mons, 1));
}

int counted_monster_list::count()
{
    int nmons = 0;
    for (counted_list::const_iterator i = list.begin(); i != list.end(); ++i)
        nmons += i->second;
    return nmons;
}

string counted_monster_list::describe(description_level_type desc)
{
    string out;

    for (counted_list::const_iterator i = list.begin(); i != list.end();)
    {
        const counted_monster &cm(*i);
        if (i != list.begin())
        {
            ++i;
            out += (i == list.end() ? " and " : ", ");
        }
        else
            ++i;

        out += cm.second > 1 ? pluralise(cm.first->name(desc))
                             : cm.first->name(desc);
    }
    return out;
}



bool move_stairs(coord_def orig, coord_def dest)
{
    const dungeon_feature_type stair_feat = grd(orig);

    if (feat_stair_direction(stair_feat) == CMD_NO_CMD)
        return false;

    // The player can't use shops to escape, so don't bother.
    if (stair_feat == DNGN_ENTER_SHOP)
        return false;

    // Don't move around notable terrain the player is aware of if it's
    // out of sight.
    if (is_notable_terrain(stair_feat)
        && env.map_knowledge(orig).known() && !you.see_cell(orig))
    {
        return false;
    }

    return slide_feature_over(orig, dest);
}
