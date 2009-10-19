#include "AppHdr.h"

#include "dlua.h"
#include "l_libs.h"

#include "tags.h"

static int file_marshall(lua_State *ls)
{
    if (lua_gettop(ls) != 2)
        luaL_error(ls, "Need two arguments: tag header and value");
    writer &th(*static_cast<writer*>( lua_touserdata(ls, 1) ));
    if (lua_isnumber(ls, 2))
        marshallLong(th, luaL_checklong(ls, 2));
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
    reader &th(*static_cast<reader*>( lua_touserdata(ls, 1) ));
    lua_pushboolean(ls, unmarshallByte(th));
    return (1);
}

static int file_unmarshall_number(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>( lua_touserdata(ls, 1) ));
    lua_pushnumber(ls, unmarshallLong(th));
    return (1);
}

static int file_unmarshall_string(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>( lua_touserdata(ls, 1) ));
    lua_pushstring(ls, unmarshallString(th).c_str());
    return (1);
}

static int file_unmarshall_fn(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>( lua_touserdata(ls, 1) ));
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
    LPT_BOOLEAN
};

static int file_marshall_meta(lua_State *ls)
{
    if (lua_gettop(ls) != 2)
        luaL_error(ls, "Need two arguments: tag header and value");
    
    writer &th(*static_cast<writer*>( lua_touserdata(ls, 1) ));
    
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
    reader &th(*static_cast<reader*>( lua_touserdata(ls, 1) ));
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

const struct luaL_reg file_lib[] =
{
{ "marshall",   file_marshall },
{ "marshall_meta", file_marshall_meta },
{ "unmarshall_meta", file_unmarshall_meta },
{ "unmarshall_number", file_unmarshall_number },
{ "unmarshall_string", file_unmarshall_string },
{ "unmarshall_fn", file_unmarshall_fn },
{ NULL, NULL }
};

