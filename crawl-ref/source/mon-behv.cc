/**
 * @file
 * @brief Monster behaviour functions.
**/

#include "AppHdr.h"

#include "mon-behv.h"

#include "ability.h"
#include "act-iter.h"
#include "areas.h"
#include "attitude-change.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgn-overview.h"
#include "dungeon.h"
#include "fineff.h"
#include "god-passive.h"
#include "hints.h"
#include "item-prop.h"
#include "losglobal.h"
#include "macro.h"
#include "message.h"
#include "mon-act.h"
#include "mon-death.h"
#include "mon-movetarget.h"
#include "mon-speak.h"
#include "ouch.h"
#include "religion.h"
#include "shout.h"
#include "spl-summoning.h"
#include "state.h"
#include "stringutil.h"
#include "terrain.h"
#include "traps.h"

static void _guess_invis_foe_pos(monster* mon)
{
    const actor* foe          = mon->get_foe();
    const int    guess_radius = 2;

    vector<coord_def> possibilities;

    // NOTE: This depends on ignoring clouds, so that cells hidden by
    // opaque clouds are included as a possibility for the foe's location.
    for (radius_iterator ri(mon->pos(), guess_radius, C_SQUARE, LOS_SOLID); ri; ++ri)
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
    if (mons_is_avatar(mon->type))
        return;

    if (mon->foe != MHITNOT && mon->foe != MHITYOU)
    {
        if (actor *foe = mon->get_foe())
        {
            const monster* foe_mons = foe->as_monster();
            if (foe_mons->alive() && monster_los_is_valid(mon, foe)
                && (mon->has_ench(ENCH_FRENZIED)
                    || mon->friendly() != foe_mons->friendly()
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
    if (mons_intel(*mon) < I_HUMAN || !mons_has_ranged_attack(*mon))
        return false;

    // Any special case should go here.
    if (mons_class_flag(mon->type, M_FIGHTER)
        && !(mon->type == MONS_CENTAUR_WARRIOR)
        && !(mon->type == MONS_YAKTAUR_CAPTAIN))
    {
        return false;
    }

    // Randomize it to make it less predictable, and reduce flip-flopping.
    return !one_chance_in(3);
}

// Monster tries to get into a firing position. Among the cells which have
// a line of fire to the target, we choose the closest one to regain LOS as
// fast as possible. If several cells are eligible, we choose the one closest
// to ideal_range (too far = easier to escape, too close = easier to ambush).
static void _set_firing_pos(monster* mon, coord_def target)
{
    const int ideal_range = LOS_DEFAULT_RANGE / 2;
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

static void _decide_monster_firing_position(monster* mon, actor* owner)
{
    // Monster can see foe: continue 'tracking'
    // by updating target x,y.
    if (mon->foe == MHITYOU)
    {
        const bool ignore_special_firing_AI = mon->friendly()
                                              || mon->berserk_or_frenzied();

        // The foe is the player.
        if (mons_class_flag(mon->type, M_MAINTAIN_RANGE)
            && !ignore_special_firing_AI)
            {
                // Get to firing range even if we are close.
                _set_firing_pos(mon, you.pos());
            }
        else if (mon->type == MONS_MERFOLK_AVATAR && !ignore_special_firing_AI)
            find_merfolk_avatar_water_target(mon);
        else if (!mon->firing_pos.zero()
                 && mon->see_cell_no_trans(mon->target))
        {
            // If monster is currently getting into firing position and
            // sees the player and can attack him, clear firing_pos.
            mon->firing_pos.reset();
        }

        if (!(mon->firing_pos.zero() && try_pathfind(mon)))
        {
            // Whew. If we arrived here, path finding didn't yield anything
            // (or wasn't even attempted) and we need to set our target
            // the traditional way.

            mon->target = you.pos();
        }
    }
    else
    {
        // We have a foe but it's not the player.
        monster* target = &env.mons[mon->foe];
        mon->target = target->pos();

        if (mons_class_flag(mon->type, M_MAINTAIN_RANGE)
            && !mon->berserk_or_frenzied()
            && !(mons_is_avatar(mon->type)
                 && owner && mon->foe == owner->mindex()))
        {
            _set_firing_pos(mon, mon->target);
        }
        // Hold position if we've reached our ideal range
        else if (mon->type == MONS_SPELLFORGED_SERVITOR
                 && (mon->pos() - target->pos()).rdist()
                 <= mon->props[IDEAL_RANGE_KEY].get_int()
                 && !one_chance_in(8))
        {
            mon->firing_pos = mon->pos();
        }
    }
}

/**
 * Should the player be treated as if they were normally visible to a given
 * monster, even though they're currently invisible?
 *
 * Random per-call; depends on proximity, monster intelligence, and Ash wrath.
 *
 * @param mon     The monster in question.
 * @param         Whether the monster correctly guessed the player's presence.
 */
static bool _monster_guesses_invis_player(const monster &mon)
{
    // Sometimes, if a player is right next to a monster, they will 'see'.
    if (grid_distance(you.pos(), mon.pos()) == 1 && one_chance_in(3))
        return true;

    // [dshaligram] Smart monsters have a chance of clueing in to
    // invisible players in various ways.
    if (mons_intel(mon) == I_HUMAN && one_chance_in(12))
        return true;

    // Ash penance makes monsters very likely to target you through invis.
    if (player_under_penance(GOD_ASHENZARI) && coinflip())
        return true;

    return false;
}

/**
 * Evaluates the monster's AI state, and sets its target based on its foe.
 */
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
    bool wontAttack = mon->wont_attack() && !mon->has_ench(ENCH_FRENZIED);

    // Whether the player position is in LOS of the monster.
    bool proxPlayer = !crawl_state.game_is_arena() && mon->see_cell(you.pos())
                      && in_bounds(you.pos());

    // If set, pretend the player isn't there, but only for hostile monsters.
    if (proxPlayer && crawl_state.disables[DIS_MON_SIGHT] && !mon->wont_attack())
        proxPlayer = false;

    bool proxFoe;
    bool isSmart    = (mons_intel(*mon) >= I_HUMAN);
    bool isScared   = mon->has_ench(ENCH_FEAR);
    bool isPacified = mon->pacified();
    bool patrolling = mon->is_patrolling();

    //mprf("AI debug: mon %d behv=%d foe=%d pos=%d %d target=%d %d",
    //     mon->mindex(), mon->behaviour, mon->foe, mon->pos().x,
    //     mon->pos().y, mon->target.x, mon->target.y);

    // Check for permanent confusion, early out.
    if (mons_class_flag(mon->type, M_CONFUSED))
    {
        set_random_target(mon);
        return;
    }

    if (mons_is_fleeing_sanctuary(*mon)
        && mons_is_fleeing(*mon)
        && is_sanctuary(you.pos()))
    {
        return;
    }

    // Make sure monsters are not targeting the player in arena mode.
    ASSERT(!crawl_state.game_is_arena() || mon->foe != MHITYOU);

    // Validate current target exists.
    _mon_check_foe_invalid(mon);

    actor *owner = (mon->summoner ? actor_by_mid(mon->summoner) : nullptr);
    if (mon->type == MONS_SPECTRAL_WEAPON)
    {
        if (mon->summoner && (!owner || !owner->alive()))
            end_spectral_weapon(mon, false);

        // Spectral weapons never do anything on their own. They just attack
        // on command. A sad existence, really.
        return;
    }

    // Change proxPlayer depending on invisibility and standing
    // in shallow water.
    if (proxPlayer && !you.visible_to(mon))
        proxPlayer = _monster_guesses_invis_player(*mon);

    // Set friendly target, if they don't already have one.
    // Berserking allies ignore your commands!
    if (isFriendly
        && (mon->foe == MHITNOT || mon->foe == MHITYOU)
        && !mon->berserk_or_frenzied()
        && mon->behaviour != BEH_WITHDRAW
        && !mons_self_destructs(*mon)
        && !mons_is_avatar(mon->type))
    {
        if (you.pet_target != MHITNOT)
            mon->foe = you.pet_target;
        else
            set_nearest_monster_foe(mon, true);
    }

    // Instead, berserkers attack nearest monsters.
    if (mon->behaviour != BEH_SLEEP
        && (mon->has_ench(ENCH_FRENZIED)
            || ((mon->berserk() || mons_self_destructs(*mon))
                && (mon->foe == MHITNOT
                    || isFriendly && mon->foe == MHITYOU))))
    {
        // Intelligent monsters prefer to attack the player,
        // even when berserking.
        if (!isFriendly
            && !mon->has_ench(ENCH_FRENZIED)
            && proxPlayer
            && mons_intel(*mon) >= I_HUMAN)
        {
            mon->foe = MHITYOU;
        }
        else
            set_nearest_monster_foe(mon);
    }

    // Pacified monsters leaving the level prefer not to attack.
    // Others choose the nearest foe.
    // XXX: This is currently expensive, so we don't want to do it
    //      every turn for every monster.
    if (!isPacified && mon->foe == MHITNOT
        && mon->behaviour != BEH_SLEEP
        && (proxPlayer || one_chance_in(3)))
    {
        set_nearest_monster_foe(mon);
    }

    // Friendly allies will come back to the player if they go out of sight
    // (and haven't been commanded to run away or guard an area).
    if (isFriendly && monster_needs_los(mon)
        && mon->behaviour != BEH_WITHDRAW
        && !mon->is_patrolling()
        && !you.can_see(*mon))
    {
        mon->target = you.pos();
    }

    // Monsters do not attack themselves. {dlb}
    if (mon->foe == mon->mindex())
        mon->foe = MHITNOT;

    // Friendly and good neutral monsters do not attack other friendly
    // and good neutral monsters.
    if (!mons_is_avatar(mon->type) && mon->foe != MHITNOT && mon->foe != MHITYOU
        && wontAttack && env.mons[mon->foe].wont_attack())
    {
        mon->foe = MHITNOT;
    }

    // Neutral monsters prefer not to attack players, or other neutrals.
    if (isNeutral
        && !mon->has_ench(ENCH_FRENZIED)
        && mon->foe != MHITNOT
        && (mon->foe == MHITYOU || env.mons[mon->foe].neutral()))
    {
        mon->foe = MHITNOT;
    }

    // Unfriendly monsters fighting other monsters will usually
    // target the player.
    if (!isFriendly && !isNeutral
        && !mons_is_avatar(mon->type)
        && mon->foe != MHITYOU && mon->foe != MHITNOT
        && proxPlayer && !mon->berserk_or_frenzied()
        && !one_chance_in(3))
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

    // Misdirected monsters will only focus on your shadow, if it's still around.
    // If it has stopped being around, snap back to the player instead.
    if (mon->has_ench(ENCH_MISDIRECTED))
    {
        actor* agent = mon->get_ench(ENCH_MISDIRECTED).agent();
        // agent() will return ANON_FRIENDLY_MONSTER if the source no longer
        // exists, but was originally friendly
        if (agent && agent->alive() && agent->mindex() != ANON_FRIENDLY_MONSTER)
        {
            mon->foe = agent->mindex();
            mon->target = agent->pos();
        }
        else
            mon->del_ench(ENCH_MISDIRECTED);
    }

    while (changed)
    {
        const actor* afoe = mon->get_foe();
        proxFoe = afoe && mon->can_see(*afoe);

        if (mon->foe == MHITYOU)
        {
            // monster::get_foe returns nullptr for friendly monsters with
            // foe == MHITYOU, so make afoe point to the player here.
            // -cao
            afoe = &you;
            proxFoe = proxPlayer;   // Take invis into account.
        }

        coord_def foepos = coord_def(0,0);
        if (afoe)
            foepos = afoe->pos();

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
            mon->firing_pos = mon->pos();
            new_foe = MHITNOT;
            break;

        case BEH_SEEK:
            // No foe?  Then wander or seek the player.
            if (mon->foe == MHITNOT)
            {
                if (crawl_state.game_is_arena()
                    || !proxPlayer && !isFriendly
                    || isNeutral && !mon->has_ench(ENCH_FRENZIED)
                    || patrolling
                    || mons_self_destructs(*mon))
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

            // just because a move takes us closer to the target doesn't mean
            // that the move will stay in los of the target, and if it leaves
            // los of the target, it's possible for just naively moving toward
            // the target will not let us reach it (due to walls or whatever)
            if (!mon->see_cell(mon->target))
                try_pathfind(mon);

            // Foe gone out of LOS?
            if (!proxFoe
                && !(mon->friendly()
                     && mon->foe == MHITYOU
                     && mon->is_travelling()
                     && mon->travel_target == MTRAV_FOE))
            {
                // If their foe is marked, the monster always knows exactly
                // where they are.
                if (afoe && (mons_foe_is_marked(*mon)
                                            || mon->has_ench(ENCH_HAUNTING)))
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
                        set_nearest_monster_foe(mon);
                        if (mon->foe == MHITNOT)
                        {
                            new_beh = BEH_WANDER;
                            break;
                        }
                        mon->target = mon->get_foe()->pos();
                    }
                }

                if (mon->travel_target == MTRAV_MERFOLK_AVATAR)
                    mon->travel_target = MTRAV_NONE;

                // Spectral weapons simply seek back to their owner if
                // they can't see their seek target.
                if (mons_is_avatar(mon->type))
                {
                    // XXX: should owner ever not be set here?
                    new_foe = owner ? owner->mindex() : MHITNOT;
                    mon->target = owner ? owner->pos() : mon->pos();
                    break;
                }
                else if (isFriendly && mon->foe != MHITYOU)
                {
                    if (patrolling || crawl_state.game_is_arena())
                    {
                        new_foe = MHITNOT;
                        new_beh = BEH_WANDER;
                    }
                    // If the player can see the target location, do not reset
                    // our target, even if this monster cannot (we'll assume
                    // the player passes along this information to allies)
                    // EXCEPTION: invisible enemies for allies without sinv
                    // (otherwise your allies get stuck doing nothing)
                    else if (!foepos.origin() && you.see_cell(foepos)
                             && afoe->visible_to(mon))
                    {
                        try_pathfind(mon);
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
                    // do a stealth check. If the foe
                    // fails, monster will then start
                    // tracking foe's CURRENT position,
                    // but only for a few moves (smell and
                    // intuition only go so far).
                    if (mon->pos() == mon->target)
                    {
                        if (mon->foe == MHITYOU)
                        {
                            if (in_bounds(you.pos())
                                && (x_chance_in_y(50, you.stealth())
                                    || you.penance[GOD_ASHENZARI]
                                       && coinflip()))
                            {
                                mon->target = you.pos();
                            }
                            else
                                mon->foe_memory = 0;
                        }
                        else
                        {
                            if (coinflip())     // XXX: cheesy!
                                mon->target = env.mons[mon->foe].pos();
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
                // If the player walk out of the LOS of a monster with a ranged
                // attack, we assume it sees in which direction the player went
                // and it tries to find a line of fire instead of following the
                // player.
                else if (grid_distance(mon->target, you.pos()) == 1
                         && _mon_tries_regain_los(mon))
                {
                    _set_firing_pos(mon, you.pos());
                }
                else //
                    mon->firing_pos.reset();

                if (!isFriendly)
                    break;
                else if (mons_is_avatar(mon->type)
                         && owner
                         && !owner->is_player())
                {
                    mon->foe = owner->mindex();
                    break;
                }
            }

            ASSERT(proxFoe || isFriendly);
            ASSERT(mon->foe != MHITNOT);

            // Monster can see foe: set memory in case it loses sight.
            // Hack: smarter monsters will tend to pursue the player longer.
            switch (mons_intel(*mon))
            {
            case I_HUMAN:
                mon->foe_memory = random_range(450, 1000);
                break;
            case I_ANIMAL:
                mon->foe_memory = random_range(250, 550);
                break;
            case I_BRAINLESS:
                mon->foe_memory = random_range(100, 300);
                break;
            }

            _decide_monster_firing_position(mon, owner);

            break;

        case BEH_WANDER:
        case BEH_BATTY:
            if (isPacified)
            {
                // If a pacified monster isn't travelling toward some place from
                // which it can leave the level (either because it was just
                // pacified, or something later interfered with it exiting),
                // make it start doing so.
                //
                // If it cannot find a path to any exit at all, wander randomly
                // instead.
                if ((mon->travel_target != MTRAV_PATROL
                     || mon->target != mon->patrol_point)
                    && !mon->props.exists(PACIFY_LEAVE_FAIL_KEY))
                {
                    new_foe = MHITNOT;
                    if (mons_path_to_nearest_level_exit(mon))
                        patrolling = true;
                    // Failed to find any reachable stair. Wander randomly and
                    // don't try again, since this is unlikely to be temporary.
                    // (This should almost never happen in practice; mostly for
                    // pacified monsters stuck behind liquids. While technically
                    // they could get blinked out again and then ought to find
                    // a stair, if they look a little dumb in this random
                    // situation, it's probably okay. (They will disappear once
                    // the player gets far enough away anyway).
                    else
                    {
                        mon->travel_target = MTRAV_NONE;
                        patrolling = false;
                        mon->patrol_point.reset();
                        set_random_target(mon);
                        mon->props[PACIFY_LEAVE_FAIL_KEY] = true;
                    }
                }

                if (pacified_leave_level(mon))
                    return;
            }

            if (mon->good_neutral() && mons_is_slime(*mon)
                && have_passive(passive_t::neutral_slimes))
            {
                set_random_slime_target(mon);
            }

            // Is our foe in LOS?
            // Batty monsters don't automatically reseek so that
            // they'll flitter away, we'll reset them just before
            // they get movement in handle_monsters() instead. -- bwr
            if ((proxFoe || mons_foe_is_marked(*mon)) && !mons_is_batty(*mon))
            {
                new_beh = BEH_SEEK;
                break;
            }

            // Creatures not currently pursuing another foe are
            // alerted by a sentinel's mark
            if (mon->foe == MHITNOT
                && (you.duration[DUR_SENTINEL_MARK]
                    || mon->has_ench(ENCH_TOUCH_OF_BEOGH))
                && in_bounds(you.pos())
                && (!isFriendly && !mons_is_avatar(mon->type) && !isNeutral
                    && !isPacified
                    || mon->has_ench(ENCH_FRENZIED)))
            {
                new_foe = MHITYOU;
                new_beh = BEH_SEEK;
                break;
            }

            // Pacified monsters who have 'given up' on leaving should wander
            check_wander_target(mon, isPacified
                                      && !mon->props.exists(PACIFY_LEAVE_FAIL_KEY));

            // During their wanderings, monsters will eventually relax
            // their guard (stupid ones will do so faster, smart
            // monsters have longer memories). Pacified monsters will
            // also eventually switch the place from which they want to
            // leave the level, in case their current choice is blocked.
            if (!proxFoe && !mons_is_avatar(mon->type) && mon->foe != MHITNOT
                   && one_chance_in(isSmart ? 60 : 20)
                   && !mons_foe_is_marked(*mon))
            {
                new_foe = MHITNOT;
                if (mon->is_travelling() && mon->travel_target != MTRAV_PATROL)
                {
#ifdef DEBUG_PATHFIND
                    mpr("It's been too long! Stop travelling.");
#endif
                    mon->travel_path.clear();
                    mon->travel_target = MTRAV_NONE;
                }
            }
            break;

        case BEH_RETREAT:
            // If the target can be reached, there is a chance the monster will
            // try to attack. The chance is low to prevent the player from
            // dancing in and out of the water.
            try_pathfind(mon);
            if (one_chance_in(10) && !target_is_unreachable(mon)
                || mons_can_attack(*mon))
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
            if (!isScared)
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
            else if (proxFoe)
            {
                // Special-cased below so that it will flee *from* the
                // correct position.
                mon->target = foepos;
            }
            break;

        case BEH_CORNERED:

            // If we were able to move since becoming cornered, resume fleeing
            if (mon->pos() != mon->props[LAST_POS_KEY].get_coord())
            {
                new_beh = BEH_FLEE;
                mon->props.erase(LAST_POS_KEY);
            }

            // Foe gone out of LOS?
            if (!proxFoe)
            {
                if ((isFriendly || proxPlayer)
                    && (!isNeutral || mon->has_ench(ENCH_FRENZIED))
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
            if (grid_distance(mon->target, mon->pos()) <= 1)
            {
                // Continue on to the rally point
                if (mon->target != mon->patrol_point)
                    mon->target = mon->patrol_point;
                // Reached rally point, stop withdrawing
                else
                    stop_retreat = true;
            }
            else
            {
                // If we appear to be standing in place (possibly because we're
                // blocked by a wall or something else), eventually cancel the
                // retreat order and return to the player.
                if (mon->props.exists(LAST_POS_KEY)
                     && mon->pos() == mon->props[LAST_POS_KEY].get_coord())
                {
                    if (!mon->props.exists(BLOCKED_DEADLINE_KEY))
                        mon->props[BLOCKED_DEADLINE_KEY] = you.elapsed_time + 30;

                    if (you.elapsed_time >= mon->props[BLOCKED_DEADLINE_KEY].get_int())
                        stop_retreat = true;
                }
                else
                    mon->props.erase(BLOCKED_DEADLINE_KEY);
            }

            if (stop_retreat)
            {
                mons_end_withdraw_order(*mon);
                // XXX: The above function already sets this, but otherwise they
                //      will be ignored just below. Ugh.
                new_beh = BEH_SEEK;
                new_foe = MHITYOU;
            }
            else
                mon->props[LAST_POS_KEY] = mon->pos();

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
}

static bool _mons_check_foe(monster* mon, const coord_def& p,
                            bool friendly, bool neutral)
{
    // We don't check for the player here because otherwise wandering
    // monsters will always attack you.

    // -- But why should they always attack monsters? -- 1KB

    monster* foe = monster_at(p);
    return foe && foe != mon
           && foe->visible_to(mon)
           && (foe->friendly() != friendly
               || neutral && !foe->neutral()
               || mon->has_ench(ENCH_FRENZIED))
           && !mons_is_projectile(*foe)
           && monster_los_is_valid(mon, p)
           && (friendly || !is_sanctuary(p))
           && !mons_is_firewood(*foe)
           && !foe->props.exists(KIKU_WRETCH_KEY)
           || p == you.pos() && mon->has_ench(ENCH_FRENZIED);
}

// Set a monster's foe to the nearest valid hostile monster (ties chosen randomly)
void set_nearest_monster_foe(monster* mon, bool also_use_player_vision)
{
    // These don't look for foes.
    if (mon->good_neutral()
        || mon->behaviour == BEH_WITHDRAW
        || mons_is_avatar(mon->type)
        || mon->has_ench(ENCH_HAUNTING))
    {
        return;
    }

    const bool friendly = mon->friendly();
    const bool neutral  = mon->neutral();

    coord_def center = mon->pos();
    bool second_pass = false;

    while (true)
    {
        for (auto di = distance_iterator(center, true, true, you.current_vision);
             di; ++di)
        {
            if (!cell_see_cell(center, *di, LOS_NO_TRANS))
                continue;

            if (_mons_check_foe(mon, *di, friendly, neutral))
            {
                if (*di == you.pos())
                    mon->foe = MHITYOU;
                else
                    mon->foe = env.mgrid(*di);
                return;
            }
        }

        // If we're selecting a friendly creature's autofoe and we were unable
        // to find a foe in los of the monster, try a second pass using the
        // player's los instead.
        if (also_use_player_vision && !second_pass)
        {
            center = you.pos();
            second_pass = true;
        }
        else
            break;
    }
}

/**
 * Make a monster react to an event, possibly re-evaluating its attitude,
 * foe, AI state, or target.
 *
 * @param mon the monster getting updated
 * @param event what it's reacting to
 * @param src who did it
 * @param src_pos and where
 * @param allow_shout whether the monster can shout in reaction.
 */
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

    bool isSmart          = (mons_intel(*mon) >= I_HUMAN);
    bool setTarget        = false;
    bool breakCharm       = false;
    bool was_unaware      = mon->asleep() || mon->foe == MHITNOT;
    string msg;
    int src_idx           = src ? src->mindex() : MHITNOT; // AXE ME

    // Monsters know to blame you for reflecting things at them.
    if (src_idx == YOU_FAULTLESS)
        src_idx = MHITYOU;

    if (is_sanctuary(mon->pos()) && mons_is_fleeing_sanctuary(*mon))
    {
        mon->behaviour = BEH_FLEE;
        mon->foe       = MHITYOU;
        mon->target    = env.sanctuary_pos;
        return;
    }

    switch (event)
    {
    case ME_DISTURB:
#ifdef DEBUG_NOISE_PROPAGATION
        dprf("Disturbing %s", mon->name(DESC_A, true).c_str());
#endif
        // Assumes disturbed by noise...
        if (mon->asleep() && !mons_just_slept(*mon))
            mon->behaviour = BEH_WANDER;

        // A bit of code to make Projected Noise actually do
        // something again. Basically, dumb monsters and
        // monsters who aren't otherwise occupied will at
        // least consider the (apparent) source of the noise
        // interesting for a moment. -- bwr
        if (!isSmart || mon->foe == MHITNOT || mons_is_wandering(*mon))
        {
            if (mon->is_patrolling())
                break;

            ASSERT(!src_pos.origin());
            mon->target = src_pos;
        }
        break;

    case ME_WHACK:
    case ME_ANNOY:
        if (mon->has_ench(ENCH_GOLD_LUST))
            mon->del_ench(ENCH_GOLD_LUST);

        // Will turn monster against <src>.

        // Allies who are retreating or who have been ordered not to attack
        // should not respond to this event.
        if (src != &you
            && (mon->behaviour == BEH_WITHDRAW
                || mon->friendly() && you.pet_target == MHITYOU))
        {
            break;
        }

        // Monster types that you can't gain experience from cannot
        // fight back, so don't bother having them do so. If you
        // worship Fedhas, create a ring of friendly plants, and try
        // to break out of the ring by killing a plant, you'll get
        // a warning prompt and penance only once. Without the
        // hostility check, the plant will remain friendly until it
        // dies, and you'll get a warning prompt and penance once
        // *per hit*. This may not be the best way to address the
        // issue, though. -cao
        if (!mons_is_threatening(*mon)
            && mon->attitude != ATT_FRIENDLY
            && mon->attitude != ATT_GOOD_NEUTRAL)
        {
            return;
        }

        mon->foe = src_idx;

        // If the monster can't reach its target and can't attack it
        // either, retreat.
        try_pathfind(mon);
        if (mons_intel(*mon) > I_BRAINLESS && !mons_can_attack(*mon)
            && target_is_unreachable(mon) && !mons_just_slept(*mon))
        {
            mon->behaviour = BEH_RETREAT;
        }
        else if (mon->has_ench(ENCH_FEAR))
        {
            // self-attacks probably shouldn't break fear.
            if (src == mon)
                break;

            if (you.can_see(*mon))
            {
                mprf("%s attack snaps %s out of %s fear.",
                        src ? src->name(DESC_ITS).c_str() : "the",
                        mon->name(DESC_THE).c_str(),
                        mon->pronoun(PRONOUN_POSSESSIVE).c_str());
            }
            mon->del_ench(ENCH_FEAR, true);
        }
        else if (!mons_is_fleeing(*mon) && !mons_just_slept(*mon))
            mon->behaviour = BEH_SEEK;

        if (src == &you && mon->angered_by_attacks())
        {
            if (mon->attitude == ATT_FRIENDLY && mon->is_summoned())
            {
                summon_dismissal_fineff::schedule(mon);
                return;
            }
            // Don't attempt to 'anger' monsters that are already hostile; this can
            // have weird and unexpected effects, such as prematurely ending hostile
            // effects.
            else if (mon->temp_attitude()  != ATT_HOSTILE)
            {
                mon->attitude = ATT_HOSTILE;
                breakCharm    = true;
            }
        }

        // XXX: Somewhat hacky, this being here.
        if (mons_is_elven_twin(mon))
            elven_twins_unpacify(mon);

        // Now set target so that monster can whack back (once) at an
        // invisible foe.
        if (event == ME_WHACK)
            setTarget = true;

        // If this is a player ally and the source is out of sight, possibly
        // interrupt the player.
        if (!crawl_state.game_is_arena()
            && mon->friendly() && you.can_see(*mon) && (!src || !you.can_see(*src)))
        {
            interrupt_activity(activity_interrupt::ally_attacked);
        }

        break;

    case ME_ALERT:
#ifdef DEBUG_NOISE_PROPAGATION
        dprf("Alerting %s", mon->name(DESC_A, true).c_str());
#endif
        // Allow monsters falling asleep while patrolling (can happen if
        // they're left alone for a long time) to be woken by this event.
        if (mon->friendly() && mon->is_patrolling()
            && !mon->asleep())
        {
            break;
        }

        // Orders to withdraw take precedence over interruptions, and monsters
        // forced to sleep should not react at all.
        if (mon->behaviour == BEH_WITHDRAW || mons_just_slept(*mon))
            break;

        // Avoid moving friendly explodey things out of BEH_WANDER.
        if (mon->friendly() && mons_self_destructs(*mon))
            break;

        // [ds] Neutral monsters don't react to your presence.
        // XXX: Neutral monsters are a tangled mess of arbitrary logic.
        // It's not even clear any more what behaviours are intended for
        // neutral monsters and what are merely accidents of the code.
        if (mon->neutral() && !mon->has_ench(ENCH_FRENZIED))
        {
            if (mon->asleep())
                mon->behaviour = BEH_WANDER;
            break;
        }

        // Will alert monster to <src> and turn them
        // against them, unless they have a current foe.
        // It won't turn friends hostile either.
        if (!mons_is_retreating(*mon))
            mon->behaviour = BEH_SEEK;

        if (mon->foe == MHITNOT)
            mon->foe = src_idx;

        if (!src_pos.origin()
            && (mon->foe == MHITNOT || src && mon->foe == src->mindex()
                || mons_is_wandering(*mon)))
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
        if (mon->is_stationary() || mon->berserk_or_frenzied())
        {
            mon->del_ench(ENCH_FEAR, true, true);
            break;
        }

        // Neither do plants or nonliving beings.
        if (mon->holiness() & (MH_PLANT | MH_NONLIVING))
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
                setTarget = true;
        else if (mon->friendly() && !crawl_state.game_is_arena())
            mon->foe = MHITYOU;

        break;

    case ME_CORNERED:
        // We only care about this event if we were actually running away
        if (!mons_is_retreating(*mon))
            break;

        // Pacified monsters shouldn't change their behaviour.
        if (mon->pacified())
            break;

        // If we were already cornered last turn, give up on trying to flee
        // and turn to fight instead. Otherwise, pause a turn in hope that
        // an escape route will open up.
        if (mons_is_cornered(*mon))
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
                    msg = "PLAIN:Cornered, @the_monster@ turns to fight!";
            }
            mon->del_ench(ENCH_FEAR, true);
            mon->behaviour = BEH_SEEK;
        }
        else if (mons_is_fleeing(*mon))
        {
            // Save their current position so we know if they manage to move
            // on the following turn (and thus resume BEH_FLEE)
            mon->props[LAST_POS_KEY].get_coord() = mon->pos();
            mon->behaviour = BEH_CORNERED;
        }
        else
            mon->behaviour = BEH_SEEK;
        break;

    case ME_EVAL:
        break;
    }

    if (setTarget && src)
    {
        mon->target = src_pos;
        if (src->is_player() && mon->angered_by_attacks()
            && mon->temp_attitude() != ATT_HOSTILE)
        {
            // Why only attacks by the player change attitude? -- 1KB
            mon->attitude = ATT_HOSTILE;
            // Non-hostile uniques might be removed from dungeon annotation
            // so we add them back.
            if (mon->props.exists(NO_ANNOTATE_KEY))
                mon->props[NO_ANNOTATE_KEY] = false;
            set_unique_annotation(mon);
            mons_att_changed(mon);
        }
    }

    // Now, break charms if appropriate.
    if (breakCharm)
    {
        mon->del_ench(ENCH_CHARM);
        gozag_break_bribe(mon);
        mons_att_changed(mon);
    }

    // Do any resultant foe or state changes.
    handle_behaviour(mon);

    // That might have made the monster leave the level.
    if (!mon->alive())
        return;

    ASSERT_IN_BOUNDS_OR_ORIGIN(mon->target);

    // If it was unaware of you and you're its new foe, it might shout.
#ifdef DEBUG_NOISE_PROPAGATION
    dprf("%s could shout in behavioural event, allow_shout: %d, foe: %d", mon->name(DESC_A, true).c_str(), allow_shout, mon->foe);
#endif
    if (was_unaware && allow_shout
        && mon->foe == MHITYOU && !mon->wont_attack())
    {
        monster_consider_shouting(*mon);
    }

    const bool isPacified = mon->pacified();

    if (isPacified
        && (event == ME_DISTURB || event == ME_ALERT || event == ME_EVAL))
    {
        // Pacified monsters leaving the level won't stop doing so just because
        // they noticed something.
        mon->behaviour = old_behaviour;
    }

    // mons_speaks_msg already handles the LOS check.
    if (!msg.empty() && mon->visible_to(&you))
        mons_speaks_msg(mon, msg, MSGCH_TALK, silenced(mon->pos()));

    if (one_chance_in(10) && mons_offers_beogh_conversion_now(*mon))
        mons_speaks_msg(mon, getSpeakString("orc_priest_preaching"), MSGCH_TALK);

    ASSERT(!crawl_state.game_is_arena()
           || mon->foe != MHITYOU && mon->target != you.pos());
}

void make_mons_stop_fleeing(monster* mon)
{
    if (mons_is_retreating(*mon))
        behaviour_event(mon, ME_CORNERED);
}

beh_type attitude_creation_behavior(mon_attitude_type att)
{
    switch (att)
    {
    case ATT_NEUTRAL:
        return BEH_NEUTRAL;
    case ATT_GOOD_NEUTRAL:
        return BEH_GOOD_NEUTRAL;
    case ATT_FRIENDLY:
        return BEH_FRIENDLY;
    default:
        return BEH_HOSTILE;
    }
}

// If you're invis and throw/zap whatever, alerts env.mons to your position.
void alert_nearby_monsters()
{
    // Judging from the above comment, this function isn't
    // intended to wake up monsters, so we're only going to
    // alert monsters that aren't sleeping. For cases where an
    // event should wake up monsters and alert them, I'd suggest
    // calling noisy() before calling this function. - bwr
    for (monster_near_iterator mi(you.pos()); mi; ++mi)
        if (!mi->asleep())
             behaviour_event(*mi, ME_ALERT, &you);
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

// If _mons_find_level_exits() is ever expanded to handle more grid
// types, this should be expanded along with it.
static void _mons_indicate_level_exit(const monster* mon)
{
    const dungeon_feature_type feat = env.grid(mon->pos());
    const bool is_shaft = (get_trap_type(mon->pos()) == TRAP_SHAFT);

    if (feat_is_gate(feat))
        simple_monster_message(*mon, " passes through the gate.");
    else if (feat_is_travelable_stair(feat))
    {
        command_type dir = feat_stair_direction(feat);
        simple_monster_message(*mon,
            make_stringf(" %s the %s.",
                dir == CMD_GO_UPSTAIRS     ? "goes up" :
                dir == CMD_GO_DOWNSTAIRS   ? "goes down"
                                           : "takes",
                feat_is_escape_hatch(feat) ? "escape hatch"
                                           : "stairs").c_str());
    }
    else if (is_shaft)
    {
        simple_monster_message(*mon,
            make_stringf(" %s the shaft.",
                mon->airborne() ? "goes down"
                                : "jumps into").c_str());
    }
}

void make_mons_leave_level(monster* mon)
{
    if (mon->pacified())
    {
        // Have pacified orcs drop their gear to avoid potentially tedious
        // methods of trying to get something to kill them before they leave the
        // level, or the deeply unflavorful approach of simply killing them for
        // it and putting up with Beogh being upset with you.
        if (mons_genus(mon->type) == MONS_ORC && you_worship(GOD_BEOGH))
        {
            if (you.can_see(*mon))
            {
                simple_monster_message(*mon,
                    make_stringf(" donates %s equipment to the cause.",
                        mon->pronoun(PRONOUN_POSSESSIVE).c_str()).c_str());
            }
            monster_drop_things(mon);
        }

        if (you.can_see(*mon))
            _mons_indicate_level_exit(mon);

        // Pacified monsters leaving the level take their stuff with
        // them.
        mon->flags |= MF_HARD_RESET;
        monster_die(*mon, KILL_DISMISSED, NON_MONSTER);
    }
}

// Summons and perma-friendly allies can't attack out of the player's LOS
bool monster_needs_los(const monster* mons)
{
    return !crawl_state.game_is_arena()
           && mons->attitude == ATT_FRIENDLY;
}

// Check whether the player has line of sight to both the attacker and defender,
// if this is a monster that requires that to attack.
//
// Returns true if the monster can attack the specified target.
bool monster_los_is_valid(const monster* mons, const coord_def &p)
{
    if (crawl_state.game_is_arena())
        return true;

    if (!monster_needs_los(mons))
        return true;

    return you.see_cell_no_trans(mons->pos()) && you.see_cell_no_trans(p);
}

bool monster_los_is_valid(const monster* mons, const actor* targ)
{
    return monster_los_is_valid(mons, targ->pos());
}

vector<monster *> find_allies_targeting(const actor &a)
{
    vector<monster *> result;
    for (monster* m : monster_near_iterator(you.pos(), LOS_DEFAULT))
        if (m->friendly() && m->foe == a.mindex())
            result.push_back(m);
    return result;
}

bool is_ally_target(const actor &a)
{
    for (monster* m : monster_near_iterator(you.pos(), LOS_DEFAULT))
        if (m->friendly() && m->foe == a.mindex())
            return true;
    return false;
}

// Make a player ally stop retreating and return to you
void mons_end_withdraw_order(monster& mons)
{
    if (mons.behaviour != BEH_WITHDRAW)
        return;

    mons.behaviour = BEH_SEEK;
    mons.foe = MHITYOU;
    mons.patrol_point.reset();
    mons.props.erase(LAST_POS_KEY);
    mons.props.erase(BLOCKED_DEADLINE_KEY);
}
