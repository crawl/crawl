/**
 * @file
 * @brief Slow projectiles, done as monsters.
**/

#include "AppHdr.h"

#include "mon-project.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "act-iter.h"
#include "areas.h"
#include "cloud.h"
#include "directn.h"
#include "env.h"
#include "item-prop.h"
#include "message.h"
#include "mgen-data.h"
#include "mon-death.h"
#include "mon-place.h"
#include "ouch.h"
#include "shout.h"
#include "stepdown.h"
#include "terrain.h"
#include "viewchar.h"

static void _fuzz_direction(const actor *caster, monster& mon, int pow);

spret cast_iood(actor *caster, int pow, bolt *beam, float vx, float vy,
                     int foe, bool fail, bool needs_tracer)
{
    const bool is_player = caster->is_player();
    if (beam && is_player && needs_tracer
             && !player_tracer(ZAP_IOOD, pow, *beam))
    {
        return spret::abort;
    }

    fail_check();

    int mtarg = !beam ? MHITNOT :
                beam->target == you.pos() ? int{MHITYOU} : mgrd(beam->target);

    monster *mon = place_monster(mgen_data(MONS_ORB_OF_DESTRUCTION,
                (is_player) ? BEH_FRIENDLY :
                    ((monster*)caster)->friendly() ? BEH_FRIENDLY : BEH_HOSTILE,
                coord_def(),
                mtarg).set_summoned(caster, 0, SPELL_IOOD), true, true);
    if (!mon)
    {
        mprf(MSGCH_ERROR, "Failed to spawn projectile.");
        return spret::abort;
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
        mon->props[IOOD_X].get_float() = beam->ray.r.start.x - 0.5;
        mon->props[IOOD_Y].get_float() = beam->ray.r.start.y - 0.5;
        mon->props[IOOD_VX].get_float() = beam->ray.r.dir.x;
        mon->props[IOOD_VY].get_float() = beam->ray.r.dir.y;
        _fuzz_direction(caster, *mon, pow);
    }
    else
    {
        // Multi-orb: spread the orbs a bit, otherwise diagonal ones might
        // fail to leave the cardinal direction: orb A moves -0.4,+0.9 and
        // orb B +0.4,+0.9, both rounded to 0,1.
        mon->props[IOOD_X].get_float() = caster->pos().x + 0.4 * vx;
        mon->props[IOOD_Y].get_float() = caster->pos().y + 0.4 * vy;
        mon->props[IOOD_VX].get_float() = vx;
        mon->props[IOOD_VY].get_float() = vy;
    }

    mon->props[IOOD_KC].get_byte() = (is_player) ? KC_YOU :
        ((monster*)caster)->friendly() ? KC_FRIENDLY : KC_OTHER;
    mon->props[IOOD_POW].get_short() = pow;
    mon->flags &= ~MF_JUST_SUMMONED;
    mon->props[IOOD_CASTER].get_string() = caster->as_monster()
        ? caster->name(DESC_A, true)
        : (caster->is_player()) ? "you" : "";
    mon->summoner = caster->mid;

    if (caster->is_player() || caster->type == MONS_PLAYER_GHOST
        || caster->type == MONS_PLAYER_ILLUSION)
    {
        mon->props[IOOD_FLAWED].get_byte() = true;
    }

    // Move away from the caster's square.
    iood_act(*mon, true);

    // If the foe was adjacent to the caster, that might have destroyed it.
    if (mon->alive())
    {
        // We need to take at least one full move (for the above), but let's
        // randomize it and take more so players won't get guaranteed instant
        // damage.
        mon->lose_energy(EUT_MOVE, 2, random2(3)+2);

        // Multi-orbs don't home during the first move, they'd likely
        // immediately explode otherwise.
        if (foe != MHITNOT)
            mon->foe = foe;
    }

    return spret::success;
}

/**
 * Find a target for a bursty (non-player-targeted) IOOD.
 *
 * Try to find an enemy that's at a reasonable angle from the angle the IOOD
 * is fired at, preferring the given foe (if a non-MHITNOT foe is given) if
 * they're valid, and otherwise preferring the closest valid foe.
 *
 * @param angle             The angle that the IOOD will be fired at, relative
 *                          to the player's position.
 * @param preferred_foe     The mindex of a target to choose if possible; may
 *                          be MHITNOT (no preferred target)
 * @return                  The mindex of a valid target for the IOOD.
 */
static int _burst_iood_target(double iood_angle, int preferred_foe)
{
    int closest_foe = MHITNOT;
    int closest_dist = INT_MAX;

    for (monster_near_iterator mi(you.pos(), LOS_SOLID); mi; ++mi)
    {
        const monster* m = *mi;
        ASSERT(m);

        if (!you.can_see(*m) || mons_is_projectile(*m))
            continue;

        // is this position at a valid angle?
        const coord_def delta = mi->pos() - you.pos();
        const double angle = atan2(delta.x, delta.y);
        const double abs_angle_diff = abs(angle - fmod(iood_angle, PI * 2));
        const double angle_diff = (abs_angle_diff > PI) ?
                                        2 * PI - abs_angle_diff :
                                        abs_angle_diff;
        if (angle_diff >= PI / 3)
        {
            dprf("can't target %s; angle diff %f",
                 m->name(DESC_PLAIN).c_str(), angle_diff);
            continue;
        }

        // if preferred foe is valid, choose it.
        if (m->mindex() == preferred_foe)
        {
            dprf("preferred target %s is valid burst target (delta %f)",
                 m->name(DESC_PLAIN).c_str(), angle_diff);
            return preferred_foe;
        }

        if (mons_aligned(m, &you) || mons_is_firewood(*m))
        {
            dprf("skipping invalid burst target %s (%s)",
                 m->name(DESC_PLAIN).c_str(),
                 mons_aligned(m, &you) ? "aligned" : "firewood");
            continue;
        }

        const int dist = grid_distance(m->pos(), you.pos());
        // on distance ties, bias by iterator order (mindex)
        if (dist >= closest_dist)
        {
            dprf("%s not closer to target than closest (%d vs %d)",
                 m->name(DESC_PLAIN).c_str(), dist, closest_dist);
            continue;
        }

        dprf("%s is valid burst target (delta %f, dist %d)",
             m->name(DESC_PLAIN).c_str(), angle_diff, dist);
        closest_dist = dist;
        closest_foe = m->mindex();
    }

    const int foe = closest_foe != MHITNOT ? closest_foe : preferred_foe;
    dprf("targeting %d", foe);
    return foe;
}

void cast_iood_burst(int pow, coord_def target)
{
    const monster* mons = monster_at(target);
    const int preferred_foe = mons && you.can_see(*mons) ?
                              mons->mindex() :
                              MHITNOT;

    const int n_orbs = random_range(3, 7);
    dprf("Bursting %d orbs.", n_orbs);
    const double angle0 = random2(2097152) * PI * 2 / 2097152;

    for (int i = 0; i < n_orbs; i++)
    {
        const double angle = angle0 + i * PI * 2 / n_orbs;
        const int foe = _burst_iood_target(angle, preferred_foe);
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
    return (dx-vx)*(dx-vx) + (dy-vy)*(dy-vy) <= (angle*angle);
}

static void _iood_stop(monster& mon, bool msg = true)
{
    if (!mon.alive())
        return;

    if (msg)
        simple_monster_message(mon, " dissipates.");
    dprf("iood: dissipating");
    monster_die(mon, KILL_DISMISSED, NON_MONSTER);
}

static void _fuzz_direction(const actor *caster, monster& mon, int pow)
{
    const float x = mon.props[IOOD_X];
    const float y = mon.props[IOOD_Y];
    float vx = mon.props[IOOD_VX];
    float vy = mon.props[IOOD_VY];

    _normalize(vx, vy);

    if (pow < 10)
        pow = 10;
    const float off = random_choose(-0.25, 0.25);
    float tan = (random2(31) - 15) * 0.019; // approx from degrees
    tan *= 75.0 / pow;
    const int inaccuracy = caster ? caster->inaccuracy() : 0;
    if (inaccuracy > 0)
        tan *= 2 * inaccuracy;

    // Cast either from left or right hand.
    mon.props[IOOD_X] = x + vy*off;
    mon.props[IOOD_Y] = y - vx*off;
    // And off the direction a bit.
    mon.props[IOOD_VX] = vx + vy*tan;
    mon.props[IOOD_VY] = vy - vx*tan;
}

// Alas, too much differs to reuse beam shield blocks :(
static bool _iood_shielded(monster& mon, actor &victim)
{
    if (!victim.shielded() || victim.incapacitated())
        return false;

    const int to_hit = 15 + (mons_is_projectile(mon.type) ?
        mon.props[IOOD_POW].get_short()/12 : mon.get_hit_dice()/2);
    const int con_block = random2(to_hit + victim.shield_block_penalty());
    const int pro_block = victim.shield_bonus();
    dprf("iood shield: pro %d, con %d", pro_block, con_block);
    return pro_block >= con_block;
}

static bool _iood_hit(monster& mon, const coord_def &pos, bool big_boom = false)
{
    bolt beam;
    beam.name = "orb of destruction";
    beam.flavour = BEAM_DEVASTATION;
    beam.attitude = mon.attitude;

    actor *caster = actor_by_mid(mon.summoner);
    if (!caster)        // caster is dead/gone, blame the orb itself (as its
        caster = &mon;  // friendliness is correct)
    beam.set_agent(caster);
    if (mon.props.exists(IOOD_REFLECTOR))
    {
        beam.reflections = 1;

        const mid_t refl_mid = mon.props[IOOD_REFLECTOR].get_int64();

        if (refl_mid == MID_PLAYER)
            beam.reflector = MID_PLAYER;
        else
        {
            // If the reflecting monster has died, credit the original caster.
            const monster * const rmon = monster_by_mid(refl_mid);
            beam.reflector = rmon ? refl_mid : caster->mid;
        }
    }
    beam.colour = WHITE;
    beam.glyph = dchar_glyph(DCHAR_FIRED_BURST);
    beam.range = 1;
    beam.source = pos;
    beam.target = pos;
    beam.hit = AUTOMATIC_HIT;
    beam.source_name = mon.props[IOOD_CASTER].get_string();

    int pow = mon.props[IOOD_POW].get_short();
    pow = stepdown_value(pow, 30, 30, 200, -1);
    const int dist = mon.props[IOOD_DIST].get_int();
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

    monster_die(mon, KILL_DISMISSED, NON_MONSTER);

    if (big_boom)
        beam.explode(true, false);
    else
        beam.fire();

    return true;
}

// returns true if the orb is gone
bool iood_act(monster& mon, bool no_trail)
{
    ASSERT(mons_is_projectile(mon.type));

    float x = mon.props[IOOD_X];
    float y = mon.props[IOOD_Y];
    float vx = mon.props[IOOD_VX];
    float vy = mon.props[IOOD_VY];

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

    // IOODs can't home in on a submerged creature.
    if (foe && !foe->submerged())
    {
        const coord_def target = foe->pos();
        float dx = target.x - x;
        float dy = target.y - y;
        _normalize(dx, dy);

        // Special case:
        // Moving diagonally when the orb is just about to hit you
        //      2
        //    ->*1
        // (from 1 to 2) would be a guaranteed escape. This may be
        // realistic (strafing!), but since the game has no non-cheesy
        // means of waiting a small fraction of a turn, we don't want it.
        const int old_t_pos = mon.props[IOOD_TPOS].get_short();
        const coord_def rpos(static_cast<int>(round(x)), static_cast<int>(round(y)));
        if (old_t_pos && old_t_pos != (256 * target.x + target.y)
            && (rpos - target).rdist() <= 1
            // ... but following an orb is ok.
            && _in_front(vx, vy, dx, dy, 1.5)) // ~97 degrees
        {
            vx = dx;
            vy = dy;
        }
        mon.props[IOOD_TPOS].get_short() = 256 * target.x + target.y;

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
        mon.props[IOOD_VX] = vx;
        mon.props[IOOD_VY] = vy;
    }

move_again:
    coord_def starting_pos = (mon.pos() == coord_def()) ?
                                                coord_def(x, y) : mon.pos();

    x += vx;
    y += vy;

    mon.props[IOOD_X] = x;
    mon.props[IOOD_Y] = y;
    mon.props[IOOD_DIST].get_int()++;

    const coord_def pos(static_cast<int>(round(x)), static_cast<int>(round(y)));
    if (!in_bounds(pos))
    {
        _iood_stop(mon);
        return true;
    }

    if (mon.props.exists(IOOD_FLAWED))
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
        place_cloud(CLOUD_MAGIC_TRAIL, starting_pos, 2 + random2(3), &mon);

    actor *victim = actor_at(pos);
    if (cell_is_solid(pos) || victim)
    {
        if (cell_is_solid(pos)
            && you.see_cell(pos)
            && you.see_cell(starting_pos))
        {
            mprf("%s hits %s.", mon.name(DESC_THE, true).c_str(),
                 feature_description_at(pos, false, DESC_A).c_str());
        }

        monster* mons = (victim && victim->is_monster()) ?
            (monster*) victim : 0;

        if (mons && mons_is_projectile(victim->type))
        {
            // Weak orbs just fizzle instead of exploding.
            if (mons->props[IOOD_DIST].get_int() < 2
                || mon.props[IOOD_DIST].get_int() < 2)
            {
                if (mons->props[IOOD_DIST].get_int() < 2)
                {
                    if (you.see_cell(pos))
                        mpr("The orb fizzles.");
                    monster_die(*mons, KILL_DISMISSED, NON_MONSTER);
                }

                // Return, if the acting orb fizzled.
                if (mon.props[IOOD_DIST].get_int() < 2)
                {
                    if (you.see_cell(pos))
                        mpr("The orb fizzles.");
                    monster_die(mon, KILL_DISMISSED, NON_MONSTER);
                    return true;
                }
            }
            else
            {
                if (mon.observable())
                    mpr("The orbs collide in a blinding explosion!");
                else
                    mpr("You hear a loud magical explosion!");
                noisy(40, pos);
                monster_die(*mons, KILL_DISMISSED, NON_MONSTER);
                _iood_hit(mon, pos, true);
                return true;
            }
        }

        if (mons && (mons->submerged() || mons->type == MONS_BATTLESPHERE))
        {
            // Try to swap with the submerged creature.
            if (mon.swap_with(mons))
            {
                dprf("iood: Swapping with a submerged monster.");
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
            if ((!shield || !shield_reflects(*shield)) && !victim->reflection())
            {
                if (victim->is_player())
                    mprf("You block %s.", mon.name(DESC_THE, true).c_str());
                else
                {
                    simple_monster_message(*mons, (" blocks "
                        + mon.name(DESC_THE, true) + ".").c_str());
                }
                victim->shield_block_succeeded(&mon);
                _iood_stop(mon);
                return true;
            }

            if (victim->is_player())
            {
                if (shield && shield_reflects(*shield))
                {
                    mprf("Your %s reflects %s!",
                         shield->name(DESC_PLAIN).c_str(),
                         mon.name(DESC_THE, true).c_str());
                    ident_reflector(shield);
                }
                else // has reflection property not from shield
                {
                    mprf("%s reflects off an invisible shield around you!",
                         mon.name(DESC_THE, true).c_str());
                }
            }
            else if (you.see_cell(pos))
            {
                if (victim->observable())
                {
                    if (shield && shield_reflects(*shield))
                    {
                        mprf("%s reflects %s off %s %s!",
                             victim->name(DESC_THE, true).c_str(),
                             mon.name(DESC_THE, true).c_str(),
                             victim->pronoun(PRONOUN_POSSESSIVE).c_str(),
                             shield->name(DESC_PLAIN).c_str());
                        ident_reflector(shield);
                    }
                    else
                    {
                        mprf("%s reflects off an invisible shield around %s!",
                             mon.name(DESC_THE, true).c_str(),
                             victim->name(DESC_THE, true).c_str());

                        item_def *amulet = victim->slot_item(EQ_AMULET);
                        if (amulet)
                            ident_reflector(amulet);
                    }
                }
                else
                {
                    mprf("%s bounces off of thin air!",
                         mon.name(DESC_THE, true).c_str());
                }
            }
            victim->shield_block_succeeded(&mon);

            // mid_t is unsigned so won't fit in a plain int
            mon.props[IOOD_REFLECTOR] = (int64_t) victim->mid;
            mon.props[IOOD_VX] = vx = -vx;
            mon.props[IOOD_VY] = vy = -vy;

            // Need to get out of the victim's square.

            // If you're next to the caster and both of you wear shields of
            // reflection, this can lead to a brief game of ping-pong, but
            // rapidly increasing shield penalties will make it short.
            mon.lose_energy(EUT_MOVE);
            goto move_again;
        }

        if (_iood_hit(mon, pos))
            return true;
    }

    if (!mon.move_to_pos(pos))
    {
        _iood_stop(mon);
        return true;
    }

    // move_to_pos() just trashed the coords, set them again
    mon.props[IOOD_X] = x;
    mon.props[IOOD_Y] = y;

    return false;
}

// Reduced copy of iood_act to move the orb while the player is off-level.
// Just goes straight and dissipates instead of hitting anything.
static bool _iood_catchup_move(monster& mon)
{
    float x = mon.props[IOOD_X];
    float y = mon.props[IOOD_Y];
    float vx = mon.props[IOOD_VX];
    float vy = mon.props[IOOD_VY];

    if (!vx && !vy) // not initialized
    {
        _iood_stop(mon, false);
        return true;
    }

    _normalize(vx, vy);

    x += vx;
    y += vy;

    mon.props[IOOD_X] = x;
    mon.props[IOOD_Y] = y;
    mon.props[IOOD_DIST].get_int()++;

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

    if (!mon.move_to_pos(pos))
    {
        _iood_stop(mon);
        return true;
    }

    // move_to_pos() just trashed the coords, set them again
    mon.props[IOOD_X] = x;
    mon.props[IOOD_Y] = y;

    return false;
}

void iood_catchup(monster* mons, int pturns)
{
    monster& mon = *mons;
    ASSERT(mons_is_projectile(*mons));

    const int moves = pturns * mon.speed / BASELINE_DELAY;

    // Handle some cases for IOOD only
    if (mons_is_projectile(*mons))
    {
        if (moves > 50)
        {
            _iood_stop(mon, false);
            return;
        }

        if (mon.props[IOOD_KC].get_byte() == KC_YOU)
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
