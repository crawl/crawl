/**
 * @file
 * @brief Functions for handling multi-turn actions.
**/

#include "AppHdr.h"

#include "externs.h"
#include "options.h"

#include <stdio.h>
#include <string.h>

#include "abl-show.h"
#include "artefact.h"
#include "clua.h"
#include "command.h"
#include "coord.h"
#include "database.h"
#include "delay.h"
#include "describe.h"
#include "directn.h"
#include "exercise.h"
#include "enum.h"
#include "fprop.h"
#include "exclude.h"
#include "food.h"
#include "godabil.h"
#include "godpassive.h"
#include "godprayer.h"
#include "invent.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "notes.h"
#include "ouch.h"
#include "output.h"
#include "player.h"
#include "player-equip.h"
#include "random.h"
#include "religion.h"
#include "godconduct.h"
#include "shout.h"
#include "spl-other.h"
#include "spl-util.h"
#include "spl-selfench.h"
#include "stairs.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "env.h"
#include "transform.h"
#include "travel.h"
#include "hints.h"
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
static void _finish_delay(const delay_queue_item &delay);
static const char *_activity_interrupt_name(activity_interrupt_type ai);

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
    return (delay_is_run(delay)
            || delay == DELAY_MACRO
            || delay == DELAY_MULTIDROP);
}

static int _push_delay(const delay_queue_item &delay)
{
    for (delay_queue_type::iterator i = you.delay_queue.begin();
         i != you.delay_queue.end(); ++i)
    {
        if (_is_parent_delay(i->type))
        {
            size_t pos = i - you.delay_queue.begin();
            you.delay_queue.insert(i, delay);
            return pos;
        }
    }
    you.delay_queue.push_back(delay);
    return (you.delay_queue.size() - 1);
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

    // Paranoia
    if (type == DELAY_WEAPON_SWAP)
        you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;

    if (delay_is_run(type))
        clear_travel_trail();

    // Handle zero-turn delays (possible with butchering).
    if (turns == 0)
    {
        delay.started = true;
        // Don't issue startup message.
        if (_push_delay(delay) == 0)
            _finish_delay(delay);
        return;
    }
    _push_delay(delay);
}

static void _maybe_interrupt_swap(bool force_unsafe = false);

void stop_delay(bool stop_stair_travel, bool force_unsafe)
{
    if (you.delay_queue.empty())
        return;

    set_more_autoclear(false);

    ASSERT(!crawl_state.game_is_arena());

    delay_queue_item delay = you.delay_queue.front();

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
        // Corpse keeps track of work in plus2 field, see handle_delay(). - bwr
        bool multiple_corpses    = false;

        for (unsigned int i = 1; i < you.delay_queue.size(); ++i)
            if (you.delay_queue[i].type == DELAY_BUTCHER
                || you.delay_queue[i].type == DELAY_BOTTLE_BLOOD)
            {
                multiple_corpses = true;
                break;
            }

        const string butcher_verb =
                (delay.type == DELAY_BUTCHER      ? "butchering" :
                 delay.type == DELAY_BOTTLE_BLOOD ? "bottling blood from"
                                                  : "sacrificing");

        mprf("You stop %s the corpse%s.", butcher_verb.c_str(),
             multiple_corpses ? "s" : "");

        _pop_delay();

        _maybe_interrupt_swap(force_unsafe);
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
        if (delay.parm1
            || (item.defined()
                && item.base_type == OBJ_CORPSES
                && item.pos == you.pos()))
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

    case DELAY_WEAPON_SWAP:       // one turn... too much trouble
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
    return (delay == DELAY_BUTCHER
            || delay == DELAY_BOTTLE_BLOOD
            || delay == DELAY_FEED_VAMPIRE);
}

void stop_butcher_delay()
{
    if (_is_butcher_delay(current_delay_action()))
        stop_delay();
}

void maybe_clear_weapon_swap()
{
    if (form_can_wield())
        you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;
}

void handle_interrupted_swap(bool swap_if_safe, bool force_unsafe)
{
    if (!you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED]
        || !you_tran_can_wear(EQ_WEAPON) || you.cannot_act() || you.berserk())
    {
        return;
    }

    // Decrease value by 1. (0 means attribute is false, 1 = a, 2 = b, ...)
    int weap = you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] - 1;
    if (weap == ENDOFPACK)
        weap = -1;

    const bool       safe   = i_feel_safe() && !force_unsafe;
    const bool       prompt = Options.prompt_for_swap && !safe;
    const delay_type delay  = current_delay_action();

    const char* prompt_str  = "Switch back to main weapon?";

    // If we're going to prompt then update the window so the player can
    // see what the monsters are.
    if (prompt)
        viewwindow();

    if (delay == DELAY_WEAPON_SWAP)
        die("handle_interrupted_swap() called while already swapping weapons");
    else if (!you.turn_is_over
             && (delay == DELAY_ASCENDING_STAIRS
                 || delay == DELAY_DESCENDING_STAIRS))
    {
        // We just arrived on the level, let rest of function do its stuff.
        ;
    }
    else if (you.turn_is_over && delay == DELAY_NOT_DELAYED)
    {
        // Turn is over, set up a delay to do swapping next turn.
        if (prompt && yesno(prompt_str, true, 'n', true, false)
            || safe && swap_if_safe)
        {
            if (weap == -1 || check_warning_inscriptions(you.inv[weap], OPER_WIELD))
                start_delay(DELAY_WEAPON_SWAP, 1, weap);
            you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;
        }
        return;
    }
    else if (delay != DELAY_NOT_DELAYED)
    {
        // If ATTR_WEAPON_SWAP_INTERRUPTED is set while a corpse is being
        // butchered/bottled/offered, then fake a weapon swap delay.
        if (_is_butcher_delay(delay)
            && (safe || prompt && yesno(prompt_str, true, 'n', true, false)))
        {
            if (weap == -1 || check_warning_inscriptions(you.inv[weap], OPER_WIELD))
                start_delay(DELAY_WEAPON_SWAP, 1, weap);
            you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;
        }
        return;
    }

    if (safe)
    {
        if (!swap_if_safe)
            return;
    }
    else if (!prompt || !yesno(prompt_str, true, 'n', true, false))
        return;

    if (weap == -1 || check_warning_inscriptions(you.inv[weap], OPER_WIELD))
    {
        weapon_switch(weap);
        print_stats();
    }
    you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;
}

static void _maybe_interrupt_swap(bool force_unsafe)
{
    bool butcher_swap_setup  = false;
    int  butcher_swap_weapon = 0;

    for (unsigned int i = 1; i < you.delay_queue.size(); ++i)
        if (you.delay_queue[i].type == DELAY_WEAPON_SWAP)
        {
            butcher_swap_weapon = you.delay_queue[i].parm1;
            butcher_swap_setup  = true;
            break;
        }

    if (!butcher_swap_setup && delays_cleared[DELAY_WEAPON_SWAP] > 0)
    {
        butcher_swap_setup  = true;
        butcher_swap_weapon = cleared_delays_parm1[DELAY_WEAPON_SWAP];
    }

    if (butcher_swap_setup)
    {
        // Use weapon slot + 1, so weapon slot 'a' (== 0) doesn't
        // return false when checking if
        // you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED].
        you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED]
            = (butcher_swap_weapon == -1 ? ENDOFPACK
                                         : butcher_swap_weapon) + 1;

        // Possibly prompt if user wants to switch back from
        // butchering tool in order to use their normal weapon to
        // fight the interrupting monster.
        handle_interrupted_swap(true, force_unsafe);
    }
}

bool you_are_delayed(void)
{
    return !you.delay_queue.empty();
}

delay_type current_delay_action(void)
{
    return (you_are_delayed() ? you.delay_queue.front().type
                              : DELAY_NOT_DELAYED);
}

bool delay_is_run(delay_type delay)
{
    return (delay == DELAY_RUN || delay == DELAY_REST || delay == DELAY_TRAVEL);
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

    for (unsigned int i = 0; i < you.delay_queue.size(); ++i)
    {
        if (you.delay_queue[i].type == DELAY_BUTCHER
            || you.delay_queue[i].type == DELAY_BOTTLE_BLOOD)
        {
            const item_def &corpse = mitm[ you.delay_queue[i].parm1 ];
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
    return (delay.type == DELAY_FEED_VAMPIRE);
}

bool is_butchering()
{
    if (!you_are_delayed())
        return false;

    const delay_queue_item &delay = you.delay_queue.front();
    return (delay.type == DELAY_BUTCHER || delay.type == DELAY_BOTTLE_BLOOD);
}

bool player_stair_delay()
{
    if (!you_are_delayed())
        return false;

    const delay_queue_item &delay = you.delay_queue.front();
    return (delay.type == DELAY_ASCENDING_STAIRS
            || delay.type == DELAY_DESCENDING_STAIRS);
}

bool already_learning_spell(int spell)
{
    if (!you_are_delayed())
        return false;

    for (unsigned int i = 0; i < you.delay_queue.size(); ++i)
    {
        if (you.delay_queue[i].type != DELAY_MEMORISE)
            continue;

        if (spell == -1 || you.delay_queue[i].parm1 == spell)
            return true;
    }
    return false;
}

// Xom is amused by a potential food source going to waste, and is
// more amused the hungrier you are.
static void _xom_check_corpse_waste()
{
    const int food_need = max(7000 - you.hunger, 0);
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
            mpr("You start putting on your armour.", MSGCH_MULTITURN_ACTION);
            break;

        case DELAY_ARMOUR_OFF:
            mpr("You start removing your armour.", MSGCH_MULTITURN_ACTION);
            break;

        case DELAY_BUTCHER:
        case DELAY_BOTTLE_BLOOD:
            if (!mitm[delay.parm1].defined())
                break;

            if (delay.type == DELAY_BOTTLE_BLOOD)
            {
                mprf(MSGCH_MULTITURN_ACTION,
                     "You start bottling blood from %s.",
                     mitm[delay.parm1].name(DESC_THE).c_str());
            }
            else
            {
                string tool;
                switch (delay.parm3)
                {
                case SLOT_BUTCHERING_KNIFE: tool = "knife"; break;
                case SLOT_CLAWS:            tool = "claws"; break;
                case SLOT_TEETH:            tool = "teeth"; break;
                case SLOT_BIRDIE:           tool = "beak and talons"; break;
                default: tool = you.inv[delay.parm3].name(DESC_QUALNAME);
                }
                mprf(MSGCH_MULTITURN_ACTION,
                     "You start butchering %s with your %s.",
                     mitm[delay.parm1].name(DESC_THE).c_str(), tool.c_str());
            }
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
            mpr("You start memorising the spell.", MSGCH_MULTITURN_ACTION);
            break;
        }

        case DELAY_PASSWALL:
            mpr("You begin to meditate on the wall.", MSGCH_MULTITURN_ACTION);
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
        // Vampires stop feeding if ...
        // * engorged ("alive")
        // * bat form runs out due to becoming full
        // * corpse becomes poisonous as the Vampire loses poison resistance
        // * corpse disappears for some reason (e.g. animated by a monster)
        if ((!delay.parm1                                         // on floor
             && ( !(mitm[ delay.parm2 ].defined())                // missing
                 || mitm[ delay.parm2 ].base_type != OBJ_CORPSES  // noncorpse
                 || mitm[ delay.parm2 ].pos != you.pos()) )       // elsewhere
            || you.hunger_state == HS_ENGORGED
            || you.hunger_state > HS_SATIATED && you.form == TRAN_BAT
            || (you.hunger_state >= HS_SATIATED
               && mitm[delay.parm2].defined()
               && is_poisonous(mitm[delay.parm2])) )
        {
            // Messages handled in _food_change() in food.cc.
            stop_delay();
            return;
        }
    }
    else if (delay.type == DELAY_BUTCHER || delay.type == DELAY_BOTTLE_BLOOD)
    {
        if (delay.type == DELAY_BOTTLE_BLOOD && you.experience_level < 6)
        {
            mpr("You cannot bottle blood anymore!");
            stop_delay();
            return;
        }

        // A monster may have raised the corpse you're chopping up! -- bwr
        // Note that a monster could have raised the corpse and another
        // monster could die and create a corpse with the same ID number...
        // However, it would not be at the player's square like the
        // original and that's why we do it this way.
        if (mitm[delay.parm1].defined()
            && mitm[ delay.parm1 ].base_type == OBJ_CORPSES
            && mitm[ delay.parm1 ].pos == you.pos())
        {
            if (mitm[ delay.parm1 ].sub_type == CORPSE_SKELETON)
            {
                mpr("The corpse rots away into a skeleton!");
                if (delay.type == DELAY_BUTCHER
                    || delay.type == DELAY_BOTTLE_BLOOD) // Shouldn't happen.
                {
                    if (player_mutation_level(MUT_SAPROVOROUS) == 3)
                        _xom_check_corpse_waste();
                    else
                        xom_is_stimulated(25);
                    delay.duration = 0;
                }
                else
                {
                    // Don't attempt to offer a skeleton.
                    _pop_delay();
                    return;
                }
            }
            else
            {
                if (food_is_rotten(mitm[delay.parm1]))
                {
                    // Only give the rotting message if the corpse wasn't
                    // previously rotten. (special < 100 is the rottenness check).
                    if (delay.parm2 >= 100)
                    {
                        mpr("The corpse rots.", MSGCH_ROTTEN_MEAT);
                        if (you.is_undead != US_UNDEAD
                            && player_mutation_level(MUT_SAPROVOROUS) < 3)
                        {
                            _xom_check_corpse_waste();
                        }
                    }

                    delay.parm2 = 99; // Don't give the message twice.

                    // Vampires won't continue bottling rotting corpses.
                    if (delay.type == DELAY_BOTTLE_BLOOD)
                    {
                        mpr("You stop bottling this corpse's foul-smelling "
                            "blood!");
                        _pop_delay();
                        return;
                    }
                }

                // Mark work done on the corpse in case we stop. -- bwr
                mitm[ delay.parm1 ].plus2++;
            }
        }
        else
        {
            // Corpse is no longer valid!  End the butchering normally
            // instead of using stop_delay(), so that the player
            // switches back to their main weapon if necessary.
            delay.duration = 0;
        }
    }
    else if (delay.type == DELAY_MULTIDROP)
    {
        // Throw away invalid items; items usually go invalid because
        // of chunks rotting away.
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

        case DELAY_BUTCHER:
            mprf(MSGCH_MULTITURN_ACTION, "You continue butchering the corpse.");
            break;

        case DELAY_BOTTLE_BLOOD:
            mprf(MSGCH_MULTITURN_ACTION, "You continue bottling blood from "
                                         "the corpse.");
            break;

        case DELAY_JEWELLERY_ON:
        case DELAY_WEAPON_SWAP:
            // These are 1-turn delays where the time cost is handled
            // in _finish_delay().
            // FIXME: get rid of this hack!
            you.time_taken = 0;
            break;

        case DELAY_MEMORISE:
            mpr("You continue memorising.", MSGCH_MULTITURN_ACTION);
            break;

        case DELAY_PASSWALL:
            mpr("You continue meditating on the rock.",
                MSGCH_MULTITURN_ACTION);
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
            mpr("You continue eating.", MSGCH_MULTITURN_ACTION);
            break;

        case DELAY_FEED_VAMPIRE:
        {
            item_def &corpse = (delay.parm1 ? you.inv[delay.parm2]
                                            : mitm[delay.parm2]);
            if (food_is_rotten(corpse))
            {
                mpr("This corpse has started to rot.", MSGCH_ROTTEN_MEAT);
                _xom_check_corpse_waste();
                stop_delay();
                return;
            }
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
    case DELAY_WEAPON_SWAP:
        weapon_switch(delay.parm1);
        you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;
        break;

    case DELAY_JEWELLERY_ON:
        puton_ring(delay.parm1);
        break;

    case DELAY_ARMOUR_ON:
        _armour_wear_effects(delay.parm1);
        // If butchery (parm2), autopickup chunks.
        if (Options.chunks_autopickup && delay.parm2)
            autopickup();
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
            mprf("You finish eating.");
        // For chunks, warn the player if they're not getting much
        // nutrition. Also, print the other eating messages only now.
        if (delay.parm1)
            chunk_nutrition_message(delay.parm1);
        else if (delay.parm2 != -1)
            finished_eating_message(delay.parm2);
        break;

    case DELAY_FEED_VAMPIRE:
    {
        mprf("You finish drinking.");

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
                item_check(false);
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
                if (!shift_monster(m) && !monster_blink(m, true))
                    monster_teleport(m, true, true);
                // Might still fail.
                if (monster_at(pass))
                {
                    mpr("...and sense your way blocked. You quickly turn back.");
                    goto passwall_aborted;
                }

                move_player_to_grid(pass, false, true);

                // Wake the monster if it's asleep.
                if (m)
                    behaviour_event(m, ME_ALERT, &you);
            }
            else
                move_player_to_grid(pass, false, true);

        passwall_aborted:
            redraw_screen();
        }
        break;
    }

    case DELAY_BUTCHER:
    case DELAY_BOTTLE_BLOOD:
    {
        item_def &item = mitm[delay.parm1];
        if (item.defined() && item.base_type == OBJ_CORPSES)
        {
            if (item.sub_type == CORPSE_SKELETON)
            {
                mprf("The corpse rots away into a skeleton just before you "
                     "finish %s!",
                     (delay.type == DELAY_BOTTLE_BLOOD ? "bottling its blood"
                                                       : "butchering"));

                if (player_mutation_level(MUT_SAPROVOROUS) == 3)
                    _xom_check_corpse_waste();
                else
                    xom_is_stimulated(50);

                break;
            }

            if (delay.type == DELAY_BOTTLE_BLOOD)
            {
                mpr("You finish bottling this corpse's blood.");

                const bool was_orc = (mons_genus(item.mon_type) == MONS_ORC);
                const bool was_holy = (mons_class_holiness(item.mon_type) == MH_HOLY);

                if (mons_skeleton(item.mon_type) && one_chance_in(3))
                    turn_corpse_into_skeleton_and_blood_potions(item);
                else
                    turn_corpse_into_blood_potions(item);

                if (was_orc)
                    did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);
                if (was_holy)
                    did_god_conduct(DID_DESECRATE_HOLY_REMAINS, 2);
            }
            else
            {
                mprf("You finish %s %s into pieces.",
                     delay.parm3 <= SLOT_CLAWS ? "ripping" : "chopping",
                     mitm[delay.parm1].name(DESC_THE).c_str());

                if (god_hates_cannibalism(you.religion)
                    && is_player_same_genus(item.mon_type))
                {
                    simple_god_message(" expects more respect for your"
                                       " departed relatives.");
                }
                else if (is_good_god(you.religion)
                    && mons_class_holiness(item.mon_type) == MH_HOLY)
                {
                    simple_god_message(" expects more respect for holy"
                                       " creatures!");
                }
                else if (you_worship(GOD_ZIN)
                         && mons_class_intel(item.mon_type) >= I_NORMAL)
                {
                    simple_god_message(" expects more respect for this"
                                       " departed soul.");
                }

                const bool was_orc = (mons_genus(item.mon_type) == MONS_ORC);
                const bool was_holy = (mons_class_holiness(item.mon_type) == MH_HOLY);

                butcher_corpse(item);

                if (you.berserk()
                    && you.berserk_penalty != NO_BERSERK_PENALTY)
                {
                    mpr("You enjoyed that.");
                    you.berserk_penalty = 0;
                }

                if (was_orc)
                    did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);
                if (was_holy)
                    did_god_conduct(DID_DESECRATE_HOLY_REMAINS, 2);
            }

            // Don't autopickup chunks/potions if there's still another
            // delay (usually more corpses to butcher or a weapon-swap)
            // waiting to happen.
            // Also, don't waste time picking up chunks if you're already
            // starving. (jpeg)
            if ((Options.chunks_autopickup
                    || delay.type == DELAY_BOTTLE_BLOOD)
                && you.delay_queue.size() == 1)
            {
                if (you.hunger_state > HS_STARVING || you.species == SP_VAMPIRE)
                    autopickup();
            }

            _maybe_interrupt_swap();
        }
        else
        {
            mprf("You stop %s.",
                 delay.type == DELAY_BUTCHER ? "butchering the corpse"
                                             : "bottling this corpse's blood");
            _pop_delay();
        }
        StashTrack.update_stash(you.pos()); // Stash-track the generated items.
        break;
    }

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

void finish_last_delay()
{
    delay_queue_item &delay = you.delay_queue.front();
    _finish_delay(delay);
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
        you.start_train.insert(SK_SHIELDS);
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

        return CMD_MOVE_NOWHERE;
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
                pray();
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
            mesclr();
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
    for (delay_queue_type::iterator i = you.delay_queue.begin();
         i != you.delay_queue.end(); ++i)
    {
        if (i->type == delay)
        {
            if (i->duration > 0)
                --i->duration;
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
        mpr("Lua not initialised", MSGCH_DIAGNOSTICS);
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
    return !(delay == DELAY_EAT || delay == DELAY_WEAPON_SWAP
             || delay == DELAY_DROP_ITEM || delay == DELAY_JEWELLERY_ON
             || delay == DELAY_UNINTERRUPTIBLE);
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

    if (ai == AI_SEE_MONSTER && player_stair_delay())
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

    // Don't interrupt butchering for monsters already in view.
    const monster* mon = static_cast<const monster* >(at.data);
    if (_is_butcher_delay(curr) && ai == AI_SEE_MONSTER
        && testbits(mon->flags, MF_WAS_IN_VIEW))
    {
        return false;
    }

    return (ai == AI_FORCE_INTERRUPT
            || Options.activity_interrupts[item.type][ai]);
}

static inline bool _monster_warning(activity_interrupt_type ai,
                                    const activity_interrupt_data &at,
                                    delay_type atype,
                                    vector<string>* msgs_buf = NULL)
{
    if (ai == AI_SENSE_MONSTER)
    {
        mpr("You sense a monster nearby.", MSGCH_WARN);
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

    const monster* mon = static_cast<const monster* >(at.data);
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
        string text = mon->full_name(DESC_A);
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
        else
            text += " comes into view.";

        ash_id_monster_equipment(const_cast<monster* >(mon));
        bool ash_id = mon->props.exists("ash_id") && mon->props["ash_id"];
        string ash_warning;

        monster_info mi(mon);

        const string mweap = get_monster_equipment_desc(mi,
                                                        ash_id ? DESC_IDENTIFIED
                                                               : DESC_WEAPON,
                                                        DESC_NONE);

        if (!mweap.empty())
        {
            if (ash_id)
                ash_warning = "Ashenzari warns you:";

            (ash_id ? ash_warning : text) +=
                " " + uppercase_first(mon->pronoun(PRONOUN_SUBJECTIVE)) + " is"
                + mweap + ".";
        }

        if (msgs_buf)
            msgs_buf->push_back(text);
        else
        {
            mpr(text, MSGCH_WARN);
            if (ash_id)
                mpr(ash_warning, MSGCH_GOD);
        }
        const_cast<monster* >(mon)->seen_context = SC_JUST_SEEN;
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
        const monster* mon = static_cast<const monster* >(at.data);
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
        mprf("%s restored.", you.species == SP_DJINNI ? "EP" : "HP");
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

static const char *activity_interrupt_names[] =
{
    "force", "keypress", "full_hp", "full_mp", "statue",
    "hungry", "message", "hp_loss", "burden", "stat",
    "monster", "monster_attack", "teleport", "hit_monster", "sense_monster"
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
    "jewellery_on", "memorise", "butcher", "bottle_blood", "weapon_swap",
    "passwall", "drop_item", "multidrop", "ascending_stairs",
    "descending_stairs",
#if TAG_MAJOR_VERSION == 34
    "recite",
#endif
    "run", "rest", "travel", "macro",
    "macro_process_key", "interruptible", "uninterruptible"
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
