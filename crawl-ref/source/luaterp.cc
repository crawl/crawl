/**
 * @file
 * @brief An interactive lua interpreter.
 *
 * Based heavily on lua.c from the Lua distribution.
 *
 * Deficiencies:
 * - Currently requires defining a global function __echo
 *  to echo results.
 * - Currently requires prefixing with '=' if you want results echoed.
 * - Appears to require delay_message_clear = true.
 * - Old input lines are not visible.
**/

#include "AppHdr.h"

#include "luaterp.h"

#include "clua.h"
#include "dlua.h"
#include "message.h"
#include "options.h"

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
    char buffer[16384];
    char *b = buffer;
    size_t l;
    string prompt = firstline ? "> " : ". ";
    if (msgwin_get_line_autohist(prompt, buffer, sizeof(buffer)))
        return 0;
    l = strlen(b);

    // XXX: get line doesn't return newline?
    if (l > 0 && b[l-1] == '\n')   // line ends with newline?
        b[l-1] = '\0';             // remove it

    if (firstline && l > 0 && b[0] == '=')  // first line starts with `=' ?
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
                                     lua_strlen(ls, 1), "=repl");

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
    status = lua_pcall(ls, 0, LUA_MULTRET, 0);
    return status;
}

static int _report(lua_State *ls, int status)
{
    if (status && !lua_isnil(ls, -1))
    {
        const char *msg = lua_tostring(ls, -1);
        if (msg == nullptr)
            msg = "(error object is not a string)";
        mprf(MSGCH_ERROR, "%s", msg);
        lua_pop(ls, 1);
    }
    return status;
}

static bool _luaterp_running = false;

static void _run_dlua_interpreter(lua_State *ls)
{
    _luaterp_running = true;

    int status;
	mpr("[해석 프로그램을 종료하기 위해서는 ESC키를 누르시오.]");
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
                mprf(MSGCH_ERROR, "error calling __echo (%s)",
                                  lua_tostring(ls, -1));
            }
        }
    }
    lua_settop(ls, 0); // clear stack

    _luaterp_running = false;
}

bool luaterp_running()
{
    return _luaterp_running;
}

static bool _loaded_terp_files = false;

void debug_terp_dlua(CLua &vm)
{
    if (!_loaded_terp_files)
    {
        vm.execfile("dlua/debug.lua", false, false);
        for (const string &file : Options.terp_files)
        {
            vm.execfile(file.c_str(), false, false);
            if (!vm.error.empty())
                mprf(MSGCH_ERROR, "Lua error: %s", vm.error.c_str());
        }
        _loaded_terp_files = true;
    }
    _run_dlua_interpreter(vm);
}
