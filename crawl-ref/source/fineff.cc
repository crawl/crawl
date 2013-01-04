/**
 * @file
 * @brief Brand/ench/etc effects that might alter something in an
 *             unexpected way.
**/

#include "AppHdr.h"
#include "coord.h"
#include "effects.h"
#include "env.h"
#include "fineff.h"
#include "libutil.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-place.h"
#include "ouch.h"
#include "religion.h"
#include "state.h"
#include "view.h"

void add_final_effect(final_effect_flavour flavour,
                      const actor *attacker,
                      const actor *defender,
                      coord_def pos,
                      int x)
{
    final_effect fe;

    fe.att = 0;
    fe.def = 0;

    if (attacker)
        fe.att = attacker->mid;
    if (defender)
        fe.def = defender->mid;

    fe.flavour = flavour;
    fe.pos     = pos;
    fe.x       = x;

    for (std::vector<final_effect>::iterator fi = env.final_effects.begin();
         fi != env.final_effects.end();
         ++fi)
    {
        if (fi->flavour == fe.flavour
            && fi->att == fe.att
            && fi->def == fe.def
            && fi->pos == fe.pos)
        {
            // If there's a non-additive effect, you'd need to handle it here.
            // Elec discharge is (now) idempotent, so that's ok.
            fi->x += fe.x;
            return;
        }
    }
    env.final_effects.push_back(fe);
}

static void _trj_spawns(actor *attacker, actor *trj, coord_def pos, int damage)
{
    int tospawn = div_rand_round(damage, 12);

    if (tospawn <= 0)
        return;

    dprf("Trying to spawn %d jellies.", tospawn);

    unsigned short foe = attacker && attacker->alive() ? attacker->mindex()
                                                       : MHITNOT;
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
        coord_def jpos = find_newmons_square_contiguous(jelly, pos);
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

    if (!spawned || !you.see_cell(pos))
        return;

    if (trj)
    {
        const std::string monnam = trj->name(DESC_THE);
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

// Effects that occur after all other effects, even if the monster is dead.
// For example, explosions that would hit other creatures, but we want
// to deal with only one creature at a time, so that's handled last.
void fire_final_effects()
{
    for (unsigned int i = 0; i < env.final_effects.size(); ++i)
    {
        const final_effect &fe = env.final_effects[i];
        // We can't just pass the pointer, as we wouldn't be notified
        // if it becomes invalid between scheduling and firing.
        actor *attacker = actor_by_mid(fe.att);
        actor *defender = actor_by_mid(fe.def);

        switch (fe.flavour)
        {
        case FINEFF_LIGHTNING_DISCHARGE:
            if (you.see_cell(fe.pos))
                mpr("Electricity arcs through the water!");
            conduct_electricity(fe.pos, attacker);
            break;

        case FINEFF_MIRROR_DAMAGE:
            if (!attacker || attacker == defender || !attacker->alive())
                continue;
            // defender being dead is ok, if we killed them we still suffer
            if (fe.att == MID_PLAYER)
            {
                mpr("It reflects your damage back at you!");
                ouch(fe.x, NON_MONSTER, KILLED_BY_REFLECTION);
            }
            else if (fe.def == MID_PLAYER)
            {
                simple_god_message(" mirrors your injury!");
#ifndef USE_TILE_LOCAL
                flash_monster_colour(attacker->as_monster(), RED, 200);
#endif

                attacker->hurt(&you, fe.x);

                if (attacker->alive())
                    print_wounds(attacker->as_monster());

                lose_piety(isqrt_ceil(fe.x));
            }
            else
            {
                simple_monster_message(attacker->as_monster(),
                                       " suffers a backlash!");
                attacker->hurt(defender, fe.x);
            }
            break;

        case FINEFF_TRAMPLE_FOLLOW:
            if (!attacker
                || attacker->pos() == fe.pos
                || !adjacent(attacker->pos(), fe.pos)
                || !attacker->is_habitable(fe.pos))
            {
                continue;
            }
            attacker->move_to_pos(fe.pos);
            break;

        case FINEFF_BLINK:
            if (defender && defender->alive() && !defender->no_tele(true, false))
                defender->blink();
            break;

        case FINEFF_DISTORTION_TELEPORT:
            if (defender && defender->alive() && !defender->no_tele(true, false))
                defender->teleport(true, one_chance_in(5));
            break;

        case FINEFF_ROYAL_JELLY_SPAWN:
            _trj_spawns(attacker, defender, fe.pos, fe.x);
            break;

        case FINEFF_BLOOD:
            monster_type montype = static_cast<monster_type>(fe.x & 0xffff);
            int blood = (fe.x >> 16) & 0xffff;
            bleed_onto_floor(fe.pos, montype, blood, true);
        }
    }
    env.final_effects.clear();
}
