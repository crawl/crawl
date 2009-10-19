#include "AppHdr.h"

#include "dlua.h"
#include "l_libs.h"
#include "initfile.h"
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

const struct luaL_reg crawl_lib[] =
{
{ "args", _crawl_args },
{ "mark_milestone", _crawl_milestone },
{ "redraw_view", _crawl_redraw_view },
#ifdef UNIX
{ "millis", _crawl_millis },
#endif
{ NULL, NULL }
};

