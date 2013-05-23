/**
 * @file
 * @brief Movement handlers for specialised movement behaviours
**/

#include "AppHdr.h"

#include "move.h"

#include "actor.h"
#include "areas.h"
#include "coord.h"
#include "directn.h"
#include "env.h"
#include "itemprop.h"
#include "mgen_data.h"
#include "mon-death.h"
#include "monster.h"
#include "mon-place.h"
#include "mon-stuff.h"
#include "ouch.h"
#include "player.h"
#include "random.h"
#include "shout.h"
#include "stuff.h"
#include "terrain.h"
#include "viewchar.h"

#define OOD_SHORT_DIST 2
#define OOD_MED_DIST 3

/**
 * Returns the appropriate movement handler for a given actor.
 *
 * @param act       The actor to get a movement handler for.
 * @return          A new movement handler object.
 */
MovementHandler* MovementHandler::handler_for(actor* act)
{
    MovementHandler* handler = NULL;

    if (act->is_player()) {
        switch (act->as_player()->form)
        {
            case TRAN_BOULDER:
                handler = new PlayerBoulderMovement(act);
                break;
            default:
                handler = new DefaultMovement(act);
                break;
        }
    }
    else switch (act->type)
    {
        case MONS_ORB_OF_DESTRUCTION:
            handler = new OrbMovement(act);
            break;
        case MONS_BOULDER_BEETLE:
            if (mons_is_boulder(act->as_monster()))
                handler = new MonsterBoulderMovement(act);
            break;
        default:
            handler = new DefaultMovement(act);
            break;
    }

    if (!handler)
        handler = new DefaultMovement(act);

    handler->setup();

    return handler;
}

/**
 * Handle movement mode changes.
 *
 * If in the middle of MovementHandler::move(), mark the object for deletion.
 * Otherwise, delete it immediately.
 */
void MovementHandler::killed()
{
    if (!currently_moving && !catching_up)
        delete this;
    else
        kill_after_move = true;
}

/**
 * Take a turn's worth of movement for the owning actor ('subject').
 *
 * Handles setup & cleanup for movement.
 *
 * @return Whether the movement was unsuccessful (!)
 */
bool MovementHandler::move()
{
    // Flag that we're inside the move code, so destruction can be handled
    // properly if the monster dies or the movement handler changes
    currently_moving = true;

    bool movement_problem = attempt_move();

    currently_moving = false;
    // The subject was killed, can safely destruct now the move is over
    if (kill_after_move)
    {
        delete this;
        return true;
    }
    else
        save();

    return movement_problem;
}

/**
 * Take a turn's worth of movement for the owning actor ('subject').
 *
 * @return Whether the movement was unsuccessful (!)
 */
bool MovementHandler::attempt_move()
{
    need_another_move = true;
    while (need_another_move && !kill_after_move)
    {
        need_another_move = false;
        const coord_def destination = get_move_pos();
        // Bounds check
        if (!check_pos(destination))
        {
            stop();
            return true;
        }

        // Final checks that movement is ok
        if (subject->pos() != destination)
        {
            if (!on_moving(destination))
                continue;

            // Save the old position
            const coord_def& old_pos = coord_def(subject->pos());

            // If move_to_pos refuses to move then there was another problem
            if (!subject->move_to_pos(destination))
            {
                stop();
                return true;
            }

            // Tasks to run when a move has completed
            post_move(old_pos);
        }

        post_move_attempt();
    }

    return false; // no problems
}

/**
 * Set up a double move.
 *
 * For situations where you can move through a square but not stay there - e.g
 * a square containing a submerged monster that you can't swap with.
 */
void MovementHandler::move_again()
{
    subject->lose_energy(EUT_MOVE);
    need_another_move = true;
}

/**
 * Simulate monster movement for the time that a player was off-level.
 *
 * Currently unused. (But should maybe be used by OODs?
 *
 * @param pturns    Number of player turns to simulate.
 */
void MovementHandler::catchup(int pturns)
{
    ASSERT(subject->is_monster());

    // Work out how many actual monster moves can be made in
    // this time
    const monster *mons = subject->as_monster();
    const int moves = pturns * mons->speed / BASELINE_DELAY;

    if (on_catchup(moves))
    {
        catching_up = true;

        for (int i = 0; i < moves; ++i)
            if (move())
                return;

        catching_up = false;
    }
}

/**
 * Check if the given position is valid.
 *
 * @param pos   The position to check.
 * @return      Whether that position is valid for this movement mode.
 */
bool MovementHandler::check_pos(const coord_def& pos)
{
    if (!in_bounds(pos))
        return false;
    return true;
}

/**
 * Get the location of the next move.
 *
 * @return      The next square the movement handler will try to move to.
 */
coord_def MovementHandler::get_move_pos()
{
    coord_def dir = get_move_dir();
    return subject->pos() + dir;
}

/**
 * Get the direction of the next move.
 *
 * @return      The vector of the next attempted move.
 */
coord_def MovementHandler::get_move_dir()
{
    // Default to no movement
    return coord_def(0,0);
}

/**
 * Check results of movement. (Has side effects.)
 *
 * Collisions with actors & solid objects.
 *
 * @param pos   The square to examine results of movement into.
 * @return      Whether it's okay to actually move.
 */
bool MovementHandler::on_moving(const coord_def& pos)
{
    // Check for collision with solid materials
    if (cell_is_solid(pos))
        return hit_solid(pos);

    // Check for collision with actors
    actor *victim = actor_at(pos);
    if (victim)
    {
        if (victim->is_player())
            hit_player();
        else if (victim->is_monster())
            hit_monster(victim->as_monster());
        return false;
    }

    return true;
}

/**
 * Read properties from the actor props table so they can be manipulated
 * locally.
 */
void ProjectileMovement::setup()
{
    // For legacy reasons the position and velocity are stored as iood_* props
    pow = subject->props["iood_pow"].get_int();
    x = subject->props["iood_x"].get_float();
    if (!x) x = subject->pos().x;
    y = subject->props["iood_y"].get_float();
    if (!y) y = subject->pos().y;
    vx = subject->props["iood_vx"].get_float();
    vy = subject->props["iood_vy"].get_float();
    distance = subject->props["iood_distance"].get_int();
    kc = subject->props["iood_kc"].get_byte();
    caster_name = subject->props["iood_caster"].get_string();
}

/**
 * Persist properties that might have changed back to the underlying props
 * store after movement.
 */
void ProjectileMovement::save()
{
    subject->props["iood_x"].get_float() = x;
    subject->props["iood_y"].get_float() = y;
    subject->props["iood_vx"].get_float() = vx;
    subject->props["iood_vy"].get_float() = vy;
    subject->props["iood_distance"].get_int() = distance;
}

/**
 * Saves extended properties.
 *
 * Usually when Orbs of Destruction are first created. (But currently unused?)
 */
void ProjectileMovement::save_all()
{
    save();
    subject->props["iood_kc"].get_byte() = kc;
    subject->props["iood_pow"].get_short() = pow;
}


/**
 * Get the location of the next move.
 *
 * Has side-effects.
 *
 * @return      The next square the movement handler will try to move to.
 */
coord_def ProjectileMovement::get_move_pos()
{
    ASSERT(vx || vy);
    normalise();

    // Adjust aim and normalise velocity (except not during catchup)
    if (!catching_up)
    {
        aim();
        normalise();
    }

    // Travelled further
    distance++;

    nx = x + vx;
    ny = y + vy;

    return coord_def(static_cast<int>(round(nx)),
                     static_cast<int>(round(ny)));
}

/**
 * Handle afteraffects of movement into a new square.
 *
 * Place a trailing cloud, if moving normally.
 *
 * @param old_pos       Unused.
 */
void ProjectileMovement::post_move(const coord_def& old_pos)
{
    if (!catching_up)
    {
        place_cloud(trail_type(), subject->pos(),
                    2 + random2(3), subject);
    }
}

/**
 * Handle afteraffects of movement. (Fractional or otherwise.)
 *
 * Set the internal fractional positions after a successful move. (As opposed
 * to the integer positions used by the actual actor.)
 */
void ProjectileMovement::post_move_attempt()
{
    x = nx;
    y = ny;
}

/**
 * Check results of hitting a solid feature.
 *
 * Print an IOOD-impact message if the wall is visible.
 *
 * @param pos       The position of the solid feature.
 * @return          Whether it's OK to move into the feature.
 */
bool ProjectileMovement::hit_solid(const coord_def& pos)
{
    if (you.see_cell(pos) && you.see_cell(subject->pos()))
    {
        mprf("%s hits %s", subject->name(DESC_THE, true).c_str(),
                feature_description_at(pos, false, DESC_A).c_str());
    }
    return false;
}

/**
 * Check results of hitting the player.
 *
 * Has side effects.
 *
 * @return          Whether you actually impacted the player. (Not blocked)
 */
bool ProjectileMovement::hit_player()
{
    return hit_actor(&you);
}

/**
 * Check results of hitting a monster.
 *
 * @param victim    The monster being hit.
 * @return          Whether you actually impacted the monster. (not blocked)
 */
bool ProjectileMovement::hit_monster(monster *victim)
{
    // Don't continue movement on a successful hit of the same type
    if (victim->type == subject->type)
        if (hit_own_kind(victim))
            return false;

    if (victim->submerged()
        || (victim->type == MONS_BATTLESPHERE)) //TODO: check attitude
    {
        // Try to swap with the submerged creature.
        // TODO: might be nice to check if the victim is netted
        if (victim->is_habitable(subject->pos()))
        {
            dprf("iood: Swapping with a monster.");
            const coord_def& pos = victim->pos();
            victim->moveto(subject->pos());
            subject->set_position(pos);
            ASSERT(!victim->is_constricted());
            ASSERT(!victim->is_constricting());
            mgrd(victim->pos()) = victim->mindex();
            mgrd(pos) = subject->mindex();

            return false;
        }
        else
        {
            // If swap fails, move ahead (losing energy)
            dprf("iood: Boosting past a monster (can't swap).");
            move_again();
            return false;
        }
    }

    return hit_actor(victim);
}

bool ProjectileMovement::hit_own_kind(monster *victim)
{
    stop();
    victim->movement()->stop();
    return true;
}

/**
 * Check results of hitting an actor (player or monster).
 *
 * @param victim    The actor being hit.
 * @return          Whether you actually impacted the actor. (not blocked)
 */
bool ProjectileMovement::hit_actor(actor *victim)
{
    // Boulders hitting orbs, etc
    if (victim->movement()->be_hit_by(subject))
        return false;

    // If victim doesn't successfully shield then do nothing; return value
    // of true means the base function can carry on and apply damage.
    if (!victim_shielded(victim)) return true;

    // Normal block
    item_def *shield = victim->shield();
    if (!shield_reflects(*shield))
    {
        if (!subject->is_player() && victim->is_player())
            mprf("You block %s.", subject->name(DESC_THE, true).c_str());
        else if (you.see_cell(subject->pos()))
        {
            simple_monster_message(victim->as_monster(), (" blocks "
                + subject->name(DESC_THE, true) + ".").c_str());
        }
        victim->shield_block_succeeded(subject);
        stop();
        return false;
    }

    // Shield of reflection block
    if (victim->is_player())
    {
        mprf("Your %s reflects %s!",
            shield->name(DESC_PLAIN).c_str(),
            subject->name(DESC_THE, true).c_str());
        ident_reflector(shield);
    }
    else if (you.see_cell(subject->pos()))
    {
        if (victim->observable())
        {
            mprf("%s reflects %s with %s %s!",
                victim->name(DESC_THE, true).c_str(),
                subject->name(DESC_THE, true).c_str(),
                victim->pronoun(PRONOUN_POSSESSIVE).c_str(),
                shield->name(DESC_PLAIN).c_str());
            ident_reflector(shield);
        }
        else if (you.see_cell(subject->pos()))
        {
            mprf("%s bounces off thin air!",
                subject->name(DESC_THE, true).c_str());
        }
    }
    victim->shield_block_succeeded(subject);

    // Mirror velocity
    if (subject->pos().x != victim->pos().x || subject->pos() == victim->pos())
        vx = -vx;
    if (subject->pos().y != victim->pos().y || subject->pos() == victim->pos())
        vy = -vy;

    return false;
}

// Alas, too much differs to reuse beam shield blocks :(
bool ProjectileMovement::victim_shielded(actor *victim)
{
    if (!victim->shield() || victim->incapacitated())
        return false;

    const int to_hit = 15 + get_hit_power();

    const int con_block = random2(to_hit + victim->shield_block_penalty());
    const int pro_block = victim->shield_bonus();
    dprf("iood shield: pro %d, con %d", pro_block, con_block);
    return pro_block >= con_block;
}

// For shielding calculations; actual hits are handled in strike(...)
int ProjectileMovement::get_hit_power()
{
    return div_rand_round(pow,12);
}

void ProjectileMovement::moved_by_other(const coord_def& c)
{
    // Assume some means of displacement, normal moves will overwrite this.
    // TODO: This will get triggered down the line when mon.move_to_pos(pos)
    // is called. Harmless (since it will be adjusted by zero) but seems
    // a bit messy.
    x += c.x - x;
    y += c.y - y;
    save();
}

bool ProjectileMovement::has_trail()
{
    return !catching_up;
}

cloud_type ProjectileMovement::trail_type()
{
    return CLOUD_MAGIC_TRAIL;
}

void ProjectileMovement::normalise()
{
    _normalise(vx,vy);
}

void ProjectileMovement::_normalise(float &x, float &y)
{
    const float d = sqrt(x*x + y*y);
    if (d <= 0.000001)
        return;
    x/=d;
    y/=d;
}

// Angle measured in chord length
bool ProjectileMovement::_in_front(float vx, float vy, float dx, float dy, float angle)
{
    return (dx-vx)*(dx-vx) + (dy-vy)*(dy-vy) <= (angle*angle);
}

// Override setup and save_all methods to handle flawed IOODs cast by player
void OrbMovement::setup()
{
    ProjectileMovement::setup();
    caster_mid = ((monster*)subject)->summoner;
    flawed = subject->props.exists("iood_flawed");
    tpos = subject->props["tpos"].get_short();
}

void OrbMovement::save()
{
    ProjectileMovement::save();
    subject->props["tpos"].get_short() = tpos;
}

void OrbMovement::save_all()
{
    ProjectileMovement::save_all();
    if (flawed)
        subject->props["iood_flawed"].get_byte() = true;

    actor *caster = get_caster();
    if (caster) {
        ((monster*)subject)->summoner = caster->mid;
        subject->props["iood_caster"].get_string() = get_caster_name();
    }
    else {
        ((monster*)subject)->summoner = 0;
        subject->props["iood_caster"] = "none";
    }
}

actor* OrbMovement::get_caster()
{
    return actor_by_mid(caster_mid);
}

string OrbMovement::get_caster_name()
{
    return caster_name;
}

void OrbMovement::aim()
{
    const actor *foe = subject->as_monster()->get_foe();
    // If the target is gone, the orb continues on a ballistic course since
    // picking a new one would require intelligence.
    // Likewise for submerged foes.
    if (!foe || foe->submerged()) return;

    const coord_def target = foe->pos();
    float dx = target.x - x;
    float dy = target.y - y;
    _normalise(dx,dy);

    // XXX: The rest of this function is incomprehensible to me right now.
    // Need to decode it and rewrite in a slightly more understandable way.

    // Special case:
    // Moving diagonally when the orb is just about to hit you
    //      2
    //    ->*1
    // (from 1 to 2) would be a guaranteed escape.  This may be
    // realistic (strafing!), but since the game has no non-cheesy
    // means of waiting a small fraction of a turn, we don't want it.
    const int old_t_pos = tpos;
    const int new_t_pos = 256 * target.x + target.y;
    const coord_def rpos(static_cast<int>(round(x)), static_cast<int>(round(y)));
    if ((old_t_pos && old_t_pos != new_t_pos)
        && (rpos - target).rdist() <= 1
        // ... but following an orb is ok.
        && _in_front(vx, vy, dx, dy, 1.5)) // ~97 degrees
    {
        vx = dx;
        vy = dy;
    }
    tpos = new_t_pos;

    if (!_in_front(vx, vy, dx, dy, 0.3f)) // ~17 degrees
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
}

// Check the Orb hasn't gone too far out of sight
bool OrbMovement::check_pos(const coord_def &pos)
{
    if (flawed)
    {
        // Don't check actual vision, the smoke trail might block it
        const actor *caster = actor_by_mid(caster_mid);
        if (!caster || caster->pos().origin() ||
            (caster->pos() - pos).rdist() > LOS_RADIUS)
        {
            stop();
            return false;
        }
    }
    return ProjectileMovement::check_pos(pos);
}

bool OrbMovement::on_catchup(int moves)
{
    // Dissipate if too many moves or player IOOD
    if (moves > 50 || kc == KC_YOU)
    {
        stop(false);
        return false;
    }
    return true;
}

void OrbMovement::stop(bool show_message)
{
    if (show_message)
        simple_monster_message(subject->as_monster(), " dissipates.");
    dprf("iood: dissipating");
    monster_die(subject->as_monster(), KILL_DISMISSED, NON_MONSTER);
}

bool OrbMovement::hit_solid(const coord_def& pos)
{
    // Base class prints a message
    ProjectileMovement::hit_solid(pos);
    // Strike the position (dissipates orb)
    strike(pos);
    return false;
}

bool OrbMovement::hit_actor(actor *victim)
{
    // If the hit is successful (not shielded) strike the target
    if (ProjectileMovement::hit_actor(victim))
        strike(victim->pos());
    return false;
}

/**
 * Handle orb-orb collisions.
 *
 * Cause an explosion if they've traveled any real distance - otherwise,
 * fizzle.
 *
 * @param pos       The location of the collision.
 * @param victim    The other orb.
 * @return          Whether this satisfactorily resolved the collision, and no
 * generic collision handling is needed. (Always true.)
 */
bool OrbMovement::hit_own_kind(monster *victim)
{
    if (distance < OOD_SHORT_DIST ||
        victim->props["iood_distance"].get_int() < OOD_SHORT_DIST)
    {
        if (you.see_cell(subject->pos()))
            mpr("The orb fizzles.");

        // Dissipate the victim
        victim->movement()->stop(false);
        // Dissipate
        stop();

        return true;
    }

    if (subject->observable())
        mpr("The orbs collide in a blinding explosion!");

    // Dissipate the victim
    victim->movement()->stop(false);

    // Make a big explosion
    strike(victim->pos(), true);
    return true;
}

/**
 * Handle foo-orb collisions.
 *
 * Hit them!
 * @param aggressor     The actor hitting you.
 * @return              Whether this resolved the collision (yes)
 */

bool OrbMovement::be_hit_by(actor* aggressor)
{
    aggressor->movement()->stop(false);
    hit_actor(aggressor);
    return true;
}

// Orbs can strike actors, walls and other features - it acts like a
// wand of disintegration regardless
void OrbMovement::strike(const coord_def &pos, bool big_boom)
{
    monster* mon = subject->as_monster();
    bolt beam;
    beam.name = "orb of destruction";
    beam.flavour = BEAM_NUKE;
    beam.attitude = mon->attitude;

    actor *caster = get_caster();
    if (!caster)        // caster is dead/gone, blame the orb itself (as its
        caster = mon;   // friendliness is correct)
    beam.set_agent(caster);
    beam.colour = WHITE;
    beam.glyph = dchar_glyph(DCHAR_FIRED_BURST);
    beam.range = 1;
    beam.source = pos;
    beam.target = pos;
    beam.hit = AUTOMATIC_HIT;
    beam.source_name = get_caster_name();

    pow = stepdown_value(pow, 30, 30, 200, -1);
    ASSERT(distance >= 0);
    if (distance < 4)
        pow = pow * (distance * 2 + 3) / 10;
    beam.damage = dice_def(9, pow / 4);

    if (distance < 3)
        beam.name = "wavering " + beam.name;
    if (distance < 2)
        beam.hit_verb = "weakly hits";
    beam.ex_size = 1;
    beam.loudness = 7;

    if (big_boom)
        beam.explode(true, false);
    else
        beam.fire();

    stop(false);
}

spret_type OrbMovement::cast_iood(actor *caster, int pow, bolt *beam,
                                  float vx, float vy, int foe, bool fail)
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
        mprf(MSGCH_ERROR, "Failed to spawn projectile.");
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
        ? caster->name(DESC_A, true)
        : (caster->is_player()) ? "you" : "";
    mon->summoner = caster->mid;

    if (caster->is_player() || caster->type == MONS_PLAYER_GHOST
        || caster->type == MONS_PLAYER_ILLUSION)
    {
        mon->props["iood_flawed"].get_byte() = true;
    }

    // Move away from the caster's square.
    // XXX: Prevent trail in this case (just check for occupant of square)
    mon->movement()->move();
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

void OrbMovement::cast_iood_burst(int pow, coord_def target)
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

void OrbMovement::_fuzz_direction(const actor *caster, monster& mon, int pow)
{
    const float x = mon.props["iood_x"];
    const float y = mon.props["iood_y"];
    float vx = mon.props["iood_vx"];
    float vy = mon.props["iood_vy"];

    _normalise(vx, vy);

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

bool BoulderMovement::hit_actor(actor *victim)
{
    if (ProjectileMovement::hit_actor(victim))
    {
        // Continue into the square if the target was killed
        if (strike(victim))
            return true;
        // Otherwise stop rolling
        else
            stop();
    }
    // Don't move into the target square
    return false;
}

// Strike a victim
bool BoulderMovement::strike(actor *victim)
{
    if (subject->is_monster())
    {
        simple_monster_message(subject->as_monster(), (string(" smashes into ")
                                + victim->name(DESC_THE) + "!").c_str());
    }
    else
    {
        mprf("%s smash into %s!",
             subject->name(DESC_THE, true).c_str(),
             victim->name(DESC_THE, true).c_str());
    }
    int dam_max = strike_max_damage();
    int dam = victim->apply_ac(roll_dice(div_rand_round(dam_max,6), dam_max));
    if (victim->is_player())
        ouch(dam, subject->mindex(), KILLED_BY_ROLLING);
    else
        victim->hurt(subject, dam);

    noisy(5, victim->pos());
    return !(victim->alive());
}

bool BoulderMovement::hit_solid(const coord_def& pos)
{
    // Base class prints a message
    ProjectileMovement::hit_solid(pos);

    noisy(10, pos);
    if ((!you.see_cell(pos) || !you.see_cell(subject->pos()))
        && !silenced(you.pos()))
        mprf("You hear a loud crash.");

    // Bounce (deaden the impact if the angle of incidence is too high)
    if (pos.x == subject->pos().x)
    {
        if (abs(vy)<0.5)
            vy = -vy;
        else
            vy = 0;
    }
    if (pos.y == subject->pos().y)
    {
        if (abs(vx)<0.5)
            vx = -vx;
        else
            vx = 0;
    }
    return false;
}

bool BoulderMovement::hit_own_kind(monster *victim)
{
    // Beetle collision. Check the beetle is actually rolling first.
    if (!mons_is_boulder(victim)) return false;

    if (subject->observable())
    {
        mpr("The boulders collide with a stupendous crash!");
        noisy(20, subject->pos());
    }
    else
        noisy(20, subject->pos(), "You hear a loud crashing sound!");

    // Remove ROLLING and add DAZED
    stop(false);
    victim->movement()->stop(false);

    // TODO: Boulders collide with player, player collides with boulder
    if (!victim->check_clarity(false))
        victim->add_ench(ENCH_CONFUSION);
    monster* mon = subject->as_monster();
    if (mon && !mon->check_clarity(false))
        mon->add_ench(ENCH_CONFUSION);
    return true;
}

cloud_type BoulderMovement::trail_type()
{
    return CLOUD_DUST_TRAIL;
}

void MonsterBoulderMovement::aim()
{
    // Slightly adjust vx/vy by a small random amount
    vx += random_range_real(-0.1,0.1,2);
    vy += random_range_real(-0.1,0.1,2);
}

int MonsterBoulderMovement::get_hit_power()
{
    return div_rand_round(subject->as_monster()->hit_dice,2);
}

bool MonsterBoulderMovement::check_pos(const coord_def &pos)
{
    // Boulders stop at lava/water to prevent unusual behaviour;
    // skimming across the water like a pebble could be justifiable, but
    // it raises too many questions.
    // XXX: Boulders used to collide with walls because this check happened elsewhere
    if (!feat_has_solid_floor(grd(pos)) || feat_is_water(grd(pos)))
    {
        if (!catching_up)
            mprf("%s screeches to a halt.", subject->name(DESC_THE, true).c_str());
        stop(false);
        return false;
    }
    return ProjectileMovement::check_pos(pos);
}

// Beetles stop rolling
void MonsterBoulderMovement::stop(bool show_message)
{
    subject->as_monster()->del_ench(ENCH_ROLLING,!show_message);
    // Flag that the movement handler needs to change
    subject->movement_changed();
}

// Static method to begin rolling
void MonsterBoulderMovement::start_rolling(monster *mon, bolt *beam)
{
    // Add the required enchantment
    mon->add_ench(ENCH_ROLLING);
    // Work out x/y/vx/vy from beam. For now pre-populating the props
    // hash so the values get picked up in setup.
    beam->choose_ray();
    mon->props["iood_x"].get_float() = beam->ray.r.start.x - 0.5;
    mon->props["iood_y"].get_float() = beam->ray.r.start.y - 0.5;
    mon->props["iood_vx"].get_float() = mons_is_fleeing(mon) ?
        -beam->ray.r.dir.x : beam->ray.r.dir.x;
    mon->props["iood_vy"].get_float() = mons_is_fleeing(mon) ?
        -beam->ray.r.dir.y : beam->ray.r.dir.y;

    // Flag the movement handler to change, and take an initial move
    mon->movement_changed();
    mon->movement()->move();
}

// Handle impulse
void PlayerBoulderMovement::impulse(float ix, float iy)
{
    // Impulse has less effect the faster you're already going
    vx += (random_range_real(0.9,1.1)*ix)/(2.0f * (abs(vx)<1?1:abs(vx)));
    vy += (random_range_real(0.9,1.1)*iy)/(2.0f * (abs(vy)<1?1:abs(vy)));

    if (abs(vx) > 5.0f)
        vx = 5.0f * sgn(vx);
    if (abs(vy) > 5.0f)
        vy = 5.0f * sgn(vy);
}

bool PlayerBoulderMovement::hit_solid(const coord_def& pos)
{
    if (grd(pos) == DNGN_CLOSED_DOOR || grd(pos) == DNGN_RUNED_DOOR)
    {
        // Slam the door open
        player_open_door(pos, false);
        // Carry on right through it
        return true;
    }

    // Bounce (deaden the impact if the angle of incidence is too high)
    if (pos.x != subject->pos().x)
    {
        if (abs(vx)<0.5)
            vx = -vx;
        else
            vx = 0;
    }
    if (pos.y != subject->pos().y)
    {
        if (abs(vy)<0.5)
            vy = -vy;
        else
            vy = 0;
    }

    if (vx || vy)
        mprf("%s bounce off %s",
             subject->name(DESC_THE, true).c_str(),
             feature_description_at(pos, false, DESC_A).c_str());
    else
        mprf("%s hit %s",
             subject->name(DESC_THE, true).c_str(),
             feature_description_at(pos, false, DESC_A).c_str());

    noisy(5, subject->pos());

    // Acknowledge if this brought you to a standstill
    if (vx == 0 && vy == 0 && subject->alive())
        stop();

    return false;
}

void PlayerBoulderMovement::stop(bool show_message)
{
    vx = 0;
    vy = 0;
    if (show_message)
        mprf("%s start gathering moss.", subject->name(DESC_THE, true).c_str());
}

// How much damage enemies take
int PlayerBoulderMovement::strike_max_damage()
{
    return div_rand_round(ceil(velocity() * 100),10);
}

// Absolute current velocity, used for damage calculations.
// Not needed on base classes since their velocity is always
// normalised to 1.
double PlayerBoulderMovement::velocity()
{
    return sqrt(vx * vx + vy * vy);
}

int PlayerBoulderMovement::get_hit_power()
{
    return div_rand_round(subject->as_player()->experience_level,2);
}

// Override normalisation, to prevent it
void PlayerBoulderMovement::normalise()
{
    // XXX: If abs(vx,vy)>1, take multiple steps so as not to jump over things...
}

void PlayerBoulderMovement::start_rolling()
{
    // TODO: Would be nice to give a little impulse based on the direction.
    // For now it's just a small random velocity.

    // Pre-populating the props hash so the values get picked up in setup.
    you.props["iood_x"].get_float() = (float)you.pos().x - 0.5f;
    you.props["iood_y"].get_float() = (float)you.pos().y - 0.5f;
    you.props["iood_vx"].get_float() = random_real() - 0.5f;
    you.props["iood_vy"].get_float() = random_real() - 0.5f;

    // Flag the movement handler to change, and take an initial move
    you.movement_changed();
    you.movement()->move();
}
