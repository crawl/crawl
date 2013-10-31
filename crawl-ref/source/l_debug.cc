/**
 * @file
 * @brief Various debugging bindings.
**/

#include "AppHdr.h"

#include "cluautil.h"
#include "l_libs.h"

#include "act-iter.h"
#include "beam.h"
#include "branch.h"
#include "chardump.h"
#include "coordit.h"
#include "dungeon.h"
#include "env.h"
#include "files.h"
#include "godwrath.h"
#include "libutil.h"
#include "los.h"
#include "message.h"
#include "mon-act.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "place.h"
#include "religion.h"
#include "stairs.h"
#include "state.h"
#include "tileview.h"
#include "view.h"
#include "wiz-dgn.h"

// WARNING: This is a very low-level call.
//
// Usage: goto_place("placename", <bind_entrance>)
// "placename" is the name of the place as used in maps, such as "Lair:2",
// "Vaults:$", etc.
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
        you.goto_place(id);
        if (bind_entrance != -1)
            startdepth[you.where_are_you] = bind_entrance;
    }
    catch (const string &err)
    {
        luaL_error(ls, err.c_str());
    }
    return 0;
}

LUAFN(debug_enter_dungeon)
{
    init_level_connectivity();

    you.where_are_you = BRANCH_MAIN_DUNGEON;
    you.depth = 1;

    load_level(DNGN_STONE_STAIRS_DOWN_I, LOAD_START_GAME, level_id());
    return 0;
}

LUAWRAP(debug_down_stairs, down_stairs(DNGN_STONE_STAIRS_DOWN_I))
LUAWRAP(debug_up_stairs, up_stairs(DNGN_STONE_STAIRS_UP_I))

LUAFN(debug_flush_map_memory)
{
    dgn_flush_map_memory();
    init_level_connectivity();
    you.unique_creatures.reset();
    return 0;
}

LUAFN(debug_generate_level)
{
    no_messages mx;
    env.map_knowledge.init(map_cell());
    los_changed();
    tile_init_default_flavour();
    tile_clear_flavour();
    tile_new_level(true);
    builder(lua_isboolean(ls, 1)? lua_toboolean(ls, 1) : true);
    return 0;
}

LUAFN(debug_reveal_mimics)
{
    for (rectangle_iterator ri(1); ri; ++ri)
        if (mimic_at(*ri))
            discover_mimic(*ri, false);
    return 0;
}

LUAFN(debug_los_changed)
{
    los_changed();
    return 0;
}

LUAFN(debug_dump_map)
{
    const int pos = lua_isuserdata(ls, 1) ? 2 : 1;
    if (lua_isstring(ls, pos))
        dump_map(lua_tostring(ls, pos), true);
    return 0;
}

LUAFN(_debug_test_explore)
{
#ifdef WIZARD
    debug_test_explore();
#endif
    return 0;
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
    beam.glyph      = '*';
    beam.colour     = LIGHTCYAN;
    beam.flavour    = BEAM_ELECTRICITY;
    beam.source     = source;
    beam.target     = target;
    beam.is_beam    = true;
    beam.draw_delay = 0;

    if (findray)
        beam.chose_ray = find_ray(source, target, beam.ray, opc_solid_see);

    beam.name       = "debug lightning beam";
    beam.short_name = "DEBUG";

    beam.fire();

    return 0;
}

// If menv[] is full, dismiss all monsters not near the player.
LUAFN(debug_cull_monsters)
{
    for (int il = 0; il < MAX_MONSTERS; il++)
    {
        if (menv[il].type == MONS_NO_MONSTER)
            // At least one empty space in menv
            return 0;
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

    return 0;
}

LUAFN(debug_dismiss_adjacent)
{
    for (adjacent_iterator ai(you.pos()); ai; ++ai)
    {
        monster* mon = monster_at(*ai);

        if (mon)
        {
            mon->flags |= MF_HARD_RESET;
            monster_die(mon, KILL_DISMISSED, NON_MONSTER);
        }
    }

    return 0;
}

LUAFN(debug_dismiss_monsters)
{
    for (monster_iterator mi; mi; ++mi)
    {
        if (mi)
        {
            mi->flags |= MF_HARD_RESET;
            monster_die(*mi, KILL_DISMISSED, NON_MONSTER);
        }
    }

    return 0;
}

LUAFN(debug_god_wrath)
{
    const char *god_name = luaL_checkstring(ls, 1);
    if (!god_name)
    {
        string err = "god_wrath requires a god!";
        return luaL_argerror(ls, 1, err.c_str());
    }

    god_type god = strcmp(god_name, "random") ? str_to_god(god_name) : GOD_RANDOM;
    if (god == GOD_NO_GOD)
    {
        string err = make_stringf("'%s' matches no god.", god_name);
        return luaL_argerror(ls, 1, err.c_str());
    }

    bool no_bonus = lua_toboolean(ls, 2);

    divine_retribution(god, no_bonus);
    return 0;
}

LUAFN(debug_handle_monster_move)
{
    MonsterWrap *mw = clua_get_userdata< MonsterWrap >(ls, MONS_METATABLE);
    if (!mw || !mw->mons)
        return 0;

    handle_monster_move(mw->mons);
    return 0;
}

static FixedBitVector<NUM_MONSTERS> saved_uniques;

LUAFN(debug_save_uniques)
{
    saved_uniques = you.unique_creatures;
    return 0;
}

LUAFN(debug_reset_uniques)
{
    you.unique_creatures.reset();
    return 0;
}

LUAFN(debug_randomize_uniques)
{
    you.unique_creatures.reset();
    for (monster_type mt = MONS_0; mt < NUM_MONSTERS; ++mt)
    {
        if (!mons_is_unique(mt))
            continue;
        you.unique_creatures.set(mt, coinflip());
    }
    return 0;
}

// Compare list of uniques on current level with
// you.unique_creatures.
static bool _check_uniques()
{
    bool ret = true;

    FixedBitVector<NUM_MONSTERS> uniques_on_level;
    for (monster_iterator mi; mi; ++mi)
        if (mons_is_unique(mi->type))
            uniques_on_level.set(mi->type);

    for (monster_type mt = MONS_0; mt < NUM_MONSTERS; ++mt)
    {
        if (!mons_is_unique(mt) || mons_species(mt) == MONS_SERPENT_OF_HELL)
            continue;
        bool was_set = saved_uniques[mt];
        bool is_set = you.unique_creatures[mt];
        bool placed = uniques_on_level[mt];
        if (placed && was_set
            || placed && !is_set
            || was_set && !is_set
            || !was_set && is_set && !placed)
        {
            mprf(MSGCH_ERROR,
                 "Bad unique tracking: %s placed=%d was_set=%d is_set=%d",
                 mons_type_name(mt, DESC_PLAIN).c_str(),
                 placed, was_set, is_set);
            ret = false;
        }
    }
    return ret;
}

LUAFN(debug_check_uniques)
{
    lua_pushboolean(ls, _check_uniques());
    return 1;
}

LUAFN(debug_viewwindow)
{
    viewwindow(lua_toboolean(ls, 1));
    return 0;
}

LUAFN(debug_seen_monsters_react)
{
    seen_monsters_react();
    return 0;
}

static const char* disablements[] =
{
    "spawns",
    "mon_act",
    "mon_regen",
    "player_regen",
    "hunger",
    "death",
    "delay",
    "confirmations",
    "afflictions",
    "mon_sight",
    "save_checkpoints",
};

LUAFN(debug_disable)
{
    COMPILE_CHECK(ARRAYSZ(disablements) == NUM_DISABLEMENTS);

    const char* what = luaL_checkstring(ls, 1);
    for (int dis = 0; dis < NUM_DISABLEMENTS; dis++)
        if (what && !strcmp(what, disablements[dis]))
        {
            bool onoff = true;
            if (lua_isboolean(ls, 2))
                onoff = lua_toboolean(ls, 2);
            crawl_state.disables.set(dis, onoff);
            return 0;
        }
    luaL_argerror(ls, 1,
                  make_stringf("unknown thing to disable: %s", what).c_str());

    return 0;
}

const struct luaL_reg debug_dlib[] =
{
{ "goto_place", debug_goto_place },
{ "enter_dungeon", debug_enter_dungeon },
{ "down_stairs", debug_down_stairs },
{ "up_stairs", debug_up_stairs },
{ "flush_map_memory", debug_flush_map_memory },
{ "generate_level", debug_generate_level },
{ "reveal_mimics", debug_reveal_mimics },
{ "los_changed", debug_los_changed },
{ "dump_map", debug_dump_map },
{ "test_explore", _debug_test_explore },
{ "bouncy_beam", debug_bouncy_beam },
{ "cull_monsters", debug_cull_monsters},
{ "dismiss_adjacent", debug_dismiss_adjacent},
{ "dismiss_monsters", debug_dismiss_monsters},
{ "god_wrath", debug_god_wrath},
{ "handle_monster_move", debug_handle_monster_move },
{ "save_uniques", debug_save_uniques },
{ "randomize_uniques", debug_randomize_uniques },
{ "reset_uniques", debug_reset_uniques },
{ "check_uniques", debug_check_uniques },
{ "viewwindow", debug_viewwindow },
{ "seen_monsters_react", debug_seen_monsters_react },
{ "disable", debug_disable },
{ NULL, NULL }
};
