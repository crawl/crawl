/**
 * @file
 * @brief Movement, open-close door commands, movement effects.
**/

#include <algorithm>
#include <cstring>
#include <string>
#include <sstream>

#include "AppHdr.h"

#include "movement.h"

#include "abyss.h"
#include "act-iter.h"
#include "art-enum.h"
#include "bloodspatter.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h" // walk_verb_to_present
#include "env.h"
#include "fight.h"
#include "fprop.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "items.h"
#include "map-knowledge.h"
#include "message.h"
#include "mon-abil.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-util.h"
#include "nearby-danger.h"
#include "player.h"
#include "player-reacts.h"
#include "prompt.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "state.h"
#include "stringutil.h"
#include "spl-damage.h"
#include "spl-summoning.h"
#include "target-compass.h"
#include "terrain.h"
#include "throw.h"
#include "traps.h"
#include "travel.h"
#include "travel-open-doors-type.h"
#include "transform.h"
#include "unwind.h"
#include "viewchar.h"
#include "xom.h" // XOM_CLOUD_TRAIL_TYPE_KEY

// Move a monster to a given location, in preparation for the player moving to
// their current location themselves.
//
// This position is usually the player's current position, but in some cases
// may not be (for instance, if the monster being swapped into cannot occupy
// the player's previous location, it may be pushed elsewhere instead.
//
// Note: This does not apply movement finalisation, relying on the caller to do
//       this later themselves!
void player_displace_monster(monster* mons, const coord_def &loc)
{
    ASSERT(map_bounds(loc));
    ASSERT(monster_habitable_grid(mons, loc));
    ASSERT(!monster_at(loc));

    // Friendly seekers dissipate when the player swaps into them.
    if (loc != you.pos())
        mprf("You push %s out of the way.", mons->name(DESC_THE).c_str());
    else if (mons_is_seeker(*mons))
    {
        simple_monster_message(*mons, " dissipates!", false,
                               MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
        monster_die(*mons, KILL_RESET, NON_MONSTER, true);
        return;
    }
    else
        mprf("You swap places with %s.", mons->name(DESC_THE).c_str());

    mons->move_to(loc, MV_ALLOW_OVERLAP, true);
}

// Check squares adjacent to player for given feature and return how
// many there are. If there's only one, return the dx and dy.
static int _check_adjacent(dungeon_feature_type feat, coord_def& delta)
{
    int num = 0;

    set<coord_def> doors;
    for (adjacent_iterator ai(you.pos(), true); ai; ++ai)
    {
        if (env.grid(*ai) == feat)
        {
            // Specialcase doors to take into account gates.
            if (feat_is_door(feat))
            {
                // Already included in a gate, skip this door.
                if (doors.count(*ai))
                    continue;

                // Check if it's part of a gate. If so, remember all its doors.
                set<coord_def> all_door;
                find_connected_identical(*ai, all_door);
                doors.insert(begin(all_door), end(all_door));
            }

            num++;
            delta = *ai - you.pos();
        }
    }

    return num;
}

static bool _cancel_barbed_move()
{
    if (you.duration[DUR_BARBS] && !you.props.exists(BARBS_MOVE_KEY)
        && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        std::string prompt = "The barbs in your skin will harm you if you move.";
        prompt += " Continue?";
        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return true;
        }

        you.props[BARBS_MOVE_KEY] = true;
    }

    return false;
}

static void _apply_barbs_damage()
{
    if (you.duration[DUR_BARBS])
    {
        mprf(MSGCH_WARN, "The barbed spikes dig painfully into your body "
                         "as you move.");
        ouch(roll_dice(2, you.attribute[ATTR_BARBS_POW]), KILLED_BY_BARBS);
        bleed_onto_floor(you.pos(), MONS_PLAYER, 2, false);

        // Sometimes decrease duration even when we move.
        if (one_chance_in(3))
            extract_barbs("The barbed spikes snap loose.");
        // But if that failed to end the effect, duration stays the same.
        if (you.duration[DUR_BARBS])
            you.duration[DUR_BARBS] += (you.time_taken);
    }
}

// For effects that should happen whenever the player actively moves with their
// limbs, but NOT if the player blinks/teleports or is otherwise displaced.
void player_did_deliberate_movement()
{
    _apply_barbs_damage();
    shake_off_sticky_flame();
}

static bool _cancel_ice_move()
{
    vector<string> effects;
    if (i_feel_safe(false, true, true))
        return false;

    if (you.duration[DUR_ICY_ARMOUR])
        effects.push_back("icy armour");

    if (you.duration[DUR_FROZEN_RAMPARTS])
        effects.push_back("frozen ramparts");

    if (!effects.empty())
    {
        string prompt = "Your "
                        + comma_separated_line(effects.begin(), effects.end())
                        + " will break if you move. Continue?";

        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return true;
        }
    }

    return false;
}

bool cancel_harmful_move(bool physically)
{
    return physically ? (_cancel_barbed_move() || _cancel_ice_move())
        : _cancel_ice_move();
}

void remove_ice_movement()
{
    if (you.duration[DUR_ICY_ARMOUR])
    {
        mprf(MSGCH_DURATION, "Your icy armour cracks and falls away as "
                             "you move.");
        you.duration[DUR_ICY_ARMOUR] = 0;
        you.redraw_armour_class = true;
    }

    if (you.duration[DUR_FROZEN_RAMPARTS])
    {
        you.duration[DUR_FROZEN_RAMPARTS] = 0;
        end_frozen_ramparts();
        mprf(MSGCH_DURATION, "The frozen ramparts melt away as you move.");
    }
}

static void _mark_potential_pursuers(coord_def new_pos)
{
    if (you.attribute[ATTR_SERPENTS_LASH]           // too fast!
        || wu_jian_move_triggers_attacks(new_pos)   // too cool!
        || crawl_state.game_is_tutorial())          // too new!
    {
        return;
    }

    const coord_def orig_pos = you.pos();
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        // Only randomize energy for monsters you're moving away from.
        if (grid_distance(new_pos, *ri) <= grid_distance(orig_pos, *ri))
            continue;
        monster* mon = monster_at(*ri);
        // No, there is no logic to this ordering (pf):
        if (!mon || !mon->can_see(you))
            continue;
        actor* foe = mon->get_foe();
        if (!foe || !foe->is_player())
            continue;

        crawl_state.potential_pursuers.insert(mon);
    }
}

bool apply_cloud_trail(const coord_def old_pos)
{
    if (you.duration[DUR_CLOUD_TRAIL])
    {
        if (cell_is_solid(old_pos))
            return false;
        else
        {
            auto cloud = static_cast<cloud_type>(
                you.props[XOM_CLOUD_TRAIL_TYPE_KEY].get_int());
            ASSERT(cloud != CLOUD_NONE);
            place_cloud(cloud, old_pos, random_range(3, 10), &you, 0, -1);
            return true;
        }
    }
    return false;
}

bool cancel_confused_move(bool stationary)
{
    dungeon_feature_type dangerous = DNGN_FLOOR;
    monster *bad_mons = 0;
    string bad_suff, bad_adj;
    bool penance = false;
    bool flight = false;
    for (adjacent_iterator ai(you.pos(), false); ai; ++ai)
    {
        if (!stationary
            && is_feat_dangerous(env.grid(*ai), true)
            && need_expiration_warning(env.grid(*ai))
            && (dangerous == DNGN_FLOOR || env.grid(*ai) == DNGN_LAVA))
        {
            dangerous = env.grid(*ai);
            if (need_expiration_warning(DUR_FLIGHT, env.grid(*ai)))
                flight = true;
            break;
        }
        else
        {
            string suffix, adj;
            monster *mons = monster_at(*ai);
            if (mons && bad_attack(mons, adj, suffix, penance))
            {
                bad_mons = mons;
                bad_suff = suffix;
                bad_adj = adj;
                if (penance)
                    break;
            }
        }
    }

    if (dangerous != DNGN_FLOOR || bad_mons)
    {
        string prompt = "";
        prompt += "Are you sure you want to ";
        prompt += !stationary ? "stumble around" : "swing wildly";
        prompt += " while confused and next to ";

        if (dangerous != DNGN_FLOOR)
        {
            prompt += (dangerous == DNGN_LAVA ? "lava" : "deep water");
            prompt += flight ? " while you are losing your buoyancy"
                             : " while your transformation is expiring";
        }
        else
        {
            string name = remove_prepended_the(bad_mons->name(DESC_PLAIN));
            if (!starts_with(bad_adj, "your"))
               bad_adj = "the " + bad_adj;
            prompt += bad_adj + name + bad_suff;
        }
        prompt += "?";

        if (penance)
            prompt += " This could place you under penance!";

        if (!crawl_state.disables[DIS_CONFIRMATIONS]
            && !yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return true;
        }
    }

    return false;
}

// Opens doors.
// If move is !::origin, it carries a specific direction for the
// door to be opened (eg if you type ctrl + dir).
void open_door_action(coord_def move)
{
    ASSERT(!crawl_state.game_is_arena());
    ASSERT(!crawl_state.arena_suspended);

    if (you.caught())
    {
        you.struggle_against_net();
        you.turn_is_over = true;
        return;
    }

    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return;
    }

    coord_def delta;

    // The player hasn't picked a direction yet.
    if (move.origin())
    {
        const int num = _check_adjacent(DNGN_CLOSED_DOOR, move)
                        + _check_adjacent(DNGN_CLOSED_CLEAR_DOOR, move)
                        + _check_adjacent(DNGN_RUNED_DOOR, move)
                        + _check_adjacent(DNGN_RUNED_CLEAR_DOOR, move);

        if (num == 0)
        {
            mpr("There's nothing to open nearby.");
            return;
        }

        // If there's only one door to open, don't ask.
        if (num == 1 && Options.easy_door)
            delta = move;
        else
        {
            delta = prompt_compass_direction();
            if (delta.origin())
                return;
        }
    }
    else
        delta = move;

    // We got a valid direction.
    const coord_def doorpos = you.pos() + delta;

    if (door_vetoed(doorpos))
    {
        // Allow doors to be locked.
        const string door_veto_message = env.markers.property_at(doorpos,
                                                                 MAT_ANY,
                                                                 "veto_reason");
        if (door_veto_message.empty())
            mpr("The door is shut tight!");
        else
            mpr(door_veto_message);
        if (you.confused())
            you.turn_is_over = true;

        return;
    }

    const dungeon_feature_type feat = (in_bounds(doorpos) ? env.grid(doorpos)
                                                          : DNGN_UNSEEN);
    switch (feat)
    {
    case DNGN_CLOSED_DOOR:
    case DNGN_CLOSED_CLEAR_DOOR:
    case DNGN_RUNED_DOOR:
    case DNGN_RUNED_CLEAR_DOOR:
        player_open_door(doorpos);
        break;
    case DNGN_OPEN_DOOR:
    case DNGN_OPEN_CLEAR_DOOR:
    {
        string door_already_open = "";
        if (in_bounds(doorpos))
        {
            door_already_open = env.markers.property_at(doorpos, MAT_ANY,
                                                    "door_verb_already_open");
        }

        if (!door_already_open.empty())
            mpr(door_already_open);
        else
            mpr("It's already open!");
        break;
    }
    case DNGN_SEALED_DOOR:
    case DNGN_SEALED_CLEAR_DOOR:
        mpr("That door is sealed shut!"); // should use door noun?
        break;
    default:
        mpr("There isn't anything that you can open there!");
        break;
    }
}

void close_door_action(coord_def move)
{
    if (you.attribute[ATTR_HELD])
    {
        mprf("You can't close doors while %s.", held_status());
        return;
    }

    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return;
    }

    coord_def delta;

    if (move.origin())
    {
        // If there's only one door to close, don't ask.
        int num = _check_adjacent(DNGN_OPEN_DOOR, move)
                  + _check_adjacent(DNGN_OPEN_CLEAR_DOOR, move);
        if (num == 0)
        {
            if (_check_adjacent(DNGN_BROKEN_DOOR, move)
                || _check_adjacent(DNGN_BROKEN_CLEAR_DOOR, move))
            {
                mpr("It's broken and can't be closed.");
            }
            else
                mpr("There's nothing to close nearby.");
            return;
        }
        // move got set in _check_adjacent
        else if (num == 1 && Options.easy_door)
            delta = move;
        else
        {
            delta = prompt_compass_direction();
            if (delta.origin())
                return;
        }
    }
    else
        delta = move;

    const coord_def doorpos = you.pos() + delta;
    const dungeon_feature_type feat = (in_bounds(doorpos) ? env.grid(doorpos)
                                                          : DNGN_UNSEEN);

    switch (feat)
    {
    case DNGN_OPEN_DOOR:
    case DNGN_OPEN_CLEAR_DOOR:
        player_close_door(doorpos);
        break;
    case DNGN_CLOSED_DOOR:
    case DNGN_CLOSED_CLEAR_DOOR:
    case DNGN_RUNED_DOOR:
    case DNGN_RUNED_CLEAR_DOOR:
    case DNGN_SEALED_DOOR:
    case DNGN_SEALED_CLEAR_DOOR:
        mpr("It's already closed!");
        break;
    case DNGN_BROKEN_DOOR:
    case DNGN_BROKEN_CLEAR_DOOR:
        mpr("It's broken and can't be closed!");
        break;
    default:
        mpr("There isn't anything that you can close there!");
        break;
    }
}

// Maybe prompt to enter a portal, return true if we should enter the
// portal, false if the user said no at the prompt.
bool prompt_dangerous_portal(dungeon_feature_type ftype)
{
    switch (ftype)
    {
    case DNGN_ENTER_PANDEMONIUM:
    case DNGN_ENTER_ZIGGURAT:
        return yesno("If you enter this portal you might not be able to return "
                     "immediately. Continue?", false, 'n');
    case DNGN_ENTER_ABYSS:
    {
        return yesno(make_stringf("If you enter this portal you could be pulled as "
                     "deep as Abyss:%d and might not be able to return immediately. "
                     "Continue?", abyss_default_depth(true)).c_str(), false, 'n');
    }
    default:
        return true;
    }
}

bool prompt_descent_shortcut(dungeon_feature_type ftype)
{
    if (ftype == DNGN_ENTER_DEPTHS && !player_in_branch(BRANCH_SLIME)
        || ftype == DNGN_ENTER_SLIME && !player_in_branch(BRANCH_VAULTS))
    {
        return yesno("This entrance appears to skip some branches and may be "
                     "quite dangerous. Continue anyway?", false, 'n');
    }
    return true;
}

static coord_def _rampage_destination(coord_def move, monster* target)
{
    const int dist = min(grid_distance(you.pos(), target->pos()) - 1,
                         you.rampaging());
    return you.pos() + move * dist;
}

/// What can the player rampage towards in the given direction, if anything?
monster* get_rampage_target(coord_def move)
{
    if (!you.rampaging())
        return nullptr;

    // Don't rampage if the player has status effects that should prevent it.
    if (you.is_nervous()
        || you.confused()
        || you.cannot_move()
        || you.is_constricted())
    {
        return nullptr;
    }

    ASSERT(move.rdist() == 1);

    const int tracer_range = you.current_vision;
    const coord_def tracer_target = you.pos() + (move * tracer_range);

    bolt beam;
    beam.range           = LOS_RADIUS;
    beam.aimed_at_spot   = true;
    beam.target          = tracer_target;
    beam.source_name     = "you";
    beam.source          = you.pos();
    beam.source_id       = MID_PLAYER;
    beam.thrower         = KILL_YOU;
    beam.pierce          = true;
    beam.affects_nothing = true;
    beam.set_is_tracer(true);
    beam.fire();

    // Iterate the tracer to see if the first visible target is a hostile mons.
    for (coord_def p : beam.path_taken)
    {
        if (!you.see_cell_no_trans(p))
            return nullptr;

        monster* mon = monster_at(p);
        if (!mon
            || fedhas_passthrough(mon)
            || !you.can_see(*mon))
        {
            // Don't rampage if our tracer path is broken by something we can't
            // safely pass through before it reaches a monster.
            if (!you.can_pass_through(p) || is_feat_dangerous(env.grid(p)))
                return nullptr;
            continue;
        }

        if (mon->wont_attack()
            || mons_is_projectile(*mon)
            || mon->is_firewood()
            || adjacent(you.pos(), mon->pos()))
        {
            return nullptr;
        }

        // Do the more expensive fear/mesmerize checks last.
        // Messaging for these occurs in the movement code when relevant.
        const coord_def dest = _rampage_destination(move, mon);
        // Don't rampage if it would take us away from a beholder,
        // or toward a fearmonger.
        if (you.get_beholder(dest) || you.get_fearmonger(dest))
            return nullptr;
        return mon;
    }
    return nullptr;
}

static void _handle_trying_to_move_into_unpassable_terrain(coord_def targ)
{
    if (env.grid(targ) == DNGN_OPEN_SEA)
        mpr("The ferocious winds and tides of the open sea thwart your progress.");
    else if (env.grid(targ) == DNGN_LAVA_SEA)
        mpr("The endless sea of lava is not a nice place.");
    else if (feat_is_tree(env.grid(targ)) && you_worship(GOD_FEDHAS))
    {
        if (you.digging)
            mpr("You cannot dig through the dense trees.");
        else
            mpr("You cannot walk through the dense trees.");
    }
    else if (env.grid(targ) == DNGN_MALIGN_GATEWAY)
        mpr("The malign portal rejects you as you step towards it.");
    // Why isn't the border permarock?
    else if (you.digging && !in_bounds(targ))
        mpr("This wall is too hard to dig through.");
    else if (you.digging && env.grid(targ) == DNGN_SLIMY_WALL)
        mpr("Trying to dig through that would dissolve your mandibles.");
    else if (you.digging)
        mpr("You can't dig through that.");
    else if (you.current_vision == 0)
        mpr("You feel something solid in that direction.");

    // Show the player the wall they've just bumped into, if they can't see it.
    if (you.current_vision == 0)
    {
        map_cell& knowledge = env.map_knowledge(targ);
        if (!knowledge.mapped() || knowledge.changed())
        {
            dungeon_feature_type newfeat = env.grid(targ);
            knowledge.set_feature(newfeat, env.grid_colours(targ), TRAP_UNASSIGNED);
            set_terrain_mapped(targ);
        }
    }
}

// For confused players, adjust a passed movement direction randomly (with
// appropriate warning prompts) and possibly abort entirely if they try to
// stumble into a wall or lava).
// Returns true if movement handling should continue after this point.
static bool _adjust_confused_movement(coord_def& move)
{
    if (you.cannot_move())
    {
        // Don't choose a random location to try to attack into - allows
        // abuse, since trying to move (not attack) takes no time, and
        // shouldn't. Just force confused trees to use ctrl.
        mpr("You cannot move. (Use ctrl+direction or * direction to "
            "attack while stationary and confused.)");
        return false;
    }

    if (cancel_confused_move(false))
        return false;

    if (cancel_harmful_move())
        return false;

    if (!one_chance_in(3))
    {
        move.x = random2(3) - 1;
        move.y = random2(3) - 1;
        if (move.origin())
        {
            mpr("You're too confused to move!");
            you.turn_is_over = true;
            crawl_state.cancel_cmd_repeat();
            return false;
        }
    }

    const coord_def new_targ = you.pos() + move;

    // Test if our new location is somewhere we cannot move, and abort early if so.
    if (!in_bounds(new_targ) || !you.can_pass_through(new_targ))
    {
        mprf("You bump into %s.",
                    feature_description_at(new_targ, false,
                                        DESC_THE).c_str());
        you.turn_is_over = true;
        crawl_state.cancel_cmd_repeat();
        return false;
    }
    else if (is_feat_dangerous(env.grid(new_targ)))
    {
        mprf("You nearly stumble into %s!",
                feature_description_at(new_targ, false, DESC_THE).c_str());
        you.turn_is_over = true;
        crawl_state.cancel_cmd_repeat();
        return false;
    }

    // Otherwise, we're good to attempt stumbling this way.
    return true;
}

static string _get_move_verb(bool is_rampage)
{
    if (is_rampage)
    {
        if (you.unrand_equipped(UNRAND_SEVEN_LEAGUE_BOOTS))
            return "stride";
        else if (you.has_mutation(MUT_STAMPEDE))
            return "stampede";
        else
            return "rampage";
    }

    return you.airborne()                          ? "fly"
           : you.swimming()                        ? "swim"
           : you.form == transformation::serpent   ? "slither"
           : you.form != transformation::none      ? "walk" // XX
           : walk_verb_to_present(lowercase_first(species::walking_verb(you.species)));
}

void east_wind_expose_monster(monster* mon)
{
    if (!mon->is_firewood() && !mon->wont_attack())
    {
        if (!mon->has_ench(ENCH_EXPOSED) && you.can_see(*mon))
            mprf("A bitter wind exposes %s.", mon->name(DESC_THE).c_str());
        mon->add_ench(mon_enchant(ENCH_EXPOSED, &you, random_range(30, 50)));
    }
}

static void _do_east_wind(int num_steps)
{
    for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS, true); ri; ++ri)
        if (monster* mon = monster_at(*ri))
            east_wind_expose_monster(mon);

    you.magic_points_regeneration += you.max_magic_points * num_steps * random_range(5, 7);
    you.did_east_wind = 2; // One stage expires immediately, so that the next can remain visually.
}

static bool _cannot_step_into(const coord_def& pos)
{
    return !you.can_pass_through(pos)
            && (!you.digging
                || !in_bounds(pos)
                || !feat_is_diggable(env.grid(pos))
                || env.grid(pos) == DNGN_SLIMY_WALL);
}

static vector<monster*> _get_stampede_line(const coord_def& start, const coord_def& delta, bool only_known = false)
{
    // Iterate to find how many connected monsters are in a row that the player can see.
    vector<monster*> move_targets;
    coord_def pos = start;
    while (monster* mon = monster_at(pos))
    {
        // Don't count things outside the player's LoS if checking known monsters.
        if (only_known && !you.can_see(*mon))
            break;

        // Can't move anything with a stationary monster in its cluster.
        if (mon->is_stationary())
            return vector<monster*>();

        // Add this monster to the line and advance one step
        move_targets.push_back(mon);
        pos += delta;
    }

    return move_targets;
}

// Determine how many tiles the player would expect to move if they stampeded now
static int _stampede_move_check(const coord_def& delta)
{
    if (!you.duration[DUR_STAMPEDE])
        return 0;

    // If the player can't move into the next tile, bail early.
    if (is_feat_dangerous(env.grid(you.pos() + delta)))
        return 0;

    vector<monster*> move_targets = _get_stampede_line(you.pos() + delta, delta, true);

    if (move_targets.empty())
        return 0;

    for (monster* mon : move_targets)
        if (!monster_habitable_grid(mon, mon->pos() + delta))
            return 0;

    // Now we know we can stampede at least one tile. Check if we can stampede 2.

    // Only bother checking the second step if the player can step there.
    if (is_feat_dangerous(env.grid(you.pos() + delta + delta)))
        return 1;

    vector<monster*> second_move_targets = _get_stampede_line(move_targets.back()->pos() + delta + delta, delta, true);

    for (monster* mon : second_move_targets)
        if (!monster_habitable_grid(mon, mon->pos() + delta))
            return 1;
    for (monster* mon : move_targets)
        if (!monster_habitable_grid(mon, mon->pos() + delta + delta))
            return 1;

    return 2;
}

static bool _try_stampede(const coord_def& target)
{
    if (you.is_constricted() || you.cannot_move() || you.caught())
        return false;

    const coord_def delta = target - you.pos();
    vector<monster*> move_targets = _get_stampede_line(target, delta);

    // Check if we have targets that can be pushed.
    if (move_targets.empty())
        return false;
    if (is_feat_dangerous(env.grid(you.pos() + delta)) || !you.can_pass_through(you.pos() + delta))
        return false;
    for (monster* mon : move_targets)
        if (!monster_habitable_grid(mon, mon->pos() + delta))
            return false;

    // Now move everyone (and ourselves).
    const coord_def old_pos = you.pos();
    for (int i = (int)move_targets.size() - 1; i >= 0; --i)
        move_targets[i]->move_to(old_pos + delta * (i + 2), MV_DEFAULT, true);
    you.move_to(old_pos + delta, MV_DELIBERATE, true);

    // Now finalise movement (to avoid dispersal traps causing all sorts of
    // problems with keeping everyone together in the middle)
    for (size_t i = 0; i < move_targets.size(); ++i)
        if (move_targets[i]->alive())
            move_targets[i]->finalise_movement();
    you.finalise_movement();

    return true;
}

// Handles the player trying to move/attack/swap into a given location.
// Returns true if handling of further steps should continue after this.
static bool _handle_player_step(const coord_def& targ, int& delay, bool rampaging,
                                bool& did_stampede,
                                bool& did_move, bool& did_attack, bool& did_open_door)
{
    const coord_def initial_pos = you.pos();
    monster* mon = monster_at(targ);
    coord_def mon_swap_dest;
    bool fedhas_move = false;

    // First, check for fighting a monster.
    if (mon)
    {
        if (mon->temp_attitude() == ATT_NEUTRAL
            && !mon->has_ench(ENCH_FRENZIED)
            && !you.confused()
            && mon->visible_to(&you))
        {
            simple_monster_message(*mon, " refuses to make way for you. "
                            "(Use ctrl+direction or * direction to attack.)");
            return false;
        }

        // Attempt to attack the monster.
        if (!mon->wont_attack() || you.confused())
        {
            if (you.duration[DUR_STAMPEDE] && !you.confused() && !rampaging
                && _try_stampede(targ))
            {
                // Accumulate cost of moving across terrain, then average it.
                int stampede_delay = player_movement_speed();
                // Move a second time (assuming we ended up where we expected to).
                if (you.pos() == targ && _try_stampede(you.pos() + (targ - initial_pos)))
                    stampede_delay = div_rand_round(stampede_delay + player_movement_speed(), 2);
                did_move = true;
                did_stampede = true;
                delay += stampede_delay;

                // Check that the target we were trampling still there.
                if (!mon || (mon->pos() - you.pos()) != (targ - initial_pos))
                    return false;
            }
            if (!player_fight(mon, rampaging))
            {
                stop_running();
                return false;
            }

            if (rampaging && mon->alive())
                mon->stagger(5);

            did_attack = true;
            you.turn_is_over = true;
            you.berserk_penalty = 0;
            return false;
        }
        else if (fedhas_passthrough(mon))
            fedhas_move = true;
        // If this is a monster we want to swap with, see if we can.
        else
        {
            if (mon->type == MONS_CLOCKWORK_BEE_INACTIVE)
            {
                // Spend a turn if we actually charge the bee, but
                // abort regardless.
                if (clockwork_bee_recharge(you, *mon))
                    you.turn_is_over = true;

                return false;
            }
            else if (!swap_check(mon, mon_swap_dest))
                return false;
        }
    }

    if (feat_is_closed_door(env.grid(targ)))
    {
        if (Options.travel_open_doors == travel_open_doors_type::open
            || !you.running)
        {
            open_door_action(targ - you.pos());

            // Used to not interrupt travel, even if we didn't 'move' this turn.
            // (Technically the player may not have opened the door, due to a
            // cancelled prompt, but an interrupt will have already stopped
            // travel in that case.)
            did_open_door = true;
        }
        return false;
    }

    // Now we know we actually want to move *into* this spot, let's see if we can.
    // XXX: Liquids the player cannot enter are handled by check_moveto_terrain(),
    //      which has already been called, so no need to check again.
    if (you.cannot_move())
    {
        canned_msg(MSG_CANNOT_MOVE);
        return false;
    }
    else if (_cannot_step_into(targ))
    {
        _handle_trying_to_move_into_unpassable_terrain(targ);
        you.digging = false;
        return false;
    }

    if (!you.confused())
    {
        if (you.is_nervous())
        {
            mpr("You're too terrified to move while being watched!");
            return false;
        }
        // If we're rampaging, we've already determined that the endpoint is at
        // an appropriate range, so don't stop for beholders in the middle.
        else if (!rampaging)
        {
            if (monster* beholder = you.get_beholder(targ))
            {
                mprf("You cannot move away from %s!",
                     beholder->name(DESC_THE).c_str());
                return false;
            }
            else if (monster* fearmonger = you.get_fearmonger(targ))
            {
                mprf("You cannot move closer to %s!",
                    fearmonger->name(DESC_THE).c_str());
                return false;
            }
        }
    }

    if (!you.attempt_escape()) // false means constricted and did not escape
        return false;

    if (you.digging)
    {
        if (!feat_is_solid(env.grid(targ)))
            you.digging = false;
        else
        {
            mprf("You dig through %s.", feature_description_at(targ, false,
                    DESC_THE).c_str());
            destroy_wall(targ);
            noisy(6, you.pos());
            drain_player(15, false, true);
        }
    }

    // Move any swapping monster to our current position.
    if (mon)
    {
        if (fedhas_move)
        {
            // Moving over plants is slow. We will print a message about it but
            // only when moving from open space->plant.
            delay += 5;

            const monster* current = monster_at(you.pos());
            if (!current || !fedhas_passthrough(current))
            {
                mprf("You %s carefully through the %s.",
                        _get_move_verb(rampaging).c_str(),
                    mons_genus(mon->type) == MONS_FUNGUS ? "fungus"
                                                         : "plants");
            }
        }
        else
            player_displace_monster(mon, mon_swap_dest);
    }

    const bool running = you_are_delayed() && current_delay()->is_run();
    if (running && env.travel_trail.empty())
        env.travel_trail.push_back(you.pos());
    else if (!running)
        clear_travel_trail();

    _mark_potential_pursuers(targ);

    // XXX: While this is deliberate movement, we specifically don't use
    //      MV_DELIBERATE here so that rampage does not trigger barbs potentially
    //      many times in a row. move_player_action() will do this later.
    you.move_to(targ, MV_NO_TRAVEL_STOP, true);

    // Calculate delay based on the tile we moved *into* (before any traps trigger
    // and potentially move us somewhere else).
    delay += player_movement_speed();
    did_move = true;

    if (mon && !fedhas_move)
    {
        if (mon->type == MONS_SOLAR_EMBER
            && mon->hit_points < mon->max_hit_points)
        {
            mon->heal(mon->max_hit_points * 3 / 4);
            mprf("You weave more energy into your solar ember.");
        }

        mon->finalise_movement();
    }

    if (running)
        env.travel_trail.push_back(you.pos());

    if (you_worship(GOD_WU_JIAN))
        did_attack |= wu_jian_post_move_effects(false, initial_pos);

    you.finalise_movement();

    return true;
}

void move_player_action(coord_def move)
{
    ASSERT(!crawl_state.game_is_arena() && !crawl_state.arena_suspended);

    ASSERT(!in_bounds(you.pos()) || !cell_is_solid(you.pos())
           || you.wizmode_teleported_into_rock);

    // XXX: In theory, it should be impossible to reach this function while this
    //      statement is untrue. But current bugs with mouse input handling can
    //      sometimes result in taking an action in the middle of taking another
    //      action. Simple abort silently in this case, until the more
    //      fundamental bugs can be fixed.
    if (you.turn_is_over)
        return;

    if (you.running.check_stop_running())
        return;

    // If stuck in a net, struggle instead of doing anything
    if (you.attribute[ATTR_HELD])
    {
        you.struggle_against_net();
        you.turn_is_over = true;
        return;
    }

    // When confused, adjust movement randomly (and maybe abort early)
    if (you.confused() && !_adjust_confused_movement(move))
        return;

    int num_steps = 1;
    // If this movement can rampage, check how many steps in total we want to move.
    monster* mon_target = get_rampage_target(move);
    if (mon_target)
        num_steps = min(you.rampaging() + 1, grid_distance(you.pos(), mon_target->pos()));
    const bool rampage_attack = mon_target && grid_distance(you.pos(), mon_target->pos()) == num_steps;

    // Now, calculate any warnings we want to give the player for each step they will take.
    const int stampede_steps = _stampede_move_check(move);
    coord_def targ = you.pos();
    string move_verb = _get_move_verb(num_steps + stampede_steps > 1);
    const int end_step = rampage_attack ? num_steps - 2 : max(num_steps, stampede_steps) - 1 ;

    // For the purposes of printing proper warnings, pretend we are taking
    // additional normal steps.
    for (int i = 0; i < max(num_steps, stampede_steps); ++i)
    {
        if (you.cannot_move())
            break;

        targ += move;

        // Don't warn about traps or clouds on spaces we will not be entering.
        if (_cannot_step_into(targ))
            break;

        if (monster* mon_at = monster_at(targ))
        {
            coord_def dummy;
            if (!stampede_steps
                && you.can_see(*mon_at)
                && (!mon_at->wont_attack()
                    || !(fedhas_passthrough(mon_at)
                         || swap_check(mon_at, dummy, true))))
            {
                break;
            }
        }

        // Check for moving *over* all spaces but our destination.
        // (ie: ignore clouds, since we don't expect to be staying here).
        if (i == end_step)
        {
            if (!check_moveto(targ, move_verb))
                return;
        }
        // At the destination, also warn about clouds and similar.
        else if (!check_move_over(targ, move_verb))
            return;
    }

    const bool did_tailwind = num_steps > 1 && you.duration[DUR_TAILWIND];
    if (did_tailwind)
        you.duration[DUR_TAILWIND] = 0;

    // Print a message, if rampaging.
    if (num_steps > 1 && mon_target)
    {
        mprf("You %s towards %s%s!",
                move_verb.c_str(),
                mon_target->name(DESC_THE, true).c_str(),
                did_tailwind ? " with incredible speed" : "");

        // Prevent full-LoS stabbing with Seven League Boots.
        if (you.unrand_equipped(UNRAND_SEVEN_LEAGUE_BOOTS))
            behaviour_event(mon_target, ME_ALERT, &you, you.pos());
    }

    // Now, we can assume the player has been fully prompted for any movement,
    // so take each step in order (tracking whether we actually moved or attack,
    // and how much time it took to move through each space).
    const coord_def initial_pos = you.pos();
    targ = you.pos();
    int delay = 0;
    int steps_taken = 0;
    bool did_move = false;
    bool did_attack = false;
    bool did_open_door = false;
    bool did_stampede = false;
    for (; steps_taken < num_steps; ++steps_taken)
    {
        // If we have somehow ended up somewhere other than where we tried
        // to move (likely due to a trap), abort any further steps.
        if (you.pos() != targ || you.caught())
            break;

        targ += move;

        // If we were rampaging towards a monster we expected to attack, and it
        // died or became displaced before we reached it, don't rampage into its space.
        if (rampage_attack && steps_taken == num_steps - 1
            && (!mon_target->alive() || mon_target->pos() != targ))
        {
            break;
        }

        if (!_handle_player_step(targ, delay, num_steps > 1,
                                 did_stampede,
                                 did_move, did_attack, did_open_door))
        {
            // Need to mark another step here so that move delay will avoid a div-by-0
            if (did_stampede)
                ++steps_taken;
            break;
        }
    }

    // Movement delay is the average of the delay of all steps we took, unless
    // we attacked without moving (in which case use the delay already set by
    // player_fight())
    if (did_move)
    {
        delay = div_rand_round(delay, steps_taken);
        you.time_taken = div_rand_round(player_speed() * delay, BASELINE_DELAY);
        you.turn_is_over = true;
    }

    if (you.running == RMODE_START)
        you.running = RMODE_CONTINUE;

    if (player_in_branch(BRANCH_ABYSS))
        maybe_shift_abyss_around_player();

    if (did_move)
    {
        player_did_deliberate_movement();
        wu_jian_trigger_serpents_lash(false);

        if (you.duration[DUR_NO_HOP])
            you.duration[DUR_NO_HOP] += you.time_taken;
        if (you.duration[DUR_MESMERISM_COOLDOWN])
            you.duration[DUR_MESMERISM_COOLDOWN] += you.time_taken;

        if (you.unrand_equipped(UNRAND_LIGHTNING_SCALES)
            || num_steps > 1 && !you.has_mutation(MUT_STAMPEDE))
        {
            did_god_conduct(DID_HASTY, 1, true);
        }

        if (!did_attack && (num_steps > 1 || did_stampede) && you.has_mutation(MUT_STAMPEDE))
            did_attack |= do_west_wind_shot();

        if (!did_attack)
            update_acrobat_status();

        // Either a rampage movement or a stampede push will sustain stampede.
        if ((num_steps > 1 || did_stampede) && you.has_mutation(MUT_STAMPEDE))
        {
            if (you.has_mutation(MUT_EAST_WIND))
                _do_east_wind(did_stampede ? 2 : num_steps);
            you.duration[DUR_STAMPEDE] = you.time_taken + 1;
        }
    }

    if (!did_move && !did_attack && !did_open_door)
    {
        stop_running();
        crawl_state.cancel_cmd_repeat();
    }

    if (did_tailwind)
        you.time_taken = 0;

    if (you.pos() != initial_pos)
        request_autopickup();
}
