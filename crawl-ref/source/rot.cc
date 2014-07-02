/**
 * @file
 * @brief Functions for blood & chunk rot.
 **/

#include "AppHdr.h"

#include "rot.h"

#include "enum.h"
#include "env.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "makeitem.h"
#include "misc.h"
#include "player.h"
#include "stuff.h"


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
