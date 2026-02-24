/**
 * @file
 * @brief melee_attack class and associated melee_attack methods
 */

#include "AppHdr.h"

#include "melee-attack.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "attitude-change.h"
#include "bloodspatter.h"
#include "chardump.h"
#include "cloud.h"
#include "delay.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "evoke.h"
#include "exercise.h"
#include "fineff.h"
#include "god-conduct.h"
#include "god-item.h"
#include "god-passive.h" // passive_t::convert_orcs
#include "hints.h"
#include "invent.h"
#include "item-prop.h"
#include "los.h"
#include "losparam.h"
#include "mapdef.h"
#include "message.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-death.h" // maybe_drop_monster_organ
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-tentacle.h"
#include "nearby-danger.h"
#include "religion.h"
#include "shout.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-summoning.h" //AF_SPIDER
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "tag-version.h"
#include "target.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "unwind.h"
#include "view.h"
#include "xom.h"

#ifdef NOTE_DEBUG_CHAOS_BRAND
    #define NOTE_DEBUG_CHAOS_EFFECTS
#endif

#ifdef NOTE_DEBUG_CHAOS_EFFECTS
    #include "notes.h"
#endif

/*
 **************************************************
 *             BEGIN PUBLIC FUNCTIONS             *
 **************************************************
*/
melee_attack::melee_attack(actor *attk, actor *defn,
                           int attack_num, int effective_attack_num)
    :  // Call attack's constructor
    ::attack(attk, defn),

    attack_number(attack_num), effective_attack_number(effective_attack_num),
    cleaving(false), is_followup(false), is_riposte(false),
    is_projected(false), is_bestial_takedown(false), is_sunder(false),
    charge_pow(0),
    never_cleave(false),
    is_involuntary(false),
    wu_jian_attack(WU_JIAN_ATTACK_NONE),
    wu_jian_number_of_targets(1),
    is_attacking_hostiles(false),
    is_special_mon_stab(false)
{
    attack_position = attacker->pos();
    set_weapon(attacker->weapon(attack_num));
}

bool melee_attack::can_reach(int dist)
{
    const int wpn_reach = weapon ? weapon_reach(*weapon) : 1;
    const int range_bonus =
            you.form == transformation::aqua
                && (attacker->is_player() || attacker->type == MONS_PLAYER_SHADOW)
                    ? 2 : 0;

    return dist <= 1
           || attk_type == AT_HIT && wpn_reach + range_bonus >= dist
           || flavour_has_reach(attk_flavour)
           || is_projected
           || is_sunder;
}

bool melee_attack::bad_attempt()
{
    // Only prompt on the first of a series of attacks
    if (attack_number || is_followup || cleaving)
        return false;

    if (!attacker->is_player() || !defender || !defender->is_monster())
        return false;

    // If the player is confused, they are too unaware to make these checks.
    if (you.confused())
        return false;

    if (!is_projected && player_unrand_bad_attempt())
        return true;

    if (!cleave_targets.empty())
    {
        const int range = you.reach_range();
        targeter_cleave hitfunc(attacker, defender->pos(), range);
        return stop_attack_prompt(hitfunc, "attack");
    }

    return stop_attack_prompt(defender->as_monster(), false, attack_position);
}

// Whether this attack, if performed, would prompt the player about damaging
// nearby allies with an unrand property (or make your god unhappy).
bool melee_attack::would_prompt_player()
{
    if (!attacker->is_player())
        return false;

    item_def* w1 = primary_weapon();
    item_def* w2 = offhand_weapon();
    bool penance;
    return w1 && needs_handle_warning(*w1, OPER_ATTACK, penance, false)
           || w2 && needs_handle_warning(*w2, OPER_ATTACK, penance, false)
           || player_unrand_bad_attempt(true);
}

bool melee_attack::player_unrand_bad_attempt(bool check_only)
{
    // Unrands with secondary effects that can harm nearby friendlies.
    // Don't prompt for confirmation (and leak information about the
    // monster's position) if the player can't see the monster.
    if (!you.can_see(*defender))
        return false;

    item_def* primary = primary_weapon();
    item_def* offhand = offhand_weapon();

    return ::player_unrand_bad_attempt(primary, offhand, defender, check_only);
}

// Freeze a random wall adjacent to our target. If all those are frozen, freeze
// one adjacent to the player instead. Regardless of either, extend the duration
// of all frozen walls nearby.
static void _do_rime_yak_freeze(coord_def targ)
{
    vector<coord_def> new_spots;
    vector<coord_def> frigid_spots;

    for (adjacent_iterator ai(targ); ai; ++ai)
    {
        if (feat_is_wall(env.grid(*ai)) && env.grid(*ai) != DNGN_FRIGID_WALL
            && you.see_cell_no_trans(*ai))
        {
            new_spots.push_back(*ai);
        }
    }

    if (new_spots.empty())
    {
        for (adjacent_iterator ai(you.pos()); ai; ++ai)
        {
            if (feat_is_wall(env.grid(*ai)) && env.grid(*ai) != DNGN_FRIGID_WALL
                && you.see_cell_no_trans(*ai))
            {
                new_spots.push_back(*ai);
            }
        }
    }

    if (!new_spots.empty())
    {
        temp_change_terrain(new_spots[random2(new_spots.size())], DNGN_FRIGID_WALL,
                            20, TERRAIN_CHANGE_RIME_YAK, MID_PLAYER);
    }

    // Extend duration of all frozen walls in range
    for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS, true); ri; ++ri)
    {
        if (env.grid(*ri) == DNGN_FRIGID_WALL)
        {
            temp_change_terrain(*ri, DNGN_FRIGID_WALL, 30 + random_range(0, 30),
                                TERRAIN_CHANGE_RIME_YAK, MID_PLAYER);
        }
    }

    // Used to tell the game to run the wall damage phase after each player
    // turn. Just needs to be longer than the max possible duration of walls
    // which may exist.
    you.duration[DUR_RIME_YAK_AURA] = 70;
}

bool melee_attack::handle_phase_attempted()
{
    // Skip invalid and dummy attacks.
    if (defender && !can_reach(grid_distance(attack_position, defender->pos()))
        || attk_flavour == AF_CRUSH
           && !attacker->can_constrict(*defender, CONSTRICT_MELEE))
    {
        return false;
    }

    // If this is a non-attack, exit now.
    if (attk_type == AT_NONE)
        return false;

    if (attacker != defender && !is_riposte)
    {
        // Allow setting of your allies' target, etc.
        attacker->attacking(defender);

        if (is_attacking_hostiles && x_chance_in_y(attacker->angry(), 100))
            attacker->go_berserk(false);
    }

    // Self-attacks due to confusion are still pretty funny, though.
    if (attacker == defender && attacker->confused())
        xom_is_stimulated(100);

    if (attk_flavour == AF_SHADOWSTAB
        && defender && !defender->can_see(*attacker))
    {
        if (you.see_cell(attack_position))
        {
            mprf("%s strikes at %s from the darkness!",
                 attacker->name(DESC_THE, true).c_str(),
                 defender->name(DESC_THE).c_str());
        }
        to_hit = AUTOMATIC_HIT;
        needs_message = false;
    }
    else if (is_projected)
        to_hit = AUTOMATIC_HIT;
    else if (attacker->is_monster()
             && attacker->type == MONS_DROWNED_SOUL)
    {
        to_hit = AUTOMATIC_HIT;
    }
    else if (defender && defender->is_monster()
             && defender->as_monster()->has_ench(ENCH_KINETIC_GRAPNEL))
    {
        mon_enchant grapnel = defender->as_monster()->get_ench(ENCH_KINETIC_GRAPNEL);
        if (grapnel.agent() == attacker)
        {
            to_hit = AUTOMATIC_HIT;
            flat_dmg_bonus = random_range(0, 3);
            defender->as_monster()->del_ench(ENCH_KINETIC_GRAPNEL, true);
            if (attacker->is_player())
                mpr("The grapnel guides your strike.");
        }
    }

    // Check for player practicing dodging
    if (defender && defender->is_player())
        practise_being_attacked();

    return true;
}

void melee_attack::handle_phase_blocked()
{
    //We need to handle jinxbite here instead of in
    //attack::handle_phase_blocked as some attacks
    //such as darts don't trigger it
    maybe_trigger_jinxbite();

    bool needs_block_message = needs_message;
    if (defender->wearing_ego(OBJ_WEAPONS, SPWPN_REBUKE))
    {
        actor* best_targ = nullptr;
        for (distance_iterator di(attacker->pos(), true, true, attacker->reach_range()); di; ++di)
        {
            if (!defender->see_cell_no_trans(*di))
                continue;

            if (actor* act = actor_at(*di))
            {
                if (act == attacker || !act->visible_to(defender) || !mons_aligned(attacker, act))
                    continue;

                if (!best_targ || best_targ->stat_hp() > act->stat_hp())
                    best_targ = act;
            }
        }

        if (best_targ)
        {
            if (you.see_cell(defender->pos()))
            {
                mprf("%s turn%s aside %s attack!",
                        defender->name(DESC_THE).c_str(),
                        defender->is_monster() ? "s" : "",
                        attacker->name(DESC_ITS).c_str());
                needs_block_message = false;
            }

            melee_attack rebuke(attacker, best_targ, attack_number, 0);
            copy_params_to(rebuke);
            rebuke.responsible = attacker;
            rebuke.never_cleave = true;
            rebuke.is_involuntary = true;
            rebuke.to_hit_bonus = 15;
            rebuke.attack();
        }
    }

    if (needs_block_message)
    {
        mprf("%s %s %s attack.",
                 defender_name(false).c_str(),
                 defender->conj_verb("block").c_str(),
                 attacker == defender ? "its own"
                                      : atk_name(DESC_ITS).c_str());
    }

    if (defender->is_player() && you.duration[DUR_DIVINE_SHIELD]
        && coinflip() && attacker->as_monster()->res_blind() <= 1)
    {
        const bool need_msg = !attacker->as_monster()->has_ench(ENCH_BLIND);
        if (attacker->as_monster()->add_ench(mon_enchant(ENCH_BLIND, &you,
                                        random_range(3, 5) * BASELINE_DELAY))
            && need_msg)
        {
            mprf("%s is struck blind by the light of your shield.",
                    attacker->name(DESC_THE).c_str());
        }
    }

    attack::handle_phase_blocked();
}

static int _minotaur_headbutt_chance()
{
    return 23 + you.experience_level;
}

void melee_attack::handle_phase_dodged()
{
    did_hit = false;

    if (needs_message)
    {
        // TODO: Unify these, placed player_warn_miss here so I can remove
        // player_attack
        if (attacker->is_player())
            player_warn_miss();
        else
        {
            mprf("%s%s misses %s.",
                 atk_name(DESC_THE).c_str(),
                 evasion_margin_adverb().c_str(),
                 defender_name(true).c_str());
        }
    }

    if (attacker->is_player())
    {
        // Upset only non-sleeping non-fleeing monsters if we missed.
        if (!defender->asleep() && !mons_is_fleeing(*defender->as_monster()))
            behaviour_event(defender->as_monster(), ME_WHACK, attacker);
    }

    if (defender->is_player())
        count_action(CACT_DODGE, DODGE_EVASION);

    maybe_trigger_jinxbite();

    // Player-centric retaliation effects.
    if (attacker != defender
        && defender->is_player()
        && attacker->alive()
        && !mons_aligned(attacker, defender) // confused friendlies attacking
        && !attacker->as_monster()->neutral()) // don't anger neutrals, even if they hit you
    {
        // Active retaliations on the player's part require you to be able to act.
        if (you.can_see(*attacker) && !you.cannot_act() && !you.confused())
        {
            const int aux_dist = you.form == transformation::aqua ? 3 : 1;
            if (grid_distance(defender->pos(), attack_position) <= aux_dist)
            {
                if (you.has_mutation(MUT_REFLEXIVE_HEADBUTT)
                    && !you.did_reprisal(REPRISAL_HEADBUTT, attacker->mid)
                    && x_chance_in_y(_minotaur_headbutt_chance(), 100))
                {
                    you.track_reprisal(REPRISAL_HEADBUTT, attacker->mid);

                    mpr("You furiously retaliate!");
                    melee_attack headbutt(&you, attacker);
                    headbutt.player_aux_setup(UNAT_HEADBUTT);
                    headbutt.player_aux_apply(UNAT_HEADBUTT);
                    if (!attacker->alive())
                        return;
                }

                if (you.duration[DUR_EXECUTION])
                {
                    melee_attack retaliation(&you, attacker);
                    retaliation.player_aux_setup(UNAT_EXECUTIONER_BLADE);
                    retaliation.player_aux_apply(UNAT_EXECUTIONER_BLADE);
                    if (!attacker->alive())
                        return;
                }
            }

            maybe_riposte();
            if (!attacker->alive())
                return;
        }

        // ...but passive ones do not.
        maybe_trigger_autodazzler();

        if (you.unrand_equipped(UNRAND_STARLIGHT))
            do_starlight();
    }
}

void melee_attack::maybe_riposte()
{
    // only riposte via fencer's gloves, which (I take it from this code)
    // monsters can't use
    const bool using_fencers =
                defender->is_player()
                    && you.unrand_equipped(UNRAND_FENCERS)
                    && (!defender->weapon()
                        || is_melee_weapon(*defender->weapon()));
    if (using_fencers
        && !you.did_reprisal(REPRISAL_FENCER, attacker->mid)
        && one_chance_in(3)
        && you.reach_range() >= grid_distance(you.pos(), attack_position))
    {
        riposte();
    }
}

void melee_attack::apply_black_mark_effects()
{
    enum black_mark_effect
    {
        ANTIMAGIC,
        WEAKNESS,
        DRAINING,
    };

    if (attacker->is_player()
        && you.has_mutation(MUT_BLACK_MARK)
        && one_chance_in(5))
    {
        if (!defender->alive())
            return;

        vector<black_mark_effect> effects;

        if (defender->antimagic_susceptible())
            effects.push_back(ANTIMAGIC);
        if (defender->is_player()
            || mons_has_attacks(*defender->as_monster()))
        {
            effects.push_back(WEAKNESS);
        }
        if (defender->res_negative_energy() < 3)
            effects.push_back(DRAINING);

        if (effects.empty())
            return;

        black_mark_effect choice = effects[random2(effects.size())];

        switch (choice)
        {
            case ANTIMAGIC:
                antimagic_affects_defender(damage_done * 8);
                break;
            case WEAKNESS:
                defender->weaken(attacker, 6);
                break;
            case DRAINING:
                defender->drain(attacker, false, damage_done);
                break;
        }
    }
}

void melee_attack::apply_sign_of_ruin_effects()
{
    enum ruin_effect
    {
        SLOW,
        WEAKNESS,
        BLIND,
    };

    if (!defender->alive())
        return;

    if (defender->is_monster() && defender->as_monster()->has_ench(ENCH_SIGN_OF_RUIN)
        || defender->is_player() && you.duration[DUR_SIGN_OF_RUIN])
    {
        // Always drain heavily, then apply one other random effect
        defender->drain(attacker, false, random_range(30, 50));

        // The draining itself might kill the victim.
        if (!defender->alive())
            return;

        vector<ruin_effect> effects;

        if (defender->is_player()
            || mons_has_attacks(*defender->as_monster()))
        {
            effects.push_back(WEAKNESS);
        }
        if (!defender->res_blind())
            effects.push_back(BLIND);
        if (!defender->stasis())
            effects.push_back(SLOW);

        if (effects.empty())
            return;

        ruin_effect choice = effects[random2(effects.size())];

        switch (choice)
        {
            case SLOW:
                defender->slow_down(attacker, random_range(5, 8));
                break;
            case WEAKNESS:
                defender->weaken(attacker, 6);
                break;
            case BLIND:
                if (defender->is_monster())
                {
                    defender->as_monster()->add_ench(mon_enchant(ENCH_BLIND, attacker,
                                                    random_range(5, 8) * BASELINE_DELAY));
                    simple_monster_message(*defender->as_monster(), " is struck blind.");
                }
                else
                    blind_player(random_range(5, 8));
                break;
        }
    }
}

void melee_attack::do_ooze_flood()
{
    if (attacker->is_player()
        && you.has_mutation(MUT_OOZE_FLOOD)
        && defender->alive()
        && coinflip())
    {
        defender->floodify(attacker, random_range(30, 70), "ooze");
    }
}

void melee_attack::try_parry_disarm()
{
    if (attacker->is_player()
        && defender->is_monster()
        && defender->alive()
        && you.rev_percent() > FULL_REV_PERCENT
        && you.wearing_ego(OBJ_GIZMOS, SPGIZMO_REVGUARD)
        && one_chance_in(50 + defender->get_experience_level() * 2
                         - you.get_experience_level()))
    {
        item_def *wpn = defender->as_monster()->disarm();
        if (wpn)
        {
            mprf("You knock %s out of %s grip!",
                wpn->name(DESC_THE).c_str(),
                defender->name(DESC_ITS).c_str());
        }
    }
}

void melee_attack::do_vampire_lifesteal()
{
    monster* mon = defender->as_monster();
    if (attacker->is_player()
        && (you.form == transformation::vampire
            || you.form == transformation::bat_swarm)
        && (stab_attempt || you.hp * 2 <= you.hp_max)
        && adjacent(you.pos(), mon->pos())
        && !mons_class_flag(defender->type, M_ACID_SPLASH))
    {
        // Stabs always heal, while thirsty attacks have a 2/3 chance.
        if (!stab_attempt && !x_chance_in_y(2, 3))
            return;

        const bool can_heal = actor_is_susceptible_to_vampirism(*mon);
        const bool can_enthrall = stab_attempt && !mon->is_summoned()
                                  && !mon->alive()
                                  && mon->holiness() & (MH_NATURAL | MH_PLANT | MH_DEMONIC);

        if (can_heal && !can_enthrall)
        {
            mprf("You sink your fangs into %s and drink %s %s.",
                    mon->name(DESC_THE, true).c_str(),
                    mon->pronoun(PRONOUN_POSSESSIVE).c_str(),
                    mon->blood_name().c_str());
        }
        else if (can_enthrall)
        {
            mprf("You sink your fangs into %s and drain %s dry!",
                    mon->name(DESC_THE, true).c_str(),
                    mon->pronoun(PRONOUN_OBJECTIVE).c_str());

            // Mark to possibly raise this monster as an ally.
            if (stab_attempt)
                mon->props[VAMPIRIC_THRALL_KEY] = true;
        }

        if (can_heal)
        {
            int heal = random2(damage_done);
            if (heal > 0 && you.hp < you.hp_max && !you.duration[DUR_DEATHS_DOOR])
            {
                you.heal(heal);
                canned_msg(MSG_GAIN_HEALTH);
            }
        }
    }
}

void melee_attack::handle_concussion_brand()
{
    const coord_def old_pos = defender->pos();
    bool did_move = false;
    if (defender->stumble_away_from(attack_position, "concussive force"))
    {
        did_move = true;
        if (monster* mon = defender->as_monster())
            mon->stagger(1 + random2(min(10, attacker->attack_delay().roll())) * 3 / 4);
    }
    else if (damage_done > 0)
    {
        const coord_def back = defender->pos() + (defender->pos() - attacker->pos()).sgn();
        if (feat_is_solid(env.grid(back)))
        {
            special_damage = random2(damage_done) * 3 / 4 + 1;
            if (needs_message)
            {
                mprf("%s crush%s %s against the %s%s",
                        attacker->name(DESC_THE).c_str(),
                        attacker->is_monster() ? "es" : "",
                        defender->name(DESC_THE).c_str(),
                        feat_is_wall(env.grid(back)) ? "wall"
                                                    : raw_feature_description(back).c_str(),
                        attack_strength_punctuation(special_damage).c_str());
            }
            inflict_damage(special_damage);
        }
    }

    // Do followup movement only for attacks that have a reasonably defined
    // 'primary target', such as regular bump attacks, lunges, or Vhi's.
    if (did_move && !is_projected && !cleaving && !never_cleave
        && wu_jian_attack != WU_JIAN_ATTACK_WHIRLWIND
        && wu_jian_attack != WU_JIAN_ATTACK_WALL_JUMP
        && adjacent(attacker->pos(), old_pos)
        && !attacker->cannot_move())
    {
        schedule_trample_follow_fineff(attacker, old_pos);
    }
}

static void _apply_flux_contam(monster &m)
{
    const mon_enchant old_glow = m.get_ench(ENCH_CONTAM);

    if (old_glow.degree >= 2)
    {
        const int dam = get_form()->get_special_damage().roll();
        string msg = make_stringf(" shudders as magic cascades through %s%s",
                                  m.pronoun(PRONOUN_OBJECTIVE).c_str(),
                                  attack_strength_punctuation(dam).c_str());
        simple_monster_message(m, msg.c_str());
        if (dam)
        {
            m.hurt(&you, dam, BEAM_MMISSILE, KILLED_BY_BEAM /*eh*/);
            if (!m.alive())
                return;
        }
        m.malmutate(&you);
        m.del_ench(ENCH_CONTAM, true);
        return;
    }

    m.add_ench(mon_enchant(ENCH_CONTAM, &you, 0, 1));
    if (!old_glow.degree)
        simple_monster_message(m, " begins to glow.");
    else
        simple_monster_message(m, " glows dangerously bright.");

    // Deduct player energy for the attack and maybe untransform.
    int& energy = you.props[FLUX_ENERGY_KEY].get_int();
    const bool above_warning = energy > FLUX_ENERGY_WARNING;
    energy -= random_range(1, 2);

    if (energy <= 0)
    {
        mprf(MSGCH_DURATION, "The last of your unstable energy dissipates and "
                             "you return to your previous form.");
        return_to_default_form();
    }
    else if (above_warning && energy < FLUX_ENERGY_WARNING)
        mprf(MSGCH_DURATION, "You feel the transmutational energy in your body is nearly expended.");
}

void melee_attack::maybe_do_mesmerism()
{
    // Check if the effect is valid.
    if (!defender->wearing_ego(OBJ_ARMOUR, SPARM_MESMERISM))
        return;

    if (defender->is_player() && you.duration[DUR_MESMERISM_COOLDOWN])
        return;
    else if (defender->is_monster() && defender->as_monster()->has_ench(ENCH_ORB_COOLDOWN))
        return;

    const int radius = defender->is_player() ? mesmerism_orb_radius() : 3;
    const int max_dur = defender->is_player() ? 5 + you.skill_rdiv(SK_EVOCATIONS, 1, 5)
                                              : 3 + defender->get_hit_dice() / 4;

    mprf("%s orb emits a pulse of dizzying energy.", defender->name(DESC_ITS).c_str());
    draw_ring_animation(defender->pos(), radius, LIGHTMAGENTA, MAGENTA, true, 30);

    for (radius_iterator ri(defender->pos(), radius, C_SQUARE, LOS_NO_TRANS); ri; ++ri)
    {
        if (actor* act = actor_at(*ri))
        {
            if (!act->is_firewood() && !act->clarity()
                && act->willpower() != WILL_INVULN
                && could_harm_enemy(act, defender, true))
            {
                act->daze(random_range(3, max_dur));
            }
        }
    }

    if (defender->is_player())
        you.duration[DUR_MESMERISM_COOLDOWN] = random_range(150, 200) - you.skill(SK_EVOCATIONS, 2);
    else
        defender->as_monster()->add_ench(mon_enchant(ENCH_ORB_COOLDOWN, defender, random_range(120, 200)));
}

static void _grow_mushrooms(const monster& mon)
{
    // Can't extract position from a reset monster (which may have happened due
    // to disto banishment).
    if (mon.type == MONS_NO_MONSTER || mon.is_firewood() || mon.wont_attack())
        return;

    vector<coord_def> spots = get_wall_ring_spots(mon.pos(),
                                                  mon.pos() + (mon.pos() - you.pos()),
                                                  3);

    mgen_data mgen = mgen_data(MONS_BURSTSHROOM, BEH_FRIENDLY, coord_def(),
                               MHITNOT, MG_FORCE_PLACE);
    mgen.hd = get_form()->get_level(1) / 2;
    mgen.set_summoned(&you, MON_SUMM_SPORE, 100, false);

    bool created = false;
    for (coord_def spot : spots)
    {
        if (actor_at(spot))
            continue;

        mgen.pos = spot;
        if (monster* shroom = create_monster(mgen))
        {
            // Randomize detonation time a little.
            shroom->number = random_range(3, 5);
            created = true;
        }
    }

    if (created)
        mprf("Mushrooms sprout behind %s.", mon.name(DESC_THE).c_str());
}

/* An attack has been determined to have hit something
 *
 * Handles to-hit effects for both attackers and defenders,
 * Determines damage and passes off execution to handle_phase_damaged
 * Also applies weapon brands
 *
 * Returns true if combat should continue, false if it should end here.
 */
bool melee_attack::handle_phase_hit()
{
    did_hit = true;
    perceived_attack = true;
    bool hit_woke_orc = false;

    if (attacker->is_player())
    {
        if (crawl_state.game_is_hints())
            Hints.hints_melee_counter++;

        // TODO: Remove this (placed here so I can get rid of player_attack)
        if (have_passive(passive_t::convert_orcs)
            && mons_genus(defender->mons_species()) == MONS_ORC
            && !defender->is_summoned()
            && !defender->as_monster()->is_shapeshifter()
            && you.see_cell(defender->pos()) && defender->asleep())
        {
            hit_woke_orc = true;
        }
    }

    damage_done = 0;
    // Slimify does no damage and serves as an on-hit effect, handle it
    if (attacker->is_player() && you.duration[DUR_SLIMIFY]
        && mon_can_be_slimified(defender->as_monster())
        && !cleaving)
    {
        // Bail out after sliming so we don't get aux unarmed and
        // attack a fellow slime.
        slimify_monster(defender->as_monster());
        you.duration[DUR_SLIMIFY] = 0;

        return false;
    }

    // Detonation catalyst should trigger even if the defender dies later on.
    maybe_trigger_detonation();

    // This does more than just calculate the damage, it also sets up
    // messages, etc. It also wakes nearby creatures on a failed stab,
    // meaning it could have made the attacked creature vanish. That
    // will be checked early in player_monattack_hit_effects
    damage_done += calc_damage();

    // Calculate and apply infusion costs immediately after we calculate
    // damage, so that later events don't result in us skipping the cost.
    if (attacker->is_player())
    {
        const int infusion = you.infusion_amount();
        if (infusion)
        {
            pay_mp(infusion);
            finalize_mp_cost();
        }
    }

    bool stop_hit = false;
    // Check if some hit-effect killed the monster.
    if (attacker->is_player())
        stop_hit = !player_monattk_hit_effects();

    // check_unrand_effects is safe to call with a dead defender, so always
    // call it, even if the hit effects said to stop.
    if (stop_hit)
    {
        check_unrand_effects();
        return false;
    }

    // Randomizing here instead of in mons_attack_spec so that the reaching
    // works properly.
    if (attk_flavour == AF_REACH_CLEAVE_UGLY)
    {
        attack_flavour flavours[] =
            {AF_FIRE, AF_COLD, AF_ELEC, AF_POISON, AF_ACID, AF_ANTIMAGIC};
        attk_flavour = RANDOM_ELEMENT(flavours);
    }

    if (damage_done > 0 || flavour_triggers_damageless(attk_flavour))
    {
        if (!handle_phase_damaged())
            return false;

        // TODO: Remove this, (placed here to remove player_attack)
        if (attacker->is_player() && hit_woke_orc)
        {
            // Call function of orcs first noticing you, but with
            // beaten-up conversion messages (if applicable).
            beogh_follower_convert(defender->as_monster(), true);
        }
    }
    else
    {
        if (needs_message)
        {
            attack_verb = attacker->is_player()
                                    ? attack_verb
                                    : attacker->conj_verb(mons_attack_verb());

            // TODO: Clean this up if possible, checking atype for do / does is ugly
            mprf("%s %s %s but %s no damage.",
                attacker->name(DESC_THE).c_str(),
                attack_verb.c_str(),
                defender_name(true).c_str(),
                attacker->is_player() ? "do" : "does");
        }
    }

    maybe_trigger_jinxbite();

    // Check for weapon brand & inflict that damage too
    apply_damage_brand();

    if (weapon && damage_brand == SPWPN_VALOUR)
        do_valour_beam();

    if (weapon && damage_brand == SPWPN_CONCUSSION)
        handle_concussion_brand();

    if (weapon && testbits(weapon->flags, ISFLAG_CHAOTIC)
        && defender->alive())
    {
        unwind_var<brand_type> save_brand(damage_brand);
        damage_brand = SPWPN_CHAOS;
        apply_damage_brand();
    }

    // Apply flux form's sfx.
    if (attacker->is_player() && you.form == transformation::flux
        && defender->alive() && defender->is_monster())
    {
        _apply_flux_contam(*(defender->as_monster()));
    }

    if (attacker->is_player() && you.form == transformation::spider
        && defender->alive() && x_chance_in_y(get_form()->get_web_chance(), 100))
    {
        defender->trap_in_web();
    }

    if (attacker->is_player() && you.form == transformation::sphinx && defender->alive())
    {
        const int spaces = min(7, airstrike_space_around(defender->pos(), true));
        const int dmg = player_airstrike_melee_damage(spaces).roll();
        special_damage = defender->apply_ac(dmg, 0);

        if (needs_message && special_damage)
        {
            tileidx_t dummy;
            mprf("%s and strikes %s%s",
                 airstrike_intensity_display(spaces, dummy).c_str(),
                 defender->name(DESC_THE).c_str(),
                 attack_strength_punctuation(special_damage).c_str());
        }

        inflict_damage(special_damage);
    }

    if (attacker->is_player() && you.form == transformation::walking_scroll
        && !defender->is_firewood() && coinflip())
    {
        inc_mp(1);
    }

    if (attacker->is_player() && you.form == transformation::eel_hands && coinflip())
        do_eel_melee_jolt(defender->pos());

    if (attacker->is_player() && you.form == transformation::spore)
    {
        _grow_mushrooms(*defender->as_monster());

        if (defender->alive() && !defender->is_unbreathing()
            && mons_has_attacks(*defender->as_monster(), false)
            && coinflip())
        {
            mprf("%s is engulfed in spores.", defender->name(DESC_THE).c_str());
            defender->weaken(&you, 3);
        }
    }

    // Fireworks when using Serpent's Lash to kill.
    if (!defender->alive()
        && defender->has_blood()
        && wu_jian_has_momentum(wu_jian_attack))
    {
        blood_spray(defender->pos(), defender->type, damage_done / 5);
        if (defender->is_monster())
            defender->as_monster()->flags |= MF_EXPLODE_KILL;
    }

    // Trigger Curse of Agony after most normal damage is already applied
    if (attacker->is_player() && defender->alive() && defender->is_monster()
        && defender->as_monster()->has_ench(ENCH_CURSE_OF_AGONY))
    {
        mon_enchant agony = defender->as_monster()->get_ench(ENCH_CURSE_OF_AGONY);
        torment_actor(defender, &you, TORMENT_AGONY);
        agony.degree -= 1;

        if (agony.degree == 0)
            defender->as_monster()->del_ench(ENCH_CURSE_OF_AGONY);
        else
            defender->as_monster()->update_ench(agony);
    }

    // Must happen before unrand effects, since we need to call this even (or
    // especially!) if the monster died.
    if (damage_done > 0)
        do_vampire_lifesteal();

    if (check_unrand_effects())
        return false;

    if (damage_done > 0 || special_damage > 0)
        apply_sign_of_ruin_effects();

    if (damage_done > 0)
    {
        apply_black_mark_effects();
        try_parry_disarm();
        maybe_do_mesmerism();
    }
    do_ooze_flood();

    if (attacker->is_player())
    {
        // Always upset monster regardless of damage.
        // However, successful stabs inhibit shouting.
        behaviour_event(defender->as_monster(), ME_WHACK, attacker,
                        coord_def(), !stab_attempt);

        // [ds] Monster may disappear after behaviour event.
        if (!defender->alive())
            return true;
    }
    else if (defender->is_player())
    {
        // These effects (mutations right now) are only triggered when
        // the player is hit, each of them will verify their own required
        // parameters.
        do_passive_freeze();
        do_fiery_armour_burn();
        do_foul_flame();
        emit_foul_stench();
    }

    return true;
}

static void _inflict_deathly_blight(monster &m)
{
    if (m.holiness() & MH_NONLIVING || m.is_peripheral())
        return;

    const int dur = random_range(3, 6) * BASELINE_DELAY;
    bool worked = false;
    if (!m.stasis())
        worked = m.add_ench(mon_enchant(ENCH_SLOW, &you, dur)) || worked;
    if (mons_has_attacks(m))
        worked = m.add_ench(mon_enchant(ENCH_WEAK, &you, dur)) || worked;
    if (m.holiness() & (MH_NATURAL | MH_PLANT))
        worked = m.add_ench(mon_enchant(ENCH_DRAINED, &you, dur, 2)) || worked;
    if (worked && you.can_see(m))
        simple_monster_message(m, " decays.");
}

bool melee_attack::handle_phase_damaged()
{
    if (defender->is_player() && you.get_mutation_level(MUT_SLIME_SHROUD)
        && !you.duration[DUR_SHROUD_TIMEOUT] && one_chance_in(4))
    {
        you.duration[DUR_SHROUD_TIMEOUT] = 100 + random2(damage_done) * 10;
        mprf("your slimy shroud breaks as it bends %s attack away%s",
                     atk_name(DESC_ITS).c_str(),
                     attack_strength_punctuation(damage_done).c_str());
        did_hit = false;
        damage_done = 0;
        return false;
    }

    if (!attack::handle_phase_damaged())
        return false;

    if (attacker->is_player())
    {
        if (you.unrand_equipped(UNRAND_POWER_GLOVES))
            inc_mp(div_rand_round(damage_done, 8));
        if (you.form == transformation::death && defender->alive()
            && defender->is_monster())
        {
            _inflict_deathly_blight(*(defender->as_monster()));
        }
        if (you.form == transformation::sun_scarab && defender->alive() && coinflip())
            sear_defender();
    }

    return true;
}

bool melee_attack::handle_phase_aux()
{
    if (attacker->is_player()
        && !cleaving && !attack_number
        && wu_jian_attack != WU_JIAN_ATTACK_TRIGGERED_AUX
        && !is_projected)
    {
        // returns whether an aux attack successfully took place
        // additional attacks from cleave don't get aux
        const int aux_dist = you.form == transformation::aqua ? 3 : 1;
        if (!defender->as_monster()->friendly()
            && grid_distance(defender->pos(), attack_position) <= aux_dist)
        {
            player_do_aux_attacks();
        }

        // Don't print wounds after the first attack with quick blades.
        if (!weapon_multihits(weapon))
            print_wounds(*defender->as_monster());
    }

    return true;
}

/**
 * Devour a monster whole!
 *
 * @param defender  The monster in question.
 */
static void _devour(monster &victim)
{
    // Sometimes, one's eyes are larger than one's stomach-mouth.
    const int size_delta = victim.body_size(PSIZE_BODY)
                            - you.body_size(PSIZE_BODY);
    mprf("You devour %s%s!",
         size_delta <= 0 ? "" :
         size_delta <= 1 ? "half of " :
                           "a chunk of ",
         victim.name(DESC_THE).c_str());

    // give a clearer message for eating invisible things
    if (!you.can_see(victim))
    {
        mprf("It tastes like %s.",
             mons_type_name(mons_genus(victim.type), DESC_PLAIN).c_str());
        // this could be the actual creature name, but it feels more
        // 'flavourful' this way??
    }
    if (victim.has_ench(ENCH_STICKY_FLAME))
        mpr("Spicy!");

    // Devour the corpse.
    victim.props[NEVER_CORPSE_KEY] = true;

    // ... but still drop dragon scales, etc, if appropriate.
    monster_type orig = victim.type;
    if (victim.props.exists(ORIGINAL_TYPE_KEY))
        orig = (monster_type) victim.props[ORIGINAL_TYPE_KEY].get_int();
    maybe_drop_monster_organ(victim.type, orig, victim.pos());

    you.duration[DUR_ENGORGED] += 10 + random_range(victim.get_experience_level() * 10 / 3,
                                                    victim.get_experience_level() * 20 / 3);
    if (you.duration[DUR_ENGORGED] > 400)
        you.duration[DUR_ENGORGED] = 400;
}


/**
 * Possibly devour the defender whole.
 *
 * @param defender  The defender in question.
 */
static void _consider_devouring(monster &defender)
{
    ASSERT(!crawl_state.game_is_arena());

    // Don't eat orcs, even heretics might be worth a miracle
    if (you_worship(GOD_BEOGH)
        && mons_genus(mons_species(defender.type)) == MONS_ORC)
    {
        return;
    }

    // can't eat enemies that leave no corpses...
    if (!maw_considers_appetising(defender)
        // the curse of midas...
        || have_passive(passive_t::goldify_corpses))
    {
        return;
    }

    // shapeshifters are mutagenic.
    // there's no real point to this now, but it is funny.
    // should polymorphed enemies do the same?
    if (defender.is_shapeshifter())
    {
        // handle this carefully, so the player knows what's going on
        mprf("You spit out %s as %s %s & %s in your maw!",
             defender.name(DESC_THE).c_str(),
             defender.pronoun(PRONOUN_SUBJECTIVE).c_str(),
             conjugate_verb("twist", defender.pronoun_plurality()).c_str(),
             conjugate_verb("change", defender.pronoun_plurality()).c_str());
        return;
    }


    // chow down.
    _devour(defender);
}

static void _handle_werewolf_kill_bonus(const monster& victim, bool takedown)
{
    if (victim.is_summoned())
        return;

    const int old_power = you.duration[DUR_WEREFURY] ? you.props[WEREFURY_KEY].get_int() : 0;
    int& power = you.props[WEREFURY_KEY].get_int();
    if (!you.duration[DUR_WEREFURY])
    {
        mpr("You revel in your kill!");
        power = div_rand_round(get_form()->get_werefury_kill_bonus() * 3 / 2, 10);
    }
    else
        power += div_rand_round(get_form()->get_werefury_kill_bonus(), 10);

    // Cap slaying bonus
    if (power > 12)
        power = 12;

    // Howl whenever we cross the 5 slaying threshold, with a smaller chance after that.
    // (Bestial takedown kills always make you howl.)
    if (takedown || (power >= 5 && (old_power < 5 || one_chance_in(4))))
    {
        if (you.is_silenced())
        {
            mpr("You raise your head to howl, but no sound comes out.");
            return;
        }

        const int howl_power = get_form()->get_howl_power();
        mpr("You let out a blood-chilling howl!");
        draw_ring_animation(you.pos(), you.current_vision, DARKGRAY, 0, true, 10);
        for (monster_near_iterator mi(you.pos()); mi; ++mi)
        {
            if (could_harm_enemy(&you, *mi, true)
                && mi->can_feel_fear(true) && !mi->has_ench(ENCH_FEAR)
                && mi->check_willpower(&you, howl_power) <= 0)
            {
                mprf("%s freezes in fear!", mi->name(DESC_THE).c_str());
                int dur = random_range(40, 70);
                mi->add_ench(mon_enchant(ENCH_FEAR, &you, dur));
                mi->add_ench(mon_enchant(ENCH_BOUND, &you, dur));
                mi->props[FROZEN_IN_FEAR_KEY] = true;
                behaviour_event(*mi, ME_SCARE, &you);
            }
        }
        noisy(you.shout_volume(), you.pos(), MID_PLAYER);
    }

    you.increase_duration(DUR_WEREFURY, random_range(7, 11), 20);
}

/**
 * Handle effects that fire when the defender (the target of the attack) is
 * killed.
 */
void melee_attack::handle_phase_killed()
{
    if (attacker->is_player()
        && you.form == transformation::maw
        && defender->is_monster() // better safe than sorry
        && defender->type != MONS_NO_MONSTER // already reset
        && adjacent(defender->pos(), attack_position))
    {
        _consider_devouring(*defender->as_monster());
    }

    // Wyrmbane needs to be notified of deaths, including ones due to aux
    // attacks, but other users of melee_effects() don't want to possibly
    // be called twice. Adding another entry for a single artefact would
    // be overkill, so here we call it by hand. check_unrand_effects()
    // avoided triggering Wyrmbane's death effect earlier in the attack.
    if (unrand_entry && weapon && weapon->unrand_idx == UNRAND_WYRMBANE)
    {
        unrand_entry->melee_effects(mutable_wpn, attacker, defender,
                                    special_damage, nullptr);
    }

    // We test this *before* the monster dies, but only trigger afterward,
    // for better message order.
    const bool execute = attacker->is_player() && defender->is_monster()
                            && you.has_mutation(MUT_MAKHLEB_MARK_EXECUTION)
                            && !you.duration[DUR_EXECUTION]
                            && !defender->is_firewood()
                            && defender->real_attitude() != ATT_FRIENDLY
                            && one_chance_in(5)
    // It's unsatisfying to repeatedly trigger a transformation on the final
    // monster of a group, so let's not cause the player that disappointment.
                            && there_are_monsters_nearby(true, true, false);

    attack::handle_phase_killed();

    if (execute)
        makhleb_execution_activate();

    if (attacker->is_player() && you.form == transformation::werewolf
        && defender->is_monster())
    {
        _handle_werewolf_kill_bonus(*defender->as_monster(), is_bestial_takedown);
    }
}

void melee_attack::handle_spectral_brand()
{
    if (attacker->type == MONS_SPECTRAL_WEAPON || !defender->alive())
        return;
    attacker->triggered_spectral = true;
    schedule_spectral_weapon_fineff(*attacker, *defender, mutable_wpn);
}

item_def *melee_attack::primary_weapon() const
{
    item_def *weap = attacker->weapon(0);
    if (!weap || is_range_weapon(*weap))
        return nullptr;
    return weap;
}

item_def *melee_attack::offhand_weapon() const
{
    item_def *offhand = attacker->offhand_weapon();
    if (!offhand || is_range_weapon(*offhand))
        return nullptr;
    return offhand;
}

// Returns true if any cleave attack succeeded.
bool melee_attack::handle_phase_cleaving()
{
    if (!cleave_targets.empty() && !simu)
        return do_followup_attacks(cleave_targets, true);

    return false;
}

// Returns true if any followup attack succeeded.
bool melee_attack::handle_phase_multihit()
{
    if (!is_followup && weapon_multihits(weapon) && defender && defender->alive())
    {
        const int hits_per_targ = weapon_hits_per_swing(*weapon);
        list<actor*> extra_hits;
        for (int i = 1; i < hits_per_targ; i++)
            extra_hits.push_back(defender);
        // effective_attack_number will be wrong for a monster that
        // does a cleaving multi-hit attack. God help us.
        const bool success = do_followup_attacks(extra_hits, false);
        if (success && attacker->is_player())
            print_wounds(*defender->as_monster());
        return success;
    }

    return false;
}

void melee_attack::handle_phase_end()
{
    handle_phase_multihit();
    handle_phase_cleaving();

    // Check for passive mutation effects.
    if (defender->is_player() && defender->alive() && attacker != defender)
    {
        mons_do_eyeball_confusion();
        mons_do_tendril_disarm();
    }

    if (attacker->is_player() && you.form == transformation::rime_yak
        && defender && !defender->is_firewood() && !mons_aligned(&you, defender))
    {
        _do_rime_yak_freeze(defender->pos());
    }

    if (attacker->alive() && attacker->is_monster())
    {
        monster* mon_attacker = attacker->as_monster();

        if (mon_attacker->has_ench(ENCH_ROLLING))
            mon_attacker->del_ench(ENCH_ROLLING);

        // Blazeheart golems tear themselves apart on impact
        if (mon_attacker->type == MONS_BLAZEHEART_GOLEM && did_hit)
        {
            mon_attacker->hurt(mon_attacker,
                               mon_attacker->max_hit_points / 3 + 1,
                               BEAM_MISSILE);
        }
        else if (mon_attacker->type == MONS_CLOCKWORK_BEE && did_hit)
        {
            if (--mon_attacker->number == 0)
                clockwork_bee_go_dormant(*mon_attacker);
        }
    }

    if (defender && !is_followup)
    {
        if (damage_brand == SPWPN_SPECTRAL)
            handle_spectral_brand();
        // Use the Nessos hack to give the player glaive of the guard spectral too
        if (weapon && is_unrandom_artefact(*weapon, UNRAND_GUARD))
            handle_spectral_brand();
    }

    // Give our rending blade one trigger per hit we land.
    if (did_hit && attacker->is_player() && you.props.exists(RENDING_BLADE_MP_KEY))
        trigger_rending_blade();

    // Dead but not yet reset, most likely due to an attack flavour that
    // destroys the attacker on-hit.
    if (attacker->is_monster()
        && attacker->as_monster()->type != MONS_NO_MONSTER
        && attacker->as_monster()->hit_points < 1)
    {
        monster_die(*attacker->as_monster(), KILL_NON_ACTOR, NON_MONSTER);
    }

    attack::handle_phase_end();
}

// Copy over initial melee-specific attack parameters (ie: things that must be
// defined before attack() or launch_attack_set() are called). Things calculated
// after this point should not be copied.
void melee_attack::copy_params_to(melee_attack &other) const
{
    other.cleaving              = cleaving;
    other.is_followup           = is_followup;
    other.is_riposte            = is_riposte;
    other.is_projected          = is_projected;
    other.is_bestial_takedown   = is_bestial_takedown;
    other.is_sunder             = is_sunder;
    other.charge_pow            = charge_pow;
    other.never_cleave          = never_cleave;
    other.is_involuntary        = is_involuntary;
    other.wu_jian_attack        = wu_jian_attack;
    other.wu_jian_number_of_targets = wu_jian_number_of_targets;

    attack::copy_params_to(other);
}

// Perform followup attacks (from cleaving or quick blades).
// Returns true if at least one of these attacks succeeded.
bool melee_attack::do_followup_attacks(list<actor*>& targets, bool is_cleaving)
{
    int new_effective_attack_number = effective_attack_number + 1;
    bool success = false;
    while (attacker->alive() && !targets.empty())
    {
        actor* def = targets.front();

        if (def && def->alive() && should_cleave_into(*attacker, *def))
        {
            melee_attack followup(attacker, def, attack_number,
                                  ++new_effective_attack_number);
            followup.set_weapon(mutable_wpn);

            copy_params_to(followup);
            followup.cleaving = is_cleaving;
            followup.is_followup = !is_cleaving;

            success |= followup.attack();
            total_damage_done += followup.total_damage_done;
        }
        targets.pop_front();
    }

    return success;
}

void melee_attack::set_weapon(item_def *wpn)
{
    weapon = mutable_wpn = wpn;
    if (const monster* mons = attacker->as_monster())
    {
        damage_brand = mons->damage_brand(attack_number);
        damage_type = mons->damage_type(attack_number);
    }
    else
    {
        damage_brand = you.damage_brand(wpn);
        damage_type = you.damage_type(wpn);
    }

    init_attack(attack_number);
    if (weapon && !using_weapon())
        wpn_skill = SK_FIGHTING;
}

// Perform a player attack with a specific weapon.
bool melee_attack::swing_with(item_def &wpn)
{
    const bool reaching = weapon_reach(wpn) > 1
                            || you.form == transformation::aqua;
    if (!is_projected
        && !reaching
        && defender     // Attacks without a defender are empty cleaves. The
                        // initial attack will do nothing, but may set up
                        // followup attacks to be handled normally.
        && !adjacent(attacker->pos(), defender->pos()))
    {
        return false;
    }

    melee_attack swing(attacker, defender,
                       attack_number,
                       effective_attack_number);
    copy_params_to(swing);
    swing.set_weapon(&wpn);
    bool success = swing.attack();
    is_sunder |= swing.is_sunder;
    cancel_attack = swing.cancel_attack;
    is_attacking_hostiles = is_attacking_hostiles || swing.is_attacking_hostiles;
    return success;
}

/**
 * Launches a set of melee attacks. If the player is using two weapons, this
 * launches attacks with the primary and off-hand weapon in a random order.
 *
 * @param skip_player_post_attack  If true, will not cause any post-attack
 *                                 effects such as extended durations or
 *                                 triggering shadow mimic.
 *
 * @return True iff either sub-attack succeeded (ie: wasn't cancelled or fumbled).
 */
bool melee_attack::launch_attack_set(bool skip_player_post_attack)
{
    if (!attacker->is_player())
        return run_monster_attack_set();

    bool success = run_player_attack_set();

    if (!skip_player_post_attack)
    {
        // Fumbled attacks still extend durations, but do not otherwise trigger
        // post-attack effects. Attacks only against firewood or friendlies do
        // neither.
        player_attempted_attack(success && is_attacking_hostiles,
                                is_attacking_hostiles, defender);
    }

    return success;
}

bool melee_attack::run_monster_attack_set()
{
    ASSERT(attacker->is_monster());
    monster* mon = attacker->as_monster();

    const int nrounds = attacker->as_monster()->has_hydra_multi_attack()
        ? attacker->heads() + (attacker->type == MONS_DRAUGR)
        : MAX_NUM_ATTACKS;
    coord_def pos = defender->pos();

    // Melee combat, tell attacker to wield its melee weapon.
    mon->wield_melee_weapon();

    bool was_hostile = !mons_aligned(attacker, defender);

    bool success = false;
    int effective_attack_num = 0;
    int attack_num;
    bool charge_sunder = false;
    for (attack_num = 0; attack_num < nrounds && attacker->alive();
         ++attack_num, ++effective_attack_num)
    {
        if (!attacker->alive())
            return false;

        // Monster went away or become friendly?
        if (!defender->alive()
            || defender->is_banished()
            // We rely on the attack itself to verify that the target is still
            // in range, but hydras want to know earlier that it will fail so
            // they can pick a new target.
            || attacker->as_monster()->has_hydra_multi_attack()
               && defender->pos() != pos && !adjacent(defender->pos(), attacker->pos())
            || was_hostile && mons_aligned(attacker, defender)
               && !mons_is_confused(*attacker->as_monster())
               && !attacker->as_monster()->has_ench(ENCH_FRENZIED))
        {
            if (attacker == defender
               || !attacker->as_monster()->has_hydra_multi_attack())
            {
                break;
            }

            // Hydras can try and pick up a new monster to attack to
            // finish out their round. -cao
            bool end = true;
            for (adjacent_iterator i(attacker->pos()); i; ++i)
            {
                if (*i == you.pos()
                    && !mons_aligned(attacker, &you)
                    && you.alive())
                {
                    attacker->as_monster()->foe = MHITYOU;
                    attacker->as_monster()->target = you.pos();
                    defender = &you;
                    was_hostile = true;
                    end = false;
                    break;
                }

                monster* mons = monster_at(*i);
                if (mons && !mons_aligned(attacker, mons))
                {
                    defender = mons;
                    was_hostile = true;
                    end = false;
                    pos = mons->pos();
                    break;
                }
            }

            // No adjacent hostiles.
            if (end)
                break;
        }

        melee_attack melee_attk(attacker, defender, attack_num,
                                effective_attack_num);

        copy_params_to(melee_attk);

        const bool attack_succeeded = melee_attk.attack();

        success |= attack_succeeded;
        did_hit |= melee_attk.did_hit;
        is_sunder |= melee_attk.is_sunder;

        // Charge up for monsters (player is handled in player_attempted_attack)
        if (melee_attk.is_sundering_weapon() && !is_projected && attack_succeeded)
            charge_sunder = true;

        if (!attack_succeeded)
            --effective_attack_num;

        fire_final_effects();
    }

    // If this attack was supposed to be instant, refund the energy now.
    if (mon->has_ench(ENCH_INSTANT_CLEAVE))
    {
        mon->del_ench(ENCH_INSTANT_CLEAVE);
        mon->speed_increment += mon->action_energy(EUT_ATTACK);
    }

    if (charge_sunder)
        mon->add_ench(mon_enchant(ENCH_SUNDER_CHARGE, attacker, random_range(30, 50), 1));

    return success;
}

bool melee_attack::run_player_attack_set()
{
    item_def *primary = you.weapon();
    item_def *offhand = you.offhand_weapon();
    if (!primary || !offhand)
    {
        // Don't launch UC attacks when you have an offhand weapon.
        if (offhand)
            set_weapon(offhand);
        return attack();
    }

    ASSERT(!attack_number);

    item_def *first_weapon = primary;
    item_def *second_weapon = offhand;
    if (coinflip())
    {
        first_weapon = offhand;
        second_weapon = primary;
    }

    bool success = swing_with(*first_weapon);
    if (cancel_attack)
        return success;

    ++attack_number;
    ++effective_attack_number;

    // If we had a primary target that became ineligable after the first swing,
    // give the next swing an empty target (so we can still cleave with it).
    if (defender && (!defender->alive() || !should_cleave_into(*attacker, *defender)))
        defender = nullptr;

    if (swing_with(*second_weapon))
        success = true;
    ASSERT(!cancel_attack);
    return success;
}

bool melee_attack::did_attack_hostiles() const
{
    return is_attacking_hostiles;
}

/* Initiate the processing of the attack
 *
 * Called from the main code (fight.cc), this method begins the actual combat
 * for a particular attack and is responsible for looping through each of the
 * appropriate phases (which then can call other related phases within
 * themselves).
 *
 * Note that this does *not* trigger offhand attacks, but *can* trigger other
 * derived attacks, e.g. cleaving or quick-blade multiswings.
 *
 * Returns whether combat was completely successful
 *      If combat was not successful, it could be any number of reasons, like
 *      the defender or attacker dying during the attack? or a defender moving
 *      from its starting position.
 */
bool melee_attack::attack()
{
    cleave_setup();

    if (!is_involuntary && bad_attempt())
    {
        cancel_attack = true;
        return false;
    }

    if (!cleaving)
    {
        // Check that we're attacking at least one non-firewood target and
        // cache this result.
        if (defender && !defender->is_firewood() && !mons_aligned(attacker, defender))
            is_attacking_hostiles = true;

        if (!is_attacking_hostiles && !cleave_targets.empty())
        {
            for (actor* targ : cleave_targets)
            {
                if (!targ->is_firewood() && !mons_aligned(attacker, targ))
                {
                    is_attacking_hostiles = true;
                    break;
                }
            }
        }

        if (attacker->is_player()
            && player_under_penance(GOD_ELYVILON)
            && god_hates_your_god(GOD_ELYVILON)
            && !is_followup
            && one_chance_in(20))
        {
            simple_god_message(" prevents your attack.", false, GOD_ELYVILON);
            return false;
        }

        // Wall jump attacks supposedly happen 'mid-air' and so shouldn't care
        // about water at the landing spot.
        if (wu_jian_attack != WU_JIAN_ATTACK_WALL_JUMP
            && attacker->fumbles_attack())
        {
            // Xom thinks fumbles are funny...
            // ... and thinks fumbling when trying to hit yourself is just
            // hilarious.
            xom_is_stimulated(attacker == defender ? 200 : 10);

            // We may try for additional quick blade attacks, even if we fumble
            // the first.
            return handle_phase_multihit();
        }

        if (!handle_phase_attempted())
            return false;
    }

    // Forced cleaves against an empty space have no defender, and so should do
    // nothing else on the main attack, but may procede to cleave other monsters.
    if (!defender)
        return handle_phase_cleaving();

    // Any attack against a monster we're afraid of has a chance to fail
    // (but we may cleave to other targets anyway.)
    if (attacker->is_player() && defender &&
        you.afraid_of(defender->as_monster()) && one_chance_in(3))
    {
        mprf("You attempt to attack %s, but flinch away in fear!",
             defender->name(DESC_THE).c_str());

        bool success = false;
        success |= handle_phase_multihit();
        success |= handle_phase_cleaving();
        return success;
    }

    if (attacker != defender && attacker->is_monster()
        && mons_self_destructs(*attacker->as_monster()))
    {
        attacker->self_destruct();
        return did_hit = perceived_attack = true;
    }

    string saved_gyre_name;
    if (weapon && is_unrandom_artefact(*weapon, UNRAND_GYRE))
    {
        saved_gyre_name = get_artefact_name(*weapon);
        const bool gimble = effective_attack_number % 2;
        set_artefact_name(*mutable_wpn, gimble ? "quick blade \"Gimble\""
                                                  : "quick blade \"Gyre\"");
    }

    // Restore gyre's name before we return. We cannot use an unwind_var here
    // because the precise address of the ARTEFACT_NAME_KEY property might
    // change, for example if a summoned item is reset.
    ON_UNWIND
    {
        if (!saved_gyre_name.empty() && weapon
                && is_unrandom_artefact(*weapon, UNRAND_GYRE))
        {
            set_artefact_name(*mutable_wpn, saved_gyre_name);
        }
    };

    // Attacker might have died from effects of cleaving handled prior to this
    if (!attacker->alive())
        return false;

    // We might have killed the kraken target by cleaving a tentacle.
    if (!defender->alive())
    {
        handle_phase_killed();
        handle_phase_end();
        return true;
    }

    // Abort early if the target is completely immune (possibly printing a message).
    // This takes no time if the attacker was the player and they should have known
    // in advance that this attack was hopeless.
    if (!could_harm(attacker, defender, attacker->is_player(), attacker->is_player()))
    {
        cancel_attack = attacker->is_player() && !(you.confused() || !you.can_see(*defender));
        return false;
    }

    // Now that we finally know that this swing is really happening, count it.
    if (attacker->is_player())
    {
        if (weapon)
        {
            if (weapon->base_type == OBJ_WEAPONS)
                if (is_unrandom_artefact(*weapon)
                    && get_unrand_entry(weapon->unrand_idx)->type_name)
                {
                    count_action(CACT_MELEE, weapon->unrand_idx);
                }
                else
                    count_action(CACT_MELEE, weapon->sub_type);
            else if (weapon->base_type == OBJ_STAVES)
                count_action(CACT_MELEE, WPN_STAFF);
        }
        else
            count_action(CACT_MELEE, -1, -1); // unarmed subtype/auxtype
    }

    if (is_sunder && !cleaving && is_sundering_weapon() && you.can_see(*attacker))
    {
        mprf("%s %s flashes viciously!", attacker->name(DESC_ITS).c_str(),
                                         weapon->name(DESC_PLAIN).c_str());
        draw_ring_animation(attacker->pos(), 2, WHITE, WHITE, true, 25, TILE_BOLT_SUNDERING);
    }

    // Apparently I'm insane for believing that we can still stay general past
    // this point in the combat code, mebe I am! --Cryptic

    // Calculate various ev values and begin to check them to determine the
    // correct handle_phase_ handler.
    const int ev = defender->evasion(false, attacker);
    ev_margin = test_hit(to_hit, ev, !attacker->is_player());
    bool shield_blocked = attack_shield_blocked();

    // Stuff for god conduct, this has to remain here for scope reasons.
    god_conduct_trigger conducts[3];

    if (attacker->is_player() && attacker != defender)
    {
        set_attack_conducts(conducts, *defender->as_monster(),
                            you.can_see(*defender) && !is_involuntary);

        // Check for stab (and set stab_attempt and stab_bonus)
        player_stab_check();
        // Make sure we hit if we passed the stab check.
        if (stab_attempt && stab_bonus > 0)
        {
            ev_margin = AUTOMATIC_HIT;
            shield_blocked = false;
        }

        // Serpent's Lash does not miss
        if (wu_jian_has_momentum(wu_jian_attack))
           ev_margin = AUTOMATIC_HIT;
    }

    if (shield_blocked)
    {
        handle_phase_blocked();
        maybe_riposte();
        if (!attacker->alive())
        {
            handle_phase_end();
            return true;
        }
    }
    else
    {
        if (attacker != defender
            && (adjacent(defender->pos(), attack_position))
            && !is_riposte)
        {
            // Check for defender Spines
            do_spines();

            // Spines can kill! With Usk's pain bond, they can even kill the
            // defender.
            if (!attacker->alive() || !defender->alive())
                return true;
        }

        if (ev_margin >= 0)
        {
            if (!paragon_defends_player())
            {
                bool cont = handle_phase_hit();

                if (cont)
                    attacker_sustain_passive_damage();
                else
                {
                    if (!defender->alive())
                        handle_phase_killed();
                    handle_phase_end();
                    return true;
                }
            }
        }
        else
            handle_phase_dodged();
    }

    // don't crash on banishment
    if (!defender->pos().origin())
        handle_noise(defender->pos());

    // Noisy weapons.
    if (attacker->is_player()
        && weapon
        && is_artefact(*weapon)
        && artefact_property(*weapon, ARTP_NOISE))
    {
        noisy_equipment(*weapon);
    }

    if (!defender->alive())
        handle_phase_killed();

    handle_phase_aux();

    handle_phase_end();

    return true;
}

bool melee_attack::check_unrand_effects()
{
    if (unrand_entry && unrand_entry->melee_effects && weapon)
    {
        // Don't trigger the Wyrmbane death effect yet; that is done in
        // handle_phase_killed().
        if (weapon->unrand_idx == UNRAND_WYRMBANE && !defender->alive())
            return true;

        unwind_var<brand_type> unwind(damage_brand, SPWPN_NORMAL);
        unrand_entry->melee_effects(mutable_wpn, attacker, defender,
                                    damage_done, this);
        return !defender->alive(); // may have changed
    }

    return false;
}

void melee_attack::maybe_trigger_detonation()
{
    if (attacker->is_player()
                       && you.duration[DUR_DETONATION_CATALYST]
                       && !cleaving && in_bounds(defender->pos()))
        {
            schedule_detonation_fineff(defender->pos(), weapon);
        }
}

class AuxAttackType
{
public:
    AuxAttackType(int _damage, int _chance, string _name) :
    damage(_damage), chance(_chance), name(_name) { };
public:
    virtual int get_damage(bool /*random*/) const { return damage; };
    virtual int get_brand() const { return SPWPN_NORMAL; };
    virtual string get_name() const { return name; };
    virtual string get_verb() const { return get_name(); };
    int get_chance() const {
        const int base = get_base_chance();
        if (xl_based_chance())
            return base * (30 + you.experience_level) / 59;
        return base;
    }
    virtual bool is_usable() const {return false; };
    virtual int get_base_chance() const { return chance; }
    virtual bool xl_based_chance() const { return true; }
    virtual string describe() const;
protected:
    const int damage;
    // Per-attack trigger percent, before accounting for XL.
    const int chance;
    const string name;
};

class AuxConstrict: public AuxAttackType
{
public:
    AuxConstrict()
    : AuxAttackType(0, 100, "grab") { };
    bool xl_based_chance() const override { return false; }

    bool is_usable() const override
    {
        return you.get_mutation_level(MUT_CONSTRICTING_TAIL) >= 2
                || you.has_mutation(MUT_TENTACLE_ARMS)
                    && you.has_usable_tentacle()
                || you.form == transformation::serpent;
    }
};

class AuxKick: public AuxAttackType
{
public:
    AuxKick()
    : AuxAttackType(5, 100, "kick") { };

    int get_damage(bool /*random*/) const override
    {
        if (you.has_hooves())
        {
            // Max hoof damage: 10.
            return damage + you.get_mutation_level(MUT_HOOVES) * 5 / 3;
        }

        if (you.has_talons())
        {
            // Max talon damage: 9.
            return damage + 1 + you.get_mutation_level(MUT_TALONS);
        }

        // Max spike damage: 8.
        // ... yes, apparently tentacle spikes are "kicks".
        return damage + you.get_mutation_level(MUT_TENTACLE_SPIKE);
    }

    string get_verb() const override
    {
        if (you.has_usable_talons())
            return "claw";
        if (you.get_mutation_level(MUT_TENTACLE_SPIKE))
            return "pierce";
        return name;
    }

    string get_name() const override
    {
        if (you.get_mutation_level(MUT_TENTACLE_SPIKE))
            return "tentacle spike";
        return name;
    }

    bool is_usable() const override
    {
        return you.has_usable_hooves()
               || you.has_usable_talons()
               || you.get_mutation_level(MUT_TENTACLE_SPIKE);
    }
};

class AuxHeadbutt: public AuxAttackType
{
public:
    AuxHeadbutt()
    : AuxAttackType(5, 67, "headbutt") { };

    int get_damage(bool /*random*/) const override
    {
        return damage + you.get_mutation_level(MUT_HORNS) * 3;
    }

    bool is_usable() const override
    {
        return you.get_mutation_level(MUT_HORNS);
    }
};

class AuxPeck: public AuxAttackType
{
public:
    AuxPeck()
    : AuxAttackType(6, 67, "peck") { };

    bool is_usable() const override
    {
        return you.get_mutation_level(MUT_BEAK);
    }
};

class AuxTail: public AuxAttackType
{
public:
    AuxTail()
    : AuxAttackType(6, 50, "tail-slap") { };

    int get_damage(bool /*random*/) const override
    {
        const int base = you.fishtail
                            || you.has_mutation(MUT_ARMOURED_TAIL)
                            || you.has_mutation(MUT_WEAKNESS_STINGER)
                            || you.has_mutation(MUT_WEAKNESS_STINGER)
                            ? 6 : 0;

        return base + max(0, you.get_mutation_level(MUT_ARMOURED_TAIL) - 1) * 4
                    + you.get_mutation_level(MUT_STINGER) * 3
                    + you.get_mutation_level(MUT_WEAKNESS_STINGER);
    }

    string get_name() const override
    {
        if (you.has_mutation(MUT_STINGER) || you.has_mutation(MUT_WEAKNESS_STINGER))
            return "sting";
        else
            return "tail-slap";
    }

    int get_brand() const override
    {
        if (you.get_mutation_level(MUT_WEAKNESS_STINGER) == 3)
            return SPWPN_WEAKNESS;

        return you.get_mutation_level(MUT_STINGER) ? SPWPN_VENOM : SPWPN_NORMAL;
    }

    bool is_usable() const override
    {
        return you.has_mutation(MUT_ARMOURED_TAIL)
                || you.has_mutation(MUT_STINGER)
                || you.has_mutation(MUT_WEAKNESS_STINGER)
                || you.fishtail;
    }
};

class AuxPunch: public AuxAttackType
{
public:
    AuxPunch()
    : AuxAttackType(5, 0, "punch") { };

    int get_damage(bool random) const override
    {
        const int base_dam = damage + (random ? you.skill_rdiv(SK_UNARMED_COMBAT, 1, 2)
                                              : you.skill(SK_UNARMED_COMBAT) / 2);

        if (you.has_usable_claws())
        {
            const int claws = you.has_claws();
            const int die_size = 3;
            // Don't use maybe_roll_dice because we want max, not mean.
            return base_dam + (random ? roll_dice(claws, die_size)
                                      : claws * die_size);
        }

        return base_dam;
    }

    string get_name() const override
    {
        if (you.has_usable_tentacles())
            return get_verb();
        else
            return "off-hand " + get_verb();
    }

    string get_verb() const override
    {
        if (you.form == transformation::eel_hands)
            return "eel-slap";

        if (you.has_usable_claws())
            return "claw";

        if (you.has_usable_tentacles())
            return "tentacle-slap";

        return name;
    }

    int get_base_chance() const override
    {
        // Huh, this is a bit low. 5% at 0 UC, 50% at 27 UC..!
        // We don't div-rand-round because we want this to be
        // consistent for mut descriptions.
        return 5 + you.skill(SK_UNARMED_COMBAT, 5) / 3;
    }

    bool is_usable() const override
    {
        return !you.weapon() // UC only
        // Bats like drinking punch, not throwing 'em.
           && get_form()->can_offhand_punch()
        // No punching with a shield or 2-handed wpn.
        // Octopodes aren't affected by this, though!
            && (you.arm_count() > 2 || you.has_usable_offhand());
    }
};

class AuxBite: public AuxAttackType
{
public:
    AuxBite()
    : AuxAttackType(1, 40, "bite") { };

    int get_damage(bool random) const override
    {
        // duplicated in _describe_talisman_form
        const int fang_damage = damage + you.has_usable_fangs() * 2;

        if (you.get_mutation_level(MUT_ANTIMAGIC_BITE))
        {
            const int hd = you.get_hit_dice();
            const int denom = 3;
            return fang_damage + (random ? div_rand_round(hd, denom) : hd / denom);
        }

        if (you.get_mutation_level(MUT_ACIDIC_BITE))
            return fang_damage + (random ? roll_dice(2, 4) : 4);

        return fang_damage;
    }

    int get_brand() const override
    {
        if (you.get_mutation_level(MUT_ANTIMAGIC_BITE))
            return SPWPN_ANTIMAGIC;

        if (you.get_mutation_level(MUT_ACIDIC_BITE))
            return SPWPN_ACID;

        return SPWPN_NORMAL;
    }

    int get_base_chance() const override
    {
        if (you.get_mutation_level(MUT_ANTIMAGIC_BITE))
            return 100;
        return chance;
    }

    bool is_usable() const override
    {
        return you.get_mutation_level(MUT_ANTIMAGIC_BITE)
            || (you.has_usable_fangs()
                || you.get_mutation_level(MUT_ACIDIC_BITE));
    }
};

class AuxPseudopods: public AuxAttackType
{
public:
    AuxPseudopods()
    : AuxAttackType(4, 67, "pseudopods") { };

    int get_damage(bool /*random*/) const override
    {
        return damage * you.has_usable_pseudopods();
    }

    string get_verb() const override
    {
        return "bludgeon";
    }

    bool is_usable() const override
    {
        return you.has_usable_pseudopods();
    }
};

class AuxTentacles: public AuxAttackType
{
public:
    AuxTentacles()
    : AuxAttackType(12, 67, "squeeze") { };

    bool is_usable() const override
    {
        return you.has_usable_tentacles();
    }
};


class AuxTouch: public AuxAttackType
{
public:
    AuxTouch()
    : AuxAttackType(6, 40, "touch") { };

    int get_damage(bool random) const override
    {
        const int max = you.get_mutation_level(MUT_DEMONIC_TOUCH) * 4;
        return damage + (random ? random2(max + 1) : max);
    }

    int get_brand() const override
    {
        if (you.get_mutation_level(MUT_DEMONIC_TOUCH) == 3)
            return SPWPN_VULNERABILITY;

        return SPWPN_NORMAL;
    }

    bool xl_based_chance() const override { return false; }

    bool is_usable() const override
    {
        return you.get_mutation_level(MUT_DEMONIC_TOUCH)
            && you.has_usable_offhand();
    }
};

class AuxMaw: public AuxAttackType
{
public:
    AuxMaw()
    : AuxAttackType(0, 100, "chomp") { };
    int get_damage(bool random) const override {
        return get_form()->get_aux_damage(random);
    };
    bool xl_based_chance() const override { return false; }

    bool is_usable() const override
    {
        return you.form == transformation::maw;
    }
};

class AuxBlades: public AuxAttackType
{
public:
    AuxBlades()
    : AuxAttackType(1, 100, "whirl of blades") { };

    string get_verb() const override
    {
        return "shred";
    }

    int get_damage(bool random) const override
    {
        return 6 + (random ? div_rand_round(you.experience_level, 2)
                           : you.experience_level / 2);
    };

    bool xl_based_chance() const override { return false; }

    bool is_usable() const override
    {
        return you.duration[DUR_EXECUTION];
    }
};

class AuxFisticloak: public AuxAttackType
{
public:
    AuxFisticloak()
    : AuxAttackType(9, 100, "shroompunch") { };

    bool xl_based_chance() const override { return false; }

    bool is_usable() const override
    {
        return false;   // Only usable by the fisticloak world_reacts function
    }
};

class AuxMedusaStinger: public AuxAttackType
{
public:
    AuxMedusaStinger()
    : AuxAttackType(0, 100, "sting") { };

    bool xl_based_chance() const override { return false; }

    bool is_usable() const override
    {
        return false; // Actually handled by _do_medusa_stinger() due to its
                      // quasi-AoE nature.
    }
};

class AuxTalismanBlade: public AuxAttackType
{
public:
    AuxTalismanBlade()
    : AuxAttackType(0, 60, "slice") { };
    int get_damage(bool random) const override {
        return get_form()->get_aux_damage(random);
    };
    bool xl_based_chance() const override { return false; }

    bool is_usable() const override
    {
        return you.form == transformation::blade;
    }
};

static const AuxConstrict   AUX_CONSTRICT = AuxConstrict();
static const AuxKick        AUX_KICK = AuxKick();
static const AuxPeck        AUX_PECK = AuxPeck();
static const AuxHeadbutt    AUX_HEADBUTT = AuxHeadbutt();
static const AuxTail        AUX_TAIL = AuxTail();
static const AuxTouch       AUX_TOUCH = AuxTouch();
static const AuxPunch       AUX_PUNCH = AuxPunch();
static const AuxBite        AUX_BITE = AuxBite();
static const AuxPseudopods  AUX_PSEUDOPODS = AuxPseudopods();
static const AuxTentacles   AUX_TENTACLES = AuxTentacles();
static const AuxMaw         AUX_MAW = AuxMaw();
static const AuxBlades      AUX_EXECUTIONER_BLADE = AuxBlades();
static const AuxFisticloak  AUX_FUNGAL_FISTICLOAK = AuxFisticloak();
static const AuxMedusaStinger AUX_MEDUSA_STINGER = AuxMedusaStinger();
static const AuxTalismanBlade AUX_TALISMAN_BLADE = AuxTalismanBlade();
static const AuxAttackType* const aux_attack_types[] =
{
    &AUX_CONSTRICT,
    &AUX_KICK,
    &AUX_HEADBUTT,
    &AUX_PECK,
    &AUX_TAIL,
    &AUX_TOUCH,
    &AUX_PUNCH,
    &AUX_BITE,
    &AUX_PSEUDOPODS,
    &AUX_TENTACLES,
    &AUX_MAW,
    &AUX_EXECUTIONER_BLADE,
    &AUX_FUNGAL_FISTICLOAK,
    &AUX_MEDUSA_STINGER,
    &AUX_TALISMAN_BLADE,
    &AUX_TALISMAN_BLADE,
};


/* Setup all unarmed (non attack_type) variables
 *
 * Clears any previous unarmed attack information and sets everything from
 * noise_factor to verb and damage. Called after player_aux_choose_uc_attack
 */
void melee_attack::player_aux_setup(unarmed_attack_type atk)
{
    const int num_aux_objs = ARRAYSZ(aux_attack_types);
    const int num_aux_atks = UNAT_LAST_ATTACK - UNAT_FIRST_ATTACK + 1;
    COMPILE_CHECK(num_aux_objs == num_aux_atks);

    ASSERT(atk >= UNAT_FIRST_ATTACK);
    ASSERT(atk <= UNAT_LAST_ATTACK);
    const AuxAttackType* const aux = aux_attack_types[atk - UNAT_FIRST_ATTACK];

    aux_damage = aux->get_damage(true);
    damage_brand = (brand_type)aux->get_brand();
    aux_attack = aux->get_name();
    aux_verb = aux->get_verb();

    if (wu_jian_attack != WU_JIAN_ATTACK_NONE)
        wu_jian_attack = WU_JIAN_ATTACK_TRIGGERED_AUX;
}

bool melee_attack::player_aux_test_hit()
{
    const int evasion = defender->evasion(false, attacker);

    bool auto_hit = one_chance_in(30);

    if (you.duration[DUR_BLIND])
    {
        if (x_chance_in_y(player_blind_miss_chance(1), 100))
            to_hit = -1;
    }

    if (to_hit >= evasion || auto_hit
        || wu_jian_has_momentum(wu_jian_attack))
    {
        return true;
    }

    mprf("Your %s misses %s.", aux_attack.c_str(),
         defender->name(DESC_THE).c_str());

    return false;
}

/* Controls the looping on available unarmed attacks
 *
 * As the master method for unarmed player combat, this loops through
 * available unarmed attacks, determining whether they hit and - if so -
 * calculating and applying their damage.
 *
 * Returns (defender dead)
 */
bool melee_attack::player_do_aux_attacks()
{
    unwind_var<brand_type> save_brand(damage_brand);

    for (int i = UNAT_FIRST_ATTACK; i <= UNAT_LAST_ATTACK; ++i)
    {
        if (!defender->alive())
            break;

        unarmed_attack_type atk = static_cast<unarmed_attack_type>(i);

        if (!_extra_aux_attack(atk))
            continue;

        if (player_do_aux_attack(atk))
            return true;
    }

    return false;
}

/* Performs the specified auxiliary attack type against the defender.
 *
 * Returns if the defender was killed
 */
bool melee_attack::player_do_aux_attack(unarmed_attack_type atk)
{
    // Determine and set damage and attack words.
    player_aux_setup(atk);

    if (atk == UNAT_CONSTRICT && !attacker->can_constrict(*defender, CONSTRICT_MELEE))
        return false;

    to_hit = random2(aux_to_hit());
    to_hit += post_roll_to_hit_modifiers(to_hit, false);

    handle_noise(defender->pos());

    // Just about anything could've happened after all that racket.
    // Let's be paranoid.
    if (!defender->alive())
        return true;

    if (player_aux_test_hit())
    {
        // Upset the monster.
        behaviour_event(defender->as_monster(), ME_WHACK, attacker);
        if (!defender->alive())
            return true;

        if (attack_shield_blocked())
            return false;
        if (player_aux_apply(atk))
            return true;
    }

    return false;
}

bool melee_attack::player_aux_apply(unarmed_attack_type atk)
{
    did_hit = true;

    // XXX: Merge action counts into a single entry. (They're otherwise identical.)
    if (atk == UNAT_TALISMAN_BLADE_2)
        atk = UNAT_TALISMAN_BLADE_1;

    count_action(CACT_MELEE, -1, atk); // aux_attack subtype/auxtype

    if (atk != UNAT_TOUCH)
    {
        // Multiply damage before modifying by stats to avoid large breakpoints.
        aux_damage  = stat_modify_damage(aux_damage * 100, SK_UNARMED_COMBAT);

        aux_damage  = div_round_near(random2(aux_damage+1), 100);

        aux_damage  = apply_fighting_skill(aux_damage, true, true);

        aux_damage  = player_apply_misc_modifiers(aux_damage);

        aux_damage  = player_apply_slaying_bonuses(aux_damage, true);

        aux_damage  = player_apply_final_multipliers(aux_damage, true);

        if (atk == UNAT_CONSTRICT || atk == UNAT_MEDUSA_STINGER)
            aux_damage = 0;
        else
            aux_damage = apply_defender_ac(aux_damage);

        aux_damage = player_apply_postac_multipliers(aux_damage);
    }

    aux_damage = inflict_damage(aux_damage, BEAM_MISSILE);
    damage_done = aux_damage;

    if (defender->alive())
    {
        if (atk == UNAT_CONSTRICT)
            attacker->start_constricting(*defender, CONSTRICT_MELEE);

        if (damage_done > 0 || atk == UNAT_CONSTRICT || atk == UNAT_MEDUSA_STINGER)
            player_announce_aux_hit(atk);
        else
        {
            mprf("You %s %s%s.",
                    aux_verb.c_str(),
                    defender->name(DESC_THE).c_str(),
                    you.can_see(*defender) ? ", but do no damage" : "");
        }

        if (atk == UNAT_MEDUSA_STINGER)
            poison_monster(defender->as_monster(), &you, random_range(1, 2));

        // Allow to trigger regardless of damage, just like venom weapons.
        if (damage_brand == SPWPN_VENOM && !one_chance_in(3))
            poison_monster(defender->as_monster(), &you);

        if (damage_done > 0)
        {
            if (damage_brand == SPWPN_ACID && !one_chance_in(3))
                defender->corrode(&you);

            if (damage_brand == SPWPN_WEAKNESS
                && !(defender->holiness() & (MH_UNDEAD | MH_NONLIVING)))
            {
                defender->weaken(&you, 6);
            }

            if (damage_brand == SPWPN_VULNERABILITY)
            {
                if (defender->strip_willpower(&you, random_range(4, 8), true))
                {
                    mprf("You sap %s willpower!",
                         defender->as_monster()->pronoun(PRONOUN_POSSESSIVE).c_str());
                }
            }

            if (damage_brand == SPWPN_ANTIMAGIC && you.has_mutation(MUT_ANTIMAGIC_BITE)
                && damage_done > 0)
            {
                const bool spell_user = defender->antimagic_susceptible();

                antimagic_affects_defender(damage_done * 32);

                mprf("You drain %s %s.",
                     defender->as_monster()->pronoun(PRONOUN_POSSESSIVE).c_str(),
                     spell_user ? "magic" : "power");

                if (you.magic_points != you.max_magic_points
                    && !defender->is_summoned()
                    && !defender->is_firewood())
                {
                    int drain = random2(damage_done * 2) + 1;
                    // Augment mana drain--1.25 "standard" effectiveness at 0 mp,
                    // 0.25 at mana == max_mana
                    drain = (int)((1.25 - you.magic_points / you.max_magic_points)
                                  * drain);
                    if (drain)
                    {
                        mpr("You feel invigorated.");
                        inc_mp(drain);
                    }
                }
            }
        }
    }
    else // defender was just alive, so this call should be ok?
        player_announce_aux_hit(atk);

    if (defender->as_monster()->hit_points < 1)
    {
        handle_phase_killed();
        return true;
    }
    else if (atk == UNAT_FUNGAL_FISTICLOAK && !defender->is_unbreathing()
            && one_chance_in(3))
    {
        defender->confuse(attacker, 5);
    }

    return false;
}

void melee_attack::player_announce_aux_hit(unarmed_attack_type atk)
{
    mprf("%s %s %s%s%s",
         atk == UNAT_MEDUSA_STINGER ? "Your tendrils" : "You",
         aux_verb.c_str(),
         defender->name(DESC_THE).c_str(),
         debug_damage_number().c_str(),
         attack_strength_punctuation(damage_done).c_str());
}

void melee_attack::player_warn_miss()
{
    did_hit = false;

    mprf("You%s miss %s%s.",
         evasion_margin_adverb().c_str(),
         defender->name(DESC_THE).c_str(),
         weapon_desc().c_str());
}

// A couple additive modifiers that should be applied to both unarmed and
// armed attacks.
int melee_attack::player_apply_misc_modifiers(int damage)
{
    if (you.duration[DUR_MIGHT] || you.duration[DUR_BERSERK])
        damage += 1 + random2(10);

    return damage;
}

// Multipliers to be applied to the final (pre-stab, pre-AC) damage.
// It might be tempting to try to pick and choose what pieces of the damage
// get affected by such multipliers, but putting them at the end is the
// simplest effect to understand if they aren't just going to be applied
// to the base damage of the weapon.
int melee_attack::player_apply_final_multipliers(int damage, bool aux)
{
    // cleave damage modifier
    if (cleaving)
        damage = cleave_damage_mod(damage);

    // martial damage modifier (wu jian)
    damage = martial_damage_mod(damage);

    // resonance armour damage modifier
    damage = resonance_damage_mod(damage, true);

    // Electric charge bonus.
    if (charge_pow > 0 && defender->res_elec() <= 0)
        damage += div_rand_round(damage * charge_pow, 150);

    if (you.duration[DUR_WEAK])
        damage = div_rand_round(damage * 3, 4);

    apply_rev_penalty(damage);

    if (you.has_mutation(MUT_RECKLESS) && weapon
        && hands_reqd(&you, weapon->base_type, weapon->sub_type) == HANDS_TWO)
    {
        damage = div_rand_round(damage * 115, 100);
    }

    if (you.duration[DUR_CONFUSING_TOUCH] && !aux)
        return 0;

    return attack::player_apply_final_multipliers(damage, aux);
}

int melee_attack::player_apply_postac_multipliers(int damage)
{
    // Statue form's damage modifier is designed to exactly compensate for
    // the slowed speed; therefore, it needs to apply after AC.
    // Flux form's modifier is the inverse of statue form's.
    if (you.form == transformation::statue)
        damage = div_rand_round(damage * 3, 2);
    else if (you.form == transformation::flux)
        damage = div_rand_round(damage, 2);
    else if (you.form == transformation::walking_scroll)
        damage = div_rand_round(damage, 2);

    return damage;
}

void melee_attack::set_attack_verb(int damage)
{
    if (!attacker->is_player())
        return;

    int weap_type = WPN_UNKNOWN;

    if (Options.has_fake_lang(flang_t::grunt))
        damage = HIT_STRONG + 1;

    if (!weapon)
        weap_type = WPN_UNARMED;
    else if (weapon->base_type == OBJ_STAVES)
        weap_type = WPN_STAFF;
    else if (weapon->base_type == OBJ_WEAPONS
             && !is_range_weapon(*weapon))
    {
        weap_type = weapon->sub_type;
    }

    // All weak hits with weapons look the same.
    if (damage < HIT_WEAK
        && weap_type != WPN_UNARMED)
    {
        if (weap_type != WPN_UNKNOWN)
            attack_verb = "hit";
        else
            attack_verb = "clumsily bash";
        return;
    }

    // Take normal hits into account. If the hit is from a weapon with
    // more than one damage type, randomly choose one damage type from
    // it.
    monster_type defender_genus = mons_genus(defender->type);
    switch (weapon ? single_damage_type(*weapon) : -1)
    {
    case DAM_PIERCE:
        if (damage < HIT_MED)
            attack_verb = "puncture";
        else if (damage < HIT_STRONG)
            attack_verb = "impale";
        else
        {
            if (defender->is_monster()
                && defender_visible
                && defender_genus == MONS_HOG)
            {
                attack_verb = "spit";
                verb_degree = "like the proverbial pig";
            }
            else if (defender_genus == MONS_CRAB
                     && Options.has_fake_lang(flang_t::grunt))
            {
                attack_verb = "attack";
                verb_degree = "'s weak point";
            }
            else
            {
                static const char * const pierce_desc[][2] =
                {
                    {"spit", "like a pig"},
                    {"skewer", "like a kebab"},
                    {"stick", "like a pincushion"},
                    {"perforate", "like a sieve"}
                };
                const int choice = random2(ARRAYSZ(pierce_desc));
                attack_verb = pierce_desc[choice][0];
                verb_degree = pierce_desc[choice][1];
            }
        }
        break;

    case DAM_SLICE:
        if (damage < HIT_MED)
            attack_verb = "slash";
        else if (damage < HIT_STRONG)
            attack_verb = "slice";
        else if (defender_genus == MONS_OGRE)
        {
            attack_verb = "dice";
            verb_degree = "like an onion";
        }
        else if (defender_genus == MONS_DRAUGR)
        {
            attack_verb = "fracture";
            verb_degree = "into splinters";
        }
        else if (defender_genus == MONS_HOG)
        {
            attack_verb = "carve";
            verb_degree = "like the proverbial ham";
        }
        else if ((defender_genus == MONS_TENGU
                  || get_mon_shape(defender_genus) == MON_SHAPE_BIRD)
                 && one_chance_in(3))
        {
            attack_verb = "carve";
            verb_degree = "like a turkey";
        }
        else if ((defender_genus == MONS_YAK || defender_genus == MONS_YAKTAUR)
                 && Options.has_fake_lang(flang_t::grunt))
        {
            attack_verb = "shave";
        }
        else
        {
            static const char * const slice_desc[][2] =
            {
                {"open",    "like a pillowcase"},
                {"slice",   "like a ripe choko"},
                {"cut",     "into ribbons"},
                {"carve",   "like a ham"},
                {"chop",    "into pieces"}
            };
            const int choice = random2(ARRAYSZ(slice_desc));
            attack_verb = slice_desc[choice][0];
            verb_degree = slice_desc[choice][1];
        }
        break;

    case DAM_BLUDGEON:
        if (damage < HIT_MED)
            attack_verb = one_chance_in(4) ? "thump" : "sock";
        else if (damage < HIT_STRONG)
            attack_verb = "bludgeon";
        else if (defender_genus == MONS_DRAUGR)
        {
            attack_verb = "shatter";
            verb_degree = "into splinters";
        }
        else if (defender->type == MONS_GREAT_ORB_OF_EYES)
        {
            attack_verb = "splatter";
            verb_degree = "into a gooey mess";
        }
        else
        {
            static const char * const bludgeon_desc[][2] =
            {
                {"crush",   "like a grape"},
                {"beat",    "like a drum"},
                {"hammer",  "like a gong"},
                {"pound",   "like an anvil"},
                {"flatten", "like a pancake"}
            };
            const int choice = random2(ARRAYSZ(bludgeon_desc));
            attack_verb = bludgeon_desc[choice][0];
            verb_degree = bludgeon_desc[choice][1];
        }
        break;

    case DAM_WHIP:
        if (damage < HIT_MED)
            attack_verb = "flog";
        else if (damage < HIT_STRONG)
            attack_verb = "flagellate";
        else
        {
            attack_verb = "thrash";
            verb_degree = "mercilessly";
        }
        break;

    case -1: // unarmed
    {
        const FormAttackVerbs verbs = get_form(you.form)->uc_attack_verbs;
        if (verbs.weak != nullptr)
        {
            if (damage < HIT_WEAK)
                attack_verb = verbs.weak;
            else if (damage < HIT_MED)
                attack_verb = verbs.medium;
            else if (damage < HIT_STRONG)
                attack_verb = verbs.strong;
            else
                attack_verb = verbs.devastating;
            break;
        }

        if (damage_type == DVORP_CLAWING)
        {
            if (damage < HIT_WEAK)
                attack_verb = "scratch";
            else if (damage < HIT_MED)
                attack_verb = "claw";
            else if (damage < HIT_STRONG)
                attack_verb = "mangle";
            else
                attack_verb = "eviscerate";
        }
        else if (damage_type == DVORP_TENTACLE)
        {
            if (damage < HIT_WEAK)
                attack_verb = "tentacle-slap";
            else if (damage < HIT_MED)
                attack_verb = "bludgeon";
            else if (damage < HIT_STRONG)
                attack_verb = "batter";
            else
                attack_verb = "thrash";
        }
        else
        {
            if (damage < HIT_WEAK)
                attack_verb = "hit";
            else if (damage < HIT_MED)
                attack_verb = "punch";
            else if (damage < HIT_STRONG)
                attack_verb = "pummel";
            else if (defender->is_monster()
                     && mons_genus(defender->type) == MONS_FORMICID)
            {
                attack_verb = "squash";
                verb_degree = "like the proverbial ant";
            }
            else
            {
                static const char * const punch_desc[][2] =
                {
                    {"pound",     "into fine dust"},
                    {"pummel",    "like a punching bag"},
                    {"pulverise", ""},
                    {"squash",    "like an ant"}
                };
                const int choice = random2(ARRAYSZ(punch_desc));
                // XXX: could this distinction work better?
                if (choice == 0
                    && defender->is_monster()
                    && mons_has_blood(defender->type))
                {
                    attack_verb = "beat";
                    verb_degree = "into a bloody pulp";
                }
                else
                {
                    attack_verb = punch_desc[choice][0];
                    verb_degree = punch_desc[choice][1];
                }
            }
        }
        break;
    }

    case WPN_UNKNOWN:
    default:
        attack_verb = "hit";
        break;
    }
}

void melee_attack::player_exercise_combat_skills()
{
    if (!defender->is_firewood())
        practise_hitting(weapon);
}

/*
 * Applies god conduct for weapon ego
 *
 * Using speed brand as a chei worshipper, or holy/unholy/wizardly weapons etc
 */
void melee_attack::player_weapon_upsets_god()
{
    if (weapon
        && (weapon->base_type == OBJ_WEAPONS || weapon->base_type == OBJ_STAVES)
        && god_hates_item_handling(*weapon))
    {
        did_god_conduct(god_hates_item_handling(*weapon), 2);
    }
}

void melee_attack::sear_defender()
{
    bool visible_effect = false;
    if (defender->is_player())
    {
        if (defender->res_fire() <= 3 && !you.duration[DUR_FIRE_VULN])
            visible_effect = true;
        you.increase_duration(DUR_FIRE_VULN, 5 + random2(attk_damage), 50);
    }
    else
    {
        if (!defender->as_monster()->has_ench(ENCH_FIRE_VULN))
            visible_effect = true;
        defender->as_monster()->add_ench(
            mon_enchant(ENCH_FIRE_VULN, attacker,
                        (5 + random2(attk_damage)) * BASELINE_DELAY));
    }

    if (needs_message && visible_effect)
    {
        mprf("%s fire resistance is stripped away!",
             def_name(DESC_ITS).c_str());
    }
}

/* Apply some player-specific hit effects.
 *
 * Called after damage is calculated, but before unrand effects and before
 * damage is dealt.
 *
 * Returns true if combat should continue, false if it should end here.
 */
bool melee_attack::player_monattk_hit_effects()
{
    player_weapon_upsets_god();

    // Don't even check effects if the monster has already been reset (for
    // example, a spectral weapon who noticed in player_stab_check that it
    // shouldn't exist anymore).
    if (defender->type == MONS_NO_MONSTER)
        return false;

    if (!defender->alive())
        return false;

    // These effects apply only to monsters that are still alive:

    // Returns true if the hydra was killed by the decapitation, in which case
    // nothing more should be done to the hydra.
    if (consider_decapitation(damage_done))
        return false;

    return true;
}

/**
 * If appropriate, chop a head off the defender. (Usually a hydra.)
 *
 * @param dam           The damage done in the attack that may or may not chop
  *                     off a head.
 * @return              Whether the defender was killed by the decapitation.
 */
bool melee_attack::consider_decapitation(int dam)
{
    if (!attack_chops_heads(dam))
        return false;

    decapitate();

    if (!defender->alive())
        return true;

    // Only living hydras get to regenerate heads.
    if (!(defender->holiness() & MH_NATURAL))
        return false;

    // What's the largest number of heads the defender can have?
    const int limit = defender->type == MONS_LERNAEAN_HYDRA ? 27 : 20;

    if (damage_brand == SPWPN_FLAMING)
    {
        if (defender_visible)
            mpr("The flame cauterises the wound!");
        return false;
    }

    int heads = defender->heads();
    if (heads >= limit - 1)
        return false; // don't overshoot the head limit!

    simple_monster_message(*defender->as_monster(), " grows two more!");
    defender->as_monster()->num_heads += 2;

    if (defender->type == MONS_SLYMDRA)
    {
        // Don't pretend this was from a real slime creature merging or the
        // player can slowly farm real XP off this.
        defender->props[SLYMDRA_FAKE_HEADS_KEY].get_int()++;
        slymdra_scale_hp(*defender->as_monster());
    }
    else
        defender->heal(8 + random2(8));

    return false;
}

/**
 * Can the given actor lose its heads? (Is it hydra or hydra-like?)
 *
 * @param defender  The actor in question.
 * @return          Whether the given actor is susceptible to head-choppage.
 */
static bool actor_can_lose_heads(const actor* defender)
{
    if (defender->is_monster()
        && defender->as_monster()->has_hydra_multi_attack()
        && defender->as_monster()->mons_species() != MONS_SPECTRAL_THING)
    {
        return true;
    }

    return false;
}

/**
 * Does this attack chop off one of the defender's heads? (Generally only
 * relevant for hydra defenders)
 *
 * @param dam           The damage done in the attack in question.
 * @param wpn_brand     The brand_type of the attack.
 * @return              Whether the attack will chop off a head.
 */
bool melee_attack::attack_chops_heads(int dam)
{
    // hydras and hydra-like things only.
    if (!actor_can_lose_heads(defender))
        return false;

    // no decapitate on riposte (Problematic)
    if (is_riposte)
        return false;

    // Monster attackers+defenders have only a 25% chance of making the
    // chop-check to prevent runaway head inflation.
    // XXX: Tentatively making an exception for spectral weapons
    const bool player_spec_weap = attacker->is_monster()
                                    && attacker->type == MONS_SPECTRAL_WEAPON
                                    && attacker->as_monster()->summoner
                                        == MID_PLAYER;
    if (attacker->is_monster() && defender->is_monster()
        && !player_spec_weap && !one_chance_in(4))
    {
        return false;
    }

    // Only cutting implements.
    if (damage_type != DVORP_SLICING
        && damage_type != DVORP_CHOPPING
        && damage_type != DVORP_CLAWING)
    {
        return false;
    }

    // Small claws are not big enough.
    if (damage_type == DVORP_CLAWING && attacker->has_claws() < 3)
        return false;

    // You need to have done at least some damage.
    if (dam <= 0 || dam < 4 && coinflip())
        return false;

    // ok, good enough!
    return true;
}

/**
 * Decapitate the (hydra or hydra-like) defender!
 */
void melee_attack::decapitate()
{
    // Player hydras don't gain or lose heads.
    ASSERT(defender->is_monster());

    const char *verb = nullptr;

    if (damage_type == DVORP_CLAWING)
    {
        static const char *claw_verbs[] = { "rip", "tear", "claw" };
        verb = RANDOM_ELEMENT(claw_verbs);
    }
    else
    {
        static const char *slice_verbs[] =
        {
            "slice", "lop", "chop", "hack"
        };
        verb = RANDOM_ELEMENT(slice_verbs);
    }

    int heads = defender->heads();
    if (heads == 1) // will be zero afterwards
    {
        if (defender_visible)
        {
            mprf("%s %s %s last head off!",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb(verb).c_str(),
                 apostrophise(defender_name(true)).c_str());
        }

        if (!defender->is_summoned())
        {
            bleed_onto_floor(defender->pos(), defender->type,
                             defender->as_monster()->hit_points, true);
        }

        if (!simu)
            monster_die(*defender->as_monster(), attacker);

        return;
    }

    if (defender_visible)
    {
        mprf("%s %s one of %s heads off!",
             atk_name(DESC_THE).c_str(),
             attacker->conj_verb(verb).c_str(),
             apostrophise(defender_name(true)).c_str());
    }

    defender->as_monster()->num_heads--;
}

/**
 * Apply passive retaliation damage from hitting acid monsters.
 */
void melee_attack::attacker_sustain_passive_damage()
{
    // If the defender has been cleaned up, it's too late for anything.
    if (!defender->alive())
        return;

    if (!mons_class_flag(defender->type, M_ACID_SPLASH))
        return;

    if (attacker->res_corr() >= 3)
        return;

    if (!adjacent(attacker->pos(), defender->pos()) || is_riposte)
        return;

    const int dmg = resist_adjust_damage(attacker, BEAM_ACID, roll_dice(1, 5));

    if (attacker->is_player())
        mpr(you.hands_act("burn", "!"));
    else
    {
        simple_monster_message(*attacker->as_monster(),
                               " is burned by acid!");
    }
    attacker->hurt(defender, dmg, BEAM_ACID,
                   KILLED_BY_ACID);

    if (attacker->alive() && one_chance_in(5))
        attacker->corrode(defender);
}

int melee_attack::staff_damage(stave_type staff) const
{
    const skill_type skill = staff_skill(staff);
    if (!x_chance_in_y(attacker->skill(SK_EVOCATIONS, 200)
                     + attacker->skill(skill, 100), 3000))
    {
        return 0;
    }

    const int mult = staff_damage_mult(staff);
    const int preac = random2((attacker->skill(skill, mult * 2)
                               + attacker->skill(SK_EVOCATIONS, mult))
                              / 80);
    return apply_defender_ac(preac, 0, staff_ac_check(staff));
}

string melee_attack::staff_message(stave_type staff, int dam) const
{
    switch (staff)
    {
    case STAFF_AIR:
        return make_stringf(
            "%s %s electrocuted%s",
            defender->name(DESC_THE).c_str(),
            defender->conj_verb("are").c_str(),
                            attack_strength_punctuation(dam).c_str());
    case STAFF_COLD:
        return make_stringf(
                "%s freeze%s %s%s",
                attacker->name(DESC_THE).c_str(),
                attacker->is_player() ? "" : "s",
                defender->name(DESC_THE).c_str(),
                attack_strength_punctuation(dam).c_str());

    case STAFF_EARTH:
        return make_stringf(
                "The ground beneath %s fractures%s",
                defender->name(DESC_THE).c_str(),
                         attack_strength_punctuation(dam).c_str());;

    case STAFF_FIRE:
        return make_stringf(
                    "%s burn%s %s%s",
                    attacker->name(DESC_THE).c_str(),
                    attacker->is_player() ? "" : "s",
                    defender->name(DESC_THE).c_str(),
                    attack_strength_punctuation(dam).c_str());
    case STAFF_ALCHEMY:
        return make_stringf(
                "%s envenom%s %s%s",
                attacker->name(DESC_THE).c_str(),
                attacker->is_player() ? "" : "s",
                defender->name(DESC_THE).c_str(),
                attack_strength_punctuation(dam).c_str());

    case STAFF_NECROMANCY:
        return make_stringf(
                "%s %s as negative energy consumes %s%s",
                defender->name(DESC_THE).c_str(),
                defender->conj_verb("shrivel").c_str(),
                defender->pronoun(PRONOUN_OBJECTIVE).c_str(),
                attack_strength_punctuation(dam).c_str());

    case STAFF_CONJURATION:
        return make_stringf(
                    "%s %s %s%s",
                    attacker->name(DESC_THE).c_str(),
                    attacker->conj_verb("blast").c_str(),
                    defender->name(DESC_THE).c_str(),
                    attack_strength_punctuation(dam).c_str());

    default:
        return "Something buggy happens! Please report this.";
    }
}

bool melee_attack::apply_staff_damage()
{
    if (!weapon)
        return false;

    if (attacker->is_player() && you.get_mutation_level(MUT_NO_ARTIFICE))
        return false;

    if (weapon->base_type != OBJ_STAVES
        || item_type_removed(weapon->base_type, weapon->sub_type))
    {
        return false;
    }

    const stave_type staff = static_cast<stave_type>(weapon->sub_type);
    int dam = staff_damage(staff);
    const beam_type flavour = staff_damage_type(staff);
    dam = resist_adjust_damage(defender, flavour, dam);
    if (staff == STAFF_EARTH && defender->airborne())
        dam /= 3;
    if (dam > 0)
    {
        if (staff == STAFF_NECROMANCY)
            attacker->god_conduct(DID_EVIL, 4);
        else if (staff == STAFF_FIRE && defender->is_player())
            maybe_melt_player_enchantments(flavour, dam);

        if (needs_message)
            mpr(staff_message(staff, dam));
    }

    dprf(DIAG_COMBAT, "Staff damage to %s: %d, flavour: %d",
         defender->name(DESC_THE).c_str(), dam, flavour);

    inflict_damage(dam, flavour);
    if (dam > 0)
    {
        defender->expose_to_element(flavour, 2, attacker);
        // Poisoning from the staff of alchemy should happen after damage.
        if (defender->alive() && flavour == BEAM_POISON)
            defender->poison(attacker, 2);
    }

    if (you.wearing_ego(OBJ_ARMOUR, SPARM_ATTUNEMENT)
        && you.magic_points < you.max_magic_points)
    {
        mpr("You draw in some of the released energy.");
        inc_mp(1 + one_chance_in(3) + one_chance_in(3));
    }

    return true;
}

int melee_attack::calc_to_hit(bool random)
{
    int mhit = attack::calc_to_hit(random);
    if (mhit == AUTOMATIC_HIT)
        return AUTOMATIC_HIT;

    return mhit;
}

int melee_attack::post_roll_to_hit_modifiers(int mhit, bool random)
{
    int modifiers = attack::post_roll_to_hit_modifiers(mhit, random);

    // Electric charges feel bad when they miss, so make them miss less often.
    if (charge_pow > 0)
        modifiers += 5;

    return modifiers;
}

void melee_attack::player_stab_check()
{
    if (!is_projected)
        attack::player_stab_check();
}

/**
 * Can we get a good stab with this weapon?
 */
bool melee_attack::player_good_stab()
{
    return you.weapon_is_good_stab(weapon);
}

/* Select the attack verb for attacker
 *
 * If klown, select randomly from klown_attack, otherwise check for any special
 * case attack verbs (tentacles or door/fountain-mimics) and if all else fails,
 * select an attack verb from attack_types based on the ENUM value of attk_type.
 *
 * Returns (attack_verb)
 */
string melee_attack::mons_attack_verb()
{
    static const char *klown_attack[] =
    {
        "hit",
        "poke",
        "prod",
        "flog",
        "pound",
        "slap",
        "tickle",
        "defenestrate",
        "sucker-punch",
        "elbow",
        "pinch",
        "strangle-hug",
        "squeeze",
        "tease",
        "eye-gouge",
        "karate-kick",
        "headlock",
        "wrestle",
        "trip-wire",
        "kneecap"
    };

    if (attacker->type == MONS_KILLER_KLOWN && attk_type == AT_HIT)
        return RANDOM_ELEMENT(klown_attack);

    //XXX: then why give them it in the first place?
    if (attk_type == AT_TENTACLE_SLAP && mons_is_tentacle(attacker->type))
        return "slap";

    if (is_special_mon_stab && attacker->type == MONS_PLAYER_SHADOW)
        return "eviscerate";

    if (attacker->type == MONS_HAUNTED_ARMOUR)
    {
        if (item_def* armour = attacker->as_monster()->body_armour())
        {
            switch (armour->sub_type)
            {
                case ARM_HAT:
                case ARM_HELMET:
                    return random_choose("headbutt", "bonk", "clobber");
                case ARM_BOOTS:
                    return random_choose("kick", "punt", "thump");
                case ARM_CLOAK:
                case ARM_SCARF:
                    return random_choose("buffet", "strangle");
                case ARM_GLOVES:
                    return random_choose("punch", "slap", "smack");

                default:
                    break;
            }
        }
    }

    return mon_attack_name(attk_type);
}

string melee_attack::mons_attack_desc()
{
    if (!you.can_see(*attacker))
        return "";

    string ret;
    int dist = (attack_position - defender->pos()).rdist();
    if (dist > 1)
        ret = " from afar";

    if (weapon && !mons_class_is_animated_weapon(attacker->type))
        ret += " with " + weapon->name(DESC_A, false, false, false);

    return ret;
}

string melee_attack::weapon_desc()
{
    if (!weapon || !you.offhand_weapon())
        return "";
    return " with " + weapon->name(DESC_YOUR, false, false, false);
}

string melee_attack::charge_desc()
{
    if (!charge_pow || defender->res_elec() > 0)
        return "";
    const string pronoun = defender->pronoun(PRONOUN_OBJECTIVE);
    return make_stringf(" and electrocute%s %s",
                        attacker->is_player() ? "" : "s",
                        pronoun.c_str());

}

void melee_attack::announce_hit()
{
    if (!needs_message || attk_flavour == AF_CRUSH)
        return;

    if (attacker->is_monster())
    {
        mprf("%s %s%s %s%s%s%s%s",
             atk_name(DESC_THE).c_str(),
             is_special_mon_stab
                && attacker->as_monster()->has_ench(ENCH_VAMPIRE_THRALL)
                    ? "stealthily " : "",
             attacker->conj_verb(mons_attack_verb()).c_str(),
             defender_name(true).c_str(),
             charge_desc().c_str(),
             debug_damage_number().c_str(),
             mons_attack_desc().c_str(),
             attack_strength_punctuation(damage_done).c_str());
    }
    else
    {
        if (!verb_degree.empty() && verb_degree[0] != ' '
            && verb_degree[0] != ',' && verb_degree[0] != '\'')
        {
            verb_degree = " " + verb_degree;
        }

        mprf("You %s %s%s%s%s%s%s",
             attack_verb.c_str(),
             defender->name(DESC_THE).c_str(), verb_degree.c_str(),
             weapon_desc().c_str(),
             charge_desc().c_str(), debug_damage_number().c_str(),
             attack_strength_punctuation(damage_done).c_str());
    }
}

// Returns if the target was actually poisoned by this attack
bool melee_attack::mons_do_poison()
{
    int amount;
    const int hd = attacker->get_hit_dice();

    if (attk_flavour == AF_POISON_STRONG)
        amount = random_range(hd * 11 / 3, hd * 13 / 2);
    else if (attk_flavour == AF_MINIPARA)
        amount = random_range(hd, hd * 2);
    else
        amount = random_range(hd * 2, hd * 4);

    if (attacker->as_monster()->has_ench(ENCH_CONCENTRATE_VENOM))
    {
        // Attach our base poison damage to the curare effect
        return curare_actor(attacker, defender, "concentrated venom",
                            attacker->name(DESC_PLAIN), amount);
    }

    if (!defender->poison(attacker, amount))
        return false;

    if (needs_message)
    {
        mprf("%s poisons %s!",
                atk_name(DESC_THE).c_str(),
                defender_name(true).c_str());
    }

    return true;
}

static void _print_resist_messages(actor* defender, int base_damage,
                                   beam_type flavour)
{
    // check_your_resists is used for the player case to get additional
    // effects such as Xom amusement, melting of icy effects, etc.
    // mons_adjust_flavoured is used for the monster case to get all of the
    // special message handling ("The ice beast melts!") correct.
    // XXX: there must be a nicer way to do this, especially because we're
    // basically calculating the damage twice in the case where messages
    // are needed.
    if (defender->is_player())
        (void)check_your_resists(base_damage, flavour, "");
    else
    {
        bolt beam;
        beam.flavour = flavour;
        (void)mons_adjust_flavoured(defender->as_monster(),
                                    beam,
                                    base_damage,
                                    true);
    }
}

bool melee_attack::mons_attack_effects()
{
    // may have died earlier, due to e.g. pain bond
    // we could continue with the rest of their attack, but it's a minefield
    // of potential crashes. so, let's not.
    if (attacker->is_monster() && invalid_monster(attacker->as_monster()))
        return false;

    // Monsters attacking themselves don't get attack flavour.
    // The message sequences look too weird. Also, stealing
    // attacks aren't handled until after the damage msg.
    if (attacker != defender)
    {
        mons_apply_attack_flavour();

        if (attacker->as_monster()->has_ench(ENCH_FIRE_CHAMPION))
            mons_apply_attack_flavour(AF_FIRE);

        // XXX: If we try this on a reset monster, the resist check to determine
        //      chaos brand will assert.
        if (defender->alive() && attacker->as_monster()->has_ench(ENCH_CHAOS_LACE))
            mons_apply_attack_flavour(AF_CHAOTIC);
    }

    if (defender->is_player())
        practise_being_hit();

    // A tentacle may have banished its own parent/sibling and thus itself.
    if (!attacker->alive())
        return false;

    // consider_decapitation() returns true if the defender was killed
    // by the decapitation, in which case we should stop the rest of the
    // attack, too.
    if (consider_decapitation(damage_done))
        return false;

    // Vhi's Electrolunge damage
    if (charge_pow > 0 && defender->alive() && defender->res_elec() <= 0)
    {
       int dmg = electrolunge_damage(charge_pow).roll();
       int hurt = defender->apply_ac(dmg, 0, ac_type::half);
       inflict_damage(hurt, BEAM_ELECTRICITY);
    }

    const bool slippery = defender->is_player()
                        && adjacent(attacker->pos(), defender->pos())
                        && !player_stair_delay() // feet otherwise occupied
                        && you.unrand_equipped(UNRAND_SLICK_SLIPPERS);
    if (attacker != defender && !is_projected
        && (attk_flavour == AF_TRAMPLE
            || slippery && attk_flavour != AF_DRAG)
        && (slippery || !attacker->cannot_move()))
    {
        do_knockback(slippery);
    }

    if (attacker != defender && attk_flavour == AF_DRAG)
        do_drag();

    special_damage = 0;
    special_damage_message.clear();
    special_damage_flavour = BEAM_NONE;

    // Defender banished. Bail since the defender is still alive in the
    // Abyss.
    if (defender->is_banished())
        return false;

    if (!defender->alive())
        return attacker->alive();

    // Bail if the monster is attacking itself without a weapon, since
    // intrinsic monster attack flavours aren't applied for self-attacks.
    if (attacker == defender && !weapon)
        return false;

    return true;
}

static bool _attack_flavour_needs_living_defender(attack_flavour flavour)
{
    switch (flavour)
    {
        case AF_SCARAB:
        case AF_VAMPIRIC:
        case AF_BLINK:
        case AF_SHADOWSTAB:
        case AF_SPIDER:
        case AF_HELL_HUNT:
        case AF_SWARM:
        case AF_BLOODZERK:
        case AF_ALEMBIC:
        case AF_BOMBLET:
            return false;

        default:
            return true;
    }
}

void melee_attack::mons_apply_attack_flavour(attack_flavour flavour)
{
    if (!attacker->alive())
        return;

    // Most of this is from BWR 4.1.2.

    // If unspecified, use standard flavour for this attack.
    if (flavour == AF_PLAIN)
        flavour = attk_flavour;

    if (flavour == AF_CHAOTIC)
        flavour = random_chaos_attack_flavour();

    // Most attack flavours don't make sense against a dead target, but some do.
    if (_attack_flavour_needs_living_defender(flavour) && !defender->alive())
        return;

    // Don't announce the speed draining component against dead targets.
    if (!defender->alive() && flavour == AF_SCARAB)
        flavour = AF_VAMPIRIC;

    const int base_damage = flavour_damage(flavour, attacker->get_hit_dice());

    // Note that if damage_done == 0 then this code won't be reached
    // unless the flavour is in flavour_triggers_damageless.
    switch (flavour)
    {
    default:
        // Just to trigger special melee damage effects for regular attacks
        // (e.g. Qazlal's elemental adaptation).
        defender->expose_to_element(BEAM_MISSILE, 2);
        break;

    case AF_POISON:
    case AF_POISON_STRONG:
    case AF_REACH_STING:
        if (attacker->as_monster()->has_ench(ENCH_CONCENTRATE_VENOM)
            ? coinflip()
            : one_chance_in(3))
        {
            mons_do_poison();
        }
        break;

    case AF_FIRE:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_FIRE,
                                 base_damage);
        special_damage_flavour = BEAM_FIRE;

        if (needs_message && base_damage)
        {
            mprf("%s %s engulfed in flames%s",
                 defender_name(false).c_str(),
                 defender->conj_verb("are").c_str(),
                 attack_strength_punctuation(special_damage).c_str());

            _print_resist_messages(defender, base_damage, BEAM_FIRE);
        }

        defender->expose_to_element(BEAM_FIRE, 2);
        break;

    case AF_COLD:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_COLD,
                                 base_damage);
        special_damage_flavour = BEAM_COLD;

        if (needs_message && base_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("freeze").c_str(),
                 defender_name(true).c_str(),
                 attack_strength_punctuation(special_damage).c_str());

            _print_resist_messages(defender, base_damage, BEAM_COLD);
        }

        defender->expose_to_element(BEAM_COLD, 2, attacker);
        break;

    case AF_ELEC:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_ELECTRICITY,
                                 base_damage);
        special_damage_flavour = BEAM_ELECTRICITY;

        if (needs_message && base_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("shock").c_str(),
                 defender_name(true).c_str(),
                 attack_strength_punctuation(special_damage).c_str());

            _print_resist_messages(defender, base_damage, BEAM_ELECTRICITY);
        }

        dprf(DIAG_COMBAT, "Shock damage: %d", special_damage);
        defender->expose_to_element(BEAM_ELECTRICITY, 2);
        break;

        // Combines drain speed and vampiric.
    case AF_SCARAB:
        if (x_chance_in_y(3, 5))
            drain_defender_speed();

        // deliberate fall-through
    case AF_VAMPIRIC:
        if (!actor_is_susceptible_to_vampirism(*defender))
            break;

        if (defender->stat_hp() < defender->stat_maxhp())
        {
            int healed = resist_adjust_damage(defender, BEAM_NEG,
                                              1 + random2(damage_done));
            if (healed)
            {
                attacker->heal(healed);
                if (needs_message)
                {
                    mprf("%s %s vitality from %s injuries!",
                         atk_name(DESC_THE).c_str(),
                         attacker->conj_verb("draw").c_str(),
                         def_name(DESC_ITS).c_str());
                }
            }
        }
        break;

    case AF_BLINK:
        // blinking can kill, delay the call
        if (one_chance_in(3))
            schedule_blink_fineff(attacker);
        break;

    case AF_BLINK_WITH:
        if (coinflip())
            schedule_blink_fineff(attacker, defender);
        break;

    case AF_CONFUSE:
        if (attk_type == AT_SPORE)
        {
            if (defender->is_unbreathing())
                break;

            monster *attkmon = attacker->as_monster();
            attkmon->set_hit_dice(attkmon->get_experience_level() - 1);
            if (attkmon->get_experience_level() <= 0)
                attacker->as_monster()->suicide();

            if (defender_visible)
            {
                mprf("%s %s engulfed in a cloud of dizzying spores!",
                     defender->name(DESC_THE).c_str(),
                     defender->conj_verb("are").c_str());
            }
        }

        if (one_chance_in(3))
        {
            if (attk_type != AT_SPORE && defender_visible)
            {
                mprf("%s %s afflicted by dizzying energies!",
                     defender->name(DESC_THE).c_str(),
                     defender->conj_verb("are").c_str());
            }
            defender->confuse(attacker,
                              1 + random2(3+attacker->get_hit_dice()));
        }
        break;

    case AF_DRAIN:
        if (coinflip())
            drain_defender();
        break;

    case AF_BARBS:
        // same duration/power as manticore barbs
        if (defender->is_player())
            barb_player(random_range(4, 8), 4);
        // Insubstantial and jellies are immune
        else if (!(defender->is_insubstantial()))
        {
            if (defender_visible)
            {
                mprf("%s %s skewered by barbed spikes.",
                     defender->name(DESC_THE).c_str(),
                     defender->conj_verb("are").c_str());
            }
            defender->as_monster()->add_ench(mon_enchant(ENCH_BARBS, attacker,
                                        random_range(5, 7) * BASELINE_DELAY, 1));
        }
        break;

    case AF_MINIPARA:
    {
        // Doesn't affect the poison-immune.
        if (defender->is_player() && you.duration[DUR_DIVINE_STAMINA] > 0)
        {
            mpr("Your divine stamina protects you from poison!");
            break;
        }
        if (defender->res_poison() >= 3)
            break;
        if (defender->res_poison() > 0 && !one_chance_in(3))
            break;
        defender->paralyse(attacker, 1);
        mons_do_poison();
        break;
    }

    case AF_POISON_PARALYSE:
    {
        // Doesn't affect the poison-immune.
        if (defender->is_player() && you.duration[DUR_DIVINE_STAMINA] > 0)
        {
            mpr("Your divine stamina protects you from poison!");
            break;
        }
        else if (defender->res_poison() >= 3)
            break;

        // Same frequency as AF_POISON and AF_POISON_STRONG.
        if (one_chance_in(3))
        {
            int dmg = random_range(attacker->get_hit_dice() * 3 / 2,
                                   attacker->get_hit_dice() * 5 / 2);
            defender->poison(attacker, dmg);
        }

        // Try to apply either paralysis or slowing, with the normal 2/3
        // chance to resist with rPois.
        if (one_chance_in(6))
        {
            if (defender->res_poison() <= 0 || one_chance_in(3))
                defender->paralyse(attacker, roll_dice(1, 3));
        }
        else if (defender->res_poison() <= 0 || one_chance_in(3))
            defender->slow_down(attacker, roll_dice(1, 3));

        break;
    }

    case AF_REACH_TONGUE:
    case AF_ACID:
        defender->splash_with_acid(attacker);
        break;

    case AF_CORRODE:
        defender->corrode(attacker, atk_name(DESC_THE).c_str());
        break;

    case AF_RIFT:
    case AF_DISTORT:
        distortion_affects_defender();
        break;

    case AF_RAGE:
        if (!one_chance_in(3) || !defender->can_go_berserk())
            break;

        if (needs_message)
        {
            mprf("%s %s %s!",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("infuriate").c_str(),
                 defender_name(true).c_str());
        }

        defender->go_berserk(false);
        break;

    case AF_CHAOTIC:
        obvious_effect = chaos_affects_actor(defender, attacker);
        break;

    case AF_STEAL:
        // Ignore monsters, for now.
        if (!defender->is_player())
            break;

        attacker->as_monster()->steal_item_from_player();
        break;

    case AF_HOLY:
        if (defender->holy_wrath_susceptible())
            special_damage = attk_damage * 0.75;

        if (needs_message && special_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("sear").c_str(),
                 defender_name(true).c_str(),
                 attack_strength_punctuation(special_damage).c_str());
        }
        break;

    case AF_FOUL_FLAME:
    {
        const int rff = defender->res_foul_flame();
        if (rff < 0)
            special_damage = attk_damage * 0.75;
        else if (rff < 3)
            special_damage = attk_damage / ((rff + 1) * 2);

        if (needs_message && special_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("sear").c_str(),
                 defender_name(true).c_str(),
                 attack_strength_punctuation(special_damage).c_str());
        }
        break;
    }

    case AF_ANTIMAGIC:

        // Apply extra stacks of the effect to monsters that have none.
        if (defender->is_monster()
            && !defender->as_monster()->has_ench(ENCH_ANTIMAGIC))
        {
            antimagic_affects_defender(attacker->get_hit_dice() * 18);
        }
        else
            antimagic_affects_defender(attacker->get_hit_dice() * 12);

        if (mons_genus(attacker->type) == MONS_VINE_STALKER
            && attacker->is_monster())
        {
            const bool spell_user = defender->antimagic_susceptible();

            if (you.can_see(*attacker) || you.can_see(*defender))
            {
                mprf("%s drains %s %s.",
                     attacker->name(DESC_THE).c_str(),
                     defender->pronoun(PRONOUN_POSSESSIVE).c_str(),
                     spell_user ? "magic" : "power");
            }

            monster* vine = attacker->as_monster();
            if (vine->has_ench(ENCH_ANTIMAGIC)
                && !defender->is_summoned() && !defender->is_firewood())
            {
                mon_enchant me = vine->get_ench(ENCH_ANTIMAGIC);
                vine->lose_ench_duration(me, random2(damage_done) + 1);
                simple_monster_message(*attacker->as_monster(),
                                       spell_user
                                       ? " looks very invigorated."
                                       : " looks invigorated.");
            }
        }
        break;

    case AF_PAIN:
        pain_affects_defender();
        break;

    case AF_ENSNARE:
        if (one_chance_in(3))
            defender->trap_in_web();
        break;

    case AF_CRUSH:
        // Works on 2/3 of hits
        if (one_chance_in(3))
            break;

        if (needs_message)
        {
            mprf("%s %s %s.",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("grab").c_str(),
                 defender_name(true).c_str());
        }
        attacker->start_constricting(*defender, CONSTRICT_MELEE);
        // if you got grabbed, interrupt stair climb and passwall
        if (defender->is_player())
            stop_delay(true);
        break;

    case AF_FLOOD:
        if (coinflip())
        {
            defender->floodify(attacker, random_range(30, 60),
                               attacker->type == MONS_VOID_OOZE ? "ooze" : "water");
        }
        break;

    case AF_PURE_FIRE:
        if (attacker->type == MONS_FIRE_VORTEX)
            attacker->as_monster()->suicide(-10);

        special_damage = defender->apply_ac(base_damage, 0, ac_type::half);
        special_damage = resist_adjust_damage(defender,
                                              BEAM_FIRE,
                                              special_damage);

        if (needs_message && special_damage)
        {
            mprf("%s %s %s!",
                    atk_name(DESC_THE).c_str(),
                    attacker->conj_verb("burn").c_str(),
                    defender_name(true).c_str());

            _print_resist_messages(defender, special_damage, BEAM_FIRE);
        }

        defender->expose_to_element(BEAM_FIRE, 2);
        break;

    case AF_DRAIN_SPEED:
        if (x_chance_in_y(3, 5))
            drain_defender_speed();
        break;

    case AF_VULN:
        if (one_chance_in(3))
            defender->strip_willpower(attacker, 20 + random2(20), !needs_message);
        break;

    case AF_SHADOWSTAB:
        attacker->as_monster()->del_ench(ENCH_INVIS, true);
        break;

    case AF_DROWN:
        if (attacker->type == MONS_DROWNED_SOUL)
            attacker->as_monster()->suicide(-1000);

        if (!defender->res_water_drowning())
        {
            special_damage = base_damage;
            special_damage_flavour = BEAM_WATER;
            kill_type = KILLED_BY_WATER;

            if (needs_message)
            {
                mprf("%s %s %s%s",
                    atk_name(DESC_THE).c_str(),
                    attacker->conj_verb("drown").c_str(),
                    defender_name(true).c_str(),
                    attack_strength_punctuation(special_damage).c_str());
            }
        }
        break;

    case AF_WEAKNESS:
        if (coinflip())
            defender->weaken(attacker, 12);
        break;

    case AF_DIM:
        if (!one_chance_in(3))
            defender->diminish(attacker, 12);
        break;

    case AF_SEAR:
        if (!one_chance_in(3))
            sear_defender();

        break;

    case AF_SPIDER:
    {
        if (!one_chance_in(3))
            break;

        if (summon_spider(*attacker, defender->pos(), SPELL_NO_SPELL,
                          attacker->get_hit_dice() * 12))
        {
            if (needs_message)
                mpr("A spider bursts forth from the wound!");
        }
        break;
    }
    case AF_HELL_HUNT:
    {
        if (one_chance_in(3))
            break;

        if (summon_hell_out_of_bat(*attacker, defender->pos()))
        {
            if (needs_message)
            {
                mprf("Faint brimstone surges around %s!",
                    defender_name(true).c_str());
            }
        }
        break;
    }

    case AF_SWARM:
        if (!defender->is_firewood())
            summon_swarm_clone(*attacker->as_monster(), defender->pos());
        break;

    case AF_BLOODZERK:
    {
        if (!defender->has_blood() || !attacker->can_go_berserk())
            break;

        monster* mon = attacker->as_monster();
        if (mon->has_ench(ENCH_MIGHT))
        {
            mon->del_ench(ENCH_MIGHT, true);
            mon->add_ench(mon_enchant(ENCH_BERSERK, mon, random_range(100, 200)));
            simple_monster_message(*mon, " enters a blood-rage!");
        }
        else
        {
            mon->add_ench(mon_enchant(ENCH_MIGHT, mon, random_range(100, 200)));
            simple_monster_message(*mon, " tastes blood and grows stronger!");
        }
        break;
    }
    case AF_SLEEP:
        if (!coinflip())
            break;
        if (attk_type == AT_SPORE)
        {
            if (defender->is_unbreathing())
                break;

            if (defender_visible)
            {
                mprf("%s %s engulfed in a cloud of soporific spores!",
                     defender->name(DESC_THE).c_str(),
                     defender->conj_verb("are").c_str());
            }
        }
        defender->put_to_sleep(attacker, random_range(3, 5) * BASELINE_DELAY);
        break;

    case AF_SLIMIFY:
        if (defender->can_polymorph())
        {
            if (monster* mon = defender->as_monster())
            {
                // Rather than adding a new status to track, just make it
                // a small chance- the only current living perma-allies have
                // rPolymorph, anyway.
                if (mon_can_be_slimified(mon) && one_chance_in(4))
                    slimify_monster(mon);
            }
            else if (you.form != transformation::jelly)
            {
                // Zin's protection blocks this from the start, since having the
                // passive will stop the transformation at the end.
                if (have_passive(passive_t::resist_polymorph))
                    simple_god_message(" protects your body from unnatural transformation!");
                else
                {
                    bool initial = you.duration[DUR_SLIMIFYING] == 0;
                    you.duration[DUR_SLIMIFYING] += random_range(70, 90);
                    if (you.duration[DUR_SLIMIFYING] >= 200)
                    {
                        you.duration[DUR_SLIMIFYING] = 0;
                        transform(20 + roll_dice(3, 10),
                                  transformation::jelly, true, false);
                        you.transform_uncancellable = true;
                    }
                    else
                    {
                        mprf(MSGCH_DANGER, "You %s!",
                            initial ? "start to liquefy" : "liquefy further");
                    }
                }
            }
        }
        break;

    case AF_ALEMBIC:
    {
        if (needs_message)
            mprf("%s vents fumes.", attacker->name(DESC_THE).c_str());

        int dur = random_range(3, 7);
        place_cloud(CLOUD_POISON, defender->pos(), dur, attacker);

        if (coinflip())
        {
            const int num_clouds = random_range(3, 4);
            int placed = 0;
            for (fair_adjacent_iterator ai(defender->pos()); ai && (placed < num_clouds); ++ai)
            {
                if ((!actor_at(*ai) || !mons_aligned(attacker, actor_at(*ai)))
                    && place_cloud(CLOUD_POISON, *ai, dur, attacker))
                {
                    ++placed;
                }
            }
        }

        // No brewing potions via punching plants.
        if (!defender || defender->is_firewood())
            break;

        if (--attacker->as_monster()->number == 0)
            alembic_brew_potion(*attacker->as_monster());
    }
    break;

    case AF_BOMBLET:
        monarch_deploy_bomblet(*attacker->as_monster(), defender->pos());
        break;

    case AF_AIRSTRIKE:
    {
        const int spaces = airstrike_space_around(defender->pos(), true);
        const int min = pow(attacker->get_hit_dice(), 1.2) * (spaces + 2) / 9;
        const int max = pow(attacker->get_hit_dice() + 1, 1.2) * (spaces + 4) / 6;
        special_damage = defender->apply_ac(random_range(min, max), 0);

        if (needs_message && special_damage)
        {
            // XXX: VFX during regular melee is distracting, but players not
            // respecting the mechanic will get quite bodied. Hrm.
            tileidx_t generic = TILE_BOLT_DEFAULT_WHITE;

            mprf("%s and strikes %s%s",
                 airstrike_intensity_display(spaces, generic).c_str(),
                 defender->name(DESC_THE).c_str(),
                 attack_strength_punctuation(special_damage).c_str());
        }
        break;
    }

    case AF_TRICKSTER:
    {
        if (!one_chance_in(3) || defender->is_player())
            break;

        monster* mdefender = defender->as_monster();
        vector<enchant_type> effects;

        if (!mdefender->has_ench(ENCH_DRAINED) && defender->res_negative_energy() < 3)
            effects.push_back(ENCH_DRAINED);
        if (!mdefender->has_ench(ENCH_CONFUSION) && !defender->clarity())
            effects.push_back(ENCH_CONFUSION);
        if (!mdefender->has_ench(ENCH_DAZED))
            effects.push_back(ENCH_DAZED);

        if (effects.empty())
            break;

        switch (effects[random2(effects.size())])
        {
            default:
            case ENCH_DAZED:
                defender->daze(random_range(1, 2));
                break;
            case ENCH_DRAINED:
                defender->drain(attacker, false, 2);
                break;
            case ENCH_CONFUSION:
                defender->confuse(attacker, 10);
                break;
        }

        break;
    }

    case AF_DOOM:
    {
        const int amount = random_range(5 + attacker->get_hit_dice() / 3,
                                        8 + attacker->get_hit_dice());
        const bool caused_bane = defender->doom(amount);
        if (!caused_bane && defender->is_player())
            mpr("Your doom draws closer.");
        break;
    }

    }

    if (needs_message && !special_damage_message.empty())
        mpr(special_damage_message);

    if (special_damage > 0 && defender->alive())
    {
        inflict_damage(special_damage, special_damage_flavour);
        special_damage = 0;
        special_damage_message.clear();
        special_damage_flavour = BEAM_NONE;
    }
}

void melee_attack::do_fiery_armour_burn()
{
    if (!you.duration[DUR_FIERY_ARMOUR]
        || !attacker->alive()
        || !adjacent(you.pos(), attacker->pos()))
    {
        return;
    }

    bolt beam;
    beam.flavour = BEAM_FIRE;
    beam.thrower = KILL_YOU;

    monster* mon = attacker->as_monster();

    const int pre_ac = roll_dice(2, 4);
    const int post_ac = attacker->apply_ac(pre_ac);
    // Don't display the adjusted messages...
    const int hurted = mons_adjust_flavoured(mon, beam, post_ac, false);

    if (!hurted)
        return;

    simple_monster_message(*mon, " is burned by your cloak of flames.");

    // ...until now, since we know damage was done.
    mons_adjust_flavoured(mon, beam, post_ac);

    mon->hurt(&you, hurted);

    if (mon->alive())
    {
        mon->expose_to_element(BEAM_FIRE, post_ac);
        print_wounds(*mon);
    }
}

void melee_attack::do_passive_freeze()
{
    if (you.has_mutation(MUT_PASSIVE_FREEZE)
        && attacker->alive()
        && adjacent(you.pos(), attacker->as_monster()->pos()))
    {
        bolt beam;
        beam.flavour = BEAM_COLD;
        beam.thrower = KILL_YOU;

        monster* mon = attacker->as_monster();

        const int orig_hurted = random2(11);
        // Don't display the adjusted messages...
        const int hurted = mons_adjust_flavoured(mon, beam, orig_hurted, false);

        if (!hurted)
            return;

        simple_monster_message(*mon, " is very cold.");

        // ...until now, since we know damage was done.
        mons_adjust_flavoured(mon, beam, orig_hurted);

        mon->hurt(&you, hurted);

        if (mon->alive())
        {
            mon->expose_to_element(BEAM_COLD, orig_hurted, &you);
            print_wounds(*mon);
        }
    }
}

void melee_attack::mons_do_eyeball_confusion()
{
    if (you.has_mutation(MUT_EYEBALLS)
        && attacker->alive()
        && adjacent(you.pos(), attacker->as_monster()->pos())
        && x_chance_in_y(you.get_mutation_level(MUT_EYEBALLS), 20))
    {
        const int ench_pow = you.get_mutation_level(MUT_EYEBALLS) * 30;
        monster* mon = attacker->as_monster();

        if (mon->check_willpower(&you, ench_pow) <= 0)
        {
            mprf("The eyeballs on your body gaze at %s.",
                 mon->name(DESC_THE).c_str());

            if (!mon->clarity())
                mon->add_ench(mon_enchant(ENCH_CONFUSION, &you, 30 + random2(100)));
        }
    }
}

void melee_attack::mons_do_tendril_disarm()
{
    monster* mon = attacker->as_monster();

    if (you.get_mutation_level(MUT_TENDRILS)
        && one_chance_in(5)
        && !x_chance_in_y(mon->get_hit_dice(), 35))
    {
        item_def* mons_wpn = mon->disarm();
        if (mons_wpn)
        {
            mprf("Your tendrils lash around %s %s and pull it to the ground!",
                 apostrophise(mon->name(DESC_THE)).c_str(),
                 mons_wpn->name(DESC_PLAIN).c_str());
        }
    }
}

void melee_attack::do_spines()
{
    // Monsters only get struck on their first attack per round
    if (attacker->is_monster() && effective_attack_number > 0)
        return;

    if (defender->is_player())
    {
        const int mut = you.get_mutation_level(MUT_SPINY);

        if (mut && attacker->alive() && coinflip())
        {
            int dmg = random_range(mut,
                div_rand_round(you.experience_level * 2, 3) + mut * 3);
            int hurt = attacker->apply_ac(dmg);

            dprf(DIAG_COMBAT, "Spiny: dmg = %d hurt = %d", dmg, hurt);

            if (hurt > 0)
            {
                simple_monster_message(*attacker->as_monster(),
                                    " is struck by your spines.");

                attacker->hurt(&you, hurt);
            }
        }

        if (you.form == transformation::quill && attacker->alive() && x_chance_in_y(2, 5))
        {
            int dmg = attacker->apply_ac(get_form()->get_special_damage().roll());
            int hurt = attacker->apply_ac(dmg);
            if (hurt > 0)
            {
                simple_monster_message(*attacker->as_monster(),
                                       " is struck by your quills.");

                attacker->hurt(&you, hurt);
            }
        }
    }
    else if (defender->as_monster()->is_spiny())
    {
        // Thorn hunters can attack their own brambles without injury
        if (defender->type == MONS_BRIAR_PATCH
            && attacker->type == MONS_THORN_HUNTER
            // Don't let spines kill things out of LOS.
            || !monster_los_is_valid(defender->as_monster(), attacker))
        {
            return;
        }

        const bool cactus = defender->type == MONS_CACTUS_GIANT;
        if (attacker->alive() && (cactus || one_chance_in(3)))
        {
            const int dmg = spines_damage(defender->type).roll();
            int hurt = attacker->apply_ac(dmg);
            dprf(DIAG_COMBAT, "Spiny: dmg = %d hurt = %d", dmg, hurt);

            if (hurt <= 0)
                return;
            if (you.can_see(*defender) || attacker->is_player())
            {
                mprf("%s %s struck by %s %s.", attacker->name(DESC_THE).c_str(),
                     attacker->conj_verb("are").c_str(),
                     defender->name(DESC_ITS).c_str(),
                     defender->type == MONS_BRIAR_PATCH ? "thorns"
                                                        : "spines");
            }
            attacker->hurt(defender, hurt, BEAM_MISSILE, KILLED_BY_SPINES);
        }
    }
}

void melee_attack::do_foul_flame()
{
    monster* mon = attacker->as_monster();

    if (you.has_mutation(MUT_FOUL_SHADOW)
        && attacker->alive()
        && attacker->res_foul_flame() < 3
        && adjacent(you.pos(), mon->pos()))
    {
        const int mut = you.get_mutation_level(MUT_FOUL_SHADOW);

        if (damage_done > 0 && x_chance_in_y(mut * 3, 20))
        {
            const int raw_dmg = random_range(mut,
                div_rand_round(you.experience_level * 3, 4) + mut * 4);

            bolt beam;
            beam.flavour = BEAM_FOUL_FLAME;
            beam.thrower = KILL_YOU;
            // Don't display the adjusted messages...
            const int dmg = mons_adjust_flavoured(mon, beam, raw_dmg, false);

            if (!dmg)
                return;

            dprf(DIAG_COMBAT, "Foul flame damage: raw_dmg %d, dmg %d", raw_dmg,
                 dmg);
            mprf("%s is seared by the foul flame within you%s",
                 attacker->name(DESC_THE).c_str(),
                 attack_strength_punctuation(dmg).c_str());

            // ...until now, since we know damage was done.
            mons_adjust_flavoured(mon, beam, raw_dmg);

            mon->hurt(&you, dmg);

            if (mon->alive())
                print_wounds(*mon);
        }
    }
}

void melee_attack::emit_foul_stench()
{
    monster* mon = attacker->as_monster();

    if (you.has_mutation(MUT_FOUL_STENCH)
        && attacker->alive()
        && adjacent(you.pos(), mon->pos()))
    {
        const int mut = you.get_mutation_level(MUT_FOUL_STENCH);

        if (damage_done > 0 && x_chance_in_y(mut * 3 - 1, 20))
        {
            mpr("You emit a cloud of foul miasma!");
            place_cloud(CLOUD_MIASMA, mon->pos(), 5 + random2(6), &you);
        }
    }
}

/** For UNRAND_STARLIGHT's dazzle effect, currently only when worn by the player.
 */
void melee_attack::do_starlight()
{
    static const vector<string> dazzle_msgs = {
        "@The_monster@ is blinded by the light from your cloak!",
        "@The_monster@ is temporarily struck blind!",
        "@The_monster_possessive@ sight is seared by the starlight!",
        "@The_monster_possessive@ vision is obscured by starry radiance!",
    };

    if (attacker->is_monster()
        && attacker->res_blind() <= 1
        && !attacker->as_monster()->has_ench(ENCH_BLIND)
        && adjacent(attacker->pos(), defender->pos())
        && x_chance_in_y(min(50, (95 - defender->get_hit_dice() * 4) / 5), 50))
    {
        attacker->as_monster()->add_ench(mon_enchant(ENCH_BLIND, &you,
                                         random_range(4, 8) * BASELINE_DELAY));

        string msg = *random_iterator(dazzle_msgs);
        msg = do_mon_str_replacements(msg, *attacker->as_monster(), S_SILENT);
        mpr(msg);
    }
}


/**
 * Launch a counterattack against the attacker. No sanity checks;
 * caller beware!
 */
void melee_attack::riposte()
{
    if (you.see_cell(defender->pos()))
    {
        mprf("%s riposte%s.", defender->name(DESC_THE).c_str(),
             defender->is_player() ? "" : "s");
    }
    melee_attack attck(defender, attacker, 0, effective_attack_number + 1);
    attck.is_riposte = true;
    attck.launch_attack_set(true);
    count_action(CACT_ATTACK, ATTACK_RIPOSTE);
    you.track_reprisal(REPRISAL_FENCER, attacker->mid);
}

void melee_attack::do_valour_beam()
{
    // Beams only happen at >80% HP (which corresponds to 'moderately damaged'
    // and below for monsters.)
    if (attacker->stat_hp() <= attacker->stat_maxhp() * 4 / 5)
        return;

    // No beams for hitting allies or firewood.
    if (mons_aligned(attacker, defender) || defender->is_firewood())
        return;

    // Find a ray that starts at the defender, but from an angle relative to the
    // attacker (so that it plays nicely with Manifold Assault).
    ray_def ray;
    if (!find_ray(attacker->pos(), defender->pos(), ray, opc_no_trans))
        return;

    bool found_targ = false;
    while (ray.advance())
    {
        if (ray.pos() == defender->pos())
        {
            found_targ = true;
            ray.regress();
            break;
        }
    }

    if (!found_targ)
        return;

    bolt beam;
    zappy(ZAP_VALOUR_BEAM, weapon_damage(), attacker->is_monster(), beam);
    beam.set_agent(attacker);
    beam.source = attacker->pos();
    beam.target = defender->pos();
    beam.range = 4;
    beam.stop_at_allies = true;
    beam.attitude = attacker->temp_attitude();
    beam.ray = ray;
    beam.chose_ray = true;

    // The player can fire a lot of these when they're playing, but it's helpful
    // not to gloss over the rare enemy using it.
    if (attacker->is_player())
        beam.draw_delay = 5;

    beam.fire();
}

bool melee_attack::do_knockback(bool slippery)
{
    if (defender->is_stationary())
        return false; // don't even print a message

    const int size_diff =
        attacker->body_size(PSIZE_BODY) - defender->body_size(PSIZE_BODY);
    const coord_def old_pos = defender->pos();
    const coord_def new_pos = old_pos + old_pos - attack_position;

    if (!slippery && !x_chance_in_y(size_diff + 3, 6)
        // need a valid tile
        || !defender->is_habitable(new_pos)
        // don't trample anywhere the attacker can't follow
        || !attacker->is_habitable(old_pos)
        // don't trample into a monster - or do we want to cause a chain
        // reaction here?
        || actor_at(new_pos)
        // Prevent trample/drown combo when flight is expiring
        || defender->is_player() && need_expiration_warning(new_pos)
        || defender->is_constricted()
        || defender->resists_dislodge(needs_message ? "being knocked back" : ""))
    {
        if (needs_message)
        {
            if (defender->is_constricted())
            {
                mprf("%s %s held in place!",
                     defender_name(false).c_str(),
                     defender->conj_verb("are").c_str());
            }
            else if (!slippery)
            {
                mprf("%s %s %s ground!",
                     defender_name(false).c_str(),
                     defender->conj_verb("hold").c_str(),
                     defender->pronoun(PRONOUN_POSSESSIVE).c_str());
            }
        }

        return false;
    }

    if (needs_message)
    {
        const bool can_stumble = !defender->airborne()
                                  && !defender->incapacitated();
        const string verb = slippery ? "slip" :
                         can_stumble ? "stumble"
                                     : "are shoved";
        mprf("%s %s backwards!",
             defender_name(false).c_str(),
             defender->conj_verb(verb).c_str());
    }

    // Schedule following _before_ actually trampling -- if the defender
    // is a player, a shaft trap will unload the level. If trampling will
    // somehow fail, move attempt will be ignored.
    if (!attacker->cannot_move())
        schedule_trample_follow_fineff(attacker, old_pos);
    defender->move_to(new_pos);
    return true;
}

bool melee_attack::do_drag()
{
    if (defender->is_stationary())
        return false; // don't even print a message

    // Calculate what is 'backwards' from the attacker's perspective, relative
    // to the defender. (Remember, this can occur on reaching attacks, so
    // attacker and defender may be non-adjacent!)
    const coord_def drag_shift = -(defender->pos() - attacker->pos()).sgn();
    const coord_def new_defender_pos = defender->pos() + drag_shift;

    // Only move the attacker back if the defender was already adjacent and we
    // want to move them *into* the attacker's space.
    bool move_attacker = new_defender_pos == attacker->pos();

    // Immobile monsters can drag things towards them with reaching weapons,
    // but cannot drag if that would require also moving themselves.
    if (move_attacker && attacker->cannot_move())
        return false;

    coord_def new_attacker_pos = (move_attacker ? attacker->pos() + drag_shift
                                                : attacker->pos());

    // Abort if there isn't habitable empty space at the endpoints of both the
    // attacker and defender's move, or if something else is interfering with it.
    if (move_attacker && (!attacker->is_habitable(new_attacker_pos)
                         || actor_at(new_attacker_pos))
        || !defender->is_habitable(new_defender_pos)
        || (new_defender_pos != attacker->pos() && actor_at(new_defender_pos))
        || defender->is_player() && need_expiration_warning(new_defender_pos)
        || defender->is_constricted()
        || defender->resists_dislodge(needs_message ? "being dragged" : ""))
    {
        return false;
    }

    // We should be okay to move, then.
    if (needs_message)
    {
        mprf("%s drags %s backwards!",
             attacker->name(DESC_THE).c_str(),
             defender_name(true).c_str());
    }

    // Only move the attacker back if the defender was already adjacent and we
    // want to move them *into* the attacker's space.
    if (new_defender_pos == attacker->pos())
        attacker->move_to(new_attacker_pos, MV_DELIBERATE);
    defender->move_to(new_defender_pos);

    return true;
}

/**
 * Find the list of targets to cleave after hitting the main target and save it.
 */
void melee_attack::cleave_setup()
{
    // Only perform setup on the 'initial' attack
    if (is_followup || is_projected || never_cleave || cleaving)
        return;

    // Also, don't cleave on a self-attack.
    if (defender && defender->pos() == attacker->pos())
        return;

    // WJC AOEs mayn't cleave.
    if (wu_jian_attack == WU_JIAN_ATTACK_WHIRLWIND
        || wu_jian_attack == WU_JIAN_ATTACK_WALL_JUMP
        || wu_jian_attack == WU_JIAN_ATTACK_TRIGGERED_AUX)
    {
        return;
    }

    // If sunder will trigger, we need to apply it before finding cleave targets.
    if (is_sundering_weapon() && attacker->sunder_is_ready())
    {
        is_sunder = true;
        to_hit_bonus = 12;
        dmg_mult = attacker->is_monster() ? 150 : 200;

        if (attacker->is_player())
            you.attribute[ATTR_SUNDERING_CHARGE] = -1;
        else
            attacker->as_monster()->del_ench(ENCH_SUNDER_CHARGE);
    }

    // We need to get the list of the remaining potential targets now because
    // if the main target dies, its position will be lost.
    get_cleave_targets(*attacker, defender ? defender->pos() : coord_def(),
                       cleave_targets, attack_number, false, weapon,
                       is_sunder ? 1 : 0);

    // We're already attacking this guy.
    if (defender)
        cleave_targets.pop_front();
}

// cleave damage modifier for additional attacks: 70% of base damage
int melee_attack::cleave_damage_mod(int dam)
{
    return div_rand_round(dam * 7, 10);
}

// Martial strikes get modified by momentum and maneuver specific damage mods.
int melee_attack::martial_damage_mod(int dam)
{
    if (wu_jian_has_momentum(wu_jian_attack))
        dam = div_rand_round(dam * 14, 10);

    if (wu_jian_attack == WU_JIAN_ATTACK_LUNGE)
        dam = div_rand_round(dam * 12, 10);

    if (wu_jian_attack == WU_JIAN_ATTACK_WHIRLWIND)
        dam = div_rand_round(dam * 8, 10);

    return dam;
}

/**
 * Does the player get to use the given aux attack during this melee attack?
 *
 * Partially random.
 *
 * @param atk   The type of aux attack being considered.
 * @return      Whether the player may use the given aux attack.
 */
bool melee_attack::_extra_aux_attack(unarmed_attack_type atk)
{
    ASSERT(atk >= UNAT_FIRST_ATTACK);
    ASSERT(atk <= UNAT_LAST_ATTACK);
    const AuxAttackType* const aux = aux_attack_types[atk - UNAT_FIRST_ATTACK];

    // Does the player even qualify to use this type of aux?
    if (!aux->is_usable())
        return false;

    if (!x_chance_in_y(aux->get_chance(), 100))
        return false;

    // Maw bites are aut-normalized (so that one will happen every 15 aut on average)
    if (atk == UNAT_MAW)
    {
        if (is_followup)
            return false;
        if (!x_chance_in_y(you.attack_delay().roll(), 15))
            return false;
    }

    if (wu_jian_attack != WU_JIAN_ATTACK_NONE
        && !x_chance_in_y(1, wu_jian_number_of_targets))
    {
       // Reduces aux chance proportionally to number of
       // enemies attacked with a martial attack
       return false;
    }

    return true;
}

bool melee_attack::using_weapon() const
{
    return weapon && is_melee_weapon(*weapon);
}

int melee_attack::weapon_damage() const
{
    if (!using_weapon())
        return 0;

    return property(*weapon, PWPN_DAMAGE);
}

int melee_attack::calc_mon_to_hit_base()
{
    const bool fighter = attacker->is_monster()
                         && attacker->as_monster()->is_fighter();
    return mon_to_hit_base(attacker->get_hit_dice(), fighter);
}

/**
 * Add modifiers to a monster's base damage.
 */
int melee_attack::apply_mon_damage_modifiers(int damage)
{
    ASSERT(attacker->is_monster());
    monster *as_mon = attacker->as_monster();

    // Berserk/mighted monsters get bonus damage.
    if (as_mon->has_ench(ENCH_MIGHT) || as_mon->berserk_or_frenzied())
        damage = damage * 3 / 2;

    if (as_mon->has_ench(ENCH_TEMPERED))
        damage = damage * 5 / 4;

    if (as_mon->has_ench(ENCH_IDEALISED))
        damage *= 2; // !

    if (as_mon->has_ench(ENCH_WEAK))
        damage = damage * 2 / 3;

    if (as_mon->has_ench(ENCH_TOUCH_OF_BEOGH))
        damage = damage * 4 / 3;

    if (as_mon->has_ench(ENCH_FIGMENT))
        damage = damage * 2 / 3;

    // If the defender is asleep, the attacker gets a stab.
    if (defender && (defender->asleep()
                     || (attk_flavour == AF_SHADOWSTAB
                         &&!defender->can_see(*attacker))))
    {
        if (mons_is_player_shadow(*attacker->as_monster())
            && player_good_stab())
        {
            is_special_mon_stab = true;
            damage += you.experience_level * 2 / 3;
        }
        else if (as_mon->has_ench(ENCH_VAMPIRE_THRALL))
        {
            is_special_mon_stab = true;
            damage += as_mon->get_hit_dice() * 3 / 2;
        }

        damage = damage * 5 / 2;
        dprf(DIAG_COMBAT, "Stab damage vs %s: %d",
             defender->name(DESC_PLAIN).c_str(),
             damage);
    }

    if (cleaving && !is_sunder)
        damage = cleave_damage_mod(damage);

    if (dmg_mult)
        damage = damage * (100 + dmg_mult) / 100;

    damage += flat_dmg_bonus;

    return damage;
}

int melee_attack::calc_damage()
{
    // Constriction deals damage over time, not when grabbing.
    if (attk_flavour == AF_CRUSH)
        return 0;

    return attack::calc_damage();
}

bool melee_attack::apply_damage_brand(const char *what)
{
    // Staff damage overrides any brands
    return apply_staff_damage() || attack::apply_damage_brand(what);
}

bool melee_attack::is_sundering_weapon() const
{
    return damage_brand == SPWPN_SUNDERING;
}

string mut_aux_attack_desc(mutation_type mut)
{
    // XXX: dedup with _extra_aux_attack()
    switch (mut)
    {
    case MUT_HOOVES:
    case MUT_TALONS:
    case MUT_TENTACLE_SPIKE:
        return AUX_KICK.describe();
    case MUT_BEAK:
        return AUX_PECK.describe();
    case MUT_HORNS:
        return AUX_HEADBUTT.describe();
    case MUT_STINGER:
    case MUT_ARMOURED_TAIL:
    case MUT_WEAKNESS_STINGER:
    case MUT_MERTAIL:
        return AUX_TAIL.describe();
    case MUT_ACIDIC_BITE:
    case MUT_ANTIMAGIC_BITE:
    case MUT_FANGS:
        return AUX_BITE.describe();
    case MUT_PSEUDOPODS:
        return AUX_PSEUDOPODS.describe();
    case MUT_DEMONIC_TOUCH:
        return AUX_TOUCH.describe();
    case MUT_REFLEXIVE_HEADBUTT:
        return make_stringf("\nTrigger chance:  %d%%\n"
                              "Base damage:     %d\n\n",
                            _minotaur_headbutt_chance(),
                            AUX_HEADBUTT.get_damage(false));
    case MUT_MAKHLEB_MARK_EXECUTION:
        return AUX_EXECUTIONER_BLADE.describe();
    default:
        return "";
    }
}

static string _desc_aux(int chance, int dam)
{
    return make_stringf("\nTrigger chance:  %d%%\n"
                          "Base damage:     %d",
                        chance,
                        dam);
}

string aux_attack_desc(unarmed_attack_type unat, int force_damage)
{
    const unsigned long idx = unat - UNAT_FIRST_ATTACK;
    ASSERT_RANGE(idx, 0, ARRAYSZ(aux_attack_types));
    const AuxAttackType* const aux = aux_attack_types[idx];
    const int dam = force_damage == -1 ? aux->get_damage(false) : force_damage;
    // lazily assume chance and to hit don't vary in/out of forms
    return _desc_aux(aux->get_chance(), dam);
}

string AuxAttackType::describe() const
{
    return _desc_aux(get_chance(), get_damage(false)) + "\n\n";
}

vector<string> get_player_aux_names()
{
    vector<string> names;
    for (int i = UNAT_FIRST_ATTACK; i <= UNAT_LAST_ATTACK; ++i)
    {
        const AuxAttackType* const aux = aux_attack_types[i - 1];
        if (aux->is_usable())
            names.push_back(aux->get_name());
    }

    return names;
}

bool coglin_spellmotor_attack()
{
    // Only operates for melee attacks.
    if (you.weapon() && is_range_weapon(*you.weapon()))
        return false;

    // Never let this give an effective speed boost for attacking. If the
    // attack would take longer than a spell, give a proportional chance of not
    // launching the attack at all.
    int delay = you.attack_delay().roll();
    if (delay > 10 && !x_chance_in_y(10, delay))
        return false;

    // Gather all possible targets in attack range.
    // (We have to manually add aqua form's reaching bonus, since it normally
    // doesn't apply to cleaving attacks.)
    list<actor*> targets;
    get_cleave_targets(you, coord_def(), targets, -1, true, nullptr,
                       you.form == transformation::aqua ? 2 : 0);

    // Test that we have at least one valid non-prompting attack
    vector<actor*> targs;
    for (actor* victim : targets)
    {
        if (victim->is_firewood())
            continue;

        melee_attack attk(&you, victim);
        if (!attk.would_prompt_player())
            targs.push_back(victim);
    }

    if (targs.empty())
        return false;

    melee_attack attk(&you, targs[random2(targs.size())]);
    mpr("Your spellmotor activates!");
    attk.launch_attack_set();

    count_action(CACT_ATTACK, ATTACK_SPELLMOTOR);

    return true;
}

const static int spellclaws_level_mult[] = {5, 15, 25, 40, 55, 70, 90, 110, 150};
bool spellclaws_attack(int spell_level)
{
    // Only operates for melee attacks.
    if (you.weapon() && is_range_weapon(*you.weapon()))
        return false;

    // Struggle against nets rather than ignore them.
    if (you.caught())
    {
        you.struggle_against_net();
        return false;
    }

    // Gather all possible targets in attack range
    list<actor*> targets;
    get_cleave_targets(you, coord_def(), targets, -1, true);

    // Then choose the one with the *most* current health (that wouldn't cause
    // a warning prompt for some reason).
    int most_hp = 0;
    actor* best_victim = nullptr;
    for (actor* victim : targets)
    {
        if (victim->is_firewood())
            continue;

        if (victim->stat_hp() <= most_hp)
            continue;

        melee_attack attk(&you, victim);
        if (attk.would_prompt_player())
            continue;

        most_hp = victim->stat_hp();
        best_victim = victim;
    }

    if (!best_victim)
        return false;

    melee_attack attk(&you, best_victim);

    // If an attack would take more time than casting a spell, reduce its damage
    // proportionally.
    int mult = 100;
    int delay = you.attack_delay().roll();
    if (delay > 10)
        mult = 1000 / delay;

    if (you.duration[DUR_ENKINDLED])
    {
        attk.to_hit = AUTOMATIC_HIT;
        mult += mult * spellclaws_level_mult[spell_level - 1] / 100;
    }

    attk.dmg_mult = mult - 100;

    // Save name first, in case the monster dies from the attack.
    string targ_name = best_victim->name(DESC_THE);
    attk.launch_attack_set();

    count_action(CACT_ATTACK, ATTACK_SPELLCLAWS);

    if (you.duration[DUR_ENKINDLED] && you.hp < you.hp_max)
    {
        mprf("You rip the existence from %s to re-knit yourself!", targ_name.c_str());
        you.heal(attk.total_damage_done * 3 / 4);
    }

    return true;
}
