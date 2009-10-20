/*
 *  File:       dlua.cc
 *  Summary:    Dungeon-builder Lua interface.
 *  Created by: dshaligram on Sat Jun 23 20:02:09 2007 UTC
 */

#include "AppHdr.h"

#include <sstream>

#include "dlua.h"
#include "l_libs.h"

#include "tags.h"

template <typename list, typename lpush>
static int dlua_gentable(lua_State *ls, const list &strings, lpush push)
{
    lua_newtable(ls);
    for (int i = 0, size = strings.size(); i < size; ++i)
    {
        push(ls, strings[i]);
        lua_rawseti(ls, -2, i + 1);
    }
    return (1);
}

inline static void dlua_pushcxxstring(lua_State *ls, const std::string &s)
{
    lua_pushstring(ls, s.c_str());
}

int dlua_stringtable(lua_State *ls, const std::vector<std::string> &s)
{
    return dlua_gentable(ls, s, dlua_pushcxxstring);
}

static int dlua_compiled_chunk_writer(lua_State *ls, const void *p,
                                      size_t sz, void *ud)
{
    std::ostringstream &out = *static_cast<std::ostringstream*>(ud);
    out.write((const char *) p, sz);
    return (0);
}

///////////////////////////////////////////////////////////////////////////
// dlua_chunk

dlua_chunk::dlua_chunk(const std::string &_context)
    : file(), chunk(), compiled(), context(_context), first(-1),
      last(-1), error()
{
    clear();
}

// Initialises a chunk from the function on the top of stack.
// This function must not be a closure, i.e. must not have any upvalues.
dlua_chunk::dlua_chunk(lua_State *ls)
    : file(), chunk(), compiled(), context(), first(-1), last(-1), error()
{
    clear();

    lua_stack_cleaner cln(ls);
    std::ostringstream out;
    const int err = lua_dump(ls, dlua_compiled_chunk_writer, &out);
    if (err)
    {
        const char *e = lua_tostring(ls, -1);
        error = e? e : "Unknown error compiling chunk";
    }
    compiled = out.str();
}

dlua_chunk dlua_chunk::precompiled(const std::string &chunk)
{
    dlua_chunk dchunk;
    dchunk.compiled = chunk;
    return (dchunk);
}

void dlua_chunk::write(writer& outf) const
{
    if (empty())
    {
        marshallByte(outf, CT_EMPTY);
        return;
    }

    if (!compiled.empty())
    {
        marshallByte(outf, CT_COMPILED);
        marshallString4(outf, compiled);
    }
    else
    {
        marshallByte(outf, CT_SOURCE);
        marshallString4(outf, chunk);
    }

    marshallString4(outf, file);
    marshallLong(outf, first);
}

void dlua_chunk::read(reader& inf)
{
    clear();
    chunk_t type = static_cast<chunk_t>(unmarshallByte(inf));
    switch (type)
    {
    case CT_EMPTY:
        return;
    case CT_SOURCE:
        unmarshallString4(inf, chunk);
        break;
    case CT_COMPILED:
        unmarshallString4(inf, compiled);
        break;
    }
    unmarshallString4(inf, file);
    first = unmarshallLong(inf);
}

void dlua_chunk::clear()
{
    file.clear();
    chunk.clear();
    first = last = -1;
    error.clear();
    compiled.clear();
}

void dlua_chunk::set_file(const std::string &s)
{
    file = s;
}

void dlua_chunk::add(int line, const std::string &s)
{
    if (first == -1)
        first = line;

    if (line != last && last != -1)
        while (last++ < line)
            chunk += '\n';

    chunk += " ";
    chunk += s;
    last = line;
}

void dlua_chunk::set_chunk(const std::string &s)
{
    chunk = s;
}

int dlua_chunk::check_op(CLua &interp, int err)
{
    error = interp.error;
    return (err);
}

int dlua_chunk::load(CLua &interp)
{
    if (!compiled.empty())
        return check_op( interp,
                         interp.loadbuffer(compiled.c_str(), compiled.length(),
                                           context.c_str()) );

    if (empty())
    {
        chunk.clear();
        return (-1000);
    }

    int err = check_op( interp,
                        interp.loadstring(chunk.c_str(), context.c_str()) );
    if (err)
        return (err);
    std::ostringstream out;
    err = lua_dump(interp, dlua_compiled_chunk_writer, &out);
    if (err)
    {
        const char *e = lua_tostring(interp, -1);
        error = e? e : "Unknown error compiling chunk";
        lua_pop(interp, 2);
    }
    compiled = out.str();
    chunk.clear();
    return (err);
}

int dlua_chunk::run(CLua &interp)
{
    int err = load(interp);
    if (err)
        return (err);
    // callfn returns true on success, but we want to return 0 on success.
    return (check_op(interp, !interp.callfn(NULL, 0, 0)));
}

int dlua_chunk::load_call(CLua &interp, const char *fn)
{
    int err = load(interp);
    if (err == -1000)
        return (0);
    if (err)
        return (err);

    return check_op(interp, !interp.callfn(fn, fn? 1 : 0, 0));
}

std::string dlua_chunk::orig_error() const
{
    rewrite_chunk_errors(error);
    return (error);
}

bool dlua_chunk::empty() const
{
    return compiled.empty() && trimmed_string(chunk).empty();
}

bool dlua_chunk::rewrite_chunk_errors(std::string &s) const
{
    const std::string contextm = "[string \"" + context + "\"]:";
    std::string::size_type dlwhere = s.find(contextm);

    if (dlwhere == std::string::npos)
        return (false);

    if (!dlwhere)
    {
        s = rewrite_chunk_prefix(s);
        return (true);
    }

    // Our chunk is mentioned, go back through and rewrite lines.
    std::vector<std::string> lines = split_string("\n", s);
    std::string newmsg = lines[0];
    bool wrote_prefix = false;
    for (int i = 2, size = lines.size() - 1; i < size; ++i)
    {
        const std::string &st = lines[i];
        if (st.find(context) != std::string::npos)
        {
            if (!wrote_prefix)
            {
                newmsg = get_chunk_prefix(st) + ": " + newmsg;
                wrote_prefix = true;
            }
            else
                newmsg += "\n" + rewrite_chunk_prefix(st);
        }
    }
    s = newmsg;
    return (true);
}

std::string dlua_chunk::rewrite_chunk_prefix(const std::string &line,
                                             bool skip_body) const
{
    std::string s = line;
    const std::string contextm = "[string \"" + context + "\"]:";
    const std::string::size_type ps = s.find(contextm);
    if (ps == std::string::npos)
        return (s);

    const std::string::size_type lns = ps + contextm.length();
    std::string::size_type pe = s.find(':', ps + contextm.length());
    if (pe != std::string::npos)
    {
        const std::string line_num = s.substr(lns, pe - lns);
        const int lnum = atoi(line_num.c_str());
        const std::string newlnum = make_stringf("%d", lnum + first - 1);
        s = s.substr(0, lns) + newlnum + s.substr(pe);
        pe = lns + newlnum.length();
    }

    return s.substr(0, ps) + (file.empty()? context : file) + ":"
        + (skip_body? s.substr(lns, pe - lns)
                    : s.substr(lns));
}

std::string dlua_chunk::get_chunk_prefix(const std::string &sorig) const
{
    return rewrite_chunk_prefix(sorig, true);
}

void luaopen_setmeta(lua_State *ls,
                     const char *global,
                     const luaL_reg *lua_lib,
                     const char *meta)
{
    luaL_newmetatable(ls, meta);
    lua_setglobal(ls, global);

    luaL_openlib(ls, global, lua_lib, 0);

    // Do <global>.__index = <global>
    lua_pushstring(ls, "__index");
    lua_pushvalue(ls, -2);
    lua_settable(ls, -3);
}

void init_dungeon_lua()
{
    lua_stack_cleaner clean(dlua);

    luaL_openlib(dlua, "dgn", dgn_lib, 0);
    luaL_openlib(dlua, "dgn", dgn_build_lib, 0);
    luaL_openlib(dlua, "dgn", dgn_event_lib, 0);
    luaL_openlib(dlua, "dgn", dgn_grid_lib, 0);
    luaL_openlib(dlua, "dgn", dgn_item_lib, 0);
    luaL_openlib(dlua, "dgn", dgn_level_lib, 0);
    luaL_openlib(dlua, "dgn", dgn_mons_lib, 0);
    luaL_openlib(dlua, "dgn", dgn_tile_lib, 0);
    // Add additional function to the Crawl module.
    luaL_openlib(dlua, "crawl", crawl_lib, 0);
    luaL_openlib(dlua, "file", file_lib, 0);
    luaL_openlib(dlua, "you", you_lib, 0);
    luaL_openlib(dlua, "los", los_lib, 0);

    dlua.execfile("clua/dungeon.lua", true, true);
    dlua.execfile("clua/luamark.lua", true, true);

    lua_getglobal(dlua, "dgn_run_map");
    luaopen_debug(dlua);
    luaL_newmetatable(dlua, MAP_METATABLE);

    luaopen_dgnevent(dlua);
    luaopen_mapmarker(dlua);
    luaopen_ray(dlua);

    register_builder_funcs(dlua);

    register_itemlist(dlua);
    register_monslist(dlua);
}

// Can be called from within a debugger to look at the current Lua
// call stack.  (Borrowed from ToME 3)
void print_dlua_stack(void)
{
    struct lua_Debug dbg;
    int              i = 0;
    lua_State       *L = dlua.state();

    fprintf(stderr, EOL);
    while (lua_getstack(L, i++, &dbg) == 1)
    {
        lua_getinfo(L, "lnuS", &dbg);

        char* file = strrchr(dbg.short_src, '/');
        if (file == NULL)
            file = dbg.short_src;
        else
            file++;

        fprintf(stderr, "%s, function %s, line %d" EOL, file,
                dbg.name, dbg.currentline);
    }

    fprintf(stderr, EOL);
}
