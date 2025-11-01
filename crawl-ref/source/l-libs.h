/**
 * @file
 * @brief Library definitions for dlua.
**/

#pragma once

#include "clua.h"

/*
 * Library loaders for clua.
 */

void cluaopen_crawl(lua_State *ls);
void cluaopen_file(lua_State *ls);
void cluaopen_item(lua_State *ls);
void cluaopen_kills(lua_State *ls);     // defined in kills.cc
void cluaopen_moninf(lua_State *ls);
void cluaopen_options(lua_State *ls);
void cluaopen_travel(lua_State *ls);
void cluaopen_view(lua_State *ls);
void cluaopen_you(lua_State *ls);
void cluaopen_spells(lua_State *ls);

void cluaopen_globals(lua_State *ls);

/*
 * (Shared) metatable names.
 */

#define MAP_METATABLE "dgn.mtmap"
#define DEVENT_METATABLE "dgn.devent"
#define MAPMARK_METATABLE "dgn.mapmark"
#define MAPGRD_METATABLE "dgn.mapgrd"
#define MAPGRD_COL_METATABLE "dgn.mapgrdcol"
#define ITEM_METATABLE "item.itemaccess"
#define MONS_METATABLE "monster.monsaccess"

/*
 * Libraries and loaders for dlua, accessed from init_dungeon_lua().
 * TODO: Rename these to dluaopen_*?
 */

extern const struct luaL_Reg debug_dlib[];
extern const struct luaL_Reg dgn_dlib[];
extern const struct luaL_Reg dgn_build_dlib[];
extern const struct luaL_Reg dgn_event_dlib[];
extern const struct luaL_Reg dgn_grid_dlib[];
extern const struct luaL_Reg dgn_item_dlib[];
extern const struct luaL_Reg dgn_level_dlib[];
extern const struct luaL_Reg dgn_mons_dlib[];
extern const struct luaL_Reg dgn_subvault_dlib[];
extern const struct luaL_Reg dgn_tile_dlib[];
extern const struct luaL_Reg feat_dlib[];
extern const struct luaL_Reg los_dlib[];
extern const struct luaL_Reg mapmarker_dlib[];

void luaopen_dgnevent(lua_State *ls);
void luaopen_mapmarker(lua_State *ls);
void luaopen_ray(lua_State *ls);

void register_monslist(lua_State *ls);
void register_itemlist(lua_State *ls);
void register_builder_funcs(lua_State *ls);

void dluaopen_crawl(lua_State *ls);
void dluaopen_file(lua_State *ls);
void dluaopen_mapgrd(lua_State *ls);
void dluaopen_monsters(lua_State *ls);
void dluaopen_you(lua_State *ls);
void dluaopen_dgn(lua_State *ls);
void dluaopen_colour(lua_State *ls);
#ifdef WIZARD
void dluaopen_wiz(lua_State *ls);
#endif

/*
 * Some shared helper functions.
 */
class map_lines;
int dgn_map_add_transform(lua_State *ls,
                          string (map_lines::*add)(const string &s));

struct monster_info;
void lua_push_moninf(lua_State *ls, monster_info *mi);

int lua_push_shop_items_at(lua_State *ls, const coord_def &s);
