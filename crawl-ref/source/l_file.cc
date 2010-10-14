#include "AppHdr.h"

#include "clua.h"
#include "cluautil.h"
#include "dlua.h"
#include "l_libs.h"
#include "libutil.h"
#include "files.h"

#include "tags.h"

///////////////////////////////////////////////////////////
// User-accessible file operations

static const struct luaL_reg file_clib[] =
{
    { "write", CLua::file_write },
    { NULL, NULL },
};

void cluaopen_file(lua_State *ls)
{
    luaL_openlib(ls, "file", file_clib, 0);
}

///////////////////////////////////////////////////////////
// Non-user-accessible file operations

static int file_marshall(lua_State *ls)
{
    if (lua_gettop(ls) != 2)
        luaL_error(ls, "Need two arguments: tag header and value");
    writer &th(*static_cast<writer*>(lua_touserdata(ls, 1)));
    if (lua_isnumber(ls, 2))
        marshallInt(th, luaL_checklong(ls, 2));
    else if (lua_isboolean(ls, 2))
        marshallByte(th, lua_toboolean(ls, 2));
    else if (lua_isstring(ls, 2))
        marshallString(th, lua_tostring(ls, 2));
    else if (lua_isfunction(ls, 2))
    {
        dlua_chunk chunk(ls);
        marshallString(th, chunk.compiled_chunk());
    }
    return (0);
}

static int file_unmarshall_boolean(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>(lua_touserdata(ls, 1)));
    lua_pushboolean(ls, unmarshallByte(th));
    return (1);
}

static int file_unmarshall_number(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>(lua_touserdata(ls, 1)));
    lua_pushnumber(ls, unmarshallInt(th));
    return (1);
}

static int file_unmarshall_string(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>(lua_touserdata(ls, 1)));
    lua_pushstring(ls, unmarshallString(th).c_str());
    return (1);
}

static int file_unmarshall_fn(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>(lua_touserdata(ls, 1)));
    const std::string s(unmarshallString(th, LUA_CHUNK_MAX_SIZE));
    dlua_chunk chunk = dlua_chunk::precompiled(s);
    if (chunk.load(dlua))
        lua_pushnil(ls);
    return (1);
}

enum lua_persist_type
{
    LPT_NONE,
    LPT_NUMBER,
    LPT_STRING,
    LPT_FUNCTION,
    LPT_NIL,
    LPT_BOOLEAN,
};

static int file_marshall_meta(lua_State *ls)
{
    if (lua_gettop(ls) != 2)
        luaL_error(ls, "Need two arguments: tag header and value");

    writer &th(*static_cast<writer*>(lua_touserdata(ls, 1)));

    lua_persist_type ptype = LPT_NONE;
    if (lua_isnumber(ls, 2))
        ptype = LPT_NUMBER;
    else if (lua_isboolean(ls, 2))
        ptype = LPT_BOOLEAN;
    else if (lua_isstring(ls, 2))
        ptype = LPT_STRING;
    else if (lua_isfunction(ls, 2))
        ptype = LPT_FUNCTION;
    else if (lua_isnil(ls, 2))
        ptype = LPT_NIL;
    else
        luaL_error(ls,
                   make_stringf("Cannot marshall %s",
                                lua_typename(ls, lua_type(ls, 2))).c_str());
    marshallByte(th, ptype);
    if (ptype != LPT_NIL)
        file_marshall(ls);
    return (0);
}

static int file_unmarshall_meta(lua_State *ls)
{
    reader &th(*static_cast<reader*>(lua_touserdata(ls, 1)));
    const lua_persist_type ptype =
    static_cast<lua_persist_type>(unmarshallByte(th));
    switch (ptype)
    {
        case LPT_BOOLEAN:
            return file_unmarshall_boolean(ls);
        case LPT_NUMBER:
            return file_unmarshall_number(ls);
        case LPT_STRING:
            return file_unmarshall_string(ls);
        case LPT_FUNCTION:
            return file_unmarshall_fn(ls);
        case LPT_NIL:
            lua_pushnil(ls);
            return (1);
        default:
            luaL_error(ls, "Unexpected type signature.");
    }
    // Never get here.
    return (0);
}

// Returns a Lua table of filenames in the named directory. The file names
// returned are unqualified. The directory must be a relative path, and will
// be resolved to an absolute path if necessary using datafile_path.
LUAFN(_file_datadir_files_recursive)
{
    const std::string rawdir(luaL_checkstring(ls, 1));
    // A filename suffix to match (such as ".des"). If empty, files
    // will be unfiltered.
    const std::string ext_filter(lua_isnoneornil(ls, 2) ? "" :
                                 luaL_checkstring(ls, 2));
    const std::string datadir(
        datafile_path(rawdir, false, false, dir_exists));

    if (datadir.empty())
        luaL_error(ls, "Cannot find data directory: '%s'", rawdir.c_str());

    const std::vector<std::string> files =
        get_dir_files_recursive(datadir, ext_filter);
    return clua_stringtable(ls, files);
}

// Returns a Lua table of filenames in the named directory. The file names
// returned are unqualified. The directory must be a relative path, and will
// be resolved to an absolute path if necessary using datafile_path.
LUAFN(_file_datadir_files)
{
    const std::string rawdir(luaL_checkstring(ls, 1));
    // A filename suffix to match (such as ".des"). If empty, files
    // will be unfiltered.
    const std::string ext_filter(lua_isnoneornil(ls, 2) ? "" :
                                 luaL_checkstring(ls, 2));
    const std::string datadir(
        datafile_path(rawdir, false, false, dir_exists));

    if (datadir.empty())
        luaL_error(ls, "Cannot find data directory: '%s'", rawdir.c_str());

    const std::vector<std::string> files =
        ext_filter.empty() ? get_dir_files(datadir) :
        get_dir_files_ext(datadir, ext_filter);
    return clua_stringtable(ls, files);
}

LUAFN(_file_writefile)
{
    const std::string fname(luaL_checkstring(ls, 1));
    FILE *f = fopen_replace(fname.c_str());
    if (f)
    {
        fprintf(f, "%s", luaL_checkstring(ls, 2));
        fclose(f);
        lua_pushboolean(ls, true);
    }
    else
    {
        lua_pushboolean(ls, false);
    }
    return (1);
}

static const struct luaL_reg file_dlib[] =
{
    { "marshall",   file_marshall },
    { "marshall_meta", file_marshall_meta },
    { "unmarshall_meta", file_unmarshall_meta },
    { "unmarshall_boolean", file_unmarshall_boolean },
    { "unmarshall_number", file_unmarshall_number },
    { "unmarshall_string", file_unmarshall_string },
    { "unmarshall_fn", file_unmarshall_fn },
    { "writefile", _file_writefile },
    { "datadir_files", _file_datadir_files },
    { "datadir_files_recursive", _file_datadir_files_recursive },
    { NULL, NULL }
};

void dluaopen_file(lua_State *ls)
{
    luaL_openlib(ls, "file", file_dlib, 0);
}
