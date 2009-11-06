/*
 * File:      l_dgngrd.cc
 * Summary:   Grid and dungeon_feature_type-related bindings.
 */

#include "AppHdr.h"

#include "clua.h"
#include "cluautil.h"
#include "l_libs.h"

#include "coord.h"
#include "directn.h"
#include "env.h"
#include "religion.h"
#include "terrain.h"

const char *dngn_feature_names[] =
{
"unseen", "closed_door", "detected_secret_door", "secret_door",
"wax_wall", "metal_wall", "green_crystal_wall", "rock_wall", "stone_wall",
"permarock_wall",
"clear_rock_wall", "clear_stone_wall", "clear_permarock_wall", "trees",
"open_sea", "orcish_idol", "", "", "", "", "",
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
"altar_beogh", "altar_jiyva", "altar_feawn", "altar_cheibriados", "", "", "",
"fountain_blue", "fountain_sparkling", "fountain_blood",
"dry_fountain_blue", "dry_fountain_sparkling", "dry_fountain_blood",
"permadry_fountain", "abandoned_shop"
};

dungeon_feature_type dungeon_feature_by_name(const std::string &name)
{
    COMPILE_CHECK(ARRAYSZ(dngn_feature_names) == NUM_FEATURES, c1);
    if (name.empty())
        return (DNGN_UNSEEN);

    for (unsigned i = 0; i < ARRAYSZ(dngn_feature_names); ++i)
    {
        if (dngn_feature_names[i] == name)
        {
            if (jiyva_is_dead() && name == "altar_jiyva")
                return (DNGN_FLOOR);

            return (static_cast<dungeon_feature_type>(i));
        }
    }

    return (DNGN_UNSEEN);
}

std::vector<std::string> dungeon_feature_matches(const std::string &name)
{
    std::vector<std::string> matches;

    COMPILE_CHECK(ARRAYSZ(dngn_feature_names) == NUM_FEATURES, c1);
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

dungeon_feature_type check_lua_feature(lua_State *ls, int idx)
{
    const dungeon_feature_type f = _get_lua_feature(ls, idx);
    if (!f)
        luaL_argerror(ls, idx, "Invalid dungeon feature");
    return (f);
}

#define FEAT(f, pos) \
dungeon_feature_type f = check_lua_feature(ls, pos)

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

static int dgn_max_bounds(lua_State *ls)
{
    lua_pushnumber(ls, GXM);
    lua_pushnumber(ls, GYM);
    return (2);
}

static int dgn_in_bounds(lua_State *ls)
{
    int x = luaL_checkint(ls, 1);
    int y = luaL_checkint(ls, 2);

    lua_pushboolean(ls, in_bounds(x, y));
    return 1;
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

// XXX: these two shouldn't be so different.
LUARET1(_dgn_is_wall, boolean,
        feat_is_wall(static_cast<dungeon_feature_type>(luaL_checkint(ls, 1))))

LUAFN(dgn_distance)
{
    COORDS(p1, 1, 2);
    COORDS(p2, 3, 4);
    lua_pushnumber(ls, distance(p1, p2));
    return (1);
}

LUAFN(_dgn_is_opaque)
{
    COORDS(c, 1, 2);
    lua_pushboolean(ls, feat_is_opaque(grd(c)));
    return (1);
}

const struct luaL_reg dgn_grid_dlib[] =
{
{ "feature_number", dgn_feature_number },
{ "feature_name", dgn_feature_name },
{ "feature_desc", dgn_feature_desc },
{ "feature_desc_at", dgn_feature_desc_at },
{ "set_feature_desc_short", dgn_set_feature_desc_short },
{ "set_feature_desc_long", dgn_set_feature_desc_long },

{ "grid", dgn_grid },
{ "is_opaque", _dgn_is_opaque },
{ "is_wall", _dgn_is_wall },
{ "max_bounds", dgn_max_bounds },
{ "in_bounds", dgn_in_bounds },
{ "distance", dgn_distance },

{ NULL, NULL }
};
