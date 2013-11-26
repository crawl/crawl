/**
 * @file
 * @brief Monster enchantments.
**/

#include "AppHdr.h"

#include <sstream>

#include "act-iter.h"
#include "actor.h"
#include "areas.h"
#include "cloud.h"
#include "coordit.h"
#include "delay.h"
#include "dgn-shoals.h"
#include "env.h"
#include "fight.h"
#include "hints.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "religion.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-summoning.h"
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "view.h"
#include "xom.h"

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
        mon_enchant_list::const_iterator i =
            enchantments.find(static_cast<enchant_type>(e));

        if (i != enchantments.end())
            return i->second;
    }

    return mon_enchant();
}

void monster::update_ench(const mon_enchant &ench)
{
    if (ench.ench != ENCH_NONE)
    {
        mon_enchant_list::iterator i = enchantments.find(ench.ench);
        if (i != enchantments.end())
            i->second = ench;
    }
}

bool monster::add_ench(const mon_enchant &ench)
{
    // silliness
    if (ench.ench == ENCH_NONE)
        return false;

    if (ench.ench == ENCH_FEAR
        && (holiness() == MH_NONLIVING || berserk_or_insane()))
    {
        return false;
    }

    if (ench.ench == ENCH_FLIGHT && has_ench(ENCH_LIQUEFYING))
    {
        del_ench(ENCH_LIQUEFYING);
        invalidate_agrid();
    }

    mon_enchant_list::iterator i = enchantments.find(ench.ench);
    bool new_enchantment = false;
    mon_enchant *added = NULL;
    if (i == enchantments.end())
    {
        new_enchantment = true;
        added = &(enchantments[ench.ench] = ench);
        ench_cache.set(ench.ench, true);
    }
    else
    {
        i->second += ench;
        added = &i->second;
    }

    // If the duration is not set, we must calculate it (depending on the
    // enchantment).
    if (!ench.duration)
        added->set_duration(this, new_enchantment ? NULL : &ench);

    if (new_enchantment)
        add_enchantment_effect(ench);

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

        if (mons_is_lurking(this))
        {
            behaviour = BEH_WANDER;
            behaviour_event(this, ME_EVAL);
        }
        calc_speed();
        break;

    case ENCH_HASTE:
        calc_speed();
        break;

    case ENCH_SLOW:
        calc_speed();
        break;

    case ENCH_STONESKIN:
        {
            // player gets 2+earth/5
            const int ac_bonus = hit_dice / 2;

            ac += ac_bonus;
            // the monster may get drained or level up, we need to store the bonus
            props["stoneskin_ac"].get_byte() = ac_bonus;
        }
        break;

    case ENCH_OZOCUBUS_ARMOUR:
        {
            // player gets 4+ice/3
            const int ac_bonus = 4 + hit_dice / 3;

            ac += ac_bonus;
            // the monster may get drained or level up, we need to store the bonus
            props["ozocubus_ac"].get_byte() = ac_bonus;
        }
        break;

    case ENCH_SUBMERGED:
        mons_clear_trapping_net(this);

        // Don't worry about invisibility.  You should be able to see if
        // something has submerged.
        if (!quiet && mons_near(this))
        {
            if (type == MONS_AIR_ELEMENTAL)
            {
                mprf("%s merges itself into the air.",
                     name(DESC_A, true).c_str());
            }
            else if (type == MONS_TRAPDOOR_SPIDER)
            {
                mprf("%s hides itself under the floor.",
                     name(DESC_A, true).c_str());
            }
            else if (seen_context == SC_SURFACES)
            {
                // The monster surfaced and submerged in the same turn
                // without doing anything else.
                interrupt_activity(AI_SEE_MONSTER,
                                   activity_interrupt_data(this,
                                                           SC_SURFACES_BRIEFLY));
                // Why does this handle only land-capables?  I'd imagine this
                // to happen mostly (only?) for fish. -- 1KB
            }
            else if (crawl_state.game_is_arena())
                mprf("%s submerges.", name(DESC_A, true).c_str());
        }

        // Pacified monsters leave the level when they submerge.
        if (pacified())
            make_mons_leave_level(this);
        break;

    case ENCH_CONFUSION:
    case ENCH_MAD:
        if (type == MONS_TRAPDOOR_SPIDER && has_ench(ENCH_SUBMERGED))
            del_ench(ENCH_SUBMERGED);

        if (mons_is_lurking(this))
        {
            behaviour = BEH_WANDER;
            behaviour_event(this, ME_EVAL);
        }
        break;

    case ENCH_CHARM:
        behaviour = BEH_SEEK;
        target    = you.pos();
        foe       = MHITYOU;

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

        // TODO -- and friends

        if (you.can_see(this))
            learned_something_new(HINT_MONSTER_FRIENDLY, pos());
        break;

    case ENCH_LIQUEFYING:
    case ENCH_SILENCE:
        invalidate_agrid(true);
        break;

    case ENCH_ROLLING:
        calc_speed();
        break;

    default:
        break;
    }
}

static bool _prepare_del_ench(monster* mon, const mon_enchant &me)
{
    if (me.ench != ENCH_SUBMERGED)
        return true;

    // Unbreathing stuff that can't swim stays on the bottom.
    if (grd(mon->pos()) == DNGN_DEEP_WATER
        && !mon->can_drown()
        && !monster_habitable_grid(mon, DNGN_DEEP_WATER))
    {
        return false;
    }

    // Lurking monsters only unsubmerge when their foe is in sight if the foe
    // is right next to them.
    if (mons_is_lurking(mon))
    {
        const actor* foe = mon->get_foe();
        if (foe != NULL && mon->can_see(foe)
            && !adjacent(mon->pos(), foe->pos()))
        {
            return false;
        }
    }

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

    // Monster un-submerging while under player or another monster.  Try to
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
    // have unsubmerged.  Can it just stay submerged where it is?
    if (monster_can_submerge(mon, grd(mon->pos())))
        return false;

    // The terrain changed and the monster can't remain submerged.
    // Try to move to an adjacent square where it would be happy.
    for (adjacent_iterator ai(mon->pos()); ai; ++ai)
    {
        if (!actor_at(*ai)
            && monster_habitable_grid(mon, grd(*ai))
            && !find_trap(*ai))
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
    mon_enchant_list::iterator i = enchantments.find(ench);
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
            simple_monster_message(this, " is no longer moving quickly.");
        break;

    case ENCH_SWIFT:
        if (!quiet)
            if (type == MONS_ALLIGATOR)
                simple_monster_message(this, " slows down.");
            else
                simple_monster_message(this, " is no longer moving somewhat quickly.");
        break;

    case ENCH_SILENCE:
        invalidate_agrid();
        if (!quiet && !silenced(pos()))
            if (alive())
                simple_monster_message(this, " becomes audible again.");
            else
                mprf("As %s dies, the sound returns.", name(DESC_THE).c_str());
        break;

    case ENCH_MIGHT:
        if (!quiet)
            simple_monster_message(this, " no longer looks unusually strong.");
        break;

    case ENCH_SLOW:
        if (!quiet)
            simple_monster_message(this, " is no longer moving slowly.");
        calc_speed();
        break;

    case ENCH_STONESKIN:
        if (props.exists("stoneskin_ac"))
            ac -= props["stoneskin_ac"].get_byte();
        if (!quiet && you.can_see(this))
        {
            mprf("%s skin looks tender.",
                 apostrophise(name(DESC_THE)).c_str());
        }
        break;

    case ENCH_OZOCUBUS_ARMOUR:
        if (props.exists("ozocubus_ac"))
            ac -= props["ozocubus_ac"].get_byte();
        if (!quiet && you.can_see(this))
        {
            mprf("%s icy armour evaporates.",
                 apostrophise(name(DESC_THE)).c_str());
        }
        break;

    case ENCH_PARALYSIS:
        if (!quiet)
            simple_monster_message(this, " is no longer paralysed.");

        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_PETRIFIED:
        if (!quiet)
            simple_monster_message(this, " is no longer petrified.");
        del_ench(ENCH_PETRIFYING);

        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_PETRIFYING:
        fully_petrify(me.agent(), quiet);

        if (alive()) // losing active flight over lava
            behaviour_event(this, ME_EVAL);
        break;

    case ENCH_FEAR:
        if (holiness() == MH_NONLIVING || berserk_or_insane())
        {
            // This should only happen because of fleeing sanctuary
            snprintf(info, INFO_SIZE, " stops retreating.");
        }
        else if (!mons_is_tentacle_or_tentacle_segment(type))
        {
            snprintf(info, INFO_SIZE, " seems to regain %s courage.",
                     pronoun(PRONOUN_POSSESSIVE, true).c_str());
        }

        if (!quiet)
            simple_monster_message(this, info);

        // Reevaluate behaviour.
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_CONFUSION:
        if (!quiet)
            simple_monster_message(this, " seems less confused.");

        // Reevaluate behaviour.
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_INVIS:
        // Note: Invisible monsters are not forced to stay invisible, so
        // that they can properly have their invisibility removed just
        // before being polymorphed into a non-invisible monster.
        if (mons_near(this) && !you.can_see_invisible() && !backlit()
            && !has_ench(ENCH_SUBMERGED))
        {
            if (!quiet)
                mprf("%s appears from thin air!", name(DESC_A, true).c_str());

            autotoggle_autopickup(false);
            handle_seen_interrupt(this);
        }
        break;

    case ENCH_CHARM:
        if (!quiet)
            simple_monster_message(this, " is no longer charmed.");

        if (you.can_see(this))
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

        // Reevaluate behaviour.
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_CORONA:
    case ENCH_SILVER_CORONA:
    if (!quiet)
        {
            if (visible_to(&you))
                simple_monster_message(this, " stops glowing.");
            else if (has_ench(ENCH_INVIS) && mons_near(this))
            {
                mprf("%s stops glowing and disappears.",
                     name(DESC_THE, true).c_str());
            }
        }
        break;

    case ENCH_STICKY_FLAME:
        if (!quiet)
            simple_monster_message(this, " stops burning.");
        break;

    case ENCH_POISON:
        if (!quiet)
            simple_monster_message(this, " looks more healthy.");
        break;

    case ENCH_ROT:
        if (!quiet)
        {
            if (type == MONS_BOG_BODY)
                simple_monster_message(this, "'s decay slows.");
            else
                simple_monster_message(this, " is no longer rotting.");
        }
        break;

    case ENCH_HELD:
    {
        int net = get_trapping_net(pos());
        if (net != NON_ITEM)
        {
            remove_item_stationary(mitm[net]);

            if (!quiet)
                simple_monster_message(this, " breaks free.");
        }
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
            simple_monster_message(this, " is no longer berserk.");

        monster_die(this, (me.ench == ENCH_FAKE_ABJURATION) ? KILL_MISC :
                            (quiet) ? KILL_DISMISSED : KILL_RESET, NON_MONSTER);
        break;
    case ENCH_SHORT_LIVED:
        // Conjured ball lightnings explode when they time out.
        monster_die(this, KILL_TIMEOUT, NON_MONSTER);
        break;
    case ENCH_SUBMERGED:
        if (mons_is_wandering(this) || mons_is_lurking(this))
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

        if (you.can_see(this))
        {
            if (!mons_is_safe(this) && delay_is_run(current_delay_action()))
            {
                // Already set somewhere else.
                if (seen_context)
                    return;

                if (!monster_habitable_grid(this, DNGN_FLOOR))
                    seen_context = SC_FISH_SURFACES;
                else
                    seen_context = SC_SURFACES;
            }
            else if (!quiet)
            {
                msg_channel_type channel = MSGCH_PLAIN;
                if (!seen_context)
                {
                    channel = MSGCH_WARN;
                    seen_context = SC_JUST_SEEN;
                }

                if (type == MONS_AIR_ELEMENTAL)
                {
                    mprf(channel, "%s forms itself from the air!",
                                  name(DESC_A, true).c_str());
                }
                else if (type == MONS_TRAPDOOR_SPIDER)
                {
                    mprf(channel,
                         "%s leaps out from its hiding place under the floor!",
                         name(DESC_A, true).c_str());
                }
                else if (type == MONS_LOST_SOUL)
                {
                    mprf(channel, "%s flickers into view.",
                                  name(DESC_A).c_str());
                }
                else if (crawl_state.game_is_arena())
                    mprf("%s surfaces.", name(DESC_A, true).c_str());
            }
        }
        else if (mons_near(this)
                 && feat_compatible(grd(pos()), DNGN_DEEP_WATER))
        {
            mpr("Something invisible bursts forth from the water.");
            interrupt_activity(AI_FORCE_INTERRUPT);
        }
        break;

    case ENCH_SOUL_RIPE:
        if (!quiet)
        {
            simple_monster_message(this,
                                   "'s soul is no longer ripe for the taking.");
        }
        break;

    case ENCH_AWAKEN_FOREST:
        env.forest_awoken_until = 0;
        if (!quiet)
            forest_message(pos(), "The forest calms down.");
        break;

    case ENCH_BLEED:
        if (!quiet)
            simple_monster_message(this, " is no longer bleeding.");
        break;

    case ENCH_WITHDRAWN:
        if (!quiet)
            simple_monster_message(this, " emerges from its shell.");
        break;

    case ENCH_LIQUEFYING:
        invalidate_agrid();

        if (!quiet)
            simple_monster_message(this, " is no longer liquefying the ground.");
        break;

    case ENCH_FLIGHT:
        apply_location_effects(pos(), me.killer(), me.kill_agent());
        break;

    case ENCH_DAZED:
        if (!quiet && alive())
                simple_monster_message(this, " is no longer dazed.");
        break;

    case ENCH_INNER_FLAME:
        if (!quiet && alive())
            simple_monster_message(this, "'s inner flame fades away.");
        break;

    case ENCH_ROLLING:
        calc_speed();
        if (!quiet && alive())
            simple_monster_message(this, " stops rolling.");
        break;

    //The following should never happen, but just in case...

    case ENCH_MUTE:
        if (!quiet && alive())
                simple_monster_message(this, " is no longer mute.");
        break;

    case ENCH_BLIND:
        if (!quiet && alive())
            simple_monster_message(this, " is no longer blind.");

        // Reevaluate behaviour.
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_DUMB:
        if (!quiet && alive())
            simple_monster_message(this, " is no longer stupefied.");

        // Reevaluate behaviour.
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_MAD:
        if (!quiet && alive())
            simple_monster_message(this, " is no longer mad.");

        // Reevaluate behaviour.
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_DEATHS_DOOR:
        if (!quiet)
            simple_monster_message(this, " is no longer invulnerable.");
        break;

    case ENCH_REGENERATION:
        if (!quiet)
            simple_monster_message(this, " is no longer regenerating.");
        break;

    case ENCH_WRETCHED:
        if (!quiet)
        {
            snprintf(info, INFO_SIZE, " seems to return to %s normal shape.",
                     pronoun(PRONOUN_POSSESSIVE, true).c_str());
            simple_monster_message(this, info);
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
            simple_monster_message(this, " is no longer weakened.");
        break;

    case ENCH_AWAKEN_VINES:
        unawaken_vines(this, quiet);
        break;

    case ENCH_CONTROL_WINDS:
        if (!quiet && you.can_see(this))
            mprf("The winds cease moving at %s will.", name(DESC_ITS).c_str());
        break;

    case ENCH_TOXIC_RADIANCE:
        if (!quiet && you.can_see(this))
            mprf("%s toxic aura wanes.", name(DESC_ITS).c_str());
        break;

    case ENCH_GRASPING_ROOTS_SOURCE:
        if (!quiet && you.see_cell(pos()))
            mpr("The grasping roots settle back into the ground.");

        // Done here to avoid duplicate messages
        if (you.duration[DUR_GRASPING_ROOTS])
            check_grasping_roots(&you, true);

        break;

    case ENCH_FIRE_VULN:
        if (!quiet)
            simple_monster_message(this, " is no longer more vulnerable to fire.");
        break;

    default:
        break;
    }
}

bool monster::lose_ench_levels(const mon_enchant &e, int lev)
{
    if (!lev)
        return false;

    if (e.duration >= INFINITE_DURATION)
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

//---------------------------------------------------------------
//
// timeout_enchantments
//
// Update a monster's enchantments when the player returns
// to the level.
//
// Management for enchantments... problems with this are the oddities
// (monster dying from poison several thousands of turns later), and
// game balance.
//
// Consider: Poison/Sticky Flame a monster at range and leave, monster
// dies but can't leave level to get to player (implied game balance of
// the delayed damage is that the monster could be a danger before
// it dies).  This could be fixed by keeping some monsters active
// off level and allowing them to take stairs (a very serious change).
//
// Compare this to the current abuse where the player gets
// effectively extended duration of these effects (although only
// the actual effects only occur on level, the player can leave
// and heal up without having the effect disappear).
//
// This is a simple compromise between the two... the enchantments
// go away, but the effects don't happen off level.  -- bwr
//
//---------------------------------------------------------------
void monster::timeout_enchantments(int levels)
{
    if (enchantments.empty())
        return;

    const mon_enchant_list ec = enchantments;
    for (mon_enchant_list::const_iterator i = ec.begin();
         i != ec.end(); ++i)
    {
        switch (i->first)
        {
        case ENCH_WITHDRAWN:
            if (hit_points >= (max_hit_points - max_hit_points / 4)
                && !one_chance_in(3))
            {
                del_ench(i->first);
                break;
            }
            // Deliberate fall-through

        case ENCH_POISON: case ENCH_ROT: case ENCH_CORONA:
        case ENCH_STICKY_FLAME: case ENCH_ABJ: case ENCH_SHORT_LIVED:
        case ENCH_SLOW: case ENCH_HASTE: case ENCH_MIGHT: case ENCH_FEAR:
        case ENCH_CHARM: case ENCH_SLEEP_WARY: case ENCH_SICK:
        case ENCH_PARALYSIS: case ENCH_PETRIFYING:
        case ENCH_PETRIFIED: case ENCH_SWIFT: case ENCH_BATTLE_FRENZY:
        case ENCH_SILENCE: case ENCH_LOWERED_MR:
        case ENCH_SOUL_RIPE: case ENCH_BLEED: case ENCH_ANTIMAGIC:
        case ENCH_FEAR_INSPIRING: case ENCH_REGENERATION: case ENCH_RAISED_MR:
        case ENCH_MIRROR_DAMAGE: case ENCH_STONESKIN: case ENCH_LIQUEFYING:
        case ENCH_SILVER_CORONA: case ENCH_DAZED: case ENCH_FAKE_ABJURATION:
        case ENCH_ROUSED: case ENCH_BREATH_WEAPON: case ENCH_DEATHS_DOOR:
        case ENCH_OZOCUBUS_ARMOUR: case ENCH_WRETCHED: case ENCH_SCREAMED:
        case ENCH_BLIND: case ENCH_WORD_OF_RECALL: case ENCH_INJURY_BOND:
        case ENCH_FLAYED:
            lose_ench_levels(i->second, levels);
            break;

        case ENCH_INVIS:
            if (!mons_class_flag(type, M_INVIS))
                lose_ench_levels(i->second, levels);
            break;

        case ENCH_INSANE:
        case ENCH_BERSERK:
        case ENCH_INNER_FLAME:
        case ENCH_ROLLING:
            del_ench(i->first);
            break;

        case ENCH_FATIGUE:
            del_ench(i->first);
            del_ench(ENCH_SLOW);
            break;

        case ENCH_TP:
            teleport(true);
            del_ench(i->first);
            break;

        case ENCH_CONFUSION:
            if (!mons_class_flag(type, M_CONFUSED))
                del_ench(i->first);
            if (!is_stationary())
                monster_blink(this, true);
            break;

        case ENCH_HELD:
            del_ench(i->first);
            break;

        case ENCH_TIDE:
        {
            const int actdur = speed_to_duration(speed) * levels;
            lose_ench_duration(i->first, actdur);
            break;
        }

        case ENCH_SLOWLY_DYING:
        {
            const int actdur = speed_to_duration(speed) * levels;
            if (lose_ench_duration(i->first, actdur))
                monster_die(this, KILL_MISC, NON_MONSTER, true);
            break;
        }

        case ENCH_PREPARING_RESURRECT:
        {
            const int actdur = speed_to_duration(speed) * levels;
            if (lose_ench_duration(i->first, actdur))
                shedu_do_actual_resurrection(this);
            break;
        }

        default:
            break;
        }

        if (!alive())
            break;
    }
}

string monster::describe_enchantments() const
{
    ostringstream oss;
    for (mon_enchant_list::const_iterator i = enchantments.begin();
         i != enchantments.end(); ++i)
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

    // Faster monsters can wiggle out of the net more quickly.
    const int spd = (me.ench == ENCH_HELD) ? speed :
                                             10;
    const int actdur = speed_to_duration(spd);
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

void monster::apply_enchantment(const mon_enchant &me)
{
    enchant_type en = me.ench;
    const int spd = 10;
    switch (me.ench)
    {
    case ENCH_INSANE:
        if (decay_enchantment(en))
        {
            simple_monster_message(this, " is no longer in an insane frenzy.");
            const int duration = random_range(70, 130);
            add_ench(mon_enchant(ENCH_FATIGUE, 0, 0, duration));
            add_ench(mon_enchant(ENCH_SLOW, 0, 0, duration));
        }
        break;

    case ENCH_BERSERK:
        if (decay_enchantment(en))
        {
            simple_monster_message(this, " is no longer berserk.");
            const int duration = random_range(70, 130);
            add_ench(mon_enchant(ENCH_FATIGUE, 0, 0, duration));
            add_ench(mon_enchant(ENCH_SLOW, 0, 0, duration));
        }
        break;

    case ENCH_FATIGUE:
        if (decay_enchantment(en))
        {
            simple_monster_message(this, " looks more energetic.");
            del_ench(ENCH_SLOW, true);
        }
        break;

    case ENCH_WITHDRAWN:
        if (hit_points >= (max_hit_points - max_hit_points / 4)
                && !one_chance_in(3))
        {
            del_ench(ENCH_WITHDRAWN);
            break;
        }

        // Deliberate fall through.

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
    case ENCH_ANTIMAGIC:
    case ENCH_REGENERATION:
    case ENCH_RAISED_MR:
    case ENCH_STONESKIN:
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
    case ENCH_DEATHS_DOOR:
    case ENCH_OZOCUBUS_ARMOUR:
    case ENCH_WRETCHED:
    case ENCH_SCREAMED:
    case ENCH_WEAK:
    case ENCH_AWAKEN_VINES:
    case ENCH_WIND_AIDED:
    case ENCH_FIRE_VULN:
    // case ENCH_ROLLING:
        decay_enchantment(en);
        break;

    case ENCH_MIRROR_DAMAGE:
        if (decay_enchantment(en))
            simple_monster_message(this, "'s dark mirror aura disappears.");
        break;

    case ENCH_SILENCE:
    case ENCH_LIQUEFYING:
        decay_enchantment(en);
        invalidate_agrid();
        break;

    case ENCH_BATTLE_FRENZY:
    case ENCH_ROUSED:
        decay_enchantment(en, false);
        break;

    case ENCH_AQUATIC_LAND:
        // Aquatic monsters lose hit points every turn they spend on dry land.
        ASSERT(mons_habitat(this) == HT_WATER);
        if (feat_is_watery(grd(pos())))
        {
            // The tide, water card or Fedhas gave us water.
            del_ench(ENCH_AQUATIC_LAND);
            break;
        }

        // Zombies don't take damage from flopping about on land.
        if (mons_is_zombified(this))
            break;

        hurt(me.agent(), 1 + random2(5), BEAM_NONE);
        break;

    case ENCH_HELD:
    {
        if (is_stationary() || cannot_act() || asleep())
            break;

        int net = get_trapping_net(pos(), true);

        if (net == NON_ITEM)
        {
            trap_def *trap = find_trap(pos());
            if (trap && trap->type == TRAP_WEB)
            {
                if (coinflip())
                {
                    if (mons_near(this) && !visible_to(&you))
                        mpr("Something you can't see is thrashing in a web.");
                    else
                    {
                        simple_monster_message(this,
                            " struggles to get unstuck from the web.");
                    }
                    break;
                }
                maybe_destroy_web(this);
            }
            del_ench(ENCH_HELD);
            break;
        }

        // Handled in handle_pickup().
        if (mons_eats_items(this))
            break;

        // The enchantment doubles as the durability of a net
        // the more corroded it gets, the more easily it will break.
        const int hold = mitm[net].plus; // This will usually be negative.
        const int mon_size = body_size(PSIZE_BODY);

        // Smaller monsters can escape more quickly.
        if (mon_size < random2(SIZE_BIG)  // BIG = 5
            && !berserk_or_insane() && type != MONS_DANCING_WEAPON)
        {
            if (mons_near(this) && !visible_to(&you))
                mpr("Something wriggles in the net.");
            else
                simple_monster_message(this, " struggles to escape the net.");

            // Confused monsters have trouble finding the exit.
            if (has_ench(ENCH_CONFUSION) && !one_chance_in(5))
                break;

            decay_enchantment(en, 2*(NUM_SIZE_LEVELS - mon_size) - hold);

            // Frayed nets are easier to escape.
            if (mon_size <= -(hold-1)/2)
                decay_enchantment(en, (NUM_SIZE_LEVELS - mon_size));
        }
        else // Large (and above) monsters always thrash the net and destroy it
        {    // e.g. ogre, large zombie (large); centaur, naga, hydra (big).

            if (mons_near(this) && !visible_to(&you))
                mpr("Something wriggles in the net.");
            else
                simple_monster_message(this, " struggles against the net.");

            // Confused monsters more likely to struggle without result.
            if (has_ench(ENCH_CONFUSION) && one_chance_in(3))
                break;

            // Nets get destroyed more quickly for larger monsters
            // and if already strongly frayed.
            int damage = 0;

            // tiny: 1/6, little: 2/5, small: 3/4, medium and above: always
            if (x_chance_in_y(mon_size + 1, SIZE_GIANT - mon_size))
                damage++;

            // Handled specially to make up for its small size.
            if (type == MONS_DANCING_WEAPON)
            {
                damage += one_chance_in(3);

                if (can_cut_meat(mitm[inv[MSLOT_WEAPON]]))
                    damage++;
            }


            // Extra damage for large (50%) and big (always).
            if (mon_size == SIZE_BIG || mon_size == SIZE_LARGE && coinflip())
                damage++;

            // overall damage per struggle:
            // tiny   -> 1/6
            // little -> 2/5
            // small  -> 3/4
            // medium -> 1
            // large  -> 1,5
            // big    -> 2

            // extra damage if already damaged
            if (random2(body_size(PSIZE_BODY) - hold + 1) >= 4)
                damage++;

            // Berserking doubles damage dealt.
            if (berserk())
                damage *= 2;

            // Faster monsters can damage the net more often per
            // time period.
            if (speed != 0)
                damage = div_rand_round(damage * speed, spd);

            mitm[net].plus -= damage;

            if (mitm[net].plus < -7)
            {
                if (mons_near(this))
                {
                    if (visible_to(&you))
                    {
                        mprf("The net rips apart, and %s comes free!",
                             name(DESC_THE).c_str());
                    }
                    else
                        mpr("All of a sudden the net rips apart!");
                }
                destroy_item(net);

                del_ench(ENCH_HELD, true);
            }
        }
        break;
    }
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

        // Air elementals are a special case, as their submerging in air
        // isn't up to choice. - bwr
        if (type == MONS_AIR_ELEMENTAL)
        {
            heal(1, one_chance_in(5));

            if (one_chance_in(5))
                del_ench(ENCH_SUBMERGED);

            break;
        }

        // Now we handle the others:
        const dungeon_feature_type grid = grd(pos());

        if (!monster_can_submerge(this, grid))
        {
            // unbreathing stuff can stay on the bottom
            if (grid != DNGN_DEEP_WATER
                || monster_habitable_grid(this, grid)
                || can_drown())
            {
                del_ench(ENCH_SUBMERGED); // forced to surface
            }
        }
        else if (mons_landlubbers_in_reach(this))
        {
            del_ench(ENCH_SUBMERGED);
            make_mons_stop_fleeing(this);
        }
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
#ifdef DEBUG_DIAGNOSTICS
            // For debugging, we don't have this silent.
            simple_monster_message(this, " takes poison damage.",
                                   MSGCH_DIAGNOSTICS);
            mprf(MSGCH_DIAGNOSTICS, "poison damage: %d", dam);
#endif

            hurt(me.agent(), dam, BEAM_POISON);
        }

        decay_enchantment(en, true);
        break;
    }
    case ENCH_ROT:
    {
        if (hit_points > 1 && one_chance_in(3))
        {
            hurt(me.agent(), 1);
            if (hit_points < max_hit_points && coinflip())
                --max_hit_points;
        }

        decay_enchantment(en, true);
        break;
    }

    // Assumption: monster::res_fire has already been checked.
    case ENCH_STICKY_FLAME:
    {
        if (feat_is_watery(grd(pos())) && (ground_level()
              || mons_intel(this) >= I_NORMAL && flight_mode()))
        {
            if (mons_near(this) && visible_to(&you))
            {
                mprf(ground_level() ? "The flames covering %s go out."
                     : "%s dips into the water, and the flames go out.",
                     name(DESC_THE, false).c_str());
            }
            del_ench(ENCH_STICKY_FLAME);
            break;
        }
        int dam = resist_adjust_damage(this, BEAM_FIRE, res_fire(),
                                       roll_dice(2, 4) - 1);

        if (dam > 0)
        {
            simple_monster_message(this, " burns!");
            dprf("sticky flame damage: %d", dam);

            if (type == MONS_SHEEP)
            {
                for (adjacent_iterator ai(pos()); ai; ++ai)
                {
                    monster *mon = monster_at(*ai);
                    if (mon && mon->type == MONS_SHEEP
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

            hurt(me.agent(), dam, BEAM_NAPALM);
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
            if (you.can_see(this))
            {
                if (type == MONS_PILLAR_OF_SALT)
                    mprf("%s crumbles away.", name(DESC_THE, false).c_str());
                else
                {
                    mprf("A nearby %s withers and dies.",
                         name(DESC_PLAIN, false).c_str());
                }
            }

            monster_die(this, KILL_MISC, NON_MONSTER, true);
        }
        break;

    case ENCH_PREPARING_RESURRECT:
        if (decay_enchantment(en))
            shedu_do_actual_resurrection(this);
        break;

    case ENCH_SPORE_PRODUCTION:
        // Reduce the timer, if that means we lose the enchantment then
        // spawn a spore and re-add the enchantment.
        if (decay_enchantment(en))
        {
            // Search for an open adjacent square to place a spore on
            int idx[] = {0, 1, 2, 3, 4, 5, 6, 7};
            shuffle_array(idx, 8);

            bool re_add = true;

            for (unsigned i = 0; i < 8; ++i)
            {
                coord_def adjacent = pos() + Compass[idx[i]];

                if (mons_class_can_pass(MONS_GIANT_SPORE, env.grid(adjacent))
                                        && !actor_at(adjacent))
                {
                    beh_type plant_attitude = SAME_ATTITUDE(this);

                    if (monster *plant = create_monster(mgen_data(MONS_GIANT_SPORE,
                                                            plant_attitude,
                                                            NULL,
                                                            0,
                                                            0,
                                                            adjacent,
                                                            MHITNOT,
                                                            MG_FORCE_PLACE)))
                    {
                        if (mons_is_god_gift(this, GOD_FEDHAS))
                        {
                            plant->flags |= MF_NO_REWARD;

                            if (plant_attitude == BEH_FRIENDLY)
                            {
                                plant->flags |= MF_ATT_CHANGE_ATTEMPT;

                                mons_make_god_gift(plant, GOD_FEDHAS);
                            }
                        }

                        plant->behaviour = BEH_WANDER;
                        plant->number = 20;

                        if (you.see_cell(adjacent) && you.see_cell(pos()))
                            mpr("A ballistomycete spawns a giant spore.");

                        // Decrease the count and maybe become inactive
                        // again.
                        if (number)
                        {
                            number--;
                            if (number == 0)
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
                if (you.can_see(this))
                    simple_monster_message(this, " suddenly becomes enraged!");
                else
                    mpr("You hear a distant and violent thrashing sound.");
            }

            attitude = ATT_HOSTILE;
            behaviour_event(this, ME_ALERT, &you);
        }
    }
    break;


    case ENCH_SEVERED:
    {
        simple_monster_message(this, " writhes!");
        coord_def base_position = props["base_position"].get_coord();
        maybe_bloodify_square(base_position);
        hurt(me.agent(), 20);
    }

    break;

    case ENCH_GLOWING_SHAPESHIFTER: // This ench never runs out!
        // Number of actions is fine for shapeshifters.  Don't change
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
                || x_chance_in_y(1000 / (15 * max(1, hit_dice) / 5), 1000)))
        {
            monster_polymorph(this, RANDOM_MONSTER);
        }
        break;

    case ENCH_TP:
        if (decay_enchantment(en, true) && !no_tele(true, false))
            monster_teleport(this, true);
        break;

    case ENCH_EAT_ITEMS:
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
            if (you.can_see(this))
            {
                mprf("The winds around %s start to calm down.",
                     name(DESC_THE).c_str());
            }
        }
        break;

    case ENCH_BLEED:
    {
        // 3, 6, 9% of current hp
        int dam = div_rand_round(random2((1 + hit_points)*(me.degree * 3)),100);

        // location, montype, damage (1 hp = 5% chance), spatter, smell_alert
        bleed_onto_floor(pos(), type, 20, false, true);

        if (dam < hit_points)
        {
            hurt(me.agent(), dam);

            dprf("hit_points: %d ; bleed damage: %d ; degree: %d",
                 hit_points, dam, me.degree);
        }

        decay_enchantment(en, true);
        break;
    }

    // This is like Corona, but if silver harms them, it has sticky
    // flame levels of damage.
    case ENCH_SILVER_CORONA:
        if (is_chaotic())
        {
            bolt beam;
            beam.flavour = BEAM_LIGHT;
            int dam = roll_dice(2, 4) - 1;

            int newdam = mons_adjust_flavoured(this, beam, dam, false);

            if (newdam > 0)
            {
                string msg = mons_has_flesh(this) ? "'s flesh" : "";
                msg += (dam < newdam) ? " is horribly charred!"
                                      : " is seared.";
                simple_monster_message(this, msg.c_str());
            }

            dprf("Zin's Corona damage: %d", dam);
            hurt(me.agent(), dam, BEAM_LIGHT);
        }

        decay_enchantment(en, true);
        break;

    case ENCH_WORD_OF_RECALL:
        // If we've gotten silenced or somehow incapacitated since we started,
        // cancel the recitation
        if (silenced(pos()) || paralysed() || petrified()
            || confused() || asleep() || has_ench(ENCH_FEAR)
            || has_ench(ENCH_BREATH_WEAPON))
        {
            this->speed_increment += me.duration;
            del_ench(ENCH_WORD_OF_RECALL, true, false);
            if (you.can_see(this))
            {
                mprf("%s word of recall is interrupted.",
                     name(DESC_ITS).c_str());
            }
            break;
        }

        if (decay_enchantment(en))
        {
            mons_word_of_recall(this, 3 + random2(5));
            // This is the same delay as vault sentinels.
            mon_enchant breath_timeout =
                mon_enchant(ENCH_BREATH_WEAPON, 1, this,
                            (4 + random2(9)) * BASELINE_DELAY);
            add_ench(breath_timeout);
        }
        break;

    case ENCH_INJURY_BOND:
        // It's hard to absorb someone else's injuries when you're dead
        if (!me.agent() || !me.agent()->alive())
            del_ench(ENCH_INJURY_BOND, true, false);
        else
            decay_enchantment(en);
        break;

    case ENCH_WATER_HOLD:
        if (!me.agent()
            || (me.agent() && !adjacent(me.agent()->as_monster()->pos(), pos())))
        {
            del_ench(ENCH_WATER_HOLD);
        }
        else
        {
            if (!res_water_drowning())
            {
                lose_ench_duration(me, -speed_to_duration(speed));
                int dam = div_rand_round((50 + stepdown((float)me.duration, 30.0))
                                          * speed_to_duration(speed),
                            BASELINE_DELAY * 10);
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

    case ENCH_CONTROL_WINDS:
        apply_control_winds(this);
        decay_enchantment(en);
        break;

    case ENCH_TOXIC_RADIANCE:
        toxic_radiance_effect(this, 1);
        decay_enchantment(en);
        break;

    case ENCH_GRASPING_ROOTS_SOURCE:
        if (!apply_grasping_roots(this))
            decay_enchantment(en);
        break;

    case ENCH_GRASPING_ROOTS:
        check_grasping_roots(this);
        break;

    case ENCH_TORNADO_COOLDOWN:
       if (decay_enchantment(en))
        {
            remove_tornado_clouds(mindex());
            if (you.can_see(this))
                mprf("The winds around %s calm down.", name(DESC_THE).c_str());
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
    {
        for (int i = 0; i < NUM_MONSTER_SLOTS; ++i)
        {
            const int item = inv[i];
            if (item != NON_ITEM)
                mitm[item].flags |= ISFLAG_SUMMONED;
        }
    }
}

bool monster::is_summoned(int* duration, int* summon_type) const
{
    const mon_enchant abj = get_ench(ENCH_ABJ);
    if (abj.ench == ENCH_NONE)
    {
        if (duration != NULL)
            *duration = -1;
        if (summon_type != NULL)
            *summon_type = 0;

        return false;
    }
    if (duration != NULL)
        *duration = abj.duration;

    const mon_enchant summ = get_ench(ENCH_SUMMON);
    if (summ.ench == ENCH_NONE)
    {
        if (summon_type != NULL)
            *summon_type = 0;

        return true;
    }
    if (summon_type != NULL)
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
        speed = 10;
    const int modded = val * 10 / speed;
    return modded? modded : 1;
}

/////////////////////////////////////////////////////////////////////////
// mon_enchant

static const char *enchant_names[] =
{
    "none", "berserk", "haste", "might", "fatigue", "slow", "fear",
    "confusion", "invis", "poison", "rot", "summon", "abj", "corona",
    "charm", "sticky_flame", "glowing_shapeshifter", "shapeshifter", "tp",
    "sleep_wary", "submerged", "short_lived", "paralysis", "sick",
    "sleepy", "held", "battle_frenzy",
#if TAG_MAJOR_VERSION == 34
    "temp_pacif",
#endif
    "petrifying",
    "petrified", "lowered_mr", "soul_ripe", "slowly_dying", "eat_items",
    "aquatic_land", "spore_production",
#if TAG_MAJOR_VERSION == 34
    "slouch",
#endif
    "swift", "tide",
    "insane", "silenced", "awaken_forest", "exploding", "bleeding",
    "tethered", "severed", "antimagic",
#if TAG_MAJOR_VERSION == 34
    "fading_away",
#endif
    "preparing_resurrect", "regen",
    "magic_res", "mirror_dam", "stoneskin", "fear inspiring", "temporarily pacified",
    "withdrawn", "attached", "guardian_timer", "flight",
    "liquefying", "tornado", "fake_abjuration",
    "dazed", "mute", "blind", "dumb", "mad", "silver_corona", "recite timer",
    "inner_flame", "roused", "breath timer", "deaths_door", "rolling",
    "ozocubus_armour", "wretched", "screamed", "rune_of_recall", "injury bond",
    "drowning", "flayed", "haunting", "retching", "weak", "dimension_anchor",
    "awaken vines", "control_winds", "wind_aided", "summon_capped",
    "toxic_radiance", "grasping_roots_source", "grasping_roots",
    "iood_charged", "fire_vuln", "tornado_cooldown", "buggy",
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
    // Sickness is not capped.
    if (ench == ENCH_SICK)
        return;

    // Hard cap to simulate old enum behaviour, we should really throw this
    // out entirely.
    const int max = (ench == ENCH_ABJ || ench == ENCH_FAKE_ABJURATION) ? 6 : 4;
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
    return _mod_speed(mons->hit_dice + hdplus, mons->speed);
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
    case ENCH_WITHDRAWN:
        cturn = 5000 / _mod_speed(25, mons->speed);
        break;

    case ENCH_SWIFT:
        cturn = 1000 / _mod_speed(25, mons->speed);
        break;
    case ENCH_HASTE:
    case ENCH_MIGHT:
    case ENCH_INVIS:
    case ENCH_FEAR_INSPIRING:
    case ENCH_STONESKIN:
    case ENCH_OZOCUBUS_ARMOUR:
        cturn = 1000 / _mod_speed(25, mons->speed);
        break;
    case ENCH_LIQUEFYING:
    case ENCH_SILENCE:
    case ENCH_REGENERATION:
    case ENCH_RAISED_MR:
    case ENCH_MIRROR_DAMAGE:
    case ENCH_DEATHS_DOOR:
        cturn = 300 / _mod_speed(25, mons->speed);
        break;
    case ENCH_SLOW:
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
    case ENCH_ROT:
        if (deg > 1)
            cturn = 1000 * (deg - 1) / _mod_speed(333, mons->speed);
        cturn += 1000 / _mod_speed(250, mons->speed);
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
        // the monster will create a giant spore.
        return random_range(475, 525) * 10;

    case ENCH_PREPARING_RESURRECT:
        // A timer. When it runs out, the creature will cast resurrect.
        return random_range(4, 7) * 10;

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
        cturn = 500 / modded_speed(mons, 10);
        break;
    case ENCH_TP:
        cturn = 1000 * deg / _mod_speed(1000, mons->speed);
        break;
    case ENCH_SLEEP_WARY:
        cturn = 1000 / _mod_speed(50, mons->speed);
        break;
    case ENCH_LIFE_TIMER:
        cturn = 10 * (4 + random2(4)) / _mod_speed(10, mons->speed);
        break;
    case ENCH_INNER_FLAME:
        return random_range(25, 35) * 10;
    case ENCH_BERSERK:
        return (16 + random2avg(13, 2)) * 10;
    case ENCH_ROLLING:
        cturn = 10000 / _mod_speed(25, mons->speed);
        break;
    case ENCH_WRETCHED:
        cturn = (20 + roll_dice(3, 10)) * 10 / _mod_speed(10, mons->speed);
        break;
    case ENCH_TORNADO_COOLDOWN:
        cturn = random_range(25, 35) * 10 / _mod_speed(10, mons->speed);
        break;
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
