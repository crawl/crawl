/**
 * @file
 * @brief Functions for using some of the wackier inventory items.
**/

#include "AppHdr.h"

#include "evoke.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "act-iter.h"
#include "areas.h"
#include "artefact.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "dungeon.h"
#include "english.h"
#include "env.h"
#include "exercise.h"
#include "fight.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-passive.h"
#include "invent.h"
#include "item-prop.h"
#include "items.h"
#include "level-state-type.h"
#include "libutil.h"
#include "losglobal.h"
#include "message.h"
#include "mgen-data.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-clone.h"
#include "mon-pick.h"
#include "mon-place.h"
#include "mutant-beast.h"
#include "place.h"
#include "player.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-util.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "throw.h"
#ifdef USE_TILE
 #include "tilepick.h"
#endif
#include "transform.h"
#include "traps.h"
#include "unicode.h"
#include "view.h"

static bool _reaching_weapon_attack(const item_def& wpn)
{
    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return false;
    }

    if (you.caught())
    {
        mprf("You cannot attack while %s.", held_status());
        return false;
    }

    bool targ_mid = false;
    dist beam;
    const reach_type reach_range = weapon_reach(wpn);

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_HOSTILE;
    args.range = reach_range;
    args.top_prompt = "Attack whom?";
    args.self = confirm_prompt_type::cancel;

    unique_ptr<targeter> hitfunc;
    if (reach_range == REACH_TWO)
        hitfunc = make_unique<targeter_reach>(&you, reach_range);
    // Assume all longer forms of reach use smite targeting.
    else
    {
        hitfunc = make_unique<targeter_smite>(&you, reach_range, 0, 0, false,
                                              [](const coord_def& p) -> bool {
                                              return you.pos() != p; });
    }
    args.hitfunc = hitfunc.get();

    direction(beam, args);
    if (!beam.isValid)
    {
        if (beam.isCancel)
            canned_msg(MSG_OK);
        return false;
    }

    if (beam.isMe())
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return false;
    }

    const coord_def delta = beam.target - you.pos();
    const int x_distance  = abs(delta.x);
    const int y_distance  = abs(delta.y);
    monster* mons = monster_at(beam.target);
    // don't allow targeting of submerged monsters
    if (mons && mons->submerged())
        mons = nullptr;

    if (x_distance > reach_range || y_distance > reach_range)
    {
        mpr("Your weapon cannot reach that far!");
        return false;
    }

    // Failing to hit someone due to a friend blocking is infuriating,
    // shadow-boxing empty space is not (and would be abusable to wait
    // with no penalty).
    if (mons)
        you.apply_berserk_penalty = false;

    // Calculate attack delay now in case we have to apply it.
    const int attack_delay = you.attack_delay().roll();

    // Check for a monster in the way. If there is one, it blocks the reaching
    // attack 50% of the time, and the attack tries to hit it if it is hostile.
    if (reach_range < REACH_THREE && (x_distance > 1 || y_distance > 1))
    {
        const int x_first_middle = you.pos().x + (delta.x) / 2;
        const int y_first_middle = you.pos().y + (delta.y) / 2;
        const int x_second_middle = beam.target.x - (delta.x) / 2;
        const int y_second_middle = beam.target.y - (delta.y) / 2;
        const coord_def first_middle(x_first_middle, y_first_middle);
        const coord_def second_middle(x_second_middle, y_second_middle);

        if (!feat_is_reachable_past(grd(first_middle))
            && !feat_is_reachable_past(grd(second_middle)))
        {
            canned_msg(MSG_SOMETHING_IN_WAY);
            return false;
        }

        // Choose one of the two middle squares (which might be the same).
        const coord_def middle =
            !feat_is_reachable_past(grd(first_middle)) ? second_middle :
            !feat_is_reachable_past(grd(second_middle)) ? first_middle :
        random_choose(first_middle, second_middle);

        bool success = true;
        monster *midmons;
        if ((midmons = monster_at(middle))
            && !midmons->submerged()
            && coinflip())
        {
            success = false;
            beam.target = middle;
            mons = midmons;
            targ_mid = true;
            if (mons->wont_attack())
            {
                // Let's assume friendlies cooperate.
                mpr("You could not reach far enough!");
                you.time_taken = attack_delay;
                return true;
            }
        }

        if (success)
            mpr("You reach to attack!");
        else
        {
            mprf("%s is in the way.",
                 mons->observable() ? mons->name(DESC_THE).c_str()
                                    : "Something you can't see");
        }
    }

    if (mons == nullptr)
    {
        // Must return true, otherwise you get a free discovery
        // of invisible monsters.
        mpr("You attack empty space.");
        you.time_taken = attack_delay;
        return true;
    }
    else if (!fight_melee(&you, mons))
    {
        if (targ_mid)
        {
            // turn_is_over may have been reset to false by fight_melee, but
            // a failed attempt to reach further should not be free; instead,
            // charge the same as a successful attempt.
            you.time_taken = attack_delay;
            you.turn_is_over = true;
        }
        else
            return false;
    }

    return true;
}

static bool _evoke_horn_of_geryon()
{
    bool created = false;

    if (silenced(you.pos()))
    {
        mpr("You can't produce a sound!");
        return false;
    }

#if TAG_MAJOR_VERSION == 34
    const int surge = pakellas_surge_devices();
    surge_power(you.spec_evoke() + surge);
#else
    const int surge = 0;
#endif
    mprf(MSGCH_SOUND, "You produce a hideous howling noise!");
    did_god_conduct(DID_EVIL, 3);
    int num = 1;
    const int adjusted_power =
        player_adjust_evoc_power(you.skill(SK_EVOCATIONS, 10), surge);
    if (adjusted_power + random2(90) > 130)
        ++num;
    if (adjusted_power + random2(90) > 180)
        ++num;
    if (adjusted_power + random2(90) > 230)
        ++num;
    for (int n = 0; n < num; ++n)
    {
        monster* mon;
        beh_type beh = BEH_HOSTILE;

        if (random2(adjusted_power) > 7)
            beh = BEH_FRIENDLY;
        mgen_data mg(MONS_HELL_BEAST, beh, you.pos(), MHITYOU, MG_FORCE_BEH);
        mg.set_summoned(&you, 3, SPELL_NO_SPELL);
        mg.set_prox(PROX_CLOSE_TO_PLAYER);
        mon = create_monster(mg);
        if (mon)
            created = true;
    }
    if (!created)
        mpr("Nothing answers your call.");
    return true;
}

/**
 * Spray lightning in all directions. (Randomly: shock, lightning bolt, OoE.)
 *
 * @param range         The range of the beams. (As with all beams, eventually
 *                      capped at LOS.)
 * @param power         The power of the beams. (Affects damage.)
 */
static void _spray_lightning(int range, int power)
{
    const zap_type which_zap = random_choose(ZAP_SHOCK,
                                             ZAP_LIGHTNING_BOLT,
                                             ZAP_ORB_OF_ELECTRICITY);

    bolt beam;
    // range has no tracer, so randomness is ok
    beam.range = range;
    beam.source = you.pos();
    beam.target = you.pos();
    beam.target.x += random2(13) - 6;
    beam.target.y += random2(13) - 6;
    // Non-controlleable, so no player tracer.
    zapping(which_zap, power, beam);
}

/**
 * Evoke a lightning rod, creating an arc of lightning that can be sustained
 * by continuing to evoke.
 *
 * @return  Whether anything happened.
 */
static bool _lightning_rod()
{
    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return false;
    }

#if TAG_MAJOR_VERSION == 34
    const int surge = pakellas_surge_devices();
    surge_power(you.spec_evoke() + surge);
#else
    const int surge = 0;
#endif
    const int power =
        player_adjust_evoc_power(5 + you.skill(SK_EVOCATIONS, 3), surge);

    const spret ret = your_spells(SPELL_THUNDERBOLT, power, false);

    if (ret == spret::abort)
        return false;

    return true;
}

/**
 * Spray lightning in all directions around the player.
 *
 * Quantity, range & power increase with level.
 */
void black_drac_breath()
{
    const int num_shots = roll_dice(2, 1 + you.experience_level / 7);
    const int range = you.experience_level / 3 + 5; // 5--14
    const int power = 25 + you.experience_level; // 25-52
    for (int i = 0; i < num_shots; ++i)
        _spray_lightning(range, power);
}

/**
 * Returns the MP cost of zapping a wand:
 *     3 if player has MP-powered wands and enough MP available,
 *     1-2 if player has MP-powered wands and only 1-2 MP left,
 *     0 otherwise.
 */
int wand_mp_cost()
{
    // Update mutation-data.h when updating this value.
    return min(you.magic_points, you.get_mutation_level(MUT_MP_WANDS) * 3);
}

void zap_wand(int slot)
{
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (you.get_mutation_level(MUT_NO_ARTIFICE))
    {
        mpr("You cannot evoke magical items.");
        return;
    }

#if TAG_MAJOR_VERSION == 34
    if (player_under_penance(GOD_PAKELLAS))
    {
        simple_god_message("'s wrath prevents you from evoking devices!",
                           GOD_PAKELLAS);
        return;
    }
#endif

    const int mp_cost = wand_mp_cost();

    int item_slot;
    if (slot != -1)
        item_slot = slot;
    else
    {
        item_slot = prompt_invent_item("Zap which item?",
                                       menu_type::invlist,
                                       OBJ_WANDS,
                                       OPER_ZAP);
    }

    if (prompt_failed(item_slot))
        return;

    item_def& wand = you.inv[item_slot];
    if (wand.base_type != OBJ_WANDS)
    {
        mpr("You can't zap that!");
        return;
    }
    if (item_type_removed(wand.base_type, wand.sub_type))
    {
        mpr("Sorry, this wand was removed!");
        return;
    }
    // If you happen to be wielding the wand, its display might change.
    if (you.equip[EQ_WEAPON] == item_slot)
        you.wield_change = true;

    if (wand.charges <= 0)
    {
        mpr("This wand has no charges.");
        return;
    }

    int power = (15 + you.skill(SK_EVOCATIONS, 7) / 2) * (mp_cost + 9) / 9;

    const spell_type spell =
        spell_in_wand(static_cast<wand_type>(wand.sub_type));

    spret ret = your_spells(spell, power, false, &wand);

    if (ret == spret::abort)
        return;
    else if (ret == spret::fail)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        you.turn_is_over = true;
        return;
    }

    // Spend MP.
    if (mp_cost)
        dec_mp(mp_cost, false);

    // Take off a charge.
    wand.charges--;

    if (wand.charges == 0)
    {
        ASSERT(in_inventory(wand));

        mpr("The now-empty wand crumbles to dust.");
        dec_inv_item_quantity(wand.link, 1);
    }

    practise_evoking(1);
    count_action(CACT_EVOKE, EVOC_WAND);
    alert_nearby_monsters();

    you.turn_is_over = true;
}

// return a slot that has manual for given skill, or -1 if none exists
// in case of multiple manuals the one with the fewest charges is returned
int manual_slot_for_skill(skill_type skill)
{
    int slot = -1;
    int charges = -1;

    FixedVector<item_def,ENDOFPACK>::const_pointer iter = you.inv.begin();
    for (;iter!=you.inv.end(); ++iter)
    {
        if (iter->base_type != OBJ_BOOKS || iter->sub_type != BOOK_MANUAL)
            continue;

        if (iter->skill != skill || iter->skill_points == 0)
            continue;

        if (slot != -1 && iter->skill_points > charges)
            continue;

        slot = iter - you.inv.begin();
        charges = iter->skill_points;
    }

    return slot;
}

int get_all_manual_charges_for_skill(skill_type skill)
{
    int charges = 0;

    FixedVector<item_def,ENDOFPACK>::const_pointer iter = you.inv.begin();
    for (;iter!=you.inv.end(); ++iter)
    {
        if (iter->base_type != OBJ_BOOKS || iter->sub_type != BOOK_MANUAL)
            continue;

        if (iter->skill != skill || iter->skill_points == 0)
            continue;

        charges += iter->skill_points;
    }

    return charges;
}

bool skill_has_manual(skill_type skill)
{
    return manual_slot_for_skill(skill) != -1;
}

void finish_manual(int slot)
{
    item_def& manual(you.inv[slot]);
    const skill_type skill = static_cast<skill_type>(manual.plus);

    mprf("You have finished your manual of %s and toss it away.",
         skill_name(skill));
    dec_inv_item_quantity(slot, 1);
}

void get_all_manual_charges(vector<int> &charges)
{
    charges.clear();

    FixedVector<item_def,ENDOFPACK>::const_pointer iter = you.inv.begin();
    for (;iter!=you.inv.end(); ++iter)
    {
        if (iter->base_type != OBJ_BOOKS || iter->sub_type != BOOK_MANUAL)
            continue;

        charges.push_back(iter->skill_points);
    }
}

void set_all_manual_charges(const vector<int> &charges)
{
    auto charge_iter = charges.begin();
    for (item_def &item : you.inv)
    {
        if (item.base_type != OBJ_BOOKS || item.sub_type != BOOK_MANUAL)
            continue;

        ASSERT(charge_iter != charges.end());
        item.skill_points = *charge_iter;
        charge_iter++;
    }
    ASSERT(charge_iter == charges.end());
}

string manual_skill_names(bool short_text)
{
    skill_set skills;

    FixedVector<item_def,ENDOFPACK>::const_pointer iter = you.inv.begin();
    for (;iter!=you.inv.end(); ++iter)
    {
        if (iter->base_type != OBJ_BOOKS
            || iter->sub_type != BOOK_MANUAL
            || is_useless_item(*iter))
        {
            continue;
        }

        skills.insert(static_cast<skill_type>(iter->plus));
    }

    if (short_text && skills.size() > 1)
    {
        char buf[40];
        sprintf(buf, "%lu skills", (unsigned long) skills.size());
        return string(buf);
    }
    else
        return skill_names(skills);
}

static bool _box_of_beasts()
{
#if TAG_MAJOR_VERSION == 34
    const int surge = pakellas_surge_devices();
    surge_power(you.spec_evoke() + surge);
#else
    const int surge = 0;
#endif
    mpr("You open the lid...");

    // two rolls to reduce std deviation - +-6 so can get < max even at 27 sk
    int rnd_factor = random2(7);
    rnd_factor -= random2(7);
    const int hd_min = min(27,
                           player_adjust_evoc_power(
                               you.skill(SK_EVOCATIONS)
                               + rnd_factor, surge));
    const int tier = mutant_beast_tier(hd_min);
    ASSERT(tier < NUM_BEAST_TIERS);

    mgen_data mg(MONS_MUTANT_BEAST, BEH_FRIENDLY, you.pos(), MHITYOU,
                 MG_AUTOFOE);
    mg.set_summoned(&you, 3 + random2(3), 0);

    mg.hd = beast_tiers[tier];
    dprf("hd %d (min %d, tier %d)", mg.hd, hd_min, tier);
    const monster* mons = create_monster(mg);

    if (!mons)
    {
        // Failed to create monster for some reason
        mpr("...but nothing happens.");
        return false;
    }

    mprf("...and %s %s out!",
         mons->name(DESC_A).c_str(), mons->airborne() ? "flies" : "leaps");
    did_god_conduct(DID_CHAOS, random_range(5,10));

    return true;
}

static bool _make_zig(item_def &zig)
{
    if (feat_is_critical(grd(you.pos())))
    {
        mpr("You can't place a gateway to a ziggurat here.");
        return false;
    }
    for (int lev = 1; lev <= brdepth[BRANCH_ZIGGURAT]; lev++)
    {
        if (is_level_on_stack(level_id(BRANCH_ZIGGURAT, lev))
            || you.where_are_you == BRANCH_ZIGGURAT)
        {
            mpr("Finish your current ziggurat first!");
            return false;
        }
    }

    ASSERT(in_inventory(zig));
    dec_inv_item_quantity(zig.link, 1);
    dungeon_terrain_changed(you.pos(), DNGN_ENTER_ZIGGURAT);
    mpr("You set the figurine down, and a mystic portal to a ziggurat forms.");
    return true;
}

struct dist_sorter
{
    coord_def pos;
    bool operator()(const actor* a, const actor* b)
    {
        return a->pos().distance_from(pos) > b->pos().distance_from(pos);
    }
};

static int _gale_push_dist(const actor* agent, const actor* victim, int pow)
{
    int dist = 1 + random2(pow / 20);

    if (victim->airborne())
        dist++;

    if (victim->body_size(PSIZE_BODY) < SIZE_MEDIUM)
        dist++;
    else if (victim->body_size(PSIZE_BODY) > SIZE_BIG)
        dist /= 2;
    else if (victim->body_size(PSIZE_BODY) > SIZE_MEDIUM)
        dist -= 1;

    int range = victim->pos().distance_from(agent->pos());
    if (range > 5)
        dist -= 2;
    else if (range > 2)
        dist--;

    if (dist < 0)
        return 0;
    else
        return dist;
}

static double _angle_between(coord_def origin, coord_def p1, coord_def p2)
{
    double ang0 = atan2(p1.x - origin.x, p1.y - origin.y);
    double ang  = atan2(p2.x - origin.x, p2.y - origin.y);
    return min(fabs(ang - ang0), fabs(ang - ang0 + 2 * PI));
}

void wind_blast(actor* agent, int pow, coord_def target, bool card)
{
    vector<actor *> act_list;

    int radius = min(5, 4 + div_rand_round(pow, 60));

    for (actor_near_iterator ai(agent->pos(), LOS_SOLID); ai; ++ai)
    {
        if (ai->is_stationary()
            || ai->pos().distance_from(agent->pos()) > radius
            || ai->pos() == agent->pos() // so it's never aimed_at_feet
            || !target.origin()
               && _angle_between(agent->pos(), target, ai->pos()) > PI/4.0)
        {
            continue;
        }

        act_list.push_back(*ai);
    }

    dist_sorter sorter = {agent->pos()};
    sort(act_list.begin(), act_list.end(), sorter);

    bolt wind_beam;
    wind_beam.hit             = AUTOMATIC_HIT;
    wind_beam.pierce          = true;
    wind_beam.affects_nothing = true;
    wind_beam.source          = agent->pos();
    wind_beam.range           = LOS_RADIUS;
    wind_beam.is_tracer       = true;

    map<actor *, coord_def> collisions;

    bool player_affected = false;
    counted_monster_list affected_monsters;

    for (actor *act : act_list)
    {
        wind_beam.target = act->pos();
        wind_beam.fire();

        int push = _gale_push_dist(agent, act, pow);
        bool pushed = false;

        for (unsigned int j = 0; j < wind_beam.path_taken.size() - 1 && push;
             ++j)
        {
            if (wind_beam.path_taken[j] == act->pos())
            {
                coord_def newpos = wind_beam.path_taken[j+1];
                if (!actor_at(newpos) && !cell_is_solid(newpos)
                    && act->can_pass_through(newpos)
                    && act->is_habitable(newpos))
                {
                    act->move_to_pos(newpos);
                    if (act->is_player())
                        stop_delay(true);
                    --push;
                    pushed = true;
                }
                else //Try to find an alternate route to push
                {
                    bool success = false;
                    for (adjacent_iterator di(newpos); di; ++di)
                    {
                        if (adjacent(*di, act->pos())
                            && di->distance_from(agent->pos())
                                == newpos.distance_from(agent->pos())
                            && !actor_at(*di) && !cell_is_solid(*di)
                            && act->can_pass_through(*di)
                            && act->is_habitable(*di))
                        {
                            act->move_to_pos(*di);
                            if (act->is_player())
                                stop_delay(true);

                            --push;
                            pushed = true;

                            // Adjust wind path for moved monster
                            wind_beam.target = *di;
                            wind_beam.fire();
                            success = true;
                            break;
                        }
                    }

                    // If no luck, they slam into something.
                    if (!success)
                        collisions.insert(make_pair(act, newpos));
                }
            }
        }

        if (pushed)
        {
            if (act->is_monster())
            {
                act->as_monster()->speed_increment -= random2(6) + 4;
                if (you.can_see(*act))
                    affected_monsters.add(act->as_monster());
            }
            else
                player_affected = true;
        }
    }

    // Now move clouds
    vector<coord_def> cloud_list;
    for (distance_iterator di(agent->pos(), true, false, radius + 2); di; ++di)
    {
        if (cloud_at(*di)
            && cell_see_cell(agent->pos(), *di, LOS_SOLID)
            && (target.origin()
                || _angle_between(agent->pos(), target, *di) <= PI/4.0))
        {
            cloud_list.push_back(*di);
        }
    }

    for (int i = cloud_list.size() - 1; i >= 0; --i)
    {
        wind_beam.target = cloud_list[i];
        wind_beam.fire();

        int dist = cloud_list[i].distance_from(agent->pos());
        int push = (dist > 5 ? 2 : dist > 2 ? 3 : 4);

        if (dist == 0 && agent->is_player())
        {
            delete_cloud(agent->pos());
            break;
        }

        for (unsigned int j = 0;
             j < wind_beam.path_taken.size() - 1 && push;
             ++j)
        {
            if (wind_beam.path_taken[j] == cloud_list[i])
            {
                coord_def newpos = wind_beam.path_taken[j+1];
                if (!cell_is_solid(newpos)
                    && !cloud_at(newpos))
                {
                    swap_clouds(newpos, wind_beam.path_taken[j]);
                    --push;
                }
                else //Try to find an alternate route to push
                {
                    for (distance_iterator di(wind_beam.path_taken[j],
                         false, true, 1); di; ++di)
                    {
                        if (di->distance_from(agent->pos())
                                == newpos.distance_from(agent->pos())
                            && *di != agent->pos() // never aimed_at_feet
                            && !cell_is_solid(*di)
                            && !cloud_at(*di))
                        {
                            swap_clouds(*di, wind_beam.path_taken[j]);
                            --push;
                            wind_beam.target = *di;
                            wind_beam.fire();
                            j--;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (agent->is_player())
    {
        const string source = card ? "card" : "fan";

        if (pow > 120)
            mprf("A mighty gale blasts forth from the %s!", source.c_str());
        else
            mprf("A fierce wind blows from the %s.", source.c_str());
    }

    noisy(8, agent->pos());

    if (player_affected)
        mpr("You are blown backwards!");

    if (!affected_monsters.empty())
    {
        const string message =
            make_stringf("%s %s blown away by the wind.",
                         affected_monsters.describe().c_str(),
                         conjugate_verb("be", affected_monsters.count() > 1).c_str());
        if (strwidth(message) < get_number_of_cols() - 2)
            mpr(message);
        else
            mpr("The monsters around you are blown away!");
    }

    for (auto it : collisions)
        if (it.first->alive())
            it.first->collide(it.second, agent, pow);
}

static bool _phial_of_floods()
{
    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return false;
    }

    dist target;
    bolt beam;

    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return false;
    }

    const int base_pow = 10 + you.skill(SK_EVOCATIONS, 4); // placeholder?
    zappy(ZAP_PRIMAL_WAVE, base_pow, false, beam);
    beam.range = LOS_RADIUS;
    beam.aimed_at_spot = true;

    direction_chooser_args args;
    args.mode = TARG_HOSTILE;
    args.top_prompt = "Aim the phial where?";

    if (spell_direction(target, beam, &args)
        && player_tracer(ZAP_PRIMAL_WAVE, base_pow, beam))
    {
#if TAG_MAJOR_VERSION == 34
        const int surge = pakellas_surge_devices();
        surge_power(you.spec_evoke() + surge);
#else
        const int surge = 0;
#endif
        const int power = player_adjust_evoc_power(base_pow, surge);
        // use real power to recalc hit/dam
        zappy(ZAP_PRIMAL_WAVE, power, false, beam);
        beam.fire();

        return true;
    }

    return false;
}

static spret _phantom_mirror()
{
    bolt beam;
    monster* victim = nullptr;
    dist spd;
    targeter_smite tgt(&you, LOS_RADIUS, 0, 0);

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.needs_path = false;
    args.self = confirm_prompt_type::cancel;
    args.top_prompt = "Aiming: <white>Phantom Mirror</white>";
    args.hitfunc = &tgt;
    if (!spell_direction(spd, beam, &args))
        return spret::abort;
    victim = monster_at(beam.target);
    if (!victim || !you.can_see(*victim))
    {
        if (beam.target == you.pos())
            mpr("You can't use the mirror on yourself.");
        else
            mpr("You can't see anything there to clone.");
        return spret::abort;
    }

    // Mirrored monsters (including by Mara, rakshasas) can still be
    // re-reflected.
    if (!actor_is_illusion_cloneable(victim)
        && !victim->has_ench(ENCH_PHANTOM_MIRROR))
    {
        mpr("The mirror can't reflect that.");
        return spret::abort;
    }

#if TAG_MAJOR_VERSION == 34
    const int surge = pakellas_surge_devices();
    surge_power(you.spec_evoke() + surge);
#else
    const int surge = 0;
#endif

    monster* mon = clone_mons(victim, true, nullptr, ATT_FRIENDLY);
    if (!mon)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::fail;
    }
    const int power = player_adjust_evoc_power(5 + you.skill(SK_EVOCATIONS, 3),
                                               surge);
    int dur = min(6, max(1,
                         player_adjust_evoc_power(
                             you.skill(SK_EVOCATIONS, 1) / 4 + 1,
                             surge)
                         * (100 - victim->check_res_magic(power)) / 100));

    mon->mark_summoned(dur, true, SPELL_PHANTOM_MIRROR);

    mon->summoner = MID_PLAYER;
    mons_add_blame(mon, "mirrored by the player character");
    mon->add_ench(ENCH_PHANTOM_MIRROR);
    mon->add_ench(mon_enchant(ENCH_DRAINED,
                              div_rand_round(mon->get_experience_level(), 3),
                              &you, INFINITE_DURATION));

    mon->behaviour = BEH_SEEK;
    set_nearest_monster_foe(mon);

    mprf("You reflect %s with the mirror, and the mirror shatters!",
         victim->name(DESC_THE).c_str());

    return spret::success;
}

/**
 * Find the cell at range 3 closest to the center of mass of monsters in range,
 * or a random range 3 cell if there are none.
 *
 * @param see_targets a boolean parameter indicating if the user can see any of
 * the targets
 * @return The cell in question.
 */
static coord_def _find_tremorstone_target(bool& see_targets)
{
    coord_def com = {0, 0};
    see_targets = false;
    int num = 0;

    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (monster_at(*ri) && !mons_is_firewood(*monster_at(*ri)))
        {
            com += *ri;
            see_targets = see_targets || you.can_see(*monster_at(*ri));
            ++num;
        }
    }

    coord_def target = {0, 0};
    int distance = LOS_RADIUS * num;
    int ties = 0;

    for (radius_iterator ri(you.pos(), 3, C_SQUARE, LOS_NO_TRANS, true); ri; ++ri)
    {
        if (ri->distance_from(you.pos()) != 3 || cell_is_solid(*ri))
            continue;

        if (num > 0)
        {
            if (com.distance_from((*ri) * num) < distance)
            {
                ties = 1;
                target = *ri;
                distance = com.distance_from((*ri) * num);
            }
            else if (com.distance_from((*ri) * num) == distance
                     && one_chance_in(++ties))
            {
                target = *ri;
            }
        }
        else if (one_chance_in(++ties))
            target = *ri;
    }

    return target;
}

/**
 * Find an adjacent tile for a tremorstone explosion to go off in.
 *
 * @param center    The original target of the stone.
 * @return          The new, final origin of the stone's explosion.
 */
static coord_def _fuzz_tremorstone_target(coord_def center)
{
    coord_def chosen = center;
    int seen = 1;
    for (adjacent_iterator ai(center); ai; ++ai)
        if (!cell_is_solid(*ai) && one_chance_in(++seen))
            chosen = *ai;
    return chosen;
}

/**
 * Number of explosions, scales up from 1 at 0 evo to 6 at 27 evo,
 * via a stepdown.
 *
 * Currently pow is just evo + 15, but the abstraction is kept around in
 * case an evocable enhancer returns to the game so that 0 evo with enhancer
 * gets some amount of enhancement.
 */
static int _tremorstone_count(int pow)
{
    return 1 + stepdown((pow - 15) / 3, 2, ROUND_CLOSE);
}

/**
 * Evokes a tremorstone, blasting something in the general area of a
 * chosen target.
 *
 * @return          spret::abort if the player cancels, spret::fail if they
 *                  try to evoke but fail, and spret::success otherwise.
 */
static spret _tremorstone()
{
    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return spret::abort;
    }

    bool see_target;
    bolt beam;

    static const int RADIUS = 2;
    static const int SPREAD = 1;
    static const int RANGE = RADIUS + SPREAD;
    const int pow = 15 + you.skill(SK_EVOCATIONS);
    const int adjust_pow = player_adjust_evoc_power(pow);
    const int num_explosions = _tremorstone_count(adjust_pow);

    beam.source_id  = MID_PLAYER;
    beam.thrower    = KILL_YOU;
    zappy(ZAP_TREMORSTONE, pow, false, beam);
    beam.range = RANGE;
    beam.ex_size = RADIUS;
    beam.target = _find_tremorstone_target(see_target);

    targeter_radius hitfunc(&you, LOS_NO_TRANS);
    auto vulnerable = [](const actor *act) -> bool
    {
        return !(have_passive(passive_t::shoot_through_plants)
                 && fedhas_protects(act->as_monster()));
    };
    if ((!see_target
        && !yesno("You can't see anything, throw a tremorstone anyway?",
                 true, 'n'))
        || stop_attack_prompt(hitfunc, "throw a tremorstone", vulnerable))
    {
        return spret::abort;
    }

    mpr("The tremorstone explodes into fragments!");
    const coord_def center = beam.target;

    for (int i = 0; i < num_explosions; i++)
    {
        bolt explosion = beam;
        explosion.target = _fuzz_tremorstone_target(center);
        explosion.explode(i == num_explosions - 1);
    }

    return spret::success;
}

random_pick_entry<cloud_type> condenser_clouds[] =
{
  { 0,   50, 200, FALL, CLOUD_MEPHITIC },
  { 0,  100, 125, PEAK, CLOUD_FIRE },
  { 0,  100, 125, PEAK, CLOUD_COLD },
  { 0,  100, 125, PEAK, CLOUD_POISON },
  { 0,  110, 50, RISE, CLOUD_NEGATIVE_ENERGY },
  { 0,  110, 50, RISE, CLOUD_STORM },
  { 0,  110, 50, RISE, CLOUD_ACID },
  { 0,0,0,FLAT,CLOUD_NONE }
};

static spret _condenser()
{
    if (env.level_state & LSTATE_STILL_WINDS)
    {
        mpr("The air is too still to form clouds.");
        return spret::abort;
    }

    const int pow = 15 + you.skill(SK_EVOCATIONS, 7) / 2;
    const int adjust_pow = min(110,player_adjust_evoc_power(pow));

    random_picker<cloud_type, NUM_CLOUD_TYPES> cloud_picker;
    cloud_type cloud = cloud_picker.pick(condenser_clouds, adjust_pow, CLOUD_NONE);

    vector<coord_def> target_cells;
    bool see_targets = false;

    for (radius_iterator di(you.pos(), LOS_NO_TRANS); di; ++di)
    {
        monster *mons = monster_at(*di);

        if (!mons || mons->wont_attack() || !mons_is_threatening(*mons))
            continue;

        if (you.can_see(*mons))
            see_targets = true;

        for (adjacent_iterator ai(mons->pos(), false); ai; ++ai)
        {
            actor * act = actor_at(*ai);
            if (!cell_is_solid(*ai) && you.see_cell(*ai) && !cloud_at(*ai)
                && !(act && act->wont_attack()))
            {
                target_cells.push_back(*ai);
            }
        }
    }

    if (!see_targets
        && !yesno("You can't see anything. Try to condense clouds anyway?",
                  true, 'n'))
    {
        return spret::abort;
    }

    if (target_cells.empty())
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::fail;
    }

    for (auto p : target_cells)
    {
        const int cloud_power = 5
            + random2avg(12 + div_rand_round(adjust_pow * 3, 4), 3);
        place_cloud(cloud, p, cloud_power, &you);
    }

    mprf("Clouds of %s condense around you!", cloud_type_name(cloud).c_str());

    return spret::success;
}

bool evoke_check(int slot, bool quiet)
{
    const bool reaching = slot != -1 && slot == you.equip[EQ_WEAPON]
                          && !you.melded[EQ_WEAPON]
                          && weapon_reach(*you.weapon()) > REACH_NONE;

    if (you.berserk() && !reaching)
    {
        if (!quiet)
            canned_msg(MSG_TOO_BERSERK);
        return false;
    }
    return true;
}

bool evoke_item(int slot)
{
    if (!evoke_check(slot))
        return false;

    if (slot == -1)
    {
        slot = prompt_invent_item("Evoke which item? (* to show all)",
                                   menu_type::invlist,
                                   OSEL_EVOKABLE, OPER_EVOKE);

        if (prompt_failed(slot))
            return false;
    }
    else if (!check_warning_inscriptions(you.inv[slot], OPER_EVOKE))
        return false;

    ASSERT(slot >= 0);

#ifdef ASSERTS // Used only by an assert
    const bool wielded = (you.equip[EQ_WEAPON] == slot);
#endif /* DEBUG */

    item_def& item = you.inv[slot];
    // Also handles messages.
    if (!item_is_evokable(item, true, false, true))
        return false;

    bool did_work   = false;  // "Nothing happens" message
    bool unevokable = false;

    const unrandart_entry *entry = is_unrandom_artefact(item)
        ? get_unrand_entry(item.unrand_idx) : nullptr;

    if (entry && entry->evoke_func)
    {
        ASSERT(item_is_equipped(item));

        if (you.confused())
        {
            canned_msg(MSG_TOO_CONFUSED);
            return false;
        }

        bool qret = entry->evoke_func(&item, &did_work, &unevokable);

        if (!unevokable)
            count_action(CACT_EVOKE, item.unrand_idx);

        // what even _is_ this return value?
        if (qret)
            return did_work;
    }
    else switch (item.base_type)
    {
    case OBJ_WANDS:
        zap_wand(slot);
        return true;

    case OBJ_WEAPONS:
        ASSERT(wielded);

        if (weapon_reach(item) > REACH_NONE)
        {
            if (_reaching_weapon_attack(item))
                did_work = true;
            else
                return false;
        }
        else
            unevokable = true;
        break;

    case OBJ_STAVES:
        ASSERT(wielded);
        if (item.sub_type != STAFF_ENERGY)
        {
            unevokable = true;
            break;
        }

        if (you.confused())
        {
            canned_msg(MSG_TOO_CONFUSED);
            return false;
        }

        if (you.magic_points >= you.max_magic_points)
        {
            canned_msg(MSG_FULL_MAGIC);
            return false;
        }
        else if (x_chance_in_y(apply_enhancement(
                                   you.skill(SK_EVOCATIONS, 100) + 1100,
                                   you.spec_evoke()),
                               4000))
        {
            mpr("You channel some magical energy.");
            inc_mp(1 + random2(3));
            did_work = true;
            practise_evoking(1);
            count_action(CACT_EVOKE, STAFF_ENERGY, OBJ_STAVES);

            did_god_conduct(DID_WIZARDLY_ITEM, 10);
            did_god_conduct(DID_CHANNEL, 1, true);
        }
        break;

    case OBJ_MISCELLANY:
        did_work = true; // easier to do it this way for misc items

        if (item.sub_type != MISC_ZIGGURAT)
        {
            if (you.get_mutation_level(MUT_NO_ARTIFICE))
            {
                mpr("You cannot evoke magical items.");
                return false;
            }
#if TAG_MAJOR_VERSION == 34
            if (player_under_penance(GOD_PAKELLAS))
            {
                simple_god_message("'s wrath prevents you from evoking "
                                   "devices!", GOD_PAKELLAS);
                return false;
            }
#endif
        }

        switch (item.sub_type)
        {
#if TAG_MAJOR_VERSION == 34
        case MISC_BOTTLED_EFREET:
            canned_msg(MSG_NOTHING_HAPPENS);
            return false;

        case MISC_FAN_OF_GALES:
            canned_msg(MSG_NOTHING_HAPPENS);
            return false;

        case MISC_LAMP_OF_FIRE:
            canned_msg(MSG_NOTHING_HAPPENS);
            return false;

        case MISC_STONE_OF_TREMORS:
            canned_msg(MSG_NOTHING_HAPPENS);
            return false;
#endif

        case MISC_PHIAL_OF_FLOODS:
            if (!evoker_charges(item.sub_type))
            {
                mpr("That is presently inert.");
                return false;
            }
            if (_phial_of_floods())
            {
                expend_xp_evoker(item.sub_type);
                practise_evoking(3);
            }
            else
                return false;
            break;

        case MISC_HORN_OF_GERYON:
            if (!evoker_charges(item.sub_type))
            {
                mpr("That is presently inert.");
                return false;
            }
            if (_evoke_horn_of_geryon())
            {
                expend_xp_evoker(item.sub_type);
                practise_evoking(3);
            }
            else
                return false;
            break;

        case MISC_BOX_OF_BEASTS:
            if (!evoker_charges(item.sub_type))
            {
                mpr("That is presently inert.");
                return false;
            }
            if (_box_of_beasts())
            {
                expend_xp_evoker(item.sub_type);
                if (!evoker_charges(item.sub_type))
                    mpr("The box is emptied!");
                practise_evoking(1);
            }
            break;

#if TAG_MAJOR_VERSION == 34
        case MISC_SACK_OF_SPIDERS:
            canned_msg(MSG_NOTHING_HAPPENS);
            return false;

        case MISC_CRYSTAL_BALL_OF_ENERGY:
            canned_msg(MSG_NOTHING_HAPPENS);
            return false;
#endif

        case MISC_LIGHTNING_ROD:
            if (!evoker_charges(item.sub_type))
            {
                mpr("That is presently inert.");
                return false;
            }
            if (_lightning_rod())
            {
                practise_evoking(1);
                expend_xp_evoker(item.sub_type);
                if (!evoker_charges(item.sub_type))
                    mpr("The lightning rod overheats!");
            }
            else
                return false;
            break;

        case MISC_QUAD_DAMAGE:
            mpr("QUAD DAMAGE!");
            you.duration[DUR_QUAD_DAMAGE] = 30 * BASELINE_DELAY;
            ASSERT(in_inventory(item));
            dec_inv_item_quantity(item.link, 1);
            invalidate_agrid(true);
            break;

        case MISC_PHANTOM_MIRROR:
            if (!evoker_charges(item.sub_type))
            {
                mpr("That is presently inert.");
                return false;
            }
            switch (_phantom_mirror())
            {
                default:
                case spret::abort:
                    return false;

                case spret::success:
                    expend_xp_evoker(item.sub_type);
                    if (!evoker_charges(item.sub_type))
                        mpr("The mirror clouds!");
                    // deliberate fall-through
                case spret::fail:
                    practise_evoking(1);
                    break;
            }
            break;

        case MISC_ZIGGURAT:
            // Don't set did_work to false, _make_zig handles the message.
            unevokable = !_make_zig(item);
            break;

        case MISC_TIN_OF_TREMORSTONES:
            if (!evoker_charges(item.sub_type))
            {
                mpr("That is presently inert.");
                return false;
            }
            switch (_tremorstone())
            {
                default:
                case spret::abort:
                    return false;

                case spret::success:
                    expend_xp_evoker(item.sub_type);
                    if (!evoker_charges(item.sub_type))
                        mpr("The tin is emptied!");
                case spret::fail:
                    practise_evoking(1);
                    break;
            }
            break;

        case MISC_CONDENSER_VANE:
            if (!evoker_charges(item.sub_type))
            {
                mpr("That is presently inert.");
                return false;
            }
            switch (_condenser())
            {
                default:
                case spret::abort:
                    return false;

                case spret::success:
                    expend_xp_evoker(item.sub_type);
                    if (!evoker_charges(item.sub_type))
                        mpr("The condenser dries out!");
                case spret::fail:
                    practise_evoking(1);
                    break;
            }
            break;

        default:
            did_work = false;
            unevokable = true;
            break;
        }
        if (did_work && !unevokable)
            count_action(CACT_EVOKE, item.sub_type, OBJ_MISCELLANY);
        break;

    default:
        unevokable = true;
        break;
    }

    if (!did_work)
        canned_msg(MSG_NOTHING_HAPPENS);

    if (!unevokable)
        you.turn_is_over = true;
    else
        crawl_state.zero_turns_taken();

    return did_work;
}
