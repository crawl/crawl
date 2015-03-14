/**
 * @file
 * @brief Functions for handling multi-turn actions.
**/

#include "AppHdr.h"

#include "delay.h"

#include <cstdio>
#include <cstring>

#include "ability.h"
#include "areas.h"
#include "artefact.h"
#include "bloodspatter.h"
#include "butcher.h"
#include "clua.h"
#include "command.h"
#include "coord.h"
#include "database.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "enum.h"
#include "env.h"
#include "exclude.h"
#include "exercise.h"
#include "food.h"
#include "fprop.h"
#include "godabil.h"
#include "godconduct.h"
#include "godpassive.h"
#include "godprayer.h"
#include "godwrath.h"
#include "hints.h"
#include "invent.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "notes.h"
#include "options.h"
#include "ouch.h"
#include "output.h"
#include "player-equip.h"
#include "player.h"
#include "prompt.h"
#include "random.h"
#include "religion.h"
#include "rot.h"
#include "shout.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "spl-util.h"
#include "stairs.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h"
#include "teleport.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "xom.h"

extern vector<SelItem> items_for_multidrop;

class interrupt_block
{
public:
    interrupt_block() { ++interrupts_blocked; }
    ~interrupt_block() { --interrupts_blocked; }

    static bool blocked() { return interrupts_blocked > 0; }
private:
    static int interrupts_blocked;
};

int interrupt_block::interrupts_blocked = 0;

static void _xom_check_corpse_waste();
static void _handle_run_delays(const delay_queue_item &delay);
static void _handle_macro_delay();
static const char *_activity_interrupt_name(activity_interrupt_type ai);
static void _finish_delay(const delay_queue_item &delay);

// Returns true if this delay can act as a parent to other delays, i.e. if
// other delays can be spawned while this delay is running. If is_parent_delay
// returns true, new delays will be pushed immediately to the front of the
// delay in question, rather than at the end of the queue.
static bool _is_parent_delay(delay_type delay)
{
    // Interlevel travel can do upstairs/downstairs delays.
    // Lua macros can in theory perform any of the other delays,
    // including travel; in practise travel still assumes there can be
    // no parent delay.
    return delay_is_run(delay)
           || delay == DELAY_MACRO
           || delay == DELAY_MULTIDROP;
}

static int _push_delay(const delay_queue_item &delay)
{
    for (auto i = you.delay_queue.begin(); i != you.delay_queue.end(); ++i)
    {
        if (_is_parent_delay(i->type))
        {
            size_t pos = i - you.delay_queue.begin();
            you.delay_queue.insert(i, delay);
            return pos;
        }
    }
    you.delay_queue.push_back(delay);
    return you.delay_queue.size() - 1;
}

static void _pop_delay()
{
    if (!you.delay_queue.empty())
        you.delay_queue.erase(you.delay_queue.begin());
}

static int delays_cleared[NUM_DELAYS];
static int cleared_delays_parm1[NUM_DELAYS];

static void _clear_pending_delays()
{
    memset(delays_cleared, 0, sizeof(delays_cleared));
    memset(cleared_delays_parm1, 0, sizeof(cleared_delays_parm1));

    while (you.delay_queue.size() > 1)
    {
        const delay_queue_item delay =
            you.delay_queue[you.delay_queue.size() - 1];

        delays_cleared[delay.type]++;
        cleared_delays_parm1[delay.type] = delay.parm1;

        you.delay_queue.pop_back();

        if (delay_is_run(delay.type) && you.running)
            stop_running();
    }
}

void start_delay(delay_type type, int turns, int parm1, int parm2, int parm3)
{
    ASSERT(!crawl_state.game_is_arena());

    if (type == DELAY_MEMORISE && already_learning_spell(parm1))
        return;

    delay_queue_item delay;

    delay.type     = type;
    delay.duration = turns;
    delay.parm1    = parm1;
    delay.parm2    = parm2;
    delay.parm3    = parm3;
    delay.started  = false;

    if (delay_is_run(type))
        clear_travel_trail();

    _push_delay(delay);
}

void stop_delay(bool stop_stair_travel, bool force_unsafe)
{
    if (you.delay_queue.empty())
        return;

    set_more_autoclear(false);

    ASSERT(!crawl_state.game_is_arena());

    delay_queue_item delay = you.delay_queue.front();

    const bool multiple_corpses =
        (delay.type == DELAY_BUTCHER || delay.type == DELAY_BOTTLE_BLOOD)
        // + 1 because this delay is still in the queue.
        && any_of(you.delay_queue.begin() + 1, you.delay_queue.end(),
                  [] (const delay_queue_item &dqi)
                  {
                      return dqi.type == DELAY_BUTCHER
                          || dqi.type == DELAY_BOTTLE_BLOOD;
                  });

    // At the very least we can remove any queued delays, right
    // now there is no problem with doing this... note that
    // any queuing here can only happen from a single command,
    // as the effect of a delay doesn't normally allow interaction
    // until it is done... it merely chains up individual actions
    // into a single action.  -- bwr
    _clear_pending_delays();

    switch (delay.type)
    {
    case DELAY_BUTCHER:
    case DELAY_BOTTLE_BLOOD:
    {
        mprf("You stop %s the corpse%s.",
             delay.type == DELAY_BUTCHER ? "butchering" : "bottling blood from",
             multiple_corpses ? "s" : "");

        _pop_delay();
        break;
    }
    case DELAY_MEMORISE:
        // Losing work here is okay... having to start from
        // scratch is a reasonable behaviour. -- bwr
        mpr("Your memorisation is interrupted.");
        _pop_delay();
        break;

    case DELAY_MULTIDROP:
        // No work lost
        if (!items_for_multidrop.empty())
            mpr("You stop dropping stuff.");
        _pop_delay();
        break;

    case DELAY_RUN:
    case DELAY_REST:
    case DELAY_TRAVEL:
    case DELAY_MACRO:
        // Always interruptible.
        _pop_delay();

        // Keep things consistent, otherwise disturbing phenomena can occur.
        // Note that runrest::stop() will turn around and call stop_delay()
        // again, but that's okay because the delay is already popped off
        // the queue.
        if (delay_is_run(delay.type) && you.running)
            stop_running();

        // There's no special action needed for macros - if we don't call out
        // to the Lua function, it can't do damage.
        break;

    case DELAY_INTERRUPTIBLE:
        // Always stoppable by definition...
        // try using a more specific type anyway. -- bwr
        _pop_delay();
        break;

    case DELAY_EAT:
        // XXX: Large problems with object destruction here... food can
        // be from in the inventory or on the ground and these are
        // still handled quite differently.  Eventually we would like
        // this to be stoppable, with partial food items implemented. -- bwr
        break;

    case DELAY_FEED_VAMPIRE:
    {
        mpr("You stop draining the corpse.");

        did_god_conduct(DID_DRINK_BLOOD, 8);

        _xom_check_corpse_waste();

        item_def &item = (delay.parm1 ? you.inv[delay.parm2]
                                      : mitm[delay.parm2]);

        const bool was_orc = (mons_genus(item.mon_type) == MONS_ORC);
        const bool was_holy = (mons_class_holiness(item.mon_type) == MH_HOLY);

        // Don't skeletonize a corpse if it's no longer there!
        if (item.defined() && item.is_type(OBJ_CORPSES, CORPSE_BODY)
            && item.pos == you.pos())
        {
            mpr("All blood oozes out of the corpse!");

            bleed_onto_floor(you.pos(), item.mon_type, delay.duration, false);

            const item_def corpse = item;

            if (mons_skeleton(item.mon_type) && one_chance_in(3))
                turn_corpse_into_skeleton(item);
            else
            {
                if (delay.parm1)
                    dec_inv_item_quantity(delay.parm2, 1);
                else
                    dec_mitm_item_quantity(delay.parm2, 1);
            }

            maybe_drop_monster_hide(corpse);
        }

        if (was_orc)
            did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);
        if (was_holy)
            did_god_conduct(DID_DESECRATE_HOLY_REMAINS, 2);

        delay.duration = 0;
        _pop_delay();
        return;
    }

    case DELAY_ARMOUR_ON:
    case DELAY_ARMOUR_OFF:
        if (delay.duration > 1 && !delay.parm3)
        {
            if (!yesno(delay.type == DELAY_ARMOUR_ON ?
                       "Keep equipping yourself?" :
                       "Keep disrobing?", false, 0, false))
            {
                mprf("You stop %s your armour.",
                     delay.type == DELAY_ARMOUR_ON ? "putting on"
                                                   : "removing");
                _pop_delay();
            }
            else
                you.delay_queue.front().parm3 = 1;
        }
        break;

    case DELAY_BLURRY_SCROLL:
        if (delay.duration <= 1 || delay.parm3)
            break;

        if (!yesno("Keep reading the scroll?", false, 0, false))
        {
            mpr("You stop reading the scroll.");
            _pop_delay();
        }
        else
            you.delay_queue.front().parm3 = 1;

        break;

    case DELAY_ASCENDING_STAIRS:  // short... and probably what people want
    case DELAY_DESCENDING_STAIRS: // short... and probably what people want
        if (stop_stair_travel)
        {
            mprf("You stop %s the stairs.",
                 delay.type == DELAY_ASCENDING_STAIRS ? "ascending"
                                                      : "descending");
            _pop_delay();
        }
        break;

    case DELAY_PASSWALL:
        if (stop_stair_travel)
        {
            mpr("Your meditation is interrupted.");
            _pop_delay();
        }
        break;

    case DELAY_SHAFT_SELF:
        if (stop_stair_travel)
        {
            mpr("You stop digging.");
            _pop_delay();
        }
        break;

    case DELAY_DROP_ITEM:         // one turn... only used for easy armour drops
    case DELAY_JEWELLERY_ON:      // one turn
    case DELAY_UNINTERRUPTIBLE:   // never stoppable
    default:
        break;
    }

    if (delay_is_run(delay.type))
        update_turn_count();
}

static bool _is_butcher_delay(int delay)
{
    return delay == DELAY_BUTCHER
           || delay == DELAY_BOTTLE_BLOOD
           || delay == DELAY_FEED_VAMPIRE;
}

bool you_are_delayed()
{
    return !you.delay_queue.empty();
}

delay_type current_delay_action()
{
    return you_are_delayed() ? you.delay_queue.front().type
                             : DELAY_NOT_DELAYED;
}

bool delay_is_run(delay_type delay)
{
    return delay == DELAY_RUN || delay == DELAY_REST || delay == DELAY_TRAVEL;
}

bool is_being_drained(const item_def &item)
{
    if (!you_are_delayed())
        return false;

    const delay_queue_item &delay = you.delay_queue.front();

    if (delay.type == DELAY_FEED_VAMPIRE)
    {
        const item_def &corpse = mitm[ delay.parm2 ];

        if (&corpse == &item)
            return true;
    }

    return false;
}

bool is_being_butchered(const item_def &item, bool just_first)
{
    if (!you_are_delayed())
        return false;

    for (const delay_queue_item &delay : you.delay_queue)
    {
        if (delay.type == DELAY_BUTCHER || delay.type == DELAY_BOTTLE_BLOOD)
        {
            const item_def &corpse = mitm[ delay.parm1 ];
            if (&corpse == &item)
                return true;

            if (just_first)
                break;
        }
        else
            break;
    }

    return false;
}

bool is_vampire_feeding()
{
    if (!you_are_delayed())
        return false;

    const delay_queue_item &delay = you.delay_queue.front();
    return delay.type == DELAY_FEED_VAMPIRE;
}

bool is_butchering()
{
    if (!you_are_delayed())
        return false;

    const delay_queue_item &delay = you.delay_queue.front();
    return delay.type == DELAY_BUTCHER || delay.type == DELAY_BOTTLE_BLOOD;
}

bool player_stair_delay()
{
    if (!you_are_delayed())
        return false;

    const delay_queue_item &delay = you.delay_queue.front();
    return delay.type == DELAY_ASCENDING_STAIRS
           || delay.type == DELAY_DESCENDING_STAIRS;
}

/**
 * Is the player currently in the middle of memorizing a spell?
 *
 * @param spell     A specific spell, or -1 to check if we're memorizing any
 *                  spell at all.
 * @return          Whether the player is currently memorizing the given type
 *                  of spell.
 */
bool already_learning_spell(int spell)
{
    if (!you_are_delayed())
        return false;

    for (const delay_queue_item &delay : you.delay_queue)
    {
        if (delay.type != DELAY_MEMORISE)
            continue;

        if (spell == -1 || delay.parm1 == spell)
            return true;
    }
    return false;
}

/**
 * Can the player currently read the scroll in the given inventory slot?
 *
 * Prints corresponding messages if the answer is false.
 *
 * @param inv_slot      The inventory slot in question.
 * @return              false if the player is confused, berserk, silenced,
 *                      has no scroll in the given slot, etc; true otherwise.
 */
static bool _can_read_scroll(int inv_slot)
{
    // prints its own messages
    if (!player_can_read())
        return false;

    const string illiteracy_reason = cannot_read_item_reason(you.inv[inv_slot]);
    if (illiteracy_reason.empty())
        return true;

    mpr(illiteracy_reason);
    return false;
}

// Xom is amused by a potential food source going to waste, and is
// more amused the hungrier you are.
static void _xom_check_corpse_waste()
{
    const int food_need = max(HUNGER_SATIATED - you.hunger, 0);
    xom_is_stimulated(50 + (151 * food_need / 6000));
}

void clear_macro_process_key_delay()
{
    if (current_delay_action() == DELAY_MACRO_PROCESS_KEY)
        _pop_delay();
}

void handle_delay()
{
    if (!you_are_delayed())
        return;

    delay_queue_item &delay = you.delay_queue.front();

    // If a Lua macro wanted Crawl to process a key normally, early exit.
    if (delay.type == DELAY_MACRO_PROCESS_KEY)
        return;

    if (!delay.started)
    {
        switch (delay.type)
        {
        case DELAY_ARMOUR_ON:
            mprf(MSGCH_MULTITURN_ACTION, "You start putting on your armour.");
            break;

        case DELAY_ARMOUR_OFF:
            mprf(MSGCH_MULTITURN_ACTION, "You start removing your armour.");
            break;

        case DELAY_MEMORISE:
        {
            spell_type spell = static_cast<spell_type>(delay.parm1);
            if (vehumet_is_offering(spell))
            {
                string message = make_stringf(" grants you knowledge of %s.",
                    spell_title(spell));
                simple_god_message(message.c_str());
            }
            mprf(MSGCH_MULTITURN_ACTION, "You start memorising the spell.");
            break;
        }

        case DELAY_PASSWALL:
            mprf(MSGCH_MULTITURN_ACTION, "You begin to meditate on the wall.");
            break;

        case DELAY_SHAFT_SELF:
            mprf(MSGCH_MULTITURN_ACTION, "You begin to dig a shaft.");
            break;

        case DELAY_BLURRY_SCROLL:
            mprf(MSGCH_MULTITURN_ACTION, "You begin reading the scroll.");
            break;

        default:
            break;
        }

        delay.started = true;
    }

    // Run delays and Lua delays don't have a specific end time.
    if (delay_is_run(delay.type))
    {
        _handle_run_delays(delay);
        return;
    }

    if (delay.type == DELAY_MACRO)
    {
        _handle_macro_delay();
        return;
    }

    // First check cases where delay may no longer be valid:
    // XXX: need to handle passwall when monster digs -- bwr
    if (delay.type == DELAY_FEED_VAMPIRE)
    {
        item_def &corpse = delay.parm1 ? you.inv[delay.parm2]
                                       : mitm[delay.parm2];
        // Vampires stop feeding if ...
        // * engorged ("alive")
        // * bat form runs out due to becoming full
        // * corpse becomes poisonous as the Vampire loses poison resistance
        // * corpse disappears for some reason (e.g. animated by a monster)
        if (!corpse.defined()                                     // missing
            || corpse.base_type != OBJ_CORPSES                    // noncorpse
            || corpse.pos != you.pos()                            // elsewhere
            || you.hunger_state == HS_ENGORGED
            || you.hunger_state > HS_SATIATED && you.form == TRAN_BAT
            || you.hunger_state >= HS_SATIATED
               && is_poisonous(corpse))
        {
            // Messages handled in _food_change() in food.cc.
            stop_delay();
            return;
        }
        else if (corpse.is_type(OBJ_CORPSES, CORPSE_SKELETON))
        {
            mprf("The corpse has rotted away into a skeleton before"
                 "you could finish drinking it!");
            _xom_check_corpse_waste();
            stop_delay();
            return;
        }
    }
    else if (delay.type == DELAY_BUTCHER || delay.type == DELAY_BOTTLE_BLOOD)
    {
        const item_def &item = mitm[delay.parm1];
        // A monster may have raised the corpse you're chopping up! -- bwr
        // Note that a monster could have raised the corpse and another
        // monster could die and create a corpse with the same ID number...
        // However, it would not be at the player's square like the
        // original and that's why we do it this way.
        if (!item.defined()
            || item.base_type != OBJ_CORPSES
            || item.pos != you.pos())
        {
            // There being no item at all could have happened for several
            // reasons, so don't bother to give a message.
            _pop_delay();
            return;
        }
        else if (item.is_type(OBJ_CORPSES, CORPSE_SKELETON))
        {
            mprf("The corpse has rotted away into a skeleton before"
                 "you could %s!",
                 (delay.type == DELAY_BOTTLE_BLOOD ? "bottle its blood"
                                                   : "butcher it"));
            _xom_check_corpse_waste();
            _pop_delay();
            return;
        }
    }
    else if (delay.type == DELAY_MULTIDROP)
    {
        // Throw away invalid items. XXX: what are they?
        while (!items_for_multidrop.empty()
               // Don't look for gold in inventory
               && items_for_multidrop[0].slot != PROMPT_GOT_SPECIAL
               && !you.inv[items_for_multidrop[0].slot].defined())
        {
            items_for_multidrop.erase(items_for_multidrop.begin());
        }

        if (items_for_multidrop.empty())
        {
            // Ran out of things to drop.
            _pop_delay();
            you.time_taken = 0;
            return;
        }
    }
    else if (delay.type == DELAY_BLURRY_SCROLL)
    {
        if (!_can_read_scroll(delay.parm1))
        {
            _pop_delay();
            you.time_taken = 0;
            return;
        }
    }

    // Handle delay:
    if (delay.duration > 0)
    {
        dprf("Delay type: %d (%s), duration: %d",
             delay.type, delay_name(delay.type), delay.duration);
        // delay.duration-- *must* be done before multidrop, because
        // multidrop is now a parent delay, which means other delays
        // can be pushed to the front of the queue, invalidating the
        // "delay" reference here, and resulting in tons of debugging
        // fun with valgrind.
        delay.duration--;

        switch (delay.type)
        {
        case DELAY_ARMOUR_ON:
            mprf(MSGCH_MULTITURN_ACTION, "You continue putting on %s.",
                 you.inv[delay.parm1].name(DESC_YOUR).c_str());
            break;

        case DELAY_ARMOUR_OFF:
            mprf(MSGCH_MULTITURN_ACTION, "You continue taking off %s.",
                 you.inv[delay.parm1].name(DESC_YOUR).c_str());
            break;

        case DELAY_JEWELLERY_ON:
            // This is a 1-turn delay where the time cost is handled
            // in _finish_delay().
            // FIXME: get rid of this hack!
            you.time_taken = 0;
            break;

        case DELAY_MEMORISE:
            mprf(MSGCH_MULTITURN_ACTION, "You continue memorising.");
            break;

        case DELAY_PASSWALL:
            mprf(MSGCH_MULTITURN_ACTION, "You continue meditating on the rock.");
            break;

        case DELAY_SHAFT_SELF:
            mprf(MSGCH_MULTITURN_ACTION, "You continue digging a shaft.");
            break;

        case DELAY_BLURRY_SCROLL:
            mprf(MSGCH_MULTITURN_ACTION, "You continue reading the scroll.");
            break;

        case DELAY_MULTIDROP:
            if (!drop_item(items_for_multidrop[0].slot,
                           items_for_multidrop[0].quantity))
            {
                you.time_taken = 0;
            }
            items_for_multidrop.erase(items_for_multidrop.begin());
            break;

        case DELAY_EAT:
            mprf(MSGCH_MULTITURN_ACTION, "You continue eating.");
            break;

        case DELAY_FEED_VAMPIRE:
        {
            item_def &corpse = (delay.parm1 ? you.inv[delay.parm2]
                                            : mitm[delay.parm2]);
            mprf(MSGCH_MULTITURN_ACTION, "You continue drinking.");
            vampire_nutrition_per_turn(corpse, 0);
            break;
        }

        default:
            break;
        }
    }
    else
    {
        _finish_delay(delay);
        if (!you.turn_is_over)
            you.time_taken = 0;
    }
}

static void _armour_wear_effects(const int item_slot);

static void _finish_delay(const delay_queue_item &delay)
{
    switch (delay.type)
    {
    case DELAY_JEWELLERY_ON:
    {
        const item_def &item = you.inv[delay.parm1];

        // recheck stasis here, since our condition may have changed since
        // starting the amulet swap process
        // just breaking here is okay because swapping jewellery is a one-turn
        // action, so conceptually there is nothing to interrupt - in other
        // words, this is equivalent to if the user took off the previous
        // amulet and was slowed before putting the amulet of stasis on as a
        // separate action on the next turn
        // XXX: duplicates a check in invent.cc:check_warning_inscriptions()
        if (nasty_stasis(item, OPER_PUTON)
            && item_ident(item, ISFLAG_KNOW_TYPE))
        {
            string prompt = "Really put on ";
            prompt += item.name(DESC_INVENTORY);
            prompt += string(" while ")
                      + (you.duration[DUR_TELEPORT] ? "about to teleport" :
                         you.duration[DUR_SLOW] ? "slowed" : "hasted");
            prompt += "?";
            if (!yesno(prompt.c_str(), false, 'n'))
                break;
        }

        puton_ring(delay.parm1, false);
        break;
    }

    case DELAY_ARMOUR_ON:
        _armour_wear_effects(delay.parm1);
        break;

    case DELAY_ARMOUR_OFF:
    {
        const equipment_type slot = get_armour_slot(you.inv[delay.parm1]);
        ASSERT(you.equip[slot] == delay.parm1);

        mprf("You finish taking off %s.",
             you.inv[delay.parm1].name(DESC_YOUR).c_str());
        unequip_item(slot);

        break;
    }

    case DELAY_EAT:
        if (delay.parm3 > 0) // If duration was just one turn, don't print.
            mpr("You finish eating.");
        // For chunks, warn the player if they're not getting much
        // nutrition. Also, print the other eating messages only now.
        if (delay.parm1)
            chunk_nutrition_message(delay.parm1);
        else if (delay.parm2 != -1)
            finished_eating_message(delay.parm2);
        break;

    case DELAY_FEED_VAMPIRE:
    {
        mpr("You finish drinking.");

        did_god_conduct(DID_DRINK_BLOOD, 8);

        item_def &item = (delay.parm1 ? you.inv[delay.parm2]
                                      : mitm[delay.parm2]);

        const bool was_orc = (mons_genus(item.mon_type) == MONS_ORC);
        const bool was_holy = (mons_class_holiness(item.mon_type) == MH_HOLY);

        vampire_nutrition_per_turn(item, 1);

        // Don't try to apply delay end effects if
        // vampire_nutrition_per_turn did a stop_delay already:
        if (is_vampire_feeding())
        {
            const item_def corpse = item;

            if (mons_skeleton(item.mon_type) && one_chance_in(3))
            {
                turn_corpse_into_skeleton(item);
                item_check();
            }
            else
            {
                if (delay.parm1)
                    dec_inv_item_quantity(delay.parm2, 1);
                else
                    dec_mitm_item_quantity(delay.parm2, 1);
            }

            maybe_drop_monster_hide(corpse);
        }

        if (was_orc)
            did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);
        if (was_holy)
            did_god_conduct(DID_DESECRATE_HOLY_REMAINS, 2);
        break;
    }

    case DELAY_MEMORISE:
    {
        spell_type spell = static_cast<spell_type>(delay.parm1);
        mpr("You finish memorising.");
        add_spell_to_memory(spell);
        vehumet_accept_gift(spell);
        break;
    }

    case DELAY_PASSWALL:
    {
        mpr("You finish merging with the rock.");
        more();  // or the above message won't be seen

        const coord_def pass(delay.parm1, delay.parm2);

        if (pass.x != 0 && pass.y != 0)
        {
            switch (grd(pass))
            {
            default:
                if (!you.is_habitable(pass))
                {
                    mpr("...yet there is something new on the other side. "
                        "You quickly turn back.");
                    goto passwall_aborted;
                }
                break;

            case DNGN_CLOSED_DOOR:      // open the door
            case DNGN_RUNED_DOOR:
                // Once opened, former runed doors become normal doors.
                // Is that ok?  Keeping it for simplicity for now...
                grd(pass) = DNGN_OPEN_DOOR;
                break;
            }

            // Move any monsters out of the way.
            if (monster* m = monster_at(pass))
            {
                // One square, a few squares, anywhere...
                if (!m->shift() && !monster_blink(m, true))
                    monster_teleport(m, true, true);
                // Might still fail.
                if (monster_at(pass))
                {
                    mpr("...and sense your way blocked. You quickly turn back.");
                    goto passwall_aborted;
                }

                move_player_to_grid(pass, false);

                // Wake the monster if it's asleep.
                if (m)
                    behaviour_event(m, ME_ALERT, &you);
            }
            else
                move_player_to_grid(pass, false);

        passwall_aborted:
            redraw_screen();
        }
        break;
    }

    case DELAY_SHAFT_SELF:
        you.do_shaft_ability();
        break;

    case DELAY_BLURRY_SCROLL:
        // Make sure the scroll still exists, the player isn't confused, etc
        if (_can_read_scroll(delay.parm1))
            read_scroll(delay.parm1);
        break;

    case DELAY_BUTCHER:
    case DELAY_BOTTLE_BLOOD:
        // We know the item is valid and a real corpse, because handle_delay()
        // checked for that.
        finish_butchering(mitm[delay.parm1], delay.type == DELAY_BOTTLE_BLOOD);
        // Don't waste time picking up chunks if you're already
        // starving. (jpeg)
        if ((you.hunger_state > HS_STARVING || you.species == SP_VAMPIRE)
            // Only pick up chunks if this is the last delay...
            && (you.delay_queue.size() == 1
            // ...Or, equivalently, if it's the last butcher one.
                || !_is_butcher_delay(you.delay_queue[1].type)))
        {
            request_autopickup();
        }
        you.turn_is_over = true;
        break;

    case DELAY_DROP_ITEM:
        // We're here if dropping the item required some action to be done
        // first, like removing armour.  At this point, it should be droppable
        // immediately.

        // Make sure item still exists.
        if (!you.inv[delay.parm1].defined())
            break;

        drop_item(delay.parm1, delay.parm2);
        break;

    case DELAY_ASCENDING_STAIRS:
        up_stairs();
        break;

    case DELAY_DESCENDING_STAIRS:
        down_stairs();
        break;

    case DELAY_INTERRUPTIBLE:
    case DELAY_UNINTERRUPTIBLE:
        // These are simple delays that have no effect when complete.
        break;

    default:
        mpr("You finish doing something.");
        break;
    }

    you.wield_change = true;
    print_stats();  // force redraw of the stats
    _pop_delay();

#ifdef USE_TILE
    tiles.update_tabs();
#endif
}

static void _armour_wear_effects(const int item_slot)
{
    const unsigned int old_talents = your_talents(false).size();

    item_def &arm = you.inv[item_slot];

    set_ident_flags(arm, ISFLAG_IDENT_MASK);
    if (is_artefact(arm))
        arm.flags |= ISFLAG_NOTED_ID;

    const equipment_type eq_slot = get_armour_slot(arm);

    mprf("You finish putting on %s.", arm.name(DESC_YOUR).c_str());

    if (eq_slot == EQ_BODY_ARMOUR)
    {
        if (you.duration[DUR_ICY_ARMOUR] != 0
            && !is_effectively_light_armour(&arm))
        {
            remove_ice_armour();
        }
    }
    else if (eq_slot == EQ_SHIELD)
    {
        if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
            remove_condensation_shield();
    }

    equip_item(eq_slot, item_slot);

    check_item_hint(you.inv[item_slot], old_talents);
}

static command_type _get_running_command()
{
    if (Options.travel_key_stop && kbhit()
        || !in_bounds(you.pos() + you.running.pos))
    {
        stop_running();
        return CMD_NO_CMD;
    }

    if (is_resting())
    {
        you.running.rest();

#ifdef USE_TILE
        if (Options.rest_delay >= 0 && tiles.need_redraw())
            tiles.redraw();
#endif

        if (!is_resting() && you.running.hp == you.hp
            && you.running.mp == you.magic_points)
        {
            mpr("Done waiting.");
        }

        if (Options.rest_delay > 0)
            delay(Options.rest_delay);

        return CMD_WAIT;
    }
    else if (you.running.is_explore() && Options.explore_delay > -1)
        delay(Options.explore_delay);
    else if (Options.travel_delay > 0)
        delay(Options.travel_delay);

    return direction_to_command(you.running.pos.x, you.running.pos.y);
}

static bool _auto_eat(delay_type type)
{
    return Options.auto_eat_chunks
           && (!you.gourmand()
               || you.duration[DUR_GOURMAND] >= GOURMAND_MAX / 4
               || you.hunger_state < HS_SATIATED)
           && (type == DELAY_REST || type == DELAY_TRAVEL);
}

static void _handle_run_delays(const delay_queue_item &delay)
{
    // Handle inconsistencies between the delay queue and you.running.
    // We don't want to send the game into a deadlock.
    if (!you.running)
    {
        update_turn_count();
        _pop_delay();
        return;
    }

    if (you.turn_is_over)
        return;

    command_type cmd = CMD_NO_CMD;

    const bool want_move =
        delay.type == DELAY_RUN || delay.type == DELAY_TRAVEL;
    if ((want_move && you.confused()) || !i_feel_safe(true, want_move))
        stop_running();
    else
    {
        if (_auto_eat(delay.type))
        {
            const interrupt_block block_interrupts;
            if (prompt_eat_chunks(true) == 1)
                return;
        }

        if (Options.auto_sacrifice == AS_YES
            && you.running == RMODE_EXPLORE_GREEDY)
        {
            LevelStashes *lev = StashTrack.find_current_level();
            if (lev && lev->sacrificeable(you.pos()))
            {
                const interrupt_block block_interrupts;
                pray(false);
                return;
            }
        }

        switch (delay.type)
        {
        case DELAY_REST:
        case DELAY_RUN:
            cmd = _get_running_command();
            break;
        case DELAY_TRAVEL:
            cmd = travel();
            break;
        default:
            break;
        }
    }

    if (cmd != CMD_NO_CMD)
    {
        if (delay.type != DELAY_REST)
            clear_messages();
        process_command(cmd);
    }

    if (!you.turn_is_over)
        you.time_taken = 0;

    // If you.running has gone to zero, and the run delay was not
    // removed, remove it now. This is needed to clean up after
    // find_travel_pos() function in travel.cc.
    if (!you.running && delay_is_run(current_delay_action()))
    {
        _pop_delay();
        update_turn_count();
    }
}

static void _handle_macro_delay()
{
    run_macro();

    // Macros may not use up turns, but unless we zero time_taken,
    // main.cc will call world_reacts and increase turn count.
    if (!you.turn_is_over && you.time_taken)
        you.time_taken = 0;
}

static void _decrement_delay(delay_type delay)
{
    for (auto &delay_item : you.delay_queue)
    {
        if (delay_item.type == delay)
        {
            if (delay_item.duration > 0)
                --delay_item.duration;
            break;
        }
    }
}

void run_macro(const char *macroname)
{
    const int currdelay = current_delay_action();
    if (currdelay != DELAY_NOT_DELAYED && currdelay != DELAY_MACRO)
        return;

    if (currdelay == DELAY_MACRO)
    {
        const delay_queue_item &delay = you.delay_queue.front();
        if (!delay.duration)
        {
            dprf("Expiring macro delay on turn: %d", you.num_turns);
            stop_delay();
            return;
        }
    }

#ifdef CLUA_BINDINGS
    if (!clua)
    {
        mprf(MSGCH_DIAGNOSTICS, "Lua not initialised");
        stop_delay();
        return;
    }

    if (!currdelay)
        start_delay(DELAY_MACRO, 1);

    // If callbooleanfn returns false, that means the macro either exited
    // normally by returning or by calling coroutine.yield(false). Either way,
    // decrement the macro duration.
    if (!clua.callbooleanfn(false, "c_macro", "s", macroname))
    {
        if (!clua.error.empty())
        {
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());
            stop_delay();
        }
        else
        {
            // Decay the macro delay's duration.
            _decrement_delay(DELAY_MACRO);
        }
    }
#else
    UNUSED(_decrement_delay);
    stop_delay();
#endif
}

bool is_delay_interruptible(delay_type delay)
{
    return !(delay == DELAY_EAT || delay == DELAY_DROP_ITEM
             || delay == DELAY_JEWELLERY_ON || delay == DELAY_UNINTERRUPTIBLE);
}

// Returns TRUE if the delay should be interrupted, MAYBE if the user function
// had no opinion on the matter, FALSE if the delay should not be interrupted.
static maybe_bool _userdef_interrupt_activity(const delay_queue_item &idelay,
                                              activity_interrupt_type ai,
                                              const activity_interrupt_data &at)
{
#ifdef CLUA_BINDINGS
    const int delay = idelay.type;
    lua_State *ls = clua.state();
    if (!ls || ai == AI_FORCE_INTERRUPT)
        return MB_TRUE;

    const char *interrupt_name = _activity_interrupt_name(ai);
    const char *act_name = delay_name(delay);

    bool ran = clua.callfn("c_interrupt_activity", "1:ssA",
                           act_name, interrupt_name, &at);
    if (ran)
    {
        // If the function returned nil, we want to cease processing.
        if (lua_isnil(ls, -1))
        {
            lua_pop(ls, 1);
            return MB_FALSE;
        }

        bool stopact = lua_toboolean(ls, -1);
        lua_pop(ls, 1);
        if (stopact)
            return MB_TRUE;
    }

    if (delay == DELAY_MACRO && clua.callbooleanfn(true, "c_interrupt_macro",
                                                   "sA", interrupt_name, &at))
    {
        return MB_TRUE;
    }
#else
    UNUSED(_activity_interrupt_name);
#endif
    return MB_MAYBE;
}

// Returns true if the activity should be interrupted, false otherwise.
static bool _should_stop_activity(const delay_queue_item &item,
                                  activity_interrupt_type ai,
                                  const activity_interrupt_data &at)
{
    switch (_userdef_interrupt_activity(item, ai, at))
    {
    case MB_TRUE:
        return true;
    case MB_FALSE:
        return false;
    case MB_MAYBE:
        break;
    }

    // Don't interrupt player on monster's turn, they might wander off.
    if (you.turn_is_over
        && (at.context == SC_ALREADY_SEEN || at.context == SC_UNCHARM))
    {
        return false;
    }

    // No monster will attack you inside a sanctuary,
    // so presence of monsters won't matter.
    if (ai == AI_SEE_MONSTER && is_sanctuary(you.pos()))
        return false;

    delay_type curr = current_delay_action();

    if ((ai == AI_SEE_MONSTER || ai == AI_MIMIC) && player_stair_delay())
        return false;

    if (ai == AI_FULL_HP || ai == AI_FULL_MP)
    {
        if (Options.rest_wait_both && curr == DELAY_REST
            && (you.magic_points < you.max_magic_points
                || you.hp < you.hp_max))
        {
            return false;
        }
    }

    // Don't interrupt feeding or butchering for monsters already in view.
    if (_is_butcher_delay(curr) && ai == AI_SEE_MONSTER
        && testbits(at.mons_data->flags, MF_WAS_IN_VIEW))
    {
        return false;
    }

    return ai == AI_FORCE_INTERRUPT
           || Options.activity_interrupts[item.type][ai];
}

static string _abyss_monster_creation_message(const monster* mon)
{
    if (mon->type == MONS_DEATH_COB)
    {
        return coinflip() ? " appears in a burst of microwaves!"
                          : " pops from nullspace!";
    }

    return make_stringf(
        random_choose_weighted(
            17, " appears in a shower of translocational energy.",
            34, " appears in a shower of sparks.",
            45, " materialises.",
            13, " emerges from chaos.",
            26, " emerges from the beyond.",
            33, " assembles %s!",
             9, " erupts from nowhere!",
            18, " bursts from nowhere!",
             7, " is cast out of space!",
            14, " is cast out of reality!",
             5, " coalesces out of pure chaos.",
            10, " coalesces out of seething chaos.",
             2, " punctures the fabric of time!",
             7, " punctures the fabric of the universe.",
             3, " manifests%2$s!%1$.0s",
             0),
        mon->pronoun(PRONOUN_REFLEXIVE).c_str(),
        silenced(you.pos()) ? "" : " with a bang");
}

static inline bool _monster_warning(activity_interrupt_type ai,
                                    const activity_interrupt_data &at,
                                    delay_type atype,
                                    vector<string>* msgs_buf = nullptr)
{
    if (ai == AI_SENSE_MONSTER)
    {
        mprf(MSGCH_WARN, "You sense a monster nearby.");
        return true;
    }
    if (ai != AI_SEE_MONSTER)
        return false;
    if (!delay_is_run(atype) && !_is_butcher_delay(atype)
        && !(atype == DELAY_NOT_DELAYED))
    {
        return false;
    }
    if (at.context != SC_NEWLY_SEEN && atype == DELAY_NOT_DELAYED)
        return false;

    ASSERT(at.apt == AIP_MONSTER);
    monster* mon = at.mons_data;
    if (!you.can_see(mon))
        return false;

    // Disable message for summons.
    if (mon->is_summoned() && atype == DELAY_NOT_DELAYED)
        return false;

    if (at.context == SC_ALREADY_SEEN || at.context == SC_UNCHARM)
    {
        // Only say "comes into view" if the monster wasn't in view
        // during the previous turn.
        if (testbits(mon->flags, MF_WAS_IN_VIEW)
            && !(atype == DELAY_NOT_DELAYED))
        {
            mprf(MSGCH_WARN, "%s is too close now for your liking.",
                 mon->name(DESC_THE).c_str());
        }
    }
    else if (mon->seen_context == SC_JUST_SEEN)
        return false;
    else
    {
        ash_id_monster_equipment(mon);
        mark_mon_equipment_seen(mon);

        string text = getMiscString(mon->name(DESC_DBNAME) + " title");
        if (text.empty())
            text = mon->full_name(DESC_A);
        if (mon->type == MONS_PLAYER_GHOST)
        {
            text += make_stringf(" (%s)",
                                 short_ghost_description(mon).c_str());
        }
        set_auto_exclude(mon);

        if (at.context == SC_DOOR)
            text += " opens the door.";
        else if (at.context == SC_GATE)
            text += " opens the gate.";
        else if (at.context == SC_TELEPORT_IN)
            text += " appears from thin air!";
        else if (at.context == SC_LEAP_IN)
            text += " leaps into view!";
        // The monster surfaced and submerged in the same turn without
        // doing anything else.
        else if (at.context == SC_SURFACES_BRIEFLY)
            text += "surfaces briefly.";
        else if (at.context == SC_SURFACES)
            if (mon->type == MONS_AIR_ELEMENTAL)
                text += " forms itself from the air.";
            else if (mon->type == MONS_TRAPDOOR_SPIDER)
                text += " leaps out from its hiding place under the floor!";
            else
                text += " surfaces.";
        else if (at.context == SC_FISH_SURFACES_SHOUT
              || at.context == SC_FISH_SURFACES)
        {
            text += " bursts forth from the ";
            if (mons_primary_habitat(mon) == HT_LAVA)
                text += "lava";
            else if (mons_primary_habitat(mon) == HT_WATER)
                text += "water";
            else
                text += "realm of bugdom";
            text += ".";
        }
        else if (at.context == SC_NONSWIMMER_SURFACES_FROM_DEEP)
            text += " emerges from the water.";
        else if (at.context == SC_UPSTAIRS)
            text += " comes up the stairs.";
        else if (at.context == SC_DOWNSTAIRS)
            text += " comes down the stairs.";
        else if (at.context == SC_ARCH)
            text += " comes through the gate.";
        else if (at.context == SC_ABYSS)
            text += _abyss_monster_creation_message(mon);
        else if (at.context == SC_THROWN_IN)
            text += " is thrown into view!";
        else
            text += " comes into view.";

        bool ash_id = mon->props.exists("ash_id") && mon->props["ash_id"];
        bool zin_id = false;
        string god_warning;

        if (you_worship(GOD_ZIN)
            && mon->is_shapeshifter()
            && !(mon->flags & MF_KNOWN_SHIFTER))
        {
            ASSERT(!ash_id);
            zin_id = true;
            mon->props["zin_id"] = true;
            discover_shifter(mon);
            god_warning = "Zin warns you: "
                          + uppercase_first(mon->pronoun(PRONOUN_SUBJECTIVE))
                          + " is a foul ";
            if (mon->has_ench(ENCH_GLOWING_SHAPESHIFTER))
                god_warning += "glowing ";
            god_warning += "shapeshifter.";
        }

        monster_info mi(mon);

        const string mweap = get_monster_equipment_desc(mi,
                                                        ash_id ? DESC_IDENTIFIED
                                                               : DESC_WEAPON,
                                                        DESC_NONE);

        if (!mweap.empty())
        {
            if (ash_id)
                god_warning = "Ashenzari warns you:";

            (ash_id ? god_warning : text) +=
                " " + uppercase_first(mon->pronoun(PRONOUN_SUBJECTIVE)) + " is"
                + (ash_id ? " " : "")
                + mweap + ".";
        }

        if (msgs_buf)
            msgs_buf->push_back(text);
        else
        {
            mprf(MSGCH_MONSTER_WARNING, "%s", text.c_str());
            if (ash_id || zin_id)
                mprf(MSGCH_GOD, "%s", god_warning.c_str());
#ifndef USE_TILE_LOCAL
            if (zin_id)
                update_monster_pane();
#endif
            if (player_under_penance(GOD_GOZAG)
                && !mon->wont_attack()
                && !mon->is_stationary()
                && !mons_is_object(mon->type)
                && !mons_is_tentacle_or_tentacle_segment(mon->type))
            {
                if (coinflip()
                    && mon->get_experience_level() >=
                       random2(you.experience_level))
                {
                    mprf(MSGCH_GOD, GOD_GOZAG, "Gozag incites %s against you.",
                         mon->name(DESC_THE).c_str());
                    gozag_incite(mon);
                }
            }
        }
        if (player_mutation_level(MUT_SCREAM)
            && x_chance_in_y(3 + player_mutation_level(MUT_SCREAM) * 3, 100))
        {
            yell(mon);
        }
        mon->seen_context = SC_JUST_SEEN;
    }

    if (crawl_state.game_is_hints())
        hints_monster_seen(*mon);

    return true;
}

// Turns autopickup off if we ran into an invisible monster or saw a monster
// turn invisible.
// Turns autopickup on if we saw an invisible monster become visible or
// killed an invisible monster.
void autotoggle_autopickup(bool off)
{
    if (off)
    {
        if (Options.autopickup_on > 0)
        {
            Options.autopickup_on = -1;
            mprf(MSGCH_WARN,
                 "Deactivating autopickup; reactivate with <w>%s</w>.",
                 command_to_string(CMD_TOGGLE_AUTOPICKUP).c_str());
        }
        if (crawl_state.game_is_hints())
        {
            learned_something_new(HINT_INVISIBLE_DANGER);
            Hints.hints_seen_invisible = you.num_turns;
        }
    }
    else if (Options.autopickup_on < 0) // was turned off automatically
    {
        Options.autopickup_on = 1;
        mprf(MSGCH_WARN, "Reactivating autopickup.");
    }
}

// Returns true if any activity was stopped. Not reentrant.
bool interrupt_activity(activity_interrupt_type ai,
                        const activity_interrupt_data &at,
                        vector<string>* msgs_buf)
{
    if (interrupt_block::blocked())
        return false;

    const interrupt_block block_recursive_interrupts;
    if (ai == AI_HIT_MONSTER || ai == AI_MONSTER_ATTACKS)
    {
        const monster* mon = at.mons_data;
        if (mon && !mon->visible_to(&you) && !mon->submerged())
            autotoggle_autopickup(true);
    }

    if (crawl_state.is_repeating_cmd())
        return interrupt_cmd_repeat(ai, at);

    const delay_type delay = current_delay_action();

    if (delay == DELAY_NOT_DELAYED)
    {
        // Printing "[foo] comes into view." messages even when not
        // auto-exploring/travelling.
        if (ai == AI_SEE_MONSTER)
            return _monster_warning(ai, at, DELAY_NOT_DELAYED, msgs_buf);
        else
            return false;
    }

    // If we get hungry while traveling, let's try to auto-eat a chunk.
    if (ai == AI_HUNGRY && _auto_eat(delay) && prompt_eat_chunks(true) == 1)
        return false;

    dprf("Activity interrupt: %s", _activity_interrupt_name(ai));

    // First try to stop the current delay.
    const delay_queue_item &item = you.delay_queue.front();

    if (ai == AI_FULL_HP)
        mpr("HP restored.");
    else if (ai == AI_FULL_MP)
        mpr("Magic restored.");

    if (_should_stop_activity(item, ai, at))
    {
        _monster_warning(ai, at, item.type, msgs_buf);
        // Teleport stops stair delays.
        stop_delay(ai == AI_TELEPORT, ai == AI_MONSTER_ATTACKS);

        return true;
    }

    // Check the other queued delays; the first delay that is interruptible
    // will kill itself and all subsequent delays. This is so that a travel
    // delay stacked behind a delay such as stair/autopickup will be killed
    // correctly by interrupts that the simple stair/autopickup delay ignores.
    for (int i = 1, size = you.delay_queue.size(); i < size; ++i)
    {
        const delay_queue_item &it = you.delay_queue[i];
        if (_should_stop_activity(it, ai, at))
        {
            // Do we have a queued run delay? If we do, flush the delay queue
            // so that stop running Lua notifications happen.
            for (int j = i; j < size; ++j)
            {
                if (delay_is_run(you.delay_queue[j].type))
                {
                    _monster_warning(ai, at, you.delay_queue[j].type, msgs_buf);
                    stop_delay(ai == AI_TELEPORT);
                    return true;
                }
            }

            // Non-run queued delays can be discarded without any processing.
            you.delay_queue.erase(you.delay_queue.begin() + i,
                                  you.delay_queue.end());
            return true;
        }
    }

    return false;
}

// Must match the order of activity_interrupt_type in enum.h!
static const char *activity_interrupt_names[] =
{
    "force", "keypress", "full_hp", "full_mp", "statue", "hungry", "message",
    "hp_loss", "stat", "monster", "monster_attack", "teleport", "hit_monster",
    "sense_monster", "mimic"
};

static const char *_activity_interrupt_name(activity_interrupt_type ai)
{
    COMPILE_CHECK(ARRAYSZ(activity_interrupt_names) == NUM_AINTERRUPTS);

    if (ai == NUM_AINTERRUPTS)
        return "";

    return activity_interrupt_names[ai];
}

activity_interrupt_type get_activity_interrupt(const string &name)
{
    COMPILE_CHECK(ARRAYSZ(activity_interrupt_names) == NUM_AINTERRUPTS);

    for (int i = 0; i < NUM_AINTERRUPTS; ++i)
        if (name == activity_interrupt_names[i])
            return activity_interrupt_type(i);

    return NUM_AINTERRUPTS;
}

static const char *delay_names[] =
{
    "not_delayed", "eat", "vampire_feed", "armour_on", "armour_off",
    "jewellery_on", "memorise", "butcher", "bottle_blood",
#if TAG_MAJOR_VERSION == 34
    "weapon_swap",
#endif
    "passwall", "drop_item", "multidrop", "ascending_stairs",
    "descending_stairs",
#if TAG_MAJOR_VERSION == 34
    "recite",
#endif
    "run", "rest", "travel", "macro",
    "macro_process_key", "interruptible", "uninterruptible", "shaft self",
    "blurry vision",
};

// Gets a delay given its name.
// name must be lowercased already!
delay_type get_delay(const string &name)
{
    COMPILE_CHECK(ARRAYSZ(delay_names) == NUM_DELAYS);

    for (int i = 0; i < NUM_DELAYS; ++i)
    {
        if (name == delay_names[i])
            return delay_type(i);
    }

    // Also check American spellings:
    if (name == "armor_on")
        return DELAY_ARMOUR_ON;

    if (name == "armor_off")
        return DELAY_ARMOUR_OFF;

    if (name == "memorize")
        return DELAY_MEMORISE;

    if (name == "jewelry_on")
        return DELAY_JEWELLERY_ON;

    return NUM_DELAYS;
}

const char *delay_name(int delay)
{
    if (delay < 0 || delay >= NUM_DELAYS)
        return "";

    return delay_names[delay];
}
