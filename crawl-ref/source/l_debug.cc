/*
 * File:       l_debug.cc
 * Summary:    Various debugging bindings.
 */

#include "AppHdr.h"

#include "cluautil.h"
#include "l_libs.h"

#include "beam.h"
#include "branch.h"
#include "chardump.h"
#include "coordit.h"
#include "dungeon.h"
#include "env.h"
#include "initfile.h"
#include "godwrath.h"
#include "libutil.h"
#include "message.h"
#include "mon-iter.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "place.h"
#include "view.h"
#include "wiz-dgn.h"

// WARNING: This is a very low-level call.
//
// Usage: goto_place("placename", <bind_entrance>)
// "placename" is the name of the place as used in maps, such as "Lair:2",
// "Vault:$", etc.
//
// If <bind_entrance> is specified, the entrance point of
// the branch specified in place_name is bound to the given level in the
// parent branch (the entrance level should be 1-based). This can be helpful
// when testing scenarios that depend on the absolute depth of the current
// place.
LUAFN(debug_goto_place)
{
    try
    {
        const level_id id = level_id::parse_level_id(luaL_checkstring(ls, 1));
        const int bind_entrance =
            lua_isnumber(ls, 2)? luaL_checkint(ls, 2) : -1;
        you.level_type = id.level_type;
        if (id.level_type == LEVEL_DUNGEON)
        {
            you.where_are_you = static_cast<branch_type>(id.branch);
            you.your_level = absdungeon_depth(id.branch, id.depth);

            if (bind_entrance != -1)
                branches[you.where_are_you].startdepth = bind_entrance;
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
    env.map_knowledge.init(map_cell());
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

LUAFN(debug_never_die)
{
#if WIZARD || DEBUG
    if (lua_isnone(ls, 1))
    {
        luaL_argerror(ls, 1, "needs a boolean argument");
        return (0);
    }
    you.never_die = lua_toboolean(ls, 1);
#else
    luaL_error(ls, "only works if DEBUG or WIZARD is defined");
#endif

    return (0);
}

// If menv[] is full, dismiss all monsters not near the player.
LUAFN(debug_cull_monsters)
{
    for (int il = 0; il < MAX_MONSTERS; il++)
    {
        if (menv[il].type == MONS_NO_MONSTER)
            // At least one empty space in menv
            return (0);
    }

    mpr("menv[] is full, dismissing non-near monsters",
        MSGCH_DIAGNOSTICS);

    // menv[] is full
    for (monster_iterator mi; mi; ++mi)
    {
        if (mons_near(*mi))
            continue;

        mi->flags |= MF_HARD_RESET;
        monster_die(*mi, KILL_DISMISSED, NON_MONSTER);
    }

    return (0);
}

LUAFN(debug_dismiss_adjacent)
{
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        monsters* mon = monster_at(*ai);

        if (mon)
        {
            mon->flags |= MF_HARD_RESET;
            monster_die(mon, KILL_DISMISSED, NON_MONSTER);
        }
    }

    return (0);
}

LUAFN(debug_god_wrath)
{
    const char *god_name = luaL_checkstring(ls, 1);
    if (!god_name)
    {
        std::string err = "god_wrath requires a god!";
        return (luaL_argerror(ls, 1, err.c_str()));
    }

    god_type god = str_to_god(god_name);
    if (god == GOD_NO_GOD)
    {
        std::string err = make_stringf("'%s' matches no god.", god_name);
        return (luaL_argerror(ls, 1, err.c_str()));
    }

    bool no_bonus = lua_toboolean(ls, 2);

    divine_retribution(god, no_bonus);
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
{ "never_die", debug_never_die },
{ "cull_monsters", debug_cull_monsters},
{ "dismiss_adjacent", debug_dismiss_adjacent},
{ "god_wrath", debug_god_wrath},

{ NULL, NULL }
};
