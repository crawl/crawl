/**
 * @file
 * @brief Monster enchantments.
**/

#include "AppHdr.h"

#include "monster.h"

#include <sstream>
#include <unordered_map>

#include "act-iter.h"
#include "areas.h"
#include "attitude-change.h"
#include "bloodspatter.h"
#include "cloud.h"
#include "coordit.h"
#include "corpse.h"
#include "delay.h"
#include "dgn-shoals.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "hints.h"
#include "god-abil.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "message.h"
#include "mon-abil.h"
#include "mon-aura.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-explode.h"
#include "mon-gear.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "religion.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-monench.h"
#include "spl-summoning.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "tag-version.h"
#include "teleport.h"
#include "terrain.h"
#include "timed-effects.h"
#include "traps.h"
#include "unwind.h"
#include "view.h"

static const unordered_map<enchant_type, cloud_type, std::hash<int>> _cloud_ring_ench_to_cloud = {
    { ENCH_RING_OF_THUNDER,     CLOUD_STORM },
    { ENCH_RING_OF_FLAMES,      CLOUD_FIRE },
    { ENCH_RING_OF_CHAOS,       CLOUD_CHAOS },
    { ENCH_RING_OF_MUTATION,    CLOUD_MUTAGENIC },
    { ENCH_RING_OF_FOG,         CLOUD_GREY_SMOKE },
    { ENCH_RING_OF_ICE,         CLOUD_COLD },
    { ENCH_RING_OF_MISERY,      CLOUD_MISERY },
    { ENCH_RING_OF_ACID,        CLOUD_ACID },
    { ENCH_RING_OF_MIASMA,      CLOUD_MIASMA },
};

static bool _has_other_cloud_ring(monster* mons, enchant_type ench)
{
    for (auto i : _cloud_ring_ench_to_cloud)
    {
        if (mons->has_ench(i.first) && i.first != ench)
            return true;
    }
    return false;
}

/**
 * If the monster has a cloud ring enchantment, surround them with clouds.
 */
void update_mons_cloud_ring(monster* mons)
{
    for (auto i : _cloud_ring_ench_to_cloud)
    {
        if (mons->has_ench(i.first))
        {
            surround_actor_with_cloud(mons, i.second);
            break; // there can only be one cloud ring
        }
    }
}

#ifdef DEBUG_ENCH_CACHE_DIAGNOSTICS
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

    if (ench.ench == ENCH_FEAR && !can_feel_fear(true))
        return false;

    if (ench.ench == ENCH_BLIND && !mons_can_be_blinded(type))
        return false;

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
        remove_summons();
    }

    if (ench.ench == ENCH_PARALYSIS)
        stop_directly_constricting_all();

    return true;
}

void monster::add_enchantment_effect(const mon_enchant &ench, bool quiet)
{
    // Check for slow/haste.
    switch (ench.ench)
    {
    case ENCH_BERSERK:
        scale_hp(3, 2);
        calc_speed();
        break;

    case ENCH_DOUBLED_HEALTH:
        scale_hp(2, 1);
        break;

    case ENCH_FRENZIED:
        calc_speed();
        break;

    case ENCH_HASTE:
        calc_speed();
        break;

    case ENCH_SLOW:
        calc_speed();
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
                target = env.mons[source_actor->as_monster()->foe].pos();
        }

        if (type == MONS_FLAYED_GHOST)
        {
            // temporarily change our attitude back (XXX: scary code...)
            unwind_var<mon_enchant_list> enchants(enchantments, mon_enchant_list{});
            unwind_var<FixedBitVector<NUM_ENCHANTMENTS>> ecache(ench_cache, {});
            end_flayed_effect(this);
        }
        del_ench(ENCH_STILL_WINDS);
        del_ench(ENCH_BULLSEYE_TARGET);

        if (is_patrolling())
        {
            // Charmed monsters stop patrolling and forget their patrol
            // point; they're supposed to follow you now.
            patrol_point.reset();
            firing_pos.reset();
        }
        mons_att_changed(this);
        // clear any constrictions on/by you
        stop_constricting(MID_PLAYER, true);
        you.stop_constricting(mid, true);

        if (invisible() && you.see_cell(pos()) && !you.can_see_invisible()
            && !backlit())
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
    case ENCH_RING_OF_FLAMES:
    case ENCH_RING_OF_CHAOS:
    case ENCH_RING_OF_MUTATION:
    case ENCH_RING_OF_FOG:
    case ENCH_RING_OF_ICE:
    case ENCH_RING_OF_MISERY:
    case ENCH_RING_OF_ACID:
    case ENCH_RING_OF_MIASMA:
        if (_has_other_cloud_ring(this, ench.ench))
            die("%s already has a cloud ring!", name(DESC_THE).c_str());
        surround_actor_with_cloud(this, _cloud_ring_ench_to_cloud.at(ench.ench));
        break;

    case ENCH_VILE_CLUTCH:
    case ENCH_GRASPING_ROOTS:
    {
        actor *source_actor = actor_by_mid(ench.source, true);
        const string noun = ench.ench == ENCH_VILE_CLUTCH ? "Zombie hands" :
                                                            "Roots";
        source_actor->start_constricting(*this);
        if (you.see_cell(pos()))
        {
            mprf(MSGCH_WARN, "%s grab %s.", noun.c_str(),
                 name(DESC_THE).c_str());
        }
        break;
    }

    case ENCH_TOUCH_OF_BEOGH:
        scale_hp(touch_of_beogh_hp_mult(*this), 100);
        break;

    default:
        break;
    }
}


bool monster::del_ench(enchant_type ench, bool quiet, bool effect)
{
    auto i = enchantments.find(ench);
    if (i == enchantments.end())
        return false;

    const mon_enchant me = i->second;
    const enchant_type et = i->first;

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

    case ENCH_FRENZIED:
        if (mons_is_elven_twin(this))
        {
            monster* twin = mons_find_elven_twin_of(this);
            if (twin && !twin->has_ench(ENCH_FRENZIED))
                attitude = twin->attitude;
            else
                attitude = ATT_HOSTILE;
        }
        mons_att_changed(this);
        break;

    case ENCH_BERSERK:
        scale_hp(2, 3);
        calc_speed();
        break;

    case ENCH_DOUBLED_HEALTH:
        scale_hp(1, 2);
        if (!quiet)
            simple_monster_message(*this, " excess health fades away.", true);
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

    case ENCH_VEXED:
        if (!quiet)
            simple_monster_message(*this, " is no longer overcome with frustration.");
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
        fully_petrify(quiet);

        if (alive()) // losing active flight over lava
            behaviour_event(this, ME_EVAL);
        break;

    case ENCH_FEAR:
    {
        string msg;
        if (is_nonliving() || berserk_or_frenzied())
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
        if (you.see_cell(pos()) && !you.can_see_invisible()
            && !friendly())
        {
            autotoggle_autopickup(false);

            if (backlit())
                break;

            if (!quiet)
                mprf("%s appears from thin air!", name(DESC_A, true).c_str());

            handle_seen_interrupt(this);
        }
        break;

    case ENCH_CHARM:
    case ENCH_NEUTRAL_BRIBED:
    case ENCH_FRIENDLY_BRIBED:
    case ENCH_HEXED:
        if (invisible() && you.see_cell(pos()) && !you.can_see_invisible()
            && !backlit())
        {
            if (!quiet)
            {
                mprf("%s is no longer %s.", name(DESC_THE, true).c_str(),
                        me.ench == ENCH_CHARM   ? "charmed"
                        : me.ench == ENCH_HEXED ? "hexed"
                                                : "bribed");

                mprf("You can %s detect %s.",
                     friendly() ? "once again" : "no longer",
                     name(DESC_THE, true).c_str());
            }

            if (!friendly())
            {
                //turn off auto pickup
                autotoggle_autopickup(true);
            }
        }
        else
        {
            if (!quiet)
            {
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
            interrupt_activity(activity_interrupt::see_monster,
                               activity_interrupt_data(this, SC_UNCHARM));
        }

        if (is_patrolling())
        {
            // Charmed monsters stop patrolling and forget their patrol point,
            // in case they were on order to wait.
            patrol_point.reset();
        }
        mons_att_changed(this);

        // Reevaluate behaviour.
        behaviour_event(this, ME_EVAL);
        break;

    case ENCH_CONTAM:
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

    case ENCH_SUMMON_TIMER:
        if (type == MONS_SPECTRAL_WEAPON)
            return end_spectral_weapon(this, false);
        else if (type == MONS_BATTLESPHERE)
            return end_battlesphere(this, false);

        if (berserk())
            simple_monster_message(*this, " is no longer berserk.");

        monster_die(*this, quiet ? KILL_RESET : KILL_TIMEOUT, NON_MONSTER);
        break;

    case ENCH_SOUL_RIPE:
        // Print message notifying the player, even if they cannot see us
        if (!quiet)
            mprf("You lose your grip on %s soul.", name(DESC_ITS, true).c_str());
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
            simple_monster_message(*this, " inner flame fades away.", true);
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

    case ENCH_STRONG_WILLED:
        if (!quiet)
            simple_monster_message(*this, " is no longer strong-willed.");
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
        if (type != MONS_SOUL_WISP && type != MONS_CLOCKWORK_BEE)
        {
            mon_enchant timer = get_ench(ENCH_SUMMON_TIMER);
            timer.degree = 1;
            timer.duration = min(5 + random2(30), timer.duration);
            update_ench(timer);
        }
        break;

    case ENCH_WEAK:
        if (!quiet)
            simple_monster_message(*this, " is no longer weakened.");
        break;

    case ENCH_TOXIC_RADIANCE:
        if (!quiet && you.can_see(*this))
            mprf("%s toxic aura wanes.", name(DESC_ITS).c_str());
        break;

    case ENCH_FIRE_VULN:
        if (!quiet)
            simple_monster_message(*this, " is no longer more vulnerable to fire.");
        break;

    case ENCH_MERFOLK_AVATAR_SONG:
        props.erase(MERFOLK_AVATAR_CALL_KEY);
        break;

    case ENCH_POISON_VULN:
        if (!quiet)
            simple_monster_message(*this, " is no longer more vulnerable to poison.");
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

    case ENCH_SIGN_OF_RUIN:
        simple_monster_message(*this, " sign of ruin fades away.", true);
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

    case ENCH_RESISTANCE:
        if (!quiet)
            simple_monster_message(*this, " is no longer unusually resistant.");
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

    case ENCH_VILE_CLUTCH:
    case ENCH_GRASPING_ROOTS:
    {
        if (is_constricted())
        {
            if (!quiet && you.can_see(*this))
            {
                if (me.ench == ENCH_VILE_CLUTCH)
                {
                    mprf("%s zombie hands return to the earth.",
                         me.agent()->name(DESC_THE).c_str());
                }
                else
                {
                    mprf("%s grasping roots sink back into the ground.",
                         me.agent()->name(DESC_THE).c_str());
                }
            }
            stop_being_constricted(true);
        }
        break;
    }

    case ENCH_STILL_WINDS:
        end_still_winds();
        break;

    case ENCH_WATERLOGGED:
        if (!quiet)
            simple_monster_message(*this, " is no longer waterlogged.");
        break;

    case ENCH_ROLLING:
        if (!quiet)
            simple_monster_message(*this, " stops rolling and uncurls.");
        break;

    case ENCH_CONCENTRATE_VENOM:
        if (!quiet)
            simple_monster_message(*this, " no longer looks unusually toxic.");
        break;

    case ENCH_BOUND:
        // From Sigil of Binding
        if (props.exists(BINDING_SIGIL_DURATION_KEY))
        {
            if (!quiet)
                simple_monster_message(*this, " lost momentum returns!", true);
            add_ench(mon_enchant(ENCH_SWIFT, 1, &you,
                                 props[BINDING_SIGIL_DURATION_KEY].get_int()));
            props.erase(BINDING_SIGIL_DURATION_KEY);
        }
        // Fathomless Shackles doesn't need messages for removal

        break;

    case ENCH_BULLSEYE_TARGET:
        // We check that this is the 'real' target before removing the status
        // from the player on death, since the status can be transiently created
        // on cloned monsters and silently removing it shouldn't end the status
        // on the original.
        if ((mid_t) you.props[BULLSEYE_TARGET_KEY].get_int() == mid)
            you.set_duration(DUR_DIMENSIONAL_BULLSEYE, 0);
        break;

    case ENCH_PROTEAN_SHAPESHIFTING:
    {
        monster_type poly_target = (monster_type)props[PROTEAN_TARGET_KEY].get_int();

        if (you.can_see(*this))
        {
            mprf(MSGCH_MONSTER_SPELL, "%s shapes itself into a furious %s!",
                    name(DESC_THE).c_str(),
                    mons_type_name(poly_target, DESC_PLAIN).c_str());
        }

        change_monster_type(this, poly_target, true);
        add_ench(mon_enchant(ENCH_HASTE, 1, this, INFINITE_DURATION));
        add_ench(mon_enchant(ENCH_MIGHT, 1, this, INFINITE_DURATION));

        // We add the enchantment back with infinite duration to mark the new
        // monster as having been created by a progenitor, so that it will be
        // properly tracked as chaotic, no matter what it's turned into.
        add_ench(mon_enchant(ENCH_PROTEAN_SHAPESHIFTING, 1,
                             this, INFINITE_DURATION));
    }
    break;

    case ENCH_CURSE_OF_AGONY:
        if (you.can_see(*this) && !quiet)
        {
            mprf("%s is freed from %s curse.", name(DESC_THE).c_str(),
                 pronoun(PRONOUN_POSSESSIVE).c_str());
        }
        break;

    case ENCH_TOUCH_OF_BEOGH:
        scale_hp(100, touch_of_beogh_hp_mult(*this));
        break;

    case ENCH_ARMED:
        // Restore our previous weapon(s)
        drop_item(MSLOT_WEAPON, false);
        if (props.exists(OLD_ARMS_KEY))
        {
            give_specific_item(this, props[OLD_ARMS_KEY].get_item());
            props.erase(OLD_ARMS_KEY);
        }
        if (props.exists(OLD_ARMS_ALT_KEY))
        {
            give_specific_item(this, props[OLD_ARMS_ALT_KEY].get_item());
            props.erase(OLD_ARMS_ALT_KEY);
        }
        break;

    case ENCH_MISDIRECTED:
        // If we're still targeting the shadow, immediately return focus to the player
        if (foe == me.agent()->mindex())
        {
            foe = MHITYOU;
            behaviour = BEH_SEEK;
            if (can_see(you))
            {
                target = you.pos();
                mprf("%s turns %s attention back to you.",
                        name(DESC_THE).c_str(),
                        pronoun(PRONOUN_POSSESSIVE).c_str());
            }
        }
        break;

    case ENCH_CHANGED_APPEARANCE:
        props.erase(MONSTER_TILE_KEY);
        break;

    case ENCH_KINETIC_GRAPNEL:
        if (you.can_see(*this) && !quiet)
            mprf("The grapnel comes loose from %s.", name(DESC_THE).c_str());
        break;

    case ENCH_BLINKITIS:
        if (!quiet)
            simple_monster_message(*this, " looks more stable.");

    case ENCH_CHAOS_LACE:
        if (!quiet)
            simple_monster_message(*this, " is no longer laced with chaos.");

    default:
        break;
    }
}

bool monster::lose_ench_levels(const mon_enchant &e, int lev, bool infinite)
{
    if (!lev)
        return false;

    // Check if this enchantment is being sustained by someone, and don't decay
    // in that case.
    if (e.ench_is_aura && aura_is_active(*this, e.ench))
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

    // Check if this enchantment is being sustained by someone, and don't decay
    // in that case.
    if (e.ench_is_aura && aura_is_active(*this, e.ench))
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

bool monster::clear_far_engulf(bool force, bool /*moved*/)
{
    if (you.duration[DUR_WATER_HOLD]
        && (mid_t) you.props[WATER_HOLDER_KEY].get_int() == mid)
    {
        you.clear_far_engulf(force);
    }

    const mon_enchant& me = get_ench(ENCH_WATER_HOLD);
    if (me.ench == ENCH_NONE)
        return false;
    const bool nonadj = !me.agent() || !adjacent(me.agent()->pos(), pos());
    if (nonadj || force)
        del_ench(ENCH_WATER_HOLD);
    return nonadj || force;
}

// Returns true if you resist the merfolk avatar's call.
static bool _merfolk_avatar_movement_effect(const monster* mons)
{
    if (you.attribute[ATTR_HELD]
        || you.duration[DUR_TIME_STEP]
        || you.cannot_act()
        || you.clarity()
        || !you.is_motile()
        || you.resists_dislodge("being lured by song"))
    {
        return true;
    }

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
        || is_feat_dangerous(env.grid(newpos))
        || !you.can_pass_through_feat(env.grid(newpos))
        || !cell_see_cell(mons->pos(), newpos, LOS_NO_TRANS))
    {
        return true;
    }

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
        else
            return true;
    }

    const coord_def oldpos = you.pos();
    mpr("The pull of its song draws you forwards.");

    if (swapping)
    {
        if (monster_at(oldpos))
        {
            mprf("Something prevents you from swapping places with %s.",
                 mon->name(DESC_THE).c_str());
            return false;
        }

        int swap_mon = env.mgrid(newpos);
        // Pick the monster up.
        env.mgrid(newpos) = NON_MONSTER;
        mon->moveto(oldpos);

        // Plunk it down.
        env.mgrid(mon->pos()) = swap_mon;

        mprf("You swap places with %s.",
             mon->name(DESC_THE).c_str());
    }
    move_player_to_grid(newpos, true);
    stop_delay(true);

    if (swapping)
        mon->apply_location_effects(newpos);

    return false;
}

static void _merfolk_avatar_song(monster* mons)
{
    // First, attempt to pull the player, if mesmerised
    if (you.beheld_by(*mons) && coinflip())
    {
        // Don't pull the player if they walked forward voluntarily this
        // turn (to avoid making you jump two spaces at once)
        if (!mons->props[FOE_APPROACHING_KEY].get_bool())
        {
            _merfolk_avatar_movement_effect(mons);

            // Reset foe tracking position so that we won't automatically
            // veto pulling on a subsequent turn because you 'approached'
            mons->props[FAUX_PAS_KEY].get_coord() = you.pos();
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
        if (mons->props.exists(MERFOLK_AVATAR_CALL_KEY))
        {
            // Normally can only happen if allies of the merfolk avatar show up
            // during a song that has already summoned drowned souls (though is
            // technically possible if some existing ally gains HD instead)
            if (you.see_cell(mons->pos()))
                mpr("The shadowy forms in the deep grow still as others approach.");
            mons->props.erase(MERFOLK_AVATAR_CALL_KEY);
        }

        return;
    }

    // Can only call up drowned souls if there's free deep water nearby
    vector<coord_def> deep_water;
    for (radius_iterator ri(mons->pos(), LOS_RADIUS, C_SQUARE); ri; ++ri)
        if (env.grid(*ri) == DNGN_DEEP_WATER && !actor_at(*ri))
            deep_water.push_back(*ri);

    if (deep_water.size())
    {
        if (!mons->props.exists(MERFOLK_AVATAR_CALL_KEY))
        {
            if (you.see_cell(mons->pos()))
            {
                mprf("Shadowy forms rise from the deep at %s song!",
                     mons->name(DESC_ITS).c_str());
            }
            mons->props[MERFOLK_AVATAR_CALL_KEY].get_bool() = true;
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
                    .set_summoned(mons, SPELL_NO_SPELL, summ_dur(1)));

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
    case ENCH_FRENZIED:
        if (decay_enchantment(en))
        {
            simple_monster_message(*this, " is no longer in a wild frenzy.");
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
    case ENCH_CONTAM:
    case ENCH_SUMMON_TIMER:
    case ENCH_CHARM:
    case ENCH_SLEEP_WARY:
    case ENCH_LOWERED_WL:
    case ENCH_TIDE:
    case ENCH_REGENERATION:
    case ENCH_DOUBLED_HEALTH:
    case ENCH_STRONG_WILLED:
    case ENCH_IDEALISED:
    case ENCH_LIFE_TIMER:
    case ENCH_FLIGHT:
    case ENCH_DAZED:
    case ENCH_RECITE_TIMER:
    case ENCH_INNER_FLAME:
    case ENCH_MUTE:
    case ENCH_BLIND:
    case ENCH_DUMB:
    case ENCH_MAD:
    case ENCH_WRETCHED:
    case ENCH_SCREAMED:
    case ENCH_WEAK:
    case ENCH_FIRE_VULN:
    case ENCH_BARBS:
    case ENCH_POISON_VULN:
    case ENCH_DIMENSION_ANCHOR:
    case ENCH_AGILE:
    case ENCH_FROZEN:
    case ENCH_SAP_MAGIC:
    case ENCH_CORROSION:
    case ENCH_GOLD_LUST:
    case ENCH_RESISTANCE:
    case ENCH_HEXED:
    case ENCH_EMPOWERED_SPELLS:
    case ENCH_BOUND_SOUL:
    case ENCH_INFESTATION:
    case ENCH_SIGN_OF_RUIN:
    case ENCH_STILL_WINDS:
    case ENCH_VILE_CLUTCH:
    case ENCH_GRASPING_ROOTS:
    case ENCH_WATERLOGGED:
    case ENCH_CONCENTRATE_VENOM:
    case ENCH_VITRIFIED:
    case ENCH_INSTANT_CLEAVE:
    case ENCH_PROTEAN_SHAPESHIFTING:
    case ENCH_CURSE_OF_AGONY:
    case ENCH_MAGNETISED:
    case ENCH_REPEL_MISSILES:
    case ENCH_MISDIRECTED:
    case ENCH_CHANGED_APPEARANCE:
    case ENCH_KINETIC_GRAPNEL:
    case ENCH_TEMPERED:
    case ENCH_CHAOS_LACE:
    case ENCH_VEXED:
        decay_enchantment(en);
        break;

    case ENCH_ANTIMAGIC:
        if (decay_enchantment(en))
            simple_monster_message(*this, " magic is no longer disrupted.", true);
        break;

    case ENCH_ROLLING:
        if (cannot_act() || asleep())
        {
            del_ench(en, true, false);
            break;
        }
        decay_enchantment(en);
        break;

    case ENCH_MIRROR_DAMAGE:
        if (decay_enchantment(en))
            simple_monster_message(*this, " dark mirror aura disappears.", true);
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
        if (monster_habitable_grid(this, pos()))
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
    case ENCH_BREATH_WEAPON:
        break; // handled in mon-act.cc:catch_breath()

    case ENCH_CONFUSION:
        if (!mons_class_flag(type, M_CONFUSED))
            decay_enchantment(en);
        break;

    case ENCH_INVIS:
        if (!mons_class_flag(type, M_INVIS))
            decay_enchantment(en);
        break;

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
        if (feat_is_water(env.grid(pos())) && ground_level())
        {
            if (you.can_see(*this))
            {
                mprf("The flames covering %s go out.",
                     name(DESC_THE, false).c_str());
            }
            del_ench(ENCH_STICKY_FLAME);
            break;
        }

        const int dam = resist_adjust_damage(this, BEAM_FIRE, roll_dice(2, 7));

        if (dam > 0)
        {
            simple_monster_message(*this, " burns!");
            dprf("sticky flame damage: %d", dam);
            hurt(me.agent(), dam, BEAM_STICKY_FLAME);
        }

        decay_enchantment(en, true);
        break;
    }

    case ENCH_SLOWLY_DYING:
        // If you are no longer dying, you must be dead.
        if (decay_enchantment(en))
            monster_die(*this, KILL_TIMEOUT, NON_MONSTER);
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
            coord_def base_position = props[BASE_POSITION_KEY].get_coord();
            // Do a thing.
            if (you.see_cell(base_position))
                mprf("The portal closes; %s is severed.", name(DESC_THE).c_str());

            if (env.grid(base_position) == DNGN_MALIGN_GATEWAY)
                env.grid(base_position) = DNGN_FLOOR;

            maybe_bloodify_square(base_position);
            add_ench(ENCH_SEVERED);

            // Severed tentacles immediately become "hostile" to everyone
            // (or frenzied)
            attitude = ATT_NEUTRAL;
            mons_att_changed(this);
            if (!crawl_state.game_is_arena())
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
            if (!crawl_state.game_is_arena())
                behaviour_event(this, ME_ALERT, &you);
        }
    }
    break;

    case ENCH_SEVERED:
    {
        simple_monster_message(*this, " writhes!");
        coord_def base_position = props[BASE_POSITION_KEY].get_coord();
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
        if (decay_enchantment(en, true) && !no_tele())
            monster_teleport(this, true, false, true);
        break;

    case ENCH_AWAKEN_FOREST:
        forest_damage(this);
        decay_enchantment(en);
        break;

    case ENCH_POLAR_VORTEX:
        polar_vortex_damage(this, speed_to_duration(speed));
        if (decay_enchantment(en))
        {
            add_ench(ENCH_POLAR_VORTEX_COOLDOWN);
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
            del_ench(en, true, false);
            if (you.can_see(*this))
                mprf("%s chant is interrupted.", name(DESC_ITS).c_str());
            break;
        }

        if (decay_enchantment(en))
        {
            const int breath_timeout_turns = random_range(4, 12);

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
            if (!res_water_drowning())
            {
                lose_ench_duration(me, -speed_to_duration(speed));
                int dur = speed_to_duration(speed); // sequence point for randomness
                int dam = div_rand_round((50 + stepdown((float)me.duration, 30.0))
                                          * dur,
                            BASELINE_DELAY * 10);
                if (dam > 0)
                {
                    simple_monster_message(*this, " is asphyxiated!");
                    hurt(me.agent(), dam);
                }
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
        {
            if (type != MONS_CLOCKWORK_BEE)
                del_ench(ENCH_HAUNTING);
            else
                clockwork_bee_pick_new_target(*this);
        }
        break;

    case ENCH_TOXIC_RADIANCE:
        toxic_radiance_effect(this, 10);
        decay_enchantment(en);
        break;

    case ENCH_POLAR_VORTEX_COOLDOWN:
        if (decay_enchantment(en))
        {
            remove_vortex_clouds(mid);
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

    case ENCH_ANGUISH:
        if (decay_enchantment(en))
            simple_monster_message(*this, " is no longer haunted by guilt.");
        break;

    case ENCH_CHANNEL_SEARING_RAY:
        // If we've gotten incapacitated since we started, cancel the spell
        if (is_silenced() || cannot_act() || confused() || asleep() || has_ench(ENCH_FEAR))
        {
            del_ench(en, true, false);
            if (you.can_see(*this))
                mprf("%s searing ray is interrupted.", name(DESC_ITS).c_str());
        }
        break;

    case ENCH_BOUND:
        // Remove Yred binding as soon as we're not in the effect area
        if (props.exists(YRED_SHACKLES_KEY)
            && (!is_blasphemy(pos()) || !is_blasphemy(you.pos())
                || !you.duration[DUR_FATHOMLESS_SHACKLES]))
        {
            props.erase(YRED_SHACKLES_KEY);
            del_ench(en, true, true);
        }
        else
            decay_enchantment(en);
        break;

    case ENCH_SOUL_RIPE:
        if (!cell_see_cell(you.pos(), pos(), LOS_NO_TRANS))
        {
            // This implies the player *just* lost sight of us, so start the countdown
            if (me.duration == INFINITE_DURATION)
            {
                // XXX: The most awkward way to work around not being able to lower
                //      duration directly, or decay things with INFINITE_DURATION....
                mon_enchant timeout(ENCH_SOUL_RIPE, me.degree, &you, 20);
                del_ench(en, true, false);
                add_ench(timeout);
            }
            else
            {
                if (!decay_enchantment(ENCH_SOUL_RIPE)
                    && me.duration <= 10)
                {
                    mprf("Your grip on %s soul is slipping...",
                         name(DESC_ITS, true).c_str());
                }
            }

        }
        else
        {
            if (me.duration < INFINITE_DURATION)
            {
                // XXX: See above. I hate it. Surely there's a better way than this.
                mon_enchant renew(ENCH_SOUL_RIPE, me.degree, &you, INFINITE_DURATION);
                del_ench(en, true, false);
                add_ench(renew);
            }
        }
        break;

    case ENCH_RIMEBLIGHT:
        tick_rimeblight(*this);
        if (!alive())
            return;
        // Instakill living/demonic/holy creatures that reach <=20% max hp
        if (holiness() & (MH_NATURAL | MH_DEMONIC | MH_HOLY)
            && hit_points * 5 <= max_hit_points)
        {
            props[RIMEBLIGHT_DEATH_KEY] = true;
            monster_die(*this, KILL_YOU, NON_MONSTER);
        }
        else if (decay_enchantment(en))
            simple_monster_message(*this, " recovers from rimeblight.");
        break;

    case ENCH_ARMED:
        // Remove this whenever the armoury stops being around;
        if (!me.agent())
            del_ench(en);
        decay_enchantment(en);
        break;

    case ENCH_BLINKITIS:
    {
        actor* agent = me.agent();
        if (agent)
            blink_away(this, agent, false, false, 3);
        else
            blink();

        hurt(agent, roll_dice(2, 2));
        if (alive())
            decay_enchantment(en);
        break;
    }

    default:
        break;
    }
}

void monster::mark_summoned(int summon_type, int longevity, bool mark_items,
                            bool make_abjurable)
{
    if (longevity > 0)
        add_ench(mon_enchant(ENCH_SUMMON_TIMER, 1, 0, longevity));

    // Fully replace any existing summon source, if we're giving a new one.
    if (has_ench(ENCH_SUMMON) && summon_type != 0)
        del_ench(ENCH_SUMMON);

    if (!has_ench(ENCH_SUMMON))
        add_ench(mon_enchant(ENCH_SUMMON, summon_type, 0, INT_MAX));

    if (mark_items)
        for (mon_inv_iterator ii(*this); ii; ++ii)
            ii->flags |= ISFLAG_SUMMONED;

    if (make_abjurable)
        flags |= MF_ACTUAL_SUMMON;
}

/* Is the monster created by a spell or effect (generally temporary)?
 *
 * Monsters with ENCH_SUMMON are considered summoned for this purpose, whether
 * or not they are 'actual' abjurable summons or non-abjurable magical creations
 * like battlespheres or hoarfrost cannons.
 *
 * This function is used to determine eligability for many effects (such as
 * vampiric draining), whether a monster should be able to take stairs, and also
 * to prevent it giving the player XP upon death.
 *
 * Note: We can't just look at whether they have a timer, since they won't have
 *       one during KILL_TIMEOUT
 *
 * @returns True if the monster is a (temporary) summon/creation, false otherwise.
 */
bool monster::is_summoned() const
{
    return has_ench(ENCH_SUMMON) && !is_unrewarding();
}

bool monster::was_created_by(int summon_type) const
{
    if (!has_ench(ENCH_SUMMON))
        return false;

    mon_enchant summ = get_ench(ENCH_SUMMON);
    return summ.degree == summon_type;
}

bool monster::was_created_by(const actor& _summoner, int summon_type) const
{
    if (!has_ench(ENCH_SUMMON))
        return false;

    if (summoner != _summoner.mid)
        return false;

    // If we didn't specify a specific summon type to check for, any summon
    // type will do.
    if (summon_type == SPELL_NO_SPELL)
        return true;

    // Otherwise check that the creating spell/effect matches the query.
    mon_enchant summ = get_ench(ENCH_SUMMON);
    return summ.degree == summon_type;
}

// Returns whether the monster is a 'proper' summon, vulnerable to abjuration,
// and whose corpse will vanish (possibly in a puff of smoke) upon them dying
// for any reason.
bool monster::is_abjurable() const
{
    return is_summoned() && testbits(flags, MF_ACTUAL_SUMMON);
}

// Returns whether the monster will be explicitly marked by the UI as
// Unrewarding, regardless of its normal properties. This is used for monsters
// created by god wrath, spawns from The Royal Jelly, and some other situations
// where we want a monster that is otherwise normal, but provides the player
// with no reward.
//
// Note: This returns false for monsters which provide no XP for other reasons,
//       such as being firewood or summoned.
bool monster::is_unrewarding() const
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
    "summon", "summon_timer", "corona",
    "charm", "sticky_flame", "glowing_shapeshifter", "shapeshifter", "tp",
    "sleep_wary",
#if TAG_MAJOR_VERSION == 34
    "submerged", "short_lived",
#endif
    "paralysis", "sick",
#if TAG_MAJOR_VERSION == 34
    "sleepy",
#endif
    "held",
#if TAG_MAJOR_VERSION == 34
     "battle_frenzy", "temp_pacif",
#endif
    "petrifying",
    "petrified", "lowered_wl", "soul_ripe", "slowly_dying",
#if TAG_MAJOR_VERSION == 34
    "eat_items",
#endif
    "aquatic_land",
#if TAG_MAJOR_VERSION == 34
    "spore_production",
    "slouch",
#endif
    "swift", "tide",
    "frenzied", "silenced", "awaken_forest", "exploding",
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
    "stoneskin", "fear inspiring",
#endif
    "temporarily pacified",
#if TAG_MAJOR_VERSION == 34
    "withdrawn", "attached",
#endif
    "guardian_timer", "flight", "liquefying", "polar_vortex",
#if TAG_MAJOR_VERSION == 34
    "fake_abjuration",
#endif
    "dazed", "mute", "blind", "dumb", "mad", "silver_corona", "recite timer",
    "inner_flame",
#if TAG_MAJOR_VERSION == 34
    "roused",
#endif
    "breath timer",
#if TAG_MAJOR_VERSION == 34
    "deaths_door",
#endif
    "rolling",
#if TAG_MAJOR_VERSION == 34
    "ozocubus_armour",
#endif
    "wretched", "screamed", "rune_of_recall",
    "injury bond", "drowning", "flayed", "haunting",
#if TAG_MAJOR_VERSION == 34
    "retching",
#endif
    "weak", "dimension_anchor",
#if TAG_MAJOR_VERSION == 34
     "awaken vines", "control_winds", "wind_aided", "summon_capped",
#endif
    "toxic_radiance",
#if TAG_MAJOR_VERSION == 34
    "grasping_roots_source",
#endif
    "grasping_roots",
    "iood_charged", "fire_vuln", "polar_vortex_cooldown", "merfolk_avatar_song",
    "barbs",
#if TAG_MAJOR_VERSION == 34
    "building_charge",
#endif
    "poison_vuln",
#if TAG_MAJOR_VERSION == 34
    "icemail",
#endif
    "agile",
    "frozen",
#if TAG_MAJOR_VERSION == 34
    "ephemeral_infusion",
#endif
    "black_mark",
#if TAG_MAJOR_VERSION == 34
    "grand_avatar",
#endif
    "sap magic",
#if TAG_MAJOR_VERSION == 34
    "shroud",
#endif
    "phantom_mirror", "bribed", "permabribed",
    "corrosion", "gold_lust", "drained", "repel missiles",
#if TAG_MAJOR_VERSION == 34
    "deflect missiles",
    "negative_vuln", "condensation_shield",
#endif
    "resistant", "hexed",
#if TAG_MAJOR_VERSION == 34
    "corpse_armour",
    "chanting_fire_storm", "chanting_word_of_entropy", "aura_of_brilliance",
#endif
    "empowered_spells", "gozag_incite", "pain_bond",
    "idealised", "bound_soul", "infestation",
    "stilling the winds", "thunder_ringed",
#if TAG_MAJOR_VERSION == 34
    "pinned_by_whirlwind", "vortex", "vortex_cooldown",
#endif
    "vile_clutch", "waterlogged", "ring_of_flames",
    "ring_chaos", "ring_mutation", "ring_fog", "ring_ice", "ring_neg",
    "ring_acid", "ring_miasma", "concentrate_venom", "fire_champion",
    "anguished",
#if TAG_MAJOR_VERSION == 34
    "simulacra", "necrotising",
#endif
     "glowing",
#if TAG_MAJOR_VERSION == 34
    "pursuing",
#endif
    "bound", "bullseye_target", "vitrified", "cleaving_attack",
    "protean_shapeshifting", "simulacrum_sculpting", "curse_of_agony",
    "channel_searing_ray",
    "touch_of_beogh", "vengeance_target",
    "rimeblight",
    "magnetised",
    "armed",
    "misdirected", "changed appearance", "shadowless", "doubled_health",
    "grapnel", "tempered", "hatching", "blinkitis", "chaos_laced", "vexed",
    "buggy", // NUM_ENCHANTMENTS
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
                         int dur, ench_aura_type is_aura)
    : ench(e), degree(deg), duration(dur), maxduration(0), ench_is_aura(is_aura)
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
    const int max = (ench == ENCH_SUMMON_TIMER) ?
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
                              : KILL_NON_ACTOR;
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
    case ENCH_WEAK:
    case ENCH_INVIS:
    case ENCH_AGILE:
    case ENCH_SIGN_OF_RUIN:
    case ENCH_RESISTANCE:
    case ENCH_IDEALISED:
    case ENCH_BOUND_SOUL:
    case ENCH_ANGUISH:
    case ENCH_CONCENTRATE_VENOM:
        cturn = 1000 / _mod_speed(25, mons->speed);
        break;
    case ENCH_LIQUEFYING:
    case ENCH_SILENCE:
    case ENCH_REGENERATION:
    case ENCH_STRONG_WILLED:
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
    case ENCH_CONTAM: // TODO: maybe faster
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
    case ENCH_SLOWLY_DYING:
    {
        // This may be a little too direct but the randomization at the end
        // of this function is excessive for toadstools. -cao
        int dur = speed_to_duration(mons->speed); // uses div_rand_round, so we need a sequence point
        return (2 * FRESHEST_CORPSE + random2(10))
                  * dur;
    }
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

    case ENCH_SUMMON_TIMER:
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
    case ENCH_DOUBLED_HEALTH:
        return (16 + random2avg(13, 2)) * 10;
    case ENCH_ROLLING:
        return random_range(10 * BASELINE_DELAY, 15 * BASELINE_DELAY);
    case ENCH_WRETCHED:
        cturn = (20 + roll_dice(3, 10)) * 10 / _mod_speed(10, mons->speed);
        break;
    case ENCH_POLAR_VORTEX_COOLDOWN:
        cturn = random_range(25, 35) * 10 / _mod_speed(10, mons->speed);
        break;
    case ENCH_FROZEN:
        cturn = 5 * 10 / _mod_speed(10, mons->speed);
        break;
    case ENCH_EMPOWERED_SPELLS:
        cturn = 35 * 10 / _mod_speed(10, mons->speed);
        break;
    case ENCH_RING_OF_THUNDER:
    case ENCH_RING_OF_FLAMES:
    case ENCH_RING_OF_CHAOS:
    case ENCH_RING_OF_MUTATION:
    case ENCH_RING_OF_FOG:
    case ENCH_RING_OF_ICE:
    case ENCH_RING_OF_MISERY:
    case ENCH_RING_OF_ACID:
    case ENCH_RING_OF_MIASMA:
    case ENCH_GOZAG_INCITE:
        cturn = 100; // is never decremented
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

// Converts a given summon duration category into a duration in aut (with random
// fuzzing).
int summ_dur(int degree)
{
    int aut = 0;

    if (degree >= 6)
        aut = 1000;
    if (degree >= 5)
        aut += 500;
    aut += 100 * min(4, degree);

    return fuzz_value(aut, 60, 40);
}
