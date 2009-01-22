/*
 *  File:       luadgn.h
 *  Summary:    Dungeon-builder Lua interface.
 *  Created by: dshaligram on Sat Jun 23 20:02:09 2007 UTC
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#ifndef LUADGN_H
#define LUADGN_H

#include "AppHdr.h"
#include "clua.h"

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

//////////////////////////////////////////////////////////////////////////

#endif
