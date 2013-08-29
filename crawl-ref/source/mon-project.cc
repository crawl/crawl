/**
 * @file
 * @brief Slow projectiles, done as monsters.
**/

#include "AppHdr.h"

#include "mon-project.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <cmath>

#include "externs.h"

#include "cloud.h"
#include "directn.h"
#include "env.h"
#include "itemprop.h"
#include "mgen_data.h"
#include "mon-place.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "ouch.h"
#include "shout.h"
#include "stuff.h"
#include "terrain.h"
#include "viewchar.h"

static void _fuzz_direction(const actor *caster, monster& mon, int pow);

spret_type cast_iood(actor *caster, int pow, bolt *beam, float vx, float vy,
                     int foe, bool fail)
{
    const bool is_player = caster->is_player();
    if (beam && is_player && !player_tracer(ZAP_IOOD, pow, *beam))
        return SPRET_ABORT;

    fail_check();

    int mtarg = !beam ? MHITNOT :
                beam->target == you.pos() ? MHITYOU : mgrd(beam->target);

    monster *mon = place_monster(mgen_data(MONS_ORB_OF_DESTRUCTION,
                (is_player) ? BEH_FRIENDLY :
                    ((monster*)caster)->friendly() ? BEH_FRIENDLY : BEH_HOSTILE,
                caster,
                0,
                SPELL_IOOD,
                coord_def(),
                mtarg,
                0,
                GOD_NO_GOD), true, true);
    if (!mon)
    {
        mpr("Failed to spawn projectile.", MSGCH_ERROR);
        return SPRET_ABORT;
    }

    if (beam)
    {
        beam->choose_ray();
#ifdef DEBUG_DIAGNOSTICS
        const coord_def pos = caster->pos();
        dprf("beam (%d,%d)+t*(%d,%d)  ray (%f,%f)+t*(%f,%f)",
            pos.x, pos.y, beam->target.x - pos.x, beam->target.y - pos.y,
            beam->ray.r.start.x - 0.5, beam->ray.r.start.y - 0.5,
            beam->ray.r.dir.x, beam->ray.r.dir.y);
#endif
        mon->props["iood_x"].get_float() = beam->ray.r.start.x - 0.5;
        mon->props["iood_y"].get_float() = beam->ray.r.start.y - 0.5;
        mon->props["iood_vx"].get_float() = beam->ray.r.dir.x;
        mon->props["iood_vy"].get_float() = beam->ray.r.dir.y;
        _fuzz_direction(caster, *mon, pow);
    }
    else
    {
        // Multi-orb: spread the orbs a bit, otherwise diagonal ones might
        // fail to leave the cardinal direction: orb A moves -0.4,+0.9 and
        // orb B +0.4,+0.9, both rounded to 0,1.
        mon->props["iood_x"].get_float() = caster->pos().x + 0.4 * vx;
        mon->props["iood_y"].get_float() = caster->pos().y + 0.4 * vy;
        mon->props["iood_vx"].get_float() = vx;
        mon->props["iood_vy"].get_float() = vy;
    }

    mon->props["iood_kc"].get_byte() = (is_player) ? KC_YOU :
        ((monster*)caster)->friendly() ? KC_FRIENDLY : KC_OTHER;
    mon->props["iood_pow"].get_short() = pow;
    mon->flags &= ~MF_JUST_SUMMONED;
    mon->props["iood_caster"].get_string() = caster->as_monster()
        ? caster->name(DESC_PLAIN, true)
        : (caster->is_player()) ? "you" : "";
    mon->summoner = caster->mid;

    if (caster->is_player() || caster->type == MONS_PLAYER_GHOST
        || caster->type == MONS_PLAYER_ILLUSION)
    {
        mon->props["iood_flawed"].get_byte() = true;
    }

    // Move away from the caster's square.
    iood_act(*mon, true);
    // We need to take at least one full move (for the above), but let's
    // randomize it and take more so players won't get guaranteed instant
    // damage.
    mon->lose_energy(EUT_MOVE, 2, random2(3)+2);

    // Multi-orbs don't home during the first move, they'd likely
    // immediately explode otherwise.
    if (foe != MHITNOT)
        mon->foe = foe;

    return SPRET_SUCCESS;
}

void cast_iood_burst(int pow, coord_def target)
{
    int foe = MHITNOT;
    if (const monster* mons = monster_at(target))
    {
        if (mons && you.can_see(mons))
            foe = mons->mindex();
    }

    int n_orbs = random_range(3, 7);
    dprf("Bursting %d orbs.", n_orbs);
    double angle0 = random2(2097152) * PI / 1048576;

    for (int i = 0; i < n_orbs; i++)
    {
        double angle = angle0 + i * PI * 2 / n_orbs;
        cast_iood(&you, pow, 0, sin(angle), cos(angle), foe);
    }
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

static void _iood_stop(monster& mon, bool msg = true)
{
    if (mons_is_boulder(&mon))
    {
        // Deduct the energy first - the move they made that just stopped
        // them was a speed 14 move.
        mon.lose_energy(EUT_MOVE);
        mon.del_ench(ENCH_ROLLING,!msg);
        return;
    }

    if (msg)
        simple_monster_message(&mon, " dissipates.");
    dprf("iood: dissipating");
    monster_die(&mon, KILL_DISMISSED, NON_MONSTER);
}

static void _fuzz_direction(const actor *caster, monster& mon, int pow)
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
    if (caster && caster->inaccuracy())
        tan *= 2;

    // Cast either from left or right hand.
    mon.props["iood_x"] = x + vy*off;
    mon.props["iood_y"] = y - vx*off;
    // And off the direction a bit.
    mon.props["iood_vx"] = vx + vy*tan;
    mon.props["iood_vy"] = vy - vx*tan;
}

// Alas, too much differs to reuse beam shield blocks :(
static bool _iood_shielded(monster& mon, actor &victim)
{
    if (!victim.shield() || victim.incapacitated())
        return false;

    const int to_hit = 15 + (mons_is_projectile(mon.type) ?
        mon.props["iood_pow"].get_short()/12 : mon.hit_dice/2);
    const int con_block = random2(to_hit + victim.shield_block_penalty());
    const int pro_block = victim.shield_bonus();
    dprf("iood shield: pro %d, con %d", pro_block, con_block);
    return (pro_block >= con_block);
}

static bool _boulder_hit(monster& mon, const coord_def &pos)
{
    actor *victim = actor_at(pos);
    if (victim)
    {
        simple_monster_message(&mon, (string(" smashes into ")
                               + victim->name(DESC_THE) + "!").c_str());

        int dam = victim->apply_ac(roll_dice(3, 20));
        if (victim->is_player())
            ouch(dam, mon.mindex(), KILLED_BY_ROLLING);
        else
            victim->hurt(&mon, dam);
    }

    noisy(5, pos);
    return victim && victim->alive();
}

static bool _iood_hit(monster& mon, const coord_def &pos, bool big_boom = false)
{
    if (mons_is_boulder(&mon))
        return _boulder_hit(mon, pos);

    bolt beam;
    beam.name = "orb of destruction";
    beam.flavour = BEAM_NUKE;
    beam.attitude = mon.attitude;

    actor *caster = actor_by_mid(mon.summoner);
    if (!caster)        // caster is dead/gone, blame the orb itself (as its
        caster = &mon;  // friendliness is correct)
    beam.set_agent(caster);
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
    beam.damage = dice_def(9, pow / 4);

    if (dist < 3)
        beam.name = "wavering " + beam.name;
    if (dist < 2)
        beam.hit_verb = "weakly hits";
    beam.ex_size = 1;
    beam.loudness = 7;

    monster_die(&mon, KILL_DISMISSED, NON_MONSTER);

    if (big_boom)
        beam.explode(true, false);
    else
        beam.fire();

    return true;
}

// returns true if the orb is gone
bool iood_act(monster& mon, bool no_trail)
{
    bool iood = mons_is_projectile(mon.type);
    ASSERT(iood || mons_is_boulder(&mon));

    float x = mon.props["iood_x"];
    float y = mon.props["iood_y"];
    float vx = mon.props["iood_vx"];
    float vy = mon.props["iood_vy"];

    dprf("iood_act: pos=(%d,%d) rpos=(%f,%f) v=(%f,%f) foe=%d",
         mon.pos().x, mon.pos().y,
         x, y, vx, vy, mon.foe);

    if (!vx && !vy) // not initialized
    {
        _iood_stop(mon);
        return true;
    }

    _normalize(vx, vy);

    const actor *foe = mon.get_foe();
    // If the target is gone, the orb continues on a ballistic course since
    // picking a new one would require intelligence.

    // Boulders don't home onto their targets.
    if (iood && foe)
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
        _iood_stop(mon);
        return true;
    }

    if (iood && mon.props.exists("iood_flawed"))
    {
        const actor *caster = actor_by_mid(mon.summoner);
        if (!caster || caster->pos().origin() ||
            (caster->pos() - pos).rdist() > LOS_RADIUS)
        {   // not actual vision, because of the smoke trail
            _iood_stop(mon);
            return true;
        }
    }

    if (pos == mon.pos())
        return false;

    if (!no_trail)
    {
        place_cloud(iood ? CLOUD_MAGIC_TRAIL : CLOUD_DUST_TRAIL, mon.pos(),
                    2 + random2(3), &mon);
    }

    actor *victim = actor_at(pos);
    if (cell_is_solid(pos) || victim)
    {
        if (cell_is_solid(pos))
        {
            if (you.see_cell(pos))
            {
                mprf("%s hits %s", mon.name(DESC_THE, true).c_str(),
                     feature_description_at(pos, false, DESC_A).c_str());
            }

            if (!iood) // boulders need to stop now
            {
                _iood_stop(mon);
                // Can't hurt rock worms anyway.
                return true;
            }
        }

        monster* mons = (victim && victim->is_monster()) ?
            (monster*) victim : 0;

        if (mons && iood && mons_is_projectile(victim->type))
        {
            if (mon.observable())
                mpr("The orbs collide in a blinding explosion!");
            else
                noisy(40, pos, "You hear a loud magical explosion!");
            monster_die(mons, KILL_DISMISSED, NON_MONSTER);
            _iood_hit(mon, pos, true);
            return true;
        }

        if (mons && mons_is_boulder(&mon) && mons_is_boulder(mons))
        {
            if (mon.observable())
                mpr("The boulders collide with a stupendous crash!");
            else
                noisy(20, pos, "You hear a loud crashing sound!");

            // Remove ROLLING and add DAZED
            _iood_stop(mon);
            _iood_stop(*mons);
            if (!mon.check_clarity(false))
                mon.add_ench(ENCH_CONFUSION);
            if (!mons->check_clarity(false))
                mons->add_ench(ENCH_CONFUSION);
            return true;
        }

        if (mons && (mons->submerged() || mons->type == MONS_BATTLESPHERE))
        {
            // Try to swap with the submerged creature.
            if (mons->is_habitable(mon.pos()))
            {
                dprf("iood: Swapping with a submerged monster.");
                mons->set_position(mon.pos());
                mon.set_position(pos);
                ASSERT(!mons->is_constricted());
                ASSERT(!mons->is_constricting());
                mgrd(mons->pos()) = mons->mindex();
                mgrd(pos) = mon.mindex();

                return false;
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
                if (victim->is_player())
                    mprf("You block %s.", mon.name(DESC_THE, true).c_str());
                else
                {
                    simple_monster_message(mons, (" blocks "
                        + mon.name(DESC_THE, true) + ".").c_str());
                }
                victim->shield_block_succeeded(&mon);
                _iood_stop(mon);
                return true;
            }

            if (victim->is_player())
            {
                mprf("Your %s reflects %s!",
                    shield->name(DESC_PLAIN).c_str(),
                    mon.name(DESC_THE, true).c_str());
                ident_reflector(shield);
            }
            else if (you.see_cell(pos))
            {
                if (victim->observable())
                {
                    mprf("%s reflects %s with %s %s!",
                        victim->name(DESC_THE, true).c_str(),
                        mon.name(DESC_THE, true).c_str(),
                        mon.pronoun(PRONOUN_POSSESSIVE).c_str(),
                        shield->name(DESC_PLAIN).c_str());
                    ident_reflector(shield);
                }
                else
                {
                    mprf("%s bounces off thin air!",
                        mon.name(DESC_THE, true).c_str());
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

        if (_iood_hit(mon, pos))
        {
            if (!iood)
                _iood_stop(mon);
            return true;
        }
    }

    // Boulders stop at lava/water to prevent unusual behaviour;
    // skimming across the water like a pebble could be justifiable, but
    // it raises too many questions.
    if (!iood && (!feat_has_solid_floor(grd(pos)) || feat_is_water(grd(pos))))
    {
        mprf("%s screeches to a halt.", mon.name(DESC_THE, true).c_str());
        _iood_stop(mon,false);
        return true;
    }

    if (!mon.move_to_pos(pos))
    {
        _iood_stop(mon);
        return true;
    }

    // move_to_pos() just trashed the coords, set them again
    mon.props["iood_x"] = x;
    mon.props["iood_y"] = y;

    return false;
}

// Reduced copy of iood_act to move the orb while the player
// is off-level. Just goes straight and dissipates instead of
// hitting anything.
static bool _iood_catchup_move(monster& mon)
{
    float x = mon.props["iood_x"];
    float y = mon.props["iood_y"];
    float vx = mon.props["iood_vx"];
    float vy = mon.props["iood_vy"];

    if (!vx && !vy) // not initialized
    {
        _iood_stop(mon, false);
        return true;
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
        _iood_stop(mon, true);
        return true;
    }

    if (pos == mon.pos())
        return false;

    actor *victim = actor_at(pos);
    if (cell_is_solid(pos) || victim)
    {
        // Just dissipate instead of hitting something.
        _iood_stop(mon, true);
        return true;
    }
    // Boulder doesn't travel over water/lava.
    if (mons_is_boulder(&mon)
        && (!feat_has_solid_floor(grd(pos)) || feat_is_water(grd(pos))))
    {
        _iood_stop(mon, false);
        return true;
    }

    if (!mon.move_to_pos(pos))
    {
        _iood_stop(mon);
        return true;
    }

    // move_to_pos() just trashed the coords, set them again
    mon.props["iood_x"] = x;
    mon.props["iood_y"] = y;

    return false;
}

void iood_catchup(monster* mons, int pturns)
{
    monster& mon = *mons;
    ASSERT(mon.is_projectile());

    const int moves = pturns * mon.speed / BASELINE_DELAY;

    // Handle some cases for IOOD only
    if (mons_is_projectile(mons))
    {
        if (moves > 50)
        {
            _iood_stop(mon, false);
            return;
        }

        if (mon.props["iood_kc"].get_byte() == KC_YOU)
        {
            // Left player's vision.
            _iood_stop(mon, false);
            return;
        }
    }

    for (int i = 0; i < moves; ++i)
        if (_iood_catchup_move(mon))
            return;
}

void boulder_start(monster *mon, bolt *beam)
{
    mon->add_ench(ENCH_ROLLING);
    // Work out x/y/vx/vy from beam
    beam->choose_ray();
    mon->props["iood_x"].get_float() = beam->ray.r.start.x - 0.5;
    mon->props["iood_y"].get_float() = beam->ray.r.start.y - 0.5;
    mon->props["iood_vx"].get_float() = mons_is_fleeing(mon) ?
        -beam->ray.r.dir.x : beam->ray.r.dir.x;
    mon->props["iood_vy"].get_float() = mons_is_fleeing(mon) ?
        -beam->ray.r.dir.y : beam->ray.r.dir.y;
    iood_act(*mon);
}
