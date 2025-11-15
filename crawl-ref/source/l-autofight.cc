/*
--- Autofight function bindings

module "autofight"
*/

#include "AppHdr.h"

#include "l-libs.h"

#include "beam.h"
#include "cluautil.h"
#include "coord.h"
#include "coordit.h"
#include "env.h"
#include "los.h"
#include "losglobal.h"
#include "map-knowledge.h"
#include "monster.h"
#include "mon-info.h"


static bool is_candidate_for_attack(coord_def c, bool no_move)
{
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

    if (mi->attitude == ATT_HOSTILE
        || (mi->attitude == ATT_NEUTRAL && mi->is(MB_FRENZIED)))
    {
        return true;
    }

    return false;
}


static int score(coord_def target, coord_def aim)
{
    bolt beam;
    beam.source = you.pos();
    beam.set_is_tracer(true);
    beam.range = LOS_RADIUS;
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
        if (is_candidate_for_attack(g, true))
            num_hostiles++;
        else
        {
            map_cell& cell = env.map_knowledge(g);
            monster_info *mi = cell.monsterinfo();
            if (mi && !mi->can_shoot_through_monster)
                num_blockers++;
        }
    }
    if (!found_main_target)
        return -1000;
    else
    {
        int main_target_score = found_main_target_first ? 1000 : 0;
        return main_target_score + num_hostiles - 10 * num_blockers;
    }
}


/*** What is the best square to aim at to hit (x, y) with a ranged weapon?
 * @tparam int x coordinate of target, in player coordinates
 * @tparam int y coordinate of target, in player coordinates
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

    coord_def best_coord = target;
    int best_score = score(target, target);
    for (radius_iterator ri(you.pos(), LOS_SOLID_SEE); ri; ++ri)
    {
        int sc = score(target, *ri);
        if (sc > best_score)
        {
            best_score = sc;
            best_coord = *ri;
        }
    }

    coord_def c = grid2player(best_coord);
    lua_pushnumber(ls, c.x);
    lua_pushnumber(ls, c.y);

    return 2;
}



/*** Is there a monster worth attacking in this square?
 * @tparam int x coordinate of target, in player coordinates
 * @tparam int y coordinate of target, in player coordinates
 * @tparam boolean whether we are allowed to move
 * @treturn whether there is a monster worth attacking here
 * @function is_candidate_for_attack
 */
LUAFN(autofight_is_candidate_for_attack)
{
    coord_def a;
    a.x = luaL_safe_checkint(ls, 1);
    a.y = luaL_safe_checkint(ls, 2);
    const bool no_move = lua_toboolean(ls, 3);

    coord_def target = player2grid(a);
    bool is_candidate = is_candidate_for_attack(target, no_move);

    lua_pushboolean(ls, is_candidate);
    return 1;
}


static const struct luaL_reg autofight_lib[] =
{
    { "best_aim", autofight_best_aim },
    { "is_candidate_for_attack", autofight_is_candidate_for_attack},
    { nullptr, nullptr }
};

void cluaopen_autofight(lua_State *ls)
{
    luaL_openlib(ls, "autofight", autofight_lib, 0);
}
