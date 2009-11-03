/*
 * File:       l_debug.cc
 * Summary:    Various debugging bindings.
 */

#include "AppHdr.h"

#include "cluautil.h"
#include "l_libs.h"

#include "beam.h"
#include "chardump.h"
#include "dungeon.h"
#include "message.h"
#include "place.h"

// WARNING: This is a very low-level call.
LUAFN(debug_goto_place)
{
    try
    {
        const level_id id = level_id::parse_level_id(luaL_checkstring(ls, 1));
        you.level_type = id.level_type;
        if (id.level_type == LEVEL_DUNGEON)
        {
            you.where_are_you = static_cast<branch_type>(id.branch);
            you.your_level = absdungeon_depth(id.branch, id.depth);
        }
    }
    catch (const std::string &err)
    {
        luaL_error(ls, err.c_str());
    }
    return (0);
}

LUAFN(debug_flush_map_memory)
{
    dgn_flush_map_memory();
    init_level_connectivity();
    return (0);
}

LUAFN(debug_generate_level)
{
    no_messages mx;
    env.show.init(0);
    env.map.init(map_cell());
#ifdef USE_TILE
    tile_init_default_flavour();
    tile_clear_flavour();
    TileNewLevel(true);
#endif
    builder(you.your_level, you.level_type);
    return (0);
}

LUAFN(debug_dump_map)
{
    const int pos = lua_isuserdata(ls, 1) ? 2 : 1;
    if (lua_isstring(ls, pos))
        dump_map(lua_tostring(ls, pos), true);
    return (0);
}

LUAFN(_debug_test_explore)
{
#ifdef WIZARD
    debug_test_explore();
#endif
    return (0);
}

LUAFN(debug_bouncy_beam)
{
    coord_def source;
    coord_def target;

    source.x = luaL_checkint(ls, 1);
    source.y = luaL_checkint(ls, 2);
    target.x = luaL_checkint(ls, 3);
    target.y = luaL_checkint(ls, 4);
    int range = luaL_checkint(ls, 5);
    bool findray = false;
    if (lua_gettop(ls) > 5)
        findray = lua_toboolean(ls, 6);

    bolt beam;

    beam.range      = range;
    beam.type       = '*';
    beam.colour     = LIGHTCYAN;
    beam.flavour    = BEAM_ELECTRICITY;
    beam.source     = source;
    beam.target     = target;
    beam.is_beam    = true;
    beam.draw_delay = 0;

    if (findray)
        beam.chose_ray = find_ray(source, target, beam.ray);

    beam.name       = "debug lightning beam";
    beam.short_name = "DEBUG";

    beam.fire();

    return (0);
}

const struct luaL_reg debug_dlib[] =
{
{ "goto_place", debug_goto_place },
{ "flush_map_memory", debug_flush_map_memory },
{ "generate_level", debug_generate_level },
{ "dump_map", debug_dump_map },
{ "test_explore", _debug_test_explore },
{ "bouncy_beam", debug_bouncy_beam },

{ NULL, NULL }
};
