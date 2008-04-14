/*
 *  File:       luadgn.cc
 *  Summary:    Dungeon-builder Lua interface.
 *  Created by: dshaligram on Sat Jun 23 20:02:09 2007 UTC
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"

#include <sstream>
#include <algorithm>

#include "branch.h"
#include "clua.h"
#include "cloud.h"
#include "direct.h"
#include "dungeon.h"
#include "files.h"
#include "initfile.h"
#include "items.h"
#include "luadgn.h"
#include "mapdef.h"
#include "mapmark.h"
#include "maps.h"
#include "misc.h"
#include "spl-util.h"
#include "stuff.h"
#include "tags.h"
#include "terrain.h"
#include "view.h"

template <typename list, typename lpush>
static int dlua_gentable(lua_State *ls, const list &strings, lpush push)
{
    lua_newtable(ls);
    for (int i = 0, size = strings.size(); i < size; ++i)
    {
        push(ls, strings[i]);
        lua_rawseti(ls, -2, i + 1);
    }
    return (1);
}

inline static void dlua_pushcxxstring(lua_State *ls, const std::string &s)
{
    lua_pushstring(ls, s.c_str());
}

int dlua_stringtable(lua_State *ls, const std::vector<std::string> &s)
{
    return dlua_gentable(ls, s, dlua_pushcxxstring);
}

static int dlua_compiled_chunk_writer(lua_State *ls, const void *p,
                                      size_t sz, void *ud)
{
    std::ostringstream &out = *static_cast<std::ostringstream*>(ud);
    out.write((const char *) p, sz);
    return (0);
}

///////////////////////////////////////////////////////////////////////////
// dlua_chunk

dlua_chunk::dlua_chunk(const std::string &_context)
    : file(), chunk(), compiled(), context(_context), first(-1),
      last(-1), error()
{
    clear();
}

// Initialises a chunk from the function on the top of stack.
// This function must not be a closure, i.e. must not have any upvalues.
dlua_chunk::dlua_chunk(lua_State *ls)
    : file(), chunk(), compiled(), context(), first(-1), last(-1), error()
{
    clear();

    lua_stack_cleaner cln(ls);
    std::ostringstream out;
    const int err = lua_dump(ls, dlua_compiled_chunk_writer, &out);
    if (err)
    {
        const char *e = lua_tostring(ls, -1);
        error = e? e : "Unknown error compiling chunk";
    }
    compiled = out.str();
}

dlua_chunk dlua_chunk::precompiled(const std::string &chunk)
{
    dlua_chunk dchunk;
    dchunk.compiled = chunk;
    return (dchunk);
}

void dlua_chunk::write(writer& outf) const
{
    if (empty())
    {
        marshallByte(outf, CT_EMPTY);
        return;
    }

    if (!compiled.empty())
    {
        marshallByte(outf, CT_COMPILED);
        marshallString4(outf, compiled);
    }
    else
    {
        marshallByte(outf, CT_SOURCE);
        marshallString4(outf, chunk);
    }

    marshallString4(outf, file);
    marshallLong(outf, first);
}

void dlua_chunk::read(reader& inf)
{
    clear();
    chunk_t type = static_cast<chunk_t>(unmarshallByte(inf));
    switch (type)
    {
    case CT_EMPTY:
        return;
    case CT_SOURCE:
        unmarshallString4(inf, chunk);
        break;
    case CT_COMPILED:
        unmarshallString4(inf, compiled);
        break;
    }
    unmarshallString4(inf, file);
    first = unmarshallLong(inf);
}

void dlua_chunk::clear()
{
    file.clear();
    chunk.clear();
    first = last = -1;
    error.clear();
    compiled.clear();
}

void dlua_chunk::set_file(const std::string &s)
{
    file = s;
}

void dlua_chunk::add(int line, const std::string &s)
{
    if (first == -1)
        first = line;

    if (line != last && last != -1)
        while (last++ < line)
            chunk += '\n';

    chunk += " ";
    chunk += s;
    last = line;
}

void dlua_chunk::set_chunk(const std::string &s)
{
    chunk = s;
}

int dlua_chunk::check_op(CLua &interp, int err)
{
    error = interp.error;
    return (err);
}

int dlua_chunk::load(CLua &interp)
{
    if (!compiled.empty())
        return check_op( interp,
                         interp.loadbuffer(compiled.c_str(), compiled.length(),
                                           context.c_str()) );

    if (empty())
    {
        chunk.clear();
        return (-1000);
    }

    int err = check_op( interp,
                        interp.loadstring(chunk.c_str(), context.c_str()) );
    if (err)
        return (err);
    std::ostringstream out;
    err = lua_dump(interp, dlua_compiled_chunk_writer, &out);
    if (err)
    {
        const char *e = lua_tostring(interp, -1);
        error = e? e : "Unknown error compiling chunk";
        lua_pop(interp, 2);
    }
    compiled = out.str();
    chunk.clear();
    return (err);
}

int dlua_chunk::load_call(CLua &interp, const char *fn)
{
    int err = load(interp);
    if (err == -1000)
        return (0);
    if (err)
        return (err);

    return check_op(interp, !interp.callfn(fn, fn? 1 : 0, 0));
}

std::string dlua_chunk::orig_error() const
{
    rewrite_chunk_errors(error);
    return (error);
}

bool dlua_chunk::empty() const
{
    return compiled.empty() && trimmed_string(chunk).empty();
}

bool dlua_chunk::rewrite_chunk_errors(std::string &s) const
{
    const std::string contextm = "[string \"" + context + "\"]:";
    std::string::size_type dlwhere = s.find(contextm);

    if (dlwhere == std::string::npos)
        return (false);

    if (!dlwhere)
    {
        s = rewrite_chunk_prefix(s);
        return (true);
    }

    // Our chunk is mentioned, go back through and rewrite lines.
    std::vector<std::string> lines = split_string("\n", s);
    std::string newmsg = lines[0];
    bool wrote_prefix = false;
    for (int i = 2, size = lines.size() - 1; i < size; ++i)
    {
        const std::string &st = lines[i];
        if (st.find(context) != std::string::npos)
        {
            if (!wrote_prefix)
            {
                newmsg = get_chunk_prefix(st) + ": " + newmsg;
                wrote_prefix = true;
            }
            else
                newmsg += "\n" + rewrite_chunk_prefix(st);
        }
    }
    s = newmsg;
    return (true);
}

std::string dlua_chunk::rewrite_chunk_prefix(const std::string &line,
                                             bool skip_body) const
{
    std::string s = line;
    const std::string contextm = "[string \"" + context + "\"]:";
    const std::string::size_type ps = s.find(contextm);
    if (ps == std::string::npos)
        return (s);

    const std::string::size_type lns = ps + contextm.length();
    std::string::size_type pe = s.find(':', ps + contextm.length());
    if (pe != std::string::npos)
    {
        const std::string line_num = s.substr(lns, pe - lns);
        const int lnum = atoi(line_num.c_str());
        const std::string newlnum = make_stringf("%d", lnum + first - 1);
        s = s.substr(0, lns) + newlnum + s.substr(pe);
        pe = lns + newlnum.length();
    }

    return s.substr(0, ps) + (file.empty()? context : file) + ":"
        + (skip_body? s.substr(lns, pe - lns)
                    : s.substr(lns));
}

std::string dlua_chunk::get_chunk_prefix(const std::string &sorig) const
{
    return rewrite_chunk_prefix(sorig, true);
}

///////////////////////////////////////////////////////////////////////////
// Lua dungeon bindings (in the dgn table).

#define MAP(ls, n, var)                             \
    map_def *var = *(map_def **) luaL_checkudata(ls, n, MAP_METATABLE)
#define DEVENT(ls, n, var) \
    dgn_event *var = *(dgn_event **) luaL_checkudata(ls, n, DEVENT_METATABLE)
#define MAPMARKER(ls, n, var) \
    map_marker *var = *(map_marker **) luaL_checkudata(ls, n, MAPMARK_METATABLE)

void dgn_reset_default_depth()
{
    lc_default_depths.clear();
}

std::string dgn_set_default_depth(const std::string &s)
{
    std::vector<std::string> frags = split_string(",", s);
    for (int i = 0, size = frags.size(); i < size; ++i)
    {
        try
        {
            lc_default_depths.push_back( level_range::parse(frags[i]) );
        }
        catch (const std::string &error)
        {
            return (error);
        }
    }
    return ("");
}

static void dgn_add_depths(depth_ranges &drs, lua_State *ls, int s, int e)
{
    for (int i = s; i <= e; ++i)
    {
        const char *depth = luaL_checkstring(ls, i);
        std::vector<std::string> frags = split_string(",", depth);
        for (int j = 0, size = frags.size(); j < size; ++j)
        {
            try
            {
                drs.push_back( level_range::parse(frags[j]) );
            }
            catch (const std::string &error)
            {
                luaL_error(ls, error.c_str());
            }
        }
    }
}

static std::string dgn_depth_list_string(const depth_ranges &drs)
{
    return (comma_separated_line(drs.begin(), drs.end(), ", ", ", "));
}

static int dgn_depth_proc(lua_State *ls, depth_ranges &dr, int s)
{
    if (lua_gettop(ls) < s)
    {
        PLUARET(string, dgn_depth_list_string(dr).c_str());
    }

    if (lua_isnil(ls, s))
    {
        dr.clear();
        return (0);
    }

    dr.clear();
    dgn_add_depths(dr, ls, s, lua_gettop(ls));
    return (0);
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
                map->place = level_id::parse_level_id(luaL_checkstring(ls, 2));
            }
            catch (const std::string &err)
            {
                luaL_error(ls, err.c_str());
            }
        }
    }
    PLUARET(string, map->place.describe().c_str());
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

static int dgn_tags_remove(lua_State *ls)
{
    MAP(ls, 1, map);

    const int top = lua_gettop(ls);
    for (int i = 2; i <= top; ++i)
    {
        const std::string axee = luaL_checkstring(ls, i);
        const std::string::size_type pos = map->tags.find(axee);
        if (pos != std::string::npos)
            map->tags =
                map->tags.substr(0, pos)
                + map->tags.substr(pos + axee.length());
    }
    PLUARET(string, map->tags.c_str());
}

static const std::string level_flag_names[] =
    {"no_tele_control", "not_mappable", "no_magic_map", ""};

static int dgn_lflags(lua_State *ls)
{
    MAP(ls, 1, map);

    try
    {
        map->level_flags = map_flags::parse(level_flag_names,
                                            luaL_checkstring(ls, 2));
    }
    catch (const std::string &error)
    {
        luaL_argerror(ls, 2, error.c_str());
    }

    return (0);
}

static int dgn_change_level_flags(lua_State *ls)
{
    map_flags flags;

    try {
        flags = map_flags::parse(level_flag_names,
                                 luaL_checkstring(ls, 1));
    }
    catch (const std::string &error)
    {
        luaL_argerror(ls, 2, error.c_str());
        lua_pushboolean(ls, false);
        return (1);
    }

    bool silent = lua_toboolean(ls, 2);

    bool changed1 = set_level_flags(flags.flags_set, silent);
    bool changed2 = unset_level_flags(flags.flags_unset, silent);

    lua_pushboolean(ls, changed1 || changed2);

    return (1);
}

static const std::string branch_flag_names[] =
    {"no_tele_control", "not_mappable", "no_magic_map", ""};

static int dgn_bflags(lua_State *ls)
{
    MAP(ls, 1, map);

    try {
        map->branch_flags = map_flags::parse(branch_flag_names,
                                             luaL_checkstring(ls, 2));
    }
    catch (const std::string &error)
    {
        luaL_argerror(ls, 2, error.c_str());
    }

    return (0);
}

static int dgn_change_branch_flags(lua_State *ls)
{
    map_flags flags;

    try {
        flags = map_flags::parse(branch_flag_names,
                                 luaL_checkstring(ls, 1));
    }
    catch (const std::string &error)
    {
        luaL_argerror(ls, 2, error.c_str());
        lua_pushboolean(ls, false);
        return (1);
    }

    bool silent = lua_toboolean(ls, 2);

    bool changed1 = set_branch_flags(flags.flags_set, silent);
    bool changed2 = unset_branch_flags(flags.flags_unset, silent);

    lua_pushboolean(ls, changed1 || changed2);

    return (1);
}

static int dgn_weight(lua_State *ls)
{
    MAP(ls, 1, map);
    if (!lua_isnil(ls, 2))
        map->chance = luaL_checkint(ls, 2);
    PLUARET(number, map->chance);
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
            const std::string orient = luaL_checkstring(ls, 2);
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

static int dgn_shuffle(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) == 1)
        return dlua_stringtable(ls, map->get_shuffle_strings());

    for (int i = 2, size = lua_gettop(ls); i <= size; ++i)
    {
        if (lua_isnil(ls, i))
            map->map.clear_shuffles();
        else
        {
            std::string err = map->map.add_shuffle(luaL_checkstring(ls, i));
            if (!err.empty())
                luaL_error(ls, err.c_str());
        }
    }

    return (0);
}

static int dgn_shuffle_remove(lua_State *ls)
{
    MAP(ls, 1, map);
    for (int i = 2, size = lua_gettop(ls); i <= size; ++i)
        map->map.remove_shuffle(luaL_checkstring(ls, i));
    return (0);
}

static int dgn_map_add_transform(
    lua_State *ls,
    std::string (map_lines::*add)(const std::string &s),
    void (map_lines::*erase)())
{
    MAP(ls, 1, map);

    for (int i = 2, size = lua_gettop(ls); i <= size; ++i)
    {
        if (lua_isnil(ls, i))
            (map->map.*erase)();
        else
        {
            std::string err = (map->map.*add)(luaL_checkstring(ls, i));
            if (!err.empty())
                luaL_error(ls, err.c_str());
        }
    }

    return (0);
}

static int dgn_subst(lua_State *ls)
{
    return dgn_map_add_transform(ls, &map_lines::add_subst,
                                 &map_lines::clear_substs);
}

static int dgn_nsubst(lua_State *ls)
{
    return dgn_map_add_transform(ls, &map_lines::add_nsubst,
                                 &map_lines::clear_nsubsts);
}

static int dgn_colour(lua_State *ls)
{
    return dgn_map_add_transform(ls,
                                 &map_lines::add_colour,
                                 &map_lines::clear_colours);
}

static int dgn_subst_remove(lua_State *ls)
{
    MAP(ls, 1, map);
    for (int i = 2, size = lua_gettop(ls); i <= size; ++i)
        map->map.remove_subst(luaL_checkstring(ls, i));
    return (0);
}

static int dgn_map(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) == 1)
        return dlua_stringtable(ls, map->map.get_lines());

    if (lua_isnil(ls, 2))
    {
        map->map.clear();
        return (0);
    }

    if (lua_isstring(ls, 2))
    {
        map->map.add_line(luaL_checkstring(ls, 2));
        return (0);
    }

    std::vector<std::string> &lines = map->map.get_lines();
    int which_line = luaL_checkint(ls, 2);
    if (which_line < 0)
        which_line += (int) lines.size();
    if (lua_gettop(ls) == 2)
    {
        if (which_line < 0 || which_line >= (int) lines.size())
        {
            luaL_error(ls,
                       lines.empty()? "Map is empty"
                       : make_stringf("Line %d out of range (0-%u)",
                                      which_line,
                                      lines.size() - 1).c_str());
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
        return (0);
    }

    const std::string newline = luaL_checkstring(ls, 3);
    if (which_line < 0)
        luaL_error(ls,
                   make_stringf("Index %d out of range", which_line).c_str());

    if (which_line < (int) lines.size())
    {
        lines[which_line] = newline;
        return (0);
    }

    lines.reserve(which_line + 1);
    lines.resize(which_line + 1, "");
    lines[which_line] = newline;
    return (0);
}

static int dgn_mons(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) == 1)
        return (0);

    if (lua_isnil(ls, 2))
    {
        map->mons.clear();
        return (0);
    }

    if (lua_isstring(ls, 2))
    {
        std::string err = map->mons.add_mons(luaL_checkstring(ls, 2));
        if (!err.empty())
            luaL_error(ls, err.c_str());
        return (0);
    }

    const int index = luaL_checkint(ls, 2);
    std::string err = map->mons.set_mons(index, luaL_checkstring(ls, 3));
    if (!err.empty())
        luaL_error(ls, err.c_str());
    return (0);
}

static int dgn_item(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) == 1)
        return (0);

    if (lua_isnil(ls, 2))
    {
        map->items.clear();
        return (0);
    }

    if (lua_isstring(ls, 2))
    {
        std::string err = map->items.add_item(luaL_checkstring(ls, 2));
        if (!err.empty())
            luaL_error(ls, err.c_str());
        return (0);
    }

    const int index = luaL_checkint(ls, 2);
    std::string err = map->items.set_item(index, luaL_checkstring(ls, 3));
    if (!err.empty())
        luaL_error(ls, err.c_str());
    return (0);
}

static int dgn_marker(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) == 1)
        return (0);
    if (lua_isnil(ls, 2))
    {
        map->map.clear_markers();
        return (0);
    }

    if (lua_isstring(ls, 2))
    {
        std::string err = map->map.add_feature_marker(luaL_checkstring(ls, 2));
        if (!err.empty())
            luaL_error(ls, err.c_str());
    }
    return (0);
}

static int dgn_kfeat(lua_State *ls)
{
    MAP(ls, 1, map);
    std::string err = map->add_key_feat(luaL_checkstring(ls, 2));
    if (!err.empty())
        luaL_error(ls, err.c_str());
    return (0);
}

static int dgn_kmons(lua_State *ls)
{
    MAP(ls, 1, map);
    std::string err = map->add_key_mons(luaL_checkstring(ls, 2));
    if (!err.empty())
        luaL_error(ls, err.c_str());
    return (0);
}

static int dgn_kitem(lua_State *ls)
{
    MAP(ls, 1, map);
    std::string err = map->add_key_item(luaL_checkstring(ls, 2));
    if (!err.empty())
        luaL_error(ls, err.c_str());
    return (0);
}

static int dgn_kmask(lua_State *ls)
{
    MAP(ls, 1, map);
    std::string err = map->add_key_mask(luaL_checkstring(ls, 2));
    if (!err.empty())
        luaL_error(ls, err.c_str());
    return (0);
}

static int dgn_name(lua_State *ls)
{
    MAP(ls, 1, map);
    PLUARET(string, map->name.c_str());
}

static int dgn_welcome(lua_State *ls)
{
    MAP(ls, 1, map);
    map->welcome_messages.push_back(luaL_checkstring(ls, 2));
    return (0);
}

static int dgn_grid(lua_State *ls)
{
    const int x = luaL_checkint(ls, 1), y = luaL_checkint(ls, 2);
    if (!map_bounds(x, y))
        luaL_error(ls,
                   make_stringf("(%d,%d) is out of bounds (%d-%d,%d-%d)",
                                x, y,
                                X_BOUND_1, X_BOUND_2,
                                Y_BOUND_1, Y_BOUND_2).c_str());
    if (lua_isnumber(ls, 3))
        grd[x][y] = static_cast<dungeon_feature_type>(luaL_checkint(ls, 3));
    PLUARET(number, grd[x][y]);
}

static int dgn_max_bounds(lua_State *ls)
{
    lua_pushnumber(ls, GXM);
    lua_pushnumber(ls, GYM);
    return (2);
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
        return luaL_error
            (ls,
             make_stringf("Not enough points to test connectedness "
                          "(need at least %d)", minargs / 2).c_str());

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

static void dlua_push_coord(lua_State *ls, const coord_def &c)
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
        dlua_push_coord(ls, c);
        return (2);
    }
    return (0);
}

static int dgn_gly_points(lua_State *ls)
{
    MAP(ls, 1, map);
    std::vector<coord_def> cs = map->find_glyph(*luaL_checkstring(ls, 2));

    for (int i = 0, size = cs.size(); i < size; ++i)
        dlua_push_coord(ls, cs[i]);
    return (cs.size() * 2);
}

static int dgn_original_map(lua_State *ls)
{
    MAP(ls, 1, map);
    if (map->original)
        clua_push_map(ls, map->original);
    else
        lua_pushnil(ls);
    return (1);
}

static int dgn_load_des_file(lua_State *ls)
{
    const std::string &file = luaL_checkstring(ls, 1);
    if (!file.empty())
        read_map(file);
    return (0);
}

static int dgn_floor_colour(lua_State *ls)
{
    MAP(ls, 1, map);

    const char *s = luaL_checkstring(ls, 2);
    int colour = str_to_colour(s);

    if (colour < 0 || colour == BLACK)
    {
        std::string error;

        if (colour == BLACK)
        {
            error = "Can't set floor to black.";
        }
        else {
            error = "No such colour as '";
            error += s;
            error += "'";
        }

        luaL_argerror(ls, 2, error.c_str());

        return (0);
    }

    map->floor_colour = (unsigned char) colour;
    return (0);
}

static int dgn_rock_colour(lua_State *ls)
{
    MAP(ls, 1, map);

    const char *s = luaL_checkstring(ls, 2);
    int colour = str_to_colour(s);

    if (colour < 0 || colour == BLACK)
    {
        std::string error;

        if (colour == BLACK)
        {
            error = "Can't set rock to black.";
        }
        else {
            error = "No such colour as '";
            error += s;
            error += "'";
        }

        luaL_argerror(ls, 2, error.c_str());

        return (0);
    }

    map->rock_colour = (unsigned char) colour;

    return (0);
}

static int dgn_get_floor_colour(lua_State *ls)
{
    PLUARET(string, colour_to_str(env.floor_colour));
}

static int dgn_get_rock_colour(lua_State *ls)
{
    PLUARET(string, colour_to_str(env.rock_colour));
}

static int dgn_change_floor_colour(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);
    int colour = str_to_colour(s);

    if (colour < 0 || colour == BLACK)
    {
        std::string error;

        if (colour == BLACK)
        {
            error = "Can't set floor to black.";
        }
        else {
            error = "No such colour as '";
            error += s;
            error += "'";
        }

        luaL_argerror(ls, 1, error.c_str());

        return (0);
    }

    env.floor_colour = (unsigned char) colour;

    viewwindow(true, false);

    return (0);
}

static int dgn_change_rock_colour(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);
    int colour = str_to_colour(s);

    if (colour < 0 || colour == BLACK)
    {
        std::string error;

        if (colour == BLACK)
        {
            error = "Can't set rock to black.";
        }
        else {
            error = "No such colour as '";
            error += s;
            error += "'";
        }

        luaL_argerror(ls, 1, error.c_str());

        return (0);
    }

    env.rock_colour = (unsigned char) colour;

    viewwindow(true, false);

    return (0);
}

const char *dngn_feature_names[] =
{
    "unseen", "closed_door", "secret_door", "wax_wall", "metal_wall",
    "green_crystal_wall", "rock_wall", "stone_wall", "permarock_wall",
    "clear_rock_wall", "clear_stone_wall", "clear_permarock_wall",
    "orcish_idol", "", "", "", "", "", "", "", "",
    "granite_statue", "statue_reserved_1", "statue_reserved_2",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "lava",
    "deep_water", "", "", "shallow_water", "water_stuck", "floor",
    "floor_special", "floor_reserved", "exit_hell", "enter_hell",
    "open_door", "", "", "trap_mechanical", "trap_magical", "trap_natural",
    "undiscovered_trap", "", "enter_shop", "enter_labyrinth",
    "stone_stairs_down_i", "stone_stairs_down_ii",
    "stone_stairs_down_iii", "escape_hatch_down", "stone_stairs_up_i",
    "stone_stairs_up_ii", "stone_stairs_up_iii", "escape_hatch_up", "",
    "", "enter_dis", "enter_gehenna", "enter_cocytus",
    "enter_tartarus", "enter_abyss", "exit_abyss", "stone_arch",
    "enter_pandemonium", "exit_pandemonium", "transit_pandemonium",
    "", "", "", "builder_special_wall", "builder_special_floor", "",
    "", "", "enter_orcish_mines", "enter_hive", "enter_lair",
    "enter_slime_pits", "enter_vaults", "enter_crypt",
    "enter_hall_of_blades", "enter_zot", "enter_temple",
    "enter_snake_pit", "enter_elven_halls", "enter_tomb",
    "enter_swamp", "enter_shoals", "enter_reserved_2",
    "enter_reserved_3", "enter_reserved_4", "", "", "",
    "return_from_orcish_mines", "return_from_hive",
    "return_from_lair", "return_from_slime_pits",
    "return_from_vaults", "return_from_crypt",
    "return_from_hall_of_blades", "return_from_zot",
    "return_from_temple", "return_from_snake_pit",
    "return_from_elven_halls", "return_from_tomb",
    "return_from_swamp", "return_from_shoals", "return_reserved_2",
    "return_reserved_3", "return_reserved_4", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "enter_portal_vault", "exit_portal_vault",
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
    "", "", "altar_zin", "altar_shining_one", "altar_kikubaaqudgha",
    "altar_yredelemnul", "altar_xom", "altar_vehumet",
    "altar_okawaru", "altar_makhleb", "altar_sif_muna", "altar_trog",
    "altar_nemelex_xobeh", "altar_elyvilon", "altar_lugonu",
    "altar_beogh", "", "", "", "", "", "", "fountain_blue",
    "fountain_sparkling", "fountain_blood", "dry_fountain_blue",
    "dry_fountain_sparkling", "dry_fountain_blood", "permadry_fountain"
};

dungeon_feature_type dungeon_feature_by_name(const std::string &name)
{
    COMPILE_CHECK(ARRAYSIZE(dngn_feature_names) == NUM_REAL_FEATURES, c1);
    if (name.empty())
        return (DNGN_UNSEEN);

    for (unsigned i = 0; i < ARRAYSIZE(dngn_feature_names); ++i)
        if (dngn_feature_names[i] == name)
            return static_cast<dungeon_feature_type>(i);

    return (DNGN_UNSEEN);
}

std::vector<std::string> dungeon_feature_matches(const std::string &name)
{
    std::vector<std::string> matches;

    COMPILE_CHECK(ARRAYSIZE(dngn_feature_names) == NUM_REAL_FEATURES, c1);
    if (name.empty())
        return (matches);

    for (unsigned i = 0; i < ARRAYSIZE(dngn_feature_names); ++i)
        if (strstr(dngn_feature_names[i], name.c_str()))
            matches.push_back(dngn_feature_names[i]);

    return (matches);
}

const char *dungeon_feature_name(dungeon_feature_type rfeat)
{
    const unsigned feat = rfeat;

    if (feat >= ARRAYSIZE(dngn_feature_names))
        return (NULL);

    return dngn_feature_names[feat];
}

static int dgn_feature_number(lua_State *ls)
{
    const std::string &name = luaL_checkstring(ls, 1);
    PLUARET(number, dungeon_feature_by_name(name));
}

static int dgn_feature_name(lua_State *ls)
{
    const unsigned feat = luaL_checkint(ls, 1);
    PLUARET(string,
            dungeon_feature_name(static_cast<dungeon_feature_type>(feat)));
}

static const char *dgn_event_type_names[] =
{
    "none", "turn", "mons_move", "player_move", "leave_level",
    "entering_level", "entered_level", "player_los", "player_climb",
    "monster_dies", "item_pickup", "item_moved", "feat_change"
};

static dgn_event_type dgn_event_type_by_name(const std::string &name)
{
    for (unsigned i = 0; i < ARRAYSIZE(dgn_event_type_names); ++i)
        if (dgn_event_type_names[i] == name)
            return static_cast<dgn_event_type>(i? 1 << (i - 1) : 0);
    return (DET_NONE);
}

static const char *dgn_event_type_name(unsigned evmask)
{
    if (evmask == 0)
        return (dgn_event_type_names[0]);

    for (unsigned i = 1; i < ARRAYSIZE(dgn_event_type_names); ++i)
        if (evmask & (1 << (i - 1)))
            return (dgn_event_type_names[i]);

    return (dgn_event_type_names[0]);
}

static void dgn_push_event_type(lua_State *ls, int n)
{
    if (lua_isstring(ls, n))
        lua_pushnumber(ls, dgn_event_type_by_name(lua_tostring(ls, n)));
    else if (lua_isnumber(ls, n))
        lua_pushstring(ls, dgn_event_type_name(luaL_checkint(ls, n)));
    else
        lua_pushnil(ls);
}

static int dgn_dgn_event(lua_State *ls)
{
    const int start = lua_isuserdata(ls, 1)? 2 : 1;
    int retvals = 0;
    for (int i = start, nargs = lua_gettop(ls); i <= nargs; ++i)
    {
        dgn_push_event_type(ls, i);
        retvals++;
    }
    return (retvals);
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
    return (0);
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
    return (0);
}

static int dgn_remove_marker(lua_State *ls)
{
    MAPMARKER(ls, 1, mark);
    env.markers.remove(mark);
    return (0);
}

static int dgn_num_matching_markers(lua_State *ls)
{
    const char* key     = luaL_checkstring(ls, 1);
    const char* val_ptr = lua_tostring(ls, 2);
    const char* val;

    if (val_ptr == NULL)
        val = "";
    else
        val = val_ptr;

    std::vector<map_marker*> markers = env.markers.get_all(key, val);

    PLUARET(number, markers.size());
}

static int dgn_feature_desc(lua_State *ls)
{
    const dungeon_feature_type feat =
        static_cast<dungeon_feature_type>(luaL_checkint(ls, 1));
    const description_level_type dtype =
        lua_isnumber(ls, 2)?
        static_cast<description_level_type>(luaL_checkint(ls, 2)) :
        description_type_by_name(lua_tostring(ls, 2));
    const bool need_stop = lua_isboolean(ls, 3)? lua_toboolean(ls, 3) : false;
    const std::string s =
        feature_description(feat, NUM_TRAPS, false, dtype, need_stop);
    lua_pushstring(ls, s.c_str());
    return (1);
}

static int dgn_feature_desc_at(lua_State *ls)
{
    const description_level_type dtype =
        lua_isnumber(ls, 3)?
        static_cast<description_level_type>(luaL_checkint(ls, 3)) :
        description_type_by_name(lua_tostring(ls, 3));
    const bool need_stop = lua_isboolean(ls, 4)? lua_toboolean(ls, 4) : false;
    const std::string s =
        feature_description(luaL_checkint(ls, 1), luaL_checkint(ls, 2), false,
                            dtype, need_stop);
    lua_pushstring(ls, s.c_str());
    return (1);
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
    dungeon_terrain_changed( coord_def( luaL_checkint(ls, 1),
                                        luaL_checkint(ls, 2) ),
                             type, affect_player,
                             preserve_features, preserve_items );
    return (0);
}

static int dgn_item_from_index(lua_State *ls)
{
    const int index = luaL_checkint(ls, 1);

    item_def *item = &mitm[index];

    if (is_valid_item(*item))
        lua_pushlightuserdata(ls, item);
    else
        lua_pushnil(ls);

    return (1);
}

static int dgn_mons_from_index(lua_State *ls)
{
    const int index = luaL_checkint(ls, 1);

    monsters *mons = &menv[index];

    if (mons->type != -1)
        push_monster(ls, mons);
    else
        lua_pushnil(ls);

    return (1);
}

static int lua_dgn_set_lt_callback(lua_State *ls)
{
    const char *level_type = luaL_checkstring(ls, 1);

    if (level_type == NULL || strlen(level_type) == 0)
        return (0);

    const char *callback_name = luaL_checkstring(ls, 2);

    if (callback_name == NULL || strlen(callback_name) == 0)
        return (0);

    dgn_set_lt_callback(level_type, callback_name);

    return (0);
}

static int dgn_fixup_stairs(lua_State *ls)
{
    const dungeon_feature_type up_feat =
        dungeon_feature_by_name(luaL_checkstring(ls, 1));

    const dungeon_feature_type down_feat =
        dungeon_feature_by_name(luaL_checkstring(ls, 2));

    if (up_feat == DNGN_UNSEEN && down_feat == DNGN_UNSEEN)
        return(0);

    for (int y = 0; y < GYM; ++y)
    {
        for (int x = 0; x < GXM; ++x)
        {
            const dungeon_feature_type feat = grd[x][y];
            if (grid_is_stone_stair(feat) || grid_is_escape_hatch(feat))
            {
                dungeon_feature_type new_feat = DNGN_UNSEEN;

                if (grid_stair_direction(feat) == CMD_GO_DOWNSTAIRS)
                    new_feat = down_feat;
                else
                    new_feat = up_feat;

                if (new_feat != DNGN_UNSEEN)
                {
                    grd[x][y] = new_feat;
                    env.markers.add(
                        new map_feature_marker(
                            coord_def(x, y),
                            new_feat));
                }
            }
        }
    }

    return (0);
}

static int dgn_floor_halo(lua_State *ls)
{
    std::string error = "";

    const char* s1 = luaL_checkstring(ls, 1);
    const dungeon_feature_type target = dungeon_feature_by_name(s1);

    if (target == DNGN_UNSEEN)
    {
        error += "No such dungeon feature as '";
        error += s1;
        error += "'.  ";
    }

    const char* s2 = luaL_checkstring(ls, 2);
    short colour = str_to_colour(s2);

    if (colour == -1)
    {
        error += "No such colour as '";
        error += s2;
        error += "'.";
    }
    else if (colour == BLACK)
    {
        error += "Can't set floor colour to black.";
    }

    if (error != "")
    {
        luaL_argerror(ls, 2, error.c_str());
        return(0);
    }

    for (int y = 0; y < GYM; ++y)
    {
        for (int x = 0; x < GXM; ++x)
        {
            const dungeon_feature_type feat = grd[x][y];
            if (feat == target)
            {

                for (int i=-1; i<=1; i++)
                     for (int j=-1; j<=1; j++)
                     {
                         if (!map_bounds(x+i, y+j))
                             continue;

                         const dungeon_feature_type feat2 = grd[x+i][y+j];

                         if (feat2 == DNGN_FLOOR
                             || feat2 == DNGN_UNDISCOVERED_TRAP)
                         {
                             env.grid_colours[x+i][y+j] = colour;
                         }
                     }
            }
        }
    }

    return (0);
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
        lua_isnil(ls, 4) ? DNGN_MINMOVE : DNGN_ORCISH_IDOL;

    if (!in_bounds(x, y))
    {
        char buf[80];
        sprintf(buf, "Point (%d,%d) isn't in bounds.", x, y);
        luaL_argerror(ls, 1, buf);
        return (0);
    }
    if (dist < 1)
    {
        luaL_argerror(ls, 3, "Distance must be positive.");
        return (0);
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

    dlua_push_coord(ls, pos);

    return (2);
}

static cloud_type dgn_cloud_name_to_type(std::string name)
{
    lowercase(name);

    if (name == "random")
        return (CLOUD_RANDOM);
    else if (name == "debugging")
        return (CLOUD_DEBUGGING);

    for (int i = CLOUD_NONE; i < CLOUD_RANDOM; i++)
        if (cloud_name(static_cast<cloud_type>(i)) == name)
            return static_cast<cloud_type>(i);

    return (CLOUD_NONE);
}

static kill_category dgn_kill_name_to_category(std::string name)
{
    if (name == "")
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

static int make_a_lua_cloud(int x, int y, int garbage, int spread_rate,
                             cloud_type ctype, kill_category whose)
{
    UNUSED( garbage );
    const int pow = random_range(lua_cloud_pow_min,
                                 lua_cloud_pow_max,
                                 lua_cloud_pow_rolls);
    place_cloud( ctype, x, y, pow, whose, spread_rate );

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

    const cloud_type ctype = dgn_cloud_name_to_type(luaL_checkstring(ls, 7));
    const char*      kname = lua_isstring(ls, 8) ? luaL_checkstring(ls, 8)
                                                 : "";
    const kill_category kc = dgn_kill_name_to_category(kname);

    const int spread_rate = lua_isnumber(ls, 9) ? luaL_checkint(ls, 9) : -1;

    if (!in_bounds(x, y))
    {
        char buf[80];
        sprintf(buf, "Point (%d,%d) isn't in bounds.", x, y);
        luaL_argerror(ls, 1, buf);
        return (0);
    }

    if (lua_cloud_pow_min < 0)
    {
        luaL_argerror(ls, 4, "pow_min must be non-negative");
        return (0);
    }

    if (lua_cloud_pow_max < lua_cloud_pow_min)
    {
        luaL_argerror(ls, 5, "pow_max must not be less than pow_min");
        return (0);
    }

    if (lua_cloud_pow_max == 0)
    {
        luaL_argerror(ls, 5, "pow_max must be positive");
        return (0);
    }

    if (lua_cloud_pow_rolls <= 0)
    {
        luaL_argerror(ls, 6, "pow_rolls must be positive");
        return (0);
    }

    if (size < 1)
    {
        luaL_argerror(ls, 4, "size must be positive.");
        return (0);
    }

    if (ctype == CLOUD_NONE)
    {
        std::string error = "Invalid cloud type '";
        error += luaL_checkstring(ls, 7);
        error += "'";
        luaL_argerror(ls, 7, error.c_str());
        return (0);
    }

    if (kc == KC_NCATEGORIES)
    {
        std::string error = "Invalid kill category '";
        error += kname;
        error += "'";
        luaL_argerror(ls, 8, error.c_str());
        return (0);
    }

    if (spread_rate < -1 || spread_rate > 100)
    {
        luaL_argerror(ls, 9, "spread_rate must be between -1 and 100,"
                      "inclusive");
        return (0);
    }

    apply_area_cloud(make_a_lua_cloud, x, y, 0, size,
                     ctype, kc, spread_rate);

    return (0);
}

static const struct luaL_reg dgn_lib[] =
{
    { "default_depth", dgn_default_depth },
    { "name", dgn_name },
    { "depth", dgn_depth },
    { "place", dgn_place },
    { "tags",  dgn_tags },
    { "tags_remove", dgn_tags_remove },
    { "lflags", dgn_lflags },
    { "bflags", dgn_bflags },
    { "chance", dgn_weight },
    { "welcome", dgn_welcome },
    { "weight", dgn_weight },
    { "orient", dgn_orient },
    { "shuffle", dgn_shuffle },
    { "shuffle_remove", dgn_shuffle_remove },
    { "subst", dgn_subst },
    { "nsubst", dgn_nsubst },
    { "colour", dgn_colour },
    { "floor_colour", dgn_floor_colour},
    { "rock_colour", dgn_rock_colour},
    { "subst_remove", dgn_subst_remove },
    { "map", dgn_map },
    { "mons", dgn_mons },
    { "item", dgn_item },
    { "marker", dgn_marker },
    { "kfeat", dgn_kfeat },
    { "kitem", dgn_kitem },
    { "kmons", dgn_kmons },
    { "kmask", dgn_kmask },

    { "grid", dgn_grid },
    { "max_bounds", dgn_max_bounds },

    { "terrain_changed", dgn_terrain_changed },
    { "points_connected", dgn_points_connected },
    { "any_point_connected", dgn_any_point_connected },
    { "has_exit_from", dgn_has_exit_from },
    { "gly_point", dgn_gly_point },
    { "gly_points", dgn_gly_points },
    { "original_map", dgn_original_map },
    { "load_des_file", dgn_load_des_file },
    { "feature_number", dgn_feature_number },
    { "feature_name", dgn_feature_name },
    { "dgn_event_type", dgn_dgn_event },
    { "register_listener", dgn_register_listener },
    { "remove_listener", dgn_remove_listener },
    { "remove_marker", dgn_remove_marker },
    { "num_matching_markers", dgn_num_matching_markers},
    { "feature_desc", dgn_feature_desc },
    { "feature_desc_at", dgn_feature_desc_at },
    { "item_from_index", dgn_item_from_index },
    { "mons_from_index", dgn_mons_from_index },
    { "change_level_flags", dgn_change_level_flags},
    { "change_branch_flags", dgn_change_branch_flags},
    { "get_floor_colour", dgn_get_floor_colour},
    { "get_rock_colour",  dgn_get_rock_colour},
    { "change_floor_colour", dgn_change_floor_colour},
    { "change_rock_colour",  dgn_change_rock_colour},
    { "set_lt_callback", lua_dgn_set_lt_callback},
    { "fixup_stairs", dgn_fixup_stairs},
    { "floor_halo", dgn_floor_halo},
    { "random_walk", dgn_random_walk},
    { "apply_area_cloud", dgn_apply_area_cloud},

    { NULL, NULL }
};

static int crawl_args(lua_State *ls)
{
    return dlua_stringtable(ls, SysEnv.cmd_args);
}

static const struct luaL_reg crawl_lib[] =
{
    { "args", crawl_args },
    { NULL, NULL }
};

static int file_marshall(lua_State *ls)
{
    if (lua_gettop(ls) != 2)
        luaL_error(ls, "Need two arguments: tag header and value");
    writer &th(*static_cast<writer*>( lua_touserdata(ls, 1) ));
    if (lua_isnumber(ls, 2))
        marshallLong(th, luaL_checklong(ls, 2));
    else if (lua_isstring(ls, 2))
        marshallString(th, lua_tostring(ls, 2));
    else if (lua_isfunction(ls, 2))
    {
        dlua_chunk chunk(ls);
        marshallString(th, chunk.compiled_chunk());
    }
    return (0);
}

static int file_unmarshall_number(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>( lua_touserdata(ls, 1) ));
    lua_pushnumber(ls, unmarshallLong(th));
    return (1);
}

static int file_unmarshall_string(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>( lua_touserdata(ls, 1) ));
    lua_pushstring(ls, unmarshallString(th).c_str());
    return (1);
}

static int file_unmarshall_fn(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>( lua_touserdata(ls, 1) ));
    const std::string s(unmarshallString(th, LUA_CHUNK_MAX_SIZE));
    dlua_chunk chunk = dlua_chunk::precompiled(s);
    if (chunk.load(dlua))
        lua_pushnil(ls);
    return (1);
}

enum lua_persist_type
{
    LPT_NONE,
    LPT_NUMBER,
    LPT_STRING,
    LPT_FUNCTION,
    LPT_NIL
};

static int file_marshall_meta(lua_State *ls)
{
    if (lua_gettop(ls) != 2)
        luaL_error(ls, "Need two arguments: tag header and value");

    writer &th(*static_cast<writer*>( lua_touserdata(ls, 1) ));

    lua_persist_type ptype = LPT_NONE;
    if (lua_isnumber(ls, 2))
        ptype = LPT_NUMBER;
    else if (lua_isstring(ls, 2))
        ptype = LPT_STRING;
    else if (lua_isfunction(ls, 2))
        ptype = LPT_FUNCTION;
    else if (lua_isnil(ls, 2))
        ptype = LPT_NIL;
    else
        luaL_error(ls, "Can marshall only numbers, strings and functions.");
    marshallByte(th, ptype);
    if (ptype != LPT_NIL)
        file_marshall(ls);
    return (0);
}

static int file_unmarshall_meta(lua_State *ls)
{
    reader &th(*static_cast<reader*>( lua_touserdata(ls, 1) ));
    const lua_persist_type ptype =
        static_cast<lua_persist_type>(unmarshallByte(th));
    switch (ptype)
    {
    case LPT_NUMBER:
        return file_unmarshall_number(ls);
    case LPT_STRING:
        return file_unmarshall_string(ls);
    case LPT_FUNCTION:
        return file_unmarshall_fn(ls);
    case LPT_NIL:
        lua_pushnil(ls);
        return (1);
    default:
        luaL_error(ls, "Unexpected type signature.");
    }
    // Never get here.
    return (0);
}

static const struct luaL_reg file_lib[] =
{
    { "marshall",   file_marshall },
    { "marshall_meta", file_marshall_meta },
    { "unmarshall_meta", file_unmarshall_meta },
    { "unmarshall_number", file_unmarshall_number },
    { "unmarshall_string", file_unmarshall_string },
    { "unmarshall_fn", file_unmarshall_fn },
    { NULL, NULL }
};

LUARET1(you_can_hear_pos, boolean,
        player_can_hear(luaL_checkint(ls,1), luaL_checkint(ls, 2)))
LUARET1(you_x_pos, number, you.x_pos)
LUARET1(you_y_pos, number, you.y_pos)
LUARET2(you_pos, number, you.x_pos, you.y_pos)

static const struct luaL_reg you_lib[] =
{
    { "hear_pos", you_can_hear_pos },
    { "x_pos", you_x_pos },
    { "y_pos", you_y_pos },
    { "pos",   you_pos },
    { NULL, NULL }
};

static int dgnevent_type(lua_State *ls)
{
    DEVENT(ls, 1, dev);
    PLUARET(number, dev->type);
}

static int dgnevent_place(lua_State *ls)
{
    DEVENT(ls, 1, dev);
    lua_pushnumber(ls, dev->place.x);
    lua_pushnumber(ls, dev->place.y);
    return (2);
}

static int dgnevent_dest(lua_State *ls)
{
    DEVENT(ls, 1, dev);
    lua_pushnumber(ls, dev->dest.x);
    lua_pushnumber(ls, dev->dest.y);
    return (2);
}

static int dgnevent_ticks(lua_State *ls)
{
    DEVENT(ls, 1, dev);
    PLUARET(number, dev->elapsed_ticks);
}

static int dgnevent_arg1(lua_State *ls)
{
    DEVENT(ls, 1, dev);
    PLUARET(number, dev->arg1);
}

static int dgnevent_arg2(lua_State *ls)
{
    DEVENT(ls, 1, dev);
    PLUARET(number, dev->arg2);
}

static const struct luaL_reg dgnevent_lib[] =
{
    { "type",  dgnevent_type },
    { "pos",   dgnevent_place },
    { "dest",  dgnevent_dest },
    { "ticks", dgnevent_ticks },
    { "arg1",  dgnevent_arg1 },
    { "arg2",  dgnevent_arg2 },
    { NULL, NULL }
};

static void luaopen_setmeta(lua_State *ls,
                            const char *global,
                            const luaL_reg *lua_lib,
                            const char *meta)
{
    luaL_newmetatable(ls, meta);
    lua_setglobal(ls, global);

    luaL_openlib(ls, global, lua_lib, 0);

    // Do <global>.__index = <global>
    lua_pushstring(ls, "__index");
    lua_pushvalue(ls, -2);
    lua_settable(ls, -3);
}

static void luaopen_dgnevent(lua_State *ls)
{
    luaopen_setmeta(ls, "dgnevent", dgnevent_lib, DEVENT_METATABLE);
}

static int mapmarker_pos(lua_State *ls)
{
    MAPMARKER(ls, 1, mark);
    lua_pushnumber(ls, mark->pos.x);
    lua_pushnumber(ls, mark->pos.y);
    return (2);
}

static int mapmarker_move(lua_State *ls)
{
    MAPMARKER(ls, 1, mark);
    const coord_def dest( luaL_checkint(ls, 2), luaL_checkint(ls, 3) );
    env.markers.move_marker(mark, dest);
    return (0);
}

static const struct luaL_reg mapmarker_lib[] =
{
    { "pos", mapmarker_pos },
    { "move", mapmarker_move },
    { NULL, NULL }
};

static void luaopen_mapmarker(lua_State *ls)
{
    luaopen_setmeta(ls, "mapmarker", mapmarker_lib, MAPMARK_METATABLE);
}

void init_dungeon_lua()
{
    lua_stack_cleaner clean(dlua);

    luaL_openlib(dlua, "dgn", dgn_lib, 0);
    // Add additional function to the Crawl module.
    luaL_openlib(dlua, "crawl", crawl_lib, 0);
    luaL_openlib(dlua, "file", file_lib, 0);
    luaL_openlib(dlua, "you", you_lib, 0);

    dlua.execfile("clua/dungeon.lua", true, true);
    dlua.execfile("clua/luamark.lua", true, true);

    lua_getglobal(dlua, "dgn_run_map");
    luaopen_debug(dlua);
    luaL_newmetatable(dlua, MAP_METATABLE);

    luaopen_dgnevent(dlua);
    luaopen_mapmarker(dlua);
}
