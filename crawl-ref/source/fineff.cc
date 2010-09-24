/*
 *  File:       fineff.cc
 *  Summary:    Brand/ench/etc effects that might alter something in an
 *              unexpected way.
 *  Written by: Adam Borowski
 */

#include "AppHdr.h"
#include "effects.h"
#include "env.h"
#include "fineff.h"
#include "ouch.h"

void add_final_effect(final_effect_flavour flavour,
                      const actor *attacker,
                      const actor *defender,
                      coord_def pos,
                      int x)
{
    final_effect fe;

    fe.att = NON_MONSTER;
    fe.def = NON_MONSTER;

    if (attacker)
        fe.att = attacker->mindex();
    if (defender)
        fe.def = defender->mindex();

    fe.flavour = flavour;
    fe.pos     = pos;
    fe.x       = x;

    for (std::vector<final_effect>::iterator fi = env.final_effects.begin();
         fi != env.final_effects.end();
         fi++)
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

        // This code doesn't check for monster being cleaned and a new one
        // immediately replacing it; this is not supposed to happen save for
        // zombifying (and then it's the same monster), but if this changes,
        // we'd need an identifier or such.
        actor *attacker = (fe.att == NON_MONSTER) ? 0 :
                          (fe.att == MHITYOU) ? (actor*)&you :
                          &menv[fe.att];
        actor *defender = (fe.def == NON_MONSTER) ? 0 :
                          (fe.def == MHITYOU) ? (actor*)&you :
                          &menv[fe.def];

        switch (fe.flavour)
        {
        case FINEFF_LIGHTNING_DISCHARGE:
            if (you.see_cell(fe.pos))
                mpr("Electricity arcs through the water!");
            conduct_electricity(fe.pos, attacker);
            break;
        case FINEFF_MIRROR_DAMAGE:
	    if (!attacker)
	        continue;
            // defender being dead is ok, if we killed them we still suffer
            if (attacker->atype() == ACT_PLAYER)
            {
                mpr("It reflects your damage back at you!");
                ouch(fe.x, NON_MONSTER, KILLED_BY_REFLECTION);
            }
            else
            {
                simple_monster_message(attacker->as_monster(),
                                       " suffers a backlash!");
                attacker->hurt(defender, fe.x);
            }
            break;
        }
    }
    env.final_effects.clear();
}
