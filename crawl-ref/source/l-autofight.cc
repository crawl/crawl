/*
--- Autofight function bindings

module "autofight"
*/

#include "AppHdr.h"

#include "l-libs.h"

#include "beam.h"
#include "clua.h"
#include "cluautil.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "los.h"
#include "losglobal.h"
#include "map-knowledge.h"
#include "monster.h"
#include "mon-info.h"
#include "throw.h"


static bool _is_candidate_for_autofight_attack(coord_def c, bool no_move, bool check_blocked_paths)
{
    if (!cell_see_cell(you.pos(), c, LOS_NO_TRANS))
        return false;

    map_cell& cell = env.map_knowledge(c);
    monster_info *mi = cell.monsterinfo();
    if (!mi)
        return false;

    if (mi->type == MONS_ORB_OF_DESTRUCTION || mi->type == MONS_BUTTERFLY)
        return false;

    if (mi->is(MB_FIREWOOD))
        return false;

    if (mi->is(MB_PLAYER_DAMAGE_IMMUNE) && (no_move || !mi->is(MB_WARDING)))
        return false;

    // If we were asked to check blocked paths, early out if no such ray can possibly exist.
    // (Note that the existence of a valid ray using opc_unblocked_shot does not guarantee that
    // this shot is possible in reality, but its absence does guarantee it isn't.)
    if (check_blocked_paths && !exists_ray(you.pos(), c, opc_unblocked_shot, you.current_vision))
        return false;

    if (mi->attitude == ATT_HOSTILE
        || (mi->attitude == ATT_NEUTRAL && mi->is(MB_FRENZIED)))
    {
        return true;
    }

    return false;
}

static double _angle_between(coord_def origin, coord_def p1, coord_def p2)
{
    double ang0 = atan2(p1.x - origin.x, p1.y - origin.y);
    double ang  = atan2(p2.x - origin.x, p2.y - origin.y);
    return min(fabs(ang - ang0), fabs(ang - ang0 + 2 * PI));
}

// Calculate a heuristic score for aiming at a given location with a given
// primary target. This requires an unblocked path to the target, highly prefers
// paths that hit the intended target first, and lightly prefers hitting as many
// targets as possible.
static int _shot_score(coord_def target, coord_def aim, bool pierce, bool primary_must_be_first)
{
    bolt beam;
    beam.set_agent(&you);
    beam.source = you.pos();
    beam.set_is_tracer(true);
    beam.range = you.current_vision;
    beam.target = aim;
    beam.aimed_at_spot = true;
    beam.pierce = true;
    beam.fire();

    int num_hostiles = 0;
    int num_blockers = 0;
    bool found_main_target = false;
    bool found_main_target_first = false;
    for (auto g : beam.path_taken)
    {
        if (g == target)
        {
            found_main_target = true;
            if (num_hostiles == 0 && num_blockers == 0)
                found_main_target_first = true;
        }
        if (_is_candidate_for_autofight_attack(g, true, false))
            num_hostiles++;
        else if (!pierce)
        {
            map_cell& cell = env.map_knowledge(g);
            monster_info *mi = cell.monsterinfo();
            if (mi && !mi->can_shoot_through_monster && mi->type != MONS_BUSH)
                num_blockers++;
        }
    }
    if (!found_main_target || (primary_must_be_first && !found_main_target_first))
        return -1000;
    else
    {
        // It's only important to prioritize hitting the main target first
        // with non-piercing shots.
        int main_target_score = !pierce && found_main_target_first ? 1000 : 0;
        return main_target_score + num_hostiles - 10 * num_blockers;
    }
}

coord_def best_ranged_aim(const coord_def& target, bool pierce, bool primary_must_be_first)
{
    coord_def best_coord = target;
    int best_score = _shot_score(target, target, pierce, primary_must_be_first);
    for (radius_iterator ri(you.pos(), you.current_vision, C_SQUARE, LOS_SOLID_SEE, true); ri; ++ri)
    {
        // Quickly exclude aim spots that could not possibly include the main target.
        if (_angle_between(you.pos(), target, *ri) > PI / 6)
            continue;

        int sc = _shot_score(target, *ri, pierce, primary_must_be_first);
        if (sc > best_score)
        {
            best_score = sc;
            best_coord = *ri;
        }
    }

    return best_score > 0 ? best_coord : coord_def();
}

/*** What is the best square to aim at to hit (x, y) with a ranged weapon?
 * @tparam int x coordinate of target, in player coordinates
 * @tparam int y coordinate of target, in player coordinates
 * @tparam boolean true if this is a piercing attack
 * @treturn (x, y) coordinates to aim at.
 * the best square to aim at.
 * @function best_aim
 */
LUAFN(autofight_best_aim)
{
    coord_def a;
    a.x = luaL_safe_checkint(ls, 1);
    a.y = luaL_safe_checkint(ls, 2);
    coord_def target = player2grid(a);
    const bool pierce = lua_toboolean(ls, 3);

    coord_def c = best_ranged_aim(target, pierce);
    if (!c.origin())
        c = grid2player(c);

    lua_pushnumber(ls, c.x);
    lua_pushnumber(ls, c.y);

    return 2;
}

/*** Is there a monster worth attacking in this square?
 * @tparam int x coordinate of target, in player coordinates
 * @tparam int y coordinate of target, in player coordinates
 * @tparam boolean whether we are allowed to move
 * @tparam boolean whether to check and exclude shot paths blocked by monsters
 * @treturn whether there is a monster worth attacking here
 * @function is_candidate_for_attack
 */
LUAFN(autofight_is_candidate_for_attack)
{
    coord_def a;
    a.x = luaL_safe_checkint(ls, 1);
    a.y = luaL_safe_checkint(ls, 2);
    const bool no_move = lua_toboolean(ls, 3);
    const bool check_path = lua_toboolean(ls, 4);

    coord_def target = player2grid(a);
    bool is_candidate = _is_candidate_for_autofight_attack(target, no_move, check_path);

    lua_pushboolean(ls, is_candidate);
    return 1;
}

/*** Is the weapon/throwable the player is using penetrating?
 * @tparam boolean Whether we're firing the quivered action (as opposed to our weapon)
 */
LUAFN(autofight_is_piercing)
{
    const bool use_quiver = lua_toboolean(ls, 1);
    const bool pierce = use_quiver ? you.quiver_action.get()->is_piercing()
                                   : you.weapon() && is_penetrating_attack(*you.weapon());

    lua_pushboolean(ls, pierce);
    return 1;
}

static const struct luaL_Reg autofight_lib[] =
{
    { "best_aim", autofight_best_aim },
    { "is_candidate_for_attack", autofight_is_candidate_for_attack},
    { "is_piercing", autofight_is_piercing},
    { nullptr, nullptr }
};

void cluaopen_autofight(lua_State *ls)
{
    if (lua_getglobal(ls, "autofight") == LUA_TNIL) {
        lua_pop(ls, 1);
        lua_newtable(ls);
    }
    luaL_setfuncs(ls, autofight_lib, 0);
    lua_setglobal(ls, "autofight");
}
