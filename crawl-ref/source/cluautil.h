/**
 * @file
 * @brief Utility functions and macros for Lua bindings.
**/

#ifndef CLUAUTIL_H
#define CLUAUTIL_H

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

/*
 * Function definitions.
 */

#define LUAFN(name) static int name(lua_State *ls)
#define LUAWRAP(name, wrapexpr) \
    static int name(lua_State *ls) \
    {   \
        wrapexpr; \
        return 0; \
    }
#define PLUARET(type, val) \
    { \
        lua_push##type(ls, val); \
        return 1; \
    }
#define LUARET1(name, type, val) \
    static int name(lua_State *ls) \
    { \
        lua_push##type(ls, val); \
        return 1; \
    }
#define LUARET2(name, type, val1, val2)  \
    static int name(lua_State *ls) \
    { \
        lua_push##type(ls, val1); \
        lua_push##type(ls, val2); \
        return 2; \
    }

#define ASSERT_DLUA \
    do {                                                            \
        if (CLua::get_vm(ls).managed_vm)                            \
            luaL_error(ls, "Operation forbidden in end-user script");   \
    } while (false)

// FIXME: remove one of these.
void luaopen_setmeta(lua_State *ls,
                     const char *global,
                     const luaL_reg *lua_lib,
                     const char *meta);

void clua_register_metatable(lua_State *ls, const char *tn,
                             const luaL_reg *lr,
                             int (*gcfn)(lua_State *ls) = nullptr);

int clua_stringtable(lua_State *ls, const vector<string> &s);

/*
 * User-data templates.
 * TODO: Consolidate these.
 */

template <class T>
static inline T *clua_get_lightuserdata(lua_State *ls, int ndx)
{
    return (lua_islightuserdata(ls, ndx))?
            static_cast<T *>(lua_touserdata(ls, ndx))
          : nullptr;
}

template <class T>
static inline T *clua_get_userdata(lua_State *ls, const char *mt, int ndx = 1)
{
    return static_cast<T*>(luaL_checkudata(ls, ndx, mt));
}

template <class T>
static int lua_object_gc(lua_State *ls)
{
    T **pptr = static_cast<T**>(lua_touserdata(ls, 1));
    if (pptr)
        delete *pptr;
    return 0;
}

template <class T> T *clua_new_userdata(
        lua_State *ls, const char *mt)
{
    void *udata = lua_newuserdata(ls, sizeof(T));
    luaL_getmetatable(ls, mt);
    lua_setmetatable(ls, -2);
    return static_cast<T*>(udata);
}

template <typename T>
static inline void dlua_push_userdata(lua_State *ls, T udata, const char *meta)
{
    T *de = clua_new_userdata<T>(ls, meta);
    *de = udata;
}

template <class T>
static int dlua_push_object_type(lua_State *ls, const char *meta, const T &data)
{
    T **ptr = clua_new_userdata<T*>(ls, meta);
    if (ptr)
        *ptr = new T(data);
    else
        lua_pushnil(ls);
    return 1;
}

/*
 * Passing objects from and to Lua.
 */
struct activity_interrupt_data;
int push_activity_interrupt(lua_State *ls, activity_interrupt_data *t);

class map_def;
void clua_push_map(lua_State *ls, map_def *map);

class dgn_event;
void clua_push_dgn_event(lua_State *ls, const dgn_event *devent);

class monster;
struct MonsterWrap
{
    monster* mons;
    int      turn;
};

// XXX: These are currently defined outside cluautil.cc.
void push_monster(lua_State *ls, monster* mons);
void clua_push_item(lua_State *ls, item_def *item);
item_def *clua_get_item(lua_State *ls, int ndx);
void lua_push_floor_items(lua_State *ls, int link);
dungeon_feature_type check_lua_feature(lua_State *ls, int idx,
                                       bool optional = false);
tileidx_t get_tile_idx(lua_State *ls, int arg);
level_id dlua_level_id(lua_State *ls, int ndx);

#define GETCOORD(c, p1, p2, boundfn)                      \
    coord_def c;                                          \
    c.x = luaL_checkint(ls, p1);                          \
    c.y = luaL_checkint(ls, p2);                          \
    if (!boundfn(c))                                        \
        luaL_error(                                          \
            ls,                                                 \
            make_stringf("Point (%d,%d) is out of bounds",      \
                         c.x, c.y).c_str());                    \
    else {};

#define COORDS(c, p1, p2)                                \
    GETCOORD(c, p1, p2, in_bounds)

#define COORDSHOW(c, p1, p2) \
    GETCOORD(c, p1, p2, in_show_bounds)

#define FEAT(f, pos) \
dungeon_feature_type f = check_lua_feature(ls, pos)

#define LEVEL(br, pos)                                              \
    const char *branch_name = luaL_checkstring(ls, pos);            \
    branch_type br = str_to_branch(branch_name);                    \
    if (br == NUM_BRANCHES)                                         \
        luaL_error(ls, "Expected branch name");

#define MAP(ls, n, var) \
map_def *var = *(map_def **) luaL_checkudata(ls, n, MAP_METATABLE)
#define LINES(ls, n, var) \
map_lines &var = (*(map_def **) luaL_checkudata(ls, n, MAP_METATABLE))->map
#define DEVENT(ls, n, var) \
dgn_event *var = *(dgn_event **) luaL_checkudata(ls, n, DEVENT_METATABLE)
#define MAPMARKER(ls, n, var) \
map_marker *var = *(map_marker **) luaL_checkudata(ls, n, MAPMARK_METATABLE)
#define LUA_ITEM(ls, name, n) \
item_def *item = *(item_def **) luaL_checkudata(ls, n, ITEM_METATABLE)

template <typename list, typename lpush>
static int clua_gentable(lua_State *ls, const list &strings, lpush push)
{
    lua_newtable(ls);
    for (int i = 0, size = strings.size(); i < size; ++i)
    {
        push(ls, strings[i]);
        lua_rawseti(ls, -2, i + 1);
    }
    return 1;
}

int clua_pushcxxstring(lua_State *ls, const string &s);
int clua_pushpoint(lua_State *ls, const coord_def &pos);

#endif
