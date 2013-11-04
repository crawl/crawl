/**
 * @file
 * @brief Monster behaviour functions.
**/

#include "AppHdr.h"
#include "mon-behv.h"

#include "externs.h"

#include "abl-show.h"
#include "act-iter.h"
#include "areas.h"
#include "coord.h"
#include "coordit.h"
#include "database.h"
#include "dungeon.h"
#include "env.h"
#include "fprop.h"
#include "exclude.h"
#include "losglobal.h"
#include "macro.h"
#include "mon-act.h"
#include "mon-death.h"
#include "mon-movetarget.h"
#include "mon-pathfind.h"
#include "mon-speak.h"
#include "mon-stuff.h"
#include "ouch.h"
#include "random.h"
#include "religion.h"
#include "spl-summoning.h"
#include "state.h"
#include "terrain.h"
#include "traps.h"
#include "hints.h"
#include "view.h"
#include "shout.h"

static void _set_nearest_monster_foe(monster* mons);

static void _guess_invis_foe_pos(monster* mon)
{
    const actor* foe          = mon->get_foe();
    const int    guess_radius = mons_sense_invis(mon) ? 3 : 2;

    vector<coord_def> possibilities;

    // NOTE: This depends on ignoring clouds, so that cells hidden by
    // opaque clouds are included as a possibility for the foe's location.
    for (radius_iterator ri(mon->pos(), guess_radius, C_ROUND, LOS_SOLID); ri; ++ri)
    {
        if (foe->is_habitable(*ri))
            possibilities.push_back(*ri);
    }

    if (!possibilities.empty())
        mon->target = possibilities[random2(possibilities.size())];
    else
        mon->target = dgn_random_point_from(mon->pos(), guess_radius);
}

static void _mon_check_foe_invalid(monster* mon)
{
    // Assume a spectral weapon has a valid target
    // Ideally this is not outside special cased like this
    if (mon->type == MONS_SPECTRAL_WEAPON)
        return;

    if (mon->foe != MHITNOT && mon->foe != MHITYOU)
    {
        if (actor *foe = mon->get_foe())
        {
            const monster* foe_mons = foe->as_monster();
            if (foe_mons->alive() && summon_can_attack(mon, foe)
                && (mon->friendly() != foe_mons->friendly()
                    || mon->neutral() != foe_mons->neutral()))
            {
                return;
            }
        }

        mon->foe = MHITNOT;
    }
}

static bool _mon_tries_regain_los(monster* mon)
{
    // Only intelligent monsters with ranged attack will try to regain LOS.
    if (mons_intel(mon) < I_NORMAL || !mons_has_ranged_attack(mon))
        return false;

    // Any special case should go here.
    if (mons_class_flag(mon->type, M_FIGHTER)
        && !(mon->type == MONS_CENTAUR_WARRIOR)
        && !(mon->type == MONS_YAKTAUR_CAPTAIN))
    {
        return false;
    }

    // Randomize it a bit to make it less predictable.
    return (mons_intel(mon) == I_NORMAL && !one_chance_in(10)
            || mons_intel(mon) == I_HIGH && !one_chance_in(20));
}

// Monster tries to get into a firing position. Among the cells which have
// a line of fire to the target, we choose the closest one to regain LOS as
// fast as possible. If several cells are eligible, we choose the one closest
// to ideal_range (too far = easier to escape, too close = easier to ambush).
static void _set_firing_pos(monster* mon, coord_def target)
{
    const int ideal_range = LOS_RADIUS / 2;
    const int current_distance = mon->pos().distance_from(target);

    // We don't consider getting farther away unless already very close.
    const int max_range = max(ideal_range, current_distance);

    int best_distance = INT_MAX;
    int best_distance_to_ideal_range = INT_MAX;
    coord_def best_pos(0, 0);

    for (distance_iterator di(mon->pos(), true, true, LOS_RADIUS);
         di; ++di)
    {
        const coord_def p(*di);
        const int range = p.distance_from(target);

        if (!mon->see_cell(*di))
            continue;

        if (!in_bounds(p) || range > max_range
            || !cell_see_cell(p, target, LOS_NO_TRANS)
            || !mon_can_move_to_pos(mon, p - mon->pos()))
        {
            continue;
        }

        const int distance = p.distance_from(mon->pos());

        if (distance < best_distance
            || distance == best_distance
               && abs(range - ideal_range) < best_distance_to_ideal_range)
        {
            best_pos = p;
            best_distance = distance;
            best_distance_to_ideal_range = abs(range - ideal_range);
        }
    }

    mon->firing_pos = best_pos;
}

struct dist2_sorter
{
    coord_def pos;
    bool operator()(const coord_def a, const coord_def b)
    {
        return distance2(a, pos) < distance2(b, pos);
    }
};

static bool _can_traverse_unseen(const monster* mon, const coord_def& target)
{
    if (mon->pos() == target)
        return true;

    ray_def ray;
    if (!find_ray(mon->pos(), target, ray, opc_immob))
        return false;

    while (ray.pos() != target && ray.advance())
        if (!mon_can_move_to_pos(mon, ray.pos() - mon->pos()))
            return false;

    return true;
}

// Attempt to find the furthest position from a given target which still has
// line of sight to it, and which the monster can move to (without pathfinding)
static coord_def _furthest_aim_spot(monster* mon, coord_def target)
{
    int best_distance = 0;
    coord_def best_pos(0, 0);

    for (distance_iterator di(mon->pos(), false, false, LOS_RADIUS);
         di; ++di)
    {
        const coord_def p(*di);
        const int range = p.distance_from(target);

        if (!cell_see_cell(mon->pos(), *di, LOS_NO_TRANS))
            continue;

        if (!in_bounds(p) || range > LOS_RADIUS - 1
            || !cell_see_cell(p, target, LOS_NO_TRANS)
            || !mon_can_move_to_pos(mon, p - mon->pos(), true))
        {
            continue;
        }

        const int distance = p.distance_from(target);
        if (distance > best_distance)
        {
            if (_can_traverse_unseen(mon, p))
            {
                best_pos = p;
                best_distance = distance;
            }
        }
    }

    return best_pos;
}

// Tries to find and set an optimal spot for the curse skull to lurk, just
// outside player's line of sight. We consider this to be the spot the furthest
// from the player which still has line of sight to where the player will move
// if they are approaching us. (This keeps it from lurking immediately around
// corners, where the player can enter melee range of it the first turn it comes
// into sight)
//
// The answer given is not always strictly optimal, since no actual pathfinding
// is done, and sometimes the best lurking position crosses spots visible to
// the player unless a more circuitous route is taken, but generally it is
// pretty good, and eliminates the most obvious cases of players luring them
// into easy kill.
static void _set_curse_skull_lurk_pos(monster* mon)
{
    // If we're already moving somewhere that we can actually reach, don't
    // search for a new spot.
    if (!mon->firing_pos.origin() && _can_traverse_unseen(mon, mon->firing_pos))
        return;

    // Examine spots adjacent to our target, starting with those closest to us
    vector<coord_def> spots;
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        if (!cell_is_solid(*ai) || feat_is_door(grd(*ai)))
            spots.push_back(*ai);
    }

    dist2_sorter sorter = {mon->pos()};
    sort(spots.begin(), spots.end(), sorter);

    coord_def lurk_pos(0, 0);
    for (unsigned int i = 0; i < spots.size(); ++i)
    {
        lurk_pos = _furthest_aim_spot(mon, spots[i]);

        // Consider the first position found to be good enough
        if (!lurk_pos.origin())
        {
            mon->firing_pos = lurk_pos;
            return;
        }
    }
}

static bool _stabber_keep_distance(const monster* mon, const actor* foe)
{
    return (mons_class_flag(mon->type, M_STABBER)
            && !mon->berserk_or_insane()
            && (mons_has_incapacitating_ranged_attack(mon, foe)
                || mons_has_incapacitating_spell(mon, foe))
            && !foe->incapacitated()
            && !adjacent(mon->pos(), foe->pos())
            && !mons_aligned(mon, foe));
}

//---------------------------------------------------------------
//
// handle_behaviour
//
// 1. Evaluates current AI state
// 2. Sets monster target x,y based on current foe
//
// XXX: Monsters of I_NORMAL or above should select a new target
// if their current target is another monster which is sitting in
// a wall and is immune to most attacks while in a wall, unless
// the monster has a spell or special/nearby ability which isn't
// affected by the wall.
//---------------------------------------------------------------
void handle_behaviour(monster* mon)
{
    // Test spawners should always be BEH_SEEK against a foe, since
    // their only purpose is to spew out monsters for testing
    // purposes.
    if (mon->type == MONS_TEST_SPAWNER)
    {
        for (monster_iterator mi; mi; ++mi)
        {
            if (mon->attitude != mi->attitude)
            {
                mon->foe       = mi->mindex();
                mon->target    = mi->pos();
                mon->behaviour = BEH_SEEK;
                return;
            }
        }
    }

    bool changed = true;
    bool isFriendly = mon->friendly();
    bool isNeutral  = mon->neutral();
    bool wontAttack = mon->wont_attack() && !mon->has_ench(ENCH_INSANE);

    // Whether the player position is in LOS of the monster.
    bool proxPlayer = !crawl_state.game_is_arena() && mon->see_cell(you.pos());

    // If set, pretend the player isn't there, but only for hostile monsters.
    if (proxPlayer && crawl_state.disables[DIS_MON_SIGHT] && !mon->wont_attack())
        proxPlayer = false;

    bool proxFoe;
    bool isHealthy  = (mon->hit_points > mon->max_hit_points / 2);
    bool isSmart    = (mons_intel(mon) > I_ANIMAL);
    bool isScared   = mon->has_ench(ENCH_FEAR);
    bool isPacified = mon->pacified();
    bool patrolling = mon->is_patrolling();
    static vector<level_exit> e;
    static int                e_index = -1;

    // Zotdef rotting
    if (crawl_state.game_is_zotdef())
    {
        if (!isFriendly && !isNeutral && env.orb_pos == mon->pos()
            && mon->speed)
        {
            const int loss = div_rand_round(10, mon->speed);
            if (loss)
            {
                mpr("Your flesh rots away as the Orb of Zot is desecrated.",
                    MSGCH_DANGER);

                // If the rot would reduce us to <= 0 max HP, attribute the
                // kill to the monster.
                if (loss >= you.hp_max_temp)
                    ouch(loss, mon->mindex(), KILLED_BY_ROTTING);

                rot_hp(loss);
            }
        }
    }

    //mprf("AI debug: mon %d behv=%d foe=%d pos=%d %d target=%d %d",
    //     mon->mindex(), mon->behaviour, mon->foe, mon->pos().x,
    //     mon->pos().y, mon->target.x, mon->target.y);

    // Check for confusion -- early out.
    if (mon->has_ench(ENCH_CONFUSION))
    {
        set_random_target(mon);
        return;
    }

    if (mons_is_fleeing_sanctuary(mon)
        && mons_is_fleeing(mon)
        && is_sanctuary(you.pos()))
    {
        return;
    }

    // Make sure monsters are not targetting the player in arena mode.
    ASSERT(!(crawl_state.game_is_arena() && mon->foe == MHITYOU));

    if (mons_wall_shielded(mon) && cell_is_solid(mon->pos()))
    {
        // Monster is safe, so its behaviour can be simplified to fleeing.
        if (mon->behaviour == BEH_CORNERED || mon->behaviour == BEH_PANIC
            || isScared)
        {
            mon->behaviour = BEH_FLEE;
        }
    }

    // Validate current target exists.
    _mon_check_foe_invalid(mon);

    // The target and foe set here for a spectral weapon should never change
    if (mon->type == MONS_SPECTRAL_WEAPON)
    {
        // Do nothing if we're still being placed
        if (!mon->summoner)
            return;

        actor *owner = actor_by_mid(mon->summoner);

        if (!owner || !owner->alive())
        {
            end_spectral_weapon(mon, false);
            return;
        }

        mon->target = owner->pos();
        mon->foe = MHITNOT;
        // Try to move towards any monsters the owner is attacking
        if (mon->props.exists(SW_TARGET_MID))
        {
            actor *atarget = actor_by_mid(mon->props[SW_TARGET_MID].get_int());

            // Only go after the target if it's still near the owner,
            // and so are we.
            // The weapon is restricted to a leash range of 2,
            // and things reachable within that leash range [qoala]
            const int leash = 2;
            if (atarget && atarget->alive()
                && (grid_distance(owner->pos(), atarget->pos())
                    <= ((mon->reach_range() == REACH_TWO) ? leash + 2 : leash + 1))
                && (grid_distance(owner->pos(), mon->pos()) <= leash))
            {
                mon->target = atarget->pos();
                mon->foe = atarget->mindex();
            }
            else
                reset_spectral_weapon(mon);
        }
    }

    // Change proxPlayer depending on invisibility and standing
    // in shallow water.
    if (proxPlayer && !you.visible_to(mon))
    {
        proxPlayer = false;

        const int intel = mons_intel(mon);
        // Sometimes, if a player is right next to a monster, they will 'see'.
        if (grid_distance(you.pos(), mon->pos()) == 1
            && one_chance_in(3))
        {
            proxPlayer = true;
        }

        // [dshaligram] Very smart monsters have a chance of clueing in to
        // invisible players in various ways.
        if (intel == I_NORMAL && one_chance_in(13)
                 || intel == I_HIGH && one_chance_in(6))
        {
            proxPlayer = true;
        }

        // Ash penance makes monsters very likely to target you through
        // invisibility, depending on their intelligence.
        if (player_under_penance(GOD_ASHENZARI) && x_chance_in_y(intel, 7))
            proxPlayer = true;
    }

    // Zotdef: immobile allies forget targets that are out of sight
    if (crawl_state.game_is_zotdef())
    {
        if (isFriendly && mon->is_stationary()
            && (mon->foe != MHITNOT && mon->foe != MHITYOU)
            && !mon->can_see(&menv[mon->foe]))
        {
            mon->foe = MHITYOU;
            //mprf("%s resetting target (cantSee)",
            //     mon->name(DESC_THE,true).c_str());
        }
    }

    // Set friendly target, if they don't already have one.
    // Berserking allies ignore your commands!
    if (isFriendly
        && (mon->foe == MHITNOT || mon->foe == MHITYOU)
        && !mon->berserk_or_insane()
        && mon->behaviour != BEH_WITHDRAW
        && mon->type != MONS_GIANT_SPORE
        && mon->type != MONS_BATTLESPHERE
        && mon->type != MONS_SPECTRAL_WEAPON)
    {
        if  (!crawl_state.game_is_zotdef())
        {
            if (you.pet_target != MHITNOT)
                mon->foe = you.pet_target;
        }
        else    // Zotdef only
        {
            // Attack pet target if nearby
            if (you.pet_target != MHITNOT && proxPlayer)
            {
                //mprf("%s setting target (player target)",
                //     mon->name(DESC_THE,true).c_str());
                mon->foe = you.pet_target;
            }
            else
            {
               // Zotdef - this is all new, for out-of-sight friendlies to do
               // something useful.  If no current target, get the closest one.
                _set_nearest_monster_foe(mon);
            }
        }
    }

    // Instead, berserkers attack nearest monsters.
    if (mon->behaviour != BEH_SLEEP
        && (mon->has_ench(ENCH_INSANE)
            || ((mon->berserk() || mon->type == MONS_GIANT_SPORE)
                && (mon->foe == MHITNOT
                    || isFriendly && mon->foe == MHITYOU))))
    {
        // Intelligent monsters prefer to attack the player,
        // even when berserking.
        if (!isFriendly
            && !mon->has_ench(ENCH_INSANE)
            && proxPlayer
            && mons_intel(mon) >= I_NORMAL)
        {
            mon->foe = MHITYOU;
        }
        else
            _set_nearest_monster_foe(mon);
    }

    // Pacified monsters leaving the level prefer not to attack.
    // Others choose the nearest foe.
    // XXX: This is currently expensive, so we don't want to do it
    //      every turn for every monster.
    if (!isPacified && mon->foe == MHITNOT
        && mon->behaviour != BEH_SLEEP
        && (proxPlayer || one_chance_in(3)))
    {
        _set_nearest_monster_foe(mon);
        if (mon->foe == MHITNOT && crawl_state.game_is_zotdef())
            mon->foe = MHITYOU;
    }

    // Friendly summons will come back to the player if they go out of sight.
    // Spectral weapon should keep its target even though it can't attack it
    if (!summon_can_attack(mon) && mon->type!=MONS_SPECTRAL_WEAPON)
        mon->target = you.pos();

    // Monsters do not attack themselves. {dlb}
    if (mon->foe == mon->mindex())
        mon->foe = MHITNOT;

    // Friendly and good neutral monsters do not attack other friendly
    // and good neutral monsters.
    if (mon->foe != MHITNOT && mon->foe != MHITYOU
        && wontAttack && menv[mon->foe].wont_attack())
    {
        mon->foe = MHITNOT;
    }

    // Neutral monsters prefer not to attack players, or other neutrals.
    if (isNeutral
        && !mon->has_ench(ENCH_INSANE)
        && mon->foe != MHITNOT
        && (mon->foe == MHITYOU || menv[mon->foe].neutral()))
    {
        mon->foe = MHITNOT;
    }

    // Unfriendly monsters fighting other monsters will usually
    // target the player, if they're healthy.
    // Zotdef: 2/3 chance of retargetting changed to 1/4
    if (!isFriendly && !isNeutral
        && mon->foe != MHITYOU && mon->foe != MHITNOT
        && proxPlayer && !mon->berserk_or_insane()
        && isHealthy
        && (crawl_state.game_is_zotdef() ? one_chance_in(4)
                                         : !one_chance_in(3)))
    {
        mon->foe = MHITYOU;
    }

    // Validate current target again.
    _mon_check_foe_invalid(mon);

    if (mon->has_ench(ENCH_HAUNTING))
    {
        actor* targ = mon->get_ench(ENCH_HAUNTING).agent();
        if (targ && targ->alive())
        {
            mon->foe = targ->mindex();
            mon->target = targ->pos();
        }
    }

    while (changed)
    {
        actor* afoe = mon->get_foe();
        proxFoe = afoe && mon->can_see(afoe);

        if (mon->foe == MHITYOU)
        {
            // monster::get_foe returns NULL for friendly monsters with
            // foe == MHITYOU, so make afoe point to the player here.
            // -cao
            afoe = &you;
            proxFoe = proxPlayer;   // Take invis into account.
        }

        coord_def foepos = coord_def(0,0);
        if (afoe)
            foepos = afoe->pos();

        if (crawl_state.game_is_zotdef() && mon->foe == MHITYOU
            && !mon->wont_attack())
        {
            foepos = PLAYER_POS;
            proxFoe = true;
        }

        if (mon->pos() == mon->firing_pos)
            mon->firing_pos.reset();

        // Track changes to state; attitude never changes here.
        beh_type new_beh       = mon->behaviour;
        unsigned short new_foe = mon->foe;

        // Take care of monster state changes.
        switch (mon->behaviour)
        {
        case BEH_SLEEP:
            // default sleep state
            mon->target = mon->pos();
            new_foe = MHITNOT;
            break;

        case BEH_LURK:
            // Make sure trapdoor spiders are not hiding in plain sight
            if (mon->type == MONS_TRAPDOOR_SPIDER && !mon->submerged())
                mon->add_ench(ENCH_SUBMERGED);

            // Fall through to get a target, but don't change to wandering.

        case BEH_SEEK:
            // No foe?  Then wander or seek the player.
            if (mon->foe == MHITNOT)
            {
                if (crawl_state.game_is_arena()
                    || !proxPlayer && !isFriendly
                    || isNeutral && !mon->has_ench(ENCH_INSANE)
                    || patrolling
                    || mon->type == MONS_GIANT_SPORE)
                {
                    if (mon->behaviour != BEH_LURK)
                        new_beh = BEH_WANDER;
                }
                else
                {
                    new_foe = MHITYOU;
                    mon->target = PLAYER_POS;
                }
                break;
            }

            // Foe gone out of LOS?
            if (!proxFoe
                && !(mon->friendly()
                     && mon->foe == MHITYOU
                     && mon->is_travelling()
                     && mon->travel_target == MTRAV_PLAYER))
            {
                // If their foe is marked, the monster always knows exactly
                // where they are.
                if (mons_foe_is_marked(mon) || mon->has_ench(ENCH_HAUNTING))
                {
                    mon->target = afoe->pos();
                    try_pathfind(mon);
                    break;
                }

                // Maybe the foe is just invisible.
                if (mon->target.origin() && afoe && mon->near_foe())
                {
                    _guess_invis_foe_pos(mon);
                    if (mon->target.origin())
                    {
                        // Having a seeking mon with a foe who's target is
                        // (0, 0) can lead to asserts, so lets try to
                        // avoid that.
                        _set_nearest_monster_foe(mon);
                        if (mon->foe == MHITNOT)
                        {
                            new_beh = BEH_WANDER;
                            break;
                        }
                        mon->target = mon->get_foe()->pos();
                    }
                }

                if (mon->travel_target == MTRAV_SIREN)
                    mon->travel_target = MTRAV_NONE;

                if (isFriendly && mon->foe != MHITYOU)
                {
                    if (patrolling || crawl_state.game_is_arena())
                    {
                        new_foe = MHITNOT;
                        new_beh = BEH_WANDER;
                    }
                    else
                    {
                        new_foe = MHITYOU;
                        mon->target = foepos;
                    }
                    break;
                }

                ASSERT(mon->foe != MHITNOT);
                if (mon->foe_memory > 0)
                {
                    // If we've arrived at our target x,y
                    // do a stealth check.  If the foe
                    // fails, monster will then start
                    // tracking foe's CURRENT position,
                    // but only for a few moves (smell and
                    // intuition only go so far).

                    if (mon->pos() == mon->target
                        && (!isFriendly || !crawl_state.game_is_zotdef()))
                    {   // hostiles only in Zotdef
                        if (mon->foe == MHITYOU)
                        {
                            if (crawl_state.game_is_zotdef())
                                mon->target = PLAYER_POS;  // infallible tracking in zotdef
                            else
                            {
                                if (x_chance_in_y(50, you.stealth())
                                    || you.penance[GOD_ASHENZARI] && coinflip())
                                {
                                    mon->target = you.pos();
                                }
                                else
                                    mon->foe_memory = 0;
                            }
                        }
                        else
                        {
                            if (coinflip())     // XXX: cheesy!
                                mon->target = menv[mon->foe].pos();
                            else
                                mon->foe_memory = 0;
                        }
                    }
                }


                if (mon->foe_memory <= 0
                    && !(mon->friendly() && mon->foe == MHITYOU))
                {
                    new_beh = BEH_WANDER;
                }
                else if ((mon->type == MONS_CURSE_SKULL
                          && mon->foe == MHITYOU
                          && grid_distance(mon->pos(), you.pos()) <= LOS_RADIUS))
                {
                    _set_curse_skull_lurk_pos(mon);
                }
                // If the player walk out of the LOS of a monster with a ranged
                // attack, we assume it sees in which direction the player went
                // and it tries to find a line of fire instead of following the
                // player.
                else if (grid_distance(mon->target, you.pos()) == 1
                         && _mon_tries_regain_los(mon))
                {
                    _set_firing_pos(mon, you.pos());
                }

                if (!isFriendly)
                    break;
            }

            ASSERT(proxFoe || isFriendly);
            ASSERT(mon->foe != MHITNOT);

            // Monster can see foe: set memory in case it loses sight.
            // Hack: smarter monsters will tend to pursue the player longer.
            switch (mons_intel(mon))
            {
            case I_HIGH:
                mon->foe_memory = random_range(700, 1300);
                break;
            case I_NORMAL:
                mon->foe_memory = random_range(300, 700);
                break;
            case I_ANIMAL:
            case I_INSECT:
            case I_REPTILE:
                mon->foe_memory = random_range(250, 550);
                break;
            case I_PLANT:
                mon->foe_memory = random_range(100, 300);
                break;
            }

            // Monster can see foe: continue 'tracking'
            // by updating target x,y.
            if (mon->foe == MHITYOU)
            {
                // The foe is the player.
                if (mons_class_flag(mon->type, M_MAINTAIN_RANGE)
                    && !mon->berserk_or_insane())
                {
                    if (mon->attitude != ATT_FRIENDLY)
                        // Get to firing range even if we are close.
                        _set_firing_pos(mon, you.pos());
                }
                else if (_stabber_keep_distance(mon, &you))
                {
                    if (mon->pos().distance_from(you.pos()) < 4
                        && !one_chance_in(7))
                    {
                        mon->firing_pos = mon->pos();
                    }
                    else
                        _set_firing_pos(mon, you.pos());
                }
                else if (!mon->firing_pos.zero()
                    && mon->see_cell_no_trans(mon->target))
                {
                    // If monster is currently getting into firing position and
                    // sees the player and can attack him, clear firing_pos.
                    mon->firing_pos.reset();
                }

                if (mon->type == MONS_SIREN
                    && you.beheld_by(mon)
                    && find_siren_water_target(mon))
                {
                    break;
                }

                if (mon->firing_pos.zero() && try_pathfind(mon))
                    break;

                // Whew. If we arrived here, path finding didn't yield anything
                // (or wasn't even attempted) and we need to set our target
                // the traditional way.

                // Sometimes, your friends will wander a bit.
                if (isFriendly && one_chance_in(8)
                    && mon->foe == MHITYOU && proxFoe)
                {
                    set_random_target(mon);
                    mon->foe = MHITNOT;
                    new_beh  = BEH_WANDER;
                }
                else
                    mon->target = PLAYER_POS;
            }
            else
            {
                // We have a foe but it's not the player.
                monster* target = &menv[mon->foe];
                mon->target = target->pos();

                if (mons_class_flag(mon->type, M_MAINTAIN_RANGE)
                    && !mon->berserk_or_insane())
                {
                    _set_firing_pos(mon, mon->target);
                }
                else if (target && _stabber_keep_distance(mon, target))
                {
                    if (mon->pos().distance_from(target->pos()) < 4
                        && !one_chance_in(7))
                    {
                        mon->firing_pos = mon->pos();
                    }
                    else
                        _set_firing_pos(mon, target->pos());
                }

            }

            break;

        case BEH_WANDER:
            if (isPacified)
            {
                // If a pacified monster isn't travelling toward
                // someplace from which it can leave the level, make it
                // start doing so.  If there's no such place, either
                // search the level for such a place again, or travel
                // randomly.
                if (mon->travel_target != MTRAV_PATROL)
                {
                    new_foe = MHITNOT;
                    mon->travel_path.clear();

                    e_index = mons_find_nearest_level_exit(mon, e);

                    if (e_index == -1 || one_chance_in(20))
                        e_index = mons_find_nearest_level_exit(mon, e, true);

                    if (e_index != -1)
                    {
                        mon->travel_target = MTRAV_PATROL;
                        patrolling = true;
                        mon->patrol_point = e[e_index].target;
                        mon->target = e[e_index].target;
                    }
                    else
                    {
                        mon->travel_target = MTRAV_NONE;
                        patrolling = false;
                        mon->patrol_point.reset();
                        set_random_target(mon);
                    }
                }

                if (pacified_leave_level(mon, e, e_index))
                    return;
            }

            if (mon->strict_neutral() && mons_is_slime(mon)
                && you_worship(GOD_JIYVA))
            {
                set_random_slime_target(mon);
            }

            // Is our foe in LOS?
            // Batty monsters don't automatically reseek so that
            // they'll flitter away, we'll reset them just before
            // they get movement in handle_monsters() instead. -- bwr
            if (proxFoe && !mons_is_batty(mon) || mons_foe_is_marked(mon))
            {
                new_beh = BEH_SEEK;
                break;
            }

            // Creatures not currently pursuing another foe are
            // alerted by a sentinel's mark
            if (mon->foe == MHITNOT && you.duration[DUR_SENTINEL_MARK]
                && (!isFriendly && !isNeutral && !isPacified
                    || mon->has_ench(ENCH_INSANE)))
            {
                new_foe = MHITYOU;
                new_beh = BEH_SEEK;
                break;
            }

            check_wander_target(mon, isPacified);

            // During their wanderings, monsters will eventually relax
            // their guard (stupid ones will do so faster, smart
            // monsters have longer memories).  Pacified monsters will
            // also eventually switch the place from which they want to
            // leave the level, in case their current choice is blocked.
            if (!proxFoe && mon->foe != MHITNOT
                   && one_chance_in(isSmart ? 60 : 20)
                   && !mons_foe_is_marked(mon)
                || isPacified && one_chance_in(isSmart ? 40 : 120))
            {
                new_foe = MHITNOT;
                if (mon->is_travelling() && mon->travel_target != MTRAV_PATROL
                    || isPacified)
                {
#ifdef DEBUG_PATHFIND
                    mpr("It's been too long! Stop travelling.");
#endif
                    mon->travel_path.clear();
                    mon->travel_target = MTRAV_NONE;

                    if (isPacified && e_index != -1)
                        e[e_index].unreachable = true;
                }
            }
            break;

        case BEH_RETREAT:
            // If the target can be reached, there is a chance the monster will
            // try to attack. The chance is low to prevent the player from
            // dancing in and out of the water.
            try_pathfind(mon);
            if (one_chance_in(10) && !target_is_unreachable(mon)
                || mons_can_attack(mon))
            {
                new_beh = BEH_SEEK;
            }
            else if (!proxPlayer && one_chance_in(5))
                new_beh = BEH_WANDER;
            else if (proxPlayer)
                mon->target = foepos;
            break;

        case BEH_FLEE:
            // Check for healed.
            if (isHealthy && !isScared)
                new_beh = BEH_SEEK;

            // Smart monsters flee until they can flee no more...
            // possible to get a 'CORNERED' event, at which point
            // we can jump back to WANDER if the foe isn't present.

            if (isFriendly)
            {
                // Special-cased below so that it will flee *towards* you.
                if (mon->foe == MHITYOU)
                    mon->target = you.pos();
            }
            else if (mons_wall_shielded(mon) && find_wall_target(mon))
                ; // Wall target found.
            else if (proxFoe)
            {
                // Special-cased below so that it will flee *from* the
                // correct position.
                mon->target = foepos;
            }
            break;

        case BEH_CORNERED:
            // Plants and nonliving monsters cannot fight back.
            if (mon->holiness() == MH_PLANT
                || mon->holiness() == MH_NONLIVING)
            {
                break;
            }

            if (isHealthy)
                new_beh = BEH_SEEK;

            // Foe gone out of LOS?
            if (!proxFoe)
            {
                if ((isFriendly || proxPlayer)
                    && (!isNeutral || mon->has_ench(ENCH_INSANE))
                    && !patrolling
                    && !crawl_state.game_is_arena())
                {
                    new_foe = MHITYOU;
                }
                else
                    new_beh = BEH_WANDER;
            }
            else
                mon->target = foepos;
            break;

        case BEH_WITHDRAW:
        {
            if (!isFriendly)
            {
                new_beh = BEH_WANDER;
                break;
            }

            bool stop_retreat = false;
            // We've approached our next destination, re-evaluate
            if (distance2(mon->target, mon->pos()) <= 3)
            {
                // Continue on to the rally point
                if (mon->target != mon->patrol_point)
                    mon->target = mon->patrol_point;
                // Reached rally point, stop withdrawing
                else
                    stop_retreat = true;

            }
            else if (distance2(mon->pos(), you.pos()) > dist_range(LOS_RADIUS + 2))
            {
                // We're too far from the player. Idle around and wait for
                // them to catch up.
                if (!mon->props.exists("idle_point"))
                {
                    mon->props["idle_point"] = mon->pos();
                    mon->props["idle_deadline"] = you.elapsed_time + 200;
                }

                mon->target = clamp_in_bounds(
                                    mon->props["idle_point"].get_coord()
                                    + coord_def(random_range(-2, 2),
                                                random_range(-2, 2)));

                if (you.elapsed_time >= mon->props["idle_deadline"].get_int())
                    stop_retreat = true;
            }
            else
            {
                // Be more lenient about player distance if a monster is
                // idling (to prevent it from repeatedly resetting idle
                // time if its own wanderings bring it closer to the player)
                if (mon->props.exists("idle_point")
                    && distance2(mon->pos(), you.pos()) < dist_range(LOS_RADIUS))
                {
                    mon->props.erase("idle_point");
                    mon->props.erase("idle_deadline");
                    mon->target = mon->patrol_point;
                }

                if (mon->pos() == mon->props["last_pos"].get_coord())
                {
                    if (!mon->props.exists("blocked_deadline"))
                        mon->props["blocked_deadline"] = you.elapsed_time + 30;

                    if (!mon->props.exists("idle_deadline"))
                        mon->props["idle_deadline"] = you.elapsed_time + 200;

                    if (you.elapsed_time >= mon->props["blocked_deadline"].get_int()
                        || you.elapsed_time >= mon->props["idle_deadline"].get_int())
                    {
                        stop_retreat = true;
                    }
                }
                else
                {
                    mon->props.erase("blocked_deadline");
                    mon->props.erase("idle_deadline");
                }
            }

            if (stop_retreat)
            {
                new_beh = BEH_SEEK;
                new_foe = MHITYOU;
                mon->props.erase("last_pos");
                mon->props.erase("idle_point");
                mon->props.erase("blocked_deadline");
                mon->props.erase("idle_deadline");
            }
            else
                mon->props["last_pos"] = mon->pos();

            break;
        }

        default:
            return;     // uh oh
        }

        changed = (new_beh != mon->behaviour || new_foe != mon->foe);
        mon->behaviour = new_beh;

        if (mon->foe != new_foe)
            mon->foe_memory = 0;

        mon->foe = new_foe;
    }

    if (mon->travel_target == MTRAV_WALL && cell_is_solid(mon->pos()))
    {
        if (mon->behaviour == BEH_FLEE)
        {
            // Monster is safe, so stay put.
            mon->target = mon->pos();
            mon->foe = MHITNOT;
        }
    }
}

static bool _mons_check_foe(monster* mon, const coord_def& p,
                            bool friendly, bool neutral)
{
    // We don't check for the player here because otherwise wandering
    // monsters will always attack you.

    // -- But why should they always attack monsters? -- 1KB

    monster* foe = monster_at(p);
    if (foe && foe != mon
        && (mon->has_ench(ENCH_INSANE)
            || foe->friendly() != friendly
            || neutral && !foe->neutral())
        && mon->can_see(foe)
        && !foe->is_projectile()
        && summon_can_attack(mon, p)
        && (friendly || !is_sanctuary(p))
        && (crawl_state.game_is_zotdef() || !mons_is_firewood(foe)))
            // Zotdef allies take out firewood
    {
        return true;
    }

    if (mon->has_ench(ENCH_INSANE) && p == you.pos())
        return true;

    return false;
}

// Choose random nearest monster as a foe.
static void _set_nearest_monster_foe(monster* mon)
{
    // These don't look for foes.
    if (mon->good_neutral() || mon->strict_neutral()
            || mon->behaviour == BEH_WITHDRAW
            || mon->type == MONS_BATTLESPHERE
            || mon->type == MONS_SPECTRAL_WEAPON
            || mon->has_ench(ENCH_HAUNTING))
    {
        return;
    }

    const bool friendly = mon->friendly();
    const bool neutral  = mon->neutral();

    for (int k = 1; k <= LOS_RADIUS; ++k)
    {
        vector<coord_def> monster_pos;
        for (int i = -k; i <= k; ++i)
            for (int j = -k; j <= k; (abs(i) == k ? j++ : j += 2*k))
            {
                const coord_def p = mon->pos() + coord_def(i, j);
                if (_mons_check_foe(mon, p, friendly, neutral))
                    monster_pos.push_back(p);
            }
        if (monster_pos.empty())
            continue;

        const coord_def mpos = monster_pos[random2(monster_pos.size())];
        if (mpos == you.pos())
            mon->foe = MHITYOU;
        else
            mon->foe = env.mgrid(mpos);
        return;
    }
}

//-----------------------------------------------------------------
//
// behaviour_event
//
// 1. Change any of: monster state, foe, and attitude
// 2. Call handle_behaviour to re-evaluate AI state and target x, y
//
//-----------------------------------------------------------------
void behaviour_event(monster* mon, mon_event_type event, const actor *src,
                     coord_def src_pos, bool allow_shout)
{
    if (!mon->alive())
        return;

    ASSERT(!crawl_state.game_is_arena() || src != &you);
    ASSERT_IN_BOUNDS_OR_ORIGIN(src_pos);
    if (mons_is_projectile(mon->type))
        return; // projectiles have no AI

    const beh_type old_behaviour = mon->behaviour;

    int fleeThreshold = min(mon->max_hit_points / 4, 20);

    bool isSmart          = (mons_intel(mon) > I_ANIMAL);
    bool wontAttack       = mon->wont_attack();
    bool sourceWontAttack = false;
    bool setTarget        = false;
    bool breakCharm       = false;
    bool was_sleeping     = mon->asleep();
    string msg;
    int src_idx           = src ? src->mindex() : MHITNOT; // AXE ME

    if (src)
        sourceWontAttack = src->wont_attack();

    if (is_sanctuary(mon->pos()) && mons_is_fleeing_sanctuary(mon))
    {
        mon->behaviour = BEH_FLEE;
        mon->foe       = MHITYOU;
        mon->target    = env.sanctuary_pos;
        return;
    }

    switch (event)
    {
    case ME_DISTURB:
        // Assumes disturbed by noise...
        if (mon->asleep())
        {
            mon->behaviour = BEH_WANDER;

            if (mons_near(mon))
                remove_auto_exclude(mon, true);
        }

        // A bit of code to make Projected Noise actually do
        // something again.  Basically, dumb monsters and
        // monsters who aren't otherwise occupied will at
        // least consider the (apparent) source of the noise
        // interesting for a moment. -- bwr
        if (!isSmart || mon->foe == MHITNOT || mons_is_wandering(mon))
        {
            if (mon->is_patrolling())
                break;

            ASSERT(!src_pos.origin());
            mon->target = src_pos;
        }
        break;

    case ME_WHACK:
    case ME_ANNOY:
        // Orders to withdraw take precedence over interruptions
        if (mon->behaviour == BEH_WITHDRAW)
            break;

        // Will turn monster against <src>, unless they
        // are BOTH friendly or good neutral AND stupid,
        // or else fleeing anyway.  Hitting someone over
        // the head, of course, always triggers this code.
        if (event != ME_WHACK
            && !mon->has_ench(ENCH_INSANE)
            && (wontAttack == sourceWontAttack && mons_intel(mon) <= I_PLANT
                || mons_is_fleeing(mon)
                || mons_class_flag(mon->type, M_FLEEING)
                || mons_is_panicking(mon)))
        {
            break;
        }

        // Monster types that you can't gain experience from cannot
        // fight back, so don't bother having them do so.  If you
        // worship Fedhas, create a ring of friendly plants, and try
        // to break out of the ring by killing a plant, you'll get
        // a warning prompt and penance only once.  Without the
        // hostility check, the plant will remain friendly until it
        // dies, and you'll get a warning prompt and penance once
        // *per hit*.  This may not be the best way to address the
        // issue, though. -cao
        if (mons_class_flag(mon->type, M_NO_EXP_GAIN)
            && mon->attitude != ATT_FRIENDLY
            && mon->attitude != ATT_GOOD_NEUTRAL)
        {
            return;
        }

        mon->foe = src_idx;

        if (mon->asleep() && mons_near(mon))
            remove_auto_exclude(mon, true);

        // If the monster can't reach its target and can't attack it
        // either, retreat.
        try_pathfind(mon);
        if (mons_intel(mon) > I_REPTILE && !mons_can_attack(mon)
            && target_is_unreachable(mon))
        {
            mon->behaviour = BEH_RETREAT;
        }
        else if (!mons_is_cornered(mon) && (mon->hit_points > fleeThreshold))
            mon->behaviour = BEH_SEEK;
        else if (mon->asleep())
            mon->behaviour = BEH_SEEK;

        if (src == &you
            && !mon->has_ench(ENCH_INSANE)
            && mon->type != MONS_BATTLESPHERE
            && mon->type != MONS_SPECTRAL_WEAPON)
        {
            mon->attitude = ATT_HOSTILE;
            breakCharm    = true;
        }

        // XXX: Somewhat hacky, this being here.
        if (mons_is_elven_twin(mon))
            elven_twins_unpacify(mon);

        // Now set target so that monster can whack back (once) at an
        // invisible foe.
        if (event == ME_WHACK)
            setTarget = true;

        break;

    case ME_ALERT:
        // Allow monsters falling asleep while patrolling (can happen if
        // they're left alone for a long time) to be woken by this event.
        if (mon->friendly() && mon->is_patrolling()
            && !mon->asleep())
        {
            break;
        }

        // Orders to withdraw take precedence over interruptions
        if (mon->behaviour == BEH_WITHDRAW)
            break;

        // Avoid moving friendly giant spores out of BEH_WANDER.
        if (mon->friendly() && mon->type == MONS_GIANT_SPORE)
            break;

        // [ds] Neutral monsters don't react to your presence.
        // XXX: Neutral monsters are a tangled mess of arbitrary logic.
        // It's not even clear any more what behaviours are intended for
        // neutral monsters and what are merely accidents of the code.
        if (mon->neutral() && !mon->has_ench(ENCH_INSANE))
        {
            if (mon->asleep())
                mon->behaviour = BEH_WANDER;
            break;
        }

        if (mon->asleep() && mons_near(mon))
            remove_auto_exclude(mon, true);

        // Will alert monster to <src> and turn them
        // against them, unless they have a current foe.
        // It won't turn friends hostile either.
        if ((!mons_is_fleeing(mon) || mons_class_flag(mon->type, M_FLEEING))
            && !mons_is_retreating(mon)
            && !mons_is_panicking(mon)
            && !mons_is_cornered(mon))
        {
            mon->behaviour = BEH_SEEK;
        }

        if (mon->foe == MHITNOT)
            mon->foe = src_idx;

        if (!src_pos.origin()
            && (mon->foe == MHITNOT || src && mon->foe == src->mindex()
                || mons_is_wandering(mon)))
        {
            if (mon->is_patrolling())
                break;

            mon->target = src_pos;

            // XXX: Should this be done in _handle_behaviour()?
            if (src == &you && src_pos == you.pos()
                && !you.see_cell(mon->pos()))
            {
                try_pathfind(mon);
            }
        }
        break;

    case ME_SCARE:
        // Stationary monsters can't flee, and berserking monsters
        // are too enraged.
        if (mon->is_stationary() || mon->berserk_or_insane())
        {
            mon->del_ench(ENCH_FEAR, true, true);
            break;
        }

        // Neither do plants or nonliving beings.
        if (mon->holiness() == MH_PLANT
            || mon->holiness() == MH_NONLIVING)
        {
            mon->del_ench(ENCH_FEAR, true, true);
            break;
        }

        msg = getSpeakString(mon->name(DESC_PLAIN) + " flee");

        // Assume monsters know where to run from, even if player is
        // invisible.
        mon->behaviour = BEH_FLEE;
        mon->foe       = src_idx;
        mon->target    = src_pos;
        if (src == &you)
        {
            // Friendly monsters don't become hostile if you read a
            // scroll of fear, but enslaved ones will.
            // Send friendlies off to a random target so they don't cling
            // to you in fear.
            if (mon->friendly())
            {
                breakCharm = true;
                mon->foe   = MHITNOT;
                set_random_target(mon);
            }
            else
                setTarget = true;
        }
        else if (mon->friendly() && !crawl_state.game_is_arena())
            mon->foe = MHITYOU;

        if (you.see_cell(mon->pos()))
            learned_something_new(HINT_FLEEING_MONSTER);

        break;

    case ME_CORNERED:
        // Some monsters can't flee.
        if (!mons_is_retreating(mon) && !mon->has_ench(ENCH_FEAR))
            break;

        // Pacified monsters shouldn't change their behaviour.
        if (mon->pacified())
            break;

        // Just set behaviour... foe doesn't change.
        if (!mons_is_cornered(mon) && !mon->has_ench(ENCH_WITHDRAWN))
        {
            if (mon->friendly() && !crawl_state.game_is_arena())
            {
                mon->foe = MHITYOU;
                msg = "PLAIN:@The_monster@ returns to your side!";
            }
            else if (!mon->is_child_tentacle())
            {
                msg = getSpeakString(mon->name(DESC_PLAIN) + " cornered");
                if (msg.empty())
                    msg = "PLAIN:@The_monster@ turns to fight!";
            }
        }

        mon->behaviour = BEH_CORNERED;
        break;

    case ME_HURT:
        // Monsters with the M_FLEES flag can flee at low HP.
        // Cannot flee if cornered.
        // Monster can flee if HP is less than 1/4 maxhp or less than 20 hp
        // (whichever is lower). Chance starts quite low, and is near 100% at 1.
        // Monsters with less than 8 maxhp are unable to flee.
        // These numbers could still use some adjusting.
        //
        // Assuming fleeThreshold is 20:
        //   at 19 hp: 5% chance of fleeing
        //   at 10 hp: 50% chance of fleeing
        //   (chance increases by 5% for every hp lost.)
        if (mons_class_flag(mon->type, M_FLEES)
            && !mons_is_cornered(mon)
            && !mon->berserk_or_insane()
            && x_chance_in_y(fleeThreshold - mon->hit_points, fleeThreshold))
        {
            mon->behaviour = BEH_FLEE;
        }
        break;

    case ME_EVAL:
        break;
    }

    if (setTarget && src)
    {
        mon->target = src_pos;
        if (src->is_player()
            && !mon->has_ench(ENCH_INSANE)
            && mon->type != MONS_BATTLESPHERE
            && mon->type != MONS_SPECTRAL_WEAPON)
        {
            // Why only attacks by the player change attitude? -- 1KB
            mon->attitude = ATT_HOSTILE;
            mons_att_changed(mon);
        }
    }

    // Now, break charms if appropriate.
    if (breakCharm)
    {
        mon->del_ench(ENCH_CHARM);
        mons_att_changed(mon);
    }

    // Do any resultant foe or state changes.
    handle_behaviour(mon);

    // That might have made the monster leave the level.
    if (!mon->alive())
        return;

    ASSERT_IN_BOUNDS_OR_ORIGIN(mon->target);

    // If it woke up and you're its new foe, it might shout.
    if (was_sleeping && !mon->asleep() && allow_shout
        && mon->foe == MHITYOU && !mon->wont_attack())
    {
        handle_monster_shouts(mon);
    }

    const bool wasLurking =
        (old_behaviour == BEH_LURK && !mons_is_lurking(mon));
    const bool isPacified = mon->pacified();

    if ((wasLurking || isPacified)
        && (event == ME_DISTURB || event == ME_ALERT || event == ME_EVAL))
    {
        // Lurking monsters or pacified monsters leaving the level won't
        // stop doing so just because they noticed something.
        mon->behaviour = old_behaviour;
    }
    else if (mon->has_ench(ENCH_SUBMERGED) && !mon->del_ench(ENCH_SUBMERGED))
    {
        // The same goes for submerged monsters, if they can't
        // unsubmerge.
        mon->behaviour = BEH_LURK;
    }

    // mons_speaks_msg already handles the LOS check.
    if (!msg.empty() && mon->visible_to(&you))
        mons_speaks_msg(mon, msg, MSGCH_TALK, silenced(mon->pos()));

    if (mons_allows_beogh_now(mon))
    {
        const bool first = !you.attribute[ATTR_SEEN_BEOGH];
        if (first || one_chance_in(10))
        {
            mons_speaks_msg(mon, getSpeakString("orc_priest_preaching"),
                            MSGCH_TALK);
            if (first)
            {
                ASSERT_RANGE(get_talent(ABIL_CONVERT_TO_BEOGH, false).hotkey,
                             'A', 'z' + 1);
                mprf("(press <white>%s %c</white> to convert to Beogh)",
                     command_to_string(CMD_USE_ABILITY).c_str(),
                     get_talent(ABIL_CONVERT_TO_BEOGH, false).hotkey);
                you.attribute[ATTR_SEEN_BEOGH] = 1;
            }
        }
    }

    ASSERT(!crawl_state.game_is_arena()
           || mon->foe != MHITYOU && mon->target != you.pos());
}

void make_mons_stop_fleeing(monster* mon)
{
    if (mons_is_retreating(mon))
        behaviour_event(mon, ME_CORNERED);
}

//Make all monsters lose track of a given target after a few turns
void shake_off_monsters(const actor* target)
{
    //If the player is under Ashenzari penance, monsters will not
    //lose track of them so easily
    if (target->is_player() && you.penance[GOD_ASHENZARI])
        return;

    for (monster_iterator mi; mi; ++mi)
    {
        monster* m = mi->as_monster();
        if (m->foe == target->mindex() && m->foe_memory > 0)
        {
            // Set foe_memory to a small non-zero amount so that monsters can
            // still close in on your old location, rather than immediately
            // realizing their target is gone, even if they took stairs while
            // out of sight
            dprf("Monster %d forgot about foe %d. (Previous foe_memory: %d)",
                    m->mindex(), target->mindex(), m->foe_memory);
            m->foe_memory = min(m->foe_memory, 7);
        }
    }
}
