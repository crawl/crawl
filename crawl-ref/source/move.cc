/**
 * @file
 * @brief Movement handlers for specialised movement behaviours
**/

#include "AppHdr.h"

#include "move.h"

#include "actor.h"
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

void MovementHandler::killed()
{
    if (!currently_moving && !catching_up)
        delete this;
    else
        kill_after_move = true;
}

bool MovementHandler::move()
{
    bool result = false;
    // Flag that we're inside the move code, so destruction can be handled
    // properly if the monster dies or the movement handler changes
    currently_moving = true;
    need_another_move = true;
    while (need_another_move && !kill_after_move)
    {
        need_another_move = false;
        const coord_def destination = get_move_pos();
        if (destination.origin())
            return true;
        // Bounds check
        if (!check_pos(destination))
        {
            result = true;
            stop();
        }
        // Final checks that movement is ok
        else if (subject->pos() != destination && on_moving(destination))
        {
            // Save the old position
            const coord_def& old_pos = coord_def(subject->pos());

            // If move_to_pos refuses to move then there was another problem
            if (!subject->move_to_pos(destination))
            {
                stop();
                result = true;
            }
            // Tasks to run when a move has completed
            else if (!on_moved(old_pos))
                result = true;
        }
        // Post-move is called regardless of whether the move was successful
        // since fractional positions might be updated
        if (!result)
            post_move(destination);
    }
    currently_moving = false;
    // The subject was killed, can safely destruct now the move is over
    if (kill_after_move)
    {
        delete this;
        return true;
    }
    else
    {
        save();
    }
    return result;
}

void MovementHandler::move_again()
{
    subject->lose_energy(EUT_MOVE);
    need_another_move = true;
}

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

bool MovementHandler::check_pos(const coord_def& pos)
{
    if (!in_bounds(pos))
        return false;
    return true;
}

coord_def MovementHandler::get_move_pos()
{
    coord_def dir = get_move_dir();
    return subject->pos() + dir;
}

coord_def MovementHandler::get_move_dir()
{
    // Default to no movement
    return coord_def(0,0);
}

bool MovementHandler::on_moving(const coord_def& pos)
{
    // Check for collision with solid materials
    if (cell_is_solid(pos))
    {
        bool player_sees = you.see_cell(pos);
        if (player_sees)
        {
            mprf("%s hits %s", subject->name(DESC_THE, true).c_str(),
                    feature_description_at(pos, false, DESC_A).c_str());
        }

        hit_solid(pos);
        return false;
    }

    // Check for collision with actors
    actor *victim = actor_at(pos);
    if (victim)
    {
        bool continue_move = false;
        if (victim->is_player())
        {
            hit_player(pos);
        }
        else if (victim->is_monster())
        {
            continue_move = hit_monster(pos, victim->as_monster());
        }
        return continue_move;
    }

    return true;
}

// Read properties from the actor props table so they can be manipulated
// locally
void ProjectileMovement::setup()
{
    // For legacy reasons the position and velocity are stored as iood_* props
    caster_mid = subject->props["iood_mid"].get_int();
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

// Persist properties that might have changed back to the underlying props
// store after movement
void ProjectileMovement::save()
{
    subject->props["iood_x"].get_float() = x;
    subject->props["iood_y"].get_float() = y;
    subject->props["iood_vx"].get_float() = vx;
    subject->props["iood_vy"].get_float() = vy;
    subject->props["iood_distance"].get_int() = distance;
}

// Saves extended properties, usually when the Orb is first created
void ProjectileMovement::save_all()
{
    save();
    subject->props["iood_kc"].get_byte() = kc;
    subject->props["iood_pow"].get_short() = pow;

    actor *caster = get_caster();
    if (caster) {
        subject->props["iood_mid"].get_int() = caster->mid;
        subject->props["iood_caster"].get_string() = get_caster_name();
    }
    else {
        subject->props["iood_mid"] = 0;
        subject->props["iood_caster"] = "none";
    }
}

string ProjectileMovement::get_caster_name()
{
    return caster_name;
}

coord_def ProjectileMovement::get_move_pos()
{
    // TODO: Following should never happen. Change to ASSERT?
    if (!vx && !vy) // not initialized
    {
        stop();
        // TODO: Currently the origin acts as a magic word in this context,
        // there are better ways to handle this case (plus it shouldn't ever
        // happen)
        return coord_def(0,0);
    }

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

    return coord_def(static_cast<int>(round(nx)), static_cast<int>(round(ny)));
}

void ProjectileMovement::post_move(const coord_def& new_pos)
{
    x = nx;
    y = ny;
}

bool ProjectileMovement::on_moved(const coord_def& old_pos)
{
    if (!catching_up)
    {
        place_cloud(trail_type(), subject->pos(),
                    2 + random2(3), subject);
    }
    return true;
}

void ProjectileMovement::hit_solid(const coord_def& pos)
{
    if (you.see_cell(pos))
    {
        mprf("%s hits %s", subject->name(DESC_THE, true).c_str(),
                feature_description_at(pos, false, DESC_A).c_str());
    }
}

bool ProjectileMovement::hit_player(const coord_def& pos)
{
    return hit_actor(pos, &you);
}

bool ProjectileMovement::hit_monster(const coord_def& pos, monster *victim)
{
    // Don't continue movement on a successful hit of the same type
    // TODO: Handle boulders and orbs hitting each other, and potential
    // as-yet-uncreated scenarios...
    if (victim->type == subject->type)
        if (hit_own_kind(pos, victim))
            return false;

    if (victim->submerged() || victim->type == MONS_BATTLESPHERE)
    {
        // Try to swap with the submerged creature.
        if (victim->is_habitable(subject->pos()))
        {
            dprf("iood: Swapping with a submerged monster.");
            victim->set_position(subject->pos());
            subject->set_position(pos);
            // TODO: These asserts seem dangerous?
            ASSERT(!victim->is_constricted());
            ASSERT(!victim->is_constricting());
            mgrd(victim->pos()) = victim->mindex();
            mgrd(pos) = subject->mindex();

            return false;
        }
        else
        {
            // If swap fails, move ahead (losing energy)
            dprf("iood: Boosting above a submerged monster (can't swap).");
            move_again();
            return false;
        }
    }

    return hit_actor(pos, victim);
}

bool ProjectileMovement::hit_own_kind(const coord_def& pos, monster *victim)
{
    stop();
    victim->movement()->stop();
    return true;
}

// Handle hits for player or monster
bool ProjectileMovement::hit_actor(const coord_def& pos, actor *victim)
{
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
    if (subject->pos().x != pos.x || subject->pos() == pos)
        vx = -vx;
    if (subject->pos().y != pos.y || subject->pos() == pos)
        vy = -vy;

    // Need to get out of the victim's square.
    // XXX: How does this work now since the move doesn't happen if
    // something is in the way?

    // If you're next to the caster and both of you wear shields of
    // reflection, this can lead to a brief game of ping-pong, but
    // rapidly increasing shield penalties will make it short.
    move_again();
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
    return (pro_block >= con_block);
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

actor* ProjectileMovement::get_caster()
{
    return actor_by_mid(caster_mid);
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
    return ((dx-vx)*(dx-vx) + (dy-vy)*(dy-vy) <= (angle*angle));
}

// Override setup and save_all methods to handle flawed IOODs cast by player
void OrbMovement::setup()
{
    ProjectileMovement::setup();
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
}

void OrbMovement::aim()
{
    const actor *foe = subject->as_monster()->get_foe();
    // If the target is gone, the orb continues on a ballistic course since
    // picking a new one would require intelligence.
    if (!foe) return;

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

void OrbMovement::hit_solid(const coord_def& pos)
{
    // Base class prints a message
    ProjectileMovement::hit_solid(pos);
    // Strike the position (dissipates orb)
    strike(pos);
}

bool OrbMovement::hit_actor(const coord_def& pos, actor *victim)
{
    // If the hit is successful (not shielded) strike the target
    if (ProjectileMovement::hit_actor(pos, victim))
        strike(pos);
    return false;
}

// Orb collision
bool OrbMovement::hit_own_kind(const coord_def& pos, monster *victim)
{
    if (subject->observable())
        mpr("The orbs collide in a blinding explosion!");
    else
        noisy(40, subject->pos(), "You hear a loud magical explosion!");

    // Dissipate the victim
    victim->movement()->stop(false);

    // Make a big explosion
    strike(victim->pos(), true);
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
        ? caster->name(DESC_PLAIN, true)
        : (caster->is_player()) ? "you" : "";
    mon->props["iood_mid"].get_int() = caster->mid;

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

bool BoulderMovement::hit_actor(const coord_def& pos, actor *victim)
{
    if (ProjectileMovement::hit_actor(pos, victim))
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
    int dam = victim->apply_ac(roll_dice(3, 20));
    if (victim->is_player())
        ouch(dam, subject->mindex(), KILLED_BY_ROLLING);
    else
        victim->hurt(subject, dam);

    noisy(5, victim->pos());
    return !(victim->alive());
}

void BoulderMovement::hit_solid(const coord_def& pos)
{
    // Base class prints a message
    ProjectileMovement::hit_solid(pos);
    // Stop rolling
    stop();
}

bool BoulderMovement::hit_own_kind(const coord_def& pos, monster *victim)
{
    // Beetle collision. Check the beetle is actually rolling first.
    if (!mons_is_boulder(victim)) return false;

    if (subject->observable())
        mpr("The boulders collide with a stupendous crash!");
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
void MonsterBoulderMovement::stop(bool show_message = true)
{
    // Deduct the energy first - the move they made that just stopped
    // them was a speed 14 move.
    // XXX: Shouldn't be necessary now
    subject->as_monster()->del_ench(ENCH_ROLLING,!show_message);
    subject->lose_energy(EUT_MOVE);
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

void PlayerBoulderMovement::impulse(float ix, float iy)
{
    vx += ix/5.0f;
    vy += iy/5.0f;

    if (abs(vx) > 5.0f)
        vx = 5.0f * sgn(vx);
    if (abs(vy) > 5.0f)
        vy = 5.0f * sgn(vy);
}

void PlayerBoulderMovement::stop(bool show_message = true)
{
    // Sudden halt
    // TODO: Bounce, take damage, etc.
    vx = 0;
    vy = 0;
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
