/*** Functions related to (auto)traveling.
 * @module travel
 */
#include "AppHdr.h"

#include "l-libs.h"

#include "branch.h"
#include "cluautil.h"
#include "coord.h"
#include "env.h"
#include "player.h"
#include "terrain.h"
#include "travel.h"
#include "map-knowledge.h"

/*** Set an exclusion.
 * Uses player-centered coordinates
 * @tparam int x
 * @tparam int y
 * @tparam[opt=LOS_RADIUS] int r
 * @function set_exclude
 */
LUAFN(l_set_exclude)
{
    coord_def s;
    s.x = luaL_safe_checkint(ls, 1);
    s.y = luaL_safe_checkint(ls, 2);
    const coord_def p = player2grid(s);
    if (!in_known_map_bounds(p))
        return luaL_error(ls, "Coordinates out of bounds: (%d, %d)", s.x, s.y);
    // XXX: dedup w/_get_full_exclusion_radius()?
    int r = LOS_RADIUS;
    if (lua_gettop(ls) > 2)
        r = luaL_safe_checkint(ls, 3);
    set_exclude(p, r);
    return 0;
}

/*** Remove an exclusion.
 * Uses player-centered coordinates
 * @tparam int x
 * @tparam int y
 * @function del_exclude
 */
LUAFN(l_del_exclude)
{
    coord_def s;
    s.x = luaL_safe_checkint(ls, 1);
    s.y = luaL_safe_checkint(ls, 2);
    const coord_def p = player2grid(s);
    if (!in_known_map_bounds(p))
        return luaL_error(ls, "Coordinates out of bounds: (%d, %d)", s.x, s.y);
    del_exclude(p);
    return 0;
}

/*** Test for exclusion.
 * Uses player-centered coordinates
 * @tparam int x
 * @tparam int y
 * @treturn boolean
 * @function is_excluded
 */
LUAFN(l_is_excluded)
{
    coord_def s;
    s.x = luaL_safe_checkint(ls, 1);
    s.y = luaL_safe_checkint(ls, 2);
    const coord_def p = player2grid(s);
    if (!in_known_map_bounds(p))
        return luaL_error(ls, "Coordinates out of bounds: (%d, %d)", s.x, s.y);
    PLUARET(boolean, is_excluded(p));
    return 1;
}

/*** Can we move onto this feature? Considers player properties like
 * amphibiousness and permanent (but not temporary) flight, and also considers
 * travel_avoid_terrain.
 * @tparam string featurename
 * @tparam boolean assume_flight If true, assume the player has permanent
 *                               flight.
 * @treturn boolean
 * @function feature_traversable
 */
LUAFN(l_feature_is_traversable)
{
    const string &name = luaL_checkstring(ls, 1);
    const bool assume_flight = lua_isboolean(ls, 1) ? lua_toboolean(ls, 1)
                                                    : false;
    const dungeon_feature_type feat = dungeon_feature_by_name(name);
    PLUARET(boolean, feat_is_traversable_now(feat, false, assume_flight));
}

/*** Is this feature solid?
 * @tparam string featurename
 * @treturn boolean
 * @function feature_solid
 */
LUAFN(l_feature_is_solid)
{
    const string &name = luaL_checkstring(ls, 1);
    const dungeon_feature_type feat = dungeon_feature_by_name(name);
    PLUARET(boolean, feat_is_solid(feat));
}

/*** What's the deepest floor we've reached in this branch?
 * @tparam string branch
 * @treturn int depth
 * @function find_deepest_explored
 */
LUAFN(l_find_deepest_explored)
{
    const string &branch = luaL_checkstring(ls, 1);
    const level_id lid(branch_by_abbrevname(branch), 1);
    if (lid.branch == NUM_BRANCHES)
        luaL_error(ls, "Bad branch name: '%s'", branch.c_str());
    if (!is_known_branch_id(lid.branch))
        PLUARET(number, 0);
    PLUARET(number, find_deepest_explored(lid).depth);
}

/*** Deltas to a given waypoint.
 * @return nil if the waypoint is not on the current floor
 * @return int,int the x and y deltas to the waypoint
 * @function waypoint_delta
 */
LUAFN(l_waypoint_delta)
{
    int waynum = luaL_safe_checkint(ls, 1);
    if (waynum < 0 || waynum >= TRAVEL_WAYPOINT_COUNT)
        return luaL_error(ls, "Bad waypoint number: %d", waynum);
    const level_pos waypoint = travel_cache.get_waypoint(waynum);
    if (waypoint.id != level_id::current())
        return 0;
    coord_def delta = you.pos() - waypoint.pos;
    lua_pushnumber(ls, delta.x);
    lua_pushnumber(ls, delta.y);
    return 2;
}

/*** Set a numbered waypoint.
 * Uses player-centered coordinates
 * @tparam int waynum
 * @tparam int x
 * @tparam int y
 * @function set_waypoint
 */
LUAFN(l_set_waypoint)
{
    int waynum = luaL_safe_checkint(ls, 1);
    if (waynum < 0 || waynum >= TRAVEL_WAYPOINT_COUNT)
        return luaL_error(ls, "Bad waypoint number: %d", waynum);
    coord_def s;
    s.x = luaL_safe_checkint(ls, 2);
    s.y = luaL_safe_checkint(ls, 3);
    const coord_def p = player2grid(s);
    if (!in_known_map_bounds(p))
        return luaL_error(ls, "Coordinates out of bounds: (%d, %d)", s.x, s.y);
    travel_cache.set_waypoint(waynum, p.x, p.y);
    return 0;
}

/*** Clears the travel trail.
 * Call crawl.redraw_screen afterward to see changes.
 * @function clear_travel_trail
 */
static int l_clear_travel_trail(lua_State*)
{
    clear_travel_trail();
    return 0;
}

/*** Set travel_trail on a cell
 * Call crawl.redraw_screen afterward to see changes.
 * Uses player-centered coordinates.
 * @tparam int x
 * @tparam int y
 * @function set_travel_trail
 */
LUAFN(l_set_travel_trail)
{
    coord_def s;
    s.x = luaL_safe_checkint(ls, 1);
    s.y = luaL_safe_checkint(ls, 2);
    const coord_def p = player2grid(s);
    if (!in_known_map_bounds(p))
        return luaL_error(ls, "Coordinates out of bounds: (%d, %d)", s.x, s.y);
    env.travel_trail.push_back(p);
    return 0;
}

static const struct luaL_reg travel_lib[] =
{
    { "set_exclude", l_set_exclude },
    { "del_exclude", l_del_exclude },
    { "is_excluded", l_is_excluded },
    { "feature_traversable", l_feature_is_traversable },
    { "feature_solid", l_feature_is_solid },
    { "find_deepest_explored", l_find_deepest_explored },
    { "waypoint_delta", l_waypoint_delta },
    { "set_waypoint", l_set_waypoint },
    { "clear_travel_trail", l_clear_travel_trail },
    { "set_travel_trail", l_set_travel_trail },

    { nullptr, nullptr }
};

void cluaopen_travel(lua_State *ls)
{
    luaL_openlib(ls, "travel", travel_lib, 0);
}
