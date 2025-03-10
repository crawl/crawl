/*** View the world.
 * All functions in this module that take coordinates use a player centered
 * coordinate system.
 * @module view
 */
#include "AppHdr.h"

#include "l-libs.h"

#include "cloud.h"
#include "cluautil.h"
#include "coord.h"
#include "describe.h"
#include "env.h"
#include "l-defs.h"
#include "losglobal.h"
#include "map-knowledge.h"
#include "mon-death.h"
#include "player.h"
#include "religion.h"
#include "stringutil.h"
#include "terrain.h"
#include "travel.h"
#include "view.h"
#include "map-knowledge.h"
#include "coordit.h"
#include "stash.h"
#include "traps.h"

/*** What is the feature here?
 * @tparam int x
 * @tparam int y
 * @treturn string feature name
 * @function feature_at
 */
LUAFN(view_feature_at)
{
    PLAYERCOORDS(p, 1, 2)
    if (!map_bounds(p))
    {
        lua_pushstring(ls, "unseen");
        return 1;
    }
    dungeon_feature_type f = env.map_knowledge(p).feat();
    lua_pushstring(ls, dungeon_feature_name(f));
    return 1;
}

/*** What kind of cloud (if any) is here?
 * @tparam int x
 * @tparam int y
 * @treturn string|nil cloud name or nil
 * @function cloud_at
 */
LUAFN(view_cloud_at)
{
    PLAYERCOORDS(p, 1, 2)
    if (!map_bounds(p))
    {
        lua_pushnil(ls);
        return 1;
    }
    cloud_type c = env.map_knowledge(p).cloud();
    if (c == CLOUD_NONE)
    {
        lua_pushnil(ls);
        return 1;
    }
    lua_pushstring(ls, cloud_type_name(c).c_str());
    return 1;
}

/*** What kind of trap (if any) is here?
 * @tparam int x
 * @tparam int y
 * @treturn string|nil The base trap name or nil. Here the base name doesn't
 *                     include the word "trap" and is the same string passed to
 *                     the c_trap_is_safe() hook.
 * @function trap_at
 */
LUAFN(view_trap_at)
{
    PLAYERCOORDS(p, 1, 2)
    if (!map_bounds(p))
    {
        lua_pushnil(ls);
        return 1;
    }

    auto trap = trap_at(p);
    if (!trap)
    {
        lua_pushnil(ls);
        return 1;
    }

    lua_pushstring(ls, trap_name(trap->type).c_str());
    return 1;
}

/*** Is it safe here? A square is considered unsafe if it has a harmful cloud,
 * harmful trap, untraversable terrain, a runed door, or is excluded. The
 * untraversable terrain check is the same as travel.feature_is_traversable().
 * @tparam int x
 * @tparam int y
 * @tparam boolean assume_flight If true, assume the player has permanent
 *                               flight.
 * @treturn boolean
 * @function is_safe_square
 */
LUAFN(view_is_safe_square)
{
    PLAYERCOORDS(p, 1, 2)
    if (!map_bounds(p))
    {
        PLUARET(boolean, false);
        return 1;
    }
    cloud_type c = env.map_knowledge(p).cloud();
    if (c != CLOUD_NONE
        && is_damaging_cloud(c, true,
            YOU_KILL(env.map_knowledge(p).cloudinfo()->killer)))
    {
        PLUARET(boolean, false);
        return 1;
    }
    trap_type t = env.map_knowledge(p).trap();
    if (t != TRAP_UNASSIGNED)
    {
        trap_def trap;
        trap.type = t;
        trap.ammo_qty = 1;
        PLUARET(boolean, trap.is_safe());
        return 1;
    }
    dungeon_feature_type f = env.map_knowledge(p).feat();
    const bool assume_flight = lua_isboolean(ls, 1) ? lua_toboolean(ls, 1)
                                                    : false;
    if (f != DNGN_UNSEEN && !feat_is_traversable_now(f, false, assume_flight)
        || feat_is_runed(f))
    {
        PLUARET(boolean, false);
        return 1;
    }
    if (is_excluded(p))
    {
        PLUARET(boolean, false);
        return 1;
    }
    PLUARET(boolean, true);
    return 1;
}

/*** Can we get to this square?
 * @tparam int x
 * @tparam int y
 * @treturn boolean
 * @function can_reach
 */
LUAFN(view_can_reach)
{
    COORDSHOW(s, 1, 2)
    // TODO: refactor this to use can_reach_attack_between()
    const int x_distance  = abs(s.x);
    const int y_distance  = abs(s.y);
    if (x_distance > 2 || y_distance > 2)
    {
        PLUARET(boolean, false);
        return 1;
    }
    if (x_distance < 2 && y_distance < 2)
    {
        PLUARET(boolean, true);
        return 1;
    }
    const coord_def first_middle(s.x/2,s.y/2);
    const coord_def second_middle(s.x - s.x/2, s.y - s.y/2);
    if (!feat_is_reachable_past(env.grid(player2grid(first_middle)))
        && !feat_is_reachable_past(env.grid(player2grid(second_middle))))
    {
        PLUARET(boolean, false);
        return 1;
    }
    PLUARET(boolean, true);
    return 1;
}

/*** Is this square excluded from movement by mesmerise-like effects?
 * @tparam int x
 * @tparam int y
 * @treturn boolean
 * @function withheld
 */
LUAFN(view_withheld)
{
    PLAYERCOORDS(p, 1, 2)
    if (!map_bounds(p))
    {
        PLUARET(boolean, false);
        return 1;
    }
    PLUARET(boolean, env.map_knowledge(p).flags & MAP_WITHHELD);
    return 1;
}

/*** Did we detect an invisible monster on this square?
 * @tparam int x
 * @tparam int y
 * @treturn boolean
 * @function invisible_monster
 */
LUAFN(view_invisible_monster)
{
    PLAYERCOORDS(p, 1, 2)
    if (!map_bounds(p))
    {
        PLUARET(boolean, false);
        return 1;
    }
    PLUARET(boolean, env.map_knowledge(p).flags & MAP_INVISIBLE_MONSTER);
    return 1;
}

/*** Can one cell see the other?
 * @tparam int x1
 * @tparam int y1
 * @tparam int x2
 * @tparam int y2
 * @treturn boolean
 * @function cell_see_cell
 */
LUAFN(view_cell_see_cell)
{
    PLAYERCOORDS(p1, 1, 2)
    PLAYERCOORDS(p2, 3, 4)
    PLUARET(boolean, cell_see_cell(p1, p2, LOS_DEFAULT));
    return 1;
}

/*** Returns the entire known map as a table of cell objects.
 * Cells have x, y, feature; optional monster (object);
 * optional cloud (name); and optional flags [visible, mapped,
 * traversable, solid, door, undiggable, frontier, excluded, item,
 * unvisited, unsafe, invisible_monster, detected_item, detected_monster].
 * The frontier flag indicates that the cell is adjacent to an unknown cell.
 * The key for each cell is 40000*(100+x) + (100+y), which is
 * a reversible composition of the x,y coordinates (much more efficient
 * as a key for lua than other options).
 * @treturn table[cell]
 * @function get_map
 */
LUAFN(view_get_map)
{
    lua_createtable(ls, env.map_knowledge.size(), 0);
    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        const coord_def p = *ri;
        map_cell& cell = env.map_knowledge(p);
        if (!cell.known())
            continue;
        auto pc = grid2player(p);
        lua_createtable(ls, 4, 0);
        LUA_PUSHINT("x", pc.x);
        LUA_PUSHINT("y", pc.y)
        auto feat = cell.feat();
        LUA_PUSHSTRING("feature", dungeon_feature_name(feat));
        auto visible = cell.visible();
        if (visible)
            LUA_PUSHBOOL("visible", true);
        if (cell.mapped() && !cell.seen())
            LUA_PUSHBOOL("mapped", true);
        if (!feat_is_solid(feat) || feat_is_door(feat))
        {
            for (adjacent_iterator ai(p); ai; ++ai)
            {
                if (!env.map_knowledge(*ai).known())
                {
                    LUA_PUSHBOOL("frontier", true);
                    break;
                }
            }
        }
        if (feat_is_traversable_now(feat))
            LUA_PUSHBOOL("traversable", true);
        if (feat_is_solid(feat))
        {
            LUA_PUSHBOOL("solid", true);
            if (feat_is_door(feat))
                LUA_PUSHBOOL("door", true);
            if (!feat_is_diggable(feat))
                LUA_PUSHBOOL("undiggable", true);
            lua_rawseti(ls, -2, 40000*(100+pc.x) + (100+pc.y));
            continue;
        }

        if (is_excluded(p))
            LUA_PUSHBOOL("excluded", true);
        if (feat == DNGN_ENTER_SHOP)
        {
            LevelStashes *lev = StashTrack.find_current_level();
            if (lev && lev->shop_needs_visit(p))
                LUA_PUSHBOOL("unvisited", true);
        }
        if (cell.item() && cell.item()->defined())
            LUA_PUSHBOOL("item", true)
        if (cell.monsterinfo())
        {
            lua_push_moninf(ls, cell.monsterinfo());
            lua_setfield(ls, -2, "monster");
        }
        else if (cell.invisible_monster())
            LUA_PUSHBOOL("invisible_monster", true);
        bool unsafe = false;
        if (cell.cloud() != CLOUD_NONE)
        {
            LUA_PUSHSTRING("cloud", cloud_type_name(cell.cloud()).c_str());
            auto killer = cell.cloudinfo()->killer;
            if (is_damaging_cloud(cell.cloud(), true, YOU_KILL(killer)))
                unsafe = true;
        }
        if (!unsafe && cell.trap() != TRAP_UNASSIGNED)
        {
            trap_def trap;
            trap.type = cell.trap();
            trap.ammo_qty = 1;
            if (!trap.is_safe())
                unsafe = true;
        }
        if (unsafe)
            LUA_PUSHBOOL("unsafe", true);
        if (!visible)
        {
            if (cell.detected_item())
                LUA_PUSHBOOL("detected_item", true);
            if (cell.detected_monster())
                LUA_PUSHBOOL("detected_monster", true);
        }
        lua_rawseti(ls, -2, 40000*(100+pc.x) + (100+pc.y));
    }
    return 1;
}

/*** Are the given coordinates in the minimal bounding box of the known map?
 * @tparam int x
 * @tparam int y
 * @treturn boolean
 * @function in_known_map_bounds
 */
LUAFN(view_in_known_map_bounds)
{
    PLAYERCOORDS(p, 1, 2)
    PLUARET(boolean, in_known_map_bounds(p));
    return 1;
}

LUAFN(view_update_monsters)
{
    ASSERT_DLUA;

    update_monsters_in_view();
    return 0;
}

static const struct luaL_reg view_lib[] =
{
    { "feature_at", view_feature_at },
    { "cloud_at", view_cloud_at },
    { "trap_at", view_trap_at },
    { "is_safe_square", view_is_safe_square },
    { "can_reach", view_can_reach },
    { "withheld", view_withheld },
    { "invisible_monster", view_invisible_monster },
    { "cell_see_cell", view_cell_see_cell },
    { "in_known_map_bounds", view_in_known_map_bounds },
    { "get_map", view_get_map },

    { "update_monsters", view_update_monsters },

    { nullptr, nullptr }
};

void cluaopen_view(lua_State *ls)
{
    luaL_openlib(ls, "view", view_lib, 0);
}
