/**
 * @file
 * @brief Monster abilities.
**/

#include "AppHdr.h"

#include "mon-abil.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <set>

#include "act-iter.h"
#include "actor.h"
#include "areas.h"
#include "arena.h"
#include "beam.h"
#include "bloodspatter.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "exclude.h"
#include "fight.h"
#include "fprop.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "losglobal.h"
#include "message.h"
#include "mgen_data.h"
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
#include "mon-speak.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "ouch.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "spl-damage.h"
#include "spl-miscast.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "viewchar.h"
#include "view.h"

static bool _slime_split_merge(monster* thing);

// Currently only used for Tiamat.
void draconian_change_colour(monster* drac)
{
    if (mons_genus(drac->type) != MONS_DRACONIAN)
        return;

    drac->base_monster = random_choose(MONS_RED_DRACONIAN,
                                       MONS_WHITE_DRACONIAN,
                                       MONS_BLACK_DRACONIAN,
                                       MONS_GREEN_DRACONIAN,
                                       MONS_PURPLE_DRACONIAN,
                                       MONS_MOTTLED_DRACONIAN,
                                       MONS_PALE_DRACONIAN,
                                       MONS_YELLOW_DRACONIAN,
                                       -1);
    drac->colour = mons_class_colour(drac->base_monster);

    // Get rid of the old breath weapon first.
    monster_spells oldspells = drac->spells;
    drac->spells.clear();
    for (size_t i = 0; i < oldspells.size(); i++)
        if (!(oldspells[i].flags & MON_SPELL_BREATH))
            drac->spells.push_back(oldspells[i]);

    drac->spells.push_back(drac_breath(draco_or_demonspawn_subspecies(drac)));
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

    int initial_count = usehd ? initial->get_hit_dice() : initial->blob_size;
    int merge_to_count = usehd ? merge_to->get_hit_dice() : merge_to->blob_size;
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
    slime->max_hit_points = (int)(slime->blob_size * max_per_blob);
    slime->hit_points = (int)(slime->blob_size * current_per_blob);
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

    int split_off = thing->blob_size / 2;
    float max_per_blob = thing->max_hit_points / float(thing->blob_size);
    float current_per_blob = thing->hit_points / float(thing->blob_size);

    thing->blob_size -= split_off;
    new_slime->blob_size = split_off;

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

    merge_to->blob_size += initial_slime->blob_size;
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
            int new_blob_count = other_thing->blob_size + thing->blob_size;
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
    if (!thing || thing->blob_size <= 1 || thing->hit_points < 4
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

    if (slime->blob_size > 1 && x_chance_in_y(4, 5))
    {
        int count = 0;
        while (slime->blob_size > 1 && count <= 10)
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
        || mon->blob_size <= 1
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
        ouch(dam, KILLED_BY_BEAM, mon->mid, "accursed screaming");
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
    int count = mons->mangrove_pests;
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
            mons->mangrove_pests--;
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

    case MONS_STARCURSED_MASS:
        if (x_chance_in_y(mons->blob_size,8) && x_chance_in_y(2,3)
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
        if (mons->hit_points * 2 < mons->max_hit_points
            && mons->mangrove_pests > 0)
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
        break;
    }

    if (used)
        mons->lose_energy(EUT_SPECIAL);

    return used;
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
