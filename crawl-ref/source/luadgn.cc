/*
 *  File:       luadgn.cc
 *  Summary:    Dungeon-builder Lua interface.
 *
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2007-06-21T19:20:47.183838Z $
 */

#include "AppHdr.h"
#include "clua.h"
#include "luadgn.h"
#include "mapdef.h"

// Lua interpreter for the dungeon builder.
CLua dlua(false);

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

static int dlua_stringtable(lua_State *ls, const std::vector<std::string> &s)
{
    return dlua_gentable(ls, s, dlua_pushcxxstring);
}

///////////////////////////////////////////////////////////////////////////
// dlua_chunk

dlua_chunk::dlua_chunk(const std::string &_context)
    : file(), chunk(), context(_context), first(-1), last(-1), error()
{
    clear();
}

void dlua_chunk::clear()
{
    file.clear();
    chunk.clear();
    first = last = -1;
    error.clear();
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

int dlua_chunk::check_op(CLua *interp, int err)
{
    error = interp->error;
    return (err);
}

int dlua_chunk::load(CLua *interp)
{
    if (trimmed_string(chunk).empty())
        return (-1000);
    return check_op(interp,
                    interp->loadstring(chunk.c_str(), context.c_str()));
}

int dlua_chunk::load_call(CLua *interp, const char *fn)
{
    int err = load(interp);
    if (err == -1000)
        return (0);
    if (err)
        return (err);

    return check_op(interp, interp->callfn(fn, 1, 0));
}

std::string dlua_chunk::orig_error() const
{
    return (error);
}

bool dlua_chunk::empty() const
{
    return trimmed_string(chunk).empty();
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

std::string dlua_chunk::rewrite_chunk_prefix(const std::string &line) const
{
    std::string s = line;
    const std::string contextm = "[string \"" + context + "\"]:";
    const std::string::size_type ps = s.find(contextm);
    if (ps == std::string::npos)
        return (s);

    std::string::size_type pe = s.find(':', ps + contextm.length());
    if (pe != std::string::npos)
    {
        const std::string::size_type lns = ps + contextm.length();
        const std::string line_num = s.substr(lns, pe - lns);
        const int lnum = atoi(line_num.c_str());
        s = s.substr(0, lns) + make_stringf("%d", lnum + first - 1)
            + s.substr(pe);
    }

    return s.substr(0, ps) + (file.empty()? context : file) + ":"
        + s.substr(ps + contextm.length());
}

std::string dlua_chunk::get_chunk_prefix(const std::string &sorig) const
{
    std::string s = rewrite_chunk_prefix(sorig);
    const std::string::size_type cpos = s.find(':');
    if (cpos == std::string::npos)
        return (s);
    const std::string::size_type cnpos = s.find(':', cpos + 1);
    if (cnpos == std::string::npos)
        return (s);
    return s.substr(0, cnpos);
}

///////////////////////////////////////////////////////////////////////////
// Lua dungeon bindings (in the dgn table).

static depth_ranges dgn_default_depths;

#define MAP(ls, n, var)                             \
    map_def *var = *(map_def **) luaL_checkudata(ls, n, MAP_METATABLE)

void dgn_reset_default_depth()
{
    dgn_default_depths.clear();
}

std::string dgn_set_default_depth(const std::string &s)
{
    std::vector<std::string> frags = split_string(",", s);
    for (int i = 0, size = frags.size(); i < size; ++i)
    {
        try
        {
            dgn_default_depths.push_back( level_range::parse(frags[i]) );
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
        for (int j = 0, size = frags.size(); i < size; ++i)
        {
            try
            {
                dgn_default_depths.push_back( level_range::parse(frags[j]) );
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
    return dgn_depth_proc(ls, dgn_default_depths, 1);
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
            map->place = luaL_checkstring(ls, 2);
    }
    PLUARET(string, map->place.c_str());
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

static int dgn_subst(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) == 1)
        return dlua_stringtable(ls, map->get_subst_strings());

    for (int i = 2, size = lua_gettop(ls); i <= size; ++i)
    {
        if (lua_isnil(ls, i))
            map->map.clear_substs();
        else
        {
            std::string err = map->map.add_subst(luaL_checkstring(ls, i));
            if (!err.empty())
                luaL_error(ls, err.c_str());
        }
    }

    return (0);    
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

static int dgn_name(lua_State *ls)
{
    MAP(ls, 1, map);
    PLUARET(string, map->name.c_str());
}

static const struct luaL_reg dgn_lib[] =
{
    { "default_depth", dgn_default_depth },
    { "name", dgn_name },
    { "depth", dgn_depth },
    { "place", dgn_place },
    { "tags",  dgn_tags },
    { "tags_remove", dgn_tags_remove },
    { "chance", dgn_weight },
    { "weight", dgn_weight },
    { "orient", dgn_orient },
    { "shuffle", dgn_shuffle },
    { "shuffle_remove", dgn_shuffle_remove },
    { "subst", dgn_subst },
    { "subst_remove", dgn_subst_remove },
    { "map", dgn_map },
    { "mons", dgn_mons },
    { "item", dgn_item },
    { "kfeat", dgn_kfeat },
    { "kitem", dgn_kitem },
    { "kmons", dgn_kmons },
    { NULL, NULL }
};

void init_dungeon_lua()
{
    dlua.execfile("clua/dungeon.lua", true, true);
    luaopen_debug(dlua);
    luaL_newmetatable(dlua, MAP_METATABLE);
    lua_pop(dlua, 1);
    luaL_openlib(dlua, "dgn", dgn_lib, 0);
}
