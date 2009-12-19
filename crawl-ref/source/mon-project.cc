/*
 *  File:       mon-project.cc
 *  Summary:    Slow projectiles, done as monsters.
 *  Written by: Adam Borowski
 */

#include "AppHdr.h"

#include "mon-project.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <cmath>

#include "externs.h"

#include "cloud.h"
#include "directn.h"
#include "coord.h"
#include "env.h"
#include "mgen_data.h"
#include "mon-place.h"
#include "mon-stuff.h"
#include "shout.h"
#include "stuff.h"
#include "terrain.h"
#include "viewchar.h"

bool mons_is_projectile(monster_type mt)
{
    return (mt == MONS_ORB_OF_DESTRUCTION);
}

bool cast_iood(actor *caster, int pow, bolt *beam)
{
    int mtarg = mgrd(beam->target);
    
    int mind = mons_place(mgen_data(MONS_ORB_OF_DESTRUCTION,
                (caster->atype() == ACT_PLAYER) ? BEH_FRIENDLY :
                    ((monsters*)caster)->wont_attack() ? BEH_FRIENDLY : BEH_HOSTILE,
                caster,
                0,
                SPELL_IOOD,
                coord_def(-1, -1),
                (mtarg != NON_MONSTER) ? mtarg :
                    (you.pos() == beam->target) ? MHITYOU : MHITNOT,
                0,
                GOD_NO_GOD));
    if (mind == -1)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return (false);
    }

    monsters &mon = menv[mind];
    const coord_def pos = caster->pos();
    mon.props["iood_x"] = (float)pos.x;
    mon.props["iood_y"] = (float)pos.y;
    mon.props["iood_vx"] = (float)(beam->target.x - pos.x);
    mon.props["iood_vy"] = (float)(beam->target.y - pos.y);
    mon.props["iood_kc"].get_byte() = (caster->atype() == ACT_PLAYER) ? KC_YOU :
            ((monsters*)caster)->wont_attack() ? KC_FRIENDLY : KC_OTHER;
    mon.flags &= ~MF_JUST_SUMMONED;

    // Move away from the caster's square.
    iood_act(mon, true);
    mon.lose_energy(EUT_MOVE);
    return (true);
}

void _normalize(float &x, float &y)
{
    const float d = sqrt(x*x + y*y);
    if (d <= 0.000001)
        return;
    x/=d;
    y/=d;
}

void _iood_dissipate(monsters &mon)
{
    simple_monster_message(&mon, " dissipates.");
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "iood: dissipating");
#endif
    monster_die(&mon, KILL_DISMISSED, NON_MONSTER);
}

bool _iood_hit(monsters &mon, const coord_def &pos, bool big_boom = false)
{
    bolt beam;
    beam.name = "orb of destruction";
    beam.flavour = BEAM_NUKE;
    beam.attitude = mon.attitude;
    beam.thrower = (mon.props["iood_kc"].get_byte() == KC_YOU)
                        ? KILL_YOU_MISSILE : KILL_MON_MISSILE;
    beam.colour = WHITE;
    beam.type = dchar_glyph(DCHAR_FIRED_BURST);
    beam.range = 1;
    beam.source = pos;
    beam.target = pos;
    beam.hit = AUTOMATIC_HIT;
    beam.damage = dice_def(3, 20);
    beam.ex_size = 1;
    if (big_boom)
        beam.explode(true, true);
    else
        beam.fire();

    monster_die(&mon, KILL_DISMISSED, NON_MONSTER);
    return (true);
}

// returns true if the orb is gone
bool iood_act(monsters &mon, bool no_trail)
{
    ASSERT(mons_is_projectile(mon.type));

    float x = mon.props["iood_x"];
    float y = mon.props["iood_y"];
    float vx = mon.props["iood_vx"];
    float vy = mon.props["iood_vy"];

#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS,
         "iood_act: pos (%d,%d) rpos (%f,%f) v (%f,%f)",
         mon.pos().x, mon.pos().y,
         x, y, vx, vy);
#endif

    if (!vx && !vy) // not initialized
    {
        _iood_dissipate(mon);
        return (true);
    }

    coord_def target(-1, -1);
    if (mon.foe == MHITYOU)
        target = you.pos();
    else if (invalid_monster_index(mon.foe))
        ;
    else if (invalid_monster_type(menv[mon.foe].type))
    {
        // Our target is gone.  Since picking a new one would require
        // intelligence, the orb continues on a ballistic course.
        mon.foe = MHITNOT;
    }
    else
        target = menv[mon.foe].pos();

    _normalize(vx, vy);

    x += vx;
    y += vy;

    mon.props["iood_x"] = x;
    mon.props["iood_y"] = y;

    coord_def pos(round(x), round(y));
    if (!in_bounds(pos))
    {
        _iood_dissipate(mon);
        return (true);
    }

    if (pos == mon.pos())
        return (false);

    actor *victim = actor_at(pos);
    if (cell_is_solid(pos) || victim)
    {
        if (cell_is_solid(pos))
        {
            if (you.see_cell(pos))
                mprf("%s hits %s", mon.name(DESC_CAP_THE, true).c_str(),
                     feature_description(pos, false, DESC_NOCAP_A).c_str());
        }
        if (victim && mons_is_projectile(victim->id()))
        {
            if (mon.observable())
                mpr("The orbs collide in a blinding explosion!");
            else
                noisy(40, pos, "You hear a loud magical explosion!");
            monster_die((monsters*)victim, KILL_DISMISSED, NON_MONSTER);
            _iood_hit(mon, pos, true);
            return (true);
        }

        if (_iood_hit(mon, pos))
            return (true);
    }

    if (!no_trail)
    {
        place_cloud(CLOUD_TLOC_ENERGY, mon.pos(),
                    2 + random2(3), mon.kill_alignment(),
                    KILL_MON_MISSILE);
    }

    if (!mon.move_to_pos(pos))
    {
        _iood_dissipate(mon);
        return (true);
    }

    return (false);
}
