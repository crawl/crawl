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
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "delay.h"
#include "dgn-overview.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "fprop.h"
#include "god-abil.h"
#include "item-prop.h"
#include "libutil.h"
#include "losglobal.h"
#include "message.h"
#include "mgen-data.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-death.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "mon-util.h"
#include "ouch.h"
#include "random.h"
#include "religion.h"
#include "spl-damage.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "view.h"
#include "viewchar.h"

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
                                       MONS_YELLOW_DRACONIAN);
    drac->colour = mons_class_colour(drac->base_monster);

    // Get rid of the old breath weapon first.
    monster_spells oldspells = drac->spells;
    drac->spells.clear();
    for (const mon_spell_slot &slot : oldspells)
        if (!(slot.flags & MON_SPELL_BREATH))
            drac->spells.push_back(slot);

    drac->spells.push_back(drac_breath(draconian_subspecies(*drac)));
}

void boris_covet_orb(monster* boris)
{
    if (boris->type != MONS_BORIS || !player_has_orb())
        return;

    if (boris->observable())
        simple_monster_message(*boris, " is empowered by the presence of the orb!");

    boris->add_ench(mon_enchant(ENCH_HASTE, 1, boris, INFINITE_DURATION));
    boris->add_ench(mon_enchant(ENCH_EMPOWERED_SPELLS, 1, boris,
                    INFINITE_DURATION));
}

bool ugly_thing_mutate(monster& ugly, bool force)
{
    if (!(one_chance_in(9) || force))
        return false;

    const char* msg = nullptr;
    // COLOUR_UNDEF means "pick a random colour".
    colour_t new_colour = COLOUR_UNDEF;

    for (fair_adjacent_iterator ai(ugly.pos()); ai && !msg; ++ai)
    {
        const actor* act = actor_at(*ai);

        if (!act)
            continue;

        if (act->is_player() && get_contamination_level())
            msg = " basks in your mutagenic energy and changes!";
        else if (mons_genus(act->type) == MONS_UGLY_THING)
        {
            msg = " basks in the mutagenic energy from its kin and changes!";
            const colour_t other_colour =
                make_low_colour(act->as_monster()->colour);
            if (make_low_colour(ugly.colour) != other_colour)
                new_colour = other_colour;
        }
    }

    if (force)
        msg = " basks in the mutagenic energy and changes!";

    if (!msg) // didn't find anything to mutate off of
        return false;

    simple_monster_message(ugly, msg);
    ugly.uglything_mutate(new_colour);
    return true;
}

// Returns whether an enchantment should be given to split monsters and inherited on merge.
// TODO: This list is definitely incomplete.
static bool _should_share_ench(enchant_type type)
{
    return type != ENCH_HELD
           && type != ENCH_GRASPING_ROOTS
           && type != ENCH_VILE_CLUTCH
           && type != ENCH_BULLSEYE_TARGET;
}

// Inflict any enchantments the parent slime has on its offspring,
// leaving durations unchanged, I guess. -cao
static void _split_ench_durations(monster* initial_slime, monster* split_off)
{
    for (const auto &entry : initial_slime->enchantments)
    {
        if (_should_share_ench(entry.second.ench))
        {
            split_off->add_ench(entry.second);

            // The newly split slime will also be vengeance marked, so we need
            // to increment the total number of monsters the player has to kill
            if (entry.second.ench == ENCH_VENGEANCE_TARGET)
                you.duration[DUR_BEOGH_SEEKING_VENGEANCE] += 1;
        }
    }
}

// What to do about any enchantments these two creatures may have?
// For now, we are averaging the durations, weighted by slime size
// or by hit dice, depending on usehd.
void merge_ench_durations(monster& initial, monster& merge_to, bool usehd)
{
    int initial_count = usehd ? initial.get_hit_dice() : initial.blob_size;
    int merge_to_count = usehd ? merge_to.get_hit_dice() : merge_to.blob_size;
    int total_count = initial_count + merge_to_count;

    mon_enchant_list &from_ench = initial.enchantments;

    for (auto &entry : from_ench)
    {
        if (!_should_share_ench(entry.second.ench))
            continue;

        // Does the other creature have this enchantment as well?
        const mon_enchant temp = merge_to.get_ench(entry.first);
        // If not, use duration 0 for their part of the average.
        const bool no_initial = temp.ench == ENCH_NONE;
        const int duration = no_initial ? 0 : temp.duration;

        entry.second.duration = (entry.second.duration * initial_count
                                 + duration * merge_to_count)/total_count;

        if (!entry.second.duration)
            entry.second.duration = 1;

        if (no_initial)
            merge_to.add_ench(entry.second);
        else
            merge_to.update_ench(entry.second);
    }

    for (auto &entry : merge_to.enchantments)
    {
        if (from_ench.find(entry.first) == from_ench.end()
            && entry.second.duration > 1)
        {
            entry.second.duration = (merge_to_count * entry.second.duration)
                                    / total_count;

            merge_to.update_ench(entry.second);
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
static monster* _do_split(monster* thing, const coord_def & target)
{
    ASSERT(thing); // XXX: change to monster &thing
    ASSERT(thing->alive());

    // Create a new slime.
    mgen_data new_slime_data = mgen_data(thing->type,
                                         SAME_ATTITUDE(thing),
                                         target,
                                         thing->foe,
                                         MG_FORCE_PLACE);

    // Don't explicitly announce the child slime coming into view if you
    // saw the split that created it
    if (you.can_see(*thing))
        new_slime_data.extra_flags |= MF_WAS_IN_VIEW;

    monster *new_slime = create_monster(new_slime_data);

    if (!new_slime)
        return 0;

    if (you.can_see(*thing))
        mprf("%s splits.", thing->name(DESC_A).c_str());

    // Inflict the new slime with any enchantments on the parent.
    _split_ench_durations(thing, new_slime);
    new_slime->attitude = thing->attitude;
    new_slime->behaviour = thing->behaviour;
    new_slime->flags = thing->flags;
    new_slime->props = thing->props;
    new_slime->summoner = thing->summoner;
    if (thing->props.exists(BLAME_KEY))
        new_slime->props[BLAME_KEY] = thing->props[BLAME_KEY].get_vector();

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

// Cause a monster to lose a turn. has_gone should be true if the
// monster has already moved this turn.
static void _lose_turn(monster* mons, bool has_gone)
{
    const monsterentry* entry = get_monster_data(mons->type);

    // We want to find out if mons will move next time it has a turn
    // (assuming for the sake of argument the next delay is 10). If it's
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

// Actually merge two slime creatures, pooling their hp, etc.
// initial_slime is the one that gets killed off by this process.
static void _do_merge_slimes(monster* initial_slime, monster* merge_to)
{
    const string old_name = merge_to->name(DESC_A);
    const bool merge_to_was_visible = you.can_see(*merge_to);

    // Combine enchantment durations.
    merge_ench_durations(*initial_slime, *merge_to);

    merge_to->blob_size += initial_slime->blob_size;
    merge_to->max_hit_points += initial_slime->max_hit_points;
    merge_to->hit_points += initial_slime->hit_points;

    // Merge monster flags (mostly so that MF_CREATED_NEUTRAL, etc. are
    // passed on if the merged slime subsequently splits. Hopefully
    // this won't do anything weird.
    merge_to->flags |= initial_slime->flags;

    // Transfer duel status over to the merge target.
    if (initial_slime->props.exists(OKAWARU_DUEL_CURRENT_KEY))
    {
        initial_slime->props.erase(OKAWARU_DUEL_CURRENT_KEY);
        merge_to->props[OKAWARU_DUEL_TARGET_KEY] = true;
        merge_to->props[OKAWARU_DUEL_CURRENT_KEY] = true;
    }

    // Merging costs the combined slime some energy. The idea is that if 2
    // slimes merge you can gain a space by moving away the turn after (maybe
    // this is too nice but there will probably be a lot of complaints about
    // the damage on higher level slimes). We see if mons has gone already by
    // checking its mindex (this works because handle_monsters just iterates
    // over env.mons in ascending order).
    _lose_turn(merge_to, merge_to->mindex() < initial_slime->mindex());

    // Overwrite the state of the slime getting merged into, because it
    // might have been resting or something.
    merge_to->behaviour = initial_slime->behaviour;
    merge_to->foe = initial_slime->foe;

    behaviour_event(merge_to, ME_EVAL);

    // Messaging cases:
    // 1. MT & I were both visible & still are
    // 2. MT was visible, I wasn't but now both are
    // 3. MT was visible, I wasn't and now both aren't
    // 4. MT wasn't visible, I was and now both are
    // 5. MT and I weren't visible & still aren't
    if (merge_to_was_visible)
    {
        if (you.can_see(*merge_to))
        {
            // cases 1 and 2
            mprf("Two slime creatures merge to form %s.",
                 merge_to->name(DESC_A).c_str());
        }
        else
        {
            // case 3
            mprf("Something merges into %s, and it vanishes!",
                 old_name.c_str());
        }

        flash_view_delay(UA_MONSTER, LIGHTGREEN, 150);
    }
    else if (you.can_see(*initial_slime))
    {
        // case 4
        mprf("%s merges with something you can't see.",
             initial_slime->name(DESC_A).c_str());
    }
    // case 5 (no-op)

    // Have to 'kill' the slime doing the merging.
    monster_die(*initial_slime, KILL_RESET, NON_MONSTER, true);
}

// Slime creatures can split but not merge under these conditions.
static bool _unoccupied_slime(monster* thing)
{
    return thing->asleep() || mons_is_wandering(*thing)
           || thing->foe == MHITNOT;
}

// Slime creatures cannot split or merge under these conditions.
static bool _disabled_merge(monster* thing)
{
    return !thing
           || mons_is_fleeing(*thing)
           || mons_is_confused(*thing)
           || thing->paralysed()
           || thing->has_ench(ENCH_FRENZIED);
}

// See if there are any appropriate adjacent slime creatures for 'thing'
// to merge with. If so, carry out the merge.
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
    int target_distance = grid_distance(thing->target, thing->pos());

    // Check for adjacent slime creatures.
    monster* merge_target = nullptr;
    for (fair_adjacent_iterator ai(thing->pos()); ai; ++ai)
    {
        // If this square won't reduce the distance to our target, don't
        // look for a potential merge, and don't allow this square to
        // prevent a merge if empty.
        if (grid_distance(thing->target, *ai) >= target_distance)
            continue;

        // Don't merge if there is an open square that reduces distance
        // to target, even if we found a possible slime to merge with.
        if (!actor_at(*ai)
            && mons_class_can_pass(MONS_SLIME_CREATURE, env.grid(*ai)))
        {
            return false;
        }

        // Is there a slime creature on this square we can consider
        // merging with?
        monster* other_thing = monster_at(*ai);
        if (!merge_target
            && other_thing
            && other_thing->type == MONS_SLIME_CREATURE
            && other_thing->attitude == thing->attitude
            && other_thing->has_ench(ENCH_CHARM) == thing->has_ench(ENCH_CHARM)
            && other_thing->has_ench(ENCH_HEXED) == thing->has_ench(ENCH_HEXED)
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
    {
        _do_merge_slimes(thing, merge_target);
        return true;
    }

    // No adjacent slime creatures we could merge with.
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
    const bool has_foe      = (foe != nullptr && thing->can_see(*foe));
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

    // Anywhere we can place an offspring?
    for (fair_adjacent_iterator ai(origin); ai; ++ai)
    {
        // Don't split if this increases the distance to the target.
        if (has_foe && grid_distance(*ai, foe_pos) > old_dist
            && !force_split)
        {
            continue;
        }

        if (_slime_can_spawn(*ai))
        {
            // This can fail if placing a new monster fails. That
            // probably means we have too many monsters on the level,
            // so just return in that case.
            return _do_split(thing, *ai);
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
bool slime_creature_polymorph(monster& slime, poly_power_type power)
{
    ASSERT(slime.type == MONS_SLIME_CREATURE);

    if (slime.blob_size > 1 && x_chance_in_y(4, 5))
    {
        int count = 0;
        while (slime.blob_size > 1 && count <= 10)
        {
            if (monster *splinter = _slime_split(&slime, true))
                slime_creature_polymorph(*splinter, power);
            else
                break;
            count++;
        }
    }

    return monster_polymorph(&slime, RANDOM_POLYMORPH_MONSTER, power);
}

static bool _starcursed_split(monster* mon)
{
    if (!mon || mon->blob_size <= 1 || mon->type != MONS_STARCURSED_MASS)
        return false;

    // Anywhere we can place an offspring?
    for (fair_adjacent_iterator ai(mon->pos()); ai; ++ai)
    {
        if (mons_class_can_pass(MONS_STARCURSED_MASS, env.grid(*ai))
            && !actor_at(*ai))
        {
            return _do_split(mon, *ai);
        }
    }

    // No free squares.
    return false;
}

static void _starcursed_scream(monster* mon, actor* target)
{
    if (!target || !target->alive())
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
    }
    else
        mprf(MSGCH_MONSTER_SPELL, "%s", message);
    target->hurt(mon, dam, BEAM_MISSILE, KILLED_BY_BEAM, "",
                 "accursed screaming");

    if (stun && target->alive())
        target->paralyse(mon, stun, "accursed screaming");

    for (monster *voice : chorus)
        if (voice->alive())
            voice->add_ench(mon_enchant(ENCH_SCREAMED, 1, voice, 1));
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

/**
 * Can a lost soul revive the given monster, assuming one is nearby?
 *
 * @param mons      The monster potentially being revived.
 * @return          Whether it's a possible target for lost souls.
 */
static bool _lost_soul_affectable(const monster &mons)
{
    // zombies are boring
    if (mons_is_zombified(mons))
        return false;

    // undead can be reknit, naturals ghosted, everyone else is out of luck
    if (!(mons.holiness() & (MH_UNDEAD | MH_NATURAL)))
        return false;

    // already been revived once
    if (testbits(mons.flags, MF_SPECTRALISED))
        return false;

    // just silly
    if (mons.type == MONS_LOST_SOUL)
        return false;

    // for ely, I guess?
    if (is_good_god(mons.god))
        return false;

    if (mons.is_summoned())
        return false;

    if (!mons_class_gives_xp(mons.type))
        return false;

    return true;
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
        if (_lost_soul_affectable(**mi))
            ++count;
        else if (mi->type == MONS_LOST_SOUL)
            --count;
    }

    const int target_hd = target->get_experience_level();
    return count <= -1 || target_hd > 9
           || x_chance_in_y(target_hd * target_hd * target_hd, 1200);
}

/**
 * Check to see if the given monster can be revived by lost souls (if it's a
 * valid target for revivication & if there are any lost souls nearby), and
 * revive it if so.
 *
 * @param mons  The monster in question.
 * @return      Whether the monster was revived/reknitted, or whether it
 *              remains dead (dying?).
 */
bool lost_soul_revive(monster& mons, killer_type killer)
{
    if (killer == KILL_RESET
        || killer == KILL_RESET_KEEP_ITEMS
        || killer == KILL_BANISHED)
    {
        return false;
    }

    if (!_lost_soul_affectable(mons))
        return false;

    for (monster_near_iterator mi(&mons, LOS_NO_TRANS); mi; ++mi)
    {
        if (mi->type != MONS_LOST_SOUL || !mons_aligned(&mons, *mi))
            continue;

        if (!_worthy_sacrifice(*mi, &mons))
            continue;

        // save this before we revive it
        const string revivee_name = mons.name(DESC_THE);
        const bool was_alive = bool(mons.holiness() & MH_NATURAL);

        // In this case the old monster will be replaced by
        // a ghostly version, so we should record defeat now.
        if (was_alive)
        {
            record_monster_defeat(&mons, killer);
            remove_unique_annotation(&mons);
        }

        targeter_radius hitfunc(*mi, LOS_SOLID);
        flash_view_delay(UA_MONSTER, GREEN, 200, &hitfunc);

        mons.heal(mons.max_hit_points);
        mons.del_ench(ENCH_CONFUSION, true);
        mons.timeout_enchantments(10);

        coord_def newpos = mi->pos();
        if (was_alive)
        {
            mons.move_to_pos(newpos);
            mons.flags |= (MF_SPECTRALISED | MF_FAKE_UNDEAD);
        }

        // check if you can see the monster *after* it maybe moved
        if (you.can_see(mons))
        {
            if (!was_alive)
            {
                mprf("%s sacrifices itself to reknit %s!",
                     mi->name(DESC_THE).c_str(),
                     revivee_name.c_str());
            }
            else
            {
                mprf("%s assumes the form of %s%s!",
                     mi->name(DESC_THE).c_str(),
                     revivee_name.c_str(),
                     (mi->is_summoned() ? " and becomes anchored to this"
                      " world" : ""));
            }
        }

        if (mi->alive())
            monster_die(**mi, KILL_NON_ACTOR, -1, true);

        return true;
    }

    return false;
}

void treant_release_fauna(monster& mons)
{
    // FIXME: this should be a fineff, at least when called from monster_die.
    int count = mons.mangrove_pests;
    bool created = false;

    monster_type fauna_t = MONS_HORNET;

    for (int i = 0; i < count; ++i)
    {
        mgen_data fauna_data(fauna_t, SAME_ATTITUDE(&mons),
                            mons.pos(),  mons.foe);
        fauna_data.extra_flags |= MF_WAS_IN_VIEW;

        // If the mangrove was summoned, give its fauna the same summon type and duration.
        if (mons.is_summoned())
        {
            mon_enchant summ = mons.get_ench(ENCH_SUMMON);
            mon_enchant timer = mons.get_ench(ENCH_SUMMON_TIMER);
            fauna_data.set_summoned(summ.agent(), summ.degree, timer.duration,
                                    mons.is_abjurable(), !!(mons.flags & ~MF_PERSISTS));
        }

        if (create_monster(fauna_data))
        {
            created = true;
            mons.mangrove_pests--;
        }
    }

    if (created && you.can_see(mons))
    {
        mprf("Angry insects surge out from beneath %s foliage!",
             mons.name(DESC_ITS).c_str());
    }
}

static bool _adj_to_tree(coord_def p)
{
    for (adjacent_iterator ai(p); ai; ++ai)
        if (feat_is_tree(env.grid(*ai)))
            return true;
    return false;
}

static coord_def _find_nearer_tree(coord_def cur_loc, coord_def target)
{
    coord_def p = {0, 0};
    int seen = 0;
    // don't bother teleporting to something that's at the same distance
    // from the target as you already are
    int closest = grid_distance(cur_loc, target) - 1;
    for (distance_iterator di(target); di; ++di)
    {
        const int dist = grid_distance(target, *di);
        if (dist > closest)
            break;

        if (!cell_see_cell(target, *di, LOS_NO_TRANS) // there might be a better iterator
            || !_adj_to_tree(*di)
            || !monster_habitable_grid(MONS_ELEIONOMA, *di))
        {
            continue;
        }
        // XXX: also check for dangerous clouds?

        closest = dist;

        seen++;
        if (x_chance_in_y(1, seen))
            p = *di;
    }
    return p;
}

static void _weeping_skull_cloud_aura(monster* mons)
{
    actor *foe = mons->get_foe();
    if (!foe || !mons->can_see(*foe))
        return;

    // Generate list of valid cloud spots.
    vector<coord_def> pos;

    for (radius_iterator ri(mons->pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (grid_distance(mons->pos(), *ri) > 2)
            continue;

        if (!feat_is_solid(env.grid(*ri)) && !actor_at(*ri) && !cloud_at(*ri))
            pos.push_back(*ri);
    }

    shuffle_array(pos);

    int num_clouds = min((int)pos.size(), random_range(1, 3));
    for (int i = 0; i < num_clouds; ++i)
        place_cloud(CLOUD_MISERY, pos[i], random2(3) + 2, mons);
}

static void _seismosaurus_egg_hatch(monster* mons)
{
    mon_enchant hatch = mons->get_ench(ENCH_HATCHING);
    hatch.duration -= 1;

    if (hatch.duration  == 4)
    {
        simple_monster_message(*mons, " cracks slightly.");
        mons->number = 1;
    }
    else if (hatch.duration  == 2)
    {
        simple_monster_message(*mons, " shakes eagerly.");
        mons->number = 2;
    }
    // Hatching time!
    else if (hatch.duration  == 0)
    {
        simple_monster_message(*mons, " hatches with a roar like a landslide!",
                                false, MSGCH_MONSTER_SPELL);

        const int old_hd = mons->get_experience_level();
        change_monster_type(mons, MONS_SEISMOSAURUS, true);
        mons->heal(mons->max_hit_points);
        mons->set_hit_dice(old_hd);

        mon_enchant timer = mons->get_ench(ENCH_SUMMON_TIMER);
        timer.duration = random_range(40, 55) * BASELINE_DELAY;
        mons->update_ench(timer);
        mons->del_ench(ENCH_HATCHING);

        // Immediately stomp if anything is in range
        mons->speed_increment = 80;
        try_mons_cast(*mons, SPELL_SEISMIC_STOMP);

        return;
    }

    mons->update_ench(hatch);
}

static inline void _mons_cast_abil(monster* mons, bolt &pbolt,
                                   spell_type spell_cast)
{
    mons_cast(mons, pbolt, spell_cast, MON_SPELL_NATURAL);
}

bool mon_special_ability(monster* mons)
{
    bool used = false;

    const monster_type mclass = (mons_genus(mons->type) == MONS_DRACONIAN)
                                  ? draconian_subspecies(*mons)
                                  : mons->type;

    // Slime creatures can split while out of sight.
    if ((!mons->near_foe() || mons->asleep())
         && mons->type != MONS_SLIME_CREATURE
         && mons->type != MONS_SEISMOSAURUS_EGG)
    {
        return false;
    }

    switch (mclass)
    {
    case MONS_UGLY_THING:
    case MONS_VERY_UGLY_THING:
        // A (very) ugly thing may mutate if it's next to other ones (or
        // next to you if you're contaminated).
        used = ugly_thing_mutate(*mons, false);
        break;

    case MONS_SLIME_CREATURE:
        // Slime creatures may split or merge depending on the
        // situation.
        used = _slime_split_merge(mons);
        if (!mons->alive())
            return true;
        break;

    case MONS_BALL_LIGHTNING:
        if (is_sanctuary(mons->pos()))
            break;

        if (mons->attitude == ATT_HOSTILE
            && grid_distance(you.pos(), mons->pos()) <= 2)
        {
            mons->suicide();
            used = true;
            break;
        }

        for (monster_near_iterator targ(mons, LOS_NO_TRANS); targ; ++targ)
        {
            if (mons_aligned(mons, *targ) || targ->is_firewood()
                || grid_distance(mons->pos(), targ->pos()) > 2
                || !you.see_cell(targ->pos()))
            {
                continue;
            }

            if (!cell_is_solid(targ->pos()))
            {
                mons->suicide();
                used = true;
                break;
            }
        }
        break;

    case MONS_FOXFIRE:
        if (is_sanctuary(mons->pos()))
            break;

        if (mons->attitude == ATT_HOSTILE
            && grid_distance(you.pos(), mons->pos()) == 1)
        {
            foxfire_attack(mons, &you);
            check_place_cloud(CLOUD_FLAME, mons->pos(), 2, mons);
            if (mons->alive())
                monster_die(*mons, KILL_RESET, NON_MONSTER, true);
            used = true;
            break;
        }

        for (monster_near_iterator targ(mons, LOS_NO_TRANS); targ; ++targ)
        {
            if (mons_aligned(mons, *targ) || targ->is_firewood()
                || grid_distance(mons->pos(), targ->pos()) > 1
                || !you.see_cell(targ->pos()))
            {
                continue;
            }

            if (!cell_is_solid(targ->pos()))
            {
                foxfire_attack(mons, *targ);
                if (mons->alive())
                    monster_die(*mons, KILL_RESET, NON_MONSTER, true);
                used = true;
                break;
            }
        }
        break;

    case MONS_STARCURSED_MASS:
        if (x_chance_in_y(mons->blob_size,8) && x_chance_in_y(2,3)
            && mons->hit_points >= 8)
        {
            _starcursed_split(mons), used = true;
        }

        if (!mons_is_confused(*mons)
                && !is_sanctuary(mons->pos()) && !is_sanctuary(mons->target)
                && _will_starcursed_scream(mons)
                && coinflip())
        {
            _starcursed_scream(mons, actor_at(mons->target));
            used = true;
        }
        break;

    case MONS_THORN_HUNTER:
    {
        // If we would try to move into a briar (that we might have just created
        // defensively), let's see if we can shoot our foe through it instead
        if (actor_at(mons->pos() + mons->props[MMOV_KEY].get_coord())
            && actor_at(mons->pos() + mons->props[MMOV_KEY].get_coord())->type == MONS_BRIAR_PATCH
            && !one_chance_in(3))
        {
            actor *foe = mons->get_foe();
            if (foe && mons->can_see(*foe))
            {
                bolt beem = setup_targeting_beam(*mons);
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
        else if (mons->props[FOE_APPROACHING_KEY].get_bool() == true
                 && !mons_is_confused(*mons)
                 && coinflip())
        {
            int briar_count = 0;
            for (monster_near_iterator mi(mons, LOS_NO_TRANS); mi; ++mi)
            {
                if (mi->type == MONS_BRIAR_PATCH
                    && grid_distance(mons->pos(), mi->pos()) > 3)
                {
                    briar_count++;
                }
            }
            if (briar_count < 4) // Probably no solid wall here
            {
                bolt beem; // unused
                _mons_cast_abil(mons, beem, SPELL_WALL_OF_BRAMBLES);
                used = true;
            }
        }
    }
    break;

    case MONS_WATER_NYMPH:
    case MONS_NORRIS:
    {
        if (!one_chance_in(5))
            break;

        actor *foe = mons->get_foe();
        if (!foe || !mons->can_see(*foe) || feat_is_water(env.grid(foe->pos())))
            break;

        const coord_def targ = foe->pos();
        coord_def spot;
        if (!find_habitable_spot_near(targ, MONS_ELECTRIC_EEL, 3, spot)
            || targ.distance_from(spot) >= targ.distance_from(mons->pos()))
        {
            break;
        }

        if (mons->move_to_pos(spot))
        {
            simple_monster_message(*mons, " flows with the water.");
            used = true;
        }
    }
    break;

    case MONS_ELEIONOMA:
    {
        if (!one_chance_in(3))
            break;

        actor *foe = mons->get_foe();
        if (!foe || !mons->can_see(*foe))
            break;

        const int dist = grid_distance(foe->pos(), mons->pos());
        if (dist < 3)
            break;

        const coord_def target = _find_nearer_tree(mons->pos(), foe->pos());
        if (target.origin() || !mons->move_to_pos(target))
            break;

        simple_monster_message(*mons, " flows through the trees.");
        used = true;
    }
    break;

    case MONS_SHAMBLING_MANGROVE:
    {
        if (mons->hit_points * 2 < mons->max_hit_points
            && mons->mangrove_pests > 0)
        {
            treant_release_fauna(*mons);
            // Intentionally takes no energy; the creatures are flying free
            // on their own time.
        }
    }
    break;

    case MONS_WEEPING_SKULL:
        _weeping_skull_cloud_aura(mons);
        break;

    case MONS_SEISMOSAURUS_EGG:
        if (egg_is_incubating(*mons))
        {
            _seismosaurus_egg_hatch(mons);
            used = true;
        }
        break;

    default:
        break;
    }

    if (used)
        mons->lose_energy(EUT_SPECIAL);

    return used;
}

bool egg_is_incubating(const monster& egg)
{
    if (!egg.has_ench(ENCH_HATCHING))
        return false;

    mon_enchant hatch = egg.get_ench(ENCH_HATCHING);

    // Check if we're near our 'parent'
    const actor* parent = hatch.agent();
    if (!parent || !adjacent(parent->pos(), egg.pos()))
        return false;

    // Finally, check that there are foes sufficiently nearby (and also in the
    // parent's LoS)
    for (monster_near_iterator mi(&egg, LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_aligned(*mi, &egg) && !mi->is_firewood()
            && grid_distance(egg.pos(), mi->pos()) <= 4
            && parent->see_cell(mi->pos()))
        {
            return true;
        }
    }

    return false;
}
