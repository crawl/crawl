/**
 * @file
 * @brief Movement, open-close door commands, movement effects.
**/

#include <algorithm>
#include <cstring>
#include <string>
#include <sstream>
#include <map>
#include <iterator>

#include "AppHdr.h"

#include "movement.h"

#include "abyss.h"
#include "bloodspatter.h"
#include "cloud.h"
#include "coord.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "env.h"
#include "fight.h"
#include "food.h"
#include "fprop.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "items.h"
#include "item-prop.h"
#include "message.h"
#include "melee-attack.h"
#include "mon-act.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-tentacle.h"
#include "mon-util.h"
#include "player.h"
#include "player-equip.h"
#include "player-reacts.h"
#include "player-tentacle.h"
#include "prompt.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "state.h"
#include "stringutil.h"
#include "spl-selfench.h" // noxious_bog_cell
#include "spl-summoning.h"
#include "terrain.h"
#include "traps.h"
#include "travel.h"
#include "transform.h"
#include "unwind.h"
#include "xom.h" // XOM_CLOUD_TRAIL_TYPE_KEY

// Swap monster to this location. Player is swapped elsewhere.
// Moves the monster into position, but does not move the player
// or apply location effects: the latter should happen after the
// player is moved.
static void _swap_places(monster* mons, const coord_def &loc)
{
    ASSERT(map_bounds(loc));
    ASSERT(monster_habitable_grid(mons, grd(loc)));

    if (monster_at(loc))
    {
        if (mons->type == MONS_WANDERING_MUSHROOM
            && monster_at(loc)->type == MONS_TOADSTOOL)
        {
            // We'll fire location effects for 'mons' back in move_player_action,
            // so don't do so here. The toadstool won't get location effects,
            // but the player will trigger those soon enough. This wouldn't
            // work so well if toadstools were aquatic, had clinging, or were
            // otherwise handled specially in monster_swap_places or in
            // apply_location_effects.
            monster_swaps_places(mons, loc - mons->pos(), true, false);
            return;
        }
        else
        {
            mpr("Something prevents you from swapping places.");
            return;
        }
    }

    // Friendly foxfire dissipates instead of damaging the player.
    if (mons->type == MONS_FOXFIRE || mons->type == MONS_WILL_O_WISP)
    {
        simple_monster_message(*mons, " dissipates!",
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
        if (grd(*ai) == feat)
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

static void _entered_malign_portal(actor* act)
{
    ASSERT(act); // XXX: change to actor &act
    if (you.can_see(*act))
    {
        mprf("%s %s twisted violently and ejected from the portal!",
             act->name(DESC_THE).c_str(), act->conj_verb("be").c_str());
    }

    act->blink();
    act->hurt(nullptr, roll_dice(2, 4), BEAM_MISSILE, KILLED_BY_WILD_MAGIC,
              "", "entering a malign gateway");
}


static bool _can_leap_target(monster* mon)
{
    if (!(mon
        && mon->alive()
        && you.see_cell_no_trans(mon->pos())
        && !mon->wont_attack()
        && !mons_is_firewood(*mon)
        && mon->type != MONS_BUTTERFLY
        && !mons_is_tentacle_or_tentacle_segment(mon->type)))
        return false;
    bolt tempbeam;
    tempbeam.source = you.pos();
    tempbeam.source_id = MID_PLAYER;
    tempbeam.target = mon->pos();
    tempbeam.range = LOS_RADIUS;
    tempbeam.is_tracer = true;
    tempbeam.fire();    
    for (auto iter : tempbeam.path_taken) {
        if (iter == mon->pos()) {
            return true;
        }
        if (!you.can_pass_through(iter))
            return false;
        if (monster_at(iter)) {
            return false;
        }
    }
    return false;
}


static monster* _mantis_leap_attack(coord_def& new_pos)
{
    int can_jump_range = you.airborne() ? 5 : 4;
    bool mon_exist = false;
    bool coward = true;

    if (!in_bounds(you.pos())) {
        return nullptr;
    }

    if (you.duration[DUR_COWARD]) {
        return nullptr;
    }

    map<monster*, coord_def> vaild_mon;
    bool diving_warning = false;
    for (distance_iterator di(you.pos(), true, true, LOS_RADIUS); di; ++di)
    {
        monster* mon = monster_at(*di);
        if (_can_leap_target(mon))
        {
            mon_exist = true;
            bolt tempbeam;
            tempbeam.source = you.pos();
            tempbeam.source_id = MID_PLAYER;
            tempbeam.target = *di;
            tempbeam.range = LOS_RADIUS;
            tempbeam.is_tracer = true;
            tempbeam.fire();

            bool first = true;
            bool vaild = false;
            bool can_jump = false;
            bool warning = false;
            int range_ = 1;
            coord_def jumping_pos;
            for (auto iter : tempbeam.path_taken)  {
                if (!first && iter == mon->pos()) {
                    if (!can_jump)
                        vaild = false;
                    else
                        vaild = true;
                    break;
                }
                jumping_pos = iter;
                if (you.can_pass_through(iter))
                    can_jump = true;
                else
                    can_jump = false;
                    
                if (is_feat_dangerous(env.grid(iter))) {
                    warning = true;
                }
                else {
                    warning = false;
                }


                if (monster_at(iter)) {
                    vaild = false;
                    break;
                }

                if (first) {
                    for (int _x = -1; _x <= 1; _x++) {
                        for (int _y = -1; _y <= 1; _y++) {
                            if (_x == 0 || _y == 0) {
                                if (new_pos == (iter + coord_def(_x, _y))) {
                                    first = false;
                                    coward = false;
                                    _x = 2;
                                    _y = 2;
                                }
                            }
                        }
                    }
                    if (first)
                        break;
                }
                range_++;
                if (range_ > can_jump_range) {
                    break;
                }

            }
            if (vaild) {
                if (warning)
                    diving_warning = true;
                vaild_mon.insert(pair<monster*, coord_def>(mon, jumping_pos));
            }
        }
    }

    if (!vaild_mon.empty()) {
        if (diving_warning) {
            if (!yesno("This action will make you die. Really Do it? (Y/N)", false, 'n')) {
                mpr("Okay, then.");
                new_pos = you.pos();
                return nullptr;
            }
        }
        map<monster*, coord_def>::iterator item = vaild_mon.begin();
        advance(item, random2(vaild_mon.size()));
        new_pos = item->second;
        return item->first;
    }
    else {
        if (mon_exist && coward) {
            you.increase_duration(DUR_COWARD, 10, 10);
        }
    }
    return nullptr;

}

static bool _mantis_leap_attack_doing(monster* mons)
{
    if (!mons->alive())
        return false;
    const int number_of_attacks = 1;

    if (number_of_attacks == 0)
    {
        mprf("You %s at %s, but your attack speed is too slow for a blow "
            "to land.", "leap", mons->name(DESC_THE).c_str());
        return false;
    }
    else
    {
        mprf("You %s at %s%s.",
            "leap",
            mons->name(DESC_THE).c_str(),
            number_of_attacks > 1 ? ", in a flurry of attacks" : "");
    }
    you.time_taken = player_speed();
    for (int i = 0; i < number_of_attacks; i++)
    {
        if (!mons->alive())
            break;
        melee_attack leap(&you, mons);
        leap.attack();
    }
    return true;
}

bool mantis_leap_point(set<coord_def>& set_, set<coord_def>& coward_set_)
{
    if (!in_bounds(you.pos())) {
        return false;
    }

    int radius_ = you.airborne() ? 5 : 4;
    if (you.duration[DUR_COWARD]) {
        return false;
    }
    for (distance_iterator di(you.pos(), true, true, LOS_RADIUS); di; ++di)
    {
        monster* mon = monster_at(*di);
        if (_can_leap_target(mon))
        {
            bolt tempbeam;
            tempbeam.source = you.pos();
            tempbeam.source_id = MID_PLAYER;
            tempbeam.target = *di;
            tempbeam.range = LOS_RADIUS;
            tempbeam.is_tracer = true;
            tempbeam.fire();

            bool first = true;
            bool vaild = false;
            bool can_jump = false;
            bool in_range = true;
            int range_ = 1;
            set<coord_def> leap_point;
            for (auto iter : tempbeam.path_taken) {
                if (!first && iter == mon->pos()) {
                    if (!can_jump)
                    {
                        vaild = false;
                        in_range = false;
                    }
                    else
                    {
                        vaild = true;
                    }
                    break;
                }
                if (you.can_pass_through(iter))
                    can_jump = true;
                else
                    can_jump = false;
                if (monster_at(iter)) {
                    in_range = false;
                    vaild = false;
                    break;
                }

                if (first) {
                    for (int _x = -1; _x <= 1; _x++) {
                        for (int _y = -1; _y <= 1; _y++) {
                            if (_x == 0 || _y == 0) {
                                first = false;
                                leap_point.insert(iter  + coord_def(_x,_y));
                            }
                        }
                    }
                    if (first)
                        break;
                }
                range_++;
                if (range_ > radius_) {
                    in_range = false;
                }
            }
            if (vaild) {
                if (in_range) {
                    set_.insert(leap_point.begin(), leap_point.end());
                }
                else {
                    coward_set_.insert(leap_point.begin(), leap_point.end());
                }
            }
        }
    }
    return true;
}

bool cancel_barbed_move()
{
    if (you.duration[DUR_BARBS] && !you.props.exists(BARBS_MOVE_KEY))
    {
        std::string prompt = "The barbs in your skin will harm you if you move."
                        " Continue?";
        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return true;
        }

        you.props[BARBS_MOVE_KEY] = true;
    }

    return false;
}

void apply_barbs_damage()
{
    if (you.duration[DUR_BARBS])
    {
        mprf(MSGCH_WARN, "The barbed spikes dig painfully into your body "
                         "as you move.");
        ouch(roll_dice(2, you.attribute[ATTR_BARBS_POW]), KILLED_BY_BARBS);
        bleed_onto_floor(you.pos(), MONS_PLAYER, 2, false);

        // Sometimes decrease duration even when we move.
        if (one_chance_in(3))
            extract_manticore_spikes("The barbed spikes snap loose.");
        // But if that failed to end the effect, duration stays the same.
        if (you.duration[DUR_BARBS])
            you.duration[DUR_BARBS] += you.time_taken;
    }
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
            && is_feat_dangerous(grd(*ai), true)
            && need_expiration_warning(grd(*ai))
            && (dangerous == DNGN_FLOOR || grd(*ai) == DNGN_LAVA))
        {
            dangerous = grd(*ai);
            if (need_expiration_warning(DUR_FLIGHT, grd(*ai)))
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
                && bad_attack(mons, adj, suffix, penance)
                && mons->angered_by_attacks())
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

    dist door_move;

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
            door_move.delta = move;
        else
        {
            mprf(MSGCH_PROMPT, "Which direction?");
            direction_chooser_args args;
            args.restricts = DIR_DIR;
            direction(door_move, args);

            if (!door_move.isValid)
                return;
        }
    }
    else
        door_move.delta = move;

    // We got a valid direction.
    const coord_def doorpos = you.pos() + door_move.delta;

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

    const dungeon_feature_type feat = (in_bounds(doorpos) ? grd(doorpos)
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
        mpr("That door is sealed shut!");
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

    dist door_move;

    if (move.origin())
    {
        // If there's only one door to close, don't ask.
        int num = _check_adjacent(DNGN_OPEN_DOOR, move)
                  + _check_adjacent(DNGN_OPEN_CLEAR_DOOR, move);
        if (num == 0)
        {
            mpr("There's nothing to close nearby.");
            return;
        }
        // move got set in _check_adjacent
        else if (num == 1 && Options.easy_door)
            door_move.delta = move;
        else
        {
            mprf(MSGCH_PROMPT, "Which direction?");
            direction_chooser_args args;
            args.restricts = DIR_DIR;
            direction(door_move, args);

            if (!door_move.isValid)
                return;
        }

        if (door_move.delta.origin())
        {
            mpr("You can't close doors on yourself!");
            return;
        }
    }
    else
        door_move.delta = move;

    const coord_def doorpos = you.pos() + door_move.delta;
    const dungeon_feature_type feat = (in_bounds(doorpos) ? grd(doorpos)
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
    case DNGN_ENTER_ABYSS:
        return yesno("If you enter this portal you might not be able to return "
                     "immediately. Continue?", false, 'n');

    case DNGN_MALIGN_GATEWAY:
        return yesno("Are you sure you wish to approach this portal? There's no "
                     "telling what its forces would wreak upon your fragile "
                     "self.", false, 'n');

    default:
        return true;
    }
}

// Called when the player moves by walking/running. Also calls attack
// function etc when necessary.
void move_player_action(coord_def move)
{
    ASSERT(!crawl_state.game_is_arena() && !crawl_state.arena_suspended);

    bool attacking = false;
    bool moving = true;         // used to prevent eventual movement (swap)
    bool swap = false;

    int additional_time_taken = 0; // Extra time independent of movement speed

    ASSERT(!in_bounds(you.pos()) || !cell_is_solid(you.pos())
        || (is_able_into_wall() && feat_is_diggable(grd(you.pos())))
           || you.wizmode_teleported_into_rock);

    if (you.attribute[ATTR_HELD])
    {
        free_self_from_net();
        you.turn_is_over = true;
        return;
    }

    const coord_def initial_position = you.pos();

    // When confused, sometimes make a random move.
    if (you.confused())
    {
        if (you.is_stationary())
        {
            // Don't choose a random location to try to attack into - allows
            // abuse, since trying to move (not attack) takes no time, and
            // shouldn't. Just force confused trees to use ctrl.
            mpr("You cannot move. (Use ctrl+direction or * direction to "
                "attack without moving.)");
            return;
        }

        if (cancel_confused_move(false))
            return;

        if (cancel_barbed_move())
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
                mprf("Your mandibles retract as you bump into %s",
                     feature_description_at(new_targ, false,
                                            DESC_THE).c_str());
                you.digging = false;
            }
            else
            {
                mprf("You bump into %s",
                     feature_description_at(new_targ, false,
                                            DESC_THE).c_str());
            }
            you.apply_berserk_penalty = true;
            crawl_state.cancel_cmd_repeat();

            return;
        }
    }

    coord_def targ = you.pos() + move;
    string wall_jump_err;
    // Don't allow wall jump against close doors via movement -- need to use
    // the ability. Also, if moving into a closed door, don't call
    // wu_jian_can_wall_jump, to avoid printing a spurious message (see 11940).
    bool can_wall_jump = Options.wall_jump_move
                         && (!in_bounds(targ)
                             || !feat_is_closed_door(grd(targ)))
                         && wu_jian_can_wall_jump(targ, wall_jump_err);
    bool did_wall_jump = false;
    // You can't walk out of bounds!
    if (!in_bounds(targ) && !can_wall_jump)
    {
        // Why isn't the border permarock?
        if (you.digging)
            mpr("This wall is too hard to dig through.");
        return;
    }

    const string walkverb = you.airborne()                     ? "fly"
                          : you.swimming()                     ? "swim"
                          : you.form == transformation::spider ? "crawl"
                          : (you.species == SP_NAGA
                             && form_keeps_mutations())        ? "slither"
                                                               : "walk";

    monster* targ_monst = monster_at(targ);


    if (targ_monst && mons_tentacle_adjacent_of_player(targ_monst))
    {
        const bool basis = targ_monst->props.exists("outwards");
        monster* outward =  basis ? monster_by_mid(targ_monst->props["outwards"].get_int()) : nullptr;
        if (outward)
            outward->props["inwards"].get_int() = targ_monst->mid;

        monster_die(*targ_monst, KILL_MISC, NON_MONSTER, true);
        targ_monst = nullptr;
    }

    if (targ_monst && you.is_parent_monster_of(targ_monst))
    {
        destroy_tentacle(targ_monst);
        monster_die(*targ_monst, KILL_MISC, NON_MONSTER, true);
        targ_monst = nullptr;
    }

    if (fedhas_passthrough(targ_monst) && !you.is_stationary())
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

    bool targ_pass = you.can_pass_through(targ) && !you.is_stationary();

    if (you.digging)
    {
        if (apply_starvation_penalties())
        {
            you.digging = false;
            canned_msg(MSG_TOO_HUNGRY);
        }
        else if (feat_is_diggable(grd(targ)))
            targ_pass = true;
        else // moving or attacking ends dig
        {
            you.digging = false;
            if (feat_is_solid(grd(targ)))
                mpr("You can't dig through that.");
            else
                mpr("You retract your mandibles.");
        }
    }

    if (is_able_into_wall()) {
        if (feat_is_diggable(grd(targ))) {
            bool success = false;
            for (adjacent_iterator ai(targ); ai; ++ai)
            {
                if (!cell_is_solid(*ai)) {
                    success = true;
                    break;
                }
            }
            if (success == true) {
                targ_pass = true;
            }
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

    if (you.running.check_stop_running())
    {
        // [ds] Do we need this? Shouldn't it be false to start with?
        you.turn_is_over = false;
        return;
    }

    coord_def mon_swap_dest;

    if (targ_monst && !targ_monst->submerged())
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
        else if (targ_monst->temp_attitude() == ATT_NEUTRAL && !you.confused()
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
            if (!you.can_see(*targ_monst)
                && !you.confused()
                && !check_moveto(targ, walkverb))
            {
                stop_running();
                you.turn_is_over = false;
                return;
            }

            you.turn_is_over = true;
            fight_melee(&you, targ_monst);

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

    monster* mantis_attack_target_mon = nullptr;
    const bool running = you_are_delayed() && current_delay()->is_run();

    if (!attacking && (targ_pass || can_wall_jump)
        && moving && !beholder && !fmonger)
    {
        if (you.confused() && is_feat_dangerous(env.grid(targ)))
        {
            mprf("You nearly stumble into %s!",
                 feature_description_at(targ, false, DESC_THE, false).c_str());
            you.apply_berserk_penalty = true;
            you.turn_is_over = true;
            return;
        }

        // can_wall_jump means `targ` is solid and can be walljumped off of,
        // so the player will never enter `targ`. Therefore, we don't want to
        // check exclusions at `targ`.
        if (!you.confused() && !can_wall_jump && !check_moveto(targ, walkverb))
        {
            stop_running();
            you.turn_is_over = false;
            return;
        }

        // If confused, we've already been prompted (in case of stumbling into
        // a monster and attacking instead).
        if (!you.confused() && cancel_barbed_move())
            return;

        if (!you.attempt_escape()) // false means constricted and did not escape
            return;

        if (you.duration[DUR_WATER_HOLD])
        {
            mpr("You slip free of the water engulfing you.");
            you.props.erase("water_holder");
            you.clear_far_engulf();
        }

        if (you.digging)
        {
            mprf("You dig through %s.", feature_description_at(targ, false,
                 DESC_THE, false).c_str());
            destroy_wall(targ);
            noisy(6, you.pos());
            make_hungry(50, true);
            additional_time_taken += BASELINE_DELAY / 5;
        }

        if (swap)
            _swap_places(targ_monst, mon_swap_dest);
        
        if (running && env.travel_trail.empty())
            env.travel_trail.push_back(you.pos());
        else if (!running)
            clear_travel_trail();

        // clear constriction data
        you.stop_directly_constricting_all(true);
        if (you.is_directly_constricted())
            you.stop_being_constricted();

        coord_def old_pos = you.pos();

        if (you.species == SP_MANTIS && !attacking)
        {
            //jump
            mantis_attack_target_mon = _mantis_leap_attack(targ);
            if (you.pos() == targ) {
                targ_pass = false;
            }
        }


        // Don't trigger traps when confusion causes no move.
        if (you.pos() != targ && targ_pass)
            move_player_to_grid(targ, true);
        else if (can_wall_jump && !running)
        {
            if (!wu_jian_do_wall_jump(targ, false))
                return; // wall jump only in the ready state, or cancelled
            else
                did_wall_jump = true;
        }

        // Now it is safe to apply the swappee's location effects and add
        // trailing effects. Doing so earlier would allow e.g. shadow traps to
        // put a monster at the player's location.
        if (swap)
            targ_monst->apply_location_effects(targ);
        else
        {

            if (you.duration[DUR_NOXIOUS_BOG])
            {
                if (cell_is_solid(old_pos))
                    ASSERT(you.wizmode_teleported_into_rock);
                else
                    noxious_bog_cell(old_pos);
            }

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
                }
            }
        }

        apply_barbs_damage();

        if (monster * pavise = find_pavise_shield(&you)) {
            int item_ = pavise->inv[MSLOT_SHIELD];
            if (item_ != NON_ITEM) {
                item_def& shield = mitm[item_];
                mitm[item_].flags &= ~ISFLAG_SUMMONED;
                int inv_slot;
                if ((!you.weapon() || is_shield_incompatible(*you.weapon(), &shield) == false) &&
                    merge_items_into_inv(shield, 1, inv_slot, true)) {
                    dec_mitm_item_quantity(item_, 1);
                    if (you.shield() == nullptr) {
                        equip_item(EQ_SHIELD, inv_slot, false);
                    }
                }
                else {
                    move_item_to_grid(&item_, pavise->pos());
                }
            }
            monster_die(*pavise, KILL_RESET, NON_MONSTER);
        }



        if (you_are_delayed() && current_delay()->is_run())
            env.travel_trail.push_back(you.pos());

        // Serpent's Lash = 1 means half of the wall jump time is refunded, so
        // the modifier is 2 * 1/2 = 1;
        int wall_jump_modifier =
            (did_wall_jump && you.attribute[ATTR_SERPENTS_LASH] != 1) ? 2 : 1;

        you.time_taken *= wall_jump_modifier * player_movement_speed();
        you.time_taken = div_rand_round(you.time_taken, 10);
        you.time_taken += additional_time_taken;

        if (you.running && you.running.travel_speed)
        {
            you.time_taken = max(you.time_taken,
                                 div_round_up(100, you.running.travel_speed));
        }

        if (you.duration[DUR_NO_HOP])
            you.duration[DUR_NO_HOP] += you.time_taken;

        move.reset();
        you.turn_is_over = true;
        request_autopickup();
    }

    if (!attacking && !targ_pass && !can_wall_jump && !running
        && moving && !beholder && !fmonger
        && Options.wall_jump_move
        && wu_jian_can_wall_jump_in_principle(targ))
    {
        // do messaging for a failed wall jump
        mpr(wall_jump_err);
    }

    // BCR - Easy doors single move
    if ((Options.travel_open_doors || !you.running)
        && !attacking
        && feat_is_closed_door(grd(targ)))
    {
        open_door_action(move);
        move.reset();
        return;
    }
    else if (!targ_pass && grd(targ) == DNGN_MALIGN_GATEWAY
             && !attacking && !you.is_stationary())
    {
        if (!crawl_state.disables[DIS_CONFIRMATIONS]
            && !prompt_dangerous_portal(grd(targ)))
        {
            return;
        }

        move.reset();
        you.turn_is_over = true;

        _entered_malign_portal(&you);
        return;
    }
    else if (!targ_pass && !attacking && !can_wall_jump)
    {
        if (you.is_stationary())
            canned_msg(MSG_CANNOT_MOVE);
        else if (grd(targ) == DNGN_OPEN_SEA)
            mpr("The ferocious winds and tides of the open sea thwart your progress.");
        else if (grd(targ) == DNGN_LAVA_SEA)
            mpr("The endless sea of lava is not a nice place.");
        else if (feat_is_tree(grd(targ)) && you_worship(GOD_FEDHAS))
            mpr("You cannot walk through the dense trees.");

        stop_running();
        move.reset();
        you.turn_is_over = false;
        crawl_state.cancel_cmd_repeat();
        return;
    }
    else if (beholder && !attacking && !can_wall_jump)
    {
        mprf("You cannot move away from %s!",
            beholder->name(DESC_THE).c_str());
        stop_running();
        return;
    }
    else if (fmonger && !attacking && !can_wall_jump)
    {
        mprf("You cannot move closer to %s!",
            fmonger->name(DESC_THE).c_str());
        stop_running();
        return;
    }

    if (you.running == RMODE_START)
        you.running = RMODE_CONTINUE;

    if (player_in_branch(BRANCH_ABYSS))
        maybe_shift_abyss_around_player();

    you.apply_berserk_penalty = !attacking && mantis_attack_target_mon == nullptr;

    if (!attacking && you_worship(GOD_CHEIBRIADOS) && one_chance_in(10)
        && you.run())
    {
        did_god_conduct(DID_HASTY, 1, true);
    }

    bool did_wu_jian_attack = false;
    if (!mantis_attack_target_mon && you_worship(GOD_WU_JIAN) && !attacking)
    {
        did_wu_jian_attack = wu_jian_post_move_effects(did_wall_jump,
                                                       initial_position);
    }

    if (mantis_attack_target_mon != nullptr) {
        _mantis_leap_attack_doing(mantis_attack_target_mon);
    }

    // If you actually moved you are eligible for amulet of the acrobat.
    if (!attacking && moving && !did_wu_jian_attack && !did_wall_jump && !mantis_attack_target_mon)
        update_acrobat_status();
}
