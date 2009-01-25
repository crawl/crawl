/*
 *  File:     delay.cc
 *  Summary:  Functions for handling multi-turn actions.
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include "externs.h"

#include <stdio.h>
#include <string.h>

#include "clua.h"
#include "command.h"
#include "database.h"
#include "delay.h"
#include "describe.h"
#include "enum.h"
#include "food.h"
#include "invent.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "it_use2.h"
#include "message.h"
#include "misc.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mstuff2.h"
#include "notes.h"
#include "ouch.h"
#include "output.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "spells4.h"
#include "spl-util.h"
#include "stash.h"
#include "state.h"
#include "stuff.h"
#include "transfor.h"
#include "travel.h"
#include "tutorial.h"
#include "view.h"
#include "xom.h"

extern std::vector<SelItem> items_for_multidrop;

static int  _interrupts_blocked = 0;

static void _xom_check_corpse_waste();
static void _handle_run_delays(const delay_queue_item &delay);
static void _handle_macro_delay();
static void _finish_delay(const delay_queue_item &delay);

// Monsters cannot be affected in these states.
// (All results of Recite, plus stationary and friendly + stupid;
//  note that berserk monsters are also hasted.)
static bool _recite_mons_useless(const monsters *mon)
{
    const mon_holy_type holiness = mons_holiness(mon);

    return (mons_intel(mon) < I_NORMAL
            || holiness != MH_HOLY
               && holiness != MH_NATURAL
               && holiness != MH_UNDEAD
               && holiness != MH_DEMONIC
            || mons_is_stationary(mon)
            || mons_is_fleeing(mon)
            || mons_is_sleeping(mon)
            || mons_wont_attack(mon)
            || mons_neutral(mon)
            || mons_is_confused(mon)
            || mons_is_paralysed(mon)
            || mon->has_ench(ENCH_BATTLE_FRENZY)
            || mon->has_ench(ENCH_HASTE));
}

// Power is maximum 50.
static int _recite_to_monsters(coord_def where, int pow, int, actor *)
{
    const int mon = mgrd(where);
    if (mon == NON_MONSTER)
        return (0);

    monsters *mons = &menv[mon];

    if (_recite_mons_useless(mons))
        return (0);

    if (coinflip()) // nothing happens
        return (0);

    int resist;
    const mon_holy_type holiness = mons_holiness(mons);

    if (holiness == MH_HOLY)
        resist = std::max(0, 7 - random2(you.skills[SK_INVOCATIONS]));
    else
    {
        resist = mons_resist_magic(mons);

        if (holiness == MH_UNDEAD)
            pow -= 2 + random2(3);
        else if (holiness == MH_DEMONIC)
            pow -= 3 + random2(5);
    }

    pow -= resist;

    if (pow > 0)
        pow = random2avg(pow, 2);

    if (pow <= 0) // Uh oh...
    {
        if (one_chance_in(resist + 1))
            return (0);  // nothing happens, whew!

        if (!one_chance_in(4) &&
             mons->add_ench(mon_enchant(ENCH_HASTE, 0, KC_YOU,
                            (16 + random2avg(13, 2)) * 10)))
        {
            simple_monster_message(mons, " speeds up in annoyance!");
        }
        else if (!one_chance_in(3)
                 && mons->add_ench(mon_enchant(ENCH_BATTLE_FRENZY, 1, KC_YOU,
                                              (16 + random2avg(13, 2)) * 10)))
        {
            simple_monster_message(mons, " goes into a battle-frenzy!");
        }
        else if (mons->can_go_berserk())
            mons->go_berserk(true);
        else
            return (0); // nothing happens

        // Bad effects stop the recital.
        stop_delay();
        return (1);
    }

    switch (pow)
    {
        case 0:
            return (0); // handled above
        case 1:
        case 2:
        case 3:
        case 4:
            if (!mons_class_is_confusable(mons->type)
                || !mons->add_ench(mon_enchant(ENCH_CONFUSION, 0, KC_YOU,
                                               (16 + random2avg(13, 2)) * 10)))
            {
                return (0);
            }
            simple_monster_message(mons, " looks confused.");
            break;
        case 5:
        case 6:
        case 7:
        case 8:
            mons->put_to_sleep();
            simple_monster_message(mons, " falls asleep!");
            break;
        case 9:
        case 10:
        case 11:
        case 12:
            if (!mons->add_ench(mon_enchant(ENCH_NEUTRAL, 0, KC_YOU,
                                (16 + random2avg(13, 2)) * 10)))
            {
                return (0);
            }
            simple_monster_message(mons, " seems impressed!");
            break;
        case 13:
        case 14:
        case 15:
            if (!mons->add_ench(ENCH_FEAR))
                return (0);
            simple_monster_message(mons, " turns to flee.");
            break;
        case 16:
        case 17:
            if (!mons->add_ench(mon_enchant(ENCH_PARALYSIS, 0, KC_YOU,
                                (16 + random2avg(13, 2)) * 10)))
            {
                return (0);
            }
            simple_monster_message(mons, " freezes in fright!");
            break;
        default:
            if (holiness == MH_HOLY)
                good_god_holy_attitude_change(mons);
            else
            {
                if (holiness == MH_UNDEAD || holiness == MH_DEMONIC)
                {
                    if (!mons->add_ench(mon_enchant(ENCH_NEUTRAL, 0, KC_YOU,
                                        (16 + random2avg(13, 2)) * 10)))
                    {
                        return (0);
                    }
                    simple_monster_message(mons, " seems impressed!");
                }
                else
                {
                    simple_monster_message(mons, " seems fully impressed!");
                    mons_pacify(mons);
                }
            }
            break;
    }

    return (1);
}

static std::string _get_recite_speech(const std::string key, int weight)
{
    seed_rng(weight + you.pos().x + you.pos().y);
    const std::string str = getSpeakString("zin_recite_speech_" + key);

    if (str.empty())
    {
        // In case nothing is found.
        if (key == "start")
            return ("begin reciting the Axioms of Law.");

        return ("reciting");
    }

    return (str);
}

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
    return (delay == DELAY_TRAVEL
            || delay == DELAY_MACRO
            || delay == DELAY_MULTIDROP);
}

static int _push_delay(const delay_queue_item &delay)
{
    for (delay_queue_type::iterator i = you.delay_queue.begin();
         i != you.delay_queue.end(); ++i)
    {
        if (_is_parent_delay( i->type ))
        {
            you.delay_queue.insert(i, delay);
            return (i - you.delay_queue.begin());
        }
    }
    you.delay_queue.push_back( delay );
    return (you.delay_queue.size() - 1);
}

static void _pop_delay()
{
    if (!you.delay_queue.empty())
        you.delay_queue.erase( you.delay_queue.begin() );
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

        if (is_run_delay(delay.type) && you.running)
            stop_running();
    }
}

void start_delay( delay_type type, int turns, int parm1, int parm2 )
{
    ASSERT(!crawl_state.arena);
    ASSERT(!crawl_state.is_repeating_cmd() || type == DELAY_MACRO);

    _interrupts_blocked = 0; // Just to be safe

    delay_queue_item delay;

    delay.type     = type;
    delay.duration = turns;
    delay.parm1    = parm1;
    delay.parm2    = parm2;
    delay.started  = false;

    // Paranoia
    if (type == DELAY_WEAPON_SWAP)
        you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;

    // Handle zero-turn delays (possible with butchering).
    if (turns == 0)
    {
        delay.started = true;
        // Don't issue startup message.
        if (_push_delay(delay) == 0)
            _finish_delay(delay);
        return;
    }
    _push_delay( delay );
}

void stop_delay( bool stop_stair_travel )
{
    _interrupts_blocked = 0; // Just to be safe

    if (you.delay_queue.empty())
        return;

    reset_more_autoclear();

    ASSERT(!crawl_state.arena);

    delay_queue_item delay = you.delay_queue.front();

    ASSERT(!crawl_state.is_repeating_cmd() || delay.type == DELAY_MACRO);

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
    case DELAY_OFFER_CORPSE:
    {
        // Corpse keeps track of work in plus2 field, see handle_delay(). -- bwr
        bool multiple_corpses    = false;
        bool butcher_swap_setup  = false;
        int  butcher_swap_weapon = 0;

        for (unsigned int i = 1; i < you.delay_queue.size(); i++)
        {
            if (you.delay_queue[i].type == DELAY_BUTCHER
                || you.delay_queue[i].type == DELAY_BOTTLE_BLOOD
                || you.delay_queue[i].type == DELAY_OFFER_CORPSE)
            {
                multiple_corpses = true;
            }
            else if (you.delay_queue[i].type == DELAY_WEAPON_SWAP)
            {
                butcher_swap_weapon = you.delay_queue[i].parm1;
                butcher_swap_setup  = true;
                break;
            }
            else
                break;
        }

        if (!butcher_swap_setup && delays_cleared[DELAY_WEAPON_SWAP] > 0)
        {
            butcher_swap_setup  = true;
            butcher_swap_weapon = cleared_delays_parm1[DELAY_WEAPON_SWAP];
        }

        const std::string butcher_verb =
                (delay.type == DELAY_BUTCHER      ? "butchering" :
                 delay.type == DELAY_BOTTLE_BLOOD ? "bottling blood from"
                                                  : "sacrificing");

        mprf("You stop %s the corpse%s.", butcher_verb.c_str(),
             multiple_corpses ? "s" : "");

        _pop_delay();

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
            if (!i_feel_safe())
                handle_interrupted_swap(false, true);
        }

        break;
    }
    case DELAY_MEMORISE:
        // Losing work here is okay... having to start from
        // scratch is a reasonable behaviour. -- bwr
        mpr( "Your memorisation is interrupted." );
        _pop_delay();
        break;

    case DELAY_MULTIDROP:
        // No work lost
        if (!items_for_multidrop.empty())
            mpr( "You stop dropping stuff." );
        _pop_delay();
        break;

    case DELAY_RECITE:
        mprf(MSGCH_PLAIN, "You stop %s.",
             _get_recite_speech("other", you.num_turns + delay.duration).c_str());
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
        if (is_run_delay(delay.type) && you.running)
            stop_running();

        // There's no special action needed for macros - if we don't call out
        // to the Lua function, it can't do damage.
        break;

    case DELAY_INTERRUPTIBLE:
        // Always stoppable by definition...
        // try using a more specific type anyways. -- bwr
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
        _xom_check_corpse_waste();

        item_def &corpse = (delay.parm1 ? you.inv[delay.parm2]
                                        : mitm[delay.parm2]);

        mpr("All blood oozes out of the corpse!");
        bleed_onto_floor(you.pos(), corpse.plus, delay.duration, false);

        if (mons_skeleton(corpse.plus) && one_chance_in(3))
            turn_corpse_into_skeleton(corpse);
        else
        {
            if (delay.parm1)
                dec_inv_item_quantity(delay.parm2, 1);
            else
                dec_mitm_item_quantity(delay.parm2, 1);
        }

        did_god_conduct(DID_DRINK_BLOOD, 8);
        delay.duration = 0;
        _pop_delay();
        handle_delay();
        return;
    }
    case DELAY_ARMOUR_ON:
    case DELAY_ARMOUR_OFF:
        // These two have the default action of not being interruptible,
        // although they will often consist of chained intermediary steps
        // (remove cloak, remove armour, wear new armour, replace cloak),
        // all of which can be stopped when complete.  This is a fairly
        // reasonable behaviour, although perhaps the character should have
        // the option of reversing the current action if it would take less
        // time to get out of the plate mail that's half on than it would
        // take to continue.  Probably too much trouble, and we'd have to
        // have a prompt... this works just fine. -- bwr
        break;

    case DELAY_ASCENDING_STAIRS:  // short... and probably what people want
    case DELAY_DESCENDING_STAIRS: // short... and probably what people want
         if (stop_stair_travel)
         {
#ifdef DEBUG_DIAGNOSTICS
             mpr("Stop ascending/descending stairs.");
#endif
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

    if (is_run_delay(delay.type))
        update_turn_count();
}

void stop_butcher_delay()
{
    if (current_delay_action() == DELAY_BUTCHER
        || current_delay_action() == DELAY_BOTTLE_BLOOD
        || current_delay_action() == DELAY_OFFER_CORPSE)
    {
        stop_delay();
    }
}

static bool _is_butcher_delay(int delay)
{
    return (delay == DELAY_BUTCHER || delay == DELAY_BOTTLE_BLOOD
            || delay == DELAY_OFFER_CORPSE);
}

void handle_interrupted_swap(bool swap_if_safe, bool force_unsafe,
                             bool transform)
{
    if (!you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED]
        || !you_tran_can_wear(EQ_WEAPON) || you.cannot_act())
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

    const char* prompt_str  = transform ? "Switch back to main weapon?"
                                        : "Switch back from butchering tool?";

    // If we're going to prompt then update the window so the player can
    // see what the monsters are.
    if (prompt)
        viewwindow(true, false);

    if (delay == DELAY_WEAPON_SWAP)
    {
        ASSERT(!"handle_interrupted_swap() called while already swapping "
                "weapons");
        you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;
        return;
    }
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
        if (prompt && yesno(prompt_str, true, 'n') || safe && swap_if_safe)
        {
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
            && (safe || prompt && yesno(prompt_str, true, 'n')))
        {
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
    else if (!prompt || !yesno(prompt_str, true, 'n'))
    {
        return;
    }

    weapon_switch(weap);
    print_stats();

    you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;
}

bool you_are_delayed( void )
{
    return (!you.delay_queue.empty());
}

delay_type current_delay_action( void )
{
    return (you_are_delayed() ? you.delay_queue.front().type
                              : DELAY_NOT_DELAYED);
}

bool is_run_delay(int delay)
{
    return (delay == DELAY_RUN || delay == DELAY_REST || delay == DELAY_TRAVEL);
}

bool is_being_butchered(const item_def &item, bool just_first)
{
    if (!you_are_delayed())
        return (false);

    for (unsigned int i = 0; i < you.delay_queue.size(); i++)
    {
        if (you.delay_queue[i].type == DELAY_BUTCHER
            || you.delay_queue[i].type == DELAY_BOTTLE_BLOOD
            || you.delay_queue[i].type == DELAY_OFFER_CORPSE)
        {
            const item_def &corpse = mitm[ you.delay_queue[i].parm1 ];
            if (&corpse == &item)
                return (true);

            if (just_first)
                break;
        }
        else
            break;
    }

    return (false);
}

bool is_vampire_feeding()
{
    if (!you_are_delayed())
        return (false);

    const delay_queue_item &delay = you.delay_queue.front();
    return (delay.type == DELAY_FEED_VAMPIRE);
}

// Check whether there are monsters who might be influenced by Recite.
// Returns 0, if no monsters found
// Returns 1, if eligible audience found
// Returns -1, if entire audience already affected or too dumb to understand.
int check_recital_audience()
{
    bool found_monsters = false;

    for ( radius_iterator ri(you.pos(), 8); ri; ++ri )
    {
        if ( mgrd(*ri) == NON_MONSTER )
            continue;

        found_monsters = true;

        // Check if audience can listen.
        if (!_recite_mons_useless( &menv[mgrd(*ri)] ) )
            return (1);
    }

#ifdef DEBUG_DIAGNOSTICS
    if (!found_monsters)
        mprf(MSGCH_DIAGNOSTICS, "No audience found!");
    else
        mprf(MSGCH_DIAGNOSTICS, "No sensible audience found!");
#endif

   // No use preaching to the choir, nor to common animals.
   if (found_monsters)
       return (-1);

   // Sorry, no audience found!
   return (0);
}

// Xom is amused by a potential food source going to waste, and is
// more amused the hungrier you are.
static void _xom_check_corpse_waste()
{
    int food_need = 7000 - you.hunger;
    if (food_need < 0)
        food_need = 0;

    xom_is_stimulated(64 + (191 * food_need / 6000));
}

void clear_macro_process_key_delay()
{
    if (current_delay_action() == DELAY_MACRO_PROCESS_KEY)
        _pop_delay();
}

void handle_delay( void )
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
            if (!is_valid_item(mitm[delay.parm1]))
                break;
            mprf(MSGCH_MULTITURN_ACTION, "You start %s the %s.",
                 (delay.type == DELAY_BOTTLE_BLOOD ? "bottling blood from"
                                                   : "butchering"),
                 mitm[delay.parm1].name(DESC_PLAIN).c_str());

            // Also for bottling blood - just in case.
            if (you.duration[DUR_PRAYER]
                && god_hates_butchery(you.religion))
            {
                did_god_conduct(DID_DEDICATED_BUTCHERY, 10);
            }
            break;
        case DELAY_MEMORISE:
            mpr("You start memorising the spell.", MSGCH_MULTITURN_ACTION);
            break;
        case DELAY_PASSWALL:
            mpr("You begin to meditate on the wall.", MSGCH_MULTITURN_ACTION);
            break;
        case DELAY_RECITE:
            mprf(MSGCH_PLAIN, "You %s",
                 _get_recite_speech("start", you.num_turns + delay.duration).c_str());

            if (apply_area_visible(_recite_to_monsters, delay.parm1))
                viewwindow(true, false);
            break;
        default:
            break;
        }
        delay.started = true;
    }

    ASSERT(!crawl_state.is_repeating_cmd() || delay.type == DELAY_MACRO);

    // Run delays and Lua delays don't have a specific end time.
    if (is_run_delay(delay.type))
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
        if (you.hunger_state == HS_ENGORGED
            || you.hunger_state > HS_SATIATED
               && you.attribute[ATTR_TRANSFORMATION] == TRAN_BAT)
        {
            // Messages handled in _food_change() in food.cc.
            stop_delay();
            return;
        }
    }
    else if (delay.type == DELAY_BUTCHER || delay.type == DELAY_BOTTLE_BLOOD
             || delay.type == DELAY_OFFER_CORPSE)
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
        if (is_valid_item(mitm[ delay.parm1 ])
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
                        xom_is_stimulated(32);
                    delay.duration = 0;
                }
                else
                {
                    // Don't attempt to offer a skeleton.
                    _pop_delay();

                    // Chain onto the next delay.
                    handle_delay();
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
                        mpr("The corpse rots.", MSGCH_ROTTEN_MEAT);

                    if (delay.type == DELAY_OFFER_CORPSE)
                    {
                        // Don't attempt to offer a rotten corpse.
                        _pop_delay();

                        // Chain onto the next delay.
                        handle_delay();
                        return;
                    }

                    delay.parm2 = 99; // Don't give the message twice.

                    if (you.is_undead != US_UNDEAD
                        && player_mutation_level(MUT_SAPROVOROUS) < 3)
                    {
                        _xom_check_corpse_waste();
                    }
                    // Vampires won't continue bottling rotting corpses.
                    if (delay.type == DELAY_BOTTLE_BLOOD)
                    {
                        mpr("You stop bottling this corpse's foul-smelling "
                            "blood!");
                        _pop_delay();
                        handle_delay();
                        return;
                    }
                }

                // Mark work done on the corpse in case we stop. -- bwr
                mitm[ delay.parm1 ].plus2++;
            }
        }
        else if (delay.type == DELAY_OFFER_CORPSE)
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf("Corpse %d no longer valid!", delay.parm1);
#endif
            // Don't attempt to offer an invalid item.
            _pop_delay();

            // Chain onto the next delay.
            handle_delay();
            return;
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
               && !is_valid_item(you.inv[ items_for_multidrop[0].slot ]))
        {
            items_for_multidrop.erase( items_for_multidrop.begin() );
        }

        if (items_for_multidrop.empty())
        {
            // Ran out of things to drop.
            _pop_delay();
            return;
        }
    }
    else if (delay.type == DELAY_RECITE)
    {
        if (check_recital_audience() < 1 // Maybe you've lost your audience...
            || Options.hp_warning && you.hp*Options.hp_warning <= you.hp_max
               && delay.parm2*Options.hp_warning > you.hp_max
            || you.hp*2 < delay.parm2) // ... or significant health drop.
        {
            stop_delay();
            return;
        }
    }

    // Handle delay:
    if (delay.duration > 0)
    {
#if DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "Delay type: %d (%s), duration: %d",
             delay.type, delay_name(delay.type), delay.duration );
#endif
        // delay.duration-- *must* be done before multidrop, because
        // multidrop is now a parent delay, which means other delays
        // can be pushed to the front of the queue, invalidating the
        // "delay" reference here, and resulting in tons of debugging
        // fun with valgrind.
        delay.duration--;

        switch ( delay.type )
        {
        case DELAY_ARMOUR_ON:
            mprf(MSGCH_MULTITURN_ACTION, "You continue putting on %s.",
                 you.inv[delay.parm1].name(DESC_NOCAP_YOUR).c_str());
            break;
        case DELAY_ARMOUR_OFF:
            mprf(MSGCH_MULTITURN_ACTION, "You continue taking off %s.",
                 you.inv[delay.parm1].name(DESC_NOCAP_YOUR).c_str());
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
        case DELAY_RECITE:
            mprf(MSGCH_MULTITURN_ACTION, "You continue %s.",
                 _get_recite_speech("other", you.num_turns + delay.duration+1).c_str());

            if (apply_area_visible(_recite_to_monsters, delay.parm1))
                viewwindow(true, false);
            break;
        case DELAY_MULTIDROP:
            drop_item( items_for_multidrop[0].slot,
                       items_for_multidrop[0].quantity,
                       items_for_multidrop.size() == 1 );
            items_for_multidrop.erase( items_for_multidrop.begin() );
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
    }
}

static void _finish_delay(const delay_queue_item &delay)
{
    switch (delay.type)
    {
    case DELAY_WEAPON_SWAP:
        weapon_switch(delay.parm1);
        break;

    case DELAY_JEWELLERY_ON:
        puton_ring(delay.parm1, false);
        break;

    case DELAY_ARMOUR_ON:
        armour_wear_effects(delay.parm1);
        break;

    case DELAY_ARMOUR_OFF:
    {
        mprf("You finish taking off %s.",
             you.inv[delay.parm1].name(DESC_NOCAP_YOUR).c_str());

        const equipment_type slot = get_armour_slot(you.inv[delay.parm1]);

        if (slot == EQ_BODY_ARMOUR)
        {
            you.equip[EQ_BODY_ARMOUR] = -1;
        }
        else
        {
            switch (slot)
            {
            case EQ_SHIELD:
            case EQ_CLOAK:
            case EQ_HELMET:
            case EQ_GLOVES:
            case EQ_BOOTS:
                if (delay.parm1 == you.equip[slot])
                    you.equip[slot] = -1;
                break;

            default:
                break;
            }
        }

        unwear_armour(delay.parm1);

        you.redraw_armour_class = true;
        you.redraw_evasion = true;
        break;
    }
    case DELAY_EAT:
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

        item_def &corpse = (delay.parm1 ? you.inv[delay.parm2]
                                        : mitm[delay.parm2]);

        vampire_nutrition_per_turn(corpse, 1);

        if (mons_skeleton(corpse.plus) && one_chance_in(3))
            turn_corpse_into_skeleton(corpse);
        else
        {
            if (delay.parm1)
                dec_inv_item_quantity(delay.parm2, 1);
            else
                dec_mitm_item_quantity(delay.parm2, 1);
        }
        break;
    }
    case DELAY_MEMORISE:
        mpr( "You finish memorising." );
        add_spell_to_memory( static_cast<spell_type>( delay.parm1 ) );
        break;

    case DELAY_RECITE:
        mprf(MSGCH_PLAIN, "You finish %s.",
             _get_recite_speech("other", you.num_turns + delay.duration).c_str());
        break;

    case DELAY_PASSWALL:
    {
        mpr( "You finish merging with the rock." );
        more();  // or the above message won't be seen

        const coord_def pass(delay.parm1, delay.parm2);

        if (pass.x != 0 && pass.y != 0)
        {

            switch (grd(pass))
            {
            default:
                if (!you.can_pass_through_feat(grd(pass)))
                    ouch(1 + you.hp, NON_MONSTER, KILLED_BY_PETRIFICATION);
                break;

            case DNGN_SECRET_DOOR:      // oughtn't happen
            case DNGN_CLOSED_DOOR:      // open the door
                grd(pass) = DNGN_OPEN_DOOR;
                break;
            }

            // Move any monsters out of the way:
            int mon = mgrd(pass);
            if (mon != NON_MONSTER)
            {
                monsters* m = &menv[mon];
                // One square, a few squares, anywhere...
                if (!shift_monster(m) && !monster_blink(m))
                    monster_teleport( m, true, true );
            }

            move_player_to_grid(pass, false, true, true);
            redraw_screen();
        }
        break;
    }

    case DELAY_BUTCHER:
    case DELAY_BOTTLE_BLOOD:
    {
        const item_def &item = mitm[delay.parm1];
        if (is_valid_item(item) && item.base_type == OBJ_CORPSES)
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
                    xom_is_stimulated(64);

                break;
            }

            if (delay.type == DELAY_BOTTLE_BLOOD)
            {
                mpr("You finish bottling this corpse's blood.");

                item_def &corpse = mitm[delay.parm1];

                if (mons_skeleton(corpse.plus) && one_chance_in(3))
                    turn_corpse_into_skeleton_and_blood_potions(corpse);
                else
                    turn_corpse_into_blood_potions(corpse);
            }
            else
            {
                mprf("You finish %s the %s into pieces.",
                     (you.has_usable_claws()
                      || player_mutation_level(MUT_FANGS) == 3
                         && you.species != SP_VAMPIRE) ? "ripping"
                                                       : "chopping",
                     mitm[delay.parm1].name(DESC_PLAIN).c_str());

                if (god_hates_cannibalism(you.religion)
                    && is_player_same_species(item.plus))
                {
                    simple_god_message(" expects more respect for your departed "
                                       "relatives.");
                }
                else if (you.religion == GOD_ZIN
                         && mons_class_intel(item.plus) >= I_NORMAL)
                {
                    simple_god_message(" expects more respect for this departed "
                                       "soul.");
                }

                if (you.species == SP_VAMPIRE && delay.type == DELAY_BUTCHER
                    && mons_has_blood(item.plus) && !food_is_rotten(item)
                    // Don't give this message if more butchering to follow.
                    && (you.delay_queue.size() == 1
                        || you.delay_queue[1].type != DELAY_BUTCHER))
                {
                    mpr("What a waste.");
                }

                item_def &corpse = mitm[delay.parm1];

                if (mons_skeleton(corpse.plus) && one_chance_in(3))
                    turn_corpse_into_skeleton_and_chunks(corpse);
                else
                    turn_corpse_into_chunks(corpse);

                if (you.duration[DUR_BERSERKER]
                    && you.berserk_penalty != NO_BERSERK_PENALTY)
                {
                    mpr("You enjoyed that.");
                    you.berserk_penalty = 0;
                }
            }

            // Don't autopickup chunks/potions if there's still another
            // delay (usually more corpses to butcher or a weapon-swap)
            // waiting to happen.
            if ((Options.chunks_autopickup
                    || delay.type == DELAY_BOTTLE_BLOOD)
                && you.delay_queue.size() == 1)
            {
                autopickup();
            }

            // If we were interrupted while butchering (by poisonig, for
            // example) then resumed butchering and finished, swap back from
            // butchering tool if appropriate.
            if (you.delay_queue.size() == 1)
                handle_interrupted_swap(true);
        }
        else
        {
            mprf("You stop %s.",
                 delay.type == DELAY_BUTCHER ? "butchering the corpse"
                                             : "bottling this corpse's blood");
            _pop_delay();
            handle_delay();
        }
        StashTrack.update_stash(); // Stash-track the generated item(s).
        break;
    }

    case DELAY_OFFER_CORPSE:
    {
        if (!you.duration[DUR_PRAYER])
        {
            stop_delay();
            return;
        }

        if (food_is_rotten(mitm[delay.parm1]))
        {
            simple_god_message(coinflip() ? " refuses to accept that"
                                            " mouldy sacrifice!"
                                          : " demands fresh blood!",
                               you.religion);
            _pop_delay();
            // Chain onto the next delay.
            handle_delay();
        }
        else
        {
            offer_corpse(delay.parm1);
            StashTrack.update_stash(); // Don't stash-track this corpse anymore.
        }
        // If we were interrupted while butchering (by poisonig, for
        // example) then resumed butchering and finished, swap back from
        // butchering tool if appropriate.
        if (you.delay_queue.size() == 1)
            handle_interrupted_swap(true);
        break;
    }
    case DELAY_DROP_ITEM:
        // Note:  checking if item is droppable is assumed to
        // be done before setting up this delay... this includes
        // quantity (delay.parm2). -- bwr

        // Make sure item still exists.
        if (!is_valid_item( you.inv[ delay.parm1 ] ))
            break;

        // Must handle unwield_item before we attempt to copy
        // so that temporary brands and such are cleared. -- bwr
        if (delay.parm1 == you.equip[EQ_WEAPON])
        {
            unwield_item();
            canned_msg( MSG_EMPTY_HANDED );
        }

        if (!copy_item_to_grid( you.inv[ delay.parm1 ],
                                you.pos(), delay.parm2,
                                true ))
        {
            mpr("Too many items on this level, not dropping the item.");
        }
        else
        {
            mprf("You drop %s.", quant_name(you.inv[delay.parm1], delay.parm2,
                                            DESC_NOCAP_A).c_str());
            dec_inv_item_quantity( delay.parm1, delay.parm2 );
        }
        break;

    case DELAY_ASCENDING_STAIRS:
        up_stairs();
        break;

    case DELAY_DESCENDING_STAIRS:
        down_stairs( delay.parm1 );
        break;

    case DELAY_INTERRUPTIBLE:
    case DELAY_UNINTERRUPTIBLE:
        // these are simple delays that have no effect when complete
        break;

    default:
        mpr( "You finish doing something." );
        break;
    }

    you.wield_change = true;
    print_stats();  // force redraw of the stats
    _pop_delay();

    // Chain onto the next delay.
    handle_delay();

#ifdef USE_TILE
    tiles.update_inventory();
#endif
}

void armour_wear_effects(const int item_slot)
{
    item_def &arm = you.inv[item_slot];

    const bool was_known = item_type_known(arm);

    set_ident_flags(arm, ISFLAG_EQ_ARMOUR_MASK);
    if (is_random_artefact(arm))
        arm.flags |= ISFLAG_NOTED_ID;

    const equipment_type eq_slot = get_armour_slot(arm);
    const bool melded = (arm.link == you.equip[eq_slot]);
    const bool known_cursed = item_known_cursed(arm);

    if (!was_known)
    {
        if (Options.autoinscribe_randarts && is_random_artefact( arm ))
            add_autoinscription( arm, randart_auto_inscription(arm));
    }
    if (!melded)
        mprf("You finish putting on %s.", arm.name(DESC_NOCAP_YOUR).c_str());

    if (eq_slot == EQ_BODY_ARMOUR)
    {
        you.equip[EQ_BODY_ARMOUR] = item_slot;

        if (you.duration[DUR_ICY_ARMOUR] != 0)
        {
            mpr( "Your icy armour melts away.", MSGCH_DURATION );
            you.redraw_armour_class = true;
            you.duration[DUR_ICY_ARMOUR] = 0;
        }
    }
    else if (eq_slot == EQ_SHIELD)
    {
        if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
            remove_condensation_shield();

        you.equip[EQ_SHIELD] = item_slot;
    }
    else if (!melded)
    {
        switch (eq_slot)
        {
        case EQ_SHIELD:
            break;
        case EQ_CLOAK:
            you.equip[EQ_CLOAK] = item_slot;
            break;
        case EQ_HELMET:
            you.equip[EQ_HELMET] = item_slot;
            break;
        case EQ_GLOVES:
            you.equip[EQ_GLOVES] = item_slot;
            break;
        case EQ_BOOTS:
            you.equip[EQ_BOOTS] = item_slot;
            break;
        default:
            break;
        }
    }

    int ego = get_armour_ego_type( arm );
    if (ego != SPARM_NORMAL)
    {
        switch (ego)
        {
        case SPARM_RUNNING:
            mprf("You feel quick%s.",
                   (you.species == SP_NAGA || player_mutation_level(MUT_HOOVES))
                    ? "" : " on your feet");
            break;

        case SPARM_FIRE_RESISTANCE:
            mpr("You feel resistant to fire.");
            break;

        case SPARM_COLD_RESISTANCE:
            mpr("You feel resistant to cold.");
            break;

        case SPARM_POISON_RESISTANCE:
            mpr("You feel healthy.");
            break;

        case SPARM_SEE_INVISIBLE:
            mpr("You feel perceptive.");
            break;

        case SPARM_DARKNESS:
            if (!you.duration[DUR_INVIS])
                mpr("You become transparent for a moment.");
            break;

        case SPARM_STRENGTH:
            modify_stat(STAT_STRENGTH, 3, false, arm);
            break;

        case SPARM_DEXTERITY:
            modify_stat(STAT_DEXTERITY, 3, false, arm);
            break;

        case SPARM_INTELLIGENCE:
            modify_stat(STAT_INTELLIGENCE, 3, false, arm);
            break;

        case SPARM_PONDEROUSNESS:
            mpr("You feel rather ponderous.");
            // you.speed += 2;
            you.redraw_evasion = true;
            break;

        case SPARM_LEVITATION:
            mpr("You feel rather light.");
            break;

        case SPARM_MAGIC_RESISTANCE:
            mpr("You feel resistant to magic.");
            break;

        case SPARM_PROTECTION:
            mpr("You feel protected.");
            break;

        case SPARM_STEALTH:
            mpr("You feel stealthy.");
            break;

        case SPARM_RESISTANCE:
            mpr("You feel resistant to extremes of temperature.");
            break;

        case SPARM_POSITIVE_ENERGY:
            mpr("Your life-force is being protected.");
            break;

        case SPARM_ARCHMAGI:
            if (!you.skills[SK_SPELLCASTING])
                mpr("You feel strangely numb.");
            else
                mpr("You feel extremely powerful.");
            break;
        }
    }

    if (is_random_artefact( arm ))
        use_randart( arm, melded );

    if (item_cursed( arm ) && !melded)
    {
        mpr( "Oops, that feels deathly cold." );
        learned_something_new(TUT_YOU_CURSED);

        // Cursed cloaks prevent you from removing body armour
        int cloak_mult = 1;
        if (get_armour_slot(arm) == EQ_CLOAK)
            cloak_mult = 2;

        if (known_cursed)
            xom_is_stimulated(32 * cloak_mult);
        else
            xom_is_stimulated(64 * cloak_mult);
    }

    if (eq_slot == EQ_SHIELD)
        warn_shield_penalties();

    you.redraw_armour_class = true;
    you.redraw_evasion = true;
}

static command_type _get_running_command()
{
    if (kbhit())
    {
        stop_running();
        return CMD_NO_CMD;
    }

    if (is_resting())
    {
        you.running.rest();

#ifdef USE_TILE
        if (tiles.need_redraw())
            tiles.redraw();
#endif

        if (!is_resting() && you.running.hp == you.hp
            && you.running.mp == you.magic_points)
        {
            mpr("Done searching.");
        }
        return CMD_MOVE_NOWHERE;
    }
    else if (Options.travel_delay > 0)
        delay(Options.travel_delay);

    return direction_to_command( you.running.pos.x, you.running.pos.y );
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

    if (cmd != CMD_NO_CMD)
    {
        if (delay.type != DELAY_REST)
            mesclr();
        process_command(cmd);
    }

    // If you.running has gone to zero, and the run delay was not
    // removed, remove it now. This is needed to clean up after
    // find_travel_pos() function in travel.cc.
    if (!you.running && is_run_delay(current_delay_action()))
    {
        _pop_delay();
        update_turn_count();
    }

    if (you.running && !you.turn_is_over
        && you_are_delayed()
        && !is_run_delay(current_delay_action()))
    {
        handle_delay();
    }
}

static void _handle_macro_delay()
{
    run_macro();

    // Macros may not use up turns, but unless we zero time_taken,
    // acr.cc will call world_reacts and increase turn count.
    if (!you.turn_is_over && you.time_taken)
        you.time_taken = 0;
}

void run_macro(const char *macroname)
{
    const int currdelay = current_delay_action();
    if (currdelay != DELAY_NOT_DELAYED && currdelay != DELAY_MACRO)
        return;

#ifdef CLUA_BINDINGS
    if (!clua)
    {
        mpr("Lua not initialized", MSGCH_DIAGNOSTICS);
        stop_delay();
        return;
    }

    if (!clua.callbooleanfn(false, "c_macro", "s", macroname))
    {
        if (clua.error.length())
            mprf(MSGCH_ERROR, "Lua error: %s", clua.error.c_str());

        stop_delay();
    }
    else
    {
        if (!you_are_delayed())
            start_delay(DELAY_MACRO, 1);
    }
#else
    stop_delay();
#endif
}

// Returns 1 if the delay should be interrupted, 0 if the user function
// had no opinion on the matter, -1 if the delay should not be interrupted.
static int _userdef_interrupt_activity( const delay_queue_item &idelay,
                                        activity_interrupt_type ai,
                                        const activity_interrupt_data &at )
{
#ifdef CLUA_BINDINGS
    const int delay = idelay.type;
    lua_State *ls = clua.state();
    if (!ls || ai == AI_FORCE_INTERRUPT)
        return (true);

    // Kludge: We have to continue to support ch_stop_run. :-(
    if (is_run_delay(delay) && you.running && ai == AI_SEE_MONSTER)
    {
        bool stop_run = false;
        if (clua.callfn("ch_stop_run", "M>b",
                        (const monsters *) at.data, &stop_run))
        {
            if (stop_run)
                return (true);

            // No further processing.
            return (-1);
        }

        // If we get here, ch_stop_run wasn't defined, fall through to the
        // other handlers.
    }

    const char *interrupt_name = activity_interrupt_name(ai);
    const char *act_name = delay_name(delay);

    bool ran = clua.callfn("c_interrupt_activity", "1:ssA",
                    act_name, interrupt_name, &at);
    if (ran)
    {
        // If the function returned nil, we want to cease processing.
        if (lua_isnil(ls, -1))
        {
            lua_pop(ls, 1);
            return (-1);
        }

        bool stopact = lua_toboolean(ls, -1);
        lua_pop(ls, 1);
        if (stopact)
            return (true);
    }

    if (delay == DELAY_MACRO && clua.callbooleanfn(true, "c_interrupt_macro",
                                                   "sA", interrupt_name, &at))
    {
        return (true);
    }

#endif
    return (false);
}

static void _block_interruptions(bool block)
{
    if (block)
        _interrupts_blocked++;
    else
        _interrupts_blocked--;
}

// Returns true if the activity should be interrupted, false otherwise.
static bool _should_stop_activity(const delay_queue_item &item,
                                  activity_interrupt_type ai,
                                  const activity_interrupt_data &at)
{
    int userd = _userdef_interrupt_activity(item, ai, at);

    // If the user script wanted to stop the activity or cease processing,
    // do so.
    if (userd)
        return (userd == 1);

    delay_type curr = current_delay_action();

    if (ai == AI_SEE_MONSTER && (curr == DELAY_ASCENDING_STAIRS
                                 || curr == DELAY_DESCENDING_STAIRS))
    {
        return (false);
    }

    if (ai == AI_FULL_HP || ai == AI_FULL_MP)
    {
        if (Options.rest_wait_both && curr == DELAY_REST
            && (you.magic_points < you.max_magic_points
                || you.hp < you.hp_max))
        {
            return (false);
        }
    }

    return (ai == AI_FORCE_INTERRUPT
            || Options.activity_interrupts[item.type][ai]);
}

inline static bool _monster_warning(activity_interrupt_type ai,
                                    const activity_interrupt_data &at,
                                    int atype)
{
    if (ai == AI_SEE_MONSTER && (is_run_delay(atype)
                                 || _is_butcher_delay(atype)))
    {
        const monsters* mon = static_cast<const monsters*>(at.data);
        if (!mon->visible())
            return (false);
        if (at.context == "already seen" || at.context == "uncharm")
        {
            // Only say "comes into view" if the monster wasn't in view
            // during the previous turn.
            if (testbits(mon->flags, MF_WAS_IN_VIEW))
            {
                mprf(MSGCH_WARN, "%s is too close now for your liking.",
                     mon->name(DESC_CAP_THE).c_str());
            }
        }
        else
        {
            ASSERT(mon->seen_context != "just seen");
            // If the monster is in the auto_exclude list, automatically
            // set an exclusion.
            if (need_auto_exclude(mon) && !is_exclude_root(mon->pos()))
                toggle_exclude(mon->pos());

            std::string text = mon->name(DESC_CAP_A);
            // For named monsters also mention the base type.
            if (!(mon->mname).empty() && mon->type != MONS_PLAYER_GHOST)
                text += " the " + mon->name(DESC_BASENAME);

            if (starts_with(at.context, "open"))
                text += " " + at.context;
            else if (at.context == "thin air")
            {
                if (mon->type == MONS_AIR_ELEMENTAL)
                    text += " forms itself from the air.";
                else
                    text += " appears from thin air!";
            }
            // The monster surfaced and submerged in the same turn without
            // doing anything else.
            else if (at.context == "surfaced")
                text += "surfaces briefly.";
            else if (at.context == "surfaces")
                text += " surfaces.";
            else if (at.context.find("bursts forth") != std::string::npos)
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
            else if (at.context.find("emerges") != std::string::npos)
                text += " emerges from the water.";
            else if (at.context.find("leaps out") != std::string::npos)
            {
                if (mon->type == MONS_TRAPDOOR_SPIDER)
                    text += " leaps out from its hiding place under the "
                            "floor!";
                else
                    text += " leaps out from hiding!";
            }
            else
                text += " comes into view.";

            const std::string mweap =
                get_monster_desc(mon, false, DESC_NONE);

            if (!mweap.empty())
            {
                text += " " + mon->pronoun(PRONOUN_CAP)
                        + " is" + mweap + ".";
            }
            print_formatted_paragraph(text,
                                      get_number_of_cols(),
                                      MSGCH_WARN);
            const_cast<monsters*>(mon)->seen_context = "just seen";
        }

        if (Options.tutorial_left)
            tutorial_first_monster(*mon);

        return (true);
    }

    return (false);
}

static bool _paranoid_option_disable( activity_interrupt_type ai,
                                      const activity_interrupt_data &at )
{
    if (ai == AI_HIT_MONSTER || ai == AI_MONSTER_ATTACKS)
    {
        const monsters* mon = static_cast<const monsters*>(at.data);
        if (mon && !player_monster_visible(mon) && !mons_is_submerged(mon))
        {
            // Now that autoprayer has been removed the vectors aren't
            // really needed anymore, but let's keep them "just in case".
            std::vector<std::string> deactivatees;
            std::vector<std::string> restart;

            if (Options.autopickup_on)
            {
                deactivatees.push_back("autopickup");
                Options.autopickup_on = false;
                restart.push_back("Ctrl+A");
            }

            if (!deactivatees.empty())
            {
                mprf(MSGCH_WARN, "Deactivating %s; reactivate with %s.",
                      comma_separated_line(deactivatees.begin(),
                                           deactivatees.end()).c_str(),
                      comma_separated_line(restart.begin(),
                                           restart.end()).c_str());
            }

            if (Options.tutorial_left)
            {
                learned_something_new(TUT_INVISIBLE_DANGER);
                Options.tut_seen_invisible = you.num_turns;
            }
        }
        return (true);
    }
    return (false);
}

// Returns true if any activity was stopped.
bool interrupt_activity( activity_interrupt_type ai,
                         const activity_interrupt_data &at )
{
    if (_interrupts_blocked > 0)
        return (false);

    bool was_monst = _paranoid_option_disable(ai, at);

    if (crawl_state.is_repeating_cmd())
        return interrupt_cmd_repeat(ai, at);

    const int delay = current_delay_action();

    if (delay == DELAY_NOT_DELAYED)
        return (false);

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "Activity interrupt: %s",
         activity_interrupt_name(ai));
#endif

    // First try to stop the current delay.
    const delay_queue_item &item = you.delay_queue.front();

    // No recursive interruptions from messages (AI_MESSAGE)
    _block_interruptions(true);
    if (ai == AI_FULL_HP)
        mpr("HP restored.");
    else if (ai == AI_FULL_MP)
        mpr("Magic restored.");
    _block_interruptions(false);

    if (_should_stop_activity(item, ai, at))
    {
        // no monster will attack you inside a sanctuary,
        // so presence of monsters won't matter
        if (is_sanctuary(you.pos()))
            return (false);

        was_monst = _monster_warning(ai, at, item.type) || was_monst;
        stop_delay();
        if (was_monst)
            handle_interrupted_swap(false, true);

        return (true);
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
                if (is_run_delay( you.delay_queue[j].type ))
                {
                    was_monst =
                        _monster_warning(ai, at, you.delay_queue[j].type)
                        || was_monst;

                    stop_delay();
                    if (was_monst)
                        handle_interrupted_swap(false, true);
                    return (true);
                }
            }

            // Non-run queued delays can be discarded without any processing.
            you.delay_queue.erase( you.delay_queue.begin() + i,
                                   you.delay_queue.end() );
            if (was_monst)
                handle_interrupted_swap(false, true);

            return (true);
        }
    }

    return (false);
}

static const char *activity_interrupt_names[] =
{
    "force", "keypress", "full_hp", "full_mp", "statue",
    "hungry", "message", "hp_loss", "burden", "stat",
    "monster", "monster_attack", "teleport", "hit_monster"
};

const char *activity_interrupt_name(activity_interrupt_type ai)
{
    ASSERT( sizeof(activity_interrupt_names)
            / sizeof(*activity_interrupt_names) == NUM_AINTERRUPTS );

    if (ai == NUM_AINTERRUPTS)
        return ("");

    return activity_interrupt_names[ai];
}

activity_interrupt_type get_activity_interrupt(const std::string &name)
{
    ASSERT( sizeof(activity_interrupt_names)
            / sizeof(*activity_interrupt_names) == NUM_AINTERRUPTS );

    for (int i = 0; i < NUM_AINTERRUPTS; ++i)
        if (name == activity_interrupt_names[i])
            return activity_interrupt_type(i);

    return (NUM_AINTERRUPTS);
}

static const char *delay_names[] =
{
    "not_delayed", "eat", "vampire_feed", "armour_on", "armour_off",
    "jewellery_on", "memorise", "butcher", "bottle_blood", "offer_corpse",
    "weapon_swap", "passwall", "drop_item", "multidrop", "ascending_stairs",
    "descending_stairs", "recite", "run", "rest", "travel", "macro",
    "macro_process_key", "interruptible", "uninterruptible"
};

// Gets a delay given its name.
// name must be lowercased already!
delay_type get_delay(const std::string &name)
{
    ASSERT( sizeof(delay_names) / sizeof(*delay_names) == NUM_DELAYS );

    for (int i = 0; i < NUM_DELAYS; ++i)
    {
        if (name == delay_names[i])
            return delay_type(i);
    }

    // Also check American spellings:
    if (name == "armor_on")
        return (DELAY_ARMOUR_ON);

    if (name == "armor_off")
        return (DELAY_ARMOUR_OFF);

    if (name == "memorize")
        return (DELAY_MEMORISE);

    if (name == "jewelry_on")
        return (DELAY_JEWELLERY_ON);

    return (NUM_DELAYS);
}

const char *delay_name(int delay)
{
    ASSERT( sizeof(delay_names) / sizeof(*delay_names) == NUM_DELAYS );

    if (delay < 0 || delay >= NUM_DELAYS)
        return ("");

    return delay_names[delay];
}
