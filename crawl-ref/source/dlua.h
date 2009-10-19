/*
 *  File:       dlua.h
 *  Summary:    Dungeon-builder Lua interface.
 *  Created by: dshaligram on Sat Jun 23 20:02:09 2007 UTC
 */

#ifndef LUADGN_H
#define LUADGN_H

#include "clua.h"
#include "stuff.h" // for COORDS macro

// Defined in acr.cc
extern CLua dlua;

// Lua chunks cannot exceed 512K. Which is plenty!
const int LUA_CHUNK_MAX_SIZE = 512 * 1024;

class reader;
class writer;

class dlua_chunk
{
private:
    std::string file;
    std::string chunk;
    std::string compiled;
    std::string context;
    int first, last;     // First and last lines of the original source.

    enum chunk_t
    {
        CT_EMPTY,
        CT_SOURCE,
        CT_COMPILED
    };

private:
    int check_op(CLua &, int);
    std::string rewrite_chunk_prefix(const std::string &line,
                                     bool skip_body = false) const;
    std::string get_chunk_prefix(const std::string &s) const;

public:
    mutable std::string error;

public:
    dlua_chunk(const std::string &_context = "dlua_chunk");
    dlua_chunk(lua_State *ls);

    static dlua_chunk precompiled(const std::string &compiled);

    void clear();
    void add(int line, const std::string &line2);
    void set_chunk(const std::string &s);

    int load(CLua &interp);
    int run(CLua &interp);
    int load_call(CLua &interp, const char *function);
    void set_file(const std::string &s);

    const std::string &lua_string() const { return chunk; }
    std::string orig_error() const;
    bool rewrite_chunk_errors(std::string &err) const;

    bool empty() const;

    const std::string &compiled_chunk() const { return compiled; }

    void write(writer&) const;
    void read(reader&);
};

void init_dungeon_lua();
std::string dgn_set_default_depth(const std::string &s);
void dgn_reset_default_depth();
int dlua_stringtable(lua_State *ls, const std::vector<std::string> &s);

dungeon_feature_type dungeon_feature_by_name(const std::string &name);
std::vector<std::string> dungeon_feature_matches(const std::string &name);
const char *dungeon_feature_name(dungeon_feature_type feat);

template <typename T>
inline void dlua_push_userdata(lua_State *ls, T udata, const char *meta)
{
    T *de = clua_new_userdata<T>(ls, meta);
    *de = udata;
}

void print_dlua_stack();


void luaopen_setmeta(lua_State *ls,
                     const char *global,
                     const luaL_reg *lua_lib,
                     const char *meta);

#define LUAFN(name) static int name(lua_State *ls)

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

#define MAP(ls, n, var)                             \
map_def *var = *(map_def **) luaL_checkudata(ls, n, MAP_METATABLE)
#define DEVENT(ls, n, var) \
dgn_event *var = *(dgn_event **) luaL_checkudata(ls, n, DEVENT_METATABLE)
#define MAPMARKER(ls, n, var) \
map_marker *var = *(map_marker **) luaL_checkudata(ls, n, MAPMARK_METATABLE)

//////////////////////////////////////////////////////////////////////////

#endif
