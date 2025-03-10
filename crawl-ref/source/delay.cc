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
#include "clua.h"
#include "command.h"
#include "coord.h"
#include "corpse.h"
#include "database.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "env.h"
#include "fineff.h"
#include "fprop.h"
#include "god-companions.h"
#include "god-passive.h"
#include "god-wrath.h"
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "item-use.h"
#include "item-status-flag-type.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-gear.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "notes.h"
#include "options.h"
#include "ouch.h"
#include "output.h"
#include "player-equip.h"
#include "player.h"
#include "prompt.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "sound.h"
#include "spl-damage.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h" // attract_monster
#include "spl-util.h"
#include "stairs.h"
#include "state.h"
#include "stringutil.h"
#include "teleport.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "xom.h"
#include "zot.h" // ZOT_CLOCK_PER_FLOOR

int interrupt_block::interrupts_blocked = 0;

static const char *_activity_interrupt_name(activity_interrupt ai);

static string _eq_category(const item_def &equip)
{
    if (is_weapon(equip))
        return "weapon";
    return equip.base_type == OBJ_JEWELLERY ? "amulet" : "armour";
}

void push_delay(shared_ptr<Delay> delay)
{
    if (delay->is_run())
        clear_travel_trail();
    for (auto i = you.delay_queue.begin(); i != you.delay_queue.end(); ++i)
    {
        if ((*i)->is_parent())
        {
            you.delay_queue.insert(i, delay);
            you.redraw_evasion = true;
            return;
        }
    }
    you.delay_queue.push_back(delay);
    you.redraw_evasion = true;
}

static void _pop_delay()
{
    if (!you.delay_queue.empty())
        you.delay_queue.erase(you.delay_queue.begin());

    you.redraw_evasion = true;
}

static void _clear_pending_delays(size_t after_index = 1)
{
    while (you.delay_queue.size() > after_index)
    {
        auto delay = you.delay_queue.back();

        you.delay_queue.pop_back();

        if (delay->is_run() && you.running)
            // If you got here, you're already clearing the delays and there's
            // no need to clear them again.
            stop_running(false);
    }
}

bool MemoriseDelay::try_interrupt(bool /*force*/)
{
    // Losing work here is okay... having to start from
    // scratch is a reasonable behaviour. -- bwr
    mpr("Your memorisation is interrupted.");
    return true;
}

bool MultidropDelay::try_interrupt(bool /*force*/)
{
    // No work lost
    if (!items.empty())
        mpr("You stop dropping stuff.");
    return true;
}

bool BaseRunDelay::try_interrupt(bool /*force*/)
{
    // Keep things consistent, otherwise disturbing phenomena can occur.
    if (you.running)
        stop_running(false);
    update_turn_count();

    // Always interruptible.
    return true;
}

bool MacroDelay::try_interrupt(bool /*force*/)
{
    // Always interruptible.
    return true;
    // There's no special action needed for macros - if we don't call out
    // to the Lua function, it can't do damage.
}

const char* EquipOnDelay::get_verb()
{
    if (is_weapon(equip))
    {
        if (you.has_mutation(MUT_SLOW_WIELD))
            return "attuning to";
        else
            return "wielding";
    }
    else if (you.has_mutation(MUT_FORMLESS))
        return "haunting";
    else
        return "putting on";
}

bool EquipOnDelay::try_interrupt(bool force)
{
    bool interrupt = false;

    if (force)
        interrupt = true;
    else if (duration > 1 && !was_prompted)
    {
        if (!crawl_state.disables[DIS_CONFIRMATIONS]
            && !yesno("Keep equipping yourself?", false, 0, false))
        {
            interrupt = true;
        }
        else
            was_prompted = true;
    }

    if (interrupt)
    {
        mprf("You stop %s your %s.", get_verb(), _eq_category(equip).c_str());
        return true;
    }
    return false;
}

const char* EquipOffDelay::get_verb()
{
    if (is_weapon(equip))
    {
        if (you.has_mutation(MUT_SLOW_WIELD))
            return "parting from";
        else
            return "unwielding";
    }
    else if (you.has_mutation(MUT_FORMLESS))
        return "removing yourself from";
    else
        return "removing";
}

bool EquipOffDelay::try_interrupt(bool force)
{
    bool interrupt = false;

    if (force)
        interrupt = true;
    else if (duration > 1 && !was_prompted)
    {
        const bool is_armour = equip.base_type == OBJ_ARMOUR
                               // Shields and orbs aren't clothes.
                               && get_armour_slot(equip) != SLOT_OFFHAND;
        const char* verb = is_armour ? "disrobing" : "removing your equipment";
        const string prompt = make_stringf("Keep %s?", verb);
        if (!crawl_state.disables[DIS_CONFIRMATIONS]
            && !yesno(prompt.c_str(), false, 0, false))
        {
            interrupt = true;
        }
        else
            was_prompted = true;
    }

    if (interrupt)
    {
        mprf("You stop %s your %s.", get_verb(), _eq_category(equip).c_str());
        return true;
    }
    return false;
}

bool AscendingStairsDelay::try_interrupt(bool /*force*/)
{
    mpr("You stop ascending the stairs.");
    untag_followers();
    return true;  // short... and probably what people want
}

bool DescendingStairsDelay::try_interrupt(bool /*force*/)
{
    mpr("You stop descending the stairs.");
    untag_followers();
    return true;  // short... and probably what people want
}

bool PasswallDelay::try_interrupt(bool /*force*/)
{
    // finish() can trigger interrupts, avoid a double message
    if (interrupt_block::blocked())
        return false;
    mpr("Your meditation is interrupted.");
    you.props.erase(PASSWALL_ARMOUR_KEY);
    you.redraw_armour_class = true;
    return true;
}

bool ShaftSelfDelay::try_interrupt(bool /*force*/)
{
    mpr("You stop digging.");
    return true;
}

bool TransformDelay::try_interrupt(bool force)
{
    bool interrupt = false;

    if (force)
        interrupt = true;
    else if (duration > 1 && !was_prompted)
    {
        if (!crawl_state.disables[DIS_CONFIRMATIONS]
            && !yesno("Keep transforming yourself?", false, 0, false))
        {
            interrupt = true;
        }
        else
            was_prompted = true;
    }

    if (!interrupt)
        return false;
    mpr("You stop transforming.");
    return true;
}

bool ImbueDelay::try_interrupt(bool force)
{
    bool interrupt = false;

    if (force)
        interrupt = true;
    else if (duration > 1 && !was_prompted)
    {
        if (!crawl_state.disables[DIS_CONFIRMATIONS]
            && !yesno("Keep imbuing your servitor?", false, 0, false))
        {
            interrupt = true;
        }
        else
            was_prompted = true;
    }

    if (interrupt)
    {
        mpr("You stop imbuing your servitor.");
        return true;
    }
    return false;
}

bool ImprintDelay::try_interrupt(bool /*force*/)
{
    mpr("Your concentration is interrupted.");
    return true;
}

void stop_delay(bool stop_relocations, bool force)
{
    if (you.delay_queue.empty())
        return;

    set_more_autoclear(false);

    ASSERT(!crawl_state.game_is_arena());

    auto delay = current_delay();

    // At the very least we can remove any queued delays, right
    // now there is no problem with doing this... note that
    // any queuing here can only happen from a single command,
    // as the effect of a delay doesn't normally allow interaction
    // until it is done... it merely chains up individual actions
    // into a single action.  -- bwr
    // Butcher delays do this on their own, in order to determine the old
    // list of delays before clearing it.
    _clear_pending_delays();

    if ((!delay->is_relocation() || stop_relocations)
        && delay->try_interrupt(force))
    {
        _pop_delay();
    }
}

bool you_are_delayed()
{
    return !you.delay_queue.empty();
}

shared_ptr<Delay> current_delay()
{
    return you_are_delayed() ? you.delay_queue.front()
                             : nullptr;
}

bool player_stair_delay()
{
    auto delay = current_delay();
    return delay && delay->is_stairs();
}

/**
 * Is the player currently in the middle of memorising a spell?
 *
 * @param spell     A specific spell, or -1 to check if we're memorising any
 *                  spell at all.
 * @return          Whether the player is currently memorising the given type
 *                  of spell.
 */
bool already_learning_spell(int spell)
{
    for (const auto &delay : you.delay_queue)
    {
        auto mem = dynamic_cast<MemoriseDelay*>(delay.get());
        if (!mem)
            continue;

        if (spell == -1 || mem->spell == spell)
            return true;
    }
    return false;
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

void clear_macro_process_key_delay()
{
    if (dynamic_cast<MacroProcessKeyDelay*>(current_delay().get()))
        _pop_delay();
}

void EquipOnDelay::start()
{
    mprf(MSGCH_MULTITURN_ACTION, "You start %s your %s.",
         get_verb(),
         _eq_category(equip).c_str());
}

void EquipOffDelay::start()
{
    mprf(MSGCH_MULTITURN_ACTION, "You start %s your %s.",
         get_verb(),
         _eq_category(equip).c_str());
}

void MemoriseDelay::start()
{
    if (vehumet_is_offering(spell, true))
    {
        string message = make_stringf(" grants you knowledge of %s.",
            spell_title(spell));
        simple_god_message(message.c_str());
    }
    mprf(MSGCH_MULTITURN_ACTION, "You start memorising the spell.");
}

void PasswallDelay::start()
{
    mprf(MSGCH_MULTITURN_ACTION, "You begin to meditate on the wall.");
}

void ShaftSelfDelay::start()
{
    mprf(MSGCH_MULTITURN_ACTION, "You begin to dig a shaft.");
}

void ImbueDelay::start()
{
    mprf(MSGCH_MULTITURN_ACTION, "You begin to imbue your servitor with "
         "knowledge of %s.",
         spell_title(spell));
}

void ImprintDelay::start()
{
    mprf(MSGCH_MULTITURN_ACTION, "You begin to imprint %s upon your paragon.",
         wpn.name(DESC_THE).c_str());
}

void TransformDelay::start()
{
    if (form == transformation::none)
        mprf(MSGCH_MULTITURN_ACTION, "You begin untransforming.");
    else
        mprf(MSGCH_MULTITURN_ACTION, "You begin transforming.");
}

command_type RunDelay::move_cmd() const
{
    return _get_running_command();
}

command_type RestDelay::move_cmd() const
{
    return _get_running_command();
}

command_type TravelDelay::move_cmd() const
{
    return travel();
}

void BaseRunDelay::handle()
{
    bool first_move = false;
    if (!started)
    {
        first_move = true;
        started = true;
    }
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

    if (want_move() && you.confused())
    {
        mprf("You're confused, stopping %s.",
             you.running.runmode_name().c_str());
        stop_running();
    }
    else if (!(unsafe_once && first_move) && !i_feel_safe(true, want_move())
            || you.running.is_rest()
               && (!can_rest_here(true) || regeneration_is_inhibited()))
    {
        stop_running();
    }
    else
        cmd = move_cmd();

    if (cmd != CMD_NO_CMD)
    {
        if (want_clear_messages())
            clear_messages();
        process_command(cmd);
        if (you.turn_is_over
            && (you.running.is_any_travel() || you.running.is_rest()))
        {
            you.running.turns_passed++;
            // sanity check: if we get up to a large number of turns on
            // an explore, run, or rest delay, something is extremely buggy
            // and we should both rescue the player, and generate a crash
            // report. The thresholds are very heuristic:
            // Rest delay, 700 turns. A maxed ogre at hp 1 with no regen bonus
            // takes around 510 turns to heal fully.
            // Travel delay, 2000. Just a big number that is quite a bit
            // bigger than any travel delay I have been able to generate. If
            // anyone can generate this on demand it should be raised. (Or
            // eventually, removed?)
            // For debuggers: runmode if negative is a meaningful delay type,
            // but when positive is used as a counter, so if it's a very large
            // number in the assert message, this is a wait delay

            const int buggy_threshold = you.running.is_rest()
                ? 700
                : (ZOT_CLOCK_PER_FLOOR / BASELINE_DELAY / 3);
            ASSERTM(you.running.turns_passed < buggy_threshold,
                    "Excessive delay, %d turns passed, delay type %d",
                    you.running.turns_passed, you.running.runmode);
        }
    }

    if (!you.turn_is_over)
        you.time_taken = 0;

    // If you.running has gone to zero, and the run delay was not
    // removed, remove it now. This is needed to clean up after
    // find_travel_pos() function in travel.cc.
    if (!you.running
        && !you.delay_queue.empty()
        && you.delay_queue.front()->is_run())
    {
        update_turn_count();
        _pop_delay();
    }
}

void MacroDelay::handle()
{
    if (!started)
        started = true;
    if (!duration)
    {
        dprf("Expiring macro delay on turn: %d", you.num_turns);
        stop_delay();
    }
    else
        run_macro();

    // Macros may not use up turns, but unless we zero time_taken,
    // main.cc will call world_reacts and increase turn count.
    if (!you.turn_is_over && you.time_taken)
        you.time_taken = 0;
}

bool MultidropDelay::invalidated()
{
    // Throw away invalid items. XXX: what are they?
    while (!items.empty()
           // Don't look for gold in inventory
           && items[0].slot != PROMPT_GOT_SPECIAL
           && !you.inv[items[0].slot].defined())
    {
        items.erase(items.begin());
    }

    if (items.empty())
    {
        // Ran out of things to drop.
        you.turn_is_over = false;
        you.time_taken = 0;
        return true;
    }
    return false;
}

void MultidropDelay::tick()
{
    if (!drop_item(items[0].slot, items[0].quantity))
    {
        you.turn_is_over = false;
        you.time_taken = 0;
    }
    items.erase(items.begin());
}

void DropItemDelay::tick()
{
    // This is a 1-turn delay where the time cost is handled
    // in finish().
    // FIXME: get rid of this hack!
    you.time_taken = 0;
}

void TransformDelay::tick()
{
    if (form == transformation::none)
        mprf(MSGCH_MULTITURN_ACTION, "You continue untransforming.");
    else
        mprf(MSGCH_MULTITURN_ACTION, "You continue transforming.");
}

void Delay::handle()
{
    if (!started)
    {
        started = true;
        start();
    }

    // First check cases where delay may no longer be valid:
    // XXX: need to handle PasswallDelay when monster digs -- bwr
    if (invalidated())
    {
        _pop_delay();
        return;
    }

    // Actually handle delay:
    if (duration > 0)
    {
        dprf("Delay type: %s, duration: %d", name(), duration);
        --duration;
        tick();
    }
    else
    {
        finish();
        you.wield_change = true;
        _pop_delay();
        print_stats();  // force redraw of the stats
        update_screen();
#ifdef USE_TILE
        tiles.update_tabs();
#endif
        if (!you.turn_is_over)
            you.time_taken = 0;
    }
}

void handle_delay()
{
    if (!you_are_delayed())
        return;

    shared_ptr<Delay> delay = current_delay();
    delay->handle();
}

bool EquipOnDelay::invalidated()
{
    return !equip.defined();
}

void EquipOnDelay::finish()
{
    mprf("You finish %s %s.", get_verb(), equip.name(DESC_YOUR).c_str());

    if (is_weapon(equip) && you.has_mutation(MUT_SLOW_WIELD))
        maybe_name_weapon(equip);

    equip_item(slot, equip.link);
}

bool EquipOffDelay::invalidated()
{
    return !equip.defined();
}

void EquipOffDelay::finish()
{
    mprf("You finish %s %s.", get_verb(), equip.name(DESC_YOUR).c_str());
    unequip_item(equip);
}

void MemoriseDelay::finish()
{
#ifdef USE_SOUND
    parse_sound(MEMORISE_SPELL_SOUND);
#endif
    mpr("You finish memorising.");
    add_spell_to_memory(spell);
    vehumet_accept_gift(spell);
    quiver::on_actions_changed();
}

void PasswallDelay::finish()
{
    // No interrupt message if our destination causes this delay to be
    // interrupted
    const interrupt_block block_double_message;
    mpr("You finish merging with the rock.");
    // included in default force_more_message

    // Immediately cancel bonus AC (since there are so many paths through this code)
    you.props.erase(PASSWALL_ARMOUR_KEY);
    you.redraw_armour_class = true;

    if (dest.x == 0 || dest.y == 0)
        return;

    switch (env.grid(dest))
    {
    default:
        if (!you.is_habitable(dest))
        {
            mpr("...yet there is something new on the other side. "
                "You quickly turn back.");
            redraw_screen();
            update_screen();
            return;
        }
        break;

    case DNGN_CLOSED_DOOR:      // open the door
    case DNGN_CLOSED_CLEAR_DOOR:
    case DNGN_RUNED_DOOR:
    case DNGN_RUNED_CLEAR_DOOR:
        // Once opened, former runed doors become normal doors.
        dgn_open_door(dest);
        break;
    }

    // Move any monsters out of the way.
    if (monster* m = monster_at(dest))
    {
        // One square only, this isn't a tloc spell!
        if (!m->shift())
        {
            mpr("...and sense your way blocked. You quickly turn back.");
            redraw_screen();
            update_screen();
            return;
        }

        move_player_to_grid(dest, false);

        // Wake the monster if it's asleep.
        if (m)
            behaviour_event(m, ME_ALERT, &you);
    }
    else
        move_player_to_grid(dest, false);

    // the last phase of the delay is a fake (0-time) turn, so world_reacts
    // and player_reacts aren't triggered. Need to do a tiny bit of cleanup.
    // This isn't very elegant, and perhaps a version of player_reacts that is
    // triggered by changing location would be better (per Pleasingfungus),
    // but player_reacts is very sensitive to order and can't be easily
    // refactored in this way.
    you.update_beholders();
    you.update_fearmongers();

    // in addition to missing player_reacts we miss world_reacts until after
    // we act, missing out on a trap.
    if (you.trapped)
    {
        do_trap_effects();
        you.trapped = false;
    }
}

void ShaftSelfDelay::finish()
{
    you.do_shaft_ability();
}

void DropItemDelay::finish()
{
    // We're here if dropping the item required some action to be done
    // first, like removing armour. At this point, it should be droppable
    // immediately.

    // Make sure item still exists.
    if (!item.defined())
        return;

    if (!drop_item(item.link, 1))
    {
        you.turn_is_over = false;
        you.time_taken = 0;
    }
}

void AscendingStairsDelay::finish()
{
    up_stairs();
}

void DescendingStairsDelay::finish()
{
    down_stairs();
}

void ImbueDelay::finish()
{
    mpr("You finish imbuing your servitor.");
    you.props[SERVITOR_SPELL_KEY] = spell;
}

void ImprintDelay::finish()
{
    mprf("You finish imprinting the physical structure of %s upon your paragon.",
            wpn.name(DESC_THE).c_str());
    you.props[PARAGON_WEAPON_KEY].get_item() = wpn;
}

bool TransformDelay::invalidated()
{
    // Got /poly'd while mid-transform?
    return you.transform_uncancellable;
}

void TransformDelay::finish()
{
    if (form == transformation::none)
    {
        unset_default_form();
        untransform(false, false);
        return;
    }

    set_default_form(form, talisman);
    return_to_default_form();
}

void run_macro(const char *macroname)
{
#ifdef CLUA_BINDINGS
    if (!clua)
    {
        mprf(MSGCH_DIAGNOSTICS, "Lua not initialised");
        stop_delay();
        return;
    }

    shared_ptr<Delay> delay;
    if (!macroname)
    {
        delay = current_delay();
        ASSERT(delay->is_macro());
    }
    else
        delay = start_delay<MacroDelay>();

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
        else if (delay->duration > 0)
            --delay->duration;
    }
#else
    mprf(MSGCH_ERROR, "CLua bindings not available on this build!");
    UNUSED(macroname);
    stop_delay();
#endif
}

// Returns TRUE if the delay should be interrupted, MAYBE if the user function
// had no opinion on the matter, FALSE if the delay should not be interrupted.
static maybe_bool _userdef_interrupt_activity(Delay* delay,
                                              activity_interrupt ai,
                                              const activity_interrupt_data &at)
{
    lua_State *ls = clua.state();
    if (!ls || ai == activity_interrupt::force)
        return true;

    const char *interrupt_name = _activity_interrupt_name(ai);

    bool ran = clua.callfn("c_interrupt_activity", "1:ssA",
                           delay->name(), interrupt_name, &at);
    if (ran)
    {
        // If the function returned nil, we want to cease processing.
        if (lua_isnil(ls, -1))
        {
            lua_pop(ls, 1);
            return false;
        }

        bool stopact = lua_toboolean(ls, -1);
        lua_pop(ls, 1);
        if (stopact)
            return true;
    }

    if (delay->is_macro() && clua.callbooleanfn(true, "c_interrupt_macro",
                                                "sA", interrupt_name, &at))
    {
        return true;
    }
    return maybe_bool::maybe;
}

// Returns true if the activity should be interrupted, false otherwise.
static bool _should_stop_activity(Delay* delay,
                                  activity_interrupt ai,
                                  const activity_interrupt_data &at)
{
    const maybe_bool user_stop = _userdef_interrupt_activity(delay, ai, at);
    if (user_stop.is_bool())
        return user_stop.to_bool();

    // Don't interrupt player on monster's turn, they might wander off.
    if (you.turn_is_over
        && (at.context == SC_ALREADY_SEEN || at.context == SC_UNCHARM))
    {
        return false;
    }

    // No monster will attack you inside a sanctuary,
    // so presence of monsters won't matter.
    if (ai == activity_interrupt::see_monster && is_sanctuary(you.pos()))
        return false;

    auto curr = current_delay(); // Not necessarily what we were passed.

    if ((ai == activity_interrupt::see_monster
         || ai == activity_interrupt::mimic)
        && player_stair_delay())
    {
        return false;
    }

    if (ai == activity_interrupt::full_hp || ai == activity_interrupt::full_mp
        || ai == activity_interrupt::ancestor_hp)
    {
        if ((Options.rest_wait_both && curr->is_resting()
             && !you.is_sufficiently_rested())
            || (Options.rest_wait_ancestor && curr->is_resting()
                && !ancestor_full_hp()))
        {
            return false;
        }
    }

    // Don't interrupt feeding or butchering for monsters already in view.
    if (ai == activity_interrupt::see_monster
        && testbits(at.mons_data->flags, MF_WAS_IN_VIEW))
    {
        return false;
    }

    return ai == activity_interrupt::force
           || Options.activity_interrupts[delay->name()][static_cast<int>(ai)];
}

static string _abyss_monster_creation_message(const monster* mon)
{
    if (mon->type == MONS_DEATH_COB)
    {
        return coinflip() ? " appears in a burst of microwaves!"
                          : " pops from nullspace!";
    }

    // You may ask: "Why these weights?" So would I!
    const vector<pair<string, int>> messages = {
        { " appears in a shower of translocational energy.", 17 },
        { " appears in a shower of sparks.", 34 },
        { " materialises.", 45 },
        { " emerges from chaos.", 13 },
        { " emerges from the beyond.", 26 },
        { make_stringf(" assembles %s!",
                       mon->pronoun(PRONOUN_REFLEXIVE).c_str()), 33 },
        { " erupts from nowhere.", 9 },
        { " bursts from nowhere.", 18 },
        { " is cast out of space.", 7 },
        { " is cast out of reality.", 14 },
        { " coalesces out of pure chaos.", 5 },
        { " coalesces out of seething chaos.", 10 },
        { " punctures the fabric of time!", 2 },
        { " punctures the fabric of the universe.", 7 },
        { make_stringf(" manifests%s!",
                       silenced(you.pos()) ? "" : " with a bang"), 3 },


    };

    return *random_choose_weighted(messages);
}

static inline bool _monster_warning(activity_interrupt ai,
                                    const activity_interrupt_data &at,
                                    shared_ptr<Delay> delay,
                                    vector<string>* msgs_buf = nullptr)
{
    if (ai == activity_interrupt::sense_monster)
    {
        mprf(MSGCH_WARN, "You sense a monster nearby.");
        return true;
    }
    if (ai != activity_interrupt::see_monster)
        return false;
    if (delay && !delay->is_run())
        return false;
    if (at.context != SC_NEWLY_SEEN && !delay)
        return false;

    ASSERT(at.apt == ai_payload::monster);
    monster* mon = at.mons_data;
    ASSERT(mon);
    if (!you.can_see(*mon))
        return false;

    // Disable message for summons.
    if (mon->is_summoned() && !delay)
        return false;

    if (at.context == SC_ALREADY_SEEN || at.context == SC_UNCHARM)
    {
        // Only say "comes into view" if the monster wasn't in view
        // during the previous turn.
        if (testbits(mon->flags, MF_WAS_IN_VIEW) && delay)
        {
            mprf(MSGCH_WARN, "%s is too close now for your liking.",
                 mon->name(DESC_THE).c_str());
        }
    }
    else if (mon->seen_context == SC_JUST_SEEN)
        return false;
    else
    {
        // XXX: This needs to be here to ensure correct messaging for
        // autoexplore, even though the correct place to process it is
        // seen_monster
        view_monster_equipment(mon);

        string text;
        if (mon->has_base_name())
            text = getMiscString(mon->mname + " title");
        else
            text = getMiscString(mon->name(DESC_DBNAME) + " title");
        if (text.empty())
            text = mon->full_name(DESC_A);
        if (mon->type == MONS_PLAYER_GHOST)
        {
            text += make_stringf(" (%s)",
                                 short_ghost_description(mon).c_str());
        }

        if (at.context == SC_DOOR)
            text += " opens the door.";
        else if (at.context == SC_GATE)
            text += " opens the gate.";
        else if (at.context == SC_TELEPORT_IN)
            text += " appears from thin air!";
        else if (at.context == SC_LEAP_IN)
            text += " leaps into view!";
        else if (at.context == SC_FISH_SURFACES)
        {
            text += " bursts forth from the ";
            if (mons_primary_habitat(*mon) == HT_LAVA)
                text += "lava";
            else if (mons_primary_habitat(*mon) == HT_WATER)
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

        bool zin_id = false;
        string god_warning;

        if (have_passive(passive_t::warn_shapeshifter)
            && mon->is_shapeshifter()
            && !(mon->flags & MF_KNOWN_SHIFTER))
        {
            zin_id = true;
            mon->props[ZIN_ID_KEY] = true;
            discover_shifter(*mon);
            god_warning = uppercase_first(god_name(you.religion))
                          + " warns you: "
                          + uppercase_first(mon->pronoun(PRONOUN_SUBJECTIVE))
                          + " "
                          + conjugate_verb("are", mon->pronoun_plurality())
                          + " a foul ";
            if (mon->has_ench(ENCH_GLOWING_SHAPESHIFTER))
                god_warning += "glowing ";
            god_warning += "shapeshifter.";
        }

        // Refresh our monster info cache, so xv shows the ID'd items.
        monster_info mi(mon);
        env.map_knowledge(mon->pos()).set_monster(mi);

        const string mweap = get_monster_equipment_desc(mi, DESC_IDENTIFIED,
                                                        DESC_NONE);

        if (!mweap.empty())
        {
            text += " " + uppercase_first(mon->pronoun(PRONOUN_SUBJECTIVE))
                + " " + conjugate_verb("are", mi.pronoun_plurality())
                + (mweap[0] != ' ' ? " " : "")
                + mweap + ".";
        }

        if (msgs_buf)
            msgs_buf->push_back(text);
        else
        {
            mprf(MSGCH_MONSTER_WARNING, "%s", text.c_str());
            if (zin_id)
                mprf(MSGCH_GOD, "%s", god_warning.c_str());
#ifndef USE_TILE_LOCAL
            if (zin_id)
                update_monster_pane();
#endif
            if (player_under_penance(GOD_GOZAG)
                && !mon->wont_attack()
                && !mon->is_stationary()
                && !mon->is_peripheral())
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
        if (should_shout_at_mons(*mon))
            yell(mon);
        mons_set_just_seen(mon);
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
bool interrupt_activity(activity_interrupt ai,
                        const activity_interrupt_data &at,
                        vector<string>* msgs_buf)
{
    if (interrupt_block::blocked())
        return false;

    const interrupt_block block_recursive_interrupts;
    if (ai == activity_interrupt::hit_monster
        || ai == activity_interrupt::monster_attacks)
    {
        const monster* mon = at.mons_data;
        if (mon && !mon->visible_to(&you))
            autotoggle_autopickup(true);
    }

    if (crawl_state.is_repeating_cmd())
        return interrupt_cmd_repeat(ai, at);

    if (!you_are_delayed())
    {
        // Printing "[foo] comes into view." messages even when not
        // auto-exploring/travelling.
        if (ai == activity_interrupt::see_monster)
            return _monster_warning(ai, at, nullptr, msgs_buf);
        else
            return false;
    }

    const auto delay = current_delay();

    dprf("Activity interrupt: %s", _activity_interrupt_name(ai));

    // First try to stop the current delay.
    if (ai == activity_interrupt::full_hp && !you.running.notified_hp_full)
    {
        you.running.notified_hp_full = true;
        mpr("HP restored.");
    }
    else if (ai == activity_interrupt::full_mp && !you.running.notified_mp_full)
    {
        you.running.notified_mp_full = true;
        mpr("Magic restored.");
    }
    else if (ai == activity_interrupt::ancestor_hp
             && !you.running.notified_ancestor_hp_full)
    {
        // This interrupt only triggers when the ancestor is in LOS,
        // so this message does not leak information.
        you.running.notified_ancestor_hp_full = true;
        mpr("Ancestor HP restored.");
    }

    if (_should_stop_activity(delay.get(), ai, at))
    {
        _monster_warning(ai, at, delay, msgs_buf);
        // Teleport stops stair delays.
        stop_delay(ai == activity_interrupt::teleport);
        quiver::set_needs_redraw();

        return true;
    }

    // Check the other queued delays; the first delay that is interruptible
    // will kill itself and all subsequent delays. This is so that a travel
    // delay stacked behind a delay such as stair/autopickup will be killed
    // correctly by interrupts that the simple stair/autopickup delay ignores.
    for (int i = 1, size = you.delay_queue.size(); i < size; ++i)
    {
        if (_should_stop_activity(you.delay_queue[i].get(), ai, at))
        {
            quiver::set_needs_redraw();
            // Do we have a queued run delay? If we do, flush the delay queue
            // so that stop running Lua notifications happen.
            for (int j = i; j < size; ++j)
            {
                if (you.delay_queue[j]->is_run())
                {
                    _monster_warning(ai, at, you.delay_queue[j], msgs_buf);
                    _clear_pending_delays(i);
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

// Must match the order of activity-interrupt-type.h!
// Also, these names are used in `delay_X` options, so check the options doc
// as well.
static const char *activity_interrupt_names[] =
{
    "force", "keypress", "full_hp", "full_mp", "ancestor_hp", "message",
    "hp_loss", "stat", "monster", "monster_attack", "teleport", "hit_monster",
    "sense_monster", MIMIC_KEY, "ally_attacked", "abyss_exit_spawned"
};

static const char *_activity_interrupt_name(activity_interrupt ai)
{
    COMPILE_CHECK(ARRAYSZ(activity_interrupt_names) == NUM_ACTIVITY_INTERRUPTS);

    if (ai == activity_interrupt::COUNT)
        return "";

    return activity_interrupt_names[static_cast<int>(ai)];
}

activity_interrupt get_activity_interrupt(const string &name)
{
    COMPILE_CHECK(ARRAYSZ(activity_interrupt_names) == NUM_ACTIVITY_INTERRUPTS);

    for (int i = 0; i < NUM_ACTIVITY_INTERRUPTS; ++i)
        if (name == activity_interrupt_names[i])
            return activity_interrupt(i);

    return activity_interrupt::COUNT;
}
