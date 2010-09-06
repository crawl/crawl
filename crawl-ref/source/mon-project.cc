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
#include "itemprop.h"
#include "mgen_data.h"
#include "mon-place.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "shout.h"
#include "stuff.h"
#include "terrain.h"
#include "viewchar.h"

static void _fuzz_direction(monsters &mon, int pow);

bool cast_iood(actor *caster, int pow, bolt *beam)
{
    int mtarg = mgrd(beam->target);
    if (beam->target == you.pos())
        mtarg = MHITYOU;

    int mind = -1;
    for (int i=0; i < 10 && mind == -1; i++)
    {
        mind = mons_place(mgen_data(MONS_ORB_OF_DESTRUCTION,
                (caster->atype() == ACT_PLAYER) ? BEH_FRIENDLY :
                    ((monsters*)caster)->friendly() ? BEH_FRIENDLY : BEH_HOSTILE,
                caster,
                0,
                SPELL_IOOD,
                coord_def(-1, -1),
                mtarg,
                0,
                GOD_NO_GOD));
    }
    if (mind == -1)
    {
        mpr("Failed to spawn projectile.", MSGCH_WARN);
        /*canned_msg(MSG_NOTHING_HAPPENS);*/
        return (false);
    }

    monsters &mon = menv[mind];
    const coord_def pos = caster->pos();
    beam->choose_ray();
    dprf("beam (%d,%d)+t*(%d,%d)  ray (%f,%f)+t*(%f,%f)",
        pos.x, pos.y, beam->target.x - pos.x, beam->target.y - pos.y,
        beam->ray.r.start.x - 0.5, beam->ray.r.start.y - 0.5,
        beam->ray.r.dir.x, beam->ray.r.dir.y);
    mon.props["iood_x"].get_float() = beam->ray.r.start.x - 0.5;
    mon.props["iood_y"].get_float() = beam->ray.r.start.y - 0.5;
    mon.props["iood_vx"].get_float() = beam->ray.r.dir.x;
    mon.props["iood_vy"].get_float() = beam->ray.r.dir.y;
    mon.props["iood_kc"].get_byte() = (caster->atype() == ACT_PLAYER) ? KC_YOU :
            ((monsters*)caster)->friendly() ? KC_FRIENDLY : KC_OTHER;
    mon.props["iood_pow"].get_short() = pow;
    mon.flags &= ~MF_JUST_SUMMONED;
    mon.props["iood_caster"].get_string() = caster->as_monster()
        ? caster->name(DESC_PLAIN, true)
        : "";

    _fuzz_direction(mon, pow);

    // Move away from the caster's square.
    iood_act(mon, true);
    // We need to take at least one full move (for the above), but let's
    // randomize it and take more so players won't get guaranteed instant
    // damage.
    mon.lose_energy(EUT_MOVE, 2, random2(3)+2);
    return (true);
}

static void _normalize(float &x, float &y)
{
    const float d = sqrt(x*x + y*y);
    if (d <= 0.000001)
        return;
    x/=d;
    y/=d;
}

// angle measured in chord length
static bool _in_front(float vx, float vy, float dx, float dy, float angle)
{
    return ((dx-vx)*(dx-vx) + (dy-vy)*(dy-vy) <= (angle*angle));
}

static void _iood_dissipate(monsters &mon, bool msg = true)
{
    if (msg)
        simple_monster_message(&mon, " dissipates.");
    dprf("iood: dissipating");
    monster_die(&mon, KILL_DISMISSED, NON_MONSTER);
}

static void _fuzz_direction(monsters &mon, int pow)
{
    const float x = mon.props["iood_x"];
    const float y = mon.props["iood_y"];
    float vx = mon.props["iood_vx"];
    float vy = mon.props["iood_vy"];

    _normalize(vx, vy);

    if (pow < 10)
        pow = 10;
    const float off = (coinflip() ? -1 : 1) * 0.25;
    float tan = (random2(31) - 15) * 0.019; // approx from degrees
    tan *= 75.0 / pow;
    if (wearing_amulet(AMU_INACCURACY))
        tan *= 2;

    // Cast either from left or right hand.
    mon.props["iood_x"] = x + vy*off;
    mon.props["iood_y"] = y - vx*off;
    // And off the direction a bit.
    mon.props["iood_vx"] = vx + vy*tan;
    mon.props["iood_vy"] = vy - vx*tan;
}

// Alas, too much differs to reuse beam shield blocks :(
static bool _iood_shielded(monsters &mon, actor &victim)
{
    if (!victim.shield() || victim.incapacitated())
        return (false);

    const int to_hit = 15 + mon.props["iood_pow"].get_short()/12;
    const int con_block = random2(to_hit + victim.shield_block_penalty());
    const int pro_block = victim.shield_bonus();
    dprf("iood shield: pro %d, con %d", pro_block, con_block);
    return (pro_block >= con_block);
}

static bool _iood_hit(monsters &mon, const coord_def &pos, bool big_boom = false)
{
    bolt beam;
    beam.name = "orb of destruction";
    beam.flavour = BEAM_NUKE;
    beam.attitude = mon.attitude;
    beam.thrower = (mon.props["iood_kc"].get_byte() == KC_YOU)
                        ? KILL_YOU_MISSILE : KILL_MON_MISSILE;
    beam.colour = WHITE;
    beam.glyph = dchar_glyph(DCHAR_FIRED_BURST);
    beam.range = 1;
    beam.source = pos;
    beam.target = pos;
    beam.hit = AUTOMATIC_HIT;
    beam.source_name = mon.props["iood_caster"].get_string();

    int pow = mon.props["iood_pow"].get_short();
    pow = stepdown_value(pow, 30, 30, 200, -1);
    const int dist = mon.props["iood_distance"].get_int();
    ASSERT(dist >= 0);
    if (dist < 4)
        pow = pow * (dist*2+3) / 10;
    beam.damage = dice_def(8, pow / 4);

    beam.ex_size = 1;
    beam.loudness = 7;

    monster_die(&mon, KILL_DISMISSED, NON_MONSTER);

    if (big_boom)
        beam.explode(true, false);
    else
        beam.fire();

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

    dprf("iood_act: pos=(%d,%d) rpos=(%f,%f) v=(%f,%f) foe=%d",
         mon.pos().x, mon.pos().y,
         x, y, vx, vy, mon.foe);

    if (!vx && !vy) // not initialized
    {
        _iood_dissipate(mon);
        return (true);
    }

    _normalize(vx, vy);

    const actor *foe = mon.get_foe();
    // If the target is gone, the orb continues on a ballistic course since
    // picking a new one would require intelligence.

    if (foe)
    {
        const coord_def target = foe->pos();
        float dx = target.x - x;
        float dy = target.y - y;
        _normalize(dx, dy);

        // Special case:
        // Moving diagonally when the orb is just about to hit you
        //      2
        //    ->*1
        // (from 1 to 2) would be a guaranteed escape.  This may be
        // realistic (strafing!), but since the game has no non-cheesy
        // means of waiting a small fraction of a turn, we don't want it.
        const int old_t_pos = mon.props["iood_tpos"].get_short();
        const coord_def rpos(static_cast<int>(round(x)), static_cast<int>(round(y)));
        if (old_t_pos && old_t_pos != (256 * target.x + target.y)
            && (rpos - target).rdist() <= 1
            // ... but following an orb is ok.
            && _in_front(vx, vy, dx, dy, 1.5)) // ~97 degrees
        {
            vx = dx;
            vy = dy;
        }
        mon.props["iood_tpos"].get_short() = 256 * target.x + target.y;

        if (!_in_front(vx, vy, dx, dy, 0.3)) // ~17 degrees
        {
            float ax, ay;
            if (dy*vx < dx*vy)
                ax = vy, ay = -vx, dprf("iood: veering left");
            else
                ax = -vy, ay = vx, dprf("iood: veering right");
            vx += ax * 0.3;
            vy += ay * 0.3;
        }
        else
            dprf("iood: keeping course");

        _normalize(vx, vy);
        mon.props["iood_vx"] = vx;
        mon.props["iood_vy"] = vy;
    }

move_again:

    x += vx;
    y += vy;

    mon.props["iood_x"] = x;
    mon.props["iood_y"] = y;
    mon.props["iood_distance"].get_int()++;

    const coord_def pos(static_cast<int>(round(x)), static_cast<int>(round(y)));
    if (!in_bounds(pos))
    {
        _iood_dissipate(mon);
        return (true);
    }

    if (mon.props["iood_kc"].get_byte() == KC_YOU
        && (you.pos() - pos).rdist() > LOS_RADIUS)
    {   // not actual vision, because of the smoke trail
        _iood_dissipate(mon);
        return (true);
    }

    if (pos == mon.pos())
        return (false);

    if (!no_trail)
    {
        place_cloud(CLOUD_MAGIC_TRAIL, mon.pos(),
                    2 + random2(3), mon.kill_alignment(),
                    KILL_MON_MISSILE);
    }

    actor *victim = actor_at(pos);
    if (cell_is_solid(pos) || victim)
    {
        if (cell_is_solid(pos))
        {
            if (you.see_cell(pos))
                mprf("%s hits %s", mon.name(DESC_CAP_THE, true).c_str(),
                     feature_description(pos, false, DESC_NOCAP_A).c_str());
        }

        monsters* monster = (victim && victim->atype() == ACT_MONSTER) ?
            (monsters*) victim : 0;

        if (monster && mons_is_projectile(victim->id()))
        {
            if (mon.observable())
                mpr("The orbs collide in a blinding explosion!");
            else
                noisy(40, pos, "You hear a loud magical explosion!");
            monster_die(monster, KILL_DISMISSED, NON_MONSTER);
            _iood_hit(mon, pos, true);
            return (true);
        }

        if (monster && monster->submerged())
        {
            // Try to swap with the submerged creature.
            if (monster->is_habitable(mon.pos()))
            {
                dprf("iood: Swapping with a submerged monster.");
                monster->set_position(mon.pos());
                mon.set_position(pos);
                mgrd(monster->pos()) = monster->mindex();
                mgrd(pos) = mon.mindex();

                return (false);
            }
            else // if swap fails, move ahead
            {
                dprf("iood: Boosting above a submerged monster (can't swap).");
                mon.lose_energy(EUT_MOVE);
                goto move_again;
            }
        }

        if (victim && _iood_shielded(mon, *victim))
        {
            item_def *shield = victim->shield();
            if (!shield_reflects(*shield))
            {
                if (victim->atype() == ACT_PLAYER)
                {
                    mprf("You block %s.", mon.name(DESC_NOCAP_THE, true).c_str());
                }
                else
                {
                    simple_monster_message(monster, (" blocks "
                        + mon.name(DESC_NOCAP_THE, true) + ".").c_str());
                }
                victim->shield_block_succeeded(&mon);
                _iood_dissipate(mon);
                return (true);
            }

            if (victim->atype() == ACT_PLAYER)
            {
                mprf("Your %s reflects %s!",
                    shield->name(DESC_PLAIN).c_str(),
                    mon.name(DESC_NOCAP_THE, true).c_str());
                ident_reflector(shield);
            }
            else if (you.see_cell(pos))
            {
                if (victim->observable())
                {
                    mprf("%s reflects %s with %s %s!",
                        victim->name(DESC_CAP_THE, true).c_str(),
                        mon.name(DESC_NOCAP_THE, true).c_str(),
                        mon.pronoun(PRONOUN_NOCAP_POSSESSIVE).c_str(),
                        shield->name(DESC_PLAIN).c_str());
                    ident_reflector(shield);
                }
                else
                {
                    mprf("%s bounces off thin air!",
                        mon.name(DESC_CAP_THE, true).c_str());
                }
            }
            victim->shield_block_succeeded(&mon);

            mon.props["iood_vx"] = vx = -vx;
            mon.props["iood_vy"] = vy = -vy;

            // Need to get out of the victim's square.

            // If you're next to the caster and both of you wear shields of
            // reflection, this can lead to a brief game of ping-pong, but
            // rapidly increasing shield penalties will make it short.
            mon.lose_energy(EUT_MOVE);
            goto move_again;
        }

        // Yay for inconsistencies in beam-vs-player and beam-vs-monsters.
        if (victim == &you)
            mprf("%s hits you!", mon.name(DESC_CAP_THE, true).c_str());

        if (_iood_hit(mon, pos))
            return (true);
    }

    if (!mon.move_to_pos(pos))
    {
        _iood_dissipate(mon);
        return (true);
    }

    // move_to_pos() just trashed the coords, set them again
    mon.props["iood_x"] = x;
    mon.props["iood_y"] = y;

    return (false);
}

// Reduced copy of iood_act to move the orb while the player
// is off-level. Just goes straight and dissipates instead of
// hitting anything.
static bool _iood_catchup_move(monsters& mon)
{
    float x = mon.props["iood_x"];
    float y = mon.props["iood_y"];
    float vx = mon.props["iood_vx"];
    float vy = mon.props["iood_vy"];

    if (!vx && !vy) // not initialized
    {
        _iood_dissipate(mon, false);
        return (true);
    }

    _normalize(vx, vy);

    x += vx;
    y += vy;

    mon.props["iood_x"] = x;
    mon.props["iood_y"] = y;
    mon.props["iood_distance"].get_int()++;

    const coord_def pos(static_cast<int>(round(x)), static_cast<int>(round(y)));
    if (!in_bounds(pos))
    {
        _iood_dissipate(mon, true);
        return (true);
    }

    if (pos == mon.pos())
        return (false);

    actor *victim = actor_at(pos);
    if (cell_is_solid(pos) || victim)
    {
        // Just dissipate instead of hitting something.
        _iood_dissipate(mon, true);
        return (true);
    }

    if (!mon.move_to_pos(pos))
    {
        _iood_dissipate(mon);
        return (true);
    }

    // move_to_pos() just trashed the coords, set them again
    mon.props["iood_x"] = x;
    mon.props["iood_y"] = y;

    return (false);
}

void iood_catchup(monsters* mons, int pturns)
{
    monsters& mon = *mons;
    ASSERT(mons_is_projectile(mon.type));

    const int moves = pturns * mon.speed / BASELINE_DELAY;

    if (moves > 50)
    {
        _iood_dissipate(mon, false);
        return;
    }

    if (mon.props["iood_kc"].get_byte() == KC_YOU)
    {
        // Left player's vision.
        _iood_dissipate(mon, false);
        return;
    }


    for (int i = 0; i < moves; ++i)
        if (_iood_catchup_move(mon))
            return;
}
