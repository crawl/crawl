/*
 * File:      cluautil.h
 * Summary:   Utility functions and macros for Lua bindings.
 */

#ifndef CLUAUTIL_H
#define CLUAUTIL_H

/*
 * Function definitions.
 */
#define LUAFN(name) static int name(lua_State *ls)


void luaopen_setmeta(lua_State *ls,
                     const char *global,
                     const luaL_reg *lua_lib,
                     const char *meta);

/*
 * Passing objects from and to Lua.
 */
struct lua_State;

struct activity_interrupt_data;
int push_activity_interrupt(lua_State *ls, activity_interrupt_data *t);

class map_def;
void clua_push_map(lua_State *ls, map_def *map);

void clua_push_coord(lua_State *ls, const coord_def &c);

class dgn_event;
void clua_push_dgn_event(lua_State *ls, const dgn_event *devent);

// XXX: These are currently defined outside cluautil.cc.
class monsters;
void push_monster(lua_State *ls, monsters* mons);
void lua_push_items(lua_State *ls, int link);
dungeon_feature_type check_lua_feature(lua_State *ls, int idx);
item_def *clua_check_item(lua_State *ls, int n);
unsigned int get_tile_idx(lua_State *ls, int arg);
level_id dlua_level_id(lua_State *ls, int ndx);


#define GETCOORD(c, p1, p2, boundfn)                      \
    coord_def c;                                          \
    c.x = luaL_checkint(ls, p1);                          \
    c.y = luaL_checkint(ls, p2);                          \
    if (!boundfn(c))                                        \
        luaL_error(                                             \
            ls,                                                 \
            make_stringf("Point (%d,%d) is out of bounds",      \
                         c.x, c.y).c_str());                    \
    else ;


#define COORDS(c, p1, p2)                                \
    GETCOORD(c, p1, p2, in_bounds)

#define FEAT(f, pos) \
dungeon_feature_type f = check_lua_feature(ls, pos)

#define LUA_ITEM(name, n) \
    item_def *name = clua_check_item(ls, n);

#define LEVEL(lev, br, pos)                                             \
const char *level_name = luaL_checkstring(ls, pos);                 \
level_area_type lev = str_to_level_area_type(level_name);           \
if (lev == NUM_LEVEL_AREA_TYPES)                                    \
luaL_error(ls, "Expected level name");                          \
const char *branch_name = luaL_checkstring(ls, pos);                \
branch_type br = str_to_branch(branch_name);                        \
if (lev == LEVEL_DUNGEON && br == NUM_BRANCHES)                     \
luaL_error(ls, "Expected branch name");

#define MAP(ls, n, var)                             \
map_def *var = *(map_def **) luaL_checkudata(ls, n, MAP_METATABLE)
#define DEVENT(ls, n, var) \
dgn_event *var = *(dgn_event **) luaL_checkudata(ls, n, DEVENT_METATABLE)
#define MAPMARKER(ls, n, var) \
map_marker *var = *(map_marker **) luaL_checkudata(ls, n, MAPMARK_METATABLE)

#endif
