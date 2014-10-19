/**
 * @file
 * @brief Monster abilities.
**/

#include "AppHdr.h"
#include "mon-abil.h"

#include "externs.h"

#include "actor.h"
#include "act-iter.h"
#include "arena.h"
#include "beam.h"
#include "bloodspatter.h"
#include "colour.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "exclude.h" //XXX
#include "fight.h"
#include "fprop.h"
#include "ghost.h"
#include "itemprop.h"
#include "losglobal.h"
#include "libutil.h"
#include "message.h"
#include "misc.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-book.h"
#include "mon-cast.h"
#include "mon-chimera.h"
#include "mon-death.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-project.h"
#include "mon-util.h"
#include "terrain.h"
#include "mgen_data.h"
#include "cloud.h"
#include "mon-speak.h"
#include "mon-tentacle.h"
#include "random.h"
#include "religion.h"
#include "spl-damage.h"
#include "spl-miscast.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "env.h"
#include "areas.h"
#include "view.h"
#include "shout.h"
#include "viewchar.h"
#include "ouch.h"
#include "target.h"
#include "items.h"
#include "teleport.h"

#include <algorithm>
#include <queue>
#include <map>
#include <set>
#include <cmath>

static bool _slime_split_merge(monster* thing);

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
void merge_ench_durations(monster* initial, monster* merge_to, bool usehd)
{
    mon_enchant_list::iterator i;

    int initial_count = usehd ? initial->get_hit_dice() : initial->number;
    int merge_to_count = usehd ? merge_to->get_hit_dice() : merge_to->number;
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
                                         SAME_ATTITUDE(thing),
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
    new_slime->behaviour = thing->behaviour;
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

    new_slime->set_hit_dice(thing->get_experience_level());

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
    const int orighd = merge_to->get_experience_level();
    int addhd = crawlie->get_experience_level();

    // Abomination is fully healed.
    if (merge_to->type == MONS_ABOMINATION_LARGE
        && merge_to->max_hit_points == merge_to->hit_points)
    {
        return false;
    }

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
            hp += hp/10;
        }
        else if (merge_to->type == MONS_ABOMINATION_LARGE)
        {
            // Healing an existing abomination.
            mhp = merge_to->max_hit_points;
            hp = merge_to->hit_points;
            if (mhp <= hp + mhp/10)
                hp = mhp;
            else
                hp += mhp/10;
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
    merge_to->set_hit_dice(orighd);
    merge_ench_durations(crawlie, merge_to, true);

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
    merge_ench_durations(initial_slime, merge_to);

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

        flash_view_delay(UA_MONSTER, LIGHTGREEN, 150);
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
    return thing->asleep() || mons_is_wandering(thing)
           || thing->foe == MHITNOT;
}

// Slime creatures cannot split or merge under these conditions.
static bool _disabled_merge(monster* thing)
{
    return !thing
           || mons_is_fleeing(thing)
           || mons_is_confused(thing)
           || thing->paralysed();
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
    case MONS_ABOMINATION_LARGE:
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
    return mons_class_can_pass(MONS_SLIME_CREATURE, env.grid(target))
           && !actor_at(target);
}

// See if slime creature 'thing' can split, and carry out the split if
// we can find a square to place the new slime creature on.
static monster *_slime_split(monster* thing, bool force_split)
{
    if (!thing || thing->number <= 1 || thing->hit_points < 4
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
        for (adjacent_iterator ri(origin); ri; ++ri)
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

    for (monster_near_iterator mi(target->pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (mi->type == MONS_STARCURSED_MASS)
            chorus.push_back(*mi);
    }

    int n = chorus.size();
    int dam = 0; int stun = 0;
    const char* message = nullptr;

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
        if (one_chance_in(3))
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
        mprf(MSGCH_MONSTER_SPELL, "%s", message);
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
    int n = 0;

    for (monster_near_iterator mi(mon->pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (mi->type != MONS_STARCURSED_MASS)
            continue;

        // Don't scream if any part of the chorus has a scream timeout
        // (This prevents it being staggered into a bunch of mini-screams)
        if (mi->has_ench(ENCH_SCREAMED))
            return false;
        else
            n++;
    }

    return one_chance_in(n);
}

// Returns true if you resist the merfolk avatar's call.
static bool _merfolk_avatar_movement_effect(const monster* mons)
{
    bool do_resist = (you.attribute[ATTR_HELD]
                      || you.duration[DUR_TIME_STEP]
                      || you.cannot_act()
                      || you.clarity()
                      || you.is_stationary());

    if (!do_resist)
    {
        // We use a beam tracer here since it is better at navigating
        // obstructing walls than merely comparing our relative positions
        bolt tracer;
        tracer.is_beam = true;
        tracer.affects_nothing = true;
        tracer.target = mons->pos();
        tracer.source = you.pos();
        tracer.range = LOS_RADIUS;
        tracer.is_tracer = true;
        tracer.aimed_at_spot = true;
        tracer.fire();

        const coord_def newpos = tracer.path_taken[0];

        if (!in_bounds(newpos)
            || is_feat_dangerous(grd(newpos))
            || !you.can_pass_through_feat(grd(newpos))
            || !cell_see_cell(mons->pos(), newpos, LOS_NO_TRANS))
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
                    && !mon->is_stationary()
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
                mpr("The pull of its song draws you forwards.");

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
                move_player_to_grid(newpos, true);

                if (swapping)
                    mon->apply_location_effects(newpos);
            }
        }
    }

    return do_resist;
}

static bool _lost_soul_affectable(const monster* mons)
{
    return ((mons->holiness() == MH_UNDEAD
             && mons->type != MONS_LOST_SOUL
             && !mons_is_zombified(mons))
            ||(mons->holiness() == MH_NATURAL
               && !is_good_god(mons->god)))
           && !mons->is_summoned()
           && !mons_class_flag(mons->type, M_NO_EXP_GAIN);
}

static bool _lost_soul_teleport(monster* mons)
{
    bool seen = you.can_see(mons);

    typedef pair<monster*, int> mon_quality;
    vector<mon_quality> candidates;

    // Assemble candidate list and randomize
    for (monster_iterator mi; mi; ++mi)
    {
        if (_lost_soul_affectable(*mi) && mons_aligned(mons, *mi))
        {
            mon_quality m(*mi, min(mi->get_experience_level(), 18) + random2(8));
            candidates.push_back(m);
        }
    }
    sort(candidates.begin(), candidates.end(), greater_second<mon_quality>());

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
    for (monster_near_iterator mi(soul, LOS_NO_TRANS); mi; ++mi)
    {
        if (_lost_soul_affectable(*mi))
            ++count;
        else if (mi->type == MONS_LOST_SOUL)
            --count;
    }

    const int target_hd = target->get_experience_level();
    return count <= -1 || target_hd > 9
           || x_chance_in_y(target_hd * target_hd * target_hd, 1200);
}

bool lost_soul_revive(monster* mons)
{
    if (!_lost_soul_affectable(mons))
        return false;

    for (monster_near_iterator mi(mons, LOS_NO_TRANS); mi; ++mi)
    {
        if (mi->type == MONS_LOST_SOUL && mons_aligned(mons, *mi))
        {
            if (!_worthy_sacrifice(*mi, mons))
                continue;

            targetter_los hitfunc(*mi, LOS_SOLID);
            flash_view_delay(UA_MONSTER, GREEN, 200, &hitfunc);

            mons->heal(mons->max_hit_points);
            mons->del_ench(ENCH_CONFUSION, true);
            mons->timeout_enchantments(10);

            coord_def newpos = mi->pos();
            if (mons->holiness() == MH_NATURAL)
            {
                mons->move_to_pos(newpos);
                mons->flags |= (MF_SPECTRALISED | MF_FAKE_UNDEAD);
            }

            // check if you can see the monster *after* it maybe moved
            if (you.can_see(mons))
            {
                if (mons->holiness() == MH_UNDEAD)
                {
                    mprf("%s sacrifices itself to reknit %s!",
                         mi->name(DESC_THE).c_str(),
                         mons->name(DESC_THE).c_str());
                }
                else
                {
                    mprf("%s assumes the form of %s%s!",
                         mi->name(DESC_THE).c_str(),
                         mons->name(DESC_THE).c_str(),
                         (mi->is_summoned() ? " and becomes anchored to this"
                          " world" : ""));
                }
            }

            monster_die(*mi, KILL_MISC, -1, true);

            return true;
        }
    }

    return false;
}

void treant_release_fauna(monster* mons)
{
    int count = mons->number;
    bool created = false;

    monster_type base_t = (one_chance_in(3) ? MONS_YELLOW_WASP
                                            : MONS_RAVEN);

    mon_enchant abj = mons->get_ench(ENCH_ABJ);

    for (int i = 0; i < count; ++i)
    {
        monster_type fauna_t = (base_t == MONS_YELLOW_WASP && one_chance_in(3)
                                                ? MONS_RED_WASP
                                                : base_t);

        mgen_data fauna_data(fauna_t, SAME_ATTITUDE(mons),
                            mons, 0, SPELL_NO_SPELL, mons->pos(),
                            mons->foe);
        fauna_data.extra_flags |= MF_WAS_IN_VIEW;
        monster* fauna = create_monster(fauna_data);

        if (fauna)
        {
            fauna->props["band_leader"].get_int() = mons->mid;

            // Give released fauna the same summon duration as their 'parent'
            if (abj.ench != ENCH_NONE)
                fauna->add_ench(abj);

            created = true;
            mons->number--;
        }
    }

    if (created && you.can_see(mons))
    {
        if (base_t == MONS_YELLOW_WASP)
        {
                mprf("Angry insects surge out from beneath %s foliage!",
                        mons->name(DESC_ITS).c_str());
        }
        else if (base_t == MONS_RAVEN)
        {
                mprf("Agitated ravens fly out from beneath %s foliage!",
                        mons->name(DESC_ITS).c_str());
        }
    }
}

static void _entangle_actor(actor* act)
{
    if (act->is_player())
    {
        you.duration[DUR_GRASPING_ROOTS] = 10;
        you.redraw_evasion = true;
        if (you.duration[DUR_FLIGHT] ||  you.attribute[ATTR_PERM_FLIGHT])
        {
            you.duration[DUR_FLIGHT] = 0;
            you.attribute[ATTR_PERM_FLIGHT] = 0;
            land_player(true);
        }
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
        mprf(MSGCH_TALK_VISUAL, "%s", random_choose(
                "Tangled roots snake along the ground.",
                "The ground creaks as gnarled roots bulge its surface.",
                "A root reaches out and grasps at passing movement.",
                0));
    }

    bool found_hostile = false;
    for (actor_near_iterator ai(mons, LOS_NO_TRANS); ai; ++ai)
    {
        if (mons_aligned(mons, *ai) || ai->is_insubstantial()
            || !ai->visible_to(mons))
        {
            continue;
        }

        found_hostile = true;

        // Roots can't reach things over deep water or lava
        if (!feat_has_solid_floor(grd(ai->pos())))
            continue;

        // Some messages are suppressed for monsters, to reduce message spam.
        if (ai->flight_mode())
        {
            if (x_chance_in_y(3, 5))
                continue;

            if (x_chance_in_y(10, 50 - ai->melee_evasion(NULL)))
            {
                if (ai->is_player())
                    mpr("Roots rise up to grasp you, but you nimbly evade.");
                continue;
            }

            if (you.can_see(*ai))
            {
                mprf("Roots rise up from beneath %s and drag %s %sto the ground.",
                     ai->name(DESC_THE).c_str(),
                     ai->pronoun(PRONOUN_OBJECTIVE).c_str(),
                     ai->is_monster() ? "" : "back ");
            }
        }
        else if (ai->is_player() && !you.duration[DUR_GRASPING_ROOTS])
        {
            mprf("Roots grasp at your %s, making movement difficult.",
                 you.foot_name(true).c_str());
        }

        _entangle_actor(*ai);
    }

    return found_hostile;
}

void check_grasping_roots(actor* act, bool quiet)
{
    bool source = false;
    for (monster_near_iterator mi(act->pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_aligned(act, *mi) && mi->has_ench(ENCH_GRASPING_ROOTS_SOURCE))
        {
            source = true;
            break;
        }
    }

    if (!source || !feat_has_solid_floor(grd(act->pos())))
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
    mons_cast(mons, pbolt, spell_cast, MON_SPELL_NATURAL);
}

void merfolk_avatar_song(monster* mons)
{
    // First, attempt to pull the player, if mesmerised
    if (you.beheld_by(mons) && coinflip())
    {
        // Don't pull the player if they walked forward voluntarily this
        // turn (to avoid making you jump two spaces at once)
        if (!mons->props["foe_approaching"].get_bool())
        {
            _merfolk_avatar_movement_effect(mons);

            // Reset foe tracking position so that we won't automatically
            // veto pulling on a subsequent turn because you 'approached'
            mons->props["foe_pos"].get_coord() = you.pos();
        }
    }

    // Only call up drowned souls if we're largely alone; otherwise our
    // mesmerisation can support the present allies well enough.
    int ally_hd = 0;
    for (monster_near_iterator mi(&you); mi; ++mi)
    {
        if (*mi != mons && mons_aligned(mons, *mi) && !mons_is_firewood(*mi)
            && mi->type != MONS_DROWNED_SOUL)
        {
            ally_hd += mi->get_experience_level();
        }
    }
    if (ally_hd > mons->get_experience_level())
    {
        if (mons->props.exists("merfolk_avatar_call"))
        {
            // Normally can only happen if allies of the merfolk avatar show up
            // during a song that has already summoned drowned souls (though is
            // technically possible if some existing ally gains HD instead)
            if (you.see_cell(mons->pos()))
                mpr("The shadowy forms in the deep grow still as others approach.");
            mons->props.erase("merfolk_avatar_call");
        }

        return;
    }

    // Can only call up drowned souls if there's free deep water nearby
    vector<coord_def> deep_water;
    for (radius_iterator ri(mons->pos(), LOS_RADIUS, C_ROUND); ri; ++ri)
        if (grd(*ri) == DNGN_DEEP_WATER && !actor_at(*ri))
            deep_water.push_back(*ri);

    if (deep_water.size())
    {
        if (!mons->props.exists("merfolk_avatar_call"))
        {
            if (you.see_cell(mons->pos()))
            {
                mprf("Shadowy forms rise from the deep at %s song!",
                     mons->name(DESC_ITS).c_str());
            }
            mons->props["merfolk_avatar_call"].get_bool() = true;
        }

        if (coinflip())
        {
            int num = 1 + one_chance_in(4);
            shuffle_array(deep_water);

            int existing = 0;
            for (monster_near_iterator mi(mons); mi; ++mi)
            {
                if (mi->type == MONS_DROWNED_SOUL)
                    existing++;
            }
            num = min(min(num, 5 - existing), int(deep_water.size()));

            for (int i = 0; i < num; ++i)
            {
                monster* soul = create_monster(mgen_data(MONS_DROWNED_SOUL,
                                 SAME_ATTITUDE(mons), mons, 1, SPELL_NO_SPELL,
                                 deep_water[i], mons->foe, MG_FORCE_PLACE));

                // Scale down drowned soul damage for low level merfolk avatars
                if (soul)
                    soul->set_hit_dice(mons->get_hit_dice());
            }
        }
    }
}

/**
 * Have a siren or merfolk avatar attempt to mesmerize the player.
 *
 * @param mons  The singing monster.
 * @param spl   The channel to print messages in.
 * @return      Whether the ability was used.
 */
static bool _siren_sing(monster* mons, msg_channel_type spl)
{
    // Don't behold observer in the arena.
    if (crawl_state.game_is_arena())
        return false;

    // Don't behold player already half down or up the stairs.
    if (!you.delay_queue.empty())
    {
        const delay_queue_item delay = you.delay_queue.front();

        if (delay.type == DELAY_ASCENDING_STAIRS
            || delay.type == DELAY_DESCENDING_STAIRS)
        {
            dprf("Taking stairs, don't mesmerise.");
            return false;
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
        return false;
    }

    // Don't even try on berserkers. Sirens know their limits.
    // (merfolk avatars should still sing since their song has other effects)
    if (mons->type != MONS_MERFOLK_AVATAR && you.berserk())
        return false;

    const bool already_mesmerised = you.beheld_by(mons);

    // If the mer is trying to mesmerize you anew, sing 60% of the time.
    // Otherwise, only sing 20% of the time.
    const bool can_mesm_you = !already_mesmerised && mons->foe == MHITYOU
                              && you.can_see(mons);

    if (x_chance_in_y(can_mesm_you ? 2 : 4, 5))
        return false;

    // Sing! Beyond this point, we should always return true.
    noisy(LOS_RADIUS, mons->pos(), mons->mindex(), true);

    if (mons->type == MONS_MERFOLK_AVATAR
        && !mons->has_ench(ENCH_MERFOLK_AVATAR_SONG))
    {
        mons->add_ench(mon_enchant(ENCH_MERFOLK_AVATAR_SONG, 0, mons, 70));
    }

    if (you.can_see(mons))
    {
        const char * const song_adj = already_mesmerised ? "its luring"
                                                         : "a haunting";
        const string song_desc = make_stringf(" chants %s song.", song_adj);
        simple_monster_message(mons, song_desc.c_str(), spl);
    }
    else
    {
        mprf(MSGCH_SOUND, "You hear %s.",
                          already_mesmerised ? "a luring song" :
                          coinflip()         ? "a haunting song"
                                             : "an eerie melody");

        // If you're already mesmerised by an invisible siren, it
        // can still prolong the enchantment.
        if (!already_mesmerised)
            return true;
    }

    // Once mesmerised by a particular monster, you cannot resist anymore.
    if (you.duration[DUR_MESMERISE_IMMUNE]
        || !already_mesmerised
           && (you.check_res_magic(mons->get_hit_dice() * 22 / 3 + 15) > 0
               || you.clarity()))
    {
        canned_msg(you.clarity() ? MSG_YOU_UNAFFECTED : MSG_YOU_RESIST);
        return true;
    }

    you.add_beholder(mons);
    return true;
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
                                  ? draco_or_demonspawn_subspecies(mons)
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

        for (monster_near_iterator targ(mons, LOS_NO_TRANS); targ; ++targ)
        {
            if (mons_aligned(mons, *targ) || distance2(mons->pos(), targ->pos()) > 4)
                continue;

            if (!cell_is_solid(targ->pos()))
            {
                mons->suicide();
                used = true;
                break;
            }
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

        if (!mons->has_ench(ENCH_ROLLING)
            && !feat_is_water(grd(mons->pos())))
        {
            // Fleeing check
            if (mons_is_fleeing(mons))
            {
                if (coinflip())
                {
                //  behaviour_event(mons, ME_CORNERED);
                    simple_monster_message(mons, " curls into a ball and rolls away!");
                    boulder_start(mons, &beem);
                    used = true;
                }
            }
            // Normal check - don't roll at adjacent targets
            else if (one_chance_in(3)
                     && !adjacent(mons->pos(), beem.target))
            {
                simple_monster_message(mons, " curls into a ball and starts rolling!");
                boulder_start(mons, &beem);
                used = true;
            }
        }
        break;

    case MONS_SIREN:
    case MONS_MERFOLK_AVATAR:
    {
        used = _siren_sing(mons, spl);
        break;
    }

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

    case MONS_LOST_SOUL:
    if (one_chance_in(3))
    {
        bool see_friend = false;
        bool see_foe = false;

        for (actor_near_iterator ai(mons, LOS_NO_TRANS); ai; ++ai)
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

            if (found)
            {
                const bool could_see = you.can_see(mons);

                fight_melee(mons, mons->get_foe());
                if (!mons->alive())
                    return true;

                if (mons->move_to_pos(hopspot))
                {
                    if (could_see || you.can_see(mons))
                    {
                        mprf("%s hops backward while attacking.",
                             mons->name(DESC_THE, true).c_str());
                    }
                    mons->speed_increment -= 2; // Add a small extra delay
                }
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
            for (monster_near_iterator mi(mons, LOS_NO_TRANS); mi; ++mi)
            {
                if (mi->type == MONS_BRIAR_PATCH
                    && distance2(mons->pos(), mi->pos()) > 17)
                {
                    briar_count++;
                }
            }
            if (briar_count < 4) // Probably no solid wall here
            {
                _mons_cast_abil(mons, beem, SPELL_WALL_OF_BRAMBLES);
                used = true;
            }
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
                if (find_habitable_spot_near(foe->pos(), MONS_ELECTRIC_EEL, 3, false, spot)
                    && foe->pos().distance_from(spot)
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

    case MONS_SHAMBLING_MANGROVE:
    {
        if (mons->hit_points * 2 < mons->max_hit_points && mons->number > 0)
        {
            treant_release_fauna(mons);
            // Intentionally takes no energy; the creatures are flying free
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

    case MONS_GUARDIAN_GOLEM:
        if (mons->hit_points * 2 < mons->max_hit_points && one_chance_in(4)
             && !mons->has_ench(ENCH_INNER_FLAME))
        {
            simple_monster_message(mons, " overheats!");
            mons->add_ench(mon_enchant(ENCH_INNER_FLAME, 0, 0,
                                       INFINITE_DURATION));
        }
        break;

    default:
        if (mons_class_flag(mclass, M_BLINKER)
            && !mons->no_tele(true, false)
            && x_chance_in_y(mons->caught() ? 3 : 1, 7))
        {
            used = monster_blink(mons);
        }
        break;
    }

    if (used)
        mons->lose_energy(EUT_SPECIAL);

    return used;
}

// Combines code for eyeball-type monsters, etc. to reduce clutter.
static bool _eyeball_will_use_ability(monster* mons)
{
    return coinflip()
       && !mons_is_confused(mons)
       && !mons_is_wandering(mons)
       && !mons_is_fleeing(mons)
       && !mons->pacified()
       && !player_or_mon_in_sanct(mons);
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
        if (_eyeball_will_use_ability(mons)
            && mons->see_cell_no_trans(foe->pos()))
        {
            const bool can_see = you.can_see(mons);
            if (can_see && you.can_see(foe))
            {
                mprf("%s blinks at %s.",
                     mons->name(DESC_THE).c_str(),
                     foe->name(DESC_THE).c_str());
            }

            int confuse_power = 2 + random2(3);

            if (foe->is_player() && !can_see)
            {
                canned_msg(MSG_BEING_WATCHED);
                interrupt_activity(AI_MONSTER_ATTACKS, mons);
            }

            int res_margin = foe->check_res_magic((mons->get_hit_dice() * 5)
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
        if (_eyeball_will_use_ability(mons)
            && mons->see_cell_no_trans(foe->pos()))
        {
            const bool can_see = you.can_see(mons);
            if (can_see && you.can_see(foe))
            {
                mprf("%s stares at %s.",
                     mons->name(DESC_THE).c_str(),
                     foe->name(DESC_THE).c_str());
            }

            if (foe->is_player() && !can_see)
                canned_msg(MSG_BEING_WATCHED);

            // Subtly different from old paralysis behaviour, but
            // it'll do.
            foe->paralyse(mons, 2 + random2(3));
        }
        break;

    case MONS_EYE_OF_DRAINING:
    case MONS_GHOST_MOTH:
        if (_eyeball_will_use_ability(mons)
            && mons->see_cell_no_trans(foe->pos())
            && foe->is_player())
        {
            if (you.can_see(mons))
                simple_monster_message(mons, " stares at you.");
            else
                canned_msg(MSG_BEING_WATCHED);

            interrupt_activity(AI_MONSTER_ATTACKS, mons);

            int mp = 5 + random2avg(13, 3);
#if TAG_MAJOR_VERSION == 34
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
#else
            mp = min(mp, you.magic_points);
#endif
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

void guardian_golem_bond(monster* mons)
{
    for (monster_near_iterator mi(mons, LOS_NO_TRANS); mi; ++mi)
    {
        if (mons_aligned(mons, *mi) && !mi->has_ench(ENCH_CHARM)
            && *mi != mons)
        {
            mi->add_ench(mon_enchant(ENCH_INJURY_BOND, 1, mons, INFINITE_DURATION));
        }
    }
}
