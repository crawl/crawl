/*
 *  File:       mon-act.cc
 *  Summary:    Monsters doing stuff (monsters acting).
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"
#include "mon-act.h"

#include "areas.h"
#include "arena.h"
#include "artefact.h"
#include "attitude-change.h"
#include "beam.h"
#include "cloud.h"
#include "coordit.h"
#include "dbg-scan.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "effects.h"
#include "env.h"
#include "map_knowledge.h"
#include "food.h"
#include "fprop.h"
#include "fight.h"
#include "fineff.h"
#include "godpassive.h"
#include "godprayer.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "mapmark.h"
#include "message.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-iter.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mgen_data.h"
#include "coord.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "notes.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "shopping.h" // for item values
#include "state.h"
#include "stuff.h"
#include "terrain.h"
#include "traps.h"
#include "hints.h"
#include "view.h"
#include "shout.h"
#include "viewchar.h"

static bool _handle_pickup(monster* mons);
static void _mons_in_cloud(monster* mons);
static bool _mon_can_move_to_pos(const monster* mons,
                                 const coord_def& delta,
                                 bool just_check = false);
static bool _is_trap_safe(const monster* mons, const coord_def& where,
                          bool just_check = false);
static bool _monster_move(monster* mons);
static spell_type _map_wand_to_mspell(int wand_type);

// [dshaligram] Doesn't need to be extern.
static coord_def mmov;

static const coord_def mon_compass[8] = {
    coord_def(-1,-1), coord_def(0,-1), coord_def(1,-1), coord_def(1,0),
    coord_def(1, 1), coord_def(0, 1), coord_def(-1,1), coord_def(-1,0)
};

static int _compass_idx(const coord_def& mov)
{
    for (int i = 0; i < 8; i++)
        if (mon_compass[i] == mov)
            return (i);
    return (-1);
}

static bool immobile_monster[MAX_MONSTERS];

// A probably needless optimization: convert the C string "just seen" to
// a C++ string just once, instead of twice every time a monster moves.
static const std::string _just_seen("just seen");

static inline bool _mons_natural_regen_roll(monster* mons)
{
    const int regen_rate = mons_natural_regen_rate(mons);
    return (x_chance_in_y(regen_rate, 25));
}

// Do natural regeneration for monster.
static void _monster_regenerate(monster* mons)
{
    if (mons->has_ench(ENCH_SICK) ||
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

    if (monster_descriptor(mons->type, MDSC_REGENERATES)
        || (mons->type == MONS_FIRE_ELEMENTAL
            && (grd(mons->pos()) == DNGN_LAVA
                || cloud_type_at(mons->pos()) == CLOUD_FIRE))

        || (mons->type == MONS_WATER_ELEMENTAL
            && feat_is_watery(grd(mons->pos())))

        || (mons->type == MONS_AIR_ELEMENTAL
            && env.cgrid(mons->pos()) == EMPTY_CLOUD
            && one_chance_in(3))

        || mons->has_ench(ENCH_REGENERATION)

        || mons->has_ench(ENCH_WITHDRAWN)

        || _mons_natural_regen_roll(mons))
    {
        mons->heal(1);
    }
}

static bool _swap_monsters(monster* mover, monster* moved)
{
    // Can't swap with a stationary monster.
    // Although nominally stationary kraken tentacles can be swapped
    // with the main body.
    if (mons_is_stationary(moved)
        && moved->type != MONS_KRAKEN_TENTACLE)
        return (false);

    // Swapping is a purposeful action.
    if (mover->confused())
        return (false);

    // Right now just happens in sanctuary.
    if (!is_sanctuary(mover->pos()) || !is_sanctuary(moved->pos()))
        return (false);

    // A friendly or good-neutral monster moving past a fleeing hostile
    // or neutral monster, or vice versa.
    if (mover->wont_attack() == moved->wont_attack()
        || mons_is_fleeing(mover) == mons_is_fleeing(moved))
    {
        return (false);
    }

    // Don't swap places if the player explicitly ordered their pet to
    // attack monsters.
    if ((mover->friendly() || moved->friendly())
        && you.pet_target != MHITYOU && you.pet_target != MHITNOT)
    {
        return (false);
    }

    if (!mover->can_pass_through(moved->pos())
        || !moved->can_pass_through(mover->pos()))
    {
        return (false);
    }

    if (!monster_habitable_grid(mover, grd(moved->pos()))
        || !monster_habitable_grid(moved, grd(mover->pos())))
    {
        return (false);
    }

    // Okay, we can do the swap.
    const coord_def mover_pos = mover->pos();
    const coord_def moved_pos = moved->pos();

    mover->set_position(moved_pos);
    moved->set_position(mover_pos);

    mgrd(mover->pos()) = mover->mindex();
    mgrd(moved->pos()) = moved->mindex();

    if (you.can_see(mover) && you.can_see(moved))
    {
        mprf("%s and %s swap places.", mover->name(DESC_CAP_THE).c_str(),
             moved->name(DESC_NOCAP_THE).c_str());
    }

    return (true);
}

static bool _do_mon_spell(monster* mons, bolt &beem)
{
    // Shapeshifters don't get spells.
    if (!mons->is_shapeshifter() || !mons->is_actual_spellcaster())
    {
        if (handle_mon_spell(mons, beem))
        {
            mmov.reset();
            return (true);
        }
    }

    return (false);
}

static void _swim_or_move_energy(monster* mon)
{
    const dungeon_feature_type feat = grd(mon->pos());

    // FIXME: Replace check with mons_is_swimming()?
    mon->lose_energy((feat >= DNGN_LAVA && feat <= DNGN_SHALLOW_WATER
                      && !(mon->airborne() || mon->is_wall_clinging())) ? EUT_SWIM
                                                                        : EUT_MOVE);
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
            if (mons_intel(mon) <= I_NORMAL && !mon->wont_attack()
                && mons_genus(mon->type) != mons_genus(ally->type))
            {
                return (false);
            }

            if (mons_has_ranged_attack(ally)
                || mons_has_ranged_spell(ally, true))
            {
                return (true);
            }
        }
        break;
    }
    return (false);
}

// Check whether there's a monster of the same type and alignment adjacent
// to the given monster in at least one of three given directions (relative to
// the monster position).
static bool _allied_monster_at(monster* mon, coord_def a, coord_def b,
                               coord_def c)
{
    std::vector<coord_def> pos;
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

        if (mons_is_stationary(ally))
            continue;

        // Hostile monsters of normal intelligence only move aside for
        // monsters of the same genus.
        if (mons_intel(mon) <= I_NORMAL && !mon->wont_attack()
            && mons_genus(mon->type) != mons_genus(ally->type))
        {
            continue;
        }

        if (mons_aligned(mon, ally))
            return (true);
    }

    return (false);
}

// Altars as well as branch entrances are considered interesting for
// some monster types.
static bool _mon_on_interesting_grid(monster* mon)
{
    // Patrolling shouldn't happen all the time.
    if (one_chance_in(4))
        return (false);

    const dungeon_feature_type feat = grd(mon->pos());

    switch (feat)
    {
    // Holy beings will tend to patrol around altars to the good gods.
    case DNGN_ALTAR_ELYVILON:
        if (!one_chance_in(3))
            return (false);
        // else fall through
    case DNGN_ALTAR_ZIN:
    case DNGN_ALTAR_SHINING_ONE:
        return (mon->is_holy());

    // Orcs will tend to patrol around altars to Beogh, and guard the
    // stairway from and to the Orcish Mines.
    case DNGN_ALTAR_BEOGH:
    case DNGN_ENTER_ORCISH_MINES:
    case DNGN_RETURN_FROM_ORCISH_MINES:
        return (mons_is_native_in_branch(mon, BRANCH_ORCISH_MINES));

    // Same for elves and the Elven Halls.
    case DNGN_ENTER_ELVEN_HALLS:
    case DNGN_RETURN_FROM_ELVEN_HALLS:
        return (mons_is_native_in_branch(mon, BRANCH_ELVEN_HALLS));

    // Same for dwarves and the Dwarf Hall.
    case DNGN_ENTER_DWARF_HALL:
    case DNGN_RETURN_FROM_DWARF_HALL:
        return (mons_is_native_in_branch(mon, BRANCH_DWARF_HALL));

    // Killer bees always return to their hive.
    case DNGN_ENTER_HIVE:
        return (mons_is_native_in_branch(mon, BRANCH_HIVE));

    default:
        return (false);
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

static void _set_mons_move_dir(const monster* mons,
                               coord_def* dir, coord_def* delta)
{
    ASSERT(dir && delta);

    // Some calculations.
    if (mons_class_flag(mons->type, M_BURROWS) && mons->foe == MHITYOU)
    {
        // Boring beetles always move in a straight line in your
        // direction.
        *delta = you.pos() - mons->pos();
    }
    else
        *delta = mons->target - mons->pos();

    // Move the monster.
    *dir = delta->sgn();

    if (mons_is_fleeing(mons) && mons->travel_target != MTRAV_WALL
        && (!mons->friendly()
            || mons->target != you.pos()))
    {
        *dir *= -1;
    }
}

static void _tweak_wall_mmov(const coord_def& monpos)
{
    // The rock worm will try to move along through rock for as long as
    // possible. If the player is walking through a corridor, for example,
    // moving along in the wall beside him is much preferable to actually
    // leaving the wall.
    // This might cause the rock worm to take detours but it still
    // comes off as smarter than otherwise.

    int dir = _compass_idx(mmov);
    ASSERT(dir != -1);

    int count = 0;
    int choice = dir; // stick with mmov if none are good
    for (int i = -1; i <= 1; ++i)
    {
        const int altdir = (dir + i + 8) % 8;
        const coord_def t = monpos + mon_compass[altdir];
        const bool good = in_bounds(t) && feat_is_rock(grd(t))
                          && !feat_is_permarock(grd(t));
        if (good && one_chance_in(++count))
            choice = altdir;
    }
    mmov = mon_compass[choice];
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
                _mon_can_move_to_pos(mons, coord_def(count_x-1, count_y-1));
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
            return (true);
    }

    return (false);
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

    // Make rock worms prefer wall.
    if (mons_wall_shielded(mons) && mons->target != mons->pos() + mmov)
        _tweak_wall_mmov(mons->pos());

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
        && !mons->berserk())
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
                       && !mons->wont_attack()
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
                       && !mons->wont_attack()
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
                           && !mons->wont_attack()
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
                            && !mons->wont_attack()
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
    if (mons->seen_context != _just_seen)
        return;

    mons->seen_context.clear();

    // If the player can't see us, it doesn't matter.
    if (!(mons->flags & MF_WAS_IN_VIEW))
        return;

    const coord_def old_pos  = mons->pos();
    const int       old_dist = grid_distance(you.pos(), old_pos);

    // We're not moving towards the player.
    if (grid_distance(you.pos(), old_pos + mmov) >= old_dist)
    {
        // Give a message if we move back out of view.
        mons->seen_context = _just_seen;
        return;
    }

    // We're already staying in the player's LOS.
    if (you.see_cell(old_pos + mmov))
        return;

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

    // The only way to get closer to the player is to step out of view;
    // give a message so they player isn't confused about its being
    // announced as coming into view but not being seen.
    mons->seen_context = _just_seen;
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
        return (false);
    }

    if (mons_itemuse(mons) < MONUSE_STARTING_EQUIPMENT)
        return (false);

    // Make sure the item actually is a potion.
    if (mitm[mons->inv[MSLOT_POTION]].base_type != OBJ_POTIONS)
        return (false);

    bool rc = false;

    const int potion_idx = mons->inv[MSLOT_POTION];
    item_def& potion = mitm[potion_idx];
    const potion_type ptype = static_cast<potion_type>(potion.sub_type);

    if (mons->can_drink_potion(ptype) && mons->should_drink_potion(ptype))
    {
        const bool was_visible = you.can_see(mons);

        // Drink the potion.
        const item_type_id_state_type id = mons->drink_potion_effect(ptype);

        // Give ID if necessary.
        if (was_visible && id != ID_UNKNOWN_TYPE)
            set_ident_type(OBJ_POTIONS, ptype, id);

        // Remove it from inventory.
        if (dec_mitm_item_quantity(potion_idx, 1))
            mons->inv[MSLOT_POTION] = NON_ITEM;
        else if (is_blood_potion(potion))
            remove_oldest_blood_potion(potion);

        mons->lose_energy(EUT_ITEM);
        rc = true;
    }

    return (rc);
}

static bool _handle_reaching(monster* mons)
{
    bool       ret = false;
    const reach_type range = mons->reach_range();
    actor *foe = mons->get_foe();

    if (!foe || !range)
        return (false);

    if (mons->submerged())
        return (false);

    if (mons_aligned(mons, foe))
        return (false);

    const coord_def foepos(foe->pos());
    const coord_def delta(foepos - mons->pos());
    const int grid_distance(delta.rdist());
    const coord_def middle(mons->pos() + delta / 2);

    if (grid_distance == 2
        // The monster has to be attacking the correct position.
        && mons->target == foepos
        // With a reaching weapon OR ...
        && (range > REACH_KNIGHT
            // ... with a native reaching attack, provided the attack
            // is not on a full diagonal.
            || delta.abs() <= 5)
        // And with no dungeon furniture in the way of the reaching
        // attack; if the middle square is empty, skip the LOS check.
        && (grd(middle) > DNGN_MAX_NONREACH
            || mons->see_cell_no_trans(foepos)))
    {
        ret = true;
        monster_attack_actor(mons, foe, false);

        if (mons->alive())
        {
            // Player saw the item reach.
            item_def *wpn = mons->weapon(0);
            if (wpn && !is_artefact(*wpn) && you.can_see(mons))
                set_ident_flags(*wpn, ISFLAG_KNOW_TYPE);
        }
    }

    return (ret);
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
        || !one_chance_in(3))
    {
        return (false);
    }

    if (mons_itemuse(mons) < MONUSE_STARTING_EQUIPMENT)
        return (false);

    // Make sure the item actually is a scroll.
    if (mitm[mons->inv[MSLOT_SCROLL]].base_type != OBJ_SCROLLS)
        return (false);

    bool                    read        = false;
    item_type_id_state_type ident       = ID_UNKNOWN_TYPE;
    bool                    was_visible = you.can_see(mons);

    // Notice how few cases are actually accounted for here {dlb}:
    const int scroll_type = mitm[mons->inv[MSLOT_SCROLL]].sub_type;
    switch (scroll_type)
    {
    case SCR_TELEPORTATION:
        if (!mons->has_ench(ENCH_TP))
        {
            if (mons->caught() || mons_is_fleeing(mons)
                || mons->pacified())
            {
                simple_monster_message(mons, " reads a scroll.");
                monster_teleport(mons, false);
                read  = true;
                ident = ID_KNOWN_TYPE;
            }
        }
        break;

    case SCR_BLINKING:
        if (mons->caught() || mons_is_fleeing(mons)
            || mons->pacified())
        {
            if (mons_near(mons))
            {
                simple_monster_message(mons, " reads a scroll.");
                monster_blink(mons);
                read  = true;
                ident = ID_KNOWN_TYPE;
            }
        }
        break;

    case SCR_SUMMONING:
        if (mons_near(mons))
        {
            simple_monster_message(mons, " reads a scroll.");
            const int mon = create_monster(
                mgen_data(MONS_ABOMINATION_SMALL, SAME_ATTITUDE(mons),
                          mons, 0, 0, mons->pos(), mons->foe,
                          MG_FORCE_BEH));

            read = true;
            if (mon != -1)
            {
                if (you.can_see(&menv[mon]))
                {
                    mprf("%s appears!", menv[mon].name(DESC_CAP_A).c_str());
                    ident = ID_KNOWN_TYPE;
                }
                player_angers_monster(&menv[mon]);
            }
            else if (you.can_see(mons))
                canned_msg(MSG_NOTHING_HAPPENS);
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

    // subsection: evocation skill and shield skill equivalents for monsters
    int evoc_num = 1;
    int evoc_den = 1;
    if (mons->type == MONS_DEEP_DWARF_ARTIFICER)
    {
        evoc_num = 2;
    }
    int shield_num = 1;
    int shield_den = 1;

    int shield_base = 1;
    if (mons->inv[MSLOT_SHIELD] != NON_ITEM)
    {
        item_def *shield = mons->mslot_item(MSLOT_SHIELD);
        switch (shield->sub_type)
        {
        case ARM_BUCKLER:
            shield_base = 5;
            break;
        case ARM_SHIELD:
            shield_base = 3;
            break;
        case ARM_LARGE_SHIELD:
            shield_base = 2;
            break;
        default:
            shield_base = 1;
        }
    }

    const int power_base = (mons->hit_dice * evoc_num) / evoc_den;
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
    beem.beam_source  = mons->mindex();
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

static void _rod_fired_pre(monster* mons, bool nice_spell)
{
    if (!nice_spell)
        make_mons_stop_fleeing(mons);

    if ((!simple_monster_message(mons, " zaps a rod.")) &&
        (!silenced(you.pos())))
            mpr("You hear a zap.", MSGCH_SOUND);
}

static bool _rod_fired_post(monster* mons, item_def &rod, int idx, bolt &beem,
    int rate, bool was_visible)
{
    rod.plus -= rate;
    dprf("rod charge: %d , %d", rod.plus, rod.plus2);

    if (was_visible)
    {
        if (!beem.is_enchantment() || beem.obvious_effect)
            set_ident_type(OBJ_STAVES, mitm[idx].sub_type, ID_KNOWN_TYPE);
        else
            set_ident_type(OBJ_STAVES, mitm[idx].sub_type, ID_MON_TRIED_TYPE);
    }

    mons->lose_energy(EUT_ITEM);
    return true;
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
    if (!item_is_rod(rod))
        return (false);

    // was the player visible when we started?
    bool was_visible = you.can_see(mons);

    int rate;
    int overriding_power = 0;
    bool nice_spell     = false;
    bool check_validity = true;
    bool is_direct_effect = false;
    spell_type mzap     = SPELL_NO_SPELL;
    switch (rod.sub_type)
    {
    case STAFF_STRIKING:
        if ((rod.plus > 100) && (mons->foe_distance() >= 2))
        {
            mzap = SPELL_STRIKING;
            rate = 100;
        }
        break;
    case STAFF_SMITING:
        if (rod.plus > 400)
        {
            mzap = SPELL_SMITING;
            overriding_power = 1;
            nice_spell = true;
            is_direct_effect = true;
            rate = 400;
        }
        break;
    case STAFF_DESTRUCTION_I:
        if (rod.plus > 600)
        {
            if (mons->foe_distance() > 2)
            {
                mzap = SPELL_FIREBALL;
                rate = 600;
            }
        }
        else if (rod.plus > 500)
        {
            mzap = SPELL_BOLT_OF_FIRE;
            rate = 500;
        }
        else if (rod.plus > 200)
        {
            mzap = SPELL_THROW_FLAME;
            rate = 200;
        }
        break;
    case STAFF_DESTRUCTION_II:
        if (rod.plus > 700)
        {
            if (mons->foe_distance() > 2)
            {
                mzap = SPELL_FREEZING_CLOUD;
                rate = 700;
            }
        }
        else if (rod.plus > 400)
        {
            mzap = SPELL_BOLT_OF_COLD;
            rate = 400;
        }
        else if (rod.plus > 200)
        {
            mzap = SPELL_THROW_FROST;
            rate = 200;
        }
        break;
    case STAFF_DESTRUCTION_III:
        if (rod.plus > 600)
        {
            if (mons->foe_distance() > 2)
                mzap = SPELL_FIREBALL;
            if (one_chance_in(2))
                mzap = SPELL_IRON_SHOT;
            else
                mzap = SPELL_LIGHTNING_BOLT;
            rate = 600;
        }
        break;
    case STAFF_DESTRUCTION_IV:
        if (rod.plus > 1000)
        {
            if (one_chance_in(2))
                mzap = SPELL_BOLT_OF_MAGMA;
            else
                mzap = SPELL_BOLT_OF_COLD;
            rate = 500;
        }
        else if (rod.plus > 500)
        {
            const int choice = random2(3);
            switch (choice)
            {
            case 0:
                mzap = SPELL_BOLT_OF_MAGMA;
                rate = 500;
                break;
            case 1:
                mzap = SPELL_BOLT_OF_COLD;
                rate = 500;
                break;
            default:
                mzap = SPELL_BOLT_OF_INACCURACY;
                rate = 300;
                break;
            }
        }
        else if (rod.plus > 300)
        {
            mzap = SPELL_BOLT_OF_INACCURACY;
            rate = 300;
        }
        break;
    case STAFF_DEMONOLOGY: // ouch
        if (rod.plus > 500)
        {
            mzap = SPELL_SUMMON_DEMON;
            _rod_fired_pre(mons, nice_spell);
            dprf("mon-act:_handle_rod():SPELL_SUMMON_DEMON");
            mons_cast(mons, beem, mzap, false);
            _rod_fired_post(mons, rod, weapon, beem, 500, was_visible);
            return true;
        }
        else if (rod.plus > 300)
        {
            mzap = SPELL_CALL_IMP;
            _rod_fired_pre(mons, nice_spell);
            dprf("mon-act:_handle_rod():SPELL_CALL_IMP");
            mons_cast(mons, beem, mzap, false);
            _rod_fired_post(mons, rod, weapon, beem, 300, was_visible);
            return true;
        }
        break;
    case STAFF_VENOM:
        if (rod.plus > 600)
        {
            if (mons->foe_distance() > 2)
                mzap = SPELL_POISONOUS_CLOUD;
            else
                mzap = SPELL_POISON_ARROW;
            rate = 600;
        }
        else if (rod.plus > 500)
        {
            mzap = SPELL_VENOM_BOLT;
            rate = 500;
        }
        break;
    case STAFF_SPELL_SUMMONING:
        if (rod.plus > 600)
        {
            mzap = SPELL_SUMMON_SWARM;
            _rod_fired_pre(mons, nice_spell);
            dprf("mon-act:_handle_rod():SPELL_SUMMON_SWARM");
            mons_cast(mons, beem, mzap, false);
            _rod_fired_post(mons, rod, weapon, beem, 600, was_visible);
            return true;
        }
        else if (rod.plus > 400)
        { // could be implemented as a mon-cast.cc spell with this code
            mzap = SPELL_SUMMON_ELEMENTAL;
            _rod_fired_pre(mons, nice_spell);
            dprf("mon-act:_handle_rod():SPELL_SUMMON_ELEMENTAL");
            const int duration = std::min(2 + mons->hit_dice / 10, 6);
            const monster_type summon = static_cast<monster_type>(
                  random_choose(
                           MONS_EARTH_ELEMENTAL, MONS_FIRE_ELEMENTAL,
                           MONS_AIR_ELEMENTAL, MONS_WATER_ELEMENTAL,
                           -1));
            create_monster(
                mgen_data::hostile_at(summon, mons->name(DESC_NOCAP_A),
                true, duration, 0, mons->pos()));
            _rod_fired_post(mons, rod, weapon, beem, 400, was_visible);
            return true;
       }
       break;
    case STAFF_WARDING: // all temporary self-status effects
    default:
        return false;
        break;
    }

    if (mzap == SPELL_NO_SPELL)
        return false;

    bool zap = false;

    // set up the beam
    int power = _generate_rod_power(mons, overriding_power);
    if (power < 1)
        power = 1;

    dprf("using rod with power %d", power);

    bolt theBeam = mons_spells(mons, mzap, power, check_validity);
    beem         = _generate_item_beem(beem, theBeam, mons);

    if (mons->confused())
    {
        beem.target = dgn_random_point_from(mons->pos(), LOS_RADIUS);
        if (beem.target.origin())
            return (false);
        zap = true;
    }
    else if (!nice_spell)
    {
        fire_tracer(mons, beem);
        zap = mons_should_fire(beem);
    }

    if (is_direct_effect)
    {
        _rod_fired_pre(mons, nice_spell);
        direct_effect(mons, mzap, beem, &you);
        return _rod_fired_post(mons, rod, weapon, beem, rate, was_visible);
    }
    else if (nice_spell || zap)
    {
        _rod_fired_pre(mons, nice_spell);
        beem.is_tracer = false;
        beem.fire();
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
        return (false);
    }

    if (mons_itemuse(mons) < MONUSE_STARTING_EQUIPMENT)
        return (false);

    if (mons->inv[MSLOT_WEAPON] != NON_ITEM
       && item_is_rod(mitm[mons->inv[MSLOT_WEAPON]]))
    {
        return (_handle_rod(mons, beem));
    }

    if (mons->inv[MSLOT_WAND] == NON_ITEM
        || mitm[mons->inv[MSLOT_WAND]].plus <= 0)
    {
        return (false);
    }

    // Make sure the item actually is a wand.
    if (mitm[mons->inv[MSLOT_WAND]].base_type != OBJ_WANDS)
        return (false);

    bool niceWand    = false;
    bool zap         = false;
    bool was_visible = you.can_see(mons);

    item_def &wand(mitm[mons->inv[MSLOT_WAND]]);

    // map wand type to monster spell type
    const spell_type mzap = _map_wand_to_mspell(wand.sub_type);
    if (mzap == SPELL_NO_SPELL)
        return (false);

    // set up the beam
    int power         = 30 + mons->hit_dice;
    bolt theBeam      = mons_spells(mons, mzap, power);
    beem = _generate_item_beem(beem, theBeam, mons);

#ifdef HISCORE_WEAPON_DETAIL
    beem.aux_source =
        wand.name(DESC_QUALNAME, false, true, false, false);
#else
    beem.aux_source =
        wand.name(DESC_QUALNAME, false, true, false, false,
                  ISFLAG_KNOW_CURSE | ISFLAG_KNOW_PLUSES);
#endif

    const int wand_type = wand.sub_type;
    switch (wand_type)
    {
    case WAND_DISINTEGRATION:
        // Dial down damage from wands of disintegration, since
        // disintegration beams can do large amounts of damage.
        beem.damage.size = beem.damage.size * 2 / 3;
        break;

    case WAND_ENSLAVEMENT:
    case WAND_DIGGING:
    case WAND_RANDOM_EFFECTS:
        // These have been deemed "too tricky" at this time {dlb}:
        return (false);

    case WAND_POLYMORPH_OTHER:
         // Monsters can be very trigger happy with wands, reduce this
         // for polymorph.
         if (!one_chance_in(5))
             return (false);
         break;

    // These are wands that monsters will aim at themselves {dlb}:
    case WAND_HASTING:
        if (!mons->has_ench(ENCH_HASTE))
        {
            beem.target = mons->pos();
            niceWand = true;
            break;
        }
        return (false);

    case WAND_HEALING:
        if (mons->hit_points <= mons->max_hit_points / 2)
        {
            beem.target = mons->pos();
            niceWand = true;
            break;
        }
        return (false);

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
        return (false);

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
        return (false);
    }

    if (mons->confused())
    {
        beem.target = dgn_random_point_from(mons->pos(), LOS_RADIUS);
        if (beem.target.origin())
            return (false);
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

        if (!simple_monster_message(mons, " zaps a wand."))
        {
            if (!silenced(you.pos()))
                mpr("You hear a zap.", MSGCH_SOUND);
        }

        // charge expenditure {dlb}
        wand.plus--;
        beem.is_tracer = false;
        beem.fire();

        if (was_visible)
        {
            if (niceWand || !beem.is_enchantment() || beem.obvious_effect)
                set_ident_type(OBJ_WANDS, wand_type, ID_KNOWN_TYPE);
            else
                set_ident_type(OBJ_WANDS, wand_type, ID_MON_TRIED_TYPE);

            // Increment zap count.
            if (wand.plus2 >= 0)
                wand.plus2++;
        }

        mons->lose_energy(EUT_ITEM);

        return (true);
    }

    return (false);
}

static void _setup_generic_throw(monster* mons, struct bolt &pbolt)
{
    // FIXME we should use a sensible range here
    pbolt.range = LOS_RADIUS;
    pbolt.beam_source = mons->mindex();

    pbolt.glyph   = dchar_glyph(DCHAR_FIRED_MISSILE);
    pbolt.flavour = BEAM_MISSILE;
    pbolt.thrower = KILL_MON_MISSILE;
    pbolt.aux_source.clear();
    pbolt.is_beam = false;
}

// msl is the item index of the thrown missile (or weapon).
static bool _mons_throw(monster* mons, struct bolt &pbolt, int msl)
{
    std::string ammo_name;

    bool returning = false;

    int baseHit = 0, baseDam = 0;       // from thrown or ammo
    int ammoHitBonus = 0, ammoDamBonus = 0;     // from thrown or ammo
    int lnchHitBonus = 0, lnchDamBonus = 0;     // special add from launcher
    int exHitBonus   = 0, exDamBonus = 0; // 'extra' bonus from skill/dex/str
    int lnchBaseDam  = 0;

    int hitMult = 0;
    int damMult  = 0;
    int diceMult = 100;

    // Some initial convenience & initializations.
    const int wepClass  = mitm[msl].base_type;
    const int wepType   = mitm[msl].sub_type;

    const int weapon    = mons->inv[MSLOT_WEAPON];
    const int lnchType  = (weapon != NON_ITEM) ? mitm[weapon].sub_type : 0;

    mon_inv_type slot = get_mon_equip_slot(mons, mitm[msl]);
    ASSERT(slot != NUM_MONSTER_SLOTS);

    const bool skilled = mons->flags & MF_FIGHTER;
    const bool archer  = mons->flags & MF_ARCHER;

    mons->lose_energy(EUT_MISSILE);
    const int throw_energy = mons->action_energy(EUT_MISSILE);

    // Dropping item copy, since the launched item might be different.
    item_def item = mitm[msl];
    item.quantity = 1;
    if (mons->friendly())
        item.flags |= ISFLAG_DROPPED_BY_ALLY;

    // FIXME we should actually determine a sensible range here
    pbolt.range         = LOS_RADIUS;

    if (setup_missile_beam(mons, pbolt, item, ammo_name, returning))
        return (false);

    pbolt.aimed_at_spot = returning;

    const launch_retval projected =
        is_launched(mons, mons->mslot_item(MSLOT_WEAPON),
                    mitm[msl]);

    // extract launcher bonuses due to magic
    if (projected == LRET_LAUNCHED)
    {
        lnchHitBonus = mitm[weapon].plus;
        lnchDamBonus = mitm[weapon].plus2;
        lnchBaseDam  = property(mitm[weapon], PWPN_DAMAGE);
    }

    // extract weapon/ammo bonuses due to magic
    ammoHitBonus = item.plus;
    ammoDamBonus = item.plus2;

    // Archers get a boost from their melee attack.
    if (archer)
    {
        const mon_attack_def attk = mons_attack_spec(mons, 0);
        if (attk.type == AT_SHOOT)
        {
            if (projected == LRET_THROWN && wepClass == OBJ_MISSILES)
                ammoHitBonus += random2avg(attk.damage, 2);
            else
                ammoDamBonus += random2avg(attk.damage, 2);
        }
    }

    if (projected == LRET_THROWN)
    {
        // Darts are easy.
        if (wepClass == OBJ_MISSILES && wepType == MI_DART)
        {
            baseHit = 11;
            hitMult = 40;
            damMult = 25;
        }
        else
        {
            baseHit = 6;
            hitMult = 30;
            damMult = 25;
        }

        baseDam = property(item, PWPN_DAMAGE);

        if (wepClass == OBJ_MISSILES)   // throw missile
        {
            // ammo damage needs adjusting here - OBJ_MISSILES
            // don't get separate tohit/damage bonuses!
            ammoDamBonus = ammoHitBonus;

            // [dshaligram] Thrown stones/darts do only half the damage of
            // launched stones/darts. This matches 4.0 behaviour.
            if (wepType == MI_DART || wepType == MI_STONE
                || wepType == MI_SLING_BULLET)
            {
                baseDam = div_rand_round(baseDam, 2);
            }
        }

        // give monster "skill" bonuses based on HD
        exHitBonus = (hitMult * mons->hit_dice) / 10 + 1;
        exDamBonus = (damMult * mons->hit_dice) / 10 + 1;
    }

    // Monsters no longer gain unfair advantages with weapons of
    // fire/ice and incorrect ammo.  They now have the same restrictions
    // as players.

          int  bow_brand  = SPWPN_NORMAL;
    const int  ammo_brand = get_ammo_brand(item);

    if (projected == LRET_LAUNCHED)
    {
        bow_brand = get_weapon_brand(mitm[weapon]);

        switch (lnchType)
        {
        case WPN_BLOWGUN:
            baseHit = 12;
            hitMult = 60;
            damMult = 0;
            lnchDamBonus = 0;
            break;
        case WPN_BOW:
        case WPN_LONGBOW:
            baseHit = 0;
            hitMult = 60;
            damMult = 35;
            // monsters get half the launcher damage bonus,
            // which is about as fair as I can figure it.
            lnchDamBonus = (lnchDamBonus + 1) / 2;
            break;
        case WPN_CROSSBOW:
            baseHit = 4;
            hitMult = 70;
            damMult = 30;
            break;
        case WPN_SLING:
            baseHit = 10;
            hitMult = 40;
            damMult = 20;
            // monsters get half the launcher damage bonus,
            // which is about as fair as I can figure it.
            lnchDamBonus /= 2;
            break;
        }

        // Launcher is now more important than ammo for base damage.
        baseDam = property(item, PWPN_DAMAGE);
        if (lnchBaseDam)
            baseDam = lnchBaseDam + random2(1 + baseDam);

        // missiles don't have pluses2;  use hit bonus
        ammoDamBonus = ammoHitBonus;

        exHitBonus = (hitMult * mons->hit_dice) / 10 + 1;
        exDamBonus = (damMult * mons->hit_dice) / 10 + 1;

        if (!baseDam && elemental_missile_beam(bow_brand, ammo_brand))
            baseDam = 4;

        // [dshaligram] This is a horrible hack - we force beam.cc to
        // consider this beam "needle-like".
        if (wepClass == OBJ_MISSILES && wepType == MI_NEEDLE)
            pbolt.ench_power = AUTOMATIC_HIT;

        // elven bow w/ elven arrow, also orcish
        if (get_equip_race(mitm[weapon])
                == get_equip_race(mitm[msl]))
        {
            baseHit++;
            baseDam++;

            if (get_equip_race(mitm[weapon]) == ISFLAG_ELVEN)
                pbolt.hit++;
        }

        // Vorpal brand increases damage dice size.
        if (bow_brand == SPWPN_VORPAL)
            diceMult = diceMult * 130 / 100;

        // As do steel ammo.
        if (ammo_brand == SPMSL_STEEL)
            diceMult = diceMult * 150 / 100;

        // Note: we already have throw_energy taken off.  -- bwr
        int speed_delta = 0;
        if (lnchType == WPN_CROSSBOW)
        {
            if (bow_brand == SPWPN_SPEED)
            {
                // Speed crossbows take 50% less time to use than
                // ordinary crossbows.
                speed_delta = div_rand_round(throw_energy * 2, 5);
            }
            else
            {
                // Ordinary crossbows take 20% more time to use
                // than ordinary bows.
                speed_delta = -div_rand_round(throw_energy, 5);
            }
        }
        else if (bow_brand == SPWPN_SPEED)
        {
            // Speed bows take 50% less time to use than
            // ordinary bows.
            speed_delta = div_rand_round(throw_energy, 2);
        }

        mons->speed_increment += speed_delta;
    }

    // Chaos overides flame and frost
    if (pbolt.flavour != BEAM_MISSILE)
    {
        baseHit    += 2;
        exDamBonus += 6;
    }

    // monster intelligence bonus
    if (mons_intel(mons) == I_HIGH)
        exHitBonus += 10;

    // Identify before throwing, so we don't get different
    // messages for first and subsequent missiles.
    if (mons->observable())
    {
        if (projected == LRET_LAUNCHED
               && item_type_known(mitm[weapon])
            || projected == LRET_THROWN
               && mitm[msl].base_type == OBJ_MISSILES)
        {
            set_ident_flags(mitm[msl], ISFLAG_KNOW_TYPE);
            set_ident_flags(item, ISFLAG_KNOW_TYPE);
        }
    }

    // Now, if a monster is, for some reason, throwing something really
    // stupid, it will have baseHit of 0 and damage of 0.  Ah well.
    std::string msg = mons->name(DESC_CAP_THE);
    msg += ((projected == LRET_LAUNCHED) ? " shoots " : " throws ");

    if (!pbolt.name.empty() && projected == LRET_LAUNCHED)
        msg += article_a(pbolt.name);
    else
    {
        // build shoot message
        msg += item.name(DESC_NOCAP_A, false, false, false);

        // build beam name
        pbolt.name = item.name(DESC_PLAIN, false, false, false);
    }
    msg += ".";

    if (mons->observable())
        mpr(msg.c_str());

    throw_noise(mons, pbolt, item);

    // Store misled values here, as the setting up of the aux source
    // will use the wrong monster name.
    int misled = you.duration[DUR_MISLED];
    you.duration[DUR_MISLED] = 0;

    // [dshaligram] When changing bolt names here, you must edit
    // hiscores.cc (scorefile_entry::terse_missile_cause()) to match.
    if (projected == LRET_LAUNCHED)
    {
        pbolt.aux_source = make_stringf("Shot with a%s %s by %s",
                 (is_vowel(pbolt.name[0]) ? "n" : ""), pbolt.name.c_str(),
                 mons->name(DESC_NOCAP_A).c_str());
    }
    else
    {
        pbolt.aux_source = make_stringf("Hit by a%s %s thrown by %s",
                 (is_vowel(pbolt.name[0]) ? "n" : ""), pbolt.name.c_str(),
                 mons->name(DESC_NOCAP_A).c_str());
    }

    // And restore it here.
    you.duration[DUR_MISLED] = misled;

    // Add everything up.
    pbolt.hit = baseHit + random2avg(exHitBonus, 2) + ammoHitBonus;
    pbolt.damage =
        dice_def(1, baseDam + random2avg(exDamBonus, 2) + ammoDamBonus);

    if (projected == LRET_LAUNCHED)
    {
        pbolt.damage.size += lnchDamBonus;
        pbolt.hit += lnchHitBonus;
    }
    pbolt.damage.size = diceMult * pbolt.damage.size / 100;

    if (mons->has_ench(ENCH_BATTLE_FRENZY))
    {
        const mon_enchant ench = mons->get_ench(ENCH_BATTLE_FRENZY);

#ifdef DEBUG_DIAGNOSTICS
        const dice_def orig_damage = pbolt.damage;
#endif

        pbolt.damage.size = pbolt.damage.size * (115 + ench.degree * 15) / 100;

#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "%s frenzy damage: %dd%d -> %dd%d",
             mons->name(DESC_PLAIN).c_str(),
             orig_damage.num, orig_damage.size,
             pbolt.damage.num, pbolt.damage.size);
#endif
    }

    // Skilled archers get better to-hit and damage.
    if (skilled)
    {
        pbolt.hit         = pbolt.hit * 120 / 100;
        pbolt.damage.size = pbolt.damage.size * 120 / 100;
    }

    scale_dice(pbolt.damage);

    // decrease inventory
    bool really_returns;
    if (returning && !one_chance_in(mons_power(mons->type) + 3))
        really_returns = true;
    else
        really_returns = false;

    pbolt.drop_item = !really_returns;

    // Redraw the screen before firing, in case the monster just
    // came into view and the screen hasn't been updated yet.
    viewwindow();
    pbolt.fire();

    // The item can be destroyed before returning.
    if (really_returns && thrown_object_destroyed(&item, pbolt.target))
    {
        really_returns = false;
    }

    if (really_returns)
    {
        // Fire beam in reverse.
        pbolt.setup_retrace();
        viewwindow();
        pbolt.fire();
        msg::stream << "The weapon returns "
                    << (you.can_see(mons)?
                          ("to " + mons->name(DESC_NOCAP_THE))
                        : "whence it came from")
                    << "!" << std::endl;

        // Player saw the item return.
        if (!is_artefact(item))
        {
            // Since this only happens for non-artefacts, also mark properties
            // as known.
            set_ident_flags(mitm[msl],
                            ISFLAG_KNOW_TYPE | ISFLAG_KNOW_PROPERTIES);
        }
    }
    else if (dec_mitm_item_quantity(msl, 1))
        mons->inv[slot] = NON_ITEM;

    if (pbolt.special_explosion != NULL)
        delete pbolt.special_explosion;

    return (true);
}

static bool _mons_has_launcher(const monster* mons)
{
    for (int i = MSLOT_WEAPON; i <= MSLOT_ALT_WEAPON; ++i)
    {
        if (item_def *item = mons->mslot_item(static_cast<mon_inv_type>(i)))
        {
            if (is_range_weapon(*item))
                return (true);
        }
    }
    return (false);
}

//---------------------------------------------------------------
//
// handle_throw
//
// Give the monster a chance to throw something. Returns true if
// the monster hurled.
//
//---------------------------------------------------------------
static bool _handle_throw(monster* mons, bolt & beem)
{
    // Yes, there is a logic to this ordering {dlb}:
    if (mons->incapacitated()
        || mons->asleep()
        || mons->submerged())
    {
        return (false);
    }

    if (mons_itemuse(mons) < MONUSE_STARTING_EQUIPMENT)
        return (false);

    const bool archer = mons->flags & MF_ARCHER;

    // Highly-specialised archers are more likely to shoot than talk. (?)
    if (one_chance_in(archer? 9 : 5))
        return (false);

    // Don't allow offscreen throwing for now.
    if (mons->foe == MHITYOU && !mons_near(mons))
        return (false);

    // Monsters won't shoot in melee range, largely for balance reasons.
    // Specialist archers are an exception to this rule.
    if (!archer && adjacent(beem.target, mons->pos()))
        return (false);

    // If the monster is a spellcaster, don't bother throwing stuff.
    // Exception: Spellcasters that already start out with some kind
    // ranged weapon. Seeing how monsters are disallowed from picking
    // up launchers if they have ranged spells, this will only apply
    // to very few monsters.
    if (mons_has_ranged_spell(mons, true, false)
        && !_mons_has_launcher(mons))
    {
        return (false);
    }

    // Greatly lowered chances if the monster is fleeing or pacified and
    // leaving the level.
    if ((mons_is_fleeing(mons) || mons->pacified())
        && !one_chance_in(8))
    {
        return (false);
    }

    item_def *launcher = NULL;
    const item_def *weapon = NULL;
    const int mon_item = mons_pick_best_missile(mons, &launcher);

    if (mon_item == NON_ITEM || !mitm[mon_item].defined())
        return (false);

    if (player_or_mon_in_sanct(mons))
        return (false);

    item_def *missile = &mitm[mon_item];

    // Throwing a net at a target that is already caught would be
    // completely useless, so bail out.
    const actor *act = actor_at(beem.target);
    if (missile->base_type == OBJ_MISSILES
        && missile->sub_type == MI_THROWING_NET
        && act && act->caught())
    {
        return (false);
    }

    // If the attack needs a launcher that we can't wield, bail out.
    if (launcher)
    {
        weapon = mons->mslot_item(MSLOT_WEAPON);
        if (weapon && weapon != launcher && weapon->cursed())
            return (false);
    }

    // Ok, we'll try it.
    _setup_generic_throw(mons, beem);

    // Set fake damage for the tracer.
    beem.damage = dice_def(10, 10);

    // Set item for tracer, even though it probably won't be used
    beem.item = missile;

    // Fire tracer.
    fire_tracer(mons, beem);

    // Clear fake damage (will be set correctly in mons_throw).
    beem.damage = 0;

    // Good idea?
    if (mons_should_fire(beem))
    {
        // Monsters shouldn't shoot if fleeing, so let them "turn to attack".
        make_mons_stop_fleeing(mons);

        if (launcher && launcher != weapon)
            mons->swap_weapons();

        beem.name.clear();
        return (_mons_throw(mons, beem, mon_item));
    }

    return (false);
}

// Give the monster its action energy (aka speed_increment).
static void _monster_add_energy(monster* mons)
{
    if (mons->speed > 0)
    {
        // Randomise to make counting off monster moves harder:
        const int energy_gained =
            std::max(1, div_rand_round(mons->speed * you.time_taken, 10));
        mons->speed_increment += energy_gained;
    }
}

#ifdef DEBUG
#    define DEBUG_ENERGY_USE(problem) \
    if (mons->speed_increment == old_energy && mons->alive()) \
             mprf(MSGCH_DIAGNOSTICS, \
                  problem " for monster '%s' consumed no energy", \
                  mons->name(DESC_PLAIN).c_str(), true);
#else
#    define DEBUG_ENERGY_USE(problem) ((void) 0)
#endif

void handle_monster_move(monster* mons)
{
    mons->hit_points = std::min(mons->max_hit_points,
                                   mons->hit_points);

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
        _mons_in_cloud(mons);

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
    if (mons->foe_memory > 0)
        mons->foe_memory--;

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

    monsterentry* entry = get_monster_data(mons->type);
    if (!entry)
        return;

    int old_energy      = INT_MAX;
    int non_move_energy = std::min(entry->energy_usage.move,
                                   entry->energy_usage.swim);

#ifdef DEBUG_MONS_SCAN
    bool monster_was_floating = mgrd(mons->pos()) != mons->mindex();
#endif
    coord_def old_pos = mons->pos();

    coord_def kraken_last_update = mons->pos();
    bool just_once = true;
    while (mons->has_action_energy())
    {
        if (just_once)
        {
            move_demon_tentacle(mons);
            just_once = false;
        }

        // The continues & breaks are WRT this.
        if (!mons->alive())
            break;

        if (old_pos != mons->pos()
            && mons_base_type(mons) == MONS_KRAKEN)
        {
            move_kraken_tentacles(mons);
            kraken_last_update = mons->pos();
        }

        old_pos = mons->pos();

#ifdef DEBUG_MONS_SCAN
        if (!monster_was_floating
            && mgrd(mons->pos()) != mons->mindex())
        {
            mprf(MSGCH_ERROR, "Monster %s became detached from mgrd "
                              "in handle_monster_move() loop",
                 mons->name(DESC_PLAIN, true).c_str());
            mpr("[[[[[[[[[[[[[[[[[[", MSGCH_WARN);
            debug_mons_scan();
            mpr("]]]]]]]]]]]]]]]]]]", MSGCH_WARN);
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

        if (mons->speed_increment >= old_energy)
        {
#ifdef DEBUG
            if (mons->speed_increment == old_energy)
            {
                mprf(MSGCH_DIAGNOSTICS, "'%s' has same energy as last loop",
                     mons->name(DESC_PLAIN, true).c_str());
            }
            else
            {
                mprf(MSGCH_DIAGNOSTICS, "'%s' has MORE energy than last loop",
                     mons->name(DESC_PLAIN, true).c_str());
            }
#endif
            mons->speed_increment = old_energy - 10;
            old_energy               = mons->speed_increment;
            continue;
        }
        old_energy = mons->speed_increment;

        if (mons_is_projectile(mons->type))
        {
            if (iood_act(*mons))
                return;
            mons->lose_energy(EUT_MOVE);
            continue;
        }

        mons->shield_blocks = 0;

        const int  cloud_num   = env.cgrid(mons->pos());
        const bool avoid_cloud = mons_avoids_cloud(mons, cloud_num);

        _mons_in_cloud(mons);
        if (!mons->alive())
            break;

        slime_wall_damage(mons, speed_to_duration(mons->speed));
        if (!mons->alive())
            break;

        if (mons->type == MONS_TIAMAT && one_chance_in(3))
            draconian_change_colour(mons);

        _monster_regenerate(mons);

        if (mons->cannot_act()
            || mons->type == MONS_SIXFIRHY // these move only 4 of 12 turns
               && ++mons->number / 4 % 3 != 2)  // but are not helpless
        {
            mons->speed_increment -= non_move_energy;
            continue;
        }

        handle_behaviour(mons);

        // handle_behaviour() could make the monster leave the level.
        if (!mons->alive())
            break;

        ASSERT(!crawl_state.game_is_arena() || mons->foe != MHITYOU);
        ASSERT(in_bounds(mons->target) || mons->target.origin());

        // Submerging monsters will hide from clouds.
        if (avoid_cloud
            && monster_can_submerge(mons, grd(mons->pos()))
            && !mons->caught()
            && !mons->submerged())
        {
            mons->add_ench(ENCH_SUBMERGED);
            mons->speed_increment -= ENERGY_SUBMERGE(entry);
            continue;
        }

        if (mons->speed >= 100)
        {
            mons->speed_increment -= non_move_energy;
            continue;
        }

        if (igrd(mons->pos()) != NON_ITEM
            && (mons_itemuse(mons) >= MONUSE_WEAPONS_ARMOUR
                || mons_itemeat(mons) != MONEAT_NOTHING))
        {
            // Keep neutral and charmed monsters from picking up stuff.
            // Same for friendlies if friendly_pickup is set to "none".
            if (!mons->neutral() && !mons->has_ench(ENCH_CHARM)
                || (you.religion == GOD_JIYVA && mons_is_slime(mons))
                && (!mons->friendly()
                    || you.friendly_pickup != FRIENDLY_PICKUP_NONE))
            {
                if (_handle_pickup(mons))
                {
                    DEBUG_ENERGY_USE("handle_pickup()");
                    continue;
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
                    // Don't unsubmerge if the monster is avoiding the
                    // cloud on top of the water.
                    if (avoid_cloud)
                    {
                        mons->speed_increment -= non_move_energy;
                        continue;
                    }

                    if (!mons->del_ench(ENCH_SUBMERGED))
                    {
                        // Couldn't unsubmerge.
                        mons->speed_increment -= non_move_energy;
                        continue;
                    }
                }
                mons->behaviour = BEH_SEEK;
            }
            else
            {
                mons->speed_increment -= non_move_energy;
                continue;
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
                mmov.reset();
                int pfound = 0;
                for (adjacent_iterator ai(mons->pos(), false); ai; ++ai)
                    if (mons->can_pass_through(*ai))
                        if (one_chance_in(++pfound))
                            mmov = *ai - mons->pos();

                // OK, mmov determined.
                const coord_def newcell = mmov + mons->pos();
                monster* enemy = monster_at(newcell);
                if (enemy
                    && newcell != mons->pos()
                    && !is_sanctuary(mons->pos()))
                {
                    if (monsters_fight(mons, enemy))
                    {
                        mmov.reset();
                        DEBUG_ENERGY_USE("monsters_fight()");
                        continue;
                    }
                    else
                    {
                        // FIXME: None of these work!
                        // Instead run away!
                        if (mons->add_ench(mon_enchant(ENCH_FEAR)))
                        {
                            behaviour_event(mons, ME_SCARE,
                                            MHITNOT, newcell);
                        }
                        break;
                    }
                }
            }
        }
        mon_nearby_ability(mons);

        if (!mons->asleep() && !mons_is_wandering(mons)
            && !mons->withdrawn()
            // Berserking monsters are limited to running up and
            // hitting their foes.
            && !mons->berserk()
                // Slime creatures can split while wandering or resting.
                || mons->type == MONS_SLIME_CREATURE)
        {
            bolt beem;

            beem.source      = mons->pos();
            beem.target      = mons->target;
            beem.beam_source = mons->mindex();

            // Prevents unfriendlies from nuking you from offscreen.
            // How nice!
            const bool friendly_or_near =
                mons->friendly() || mons->near_foe();
            if (friendly_or_near
                || mons->type == MONS_TEST_SPAWNER
                // Slime creatures can split when offscreen.
                || mons->type == MONS_SLIME_CREATURE)
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
                    continue;
                }
            }

            if (friendly_or_near)
            {
                if (_handle_potion(mons, beem))
                {
                    DEBUG_ENERGY_USE("_handle_potion()");
                    continue;
                }

                if (_handle_scroll(mons))
                {
                    DEBUG_ENERGY_USE("_handle_scroll()");
                    continue;
                }

                if (_handle_wand(mons, beem))
                {
                    DEBUG_ENERGY_USE("_handle_wand()");
                    continue;
                }

                if (_handle_reaching(mons))
                {
                    DEBUG_ENERGY_USE("_handle_reaching()");
                    continue;
                }
            }

            if (_handle_throw(mons, beem))
            {
                DEBUG_ENERGY_USE("_handle_throw()");
                continue;
            }
        }

        if (!mons->caught())
        {
            if (mons->pos() + mmov == you.pos())
            {
                ASSERT(!crawl_state.game_is_arena());

                if (!mons->wont_attack() && !mons->has_ench(ENCH_CHARM)
                    && !mons->withdrawn())
                {
                    // If it steps into you, cancel other targets.
                    mons->foe = MHITYOU;
                    mons->target = you.pos();

                    monster_attack(mons);

                    if (mons_is_batty(mons))
                    {
                        mons->behaviour = BEH_WANDER;
                        set_random_target(mons);
                    }
                    DEBUG_ENERGY_USE("monster_attack()");
                    mmov.reset();
                    continue;
                }
            }

            // See if we move into (and fight) an unfriendly monster.
            monster* targ = monster_at(mons->pos() + mmov);
            if (mons_base_type(mons) == MONS_KRAKEN
                && targ && targ->type == MONS_KRAKEN_TENTACLE_SEGMENT
                && targ->props.exists("inwards") && targ->props["inwards"].get_int() == mons->mindex()
                && env.grid(targ->pos()) == DNGN_DEEP_WATER)
            {
                bool basis = targ->props.exists("outwards");
                int out_idx = basis ? targ->props["outwards"].get_int() : -1;
                if (out_idx != -1)
                {
                    menv[out_idx].props["inwards"].get_int() = mons->mindex();
                }

                monster_die(targ,
                            KILL_MISC, NON_MONSTER, true);
                targ = NULL;
            }

            if (targ
                && targ != mons
                && !mons_aligned(mons, targ)
                && monster_can_hit_monster(mons, targ))
            {
                // Maybe they can swap places?
                if (_swap_monsters(mons, targ))
                {
                    _swim_or_move_energy(mons);
                    continue;
                }
                // Figure out if they fight.
                else if ((!mons_is_firewood(targ)
                          || mons->type == MONS_KRAKEN_TENTACLE)
                              && monsters_fight(mons, targ))
                {
                    if (mons_is_batty(mons))
                    {
                        mons->behaviour = BEH_WANDER;
                        set_random_target(mons);
                        // mons->speed_increment -= mons->speed;
                    }

                    mmov.reset();
                    DEBUG_ENERGY_USE("monsters_fight()");
                    continue;
                }
            }

            if (invalid_monster(mons) || mons_is_stationary(mons))
            {
                if (mons->speed_increment == old_energy)
                    mons->speed_increment -= non_move_energy;
                continue;
            }

            if (mons->cannot_move() || !_monster_move(mons))
                mons->speed_increment -= non_move_energy;
        }
        you.update_beholder(mons);
        you.update_fearmonger(mons);

        // Reevaluate behaviour, since the monster's surroundings have
        // changed (it may have moved, or died for that matter).  Don't
        // bother for dead monsters.  :)
        if (mons->alive())
        {
            handle_behaviour(mons);
            ASSERT(in_bounds(mons->target) || mons->target.origin());
        }
    }

    if (mons_base_type(mons) == MONS_KRAKEN)
    {
        if (mons->pos() != kraken_last_update)
        {
            move_kraken_tentacles(mons);
        }
        move_kraken_tentacles(mons);
    }

    if (mons->type != MONS_NO_MONSTER && mons->hit_points < 1)
        monster_die(mons, KILL_MISC, NON_MONSTER);
}

//---------------------------------------------------------------
//
// handle_monsters
//
// This is the routine that controls monster AI.
//
//---------------------------------------------------------------
void handle_monsters()
{
    // Keep track of monsters that have already moved and don't allow
    // them to move again.
    memset(immobile_monster, 0, sizeof immobile_monster);

    for (monster_iterator mi; mi; ++mi)
    {
        if (immobile_monster[mi->mindex()])
            continue;

        const coord_def oldpos = mi->pos();

        handle_monster_move(*mi);
        fire_final_effects();

        if (!invalid_monster(*mi) && mi->pos() != oldpos)
            immobile_monster[mi->mindex()] = true;

        // If the player got banished, discard pending monster actions.
        if (you.banished)
        {
            // Clear list of mesmerising monsters.
            you.clear_beholders();
            you.clear_fearmongers();
            break;
        }
    }

    // Clear one-turn deep sleep flag.
    // XXX: With the current handling, it would be cleaner to
    //      not treat this as an enchantment.
    // XXX: ENCH_SLEEPY only really works for player-cast
    //      hibernation.
    for (monster_iterator mi; mi; ++mi)
        mi->del_ench(ENCH_SLEEPY);

    // Clear any summoning flags so that lower indiced
    // monsters get their actions in the next round.
    for (int i = 0; i < MAX_MONSTERS; i++)
        menv[i].flags &= ~MF_JUST_SUMMONED;
}

static bool _jelly_divide(monster* parent)
{
    if (!mons_class_flag(parent->type, M_SPLITS))
        return (false);

    const int reqd = std::max(parent->hit_dice * 8, 50);
    if (parent->hit_points < reqd)
        return (false);

    monster* child = NULL;
    coord_def child_spot;
    int num_spots = 0;

    // First, find a suitable spot for the child {dlb}:
    for (adjacent_iterator ai(parent->pos()); ai; ++ai)
        if (actor_at(*ai) == NULL && parent->can_pass_through(*ai))
            if (one_chance_in(++num_spots))
                child_spot = *ai;

    if (num_spots == 0)
        return (false);

    // Now that we have a spot, find a monster slot {dlb}:
    child = get_free_monster();
    if (!child)
        return (false);

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

    mgrd(child->pos()) = child->mindex();

    if (!simple_monster_message(parent, " splits in two!"))
        if (player_can_hear(parent->pos()) || player_can_hear(child->pos()))
            mpr("You hear a squelching noise.", MSGCH_SOUND);

    if (crawl_state.game_is_arena())
        arena_placed_monster(child);

    return (true);
}

// XXX: This function assumes that only jellies eat items.
static bool _monster_eat_item(monster* mons, bool nearby)
{
    if (!mons_eats_items(mons))
        return (false);

    // Friendly jellies won't eat (unless worshipping Jiyva).
    if (mons->friendly() && you.religion != GOD_JIYVA)
        return (false);

    // Off-limit squares are off-limit.
    if (testbits(env.pgrid(mons->pos()), FPROP_NO_JIYVA))
        return (false);

    int hps_changed = 0;
    int max_eat = roll_dice(1, 10);
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

#if defined(DEBUG_DIAGNOSTICS) || defined(DEBUG_EATERS)
        mprf(MSGCH_DIAGNOSTICS,
             "%s eating %s", mons->name(DESC_PLAIN, true).c_str(),
             si->name(DESC_PLAIN).c_str());
#endif

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
            quant = std::min(quant, max_eat - eaten);

            hps_changed += (quant * item_mass(*si)) / 20 + quant;
            eaten += quant;

            if (mons->caught()
                && si->base_type == OBJ_MISSILES
                && si->sub_type == MI_THROWING_NET
                && item_is_stationary(*si))
            {
                mons->del_ench(ENCH_HELD, true);
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

        if (you.religion == GOD_JIYVA)
        {
            gain = sacrifice_item_stack(*si, &js);
            if (gain > PIETY_NONE)
                simple_god_message(" appreciates your sacrifice.");

            jiyva_slurp_message(js);
        }

        if (quant >= si->quantity)
            item_was_destroyed(*si, mons->mindex());

        if (is_blood_potion(*si))
            for (int i = 0; i < quant; ++i)
                remove_oldest_blood_potion(*si);
        dec_mitm_item_quantity(si.link(), quant);
    }

    if (eaten > 0)
    {
        hps_changed = std::max(hps_changed, 1);
        hps_changed = std::min(hps_changed, 50);

        if (death_ooze_ate_good)
            mons->hurt(NULL, hps_changed, BEAM_NONE, false);
        else
        {
            // This is done manually instead of using heal_monster(),
            // because that function doesn't work quite this way. - bwr
            mons->hit_points += hps_changed;
            mons->max_hit_points = std::max(mons->hit_points,
                                               mons->max_hit_points);
        }

        if (death_ooze_ate_corpse)
        {
            place_cloud (CLOUD_MIASMA, mons->pos(),
                          4 + random2(5), mons->kill_alignment(),
                          KILL_MON_MISSILE);
        }

        if (death_ooze_ate_good)
            simple_monster_message(mons, " twists violently!");
        else if (eaten_net)
            simple_monster_message(mons, " devours the net!");
        else
            _jelly_divide(mons);
    }

    return (eaten > 0);
}

static bool _monster_eat_single_corpse(monster* mons, item_def& item,
                                       bool do_heal, bool nearby)
{
    if (item.base_type != OBJ_CORPSES || item.sub_type != CORPSE_BODY)
        return (false);

    monster_type mt = static_cast<monster_type>(item.plus);
    if (do_heal)
    {
        mons->hit_points += 1 + random2(mons_weight(mt)) / 100;

        // Limited growth factor here - should 77 really be the cap? {dlb}:
        mons->hit_points = std::min(100, mons->hit_points);
        mons->max_hit_points = std::max(mons->hit_points,
                                           mons->max_hit_points);
    }

    if (nearby)
    {
        mprf("%s eats %s.", mons->name(DESC_CAP_THE).c_str(),
             item.name(DESC_NOCAP_THE).c_str());
    }

    // Assume that eating a corpse requires butchering it.  Use logic
    // from misc.cc:turn_corpse_into_chunks() and the butchery-related
    // delays in delay.cc:stop_delay().

    const int max_chunks = get_max_corpse_chunks(mt);

    // Only fresh corpses bleed enough to colour the ground.
    if (!food_is_rotten(item))
        bleed_onto_floor(mons->pos(), mt, max_chunks, true);

    if (mons_skeleton(mt) && one_chance_in(3))
        turn_corpse_into_skeleton(item);
    else
        destroy_item(item.index());

    return (true);
}

static bool _monster_eat_corpse(monster* mons, bool do_heal, bool nearby)
{
    if (!mons_eats_corpses(mons))
        return (false);

    int eaten = 0;

    for (stack_iterator si(mons->pos()); si; ++si)
    {
        if (_monster_eat_single_corpse(mons, *si, do_heal, nearby))
        {
            eaten++;
            break;
        }
    }

    return (eaten > 0);
}

static bool _monster_eat_food(monster* mons, bool nearby)
{
    if (!mons_eats_food(mons))
        return (false);

    if (mons_is_fleeing(mons))
        return (false);

    int eaten = 0;

    for (stack_iterator si(mons->pos()); si; ++si)
    {
        const bool is_food = (si->base_type == OBJ_FOOD);
        const bool is_corpse = (si->base_type == OBJ_CORPSES
                                   && si->sub_type == CORPSE_BODY);

        if (!is_food && !is_corpse)
            continue;

        if ((mons->wont_attack()
                || grid_distance(mons->pos(), you.pos()) > 1)
            && coinflip())
        {
            if (is_food)
            {
                if (nearby)
                {
                    mprf("%s eats %s.", mons->name(DESC_CAP_THE).c_str(),
                         quant_name(*si, 1, DESC_NOCAP_THE).c_str());
                }

                dec_mitm_item_quantity(si.link(), 1);

                eaten++;
                break;
            }
            else
            {
                // Assume that only undead can heal from eating corpses.
                if (_monster_eat_single_corpse(mons, *si,
                                               mons->holiness() == MH_UNDEAD,
                                               nearby))
                {
                    eaten++;
                    break;
                }
            }
        }
    }

    return (eaten > 0);
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
        return (false);

    // Hack - Harpies fly over water, but we don't have a general
    // system for monster igrd yet.  Flying intelligent monsters
    // (kenku!) would also count here.
    dungeon_feature_type feat = grd(mons->pos());

    if ((feat == DNGN_LAVA || feat == DNGN_DEEP_WATER) && !mons->flight_mode())
        return (false);

    const bool nearby = mons_near(mons);
    int count_pickup = 0;

    if (mons_itemeat(mons) != MONEAT_NOTHING)
    {
        if (mons_eats_items(mons))
        {
            if (_monster_eat_item(mons, nearby))
                return (false);
        }
        else if (mons_eats_corpses(mons))
        {
            // Assume that only undead can heal from eating corpses.
            if (_monster_eat_corpse(mons, mons->holiness() == MH_UNDEAD,
                                    nearby))
            {
                return (false);
            }
        }
        else if (mons_eats_food(mons))
        {
            if (_monster_eat_food(mons, nearby))
                return (false);
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
            if (mons->pickup_item(*si, nearby))
                count_pickup++;

            if (count_pickup > 1 || coinflip())
                break;
        }
    }

    return (count_pickup > 0);
}

// Randomise potential damage.
static int _estimated_trap_damage(trap_type trap)
{
    switch (trap)
    {
        case TRAP_BLADE: return (10 + random2(30));
        case TRAP_DART:  return (random2(4));
        case TRAP_ARROW: return (random2(7));
        case TRAP_SPEAR: return (random2(10));
        case TRAP_BOLT:  return (random2(13));
        case TRAP_AXE:   return (random2(15));
        default:         return (0);
    }
}

// Check whether a given trap (described by trap position) can be
// regarded as safe.  Takes into account monster intelligence and
// allegiance.
// (just_check is used for intelligent monsters trying to avoid traps.)
static bool _is_trap_safe(const monster* mons, const coord_def& where,
                          bool just_check)
{
    const int intel = mons_intel(mons);

    const trap_def *ptrap = find_trap(where);
    if (!ptrap)
        return (true);
    const trap_def& trap = *ptrap;

    const bool player_knows_trap = (trap.is_known(&you));

    // No friendly monsters will ever enter a Zot trap you know.
    if (player_knows_trap && mons->friendly() && trap.type == TRAP_ZOT)
        return (false);

    // Dumb monsters don't care at all.
    if (intel == I_PLANT)
        return (true);

    // Known shafts are safe. Unknown ones are unknown.
    if (trap.type == TRAP_SHAFT)
        return (true);

    // Hostile monsters are not afraid of non-mechanical traps.
    // Allies will try to avoid teleportation and zot traps.
    const bool mechanical = (trap.category() == DNGN_TRAP_MECHANICAL);

    if (trap.is_known(mons))
    {
        if (just_check)
            return (false); // Square is blocked.
        else
        {
            // Test for corridor-like environment.
            const int x = where.x - mons->pos().x;
            const int y = where.y - mons->pos().y;

            // The question is whether the monster (m) can easily reach its
            // presumable destination (x) without stepping on the trap. Traps
            // in corridors do not allow this. See e.g
            //  #x#        ##
            //  #^#   or  m^x
            //   m         ##
            //
            // The same problem occurs if paths are blocked by monsters,
            // hostile terrain or other traps rather than walls.
            // What we do is check whether the squares with the relative
            // positions (-1,0)/(+1,0) or (0,-1)/(0,+1) form a "corridor"
            // (relative to the _trap_ position rather than the monster one).
            // If they don't, the trap square is marked as "unsafe" (because
            // there's a good alternative move for the monster to take),
            // otherwise the decision will be made according to later tests
            // (monster hp, trap type, ...)
            // If a monster still gets stuck in a corridor it will usually be
            // because it has less than half its maximum hp.

            if ((_mon_can_move_to_pos(mons, coord_def(x-1, y), true)
                 || _mon_can_move_to_pos(mons, coord_def(x+1,y), true))
                && (_mon_can_move_to_pos(mons, coord_def(x,y-1), true)
                    || _mon_can_move_to_pos(mons, coord_def(x,y+1), true)))
            {
                return (false);
            }
        }
    }

    // Friendlies will try not to be parted from you.
    if (intelligent_ally(mons) && trap.type == TRAP_TELEPORT
        && player_knows_trap && mons_near(mons))
    {
        return (false);
    }

    // Healthy monsters don't mind a little pain.
    if (mechanical && mons->hit_points >= mons->max_hit_points / 2
        && (intel == I_ANIMAL
            || mons->hit_points > _estimated_trap_damage(trap.type)))
    {
        return (true);
    }

    // Friendly and good neutral monsters don't enjoy Zot trap perks;
    // handle accordingly.  In the arena Zot traps affect all monsters.
    if (mons->wont_attack() || crawl_state.game_is_arena())
    {
        return (mechanical ? mons_flies(mons)
                           : !trap.is_known(mons) || trap.type != TRAP_ZOT);
    }
    else
        return (!mechanical || mons_flies(mons));
}

static void _mons_open_door(monster* mons, const coord_def &pos)
{
    dungeon_feature_type grid = grd(pos);
    const char *adj = "", *noun = "door";

    bool was_secret = false;
    bool was_seen   = false;

    std::set<coord_def> all_door = connected_doors(pos);
    get_door_description(all_door.size(), &adj, &noun);

    for (std::set<coord_def>::iterator i = all_door.begin();
         i != all_door.end(); ++i)
    {
        const coord_def& dc = *i;
        if (grd(dc) == DNGN_SECRET_DOOR && you.see_cell(dc))
        {
            grid = grid_secret_door_appearance(dc);
            was_secret = true;
        }

        if (you.see_cell(dc))
            was_seen = true;

        grd(dc) = DNGN_OPEN_DOOR;
        set_terrain_changed(dc);
    }

    if (was_seen)
    {
        viewwindow();

        if (was_secret)
        {
            mprf("%s was actually a secret door!",
                 feature_description(grid, NUM_TRAPS, "",
                                     DESC_CAP_THE, false).c_str());
            learned_something_new(HINT_FOUND_SECRET_DOOR, pos);
        }

        std::string open_str = "opens the ";
        open_str += adj;
        open_str += noun;
        open_str += ".";

        mons->seen_context = open_str;

        if (!you.can_see(mons))
        {
            mprf("Something unseen %s", open_str.c_str());
            interrupt_activity(AI_FORCE_INTERRUPT);
        }
        else if (!you_are_delayed())
        {
            mprf("%s %s", mons->name(DESC_CAP_A).c_str(),
                 open_str.c_str());
        }
    }

    mons->lose_energy(EUT_MOVE);

    dungeon_events.fire_position_event(DET_DOOR_OPENED, pos);
}

static bool _habitat_okay(const monster* mons, dungeon_feature_type targ)
{
    return (monster_habitable_grid(mons, targ));
}

static bool _no_habitable_adjacent_grids(const monster* mon)
{
    for (adjacent_iterator ai(mon->pos()); ai; ++ai)
        if (_habitat_okay(mon, grd(*ai)))
            return (false);

    return (true);
}

static bool _same_kraken_parts(const monster* mpusher,
                               const monster* mpushee)
{
    if (mons_base_type(mpusher) != MONS_KRAKEN)
        return (false);

    if (mpushee->type == MONS_KRAKEN_TENTACLE
        && int(mpushee->number) == mpusher->mindex())
    {
        return (true);
    }

    if (mpushee->type == MONS_KRAKEN_TENTACLE_SEGMENT
        && int(menv[mpushee->number].number) == mpusher->mindex()
        && mpushee->props.exists("inwards") && mpushee->props["inwards"].get_int() == mpusher->mindex())
    {
        return (true);
    }

    return false;
}

static bool _mons_can_displace(const monster* mpusher,
                               const monster* mpushee)
{
    if (invalid_monster(mpusher) || invalid_monster(mpushee))
        return (false);

    const int ipushee = mpushee->mindex();
    if (invalid_monster_index(ipushee))
        return (false);


    if (immobile_monster[ipushee]
        && !_same_kraken_parts(mpusher, mpushee))
    {
        return (false);
    }

    // Confused monsters can't be pushed past, sleeping monsters
    // can't push. Note that sleeping monsters can't be pushed
    // past, either, but they may be woken up by a crowd trying to
    // elbow past them, and the wake-up check happens downstream.
    if (mons_is_confused(mpusher)      || mons_is_confused(mpushee)
        || mpusher->cannot_move()   || mons_is_stationary(mpusher)
        || (!_same_kraken_parts(mpusher, mpushee)
           && (mpushee->cannot_move()
               || mons_is_stationary(mpushee)))
        || mpusher->asleep())
    {
        return (false);
    }

    // Batty monsters are unpushable.
    if (mons_is_batty(mpusher) || mons_is_batty(mpushee))
        return (false);

    if (!monster_shover(mpusher))
        return (false);

    // Fleeing monsters of the same type may push past higher ranking ones.
    if (!monster_senior(mpusher, mpushee, mons_is_fleeing(mpusher)))
        return (false);

    return (true);
}

// Check whether a monster can move to given square (described by its relative
// coordinates to the current monster position). just_check is true only for
// calls from is_trap_safe when checking the surrounding squares of a trap.
static bool _mon_can_move_to_pos(const monster* mons,
                                 const coord_def& delta, bool just_check)
{
    const coord_def targ = mons->pos() + delta;

    // Bounds check: don't consider moving out of grid!
    if (!in_bounds(targ))
        return (false);

    // No monster may enter the open sea.
    if (grd(targ) == DNGN_OPEN_SEA)
        return (false);

    // Non-friendly and non-good neutral monsters won't enter
    // sanctuaries.
    if (!mons->wont_attack()
        && is_sanctuary(targ)
        && !is_sanctuary(mons->pos()))
    {
        return (false);
    }

    // Inside a sanctuary don't attack anything!
    if (is_sanctuary(mons->pos()) && actor_at(targ))
        return (false);

    const dungeon_feature_type target_grid = grd(targ);
    const habitat_type habitat = mons_primary_habitat(mons);

    // The kraken is so large it cannot enter shallow water.
    // Its tentacles can, and will, though.
    if (mons_base_type(mons) == MONS_KRAKEN
        && target_grid == DNGN_SHALLOW_WATER)
    {
        return (false);
    }

    // Effectively slows down monster movement across water.
    // Fire elementals can't cross at all.
    bool no_water = false;
    if (mons->type == MONS_FIRE_ELEMENTAL || one_chance_in(5))
        no_water = true;

    const int targ_cloud_num = env.cgrid(targ);
    if (mons_avoids_cloud(mons, targ_cloud_num))
        return (false);

    const bool burrows = mons_class_flag(mons->type, M_BURROWS);
    const bool flattens_trees = mons_flattens_trees(mons);
    if ((burrows && (target_grid == DNGN_ROCK_WALL
                     || target_grid == DNGN_CLEAR_ROCK_WALL))
        || (flattens_trees && target_grid == DNGN_TREE))
    {
        // Don't burrow out of bounds.
        if (!in_bounds(targ))
            return (false);

        // Don't burrow at an angle (legacy behaviour).
        if (delta.x != 0 && delta.y != 0)
            return (false);
    }
    else if (no_water && feat_is_water(target_grid))
        return (false);
    else if (!mons_can_traverse(mons, targ, false)
             && !_habitat_okay(mons, target_grid))
    {
        // If the monster somehow ended up in this habitat (and is
        // not dead by now), give it a chance to get out again.
        if (grd(mons->pos()) == target_grid
            && _no_habitable_adjacent_grids(mons))
        {
            return (true);
        }

        return (false);
    }

    // Wandering mushrooms usually don't move while you are looking.
    if (mons->type == MONS_WANDERING_MUSHROOM)
    {
        if (!mons->wont_attack()
            && is_sanctuary(mons->pos()))
        {
            return (true);
        }

        if (!mons->friendly()
                && you.see_cell(targ)
            || mon_enemies_around(mons))
        {
            return (false);
        }
    }

    // Water elementals avoid fire and heat.
    if (mons->type == MONS_WATER_ELEMENTAL && target_grid == DNGN_LAVA)
        return (false);

    // Fire elementals avoid water and cold.
    if (mons->type == MONS_FIRE_ELEMENTAL && feat_is_watery(target_grid))
        return (false);

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
        return (false);
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
                return (true);

            return (false); // blocks square
        }

        // Cut down plants only when no alternative, or they're
        // our target.
        if (mons_is_firewood(targmonster) && mons->target != targ)
            return (false);

        if (mons_aligned(mons, targmonster)
            && !_mons_can_displace(mons, targmonster))
        {
            return (false);
        }
    }

    // Friendlies shouldn't try to move onto the player's
    // location, if they are aiming for some other target.
    if (mons->wont_attack()
        && mons->foe != MHITYOU
        && (mons->foe != MHITNOT || mons->is_patrolling())
        && targ == you.pos())
    {
        return (false);
    }

    // Wandering through a trap is OK if we're pretty healthy,
    // really stupid, or immune to the trap.
    if (!_is_trap_safe(mons, targ, just_check))
        return (false);

    // If we end up here the monster can safely move.
    return (true);
}

// Uses, and updates the global variable mmov.
static void _find_good_alternate_move(monster* mons,
                                      const move_array& good_move)
{
    const int current_distance = distance(mons->pos(), mons->target);

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
            {
                dist[i] = distance(mons->pos()+mon_compass[newdir],
                                   mons->target);
            }
            else
            {
                dist[i] = (mons_is_fleeing(mons)) ? (-FAR_AWAY) : FAR_AWAY;
            }
        }

        const int dir0 = ((dir + 8 + sdir) % 8);
        const int dir1 = ((dir + 8 - sdir) % 8);

        // Now choose.
        if (dist[0] == dist[1] && abs(dist[0]) == FAR_AWAY)
            continue;

        // Which one was better? -- depends on FLEEING or not.
        if (mons_is_fleeing(mons))
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

    mons->hit_points += 5;

    // note here, that this makes jellies "grow" {dlb}:
    if (mons->hit_points > mons->max_hit_points)
        mons->max_hit_points = mons->hit_points;

    _jelly_divide(mons);
}

static bool _monster_swaps_places(monster* mon, const coord_def& delta)
{
    if (delta.origin())
        return (false);

    monster* const m2 = monster_at(mon->pos() + delta);

    if (!m2)
        return (false);

    if (!_mons_can_displace(mon, m2))
        return (false);

    if (m2->asleep())
    {
        if (coinflip())
        {
#ifdef DEBUG_DIAGNOSTICS
            mprf(MSGCH_DIAGNOSTICS,
                 "Alerting monster %s at (%d,%d)",
                 m2->name(DESC_PLAIN).c_str(), m2->pos().x, m2->pos().y);
#endif
            behaviour_event(m2, ME_ALERT, MHITNOT);
        }
        return (false);
    }

    // Check that both monsters will be happy at their proposed new locations.
    const coord_def c = mon->pos();
    const coord_def n = mon->pos() + delta;

    if (!_habitat_okay(mon, grd(n)) || !_habitat_okay(m2, grd(c)))
        return (false);

    // Okay, do the swap!
    _swim_or_move_energy(mon);

    mon->set_position(n);
    mgrd(n) = mon->mindex();
    m2->set_position(c);
    const int m2i = m2->mindex();
    ASSERT(m2i >= 0 && m2i < MAX_MONSTERS);
    mgrd(c) = m2i;
    immobile_monster[m2i] = true;

    mon->check_redraw(c);
    mon->apply_location_effects(c);
    m2->check_redraw(c);
    m2->apply_location_effects(n);

    // The seen context no longer applies if the monster is moving normally.
    mon->seen_context.clear();
    m2->seen_context.clear();

    return (false);
}

static bool _do_move_monster(monster* mons, const coord_def& delta)
{
    const coord_def f = mons->pos() + delta;

    if (!in_bounds(f))
        return (false);

    if (f == you.pos())
    {
        monster_attack(mons);
        return (true);
    }

    // This includes the case where the monster attacks itself.
    if (monster* def = monster_at(f))
    {
        monsters_fight(mons, def);
        return (true);
    }

    if (mons_can_open_door(mons, f))
    {
        _mons_open_door(mons, f);
        return (true);
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

    // The monster gave a "comes into view" message and then immediately
    // moved back out of view, leaing the player nothing to see, so give
    // this message to avoid confusion.
    if (mons->seen_context == _just_seen && !you.see_cell(f))
        simple_monster_message(mons, " moves out of view.");
    else if (Hints.hints_left && (mons->flags & MF_WAS_IN_VIEW)
             && !you.see_cell(f))
    {
        learned_something_new(HINT_MONSTER_LEFT_LOS, mons->pos());
    }

    // The seen context no longer applies if the monster is moving normally.
    mons->seen_context.clear();

    // This appears to be the real one, ie where the movement occurs:
    _swim_or_move_energy(mons);

    if (grd(mons->pos()) == DNGN_DEEP_WATER && grd(f) != DNGN_DEEP_WATER
        && !monster_habitable_grid(mons, DNGN_DEEP_WATER))
    {
        mons->seen_context = "emerges from the water";
    }
    mgrd(mons->pos()) = NON_MONSTER;

    coord_def old_pos = mons->pos();

    mons->set_position(f);

    mgrd(mons->pos()) = mons->mindex();

    ballisto_on_move(mons, old_pos);

    mons->check_redraw(mons->pos() - delta);
    mons->apply_location_effects(mons->pos() - delta);

    return (true);
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
    const bool bad_align =
        ((mons->friendly() || mons->good_neutral())
         && (targ->friendly() || targ->good_neutral()));
    return (mons_is_firewood(targ) && !bad_align);
}

static bool _monster_move(monster* mons)
{
    move_array good_move;

    const habitat_type habitat = mons_primary_habitat(mons);
    bool deep_water_available = false;

    if (mons->type == MONS_TRAPDOOR_SPIDER)
    {
        if (mons->submerged())
            return (false);

        // Trapdoor spiders hide if they can't see their foe.
        // (Note that friendly trapdoor spiders will thus hide even
        // if they can see you.)
        const actor *foe = mons->get_foe();
        const bool can_see = foe && mons->can_see(foe);

        if (monster_can_submerge(mons, grd(mons->pos()))
            && !can_see && !mons_is_confused(mons)
            && !mons->caught()
            && !mons->berserk())
        {
            mons->add_ench(ENCH_SUBMERGED);
            mons->behaviour = BEH_LURK;
            return (false);
        }
    }

    // Berserking monsters make a lot of racket.
    if (mons->berserk())
    {
        int noise_level = get_shout_noise_level(mons_shouts(mons->type));
        if (noise_level > 0)
        {
            if (you.can_see(mons))
            {
                if (one_chance_in(10))
                {
                    mprf(MSGCH_TALK_VISUAL, "%s rages.",
                         mons->name(DESC_CAP_THE).c_str());
                }
                noisy(noise_level, mons->pos(), mons->mindex());
            }
            else if (one_chance_in(5))
                handle_monster_shouts(mons, true);
            else
            {
                // Just be noisy without messaging the player.
                noisy(noise_level, mons->pos(), mons->mindex());
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
        return (false);
    }

    // If a water monster is currently flopping around on land, it cannot
    // really control where it wants to move, though there's a 50% chance
    // of flopping into an adjacent water grid.
    if (mons->has_ench(ENCH_AQUATIC_LAND))
    {
        std::vector<coord_def> adj_water;
        std::vector<coord_def> adj_move;
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
            return (false);
        }

        std::vector<coord_def> moves = adj_water;
        if (adj_water.empty() || coinflip())
            moves = adj_move;

        coord_def newpos = mons->pos();
        int count = 0;
        for (unsigned int i = 0; i < moves.size(); ++i)
            if (one_chance_in(++count))
                newpos = moves[i];

        const monster* mon2 = monster_at(newpos);
        if (newpos == you.pos() && mons->wont_attack()
            || (mon2 && mons->wont_attack() == mon2->wont_attack()))
        {

            simple_monster_message(mons, " flops around on dry land!");
            return (false);
        }

        return _do_move_monster(mons, newpos - mons->pos());
    }

    // Let's not even bother with this if mmov is zero.
    if (mmov.origin())
        return (false);

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
                _mon_can_move_to_pos(mons, coord_def(count_x-1, count_y-1));
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

    const bool burrows = mons_class_flag(mons->type, M_BURROWS);
    const bool flattens_trees = mons_flattens_trees(mons);
    // Take care of beetle burrowing.
    if (burrows || flattens_trees)
    {
        const dungeon_feature_type feat = grd(mons->pos() + mmov);

        if ((((feat == DNGN_ROCK_WALL || feat == DNGN_CLEAR_ROCK_WALL)
              && burrows)
             || (feat == DNGN_TREE && flattens_trees))
            && good_move[mmov.x + 1][mmov.y + 1] == true)
        {
            const coord_def target(mons->pos() + mmov);

            const dungeon_feature_type newfeat =
                (feat == DNGN_TREE? dgn_tree_base_feature_at(target)
                 : DNGN_FLOOR);
            grd(target) = newfeat;
            set_terrain_changed(target);

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
                         mons->name(DESC_CAP_THE).c_str() : "Something");
                    noisy(25, target);
                }
                else
                {
                    noisy(25, target, "You hear a crashing sound.");
                }
            }
            else if (player_can_hear(mons->pos() + mmov))
            {
                // Message depends on whether caused by boring beetle or
                // acid (Dissolution).
                mpr((mons->type == MONS_BORING_BEETLE) ?
                    "You hear a grinding noise." :
                    "You hear a sizzling sound.", MSGCH_SOUND);
            }
        }
    }

    bool ret = false;
    if (good_move[mmov.x + 1][mmov.y + 1] && !mmov.origin())
    {
        // Check for attacking player.
        if (mons->pos() + mmov == you.pos())
        {
            ret = monster_attack(mons);
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

#ifdef DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS,
                     "BUG: %s was marked as follower when not following!",
                     mons->name(DESC_PLAIN).c_str(), true);
#endif
            }
            else
            {
                ret    = true;
                mmov.reset();

#ifdef DEBUG_DIAGNOSTICS
                mprf(MSGCH_DIAGNOSTICS,
                     "%s is skipping movement in order to follow.",
                     mons->name(DESC_CAP_THE).c_str(), true);
#endif
            }
        }

        // Check for attacking another monster.
        if (monster* targ = monster_at(mons->pos() + mmov))
        {
            if (mons_aligned(mons, targ))
                ret = _monster_swaps_places(mons, mmov);
            else
            {
                monsters_fight(mons, targ);
                ret = true;
            }

            // If the monster swapped places, the work's already done.
            mmov.reset();
        }

        // The monster could die after a melee attack due to a mummy
        // death curse or something.
        if (!mons->alive())
            return (true);

        if (mons_genus(mons->type) == MONS_EFREET
            || mons->type == MONS_FIRE_ELEMENTAL)
        {
            place_cloud(CLOUD_FIRE, mons->pos(),
                         2 + random2(4), mons->kill_alignment(),
                         KILL_MON_MISSILE);
        }

        if (mons->type == MONS_ROTTING_DEVIL
            || mons->type == MONS_CURSE_TOE)
        {
            place_cloud(CLOUD_MIASMA, mons->pos(),
                         2 + random2(3), mons->kill_alignment(),
                         KILL_MON_MISSILE);
        }

        // Commented out, but left in as an example of gloom. {due}
        //if (mons->type == MONS_SHADOW)
        //{
        //    big_cloud (CLOUD_GLOOM, mons->kill_alignment(), mons->pos(), 10 + random2(5), 2 + random2(8));
        //}

    }
    else
    {
        monster* targ = monster_at(mons->pos() + mmov);
        if (!mmov.origin() && targ && _may_cutdown(mons, targ))
        {
            monsters_fight(mons, targ);
            ret = true;
        }

        mmov.reset();

        // Fleeing monsters that can't move will panic and possibly
        // turn to face their attacker.
        make_mons_stop_fleeing(mons);
    }

    if (mmov.x || mmov.y || (mons->confused() && one_chance_in(6)))
        return (_do_move_monster(mons, mmov));

    return (ret);
}

static bool _mephitic_cloud_roll(const monster* mons)
{
    const int meph_hd_cap = 21;
    return (mons->hit_dice >= meph_hd_cap? one_chance_in(50)
            : !x_chance_in_y(mons->hit_dice, meph_hd_cap));
}

static void _mons_in_cloud(monster* mons)
{
    // Submerging in water or lava saves from clouds.
    if (mons->submerged() && env.grid(mons->pos()) != DNGN_FLOOR)
        return;

    const int wc = env.cgrid(mons->pos());
    if (wc == EMPTY_CLOUD)
        return;
    const cloud_struct& cloud(env.cloud[wc]);
    if (cloud.type == CLOUD_NONE)
        return;

    int hurted = 0;
    int resist = 0;
    bolt beam;

    const int speed = ((mons->speed > 0) ? mons->speed : 10);
    bool wake = false;

    if (mons_is_mimic(mons->type))
    {
        mimic_alert(mons);
        return;
    }

    switch (cloud.type)
    {
    case CLOUD_DEBUGGING:
        mprf(MSGCH_ERROR,
             "Monster %s stepped on a nonexistent cloud at (%d,%d)",
             mons->name(DESC_PLAIN, true).c_str(),
             mons->pos().x, mons->pos().y);
        return;

    case CLOUD_FIRE:
    case CLOUD_FOREST_FIRE:
        if (mons->type == MONS_FIRE_VORTEX
            || mons->type == MONS_EFREET
            || mons->type == MONS_FIRE_ELEMENTAL)
        {
            return;
        }

        cloud.announce_actor_engulfed(mons);
        hurted +=
            resist_adjust_damage(mons,
                                 BEAM_FIRE,
                                 mons->res_fire(),
                                 ((random2avg(16, 3) + 6) * 10) / speed);

        hurted -= random2(1 + mons->ac);
        break;

    case CLOUD_STINK:
        cloud.announce_actor_engulfed(mons);
        // If you don't have to breathe, unaffected.
        if (mons->res_poison() > 0 || mons->is_unbreathing())
            return;

        beam.flavour = BEAM_CONFUSION;
        beam.thrower = cloud.killer;

        if (cloud.whose == KC_FRIENDLY)
            beam.beam_source = ANON_FRIENDLY_MONSTER;

        if (mons_class_is_confusable(mons->type)
            && _mephitic_cloud_roll(mons))
        {
            beam.apply_enchantment_to_monster(mons);
        }

        hurted += (random2(3) * 10) / speed;
        break;

    case CLOUD_COLD:
        cloud.announce_actor_engulfed(mons);
        hurted +=
            resist_adjust_damage(mons,
                                 BEAM_COLD,
                                 mons->res_cold(),
                                 ((6 + random2avg(16, 3)) * 10) / speed);

        hurted -= random2(1 + mons->ac);
        break;

    case CLOUD_POISON:
        cloud.announce_actor_engulfed(mons);
        if (mons->res_poison() > 0)
            return;

        poison_monster(mons, cloud.whose);
        // If the monster got poisoned, wake it up.
        wake = true;

        hurted += (random2(8) * 10) / speed;

        if (mons->res_poison() < 0)
            hurted += (random2(4) * 10) / speed;
        break;

    case CLOUD_STEAM:
    {
        // FIXME: couldn't be bothered coding for armour of res fire
        cloud.announce_actor_engulfed(mons);
        const int steam_base_damage = steam_cloud_damage(cloud);
        hurted +=
            resist_adjust_damage(
                mons,
                BEAM_STEAM,
                mons->res_steam(),
                (random2avg(steam_base_damage, 2) * 10) / speed);

        hurted -= random2(1 + mons->ac);
        break;
    }

    case CLOUD_MIASMA:
        cloud.announce_actor_engulfed(mons);
        if (mons->res_rotting())
            return;

        miasma_monster(mons, cloud.whose);

        hurted += (10 * random2avg(12, 3)) / speed;    // 3
        break;

    case CLOUD_RAIN:
        if (mons->is_fiery())
        {
            if (!silenced(mons->pos()))
                simple_monster_message(mons, " sizzles in the rain!");
            else
                simple_monster_message(mons, " steams in the rain!");

            hurted += ((4 * random2(3)) - random2(mons->ac));
            wake = true;
        }
        break;

    case CLOUD_MUTAGENIC:
        cloud.announce_actor_engulfed(mons);
        // Will only polymorph a monster if they're not magic immune, can
        // mutate, aren't res asphyx, and pass the same check as meph cloud.
        if (mons->can_mutate() && !mons_immune_magic(mons)
                && 1 + random2(27) >= mons->hit_dice
                && !mons->res_asphyx())
        {
            if (mons->mutate())
                wake = true;
        }
        break;

    case CLOUD_HOLY_FLAMES:
        cloud.announce_actor_engulfed(mons, mons->is_holy());

        if (mons->is_holy())
            resist = 3;
        if (mons->is_evil() || mons->is_unholy())
            resist = -1;

        hurted +=
            resist_adjust_damage(mons,
                                  BEAM_HOLY_FLAME,
                                  resist,
                                  ((random2avg(16, 3) + 6) * 10) / speed);

        dprf("Pain: %d, resist: %d", hurted, resist);

        hurted -= random2(1 + mons->ac);
        break;

    case CLOUD_CHAOS:
        if (coinflip())
        {
            cloud.announce_actor_engulfed(mons);
            chaos_affect_actor(mons);
            wake = true;
        }
        break;

    default:                // 'harmless' clouds -- colored smoke, etc {dlb}.
        return;
    }

    // A sleeping monster that sustains damage will wake up.
    if ((wake || hurted > 0) && mons->asleep())
    {
        // We have no good coords to give the monster as the source of the
        // disturbance other than the cloud itself.
        behaviour_event(mons, ME_DISTURB, MHITNOT, mons->pos());
    }

    if (hurted > 0)
    {
#ifdef DEBUG_DIAGNOSTICS
        mprf(MSGCH_DIAGNOSTICS, "%s takes %d damage from cloud.",
             mons->name(DESC_CAP_THE).c_str(), hurted);
#endif
        mons->hurt(NULL, hurted, BEAM_MISSILE, false);

        if (mons->hit_points < 1)
        {
            mon_enchant death_ench(ENCH_NONE, 0, cloud.whose);
            monster_die(mons, cloud.killer, death_ench.kill_agent());
        }
    }
}

static spell_type _map_wand_to_mspell(int wand_type)
{
    switch (wand_type)
    {
    case WAND_FLAME:           return SPELL_THROW_FLAME;
    case WAND_FROST:           return SPELL_THROW_FROST;
    case WAND_SLOWING:         return SPELL_SLOW;
    case WAND_HASTING:         return SPELL_HASTE;
    case WAND_MAGIC_DARTS:     return SPELL_MAGIC_DART;
    case WAND_HEALING:         return SPELL_MINOR_HEALING;
    case WAND_PARALYSIS:       return SPELL_PARALYSE;
    case WAND_FIRE:            return SPELL_BOLT_OF_FIRE;
    case WAND_COLD:            return SPELL_BOLT_OF_COLD;
    case WAND_CONFUSION:       return SPELL_CONFUSE;
    case WAND_INVISIBILITY:    return SPELL_INVISIBILITY;
    case WAND_TELEPORTATION:   return SPELL_TELEPORT_OTHER;
    case WAND_LIGHTNING:       return SPELL_LIGHTNING_BOLT;
    case WAND_DRAINING:        return SPELL_BOLT_OF_DRAINING;
    case WAND_DISINTEGRATION:  return SPELL_DISINTEGRATE;
    case WAND_POLYMORPH_OTHER: return SPELL_POLYMORPH_OTHER;
    default:                   return SPELL_NO_SPELL;
    }
}
