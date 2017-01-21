/**
 * @file
 * @brief Dungeon-builder Lua interface.
**/

#include "AppHdr.h"

#include "dlua.h"

#include <sstream>

#include "l-libs.h"
#include "stringutil.h"

static int dlua_compiled_chunk_writer(lua_State *ls, const void *p,
                                      size_t sz, void *ud)
{
    ostringstream &out = *static_cast<ostringstream*>(ud);
    out.write(static_cast<const char *>(p), sz);
    return 0;
}

///////////////////////////////////////////////////////////////////////////
// dlua_chunk

dlua_chunk::dlua_chunk(const string &_context)
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
    ostringstream out;
    const int err = lua_dump(ls, dlua_compiled_chunk_writer, &out);
    if (err)
    {
        const char *e = lua_tostring(ls, -1);
        error = e? e : "Unknown error compiling chunk";
    }
    compiled = out.str();
}

dlua_chunk dlua_chunk::precompiled(const string &_chunk)
{
    dlua_chunk dchunk;
    dchunk.compiled = _chunk;
    return dchunk;
}

string dlua_chunk::describe(const string &name) const
{
    if (chunk.empty())
        return "";
    return make_stringf("function %s()\n%s\nend\n",
                        name.c_str(), chunk.c_str());
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
    marshallInt(outf, first);
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
    first = unmarshallInt(inf);
}

void dlua_chunk::clear()
{
    file.clear();
    chunk.clear();
    first = last = -1;
    error.clear();
    compiled.clear();
}

void dlua_chunk::set_file(const string &s)
{
    file = s;
}

void dlua_chunk::add(int line, const string &s)
{
    if (first == -1)
        first = line;

    if (line != last && last != -1)
    {
        while (last++ < line)
            chunk += '\n';
    }

    chunk += " ";
    chunk += s;
    last = line;
}

void dlua_chunk::set_chunk(const string &s)
{
    chunk = s;
}

int dlua_chunk::check_op(CLua &interp, int err)
{
    error = interp.error;
    return err;
}

int dlua_chunk::load(CLua &interp)
{
    if (!compiled.empty())
    {
        return check_op(interp,
                         interp.loadbuffer(compiled.c_str(), compiled.length(),
                                           context.c_str()));
    }

    if (empty())
    {
        chunk.clear();
        return E_CHUNK_LOAD_FAILURE;
    }

    int err = check_op(interp,
                        interp.loadstring(chunk.c_str(), context.c_str()));
    if (err)
        return err;
    ostringstream out;
    err = lua_dump(interp, dlua_compiled_chunk_writer, &out);
    if (err)
    {
        const char *e = lua_tostring(interp, -1);
        error = e? e : "Unknown error compiling chunk";
        lua_pop(interp, 2);
    }
    compiled = out.str();
    return err;
}

int dlua_chunk::run(CLua &interp)
{
    int err = load(interp);
    if (err)
        return err;
    // callfn returns true on success, but we want to return 0 on success.
    return check_op(interp, !interp.callfn(nullptr, 0, 0));
}

int dlua_chunk::load_call(CLua &interp, const char *fn)
{
    int err = load(interp);
    if (err == E_CHUNK_LOAD_FAILURE)
        return 0;
    if (err)
        return err;

    return check_op(interp, !interp.callfn(fn, fn? 1 : 0, 0));
}

string dlua_chunk::orig_error() const
{
    rewrite_chunk_errors(error);
    return error;
}

bool dlua_chunk::empty() const
{
    return compiled.empty() && trimmed_string(chunk).empty();
}

bool dlua_chunk::rewrite_chunk_errors(string &s) const
{
    const string contextm = "[string \"" + context + "\"]:";
    string::size_type dlwhere = s.find(contextm);

    if (dlwhere == string::npos)
        return false;

    if (!dlwhere)
    {
        s = rewrite_chunk_prefix(s);
        return true;
    }

    // Our chunk is mentioned, go back through and rewrite lines.
    vector<string> lines = split_string("\n", s);
    string newmsg = lines[0];
    bool wrote_prefix = false;
    for (int i = 2, size = lines.size() - 1; i < size; ++i)
    {
        const string &st = lines[i];
        if (st.find(context) != string::npos)
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
    return true;
}

string dlua_chunk::rewrite_chunk_prefix(const string &line, bool skip_body) const
{
    string s = line;
    const string contextm = "[string \"" + context + "\"]:";
    const string::size_type ps = s.find(contextm);
    if (ps == string::npos)
        return s;

    const string::size_type lns = ps + contextm.length();
    string::size_type pe = s.find(':', ps + contextm.length());
    if (pe != string::npos)
    {
        const string line_num = s.substr(lns, pe - lns);
        const int lnum = atoi(line_num.c_str());
        const string newlnum = to_string(lnum + first - 1);
        s = s.substr(0, lns) + newlnum + s.substr(pe);
        pe = lns + newlnum.length();
    }

    return s.substr(0, ps) + (file.empty()? context : file) + ":"
        + (skip_body? s.substr(lns, pe - lns)
                    : s.substr(lns));
}

string dlua_chunk::get_chunk_prefix(const string &sorig) const
{
    return rewrite_chunk_prefix(sorig, true);
}

static void _dlua_register_constants(CLua &lua)
{
    lua_pushstring(lua, CORPSE_NEVER_DECAYS);
    lua.setglobal("CORPSE_NEVER_DECAYS");
}

void init_dungeon_lua()
{
    lua_stack_cleaner clean(dlua);

    dluaopen_colour(dlua);
    dluaopen_crawl(dlua);
    dluaopen_file(dlua);
    dluaopen_mapgrd(dlua);
    dluaopen_monsters(dlua);
    dluaopen_you(dlua);
    dluaopen_dgn(dlua);

    luaL_openlib(dlua, "feat", feat_dlib, 0);
    luaL_openlib(dlua, "spells", spells_dlib, 0);
    luaL_openlib(dlua, "debug", debug_dlib, 0);
    luaL_openlib(dlua, "los", los_dlib, 0);

    dlua.execfile("dlua/dungeon.lua", true, true);
    dlua.execfile("dlua/luamark.lua", true, true);
    dlua.execfile("dlua/mapinit.lua", true, true);

    lua_getglobal(dlua, "dgn_run_map");
    luaopen_debug(dlua);
    luaL_newmetatable(dlua, MAP_METATABLE);

    luaopen_dgnevent(dlua);
    luaopen_mapmarker(dlua);
    luaopen_ray(dlua);

    register_itemlist(dlua);
    register_monslist(dlua);

    _dlua_register_constants(dlua);
}
