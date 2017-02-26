/**
 * @file
 * @brief Dungeon-builder Lua interface.
**/

#pragma once

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
    string file;
    string chunk;
    string compiled;
    string context;
    int first, last;     // First and last lines of the original source.

    enum chunk_t
    {
        CT_EMPTY,
        CT_SOURCE,
        CT_COMPILED
    };

private:
    int check_op(CLua &, int);
    string rewrite_chunk_prefix(const string &line, bool skip_body = false) const;
    string get_chunk_prefix(const string &s) const;

public:
    mutable string error;

public:
    dlua_chunk(const string &_context = "dlua_chunk");
    dlua_chunk(lua_State *ls);

    static dlua_chunk precompiled(const string &compiled);

    string describe(const string &chunkname) const;
    void clear();
    void add(int line, const string &line2);
    void set_chunk(const string &s);

    int load(CLua &interp);
    int run(CLua &interp);
    int load_call(CLua &interp, const char *function);
    void set_file(const string &s);

    const string &lua_string() const { return chunk; }
    string orig_error() const;
    bool rewrite_chunk_errors(string &err) const;

    bool empty() const;

    const string &compiled_chunk() const { return compiled; }

    void write(writer&) const;
    void read(reader&);
};

void init_dungeon_lua();
