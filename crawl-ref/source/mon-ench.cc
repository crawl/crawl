/**
 * @file
 * @brief Monster enchantments.
**/

#include "AppHdr.h"

#include "monster.h"

#include <sstream>

#include "act-iter.h"
#include "areas.h"
#include "attitude-change.h"
#include "bloodspatter.h"
#include "cloud.h"
#include "coordit.h"
#include "delay.h"
#include "dgn-shoals.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "fprop.h"
#include "hints.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "message.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-book.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "religion.h"
#include "rot.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-summoning.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "teleport.h"
#include "terrain.h"
#include "timed-effects.h"
#include "traps.h"
#include "unwind.h"
#include "view.h"
#include "xom.h"

static void _place_thunder_ring(const monster &mons)
{
    const cloud_type ctype = CLOUD_STORM;

    for (adjacent_iterator ai(mons.pos()); ai; ++ai)
        if (!cell_is_solid(*ai)
            && (!cloud_at(*ai)
                || cloud_at(*ai)->type == ctype))
        {
            place_cloud(ctype, *ai, 2 + random2(3), &mons);
        }
}

#ifdef DEBUG_DIAGNOSTICS
bool monster::has_ench(enchant_type ench) const
{
    mon_enchant e = get_ench(ench);
    if (e.ench == ench)
    {
        if (!ench_cache[ench])
        {
            die("monster %s has ench '%s' not in cache",
                name(DESC_PLAIN).c_str(),
                string(e).c_str());
        }
    }
    else if (e.ench == ENCH_NONE)
    {
        if (ench_cache[ench])
        {
            die("monster %s has no ench '%s' but cache says it does",
                name(DESC_PLAIN).c_str(),
                string(mon_enchant(ench)).c_str());
        }
    }
    else
    {
        die("get_ench returned '%s' when asked for '%s'",
            string(e).c_str(),
            string(mon_enchant(ench)).c_str());
    }
    return ench_cache[ench];
}
#endif

bool monster::has_ench(enchant_type ench, enchant_type ench2) const
{
    if (ench2 == ENCH_NONE)
        ench2 = ench;

    for (int i = ench; i <= ench2; ++i)
        if (has_ench(static_cast<enchant_type>(i)))
            return true;

    return false;
}

mon_enchant monster::get_ench(enchant_type ench1,
                               enchant_type ench2) const
{
    if (ench2 == ENCH_NONE)
        ench2 = ench1;

    for (int e = ench1; e <= ench2; ++e)
    {
        auto i = enchantments.find(static_cast<enchant_type>(e));

        if (i != enchantments.end())
            return i->second;
    }

    return mon_enchant();
}

void monster::update_ench(const mon_enchant &ench)
{
    if (ench.ench != ENCH_NONE)
    {
        if (mon_enchant *curr_ench = map_find(enchantments, ench.ench))
            *curr_ench = ench;
    }
}

bool monster::add_ench(const mon_enchant &ench)
{
    // silliness
    if (ench.ench == ENCH_NONE)
        return false;

    if (ench.ench == ENCH_FEAR
        && (is_nonliving() || berserk_or_insane()))
    {
        return false;
    }

    if (ench.ench == ENCH_BLIND && !mons_can_be_blinded(type))
        return false;

    if (ench.ench == ENCH_FLIGHT && has_ench(ENCH_LIQUEFYING))
    {
        del_ench(ENCH_LIQUEFYING);
        invalidate_agrid();
    }

    // If we have never changed shape, mark us as shapeshifter, so that
    // "goblin perm_ench:shapeshifter" reverts on death.
    if (ench.ench == ENCH_SHAPESHIFTER)
    {
        if (!props.exists(ORIGINAL_TYPE_KEY))
            props[ORIGINAL_TYPE_KEY].get_int() = MONS_SHAPESHIFTER;
    }
    else if (ench.ench == ENCH_GLOWING_SHAPESHIFTER)
    {
        if (!props.exists(ORIGINAL_TYPE_KEY))
            props[ORIGINAL_TYPE_KEY].get_int() = MONS_GLOWING_SHAPESHIFTER;
    }

    bool new_enchantment = false;
    mon_enchant *added = map_find(enchantments, ench.ench);
    if (added)
        *added += ench;
    else
    {
        new_enchantment = true;
        added = &(enchantments[ench.ench] = ench);
        ench_cache.set(ench.ench, true);
    }

    // If the duration is not set, we must calculate it (depending on the
    // enchantment).
    if (!ench.duration)
        added->set_duration(this, new_enchantment ? nullptr : &ench);

    if (new_enchantment)
        add_enchantment_effect(ench);

    if (ench.ench == ENCH_CHARM
        || ench.ench == ENCH_NEUTRAL_BRIBED
        || ench.ench == ENCH_FRIENDLY_BRIBED
        || ench.ench == ENCH_HEXED)
    {
        align_avatars(true);
    }
    return true;
}

void monster::add_enchantment_effect(const mon_enchant &ench, bool quiet)
{
    // Check for slow/haste.
    switch (ench.ench)
    {
    case ENCH_BERSERK:
        // Inflate hp.
        scale_hp(3, 2);
        // deliberate fall-through

    case ENCH_INSANE:
        if (has_ench(ENCH_SUBMERGED))
            del_ench(ENCH_SUBMERGED);

        calc_speed();
        break;

    case ENCH_HASTE:
        calc_speed();
        break;

    case ENCH_SLOW:
        calc_speed();
        break;

    case ENCH_SUBMERGED:
        dprf("%s submerges.", name(DESC_A, true).c_str());
        break;

    case ENCH_CHARM:
    case ENCH_NEUTRAL_BRIBED:
    case ENCH_FRIENDLY_BRIBED:
    case ENCH_HEXED:
    {
        behaviour = BEH_SEEK;

        actor *source_actor = actor_by_mid(ench.source, true);
        if (!source_actor)
        {
            target = pos();
            foe = MHITNOT;
        }
        else if (source_actor->is_player())
        {
            target = you.pos();
            foe = MHITYOU;
        }
        else
        {
            foe = source_actor->as_monster()->foe;
            if (foe == MHITYOU)
                target = you.pos();
            else if (foe != MHITNOT)
                target = menv[source_actor->as_monster()->foe].pos();
        }

        if (type == MONS_FLAYED_GHOST)
        {
            // temporarly change our attitude back (XXX: scary code...)
            unwind_var<mon_enchant_list> enchants(enchantments, mon_enchant_list{});
            unwind_var<FixedBitVector<NUM_ENCHANTMENTS>> ecache(ench_cache, {});
            end_flayed_effect(this);
        }
        del_ench(ENCH_STILL_WINDS);

        if (is_patrolling())
        {
            // Enslaved monsters stop patrolling and forget their patrol
            // point; they're supposed to follow you now.
            patrol_point.reset();
            firing_pos.reset();
        }
        mons_att_changed(this);
        // clear any constrictions on/by you
        stop_constricting(MID_PLAYER, true);
        you.stop_constricting(mid, true);

        if (invisible() && you.see_cell(pos()) && !you.can_see_invisible()
            && !backlit() && !has_ench(ENCH_SUBMERGED))
        {
            if (!quiet)
            {
                mprf("You %sdetect the %s %s.",
                     friendly() ? "" : "can no longer ",
                     ench.ench == ENCH_HEXED ? "hexed" :
                     ench.ench == ENCH_CHARM ? "charmed"
                                             : "bribed",
                     name(DESC_PLAIN, true).c_str());
            }

            autotoggle_autopickup(!friendly());
            handle_seen_interrupt(this);
        }

        // TODO -- and friends

        if (you.can_see(*this) && friendly())
            learned_something_new(HINT_MONSTER_FRIENDLY, pos());
        break;
    }

    case ENCH_LIQUEFYING:
    case ENCH_SILENCE:
        invalidate_agrid(true);
        break;

    case ENCH_FROZEN:
        calc_speed();
        break;

    case ENCH_INVIS:
        if (testbits(flags, MF_WAS_IN_VIEW))
        {
            went_unseen_this_turn = true;
            unseen_pos = pos();
        }
        break;

    case ENCH_STILL_WINDS:
        start_still_winds();
        break;

    case ENCH_RING_OF_THUNDER:
        _place_thunder_ring(*this);
        mprf(MSGCH_WARN, "A violent storm begins to rage around %s.",
             name(DESC_THE).c_str());
        break;

    default:
        break;
    }
}

static bool _prepare_del_ench(monster* mon, const mon_enchant &me)
{
    if (me.ench != ENCH_SUBMERGED)
        return true;

    int midx = mon->mindex();

    if (!monster_at(mon->pos()))
        mgrd(mon->pos()) = midx;

    if (mon->pos() != you.pos() && midx == mgrd(mon->pos()))
        return true;

    if (midx != mgrd(mon->pos()))
    {
        monster* other_mon = &menv[mgrd(mon->pos())];

        if (other_mon->type == MONS_NO_MONSTER
            || other_mon->type == MONS_PROGRAM_BUG)
        {
            mgrd(mon->pos()) = midx;

            mprf(MSGCH_ERROR, "mgrd(%d,%d) points to %s monster, even "
                 "though it contains submerged monster %s (see bug 2293518)",
                 mon->pos().x, mon->pos().y,
                 other_mon->type == MONS_NO_MONSTER ? "dead" : "buggy",
                 mon->name(DESC_PLAIN, true).c_str());

            if (mon->pos() != you.pos())
                return true;
        }
        else
            mprf(MSGCH_ERROR, "%s tried to unsubmerge while on same square as "
                 "%s (see bug 2293518)", mon->name(DESC_THE, true).c_str(),
                 mon->name(DESC_A, true).c_str());
    }

    // Monster un-submerging while under player or another monster. Try to
    // move to an adjacent square in which the monster could have been
    // submerged and have it unsubmerge from there.
    coord_def target_square;
    int       okay_squares = 0;

    for (adjacent_iterator ai(mon->pos()); ai; ++ai)
        if (!actor_at(*ai)
            && monster_can_submerge(mon, grd(*ai))
            && one_chance_in(++okay_squares))
        {
            target_square = *ai;
        }

    if (okay_squares > 0)
        return mon->move_to_pos(target_square);

    // No available adjacent squares from which the monster could also
    // have unsubmerged. Can it just stay submerged where it is?
    if (monster_can_submerge(mon, grd(mon->pos())))
        return false;

    // The terrain changed and the monster can't remain submerged.
    // Try to move to an adjacent square where it would be happy.
    for (adjacent_iterator ai(mon->pos()); ai; ++ai)
    {
        if (!actor_at(*ai)
            && monster_habitable_grid(mon, grd(*ai))
            && !trap_at(*ai))
        {
            if (one_chance_in(++okay_squares))
                target_square = *ai;
        }
    }

    if (okay_squares > 0)
        return mon->move_to_pos(target_square);

    return true;
}

bool monster::del_ench(enchant_type ench, bool quiet, bool effect)
{
    auto i = enchantments.find(ench);
    if (i == enchantments.end())
        return false;

    const mon_enchant me = i->second;
    const enchant_type et = i->first;

    if (!_prepare_del_ench(this, me))
        return false;

    enchantments.erase(et);
    ench_cache.set(et, false);
    if (effect)
        remove_enchantment_effect(me, quiet);
    return true;
}

void monster::remove_enchantment_effect(const mon_enchant &me, bool quiet)
{
    switch (me.ench)
    {
    case ENCH_TIDE:
        shoals_release_tide(this);
        break;

    case ENCH_INSANE:
        if (mons_is_elven_twin(this))
        {
            monster* twin = mons_find_elven_twin_of(this);
            if (twin && !twin->has_ench(ENCH_INSANE))
                attitude = twin->attitude;
            else
                attitude = ATT_HOSTILE;
        }
        else
            attitude = static_cast<mon_attitude_type>(props["old_attitude"].get_short());
        mons_att_changed(this);
        break;

    case ENCH_BERSERK:
        scale_hp(2, 3);
        calc_speed();
        break;

    case ENCH_HASTE:
        calc_speed();
        if (!quiet)
            simple_monster_message(*this, " is no longer moving quickly.");
        break;

    case ENCH_SWIFT:
        if (!quiet)
        {
            if (type == MONS_ALLIGATOR)
                simple_monster_message(*this, " slows down.");
            else
                simple_monster_message(*this, " is no longer moving quickly.");
        }
        break;

    case ENCH_SILENCE:
        invalidate_agrid();
        if (!quiet && !silenced(pos()))
        {
            if (alive())
                simple_monster_message(*this, " becomes audible again.");
            else
                mprf("As %s %s, the sound returns.",
                     name(DESC_THE).c_str(),
                     wounded_damaged(holiness()) ? "is destroyed" : "dies");
        }
        break;

    case ENCH_MIGHT:
        if (!quiet)
            simple_monster_message(*this, " no longer looks unusually strong.");
        break;

    case ENCH_SLOW:
        if (!quiet)
            simple_monster_message(*this, " is no longer moving slowly.");
        calc_speed();
        break;

    case ENCH_OZOCUBUS_ARMOUR:
        if (!quiet && you.can_see(*this))
        {
            mprf("%s icy armour evaporates.",
                 apostrophise(name(DESC_THE)).c_str());
        }
        break;

    case ENCH_PARALYSIS:
        if (!quiet)
            simple_monster_message(*this, " is no longer paralysed.");

        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_PETRIFIED:
        if (!quiet)
            simple_monster_message(*this, " is no longer petrified.");
        del_ench(ENCH_PETRIFYING);

        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_PETRIFYING:
        fully_petrify(me.agent(), quiet);

        if (alive()) // losing active flight over lava
            behaviour_event(this, ME_EVAL);
        break;

    case ENCH_FEAR:
    {
        string msg;
        if (is_nonliving() || berserk_or_insane())
        {
            // This should only happen because of fleeing sanctuary
            msg = " stops retreating.";
        }
        else if (!mons_is_tentacle_or_tentacle_segment(type))
        {
            msg = " seems to regain " + pronoun(PRONOUN_POSSESSIVE, true)
                                      + " courage.";
        }

        if (!quiet)
            simple_monster_message(*this, msg.c_str());

        // Reevaluate behaviour.
        behaviour_event(this, ME_EVAL);
        break;
    }

    case ENCH_CONFUSION:
        if (!quiet)
            simple_monster_message(*this, " seems less confused.");

        // Reevaluate behaviour.
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_INVIS:
        // Note: Invisible monsters are not forced to stay invisible, so
        // that they can properly have their invisibility removed just
        // before being polymorphed into a non-invisible monster.
        if (you.see_cell(pos()) && !you.can_see_invisible() && !backlit()
            && !has_ench(ENCH_SUBMERGED)
            && !friendly())
        {
            if (!quiet)
                mprf("%s appears from thin air!", name(DESC_A, true).c_str());

            autotoggle_autopickup(false);
            handle_seen_interrupt(this);
        }
        break;

    case ENCH_CHARM:
    case ENCH_NEUTRAL_BRIBED:
    case ENCH_FRIENDLY_BRIBED:
    case ENCH_HEXED:
        if (invisible() && you.see_cell(pos()) && !you.can_see_invisible()
            && !backlit() && !has_ench(ENCH_SUBMERGED))
        {
            if (!quiet)
            {
                if (me.ench == ENCH_CHARM && props.exists("charmed_demon"))
                {
                    mprf("%s breaks free of your control!",
                         name(DESC_THE, true).c_str());
                }
                else
                    mprf("%s is no longer %s.", name(DESC_THE, true).c_str(),
                         me.ench == ENCH_CHARM   ? "charmed"
                         : me.ench == ENCH_HEXED ? "hexed"
                                                 : "bribed");

                mprf("You can %s detect the %s.",
                     friendly() ? "once again" : "no longer",
                     name(DESC_PLAIN, true).c_str());
            }

            autotoggle_autopickup(friendly());
        }
        else
        {
            if (!quiet)
            {
                if (me.ench == ENCH_CHARM && props.exists("charmed_demon"))
                {
                    simple_monster_message(*this,
                                           " breaks free of your control!");
                }
                else
                    simple_monster_message(*this,
                                        me.ench == ENCH_CHARM
                                        ? " is no longer charmed."
                                        : me.ench == ENCH_HEXED
                                        ? " is no longer hexed."
                                        : " is no longer bribed.");
            }

        }

        if (you.can_see(*this))
        {
            // and fire activity interrupts
            interrupt_activity(AI_SEE_MONSTER,
                               activity_interrupt_data(this, SC_UNCHARM));
        }

        if (is_patrolling())
        {
            // Enslaved monsters stop patrolling and forget their patrol point,
            // in case they were on order to wait.
            patrol_point.reset();
        }
        mons_att_changed(this);

        // If a greater demon is breaking free, give the player time to respond
        if (me.ench == ENCH_CHARM && props.exists("charmed_demon"))
        {
            speed_increment -= speed;
            props.erase("charmed_demon");
        }

        // Reevaluate behaviour.
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_CORONA:
    case ENCH_SILVER_CORONA:
    if (!quiet)
        {
            if (visible_to(&you))
                simple_monster_message(*this, " stops glowing.");
            else if (has_ench(ENCH_INVIS) && you.see_cell(pos()))
            {
                mprf("%s stops glowing and disappears.",
                     name(DESC_THE, true).c_str());
            }
        }
        break;

    case ENCH_STICKY_FLAME:
        if (!quiet)
            simple_monster_message(*this, " stops burning.");
        break;

    case ENCH_POISON:
        if (!quiet)
            simple_monster_message(*this, " looks more healthy.");
        break;

    case ENCH_HELD:
    {
        int net = get_trapping_net(pos());
        if (net != NON_ITEM)
        {
            free_stationary_net(net);

            if (props.exists(NEWLY_TRAPPED_KEY))
                props.erase(NEWLY_TRAPPED_KEY);

            if (!quiet)
                simple_monster_message(*this, " breaks free.");
            break;
        }

        monster_web_cleanup(*this, true);
        break;
    }
    case ENCH_FAKE_ABJURATION:
        if (type == MONS_BATTLESPHERE)
            return end_battlesphere(this, false);
    case ENCH_ABJ:
        if (type == MONS_SPECTRAL_WEAPON)
            return end_spectral_weapon(this, false);
        // Set duration to -1 so that monster_die() and any of its
        // callees can tell that the monster ran out of time or was
        // abjured.
        add_ench(mon_enchant(
            (me.ench != ENCH_FAKE_ABJURATION) ?
                ENCH_ABJ : ENCH_FAKE_ABJURATION, 0, 0, -1));

        if (berserk())
            simple_monster_message(*this, " is no longer berserk.");

        monster_die(*this, (me.ench == ENCH_FAKE_ABJURATION) ? KILL_MISC :
                            (quiet) ? KILL_DISMISSED : KILL_RESET, NON_MONSTER);
        break;
    case ENCH_SHORT_LIVED:
        // Conjured ball lightnings explode when they time out.
        suicide();
        monster_die(*this, KILL_TIMEOUT, NON_MONSTER);
        break;
    case ENCH_SUBMERGED:
        if (mons_is_wandering(*this))
        {
            behaviour = BEH_SEEK;
            behaviour_event(this, ME_EVAL);
        }

        if (you.pos() == pos())
        {
            // If, despite our best efforts, it unsubmerged on the same
            // square as the player, teleport it away.
            monster_teleport(this, true, false);
            if (you.pos() == pos())
            {
                mprf(MSGCH_ERROR, "%s is on the same square as you!",
                     name(DESC_A).c_str());
            }
        }

        if (you.can_see(*this))
        {
            if (!quiet && feat_is_watery(grd(pos())))
            {
                mprf(MSGCH_WARN, "%s bursts forth from the water.",
                     name(DESC_A, true).c_str());
                seen_monster(this);
            }
        }
        else if (you.see_cell(pos()) && feat_is_watery(grd(pos())))
        {
            mpr("Something invisible bursts forth from the water.");
            interrupt_activity(AI_FORCE_INTERRUPT);
        }
        break;

    case ENCH_SOUL_RIPE:
        if (!quiet)
        {
            simple_monster_message(*this,
                                   "'s soul is no longer ripe for the taking.");
        }
        break;

    case ENCH_AWAKEN_FOREST:
        env.forest_awoken_until = 0;
        if (!quiet)
            forest_message(pos(), "The forest calms down.");
        break;

    case ENCH_LIQUEFYING:
        invalidate_agrid();

        if (!quiet)
            simple_monster_message(*this, " is no longer liquefying the ground.");
        break;

    case ENCH_FLIGHT:
        apply_location_effects(pos(), me.killer(), me.kill_agent());
        break;

    case ENCH_DAZED:
        if (!quiet && alive())
                simple_monster_message(*this, " is no longer dazed.");
        break;

    case ENCH_INNER_FLAME:
        if (!quiet && alive())
            simple_monster_message(*this, "'s inner flame fades away.");
        break;

    //The following should never happen, but just in case...

    case ENCH_MUTE:
        if (!quiet && alive())
                simple_monster_message(*this, " is no longer mute.");
        break;

    case ENCH_BLIND:
        if (!quiet && alive())
            simple_monster_message(*this, " is no longer blind.");

        // Reevaluate behaviour.
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_DUMB:
        if (!quiet && alive())
            simple_monster_message(*this, " is no longer stupefied.");

        // Reevaluate behaviour.
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_MAD:
        if (!quiet && alive())
            simple_monster_message(*this, " is no longer mad.");

        // Reevaluate behaviour.
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_REGENERATION:
        if (!quiet)
            simple_monster_message(*this, " is no longer regenerating.");
        break;

    case ENCH_RAISED_MR:
        if (!quiet)
        {
            simple_monster_message(*this, " is no longer resistant to hostile "
                                          "enchantments.");
        }
        break;

    case ENCH_WRETCHED:
        if (!quiet)
        {
            const string msg = " seems to return to " +
                               pronoun(PRONOUN_POSSESSIVE, true) +
                               " normal shape.";
            simple_monster_message(*this, msg.c_str());
        }
        break;

    case ENCH_FLAYED:
        heal_flayed_effect(this);
        break;

    case ENCH_HAUNTING:
    {
        mon_enchant abj = get_ench(ENCH_ABJ);
        abj.degree = 1;
        abj.duration = min(5 + random2(30), abj.duration);
        update_ench(abj);
        break;
    }

    case ENCH_WEAK:
        if (!quiet)
            simple_monster_message(*this, " is no longer weakened.");
        break;

    case ENCH_AWAKEN_VINES:
        unawaken_vines(this, quiet);
        break;

    case ENCH_TOXIC_RADIANCE:
        if (!quiet && you.can_see(*this))
            mprf("%s toxic aura wanes.", name(DESC_ITS).c_str());
        break;

    case ENCH_GRASPING_ROOTS_SOURCE:
        if (!quiet && you.see_cell(pos()))
            mpr("The grasping roots settle back into the ground.");

        // Done here to avoid duplicate messages
        if (you.duration[DUR_GRASPING_ROOTS])
            check_grasping_roots(you, true);

        break;

    case ENCH_FIRE_VULN:
        if (!quiet)
            simple_monster_message(*this, " is no longer more vulnerable to fire.");
        break;

    case ENCH_MERFOLK_AVATAR_SONG:
        props.erase("merfolk_avatar_call");
        break;

    case ENCH_POISON_VULN:
        if (!quiet)
            simple_monster_message(*this, " is no longer more vulnerable to poison.");
        break;

    case ENCH_ICEMAIL:
        if (!quiet && you.can_see(*this))
        {
            mprf("%s icy envelope dissipates!",
                 apostrophise(name(DESC_THE)).c_str());
        }
        break;

    case ENCH_AGILE:
        if (!quiet)
            simple_monster_message(*this, " is no longer unusually agile.");
        break;

    case ENCH_FROZEN:
        if (!quiet)
            simple_monster_message(*this, " is no longer encased in ice.");
        calc_speed();
        break;

    case ENCH_BLACK_MARK:
        if (!quiet)
        {
            simple_monster_message(*this, " is no longer absorbing vital"
                                         " energies.");
        }
        calc_speed();
        break;

    case ENCH_SAP_MAGIC:
        if (!quiet)
            simple_monster_message(*this, " is no longer being sapped.");
        break;

    case ENCH_CORROSION:
        if (!quiet)
           simple_monster_message(*this, " is no longer covered in acid.");
        break;

    case ENCH_GOLD_LUST:
        if (!quiet)
           simple_monster_message(*this, " is no longer distracted by gold.");
        break;

    case ENCH_DRAINED:
        if (!quiet)
            simple_monster_message(*this, " seems less drained.");
        break;

    case ENCH_REPEL_MISSILES:
        if (!quiet)
            simple_monster_message(*this, " is no longer repelling missiles.");
        break;

    case ENCH_DEFLECT_MISSILES:
        if (!quiet)
            simple_monster_message(*this, " is no longer deflecting missiles.");
        break;

    case ENCH_RESISTANCE:
        if (!quiet)
            simple_monster_message(*this, " is no longer unusually resistant.");
        break;

    case ENCH_BRILLIANCE_AURA:
        if (!quiet)
            simple_monster_message(*this, " is no longer giving off an aura.");
        break;

    case ENCH_EMPOWERED_SPELLS:
        if (!quiet)
            simple_monster_message(*this, " seems less brilliant.");
        break;

    case ENCH_IDEALISED:
        if (!quiet)
            simple_monster_message(*this, " loses the glow of perfection.");
        break;

    case ENCH_BOUND_SOUL:
        if (!quiet && you.can_see(*this))
            mprf("%s soul is no longer bound.", name(DESC_ITS).c_str());
        break;

    case ENCH_INFESTATION:
        if (!quiet)
            simple_monster_message(*this, " is no longer infested.");
        break;

    case ENCH_STILL_WINDS:
        end_still_winds();
        break;

    default:
        break;
    }
}

bool monster::lose_ench_levels(const mon_enchant &e, int lev, bool infinite)
{
    if (!lev)
        return false;

    if (e.duration >= INFINITE_DURATION && !infinite)
        return false;
    if (e.degree <= lev)
    {
        del_ench(e.ench);
        return true;
    }
    else
    {
        mon_enchant newe(e);
        newe.degree -= lev;
        update_ench(newe);
        return false;
    }
}

bool monster::lose_ench_duration(const mon_enchant &e, int dur)
{
    if (!dur)
        return false;

    if (e.duration >= INFINITE_DURATION)
        return false;
    if (e.duration <= dur)
    {
        del_ench(e.ench);
        return true;
    }
    else
    {
        mon_enchant newe(e);
        newe.duration -= dur;
        update_ench(newe);
        return false;
    }
}

string monster::describe_enchantments() const
{
    ostringstream oss;
    for (auto i = enchantments.begin(); i != enchantments.end(); ++i)
    {
        if (i != enchantments.begin())
            oss << ", ";
        oss << string(i->second);
    }
    return oss.str();
}

bool monster::decay_enchantment(enchant_type en, bool decay_degree)
{
    if (!has_ench(en))
        return false;

    const mon_enchant& me(get_ench(en));

    if (me.duration >= INFINITE_DURATION)
        return false;

    // Gozag-incited haste/berserk is permanent.
    if (has_ench(ENCH_GOZAG_INCITE)
        && (en == ENCH_HASTE || en == ENCH_BERSERK))
    {
        return false;
    }

    // Faster monsters can wiggle out of the net more quickly.
    const int spd = (me.ench == ENCH_HELD) ? speed :
                                             10;
    int actdur = speed_to_duration(spd);

    // Don't let ENCH_SLOW time out while a torpor snail is around.
    if (en == ENCH_SLOW)
    {
        if (torpor_slowed())
            actdur = min(actdur, me.duration - 1);
        else
            props.erase(TORPOR_SLOWED_KEY);
    }

    if (lose_ench_duration(me, actdur))
        return true;

    if (!decay_degree)
        return false;

    // Decay degree so that higher degrees decay faster than lower
    // degrees, and a degree of 1 does not decay (it expires when the
    // duration runs out).
    const int level = me.degree;
    if (level <= 1)
        return false;

    const int decay_factor = level * (level + 1) / 2;
    if (me.duration < me.maxduration * (decay_factor - 1) / decay_factor)
    {
        mon_enchant newme = me;
        --newme.degree;
        newme.maxduration = newme.duration;

        if (newme.degree <= 0)
        {
            del_ench(me.ench);
            return true;
        }
        else
            update_ench(newme);
    }
    return false;
}

bool monster::clear_far_engulf(void)
{
    if (you.duration[DUR_WATER_HOLD]
        && (mid_t) you.props["water_holder"].get_int() == mid)
    {
        you.clear_far_engulf();
    }

    const mon_enchant& me = get_ench(ENCH_WATER_HOLD);
    if (me.ench == ENCH_NONE)
        return false;
    const bool nonadj = !me.agent() || !adjacent(me.agent()->pos(), pos());
    if (nonadj)
        del_ench(ENCH_WATER_HOLD);
    return nonadj;
}

static void _entangle_actor(actor* act)
{
    if (act->is_player())
    {
        you.duration[DUR_GRASPING_ROOTS] = 10;
        you.redraw_evasion = true;
        if (you.duration[DUR_FLIGHT] || you.attribute[ATTR_PERM_FLIGHT])
        {
            you.attribute[ATTR_LAST_FLIGHT_STATUS] =
                you.attribute[ATTR_PERM_FLIGHT];
            you.duration[DUR_FLIGHT] = 0;
            you.attribute[ATTR_PERM_FLIGHT] = 0;
            land_player(true);
        }
    }
    else
    {
        monster* mact = act->as_monster();
        mact->add_ench(mon_enchant(ENCH_GRASPING_ROOTS, 1, nullptr, INFINITE_DURATION));
    }
}

// Returns true if there are any affectable hostiles are in range of the effect
// (whether they were affected or not this round)
static bool _apply_grasping_roots(monster* mons)
{
    if (you.see_cell(mons->pos()) && one_chance_in(12))
    {
        mprf(MSGCH_TALK_VISUAL, "%s", random_choose(
                "Tangled roots snake along the ground.",
                "The ground creaks as gnarled roots bulge its surface.",
                "A root reaches out and grasps at passing movement."));
    }

    bool found_hostile = false;
    for (actor_near_iterator ai(mons, LOS_NO_TRANS); ai; ++ai)
    {
        if (mons_aligned(mons, *ai) || ai->is_insubstantial())
            continue;

        found_hostile = true;

        // Roots can't reach things over deep water or lava
        if (!feat_has_solid_floor(grd(ai->pos())))
            continue;

        // Some messages are suppressed for monsters, to reduce message spam.
        if (ai->airborne())
        {
            if (x_chance_in_y(3, 5))
                continue;

            if (x_chance_in_y(10, 50 - ai->evasion()))
            {
                if (ai->is_player())
                    mpr("Roots rise up to grasp you, but you nimbly evade.");
                continue;
            }

            if (you.can_see(**ai))
            {
                mprf("Roots rise up from beneath %s and drag %s %sto the ground.",
                     ai->name(DESC_THE).c_str(),
                     ai->pronoun(PRONOUN_OBJECTIVE).c_str(),
                     ai->is_monster() ? "" : "back ");
            }
        }
        else if (ai->is_player() && !you.duration[DUR_GRASPING_ROOTS])
        {
            mprf("Roots grasp at your %s, making movement difficult.",
                 you.foot_name(true).c_str());
        }

        _entangle_actor(*ai);
    }

    return found_hostile;
}

// Returns true if you resist the merfolk avatar's call.
static bool _merfolk_avatar_movement_effect(const monster* mons)
{
    bool do_resist = (you.attribute[ATTR_HELD]
                      || you.duration[DUR_TIME_STEP]
                      || you.cannot_act()
                      || you.clarity()
                      || you.is_stationary());

    if (!do_resist)
    {
        // We use a beam tracer here since it is better at navigating
        // obstructing walls than merely comparing our relative positions
        bolt tracer;
        tracer.pierce          = true;
        tracer.affects_nothing = true;
        tracer.target          = mons->pos();
        tracer.source          = you.pos();
        tracer.range           = LOS_RADIUS;
        tracer.is_tracer       = true;
        tracer.aimed_at_spot   = true;
        tracer.fire();

        const coord_def newpos = tracer.path_taken[0];

        if (!in_bounds(newpos)
            || is_feat_dangerous(grd(newpos))
            || !you.can_pass_through_feat(grd(newpos))
            || !cell_see_cell(mons->pos(), newpos, LOS_NO_TRANS))
        {
            do_resist = true;
        }
        else
        {
            bool swapping = false;
            monster* mon = monster_at(newpos);
            if (mon)
            {
                coord_def swapdest;
                if (mon->wont_attack()
                    && !mon->is_stationary()
                    && !mons_is_projectile(*mon)
                    && !mon->cannot_act()
                    && !mon->asleep()
                    && swap_check(mon, swapdest, true))
                {
                    swapping = true;
                }
                else if (!mon->submerged())
                    do_resist = true;
            }

            if (!do_resist)
            {
                const coord_def oldpos = you.pos();
                mpr("The pull of its song draws you forwards.");

                if (swapping)
                {
                    if (monster_at(oldpos))
                    {
                        mprf("Something prevents you from swapping places "
                             "with %s.",
                             mon->name(DESC_THE).c_str());
                        return do_resist;
                    }

                    int swap_mon = mgrd(newpos);
                    // Pick the monster up.
                    mgrd(newpos) = NON_MONSTER;
                    mon->moveto(oldpos);

                    // Plunk it down.
                    mgrd(mon->pos()) = swap_mon;

                    mprf("You swap places with %s.",
                         mon->name(DESC_THE).c_str());
                }
                move_player_to_grid(newpos, true);
                stop_delay(true);

                if (swapping)
                    mon->apply_location_effects(newpos);
            }
        }
    }

    return do_resist;
}

static void _merfolk_avatar_song(monster* mons)
{
    // First, attempt to pull the player, if mesmerised
    if (you.beheld_by(*mons) && coinflip())
    {
        // Don't pull the player if they walked forward voluntarily this
        // turn (to avoid making you jump two spaces at once)
        if (!mons->props["foe_approaching"].get_bool())
        {
            _merfolk_avatar_movement_effect(mons);

            // Reset foe tracking position so that we won't automatically
            // veto pulling on a subsequent turn because you 'approached'
            mons->props["foe_pos"].get_coord() = you.pos();
        }
    }

    // Only call up drowned souls if we're largely alone; otherwise our
    // mesmerisation can support the present allies well enough.
    int ally_hd = 0;
    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (*mi != mons && mons_aligned(mons, *mi) && mons_is_threatening(**mi)
            && mi->type != MONS_DROWNED_SOUL)
        {
            ally_hd += mi->get_experience_level();
        }
    }
    if (ally_hd > mons->get_experience_level())
    {
        if (mons->props.exists("merfolk_avatar_call"))
        {
            // Normally can only happen if allies of the merfolk avatar show up
            // during a song that has already summoned drowned souls (though is
            // technically possible if some existing ally gains HD instead)
            if (you.see_cell(mons->pos()))
                mpr("The shadowy forms in the deep grow still as others approach.");
            mons->props.erase("merfolk_avatar_call");
        }

        return;
    }

    // Can only call up drowned souls if there's free deep water nearby
    vector<coord_def> deep_water;
    for (radius_iterator ri(mons->pos(), LOS_RADIUS, C_SQUARE); ri; ++ri)
        if (grd(*ri) == DNGN_DEEP_WATER && !actor_at(*ri))
            deep_water.push_back(*ri);

    if (deep_water.size())
    {
        if (!mons->props.exists("merfolk_avatar_call"))
        {
            if (you.see_cell(mons->pos()))
            {
                mprf("Shadowy forms rise from the deep at %s song!",
                     mons->name(DESC_ITS).c_str());
            }
            mons->props["merfolk_avatar_call"].get_bool() = true;
        }

        if (coinflip())
        {
            int num = 1 + one_chance_in(4);
            shuffle_array(deep_water);

            int existing = 0;
            for (monster_near_iterator mi(mons); mi; ++mi)
            {
                if (mi->type == MONS_DROWNED_SOUL)
                    existing++;
            }
            num = min(min(num, 5 - existing), int(deep_water.size()));

            for (int i = 0; i < num; ++i)
            {
                monster* soul = create_monster(
                    mgen_data(MONS_DROWNED_SOUL, SAME_ATTITUDE(mons),
                              deep_water[i], mons->foe, MG_FORCE_PLACE)
                    .set_summoned(mons, 1, SPELL_NO_SPELL));

                // Scale down drowned soul damage for low level merfolk avatars
                if (soul)
                    soul->set_hit_dice(mons->get_hit_dice());
            }
        }
    }
}

void monster::apply_enchantment(const mon_enchant &me)
{
    enchant_type en = me.ench;
    switch (me.ench)
    {
    case ENCH_INSANE:
        if (decay_enchantment(en))
        {
            simple_monster_message(*this, " is no longer in an insane frenzy.");
            const int duration = random_range(70, 130);
            add_ench(mon_enchant(ENCH_FATIGUE, 0, 0, duration));
            add_ench(mon_enchant(ENCH_SLOW, 0, 0, duration));
        }
        break;

    case ENCH_BERSERK:
        if (decay_enchantment(en))
        {
            simple_monster_message(*this, " is no longer berserk.");
            const int duration = random_range(70, 130);
            add_ench(mon_enchant(ENCH_FATIGUE, 0, 0, duration));
            add_ench(mon_enchant(ENCH_SLOW, 0, 0, duration));
        }
        break;

    case ENCH_FATIGUE:
        if (decay_enchantment(en))
        {
            simple_monster_message(*this, " looks more energetic.");
            del_ench(ENCH_SLOW, true);
        }
        break;

    case ENCH_SLOW:
    case ENCH_HASTE:
    case ENCH_SWIFT:
    case ENCH_MIGHT:
    case ENCH_FEAR:
    case ENCH_PARALYSIS:
    case ENCH_PETRIFYING:
    case ENCH_PETRIFIED:
    case ENCH_SICK:
    case ENCH_CORONA:
    case ENCH_ABJ:
    case ENCH_CHARM:
    case ENCH_SLEEP_WARY:
    case ENCH_LOWERED_MR:
    case ENCH_SOUL_RIPE:
    case ENCH_TIDE:
    case ENCH_REGENERATION:
    case ENCH_RAISED_MR:
    case ENCH_IDEALISED:
    case ENCH_FEAR_INSPIRING:
    case ENCH_LIFE_TIMER:
    case ENCH_FLIGHT:
    case ENCH_DAZED:
    case ENCH_FAKE_ABJURATION:
    case ENCH_RECITE_TIMER:
    case ENCH_INNER_FLAME:
    case ENCH_MUTE:
    case ENCH_BLIND:
    case ENCH_DUMB:
    case ENCH_MAD:
    case ENCH_BREATH_WEAPON:
    case ENCH_WRETCHED:
    case ENCH_SCREAMED:
    case ENCH_WEAK:
    case ENCH_AWAKEN_VINES:
    case ENCH_FIRE_VULN:
    case ENCH_BARBS:
    case ENCH_POISON_VULN:
    case ENCH_DIMENSION_ANCHOR:
    case ENCH_AGILE:
    case ENCH_FROZEN:
    case ENCH_SAP_MAGIC:
    case ENCH_CORROSION:
    case ENCH_GOLD_LUST:
    case ENCH_DISTRACTED_ACROBATICS:
    case ENCH_RESISTANCE:
    case ENCH_HEXED:
    case ENCH_BRILLIANCE_AURA:
    case ENCH_EMPOWERED_SPELLS:
    case ENCH_ANTIMAGIC:
    case ENCH_BOUND_SOUL:
    case ENCH_INFESTATION:
    case ENCH_BLACK_MARK:
    case ENCH_STILL_WINDS:
    case ENCH_RING_OF_THUNDER:
        decay_enchantment(en);
        break;

    case ENCH_MIRROR_DAMAGE:
        if (decay_enchantment(en))
            simple_monster_message(*this, "'s dark mirror aura disappears.");
        break;

    case ENCH_SILENCE:
    case ENCH_LIQUEFYING:
        decay_enchantment(en);
        invalidate_agrid();
        break;

    case ENCH_DRAINED:
        decay_enchantment(en, false);
        break;

    case ENCH_AQUATIC_LAND:
        // Aquatic monsters lose hit points every turn they spend on dry land.
        ASSERT(mons_habitat(*this) == HT_WATER || mons_habitat(*this) == HT_LAVA);
        if (monster_habitable_grid(this, grd(pos())))
        {
            del_ench(ENCH_AQUATIC_LAND);
            break;
        }

        // Zombies don't take damage from flopping about on land.
        if (mons_is_zombified(*this))
            break;

        hurt(me.agent(), 1 + random2(5), BEAM_NONE);
        break;

    case ENCH_HELD:
        break; // handled in mon-act.cc:struggle_against_net()

    case ENCH_CONFUSION:
        if (!mons_class_flag(type, M_CONFUSED))
            decay_enchantment(en);
        break;

    case ENCH_INVIS:
        if (!mons_class_flag(type, M_INVIS))
            decay_enchantment(en);
        break;

    case ENCH_SUBMERGED:
    {
        // Don't unsubmerge into a harmful cloud
        if (!is_harmless_cloud(cloud_type_at(pos())))
            break;

        // Now we handle the others:
        const dungeon_feature_type grid = grd(pos());

        if (!monster_can_submerge(this, grid))
            del_ench(ENCH_SUBMERGED); // forced to surface
        break;
    }
    case ENCH_POISON:
    {
        const int poisonval = me.degree;
        int dam = (poisonval >= 4) ? 1 : 0;

        if (coinflip())
            dam += roll_dice(1, poisonval + 1);

        if (res_poison() < 0)
            dam += roll_dice(2, poisonval) - 1;

        if (dam > 0)
        {
            dprf("%s takes poison damage: %d (degree %d)",
                 name(DESC_THE).c_str(), dam, me.degree);

            hurt(me.agent(), dam, BEAM_POISON, KILLED_BY_POISON);
        }

        decay_enchantment(en, true);
        break;
    }

    // Assumption: monster::res_fire has already been checked.
    case ENCH_STICKY_FLAME:
    {
        if (feat_is_watery(grd(pos())) && ground_level())
        {
            if (you.can_see(*this))
            {
                mprf("The flames covering %s go out.",
                     name(DESC_THE, false).c_str());
            }
            del_ench(ENCH_STICKY_FLAME);
            break;
        }
        const int dam = resist_adjust_damage(this, BEAM_FIRE,
                                             roll_dice(2, 4) - 1);

        if (dam > 0)
        {
            simple_monster_message(*this, " burns!");
            dprf("sticky flame damage: %d", dam);

            if (mons_genus(type) == MONS_SHEEP)
            {
                for (adjacent_iterator ai(pos()); ai; ++ai)
                {
                    monster *mon = monster_at(*ai);
                    if (mon
                        && (mons_genus(mon->type) == MONS_SHEEP)
                        && !mon->has_ench(ENCH_STICKY_FLAME)
                        && coinflip())
                    {
                        const int dur = me.degree/2 + 1 + random2(me.degree);
                        mon->add_ench(mon_enchant(ENCH_STICKY_FLAME, dur,
                                                  me.agent()));
                        mon->add_ench(mon_enchant(ENCH_FEAR, dur + random2(20),
                                                  me.agent()));
                        if (visible_to(&you))
                            mprf("%s catches fire!", mon->name(DESC_A).c_str());
                        behaviour_event(mon, ME_SCARE, me.agent());
                        xom_is_stimulated(100);
                    }
                }
            }

            hurt(me.agent(), dam, BEAM_STICKY_FLAME);
        }

        decay_enchantment(en, true);
        break;
    }

    case ENCH_SHORT_LIVED:
        // This should only be used for ball lightning -- bwr
        if (decay_enchantment(en))
            suicide();
        break;

    case ENCH_SLOWLY_DYING:
        // If you are no longer dying, you must be dead.
        if (decay_enchantment(en))
        {
            if (you.can_see(*this))
            {
                if (type == MONS_PILLAR_OF_SALT)
                    mprf("%s crumbles away.", name(DESC_THE, false).c_str());
                else if (type == MONS_BLOCK_OF_ICE)
                    mprf("%s melts away.", name(DESC_THE, false).c_str());
                else
                {
                    mprf("A nearby %s withers and dies.",
                         name(DESC_PLAIN, false).c_str());
                }
            }

            monster_die(*this, KILL_MISC, NON_MONSTER, true);
        }
        break;

    case ENCH_SPORE_PRODUCTION:
        // Reduce the timer, if that means we lose the enchantment then
        // spawn a spore and re-add the enchantment.
        if (decay_enchantment(en))
        {
            bool re_add = true;

            for (fair_adjacent_iterator ai(pos()); ai; ++ai)
            {
                if (mons_class_can_pass(MONS_BALLISTOMYCETE_SPORE, grd(*ai))
                    && !actor_at(*ai))
                {
                    beh_type plant_attitude = SAME_ATTITUDE(this);

                    if (monster *plant = create_monster(mgen_data(MONS_BALLISTOMYCETE_SPORE,
                                                            plant_attitude,
                                                            *ai,
                                                            MHITNOT,
                                                            MG_FORCE_PLACE)))
                    {
                        if (mons_is_god_gift(*this, GOD_FEDHAS))
                        {
                            plant->flags |= MF_NO_REWARD;

                            if (plant_attitude == BEH_FRIENDLY)
                            {
                                plant->flags |= MF_ATT_CHANGE_ATTEMPT;

                                mons_make_god_gift(*plant, GOD_FEDHAS);
                            }
                        }

                        plant->behaviour = BEH_WANDER;
                        plant->spore_cooldown = 20;

                        if (you.see_cell(*ai) && you.see_cell(pos()))
                            mpr("A ballistomycete spawns a ballistomycete spore.");

                        // Decrease the count and maybe become inactive
                        // again.
                        if (ballisto_activity)
                        {
                            ballisto_activity--;
                            if (ballisto_activity == 0)
                            {
                                colour = MAGENTA;
                                del_ench(ENCH_SPORE_PRODUCTION);
                                re_add = false;
                            }
                        }

                    }
                    break;
                }
            }
            // Re-add the enchantment (this resets the spore production
            // timer).
            if (re_add)
                add_ench(ENCH_SPORE_PRODUCTION);
        }

        break;

    case ENCH_EXPLODING:
    {
        // Reduce the timer, if that means we lose the enchantment then
        // spawn a spore and re-add the enchantment
        if (decay_enchantment(en))
        {
            monster_type mtype = type;
            bolt beam;

            setup_spore_explosion(beam, *this);

            beam.explode();

            // Ballistos are immune to their own explosions, but must
            // die some time
            this->set_hit_dice(this->get_experience_level() - 1);
            if (this->get_experience_level() <= 0)
                this->self_destruct();

            // The ballisto dying, then a spore being created in its slot
            // env.mons means we can appear to be alive, but in fact be
            // an entirely different monster.
            if (alive() && type == mtype)
                add_ench(ENCH_EXPLODING);
        }

    }
    break;

    case ENCH_PORTAL_TIMER:
    {
        if (decay_enchantment(en))
        {
            coord_def base_position = props["base_position"].get_coord();
            // Do a thing.
            if (you.see_cell(base_position))
                mprf("The portal closes; %s is severed.", name(DESC_THE).c_str());

            if (env.grid(base_position) == DNGN_MALIGN_GATEWAY)
                env.grid(base_position) = DNGN_FLOOR;

            maybe_bloodify_square(base_position);
            add_ench(ENCH_SEVERED);

            // Severed tentacles immediately become "hostile" to everyone (or insane)
            attitude = ATT_NEUTRAL;
            mons_att_changed(this);
            behaviour_event(this, ME_ALERT);
        }
    }
    break;

    case ENCH_PORTAL_PACIFIED:
    {
        if (decay_enchantment(en))
        {
            if (has_ench(ENCH_SEVERED))
                break;

            if (!friendly())
                break;

            if (!silenced(you.pos()))
            {
                if (you.can_see(*this))
                    simple_monster_message(*this, " suddenly becomes enraged!");
                else
                    mpr("You hear a distant and violent thrashing sound.");
            }

            attitude = ATT_HOSTILE;
            mons_att_changed(this);
            behaviour_event(this, ME_ALERT, &you);
        }
    }
    break;

    case ENCH_SEVERED:
    {
        simple_monster_message(*this, " writhes!");
        coord_def base_position = props["base_position"].get_coord();
        maybe_bloodify_square(base_position);
        hurt(me.agent(), 20);
    }

    break;

    case ENCH_GLOWING_SHAPESHIFTER: // This ench never runs out!
        // Number of actions is fine for shapeshifters. Don't change
        // shape while taking the stairs because monster_polymorph() has
        // an assert about it. -cao
        if (!(flags & MF_TAKING_STAIRS)
            && !(paralysed() || petrified() || petrifying() || asleep())
            && (type == MONS_GLOWING_SHAPESHIFTER
                || one_chance_in(4)))
        {
            monster_polymorph(this, RANDOM_MONSTER);
        }
        break;

    case ENCH_SHAPESHIFTER:         // This ench never runs out!
        if (!(flags & MF_TAKING_STAIRS)
            && !(paralysed() || petrified() || petrifying() || asleep())
            && (type == MONS_SHAPESHIFTER
                || x_chance_in_y(1000 / (15 * max(1, get_hit_dice()) / 5),
                                 1000)))
        {
            monster_polymorph(this, RANDOM_MONSTER);
        }
        break;

    case ENCH_TP:
        if (decay_enchantment(en, true) && !no_tele(true, false))
            monster_teleport(this, true);
        break;

    case ENCH_AWAKEN_FOREST:
        forest_damage(this);
        decay_enchantment(en);
        break;

    case ENCH_TORNADO:
        tornado_damage(this, speed_to_duration(speed));
        if (decay_enchantment(en))
        {
            add_ench(ENCH_TORNADO_COOLDOWN);
            if (you.can_see(*this))
            {
                mprf("The winds around %s start to calm down.",
                     name(DESC_THE).c_str());
            }
        }
        break;

    case ENCH_VORTEX:
        tornado_damage(this, speed_to_duration(speed), true);
        if (decay_enchantment(en))
        {
            add_ench(ENCH_VORTEX_COOLDOWN);
            if (you.can_see(*this))
            {
                mprf("The winds around %s start to calm down.",
                     name(DESC_THE).c_str());
            }
        }
        break;

    // This is like Corona, but if silver harms them, it has sticky
    // flame levels of damage.
    case ENCH_SILVER_CORONA:
        if (how_chaotic())
        {
            int dam = roll_dice(2, 4) - 1;
            simple_monster_message(*this, " is seared!");
            dprf("Zin's Corona damage: %d", dam);
            hurt(me.agent(), dam);
        }

        decay_enchantment(en, true);
        break;

    case ENCH_WORD_OF_RECALL:
        // If we've gotten silenced or somehow incapacitated since we started,
        // cancel the recitation
        if (is_silenced() || cannot_act() || has_ench(ENCH_BREATH_WEAPON)
            || confused() || asleep() || has_ench(ENCH_FEAR))
        {
            speed_increment += me.duration;
            del_ench(en, true, false);
            if (you.can_see(*this))
            {
                mprf("%s chant is interrupted.",
                     name(DESC_ITS).c_str());
            }
            break;
        }

        if (decay_enchantment(en))
        {
            int breath_timeout_turns = random_range(4, 12);

            mons_word_of_recall(this, random_range(3, 7));
            add_ench(mon_enchant(ENCH_BREATH_WEAPON, 1, this,
                                 breath_timeout_turns * BASELINE_DELAY));
        }
        break;

    case ENCH_INJURY_BOND:
        // It's hard to absorb someone else's injuries when you're dead
        if (!me.agent() || !me.agent()->alive()
            || me.agent()->mid == MID_ANON_FRIEND)
        {
            del_ench(ENCH_INJURY_BOND, true, false);
        }
        else
            decay_enchantment(en);
        break;

    case ENCH_WATER_HOLD:
        if (!clear_far_engulf())
        {
            if (res_water_drowning() <= 0)
            {
                lose_ench_duration(me, -speed_to_duration(speed));
                int dam = div_rand_round((50 + stepdown((float)me.duration, 30.0))
                                          * speed_to_duration(speed),
                            BASELINE_DELAY * 10);
                if (res_water_drowning() < 0)
                    dam = dam * 3 / 2;
                hurt(me.agent(), dam);
            }
        }
        break;

    case ENCH_FLAYED:
    {
        bool near_ghost = false;
        for (monster_iterator mi; mi; ++mi)
        {
            if (mi->type == MONS_FLAYED_GHOST && !mons_aligned(this, *mi)
                && see_cell(mi->pos()))
            {
                near_ghost = true;
                break;
            }
        }
        if (!near_ghost)
            decay_enchantment(en);

        break;
    }

    case ENCH_HAUNTING:
        if (!me.agent() || !me.agent()->alive())
            del_ench(ENCH_HAUNTING);
        break;

    case ENCH_TOXIC_RADIANCE:
        toxic_radiance_effect(this, 1);
        decay_enchantment(en);
        break;

    case ENCH_GRASPING_ROOTS_SOURCE:
        if (!_apply_grasping_roots(this))
            decay_enchantment(en);
        break;

    case ENCH_GRASPING_ROOTS:
        check_grasping_roots(*this);
        break;

    case ENCH_TORNADO_COOLDOWN:
    case ENCH_VORTEX_COOLDOWN:
        if (decay_enchantment(en))
        {
            remove_tornado_clouds(mid);
            if (you.can_see(*this))
                mprf("The winds around %s calm down.", name(DESC_THE).c_str());
        }
        break;

    case ENCH_MERFOLK_AVATAR_SONG:
        // If we've gotten silenced or somehow incapacitated since we started,
        // cancel the song
        if (silenced(pos()) || paralysed() || petrified()
            || confused() || asleep() || has_ench(ENCH_FEAR))
        {
            del_ench(ENCH_MERFOLK_AVATAR_SONG, true, false);
            if (you.can_see(*this))
            {
                mprf("%s song is interrupted.",
                     name(DESC_ITS).c_str());
            }
            break;
        }

        _merfolk_avatar_song(this);

        // The merfolk avatar will stop singing without her audience
        if (!see_cell_no_trans(you.pos()))
            decay_enchantment(en);

        break;

    case ENCH_PAIN_BOND:
        if (decay_enchantment(en))
        {
            const string msg = " is no longer sharing " +
                               pronoun(PRONOUN_POSSESSIVE, true) +
                               " pain.";
            simple_monster_message(*this, msg.c_str());
        }
        break;

    default:
        break;
    }
}

void monster::mark_summoned(int longevity, bool mark_items, int summon_type, bool abj)
{
    if (abj)
        add_ench(mon_enchant(ENCH_ABJ, longevity));
    if (summon_type != 0)
        add_ench(mon_enchant(ENCH_SUMMON, summon_type, 0, INT_MAX));

    if (mark_items)
        for (mon_inv_iterator ii(*this); ii; ++ii)
            ii->flags |= ISFLAG_SUMMONED;
}

bool monster::is_summoned(int* duration, int* summon_type) const
{
    const mon_enchant abj = get_ench(ENCH_ABJ);
    if (abj.ench == ENCH_NONE)
    {
        if (duration != nullptr)
            *duration = -1;
        if (summon_type != nullptr)
            *summon_type = 0;

        return false;
    }
    if (duration != nullptr)
        *duration = abj.duration;

    const mon_enchant summ = get_ench(ENCH_SUMMON);
    if (summ.ench == ENCH_NONE)
    {
        if (summon_type != nullptr)
            *summon_type = 0;

        return true;
    }
    if (summon_type != nullptr)
        *summon_type = summ.degree;

    if (mons_is_conjured(type))
        return false;

    switch (summ.degree)
    {
    // Temporarily dancing weapons are really there.
    case SPELL_TUKIMAS_DANCE:

    // A corpse/skeleton which was temporarily animated.
    case SPELL_ANIMATE_DEAD:
    case SPELL_ANIMATE_SKELETON:

    // Conjured stuff (fire vortices, ball lightning, IOOD) is handled above.

    // Clones aren't really summoned (though their equipment might be).
    case MON_SUMM_CLONE:

    // Nor are body parts.
    case SPELL_CREATE_TENTACLES:

    // Some object which was animated, and thus not really summoned.
    case MON_SUMM_ANIMATE:
        return false;
    }

    return true;
}

bool monster::is_perm_summoned() const
{
    return testbits(flags, MF_HARD_RESET | MF_NO_REWARD);
}

void monster::apply_enchantments()
{
    if (enchantments.empty())
        return;

    // We process an enchantment only if it existed both at the start of this
    // function and when getting to it in order; any enchantment can add, modify
    // or remove others -- or even itself.
    FixedBitVector<NUM_ENCHANTMENTS> ec = ench_cache;

    // The ordering in enchant_type makes sure that "super-enchantments"
    // like berserk time out before their parts.
    for (int i = 0; i < NUM_ENCHANTMENTS; ++i)
        if (ec[i] && has_ench(static_cast<enchant_type>(i)))
            apply_enchantment(enchantments.find(static_cast<enchant_type>(i))->second);
}

// Used to adjust time durations in calc_duration() for monster speed.
static inline int _mod_speed(int val, int speed)
{
    if (!speed)
        speed = BASELINE_DELAY;
    const int modded = val * BASELINE_DELAY / speed;
    return modded? modded : 1;
}

/////////////////////////////////////////////////////////////////////////
// mon_enchant

static const char *enchant_names[] =
{
    "none", "berserk", "haste", "might", "fatigue", "slow", "fear",
    "confusion", "invis", "poison",
#if TAG_MAJOR_VERSION == 34
    "rot",
#endif
    "summon", "abj", "corona",
    "charm", "sticky_flame", "glowing_shapeshifter", "shapeshifter", "tp",
    "sleep_wary", "submerged", "short_lived", "paralysis", "sick",
#if TAG_MAJOR_VERSION == 34
    "sleepy",
#endif
    "held",
#if TAG_MAJOR_VERSION == 34
     "battle_frenzy", "temp_pacif",
#endif
    "petrifying",
    "petrified", "lowered_mr", "soul_ripe", "slowly_dying",
#if TAG_MAJOR_VERSION == 34
    "eat_items",
#endif
    "aquatic_land", "spore_production",
#if TAG_MAJOR_VERSION == 34
    "slouch",
#endif
    "swift", "tide",
    "insane", "silenced", "awaken_forest", "exploding",
#if TAG_MAJOR_VERSION == 34
    "bleeding",
#endif
    "tethered", "severed", "antimagic",
#if TAG_MAJOR_VERSION == 34
    "fading_away", "preparing_resurrect",
#endif
    "regen",
    "magic_res", "mirror_dam",
#if TAG_MAJOR_VERSION == 34
    "stoneskin",
#endif
    "fear inspiring", "temporarily pacified",
#if TAG_MAJOR_VERSION == 34
    "withdrawn", "attached",
#endif
    "guardian_timer", "flight", "liquefying", "tornado", "fake_abjuration",
    "dazed", "mute", "blind", "dumb", "mad", "silver_corona", "recite timer",
    "inner_flame",
#if TAG_MAJOR_VERSION == 34
    "roused",
#endif
    "breath timer",
#if TAG_MAJOR_VERSION == 34
    "deaths_door", "rolling",
#endif
    "ozocubus_armour", "wretched", "screamed", "rune_of_recall", "injury bond",
    "drowning", "flayed", "haunting",
#if TAG_MAJOR_VERSION == 34
    "retching",
#endif
    "weak", "dimension_anchor", "awaken vines",
#if TAG_MAJOR_VERSION == 34
    "control_winds", "wind_aided",
#endif
    "summon_capped",
    "toxic_radiance", "grasping_roots_source", "grasping_roots",
    "iood_charged", "fire_vuln", "tornado_cooldown", "merfolk_avatar_song",
    "barbs",
#if TAG_MAJOR_VERSION == 34
    "building_charge",
#endif
    "poison_vuln", "icemail", "agile",
    "frozen",
#if TAG_MAJOR_VERSION == 34
    "ephemeral_infusion",
#endif
    "black_mark",
#if TAG_MAJOR_VERSION == 34
    "grand_avatar",
#endif
    "sap magic", "shroud", "phantom_mirror", "bribed", "permabribed",
    "corrosion", "gold_lust", "drained", "repel missiles",
    "deflect missiles",
#if TAG_MAJOR_VERSION == 34
    "negative_vuln", "condensation_shield",
#endif
    "resistant", "hexed", "corpse_armour",
#if TAG_MAJOR_VERSION == 34
    "chanting_fire_storm", "chanting_word_of_entropy",
#endif
    "aura_of_brilliance", "empowered_spells", "gozag_incite", "pain_bond",
    "idealised", "bound_soul", "infestation",
    "stilling the winds", "thunder_ringed", "distracted by acrobatics",
    "vortex", "vortex_cooldown",
    "buggy",
};

static const char *_mons_enchantment_name(enchant_type ench)
{
    COMPILE_CHECK(ARRAYSZ(enchant_names) == NUM_ENCHANTMENTS+1);

    if (ench > NUM_ENCHANTMENTS)
        ench = NUM_ENCHANTMENTS;

    return enchant_names[ench];
}

enchant_type name_to_ench(const char *name)
{
    for (unsigned int i = ENCH_NONE; i < ARRAYSZ(enchant_names); i++)
        if (!strcmp(name, enchant_names[i]))
            return (enchant_type)i;
    return ENCH_NONE;
}

mon_enchant::mon_enchant(enchant_type e, int deg, const actor* a,
                         int dur)
    : ench(e), degree(deg), duration(dur), maxduration(0)
{
    if (a)
    {
        who = a->kill_alignment();
        source = a->mid;
    }
    else
    {
        who = KC_OTHER;
        source = 0;
    }
}

mon_enchant::operator string () const
{
    const actor *a = agent();
    return make_stringf("%s (%d:%d%s %s)",
                        _mons_enchantment_name(ench),
                        degree,
                        duration,
                        kill_category_desc(who),
                        source == MID_ANON_FRIEND ? "anon friend" :
                        source == MID_YOU_FAULTLESS ? "you w/o fault" :
                            a ? a->name(DESC_PLAIN, true).c_str() : "N/A");
}

const char *mon_enchant::kill_category_desc(kill_category k) const
{
    return k == KC_YOU ?      " you" :
           k == KC_FRIENDLY ? " pet" : "";
}

void mon_enchant::merge_killer(kill_category k, mid_t m)
{
    if (who >= k) // prefer the new one
        who = k, source = m;
}

void mon_enchant::cap_degree()
{
    // Sickness & draining are not capped.
    if (ench == ENCH_SICK || ench == ENCH_DRAINED)
        return;

    // Hard cap to simulate old enum behaviour, we should really throw this
    // out entirely.
    const int max = (ench == ENCH_ABJ || ench == ENCH_FAKE_ABJURATION) ?
            MAX_ENCH_DEGREE_ABJURATION : MAX_ENCH_DEGREE_DEFAULT;
    if (degree > max)
        degree = max;
}

mon_enchant &mon_enchant::operator += (const mon_enchant &other)
{
    if (ench == other.ench)
    {
        degree   += other.degree;
        cap_degree();
        duration += other.duration;
        if (duration > INFINITE_DURATION)
            duration = INFINITE_DURATION;
        merge_killer(other.who, other.source);
    }
    return *this;
}

mon_enchant mon_enchant::operator + (const mon_enchant &other) const
{
    mon_enchant tmp(*this);
    tmp += other;
    return tmp;
}

killer_type mon_enchant::killer() const
{
    return who == KC_YOU      ? KILL_YOU :
           who == KC_FRIENDLY ? KILL_MON
                              : KILL_MISC;
}

int mon_enchant::kill_agent() const
{
    return who == KC_FRIENDLY ? ANON_FRIENDLY_MONSTER : 0;
}

actor* mon_enchant::agent() const
{
    return find_agent(source, who);
}

int mon_enchant::modded_speed(const monster* mons, int hdplus) const
{
    return _mod_speed(mons->get_hit_dice() + hdplus, mons->speed);
}

int mon_enchant::calc_duration(const monster* mons,
                               const mon_enchant *added) const
{
    int cturn = 0;

    const int newdegree = added ? added->degree : degree;
    const int deg = newdegree ? newdegree : 1;

    // Beneficial enchantments (like Haste) should not be throttled by
    // monster HD via modded_speed(). Use _mod_speed instead!
    switch (ench)
    {
    case ENCH_SWIFT:
        cturn = 1000 / _mod_speed(25, mons->speed);
        break;
    case ENCH_HASTE:
    case ENCH_MIGHT:
    case ENCH_INVIS:
    case ENCH_FEAR_INSPIRING:
    case ENCH_AGILE:
    case ENCH_BLACK_MARK:
    case ENCH_RESISTANCE:
    case ENCH_IDEALISED:
    case ENCH_BOUND_SOUL:
    case ENCH_RING_OF_THUNDER:
        cturn = 1000 / _mod_speed(25, mons->speed);
        break;
    case ENCH_LIQUEFYING:
    case ENCH_SILENCE:
    case ENCH_REGENERATION:
    case ENCH_RAISED_MR:
    case ENCH_MIRROR_DAMAGE:
    case ENCH_SAP_MAGIC:
    case ENCH_STILL_WINDS:
        cturn = 300 / _mod_speed(25, mons->speed);
        break;
    case ENCH_SLOW:
    case ENCH_CORROSION:
        cturn = 250 / (1 + modded_speed(mons, 10));
        break;
    case ENCH_FEAR:
        cturn = 150 / (1 + modded_speed(mons, 5));
        break;
    case ENCH_PARALYSIS:
        cturn = max(90 / modded_speed(mons, 5), 3);
        break;
    case ENCH_PETRIFIED:
        cturn = max(8, 150 / (1 + modded_speed(mons, 5)));
        break;
    case ENCH_DAZED:
    case ENCH_PETRIFYING:
        cturn = 50 / _mod_speed(10, mons->speed);
        break;
    case ENCH_CONFUSION:
        cturn = max(100 / modded_speed(mons, 5), 3);
        break;
    case ENCH_HELD:
        cturn = 120 / _mod_speed(25, mons->speed);
        break;
    case ENCH_POISON:
        cturn = 1000 * deg / _mod_speed(125, mons->speed);
        break;
    case ENCH_STICKY_FLAME:
        cturn = 1000 * deg / _mod_speed(200, mons->speed);
        break;
    case ENCH_CORONA:
    case ENCH_SILVER_CORONA:
        if (deg > 1)
            cturn = 1000 * (deg - 1) / _mod_speed(200, mons->speed);
        cturn += 1000 / _mod_speed(100, mons->speed);
        break;
    case ENCH_SHORT_LIVED:
        cturn = 1200 / _mod_speed(200, mons->speed);
        break;
    case ENCH_SLOWLY_DYING:
        // This may be a little too direct but the randomization at the end
        // of this function is excessive for toadstools. -cao
        return (2 * FRESHEST_CORPSE + random2(10))
                  * speed_to_duration(mons->speed);
    case ENCH_SPORE_PRODUCTION:
        // This is used as a simple timer, when the enchantment runs out
        // the monster will create a ballistomycete spore.
        return random_range(475, 525) * 10;

    case ENCH_EXPLODING:
        return random_range(3, 7) * 10;

    case ENCH_PORTAL_PACIFIED:
        // Must be set by spell.
        return 0;

    case ENCH_BREATH_WEAPON:
        // Must be set by creature.
        return 0;

    case ENCH_PORTAL_TIMER:
        cturn = 30 * 10 / _mod_speed(10, mons->speed);
        break;

    case ENCH_FAKE_ABJURATION:
    case ENCH_ABJ:
        // The duration is:
        // deg = 1     90 aut
        // deg = 2    180 aut
        // deg = 3    270 aut
        // deg = 4    360 aut
        // deg = 5    810 aut
        // deg = 6   1710 aut
        // with a large fuzz
        if (deg >= 6)
            cturn = 1000 / _mod_speed(10, mons->speed);
        if (deg >= 5)
            cturn += 1000 / _mod_speed(20, mons->speed);
        cturn += 1000 * min(4, deg) / _mod_speed(100, mons->speed);
        break;
    case ENCH_CHARM:
    case ENCH_HEXED:
        cturn = 500 / modded_speed(mons, 10);
        break;
    case ENCH_TP:
        cturn = 1000 * deg / _mod_speed(1000, mons->speed);
        break;
    case ENCH_SLEEP_WARY:
        cturn = 1000 / _mod_speed(50, mons->speed);
        break;
    case ENCH_LIFE_TIMER:
        cturn = 20 * (4 + random2(4)) / _mod_speed(10, mons->speed);
        break;
    case ENCH_INNER_FLAME:
        return random_range(25, 35) * 10;
    case ENCH_BERSERK:
        return (16 + random2avg(13, 2)) * 10;
    case ENCH_WRETCHED:
        cturn = (20 + roll_dice(3, 10)) * 10 / _mod_speed(10, mons->speed);
        break;
    case ENCH_TORNADO_COOLDOWN:
        cturn = random_range(25, 35) * 10 / _mod_speed(10, mons->speed);
        break;
    case ENCH_VORTEX_COOLDOWN:
        cturn = random_range(7, 17) * 10 / _mod_speed(10, mons->speed);
        break;
    case ENCH_FROZEN:
        cturn = 3 * BASELINE_DELAY;
        break;
    case ENCH_BRILLIANCE_AURA:
        cturn = 20 * BASELINE_DELAY;
        break;
    case ENCH_EMPOWERED_SPELLS:
        cturn = 2 * BASELINE_DELAY;
        break;
    case ENCH_GOZAG_INCITE:
        cturn = 100; // is never decremented
    default:
        break;
    }

    cturn = max(2, cturn);

    int raw_duration = (cturn * speed_to_duration(mons->speed));
    // Note: this fuzzing is _not_ symmetric, resulting in 90% of input
    // on the average.
    raw_duration = max(15, fuzz_value(raw_duration, 60, 40));

    dprf("cturn: %d, raw_duration: %d", cturn, raw_duration);

    return raw_duration;
}

// Calculate the effective duration (in terms of normal player time - 10
// duration units being one normal player action) of this enchantment.
void mon_enchant::set_duration(const monster* mons, const mon_enchant *added)
{
    if (duration && !added)
        return;

    if (added && added->duration)
        duration += added->duration;
    else
        duration += calc_duration(mons, added);

    if (duration > maxduration)
        maxduration = duration;
}
