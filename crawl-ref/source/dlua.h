/*
 *  File:       dlua.h
 *  Summary:    Dungeon-builder Lua interface.
 *  Created by: dshaligram on Sat Jun 23 20:02:09 2007 UTC
 */

#ifndef DLUA_H
#define DLUA_H

#include "clua.h"

// Defined in main.cc
#ifdef DEBUG_GLOBALS
#define dlua (*real_dlua)
#endif
extern CLua dlua;

// Lua chunks cannot exceed 512K. Which is plenty!
const int LUA_CHUNK_MAX_SIZE = 512 * 1024;
const int E_CHUNK_LOAD_FAILURE = -1000;

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

#endif
