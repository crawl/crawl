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
#include "food.h"
#include "fprop.h"
#include "monster.h"
#include "mon-pathfind.h"
#include "mon-place.h"
#include "mon-tentacle.h"
#include "player.h"
#include "player-stats.h"
#include "stringutil.h"
#include "state.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"


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
    // At the very least, we shouldn't consider a visible monster with a
    // direct path to you "safe" just because it would be too stupid to
    // track you that far out-of-sight. Use a factor of 2 for smarter
    // creatures as a safety margin.
    if (range > 0)
        mp.set_range(max(LOS_RADIUS, range * 2));

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
        && mons_has_known_ranged_attack(*mon))
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
           || (mon->type == MONS_BALLISTOMYCETE
               && !mons_is_active_ballisto(*mon));
}

bool mons_is_safe(const monster* mon, const bool want_move,
                  const bool consider_user_options, bool check_dist)
{
    // Short-circuit plants, some vaults have tons of those. Except for both
    // active and inactive ballistos, players may still want these.
    if (mons_is_firewood(*mon) && mon->type != MONS_BALLISTOMYCETE)
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
        bool moving = you_are_delayed()
                       && current_delay()->is_run()
                       && current_delay()->is_resting()
                      || you.running < RMODE_NOT_RUNNING
                      || want_move;

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
        range = you.current_vision;

    vector<monster* > mons;

    // Sweep every visible square within range.
    for (radius_iterator ri(you.pos(), range, C_SQUARE, you.xray_vision ? LOS_NONE : LOS_DEFAULT); ri; ++ri)
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
    const int radius = want_move ? 2 : 1;
    for (radius_iterator ri(you.pos(), radius, C_SQUARE, LOS_DEFAULT); ri; ++ri)
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
        if (in_bounds(you.pos()))
        {
            const cloud_type type = cloud_type_at(you.pos());

            // Temporary immunity allows travelling through a cloud but not
            // resting in it.
            // Qazlal immunity will allow for it, however.
            if (is_damaging_cloud(type, want_move, cloud_is_yours_at(you.pos())))
            {
                if (announce)
                {
                    mprf(MSGCH_WARN, "당신은 %s의 구름에 들어와 있다!",
                         cloud_type_name(type).c_str());
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
                mprf(MSGCH_WARN, "당신의 몸은 치사량의 독에 중독되었다!");

            return false;
        }

        if (you.duration[DUR_LIQUID_FLAMES])
        {
            if (announce)
                mprf(MSGCH_WARN, "당신의 몸에 불이 붙었다!");

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
        msg = make_stringf("%s is nearby!", m.name(DESC_A).c_str());
    }
    else if (visible.size() > 1)
        msg = "주변에 몬스터가 있다!";
    else if (_exposed_monsters_nearby(want_move))
        msg = "주변에서 무언가에게 방해를 받고 있다!";
    else
        return true;

    if (announce)
    {
        mprf(MSGCH_WARN, "%s", msg.c_str());

        if (Options.use_animations & UA_MONSTER_IN_SIGHT)
        {
            static bool tried = false;

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

        const int xp = exper_value(**mi);
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

void bring_to_safety()
{
    if (player_in_branch(BRANCH_ABYSS))
        return abyss_teleport();

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
            || grd(pos) != DNGN_FLOOR
            || cloud_at(pos)
            || monster_at(pos)
            || env.pgrid(pos) & FPROP_NO_TELE_INTO
            || crawl_state.game_is_sprint()
               && grid_distance(pos, you.pos()) > 8)
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
        _monster_threat_values(&gen_threat, &hi_threat, &junk, pos);
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

#if TAG_MAJOR_VERSION == 34
    you.attribute[ATTR_DELAYED_FIREBALL] = 0;
#endif
    clear_trapping_net();
    you.attribute[ATTR_DIVINE_VIGOUR] = 0;
    you.attribute[ATTR_DIVINE_STAMINA] = 0;
    you.attribute[ATTR_DIVINE_SHIELD] = 0;
    if (you.form != transformation::none)
        untransform();
    you.clear_beholders();
    you.clear_fearmongers();
    you.attribute[ATTR_DIVINE_DEATH_CHANNEL] = 0;
    you.attribute[ATTR_INVIS_UNCANCELLABLE] = 0;
    you.attribute[ATTR_FLIGHT_UNCANCELLABLE] = 0;
    you.attribute[ATTR_XP_DRAIN] = 0;
    you.attribute[ATTR_HEAVENLY_STORM] = 0;
    you.los_noise_level = 0;
    you.los_noise_last_turn = 0; // silence in death
    if (you.duration[DUR_SCRYING])
        you.xray_vision = false;

    for (int dur = 0; dur < NUM_DURATIONS; dur++)
        if (dur != DUR_GOURMAND && dur != DUR_PIETY_POOL)
            you.duration[dur] = 0;

    update_vision_range(); // in case you had darkness cast before
    you.props["corrosion_amount"] = 0;

    unrot_hp(9999);
    set_hp(9999);
    set_mp(9999);
    you.pending_revival = false;

    // Remove silence.
    invalidate_agrid();

    if (you.hp_max <= 0)
    {
        you.lives = 0;
        mpr("당신은 살아가기엔 너무나도 허약하다.");
        // possible only with an extreme abuse of Borgnjor's
        ouch(INSTANT_DEATH, KILLED_BY_DRAINING);
    }

    mpr("당신은 이승으로 다시 돌아왔다...");
    // included in default force_more_message
}
