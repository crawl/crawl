/**
 * @file
 * @brief Non-enchantment spells that didn't fit anywhere else.
 *           Mostly Transmutations.
**/

#include "AppHdr.h"

#include "spl-other.h"

#include "act-iter.h"
#include "cloud.h"
#include "directn.h"
#include "delay.h"
#include "dungeon.h"
#include "english.h"
#include "env.h"
#include "food.h"
#include "god-companions.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "misc.h"
#include "makeitem.h"
#include "mercenaries.h"
#include "mon-place.h"
#include "mon-util.h"
#include "place.h"
#include "player-stats.h"
#include "potion.h"
#include "religion.h"
#include "spl-util.h"
#include "state.h"
#include "target.h"
#include "terrain.h"
#include "traps.h"
#include "god-conduct.h"
#include "items.h"
#include "item-name.h"
#include "coordit.h"

spret cast_sublimation_of_blood(int pow, bool fail)
{
    bool success = false;

    if (you.duration[DUR_DEATHS_DOOR])
        mpr("You can't draw power from your own body while in death's door.");
    else if (!you.can_bleed())
    {
        if (you.species == SP_VAMPIRE)
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


spret cast_recall(bool fail)
{
    fail_check();
    start_recall(recall_t::spell);
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
        else if (type == recall_t::caravan)
        {
            if (!(mi->props.exists(MERCENARY_FLAG) && mi->props[MERCENARY_FLAG]))
                continue;
        }

        mid_hd m(mi->mid, mi->get_experience_level());
        rlist.push_back(m);
    }

    if (type != recall_t::spell && branch_allows_followers(you.where_are_you))
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
            if (!_feat_is_passwallable(grd(pos)))
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
    if (grd(actual_dest) == DNGN_DEEP_WATER)
        terrain_msg = "You sense a deep body of water on the other side of the rock.";
    else if (grd(actual_dest) == DNGN_LAVA)
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

static int _intoxicate_monsters(coord_def where, int pow)
{
    monster* mons = monster_at(where);
    if (mons == nullptr
        || mons_intel(*mons) < I_HUMAN
        || !(mons->holiness() & MH_NATURAL)
        || mons->check_clarity()
        || monster_resists_this_poison(*mons))
    {
        return 0;
    }

    if (x_chance_in_y(40 + pow/3, 100))
    {
        mons->add_ench(mon_enchant(ENCH_CONFUSION, 0, &you));
        simple_monster_message(*mons, " looks rather confused.");
        return 1;
    }
    return 0;
}

spret cast_intoxicate(int pow, bool fail)
{
    fail_check();
    mpr("You attempt to intoxicate your foes!");
    int count = apply_area_visible([pow] (coord_def where) {
        return _intoxicate_monsters(where, pow);
    }, you.pos());
    if (count > 0)
    {
        if (x_chance_in_y(60 - pow/3, 100))
        {
            mprf(MSGCH_WARN, "The world spins around you!");
            you.increase_duration(DUR_VERTIGO, 4 + random2(20 + (100 - pow) / 10));
            you.redraw_evasion = true;
        }
    }

    return spret::success;
}

// The intent of this spell isn't to produce helpful potions
// for drinking, but rather to provide ammo for the Evaporate
// spell out of corpses, thus potentially making it useful.
// Producing helpful potions would break game balance here...
// and producing more than one potion from a corpse, or not
// using up the corpse might also lead to game balance problems. - bwr
spret cast_fulsome_distillation(bool fail)
{
    //int num_corpses = 0;
    //item_def* corpse = corpse_at(you.pos(), &num_corpses);
    //if (num_corpses && you.flight_mode() == FL_LEVITATE)
    //    num_corpses = -1;
    bool found = false;
    int co = -1;
    for (stack_iterator si(you.pos(), true); si; ++si)
    {
        if (si->is_type(OBJ_CORPSES, CORPSE_BODY))
        {
            found = true;
            co = si->index();
        }
    }

    if (!found)
    {
        mpr("There is nothing here that can be fulsome distillation!");
        return spret::abort;
    }
    item_def& corpse = mitm[co];

    // If there is only one corpse, distill it; otherwise, ask the player
    // which corpse to use.
    /*switch (num_corpses)
    {
    case 0: case -1:
        // Allow using Z to victory dance fulsome.
        if (!check_range)
        {
            fail_check();
            mpr("The spell fizzles.");
            return spret::success;
        }

        if (num_corpses == -1)
            mpr("You can't reach the corpse!");
        else
            mpr("There aren't any corpses here.");
        return spret::abort;
    case 1:
        // Use the only corpse available without prompting.
        break;
    default:
        // Search items at the player's location for corpses.
        // The last corpse detected earlier is irrelevant.
        corpse = NULL;
        for (stack_iterator si(you.pos(), true); si; ++si)
        {
            if (item_is_corpse(*si))
            {
                const std::string corpsedesc =
                    get_menu_colour_prefix_tags(*si, DESC_THE);
                const std::string prompt =
                    make_stringf("Distill a potion from %s?",
                        corpsedesc.c_str());

                if (yesno(prompt.c_str(), true, 0, false))
                {
                    corpse = &*si;
                    break;
                }
            }
        }
    }
    */

    fail_check();

    potion_type pot_type = POT_WATER;

    switch (mons_corpse_effect(corpse.mon_type))
    {
    case CE_CLEAN:
        pot_type = POT_WATER;
        break;
    case CE_NOXIOUS:
        pot_type = random2(4) < 3 ? POT_DEGENERATION : POT_POISON;
        break;
    case CE_MUTAGEN:
        pot_type = POT_UNSTABLE_MUTATION;
        break;
    case CE_NOCORPSE:       // shouldn't occur
    default:
        break;
    }

    if (pot_type != POT_UNSTABLE_MUTATION) {
        switch (corpse.mon_type)
        {
        case MONS_WASP:
            pot_type = POT_SLOWING;
            break;
        default:
            break;
        }

        struct monsterentry* smc = get_monster_data(corpse.mon_type);

        for (int nattk = 0; nattk < 4; ++nattk)
        {
            if (smc->attack[nattk].flavour == AF_POISON_MEDIUM
                || smc->attack[nattk].flavour == AF_POISON_STRONG
                || smc->attack[nattk].flavour == AF_POISON_STR
                || smc->attack[nattk].flavour == AF_POISON_INT
                || smc->attack[nattk].flavour == AF_POISON_DEX
                || smc->attack[nattk].flavour == AF_POISON_STAT)
            {
                pot_type = POT_STRONG_POISON;
            }
        }
    }

    const bool was_orc = (mons_genus(corpse.mon_type) == MONS_ORC);
    const bool was_holy = (mons_class_holiness(corpse.mon_type) == MH_HOLY);

    // We borrow the corpse's object to make our potion.
    corpse.base_type = OBJ_POTIONS;
    corpse.sub_type = pot_type;
    corpse.quantity = 1;
    corpse.plus = 0;
    corpse.plus2 = 0;
    corpse.flags = 0;
    corpse.inscription.clear();
    item_colour(corpse); // sets special as well

    // Always identify said potion.
    set_ident_type(corpse, true);

    mprf("You extract %s from the corpse.",
        corpse.name(DESC_A).c_str());

    // Try to move the potion to the player (for convenience);
    // they probably won't autopickup bad potions.
    // Treats potion as though it was being picked up manually (0005916).
    std::map<int, int> tmp_l_p = you.last_pickup;
    you.last_pickup.clear();


    if (you.last_pickup.empty())
        you.last_pickup = tmp_l_p;

    if (was_orc)
        did_god_conduct(DID_DESECRATE_ORCISH_REMAINS, 2);
    if (was_holy)
        did_god_conduct(DID_DESECRATE_HOLY_REMAINS, 2);

    return spret::success;
}


spret cast_darkness(int pow, bool fail)
{
    fail_check();
    if (you.duration[DUR_DARKNESS])
        mprf(MSGCH_DURATION, "It gets a bit darker.");
    else
        mprf(MSGCH_DURATION, "It gets dark.");
    you.increase_duration(DUR_DARKNESS, 15 + random2(1 + pow/3), 100);
    update_vision_range();

    return spret::success;
}

spret cast_wall_melting(const coord_def& pos, int pow, bool fail)
{
    fail_check();
    if (feat_is_diggable(grd(pos)) && !monster_at(pos) && in_bounds(pos)) {
        you.move_to_pos(pos);
    }
    else {
        mprf("You can only assimilate on diggable walls.");
        return spret::abort;
    }

    if (you.duration[DUR_WALL_MELTING])
        mprf(MSGCH_DURATION, "you are more in harmony with the wall.");
    else
        mprf(MSGCH_DURATION, "you are in harmony with the wall.");
    you.increase_duration(DUR_WALL_MELTING, 20 + random2(1 + pow / 5), 100);
    you.props.erase(EMERGENCY_WALL_KEY);

    return spret::success;
}


spret cast_wall_melting2(const coord_def&, int pow, bool fail)
{
    fail_check();

    if (you.duration[DUR_WALL_MELTING2])
        mprf(MSGCH_DURATION, "you are more in harmony with the wall.");
    else
        mprf(MSGCH_DURATION, "you are in harmony with the wall.");

    string msg = "(Press <w>%</w> near the wall to be invisible.)";
    insert_commands(msg, { CMD_WAIT });
    mpr(msg);
    you.increase_duration(DUR_WALL_MELTING2, 20 + random2(1 + pow / 5), 100);
    you.props.erase(WALL_INVISIBLE_KEY);

    return spret::success;
}

spret cast_phase_shift(int pow, bool fail)
{
    if (you.duration[DUR_DIMENSION_ANCHOR])
    {
        mpr("You are anchored firmly to the material plane!");
        return spret::abort;
    }

    fail_check();
    if (!you.duration[DUR_PHASE_SHIFT])
        mpr("You feel the strange sensation of being on two planes at once.");
    else
        mpr("You feel the material plane grow further away.");

    you.increase_duration(DUR_PHASE_SHIFT, 5 + random2(pow), 30);
    you.redraw_evasion = true;
    return spret::success;
}

spret cast_will_of_earth(const coord_def&, int pow, bool fail)
{
    vector<coord_def> break_walls;
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        if (!in_bounds(*ai)
            || !cell_is_solid(*ai)
            || !feat_is_diggable(grd (*ai)))
        {
            continue;
        }
        break_walls.emplace_back(*ai);
    }

    if (break_walls.size() == 0) {
        mprf("You can only use it near diggable walls.");
        return spret::abort;
    }

    fail_check();

    for (coord_def& wall : break_walls) {
        destroy_wall(wall);
        place_cloud(CLOUD_DUST, wall, 2 + random2(3), &you);
    }

    if (you.duration[DUR_WILL_OF_EARTH])
        mpr("You gathered new stones.");
    else
        mpr("You gathered stones.");

    you.set_duration(DUR_WILL_OF_EARTH, 20 + random2avg(pow, 2));
    
    you.props[WILL_OF_EARTH_POWER_KEY] = pow;
    you.props[WILL_OF_EARTH_KEY] = (int)break_walls.size();
    return spret::success;
}

static bool _find_wall_target(coord_def& target, targeter* hitfunc = nullptr)
{
    while (true)
    {
        // query for location {dlb}:
        direction_chooser_args args;
        args.restricts = DIR_TARGET;
        args.needs_path = false;
        args.top_prompt = "Create where?";
        args.hitfunc = hitfunc;
        dist beam;
        direction(beam, args);

        if (crawl_state.seen_hups)
        {
            mprf("Cancelling create due to HUP.");
            return false;
        }

        if (!beam.isValid || beam.target == you.pos())
        {
            canned_msg(MSG_OK);
            return false;
        }

        if (cell_is_solid(beam.target))
        {
            clear_messages();
            mprf("You can't create into that!");
            continue;
        }

        monster* target_mons = monster_at(beam.target);
        if (target_mons && you.can_see(*target_mons))
        {
            mprf("You can't create onto %s!",
                target_mons->name(DESC_THE).c_str());
            continue;
        }

        if (!you.see_cell_no_trans(beam.target))
        {
            clear_messages();
            if (you.trans_wall_blocking(beam.target))
                canned_msg(MSG_SOMETHING_IN_WAY);
            else
                canned_msg(MSG_CANNOT_SEE);
            continue;
        }

        target = beam.target; // Grid in los, no problem.
        return true;
    }
}

spret create_wall(bool fail)
{
    static const set<dungeon_feature_type> safe_tiles =
    {
        DNGN_SHALLOW_WATER, DNGN_FLOOR, DNGN_OPEN_DOOR,
        DNGN_OPEN_CLEAR_DOOR, 
        DNGN_DEEP_WATER, DNGN_LAVA //make floor
    };

    coord_def target;
    targeter_smite tgt(&you, 1, 0, 0);
    while (true)
    {
        if (!_find_wall_target(target, &tgt))
            return spret::abort;
        if (grid_distance(you.pos(), target) > 1)
        {
            mpr("That's out of range!");
            continue;
        }
        break;
    }

    fail_check();

    if (!safe_tiles.count(grd(target)) || feat_is_trap(grd(target)))
    {

        mpr("You cannot make wall here.");
        return spret::abort;
    }

    you.props[WILL_OF_EARTH_KEY].get_int()--;

    bool make_floor_ = (grd(target) == DNGN_DEEP_WATER ||
        grd(target) == DNGN_LAVA);

    
    int power = you.props[WILL_OF_EARTH_POWER_KEY].get_int();
    temp_change_terrain(target, make_floor_? DNGN_TEMPORARY_FLOOR : DNGN_ROCK_WALL, 50 + power + random2(power), TERRAIN_CHANGE_WALL_CREATE);

    mpr("The wall rise from the ground!");

    if (you.props[WILL_OF_EARTH_KEY].get_int() == 0) {
        you.set_duration(DUR_WILL_OF_EARTH, 0);
    }

    return spret::success;
}

spret cast_stoneskin(int pow, bool fail)
{
    if (you.form != transformation::none
        && you.form != transformation::appendage
        && you.form != transformation::statue
        && you.form != transformation::blade_hands)
    {
        mpr("This spell does not affect your current form.");
        return spret::abort;
    }

    if (you.duration[DUR_ICY_ARMOUR])
    {
        mpr("Turning your skin into stone would shatter your icy armour.");
        return spret::abort;
    }

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_LAVA_ORC)
    {
        // We can't get here from normal casting, and probably don't want
        // a message from the Helm card.
        // mpr("Your skin is naturally stony.");
        return spret::abort;
    }
#endif

    fail_check();

    if (you.duration[DUR_STONESKIN])
        mpr("Your skin feels harder.");
    else if (you.form == transformation::statue)
        mpr("Your stone body feels more resilient.");
    else
        mpr("Your skin hardens.");

    if (you.attribute[ATTR_BONE_ARMOUR] > 0)
    {
        you.attribute[ATTR_BONE_ARMOUR] = 0;
        mpr("Your corpse armour falls away.");
    }

    you.increase_duration(DUR_STONESKIN, 10 + random2(pow) + random2(pow), 50);
    you.props[STONESKIN_KEY] = pow;
    you.redraw_armour_class = true;

    return spret::success;
}

void remove_condensation_shield()
{
    mprf(MSGCH_DURATION, "Your icy shield evaporates.");
    you.duration[DUR_CONDENSATION_SHIELD] = 0;
    you.redraw_armour_class = true;
}

spret cast_condensation_shield(int pow, bool fail)
{
    if (you.shield())
    {
        mpr("You can't cast this spell while wearing a shield.");
        return spret::abort;
    }

    if (you.duration[DUR_FIRE_SHIELD])
    {
        mpr("Your ring of flames would instantly melt the ice.");
        return spret::abort;
    }

    fail_check();

    if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        mpr("The disc of vapour around you crackles some more.");
    else
        mpr("A crackling disc of dense vapour forms in the air!");
    you.increase_duration(DUR_CONDENSATION_SHIELD, 15 + random2(pow), 40);
    you.props[CONDENSATION_SHIELD_KEY] = pow;
    you.redraw_armour_class = true;

    return spret::success;
}


static bool _find_web_target(coord_def& target, targeter* hitfunc = nullptr)
{
    while (true)
    {
        // query for location {dlb}:
        direction_chooser_args args;
        args.restricts = DIR_TARGET;
        args.mode = TARG_HOSTILE;
        args.needs_path = false;
        args.top_prompt = "Create where?";
        args.hitfunc = hitfunc;
        dist beam;
        direction(beam, args);

        if (crawl_state.seen_hups)
        {
            mprf("Cancelling create due to HUP.");
            return false;
        }

        if (!beam.isValid || beam.target == you.pos())
        {
            canned_msg(MSG_OK);
            return false;
        }

        if (cell_is_solid(beam.target))
        {
            clear_messages();
            const char* feat = feat_type_name(grd(beam.target));
            mprf("You can't place the web on %s.", article_a(feat).c_str());
            continue;
        }

        if (!you.see_cell_no_trans(beam.target))
        {
            clear_messages();
            if (you.trans_wall_blocking(beam.target))
                canned_msg(MSG_SOMETHING_IN_WAY);
            else
                canned_msg(MSG_CANNOT_SEE);
            continue;
        }

        target = beam.target; // Grid in los, no problem.
        return true;
    }
}

spret create_web_trap(int power, bool fail)
{
    static const set<dungeon_feature_type> safe_tiles =
    {
        DNGN_FLOOR
    };

    coord_def target;
    targeter_smite tgt(&you, 3, 0, 0);
    while (true)
    {
        if (!_find_web_target(target, &tgt))
            return spret::abort;
        if (grid_distance(you.pos(), target) > 3)
        {
            mpr("That's out of range!");
            continue;
        }
        break;
    }

    fail_check();

    if (!safe_tiles.count(grd(target)) || feat_is_trap(grd(target)))
    {
        mpr("You cannot place the web here.");
        return spret::abort;
    }

    //power 11 ~ 200
    int web_hp = 1 + random2(2 + power/40); //min 1~2, max 1~7
    int wep_hit = 10 + power / 10; //11 ~ 30
    monster* m = monster_at(target);
    if (m && you.can_see(*m))
    {
        if (m->is_web_immune())
        {
            if (m)
            {
                if (m->is_insubstantial())
                    simple_monster_message(*m, " passes through a web.");
                else if (mons_genus(m->type) == MONS_JELLY)
                    simple_monster_message(*m, " oozes through a web.");
            }
        }
        else if (random2(m->evasion()) > wep_hit) {
            simple_monster_message(*m, " evades a web.");
        }
        else {
            if (m->visible_to(&you))
                simple_monster_message(*m, " is caught in a web!");
            else
                mpr("A web moves frantically as something is caught in it!");

            // If somehow already caught, make it worse.
            m->add_ench(ENCH_HELD);

            // Don't try to escape the web in the same turn
            m->props[NEWLY_TRAPPED_KEY] = true;
            web_hp--;
        }
    }

    if (web_hp > 0) {
        place_specific_trap(target, TRAP_WEB, web_hp);
        trap_at(target)->reveal();
    }

    return spret::success;
}