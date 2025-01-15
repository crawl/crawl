/**
 * @file
 * @brief Monsters doing stuff (monsters acting).
**/

#include "AppHdr.h"

#include "mon-act.h"

#include "act-iter.h"
#include "areas.h"
#include "arena.h"
#include "attitude-change.h"
#include "bloodspatter.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "corpse.h"
#include "dbg-scan.h"
#include "delay.h"
#include "directn.h" // feature_description_at
#include "dungeon.h"
#include "english.h" // apostrophise
#include "fight.h"
#include "fineff.h"
#include "ghost.h"
#include "god-abil.h" // GOZAG_GOLD_AURA_KEY
#include "god-passive.h"
#include "god-prayer.h"
#include "hints.h"
#include "item-name.h"
#include "item-prop.h"
#include "item-status-flag-type.h"
#include "items.h"
#include "level-state-type.h"
#include "libutil.h"
#include "losglobal.h"
#include "los.h"
#include "mapmark.h"
#include "message.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-book.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-movetarget.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-project.h"
#include "mon-speak.h"
#include "mon-tentacle.h"
#include "nearby-danger.h"
#include "religion.h"
#include "shout.h"
#include "spl-book.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-monench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "spl-zap.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "throw.h"
#include "timed-effects.h"
#include "traps.h"
#include "viewchar.h"
#include "view.h"

static bool _handle_pickup(monster* mons);
static bool _monster_move(monster* mons, coord_def& delta);
static bool _monster_swaps_places(monster* mon, const coord_def& delta);
static bool _do_move_monster(monster& mons, const coord_def& delta);

/**
 * Get the monster's "hit dice".
 *
 * @return          The monster's HD.
 */
int monster::get_hit_dice() const
{
    const int base_hd = get_experience_level();

    const mon_enchant drain_ench = get_ench(ENCH_DRAINED);
    const int drained_hd = base_hd - drain_ench.degree;

    // temp malmuts (-25% HD)
    if (has_ench(ENCH_WRETCHED))
        return max(drained_hd * 3 / 4, 1);
    else if (has_ench(ENCH_TEMPERED))
        return max(drained_hd + 4, 1);
    return max(drained_hd, 1);
}

/**
 * Get the monster's "experience level" - their hit dice, unmodified by
 * temporary enchantments (draining).
 *
 * @return          The monster's XL.
 */
int monster::get_experience_level() const
{
    return hit_dice;
}

static const coord_def mon_compass[8] =
{
    { -1,-1 }, { 0,-1 }, {  1,-1 }, {  1,0 }, // bjnl
    {  1, 1 }, { 0, 1 }, { -1, 1 }, { -1,0 }  // ukyh
};

static int _compass_idx(const coord_def& mov)
{
    for (int i = 0; i < 8; i++)
        if (mon_compass[i] == mov)
            return i;
    return -1;
}

static inline bool _mons_natural_regen_roll(monster* mons)
{
    const int regen_rate = mons->natural_regen_rate();
    return x_chance_in_y(regen_rate, 25);
}

// Do natural regeneration for monster.
static void _monster_regenerate(monster* mons)
{
    // Early bailout so that regen-based triggers don't get spammed
    if (mons->hit_points == mons->max_hit_points)
        return;

    if (crawl_state.disables[DIS_MON_REGEN])
        return;

    if (mons->has_ench(ENCH_SICK)
        || !mons_can_regenerate(*mons) && !(mons->has_ench(ENCH_REGENERATION)))
    {
        return;
    }

    // Non-land creatures out of their element cannot regenerate.
    if (mons_primary_habitat(*mons) != HT_LAND
        && !monster_habitable_grid(mons, mons->pos()))
    {
        return;
    }

    if (mons_class_fast_regen(mons->type)
        || _mons_natural_regen_roll(mons))
    {
        mons->heal(mons_class_regen_amount(mons->type));
    }

    if (mons->has_ench(ENCH_REGENERATION))
        mons->heal(3 + div_rand_round(mons->max_hit_points, 20));

    if (mons_is_hepliaklqana_ancestor(mons->type))
    {
        if (mons->hit_points == mons->max_hit_points && you.can_see(*mons))
            interrupt_activity(activity_interrupt::ancestor_hp);
    }
}

static void _escape_water_hold(monster& mons)
{
    if (mons.has_ench(ENCH_WATER_HOLD))
    {
        simple_monster_message(mons, " is no longer engulfed.");
        mons.del_ench(ENCH_WATER_HOLD);
    }
}

// Returns true iff the monster does nothing.
static bool _handle_ru_melee_redirection(monster &mons, monster **new_target)
{
    // Check to see if your religion redirects the attack
    if (!does_ru_wanna_redirect(mons))
        return false;

    const ru_interference interference =
            get_ru_attack_interference_level();
    if (interference == DO_BLOCK_ATTACK)
    {
        simple_monster_message(mons,
            " is stunned by your conviction and fails to attack.", false,
            MSGCH_GOD);
        return true;
    }
    if (interference == DO_REDIRECT_ATTACK)
    {
        // get a target
        int pfound = 0;
        for (adjacent_iterator ai(mons.pos(), false); ai; ++ai)
        {
            monster* candidate = monster_at(*ai);
            if (!candidate
                || mons_is_projectile(candidate->type)
                || candidate->is_firewood()
                || candidate->friendly())
            {
                continue;
            }
            ASSERT(candidate);
            if (one_chance_in(++pfound))
                *new_target = candidate;
        }
    }
    return false;
}

static void _melee_attack_player(monster &mons, monster* ru_target)
{
    if (ru_target)
    {
        // attack that target
        mons.target = ru_target->pos();
        mons.foe = ru_target->mindex();
        mprf(MSGCH_GOD, "You redirect %s's attack!",
             mons.name(DESC_THE).c_str());
        fight_melee(&mons, ru_target);
    }
    else
        fight_melee(&mons, &you, nullptr, false);
}

static bool _swap_monsters(monster& mover, monster& moved)
{
    // Can't swap with a stationary monster.
    // Although nominally stationary kraken tentacles can be swapped
    // with the main body.
    if (moved.is_stationary() && !moved.is_child_tentacle())
        return false;

    // If the target monster is constricted it is stuck
    // and not eligible to be swapped with
    if (moved.is_constricted() || moved.has_ench(ENCH_BOUND))
    {
        dprf("%s fails to swap with %s, %s.",
            mover.name(DESC_THE).c_str(),
            moved.name(DESC_THE).c_str(),
            moved.is_constricted() ? "constricted" : "bound in place");
            return false;
    }

    // Swapping is a purposeful action.
    if (mover.confused())
        return false;

    // Right now just happens in sanctuary.
    if (!is_sanctuary(mover.pos()) || !is_sanctuary(moved.pos()))
        return false;

    // A friendly or good-neutral monster moving past a fleeing hostile
    // or neutral monster, or vice versa.
    if (mover.wont_attack() == moved.wont_attack()
        || mons_is_retreating(mover) == mons_is_retreating(moved))
    {
        return false;
    }

    // Don't swap places if the player explicitly ordered their pet to
    // attack monsters.
    if ((mover.friendly() || moved.friendly())
        && you.pet_target != MHITYOU && you.pet_target != MHITNOT)
    {
        return false;
    }

    // Okay, we can probably do the swap.
    if (!mover.swap_with(&moved))
        return false;

    if (you.can_see(mover) && you.can_see(moved))
    {
        mprf("%s and %s swap places.", mover.name(DESC_THE).c_str(),
             moved.name(DESC_THE).c_str());
    }

    mover.did_deliberate_movement();
    moved.did_deliberate_movement();

    if (moved.type == MONS_FOXFIRE)
    {
        mprf(MSGCH_GOD, "By Zin's power the foxfire is contained!");
        monster_die(moved, KILL_RESET, NON_MONSTER, true);
    }

    return true;
}



static energy_use_type _get_swim_or_move(monster& mon)
{
    const dungeon_feature_type feat = env.grid(mon.pos());
    // FIXME: Replace check with mons_is_swimming()?
    return (feat_is_lava(feat) || feat_is_water(feat))
            && mon.ground_level() ? EUT_SWIM : EUT_MOVE;
}

static void _swim_or_move_energy(monster& mon)
{
    mon.lose_energy(_get_swim_or_move(mon));
}

static bool _unfriendly_or_impaired(const monster& mon)
{
    return !mon.wont_attack() || mon.has_ench(ENCH_FRENZIED) || mon.confused();
}

// Check up to eight grids in the given direction for whether there's a
// monster of the same alignment as the given monster that happens to
// have a ranged attack. If this is true for the first monster encountered,
// returns true. Otherwise returns false.
static bool _ranged_ally_in_dir(monster* mon, coord_def p)
{
    coord_def pos = mon->pos();

    for (int i = 1; i <= LOS_RADIUS; i++)
    {
        pos += p;
        if (!in_bounds(pos))
            break;

        const actor* ally = actor_at(pos);
        if (ally == nullptr)
            continue;

        if (mons_aligned(mon, ally))
        {
            // Hostile monsters of normal intelligence only move aside for
            // monsters of the same type.
            if (_unfriendly_or_impaired(*mon)
                && mons_genus(mon->type) != mons_genus(ally->type))
            {
                return false;
            }

            // XXX: Sometimes the player wants allies in front of them to stay
            // out of LOF. However use of allies for cover is extremely common,
            // so it doesn't work well to always have allies move out of player
            // LOF. Until a better interface or method can be found to handle
            // both cases, have allies move out of the way only for other
            // monsters.
            if (ally->is_monster())
                return mons_has_ranged_attack(*(ally->as_monster()));
        }
        break;
    }
    return false;
}

// Check whether there's a monster of the same type and alignment adjacent
// to the given monster in the given direction (relative to
// the monster position).
static bool _allied_monster_at(monster* mon, coord_def delta)
{
    coord_def pos = mon->pos() + delta;
    if (!in_bounds(pos))
        return false;

    const monster* ally = monster_at(pos);
    if (ally == nullptr)
        return false;

    if (ally->is_stationary() || ally->reach_range() > REACH_NONE)
        return false;

    // Hostile monsters of normal intelligence only move aside for
    // monsters of the same genus.
    if (_unfriendly_or_impaired(*mon)
        && mons_genus(mon->type) != mons_genus(ally->type))
    {
        return false;
    }

    return mons_aligned(mon, ally);
}

static void _passively_summon_butterfly(const monster &summoner)
{
    const actor* foe = summoner.get_foe();
    if (!foe || !summoner.see_cell_no_trans(foe->pos()))
        return;

    auto mg = mgen_data(MONS_BUTTERFLY, SAME_ATTITUDE(&summoner),
                        summoner.pos(), summoner.foe);
    mg.set_summoned(&summoner, MON_SUMM_BUTTERFLIES, summ_dur(1));
    monster *butt = create_monster(mg);
    if (!butt)
        return;

    // Prefer to summon adj to the summoner and closer to the foe,
    // if one exists. (Otherwise, they tend to be too irrelevant.)
    int closest_dist = 10000;
    coord_def closest_pos = butt->pos();
    for (adjacent_iterator ai(summoner.pos()); ai; ++ai)
    {
        if (actor_at(*ai) || cell_is_solid(*ai))
            continue;
        const int dist = grid_distance(*ai, foe->pos());
        if (dist < closest_dist)
        {
            closest_dist = dist;
            closest_pos = *ai;
        }
    }
    butt->move_to_pos(closest_pos);
}

static bool _mons_can_cast_dig(const monster* mons, bool random)
{
    if (mons->foe == MHITNOT || !mons->has_spell(SPELL_DIG) || mons->confused()
        || mons->berserk_or_frenzied())
    {
        return false;
    }

    const bool antimagiced = mons->has_ench(ENCH_ANTIMAGIC)
                      && (random
                          && !x_chance_in_y(4 * BASELINE_DELAY,
                                            4 * BASELINE_DELAY
                                            + mons->get_ench(ENCH_ANTIMAGIC).duration)
                      || (!random
                          && mons->get_ench(ENCH_ANTIMAGIC).duration
                             >= 4 * BASELINE_DELAY));
    const auto flags = mons->spell_slot_flags(SPELL_DIG);
    return !(antimagiced && flags & MON_SPELL_ANTIMAGIC_MASK)
            && !(mons->is_silenced() && flags & MON_SPELL_SILENCE_MASK);
}

//Returns the delta from the monster's position to their target position
static coord_def _get_mons_destination(const monster* mons)
{
    coord_def delta;
    // Some calculations.
    if ((mons->can_burrow()
         || _mons_can_cast_dig(mons, false))
        && mons->foe == MHITYOU)
    {
        // Digging monsters always move in a straight line in your direction.
        delta = you.pos() - mons->pos();
    }
    else
    {
        const bool use_firing_pos = !mons->firing_pos.zero()
                                    && !mons_is_fleeing(*mons)
                                    && !mons_is_confused(*mons)
                                    && !mons->berserk_or_frenzied();
        delta = (use_firing_pos ? mons->firing_pos : mons->target)
                 - mons->pos();
    }
    return delta;
}

static coord_def _get_step_from_dest(const monster* mons, const coord_def dest)
{
    coord_def direction = dest.sgn();

    if (mons_is_retreating(*mons)
        && (!mons->friendly() || mons->target != you.pos()))
    {
        direction *= -1;
    }
    return direction;
}

typedef FixedArray< bool, 3, 3 > move_array;

static void _fill_good_move(const monster* mons, move_array* good_move)
{
    for (int count_x = 0; count_x < 3; count_x++)
        for (int count_y = 0; count_y < 3; count_y++)
        {
            const int targ_x = mons->pos().x + count_x - 1;
            const int targ_y = mons->pos().y + count_y - 1;

            // Bounds check: don't consider moving out of grid!
            if (!in_bounds(targ_x, targ_y))
            {
                (*good_move)[count_x][count_y] = false;
                continue;
            }

            (*good_move)[count_x][count_y] =
                mon_can_move_to_pos(mons, coord_def(count_x-1, count_y-1));
        }
}

static const string BATTY_TURNS_KEY = "BATTY_TURNS";

static void _handle_battiness(monster &mons)
{
    if (!mons_is_batty(mons))
        return;
    mons.behaviour = BEH_BATTY;
    set_random_target(&mons);
    mons.props[BATTY_TURNS_KEY] = 0;
}

static bool _fungal_move_check(monster &mon)
{
    // These monsters have restrictions on moving while you are looking.
    if ((mon.type == MONS_WANDERING_MUSHROOM || mon.type == MONS_DEATHCAP)
            && mon.foe_distance() > 1 // can attack if adjacent
        || (mon.type == MONS_LURKING_HORROR
            // 1 in los chance at max los
            && mon.foe_distance() > random2(you.current_vision + 1)))
    {
        if (!mon.wont_attack() && is_sanctuary(mon.pos()))
            return true;

        if (mon_enemies_around(&mon))
            return false;
    }
    return true;
}

static coord_def _find_best_step(monster* mons)
{
    if (!_fungal_move_check(*mons))
        return coord_def();

    if (sanctuary_exists())
    {
        // Monsters will try to flee out of a sanctuary.
        if (is_sanctuary(mons->pos())
            && mons_is_influenced_by_sanctuary(*mons)
            && !mons_is_fleeing_sanctuary(*mons))
        {
            mons_start_fleeing_from_sanctuary(*mons);
        }
        else if (mons_is_fleeing_sanctuary(*mons)
                 && !is_sanctuary(mons->pos()))
        {
            // Once outside there's a chance they'll regain their courage.
            // Nonliving and berserking monsters always stop immediately,
            // since they're only being forced out rather than actually
            // scared.
            if (mons->is_nonliving()
                || mons->berserk()
                || mons->has_ench(ENCH_FRENZIED)
                || x_chance_in_y(2, 5))
            {
                mons_stop_fleeing_from_sanctuary(*mons);
            }
        }
    }

    coord_def delta = _get_mons_destination(mons);
    coord_def dir = _get_step_from_dest(mons, delta);

    if (sanctuary_exists())
    {
        // Don't allow monsters to enter a sanctuary or attack you inside a
        // sanctuary, even if you're right next to them.
        if (is_sanctuary(mons->pos() + dir)
            && (!is_sanctuary(mons->pos())
                || mons->pos() + dir == you.pos()))
        {
            return coord_def();
        }
    }

    // Bounds check: don't let fleeing monsters try to run off the grid.
    const coord_def s = mons->pos() + dir;
    if (!in_bounds_x(s.x))
        dir.x = 0;
    if (!in_bounds_y(s.y))
        dir.y = 0;

    if (delta.rdist() > 3)
    {
        // Reproduced here is some semi-legacy code that makes monsters
        // move somewhat randomly along oblique paths. It is an
        // exceedingly good idea, given crawl's unique line of sight
        // properties.
        //
        // Added a check so that oblique movement paths aren't used when
        // close to the target square. -- bwr

        // Sometimes we'll just move parallel the x axis.
        if (abs(delta.x) > abs(delta.y) && coinflip())
            dir.y = 0;

        // Sometimes we'll just move parallel the y axis.
        if (abs(delta.y) > abs(delta.x) && coinflip())
            dir.x = 0;
    }

    // Now quit if we can't move.
    if (dir.origin())
        return dir;

    const coord_def newpos(mons->pos() + dir);

    // Filling this is relatively costly and not always needed, so be a bit
    // lazy about it.
    move_array good_move;
    bool good_move_filled = false;

    // If the monster is moving in your direction, whether to attack or
    // protect you, or towards a monster it intends to attack, check
    // whether we first need to take a step to the side to make sure the
    // reinforcement can follow through. Only do this with 50% chance,
    // though, so it's not completely predictable.

    // First, check whether the monster is smart enough to even consider
    // this.
    if ((newpos == you.pos()
           || monster_at(newpos) && mons->foe == env.mgrid(newpos))
        && mons_intel(*mons) > I_BRAINLESS
        && coinflip()
        && !mons_is_confused(*mons) && !mons->caught()
        && !mons->berserk_or_frenzied())
    {
        _fill_good_move(mons, &good_move);
        good_move_filled = true;
        // If the monster is moving parallel to the x or y axis, sometimes
        // move diagonally in that direction instead. Do this if either
        //
        // a) an ally is behind us and can't easily move around us;
        // or
        // b) we're intelligent and blocking a ranged attack.
        if (dir.y == 0)
        {
            if ((good_move[dir.x+1][0] || good_move[dir.x+1][2])
                && ((_allied_monster_at(mons, coord_def(-dir.x, -1))
                     || _allied_monster_at(mons, coord_def(-dir.x, 0))
                     || _allied_monster_at(mons, coord_def(-dir.x, 1)))
                       && !good_move[1][0] && !good_move[1][2]
                    || _allied_monster_at(mons, coord_def(-dir.x, -1))
                       && !good_move[1][0] && !good_move[1-dir.x][1]
                    || _allied_monster_at(mons, coord_def(-dir.x, 1))
                       && !good_move[1][2] && !good_move[1-dir.x][1]
                    || mons_intel(*mons) >= I_HUMAN
                       && _ranged_ally_in_dir(mons, coord_def(-dir.x, 0))))
            {
                if (good_move[dir.x+1][0])
                    dir.y = -1;
                if (good_move[dir.x+1][2] && (dir.y == 0 || coinflip()))
                    dir.y = 1;
            }
        }
        else if (dir.x == 0)
        {
            if ((good_move[0][dir.y+1] || good_move[2][dir.y+1])
                && ((_allied_monster_at(mons, coord_def(-1, -dir.y))
                     || _allied_monster_at(mons, coord_def(0, -dir.y))
                     || _allied_monster_at(mons, coord_def(1, -dir.y)))
                       && !good_move[0][1] && !good_move[2][1]
                    || _allied_monster_at(mons, coord_def(-1, -dir.y))
                       && !good_move[0][1] && !good_move[1][1-dir.y]
                    || _allied_monster_at(mons, coord_def(1, -dir.y))
                       && !good_move[2][1] && !good_move[1][1-dir.y]
                    || mons_intel(*mons) >= I_HUMAN
                       && _ranged_ally_in_dir(mons, coord_def(0, -dir.y))))
            {
                if (good_move[0][dir.y+1])
                    dir.x = -1;
                if (good_move[2][dir.y+1] && (dir.x == 0 || coinflip()))
                    dir.x = 1;
            }
        }
        else
        // We're moving diagonally now. Sometimes move horizontally or
        // vertically in that direction instead, under similar conditions.
        {
            if (good_move[dir.x+1][1] && !good_move[1][dir.y+1])
            {
                if ((_allied_monster_at(mons, coord_def(-dir.x, -1))
                     || _allied_monster_at(mons, coord_def(-dir.x, 0))
                     || _allied_monster_at(mons, coord_def(-dir.x, 1)))
                    || mons_intel(*mons) >= I_HUMAN
                       && _ranged_ally_in_dir(mons, coord_def(-dir.x, -dir.y)))
                {
                    dir.y = 0;
                }
            }
            else if (good_move[1][dir.y+1] && !good_move[dir.x+1][1])
            {
                if ((_allied_monster_at(mons, coord_def(-1, -dir.y))
                     || _allied_monster_at(mons, coord_def(0, -dir.y))
                     || _allied_monster_at(mons, coord_def(1, -dir.y)))
                    || mons_intel(*mons) >= I_HUMAN
                       && _ranged_ally_in_dir(mons, coord_def(-dir.x, -dir.y)))
                {
                    dir.x = 0;
                }
            }
            else if (good_move[dir.x+1][1] && good_move[1][dir.y+1])
            {
                if (_allied_monster_at(mons, coord_def(-dir.x, -dir.y))
                    && !good_move[1-dir.x][1] && !good_move[1][1-dir.y]
                    || mons_intel(*mons) >= I_HUMAN
                       && _ranged_ally_in_dir(mons, coord_def(-dir.x, -dir.y)))
                {
                    if (coinflip())
                        dir.x = 0;
                    else
                        dir.y = 0;
                }
            }
        }
    }

    // Now quit if we can't move.
    if (dir.origin())
        return dir;

    // everything below here is irrelevant if the player is not in bounds, for
    // example if they have stepped from time.
    if (!in_bounds(you.pos()))
        return dir;

    // Try to stay in sight of the player if we're moving towards
    // him/her, in order to avoid the monster coming into view,
    // shouting, and then taking a step in a path to the player which
    // temporarily takes it out of view, which can lead to the player
    // getting "comes into view" and shout messages with no monster in
    // view.

    // Doesn't matter for arena mode.
    if (crawl_state.game_is_arena())
        return dir;

    // Did we just come into view?
    // TODO: This doesn't seem to work right. Fix, or remove?

    if (mons->seen_context != SC_JUST_SEEN)
        return dir;
    if (testbits(mons->flags, MF_WAS_IN_VIEW))
        return dir;

    const coord_def old_pos  = mons->pos();
    const int       old_dist = grid_distance(you.pos(), old_pos);

    // We're already staying in the player's LOS.
    if (you.see_cell(old_pos + dir))
        return dir;

    // We're not moving towards the player.
    if (grid_distance(you.pos(), old_pos + dir) >= old_dist)
    {
        // Instead of moving out of view, we stay put.
        if (you.see_cell(old_pos))
            dir.reset();
        return dir;
    }

    // Try to find a move that brings us closer to the player while
    // keeping us in view.
    int matches = 0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
        {
            if (i == 0 && j == 0)
                continue;

            coord_def d(i - 1, j - 1);
            coord_def tmp = old_pos + d;
            if (!you.see_cell(tmp))
                continue;

            if (!good_move_filled)
            {
                _fill_good_move(mons, &good_move);
                good_move_filled = true;
            }
            if (!good_move[i][j])
                continue;

            if (grid_distance(you.pos(), tmp) < old_dist)
            {
                if (one_chance_in(++matches))
                    dir = d;
                break;
            }
        }

    // We haven't been able to find a visible cell to move to. If previous
    // position was visible, we stay put.
    if (you.see_cell(old_pos) && !you.see_cell(old_pos + dir))
        dir.reset();

    return dir;
}

/**
 * Check if the monster has a swooping or flanking attack and is in a
 * position to use it, and do so if they can.
 *
 * For either, if there's space, try to move to the opposite side of the
 * target and perform a melee attack, then set a cooldown for 4-8 turns.
 * Swoops happen from a distance, flanks happen in melee.
 *
 * @param mons The monster who might be swooping or flanking.
 * @return Whether they performed a swoop attack. False if the monster
 *         can't swoop, the foe isn't hostile, the positioning doesn't
 *         work, etc.
 */

static bool _handle_swoop_or_flank(monster& mons)
{
    bool is_swoop;

    // Check which of the two it is, then modulate the chance to try.
    // TODO: check for the flavour in other slots and/or make it work there?
    switch (mons_attack_spec(mons, 0, true).flavour)
    {
        case AF_SWOOP:
            if (!one_chance_in(4))
                return false;
            else
                is_swoop = true;
            break;

        case AF_FLANK:
            if (!one_chance_in(3))
                return false;
            else
                is_swoop = false;
            break;

        default:
            return false;
    }

    actor *defender = mons.get_foe();
    if (mons.confused() || !defender || !mons.can_see(*defender)
        || (mons_aligned(&mons, defender) && !mons.has_ench(ENCH_FRENZIED))
        || mons_is_fleeing(mons) || mons.pacified()
        || is_sanctuary(mons.pos()) || is_sanctuary(defender->pos())
        || mons.has_ench(ENCH_BOUND))
    {
        return false;
    }

    // Swoop can only be used at range. Flanking can only be used in melee.
    if (is_swoop && (mons.foe_distance() >= 5 || mons.foe_distance() == 1))
        return false;
    else if (!is_swoop && mons.foe_distance() != 1)
        return false;

    if (mons.props.exists(SWOOP_COOLDOWN_KEY)
        && (you.elapsed_time < mons.props[SWOOP_COOLDOWN_KEY].get_int()))
    {
        return false;
    }

    coord_def target = defender->pos();

    bolt tracer;
    tracer.source = mons.pos();
    tracer.target = target;
    tracer.is_tracer = true;
    tracer.pierce = true;
    tracer.range = LOS_RADIUS;
    tracer.fire();

    for (unsigned int j = 0; j < tracer.path_taken.size() - 1; ++j)
    {
        if (tracer.path_taken[j] != target)
            continue;

        if (!monster_habitable_grid(&mons, tracer.path_taken[j+1])
            || actor_at(tracer.path_taken[j+1]))
        {
            continue;
        }

        if (you.can_see(mons))
        {
            if (is_swoop)
            {
                mprf("%s swoops through the air toward %s!",
                mons.name(DESC_THE).c_str(),
                defender->name(DESC_THE).c_str());
            }
            else
            {
                mprf("%s slips past %s!",
                mons.name(DESC_THE).c_str(),
                defender->name(DESC_THE).c_str());
            }
        }
        const coord_def old_pos = mons.pos();
        mons.move_to_pos(tracer.path_taken[j+1]);
        // Apply traps and other location effects to the monster.
        mons.apply_location_effects(old_pos);
        fight_melee(&mons, defender);
        mons.props[SWOOP_COOLDOWN_KEY].get_int() = you.elapsed_time
                                                  + 40 + random2(51);
        return true;
    }

    return false;
}

/**
 * Check whether this monster can make a reaching attack, and do so if
 * they can.
 *
 * @param mons The monster who might be reaching.
 * @return Whether they attempted a reaching attack. False if the monster
 *         doesn't have a reaching weapon, the foe isn't hostile, the foe
 *         is too near or too far, etc.
 */
static bool _handle_reaching(monster& mons)
{
    bool       ret = false;
    const reach_type range = mons.reach_range();
    actor *foe = mons.get_foe();

    // Don't attempt to reach-attack a player we cannot see through Nightfall
    if (you.current_vision < range)
        return false;

    if (mons.caught()
        || mons_is_confused(mons)
        || !foe
        || range <= REACH_NONE
        || is_sanctuary(mons.pos())
        || is_sanctuary(foe->pos())
        || (mons_aligned(&mons, foe) && !mons.has_ench(ENCH_FRENZIED))
        || mons_is_fleeing(mons)
        || mons.pacified())
    {
        return false;
    }

    const coord_def foepos(foe->pos());
    if (can_reach_attack_between(mons.pos(), foepos, range)
        // The monster has to be attacking the correct position.
        && mons.target == foepos)
    {
        ret = true;

        ASSERT(foe->is_player() || foe->is_monster());

        fight_melee(&mons, foe);
    }

    return ret;
}

static void _handle_boulder_movement(monster& boulder)
{
    // If we don't have a movement direction (probably from a vault-placed
    // decorative boulder), don't crash by assuming we do.
    // Also, since we bypass a lot of normal action code, yet can get netted,
    // prevent the boulder from moving away from the net's location and creating
    // multiple item bugs due to how netting works.
    if (!boulder.props.exists(BOULDER_DIRECTION_KEY) || boulder.caught())
    {
        // Have to use energy anyway, or we cause an infinite loop.
        _swim_or_move_energy(boulder);
        return;
    }

    place_cloud(CLOUD_DUST, boulder.pos(), 2 + random2(3), &boulder);

    // First, find out where we intend to move next
    coord_def dir = boulder.props[BOULDER_DIRECTION_KEY].get_coord();
    coord_def targ = boulder.pos() + dir;

    // If our summoner is the player, and they cannot see us, silently crumble
    if (boulder.summoner == MID_PLAYER
        && (!you.can_see(boulder) || !you.see_cell(targ)))
    {
        simple_monster_message(boulder, " crumbles as it rolls away.");
        monster_die(boulder, KILL_RESET, true);
        return;
    }

    // If we're moving into something solid, shatter.
    if (feat_is_solid(env.grid(targ)))
    {
        if (you.can_see(boulder))
        {
            mprf("%s slams into %s and falls apart!",
                 boulder.name(DESC_THE).c_str(),
                 article_a(feat_type_name(env.grid(targ))).c_str());
        }
        monster_die(boulder, KILL_NONE, true);
        return;
    }

    // If we're about to run into things, this is a bit more complicated
    if (actor_at(targ))
    {
        actor* obstruction = actor_at(targ);
        // First, do impact damage to the actor and see if we kill it

        do_boulder_impact(boulder, *obstruction);
        if (!boulder.alive())
            return;

        // If the obstacle is still alive (and so is the boulder), do knockback calculations
        if (obstruction->alive())
        {
            // We want to trace a line of all connected monsters in a row, in the
            // direction we're moving, and then push them away in reverse order.
            vector<actor*> push_targs;
            coord_def pos = targ;
            while (actor_at(pos))
            {
                push_targs.push_back(actor_at(pos));
                pos += dir;
            }

            for (int i = push_targs.size() - 1; i >= 0; --i)
                if (push_targs[i]->alive()) // died from earlier knockback?
                    push_targs[i]->stumble_away_from(boulder.pos());
        }

        // If there is still somehow something in our way (maybe we were unable
        // to push everything out of it), destroy the boulder (and deal an
        // additional hit to the actor in question if they're against something
        // solid)
        if (actor_at(targ))
        {
            actor* blocker = nullptr;
            string blocker_name;
            if (blocker = monster_at(obstruction->pos() + dir))
                blocker_name = blocker->name(DESC_THE);
            else
                blocker_name = feature_description_at(targ + dir);
            if (blocker_name.empty())
                blocker_name = "something";

            if (blocker || cell_is_solid(targ + dir))
            {
                if (you.can_see(boulder) || you.can_see(*obstruction))
                {
                    mprf("%s crushes %s against %s and falls apart!",
                            boulder.name(DESC_THE).c_str(),
                            obstruction->name(DESC_THE).c_str(),
                            blocker_name.c_str());
                }
                do_boulder_impact(boulder, *obstruction, true);
            }
            // This generally means we're trying to push an enemy against deep
            // water / lava without it being able to swim. This should still
            // destroy the boulder for gameplay reasons, but it's awkward to say
            // something was 'crushed' against water.
            else
            {
                if (you.can_see(boulder))
                {
                    mprf("%s crumbles against %s.",
                            boulder.name(DESC_THE).c_str(),
                            obstruction->name(DESC_THE).c_str());
                }
            }

            monster_die(boulder, nullptr, true);
            return;
        }
    }

    // If we're still here, actually move. (But consume energy, even if we somehow don't)
    if (!_do_move_monster(boulder, dir))
        _swim_or_move_energy(boulder);
    else
    {
        // Might have been killed by barbs while moving!
        if (!boulder.alive())
            return;

        // Abrade boulder by the number of solid tiles we passed by just now
        int solid_count = 0;
        string solid_name;
        for (adjacent_iterator ai(boulder.pos()); ai; ++ai)
            if (cell_is_solid(*ai))
            {
                ++solid_count;
                if (solid_name.empty())
                    solid_name = feat_type_name(env.grid(*ai));
            }

        const int self_dmg = roll_dice(solid_count, BOULDER_ABRASION_DAMAGE);
        if (self_dmg > boulder.hit_points)
        {
            mprf("%s is abraded by the %s and falls apart.",
                    boulder.name(DESC_THE).c_str(), solid_name.c_str());
            monster_die(boulder, nullptr, true);
        }
        else
            boulder.hurt(nullptr, self_dmg, BEAM_NONE, KILLED_BY_COLLISION);
    }
}

static void _handle_hellfire_mortar(monster& mortar)
{
    // First, fire a bolt of magma at some random target we have line of fire to
    vector<coord_def> targs;
    for (actor_near_iterator ai(&mortar, LOS_NO_TRANS); ai; ++ai)
    {
        if (!mons_aligned(&mortar, *ai) && monster_los_is_valid(&mortar, *ai))
        {
            bolt tracer = mons_spell_beam(&mortar, SPELL_MAGMA_BARRAGE, 100);
            tracer.source = mortar.pos();
            tracer.target = ai->pos();

            fire_tracer(&mortar, tracer);
            if (mons_should_fire(tracer))
                targs.push_back(ai->pos());
        }
    }

    shuffle_array(targs);

    int shots_fired = 0;
    for (size_t i = 0; i < targs.size() && shots_fired < 2; ++i)
    {
        actor* foe = actor_at(targs[i]);

        // Skip dead targets, and move onto the next possible candidate, if one exists.
        // (They might have been killed by a previous beam)
        if (!foe)
            continue;

        // Set our foe to whatever we're shooting at for accurate cast messages
        mortar.foe = actor_at(targs[i])->mindex();

        bolt beam;
        setup_mons_cast(&mortar, beam, SPELL_MAGMA_BARRAGE);
        beam.target = targs[i];
        mons_cast(&mortar, beam, SPELL_MAGMA_BARRAGE,
                    mortar.spell_slot_flags(SPELL_MAGMA_BARRAGE));

        ++shots_fired;
    }

    // Then try to advance one tile further along our movement path.
    // If we cannot (either because our path has ended, or we're blocked), die.
    CrawlVector& path = mortar.props[HELLFIRE_PATH_KEY].get_vector();
    for (unsigned int i = 0; i < path.size(); ++i)
    {
        if (path[i].get_coord() == mortar.pos())
        {
            // We've reached the end of our path.
            if (i + 1 == path.size())
            {
                simple_monster_message(mortar, " sinks back into the magma.");
                monster_die(mortar, KILL_NON_ACTOR, NON_MONSTER, true);
                return;
            }

            coord_def new_pos = path[i+1].get_coord();

            // We're blocked by someone! Try to push them backwards first, if we can.
            if (actor_at(new_pos))
                actor_at(new_pos)->stumble_away_from(mortar.pos());

            // If there's *still* someone in the way (or we've run out of lava),
            // die.
            if (env.grid(new_pos) != DNGN_LAVA || actor_at(new_pos))
            {
                if (you.can_see(mortar))
                {
                    string barrier, collides = " collides with ", _and = " and";
                    if (actor_at(new_pos))
                        barrier = actor_at(new_pos)->name(DESC_THE);
                    else if (cell_is_solid(new_pos))
                        barrier = article_a(feat_type_name(env.grid(new_pos)));
                    else
                        collides = _and = "";

                    mpr(mortar.name(DESC_THE) + collides + barrier + _and +
                        " sinks back into the magma.");
                }

                monster_die(mortar, KILL_NON_ACTOR, NON_MONSTER, true);
                return;
            }

            if (_do_move_monster(mortar, new_pos - mortar.pos()))
                return;
        }
    }

    // Generally we shouldn't reach this point, but if we're unable to move
    // *and* don't look like we should die either, consume energy so we don't
    // cause an infinite loop.
    mortar.lose_energy(EUT_ATTACK);
}

static void _check_blazeheart_golem_link(monster& mons)
{
    // Check distance from player. If we're non-adjacent, decrease timer
    // (we get a turn or two's grace period to make managing this less fiddly)
    if (grid_distance(you.pos(), mons.pos()) > 1)
    {
        mons.blazeheart_heat -= 1;
        if (mons.blazeheart_heat <= 0 && !mons.has_ench(ENCH_PARALYSIS))
        {
            simple_monster_message(mons, " core grows cold and it stops moving.", true);
            mons.add_ench(mon_enchant(ENCH_PARALYSIS, 1, &mons, INFINITE_DURATION));
        }
    }
    else
    {
        // If we are dormant, wake up.
        if (mons.has_ench(ENCH_PARALYSIS))
        {
            mons.del_ench(ENCH_PARALYSIS, true);
            simple_monster_message(mons, " core flares to life once more.", true);

            // Since we check this at the END of the golem's move, even if it
            // started its turn with the player next to them (due to player
            // movement), grant some instant energy to make it look like it
            // activated first.
            //mons.speed_increment += mons.action_energy(EUT_MOVE);
        }

        // Give the golem another turn before it goes cold.
        mons.blazeheart_heat = 2;
    }
}

static bool _handle_rending_blade_trigger(monster* blade)
{
    if (blade->number <= 0)
        return false;

    const int pow = blade->props[RENDING_BLADE_POWER_KEY].get_int();

    coord_def best_targ;
    int best_weight = 0;
    int best_range = 0;
    int num_best_found = 0;

    // Look at all hostile enemies both you and the blade can see, then
    // trace a path towards each of them, going as far as possible without
    // exceeding the blade's maximum range (4), hitting an ally, or leaving
    // the player's LoS. If no path is productive and safe, do nothing and
    // hope we'll be in a better position next turn.
    for (monster_near_iterator mi(blade, LOS_NO_TRANS); mi; ++mi)
    {
        if (mons_aligned(*mi, blade) || !you.can_see(**mi)
            || grid_distance(mi->pos(), blade->pos()) > 4)
        {
            continue;
        }

        ray_def ray;
        if (!find_ray(blade->pos(), mi->pos(), ray, opc_solid_see))
            continue;

        // Examine spaces one at a time, stopping if we hit a friendly
        // monster, wall, or leave the player's LoS, and otherwise keep
        // track of the furthest safe empty space we can occupy (to determine
        // the maximal range of this shot).
        int steps_taken = 0;
        int furthest_safe = 0;
        int enemy_power = 0;
        int enemy_power_reachable = 0;

        while (ray.advance() && steps_taken < 4)
        {
            ++steps_taken;
            const coord_def p = ray.pos();

            // Don't leave the player's LoS.
            if (!you.see_cell_no_trans(p) || p == you.pos())
                break;

            if (monster* targ = monster_at(p))
            {
                // Don't hurt allies.
                if (mons_aligned(blade, targ))
                    break;

                if (!targ->is_firewood())
                    enemy_power += targ->get_experience_level();
            }
            // We need somewhere safe to end our path.
            else if (monster_habitable_grid(MONS_RENDING_BLADE, p))
            {
                furthest_safe = steps_taken;
                enemy_power_reachable = enemy_power;
            }
        }

        if (furthest_safe > 0)
        {
            if (enemy_power_reachable > best_weight)
            {
                best_weight = enemy_power_reachable;
                best_range = furthest_safe;
                best_targ = mi->pos();
                num_best_found = 0;
            }
            // Choose randomly among ties for best shot
            else if (enemy_power_reachable > 0
                     && enemy_power_reachable == best_weight
                     && one_chance_in(++num_best_found))
            {
                best_weight = enemy_power_reachable;
                best_range = furthest_safe;
                best_targ = mi->pos();
            }
        }
    }

    // If we found no valid path, bail.
    if (best_targ.origin())
        return false;

    // Point blade at foe, so it won't wander off
    blade->foe = monster_at(best_targ)->mindex();

    bolt slash;
    zappy(ZAP_RENDING_SLASH, pow, true, slash);
    slash.range = best_range;
    slash.source = blade->pos();
    slash.source_id = blade->mid;
    slash.thrower = KILL_MON_MISSILE;
    slash.origin_spell = SPELL_RENDING_BLADE;
    slash.target = best_targ;
    slash.hit_verb = "slices through";

    simple_monster_message(*blade, " flashes!");

    slash.fire();
    blade->blink_to(slash.path_taken[slash.path_taken.size() - 1], true, true);
    blade->number -= 1;

    return true;
}

static void _handle_lightning_spire(monster& spire)
{
    // 50% chance of casting each turn
    if (coinflip() || spire.is_silenced())
        return;

    // Gather all eligable targets in sight
    vector<actor*> targs;
    for (actor_near_iterator ai(&spire, LOS_NO_TRANS); ai; ++ai)
    {
        if (mons_aligned(*ai, &spire) || !monster_los_is_valid(&spire, *ai))
            continue;

        if (ai->is_peripheral())
            continue;

        targs.push_back(*ai);
    }

    // Sort farthest-to-nearest (with ties randomized)
    shuffle_array(targs);
    sort(targs.begin( ), targs.end( ), [spire](actor* a, actor* b)
    {
        return grid_distance(a->pos(), spire.pos())
               > grid_distance(b->pos(), spire.pos());
    });

    // Now attempt to cast electrical bolt on each target, in order of
    // decreasing priority.
    for (size_t i = 0; i < targs.size(); ++i)
    {
        spire.foe = targs[i]->mindex();
        spire.target = targs[i]->pos();
        if (try_mons_cast(spire, SPELL_ELECTRICAL_BOLT))
            return;
    }
}

static void _mons_fire_wand(monster& mons, spell_type mzap, bolt &beem)
{
    if (!simple_monster_message(mons, " zaps a wand."))
    {
        if (!silenced(you.pos()))
            mprf(MSGCH_SOUND, "You hear a zap.");
    }
    mons_cast(&mons, beem, mzap, MON_SPELL_EVOKE, false);
    mons.lose_energy(EUT_ITEM);
}

static bool _handle_wand(monster& mons)
{
    item_def *wand = mons.mslot_item(MSLOT_WAND);
    // Yes, there is a logic to this ordering {dlb}:
    // (no there's not -- pf)
    if (!wand
        || wand->base_type != OBJ_WANDS

        || !mons.get_foe()
        || !cell_see_cell(mons.pos(), mons.get_foe()->pos(), LOS_SOLID_SEE)

        || mons.asleep()
        || mons_is_fleeing(mons)
        || mons.pacified()
        || mons.confused()
        || mons_itemuse(mons) < MONUSE_STARTING_EQUIPMENT
        || !mons.likes_wand(*wand)

        || x_chance_in_y(3, 4))
    {
        return false;
    }

    if (wand->charges <= 0)
        return false;

    if (item_type_removed(wand->base_type, wand->sub_type))
        return false;

    // Digging is handled elsewhere so that sensible (wall) targets are
    // chosen.
    if (wand->sub_type == WAND_DIGGING)
        return false;

    bolt beem;

    const spell_type mzap =
        spell_in_wand(static_cast<wand_type>(wand->sub_type));

    if (!ai_action::is_viable(monster_spell_goodness(&mons, mzap)))
        return false;

    if (!setup_mons_cast(&mons, beem, mzap, true))
        return false;

    beem.source     = mons.pos();
    beem.aux_source =
        wand->name(DESC_QUALNAME, false, true, false, false);
    fire_tracer(&mons, beem);
    if (!mons_should_fire(beem))
        return false;

    _mons_fire_wand(mons, mzap, beem);
    return true;
}

bool handle_throw(monster* mons, bolt & beem, bool teleport, bool check_only)
{
    // Yes, there is a logic to this ordering {dlb}:
    if (mons->incapacitated()
        || mons->caught()
        || mons_is_confused(*mons)
        || (mons->has_ench(ENCH_ARMED)
            && mons->weapon()
            && !is_range_weapon(*mons->weapon())))
    {
        return false;
    }

    if (mons_itemuse(*mons) < MONUSE_STARTING_EQUIPMENT
        && mons->type != MONS_BOUND_SOUL
        && !mons_class_is_animated_object(mons->type))
    {
        return false;
    }

    const bool prefer_ranged_attack
        = mons_class_flag(mons->type, M_PREFER_RANGED)
          || (mons->has_ench(ENCH_ARMED)
              && mons->weapon()
              && is_range_weapon(*mons->weapon()));

    // Don't allow offscreen throwing for now.
    if (mons->foe == MHITYOU && !you.see_cell(mons->pos()))
        return false;

    // Most monsters won't shoot in melee range, largely for balance reasons.
    // Specialist archers are an exception to this rule, though most archers
    // lack the M_PREFER_RANGED flag.
    // Monsters who only can attack with ranged still should. Keep in mind
    // that M_PREFER_RANGED only applies if the monster has ammo.
    if (adjacent(beem.target, mons->pos()) && !prefer_ranged_attack)
        return false;

    // Don't let fleeing (or pacified creatures) stop to shoot at things
    if (mons_is_fleeing(*mons) || mons->pacified())
        return false;

    const item_def *launcher = mons->launcher();
    item_def *throwable = mons->missiles();
    const bool can_throw = (throwable && is_throwable(mons, *throwable));
    item_def fake_proj;
    item_def *missile = &fake_proj;
    bool using_launcher = false;
    // If a monster somehow has both a launcher and a throwable, use the
    // launcher 2/3 of the time.
    if (launcher && (!can_throw || !one_chance_in(3)))
    {
        populate_fake_projectile(*launcher, fake_proj);
        using_launcher = true;
    }
    else if (can_throw)
        missile = throwable;
    else
        return false;

    if (player_or_mon_in_sanct(*mons))
        return false;

    const actor *act = actor_at(beem.target);
    ASSERT(missile->base_type == OBJ_MISSILES);
    if (act && missile->sub_type == MI_THROWING_NET)
    {
        // Throwing a net at a target that is already caught would be
        // completely useless, so bail out.
        if (act->caught())
            return false;
        // Netting targets that are already permanently stuck in place
        // is similarly useless.
        if (mons_class_is_stationary(act->type))
            return false;
    }

    if (prefer_ranged_attack)
    {
        // Master archers are always quite likely to shoot you, if they can.
        //
        // (They always fire when in melee, to keep them from rarely swapping
        // their launchers away when they inevitably bump attack their target
        // anyway)
        if (one_chance_in(10) && !adjacent(beem.target, mons->pos()))
            return false;
    }
    else if (launcher)
    {
        // Fellas with ranged weapons are likely to use them, though slightly
        // less likely than master archers. XXX: this is a bit silly and we
        // could probably collapse this chance and master archers' together.
        if (one_chance_in(5))
            return false;
    }
    else if (!one_chance_in(3))
    {
        // Monsters with throwing weapons only use them one turn in three
        // if they're not master archers.
        return false;
    }

    // Ok, we'll try it.
    setup_monster_throw_beam(mons, beem);

    // Set fake damage for the tracer.
    beem.damage = dice_def(10, 10);

    // Set item for tracer, even though it probably won't be used
    beem.item = missile;

    ru_interference interference = DO_NOTHING;
    // See if Ru worshippers block or redirect the attack.
    if (does_ru_wanna_redirect(*mons))
    {
        interference = get_ru_attack_interference_level();
        if (interference == DO_BLOCK_ATTACK)
        {
            simple_monster_message(*mons,
                                " is stunned by your conviction and fails to attack.", false,
                                MSGCH_GOD);
            return false;
        }
        else if (interference == DO_REDIRECT_ATTACK)
        {
            mprf(MSGCH_GOD, "You redirect %s's attack!",
                    mons->name(DESC_THE).c_str());
            int pfound = 0;
            for (radius_iterator ri(you.pos(),
                LOS_DEFAULT); ri; ++ri)
            {
                monster* new_target = monster_at(*ri);

                if (!new_target
                    || mons_is_projectile(new_target->type)
                    || new_target->is_firewood()
                    || new_target->friendly())
                {
                    continue;
                }

                ASSERT(new_target);

                if (one_chance_in(++pfound))
                {
                    mons->target = new_target->pos();
                    mons->foe = new_target->mindex();
                    beem.target = mons->target;
                }
            }
        }
    }

    // Fire tracer.
    if (!teleport)
        fire_tracer(mons, beem);

    // Clear fake damage (will be set correctly in mons_throw).
    beem.damage = dice_def();

    // Good idea?
    if (teleport || mons_should_fire(beem) || interference != DO_NOTHING)
    {
        if (check_only)
            return true;

        // Monsters shouldn't shoot if fleeing, so let them "turn to attack".
        make_mons_stop_fleeing(mons);

        if (launcher && using_launcher && launcher != mons->weapon())
            mons->swap_weapons();

        beem.name.clear();
        return mons_throw(mons, beem, teleport);
    }

    return false;
}

// Give the monster its action energy (aka speed_increment).
static void _monster_add_energy(monster& mons)
{
    if (mons.speed > 0)
    {
        // Randomise to make counting off monster moves harder:
        const int energy_gained =
            max(1, div_rand_round(mons.speed * you.time_taken, 10));
        mons.speed_increment += energy_gained;
    }
}

#ifdef DEBUG
#    define DEBUG_ENERGY_USE(problem) \
    if (mons->speed_increment == old_energy && mons->alive()) \
             mprf(MSGCH_DIAGNOSTICS, \
                  problem " for monster '%s' consumed no energy", \
                  mons->name(DESC_PLAIN).c_str());
#    define DEBUG_ENERGY_USE_REF(problem) \
    if (mons.speed_increment == old_energy && mons.alive()) \
             mprf(MSGCH_DIAGNOSTICS, \
                  problem " for monster '%s' consumed no energy", \
                  mons.name(DESC_PLAIN).c_str());
#else
#    define DEBUG_ENERGY_USE(problem) ((void) 0)
#    define DEBUG_ENERGY_USE_REF(problem) ((void) 0)
#endif

static coord_def _confused_move_dir(monster *mons)
{
    coord_def conf_mov;
    int pfound = 0;
    for (adjacent_iterator ai(mons->pos(), false); ai; ++ai)
        if (mons->can_pass_through(*ai) && one_chance_in(++pfound))
            conf_mov = *ai - mons->pos();
    return conf_mov;
}

static int _tentacle_move_speed(monster_type type)
{
    if (type == MONS_KRAKEN)
        return 10;
    else if (type == MONS_TENTACLED_STARSPAWN)
        return 18;
    else
        return 0;
}

static void _pre_monster_move(monster& mons)
{
    mons.hit_points = min(mons.max_hit_points, mons.hit_points);

    if (mons.type == MONS_SPATIAL_MAELSTROM
        && !player_in_branch(BRANCH_ABYSS)
        && !player_in_branch(BRANCH_ZIGGURAT))
    {
        for (int i = 0; i < you.time_taken; ++i)
        {
            if (one_chance_in(100))
            {
                mons.banish(&mons);
                return;
            }
        }
    }

    if (mons.has_ench(ENCH_HEXED))
    {
        const actor* const agent =
            actor_by_mid(mons.get_ench(ENCH_HEXED).source);
        if (!agent || !agent->alive())
            mons.del_ench(ENCH_HEXED);
    }

    // Dissipate player ball lightnings and foxfires
    // that have left the player's sight
    // (monsters are allowed to 'cheat', as with orb of destruction)
    if ((mons.type == MONS_BALL_LIGHTNING || mons.type == MONS_FOXFIRE)
        && mons.summoner == MID_PLAYER
        && !cell_see_cell(you.pos(), mons.pos(), LOS_NO_TRANS))
    {
        if (mons.type == MONS_FOXFIRE)
            check_place_cloud(CLOUD_FLAME, mons.pos(), 2, &mons);
        monster_die(mons, KILL_RESET, NON_MONSTER);
        return;
    }

    if (mons_stores_tracking_data(mons))
    {
        actor* foe = mons.get_foe();
        if (foe)
        {
            if (!mons.props.exists(FAUX_PAS_KEY))
                mons.props[FAUX_PAS_KEY].get_coord() = foe->pos();
            else
            {
                if (mons.props[FAUX_PAS_KEY].get_coord().distance_from(mons.pos())
                    > foe->pos().distance_from(mons.pos()))
                {
                    mons.props[FOE_APPROACHING_KEY].get_bool() = true;
                }
                else
                    mons.props[FOE_APPROACHING_KEY].get_bool() = false;

                mons.props[FAUX_PAS_KEY].get_coord() = foe->pos();
            }
        }
        else
            mons.props.erase(FAUX_PAS_KEY);
    }

    fedhas_neutralise(&mons);
    slime_convert(&mons);

    // Check for golem summoner proximity whether we have energy to act or not.
    // (Avoids awkward situations of moving next to a dormant golem with very
    // fast actions not appearing to wake it up until the following turn)
    if (mons.type == MONS_BLAZEHEART_GOLEM)
        _check_blazeheart_golem_link(mons);

    // Monster just summoned (or just took stairs), skip this action.
    if (testbits(mons.flags, MF_JUST_SUMMONED))
    {
        mons.flags &= ~MF_JUST_SUMMONED;
        return;
    }

    mon_acting mact(&mons);

    _monster_add_energy(mons);

    // Handle clouds on nonmoving monsters.
    if (mons.speed == 0)
        actor_apply_cloud(&mons);

    if (mons.type == MONS_NO_MONSTER)
        return;

    // Apply monster enchantments once for every normal-speed
    // player turn.
    mons.ench_countdown -= you.time_taken;
    while (mons.ench_countdown < 0)
    {
        mons.ench_countdown += 10;
        mons.apply_enchantments();

        // If the monster *merely* died just break from the loop
        // rather than quit altogether, since we have to deal with
        // ballistomycete spores and ball lightning exploding at the end of the
        // function, but do return if the monster's data has been
        // reset, since then the monster type is invalid.
        if (mons.type == MONS_NO_MONSTER)
            return;
        else if (mons.hit_points < 1)
            break;
    }

    // Memory is decremented here for a reason -- we only want it
    // decrementing once per monster "move".
    if (mons.foe_memory > 0 && !you.penance[GOD_ASHENZARI])
        mons.foe_memory -= you.time_taken;

    // Otherwise there are potential problems with summonings.
    if (mons.type == MONS_GLOWING_SHAPESHIFTER)
        mons.add_ench(ENCH_GLOWING_SHAPESHIFTER);

    if (mons.type == MONS_SHAPESHIFTER)
        mons.add_ench(ENCH_SHAPESHIFTER);

    mons.check_speed();
}

// Handle weird stuff like spells/special abilities, item use,
// reaching, swooping, etc.
// Returns true iff the monster used up their turn.
static bool _mons_take_special_action(monster &mons, int old_energy)
{
    if ((mons.asleep() || mons_is_wandering(mons))
        // Slime creatures can split while wandering or resting.
        && mons.type != MONS_SLIME_CREATURE)
    {
        return false;
    }

    // Berserking monsters are limited to running up and
    // hitting their foes.
    if (mons.berserk_or_frenzied())
    {
        if (_handle_reaching(mons))
        {
            DEBUG_ENERGY_USE_REF("_handle_reaching()");
            return true;
        }

        return false;
    }

    // Prevents unfriendlies from nuking you from offscreen.
    // How nice!
    const bool friendly_or_near =
            mons.friendly() && mons.foe == MHITYOU
            || mons.near_foe();

    // Activate spells or abilities?
    if (friendly_or_near
        || mons.type == MONS_TEST_SPAWNER
        // Slime creatures can split when offscreen.
        || mons.type == MONS_SLIME_CREATURE)
    {
        // [ds] Special abilities shouldn't overwhelm
        // spellcasting in monsters that have both. This aims
        // to give them both roughly the same weight.
        if (coinflip() ? mon_special_ability(&mons) || handle_mon_spell(&mons)
                       : handle_mon_spell(&mons) || mon_special_ability(&mons))
        {
            _handle_battiness(mons);
            DEBUG_ENERGY_USE_REF("spell or special");
            return true;
        }
    }

    if (_handle_wand(mons))
    {
        DEBUG_ENERGY_USE_REF("_handle_wand()");
        return true;
    }

    if (_handle_swoop_or_flank(mons))
    {
        DEBUG_ENERGY_USE_REF("_handle_swoop_or_flank()");
        return true;
    }

    if (friendly_or_near)
    {
        bolt beem = setup_targeting_beam(mons);
        if (handle_throw(&mons, beem, false, false))
        {
            DEBUG_ENERGY_USE_REF("_handle_throw()");
            return true;
        }
    }

    if (_handle_reaching(mons))
    {
        DEBUG_ENERGY_USE_REF("_handle_reaching()");
        return true;
    }

#ifndef DEBUG
    UNUSED(old_energy);
#endif

    return false;
}

static bool _beetle_must_return(const monster* mons)
{
    if (mons->type != MONS_PHALANX_BEETLE)
        return false;

    actor* creator = actor_by_mid(mons->summoner);
    return creator && creator->alive()
           && !adjacent(mons->pos(), creator->pos());
}

void handle_monster_move(monster* mons)
{
    ASSERT(mons); // XXX: change to monster &mons
    const monsterentry* entry = get_monster_data(mons->type);
    if (!entry)
        return;

    coord_def mmov;

    const bool disabled = crawl_state.disables[DIS_MON_ACT]
                          && _unfriendly_or_impaired(*mons);

    const int old_energy = mons->speed_increment;
    int non_move_energy = min(entry->energy_usage.move,
                              entry->energy_usage.swim);

#ifdef DEBUG_MONS_SCAN
    bool monster_was_floating = env.mgrid(mons->pos()) != mons->mindex();
#endif
    coord_def old_pos = mons->pos();

    if (!mons->has_action_energy())
        return;

    if (!disabled)
        move_solo_tentacle(mons);

    if (!mons->alive())
        return;

    if (!disabled && mons_is_tentacle_head(mons_base_type(*mons)))
        move_child_tentacles(mons);

    old_pos = mons->pos();

#ifdef DEBUG_MONS_SCAN
    if (!monster_was_floating
        && env.mgrid(mons->pos()) != mons->mindex())
    {
        mprf(MSGCH_ERROR, "Monster %s became detached from env.mgrid "
                          "in handle_monster_move() loop",
             mons->name(DESC_PLAIN, true).c_str());
        mprf(MSGCH_WARN, "[[[[[[[[[[[[[[[[[[");
        debug_mons_scan();
        mprf(MSGCH_WARN, "]]]]]]]]]]]]]]]]]]");
        monster_was_floating = true;
    }
    else if (monster_was_floating
             && env.mgrid(mons->pos()) == mons->mindex())
    {
        mprf(MSGCH_DIAGNOSTICS, "Monster %s re-attached itself to env.mgrid "
                                "in handle_monster_move() loop",
             mons->name(DESC_PLAIN, true).c_str());
        monster_was_floating = false;
    }
#endif

    if (mons_is_projectile(*mons))
    {
        if (iood_act(*mons))
            return;
        mons->lose_energy(EUT_MOVE);
        return;
    }

    if (mons->type == MONS_FULMINANT_PRISM
        || mons->type == MONS_SHADOW_PRISM)
    {
        ++mons->prism_charge;
        if (mons->prism_charge == 2)
            mons->suicide();
        else
        {
            if (mons->type == MONS_FULMINANT_PRISM
                && player_can_hear(mons->pos()))
            {
                if (you.can_see(*mons))
                {
                    simple_monster_message(*mons, " crackles loudly.", false,
                                           MSGCH_WARN);
                }
                else
                    mprf(MSGCH_SOUND, "You hear a loud crackle.");
            }
            // Done this way to keep the detonation timer predictable
            mons->speed_increment -= BASELINE_DELAY;
        }
        return;
    }

    if (mons->type == MONS_FOXFIRE)
    {
        if (mons->steps_remaining == 0)
        {
            check_place_cloud(CLOUD_FLAME, mons->pos(), 2, mons);
            monster_die(*mons, KILL_TIMEOUT, NON_MONSTER);
            return;
        }
    }

    if (mons->type == MONS_BOULDER)
    {
        _handle_boulder_movement(*mons);
        return;
    }

    if (mons->type == MONS_HELLFIRE_MORTAR)
    {
        _handle_hellfire_mortar(*mons);
        return;
    }

    if (mons->type == MONS_DIAMOND_SAWBLADE)
    {
        // XXX: If its foe gets set to something that is no longer in sight, it
        //      will refuse to shred entirely.
        mons->foe = MHITNOT;
        if (!is_sanctuary(mons->pos()))
            try_mons_cast(*mons, SPELL_SHRED);
        mons->lose_energy(EUT_SPELL);
        return;
    }

    if (mons->type == MONS_LIGHTNING_SPIRE)
    {
        _handle_lightning_spire(*mons);
        mons->lose_energy(EUT_SPELL);
        return;
    }

    // Melt barricades whose creator has moved too far away.
    if (mons->type == MONS_SPLINTERFROST_BARRICADE)
    {
        actor* agent = actor_by_mid(mons->summoner);
        if (!agent || grid_distance(agent->pos(), mons->pos()) > 2)
        {
            monster_die(*mons, KILL_TIMEOUT, NON_MONSTER);
            return;
        }
    }

    if (mons->type == MONS_BLAZEHEART_CORE)
    {
        mons->suicide();
        return;
    }

    if (mons->type == MONS_RENDING_BLADE)
    {
        // Perform as many slashes as we are able and have charge for.
        bool did_slash = false;
        while (_handle_rending_blade_trigger(mons))
            did_slash = true;

        // Pause in place after attacking (for slightly better visuals).
        if (did_slash)
        {
            mons->speed_increment = 60;
            return;
        }
    }

    // Friendly player shadows don't act independently (though hostile ones from
    // wrath effects may do so)
    if (mons_is_player_shadow(*mons))
    {
        mons->lose_energy(EUT_MOVE);
        return;
    }

    mons->shield_blocks = 0;
    check_spectral_weapon(*mons);

    actor_apply_cloud(mons);
    actor_apply_toxic_bog(mons);

    if (!mons->alive())
        return;

    if (you.duration[DUR_OOZEMANCY] && (env.level_state & LSTATE_SLIMY_WALL))
        slime_wall_damage(mons, speed_to_duration(mons->speed));

    if (!mons->alive())
        return;

    if (env.level_state & LSTATE_ICY_WALL)
        ice_wall_damage(*mons, speed_to_duration(mons->speed));

    if (!mons->alive())
        return;

    if (mons->type == MONS_TIAMAT && one_chance_in(3))
        draconian_change_colour(mons);

    if (mons->type == MONS_JEREMIAH && !mons->asleep())
        for (int i = 0; i < 2; i++)
            _passively_summon_butterfly(*mons);

    _monster_regenerate(mons);

    if (mons->has_ench(ENCH_VEXED))
    {
        do_vexed_attack(*mons);
        mons->lose_energy(EUT_ATTACK);
        return;
    }

    // Please change _slouch_damage to match!
    if (mons->cannot_act()
        || mons->type == MONS_SIXFIRHY // these move only 8 of 24 turns
            && ++mons->move_spurt / 8 % 3 != 2  // but are not helpless
        || mons->type == MONS_JIANGSHI // similarly, but more irregular (48 of 90)
            && (++mons->move_spurt / 6 % 3 == 1 || mons->move_spurt / 3 % 5 == 1))
    {
        mons->speed_increment -= non_move_energy;
        return;
    }

    // Continue reciting.
    if (mons->has_ench(ENCH_WORD_OF_RECALL))
    {
        mons->speed_increment -= non_move_energy;
        return;
    }

    if (mons->has_ench(ENCH_CHANNEL_SEARING_RAY))
    {
        // If we are continuing to fire searing ray, remain in place.
        // XXX: Doesn't track how many turns this has been channelled, but that
        //      doesn't presently matter.
        if (handle_searing_ray(*mons, 1))
        {
            mons->speed_increment -= non_move_energy;
            return;
        }
        // Otherwise (if it was cancelled or interrupted), take turn as normal
    }

    if (mons->has_ench(ENCH_DAZED) && one_chance_in(4))
    {
        simple_monster_message(*mons, " is lost in a daze.");
        mons->speed_increment -= non_move_energy;
        return;
    }

    if (mons->has_ench(ENCH_GOLD_LUST))
    {
        mons->speed_increment -= non_move_energy;
        return;
    }

    if (you.duration[DUR_GOZAG_GOLD_AURA]
        && have_passive(passive_t::gold_aura)
        && you.see_cell(mons->pos())
        && !mons->asleep()
        && !mons->is_peripheral()
        && !mons->wont_attack())
    {
        const int gold = you.props[GOZAG_GOLD_AURA_KEY].get_int();
        if (x_chance_in_y(3 * gold, 100))
        {
            simple_monster_message(*mons,
                " is distracted by your dazzling golden aura.");

            mons->add_ench(
                mon_enchant(ENCH_GOLD_LUST, 1, nullptr,
                            random_range(1, 5) * BASELINE_DELAY));
            mons->foe = MHITNOT;
            mons->target = mons->pos();
            mons->speed_increment -= non_move_energy;
            return;
        }
    }

    if (disabled)
    {
        mons->speed_increment -= non_move_energy;
        return;
    }

    handle_behaviour(mons);

    // handle_behaviour() could make the monster leave the level.
    if (!mons->alive())
        return;

    ASSERT(!crawl_state.game_is_arena() || mons->foe != MHITYOU);
    ASSERT_IN_BOUNDS_OR_ORIGIN(mons->target);

    if (mons->speed >= 100)
    {
        mons->speed_increment -= non_move_energy;
        return;
    }

    if (_handle_pickup(mons))
    {
        DEBUG_ENERGY_USE("handle_pickup()");
        return;
    }

    if (mons->caught())
    {
        // Struggling against the net takes time.
        _swim_or_move_energy(*mons);
    }
    else if (!mons->petrified())
    {
        // Calculates mmov based on monster target.
        mmov = _find_best_step(mons);

        // Confused monsters sometimes stumble about instead of moving with
        // purpose.
        if (mons_is_confused(*mons) && !one_chance_in(3))
        {
            set_random_target(mons);
            mmov = _confused_move_dir(mons);
        }
    }
    if (!mons->asleep())
        maybe_mons_speaks(mons);

    if (!mons->alive())
        return;

    // XXX: A bit hacky, but stores where we WILL move, if we don't take
    //      another action instead (used for decision-making)
    if (mons_stores_tracking_data(*mons))
        mons->props[MMOV_KEY].get_coord() = mmov;

    if (_mons_take_special_action(*mons, old_energy))
        return;

    const coord_def new_pos = mons->pos() + mmov;
    if (!mons->caught())
    {
        if (new_pos == you.pos())
        {
            ASSERT(!crawl_state.game_is_arena());

            if (_unfriendly_or_impaired(*mons)
                && !mons->has_ench(ENCH_CHARM)
                && !mons->has_ench(ENCH_HEXED))
            {
                monster* new_target = nullptr;
                // XXX: why does this check exist?
                if (!mons->wont_attack())
                {
                    // Otherwise, if it steps into you, cancel other targets.
                    mons->foe = MHITYOU;
                    mons->target = you.pos();

                    if (mons_has_attacks(*mons, true)
                        && _handle_ru_melee_redirection(*mons, &new_target))
                    {
                        mons->speed_increment -= non_move_energy;
                        DEBUG_ENERGY_USE("_handle_ru_redirection()");
                        return;
                    }
                }

                _melee_attack_player(*mons, new_target);

                // chance to confusedly whack itself at the same time. Handled
                // separately for monsters in _monster_move.
                if (!new_target && mons->confused() && one_chance_in(6))
                    _do_move_monster(*mons, coord_def(0,0));
                _handle_battiness(*mons);
                DEBUG_ENERGY_USE("fight_melee()");
                return;
            }
        }

        // See if we move into (and fight) an unfriendly monster.
        monster* targ = monster_at(new_pos);

        //If a tentacle owner is attempting to move into an adjacent
        //segment, kill the segment and adjust connectivity data.
        if (targ && mons_tentacle_adjacent(mons, targ))
        {
            const bool basis = targ->props.exists(OUTWARDS_KEY);
            monster* outward =  basis ? monster_by_mid(targ->props[OUTWARDS_KEY].get_int()) : nullptr;
            if (outward)
                outward->props[INWARDS_KEY].get_int() = mons->mid;

            monster_die(*targ, KILL_TENTACLE_CLEANUP, NON_MONSTER, true);
            targ = nullptr;
        }

        if (targ
            && targ != mons
            && mons->behaviour != BEH_WITHDRAW
            && !_beetle_must_return(mons)
            && (!(mons_aligned(mons, targ) || targ->type == MONS_FOXFIRE)
                || mons->has_ench(ENCH_FRENZIED))
            && monster_los_is_valid(mons, targ))
        {
            // Maybe they can swap places?
            if (_swap_monsters(*mons, *targ))
            {
                _swim_or_move_energy(*mons);
                return;
            }
            // Figure out if they fight.
            else if ((!targ->is_firewood()
                      || mons->is_child_tentacle())
                          && fight_melee(mons, targ))
            {
                _handle_battiness(*mons);

                DEBUG_ENERGY_USE("fight_melee()");
                return;
            }
        }
        else if (mons->behaviour == BEH_WITHDRAW
                 && ((targ && targ != mons && targ->friendly())
                      || (you.pos() == new_pos)))
        {
            // Don't count turns spent blocked by friendly creatures
            // (or the player) as an indication that we're stuck
            mons->props.erase(BLOCKED_DEADLINE_KEY);
        }

        if (invalid_monster(mons) || mons->is_stationary())
        {
            if (mons->speed_increment == old_energy)
                mons->speed_increment -= non_move_energy;
            return;
        }

        if (mons->cannot_act() || !_monster_move(mons, mmov))
            mons->speed_increment -= non_move_energy;
    }
    you.update_beholder(mons);
    you.update_fearmonger(mons);

    // Reevaluate behaviour, since the monster's surroundings have
    // changed (it may have moved, or died for that matter). Don't
    // bother for dead monsters.  :)
    if (mons->alive())
    {
        handle_behaviour(mons);
        ASSERT_IN_BOUNDS_OR_ORIGIN(mons->target);
    }

    if (mons_is_tentacle_head(mons_base_type(*mons)))
    {
        move_child_tentacles(mons);

        mons->move_spurt += (old_energy - mons->speed_increment)
                             * _tentacle_move_speed(mons_base_type(*mons));
        ASSERT(mons->move_spurt > 0);
        while (mons->move_spurt >= 100)
        {
            move_child_tentacles(mons);
            mons->move_spurt -= 100;
        }
    }
}

void monster::catch_breath()
{
    decay_enchantment(ENCH_BREATH_WEAPON);
}

/**
 * Let trapped monsters struggle against nets, webs, etc.
 */
void monster::struggle_against_net()
{
    if (is_stationary() || cannot_act() || asleep())
        return;

    if (props.exists(NEWLY_TRAPPED_KEY))
    {
        props.erase(NEWLY_TRAPPED_KEY);
        return; // don't try to escape on the same turn you were trapped!
    }

    int net = get_trapping_net(pos(), true);

    if (net == NON_ITEM)
    {
        trap_def *trap = trap_at(pos());
        if (trap && trap->type == TRAP_WEB)
        {
            if (coinflip())
            {
                if (you.see_cell(pos()))
                {
                    if (!visible_to(&you))
                        mpr("Something you can't see is thrashing in a web.");
                    else
                        simple_monster_message(*this,
                                           " struggles to get unstuck from the web.");
                }
                return;
            }
        }
        monster_web_cleanup(*this);
        del_ench(ENCH_HELD);
        return;
    }

    if (you.see_cell(pos()))
    {
        if (!visible_to(&you))
            mpr("Something wriggles in the net.");
        else
            simple_monster_message(*this, " struggles against the net.");
    }

    int damage = 1 + random2(2);

    // Faster monsters can damage the net more often per
    // time period.
    if (speed != 0)
        damage = div_rand_round(damage * speed, 10);

    env.item[net].net_durability -= damage;

    if (env.item[net].net_durability < NET_MIN_DURABILITY)
    {
        if (you.see_cell(pos()))
        {
            if (visible_to(&you))
            {
                mprf("The net rips apart, and %s comes free!",
                     name(DESC_THE).c_str());
            }
            else
                mpr("All of a sudden the net rips apart!");
        }
        destroy_item(net);

        del_ench(ENCH_HELD, true);
    }
}

static void _ancient_zyme_sicken(monster* mons)
{
    if (is_sanctuary(mons->pos()))
        return;

    if (!is_sanctuary(you.pos())
        && !mons->wont_attack()
        && !you.res_miasma()
        && !you.duration[DUR_DIVINE_STAMINA]
        && cell_see_cell(you.pos(), mons->pos(), LOS_SOLID_SEE))
    {
        if (!you.duration[DUR_SICKNESS])
        {
            if (!you.duration[DUR_SICKENING])
            {
                mprf(MSGCH_WARN, "You feel yourself growing ill in the "
                                 "presence of %s.",
                    mons->name(DESC_THE).c_str());
            }

            you.duration[DUR_SICKENING] += (2 + random2(4)) * BASELINE_DELAY;
            if (you.duration[DUR_SICKENING] > 100)
            {
                you.sicken(40 + random2(30));
                you.duration[DUR_SICKENING] = 0;
            }
        }
        else
        {
            if (x_chance_in_y(you.time_taken, 60))
                you.sicken(15 + random2(30));
        }
    }

    for (radius_iterator ri(mons->pos(), LOS_RADIUS, C_SQUARE); ri; ++ri)
    {
        monster *m = monster_at(*ri);
        if (m && !mons_aligned(mons, m)
            && cell_see_cell(mons->pos(), *ri, LOS_SOLID_SEE)
            && !is_sanctuary(*ri))
        {
            m->sicken(2 * you.time_taken);
        }
    }
}

static void _post_monster_move(monster* mons)
{
    if (invalid_monster(mons))
        return;

    mons->handle_constriction();

    if (mons->has_ench(ENCH_HELD))
        mons->struggle_against_net();

    if (mons->has_ench(ENCH_BREATH_WEAPON))
        mons->catch_breath();

    if (mons->type == MONS_ANCIENT_ZYME)
        _ancient_zyme_sicken(mons);

    mons_update_aura(*mons);

    // Check golem distance again at the END of its move (so that it won't go
    // dormant if it is following a player who was adjacent to it at the start
    // of the player's own move)
    if (mons->type == MONS_BLAZEHEART_GOLEM)
         _check_blazeheart_golem_link(*mons);

    if (mons->type == MONS_WATER_NYMPH
        || mons->type == MONS_ELEMENTAL_WELLSPRING
        || mons->type == MONS_NORRIS)
    {
        for (adjacent_iterator ai(mons->pos(), false); ai; ++ai)
            if (feat_has_solid_floor(env.grid(*ai))
                && (coinflip() || *ai == mons->pos()))
            {
                if (env.grid(*ai) != DNGN_SHALLOW_WATER
                    && env.grid(*ai) != DNGN_FLOOR
                    && you.see_cell(*ai))
                {
                    mprf("%s watery aura covers %s.",
                         apostrophise(mons->name(DESC_THE)).c_str(),
                         feature_description_at(*ai, false, DESC_THE).c_str());
                }
                temp_change_terrain(*ai, DNGN_SHALLOW_WATER,
                                    random_range(50, 80),
                                    TERRAIN_CHANGE_FLOOD, mons->mid);
            }
    }

    // A rakshasa that has regained full health dismisses its emergency clones
    // (if they're somehow still alive) and regains the ability to summon new ones.
    if (mons->type == MONS_RAKSHASA && mons->hit_points == mons->max_hit_points
        && !mons->has_ench(ENCH_PHANTOM_MIRROR)
        && mons->props.exists(EMERGENCY_CLONE_KEY))
    {
        mons->props.erase(EMERGENCY_CLONE_KEY);
        for (monster_iterator mi; mi; ++mi)
        {
            if (mi->type == MONS_RAKSHASA && mi->summoner == mons->mid)
                mi->del_ench(ENCH_SUMMON_TIMER);
        }
    }

    if (mons->type == MONS_VAMPIRE_BAT)
        blorkula_bat_merge(*mons);

    update_mons_cloud_ring(mons);

    const item_def * weapon = mons->mslot_item(MSLOT_WEAPON);
    if (weapon && get_weapon_brand(*weapon) == SPWPN_SPECTRAL
        && !mons_is_avatar(mons->type))
    {
        // TODO: implement monster spectral ego
    }

    if (mons->behaviour == BEH_BATTY)
    {
        int &bat_turns = mons->props[BATTY_TURNS_KEY].get_int();
        bat_turns++;
        int turns_to_bat = div_rand_round(mons_base_speed(*mons), 10);
        if (turns_to_bat < 2)
            turns_to_bat = 2;
        if (bat_turns >= turns_to_bat)
            mons->behaviour = BEH_SEEK;
    }

    // Don't let monsters launch pursuit attacks on their second move
    // after the player's turn.
    crawl_state.potential_pursuers.erase(mons);

    if (mons->type != MONS_NO_MONSTER && mons->hit_points < 1)
        monster_die(*mons, KILL_NON_ACTOR, NON_MONSTER);
}

priority_queue<pair<monster *, int>,
               vector<pair<monster *, int> >,
               MonsterActionQueueCompare> monster_queue;

// Inserts a monster into the monster queue (needed to ensure that any monsters
// given energy or an action by a effect can actually make use of that energy
// this round)
void queue_monster_for_action(monster* mons)
{
    monster_queue.emplace(mons, mons->speed_increment);
}

void clear_monster_flags()
{
    // Clear any summoning flags so that lower indiced monsters get their
    // actions in the next round. Also clear one-turn deep sleep flag.
    // Finally, track the highest index of monster still alive, for
    // monster_iterator optimisation purposes.
    env.max_mon_index = 0;
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        if (env.mons[i].defined())
        {
            env.max_mon_index = i;
            env.mons[i].flags &= ~MF_JUST_SUMMONED & ~MF_JUST_SLEPT;
        }
    }
}

/**
* On each monster turn, check to see if we need to update monster attitude.
* At the time of writing, it just checks for MUT_NO_LOVE from Ru Sacrifice Love
* and Okawaru's ally-prevention conduct.
*
* @param mon     The targeted monster
* @return        Void
**/
static void _update_monster_attitude(monster *mon)
{
    if (you.allies_forbidden() && !mon->is_peripheral())
        mon->attitude = ATT_HOSTILE;
}

vector<monster *> just_seen_queue;

void mons_set_just_seen(monster *mon)
{
    mon->seen_context = SC_JUST_SEEN;
    just_seen_queue.push_back(mon);
}

void mons_reset_just_seen()
{
    // this may be called when the pointers are not valid, so don't mess with
    // seen_context.
    just_seen_queue.clear();
}

static void _display_just_seen()
{
    // these are monsters that were marked as SC_JUST_SEEN at some point since
    // last time this was called. We announce any that leave all at once so
    // as to handle monsters that may move multiple times per world_reacts.
    if (in_bounds(you.pos()))
    {
        for (auto m : just_seen_queue)
        {
            if (!m || invalid_monster(m) || !m->alive())
                continue;
            // can't use simple_monster_message here, because m is out of view.
            // The monster should be visible to be in this queue.
            if (in_bounds(m->pos()) && !you.see_cell(m->pos()))
            {
                mprf(MSGCH_PLAIN, "%s moves out of view.",
                     m->name(DESC_THE, true).c_str());
            }
        }
    }
    mons_reset_just_seen();
}

/**
 * Get all monsters to make an action, if they can/want to.
 *
 * @param with_noise whether to process noises after the loop.
 */
void handle_monsters(bool with_noise)
{
    for (monster_iterator mi; mi; ++mi)
    {
        _pre_monster_move(**mi);
        if (!invalid_monster(*mi) && mi->alive() && mi->has_action_energy())
            monster_queue.emplace(*mi, mi->speed_increment);
        fire_final_effects();
    }

    int tries = 0; // infinite loop protection, shouldn't be ever needed
    while (!monster_queue.empty())
    {
        if (tries++ > 32767)
        {
            die("infinite handle_monsters() loop, mons[0 of %d] is %s",
                (int)monster_queue.size(),
                monster_queue.top().first->name(DESC_PLAIN, true).c_str());
        }

        monster *mon = monster_queue.top().first;
        const int oldspeed = monster_queue.top().second;
        monster_queue.pop();

        if (invalid_monster(mon) || !mon->alive() || !mon->has_action_energy())
            continue;

        _update_monster_attitude(mon);

        // Only move the monster if nothing else has played with its energy
        // during their turn.
        // If something's played with the energy, they get added back to
        // the queue just after this.
        if (oldspeed == mon->speed_increment)
        {
            handle_monster_move(mon);
            _post_monster_move(mon);
            fire_final_effects();
        }

        if (mon->has_action_energy())
            monster_queue.emplace(mon, mon->speed_increment);

        // If the player got banished, discard pending monster actions.
        if (you.banished)
        {
            // Clear list of mesmerising monsters.
            you.clear_beholders();
            you.clear_fearmongers();
            you.stop_constricting_all();
            you.stop_being_constricted();
            break;
        }
    }
    _display_just_seen();

    // Process noises now (before clearing the sleep flag).
    if (with_noise)
        apply_noises();
}

static bool _jelly_divide(monster& parent)
{
    if (!mons_class_flag(parent.type, M_SPLITS))
        return false;

    const int reqd = max(parent.get_experience_level() * 8, 50);
    if (parent.hit_points < reqd)
        return false;

    monster* child = nullptr;
    coord_def child_spot;
    int num_spots = 0;

    // First, find a suitable spot for the child {dlb}:
    for (adjacent_iterator ai(parent.pos()); ai; ++ai)
        if (actor_at(*ai) == nullptr && parent.can_pass_through(*ai)
            && one_chance_in(++num_spots))
        {
            child_spot = *ai;
        }

    if (num_spots == 0)
        return false;

    // Now that we have a spot, find a monster slot {dlb}:
    child = get_free_monster();
    if (!child)
        return false;

    // Handle impact of split on parent {dlb}:
    parent.max_hit_points /= 2;

    if (parent.hit_points > parent.max_hit_points)
        parent.hit_points = parent.max_hit_points;

    // Create child {dlb}:
    // This is terribly partial and really requires
    // more thought as to generation ... {dlb}
    *child = parent;
    child->max_hit_points  = child->hit_points;
    child->speed_increment = 70 + random2(5);
    child->set_new_monster_id();
    child->move_to_pos(child_spot);

    if (!simple_monster_message(parent, " splits in two!")
        && (player_can_hear(parent.pos()) || player_can_hear(child->pos())))
    {
        mprf(MSGCH_SOUND, "You hear a squelching noise.");
    }

    if (crawl_state.game_is_arena())
        arena_placed_monster(child);

    return true;
}

// Only Jiyva jellies eat items.
static bool _monster_eat_item(monster* mons)
{
    // Off-limit squares are off-limit.
    if (testbits(env.pgrid(mons->pos()), FPROP_NO_JIYVA))
        return false;

    int hps_changed = 0;
    int max_eat = roll_dice(1, 10);
    int eaten = 0;
    bool shown_msg = false;

    // Jellies can swim, so don't check water
    for (stack_iterator si(mons->pos());
         si && eaten < max_eat && hps_changed < 50; ++si)
    {
        if (!item_is_jelly_edible(*si))
            continue;

        dprf("%s eating %s", mons->name(DESC_PLAIN, true).c_str(),
             si->name(DESC_PLAIN).c_str());

        int quant = si->quantity;

        if (si->base_type != OBJ_GOLD)
        {
            quant = min(quant, max_eat - eaten);

            hps_changed += quant * 3;
            eaten += quant;
        }
        else
        {
            // Shouldn't be much trouble to digest a huge pile of gold!
            if (quant > 500)
                quant = 500 + roll_dice(2, (quant - 500) / 2);

            hps_changed += quant / 10 + 1;
            eaten++;
        }

        if (eaten && !shown_msg && player_can_hear(mons->pos()))
        {
            mprf(MSGCH_SOUND, "You hear a%s slurping noise.",
                 you.see_cell(mons->pos()) ? "" : " distant");
            shown_msg = true;
        }

        if (quant >= si->quantity)
            item_was_destroyed(*si);
        dec_mitm_item_quantity(si.index(), quant, false);
    }

    if (eaten > 0)
    {
        hps_changed = max(hps_changed, 1);
        hps_changed = min(hps_changed, 50);

        // This is done manually instead of using heal_monster(),
        // because that function doesn't work quite this way. - bwr
        const int avg_hp = mons_avg_hp(mons->type);
        mons->hit_points += hps_changed;
        mons->hit_points = min(MAX_MONSTER_HP,
                               min(avg_hp * 4, mons->hit_points));
        mons->max_hit_points = max(mons->hit_points, mons->max_hit_points);

        _jelly_divide(*mons);
    }

    return eaten > 0;
}


static bool _handle_pickup(monster* mons)
{
    if (env.igrid(mons->pos()) == NON_ITEM
        // Summoned monsters never pick anything up.
        || mons->is_summoned()
        // And monsters which would remove the item from existence also shouldn't
        || mons->flags & MF_HARD_RESET
        || mons->asleep())
    {
        return false;
    }

    // Flying over water doesn't let you pick up stuff. This is inexact, as
    // a merfolk could be flying, but that's currently impossible except for
    // being polar vortex'd, and with *that* low life expectancy let's not care.
    dungeon_feature_type feat = env.grid(mons->pos());

    if ((feat == DNGN_LAVA || feat == DNGN_DEEP_WATER) && mons->airborne())
        return false;

    int count_pickup = 0;

    if (mons_eats_items(*mons) && _monster_eat_item(mons))
        return false;

    if (mons_itemuse(*mons) < MONUSE_WEAPONS_ARMOUR)
        return false;

    // Keep neutral, charmed, and friendly monsters from
    // picking up stuff.
    const bool never_pickup
        = mons->neutral() || mons->friendly()
          || have_passive(passive_t::neutral_slimes) && mons_is_slime(*mons)
          || mons->has_ench(ENCH_CHARM) || mons->has_ench(ENCH_HEXED);


    // Note: Monsters only look at stuff near the top of stacks.
    //
    // Monsters may now pick up up to two items in the same turn.
    // (jpeg)
    for (stack_iterator si(mons->pos()); si; ++si)
    {
        if (!crawl_state.game_is_arena()
            && (never_pickup
                // Monsters being able to pick up items you've seen encourages
                // tediously moving everything away from a place where they
                // could use them. Maurice being able to pick up such items
                // encourages killing Maurice, since there's just one of him.
                // Usually.
                || (testbits(si->flags, ISFLAG_SEEN)
                    && !mons->has_attack_flavour(AF_STEAL)))
            // ...but it's ok if it dropped the item itself.
            && !(si->props.exists(DROPPER_MID_KEY)
                 && si->props[DROPPER_MID_KEY].get_int() == (int)mons->mid))
        {
            // don't pick up any items beneath one that the player's seen,
            // to prevent seemingly-buggy behavior (monsters picking up items
            // from the middle of a stack while the player is watching)
            return false;
        }

        if (si->flags & ISFLAG_NO_PICKUP)
            continue;

        if (mons->pickup_item(*si, you.see_cell(mons->pos()), false))
        {
            count_pickup++;
            si->props.erase(DROPPER_MID_KEY);
        }

        if (count_pickup > 1 || coinflip())
            break;
    }

    return count_pickup > 0;
}

static void _mons_open_door(monster& mons, const coord_def &pos)
{
    const char *adj = "", *noun = "door";

    bool was_seen   = false;

    set<coord_def> all_door;
    find_connected_identical(pos, all_door);
    get_door_description(all_door.size(), &adj, &noun);

    const bool broken = mons.behaviour == BEH_SEEK
                        && (mons.berserk() || one_chance_in(3));
    for (const auto &dc : all_door)
    {
        if (you.see_cell(dc))
            was_seen = true;

        if (broken)
            dgn_break_door(dc);
        else
            dgn_open_door(dc);
        set_terrain_changed(dc);
    }

    if (was_seen)
    {
        viewwindow();
        update_screen();

        // XXX: should use custom verbs
        string open_str = broken ? "breaks down the " : "opens the ";
        open_str += adj;
        open_str += noun;
        open_str += ".";

        // Should this be conditionalized on you.can_see(mons?)
        mons.seen_context = (all_door.size() <= 2) ? SC_DOOR : SC_GATE;

        if (!you.can_see(mons))
        {
            mprf("Something unseen %s", open_str.c_str());
            interrupt_activity(activity_interrupt::sense_monster);
        }
        else if (!you_are_delayed())
        {
            mprf("%s %s", mons.name(DESC_A).c_str(),
                 open_str.c_str());
        }
    }

    mons.lose_energy(EUT_MOVE);

    dungeon_events.fire_position_event(DET_DOOR_OPENED, pos);
}

static bool _no_habitable_adjacent_grids(const monster* mon)
{
    for (adjacent_iterator ai(mon->pos()); ai; ++ai)
        if (monster_habitable_grid(mon, *ai))
            return false;

    return true;
}

static bool _same_tentacle_parts(const monster* mpusher,
                               const monster* mpushee)
{
    if (!mons_is_tentacle_head(mons_base_type(*mpusher)))
        return false;

    if (mpushee->is_child_tentacle_of(mpusher))
        return true;

    if (mons_tentacle_adjacent(mpusher, mpushee))
        return true;

    return false;
}

static bool _mons_can_displace(const monster* mpusher,
                               const monster* mpushee)
{
    if (invalid_monster(mpusher) || invalid_monster(mpushee))
        return false;

    const int ipushee = mpushee->mindex();
    if (invalid_monster_index(ipushee))
        return false;

    if (!mpushee->has_action_energy()
        && !_same_tentacle_parts(mpusher, mpushee)
        && mpushee->type != MONS_FOXFIRE)
    {
        return false;
    }

    // Confused monsters can't be pushed past, sleeping monsters
    // can't push. Note that sleeping monsters can't be pushed
    // past, either, but they may be woken up by a crowd trying to
    // elbow past them, and the wake-up check happens downstream.
    if (mons_is_confused(*mpusher)
        || mons_is_confused(*mpushee)
        || !_same_tentacle_parts(mpusher, mpushee) && mpushee->unswappable()
        || mpusher->unswappable()
        || mpusher->asleep())
    {
        return false;
    }

    // Foxfires can always be pushed
    if (mpushee->type == MONS_FOXFIRE)
        return !mons_aligned(mpushee, mpusher); // But allies won't do it

    // OODs should crash into things, not push them around.
    if (mons_is_projectile(*mpusher) || mons_is_projectile(*mpushee))
        return false;

    // Likewise, OOBs (orbs of beetle)
    if (mpusher->has_ench(ENCH_ROLLING) || mpushee->has_ench(ENCH_ROLLING))
        return false;

    // Fleeing monsters cannot push past other fleeing monsters
    // (This helps to prevent some traffic jams in confined spaces)
    if (mons_is_fleeing(*mpusher) && mons_is_fleeing(*mpushee))
        return false;

    // Batty monsters are unpushable.
    if (mons_is_batty(*mpusher) || mons_is_batty(*mpushee))
        return false;

    if (!monster_shover(*mpusher))
        return false;

    // Fleeing monsters of the same type may push past higher ranking ones.
    if (!monster_senior(*mpusher, *mpushee, mons_is_retreating(*mpusher)))
        return false;

    return true;
}

// Returns true if the monster should try to avoid that position
// because of taking damage from damaging walls.
static bool _check_damaging_walls(const monster *mon,
                                  const coord_def &targ)
{
    const bool have_slimy = env.level_state & LSTATE_SLIMY_WALL;
    const bool have_icy   = env.level_state & LSTATE_ICY_WALL;

    if (!have_slimy && !have_icy)
        return false;

    if (!have_icy && actor_slime_wall_immune(mon)
        || mons_intel(*mon) <= I_BRAINLESS)
    {
        return false;
    }

    // Monsters are only ever affected by one wall at a time, so we don't care
    // about wall counts past 1.
    const bool target_damages = count_adjacent_slime_walls(targ)
        + count_adjacent_icy_walls(targ);

    // Entirely safe.
    if (!target_damages)
        return false;

    const bool current_damages = count_adjacent_slime_walls(mon->pos())
        + count_adjacent_icy_walls(mon->pos());

    // We're already taking damage, so moving into damage is fine.
    if (current_damages)
        return false;

    // The monster needs to have a purpose to risk taking damage.
    if (!mons_is_seeking(*mon))
        return true;

    // With enough hit points monsters will consider moving
    // onto more dangerous squares.
    return mon->hit_points < mon->max_hit_points / 2;
}

// Check whether a monster can move to given square (described by its relative
// coordinates to the current monster position). just_check is true only for
// calls from is_trap_safe when checking the surrounding squares of a trap.
bool mon_can_move_to_pos(const monster* mons, const coord_def& delta,
                         bool just_check)
{
    const coord_def targ = mons->pos() + delta;

    // Bounds check: don't consider moving out of grid!
    if (!in_bounds(targ))
        return false;

    // Non-friendly and non-good neutral monsters won't enter
    // sanctuaries.
    if (is_sanctuary(targ)
        && !is_sanctuary(mons->pos())
        && !mons->wont_attack())
    {
        return false;
    }

    // Inside a sanctuary don't attack anything!
    if (is_sanctuary(mons->pos()) && actor_at(targ))
        return false;

    const dungeon_feature_type target_grid = env.grid(targ);
    const habitat_type habitat = mons_primary_habitat(*mons);

    // No monster may enter the open sea.
    if (feat_is_endless(target_grid))
        return false;

    // A confused monster will happily go wherever it can, regardless of
    // consequences.
    if (mons->confused() && mons->can_pass_through(targ))
        return true;

    if (mons_avoids_cloud(mons, targ))
        return false;

    if (_check_damaging_walls(mons, targ))
        return false;

    const bool digs = _mons_can_cast_dig(mons, false);
    if (digs && feat_is_diggable(target_grid)
        || mons->can_burrow_through(target_grid)
        || mons->type == MONS_SPATIAL_MAELSTROM
           && feat_is_solid(target_grid) && !feat_is_permarock(target_grid)
           && !feat_is_critical(target_grid)
        || feat_is_tree(target_grid) && mons_flattens_trees(*mons))
    {
    }
    else if (!mons_can_traverse(*mons, targ, false, false)
             && !monster_habitable_feat(mons, target_grid))
    {
        // If the monster somehow ended up in this habitat (and is
        // not dead by now), give it a chance to get out again.
        if (env.grid(mons->pos()) == target_grid && mons->ground_level()
            && _no_habitable_adjacent_grids(mons))
        {
            return true;
        }

        return false;
    }

    if (mons->type == MONS_MERFOLK_AVATAR)
    {
        // Don't voluntarily break LoS with a player we're mesmerising
        if (you.beheld_by(*mons) && !you.see_cell(targ))
            return false;

        // And path around players instead of into them
        if (you.pos() == targ)
            return false;
    }

    // Try to avoid deliberately blocking the player's line of fire.
    if (mons->type == MONS_SPELLSPARK_SERVITOR)
    {
        const actor * const summoner = actor_by_mid(mons->summoner);

        if (!summoner) // XXX
            return false;

        // Only check if our target is something the caster could theoretically
        // be aiming at.
        if (mons->get_foe() && mons->target != summoner->pos()
                            && summoner->see_cell_no_trans(mons->target))
        {
            ray_def ray;
            if (find_ray(summoner->pos(), mons->target, ray, opc_immob))
            {
                while (ray.advance())
                {
                    // Either we've reached the end of the ray, or we're already
                    // (maybe) in the player's way and shouldn't care if our
                    // next step also is.
                    if (ray.pos() == mons->target || ray.pos() == mons->pos())
                        break;
                    else if (ray.pos() == targ)
                        return false;
                }
            }
        }
    }

    // Never voluntarily leave your creator's side if you're already next to
    // them (but can freely move to catch up with them, if not.)
    if (mons->type == MONS_PHALANX_BEETLE)
    {
        actor* creator = actor_by_mid(mons->summoner);
        if (creator && creator->alive())
        {
            if (adjacent(creator->pos(), mons->pos())
                && !adjacent(creator->pos(), targ))
            {
                return false;
            }
            // Don't consider moving into enemies good enough if we're trying
            // to return to our creator.
            else if (!adjacent(creator->pos(), mons->pos())
                     && actor_at(targ))
            {
                return false;
            }
        }
    }

    // Keep helpers with somewhat chaotic movement from deliberately breaking
    // LoS with their owners.
    if (mons->type == MONS_BATTLESPHERE || mons->type == MONS_RENDING_BLADE)
    {
        actor* creator = actor_by_mid(mons->summoner);
        if (creator && creator->alive())
        {
            if (creator->see_cell_no_trans(mons->pos())
                && !creator->see_cell_no_trans(targ))
            {
                return false;
            }
        }
    }

    // Submerged water creatures avoid the shallows where
    // they would be forced to surface. -- bwr
    // [dshaligram] Monsters now prefer to head for deep water only if
    // they're low on hitpoints. No point in hiding if they want a
    // fight.
    if (habitat == HT_WATER
        && targ != you.pos()
        && target_grid != DNGN_DEEP_WATER
        && env.grid(mons->pos()) == DNGN_DEEP_WATER
        && mons->hit_points < (mons->max_hit_points * 3) / 4)
    {
        return false;
    }

    // Smacking the player is always a good move if we're
    // hostile (even if we're heading somewhere else).
    // Also friendlies want to keep close to the player
    // so it's okay as well.

    // Smacking another monster is good, if the monsters
    // are aligned differently.
    if (monster* targmonster = monster_at(targ))
    {
        if (just_check)
        {
            if (targ == mons->pos())
                return true;

            return false; // blocks square
        }

        if (!monster_los_is_valid(mons, targ))
            return false;

        // Cut down plants only when no alternative, or they're
        // our target.
        if (targmonster->is_firewood() && mons->target != targ)
            return false;

        if ((mons_aligned(mons, targmonster)
             || targmonster->type == MONS_FOXFIRE)
            && !mons->has_ench(ENCH_FRENZIED)
            && !_mons_can_displace(mons, targmonster))
        {
            return false;
        }
        // Prefer to move past enemies instead of hit them, if we're retreating
        else if ((!mons_aligned(mons, targmonster)
                  || mons->has_ench(ENCH_FRENZIED))
                 && mons->behaviour == BEH_WITHDRAW)
        {
            return false;
        }
    }

    // Friendlies shouldn't try to move onto the player's
    // location, if they are aiming for some other target.
    if (mons->foe != MHITYOU
        && targ == you.pos()
        && (mons->foe != MHITNOT || mons->is_patrolling())
        && !_unfriendly_or_impaired(*mons))
    {
        return false;
    }

    // Wandering through a trap sometimes isn't allowed for friendlies.
    if (!mons->is_trap_safe(targ))
        return false;

    // If we end up here the monster can safely move.
    return true;
}

// May mons attack targ if it's in its way, despite
// possibly aligned attitudes?
// The aim of this is to have monsters cut down plants
// to get to the player if necessary.
static bool _may_cutdown(monster* mons, monster* targ)
{
    // Save friendly plants from allies.
    // [ds] I'm deliberately making the alignment checks symmetric here.
    // The previous check involved good-neutrals never attacking friendlies
    // and friendlies never attacking anything other than hostiles.
    if ((mons->friendly() || mons->good_neutral())
         && (targ->friendly() || targ->good_neutral()))
    {
        return false;
    }
    // Outside of that case, can always cut mundane plants
    // (but don't try to attack briars unless their damage will be insignificant)
    return targ->is_firewood()
        && (targ->type != MONS_BRIAR_PATCH
            || (targ->friendly() && !mons_aligned(mons, targ))
            || mons->type == MONS_THORN_HUNTER
            || mons->armour_class() * mons->hit_points >= 400);
}

static void _find_good_alternate_move(monster* mons, coord_def& delta,
                                      const move_array& good_move)
{
    const coord_def target = mons->firing_pos.zero() ? mons->target
                                                     : mons->firing_pos;
    const int current_distance = distance2(mons->pos(), target);

    int dir = _compass_idx(delta);

    // Only handle if the original move is to an adjacent square.
    if (dir == -1)
        return;

    int dist[2];

    // First 1 away, then 2 (3 is silly).
    for (int j = 1; j <= 2; j++)
    {
        const int FAR_AWAY = 1000000;

        // Try both directions (but randomise which one is first).
        const int sdir = random_choose(j, -j);
        const int inc = -2 * sdir;

        for (int mod = sdir, i = 0; i < 2; mod += inc, i++)
        {
            const int newdir = (dir + 8 + mod) % 8;
            if (good_move[mon_compass[newdir].x+1][mon_compass[newdir].y+1])
                dist[i] = distance2(mons->pos()+mon_compass[newdir], target);
            else
            {
                // If we can cut firewood there, it's still not a good move,
                // but update delta so we can fall back to it.
                monster* targ = monster_at(mons->pos() + mon_compass[newdir]);
                const bool retreating = mons_is_retreating(*mons);

                dist[i] = (targ && _may_cutdown(mons, targ))
                          ? current_distance
                          : retreating ? -FAR_AWAY : FAR_AWAY;
            }
        }

        const int dir0 = ((dir + 8 + sdir) % 8);
        const int dir1 = ((dir + 8 - sdir) % 8);

        // Now choose.
        if (dist[0] == dist[1] && abs(dist[0]) == FAR_AWAY)
            continue;

        // Which one was better? -- depends on FLEEING or not.
        if (mons_is_retreating(*mons))
        {
            if (dist[0] >= dist[1] && dist[0] >= current_distance)
            {
                delta = mon_compass[dir0];
                break;
            }
            if (dist[1] >= dist[0] && dist[1] >= current_distance)
            {
                delta = mon_compass[dir1];
                break;
            }
        }
        else
        {
            if (dist[0] <= dist[1] && dist[0] <= current_distance)
            {
                delta = mon_compass[dir0];
                break;
            }
            if (dist[1] <= dist[0] && dist[1] <= current_distance)
            {
                delta = mon_compass[dir1];
                break;
            }
        }
    }
}

static void _jelly_grows(monster& mons)
{
    if (player_can_hear(mons.pos()))
    {
        mprf(MSGCH_SOUND, "You hear a%s slurping noise.",
             you.see_cell(mons.pos()) ? "" : " distant");
    }

    const int avg_hp = mons_avg_hp(mons.type);
    mons.hit_points += 5;
    mons.hit_points = min(MAX_MONSTER_HP,
                          min(avg_hp * 4, mons.hit_points));

    // note here, that this makes jellies "grow" {dlb}:
    if (mons.hit_points > mons.max_hit_points)
        mons.max_hit_points = mons.hit_points;

    _jelly_divide(mons);
}

static bool _monster_swaps_places(monster* mon, const coord_def& delta)
{
    if (delta.origin())
        return false;

    monster* const m2 = monster_at(mon->pos() + delta);

    if (!m2)
        return false;

    if (!_mons_can_displace(mon, m2))
        return false;

    if (m2->asleep())
    {
        if (coinflip())
        {
            dprf("Alerting monster %s at (%d,%d)",
                 m2->name(DESC_PLAIN).c_str(), m2->pos().x, m2->pos().y);
            behaviour_event(m2, ME_ALERT);
        }
        return false;
    }

    if (!mon->swap_with(m2))
        return false;

    _swim_or_move_energy(*mon);
    _swim_or_move_energy(*m2);

    mon->check_redraw(m2->pos(), false);
    mon->apply_location_effects(m2->pos());

    m2->check_redraw(mon->pos(), false);
    m2->apply_location_effects(mon->pos());

    // The seen context no longer applies if the monster is moving normally.
    mon->seen_context = SC_NONE;
    m2->seen_context = SC_NONE;

    mon->did_deliberate_movement();
    m2->did_deliberate_movement();

    // Pushing past a foxfire gets you burned regardless of alignment
    if (m2->type == MONS_FOXFIRE)
    {
        foxfire_attack(m2, mon);
        monster_die(*m2, KILL_RESET, NON_MONSTER, true);
    }

    return false;
}

static void _maybe_randomize_energy(monster &mons, coord_def orig_pos)
{
    if (!mons.alive() // barbs!
        || !crawl_state.potential_pursuers.count(&mons)
        || mons.wont_attack()
        || mons_is_confused(mons, true)
        || mons_is_fleeing(mons)
        || !mons.can_see(you))
    {
        return;
    }


    // Only randomize energy for monsters moving toward you, not doing tricky
    // M_MAINTAIN_RANGE nonsense or wandering around a lake or what have you.
    if (grid_distance(mons.pos(), you.pos()) > grid_distance(orig_pos, you.pos()))
        return;

    // If the monster forgot about you between your turn and its, move on.
    actor* foe = mons.get_foe();
    if (!foe || !foe->is_player())
        return;

    // Randomize energy.
    mons.speed_increment += random2(3) - 1;
}

static void _launch_opportunity_attack(monster& mons)
{
    monster *ru_target = nullptr;
    if (_handle_ru_melee_redirection(mons, &ru_target))
        return;
    _melee_attack_player(mons, ru_target);
    learned_something_new(HINT_OPPORTUNITY_ATTACK);
}

static void _maybe_launch_opportunity_attack(monster &mon, coord_def orig_pos)
{
    if (!mon.alive() || !crawl_state.potential_pursuers.count(&mon))
        return;

    const int new_dist = grid_distance(you.pos(), mon.pos());
    // Some of these duplicate checks when marking potential
    // pursuers. This is to avoid state changes after your turn
    // and before the monster's.
    // No, there is no logic to this ordering (pf):
    if (!mon.alive()
        || !one_chance_in(3)
        || mon.wont_attack()
        || !mons_has_attacks(mon)
        || mon.confused()
        || mon.incapacitated()
        || mons_is_fleeing(mon)
        || !mon.can_see(you)
        // the monster must actually be approaching you.
        || new_dist >= grid_distance(you.pos(), orig_pos)
        // make sure they can actually reach you.
        || new_dist > mon.reach_range()
        || !cell_see_cell(mon.pos(), you.pos(), LOS_NO_TRANS)
        // Zin protects!
        || is_sanctuary(mon.pos()))
    {
        return;
    }

    actor* foe = mon.get_foe();
    if (!foe || !foe->is_player())
        return;

    // No random energy and no double opportunity attacks in a turn
    // that they already launched an attack.
    crawl_state.potential_pursuers.erase(&mon);

    const string msg = make_stringf(" attacks as %s pursue%s you!",
                                    mon.pronoun(PRONOUN_SUBJECTIVE).c_str(),
                                    mon.pronoun_plurality() ? "" : "s");
    simple_monster_message(mon, msg.c_str());
    const int old_energy = mon.speed_increment;
    _launch_opportunity_attack(mon);
    // Refund most of the energy from the attack - for normal attack
    // speed monsters, it will cost 0 energy 1/2 of the time and
    // 1 energy 1/2 of the time.
    // Only slow-attacking monsters will spend more than 1 energy.
    mon.speed_increment = min(mon.speed_increment + 10, old_energy - random2(2));
}

static bool _do_move_monster(monster& mons, const coord_def& delta)
{
    const coord_def orig_pos = mons.pos();
    const coord_def f = mons.pos() + delta;

    if (!in_bounds(f))
        return false;

    if (f == you.pos())
    {
        // XX is this actually reachable?
        fight_melee(&mons, &you);
        return true;
    }

    // This includes the case where the monster attacks itself.
    if (monster* def = monster_at(f))
    {
        fight_melee(&mons, def);
        return true;
    }

    if (mons.is_constricted())
    {
        if (mons.attempt_escape())
            simple_monster_message(mons, " escapes!");
        else
        {
            simple_monster_message(mons, " struggles to escape constriction.");
            _swim_or_move_energy(mons);
            return true;
        }
    }

    // We should have handled all cases of a monster attempting to attack instead of *just* move, so it should fine to simply silently
    // stand in place here.
    if (mons.has_ench(ENCH_BOUND))
        return false;

    ASSERT(!cell_is_runed(f)); // should be checked in mons_can_traverse

    if (feat_is_closed_door(env.grid(f)))
    {
        if (mons_can_destroy_door(mons, f))
        {
            env.grid(f) = DNGN_FLOOR;
            set_terrain_changed(f);

            if (you.see_cell(f))
            {
                viewwindow();
                update_screen();

                if (!you.can_see(mons))
                {
                    mpr("The door bursts into shrapnel!");
                    interrupt_activity(activity_interrupt::sense_monster);
                }
                else
                    simple_monster_message(mons, " bursts through the door, destroying it!");
            }
        }
        else if (mons_can_open_door(mons, f))
        {
            _mons_open_door(mons, f);
            return true;
        }
        else if (mons_can_eat_door(mons, f))
        {
            env.grid(f) = DNGN_FLOOR;
            set_terrain_changed(f);

            _jelly_grows(mons);

            if (you.see_cell(f))
            {
                viewwindow();
                update_screen();

                if (!you.can_see(mons))
                {
                    mpr("The door mysteriously vanishes.");
                    interrupt_activity(activity_interrupt::sense_monster);
                }
                else
                    simple_monster_message(mons, " eats the door!");
            }
        } // done door-eating jellies
    }
    // This appears to be the real one, ie where the movement occurs:

    // The monster gave a "comes into view" message and then immediately
    // moved back out of view, leaving the player nothing to see, so give
    // this message to avoid confusion.
    else if (crawl_state.game_is_hints() && mons.flags & MF_WAS_IN_VIEW
             && !you.see_cell(f))
    {
        learned_something_new(HINT_MONSTER_LEFT_LOS, mons.pos());
    }

    // The seen context no longer applies if the monster is moving normally.
    mons.seen_context = SC_NONE;

    if (mons.type == MONS_FOXFIRE)
        --mons.steps_remaining;

    _escape_water_hold(mons);

    if (env.grid(mons.pos()) == DNGN_DEEP_WATER && env.grid(f) != DNGN_DEEP_WATER
        && !monster_habitable_feat(&mons, DNGN_DEEP_WATER))
    {
        // er, what?  Seems impossible.
        mons.seen_context = SC_NONSWIMMER_SURFACES_FROM_DEEP;
    }

    mons.move_to_pos(f, false);

    // Let go of all constrictees; only stop *being* constricted if we are now
    // too far away (done in move_to_pos above).
    mons.stop_directly_constricting_all(false);

    mons.check_redraw(mons.pos() - delta);
    mons.apply_location_effects(mons.pos() - delta);
    if (!invalid_monster(&mons) && you.can_see(mons))
    {
        handle_seen_interrupt(&mons);
        seen_monster(&mons);
    }

    mons.did_deliberate_movement();

    _swim_or_move_energy(mons);

    _maybe_launch_opportunity_attack(mons, orig_pos);
    _maybe_randomize_energy(mons, orig_pos);

    return true;
}

static bool _monster_move(monster* mons, coord_def& delta)
{
    ASSERT(mons); // XXX: change to monster &mons
    move_array good_move;

    const habitat_type habitat = mons_primary_habitat(*mons);
    bool deep_water_available = false;

    // TODO: move the above logic out of move code.
    if (one_chance_in(10) && you.can_see(*mons) && mons->berserk())
        mprf(MSGCH_TALK_VISUAL, "%s rages.", mons->name(DESC_THE).c_str());
    // Look, this is silly.
    if (one_chance_in(5)
        && mons->has_ench(ENCH_FRENZIED)
        && get_shout_noise_level(mons_shouts(mons->type)))
    {
        monster_attempt_shout(*mons);
    }

    // If a water (or lava) monster is currently flopping around on land, it
    // cannot really control where it wants to move, though there's a 50%
    // chance of flopping into an adjacent water (or lava) grid.
    if (mons->has_ench(ENCH_AQUATIC_LAND))
    {
        vector<coord_def> adj_water;
        vector<coord_def> adj_move;
        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
        {
            if (!cell_is_solid(*ai))
            {
                adj_move.push_back(*ai);
                if (habitat == HT_WATER && feat_is_water(env.grid(*ai))
                    || habitat == HT_LAVA && feat_is_lava(env.grid(*ai)))
                {
                    adj_water.push_back(*ai);
                }
            }
        }
        if (adj_move.empty())
        {
            simple_monster_message(*mons, " flops around on dry land!");
            return false;
        }

        vector<coord_def> moves = adj_water;
        if (adj_water.empty() || coinflip())
            moves = adj_move;

        const coord_def newpos = moves.empty() ? mons->pos()
                                               : moves[random2(moves.size())];

        const monster* mon2 = monster_at(newpos);
        if (!mons->has_ench(ENCH_FRENZIED)
            && (newpos == you.pos() && mons->wont_attack()
                || (mon2 && mons->wont_attack() == mon2->wont_attack())))
        {
            simple_monster_message(*mons, " flops around on dry land!");
            return false;
        }

        return _do_move_monster(*mons, newpos - mons->pos());
    }

    // If we're cautious, can see our foe, and already have a valid spell we
    // could have cast, often hold position instead of advancing closer
    if (mons->flags & MF_CAUTIOUS
        && mons->props.exists(MON_SPELL_USABLE_KEY)
        && mons->get_foe() && mons->can_see(*mons->get_foe())
        && !one_chance_in(3))
    {
        mons->props.erase(MON_SPELL_USABLE_KEY);
        delta.reset();
    }

    // Let's not even bother with this if we're standing still.
    if (delta.origin() && !mons->confused())
        return false;

    for (int count_x = 0; count_x < 3; count_x++)
        for (int count_y = 0; count_y < 3; count_y++)
        {
            const int targ_x = mons->pos().x + count_x - 1;
            const int targ_y = mons->pos().y + count_y - 1;

            // Bounds check: don't consider moving out of grid!
            if (!in_bounds(targ_x, targ_y))
            {
                good_move[count_x][count_y] = false;
                continue;
            }
            dungeon_feature_type target_grid = env.grid[targ_x][targ_y];

            if (target_grid == DNGN_DEEP_WATER)
                deep_water_available = true;

            good_move[count_x][count_y] =
                mon_can_move_to_pos(mons, coord_def(count_x-1, count_y-1));
        }

    // Now we know where we _can_ move.

    const coord_def newpos = mons->pos() + delta;
    // Water creatures have a preference for water they can hide in -- bwr
    // [ds] Weakened the powerful attraction to deep water if the monster
    // is in good health.
    if (habitat == HT_WATER
        && deep_water_available
        && env.grid(mons->pos()) != DNGN_DEEP_WATER
        && env.grid(newpos) != DNGN_DEEP_WATER
        && newpos != you.pos()
        && (one_chance_in(3)
            || mons->hit_points <= (mons->max_hit_points * 3) / 4))
    {
        int count = 0;

        for (int cx = 0; cx < 3; cx++)
            for (int cy = 0; cy < 3; cy++)
            {
                if (good_move[cx][cy]
                    && env.grid[mons->pos().x + cx - 1][mons->pos().y + cy - 1]
                            == DNGN_DEEP_WATER)
                {
                    if (one_chance_in(++count))
                    {
                        delta.x = cx - 1;
                        delta.y = cy - 1;
                    }
                }
            }
    }

    // Now, if a monster can't move in its intended direction, try
    // either side. If they're both good, move in whichever dir
    // gets it closer (farther for fleeing monsters) to its target.
    // If neither does, do nothing.
    if (good_move[delta.x + 1][delta.y + 1] == false)
        _find_good_alternate_move(mons, delta, good_move);

    // ------------------------------------------------------------------
    // If we haven't found a good move by this point, we're not going to.
    // ------------------------------------------------------------------

    if (mons->type == MONS_SPATIAL_MAELSTROM)
    {
        const dungeon_feature_type feat = env.grid(mons->pos() + delta);
        if (!feat_is_permarock(feat) && feat_is_solid(feat))
        {
            const coord_def target(mons->pos() + delta);
            create_monster(
                    mgen_data(MONS_SPATIAL_VORTEX, SAME_ATTITUDE(mons), target,
                              MHITNOT, MG_NONE, GOD_LUGONU)
                    .set_summoned(mons, MON_SUMM_ANIMATE, summ_dur(2)));
            destroy_wall(target);
        }
    }

    // Take care of Dissolution burrowing, lerny, etc
    const dungeon_feature_type feat = env.grid(mons->pos() + delta);
    const bool burrows = mons->can_burrow_through(feat);
    const bool flattens_trees = mons_flattens_trees(*mons) && feat_is_tree(feat);
    const bool digs = _mons_can_cast_dig(mons, false) && feat_is_diggable(feat);
    if (digs)
    {
        bolt beem;
        // XXX: Check for antimagic causing failure at this point. Ideally,
        //      monster spellcasting functions could allow this without duplication.
        if (_mons_can_cast_dig(mons, true))
        {
            setup_mons_cast(mons, beem, SPELL_DIG);
            beem.target = mons->pos() + delta;
            mons_cast(mons, beem, SPELL_DIG,
                        mons->spell_slot_flags(SPELL_DIG));
        }
        else
            simple_monster_message(*mons, " falters for a moment.");
        mons->lose_energy(EUT_SPELL);
        return true;
    }
    else if ((burrows || flattens_trees)
                && good_move[delta.x + 1][delta.y + 1] == true)
    {
        const coord_def target(mons->pos() + delta);
        destroy_wall(target);

        if (flattens_trees)
        {
            // Flattening trees has a movement cost to the monster
            // - 100% over and above its normal move cost.
            _swim_or_move_energy(*mons);
            if (you.see_cell(target))
            {
                const bool actor_visible = you.can_see(*mons);
                mprf("%s knocks down a tree!",
                        actor_visible?
                        mons->name(DESC_THE).c_str() : "Something");
                noisy(25, target);
            }
            else
                noisy(25, target, "You hear a crashing sound.");
        }
        // Dissolution dissolves walls.
        else if (player_can_hear(mons->pos() + delta))
        {
            mprf(MSGCH_SOUND, mons->type == MONS_DISSOLUTION
                                ? "You hear a sizzling sound."
                                : "You hear a grinding noise.");
        }
    }

    bool ret = false;
    if (good_move[delta.x + 1][delta.y + 1] && !delta.origin())
    {
        // Check for attacking player.
        // XX is this actually reachable? this exact condition is already dealt
        // with in handle_monster_move
        if (mons->pos() + delta == you.pos())
        {
            ret = fight_melee(mons, &you);
            delta.reset();
        }

        // If we're following the player through stairs, the only valid
        // movement is towards the player. -- bwr
        if (testbits(mons->flags, MF_TAKING_STAIRS))
        {
            if (!player_stair_delay())
            {
                mons->flags &= ~MF_TAKING_STAIRS;

                dprf("BUG: %s was marked as follower when not following!",
                     mons->name(DESC_PLAIN).c_str());
            }
            else
            {
                ret    = true;
                delta.reset();

                dprf("%s is skipping movement in order to follow.",
                     mons->name(DESC_THE).c_str());
            }
        }

        // Check for attacking another monster.
        if (monster* targ = monster_at(mons->pos() + delta))
        {
            if ((mons_aligned(mons, targ) || targ->type == MONS_FOXFIRE)
                && !(mons->has_ench(ENCH_FRENZIED)
                     || mons->confused()))
            {
                ret = _monster_swaps_places(mons, delta);
            }
            else if (!delta.origin()) // confused self-hit handled below
            {
                fight_melee(mons, targ);
                ret = true;
            }

            // If the monster swapped places, the work's already done.
            delta.reset();
        }

        // The monster could die after a melee attack due to a mummy
        // death curse or something.
        if (!mons->alive())
            return true;

        if (mons_genus(mons->type) == MONS_EFREET
            || mons->type == MONS_FIRE_ELEMENTAL)
        {
            place_cloud(CLOUD_FIRE, mons->pos(), 2 + random2(4), mons);
        }

        if (mons->has_ench(ENCH_ROLLING))
            place_cloud(CLOUD_DUST, mons->pos(), 2, mons);

        if (mons->type == MONS_BALL_LIGHTNING)
            place_cloud(CLOUD_ELECTRICITY, mons->pos(), random_range(2, 3), mons);

        if (mons->type == MONS_FOXFIRE)
            check_place_cloud(CLOUD_FLAME, mons->pos(), 2, mons);

        if (mons->type == MONS_CURSE_TOE)
            place_cloud(CLOUD_MIASMA, mons->pos(), 2 + random2(3), mons);
    }
    else
    {
        monster* targ = monster_at(mons->pos() + delta);
        if (!delta.origin() && targ && _may_cutdown(mons, targ))
        {
            fight_melee(mons, targ);
            ret = true;
        }

        delta.reset();

        // Don't let boulder beetles roll in place.
        if (mons->has_ench(ENCH_ROLLING))
            mons->del_ench(ENCH_ROLLING);

        // Fleeing monsters that can't move will panic and possibly
        // turn to face their attacker.
        make_mons_stop_fleeing(mons);
    }

    // This handles the chance for the monster to hit itself.
    // n.b. this is reachable by hitting another monster and having delta.reset()
    // called. I'm taking this as a sort of intentional slapstick effect and
    // leaving it in place. (It's also reachable in a few other rare cases
    // where delta.reset() is called.)
    if (delta.x || delta.y || (mons->confused() && one_chance_in(6)))
        return _do_move_monster(*mons, delta);

    // Battlespheres need to preserve their tracking targets after each move
    if (mons_is_wandering(*mons)
        && mons->type != MONS_BATTLESPHERE)
    {
        // trigger a re-evaluation of our wander target on our next move -cao
        mons->target = mons->pos();
        if (!mons->is_patrolling() || mons->pacified())
        {
            mons->travel_target = MTRAV_NONE;
            mons->travel_path.clear();
            mons->patrol_point.reset();
        }
        mons->firing_pos.reset();
    }

    return ret;
}
