/**
 * @file
 * @brief Misc functions.
**/

#include "AppHdr.h"

#include "misc.h"

#include <string.h>
#include <algorithm>

#if !defined(__IBMCPP__) && !defined(TARGET_COMPILER_VC)
#include <unistd.h>
#endif

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cfloat>

#include "externs.h"
#include "misc.h"

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "artefact.h"
#include "clua.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "dgn-shoals.h"
#include "dgnevent.h"
#include "env.h"
#include "feature.h"
#include "fight.h"
#include "files.h"
#include "fprop.h"
#include "food.h"
#include "ghost.h"
#include "godabil.h"
#include "godpassive.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "losglobal.h"
#include "makeitem.h"
#include "mapmark.h"
#include "message.h"
#include "mgen_data.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-pathfind.h"
#include "mon-info.h"
#include "ng-setup.h"
#include "notes.h"
#include "ouch.h"
#include "player.h"
#include "player-stats.h"
#include "prompt.h"
#include "random.h"
#include "religion.h"
#include "rot.h"
#include "godconduct.h"
#include "shopping.h"
#include "skills.h"
#include "skills2.h"
#include "spl-clouds.h"
#include "state.h"
#include "stringutil.h"
#include "target.h"
#include "terrain.h"
#include "tileview.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "hints.h"
#include "view.h"
#include "shout.h"
#include "xom.h"

string get_desc_quantity(const int quant, const int total, string whose)
{
    if (total == quant)
        return uppercase_first(whose);
    else if (quant == 1)
        return "One of " + whose;
    else if (quant == 2)
        return "Two of " + whose;
    else if (quant >= total * 3 / 4)
        return "Most of " + whose;
    else
        return "Some of " + whose;
}

// Update the trackers after the player changed level.
void trackers_init_new_level(bool transit)
{
    travel_init_new_level();
}

string weird_glowing_colour()
{
    return getMiscString("glowing_colour_name");
}

string weird_writing()
{
    return getMiscString("writing_name");
}

string weird_smell()
{
    return getMiscString("smell_name");
}

string weird_sound()
{
    return getMiscString("sound_name");
}

// HACK ALERT: In the following several functions, want_move is true if the
// player is travelling. If that is the case, things must be considered one
// square closer to the player, since we don't yet know where the player will
// be next turn.

// Returns true if the monster has a path to the player, or it has to be
// assumed that this is the case.
static bool _mons_has_path_to_player(const monster* mon, bool want_move = false)
{
    if (mon->is_stationary() && !mons_is_tentacle(mon->type))
    {
        int dist = grid_distance(you.pos(), mon->pos());
        if (want_move)
            dist--;
        if (dist >= 2)
            return false;
    }

    // Short-cut if it's already adjacent.
    if (grid_distance(mon->pos(), you.pos()) <= 1)
        return true;

    // If the monster is awake and knows a path towards the player
    // (even though the player cannot know this) treat it as unsafe.
    if (mon->travel_target == MTRAV_FOE)
        return true;

    if (mon->travel_target == MTRAV_KNOWN_UNREACHABLE)
        return false;

    // Try to find a path from monster to player, using the map as it's
    // known to the player and assuming unknown terrain to be traversable.
    monster_pathfind mp;
    const int range = mons_tracking_range(mon);
    // Use a large safety margin.  x4 should be ok.
    if (range > 0)
        mp.set_range(range * 4);

    if (mp.init_pathfind(mon, you.pos(), true, false, true))
        return true;

    // Now we know the monster cannot possibly reach the player.
    mon->travel_target = MTRAV_KNOWN_UNREACHABLE;

    return false;
}

bool mons_can_hurt_player(const monster* mon, const bool want_move)
{
    // FIXME: This takes into account whether the player knows the map!
    //        It should, for the purposes of i_feel_safe. [rob]
    // It also always returns true for sleeping monsters, but that's okay
    // for its current purposes. (Travel interruptions and tension.)
    if (_mons_has_path_to_player(mon, want_move))
        return true;

    // Even if the monster can not actually reach the player it might
    // still use some ranged form of attack.
    if (you.see_cell_no_trans(mon->pos())
        && mons_has_known_ranged_attack(mon))
    {
        return true;
    }

    return false;
}

// Returns true if a monster can be considered safe regardless
// of distance.
static bool _mons_is_always_safe(const monster *mon)
{
    return mon->wont_attack()
           || mon->type == MONS_BUTTERFLY
           || mon->withdrawn()
           || mon->type == MONS_BALLISTOMYCETE && mon->number == 0;
}

bool mons_is_safe(const monster* mon, const bool want_move,
                  const bool consider_user_options, bool check_dist)
{
    // Short-circuit plants, some vaults have tons of those.
    // Except for inactive ballistos, players may still want these.
    if (mons_is_firewood(mon) && mon->type != MONS_BALLISTOMYCETE)
        return true;

    int  dist    = grid_distance(you.pos(), mon->pos());

    bool is_safe = (_mons_is_always_safe(mon)
                    || check_dist
                       && (mon->pacified() && dist > 1
                           || crawl_state.disables[DIS_MON_SIGHT] && dist > 2
                           // Only seen through glass walls or within water?
                           // Assuming that there are no water-only/lava-only
                           // monsters capable of throwing or zapping wands.
                           || !mons_can_hurt_player(mon, want_move)));

#ifdef CLUA_BINDINGS
    if (consider_user_options)
    {
        bool moving = (!you.delay_queue.empty()
                          && delay_is_run(you.delay_queue.front().type)
                          && you.delay_queue.front().type != DELAY_REST
                       || you.running < RMODE_NOT_RUNNING
                       || want_move);

        bool result = is_safe;

        monster_info mi(mon, MILEV_SKIP_SAFE);
        if (clua.callfn("ch_mon_is_safe", "Ibbd>b",
                        &mi, is_safe, moving, dist,
                        &result))
        {
            is_safe = result;
        }
    }
#endif

    return is_safe;
}

// Return all nearby monsters in range (default: LOS) that the player
// is able to recognise as being monsters (i.e. no submerged creatures.)
//
// want_move       (??) Somehow affects what monsters are considered dangerous
// just_check      Return zero or one monsters only
// dangerous_only  Return only "dangerous" monsters
// require_visible Require that monsters be visible to the player
// range           search radius (defaults: LOS)
//
vector<monster* > get_nearby_monsters(bool want_move,
                                      bool just_check,
                                      bool dangerous_only,
                                      bool consider_user_options,
                                      bool require_visible,
                                      bool check_dist,
                                      int range)
{
    ASSERT(!crawl_state.game_is_arena());

    if (range == -1)
        range = LOS_RADIUS;

    vector<monster* > mons;

    // Sweep every visible square within range.
    for (radius_iterator ri(you.pos(), range, C_ROUND, LOS_DEFAULT); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            if (mon->alive()
                && (!require_visible || mon->visible_to(&you))
                && !mon->submerged()
                && (!dangerous_only || !mons_is_safe(mon, want_move,
                                                     consider_user_options,
                                                     check_dist)))
            {
                mons.push_back(mon);
                if (just_check) // stop once you find one
                    break;
            }
        }
    }
    return mons;
}

static bool _exposed_monsters_nearby(bool want_move)
{
    const int radius = want_move ? 8 : 2;
    for (radius_iterator ri(you.pos(), radius, C_CIRCLE, LOS_DEFAULT); ri; ++ri)
        if (env.map_knowledge(*ri).flags & MAP_INVISIBLE_MONSTER)
            return true;
    return false;
}

bool i_feel_safe(bool announce, bool want_move, bool just_monsters,
                 bool check_dist, int range)
{
    if (!just_monsters)
    {
        // check clouds
        if (in_bounds(you.pos()) && env.cgrid(you.pos()) != EMPTY_CLOUD)
        {
            const int cloudidx = env.cgrid(you.pos());
            const cloud_type type = env.cloud[cloudidx].type;

            // Temporary immunity allows travelling through a cloud but not
            // resting in it.
            // Qazlal immunity will allow for it, however.
            if (is_damaging_cloud(type, want_move)
                && (env.cloud[cloudidx].whose != KC_YOU
                    || !you_worship(GOD_QAZLAL)
                    || player_under_penance()))
            {
                if (announce)
                {
                    mprf(MSGCH_WARN, "You're standing in a cloud of %s!",
                         cloud_name_at_index(cloudidx).c_str());
                }
                return false;
            }
        }

        // No monster will attack you inside a sanctuary,
        // so presence of monsters won't matter -- until it starts shrinking...
        if (is_sanctuary(you.pos()) && env.sanctuary_time >= 5)
            return true;

        if (poison_is_lethal())
        {
            if (announce)
                mprf(MSGCH_WARN, "There is a lethal amount of poison in your body!");

            return false;
        }
    }

    // Monster check.
    vector<monster* > visible =
        get_nearby_monsters(want_move, !announce, true, true, true,
                            check_dist, range);

    // Announce the presence of monsters (Eidolos).
    string msg;
    if (visible.size() == 1)
    {
        const monster& m = *visible[0];
        const string monname = mons_is_mimic(m.type) ? "A mimic"
                                                     : m.name(DESC_A);
        msg = make_stringf("%s is nearby!", monname.c_str());
    }
    else if (visible.size() > 1)
        msg = "There are monsters nearby!";
    else if (_exposed_monsters_nearby(want_move))
        msg = "There is a strange disturbance nearby!";
    else
        return true;

    if (announce)
        mprf(MSGCH_WARN, "%s", msg.c_str());

    return false;
}

bool there_are_monsters_nearby(bool dangerous_only, bool require_visible,
                               bool consider_user_options)
{
    return !get_nearby_monsters(false, true, dangerous_only,
                                consider_user_options,
                                require_visible).empty();
}

// General threat = sum_of_logexpervalues_of_nearby_unfriendly_monsters.
// Highest threat = highest_logexpervalue_of_nearby_unfriendly_monsters.
static void monster_threat_values(double *general, double *highest,
                                  bool *invis)
{
    *invis = false;

    double sum = 0;
    int highest_xp = -1;

    for (monster_near_iterator mi(you.pos()); mi; ++mi)
    {
        if (mi->friendly())
            continue;

        const int xp = exper_value(*mi);
        const double log_xp = log((double)xp);
        sum += log_xp;
        if (xp > highest_xp)
        {
            highest_xp = xp;
            *highest   = log_xp;
        }
        if (!mi->visible_to(&you))
            *invis = true;
    }

    *general = sum;
}

bool player_in_a_dangerous_place(bool *invis)
{
    bool junk;
    if (invis == NULL)
        invis = &junk;

    const double logexp = log((double)you.experience);
    double gen_threat = 0.0, hi_threat = 0.0;
    monster_threat_values(&gen_threat, &hi_threat, invis);

    return gen_threat > logexp * 1.3 || hi_threat > logexp / 2;
}

static void _drop_tomb(const coord_def& pos, bool premature, bool zin)
{
    int count = 0;
    monster* mon = monster_at(pos);

    // Don't wander on duty!
    if (mon)
        mon->behaviour = BEH_SEEK;

    bool seen_change = false;
    for (adjacent_iterator ai(pos); ai; ++ai)
    {
        // "Normal" tomb (card or monster spell)
        if (!zin && revert_terrain_change(*ai, TERRAIN_CHANGE_TOMB))
        {
            count++;
            if (you.see_cell(*ai))
                seen_change = true;
        }
        // Zin's Imprison.
        else if (zin && revert_terrain_change(*ai, TERRAIN_CHANGE_IMPRISON))
        {
            vector<map_marker*> markers = env.markers.get_markers_at(*ai);
            for (int i = 0, size = markers.size(); i < size; ++i)
            {
                map_marker *mark = markers[i];
                if (mark->property("feature_description")
                    == "a gleaming silver wall")
                {
                    env.markers.remove(mark);
                }
            }

            env.grid_colours(*ai) = 0;
            tile_clear_flavour(*ai);
            tile_init_flavour(*ai);
            count++;
            if (you.see_cell(*ai))
                seen_change = true;
        }
    }

    if (count)
    {
        if (seen_change && !zin)
            mprf("The walls disappear%s!", premature ? " prematurely" : "");
        else if (seen_change && zin)
        {
            mprf("Zin %s %s %s.",
                 (mon) ? "releases"
                       : "dismisses",
                 (mon) ? mon->name(DESC_THE).c_str()
                       : "the silver walls,",
                 (mon) ? make_stringf("from %s prison",
                             mon->pronoun(PRONOUN_POSSESSIVE).c_str()).c_str()
                       : "but there is nothing inside them");
        }
        else
        {
            if (!silenced(you.pos()))
                mprf(MSGCH_SOUND, "You hear a deep rumble.");
            else
                mpr("You feel the ground shudder.");
        }
    }
}

static vector<map_malign_gateway_marker*> _get_malign_gateways()
{
    vector<map_malign_gateway_marker*> mm_markers;

    vector<map_marker*> markers = env.markers.get_all(MAT_MALIGN);
    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_marker *mark = markers[i];
        if (mark->get_type() != MAT_MALIGN)
            continue;

        map_malign_gateway_marker *mmark = dynamic_cast<map_malign_gateway_marker*>(mark);

        mm_markers.push_back(mmark);
    }

    return mm_markers;
}

int count_malign_gateways()
{
    return _get_malign_gateways().size();
}

void timeout_malign_gateways(int duration)
{
    // Passing 0 should allow us to just touch the gateway and see
    // if it should decay. This, in theory, should resolve the one
    // turn delay between it timing out and being recastable. -due
    vector<map_malign_gateway_marker*> markers = _get_malign_gateways();

    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_malign_gateway_marker *mmark = markers[i];

        if (duration)
            mmark->duration -= duration;

        if (mmark->duration > 0)
            big_cloud(CLOUD_TLOC_ENERGY, 0, mmark->pos, 3+random2(10), 2+random2(5));
        else
        {
            monster* mons = monster_at(mmark->pos);
            if (mmark->monster_summoned && !mons)
            {
                // The marker hangs around until later.
                if (env.grid(mmark->pos) == DNGN_MALIGN_GATEWAY)
                    env.grid(mmark->pos) = DNGN_FLOOR;

                env.markers.remove(mmark);
            }
            else if (!mmark->monster_summoned && !mons)
            {
                bool is_player = mmark->is_player;
                actor* caster = 0;
                if (is_player)
                    caster = &you;

                mgen_data mg = mgen_data(MONS_ELDRITCH_TENTACLE,
                                         mmark->behaviour,
                                         caster,
                                         0,
                                         0,
                                         mmark->pos,
                                         MHITNOT,
                                         MG_FORCE_PLACE,
                                         mmark->god);
                if (!is_player)
                    mg.non_actor_summoner = mmark->summoner_string;

                if (monster *tentacle = create_monster(mg))
                {
                    tentacle->flags |= MF_NO_REWARD;
                    tentacle->add_ench(ENCH_PORTAL_TIMER);
                    mon_enchant kduration = mon_enchant(ENCH_PORTAL_PACIFIED, 4,
                        caster, (random2avg(mmark->power, 6)-random2(4))*10);
                    tentacle->props["base_position"].get_coord()
                                        = tentacle->pos();
                    tentacle->add_ench(kduration);

                    mmark->monster_summoned = true;
                }
            }
        }
    }
}

void timeout_tombs(int duration)
{
    if (!duration)
        return;

    vector<map_marker*> markers = env.markers.get_all(MAT_TOMB);

    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_marker *mark = markers[i];
        if (mark->get_type() != MAT_TOMB)
            continue;

        map_tomb_marker *cmark = dynamic_cast<map_tomb_marker*>(mark);
        cmark->duration -= duration;

        // Empty tombs disappear early.
        monster* mon_entombed = monster_at(cmark->pos);
        bool empty_tomb = !(mon_entombed || you.pos() == cmark->pos);
        bool zin = (cmark->source == -GOD_ZIN);

        if (cmark->duration <= 0 || empty_tomb)
        {
            _drop_tomb(cmark->pos, empty_tomb, zin);

            monster* mon_src =
                !invalid_monster_index(cmark->source) ? &menv[cmark->source]
                                                      : NULL;
            // A monster's Tomb of Doroklohe spell.
            if (mon_src
                && mon_src == mon_entombed)
            {
                mon_src->lose_energy(EUT_SPELL);
            }

            env.markers.remove(cmark);
        }
    }
}

void timeout_terrain_changes(int duration, bool force)
{
    if (!duration && !force)
        return;

    int num_seen[NUM_TERRAIN_CHANGE_TYPES] = {0};

    vector<map_marker*> markers = env.markers.get_all(MAT_TERRAIN_CHANGE);

    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        map_terrain_change_marker *marker =
                dynamic_cast<map_terrain_change_marker*>(markers[i]);

        if (marker->duration != INFINITE_DURATION)
            marker->duration -= duration;

        if (marker->change_type == TERRAIN_CHANGE_DOOR_SEAL
            && !feat_is_sealed(grd(marker->pos)))
        {
            continue;
        }

        monster* mon_src = monster_by_mid(marker->mon_num);
        if (marker->duration <= 0
            || (marker->mon_num != 0
                && (!mon_src || !mon_src->alive() || mon_src->pacified())))
        {
            if (you.see_cell(marker->pos))
                num_seen[marker->change_type]++;
            revert_terrain_change(marker->pos, marker->change_type);
        }
    }

    if (num_seen[TERRAIN_CHANGE_DOOR_SEAL] > 1)
        mpr("The runic seals fade away.");
    else if (num_seen[TERRAIN_CHANGE_DOOR_SEAL] > 0)
        mpr("The runic seal fades away.");
}

void bring_to_safety()
{
    if (player_in_branch(BRANCH_ABYSS))
        return abyss_teleport();

    if (crawl_state.game_is_zotdef() && !orb_position().origin())
    {
        // In ZotDef, it's not the safety of your sorry butt that matters.
        for (distance_iterator di(env.orb_pos, true, false); di; ++di)
            if (!monster_at(*di)
                && !(env.pgrid(*di) & FPROP_NO_TELE_INTO))
            {
                you.moveto(*di);
                return;
            }
    }

    coord_def best_pos, pos;
    double min_threat = DBL_MAX;
    int tries = 0;

    // Up to 100 valid spots, but don't lock up when there's none.  This can happen
    // on tiny Zig/portal rooms with a bad summon storm and you in cloud / over water.
    while (tries < 100000 && min_threat > 0)
    {
        pos.x = random2(GXM);
        pos.y = random2(GYM);
        if (!in_bounds(pos)
            || grd(pos) != DNGN_FLOOR
            || env.cgrid(pos) != EMPTY_CLOUD
            || monster_at(pos)
            || env.pgrid(pos) & FPROP_NO_TELE_INTO
            || crawl_state.game_is_sprint()
               && distance2(pos, you.pos()) > dist_range(10))
        {
            tries++;
            continue;
        }

        for (adjacent_iterator ai(pos); ai; ++ai)
            if (grd(*ai) == DNGN_SLIMY_WALL)
            {
                tries++;
                continue;
            }

        bool junk;
        double gen_threat = 0.0, hi_threat = 0.0;
        monster_threat_values(&gen_threat, &hi_threat, &junk);
        const double threat = gen_threat * hi_threat;

        if (threat < min_threat)
        {
            best_pos = pos;
            min_threat = threat;
        }
        tries += 1000;
    }

    if (min_threat < DBL_MAX)
        you.moveto(best_pos);
}

// This includes ALL afflictions, unlike wizard/Xom revive.
void revive()
{
    adjust_level(-1);
    // Allow a spare after two levels (we just lost one); the exact value
    // doesn't matter here.
    you.attribute[ATTR_LIFE_GAINED] = 0;

    you.disease = 0;
    you.magic_contamination = 0;
    set_hunger(HUNGER_DEFAULT, true);
    restore_stat(STAT_ALL, 0, true);
    you.rotting = 0;

    you.attribute[ATTR_DELAYED_FIREBALL] = 0;
    clear_trapping_net();
    you.attribute[ATTR_DIVINE_VIGOUR] = 0;
    you.attribute[ATTR_DIVINE_STAMINA] = 0;
    you.attribute[ATTR_DIVINE_SHIELD] = 0;
    if (you.form)
        untransform();
    you.clear_beholders();
    you.clear_fearmongers();
    you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = 0;
    you.attribute[ATTR_INVIS_UNCANCELLABLE] = 0;
    you.attribute[ATTR_FLIGHT_UNCANCELLABLE] = 0;
    you.attribute[ATTR_XP_DRAIN] = 0;
    if (you.duration[DUR_SCRYING])
        you.xray_vision = false;

    for (int dur = 0; dur < NUM_DURATIONS; dur++)
        if (dur != DUR_GOURMAND && dur != DUR_PIETY_POOL)
            you.duration[dur] = 0;

    // Stat death that wasn't cleared might be:
    // * permanent (focus card): our fix is spot on
    // * long-term (mutation): we induce some penalty, ok
    // * short-term (-stat item): could be done better...
    unfocus_stats();
    you.stat_zero.init(0);

    unrot_hp(9999);
    set_hp(9999);
    set_mp(9999);
    you.dead = false;

    // Remove silence.
    invalidate_agrid();

    if (you.hp_max <= 0)
    {
        you.lives = 0;
        mpr("You are too frail to live.");
        // possible only with an extreme abuse of Borgnjor's
        ouch(INSTANT_DEATH, NON_MONSTER, KILLED_BY_DRAINING);
    }

    mpr("You rejoin the land of the living...");
    more();
}

////////////////////////////////////////////////////////////////////////////
// Living breathing dungeon stuff.
//

static vector<coord_def> sfx_seeds;

void setup_environment_effects()
{
    sfx_seeds.clear();

    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
    {
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        {
            if (!in_bounds(x, y))
                continue;

            const int grid = grd[x][y];
            if (grid == DNGN_LAVA
                    || (grid == DNGN_SHALLOW_WATER
                        && player_in_branch(BRANCH_SWAMP)))
            {
                const coord_def c(x, y);
                sfx_seeds.push_back(c);
            }
        }
    }
    dprf("%u environment effect seeds", (unsigned int)sfx_seeds.size());
}

static void apply_environment_effect(const coord_def &c)
{
    const dungeon_feature_type grid = grd(c);
    // Don't apply if if the feature doesn't want it.
    if (testbits(env.pgrid(c), FPROP_NO_CLOUD_GEN))
        return;
    if (grid == DNGN_LAVA)
        check_place_cloud(CLOUD_BLACK_SMOKE, c, random_range(4, 8), 0);
    else if (one_chance_in(3) && grid == DNGN_SHALLOW_WATER)
        check_place_cloud(CLOUD_MIST,        c, random_range(2, 5), 0);
}

static const int Base_Sfx_Chance = 5;
void run_environment_effects()
{
    if (!you.time_taken)
        return;

    dungeon_events.fire_event(DET_TURN_ELAPSED);

    // Each square in sfx_seeds has this chance of doing something special
    // per turn.
    const int sfx_chance = Base_Sfx_Chance * you.time_taken / 10;
    const int nseeds = sfx_seeds.size();

    // If there are a large number of seeds, speed things up by fudging the
    // numbers.
    if (nseeds > 50)
    {
        int nsels = div_rand_round(sfx_seeds.size() * sfx_chance, 100);
        if (one_chance_in(5))
            nsels += random2(nsels * 3);

        for (int i = 0; i < nsels; ++i)
            apply_environment_effect(sfx_seeds[ random2(nseeds) ]);
    }
    else
    {
        for (int i = 0; i < nseeds; ++i)
        {
            if (random2(100) >= sfx_chance)
                continue;

            apply_environment_effect(sfx_seeds[i]);
        }
    }

    run_corruption_effects(you.time_taken);
    shoals_apply_tides(div_rand_round(you.time_taken, BASELINE_DELAY),
                       false, true);
    timeout_tombs(you.time_taken);
    timeout_malign_gateways(you.time_taken);
    timeout_terrain_changes(you.time_taken);
    run_cloud_spreaders(you.time_taken);
}

// Converts a movement speed to a duration. i.e., answers the
// question: if the monster is so fast, how much time has it spent in
// its last movement?
//
// If speed is 10 (normal),    one movement is a duration of 10.
// If speed is 1  (very slow), each movement is a duration of 100.
// If speed is 15 (50% faster than normal), each movement is a duration of
// 6.6667.
int speed_to_duration(int speed)
{
    if (speed < 1)
        speed = 10;
    else if (speed > 100)
        speed = 100;

    return div_rand_round(100, speed);
}

bool bad_attack(const monster *mon, string& adj, string& suffix,
                bool& would_cause_penance, coord_def attack_pos,
                bool check_landing_only)
{
    ASSERT(!crawl_state.game_is_arena());
    bool bad_landing = false;

    if (!you.can_see(mon))
        return false;

    if (attack_pos == coord_def(0, 0))
        attack_pos = you.pos();

    adj.clear();
    suffix.clear();
    would_cause_penance = false;

    if (!check_landing_only
        && (is_sanctuary(mon->pos()) || is_sanctuary(attack_pos)))
    {
        suffix = ", despite your sanctuary";
    }
    else if (check_landing_only && is_sanctuary(attack_pos))
    {
        suffix = ", when you might land in your sanctuary";
        bad_landing = true;
    }
    if (check_landing_only)
        return bad_landing;

    if (you_worship(GOD_JIYVA) && mons_is_slime(mon)
        && !(mon->is_shapeshifter() && (mon->flags & MF_KNOWN_SHIFTER)))
    {
        would_cause_penance = true;
        return true;
    }

    if (mon->friendly())
    {
        if (you_worship(GOD_OKAWARU))
        {
            adj = "your ally ";

            monster_info mi(mon, MILEV_NAME);
            if (!mi.is(MB_NAME_UNQUALIFIED))
                adj += "the ";
        }
        else
            adj = "your ";

        if (god_hates_attacking_friend(you.religion, mon))
            would_cause_penance = true;

        return true;
    }

    if (find_stab_type(&you, mon) != STAB_NO_STAB
        && you_worship(GOD_SHINING_ONE)
        && !tso_unchivalric_attack_safe_monster(mon))
    {
        adj += "helpless ";
        would_cause_penance = true;
    }

    if (mon->neutral() && is_good_god(you.religion))
    {
        adj += "neutral ";
        if (you_worship(GOD_SHINING_ONE) || you_worship(GOD_ELYVILON))
            would_cause_penance = true;
    }
    else if (mon->wont_attack())
    {
        adj += "non-hostile ";
        if (you_worship(GOD_SHINING_ONE) || you_worship(GOD_ELYVILON))
            would_cause_penance = true;
    }

    return !adj.empty() || !suffix.empty();
}

bool stop_attack_prompt(const monster* mon, bool beam_attack,
                        coord_def beam_target, bool autohit_first,
                        bool *prompted, coord_def attack_pos,
                        bool check_landing_only)
{
    bool penance = false;

    if (prompted)
        *prompted = false;

    if (crawl_state.disables[DIS_CONFIRMATIONS])
        return false;

    if (you.confused() || !you.can_see(mon))
        return false;

    string adj, suffix;
    if (!bad_attack(mon, adj, suffix, penance, attack_pos, check_landing_only))
        return false;

    // Listed in the form: "your rat", "Blork the orc".
    string mon_name = mon->name(DESC_PLAIN);
    if (!mon_name.find("the ")) // no "your the royal jelly" nor "the the RJ"
        mon_name.erase(0, 4);
    if (adj.find("your"))
        adj = "the " + adj;
    mon_name = adj + mon_name;
    string verb;
    if (beam_attack)
    {
        verb = "fire ";
        if (beam_target == mon->pos())
            verb += "at ";
        else if (you.pos() < beam_target && beam_target < mon->pos()
                 || you.pos() > beam_target && beam_target > mon->pos())
        {
            if (autohit_first)
                return false;

            verb += "in " + apostrophise(mon_name) + " direction";
            mon_name = "";
        }
        else
            verb += "through ";
    }
    else
        verb = "attack ";

    snprintf(info, INFO_SIZE, "Really %s%s%s?%s",
             verb.c_str(), mon_name.c_str(), suffix.c_str(),
             penance ? " This attack would place you under penance!" : "");

    if (prompted)
        *prompted = true;

    if (yesno(info, false, 'n'))
        return false;
    else
    {
        canned_msg(MSG_OK);
        return true;
    }
}

bool stop_attack_prompt(targetter &hitfunc, const char* verb,
                        bool (*affects)(const actor *victim), bool *prompted)
{
    if (crawl_state.disables[DIS_CONFIRMATIONS])
        return false;

    if (crawl_state.which_god_acting() == GOD_XOM)
        return false;

    if (you.confused())
        return false;

    string adj, suffix;
    bool penance = false;
    counted_monster_list victims;
    for (distance_iterator di(hitfunc.origin, false, true, LOS_RADIUS); di; ++di)
    {
        if (hitfunc.is_affected(*di) <= AFF_NO)
            continue;
        const monster* mon = monster_at(*di);
        if (!mon || !you.can_see(mon))
            continue;
        if (affects && !affects(mon))
            continue;
        string adjn, suffixn;
        bool penancen = false;
        if (bad_attack(mon, adjn, suffixn, penancen))
        {
            // record the adjectives for the first listed, or
            // first that would cause penance
            if (victims.empty() || penancen && !penance)
                adj = adjn, suffix = suffixn, penance = penancen;
            victims.add(mon);
        }
    }

    if (victims.empty())
        return false;

    // Listed in the form: "your rat", "Blork the orc".
    string mon_name = victims.describe(DESC_PLAIN);
    if (!mon_name.find("the ")) // no "your the royal jelly" nor "the the RJ"
        mon_name.erase(0, 4);
    if (adj.find("your"))
        adj = "the " + adj;
    mon_name = adj + mon_name;

    snprintf(info, INFO_SIZE, "Really %s %s%s?%s",
             verb, mon_name.c_str(), suffix.c_str(),
             penance ? " This attack would place you under penance!" : "");

    if (prompted)
        *prompted = true;

    if (yesno(info, false, 'n'))
        return false;
    else
    {
        canned_msg(MSG_OK);
        return true;
    }
}

// Make the player swap positions with a given monster.
void swap_with_monster(monster* mon_to_swap)
{
    monster& mon(*mon_to_swap);
    ASSERT(mon.alive());
    const coord_def newpos = mon.pos();

    if (stasis_blocks_effect(true, "%s emits a piercing whistle.",
                             20, "%s makes your neck tingle."))
    {
        return;
    }

    // Be nice: no swapping into uninhabitable environments.
    if (!you.is_habitable(newpos) || !mon.is_habitable(you.pos()))
    {
        mpr("You spin around.");
        return;
    }

    const bool mon_caught = mon.caught();
    const bool you_caught = you.attribute[ATTR_HELD];

    // If it was submerged, it surfaces first.
    mon.del_ench(ENCH_SUBMERGED);

    mprf("You swap places with %s.", mon.name(DESC_THE).c_str());

    // Pick the monster up.
    mgrd(newpos) = NON_MONSTER;
    mon.moveto(you.pos());

    // Plunk it down.
    mgrd(mon.pos()) = mon_to_swap->mindex();

    if (you_caught)
    {
        check_net_will_hold_monster(&mon);
        if (!mon_caught)
        {
            you.attribute[ATTR_HELD] = 0;
            you.redraw_quiver = true;
            you.redraw_evasion = true;
        }
    }

    // Move you to its previous location.
    move_player_to_grid(newpos, false);

    if (mon_caught)
    {
        if (you.body_size(PSIZE_BODY) >= SIZE_GIANT)
        {
            int net = get_trapping_net(you.pos());
            if (net != NON_ITEM)
                destroy_item(net);
            mprf("The %s rips apart!", (net == NON_ITEM) ? "web" : "net");
            you.attribute[ATTR_HELD] = 0;
            you.redraw_quiver = true;
            you.redraw_evasion = true;
        }
        else
        {
            you.attribute[ATTR_HELD] = 10;
            if (get_trapping_net(you.pos()) != NON_ITEM)
                mpr("You become entangled in the net!");
            else
                mpr("You get stuck in the web!");
            you.redraw_quiver = true; // Account for being in a net.
            // Xom thinks this is hilarious if you trap yourself this way.
            if (you_caught)
                xom_is_stimulated(12);
            else
                xom_is_stimulated(200);
        }

        if (!you_caught)
            mon.del_ench(ENCH_HELD, true);
    }
}

void auto_id_inventory()
{
    for (int i = 0; i < ENDOFPACK; i++)
    {
        item_def& item = you.inv[i];
        if (item.defined())
            god_id_item(item, false);
    }
}

// Reduce damage by AC.
// In most cases, we want AC to mostly stop weak attacks completely but affect
// strong ones less, but the regular formula is too hard to apply well to cases
// when damage is spread into many small chunks.
//
// Every point of damage is processed independently.  Every point of AC has
// an independent 1/81 chance of blocking that damage.
//
// AC 20 stops 22% of damage, AC 40 -- 39%, AC 80 -- 63%.
int apply_chunked_AC(int dam, int ac)
{
    double chance = pow(80.0/81, ac);
    uint64_t cr = chance * (((uint64_t)1) << 32);

    int hurt = 0;
    for (int i = 0; i < dam; i++)
        if (random_int() < cr)
            hurt++;

    return hurt;
}

void entered_malign_portal(actor* act)
{
    if (you.can_see(act))
    {
        mprf("The portal repels %s, its terrible forces doing untold damage!",
             act->is_player() ? "you" : act->name(DESC_THE).c_str());
    }

    act->blink(false);
    if (act->is_player())
        ouch(roll_dice(2, 4), NON_MONSTER, KILLED_BY_WILD_MAGIC, "a malign gateway");
    else
        act->hurt(NULL, roll_dice(2, 4));
}

void handle_real_time(time_t t)
{
    you.real_time += min<time_t>(t - you.last_keypress_time, IDLE_TIME_CLAMP);
    you.last_keypress_time = t;
}

string part_stack_string(const int num, const int total)
{
    if (num == total)
        return "Your";

    string ret  = uppercase_first(number_in_words(num));
           ret += " of your";

    return ret;
}

unsigned int breakpoint_rank(int val, const int breakpoints[],
                             unsigned int num_breakpoints)
{
    unsigned int result = 0;
    while (result < num_breakpoints && val >= breakpoints[result])
        ++result;

    return result;
}

void counted_monster_list::add(const monster* mons)
{
    const string name = mons->name(DESC_PLAIN);
    for (counted_list::iterator i = list.begin(); i != list.end(); ++i)
    {
        if (i->first->name(DESC_PLAIN) == name)
        {
            i->second++;
            return;
        }
    }
    list.push_back(counted_monster(mons, 1));
}

int counted_monster_list::count()
{
    int nmons = 0;
    for (counted_list::const_iterator i = list.begin(); i != list.end(); ++i)
        nmons += i->second;
    return nmons;
}

string counted_monster_list::describe(description_level_type desc)
{
    string out;

    for (counted_list::const_iterator i = list.begin(); i != list.end();)
    {
        const counted_monster &cm(*i);
        if (i != list.begin())
        {
            ++i;
            out += (i == list.end() ? " and " : ", ");
        }
        else
            ++i;

        out += cm.second > 1 ? pluralise(cm.first->name(desc))
                             : cm.first->name(desc);
    }
    return out;
}
