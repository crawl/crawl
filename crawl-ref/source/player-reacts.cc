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
# ifndef __ANDROID__
#  include <langinfo.h>
# endif
#endif
#include <fcntl.h>
#ifdef USE_UNIX_SIGNALS
#include <csignal>
#endif

#include "act-iter.h"
#include "areas.h"
#include "beam.h"
#include "cio.h"
#include "cloud.h"
#include "clua.h"
#include "colour.h"
#include "command.h"
#include "coord.h"
#include "coordit.h"
#include "crash.h"
#include "database.h"
#include "dbg-util.h"
#include "delay.h"
#include "describe.h"
#ifdef DGL_SIMPLE_MESSAGING
#include "dgl-message.h"
#endif
#include "dgn-overview.h"
#include "dgn-shoals.h"
#include "dlua.h"
#include "dungeon.h"
#include "effects.h"
#include "env.h"
#include "evoke.h"
#include "exercise.h"
#include "fight.h"
#include "files.h"
#include "fineff.h"
#include "food.h"
#include "fprop.h"
#include "godabil.h"
#include "godcompanions.h"
#include "godconduct.h"
#include "goditem.h"
#include "godpassive.h"
#include "godprayer.h"
#include "hints.h"
#include "initfile.h"
#include "invent.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "luaterp.h"
#include "macro.h"
#include "map_knowledge.h"
#include "mapmark.h"
#include "maps.h"
#include "melee_attack.h"
#include "message.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-act.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-transit.h"
#include "mon-util.h"
#include "mutation.h"
#include "options.h"
#include "ouch.h"
#include "output.h"
#include "player.h"
#include "player-stats.h"
#include "quiver.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "shout.h"
#include "skills.h"
#include "species.h"
#include "spl-cast.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "spl-wpnench.h"
#include "stairs.h"
#include "startup.h"
#include "stash.h"
#include "state.h"
#include "stringutil.h"
#include "tags.h"
#include "target.h"
#include "terrain.h"
#include "throw.h"
#ifdef USE_TILE
#include "tiledef-dngn.h"
#include "tilepick.h"
#endif
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "version.h"
#include "viewchar.h"
#include "viewgeom.h"
#include "view.h"
#include "viewmap.h"
#include "xom.h"

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
                                 int midloss = 0,
                                 const char* midmsg = nullptr,
                                 msg_channel_type chan = MSGCH_DURATION)
{
    ASSERT(you.duration[dur] >= 0);
    if (you.duration[dur] == 0)
        return false;

    ASSERT(!midloss || midmsg != nullptr);
    const int midpoint = get_expiration_threshold(dur);
    ASSERTM(!midloss || midloss * BASELINE_DELAY < midpoint,
            "midpoint delay loss %d not less than duration midpoint %d",
            midloss * BASELINE_DELAY, midpoint);

    int old_dur = you.duration[dur];
    you.duration[dur] -= delay;

    // If we cross the midpoint, handle midloss and print the midpoint message.
    if (you.duration[dur] <= midpoint && old_dur > midpoint)
    {
        you.duration[dur] -= midloss * BASELINE_DELAY;
        if (midmsg)
        {
            // Make sure the player has a turn to react to the midpoint
            // message.
            if (you.duration[dur] <= 0)
                you.duration[dur] = 1;
            if (need_expiration_warning(dur))
                mprf(MSGCH_DANGER, "Careful! %s", midmsg);
            else
                mprf(chan, "%s", midmsg);
        }
    }

    if (you.duration[dur] <= 0)
    {
        you.duration[dur] = 0;
        if (endmsg)
            mprf(chan, "%s", endmsg);
        return true;
    }

    return false;
}


static void _decrement_petrification(int delay)
{
    if (_decrement_a_duration(DUR_PETRIFIED, delay) && !you.paralysed())
    {
        you.redraw_evasion = true;
        mprf(MSGCH_DURATION, "You turn to %s and can move again.",
             you.form == TRAN_LICH ? "bone" :
             you.form == TRAN_ICE_BEAST ? "ice" :
             "flesh");
    }

    if (you.duration[DUR_PETRIFYING])
    {
        int &dur = you.duration[DUR_PETRIFYING];
        int old_dur = dur;
        if ((dur -= delay) <= 0)
        {
            dur = 0;
            // If we'd kill the player when active flight stops, this will
            // need to pass the killer.  Unlike monsters, almost all flight is
            // magical, inluding tengu, as there's no flapping of wings.  Should
            // we be nasty to dragon and bat forms?  For now, let's not instakill
            // them even if it's inconsistent.
            you.fully_petrify(NULL);
        }
        else if (dur < 15 && old_dur >= 15)
            mpr("Your limbs are stiffening.");
    }
}

static void _decrement_paralysis(int delay)
{
    _decrement_a_duration(DUR_PARALYSIS_IMMUNITY, delay);

    if (you.duration[DUR_PARALYSIS])
    {
        _decrement_a_duration(DUR_PARALYSIS, delay);

        if (!you.duration[DUR_PARALYSIS] && !you.petrified())
        {
            mprf(MSGCH_DURATION, "You can move again.");
            you.redraw_evasion = true;
            you.duration[DUR_PARALYSIS_IMMUNITY] = roll_dice(1, 3)
            * BASELINE_DELAY;
            if (you.props.exists("paralysed_by"))
                you.props.erase("paralysed_by");
        }
    }
}

/**
 * Check whether the player's ice (ozocubu's) armour and/or condensation shield
 * were melted this turn; if so, print the appropriate message.
 */
static void _maybe_melt_armour()
{
    // We have to do the messaging here, because a simple wand of flame will
    // call _maybe_melt_player_enchantments twice. It also avoids duplicate
    // messages when melting because of several heat sources.
    string what;
    if (you.props.exists(MELT_ARMOUR_KEY))
    {
        what = "armour";
        you.props.erase(MELT_ARMOUR_KEY);
    }

    if (you.props.exists(MELT_SHIELD_KEY))
    {
        if (what != "")
            what += " and ";
        what += "shield";
        you.props.erase(MELT_SHIELD_KEY);
    }

    if (what != "")
        mprf(MSGCH_DURATION, "The heat melts your icy %s.", what.c_str());
}

/**
 * How much horror does the player character feel in the current situation?
 *
 * (For Ru's MUT_COWARDICE.)
 *
 * Penalties are based on the "scariness" (threat level) of monsters currently
 * visible.
 */
static int _current_horror_level()
{
    const coord_def& center = you.pos();
    const int radius = 8;
    int horror_level = 0;

    for (radius_iterator ri(center, radius, C_POINTY); ri; ++ri)
    {
        const monster* const mon = monster_at(*ri);

        if (mon == NULL
            || mons_aligned(mon, &you)
            || mons_is_firewood(mon)
            || !you.can_see(mon))
        {
            continue;
        }

        ASSERT(mon);

        const mon_threat_level_type threat_level = mons_threat_level(mon);
        if (threat_level == MTHRT_NASTY)
            horror_level += 3;
        else if (threat_level == MTHRT_TOUGH)
            horror_level += 1;
    }
    // Subtract one from the horror level so that you don't get a message
    // when a single tough monster appears.
    horror_level = max(0, horror_level - 1);
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
    if (!player_mutation_level(MUT_COWARDICE))
    {
        // If the player somehow becomes sane again, handle that
        _end_horror();
        return;
    }

    const int horror_level = _current_horror_level();

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

/**
 * Player reactions after monster and cloud activities in the turn are finished.
 */
void player_reacts_to_monsters()
{
    // In case Maurice managed to steal a needed item for example.
    if (!you_are_delayed())
        update_can_train();

    if (you.duration[DUR_FIRE_SHIELD] > 0)
        manage_fire_shield(you.time_taken);

    check_monster_detect();

    if (in_good_standing(GOD_ASHENZARI) || you.mutation[MUT_JELLY_GROWTH])
        detect_items(-1);

    if (you.duration[DUR_TELEPATHY])
    {
        detect_creatures(1 + you.duration[DUR_TELEPATHY] /
                         (2 * BASELINE_DELAY), true);
    }

    handle_starvation();
    _decrement_paralysis(you.time_taken);
    _decrement_petrification(you.time_taken);
    if (_decrement_a_duration(DUR_SLEEP, you.time_taken))
        you.awake();
    _maybe_melt_armour();
    _update_cowardice();
}

static bool _check_recite()
{
    if (you.hp*2 < you.attribute[ATTR_RECITE_HP]
        || silenced(you.pos())
        || you.paralysed()
        || you.confused()
        || you.asleep()
        || you.petrified()
        || you.berserk())
    {
        zin_recite_interrupt();
        return false;
    }
    return true;
}


static int _zin_recite_to_monsters(coord_def where, int prayertype, int, actor *)
{
    ASSERT_RANGE(prayertype, 0, NUM_RECITE_TYPES);
    return zin_recite_to_single_monster(where);
}


static void _handle_recitation(int step)
{
    mprf("\"%s\"",
         zin_recite_text(you.attribute[ATTR_RECITE_SEED],
                         you.attribute[ATTR_RECITE_TYPE], step).c_str());

    if (apply_area_visible(_zin_recite_to_monsters,
                           you.attribute[ATTR_RECITE_TYPE], &you))
    {
        viewwindow();
    }

    // Recite trains more than once per use, because it has a
    // long timer in between uses and actually takes up multiple
    // turns.
    practise(EX_USED_ABIL, ABIL_ZIN_RECITE);

    noisy(you.shout_volume(), you.pos());

    if (step == 0)
    {
        string speech = zin_recite_text(you.attribute[ATTR_RECITE_SEED],
                                        you.attribute[ATTR_RECITE_TYPE], -1);
        speech += ".";
        if (one_chance_in(9))
        {
            const string closure = getSpeakString("recite_closure");
            if (!closure.empty() && one_chance_in(3))
            {
                speech += " ";
                speech += closure;
            }
        }
        mprf(MSGCH_DURATION, "You finish reciting %s", speech.c_str());
        mpr("You feel short of breath.");
        you.increase_duration(DUR_BREATH_WEAPON, random2(10) + random2(30));
    }
}


//  Perhaps we should write functions like: update_liquid_flames(), etc.
//  Even better, we could have a vector of callback functions (or
//  objects) which get installed at some point.

/**
 * Decrement player durations based on how long the player's turn lasted in aut.
 */
static void _decrement_durations()
{
    int delay = you.time_taken;

    if (you.gourmand())
    {
        // Innate gourmand is always fully active.
        if (player_mutation_level(MUT_GOURMAND) > 0)
            you.duration[DUR_GOURMAND] = GOURMAND_MAX;
        else if (you.duration[DUR_GOURMAND] < GOURMAND_MAX && coinflip())
            you.duration[DUR_GOURMAND] += delay;
    }
    else
        you.duration[DUR_GOURMAND] = 0;

    if (you.duration[DUR_ICEMAIL_DEPLETED] > 0)
    {
        if (delay > you.duration[DUR_ICEMAIL_DEPLETED])
            you.duration[DUR_ICEMAIL_DEPLETED] = 0;
        else
            you.duration[DUR_ICEMAIL_DEPLETED] -= delay;

        if (!you.duration[DUR_ICEMAIL_DEPLETED])
            mprf(MSGCH_DURATION, "Your icy envelope is restored.");

        you.redraw_armour_class = true;
    }

    if (you.duration[DUR_DEMONIC_GUARDIAN] > 0)
    {
        if (delay > you.duration[DUR_DEMONIC_GUARDIAN])
            you.duration[DUR_DEMONIC_GUARDIAN] = 0;
        else
            you.duration[DUR_DEMONIC_GUARDIAN] -= delay;
    }

    if (you.duration[DUR_LIQUID_FLAMES])
        dec_napalm_player(delay);

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

    _decrement_a_duration(DUR_SILENCE, delay, "Your hearing returns.");

    if (_decrement_a_duration(DUR_TROGS_HAND, delay,
                              NULL, coinflip(),
                              "You feel the effects of Trog's Hand fading."))
    {
        trog_remove_trogs_hand();
    }

    _decrement_a_duration(DUR_REGENERATION, delay,
                          "Your skin stops crawling.",
                          coinflip(),
                          "Your skin is crawling a little less now.");

    _decrement_a_duration(DUR_VEHUMET_GIFT, delay);

    if (you.duration[DUR_DIVINE_SHIELD] > 0)
    {
        if (you.duration[DUR_DIVINE_SHIELD] > 1)
        {
            you.duration[DUR_DIVINE_SHIELD] -= delay;
            if (you.duration[DUR_DIVINE_SHIELD] <= 1)
            {
                you.duration[DUR_DIVINE_SHIELD] = 1;
                mprf(MSGCH_DURATION, "Your divine shield starts to fade.");
            }
        }

        if (you.duration[DUR_DIVINE_SHIELD] == 1 && !one_chance_in(3))
        {
            you.redraw_armour_class = true;
            if (--you.attribute[ATTR_DIVINE_SHIELD] == 0)
            {
                you.duration[DUR_DIVINE_SHIELD] = 0;
                mprf(MSGCH_DURATION, "Your divine shield fades away.");
            }
        }
    }

    //jmf: More flexible weapon branding code.
    if (you.duration[DUR_WEAPON_BRAND] > 0)
    {
        you.duration[DUR_WEAPON_BRAND] -= delay;

        if (you.duration[DUR_WEAPON_BRAND] <= 0)
        {
            you.duration[DUR_WEAPON_BRAND] = 1;
            ASSERT(you.weapon());
            end_weapon_brand(*you.weapon(), true);
        }
    }

    // FIXME: [ds] Remove this once we've ensured durations can never go < 0?
    if (you.duration[DUR_TRANSFORMATION] <= 0
        && you.form != TRAN_NONE)
    {
        you.duration[DUR_TRANSFORMATION] = 1;
    }

    // Vampire bat transformations are permanent (until ended), unless they
    // are uncancellable (polymorph wand on a full vampire).
    if (you.species != SP_VAMPIRE || you.form != TRAN_BAT
        || you.duration[DUR_TRANSFORMATION] <= 5 * BASELINE_DELAY
        || you.transform_uncancellable)
    {
        if (_decrement_a_duration(DUR_TRANSFORMATION, delay, NULL, random2(3),
                                  "Your transformation is almost over."))
        {
            untransform();
        }
    }

    // Must come after transformation duration.
    _decrement_a_duration(DUR_BREATH_WEAPON, delay,
                          "You have got your breath back.", 0, NULL,
                          MSGCH_RECOVERY);

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

    _decrement_a_duration(DUR_RESISTANCE, delay,
                          "Your resistance to elements expires.", coinflip(),
                          "You start to feel less resistant.");

    if (_decrement_a_duration(DUR_PHASE_SHIFT, delay,
                              "You are firmly grounded in the material plane once more.",
                              coinflip(),
                              "You feel closer to the material plane."))
    {
        you.redraw_evasion = true;
    }

    _decrement_a_duration(DUR_POWERED_BY_DEATH, delay,
                          "You feel less regenerative.");

    _decrement_a_duration(DUR_TELEPATHY, delay, "You feel less empathic.");

    if (_decrement_a_duration(DUR_CONDENSATION_SHIELD, delay,
                              "Your icy shield evaporates.",
                              coinflip(),
                              "Your icy shield starts to melt."))
    {
        if (you.props.exists(CONDENSATION_SHIELD_KEY))
            you.props.erase(CONDENSATION_SHIELD_KEY);
        you.redraw_armour_class = true;
    }

    if (_decrement_a_duration(DUR_MAGIC_SHIELD, delay,
                              "Your magical shield disappears."))
    {
        you.redraw_armour_class = true;
    }

    if (_decrement_a_duration(DUR_STONESKIN, delay, "Your skin feels tender."))
    {
        if (you.props.exists(STONESKIN_KEY))
            you.props.erase(STONESKIN_KEY);
        you.redraw_armour_class = true;
    }

    if (_decrement_a_duration(DUR_TELEPORT, delay))
    {
        you_teleport_now(true);
        untag_followers();
    }

    _decrement_a_duration(DUR_CONTROL_TELEPORT, delay,
                          "You feel uncertain.", coinflip(),
                          "You start to feel a little uncertain.");

    if (_decrement_a_duration(DUR_DEATH_CHANNEL, delay,
                              "Your unholy channel expires.", coinflip(),
                              "Your unholy channel is weakening."))
    {
        you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = 0;
    }

    _decrement_a_duration(DUR_STEALTH, delay, "You feel less stealthy.");

    if (_decrement_a_duration(DUR_INVIS, delay, NULL,
                              coinflip(), "You flicker for a moment."))
    {
        if (you.invisible())
            mprf(MSGCH_DURATION, "You feel more conspicuous.");
        else
            mprf(MSGCH_DURATION, "You flicker back into view.");
        you.attribute[ATTR_INVIS_UNCANCELLABLE] = 0;
    }

    _decrement_a_duration(DUR_CONF, delay, "You feel less confused.");
    _decrement_a_duration(DUR_LOWERED_MR, delay, "You feel less vulnerable to hostile enchantments.");
    _decrement_a_duration(DUR_SLIMIFY, delay, "You feel less slimy.",
                          coinflip(), "Your slime is starting to congeal.");
    if (_decrement_a_duration(DUR_QUAD_DAMAGE, delay, NULL, 0,
                              "Quad Damage is wearing off."))
    {
        invalidate_agrid(true);
    }
    _decrement_a_duration(DUR_MIRROR_DAMAGE, delay,
                          "Your dark mirror aura disappears.");
    if (_decrement_a_duration(DUR_HEROISM, delay,
                              "You feel like a meek peon again."))
    {
        you.redraw_evasion      = true;
        you.redraw_armour_class = true;
    }

    _decrement_a_duration(DUR_FINESSE, delay,
                          you.hands_act("slow", "down.").c_str());

    _decrement_a_duration(DUR_CONFUSING_TOUCH, delay,
                          you.hands_act("stop", "glowing.").c_str());

    _decrement_a_duration(DUR_SURE_BLADE, delay,
                          "The bond with your blade fades away.");

    _decrement_a_duration(DUR_FORESTED, delay,
                          "Space becomes stable.");

    if (_decrement_a_duration(DUR_MESMERISED, delay,
                              "You break out of your daze.",
                              0, NULL, MSGCH_RECOVERY))
    {
        you.clear_beholders();
    }

    _decrement_a_duration(DUR_MESMERISE_IMMUNE, delay);

    if (_decrement_a_duration(DUR_AFRAID, delay,
                              "Your fear fades away.",
                              0, NULL, MSGCH_RECOVERY))
    {
        you.clear_fearmongers();
    }

    _decrement_a_duration(DUR_FROZEN, delay,
                          "The ice encasing you melts away.",
                          0, NULL, MSGCH_RECOVERY);

    _decrement_a_duration(DUR_NO_POTIONS, delay,
                          you_foodless(true) ? NULL
                                             : "You can drink potions again.",
                          0, NULL, MSGCH_RECOVERY);

    _decrement_a_duration(DUR_NO_SCROLLS, delay,
                          "You can read scrolls again.",
                          0, NULL, MSGCH_RECOVERY);

    dec_slow_player(delay);
    dec_exhaust_player(delay);
    dec_haste_player(delay);

    if (you.duration[DUR_LIQUEFYING] && !you.stand_on_solid_ground())
        you.duration[DUR_LIQUEFYING] = 1;

    if (_decrement_a_duration(DUR_LIQUEFYING, delay,
                              "The ground is no longer liquid beneath you."))
    {
        invalidate_agrid();
    }

    if (_decrement_a_duration(DUR_FORTITUDE, delay,
                              "Your fortitude fades away."))
    {
        notify_stat_change(STAT_STR, -10, true);
    }


    if (_decrement_a_duration(DUR_MIGHT, delay,
                              "You feel a little less mighty now."))
    {
        notify_stat_change(STAT_STR, -5, true);
    }

    if (_decrement_a_duration(DUR_AGILITY, delay,
                              "You feel a little less agile now."))
    {
        notify_stat_change(STAT_DEX, -5, true);
    }

    if (_decrement_a_duration(DUR_BRILLIANCE, delay,
                              "You feel a little less clever now."))
    {
        notify_stat_change(STAT_INT, -5, true);
    }

    if (you.duration[DUR_BERSERK]
        && (_decrement_a_duration(DUR_BERSERK, delay)
            || you.hunger + 100 <= HUNGER_STARVING + BERSERK_NUTRITION))
    {
        mpr("You are no longer berserk.");
        you.duration[DUR_BERSERK] = 0;

        // Sometimes berserk leaves us physically drained.
        //
        // Chance of passing out:
        //     - mutation gives a large plus in order to try and
        //       avoid the mutation being a "death sentence" to
        //       certain characters.

        if (you.berserk_penalty != NO_BERSERK_PENALTY
            && one_chance_in(10 + player_mutation_level(MUT_BERSERK) * 25))
        {
            // Note the beauty of Trog!  They get an extra save that's at
            // the very least 20% and goes up to 100%.
            if (in_good_standing(GOD_TROG)
                && x_chance_in_y(you.piety, piety_breakpoint(5)))
            {
                mpr("Trog's vigour flows through your veins.");
            }
            else
            {
                mprf(MSGCH_WARN, "You pass out from exhaustion.");
                you.increase_duration(DUR_PARALYSIS, roll_dice(1,4));
                you.stop_constricting_all();
            }
        }

        if (!you.duration[DUR_PARALYSIS] && !you.petrified())
            mprf(MSGCH_WARN, "You are exhausted.");

#if TAG_MAJOR_VERSION == 34
        if (you.species == SP_LAVA_ORC)
            mpr("You feel less hot-headed.");
#endif

        // This resets from an actual penalty or from NO_BERSERK_PENALTY.
        you.berserk_penalty = 0;

        int dur = 12 + roll_dice(2, 12);
        // For consistency with slow give exhaustion 2 times the nominal
        // duration.
        you.increase_duration(DUR_EXHAUSTED, dur * 2);

        notify_stat_change(STAT_STR, -5, true);

        // Don't trigger too many hints mode messages.
        const bool hints_slow = Hints.hints_events[HINT_YOU_ENCHANTED];
        Hints.hints_events[HINT_YOU_ENCHANTED] = false;

        slow_player(dur);

        make_hungry(BERSERK_NUTRITION, true);
        you.hunger = max(HUNGER_STARVING - 100, you.hunger);

        // 1KB: No berserk healing.
        set_hp((you.hp + 1) * 2 / 3);
        calc_hp();

        learned_something_new(HINT_POSTBERSERK);
        Hints.hints_events[HINT_YOU_ENCHANTED] = hints_slow;
        you.redraw_quiver = true; // Can throw again.
    }

    if (_decrement_a_duration(DUR_CORONA, delay) && !you.backlit())
        mprf(MSGCH_DURATION, "You are no longer glowing.");

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
    {
        disjunction();
        _decrement_a_duration(DUR_DISJUNCTION, delay,
                              "The translocation energy dissipates.");
        if (!you.duration[DUR_DISJUNCTION])
            invalidate_agrid(true);
    }

    if (_decrement_a_duration(DUR_TORNADO_COOLDOWN, delay,
                              "The winds around you calm down."))
    {
        remove_tornado_clouds(MID_PLAYER);
    }
    // Should expire before flight.
    if (you.duration[DUR_TORNADO])
    {
        tornado_damage(&you, min(delay, you.duration[DUR_TORNADO]));
        _decrement_a_duration(DUR_TORNADO, delay,
                              "The winds around you start to calm down.");
        if (!you.duration[DUR_TORNADO])
            you.duration[DUR_TORNADO_COOLDOWN] = random_range(25, 35);
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
        }
        else if ((you.duration[DUR_FLIGHT] -= delay) <= 0)
        {
            // Just time out potions/spells/miscasts.
            you.attribute[ATTR_FLIGHT_UNCANCELLABLE] = 0;
            you.duration[DUR_FLIGHT] = 0;
        }
    }

    if (you.rotting > 0)
    {
        // XXX: Mummies have an ability (albeit an expensive one) that
        // can fix rotted HPs now... it's probably impossible for them
        // to even start rotting right now, but that could be changed. - bwr
        // It's not normal biology, so Cheibriados won't help.
        if (you.species == SP_MUMMY)
            you.rotting = 0;
        else if (x_chance_in_y(you.rotting, 20)
                 && !you.duration[DUR_DEATHS_DOOR])
        {
            mprf(MSGCH_WARN, "You feel your flesh rotting away.");
            rot_hp(1);
            you.rotting--;
        }
    }

    // ghoul rotting is special, but will deduct from you.rotting
    // if it happens to be positive - because this is placed after
    // the "normal" rotting check, rotting attacks can be somewhat
    // more painful on ghouls - reversing order would make rotting
    // attacks somewhat less painful, but that seems wrong-headed {dlb}:
    if (you.species == SP_GHOUL)
    {
        int resilience = 400;

        if (you_worship(GOD_CHEIBRIADOS) && you.piety >= piety_breakpoint(0))
            resilience = resilience * 3 / 2;

        // Faster rotting when hungry.
        if (you.hunger_state < HS_SATIATED)
            resilience >>= HS_SATIATED - you.hunger_state;

        if (one_chance_in(resilience))
        {
            dprf("rot rate: 1/%d", resilience);
            mprf(MSGCH_WARN, "You feel your flesh rotting away.");
            rot_hp(1);
            if (you.rotting > 0)
                you.rotting--;
        }
    }

    if (you.duration[DUR_DEATHS_DOOR])
    {
        if (you.hp > allowed_deaths_door_hp())
        {
            set_hp(allowed_deaths_door_hp());
            you.redraw_hit_points = true;
        }

        if (_decrement_a_duration(DUR_DEATHS_DOOR, delay,
                                  "Your life is in your own hands again!",
                                  random2(6),
                                  "Your time is quickly running out!"))
        {
            you.increase_duration(DUR_EXHAUSTED, roll_dice(1,3));
        }
    }

    if (_decrement_a_duration(DUR_DIVINE_STAMINA, delay))
        zin_remove_divine_stamina();

    if (_decrement_a_duration(DUR_DIVINE_VIGOUR, delay))
        elyvilon_remove_divine_vigour();

    _decrement_a_duration(DUR_REPEL_STAIRS_MOVE, delay);
    _decrement_a_duration(DUR_REPEL_STAIRS_CLIMB, delay);

    _decrement_a_duration(DUR_COLOUR_SMOKE_TRAIL, 1);

    if (_decrement_a_duration(DUR_SCRYING, delay,
                              "Your astral sight fades away."))
    {
        you.xray_vision = false;
    }

    _decrement_a_duration(DUR_LIFESAVING, delay,
                          "Your divine protection fades away.");

    if (_decrement_a_duration(DUR_DARKNESS, delay,
                              "The ambient light returns to normal.")
        || (you.duration[DUR_DARKNESS] && you.haloed()))
    {
        if (you.duration[DUR_DARKNESS])
        {
            you.duration[DUR_DARKNESS] = 0;
            mpr("The divine light dispels your darkness!");
        }
        update_vision_range();
    }

    _decrement_a_duration(DUR_SHROUD_OF_GOLUBRIA, delay,
                          "Your shroud unravels.",
                          0,
                          "Your shroud begins to fray at the edges.");

    _decrement_a_duration(DUR_INFUSION, delay,
                          "You are no longer magically infusing your attacks.",
                          0,
                          "Your magical infusion is running out.");

    _decrement_a_duration(DUR_SONG_OF_SLAYING, delay,
                          "Your song has ended.",
                          0,
                          "Your song is almost over.");

    _decrement_a_duration(DUR_SENTINEL_MARK, delay,
                          "The sentinel's mark upon you fades away.");

    _decrement_a_duration(DUR_WEAK, delay,
                          "Your attacks no longer feel as feeble.");

    _decrement_a_duration(DUR_DIMENSION_ANCHOR, delay,
                          "You are no longer firmly anchored in space.");

    _decrement_a_duration(DUR_SICKENING, delay);

    _decrement_a_duration(DUR_SAP_MAGIC, delay,
                          "Your magic seems less tainted.");

    _decrement_a_duration(DUR_CLEAVE, delay,
                          "Your cleaving frenzy subsides.");

    if (_decrement_a_duration(DUR_CORROSION, delay,
                          "You repair your equipment."))
    {
        you.props["corrosion_amount"] = 0;
        you.redraw_armour_class = true;
        you.wield_change = true;
    }

    if (!you.duration[DUR_SAP_MAGIC])
    {
        _decrement_a_duration(DUR_MAGIC_SAPPED, delay,
                              "You feel more in control of your magic.");
    }

    _decrement_a_duration(DUR_ANTIMAGIC, delay,
                          "You regain control over your magic.");

    _decrement_a_duration(DUR_WATER_HOLD_IMMUNITY, delay);
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
    {
        int ticks = (you.duration[DUR_TOXIC_RADIANCE] / 10)
        - ((you.duration[DUR_TOXIC_RADIANCE] - delay) / 10);
        toxic_radiance_effect(&you, ticks);
        _decrement_a_duration(DUR_TOXIC_RADIANCE, delay,
                              "Your toxic aura wanes.");
    }

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

    if (you.duration[DUR_GRASPING_ROOTS])
        check_grasping_roots(&you);

    if (you.attribute[ATTR_NEXT_RECALL_INDEX] > 0)
        do_recall(delay);

    _decrement_a_duration(DUR_SLEEP_IMMUNITY, delay);

    _decrement_a_duration(DUR_FIRE_VULN, delay,
                          "You feel less vulnerable to fire.");

    _decrement_a_duration(DUR_POISON_VULN, delay,
                          "You feel less vulnerable to poison.");

    _decrement_a_duration(DUR_NEGATIVE_VULN, delay,
                          "You feel less vulnerable to negative energy.");

    if (_decrement_a_duration(DUR_PORTAL_PROJECTILE, delay,
                              "You are no longer teleporting projectiles to their destination."))
    {
        you.attribute[ATTR_PORTAL_PROJECTILE] = 0;
    }

    _decrement_a_duration(DUR_DRAGON_CALL_COOLDOWN, delay,
                          "You can once more reach out to the dragon horde.");

    if (you.duration[DUR_DRAGON_CALL])
    {
        do_dragon_call(delay);
        if (_decrement_a_duration(DUR_DRAGON_CALL, delay,
                                  "The roar of the dragon horde subsides."))
        {
            you.duration[DUR_DRAGON_CALL_COOLDOWN] = random_range(150, 250);
        }

    }

    if (you.duration[DUR_ABJURATION_AURA])
    {
        do_aura_of_abjuration(delay);
        _decrement_a_duration(DUR_ABJURATION_AURA, delay,
                              "Your aura of abjuration expires.");
    }

    _decrement_a_duration(DUR_QAZLAL_FIRE_RES, delay,
                          "You feel less protected from fire.",
                          coinflip(), "Your protection from fire is fading.");
    _decrement_a_duration(DUR_QAZLAL_COLD_RES, delay,
                          "You feel less protected from cold.",
                          coinflip(), "Your protection from cold is fading.");
    _decrement_a_duration(DUR_QAZLAL_ELEC_RES, delay,
                          "You feel less protected from electricity.",
                          coinflip(),
                          "Your protection from electricity is fading.");
    if (_decrement_a_duration(DUR_QAZLAL_AC, delay,
                              "You feel less protected from physical attacks.",
                              coinflip(),
                              "Your protection from physical attacks is fading."))
    {
        you.redraw_armour_class = true;
    }

    dec_elixir_player(delay);

    if (!env.sunlight.empty())
        process_sunlights();
}


// For worn items; weapons do this on melee attacks.
static void _check_equipment_conducts()
{
    if (you_worship(GOD_DITHMENOS) && one_chance_in(10))
    {
        bool fiery = false;
        const item_def* item;
        for (int i = EQ_MIN_ARMOUR; i < NUM_EQUIP; i++)
        {
            item = you.slot_item(static_cast<equipment_type>(i));
            if (item && is_fiery_item(*item))
            {
                fiery = true;
                break;
            }
        }
        if (fiery)
            did_god_conduct(DID_FIRE, 1, true);
    }
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

    // XXX: using an int tmp to fix the fact that hit_points_regeneration
    // is only an unsigned char and is thus likely to overflow. -- bwr
    int tmp = you.hit_points_regeneration;

    if (you.hp < you.hp_max && !you.duration[DUR_DEATHS_DOOR])
    {
        const int base_val = player_regen();
        tmp += div_rand_round(base_val * delay, BASELINE_DELAY);
    }

    while (tmp >= 100)
    {
        // at low mp, "mana link" restores mp in place of hp
        if (you.mutation[MUT_MANA_LINK]
            && !x_chance_in_y(you.magic_points, you.max_magic_points))
        {
            inc_mp(1);
        }
        else // standard hp regeneration
            inc_hp(1);
        tmp -= 100;
    }

    ASSERT_RANGE(tmp, 0, 100);
    you.hit_points_regeneration = tmp;

    // XXX: Don't let DD use guardian spirit for free HP, since their
    // damage shaving is enough. (due, dpeg)
    if (you.spirit_shield() && you.species == SP_DEEP_DWARF)
        return;

    // XXX: Doing the same as the above, although overflow isn't an
    // issue with magic point regeneration, yet. -- bwr
    tmp = you.magic_points_regeneration;

    if (you.magic_points < you.max_magic_points)
    {
        const int base_val = 7 + you.max_magic_points / 2;
        int mp_regen_countup = div_rand_round(base_val * delay, BASELINE_DELAY);
        if (you.mutation[MUT_MANA_REGENERATION])
            mp_regen_countup *= 2;
        tmp += mp_regen_countup;
    }

    while (tmp >= 100)
    {
        inc_mp(1);
        tmp -= 100;
    }

    ASSERT_RANGE(tmp, 0, 100);
    you.magic_points_regeneration = tmp;
}

void player_reacts()
{
    extern int stealth;             // defined in main.cc

    search_around();

    stealth = check_stealth();

#ifdef DEBUG_STEALTH
    // Too annoying for regular diagnostics.
    mprf(MSGCH_DIAGNOSTICS, "stealth: %d", stealth);
#endif

    if (you.attribute[ATTR_SHADOWS])
        shadow_lantern_effect();

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_LAVA_ORC)
        temperature_check();
#endif

    if (player_mutation_level(MUT_DEMONIC_GUARDIAN))
        check_demonic_guardian();

    _check_equipment_conducts();

    if (you.unrand_reacts.any())
        unrand_reacts();

    // Handle sound-dependent effects that are silenced
    if (silenced(you.pos()))
    {
        if (you.duration[DUR_SONG_OF_SLAYING])
        {
            mpr("The silence causes your song to end.");
            _decrement_a_duration(DUR_SONG_OF_SLAYING, you.duration[DUR_SONG_OF_SLAYING]);
        }
    }

    // Singing makes a continuous noise
    if (you.duration[DUR_SONG_OF_SLAYING])
        noisy(spell_effect_noise(SPELL_SONG_OF_SLAYING), you.pos());

    if (one_chance_in(10))
    {
        const int teleportitis_level = player_teleport();
        // this is instantaneous
        if (teleportitis_level > 0 && one_chance_in(100 / teleportitis_level))
        {
            if (teleportitis_level >= 8)
                you_teleport_now(false);
            else
                you_teleport_now(false, false, teleportitis_level * 5);
        }
        else if (player_in_branch(BRANCH_ABYSS) && one_chance_in(80)
                 && (!map_masked(you.pos(), MMT_VAULT) || one_chance_in(3)))
        {
            you_teleport_now(false); // to new area of the Abyss

            // It's effectively a new level, make a checkpoint save so eventual
            // crashes lose less of the player's progress (and fresh new bad
            // mutations).
            if (!crawl_state.disables[DIS_SAVE_CHECKPOINTS])
                save_game(false);
        }
        else if (you.form == TRAN_WISP && !you.stasis())
            random_blink(false);
    }

    actor_apply_cloud(&you);

    if (env.level_state & LSTATE_SLIMY_WALL)
        slime_wall_damage(&you, you.time_taken);

    // Icy shield and armour melt over lava.
    if (grd(you.pos()) == DNGN_LAVA)
        expose_player_to_element(BEAM_LAVA);

    you.update_beholders();
    you.update_fearmongers();

    _decrement_durations();
    you.handle_constriction();

    // increment constriction durations
    you.accum_has_constricted();

    int capped_time = you.time_taken;
    if (you.walking && capped_time > BASELINE_DELAY)
        capped_time = BASELINE_DELAY;

    int food_use = player_hunger_rate();
    food_use = div_rand_round(food_use * capped_time, BASELINE_DELAY);

    if (food_use > 0 && you.hunger > 0)
        make_hungry(food_use, true);

    _regenerate_hp_and_mp(capped_time);

    dec_disease_player(capped_time);
    if (you.duration[DUR_POISONING])
        handle_player_poison(capped_time);

    recharge_rods(you.time_taken, false);

    // Reveal adjacent mimics.
    for (adjacent_iterator ai(you.pos(), false); ai; ++ai)
        discover_mimic(*ai);

    // Player stealth check.
    seen_monsters_react();

    update_stat_zero();

    // XOM now ticks from here, to increase his reaction time to tension.
    if (you_worship(GOD_XOM))
        xom_tick();
    else if (you_worship(GOD_QAZLAL))
        qazlal_storm_clouds();
}

void extract_manticore_spikes(const char* endmsg)
{
    if (_decrement_a_duration(DUR_BARBS, you.time_taken, endmsg))
    {
        // Note: When this is called in _move player(), ATTR_BARBS_POW
        // has already been used to calculated damage for the player.
        // Otherwise, this prevents the damage.

        you.attribute[ATTR_BARBS_POW] = 0;

        you.props.erase(BARBS_MOVE_KEY);
    }
}
