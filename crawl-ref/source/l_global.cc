#include "AppHdr.h"

#include "l_libs.h"

#include "clua.h"
#include "libutil.h" // map_find

//////////////////////////////////////////////////////////////////////
// Miscellaneous globals

#define PATTERN_FLUSH_CEILING 100

typedef map<string, base_pattern*> pattern_map;
static pattern_map pattern_cache;

static base_pattern *get_text_pattern(const string &s, bool checkcase)
{
    if (base_pattern **pat = map_find(pattern_cache, s))
        return *pat;

    if (pattern_cache.size() > PATTERN_FLUSH_CEILING)
    {
        for (auto &kv : pattern_cache)
            delete kv.second;
        pattern_cache.clear();
    }

    if (s.length() > 0 && s[0] == '/')
    {
        string pattern(s);
        pattern.erase(0, 1);
        pattern_cache[s] = new text_pattern(pattern, !checkcase);
    }
    else
        pattern_cache[s] = new plaintext_pattern(s, !checkcase);
    return pattern_cache[s];
}

static int lua_pmatch(lua_State *ls)
{
    const char *pattern = luaL_checkstring(ls, 1);
    if (!pattern)
        return 0;

    const char *text = luaL_checkstring(ls, 2);
    if (!text)
        return 0;

    bool checkcase = true;
    if (lua_isboolean(ls, 3))
        checkcase = lua_toboolean(ls, 3);

    base_pattern *tp = get_text_pattern(pattern, checkcase);
    lua_pushboolean(ls, tp->matches(text));
    return 1;
}

void cluaopen_globals(lua_State *ls)
{
    lua_pushcfunction(ls, lua_pmatch);
    lua_setglobal(ls, "pmatch");
}
