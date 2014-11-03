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
#include "butcher.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "dbg-scan.h"
#include "delay.h"
#include "dungeon.h"
#include "effects.h"
#include "evoke.h"
#include "fight.h"
#include "fineff.h"
#include "ghost.h"
#include "godpassive.h"
#include "godprayer.h"
#include "hints.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "los.h"
#include "mapmark.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-book.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mon-speak.h"
#include "mon-tentacle.h"
#include "religion.h"
#include "rot.h"
#include "shout.h"
#include "spl-book.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "throw.h"
#include "traps.h"
#include "viewchar.h"
#include "view.h"

static bool _handle_pickup(monster* mons);
static void _mons_in_cloud(monster* mons);
#if TAG_MAJOR_VERSION == 34
static void _heated_area(monster* mons);
#endif
static bool _monster_move(monster* mons);
static spell_type _map_wand_to_mspell(wand_type kind);

// [dshaligram] Doesn't need to be extern.
static coord_def mmov;

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

    // temp malmuts
    const mon_enchant wretched_ench = get_ench(ENCH_WRETCHED);
    // cap at 5 stacks for hd purposes
    const int wretchedness = min(wretched_ench.degree, 5);
    // -10% hd per wretched level (cap at -50%)
    const int wretched_hd = drained_hd * (10 - wretchedness) / 10;

    return max(wretched_hd, 1);
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
    coord_def(-1,-1), coord_def(0,-1), coord_def(1,-1), coord_def(1,0),
    coord_def(1, 1), coord_def(0, 1), coord_def(-1,1), coord_def(-1,0)
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
    if (crawl_state.disables[DIS_MON_REGEN])
        return;

    if (mons->has_ench(ENCH_SICK) || mons->has_ench(ENCH_DEATHS_DOOR) ||
        (!mons_can_regenerate(mons) && !(mons->has_ench(ENCH_REGENERATION))))
    {
        return;
    }

    // Non-land creatures out of their element cannot regenerate.
    if (mons_primary_habitat(mons) != HT_LAND
        && !monster_habitable_grid(mons, grd(mons->pos())))
    {
        return;
    }

    if (mons_class_fast_regen(mons->type)
        || mons->has_ench(ENCH_REGENERATION)
        || mons->has_ench(ENCH_WITHDRAWN)
        || _mons_natural_regen_roll(mons))
    {
        mons->heal(1);
    }
}

static void _escape_water_hold(monster* mons)
{
    if (mons->has_ench(ENCH_WATER_HOLD))
    {
        if (mons_habitat(mons) != HT_AMPHIBIOUS
            && mons_habitat(mons) != HT_WATER)
        {
            mons->speed_increment -= 5;
        }
        simple_monster_message(mons, " pulls free of the water.");
        mons->del_ench(ENCH_WATER_HOLD);
    }
}

static void _handle_manticore_barbs(monster* mons)
{
    if (mons->has_ench(ENCH_BARBS))
    {
        mon_enchant barbs = mons->get_ench(ENCH_BARBS);

        // Save these first because hurt() might kill the monster.
        const coord_def pos = mons->pos();
        const monster_type type = mons->type;
        mons->hurt(monster_by_mid(barbs.source),
                   roll_dice(2, barbs.degree * 2 + 2));
        bleed_onto_floor(pos, type, 2, false);
        if (coinflip())
        {
            barbs.duration--;
            mons->update_ench(barbs);
        }
    }
}

static bool _swap_monsters(monster* mover, monster* moved)
{
    // Can't swap with a stationary monster.
    // Although nominally stationary kraken tentacles can be swapped
    // with the main body.
    if (moved->is_stationary() && !moved->is_child_tentacle())
        return false;

    // If the target monster is constricted it is stuck
    // and not eligible to be swapped with
    if (moved->is_constricted())
    {
        dprf("%s fails to swap with %s, constricted.",
            mover->name(DESC_THE).c_str(),
            moved->name(DESC_THE).c_str());
            return false;
    }

    // Swapping is a purposeful action.
    if (mover->confused())
        return false;

    // Right now just happens in sanctuary.
    if (!is_sanctuary(mover->pos()) || !is_sanctuary(moved->pos()))
        return false;

    // A friendly or good-neutral monster moving past a fleeing hostile
    // or neutral monster, or vice versa.
    if (mover->wont_attack() == moved->wont_attack()
        || mons_is_retreating(mover) == mons_is_retreating(moved))
    {
        return false;
    }

    // Don't swap places if the player explicitly ordered their pet to
    // attack monsters.
    if ((mover->friendly() || moved->friendly())
        && you.pet_target != MHITYOU && you.pet_target != MHITNOT)
    {
        return false;
    }

    if (!mover->can_pass_through(moved->pos())
        || !moved->can_pass_through(mover->pos()))
    {
        return false;
    }

    if (!monster_habitable_grid(mover, grd(moved->pos()))
            && !mover->can_cling_to(moved->pos())
        || !monster_habitable_grid(moved, grd(mover->pos()))
            && !moved->can_cling_to(mover->pos()))
    {
        return false;
    }

    // Okay, we can do the swap.
    const coord_def mover_pos = mover->pos();
    const coord_def moved_pos = moved->pos();

    mover->set_position(moved_pos);
    moved->set_position(mover_pos);

    mover->clear_far_constrictions();
    moved->clear_far_constrictions();

    mover->check_clinging(true);
    moved->check_clinging(true);

    mgrd(mover->pos()) = mover->mindex();
    mgrd(moved->pos()) = moved->mindex();

    if (you.can_see(mover) && you.can_see(moved))
    {
        mprf("%s and %s swap places.", mover->name(DESC_THE).c_str(),
             moved->name(DESC_THE).c_str());
    }

    _escape_water_hold(mover);

    _handle_manticore_barbs(mover);
    _handle_manticore_barbs(moved);

    return true;
}

static bool _do_mon_spell(monster* mons, bolt &beem)
{
    // Shapeshifters don't get spells.
    if (!mons->is_shapeshifter() || !mons->is_actual_spellcaster())
    {
        if (handle_mon_spell(mons, beem))
        {
            // If a Pan lord/pghost is known to be a spellcaster, it's safer
            // to assume it has ranged spells too.  For others, it'd just
            // lead to unnecessary false positives.
            if (mons_is_ghost_demon(mons->type))
                mons->flags |= MF_SEEN_RANGED;

            mmov.reset();
            return true;
        }
    }

    return false;
}

static void _swim_or_move_energy(monster* mon, bool diag = false)
{
    const dungeon_feature_type feat = grd(mon->pos());

    // FIXME: Replace check with mons_is_swimming()?
    mon->lose_energy(((feat_is_lava(feat) || feat_is_water(feat))
                      && mon->ground_level()) ? EUT_SWIM : EUT_MOVE,
                      diag ? 10 : 1, diag ? 14 : 1);
}

static bool _unfriendly_or_insane(const monster* mon)
{
    return !mon->wont_attack() || mon->has_ench(ENCH_INSANE);
}

// Check up to eight grids in the given direction for whether there's a
// monster of the same alignment as the given monster that happens to
// have a ranged attack. If this is true for the first monster encountered,
// returns true. Otherwise returns false.
static bool _ranged_allied_monster_in_dir(monster* mon, coord_def p)
{
    coord_def pos = mon->pos();

    for (int i = 1; i <= LOS_RADIUS; i++)
    {
        pos += p;
        if (!in_bounds(pos))
            break;

        const monster* ally = monster_at(pos);
        if (ally == NULL)
            continue;

        if (mons_aligned(mon, ally))
        {
            // Hostile monsters of normal intelligence only move aside for
            // monsters of the same type.
            if (mons_intel(mon) <= I_NORMAL
                && _unfriendly_or_insane(mon)
                && mons_genus(mon->type) != mons_genus(ally->type))
            {
                return false;
            }

            if (mons_has_ranged_attack(ally))
                return true;
        }
        break;
    }
    return false;
}

// Check whether there's a monster of the same type and alignment adjacent
// to the given monster in at least one of three given directions (relative to
// the monster position).
static bool _allied_monster_at(monster* mon, coord_def a, coord_def b,
                               coord_def c)
{
    vector<coord_def> pos;
    pos.push_back(mon->pos() + a);
    pos.push_back(mon->pos() + b);
    pos.push_back(mon->pos() + c);

    for (unsigned int i = 0; i < pos.size(); i++)
    {
        if (!in_bounds(pos[i]))
            continue;

        const monster* ally = monster_at(pos[i]);
        if (ally == NULL)
            continue;

        if (ally->is_stationary() || ally->reach_range() > REACH_NONE)
            continue;

        // Hostile monsters of normal intelligence only move aside for
        // monsters of the same genus.
        if (mons_intel(mon) <= I_NORMAL
            && _unfriendly_or_insane(mon)
            && mons_genus(mon->type) != mons_genus(ally->type))
        {
            continue;
        }

        if (mons_aligned(mon, ally))
            return true;
    }

    return false;
}

// Altars as well as branch entrances are considered interesting for
// some monster types.
static bool _mon_on_interesting_grid(monster* mon)
{
    // Patrolling shouldn't happen all the time.
    if (one_chance_in(4))
        return false;

    const dungeon_feature_type feat = grd(mon->pos());

    switch (feat)
    {
    // Holy beings will tend to patrol around altars to the good gods.
    case DNGN_ALTAR_ELYVILON:
        if (!one_chance_in(3))
            return false;
        // else fall through
    case DNGN_ALTAR_ZIN:
    case DNGN_ALTAR_SHINING_ONE:
        return mon->is_holy();

    // Orcs will tend to patrol around altars to Beogh, and guard the
    // stairway from and to the Orcish Mines.
    case DNGN_ALTAR_BEOGH:
    case DNGN_ENTER_ORC:
    case DNGN_RETURN_FROM_ORC:
        return mons_is_native_in_branch(mon, BRANCH_ORC);

    // Same for elves and the Elven Halls.
    case DNGN_ENTER_ELF:
    case DNGN_RETURN_FROM_ELF:
        return mons_is_native_in_branch(mon, BRANCH_ELF);

    // Spiders...
    case DNGN_ENTER_SPIDER:
        return mons_is_native_in_branch(mon, BRANCH_SPIDER);

    default:
        return false;
    }
}

// If a hostile monster finds itself on a grid of an "interesting" feature,
// while unoccupied, it will remain in that area, and try to return to it
// if it left it for fighting, seeking etc.
static void _maybe_set_patrol_route(monster* mons)
{
    if (mons_is_wandering(mons)
        && !mons->friendly()
        && !mons->is_patrolling()
        && _mon_on_interesting_grid(mons))
    {
        mons->patrol_point = mons->pos();
    }
}

static bool _mons_can_cast_dig(const monster* mons, bool random)
{
    if (mons->foe == MHITNOT || !mons->has_spell(SPELL_DIG) || mons->confused())
        return false;

    const bool antimagiced = mons->has_ench(ENCH_ANTIMAGIC)
                      && (random
                          && !x_chance_in_y(4 * BASELINE_DELAY,
                                            4 * BASELINE_DELAY
                                            + mons->get_ench(ENCH_ANTIMAGIC).duration)
                      || (!random
                          && mons->get_ench(ENCH_ANTIMAGIC).duration
                             >= 4 * BASELINE_DELAY));
    const unsigned int flags = mons->spell_slot_flags(SPELL_DIG);
    return !(antimagiced && flags & MON_SPELL_ANTIMAGIC_MASK)
            && !(mons->is_silenced() && flags & MON_SPELL_SILENCE_MASK);
}

static bool _mons_can_zap_dig(const monster* mons)
{
    return mons->foe != MHITNOT
           && !mons->asleep()
           && !mons->confused() // they don't get here anyway
           && !mons->submerged()
           && mons_itemuse(mons) >= MONUSE_STARTING_EQUIPMENT
           && mons->inv[MSLOT_WAND] != NON_ITEM
           && mitm[mons->inv[MSLOT_WAND]].base_type == OBJ_WANDS
           && mitm[mons->inv[MSLOT_WAND]].sub_type == WAND_DIGGING
           && mitm[mons->inv[MSLOT_WAND]].plus > 0;
}

static void _set_mons_move_dir(const monster* mons,
                               coord_def* dir, coord_def* delta)
{
    ASSERT(dir);
    ASSERT(delta);

    // Some calculations.
    if ((mons_class_flag(mons->type, M_BURROWS)
         || _mons_can_cast_dig(mons, false))
        && mons->foe == MHITYOU)
    {
        // Boring beetles always move in a straight line in your
        // direction.
        *delta = you.pos() - mons->pos();
    }
    else
    {
        *delta = (mons->firing_pos.zero() ? mons->target : mons->firing_pos)
                 - mons->pos();
    }

    // Move the monster.
    *dir = delta->sgn();

    if (mons_is_retreating(mons)
        && (!mons->friendly() || mons->target != you.pos()))
    {
        *dir *= -1;
    }
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

// This only tracks movement, not whether hitting an
// adjacent monster is a possible move.
bool mons_can_move_towards_target(const monster* mon)
{
    coord_def mov, delta;
    _set_mons_move_dir(mon, &mov, &delta);

    move_array good_move;
    _fill_good_move(mon, &good_move);

    int dir = _compass_idx(mov);
    for (int i = -1; i <= 1; ++i)
    {
        const int altdir = (dir + i + 8) % 8;
        const coord_def p = mon_compass[altdir] + coord_def(1, 1);
        if (good_move(p))
            return true;
    }

    return false;
}

//---------------------------------------------------------------
//
// handle_movement
//
// Move the monster closer to its target square.
//
//---------------------------------------------------------------
static void _handle_movement(monster* mons)
{
    _maybe_set_patrol_route(mons);

    // Monsters will try to flee out of a sanctuary.
    if (is_sanctuary(mons->pos())
        && mons_is_influenced_by_sanctuary(mons)
        && !mons_is_fleeing_sanctuary(mons))
    {
        mons_start_fleeing_from_sanctuary(mons);
    }
    else if (mons_is_fleeing_sanctuary(mons)
             && !is_sanctuary(mons->pos()))
    {
        // Once outside there's a chance they'll regain their courage.
        // Nonliving and berserking monsters always stop immediately,
        // since they're only being forced out rather than actually
        // scared.
        if (mons->holiness() == MH_NONLIVING
            || mons->berserk()
            || mons->has_ench(ENCH_INSANE)
            || x_chance_in_y(2, 5))
        {
            mons_stop_fleeing_from_sanctuary(mons);
        }
    }

    coord_def delta;
    _set_mons_move_dir(mons, &mmov, &delta);

    // Don't allow monsters to enter a sanctuary or attack you inside a
    // sanctuary, even if you're right next to them.
    if (is_sanctuary(mons->pos() + mmov)
        && (!is_sanctuary(mons->pos())
            || mons->pos() + mmov == you.pos()))
    {
        mmov.reset();
    }

    // Bounds check: don't let fleeing monsters try to run off the grid.
    const coord_def s = mons->pos() + mmov;
    if (!in_bounds_x(s.x))
        mmov.x = 0;
    if (!in_bounds_y(s.y))
        mmov.y = 0;

    if (delta.rdist() > 3)
    {
        // Reproduced here is some semi-legacy code that makes monsters
        // move somewhat randomly along oblique paths.  It is an
        // exceedingly good idea, given crawl's unique line of sight
        // properties.
        //
        // Added a check so that oblique movement paths aren't used when
        // close to the target square. -- bwr

        // Sometimes we'll just move parallel the x axis.
        if (abs(delta.x) > abs(delta.y) && coinflip())
            mmov.y = 0;

        // Sometimes we'll just move parallel the y axis.
        if (abs(delta.y) > abs(delta.x) && coinflip())
            mmov.x = 0;
    }

    // Now quit if we can't move.
    if (mmov.origin())
        return;

    const coord_def newpos(mons->pos() + mmov);

    move_array good_move;
    _fill_good_move(mons, &good_move);

    // If the monster is moving in your direction, whether to attack or
    // protect you, or towards a monster it intends to attack, check
    // whether we first need to take a step to the side to make sure the
    // reinforcement can follow through. Only do this with 50% chance,
    // though, so it's not completely predictable.

    // First, check whether the monster is smart enough to even consider
    // this.
    if ((newpos == you.pos()
           || monster_at(newpos) && mons->foe == mgrd(newpos))
        && mons_intel(mons) >= I_ANIMAL
        && coinflip()
        && !mons_is_confused(mons) && !mons->caught()
        && !mons->berserk_or_insane())
    {
        // If the monster is moving parallel to the x or y axis, check
        // whether
        //
        // a) the neighbouring grids are blocked
        // b) there are other unblocked grids adjacent to the target
        // c) there's at least one allied monster waiting behind us.
        //
        // (For really smart monsters, also check whether there's a
        // monster farther back in the corridor that has some kind of
        // ranged attack.)
        if (mmov.y == 0)
        {
            if (!good_move[1][0] && !good_move[1][2]
                && (good_move[mmov.x+1][0] || good_move[mmov.x+1][2])
                && (_allied_monster_at(mons, coord_def(-mmov.x, -1),
                                       coord_def(-mmov.x, 0),
                                       coord_def(-mmov.x, 1))
                    || mons_intel(mons) >= I_NORMAL
                       && _unfriendly_or_insane(mons)
                       && _ranged_allied_monster_in_dir(mons,
                                                        coord_def(-mmov.x, 0))))
            {
                if (good_move[mmov.x+1][0])
                    mmov.y = -1;
                if (good_move[mmov.x+1][2] && (mmov.y == 0 || coinflip()))
                    mmov.y = 1;
            }
        }
        else if (mmov.x == 0)
        {
            if (!good_move[0][1] && !good_move[2][1]
                && (good_move[0][mmov.y+1] || good_move[2][mmov.y+1])
                && (_allied_monster_at(mons, coord_def(-1, -mmov.y),
                                       coord_def(0, -mmov.y),
                                       coord_def(1, -mmov.y))
                    || mons_intel(mons) >= I_NORMAL
                       && _unfriendly_or_insane(mons)
                       && _ranged_allied_monster_in_dir(mons,
                                                        coord_def(0, -mmov.y))))
            {
                if (good_move[0][mmov.y+1])
                    mmov.x = -1;
                if (good_move[2][mmov.y+1] && (mmov.x == 0 || coinflip()))
                    mmov.x = 1;
            }
        }
        else // We're moving diagonally.
        {
            if (good_move[mmov.x+1][1])
            {
                if (!good_move[1][mmov.y+1]
                    && (_allied_monster_at(mons, coord_def(-mmov.x, -1),
                                           coord_def(-mmov.x, 0),
                                           coord_def(-mmov.x, 1))
                        || mons_intel(mons) >= I_NORMAL
                           && _unfriendly_or_insane(mons)
                           && _ranged_allied_monster_in_dir(mons,
                                                coord_def(-mmov.x, -mmov.y))))
                {
                    mmov.y = 0;
                }
            }
            else if (good_move[1][mmov.y+1]
                     && (_allied_monster_at(mons, coord_def(-1, -mmov.y),
                                            coord_def(0, -mmov.y),
                                            coord_def(1, -mmov.y))
                         || mons_intel(mons) >= I_NORMAL
                            && _unfriendly_or_insane(mons)
                            && _ranged_allied_monster_in_dir(mons,
                                                coord_def(-mmov.x, -mmov.y))))
            {
                mmov.x = 0;
            }
        }
    }

    // Now quit if we can't move.
    if (mmov.origin())
        return;

    // Try to stay in sight of the player if we're moving towards
    // him/her, in order to avoid the monster coming into view,
    // shouting, and then taking a step in a path to the player which
    // temporarily takes it out of view, which can lead to the player
    // getting "comes into view" and shout messages with no monster in
    // view.

    // Doesn't matter for arena mode.
    if (crawl_state.game_is_arena())
        return;

    // Did we just come into view?
    // TODO: This doesn't seem to work right. Fix, or remove?

    if (mons->seen_context != SC_JUST_SEEN)
        return;
    if (testbits(mons->flags, MF_WAS_IN_VIEW))
        return;

    const coord_def old_pos  = mons->pos();
    const int       old_dist = grid_distance(you.pos(), old_pos);

    // We're already staying in the player's LOS.
    if (you.see_cell(old_pos + mmov))
        return;

    // We're not moving towards the player.
    if (grid_distance(you.pos(), old_pos + mmov) >= old_dist)
    {
        // Instead of moving out of view, we stay put.
        if (you.see_cell(old_pos))
            mmov.reset();
        return;
    }

    // Try to find a move that brings us closer to the player while
    // keeping us in view.
    int matches = 0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
        {
            if (i == 0 && j == 0)
                continue;

            if (!good_move[i][j])
                continue;

            coord_def d(i - 1, j - 1);
            coord_def tmp = old_pos + d;

            if (grid_distance(you.pos(), tmp) < old_dist && you.see_cell(tmp))
            {
                if (one_chance_in(++matches))
                    mmov = d;
                break;
            }
        }

    // We haven't been able to find a visible cell to move to. If previous
    // position was visible, we stay put.
    if (you.see_cell(old_pos) && !you.see_cell(old_pos + mmov))
        mmov.reset();
}

//---------------------------------------------------------------
//
// _handle_potion
//
// Give the monster a chance to quaff a potion. Returns true if
// the monster imbibed.
//
//---------------------------------------------------------------
static bool _handle_potion(monster* mons, bolt & beem)
{
    if (mons->asleep()
        || mons->inv[MSLOT_POTION] == NON_ITEM
        || !one_chance_in(3))
    {
        return false;
    }

    if (mons_itemuse(mons) < MONUSE_STARTING_EQUIPMENT)
        return false;

    // Make sure the item actually is a potion.
    if (mitm[mons->inv[MSLOT_POTION]].base_type != OBJ_POTIONS)
        return false;

    bool rc = false;

    const int potion_idx = mons->inv[MSLOT_POTION];
    item_def& potion = mitm[potion_idx];
    const potion_type ptype = static_cast<potion_type>(potion.sub_type);

    if (mons->can_drink_potion(ptype) && mons->should_drink_potion(ptype))
    {
        const bool was_visible = you.can_see(mons);

        // XXX: this is mostly to prevent a funny message order:
        // "$foo drinks a potion. $foo wields a great mace. $foo goes berserk!"
        if (ptype == POT_BERSERK_RAGE)
            mons->wield_melee_weapon();

        // Drink the potion.
        const item_type_id_state_type id = mons->drink_potion_effect(ptype);

        // Give ID if necessary.
        if (was_visible && id != ID_UNKNOWN_TYPE)
        {
            set_ident_type(OBJ_POTIONS, ptype, id);
            identify_healing_pots();
        }

        // Remove the oldest blood timer.
        if (is_blood_potion(potion))
            remove_oldest_perishable_item(potion);

        // Remove it from inventory.
        if (dec_mitm_item_quantity(potion_idx, 1))
            mons->inv[MSLOT_POTION] = NON_ITEM;

        mons->lose_energy(EUT_ITEM);
        rc = true;
    }

    return rc;
}

static bool _handle_evoke_equipment(monster* mons, bolt & beem)
{
    // TODO: check non-ring, non-amulet equipment
    if (mons->asleep()
        || mons->inv[MSLOT_JEWELLERY] == NON_ITEM
        || !one_chance_in(3))
    {
        return false;
    }

    if (mons_itemuse(mons) < MONUSE_STARTING_EQUIPMENT)
        return false;

    // Make sure the item actually is a ring or amulet.
    if (mitm[mons->inv[MSLOT_JEWELLERY]].base_type != OBJ_JEWELLERY)
        return false;

    bool rc = false;

    const int jewellery_idx = mons->inv[MSLOT_JEWELLERY];
    item_def& jewellery = mitm[jewellery_idx];
    const jewellery_type jtype =
        static_cast<jewellery_type>(jewellery.sub_type);

    if (mons->can_evoke_jewellery(jtype) &&
        mons->should_evoke_jewellery(jtype))
    {
        const bool was_visible = you.can_see(mons);

        // Drink the potion.
        const item_type_id_state_type id = mons->evoke_jewellery_effect(jtype);

        // Give ID if necessary.
        if (was_visible && id != ID_UNKNOWN_TYPE)
            set_ident_type(OBJ_JEWELLERY, jtype, id);

        mons->lose_energy(EUT_ITEM);
        rc = true;
    }

    return rc;
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
static bool _handle_reaching(monster* mons)
{
    bool       ret = false;
    const reach_type range = mons->reach_range();
    actor *foe = mons->get_foe();

    if (mons->caught())
        return false;

    if (!foe || range <= REACH_NONE)
        return false;

    if (is_sanctuary(mons->pos()) || is_sanctuary(foe->pos()))
        return false;

    if (mons->submerged())
        return false;

    if (mons_aligned(mons, foe) && !mons->has_ench(ENCH_INSANE))
        return false;

    // Don't stop to jab at things while fleeing or leaving the level
    if ((mons_is_fleeing(mons) || mons->pacified()))
        return false;

    const coord_def foepos(foe->pos());
    const coord_def delta(foepos - mons->pos());
    const int grid_distance(delta.rdist());
    const coord_def first_middle(mons->pos() + delta / 2);
    const coord_def second_middle(foepos - delta / 2);

    if (grid_distance == 2
        // The monster has to be attacking the correct position.
        && mons->target == foepos
        // With a reaching attack with a large enough range:
        && delta.abs() <= range
        // And with no dungeon furniture in the way of the reaching
        // attack;
        && (feat_is_reachable_past(grd(first_middle))
            || feat_is_reachable_past(grd(second_middle)))
        // The foe should be on the map (not stepped from time).
        && in_bounds(foepos))
    {
        ret = true;

        ASSERT(foe->is_player() || foe->is_monster());

        fight_melee(mons, foe);
    }

    return ret;
}

//---------------------------------------------------------------
//
// handle_scroll
//
// Give the monster a chance to read a scroll. Returns true if
// the monster read something.
//
//---------------------------------------------------------------
static bool _handle_scroll(monster* mons)
{
    // Yes, there is a logic to this ordering {dlb}:
    if (mons->asleep()
        || mons_is_confused(mons)
        || mons->submerged()
        || mons->inv[MSLOT_SCROLL] == NON_ITEM
        || mons->has_ench(ENCH_BLIND)
        || !one_chance_in(3))
    {
        return false;
    }

    if (mons_itemuse(mons) < MONUSE_STARTING_EQUIPMENT)
        return false;

    if (silenced(mons->pos()))
        return false;

    // Make sure the item actually is a scroll.
    if (mitm[mons->inv[MSLOT_SCROLL]].base_type != OBJ_SCROLLS)
        return false;

    bool                    read        = false;
    item_type_id_state_type ident       = ID_UNKNOWN_TYPE;
    bool                    was_visible = you.can_see(mons);

    // Notice how few cases are actually accounted for here {dlb}:
    const int scroll_type = mitm[mons->inv[MSLOT_SCROLL]].sub_type;
    switch (scroll_type)
    {
    case SCR_TELEPORTATION:
        if (!mons->has_ench(ENCH_TP) && !mons->no_tele(true, false))
        {
            if (mons->caught() || mons_is_fleeing(mons) || mons->pacified())
            {
                simple_monster_message(mons, " reads a scroll.");
                read = true;
                ident = ID_KNOWN_TYPE;
                monster_teleport(mons, false);
            }
        }
        break;

    case SCR_BLINKING:
        if ((mons->caught() || mons_is_fleeing(mons) || mons->pacified())
            && mons_near(mons) && !mons->no_tele(true, false))
        {
            simple_monster_message(mons, " reads a scroll.");
            read = true;
            if (mons->caught())
            {
                ident = ID_KNOWN_TYPE;
                monster_blink(mons);
            }
            else if (blink_away(mons))
                ident = ID_KNOWN_TYPE;
        }
        break;

    case SCR_SUMMONING:
        if (mons_near(mons))
        {
            simple_monster_message(mons, " reads a scroll.");
            mprf("Wisps of shadow swirl around %s.", mons->name(DESC_THE).c_str());
            read = true;
            int count = roll_dice(2, 2);
            for (int i = 0; i < count; ++i)
            {
                create_monster(
                    mgen_data(RANDOM_MOBILE_MONSTER, SAME_ATTITUDE(mons), mons,
                              3, MON_SUMM_SCROLL, mons->pos(), mons->foe,
                              0, GOD_NO_GOD));
            }
            ident = ID_KNOWN_TYPE;
        }
        break;
    }

    if (read)
    {
        if (dec_mitm_item_quantity(mons->inv[MSLOT_SCROLL], 1))
            mons->inv[MSLOT_SCROLL] = NON_ITEM;

        if (ident != ID_UNKNOWN_TYPE && was_visible)
            set_ident_type(OBJ_SCROLLS, scroll_type, ident);

        mons->lose_energy(EUT_ITEM);
    }

    return read;
}

static int _generate_rod_power(monster *mons, int overriding_power = 0)
{
    // power is actually 5 + Evocations + 2d(Evocations)
    // modified by shield and shield skill
    int shield_num = 1;
    int shield_den = 1;
    int shield_base = 1;

    if (mons->inv[MSLOT_SHIELD] != NON_ITEM)
    {
        item_def *shield = mons->mslot_item(MSLOT_SHIELD);
        switch (shield->sub_type)
        {
        case ARM_BUCKLER:
            shield_base += 4;
            break;
        case ARM_SHIELD:
            shield_base += 2;
            break;
        case ARM_LARGE_SHIELD:
            shield_base++;
            break;
        default:
            break;
        }
    }

    const int power_base = mons->skill(SK_EVOCATIONS);
    int power            = 5 + power_base + (2 * random2(power_base));

    if (shield_base > 1)
    {
        const int shield_mod = ((power / shield_base) * shield_num) / shield_den;
        power -= shield_mod;
    }

    if (overriding_power > 0)
        power = overriding_power;

    return power;
}

static bolt& _generate_item_beem(bolt &beem, bolt& from, monster* mons)
{
    beem.name         = from.name;
    beem.source_id    = mons->mid;
    beem.source       = mons->pos();
    beem.colour       = from.colour;
    beem.range        = from.range;
    beem.damage       = from.damage;
    beem.ench_power   = from.ench_power;
    beem.hit          = from.hit;
    beem.glyph        = from.glyph;
    beem.flavour      = from.flavour;
    beem.thrower      = from.thrower;
    beem.is_beam      = from.is_beam;
    beem.is_explosion = from.is_explosion;
    return beem;
}

static bool _setup_wand_beam(bolt& beem, monster* mons)
{
    item_def &wand(mitm[mons->inv[MSLOT_WAND]]);

    // map wand type to monster spell type
    const spell_type mzap = _map_wand_to_mspell((wand_type)wand.sub_type);
    if (mzap == SPELL_NO_SPELL)
        return false;

    // set up the beam
    int power         = 30 + mons->get_hit_dice();
    bolt theBeam      = mons_spell_beam(mons, mzap, power);
    beem = _generate_item_beem(beem, theBeam, mons);

    beem.aux_source =
        wand.name(DESC_QUALNAME, false, true, false, false);

    return true;
}

static void _mons_fire_wand(monster* mons, item_def &wand, bolt &beem,
                            bool was_visible, bool niceWand)
{
    if (!simple_monster_message(mons, " zaps a wand."))
    {
        if (!silenced(you.pos()))
            mprf(MSGCH_SOUND, "You hear a zap.");
    }

    // charge expenditure {dlb}
    wand.plus--;
    beem.is_tracer = false;
    beem.fire();

    if (was_visible)
    {
        const int wand_type = wand.sub_type;

        set_ident_type(OBJ_WANDS, wand_type, ID_KNOWN_TYPE);
        if (!mons->props["wand_known"].get_bool())
        {
            mprf("It is %s.", wand.name(DESC_A).c_str());
            mons->props["wand_known"] = true;
        }

        // Increment zap count.
        if (wand.used_count >= 0)
            wand.used_count++;

        mons->flags |= MF_SEEN_RANGED;
    }

    mons->lose_energy(EUT_ITEM);
}

static void _rod_fired_pre(monster* mons)
{
    make_mons_stop_fleeing(mons);

    if (!simple_monster_message(mons, " zaps a rod.")
        && !silenced(you.pos()))
    {
        mprf(MSGCH_SOUND, "You hear a zap.");
    }
}

static bool _rod_fired_post(monster* mons, item_def &rod, int idx, bolt &beem,
    int rate, bool was_visible)
{
    rod.plus -= rate;
    dprf("rod charge: %d, %d", rod.charges, rod.charge_cap);

    if (was_visible)
    {
        if (!beem.is_enchantment() || beem.obvious_effect)
            set_ident_flags(rod, ISFLAG_KNOW_TYPE);
    }

    mons->lose_energy(EUT_ITEM);
    return true;
}

static bool _thunderbolt_tracer(monster *caster, int pow, coord_def aim)
{
    coord_def prev;
    if (caster->props.exists("thunderbolt_last")
        && caster->props["thunderbolt_last"].get_int() + 1 == you.num_turns)
    {
        prev = caster->props["thunderbolt_aim"].get_coord();
    }

    targetter_thunderbolt hitfunc(caster, spell_range(SPELL_THUNDERBOLT, pow),
                                  prev);
    hitfunc.set_aim(aim);

    mon_attitude_type castatt = caster->temp_attitude();
    int friendly = 0, enemy = 0;

    for (map<coord_def, aff_type>::const_iterator p = hitfunc.zapped.begin();
         p != hitfunc.zapped.end(); ++p)
    {
        if (p->second <= 0)
            continue;

        const actor *victim = actor_at(p->first);
        if (!victim)
            continue;

        int dam = 4 >> victim->res_elec();
        if (mons_atts_aligned(castatt, victim->temp_attitude()))
            friendly += dam;
        else
            enemy += dam;
    }

    return enemy > friendly;

    return false;
}

// handle_rod
// -- implemented as a dependent to handle_wand currently
// (no wand + rod turns this way)
// notes:
// shamelessly repurposing handle_wand code
// not one word about the name of this function!
static bool _handle_rod(monster *mons, bolt &beem)
{
    const int weapon = mons->inv[MSLOT_WEAPON];
    item_def &rod(mitm[weapon]);

    // Make sure the item actually is a rod.
    ASSERT(rod.base_type == OBJ_RODS);

    // was the player visible when we started?
    bool was_visible = you.can_see(mons);

    spell_type mzap = spell_in_rod(rod.sub_type);
    int rate        = spell_difficulty(mzap) * ROD_CHARGE_MULT;

    if (rod.plus < rate)
        return false;

    // XXX: There should be a better way to do this than hardcoding
    // monster-castable rod spells!
    switch (mzap)
    {
    case SPELL_BOLT_OF_INACCURACY:
    case SPELL_CLOUD_CONE:
    // Handled in mons_spell_beam
    case SPELL_RANDOM_BOLT:
        break;

    case SPELL_THUNDERBOLT:
        if (mons->props.exists("thunderbolt_last")
            && mons->props["thunderbolt_last"].get_int() + 1 == you.num_turns)
        {
            rate = min(5 * ROD_CHARGE_MULT, (int)rod.plus);
            mons->props["thunderbolt_mana"].get_int() = rate;
        }
        break;

    case SPELL_SUMMON_SWARM:
    case SPELL_WEAVE_SHADOWS:
        _rod_fired_pre(mons);
        mons_cast(mons, beem, mzap, MON_SPELL_NO_FLAGS, false);
        _rod_fired_post(mons, rod, weapon, beem, rate, was_visible);
        return true;

       break;

    default:
        return false;
    }

    bool zap = false;

    // set up the beam
    const int power = max(_generate_rod_power(mons), 1);

    dprf("using rod with power %d", power);

    bolt theBeam;
    do
        theBeam = mons_spell_beam(mons, mzap, power, true);
    //XXX: this does fixed 3d20 by monsters, too nasty
    while (mzap == SPELL_RANDOM_BOLT
           && theBeam.origin_spell == SPELL_QUICKSILVER_BOLT);

    beem         = _generate_item_beem(beem, theBeam, mons);
    beem.aux_source =
        rod.name(DESC_QUALNAME, false, true, false, false);

    if (mons->confused())
    {
        beem.target = dgn_random_point_from(mons->pos(), LOS_RADIUS);
        if (beem.target.origin())
            return false;
        zap = true;
    }
    else if (mzap == SPELL_THUNDERBOLT)
        zap = _thunderbolt_tracer(mons, power, beem.target);
    else if (mzap == SPELL_CLOUD_CONE)
        zap = mons_should_cloud_cone(mons, power, beem.target);
    else
    {
        fire_tracer(mons, beem);
        zap = mons_should_fire(beem);
    }

    if (zap)
    {
        _rod_fired_pre(mons);
        if (mzap == SPELL_THUNDERBOLT)
            cast_thunderbolt(mons, power, beem.target);
        else if (mzap == SPELL_CLOUD_CONE)
            cast_cloud_cone(mons, power, beem.target);
        else
        {
            beem.is_tracer = false;
            beem.fire();
        }
        return _rod_fired_post(mons, rod, weapon, beem, rate, was_visible);
    }

    return false;
}

//---------------------------------------------------------------
//
// handle_wand
//
// Give the monster a chance to zap a wand or rod. Returns true
// if the monster zapped.
//
//---------------------------------------------------------------
static bool _handle_wand(monster* mons, bolt &beem)
{
    // Yes, there is a logic to this ordering {dlb}:
    // FIXME: monsters should be able to use wands or rods
    //        out of sight of the player [rob]
    if (!mons_near(mons)
        || mons->asleep()
        || mons->has_ench(ENCH_SUBMERGED)
        || coinflip())
    {
        return false;
    }

    if (mons_itemuse(mons) < MONUSE_STARTING_EQUIPMENT)
        return false;

    if (mons->inv[MSLOT_WEAPON] != NON_ITEM
        && mitm[mons->inv[MSLOT_WEAPON]].base_type == OBJ_RODS)
    {
        return _handle_rod(mons, beem);
    }

    if (mons->inv[MSLOT_WAND] == NON_ITEM
        || mitm[mons->inv[MSLOT_WAND]].plus <= 0)
    {
        return false;
    }

    // Make sure the item actually is a wand.
    if (mitm[mons->inv[MSLOT_WAND]].base_type != OBJ_WANDS)
        return false;

    bool niceWand    = false;
    bool zap         = false;
    bool was_visible = you.can_see(mons);

    if (!_setup_wand_beam(beem, mons))
        return false;

    item_def &wand = mitm[mons->inv[MSLOT_WAND]];

    const wand_type kind = (wand_type)wand.sub_type;
    switch (kind)
    {
    case WAND_DISINTEGRATION:
        // Dial down damage from wands of disintegration, since
        // disintegration beams can do large amounts of damage.
        beem.damage.size = beem.damage.size * 2 / 3;
        break;

    case WAND_ENSLAVEMENT:
    case WAND_RANDOM_EFFECTS:
        // These have been deemed "too tricky" at this time {dlb}:
        return false;

    case WAND_DIGGING:
        // This is handled elsewhere.
        return false;

    // These are wands that monsters will aim at themselves {dlb}:
    case WAND_HASTING:
        if (!mons->has_ench(ENCH_HASTE))
        {
            beem.target = mons->pos();
            niceWand = true;
            break;
        }
        return false;

    case WAND_HEAL_WOUNDS:
        if (mons->hit_points <= mons->max_hit_points / 2)
        {
            beem.target = mons->pos();
            niceWand = true;
            break;
        }
        return false;

    case WAND_INVISIBILITY:
        if (!mons->has_ench(ENCH_INVIS)
            && !mons->has_ench(ENCH_SUBMERGED)
            && !mons->glows_naturally()
            && (!mons->friendly() || you.can_see_invisible(false)))
        {
            beem.target = mons->pos();
            niceWand = true;
            break;
        }
        return false;

    case WAND_TELEPORTATION:
        if (mons->hit_points <= mons->max_hit_points / 2
            || mons->caught())
        {
            if (!mons->has_ench(ENCH_TP)
                && !one_chance_in(20))
            {
                beem.target = mons->pos();
                niceWand = true;
                break;
            }
            // This break causes the wand to be tried on the player.
            break;
        }
        return false;

    default:
        break;
    }

    if (mons->confused())
    {
        beem.target = dgn_random_point_from(mons->pos(), LOS_RADIUS);
        if (beem.target.origin())
            return false;
        zap = true;
    }
    else if (!niceWand)
    {
        // Fire tracer, if necessary.
        fire_tracer(mons, beem);

        // Good idea?
        zap = mons_should_fire(beem);
    }

    if (niceWand || zap)
    {
        if (!niceWand)
            make_mons_stop_fleeing(mons);

        _mons_fire_wand(mons, wand, beem, was_visible, niceWand);

        return true;
    }

    return false;
}

//---------------------------------------------------------------
//
// handle_throw
//
// Give the monster a chance to throw something. Returns true if
// the monster hurled.
//
//---------------------------------------------------------------
bool handle_throw(monster* mons, bolt & beem, bool teleport, bool check_only)
{
    // Yes, there is a logic to this ordering {dlb}:
    if (mons->incapacitated()
        || mons->submerged()
        || mons->caught())
    {
        return false;
    }

    if (mons_itemuse(mons) < MONUSE_STARTING_EQUIPMENT
        && mons->type != MONS_SPECTRAL_THING)
    {
        return false;
    }

    const bool archer = mons_class_flag(mons->type, M_DONT_MELEE);

    const bool liquefied = mons->liquefied_ground();

    // Don't allow offscreen throwing for now.
    if (mons->foe == MHITYOU && !mons_near(mons))
        return false;

    // Monsters won't shoot in melee range, largely for balance reasons.
    // Specialist archers are an exception to this rule.
    if (adjacent(beem.target, mons->pos()))
    {
        if (!archer)
            return false;
        // If adjacent, archers should always shoot (otherwise they would
        // try to melee). Hence the else if below.
    }
    else if (!teleport && ((liquefied && !archer && one_chance_in(9))
                           || (!liquefied && one_chance_in(archer ? 9 : 5))))
    {
        // Highly-specialised archers are more likely to shoot than talk.
        // If we're standing on liquefied ground, try to stand and fire!
        // (Particularly archers.)
        return false;
    }

    // Don't let fleeing (or pacified creatures) stop to shoot at things
    if (mons_is_fleeing(mons) || mons->pacified())
        return false;

    item_def *launcher = NULL;
    const item_def *weapon = NULL;
    const int mon_item = mons_usable_missile(mons, &launcher);

    if (mon_item == NON_ITEM || !mitm[mon_item].defined())
        return false;

    if (player_or_mon_in_sanct(mons))
        return false;

    item_def *missile = &mitm[mon_item];

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

    // If the attack needs a launcher that we can't wield, bail out.
    if (launcher)
    {
        weapon = mons->mslot_item(MSLOT_WEAPON);
        if (weapon && weapon != launcher && weapon->cursed())
            return false;
    }

    // Ok, we'll try it.
    setup_monster_throw_beam(mons, beem);

    // Set fake damage for the tracer.
    beem.damage = dice_def(10, 10);

    // Set item for tracer, even though it probably won't be used
    beem.item = missile;

    // Fire tracer.
    if (!teleport)
        fire_tracer(mons, beem);

    // Clear fake damage (will be set correctly in mons_throw).
    beem.damage = dice_def();

    // Good idea?
    if (teleport || mons_should_fire(beem))
    {
        if (check_only)
            return true;

        // Monsters shouldn't shoot if fleeing, so let them "turn to attack".
        make_mons_stop_fleeing(mons);

        if (launcher && launcher != weapon)
            mons->swap_weapons();

        beem.name.clear();
        return mons_throw(mons, beem, mon_item, teleport);
    }

    return false;
}

// Give the monster its action energy (aka speed_increment).
static void _monster_add_energy(monster* mons)
{
    if (mons->speed > 0)
    {
        // Randomise to make counting off monster moves harder:
        const int energy_gained =
            max(1, div_rand_round(mons->speed * you.time_taken, 10));
        mons->speed_increment += energy_gained;
    }
}

#ifdef DEBUG
#    define DEBUG_ENERGY_USE(problem) \
    if (mons->speed_increment == old_energy && mons->alive()) \
             mprf(MSGCH_DIAGNOSTICS, \
                  problem " for monster '%s' consumed no energy", \
                  mons->name(DESC_PLAIN).c_str());
#else
#    define DEBUG_ENERGY_USE(problem) ((void) 0)
#endif

static void _confused_move_dir(monster *mons)
{
    mmov.reset();
    int pfound = 0;
    for (adjacent_iterator ai(mons->pos(), false); ai; ++ai)
        if (mons->can_pass_through(*ai))
        {
            // Highly intelligent monsters don't move if they might drown.
            if (mons_intel(mons) == I_HIGH
                && !mons->is_habitable(*ai))
            {
                // Players without a spoiler sheet have no way to know which
                // monsters are I_HIGH, and this behaviour is obscure.
                // Thus, give a message.
                const string where = make_stringf("%s@%d,%d",
                    level_id::current().describe().c_str(),
                    mons->pos().x, mons->pos().y);
                if (!mons->props.exists("no_conf_move")
                    || mons->props["no_conf_move"].get_string() != where)
                {
                    // But don't spam.
                    mons->props["no_conf_move"] = where;
                    simple_monster_message(mons,
                        make_stringf(" stays still, afraid of the %s.",
                        feat_type_name(grd(*ai))).c_str());
                }
                mmov.reset();
                break;
            }
            else if (one_chance_in(++pfound))
                mmov = *ai - mons->pos();
        }
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

static void _pre_monster_move(monster* mons)
{
    mons->hit_points = min(mons->max_hit_points, mons->hit_points);

    if (mons->type == MONS_SPATIAL_MAELSTROM
        && !player_in_branch(BRANCH_ABYSS)
        && !player_in_branch(BRANCH_ZIGGURAT))
    {
        for (int i = 0; i < you.time_taken; ++i)
        {
            if (one_chance_in(100))
            {
                mons->banish(mons);
                return;
            }
        }
    }

    int sumtype = 0;
    if (mons->summoner && (mons->is_summoned(NULL, &sumtype)
                           || sumtype == MON_SUMM_CLONE))
    {
        const actor * const summoner = actor_by_mid(mons->summoner);
        if ((!summoner || !summoner->alive()) && mons->del_ench(ENCH_ABJ))
            return;
    }

    if (mons->type == MONS_SNAPLASHER_VINE
        && mons->props.exists("vine_awakener"))
    {
        monster* awakener = monster_by_mid(mons->props["vine_awakener"].get_int());
        if (awakener && !awakener->can_see(mons))
        {
            simple_monster_message(mons, " falls limply to the ground.");
            monster_die(mons, KILL_RESET, NON_MONSTER);
            return;
        }
    }

    // Dissipate player ball lightnings that have left the player's sight
    // (monsters are allowed to 'cheat', as with orb of destruction)
    if (mons->type == MONS_BALL_LIGHTNING && mons->summoner == MID_PLAYER
        && !cell_see_cell(you.pos(), mons->pos(), LOS_SOLID))
    {
        monster_die(mons, KILL_RESET, NON_MONSTER);
        return;
    }

    if (mons_stores_tracking_data(mons))
    {
        actor* foe = mons->get_foe();
        if (foe)
        {
            if (!mons->props.exists("foe_pos"))
                mons->props["foe_pos"].get_coord() = foe->pos();
            else
            {
                if (mons->props["foe_pos"].get_coord().distance_from(mons->pos())
                    > foe->pos().distance_from(mons->pos()))
                {
                    mons->props["foe_approaching"].get_bool() = true;
                }
                else
                    mons->props["foe_approaching"].get_bool() = false;

                mons->props["foe_pos"].get_coord() = foe->pos();
            }
        }
        else
            mons->props.erase("foe_pos");
    }

    reset_battlesphere(mons);
    reset_spectral_weapon(mons);

    fedhas_neutralise(mons);

    // Monster just summoned (or just took stairs), skip this action.
    if (testbits(mons->flags, MF_JUST_SUMMONED))
    {
        mons->flags &= ~MF_JUST_SUMMONED;
        return;
    }

    mon_acting mact(mons);

    _monster_add_energy(mons);

    // Handle clouds on nonmoving monsters.
    if (mons->speed == 0)
    {
        _mons_in_cloud(mons);

        // Update constriction durations
        mons->accum_has_constricted();

#if TAG_MAJOR_VERSION == 34
        _heated_area(mons);
#endif
        if (mons->type == MONS_NO_MONSTER)
            return;
    }

    // Apply monster enchantments once for every normal-speed
    // player turn.
    mons->ench_countdown -= you.time_taken;
    while (mons->ench_countdown < 0)
    {
        mons->ench_countdown += 10;
        mons->apply_enchantments();

        // If the monster *merely* died just break from the loop
        // rather than quit altogether, since we have to deal with
        // giant spores and ball lightning exploding at the end of the
        // function, but do return if the monster's data has been
        // reset, since then the monster type is invalid.
        if (mons->type == MONS_NO_MONSTER)
            return;
        else if (mons->hit_points < 1)
            break;
    }

    // Memory is decremented here for a reason -- we only want it
    // decrementing once per monster "move".
    if (mons->foe_memory > 0 && !you.penance[GOD_ASHENZARI])
        mons->foe_memory -= you.time_taken;

    // Otherwise there are potential problems with summonings.
    if (mons->type == MONS_GLOWING_SHAPESHIFTER)
        mons->add_ench(ENCH_GLOWING_SHAPESHIFTER);

    if (mons->type == MONS_SHAPESHIFTER)
        mons->add_ench(ENCH_SHAPESHIFTER);

    // We reset batty monsters from wander to seek here, instead
    // of in handle_behaviour() since that will be called with
    // every single movement, and we want these monsters to
    // hit and run. -- bwr
    if (mons->foe != MHITNOT && mons_is_wandering(mons)
        && mons_is_batty(mons))
    {
        mons->behaviour = BEH_SEEK;
    }

    mons->check_speed();
}

static void _grand_avatar_act(monster* mons)
{
    if (mons->props.exists(GA_TARGET_MID))
    {
        actor* target = actor_by_mid(mons->props[GA_TARGET_MID].get_int());
        if (!target)
        {
            grand_avatar_reset(mons);
            return;
        }
        mons->foe = target->mindex();
        if (mons->props.exists(GA_SPELL))
        {
            if (!mons->firing_pos.origin())
            {
                if (mons->pos() != mons->firing_pos)
                {
                    mons->target = mons->firing_pos;
                    return;
                }
                mons->firing_pos.reset();
            }
            bolt beam;
            beam.name        = "barrage of energy";
            beam.source_name = "grand avatar";
            beam.source      = mons->pos();
            beam.target      = target->pos();
            beam.range       = LOS_RADIUS;
            beam.damage      = dice_def(2, 10);
            beam.glyph       = dchar_glyph(DCHAR_FIRED_ZAP);
            beam.colour      = LIGHTGREEN;
            beam.flavour     = BEAM_MMISSILE;
            beam.hit         = AUTOMATIC_HIT;
            beam.is_beam     = false;
            beam.foe_ratio   = 0;
            fire_tracer(mons, beam);
            if (mons_should_fire(beam))
            {
                simple_monster_message(mons, " fires!");
                beam.fire();
                mons->lose_energy(EUT_SPECIAL);
                grand_avatar_reset(mons);
            }
            else
            {
                mons->target = target->pos();
                for (distance_iterator di(mons->pos(), true, true, 2); di; ++di)
                {
                    if (*di == beam.target
                        || actor_at(*di)
                        || cell_is_solid(*di))
                    {
                        continue;
                    }
                    beam.source = *di;
                    beam.friend_info.reset();
                    beam.foe_info.reset();
                    fire_tracer(mons, beam);
                    if (mons_should_fire(beam))
                    {
                        mons->firing_pos = *di;
                        mons->target = mons->firing_pos;
                        break;
                    }
                }
            }
        }
        else if (mons->props.exists(GA_MELEE))
            mons->target = target->pos();
    }
    else
    {
        grand_avatar_reset(mons);
        if (!mons->alive())
            return;
    }

    // Avatar may have dissipated after firing.
    if (!mons->alive())
        return;

    bolt tracer;
    tracer.source    = mons->pos();
    tracer.target    = mons->target;
    tracer.range     = LOS_RADIUS;
    tracer.hit       = AUTOMATIC_HIT;
    tracer.flavour   = BEAM_MMISSILE;
    tracer.is_tracer = true;
    fire_tracer(mons, tracer);

    if (tracer.path_taken.back() != mons->target)
    {
        coord_def near;
        if (find_habitable_spot_near(mons->target, mons->type, 2, false, near)
            && mons->blink_to(near))
        {
            mons->lose_energy(EUT_SPECIAL);
        }
    }
}

static void _maybe_submerge(monster* mons)
{
    if (mons->asleep() || mons->submerged())
        return;

    if (monster_can_submerge(mons, grd(mons->pos()))
        && !mons->caught()         // No submerging while caught.
        && !mons->asleep()         // No submerging when asleep.
        && !you.beheld_by(mons)    // No submerging if player entranced.
        && !mons_is_lurking(mons)  // Handled elsewhere.
        && mons->wants_submerge())
    {
        const monsterentry* entry = get_monster_data(mons->type);

        mons->add_ench(ENCH_SUBMERGED);
        mons->speed_increment -= ENERGY_SUBMERGE(entry);
        return;
    }

    if (mons->type == MONS_AIR_ELEMENTAL && one_chance_in(5))
    {
        // Takes no time.
        mons->add_ench(ENCH_SUBMERGED);
    }
}

void handle_monster_move(monster* mons)
{
    const monsterentry* entry = get_monster_data(mons->type);
    if (!entry)
        return;

    int old_energy      = mons->speed_increment;
    int non_move_energy = min(entry->energy_usage.move,
                              entry->energy_usage.swim);

#ifdef DEBUG_MONS_SCAN
    bool monster_was_floating = mgrd(mons->pos()) != mons->mindex();
#endif
    coord_def old_pos = mons->pos();

    if (!mons->has_action_energy())
        return;

    move_solo_tentacle(mons);

    if (!mons->alive())
        return;

    if (mons_is_tentacle_head(mons_base_type(mons)))
        move_child_tentacles(mons);

    old_pos = mons->pos();

#ifdef DEBUG_MONS_SCAN
    if (!monster_was_floating
        && mgrd(mons->pos()) != mons->mindex())
    {
        mprf(MSGCH_ERROR, "Monster %s became detached from mgrd "
                          "in handle_monster_move() loop",
             mons->name(DESC_PLAIN, true).c_str());
        mprf(MSGCH_WARN, "[[[[[[[[[[[[[[[[[[");
        debug_mons_scan();
        mprf(MSGCH_WARN, "]]]]]]]]]]]]]]]]]]");
        monster_was_floating = true;
    }
    else if (monster_was_floating
             && mgrd(mons->pos()) == mons->mindex())
    {
        mprf(MSGCH_DIAGNOSTICS, "Monster %s re-attached itself to mgrd "
                                "in handle_monster_move() loop",
             mons->name(DESC_PLAIN, true).c_str());
        monster_was_floating = false;
    }
#endif

    if (mons->is_projectile())
    {
        if (iood_act(*mons))
            return;
        mons->lose_energy(EUT_MOVE);
        return;
    }

    if (mons->type == MONS_BATTLESPHERE)
    {
        if (fire_battlesphere(mons))
            mons->lose_energy(EUT_SPECIAL);
    }

    if (mons->type == MONS_FULMINANT_PRISM)
    {
        ++mons->prism_charge;
        if (mons->prism_charge == 2)
            mons->suicide();
        else
        {
            if (player_can_hear(mons->pos()))
            {
                if (you.can_see(mons))
                {
                    simple_monster_message(mons, " crackles loudly.",
                                           MSGCH_WARN);
                }
                else
                    mprf(MSGCH_SOUND, "You hear a loud crackle.");
            }
            // Done this way to keep the detonation timer predictable
            mons->speed_increment -= 10;
        }
        return;
    }

    if (mons->type == MONS_GRAND_AVATAR)
        _grand_avatar_act(mons);

    mons->shield_blocks = 0;

    _mons_in_cloud(mons);
#if TAG_MAJOR_VERSION == 34
    _heated_area(mons);
#endif
    if (!mons->alive())
        return;

    if (env.level_state & LSTATE_SLIMY_WALL)
        slime_wall_damage(mons, speed_to_duration(mons->speed));
    if (!mons->alive())
        return;

    if (mons->type == MONS_TIAMAT && one_chance_in(3))
        draconian_change_colour(mons);

    _monster_regenerate(mons);

    if (mons->cannot_act()
        || mons->type == MONS_SIXFIRHY // these move only 8 of 24 turns
           && ++mons->move_spurt / 8 % 3 != 2  // but are not helpless
        || mons->type == MONS_JIANGSHI // similarly, but more irregular (48 of 90)
            && (++mons->move_spurt / 6 % 3 == 1 || mons->move_spurt / 3 % 5 == 1))
    {
        mons->speed_increment -= non_move_energy;
        return;
    }

    if (mons->has_ench(ENCH_DAZED) && one_chance_in(5))
    {
        simple_monster_message(mons, " is lost in a daze.");
        mons->speed_increment -= non_move_energy;
        return;
    }

    if (mons->has_ench(ENCH_GOLD_LUST))
    {
        mons->speed_increment -= non_move_energy;
        return;
    }

    const int gold = gozag_gold_in_los(mons);
    if (!mons->asleep()
        && !mons_is_avatar(mons->type)
        && !mons->wont_attack() && gold > 0)
    {
        if (bernoulli(gold, 3.0/100.0))
        {
            simple_monster_message(mons,
                    " is distracted by the nearby gold.");
            mons->add_ench(
                mon_enchant(ENCH_GOLD_LUST, 1, NULL,
                            random_range(1, 5) * BASELINE_DELAY));
            mons->foe = MHITNOT;
            mons->target = mons->pos();
            mons->speed_increment -= non_move_energy;
            return;
        }
    }

    if (crawl_state.disables[DIS_MON_ACT] && _unfriendly_or_insane(mons))
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

    if (igrd(mons->pos()) != NON_ITEM
        && (mons_itemuse(mons) >= MONUSE_WEAPONS_ARMOUR
            || mons_itemeat(mons) != MONEAT_NOTHING))
    {
        // Keep neutral, charmed, summoned, and friendly monsters from
        // picking up stuff.
        if ((!mons->neutral() && !mons->has_ench(ENCH_CHARM)
             || (you_worship(GOD_JIYVA) && mons_is_slime(mons)))
            && !mons->is_summoned() && !mons->is_perm_summoned()
            && !mons->friendly())
        {
            if (_handle_pickup(mons))
            {
                DEBUG_ENERGY_USE("handle_pickup()");
                return;
            }
        }
    }

    // Lurking monsters only stop lurking if their target is right
    // next to them, otherwise they just sit there.
    // However, if the monster is involuntarily submerged but
    // still alive (e.g., nonbreathing which had water poured
    // on top of it), this doesn't apply.
    if (mons_is_lurking(mons) || mons->has_ench(ENCH_SUBMERGED))
    {
        if (mons->foe != MHITNOT
            && grid_distance(mons->target, mons->pos()) <= 1)
        {
            if (mons->submerged())
            {
                if (!mons->del_ench(ENCH_SUBMERGED))
                {
                    // Couldn't unsubmerge.
                    mons->speed_increment -= non_move_energy;
                    return;
                }
            }
            mons->behaviour = BEH_SEEK;
        }
        else
        {
            mons->speed_increment -= non_move_energy;
            return;
        }
    }

    if (mons->caught())
    {
        // Struggling against the net takes time.
        _swim_or_move_energy(mons);
    }
    else if (!mons->petrified())
    {
        // Calculates mmov based on monster target.
        _handle_movement(mons);

        if (mons_is_confused(mons)
            || mons->type == MONS_AIR_ELEMENTAL
               && mons->submerged())
        {
            _confused_move_dir(mons);

            // OK, mmov determined.
            const coord_def newcell = mmov + mons->pos();
            monster* enemy = monster_at(newcell);
            if (enemy
                && newcell != mons->pos()
                && !is_sanctuary(mons->pos()))
            {
                if (fight_melee(mons, enemy))
                {
                    mmov.reset();
                    DEBUG_ENERGY_USE("fight_melee()");
                    return;
                }
                else
                {
                    // FIXME: None of these work!
                    // Instead run away!
                    if (mons->add_ench(mon_enchant(ENCH_FEAR)))
                        behaviour_event(mons, ME_SCARE, 0, newcell);
                    return;
                }
            }
        }
    }
    _maybe_submerge(mons);
    // do we really need this check in the case of colour cycling?
    if (!mons->asleep()
        && !mons->submerged())
    {
        maybe_mons_speaks(mons);
        if (mons->type == MONS_KILLER_KLOWN
            || mons->type == MONS_SPATIAL_VORTEX
            || mons->type == MONS_PANDEMONIUM_LORD
               && mons->ghost->cycle_colours)
        {
            mons->colour = random_colour();
        }
    }

    if (!mons->alive())
        return;

    // XXX: A bit hacky, but stores where we WILL move, if we don't take
    //      another action instead (used for decision-making)
    if (mons_stores_tracking_data(mons))
        mons->props["mmov"].get_coord() = mmov;

    if (!mons->asleep() && !mons_is_wandering(mons)
            && !mons->withdrawn()
            // Berserking monsters are limited to running up and
            // hitting their foes.
            && !mons->berserk_or_insane()
        // Slime creatures can split while wandering or resting.
        || mons->type == MONS_SLIME_CREATURE)
    {
        bolt beem;

        beem.source    = mons->pos();
        beem.target    = mons->target;
        beem.source_id = mons->mid;

        // XXX: Otherwise perma-confused monsters can almost never properly
        // aim spells, since their target is constantly randomized.
        // This does make them automatically aware of the player in several
        // situations they otherwise would not, however.
        if (mons_class_flag(mons->type, M_CONFUSED))
        {
            actor* foe = mons->get_foe();
            if (foe && mons->can_see(foe))
                beem.target = foe->pos();
        }

        // Prevents unfriendlies from nuking you from offscreen.
        // How nice!
        const bool friendly_or_near =
            mons->friendly() && mons->foe == MHITYOU || mons->near_foe();
        if (friendly_or_near
            || mons->type == MONS_TEST_SPAWNER
            // Slime creatures can split when offscreen.
            || mons->type == MONS_SLIME_CREATURE
            // Lost souls can flicker away at any time they're isolated
            || mons->type == MONS_LOST_SOUL
            // Let monsters who have Dig use it off-screen.
            || mons->has_spell(SPELL_DIG))
        {
            // [ds] Special abilities shouldn't overwhelm
            // spellcasting in monsters that have both.  This aims
            // to give them both roughly the same weight.
            if (coinflip() ? mon_special_ability(mons, beem)
                             || _do_mon_spell(mons, beem)
                           : _do_mon_spell(mons, beem)
                             || mon_special_ability(mons, beem))
            {
                DEBUG_ENERGY_USE("spell or special");
                mmov.reset();
                return;
            }
        }

        if (friendly_or_near)
        {
            if (_handle_potion(mons, beem))
            {
                DEBUG_ENERGY_USE("_handle_potion()");
                return;
            }

            if (_handle_scroll(mons))
            {
                DEBUG_ENERGY_USE("_handle_scroll()");
                return;
            }

            if (_handle_evoke_equipment(mons, beem))
            {
                DEBUG_ENERGY_USE("_handle_evoke_equipment()");
                return;
            }

            if (_handle_wand(mons, beem))
            {
                DEBUG_ENERGY_USE("_handle_wand()");
                return;
            }

            if (_handle_reaching(mons))
            {
                DEBUG_ENERGY_USE("_handle_reaching()");
                return;
            }
        }

        if (handle_throw(mons, beem, false, false))
        {
            DEBUG_ENERGY_USE("_handle_throw()");
            return;
        }
    }

    if (!mons->caught())
    {
        if (mons->pos() + mmov == you.pos())
        {
            ASSERT(!crawl_state.game_is_arena());

            if (_unfriendly_or_insane(mons)
                && !mons->has_ench(ENCH_CHARM)
                && !mons->withdrawn())
            {
                monster* new_target = 0;
                if (!mons->wont_attack())
                {
                    // Otherwise, if it steps into you, cancel other targets.
                    mons->foe = MHITYOU;
                    mons->target = you.pos();

                    // Check to see if your religion redirects the attack
                    if (does_ru_wanna_redirect(mons))
                    {
                        int r = random2(100);
                        int chance = div_rand_round(you.piety, 16);
                        // stun chance maxes at 10%
                        if (r < chance)
                        {
                            simple_monster_message(mons,
                                " is stunned by your will and fails to attack.",
                                MSGCH_GOD);
                            mons->speed_increment -= non_move_energy;
                            return;
                        }
                        // redirect maxes at 5%, given the above
                        else if (r < chance + div_rand_round(chance, 2))
                        {
                            // get a target
                            int pfound = 0;
                            for (adjacent_iterator ai(mons->pos(), false); ai; ++ai)
                            {
                                monster* candidate = monster_at(*ai);
                                if (candidate == NULL
                                    || mons_is_projectile(candidate->type)
                                    || mons_is_firewood(candidate))
                                {
                                    continue;
                                }
                                ASSERT(candidate);
                                if (one_chance_in(++pfound))
                                    new_target = candidate;
                            }
                        }
                    }
                }

                if (new_target)
                {
                    // attack that target
                    mons->target = new_target->pos();
                    mons->foe = new_target->mindex();
                    mprf(MSGCH_GOD, "You redirect %s's attack!",
                         mons->name(DESC_THE).c_str());
                    fight_melee(mons, new_target);
                }
                else
                    fight_melee(mons, &you);

                if (mons_is_batty(mons))
                {
                    mons->behaviour = BEH_WANDER;
                    set_random_target(mons);
                }
                DEBUG_ENERGY_USE("fight_melee()");
                mmov.reset();
                return;
            }
        }

        // See if we move into (and fight) an unfriendly monster.
        monster* targ = monster_at(mons->pos() + mmov);

        //If a tentacle owner is attempting to move into an adjacent
        //segment, kill the segment and adjust connectivity data.
        if (targ && mons_tentacle_adjacent(mons, targ))
        {
            bool basis = targ->props.exists("outwards");
            int out_idx = basis ? targ->props["outwards"].get_int() : -1;
            if (out_idx != -1)
                menv[out_idx].props["inwards"].get_int() = mons->mindex();

            monster_die(targ,
                        KILL_MISC, NON_MONSTER, true);
            targ = NULL;
        }

        if (targ
            && targ != mons
            && mons->behaviour != BEH_WITHDRAW
            && (!mons_aligned(mons, targ) || mons->has_ench(ENCH_INSANE))
            && monster_can_hit_monster(mons, targ))
        {
            // Maybe they can swap places?
            if (_swap_monsters(mons, targ))
            {
                _swim_or_move_energy(mons);
                return;
            }
            // Figure out if they fight.
            else if ((!mons_is_firewood(targ)
                      || mons->is_child_tentacle())
                          && fight_melee(mons, targ))
            {
                if (mons_is_batty(mons))
                {
                    mons->behaviour = BEH_WANDER;
                    set_random_target(mons);
                    // mons->speed_increment -= mons->speed;
                }

                mmov.reset();
                DEBUG_ENERGY_USE("fight_melee()");
                return;
            }
        }
        else if (mons->behaviour == BEH_WITHDRAW
                 && ((targ && targ != mons && targ->friendly())
                      || (you.pos() == mons->pos() + mmov)))
        {
            // Don't count turns spent blocked by friendly creatures
            // (or the player) as an indication that we're stuck
            mons->props.erase("blocked_deadline");
        }

        if (invalid_monster(mons) || mons->is_stationary())
        {
            if (mons->speed_increment == old_energy)
                mons->speed_increment -= non_move_energy;
            return;
        }

        if (mons->cannot_move() || !_monster_move(mons))
        {
            mons->speed_increment -= non_move_energy;
            mons->check_clinging(false);
        }
    }
    you.update_beholder(mons);
    you.update_fearmonger(mons);

    // Reevaluate behaviour, since the monster's surroundings have
    // changed (it may have moved, or died for that matter).  Don't
    // bother for dead monsters.  :)
    if (mons->alive())
    {
        handle_behaviour(mons);
        ASSERT_IN_BOUNDS_OR_ORIGIN(mons->target);
    }

    if (mons_is_tentacle_head(mons_base_type(mons)))
    {
        move_child_tentacles(mons);

        mons->move_spurt += (old_energy - mons->speed_increment)
                             * _tentacle_move_speed(mons_base_type(mons));
        ASSERT(mons->move_spurt > 0);
        while (mons->move_spurt >= 100)
        {
            move_child_tentacles(mons);
            mons->move_spurt -= 100;
        }
    }
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
        trap_def *trap = find_trap(pos());
        if (trap && trap->type == TRAP_WEB)
        {

            if (coinflip())
            {
                if (mons_near(this) && !visible_to(&you))
                    mpr("Something you can't see is thrashing in a web.");
                else
                {
                    simple_monster_message(this,
                                           " struggles to get unstuck from the web.");
                }
                return;
            }
            maybe_destroy_web(this);
        }
        del_ench(ENCH_HELD);
        return;
    }

    // Handled in handle_pickup().
    if (mons_eats_items(this))
        return;

    // The enchantment doubles as the durability of a net
    // the more corroded it gets, the more easily it will break.
    const int hold = mitm[net].plus; // This will usually be negative.
    const int mon_size = body_size(PSIZE_BODY);

    // Smaller monsters can escape more quickly.
    if (mon_size < random2(SIZE_BIG)  // BIG = 5
        && !berserk_or_insane() && type != MONS_DANCING_WEAPON)
    {
        if (mons_near(this) && !visible_to(&you))
            mpr("Something wriggles in the net.");
        else
            simple_monster_message(this, " struggles to escape the net.");

        // Confused monsters have trouble finding the exit.
        if (has_ench(ENCH_CONFUSION) && !one_chance_in(5))
            return;

        decay_enchantment(ENCH_HELD, 2*(NUM_SIZE_LEVELS - mon_size) - hold);

        // Frayed nets are easier to escape.
        if (mon_size <= -(hold-1)/2)
            decay_enchantment(ENCH_HELD, (NUM_SIZE_LEVELS - mon_size));
    }
    else // Large (and above) monsters always thrash the net and destroy it
    {    // e.g. ogre, large zombie (large); centaur, naga, hydra (big).

        if (mons_near(this) && !visible_to(&you))
            mpr("Something wriggles in the net.");
        else
            simple_monster_message(this, " struggles against the net.");

        // Confused monsters more likely to struggle without result.
        if (has_ench(ENCH_CONFUSION) && one_chance_in(3))
            return;

        // Nets get destroyed more quickly for larger monsters
        // and if already strongly frayed.
        int damage = 0;

        // tiny: 1/6, little: 2/5, small: 3/4, medium and above: always
        if (x_chance_in_y(mon_size + 1, SIZE_GIANT - mon_size))
            damage++;

        // Handled specially to make up for its small size.
        if (type == MONS_DANCING_WEAPON)
        {
            damage += one_chance_in(3);

            if (can_cut_meat(mitm[inv[MSLOT_WEAPON]]))
                damage++;
        }

        // Extra damage for large (50%) and big (always).
        if (mon_size == SIZE_BIG || mon_size == SIZE_LARGE && coinflip())
            damage++;

        // overall damage per struggle:
        // tiny   -> 1/6
        // little -> 2/5
        // small  -> 3/4
        // medium -> 1
        // large  -> 1,5
        // big    -> 2

        // extra damage if already damaged
        if (random2(body_size(PSIZE_BODY) - hold + 1) >= 4)
            damage++;

        // Berserking doubles damage dealt.
        if (berserk())
            damage *= 2;

        // Faster monsters can damage the net more often per
        // time period.
        if (speed != 0)
            damage = div_rand_round(damage * speed, 10);

        mitm[net].plus -= damage;

        if (mitm[net].plus < -7)
        {
            if (mons_near(this))
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
}

static void _ancient_zyme_sicken(monster* mons)
{
    if (is_sanctuary(mons->pos()))
        return;

    if (!is_sanctuary(you.pos())
        && you.res_rotting() <= 0
        && !you.duration[DUR_DIVINE_STAMINA]
        && cell_see_cell(you.pos(), mons->pos(), LOS_SOLID_SEE))
    {
        if (!you.disease)
        {
            if (!you.duration[DUR_SICKENING])
            {
                mprf(MSGCH_WARN, "You feel yourself growing ill in the presence of %s.",
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

    for (radius_iterator ri(mons->pos(), LOS_RADIUS, C_ROUND); ri; ++ri)
    {
        monster *m = monster_at(*ri);
        if (m && cell_see_cell(mons->pos(), *ri, LOS_SOLID_SEE)
            && !is_sanctuary(*ri))
        {
            m->sicken(2 * you.time_taken);
        }
    }
}

/**
 * Apply the torpor snail slowing effect.
 *
 * @param mons      The snail applying the effect.
 */
static void _torpor_snail_slow(monster* mons)
{
    // XXX: might be nice to refactor together with _ancient_zyme_sicken().
    // XXX: also with torpor_slowed().... so many duplicated checks :(

    if (is_sanctuary(mons->pos())
        || mons->attitude != ATT_HOSTILE
        || mons->has_ench(ENCH_CHARM))
    {
        return;
    }

    if (!is_sanctuary(you.pos())
        && !you.stasis()
        && cell_see_cell(you.pos(), mons->pos(), LOS_SOLID_SEE))
    {
        if (!you.duration[DUR_SLOW])
        {
            mprf("Being near %s leaves you feeling lethargic.",
                 mons->name(DESC_THE).c_str());
        }

        if (you.duration[DUR_SLOW] <= 1)
            you.set_duration(DUR_SLOW, 1);
        you.props[TORPOR_SLOWED_KEY] = true;
    }

    for (monster_near_iterator ri(mons->pos(), LOS_SOLID_SEE); ri; ++ri)
    {
        monster *m = *ri;
        if (m && !mons_aligned(mons, m) && !m->check_stasis(true)
            && !m->is_stationary() && !is_sanctuary(m->pos()))
        {
            m->add_ench(mon_enchant(ENCH_SLOW, 0, mons, 1));
            m->props[TORPOR_SLOWED_KEY] = true;
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

    if (mons->type == MONS_ANCIENT_ZYME)
        _ancient_zyme_sicken(mons);

    if (mons->type == MONS_TORPOR_SNAIL)
        _torpor_snail_slow(mons);

    if (mons->type == MONS_ASMODEUS)
    {
        cloud_type ctype = CLOUD_FIRE;

        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
            if (!cell_is_solid(*ai)
                && (env.cgrid(*ai) == EMPTY_CLOUD
                    || env.cloud[env.cgrid(*ai)].type == ctype))
            {
                place_cloud(ctype, *ai, 2 + random2(6), mons);
            }
    }

    if (mons->type == MONS_WATER_NYMPH)
    {
        for (adjacent_iterator ai(mons->pos(), false); ai; ++ai)
            if (can_flood_feature(grd(*ai))
                && (coinflip() || *ai == mons->pos()))
            {
                temp_change_terrain(*ai, DNGN_SHALLOW_WATER, random_range(50, 80),
                                    TERRAIN_CHANGE_FLOOD);
            }
    }

    if (mons->type == MONS_GUARDIAN_GOLEM)
        guardian_golem_bond(mons);

    // A rakshasa that has regained full health dismisses its emergency clones
    // (if they're somehow still alive) and regains the ability to summon new ones.
    if (mons->type == MONS_RAKSHASA && mons->hit_points == mons->max_hit_points
        && !mons->has_ench(ENCH_PHANTOM_MIRROR)
        && mons->props.exists("emergency_clone"))
    {
        mons->props.erase("emergency_clone");
        for (monster_iterator mi; mi; ++mi)
        {
            if (mi->type == MONS_RAKSHASA && mi->summoner == mons->mid)
                mi->del_ench(ENCH_ABJ);
        }
    }

    if (mons->type != MONS_NO_MONSTER && mons->hit_points < 1)
        monster_die(mons, KILL_MISC, NON_MONSTER);
}

priority_queue<pair<monster *, int>,
               vector<pair<monster *, int> >,
               MonsterActionQueueCompare> monster_queue;

// Inserts a monster into the monster queue (needed to ensure that any monsters
// given energy or an action by a effect can actually make use of that energy
// this round)
void queue_monster_for_action(monster* mons)
{
    monster_queue.push(pair<monster *, int>(mons, mons->speed_increment));
}

static void _clear_monster_flags()
{
    // Clear any summoning flags so that lower indiced
    // monsters get their actions in the next round.
    // Also clear one-turn deep sleep flag.
    // XXX: MF_JUST_SLEPT only really works for player-cast hibernation.
    for (int i = 0; i < MAX_MONSTERS; i++)
        menv[i].flags &= ~MF_JUST_SUMMONED & ~MF_JUST_SLEPT;
}

/**
* On each monster turn, check to see if we need to update monster attitude.
* At the time of writing, it just checks for MUT_NO_LOVE from Ru Sacrifice Love.
*
* @param mon     The targeted monster
* @return        Void
**/
static void _update_monster_attitude(monster *mon)
{
    if (player_mutation_level(MUT_NO_LOVE)
        && !mons_class_flag(mon->type, M_SPELL_PROXY))
    {
        mon->attitude = ATT_HOSTILE;
    }
}

//---------------------------------------------------------------
//
// handle_monsters
//
// This is the routine that controls monster AI.
//
//---------------------------------------------------------------
void handle_monsters(bool with_noise)
{
    for (monster_iterator mi; mi; ++mi)
    {
        _pre_monster_move(*mi);
        if (!invalid_monster(*mi) && mi->alive() && mi->has_action_energy())
            monster_queue.push(pair<monster *, int>(*mi, mi->speed_increment));
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
        // This can happen with, e.g., headbutt stuns, cold attacks on cold-
        // -blooded monsters, etc.
        // If something's played with the energy, they get added back to
        // the queue just after this.
        if (oldspeed == mon->speed_increment)
        {
            handle_monster_move(mon);
            _post_monster_move(mon);
            fire_final_effects();
        }

        if (mon->has_action_energy())
        {
            monster_queue.push(
                pair<monster *, int>(mon, mon->speed_increment));
        }

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

    // Process noises now (before clearing the sleep flag).
    if (with_noise)
        apply_noises();

    _clear_monster_flags();
}

static bool _jelly_divide(monster* parent)
{
    if (!mons_class_flag(parent->type, M_SPLITS))
        return false;

    const int reqd = max(parent->get_experience_level() * 8, 50);
    if (parent->hit_points < reqd)
        return false;

    monster* child = NULL;
    coord_def child_spot;
    int num_spots = 0;

    // First, find a suitable spot for the child {dlb}:
    for (adjacent_iterator ai(parent->pos()); ai; ++ai)
        if (actor_at(*ai) == NULL && parent->can_pass_through(*ai)
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
    parent->max_hit_points /= 2;

    if (parent->hit_points > parent->max_hit_points)
        parent->hit_points = parent->max_hit_points;

    parent->init_experience();
    parent->experience = parent->experience * 3 / 5 + 1;

    // Create child {dlb}:
    // This is terribly partial and really requires
    // more thought as to generation ... {dlb}
    *child = *parent;
    child->max_hit_points  = child->hit_points;
    child->speed_increment = 70 + random2(5);
    child->moveto(child_spot);
    child->set_new_monster_id();

    mgrd(child->pos()) = child->mindex();

    if (!simple_monster_message(parent, " splits in two!")
        && (player_can_hear(parent->pos()) || player_can_hear(child->pos())))
    {
        mprf(MSGCH_SOUND, "You hear a squelching noise.");
    }

    if (crawl_state.game_is_arena())
        arena_placed_monster(child);

    return true;
}

// XXX: This function assumes that only jellies eat items.
static bool _monster_eat_item(monster* mons, bool nearby)
{
    if (!mons_eats_items(mons))
        return false;

    // Friendly jellies won't eat (unless worshipping Jiyva).
    if (mons->friendly() && !you_worship(GOD_JIYVA))
        return false;

    // Off-limit squares are off-limit.
    if (testbits(env.pgrid(mons->pos()), FPROP_NO_JIYVA))
        return false;

    int hps_changed = 0;
    // Zotdef jellies are toned down slightly
    int max_eat = roll_dice(1, (crawl_state.game_is_zotdef() ? 8 : 10));
    int eaten = 0;
    bool eaten_net = false;
    bool death_ooze_ate_good = false;
    bool death_ooze_ate_corpse = false;
    bool shown_msg = false;
    piety_gain_t gain = PIETY_NONE;
    int js = JS_NONE;

    // Jellies can swim, so don't check water
    for (stack_iterator si(mons->pos());
         si && eaten < max_eat && hps_changed < 50; ++si)
    {
        if (!is_item_jelly_edible(*si))
            continue;

        dprf("%s eating %s", mons->name(DESC_PLAIN, true).c_str(),
             si->name(DESC_PLAIN).c_str());

        int quant = si->quantity;

        death_ooze_ate_good = (mons->type == MONS_DEATH_OOZE
                               && (get_weapon_brand(*si) == SPWPN_HOLY_WRATH
                                   || get_ammo_brand(*si) == SPMSL_SILVER));
        death_ooze_ate_corpse = (mons->type == MONS_DEATH_OOZE
                                 && ((si->base_type == OBJ_CORPSES
                                     && si->sub_type == CORPSE_BODY)
                                 || si->base_type == OBJ_FOOD
                                     && si->sub_type == FOOD_CHUNK));

        if (si->base_type != OBJ_GOLD)
        {
            quant = min(quant, max_eat - eaten);

            hps_changed += (quant * item_mass(*si))
                           / (crawl_state.game_is_zotdef() ? 30 : 20) + quant;
            eaten += quant;

            if (mons->caught() && item_is_stationary_net(*si))
            {
                // We don't want to mulch the net just yet.
                mons->del_ench(ENCH_HELD, true, false);
                eaten_net = true;
            }
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
                 nearby ? "" : " distant");
            shown_msg = true;
        }

        if (you_worship(GOD_JIYVA))
        {
            gain = sacrifice_item_stack(*si, &js, quant);
            if (gain > PIETY_NONE)
                simple_god_message(" appreciates your sacrifice.");

            jiyva_slurp_message(js);
        }

        if (quant >= si->quantity)
            item_was_destroyed(*si);
        else if (is_perishable_stack(*si))
            for (int i = 0; i < quant; ++i)
                remove_oldest_perishable_item(*si);
        dec_mitm_item_quantity(si.link(), quant);
    }

    if (eaten > 0)
    {
        hps_changed = max(hps_changed, 1);
        hps_changed = min(hps_changed, 50);

        if (death_ooze_ate_good)
            mons->hurt(NULL, hps_changed, BEAM_NONE, false);
        else
        {
            // This is done manually instead of using heal_monster(),
            // because that function doesn't work quite this way. - bwr
            const int avg_hp = mons_avg_hp(mons->type);
            mons->hit_points += hps_changed;
            mons->hit_points = min(MAX_MONSTER_HP,
                                   min(avg_hp * 4, mons->hit_points));
            mons->max_hit_points = max(mons->hit_points, mons->max_hit_points);
        }

        if (death_ooze_ate_corpse)
            place_cloud(CLOUD_MIASMA, mons->pos(), 4 + random2(5), mons);

        if (death_ooze_ate_good)
            simple_monster_message(mons, " twists violently!");
        else if (eaten_net)
            simple_monster_message(mons, " devours the net!");
        else
            _jelly_divide(mons);
    }

    return eaten > 0;
}

static bool _monster_eat_single_corpse(monster* mons, item_def& item,
                                       bool do_heal, bool nearby)
{
    if (item.base_type != OBJ_CORPSES || item.sub_type != CORPSE_BODY)
        return false;

    const monster_type mt = item.mon_type;
    if (do_heal)
    {
        const int avg_hp = mons_avg_hp(mons->type);
        mons->hit_points += 1 + random2(mons_weight(mt)) / 100;
        mons->hit_points = min(MAX_MONSTER_HP,
                               min(avg_hp * 2, mons->hit_points));
        mons->max_hit_points = max(mons->hit_points, mons->max_hit_points);
    }

    if (nearby)
    {
        mprf("%s eats %s.", mons->name(DESC_THE).c_str(),
             item.name(DESC_THE).c_str());
    }

    // Butcher the corpse without leaving chunks.
    butcher_corpse(item, MB_MAYBE, false);

    return true;
}

static bool _monster_eat_corpse(monster* mons, bool do_heal, bool nearby)
{
    if (!mons_eats_corpses(mons))
        return false;

    int eaten = 0;

    for (stack_iterator si(mons->pos()); si; ++si)
    {
        if (_monster_eat_single_corpse(mons, *si, do_heal, nearby))
        {
            eaten++;
            break;
        }
    }

    return eaten > 0;
}


//---------------------------------------------------------------
//
// handle_pickup
//
// Returns false if monster doesn't spend any time picking something up.
//
//---------------------------------------------------------------
static bool _handle_pickup(monster* mons)
{
    if (mons->asleep() || mons->submerged())
        return false;

    // Flying over water doesn't let you pick up stuff.  This is inexact, as
    // a merfolk could be flying, but that's currently impossible except for
    // being tornadoed, and with *that* low life expectancy let's not care.
    dungeon_feature_type feat = grd(mons->pos());

    if ((feat == DNGN_LAVA || feat == DNGN_DEEP_WATER) && mons->flight_mode())
        return false;

    const bool nearby = mons_near(mons);
    int count_pickup = 0;

    if (mons_itemeat(mons) != MONEAT_NOTHING)
    {
        if (mons_eats_items(mons))
        {
            if (_monster_eat_item(mons, nearby))
                return false;
        }
        else if (mons_eats_corpses(mons))
        {
            // Assume that only undead can heal from eating corpses.
            if (_monster_eat_corpse(mons, mons->holiness() == MH_UNDEAD,
                                    nearby))
            {
                return false;
            }
        }
    }

    if (mons_itemuse(mons) >= MONUSE_WEAPONS_ARMOUR)
    {
        // Note: Monsters only look at stuff near the top of stacks.
        //
        // XXX: Need to put in something so that monster picks up
        // multiple items (e.g. ammunition) identical to those it's
        // carrying.
        //
        // Monsters may now pick up up to two items in the same turn.
        // (jpeg)
        for (stack_iterator si(mons->pos()); si; ++si)
        {
            if (si->flags & ISFLAG_NO_PICKUP)
                continue;

            if (mons->pickup_item(*si, nearby))
                count_pickup++;

            if (count_pickup > 1 || coinflip())
                break;
        }
    }

    return count_pickup > 0;
}

static void _mons_open_door(monster* mons, const coord_def &pos)
{
    const char *adj = "", *noun = "door";

    bool was_seen   = false;

    set<coord_def> all_door;
    find_connected_identical(pos, all_door);
    get_door_description(all_door.size(), &adj, &noun);

    for (set<coord_def>::iterator i = all_door.begin();
         i != all_door.end(); ++i)
    {
        const coord_def& dc = *i;

        if (you.see_cell(dc))
            was_seen = true;

        grd(dc) = DNGN_OPEN_DOOR;
        set_terrain_changed(dc);
    }

    if (was_seen)
    {
        viewwindow();

        string open_str = "opens the ";
        open_str += adj;
        open_str += noun;
        open_str += ".";

        // Should this be conditionalized on you.can_see(mons?)
        mons->seen_context = (all_door.size() <= 2) ? SC_DOOR : SC_GATE;

        if (!you.can_see(mons))
        {
            mprf("Something unseen %s", open_str.c_str());
            interrupt_activity(AI_FORCE_INTERRUPT);
        }
        else if (!you_are_delayed())
        {
            mprf("%s %s", mons->name(DESC_A).c_str(),
                 open_str.c_str());
        }
    }

    mons->lose_energy(EUT_MOVE);

    dungeon_events.fire_position_event(DET_DOOR_OPENED, pos);
}

static bool _no_habitable_adjacent_grids(const monster* mon)
{
    for (adjacent_iterator ai(mon->pos()); ai; ++ai)
        if (monster_habitable_grid(mon, grd(*ai)))
            return false;

    return true;
}

static bool _same_tentacle_parts(const monster* mpusher,
                               const monster* mpushee)
{
    if (!mons_is_tentacle_head(mons_base_type(mpusher)))
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

    if (mpusher->type == MONS_WANDERING_MUSHROOM
        && mpushee->type == MONS_TOADSTOOL
        || mpusher->type == MONS_TOADSTOOL
           && mpushee->type == MONS_WANDERING_MUSHROOM)
    {
        return true;
    }

    if (!mpushee->has_action_energy()
        && !_same_tentacle_parts(mpusher, mpushee))
    {
        return false;
    }

    // Confused monsters can't be pushed past, sleeping monsters
    // can't push. Note that sleeping monsters can't be pushed
    // past, either, but they may be woken up by a crowd trying to
    // elbow past them, and the wake-up check happens downstream.
    // Monsters caught in a net also can't be pushed past.
    if (mons_is_confused(mpusher) || mons_is_confused(mpushee)
        || mpusher->cannot_move() || mpusher->is_stationary()
        || mpusher->is_constricted() || mpushee->is_constricted()
        || (!_same_tentacle_parts(mpusher, mpushee)
           && (mpushee->cannot_move() || mpushee->is_stationary()))
        || mpusher->asleep() || mpushee->caught())
    {
        return false;
    }

    // Fleeing monsters cannot push past other fleeing monsters
    // (This helps to prevent some traffic jams in confined spaces)
    if (mons_is_fleeing(mpusher) && mons_is_fleeing(mpushee))
        return false;

    // Batty monsters are unpushable.
    if (mons_is_batty(mpusher) || mons_is_batty(mpushee))
        return false;

    // Anyone can displace a submerged monster (otherwise monsters can
    // get stuck behind a trapdoor spider, giving away its presence).
    if (mpushee->submerged())
        return true;

    if (!monster_shover(mpusher))
        return false;

    // Fleeing monsters of the same type may push past higher ranking ones.
    if (!monster_senior(mpusher, mpushee, mons_is_retreating(mpusher)))
        return false;

    return true;
}

static int _count_adjacent_slime_walls(const coord_def &pos)
{
    int count = 0;
    for (adjacent_iterator ai(pos); ai; ++ai)
        if (env.grid(*ai) == DNGN_SLIMY_WALL)
            count++;

    return count;
}

// Returns true if the monster should try to avoid that position
// because of taking damage from slime walls.
static bool _check_slime_walls(const monster *mon,
                               const coord_def &targ)
{
    if (mons_is_slime(mon) || actor_slime_wall_immune(mon)
        || mons_intel(mon) <= I_REPTILE)
    {
        return false;
    }
    const int target_count = _count_adjacent_slime_walls(targ);
    // Entirely safe.
    if (!target_count)
        return false;

    const int current_count = _count_adjacent_slime_walls(mon->pos());
    if (target_count <= current_count)
        return false;

    // The monster needs to have a purpose to risk taking damage.
    if (!mons_is_seeking(mon))
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
    if (!mons->wont_attack()
        && is_sanctuary(targ)
        && !is_sanctuary(mons->pos()))
    {
        return false;
    }

    // Inside a sanctuary don't attack anything!
    if (is_sanctuary(mons->pos()) && actor_at(targ))
        return false;

    const dungeon_feature_type target_grid = grd(targ);
    const habitat_type habitat = mons_primary_habitat(mons);

    // No monster may enter the open sea.
    if (target_grid == DNGN_OPEN_SEA || target_grid == DNGN_LAVA_SEA)
        return false;

    // The kraken is so large it cannot enter shallow water.
    // Its tentacles can, and will, though.
    if (target_grid == DNGN_SHALLOW_WATER && mons_base_type(mons) == MONS_KRAKEN)
        return false;

    const int targ_cloud_num = env.cgrid(targ);
    if (mons_avoids_cloud(mons, targ_cloud_num))
        return false;

    if (env.level_state & LSTATE_SLIMY_WALL && _check_slime_walls(mons, targ))
        return false;

    const bool digs = _mons_can_cast_dig(mons, false)
                      || _mons_can_zap_dig(mons);
    if ((target_grid == DNGN_ROCK_WALL || target_grid == DNGN_CLEAR_ROCK_WALL)
           && (mons_class_flag(mons->type, M_BURROWS) || digs)
        || mons->type == MONS_SPATIAL_MAELSTROM
           && feat_is_solid(target_grid) && !feat_is_permarock(target_grid)
        || feat_is_tree(target_grid) && mons_flattens_trees(mons)
        || target_grid == DNGN_GRATE && digs)
    {
    }
    else if (!mons_can_traverse(mons, targ, false, false)
             && !monster_habitable_grid(mons, target_grid))
    {
        // If the monster somehow ended up in this habitat (and is
        // not dead by now), give it a chance to get out again.
        if (grd(mons->pos()) == target_grid && mons->ground_level()
            && _no_habitable_adjacent_grids(mons))
        {
            return true;
        }

        return false;
    }

    // Wandering mushrooms usually don't move while you are looking.
    if (mons->type == MONS_WANDERING_MUSHROOM
        || mons->type == MONS_DEATHCAP
        || (mons->type == MONS_LURKING_HORROR
            && mons->foe_distance() > random2(LOS_RADIUS + 1)))
    {
        if (!mons->wont_attack() && is_sanctuary(mons->pos()))
            return true;

        if (!mons->friendly() && you.see_cell(targ)
            || mon_enemies_around(mons))
        {
            return false;
        }
    }

    if (mons->type == MONS_MERFOLK_AVATAR)
    {
        // Don't voluntarily break LoS with a player we're mesmerising
        if (you.beheld_by(mons) && !you.see_cell(targ))
            return false;

        // And path around players instead of into them
        if (you.pos() == targ)
            return false;
    }

    // Fire elementals avoid water and cold.
    if (mons->type == MONS_FIRE_ELEMENTAL && feat_is_watery(target_grid))
        return false;

    // Try to avoid deliberately blocking the player's line of fire.
    if (mons->type == MONS_SPELLFORGED_SERVITOR)
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

    // Submerged water creatures avoid the shallows where
    // they would be forced to surface. -- bwr
    // [dshaligram] Monsters now prefer to head for deep water only if
    // they're low on hitpoints. No point in hiding if they want a
    // fight.
    if (habitat == HT_WATER
        && targ != you.pos()
        && target_grid != DNGN_DEEP_WATER
        && grd(mons->pos()) == DNGN_DEEP_WATER
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

        if (!summon_can_attack(mons, targ))
            return false;

        if (targmonster->type == MONS_TOADSTOOL
            && mons->type == MONS_WANDERING_MUSHROOM)
        {
            return true;
        }

        // Cut down plants only when no alternative, or they're
        // our target.
        if (mons_is_firewood(targmonster) && mons->target != targ)
            return false;

        if (mons_aligned(mons, targmonster)
            && !mons->has_ench(ENCH_INSANE)
            && !_mons_can_displace(mons, targmonster))
        {
            // In Zotdef hostiles will whack other hostiles if immobile
            // - prevents plugging gaps with hostile oklobs
            if (crawl_state.game_is_zotdef())
            {
                if (!targmonster->is_stationary()
                    || targmonster->attitude != ATT_HOSTILE)
                {
                    return false;
                }
            }
            else
                return false;
        }
        // Prefer to move past enemies instead of hit them, if we're retreating
        else if ((!mons_aligned(mons, targmonster)
                  || mons->has_ench(ENCH_INSANE))
                 && mons->behaviour == BEH_WITHDRAW)
        {
            return false;
        }
    }

    // Friendlies shouldn't try to move onto the player's
    // location, if they are aiming for some other target.
    if (!_unfriendly_or_insane(mons)
        && mons->foe != MHITYOU
        && (mons->foe != MHITNOT || mons->is_patrolling())
        && targ == you.pos())
    {
        return false;
    }

    // Wandering through a trap is OK if we're pretty healthy,
    // really stupid, or immune to the trap.
    if (!mons->is_trap_safe(targ, just_check))
        return false;

    // If we end up here the monster can safely move.
    return true;
}

// Uses, and updates the global variable mmov.
static void _find_good_alternate_move(monster* mons,
                                      const move_array& good_move)
{
    const coord_def target = mons->firing_pos.zero() ? mons->target
                                                     : mons->firing_pos;
    const int current_distance = distance2(mons->pos(), target);

    int dir = _compass_idx(mmov);

    // Only handle if the original move is to an adjacent square.
    if (dir == -1)
        return;

    int dist[2];

    // First 1 away, then 2 (3 is silly).
    for (int j = 1; j <= 2; j++)
    {
        const int FAR_AWAY = 1000000;

        // Try both directions (but randomise which one is first).
        const int sdir = coinflip() ? j : -j;
        const int inc = -2 * sdir;

        for (int mod = sdir, i = 0; i < 2; mod += inc, i++)
        {
            const int newdir = (dir + 8 + mod) % 8;
            if (good_move[mon_compass[newdir].x+1][mon_compass[newdir].y+1])
                dist[i] = distance2(mons->pos()+mon_compass[newdir], target);
            else
                dist[i] = mons_is_retreating(mons) ? (-FAR_AWAY) : FAR_AWAY;
        }

        const int dir0 = ((dir + 8 + sdir) % 8);
        const int dir1 = ((dir + 8 - sdir) % 8);

        // Now choose.
        if (dist[0] == dist[1] && abs(dist[0]) == FAR_AWAY)
            continue;

        // Which one was better? -- depends on FLEEING or not.
        if (mons_is_retreating(mons))
        {
            if (dist[0] >= dist[1] && dist[0] >= current_distance)
            {
                mmov = mon_compass[dir0];
                break;
            }
            if (dist[1] >= dist[0] && dist[1] >= current_distance)
            {
                mmov = mon_compass[dir1];
                break;
            }
        }
        else
        {
            if (dist[0] <= dist[1] && dist[0] <= current_distance)
            {
                mmov = mon_compass[dir0];
                break;
            }
            if (dist[1] <= dist[0] && dist[1] <= current_distance)
            {
                mmov = mon_compass[dir1];
                break;
            }
        }
    }
}

static void _jelly_grows(monster* mons)
{
    if (player_can_hear(mons->pos()))
    {
        mprf(MSGCH_SOUND, "You hear a%s slurping noise.",
             mons_near(mons) ? "" : " distant");
    }

    const int avg_hp = mons_avg_hp(mons->type);
    mons->hit_points += 5;
    mons->hit_points = min(MAX_MONSTER_HP,
                           min(avg_hp * 4, mons->hit_points));

    // note here, that this makes jellies "grow" {dlb}:
    if (mons->hit_points > mons->max_hit_points)
        mons->max_hit_points = mons->hit_points;

    _jelly_divide(mons);
}

/**
 * Possibly place mold & ballistomycetes in giant spores' wake.
 *
 * @param mons      The giant spore in question.
 * @param position  Its last location. (Where to place the ballistomycete.)
 */
static void _ballisto_on_move(monster* mons, const coord_def& position)
{
    if (mons->type != MONS_GIANT_SPORE
        || crawl_state.game_is_zotdef()
        || mons->is_summoned())
    {
        return;
    }

    // place mold under the spore's current tile, if there isn't any now.
    const dungeon_feature_type current_ftype = env.grid(mons->pos());
    if (current_ftype == DNGN_FLOOR)
        env.pgrid(mons->pos()) |= FPROP_MOLD;

    if (mons->spore_cooldown > 0)
    {
        mons->spore_cooldown--;
        return;
    }

    if (!one_chance_in(4))
        return;

    // try to make a ballistomycete.
    const beh_type attitude = attitude_creation_behavior(mons->attitude);
    monster *plant = create_monster(mgen_data(MONS_BALLISTOMYCETE, attitude,
                                              NULL, 0, 0, position, MHITNOT,
                                              MG_FORCE_PLACE));

    if (!plant)
        return;

    if (mons_is_god_gift(mons, GOD_FEDHAS))
    {
        plant->flags |= MF_NO_REWARD; // XXX: is this needed?

        if (attitude == BEH_FRIENDLY)
        {
            plant->flags |= MF_ATT_CHANGE_ATTEMPT;

            mons_make_god_gift(plant, GOD_FEDHAS);
        }
    }

    // Don't leave mold on squares we place ballistos on
    remove_mold(position);

    if (you.can_see(plant))
    {
        mprf("%s grows in the wake of %s.",
             plant->name(DESC_A).c_str(), mons->name(DESC_THE).c_str());
    }

    // reset the cooldown.
    mons->spore_cooldown = 40;
}

bool monster_swaps_places(monster* mon, const coord_def& delta, bool takes_time)
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

    // Check that both monsters will be happy at their proposed new locations.
    const coord_def c = mon->pos();
    const coord_def n = mon->pos() + delta;

    if (!monster_habitable_grid(mon, grd(n)) && !mon->can_cling_to(n)
        || !monster_habitable_grid(m2, grd(c)) && !m2->can_cling_to(c))
    {
        return false;
    }

    // Okay, do the swap!
    if (takes_time)
    {
#ifdef EUCLIDEAN
        _swim_or_move_energy(mon, delta.abs() == 2);
#else
        _swim_or_move_energy(mon);
#endif
    }

    mon->set_position(n);
    mgrd(n) = mon->mindex();
    m2->set_position(c);

    mon->clear_far_constrictions();
    m2->clear_far_constrictions();

    const int m2i = m2->mindex();
    ASSERT_RANGE(m2i, 0, MAX_MONSTERS);
    mgrd(c) = m2i;
    if (takes_time)
        _swim_or_move_energy(m2);

    mon->check_redraw(c, false);
    if (mon->is_wall_clinging())
        mon->check_clinging(true);
    else
        mon->apply_location_effects(c);

    m2->check_redraw(n, false);
    if (m2->is_wall_clinging())
        m2->check_clinging(true);
    else
        m2->apply_location_effects(n);

    // The seen context no longer applies if the monster is moving normally.
    mon->seen_context = SC_NONE;
    m2->seen_context = SC_NONE;

    _handle_manticore_barbs(mon);
    _handle_manticore_barbs(m2);

    return false;
}

static bool _do_move_monster(monster* mons, const coord_def& delta)
{
    const coord_def f = mons->pos() + delta;

    if (!in_bounds(f))
        return false;

    if (f == you.pos())
    {
        fight_melee(mons, &you);
        return true;
    }

    // This includes the case where the monster attacks itself.
    if (monster* def = monster_at(f))
    {
        fight_melee(mons, def);
        return true;
    }

    if (mons->is_constricted())
    {
        if (mons->attempt_escape())
            simple_monster_message(mons, " escapes!");
        else
        {
            simple_monster_message(mons, " struggles to escape constriction.");
            _swim_or_move_energy(mons);
            return true;
        }
    }

    if (grd(f) == DNGN_CLOSED_DOOR || grd(f) == DNGN_SEALED_DOOR)
    {
        if (mons_can_destroy_door(mons, f))
        {
            grd(f) = DNGN_FLOOR;
            set_terrain_changed(f);

            if (you.see_cell(f))
            {
                viewwindow();

                if (!you.can_see(mons))
                {
                    mpr("The door bursts into shrapnel!");
                    interrupt_activity(AI_FORCE_INTERRUPT);
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
            grd(f) = DNGN_FLOOR;
            set_terrain_changed(f);

            _jelly_grows(mons);

            if (you.see_cell(f))
            {
                viewwindow();

                if (!you.can_see(mons))
                {
                    mpr("The door mysteriously vanishes.");
                    interrupt_activity(AI_FORCE_INTERRUPT);
                }
                else
                    simple_monster_message(mons, " eats the door!");
            }
        } // done door-eating jellies
    }

    // The monster gave a "comes into view" message and then immediately
    // moved back out of view, leaing the player nothing to see, so give
    // this message to avoid confusion.
    if (mons->seen_context == SC_JUST_SEEN && !you.see_cell(f))
        simple_monster_message(mons, " moves out of view.");
    else if (crawl_state.game_is_hints() && (mons->flags & MF_WAS_IN_VIEW)
             && !you.see_cell(f))
    {
        learned_something_new(HINT_MONSTER_LEFT_LOS, mons->pos());
    }

    // The seen context no longer applies if the monster is moving normally.
    mons->seen_context = SC_NONE;

    // This appears to be the real one, ie where the movement occurs:
#ifdef EUCLIDEAN
    _swim_or_move_energy(mons, delta.abs() == 2);
#else
    _swim_or_move_energy(mons);
#endif

    _escape_water_hold(mons);

    if (grd(mons->pos()) == DNGN_DEEP_WATER && grd(f) != DNGN_DEEP_WATER
        && !monster_habitable_grid(mons, DNGN_DEEP_WATER)
        && !mons->is_wall_clinging())
    {
        // er, what?  Seems impossible.
        mons->seen_context = SC_NONSWIMMER_SURFACES_FROM_DEEP;
    }
    mgrd(mons->pos()) = NON_MONSTER;

    coord_def old_pos = mons->pos();

    mons->set_position(f);

    mgrd(mons->pos()) = mons->mindex();

    mons->check_clinging(true);
    _ballisto_on_move(mons, old_pos);

    // Let go of all constrictees; only stop *being* constricted if we are now
    // too far away.
    mons->stop_constricting_all(true);
    mons->clear_far_constrictions();

    mons->check_redraw(mons->pos() - delta);
    mons->apply_location_effects(mons->pos() - delta);

    _handle_manticore_barbs(mons);

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
    // Outside of that case, can always cut mundane plants.
    if (mons_is_firewood(targ))
    {
        // Don't try to attack briars unless their damage will be insignificant
        if (targ->type == MONS_BRIAR_PATCH && mons->type != MONS_THORN_HUNTER
            && (mons->armour_class() * mons->hit_points) < 400)
        {
            return false;
        }
        else
            return true;
    }

    // In normal games, that's it.  Gotta keep those butterflies alive...
    if (!crawl_state.game_is_zotdef())
        return false;

    return targ->is_stationary()
           || mons_class_flag(mons_base_type(targ), M_NO_EXP_GAIN)
              && !mons_is_tentacle_or_tentacle_segment(targ->type);
}

static bool _monster_move(monster* mons)
{
    move_array good_move;

    const habitat_type habitat = mons_primary_habitat(mons);
    bool deep_water_available = false;

    if (mons->type == MONS_TRAPDOOR_SPIDER)
    {
        if (mons->submerged())
            return false;

        // Trapdoor spiders sometime hide if they can't see their foe.
        // (but friendly trapdoor spiders won't hide at all, for sanity's
        // sake.)
        const actor *foe = mons->get_foe();
        const bool can_see = foe && mons->can_see(foe);

        if (monster_can_submerge(mons, grd(mons->pos()))
            && !can_see && !mons_is_confused(mons)
            && !mons->friendly()
            && !mons->caught()
            && !mons->berserk_or_insane()
            && one_chance_in(5))
        {
            mons->add_ench(ENCH_SUBMERGED);
            mons->behaviour = BEH_LURK;
            return false;
        }
    }

    // Berserking monsters make a lot of racket.
    if (mons->berserk_or_insane())
    {
        int noise_level = get_shout_noise_level(mons_shouts(mons->type));
        if (noise_level > 0)
        {
            if (you.can_see(mons) && mons->berserk())
            {
                if (one_chance_in(10))
                {
                    mprf(MSGCH_TALK_VISUAL, "%s rages.",
                         mons->name(DESC_THE).c_str());
                }
                noisy(noise_level, mons->pos(), mons->mid);
            }
            else if (one_chance_in(5))
                handle_monster_shouts(mons, true);
            else
            {
                // Just be noisy without messaging the player.
                noisy(noise_level, mons->pos(), mons->mid);
            }
        }
    }

    if (mons->confused())
    {
        if (!mmov.origin() || one_chance_in(15))
        {
            const coord_def newpos = mons->pos() + mmov;
            if (in_bounds(newpos)
                && (habitat == HT_LAND
                    || monster_habitable_grid(mons, grd(newpos))))
            {
                return _do_move_monster(mons, mmov);
            }
        }
        return false;
    }

    // If a water monster is currently flopping around on land, it cannot
    // really control where it wants to move, though there's a 50% chance
    // of flopping into an adjacent water grid.
    if (mons->has_ench(ENCH_AQUATIC_LAND))
    {
        vector<coord_def> adj_water;
        vector<coord_def> adj_move;
        for (adjacent_iterator ai(mons->pos()); ai; ++ai)
        {
            if (!cell_is_solid(*ai))
            {
                adj_move.push_back(*ai);
                if (feat_is_watery(grd(*ai)))
                    adj_water.push_back(*ai);
            }
        }
        if (adj_move.empty())
        {
            simple_monster_message(mons, " flops around on dry land!");
            return false;
        }

        vector<coord_def> moves = adj_water;
        if (adj_water.empty() || coinflip())
            moves = adj_move;

        coord_def newpos = mons->pos();
        int count = 0;
        for (unsigned int i = 0; i < moves.size(); ++i)
            if (one_chance_in(++count))
                newpos = moves[i];

        const monster* mon2 = monster_at(newpos);
        if (!mons->has_ench(ENCH_INSANE)
            && (newpos == you.pos() && mons->wont_attack()
                || (mon2 && mons->wont_attack() == mon2->wont_attack())))
        {
            simple_monster_message(mons, " flops around on dry land!");
            return false;
        }

        return _do_move_monster(mons, newpos - mons->pos());
    }

    // Let's not even bother with this if mmov is zero.
    if (mmov.origin())
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
            dungeon_feature_type target_grid = grd[targ_x][targ_y];

            if (target_grid == DNGN_DEEP_WATER)
                deep_water_available = true;

            good_move[count_x][count_y] =
                mon_can_move_to_pos(mons, coord_def(count_x-1, count_y-1));
        }

    // Now we know where we _can_ move.

    const coord_def newpos = mons->pos() + mmov;
    // Water creatures have a preference for water they can hide in -- bwr
    // [ds] Weakened the powerful attraction to deep water if the monster
    // is in good health.
    if (habitat == HT_WATER
        && deep_water_available
        && grd(mons->pos()) != DNGN_DEEP_WATER
        && grd(newpos) != DNGN_DEEP_WATER
        && newpos != you.pos()
        && (one_chance_in(3)
            || mons->hit_points <= (mons->max_hit_points * 3) / 4))
    {
        int count = 0;

        for (int cx = 0; cx < 3; cx++)
            for (int cy = 0; cy < 3; cy++)
            {
                if (good_move[cx][cy]
                    && grd[mons->pos().x + cx - 1][mons->pos().y + cy - 1]
                            == DNGN_DEEP_WATER)
                {
                    if (one_chance_in(++count))
                    {
                        mmov.x = cx - 1;
                        mmov.y = cy - 1;
                    }
                }
            }
    }

    // Now, if a monster can't move in its intended direction, try
    // either side.  If they're both good, move in whichever dir
    // gets it closer (farther for fleeing monsters) to its target.
    // If neither does, do nothing.
    if (good_move[mmov.x + 1][mmov.y + 1] == false)
        _find_good_alternate_move(mons, good_move);

    // ------------------------------------------------------------------
    // If we haven't found a good move by this point, we're not going to.
    // ------------------------------------------------------------------

    if (mons->type == MONS_SPATIAL_MAELSTROM)
    {
        const dungeon_feature_type feat = grd(mons->pos() + mmov);
        if (!feat_is_permarock(feat) && feat_is_solid(feat))
        {
            const coord_def target(mons->pos() + mmov);
            create_monster(
                    mgen_data(MONS_SPATIAL_VORTEX, SAME_ATTITUDE(mons), mons,
                          2, MON_SUMM_ANIMATE,
                          target, MHITNOT,
                          0, GOD_LUGONU));
            destroy_wall(target);
        }
    }

    const bool burrows = mons_class_flag(mons->type, M_BURROWS);
    const bool flattens_trees = mons_flattens_trees(mons);
    const bool digs = _mons_can_cast_dig(mons, false)
                      || _mons_can_zap_dig(mons);
    // Take care of beetle burrowing.
    if (burrows || flattens_trees || digs)
    {
        const dungeon_feature_type feat = grd(mons->pos() + mmov);
        if ((feat == DNGN_ROCK_WALL || feat == DNGN_CLEAR_ROCK_WALL)
                && !burrows && digs
            || feat == DNGN_GRATE && digs)
        {
            bolt beem;
            if (_mons_can_cast_dig(mons, true))
            {
                setup_mons_cast(mons, beem, SPELL_DIG);
                beem.target = mons->pos() + mmov;
                mons_cast(mons, beem, SPELL_DIG,
                          mons->spell_slot_flags(SPELL_DIG));
            }
            else if (_mons_can_zap_dig(mons))
            {
                ASSERT(mons->inv[MSLOT_WAND] != NON_ITEM);
                item_def &wand = mitm[mons->inv[MSLOT_WAND]];
                beem.target = mons->pos() + mmov;
                _setup_wand_beam(beem, mons);
                _mons_fire_wand(mons, wand, beem, you.can_see(mons), false);
            }
            else
                simple_monster_message(mons, " falters for a moment.");
            mons->lose_energy(EUT_SPELL);
            return true;
        }
        else if ((((feat == DNGN_ROCK_WALL || feat == DNGN_CLEAR_ROCK_WALL)
                  && burrows)
                  || (flattens_trees && feat_is_tree(feat)))
                 && good_move[mmov.x + 1][mmov.y + 1] == true)
        {
            const coord_def target(mons->pos() + mmov);
            destroy_wall(target);

            if (flattens_trees)
            {
                // Flattening trees has a movement cost to the monster
                // - 100% over and above its normal move cost.
                _swim_or_move_energy(mons);
                if (you.see_cell(target))
                {
                    const bool actor_visible = you.can_see(mons);
                    mprf("%s knocks down a tree!",
                         actor_visible?
                         mons->name(DESC_THE).c_str() : "Something");
                    noisy(25, target);
                }
                else
                    noisy(25, target, "You hear a crashing sound.");
            }
            else if (player_can_hear(mons->pos() + mmov))
            {
                // Formicids take extra time to dig.
                if (mons_genus(mons->type) == MONS_FORMICID)
                    mons->lose_energy(EUT_MOVE, 5);

                // Message depends on whether caused by acid (Dissolution)
                // or direct digging (boring beetles, formicids).
                mprf(MSGCH_SOUND, (mons->type == MONS_DISSOLUTION) ?
                     "You hear a sizzling sound." :
                     "You hear a grinding noise."
                     );
            }
        }
    }

    bool ret = false;
    if (good_move[mmov.x + 1][mmov.y + 1] && !mmov.origin())
    {
        // Check for attacking player.
        if (mons->pos() + mmov == you.pos())
        {
            ret = fight_melee(mons, &you);
            mmov.reset();
        }

        // If we're following the player through stairs, the only valid
        // movement is towards the player. -- bwr
        if (testbits(mons->flags, MF_TAKING_STAIRS))
        {
            const delay_type delay = current_delay_action();
            if (delay != DELAY_ASCENDING_STAIRS
                && delay != DELAY_DESCENDING_STAIRS)
            {
                mons->flags &= ~MF_TAKING_STAIRS;

                dprf("BUG: %s was marked as follower when not following!",
                     mons->name(DESC_PLAIN).c_str());
            }
            else
            {
                ret    = true;
                mmov.reset();

                dprf("%s is skipping movement in order to follow.",
                     mons->name(DESC_THE).c_str());
            }
        }

        // Check for attacking another monster.
        if (monster* targ = monster_at(mons->pos() + mmov))
        {
            if (mons_aligned(mons, targ)
                && !mons->has_ench(ENCH_INSANE)
                && (!crawl_state.game_is_zotdef()
                    || !_may_cutdown(mons, targ)))
            {
                bool takes_time = !(mons->type == MONS_WANDERING_MUSHROOM
                                    && targ->type == MONS_TOADSTOOL
                                    || mons->type == MONS_TOADSTOOL
                                       && targ->type == MONS_WANDERING_MUSHROOM);
                // Zotdef: monsters will cut down firewood
                ret = monster_swaps_places(mons, mmov, takes_time);
            }
            else
            {
                fight_melee(mons, targ);
                ret = true;
            }

            // If the monster swapped places, the work's already done.
            mmov.reset();
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

        if (mons->type == MONS_CURSE_TOE || mons->type == MONS_DEATH_SCARAB)
            place_cloud(CLOUD_MIASMA, mons->pos(), 2 + random2(3), mons);
    }
    else
    {
        monster* targ = monster_at(mons->pos() + mmov);
        if (!mmov.origin() && targ && _may_cutdown(mons, targ))
        {
            fight_melee(mons, targ);
            ret = true;
        }

        mmov.reset();

        // zotdef: sometimes seem to get gridlock. Reset travel path
        // if we can't move, occasionally
        if (crawl_state.game_is_zotdef() && one_chance_in(20))
        {
             mons->travel_path.clear();
             mons->travel_target = MTRAV_NONE;
        }

        // Fleeing monsters that can't move will panic and possibly
        // turn to face their attacker.
        make_mons_stop_fleeing(mons);
    }

    if (mmov.x || mmov.y || (mons->confused() && one_chance_in(6)))
        return _do_move_monster(mons, mmov);

    // Battlespheres need to preserve their tracking targets after each move
    if (mons_is_wandering(mons)
        && mons->type != MONS_BATTLESPHERE
        && mons->type != MONS_GRAND_AVATAR)
    {
        // trigger a re-evaluation of our wander target on our next move -cao
        mons->target = mons->pos();
        if (!mons->is_patrolling())
        {
            mons->travel_target = MTRAV_NONE;
            mons->travel_path.clear();
        }
        mons->firing_pos.reset();
    }

    return ret;
}

static void _mons_in_cloud(monster* mons)
{
    // Submerging in water or lava saves from clouds.
    if (mons->submerged() && env.grid(mons->pos()) != DNGN_FLOOR)
        return;

    actor_apply_cloud(mons);
}

#if TAG_MAJOR_VERSION == 34
static void _heated_area(monster* mons)
{
    if (!heated(mons->pos()))
        return;

    if (mons->is_fiery())
        return;

    // HACK: Currently this prevents even auras not caused by lava orcs...
    if (you_worship(GOD_BEOGH) && mons->friendly() && mons->god == GOD_BEOGH)
        return;

    const int base_damage = random2(11);

    // Timescale, like with clouds:
    const int speed = mons->speed > 0 ? mons->speed : 10;
    const int timescaled = max(0, base_damage) * 10 / speed;

    // rF protects:
    const int resist = mons->res_fire();
    const int adjusted_damage = resist_adjust_damage(mons,
                                BEAM_FIRE, resist,
                                timescaled, true);
    // So does AC:
    const int final_damage = max(0, adjusted_damage
                                 - random2(mons->armour_class()));

    if (final_damage > 0)
    {
        if (mons->observable())
        {
            mprf("%s is %s by your radiant heat.",
                 mons->name(DESC_THE).c_str(),
                 (final_damage) > 10 ? "blasted" : "burned");
        }

        behaviour_event(mons, ME_DISTURB, 0, mons->pos());

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "%s %s %d damage from heat.",
             mons->name(DESC_THE).c_str(),
             mons->conj_verb("take").c_str(),
             final_damage);
#endif

        mons->hurt(&you, final_damage, BEAM_MISSILE);

        if (mons->alive() && mons->observable())
            print_wounds(mons);
    }
}
#endif

static spell_type _map_wand_to_mspell(wand_type kind)
{
    switch (kind)
    {
    case WAND_FLAME:           return SPELL_THROW_FLAME;
    case WAND_FROST:           return SPELL_THROW_FROST;
    case WAND_SLOWING:         return SPELL_SLOW;
    case WAND_HASTING:         return SPELL_HASTE;
    case WAND_MAGIC_DARTS:     return SPELL_MAGIC_DART;
    case WAND_HEAL_WOUNDS:     return SPELL_MINOR_HEALING;
    case WAND_PARALYSIS:       return SPELL_PARALYSE;
    case WAND_FIRE:            return SPELL_BOLT_OF_FIRE;
    case WAND_COLD:            return SPELL_BOLT_OF_COLD;
    case WAND_CONFUSION:       return SPELL_CONFUSE;
    case WAND_INVISIBILITY:    return SPELL_INVISIBILITY;
    case WAND_TELEPORTATION:   return SPELL_TELEPORT_OTHER;
    case WAND_LIGHTNING:       return SPELL_LIGHTNING_BOLT;
    case WAND_DRAINING:        return SPELL_BOLT_OF_DRAINING;
    case WAND_DISINTEGRATION:  return SPELL_DISINTEGRATE;
    case WAND_POLYMORPH:       return SPELL_POLYMORPH;
    case WAND_DIGGING:         return SPELL_DIG;
    default:                   return SPELL_NO_SPELL;
    }
}
