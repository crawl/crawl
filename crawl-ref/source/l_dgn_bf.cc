/*
 * File:      l_dgnbf.cc
 * Summary:   Dungeon lua builder functions.
 */

#include "AppHdr.h"

#include "dlua.h"
#include "l_libs.h"

#include "mapdef.h"

// Return the integer stored in the table (on the stack) with the key name.
// If the key doesn't exist or the value is the wrong type, return defval.
static int _table_int(lua_State *ls, int idx, const char *name, int defval)
{
    lua_pushstring(ls, name);
    lua_gettable(ls, idx < 0 ? idx - 1 : idx);
    bool nil = lua_isnil(ls, idx);
    bool valid = lua_isnumber(ls, idx);
    if (!nil && !valid)
        luaL_error(ls, "'%s' in table, but not an int.", name);
    int ret = (!nil && valid ? lua_tonumber(ls, idx) : defval);
    lua_pop(ls, 1);
    return (ret);
}

// Return the character stored in the table (on the stack) with the key name.
// If the key doesn't exist or the value is the wrong type, return defval.
static char _table_char(lua_State *ls, int idx, const char *name, char defval)
{
    lua_pushstring(ls, name);
    lua_gettable(ls, idx < 0 ? idx - 1 : idx);
    bool nil = lua_isnil(ls, idx);
    bool valid = lua_isstring(ls, idx);
    if (!nil && !valid)
        luaL_error(ls, "'%s' in table, but not a string.", name);

    char ret = defval;
    if (!nil && valid)
    {
        const char *str = lua_tostring(ls, idx);
        if (str[0] && !str[1])
            ret = str[0];
        else
            luaL_error(ls, "'%s' has more than one character.", name);
    }
    lua_pop(ls, 1);
    return (ret);
}

// Return the string stored in the table (on the stack) with the key name.
// If the key doesn't exist or the value is the wrong type, return defval.
static const char* _table_str(lua_State *ls, int idx, const char *name, const char *defval)
{
    lua_pushstring(ls, name);
    lua_gettable(ls, idx < 0 ? idx - 1 : idx);
    bool nil = lua_isnil(ls, idx);
    bool valid = lua_isstring(ls, idx);
    if (!nil && !valid)
        luaL_error(ls, "'%s' in table, but not a string.", name);
    const char *ret = (!nil && valid ? lua_tostring(ls, idx) : defval);
    lua_pop(ls, 1);
    return (ret);
}

// Return the boolean stored in the table (on the stack) with the key name.
// If the key doesn't exist or the value is the wrong type, return defval.
static bool _table_bool(lua_State *ls, int idx, const char *name, bool defval)
{
    lua_pushstring(ls, name);
    lua_gettable(ls, idx < 0 ? idx - 1 : idx);
    bool nil = lua_isnil(ls, idx);
    bool valid = lua_isboolean(ls, idx);
    if (!nil && !valid)
        luaL_error(ls, "'%s' in table, but not a bool.", name);
    bool ret = (!nil && valid ? lua_toboolean(ls, idx) : defval);
    lua_pop(ls, 1);
    return (ret);
}

#define BF_INT(ls, val, def) int val = _table_int(ls, -1, #val, def);
#define BF_CHAR(ls, val, def) char val = _table_char(ls, -1, #val, def);
#define BF_STR(ls, val, def) const char *val = _table_str(ls, -1, #val, def);
#define BF_BOOL(ls, val, def) bool val = _table_bool(ls, -1, #val, def);

static void bf_octa_room(lua_State *ls, map_lines &lines)
{
    int default_oblique = std::min(lines.width(), lines.height()) / 2 - 1;
    BF_INT(ls, oblique, default_oblique);
    BF_CHAR(ls, outside, 'x');
    BF_CHAR(ls, inside, '.');
    BF_STR(ls, replace, ".");

    coord_def tl, br;
    if (!lines.find_bounds(replace, tl, br))
        return;

    for (rectangle_iterator ri(tl, br); ri; ++ri)
    {
        const coord_def mc = *ri;
        char glyph = lines(mc);
        if (replace[0] && !strchr(replace, glyph))
            continue;

        int ob = 0;
        ob += std::max(oblique + tl.x - mc.x, 0);
        ob += std::max(oblique + mc.x - br.x, 0);

        bool is_inside = (mc.y >= tl.y + ob && mc.y <= br.y - ob);
        lines(mc) = is_inside ? inside : outside;
    }
}

static void bf_smear(lua_State *ls, map_lines &lines)
{
    BF_INT(ls, iterations, 1);
    BF_CHAR(ls, smear, 'x');
    BF_STR(ls, onto, ".");
    BF_BOOL(ls, boxy, false);

    const int max_test_per_iteration = 10;
    int sanity = 0;
    int max_sanity = iterations * max_test_per_iteration;

    for (int i = 0; i < iterations; i++)
    {
        bool diagonals, straights;
        coord_def mc;

        do
        {
            do
            {
                sanity++;
                mc.x = random_range(1, lines.width() - 2);
                mc.y = random_range(1, lines.height() - 2);
            }
            while (onto[0] && !strchr(onto, lines(mc)));

            // Prevent too many iterations.
            if (sanity > max_sanity)
                return;

            // Is there a "smear" feature along the diagonal from mc?
            diagonals = lines(coord_def(mc.x+1, mc.y+1)) == smear ||
                        lines(coord_def(mc.x-1, mc.y+1)) == smear ||
                        lines(coord_def(mc.x-1, mc.y-1)) == smear ||
                        lines(coord_def(mc.x+1, mc.y-1)) == smear;

            // Is there a "smear" feature up, down, left, or right from mc?
            straights = lines(coord_def(mc.x+1, mc.y)) == smear ||
                        lines(coord_def(mc.x-1, mc.y)) == smear ||
                        lines(coord_def(mc.x, mc.y+1)) == smear ||
                        lines(coord_def(mc.x, mc.y-1)) == smear;
        }
        while (!straights && (boxy || !diagonals));

        lines(mc) = smear;
    }
}

static void bf_extend(lua_State *ls, map_lines &lines)
{
    BF_INT(ls, height, 1);
    BF_INT(ls, width, 1);
    BF_CHAR(ls, fill, 'x');

    lines.extend(width, height, fill);
}

typedef void (*bf_func)(lua_State *ls, map_lines &lines);
struct bf_entry
{
    const char* name;
    bf_func func;
};

// Create a separate list of builder funcs so that we can automatically
// generate a list of closures for them, rather than individually
// and explicitly exposing them to the dgn namespace.
static struct bf_entry bf_map[] =
{
    { "map_octa_room", &bf_octa_room },
    { "map_smear", &bf_smear },
    { "map_extend", &bf_extend }
};

static int dgn_call_builder_func(lua_State *ls)
{
    // This function gets called for all the builder functions that
    // operate on map_lines.

    MAP(ls, 1, map);
    if (!lua_istable(ls, 2) && !lua_isfunction(ls, 2))
        return luaL_argerror(ls, 2, "Expected table");

    bf_func *func = (bf_func *)lua_topointer(ls, lua_upvalueindex(1));
    if (!func)
        return luaL_error(ls, "Expected C function in closure upval");

    // Put the table on top.
    lua_settop(ls, 2);

    // Call the builder func itself.
    (*func)(ls, map->map);

    return (0);
}

void register_builder_funcs(lua_State *ls)
{
    lua_getglobal(ls, "dgn");

    const size_t num_entries = sizeof(bf_map) / sizeof(bf_entry);
    for (size_t i = 0; i < num_entries; i++)
    {
        // Push a closure with the C function into the dgn table.
        lua_pushlightuserdata(ls, &bf_map[i].func);
        lua_pushcclosure(ls, &dgn_call_builder_func, 1);
        lua_setfield(ls, -2, bf_map[i].name);
    }

    lua_pop(ls, 1);
}
