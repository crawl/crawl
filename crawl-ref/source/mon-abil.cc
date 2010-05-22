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
#include "spl-mis.h"
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
#include <set>

bool ugly_thing_mutate(monsters *ugly, bool proximity)
{
    bool success = false;

    std::string src = "";

    unsigned char mon_colour = BLACK;

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
                monsters *ugly_near = monster_at(*ri);

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
static void _split_ench_durations(monsters *initial_slime, monsters *split_off)
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
static void _merge_ench_durations(monsters *initial_slime, monsters *merge_to)
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
        // If not, use duration 0 for their part of the average.
        int duration = temp.ench == ENCH_NONE ? 0 : temp.duration;

        i->second.duration = (i->second.duration * initial_count
                              + duration * merge_to_count)/total_count;

        if (!i->second.duration)
            i->second.duration = 1;

        merge_to->add_ench(i->second);
    }

    for (i = merge_to->enchantments.begin();
         i != merge_to->enchantments.end(); ++i)
    {
        if (initial_slime->enchantments.find(i->first)
                != initial_slime->enchantments.end()
            && i->second.duration > 1)
        {
            i->second.duration = (merge_to_count * i->second.duration)
                                  / total_count;

            merge_to->update_ench(i->second);
        }
    }
}

// Calculate slime creature hp based on how many are merged.
static void _stats_from_blob_count(monsters *slime, float max_per_blob,
                                   float current_per_blob)
{
    slime->max_hit_points = (int)(slime->number * max_per_blob);
    slime->hit_points = (int)(slime->number * current_per_blob);
}

// Create a new slime creature at 'target', and split 'thing''s hp and
// merge count with the new monster.
static bool _do_split(monsters *thing, coord_def & target)
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

    monsters *new_slime = &env.mons[slime_idx];

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
static bool _do_merge(monsters *initial_slime, monsters *merge_to)
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

    if(can_move)
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
static bool _unoccupied_slime(monsters *thing)
{
     return (thing->asleep() || mons_is_wandering(thing)
             || thing->foe == MHITNOT);
}

// Slime creatures cannot split or merge under these conditions.
static bool _disabled_slime(monsters *thing)
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
static bool _slime_merge(monsters *thing)
{
    if (!thing || _disabled_slime(thing) || _unoccupied_slime(thing))
        return (false);

    int max_slime_merge = 5;
    int compass_idx[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    std::random_shuffle(compass_idx, compass_idx + 8);
    coord_def origin = thing->pos();

    int target_distance = grid_distance(thing->target, thing->pos());

    monsters * merge_target = NULL;
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
        monsters *other_thing = monster_at(target);
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
static bool _slime_split(monsters *thing)
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
bool slime_split_merge(monsters *thing)
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
static bool _siren_movement_effect(const monsters *monster)
{
    bool do_resist = (you.attribute[ATTR_HELD] || you.check_res_magic(70)
                      || you.cannot_act() || you.asleep());

    if (!do_resist)
    {
        coord_def dir(coord_def(0,0));
        if (monster->pos().x < you.pos().x)
            dir.x = -1;
        else if (monster->pos().x > you.pos().x)
            dir.x = 1;
        if (monster->pos().y < you.pos().y)
            dir.y = -1;
        else if (monster->pos().y > you.pos().y)
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
            monsters *mon = monster_at(newpos);
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
                move_player_to_grid(newpos, true, true, true);

                if (swapping)
                    mon->apply_location_effects(newpos);
            }
        }
    }

    return (do_resist);
}

static bool _silver_statue_effects(monsters *mons)
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

static bool _orange_statue_effects(monsters *mons)
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

static bool _orc_battle_cry(monsters *chief)
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
        std::vector<monsters*> seen_affected;
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

static bool _make_monster_angry(const monsters *mon, monsters *targ)
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
        const monsters *vmons = &menv[targ->foe];
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

static bool _moth_incite_monsters(const monsters *mon)
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

static inline void _mons_cast_abil(monsters *monster, bolt &pbolt,
                                   spell_type spell_cast)
{
    mons_cast(monster, pbolt, spell_cast, true, true);
}

//---------------------------------------------------------------
//
// mon_special_ability
//
//---------------------------------------------------------------
bool mon_special_ability(monsters *monster, bolt & beem)
{
    bool used = false;

    const monster_type mclass = (mons_genus( monster->type ) == MONS_DRACONIAN)
                                  ? draco_subspecies( monster )
                                  : static_cast<monster_type>( monster->type );

    // Slime creatures can split while out of sight.
    if ((!monster->near_foe() || monster->asleep() || monster->submerged())
         && monster->type != MONS_SLIME_CREATURE)
    {
        return (false);
    }

    const msg_channel_type spl = (monster->friendly() ? MSGCH_FRIEND_SPELL
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
        used = ugly_thing_mutate(monster, true);
        break;

    case MONS_SLIME_CREATURE:
        // Slime creatures may split or merge depending on the
        // situation.
        used = slime_split_merge(monster);
        if (!monster->alive())
            return (true);
        break;

    case MONS_ORC_KNIGHT:
    case MONS_ORC_WARLORD:
    case MONS_SAINT_ROKA:
        if (is_sanctuary(monster->pos()))
            break;

        used = _orc_battle_cry(monster);
        break;

    case MONS_ORANGE_STATUE:
        if (player_or_mon_in_sanct(monster))
            break;

        used = _orange_statue_effects(monster);
        break;

    case MONS_SILVER_STATUE:
        if (player_or_mon_in_sanct(monster))
            break;

        used = _silver_statue_effects(monster);
        break;

    case MONS_BALL_LIGHTNING:
        if (is_sanctuary(monster->pos()))
            break;

        if (monster->attitude == ATT_HOSTILE
            && distance(you.pos(), monster->pos()) <= 5)
        {
            monster->hit_points = -1;
            used = true;
            break;
        }

        c = circle_def(monster->pos(), 4, C_CIRCLE);
        for (monster_iterator targ(&c); targ; ++targ)
        {
            if (mons_aligned(monster, *targ))
                continue;

            if (monster->can_see(*targ) && !feat_is_solid(grd(targ->pos())))
            {
                monster->hit_points = -1;
                used = true;
                break;
            }
        }
        break;

    case MONS_LAVA_SNAKE:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (!you.visible_to(monster))
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
        beem.beam_source = monster->mindex();
        beem.thrower     = KILL_MON;

        // Fire tracer.
        fire_tracer(monster, beem);

        // Good idea?
        if (mons_should_fire(beem))
        {
            make_mons_stop_fleeing(monster);
            simple_monster_message(monster, " spits lava!");
            beem.fire();
            used = true;
        }
        break;

    case MONS_ELECTRIC_EEL:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (!you.visible_to(monster))
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
        beem.beam_source = monster->mindex();
        beem.thrower     = KILL_MON;
        beem.is_beam     = true;

        // Fire tracer.
        fire_tracer(monster, beem);

        // Good idea?
        if (mons_should_fire(beem))
        {
            make_mons_stop_fleeing(monster);
            simple_monster_message(monster,
                                   " shoots out a bolt of electricity!");
            beem.fire();
            used = true;
        }
        break;

    case MONS_ACID_BLOB:
    case MONS_OKLOB_PLANT:
    case MONS_YELLOW_DRACONIAN:
    {
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (player_or_mon_in_sanct(monster))
            break;

        bool spit = one_chance_in(3);
        if (monster->type == MONS_OKLOB_PLANT)
            spit = x_chance_in_y(monster->hit_dice, 30);

        if (spit)
        {
            spell = SPELL_ACID_SPLASH;
            setup_mons_cast(monster, beem, spell);

            // Fire tracer.
            fire_tracer(monster, beem);

            // Good idea?
            if (mons_should_fire(beem))
            {
                make_mons_stop_fleeing(monster);
                _mons_cast_abil(monster, beem, spell);
                used = true;
            }
        }
        break;
    }

    case MONS_MOTH_OF_WRATH:
        if (one_chance_in(3))
            used = _moth_incite_monsters(monster);
        break;

    case MONS_SNORG:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (monster->foe == MHITNOT
            || monster->foe == MHITYOU && monster->friendly())
        {
            break;
        }

        // There's a 5% chance of Snorg spontaneously going berserk that
        // increases to 20% once he is wounded.
        if (monster->hit_points == monster->max_hit_points && !one_chance_in(4))
            break;

        if (one_chance_in(5))
            monster->go_berserk(true);
        break;

    case MONS_PIT_FIEND:
        if (one_chance_in(3))
            break;
        // deliberate fall through
    case MONS_FIEND:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (player_or_mon_in_sanct(monster))
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
                if (!monster->friendly())
                {
                    make_mons_stop_fleeing(monster);
                    spell_cast = SPELL_SYMBOL_OF_TORMENT;
                    _mons_cast_abil(monster, beem, spell_cast);
                    used = true;
                    break;
                }
                // deliberate fallthrough -- see above
            case 1:
            case 2:
            case 3:
                spell_cast = SPELL_HELLFIRE;
                setup_mons_cast(monster, beem, spell_cast);

                // Fire tracer.
                fire_tracer(monster, beem);

                // Good idea?
                if (mons_should_fire(beem))
                {
                    make_mons_stop_fleeing(monster);

                    _mons_cast_abil(monster, beem, spell_cast);
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
        if (one_chance_in(7) || monster->caught() && one_chance_in(3))
            used = monster_blink(monster);
        break;

    case MONS_MANTICORE:
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (!you.visible_to(monster))
            break;

        // The fewer spikes the manticore has left, the less
        // likely it will use them.
        if (random2(16) >= static_cast<int>(monster->number))
            break;

        // Do the throwing right here, since the beam is so
        // easy to set up and doesn't involve inventory.

        // Set up the beam.
        beem.name        = "volley of spikes";
        beem.aux_source  = "volley of spikes";
        beem.range       = 6;
        beem.hit         = 14;
        beem.damage      = dice_def( 2, 10 );
        beem.beam_source = monster->mindex();
        beem.glyph       = dchar_glyph(DCHAR_FIRED_MISSILE);
        beem.colour      = LIGHTGREY;
        beem.flavour     = BEAM_MISSILE;
        beem.thrower     = KILL_MON;
        beem.is_beam     = false;

        // Fire tracer.
        fire_tracer(monster, beem);

        // Good idea?
        if (mons_should_fire(beem))
        {
            make_mons_stop_fleeing(monster);
            simple_monster_message(monster, " flicks its tail!");
            beem.fire();
            used = true;
            // Decrement # of volleys left.
            monster->number--;
        }
        break;

    case MONS_PLAYER_GHOST:
    {
        const ghost_demon &ghost = *(monster->ghost);

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

        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (!you.visible_to(monster))
            break;

        if (monster->type != MONS_HELL_HOUND && x_chance_in_y(3, 13)
            || one_chance_in(10))
        {
            setup_mons_cast(monster, beem, spell);

            // Fire tracer.
            fire_tracer(monster, beem);

            // Good idea?
            if (mons_should_fire(beem))
            {
                make_mons_stop_fleeing(monster);
                _mons_cast_abil(monster, beem, spell);
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
        if (monster->has_ench(ENCH_CONFUSION)
            || mons_is_fleeing(monster)
            || monster->pacified()
            || monster->friendly()
            || !player_can_hear(monster->pos()))
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
        if (monster->invisible()
            && monster->hit_points <= monster->max_hit_points / 2
            && !one_chance_in(3))
        {
            break;
        }

        bool already_mesmerised = you.beheld_by(monster);

        if (one_chance_in(5)
            || monster->foe == MHITYOU && !already_mesmerised && coinflip())
        {
            noisy(LOS_RADIUS, monster->pos(), monster->mindex(), true);

            bool did_resist = false;
            if (you.can_see(monster))
            {
                simple_monster_message(monster,
                    make_stringf(" chants %s song.",
                    already_mesmerised ? "her luring" : "a haunting").c_str(),
                    spl);

                if (monster->type == MONS_SIREN)
                {
                    if (_siren_movement_effect(monster))
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

            you.add_beholder(monster);

            used = true;
        }
        break;
    }

    default:
        break;
    }

    if (used)
        monster->lose_energy(EUT_SPECIAL);

    return (used);
}

// Combines code using in Confusing Eye, Giant Eye and Eye of Draining to
// reduce clutter.
bool _eyeball_will_use_ability(monsters *monster)
{
    return (coinflip()
        && !mons_is_wandering(monster)
        && !mons_is_fleeing(monster)
        && !monster->pacified()
        && !player_or_mon_in_sanct(monster));
}

//---------------------------------------------------------------
//
// mon_nearby_ability
//
// Gives monsters a chance to use a special ability when they're
// next to the player.
//
//---------------------------------------------------------------
void mon_nearby_ability(monsters *monster)
{
    actor *foe = monster->get_foe();
    if (!foe
        || !monster->can_see(foe)
        || monster->asleep()
        || monster->submerged())
    {
        return;
    }

    maybe_mons_speaks(monster);

    if (monster_can_submerge(monster, grd(monster->pos()))
        && !monster->caught()             // No submerging while caught.
        && !you.beheld_by(monster) // No submerging if player entranced.
        && !mons_is_lurking(monster)  // Handled elsewhere.
        && monster->wants_submerge())
    {
        monsterentry* entry = get_monster_data(monster->type);

        monster->add_ench(ENCH_SUBMERGED);
        monster->speed_increment -= ENERGY_SUBMERGE(entry);
        return;
    }

    switch (monster->type)
    {
    case MONS_SPATIAL_VORTEX:
    case MONS_KILLER_KLOWN:
        // Choose random colour.
        monster->colour = random_colour();
        break;

    case MONS_GOLDEN_EYE:
        if (_eyeball_will_use_ability(monster))
        {
            const bool can_see = you.can_see(monster);
            if (can_see && you.can_see(foe))
                mprf("%s blinks at %s.",
                     monster->name(DESC_CAP_THE).c_str(),
                     foe->name(DESC_NOCAP_THE).c_str());

            int confuse_power = 2 + random2(3);

            if (foe->atype() == ACT_PLAYER && !can_see)
                mpr("You feel you are being watched by something.");

            if (foe->check_res_magic((monster->hit_dice * 5) * confuse_power))
            {
                if (foe->atype() == ACT_PLAYER)
                    canned_msg(MSG_YOU_RESIST);
                else if (foe->atype() == ACT_MONSTER)
                {
                    const monsters *foe_mons = foe->as_monster();
                    simple_monster_message(foe_mons, mons_resist_string(foe_mons));
                }
                break;
            }

            foe->confuse(monster, 2 + random2(3));
        }
        break;

    case MONS_GIANT_EYEBALL:
        if (_eyeball_will_use_ability(monster))
        {
            const bool can_see = you.can_see(monster);
            if (can_see && you.can_see(foe))
                mprf("%s stares at %s.",
                     monster->name(DESC_CAP_THE).c_str(),
                     foe->name(DESC_NOCAP_THE).c_str());

            if (foe->atype() == ACT_PLAYER && !can_see)
                mpr("You feel you are being watched by something.");

            // Subtly different from old paralysis behaviour, but
            // it'll do.
            foe->paralyse(monster, 2 + random2(3));
        }
        break;

    case MONS_EYE_OF_DRAINING:
        if (_eyeball_will_use_ability(monster) && foe->atype() == ACT_PLAYER)
        {
            if (you.can_see(monster))
                simple_monster_message(monster, " stares at you.");
            else
                mpr("You feel you are being watched by something.");

            int mp = std::min(5 + random2avg(13, 3), you.magic_points);
            dec_mp(mp);

            monster->heal(mp, true); // heh heh {dlb}
        }
        break;

    case MONS_AIR_ELEMENTAL:
        if (one_chance_in(5))
            monster->add_ench(ENCH_SUBMERGED);
        break;

    case MONS_PANDEMONIUM_DEMON:
        if (monster->ghost->cycle_colours)
            monster->colour = random_colour();
        break;

    default:
        break;
    }
}

// When giant spores move maybe place a ballistomycete on the they move
// off of.
void ballisto_on_move(monsters * monster, const coord_def & position)
{
    if (monster->type == MONS_GIANT_SPORE
        && !monster->is_summoned())
    {
        dungeon_feature_type ftype = env.grid(monster->pos());

        if (ftype >= DNGN_FLOOR_MIN && ftype <= DNGN_FLOOR_MAX)
            env.pgrid(monster->pos()) |= FPROP_MOLD;

        // The number field is used as a cooldown timer for this behavior.
        if (monster->number <= 0)
        {
            if (one_chance_in(4))
            {
                beh_type attitude = actual_same_attitude(*monster);
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
                    env.pgrid(position) &= ~FPROP_MOLD;
                    if  (you.can_see(&env.mons[rc]))
                        mprf("A ballistomycete grows in the wake of the spore.");
                }

                monster->number = 40;
            }
        }
        else
        {
            monster->number--;
        }

    }
}

struct position_node
{
    position_node()
    {
        pos.x=0;
        pos.y=0;
        last = NULL;
    }

    coord_def pos;
    const position_node * last;
    bool operator < (const position_node & right) const
    {
        unsigned idx = pos.x + pos.y * X_WIDTH;
        unsigned other_idx = right.pos.x + right.pos.y * X_WIDTH;
        return  idx < other_idx;
    }
};

bool _ballisto_at(const coord_def & target)
{
    monsters * mons = monster_at(target);
    return (mons && mons ->type == MONS_BALLISTOMYCETE
            && mons->alive());
}

bool _player_at(const coord_def & target)
{
    return (you.pos() == target);
}

// If 'monster' is a ballistomycete or spore, activate some number of
// ballistomycetes on the level.
void activate_ballistomycetes(monsters * monster, const coord_def & origin,
                              bool player_kill)
{
    if (!monster || monster->is_summoned()
                 || monster->type != MONS_BALLISTOMYCETE
                    && monster->type != MONS_GIANT_SPORE)
    {
        return;
    }

    // If a spore or inactive ballisto died we will only activate one
    // other ballisto. If it was an active ballisto we will distribute
    // its count to others on the level.
    int activation_count = 1;
    if (monster->type == MONS_BALLISTOMYCETE)
        activation_count += monster->number;

    int spore_count = 0;
    int ballisto_count = 0;

    bool any_friendly = monster->attitude == ATT_FRIENDLY;
    bool fedhas_mode  = false;
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi->mindex() != monster->mindex() && mi->alive())
        {
            if (mi->type == MONS_BALLISTOMYCETE)
                ballisto_count++;
            else if (mi->type == MONS_GIANT_SPORE)
                spore_count++;

            if (mi->attitude == ATT_FRIENDLY)
                any_friendly = true;
        }
    }

    bool exhaustive = true;
    bool (*valid_target)(const coord_def & ) = _ballisto_at;
    position_node temp_node;
    temp_node.pos = origin;
    temp_node.last = NULL;

    std::set<position_node> visited;
    std::vector<std::set<position_node>::iterator > candidates;
    std::queue<std::set<position_node>::iterator > fringe;

    std::set<position_node>::iterator current = visited.insert(temp_node).first;
    fringe.push(current);

    if (you.religion == GOD_FEDHAS)
    {
        if (spore_count == 0
            && ballisto_count == 0
            && any_friendly
            && monster->type == MONS_BALLISTOMYCETE)
        {
            mprf("Your fungal colony was destroyed.");
            dock_piety(5, 0);
        }

        fedhas_mode = true;
        activation_count = 1;
        exhaustive = false;
        valid_target = _player_at;
    }

    while (!fringe.empty())
    {
        current = fringe.front();
        fringe.pop();

        if (valid_target(current->pos))
        {
            candidates.push_back(current);
            if (!exhaustive)
                break;
        }

        for (adjacent_iterator adj_it(current->pos); adj_it; ++adj_it)
        {
            if (in_bounds(*adj_it)
                && (is_moldy(*adj_it)
                    || _ballisto_at(*adj_it)))
            {
                temp_node.pos = *adj_it;
                temp_node.last = &(*current);

                std::pair<std::set<position_node>::iterator, bool > res;
                res = visited.insert(temp_node);
                if (res.second)
                    fringe.push(res.first);
            }
        }
    }

    if (candidates.empty())
    {
        if (player_kill
            && !fedhas_mode
            && spore_count == 0
            && ballisto_count == 0
            && monster->attitude == ATT_HOSTILE)
        {
            mprf("Having destroyed the fungal colony, you feel a bit more "
                 "experienced.");
            gain_exp(500);

            // Get rid of the mold, so it'll be more useful when new fungi
            // spawn.
            // NOTE: Not triggered if eradication happens by hostile monster,
            //       so we don't give anything away. (jpeg)
            for (rectangle_iterator ri(1); ri; ++ri)
                if (is_moldy(*ri))
                    env.pgrid(*ri) &= ~(FPROP_MOLD);
        }

        return;
    }

    // A (very) soft cap on colony growth, no activations if there are
    // already a lot of ballistos on level.
    if (candidates.size() > 25)
        return;


    std::random_shuffle(candidates.begin(), candidates.end());

    int index = 0;
    you.mold_colour = LIGHTRED;

    bool draw = false;
    for (int i=0; i<activation_count; ++i)
    {
        index = i % candidates.size();

        monsters * spawner = monster_at(candidates[index]->pos);

        // This may be the players position, in which case we don't
        // have to mess with spore production on anything
        if (spawner && !fedhas_mode)
        {
            spawner->number++;

            // Change color and start the spore production timer if we
            // are moving from 0 to 1.
            if (spawner->number == 1)
            {
                spawner->colour = LIGHTRED;
                // Reset the spore production timer.
                spawner->del_ench(ENCH_SPORE_PRODUCTION, false);
                spawner->add_ench(ENCH_SPORE_PRODUCTION);
            }
        }

        const position_node * thread = &(*candidates[index]);
        while(thread)
        {
            if (you.see_cell(thread->pos))
            {
                view_update_at(thread->pos);
                draw = true;
            }

            thread = thread->last;
        }
    }

    if (draw)
    {
        viewwindow(false, false);
        int sp_delay = 150;

        // Scale delay to match change in arena_delay.
        if (crawl_state.game_is_arena())
        {
            sp_delay *= Options.arena_delay;
            sp_delay /= 600;
        }

        delay(sp_delay);
    }
}
