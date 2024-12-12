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
#include "art-enum.h"
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
#include "god-item.h"
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
#include "nearby-danger.h" // i_feel_safe
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
#include "spl-monench.h" // FASTROOT_POWER_KEY
#include "spl-util.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "tag-version.h"
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

#define PHIAL_RANGE 5

static bool _evoke_horn_of_geryon()
{
    if (stop_summoning_prompt(MR_NO_FLAGS, M_NO_FLAGS, "toot the horn"))
        return false;
    // Fool! Geryon toots as he pleases!

    bool created = false;

    mprf(MSGCH_SOUND, "You produce a hideous howling noise!");
    noisy(15, you.pos()); // same as hell effect noise
    did_god_conduct(DID_EVIL, 3);
    int num = 1;
    const int adjusted_power = you.skill(SK_EVOCATIONS, 10);
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
        mgen_data mg(MONS_SIN_BEAST, beh, you.pos(), MHITYOU, MG_AUTOFOE);
        mg.set_summoned(&you, SPELL_NO_SPELL, summ_dur(3));
        mg.set_prox(PROX_CLOSE_TO_PLAYER);
        mon = create_monster(mg);
        if (mon)
            created = true;
    }
    if (!created)
        mpr("Nothing answers your call.");
    return true;
}

static int _lightning_rod_power()
{
    return 5 + you.skill(SK_EVOCATIONS, 3);
}

/**
 * Evoke a lightning rod, creating an arc of lightning that can be sustained
 * by continuing to evoke.
 *
 * @return  Whether anything happened.
 */
static bool _lightning_rod(dist *preselect)
{
    const spret ret = your_spells(SPELL_THUNDERBOLT, _lightning_rod_power(),
            false, nullptr, preselect);

    if (ret == spret::abort)
        return false;

    return true;
}

/**
 * Returns the MP cost of zapping a wand, depending on the player's MP-powered wands
 * level and their available MP (or HP, if they're a djinn).
 */
int wand_mp_cost()
{
    const int cost = you.get_mutation_level(MUT_MP_WANDS) * 3;
    if (you.has_mutation(MUT_HP_CASTING))
        return min(you.hp - 1, cost);
    // Update mutation-data.h when updating this value.
    return min(you.magic_points, cost);
}

int wand_power(spell_type wand_spell)
{
    const int cap = spell_power_cap(wand_spell);
    if (cap == 0)
        return -1;
    const int mp_cost = wand_mp_cost();
    int pow = (15 + you.skill(SK_EVOCATIONS, 7) / 2) * (mp_cost + 9) / 9;
    if (you.unrand_equipped(UNRAND_GADGETEER))
        pow = pow * 130 / 100;
    return min(pow, cap);
}

void zap_wand(int slot, dist *_target)
{
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED); // why is this handled here??
        return;
    }

    if (!item_currently_evokable(slot == -1 ? nullptr : &you.inv[slot]))
        return;

    int item_slot;
    if (slot != -1)
        item_slot = slot;
    else
    {
        // TODO: move to UseItemMenu? This code path is currently unbound and
        // it's unclear to me if anyone ever uses it
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

    if (!item_currently_evokable(&wand))
        return;

    const int mp_cost = wand_mp_cost();
    const spell_type spell =
        spell_in_wand(static_cast<wand_type>(wand.sub_type));
    const int power = wand_power(spell);
    pay_mp(mp_cost);

    if (spell == SPELL_FASTROOT)
        you.props[FASTROOT_POWER_KEY] = power; // we may cancel, but that's fine

    spret ret = your_spells(spell, power, false, &wand, _target);

    if (ret == spret::abort)
    {
        refund_mp(mp_cost);
        return;
    }
    else if (ret == spret::fail)
    {
        refund_mp(mp_cost);
        canned_msg(MSG_NOTHING_HAPPENS);
        you.turn_is_over = true;
        return;
    }

    // Spend MP.
    if (mp_cost)
        finalize_mp_cost();

    // Take off a charge (unless gadgeteer procs)
    if ((you.wearing_ego(OBJ_GIZMOS, SPGIZMO_GADGETEER)
        || you.unrand_equipped(UNRAND_GADGETEER))
        && x_chance_in_y(3, 10))
    {
        mpr("You conserve a charge of your wand.");
    }
    else
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

string manual_skill_names(bool short_text)
{
    skill_set skills;
    for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; sk++)
        if (you.skill_manual_points[sk])
            skills.insert(sk);

    if (short_text && skills.size() > 1)
        return make_stringf("%d skills", static_cast<int>(skills.size()));
    else
        return skill_names(skills);
}

static bool _box_of_beasts()
{
    if (stop_summoning_prompt(MR_NO_FLAGS, M_NO_FLAGS, "open the box"))
        return false;

    mpr("You open the lid...");

    // two rolls to reduce std deviation - +-6 so can get < max even at 27 sk
    int rnd_factor = random2(7);
    rnd_factor -= random2(7);
    const int hd_min = min(27, you.skill(SK_EVOCATIONS) + rnd_factor);
    const int tier = mutant_beast_tier(hd_min);
    ASSERT(tier < NUM_BEAST_TIERS);

    mgen_data mg(MONS_MUTANT_BEAST, BEH_FRIENDLY, you.pos(), MHITYOU,
                 MG_AUTOFOE);
    mg.set_summoned(&you, 0, summ_dur(3 + random2(3)));

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

// Generate webs on hostile monsters and trap them.
static bool _place_webs()
{
    bool webbed = false;
    const int evo_skill = you.skill(SK_EVOCATIONS);
    // At 0 evo skill, this is about a 1/3 chance of webbing each
    // enemy. At 27 skill, it's about an 9/10 chance.
    const int web_chance = 36 + evo_skill * 2;
    const int max_range = LOS_DEFAULT_RANGE / 2 + 2;
    for (monster_near_iterator mi(you.pos(), LOS_SOLID); mi; ++mi)
    {
        trap_def *trap = trap_at((*mi)->pos());
        // Don't destroy non-web traps or try to trap monsters
        // currently caught by something.
        if (you.pos().distance_from((*mi)->pos()) > max_range
            || (!trap && env.grid((*mi)->pos()) != DNGN_FLOOR)
            || (trap && trap->type != TRAP_WEB)
            || (*mi)->friendly()
            || (*mi)->caught())
        {
            continue;
        }

        if (!x_chance_in_y(web_chance, 100))
            continue;

        if (trap && trap->type == TRAP_WEB)
            destroy_trap((*mi)->pos());

        place_specific_trap((*mi)->pos(), TRAP_WEB, 1); // 1 ammo = temp
        // Reveal the trap
        env.grid((*mi)->pos()) = DNGN_TRAP_WEB;
        trap = trap_at((*mi)->pos());
        trap->trigger(**mi);
        webbed = true;
    }
    return webbed;
}

static const vector<pop_entry> pop_spiders =
{ // Sack of Spiders
  // tier zero :(
  {  0,  13,  200, FALL, MONS_SCORPION },
  // tier one
  {  0,  18,  100, PEAK, MONS_REDBACK },
  {  0,  18,  100, PEAK, MONS_CULICIVORA },
  // tier two
  {  6,  13,  100, RISE, MONS_JUMPING_SPIDER },
  { 14,  27,  100, FALL, MONS_JUMPING_SPIDER },
  {  6,  13,  100, RISE, MONS_TARANTELLA },
  { 14,  27,  100, FALL, MONS_TARANTELLA },
  // tier three
  { 13,  20,  100, RISE, MONS_WOLF_SPIDER },
  { 13,  20,  100, RISE, MONS_STEELBARB_WORM },
  { 21,  33,  100, FALL, MONS_WOLF_SPIDER },
  { 21,  33,  100, FALL, MONS_STEELBARB_WORM },
  // tier MAXX
  { 18,  27,  100, RISE, MONS_DEMONIC_CRAWLER },
  { 21,  27,  100, RISE, MONS_BROODMOTHER },
};

static bool _sack_of_spiders_veto_mon(monster_type mon)
{
   // Don't summon any beast that would anger your god.
    return god_hates_monster(mon);
}

static bool _spill_out_spiders()
{
    const int evo_skill = you.skill_rdiv(SK_EVOCATIONS);
    // 2 at min skill, 3-4 at mid, 4-6 at max
    const int min_pals = 2 + div_rand_round(2 * evo_skill, 27);
    const int max_buds = 2 + div_rand_round(4 * evo_skill, 27);
    const int n_mons = random_range(min_pals, max(min_pals, max_buds));
    bool made_mons = false;
    for (int n = 0; n < n_mons; n++)
    {
        // Invoke mon-pick with our custom list
        monster_type mon = pick_monster_from(pop_spiders, evo_skill,
                                             _sack_of_spiders_veto_mon);
        mgen_data mg(mon, BEH_FRIENDLY, you.pos(), MHITYOU, MG_AUTOFOE);
        mg.set_summoned(&you, 0, summ_dur(3 + random2(4)));
        if (create_monster(mg))
            made_mons = true;
    }
    return made_mons;
}

static bool _sack_of_spiders()
{
    if (stop_summoning_prompt(MR_NO_FLAGS, M_NO_FLAGS, "reach into the sack"))
        return false;

    mpr("You reach into the sack...");

    const bool made_mons = !you.allies_forbidden() && _spill_out_spiders();
    if (made_mons)
        mpr("...and things crawl out!");

    const bool webbed = _place_webs();
    if (!made_mons && !webbed)
    {
        mpr("...but nothing happens.");
        return false;
    }

    if (!made_mons)
        mpr("...but only cobwebs fall out.");
    return true;
}


static bool _make_zig(item_def &zig)
{
    if (feat_is_critical(env.grid(you.pos()))
        || player_in_branch(BRANCH_ARENA)
        || is_temp_terrain(you.pos()))
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

static int _gale_push_dist(const actor* agent, const actor* victim, int pow)
{
    int dist = 1 + random2(pow / 20);

    if (victim->body_size(PSIZE_BODY) < SIZE_MEDIUM)
        dist++;
    else if (victim->body_size(PSIZE_BODY) > SIZE_LARGE)
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

void wind_blast(actor* agent, int pow, coord_def target)
{
    vector<actor *> act_list;

    int radius = min(5, 4 + div_rand_round(pow, 60));

    for (actor_near_iterator ai(agent->pos(), LOS_SOLID); ai; ++ai)
    {
        if (ai->pos().distance_from(agent->pos()) > radius
            || ai->pos() == agent->pos() // so it's never aimed_at_feet
            || !target.origin()
               && _angle_between(agent->pos(), target, ai->pos()) > PI/4.0)
        {
            continue;
        }

        act_list.push_back(*ai);
    }

    far_to_near_sorter sorter = {agent->pos()};
    sort(act_list.begin(), act_list.end(), sorter);

    bolt wind_beam;
    wind_beam.hit             = AUTOMATIC_HIT;
    wind_beam.pierce          = true;
    wind_beam.affects_nothing = true;
    wind_beam.source          = agent->pos();
    wind_beam.range           = LOS_RADIUS;
    wind_beam.is_tracer       = true;

    if (agent->is_player())
    {
        // Nemelex card only.
        if (pow > 120)
            mpr("A mighty gale blasts forth from the card!");
        else
            mpr("A fierce wind blows from the card.");
    }

    noisy(8, agent->pos());

    for (actor *act : act_list)
    {
        // May have died to a previous collision.
        if (act->alive())
        {
            const int push = _gale_push_dist(agent, act, pow);
            act->knockback(*agent, push, default_collision_damage(pow, true).roll(),
                           "gust of wind");
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
}

static int _phial_power()
{
    return 10 + you.skill(SK_EVOCATIONS, 4);
}

static bool _phial_of_floods(dist *target)
{
    dist target_local;
    if (!target)
        target = &target_local;
    bolt beam;

    const int power = _phial_power();
    zappy(ZAP_PRIMAL_WAVE, power, false, beam);
    beam.range = PHIAL_RANGE;
    beam.aimed_at_spot = true;

    // TODO: this needs a custom targeter
    direction_chooser_args args;
    args.mode = TARG_HOSTILE;
    args.top_prompt = "Aim the phial where?";

    unique_ptr<targeter> hitfunc = find_spell_targeter(SPELL_PRIMAL_WAVE,
            power, beam.range);
    targeter_beam* beamfunc = dynamic_cast<targeter_beam*>(hitfunc.get());
    if (beamfunc && beamfunc->beam.hit > 0)
    {
        args.get_desc_func = bind(desc_beam_hit_chance, placeholders::_1,
            hitfunc.get());
    }

    if (spell_direction(*target, beam, &args)
        && player_tracer(ZAP_PRIMAL_WAVE, power, beam))
    {
        zappy(ZAP_PRIMAL_WAVE, power, false, beam);
        beam.fire();
        return true;
    }

    return false;
}

static spret _phantom_mirror(dist *target)
{
    bolt beam;
    monster* victim = nullptr;
    dist target_local;
    if (!target)
        target = &target_local;

    targeter_smite tgt(&you, LOS_RADIUS);

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.needs_path = false;
    args.self = confirm_prompt_type::cancel;
    args.top_prompt = "Aiming: <white>Phantom Mirror</white>";
    args.hitfunc = &tgt;
    if (!spell_direction(*target, beam, &args))
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

    monster_info mi(victim);
    monclass_flags_t mf = M_NO_FLAGS;
    if (mi.airborne())
        mf |= M_FLIES;
    if (stop_summoning_prompt(mi.mresists, mf, "use the mirror"))
        return spret::abort;

    monster* mon = clone_mons(victim, true, nullptr, ATT_FRIENDLY);
    if (!mon)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::fail;
    }
    const int power = 5 + you.skill(SK_EVOCATIONS, 3);
    int dur = min(6, max(1, (you.skill(SK_EVOCATIONS, 1) / 4 + 1)
                         * (100 - victim->check_willpower(&you, power)) / 100));

    mon->mark_summoned(SPELL_PHANTOM_MIRROR, summ_dur(dur), true, true);

    mon->summoner = MID_PLAYER;
    mons_add_blame(mon, "mirrored by the player character");
    mon->add_ench(ENCH_PHANTOM_MIRROR);
    mon->add_ench(mon_enchant(ENCH_DRAINED,
                              div_rand_round(mon->get_experience_level(), 3),
                              &you, INFINITE_DURATION));

    mon->behaviour = BEH_SEEK;
    set_nearest_monster_foe(mon);

    mprf("You reflect %s with the mirror!",
         victim->name(DESC_THE).c_str());

    return spret::success;
}

static bool _valid_tremorstone_target(const monster &m)
{
    return !m.is_firewood()
        && !never_harm_monster(&you, m);
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
        if (monster_at(*ri) && _valid_tremorstone_target(*monster_at(*ri)))
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

    // It's possible to not find any targets at radius 3 if e.g. we're in fog.
    if (!target.origin())
        return target;

    // Be very careless for this rare case.
    for (radius_iterator ri(you.pos(), 2, C_SQUARE, LOS_NO_TRANS, true); ri; ++ri)
    {
        if (ri->distance_from(you.pos()) == 2
            && !cell_is_solid(*ri)
            && one_chance_in(++ties))
        {
            target = *ri;
        }
    }

    if (!target.origin())
        return target;

    for (adjacent_iterator ai(you.pos()); ai; ++ai)
        if (!cell_is_solid(*ai) && one_chance_in(++ties))
            target = *ai;
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

static int _tremorstone_power()
{
    return 15 + you.skill(SK_EVOCATIONS);
}

/**
 * Number of explosions, scales up from 1 at 0 evo to 6 at 27 evo,
 * via a stepdown.
 *
 * Currently pow is just evo + 15, but the abstraction is kept around in
 * case an evocable enhancer returns to the game so that 0 evo with enhancer
 * gets some amount of enhancement.
 */
int tremorstone_count(int pow)
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
    bool see_target;
    const coord_def center = _find_tremorstone_target(see_target);

    targeter_radius hitfunc(&you, LOS_NO_TRANS);
    auto vulnerable = [](const actor *act) -> bool
    {
        return act && _valid_tremorstone_target(*act->as_monster());
    };
    if ((!see_target
        && !yesno("You can't see anything, release a tremorstone anyway?",
                 true, 'n'))
        || stop_attack_prompt(hitfunc, "release a tremorstone", vulnerable))
    {
        return spret::abort;
    }

    const int power = _tremorstone_power();

    bolt beam;
    beam.source_id  = MID_PLAYER;
    beam.thrower    = KILL_YOU;
    zappy(ZAP_TREMORSTONE, power, false, beam);
    beam.range = 3;
    beam.ex_size = 2;
    beam.target = center;

    mpr("The tremorstone explodes into fragments!");

    const int num_explosions = tremorstone_count(power);
    for (int i = 0; i < num_explosions; i++)
    {
        bolt explosion = beam;
        explosion.target = _fuzz_tremorstone_target(center);
        explosion.explode(i == num_explosions - 1);
    }

    return spret::success;
}

static const vector<random_pick_entry<cloud_type>> condenser_clouds =
{
  { 0,   50, 200, FALL, CLOUD_MEPHITIC },
  { 0,  100, 125, PEAK, CLOUD_FIRE },
  { 0,  100, 125, PEAK, CLOUD_COLD },
  { 0,  100, 125, PEAK, CLOUD_POISON },
  { 0,  110, 50, RISE, CLOUD_MISERY },
  { 0,  110, 50, RISE, CLOUD_STORM },
  { 0,  110, 50, RISE, CLOUD_ACID },
};

static spret _condenser()
{
    const int pow = 15 + you.skill(SK_EVOCATIONS, 7) / 2;

    random_picker<cloud_type, NUM_CLOUD_TYPES> cloud_picker;

    set<coord_def> target_cells;
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
            if (cell_is_solid(*ai) || !you.see_cell(*ai) || cloud_at(*ai))
                continue;
            const actor *act = actor_at(*ai);
            if (!act || !act->wont_attack())
                target_cells.insert(*ai);
        }
    }

    if (!see_targets
        && !yesno("You can't see anything. Try to condense clouds anyway?",
                  true, 'n'))
    {
        canned_msg(MSG_OK);
        return spret::abort;
    }

    if (target_cells.empty())
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return spret::fail;
    }

    vector<coord_def> target_list;
    for (coord_def t : target_cells)
        target_list.push_back(t);
    shuffle_array(target_list);
    bool did_something = false;

    for (auto p : target_list)
    {
        cloud_type cloud = cloud_picker.pick(condenser_clouds, pow, CLOUD_NONE);

        // Reroll misery clouds until we get something our god is okay with
        while (is_good_god(you.religion) && cloud == CLOUD_MISERY)
            cloud = cloud_picker.pick(condenser_clouds, pow, CLOUD_NONE);

        // Get at least one cloud, even at 0 power.
        if (did_something && !x_chance_in_y(50 + pow, 160))
            continue;

        const int cloud_power = 5
            + random2avg(12 + div_rand_round(pow * 3, 4), 3);
        place_cloud(cloud, p, cloud_power, &you);
        did_something = true;
    }

    if (did_something)
        mpr("Clouds condense from the air!");

    return spret::success;
}

static int _gravitambourine_power()
{
    return 15 + you.skill(SK_EVOCATIONS, 7) / 2;
}

static bool _gravitambourine(dist *target)
{
    const spret ret = your_spells(SPELL_GRAVITAS, _gravitambourine_power(),
            false, nullptr, target);

    if (ret == spret::abort)
        return false;

    return true;
}

static transformation _form_for_talisman(const item_def &talisman)
{
    if (you.using_talisman(talisman))
        return transformation::none;
    return form_for_talisman(talisman);
}

static bool _evoke_talisman(const item_def &talisman)
{
    const transformation trans = _form_for_talisman(talisman);
    if (!check_transform_into(trans, false, &talisman))
        return false;
    if (!i_feel_safe(true) && !yesno("Still begin transforming?", true, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }

    count_action(CACT_FORM, (int)trans);
    start_delay<TransformDelay>(trans, &talisman);
    if (god_despises_item(talisman, you.religion))
        excommunication();
    you.turn_is_over = true;
    return true;
}

/// Does the item only serve to produce summons or allies?
static bool _evoke_ally_only(const item_def &item, bool ident)
{
    if (item.base_type == OBJ_WANDS && ident)
        return item.sub_type == WAND_CHARMING;
    else if (item.base_type == OBJ_MISCELLANY)
    {
        switch (item.sub_type)
        {
        case MISC_PHANTOM_MIRROR:
        case MISC_HORN_OF_GERYON:
        case MISC_BOX_OF_BEASTS:
        case MISC_SACK_OF_SPIDERS:
            return true;
        default:
            return false;
        }
    }
    return false;
}

string cannot_evoke_item_reason(const item_def *item, bool temp, bool ident)
{
    // id is not at issue here
    if (temp && you.berserk())
        return "You are too berserk!";

    if (temp && you.confused())
        return "You are too confused!";

    // all generic checks passed
    if (!item)
        return "";

    // historically allowed under confusion/berserk, but why?
    if (item->is_type(OBJ_MISCELLANY, MISC_ZIGGURAT))
    {
        // override sac artifice for zigfigs, including a general check
        // TODO: zigfig has some terrain/level constraints that aren't handled
        // here
        return "";
    }

    if (item->base_type == OBJ_TALISMANS)
    {
        const transformation trans = _form_for_talisman(*item);
        const string form_unreason = cant_transform_reason(trans, false, temp);
        if (!form_unreason.empty())
            return lowercase_first(form_unreason);

        if (you.form != you.default_form)
            return "you need to leave your temporary form first.";
        return "";
    }

    if (you.get_mutation_level(MUT_NO_ARTIFICE))
        return "You cannot evoke magical items.";

    // is this really necessary?
    if (item_type_removed(item->base_type, item->sub_type))
        return "Sorry, this item was removed!";
    if (item->base_type != OBJ_WANDS && item->base_type != OBJ_MISCELLANY)
        return "You can't evoke that!";

#if TAG_MAJOR_VERSION == 34
    if (is_known_empty_wand(*item))
        return "This wand has no charges.";
#endif

    if (_evoke_ally_only(*item, ident) && you.allies_forbidden())
        return "That item cannot be used by those who cannot gain allies!";

    if (temp
        && item->base_type == OBJ_MISCELLANY
        && item->sub_type == MISC_CONDENSER_VANE
        && (env.level_state & LSTATE_STILL_WINDS))
    {
        return "The air is too still for clouds to form.";
    }

    if (temp
        && item->base_type == OBJ_MISCELLANY
        && item->sub_type == MISC_HORN_OF_GERYON
        && silenced(you.pos()))
    {
        return "You can't produce a sound!";
    }

    if (temp && is_xp_evoker(*item) && evoker_charges(item->sub_type) <= 0)
    {
        // DESC_THE prints "The tin of tremorstones (inert) is presently inert."
        return make_stringf("The %s is presently inert.",
                                            item->name(DESC_DBNAME).c_str());
    }

    return "";
}

bool item_currently_evokable(const item_def *item)
{
    const string err = cannot_evoke_item_reason(item);
    if (!err.empty())
        mpr(err);
    return err.empty();
}

bool item_ever_evokable(const item_def &item)
{
    return cannot_evoke_item_reason(&item, false).empty();
}

bool evoke_item(item_def& item, dist *preselect)
{
    if (!item_currently_evokable(&item))
        return false;

    bool did_work   = false;  // "Nothing happens" message
    bool unevokable = false;

    switch (item.base_type)
    {
    case OBJ_WANDS:
        ASSERT(in_inventory(item));
        zap_wand(item.link, preselect);
        return true;

    case OBJ_TALISMANS:
        return _evoke_talisman(item);

    case OBJ_MISCELLANY:
        ASSERT(in_inventory(item));
        did_work = true; // easier to do it this way for misc items

        switch (item.sub_type)
        {
#if TAG_MAJOR_VERSION == 34
        case MISC_BOTTLED_EFREET:
        case MISC_FAN_OF_GALES:
        case MISC_LAMP_OF_FIRE:
        case MISC_STONE_OF_TREMORS:
        case MISC_CRYSTAL_BALL_OF_ENERGY:
            canned_msg(MSG_NOTHING_HAPPENS);
            return false;
#endif

        case MISC_PHIAL_OF_FLOODS:
            if (_phial_of_floods(preselect))
            {
                expend_xp_evoker(item.sub_type);
                practise_evoking(3);
            }
            else
                return false;
            break;

        case MISC_GRAVITAMBOURINE:
            if (_gravitambourine(preselect))
            {
                expend_xp_evoker(item.sub_type);
                practise_evoking(3);
            }
            else
                return false;
            break;

        case MISC_HORN_OF_GERYON:
            if (!_evoke_horn_of_geryon())
                return false;
            expend_xp_evoker(item.sub_type);
            practise_evoking(3);
            break;

        case MISC_BOX_OF_BEASTS:
            if (!_box_of_beasts())
                return false;
            expend_xp_evoker(item.sub_type);
            if (!evoker_charges(item.sub_type))
                mpr("The box is emptied!");
            practise_evoking(1);
            break;

        case MISC_SACK_OF_SPIDERS:
            if (!_sack_of_spiders())
                return false;
            expend_xp_evoker(item.sub_type);
            if (!evoker_charges(item.sub_type))
                mpr("The sack is emptied!");
            practise_evoking(1);
            break;

        case MISC_LIGHTNING_ROD:
            if (_lightning_rod(preselect))
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
            switch (_phantom_mirror(preselect))
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

/**
 * For the clua api, returns the description displayed if targeting a monster
 * with an evokable.
 *
 * @param mi     The targeted monster.
 * @param spell  The item being evoked.
 * @return       The displayed string.
 **/
string target_evoke_desc(const monster_info& mi, const item_def& item)
{
    spell_type spell;
    int power;
    int range;
    if (item.base_type == OBJ_WANDS)
    {
        spell = spell_in_wand(static_cast<wand_type>(item.sub_type));
        power = wand_power(spell);
        range = spell_range(spell, power, false);
    }
    else if (item.base_type == OBJ_MISCELLANY
            && item.sub_type == MISC_PHIAL_OF_FLOODS)
    {
        spell = SPELL_PRIMAL_WAVE;
        range = PHIAL_RANGE;
        power = _phial_power();
    }
    else
        return "";

    unique_ptr<targeter> hitfunc = find_spell_targeter(spell, power, range);
    if (!hitfunc)
        return "";

    desc_filter addl_desc = targeter_addl_desc(spell, power,
                                get_spell_flags(spell), hitfunc.get());
    if (!addl_desc)
        return "";

    vector<string> d = addl_desc(mi);
    return comma_separated_line(d.begin(), d.end());
}

string evoke_damage_string(const item_def& item)
{
    if (item.base_type == OBJ_WANDS)
    {
        return spell_damage_string(
            spell_in_wand(static_cast<wand_type>(item.sub_type)), true);
    }
    else if (item.base_type == OBJ_MISCELLANY)
    {
        if (item.sub_type == MISC_PHIAL_OF_FLOODS)
        {
            return spell_damage_string(SPELL_PRIMAL_WAVE, true,
                _phial_power());
        }
        else if (item.sub_type == MISC_LIGHTNING_ROD)
        {
            return spell_damage_string(SPELL_THUNDERBOLT, true,
                _lightning_rod_power());
        }
        else if (item.sub_type == MISC_TIN_OF_TREMORSTONES)
        {
            return spell_damage_string(SPELL_TREMORSTONE, true,
                _tremorstone_power());
        }
        else if (item.sub_type == MISC_GRAVITAMBOURINE)
        {
            return spell_damage_string(SPELL_GRAVITAS, true,
                _gravitambourine_power());
        }
        else
            return "";
    }
    else
        return "";
}

string evoke_noise_string(const item_def& item)
{
    if (item.base_type == OBJ_WANDS)
    {
        return spell_noise_string(
            spell_in_wand(static_cast<wand_type>(item.sub_type)));
    }
    else if (item.base_type == OBJ_MISCELLANY)
    {
        if (item.sub_type == MISC_PHIAL_OF_FLOODS)
            return spell_noise_string(SPELL_PRIMAL_WAVE);
        else if (item.sub_type == MISC_LIGHTNING_ROD)
            return spell_noise_string(SPELL_THUNDERBOLT);
        else if (item.sub_type == MISC_TIN_OF_TREMORSTONES)
            return spell_noise_string(SPELL_TREMORSTONE);
        else if (item.sub_type == MISC_GRAVITAMBOURINE)
            return spell_noise_string(SPELL_GRAVITAS);
        else
            return "";
    }
    else
        return "";
}
