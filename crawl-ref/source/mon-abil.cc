/**
 * @file
 * @brief Monster abilities.
**/

#include "AppHdr.h"
#include "mon-abil.h"

#include "externs.h"

#include "act-iter.h"
#include "arena.h"
#include "beam.h"
#include "colour.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "dgnevent.h" //XXX
#include "evoke.h" // wind_blast
#include "exclude.h" //XXX
#ifdef USE_TILE
 #include "tiledef-dngn.h"
 #include "tilepick.h"
#endif
#include "fight.h"
#include "fprop.h"
#include "ghost.h"
#include "itemprop.h"
#include "losglobal.h"
#include "libutil.h"
#include "misc.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-chimera.h"
#include "mon-iter.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mon-util.h"
#include "mutation.h"
#include "terrain.h"
#include "mgen_data.h"
#include "cloud.h"
#include "mon-speak.h"
#include "mon-stuff.h"
#include "random.h"
#include "religion.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "env.h"
#include "areas.h"
#include "view.h"
#include "shout.h"
#include "viewchar.h"
#include "ouch.h"
#include "target.h"
#include "items.h"
#include "mapmark.h"
#include "teleport.h"

#include <algorithm>
#include <queue>
#include <map>
#include <set>
#include <cmath>

const int MAX_KRAKEN_TENTACLE_DIST = 12;

static bool _slime_split_merge(monster* thing);
template<typename valid_T, typename connect_T>
static void _search_dungeon(const coord_def & start,
                    valid_T & valid_target,
                    connect_T & connecting_square,
                    set<position_node> & visited,
                    vector<set<position_node>::iterator> & candidates,
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


    queue<set<position_node>::iterator > fringe;

    set<position_node>::iterator current = visited.insert(temp_node).first;
    fringe.push(current);


    bool done = false;
    while (!fringe.empty())
    {
        current = fringe.front();
        fringe.pop();

        shuffle_array(compass_idx, connect_mode);

        for (int i=0; i < connect_mode; ++i)
        {
            coord_def adjacent = current->pos + Compass[compass_idx[i]];
            if (in_bounds(adjacent))
            {
                temp_node.pos = adjacent;
                temp_node.last = &(*current);
                pair<set<position_node>::iterator, bool > res;
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

// Currently only used for Tiamat.
void draconian_change_colour(monster* drac)
{
    if (mons_genus(drac->type) != MONS_DRACONIAN)
        return;

    switch (random2(5))
    {
    case 0:
        drac->colour = RED;
        drac->base_monster = MONS_RED_DRACONIAN;
        break;

    case 1:
        drac->colour = WHITE;
        drac->base_monster = MONS_WHITE_DRACONIAN;
        break;

    case 2:
        drac->colour = BLUE;
        drac->base_monster = MONS_BLACK_DRACONIAN;
        break;

    case 3:
        drac->colour = GREEN;
        drac->base_monster = MONS_GREEN_DRACONIAN;
        break;

    case 4:
        drac->colour = MAGENTA;
        drac->base_monster = MONS_PURPLE_DRACONIAN;
        break;

    default:
        break;
    }
}

bool ugly_thing_mutate(monster* ugly, bool proximity)
{
    bool success = false;

    string src = "";

    colour_t mon_colour = BLACK;

    if (!proximity)
        success = true;
    else if (one_chance_in(9))
    {
        int you_mutate_chance = 0;
        int ugly_mutate_chance = 0;
        int mon_mutate_chance = 0;

        for (adjacent_iterator ri(ugly->pos()); ri; ++ri)
        {
            if (you.pos() == *ri)
                you_mutate_chance = get_contamination_level();
            else
            {
                monster* mon_near = monster_at(*ri);

                if (!mon_near
                    || !mons_class_flag(mon_near->type, M_GLOWS_RADIATION))
                {
                    continue;
                }

                const bool ugly_type =
                    mon_near->type == MONS_UGLY_THING
                        || mon_near->type == MONS_VERY_UGLY_THING;

                int i = mon_near->type == MONS_VERY_UGLY_THING ? 3 :
                        mon_near->type == MONS_UGLY_THING      ? 2
                                                               : 1;

                for (; i > 0; --i)
                {
                    if (coinflip())
                    {
                        if (ugly_type)
                        {
                            ugly_mutate_chance++;

                            if (coinflip())
                            {
                                const colour_t ugly_colour =
                                    make_low_colour(ugly->colour);
                                const colour_t ugly_near_colour =
                                    make_low_colour(mon_near->colour);

                                if (ugly_colour != ugly_near_colour)
                                    mon_colour = ugly_near_colour;
                            }
                        }
                        else
                            mon_mutate_chance++;
                    }
                }
            }
        }

        // The maximum number of monsters that can surround this monster
        // is 8, and the maximum mutation chance from each surrounding
        // monster is 3, so the maximum mutation value is 24.
        you_mutate_chance = min(24, you_mutate_chance);
        ugly_mutate_chance = min(24, ugly_mutate_chance);
        mon_mutate_chance = min(24, mon_mutate_chance);

        if (!one_chance_in(you_mutate_chance
                           + ugly_mutate_chance
                           + mon_mutate_chance
                           + 1))
        {
            int proximity_chance = you_mutate_chance;
            int proximity_type = 0;

            if (ugly_mutate_chance > proximity_chance
                || (ugly_mutate_chance == proximity_chance && coinflip()))
            {
                proximity_chance = ugly_mutate_chance;
                proximity_type = 1;
            }

            if (mon_mutate_chance > proximity_chance
                || (mon_mutate_chance == proximity_chance && coinflip()))
            {
                proximity_chance = mon_mutate_chance;
                proximity_type = 2;
            }

            src  = " from ";
            src += proximity_type == 0 ? "you" :
                   proximity_type == 1 ? "its kin"
                                       : "its neighbour";

            success = true;
        }
    }

    if (success)
    {
        simple_monster_message(ugly,
            make_stringf(" basks in the mutagenic energy%s and changes!",
                         src.c_str()).c_str());

        ugly->uglything_mutate(mon_colour);

        return true;
    }

    return false;
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

// What to do about any enchantments these two creatures may have?
// For now, we are averaging the durations, weighted by slime size
// or by hit dice, depending on usehd.
static void _merge_ench_durations(monster* initial, monster* merge_to, bool usehd = false)
{
    mon_enchant_list::iterator i;

    int initial_count = usehd ? initial->hit_dice : initial->number;
    int merge_to_count = usehd ? merge_to->hit_dice : merge_to->number;
    int total_count = initial_count + merge_to_count;

    for (i = initial->enchantments.begin();
         i != initial->enchantments.end(); ++i)
    {
        // Does the other creature have this enchantment as well?
        mon_enchant temp = merge_to->get_ench(i->first);
        // If not, use duration 0 for their part of the average.
        bool no_initial = temp.ench == ENCH_NONE;
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
        if (initial->enchantments.find(i->first)
                == initial->enchantments.end()
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
// Now it returns index of new slime (-1 if it fails).
static monster* _do_split(monster* thing, coord_def & target)
{
    ASSERT(thing->alive());

    // Create a new slime.
    mgen_data new_slime_data = mgen_data(thing->type,
                                         thing->behaviour,
                                         0,
                                         0,
                                         0,
                                         target,
                                         thing->foe,
                                         MG_FORCE_PLACE);

    // Don't explicitly announce the child slime coming into view if you
    // saw the split that created it
    if (you.can_see(thing))
        new_slime_data.extra_flags |= MF_WAS_IN_VIEW;

    monster *new_slime = create_monster(new_slime_data);

    if (!new_slime)
        return 0;

    if (you.can_see(thing))
        mprf("%s splits.", thing->name(DESC_A).c_str());

    // Inflict the new slime with any enchantments on the parent.
    _split_ench_durations(thing, new_slime);
    new_slime->attitude = thing->attitude;
    new_slime->flags = thing->flags;
    new_slime->props = thing->props;
    new_slime->summoner = thing->summoner;
    if (thing->props.exists("blame"))
        new_slime->props["blame"] = thing->props["blame"].get_vector();

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

    ASSERT(thing->alive());
    ASSERT(new_slime->alive());

    return new_slime;
}

// Cause a monster to lose a turn.  has_gone should be true if the
// monster has already moved this turn.
static void _lose_turn(monster* mons, bool has_gone)
{
    const monsterentry* entry = get_monster_data(mons->type);

    // We want to find out if mons will move next time it has a turn
    // (assuming for the sake of argument the next delay is 10).  If it's
    // already going to lose a turn we don't need to do anything.
    mons->speed_increment += entry->speed;
    if (!mons->has_action_energy())
        return;
    mons->speed_increment -= entry->speed;

    mons->speed_increment -= entry->energy_usage.move;

    // So we subtracted some energy above, but if mons hasn't moved yet
    // /this turn, that will just cancel its turn in this round of
    // world_reacts().
    if (!has_gone)
        mons->speed_increment -= entry->energy_usage.move;
}

// Merge a crawling corpse/macabre mass into another corpse/mass or an
// abomination.
static bool _do_merge_crawlies(monster* crawlie, monster* merge_to)
{
    const int orighd = merge_to->hit_dice;
    int addhd = crawlie->hit_dice;

    // Need twice as many HD past 15.
    if (orighd > 15)
        addhd = (1 + addhd)/2;
    else if (orighd + addhd > 15)
        addhd = (15 - orighd) + (1 + addhd - (15 - orighd))/2;

    int newhd = orighd + addhd;
    monster_type new_type;
    int hp, mhp;

    if (newhd < 6)
    {
        // Not big enough for an abomination yet.
        new_type = MONS_MACABRE_MASS;
        mhp = merge_to->max_hit_points + crawlie->max_hit_points;
        hp = merge_to->hit_points += crawlie->hit_points;
    }
    else
    {
        // Need 11 HD and 3 corpses for a large abomination.
        if (newhd < 11
            || (crawlie->type == MONS_CRAWLING_CORPSE
                && merge_to->type == MONS_CRAWLING_CORPSE))
        {
            new_type = MONS_ABOMINATION_SMALL;
            newhd = min(newhd, 15);
        }
        else
        {
            new_type = MONS_ABOMINATION_LARGE;
            newhd = min(newhd, 30);
        }

        // Recompute in case we limited newhd.
        addhd = newhd - orighd;

        if (merge_to->type == MONS_ABOMINATION_SMALL)
        {
            // Adding to an existing abomination.
            const int hp_gain = hit_points(addhd, 2, 5);
            mhp = merge_to->max_hit_points + hp_gain;
            hp = merge_to->hit_points + hp_gain;
        }
        else
        {
            // Making a new abomination.
            hp = mhp = hit_points(newhd, 2, 5);
        }
    }

    const monster_type old_type = merge_to->type;
    const string old_name = merge_to->name(DESC_A);

    // Change the monster's type if we need to.
    if (new_type != old_type)
        change_monster_type(merge_to, new_type);

    // Combine enchantment durations (weighted by original HD).
    merge_to->hit_dice = orighd;
    _merge_ench_durations(crawlie, merge_to, true);

    init_abomination(merge_to, newhd);
    merge_to->max_hit_points = mhp;
    merge_to->hit_points = hp;

    // TODO: probably should be more careful about which flags.
    merge_to->flags |= crawlie->flags;

    _lose_turn(merge_to, merge_to->mindex() < crawlie->mindex());

    behaviour_event(merge_to, ME_EVAL);

    // Messaging.
    if (you.can_see(merge_to))
    {
        const bool changed = new_type != old_type;
        if (you.can_see(crawlie))
        {
            if (crawlie->type == old_type)
            {
                mprf("Two %s merge%s%s.",
                     pluralise(crawlie->name(DESC_PLAIN)).c_str(),
                     changed ? " to form " : "",
                     changed ? merge_to->name(DESC_A).c_str() : "");
            }
            else
            {
                mprf("%s merges with %s%s%s.",
                     crawlie->name(DESC_A).c_str(),
                     old_name.c_str(),
                     changed ? " to form " : "",
                     changed ? merge_to->name(DESC_A).c_str() : "");
            }
        }
        else if (changed)
        {
            mprf("%s suddenly becomes %s.",
                 uppercase_first(old_name).c_str(),
                 merge_to->name(DESC_A).c_str());
        }
        else
            mprf("%s twists grotesquely.", merge_to->name(DESC_A).c_str());
    }
    else if (you.can_see(crawlie))
        mprf("%s suddenly disappears!", crawlie->name(DESC_A).c_str());

    // Now kill the other monster.
    monster_die(crawlie, KILL_DISMISSED, NON_MONSTER, true);

    return true;
}

// Actually merge two slime creatures, pooling their hp, etc.
// initial_slime is the one that gets killed off by this process.
static bool _do_merge_slimes(monster* initial_slime, monster* merge_to)
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

    // Merging costs the combined slime some energy.  The idea is that if 2
    // slimes merge you can gain a space by moving away the turn after (maybe
    // this is too nice but there will probably be a lot of complaints about
    // the damage on higher level slimes).  We see if mons has gone already by
    // checking its mindex (this works because handle_monsters just iterates
    // over env.mons in ascending order).
    _lose_turn(merge_to, merge_to->mindex() < initial_slime->mindex());

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
                 merge_to->name(DESC_A).c_str());
        }
        else
        {
            mprf("A slime creature suddenly becomes %s.",
                 merge_to->name(DESC_A).c_str());
        }

        flash_view_delay(LIGHTGREEN, 150);
    }
    else if (you.can_see(initial_slime))
        mpr("A slime creature suddenly disappears!");

    // Have to 'kill' the slime doing the merging.
    monster_die(initial_slime, KILL_DISMISSED, NON_MONSTER, true);

    return true;
}

// Slime creatures can split but not merge under these conditions.
static bool _unoccupied_slime(monster* thing)
{
    return (thing->asleep() || mons_is_wandering(thing)
            || thing->foe == MHITNOT);
}

// Slime creatures cannot split or merge under these conditions.
static bool _disabled_merge(monster* thing)
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
    if (!thing || _disabled_merge(thing) || _unoccupied_slime(thing))
        return false;

    int max_slime_merge = 5;
    int compass_idx[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    shuffle_array(compass_idx, 8);
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
            && other_thing->has_ench(ENCH_CHARM) == thing->has_ench(ENCH_CHARM)
            && other_thing->is_summoned() == thing->is_summoned()
            && !other_thing->is_shapeshifter()
            && !_disabled_merge(other_thing))
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
        return _do_merge_slimes(thing, merge_target);

    // No adjacent slime creatures we could merge with.
    return false;
}

static bool _crawlie_is_mergeable(monster *mons)
{
    if (!mons)
        return false;

    switch (mons->type)
    {
    case MONS_ABOMINATION_SMALL:
    case MONS_CRAWLING_CORPSE:
    case MONS_MACABRE_MASS:
        break;
    default:
        return false;
    }

    return !(mons->is_shapeshifter() || _disabled_merge(mons));
}

static bool _crawling_corpse_merge(monster *crawlie)
{
    if (!crawlie || _disabled_merge(crawlie))
        return false;

    int compass_idx[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    shuffle_array(compass_idx, 8);
    coord_def origin = crawlie->pos();

    monster* merge_target = NULL;
    // Check for adjacent crawlies
    for (int i = 0; i < 8; ++i)
    {
        coord_def target = origin + Compass[compass_idx[i]];

        // Is there a crawlie/abomination on this square we can merge with?
        monster* other_thing = monster_at(target);
        if (!merge_target && _crawlie_is_mergeable(other_thing))
            merge_target = other_thing;
    }

    if (merge_target)
        return _do_merge_crawlies(crawlie, merge_target);

    // No adjacent crawlies.
    return false;
}


static bool _slime_can_spawn(const coord_def target)
{
    return (mons_class_can_pass(MONS_SLIME_CREATURE, env.grid(target))
            && !actor_at(target));
}

// See if slime creature 'thing' can split, and carry out the split if
// we can find a square to place the new slime creature on.
static monster *_slime_split(monster* thing, bool force_split)
{
    if (!thing || thing->number <= 1
        || (coinflip() && !force_split) // Don't make splitting quite so reliable. (jpeg)
        || _disabled_merge(thing))
    {
        return 0;
    }

    const coord_def origin  = thing->pos();

    const actor* foe        = thing->get_foe();
    const bool has_foe      = (foe != NULL && thing->can_see(foe));
    const coord_def foe_pos = (has_foe ? foe->position : coord_def(0,0));
    const int old_dist      = (has_foe ? grid_distance(origin, foe_pos) : 0);

    if ((has_foe && old_dist > 1) && !force_split)
    {
        // If we're not already adjacent to the foe, check whether we can
        // move any closer. If so, do that rather than splitting.
        for (radius_iterator ri(origin, 1, true, false, true); ri; ++ri)
        {
            if (_slime_can_spawn(*ri)
                && grid_distance(*ri, foe_pos) < old_dist)
            {
                return 0;
            }
        }
    }

    int compass_idx[] = {0, 1, 2, 3, 4, 5, 6, 7};
    shuffle_array(compass_idx, 8);

    // Anywhere we can place an offspring?
    for (int i = 0; i < 8; ++i)
    {
        coord_def target = origin + Compass[compass_idx[i]];

        // Don't split if this increases the distance to the target.
        if (has_foe && grid_distance(target, foe_pos) > old_dist
            && !force_split)
        {
            continue;
        }

        if (_slime_can_spawn(target))
        {
            // This can fail if placing a new monster fails.  That
            // probably means we have too many monsters on the level,
            // so just return in that case.
            return _do_split(thing, target);
        }
    }

    // No free squares.
    return 0;
}

// See if a given slime creature can split or merge.
static bool _slime_split_merge(monster* thing)
{
    // No merging/splitting shapeshifters.
    if (!thing
        || thing->is_shapeshifter()
        || thing->type != MONS_SLIME_CREATURE)
    {
        return false;
    }

    if (_slime_split(thing, false))
        return true;

    return _slime_merge(thing);
}

// Splits and polymorphs merged slime creatures.
bool slime_creature_polymorph(monster* slime)
{
    ASSERT(slime->type == MONS_SLIME_CREATURE);

    if (slime->number > 1 && x_chance_in_y(4, 5))
    {
        int count = 0;
        while (slime->number > 1 && count <= 10)
        {
            if (monster *splinter = _slime_split(slime, true))
                slime_creature_polymorph(splinter);
            else
                break;
            count++;
        }
    }

    return monster_polymorph(slime, RANDOM_MONSTER);
}

static bool _starcursed_split(monster* mon)
{
    if (!mon
        || mon->number <= 1
        || mon->type != MONS_STARCURSED_MASS)
    {
        return false;
    }

    const coord_def origin = mon->pos();

    int compass_idx[] = {0, 1, 2, 3, 4, 5, 6, 7};
    shuffle_array(compass_idx, 8);

    // Anywhere we can place an offspring?
    for (int i = 0; i < 8; ++i)
    {
        coord_def target = origin + Compass[compass_idx[i]];

        if (mons_class_can_pass(MONS_STARCURSED_MASS, env.grid(target))
            && !actor_at(target))
        {
            return _do_split(mon, target);
        }
    }

    // No free squares.
    return false;
}

static void _starcursed_scream(monster* mon, actor* target)
{
    if (!target || !target->alive())
        return;

    // These monsters have too primitive a mind to be affected
    if (!target->is_player() && mons_intel(target->as_monster()) <= I_INSECT)
        return;

    //Gather the chorus
    vector<monster*> chorus;

    for (actor_iterator ai(target->get_los()); ai; ++ai)
    {
        if (ai->is_monster())
        {
            monster* m = ai->as_monster();
            if (m->type == MONS_STARCURSED_MASS)
                chorus.push_back(m);
        }
    }

    int n = chorus.size();
    int dam = 0; int stun = 0;
    string message;

    dprf("Chorus size: %d", n);

    if (n > 7)
    {
        message = "A cacophony of accursed wailing tears at your sanity!";
        if (coinflip())
            stun = 2;
    }
    else if (n > 4)
    {
        message = "A deafening chorus of shrieks assaults your mind!";
        if (x_chance_in_y(1,3))
            stun = 1;
    }
    else if (n > 1)
        message = "A chorus of shrieks assaults your mind.";
    else
        message = "The starcursed mass shrieks in your mind.";

    dam = 4 + random2(5) + random2(n * 3 / 2);

    if (!target->is_player())
    {
        if (you.see_cell(target->pos()))
        {
            mprf(target->as_monster()->friendly() ? MSGCH_FRIEND_SPELL
                                                  : MSGCH_MONSTER_SPELL,
                 "%s writhes in pain as voices assail %s mind.",
                 target->name(DESC_THE).c_str(),
                 target->pronoun(PRONOUN_POSSESSIVE).c_str());
        }
        target->hurt(mon, dam);
    }
    else
    {
        mpr(message, MSGCH_MONSTER_SPELL);
        ouch(dam, mon->mindex(), KILLED_BY_BEAM, "accursed screaming");
    }

    if (stun && target->alive())
        target->paralyse(mon, stun, "accursed screaming");

    for (unsigned int i = 0; i < chorus.size(); ++i)
        if (chorus[i]->alive())
            chorus[i]->add_ench(mon_enchant(ENCH_SCREAMED, 1, chorus[i], 1));
}

static bool _will_starcursed_scream(monster* mon)
{
    vector<monster*> chorus;

    for (actor_iterator ai(mon->get_los()); ai; ++ai)
    {
        if (ai->is_monster())
        {
            monster* m = ai->as_monster();
            if (m->type == MONS_STARCURSED_MASS)
            {
                //Don't scream if any part of the chorus has a scream timeout
                //(This prevents it being staggered into a bunch of mini-screams)
                if (m->has_ench(ENCH_SCREAMED))
                    return false;
                else
                    chorus.push_back(m);
            }
        }
    }

    return x_chance_in_y(1, chorus.size() + 1);
}

// Returns true if you resist the siren's call.
static bool _siren_movement_effect(const monster* mons)
{
    bool do_resist = (you.attribute[ATTR_HELD] || you.check_res_magic(70) > 0
                      || you.cannot_act() || you.asleep()
                      || you.clarity());

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

        if (!in_bounds(newpos)
            || (is_feat_dangerous(grd(newpos)) && !you.can_cling_to(newpos))
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
                    && !mon->is_projectile()
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
                             mon->name(DESC_THE).c_str());
                        return do_resist;
                    }

                    int swap_mon = mgrd(newpos);
                    // Pick the monster up.
                    mgrd(newpos) = NON_MONSTER;
                    mon->moveto(oldpos);

                    // Plunk it down.
                    mgrd(mon->pos()) = swap_mon;

                    mprf("You swap places with %s.",
                         mon->name(DESC_THE).c_str());
                }
                move_player_to_grid(newpos, true, true);

                if (swapping)
                    mon->apply_location_effects(newpos);
            }
        }
    }

    return do_resist;
}

static bool _silver_statue_effects(monster* mons)
{
    actor *foe = mons->get_foe();

    int abjuration_duration = 5;

    // Tone down friendly silver statues for Zotdef.
    if (mons->attitude == ATT_FRIENDLY && !(foe && foe->is_player())
        && crawl_state.game_is_zotdef())
    {
        if (!one_chance_in(3))
            return false;
        abjuration_duration = 1;
    }

    if (foe && mons->can_see(foe) && !one_chance_in(3))
    {
        const string msg = "'s eyes glow " + weird_glowing_colour() + '.';
        simple_monster_message(mons, msg.c_str(), MSGCH_WARN);

        create_monster(
            mgen_data(
                summon_any_demon((coinflip() ? RANDOM_DEMON_COMMON
                                             : RANDOM_DEMON_LESSER)),
                SAME_ATTITUDE(mons), mons, abjuration_duration, 0,
                foe->pos(), mons->foe));
        return true;
    }
    return false;
}

static bool _orange_statue_effects(monster* mons)
{
    actor *foe = mons->get_foe();

    int pow  = random2(15);
    int fail = random2(150);

    if (foe && mons->can_see(foe) && !one_chance_in(3))
    {
        // Tone down friendly OCSs for Zotdef.
        if (mons->attitude == ATT_FRIENDLY && !foe->is_player()
            && crawl_state.game_is_zotdef())
        {
            if (foe->check_res_magic(120) > 0)
                return false;
            pow  /= 2;
            fail /= 2;
        }

        if (you.can_see(foe))
        {
            if (foe->is_player())
                mprf(MSGCH_WARN, "A hostile presence attacks your mind!");
            else if (you.can_see(mons))
                mprf(MSGCH_WARN, "%s fixes %s piercing gaze on %s.",
                     mons->name(DESC_THE).c_str(),
                     mons->pronoun(PRONOUN_POSSESSIVE).c_str(),
                     foe->name(DESC_THE).c_str());
        }

        MiscastEffect(foe, mons->mindex(), SPTYP_DIVINATION,
                      pow, fail,
                      "an orange crystal statue");
        return true;
    }

    return false;
}

enum battlecry_type
{
    BATTLECRY_ORC,
    BATTLECRY_CHERUB_HYMN,
    BATTLECRY_SATYR_PIPES,
    NUM_BATTLECRIES
};

static bool _is_battlecry_compatible(monster* mons, battlecry_type type)
{
    switch(type)
    {
        case BATTLECRY_ORC:
            return (mons_genus(mons->type) == MONS_ORC);
        case BATTLECRY_CHERUB_HYMN:
            return (mons->holiness() == MH_HOLY);
        case BATTLECRY_SATYR_PIPES:
            return (mons->holiness() == MH_NATURAL);
        default:
            return false;
    }
}

static int _battle_cry(monster* chief, battlecry_type type)
{
    const actor *foe = chief->get_foe();
    int affected = 0;

    enchant_type battlecry = (type == BATTLECRY_ORC ? ENCH_BATTLE_FRENZY
                                                    : ENCH_ROUSED);

    const string messages[NUM_BATTLECRIES][4] =
    {
        {
            "%s roars a battle-cry!",
            "%s goes into a battle-frenzy!",
            "%s %s go into a battle-frenzy!",
            "orcs"
        },
        {
            "%s sings a powerful hymn!",
            "%s is roused by the hymn!",
            "%s %s are roused to righteous anger!",
            "holy creatures"
        },
        {
            "%s plays a rousing melody on his pipes!",
            "%s is stirred to greatness!",
            "%s %s are stirred to greatness!",
            "satyr's allies"
        },
    };

    if (foe
        && (!foe->is_player() || !chief->friendly())
        && !silenced(chief->pos())
        && !chief->has_ench(ENCH_MUTE)
        && chief->can_see(foe)
        && coinflip())
    {
        const int level = chief->hit_dice > 12? 2 : 1;
        vector<monster* > seen_affected;
        for (monster_iterator mi(chief); mi; ++mi)
        {
            if (*mi != chief
                && _is_battlecry_compatible(*mi, type)
                && mons_aligned(chief, *mi)
                && mi->hit_dice < chief->hit_dice
                && !mi->berserk_or_insane()
                && !mi->has_ench(ENCH_MIGHT)
                && !mi->cannot_move()
                && !mi->confused())
            {
                mon_enchant ench = mi->get_ench(battlecry);
                if (ench.ench == ENCH_NONE || ench.degree < level)
                {
                    const int dur =
                        random_range(12, 20) * speed_to_duration(mi->speed);

                    if (ench.ench != ENCH_NONE)
                    {
                        ench.degree   = level;
                        ench.duration = max(ench.duration, dur);
                        mi->update_ench(ench);
                    }
                    else
                        mi->add_ench(mon_enchant(battlecry, level, chief, dur));

                    affected++;
                    if (you.can_see(*mi))
                        seen_affected.push_back(*mi);

                    if (mi->asleep())
                        behaviour_event(*mi, ME_DISTURB, 0, chief->pos());
                }
            }
        }

        if (affected)
        {
            if (you.can_see(chief) && player_can_hear(chief->pos()))
            {
                mprf(MSGCH_SOUND, messages[type][0].c_str(),
                     chief->name(DESC_THE).c_str());
            }

            // The yell happens whether you happen to see it or not.
            noisy(LOS_RADIUS, chief->pos(), chief->mindex());

            // Disabling detailed frenzy announcement because it's so spammy.
            const msg_channel_type channel =
                        chief->friendly() ? MSGCH_MONSTER_ENCHANT
                                          : MSGCH_FRIEND_ENCHANT;

            if (!seen_affected.empty())
            {
                string who;
                if (seen_affected.size() == 1)
                {
                    who = seen_affected[0]->name(DESC_THE);
                    mprf(channel, messages[type][1].c_str(), who.c_str());
                }
                else
                {
                    bool generic = false;
                    monster_type mon_type = seen_affected[0]->type;
                    for (unsigned int i = 0; i < seen_affected.size(); i++)
                    {
                        if (seen_affected[i]->type != mon_type)
                        {
                            // not homogeneous - use the generic term instead
                            generic = true;
                            break;
                        }
                    }
                    who = get_monster_data(mon_type)->name;


                    mprf(channel, messages[type][2].c_str(),
                         chief->friendly() ? "Your" : "The",
                         (!generic ? pluralise(who).c_str()
                                   : messages[type][3].c_str()));
                }
            }
        }
    }

    return affected;
}

static bool _make_monster_angry(const monster* mon, monster* targ)
{
    if (mon->friendly() != targ->friendly())
        return false;

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
            return false;
        victim = vmons->pos();
    }
    else
    {
        // Should be impossible. needs_berserk should find this case.
        die("angered by no foe");
    }

    // If mon may be blocking targ from its victim, don't try.
    if (victim.distance_from(targ->pos()) > victim.distance_from(mon->pos()))
        return false;

    if (you.can_see(mon))
    {
        const string targ_name = (targ->visible_to(&you)) ? targ->name(DESC_THE)
                                                          : "something";
        if (mon->type == MONS_QUEEN_BEE && targ->type == MONS_KILLER_BEE)
        {
            mprf("%s calls on %s to defend %s!",
                mon->name(DESC_THE).c_str(),
                targ_name.c_str(),
                mon->pronoun(PRONOUN_OBJECTIVE).c_str());
        }
        else
            mprf("%s goads %s on!", mon->name(DESC_THE).c_str(),
                 targ_name.c_str());
    }

    targ->go_berserk(false);

    return true;
}

static bool _moth_polymorph(const monster* mon)
{
    if (is_sanctuary(you.pos()) || is_sanctuary(mon->pos()))
        return false;

    circle_def c(mon->pos(), 4, C_ROUND);
    for (monster_iterator mi(&c); mi; ++mi)
    {
        if (is_sanctuary(mi->pos()))
            continue;

        if (!you.see_cell(mi->pos()))
            continue;

        if (!mon->can_see(*mi))
            continue;

        if (mi->type == MONS_POLYMOTH)
            continue;

        // Decrease the chances of repeatedly polymorphing high HD monsters.
        if (mi->flags & MF_POLYMORPHED)
        {
          int skip_chance = 1 + pow(mi->hit_dice, 0.5);
          if (!one_chance_in(skip_chance))
              continue;
        }

        // No ally scumming.
        if (mon->friendly() || mi->friendly() || mi->neutral())
            continue;

        // Polymorphing firewood is very, very bad.
        if (mons_is_firewood(*mi))
            continue;

        if (one_chance_in(2))
        {
            const string targ_name = (mi->visible_to(&you)) ? mi->name(DESC_THE)
                                                            : "something";

            if (you.can_see(*mi))
            {
                mprf("%s irradiates %s!",
                    mon->name(DESC_THE).c_str(),
                    targ_name.c_str());
            }
            monster_polymorph(*mi, RANDOM_TOUGHER_MONSTER);
            return true;
        }
    }

    return false;
}

static bool _moth_incite_monsters(const monster* mon)
{
    if (is_sanctuary(you.pos()) || is_sanctuary(mon->pos()))
        return false;

    int goaded = 0;
    circle_def c(mon->pos(), 4, C_ROUND);
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
            return true;
    }

    return goaded != 0;
}

static bool _queen_incite_worker(const monster* queen)
{
    ASSERT(queen->type == MONS_QUEEN_BEE);
    if (is_sanctuary(you.pos()) || is_sanctuary(queen->pos()))
        return false;

    int goaded = 0;
    circle_def c(queen->pos(), 4, C_ROUND);
    for (monster_iterator mi(&c); mi; ++mi)
    {
        // Only goad killer bees
        if (mi->type != MONS_KILLER_BEE)
            continue;

        if (*mi == queen || !mi->needs_berserk())
            continue;

        if (is_sanctuary(mi->pos()))
            continue;

        if (_make_monster_angry(queen, *mi) && !one_chance_in(3 * ++goaded))
            return true;
    }

    return goaded != 0;

}

static void _set_door(set<coord_def> door, dungeon_feature_type feat)
{
    for (set<coord_def>::const_iterator i = door.begin();
         i != door.end(); ++i)
    {
        grd(*i) = feat;
        set_terrain_changed(*i);
    }
}

// Find an adjacent space to displace a stack of items or a creature
// (If act is null, we are just moving items and not an actor)
bool get_push_space(const coord_def& pos, coord_def& newpos, actor* act,
                    bool ignore_tension, const vector<coord_def>* excluded)
{
    if (act && act->is_monster() && mons_is_stationary(act->as_monster()))
        return false;

    int max_tension = -1;
    coord_def best_spot(-1, -1);
    bool can_push = false;
    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        dungeon_feature_type feat = grd(*ai);
        if (feat_has_solid_floor(feat))
        {
            // Extra checks if we're moving a monster instead of an item
            if (act)
            {
                if (actor_at(*ai)
                    || !act->can_pass_through(*ai)
                    || !act->is_habitable(*ai))
                {
                    continue;
                }

                bool spot_vetoed = false;
                if (excluded)
                {
                    for (unsigned int i = 0; i < excluded->size(); ++i)
                        if (excluded->at(i) == *ai)
                        {
                            spot_vetoed = true;
                            break;
                        }
                }
                if (spot_vetoed)
                    continue;

                // If we don't care about tension, first valid spot is acceptable
                if (ignore_tension)
                {
                    newpos = *ai;
                    return true;
                }
                else // Calculate tension with monster at new location
                {
                    set<coord_def> all_door;
                    find_connected_identical(pos, grd(pos), all_door);
                    dungeon_feature_type old_feat = grd(pos);

                    act->move_to_pos(*ai);
                    _set_door(all_door, DNGN_CLOSED_DOOR);
                    int new_tension = get_tension(GOD_NO_GOD);
                    _set_door(all_door, old_feat);
                    act->move_to_pos(pos);

                    if (new_tension > max_tension)
                    {
                        max_tension = new_tension;
                        best_spot = *ai;
                        can_push = true;
                    }
                }
            }
            else //If we're not moving a creature, the first open spot is enough
            {
                newpos = *ai;
                return true;
            }
        }
    }

    if (can_push)
        newpos = best_spot;
    return can_push;
}

bool has_push_space(const coord_def& pos, actor* act,
                    const vector<coord_def>* excluded)
{
    coord_def dummy(-1, -1);
    return get_push_space(pos, dummy, act, true, excluded);
}

static bool _can_force_door_shut(const coord_def& door)
{
    if (grd(door) != DNGN_OPEN_DOOR)
        return false;

    set<coord_def> all_door;
    vector<coord_def> veto_spots;
    find_connected_identical(door, grd(door), all_door);
    copy(all_door.begin(), all_door.end(), back_inserter(veto_spots));

    for (set<coord_def>::const_iterator i = all_door.begin();
         i != all_door.end(); ++i)
    {
        // Only attempt to push players and non-hostile monsters out of
        // doorways
        actor* act = actor_at(*i);
        if (act)
        {
            if (act->is_player()
                || act->is_monster()
                    && act->as_monster()->attitude != ATT_HOSTILE)
            {
                coord_def newpos;
                if (!get_push_space(*i, newpos, act, true, &veto_spots))
                    return false;
                else
                    veto_spots.push_back(newpos);
            }
            else
                return false;
        }
        // If there are items in the way, see if there's room to push them
        // out of the way
        else if (igrd(*i) != NON_ITEM)
        {
            if (!has_push_space(*i, 0))
                return false;
        }
    }

    // Didn't find any items we couldn't displace
    return true;
}

static bool _should_force_door_shut(const coord_def& door)
{
    if (grd(door) != DNGN_OPEN_DOOR)
        return false;

    dungeon_feature_type old_feat = grd(door);
    int cur_tension = get_tension(GOD_NO_GOD);

    set<coord_def> all_door;
    vector<coord_def> veto_spots;
    find_connected_identical(door, grd(door), all_door);
    copy(all_door.begin(), all_door.end(), back_inserter(veto_spots));

    bool player_in_door = false;
    for (set<coord_def>::const_iterator i = all_door.begin();
        i != all_door.end(); ++i)
    {
        if (you.pos() == *i)
        {
            player_in_door = true;
            break;
        }
    }

    int new_tension;
    if (player_in_door)
    {
        coord_def newpos;
        coord_def oldpos = you.pos();
        get_push_space(oldpos, newpos, &you, false, &veto_spots);
        you.move_to_pos(newpos);
        _set_door(all_door, DNGN_CLOSED_DOOR);
        new_tension = get_tension(GOD_NO_GOD);
        _set_door(all_door, old_feat);
        you.move_to_pos(oldpos);
    }
    else
    {
        _set_door(all_door, DNGN_CLOSED_DOOR);
        new_tension = get_tension(GOD_NO_GOD);
        _set_door(all_door, old_feat);
    }

    // If closing the door would reduce player tension by too much, probably
    // it is scarier for the player to leave it open and thus it should be left
    // open

    // Currently won't allow tension to be lowered by more than 33%
    return (((cur_tension - new_tension) * 3) <= cur_tension);
}

static bool _seal_doors_and_stairs(const monster* warden)
{
    ASSERT(warden);
    ASSERT(warden->type == MONS_VAULT_WARDEN);

    int num_closed = 0;
    int seal_duration = 80 + random2(80);
    bool player_pushed = false;
    bool had_effect = false;

    for (radius_iterator ri(you.pos(), LOS_RADIUS, C_ROUND);
                 ri; ++ri)
    {
        if (grd(*ri) == DNGN_OPEN_DOOR)
        {
            if (!_can_force_door_shut(*ri))
                continue;

            // If it's scarier to leave this door open, do so
            if (!_should_force_door_shut(*ri))
                continue;

            set<coord_def> all_door;
            vector<coord_def> veto_spots;
            find_connected_identical(*ri, grd(*ri), all_door);
            copy(all_door.begin(), all_door.end(), back_inserter(veto_spots));
            for (set<coord_def>::const_iterator i = all_door.begin();
                 i != all_door.end(); ++i)
            {
                // If there are things in the way, push them aside
                actor* act = actor_at(*i);
                if (igrd(*i) != NON_ITEM || act)
                {
                    coord_def newpos;
                    get_push_space(*i, newpos, act, false, &veto_spots);
                    move_items(*i, newpos);
                    if (act)
                    {
                        actor_at(*i)->move_to_pos(newpos);
                        if (act->is_player())
                            player_pushed = true;
                        veto_spots.push_back(newpos);
                    }
                }
            }

            // Close the door
            bool seen = false;
            vector<coord_def> excludes;
            for (set<coord_def>::const_iterator i = all_door.begin();
                 i != all_door.end(); ++i)
            {
                const coord_def& dc = *i;
                grd(dc) = DNGN_CLOSED_DOOR;
                set_terrain_changed(dc);
                dungeon_events.fire_position_event(DET_DOOR_CLOSED, dc);

                if (is_excluded(dc))
                    excludes.push_back(dc);

                if (you.see_cell(dc))
                    seen = true;

                had_effect = true;
            }

            if (seen)
            {
                for (set<coord_def>::const_iterator i = all_door.begin();
                     i != all_door.end(); ++i)
                {
                    if (env.map_knowledge(*i).seen())
                    {
                        env.map_knowledge(*i).set_feature(DNGN_CLOSED_DOOR);
#ifdef USE_TILE
                        env.tile_bk_bg(*i) = TILE_DNGN_CLOSED_DOOR;
#endif
                    }
                }

                update_exclusion_los(excludes);
                ++num_closed;
            }
        }

        // Try to seal the door
        if (grd(*ri) == DNGN_CLOSED_DOOR || grd(*ri) == DNGN_RUNED_DOOR)
        {
            set<coord_def> all_door;
            find_connected_identical(*ri, grd(*ri), all_door);
            for (set<coord_def>::const_iterator i = all_door.begin();
                 i != all_door.end(); ++i)
            {
                temp_change_terrain(*i, DNGN_SEALED_DOOR, seal_duration,
                                    TERRAIN_CHANGE_DOOR_SEAL, warden);
                had_effect = true;
            }
        }
        else if (feat_is_travelable_stair(grd(*ri)))
        {
            dungeon_feature_type stype;
            if (feat_stair_direction(grd(*ri)) == CMD_GO_UPSTAIRS)
                stype = DNGN_SEALED_STAIRS_UP;
            else
                stype = DNGN_SEALED_STAIRS_DOWN;

            temp_change_terrain(*ri, stype, seal_duration,
                                TERRAIN_CHANGE_DOOR_SEAL, warden);
            had_effect = true;
        }
    }

    if (had_effect)
    {
        mprf(MSGCH_MONSTER_SPELL, "%s activates a sealing rune.",
                (warden->visible_to(&you) ? warden->name(DESC_THE, true).c_str()
                                          : "Someone"));
        if (num_closed > 1)
            mpr("The doors slam shut!");
        else if (num_closed == 1)
            mpr("A door slams shut!");

        if (player_pushed)
            mpr("You are pushed out of the doorway!");

        return true;
    }

    return false;
}

static bool _flay_creature(monster* mon, actor* victim)
{
    int dam;
    bool was_flayed = false;

    if (victim->holiness() != MH_NATURAL)
        return false;

    if (victim->is_player())
    {
        dam = (6 + (you.hp * 18 / you.hp_max)) * you.hp_max / 100;
        dam = min(dam, max(0, you.hp - 25 - random2(15)));
        if (dam < 10)
            return false;

        if (you.duration[DUR_FLAYED])
            was_flayed = true;

        you.duration[DUR_FLAYED] = max(you.duration[DUR_FLAYED], 55 + random2(66));
    }
    else
    {
        monster* mon_victim = victim->as_monster();

        dam = (6 + (mon_victim->hit_points * 18 / mon_victim->max_hit_points))
                   * mon_victim->max_hit_points / 100;
        dam = min(dam, max(0, mon_victim->hit_points - 25 - random2(15)));
        if (dam < 10)
            return false;

        if (mon_victim->has_ench(ENCH_FLAYED))
        {
            was_flayed = true;
            mon_enchant flayed = mon_victim->get_ench(ENCH_FLAYED);
            flayed.duration = min(flayed.duration + 30 + random2(50), 150);
            mon_victim->update_ench(flayed);
        }
        else
        {
            mon_enchant flayed(ENCH_FLAYED, 1, mon, 30 + random2(50));
            mon_victim->add_ench(flayed);
        }
    }

    if (you.can_see(mon))
    {
        bool silent = silenced(mon->pos()) || silenced(you.pos());
        int msg_type =  (silent ? random2(2) : random2(3) + 1);

        switch (msg_type)
        {
            case 0:
                mprf(MSGCH_MONSTER_SPELL, "%s moans in silent anguish.",
                    mon->name(DESC_THE).c_str());
                break;
            case 1:
                mprf(MSGCH_MONSTER_SPELL, "%s stares at %s with tortured malice.",
                    mon->name(DESC_THE).c_str(), victim->name(DESC_THE).c_str());
                break;
            case 2:
                mprf(MSGCH_MONSTER_SPELL, "%s cries, \"Suffer as I suffered!\"",
                    mon->name(DESC_THE).c_str());
                break;
            case 3:
                mprf(MSGCH_MONSTER_SPELL, "%s cries, \"Feel what I felt!\"",
                    mon->name(DESC_THE).c_str());
                break;
        }
    }

    if (you.can_see(victim))
    {
        if (was_flayed)
        {
            mprf("Terrible wounds spread across more of %s body!",
                 victim->name(DESC_ITS).c_str());
        }
        else
        {
            mprf("Terrible wounds open up all over %s body!",
                 victim->name(DESC_ITS).c_str());
        }
    }

    if (victim->is_player())
    {
        // Bypassing ::hurt so that flay damage can ignore guardian spirit
        ouch(dam, mon->mindex(), KILLED_BY_MONSTER, "flay_damage",
             mon->visible_to(&you));
    }
    else
        victim->hurt(mon, dam, BEAM_NONE, true);
    victim->props["flay_damage"].get_int() += dam;

    vector<coord_def> old_blood;
    CrawlVector &new_blood = victim->props["flay_blood"].get_vector();

    // Find current blood splatters
    for (radius_iterator ri(victim->get_los()); ri; ++ri)
    {
        if (env.pgrid(*ri) & FPROP_BLOODY)
            old_blood.push_back(*ri);
    }

    blood_spray(victim->pos(), victim->type, 20);

    // Compute and store new blood splatters
    unsigned int i = 0;
    for (radius_iterator ri(victim->get_los()); ri; ++ri)
    {
        if (env.pgrid(*ri) & FPROP_BLOODY)
        {
            if (i < old_blood.size() && old_blood[i] == *ri)
                ++i;
            else
                new_blood.push_back(*ri);
        }
    }

    return true;
}

void heal_flayed_effect(actor* act, bool quiet, bool blood_only)
{
    if (!blood_only)
    {
        if (act->is_player())
            you.duration[DUR_FLAYED] = 0;
        else
            act->as_monster()->del_ench(ENCH_FLAYED, true, false);

        if (you.can_see(act) && !quiet)
        {
            mprf("The terrible wounds on %s body vanish.",
                 act->name(DESC_ITS).c_str());
        }

        act->heal(act->props["flay_damage"].get_int());
        act->props.erase("flay_damage");
    }

    CrawlVector &blood = act->props["flay_blood"].get_vector();

    for (int i = 0; i < blood.size(); ++i)
        env.pgrid(blood[i].get_coord()) &= ~FPROP_BLOODY;
    act->props.erase("flay_blood");
}

void end_flayed_effect(monster* ghost)
{
    if (you.duration[DUR_FLAYED] && !ghost->wont_attack()
        && cell_see_cell(ghost->pos(), you.pos(), LOS_DEFAULT))
    {
        heal_flayed_effect(&you);
    }

    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->has_ench(ENCH_FLAYED) && !mons_aligned(ghost, *mi)
            && cell_see_cell(ghost->pos(), mi->pos(), LOS_DEFAULT))
        {
            heal_flayed_effect(*mi);
        }
    }
}

static bool _lost_soul_affectable(const monster* mons)
{
    return (((mons->holiness() == MH_UNDEAD
              && mons->type != MONS_LOST_SOUL
              && !mons_is_zombified(mons))
             ||(mons->holiness() == MH_NATURAL
                && mons_can_be_zombified(mons)))
            && !mons->is_summoned()
            && !mons_class_flag(mons->type, M_NO_EXP_GAIN));
}

struct hd_sorter
{
    bool operator()(const pair<monster* ,int> &a, const pair<monster* ,int> &b)
    {
        return a.second > b.second;
    }
};

static bool _lost_soul_teleport(monster* mons)
{
    bool seen = you.can_see(mons);

   vector<pair<monster*, int> > candidates;

    // Assemble candidate list and randomize
    for (monster_iterator mi; mi; ++mi)
    {
        if (_lost_soul_affectable(*mi) && mons_aligned(mons, *mi))
        {
            pair<monster* , int> m = make_pair(*mi, min(mi->hit_dice, 18)
                                                    + random2(8));
            candidates.push_back(m);
        }
    }
    sort(candidates.begin(), candidates.end(), hd_sorter());

    for (unsigned int i = 0; i < candidates.size(); ++i)
    {
        coord_def empty;
        if (find_habitable_spot_near(candidates[i].first->pos(), mons_base_type(mons), 3, false, empty)
            && mons->move_to_pos(empty))
        {
            mons->add_ench(ENCH_SUBMERGED);
            mons->behaviour = BEH_WANDER;
            mons->foe = MHITNOT;
            mons->props["band_leader"].get_int() = candidates[i].first->mid;
            if (seen)
            {
                mprf("%s flickers out of the living world.",
                        mons->name(DESC_THE, true).c_str());
            }
            return true;
        }
    }

    // If we can't find anywhere useful to go, flicker away to stop the player
    // being annoyed chasing after us
    if (one_chance_in(3))
    {
        if (seen)
        {
            mprf("%s flickers out of the living world.",
                        mons->name(DESC_THE, true).c_str());
        }
        monster_die(mons, KILL_MISC, -1, true);
        return true;
    }

    return false;
}

// Is it worth sacrificing ourselves to revive this monster? This is based
// on monster HD, with a lower chance for weaker monsters so long as other
// monsters are present, but always true if there are only as many valid
// targets as nearby lost souls.
static bool _worthy_sacrifice(monster* soul, const monster* target)
{
    int count = 0;
    for (monster_iterator mi(soul->get_los_no_trans()); mi; ++mi)
    {
        if (_lost_soul_affectable(*mi))
            ++count;
        else if (mi->type == MONS_LOST_SOUL)
            --count;
    }

    return (count <= -1 || target->hit_dice > 9
            || x_chance_in_y(target->hit_dice * target->hit_dice * target->hit_dice,
                             1200));
}

bool lost_soul_revive(monster* mons)
{
    if (!_lost_soul_affectable(mons))
        return false;

    for (monster_iterator mi(mons->get_los_no_trans()); mi; ++mi)
    {
        if (mi->type == MONS_LOST_SOUL && mons_aligned(mons, *mi))
        {
            if (!_worthy_sacrifice(*mi, mons))
                continue;

            targetter_los hitfunc(*mi, LOS_SOLID);
            flash_view_delay(GREEN, 200, &hitfunc);

            if (you.can_see(*mi))
            {
                mprf("%s sacrifices itself to reknit %s!",
                     mi->name(DESC_THE).c_str(),
                     mons->name(DESC_THE).c_str());
            }
            else if (you.can_see(mons))
            {
                mprf("Necromantic energies suffuse and reknit %s!",
                     mons->name(DESC_THE).c_str());
            }

            mons->heal(mons->max_hit_points);
            mons->del_ench(ENCH_CONFUSION, true);
            mons->timeout_enchantments(10);
            monster_die(*mi, KILL_MISC, -1, true);
            return true;
        }
    }

    return false;
}

bool lost_soul_spectralize(monster* mons)
{
    if (!_lost_soul_affectable(mons))
        return false;

    for (monster_iterator mi(mons); mi; ++mi)
    {
        if (mi->type == MONS_LOST_SOUL && mons_aligned(mons, *mi))
        {
            if (!_worthy_sacrifice(*mi, mons))
                continue;

            targetter_los hitfunc(*mi, LOS_SOLID);
            flash_view_delay(GREEN, 200, &hitfunc);

            if (you.can_see(*mi))
            {
                mprf("The lost soul assumes the form of %s%s!",
                     mons->name(DESC_THE).c_str(),
                     (mi->is_summoned() ? " and becomes anchored to this world"
                                        : ""));
            }

            define_zombie(*mi, mons->type, MONS_SPECTRAL_THING);
            mi->flags |= MF_NO_REWARD | MF_SEEN;
            mi->flags |= mons->flags & (MF_MELEE_MASK | MF_SPELL_MASK);
            mi->spells = mons->spells;
            mi->god = GOD_NAMELESS;

            name_zombie(*mi, mons);
            invalidate_agrid();

            // Duplicate objects, or unequip them if they can't be duplicated.
            for (int i = 0; i < NUM_MONSTER_SLOTS; i++)
            {
                const int old_index = mons->inv[i];

                if (old_index == NON_ITEM)
                    continue;

                const int new_index = get_mitm_slot(0);
                if (new_index == NON_ITEM)
                {
                    mi->unequip(mitm[old_index], i, 0, true);
                    mi->inv[i] = NON_ITEM;
                    continue;
                }

                mi->inv[i]      = new_index;
                mitm[new_index] = mitm[old_index];
                mitm[new_index].set_holding_monster(mi->mindex());
                mitm[new_index].flags |= ISFLAG_SUMMONED;

                // Make holy items plain
                if (get_weapon_brand(mitm[new_index]) == SPWPN_HOLY_WRATH)
                    set_item_ego_type(mitm[new_index], OBJ_WEAPONS, SPWPN_NORMAL);
            }

            return true;
        }
    }

    return false;
}

void treant_release_wasps(monster* mons)
{
    int count = mons->number;
    bool created = false;
    for (int i = 0; i < count; ++i)
    {
        monster_type wasp_t = (one_chance_in(3) ? MONS_RED_WASP
                                                : MONS_YELLOW_WASP);

        mgen_data wasp_data(wasp_t, SAME_ATTITUDE(mons),
                            mons, 0, SPELL_NO_SPELL, mons->pos(),
                            mons->foe);
        wasp_data.extra_flags |= MF_WAS_IN_VIEW;
        monster* wasp = create_monster(wasp_data);

        if (wasp)
        {
            wasp->props["band_leader"].get_int() = mons->mid;
            created = true;
            mons->number--;
        }
    }

    if (created && you.can_see(mons))
    {
        mprf("Angry insects surge out from beneath %s foliage!",
            mons->name(DESC_ITS).c_str());
    }
}

static void _entangle_actor(actor* act)
{
    if (act->is_player())
    {
        you.duration[DUR_GRASPING_ROOTS] = 10;
        you.redraw_evasion = true;
        you.duration[DUR_FLIGHT] = 0;
        you.attribute[ATTR_PERM_FLIGHT] = 0;
        land_player(true);
    }
    else
    {
        monster* mact = act->as_monster();
        mact->add_ench(mon_enchant(ENCH_GRASPING_ROOTS, 1, NULL, INFINITE_DURATION));
    }
}

// Returns true if there are any affectable hostiles are in range of the effect
// (whether they were affected or not this round)
bool apply_grasping_roots(monster* mons)
{
    if (you.see_cell(mons->pos()) && one_chance_in(12))
    {
        mpr(random_choose(
                "Tangled roots snake along the ground.",
                "The ground creaks as gnarled roots bulge its surface.",
                "A root reaches out and grasps at passing movement.",
                0), MSGCH_TALK_VISUAL);
    }

    bool found_hostile = false;
    for (actor_iterator ai(mons); ai; ++ai)
    {
        if (!mons_aligned(mons, *ai))
        {
            // Cannot grasp insubstancial creatures
            if (ai->is_monster() && ai->as_monster()->is_insubstantial()
                || ai->is_player() && you.form == TRAN_WISP)
            {
                continue;
            }

            found_hostile = true;

            // Only applies over land, not water or lava.
            if (!feat_has_dry_floor(grd(ai->pos())))
                continue;

            // (Some messages are suppressed for monsters, to reduce message spam)
            if (ai->flight_mode() && x_chance_in_y(2, 5))
            {
                if (x_chance_in_y(10, 50 - ai->melee_evasion(NULL)))
                {
                    if (ai->is_player())
                        mpr("Roots rise up to grasp you, but you nimbly evade.");
                }
                else
                {
                    if (you.can_see(*ai))
                    {
                        mprf("Roots rise up from beneath %s and drag %s%s"
                                "to the ground.", ai->name(DESC_THE).c_str(),
                                ai->is_monster() ? "it" : "you",
                                ai->is_monster() ? " " : " back");
                    }
                    _entangle_actor(*ai);
                }
            }
            else if (!ai->flight_mode())
            {
                if (ai->is_player() && !you.duration[DUR_GRASPING_ROOTS])
                {
                    mprf("Roots grasp at your %s, making movement difficult.",
                        you.foot_name(true).c_str());
                }
                _entangle_actor(*ai);
            }
        }
    }

    return (found_hostile);
}

void check_grasping_roots(actor* act, bool quiet)
{
    bool source = false;
    for (monster_iterator mi(act->get_los()) ; mi; ++mi)
    {
        if (!mons_aligned(act, *mi) && mi->has_ench(ENCH_GRASPING_ROOTS_SOURCE))
        {
            source = true;
            break;
        }
    }

    if (!source || !feat_has_dry_floor(grd(act->pos())))
    {
        if (act->is_player())
        {
            if (!quiet)
                mpr("You escape the reach of the grasping roots.");
            you.duration[DUR_GRASPING_ROOTS] = 0;
            you.redraw_evasion = true;
        }
        else
            act->as_monster()->del_ench(ENCH_GRASPING_ROOTS);
    }
}

static bool _swoop_attack(monster* mons, actor* defender)
{
    coord_def target = defender->pos();

    bolt tracer;
    tracer.source = mons->pos();
    tracer.target = target;
    tracer.is_tracer = true;
    tracer.range = LOS_RADIUS;
    tracer.fire();

    for (unsigned int j = 0; j < tracer.path_taken.size(); ++j)
    {
        if (tracer.path_taken[j] == target)
        {
            if (tracer.path_taken.size() > j + 1)
            {
                if (monster_habitable_grid(mons, grd(tracer.path_taken[j+1]))
                    && !actor_at(tracer.path_taken[j+1]))
                {
                    if (you.can_see(mons))
                    {
                        mprf("%s swoops through the air toward %s!",
                             mons->name(DESC_THE).c_str(),
                             defender->name(DESC_THE).c_str());
                    }
                    mons->move_to_pos(tracer.path_taken[j+1]);
                    fight_melee(mons, defender);
                    return true;
                }
            }
        }
    }

    return false;
}

static inline void _mons_cast_abil(monster* mons, bolt &pbolt,
                                   spell_type spell_cast)
{
    mons_cast(mons, pbolt, spell_cast, true, true);
}

static void _establish_connection(int tentacle,
                                  int head,
                                  set<position_node>::iterator path,
                                  monster_type connector_type)
{
    const position_node * last = &(*path);
    const position_node * current = last->last;

    // Tentacle is adjacent to the end position, not much to do.
    if (!current)
    {
        // This is a little awkward now. Oh well. -cao
        if (tentacle != head)
            menv[tentacle].props["inwards"].get_int() = head;
        else
            menv[tentacle].props["inwards"].get_int() = -1;

     //   mprf ("null tentacle thing, res %d", menv[tentacle].props["inwards"].get_int());
        return;
    }

    monster* main = &env.mons[head];

    // No base monster case (demonic tentacles)
    if (!monster_at(last->pos))
    {
        if (monster *connect = create_monster(
            mgen_data(connector_type, SAME_ATTITUDE(main), main,
                      0, 0, last->pos, main->foe,
                      MG_FORCE_PLACE, main->god, MONS_NO_MONSTER, tentacle,
                      main->colour, PROX_CLOSE_TO_PLAYER)))
        {
            connect->props["inwards"].get_int()  = -1;
            connect->props["outwards"].get_int() = -1;

            if (main->holiness() == MH_UNDEAD)
                connect->flags |= MF_FAKE_UNDEAD;

            connect->max_hit_points = menv[tentacle].max_hit_points;
            connect->hit_points = menv[tentacle].hit_points;
        }
        else
        {
            // Big failure mode.
            return;
        }
    }

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
            current_mons->props["inwards"].get_int() = last_mon_idx;
            last_mon->props["outwards"].get_int() = current_mons->mindex();
            break;
        }

         // place a connector
        if (monster *connect = create_monster(
            mgen_data(connector_type, SAME_ATTITUDE(main), main,
                      0, 0, current->pos, main->foe,
                      MG_FORCE_PLACE, main->god, MONS_NO_MONSTER, tentacle,
                      main->colour, PROX_CLOSE_TO_PLAYER)))
        {
            connect->max_hit_points = menv[tentacle].max_hit_points;
            connect->hit_points = menv[tentacle].hit_points;

            connect->props["inwards"].get_int() = last_mon_idx;
            connect->props["outwards"].get_int() = -1;

            if (last_mon->type == connector_type)
                menv[last_mon_idx].props["outwards"].get_int() = connect->mindex();

            if (main->holiness() == MH_UNDEAD)
                connect->flags |= MF_FAKE_UNDEAD;

            if (monster_can_submerge(connect, env.grid(connect->pos())))
                connect->add_ench(ENCH_SUBMERGED);
        }
        else
        {
            // connector placement failed, what to do?
            mprf("connector placement failed at %d %d", current->pos.x, current->pos.y);
        }

        last = current;
        current = current->last;
    }
}

struct tentacle_attack_constraints
{
    vector<coord_def> * target_positions;

    map<coord_def, set<int> > * connection_constraints;
    monster *base_monster;
    int max_string_distance;
    int connect_idx[8];

    tentacle_attack_constraints()
    {
        for (int i=0; i<8; i++)
            connect_idx[i] = i;
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
                    vector<position_node> & expansion)
    {
        shuffle_array(connect_idx, 8);

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

            if (!in_bounds(temp.pos) || is_sanctuary(temp.pos))
                continue;

            if (!base_monster->is_habitable(temp.pos))
                temp.path_distance = DISCONNECT_DIST;
            else
            {
                actor * act_at = actor_at(temp.pos);
                monster* mons_at = monster_at(temp.pos);

                if (!act_at)
                    temp.path_distance += 1;
                // Can still search through a firewood monster, just at a higher
                // path cost.
                else if (mons_at && mons_is_firewood(mons_at)
                    && !mons_aligned(base_monster, mons_at))
                {
                    temp.path_distance += 10;
                }
                // An actor we can't path through is there
                else
                    temp.path_distance = DISCONNECT_DIST;

            }

            int connect_level = temp.connect_level;
            int base_connect_level = connect_level;

            map<coord_def, set<int> >::iterator probe
                        = connection_constraints->find(temp.pos);


            if (probe != connection_constraints->end())
            {
                int max_val = probe->second.empty() ? INT_MAX : *probe->second.rbegin();

                if (max_val < connect_level)
                    temp.departure = true;

                // If we can still feasibly retract (haven't left connect range)
                if (!temp.departure)
                {
                    if (probe->second.find(connect_level) != probe->second.end())
                    {
                        while (probe->second.find(connect_level + 1) != probe->second.end())
                            connect_level++;
                    }

                    int delta = connect_level - base_connect_level;
                    temp.connect_level = connect_level;
                    if (delta)
                        temp.string_distance -= delta;
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

//            if (temp.string_distance > MAX_KRAKEN_TENTACLE_DIST)
            if (temp.string_distance > max_string_distance)
                temp.path_distance = DISCONNECT_DIST;

            if (temp.path_distance != DISCONNECT_DIST)
                temp.estimate = min_dist(temp.pos);

            expansion.push_back(temp);
        }
    }

};


struct tentacle_connect_constraints
{
    map<coord_def, set<int> > * connection_constraints;

    monster* base_monster;

    tentacle_connect_constraints()
    {
        for (int i=0; i<8; i++)
            connect_idx[i] = i;
    }

    int connect_idx[8];
    void operator()(const position_node & node,
                    vector<position_node> & expansion)
    {
        shuffle_array(connect_idx, 8);

        for (unsigned i=0; i < 8; i++)
        {
            position_node temp;

            temp.pos = node.pos + Compass[connect_idx[i]];

            if (!in_bounds(temp.pos))
                continue;

            map<coord_def, set<int> >::iterator probe
                        = connection_constraints->find(temp.pos);

            if (probe == connection_constraints->end()
                || probe->second.find(node.connect_level) == probe->second.end())
            {
                continue;
            }


            if (!base_monster->is_habitable(temp.pos)
                || actor_at(temp.pos))
            {
                temp.path_distance = DISCONNECT_DIST;
            }
            else
                temp.path_distance = 1 + node.path_distance;



            //temp.estimate = grid_distance(temp.pos, kraken->pos());
            // Don't bother with an estimate, the search is highly constrained
            // so it's not really going to help
            temp.estimate = 0;
            int test_level = node.connect_level;

/*            for (set<int>::iterator j = probe->second.begin();
                 j!= probe->second.end();
                 j++)
            {
                if (*j == (test_level + 1))
                    test_level++;
            }
            */
            while (probe->second.find(test_level + 1) != probe->second.end())
                test_level++;

            int max = probe->second.empty() ? INT_MAX : *(probe->second.rbegin());

//            mprf("start %d, test %d, max %d", temp.connect_level, test_level, max);
            if (test_level < max)
                continue;

            temp.connect_level = test_level;

            expansion.push_back(temp);
        }
    }

};

struct target_position
{
    coord_def target;
    bool operator() (const coord_def & pos)
    {
        return (pos == target);
    }

};

struct target_monster
{
    int target_mindex;

    bool operator() (const coord_def & pos)
    {
        monster* temp = monster_at(pos);
        if (!temp || temp->mindex() != target_mindex)
            return false;
        return true;

    }
};

struct multi_target
{
    vector<coord_def> * targets;

    bool operator() (const coord_def & pos)
    {
        for (unsigned i = 0; i < targets->size(); ++i)
        {
            if (pos == targets->at(i))
                return true;
        }
        return false;
    }


};

// returns pathfinding success/failure
static bool _tentacle_pathfind(monster* tentacle,
                       tentacle_attack_constraints & attack_constraints,
                       coord_def & new_position,
                       vector<coord_def> & target_positions,
                       int total_length)
{
    multi_target foe_check;
    foe_check.targets = &target_positions;

    vector<set<position_node>::iterator > tentacle_path;

    set<position_node> visited;
    visited.clear();

    position_node temp;
    temp.pos = tentacle->pos();

    map<coord_def, set<int> >::iterator probe
        = attack_constraints.connection_constraints->find(temp.pos);
    ASSERT(probe != attack_constraints.connection_constraints->end());
    temp.connect_level = 0;
    while (probe->second.find(temp.connect_level + 1) != probe->second.end())
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


    return path_found;
}

static bool _try_tentacle_connect(const coord_def & new_pos,
                                  const coord_def & base_position,
                                  int tentacle_idx,
                                  int base_idx,
                                  tentacle_connect_constraints & connect_costs,
                                  monster_type connect_type)
{
    // Nothing to do here.
    // Except fix the tentacle end's pointer, idiot.
    if (base_position == new_pos)
    {
        if (tentacle_idx == base_idx)
            menv[tentacle_idx].props["inwards"].get_int() = -1;
        else
            menv[tentacle_idx].props["inwards"].get_int() = base_idx;
        return true;
    }

    int start_level = 0;
    map<coord_def, set<int> >::iterator it
                    = connect_costs.connection_constraints->find(new_pos);

    // This condition should never miss
    if (it != connect_costs.connection_constraints->end())
    {
        while (it->second.find(start_level + 1) != it->second.end())
            start_level++;
    }

    // Find the tentacle -> head path
    target_position current_target;
    current_target.target = base_position;
/*  target_monster current_target;
    current_target.target_mindex = headnum;
*/

    set<position_node> visited;
    vector<set<position_node>::iterator> candidates;

    position_node temp;
    temp.pos = new_pos;
    temp.connect_level = start_level;

    search_astar(temp,
                 current_target, connect_costs,
                 visited, candidates);

    if (candidates.empty())
        return false;

    _establish_connection(tentacle_idx, base_idx,candidates[0], connect_type);

    return true;
}

static void _collect_tentacles(monster* mons,
                               vector<monster_iterator> & tentacles)
{
    monster_type tentacle = mons_tentacle_child_type(mons);
    // TODO: reorder tentacles based on distance to head or something.
    for (monster_iterator mi; mi; ++mi)
    {
        if (int (mi->number) == mons->mindex() && mi->type == tentacle)
            tentacles.push_back(mi);
    }
}

static void _purge_connectors(int tentacle_idx, monster_type mon_type)
{
    for (monster_iterator mi; mi; ++mi)
    {
        if ((int) mi->number == tentacle_idx
            && mi->type == mon_type)
        {
            int hp = menv[mi->mindex()].hit_points;
            if (hp > 0 && hp < menv[tentacle_idx].hit_points)
                menv[tentacle_idx].hit_points = hp;

            monster_die(&env.mons[mi->mindex()],
                    KILL_MISC, NON_MONSTER, true);
        }
    }
}

struct complicated_sight_check
{
    coord_def base_position;
    bool operator()(monster* mons, actor * test)
    {
        return (test->visible_to(mons)
                && cell_see_cell(base_position, test->pos(), LOS_SOLID_SEE));
    }
};

static bool _basic_sight_check(monster* mons, actor * test)
{
    return mons->can_see(test);
}

template<typename T>
static void _collect_foe_positions(monster* mons,
                                   vector<coord_def> & foe_positions,
                                   T & sight_check)
{
    coord_def foe_pos(-1, -1);
    actor * foe = mons->get_foe();
    if (foe && sight_check(mons, foe))
    {
        foe_positions.push_back(mons->get_foe()->pos());
        foe_pos = foe_positions.back();
    }

    for (monster_iterator mi; mi; ++mi)
    {
        monster* test = &menv[mi->mindex()];
        if (!mons_is_firewood(test)
            && !mons_aligned(test, mons)
            && test->pos() != foe_pos
            && sight_check(mons, test))
        {
            foe_positions.push_back(test->pos());
        }
    }
}

// Return value is a retract position for the tentacle or -1, -1 if no
// retract position exists.
//
// move_kraken_tentacle should check retract pos, it could potentially
// give the kraken head's position as a retract pos.
static int _collect_connection_data(monster* start_monster,
               map<coord_def, set<int> > & connection_data,
               coord_def & retract_pos)
{
    int current_count = 0;
    monster* current_mon = start_monster;
    retract_pos.x = -1;
    retract_pos.y = -1;
    bool retract_found = false;

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
            && menv[next_idx].is_child_tentacle_of(start_monster))
        {
            current_mon = &menv[next_idx];
            if (int(current_mon->number) != start_monster->mindex())
                mprf("link information corruption!!! tentacle in chain doesn't match mindex");
            if (!retract_found)
            {
                retract_pos = current_mon->pos();
                retract_found = true;
            }
        }
        else
        {
            current_mon = NULL;
//            mprf("null at count %d", current_count);
        }
        current_count++;
    }


//    mprf("returned count %d", current_count);
    return current_count;
}

void move_solo_tentacle(monster* tentacle)
{
    if (!tentacle || (tentacle->type != MONS_ELDRITCH_TENTACLE
                      && tentacle->type != MONS_SNAPLASHER_VINE))
    {
        return;
    }

    monster_type segment_type = mons_tentacle_child_type(tentacle);

    int compass_idx[8] = {0, 1, 2, 3, 4, 5, 6, 7};

    int tentacle_idx = tentacle->mindex();

    vector<coord_def> foe_positions;

    bool attack_foe = false;
    bool severed = tentacle->has_ench(ENCH_SEVERED);

    coord_def base_position;
    if (!tentacle->props.exists("base_position"))
        tentacle->props["base_position"].get_coord() = tentacle->pos();

    base_position = tentacle->props["base_position"].get_coord();


    if (!severed)
    {
        complicated_sight_check base_sight;
        base_sight.base_position = base_position;
        _collect_foe_positions(tentacle, foe_positions, base_sight);
        attack_foe = !foe_positions.empty();
    }


    coord_def retract_pos;
    map<coord_def, set<int> > connection_data;

    int visited_count = _collect_connection_data(tentacle,
                                                 connection_data,
                                                 retract_pos);

    bool retract_found = retract_pos.x != -1 && retract_pos.y != -1;

    _purge_connectors(tentacle->mindex(), segment_type);

    if (severed)
    {
        shuffle_array(compass_idx, 8);
        for (unsigned i = 0; i < 8; ++i)
        {
            coord_def new_base = base_position + Compass[compass_idx[i]];
            if (!actor_at(new_base)
                && tentacle->is_habitable(new_base))
            {
                tentacle->props["base_position"].get_coord() = new_base;
                base_position = new_base;
                break;
            }
        }
    }

    coord_def new_pos = tentacle->pos();
    coord_def old_pos = tentacle->pos();

    int demonic_max_dist = (tentacle->type == MONS_ELDRITCH_TENTACLE ? 5 : 8);
    tentacle_attack_constraints attack_constraints;
    attack_constraints.base_monster = tentacle;
    attack_constraints.max_string_distance = demonic_max_dist;
    attack_constraints.connection_constraints = &connection_data;
    attack_constraints.target_positions = &foe_positions;

    bool path_found = false;
    if (attack_foe)
    {
        path_found = _tentacle_pathfind(tentacle, attack_constraints,
                                        new_pos, foe_positions, visited_count);
    }

    //If this tentacle is constricting a creature, attempt to pull it back
    //towards the head.
    bool pull_constrictee = false;
    bool shift_constrictee = false;
    coord_def shift_pos;
    actor* constrictee = NULL;
    if (tentacle->is_constricting())
    {
        actor::constricting_t::const_iterator it = tentacle->constricting->begin();
        constrictee = actor_by_mid(it->first);

        if (retract_found)
        {
            if (constrictee->is_habitable(old_pos))
            {
                pull_constrictee = true;
                shift_pos = old_pos;
            }
        }
        else if (tentacle->type == MONS_SNAPLASHER_VINE)
        {
            // Don't shift our victim if they're already next to a tree
            // (To avoid shaking players back and forth constantly)
            bool near_tree = false;
            for (adjacent_iterator ai(constrictee->pos()); ai; ++ai)
            {
                if (feat_is_tree(grd(*ai)))
                {
                    near_tree = true;
                    break;
                }
            }

            if (!near_tree)
            {
                for (adjacent_iterator ai(tentacle->pos()); ai; ++ai)
                {
                    if (adjacent(*ai, constrictee->pos())
                        && constrictee->is_habitable(*ai)
                        && !actor_at(*ai))
                    {
                        for (adjacent_iterator ai2(*ai); ai2; ++ai2)
                        {
                            if (feat_is_tree(grd(*ai2)))
                            {
                                pull_constrictee = true;
                                shift_constrictee = true;
                                shift_pos = *ai;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if (!attack_foe || !path_found)
    {
        // todo: set a random position?

        dprf("pathing failed, target %d %d", new_pos.x, new_pos.y);
        shuffle_array(compass_idx, 8);
        for (int i=0; i < 8; ++i)
        {
            coord_def test = old_pos + Compass[compass_idx[i]];
            if (!in_bounds(test) || is_sanctuary(test)
                || actor_at(test))
            {
                continue;
            }

            int escalated = 0;
            map<coord_def, set<int> >::iterator probe = connection_data.find(test);

            while (probe->second.find(escalated + 1) != probe->second.end())
                escalated++;


            if (!severed
                && tentacle->is_habitable(test)
                && escalated == *probe->second.rbegin()
                && (visited_count < demonic_max_dist
                    || connection_data.find(test)->second.size() > 1))
            {
                new_pos = test;
                break;
            }
            else if (tentacle->is_habitable(test)
                     && visited_count > 1
                     && escalated == *probe->second.rbegin()
                     && connection_data.find(test)->second.size() > 1)
            {
                new_pos = test;
                break;
            }
        }
    }
    else if (pull_constrictee && !shift_constrictee)
        new_pos = retract_pos;

    if (new_pos != old_pos)
    {
        // Did we path into a target?
        actor * blocking_actor = actor_at(new_pos);
        if (blocking_actor)
        {
            tentacle->target = new_pos;
            monster* mtemp = monster_at(new_pos);
            if (mtemp)
                tentacle->foe = mtemp->mindex();
            else if (new_pos == you.pos())
                tentacle->foe = MHITYOU;

            new_pos = old_pos;
        }
    }

    mgrd(tentacle->pos()) = NON_MONSTER;

    // Why do I have to do this move? I don't get it.
    // specifically, if tentacle isn't registered at its new position on mgrd
    // the search fails (sometimes), Don't know why. -cao
    tentacle->set_position(new_pos);
    mgrd(tentacle->pos()) = tentacle->mindex();

    if (pull_constrictee)
    {
        if (you.can_see(tentacle))
        {
            mprf("The vine drags %s backwards!",
                    constrictee->name(DESC_THE).c_str());
        }

        if (constrictee->as_player())
            move_player_to_grid(shift_pos, false, true);
        else
            constrictee->move_to_pos(shift_pos);

        // Interrupt stair travel and passwall.
        if (constrictee->is_player())
            stop_delay(true);
    }
    tentacle->clear_far_constrictions();

    tentacle_connect_constraints connect_costs;
    connect_costs.connection_constraints = &connection_data;
    connect_costs.base_monster = tentacle;

    bool connected = _try_tentacle_connect(new_pos, base_position,
                                           tentacle_idx, tentacle_idx,
                                           connect_costs,
                                           segment_type);

    if (!connected)
    {
        // This should really never fail for demonic tentacles (they don't
        // have the whole shifting base problem). -cao
        mprf("tentacle connect failed! What the heck!  severed status %d",
             tentacle->has_ench(ENCH_SEVERED));
        mprf("pathed to %d %d from %d %d mid %d count %d", new_pos.x, new_pos.y,
             old_pos.x, old_pos.y, tentacle->mindex(), visited_count);

//      mgrd(tentacle->pos()) = tentacle->mindex();

        // Is it ok to purge the tentacle here?
        monster_die(tentacle, KILL_MISC, NON_MONSTER, true);
        return;
    }

//  mprf("mindex %d vsisted %d", tentacle_idx, visited_count);
    tentacle->check_redraw(old_pos);
    tentacle->apply_location_effects(old_pos);
}

void move_child_tentacles(monster* mons)
{
    if (!mons_is_tentacle_head(mons_base_type(mons))
        || mons->asleep())
    {
        return;
    }

    bool no_foe = false;

    vector<coord_def> foe_positions;
    _collect_foe_positions(mons, foe_positions, _basic_sight_check);

    //if (!kraken->near_foe())
    if (foe_positions.empty()
        || mons->behaviour == BEH_FLEE
        || mons->behaviour == BEH_WANDER)
    {
        no_foe = true;
    }
    vector<monster_iterator> tentacles;
    _collect_tentacles(mons, tentacles);

    // Move each tentacle in turn
    for (unsigned i = 0; i < tentacles.size(); i++)
    {
        monster* tentacle = monster_at(tentacles[i]->pos());

        if (!tentacle)
        {
            dprf("Missing tentacle in path.");
            continue;
        }

        tentacle_connect_constraints connect_costs;
        map<coord_def, set<int> > connection_data;

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
                && (menv[next_idx].is_child_tentacle_of(tentacle)
                    || menv[next_idx].is_parent_monster_of(tentacle)))
            {
                current_mon = &menv[next_idx];
                if (!retract_found
                    && current_mon->is_child_tentacle_of(tentacle))
                {
                    retract_pos = current_mon->pos();
                    retract_found = true;
                }
            }
            else
                current_mon = NULL;
            current_count++;
        }

        int tentacle_idx = tentacle->mindex();

        _purge_connectors(tentacle_idx, mons_tentacle_child_type(tentacle));

        if (no_foe
            && grid_distance(tentacle->pos(), mons->pos()) == 1)
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

        tentacle_attack_constraints attack_constraints;
        attack_constraints.base_monster = tentacle;
        attack_constraints.max_string_distance = MAX_KRAKEN_TENTACLE_DIST;
        attack_constraints.connection_constraints = &connection_data;
        attack_constraints.target_positions = &foe_positions;

        //If this tentacle is constricting a creature, attempt to pull it back
        //towards the head.
        bool pull_constrictee = false;
        actor* constrictee = NULL;
        if (tentacle->is_constricting() && retract_found)
        {
            actor::constricting_t::const_iterator it = tentacle->constricting->begin();
            constrictee = actor_by_mid(it->first);
            if (grd(old_pos) >= DNGN_SHALLOW_WATER
                && constrictee->is_habitable(old_pos))
            {
                pull_constrictee = true;
            }
        }

        if (!no_foe && !pull_constrictee)
        {
            path_found = _tentacle_pathfind(
                    tentacle,
                    attack_constraints,
                    new_pos,
                    foe_positions,
                    current_count);
        }

        if (no_foe || !path_found || pull_constrictee)
        {
            if (retract_found)
                new_pos = retract_pos;
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
                tentacle->foe = mtemp->mindex();
            else if (new_pos == you.pos())
                tentacle->foe = MHITYOU;

            new_pos = old_pos;
        }

        mgrd(tentacle->pos()) = NON_MONSTER;

        // Why do I have to do this move? I don't get it.
        // specifically, if tentacle isn't registered at its new position on
        // mgrd the search fails (sometimes), Don't know why. -cao
        tentacle->set_position(new_pos);
        mgrd(tentacle->pos()) = tentacle->mindex();

        if (pull_constrictee)
        {
            if (you.can_see(tentacle))
            {
                mprf("The tentacle pulls %s backwards!",
                     constrictee->name(DESC_THE).c_str());
            }

            if (constrictee->as_player())
                move_player_to_grid(old_pos, false, true);
            else
                constrictee->move_to_pos(old_pos);

            // Interrupt stair travel and passwall.
            if (constrictee->is_player())
                stop_delay(true);
        }
        tentacle->clear_far_constrictions();

        connect_costs.connection_constraints = &connection_data;
        connect_costs.base_monster = tentacle;
        bool connected = _try_tentacle_connect(new_pos, mons->pos(),
                                tentacle_idx, mons->mindex(),
                                connect_costs,
                                mons_tentacle_child_type(tentacle));

        // Can't connect, usually the head moved and invalidated our position
        // in some way. Should look into this more at some point -cao
        if (!connected)
        {
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
                                  // Pick a random chimera component
                                  : (mons->type == MONS_CHIMERA ?
                                     get_chimera_part(mons, random2(3) + 1)
                                     : mons->type);

    // Slime creatures can split while out of sight.
    if ((!mons->near_foe() || mons->asleep() || mons->submerged())
         && mons->type != MONS_SLIME_CREATURE
         && mons->type != MONS_LOST_SOUL
         && !_crawlie_is_mergeable(mons))
    {
        return false;
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
        // to others of its kind, or to other monsters glowing with
        // radiation, can mutate it into a different (very) ugly thing.
        used = ugly_thing_mutate(mons, true);
        break;

    case MONS_SLIME_CREATURE:
        // Slime creatures may split or merge depending on the
        // situation.
        used = _slime_split_merge(mons);
        if (!mons->alive())
            return true;
        break;

    case MONS_CRAWLING_CORPSE:
    case MONS_MACABRE_MASS:
        used = _crawling_corpse_merge(mons);
        if (!mons->alive())
            return true;
        break;

    case MONS_ORC_KNIGHT:
    case MONS_ORC_WARLORD:
    case MONS_SAINT_ROKA:
        if (is_sanctuary(mons->pos()))
            break;

        _battle_cry(mons, BATTLECRY_ORC);
        // Doesn't cost a turn.
        break;

    case MONS_CHERUB:
        _battle_cry(mons, BATTLECRY_CHERUB_HYMN);
        break;

    case MONS_SATYR:
        if (one_chance_in(4))
        {
            if (_battle_cry(mons, BATTLECRY_SATYR_PIPES))
                used = true;
        }
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
            && distance2(you.pos(), mons->pos()) <= 5)
        {
            mons->suicide();
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
                mons->suicide();
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
    case MONS_LIGHTNING_SPIRE:
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
        beem.damage      = dice_def(3, 6);
        beem.hit         = 35;
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
    case MONS_OKLOB_SAPLING:
    case MONS_YELLOW_DRACONIAN:
    {
        if (mons->has_ench(ENCH_CONFUSION))
            break;

        if (player_or_mon_in_sanct(mons))
            break;

        bool spit = one_chance_in(3);
        if (mons->type == MONS_OKLOB_PLANT)
        {
            // reduced chance in zotdef
            spit = x_chance_in_y(mons->hit_dice,
                crawl_state.game_is_zotdef() ? 40 : 30);
        }
        if (mons->type == MONS_OKLOB_SAPLING)
            spit = one_chance_in(4);

        if (spit)
        {
            spell = SPELL_SPIT_ACID;
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

    case MONS_BURNING_BUSH:
    {
        if (mons->has_ench(ENCH_CONFUSION))
            break;

        if (player_or_mon_in_sanct(mons))
            break;

        if (one_chance_in(3))
        {
            spell = SPELL_THROW_FLAME;
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

    case MONS_POLYMOTH:
        if (one_chance_in(3))
            used = _moth_polymorph(mons);
        break;

    case MONS_QUEEN_BEE:
        if (one_chance_in(4)
            || mons->hit_points < mons->max_hit_points / 3 && one_chance_in(2))
        {
            used = _queen_incite_worker(mons);
        }
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

    case MONS_CRIMSON_IMP:
    case MONS_PHANTOM:
    case MONS_INSUBSTANTIAL_WISP:
    case MONS_BLINK_FROG:
    case MONS_KILLER_KLOWN:
    case MONS_PRINCE_RIBBIT:
    case MONS_MARA:
    case MONS_MARA_FAKE:
    case MONS_GOLDEN_EYE:
        if (mons->no_tele(true, false))
            break;
        if (one_chance_in(7) || mons->caught() && one_chance_in(3))
            used = monster_blink(mons);
        break;

    case MONS_PHANTASMAL_WARRIOR:
    {
        actor *foe = mons->get_foe();
        if (mons->no_tele(true, false))
            break;
        if (foe && foe->pos().distance_from(mons->pos()) > 2 && one_chance_in(3))
        {
            blink_close(mons);
            used = true;
        }
        break;
    }

    case MONS_SKY_BEAST:
        if (one_chance_in(8))
        {
            // If we're invisible, become visible.
            if (mons->invisible())
            {
                mons->del_ench(ENCH_INVIS);
                place_cloud(CLOUD_RAIN, mons->pos(), 2, mons);
            }
            // Otherwise, go invisible.
            else
                enchant_monster_invisible(mons, "flickers out of sight");
        }
        break;

    case MONS_BOG_BODY:
        if (one_chance_in(8))
        {
            // A hacky way of making these rot regularly.
            if (mons->has_ench(ENCH_ROT))
                break;

            mon_enchant rot = mon_enchant(ENCH_ROT, 0, 0, 10);
            mons->add_ench(rot);

            if (mons->visible_to(&you))
                simple_monster_message(mons, " begins to rapidly decay!");
        }
        break;

    case MONS_AGATE_SNAIL:
    case MONS_SNAPPING_TURTLE:
    case MONS_ALLIGATOR_SNAPPING_TURTLE:
        // Use the same calculations as for low-HP casting
        if (mons->hit_points < mons->max_hit_points / 4 && !one_chance_in(4)
            && !mons->has_ench(ENCH_WITHDRAWN))
        {
            mons->add_ench(ENCH_WITHDRAWN);

            if (mons_is_retreating(mons))
                behaviour_event(mons, ME_CORNERED);

            simple_monster_message(mons, " withdraws into its shell!");
        }
        break;

    case MONS_BOULDER_BEETLE:
        if (mons->has_ench(ENCH_CONFUSION))
            break;

        if (!mons->has_ench(ENCH_ROLLING))
        {
            // Fleeing check
            if (mons_is_fleeing(mons))
            {
                if (coinflip())
                {
                //  behaviour_event(mons, ME_CORNERED);
                    simple_monster_message(mons, " curls into a ball and rolls away!");
                    boulder_start(mons, &beem);
                }
            }
            // Normal check - don't roll at adjacent targets
            else if (one_chance_in(3) &&
                     !adjacent(mons->pos(), beem.target))
            {
                simple_monster_message(mons, " curls into a ball and starts rolling!");
                boulder_start(mons, &beem);
            }
        }
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
        beem.damage      = dice_def(2, 10);
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

    case MONS_APOCALYPSE_CRAB:
        if (spell == SPELL_NO_SPELL)
            spell = SPELL_CHAOS_BREATH;
    // Intentional fallthrough

    case MONS_CHAOS_BUTTERFLY:
        if (spell == SPELL_NO_SPELL)
        {
            if (!mons->props.exists("twister_time")
                || you.elapsed_time - (int)mons->props["twister_time"] > 200)
            {
                spell = SPELL_SUMMON_TWISTER;
            }
            else
                break;
        }
    // Intentional fallthrough

    // Dragon breath weapons:
    case MONS_DRAGON:
    case MONS_HELL_HOUND:
    case MONS_LINDWURM:
    case MONS_FIRE_DRAKE:
    case MONS_XTAHUA:
    case MONS_FIRE_CRAB:
        if (spell == SPELL_NO_SPELL)
            spell = SPELL_FIRE_BREATH;

        if (mons->has_ench(ENCH_CONFUSION))
            break;

        if (!you.visible_to(mons))
            break;

        if ((mons_genus(mons->type) == MONS_DRAGON
            || mons_genus(mons->type) == MONS_DRACONIAN)
                && mons->has_ench(ENCH_BREATH_WEAPON))
        {
            break;
        }

        if (mons->type == MONS_HELL_HOUND || mons->type == MONS_CHAOS_BUTTERFLY
                ? one_chance_in(10)
                : x_chance_in_y(4, 13))
        {
            setup_mons_cast(mons, beem, spell);

            if (mons->type == MONS_FIRE_CRAB)
            {
                beem.is_big_cloud = true;
                beem.damage       = dice_def(1, (mons->hit_dice*3)/2);
            }

            if (mons->type == MONS_APOCALYPSE_CRAB)
            {
                beem.is_big_cloud = true;
                beem.damage       = dice_def(1, (mons->hit_dice*3)/2);
            }

            // Fire tracer.
            bool spellOK = true;

            if (spell_needs_tracer(spell))
            {
                fire_tracer(mons, beem);
                spellOK = mons_should_fire(beem);
            }

            // Good idea?
            if (spellOK)
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
                && (you.species == SP_MERFOLK
                    || you.check_res_magic(100) > 0
                    || you.clarity()))
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

    case MONS_WRETCHED_STAR:
        if (is_sanctuary(mons->pos()))
            break;

        if (one_chance_in(5))
        {
            if (cell_see_cell(you.pos(), mons->pos(), LOS_DEFAULT))
            {
                targetter_los hitfunc(mons, LOS_SOLID);
                flash_view_delay(MAGENTA, 300, &hitfunc);
                simple_monster_message(mons, " pulses with an eldritch light!");

                if (!is_sanctuary(you.pos())
                        && cell_see_cell(you.pos(), mons->pos(), LOS_SOLID))
                {
                    int num_mutations = 2 + random2(3);
                    for (int i = 0; i < num_mutations; ++i)
                        temp_mutate(RANDOM_BAD_MUTATION, "wretched star");
                }
            }

            for (radius_iterator ri(mons->pos(), LOS_RADIUS, C_ROUND); ri; ++ri)
            {
                monster *m = monster_at(*ri);
                if (m && cell_see_cell(mons->pos(), *ri, LOS_SOLID_SEE)
                    && m->can_mutate())
                {
                    switch (m->type)
                    {
                    // colour/attack/resistances change
                    case MONS_UGLY_THING:
                    case MONS_VERY_UGLY_THING:
                        m->malmutate("wretched star");
                        break;

                    // tile change, already mutated wrecks
                    case MONS_ABOMINATION_SMALL:
                    case MONS_ABOMINATION_LARGE:
                        m->props["tile_num"].get_short() = random2(256);
                    case MONS_WRETCHED_STAR:
                    case MONS_CHAOS_SPAWN:
                    case MONS_PULSATING_LUMP:
                        break;

                    default:
                        m->add_ench(mon_enchant(ENCH_WRETCHED, 1));
                    }
                }
            }

            used = true;
        }
        break;

    case MONS_STARCURSED_MASS:
        if (x_chance_in_y(mons->number,8) && x_chance_in_y(2,3)
            && mons->hit_points >= 8)
        {
            _starcursed_split(mons), used = true;
        }

        if (!mons_is_confused(mons)
                && !is_sanctuary(mons->pos()) && !is_sanctuary(beem.target)
                && _will_starcursed_scream(mons)
                && coinflip())
        {
            _starcursed_scream(mons, actor_at(beem.target));
            used = true;
        }
        break;


    case MONS_VAULT_SENTINEL:
        if (mons->has_ench(ENCH_CONFUSION) || mons->friendly())
            break;

        if (!silenced(mons->pos()) && !mons->has_ench(ENCH_BREATH_WEAPON)
                && one_chance_in(4))
        {
            if (you.can_see(mons))
            {
                simple_monster_message(mons, " blows on a signal horn!");
                noisy(25, mons->pos());
            }
            else
                noisy(25, mons->pos(), "You hear a note blown loudly on a horn!");

            // This is probably coopting the enchant for something beyond its
            // intended purpose, but the message does match....
            mon_enchant breath_timeout =
                mon_enchant(ENCH_BREATH_WEAPON, 1, mons, (4 +  random2(9)) * 10);
            mons->add_ench(breath_timeout);
            used = true;
        }
        break;

    case MONS_VAULT_WARDEN:
        if (mons->has_ench(ENCH_CONFUSION) || mons->attitude != ATT_HOSTILE)
            break;

        if (!mons->can_see(&you))
            break;

        if (one_chance_in(4))
        {
            if (_seal_doors_and_stairs(mons))
                used = true;
        }
        break;

    case MONS_FLAYED_GHOST:
    {
        if (mons->has_ench(ENCH_CONFUSION))
            break;

        actor *foe = mons->get_foe();
        if (foe && mons->can_see(foe) && one_chance_in(5))
        {
            if (_flay_creature(mons, foe))
                used = true;
        }

        break;
    }

    case MONS_LOST_SOUL:
    if (one_chance_in(3))
    {
        bool see_friend = false;
        bool see_foe = false;

        for (actor_iterator ai(mons->get_los_no_trans()); ai; ++ai)
        {
            if (ai->is_monster() && mons_aligned(*ai, mons))
            {
                if (_lost_soul_affectable(ai->as_monster()))
                    see_friend = true;
            }
            else
                see_foe = true;

            if (see_friend)
                break;
        }

        if (see_foe && !see_friend)
            if (_lost_soul_teleport(mons))
                used = true;
    }
    break;

    case MONS_BLUE_DEVIL:
        if (mons->confused() || !mons->can_see(mons->get_foe()))
            break;

        if (mons->foe_distance() < 5 && mons->foe_distance() > 1)
        {
            if (one_chance_in(4))
            {
                if (mons->props.exists("swoop_cooldown")
                    && (you.elapsed_time < mons->props["swoop_cooldown"].get_int()))
                {
                    break;
                }

                if (_swoop_attack(mons, mons->get_foe()))
                {
                    used = true;
                    mons->props["swoop_cooldown"].get_int() = you.elapsed_time
                                                              + 40 + random2(51);
                }
            }
        }
        break;

    case MONS_RED_DEVIL:
        if (mons->confused() || !mons->can_see(mons->get_foe()))
            break;

        if (mons->foe_distance() == 1 && mons->reach_range() == REACH_TWO
            && x_chance_in_y(3, 5))
        {
            coord_def foepos = mons->get_foe()->pos();
            coord_def hopspot = mons->pos() - (foepos - mons->pos()).sgn();

            bool found = false;
            if (!monster_habitable_grid(mons, grd(hopspot)) ||
                actor_at(hopspot))
            {
                for (adjacent_iterator ai(mons->pos()); ai; ++ai)
                {
                    if (ai->distance_from(foepos) != 2)
                        continue;
                    else
                    {
                        if (monster_habitable_grid(mons, grd(*ai))
                            && !actor_at(*ai))
                        {
                            hopspot = *ai;
                            found = true;
                            break;
                        }
                    }
                }
            }
            else
                found = true;

            if (found && mons->move_to_pos(hopspot))
            {
                simple_monster_message(mons, " hops backward while attacking.");
                fight_melee(mons, mons->get_foe());
                mons->speed_increment -= 2; // Add a small extra delay
                return true; // Energy has already been deducted via melee
            }
        }
        break;

    case MONS_SHADOW:
        if (!mons->invisible() && !mons->backlit() && one_chance_in(6))
        {
            if (enchant_monster_invisible(mons, "slips into darkness"))
            {
                mon_enchant invis = mons->get_ench(ENCH_INVIS);
                invis.duration = 40 + random2(60);
                mons->update_ench(invis);
                used = true;
            }
        }
        break;

    case MONS_WIND_DRAKE:
    {
        if (mons->has_ench(ENCH_BREATH_WEAPON))
            break;

        actor *foe = mons->get_foe();
        if (foe && mons->can_see(foe) && one_chance_in(4))
        {
            simple_monster_message(mons, " exhales a fierce blast of wind!");
            wind_blast(mons, 12 * mons->hit_dice, foe->pos());
            mon_enchant breath_timeout =
                mon_enchant(ENCH_BREATH_WEAPON, 1, mons,
                            (4 + random2(9)) * 10);
            mons->add_ench(breath_timeout);
            used = true;
        }
        break;
    }

    case MONS_THORN_HUNTER:
    {
        // If we would try to move into a briar (that we might have just created
        // defensively), let's see if we can shoot our foe through it instead
        if (actor_at(mons->pos() + mons->props["mmov"].get_coord())
            && actor_at(mons->pos() + mons->props["mmov"].get_coord())->type == MONS_BRIAR_PATCH
            && !one_chance_in(3))
        {
            actor *foe = mons->get_foe();
            if (foe && mons->can_see(foe))
            {
                beem.target = foe->pos();
                setup_mons_cast(mons, beem, SPELL_THORN_VOLLEY);

                fire_tracer(mons, beem);
                if (mons_should_fire(beem))
                {
                    make_mons_stop_fleeing(mons);
                    _mons_cast_abil(mons, beem, SPELL_THORN_VOLLEY);
                    used = true;
                }
            }
        }
        // Otherwise, if our foe is approaching us, we might want to raise a
        // defensive wall of brambles (use the number of brambles in the area
        // as some indication if we've already done this, and shouldn't repeat)
        else if (mons->props["foe_approaching"].get_bool() == true
                 && coinflip())
        {
            int briar_count = 0;
            c = circle_def(mons->pos(), 4, C_ROUND);
            for (monster_iterator mi(&c); mi; ++mi)
            {
                if (mi->type == MONS_BRIAR_PATCH)
                    briar_count++;
            }
            if (briar_count < 4) // Probably no solid wall here
            {
                _mons_cast_abil(mons, beem, SPELL_WALL_OF_BRAMBLES);
                used = true;
            }
        }
    }
    break;

    case MONS_SPIRIT_WOLF:
    {
        if (!you.duration[DUR_SPIRIT_HOWL] && !mons->is_summoned() && !mons->wont_attack()
            && (one_chance_in(35)
                || (mons->hit_points < mons->max_hit_points / 3 && one_chance_in(7))))
        {
            // Take a little breather between howl effects
            if (you.props.exists("spirit_howl_cooldown")
                && you.props["spirit_howl_cooldown"].get_int() > you.elapsed_time)
            {
                break;
            }

            // Set total number of wolves in the pack, if we haven't already
            if (!you.props.exists("spirit_wolf_total"))
                you.props["spirit_wolf_total"].get_int() = random_range(12, 20);

            // Too few wolves to bother summoning
            if (you.props["spirit_wolf_total"].get_int() < 3)
                break;

            simple_monster_message(mons, " howls for its pack!", MSGCH_MONSTER_SPELL);
            you.duration[DUR_SPIRIT_HOWL] = 450 + random2(350);
            spawn_spirit_pack(&you);
            you.props["next_spirit_pack"].get_int() = you.elapsed_time + 40 + random2(85);
            used = true;
        }
    }
    break;

    case MONS_WATER_NYMPH:
    {
        if (one_chance_in(5))
        {
            actor *foe = mons->get_foe();
            if (foe && !feat_is_water(grd(foe->pos())))
            {
                coord_def spot;
                find_habitable_spot_near(foe->pos(), MONS_BIG_FISH, 3, false, spot);
                if (!spot.origin() && foe->pos().distance_from(spot)
                                      < foe->pos().distance_from(mons->pos()))
                {
                    if (mons->move_to_pos(spot))
                    {
                        simple_monster_message(mons, " flows with the water.");
                        used = true;
                    }
                }
            }
        }
    }
    break;

    case MONS_TREANT:
    {
        if (mons->hit_points * 2 < mons->max_hit_points && mons->number > 0)
        {
            treant_release_wasps(mons);
            // Intentionally takes no energy; the insects are flying free
            // on their own time.
        }

        if (!mons->has_ench(ENCH_GRASPING_ROOTS_SOURCE) && x_chance_in_y(2, 3))
        {
            // Duration does not decay while there are hostiles around
            mons->add_ench(mon_enchant(ENCH_GRASPING_ROOTS_SOURCE, 1, mons,
                                       random_range(3, 7) * BASELINE_DELAY));
            if (you.can_see(mons))
            {
                mprf(MSGCH_MONSTER_SPELL, "%s reaches out with a gnarled limb.",
                     mons->name(DESC_THE).c_str());
                mprf("Grasping roots rise from the ground around %s!",
                     mons->name(DESC_THE).c_str());
            }
            else if (you.see_cell(mons->pos()))
                mpr("Grasping roots begin to rise from the ground!");

            used = true;
        }
    }
    break;

    case MONS_THORN_LOTUS:
        if (x_chance_in_y(2, 5))
        {
            setup_mons_cast(mons, beem, SPELL_THORN_VOLLEY);

            fire_tracer(mons, beem);
            if (mons_should_fire(beem))
            {
                make_mons_stop_fleeing(mons);
                _mons_cast_abil(mons, beem, SPELL_THORN_VOLLEY);
                used = true;
            }
        }
        break;

    case MONS_FIREFLY:
        if (one_chance_in(7) && !mons->friendly() && !mons->confused())
        {
            // In the odd case we're invisible somehow
            mons->del_ench(ENCH_INVIS);
            simple_monster_message(mons, " flashes a warning beacon!",
                                   MSGCH_MONSTER_SPELL);

            circle_def range(mons->pos(), 13, C_ROUND);
            for (monster_iterator mi(&range); mi; ++mi)
            {
                // Fireflies (and their riders) are attuned enough to the light
                // to wake in response, while other creatures sleep through it
                if (mi->asleep() && (mi->type == MONS_FIREFLY
                                     || mi->type == MONS_SPRIGGAN_RIDER))
                {
                    behaviour_event(*mi, ME_DISTURB, mons->get_foe(), mons->pos());

                    // Give riders a chance to shout and alert their companions
                    // (this won't otherwise happen if the player is still out
                    // of their sight)
                    if (mi->behaviour == BEH_WANDER && coinflip())
                        handle_monster_shouts(*mi);
                }

                if (!mi->asleep())
                {
                    // We consider the firefly's foe to be the 'source' of the
                    // light here so that monsters wandering around for the player
                    // will be alerted by fireflies signaling in response to the
                    // player, but ignore signals in response to other monsters
                    // if they're already in pursuit of the player.
                    behaviour_event(*mi, ME_ALERT, mons->get_foe(), mons->pos());
                    if (mi->target == mons->pos())
                    {
                        monster_pathfind mp;
                        if (mp.init_pathfind(*mi, mons->pos()))
                        {
                            mi->travel_path = mp.calc_waypoints();
                            if (!mi->travel_path.empty())
                            {
                                mi->target = mi->travel_path[0];
                                mi->travel_target = MTRAV_PATROL;
                            }
                        }
                    }
                }
            }

            if (!you.see_cell(mons->pos()) && range.contains(you.pos()))
                mpr("You see a distant pulse of light.");
        }
        break;

    default:
        break;
    }

    if (used)
        mons->lose_energy(EUT_SPECIAL);

    // XXX: Unless monster dragons get abilities that are not a breath
    // weapon...
    if (used)
    {
        if (mons_genus(mons->type) == MONS_DRAGON
            || mons_genus(mons->type) == MONS_DRACONIAN)
        {
            setup_breath_timeout(mons);
        }
        else if (mons->type == MONS_CHAOS_BUTTERFLY)
            mons->props["twister_time"].get_int() = you.elapsed_time;
    }

    return used;
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
        || mons->submerged()
        || !summon_can_attack(mons, foe))
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
        const monsterentry* entry = get_monster_data(mons->type);

        mons->add_ench(ENCH_SUBMERGED);
        mons->speed_increment -= ENERGY_SUBMERGE(entry);
        return;
    }

    const monster_type mclass = mons->type == MONS_CHIMERA ?
                                    get_chimera_part(mons, random2(3) + 1)
                                    : mons->type;

    switch (mclass)
    {
    case MONS_PANDEMONIUM_LORD:
        if (!mons->ghost->cycle_colours)
            break;
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
                     mons->name(DESC_THE).c_str(),
                     foe->name(DESC_THE).c_str());

            int confuse_power = 2 + random2(3);

            if (foe->is_player() && !can_see)
            {
                canned_msg(MSG_BEING_WATCHED);
                interrupt_activity(AI_MONSTER_ATTACKS, mons);
            }

            int res_margin = foe->check_res_magic((mons->hit_dice * 5)
                             * confuse_power);
            if (res_margin > 0)
            {
                if (foe->is_player())
                    canned_msg(MSG_YOU_RESIST);
                else if (foe->is_monster())
                {
                    const monster* foe_mons = foe->as_monster();
                    simple_monster_message(foe_mons,
                           mons_resist_string(foe_mons, res_margin));
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
                     mons->name(DESC_THE).c_str(),
                     foe->name(DESC_THE).c_str());

            if (foe->is_player() && !can_see)
                canned_msg(MSG_BEING_WATCHED);

            // Subtly different from old paralysis behaviour, but
            // it'll do.
            foe->paralyse(mons, 2 + random2(3));
        }
        break;

    case MONS_EYE_OF_DRAINING:
    case MONS_GHOST_MOTH:
        if (_eyeball_will_use_ability(mons) && foe->is_player())
        {
            if (you.can_see(mons))
                simple_monster_message(mons, " stares at you.");
            else
                canned_msg(MSG_BEING_WATCHED);

            interrupt_activity(AI_MONSTER_ATTACKS, mons);

            int mp = 5 + random2avg(13, 3);
            if (you.species != SP_DJINNI)
                mp = min(mp, you.magic_points);
            else
            {
                // Cap draining somehow, otherwise a ghost moth will heal
                // itself every turn.  Other races don't have such a problem
                // because they drop to 0 mp nearly immediately.
                mp = mp * (you.hp_max - you.duration[DUR_ANTIMAGIC] / 3)
                        / you.hp_max;
            }
            drain_mp(mp);

            mons->heal(mp, true); // heh heh {dlb}
        }
        break;

    case MONS_AIR_ELEMENTAL:
        if (one_chance_in(5))
            mons->add_ench(ENCH_SUBMERGED);
        break;

    default:
        break;
    }
}

// When giant spores move maybe place a ballistomycete on the they move
// off of.
void ballisto_on_move(monster* mons, const coord_def& position)
{
    if (mons->type == MONS_GIANT_SPORE && !crawl_state.game_is_zotdef()
        && !mons->is_summoned())
    {
        dungeon_feature_type ftype = env.grid(mons->pos());

        if (ftype == DNGN_FLOOR)
            env.pgrid(mons->pos()) |= FPROP_MOLD;

        // The number field is used as a cooldown timer for this behavior.
        if (mons->number <= 0)
        {
            if (one_chance_in(4))
            {
                beh_type attitude = actual_same_attitude(*mons);
                if (monster *plant = create_monster(mgen_data(MONS_BALLISTOMYCETE,
                                                        attitude,
                                                        NULL,
                                                        0,
                                                        0,
                                                        position,
                                                        MHITNOT,
                                                        MG_FORCE_PLACE)))
                {
                    if (mons_is_god_gift(mons, GOD_FEDHAS))
                    {
                        plant->flags |= MF_NO_REWARD;

                        if (attitude == BEH_FRIENDLY)
                        {
                            plant->flags |= MF_ATT_CHANGE_ATTEMPT;

                            mons_make_god_gift(plant, GOD_FEDHAS);
                        }
                    }

                    // Don't leave mold on squares we place ballistos on
                    remove_mold(position);
                    if (you.can_see(plant))
                        mprf("A ballistomycete grows in the wake of the spore.");
                }

                mons->number = 40;
            }
        }
        else
            mons->number--;
    }
}

static bool _ballisto_at(const coord_def & target)
{
    monster* mons = monster_at(target);
    return (mons && mons->type == MONS_BALLISTOMYCETE
            && mons->alive());
}

static bool _player_at(const coord_def & target)
{
    return (you.pos() == target);
}

static bool _mold_connected(const coord_def & target)
{
    return (is_moldy(target) || _ballisto_at(target));
}


// If 'monster' is a ballistomycete or spore, activate some number of
// ballistomycetes on the level.
void activate_ballistomycetes(monster* mons, const coord_def& origin,
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
    bool (*valid_target)(const coord_def &) = _ballisto_at;
    bool (*connecting_square) (const coord_def &) = _mold_connected;

    set<position_node> visited;
    vector<set<position_node>::iterator > candidates;

    if (you_worship(GOD_FEDHAS))
    {
        if (non_activable_count == 0
            && ballisto_count == 0
            && any_friendly
            && mons->type == MONS_BALLISTOMYCETE)
        {
            mpr("Your fungal colony was destroyed.");
            dock_piety(5, 0);
        }

        fedhas_mode = true;
        activation_count = 1;
        exhaustive = false;
        valid_target = _player_at;
    }

    _search_dungeon(origin, valid_target, connecting_square, visited,
                    candidates, exhaustive);

    if (candidates.empty())
    {
        if (!fedhas_mode
            && non_activable_count == 0
            && ballisto_count == 0
            && mons->attitude == ATT_HOSTILE)
        {
            if (player_kill)
            {
                mpr("Having destroyed the fungal colony, you feel a bit more "
                    "experienced.");
                gain_exp(200);
            }

            // Get rid of the mold, so it'll be more useful when new fungi
            // spawn.
            for (rectangle_iterator ri(1); ri; ++ri)
                remove_mold(*ri);
        }

        return;
    }

    // A (very) soft cap on colony growth, no activations if there are
    // already a lot of ballistos on level.
    if (candidates.size() > 25)
        return;

    shuffle_array(candidates);

    int index = 0;

    for (int i = 0; i < activation_count; ++i)
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

        const position_node* thread = &(*candidates[index]);
        while (thread)
        {
            if (!one_chance_in(3))
                env.pgrid(thread->pos) |= FPROP_GLOW_MOLD;

            thread = thread->last;
        }
        env.level_state |= LSTATE_GLOW_MOLD;
    }
}

void ancient_zyme_sicken(monster* mons)
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

static bool _do_merge_masses(monster* initial_mass, monster* merge_to)
{
    // Combine enchantment durations.
    _merge_ench_durations(initial_mass, merge_to);

    merge_to->number += initial_mass->number;
    merge_to->max_hit_points += initial_mass->max_hit_points;
    merge_to->hit_points += initial_mass->hit_points;

    // Merge monster flags (mostly so that MF_CREATED_NEUTRAL, etc. are
    // passed on if the merged slime subsequently splits.  Hopefully
    // this won't do anything weird.
    merge_to->flags |= initial_mass->flags;

    // Overwrite the state of the slime getting merged into, because it
    // might have been resting or something.
    merge_to->behaviour = initial_mass->behaviour;
    merge_to->foe = initial_mass->foe;

    behaviour_event(merge_to, ME_EVAL);

    // Have to 'kill' the slime doing the merging.
    monster_die(initial_mass, KILL_DISMISSED, NON_MONSTER, true);

    return true;
}

void starcursed_merge(monster* mon, bool forced)
{
    //Find a random adjacent starcursed mass
    int compass_idx[] = {0, 1, 2, 3, 4, 5, 6, 7};
    shuffle_array(compass_idx, 8);

    for (int i = 0; i < 8; ++i)
    {
        coord_def target = mon->pos() + Compass[compass_idx[i]];
        monster* mergee = monster_at(target);
        if (mergee && mergee->alive() && mergee->type == MONS_STARCURSED_MASS)
        {
            if (forced)
                simple_monster_message(mon, " shudders and is absorbed by its neighbour.");
            if (_do_merge_masses(mon, mergee))
                return;
        }
    }

    // If there was nothing adjacent to merge with, at least try to move toward
    // another starcursed mass
    for (distance_iterator di(mon->pos(), true, true, 8); di; ++di)
    {
        monster* ally = monster_at(*di);
        if (ally && ally->alive() && ally->type == MONS_STARCURSED_MASS
            && mon->can_see(ally))
        {
            bool moved = false;

            coord_def sgn = (*di - mon->pos()).sgn();
            if (mon_can_move_to_pos(mon, sgn))
            {
                mon->move_to_pos(mon->pos()+sgn, false);
                moved = true;
            }
            else if (abs(sgn.x) != 0)
            {
                coord_def dx(sgn.x, 0);
                if (mon_can_move_to_pos(mon, dx))
                {
                    mon->move_to_pos(mon->pos()+dx, false);
                    moved = true;
                }
            }
            else if (abs(sgn.y) != 0)
            {
                coord_def dy(0, sgn.y);
                if (mon_can_move_to_pos(mon, dy))
                {
                    mon->move_to_pos(mon->pos()+dy, false);
                    moved = true;
                }
            }

            if (moved)
            {
                simple_monster_message(mon, " shudders and withdraws towards its neighbour.");
                mon->speed_increment -= 10;
            }
        }
    }
}

int spawn_spirit_pack(const actor* target)
{
    for (int t = 0; t < 100; ++t)
    {
        coord_def area = clamp_in_bounds(target->pos() + coord_def(random_range(-15, 15),
                                                                   random_range(-15, 15)));
        if (cell_see_cell(target->pos(), area, LOS_DEFAULT))
            continue;

        coord_def base_spot;
        int tries = 0;
        while (tries < 10 && base_spot.origin())
        {
            find_habitable_spot_near(area, MONS_SPIRIT_WOLF, 6, false, base_spot);
            if (cell_see_cell(target->pos(), base_spot, LOS_DEFAULT))
                base_spot.reset();
            ++tries;
        }
        if (base_spot.origin())
            continue;

        int wolves = min(1 + random2(3), you.props["spirit_wolf_total"].get_int());
        int created = 0;
        for (int i = 0; i < wolves; ++i)
        {
            if (monster *mons = create_monster(
                                  mgen_data(MONS_SPIRIT_WOLF, BEH_HOSTILE, NULL,
                                            0, SPELL_NO_SPELL, base_spot,
                                            target->mindex(), MG_FORCE_BEH)))
            {
                mons->add_ench(mon_enchant(ENCH_HAUNTING, 1, target, INFINITE_DURATION));
                mons->props["howl_called"].get_bool() = true;
                mons->behaviour = BEH_SEEK;
                ++created;
                you.props["spirit_wolf_total"].get_int()--;
            }
        }

        return created;
    }

    return 0;
}

void waterport_touch(monster* nymph, actor* target)
{
    // Don't constantly teleport the target if it's already standing in water
    if (feat_is_water(grd(target->pos())))
        return;

    coord_def base;

    // If we don't have a 'home', let's at least try to find some water somewhere
    if (nymph->patrol_point.origin())
    {
        // Check within LoS first, but then keep going until we find some water
        for (distance_iterator di(nymph->pos()); di; ++di)
        {
            if (feat_is_water(grd(*di)))
            {
                base = *di;
                break;
            }
        }

        // If somehow we haven't found any (none on the level?) just bail
        if (base.origin())
            return;
    }
    else
        base = nymph->patrol_point;

    int least_land = 1000;
    coord_def best_spot;
    for (int tries = 0; tries < 10; ++tries)
    {
        coord_def spot;
        // MONS_BIG_FISH is just a placeholder for 'find water' here
        find_habitable_spot_near(base, MONS_BIG_FISH, 6, false, spot);

        if (!target->is_habitable_feat(grd(spot)))
            continue;

        if (!spot.origin())
        {
            int land_count = -1;
            for (distance_iterator di(spot, false, false, 3); di; ++di)
                if (feat_has_dry_floor(grd(*di)))
                    ++land_count;
            if (land_count < least_land)
            {
                least_land = land_count;
                best_spot = spot;
            }
        }

        // Not going to be able to do better than this, so save the effort
        if (least_land == 0)
            break;
    }

    if (!best_spot.origin())
    {
        for (adjacent_iterator ai(best_spot); ai; ++ai)
        {
            if (feat_is_water(grd(*ai)) && !actor_at(*ai))
            {
                nymph->move_to_pos(*ai);
                target->move_to_pos(best_spot);
                mprf("%s draws %s back to her home.",
                        nymph->name(DESC_THE).c_str(),
                        target->name(DESC_THE).c_str());
                return;
            }
        }
    }
}
