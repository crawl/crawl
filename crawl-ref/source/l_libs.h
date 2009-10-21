/*
 * File:     l_libs.h
 * Summary:  Library definitions for dlua.
 */

#ifndef L_LIBS_H
#define L_LIBS_H

#include "clua.h"

/*
 * Library loaders for clua.
 */

void cluaopen_crawl(lua_State *ls);
void cluaopen_file(lua_State *ls);
void cluaopen_food(lua_State *ls);
void cluaopen_item(lua_State *ls);
void cluaopen_kills(lua_State *ls);     // defined in kills.cc
void cluaopen_monsters(lua_State *ls);
void cluaopen_options(lua_State *ls);
void cluaopen_you(lua_State *ls);

void cluaopen_globals(lua_State *ls);

/*
 * (Shared) metatable names.
 */

#define MAP_METATABLE "dgn.mtmap"
#define DEVENT_METATABLE "dgn.devent"
#define MAPMARK_METATABLE "dgn.mapmark"
#define MAPGRD_METATABLE "dgn.mapgrd"
#define MAPGRD_COL_METATABLE "dgn.mapgrdcol"

/*
 * Libraries and loaders for dlua, accessed from init_dungeon_lua().
 * TODO: Rename these to dluaopen_*.
 */

extern const struct luaL_reg debug_dlib[];
extern const struct luaL_reg dgn_dlib[];
extern const struct luaL_reg dgn_build_dlib[];
extern const struct luaL_reg dgn_event_dlib[];
extern const struct luaL_reg dgn_grid_dlib[];
extern const struct luaL_reg dgn_item_dlib[];
extern const struct luaL_reg dgn_level_dlib[];
extern const struct luaL_reg dgn_mons_dlib[];
extern const struct luaL_reg dgn_tile_dlib[];
extern const struct luaL_reg los_dlib[];
extern const struct luaL_reg mapmarker_dlib[];

void luaopen_dgnevent(lua_State *ls);
void luaopen_mapmarker(lua_State *ls);
void luaopen_ray(lua_State *ls);

void register_monslist(lua_State *ls);
void register_itemlist(lua_State *ls);
void register_builder_funcs(lua_State *ls);

void dluaopen_crawl(lua_State *ls);
void dluaopen_file(lua_State *ls);
void dluaopen_mapgrd(lua_State *ls);
void dluaopen_you(lua_State *ls);

/*
 * Macros for processing object arguments.
 * TODO: Collect these in cluautil.{h,cc}.
 */
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


/*
 * Some shared helper functions.
 */
class map_lines;
int dgn_map_add_transform(lua_State *ls,
          std::string (map_lines::*add)(const std::string &s));
unsigned int get_tile_idx(lua_State *ls, int arg);
level_id dlua_level_id(lua_State *ls, int ndx);
dungeon_feature_type check_lua_feature(lua_State *ls, int idx);
item_def *clua_check_item(lua_State *ls, int n);

#endif
