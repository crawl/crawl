/**
 * @file
 * @brief Brand/ench/etc effects that might alter something in an
 *             unexpected way.
**/

#include "AppHdr.h"
#include "coord.h"
#include "dactions.h"
#include "effects.h"
#include "env.h"
#include "fineff.h"
#include "libutil.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-place.h"
#include "ouch.h"
#include "religion.h"
#include "state.h"
#include "transform.h"
#include "view.h"

void final_effect::schedule()
{
    for (vector<final_effect *>::iterator fi = env.final_effects.begin();
         fi != env.final_effects.end(); ++fi)
    {
        if ((*fi)->mergeable(*this))
        {
            (*fi)->merge(*this);
            delete this;
            return;
        }
    }
    env.final_effects.push_back(this);
}

bool lightning_fineff::mergeable(const final_effect &fe) const
{
    const lightning_fineff *o = dynamic_cast<const lightning_fineff *>(&fe);
    return o && att == o->att && posn == o->posn;
}

bool mirror_damage_fineff::mergeable(const final_effect &fe) const
{
    const mirror_damage_fineff *o =
        dynamic_cast<const mirror_damage_fineff *>(&fe);
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

bool distortion_tele_fineff::mergeable(const final_effect &fe) const
{
    const distortion_tele_fineff *o =
        dynamic_cast<const distortion_tele_fineff *>(&fe);
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

bool delayed_action_fineff::mergeable(const final_effect &fe) const
{
    return false;
}

void mirror_damage_fineff::merge(const final_effect &fe)
{
    const mirror_damage_fineff *mdfe =
        dynamic_cast<const mirror_damage_fineff *>(&fe);
    ASSERT(mdfe);
    ASSERT(mergeable(*mdfe));
    damage += mdfe->damage;
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

void lightning_fineff::fire()
{
    if (you.see_cell(posn))
        mpr("Electricity arcs through the water!");
    conduct_electricity(posn, attacker());
}

void mirror_damage_fineff::fire()
{
    actor *attack = attacker();
    if (!attack || attack == defender() || !attack->alive())
        return;
    // defender being dead is ok, if we killed them we still suffer

    if (att == MID_PLAYER)
    {
        mpr("Your damage is reflected back at you!");
        ouch(damage, NON_MONSTER, KILLED_BY_MIRROR_DAMAGE);
    }
    else if (def == MID_PLAYER)
    {
        simple_god_message(" mirrors your injury!");
#ifndef USE_TILE_LOCAL
        flash_monster_colour(monster_by_mid(att), RED, 200);
#endif

        attack->hurt(&you, damage);

        if (attack->alive())
            print_wounds(monster_by_mid(att));

        lose_piety(isqrt_ceil(damage));
    }
    else
    {
        simple_monster_message(monster_by_mid(att), " suffers a backlash!");
        attack->hurt(defender(), damage);
    }
}

void trample_follow_fineff::fire()
{
    actor *attack = attacker();
    if (attack
        && attack->pos() != posn
        && adjacent(attack->pos(), posn)
        && attack->is_habitable(posn))
    {
        attack->move_to_pos(posn);
    }
}

void blink_fineff::fire()
{
    actor *defend = defender();
    if (defend && defend->alive() && !defend->no_tele(true, false))
        defend->blink();
}

void distortion_tele_fineff::fire()
{
    actor *defend = defender();
    if (defend && defend->alive() && !defend->no_tele(true, false))
        defend->teleport(true, one_chance_in(5));
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
                              mgen_data(jelly, spawn_beh, trj, 0, 0, jpos,
                                        foe, MG_DONT_COME, GOD_JIYVA)))
        {
            // Don't allow milking the royal jelly.
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
        mpr("One of the royal jelly's fragments survives.");
    else
    {
        mprf("The dying royal jelly spits out %s more jellies.",
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

        df->hurt(attacker(), damage, BEAM_MISSILE, true, attacker_effects);
    }
}

void starcursed_merge_fineff::fire()
{
    actor *defend = defender();
    if (defend && defend->alive())
        starcursed_merge(defender()->as_monster(), true);
}

void delayed_action_fineff::fire()
{
    if (final_msg)
        mpr(final_msg);
    add_daction(action);
}

void kirke_death_fineff::fire()
{
    delayed_action_fineff::fire();

    // Revert the player last
    if (you.form == TRAN_PIG)
        untransform();
}

// Effects that occur after all other effects, even if the monster is dead.
// For example, explosions that would hit other creatures, but we want
// to deal with only one creature at a time, so that's handled last.
void fire_final_effects()
{
    while (!env.final_effects.empty())
    {
        // Remove it first so nothing can merge with it.
        final_effect *eff = env.final_effects.back();
        env.final_effects.pop_back();

        eff->fire();

        delete eff;
    }
}
