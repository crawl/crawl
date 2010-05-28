/*
 *  File:       mon-behv.h
 *  Summary:    Monster behaviour functions.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"
#include "mon-behv.h"

#include "externs.h"

#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "fprop.h"
#include "exclude.h"
#include "mon-death.h"
#include "mon-iter.h"
#include "mon-movetarget.h"
#include "mon-pathfind.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "random.h"
#include "state.h"
#include "terrain.h"
#include "traps.h"
#include "hints.h"
#include "view.h"
#include "shout.h"

static void _set_nearest_monster_foe(monsters *monster);

static void _guess_invis_foe_pos(monsters *mon, bool strict = true)
{
    const actor* foe          = mon->get_foe();
    const int    guess_radius = mons_sense_invis(mon) ? 3 : 2;

    std::vector<coord_def> possibilities;

    for (radius_iterator ri(mon->pos(), guess_radius, C_ROUND); ri; ++ri)
    {
        // NOTE: This depends on mon_see_cell() ignoring clouds,
        // so that cells hidden by opaque clouds are included
        // as a possibility for the foe's location.
        if (!strict || foe->is_habitable(*ri) && mon->mon_see_cell(*ri))
            possibilities.push_back(*ri);
    }

    // If being strict (monster must see possible cell, foe must be
    // able to live there) gives no possibilites, then find *some*
    // cell near the foe.
    if (strict && possibilities.empty())
    {
        _guess_invis_foe_pos(mon, false);
        return;
    }

    if (!possibilities.empty())
        mon->target = possibilities[random2(possibilities.size())];
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
void handle_behaviour(monsters *mon)
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
    bool wontAttack = mon->wont_attack();

    // Whether the player is in LOS of the monster and can see
    // or has guessed the player's location.
    bool proxPlayer = mons_near(mon) && !crawl_state.game_is_arena();

#ifdef WIZARD
    // If stealth is greater than actually possible (wizmode level)
    // pretend the player isn't there, but only for hostile monsters.
    if (proxPlayer && you.skills[SK_STEALTH] > 27 && !mon->wont_attack())
        proxPlayer = false;
#endif
    bool proxFoe;
    bool isHurt     = (mon->hit_points <= mon->max_hit_points / 4 - 1);
    bool isHealthy  = (mon->hit_points > mon->max_hit_points / 2);
    bool isSmart    = (mons_intel(mon) > I_ANIMAL);
    bool isScared   = mon->has_ench(ENCH_FEAR);
    bool isMobile   = !mons_is_stationary(mon);
    bool isPacified = mon->pacified();
    bool patrolling = mon->is_patrolling();
    static std::vector<level_exit> e;
    static int                     e_index = -1;

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

    // Make sure monsters are not targeting the player in arena mode.
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

    const dungeon_feature_type can_move =
        (mons_amphibious(mon)) ? DNGN_DEEP_WATER : DNGN_SHALLOW_WATER;

    // Validate current target exists.
    if (mon->foe != MHITNOT && mon->foe != MHITYOU)
    {
        const monsters& foe_monster = menv[mon->foe];
        if (!foe_monster.alive())
            mon->foe = MHITNOT;
        if (foe_monster.friendly() == isFriendly)
            mon->foe = MHITNOT;
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
    }

    // Set friendly target, if they don't already have one.
    // Berserking allies ignore your commands!
    if (isFriendly
        && you.pet_target != MHITNOT
        && (mon->foe == MHITNOT || mon->foe == MHITYOU)
        && !mon->berserk()
        && mon->type != MONS_GIANT_SPORE)
    {
        mon->foe = you.pet_target;
    }

    // Instead, berserkers attack nearest monsters.
    if ((mon->berserk() || mon->type == MONS_GIANT_SPORE)
        && (mon->foe == MHITNOT || isFriendly && mon->foe == MHITYOU))
    {
        // Intelligent monsters prefer to attack the player,
        // even when berserking.
        if (!isFriendly && proxPlayer && mons_intel(mon) >= I_NORMAL)
            mon->foe = MHITYOU;
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
    }

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
    if (isNeutral && mon->foe != MHITNOT
        && (mon->foe == MHITYOU || menv[mon->foe].neutral()))
    {
        mon->foe = MHITNOT;
    }

    // Unfriendly monsters fighting other monsters will usually
    // target the player, if they're healthy.
    if (!isFriendly && !isNeutral
        && mon->foe != MHITYOU && mon->foe != MHITNOT
        && proxPlayer && !mon->berserk() && isHealthy
        && !one_chance_in(3))
    {
        mon->foe = MHITYOU;
    }

    // Validate current target again.
    if (mon->foe != MHITNOT && mon->foe != MHITYOU)
    {
        const monsters& foe_monster = menv[mon->foe];
        if (!foe_monster.alive())
            mon->foe = MHITNOT;
        if (foe_monster.friendly() == isFriendly)
            mon->foe = MHITNOT;
    }

    while (changed)
    {
        actor* afoe = mon->get_foe();
        proxFoe = afoe && mon->can_see(afoe);

        if (mon->foe == MHITYOU)
        {
            // monsters::get_foe returns NULL for friendly monsters with
            // foe == MHITYOU, so make afoe point to the player here.
            // -cao
            afoe = &you;
            proxFoe = proxPlayer;   // Take invis into account.
        }

        coord_def foepos = coord_def(0,0);
        if (afoe)
            foepos = afoe->pos();


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
        case BEH_SEEK:
            // No foe?  Then wander or seek the player.
            if (mon->foe == MHITNOT)
            {
                if (crawl_state.game_is_arena() || !proxPlayer || isNeutral || patrolling
                    || mon->type == MONS_GIANT_SPORE)
                {
                    new_beh = BEH_WANDER;
                }
                else
                {
                    new_foe = MHITYOU;
                    mon->target = you.pos();
                }
                break;
            }

            // Foe gone out of LOS?
            if (!proxFoe)
            {
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

                if (mon->foe == MHITYOU && mon->is_travelling()
                    && mon->travel_target == MTRAV_PLAYER)
                {
                    // We've got a target, so we'll continue on our way.
#ifdef DEBUG_PATHFIND
                    mpr("Player out of LoS... start wandering.");
#endif
                    new_beh = BEH_WANDER;
                    break;
                }

                if (isFriendly)
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

                    if (mon->pos() == mon->target)
                    {
                        if (mon->foe == MHITYOU)
                        {
                            if (one_chance_in(you.skills[SK_STEALTH]/3))
                                mon->target = you.pos();
                            else
                                mon->foe_memory = 0;
                        }
                        else
                        {
                            if (coinflip())     // XXX: cheesy!
                                mon->target = menv[mon->foe].pos();
                            else
                                mon->foe_memory = 0;
                        }
                    }

                    // Either keep chasing, or start wandering.
                    if (mon->foe_memory < 2)
                    {
                        mon->foe_memory = 0;
                        new_beh = BEH_WANDER;
                    }
                    break;
                }

                ASSERT(mon->foe_memory == 0);
                // Hack: smarter monsters will tend to pursue the player longer.
                switch (mons_intel(mon))
                {
                case I_HIGH:
                    mon->foe_memory = 100 + random2(200);
                    break;
                case I_NORMAL:
                    mon->foe_memory = 50 + random2(100);
                    break;
                case I_ANIMAL:
                case I_INSECT:
                    mon->foe_memory = 25 + random2(75);
                    break;
                case I_PLANT:
                    mon->foe_memory = 10 + random2(50);
                    break;
                }
                break;  // switch/case BEH_SEEK
            }

            ASSERT(proxFoe && mon->foe != MHITNOT);
            // Monster can see foe: continue 'tracking'
            // by updating target x,y.
            if (mon->foe == MHITYOU)
            {
                // The foe is the player.
                if (mon->type == MONS_SIREN
                    && you.beheld_by(mon)
                    && find_siren_water_target(mon))
                {
                    break;
                }

                if (try_pathfind(mon, can_move))
                    break;

                // Whew. If we arrived here, path finding didn't yield anything
                // (or wasn't even attempted) and we need to set our target
                // the traditional way.

                // Sometimes, your friends will wander a bit.
                if (isFriendly && one_chance_in(8))
                {
                    set_random_target(mon);
                    mon->foe = MHITNOT;
                    new_beh  = BEH_WANDER;
                }
                else
                {
                    mon->target = you.pos();
                }
            }
            else
            {
                // We have a foe but it's not the player.
                mon->target = menv[mon->foe].pos();
            }

            // Smart monsters, zombified monsters other than spectral
            // things, plants, and nonliving monsters cannot flee.
            if (isHurt && !isSmart && isMobile
                && (!mons_is_zombified(mon) || mon->type == MONS_SPECTRAL_THING)
                && mon->holiness() != MH_PLANT
                && mon->holiness() != MH_NONLIVING)
            {
                new_beh = BEH_FLEE;

                // This is here instead of in the BEH_FLEE section of the switch
                // for handle_behaviour, as it only needs to happen once per
                // fleeing.
                if (mon->type == MONS_KRAKEN)
                {
                    int tcount = 0;
                    int headnum = mon->mindex();
                    for (monster_iterator mi; mi; ++mi)
                        if (mi->type == MONS_KRAKEN_TENTACLE
                            && (int)mi->number == headnum)
                        {
                            monster_die(*mi, KILL_MISC, NON_MONSTER, true);
                            tcount++;
                        }

                    if (tcount > 0)
                        mpr("The kraken's tentacles slip beneath the water.",
                            MSGCH_WARN);
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
                && you.religion == GOD_JIYVA)
            {
                set_random_slime_target(mon);
            }

            // Is our foe in LOS?
            // Batty monsters don't automatically reseek so that
            // they'll flitter away, we'll reset them just before
            // they get movement in handle_monsters() instead. -- bwr
            if (proxFoe && !mons_is_batty(mon))
            {
                new_beh = BEH_SEEK;
                break;
            }

            check_wander_target(mon, isPacified, can_move);

            // During their wanderings, monsters will eventually relax
            // their guard (stupid ones will do so faster, smart
            // monsters have longer memories).  Pacified monsters will
            // also eventually switch the place from which they want to
            // leave the level, in case their current choice is blocked.
            if (!proxFoe && mon->foe != MHITNOT
                   && one_chance_in(isSmart ? 60 : 20)
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
                if ((isFriendly || proxPlayer) && !isNeutral && !patrolling
                    && !crawl_state.game_is_arena())
                {
                    new_foe = MHITYOU;
                }
                else
                    new_beh = BEH_WANDER;
            }
            else
            {
                mon->target = foepos;
            }
            break;

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

static bool _mons_check_foe(monsters *mon, const coord_def& p,
                            bool friendly, bool neutral)
{
    if (!in_bounds(p))
        return (false);

    if (p == you.pos())
    {
        // The player: We don't return true here because
        // otherwise wandering monsters will always
        // attack the player.
        return (false);
    }

    if (monsters *foe = monster_at(p))
    {
        if (foe != mon
            && mon->can_see(foe)
            && !mons_is_projectile(foe->type)
            && (friendly || !is_sanctuary(p))
            && (foe->friendly() != friendly
                || neutral && !foe->neutral())
            && !mons_is_firewood(foe))
        {
            return (true);
        }
    }
    return (false);
}

// Choose random nearest monster as a foe.
void _set_nearest_monster_foe(monsters *mon)
{
    const bool friendly = mon->friendly();
    const bool neutral  = mon->neutral();

    for (int k = 1; k <= LOS_RADIUS; ++k)
    {
        std::vector<coord_def> monster_pos;
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
void behaviour_event(monsters *mon, mon_event_type event, int src,
                     coord_def src_pos, bool allow_shout)
{
    if (!mon->alive())
        return;

    ASSERT(src >= 0 && src <= MHITYOU);
    ASSERT(!crawl_state.game_is_arena() || src != MHITYOU);
    ASSERT(in_bounds(src_pos) || src_pos.origin());
    if (mons_is_projectile(mon->type))
        return; // projectiles have no AI

    const beh_type old_behaviour = mon->behaviour;

    bool isSmart          = (mons_intel(mon) > I_ANIMAL);
    bool wontAttack       = mon->wont_attack();
    bool sourceWontAttack = false;
    bool setTarget        = false;
    bool breakCharm       = false;
    bool was_sleeping     = mon->asleep();

    if (src == MHITYOU)
        sourceWontAttack = true;
    else if (src != MHITNOT)
        sourceWontAttack = menv[src].wont_attack();

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
        // Will turn monster against <src>, unless they
        // are BOTH friendly or good neutral AND stupid,
        // or else fleeing anyway.  Hitting someone over
        // the head, of course, always triggers this code.
        if (event == ME_WHACK
            || ((wontAttack != sourceWontAttack || isSmart)
                && !mons_is_fleeing(mon) && !mons_is_panicking(mon)))
        {
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

            mon->foe = src;

            if (mon->asleep() && mons_near(mon))
                remove_auto_exclude(mon, true);

            if (!mons_is_cornered(mon))
                mon->behaviour = BEH_SEEK;

            if (src == MHITYOU)
            {
                mon->attitude = ATT_HOSTILE;
                breakCharm    = true;
            }

            // XXX: Somewhat hacky, this being here.
            if (mons_is_elven_twin(mon))
                elven_twins_unpacify(mon);
        }

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

        // Avoid moving friendly giant spores out of BEH_WANDER.
        if (mon->friendly() && mon->type == MONS_GIANT_SPORE)
            break;

        if (mon->asleep() && mons_near(mon))
            remove_auto_exclude(mon, true);

        // Will alert monster to <src> and turn them
        // against them, unless they have a current foe.
        // It won't turn friends hostile either.
        if (!mons_is_fleeing(mon) && !mons_is_panicking(mon)
            && !mons_is_cornered(mon))
        {
            mon->behaviour = BEH_SEEK;
        }

        if (mon->foe == MHITNOT)
            mon->foe = src;

        if (!src_pos.origin()
            && (mon->foe == MHITNOT || mon->foe == src
                || mons_is_wandering(mon)))
        {
            if (mon->is_patrolling())
                break;

            mon->target = src_pos;

            // XXX: Should this be done in _handle_behaviour()?
            if (src == MHITYOU && src_pos == you.pos()
                && !you.see_cell(mon->pos()))
            {
                const dungeon_feature_type can_move =
                    (mons_amphibious(mon)) ? DNGN_DEEP_WATER
                                           : DNGN_SHALLOW_WATER;

                try_pathfind(mon, can_move);
            }
        }
        break;

    case ME_SCARE:
        // Stationary monsters can't flee, and berserking monsters
        // are too enraged.
        if (mons_is_stationary(mon) || mon->berserk())
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

        // Assume monsters know where to run from, even if player is
        // invisible.
        mon->behaviour = BEH_FLEE;
        mon->foe       = src;
        mon->target    = src_pos;
        if (src == MHITYOU)
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

        if (mon->type == MONS_KRAKEN)
        {
            int tcount = 0;
            int headnum = mon->mindex();
            for (monster_iterator mi; mi; ++mi)
                if (mi->type == MONS_KRAKEN_TENTACLE
                    && (int)mi->number == headnum)
                {
                    monster_die(*mi, KILL_MISC, NON_MONSTER, true);
                    tcount++;
                }

            if (tcount > 0)
                mpr("The kraken's tentacles slip beneath the water.", MSGCH_WARN);
        }

        break;

    case ME_CORNERED:
        // Some monsters can't flee.
        if (mon->behaviour != BEH_FLEE && !mon->has_ench(ENCH_FEAR))
            break;

        // Pacified monsters shouldn't change their behaviour.
        if (mon->pacified())
            break;

        // Just set behaviour... foe doesn't change.
        if (!mons_is_cornered(mon))
        {
            if (mon->friendly() && !crawl_state.game_is_arena())
            {
                mon->foe = MHITYOU;
                simple_monster_message(mon, " returns to your side!");
            }
            else if (mon->type != MONS_KRAKEN_TENTACLE)
                simple_monster_message(mon, " turns to fight!");
        }

        mon->behaviour = BEH_CORNERED;
        break;

    case ME_EVAL:
        break;
    }

    if (setTarget)
    {
        if (src == MHITYOU)
        {
            mon->target = you.pos();
            mon->attitude = ATT_HOSTILE;
            mons_att_changed(mon);
        }
        else if (src != MHITNOT)
            mon->target = src_pos;
    }

    // Now, break charms if appropriate.
    if (breakCharm)
    {
        mon->del_ench(ENCH_CHARM);
        mons_att_changed(mon);
    }

    // Do any resultant foe or state changes.
    handle_behaviour(mon);
    ASSERT(in_bounds(mon->target) || mon->target.origin());

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
    else if (wasLurking && mon->has_ench(ENCH_SUBMERGED)
             && !mon->del_ench(ENCH_SUBMERGED))
    {
        // The same goes for lurking submerged monsters, if they can't
        // unsubmerge.
        mon->behaviour = BEH_LURK;
    }

    ASSERT(!crawl_state.game_is_arena()
           || mon->foe != MHITYOU && mon->target != you.pos());
}

void make_mons_stop_fleeing(monsters *mon)
{
    if (mons_is_fleeing(mon))
        behaviour_event(mon, ME_CORNERED);
}
