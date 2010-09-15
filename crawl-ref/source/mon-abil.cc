/*
 *  File:       mon-abil.cc
 *  Summary:    Monster abilities.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"
#include "mon-abil.h"

#include "externs.h"

#include "arena.h"
#include "beam.h"
#include "colour.h"
#include "coordit.h"
#include "directn.h"
#include "fprop.h"
#include "ghost.h"
#include "libutil.h"
#include "misc.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-iter.h"
#include "mon-place.h"
#include "terrain.h"
#include "mgen_data.h"
#include "coord.h"
#include "mon-speak.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "options.h"
#include "random.h"
#include "religion.h"
#include "spl-miscast.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "env.h"
#include "areas.h"
#include "view.h"
#include "shout.h"
#include "viewchar.h"

#include <algorithm>
#include <queue>
#include <map>
#include <set>

const int MAX_KRAKEN_TENTACLE_DIST = 12;

static bool _slime_split_merge(monster* thing);

struct position_node
{
    position_node(const position_node & existing)
    {
        pos = existing.pos;
        last = existing.last;
        estimate = existing.estimate;
        path_distance = existing.path_distance;
        connect_level = existing.connect_level;
        string_distance = existing.string_distance;
        departure = existing.departure;
    }

    position_node()
    {
        pos.x=0;
        pos.y=0;
        last = NULL;
        estimate = 0;
        path_distance = 0;
        connect_level = 0;
        string_distance = 0;
        departure = false;
    }

    coord_def pos;
    const position_node * last;

    int estimate;
    int path_distance;
    int connect_level;
    int string_distance;
    bool departure;

    bool operator < (const position_node & right) const
    {
        if (pos == right.pos)
            return string_distance < right.string_distance;

        return pos < right.pos;

  //      if (pos.x == right.pos.x)
//            return pos.y < right.pos.y;

//        return pos.x < right.pos.x;
    }

    int total_dist() const
    {
        return (estimate + path_distance);
    }
};

struct path_less
{
    bool operator()(const std::set<position_node>::iterator & left,
                    const std::set<position_node>::iterator & right)
    {
        return (left->total_dist() > right->total_dist());
    }

};

#define DISCONNECT_DIST (INT_MAX - 1000)

template<typename cost_T, typename est_T>
struct simple_connect
{
    cost_T cost_function;
    est_T estimate_function;

    int connect;
    int compass_idx[8];

    simple_connect()
    {
        for (unsigned i=0; i<8; i++)
        {
            compass_idx[i] = i;
        }
    }

    void operator()(const position_node & node,
                    std::vector<position_node> & expansion)
    {
        std::random_shuffle(compass_idx, compass_idx + connect);

        for (int i=0; i < connect; i++)
        {
            position_node temp;
            temp.pos = node.pos + Compass[compass_idx[i]];
            if (!in_bounds(temp.pos))
                continue;

            int cost = cost_function(temp.pos);
//            if (cost == DISCONNECT_DIST)
  //              continue;
            temp.path_distance = node.path_distance + cost;

            temp.estimate = estimate_function(temp.pos);
            expansion.push_back(temp);
            // leaving last undone for now, don't want to screw the pointer up.
        }
    }
};

struct coord_wrapper
{
    coord_wrapper( int (*input) (const coord_def & pos))
    {
        test = input;
    }
    int (*test) (const coord_def & pos);
    int  operator()(const coord_def & pos)
    {
        return (test(pos));
    }

    coord_wrapper()
    {

    }
};

template<typename valid_T, typename expand_T>
void search_astar(position_node & start,
                  valid_T & valid_target,
                  expand_T & expand_node,
                  std::set<position_node> & visited,
                  std::vector<std::set<position_node>::iterator > & candidates)
{
    std::priority_queue<std::set<position_node>::iterator,
                        std::vector<std::set<position_node>::iterator>,
                        path_less  > fringe;

    std::set<position_node>::iterator current = visited.insert(start).first;
    fringe.push(current);


    bool done = false;
    while (!fringe.empty())
    {
        current = fringe.top();
        fringe.pop();

        std::vector<position_node> expansion;
        expand_node(*current, expansion);

        for (unsigned i=0;i < expansion.size(); ++i)
        {
            expansion[i].last = &(*current);

            std::pair<std::set<position_node>::iterator, bool > res;
            res = visited.insert(expansion[i]);

            if (!res.second)
            {
                continue;
            }

            if (valid_target(res.first->pos))
            {
                candidates.push_back(res.first);
                done = true;
                break;
            }

            if (res.first->path_distance < DISCONNECT_DIST)
            {
                fringe.push(res.first);
            }
        }
        if (done)
            break;
    }
}

template<typename valid_T, typename expand_T>
void search_astar(const coord_def & start,
                  valid_T & valid_target,
                  expand_T & expand_node,
                  std::set<position_node> & visited,
                  std::vector<std::set<position_node>::iterator > & candidates)
{
    position_node temp_node;
    temp_node.pos = start;
    temp_node.last = NULL;
    temp_node.path_distance = 0;

    search_astar(temp_node, valid_target, expand_node, visited, candidates);
}

template<typename valid_T, typename cost_T, typename est_T>
void search_astar(const coord_def & start,
                  valid_T & valid_target,
                  cost_T & connection_cost,
                  est_T & cost_estimate,
                  std::set<position_node> & visited,
                  std::vector<std::set<position_node>::iterator > & candidates,
                  int connect_mode = 8)
{
    if (connect_mode < 1 || connect_mode > 8)
        connect_mode = 8;

    simple_connect<cost_T, est_T> connect;
    connect.connect = connect_mode;
    connect.cost_function = connection_cost;
    connect.estimate_function = cost_estimate;

    search_astar(start, valid_target, connect, visited, candidates);
}

template<typename valid_T, typename connect_T>
void search_dungeon(const coord_def & start,
                    valid_T & valid_target,
                    connect_T & connecting_square,
                    std::set<position_node> & visited,
                    std::vector<std::set<position_node>::iterator> & candidates,
                    bool exhaustive = true,
                    int connect_mode = 8)
{

    if (connect_mode < 1 || connect_mode > 8)
        connect_mode = 8;

    // Ordering the default compass index this way gives us the non
    // diagonal directions as the first four elements - so by just
    // using the first 4 elements instead of the whole array we
    // can have 4-connectivity.
    int compass_idx[] = {0, 2, 4, 6, 1, 3, 5, 7};


    position_node temp_node;
    temp_node.pos = start;
    temp_node.last = NULL;


    std::queue<std::set<position_node>::iterator > fringe;

    std::set<position_node>::iterator current = visited.insert(temp_node).first;
    fringe.push(current);


    bool done = false;
    while (!fringe.empty())
    {
        current = fringe.front();
        fringe.pop();

        std::random_shuffle(compass_idx, compass_idx + connect_mode);

        for (int i=0; i < connect_mode; ++i)
        {
            coord_def adjacent = current->pos + Compass[compass_idx[i]];
            if (in_bounds(adjacent))
            {
                temp_node.pos = adjacent;
                temp_node.last = &(*current);
                std::pair<std::set<position_node>::iterator, bool > res;
                res = visited.insert(temp_node);

                if (!res.second)
                    continue;

                if (valid_target(adjacent))
                {
                    candidates.push_back(res.first);
                    if (!exhaustive)
                    {
                        done = true;
                        break;
                    }

                }

                if (connecting_square(adjacent))
                {
//                    if (res.second)
                    fringe.push(res.first);
                }
            }
        }
        if (done)
            break;
    }
}

bool ugly_thing_mutate(monster* ugly, bool proximity)
{
    bool success = false;

    std::string src = "";

    uint8_t mon_colour = BLACK;

    if (!proximity)
        success = true;
    else if (one_chance_in(8))
    {
        int you_mutate_chance = 0;
        int mon_mutate_chance = 0;

        for (adjacent_iterator ri(ugly->pos()); ri; ++ri)
        {
            if (you.pos() == *ri)
                you_mutate_chance = get_contamination_level();
            else
            {
                monster* ugly_near = monster_at(*ri);

                if (!ugly_near
                    || ugly_near->type != MONS_UGLY_THING
                        && ugly_near->type != MONS_VERY_UGLY_THING)
                {
                    continue;
                }

                for (int i = 0; i < 2; ++i)
                {
                    if (coinflip())
                    {
                        mon_mutate_chance++;

                        if (coinflip())
                        {
                            const int ugly_colour =
                                make_low_colour(ugly->colour);
                            const int ugly_near_colour =
                                make_low_colour(ugly_near->colour);

                            if (ugly_colour != ugly_near_colour)
                                mon_colour = ugly_near_colour;
                        }
                    }

                    if (ugly_near->type != MONS_VERY_UGLY_THING)
                        break;
                }
            }
        }

        you_mutate_chance = std::min(16, you_mutate_chance);
        mon_mutate_chance = std::min(16, mon_mutate_chance);

        if (!one_chance_in(you_mutate_chance + mon_mutate_chance + 1))
        {
            const bool proximity_you =
                (you_mutate_chance  > mon_mutate_chance) ? true :
                (you_mutate_chance == mon_mutate_chance) ? coinflip()
                                                         : false;

            src = proximity_you ? " from you" : " from its kin";

            success = true;
        }
    }

    if (success)
    {
        simple_monster_message(ugly,
            make_stringf(" basks in the mutagenic energy%s and changes!",
                         src.c_str()).c_str());

        ugly->uglything_mutate(mon_colour);

        return (true);
    }

    return (false);
}

// Inflict any enchantments the parent slime has on its offspring,
// leaving durations unchanged, I guess. -cao
static void _split_ench_durations(monster* initial_slime, monster* split_off)
{
    mon_enchant_list::iterator i;

    for (i = initial_slime->enchantments.begin();
         i != initial_slime->enchantments.end(); ++i)
    {
        split_off->add_ench(i->second);
    }

}

// What to do about any enchantments these two slimes may have?  For
// now, we are averaging the durations. -cao
static void _merge_ench_durations(monster* initial_slime, monster* merge_to)
{
    mon_enchant_list::iterator i;

    int initial_count = initial_slime->number;
    int merge_to_count = merge_to->number;
    int total_count = initial_count + merge_to_count;

    for (i = initial_slime->enchantments.begin();
         i != initial_slime->enchantments.end(); ++i)
    {
        // Does the other slime have this enchantment as well?
        mon_enchant temp = merge_to->get_ench(i->first);
        bool no_initial = temp.ench == ENCH_NONE;        // If not, use duration 0 for their part of the average.
        int duration = no_initial ? 0 : temp.duration;

        i->second.duration = (i->second.duration * initial_count
                              + duration * merge_to_count)/total_count;

        if (!i->second.duration)
            i->second.duration = 1;

        if (no_initial)
            merge_to->add_ench(i->second);
        else
            merge_to->update_ench(i->second);
    }

    for (i = merge_to->enchantments.begin();
         i != merge_to->enchantments.end(); ++i)
    {
        if (initial_slime->enchantments.find(i->first)
                == initial_slime->enchantments.end()
            && i->second.duration > 1)
        {
            i->second.duration = (merge_to_count * i->second.duration)
                                  / total_count;

            merge_to->update_ench(i->second);
        }
    }
}

// Calculate slime creature hp based on how many are merged.
static void _stats_from_blob_count(monster* slime, float max_per_blob,
                                   float current_per_blob)
{
    slime->max_hit_points = (int)(slime->number * max_per_blob);
    slime->hit_points = (int)(slime->number * current_per_blob);
}

// Create a new slime creature at 'target', and split 'thing''s hp and
// merge count with the new monster.
static bool _do_split(monster* thing, coord_def & target)
{
    // Create a new slime.
    int slime_idx = create_monster(mgen_data(MONS_SLIME_CREATURE,
                                             thing->behaviour,
                                             0,
                                             0,
                                             0,
                                             target,
                                             thing->foe,
                                             MG_FORCE_PLACE));

    if (slime_idx == -1)
        return (false);

    monster* new_slime = &env.mons[slime_idx];

    if (!new_slime)
        return (false);

    // Inflict the new slime with any enchantments on the parent.
    _split_ench_durations(thing, new_slime);
    new_slime->attitude = thing->attitude;
    new_slime->flags = thing->flags;
    new_slime->props = thing->props;
    // XXX copy summoner info

    if (you.can_see(thing))
        mprf("%s splits.", thing->name(DESC_CAP_A).c_str());

    int split_off = thing->number / 2;
    float max_per_blob = thing->max_hit_points / float(thing->number);
    float current_per_blob = thing->hit_points / float(thing->number);

    thing->number -= split_off;
    new_slime->number = split_off;

    new_slime->hit_dice = thing->hit_dice;

    _stats_from_blob_count(thing, max_per_blob, current_per_blob);
    _stats_from_blob_count(new_slime, max_per_blob, current_per_blob);

    if (crawl_state.game_is_arena())
        arena_split_monster(thing, new_slime);

    return (true);
}

// Actually merge two slime creature, pooling their hp, etc.
// initial_slime is the one that gets killed off by this process.
static bool _do_merge(monster* initial_slime, monster* merge_to)
{
    // Combine enchantment durations.
    _merge_ench_durations(initial_slime, merge_to);

    merge_to->number += initial_slime->number;
    merge_to->max_hit_points += initial_slime->max_hit_points;
    merge_to->hit_points += initial_slime->hit_points;

    // Merge monster flags (mostly so that MF_CREATED_NEUTRAL, etc. are
    // passed on if the merged slime subsequently splits.  Hopefully
    // this won't do anything weird.
    merge_to->flags |= initial_slime->flags;

    // Merging costs the combined slime some energy.
    monsterentry* entry = get_monster_data(merge_to->type);

    // We want to find out if merge_to will move next time it has a turn
    // (assuming for the sake of argument the next delay is 10). The
    // purpose of subtracting energy from merge_to is to make it lose a
    // turn after the merge takes place, if it's already going to lose
    // a turn we don't need to do anything.
    merge_to->speed_increment += entry->speed;
    bool can_move = merge_to->has_action_energy();
    merge_to->speed_increment -= entry->speed;

    if (can_move)
    {
        merge_to->speed_increment -= entry->energy_usage.move;

        // This is dumb.  With that said, the idea is that if 2 slimes merge
        // you can gain a space by moving away the turn after (maybe this is
        // too nice but there will probably be a lot of complaints about the
        // damage on higher level slimes).  So we subtracted some energy
        // above, but if merge_to hasn't moved yet this turn, that will just
        // cancel its turn in this round of world_reacts().  So we are going
        // to see if merge_to has gone already by checking its mindex (this
        // works because handle_monsters just iterates over env.mons in
        // ascending order).
        if (initial_slime->mindex() < merge_to->mindex())
            merge_to->speed_increment -= entry->energy_usage.move;
    }

    // Overwrite the state of the slime getting merged into, because it
    // might have been resting or something.
    merge_to->behaviour = initial_slime->behaviour;
    merge_to->foe = initial_slime->foe;

    behaviour_event(merge_to, ME_EVAL);

    // Messaging.
    if (you.can_see(merge_to))
    {
        if (you.can_see(initial_slime))
        {
            mprf("Two slime creatures merge to form %s.",
                 merge_to->name(DESC_NOCAP_A).c_str());
        }
        else
        {
            mprf("A slime creature suddenly becomes %s.",
                 merge_to->name(DESC_NOCAP_A).c_str());
        }

        flash_view_delay(LIGHTGREEN, 150);
    }
    else if (you.can_see(initial_slime))
        mpr("A slime creature suddenly disappears!");

    // Have to 'kill' the slime doing the merging.
    monster_die(initial_slime, KILL_MISC, NON_MONSTER, true);

    return (true);
}

// Slime creatures can split but not merge under these conditions.
static bool _unoccupied_slime(monster* thing)
{
     return (thing->asleep() || mons_is_wandering(thing)
             || thing->foe == MHITNOT);
}

// Slime creatures cannot split or merge under these conditions.
static bool _disabled_slime(monster* thing)
{
    return (!thing
            || mons_is_fleeing(thing)
            || mons_is_confused(thing)
            || thing->paralysed());
}

// See if there are any appropriate adjacent slime creatures for 'thing'
// to merge with.  If so, carry out the merge.
//
// A slime creature will merge if there is an adjacent slime, merging
// onto that slime would reduce the distance to the original slime's
// target, and there are no empty squares that would also reduce the
// distance to the target.
static bool _slime_merge(monster* thing)
{
    if (!thing || _disabled_slime(thing) || _unoccupied_slime(thing))
        return (false);

    int max_slime_merge = 5;
    int compass_idx[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    std::random_shuffle(compass_idx, compass_idx + 8);
    coord_def origin = thing->pos();

    int target_distance = grid_distance(thing->target, thing->pos());

    monster* merge_target = NULL;
    // Check for adjacent slime creatures.
    for (int i = 0; i < 8; ++i)
    {
        coord_def target = origin + Compass[compass_idx[i]];

        // If this square won't reduce the distance to our target, don't
        // look for a potential merge, and don't allow this square to
        // prevent a merge if empty.
        if (grid_distance(thing->target, target) >= target_distance)
            continue;

        // Don't merge if there is an open square that reduces distance
        // to target, even if we found a possible slime to merge with.
        if (!actor_at(target)
            && mons_class_can_pass(MONS_SLIME_CREATURE, env.grid(target)))
        {
            return false;
        }

        // Is there a slime creature on this square we can consider
        // merging with?
        monster* other_thing = monster_at(target);
        if (!merge_target
            && other_thing
            && other_thing->type == MONS_SLIME_CREATURE
            && other_thing->attitude == thing->attitude
            && other_thing->is_summoned() == thing->is_summoned()
            && !other_thing->is_shapeshifter()
            && !_disabled_slime(other_thing))
        {
            // We can potentially merge if doing so won't take us over
            // the merge cap.
            int new_blob_count = other_thing->number + thing->number;
            if (new_blob_count <= max_slime_merge)
                merge_target = other_thing;
        }
    }

    // We found a merge target and didn't find an open square that
    // would reduce distance to target, so we can actually merge.
    if (merge_target)
        return (_do_merge(thing, merge_target));

    // No adjacent slime creatures we could merge with.
    return (false);
}

static bool _slime_can_spawn(const coord_def target)
{
    return (mons_class_can_pass(MONS_SLIME_CREATURE, env.grid(target))
            && !actor_at(target));
}

// See if slime creature 'thing' can split, and carry out the split if
// we can find a square to place the new slime creature on.
static bool _slime_split(monster* thing)
{
    if (!thing || thing->number <= 1
        || coinflip() // Don't make splitting quite so reliable. (jpeg)
        || _disabled_slime(thing))
    {
        return (false);
    }

    const coord_def origin  = thing->pos();

    const actor* foe        = thing->get_foe();
    const bool has_foe      = (foe != NULL && thing->can_see(foe));
    const coord_def foe_pos = (has_foe ? foe->position : coord_def(0,0));
    const int old_dist      = (has_foe ? grid_distance(origin, foe_pos) : 0);

    if (has_foe && old_dist > 1)
    {
        // If we're not already adjacent to the foe, check whether we can
        // move any closer. If so, do that rather than splitting.
        for (radius_iterator ri(origin, 1, true, false, true); ri; ++ri)
        {
            if (_slime_can_spawn(*ri)
                && grid_distance(*ri, foe_pos) < old_dist)
            {
                return (false);
            }
        }
    }

    int compass_idx[] = {0, 1, 2, 3, 4, 5, 6, 7};
    std::random_shuffle(compass_idx, compass_idx + 8);

    // Anywhere we can place an offspring?
    for (int i = 0; i < 8; ++i)
    {
        coord_def target = origin + Compass[compass_idx[i]];

        // Don't split if this increases the distance to the target.
        if (has_foe && grid_distance(target, foe_pos) > old_dist)
            continue;

        if (_slime_can_spawn(target))
        {
            // This can fail if placing a new monster fails.  That
            // probably means we have too many monsters on the level,
            // so just return in that case.
            return (_do_split(thing, target));
        }
    }

   // No free squares.
   return (false);
}

// See if a given slime creature can split or merge.
static bool _slime_split_merge(monster* thing)
{
    // No merging/splitting shapeshifters.
    if (!thing
        || thing->is_shapeshifter()
        || thing->type != MONS_SLIME_CREATURE)
    {
        return (false);
    }

    if (_slime_split(thing))
        return (true);

    return (_slime_merge(thing));
}

// Returns true if you resist the siren's call.
static bool _siren_movement_effect(const monster* mons)
{
    bool do_resist = (you.attribute[ATTR_HELD] || you.check_res_magic(70)
                      || you.cannot_act() || you.asleep());

    if (!do_resist)
    {
        coord_def dir(coord_def(0,0));
        if (mons->pos().x < you.pos().x)
            dir.x = -1;
        else if (mons->pos().x > you.pos().x)
            dir.x = 1;
        if (mons->pos().y < you.pos().y)
            dir.y = -1;
        else if (mons->pos().y > you.pos().y)
            dir.y = 1;

        const coord_def newpos = you.pos() + dir;

        if (!in_bounds(newpos) || is_feat_dangerous(grd(newpos))
            || !you.can_pass_through_feat(grd(newpos)))
        {
            do_resist = true;
        }
        else
        {
            bool swapping = false;
            monster* mon = monster_at(newpos);
            if (mon)
            {
                coord_def swapdest;
                if (mon->wont_attack()
                    && !mons_is_stationary(mon)
                    && !mons_is_projectile(mon->type)
                    && !mon->cannot_act()
                    && !mon->asleep()
                    && swap_check(mon, swapdest, true))
                {
                    swapping = true;
                }
                else if (!mon->submerged())
                    do_resist = true;
            }

            if (!do_resist)
            {
                const coord_def oldpos = you.pos();
                mprf("The pull of her song draws you forwards.");

                if (swapping)
                {
                    if (monster_at(oldpos))
                    {
                        mprf("Something prevents you from swapping places "
                             "with %s.",
                             mon->name(DESC_NOCAP_THE).c_str());
                        return (do_resist);
                    }

                    int swap_mon = mgrd(newpos);
                    // Pick the monster up.
                    mgrd(newpos) = NON_MONSTER;
                    mon->moveto(oldpos);

                    // Plunk it down.
                    mgrd(mon->pos()) = swap_mon;

                    mprf("You swap places with %s.",
                         mon->name(DESC_NOCAP_THE).c_str());
                }
                move_player_to_grid(newpos, true, true);

                if (swapping)
                    mon->apply_location_effects(newpos);
            }
        }
    }

    return (do_resist);
}

static bool _silver_statue_effects(monster* mons)
{
    actor *foe = mons->get_foe();
    if (foe && mons->can_see(foe) && !one_chance_in(3))
    {
        const std::string msg =
            "'s eyes glow " + weird_glowing_colour() + '.';
        simple_monster_message(mons, msg.c_str(), MSGCH_WARN);

        create_monster(
            mgen_data(
                summon_any_demon((coinflip() ? DEMON_COMMON
                                             : DEMON_LESSER)),
                SAME_ATTITUDE(mons), mons, 5, 0, foe->pos(), mons->foe));
        return (true);
    }
    return (false);
}

static bool _orange_statue_effects(monster* mons)
{
    actor *foe = mons->get_foe();
    if (foe && mons->can_see(foe) && !one_chance_in(3))
    {
        if (you.can_see(foe))
        {
            if (foe == &you)
                mprf(MSGCH_WARN, "A hostile presence attacks your mind!");
            else if (you.can_see(mons))
                mprf(MSGCH_WARN, "%s fixes %s piercing gaze on %s.",
                     mons->name(DESC_CAP_THE).c_str(),
                     mons->pronoun(PRONOUN_NOCAP_POSSESSIVE).c_str(),
                     foe->name(DESC_NOCAP_THE).c_str());
        }

        MiscastEffect(foe, mons->mindex(), SPTYP_DIVINATION,
                      random2(15), random2(150),
                      "an orange crystal statue");
        return (true);
    }

    return (false);
}

static bool _orc_battle_cry(monster* chief)
{
    const actor *foe = chief->get_foe();
    int affected = 0;

    if (foe
        && (foe != &you || !chief->friendly())
        && !silenced(chief->pos())
        && chief->can_see(foe)
        && coinflip())
    {
        const int level = chief->hit_dice > 12? 2 : 1;
        std::vector<monster* > seen_affected;
        for (monster_iterator mi(chief); mi; ++mi)
        {
            if (*mi != chief
                && mons_species(mi->type) == MONS_ORC
                && mons_aligned(chief, *mi)
                && mi->hit_dice < chief->hit_dice
                && !mi->berserk()
                && !mi->has_ench(ENCH_MIGHT)
                && !mi->cannot_move()
                && !mi->confused())
            {
                mon_enchant ench = mi->get_ench(ENCH_BATTLE_FRENZY);
                if (ench.ench == ENCH_NONE || ench.degree < level)
                {
                    const int dur =
                        random_range(12, 20) * speed_to_duration(mi->speed);

                    if (ench.ench != ENCH_NONE)
                    {
                        ench.degree   = level;
                        ench.duration = std::max(ench.duration, dur);
                        mi->update_ench(ench);
                    }
                    else
                    {
                        mi->add_ench(mon_enchant(ENCH_BATTLE_FRENZY, level,
                                                  KC_OTHER, dur));
                    }

                    affected++;
                    if (you.can_see(*mi))
                        seen_affected.push_back(*mi);

                    if (mi->asleep())
                        behaviour_event(*mi, ME_DISTURB, MHITNOT, chief->pos());
                }
            }
        }

        if (affected)
        {
            if (you.can_see(chief) && player_can_hear(chief->pos()))
            {
                mprf(MSGCH_SOUND, "%s roars a battle-cry!",
                     chief->name(DESC_CAP_THE).c_str());
            }

            // The yell happens whether you happen to see it or not.
            noisy(LOS_RADIUS, chief->pos(), chief->mindex());

            // Disabling detailed frenzy announcement because it's so spammy.
            const msg_channel_type channel =
                        chief->friendly() ? MSGCH_MONSTER_ENCHANT
                                          : MSGCH_FRIEND_ENCHANT;

            if (!seen_affected.empty())
            {
                std::string who;
                if (seen_affected.size() == 1)
                {
                    who = seen_affected[0]->name(DESC_CAP_THE);
                    mprf(channel, "%s goes into a battle-frenzy!", who.c_str());
                }
                else
                {
                    int type = seen_affected[0]->type;
                    for (unsigned int i = 0; i < seen_affected.size(); i++)
                    {
                        if (seen_affected[i]->type != type)
                        {
                            // just mention plain orcs
                            type = MONS_ORC;
                            break;
                        }
                    }
                    who = get_monster_data(type)->name;

                    mprf(channel, "%s %s go into a battle-frenzy!",
                         chief->friendly() ? "Your" : "The",
                         pluralise(who).c_str());
                }
            }
        }
    }
    // Orc battle cry doesn't cost the monster an action.
    return (false);
}

static bool _make_monster_angry(const monster* mon, monster* targ)
{
    if (mon->friendly() != targ->friendly())
        return (false);

    // targ is guaranteed to have a foe (needs_berserk checks this).
    // Now targ needs to be closer to *its* foe than mon is (otherwise
    // mon might be in the way).

    coord_def victim;
    if (targ->foe == MHITYOU)
        victim = you.pos();
    else if (targ->foe != MHITNOT)
    {
        const monster* vmons = &menv[targ->foe];
        if (!vmons->alive())
            return (false);
        victim = vmons->pos();
    }
    else
    {
        // Should be impossible. needs_berserk should find this case.
        ASSERT(false);
        return (false);
    }

    // If mon may be blocking targ from its victim, don't try.
    if (victim.distance_from(targ->pos()) > victim.distance_from(mon->pos()))
        return (false);

    if (you.can_see(mon))
    {
        mprf("%s goads %s on!", mon->name(DESC_CAP_THE).c_str(),
             targ->name(DESC_NOCAP_THE).c_str());
    }

    targ->go_berserk(false);

    return (true);
}

static bool _moth_incite_monsters(const monster* mon)
{
    if (is_sanctuary(you.pos()) || is_sanctuary(mon->pos()))
        return false;

    int goaded = 0;
    circle_def c(mon->pos(), 3, C_SQUARE);
    for (monster_iterator mi(&c); mi; ++mi)
    {
        if (*mi == mon || !mi->needs_berserk())
            continue;

        if (is_sanctuary(mi->pos()))
            continue;

        // Cannot goad other moths of wrath!
        if (mi->type == MONS_MOTH_OF_WRATH)
            continue;

        if (_make_monster_angry(mon, *mi) && !one_chance_in(3 * ++goaded))
            return (true);
    }

    return (false);
}

static inline void _mons_cast_abil(monster* mons, bolt &pbolt,
                                   spell_type spell_cast)
{
    mons_cast(mons, pbolt, spell_cast, true, true);
}

static void _establish_connection(int tentacle,
                                  int head,
                                  std::set<position_node>::iterator path)
{
    const position_node * last = &(*path);
    const position_node * current = last->last;

    // Tentacle is adjacent to the head, not much to do.
    if (!current)
    {
        menv[tentacle].props["inwards"].get_int() = head;
        return;
    }

    monster* main = &env.mons[head];
    while (current)
    {

        // Last monster we visited or placed
        monster* last_mon = monster_at(last->pos);
        if (!last_mon)
        {
            // Should be something there, what to do if there isn't?
            mprf("Error! failed to place monster in tentacle connect change");
            break;
        }
        int last_mon_idx = last_mon->mindex();

        // Monster at the current square, should be the end of the line if there
        monster* current_mons = monster_at(current->pos);
        if (current_mons)
        {
            // Todo verify current monster type
            menv[current_mons->mindex()].props["inwards"].get_int() = last_mon_idx;
            menv[last_mon_idx].props["outwards"].get_int() = current_mons->mindex();
            break;
        }

         // place a connector
        int connect = create_monster(
            mgen_data(MONS_KRAKEN_CONNECTOR, SAME_ATTITUDE(main), main,
                      0, 0, current->pos, main->foe,
                      MG_FORCE_PLACE, main->god, MONS_NO_MONSTER, tentacle,
                      main->colour, you.absdepth0, PROX_CLOSE_TO_PLAYER,
                      you.level_type));

        if (connect >= 0)
        {
            menv[connect].max_hit_points = menv[tentacle].max_hit_points;
            menv[connect].hit_points = menv[tentacle].hit_points;

            menv[connect].props["inwards"].get_int() = last_mon_idx;
            menv[connect].props["outwards"].get_int() = -1;

            if (last_mon->type == MONS_KRAKEN_CONNECTOR)
                menv[last_mon_idx].props["outwards"].get_int() = connect;

            if (main->holiness() == MH_UNDEAD)
            {
                menv[connect].flags |= MF_HONORARY_UNDEAD;
            }
            if (monster_can_submerge(&menv[connect], env.grid(menv[connect].pos())))
            {
                menv[connect].add_ench(ENCH_SUBMERGED);
            }

        }
        else
        {
            // connector placement failed, what to do?
        }

        last = current;
        current = current->last;
    }
}

struct tentacle_attack_constraints
{
    std::vector<coord_def> * target_positions;

    std::map<coord_def, std::set<int> > * connection_constraints;
    monster* kraken;
    int connect_idx[8];

    tentacle_attack_constraints()
    {
        for (int i=0; i<8; i++)
        {
            connect_idx[i] = i;
        }
    }

    int min_dist(const coord_def & pos)
    {
        int min = INT_MAX;
        for (unsigned i = 0; i < target_positions->size(); ++i)
        {
            int dist = grid_distance(pos, target_positions->at(i));

            if (dist < min)
                min = dist;
        }
        return min;
    }

    void operator()(const position_node & node,
                    std::vector<position_node> & expansion)
    {
        std::random_shuffle(connect_idx, connect_idx + 8);

//        mprf("expanding %d %d, string dist %d", node.pos.x, node.pos.y, node.string_distance);
        for (unsigned i=0; i < 8; i++)
        {
            position_node temp;

            temp.pos = node.pos + Compass[connect_idx[i]];
            temp.string_distance = node.string_distance;
            temp.departure = node.departure;
            temp.connect_level = node.connect_level;
            temp.path_distance = node.path_distance;
            temp.estimate = 0;

            if (!in_bounds(temp.pos))
                continue;


            if (feat_is_solid(env.grid(temp.pos)) || env.grid(temp.pos) == DNGN_LAVA)
            {
                temp.path_distance = DISCONNECT_DIST;
            }
            else
            {
                actor * act_at = actor_at(temp.pos);
                monster* mons_at = monster_at(temp.pos);

                if (!act_at)
                {
                    temp.path_distance += 1;
                }
                // Can still search through a firewood monster, just at a higher
                // path cost.
                else if (mons_at && mons_is_firewood(mons_at)
                    && !mons_aligned(kraken, mons_at))
                {
                    temp.path_distance += 10;
                }
                // An actor we can't path through is there
                else
                {
                    temp.path_distance = DISCONNECT_DIST;
                }

            }

            int connect_level = temp.connect_level;
            int base_connect_level = connect_level;

            std::map<coord_def, std::set<int> >::iterator probe
                        = connection_constraints->find(temp.pos);


            if (probe != connection_constraints->end())
            {
                int max_val = probe->second.empty() ? INT_MAX : *probe->second.rbegin();

                if (max_val < connect_level)
                    temp.departure = true;

                // If we can still feasibly retract (haven't left connect range)
                if (!temp.departure)
                {
                    while (probe->second.find(connect_level + 1) != probe->second.end() )
                    {
                        connect_level++;
                    }

                    int delta = connect_level - base_connect_level;
                    temp.connect_level = connect_level;
                    if (delta)
                    {
                        temp.string_distance -= delta;
                    }
                }


                if (connect_level < max_val)
                   temp.path_distance = DISCONNECT_DIST;
            }
            else
            {
                // We stopped retracting
                temp.departure = true;
            }

            if (temp.departure)
                temp.string_distance++;

            if (temp.string_distance > MAX_KRAKEN_TENTACLE_DIST)
                temp.path_distance = DISCONNECT_DIST;

            if (temp.path_distance != DISCONNECT_DIST)
                temp.estimate = min_dist(temp.pos);

            expansion.push_back(temp);
        }
    }

};


struct tentacle_connect_constraints
{
    monster* kraken;
    std::map<coord_def, std::set<int> > * connection_constraints;


    tentacle_connect_constraints()
    {
        for (int i=0; i<8; i++)
        {
            connect_idx[i] = i;
        }
    }

    int connect_idx[8];
    void operator()(const position_node & node,
                    std::vector<position_node> & expansion)
    {
        std::random_shuffle(connect_idx, connect_idx + 8);

        for (unsigned i=0; i < 8; i++)
        {
            position_node temp;

            temp.pos = node.pos + Compass[connect_idx[i]];

            if (!in_bounds(temp.pos))
                continue;

            std::map<coord_def, std::set<int> >::iterator probe
                        = connection_constraints->find(temp.pos);

            if (feat_is_solid(env.grid(temp.pos))
                || env.grid(temp.pos) == DNGN_LAVA
                || actor_at(temp.pos)
                )
//                || !kraken->see_cell_no_trans(temp.pos))
            {
                temp.path_distance = DISCONNECT_DIST;
            }
            else
                temp.path_distance = 1 + node.path_distance;

            if (probe == connection_constraints->end()
                || probe->second.find(node.connect_level) == probe->second.end())
            {
                continue;
            }


            //temp.estimate = grid_distance(temp.pos, kraken->pos());
            // Don't bother with an estimate, the search is highly constrained
            // so it's not really going to help
            temp.estimate = 0;
            int test_level = node.connect_level;

            for (std::set<int>::iterator j = probe->second.begin();
                 j!= probe->second.end();
                 j++)
            {
                if (*j == (test_level + 1))
                    test_level++;
            }

            int max = probe->second.empty() ? INT_MAX : *(probe->second.rbegin());

            if (test_level < max)
                continue;

            temp.connect_level = test_level;

            expansion.push_back(temp);
        }
    }

};

struct target_monster
{
    int target_mindex;

    bool operator() (const coord_def & pos)
    {
        monster* temp = monster_at(pos);
        if (!temp || temp->mindex() != target_mindex)
            return (false);
        return (true);

    }
};

struct multi_target
{
    std::vector<coord_def> * targets;

    bool operator() (const coord_def & pos)
    {
        for (unsigned i = 0; i < targets->size(); ++i)
        {
            if (pos == targets->at(i))
                return (true);
        }
        return (false);
    }


};

// returns pathfinding success/failure
bool tentacle_pathfind(monster* kraken, monster* tentacle,
                       std::vector<coord_def> & target_positions,
                       coord_def & new_position,
                       std::map<coord_def, std::set<int> > & connect_data,
                       int total_length)
{
    multi_target foe_check;
    foe_check.targets = &target_positions;

    std::vector<std::set<position_node>::iterator > tentacle_path;

    std::set<position_node> visited;
    visited.clear();

    tentacle_attack_constraints attack_constraints;
    attack_constraints.kraken = kraken;
    attack_constraints.connection_constraints = &connect_data;
    attack_constraints.target_positions = &target_positions;

    position_node temp;
    temp.pos = tentacle->pos();

    std::map<coord_def, std::set<int> >::iterator probe = connect_data.find(temp.pos);
    temp.connect_level = 0;
    while (probe->second.find(temp.connect_level + 1 ) != probe->second.end())
        temp.connect_level++;

    temp.departure = false;
    temp.string_distance = total_length;


    search_astar(temp,
                 foe_check, attack_constraints,
                 visited, tentacle_path);

    bool path_found = false;
    // Did we find a path?
    if (!tentacle_path.empty())
    {
        // The end position is the enemy or target square, we need
        // to rewind the found path to find the next move

        const position_node * current = &(*tentacle_path[0]);
        const position_node * last;


        // The last position in the chain is the base position,
        // so we want to stop at the one before the last.
        while (current && current->last)
        {
            last = current;
            current = current->last;
            new_position = last->pos;
            path_found = true;
        }
    }


    return (path_found);
}

bool try_tentacle_connect(const coord_def & new_pos, int headnum,
                          int tentacle_idx,
                          tentacle_connect_constraints & connect_costs)
{
    int start_level = 0;
    std::map<coord_def, std::set<int> >::iterator it
                    = connect_costs.connection_constraints->find(new_pos);

    // This condition should never miss
    if (it != connect_costs.connection_constraints->end())
    {
        while (it->second.find(start_level + 1) != it->second.end())
        {
            start_level++;
        }
    }

    // Find the tentacle -> head path
    target_monster current_target;
    current_target.target_mindex = headnum;

    std::set<position_node> visited;
    std::vector<std::set<position_node>::iterator> candidates;

    position_node temp;
    temp.pos = new_pos;
    temp.connect_level = start_level;

    search_astar(temp,
                 current_target, connect_costs,
                 visited, candidates);

    if (candidates.empty())
    {
        return (false);
    }

    _establish_connection(tentacle_idx, headnum,candidates[0]);

    return (true);
}

void collect_tentacles(int headnum, std::vector<monster_iterator> & tentacles)
{
    // TODO: reorder tentacles based on distance to head or something.
    for (monster_iterator mi; mi; ++mi)
     {
         if (int (mi->number) == headnum)
         {
             if (mi->type == MONS_KRAKEN_TENTACLE)
             {
                 tentacles.push_back(mi);
             }
         }
     }
}

void purge_connectors(int tentacle_idx)
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (int (mi->number) == tentacle_idx)
        {
            if (mi->type == MONS_KRAKEN_CONNECTOR)
            {
                int hp = menv[mi->mindex()].hit_points;
                if (hp > 0 && hp < menv[tentacle_idx].hit_points)
                    menv[tentacle_idx].hit_points = hp;

                monster_die(&env.mons[mi->mindex()],
                        KILL_MISC, NON_MONSTER, true);
            }

        }
    }
}

void collect_foe_positions(monster* kraken, std::vector<coord_def> & foe_positions)
{
    coord_def foe_pos(-1, -1);
    if (kraken->near_foe())
    {
        foe_positions.push_back(kraken->get_foe()->pos());
        foe_pos = foe_positions.back();
    }

    for (monster_iterator mi; mi; ++mi)
    {
        monster* test = &menv[mi->mindex()];
        if (!mons_is_firewood(test)
            && !mons_aligned(test, kraken)
            && test->pos() != foe_pos
            && kraken->can_see(test))
        {
            foe_positions.push_back(test->pos());
        }
    }
}


void move_kraken_tentacles(monster* kraken)
{
    if (mons_base_type(kraken) != MONS_KRAKEN
        || kraken->asleep())
    {
        return;
    }


    bool no_foe = false;
    actor * foe = NULL;

    std::vector<coord_def> foe_positions;
    collect_foe_positions(kraken, foe_positions);

    //if (!kraken->near_foe())
    if (foe_positions.empty()
        || kraken->behaviour == BEH_FLEE
        || kraken->behaviour == BEH_WANDER )
    {
        no_foe = true;
    }
    else
        foe = kraken->get_foe();

    std::vector<monster_iterator> tentacles;

    int headnum = kraken->mindex();

    collect_tentacles(headnum, tentacles);



    // Move each tentacle in turn
    for (unsigned i=0;i<tentacles.size();i++)
    {
        monster* tentacle = monster_at(tentacles[i]->pos());

        if (!tentacle)
        {
            mprf("missing tentacle in path");
            continue;
        }

        tentacle_connect_constraints connect_costs;
        std::map<coord_def, std::set<int> > connection_data;

        connect_costs.kraken = kraken;

        monster* current_mon = tentacle;
        int current_count = 0;
        bool retract_found = false;
        coord_def retract_pos(-1, -1);

        while (current_mon)
        {
            for (adjacent_iterator adj_it(current_mon->pos(), false);
                 adj_it; ++adj_it)
            {
                connection_data[*adj_it].insert(current_count);
            }

            bool basis = current_mon->props.exists("inwards");
            int next_idx = basis ? current_mon->props["inwards"].get_int() : -1;

            if (next_idx != -1 && menv[next_idx].alive()
                && (menv[next_idx].type == MONS_KRAKEN_CONNECTOR
                    || mons_base_type(&menv[next_idx]) == MONS_KRAKEN))
            {
                current_mon = &menv[next_idx];
                if (!retract_found && current_mon->type == MONS_KRAKEN_CONNECTOR)
                {
                    retract_pos = current_mon->pos();
                    retract_found = true;
                }
            }
            else
            {
                current_mon = NULL;
            }
            current_count++;
        }

        int tentacle_idx = tentacle->mindex();

        purge_connectors(tentacle_idx);

        if (no_foe
            && grid_distance(tentacle->pos(), kraken->pos()) == 1)
        {
            // Drop the tentacle if no enemies are in sight and it is
            // adjacent to the main body. This is to prevent players from
            // just sniping tentacles while outside the kraken's fov.
            monster_die(tentacle, KILL_MISC, NON_MONSTER, true);
            continue;
        }

        coord_def new_pos = tentacle->pos();
        coord_def old_pos = tentacle->pos();

        bool path_found = false;
        if (!no_foe)
        {
            path_found = tentacle_pathfind(kraken, tentacle, foe_positions, new_pos,
                                           connection_data, current_count);
        }

        if (no_foe || !path_found)
        {
            if (retract_found)
            {
                new_pos = retract_pos;
            }
            else
            {
                // What happened here? Usually retract found should be true
                // or we should get pruned (due to being adjacent to the
                // head), in any case just stay here.
            }
        }

        // Did we path into a target?
        actor * blocking_actor = actor_at(new_pos);
        if (blocking_actor)
        {
            tentacle->target = new_pos;
            monster* mtemp = monster_at(new_pos);
            if (mtemp)
            {
                tentacle->foe = mtemp->mindex();
            }
            else if (new_pos == you.pos())
            {
                tentacle->foe = MHITYOU;
            }

            new_pos = old_pos;
        }

        mgrd(tentacle->pos()) = NON_MONSTER;

        // Why do I have to do this move? I don't get it.
        // specifically, if tentacle isn't registered at its new position on mgrd
        // the search fails (sometimes), Don't know why. -cao
        tentacle->set_position(new_pos);
        mgrd(tentacle->pos()) = tentacle->mindex();

        connect_costs.connection_constraints = &connection_data;
        bool connected = try_tentacle_connect(new_pos, headnum, tentacle_idx, connect_costs);


        // Can't connect, usually the head moved and invalidated our position
        // in some way. Should look into this more at some point -cao
        if (!connected)
        {
            //mprf("CONNECT FAILED, PURGING TENTACLE");
            mgrd(tentacle->pos()) = tentacle->mindex();
            monster_die(tentacle, KILL_MISC, NON_MONSTER, true);

            continue;
        }

        tentacle->check_redraw(old_pos);
        tentacle->apply_location_effects(old_pos);

    }
}

//---------------------------------------------------------------
//
// mon_special_ability
//
//---------------------------------------------------------------
bool mon_special_ability(monster* mons, bolt & beem)
{
    bool used = false;

    const monster_type mclass = (mons_genus(mons->type) == MONS_DRACONIAN)
                                  ? draco_subspecies(mons)
                                  : static_cast<monster_type>(mons->type);

    // Slime creatures can split while out of sight.
    if ((!mons->near_foe() || mons->asleep() || mons->submerged())
         && mons->type != MONS_SLIME_CREATURE)
    {
        return (false);
    }

    const msg_channel_type spl = (mons->friendly() ? MSGCH_FRIEND_SPELL
                                                   : MSGCH_MONSTER_SPELL);

    spell_type spell = SPELL_NO_SPELL;

    circle_def c;
    switch (mclass)
    {
    case MONS_UGLY_THING:
    case MONS_VERY_UGLY_THING:
        // A (very) ugly thing's proximity to you if you're glowing, or
        // to others of its kind, can mutate it into a different (very)
        // ugly thing.
        used = ugly_thing_mutate(mons, true);
        break;

    case MONS_SLIME_CREATURE:
        // Slime creatures may split or merge depending on the
        // situation.
        used = _slime_split_merge(mons);
        if (!mons->alive())
            return (true);
        break;

    case MONS_ORC_KNIGHT:
    case MONS_ORC_WARLORD:
    case MONS_SAINT_ROKA:
        if (is_sanctuary(mons->pos()))
            break;

        used = _orc_battle_cry(mons);
        break;

    case MONS_ORANGE_STATUE:
        if (player_or_mon_in_sanct(mons))
            break;

        used = _orange_statue_effects(mons);
        break;

    case MONS_SILVER_STATUE:
        if (player_or_mon_in_sanct(mons))
            break;

        used = _silver_statue_effects(mons);
        break;

    case MONS_BALL_LIGHTNING:
        if (is_sanctuary(mons->pos()))
            break;

        if (mons->attitude == ATT_HOSTILE
            && distance(you.pos(), mons->pos()) <= 5)
        {
            mons->hit_points = -1;
            used = true;
            break;
        }

        c = circle_def(mons->pos(), 4, C_CIRCLE);
        for (monster_iterator targ(&c); targ; ++targ)
        {
            if (mons_aligned(mons, *targ))
                continue;

            if (mons->can_see(*targ) && !feat_is_solid(grd(targ->pos())))
            {
                mons->hit_points = -1;
                used = true;
                break;
            }
        }
        break;

    case MONS_LAVA_SNAKE:
        if (mons->has_ench(ENCH_CONFUSION))
            break;

        if (!you.visible_to(mons))
            break;

        if (coinflip())
            break;

        // Setup tracer.
        beem.name        = "glob of lava";
        beem.aux_source  = "glob of lava";
        beem.range       = 6;
        beem.damage      = dice_def(3, 10);
        beem.hit         = 20;
        beem.colour      = RED;
        beem.glyph       = dchar_glyph(DCHAR_FIRED_ZAP);
        beem.flavour     = BEAM_LAVA;
        beem.beam_source = mons->mindex();
        beem.thrower     = KILL_MON;

        // Fire tracer.
        fire_tracer(mons, beem);

        // Good idea?
        if (mons_should_fire(beem))
        {
            make_mons_stop_fleeing(mons);
            simple_monster_message(mons, " spits lava!");
            beem.fire();
            used = true;
        }
        break;

    case MONS_ELECTRIC_EEL:
        if (mons->has_ench(ENCH_CONFUSION))
            break;

        if (!you.visible_to(mons))
            break;

        if (coinflip())
            break;

        // Setup tracer.
        beem.name        = "bolt of electricity";
        beem.aux_source  = "bolt of electricity";
        beem.range       = 8;
        beem.damage      = dice_def( 3, 6 );
        beem.hit         = 50;
        beem.colour      = LIGHTCYAN;
        beem.glyph       = dchar_glyph(DCHAR_FIRED_ZAP);
        beem.flavour     = BEAM_ELECTRICITY;
        beem.beam_source = mons->mindex();
        beem.thrower     = KILL_MON;
        beem.is_beam     = true;

        // Fire tracer.
        fire_tracer(mons, beem);

        // Good idea?
        if (mons_should_fire(beem))
        {
            make_mons_stop_fleeing(mons);
            simple_monster_message(mons,
                                   " shoots out a bolt of electricity!");
            beem.fire();
            used = true;
        }
        break;

    case MONS_ACID_BLOB:
    case MONS_OKLOB_PLANT:
    case MONS_YELLOW_DRACONIAN:
    {
        if (mons->has_ench(ENCH_CONFUSION))
            break;

        if (player_or_mon_in_sanct(mons))
            break;

        bool spit = one_chance_in(3);
        if (mons->type == MONS_OKLOB_PLANT)
            spit = x_chance_in_y(mons->hit_dice, 30);

        if (spit)
        {
            spell = SPELL_ACID_SPLASH;
            setup_mons_cast(mons, beem, spell);

            // Fire tracer.
            fire_tracer(mons, beem);

            // Good idea?
            if (mons_should_fire(beem))
            {
                make_mons_stop_fleeing(mons);
                _mons_cast_abil(mons, beem, spell);
                used = true;
            }
        }
        break;
    }

    case MONS_MOTH_OF_WRATH:
        if (one_chance_in(3))
            used = _moth_incite_monsters(mons);
        break;

    case MONS_SNORG:
        if (mons->has_ench(ENCH_CONFUSION))
            break;

        if (mons->foe == MHITNOT
            || mons->foe == MHITYOU && mons->friendly())
        {
            break;
        }

        // There's a 5% chance of Snorg spontaneously going berserk that
        // increases to 20% once he is wounded.
        if (mons->hit_points == mons->max_hit_points && !one_chance_in(4))
            break;

        if (one_chance_in(5))
            mons->go_berserk(true);
        break;

    case MONS_PIT_FIEND:
        if (one_chance_in(3))
            break;
        // deliberate fall through
    case MONS_FIEND:
        if (mons->has_ench(ENCH_CONFUSION))
            break;

        if (player_or_mon_in_sanct(mons))
            break;

        // Friendly fiends won't use torment, preferring hellfire
        // (right now there is no way a monster can predict how
        // badly they'll damage the player with torment) -- GDL

        // Well, I guess you could allow it if the player is torment
        // resistant, but there's a very good reason torment resistant
        // players can't cast Torment themselves, and allowing your
        // allies to cast it would just introduce harmless Torment
        // through the backdoor. Thus, shouldn't happen. (jpeg)
        if (one_chance_in(4))
        {
            spell_type spell_cast = SPELL_NO_SPELL;

            switch (random2(4))
            {
            case 0:
                if (!mons->friendly())
                {
                    make_mons_stop_fleeing(mons);
                    spell_cast = SPELL_SYMBOL_OF_TORMENT;
                    _mons_cast_abil(mons, beem, spell_cast);
                    used = true;
                    break;
                }
                // deliberate fallthrough -- see above
            case 1:
            case 2:
            case 3:
                spell_cast = SPELL_HELLFIRE;
                setup_mons_cast(mons, beem, spell_cast);

                // Fire tracer.
                fire_tracer(mons, beem);

                // Good idea?
                if (mons_should_fire(beem))
                {
                    make_mons_stop_fleeing(mons);

                    _mons_cast_abil(mons, beem, spell_cast);
                    used = true;
                }
                break;
            }
        }
        break;

    case MONS_IMP:
    case MONS_PHANTOM:
    case MONS_INSUBSTANTIAL_WISP:
    case MONS_BLINK_FROG:
    case MONS_KILLER_KLOWN:
    case MONS_PRINCE_RIBBIT:
    case MONS_MARA:
    case MONS_MARA_FAKE:
    case MONS_GOLDEN_EYE:
        if (one_chance_in(7) || mons->caught() && one_chance_in(3))
            used = monster_blink(mons);
        break;

    case MONS_MANTICORE:
        if (mons->has_ench(ENCH_CONFUSION))
            break;

        if (!you.visible_to(mons))
            break;

        // The fewer spikes the manticore has left, the less
        // likely it will use them.
        if (random2(16) >= static_cast<int>(mons->number))
            break;

        // Do the throwing right here, since the beam is so
        // easy to set up and doesn't involve inventory.

        // Set up the beam.
        beem.name        = "volley of spikes";
        beem.aux_source  = "volley of spikes";
        beem.range       = 6;
        beem.hit         = 14;
        beem.damage      = dice_def( 2, 10 );
        beem.beam_source = mons->mindex();
        beem.glyph       = dchar_glyph(DCHAR_FIRED_MISSILE);
        beem.colour      = LIGHTGREY;
        beem.flavour     = BEAM_MISSILE;
        beem.thrower     = KILL_MON;
        beem.is_beam     = false;

        // Fire tracer.
        fire_tracer(mons, beem);

        // Good idea?
        if (mons_should_fire(beem))
        {
            make_mons_stop_fleeing(mons);
            simple_monster_message(mons, " flicks its tail!");
            beem.fire();
            used = true;
            // Decrement # of volleys left.
            mons->number--;
        }
        break;

    case MONS_PLAYER_GHOST:
    {
        const ghost_demon &ghost = *(mons->ghost);

        if (ghost.species < SP_RED_DRACONIAN
            || ghost.species == SP_GREY_DRACONIAN
            || ghost.species >= SP_BASE_DRACONIAN
            || ghost.xl < 7
            || one_chance_in(ghost.xl - 5))
        {
            break;
        }
    }
    // Intentional fallthrough

    case MONS_WHITE_DRACONIAN:
    case MONS_RED_DRACONIAN:
        spell = SPELL_DRACONIAN_BREATH;
    // Intentional fallthrough

    case MONS_ICE_DRAGON:
        if (spell == SPELL_NO_SPELL)
            spell = SPELL_COLD_BREATH;
    // Intentional fallthrough

    // Dragon breath weapons:
    case MONS_DRAGON:
    case MONS_HELL_HOUND:
    case MONS_LINDWURM:
    case MONS_FIRE_DRAKE:
    case MONS_XTAHUA:
        if (spell == SPELL_NO_SPELL)
            spell = SPELL_FIRE_BREATH;

        if (mons->has_ench(ENCH_CONFUSION))
            break;

        if (!you.visible_to(mons))
            break;

        if (mons->type != MONS_HELL_HOUND && x_chance_in_y(3, 13)
            || one_chance_in(10))
        {
            setup_mons_cast(mons, beem, spell);

            // Fire tracer.
            fire_tracer(mons, beem);

            // Good idea?
            if (mons_should_fire(beem))
            {
                make_mons_stop_fleeing(mons);
                _mons_cast_abil(mons, beem, spell);
                used = true;
            }
        }
        break;

    case MONS_MERMAID:
    case MONS_SIREN:
    {
        // Don't behold observer in the arena.
        if (crawl_state.game_is_arena())
            break;

        // Don't behold player already half down or up the stairs.
        if (!you.delay_queue.empty())
        {
            delay_queue_item delay = you.delay_queue.front();

            if (delay.type == DELAY_ASCENDING_STAIRS
                || delay.type == DELAY_DESCENDING_STAIRS)
            {
                dprf("Taking stairs, don't mesmerise.");
                break;
            }
        }

        // Won't sing if either of you silenced, or it's friendly,
        // confused, fleeing, or leaving the level.
        if (mons->has_ench(ENCH_CONFUSION)
            || mons_is_fleeing(mons)
            || mons->pacified()
            || mons->friendly()
            || !player_can_hear(mons->pos()))
        {
            break;
        }

        // Don't even try on berserkers. Mermaids know their limits.
        if (you.berserk())
            break;

        // Reduce probability because of spamminess.
        if (you.species == SP_MERFOLK && !one_chance_in(4))
            break;

        // A wounded invisible mermaid is less likely to give away her position.
        if (mons->invisible()
            && mons->hit_points <= mons->max_hit_points / 2
            && !one_chance_in(3))
        {
            break;
        }

        bool already_mesmerised = you.beheld_by(mons);

        if (one_chance_in(5)
            || mons->foe == MHITYOU && !already_mesmerised && coinflip())
        {
            noisy(LOS_RADIUS, mons->pos(), mons->mindex(), true);

            bool did_resist = false;
            if (you.can_see(mons))
            {
                simple_monster_message(mons,
                    make_stringf(" chants %s song.",
                    already_mesmerised ? "her luring" : "a haunting").c_str(),
                    spl);

                if (mons->type == MONS_SIREN)
                {
                    if (_siren_movement_effect(mons))
                    {
                        canned_msg(MSG_YOU_RESIST); // flavour only
                        did_resist = true;
                    }
                }
            }
            else
            {
                // If you're already mesmerised by an invisible mermaid she
                // can still prolong the enchantment; otherwise you "resist".
                if (already_mesmerised)
                    mpr("You hear a luring song.", MSGCH_SOUND);
                else
                {
                    if (one_chance_in(4)) // reduce spamminess
                    {
                        if (coinflip())
                            mpr("You hear a haunting song.", MSGCH_SOUND);
                        else
                            mpr("You hear an eerie melody.", MSGCH_SOUND);

                        canned_msg(MSG_YOU_RESIST); // flavour only
                    }
                    break;
                }
            }

            // Once mesmerised by a particular monster, you cannot resist
            // anymore.
            if (!already_mesmerised
                && (you.species == SP_MERFOLK || you.check_res_magic(100)))
            {
                if (!did_resist)
                    canned_msg(MSG_YOU_RESIST);
                break;
            }

            you.add_beholder(mons);

            used = true;
        }
        break;
    }

    default:
        break;
    }

    if (used)
        mons->lose_energy(EUT_SPECIAL);

    return (used);
}

// Combines code for eyeball-type monsters, etc. to reduce clutter.
static bool _eyeball_will_use_ability(monster* mons)
{
    return (coinflip()
        && !mons_is_confused(mons)
        && !mons_is_wandering(mons)
        && !mons_is_fleeing(mons)
        && !mons->pacified()
        && !player_or_mon_in_sanct(mons));
}

//---------------------------------------------------------------
//
// mon_nearby_ability
//
// Gives monsters a chance to use a special ability when they're
// next to the player.
//
//---------------------------------------------------------------
void mon_nearby_ability(monster* mons)
{
    actor *foe = mons->get_foe();
    if (!foe
        || !mons->can_see(foe)
        || mons->asleep()
        || mons->submerged())
    {
        return;
    }

    maybe_mons_speaks(mons);

    if (monster_can_submerge(mons, grd(mons->pos()))
        && !mons->caught()         // No submerging while caught.
        && !mons->asleep()         // No submerging when asleep.
        && !you.beheld_by(mons)    // No submerging if player entranced.
        && !mons_is_lurking(mons)  // Handled elsewhere.
        && mons->wants_submerge())
    {
        monsterentry* entry = get_monster_data(mons->type);

        mons->add_ench(ENCH_SUBMERGED);
        mons->speed_increment -= ENERGY_SUBMERGE(entry);
        return;
    }

    switch (mons->type)
    {
    case MONS_SPATIAL_VORTEX:
    case MONS_KILLER_KLOWN:
        // Choose random colour.
        mons->colour = random_colour();
        break;

    case MONS_GOLDEN_EYE:
        if (_eyeball_will_use_ability(mons))
        {
            const bool can_see = you.can_see(mons);
            if (can_see && you.can_see(foe))
                mprf("%s blinks at %s.",
                     mons->name(DESC_CAP_THE).c_str(),
                     foe->name(DESC_NOCAP_THE).c_str());

            int confuse_power = 2 + random2(3);

            if (foe->atype() == ACT_PLAYER && !can_see)
                mpr("You feel you are being watched by something.");

            if (foe->check_res_magic((mons->hit_dice * 5) * confuse_power))
            {
                if (foe->atype() == ACT_PLAYER)
                    canned_msg(MSG_YOU_RESIST);
                else if (foe->atype() == ACT_MONSTER)
                {
                    const monster* foe_mons = foe->as_monster();
                    simple_monster_message(foe_mons, mons_resist_string(foe_mons));
                }
                break;
            }

            foe->confuse(mons, 2 + random2(3));
        }
        break;

    case MONS_GIANT_EYEBALL:
        if (_eyeball_will_use_ability(mons))
        {
            const bool can_see = you.can_see(mons);
            if (can_see && you.can_see(foe))
                mprf("%s stares at %s.",
                     mons->name(DESC_CAP_THE).c_str(),
                     foe->name(DESC_NOCAP_THE).c_str());

            if (foe->atype() == ACT_PLAYER && !can_see)
                mpr("You feel you are being watched by something.");

            // Subtly different from old paralysis behaviour, but
            // it'll do.
            foe->paralyse(mons, 2 + random2(3));
        }
        break;

    case MONS_EYE_OF_DRAINING:
    case MONS_GHOST_MOTH:
        if (_eyeball_will_use_ability(mons) && foe->atype() == ACT_PLAYER)
        {
            if (you.can_see(mons))
                simple_monster_message(mons, " stares at you.");
            else
                mpr("You feel you are being watched by something.");

            int mp = std::min(5 + random2avg(13, 3), you.magic_points);
            dec_mp(mp);

            mons->heal(mp, true); // heh heh {dlb}
        }
        break;

    case MONS_AIR_ELEMENTAL:
        if (one_chance_in(5))
            mons->add_ench(ENCH_SUBMERGED);
        break;

    case MONS_PANDEMONIUM_DEMON:
        if (mons->ghost->cycle_colours)
            mons->colour = random_colour();
        break;

    default:
        break;
    }
}

// When giant spores move maybe place a ballistomycete on the they move
// off of.
void ballisto_on_move(monster* mons, const coord_def & position)
{
    if (mons->type == MONS_GIANT_SPORE
        && !mons->is_summoned())
    {
        dungeon_feature_type ftype = env.grid(mons->pos());

        if (ftype >= DNGN_FLOOR_MIN && ftype <= DNGN_FLOOR_MAX)
            env.pgrid(mons->pos()) |= FPROP_MOLD;

        // The number field is used as a cooldown timer for this behavior.
        if (mons->number <= 0)
        {
            if (one_chance_in(4))
            {
                beh_type attitude = actual_same_attitude(*mons);
                int rc = create_monster(mgen_data(MONS_BALLISTOMYCETE,
                                                  attitude,
                                                  NULL,
                                                  0,
                                                  0,
                                                  position,
                                                  MHITNOT,
                                                  MG_FORCE_PLACE));

                if (rc != -1)
                {
                    // Don't leave mold on squares we place ballistos on
                    remove_mold(position);
                    if  (you.can_see(&env.mons[rc]))
                        mprf("A ballistomycete grows in the wake of the spore.");
                }

                mons->number = 40;
            }
        }
        else
        {
            mons->number--;
        }

    }
}

static bool _ballisto_at(const coord_def & target)
{
    monster* mons = monster_at(target);
    return (mons && mons ->type == MONS_BALLISTOMYCETE
            && mons->alive());
}

static bool _player_at(const coord_def & target)
{
    return (you.pos() == target);
}

bool _mold_connected(const coord_def & target)
{
    return (is_moldy(target) || _ballisto_at(target));
}


// If 'monster' is a ballistomycete or spore, activate some number of
// ballistomycetes on the level.
void activate_ballistomycetes(monster* mons, const coord_def & origin,
                              bool player_kill)
{
    if (!mons || mons->is_summoned()
              || mons->mons_species() != MONS_BALLISTOMYCETE
                 && mons->type != MONS_GIANT_SPORE)
    {
        return;
    }

    // If a spore or inactive ballisto died we will only activate one
    // other ballisto. If it was an active ballisto we will distribute
    // its count to others on the level.
    int activation_count = 1;
    if (mons->type == MONS_BALLISTOMYCETE)
        activation_count += mons->number;
    if (mons->type == MONS_HYPERACTIVE_BALLISTOMYCETE)
        activation_count = 0;

    int non_activable_count = 0;
    int ballisto_count = 0;

    bool any_friendly = mons->attitude == ATT_FRIENDLY;
    bool fedhas_mode  = false;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->mindex() != mons->mindex() && mi->alive())
        {
            if (mi->type == MONS_BALLISTOMYCETE)
                ballisto_count++;
            else if (mi->type == MONS_GIANT_SPORE
                     || mi->type == MONS_HYPERACTIVE_BALLISTOMYCETE)
            {
                non_activable_count++;
            }

            if (mi->attitude == ATT_FRIENDLY)
                any_friendly = true;
        }
    }

    bool exhaustive = true;
    bool (*valid_target)(const coord_def & ) = _ballisto_at;
    bool (*connecting_square) (const coord_def &) = _mold_connected;

    std::set<position_node> visited;
    std::vector<std::set<position_node>::iterator > candidates;

    if (you.religion == GOD_FEDHAS)
    {
        if (non_activable_count == 0
            && ballisto_count == 0
            && any_friendly
            && mons->type == MONS_BALLISTOMYCETE)
        {
            mprf("Your fungal colony was destroyed.");
            dock_piety(5, 0);
        }

        fedhas_mode = true;
        activation_count = 1;
        exhaustive = false;
        valid_target = _player_at;
    }

    search_dungeon(origin, valid_target, connecting_square, visited,
                   candidates, exhaustive);

    if (candidates.empty())
    {
        if (player_kill
            && !fedhas_mode
            && non_activable_count == 0
            && ballisto_count == 0
            && mons->attitude == ATT_HOSTILE)
        {
            mprf("Having destroyed the fungal colony, you feel a bit more "
                 "experienced.");
            gain_exp(500);

            // Get rid of the mold, so it'll be more useful when new fungi
            // spawn.
            // NOTE: Not triggered if eradication happens by hostile monster,
            //       so we don't give anything away. (jpeg)
            for (rectangle_iterator ri(1); ri; ++ri)
                remove_mold(*ri);
        }

        return;
    }

    // A (very) soft cap on colony growth, no activations if there are
    // already a lot of ballistos on level.
    if (candidates.size() > 25)
        return;


    std::random_shuffle(candidates.begin(), candidates.end());

    int index = 0;

    for (int i=0; i<activation_count; ++i)
    {
        index = i % candidates.size();

        monster* spawner = monster_at(candidates[index]->pos);

        // This may be the players position, in which case we don't
        // have to mess with spore production on anything
        if (spawner && !fedhas_mode)
        {
            spawner->number++;

            // Change color and start the spore production timer if we
            // are moving from 0 to 1.
            if (spawner->number == 1)
            {
                spawner->colour = RED;
                // Reset the spore production timer.
                spawner->del_ench(ENCH_SPORE_PRODUCTION, false);
                spawner->add_ench(ENCH_SPORE_PRODUCTION);
            }
        }

        const position_node * thread = &(*candidates[index]);
        while (thread)
        {
            if (you.see_cell(thread->pos))
            {
                env.pgrid(thread->pos) |= FPROP_GLOW_MOLD;

            }

            thread = thread->last;
        }
    }

}
