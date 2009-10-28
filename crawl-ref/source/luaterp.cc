/*
 * File:     luaterp.cc
 * Summary:  An interactive lua interpreter.
 *
 * Based heavily on lua.c from the Lua distribution.
 */

#include "AppHdr.h"

#include "luaterp.h"

#include "cio.h"
#include "clua.h"
#include "dlua.h"

static int _incomplete(lua_State *ls, int status)
{
    if (status == LUA_ERRSYNTAX)
    {
        size_t lmsg;
        const char *msg = lua_tolstring(ls, -1, &lmsg);
        const char *tp = msg + lmsg - (sizeof(LUA_QL("<eof>")) - 1);
        if (strstr(msg, LUA_QL("<eof>")) == tp)
        {
            lua_pop(ls, 1);
            return 1;
        }
    }
    return 0;
}

static int _pushline(lua_State *ls, int firstline)
{
    char buffer[80];
    char *b = buffer;
    size_t l;
    mpr(firstline ? "> " : ". ", MSGCH_PROMPT);
    get_input_line(buffer, sizeof(buffer));  
    l = strlen(b);
    if (l == 0)
        return (0);

    if (l > 0 && b[l-1] == '\n')   // line ends with newline?
        b[l-1] = '\0';             // remove it
    if (firstline && b[0] == '=')  // first line starts with `=' ?
        lua_pushfstring(ls, "return %s", b+1);  // change it to `return'
    else
        lua_pushstring(ls, b);
    return 1;
}

static int _loadline(lua_State *ls)
{
    int status;
    lua_settop(ls, 0);

    if (!_pushline(ls, 1))
        return -1;  /* no input */

    // repeat until gets a complete line
    for (;;)
    {
        status = luaL_loadbuffer(ls, lua_tostring(ls, 1),
                                     lua_strlen(ls, 1), "=terp");

        if (!_incomplete(ls, status)) 
            break;
        if (!_pushline(ls, 0))
            return -1;
        lua_pushliteral(ls, "\n");  // add a new line...
        lua_insert(ls, -2);         // ...between the two lines
        lua_concat(ls, 3);          // join them
    }
    lua_remove(ls, 1);              // remove line
    return status;
}

static int _docall(lua_State *ls)
{
    int status;
    int base = lua_gettop(ls);
    status = lua_pcall(ls, 0, LUA_MULTRET, base);
    if (status != 0)
        lua_gc(ls, LUA_GCCOLLECT, 0);
    return (status);
}

static int _report(lua_State *ls, int status)
{
    if (status && !lua_isnil(ls, -1))
    {
        const char *msg = lua_tostring(ls, -1);
        if (msg == NULL)
            msg = "(error object is not a string)";
        mprf(MSGCH_ERROR, msg);
        lua_pop(ls, 1);
    }
    return status;
}

void run_clua_interpreter(lua_State *ls)
{
    int status;
    while ((status = _loadline(ls)) != -1)
    {
        if (status == 0)
            status = _docall(ls);
        _report(ls, status);
        if (status == 0 && lua_gettop(ls) > 0)
        {
            lua_getglobal(ls, "__echo");
            lua_insert(ls, 1);
            if (lua_pcall(ls, lua_gettop(ls) - 1, 0, 0) != 0)
            {
                mprf(MSGCH_ERROR, "error calling crawl.mpr (%s)", 
                                  lua_tostring(ls, -1));
            }
//              mpr("got return values");
        }
    }
    lua_settop(ls, 0); // clear stack
}

void debug_call_dlua()
{
    run_clua_interpreter(dlua);
}

