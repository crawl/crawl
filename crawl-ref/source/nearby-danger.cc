/**
 * @file
 * @brief What's near the player?
**/

#include "AppHdr.h"

#include "nearby-danger.h"

#include <cfloat>
#include <cmath>

#include "abyss.h"
#include "act-iter.h"
#include "areas.h"
#include "cloud.h"
#include "clua.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "env.h"
#include "fprop.h"
#include "god-abil.h"
#include "god-passive.h"
#include "monster.h"
#include "mon-movetarget.h"
#include "mon-pathfind.h"
#include "mon-tentacle.h"
#include "player.h"
#include "player-stats.h"
#include "spl-damage.h"
#include "stringutil.h"
#include "state.h"
#include "terrain.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "zot.h" // decr_zot_clock

// Checks if this is a monster that should be completely ignored for autoexplore
// or tension purposes. This errs on the side of false positives, since there
// are many situations where a monster is *realistically* harmless, but cannot
// be categorically ruled out.
bool mons_is_irrelevant(const monster* mon)
{
    // If we can see a monster with no walls in the way, there's always a chance
    // they could be relevant. (Even melee-only monsters on the other side of
    // water can cause issues with autotravel if the player can cross water
    // themselves).
    if (you.see_cell_no_trans(mon->pos()))
        return false;

    // Otherwise, test if the player has enough map knowledge to be sure that
    // the monster cannot path to them somehow.

    // If the monster is awake and knows a path towards the player
    // (even though the player cannot know this) treat it as unsafe.
    if (mon->travel_target == MTRAV_FOE)
        return false;

    // use MTRAV_KNOWN_UNREACHABLE as a cache, but only for monsters
    // that aren't visible. This forces a pathfinding check whenever previously
    // known unreachable monsters come into (or are in) view. It may be a bit
    // inefficient for certain vaults, but it is necessary to check for things
    // like sensed monsters via ash (which will get set as known unreachable
    // on detection).
    if (mon->travel_target == MTRAV_KNOWN_UNREACHABLE
                                        && !you.see_cell(mon->pos()))
    {
        return false;
    }

    // Try to find a path from monster to player, using the map as it's
    // known to the player and assuming unknown terrain to be traversable.
    monster_pathfind mp;
    const int range = mons_tracking_range(mon);
    // At the very least, we shouldn't consider a visible monster with a
    // direct path to you "safe" just because it would be too stupid to
    // track you that far out-of-sight. Use a factor of 2 for smarter
    // creatures as a safety margin.
    mp.set_range(max(LOS_RADIUS, range * 2));

    if (mp.init_pathfind(mon, you.pos(), true, false, true))
        return false;

    // Now we know the monster cannot possibly reach the player.
    mon->travel_target = MTRAV_KNOWN_UNREACHABLE;
    return true;
}

// Returns true if a monster can be considered safe regardless
// of distance.
bool mons_is_always_safe(const monster *mon)
{
    return (mon->wont_attack() && (!mons_blows_up(*mon) || mon->type == MONS_SHADOW_PRISM))
          || mon->type == MONS_BUTTERFLY
          || (mon->type == MONS_BALLISTOMYCETE
              && !mons_is_active_ballisto(*mon))
          || mon->type == MONS_TRAINING_DUMMY && !mon->weapon();;
}

// HACK ALERT: In the following several functions, want_move is true if the
// player is travelling. If that is the case, things must be considered one
// square closer to the player, since we don't yet know where the player will
// be next turn.
bool mons_is_safe(const monster* mon, const bool want_move,
                  const bool consider_user_options, bool check_dist)
{
    // Short-circuit plants, some vaults have tons of those.
    if (mon->is_firewood())
        return true;

    int  dist    = grid_distance(you.pos(), mon->pos());

    bool is_safe = (mons_is_always_safe(mon)
                    || check_dist
                       && (mon->pacified() && dist > 1
                           || crawl_state.disables[DIS_MON_SIGHT] && dist > 2
                           // Only seen through glass walls?
                           || mons_is_irrelevant(mon)));

    // If is_safe is true, ch_mon_is_safe will always immediately return true
    // anyway, so let's skip constructing another monster_info and handling lua
    // dispatch entirely in that case.
    if (consider_user_options && !is_safe)
    {
        bool moving = you_are_delayed()
                       && current_delay()->is_run()
                       && current_delay()->is_resting()
                      || you.running < RMODE_NOT_RUNNING
                      || want_move;

        bool result = is_safe;

        if (clua.callfn("ch_mon_is_safe", "sbbd>b",
                        mon->name(DESC_PLAIN).c_str(), is_safe, moving, dist,
                        &result))
        {
            is_safe = result;
        }
    }

    return is_safe;
}

static string _seen_monsters_announcement(const vector<monster*> &visible,
                                          bool sensed_monster)
{
    // Announce the presence of monsters (Eidolos).
    if (visible.size() == 1)
    {
        const monster& m = *visible[0];
        return make_stringf("%s is nearby", m.name(DESC_A).c_str());
    }
    if (visible.size() > 1)
        return "there are monsters nearby";
    if (sensed_monster)
        return "there is a strange disturbance nearby";
    return "";
}

static void _announce_monsters(string announcement, vector<monster*> &visible)
{
    mprf(MSGCH_WARN, "%s!", announcement.c_str());

    if (Options.use_animations & UA_MONSTER_IN_SIGHT)
    {
        static bool tried = false; // !!??!!

        if (visible.size() && tried)
        {
            monster_view_annotator flasher(&visible);
            delay(100);
        }
        else if (visible.size())
            tried = true;
        else
            tried = false;
    }
}

// Return all nearby monsters in range (default: LOS) that the player
// is able to recognise as being monsters.
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
        range = you.current_vision;

    vector<monster* > mons;

    // Sweep every visible square within range.
    for (vision_iterator ri(you); ri; ++ri)
    {
        if (ri->distance_from(you.pos()) > range)
            continue;

        if (monster* mon = monster_at(*ri))
        {
            if (mon->alive()
                && (!require_visible || mon->visible_to(&you))
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

#define UNSAFE_MSG(msg) {                           \
    if (announce || reason)                         \
    {                                               \
        if (announce)                               \
            mprf(MSGCH_WARN, "%s!", msg);           \
        if (reason)                                 \
            *reason = msg;                          \
        return false;                               \
    }}

bool i_feel_safe(bool announce, bool want_move, bool just_monsters,
                 bool check_dist, int range, string* reason)
{
    if (!just_monsters)
    {
        // check clouds
        if (in_bounds(you.pos()))
        {
            const cloud_type type = cloud_type_at(you.pos());

            // Temporary immunity allows travelling through a cloud but not
            // resting in it.
            // Qazlal immunity will allow for it, however.
            bool your_fault = cloud_is_yours_at(you.pos());
            if (cloud_damages_over_time(type, want_move, your_fault))
                UNSAFE_MSG(make_stringf("you are in a cloud of %s", cloud_type_name(type).c_str()).c_str());
        }

        if (poison_is_lethal())
            UNSAFE_MSG("there is a lethal amount of poison in your body");

        if (contam_max_damage() >= you.hp)
            UNSAFE_MSG("you are contaminated with a potentially lethal amount of magic");

        if (you.duration[DUR_STICKY_FLAME])
            UNSAFE_MSG("you are on fire");

        if (you.props[EMERGENCY_FLIGHT_KEY])
            UNSAFE_MSG("you are being drained by your emergency flight");

        // No monster will attack you inside a sanctuary,
        // so presence of monsters won't matter -- until it starts shrinking...
        if (is_sanctuary(you.pos()) && env.sanctuary_time >= 5)
            return true;
    }

    // Monster check.
    vector<monster* > monsters =
        get_nearby_monsters(want_move, !announce, true, true, false,
                            check_dist, range);

    vector<monster* > visible;
    copy_if(monsters.begin(), monsters.end(), back_inserter(visible),
            [](const monster *mon){ return mon->visible_to(&you); });
    const bool sensed = any_of(monsters.begin(), monsters.end(),
                   [](const monster *mon){
                       return env.map_knowledge(mon->pos()).flags
                              & MAP_INVISIBLE_MONSTER;
                   });

    const string announcement = _seen_monsters_announcement(visible, sensed);
    if (announcement.empty())
        return true;

    if (announce)
        _announce_monsters(announcement, visible);
    if (reason)
        *reason = announcement;

    return false;
}
#undef UNSAFE_MSG

bool can_rest_here(bool announce)
{
    // XXX: consider doing a check for whether your regen is *ever* inhibited
    // before iterating over each monster.
    vector<monster*> visible;
    bool sensed = false;
    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!regeneration_is_inhibited(*mi))
            continue;
        if (mi->visible_to(&you))
            visible.push_back(*mi);
        else if (env.map_knowledge(mi->pos()).flags & MAP_INVISIBLE_MONSTER)
            sensed = true;
    }

    const string announcement = _seen_monsters_announcement(visible, sensed);
    if (announcement.empty())
        return true;

    if (announce)
        _announce_monsters(announcement, visible);
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
static void _monster_threat_values(double *general, double *highest,
                                   bool *invis, coord_def pos = you.pos())
{
    *invis = false;

    double sum = 0;
    int highest_xp = -1;

    for (monster_near_iterator mi(pos); mi; ++mi)
    {
        if (mi->friendly())
            continue;

        const int xp = exp_value(**mi);
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
    if (invis == nullptr)
        invis = &junk;

    const double logexp = log((double)you.experience);
    double gen_threat = 0.0, hi_threat = 0.0;
    _monster_threat_values(&gen_threat, &hi_threat, invis);

    return gen_threat > logexp * 1.3 || hi_threat > logexp / 2;
}

/// Returns true iff the player moved, or something moved.
bool bring_to_safety()
{
    if (player_in_branch(BRANCH_ABYSS))
    {
        abyss_teleport();
        return true;
    }

    coord_def best_pos, pos;
    double min_threat = DBL_MAX;
    int tries = 0;

    // Up to 100 valid spots, but don't lock up when there's none. This can happen
    // on tiny Zig/portal rooms with a bad summon storm and you in cloud / over water.
    while (tries < 100000 && min_threat > 0)
    {
        pos.x = random2(GXM);
        pos.y = random2(GYM);
        if (!in_bounds(pos)
            || env.grid(pos) != DNGN_FLOOR
            || cloud_at(pos)
            || monster_at(pos)
            || env.pgrid(pos) & FPROP_NO_TELE_INTO
            || crawl_state.game_is_sprint()
               && grid_distance(pos, you.pos()) > 8)
        {
            tries++;
            continue;
        }

        bool junk;
        double gen_threat = 0.0, hi_threat = 0.0;
        _monster_threat_values(&gen_threat, &hi_threat, &junk, pos);
        const double threat = gen_threat * hi_threat;

        if (threat < min_threat)
        {
            best_pos = pos;
            min_threat = threat;
        }
        tries += 1000;
    }

    if (min_threat == DBL_MAX)
        return false;

    you.move_to(best_pos, MV_INTERNAL);
    return true;
}

// This includes ALL afflictions, unlike wizard/Xom revive.
void revive()
{
    // Allow a spare after a few levels; the exact value doesn't matter here.
    you.attribute[ATTR_LIFE_GAINED] = 0;

    you.magic_contamination = 0;

    you.stop_being_caught(true);
    you.stop_being_constricted();
    you.attribute[ATTR_DIVINE_VIGOUR] = 0;
    you.attribute[ATTR_DIVINE_STAMINA] = 0;
    if (you.form != you.default_form)
        return_to_default_form();
    you.clear_beholders();
    you.clear_fearmongers();
    you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = 0;
    you.attribute[ATTR_SERPENTS_LASH] = 0;
    decr_zot_clock(true);
    you.los_noise_level = 0;
    you.los_noise_last_turn = 0; // silence in death

    stop_channelling_spells();

    if (you.duration[DUR_FROZEN_RAMPARTS])
        end_frozen_ramparts();

    if (you.duration[DUR_HEAVENLY_STORM])
        wu_jian_end_heavenly_storm();

    if (you.duration[DUR_FATHOMLESS_SHACKLES])
        yred_end_blasphemy();

    if (you.duration[DUR_BLOOD_FOR_BLOOD])
        beogh_end_blood_for_blood();

    // TODO: this doesn't seem to call any duration end effects?
    for (int dur = 0; dur < NUM_DURATIONS; dur++)
    {
        if (dur != DUR_PIETY_POOL
            && dur != DUR_TRANSFORMATION
            && dur != DUR_BEOGH_SEEKING_VENGEANCE
            && dur != DUR_BEOGH_DIVINE_CHALLENGE
            && dur != DUR_GRAVE_CLAW_RECHARGE)
        {
            you.duration[dur] = 0;
        }

        if (dur == DUR_TELEPORT)
            you.props.erase(TELEPORTITIS_SOURCE);
    }

    update_vision_range(); // in case you had darkness cast before
    you.props[CORROSION_KEY] = 0;

    undrain_hp(9999);
    set_hp(9999);
    set_mp(9999);
    you.pending_revival = false;

    // Remove silence.
    invalidate_agrid();

    if (you.hp_max <= 0)
    {
        you.lives = 0;
        mpr("You are too frail to live.");
        // possible only with an extreme abuse of Borgnjor's
        // might be impossible now that felids don't level down on death?
        player_die(KILLED_BY_DRAINING);
    }

    mpr("You rejoin the land of the living...");
    // included in default force_more_message
}
