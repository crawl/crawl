/**
 * @file
 * @brief Brand/ench/etc effects that might alter something in an
 *             unexpected way.
**/

#include "AppHdr.h"

#include "fineff.h"

#include "act-iter.h"
#include "attitude-change.h"
#include "beam.h"
#include "bloodspatter.h"
#include "coordit.h"
#include "dactions.h"
#include "death-curse.h"
#include "delay.h" // stop_delay
#include "directn.h"
#include "english.h"
#include "env.h"
#include "fight.h"
#include "god-abil.h"
#include "god-companions.h"
#include "god-wrath.h" // lucy_check_meddling
#include "libutil.h"
#include "losglobal.h"
#include "melee-attack.h"
#include "message.h"
#include "mon-abil.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "movement.h"
#include "ouch.h"
#include "religion.h"
#include "spl-damage.h"
#include "spl-monench.h"
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "transform.h"
#include "view.h"

/*static*/ void final_effect::schedule(final_effect *eff)
{
    for (auto fe : env.final_effects)
    {
        if (fe->mergeable(*eff))
        {
            fe->merge(*eff);
            delete eff;
            return;
        }
    }
    env.final_effects.push_back(eff);
}

bool mirror_damage_fineff::mergeable(const final_effect &fe) const
{
    const mirror_damage_fineff *o =
        dynamic_cast<const mirror_damage_fineff *>(&fe);
    return o && att == o->att && def == o->def;
}

bool anguish_fineff::mergeable(const final_effect &fe) const
{
    const anguish_fineff *o =
        dynamic_cast<const anguish_fineff *>(&fe);
    return o && att == o->att;
}

bool ru_retribution_fineff::mergeable(const final_effect &fe) const
{
    const ru_retribution_fineff *o =
        dynamic_cast<const ru_retribution_fineff *>(&fe);
    return o && att == o->att && def == o->def;
}

bool trample_follow_fineff::mergeable(const final_effect &fe) const
{
    const trample_follow_fineff *o =
        dynamic_cast<const trample_follow_fineff *>(&fe);
    return o && att == o->att && posn == o->posn;
}

bool blink_fineff::mergeable(const final_effect &fe) const
{
    const blink_fineff *o = dynamic_cast<const blink_fineff *>(&fe);
    return o && def == o->def && att == o->att;
}

bool teleport_fineff::mergeable(const final_effect &fe) const
{
    const teleport_fineff *o =
        dynamic_cast<const teleport_fineff *>(&fe);
    return o && def == o->def;
}

bool trj_spawn_fineff::mergeable(const final_effect &fe) const
{
    const trj_spawn_fineff *o = dynamic_cast<const trj_spawn_fineff *>(&fe);
    return o && att == o->att && def == o->def && posn == o->posn;
}

bool blood_fineff::mergeable(const final_effect &fe) const
{
    const blood_fineff *o = dynamic_cast<const blood_fineff *>(&fe);
    return o && posn == o->posn && mtype == o->mtype;
}

bool deferred_damage_fineff::mergeable(const final_effect &fe) const
{
    const deferred_damage_fineff *o = dynamic_cast<const deferred_damage_fineff *>(&fe);
    return o && att == o->att && def == o->def
           && attacker_effects == o->attacker_effects && fatal == o->fatal;
}

bool starcursed_merge_fineff::mergeable(const final_effect &fe) const
{
    const starcursed_merge_fineff *o = dynamic_cast<const starcursed_merge_fineff *>(&fe);
    return o && def == o->def;
}

bool shock_discharge_fineff::mergeable(const final_effect &fe) const
{
    const shock_discharge_fineff *o = dynamic_cast<const shock_discharge_fineff *>(&fe);
    return o && def == o->def;
}

bool delayed_action_fineff::mergeable(const final_effect &) const
{
    return false;
}

bool rakshasa_clone_fineff::mergeable(const final_effect &fe) const
{
    const rakshasa_clone_fineff *o =
        dynamic_cast<const rakshasa_clone_fineff *>(&fe);
    return o && att == o->att && def == o->def && posn == o->posn;
}

bool summon_dismissal_fineff::mergeable(const final_effect &fe) const
{
    const summon_dismissal_fineff *o =
        dynamic_cast<const summon_dismissal_fineff *>(&fe);
    return o && def == o->def;
}

void mirror_damage_fineff::merge(const final_effect &fe)
{
    const mirror_damage_fineff *mdfe =
        dynamic_cast<const mirror_damage_fineff *>(&fe);
    ASSERT(mdfe);
    ASSERT(mergeable(*mdfe));
    damage += mdfe->damage;
}

void anguish_fineff::merge(const final_effect &fe)
{
    const anguish_fineff *afe =
        dynamic_cast<const anguish_fineff *>(&fe);
    ASSERT(afe);
    ASSERT(mergeable(*afe));
    damage += afe->damage;
}

void ru_retribution_fineff::merge(const final_effect &fe)
{
    const ru_retribution_fineff *mdfe =
        dynamic_cast<const ru_retribution_fineff *>(&fe);
    ASSERT(mdfe);
    ASSERT(mergeable(*mdfe));
}

void trj_spawn_fineff::merge(const final_effect &fe)
{
    const trj_spawn_fineff *trjfe =
        dynamic_cast<const trj_spawn_fineff *>(&fe);
    ASSERT(trjfe);
    ASSERT(mergeable(*trjfe));
    damage += trjfe->damage;
}

void blood_fineff::merge(const final_effect &fe)
{
    const blood_fineff *bfe = dynamic_cast<const blood_fineff *>(&fe);
    ASSERT(bfe);
    ASSERT(mergeable(*bfe));
    blood += bfe->blood;
}

void deferred_damage_fineff::merge(const final_effect &fe)
{
    const deferred_damage_fineff *ddamfe =
        dynamic_cast<const deferred_damage_fineff *>(&fe);
    ASSERT(ddamfe);
    ASSERT(mergeable(*ddamfe));
    damage += ddamfe->damage;
}

void shock_discharge_fineff::merge(const final_effect &fe)
{
    const shock_discharge_fineff *ssdfe =
        dynamic_cast<const shock_discharge_fineff *>(&fe);
    power += ssdfe->power;
}

void summon_dismissal_fineff::merge(const final_effect &)
{
    // no damage to accumulate, but no need to fire this more than once
    return;
}

void mirror_damage_fineff::fire()
{
    actor *attack = attacker();
    if (!attack || attack == defender() || !attack->alive())
        return;
    // defender being dead is ok, if we killed them we still suffer

    god_acting gdact(GOD_YREDELEMNUL); // XXX: remove?

    if (att == MID_PLAYER)
    {
        const monster* reflector = defender() ?
                                   defender()->as_monster() : nullptr;
        if (reflector)
        {
            mprf("%s reflects your damage back at you!",
                 reflector->name(DESC_THE).c_str());
        }
        else
            mpr("Your damage is reflected back at you!");
        ouch(damage, KILLED_BY_MIRROR_DAMAGE);
    }
    else
    {
        simple_monster_message(*monster_by_mid(att), " suffers a backlash!");
        attack->hurt(defender(), damage);
    }
}

void anguish_fineff::fire()
{
    actor *attack = attacker();
    if (!attack || !attack->alive())
        return;

    const string punct = attack_strength_punctuation(damage);
    const string msg = make_stringf(" is wracked by anguish%s", punct.c_str());
    simple_monster_message(*monster_by_mid(att), msg.c_str());
    attack->hurt(monster_by_mid(MID_YOU_FAULTLESS), damage);
}

void ru_retribution_fineff::fire()
{
    actor *attack = attacker();
    if (!attack || attack == defender() || !attack->alive())
        return;
    if (def == MID_PLAYER)
        ru_do_retribution(monster_by_mid(att), damage);
}

void trample_follow_fineff::fire()
{
    actor *attack = attacker();
    if (attack
        && attack->pos() != posn
        && adjacent(attack->pos(), posn)
        && attack->is_habitable(posn))
    {
        const coord_def old_pos = attack->pos();
        attack->move_to_pos(posn);
        attack->apply_location_effects(old_pos);
        attack->did_deliberate_movement();
    }
}

void blink_fineff::fire()
{
    actor *defend = defender();
    if (!defend || !defend->alive() || defend->no_tele())
        return;

    // if we're doing 'blink with', only blink if we have a partner
    actor *pal = attacker();
    if (pal && (!pal->alive() || pal->no_tele()))
        return;

    defend->blink();
    if (!defend->alive())
        return;

    // Is something else also getting blinked?
    if (!pal || !pal->alive() || pal->no_tele())
        return;

    int cells_seen = 0;
    coord_def target;
    for (fair_adjacent_iterator ai(defend->pos()); ai; ++ai)
    {
        // No blinking into teleport closets.
        if (testbits(env.pgrid(*ai), FPROP_NO_TELE_INTO))
            continue;
        // XXX: allow fedhasites to be blinked into plants?
        if (actor_at(*ai) || !pal->is_habitable(*ai))
            continue;
        cells_seen++;
        if (one_chance_in(cells_seen))
            target = *ai;
    }
    if (!cells_seen)
        return;

    pal->blink_to(target);
}

void teleport_fineff::fire()
{
    actor *defend = defender();
    if (defend && defend->alive() && !defend->no_tele())
        defend->teleport(true);
}

void trj_spawn_fineff::fire()
{
    const actor *attack = attacker();
    actor *trj = defender();

    int tospawn = div_rand_round(damage, 12);

    if (tospawn <= 0)
        return;

    dprf("Trying to spawn %d jellies.", tospawn);

    unsigned short foe = attack && attack->alive() ? attack->mindex() : MHITNOT;
    // may be ANON_FRIENDLY_MONSTER
    if (invalid_monster_index(foe) && foe != MHITYOU)
        foe = MHITNOT;

    // Give spawns the same attitude as TRJ; if TRJ is now dead, make them
    // hostile.
    const beh_type spawn_beh = trj
        ? attitude_creation_behavior(trj->as_monster()->attitude)
        : BEH_HOSTILE;

    // No permanent friendly jellies from a charmed TRJ.
    if (spawn_beh == BEH_FRIENDLY && !crawl_state.game_is_arena())
        return;

    int spawned = 0;
    for (int i = 0; i < tospawn; ++i)
    {
        const monster_type jelly = royal_jelly_ejectable_monster();
        coord_def jpos = find_newmons_square_contiguous(jelly, posn, 3, false);
        if (!in_bounds(jpos))
            continue;

        if (monster *mons = mons_place(
                              mgen_data(jelly, spawn_beh, jpos, foe,
                                        MG_DONT_COME, GOD_JIYVA)
                              .set_summoned(trj, 0)))
        {
            // Don't allow milking the Royal Jelly.
            mons->flags |= MF_NO_REWARD | MF_HARD_RESET;
            spawned++;
        }
    }

    if (!spawned || !you.see_cell(posn))
        return;

    if (trj)
    {
        const string monnam = trj->name(DESC_THE);
        mprf("%s shudders%s.", monnam.c_str(),
             spawned >= 5 ? " alarmingly" :
             spawned >= 3 ? " violently" :
             spawned > 1 ? " vigorously" : "");

        if (spawned == 1)
            mprf("%s spits out another jelly.", monnam.c_str());
        else
        {
            mprf("%s spits out %s more jellies.",
                 monnam.c_str(),
                 number_in_words(spawned).c_str());
        }
    }
    else if (spawned == 1)
        mpr("One of the Royal Jelly's fragments survives.");
    else
    {
        mprf("The dying Royal Jelly spits out %s more jellies.",
             number_in_words(spawned).c_str());
    }
}

void blood_fineff::fire()
{
    bleed_onto_floor(posn, mtype, blood, true);
}

void deferred_damage_fineff::fire()
{
    actor *df = defender();
    if (!df)
        return;

    // Once we actually apply damage to tentacle monsters,
    // remove their protection from further damage.
    if (df->props.exists(TENTACLE_LORD_HITS))
        df->props.erase(TENTACLE_LORD_HITS);

    if (!fatal)
    {
        // Cap non-fatal damage by the defender's hit points
        // FIXME: Consider adding a 'fatal' parameter to ::hurt
        //        to better interact with damage reduction/boosts
        //        which may be applied later.
        int df_hp = df->is_player() ? you.hp
                                    : df->as_monster()->hit_points;
        damage = min(damage, df_hp - 1);
    }

    df->hurt(attacker(), damage, BEAM_MISSILE, KILLED_BY_MONSTER, "", "",
             true, attacker_effects);
}

static void _do_merge_masses(monster* initial_mass, monster* merge_to)
{
    // Combine enchantment durations.
    merge_ench_durations(*initial_mass, *merge_to);

    merge_to->blob_size += initial_mass->blob_size;
    merge_to->max_hit_points += initial_mass->max_hit_points;
    merge_to->hit_points += initial_mass->hit_points;

    // Merge monster flags (mostly so that MF_CREATED_NEUTRAL, etc. are
    // passed on if the merged slime subsequently splits. Hopefully
    // this won't do anything weird.
    merge_to->flags |= initial_mass->flags;

    // Overwrite the state of the slime getting merged into, because it
    // might have been resting or something.
    merge_to->behaviour = initial_mass->behaviour;
    merge_to->foe = initial_mass->foe;

    behaviour_event(merge_to, ME_EVAL);

    // Have to 'kill' the slime doing the merging.
    monster_die(*initial_mass, KILL_RESET, NON_MONSTER, true);
}

void starcursed_merge_fineff::fire()
{
    actor *defend = defender();
    if (!defend || !defend->alive())
        return;

    monster *mon = defend->as_monster();

    // Find a random adjacent starcursed mass and merge with it.
    for (fair_adjacent_iterator ai(mon->pos()); ai; ++ai)
    {
        monster* mergee = monster_at(*ai);
        if (mergee && mergee->alive() && mergee->type == MONS_STARCURSED_MASS)
        {
            simple_monster_message(*mon,
                    " shudders and is absorbed by its neighbour.");
            _do_merge_masses(mon, mergee);
            return;
        }
    }

    // If there was nothing adjacent to merge with, at least try to move toward
    // another starcursed mass
    for (distance_iterator di(mon->pos(), true, true, LOS_RADIUS); di; ++di)
    {
        monster* ally = monster_at(*di);
        if (ally && ally->alive() && ally->type == MONS_STARCURSED_MASS
            && mon->can_see(*ally))
        {
            bool moved = false;

            coord_def sgn = (*di - mon->pos()).sgn();
            if (mon_can_move_to_pos(mon, sgn))
            {
                mon->move_to_pos(mon->pos()+sgn, false);
                moved = true;
            }
            else if (abs(sgn.x) != 0)
            {
                coord_def dx(sgn.x, 0);
                if (mon_can_move_to_pos(mon, dx))
                {
                    mon->move_to_pos(mon->pos()+dx, false);
                    moved = true;
                }
            }
            else if (abs(sgn.y) != 0)
            {
                coord_def dy(0, sgn.y);
                if (mon_can_move_to_pos(mon, dy))
                {
                    mon->move_to_pos(mon->pos()+dy, false);
                    moved = true;
                }
            }

            if (moved)
            {
                simple_monster_message(*mon, " shudders and withdraws towards its neighbour.");
                mon->speed_increment -= 10;
            }
        }
    }
}

void shock_discharge_fineff::fire()
{
    if (!oppressor.alive())
        return;

    const int max_range = 3; // v0v
    if (grid_distance(oppressor.pos(), position) > max_range
        || !cell_see_cell(position, oppressor.pos(), LOS_SOLID_SEE))
    {
        return;
    }

    const int amount = roll_dice(3, 4 + power * 3 / 2);
    int final_dmg = resist_adjust_damage(&oppressor, BEAM_ELECTRICITY, amount);
    final_dmg = oppressor.apply_ac(final_dmg, 0, ac_type::half);

    const actor *serpent = defender();
    if (serpent && you.can_see(*serpent))
    {
        mprf("%s %s discharges%s, shocking %s%s",
             serpent->name(DESC_ITS).c_str(),
             shock_source.c_str(),
             power < 4 ? "" : " violently",
             oppressor.name(DESC_THE).c_str(),
             attack_strength_punctuation(final_dmg).c_str());
    }
    else if (you.can_see(oppressor))
    {
        mprf("The air sparks with electricity, shocking %s%s",
             oppressor.name(DESC_THE).c_str(),
             attack_strength_punctuation(final_dmg).c_str());
    }

    bolt beam;
    beam.flavour = BEAM_ELECTRICITY;
    const string name = serpent && serpent->alive() ?
                        serpent->name(DESC_A, true) :
                        "a shock serpent"; // dubious
    oppressor.hurt(serpent, final_dmg, beam.flavour, KILLED_BY_BEAM,
                   name.c_str(), shock_source.c_str());

    // Do resist messaging
    if (oppressor.alive())
    {
        oppressor.beam_resists(beam, amount, true);
        if (final_dmg)
            oppressor.expose_to_element(beam.flavour, final_dmg);
    }
}

void explosion_fineff::fire()
{
    if (is_sanctuary(beam.target))
    {
        if (you.see_cell(beam.target))
            mprf(MSGCH_GOD, "%s", sanctuary_message.c_str());
        return;
    }

    if (you.see_cell(beam.target))
    {
        if (typ == EXPLOSION_FINEFF_CONCUSSION)
            mpr(boom_message);
        else
            mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s", boom_message.c_str());
    }

    if (typ == EXPLOSION_FINEFF_INNER_FLAME)
        for (adjacent_iterator ai(beam.target, false); ai; ++ai)
            if (!cell_is_solid(*ai) && !cloud_at(*ai) && !one_chance_in(5))
                place_cloud(CLOUD_FIRE, *ai, 10 + random2(10), flame_agent);

    beam.explode();

    if (typ == EXPLOSION_FINEFF_CONCUSSION)
    {
        for (fair_adjacent_iterator ai(beam.target); ai; ++ai)
        {
            actor *act = actor_at(*ai);
            if (!act
                || act->is_stationary()
                || act->is_monster() && never_harm_monster(&you, *act->as_monster()))
            {
                continue;
            }
            // TODO: dedup with knockback_actor in beam.cc

            const coord_def newpos = (*ai - beam.target) + *ai;
            if (newpos == *ai
                || cell_is_solid(newpos)
                || actor_at(newpos)
                || !act->can_pass_through(newpos)
                || !act->is_habitable(newpos))
            {
                continue;
            }

            act->move_to_pos(newpos);
            if (act->is_player())
                stop_delay(true);
            if (you.can_see(*act))
            {
                mprf("%s %s knocked back by the blast.",
                     act->name(DESC_THE).c_str(),
                     act->conj_verb("are").c_str());
            }

            act->apply_location_effects(*ai, beam.killer(),
                                        actor_to_death_source(beam.agent()));
        }
    }

    if (you.see_cell(beam.target) && !poof_message.empty())
        mprf(MSGCH_MONSTER_TIMEOUT, "%s", poof_message.c_str());
}

void delayed_action_fineff::fire()
{
    if (!final_msg.empty())
        mpr(final_msg);
    add_daction(action);
}

void kirke_death_fineff::fire()
{
    delayed_action_fineff::fire();

    // Revert the player last
    if (you.form == transformation::pig)
        return_to_default_form();
}

void rakshasa_clone_fineff::fire()
{
    actor *defend = defender();
    if (!defend)
        return;

    monster *rakshasa = defend->as_monster();
    ASSERT(rakshasa);

    // Using SPELL_NO_SPELL to prevent overwriting normal clones
    cast_phantom_mirror(rakshasa, rakshasa, 50, SPELL_NO_SPELL);
    cast_phantom_mirror(rakshasa, rakshasa, 50, SPELL_NO_SPELL);
    rakshasa->lose_energy(EUT_SPELL);

    if (you.can_see(*rakshasa))
    {
        mprf(MSGCH_MONSTER_SPELL,
             "The injured %s weaves a defensive illusion!",
             rakshasa->name(DESC_PLAIN).c_str());
    }
}

void bennu_revive_fineff::fire()
{
    // Bennu only resurrect once and immediately in the same spot,
    // so this is rather abbreviated compared to felids.
    // XXX: Maybe generalize felid_revives and merge the two anyway?

    bool res_visible = you.see_cell(posn);


    monster *newmons = create_monster(mgen_data(MONS_BENNU, attitude, posn, foe,
                                                res_visible ? MG_DONT_COME
                                                            : MG_NONE));
    if (newmons)
        newmons->props[BENNU_REVIVES_KEY].get_byte() = revives + 1;

    // If we were dueling the original bennu, the duel continues.
    if (duel)
    {
        newmons->props[OKAWARU_DUEL_TARGET_KEY] = true;
        newmons->props[OKAWARU_DUEL_CURRENT_KEY] = true;
    }

    if (gozag_bribe.ench != ENCH_NONE)
        newmons->add_ench(gozag_bribe);
}

void avoided_death_fineff::fire()
{
    ASSERT(defender() && defender()->is_monster());
    defender()->as_monster()->hit_points = hp;
    defender()->as_monster()->flags &= ~MF_PENDING_REVIVAL;
}

void infestation_death_fineff::fire()
{
    if (monster *scarab = create_monster(mgen_data(MONS_DEATH_SCARAB,
                                                   BEH_FRIENDLY, posn,
                                                   MHITYOU, MG_AUTOFOE)
                                         .set_summoned(&you, SPELL_INFESTATION,
                                                       summ_dur(5), false),
                                         false))
    {
        if (you.see_cell(posn) || you.can_see(*scarab))
        {
            mprf("%s bursts from %s!", scarab->name(DESC_A, true).c_str(),
                                       name.c_str());
        }
    }
}

void make_derived_undead_fineff::fire()
{
    monster *undead = create_monster(mg);
    if (!undead)
        return;

    if (!message.empty() && you.can_see(*undead))
        mpr(message);

    // If the original monster has been levelled up, its HD might be
    // different from its class HD, in which case its HP should be
    // rerolled to match.
    if (undead->get_experience_level() != experience_level)
    {
        undead->set_hit_dice(max(experience_level, 1));
        roll_zombie_hp(undead);
    }

    // Fix up custom names
    if (!mg.mname.empty())
        name_zombie(*undead, mg.base_type, mg.mname);

    if (!agent.empty())
        mons_add_blame(undead, "animated by " + agent);
}

const actor *mummy_death_curse_fineff::fixup_attacker(const actor *a)
{
    if (a && a->is_monster() && a->as_monster()->friendly()
        && !crawl_state.game_is_arena())
    {
        // Mummies are smart enough not to waste curses on summons or allies.
        return &you;
    }
    return a;
}

void mummy_death_curse_fineff::fire()
{
    if (pow <= 0)
        return;

    switch (killer)
    {
        // Mummy killed by trap or something other than the player or
        // another monster, so no curse.
        case KILL_NON_ACTOR:
        case KILL_RESET:
        case KILL_RESET_KEEP_ITEMS:
        // Mummy sent to the Abyss wasn't actually killed, so no curse.
        case KILL_BANISHED:
            return;

        default:
            break;
    }

    actor* victim;

    if (YOU_KILL(killer))
        victim = &you;
    // Killed by a Zot trap, a god, etc, or suicide.
    else if (!attacker() || attacker() == defender())
        return;
    else
        victim = attacker();

    // Mummy was killed by a ballistomycete spore or ball lightning?
    if (!victim->alive())
        return;

    // Stepped from time?
    if (!in_bounds(victim->pos()))
        return;

    if (victim->is_player())
        mprf(MSGCH_MONSTER_SPELL, "You feel extremely nervous for a moment...");
    else if (you.can_see(*victim))
    {
        mprf(MSGCH_MONSTER_SPELL, "A malignant aura surrounds %s.",
             victim->name(DESC_THE).c_str());
    }
    const string cause = make_stringf("%s death curse",
                            apostrophise(name).c_str());
    // source is used as a melee source and must be alive
    // since the mummy is dead now we pass nullptr
    death_curse(*victim, nullptr, cause, pow);
}

void summon_dismissal_fineff::fire()
{
    if (defender() && defender()->alive())
        monster_die(*(defender()->as_monster()), KILL_TIMEOUT, NON_MONSTER);
}

void spectral_weapon_fineff::fire()
{
    actor *atkr = attacker();
    actor *defend = defender();
    if (!defend || !atkr || !defend->alive() || !atkr->alive())
        return;

    const coord_def target = defend->pos();

    // Do we already have a spectral weapon?
    monster* sw = find_spectral_weapon(atkr);
    if (sw)
    {
        if (sw == defend)
            return; // don't attack yourself. too silly.
        // Is it already in range?
        const reach_type sw_range = sw->reach_range();
        if (sw_range > REACH_NONE
            && can_reach_attack_between(sw->pos(), target, sw_range)
            || adjacent(sw->pos(), target))
        {
            // Just attack.
            melee_attack melee_attk(sw, defend);
            melee_attk.attack();
            return;
        }
    }

    // Can we find a nearby space to attack from?
    const reach_type atk_range = atkr->reach_range();
    int seen_valid = 0;
    coord_def chosen_pos;
    // Try only spaces adjacent to the attacker.
    for (adjacent_iterator ai(atkr->pos()); ai; ++ai)
    {
        if (actor_at(*ai)
            || !monster_habitable_grid(MONS_SPECTRAL_WEAPON, *ai))
        {
            continue;
        }
        // ... and only spaces the weapon could attack the defender from.
        if (grid_distance(*ai, target) > 1
            && (atk_range <= REACH_NONE
                || !can_reach_attack_between(*ai, target, atk_range)))
        {
            continue;
        }
        // Reservoir sampling.
        seen_valid++;
        if (one_chance_in(seen_valid))
            chosen_pos = *ai;
    }
    if (!seen_valid || !weapon || !weapon->defined())
        return;

    mgen_data mg(MONS_SPECTRAL_WEAPON,
                 atkr->is_player() ? BEH_FRIENDLY
                                  : SAME_ATTITUDE(atkr->as_monster()),
                 chosen_pos,
                 atkr->mindex(),
                 MG_FORCE_BEH | MG_FORCE_PLACE);
    mg.set_summoned(atkr, 0);
    mg.extra_flags |= (MF_NO_REWARD | MF_HARD_RESET);
    mg.props[TUKIMA_WEAPON] = *weapon;
    mg.props[TUKIMA_POWER] = 50;

    dprf("spawning at %d,%d", chosen_pos.x, chosen_pos.y);

    monster *mons = create_monster(mg);
    if (!mons)
        return;

    // We successfully made a new one! Kill off the old one,
    // and don't spam the player with a spawn message.
    if (sw)
    {
        mons->flags |= MF_WAS_IN_VIEW | MF_SEEN;
        end_spectral_weapon(sw, false, true);
    }

    dprf("spawned at %d,%d", mons->pos().x, mons->pos().y);

    melee_attack melee_attk(mons, defend);
    melee_attk.attack();

    mons->summoner = atkr->mid;
    mons->behaviour = BEH_SEEK; // for display
    atkr->props[SPECTRAL_WEAPON_KEY].get_int() = mons->mid;
}

void lugonu_meddle_fineff::fire() {
    lucy_check_meddling();
}

void jinxbite_fineff::fire()
{
    actor* defend = defender();
    if (defend && defend->alive())
        attempt_jinxbite_hit(*defend);
}

bool beogh_resurrection_fineff::mergeable(const final_effect &fe) const
{
    const beogh_resurrection_fineff *o =
        dynamic_cast<const beogh_resurrection_fineff *>(&fe);
    return o && ostracism_only == o->ostracism_only;
}

void beogh_resurrection_fineff::fire()
{
    beogh_resurrect_followers(ostracism_only);
}

void dismiss_divine_allies_fineff::fire()
{
    if (god == GOD_BEOGH)
        beogh_do_ostracism();
    else
        dismiss_god_summons(god);
}

void death_spawn_fineff::fire()
{
    create_monster(mg);
}

void splinterfrost_fragment_fineff::fire()
{
    if (!msg.empty())
        mprf(MSGCH_MONSTER_DAMAGE, MDAM_DEAD, "%s", msg.c_str());

    beam.fire();
}

// Effects that occur after all other effects, even if the monster is dead.
// For example, explosions that would hit other creatures, but we want
// to deal with only one creature at a time, so that's handled last.
void fire_final_effects()
{
    while (!env.final_effects.empty())
    {
        // Remove it first so nothing can merge with it.
        unique_ptr<final_effect> eff(env.final_effects.back());
        env.final_effects.pop_back();
        eff->fire();
    }

    // Clear all cached monster copies
    env.final_effect_monster_cache.clear();
}
