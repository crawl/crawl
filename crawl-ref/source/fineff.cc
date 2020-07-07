/**
 * @file
 * @brief Brand/ench/etc effects that might alter something in an
 *             unexpected way.
**/

#include "AppHdr.h"

#include "fineff.h"

#include "bloodspatter.h"
#include "coordit.h"
#include "dactions.h"
#include "death-curse.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "god-abil.h"
#include "libutil.h"
#include "message.h"
#include "mon-abil.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "ouch.h"
#include "religion.h"
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
    return o && def == o->def;
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

bool shock_serpent_discharge_fineff::mergeable(const final_effect &fe) const
{
    const shock_serpent_discharge_fineff *o = dynamic_cast<const shock_serpent_discharge_fineff *>(&fe);
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

void shock_serpent_discharge_fineff::merge(const final_effect &fe)
{
    const shock_serpent_discharge_fineff *ssdfe =
        dynamic_cast<const shock_serpent_discharge_fineff *>(&fe);
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

    god_acting gdact(GOD_YREDELEMNUL);

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
    else if (def == MID_PLAYER)
    {
        simple_god_message(" mirrors your injury!");

        attack->hurt(&you, damage);

        if (attack->alive())
            print_wounds(*monster_by_mid(att));

        lose_piety(isqrt_ceil(damage));
    }
    else
    {
        simple_monster_message(*monster_by_mid(att), " suffers a backlash!");
        attack->hurt(defender(), damage);
    }
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
    }
}

void blink_fineff::fire()
{
    actor *defend = defender();
    if (defend && defend->alive() && !defend->no_tele(true, false))
        defend->blink();
}

void teleport_fineff::fire()
{
    actor *defend = defender();
    if (defend && defend->alive() && !defend->no_tele(true, false))
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

    // No permanent friendly jellies from an enslaved TRJ.
    if (spawn_beh == BEH_FRIENDLY && !crawl_state.game_is_arena())
        return;

    int spawned = 0;
    for (int i = 0; i < tospawn; ++i)
    {
        const monster_type jelly = royal_jelly_ejectable_monster();
        coord_def jpos = find_newmons_square_contiguous(jelly, posn);
        if (!in_bounds(jpos))
            continue;

        if (monster *mons = mons_place(
                              mgen_data(jelly, spawn_beh, jpos, foe,
                                        MG_DONT_COME, GOD_JIYVA)
                              .set_summoned(trj, 0, 0)))
        {
            // Don't allow milking the Royal Jelly.
            mons->flags |= MF_NO_REWARD;
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
    if (actor *df = defender())
    {
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
    monster_die(*initial_mass, KILL_DISMISSED, NON_MONSTER, true);
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

void shock_serpent_discharge_fineff::fire()
{
    if (!oppressor.alive())
        return;

    const int max_range = 3; // v0v
    if (grid_distance(oppressor.pos(), position) > max_range)
        return;

    const monster* serpent = defender() ? defender()->as_monster() : nullptr;
    if (serpent && you.can_see(*serpent))
    {
        mprf("%s electric aura discharges%s, shocking %s!",
             serpent->name(DESC_ITS).c_str(),
             power < 4 ? "" : " violently",
             oppressor.name(DESC_THE).c_str());
    }
    else if (you.can_see(oppressor))
    {
        mprf("The air sparks with electricity, shocking %s!",
             oppressor.name(DESC_THE).c_str());
    }
    bolt beam;
    beam.flavour = BEAM_ELECTRICITY;

    int amount = roll_dice(3, 4 + power * 3 / 2);
    amount = oppressor.apply_ac(oppressor.beam_resists(beam, amount, true),
                                0, ac_type::half);
    oppressor.hurt(serpent, amount, beam.flavour, KILLED_BY_BEAM,
                                        "a shock serpent", "electric aura");
    if (amount)
        oppressor.expose_to_element(beam.flavour, amount);
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
        untransform();
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
        newmons->props["bennu_revives"].get_byte() = revives + 1;
}

void infestation_death_fineff::fire()
{
    if (monster *scarab = create_monster(mgen_data(MONS_DEATH_SCARAB,
                                                   BEH_FRIENDLY, posn,
                                                   MHITYOU, MG_AUTOFOE)
                                         .set_summoned(&you, 0,
                                                       SPELL_INFESTATION),
                                         false))
    {
        scarab->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 5));

        if (you.see_cell(posn) || you.can_see(*scarab))
        {
            mprf("%s bursts from %s!", scarab->name(DESC_A, true).c_str(),
                                       name.c_str());
        }
    }
}

void make_derived_undead_fineff::fire()
{
    if (monster *undead = create_monster(mg))
    {
        if (!message.empty())
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

        undead->add_ench(mon_enchant(ENCH_FAKE_ABJURATION, 5));
        if (!agent.empty())
        {
            mons_add_blame(undead,
                "animated by " + agent);
        }
    }
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
        case KILL_MISC:
        case KILL_RESET:
        case KILL_DISMISSED:
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
        monster_die(*(defender()->as_monster()), KILL_DISMISSED, NON_MONSTER);
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
}
