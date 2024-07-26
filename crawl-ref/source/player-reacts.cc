/**
 * @file player_reacts.cc
 * @brief Player functions called every turn, mostly handling enchantment durations/expirations.
 **/

#include "AppHdr.h"

#include "player-reacts.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <sstream>
#include <string>

#ifndef TARGET_OS_WINDOWS
# include <langinfo.h>
#endif
#include <fcntl.h>
#ifdef USE_UNIX_SIGNALS
#include <csignal>
#endif

#include "abyss.h" // abyss_maybe_spawn_xp_exit
#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "beam.h"
#include "cloud.h"
#include "clua.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "database.h"
#include "dbg-util.h"
#include "delay.h"
#ifdef DGL_SIMPLE_MESSAGING
#include "dgl-message.h"
#endif
#include "dlua.h"
#include "dungeon.h"
#include "env.h"
#include "exercise.h"
#include "files.h"
#include "god-abil.h"
#include "god-companions.h"
#include "god-passive.h"
#include "invent.h"
#include "item-prop.h"
#include "item-use.h"
#include "level-state-type.h"
#include "libutil.h"
#include "maps.h"
#include "message.h"
#include "mon-abil.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "mutation.h"
#include "nearby-danger.h"
#include "ouch.h"
#include "player.h"
#include "player-stats.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "shout.h"
#include "skills.h"
#include "spl-cast.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-selfench.h"
#include "spl-other.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "state.h"
#include "status.h"
#include "stepdown.h"
#include "stringutil.h"
#include "terrain.h"
#ifdef USE_TILE
#include "rltiles/tiledef-dngn.h"
#include "tilepick.h"
#endif
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "xom.h"
#include "zot.h" // bezotting

/**
 * Decrement a duration by the given delay.

 * The midloss value should be either 0 or a number of turns where the delay
 * from those turns at normal speed is less than the duration's midpoint. The
 * use of midloss prevents the player from knowing the exact remaining duration
 * when the midpoint message is displayed.
 *
 * @param dur The duration type to be decremented.
 * @param delay The delay aut amount by which to decrement the duration.
 * @param endmsg The message to be displayed when the duration ends.
 * @param midloss A number of normal-speed turns by which to further decrement
 *                the duration if we cross the duration's midpoint.
 * @param endmsg The message to be displayed when the duration is decremented
 *               to a value under its midpoint.
 * @param chan The channel where the endmsg will be printed if the duration
 *             ends.
 *
 * @return  True if the duration ended, false otherwise.
 */

static bool _decrement_a_duration(duration_type dur, int delay,
                                 const char* endmsg = nullptr,
                                 int exploss = 0,
                                 const char* expmsg = nullptr,
                                 msg_channel_type chan = MSGCH_DURATION)
{
    ASSERT(you.duration[dur] >= 0);
    if (you.duration[dur] == 0)
        return false;

    ASSERT(!exploss || expmsg != nullptr);
    const int exppoint = duration_expire_point(dur);
    ASSERTM(!exploss || exploss * BASELINE_DELAY < exppoint,
            "expiration delay loss %d not less than duration expiration point %d",
            exploss * BASELINE_DELAY, exppoint);

    if (dur == DUR_SENTINEL_MARK && aura_is_active_on_player(OPHAN_MARK_KEY))
        return false;

    const int old_dur = you.duration[dur];
    you.duration[dur] -= delay;

    // If we start expiring, handle exploss and print the exppoint message.
    if (you.duration[dur] <= exppoint && old_dur > exppoint)
    {
        you.duration[dur] -= exploss * BASELINE_DELAY;
        if (expmsg)
        {
            // Make sure the player has a turn to react to the midpoint
            // message.
            if (you.duration[dur] <= 0)
                you.duration[dur] = 1;
            if (need_expiration_warning(dur))
                mprf(MSGCH_DANGER, "Careful! %s", expmsg);
            else
                mprf(chan, "%s", expmsg);
        }
    }

    if (you.duration[dur] <= 0)
    {
        you.duration[dur] = 0;
        if (endmsg && *endmsg != '\0')
            mprf(chan, "%s", endmsg);
        return true;
    }

    return false;
}


static void _decrement_petrification(int delay)
{
    if (_decrement_a_duration(DUR_PETRIFIED, delay))
    {
        you.redraw_armour_class = true;
        you.redraw_evasion = true;
        // implicit assumption: all races that can be petrified are made of
        // flesh when not petrified. (Unfortunately, species::skin_name doesn't
        // really work here..)
        const string flesh_equiv = get_form()->flesh_equivalent.empty() ?
                                            "flesh" :
                                            get_form()->flesh_equivalent;

        mprf(MSGCH_DURATION, "You turn to %s%s.",
             flesh_equiv.c_str(),
             you.paralysed() ? "" : " and can move again");

        if (you.props.exists(PETRIFIED_BY_KEY))
            you.props.erase(PETRIFIED_BY_KEY);
    }

    if (you.duration[DUR_PETRIFYING])
    {
        int &dur = you.duration[DUR_PETRIFYING];
        int old_dur = dur;
        if ((dur -= delay) <= 0)
        {
            dur = 0;
            you.fully_petrify();
        }
        else if (dur < 15 && old_dur >= 15)
            mpr("Your limbs are stiffening.");
    }
}

static void _decrement_attraction(int delay)
{
    if (!you.duration[DUR_ATTRACTIVE])
        return;

    attract_monsters(delay);
    if (_decrement_a_duration(DUR_ATTRACTIVE, delay))
        mpr("You feel less attractive to monsters.");
}

static void _decrement_paralysis(int delay)
{
    _decrement_a_duration(DUR_PARALYSIS_IMMUNITY, delay);

    if (!you.duration[DUR_PARALYSIS])
        return;

    _decrement_a_duration(DUR_PARALYSIS, delay);

    if (you.duration[DUR_PARALYSIS])
        return;

    if (you.props.exists(PARALYSED_BY_KEY))
        you.props.erase(PARALYSED_BY_KEY);

    const int immunity = roll_dice(1, 3) * BASELINE_DELAY;
    you.duration[DUR_PARALYSIS_IMMUNITY] = immunity;
    if (you.petrified())
    {
        // no chain paralysis + petrification combos!
        you.duration[DUR_PARALYSIS_IMMUNITY] += you.duration[DUR_PETRIFIED];
        return;
    }

    mprf(MSGCH_DURATION, "You can move again.");
    you.redraw_armour_class = true;
    you.redraw_evasion = true;
}

/**
 * Check whether the player's ice (Ozocubu's) armour was melted this turn.
 * If so, print the appropriate message and clear the flag.
 */
static void _maybe_melt_armour()
{
    // We have to do the messaging here, because a simple wand of flame will
    // call _maybe_melt_player_enchantments twice. It also avoids duplicate
    // messages when melting because of several heat sources.
    if (you.props.exists(MELT_ARMOUR_KEY))
    {
        you.props.erase(MELT_ARMOUR_KEY);
        mprf(MSGCH_DURATION, "The heat melts your icy armour.");
    }
}

// Give a two turn grace period for the sprites to lose interest when no
// valid enemies are in sight (and reset it if enemies come into sight again)
static void _handle_jinxbite_interest()
{
    if (you.duration[DUR_JINXBITE])
    {
        if (!jinxbite_targets_available())
        {
            if (!you.duration[DUR_JINXBITE_LOST_INTEREST])
                you.duration[DUR_JINXBITE_LOST_INTEREST] = 20;
        }
        else
            you.duration[DUR_JINXBITE_LOST_INTEREST] = 0;
    }
}

/**
 * How much horror does the player character feel in the current situation?
 *
 * (For Ru's `MUT_COWARDICE` and for the Sacred Labrys.)
 *
 * Penalties are based on the "scariness" (threat level) of monsters currently
 * visible.
 */
int current_horror_level()
{
    int horror_level = 0;

    for (monster_near_iterator mi(&you, LOS_NO_TRANS); mi; ++mi)
    {
        if (mons_aligned(*mi, &you)
            || !mons_is_threatening(**mi)
            || mons_is_tentacle_or_tentacle_segment(mi->type))
        {
            continue;
        }

        const mon_threat_level_type threat_level = mons_threat_level(**mi);
        if (threat_level == MTHRT_NASTY)
            horror_level += 3;
        else if (threat_level == MTHRT_TOUGH)
            horror_level += 1;
    }
    return horror_level;
}

/**
 * What was the player's most recent horror level?
 *
 * (For Ru's MUT_COWARDICE.)
 */
static int _old_horror_level()
{
    if (you.duration[DUR_HORROR])
        return you.props[HORROR_PENALTY_KEY].get_int();
    return 0;
}

/**
 * When the player should no longer be horrified, end the DUR_HORROR if it
 * exists & cleanup the corresponding prop.
 */
static void _end_horror()
{
    if (!you.duration[DUR_HORROR])
        return;

    you.props.erase(HORROR_PENALTY_KEY);
    you.set_duration(DUR_HORROR, 0);
}

/**
 * Update penalties for cowardice based on the current situation, if the player
 * has Ru's MUT_COWARDICE.
 */
static void _update_cowardice()
{
    if (!you.has_mutation(MUT_COWARDICE))
    {
        // If the player somehow becomes sane again, handle that
        _end_horror();
        return;
    }

    // Subtract one from the horror level so that you don't get a message
    // when a single tough monster appears.
    const int horror_level = max(0, current_horror_level() - 1);

    if (horror_level <= 0)
    {
        // If you were horrified before & aren't now, clean up.
        _end_horror();
        return;
    }

    // Lookup the old value before modifying it
    const int old_horror_level = _old_horror_level();

    // as long as there's still scary enemies, keep the horror going
    you.props[HORROR_PENALTY_KEY] = horror_level;
    you.set_duration(DUR_HORROR, 1);

    // only show a message on increase
    if (horror_level <= old_horror_level)
        return;

    if (horror_level >= HORROR_LVL_OVERWHELMING)
        mpr("Monsters! Monsters everywhere! You have to get out of here!");
    else if (horror_level >= HORROR_LVL_EXTREME)
        mpr("You reel with horror at the sight of these foes!");
    else
        mpr("You feel a twist of horror at the sight of this foe.");
}

// Uskayaw piety decays incredibly fast, but only to a baseline level of *.
// Using Uskayaw abilities can still take you under *.
static void _handle_uskayaw_piety(int time_taken)
{
    if (you.props[USKAYAW_NUM_MONSTERS_HURT].get_int() > 0)
    {
        int num_hurt = you.props[USKAYAW_NUM_MONSTERS_HURT];
        int hurt_val = you.props[USKAYAW_MONSTER_HURT_VALUE];
        int piety_gain = max(num_hurt, stepdown_value(hurt_val, 5, 10, 20, 40));

        gain_piety(piety_gain);
        you.props[USKAYAW_AUT_SINCE_PIETY_GAIN] = 0;
    }
    else if (you.piety > piety_breakpoint(0))
    {
        // If we didn't do a dance action and we can lose piety, we're going
        // to lose piety proportional to the time since the last time we took
        // a dance action and hurt a monster.
        int time_since_gain = you.props[USKAYAW_AUT_SINCE_PIETY_GAIN].get_int();
        time_since_gain += time_taken;

        // Only start losing piety if it's been a few turns since we gained
        // piety, in order to give more tolerance for missing in combat.
        if (time_since_gain > 30)
        {
            int piety_lost = min(you.piety - piety_breakpoint(0),
                    div_rand_round(time_since_gain, 10));

            if (piety_lost > 0)
                lose_piety(piety_lost);

        }
        you.props[USKAYAW_AUT_SINCE_PIETY_GAIN] = time_since_gain;
    }

    // Re-initialize Uskayaw piety variables
    you.props[USKAYAW_NUM_MONSTERS_HURT] = 0;
    you.props[USKAYAW_MONSTER_HURT_VALUE] = 0;
}

static void _handle_uskayaw_time(int time_taken)
{
    _handle_uskayaw_piety(time_taken);

    int audience_timer = you.props[USKAYAW_AUDIENCE_TIMER].get_int();
    int bond_timer = you.props[USKAYAW_BOND_TIMER].get_int();

    // For the timered abilities, if we set the timer to -1, that means we
    // need to trigger the abilities this turn. Otherwise we'll decrement the
    // timer down to a minimum of 0, at which point it becomes eligible to
    // trigger again.
    if (audience_timer == -1 || (you.piety >= piety_breakpoint(2)
            && x_chance_in_y(time_taken, 100 + audience_timer)))
    {
        uskayaw_prepares_audience();
    }
    else
        you.props[USKAYAW_AUDIENCE_TIMER] = max(0, audience_timer - time_taken);

    if (bond_timer == -1 || (you.piety >= piety_breakpoint(3)
            && x_chance_in_y(time_taken, 100 + bond_timer)))
    {
        uskayaw_bonds_audience();
    }
    else
        you.props[USKAYAW_BOND_TIMER] =  max(0, bond_timer - time_taken);
}

/**
 * Player reactions after monster and cloud activities in the turn are finished.
 */
void player_reacts_to_monsters()
{
    // In case Maurice managed to steal a needed item for example.
    if (!you_are_delayed())
        update_can_currently_train();

    check_monster_detect();

    if (have_passive(passive_t::detect_items) || you.has_mutation(MUT_JELLY_GROWTH)
        || you.get_mutation_level(MUT_STRONG_NOSE) > 0)
    {
        detect_items(-1);
    }

    _decrement_attraction(you.time_taken);
    _decrement_paralysis(you.time_taken);
    _decrement_petrification(you.time_taken);
    if (_decrement_a_duration(DUR_SLEEP, you.time_taken))
        you.awaken();

    if (_decrement_a_duration(DUR_GRASPING_ROOTS, you.time_taken)
        && you.is_constricted())
    {
        actor* src = actor_by_mid(you.constricted_by);
        mprf("%s grasping roots sink back into the ground.",
             src ? src->name(DESC_ITS).c_str() : "The");
        you.stop_being_constricted(true);
    }
    if (_decrement_a_duration(DUR_VILE_CLUTCH, you.time_taken)
        && you.is_constricted())
    {
        actor* src = actor_by_mid(you.constricted_by);
        mprf("%s zombie hands return to the earth.",
             src ? src->name(DESC_ITS).c_str() : "The");
        you.stop_being_constricted(true);
    }

    _handle_jinxbite_interest();

    // If you have signalled your allies to stop attacking, cancel this order
    // once there are no longer any enemies in view for 50 consecutive aut
    if (you.pet_target == MHITYOU)
    {
        // Reset the timer if there are hostiles in sight
        if (there_are_monsters_nearby(true, true, false))
            you.duration[DUR_ALLY_RESET_TIMER] = 0;
        else
        {
            if (!you.duration[DUR_ALLY_RESET_TIMER])
                you.duration[DUR_ALLY_RESET_TIMER] = 50;
            else if (_decrement_a_duration(DUR_ALLY_RESET_TIMER, you.time_taken))
                you.pet_target = MHITNOT;
        }
    }

    // Expire Blood For Blood much faster when there are no enemies around for
    // the horde to fight (but not at the tail end of its duration, which could
    // otherwise make the expiry message misleading)
    if (you.duration[DUR_BLOOD_FOR_BLOOD] > 40
        && !there_are_monsters_nearby(true, true, false))
    {
        you.duration[DUR_BLOOD_FOR_BLOOD] -= you.time_taken * 3;
        if (you.duration[DUR_BLOOD_FOR_BLOOD] < 1)
            you.duration[DUR_BLOOD_FOR_BLOOD] = 1;
    }

    _maybe_melt_armour();
    _update_cowardice();
    if (you_worship(GOD_USKAYAW))
        _handle_uskayaw_time(you.time_taken);

    announce_beogh_conversion_offer();

    if (player_in_branch(BRANCH_ARENA) && !okawaru_duel_active())
        okawaru_end_duel();
}

static bool _check_recite()
{
    if (silenced(you.pos())
        || you.paralysed()
        || you.confused()
        || you.asleep()
        || you.petrified()
        || you.berserk())
    {
        mprf(MSGCH_DURATION, "Your recitation is interrupted.");
        you.duration[DUR_RECITE] = 0;
        you.set_duration(DUR_RECITE_COOLDOWN, 1 + random2(10) + random2(30));
        return false;
    }
    return true;
}


static void _handle_recitation(int step)
{
    mprf("\"%s\"",
         zin_recite_text(you.attribute[ATTR_RECITE_SEED],
                         you.attribute[ATTR_RECITE_TYPE], step).c_str());

    if (apply_area_visible(zin_recite_to_single_monster, you.pos()))
    {
        viewwindow();
        update_screen();
    }

    // Recite trains more than once per use, because it has a
    // long timer in between uses and actually takes up multiple
    // turns.
    practise_using_ability(ABIL_ZIN_RECITE);

    noisy(you.shout_volume(), you.pos());

    if (step == 0)
    {
        ostringstream speech;
        speech << zin_recite_text(you.attribute[ATTR_RECITE_SEED],
                                  you.attribute[ATTR_RECITE_TYPE], -1);
        speech << '.';
        if (one_chance_in(27))
        {
            const string closure = getSpeakString("recite_closure");
            if (!closure.empty())
                speech << ' ' << closure;
        }
        mprf(MSGCH_DURATION, "You finish reciting %s", speech.str().c_str());
        you.set_duration(DUR_RECITE_COOLDOWN, 1 + random2(10) + random2(30));
    }
}

/**
 * Try to respawn the player's ancestor, if possible.
 */
static void _try_to_respawn_ancestor()
{
     monster *ancestor = create_monster(hepliaklqana_ancestor_gen_data());
     if (!ancestor)
         return;

    mprf("%s emerges from the mists of memory!",
         ancestor->name(DESC_YOUR).c_str());
    add_companion(ancestor);
    check_place_cloud(CLOUD_MIST, ancestor->pos(), random_range(1,2),
                      ancestor); // ;)
}

static void _decrement_transform_duration(int delay)
{
    if (you.form == you.default_form)
        return;

    // FIXME: [ds] Remove this once we've ensured durations can never go < 0?
    if (you.duration[DUR_TRANSFORMATION] <= 0
        && you.form != transformation::none)
    {
        you.duration[DUR_TRANSFORMATION] = 1;
    }
    // Vampire bat transformations are permanent (until ended), unless they
    // are uncancellable (polymorph wand on a full vampire).
    if (you.get_mutation_level(MUT_VAMPIRISM) < 2
        || you.form != transformation::bat
        || you.transform_uncancellable)
    {
        if (form_can_fly()
            || form_can_swim() && feat_is_water(env.grid(you.pos())))
        {
            // Disable emergency flight if it was active
            you.props.erase(EMERGENCY_FLIGHT_KEY);
        }
        if (_decrement_a_duration(DUR_TRANSFORMATION, delay, nullptr, random2(3),
                                  "Your transformation is almost over."))
        {
            return_to_default_form();
        }
    }
}

static void _decrement_rampage_heal_duration(int delay)
{
    const int heal = you.props[RAMPAGE_HEAL_KEY].get_int();
    if (heal > 0 && _decrement_a_duration(DUR_RAMPAGE_HEAL, delay))
    {
        you.props[RAMPAGE_HEAL_KEY] = heal - 1;
        reset_rampage_heal_duration();
    }
}

/**
 * Take a 'simple' duration, decrement it, and print messages as appropriate
 * when it hits 50% and 0% remaining.
 *
 * @param dur       The duration in question.
 * @param delay     How much to decrement the duration by.
 */
static void _decrement_simple_duration(duration_type dur, int delay)
{
    if (_decrement_a_duration(dur, delay, duration_end_message(dur),
                             duration_expire_offset(dur),
                             duration_expire_message(dur),
                             duration_expire_chan(dur)))
    {
        duration_end_effect(dur);
    }
}


/**
 * Decrement player durations based on how long the player's turn lasted in aut.
 */
static void _decrement_durations()
{
    const int delay = you.time_taken;

    if (you.duration[DUR_STICKY_FLAME])
        dec_sticky_flame_player(delay);

    const bool melted = you.props.exists(MELT_ARMOUR_KEY);
    if (_decrement_a_duration(DUR_ICY_ARMOUR, delay,
                              "Your icy armour evaporates.",
                              melted ? 0 : coinflip(),
                              melted ? nullptr
                              : "Your icy armour starts to melt."))
    {
        if (you.props.exists(ICY_ARMOUR_KEY))
            you.props.erase(ICY_ARMOUR_KEY);
        you.redraw_armour_class = true;
    }

    // Possible reduction of silence radius.
    if (you.duration[DUR_SILENCE])
        invalidate_agrid();
    // and liquefying radius.
    if (you.duration[DUR_LIQUEFYING])
        invalidate_agrid();

    _decrement_transform_duration(delay);

    if (you.attribute[ATTR_SWIFTNESS] >= 0)
    {
        if (_decrement_a_duration(DUR_SWIFTNESS, delay,
                                  "You feel sluggish.", coinflip(),
                                  "You start to feel a little slower."))
        {
            // Start anti-swiftness.
            you.duration[DUR_SWIFTNESS] = you.attribute[ATTR_SWIFTNESS];
            you.attribute[ATTR_SWIFTNESS] = -1;
        }
    }
    else
    {
        if (_decrement_a_duration(DUR_SWIFTNESS, delay,
                                  "You no longer feel sluggish.", coinflip(),
                                  "You start to feel a little faster."))
        {
            you.attribute[ATTR_SWIFTNESS] = 0;
        }
    }

    // Decrement Powered By Death strength
    int pbd_str = you.props[POWERED_BY_DEATH_KEY].get_int();
    if (pbd_str > 0 && _decrement_a_duration(DUR_POWERED_BY_DEATH, delay))
    {
        you.props[POWERED_BY_DEATH_KEY] = pbd_str - 1;
        reset_powered_by_death_duration();
    }

    _decrement_rampage_heal_duration(delay);

    dec_ambrosia_player(delay);
    dec_channel_player(delay);
    dec_slow_player(delay);
    dec_berserk_recovery_player(delay);
    dec_haste_player(delay);

    for (int i = 0; i < NUM_STATS; ++i)
    {
        stat_type s = static_cast<stat_type>(i);
        if (you.stat(s) > 0
            && _decrement_a_duration(stat_zero_duration(s), delay))
        {
            mprf(MSGCH_RECOVERY, "Your %s has recovered.", stat_desc(s, SD_NAME));
            you.redraw_stats[s] = true;
            if (you.duration[DUR_SLOW] == 0)
                mprf(MSGCH_DURATION, "You feel yourself speed up.");
        }
    }

    // Leak piety from the piety pool into actual piety.
    // Note that changes of religious status without corresponding actions
    // (killing monsters, offering items, ...) might be confusing for characters
    // of other religions.
    // For now, though, keep information about what happened hidden.
    if (you.piety < MAX_PIETY && you.duration[DUR_PIETY_POOL] > 0
        && one_chance_in(5))
    {
        you.duration[DUR_PIETY_POOL]--;
        gain_piety(1, 1);

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_SACRIFICE) || defined(DEBUG_PIETY)
        mprf(MSGCH_DIAGNOSTICS, "Piety increases by 1 due to piety pool.");

        if (you.duration[DUR_PIETY_POOL] == 0)
            mprf(MSGCH_DIAGNOSTICS, "Piety pool is now empty.");
#endif
    }

    if (you.duration[DUR_DISJUNCTION])
        disjunction_spell();

    // Should expire before flight.
    if (you.duration[DUR_VORTEX])
    {
        polar_vortex_damage(&you, min(delay, you.duration[DUR_VORTEX]));
        if (_decrement_a_duration(DUR_VORTEX, delay,
                                  "The winds around you start to calm down."))
        {
            you.duration[DUR_VORTEX_COOLDOWN] = random_range(35, 45);
        }
    }

    if (you.duration[DUR_FLIGHT])
    {
        if (!you.permanent_flight())
        {
            if (_decrement_a_duration(DUR_FLIGHT, delay, nullptr, random2(6),
                                      "You are starting to lose your buoyancy."))
            {
                land_player();
            }
            else
            {
                // Disable emergency flight if it was active
                you.props.erase(EMERGENCY_FLIGHT_KEY);
            }
        }
        else if ((you.duration[DUR_FLIGHT] -= delay) <= 0)
        {
            // Just time out potions/spells/miscasts.
            you.duration[DUR_FLIGHT] = 0;
            you.props.erase(EMERGENCY_FLIGHT_KEY);
        }
    }

    if (you.duration[DUR_DEATHS_DOOR]
        && you.attribute[ATTR_DEATHS_DOOR_HP] > 0
        && you.hp > you.attribute[ATTR_DEATHS_DOOR_HP])
    {
        set_hp(you.attribute[ATTR_DEATHS_DOOR_HP]);
        you.redraw_hit_points = true;
    }
    else if (!you.duration[DUR_DEATHS_DOOR]
             && you.attribute[ATTR_DEATHS_DOOR_HP] > 0)
    {
        you.attribute[ATTR_DEATHS_DOOR_HP] = 0;
    }

    if (_decrement_a_duration(DUR_CLOUD_TRAIL, delay,
            "Your trail of clouds dissipates."))
    {
        you.props.erase(XOM_CLOUD_TRAIL_TYPE_KEY);
    }

    if (you.duration[DUR_WATER_HOLD])
        handle_player_drowning(delay);

    if (you.duration[DUR_FLAYED])
    {
        bool near_ghost = false;
        for (monster_iterator mi; mi; ++mi)
        {
            if (mi->type == MONS_FLAYED_GHOST && !mi->wont_attack()
                && you.see_cell(mi->pos()))
            {
                near_ghost = true;
                break;
            }
        }
        if (!near_ghost)
        {
            if (_decrement_a_duration(DUR_FLAYED, delay))
                heal_flayed_effect(&you);
        }
        else if (you.duration[DUR_FLAYED] < 80)
            you.duration[DUR_FLAYED] += div_rand_round(50, delay);
    }

    if (you.duration[DUR_TOXIC_RADIANCE])
        toxic_radiance_effect(&you, min(delay, you.duration[DUR_TOXIC_RADIANCE]));

    if (you.duration[DUR_FATHOMLESS_SHACKLES])
        yred_fathomless_shackles_effect(min(delay, you.duration[DUR_FATHOMLESS_SHACKLES]));

    if (you.duration[DUR_RECITE] && _check_recite())
    {
        const int old_recite =
            (you.duration[DUR_RECITE] + BASELINE_DELAY - 1) / BASELINE_DELAY;
        _decrement_a_duration(DUR_RECITE, delay);
        const int new_recite =
            (you.duration[DUR_RECITE] + BASELINE_DELAY - 1) / BASELINE_DELAY;
        if (old_recite != new_recite)
            _handle_recitation(new_recite);
    }

    if (you.duration[DUR_DRAGON_CALL])
        do_dragon_call(delay);

    if (you.duration[DUR_INFERNAL_LEGION])
        makhleb_infernal_legion_tick(delay);

    if (you.duration[DUR_DOOM_HOWL])
        doom_howl(min(delay, you.duration[DUR_DOOM_HOWL]));

    dec_elixir_player(delay);
    dec_frozen_ramparts(delay);

    if (!you.cannot_act()
        && !you.confused())
    {
        extract_barbs(
            make_stringf("You %s the barbed spikes from your body.",
                you.berserk() ? "rip and tear" : "carefully extract").c_str());
    }

    if (!you.duration[DUR_ANCESTOR_DELAY]
        && have_passive(passive_t::frail)
        && hepliaklqana_ancestor() == MID_NOBODY)
    {
        _try_to_respawn_ancestor();
    }

    const bool sanguine_armour_is_valid = sanguine_armour_valid();
    if (sanguine_armour_is_valid)
        activate_sanguine_armour();
    else if (!sanguine_armour_is_valid && you.duration[DUR_SANGUINE_ARMOUR])
        you.duration[DUR_SANGUINE_ARMOUR] = 1; // expire
    refresh_meek_bonus();

    if (you.props.exists(WU_JIAN_HEAVENLY_STORM_KEY))
    {
        ASSERT(you.duration[DUR_HEAVENLY_STORM]);
        wu_jian_heaven_tick();
    }

    if (you.duration[DUR_TEMP_CLOUD_IMMUNITY])
        _decrement_a_duration(DUR_TEMP_CLOUD_IMMUNITY, delay);

    if (you.duration[DUR_BLOOD_FOR_BLOOD])
        beogh_blood_for_blood_tick(delay);

    if (you.duration[DUR_FUSILLADE] && you.time_taken > 0)
        fire_fusillade();

    // these should be after decr_ambrosia, transforms, liquefying, etc.
    for (int i = 0; i < NUM_DURATIONS; ++i)
        if (duration_decrements_normally((duration_type) i))
            _decrement_simple_duration((duration_type) i, delay);
}

static void _handle_emergency_flight()
{
    ASSERT(you.props[EMERGENCY_FLIGHT_KEY].get_bool());

    if (!is_feat_dangerous(orig_terrain(you.pos()), true, false))
    {
        mpr("You float gracefully downwards.");
        land_player();
        you.props.erase(EMERGENCY_FLIGHT_KEY);
    }
    else
    {
        const int drain = div_rand_round(15 * you.time_taken, BASELINE_DELAY);
        drain_player(drain, true, true);
    }
}

// Regeneration and Magic Regeneration items only work if the player has reached
// max hp/mp while they are being worn. This scans and updates such items when
// the player refills their hp/mp.
static void _maybe_attune_items(bool attune_regen, bool attune_mana_regen)
{
    if (!attune_regen && !attune_mana_regen)
        return;

    vector<string> eq_list;
    bool plural = false;

    bool gained_regen = false;
    bool gained_mana_regen = false;

    for (int slot = EQ_MIN_ARMOUR; slot <= EQ_MAX_WORN; ++slot)
    {
        if (you.melded[slot] || you.equip[slot] == -1 || you.activated[slot])
            continue;
        const item_def &arm = you.inv[you.equip[slot]];

        if ((attune_regen && is_regen_item(arm)
             && (you.magic_points == you.max_magic_points || !is_mana_regen_item(arm)))
            || (attune_mana_regen && is_mana_regen_item(arm)
                && (you.hp == you.hp_max || !is_regen_item(arm))))
        {
            // Track which properties we should notify the player they have gained.
            if (!gained_regen && is_regen_item(arm))
                gained_regen = true;
            if (!gained_mana_regen && is_mana_regen_item(arm))
                gained_mana_regen = true;

            eq_list.push_back(is_artefact(arm) ? get_artefact_name(arm) :
                slot == EQ_AMULET ? "amulet" :
                slot != EQ_BODY_ARMOUR ?
                    item_slot_name(static_cast<equipment_type>(slot)) :
                    "armour");

            if (slot == EQ_BOOTS && arm.sub_type != ARM_BARDING
                || slot == EQ_GLOVES)
            {
                plural = true;
            }
            you.activated.set(slot);
        }
    }

    if (eq_list.empty())
        return;

    const char* msg = (gained_regen && gained_mana_regen) ? " health and magic"
                       : (gained_regen ? "" : " magic");

    plural = plural || eq_list.size() > 1;
    string eq_str = comma_separated_line(eq_list.begin(), eq_list.end());
    mprf("Your %s attune%s to your body and you begin to regenerate%s "
         "more quickly.", eq_str.c_str(), plural ? " themselves" : "s itself",
         msg);
}

// cjo: Handles player hp and mp regeneration. If the counter
// you.hit_points_regeneration is over 100, a loop restores 1 hp and decreases
// the counter by 100 (so you can regen more than 1 hp per turn). If the counter
// is below 100, it is increased by a variable calculated from delay,
// BASELINE_DELAY, and your regeneration rate. MP regeneration happens
// similarly, but the countup depends on delay, BASELINE_DELAY, and
// you.max_magic_points
static void _regenerate_hp_and_mp(int delay)
{
    if (crawl_state.disables[DIS_PLAYER_REGEN])
        return;

    const int old_hp = you.hp;
    const int old_mp = you.magic_points;

    // HP Regeneration
    if (!you.duration[DUR_DEATHS_DOOR])
    {
        const int base_val = player_regen();
        you.hit_points_regeneration += div_rand_round(base_val * delay, BASELINE_DELAY);
    }

    while (you.hit_points_regeneration >= 100)
    {
        // at low mp, "mana link" restores mp in place of hp
        if (you.has_mutation(MUT_MANA_LINK)
            && !x_chance_in_y(you.magic_points, you.max_magic_points))
        {
            inc_mp(1);
        }
        else // standard hp regeneration
            inc_hp(1);
        you.hit_points_regeneration -= 100;
    }

    ASSERT_RANGE(you.hit_points_regeneration, 0, 100);

    // MP Regeneration
    if (player_regenerates_mp())
    {
        if (you.magic_points < you.max_magic_points)
        {
            const int base_val = player_mp_regen();
            int mp_regen_countup = div_rand_round(base_val * delay, BASELINE_DELAY);
            you.magic_points_regeneration += mp_regen_countup;
        }

        while (you.magic_points_regeneration >= 100)
        {
            inc_mp(1);
            you.magic_points_regeneration -= 100;
        }

        ASSERT_RANGE(you.magic_points_regeneration, 0, 100);
    }

    // Update attunement of regeneration items if our hp/mp has refilled.
    _maybe_attune_items(you.hp != old_hp && you.hp == you.hp_max,
                        you.magic_points != old_mp
                        && you.magic_points == you.max_magic_points);
}

static void _handle_fugue(int delay)
{
    if (you.duration[DUR_FUGUE]
        && x_chance_in_y(you.props[FUGUE_KEY].get_int() * delay,
                         9 * BASELINE_DELAY)
        && !silenced(you.pos()))
    {
        // Keep the spam down
        if (you.props[FUGUE_KEY].get_int() < 3 || one_chance_in(5))
            mpr("The wailing of tortured souls fills the air!");
        noisy(spell_effect_noise(SPELL_FUGUE_OF_THE_FALLEN), you.pos());
    }
}

void player_reacts()
{
    // don't allow reactions while stair peeking in descent mode
    if (crawl_state.game_is_descent() && !env.properties.exists(DESCENT_STAIRS_KEY))
        return;

    // This happens as close as possible after the player acts, for better messaging
    if (you_worship(GOD_BEOGH))
        beogh_ally_healing();

    //XXX: does this _need_ to be calculated up here?
    const int stealth = player_stealth();

#ifdef DEBUG_STEALTH
    // Too annoying for regular diagnostics.
    mprf(MSGCH_DIAGNOSTICS, "stealth: %d", stealth);
#endif

    if (you.unrand_reacts.any())
        unrand_reacts();

    _handle_fugue(you.time_taken);
    if (you.has_mutation(MUT_WARMUP_STRIKES))
        you.rev_down(you.time_taken);

    if (x_chance_in_y(you.time_taken, 10 * BASELINE_DELAY))
    {
        const int teleportitis_level = get_teleportitis_level();
        // this is instantaneous
        if (teleportitis_level > 0 && one_chance_in(100 / teleportitis_level))
            you_teleport_now(false, true, "You feel strangely unstable.");
        else if (player_in_branch(BRANCH_ABYSS) && one_chance_in(80)
                 && (!map_masked(you.pos(), MMT_VAULT) || one_chance_in(3)))
        {
            you_teleport_now(); // to new area of the Abyss

            // It's effectively a new level, make a checkpoint save so eventual
            // crashes lose less of the player's progress (and fresh new bad
            // mutations).
            if (!crawl_state.disables[DIS_SAVE_CHECKPOINTS])
                save_game(false);
        }
    }

    abyss_maybe_spawn_xp_exit();

    actor_apply_cloud(&you);
    // Immunity due to just casting Volatile Blastmotes. Only lasts for one
    // turn, so erase it just after we apply clouds for the turn (above).
    if (you.props.exists(BLASTMOTE_IMMUNE_KEY))
        you.props.erase(BLASTMOTE_IMMUNE_KEY);

    actor_apply_toxic_bog(&you);

    _decrement_durations();

    // Translocations and possibly other duration decrements can
    // escape a player from beholders and fearmongers. These should
    // update after.
    you.update_beholders();
    you.update_fearmongers();

    you.handle_constriction();

    _regenerate_hp_and_mp(you.time_taken);

    if (you.duration[DUR_CELEBRANT_COOLDOWN] && you.hp == you.hp_max)
    {
        mprf(MSGCH_DURATION, "You are ready to perform a blood rite again.");
        you.duration[DUR_CELEBRANT_COOLDOWN] = 0;
    }

    if (you.duration[DUR_POISONING])
        handle_player_poison(you.time_taken);

    // safety first: make absolutely sure that there's no mimic underfoot.
    // (this can happen with e.g. apport.)
    discover_mimic(you.pos());

    // Player stealth check.
    seen_monsters_react(stealth);

    // XOM now ticks from here, to increase his reaction time to tension.
    if (you_worship(GOD_XOM))
        xom_tick();
    else if (you_worship(GOD_QAZLAL))
        qazlal_storm_clouds();
    else if (you_worship(GOD_ASHENZARI))
        ash_scrying();

    if (you.props[EMERGENCY_FLIGHT_KEY].get_bool())
        _handle_emergency_flight();

    if (you.duration[DUR_PRIMORDIAL_NIGHTFALL])
        update_vision_range();

    incr_gem_clock();
    incr_zot_clock();
}

void extract_barbs(const char* endmsg)
{
    if (_decrement_a_duration(DUR_BARBS, you.time_taken, endmsg))
    {
        // Note: When this is called in _move player(), ATTR_BARBS_POW
        // has already been used to calculated damage for the player.
        // Otherwise, this prevents the damage.

        you.attribute[ATTR_BARBS_POW] = 0;

        you.props.erase(BARBS_MOVE_KEY);

        // somewhat hacky: ensure that a rest delay can get the right interrupt
        // check when barbs are removed, and all other rest stop conditions are
        // satisfied
        if (you.is_sufficiently_rested())
            interrupt_activity(activity_interrupt::full_hp);
    }
}
