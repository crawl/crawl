/*
 *  File:       mapmark.cc
 *  Summary:    Level markers (annotations).
 *
 *  Modified for Crawl Reference by $Author: dshaligram $ on $Date: 2007-07-20T11:40:25.964128Z $
 *  
 */

#include "AppHdr.h"
#include "mapmark.h"

#include "clua.h"
#include "direct.h"
#include "libutil.h"
#include "luadgn.h"
#include "stuff.h"
#include "tags.h"
#include "view.h"

////////////////////////////////////////////////////////////////////////
// Dungeon markers

map_marker::marker_reader map_marker::readers[NUM_MAP_MARKER_TYPES] =
{
    &map_feature_marker::read,
    &map_lua_marker::read,
};

map_marker::marker_parser map_marker::parsers[NUM_MAP_MARKER_TYPES] =
{
    &map_feature_marker::parse,
    &map_lua_marker::parse,
};

map_marker::map_marker(map_marker_type t, const coord_def &p)
    : pos(p), type(t)
{
}

map_marker::~map_marker()
{
}

void map_marker::activate()
{
}

void map_marker::write(tagHeader &outf) const
{
    marshallShort(outf, type);
    marshallCoord(outf, pos);
}

void map_marker::read(tagHeader &inf)
{
    // Don't read type! The type has to be read by someone who knows how
    // to look up the unmarshall function.
    unmarshallCoord(inf, pos);
}

map_marker *map_marker::read_marker(tagHeader &inf)
{
    const map_marker_type type =
        static_cast<map_marker_type>(unmarshallShort(inf));
    return readers[type]? (*readers[type])(inf, type) : NULL;
}

map_marker *map_marker::parse_marker(
    const std::string &s, const std::string &ctx) throw (std::string)
{
    for (int i = 0; i < NUM_MAP_MARKER_TYPES; ++i)
    {
        if (parsers[i])
        {
            if (map_marker *m = parsers[i](s, ctx))
                return (m);
        }
    }
    return (NULL);
}

////////////////////////////////////////////////////////////////////////////
// map_feature_marker

map_feature_marker::map_feature_marker(
    const coord_def &p,
    dungeon_feature_type _feat)
    : map_marker(MAT_FEATURE, p), feat(_feat)
{
}

map_feature_marker::map_feature_marker(
    const map_feature_marker &other)
    : map_marker(MAT_FEATURE, other.pos), feat(other.feat)
{
}

void map_feature_marker::write(tagHeader &outf) const
{
    this->map_marker::write(outf);
    marshallShort(outf, feat);
}

void map_feature_marker::read(tagHeader &inf)
{
    map_marker::read(inf);
    feat = static_cast<dungeon_feature_type>(unmarshallShort(inf));
}

map_marker *map_feature_marker::read(tagHeader &inf, map_marker_type)
{
    map_marker *mapf = new map_feature_marker();
    mapf->read(inf);
    return (mapf);
}

map_marker *map_feature_marker::parse(
    const std::string &s, const std::string &) throw (std::string)
{
    if (s.find("feat:") != 0)
        return (NULL);
    std::string raw = s;
    strip_tag(raw, "feat:", true);

    const dungeon_feature_type ft = dungeon_feature_by_name(raw);
    if (ft == DNGN_UNSEEN)
        throw make_stringf("Bad feature marker: %s (unknown feature '%s')",
                           s.c_str(), raw.c_str());
    return new map_feature_marker(coord_def(0, 0), ft);
}

std::string map_feature_marker::describe() const
{
    return make_stringf("feature (%s)", dungeon_feature_name(feat));
}

////////////////////////////////////////////////////////////////////////////
// map_lua_marker

map_lua_marker::map_lua_marker()
    : map_marker(MAT_LUA_MARKER, coord_def()), initialised(false)
{
}

map_lua_marker::map_lua_marker(const std::string &s, const std::string &)
    : map_marker(MAT_LUA_MARKER, coord_def()), initialised(false)
{
    lua_stack_cleaner clean(dlua);
    if (dlua.loadstring(("return " + s).c_str(), "lua_marker"))
        mprf(MSGCH_WARN, "lua_marker load error: %s", dlua.error.c_str());
    if (!dlua.callfn("dgn_run_map", 1, 1))
        mprf(MSGCH_WARN, "lua_marker exec error: %s", dlua.error.c_str());
    check_register_table();
}

map_lua_marker::~map_lua_marker()
{
    // Remove the Lua marker table from the registry.
    if (initialised)
    {
        lua_pushlightuserdata(dlua, this);
        lua_pushnil(dlua);
        lua_settable(dlua, LUA_REGISTRYINDEX);
    }
}

void map_lua_marker::check_register_table()
{
    if (!lua_istable(dlua, -1))
    {
        mprf(MSGCH_WARN, "lua_marker: Expected table, didn't get it.");
        initialised = false;
        return;
    }

    // Got a table. Save it in the registry.

    // Key is this.
    lua_pushlightuserdata(dlua, this);
    // Move key before value.
    lua_insert(dlua, -2);
    lua_settable(dlua, LUA_REGISTRYINDEX);

    initialised = true;
}

bool map_lua_marker::get_table() const
{
    // First save the unmarshall Lua function.
    lua_pushlightuserdata(dlua, const_cast<map_lua_marker*>(this));
    lua_gettable(dlua, LUA_REGISTRYINDEX);
    return (lua_istable(dlua, -1));
}

void map_lua_marker::write(tagHeader &th) const
{
    map_marker::write(th);
    
    lua_stack_cleaner clean(dlua);
    bool init = initialised;
    if (!get_table())
    {
        mprf(MSGCH_WARN, "Couldn't find table.");
        init = false;
    }
    
    marshallByte(th, init);
    if (!init)
        return;

    // Call dlua_marker_function(table, 'read')
    lua_pushstring(dlua, "read");
    if (!dlua.callfn("dlua_marker_function", 2, 1))
        end(1, false, "lua_marker: write error: %s", dlua.error.c_str());

    // Right, what's on top should be a function. Save it.
    dlua_chunk reader(dlua);
    if (!reader.error.empty())
        end(1, false, "lua_marker: couldn't save read function: %s",
            reader.error.c_str());

    marshallString(th, reader.compiled_chunk());

    // Okay, saved the reader. Now ask the writer to do its thing.

    // Call: dlua_marker_method(table, fname, marker)
    get_table();
    lua_pushstring(dlua, "write");
    lua_pushlightuserdata(dlua, const_cast<map_lua_marker*>(this));
    lua_pushlightuserdata(dlua, &th);

    if (!dlua.callfn("dlua_marker_method", 4))
        end(1, false, "lua_marker::write error: %s", dlua.error.c_str());
}

void map_lua_marker::read(tagHeader &th)
{
    map_marker::read(th);
    
    if (!(initialised = unmarshallByte(th)))
        return;

    lua_stack_cleaner cln(dlua);
    // Read the Lua chunk we saved.
    const std::string compiled = unmarshallString(th, LUA_CHUNK_MAX_SIZE);

    dlua_chunk chunk = dlua_chunk::precompiled(compiled);
    if (chunk.load(dlua))
        end(1, false, "lua_marker::read error: %s", chunk.error.c_str());
    dlua_push_userdata(dlua, this, MAPMARK_METATABLE);
    lua_pushlightuserdata(dlua, &th);
    if (!dlua.callfn("dlua_marker_read", 3, 1))
        end(1, false, "lua_marker::read error: %s", dlua.error.c_str());

    // Right, what's on top had better be a table.
    check_register_table();
}

map_marker *map_lua_marker::read(tagHeader &th, map_marker_type)
{
    map_marker *marker = new map_lua_marker;
    marker->read(th);
    return (marker);
}

void map_lua_marker::push_fn_args(const char *fn) const
{
    get_table();
    lua_pushstring(dlua, fn);
    dlua_push_userdata(dlua, this, MAPMARK_METATABLE);
}

bool map_lua_marker::callfn(const char *fn, bool warn_err) const
{
    const int top = lua_gettop(dlua);
    push_fn_args(fn);
    const bool res =
        dlua.callfn("dlua_marker_method", lua_gettop(dlua) - top, 1);
    if (!res && warn_err)
        mprf(MSGCH_WARN, "mlua error: %s", dlua.error.c_str());
    return (res);
}

void map_lua_marker::activate()
{
    lua_stack_cleaner clean(dlua);
    callfn("activate", true);
}

void map_lua_marker::notify_dgn_event(const dgn_event &e)
{
    lua_stack_cleaner clean(dlua);
    push_fn_args("event");
    clua_push_dgn_event(dlua, &e);
    if (!dlua.callfn("dlua_marker_method", 4, 0))
        mprf(MSGCH_WARN, "notify_dgn_event: Lua error: %s",
             dlua.error.c_str());
}

std::string map_lua_marker::describe() const
{
    lua_stack_cleaner cln(dlua);
    if (!callfn("describe"))
        return make_stringf("error: %s", dlua.error.c_str());

    std::string desc;
    if (lua_isstring(dlua, -1))
        desc = lua_tostring(dlua, -1);
    return desc;
}

map_marker *map_lua_marker::parse(
    const std::string &s, const std::string &ctx) throw (std::string)
{
    if (s.find("lua:") != 0)
        return (NULL);
    std::string raw = s;
    strip_tag(raw, "lua:");
    map_lua_marker *mark = new map_lua_marker(raw, ctx);
    if (!mark->initialised)
    {
        delete mark;
        throw make_stringf("Unable to initialise Lua marker from '%s'",
                           raw.c_str());
    }
    return (mark);
}

//////////////////////////////////////////////////////////////////////////
// Map markers in env.

void env_activate_markers()
{
    for (dgn_marker_map::iterator i = env.markers.begin();
         i != env.markers.end(); ++i)
    {
        i->second->activate();
    }
}

void env_add_marker(map_marker *marker)
{
    env.markers.insert(dgn_pos_marker(marker->pos, marker));
}

void env_remove_marker(map_marker *marker)
{
    std::pair<dgn_marker_map::iterator, dgn_marker_map::iterator>
        els = env.markers.equal_range(marker->pos);
    for (dgn_marker_map::iterator i = els.first; i != els.second; ++i)
    {
        if (i->second == marker)
        {
            env.markers.erase(i);
            break;
        }
    }
    delete marker;
}

void env_remove_markers_at(const coord_def &c,
                           map_marker_type type)
{
    std::pair<dgn_marker_map::iterator, dgn_marker_map::iterator>
        els = env.markers.equal_range(c);
    for (dgn_marker_map::iterator i = els.first; i != els.second; )
    {
        dgn_marker_map::iterator todel = i++;
        if (type == MAT_ANY || todel->second->get_type() == type)
        {
            delete todel->second;
            env.markers.erase(todel);
        }
    }
}

map_marker *env_find_marker(const coord_def &c, map_marker_type type)
{
    std::pair<dgn_marker_map::const_iterator, dgn_marker_map::const_iterator>
        els = env.markers.equal_range(c);
    for (dgn_marker_map::const_iterator i = els.first; i != els.second; ++i)
        if (type == MAT_ANY || i->second->get_type() == type)
            return (i->second);
    return (NULL);
}

std::vector<map_marker*> env_get_markers(const coord_def &c)
{
    std::pair<dgn_marker_map::const_iterator, dgn_marker_map::const_iterator>
        els = env.markers.equal_range(c);
    std::vector<map_marker*> rmarkers;
    for (dgn_marker_map::const_iterator i = els.first; i != els.second; ++i)
        rmarkers.push_back(i->second);
    return (rmarkers);
}

void env_clear_markers()
{
    for (dgn_marker_map::iterator i = env.markers.begin();
         i != env.markers.end(); ++i)
        delete i->second;
    env.markers.clear();
}
