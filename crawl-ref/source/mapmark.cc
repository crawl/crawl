/**
 * @file
 * @brief Level markers (annotations).
**/

#include "AppHdr.h"

#include <algorithm>

#include "mapmark.h"

#include "clua.h"
#include "cluautil.h"
#include "coordit.h"
#include "directn.h"
#include "dlua.h"
#include "libutil.h"
#include "l_libs.h"
#include "stuff.h"
#include "env.h"
#include "tags.h"
#include "terrain.h"
#include "unwind.h"

////////////////////////////////////////////////////////////////////////
// Dungeon markers

map_marker::marker_reader map_marker::readers[NUM_MAP_MARKER_TYPES] =
{
    &map_feature_marker::read,
    &map_lua_marker::read,
    &map_corruption_marker::read,
    &map_wiz_props_marker::read,
    &map_tomb_marker::read,
    &map_malign_gateway_marker::read,
    &map_phoenix_marker::read,
    &map_position_marker::read,
#if TAG_MAJOR_VERSION == 34
    &map_door_seal_marker::read,
#endif
    &map_terrain_change_marker::read,
    &map_cloud_spreader_marker::read
};

map_marker::marker_parser map_marker::parsers[NUM_MAP_MARKER_TYPES] =
{
    &map_feature_marker::parse,
    &map_lua_marker::parse,
    NULL,
    NULL,
    NULL
};

map_marker::map_marker(map_marker_type t, const coord_def &p)
    : pos(p), type(t)
{
}

map_marker::~map_marker()
{
}

void map_marker::activate(bool)
{
}

void map_marker::write(writer &outf) const
{
    marshallShort(outf, type);
    marshallCoord(outf, pos);
}

void map_marker::read(reader &inf)
{
    // Don't read type! The type has to be read by someone who knows how
    // to look up the unmarshall function.
    pos = unmarshallCoord(inf);
}

string map_marker::property(const string &pname) const
{
    return "";
}

map_marker *map_marker::read_marker(reader &inf)
{
    const map_marker_type mtype =
        static_cast<map_marker_type>(unmarshallShort(inf));
    return readers[mtype]? (*readers[mtype])(inf, mtype) : NULL;
}

map_marker *map_marker::parse_marker(const string &s,
                                     const string &ctx) throw (string)
{
    for (int i = 0; i < NUM_MAP_MARKER_TYPES; ++i)
    {
        if (parsers[i])
        {
            if (map_marker *m = parsers[i](s, ctx))
                return m;
        }
    }
    return NULL;
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

void map_feature_marker::write(writer &outf) const
{
    map_marker::write(outf);
    marshallShort(outf, feat);
}

void map_feature_marker::read(reader &inf)
{
    map_marker::read(inf);
    feat = static_cast<dungeon_feature_type>(unmarshallShort(inf));
}

map_marker *map_feature_marker::clone() const
{
    return new map_feature_marker(pos, feat);
}

map_marker *map_feature_marker::read(reader &inf, map_marker_type)
{
    map_marker *mapf = new map_feature_marker();
    mapf->read(inf);
    return mapf;
}

map_marker *map_feature_marker::parse(const string &s,
                                      const string &) throw (string)
{
    if (s.find("feat:") != 0)
        return NULL;
    string raw = s;
    strip_tag(raw, "feat:", true);

    const dungeon_feature_type ft = dungeon_feature_by_name(raw);
    if (ft == DNGN_UNSEEN)
    {
        throw make_stringf("Bad feature marker: %s (unknown feature '%s')",
                           s.c_str(), raw.c_str());
    }
    return new map_feature_marker(coord_def(0, 0), ft);
}

string map_feature_marker::debug_describe() const
{
    return make_stringf("feature (%s)", dungeon_feature_name(feat));
}

////////////////////////////////////////////////////////////////////////////
// map_lua_marker

map_lua_marker::map_lua_marker()
    : map_marker(MAT_LUA_MARKER, coord_def()), initialised(false)
{
}

map_lua_marker::map_lua_marker(const lua_datum &fn)
    : map_marker(MAT_LUA_MARKER, coord_def())
{
    lua_stack_cleaner clean(dlua);
    fn.push();
    if (fn.is_function() && !dlua.callfn("dgn_run_map", 1, 1))
        mprf(MSGCH_ERROR, "lua_marker exec error: %s", dlua.error.c_str());
    else
        check_register_table();
}

map_lua_marker::map_lua_marker(const string &s, const string &,
                               bool mapdef_marker)
    : map_marker(MAT_LUA_MARKER, coord_def()), initialised(false)
{
    lua_stack_cleaner clean(dlua);
    if (mapdef_marker)
    {
        if (dlua.loadstring(("return " + s).c_str(), "lua_marker"))
            mprf(MSGCH_ERROR, "lua_marker load error: %s", dlua.error.c_str());
        if (!dlua.callfn("dgn_run_map", 1, 1))
            mprf(MSGCH_ERROR, "lua_marker exec error: %s", dlua.error.c_str());
    }
    else
    {
        if (dlua.execstring(("return " + s).c_str(), "lua_marker_mapless", 1))
            mprf(MSGCH_ERROR, "lua_marker_mapless exec error: %s",
                 dlua.error.c_str());
    }
    check_register_table();
}

map_lua_marker::~map_lua_marker()
{
}

map_marker *map_lua_marker::clone() const
{
    map_lua_marker *copy = new map_lua_marker();
    copy->pos = pos;
    if (get_table())
        copy->check_register_table();
    return copy;
}

void map_lua_marker::check_register_table()
{
    if (!lua_istable(dlua, -1))
    {
        mpr("lua_marker: Expected table, didn't get it.", MSGCH_ERROR);
        initialised = false;
        return;
    }

    // Got a table. Save it in the registry.
    marker_table.reset(new lua_datum(dlua));
    initialised = true;
}

bool map_lua_marker::get_table() const
{
    if (marker_table.get())
    {
        marker_table->push();
        return lua_istable(dlua, -1);
    }
    else
        return false;
}

void map_lua_marker::write(writer &outf) const
{
    map_marker::write(outf);

    lua_stack_cleaner clean(dlua);
    bool init = initialised;
    if (!get_table())
    {
        mpr("Couldn't find table.", MSGCH_ERROR);
        init = false;
    }

    marshallByte(outf, init);
    if (!init)
        return;

    // Call dlua_marker_function(table, 'read')
    lua_pushstring(dlua, "read");
    if (!dlua.callfn("dlua_marker_reader_name", 2, 1))
        end(1, false, "lua_marker: write error: %s", dlua.error.c_str());

    // Right, what's on top should be a table name. Save it.
    if (!lua_isstring(dlua, -1))
    {
        end(1, false, "Expected marker class name (string) to save, got %s",
            lua_typename(dlua, lua_type(dlua, -1)));
    }

    const string marker_class(lua_tostring(dlua, -1));
    marshallString(outf, marker_class);

    // Okay, saved the marker's class. Now ask the writer to do its thing.

    // Call: dlua_marker_method(table, fname, marker)
    get_table();
    lua_pushstring(dlua, "write");
    lua_pushlightuserdata(dlua, const_cast<map_lua_marker*>(this));
    lua_pushlightuserdata(dlua, &outf);

    if (!dlua.callfn("dlua_marker_method", 4))
        end(1, false, "lua_marker::write error: %s", dlua.error.c_str());
}

void map_lua_marker::read(reader &inf)
{
    map_marker::read(inf);

    if (!(initialised = unmarshallByte(inf)))
        return;

    const string marker_class = unmarshallString(inf);
    lua_pushstring(dlua, marker_class.c_str());
    dlua_push_userdata(dlua, this, MAPMARK_METATABLE);
    lua_pushlightuserdata(dlua, &inf);
    if (!dlua.callfn("dlua_marker_read", 3, 1))
        end(1, false, "lua_marker::read error: %s", dlua.error.c_str());

    // Right, what's on top had better be a table.
    check_register_table();
}

map_marker *map_lua_marker::read(reader &inf, map_marker_type)
{
    map_marker *marker = new map_lua_marker;
    marker->read(inf);
    return marker;
}

void map_lua_marker::push_fn_args(const char *fn) const
{
    get_table();
    lua_pushstring(dlua, fn);
    dlua_push_userdata(dlua, this, MAPMARK_METATABLE);
}

bool map_lua_marker::callfn(const char *fn, bool warn_err, int args) const
{
    if (args == -1)
    {
        const int top = lua_gettop(dlua);
        push_fn_args(fn);
        args = lua_gettop(dlua) - top;
    }
    const bool res = dlua.callfn("dlua_marker_method", args, 1);
    if (!res && warn_err)
        mprf(MSGCH_ERROR, "mlua error: %s", dlua.error.c_str());
    return res;
}

void map_lua_marker::activate(bool verbose)
{
    lua_stack_cleaner clean(dlua);
    push_fn_args("activate");
    lua_pushboolean(dlua, verbose);
    callfn("activate", true, 4);
}

bool map_lua_marker::notify_dgn_event(const dgn_event &e)
{
    lua_stack_cleaner clean(dlua);
    push_fn_args("event");
    clua_push_dgn_event(dlua, &e);
    if (!dlua.callfn("dlua_marker_method", 4, 1))
    {
        mprf(MSGCH_ERROR, "notify_dgn_event: Lua error: %s",
             dlua.error.c_str());

        // Lua error prevents veto if the event is vetoable.
        return true;
    }

    bool accepted = true;

    // We accept only a real boolean false as a veto.
    if (lua_isboolean(dlua, -1))
        accepted = lua_toboolean(dlua, -1);

    return accepted;
}

string map_lua_marker::call_str_fn(const char *fn) const
{
    lua_stack_cleaner cln(dlua);
    if (!callfn(fn))
        return make_stringf("error (%s): %s", fn, dlua.error.c_str());

    string result;
    if (lua_isstring(dlua, -1))
        result = lua_tostring(dlua, -1);
    return result;
}

string map_lua_marker::debug_describe() const
{
    return call_str_fn("describe");
}

string map_lua_marker::property(const string &pname) const
{
    lua_stack_cleaner cln(dlua);
    push_fn_args("property");
    lua_pushstring(dlua, pname.c_str());
    if (!callfn("property", false, 4))
    {
        mprf(MSGCH_ERROR, "Lua marker property (%s) error: %s",
             pname.c_str(), dlua.error.c_str());
        return make_stringf("error (prop:%s): %s",
                            pname.c_str(), dlua.error.c_str());
    }
    string result;
    if (lua_isstring(dlua, -1))
        result = lua_tostring(dlua, -1);
    return result;
}

string map_lua_marker::debug_to_string() const
{
    lua_stack_cleaner cln(dlua);

    if (!get_table())
        return "Unable to get table for lua marker.";

    if (!dlua.callfn("table_to_string", 1, 1))
        return make_stringf("error (table_to_string): %s", dlua.error.c_str());

    string result;
    if (lua_isstring(dlua, -1))
        result = lua_tostring(dlua, -1);
    else
        result = "table_to_string() returned nothing";
    return result;
}

map_marker *map_lua_marker::parse(const string &s,
                                  const string &ctx) throw (string)
{
    string raw           = s;
    bool   mapdef_marker = true;

    if (s.find("lua:") == 0)
        strip_tag(raw, "lua:", true);
    else if (s.find("lua_mapless:") == 0)
    {
        strip_tag(raw, "lua_mapless:", true);
        mapdef_marker = false;
    }
    else
        return NULL;

    map_lua_marker *mark = new map_lua_marker(raw, ctx, mapdef_marker);
    if (!mark->initialised)
    {
        delete mark;
        throw make_stringf("Unable to initialise Lua marker from '%s'",
                           raw.c_str());
    }
    return mark;
}

//////////////////////////////////////////////////////////////////////////
// map_corruption_marker

map_corruption_marker::map_corruption_marker(const coord_def &p,
                                             int dur)
    : map_marker(MAT_CORRUPTION_NEXUS, p), duration(dur)
{
}

void map_corruption_marker::write(writer &out) const
{
    map_marker::write(out);
    marshallShort(out, duration);
}

void map_corruption_marker::read(reader &in)
{
    map_marker::read(in);
    duration = unmarshallShort(in);
}

map_marker *map_corruption_marker::read(reader &in, map_marker_type)
{
    map_corruption_marker *mc = new map_corruption_marker();
    mc->read(in);
    return mc;
}

map_marker *map_corruption_marker::clone() const
{
    map_corruption_marker *mark = new map_corruption_marker(pos, duration);
    return mark;
}

string map_corruption_marker::debug_describe() const
{
    return make_stringf("Lugonu corrupt (%d)", duration);
}

////////////////////////////////////////////////////////////////////////////
// map_feature_marker

map_wiz_props_marker::map_wiz_props_marker(
    const coord_def &p)
    : map_marker(MAT_WIZ_PROPS, p)
{
}

map_wiz_props_marker::map_wiz_props_marker(
    const map_wiz_props_marker &other)
    : map_marker(MAT_WIZ_PROPS, other.pos), properties(other.properties)
{
}

void map_wiz_props_marker::write(writer &outf) const
{
    map_marker::write(outf);
    marshallShort(outf, properties.size());
    for (map<string, string>::const_iterator i = properties.begin();
         i != properties.end(); ++i)
    {
        marshallString(outf, i->first);
        marshallString(outf, i->second);
    }
}

void map_wiz_props_marker::read(reader &inf)
{
    map_marker::read(inf);

    short numPairs = unmarshallShort(inf);
    for (short i = 0; i < numPairs; i++)
    {
        const string key = unmarshallString(inf);
        const string val = unmarshallString(inf);

        set_property(key, val);
    }
}

string map_wiz_props_marker::property(const string &pname) const
{
    if (pname == "desc")
        return property("feature_description");

    map<string, string>::const_iterator i = properties.find(pname);

    if (i != properties.end())
        return i->second;
    else
        return "";
}

string map_wiz_props_marker::set_property(const string &key, const string &val)
{
    string old_val = properties[key];
    properties[key] = val;
    return old_val;
}

map_marker *map_wiz_props_marker::clone() const
{
    return new map_wiz_props_marker(*this);
}

map_marker *map_wiz_props_marker::read(reader &inf, map_marker_type)
{
    map_marker *mapf = new map_wiz_props_marker();
    mapf->read(inf);
    return mapf;
}

map_marker *map_wiz_props_marker::parse(const string &s, const string &)
    throw (string)
{
    throw make_stringf("map_wiz_props_marker::parse() not implemented");
}

string map_wiz_props_marker::debug_describe() const
{
    return "Wizard props: " + property("feature_description");
}

//////////////////////////////////////////////////////////////////////////
// map_tomb_marker

map_tomb_marker::map_tomb_marker(const coord_def &p,
                                 int dur, int src, int targ)
    : map_marker(MAT_TOMB, p), duration(dur), source(src), target(targ)
{
}

void map_tomb_marker::write(writer &out) const
{
    map_marker::write(out);
    marshallShort(out, duration);
    marshallShort(out, source);
    marshallShort(out, target);
}

void map_tomb_marker::read(reader &in)
{
    map_marker::read(in);
    duration = unmarshallShort(in);
    source   = unmarshallShort(in);
    target   = unmarshallShort(in);
}

map_marker *map_tomb_marker::read(reader &in, map_marker_type)
{
    map_tomb_marker *mc = new map_tomb_marker();
    mc->read(in);
    return mc;
}

map_marker *map_tomb_marker::clone() const
{
    map_tomb_marker *mark = new map_tomb_marker(pos, duration,
                                                source, target);
    return mark;
}

string map_tomb_marker::debug_describe() const
{
    return make_stringf("Tomb (%d, %d, %d)", duration,
                                             source, target);
}

//////////////////////////////////////////////////////////////////////////
// map_malign_gateway_marker

map_malign_gateway_marker::map_malign_gateway_marker(const coord_def &p,
                                 int dur, bool ip, string sum, beh_type b,
                                 god_type gd, int pow)
    : map_marker(MAT_MALIGN, p), duration(dur), is_player(ip), monster_summoned(false),
      summoner_string(sum), behaviour(b), god(gd), power(pow)
{
}

void map_malign_gateway_marker::write(writer &out) const
{
    map_marker::write(out);
    marshallShort(out, duration);
    marshallBoolean(out, is_player);
    marshallBoolean(out, monster_summoned);
    marshallString(out, summoner_string);
    marshallUByte(out, behaviour);
    marshallUByte(out, god);
    marshallShort(out, power);
}

void map_malign_gateway_marker::read(reader &in)
{
    map_marker::read(in);
    duration  = unmarshallShort(in);
    is_player = unmarshallBoolean(in);

    monster_summoned = unmarshallBoolean(in);
    summoner_string = unmarshallString(in);
    behaviour = static_cast<beh_type>(unmarshallUByte(in));

    god       = static_cast<god_type>(unmarshallByte(in));
    power     = unmarshallShort(in);
}

map_marker *map_malign_gateway_marker::read(reader &in, map_marker_type)
{
    map_malign_gateway_marker *mc = new map_malign_gateway_marker();
    mc->read(in);
    return mc;
}

map_marker *map_malign_gateway_marker::clone() const
{
    map_malign_gateway_marker *mark = new map_malign_gateway_marker(pos,
        duration, is_player, summoner_string, behaviour, god, power);
    return mark;
}

string map_malign_gateway_marker::debug_describe() const
{
    return make_stringf("Malign gateway (%d, %s)", duration,
                        is_player ? "player" : "monster");
}

//////////////////////////////////////////////////////////////////////////
// map_phoenix_marker

map_phoenix_marker::map_phoenix_marker(const coord_def& p,
                    int dur, int mnum, beh_type bh, mon_attitude_type at,
                    god_type gd, coord_def cp)
    : map_marker(MAT_PHOENIX, p), duration(dur), mon_num(mnum),
            behaviour(bh), attitude(at), god(gd), corpse_pos(cp)
{
}

void map_phoenix_marker::write(writer &out) const
{
    map_marker::write(out);
    marshallShort(out, duration);
    marshallShort(out, mon_num);
    marshallUByte(out, behaviour);
    marshallUByte(out, attitude);
    marshallUByte(out, god);
    marshallCoord(out, corpse_pos);
}

void map_phoenix_marker::read(reader &in)
{
    map_marker::read(in);

    duration = unmarshallShort(in);
    mon_num = unmarshallShort(in);
    behaviour = static_cast<beh_type>(unmarshallUByte(in));
    attitude = static_cast<mon_attitude_type>(unmarshallUByte(in));
    god       = static_cast<god_type>(unmarshallByte(in));
    corpse_pos = unmarshallCoord(in);
}

map_marker *map_phoenix_marker::read(reader &in, map_marker_type)
{
    map_phoenix_marker *mc = new map_phoenix_marker();
    mc->read(in);
    return mc;
}

map_marker *map_phoenix_marker::clone() const
{
    map_phoenix_marker *mark = new map_phoenix_marker(pos, duration, mon_num,
                                    behaviour, attitude, god, corpse_pos);
    return mark;
}

string map_phoenix_marker::debug_describe() const
{
    return make_stringf("Phoenix marker (%d, %d)", duration, mon_num);
}

#if TAG_MAJOR_VERSION == 34
////////////////////////////////////////////////////////////////////////////
// map_door_seal_marker

map_door_seal_marker::map_door_seal_marker(const coord_def& p,
                    int dur, int mnum, dungeon_feature_type oldfeat)
    : map_marker(MAT_DOOR_SEAL, p), duration(dur), mon_num(mnum),
        old_feature(oldfeat)
{
}

void map_door_seal_marker::write(writer &out) const
{
    map_marker::write(out);
    marshallShort(out, duration);
    marshallShort(out, mon_num);
    marshallUByte(out, old_feature);
}

void map_door_seal_marker::read(reader &in)
{
    map_marker::read(in);

    duration = unmarshallShort(in);
    mon_num = unmarshallShort(in);
    old_feature = static_cast<dungeon_feature_type>(unmarshallUByte(in));
}

map_marker *map_door_seal_marker::read(reader &in, map_marker_type)
{
    map_door_seal_marker *mc = new map_door_seal_marker();
    mc->read(in);
    return mc;
}

map_marker *map_door_seal_marker::clone() const
{
    map_door_seal_marker *mark = new map_door_seal_marker(pos, duration, mon_num, old_feature);
    return mark;
}

string map_door_seal_marker::debug_describe() const
{
    return make_stringf("Door seal marker (%d, %d)", duration, mon_num);
}
#endif

////////////////////////////////////////////////////////////////////////////
// map_terrain_change_marker

map_terrain_change_marker::map_terrain_change_marker (const coord_def& p,
                    dungeon_feature_type oldfeat, dungeon_feature_type newfeat,
                    int dur, terrain_change_type ctype, int mnum)
    : map_marker(MAT_TERRAIN_CHANGE, p), duration(dur), mon_num(mnum),
        old_feature(oldfeat), new_feature(newfeat), change_type(ctype)
{
}

void map_terrain_change_marker::write(writer &out) const
{
    map_marker::write(out);
    marshallShort(out, duration);
    marshallUByte(out, old_feature);
    marshallUByte(out, new_feature);
    marshallUByte(out, change_type);
    marshallShort(out, mon_num);
}

void map_terrain_change_marker::read(reader &in)
{
    map_marker::read(in);

    duration = unmarshallShort(in);
    old_feature = static_cast<dungeon_feature_type>(unmarshallUByte(in));
    new_feature = static_cast<dungeon_feature_type>(unmarshallUByte(in));
    change_type = static_cast<terrain_change_type>(unmarshallUByte(in));
    mon_num = unmarshallShort(in);
}

map_marker *map_terrain_change_marker::read(reader &in, map_marker_type)
{
    map_terrain_change_marker *mc = new map_terrain_change_marker();
    mc->read(in);
    return mc;
}

map_marker *map_terrain_change_marker::clone() const
{
    map_terrain_change_marker *mark =
        new map_terrain_change_marker(pos, old_feature, new_feature, duration,
                                      change_type, mon_num);
    return mark;
}

string map_terrain_change_marker::debug_describe() const
{
    return make_stringf("Terrain change marker (%d->%d, %d)",
                        old_feature, new_feature, duration);
}

////////////////////////////////////////////////////////////////////////////
// map_cloud_spreader_marker

map_cloud_spreader_marker::map_cloud_spreader_marker(const coord_def &p,
                              cloud_type cloud, int spd, int amt,
                              int max_radius, int dur, actor* agent)
: map_marker(MAT_CLOUD_SPREADER, p), ctype(cloud), speed(spd),
  remaining(amt), max_rad(max_radius), duration(dur), speed_increment(0)
{
    if (agent)
    {
        agent_mid = agent->mid;
        if (agent->is_monster())
            kcat = (agent->as_monster()->friendly() ? KC_FRIENDLY : KC_OTHER);
        else
            kcat = KC_YOU;
    }
    else
    {
        agent_mid = NON_ENTITY;
        kcat = KC_OTHER;
    }
}

void map_cloud_spreader_marker::write(writer &out) const
{
    map_marker::write(out);
    marshallByte(out, ctype);
    marshallShort(out, speed);
    marshallShort(out, duration);
    marshallByte(out, max_rad);
    marshallShort(out, remaining);
    marshallShort(out, speed_increment);
    marshallInt(out, agent_mid);
    marshallByte(out, kcat);
}

void map_cloud_spreader_marker::read(reader &in)
{
    map_marker::read(in);

    ctype = static_cast<cloud_type>(unmarshallByte(in));
    speed = unmarshallShort(in);
    duration = unmarshallShort(in);
    max_rad = unmarshallByte(in);
    remaining = unmarshallShort(in);
    speed_increment = unmarshallShort(in);
    agent_mid = static_cast<mid_t>(unmarshallInt(in));
    kcat = static_cast<kill_category>(unmarshallByte(in));
}

map_marker *map_cloud_spreader_marker::read(reader &in, map_marker_type)
{
    map_cloud_spreader_marker *mc = new map_cloud_spreader_marker();
    mc->read(in);
    return mc;
}

map_marker *map_cloud_spreader_marker::clone() const
{
    map_cloud_spreader_marker *mark =
            new map_cloud_spreader_marker(pos, ctype, speed, remaining, max_rad,
                                          duration);
    mark->agent_mid = agent_mid;
    mark->kcat = kcat;
    return mark;
}

string map_cloud_spreader_marker::debug_describe() const
{
    return make_stringf("Cloud spreader marker (%d)", ctype);
}

////////////////////////////////////////////////////////////////////////////
// map_position_marker

map_position_marker::map_position_marker(
    const coord_def &p,
    const coord_def _dest)
    : map_marker(MAT_POSITION, p), dest(_dest)
{
}

map_position_marker::map_position_marker(
    const map_position_marker &other)
    : map_marker(MAT_POSITION, other.pos), dest(other.dest)
{
}

void map_position_marker::write(writer &outf) const
{
    map_marker::write(outf);
    marshallCoord(outf, dest);
}

void map_position_marker::read(reader &inf)
{
    map_marker::read(inf);
    dest = (unmarshallCoord(inf));
}

map_marker *map_position_marker::clone() const
{
    return new map_position_marker(pos, dest);
}

map_marker *map_position_marker::read(reader &inf, map_marker_type)
{
    map_marker *mapf = new map_position_marker();
    mapf->read(inf);
    return mapf;
}

string map_position_marker::debug_describe() const
{
    return make_stringf("position (%d,%d)", dest.x, dest.y);
}

//////////////////////////////////////////////////////////////////////////
// Map markers in env.

map_markers::map_markers() : markers(), have_inactive_markers(false)
{
}

map_markers::map_markers(const map_markers &c)
  : markers(), have_inactive_markers(false)
{
    init_from(c);
}

map_markers &map_markers::operator = (const map_markers &c)
{
    if (this != &c)
    {
        clear();
        init_from(c);
    }
    return *this;
}

map_markers::~map_markers()
{
    clear();
}

void map_markers::init_from(const map_markers &c)
{
    for (dgn_marker_map::const_iterator i = c.markers.begin();
         i != c.markers.end(); ++i)
    {
        add(i->second->clone());
    }
    have_inactive_markers = c.have_inactive_markers;
}

void map_markers::clear_need_activate()
{
    have_inactive_markers = false;
}

void map_markers::activate_all(bool verbose)
{
    for (dgn_marker_map::iterator i = markers.begin();
         i != markers.end();)
    {
        map_marker *marker = i->second;
        ++i;
        marker->activate(verbose);
    }

    for (dgn_marker_map::iterator i = markers.begin();
         i != markers.end();)
    {
        map_marker *marker = i->second;
        ++i;
        if (!marker->property("post_activate_remove").empty())
            remove(marker);
    }

    have_inactive_markers = false;
}

void map_markers::activate_markers_at(coord_def p)
{
    const vector<map_marker *> activatees = get_markers_at(p);
    for (int i = 0, size = activatees.size(); i < size; ++i)
        activatees[i]->activate();

    const vector<map_marker *> active_markers = get_markers_at(p);
    for (int i = 0, size = active_markers.size(); i < size; ++i)
    {
        const string prop = active_markers[i]->property("post_activate_remove");
        if (!prop.empty())
            remove(active_markers[i]);
    }
}

void map_markers::add(map_marker *marker)
{
    markers.insert(dgn_pos_marker(marker->pos, marker));
    have_inactive_markers = true;
}

void map_markers::unlink_marker(const map_marker *marker)
{
    pair<dgn_marker_map::iterator, dgn_marker_map::iterator>
        els = markers.equal_range(marker->pos);
    for (dgn_marker_map::iterator i = els.first; i != els.second; ++i)
    {
        if (i->second == marker)
        {
            markers.erase(i);
            break;
        }
    }
}

void map_markers::check_empty()
{
    if (markers.empty())
        have_inactive_markers = false;
}

void map_markers::remove(map_marker *marker)
{
    unlink_marker(marker);
    delete marker;
    check_empty();
}

void map_markers::remove_markers_at(const coord_def &c,
                                    map_marker_type type)
{
    pair<dgn_marker_map::iterator, dgn_marker_map::iterator>
        els = markers.equal_range(c);
    for (dgn_marker_map::iterator i = els.first; i != els.second;)
    {
        dgn_marker_map::iterator todel = i++;
        if (type == MAT_ANY || todel->second->get_type() == type)
        {
            delete todel->second;
            markers.erase(todel);
        }
    }
    check_empty();
}

map_marker *map_markers::find(const coord_def &c, map_marker_type type)
{
    pair<dgn_marker_map::const_iterator, dgn_marker_map::const_iterator>
        els = markers.equal_range(c);
    for (dgn_marker_map::const_iterator i = els.first; i != els.second; ++i)
        if (type == MAT_ANY || i->second->get_type() == type)
            return i->second;
    return NULL;
}

map_marker *map_markers::find(map_marker_type type)
{
    for (dgn_marker_map::const_iterator i = markers.begin();
         i != markers.end(); ++i)
    {
        if (type == MAT_ANY || i->second->get_type() == type)
            return i->second;
    }
    return NULL;
}

void map_markers::move(const coord_def &from, const coord_def &to)
{
    unwind_bool inactive(have_inactive_markers);
    pair<dgn_marker_map::iterator, dgn_marker_map::iterator>
        els = markers.equal_range(from);

    list<map_marker*> tmarkers;
    for (dgn_marker_map::iterator i = els.first; i != els.second;)
    {
        dgn_marker_map::iterator curr = i++;
        tmarkers.push_back(curr->second);
        markers.erase(curr);
    }

    for (list<map_marker*>::iterator i = tmarkers.begin();
         i != tmarkers.end(); ++i)
    {
        (*i)->pos = to;
        add(*i);
    }
}

void map_markers::move_marker(map_marker *marker, const coord_def &to)
{
    unwind_bool inactive(have_inactive_markers);
    unlink_marker(marker);
    marker->pos = to;
    add(marker);
}

vector<map_marker*> map_markers::get_all(map_marker_type mat)
{
    vector<map_marker*> rmarkers;
    for (dgn_marker_map::const_iterator i = markers.begin();
         i != markers.end(); ++i)
    {
        if (mat == MAT_ANY || i->second->get_type() == mat)
            rmarkers.push_back(i->second);
    }
    return rmarkers;
}

vector<map_marker*> map_markers::get_all(const string &key, const string &val)
{
    vector<map_marker*> rmarkers;

    for (dgn_marker_map::const_iterator i = markers.begin();
         i != markers.end(); ++i)
    {
        map_marker*  marker = i->second;
        const string prop   = marker->property(key);

        if (val.empty() && !prop.empty() || !val.empty() && val == prop)
            rmarkers.push_back(marker);
    }

    return rmarkers;
}

vector<map_marker*> map_markers::get_markers_at(const coord_def &c)
{
    pair<dgn_marker_map::const_iterator, dgn_marker_map::const_iterator>
        els = markers.equal_range(c);
    vector<map_marker*> rmarkers;
    for (dgn_marker_map::const_iterator i = els.first; i != els.second; ++i)
        rmarkers.push_back(i->second);
    return rmarkers;
}

string map_markers::property_at(const coord_def &c, map_marker_type type,
                                const string &key)
{
    pair<dgn_marker_map::const_iterator, dgn_marker_map::const_iterator>
        els = markers.equal_range(c);
    for (dgn_marker_map::const_iterator i = els.first; i != els.second; ++i)
    {
        const string prop = i->second->property(key);
        if (!prop.empty())
            return prop;
    }
    return "";
}

void map_markers::clear()
{
    for (dgn_marker_map::iterator i = markers.begin(); i != markers.end(); ++i)
        delete i->second;
    markers.clear();
    check_empty();
}

static const long MARKERS_COOKY = 0x17742C32;
void map_markers::write(writer &outf) const
{
    marshallInt(outf, MARKERS_COOKY);

    vector<unsigned char> buf;

    marshallShort(outf, markers.size());
    for (dgn_marker_map::const_iterator i = markers.begin();
         i != markers.end(); ++i)
    {
        buf.clear();
        writer tmp_outf(&buf);
        i->second->write(tmp_outf);

        // Write the marker data, prefixed by a size
        marshallInt(outf, buf.size());
        for (vector<unsigned char>::const_iterator bi = buf.begin();
              bi != buf.end(); ++bi)
        {
            outf.writeByte(*bi);
        }
    }
}

void map_markers::read(reader &inf)
{
    clear();

    const int cooky = unmarshallInt(inf);
    ASSERT(cooky == MARKERS_COOKY);
    UNUSED(cooky);

    const int nmarkers = unmarshallShort(inf);
    for (int i = 0; i < nmarkers; ++i)
    {
        // used by tools
        unmarshallInt(inf);
        if (map_marker *mark = map_marker::read_marker(inf))
            add(mark);
    }
}

/////////////////////////////////////////////////////////////////////////

bool marker_vetoes_operation(const char *op)
{
    return env.markers.property_at(you.pos(), MAT_ANY, op) == "veto";
}

coord_def find_marker_position_by_prop(const string &prop,
                                       const string &expected)
{
    const vector<coord_def> markers =
        find_marker_positions_by_prop(prop, expected, 1);
    if (markers.empty())
    {
        const coord_def nowhere(-1, -1);
        return nowhere;
    }
    return markers[0];
}

vector<coord_def> find_marker_positions_by_prop(const string &prop,
                                                const string &expected,
                                                unsigned maxresults)
{
    vector<coord_def> marker_positions;
    for (rectangle_iterator i(0, 0); i; ++i)
    {
        const string value = env.markers.property_at(*i, MAT_ANY, prop);
        if (!value.empty() && (expected.empty() || value == expected))
        {
            marker_positions.push_back(*i);
            if (maxresults && marker_positions.size() >= maxresults)
                return marker_positions;
        }
    }
    return marker_positions;
}

vector<map_marker*> find_markers_by_prop(const string &prop,
                                         const string &expected,
                                         unsigned maxresults)
{
    vector<map_marker*> markers;
    for (rectangle_iterator pos(0, 0); pos; ++pos)
    {
        const vector<map_marker*> markers_here =
            env.markers.get_markers_at(*pos);
        for (unsigned i = 0, size = markers_here.size(); i < size; ++i)
        {
            const string value(markers_here[i]->property(prop));
            if (!value.empty() && (expected.empty() || value == expected))
            {
                markers.push_back(markers_here[i]);
                if (maxresults && markers.size() >= maxresults)
                    return markers;
            }
        }
    }
    return markers;
}

///////////////////////////////////////////////////////////////////

// Safely remove all markers and dungeon listeners at the given square.
void remove_markers_and_listeners_at(coord_def p)
{
    // Look for Lua markers on this square that are listening for
    // non-positional events, (such as bazaar portals listening for
    // turncount changes) and detach them manually from the dungeon
    // event dispatcher.
    const vector<map_marker *> markers = env.markers.get_markers_at(p);
    for (int i = 0, size = markers.size(); i < size; ++i)
    {
        if (markers[i]->get_type() == MAT_LUA_MARKER)
            dungeon_events.remove_listener(
                dynamic_cast<map_lua_marker*>(markers[i]));
    }

    env.markers.remove_markers_at(p);
    dungeon_events.clear_listeners_at(p);
}
