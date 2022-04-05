#include "AppHdr.h"

#include "l-libs.h"

#include "clua.h"
#include "cluautil.h"
#include "dlua.h"
#include "files.h"
#include "libutil.h"
#include "stringutil.h"
#include "tags.h"

//
// User-accessible file operations
//

static const struct luaL_reg file_clib[] =
{
    { "write", CLua::file_write },
    { nullptr, nullptr },
};

void cluaopen_file(lua_State *ls)
{
    luaL_openlib(ls, "file", file_clib, 0);
}

//
// Non-user-accessible file operations
//

static int file_marshall(lua_State *ls)
{
    if (lua_gettop(ls) != 2)
        luaL_error(ls, "Need two arguments: tag header and value");
    writer &th(*static_cast<writer*>(lua_touserdata(ls, 1)));
    ASSERT(!lua_isfunction(ls, 2));
    if (lua_isnumber(ls, 2))
        marshallInt(th, luaL_safe_checklong(ls, 2));
    else if (lua_isboolean(ls, 2))
        marshallByte(th, lua_toboolean(ls, 2));
    else if (lua_isstring(ls, 2))
        marshallString(th, lua_tostring(ls, 2));
    return 0;
}

static int file_minor_version(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>(lua_touserdata(ls, 1)));
    lua_pushnumber(ls, th.getMinorVersion());
    return 1;
}

static int file_major_version(lua_State *ls)
{
    lua_pushnumber(ls, TAG_MAJOR_VERSION);
    return 1;
}

static int file_unmarshall_boolean(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>(lua_touserdata(ls, 1)));
    lua_pushboolean(ls, unmarshallByte(th));
    return 1;
}

static int file_unmarshall_number(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>(lua_touserdata(ls, 1)));
    lua_pushnumber(ls, unmarshallInt(th));
    return 1;
}

static int file_unmarshall_string(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>(lua_touserdata(ls, 1)));
    lua_pushstring(ls, unmarshallString(th).c_str());
    return 1;
}

enum class lua_persist
{
    none,
    number,
    string,
    // [ds] No longer supported for save portability:
    function,
    nil,
    boolean,
};

static int file_marshall_meta(lua_State *ls)
{
    if (lua_gettop(ls) != 2)
        luaL_error(ls, "Need two arguments: tag header and value");

    writer &th(*static_cast<writer*>(lua_touserdata(ls, 1)));

    auto ptype = lua_persist::none;
    if (lua_isnumber(ls, 2))
        ptype = lua_persist::number;
    else if (lua_isboolean(ls, 2))
        ptype = lua_persist::boolean;
    else if (lua_isstring(ls, 2))
        ptype = lua_persist::string;
    else if (lua_isnil(ls, 2))
        ptype = lua_persist::nil;
    else
        luaL_error(ls,
                   make_stringf("Cannot marshall %s",
                                lua_typename(ls, lua_type(ls, 2))).c_str());
    marshallByte(th, static_cast<int8_t>(ptype));
    if (ptype != lua_persist::nil)
        file_marshall(ls);
    return 0;
}

static int file_unmarshall_meta(lua_State *ls)
{
    reader &th(*static_cast<reader*>(lua_touserdata(ls, 1)));
    auto ptype = static_cast<lua_persist>(unmarshallByte(th));
    switch (ptype)
    {
        case lua_persist::boolean:
            return file_unmarshall_boolean(ls);
        case lua_persist::number:
            return file_unmarshall_number(ls);
        case lua_persist::string:
            return file_unmarshall_string(ls);
        case lua_persist::nil:
            lua_pushnil(ls);
            return 1;
        default:
            luaL_error(ls, "Unexpected type signature.");
    }
    // Never get here.
    return 0;
}

// Returns a Lua table of filenames in the named directory. The file names
// returned are unqualified. The directory must be a relative path, and will
// be resolved to an absolute path if necessary using datafile_path.
LUAFN(_file_datadir_files_recursive)
{
    const string rawdir(luaL_checkstring(ls, 1));
    // A filename suffix to match (such as ".des"). If empty, files
    // will be unfiltered.
    const string ext_filter(lua_isnoneornil(ls, 2) ? ""
                                                   : luaL_checkstring(ls, 2));
    const string datadir(datafile_path(rawdir, false, false, dir_exists));

    if (datadir.empty())
        luaL_error(ls, "Cannot find data directory: '%s'", rawdir.c_str());

    const vector<string> files = get_dir_files_recursive(datadir, ext_filter);
    return clua_stringtable(ls, files);
}

// Returns a Lua table of filenames in the named directory. The file names
// returned are unqualified. The directory must be a relative path, and will
// be resolved to an absolute path if necessary using datafile_path.
LUAFN(_file_datadir_files)
{
    const string rawdir(luaL_checkstring(ls, 1));
    // A filename suffix to match (such as ".des"). If empty, files
    // will be unfiltered.
    const string ext_filter(lua_isnoneornil(ls, 2) ? ""
                                                   : luaL_checkstring(ls, 2));
    const string datadir(datafile_path(rawdir, false, false, dir_exists));

    if (datadir.empty())
        luaL_error(ls, "Cannot find data directory: '%s'", rawdir.c_str());

    const vector<string> files = ext_filter.empty()
                                 ? get_dir_files_sorted(datadir)
                                 : get_dir_files_ext(datadir, ext_filter);
    return clua_stringtable(ls, files);
}

LUAFN(_file_writefile)
{
    const string fname(luaL_checkstring(ls, 1));
    FILE *f = fopen_replace(fname.c_str());
    if (f)
    {
        fprintf(f, "%s", luaL_checkstring(ls, 2));
        fclose(f);
        lua_pushboolean(ls, true);
    }
    else
        lua_pushboolean(ls, false);
    return 1;
}

static const struct luaL_reg file_dlib[] =
{
    { "marshall",   file_marshall },
    { "marshall_meta", file_marshall_meta },
    { "unmarshall_meta", file_unmarshall_meta },
    { "unmarshall_boolean", file_unmarshall_boolean },
    { "unmarshall_number", file_unmarshall_number },
    { "unmarshall_string", file_unmarshall_string },
    { "writefile", _file_writefile },
    { "datadir_files", _file_datadir_files },
    { "datadir_files_recursive", _file_datadir_files_recursive },
    { "minor_version", file_minor_version },
    { "major_version", file_major_version },
    { nullptr, nullptr }
};

void dluaopen_file(lua_State *ls)
{
    luaL_openlib(ls, "file", file_dlib, 0);
}
