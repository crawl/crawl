/*
 *  File:       luadgn.cc
 *  Summary:    Dungeon-builder Lua interface.
 *  Created by: dshaligram on Sat Jun 23 20:02:09 2007 UTC
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include <sstream>
#include <algorithm>
#include <memory>
#include <cmath>

#include "branch.h"
#include "chardump.h"
#include "clua.h"
#include "cloud.h"
#include "describe.h"
#include "directn.h"
#include "dungeon.h"
#include "files.h"
#include "initfile.h"
#include "items.h"
#include "luadgn.h"
#include "mapdef.h"
#include "mapmark.h"
#include "maps.h"
#include "misc.h"
#include "mon-util.h"
#include "monplace.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "tags.h"
#include "terrain.h"
#include "view.h"

#ifdef UNIX
#include <sys/time.h>
#include <time.h>
#endif

#define MONSLIST_METATABLE "crawldgn.monster_list"
#define ITEMLIST_METATABLE "crawldgn.item_list"

static mons_list _lua_get_mlist(lua_State *ls, int ndx);
static item_list _lua_get_ilist(lua_State *ls, int ndx);

template <class T>
static void _push_object_type(lua_State *ls, const char *meta, const T &data)
{
    T **ptr = clua_new_userdata<T*>(ls, meta);
    *ptr = new T(data);
}

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

int dlua_chunk::run(CLua &interp)
{
    int err = load(interp);
    if (err)
        return (err);
    // callfn returns true on success, but we want to return 0 on success.
    return (check_op(interp, !interp.callfn(NULL, 0, 0)));
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

#define LUAFN(name) static int name(lua_State *ls)

static dungeon_feature_type _get_lua_feature(lua_State *ls, int idx)
{
    dungeon_feature_type feat = (dungeon_feature_type)0;
    if (lua_isnumber(ls, idx))
        feat = (dungeon_feature_type)luaL_checkint(ls, idx);
    else if (lua_isstring(ls, idx))
        feat = dungeon_feature_by_name(luaL_checkstring(ls, idx));
    else
        luaL_argerror(ls, idx, "Feature must be a string or a feature index.");

    return feat;
}

static dungeon_feature_type _check_lua_feature(lua_State *ls, int idx)
{
    const dungeon_feature_type f = _get_lua_feature(ls, idx);
    if (!f)
        luaL_argerror(ls, idx, "Invalid dungeon feature");
    return (f);
}

static inline bool _lua_boolean(lua_State *ls, int ndx, bool defval)
{
    return lua_isnone(ls, ndx)? defval : lua_toboolean(ls, ndx);
}

#define GETCOORD(c, p1, p2, boundfn)                      \
    coord_def c;                                          \
    c.x = luaL_checkint(ls, p1);                          \
    c.y = luaL_checkint(ls, p2);                          \
    if (!boundfn(c))                                        \
        luaL_error(                                             \
            ls,                                                 \
            make_stringf("Point (%d,%d) is out of bounds",      \
                         c.x, c.y).c_str());                    \
    else ;                                                      \

#define COORDS(c, p1, p2)                                \
    GETCOORD(c, p1, p2, in_bounds)

#define FEAT(f, pos) \
    dungeon_feature_type f = _check_lua_feature(ls, pos)

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

static int dgn_set_random_mon_list(lua_State *ls)
{
    // Don't complain if we're being called when the map is being loaded
    // and validated.
    if (you.level_type != LEVEL_PORTAL_VAULT &&
        !(you.start_time == 0 && !you.entering_level && !Generating_Level))
    {
        luaL_error(ls, "Can only be used in portal vaults.");
        return (0);
    }

    const int nargs = lua_gettop(ls);

    map_def *map = NULL;
    if (nargs > 2)
    {
        luaL_error(ls, "Too many arguments.");
        return (0);
    }
    else if (nargs == 0)
    {
        luaL_error(ls, "Too few arguments.");
        return (0);
    }
    else if (nargs == 2)
    {
        map_def **_map(
                clua_get_userdata<map_def*>(ls, MAP_METATABLE, 1));
        map = *_map;
    }

    if (map)
    {
        if (map->orient != MAP_ENCOMPASS || map->place.is_valid()
            || !map->depths.empty())
        {
            luaL_error(ls, "Can only be used in portal vaults.");
            return (0);
        }
    }

    int       list_pos = (map != NULL) ? 2 : 1;
    mons_list mlist    = _lua_get_mlist(ls, list_pos);

    if (mlist.size() == 0)
    {
        luaL_argerror(ls, list_pos, "Mon list is empty.");
        return (0);
    }

    if (mlist.size() > 1)
    {
        luaL_argerror(ls, list_pos, "Mon list must contain only one slot.");
        return (0);
    }

    const int num_mons = mlist.slot_size(0);

    if (num_mons == 0)
    {
        luaL_argerror(ls, list_pos, "Mon list is empty.");
        return (0);
    }

    std::vector<mons_spec> mons;
    int num_lords = 0;
    for (int i = 0; i < num_mons; i++)
    {
        mons_spec mon = mlist.get_monster(0, i);

        // Pandemonium lords are pseudo-unique, so don't randomly generate
        // them.
        if (mon.mid == MONS_PANDEMONIUM_DEMON)
        {
            num_lords++;
            continue;
        }

        std::string name;
        if (mon.place.is_valid())
        {
            if (mon.place.level_type == LEVEL_LABYRINTH
                || mon.place.level_type == LEVEL_PORTAL_VAULT)
            {
                std::string err;
                err = make_stringf("mon #%d: Can't use Lab or Portal as a "
                                   "monster place.", i + 1);
                luaL_argerror(ls, list_pos, err.c_str());
                return(0);
            }
            name = mon.place.describe();
        }
        else
        {
            if (mon.mid == RANDOM_MONSTER || mon.monbase == RANDOM_MONSTER)
            {
                std::string err;
                err = make_stringf("mon #%d: can't use random monster in "
                                   "list specifying random monsters", i + 1);
                luaL_argerror(ls, list_pos, err.c_str());
                return(0);
            }
            if (mon.mid == -1)
                mon.mid = MONS_PROGRAM_BUG;
            name = mons_type_name(mon.mid, DESC_PLAIN);
        }

        mons.push_back(mon);

        if (mon.number != 0)
            mprf(MSGCH_ERROR, "dgn.set_random_mon_list() : number for %s "
                              "being discarded.",
                 name.c_str());

        if (mon.band)
            mprf(MSGCH_ERROR, "dgn.set_random_mon_list() : band request for "
                              "%s being ignored.",
                 name.c_str());

        if (mon.colour != BLACK)
            mprf(MSGCH_ERROR, "dgn.set_random_mon_list() : colour for "
                              "%s being ignored.",
                 name.c_str());

        if (mon.items.size() > 0)
            mprf(MSGCH_ERROR, "dgn.set_random_mon_list() : items for "
                              "%s being ignored.",
                 name.c_str());
    } // for (int i = 0; i < num_mons; i++)

    if (mons.size() == 0 && num_lords > 0)
    {
        luaL_argerror(ls, list_pos,
                      "Mon list contains only pandemonium lords.");
        return (0);
    }

    if (map)
        map->random_mons = mons;
    else
        set_vault_mon_list(mons);

    return (0);
}

static int dgn_chance(lua_State *ls)
{
    MAP(ls, 1, map);
    if (!lua_isnil(ls, 2) && !lua_isnil(ls, 3))
    {
        const int chance_priority = luaL_checkint(ls, 2);
        const int chance = luaL_checkint(ls, 3);
        if (chance < 0 || chance > CHANCE_ROLL)
            luaL_argerror(ls, 2,
                          make_stringf("Chance must be in the range [0,%d]",
                                       CHANCE_ROLL).c_str());

        map->chance_priority = chance_priority;
        map->chance = chance;
    }
    PLUARET(number, map->chance);
}

static int dgn_weight(lua_State *ls)
{
    MAP(ls, 1, map);
    if (!lua_isnil(ls, 2))
        map->weight = luaL_checkint(ls, 2);
    PLUARET(number, map->weight);
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

    // map(<map>, x, y) = glyph at (x,y), subject to map being
    // resolved and normalised.
    if (lua_gettop(ls) == 3 && lua_isnumber(ls, 2) && lua_isnumber(ls, 3))
    {
        const int gly = map->map.glyph(luaL_checkint(ls, 2),
                                          luaL_checkint(ls, 3));
        char buf[2] = "";
        buf[0] = gly;
        lua_pushstring(ls, buf);
        return (1);
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

static int dgn_lua_marker(lua_State *ls)
{
    MAP(ls, 1, map);
    if (lua_gettop(ls) != 3 || !lua_isstring(ls, 2)
        || (!lua_isfunction(ls, 3) && !lua_istable(ls, 3)))
    {
        luaL_error(ls, "Expected marker key and marker function/table.");
    }

    CLua &lvm(CLua::get_vm(ls));
    std::string key = lua_tostring(ls, 2);
    lua_datum function(lvm, 3, false);

    const std::string err = map->map.add_lua_marker(key, function);
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

static int dgn_map_size(lua_State *ls)
{
    MAP(ls, 1, map);
    lua_pushnumber(ls, map->map.width());
    lua_pushnumber(ls, map->map.height());
    return (2);
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
    GETCOORD(c, 1, 2, map_bounds);

    if (!lua_isnone(ls, 3))
    {
        const dungeon_feature_type feat = _get_lua_feature(ls, 3);
        if (feat)
            grd(c) = feat;
    }
    PLUARET(number, grd(c));
}

LUARET1(_dgn_is_wall, boolean,
        grid_is_wall(static_cast<dungeon_feature_type>(luaL_checkint(ls, 1))))

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

static int dgn_lfloorcol(lua_State *ls)
{
    MAP(ls, 1, map);

    if (!lua_isnone(ls, 2))
    {
        const char *s = luaL_checkstring(ls, 2);
        int colour = str_to_colour(s);

        if (colour < 0 || colour == BLACK)
        {
            std::string error;

            if (colour == BLACK)
            {
                error = "Can't set floor to black.";
            }
            else
            {
                error = "No such colour as '";
                error += s;
                error += "'";
            }

            luaL_argerror(ls, 2, error.c_str());

            return (0);
        }
        map->floor_colour = (unsigned char) colour;
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
            std::string error;

            if (colour == BLACK)
            {
                error = "Can't set rock to black.";
            }
            else
            {
                error = "No such colour as '";
                error += s;
                error += "'";
            }

            luaL_argerror(ls, 2, error.c_str());

            return (0);
        }

        map->rock_colour = (unsigned char) colour;
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
            std::string error;
            if (colour == forbidden_colour)
                error = std::string("Can't set floor to ") + s;
            else
                error = std::string("Unknown colour: '") + s + "'";
            return luaL_argerror(ls, 1, error.c_str());
        }
        return (colour);
    }
    return luaL_argerror(ls, ndx, "Expected colour name or number");
}

static int dgn_change_floor_colour(lua_State *ls)
{
    const int colour = _lua_colour(ls, 1, BLACK);
    const bool update_now = _lua_boolean(ls, 2, false);

    env.floor_colour = (unsigned char) colour;

    if (crawl_state.need_save && update_now)
        viewwindow(true, false);
    return (0);
}

static int dgn_change_rock_colour(lua_State *ls)
{
    const int colour = _lua_colour(ls, 1, BLACK);
    const bool update_now = _lua_boolean(ls, 2, false);

    env.rock_colour = (unsigned char) colour;

    if (crawl_state.need_save && update_now)
        viewwindow(true, false);
    return (0);
}

static int dgn_colour_at(lua_State *ls)
{
    COORDS(c, 1, 2);
    if (!lua_isnone(ls, 3))
        env.grid_colours(c) = _lua_colour(ls, 3);
    PLUARET(string, colour_to_str(env.grid_colours(c)).c_str());
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
    "dry_fountain_sparkling", "dry_fountain_blood", "permadry_fountain",
    "abandoned_shop"
};

dungeon_feature_type dungeon_feature_by_name(const std::string &name)
{
    COMPILE_CHECK(ARRAYSZ(dngn_feature_names) == NUM_REAL_FEATURES, c1);
    if (name.empty())
        return (DNGN_UNSEEN);

    for (unsigned i = 0; i < ARRAYSZ(dngn_feature_names); ++i)
        if (dngn_feature_names[i] == name)
            return static_cast<dungeon_feature_type>(i);

    return (DNGN_UNSEEN);
}

std::vector<std::string> dungeon_feature_matches(const std::string &name)
{
    std::vector<std::string> matches;

    COMPILE_CHECK(ARRAYSZ(dngn_feature_names) == NUM_REAL_FEATURES, c1);
    if (name.empty())
        return (matches);

    for (unsigned i = 0; i < ARRAYSZ(dngn_feature_names); ++i)
        if (strstr(dngn_feature_names[i], name.c_str()))
            matches.push_back(dngn_feature_names[i]);

    return (matches);
}

const char *dungeon_feature_name(dungeon_feature_type rfeat)
{
    const unsigned feat = rfeat;

    if (feat >= ARRAYSZ(dngn_feature_names))
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
    "monster_dies", "item_pickup", "item_moved", "feat_change",
};

static dgn_event_type dgn_event_type_by_name(const std::string &name)
{
    for (unsigned i = 0; i < ARRAYSZ(dgn_event_type_names); ++i)
        if (dgn_event_type_names[i] == name)
            return static_cast<dgn_event_type>(i? 1 << (i - 1) : 0);
    return (DET_NONE);
}

static const char *dgn_event_type_name(unsigned evmask)
{
    if (evmask == 0)
        return (dgn_event_type_names[0]);

    for (unsigned i = 1; i < ARRAYSZ(dgn_event_type_names); ++i)
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
        feature_description(coord_def(luaL_checkint(ls, 1),
                                      luaL_checkint(ls, 2)),
                            false, dtype, need_stop);
    lua_pushstring(ls, s.c_str());
    return (1);
}

static int dgn_set_feature_desc_short(lua_State *ls)
{
    const std::string base_name = luaL_checkstring(ls, 1);
    const std::string desc      = luaL_checkstring(ls, 2);

    if (base_name.empty())
    {
        luaL_argerror(ls, 1, "Base name can't be empty");
        return (0);
    }

    set_feature_desc_short(base_name, desc);

    return (0);
}

static int dgn_set_feature_desc_long(lua_State *ls)
{
    const std::string raw_name = luaL_checkstring(ls, 1);
    const std::string desc     = luaL_checkstring(ls, 2);

    if (raw_name.empty())
    {
        luaL_argerror(ls, 1, "Raw name can't be empty");
        return (0);
    }

    set_feature_desc_long(raw_name, desc);

    return (0);
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

static int dgn_mons_at(lua_State *ls)
{
    COORDS(c, 1, 2);

    monsters *mon = monster_at(c);
    if (mon && mon->alive())
        push_monster(ls, mon);
    else
        lua_pushnil(ls);
    return (1);
}

static int dgn_items_at(lua_State *ls)
{
    COORDS(c, 1, 2);
    lua_push_items(ls, env.igrid(c));
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

#ifdef USE_TILE
static unsigned int _get_tile_idx(lua_State *ls, int arg)
{
    if (!lua_isstring(ls, arg))
    {
        luaL_argerror(ls, arg, "Expected string for tile name");
        return 0;
    }

    const char *tile_name = luaL_checkstring(ls, arg);

    unsigned int idx;
    if (!tile_dngn_index(tile_name, idx))
    {
        std::string error = "Couldn't find tile '";
        error += tile_name;
        error += "'";
        luaL_argerror(ls, arg, error.c_str());
        return 0;
    }

    return idx;
}
#endif

static int dgn_floor_halo(lua_State *ls)
{
    std::string error = "";

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
        return(0);
    }

    for (int y = 0; y < GYM; ++y)
        for (int x = 0; x < GXM; ++x)
        {
            const dungeon_feature_type feat = grd[x][y];
            if (feat == target)
            {

                for (int i = -1; i <= 1; i++)
                     for (int j = -1; j <= 1; j++)
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

#ifdef USE_TILE
    unsigned int tile = _get_tile_idx(ls, 3);
    if (!tile)
        return (0);
    if (tile_dngn_count(tile) != 9)
    {
        error += "'";
        error += luaL_checkstring(ls, 3);
        error += "' is not a valid halo tile. It has ";
        error += tile_dngn_count(tile);
        error += " variations, but needs exactly 9.";
        luaL_argerror(ls, 3, error.c_str());
        return (0);
    }

    tile_floor_halo(target, tile);
#endif

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

static int make_a_lua_cloud(coord_def where, int garbage, int spread_rate,
                            cloud_type ctype, kill_category whose,
                            killer_type killer)
{
    UNUSED( garbage );

    const int pow = random_range(lua_cloud_pow_min,
                                 lua_cloud_pow_max,
                                 lua_cloud_pow_rolls);
    place_cloud( ctype, where, pow, whose, killer, spread_rate );
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

    apply_area_cloud(make_a_lua_cloud, coord_def(x, y), 0, size,
                     ctype, kc, cloud_struct::whose_to_killer(kc),
                     spread_rate);

    return (0);
}

static void _clamp_to_bounds(int &x, int &y, bool edge_ok = false)
{
    const int edge_offset = edge_ok ? 0 : 1;
    x = std::min(std::max(x, X_BOUND_1 + edge_offset), X_BOUND_2 - edge_offset);
    y = std::min(std::max(y, Y_BOUND_1 + edge_offset), Y_BOUND_2 - edge_offset);
}

static int dgn_fill_area(lua_State *ls)
{
    int x1 = luaL_checkint(ls, 1);
    int y1 = luaL_checkint(ls, 2);
    int x2 = luaL_checkint(ls, 3);
    int y2 = luaL_checkint(ls, 4);
    dungeon_feature_type feat = _check_lua_feature(ls, 5);

    _clamp_to_bounds(x1, y1);
    _clamp_to_bounds(x2, y2);
    if (x2 < x1)
        std::swap(x1, x2);
    if (y2 < y1)
        std::swap(y1, y2);

    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            grd[x][y] = feat;

    return 0;
}

static int dgn_replace_area(lua_State *ls)
{
    int x1 = luaL_checkint(ls, 1);
    int y1 = luaL_checkint(ls, 2);
    int x2 = luaL_checkint(ls, 3);
    int y2 = luaL_checkint(ls, 4);
    dungeon_feature_type search = _check_lua_feature(ls, 5);
    dungeon_feature_type replace = _check_lua_feature(ls, 6);

    // gracefully handle out of bound areas by truncating them.
    _clamp_to_bounds(x1, y1);
    _clamp_to_bounds(x2, y2);
    if (x2 < x1)
        std::swap(x1, x2);
    if (y2 < y1)
        std::swap(y1, y2);

    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            if (grd[x][y] == search)
                grd[x][y] = replace;

    return 0;
}

static int dgn_octa_room(lua_State *ls)
{
    int x1 = luaL_checkint(ls, 1);
    int y1 = luaL_checkint(ls, 2);
    int x2 = luaL_checkint(ls, 3);
    int y2 = luaL_checkint(ls, 4);
    int oblique = luaL_checkint(ls, 5);
    dungeon_feature_type fill = _check_lua_feature(ls, 6);

    spec_room sr;
    sr.tl.x = x1;
    sr.br.x = x2;
    sr.tl.y = y1;
    sr.br.y = y2;

    octa_room(sr, oblique, fill);

    return 0;
}

static int dgn_make_pillars(lua_State *ls)
{
    int center_x = luaL_checkint(ls, 1);
    int center_y = luaL_checkint(ls, 2);
    int num = luaL_checkint(ls, 3);
    int scale_x = luaL_checkint(ls, 4);
    int big_radius = luaL_checkint(ls, 5);
    int pillar_radius = luaL_checkint(ls, 6);
    dungeon_feature_type fill = _check_lua_feature(ls, 8);

    // [enne] The underscore is for DJGPP's brain damage.
    const float _PI = 3.14159265f;
    for (int n = 0; n < num; n++)
    {
        float angle = n * 2 * _PI / (float)num;
        int x = (int)std::floor(std::cos(angle) * big_radius * scale_x + 0.5f);
        int y = (int)std::floor(std::sin(angle) * big_radius + 0.5f);

        lua_pushvalue(ls, 7);
        lua_pushnumber(ls, center_x + x);
        lua_pushnumber(ls, center_y + y);
        lua_pushnumber(ls, pillar_radius);
        lua_pushnumber(ls, fill);

        lua_call(ls, 4, 0);
    }

    return 0;
}

static int dgn_make_square(lua_State *ls)
{
    int center_x = luaL_checkint(ls, 1);
    int center_y = luaL_checkint(ls, 2);
    int radius = std::abs(luaL_checkint(ls, 3));
    dungeon_feature_type fill = _check_lua_feature(ls, 4);

    for (int x = -radius; x <= radius; x++)
        for (int y = -radius; y <= radius; y++)
            grd[center_x + x][center_y + y] = fill;

    return 0;
}

static int dgn_make_rounded_square(lua_State *ls)
{
    int center_x = luaL_checkint(ls, 1);
    int center_y = luaL_checkint(ls, 2);
    int radius = std::abs(luaL_checkint(ls, 3));
    dungeon_feature_type fill = _check_lua_feature(ls, 4);

    for (int x = -radius; x <= radius; x++)
        for (int y = -radius; y <= radius; y++)
            if (std::abs(x) != radius || std::abs(y) != radius)
                grd[center_x + x][center_y + y] = fill;

    return 0;
}

static int dgn_make_circle(lua_State *ls)
{
    int center_x = luaL_checkint(ls, 1);
    int center_y = luaL_checkint(ls, 2);
    int radius = std::abs(luaL_checkint(ls, 3));
    dungeon_feature_type fill = _check_lua_feature(ls, 4);

    for (int x = -radius; x <= radius; x++)
        for (int y = -radius; y <= radius; y++)
            if (x * x + y * y < radius * radius)
                grd[center_x + x][center_y + y] = fill;

    return 0;
}

static int dgn_in_bounds(lua_State *ls)
{
    int x = luaL_checkint(ls, 1);
    int y = luaL_checkint(ls, 2);

    lua_pushboolean(ls, in_bounds(x, y));
    return 1;
}

static int dgn_replace_first(lua_State *ls)
{
    int x = luaL_checkint(ls, 1);
    int y = luaL_checkint(ls, 2);
    int dx = luaL_checkint(ls, 3);
    int dy = luaL_checkint(ls, 4);
    dungeon_feature_type search = _check_lua_feature(ls, 5);
    dungeon_feature_type replace = _check_lua_feature(ls, 6);

    _clamp_to_bounds(x, y);
    bool found = false;
    while (in_bounds(x, y))
    {
        if (grd[x][y] == search)
        {
            grd[x][y] = replace;
            found = true;
            break;
        }

        x += dx;
        y += dy;
    }

    lua_pushboolean(ls, found);
    return 1;
}

static int dgn_replace_random(lua_State *ls)
{
    dungeon_feature_type search = _check_lua_feature(ls, 1);
    dungeon_feature_type replace = _check_lua_feature(ls, 2);

    int x, y;
    do
    {
        x = random2(GXM);
        y = random2(GYM);
    }
    while (grd[x][y] != search);

    grd[x][y] = replace;

    return 0;
}

static int dgn_spotty_level(lua_State *ls)
{
    bool seeded = lua_toboolean(ls, 1);
    int iterations = luaL_checkint(ls, 2);
    bool boxy = lua_toboolean(ls, 3);

    spotty_level(seeded, iterations, boxy);
    return 0;
}

static int dgn_smear_feature(lua_State *ls)
{
    int iterations = luaL_checkint(ls, 1);
    bool boxy = lua_toboolean(ls, 2);
    dungeon_feature_type feat = _check_lua_feature(ls, 3);

    int x1 = luaL_checkint(ls, 4);
    int y1 = luaL_checkint(ls, 5);
    int x2 = luaL_checkint(ls, 6);
    int y2 = luaL_checkint(ls, 7);

    _clamp_to_bounds(x1, y1, true);
    _clamp_to_bounds(x2, y2, true);

    smear_feature(iterations, boxy, feat, x1, y1, x2, y2);

    return 0;
}

static int dgn_count_feature_in_box(lua_State *ls)
{
    int x1 = luaL_checkint(ls, 1);
    int y1 = luaL_checkint(ls, 2);
    int x2 = luaL_checkint(ls, 3);
    int y2 = luaL_checkint(ls, 4);
    dungeon_feature_type feat = _check_lua_feature(ls, 5);

    lua_pushnumber(ls, count_feature_in_box(x1, y1, x2, y2, feat));
    return 1;
}

static int dgn_count_antifeature_in_box(lua_State *ls)
{
    int x1 = luaL_checkint(ls, 1);
    int y1 = luaL_checkint(ls, 2);
    int x2 = luaL_checkint(ls, 3);
    int y2 = luaL_checkint(ls, 4);
    dungeon_feature_type feat = _check_lua_feature(ls, 5);

    lua_pushnumber(ls, count_antifeature_in_box(x1, y1, x2, y2, feat));
    return 1;
}

static int dgn_count_neighbours(lua_State *ls)
{
    int x = luaL_checkint(ls, 1);
    int y = luaL_checkint(ls, 2);
    dungeon_feature_type feat = _check_lua_feature(ls, 3);

    lua_pushnumber(ls, count_neighbours(x, y, feat));
    return 1;
}

static int dgn_join_the_dots(lua_State *ls)
{
    int from_x = luaL_checkint(ls, 1);
    int from_y = luaL_checkint(ls, 2);
    int to_x = luaL_checkint(ls, 3);
    int to_y = luaL_checkint(ls, 4);
    // TODO enne - push map masks to lua?
    unsigned map_mask = MMT_VAULT;
    bool early_exit = lua_toboolean(ls, 5);

    coord_def from(from_x, from_y);
    coord_def to(to_x, to_y);

    bool ret = join_the_dots(from, to, map_mask, early_exit);
    lua_pushboolean(ls, ret);

    return 1;
}

static int dgn_fill_disconnected_zones(lua_State *ls)
{
    int from_x = luaL_checkint(ls, 1);
    int from_y = luaL_checkint(ls, 2);
    int to_x = luaL_checkint(ls, 3);
    int to_y = luaL_checkint(ls, 4);

    dungeon_feature_type feat = _check_lua_feature(ls, 5);

    process_disconnected_zones(from_x, from_y, to_x, to_y, true, feat);

    return 0;
}

static int _dgn_is_passable(lua_State *ls)
{
    COORDS(c, 1, 2);
    lua_pushboolean(ls, is_travelsafe_square(c, false, true));
    return (1);
}

static int dgn_register_feature_marker(lua_State *ls)
{
    COORDS(c, 1, 2);
    FEAT(feat, 3);
    env.markers.add( new map_feature_marker(c, feat) );
    return (0);
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
    return (0);
}

static int dgn_create_monster(lua_State *ls)
{
    COORDS(c, 1, 2);

    mons_list mlist = _lua_get_mlist(ls, 3);
    for (int i = 0, size = mlist.size(); i < size; ++i)
    {
        mons_spec mspec = mlist.get_monster(i);
        const int mid = dgn_place_monster(mspec, you.your_level, c,
                                          false, false, false);
        if (mid != -1)
        {
            push_monster(ls, &menv[mid]);
            return (1);
        }
    }
    lua_pushnil(ls);
    return (1);
}

static int _dgn_monster_spec(lua_State *ls)
{
    const mons_list mlist = _lua_get_mlist(ls, 1);
    _push_object_type<mons_list>(ls, MONSLIST_METATABLE, mlist);
    return (1);
}

static int _dgn_item_spec(lua_State *ls)
{
    const item_list ilist = _lua_get_ilist(ls, 1);
    _push_object_type<item_list>(ls, ITEMLIST_METATABLE, ilist);
    return (1);
}

LUARET1(_dgn_max_monsters, number, MAX_MONSTERS)

static int dgn_create_item(lua_State *ls)
{
    COORDS(c, 1, 2);

    item_list ilist = _lua_get_ilist(ls, 3);
    const int level =
        lua_isnumber(ls, 4) ? lua_tointeger(ls, 4) : you.your_level;

    dgn_place_multiple_items(ilist, c, level);
    link_items();
    return (0);
}

static std::auto_ptr<lua_datum> _dgn_map_safe_bounds_fn;

static bool _lua_map_place_valid(const map_def &map,
                                 const coord_def &c,
                                 const coord_def &size)
{
#ifdef DEBUG_DIAGNOSTICS
    mprf(MSGCH_DIAGNOSTICS, "lua_map_place_invalid: (%d,%d) (%d,%d)",
         c.x, c.y, size.x, size.y);
#endif

    lua_stack_cleaner clean(_dgn_map_safe_bounds_fn->lua);

    // Push the Lua function onto the stack.
    _dgn_map_safe_bounds_fn->push();

    lua_State *ls = _dgn_map_safe_bounds_fn->lua;

    // Push map, pos.x, pos.y, size.x, size.y
    clua_push_map(ls, const_cast<map_def*>(&map));
    clua_push_coord(ls, c);
    clua_push_coord(ls, size);

    const int err = lua_pcall(ls, 5, 1, 0);

    // Lua error invalidates place.
    if (err)
    {
        mprf(MSGCH_ERROR, "Lua error: %s", lua_tostring(ls, -1));
        return (true);
    }

    return (lua_toboolean(ls, -1));
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

        _dgn_map_safe_bounds_fn.reset(NULL);
    }

    if (err)
        lua_error(ls);

    return (1);
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
                map_anchor_points.push_back(
                    coord_def( lua_tointeger(ls, i),
                               lua_tointeger(ls, i + 1) ) );
        }

        ASSERT(lua_isfunction(ls, -1));

        lua_pushvalue(ls, -1);
        err = lua_pcall(ls, 0, 1, 0);
    }
    if (err)
        lua_error(ls);
    return (1);
}

#define BRANCH(br, pos)                                                 \
    const char *branch_name = luaL_checkstring(ls, pos);                \
    branch_type req_branch_type = str_to_branch(branch_name);           \
    if (req_branch_type == NUM_BRANCHES)                                \
        luaL_error(ls, "Expected branch name");                         \
    Branch &br = branches[req_branch_type]

#define BRANCHFN(name, type, expr)   \
    LUAFN(dgn_br_##name) {           \
    BRANCH(br, 1);                   \
    PLUARET(type, expr);             \
    }

BRANCHFN(floorcol, number, br.floor_colour)
BRANCHFN(rockcol, number, br.rock_colour)
BRANCHFN(has_shops, boolean, br.has_shops)
BRANCHFN(has_uniques, boolean, br.has_uniques)
BRANCHFN(parent_branch, string,
         br.parent_branch == NUM_BRANCHES ? ""
         : branches[br.parent_branch].abbrevname)

#define LEVEL(lev, br, pos)                                             \
    const char *level_name = luaL_checkstring(ls, pos);                 \
    level_area_type lev = str_to_level_area_type(level_name);           \
    if (lev == NUM_LEVEL_AREA_TYPES)                                    \
        luaL_error(ls, "Expected level name");                          \
    const char *branch_name = luaL_checkstring(ls, pos);                \
    branch_type br = str_to_branch(branch_name);                        \
    if (lev == LEVEL_DUNGEON && br == NUM_BRANCHES)                     \
        luaL_error(ls, "Expected branch name");

static void push_level_id(lua_State *ls, const level_id &lid)
{
    // We're skipping the constructor; naughty, but level_id has no
    // virtual methods and no dynamically allocated memory.
    level_id *nlev =
        static_cast<level_id*>(lua_newuserdata(ls, sizeof(level_id)));
    *nlev = lid;
}

static level_id _lua_level_id(lua_State *ls, int ndx)
{
    if (lua_isstring(ls, ndx))
    {
        const char *s = lua_tostring(ls, 1);
        try
        {
            return level_id::parse_level_id(s);
        }
        catch (const std::string &err)
        {
            luaL_error(ls, err.c_str());
        }
    }
    else if (lua_isuserdata(ls, ndx))
    {
        const level_id *lid = static_cast<level_id*>(lua_touserdata(ls, ndx));
        return (*lid);
    }

    luaL_argerror(ls, ndx, "Expected level_id");
    // Never gets here.
    return level_id();
}

LUAFN(dgn_level_id)
{
    const int nargs = lua_gettop(ls);
    if (!nargs)
        push_level_id(ls, level_id::current());
    else if (nargs == 1)
        push_level_id(ls, _lua_level_id(ls, 1));
    return (1);
}

LUAFN(dgn_level_name)
{
    const level_id lid(_lua_level_id(ls, 1));
    lua_pushstring(ls, lid.describe().c_str());
    return (1);
}

LUAFN(dgn_set_level_type_name)
{
    if (you.level_type != LEVEL_PORTAL_VAULT)
    {
        luaL_error(ls, "Can only set level type name on portal vaults");
        return(0);
    }

    if (!lua_isstring(ls, 1))
    {
        luaL_argerror(ls, 1, "Expected string for level type name");
        return(0);
    }

    you.level_type_name = luaL_checkstring(ls, 1);

    return(0);
}

LUAFN(dgn_set_level_type_name_abbrev)
{
    if (you.level_type != LEVEL_PORTAL_VAULT)
    {
        luaL_error(ls, "Can only set level type name abbreviation on "
                       "portal vaults");
        return(0);
    }

    if (!lua_isstring(ls, 1))
    {
        luaL_argerror(ls, 1, "Expected string for level type name "
                             "abbreviation");
        return(0);
    }

    you.level_type_name_abbrev = luaL_checkstring(ls, 1);

    return(0);
}

LUAFN(dgn_set_level_type_origin)
{
    if (you.level_type != LEVEL_PORTAL_VAULT)
    {
        luaL_error(ls, "Can only set level type origin on portal vaults");
        return(0);
    }

    if (!lua_isstring(ls, 1))
    {
        luaL_argerror(ls, 1, "Expected string for level type origin");
        return(0);
    }

    you.level_type_origin = luaL_checkstring(ls, 1);

    return(0);
}

static int _lua_push_map(lua_State *ls, const map_def *map)
{
    if (map)
        clua_push_map(ls, const_cast<map_def*>(map));
    else
        lua_pushnil(ls);
    return (1);
}

LUAFN(dgn_map_by_tag)
{
    if (const char *tag = luaL_checkstring(ls, 1))
    {
        const bool check_depth = _lua_boolean(ls, 3, true);
        return _lua_push_map(ls, random_map_for_tag(tag, check_depth));
    }
    return (0);
}

LUAFN(dgn_map_in_depth)
{
    const level_id lid = _lua_level_id(ls, 1);
    const bool mini = _lua_boolean(ls, 2, true);
    return _lua_push_map(ls, random_map_in_depth(lid, mini));
}

LUAFN(dgn_map_by_place)
{
    const level_id lid = _lua_level_id(ls, 1);
    return _lua_push_map(ls, random_map_for_place(lid));
}

LUAFN(_dgn_place_map)
{
    MAP(ls, 1, map);
    const bool clobber = _lua_boolean(ls, 2, false);
    const bool no_exits = _lua_boolean(ls, 3, false);
    coord_def where(-1, -1);
    if (lua_isnumber(ls, 4) && lua_isnumber(ls, 5))
    {
        COORDS(c, 4, 5);
        where = c;
    }
    if (dgn_place_map(map, clobber, no_exits, where) && !Level_Vaults.empty())
        lua_pushlightuserdata(ls, &Level_Vaults[Level_Vaults.size() - 1]);
    else
        lua_pushnil(ls);
    return (1);
}

LUAFN(_dgn_in_vault)
{
    GETCOORD(c, 1, 2, map_bounds);
    const int mask = lua_isnone(ls, 3) ? MMT_VAULT : lua_tointeger(ls, 3);
    lua_pushboolean(ls, dgn_Map_Mask(c) & mask);
    return (1);
}

LUAFN(_dgn_find_marker_prop)
{
    const char *prop = luaL_checkstring(ls, 1);
    const std::string value(
        lua_gettop(ls) >= 2 ? luaL_checkstring(ls, 2) : "");
    const coord_def place = find_marker_prop(prop, value);
    if (map_bounds(place))
        clua_push_coord(ls, place);
    else
    {
        lua_pushnil(ls);
        lua_pushnil(ls);
    }
    return (2);
}

extern spec_room lua_special_room_spec;
extern int       lua_special_room_level;

LUAFN(dgn_get_special_room_info)
{
     if (!lua_special_room_spec.created || !in_bounds(lua_special_room_spec.tl)
         || lua_special_room_level == -1)
     {
         return (0);
     }

     lua_pushnumber(ls,  lua_special_room_level);
     dlua_push_coord(ls, lua_special_room_spec.tl);
     dlua_push_coord(ls, lua_special_room_spec.br);

     return (5);
}

LUAFN(_dgn_resolve_map)
{
    if (lua_isnil(ls, 1))
    {
        lua_pushnil(ls);
        return (1);
    }

    MAP(ls, 1, map);
    const bool check_collisions = _lua_boolean(ls, 2, true);

    // Save the vault_placement into Temp_Vaults because the map_def
    // will need to be alive through to the end of dungeon gen.
    Temp_Vaults.push_back(vault_placement());

    vault_placement &place(Temp_Vaults[Temp_Vaults.size() - 1]);

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
    return (2);
}

LUAFN(_dgn_reuse_map)
{
    if (!lua_isuserdata(ls, 1))
        luaL_argerror(ls, 1, "Expected vault_placement");

    vault_placement &vp(
        *static_cast<vault_placement*>(lua_touserdata(ls, 1)));

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

    return (0);
}

LUAFN(dgn_lev_floortile)
{
#ifdef USE_TILE
    LEVEL(lev, br, 1);

    tile_flavour flv;
    tile_default_flv(lev, br, flv);

    const char *tile_name = tile_dngn_name(flv.floor);
    PLUARET(string, tile_name);
#else
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_lev_rocktile)
{
#ifdef USE_TILE
    LEVEL(lev, br, 1);

    tile_flavour flv;
    tile_default_flv(lev, br, flv);

    const char *tile_name = tile_dngn_name(flv.wall);
    PLUARET(string, tile_name);
#else
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_lrocktile)
{
    MAP(ls, 1, map);

#ifdef USE_TILE
    unsigned short tile = _get_tile_idx(ls, 2);
    map->rock_tile = tile;

    const char *tile_name = tile_dngn_name(tile);
    PLUARET(string, tile_name);
#else
    UNUSED(map);
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_lfloortile)
{
    MAP(ls, 1, map);

#ifdef USE_TILE
    unsigned short tile = _get_tile_idx(ls, 2);
    map->floor_tile = tile;

    const char *tile_name = tile_dngn_name(tile);
    PLUARET(string, tile_name);
#else
    UNUSED(map);
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_change_rock_tile)
{
#ifdef USE_TILE
    unsigned short tile = _get_tile_idx(ls, 1);
    if (tile)
        env.tile_default.wall = tile;

    const char *tile_name = tile_dngn_name(tile);
    PLUARET(string, tile_name);
#else
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_change_floor_tile)
{
#ifdef USE_TILE
    unsigned short tile = _get_tile_idx(ls, 1);
    if (tile)
        env.tile_default.floor = tile;

    const char *tile_name = tile_dngn_name(tile);
    PLUARET(string, tile_name);
#else
    PLUARET(string, "invalid");
#endif
}

LUAFN(dgn_ftile)
{
#ifdef USE_TILE
    return dgn_map_add_transform(ls,
                                 &map_lines::add_floortile,
                                 &map_lines::clear_floortiles);
#else
    return 0;
#endif
}

LUAFN(dgn_rtile)
{
#ifdef USE_TILE
    return dgn_map_add_transform(ls,
                                 &map_lines::add_rocktile,
                                 &map_lines::clear_rocktiles);
#else
    return 0;
#endif
}

LUAFN(dgn_debug_dump_map)
{
    const int pos = lua_isuserdata(ls, 1) ? 2 : 1;
    if (lua_isstring(ls, pos))
        dump_map(lua_tostring(ls, pos), true);
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
    { "chance", dgn_chance },
    { "weight", dgn_weight },
    { "welcome", dgn_welcome },
    { "weight", dgn_weight },
    { "orient", dgn_orient },
    { "shuffle", dgn_shuffle },
    { "shuffle_remove", dgn_shuffle_remove },
    { "subst", dgn_subst },
    { "nsubst", dgn_nsubst },
    { "colour", dgn_colour },
    { "lfloorcol", dgn_lfloorcol},
    { "lrockcol", dgn_lrockcol},
    { "subst_remove", dgn_subst_remove },
    { "map", dgn_map },
    { "mons", dgn_mons },
    { "item", dgn_item },
    { "marker", dgn_marker },
    { "lua_marker", dgn_lua_marker },
    { "kfeat", dgn_kfeat },
    { "kitem", dgn_kitem },
    { "kmons", dgn_kmons },
    { "kmask", dgn_kmask },
    { "mapsize", dgn_map_size },

    { "grid", dgn_grid },
    { "is_wall", _dgn_is_wall },
    { "max_bounds", dgn_max_bounds },
    { "colour_at", dgn_colour_at },

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
    { "num_matching_markers", dgn_num_matching_markers },
    { "feature_desc", dgn_feature_desc },
    { "feature_desc_at", dgn_feature_desc_at },
    { "set_feature_desc_short", dgn_set_feature_desc_short },
    { "set_feature_desc_long", dgn_set_feature_desc_long },
    { "item_from_index", dgn_item_from_index },
    { "mons_from_index", dgn_mons_from_index },
    { "mons_at", dgn_mons_at },
    { "items_at", dgn_items_at },
    { "change_level_flags", dgn_change_level_flags },
    { "change_branch_flags", dgn_change_branch_flags },
    { "set_random_mon_list", dgn_set_random_mon_list },
    { "get_floor_colour", dgn_get_floor_colour },
    { "get_rock_colour",  dgn_get_rock_colour },
    { "change_floor_colour", dgn_change_floor_colour },
    { "change_rock_colour",  dgn_change_rock_colour },
    { "set_lt_callback", lua_dgn_set_lt_callback },
    { "fixup_stairs", dgn_fixup_stairs },
    { "floor_halo", dgn_floor_halo },
    { "random_walk", dgn_random_walk },
    { "apply_area_cloud", dgn_apply_area_cloud },

    // building routines
    { "fill_area", dgn_fill_area },
    { "replace_area", dgn_replace_area },
    { "octa_room", dgn_octa_room },
    { "make_pillars", dgn_make_pillars },
    { "make_square", dgn_make_square },
    { "make_rounded_square", dgn_make_rounded_square },
    { "make_circle", dgn_make_circle },
    { "in_bounds", dgn_in_bounds },
    { "replace_first", dgn_replace_first },
    { "replace_random", dgn_replace_random },
    { "spotty_level", dgn_spotty_level },
    { "smear_feature", dgn_smear_feature },
    { "count_feature_in_box", dgn_count_feature_in_box },
    { "count_antifeature_in_box", dgn_count_antifeature_in_box },
    { "count_neighbours", dgn_count_neighbours },
    { "join_the_dots", dgn_join_the_dots },
    { "fill_disconnected_zones", dgn_fill_disconnected_zones },

    { "is_passable", _dgn_is_passable },

    { "register_feature_marker", dgn_register_feature_marker },
    { "register_lua_marker", dgn_register_lua_marker },

    { "create_monster", dgn_create_monster },
    { "create_item", dgn_create_item },

    { "monster_spec", _dgn_monster_spec },
    { "item_spec", _dgn_item_spec },

    { "max_monsters", _dgn_max_monsters },

    { "with_map_bounds_fn", dgn_with_map_bounds_fn },
    { "with_map_anchors", dgn_with_map_anchors },

    { "br_floorcol", dgn_br_floorcol },
    { "br_rockcol", dgn_br_rockcol },
    { "br_has_shops", dgn_br_has_shops },
    { "br_has_uniques", dgn_br_has_uniques },
    { "br_parent_branch", dgn_br_parent_branch },

    { "level_id", dgn_level_id },
    { "level_name", dgn_level_name },
    { "set_level_type_name", dgn_set_level_type_name },
    { "set_level_type_name_abbrev", dgn_set_level_type_name_abbrev },
    { "set_level_type_origin", dgn_set_level_type_origin },
    { "map_by_tag", dgn_map_by_tag },
    { "map_in_depth", dgn_map_in_depth },
    { "map_by_place", dgn_map_by_place },
    { "place_map", _dgn_place_map },
    { "reuse_map", _dgn_reuse_map },
    { "resolve_map", _dgn_resolve_map },
    { "in_vault", _dgn_in_vault },

    { "find_marker_prop", _dgn_find_marker_prop },

    { "get_special_room_info", dgn_get_special_room_info },

    { "lrocktile", dgn_lrocktile },
    { "lfloortile", dgn_lfloortile },
    { "rtile", dgn_rtile },
    { "ftile", dgn_ftile },
    { "change_rock_tile", dgn_change_rock_tile },
    { "change_floor_tile", dgn_change_floor_tile },
    { "lev_floortile", dgn_lev_floortile },
    { "lev_rocktile", dgn_lev_rocktile },

    { "debug_dump_map", dgn_debug_dump_map },

    { NULL, NULL }
};

LUAFN(_crawl_args)
{
    return dlua_stringtable(ls, SysEnv.cmd_args);
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

static const struct luaL_reg crawl_lib[] =
{
    { "args", _crawl_args },
#ifdef UNIX
    { "millis", _crawl_millis },
#endif
    { NULL, NULL }
};

static int file_marshall(lua_State *ls)
{
    if (lua_gettop(ls) != 2)
        luaL_error(ls, "Need two arguments: tag header and value");
    writer &th(*static_cast<writer*>( lua_touserdata(ls, 1) ));
    if (lua_isnumber(ls, 2))
        marshallLong(th, luaL_checklong(ls, 2));
    else if (lua_isboolean(ls, 2))
        marshallByte(th, lua_toboolean(ls, 2));
    else if (lua_isstring(ls, 2))
        marshallString(th, lua_tostring(ls, 2));
    else if (lua_isfunction(ls, 2))
    {
        dlua_chunk chunk(ls);
        marshallString(th, chunk.compiled_chunk());
    }
    return (0);
}

static int file_unmarshall_boolean(lua_State *ls)
{
    if (lua_gettop(ls) != 1)
        luaL_error(ls, "Need reader as one argument");
    reader &th(*static_cast<reader*>( lua_touserdata(ls, 1) ));
    lua_pushboolean(ls, unmarshallByte(th));
    return (1);
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
    LPT_NIL,
    LPT_BOOLEAN
};

static int file_marshall_meta(lua_State *ls)
{
    if (lua_gettop(ls) != 2)
        luaL_error(ls, "Need two arguments: tag header and value");

    writer &th(*static_cast<writer*>( lua_touserdata(ls, 1) ));

    lua_persist_type ptype = LPT_NONE;
    if (lua_isnumber(ls, 2))
        ptype = LPT_NUMBER;
    else if (lua_isboolean(ls, 2))
        ptype = LPT_BOOLEAN;
    else if (lua_isstring(ls, 2))
        ptype = LPT_STRING;
    else if (lua_isfunction(ls, 2))
        ptype = LPT_FUNCTION;
    else if (lua_isnil(ls, 2))
        ptype = LPT_NIL;
    else
        luaL_error(ls,
                   make_stringf("Cannot marshall %s",
                                lua_typename(ls, lua_type(ls, 2))).c_str());
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
    case LPT_BOOLEAN:
        return file_unmarshall_boolean(ls);
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
        player_can_hear(coord_def(luaL_checkint(ls,1), luaL_checkint(ls, 2))))
LUARET1(you_x_pos, number, you.pos().x)
LUARET1(you_y_pos, number, you.pos().y)
LUARET2(you_pos, number, you.pos().x, you.pos().y)

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

static mons_list _lua_get_mlist(lua_State *ls, int ndx)
{
    if (lua_isstring(ls, ndx))
    {
        const char *spec = lua_tostring(ls, ndx);
        mons_list mlist;
        const std::string err = mlist.add_mons(spec);
        if (!err.empty())
            luaL_error(ls, err.c_str());
        return (mlist);
    }
    else
    {
        mons_list **mlist(
            clua_get_userdata<mons_list*>(ls, MONSLIST_METATABLE, ndx));
        if (mlist)
            return (**mlist);

        luaL_argerror(ls, ndx, "Expected monster list object or string");
        return mons_list();
    }
}

static item_list _lua_get_ilist(lua_State *ls, int ndx)
{
    if (lua_isstring(ls, ndx))
    {
        const char *spec = lua_tostring(ls, ndx);

        item_list ilist;
        const std::string err = ilist.add_item(spec);
        if (!err.empty())
            luaL_error(ls, err.c_str());

        return (ilist);
    }
    else
    {
        item_list **ilist(
            clua_get_userdata<item_list*>(ls, ITEMLIST_METATABLE, ndx));
        if (ilist)
            return (**ilist);

        luaL_argerror(ls, ndx, "Expected item list object or string");
        return item_list();
    }
}

static void _register_mapdef_tables(lua_State *ls)
{
    clua_register_metatable(ls, MONSLIST_METATABLE, NULL,
                            lua_object_gc<mons_list>);
    clua_register_metatable(ls, ITEMLIST_METATABLE, NULL,
                            lua_object_gc<item_list>);
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

    _register_mapdef_tables(dlua);
}

// Can be called from within a debugger to look at the current Lua
// call stack.  (Borrowed from ToME 3)
void print_dlua_stack(void)
{
    struct lua_Debug dbg;
    int              i = 0;
    lua_State       *L = dlua.state();

    fprintf(stderr, EOL);
    while (lua_getstack(L, i++, &dbg) == 1)
    {
        lua_getinfo(L, "lnuS", &dbg);

        char* file = strrchr(dbg.short_src, '/');
        if (file == NULL)
            file = dbg.short_src;
        else
            file++;

        fprintf(stderr, "%s, function %s, line %d" EOL, file,
                dbg.name, dbg.currentline);
    }

    fprintf(stderr, EOL);
}
