#include "AppHdr.h"

#include "luaterp.h"

#include "cio.h"
#include "clua.h"
#include "dlua.h"

void _debug_call_lua(lua_State *ls)
{
    char specs[80];
    while (true)
    {
        mpr("> ", MSGCH_PROMPT);
        get_input_line(specs, sizeof(specs));
        std::string s = specs;

        if (s.empty())
            return;

        int err = 0;
        if (err = luaL_dostring(ls, s.c_str()))
        {
            mprf(MSGCH_ERROR, "lua error: %d", err);
        }
    }
}

void debug_call_dlua()
{
    _debug_call_lua(dlua);
}

