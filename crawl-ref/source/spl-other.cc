/**
 * @file
 * @brief Non-enchantment spells that didn't fit anywhere else.
 *           Mostly Transmutations.
**/

#include "AppHdr.h"

#include "spl-other.h"

#include "act-iter.h"
#include "beam.h"
#include "coordit.h"
#include "delay.h"
#include "env.h"
#include "god-companions.h"
#include "libutil.h"
#include "message.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-util.h"
#include "movement.h" // passwall
#include "place.h"
#include "potion.h"
#include "religion.h"
#include "spl-util.h"
#include "terrain.h"
#include "timed-effects.h"
#include "view.h"

spret cast_sublimation_of_blood(int pow, bool fail)
{
    bool success = false;

    if (you.duration[DUR_DEATHS_DOOR])
        mpr("You can't draw power from your own body while in death's door.");
    else if (!you.has_blood())
    {
        if (you.has_mutation(MUT_VAMPIRISM))
            mpr("You don't have enough blood to draw power from your own body.");
        else
            mpr("Your body is bloodless.");
    }
    else if (!enough_hp(2, true))
        mpr("Your attempt to draw power from your own body fails.");
    else
    {
        // Take at most 90% of currhp.
        const int minhp = max(div_rand_round(you.hp, 10), 1);

        while (you.magic_points < you.max_magic_points && you.hp > minhp)
        {
            fail_check();
            success = true;

            inc_mp(1);
            dec_hp(1, false);

            for (int i = 0; i < (you.hp > minhp ? 3 : 0); ++i)
                if (x_chance_in_y(6, pow))
                    dec_hp(1, false);

            if (x_chance_in_y(6, pow))
                break;
        }
        if (success)
            mpr("You draw magical energy from your own body!");
        else
            mpr("Your attempt to draw power from your own body fails.");
    }

    return success ? spret::success : spret::abort;
}

spret cast_death_channel(int pow, god_type god, bool fail)
{
    fail_check();
    mpr("Malign forces permeate your being, awaiting release.");

    you.increase_duration(DUR_DEATH_CHANNEL,
                          30 + random2(1 + div_rand_round(2 * pow, 3)), 200);

    if (god != GOD_NO_GOD)
        you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = static_cast<int>(god);

    return spret::success;
}

static bool _dismiss_dead()
{
    bool found = false;
    for (monster_iterator mi; mi; ++mi)
    {
        if (!*mi)
            continue;

        monster &mon = **mi;
        if (!mon.was_created_by(you, SPELL_ANIMATE_DEAD))
            continue;

        // crumble into dust...
        monster_die(mon, KILL_TIMEOUT, NON_MONSTER);
        found = true;
    }
    return found;
}

spret cast_animate_dead(int pow, bool fail)
{
    fail_check();

    if (_dismiss_dead())
        mpr("You dismiss your zombies and call upon the dead to rise afresh.");
    else
        mpr("You call upon the dead to rise.");

    you.increase_duration(DUR_ANIMATE_DEAD, 20 + random2(1 + pow), 100);
    you.props[ANIMATE_DEAD_POWER_KEY] = pow;

    return spret::success;
}

void do_player_recall(recall_t type)
{
    bool did_recall = false;

    // Search for recallable allies on your current floor
    for (monster_iterator mi; mi; ++mi)
    {
        if (!mons_is_recallable(&you, **mi))
            continue;

        if (type == recall_t::yred)
        {
            if (!(mi->holiness() & MH_UNDEAD))
                continue;
        }
        else if (type == recall_t::beogh)
        {
            if (!is_apostle_follower(**mi))
                continue;
        }

        if (try_recall(mi->mid))
            did_recall = true;
    }

    // Then search for recallable companions on any floor
    for (auto &entry : companion_list)
    {
        const int mid = entry.first;
        if (companion_is_elsewhere(mid, true))
        {
            if (try_recall(mid))
                did_recall = true;
        }
    }

    if (!did_recall)
        mpr("Nothing appears to have answered your call.");
}

// Remind a recalled ally (or one skipped due to proximity) not to run
// away or wander off.
void recall_orders(monster *mons)
{
    // FIXME: is this okay for berserk monsters? We still want them to
    // stick around...

    // Don't patrol
    mons->patrol_point = coord_def(0, 0);
    mons->travel_path.clear();

    // Don't wander
    mons->behaviour = BEH_SEEK;

    // Don't pursue distant enemies
    const actor *foe = mons->get_foe();
    if (foe && !you.can_see(*foe))
        mons->foe = MHITYOU;
}

// Attempt to recall a single monster by mid, which might be either on or off
// our current level. Returns whether this monster was successfully recalled.
bool try_recall(mid_t mid)
{
    monster* mons = monster_by_mid(mid);
    // Either it's dead or off-level.
    if (!mons)
        return recall_offlevel_ally(mid);
    if (!mons->alive())
        return false;
    // Don't recall monsters that are already close to the player
    if (mons->pos().distance_from(you.pos()) < 3
        && mons->see_cell_no_trans(you.pos()))
    {
        recall_orders(mons);
        return false;
    }
    coord_def empty;
    if (!find_habitable_spot_near(you.pos(), mons->type, 3, empty)
        || !mons->move_to_pos(empty))
    {
        return false;
    }
    recall_orders(mons);
    simple_monster_message(*mons, " is recalled.");
    mons->apply_location_effects(mons->pos());
    // mons may have been killed, shafted, etc,
    // but they were still recalled!
    return true;
}

static bool _feat_is_passwallable(dungeon_feature_type feat)
{
    // Worked stone walls are out, they're not diggable and
    // are used for impassable walls...
    switch (feat)
    {
    case DNGN_ROCK_WALL:
    case DNGN_SLIMY_WALL:
    case DNGN_CLEAR_ROCK_WALL:
        return true;
    default:
        return false;
    }
}

static bool _grid_is_passwallable(coord_def pos)
{
    return _feat_is_passwallable(env.grid(pos))
            && !is_temp_terrain(pos);
}

bool passwall_simplified_check(const actor &act)
{
    for (adjacent_iterator ai(act.pos(), true); ai; ++ai)
        if (_grid_is_passwallable(*ai))
            return true;
    return false;
}

passwall_path::passwall_path(const actor &act, const coord_def& dir, int max_range)
    : start(act.pos()), delta(dir.sgn()),
      range(max_range),
      dest_found(false)
{
    if (delta.zero())
        return;
    ASSERT(range > 0);
    coord_def pos;
    // TODO: something better than sgn for delta?
    for (pos = start + delta;
         (pos - start).rdist() - 1 <= range;
         pos += delta)
    {
        path.emplace_back(pos);
        if (in_bounds(pos))
        {
            if (!_grid_is_passwallable(pos))
            {
                if (!dest_found)
                {
                    actual_dest = pos;
                    dest_found = true;
                }
                if (env.map_knowledge(pos).feat() != DNGN_UNSEEN)
                    break;
            }
        }
        else if (!dest_found) // no destination in bounds
        {
            actual_dest = pos;
            dest_found = true;
            break; // can't render oob rays anyways, so no point in considering
                   // more than one of them
        }
    }
    // if dest_found is false, actual_dest is guaranteed to be out of bounds
}

/// max walls that there could be, given the player's knowledge and map bounds
int passwall_path::max_walls() const
{
    if (path.size() == 0)
        return 0;
    // the in_bounds check is in case the player is standing next to bounds
    return max((int) path.size() - 1, in_bounds(path.back()) ? 0 : 1);
}

/// actual walls (or max walls, if actual_dest is out of bounds)
int passwall_path::actual_walls() const
{
    return !in_bounds(actual_dest) ?
            max_walls() :
            (actual_dest - start).rdist() - 1;
}

bool passwall_path::spell_succeeds() const
{
    // this isn't really the full story -- since moveto needs to be checked
    // also.
    return actual_walls() > 0;
}

bool passwall_path::is_valid(string *fail_msg) const
{
    // does not check moveto cases, incl lava/deep water, since these prompt.
    if (delta.zero())
    {
        if (fail_msg)
            *fail_msg = "Please select a wall.";
        return false;
    }
    if (actual_walls() == 0)
    {
        if (fail_msg)
            *fail_msg = "There is no adjacent passable wall in that direction.";
        return false;
    }
    if (!dest_found)
    {
        if (fail_msg)
            *fail_msg = "This rock feels extremely deep.";
        return false;
    }
    if (!in_bounds(actual_dest))
    {
        if (fail_msg)
            *fail_msg = "You sense an overwhelming volume of rock.";
        return false;
    }
    const monster *mon = monster_at(actual_dest);
    if (cell_is_solid(actual_dest) || (mon && mon->is_stationary()))
    {
        if (fail_msg)
            *fail_msg = "Something is blocking your path through the rock.";
        return false;
    }
    return true;
}

/// find possible destinations, given the player's map knowledge
vector <coord_def> passwall_path::possible_dests() const
{
    // uses comparison to DNGN_UNSEEN so that this works sensibly with magic
    // mapping etc
    vector<coord_def> dests;
    for (auto p : path)
        if (!in_bounds(p) ||
            (env.map_knowledge(p).feat() == DNGN_UNSEEN || !cell_is_solid(p)))
        {
            dests.push_back(p);
        }
    return dests;
}

bool passwall_path::check_moveto() const
{
    // assumes is_valid()

    string terrain_msg;
    if (env.grid(actual_dest) == DNGN_DEEP_WATER)
        terrain_msg = "You sense a deep body of water on the other side of the rock.";
    else if (env.grid(actual_dest) == DNGN_LAVA)
        terrain_msg = "You sense an intense heat on the other side of the rock.";

    // Pre-confirm exclusions in unseen squares as well as the actual dest
    // even if seen, so that this doesn't leak information.

    // TODO: handle uncertainty in messaging for things other than exclusions

    return check_moveto_terrain(actual_dest, "passwall", terrain_msg)
        && check_moveto_exclusions(possible_dests(), "passwall")
        && check_moveto_cloud(actual_dest, "passwall")
        && check_moveto_trap(actual_dest, "passwall");

}

spret cast_passwall(const coord_def& c, int pow, bool fail)
{
    // prompt player to end position-based ice spells
    if (cancel_harmful_move(false))
        return spret::abort;

    // held away from the wall
    if (you.is_constricted())
        return spret::abort;

    coord_def delta = c - you.pos();
    passwall_path p(you, delta, spell_range(SPELL_PASSWALL, pow));
    string fail_msg;
    bool valid = p.is_valid(&fail_msg);
    if (!p.spell_succeeds())
    {
        if (fail_msg.size())
            mpr(fail_msg);
        return spret::abort;
    }

    fail_check();

    if (!valid)
    {
        if (fail_msg.size())
            mpr(fail_msg);
    }
    else if (p.check_moveto())
    {
        start_delay<PasswallDelay>(p.actual_walls() + 1, p.actual_dest);

        // Give bonus AC while moving through the wall.
        you.props[PASSWALL_ARMOUR_KEY].get_int() = 5 + div_rand_round(pow, 10);
        you.redraw_armour_class = true;

        return spret::success;
    }

    // at this point, the spell failed or was cancelled. Does it cost MP?
    vector<coord_def> dests = p.possible_dests();
    if (dests.size() == 0 ||
        (dests.size() == 1 && (!in_bounds(dests[0]) ||
        env.map_knowledge(dests[0]).feat() != DNGN_UNSEEN)))
    {
        // if there are no possible destinations, or only 1 that has been seen,
        // the player already had full knowledge. The !in_bounds case is if they
        // are standing next to the map edge, which is a leak of sorts, but
        // already apparent from the targeting (and we leak this info all over
        // the place, really).
        return spret::abort;
    }
    return spret::success;
}

static int _intoxicate_monsters(coord_def where, int pow, bool tracer)
{
    monster* mons = monster_at(where);
    if (mons == nullptr
        || mons_intel(*mons) < I_HUMAN
        || mons->clarity()
        || mons->res_poison() >= 3)
    {
        return 0;
    }

    if (tracer && !you.can_see(*mons))
        return 0;

    if (!tracer && monster_resists_this_poison(*mons))
        return 0;

    if (!tracer && x_chance_in_y(40 + div_rand_round(pow, 3), 100))
    {
        mons->add_ench(mon_enchant(ENCH_CONFUSION, 0, &you));
        simple_monster_message(*mons, " looks rather confused.");
        return 1;
    }
    // Just count affectable monsters for the tracer
    return tracer ? 1 : 0;
}

spret cast_intoxicate(int pow, bool fail, bool tracer)
{
    if (tracer)
    {
        const int work = apply_area_visible([] (coord_def where) {
            return _intoxicate_monsters(where, 0, true);
        }, you.pos());

        return work > 0 ? spret::success : spret::abort;
    }

    fail_check();
    mpr("You attempt to intoxicate your foes!");

    const int count = apply_area_visible([pow] (coord_def where) {
        return _intoxicate_monsters(where, pow, false);
    }, you.pos());

    if (count > 0)
    {
        mprf(MSGCH_WARN, "The world spins around you!");
        you.increase_duration(DUR_VERTIGO, 4 + count + random2(count + 1));
        you.redraw_evasion = true;
    }

    return spret::success;
}

vector<coord_def> find_sigil_locations(bool tracer)
{
    vector<coord_def> positions;
    for (radius_iterator ri(you.pos(), 2, C_SQUARE); ri; ++ri)
    {
        if (you.see_cell(*ri) && env.grid(*ri) == DNGN_FLOOR
            && (!actor_at(*ri) || (actor_at(*ri) && tracer
                                   && !you.can_see(*actor_at(*ri)))))
        {
            positions.push_back(*ri);
        }
    }

    return positions;
}

spret cast_sigil_of_binding(int pow, bool fail, bool tracer)
{
    // Fill list of viable locations to create the sigil (keeping separate lists
    // for distance 1 and 2)
    vector<coord_def> positions = find_sigil_locations(tracer);
    vector<coord_def> sigil_pos_d1;
    vector<coord_def> sigil_pos_d2;
    for (auto p : positions)
    {
        if (grid_distance(you.pos(), p) == 1)
            sigil_pos_d1.push_back(p);
        else
            sigil_pos_d2.push_back(p);
    }

    // If the player knows there are no valid places for a sigil, abort. But if
    // invisible monsters are standing on the only valid locations, we will need
    // to simply fail at cast time.
    bool success = !(sigil_pos_d1.empty() && sigil_pos_d2.empty());
    if (tracer)
        return success ? spret::success : spret::abort;

    fail_check();
    if (!success)
    {
        mpr("There was nowhere nearby to inscribe sigils!");
        return spret::success;
    }

    // Remove any old sigil that may still be active.
    timeout_binding_sigils();

    int dur = BASELINE_DELAY * random_range(5 + div_rand_round(pow, 4),
                                            8 + div_rand_round(pow, 2));
    // Now select a random spot for each range and put the sigil there -
    // attempting to favor placing the sigils in two positions that are
    // non-adjacent.
    shuffle_array(sigil_pos_d1);
    shuffle_array(sigil_pos_d2);

    if (!sigil_pos_d1.empty())
    {
        temp_change_terrain(sigil_pos_d1[0], DNGN_BINDING_SIGIL, dur,
                            TERRAIN_CHANGE_BINDING_SIGIL, you.mid);
    }

    if (!sigil_pos_d2.empty())
    {
        // If this is in fact the second sigil, try to place it non-adjacent to
        // the first one.
        bool non_adj = false;
        if (!sigil_pos_d1.empty())
        {
            for (unsigned int i = 0; i < sigil_pos_d2.size(); ++i)
            {
                // Skip adjacent grids on first pass
                if (grid_distance(sigil_pos_d1[0], sigil_pos_d2[i]) <= 1)
                    continue;

                temp_change_terrain(sigil_pos_d2[i], DNGN_BINDING_SIGIL, dur,
                                    TERRAIN_CHANGE_BINDING_SIGIL, you.mid);
                non_adj = true;
                break;
            }
        }

        // If we couldn't find a non-adjacent position to put the second sigil,
        // or if this is the only sigil, just take any viable space instead.
        if (!non_adj)
        {
            temp_change_terrain(sigil_pos_d2[0], DNGN_BINDING_SIGIL, dur,
                                TERRAIN_CHANGE_BINDING_SIGIL, you.mid);
        }
    }

    if (!sigil_pos_d1.empty() && !sigil_pos_d2.empty())
        mpr("You inscribe a pair of binding sigils.");
    else
        mpr("You inscribe a binding sigil.");

    return spret::success;
}

void trigger_binding_sigil(actor& actor)
{
    if (actor.is_binding_sigil_immune())
    {
        mprf("%s cannot be bound by the sigil due to %s high momentum!",
             actor.name(DESC_THE).c_str(), actor.pronoun(PRONOUN_POSSESSIVE).c_str());
        return;
    }

    if (actor.is_player())
    {
        mprf(MSGCH_WARN, "You move over the binding sigil and are bound in place!");
        you.increase_duration(DUR_NO_MOMENTUM, random_range(3, 6));
        revert_terrain_change(you.pos(), TERRAIN_CHANGE_BINDING_SIGIL);
        return;
    }

    monster* m = actor.as_monster();
    const int pow = calc_spell_power(SPELL_SIGIL_OF_BINDING);
    const int dur = max(2, random_range(4 + div_rand_round(pow, 12),
                                        7 + div_rand_round(pow, 8))
                        - div_rand_round(m->get_hit_dice(), 4))
                    * BASELINE_DELAY;

    if (m->add_ench(mon_enchant(ENCH_BOUND, 0, &you, dur)))
    {
        simple_monster_message(*m,
            " moves over the binding sigil and is bound in place!",
            false, MSGCH_FRIEND_SPELL);

        // The enemy will gain swift for twice as long as it was bound
        m->props[BINDING_SIGIL_DURATION_KEY] = dur * 2;
    }

    revert_terrain_change(actor.pos(), TERRAIN_CHANGE_BINDING_SIGIL);
}

spret cast_spike_launcher(int pow, bool fail)
{
    fail_check();

    int valid = 0;
    coord_def spot;
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        if (feat_is_wall(env.grid(*ai)))
        {
            if (one_chance_in(++valid))
                spot = *ai;
        }
    }

    // Targeter should prevent this from happening.
    if (spot.origin())
        return spret::abort;

    // Remove any existing spike launcher, if we have one
    if (you.duration[DUR_SPIKE_LAUNCHER_ACTIVE])
    {
        for (map_marker *mark : env.markers.get_all(MAT_TERRAIN_CHANGE))
        {
            map_terrain_change_marker *marker =
                dynamic_cast<map_terrain_change_marker*>(mark);

            if (marker->change_type == TERRAIN_CHANGE_SPIKE_LAUNCHER)
                revert_terrain_change(marker->pos, TERRAIN_CHANGE_SPIKE_LAUNCHER);
        }
    }

    int dur = random_range(50, 90) + pow;

    temp_change_terrain(spot, DNGN_SPIKE_LAUNCHER, dur, TERRAIN_CHANGE_SPIKE_LAUNCHER);
    you.duration[DUR_SPIKE_LAUNCHER_ACTIVE] = dur;
    you.props.erase(SPIKE_LAUNCHER_TIMER);
    you.props[SPIKE_LAUNCHER_POWER] = pow;

    mpr("You shape a spike launcher from a nearby wall.");

    return spret::success;
}

static void _fire_spike_launcher(monster* target, const coord_def& origin)
{
    bolt spike;
    zappy(ZAP_SPIKE_LAUNCHER, you.props[SPIKE_LAUNCHER_POWER].get_int(),
            false, spike);
    spike.source = target->pos();
    spike.target = target->pos();
    spike.seen = true;
    spike.range = 1;
    spike.hit_verb = "skewers";
    spike.thrower = KILL_YOU;

    dungeon_feature_type feat = orig_terrain(origin);
    switch (feat)
    {
        case DNGN_STONE_WALL:
        case DNGN_CLEAR_STONE_WALL:
            spike.name = "stone spike";
            break;

        case DNGN_METAL_WALL:
            spike.name = "metal spike";
            break;

        case DNGN_CRYSTAL_WALL:
            spike.name = "crystalline spike";
            break;

        // Rock already handled by zappy()
        default:
            break;
    }

    flash_tile(target->pos(), CYAN);
    spike.fire();
}

static coord_def _find_spike_launcher()
{
    for (map_marker *mark : env.markers.get_all(MAT_TERRAIN_CHANGE))
    {
        map_terrain_change_marker *marker =
            dynamic_cast<map_terrain_change_marker*>(mark);

        if (marker->change_type == TERRAIN_CHANGE_SPIKE_LAUNCHER)
            return marker->pos;
    }

    return coord_def();
}

void handle_spike_launcher(int delay)
{
    coord_def pos = _find_spike_launcher();

    // Somehow it doesn't exist. Maybe it got destroyed?
    if (pos.origin())
    {
        you.duration[DUR_SPIKE_LAUNCHER_ACTIVE] = 0;
        return;
    }

    // Check if we've gotten too far away from our launcher
    if (!you.see_cell_no_trans(pos) || grid_distance(you.pos(), pos) > 3)
    {
        mpr("Your spike launcher falls apart as you grow too distant to "
            "maintain it.");
        revert_terrain_change(pos, TERRAIN_CHANGE_SPIKE_LAUNCHER);
        you.duration[DUR_SPIKE_LAUNCHER_ACTIVE] = 0;
        return;
    }

    int& timer = you.props[SPIKE_LAUNCHER_TIMER].get_int();
    timer -= delay;

    // Now, fire the launcher, if anything is in range.
    while (timer < 0)
    {
        for (fair_adjacent_iterator ai(pos); ai; ++ai)
        {
            if (monster* targ = monster_at(*ai))
            {
                if (!you.see_cell_no_trans(*ai) || targ->wont_attack()
                    || targ->is_firewood())
                {
                    continue;
                }

                _fire_spike_launcher(targ, pos);
                break;
            }
        }

        timer += BASELINE_DELAY;
    }
}

void end_spike_launcher()
{
    for (map_marker *mark : env.markers.get_all(MAT_TERRAIN_CHANGE))
    {
        map_terrain_change_marker *marker =
            dynamic_cast<map_terrain_change_marker*>(mark);

        if (marker->change_type == TERRAIN_CHANGE_SPIKE_LAUNCHER)
        {
            mpr("Your spike launcher falls apart.");
            revert_terrain_change(marker->pos, TERRAIN_CHANGE_SPIKE_LAUNCHER);
            return;
        }
    }
}

vector<coord_def> find_spike_launcher_walls()
{
    vector<coord_def> wall_locs;
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        if (feat_is_wall(env.grid(*ai)))
            wall_locs.push_back(*ai);
    }
    return wall_locs;
}
