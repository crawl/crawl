/**
 * @file
 * @brief Functions for blood, chunk, & corpse rot.
 **/

#include "AppHdr.h"

#include "rot.h"

#include "areas.h"
#include "butcher.h"
#include "delay.h"
#include "enum.h"
#include "env.h"
#include "hints.h"
#include "itemprop.h"
#include "items.h"
#include "makeitem.h"
#include "misc.h"
#include "mon-death.h"
#include "player.h"
#include "player-equip.h"
#include "prompt.h"
#include "religion.h"
#include "shopping.h"
#include "stringutil.h"


#define TIMER_KEY "timer"


/**
 * Check whether a food item changes over time. (Corpses, chunks, and blood.)
 *
 * @param item      The item to check.
 * @return          Whether the item changes (rots) over time.
 */
static bool _food_item_needs_time_check(item_def &item)
{
    if (!item.defined())
        return false;

    if (item.base_type != OBJ_CORPSES
        && item.base_type != OBJ_FOOD
        && item.base_type != OBJ_POTIONS)
    {
        return false;
    }

    if (item.base_type == OBJ_CORPSES
        && item.sub_type > CORPSE_SKELETON)
    {
        return false;
    }

    if (item.base_type == OBJ_FOOD && item.sub_type != FOOD_CHUNK)
        return false;

    if (item.base_type == OBJ_POTIONS && !is_blood_potion(item))
        return false;

    // The object specifically asks not to be checked:
    if (item.props.exists(CORPSE_NEVER_DECAYS))
        return false;

    return true;
}

/**
 * Decay Gozag-created gold auras.
 *
 * @param it        The stack of gold to decay the aura of.
 * @param rot_time  The length of time to decay the aura for.
 */
static void _rot_floor_gold(item_def &it, int rot_time)
{
    const bool old_aura = it.special > 0;
    it.special = max(0, it.special - rot_time);
    if (old_aura && !it.special)
    {
        invalidate_agrid(true);
        you.redraw_armour_class = true;
        you.redraw_evasion = true;
    }
}

/**
 * Decay items on the floor: corpses, chunks, and Gozag gold auras.
 *
 * @param elapsedTime   The amount of time to rot the corpses for.
 */
void rot_floor_items(int elapsedTime)
{
    if (elapsedTime <= 0)
        return;

    const int rot_time = elapsedTime / 20;

    for (int c = 0; c < MAX_ITEMS; ++c)
    {
        item_def &it = mitm[c];

        if (is_shop_item(it))
            continue;

        if (you_worship(GOD_GOZAG) && it.base_type == OBJ_GOLD)
        {
            _rot_floor_gold(it, rot_time);
            continue;
        }

        if (!_food_item_needs_time_check(it))
            continue;

        if (it.base_type == OBJ_POTIONS)
        {
            maybe_coagulate_blood_potions_floor(c);
            continue;
        }

        if (rot_time >= it.special && !is_being_butchered(it))
        {
            if (it.base_type == OBJ_FOOD)
                destroy_item(c);
            else
            {
                if (it.sub_type == CORPSE_SKELETON
                    || !mons_skeleton(it.mon_type))
                {
                    item_was_destroyed(it);
                    destroy_item(c);
                }
                else
                    turn_corpse_into_skeleton(it);
            }
        }
        else
            it.special -= rot_time;
    }
}

/**
 * Rot chunks & blood in the player's inventory.
 *
 * @param time_delta    The amount of time to rot for.
 */
void rot_inventory_food(int time_delta)
{
    vector<char> rotten_items;

    int num_chunks         = 0;
    int num_chunks_gone    = 0;

    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def &item(you.inv[i]);

        if (item.quantity < 1)
            continue;

        if (!_food_item_needs_time_check(item))
            continue;

        if (item.base_type == OBJ_POTIONS)
        {
            maybe_coagulate_blood_potions_inv(item);
            continue;
        }

#if TAG_MAJOR_VERSION == 34
        if (item.base_type != OBJ_FOOD)
            continue; // old corpses & skeletons

        ASSERT(item.sub_type == FOOD_CHUNK);
#else
        ASSERT(item.base_type == OBJ_FOOD && item.sub_type == FOOD_CHUNK);
#endif

        num_chunks++;

        // Food item timed out -> make it disappear.
        if ((time_delta / 20) >= item.special)
        {
            if (you.equip[EQ_WEAPON] == i)
                unwield_item();

            item_was_destroyed(item);
            destroy_item(item);
            num_chunks_gone++;

            continue;
        }

        // If it hasn't disappeared, reduce the rotting timer.
        item.special -= (time_delta / 20);

        if (food_is_rotten(item)
            && (item.special + (time_delta / 20) > ROTTING_CORPSE))
        {
            rotten_items.push_back(index_to_letter(i));
            if (you.equip[EQ_WEAPON] == i)
                you.wield_change = true;
        }
    }

    //mv: messages when chunks/corpses become rotten
    if (!rotten_items.empty())
    {
        string msg = "";

        // Races that can't smell don't care, and trolls are stupid and
        // don't care.
        if (you.can_smell() && you.species != SP_TROLL)
        {
            int temp_rand = 0; // Grr.
            int level = player_mutation_level(MUT_SAPROVOROUS);
            if (!level && you.species == SP_VAMPIRE)
                level = 1;

            switch (level)
            {
                    // level 1 and level 2 saprovores, as well as vampires, aren't so touchy
                case 1:
                case 2:
                    temp_rand = random2(8);
                    msg = (temp_rand  < 5) ? "You smell something rotten." :
                    (temp_rand == 5) ? "You smell rotting flesh." :
                    (temp_rand == 6) ? "You smell decay."
                    : "There is something rotten in your inventory.";
                    break;

                    // level 3 saprovores like it
                case 3:
                    temp_rand = random2(8);
                    msg = (temp_rand  < 5) ? "You smell something rotten." :
                    (temp_rand == 5) ? "The smell of rotting flesh makes you hungry." :
                    (temp_rand == 6) ? "You smell decay. Yum-yum."
                    : "Wow! There is something tasty in your inventory.";
                    break;

                default:
                    temp_rand = random2(8);
                    msg = (temp_rand  < 5) ? "You smell something rotten." :
                    (temp_rand == 5) ? "The smell of rotting flesh makes you sick." :
                    (temp_rand == 6) ? "You smell decay. Yuck!"
                    : "Ugh! There is something really disgusting in your inventory.";
                    break;
            }
        }
        else
            msg = "Something in your inventory has become rotten.";

        mprf(MSGCH_ROTTEN_MEAT, "%s (slot%s %s)",
             msg.c_str(),
             rotten_items.size() > 1 ? "s" : "",
             comma_separated_line(rotten_items.begin(),
                                  rotten_items.end()).c_str());

        learned_something_new(HINT_ROTTEN_FOOD);
    }

    if (num_chunks_gone > 0)
    {
        mprf(MSGCH_ROTTEN_MEAT,
             "%s of the chunks of flesh in your inventory have rotted away.",
             num_chunks_gone == num_chunks ? "All" : "Some");
    }
}


/**
 * Initialise a stack of blood potions with a vector of timers, representing
 * the time at which each potion will rot.
 *
 * @param stack     The stack of blood potions to be initialized.
 * @param age       The age for which the stack will last before rotting.
 * (If -1, will be initialized to a default value.)
 */
void init_stack_blood_potions(item_def &stack, int age = -1)
{
    ASSERT(is_blood_potion(stack));
    ASSERT(stack.quantity);

    CrawlHashTable &props = stack.props;
    props.clear(); // sanity measure
    props[TIMER_KEY].new_vector(SV_INT, SFLAG_CONST_TYPE);
    CrawlVector &timer = props[TIMER_KEY].get_vector();

    if (age == -1)
    {
        if (stack.sub_type == POT_BLOOD)
            age = FRESHEST_BLOOD;
        else // coagulated blood
            age = ROTTING_BLOOD;
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

// Compare two CrawlStoreValues storing type T.
template<class T>
static bool _storeval_greater(const CrawlStoreValue &a,
                              const CrawlStoreValue &b)
{
    return T(a) > T(b);
}

// Sort a CrawlVector containing only Ts.
template<class T>
static void _sort_cvec(CrawlVector &vec)
{
    sort(vec.begin(), vec.end(), _storeval_greater<T>);
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
                                   | ISFLAG_NO_PICKUP | ISFLAG_SUMMONED);
    stack.inscription = old.inscription;
    item_colour(stack);

    CrawlHashTable &props_new = stack.props;
    props_new[TIMER_KEY].new_vector(SV_INT, SFLAG_CONST_TYPE);
    CrawlVector &timer_new = props_new[TIMER_KEY].get_vector();

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
    if (!props.exists(TIMER_KEY))
        init_stack_blood_potions(blood);

    ASSERT(props.exists(TIMER_KEY));
    CrawlVector &timer = props[TIMER_KEY].get_vector();
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
                if (!props2.exists(TIMER_KEY))
                    init_stack_blood_potions(*si);

                ASSERT(props2.exists(TIMER_KEY));
                CrawlVector &timer2 = props2[TIMER_KEY].get_vector();
                ASSERT(timer2.size() == si->quantity);

                // Update timer -> push(pop).
                while (!age_timer.empty())
                {
                    const int val = age_timer.back();
                    age_timer.pop_back();
                    timer2.push_back(val);
                }
                _sort_cvec<int>(timer2);
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

// Prints messages for blood potions coagulating or rotting in inventory.
static void _potion_stack_changed_message(item_def &potion, int num_changed,
                                          string verb)
{
    ASSERT(num_changed > 0);

    verb = replace_all(verb, "%s", num_changed == 1 ? "s" : "");
    mprf(MSGCH_ROTTEN_MEAT, "%s %s %s.",
         get_desc_quantity(num_changed, potion.quantity).c_str(),
         potion.name(DESC_PLAIN, false).c_str(),
         verb.c_str());
}


/**
 * Coagulate and/or rot away blood potions in a stack if necessary.
 * @param blood The blood potion.
 */
void maybe_coagulate_blood_potions_inv(item_def &blood)
{
    ASSERT(blood.defined());
    ASSERT(is_blood_potion(blood));

    CrawlHashTable &props = blood.props;
    if (!props.exists(TIMER_KEY))
        init_stack_blood_potions(blood);

    ASSERT(props.exists(TIMER_KEY));
    CrawlVector &timer = props[TIMER_KEY].get_vector();
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
        return; // Nothing to be done.

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
        return;
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
            if (!props2.exists(TIMER_KEY))
                init_stack_blood_potions(you.inv[m]);

            ASSERT(props2.exists(TIMER_KEY));
            CrawlVector &timer2 = props2[TIMER_KEY].get_vector();
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
            _sort_cvec<int>(timer2);
            return;
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
        return;
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
        return;
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
            if (!props2.exists(TIMER_KEY))
                init_stack_blood_potions(mitm[o]);

            ASSERT(props2.exists(TIMER_KEY));
            CrawlVector &timer2 = props2[TIMER_KEY].get_vector();
            ASSERT(timer2.size() == mitm[o].quantity);

            // Update timer -> push(pop).
            int val;
            while (!age_timer.empty())
            {
                val = age_timer[age_timer.size() - 1];
                age_timer.pop_back();
                timer2.push_back(val);
            }
            _sort_cvec<int>(timer2);

            inc_mitm_item_quantity(o, coag_count);
            ASSERT(timer2.size() == mitm[o].quantity);
            dec_inv_item_quantity(blood.link, rot_count + coag_count);
            _compare_blood_quantity(blood, timer.size());
            return;
        }
        o = mitm[o].link;
    }
    // If we got here nothing was found!

    // Create a new stack of potions.
    o = get_mitm_slot();
    if (o == NON_ITEM)
        return;

    _init_coagulated_blood(mitm[o], coag_count, blood, age_timer);

    move_item_to_grid(&o, you.pos());

    if (!dec_inv_item_quantity(blood.link, coag_count + rot_count))
        _compare_blood_quantity(blood, timer.size());
}

// Removes the oldest timer of a stack of blood potions.
// Mostly used for (q)uaff and (f)ire.
int remove_oldest_blood_potion(item_def &stack)
{
    ASSERT(stack.defined());
    ASSERT(is_blood_potion(stack));

    CrawlHashTable &props = stack.props;
    if (!props.exists(TIMER_KEY))
        init_stack_blood_potions(stack);
    ASSERT(props.exists(TIMER_KEY));
    CrawlVector &timer = props[TIMER_KEY].get_vector();
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
    if (!props.exists(TIMER_KEY))
        init_stack_blood_potions(stack);
    ASSERT(props.exists(TIMER_KEY));
    CrawlVector &timer = props[TIMER_KEY].get_vector();
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
    _sort_cvec<int>(timer);
}

void merge_blood_potion_stacks(const item_def &source, item_def &dest,
                               int quant)
{
    if (!source.defined() || !dest.defined())
        return;

    ASSERT_RANGE(quant, 1, source.quantity + 1);
    ASSERT(is_blood_potion(source));
    ASSERT(is_blood_potion(dest));

    const CrawlHashTable &props = source.props;

    CrawlHashTable &props2 = dest.props;
    if (!props2.exists(TIMER_KEY))
        init_stack_blood_potions(dest);
    ASSERT(props2.exists(TIMER_KEY));
    CrawlVector &timer2 = props2[TIMER_KEY].get_vector();

    dprf("origin qt: %d, taking %d, putting into stack of size %d with initial timer count %d", source.quantity, quant, dest.quantity, timer2.size());

    ASSERT(timer2.size() == dest.quantity);

    // Update timer2
    for (int i = 0; i < quant; i++)
    {
        const int timer_index = source.quantity - 1 - i;
        const int timer_value = props.exists(TIMER_KEY) ?
                                props[TIMER_KEY].get_vector()[timer_index].get_int()
                            : FRESHEST_BLOOD;
        timer2.push_back(timer_value);
    }

    dprf("Eventual timer size: %d", timer2.size());

    ASSERT(timer2.size() == dest.quantity + quant);

    // Re-sort timer.
    _sort_cvec<int>(timer2);
}
