/***
 * @module dgn
 */
#include "AppHdr.h"

#include "l-libs.h"

#include <cmath>
#include <vector>

#include "cluautil.h"
#include "coordit.h"
#include "dgn-delve.h"
#include "dgn-irregular-box.h"
#include "dgn-layouts.h"
#include "dgn-shoals.h"
#include "dgn-swamp.h"
#include "dungeon.h"

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
    int ret = (!nil && valid ? luaL_safe_checkint(ls, idx) : defval);
    lua_pop(ls, 1);
    return ret;
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
    return ret;
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
    return ret;
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
    return ret;
}

// These macros all assume the table is on the top of the lua stack.
#define TABLE_INT(ls, val, def) int val = _table_int(ls, -1, #val, def);
#define TABLE_CHAR(ls, val, def) char val = _table_char(ls, -1, #val, def);
#define TABLE_STR(ls, val, def) const char *val = _table_str(ls, -1, #val, def);
#define TABLE_BOOL(ls, val, def) bool val = _table_bool(ls, -1, #val, def);

#define ARG_INT(ls, num, val, def) int val = lua_isnone(ls, num) ? \
                                             def : luaL_safe_tointeger(ls, num)

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
        swap(x1, x2);
    if (y2 < y1)
        swap(y1, y2);

    return x1 + border <= x2 - border && y1 + border <= y2 - border;
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
static int _fill_area(lua_State *ls, map_lines &lines, int x1, int y1, int x2, int y2, char fill)
{
    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            lines(x, y) = fill;

    return 0;
}

static void _border_area(map_lines &lines, int x1, int y1, int x2, int y2, char border)
{
    for (int x = x1 + 1; x < x2; ++x)
        lines(x, y1) = border, lines(x, y2) = border;
    for (int y = y1; y <= y2; ++y)
        lines(x1, y) = border, lines(x2, y) = border;
}

// Does what count_passable_neighbors does, but in C++ form.
static int _count_passable_neighbors(lua_State *ls, map_lines &lines, int x,
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

    return count;
}

static vector<coord_def> _get_pool_seed_positions(
                                                vector<vector<int> > pool_index,
                                                int pool_size,
                                                int min_separation)
{
    const int NO_POOL   = 999997; // must match dgn_add_pools

    if (pool_size < 1)
        pool_size = 1;

    // 1. Find all floor positions

    vector<coord_def> floor_positions;

    for (unsigned int x = 0; x < pool_index.size(); x++)
        for (unsigned int y = 0; y < pool_index[x].size(); y++)
        {
            if (pool_index[x][y] == NO_POOL)
                floor_positions.emplace_back(x, y);
        }

    // 2. Choose the pool seed positions

    int min_separation_squared = min_separation * min_separation;
    int pool_count_goal = (floor_positions.size() + random2(pool_size))
                          / pool_size;

    vector<coord_def> seeds;

    for (int i = 0; i < pool_count_goal; i++)
    {
        if (floor_positions.empty())
        {
            // give up if no more positions
            break;
        }

        // choose a random position
        int chosen_index = random2(floor_positions.size());
        coord_def chosen_coord = floor_positions[chosen_index];
        floor_positions[chosen_index] = floor_positions.back();
        floor_positions.pop_back();

        // check if it is too close to another seed
        bool too_close = false;
        for (coord_def seed : seeds)
        {
            int diff_x = chosen_coord.x - seed.x;
            int diff_y = chosen_coord.y - seed.y;
            int distance_squared = diff_x * diff_x + diff_y * diff_y;

            if (distance_squared < min_separation_squared)
            {
                too_close = true;
                break;
            }
        }

        // if not too close, add it to the list
        if (!too_close)
            seeds.push_back(chosen_coord);
    }

    // 3. Return the pool seeds

    return seeds;
}

// This function assumes the table is on the top of the lua stack.
static vector<char> _pool_fill_glyphs_from_table(lua_State *ls,
                                                 const char *name)
{
    // We will go through the table and put each possible pool
    //  fill glyph in a vector once for each weight. This will
    //  make it easy to select random ones with the correct
    //  distribution when we need to.
    vector<char> fill_glyphs;

    lua_pushstring(ls, name);
    lua_gettable(ls, -2);
    if (!lua_isnil(ls, -1) && lua_istable(ls, -1))
    {
        // For some reason, LUA requires us to have a dummy
        //  value to remove from the stack whenever we do a
        //  table lookup. Here is the first one
        lua_pushnil(ls);

        // table is now at -2
        while (lua_next(ls, -2) != 0)
        {
            // uses 'key' (at index -2) and 'value' (at index -1)
            if (lua_type(ls, -2) == LUA_TSTRING
                && lua_type(ls, -1) == LUA_TNUMBER)
            {
                // we use first character of string as glyph
                char glyph = (lua_tostring(ls, -2))[0];

                int count = luaL_safe_tointeger(ls, -1);
                // sanity-check
                if (count > 10000)
                    count = 10000;

                if (glyph != '\0')
                {
                    for (int i = 0; i < count; i++)
                        fill_glyphs.push_back(glyph);
                }
            }

            // removes 'value'; keeps 'key' for next iteration
            lua_pop(ls, 1);
        }
    }
    lua_pop(ls, 1);

    // We might have not got anything, if so, use floor
    if (fill_glyphs.size() == 0)
        fill_glyphs.push_back('.');
    sort(fill_glyphs.begin(), fill_glyphs.end());

    return fill_glyphs;
}

// These functions check for irregularities before the first
//  corner along a wall in the indicated direction.
static bool _wall_is_empty(map_lines &lines,
                           int x, int y,
                           const char* wall, const char* floor,
                           bool horiz = false,
                           int max_check = 9999)
{
    coord_def normal(horiz ? 0 : 1, horiz ? 1 : 0);
    for (int d = 1; d >= -1; d-=2)
    {
        coord_def length(horiz ? d : 0, horiz ? 0 : d);
        int n = 1;

        while (n <= max_check)
        {
            coord_def pos(x + length.x*n,y + length.y*n);
            if (!lines.in_bounds(coord_def(pos.x + normal.x, pos.y + normal.y))
                || !strchr(floor, lines(pos.x + normal.x, pos.y + normal.y)))
            {
                break;
            }
            if (!lines.in_bounds(coord_def(pos.x - normal.x, pos.y - normal.y))
                || !strchr(floor, lines(pos.x - normal.x, pos.y - normal.y)))
            {
                break;
            }

            if (!strchr(wall, lines(pos.x, pos.y)))
                return false;

            n++;
        }
    }

    // hit the end of the wall, so this is good
    return true;
}

// Only used for the join_the_dots command.
struct join_the_dots_path
{
    vector<coord_def> cells;
    int hit_vault_count;
    int avoid_vault_count;
};

/*
 * Calculates a possible path joining the provided coordinates.
 *
 * @param from              The start of the path to be calculated.
 * @param to                The end of the path to be calculated.
 * @param force_straight    Whether the path must be a straight line.
 * @param allow_diagonals   Whether the path can travel diagonally.
 * @return A data structure containing (1) the path, & (2) the number of times
 * it hit or almost hit an existing vault.
 */
static join_the_dots_path _calculate_join_the_dots_path (const coord_def& from,
                                                         const coord_def& to,
                                                         bool force_straight,
                                                         bool allow_diagonals)
{
    join_the_dots_path path;
    path.hit_vault_count = 0;
    path.avoid_vault_count = 0;

    coord_def at = from;
    while (true) // loop breaks below
    {
        // 1. Handle this position

        path.cells.push_back(at);
        if (env.level_map_mask(at) & MMT_VAULT)
            path.hit_vault_count++;

        // check done after recording position
        if (at == to)
            break;  // exit loop

        // 2. Identify good moves

        // possible next positions
        int x_move = (at.x < to.x) ? 1 : ((at.x > to.x) ? -1 : 0);
        int y_move = (at.y < to.y) ? 1 : ((at.y > to.y) ? -1 : 0);

        coord_def next_x  = coord_def(at.x + x_move, at.y);
        coord_def next_y  = coord_def(at.x,          at.y + y_move);
        coord_def next_xy = coord_def(at.x + x_move, at.y + y_move);

        // moves that get you closer
        bool good_x  = (x_move != 0);
        bool good_y  = (y_move != 0);
        bool good_xy = (x_move != 0) && (y_move != 0) && allow_diagonals;

        // avoid vaults if possible
        bool vault_x  = env.level_map_mask(next_x)  & MMT_VAULT;
        bool vault_y  = env.level_map_mask(next_y)  & MMT_VAULT;
        bool vault_xy = env.level_map_mask(next_xy) & MMT_VAULT;
        if (   (!vault_x  && good_x)
            || (!vault_y  && good_y)
            || (!vault_xy && good_xy))
        {
            // if there is a good path that doesn't hit a vault,
            //  disable the otherwise-good paths that do

            if (vault_x)  path.avoid_vault_count++;
            if (vault_y)  path.avoid_vault_count++;
            if (vault_xy) path.avoid_vault_count++;

            // There is no &&= operator because short-circuit
            //  evaluation can do strange and terrible things
            //  when combined with function calls.
            good_x  &= !vault_x;
            good_y  &= !vault_y;
            good_xy &= !vault_xy;
        }
        else
        {
            // there is no way to avoid vaults, so hitting one is OK
            path.avoid_vault_count += 3;
        }

        // 3. Choose the next move
        if (force_straight)
        {
            if (good_xy)
                at = next_xy;
            else if (good_x)
                at = next_x;
            else
                at = next_y;
        }
        else
        {
            // allow irregular paths

            // used for movement ratios; our goal is to make a
            //  path approximately straight in any direction
            int x_diff = abs(at.x - to.x);
            int y_diff = abs(at.y - to.y);
            int sum_diff = x_diff + y_diff;
            int min_diff = (x_diff < y_diff) ? x_diff : y_diff;
            int max_diff = sum_diff - min_diff;

            // halve chance because a diagonal is worth 2 other moves
            if (good_xy && (x_chance_in_y(min_diff, max_diff * 2)
                            || (!good_x && !good_y)))
            {
                at = next_xy;
            }
            else if (good_x && (x_chance_in_y(x_diff, sum_diff) || !good_y))
                at = next_x;
            else
                at = next_y;
        }
    }

    // path is finished
    return path;
}


/*
 * Calculates a possible path joining the provided coordinates.
 *
 * @param from              The start of the path to be calculated.
 * @param to                The end of the path to be calculated.
 * @param force_straight    Whether the path must be a straight line.
 * @param allow_diagonals   Whether the path can travel diagonally.
 * @return A data structure containing (1) the path, & (2) the number of times
 * it hit or almost hit an existing vault.
 */
static void _draw_join_the_dots_path (map_lines &lines,
                                      const join_the_dots_path& path,
                                      const char* passable,
                                      int thickness, char fill)
{
    int delta_min = -thickness / 2;
    int delta_max = delta_min + thickness;
    for (coord_def center : path.cells)
    {
        for (int dx = delta_min; dx < delta_max; dx++)
            for (int dy = delta_min; dy < delta_max; dy++)
            {
                int x = center.x + dx;
                int y = center.y + dy;

                // we never change the border
                if (x >= 1 && x < lines.width()  - 1 &&
                    y >= 1 && y < lines.height() - 1 &&
                    !strchr(passable, lines(x, y)))
                {
                    lines(x, y) = fill;
                }
            }
    }
}


LUAFN(dgn_count_feature_in_box)
{
    LINES(ls, 1, lines);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

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
        return 0;

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
        return 0;

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
        return 1;
    }

    lua_pushnumber(ls, _count_passable_neighbors(ls, lines, x, y, passable));
    return 1;
}

LUAFN(dgn_is_valid_coord)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);

    if (x < 0 || x >= lines.width())
    {
        lua_pushboolean(ls, false);
        return 1;
    }

    if (y < 0 || y >= lines.height())
    {
        lua_pushboolean(ls, false);
        return 1;
    }

    lua_pushboolean(ls, true);
    return 1;
}

LUAFN(dgn_extend_map)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, height, 1);
    TABLE_INT(ls, width, 1);
    TABLE_CHAR(ls, fill, 'x');

    lines.extend(width, height, fill);

    return 0;
}

LUAFN(dgn_fill_area)
{
    LINES(ls, 1, lines);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    TABLE_CHAR(ls, fill, 'x');
    TABLE_CHAR(ls, border, fill);

    _fill_area(ls, lines, x1, y1, x2, y2, fill);
    if (border != fill)
        _border_area(lines, x1, y1, x2, y2, border);

    return 0;
}

LUAFN(dgn_fill_disconnected)
{
    LINES(ls, 1, lines);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

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

    return 0;
}

LUAFN(dgn_is_passable_coord)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);
    TABLE_STR(ls, passable, traversable_glyphs);

    if (!_valid_coord(ls, lines, x, y))
        return 0;

    if (strchr(passable, lines(x, y)))
        lua_pushboolean(ls, true);
    else
        lua_pushboolean(ls, false);

    return 1;
}

LUAFN(dgn_find_in_area)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x1, -1);
    TABLE_INT(ls, y1, -1);
    TABLE_INT(ls, x2, -1);
    TABLE_INT(ls, y2, -1);
    TABLE_BOOL(ls, find_vault, false);

    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    TABLE_STR(ls, find, "x");

    int x, y;

    for (x = x1; x <= x2; x++)
        for (y = y1; y <= y2; y++)
            if (strchr(find, lines(x, y))
                || (find_vault && (env.level_map_mask(coord_def(x,y))
                                   & MMT_VAULT)))
            {
                lua_pushboolean(ls, true);
                return 1;
            }

    lua_pushboolean(ls, false);
    return 1;
}

LUAFN(dgn_height)
{
    LINES(ls, 1, lines);
    PLUARET(number, lines.height());
}

LUAFN(dgn_primary_vault_dimensions)
{
    // we don't need this because this function doesn't use the
    //  current map
    // LINES(ls, 1, lines);

    static const int NO_PRIMARY_VAULT = 99999;

    int x_min =  NO_PRIMARY_VAULT;
    int x_max = -NO_PRIMARY_VAULT;
    int y_min =  NO_PRIMARY_VAULT;
    int y_max = -NO_PRIMARY_VAULT;

    for (int y = 0; y < GYM; y++)
        for (int x = 0; x < GXM; x++)
        {
            if (env.level_map_mask(coord_def(x,y)) & MMT_VAULT)
            {
                if (x < x_min)
                    x_min = x;
                if (x > x_max)
                    x_max = x;
                if (y < y_min)
                    y_min = y;
                if (y > y_max)
                    y_max = y;
            }
        }

    if (x_min != NO_PRIMARY_VAULT)
    {
        if (x_max == -NO_PRIMARY_VAULT)
            return luaL_error(ls, "Primary vault has x_min %d but no x_max", x_min);
        if (y_min ==  NO_PRIMARY_VAULT)
            return luaL_error(ls, "Primary vault has x_min %d but no y_min", x_min);
        if (y_max == -NO_PRIMARY_VAULT)
            return luaL_error(ls, "Primary vault has x_min %d but no y_max", x_min);

        lua_pushnumber(ls, x_min);
        lua_pushnumber(ls, x_max);
        lua_pushnumber(ls, y_min);
        lua_pushnumber(ls, y_max);
    }
    else  // no primary vault found
    {
        if (x_max != -NO_PRIMARY_VAULT)
            return luaL_error(ls, "Primary vault has x_max %d but no x_min", x_max);
        if (y_min !=  NO_PRIMARY_VAULT)
            return luaL_error(ls, "Primary vault has y_min %d but no x_min", y_min);
        if (y_max != -NO_PRIMARY_VAULT)
            return luaL_error(ls, "Primary vault has y_max %d but no x_min", y_max);

        lua_pushnil(ls);
        lua_pushnil(ls);
        lua_pushnil(ls);
        lua_pushnil(ls);
    }

    return 4;
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
    TABLE_BOOL(ls, force_straight, false);
    TABLE_BOOL(ls, allow_diagonals, false);
    TABLE_INT(ls, thickness, 1);

    if (!_valid_coord(ls, lines, x1, y1))
        return 0;
    if (!_valid_coord(ls, lines, x2, y2))
        return 0;

    coord_def from(x1, y1);
    coord_def to(x2, y2);

    if (from == to)
        return 0;

    // calculate possible paths
    join_the_dots_path path1 =
        _calculate_join_the_dots_path(from, to,
                                      force_straight, allow_diagonals);
    join_the_dots_path path2 =
        _calculate_join_the_dots_path(to, from,
                                      force_straight, allow_diagonals);

    // add better path
    // prefer fewer vaults hit, then fewer vaults avoided, then toss a coin
    const bool first_path_better =
        path1.hit_vault_count < path2.hit_vault_count
        || (path1.hit_vault_count == path2.hit_vault_count
            && (path1.avoid_vault_count < path2.avoid_vault_count
                || path1.avoid_vault_count == path2.avoid_vault_count
                   && coinflip()
                )
            );
    _draw_join_the_dots_path(lines, first_path_better ? path1 : path2,
                             passable, thickness, fill);

    return 0;
}

LUAFN(dgn_make_circle)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);
    TABLE_INT(ls, radius, 1);
    TABLE_CHAR(ls, fill, 'x');

    if (!_valid_coord(ls, lines, x, y))
        return 0;

    float radius_squared_max = (radius + 0.5f) * (radius + 0.5f);
    for (int ry = -radius; ry <= radius; ++ry)
        for (int rx = -radius; rx <= radius; ++rx)
            if (rx * rx + ry * ry < radius_squared_max)
                lines(x + rx, y + ry) = fill;

    return 0;
}

LUAFN(dgn_make_diamond)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);
    TABLE_INT(ls, radius, 1);
    TABLE_CHAR(ls, fill, 'x');

    if (!_valid_coord(ls, lines, x, y))
        return 0;

    for (int ry = -radius; ry <= radius; ++ry)
        for (int rx = -radius; rx <= radius; ++rx)
            if (abs(rx) + abs(ry) <= radius)
                lines(x + rx, y + ry) = fill;

    return 0;
}

LUAFN(dgn_make_rounded_square)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);
    TABLE_INT(ls, radius, 1);
    TABLE_CHAR(ls, fill, 'x');

    if (!_valid_coord(ls, lines, x, y))
        return 0;

    for (int ry = -radius; ry <= radius; ++ry)
        for (int rx = -radius; rx <= radius; ++rx)
            if (abs(rx) != radius || abs(ry) != radius)
                lines(x + rx, y + ry) = fill;

    return 0;
}

LUAFN(dgn_make_square)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, -1);
    TABLE_INT(ls, y, -1);
    TABLE_INT(ls, radius, 1);
    TABLE_CHAR(ls, fill, 'x');

    if (!_valid_coord(ls, lines, x, y))
        return 0;

    for (int ry = -radius; ry <= radius; ++ry)
        for (int rx = -radius; rx <= radius; ++rx)
            lines(x + rx, y + ry) = fill;

    return 0;
}

LUAFN(dgn_make_box)
{
    LINES(ls, 1, lines);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    TABLE_CHAR(ls, floor, '.');
    TABLE_CHAR(ls, wall, 'x');
    TABLE_INT(ls, thickness, 1);

    _fill_area(ls, lines, x1, y1, x2, y2, wall);
    _fill_area(ls, lines, x1+thickness, y1+thickness,
                          x2-thickness, y2-thickness, floor);

    return 0;
}

LUAFN(dgn_make_box_doors)
{
    LINES(ls, 1, lines);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    TABLE_INT(ls, number, 1);
    TABLE_INT(ls, thickness, 1);
    TABLE_CHAR(ls, door, '+');
    TABLE_CHAR(ls, inner_door, '.');
    TABLE_CHAR(ls, between_doors, '.');
    TABLE_BOOL(ls, veto_gates, false);
    TABLE_STR(ls, passable, traversable_glyphs);

    // size doesn't include corners
    int size_x = x2 - x1 + 1 - thickness * 2;
    int size_y = y2 - y1 + 1 - thickness * 2;
    int position_count = size_x * 2 + size_y * 2;

    int max_sanity = number * 100;
    int sanity = 0;

    int door_count = 0;
    while (door_count < number)
    {
        int position = random2(position_count);
        int side;
        int x, y;
        if (position < size_x)
        {
            side = 0;
            x = x1 + thickness + position;
            y = y1;
        }
        else if (position < size_x * 2)
        {
            side = 1;
            x = x1 + thickness + (position - size_x);
            y = y2;
        }
        else if (position < size_x * 2 + size_y)
        {
            side = 2;
            x = x1;
            y = y1 + thickness + (position - size_x * 2);
        }
        else
        {
            side = 3;
            x = x2;
            y = y1 + thickness + (position - size_x * 2 - size_y);
        }

        // We veto a position if:
        //  -> The cell outside the box is not passible
        //  -> The cell (however far) inside the box is not passible
        //  -> There is a door to the left or right and we are vetoing gates
        bool good = true;
        if (side < 2)
        {
            if (!strchr(passable, lines(x, y - (side == 0 ? 1 : thickness))))
                good = false;
            if (!strchr(passable, lines(x, y + (side == 1 ? 1 : thickness))))
                good = false;
            if (veto_gates && (lines(x-1, y) == door || lines(x+1, y) == door))
                good = false;
        }
        else
        {
            if (!strchr(passable, lines(x - (side == 2 ? 1 : thickness), y)))
                good = false;
            if (!strchr(passable, lines(x + (side == 3 ? 1 : thickness), y)))
                good = false;
            if (veto_gates && (lines(x, y-1) == door || lines(x, y+1) == door))
                good = false;
        }

        if (good)
        {
            door_count++;
            lines(x, y) = door;
            for (int i = 1; i < thickness; i++)
            {
                switch (side)
                {
                case 0: y++;  break;
                case 1: y--;  break;
                case 2: x++;  break;
                case 3: x--;  break;
                }
                lines(x, y) = between_doors;
            }
            if (thickness > 1)
                lines(x, y) = inner_door;
        }
        else
        {
            sanity++;
            if (sanity >= max_sanity)
                break;
        }
    }

    lua_pushnumber(ls, door_count);
    return 1;
}

LUAFN(dgn_make_irregular_box)
{
    LINES(ls, 1, lines);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    TABLE_CHAR(ls, floor, '.');
    TABLE_CHAR(ls, wall, 'x');
    TABLE_CHAR(ls, door, '+');
    TABLE_INT(ls, door_count, 1);
    TABLE_INT(ls, div_x, 1);
    TABLE_INT(ls, div_y, 1);
    TABLE_INT(ls, in_x, 10000);
    TABLE_INT(ls, in_y, 10000);

    make_irregular_box(lines, x1, y1, x2, y2,
                       div_x, div_y, in_x, in_y,
                       floor, wall, door, door_count);
    return 0;
}

LUAFN(dgn_make_round_box)
{
    LINES(ls, 1, lines);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    TABLE_CHAR(ls, floor, '.');
    TABLE_CHAR(ls, wall, 'x');
    TABLE_CHAR(ls, door, '+');
    TABLE_INT(ls, door_count, 1);
    TABLE_STR(ls, passable, traversable_glyphs);
    TABLE_BOOL(ls, veto_gates, false);
    TABLE_BOOL(ls, veto_if_no_doors, false);

    const int OUTSIDE = 0;
    const int WALL    = 1;
    const int FLOOR   = 2;
    const int DOOR    = 3;

    int size_x = x2 - x1 + 1;
    int size_y = y2 - y1 + 1;

    //
    //  The basic idea here is we draw a filled circle, hollow
    //    out the middle, and then place doors on straight walls
    //    until we have enough.
    //
    //  We do not know for sure whether we want to actually draw
    //    anything until the end, so we will draw out tower onto
    //    our own separate array (actually a vector of vectors
    //    so we can set the size at runtime). Then, if
    //    everything goes well, we will copy it to the world with
    //    the appropriate glyphs.
    //
    //  Note that each of these steps has to be completed before
    //    we can do the next one, so all the loops over the same
    //    area are required.
    //

    //  1. Fill with OUTSIDE glyphs.
    vector<vector<int> > new_glyphs(size_x, vector<int>(size_y, OUTSIDE));

    //  2. Draw wall glyphs for filled circle.
    //    -> This is a formula for an ellipse in case we get a
    //       non-circular room to make
    //    -> we add an extra 0.5 to the radius so that we don't
    //       get wall isolated cells on the outside
    float radius_x = (size_x - 1.0f) * 0.5f;
    float radius_y = (size_y - 1.0f) * 0.5f;
    for (int x = 0; x < size_x; x++)
        for (int y = 0; y < size_y; y++)
        {
            float fraction_x = (x - radius_x) / (radius_x + 0.5f);
            float fraction_y = (y - radius_y) / (radius_y + 0.5f);
            if (fraction_x * fraction_x + fraction_y * fraction_y <= 1.0f)
                new_glyphs[x][y] = WALL;
        }

    //  3. Replace all wall glypyhs that don't touch outside the
    //     circle with floor glyphs.
    for (int x = 0; x < size_x; x++)
        for (int y = 0; y < size_y; y++)
        {
            // we can't use adjacent_iterator it doesn't
            //  report neighbours with negative coordinates
            if (new_glyphs[x][y] == WALL
                && x > 0 && x < size_x - 1
                && y > 0 && y < size_y - 1
                && new_glyphs[x - 1][y - 1] != OUTSIDE
                && new_glyphs[x    ][y - 1] != OUTSIDE
                && new_glyphs[x + 1][y - 1] != OUTSIDE
                && new_glyphs[x - 1][y    ] != OUTSIDE
                && new_glyphs[x + 1][y    ] != OUTSIDE
                && new_glyphs[x - 1][y + 1] != OUTSIDE
                && new_glyphs[x    ][y + 1] != OUTSIDE
                && new_glyphs[x + 1][y + 1] != OUTSIDE)
            {
                new_glyphs[x][y] = FLOOR;
            }
        }

    //  4. Find all potential door positions.
    vector<coord_def> door_positions;
    for (int x = 0; x < size_x; x++)
        for (int y = 0; y < size_y; y++)
            if (new_glyphs[x][y] == WALL)
            {
                // check for wall in each direction
                bool xm = (x - 1 >= 0      && new_glyphs[x - 1][y] == WALL);
                bool xp = (x + 1 <  size_x && new_glyphs[x + 1][y] == WALL);
                bool ym = (y - 1 >= 0      && new_glyphs[x][y - 1] == WALL);
                bool yp = (y + 1 <  size_y && new_glyphs[x][y + 1] == WALL);

                int real_x = x1 + x;
                int real_y = y1 + y;

                // We are on an X-aligned wall
                if (xm && xp && !ym && !yp)
                {
                    //
                    //  Check for passable glyphs in real map
                    //    and outside the tower. The check
                    //    order is:
                    //    -> in real map
                    //    -> passable
                    //    -> outside temporary array
                    //       or array has OUTSIDE glyph
                    //
                    //  If we can find one on at least one side,
                    //    we can put a door here.
                    //    -> we will only get two on rooms only
                    //       1 cell wide including walls
                    //

                    if (real_y - 1 >= 0
                        && strchr(passable, lines(real_x, real_y - 1))
                        && (y - 1 < 0
                            || new_glyphs[x][y - 1] == OUTSIDE))
                    {
                        door_positions.emplace_back(x, y);
                    }
                    else if (real_y + 1 < lines.height()
                             && strchr(passable, lines(real_x, real_y + 1))
                             && (y + 1 >= size_y
                                 || new_glyphs[x][y + 1] == OUTSIDE))
                    {
                        door_positions.emplace_back(x, y);
                    }
                }

                // We are on an Y-aligned wall
                if (!xm && !xp && ym && yp)
                {
                    // Same checks as above, but the other axis
                    if (real_x - 1 >= 0
                        && strchr(passable, lines(real_x - 1, real_y))
                        && (x - 1 < 0
                            || new_glyphs[x - 1][y] == OUTSIDE))
                    {
                        door_positions.emplace_back(x, y);
                    }
                    else if (real_x + 1 < lines.width()
                             && strchr(passable, lines(real_x + 1, real_y))
                             && (x + 1 >= size_x
                                 || new_glyphs[x + 1][y] == OUTSIDE))
                    {
                        door_positions.emplace_back(x, y);
                    }
                }
            }

    //  5. Add doors
    int doors_placed = 0;
    while (doors_placed < door_count && !door_positions.empty())
    {
        int index = random2(door_positions.size());
        coord_def pos = door_positions[index];
        door_positions[index] = door_positions[door_positions.size() - 1];
        door_positions.pop_back();

        bool good_spot = true;
        if (veto_gates)
        {
            if (pos.x - 1 >= 0     && new_glyphs[pos.x - 1][pos.y] == DOOR)
                good_spot = false;
            if (pos.x + 1 < size_x && new_glyphs[pos.x + 1][pos.y] == DOOR)
                good_spot = false;
            if (pos.y - 1 >= 0     && new_glyphs[pos.x][pos.y - 1] == DOOR)
                good_spot = false;
            if (pos.y + 1 < size_y && new_glyphs[pos.x][pos.y + 1] == DOOR)
                good_spot = false;
        }

        if (good_spot)
        {
            new_glyphs[pos.x][pos.y] = DOOR;
            doors_placed++;
        }
    }

    //  6. Add tower to map (if not vetoed)
    if (doors_placed > 0 || !veto_if_no_doors)
    {
        for (int x = 0; x < size_x; x++)
            for (int y = 0; y < size_y; y++)
            {
                switch (new_glyphs[x][y])
                {
                // leave existing glyphs on OUTSIDE
                case WALL:  lines(x1 + x, y1 + y) = wall;  break;
                case FLOOR: lines(x1 + x, y1 + y) = floor; break;
                case DOOR:  lines(x1 + x, y1 + y) = door; break;
                }
            }

        lua_pushboolean(ls, true);
    }
    else
        lua_pushboolean(ls, false);

    return 1;
}

// Return a metatable for a point on the map_lines grid.
LUAFN(dgn_mapgrd_table)
{
    MAP(ls, 1, map);

    map_def **mapref = clua_new_userdata<map_def *>(ls, MAPGRD_METATABLE);
    *mapref = map;

    return 1;
}

LUAFN(dgn_octa_room)
{
    LINES(ls, 1, lines);

    int default_oblique = min(lines.width(), lines.height()) / 2 - 1;
    TABLE_INT(ls, oblique, default_oblique);
    TABLE_CHAR(ls, outside, 'x');
    TABLE_CHAR(ls, inside, '.');
    TABLE_STR(ls, replace, "");

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    coord_def tl(x1, y1);
    coord_def br(x2, y2);

    for (rectangle_iterator ri(tl, br); ri; ++ri)
    {
        const coord_def mc = *ri;
        char glyph = lines(mc);
        if (replace[0] && !strchr(replace, glyph))
            continue;

        int ob = 0;
        ob += max(oblique + tl.x - mc.x, 0);
        ob += max(oblique + mc.x - br.x, 0);

        bool is_inside = (mc.y >= tl.y + ob && mc.y <= br.y - ob);
        lines(mc) = is_inside ? inside : outside;
    }

    return 0;
}

LUAFN(dgn_remove_isolated_glyphs)
{
    LINES(ls, 1, lines);

    TABLE_STR(ls, find, "");
    TABLE_CHAR(ls, replace, '.');
    TABLE_INT(ls, percent, 100);
    TABLE_BOOL(ls, boxy, false);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    // we never change the border
    if (x1 < 1)
        x1 = 1;
    if (x2 >= lines.width() - 1)
        x2 = lines.width() - 2;
    if (y1 < 1)
        y1 = 1;
    if (y2 >= lines.height() - 1)
        y2 = lines.height() - 2;

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            if (strchr(find, lines(x, y)) && x_chance_in_y(percent, 100))
            {
                bool do_replace = true;
                for (radius_iterator ri(coord_def(x, y), 1,
                                        (boxy ? C_ROUND : C_POINTY),
                                        true); ri; ++ri)
                {
                    if (_valid_coord(ls, lines, ri->x, ri->y, false))
                        if (strchr(find, lines(*ri)))
                        {
                            do_replace = false;
                            break;
                        }
                }
                if (do_replace)
                    lines(x, y) = replace;
            }

    return 0;
}

LUAFN(dgn_widen_paths)
{
    LINES(ls, 1, lines);

    TABLE_STR(ls, find, "");
    TABLE_CHAR(ls, replace, '.');
    TABLE_STR(ls, passable, traversable_glyphs);
    TABLE_INT(ls, percent, 100);
    TABLE_BOOL(ls, boxy, false);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    // we never change the border
    if (x1 < 1)
        x1 = 1;
    if (x2 >= lines.width() - 1)
        x2 = lines.width() - 2;
    if (y1 < 1)
        y1 = 1;
    if (y2 >= lines.height() - 1)
        y2 = lines.height() - 2;

    float antifraction_each = 1.0 - percent / 100.0f;
    float antifraction_current = 1.0;
    int percent_for_neighbours[9];
    for (int i = 0; i < 9; i++)
    {
        percent_for_neighbours[i] = 100 - (int)(antifraction_current * 100);
        antifraction_current *= antifraction_each;
    }

    // We do not replace this as we go to avoid favouring some directions.
    vector<coord_def> coord_to_replace;

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            if (strchr(find, lines(x, y)))
            {
                int neighbour_count = 0;
                for (radius_iterator ri(coord_def(x, y), 1,
                                        (boxy ? C_ROUND : C_POINTY),
                                        true); ri; ++ri)
                {
                    if (_valid_coord(ls, lines, ri->x, ri->y, false))
                        if (strchr(passable, lines(*ri)))
                            neighbour_count++;
                }

                // store this coordinate if needed
                if (x_chance_in_y(percent_for_neighbours[neighbour_count], 100))
                    coord_to_replace.emplace_back(x, y);
            }

    // now go through and actually replace the positions
    for (coord_def c : coord_to_replace)
        lines(c) = replace;

    return 0;
}

LUAFN(dgn_connect_adjacent_rooms)
{
    LINES(ls, 1, lines);

    TABLE_STR(ls, wall, "xcvbmn");
    TABLE_STR(ls, floor, ".");
    TABLE_CHAR(ls, replace, '.');
    TABLE_INT(ls, max, 1);
    TABLE_INT(ls, min, max);
    TABLE_INT(ls, check_distance, 9999);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    // we never go right up to the border to avoid looking off the map edge
    if (x1 < 1)
        x1 = 1;
    if (x2 >= lines.width() - 1)
        x2 = lines.width() - 2;
    if (y1 < 1)
        y1 = 1;
    if (y2 >= lines.height() - 1)
        y2 = lines.height() - 2;

    if (min < 0)
        return luaL_error(ls, "Invalid min connections: %d", min);
    if (max < min)
    {
        return luaL_error(ls, "Invalid max connections: %d (min is %d)",
                          max, min);
    }

    int count = random_range(min, max);
    for (random_rectangle_iterator ri(coord_def(x1, y1),
                                      coord_def(x2, y2)); ri; ++ri)
    {
        if (count <= 0)
        {
            // stop when have checked enough spots
            return 0;
        }

        int x = ri->x;
        int y = ri->y;

        if (strchr(wall, lines(*ri)))
        {
            if (strchr(floor, lines(x, y - 1))
                && strchr(floor, lines(x, y + 1))
                && (_wall_is_empty(lines, x, y, wall, floor,
                                   true, check_distance)))
            {
                lines(*ri) = replace;
            }
            else if (strchr(floor, lines(x - 1, y))
                     && strchr(floor, lines(x + 1, y))
                     && (_wall_is_empty(lines, x, y, wall, floor,
                                        false, check_distance)))
            {
                lines(*ri) = replace;
            }
        }
        count--;
    }

    return 0;
}

LUAFN(dgn_remove_disconnected_doors)
{
    LINES(ls, 1, lines);

    TABLE_STR(ls, door, "+");
    TABLE_STR(ls, open, traversable_glyphs);
    TABLE_CHAR(ls, replace, '.');

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    // we never go right up to the border to avoid looking off the map edge
    if (x1 < 1)
        x1 = 1;
    if (x2 >= lines.width() - 1)
        x2 = lines.width() - 2;
    if (y1 < 1)
        y1 = 1;
    if (y2 >= lines.height() - 1)
        y2 = lines.height() - 2;

    // TODO: Improve this to handle gates correctly.
    //  -> Right now it just removes them.
    //  -> I (infiniplex) tried to find formulas for this and there were
    //     too many weird cases.
    //    -> 2-long hallway with doors at both end should not be a gate
    //    -> end(s) of gate surround by wall should become wall
    //    -> door glyphs in a shape other than a straight line
    //    -> walls of either side of the middle of a gate when the ends
    //       are open may connect otherwise-disconnected regions
    //    -> results must be direction-independent

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            if (strchr(door, lines(x, y)))
            {
                //
                // This door is not part of a gate
                //  -> There are a lot of cases here because doors
                //     can be at corners
                //  -> We will choose the doors we want to keep
                //     and remove the rest of them
                //

                // which directions are open
                bool south     = strchr(open, lines(x,     y + 1));
                bool north     = strchr(open, lines(x,     y - 1));
                bool east      = strchr(open, lines(x + 1, y));
                bool west      = strchr(open, lines(x - 1, y));
                bool southeast = strchr(open, lines(x + 1, y + 1));
                bool northwest = strchr(open, lines(x - 1, y - 1));
                bool southwest = strchr(open, lines(x - 1, y + 1));
                bool northeast = strchr(open, lines(x + 1, y - 1));


                //
                // A door in an S-N straight wall
                //
                //     x      x     .x      x      x.
                //    .+.     +.     +.    .+     .+
                //     x     .x      x      x.     x
                //
                if (!south && !north
                    && (east || west)
                    && (east || southeast || northeast)
                    && (west || southwest || northwest))
                {
                    continue;
                }

                //
                // A door in an E-W straight wall
                //
                //     .     .        .     .      .
                //    x+x    x+x    x+x    x+x    x+x
                //     .      .      .     .        .
                //
                if (!east && !west
                    && (south || north)
                    && (south || northeast || northwest)
                    && (north || southeast || southwest))
                {
                    continue;
                }

                //
                // A door in a SE-NW diagonal
                //
                //    .x     ..     .xx
                //    x+.    .+x    x+x
                //     ..     x.    xx.
                //
                if (southeast && northwest && south == east && north == west)
                {
                    if (south != west)
                        continue;
                    else if (!south && !west && !southwest && !northeast)
                        continue;
                }

                //
                // A door in a SW-NE diagonal
                //
                //     ..     x.    xx.
                //    x+.    .+x    x+x
                //    .x     ..     .xx
                //
                if (southwest && northeast && south == west && north == east)
                {
                    if (south != east)
                        continue;
                    else if (!south && !east && !southeast && !northwest)
                        continue;
                }

                // if we get her, the door is invalid
                lines(x, y) = replace;
            }

    return 0;
}

LUAFN(dgn_add_windows)
{
    LINES(ls, 1, lines);

    TABLE_STR(ls, wall, "xcvbmn");
    TABLE_STR(ls, open, traversable_glyphs);
    TABLE_CHAR(ls, window, 'm');

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    // we never go right up to the border to avoid looking off the map edge
    if (x1 < 1)
        x1 = 1;
    if (x2 >= lines.width() - 1)
        x2 = lines.width() - 2;
    if (y1 < 1)
        y1 = 1;
    if (y2 >= lines.height() - 1)
        y2 = lines.height() - 2;

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            if (strchr(wall, lines(x, y)))
            {
                // which directions are open
                bool south_open     = strchr(open, lines(x,     y + 1));
                bool north_open     = strchr(open, lines(x,     y - 1));
                bool east_open      = strchr(open, lines(x + 1, y));
                bool west_open      = strchr(open, lines(x - 1, y));
                bool southeast_open = strchr(open, lines(x + 1, y + 1));
                bool northwest_open = strchr(open, lines(x - 1, y - 1));
                bool southwest_open = strchr(open, lines(x - 1, y + 1));
                bool northeast_open = strchr(open, lines(x + 1, y - 1));

                // which directions are blocked by walls
                bool south_blocked     = strchr(wall, lines(x,     y + 1));
                bool north_blocked     = strchr(wall, lines(x,     y - 1));
                bool east_blocked      = strchr(wall, lines(x + 1, y));
                bool west_blocked      = strchr(wall, lines(x - 1, y));
                bool southeast_blocked = strchr(wall, lines(x + 1, y + 1));
                bool northwest_blocked = strchr(wall, lines(x - 1, y - 1));
                bool southwest_blocked = strchr(wall, lines(x - 1, y + 1));
                bool northeast_blocked = strchr(wall, lines(x + 1, y - 1));

                // a simple window in a straight wall
                //
                //   .    x
                //  xmx  .m.
                //   .    x
                //
                if (south_open && north_open && east_blocked && west_blocked)
                    lines(x, y) = window;
                if (east_open && west_open && south_blocked && north_blocked)
                    lines(x, y) = window;

                // a diagonal window in a straight segment of a bent wall
                //
                //  . x  x .   x    x
                //  xmx  xmx  .m.  .m.
                //  x .  . x   x    x
                //
                if (east_blocked && west_blocked)
                {
                    if (southeast_open && northwest_open
                        && southwest_blocked && northeast_blocked)
                    {
                        lines(x, y) = window;
                    }
                    if (southwest_open && northeast_open
                        && southeast_blocked && northwest_blocked)
                    {
                        lines(x, y) = window;
                    }
                }

                // a window in a 3-wide diagonal wall
                //
                //  .x    x.
                //  xmx  xmx
                //   x.  .x
                //
                if (   south_blocked && north_blocked
                    && east_blocked  && west_blocked)
                {
                    if (southeast_open && northwest_open)
                        lines(x, y) = window;
                    if (southwest_open && northeast_open)
                        lines(x, y) = window;
                }

                // a window in a 2-wide diagonal wall
                //  -> this will change the whole wall
                //
                //   .    .   .x    x.
                //  .mx  xm.  xm.  .mx
                //   x.  .x    .    .
                //
                if (south_blocked && east_blocked
                    && north_open && west_open && southeast_open)
                {
                    lines(x, y) = window;
                }
                if (south_blocked && west_blocked
                    && north_open && east_open && southwest_open)
                {
                    lines(x, y) = window;
                }
                if (north_blocked && west_blocked
                    && south_open && east_open && northwest_open)
                {
                    lines(x, y) = window;
                }
                if (north_blocked && east_blocked
                    && south_open && west_open && northeast_open)
                {
                    lines(x, y) = window;
                }
            }

    return 0;
}

LUAFN(dgn_replace_area)
{
    LINES(ls, 1, lines);

    TABLE_STR(ls, find, 0);
    TABLE_CHAR(ls, replace, '\0');

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            if (strchr(find, lines(x, y)))
                lines(x, y) = replace;

    return 0;
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
        return 0;

    if (xdir < -1 || xdir > 1)
        return luaL_error(ls, "Invalid xdir: %d", xdir);

    if (ydir < -1 || ydir > 1)
        return luaL_error(ls, "Invalid ydir: %d", ydir);

    while (lines.in_bounds(coord_def(x, y)))
    {
        if (lines(x, y) == find)
        {
            lines(x, y) = replace;
            return 0;
        }

        x += xdir;
        y += ydir;
    }

    if (required)
        return luaL_error(ls, "Could not find feature '%c' to replace", find);

    return 0;
}

LUAFN(dgn_replace_random)
{
    LINES(ls, 1, lines);

    TABLE_CHAR(ls, find, '\0');
    TABLE_CHAR(ls, replace, '\0');
    TABLE_BOOL(ls, required, false);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    int count = (x2 - x1 + 1) * (y2 - y1 + 1);
    if (!count)
    {
        if (required)
            luaL_error(ls, "%s", "No elements to replace");
        return 0;
    }

    vector<coord_def> loc;
    loc.reserve(count);

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            if (lines(x, y) == find)
                loc.emplace_back(x, y);

    if (loc.empty())
    {
        if (required)
            luaL_error(ls, "Could not find '%c'", find);
        return 0;
    }

    int idx = random2(loc.size());
    lines(loc[idx]) = replace;

    return 0;
}

LUAFN(dgn_replace_closest)
{
    LINES(ls, 1, lines);

    TABLE_INT(ls, x, 0);
    TABLE_INT(ls, y, 0);
    TABLE_CHAR(ls, find, '\0');
    TABLE_CHAR(ls, replace, '\0');
    TABLE_BOOL(ls, required, false);

    coord_def center(x, y);

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    int count = (x2 - x1 + 1) * (y2 - y1 + 1);
    if (!count)
    {
        if (required)
            luaL_error(ls, "%s", "No elements to replace");
        return 0;
    }

    vector<coord_def> loc;
    loc.reserve(count);

    int best_distance = 10000;
    unsigned int best_count = 0;
    coord_def best_coord;

    for (int this_y = y1; this_y <= y2; ++this_y)
        for (int this_x = x1; this_x <= x2; ++this_x)
            if (lines(this_x, this_y) == find)
            {
                coord_def here(this_x, this_y);
                int distance = here.distance_from(center);
                if (distance < best_distance)
                {
                    best_distance = distance;
                    best_count = 1;
                    best_coord = here;
                }
                else if (distance == best_distance)
                {
                    best_count++;
                    if (one_chance_in(best_count))
                        best_coord = here;
                }
            }

    if (best_count == 0)
    {
        if (required)
            return luaL_error(ls, "Could not find '%c'", find);
        return 0;
    }

    lines(best_coord) = replace;

    return 0;
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
        return 0;

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
                    return 0;

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

    return 0;
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
        return 0;

    const int max_test_per_iteration = 10;
    int sanity = 0;
    int max_sanity = iterations * max_test_per_iteration;

    for (int i = 0; i < iterations; i++)
    {
        int x, y;
        do
        {
            if (sanity++ > max_sanity)
                return 0;

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

        for (radius_iterator ai(coord_def(x, y), boxy ? 2 : 1, C_CIRCLE,
                                false); ai; ++ai)
        {
            if (strchr(replace, lines(*ai)))
                lines(*ai) = fill;
        }
    }

    return 0;
}

LUAFN(dgn_add_pools)
{
    LINES(ls, 1, lines);

    TABLE_STR(ls, replace, ".");
    TABLE_CHAR(ls, border, '.');
    TABLE_INT(ls, pool_size, 100);
    TABLE_INT(ls, seed_separation, 2);

    vector<char> fill_glyphs = _pool_fill_glyphs_from_table(ls, "contents");

    int x1, y1, x2, y2;
    if (!_coords(ls, lines, x1, y1, x2, y2))
        return 0;

    int size_x = x2 - x1 + 1;
    int size_y = y2 - y1 + 1;

    //
    //  The basic ideas here is that we place a number of
    //    pool "seeds" on the map and spread them outward
    //    randomly until they run into each other. We never
    //    fill in the last cell, so they never actually
    //    touch and we get paths between them.
    //
    //  The algorithm we use to spread the pools is like a
    //    depth-/breadth-first search, except that:
    //      1. We choose a random element from the open list
    //      2. We have multiple starting locations, each
    //         with its own "closed" value
    //      3. We mark all cells bordered by 2 (or more)
    //         distinct non-BORDER closed values with special
    //         closed value BORDER
    //
    //  In the end, we used the "closed" values to determine
    //    which pool we are in. The BORDER value indicates
    //    a path between pools.
    //

    // NO_POOL
    //  -> must be lowest constant
    //  -> must match _get_pool_seed_positions
    const int NO_POOL   = 999997;
    const int IN_LIST   = 999998;
    const int BORDER    = 999999;
    const int FORBIDDEN = 1000000;

    // Step 1: Make a 2D array to store which pool each cell is part of
    //    -> We use nested vectors to store this. We cannot use
    //       a fixedarray because we don't know the size at
    //       compile time.

    vector<vector<int> > pool_index(size_x, vector<int>(size_y, FORBIDDEN));
    for (int x = 0; x < size_x; x++)
        for (int y = 0; y < size_y; y++)
        {
            if (strchr(replace, lines(x + x1, y + y1)))
                pool_index[x][y] = NO_POOL;
        }

    // Step 2: Place the pool seeds and add their neighbours to the open list

    vector<coord_def> pool_seeds = _get_pool_seed_positions(pool_index,
                                                            pool_size,
                                                            seed_separation);
    vector<coord_def> open_list;

    for (unsigned int pool = 0; pool < pool_seeds.size(); pool++)
    {
        int x = pool_seeds[pool].x;
        int y = pool_seeds[pool].y;

        pool_index[x][y] = pool;

        // add neighbours to open list
        for (orth_adjacent_iterator ai(pool_seeds[pool]); ai; ++ai)
            if (_valid_coord(ls, lines, ai->x, ai->y, false))
            {
                pool_index[ai->x][ai->y] = IN_LIST;
                open_list.push_back(*ai);
            }
    }

    // Step 3: Spread out pools as far as possible

    while (!open_list.empty())
    {
        // remove a random position from the open list
        int index_chosen = random2(open_list.size());
        coord_def chosen_coord = open_list[index_chosen];
        open_list[index_chosen] = open_list.back();
        open_list.pop_back();

        // choose which neighbouring pool to join
        int chosen_pool = NO_POOL;
        for (adjacent_iterator ai(chosen_coord); ai; ++ai)
            if (_valid_coord(ls, lines, ai->x, ai->y, false))
            {
                int neighbour_pool = pool_index[ai->x][ai->y];
                if (neighbour_pool < NO_POOL)
                {
                    // this is a valid pool, consider it
                    if (chosen_pool == NO_POOL)
                        chosen_pool = neighbour_pool;
                    else if (chosen_pool == neighbour_pool)
                        ; // already correct
                    else
                    {
                        // this is the path between pools
                        chosen_pool = BORDER;
                        break;
                    }
                }
                else if (neighbour_pool == FORBIDDEN)
                {
                    // next to a wall
                    chosen_pool = BORDER;
                    break;
                }
            }

        if (chosen_pool != NO_POOL)
        {
            // add this cell to the appropriate pool
            pool_index[chosen_coord.x][chosen_coord.y] = chosen_pool;

            // add neighbours to open list
            for (orth_adjacent_iterator ai(chosen_coord); ai; ++ai)
                if (_valid_coord(ls, lines, ai->x, ai->y, false)
                    && pool_index[ai->x][ai->y] == NO_POOL)
                {
                    pool_index[ai->x][ai->y] = IN_LIST;
                    open_list.push_back(*ai);
                }
        }
        else
        {
            // a default, although I do not know why we ever get here
            pool_index[chosen_coord.x][chosen_coord.y] = NO_POOL;
        }
    }

    // Step 4: Add the pools to the map

    vector<char> pool_glyphs(pool_seeds.size(), '\0');
    for (char &gly : pool_glyphs)
        gly = fill_glyphs[random2(fill_glyphs.size())];

    for (int x = 0; x < size_x; x++)
        for (int y = 0; y < size_y; y++)
            {
            int index = pool_index[x][y];
            if (index < (int)(pool_glyphs.size()))
                lines(x + x1, y + y1) = pool_glyphs[index];
            else if (index == NO_POOL || index == BORDER)
                lines(x + x1, y + y1) = border;
            else if (index == FORBIDDEN)
                ; // leave it alone
            else
            {
                return luaL_error(ls, "Invalid pool index %i/%i at (%i, %i)",
                                  index, pool_glyphs.size(), x + x1, y + y1);
            }
        }

    return 0;
}

static int dgn_width(lua_State *ls)
{
    LINES(ls, 1, lines);
    PLUARET(number, lines.width());
}

LUAFN(dgn_delve)
{
    LINES(ls, 1, lines);

    ARG_INT(ls, 2, ngb_min, 2);
    ARG_INT(ls, 3, ngb_max, 3);
    ARG_INT(ls, 4, connchance, 0);
    ARG_INT(ls, 5, cellnum, -1);
    ARG_INT(ls, 6, top, 125);

    delve(&lines, ngb_min, ngb_max, connchance, cellnum, top);
    return 0;
}

LUAFN(dgn_farthest_from)
{
    LINES(ls, 1, lines);
    const char *beacons = luaL_checkstring(ls, 2);

    ASSERT(lines.width() <= GXM);
    ASSERT(lines.height() <= GYM);
    FixedArray<bool, GXM, GYM> visited;
    visited.init(false);
    vector<coord_def> queue;
    unsigned int dc_prev = 0, dc_next; // indices where dist changes to the next value

    for (int x = lines.width(); x >= 0; x--)
        for (int y = lines.height(); y >= 0; y--)
        {
            coord_def c(x, y);
            if (lines.in_map(c) && strchr(beacons, lines(c)))
            {
                queue.push_back(c);
                visited(c) = true;
            }
        }

    dc_next = queue.size();
    if (!dc_next)
    {
        // Not a single beacon, nowhere to go.
        lua_pushnil(ls);
        lua_pushnil(ls);
        return 2;
    }

    for (unsigned int dc = 0; dc < queue.size(); dc++)
    {
        if (dc >= dc_next)
        {
            dc_prev = dc_next;
            dc_next = dc;
        }

        coord_def c = queue[dc];
        for (adjacent_iterator ai(c); ai; ++ai)
            if (lines.in_map(*ai) && !visited(*ai)
                && strchr(traversable_glyphs, lines(*ai)))
            {
                queue.push_back(*ai);
                visited(*ai) = true;
            }
    }

    ASSERT(dc_next > dc_prev);
    // There may be multiple farthest cells, pick one at random.
    coord_def loc = queue[random_range(dc_prev, dc_next - 1)];
    lua_pushnumber(ls, loc.x);
    lua_pushnumber(ls, loc.y);
    return 2;
}

/* Wrappers for C++ layouts, to facilitate choosing of layouts by weight and
 * depth */

LUAFN(dgn_layout_basic)
{
    dgn_build_basic_level();
    return 0;
}

LUAFN(dgn_layout_bigger_room)
{
    dgn_build_bigger_room_level();
    return 0;
}

LUAFN(dgn_layout_chaotic_city)
{
    const dungeon_feature_type feature = check_lua_feature(ls, 2, true);
    dgn_build_chaotic_city_level(feature == DNGN_UNSEEN ? NUM_FEATURES : feature);
    return 0;
}

LUAFN(dgn_layout_shoals)
{
    dgn_build_shoals_level();
    return 0;
}

LUAFN(dgn_layout_swamp)
{
    dgn_build_swamp_level();
    return 0;
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
    { "primary_vault_dimensions", &dgn_primary_vault_dimensions },
    { "join_the_dots", &dgn_join_the_dots },
    { "make_circle", &dgn_make_circle },
    { "make_diamond", &dgn_make_diamond },
    { "make_rounded_square", &dgn_make_rounded_square },
    { "make_square", &dgn_make_square },
    { "make_box", &dgn_make_box },
    { "make_box_doors", &dgn_make_box_doors },
    { "make_irregular_box", &dgn_make_irregular_box },
    { "make_round_box", &dgn_make_round_box },
    { "mapgrd_table", dgn_mapgrd_table },
    { "octa_room", &dgn_octa_room },
    { "remove_isolated_glyphs", &dgn_remove_isolated_glyphs },
    { "widen_paths", &dgn_widen_paths },
    { "connect_adjacent_rooms", &dgn_connect_adjacent_rooms },
    { "remove_disconnected_doors", &dgn_remove_disconnected_doors },
    { "add_windows", &dgn_add_windows },
    { "replace_area", &dgn_replace_area },
    { "replace_first", &dgn_replace_first },
    { "replace_random", &dgn_replace_random },
    { "replace_closest", &dgn_replace_closest },
    { "smear_map", &dgn_smear_map },
    { "spotty_map", &dgn_spotty_map },
    { "add_pools", &dgn_add_pools },
    { "delve", &dgn_delve },
    { "width", dgn_width },
    { "farthest_from", &dgn_farthest_from },

    { "layout_basic", &dgn_layout_basic },
    { "layout_bigger_room", &dgn_layout_bigger_room },
    { "layout_chaotic_city", &dgn_layout_chaotic_city },
    { "layout_shoals", &dgn_layout_shoals },
    { "layout_swamp", &dgn_layout_swamp },

    { nullptr, nullptr }
};
