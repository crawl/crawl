/**
 * @file
 * @brief Non-enchantment spells that didn't fit anywhere else.
 *           Mostly Transmutations.
**/

#include "AppHdr.h"

#include "spl-other.h"

#include "act-iter.h"
#include "coordit.h"
#include "delay.h"
#include "env.h"
#include "god-companions.h"
#include "libutil.h"
#include "message.h"
#include "mon-place.h"
#include "mon-util.h"
#include "movement.h" // passwall
#include "place.h"
#include "potion.h"
#include "religion.h"
#include "spl-util.h"
#include "terrain.h"

spret cast_sublimation_of_blood(int pow, bool fail)
{
    bool success = false;

    if (you.duration[DUR_DEATHS_DOOR])
        mpr("You can't draw power from your own body while in death's door.");
    else if (!you.can_bleed())
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

    you.increase_duration(DUR_DEATH_CHANNEL, 30 + random2(1 + 2*pow/3), 200);

    if (god != GOD_NO_GOD)
        you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = static_cast<int>(god);

    return spret::success;
}

spret cast_animate_dead(int pow, bool fail)
{
    fail_check();
    mpr("You call upon the dead to rise.");

    you.increase_duration(DUR_ANIMATE_DEAD, 20 + random2(1 + pow), 100);
    you.props[ANIMATE_DEAD_POWER_KEY] = pow;

    return spret::success;
}

void start_recall(recall_t type)
{
    // Assemble the recall list.
    typedef pair<mid_t, int> mid_hd;
    vector<mid_hd> rlist;

    you.recall_list.clear();
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
            if (!is_orcish_follower(**mi))
                continue;
        }

        mid_hd m(mi->mid, mi->get_experience_level());
        rlist.push_back(m);
    }

    if (branch_allows_followers(you.where_are_you))
        populate_offlevel_recall_list(rlist);

    if (!rlist.empty())
    {
        // Sort the recall list roughly
        for (mid_hd &entry : rlist)
            entry.second += random2(10);
        sort(rlist.begin(), rlist.end(), greater_second<mid_hd>());

        you.recall_list.clear();
        for (mid_hd &entry : rlist)
            you.recall_list.push_back(entry.first);

        you.attribute[ATTR_NEXT_RECALL_INDEX] = 1;
        you.attribute[ATTR_NEXT_RECALL_TIME] = 0;
        mpr("You begin recalling your allies.");
    }
    else
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
    else if (mons->alive())
    {
        // Don't recall monsters that are already close to the player
        if (mons->pos().distance_from(you.pos()) < 3
            && mons->see_cell_no_trans(you.pos()))
        {
            recall_orders(mons);
            return false;
        }
        else
        {
            coord_def empty;
            if (find_habitable_spot_near(you.pos(), mons_base_type(*mons), 3, false, empty)
                && mons->move_to_pos(empty))
            {
                recall_orders(mons);
                simple_monster_message(*mons, " is recalled.");
                mons->apply_location_effects(mons->pos());
                // mons may have been killed, shafted, etc,
                // but they were still recalled!
                return true;
            }
        }
    }

    return false;
}

// Attempt to recall a number of allies proportional to how much time
// has passed. Once the list has been fully processed, terminate the
// status.
void do_recall(int time)
{
    while (time > you.attribute[ATTR_NEXT_RECALL_TIME])
    {
        // Try to recall an ally.
        mid_t mid = you.recall_list[you.attribute[ATTR_NEXT_RECALL_INDEX]-1];
        you.attribute[ATTR_NEXT_RECALL_INDEX]++;
        if (try_recall(mid))
        {
            time -= you.attribute[ATTR_NEXT_RECALL_TIME];
            you.attribute[ATTR_NEXT_RECALL_TIME] = 3 + random2(4);
        }
        if ((unsigned int)you.attribute[ATTR_NEXT_RECALL_INDEX] >
             you.recall_list.size())
        {
            end_recall();
            mpr("You finish recalling your allies.");
            return;
        }
    }

    you.attribute[ATTR_NEXT_RECALL_TIME] -= time;
    return;
}

void end_recall()
{
    you.attribute[ATTR_NEXT_RECALL_INDEX] = 0;
    you.attribute[ATTR_NEXT_RECALL_TIME] = 0;
    you.recall_list.clear();
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

bool passwall_simplified_check(const actor &act)
{
    for (adjacent_iterator ai(act.pos(), true); ai; ++ai)
        if (_feat_is_passwallable(env.grid(*ai)))
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
            if (!_feat_is_passwallable(env.grid(pos)))
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

    if (!tracer && x_chance_in_y(40 + pow/3, 100))
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
