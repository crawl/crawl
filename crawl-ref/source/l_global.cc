#include "AppHdr.h"

#include "clua.h"
#include "l_libs.h"

//////////////////////////////////////////////////////////////////////
// Miscellaneous globals

#define PATTERN_FLUSH_CEILING 100

typedef std::map<std::string, text_pattern> pattern_map;
static pattern_map pattern_cache;

static text_pattern &get_text_pattern(const std::string &s, bool checkcase)
{
    pattern_map::iterator i = pattern_cache.find(s);
    if (i != pattern_cache.end())
        return i->second;

    if (pattern_cache.size() > PATTERN_FLUSH_CEILING)
        pattern_cache.clear();

    pattern_cache[s] = text_pattern(s, !checkcase);
    return (pattern_cache[s]);
}

static int lua_pmatch(lua_State *ls)
{
    const char *pattern = luaL_checkstring(ls, 1);
    if (!pattern)
        return (0);

    const char *text = luaL_checkstring(ls, 2);
    if (!text)
        return (0);

    bool checkcase = true;
    if (lua_isboolean(ls, 3))
        checkcase = lua_toboolean(ls, 3);

    text_pattern &tp = get_text_pattern(pattern, checkcase);
    lua_pushboolean(ls, tp.matches(text));
    return (1);
}

void cluaopen_globals(lua_State *ls)
{
    lua_pushcfunction(ls, lua_pmatch);
    lua_setglobal(ls, "pmatch");
}
