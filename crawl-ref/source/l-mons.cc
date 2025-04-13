/*** Dungeon building monster functions (dlua only).
 * @module mons
 */
#include "AppHdr.h"

#include "l-libs.h"

#include "areas.h"
#include "cluautil.h"
#include "database.h"
#include "dlua.h"
#include "items.h"
#include "libutil.h"
#include "mon-act.h"
#include "mon-behv.h"
#include "mon-death.h"
#include "mon-pick.h"
#include "mon-speak.h"
#include "monster.h"
#include "mon-util.h"
#include "stringutil.h"
#include "tag-version.h"

#define WRAPPED_MONSTER(ls, name)                                       \
    MonsterWrap *___mw = clua_get_userdata< MonsterWrap >(ls, MONS_METATABLE); \
    if (!___mw                                                          \
        || !___mw->mons                                              \
        || CLua::get_vm(ls).managed_vm && ___mw->turn != you.num_turns) \
    {                                                                \
        luaL_argerror(ls, 1, "Invalid monster wrapper");             \
    } \
    monster *name(___mw ? ___mw->mons : nullptr)

void push_monster(lua_State *ls, monster* mons)
{
    MonsterWrap *mw = clua_new_userdata< MonsterWrap >(ls, MONS_METATABLE);
    mw->turn = you.num_turns;
    mw->mons = mons;
}

#define MDEF(name)                                                      \
    static int l_mons_##name(lua_State *ls, monster* mons)             \

#define MDEFN(name, closure)                    \
    static int l_mons_##name(lua_State *ls, monster* mons) \
    {                                                                   \
    lua_pushlightuserdata(ls, mons);                                    \
    lua_pushcclosure(ls, l_mons_##closure, 1);                          \
    return 1;                                                         \
    }

MDEF(name)
{
    PLUARET(string, mons->name(DESC_PLAIN, true).c_str());
}

MDEF(unique)
{
    PLUARET(boolean, mons_is_unique(mons->type));
}

MDEF(base_name)
{
    PLUARET(string, mons->base_name(DESC_PLAIN, true).c_str());
}

MDEF(full_name)
{
    PLUARET(string, mons->full_name(DESC_PLAIN).c_str());
}

MDEF(db_name)
{
    PLUARET(string, mons->name(DESC_DBNAME, true).c_str());
}

MDEF(type_name)
{
    PLUARET(string, mons_type_name(mons->type, DESC_PLAIN).c_str());
}

MDEF(entry_name)
{
    ASSERT_DLUA;

    const monsterentry *me = get_monster_data(mons->type);
    if (me)
        lua_pushstring(ls, me->name);
    else
        lua_pushnil(ls);

    return 1;
}

MDEF(x)
{
    PLUARET(number, int(mons->pos().x) - int(you.pos().x));
}

MDEF(y)
{
    PLUARET(number, int(mons->pos().y) - int(you.pos().y));
}

MDEF(hp)
{
    PLUARET(number, int(mons->hit_points));
}

static const char* _behaviour_name(beh_type beh);
MDEF(beh)
{
    PLUARET(string, _behaviour_name(mons->behaviour));
}

MDEF(energy)
{
    // XXX: fix this after speed_increment clean up
    PLUARET(number, (mons->speed_increment - 79));
}

MDEF(in_local_population)
{
    // TODO: should this be in moninf? need a mons for the native check...
    // The nativity check is because there are various cases where monsters are
    // not in the population tables, but should be considered native, e.g.
    // vault gaurds in vaults, orb guardians in zot 5
    PLUARET(boolean,
        mons_is_native_in_branch(*mons, you.where_are_you)
     || monster_in_population(you.where_are_you, mons->type)
     || monster_in_population(you.where_are_you, mons->mons_species(false))
     || monster_in_population(you.where_are_you, mons->mons_species(true)));
}

LUAFN(l_mons_add_energy)
{
    ASSERT_DLUA;
    monster* mons = clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));
    mons->speed_increment += luaL_safe_checkint(ls, 1);
    return 0;
}
MDEFN(add_energy, add_energy)

#define LUANAMEFN(name, expr)                     \
    static int l_mons_##name##_fn(lua_State *ls) {      \
       ASSERT_DLUA;                              \
       const monster* mons =                                    \
           clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1)); \
       const description_level_type dtype =                          \
           lua_isstring(ls, 1)?                                      \
           description_type_by_name(luaL_checkstring(ls, 1))         \
           : DESC_PLAIN;                                             \
       PLUARET(string, expr.c_str());             \
    }                                             \
    MDEFN(name, name##_fn)                        \

LUANAMEFN(mname, mons->name(dtype, true))
LUANAMEFN(mfull_name, mons->full_name(dtype))
LUANAMEFN(mbase_name, mons->base_name(dtype, true))

MDEF(hd)
{
    PLUARET(number, mons->get_hit_dice());
}

MDEF(targetx)
{
    PLUARET(number, mons->target.x);
}

MDEF(targety)
{
    PLUARET(number, mons->target.y);
}

MDEF(shapeshifter)
{
    ASSERT_DLUA;
    if (mons->has_ench(ENCH_GLOWING_SHAPESHIFTER))
        lua_pushstring(ls, "glowing shapeshifter");
    else if (mons->has_ench(ENCH_SHAPESHIFTER))
        lua_pushstring(ls, "shapeshifter");
    else
        lua_pushnil(ls);
    return 1;
}

MDEF(dancing_weapon)
{
    ASSERT_DLUA;

    if (mons_genus(mons->type) == MONS_DANCING_WEAPON)
        lua_pushstring(ls, "dancing weapon");
    else
        lua_pushnil(ls);

    return 1;
}

MDEF(wont_attack)
{
    ASSERT_DLUA;
    lua_pushboolean(ls, mons->wont_attack());
    return 1;
}

static const char *_monuse_names[] =
{
    "nothing", "open_doors", "starting_equipment", "weapons_armour"
};

static const char *_monuse_to_str(mon_itemuse_type utyp)
{
    COMPILE_CHECK(ARRAYSZ(_monuse_names) == NUM_MONUSE);
    return _monuse_names[utyp];
}

MDEF(muse)
{
    if (const monsterentry *me = mons->find_monsterentry())
        PLUARET(string, _monuse_to_str(me->gmon_use));
    return 0;
}

static int l_mons_get_inventory(lua_State *ls)
{
    monster* mons = clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));
    lua_newtable(ls);
    int index = 0;
    for (mon_inv_iterator ii(*mons); ii; ++ii)
    {
        if (ii->defined())
        {
            clua_push_item(ls, &*ii);
            lua_rawseti(ls, -2, ++index);
        }
    }
    return 1;
}
MDEFN(inventory, get_inventory)

static int l_mons_do_dismiss(lua_State *ls)
{
    // dismiss is only callable from dlua, not from managed VMs (i.e.
    // end-user scripts cannot dismiss monsters).
    ASSERT_DLUA;
    monster* mons = clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));

    if (mons->alive())
        monster_die(*mons, KILL_RESET, NON_MONSTER);
    return 0;
}
MDEFN(dismiss, do_dismiss)

static int l_mons_set_hp(lua_State *ls)
{
    ASSERT_DLUA;

    monster* mons =
        clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));

    int hp = luaL_safe_checkint(ls, 1);
    if (hp <= 0)
    {
        luaL_argerror(ls, 1, "hp must be positive");
        return 0;
    }
    hp = min(hp, mons->max_hit_points);
    mons->hit_points = min(hp, MAX_MONSTER_HP);
    return 0;
}
MDEFN(set_hp, set_hp)

static int l_mons_set_max_hp(lua_State *ls)
{
    ASSERT_DLUA;

    monster* mons =
        clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));

    int maxhp = luaL_safe_checkint(ls, 1);
    if (maxhp <= 0)
    {
        luaL_argerror(ls, 1, "maxhp must be positive");
        return 0;
    }
    mons->max_hit_points = min(maxhp, MAX_MONSTER_HP);
    mons->hit_points = mons->max_hit_points;
    return 0;
}
MDEFN(set_max_hp, set_max_hp)

// Run the monster AI code.
static int l_mons_do_run_ai(lua_State *ls)
{
    ASSERT_DLUA;
    monster* mons = clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));
    if (mons->alive())
        handle_monster_move(mons);
    return 0;
}
MDEFN(run_ai, do_run_ai)

static int l_mons_do_handle_behaviour(lua_State *ls)
{
    ASSERT_DLUA;
    monster* mons = clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));
    if (mons->alive())
        handle_behaviour(mons);
    return 0;
}
MDEFN(handle_behaviour, do_handle_behaviour)

static int l_mons_do_random_teleport(lua_State *ls)
{
    // We should only be able to teleport monsters from dlua.
    ASSERT_DLUA;

    monster* mons = clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));

    if (mons->alive())
        mons->teleport(true);

    return 0;
}

MDEFN(random_teleport, do_random_teleport)

MDEF(experience)
{
    ASSERT_DLUA;
    PLUARET(number, exper_value(*mons));
}

static int l_mons_do_set_prop(lua_State *ls)
{
    // We should only be able to set properties from dlua.
    ASSERT_DLUA;

    monster* mons =
        clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));

    const char *prop_name = luaL_checkstring(ls, 1);

    if (lua_isnoneornil(ls, 2))
        mons->props.erase(prop_name);
    else if (lua_isboolean(ls, 2))
        mons->props[prop_name] = (bool) lua_toboolean(ls, 2);
    // NOTE: number has to be before string, or numbers will get converted
    // into strings.
    else if (lua_isnumber(ls, 2))
        mons->props[prop_name].get_int() = luaL_safe_checklong(ls, 2);
    else if (lua_isstring(ls, 2))
        mons->props[prop_name] = lua_tostring(ls, 2);
    else if (lua_isfunction(ls, 2))
    {
        dlua_chunk chunk(ls);
        mons->props[prop_name] = chunk;
    }
    else
    {
        string err
            = make_stringf("Don't know how to set monster property of the "
                           "given value type for property '%s'", prop_name);
        luaL_argerror(ls, 2, err.c_str());
    }

    return 0;
}

MDEFN(set_prop, do_set_prop)

static int l_mons_do_get_prop(lua_State *ls)
{
    // We should only be able to set properties from dlua.
    ASSERT_DLUA;

    monster* mons =
        clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));

    const char *prop_name = luaL_checkstring(ls, 1);

    if (!mons->props.exists(prop_name))
    {
        if (lua_isboolean(ls, 2))
        {
            string err = make_stringf("Don't have a property called '%s'.", prop_name);
            luaL_argerror(ls, 2, err.c_str());
        }

        return 0;
    }

    CrawlStoreValue prop = mons->props[prop_name];
    int num_pushed = 1;

    switch (prop.get_type())
    {
    case SV_NONE: lua_pushnil(ls); break;
    case SV_BOOL: lua_pushboolean(ls, prop.get_bool()); break;
    case SV_BYTE: lua_pushboolean(ls, prop.get_byte()); break;
    case SV_SHORT: lua_pushnumber(ls, prop.get_short()); break;
    case SV_INT: lua_pushnumber(ls, prop.get_int()); break;
    case SV_FLOAT: lua_pushnumber(ls, prop.get_float()); break;
    case SV_STR: lua_pushstring(ls, prop.get_string().c_str()); break;
    case SV_COORD:
        num_pushed++;
        lua_pushnumber(ls, prop.get_coord().x);
        lua_pushnumber(ls, prop.get_coord().y);
        break;
    default:
        // Do nothing for some things.
        lua_pushnil(ls);
        break;
    }

    return num_pushed;
}

MDEFN(get_prop, do_get_prop)

static int l_mons_do_has_prop(lua_State *ls)
{
    // We should only be able to get properties from dlua.
    ASSERT_DLUA;

    monster* mons =
        clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));

    const char *prop_name = luaL_checkstring(ls, 1);

    lua_pushboolean(ls, mons->props.exists(prop_name));
    return 1;
}

MDEFN(has_prop, do_has_prop)

static int l_mons_do_add_ench(lua_State *ls)
{
    ASSERT_DLUA;

    monster* mons =
        clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));

    const char *ench_name = luaL_checkstring(ls, 1);
    enchant_type met = name_to_ench(ench_name);
    if (!met)
    {
        string err = make_stringf("No such enchantment: %s", ench_name);
        luaL_argerror(ls, 1, err.c_str());
        return 0;
    }

    mons->add_ench(mon_enchant(met, luaL_safe_checkint(ls, 2), 0,
                               luaL_safe_checkint(ls, 3)));
    return 0;
}

MDEFN(add_ench, do_add_ench)

static int l_mons_do_del_ench(lua_State *ls)
{
    ASSERT_DLUA;

    monster* mons =
        clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));

    const char *ench_name = luaL_checkstring(ls, 1);
    enchant_type met = name_to_ench(ench_name);
    if (!met)
    {
        string err = make_stringf("No such enchantment: %s", ench_name);
        luaL_argerror(ls, 1, err.c_str());
        return 0;
    }

    mons->del_ench(met);
    return 0;
}

MDEFN(del_ench, do_del_ench)

MDEF(you_can_see)
{
    ASSERT_DLUA;
    PLUARET(boolean, you.can_see(*mons));
}

static int l_mons_do_speak(lua_State *ls)
{
    // We should only be able to get monsters to speak from dlua.
    ASSERT_DLUA;

    monster* mons = clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));

    const char *speech_key = luaL_checkstring(ls, 1);

    mons_speaks_msg(mons, getSpeakString(speech_key), MSGCH_TALK,
                    silenced(mons->pos()));

    return 0;
}

MDEFN(speak, do_speak)

static int l_mons_do_get_info(lua_State *ls)
{
    // for a non-dlua version, see monster.get_monster_at
    ASSERT_DLUA;
    monster* m = clua_get_lightuserdata<monster>(ls, lua_upvalueindex(1));
    monster_info mi(m);
    lua_push_moninf(ls, &mi);
    return 1;
}

MDEFN(get_info, do_get_info)

struct MonsAccessor
{
    const char *attribute;
    int (*accessor)(lua_State *ls, monster* mons);
};

static MonsAccessor mons_attrs[] =
{
    { "name",           l_mons_name      },
    { "base_name",      l_mons_base_name },
    { "full_name",      l_mons_full_name },
    { "db_name",        l_mons_db_name   },
    { "type_name",      l_mons_type_name },
    { "entry_name",     l_mons_entry_name },
    { "unique"   ,      l_mons_unique },
    { "shapeshifter",   l_mons_shapeshifter },
    { "dancing_weapon", l_mons_dancing_weapon },
    { "wont_attack",    l_mons_wont_attack },

    { "x"   , l_mons_x    },
    { "y"   , l_mons_y    },
    { "hd"  , l_mons_hd   },
    { "beh" , l_mons_beh  },
    { "muse", l_mons_muse },
    { "hp"  , l_mons_hp   },

    { "targetx", l_mons_targetx },
    { "targety", l_mons_targety },

    { "mname",           l_mons_mname           },
    { "mfull_name",      l_mons_mfull_name      },
    { "mbase_name",      l_mons_mbase_name      },
    { "energy",          l_mons_energy          },
    { "add_energy",      l_mons_add_energy      },
    { "dismiss",         l_mons_dismiss         },
    { "set_hp",          l_mons_set_hp          },
    { "set_max_hp",      l_mons_set_max_hp      },
    { "run_ai",          l_mons_run_ai          },
    { "handle_behaviour",l_mons_handle_behaviour },
    { "experience",      l_mons_experience      },
    { "random_teleport", l_mons_random_teleport },
    { "set_prop",        l_mons_set_prop        },
    { "get_prop",        l_mons_get_prop        },
    { "has_prop",        l_mons_has_prop        },
    { "add_ench",        l_mons_add_ench        },
    { "del_ench",        l_mons_del_ench        },
    { "you_can_see",     l_mons_you_can_see     },
    { "get_inventory",   l_mons_inventory       },
    { "in_local_population", l_mons_in_local_population },

    { "speak",           l_mons_speak           },
    { "get_info",        l_mons_get_info        }
};

static int monster_get(lua_State *ls)
{
    WRAPPED_MONSTER(ls, mons);

    const char *attr = luaL_checkstring(ls, 2);
    if (!attr)
        return 0;

    for (const MonsAccessor &ma : mons_attrs)
        if (!strcmp(attr, ma.attribute))
            return ma.accessor(ls, mons);

    return 0;
}

static const char *_monster_behaviour_names[] =
{
    "sleep",
    "wander",
    "seek",
    "flee",
    "cornered",
#if TAG_MAJOR_VERSION == 34
    "panic",
    "lurk",
#endif
    "retreat",
    "withdraw",
    "batty",
};

static const char* _behaviour_name(beh_type beh)
{
    if (0 <= beh && beh < NUM_BEHAVIOURS)
        return _monster_behaviour_names[beh];
    else
        return "invalid";
}

static beh_type behaviour_by_name(const string &name)
{
    COMPILE_CHECK(ARRAYSZ(_monster_behaviour_names) == NUM_BEHAVIOURS);

    for (unsigned i = 0; i < NUM_BEHAVIOURS; ++i)
        if (name == _monster_behaviour_names[i])
            return static_cast<beh_type>(i);

    return NUM_BEHAVIOURS;
}

static int monster_set(lua_State *ls)
{
    // Changing monster behaviour is for the dungeon builder only,
    // never for user scripts.
    ASSERT_DLUA;

    MonsterWrap *mw = clua_get_userdata< MonsterWrap >(ls, MONS_METATABLE);
    if (!mw || !mw->mons)
        return 0;

    const char *attr = luaL_checkstring(ls, 2);
    if (!attr)
        return 0;

    if (!strcmp(attr, "beh"))
    {
        const beh_type beh =
            lua_isnumber(ls, 3) ? static_cast<beh_type>(luaL_safe_checkint(ls, 3)) :
            lua_isstring(ls, 3) ? behaviour_by_name(lua_tostring(ls, 3))
                                : NUM_BEHAVIOURS;

        if (beh != NUM_BEHAVIOURS)
            mw->mons->behaviour = beh;
    }
    else if (!strcmp(attr, "targetx"))
        mw->mons->target.x = luaL_safe_checkint(ls, 3);
    else if (!strcmp(attr, "targety"))
        mw->mons->target.y = luaL_safe_checkint(ls, 3);

    return 0;
}

static int mons_behaviour(lua_State *ls)
{
    if (lua_gettop(ls) < 1)
        return 0;

    if (lua_isnumber(ls, 1))
    {
        lua_pushvalue(ls, 1);
        return 1;
    }
    else if (lua_isstring(ls, 1))
    {
        const beh_type beh = behaviour_by_name(lua_tostring(ls, 1));
        if (beh != NUM_BEHAVIOURS)
        {
            lua_pushnumber(ls, beh);
            return 1;
        }
    }
    return 0;
}

static const struct luaL_reg mons_lib[] =
{
    { "behaviour", mons_behaviour },
    { nullptr, nullptr }
};

void dluaopen_monsters(lua_State *ls)
{
    lua_stack_cleaner stack_clean(ls);
    luaL_newmetatable(ls, MONS_METATABLE);
    lua_pushstring(ls, "__index");
    lua_pushcfunction(ls, monster_get);
    lua_settable(ls, -3);

    lua_pushstring(ls, "__newindex");
    lua_pushcfunction(ls, monster_set);
    lua_settable(ls, -3);
    luaL_register(ls, "mons", mons_lib);
}
