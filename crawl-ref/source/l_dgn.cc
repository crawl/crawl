#include "AppHdr.h"

#include "l_libs.h"

#include <cmath>

#include "branch.h"
#include "cloud.h"
#include "cluautil.h"
#include "colour.h"
#include "coord.h"
#include "coordit.h"
#include "dgn-shoals.h"
#include "directn.h"
#include "dungeon.h"
#include "flood_find.h"
#include "l_defs.h"
#include "libutil.h"
#include "mapmark.h"
#include "maps.h"
#include "shout.h"
#include "spl-util.h"
#include "state.h"
#include "stringutil.h"
#include "tiledef-dngn.h"
#include "tileview.h"
#include "view.h"

static const char *VAULT_PLACEMENT_METATABLE = "crawl.vault-placement";

///////////////////////////////////////////////////////////////////////////
// Lua dungeon bindings (in the dgn table).

static inline bool _lua_boolean(lua_State *ls, int ndx, bool defval)
{
    return lua_isnone(ls, ndx)? defval : lua_toboolean(ls, ndx);
}

void dgn_reset_default_depth()
{
    lc_default_depths.clear();
}

string dgn_set_default_depth(const string &s)
{
    try
    {
        lc_default_depths = depth_ranges::parse_depth_ranges(s);
    }
    catch (const string &error)
    {
        return error;
    }
    return "";
}

static void dgn_add_depths(depth_ranges &drs, lua_State *ls, int s, int e)
{
    for (int i = s; i <= e; ++i)
    {
        const char *depth = luaL_checkstring(ls, i);
        try
        {
            drs.add_depths(depth_ranges::parse_depth_ranges(depth));
        }
        catch (const string &error)
        {
            luaL_error(ls, error.c_str());
        }
    }
}

static int dgn_depth_proc(lua_State *ls, depth_ranges &dr, int s)
{
    if (lua_gettop(ls) < s)
        PLUARET(string, dr.describe().c_str());

    if (lua_isnil(ls, s))
    {
        dr.clear();
        return 0;
    }

    dr.clear();
    dgn_add_depths(dr, ls, s, lua_gettop(ls));
    return 0;
}

static int dgn_default_depth(lua_State *ls)
{
    return dgn_depth_proc(ls, lc_default_depths, 1);
}

static int dgn_depth(lua_State *ls)
{
    MAP(ls, 1, map);
    return dgn_depth_proc(ls, map->depths, 2);
}

static int dgn_place(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) > 1)
    {
        if (lua_isnil(ls, 2))
            map->place.clear();
        else
        {
            try
            {
                map->place = depth_ranges::parse_depth_ranges(luaL_checkstring(ls, 2));
            }
            catch (const string &err)
            {
                luaL_error(ls, err.c_str());
            }
        }
    }
    PLUARET(string, map->place.describe().c_str());
}

static int dgn_desc(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) > 1)
    {
        if (lua_isnil(ls, 2))
            map->description.clear();
        else
            map->description = trimmed_string(luaL_checkstring(ls, 2));
    }
    PLUARET(string, map->description.c_str());
}

static int dgn_order(lua_State *ls)
{
    MAP(ls, 1, map);
    map->order = luaL_checkint(ls, 2);
    PLUARET(number, map->order);
}

static int dgn_tags(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) > 1)
    {
        if (lua_isnil(ls, 2))
            map->tags.clear();
        else
        {
            const char *s = luaL_checkstring(ls, 2);
            map->tags += " " + trimmed_string(s) + " ";
        }
    }
    PLUARET(string, map->tags.c_str());
}

static int dgn_has_tag(lua_State *ls)
{
    MAP(ls, 1, map);
    PLUARET(boolean, map->has_tag(luaL_checkstring(ls, 2)));
}

static int dgn_tags_remove(lua_State *ls)
{
    MAP(ls, 1, map);

    const int top = lua_gettop(ls);
    for (int i = 2; i <= top; ++i)
    {
        const string axee = luaL_checkstring(ls, i);
        while (strip_tag(map->tags, axee));
    }
    PLUARET(string, map->tags.c_str());
}

static const string level_flag_names[] =
{"no_tele_control", "not_mappable", "no_magic_map", ""};

static int dgn_lflags(lua_State *ls)
{
    MAP(ls, 1, map);

    try
    {
        map->level_flags = map_flags::parse(level_flag_names,
                                            luaL_checkstring(ls, 2));
    }
    catch (const string &error)
    {
        luaL_argerror(ls, 2, error.c_str());
    }

    return 0;
}

static int dgn_change_level_flags(lua_State *ls)
{
    map_flags flags;

    try
    {
        flags = map_flags::parse(level_flag_names,
                                 luaL_checkstring(ls, 1));
    }
    catch (const string &error)
    {
        luaL_argerror(ls, 2, error.c_str());
        lua_pushboolean(ls, false);
        return 1;
    }

    bool silent = lua_toboolean(ls, 2);

    bool changed1 = set_level_flags(flags.flags_set, silent);
    bool changed2 = unset_level_flags(flags.flags_unset, silent);

    lua_pushboolean(ls, changed1 || changed2);

    return 1;
}

static void _chance_magnitude_check(lua_State *ls, int which_par, int chance)
{
    if (chance < 0 || chance > CHANCE_ROLL)
    {
        luaL_argerror(ls, which_par,
                      make_stringf("Chance must be in the range [0,%d]",
                                   CHANCE_ROLL).c_str());
    }
}

static int dgn_chance(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_isnumber(ls, 2))
    {
        const bool has_priority = lua_isnumber(ls, 3);
        const int chance_priority =
            has_priority? luaL_checkint(ls, 2) : DEFAULT_CHANCE_PRIORITY;
        const int chance_par = 2 + has_priority;
        const int chance = luaL_checkint(ls, chance_par);
        _chance_magnitude_check(ls, chance_par, chance);
        map->_chance.set_default(map_chance(chance_priority, chance));
    }
    return 0;
}

static int dgn_depth_chance(lua_State *ls)
{
    MAP(ls, 1, map);
    const string depth(luaL_checkstring(ls, 2));
    const bool has_priority = lua_gettop(ls) == 4;
    const int chance_priority =
        has_priority? luaL_checkint(ls, 3) : DEFAULT_CHANCE_PRIORITY;
    const int chance_par = 3 + has_priority;
    const int chance = luaL_checkint(ls, chance_par);
    _chance_magnitude_check(ls, chance_par, chance);
    try
    {
        map->_chance.add_range(depth, map_chance(chance_priority, chance));
    }
    catch (const string &error)
    {
        luaL_error(ls, error.c_str());
    }
    return 0;
}

#define WEIGHT(ls, n, weight) \
    const int weight = luaL_checkint(ls, n); \
    if (weight < 0)                          \
        luaL_error(ls, "Bad weight: %d (must be >= 0)", weight);

static int dgn_weight(lua_State *ls)
{
    MAP(ls, 1, map);
    if (!lua_isnil(ls, 2))
    {
        WEIGHT(ls, 2, weight);
        map->_weight.set_default(weight);
    }
    return 0;
}

static int dgn_depth_weight(lua_State *ls)
{
    MAP(ls, 1, map);
    const string depth(luaL_checkstring(ls, 2));
    WEIGHT(ls, 3, weight);
    map->_weight.add_range(depth, weight);
    return 0;
}

static int dgn_orient(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) > 1)
    {
        if (lua_isnil(ls, 2))
            map->orient = MAP_NONE;
        else
        {
            const string orient = luaL_checkstring(ls, 2);
            bool found = false;
            // Note: Empty string is intentionally mapped to MAP_NONE!
            for (int i = MAP_NONE; i < MAP_NUM_SECTION_TYPES; ++i)
            {
                if (orient == map_section_name(i))
                {
                    map->orient = static_cast<map_section_type>(i);
                    found = true;
                    break;
                }
            }
            if (!found)
                luaL_error(ls, ("Bad orient: " + orient).c_str());
        }
    }
    PLUARET(string, map_section_name(map->orient));
}

int dgn_map_add_transform(lua_State *ls,
                          string (map_lines::*add)(const string &s))
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) == 1)
        luaL_error(ls, "Expected args, got none.");

    for (int i = 2, size = lua_gettop(ls); i <= size; ++i)
    {
        if (lua_isnil(ls, i))
            luaL_error(ls, "Unexpected nil.");
        else
        {
            string err = (map->map.*add)(luaL_checkstring(ls, i));
            if (!err.empty())
                luaL_error(ls, err.c_str());
        }
    }

    return 0;
}

static int dgn_shuffle(lua_State *ls)
{
    return dgn_map_add_transform(ls, &map_lines::add_shuffle);
}

static int dgn_clear(lua_State *ls)
{
    return dgn_map_add_transform(ls, &map_lines::add_clear);
}

static int dgn_subst(lua_State *ls)
{
    return dgn_map_add_transform(ls, &map_lines::add_subst);
}

static int dgn_nsubst(lua_State *ls)
{
    return dgn_map_add_transform(ls, &map_lines::add_nsubst);
}

static int dgn_colour(lua_State *ls)
{
    return dgn_map_add_transform(ls, &map_lines::add_colour);
}

static int dgn_normalise(lua_State *ls)
{
    MAP(ls, 1, map);
    map->map.normalise();
    return 0;
}

static int dgn_map(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) == 1)
        return clua_stringtable(ls, map->map.get_lines());

    if (lua_isnil(ls, 2))
    {
        map->map.clear();
        return 0;
    }

    // map(<map>, x, y) = glyph at (x,y), subject to map being
    // resolved and normalised.
    if (lua_gettop(ls) == 3 && lua_isnumber(ls, 2) && lua_isnumber(ls, 3))
    {
        const int gly = map->map.glyph(luaL_checkint(ls, 2),
                                       luaL_checkint(ls, 3));
        char buf[2] = "";
        buf[0] = gly;
        lua_pushstring(ls, buf);
        return 1;
    }

    if (lua_isstring(ls, 2))
    {
        map->map.add_line(luaL_checkstring(ls, 2));
        return 0;
    }

    vector<string> &lines = map->map.get_lines();
    int which_line = luaL_checkint(ls, 2);
    if (which_line < 0)
        which_line += (int) lines.size();
    if (lua_gettop(ls) == 2)
    {
        if (which_line < 0 || which_line >= (int) lines.size())
        {
            luaL_error(ls,
                       lines.empty()? "Map is empty"
                       : make_stringf("Line %d out of range (0-%d)",
                                      which_line,
                                      (int)lines.size() - 1).c_str());
        }
        PLUARET(string, lines[which_line].c_str());
    }

    if (lua_isnil(ls, 3))
    {
        if (which_line >= 0 && which_line < (int) lines.size())
        {
            lines.erase(lines.begin() + which_line);
            PLUARET(boolean, true);
        }
        return 0;
    }

    const string newline = luaL_checkstring(ls, 3);
    if (which_line < 0)
    {
        luaL_error(ls,
                   make_stringf("Index %d out of range", which_line).c_str());
    }

    if (which_line < (int) lines.size())
    {
        lines[which_line] = newline;
        return 0;
    }

    lines.reserve(which_line + 1);
    lines.resize(which_line + 1, "");
    lines[which_line] = newline;
    return 0;
}

static int dgn_mons(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) == 1)
        return 0;

    if (lua_isnil(ls, 2))
    {
        map->mons.clear();
        return 0;
    }

    if (lua_isstring(ls, 2))
    {
        string err = map->mons.add_mons(luaL_checkstring(ls, 2));
        if (!err.empty())
            luaL_error(ls, err.c_str());
        return 0;
    }

    const int index = luaL_checkint(ls, 2);
    string err = map->mons.set_mons(index, luaL_checkstring(ls, 3));
    if (!err.empty())
        luaL_error(ls, err.c_str());
    return 0;
}

static int dgn_item(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) == 1)
        return 0;

    if (lua_isnil(ls, 2))
    {
        map->items.clear();
        return 0;
    }

    if (lua_isstring(ls, 2))
    {
        string err = map->items.add_item(luaL_checkstring(ls, 2));
        if (!err.empty())
            luaL_error(ls, err.c_str());
        return 0;
    }

    const int index = luaL_checkint(ls, 2);
    string err = map->items.set_item(index, luaL_checkstring(ls, 3));
    if (!err.empty())
        luaL_error(ls, err.c_str());
    return 0;
}

static int dgn_lua_marker(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) != 3 || !lua_isstring(ls, 2)
        || (!lua_isfunction(ls, 3) && !lua_istable(ls, 3)))
    {
        luaL_error(ls, "Expected marker key and marker function/table.");
    }

    CLua &lvm(CLua::get_vm(ls));
    string key = lua_tostring(ls, 2);
    lua_datum function(lvm, 3, false);

    const string err = map->map.add_lua_marker(key, function);
    if (!err.empty())
        luaL_error(ls, err.c_str());

    return 0;
}

static int dgn_marker(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) == 1)
        return 0;
    if (lua_isnil(ls, 2))
    {
        map->map.clear_markers();
        return 0;
    }

    if (lua_isstring(ls, 2))
    {
        string err = map->map.add_feature_marker(luaL_checkstring(ls, 2));
        if (!err.empty())
            luaL_error(ls, err.c_str());
    }
    return 0;
}

static int dgn_kfeat(lua_State *ls)
{
    MAP(ls, 1, map);
    string err = map->map.add_key_feat(luaL_checkstring(ls, 2));
    if (!err.empty())
        luaL_error(ls, err.c_str());
    return 0;
}

static int dgn_kmons(lua_State *ls)
{
    MAP(ls, 1, map);
    string err = map->map.add_key_mons(luaL_checkstring(ls, 2));
    if (!err.empty())
        luaL_error(ls, err.c_str());
    return 0;
}

static int dgn_kitem(lua_State *ls)
{
    MAP(ls, 1, map);
    string err = map->map.add_key_item(luaL_checkstring(ls, 2));
    if (!err.empty())
        luaL_error(ls, err.c_str());
    return 0;
}

static int dgn_kmask(lua_State *ls)
{
    MAP(ls, 1, map);
    string err = map->map.add_key_mask(luaL_checkstring(ls, 2));
    if (!err.empty())
        luaL_error(ls, err.c_str());
    return 0;
}

static int dgn_kprop(lua_State *ls)
{
    return dgn_map_add_transform(ls, &map_lines::add_fproperty);
}

static int dgn_fheight(lua_State *ls)
{
    return dgn_map_add_transform(ls, &map_lines::add_fheight);
}

static int dgn_map_size(lua_State *ls)
{
    MAP(ls, 1, map);
    lua_pushnumber(ls, map->map.width());
    lua_pushnumber(ls, map->map.height());
    return 2;
}

static int dgn_subvault(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) == 1)
        luaL_error(ls, "Expected args, got none.");

    for (int i = 2, size = lua_gettop(ls); i <= size; ++i)
    {
        if (lua_isnil(ls, i))
            luaL_error(ls, "Unexpected nil.");
        else
        {
            string err = map->subvault_from_tagstring(luaL_checkstring(ls, i));
            if (!err.empty())
                luaL_error(ls, err.c_str());
        }
    }

    return 0;
}

static int dgn_name(lua_State *ls)
{
    MAP(ls, 1, map);
    PLUARET(string, map->name.c_str());
}

typedef
    flood_find<map_def::map_feature_finder, map_def::map_bounds_check>
    map_flood_finder;

static int dgn_map_pathfind(lua_State *ls, int minargs,
                            bool (map_flood_finder::*f)(const coord_def &))
{
    MAP(ls, 1, map);
    const int nargs = lua_gettop(ls);
    if (nargs < minargs)
    {
        return luaL_error
        (ls,
         make_stringf("Not enough points to test connectedness "
                      "(need at least %d)", minargs / 2).c_str());
    }

    map_def::map_feature_finder feat_finder(*map);
    map_def::map_bounds_check bounds_checker(*map);
    map_flood_finder finder(feat_finder, bounds_checker);

    for (int i = 4; i < nargs; i += 2)
    {
        const coord_def c(luaL_checkint(ls, i),
                          luaL_checkint(ls, i + 1));
        finder.add_point(c);
    }

    const coord_def pos(luaL_checkint(ls, 2), luaL_checkint(ls, 3));
    PLUARET(boolean, (finder.*f)(pos));
}

static int dgn_points_connected(lua_State *ls)
{
    return dgn_map_pathfind(ls, 5, &map_flood_finder::points_connected_from);
}

static int dgn_any_point_connected(lua_State *ls)
{
    return dgn_map_pathfind(ls, 5, &map_flood_finder::any_point_connected_from);
}

static int dgn_has_exit_from(lua_State *ls)
{
    return dgn_map_pathfind(ls, 3, &map_flood_finder::has_exit_from);
}

static void dlua_push_coordinates(lua_State *ls, const coord_def &c)
{
    lua_pushnumber(ls, c.x);
    lua_pushnumber(ls, c.y);
}

static int dgn_gly_point(lua_State *ls)
{
    MAP(ls, 1, map);
    coord_def c = map->find_first_glyph(*luaL_checkstring(ls, 2));
    if (c.x != -1 && c.y != -1)
    {
        dlua_push_coordinates(ls, c);
        return 2;
    }
    return 0;
}

static int dgn_gly_points(lua_State *ls)
{
    MAP(ls, 1, map);
    vector<coord_def> cs = map->find_glyph(*luaL_checkstring(ls, 2));

    for (int i = 0, size = cs.size(); i < size; ++i)
        dlua_push_coordinates(ls, cs[i]);
    return cs.size() * 2;
}

static int dgn_original_map(lua_State *ls)
{
    MAP(ls, 1, map);
    if (map->original)
        clua_push_map(ls, map->original);
    else
        lua_pushnil(ls);
    return 1;
}

static int dgn_load_des_file(lua_State *ls)
{
    const string &file = luaL_checkstring(ls, 1);
    if (!file.empty())
        read_map(file);
    return 0;
}

static int dgn_lfloorcol(lua_State *ls)
{
    MAP(ls, 1, map);

    if (!lua_isnone(ls, 2))
    {
        const char *s = luaL_checkstring(ls, 2);
        int colour = str_to_colour(s);

        if (colour < 0 || colour == BLACK)
        {
            string error;

            if (colour == BLACK)
                error = "Can't set floor to black.";
            else
            {
                error = "No such colour as '";
                error += s;
                error += "'";
            }

            luaL_argerror(ls, 2, error.c_str());

            return 0;
        }
        map->floor_colour = colour;
    }
    PLUARET(string, colour_to_str(map->floor_colour).c_str());
}

static int dgn_lrockcol(lua_State *ls)
{
    MAP(ls, 1, map);

    if (!lua_isnone(ls, 2))
    {
        const char *s = luaL_checkstring(ls, 2);
        int colour = str_to_colour(s);

        if (colour < 0 || colour == BLACK)
        {
            string error;

            if (colour == BLACK)
                error = "Can't set rock to black.";
            else
            {
                error = "No such colour as '";
                error += s;
                error += "'";
            }

            luaL_argerror(ls, 2, error.c_str());

            return 0;
        }

        map->rock_colour = colour;
    }
    PLUARET(string, colour_to_str(map->rock_colour).c_str());
}

static int dgn_get_floor_colour(lua_State *ls)
{
    PLUARET(string, colour_to_str(env.floor_colour).c_str());
}

static int dgn_get_rock_colour(lua_State *ls)
{
    PLUARET(string, colour_to_str(env.rock_colour).c_str());
}

static int _lua_colour(lua_State *ls, int ndx,
                       int forbidden_colour = -1)
{
    if (lua_isnumber(ls, ndx))
        return lua_tointeger(ls, ndx);
    else if (const char *s = luaL_checkstring(ls, ndx))
    {
        const int colour = str_to_colour(s);

        if (colour < 0 || colour == forbidden_colour)
        {
            string error;
            if (colour == forbidden_colour)
                error = string("Can't set floor to ") + s;
            else
                error = string("Unknown colour: '") + s + "'";
            return luaL_argerror(ls, 1, error.c_str());
        }
        return colour;
    }
    return luaL_argerror(ls, ndx, "Expected colour name or number");
}

static int dgn_change_floor_colour(lua_State *ls)
{
    const int colour = _lua_colour(ls, 1, BLACK);
    const bool update_now = _lua_boolean(ls, 2, false);

    env.floor_colour = colour;

    if (crawl_state.need_save && update_now)
        viewwindow();
    return 0;
}

static int dgn_change_rock_colour(lua_State *ls)
{
    const int colour = _lua_colour(ls, 1, BLACK);
    const bool update_now = _lua_boolean(ls, 2, false);

    env.rock_colour = colour;

    if (crawl_state.need_save && update_now)
        viewwindow();
    return 0;
}

static int dgn_colour_at(lua_State *ls)
{
    COORDS(c, 1, 2);
    if (!lua_isnone(ls, 3))
        env.grid_colours(c) = _lua_colour(ls, 3);
    PLUARET(string, colour_to_str(env.grid_colours(c)).c_str());
}

static int dgn_register_listener(lua_State *ls)
{
    unsigned mask = luaL_checkint(ls, 1);
    MAPMARKER(ls, 2, mark);
    map_lua_marker *listener = dynamic_cast<map_lua_marker*>(mark);
    coord_def pos;
    // Was a position supplied?
    if (lua_gettop(ls) == 4)
    {
        pos.x = luaL_checkint(ls, 3);
        pos.y = luaL_checkint(ls, 4);
    }

    dungeon_events.register_listener(mask, listener, pos);
    return 0;
}

static int dgn_remove_listener(lua_State *ls)
{
    MAPMARKER(ls, 1, mark);
    map_lua_marker *listener = dynamic_cast<map_lua_marker*>(mark);
    coord_def pos;
    // Was a position supplied?
    if (lua_gettop(ls) == 3)
    {
        pos.x = luaL_checkint(ls, 2);
        pos.y = luaL_checkint(ls, 3);
    }
    dungeon_events.remove_listener(listener, pos);
    return 0;
}

static int dgn_remove_marker(lua_State *ls)
{
    MAPMARKER(ls, 1, mark);
    env.markers.remove(mark);
    return 0;
}

static int dgn_num_matching_markers(lua_State *ls)
{
    const char* key     = luaL_checkstring(ls, 1);
    const char* val_ptr = lua_tostring(ls, 2);
    const char* val;

    if (val_ptr == nullptr)
        val = "";
    else
        val = val_ptr;

    vector<map_marker*> markers = env.markers.get_all(key, val);

    PLUARET(number, markers.size());
}

static int dgn_terrain_changed(lua_State *ls)
{
    dungeon_feature_type type = DNGN_UNSEEN;
    if (lua_isnumber(ls, 3))
        type = static_cast<dungeon_feature_type>(luaL_checkint(ls, 3));
    else if (lua_isstring(ls, 3))
        type = dungeon_feature_by_name(lua_tostring(ls, 3));
    const bool affect_player =
    lua_isboolean(ls, 4)? lua_toboolean(ls, 4) : true;
    const bool preserve_features =
    lua_isboolean(ls, 5)? lua_toboolean(ls, 5) : true;
    const bool preserve_items =
    lua_isboolean(ls, 6)? lua_toboolean(ls, 6) : true;
    dungeon_terrain_changed(coord_def(luaL_checkint(ls, 1),
                                       luaL_checkint(ls, 2)),
                            type, affect_player,
                            preserve_features, preserve_items);
    return 0;
}

static int dgn_fprop_changed(lua_State *ls)
{
    feature_property_type prop = FPROP_NONE;

    if (lua_isnumber(ls, 3))
        prop = static_cast<feature_property_type>(luaL_checkint(ls, 3));
    else if (lua_isstring(ls, 3))
        prop = str_to_fprop(lua_tostring(ls, 3));

    coord_def pos = coord_def(luaL_checkint(ls, 1), luaL_checkint(ls, 2));

    if (in_bounds(pos) && prop != FPROP_NONE)
    {
        if (testbits(env.pgrid(pos), prop))
        {
            env.pgrid(pos) &= ~prop;
            lua_pushboolean(ls, true);
        }
        else if (!testbits(env.pgrid(pos), prop))
        {
            env.pgrid(pos) |= prop;
            lua_pushboolean(ls, true);
        }
        else
            lua_pushboolean(ls, false);
    }
    else
        lua_pushboolean(ls, false);

    return 1;
}

static int dgn_fprop_at(lua_State *ls)
{
    feature_property_type prop = FPROP_NONE;

    if (lua_isnumber(ls, 3))
        prop = static_cast<feature_property_type>(luaL_checkint(ls, 3));
    else if (lua_isstring(ls, 3))
        prop = str_to_fprop(lua_tostring(ls, 3));

    coord_def pos = coord_def(luaL_checkint(ls, 1), luaL_checkint(ls, 2));

    if (in_bounds(pos) && prop != FPROP_NONE)
        lua_pushboolean(ls, testbits(env.pgrid(pos), prop));
    else
        lua_pushboolean(ls, false);

    return 1;
}

static int dgn_cloud_at(lua_State *ls)
{
    COORDS(c, 1, 2);

    if (!in_bounds(c))
        return 0;

    int cloudno = env.cgrid(c);

    if (cloudno == EMPTY_CLOUD)
        lua_pushstring(ls, "none");
    else
        lua_pushstring(ls, cloud_name_at_index(cloudno).c_str());

    return 1;
}

static int lua_dgn_set_branch_epilogue(lua_State *ls)
{
    const char *branch_name = luaL_checkstring(ls, 1);

    if (!branch_name)
        return 0;

    branch_type br = str_to_branch(branch_name);
    if (br == NUM_BRANCHES)
    {
        luaL_error(ls, make_stringf("unknown branch: '%s'.", branch_name).c_str());
        return 0;
    }

    const char *func_name = luaL_checkstring(ls, 2);

    if (!func_name || !*func_name)
        return 0;

    dgn_set_branch_epilogue(br, func_name);

    return 0;
}

// XXX: Currently, this is hacked so that map_def->border_fill_type is marshalled
//      when the maps are stored. This relies on the individual map Lua prelude
//      being executed whenever maps are loaded and verified, which means that
//      the next time the map is loaded, border_fill_type is already stored.
static int lua_dgn_set_border_fill_type(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) != 2)
        luaL_error(ls, "set_border_fill_type requires a feature.");

    string fill_string = luaL_checkstring(ls, 2);
    dungeon_feature_type fill_type = dungeon_feature_by_name(fill_string);

    if (fill_type == DNGN_UNSEEN)
    {
        luaL_error(ls, ("unknown feature '" + fill_string + "'.").c_str());
        return 0;
    }

    if (feat_is_valid_border(fill_type))
        map->border_fill_type = fill_type;
    else
        luaL_error(ls, ("set_border_fill_type cannot be the feature '" +
                         fill_string +"'.").c_str());

    return 0;
}

static int lua_dgn_set_feature_name(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) != 3)
        luaL_error(ls, "set_feature_name takes a feature and the new name.");

    string feat_string = luaL_checkstring(ls, 2);
    dungeon_feature_type feat_type = dungeon_feature_by_name(feat_string);

    if (feat_type == DNGN_UNSEEN)
    {
        luaL_error(ls, ("unknown feature '" + feat_string + "'.").c_str());
        return 0;
    }

    map->feat_renames[feat_type] = luaL_checkstring(ls, 3);

    return 0;
}

static int dgn_floor_halo(lua_State *ls)
{
    string error = "";

    const char *s1 = luaL_checkstring(ls, 1);
    const dungeon_feature_type target = dungeon_feature_by_name(s1);

    if (target == DNGN_UNSEEN)
    {
        error += "No such dungeon feature as '";
        error += s1;
        error += "'.  ";
    }

    const char *s2 = luaL_checkstring(ls, 2);
    short colour = str_to_colour(s2);

    if (colour == -1)
    {
        error += "No such colour as '";
        error += s2;
        error += "'.";
    }
    else if (colour == BLACK)
        error += "Can't set floor colour to black.";

    if (!error.empty())
    {
        luaL_argerror(ls, 2, error.c_str());
        return 0;
    }

    for (rectangle_iterator ri(0); ri; ++ri)
    {
        if (grd(*ri) == target)
        {
            for (adjacent_iterator ai(*ri, false); ai; ++ai)
            {
                if (!map_bounds(*ai))
                    continue;

                const dungeon_feature_type feat2 = grd(*ai);

                if (feat2 == DNGN_FLOOR || feat2 == DNGN_UNDISCOVERED_TRAP)
                    env.grid_colours(*ai) = colour;
            }
        }
    }

    unsigned int tile = get_tile_idx(ls, 3);
    if (!tile)
        return 0;
    if (tile_dngn_count(tile) != 9)
    {
        error += "'";
        error += luaL_checkstring(ls, 3);
        error += "' is not a valid halo tile. It has ";
        error += tile_dngn_count(tile);
        error += " variations, but needs exactly 9.";
        luaL_argerror(ls, 3, error.c_str());
        return 0;
    }

    tile_floor_halo(target, tile);

    return 0;
}

#define SQRT_2 1.41421356237309504880

static int dgn_random_walk(lua_State *ls)
{
    const int x     = luaL_checkint(ls, 1);
    const int y     = luaL_checkint(ls, 2);
    const int dist = luaL_checkint(ls, 3);

    // Fourth param being true means that we can move past
    // statues.
    const dungeon_feature_type minmove =
    lua_isnil(ls, 4) ? DNGN_LAVA : DNGN_ORCISH_IDOL;

    if (!in_bounds(x, y))
    {
        char buf[80];
        sprintf(buf, "Point (%d,%d) isn't in bounds.", x, y);
        luaL_argerror(ls, 1, buf);
        return 0;
    }
    if (dist < 1)
    {
        luaL_argerror(ls, 3, "Distance must be positive.");
        return 0;
    }

    float dist_left = dist;
    // Allow movement to all 8 adjacent squares if distance is 1
    // (needed since diagonal moves are distance sqrt(2))
    if (dist == 1)
        dist_left = (float)SQRT_2;

    int moves_left = dist;
    coord_def pos(x, y);
    while (dist_left >= 1.0 && moves_left-- > 0)
    {
        int okay_dirs = 0;
        int dir       = -1;
        for (int j = 0; j < 8; j++)
        {
            const coord_def new_pos   = pos + Compass[j];
            const float     move_dist = (j % 2 == 0) ? 1.0 : SQRT_2;

            if (in_bounds(new_pos) && grd(new_pos) >= minmove
                && move_dist <= dist_left)
            {
                if (one_chance_in(++okay_dirs))
                    dir = j;
            }
        }

        if (okay_dirs == 0)
            break;

        if (one_chance_in(++okay_dirs))
            continue;

        pos       += Compass[dir];
        dist_left -= (dir % 2 == 0) ? 1.0 : SQRT_2;
    }

    dlua_push_coordinates(ls, pos);

    return 2;
}

static kill_category dgn_kill_name_to_category(string name)
{
    if (name.empty())
        return KC_OTHER;

    lowercase(name);

    if (name == "you")
        return KC_YOU;
    else if (name == "friendly")
        return KC_FRIENDLY;
    else if (name == "other")
        return KC_OTHER;
    else
        return KC_NCATEGORIES;
}

static int lua_cloud_pow_min;
static int lua_cloud_pow_max;
static int lua_cloud_pow_rolls;

static int make_a_lua_cloud(coord_def where, int /*garbage*/, int spread_rate,
                            cloud_type ctype, const actor *agent, int colour,
                            string name, string tile, int excl_rad)
{
    const int pow = random_range(lua_cloud_pow_min,
                                 lua_cloud_pow_max,
                                 lua_cloud_pow_rolls);
    place_cloud(ctype, where, pow, agent, spread_rate, colour, name, tile, excl_rad);
    return 1;
}

static int dgn_apply_area_cloud(lua_State *ls)
{
    const int x         = luaL_checkint(ls, 1);
    const int y         = luaL_checkint(ls, 2);
    lua_cloud_pow_min   = luaL_checkint(ls, 3);
    lua_cloud_pow_max   = luaL_checkint(ls, 4);
    lua_cloud_pow_rolls = luaL_checkint(ls, 5);
    const int size      = luaL_checkint(ls, 6);

    const cloud_type ctype = cloud_name_to_type(luaL_checkstring(ls, 7));
    const char*      kname = lua_isstring(ls, 8) ? luaL_checkstring(ls, 8)
    : "";
    const kill_category kc = dgn_kill_name_to_category(kname);

    const int spread_rate = lua_isnumber(ls, 9) ? luaL_checkint(ls, 9) : -1;

    const int colour    = lua_isstring(ls, 10) ? str_to_colour(luaL_checkstring(ls, 10)) : -1;
    string name = lua_isstring(ls, 11) ? luaL_checkstring(ls, 11) : "";
    string tile = lua_isstring(ls, 12) ? luaL_checkstring(ls, 12) : "";
    const int excl_rad = lua_isnumber(ls, 13) ? luaL_checkint(ls, 13) : -1;

    if (!in_bounds(x, y))
    {
        char buf[80];
        sprintf(buf, "Point (%d,%d) isn't in bounds.", x, y);
        luaL_argerror(ls, 1, buf);
        return 0;
    }

    if (lua_cloud_pow_min < 0)
    {
        luaL_argerror(ls, 4, "pow_min must be non-negative");
        return 0;
    }

    if (lua_cloud_pow_max < lua_cloud_pow_min)
    {
        luaL_argerror(ls, 5, "pow_max must not be less than pow_min");
        return 0;
    }

    if (lua_cloud_pow_max == 0)
    {
        luaL_argerror(ls, 5, "pow_max must be positive");
        return 0;
    }

    if (lua_cloud_pow_rolls <= 0)
    {
        luaL_argerror(ls, 6, "pow_rolls must be positive");
        return 0;
    }

    if (size < 1)
    {
        luaL_argerror(ls, 4, "size must be positive.");
        return 0;
    }

    if (ctype == CLOUD_NONE)
    {
        string error = "Invalid cloud type '";
        error += luaL_checkstring(ls, 7);
        error += "'";
        luaL_argerror(ls, 7, error.c_str());
        return 0;
    }

    if (kc == KC_NCATEGORIES || kc != KC_OTHER)
    {
        string error = "Invalid kill category '";
        error += kname;
        error += "'";
        luaL_argerror(ls, 8, error.c_str());
        return 0;
    }

    if (spread_rate < -1 || spread_rate > 100)
    {
        luaL_argerror(ls, 9, "spread_rate must be between -1 and 100,"
                      "inclusive");
        return 0;
    }

    apply_area_cloud(make_a_lua_cloud, coord_def(x, y), 0, size,
                     ctype, 0, spread_rate, colour, name, tile,
                     excl_rad);

    return 0;
}

static int dgn_delete_cloud(lua_State *ls)
{
    COORDS(c, 1, 2);

    if (in_bounds(c) && env.cgrid(c) != EMPTY_CLOUD)
        delete_cloud(env.cgrid(c));

    return 0;
}

static int dgn_place_cloud(lua_State *ls)
{
    const int x         = luaL_checkint(ls, 1);
    const int y         = luaL_checkint(ls, 2);
    const cloud_type ctype = cloud_name_to_type(luaL_checkstring(ls, 3));
    const int cl_range      = luaL_checkint(ls, 4);
    const char*      kname = lua_isstring(ls, 5) ? luaL_checkstring(ls, 5)
    : "";
    const kill_category kc = dgn_kill_name_to_category(kname);

    const int spread_rate = lua_isnumber(ls, 6) ? luaL_checkint(ls, 6) : -1;

    const int colour    = lua_isstring(ls, 7) ? str_to_colour(luaL_checkstring(ls, 7)) : -1;
    string name = lua_isstring(ls, 8) ? luaL_checkstring(ls, 8) : "";
    string tile = lua_isstring(ls, 9) ? luaL_checkstring(ls, 9) : "";
    const int excl_rad = lua_isnumber(ls, 10) ? luaL_checkint(ls, 10) : -1;

    if (!in_bounds(x, y))
    {
        char buf[80];
        sprintf(buf, "Point (%d,%d) isn't in bounds.", x, y);
        luaL_argerror(ls, 1, buf);
        return 0;
    }

    if (ctype == CLOUD_NONE)
    {
        string error = "Invalid cloud type '";
        error += luaL_checkstring(ls, 3);
        error += "'";
        luaL_argerror(ls, 3, error.c_str());
        return 0;
    }

    if (kc == KC_NCATEGORIES || kc != KC_OTHER)
    {
        string error = "Invalid kill category '";
        error += kname;
        error += "'";
        luaL_argerror(ls, 5, error.c_str());
        return 0;
    }

    if (spread_rate < -1 || spread_rate > 100)
    {
        luaL_argerror(ls, 6, "spread_rate must be between -1 and 100,"
                      "inclusive");
        return 0;
    }

    place_cloud(ctype, coord_def(x, y), cl_range, 0, spread_rate, colour, name, tile, excl_rad);

    return 0;
}

// XXX: Doesn't allow for messages or specifying the noise source.
LUAFN(dgn_noisy)
{
    const int loudness = luaL_checkint(ls, 1);
    COORDS(pos, 2, 3);

    noisy(loudness, pos);

    return 0;
}

static int _dgn_is_passable(lua_State *ls)
{
    COORDS(c, 1, 2);
    lua_pushboolean(ls, dgn_square_travel_ok(c));
    return 1;
}

static int dgn_register_feature_marker(lua_State *ls)
{
    COORDS(c, 1, 2);
    FEAT(feat, 3);
    env.markers.add(new map_feature_marker(c, feat));
    return 0;
}

static int _dgn_map_register_flag(lua_State *ls)
{
    map_register_flag(luaL_checkstring(ls, 1));
    return 0;
}

static int dgn_register_lua_marker(lua_State *ls)
{
    COORDS(c, 1, 2);
    if (!lua_istable(ls, 3) && !lua_isfunction(ls, 3))
        return luaL_argerror(ls, 3, "Expected marker table or function");

    lua_datum table(CLua::get_vm(ls), 3, false);
    map_marker *marker = new map_lua_marker(table);
    marker->pos = c;
    env.markers.add(marker);
    return 0;
}

static unique_ptr<lua_datum> _dgn_map_safe_bounds_fn;

static bool _lua_map_place_valid(const map_def &map,
                                 const coord_def &c,
                                 const coord_def &size)
{
    dprf("lua_map_place_invalid: (%d,%d) (%d,%d)",
         c.x, c.y, size.x, size.y);

    lua_stack_cleaner clean(_dgn_map_safe_bounds_fn->lua);

    // Push the Lua function onto the stack.
    _dgn_map_safe_bounds_fn->push();

    lua_State *ls = _dgn_map_safe_bounds_fn->lua;

    // Push map, pos.x, pos.y, size.x, size.y
    clua_push_map(ls, const_cast<map_def*>(&map));
    dlua_push_coordinates(ls, c);
    dlua_push_coordinates(ls, size);

    const int err = lua_pcall(ls, 5, 1, 0);

    // Lua error invalidates place.
    if (err)
    {
        mprf(MSGCH_ERROR, "Lua error: %s", lua_tostring(ls, -1));
        return true;
    }

    return lua_toboolean(ls, -1);
}

LUAFN(dgn_with_map_bounds_fn)
{
    CLua &vm(CLua::get_vm(ls));
    if (lua_gettop(ls) != 2 || !lua_isfunction(ls, 1) || !lua_isfunction(ls, 2))
        luaL_error(ls, "Expected map-bounds check fn and action fn.");

    _dgn_map_safe_bounds_fn.reset(new lua_datum(vm, 1, false));

    int err = 0;
    {
        unwind_var<map_place_check_t> mpc(map_place_valid,
                                          _lua_map_place_valid);

        // All set, call our friend, the second function.
        ASSERT(lua_isfunction(ls, -1));

        // Copy the function since pcall will pop it off.
        lua_pushvalue(ls, -1);

        // Use pcall to catch the error here, else unwind_var won't
        // happen when lua_call does its longjmp.
        err = lua_pcall(ls, 0, 1, 0);

        _dgn_map_safe_bounds_fn.reset(nullptr);
    }

    if (err)
        lua_error(ls);

    return 1;
}

// Accepts any number of point coordinates and a function, binds the
// points as anchors that floating vaults must match and calls the
// function, returning the return value of the function.
LUAFN(dgn_with_map_anchors)
{
    const int top = lua_gettop(ls);
    int err = 0;
    {
        unwind_var<point_vector> uanchor(map_anchor_points);

        map_anchor_points.clear();

        int i;
        for (i = 1; i < top; i += 2)
        {
            if (lua_isnumber(ls, i) && lua_isnumber(ls, i + 1))
            {
                map_anchor_points.emplace_back(lua_tointeger(ls, i),
                                               lua_tointeger(ls, i + 1));
            }
        }

        ASSERT(lua_isfunction(ls, -1));

        lua_pushvalue(ls, -1);
        err = lua_pcall(ls, 0, 1, 0);
    }
    if (err)
        lua_error(ls);
    return 1;
}

static int _lua_push_map(lua_State *ls, const map_def *map)
{
    if (map)
        clua_push_map(ls, const_cast<map_def*>(map));
    else
        lua_pushnil(ls);
    return 1;
}

LUAFN(dgn_map_by_tag)
{
    if (const char *tag = luaL_checkstring(ls, 1))
    {
        const bool check_depth = _lua_boolean(ls, 3, true);
        return _lua_push_map(ls, random_map_for_tag(tag, check_depth));
    }
    return 0;
}

LUAFN(dgn_map_by_name)
{
    if (const char *name = luaL_checkstring(ls, 1))
        return _lua_push_map(ls, find_map_by_name(name));

    return 0;
}

LUAFN(dgn_map_in_depth)
{
    const level_id lid = dlua_level_id(ls, 1);
    const bool mini = _lua_boolean(ls, 2, true);
    return _lua_push_map(ls, random_map_in_depth(lid, mini));
}

LUAFN(dgn_map_by_place)
{
    const level_id lid = dlua_level_id(ls, 1);
    const bool mini = _lua_boolean(ls, 2, false);
    return _lua_push_map(ls, random_map_for_place(lid, mini));
}

LUAFN(_dgn_place_map)
{
    MAP(ls, 1, map);
    const bool check_collision = _lua_boolean(ls, 2, true);
    const bool no_exits = _lua_boolean(ls, 3, false);
    coord_def where(-1, -1);
    if (lua_isnumber(ls, 4) && lua_isnumber(ls, 5))
    {
        COORDS(c, 4, 5);
        where = c;
    }
    {
        dgn_map_parameters mp(lua_gettop(ls) >= 6
                              ? luaL_checkstring(ls, 6)
                              : "");
        if (dgn_place_map(map, check_collision, no_exits, where)
            && !env.level_vaults.empty())
        {
            lua_pushlightuserdata(ls,
                                  env.level_vaults[env.level_vaults.size() - 1]);
        }
        else
            lua_pushnil(ls);
    }
    return 1;
}

LUAFN(_dgn_in_vault)
{
    GETCOORD(c, 1, 2, map_bounds);
    const int mask = lua_isnone(ls, 3) ? MMT_VAULT : lua_tointeger(ls, 3);
    lua_pushboolean(ls, env.level_map_mask(c) & mask);
    return 1;
}

LUAFN(_dgn_set_map_mask)
{
    GETCOORD(c, 1, 2, map_bounds);
    const int mask = lua_isnone(ls, 3) ? MMT_VAULT : lua_tointeger(ls, 3);
    env.level_map_mask(c) |= mask;
    return 1;
}

LUAFN(_dgn_unset_map_mask)
{
    GETCOORD(c, 1, 2, map_bounds);
    const int mask = lua_isnone(ls, 3) ? MMT_VAULT : lua_tointeger(ls, 3);
    env.level_map_mask(c) &= ~mask;
    return 1;
}

LUAFN(_dgn_map_parameters)
{
    return clua_stringtable(ls, map_parameters);
}

static int _dgn_push_vault_placement(lua_State *ls, const vault_placement *vp)
{
    return dlua_push_object_type(ls, VAULT_PLACEMENT_METATABLE, *vp);
}

LUAFN(_dgn_maps_used_here)
{
    return clua_gentable(ls, env.level_vaults, _dgn_push_vault_placement);
}

LUAFN(_dgn_vault_at)
{
    GETCOORD(c, 1, 2, map_bounds);
    vault_placement *place = dgn_vault_at(c);
    if (place)
        _dgn_push_vault_placement(ls, place);
    else
        lua_pushnil(ls);

    return 1;
}

LUAFN(_dgn_find_marker_position_by_prop)
{
    const char *prop = luaL_checkstring(ls, 1);
    const string value(lua_gettop(ls) >= 2 ? luaL_checkstring(ls, 2) : "");
    const coord_def place = find_marker_position_by_prop(prop, value);
    if (map_bounds(place))
        dlua_push_coordinates(ls, place);
    else
    {
        lua_pushnil(ls);
        lua_pushnil(ls);
    }
    return 2;
}

LUAFN(_dgn_find_marker_positions_by_prop)
{
    const char *prop = luaL_checkstring(ls, 1);
    const string value(lua_gettop(ls) >= 2 ? luaL_checkstring(ls, 2) : "");
    const unsigned limit(lua_gettop(ls) >= 3 ? luaL_checkint(ls, 3) : 0);
    const vector<coord_def> places = find_marker_positions_by_prop(prop, value,
                                                                   limit);
    clua_gentable(ls, places, clua_pushpoint);
    return 1;
}

static int _push_mapmarker(lua_State *ls, map_marker *marker)
{
    dlua_push_userdata(ls, marker, MAPMARK_METATABLE);
    return 1;
}

LUAFN(_dgn_find_markers_by_prop)
{
    const char *prop = luaL_checkstring(ls, 1);
    const string value(lua_gettop(ls) >= 2 ? luaL_checkstring(ls, 2) : "");
    const unsigned limit(lua_gettop(ls) >= 3 ? luaL_checkint(ls, 3) : 0);
    const vector<map_marker*> places = find_markers_by_prop(prop, value, limit);
    clua_gentable(ls, places, _push_mapmarker);
    return 1;
}

LUAFN(_dgn_marker_at_pos)
{
    const int x = luaL_checkint(ls, 1);
    const int y = luaL_checkint(ls, 2);

    coord_def p(x, y);

    map_marker* marker = env.markers.find(p);

    if (marker == nullptr)
        lua_pushnil(ls);
    else
        _push_mapmarker(ls, marker);

    return 1;
}

LUAFN(dgn_is_validating)
{
    MAP(ls, 1, map);
    lua_pushboolean(ls, map->is_validating());
    return 1;
}

LUAFN(_dgn_resolve_map)
{
    if (lua_isnil(ls, 1))
    {
        lua_pushnil(ls);
        return 1;
    }

    MAP(ls, 1, map);
    const bool check_collisions = _lua_boolean(ls, 2, true);

    // Save the vault_placement into Temp_Vaults because the map_def
    // will need to be alive through to the end of dungeon gen.
    Temp_Vaults.emplace_back();
    vault_placement &place(Temp_Vaults.back());

    if (vault_main(place, map, check_collisions) != MAP_NONE)
    {
        clua_push_map(ls, &place.map);
        lua_pushlightuserdata(ls, &place);
    }
    else
    {
        lua_pushnil(ls);
        lua_pushnil(ls);
    }
    return 2;
}

LUAFN(_dgn_reuse_map)
{
    if (!lua_isuserdata(ls, 1))
        luaL_argerror(ls, 1, "Expected vault_placement");

    vault_placement &vp(*static_cast<vault_placement*>(lua_touserdata(ls, 1)));

    COORDS(place, 2, 3);

    const bool flip_horiz = _lua_boolean(ls, 4, false);
    const bool flip_vert = _lua_boolean(ls, 5, false);

    // 1 for clockwise, -1 for anticlockwise, 0 for no rotation.
    const int rotate_dir = lua_isnone(ls, 6) ? 0 : luaL_checkint(ls, 6);

    const bool register_place = _lua_boolean(ls, 7, true);
    const bool register_vault = register_place && _lua_boolean(ls, 8, false);

    if (flip_horiz)
        vp.map.hmirror();
    if (flip_vert)
        vp.map.vmirror();
    if (rotate_dir)
        vp.map.rotate(rotate_dir == 1);

    vp.size = vp.map.map.size();

    // draw_at changes vault_placement.
    vp.draw_at(place);

    if (register_place)
        dgn_register_place(vp, register_vault);

    return 0;
}

// dgn.inspect_map(vplace,x,y)
//
// You must first have resolved a map to get the vault_placement, using
// dgn.resolve_map(..). This function will then inspect a coord on that map
// where <0,0> is the top-left cell and tell you the feature type. This will
// respect all KFEAT and other map directives; and SUBST and other
// transformations will have already taken place during resolve_map.
LUAFN(_dgn_inspect_map)
{
    if (!lua_isuserdata(ls, 1))
        luaL_argerror(ls, 1, "Expected vault_placement");

    vault_placement &vp(*static_cast<vault_placement*>(lua_touserdata(ls, 1)));

    // Not using the COORDS macro because it checks against in_bounds which will fail 0,0 (!)
    coord_def c;
    c.x = luaL_checkint(ls, 2);
    c.y = luaL_checkint(ls, 3);

    lua_pushnumber(ls, vp.feature_at(c));
    lua_pushboolean(ls, vp.is_exit(c));
    lua_pushboolean(ls, vp.is_space(c));
    return 3;
}

LUAWRAP(_dgn_reset_level, dgn_reset_level())

LUAFN(dgn_fill_grd_area)
{
    int x1 = luaL_checkint(ls, 1);
    int y1 = luaL_checkint(ls, 2);
    int x2 = luaL_checkint(ls, 3);
    int y2 = luaL_checkint(ls, 4);
    dungeon_feature_type feat = check_lua_feature(ls, 5);

    x1 = min(max(x1, X_BOUND_1+1), X_BOUND_2-1);
    y1 = min(max(y1, Y_BOUND_1+1), Y_BOUND_2-1);
    x2 = min(max(x2, X_BOUND_1+1), X_BOUND_2-1);
    y2 = min(max(y2, Y_BOUND_1+1), Y_BOUND_2-1);

    if (x2 < x1)
        swap(x1, x2);
    if (y2 < y1)
        swap(y1, y2);

    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            grd[x][y] = feat;

    return 0;
}

LUAFN(dgn_apply_tide)
{
    shoals_apply_tides(0, true, true);
    return 0;
}

const struct luaL_reg dgn_dlib[] =
{
{ "reset_level", _dgn_reset_level },

{ "default_depth", dgn_default_depth },
{ "name", dgn_name },
{ "depth", dgn_depth },
{ "place", dgn_place },
{ "desc", dgn_desc },
{ "order", dgn_order },
{ "tags",  dgn_tags },
{ "has_tag", dgn_has_tag },
{ "tags_remove", dgn_tags_remove },
{ "lflags", dgn_lflags },
{ "chance", dgn_chance },
{ "depth_chance", dgn_depth_chance },
{ "weight", dgn_weight },
{ "depth_weight", dgn_depth_weight },
{ "orient", dgn_orient },
{ "shuffle", dgn_shuffle },
{ "clear", dgn_clear },
{ "subst", dgn_subst },
{ "nsubst", dgn_nsubst },
{ "colour", dgn_colour },
{ "lfloorcol", dgn_lfloorcol},
{ "lrockcol", dgn_lrockcol},
{ "normalise", dgn_normalise },
{ "map", dgn_map },
{ "mons", dgn_mons },
{ "item", dgn_item },
{ "marker", dgn_marker },
{ "lua_marker", dgn_lua_marker },
{ "kfeat", dgn_kfeat },
{ "kitem", dgn_kitem },
{ "kmons", dgn_kmons },
{ "kprop", dgn_kprop },
{ "kmask", dgn_kmask },
{ "mapsize", dgn_map_size },
{ "subvault", dgn_subvault },
{ "fheight", dgn_fheight },

{ "colour_at", dgn_colour_at },
{ "fprop_at", dgn_fprop_at },
{ "cloud_at", dgn_cloud_at },

{ "terrain_changed", dgn_terrain_changed },
{ "fprop_changed", dgn_fprop_changed },
{ "points_connected", dgn_points_connected },
{ "any_point_connected", dgn_any_point_connected },
{ "has_exit_from", dgn_has_exit_from },
{ "gly_point", dgn_gly_point },
{ "gly_points", dgn_gly_points },
{ "original_map", dgn_original_map },
{ "load_des_file", dgn_load_des_file },
{ "register_listener", dgn_register_listener },
{ "remove_listener", dgn_remove_listener },
{ "remove_marker", dgn_remove_marker },
{ "num_matching_markers", dgn_num_matching_markers },
{ "change_level_flags", dgn_change_level_flags },
{ "get_floor_colour", dgn_get_floor_colour },
{ "get_rock_colour",  dgn_get_rock_colour },
{ "change_floor_colour", dgn_change_floor_colour },
{ "change_rock_colour",  dgn_change_rock_colour },
{ "set_branch_epilogue", lua_dgn_set_branch_epilogue },
{ "set_border_fill_type", lua_dgn_set_border_fill_type },
{ "set_feature_name", lua_dgn_set_feature_name },
{ "floor_halo", dgn_floor_halo },
{ "random_walk", dgn_random_walk },
{ "apply_area_cloud", dgn_apply_area_cloud },
{ "delete_cloud", dgn_delete_cloud },
{ "place_cloud", dgn_place_cloud },
{ "noisy", dgn_noisy },

{ "is_passable", _dgn_is_passable },

{ "map_register_flag", _dgn_map_register_flag },
{ "register_feature_marker", dgn_register_feature_marker },
{ "register_lua_marker", dgn_register_lua_marker },

{ "with_map_bounds_fn", dgn_with_map_bounds_fn },
{ "with_map_anchors", dgn_with_map_anchors },

{ "map_by_tag", dgn_map_by_tag },
{ "map_by_name", dgn_map_by_name },
{ "map_in_depth", dgn_map_in_depth },
{ "map_by_place", dgn_map_by_place },
{ "place_map", _dgn_place_map },
{ "reuse_map", _dgn_reuse_map },
{ "resolve_map", _dgn_resolve_map },
{ "inspect_map", _dgn_inspect_map },
{ "in_vault", _dgn_in_vault },
{ "set_map_mask", _dgn_set_map_mask },
{ "unset_map_mask", _dgn_unset_map_mask },

{ "map_parameters", _dgn_map_parameters },

{ "maps_used_here", _dgn_maps_used_here },
{ "vault_at", _dgn_vault_at },

{ "find_marker_position_by_prop", _dgn_find_marker_position_by_prop },
{ "find_marker_positions_by_prop", _dgn_find_marker_positions_by_prop },
{ "find_markers_by_prop", _dgn_find_markers_by_prop },

{ "marker_at_pos", _dgn_marker_at_pos },

{ "is_validating", dgn_is_validating },

{ "fill_grd_area", dgn_fill_grd_area },

{ "apply_tide", dgn_apply_tide },

{ nullptr, nullptr }
};

#define VP(name) \
    vault_placement &name =                                             \
        **clua_get_userdata<vault_placement*>(                       \
            ls, VAULT_PLACEMENT_METATABLE)

LUAFN(_vp_pos)
{
    VP(vp);
    clua_pushpoint(ls, vp.pos);
    return 1;
}

LUAFN(_vp_size)
{
    VP(vp);
    clua_pushpoint(ls, vp.size);
    return 1;
}

LUAFN(_vp_orient)
{
    VP(vp);
    PLUARET(number, vp.orient)
}

LUAFN(_vp_map)
{
    VP(vp);
    clua_push_map(ls, &vp.map);
    return 1;
}

LUAFN(_vp_exits)
{
    VP(vp);
    return clua_gentable(ls, vp.exits, clua_pushpoint);
}

static const luaL_reg dgn_vaultplacement_ops[] =
{
    { "pos", _vp_pos },
    { "size", _vp_size },
    { "orient", _vp_orient },
    { "map", _vp_map },
    { "exits", _vp_exits },
    { nullptr, nullptr }
};

static void _dgn_register_metatables(lua_State *ls)
{
    clua_register_metatable(ls,
                            VAULT_PLACEMENT_METATABLE,
                            dgn_vaultplacement_ops,
                            lua_object_gc<vault_placement>);
}

void dluaopen_dgn(lua_State *ls)
{
    _dgn_register_metatables(ls);

    luaL_openlib(ls, "dgn", dgn_dlib, 0);
    luaL_openlib(ls, "dgn", dgn_build_dlib, 0);
    luaL_openlib(ls, "dgn", dgn_event_dlib, 0);
    luaL_openlib(ls, "dgn", dgn_grid_dlib, 0);
    luaL_openlib(ls, "dgn", dgn_item_dlib, 0);
    luaL_openlib(ls, "dgn", dgn_level_dlib, 0);
    luaL_openlib(ls, "dgn", dgn_mons_dlib, 0);
    luaL_openlib(ls, "dgn", dgn_subvault_dlib, 0);
    luaL_openlib(ls, "dgn", dgn_tile_dlib, 0);
}
