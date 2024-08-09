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
#include "target-compass.h"
#include "terrain.h"
#include "traps.h"
#include "travel.h"
#include "travel-open-doors-type.h"
#include "transform.h"
#include "unwind.h"
#include "xom.h" // XOM_CLOUD_TRAIL_TYPE_KEY

static void _apply_move_time_taken();

// Swap monster to this location. Player is swapped elsewhere.
// Moves the monster into position, but does not move the player
// or apply location effects: the latter should happen after the
// player is moved.
static void _swap_places(monster* mons, const coord_def &loc)
{
    ASSERT(map_bounds(loc));
    ASSERT(monster_habitable_grid(mons, env.grid(loc)));

    if (monster_at(loc))
    {
        mpr("Something prevents you from swapping places.");
        return;
    }

    // Friendly foxfire dissipates instead of damaging the player.
    if (mons->type == MONS_FOXFIRE)
    {
        simple_monster_message(*mons, " dissipates!", false,
                               MSGCH_MONSTER_DAMAGE, MDAM_DEAD);
        monster_die(*mons, KILL_DISMISSED, NON_MONSTER, true);
        return;
    }

    mpr("You swap places.");

    mons->move_to_pos(loc, true, true);

    return;
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

static bool _cancel_barbed_move(bool rampaging)
{
    if (you.duration[DUR_BARBS] && !you.props.exists(BARBS_MOVE_KEY))
    {
        std::string prompt = "The barbs in your skin will harm you if you move.";
        prompt += rampaging ? " Rampaging like this could really hurt!" : "";
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

static void _apply_barbs_damage(bool rampaging)
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
            you.duration[DUR_BARBS] += (rampaging ? 0 : you.time_taken);
    }
}

// For effects that should happen whenever the player actively moves with their
// limbs, but NOT if the player blinks/teleports or is otherwise displaced.
void player_did_deliberate_movement(bool rampaging)
{
    _apply_barbs_damage(rampaging);
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

bool cancel_harmful_move(bool physically, bool rampaging)
{
    return physically ? (_cancel_barbed_move(rampaging) || _cancel_ice_move())
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

static void _clear_constriction_data()
{
    you.stop_directly_constricting_all(true);
    if (you.get_constrict_type() == CONSTRICT_MELEE)
        you.stop_being_constricted();
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
            ASSERT(you.wizmode_teleported_into_rock);
        else
        {
            auto cloud = static_cast<cloud_type>(
                you.props[XOM_CLOUD_TRAIL_TYPE_KEY].get_int());
            ASSERT(cloud != CLOUD_NONE);
            check_place_cloud(cloud, old_pos, random_range(3, 10), &you,
                              0, -1);
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
            if (mons
                && (stationary
                    || !(is_sanctuary(you.pos()) && is_sanctuary(mons->pos()))
                       && !fedhas_passthrough(mons))
                && bad_attack(mons, adj, suffix, penance))
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
            string name = bad_mons->name(DESC_PLAIN);
            if (starts_with(name, "the "))
               name.erase(0, 4);
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

    if (you.attribute[ATTR_HELD])
    {
        free_self_from_net();
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
    case DNGN_ENTER_ABYSS:
        return yesno("If you enter this portal you might not be able to return "
                     "immediately. Continue?", false, 'n');
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
    if (!player_equip_unrand(UNRAND_SEVEN_LEAGUE_BOOTS))
        return you.pos() + move;
    const int dist = grid_distance(you.pos(), target->pos()) - 1;
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
        || !you.is_motile()
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
    beam.is_tracer       = true;
    // is_targeting prevents bolt::do_fire() from interrupting with a message
    // if our tracer crosses something that blocks line of fire.
    beam.is_targeting    = true;
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
            || mons_is_firewood(*mon)
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

/**
 * Rampages the player toward a hostile monster, if one exists in the direction
 * of the move input. Invalid things along the rampage path cancel the rampage.
 *
 * @param move  A relative coord_def of the player's CMD_MOVE input,
 *              as called by move_player_action().
 * @return      spret::fail if something invalid prevented the rampage,
 *              spret::abort if a player prompt response should cancel the move
 *              entirely,
 *              spret::success if the rampage occurred.
 */
static spret _rampage_forward(coord_def move)
{
    ASSERT(!crawl_state.game_is_arena());

    const bool enhanced = player_equip_unrand(UNRAND_SEVEN_LEAGUE_BOOTS);
    const bool rolling = you.has_mutation(MUT_ROLLPAGE);
    const string noun = enhanced ? "stride" :
                         rolling ? "roll" : "rampage";
    const string verb = enhanced ? "striding" :
                         rolling ? "rolling" : "rampaging";

    if (crawl_state.is_replaying_keys())
    {
        crawl_state.cancel_cmd_all("You can't repeat " + verb + ".");
        return spret::abort;
    }

    monster* mon_target = get_rampage_target(move);
    if (!mon_target)
        return spret::fail;

    const coord_def rampage_destination = _rampage_destination(move, mon_target);
    const coord_def rampage_target = rampage_destination + move;

    // Will the second move be an attack?
    const bool attacking = mon_target->pos() == rampage_target;

    // Don't rampage if the player's tile is being targeted, somehow.
    if (rampage_destination == you.pos())
        return spret::fail;

    // Do allow rampaging on top of Fedhas plants,
    const monster* mons = monster_at(rampage_destination);
    bool fedhas_move = false;
    if (mons && fedhas_passthrough(mons))
        fedhas_move = true;
    // but otherwise, don't rampage if it would land us on top of a monster,
    else if (mons)
    {
        if (!you.can_see(*mons))
        {
            // .. and if a mons was in the way and invisible, notify the player.
            clear_messages();
            mprf("Something unexpectedly blocks you, preventing you from %s!",
                 verb.c_str());
        }
        return spret::fail;
    }

    // Abort if the player answers no to
    // * barbs damaging move prompt
    // * breaking ice spells prompt
    // * dangerous terrain/trap/cloud/exclusion prompt
    // * weapon check prompts;
    // messaging for this is handled by check_moveto().
    if (attacking && !check_moveto(rampage_destination, noun)
        || attacking && !wielded_weapon_check(you.weapon(), noun + " and attack")
        || !attacking && !check_moveto(rampage_target, noun))
    {
        stop_running();
        you.turn_is_over = false;
        return spret::abort;
    }

    // We've passed the validity checks, go ahead and rampage.
    const coord_def old_pos = you.pos();

    clear_messages();
    const monster* current = monster_at(you.pos());
    if (fedhas_move && (!current || !fedhas_passthrough(current)))
    {
        mprf("You %s quickly through the %s towards %s!",
             noun.c_str(),
             mons_genus(mons->type) == MONS_FUNGUS ? "fungus" : "plants",
             mon_target->name(DESC_THE, true).c_str());
    }
    else
    {
        mprf("You %s towards %s!",
             noun.c_str(),
             mon_target->name(DESC_THE, true).c_str());
    }

    // First, apply any necessary pre-move effects:
    _clear_constriction_data();
    _mark_potential_pursuers(rampage_destination);

    // stepped = true, we're flavouring this as movement, not a blink.
    move_player_to_grid(rampage_destination, true);

    // Verify the new position is valid, in case something unexpected happened.
    ASSERT(!in_bounds(you.pos()) || !cell_is_solid(you.pos())
            || you.wizmode_teleported_into_rock);

    you.clear_far_engulf(false, true);
    // No full-LOS stabbing.
    if (enhanced)
        behaviour_event(mon_target, ME_ALERT, &you, you.pos());

    // Lastly, apply post-move effects unhandled by move_player_to_grid().
    apply_rampage_heal();
    player_did_deliberate_movement(true);
    remove_ice_movement();
    you.clear_far_engulf(false, true);
    apply_cloud_trail(old_pos);

    // If there is somehow an active run delay here, update the travel trail.
    if (you_are_delayed() && current_delay()->is_run())
        env.travel_trail.push_back(you.pos());

    return spret::success;
}

static void _apply_move_time_taken()
{
    you.time_taken *= player_movement_speed();
    you.time_taken = div_rand_round(you.time_taken, 10);
    if (you.duration[DUR_NO_HOP])
        you.duration[DUR_NO_HOP] += you.time_taken;
}

// The "first square" of rampaging ordinarily has no time cost, and the "second
// square" is where its move delay or attack delay would be applied. If the
// player begins a rampage, and then cancels the second move, as through a
// prompt, we have to ensure they don't get zero-cost movement out of it. Here
// we apply movedelay, end the turn, and call relevant post-move effects.
static void _finalize_cancelled_rampage_move()
{
    _apply_move_time_taken();
    you.turn_is_over = true;

    if (player_in_branch(BRANCH_ABYSS))
        maybe_shift_abyss_around_player();

    you.apply_berserk_penalty = true;

    // Rampaging prevents Wu Jian attacks, so we do not process them
    // here
    update_acrobat_status();
}

// Called when the player moves by walking/running. Also calls attack
// function etc when necessary.
void move_player_action(coord_def move)
{
    ASSERT(!crawl_state.game_is_arena() && !crawl_state.arena_suspended);

    bool attacking = false;
    bool moving = true;         // used to prevent eventual movement (swap)
    bool swap = false;

    ASSERT(!in_bounds(you.pos()) || !cell_is_solid(you.pos())
           || you.wizmode_teleported_into_rock);

    if (you.attribute[ATTR_HELD])
    {
        free_self_from_net();
        you.turn_is_over = true;
        return;
    }

    coord_def initial_position = you.pos();

    // When confused, sometimes make a random move.
    if (you.confused())
    {
        if (!you.is_motile())
        {
            // Don't choose a random location to try to attack into - allows
            // abuse, since trying to move (not attack) takes no time, and
            // shouldn't. Just force confused trees to use ctrl.
            mpr("You cannot move. (Use ctrl+direction or * direction to "
                "attack while stationary and confused.)");
            return;
        }

        if (cancel_confused_move(false))
            return;

        if (cancel_harmful_move())
            return;

        if (!one_chance_in(3))
        {
            move.x = random2(3) - 1;
            move.y = random2(3) - 1;
            if (move.origin())
            {
                mpr("You're too confused to move!");
                you.apply_berserk_penalty = true;
                you.turn_is_over = true;
                return;
            }
        }

        const coord_def new_targ = you.pos() + move;
        if (!in_bounds(new_targ) || !you.can_pass_through(new_targ))
        {
            you.turn_is_over = true;
            if (you.digging) // no actual damage
            {
                mprf("Your mandibles retract as you bump into %s.",
                     feature_description_at(new_targ, false,
                                            DESC_THE).c_str());
                you.digging = false;
            }
            else
            {
                mprf("You bump into %s.",
                     feature_description_at(new_targ, false,
                                            DESC_THE).c_str());
            }
            you.apply_berserk_penalty = true;
            crawl_state.cancel_cmd_repeat();

            return;
        }
    }

    bool rampaged = false;
    bool did_wu_jian_attack = false;

    if (you.rampaging())
    {
        const monster *rampage_targ = get_rampage_target(move);
        switch (_rampage_forward(move))
        {
            // Cancel the move entirely if rampage was aborted from a prompt.
            case spret::abort:
                return;

            case spret::success:
                rampaged = true;
                if (you_worship(GOD_WU_JIAN)
                    && wu_jian_post_move_effects(false, initial_position))
                {
                    did_wu_jian_attack = true;
                    // If you kill something with a lunge, don't continue
                    // rampaging into its space. That could be a nasty surprise
                    // for players who land on traps, clouds, exclusions, etc,
                    // even if we prevented moving into solid terrain or lava.
                    if (rampage_targ && !rampage_targ->alive())
                    {
                        you.time_taken = you.attack_delay().roll();
                        you.turn_is_over = true;
                        you.berserk_penalty = 0;
                        wu_jian_trigger_serpents_lash(false, initial_position);
                        return;
                    }
                }
                // If we've rampaged, reset initial_position for the second
                // move.
                initial_position = you.pos();
                break;
            case spret::fail:
            default:
                break;
        }
    }

    const coord_def targ = you.pos() + move;
    // You can't walk out of bounds!
    if (!in_bounds(targ))
    {
        // Why isn't the border permarock?
        if (you.digging)
        {
            mpr("This wall is too hard to dig through.");
            you.digging = false;
        }
        return;
    }

    // XX generalize?
    const string walkverb = you.airborne()                     ? "fly"
                          : you.swimming()                     ? "swim"
                          : you.form == transformation::serpent ? "slither"
                          : you.form != transformation::none   ? "walk" // XX
                          : walk_verb_to_present(lowercase_first(species::walking_verb(you.species)));

    monster* targ_monst = monster_at(targ);
    if (fedhas_passthrough(targ_monst) && you.is_motile())
    {
        // Moving on a plant takes 1.5 x normal move delay. We
        // will print a message about it but only when moving
        // from open space->plant (hopefully this will cut down
        // on the message spam).
        you.time_taken = div_rand_round(you.time_taken * 3, 2);

        monster* current = monster_at(you.pos());
        if (!current || !fedhas_passthrough(current))
        {
            // Probably need a better message. -cao
            mprf("You %s carefully through the %s.", walkverb.c_str(),
                 mons_genus(targ_monst->type) == MONS_FUNGUS ? "fungus"
                                                             : "plants");
        }
        targ_monst = nullptr;
    }

    bool targ_pass = you.can_pass_through(targ) && you.is_motile();

    if (you.digging)
    {
        if (feat_is_diggable(env.grid(targ)) && env.grid(targ) != DNGN_SLIMY_WALL)
            targ_pass = true;
        else // moving or attacking ends dig
        {
            you.digging = false;
            if (env.grid(targ) == DNGN_SLIMY_WALL)
                mpr("Trying to dig through that would dissolve your mandibles.");
            else if (feat_is_solid(env.grid(targ)))
                mpr("You can't dig through that.");
            else
                mpr("You retract your mandibles.");
        }
    }

    // You can swap places with a friendly or good neutral monster if
    // you're not confused, or even with hostiles if both of you are inside
    // a sanctuary.
    const bool try_to_swap = targ_monst
                             && (targ_monst->wont_attack()
                                    && !you.confused()
                                 || is_sanctuary(you.pos())
                                    && is_sanctuary(targ));

    // You cannot move away from a siren but you CAN fight monsters on
    // neighbouring squares.
    monster* beholder = nullptr;
    if (!you.confused())
        beholder = you.get_beholder(targ);

    // You cannot move closer to a fear monger.
    monster *fmonger = nullptr;
    if (!you.confused())
        fmonger = you.get_fearmonger(targ);

    if (!rampaged && you.running.check_stop_running())
    {
        // [ds] Do we need this? Shouldn't it be false to start with?
        you.turn_is_over = false;
        return;
    }

    coord_def mon_swap_dest;

    if (targ_monst)
    {
        if (try_to_swap && !beholder && !fmonger)
        {
            if (swap_check(targ_monst, mon_swap_dest))
                swap = true;
            else
            {
                stop_running();
                moving = false;
            }
        }
        else if (targ_monst->temp_attitude() == ATT_NEUTRAL
                 && !targ_monst->has_ench(ENCH_FRENZIED)
                 && !you.confused()
                 && targ_monst->visible_to(&you))
        {
            simple_monster_message(*targ_monst, " refuses to make way for you. "
                              "(Use ctrl+direction or * direction to attack.)");
            you.turn_is_over = false;
            return;
        }
        else if (!try_to_swap) // attack!
        {
            // Don't allow the player to freely locate invisible monsters
            // with confirmation prompts.
            if (!you.can_see(*targ_monst) && !you.is_motile())
            {
                canned_msg(MSG_CANNOT_MOVE);
                you.turn_is_over = false;
                return;
            }
            // Rampaging forcibly initiates the attack, but the attack
            // can still be cancelled.
            if (!rampaged && !you.can_see(*targ_monst)
                && !you.confused()
                && !check_moveto(targ, walkverb)
                // Attack cancelled by fight_melee
                || !fight_melee(&you, targ_monst))
            {
                stop_running();
                // If we cancel this move after rampaging, we end the turn.
                if (rampaged)
                {
                    move.reset();
                    _finalize_cancelled_rampage_move();
                    return;
                }
                you.turn_is_over = false;
                return;
            }

            you.turn_is_over = true;
            you.berserk_penalty = 0;
            attacking = true;
        }
    }
    else if (you.form == transformation::fungus && moving && !you.confused())
    {
        if (you.is_nervous())
        {
            mpr("You're too terrified to move while being watched!");
            stop_running();
            you.turn_is_over = false;
            return;
        }
    }

    const bool running = you_are_delayed() && current_delay()->is_run();
    bool dug = false;

    if (!attacking && targ_pass && moving && !beholder && !fmonger)
    {
        if (you.confused() && is_feat_dangerous(env.grid(targ)))
        {
            mprf("You nearly stumble into %s!",
                 feature_description_at(targ, false, DESC_THE).c_str());
            you.apply_berserk_penalty = true;
            you.turn_is_over = true;
            return;
        }

        // Allow (e.g.) "Really move and attack while wielding nothing?" abort.
        if (!rampaged && wu_jian_move_triggers_attacks(targ)
            && !wielded_weapon_check(you.weapon(), "move and attack"))
        {
            return;
        }

        // If confused, we've already been prompted (in case of stumbling into
        // a monster and attacking instead).
        // If rampaging we've already been prompted.
        if (!you.confused() && !rampaged && !check_moveto(targ, walkverb))
        {
            stop_running();
            you.turn_is_over = false;
            return;
        }

        if (!you.attempt_escape()) // false means constricted and did not escape
            return;

        if (you.digging)
        {
            mprf("You dig through %s.", feature_description_at(targ, false,
                 DESC_THE).c_str());
            destroy_wall(targ);
            noisy(6, you.pos());
            drain_player(15, false, true);
            dug = true;
        }

        if (swap)
            _swap_places(targ_monst, mon_swap_dest);

        if (running && env.travel_trail.empty())
            env.travel_trail.push_back(you.pos());
        else if (!running)
            clear_travel_trail();

        _apply_move_time_taken();

        coord_def old_pos = you.pos();
        // Don't trigger things that require movement
        // when confusion causes no move.
        if (you.pos() != targ && targ_pass)
        {
            _clear_constriction_data();
            _mark_potential_pursuers(targ);
            move_player_to_grid(targ, true);
            if (rampaged)
                apply_rampage_heal();
            player_did_deliberate_movement();
            remove_ice_movement();
            you.clear_far_engulf(false, true);
            apply_cloud_trail(old_pos);
        }

        // Now it is safe to apply the swappee's location effects and add
        // trailing effects. Doing so earlier would allow e.g. shadow traps to
        // put a monster at the player's location.
        if (swap)
            targ_monst->apply_location_effects(targ);

        if (you_are_delayed() && current_delay()->is_run())
            env.travel_trail.push_back(you.pos());

        move.reset();
        you.turn_is_over = true;
        request_autopickup();
    }

    // BCR - Easy doors single move
    if ((Options.travel_open_doors == travel_open_doors_type::open
             || !you.running)
        && !attacking && feat_is_closed_door(env.grid(targ)))
    {
        open_door_action(move);
        move.reset();
        return;
    }
    else if (!targ_pass && !attacking)
    {
        // No rampage check here, since you can't rampage at walls
        if (!you.is_motile())
            canned_msg(MSG_CANNOT_MOVE);
        else if (env.grid(targ) == DNGN_OPEN_SEA)
            mpr("The ferocious winds and tides of the open sea thwart your progress.");
        else if (env.grid(targ) == DNGN_LAVA_SEA)
            mpr("The endless sea of lava is not a nice place.");
        else if (feat_is_tree(env.grid(targ)) && you_worship(GOD_FEDHAS))
            mpr("You cannot walk through the dense trees.");
        else if (!try_to_swap && env.grid(targ) == DNGN_MALIGN_GATEWAY)
            mpr("The malign portal rejects you as you step towards it.");
        // Show the player the wall they've just bumped into, if they can't see it.
        else if (you.current_vision == 0)
        {
            mpr("You feel something solid in that direction.");
            map_cell& knowledge = env.map_knowledge(targ);
            if (!knowledge.mapped() || knowledge.changed())
            {
                dungeon_feature_type newfeat = env.grid(targ);
                knowledge.set_feature(newfeat, env.grid_colours(targ), TRAP_UNASSIGNED);
                set_terrain_mapped(targ);
            }
        }

        stop_running();
        move.reset();
        you.turn_is_over = false;
        crawl_state.cancel_cmd_repeat();
        return;
    }
    else if (beholder && !attacking)
    {
        mprf("You cannot move away from %s!",
            beholder->name(DESC_THE).c_str());
        stop_running();
        // If this would have been the move after rampaging, we end the turn.
        if (rampaged)
        {
            move.reset();
            _finalize_cancelled_rampage_move();
            return;
        }
        you.turn_is_over = false;
        return;
    }
    else if (fmonger && !attacking)
    {
        mprf("You cannot move closer to %s!",
            fmonger->name(DESC_THE).c_str());
        stop_running();
        // If this would have been the move after rampaging, we end the turn.
        if (rampaged)
        {
            move.reset();
            _finalize_cancelled_rampage_move();
            return;
        }
        you.turn_is_over = false;
        return;
    }

    if (you.running == RMODE_START)
        you.running = RMODE_CONTINUE;

    if (player_in_branch(BRANCH_ABYSS))
        maybe_shift_abyss_around_player();

    you.apply_berserk_penalty = !attacking;

    if (rampaged && !you.has_mutation(MUT_ROLLPAGE)
        || player_equip_unrand(UNRAND_LIGHTNING_SCALES))
    {
        did_god_conduct(DID_HASTY, 1, true);
    }

    if (you_worship(GOD_WU_JIAN) && !attacking && !dug)
        did_wu_jian_attack = wu_jian_post_move_effects(false, initial_position);
    else if (you_worship(GOD_WU_JIAN) && attacking && rampaged)
        wu_jian_trigger_serpents_lash(false, initial_position);

    // If you actually moved without attacking, acrobatics may kick in.
    if (!attacking && moving && !did_wu_jian_attack)
        update_acrobat_status();
}
