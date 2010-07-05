/*
 * File:       l_dgnbld.cc
 * Summary:    Building routines (library "dgn").
 */

#include "AppHdr.h"

#include <cmath>

#include "cluautil.h"
#include "coord.h"
#include "coordit.h"
#include "dungeon.h"
#include "l_libs.h"
#include "mapdef.h"
#include "random.h"

static const char *traversable_glyphs =
    ".+=w@{}()[]<>BC^~TUVY$%*|Odefghijk0123456789";

static const char *exit_glyphs = "{}()[]<>@";

// Return the integer stored in the table (on the stack) with the key name.
// If the key doesn't exist or the value is the wrong type, return defval.
static int _table_int(lua_State *ls, int idx, const char *name, int defval)
{
    if (!lua_istable(ls, idx))
        return defval;
    lua_pushstring(ls, name);
    lua_gettable(ls, idx < 0 ? idx - 1 : idx);
    bool nil = lua_isnil(ls, idx);
    bool valid = lua_isnumber(ls, idx);
    if (!nil && !valid)
        luaL_error(ls, "'%s' in table, but not an int.", name);
    int ret = (!nil && valid ? luaL_checkint(ls, idx) : defval);
    lua_pop(ls, 1);
    return (ret);
}

// Return the character stored in the table (on the stack) with the key name.
// If the key doesn't exist or the value is the wrong type, return defval.
static char _table_char(lua_State *ls, int idx, const char *name, char defval)
{
    if (!lua_istable(ls, idx))
        return defval;
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
    if (!lua_istable(ls, idx))
        return defval;
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
    if (!lua_istable(ls, idx))
        return defval;
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

// These macros all assume the table is on the top of the lua stack.
#define TABLE_INT(ls, val, def) int val = _table_int(ls, -1, #val, def);
#define TABLE_CHAR(ls, val, def) char val = _table_char(ls, -1, #val, def);
#define TABLE_STR(ls, val, def) const char *val = _table_str(ls, -1, #val, def);
#define TABLE_BOOL(ls, val, def) bool val = _table_bool(ls, -1, #val, def);

// Read a set of box coords (x1, y1, x2, y2) from the table.
// Return true if coords are valid.
static bool _coords(lua_State *ls, map_lines &lines,
                    int &x1, int &y1, int &x2, int &y2, int border = 0)
{
    const int idx = -1;
    x1 = _table_int(ls, idx, "x1", 0);
    y1 = _table_int(ls, idx, "y1", 0);
    x2 = _table_int(ls, idx, "x2", lines.width() - 1);
    y2 = _table_int(ls, idx, "y2", lines.height() - 1);

    if (x2 < x1)
        std::swap(x1, x2);
    if (y2 < y1)
        std::swap(y1, y2);

    return (x1 + border <= x2 - border && y1 + border <= y2 - border);
}

// Check if a given coordiante is valid for lines.
static bool _valid_coord(lua_State *ls, map_lines &lines, int x, int y, bool error = true)
{
    if (x < 0 || x >= lines.width())
    {
        if (error)
            luaL_error(ls, "Invalid x coordinate: %d", x);
        return false;
    }

    if (y < 0 || y >= lines.height())
    {
        if (error)
            luaL_error(ls, "Invalid y coordinate: %d", y);
        return false;
    }

    return true;
}

// Does what fill_area did, but here, so that it can be used through
// multiple functions (including make_box).
static int _fill_area (lua_State *ls, map_lines &lines, int x1, int y1, int x2, int y2, char fill)
{
    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
        {
            lines(x, y) = fill;
        }

    return (0);
}

// Specifically only deals with horizontal lines.
static std::vector<coord_def> _box_side (int x1, int y1, int x2, int y2, int side)
{
    std::vector<coord_def> line;

    int start_x, start_y, stop_x, stop_y, x, y;

    switch (side)
    {
    case 0: start_x = x1; start_y = y1; stop_x = x2; stop_y = y1; break;
    case 1: start_x = x2; start_y = y1; stop_x = x2; stop_y = y2; break;
    case 2: start_x = x1; start_y = y2; stop_x = x2; stop_y = y2; break;
    case 3: start_x = x1; start_y = y1; stop_x = x1; stop_y = y2; break;
    default: ASSERT(!"invalid _box_side"); return (line);
    }

    x = start_x; y = start_y;

    if (start_x == stop_x)
        for (y = start_y+1; y < stop_y; y++)
            line.push_back(coord_def(x, y));
    else
        for (x = start_x+1; x < stop_x; x++)
            line.push_back(coord_def(x, y));

    return (line);
}

// Does what count_passable_neighbors does, but in C++ form.
static int _count_passable_neighbors (lua_State *ls, map_lines &lines, int x,
                                      int y, const char *passable = traversable_glyphs)
{
    coord_def tl(x, y);
    int count = 0;

    for (adjacent_iterator ai(tl); ai; ++ai)
    {
        if (_valid_coord(ls, lines, ai->x, ai->y, false))
            if (strchr(passable, lines(*ai)))
                count++;
    }

    return (count);
}

static int _count_passable_neighbors (lua_State *ls, map_lines &lines, coord_def point,
                                      const char *passable = traversable_glyphs)
{
    return (_count_passable_neighbors(ls, lines, point.x, point.y, passable));
}


LUAFN(dgn_count_feature_in_box)
{
    LINES(ls, 1, lines);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return (0);

    TABLE_STR(ls, feat, "");

    coord_def tl(x1, y1);
    coord_def br(x2, y2);

    PLUARET(number, lines.count_feature_in_box(tl, br, feat));
}

LUAFN(dgn_count_antifeature_in_box)
{
    LINES(ls, 1, lines);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return (0);

    TABLE_STR(ls, feat, "");

    coord_def tl(x1, y1);
    coord_def br(x2, y2);

    int sum = (br.x - tl.x + 1) * (br.y - tl.y + 1);
    PLUARET(number, sum - lines.count_feature_in_box(tl, br, feat));
}

LUAFN(dgn_count_neighbors)
{
    LINES(ls, 1, lines);

    TABLE_STR(ls, feat, "");
    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);

    if (!_valid_coord(ls, lines, x, y))
        return (0);

    coord_def tl(x-1, y-1);
    coord_def br(x+1, y+1);

    PLUARET(number, lines.count_feature_in_box(tl, br, feat));
}

LUAFN(dgn_count_passable_neighbors)
{
    LINES(ls, 1, lines);

    TABLE_STR(ls, passable, traversable_glyphs);
    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);

    if (!_valid_coord(ls, lines, x, y))
    {
        lua_pushnumber(ls, 0);
        return (1);
    }

    lua_pushnumber(ls, _count_passable_neighbors(ls, lines, x, y, passable));
    return (1);
}


LUAFN(dgn_is_valid_coord)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);

    if (x < 0 || x >= lines.width())
    {
        lua_pushboolean(ls, false);
        return (1);
    }

    if (y < 0 || y >= lines.height())
    {
        lua_pushboolean(ls, false);
        return (1);
    }

    lua_pushboolean(ls, true);
    return (1);
}

LUAFN(dgn_extend_map)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, height, 1);
    TABLE_INT(ls, width, 1);
    TABLE_CHAR(ls, fill, 'x');

    lines.extend(width, height, fill);

    return (0);
}

LUAFN(dgn_fill_area)
{
    LINES(ls, 1, lines);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return (0);

    TABLE_CHAR(ls, fill, 'x');

    _fill_area (ls, lines, x1, y1, x2, y2, fill);

    return (0);
}

LUAFN(dgn_fill_disconnected)
{
    LINES(ls, 1, lines);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return (0);

    TABLE_CHAR(ls, fill, 'x');
    TABLE_STR(ls, passable, traversable_glyphs);
    TABLE_STR(ls, wanted, exit_glyphs);

    coord_def tl(x1, y1);
    coord_def br(x2, y2);

    travel_distance_grid_t tpd;
    memset(tpd, 0, sizeof(tpd));

    int nzones = 0;
    for (rectangle_iterator ri(tl, br); ri; ++ri)
    {
        const coord_def c = *ri;
        if (tpd[c.x][c.y] || passable && !strchr(passable, lines(c)))
            continue;

        if (lines.fill_zone(tpd, c, tl, br, ++nzones, wanted, passable))
            continue;

        // If wanted wasn't found, fill every passable square that
        // we just found with the 'fill' glyph.
        for (rectangle_iterator f(tl, br); f; ++f)
        {
            const coord_def fc = *f;
            if (tpd[fc.x][fc.y] == nzones)
                lines(fc) = fill;
        }
    }

    return (0);
}

LUAFN(dgn_is_passable_coord)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);
    TABLE_STR(ls, passable, traversable_glyphs);

    if (!_valid_coord(ls, lines, x, y))
        return (0);

    if (strchr(passable, lines(x, y)))
        lua_pushboolean(ls, true);
    else
        lua_pushboolean(ls, false);

    return (1);
}

LUAFN(dgn_find_in_area)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x1, -1);
    TABLE_INT(ls, y1, -1);
    TABLE_INT(ls, x2, -1);
    TABLE_INT(ls, y2, -1);

    if (!_coords(ls, lines, x1, y1, x2, y2))
        return (0);

    TABLE_CHAR(ls, find, 'x');

    int x, y;

    for (x = x1; x <= x2; x++)
        for (y = y1; y <= y2; y++)
            if (lines(x, y) == find)
            {
                lua_pushboolean(ls, true);
                return (1);
            }

    lua_pushboolean(ls, false);
    return (1);
}

LUAFN(dgn_height)
{
    LINES(ls, 1, lines);
    PLUARET(number, lines.height());
}

LUAFN(dgn_join_the_dots)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x1, -1);
    TABLE_INT(ls, y1, -1);
    TABLE_INT(ls, x2, -1);
    TABLE_INT(ls, y2, -1);
    TABLE_STR(ls, passable, traversable_glyphs);
    TABLE_CHAR(ls, fill, '.');

    if (!_valid_coord(ls, lines, x1, y1))
        return (0);
    if (!_valid_coord(ls, lines, x2, y2))
        return (0);

    coord_def from(x1, y1);
    coord_def to(x2, y2);

    if (from == to)
        return (0);

    coord_def at = from;
    do
    {
        char glyph = lines(at);

        if (!strchr(passable, glyph))
            lines(at) = fill;

        if (at == to)
            break;

        if (at.x < to.x)
        {
            at.x++;
            continue;
        }

        if (at.x > to.x)
        {
            at.x--;
            continue;
        }

        if (at.y > to.y)
        {
            at.y--;
            continue;
        }

        if (at.y < to.y)
        {
            at.y++;
            continue;
        }
    }
    while (true);

    return (0);
}

LUAFN(dgn_make_circle)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);
    TABLE_INT(ls, radius, 1);
    TABLE_CHAR(ls, fill, 'x');

    if (!_valid_coord(ls, lines, x, y))
        return (0);

    for (int ry = -radius; ry <= radius; ++ry)
        for (int rx = -radius; rx <= radius; ++rx)
            if (rx * rx + ry * ry < radius * radius)
                lines(x + rx, y + ry) = fill;

    return (0);
}

LUAFN(dgn_make_diamond)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);
    TABLE_INT(ls, radius, 1);
    TABLE_CHAR(ls, fill, 'x');

    if (!_valid_coord(ls, lines, x, y))
        return (0);

    for (int ry = -radius; ry <= radius; ++ry)
        for (int rx = -radius; rx <= radius; ++rx)
            if (std::abs(rx) + std::abs(ry) <= radius)
                lines(x + rx, y + ry) = fill;

    return (0);
}

LUAFN(dgn_make_rounded_square)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);
    TABLE_INT(ls, radius, 1);
    TABLE_CHAR(ls, fill, 'x');

    if (!_valid_coord(ls, lines, x, y))
        return (0);

    for (int ry = -radius; ry <= radius; ++ry)
        for (int rx = -radius; rx <= radius; ++rx)
            if (std::abs(rx) != radius || std::abs(ry) != radius)
                lines(x + rx, y + ry) = fill;

    return (0);
}

LUAFN(dgn_make_square)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);
    TABLE_INT(ls, radius, 1);
    TABLE_CHAR(ls, fill, 'x');

    if (!_valid_coord(ls, lines, x, y))
        return (0);

    for (int ry = -radius; ry <= radius; ++ry)
        for (int rx = -radius; rx <= radius; ++rx)
            lines(x + rx, y + ry) = fill;

    return (0);
}

LUAFN(dgn_make_box)
{
    LINES(ls, 1, lines);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return (0);

    TABLE_CHAR(ls, floor, '.');
    TABLE_CHAR(ls, wall, 'x');
    TABLE_INT(ls, width, 1);

    _fill_area(ls, lines, x1, y1, x2, y2, wall);
    _fill_area(ls, lines, x1+width, y1+width, x2-width, y2-width, floor);

    return (0);
}

LUAFN(dgn_make_box_doors)
{
    LINES(ls, 1, lines);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return (0);

    TABLE_INT(ls, number, 1);

    int sides[4] = {0, 0, 0, 0};

    int door_count;

    for (door_count = 0; door_count < number; door_count++)
    {
        int current_side = random2(4);
        if (sides[current_side] >= 2)
            current_side = random2(4);

        std::vector<coord_def> points = _box_side(x1, y1, x2, y2, current_side);

        int total_points = int(points.size());

        int index = random2avg(total_points, 2 + random2(number));

        int tries = 50;

        while (_count_passable_neighbors(ls, lines, points[index]) <= 3)
        {
            tries--;
            index = random2(total_points);

            if (tries == 0)
                break;
        }

        if (tries == 0)
        {
            door_count--;
            continue;
        }

        sides[current_side]++;
        lines(points[index]) = '+';
    }

    lua_pushnumber(ls, door_count);
    return (1);
}

// Return a metatable for a point on the map_lines grid.
LUAFN(dgn_mapgrd_table)
{
    MAP(ls, 1, map);

    map_def **mapref = clua_new_userdata<map_def *>(ls, MAPGRD_METATABLE);
    *mapref = map;

    return (1);
}

LUAFN(dgn_octa_room)
{
    LINES(ls, 1, lines);

    int default_oblique = std::min(lines.width(), lines.height()) / 2 - 1;
    TABLE_INT(ls, oblique, default_oblique);
    TABLE_CHAR(ls, outside, 'x');
    TABLE_CHAR(ls, inside, '.');
    TABLE_STR(ls, replace, "");

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return (0);

    coord_def tl(x1, y1);
    coord_def br(x2, y2);

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

    return (0);
}

LUAFN(dgn_replace_area)
{
    LINES(ls, 1, lines);

    TABLE_STR(ls, find, '\0');
    TABLE_CHAR(ls, replace, '\0');

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return (0);

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            if (strchr(find, lines(x, y)))
                lines(x, y) = replace;

    return (0);
}

LUAFN(dgn_replace_first)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, 0);
    TABLE_INT(ls, y, 0);
    TABLE_INT(ls, xdir, 2);
    TABLE_INT(ls, ydir, 2);
    TABLE_CHAR(ls, find, '\0');
    TABLE_CHAR(ls, replace, '\0');
    TABLE_BOOL(ls, required, false);

    if (!_valid_coord(ls, lines, x, y))
        return (0);

    if (xdir < -1 || xdir > 1)
    {
        return (luaL_error(ls, "Invalid xdir: %d", xdir));
    }

    if (ydir < -1 || ydir > 1)
    {
        return (luaL_error(ls, "Invalid ydir: %d", ydir));
    }

    while (lines.in_bounds(coord_def(x, y)))
    {
        if (lines(x, y) == find)
        {
            lines(x, y) = replace;
            return (0);
        }

        x += xdir;
        y += ydir;
    }

    if (required)
        return (luaL_error(ls, "Could not find feature '%c' to replace", find));

    return (0);
}

LUAFN(dgn_replace_random)
{
    LINES(ls, 1, lines);

    TABLE_CHAR(ls, find, '\0');
    TABLE_CHAR(ls, replace, '\0');
    TABLE_BOOL(ls, required, false);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return (0);

    int count = (x2 - x1) * (y2 - y1);
    if (!count)
    {
        if (required)
            luaL_error(ls, "%s", "No elements to replace");
        return (0);
    }

    std::vector<coord_def> loc;
    loc.reserve(count);

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            if (lines(x, y) == find)
                loc.push_back(coord_def(x, y));

    if (!loc.size())
    {
        if (required)
            return (luaL_error(ls, "Could not find '%c'", find));
    }

    int idx = random2(loc.size());
    lines(loc[idx]) = replace;

    return (0);
}

LUAFN(dgn_smear_map)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, iterations, 1);
    TABLE_CHAR(ls, smear, 'x');
    TABLE_STR(ls, onto, ".");
    TABLE_BOOL(ls, boxy, false);

    const int border = 1;
    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2, border))
        return (0);

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
                if (sanity++ > max_sanity)
                    return (0);

                mc.x = random_range(x1+border, y2-border);
                mc.y = random_range(y1+border, y2-border);
            }
            while (onto[0] && !strchr(onto, lines(mc)));

            // Is there a "smear" feature along the diagonal from mc?
            diagonals = lines(mc.x + 1, mc.y + 1) == smear
                        || lines(mc.x - 1, mc.y + 1) == smear
                        || lines(mc.x - 1, mc.y - 1) == smear
                        || lines(mc.x + 1, mc.y - 1) == smear;

            // Is there a "smear" feature up, down, left, or right from mc?
            straights = lines(mc.x + 1, mc.y) == smear
                        || lines(mc.x - 1, mc.y) == smear
                        || lines(mc.x, mc.y + 1) == smear
                        || lines(mc.x, mc.y - 1) == smear;
        }
        while (!straights && (boxy || !diagonals));

        lines(mc) = smear;
    }

    return (0);
}

LUAFN(dgn_spotty_map)
{
    LINES(ls, 1, lines);

    TABLE_STR(ls, replace, "x");
    TABLE_CHAR(ls, fill, '.');
    TABLE_BOOL(ls, boxy, true);
    TABLE_INT(ls, iterations, random2(boxy ? 750 : 1500));

    const int border = 4;
    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2, border))
        return (0);

    const int max_test_per_iteration = 10;
    int sanity = 0;
    int max_sanity = iterations * max_test_per_iteration;

    for (int i = 0; i < iterations; i++)
    {
        int x, y;
        do
        {
            if (sanity++ > max_sanity)
                return (0);

            x = random_range(x1 + border, x2 - border);
            y = random_range(y1 + border, y2 - border);
        }
        while (strchr(replace, lines(x, y))
               && strchr(replace, lines(x-1, y))
               && strchr(replace, lines(x+1, y))
               && strchr(replace, lines(x, y-1))
               && strchr(replace, lines(x, y+1))
               && strchr(replace, lines(x-2, y))
               && strchr(replace, lines(x+2, y))
               && strchr(replace, lines(x, y-2))
               && strchr(replace, lines(x, y+2)));

        if (strchr(replace, lines(x, y)))
            lines(x, y) = fill;
        if (strchr(replace, lines(x, y-1)))
            lines(x, y-1) = fill;
        if (strchr(replace, lines(x, y+1)))
            lines(x, y+1) = fill;
        if (strchr(replace, lines(x-1, y)))
            lines(x-1, y) = fill;
        if (strchr(replace, lines(x+1, y)))
            lines(x+1, y) = fill;

        if (boxy)
        {
            if (strchr(replace, lines(x-1, y-1)))
                lines(x-1, y-1) = fill;
            if (strchr(replace, lines(x+1, y+1)))
                lines(x+1, y+1) = fill;
            if (strchr(replace, lines(x-1, y+1)))
                lines(x-1, y+1) = fill;
            if (strchr(replace, lines(x+1, y-1)))
                lines(x+1, y-1) = fill;
        }
    }

    return (0);
}

static int dgn_width(lua_State *ls)
{
    LINES(ls, 1, lines);
    PLUARET(number, lines.width());
}

const struct luaL_reg dgn_build_dlib[] =
{
    { "count_feature_in_box", &dgn_count_feature_in_box },
    { "count_antifeature_in_box", &dgn_count_antifeature_in_box },
    { "count_neighbors", &dgn_count_neighbors },
    { "count_passable_neighbors", &dgn_count_passable_neighbors },
    { "is_valid_coord", &dgn_is_valid_coord },
    { "is_passable_coord", &dgn_is_passable_coord },
    { "extend_map", &dgn_extend_map },
    { "fill_area", &dgn_fill_area },
    { "fill_disconnected", &dgn_fill_disconnected },
    { "find_in_area", &dgn_find_in_area },
    { "height", dgn_height },
    { "join_the_dots", &dgn_join_the_dots },
    { "make_circle", &dgn_make_circle },
    { "make_diamond", &dgn_make_diamond },
    { "make_rounded_square", &dgn_make_rounded_square },
    { "make_square", &dgn_make_square },
    { "make_box", &dgn_make_box },
    { "make_box_doors", &dgn_make_box_doors },
    { "mapgrd_table", dgn_mapgrd_table },
    { "octa_room", &dgn_octa_room },
    { "replace_area", &dgn_replace_area },
    { "replace_first", &dgn_replace_first },
    { "replace_random", &dgn_replace_random },
    { "smear_map", &dgn_smear_map },
    { "spotty_map", &dgn_spotty_map },
    { "width", dgn_width },

    { NULL, NULL }
};
