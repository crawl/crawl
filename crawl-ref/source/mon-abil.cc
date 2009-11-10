/*
 *  File:       mon-abil.cc
 *  Summary:    Monster abilities.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"
#include "mon-abil.h"

#include "externs.h"
#include "options.h"

#ifdef TARGET_OS_DOS
#include <conio.h>
#endif

#include "arena.h"
#include "beam.h"
#include "colour.h"
#include "directn.h"
#include "fprop.h"
#include "ghost.h"
#include "misc.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "monplace.h"
#include "monspeak.h"
#include "monstuff.h"
#include "random.h"
#include "spl-mis.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "view.h"
#include "shout.h"
#include "viewchar.h"

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
static void _stats_from_blob_count(monsters *slime, float hp_per_blob)
{
    slime->max_hit_points = (int)(slime->number * hp_per_blob);
    slime->hit_points = slime->max_hit_points;
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

    if (you.can_see(thing))
        mprf("%s splits.", thing->name(DESC_CAP_A).c_str());

    int split_off = thing->number / 2;
    float hp_per_blob = thing->max_hit_points / float(thing->number);

    thing->number -= split_off;
    new_slime->number = split_off;

    new_slime->hit_dice = thing->hit_dice;

    _stats_from_blob_count(thing, hp_per_blob);
    _stats_from_blob_count(new_slime, hp_per_blob);

    if (crawl_state.arena)
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
    merge_to->hit_points += initial_slime->max_hit_points;

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

        you.flash_colour = LIGHTGREEN;
        viewwindow(false);

        int flash_delay = 150;
        // Scale delay to match change in arena_delay.
        if (crawl_state.arena)
        {
            flash_delay *= Options.arena_delay;
            flash_delay /= 600;
        }

        delay(flash_delay);
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
     return (thing->asleep()
            || mons_is_wandering(thing)
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

// See if slime creature 'thing' can split, and carry out the split if
// we can find a square to place the new slime creature on.
static bool _slime_split(monsters *thing)
{
    if (!thing
        || !_unoccupied_slime(thing)
        || _disabled_slime(thing)
        || thing->number <= 1)
    {
        return (false);
    }

    int compass_idx[] = {0, 1, 2, 3, 4, 5, 6, 7};
    std::random_shuffle(compass_idx, compass_idx + 8);
    coord_def origin = thing->pos();

    // Anywhere we can place an offspring?
    for (int i = 0; i < 8; ++i)
    {
        coord_def target = origin + Compass[compass_idx[i]];

        if (mons_class_can_pass(MONS_SLIME_CREATURE, env.grid(target))
            && !actor_at(target))
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
    bool do_resist = (you.attribute[ATTR_HELD] || you.check_res_magic(70));

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
                SAME_ATTITUDE(mons), 5, 0, foe->pos(), mons->foe));
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

        MiscastEffect(foe, monster_index(mons), SPTYP_DIVINATION,
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
        const int boss_index = monster_index(chief);
        const int level = chief->hit_dice > 12? 2 : 1;
        std::vector<monsters*> seen_affected;
        for (int i = 0; i < MAX_MONSTERS; ++i)
        {
            monsters *mon = &menv[i];
            if (mon != chief
                && mon->alive()
                && mons_species(mon->type) == MONS_ORC
                && mons_aligned(boss_index, i)
                && mon->hit_dice < chief->hit_dice
                && !mon->berserk()
                && !mon->has_ench(ENCH_MIGHT)
                && !mon->cannot_move()
                && !mon->confused()
                && chief->can_see(mon))
            {
                mon_enchant ench = mon->get_ench(ENCH_BATTLE_FRENZY);
                if (ench.ench == ENCH_NONE || ench.degree < level)
                {
                    const int dur =
                        random_range(12, 20) * speed_to_duration(mon->speed);

                    if (ench.ench != ENCH_NONE)
                    {
                        ench.degree   = level;
                        ench.duration = std::max(ench.duration, dur);
                        mon->update_ench(ench);
                    }
                    else
                    {
                        mon->add_ench(mon_enchant(ENCH_BATTLE_FRENZY, level,
                                                  KC_OTHER, dur));
                    }

                    affected++;
                    if (you.can_see(mon))
                        seen_affected.push_back(mon);

                    if (mon->asleep())
                        behaviour_event(mon, ME_DISTURB, MHITNOT, chief->pos());
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
            noisy(15, chief->pos(), chief->mindex());

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
    for (int i = 0; i < MAX_MONSTERS; ++i)
    {
        monsters *targ = &menv[i];
        if (targ == mon || !targ->alive() || !targ->needs_berserk())
            continue;

        if (mon->pos().distance_from(targ->pos()) > 3)
            continue;

        if (is_sanctuary(targ->pos()))
            continue;

        // Cannot goad other moths of wrath!
        if (targ->type == MONS_MOTH_OF_WRATH)
            continue;

        if (_make_monster_angry(mon, targ) && !one_chance_in(3 * ++goaded))
            return (true);
    }

    return (false);
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
    if ((!mons_near(monster)
         || monster->asleep()
         || monster->submerged())
             && monster->type != MONS_SLIME_CREATURE)
    {
        return (false);
    }

    const msg_channel_type spl = (monster->friendly() ? MSGCH_FRIEND_SPELL
                                                         : MSGCH_MONSTER_SPELL);

    spell_type spell = SPELL_NO_SPELL;

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

        for (int i = 0; i < MAX_MONSTERS; i++)
        {
            monsters *targ = &menv[i];

            if (targ->type == MONS_NO_MONSTER)
                continue;

            if (distance(monster->pos(), targ->pos()) >= 5)
                continue;

            if (mons_atts_aligned(monster->attitude, targ->attitude))
                continue;

            // Faking LOS by checking the neighbouring square.
            coord_def diff = targ->pos() - monster->pos();
            coord_def sg(sgn(diff.x), sgn(diff.y));
            coord_def t = monster->pos() + sg;

            if (!in_bounds(t))
                continue;

            if (!feat_is_solid(grd(t)))
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
        beem.type        = dchar_glyph(DCHAR_FIRED_ZAP);
        beem.flavour     = BEAM_LAVA;
        beem.beam_source = monster_index(monster);
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
        beem.type        = dchar_glyph(DCHAR_FIRED_ZAP);
        beem.flavour     = BEAM_ELECTRICITY;
        beem.beam_source = monster_index(monster);
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
        if (monster->has_ench(ENCH_CONFUSION))
            break;

        if (player_or_mon_in_sanct(monster))
            break;

        if (one_chance_in(3))
        {
            spell = SPELL_ACID_SPLASH;
            setup_mons_cast(monster, beem, spell);

            // Fire tracer.
            fire_tracer(monster, beem);

            // Good idea?
            if (mons_should_fire(beem))
            {
                make_mons_stop_fleeing(monster);
                mons_cast(monster, beem, spell);
                used = true;
            }
        }
        break;

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
                    mons_cast(monster, beem, spell_cast);
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

                    mons_cast(monster, beem, spell_cast);
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
        beem.beam_source = monster_index(monster);
        beem.type        = dchar_glyph(DCHAR_FIRED_MISSILE);
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
    case MONS_FIREDRAKE:
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
                mons_cast(monster, beem, spell);
                used = true;
            }
        }
        break;

    case MONS_MERMAID:
    case MONS_SIREN:
    {
        // Don't behold observer in the arena.
        if (crawl_state.arena)
            break;

        // Don't behold player already half down or up the stairs.
        if (!you.delay_queue.empty())
        {
            delay_queue_item delay = you.delay_queue.front();

            if (delay.type == DELAY_ASCENDING_STAIRS
                || delay.type == DELAY_DESCENDING_STAIRS)
            {
#ifdef DEBUG_DIAGNOSTICS
                mpr("Taking stairs, don't mesmerise.", MSGCH_DIAGNOSTICS);
#endif
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
            noisy(12, monster->pos(), monster->mindex(), true);

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
bool _eyeball_will_use_ability (monsters *monster)
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

#define MON_SPEAK_CHANCE 21

    if (monster->is_patrolling() || mons_is_wandering(monster)
        || monster->attitude == ATT_NEUTRAL)
    {
        // Very fast wandering/patrolling monsters might, in one monster turn,
        // move into the player's LOS and then back out (or the player
        // might move into their LOS and the monster move back out before
        // the player's view has a chance to update) so prevent them
        // from speaking.
        ;
    }
    else if ((mons_class_flag(monster->type, M_SPEAKS)
                    || !monster->mname.empty())
                && one_chance_in(MON_SPEAK_CHANCE))
    {
        mons_speaks(monster);
    }
    else if (get_mon_shape(monster) >= MON_SHAPE_QUADRUPED)
    {
        // Non-humanoid-ish monsters have a low chance of speaking
        // without the M_SPEAKS flag, to give the dungeon some
        // atmosphere/flavour.
        int chance = MON_SPEAK_CHANCE * 4;

        // Band members are a lot less likely to speak, since there's
        // a lot of them.
        if (testbits(monster->flags, MF_BAND_MEMBER))
            chance *= 10;

        // However, confused and fleeing monsters are more interesting.
        if (mons_is_fleeing(monster))
            chance /= 2;
        if (monster->has_ench(ENCH_CONFUSION))
            chance /= 2;

        if (one_chance_in(chance))
            mons_speaks(monster);
    }
    // Okay then, don't speak.

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
            if (you.can_see(monster) && you.can_see(foe))
                mprf("%s blinks at %s.",
                     monster->name(DESC_CAP_THE).c_str(),
                     foe->name(DESC_NOCAP_THE).c_str());

            int confuse_power = 2 + random2(3);

            if (foe->check_res_magic((monster->hit_dice * 5) * confuse_power))
            {
                if (foe->atype() == ACT_PLAYER)
                    canned_msg(MSG_YOU_RESIST);
                else if (foe->atype() == ACT_MONSTER)
                {
                    const monsters *foe_mons = dynamic_cast<const monsters*>(foe);
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
            if (you.can_see(monster) && you.can_see(foe))
                mprf("%s stares at %s.",
                     monster->name(DESC_CAP_THE).c_str(),
                     foe->name(DESC_NOCAP_THE).c_str());

            // Subtly different from old paralysis behaviour, but
            // it'll do.
            foe->paralyse(monster, 2 + random2(3));
        }
        break;

    case MONS_EYE_OF_DRAINING:
        if (_eyeball_will_use_ability(monster)
            && foe->atype() == ACT_PLAYER)
        {
            simple_monster_message(monster, " stares at you.");

            dec_mp(5 + random2avg(13, 3));

            heal_monster(monster, 10, true); // heh heh {dlb}
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


