#include "AppHdr.h"

#include "dlua.h"
#include "l_libs.h"
#include "initfile.h"
#include "itemname.h"
#include "stuff.h"
#include "view.h"

#ifdef UNIX
#include <sys/time.h>
#include <time.h>
#endif

LUAFN(_crawl_args)
{
    return dlua_stringtable(ls, SysEnv.cmd_args);
}

LUAFN(_crawl_milestone)
{
#ifdef DGL_MILESTONES
    mark_milestone(luaL_checkstring(ls, 1),
                   luaL_checkstring(ls, 2));
#endif
    return (0);
}

LUAFN(_crawl_redraw_view)
{
    viewwindow(true, false);
    return (0);
}

#ifdef UNIX
LUAFN(_crawl_millis)
{
    struct timeval tv;
    struct timezone tz;
    const int error = gettimeofday(&tv, &tz);
    if (error)
        luaL_error(ls, make_stringf("Failed to get time: %s",
                                    strerror(error)).c_str());
    
    lua_pushnumber(ls, tv.tv_sec * 1000 + tv.tv_usec / 1000);
    return (1);
}
#endif

std::string _crawl_make_name (lua_State *ls)
{
    // A quick wrapper around itemname:make_name. Seed is random_int().
    // Possible parameters: all caps, max length, char start. By default
    // these are false, -1, and 0 as per make_name.
    if (lua_gettop(ls) == 0) 
        return make_name (random_int(), false);
    else
    {
        bool all_caps (false);
        int maxlen = -1;
        char start = 0;
        if (lua_gettop(ls) >= 1 && lua_isboolean(ls, 1)) // Want all caps. 
            all_caps = lua_toboolean(ls, 1);
        if (lua_gettop(ls) >= 2 && lua_isnumber(ls, 2))  // Specified a maxlen.
            maxlen = lua_tonumber(ls, 2);
        if (lua_gettop(ls) >= 3 && lua_isstring(ls, 3))  // Specificied a start character
            start = lua_tostring(ls, 3)[0];
        return make_name (random_int(), all_caps, maxlen, start);
    }
}

LUARET1(crawl_make_name, string, _crawl_make_name(ls).c_str())

const struct luaL_reg crawl_lib[] =
{
{ "args", _crawl_args },
{ "mark_milestone", _crawl_milestone },
{ "redraw_view", _crawl_redraw_view },
#ifdef UNIX
{ "millis", _crawl_millis },
#endif
{ "make_name", crawl_make_name },
{ NULL, NULL }
};

