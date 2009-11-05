#include "AppHdr.h"

#include "cluautil.h"
#include "l_libs.h"

#include "delay.h"
#include "mon-util.h"
#include "monstuff.h"

/////////////////////////////////////////////////////////////////////
// Monster handling

#define MONS_METATABLE "monster.monsaccess"

struct MonsterWrap
{
    monsters *mons;
    long      turn;
};

void push_monster(lua_State *ls, monsters *mons)
{
    MonsterWrap *mw = clua_new_userdata< MonsterWrap >(ls, MONS_METATABLE);
    mw->turn = you.num_turns;
    mw->mons = mons;
}

#define MDEF(name)                                                      \
    static int l_mons_##name(lua_State *ls, monsters *mons,             \
                             const char *attr)                         \

#define MDEFN(name, closure)                    \
    static int l_mons_##name(lua_State *ls, monsters *mons, const char *attrs) \
    {                                                                   \
    lua_pushlightuserdata(ls, mons);                                    \
    lua_pushcclosure(ls, l_mons_##closure, 1);                          \
    return (1);                                                         \
    }

MDEF(name)
{
    PLUARET(string, mons_type_name(mons->type, DESC_PLAIN).c_str());
}

MDEF(x)
{
    PLUARET(number, int(mons->pos().x) - int(you.pos().x));
}

MDEF(y)
{
    PLUARET(number, int(mons->pos().y) - int(you.pos().y));
}

MDEF(hd)
{
    PLUARET(number, mons->hit_dice);
}

static const char *_monuse_names[] =
{
    "nothing", "open_doors", "starting_equipment", "weapons_armour",
    "magic_items"
};

static const char *_monuse_to_str(mon_itemuse_type utyp)
{
    COMPILE_CHECK(ARRAYSZ(_monuse_names) == NUM_MONUSE, c1);
    return _monuse_names[utyp];
}

MDEF(muse)
{
    if (const monsterentry *me = mons->find_monsterentry())
    {
        PLUARET(string, _monuse_to_str(me->gmon_use));
    }
    return (0);
}

static const char *_moneat_names[] =
{
    "nothing", "items", "corpses", "food"
};

static const char *_moneat_to_str(mon_itemeat_type etyp)
{
    COMPILE_CHECK(ARRAYSZ(_moneat_names) == NUM_MONEAT, c1);
    return _moneat_names[etyp];
}

MDEF(meat)
{
    if (const monsterentry *me = mons->find_monsterentry())
    {
        PLUARET(string, _moneat_to_str(me->gmon_eat));
    }
    return (0);
}

static int l_mons_do_dismiss(lua_State *ls)
{
    // dismiss is only callable from dlua, not from managed VMs (i.e.
    // end-user scripts cannot dismiss monsters).
    ASSERT_DLUA;
    monsters *mons =
        util_get_userdata<monsters>(ls, lua_upvalueindex(1));
    if (mons->alive())
    {
        mons->flags |= MF_HARD_RESET;
        monster_die(mons, KILL_DISMISSED, NON_MONSTER);
    }
    return (0);
}

MDEFN(dismiss, do_dismiss)

static int l_mons_do_random_teleport(lua_State *ls)
{
    // We should only be able to teleport monsters from dlua.
    ASSERT_DLUA;

    monsters *mons =
        util_get_userdata<monsters>(ls, lua_upvalueindex(1));
    if (mons->alive())
    {
        mons->teleport(true);
    }
    return (0);
}

MDEFN(random_teleport, do_random_teleport)

MDEF(experience)
{
    ASSERT_DLUA;
    PLUARET(number, exper_value(mons));
}

struct MonsAccessor
{
    const char *attribute;
    int (*accessor)(lua_State *ls, monsters *mons, const char *attr);
};

static MonsAccessor mons_attrs[] =
{
    { "name", l_mons_name },
    { "x"   , l_mons_x    },
    { "y"   , l_mons_y    },
    { "hd"  , l_mons_hd   },
    { "muse", l_mons_muse },
    { "meat", l_mons_meat },
    { "dismiss", l_mons_dismiss },
    { "experience", l_mons_experience },
    { "random_teleport", l_mons_random_teleport }
};

static int monster_get(lua_State *ls)
{
    MonsterWrap *mw = clua_get_userdata< MonsterWrap >(ls, MONS_METATABLE);
    if (!mw || mw->turn != you.num_turns || !mw->mons)
        return (0);

    const char *attr = luaL_checkstring(ls, 2);
    if (!attr)
        return (0);

    for (unsigned i = 0; i < sizeof(mons_attrs) / sizeof(mons_attrs[0]); ++i)
    {
        if (!strcmp(attr, mons_attrs[i].attribute))
            return (mons_attrs[i].accessor(ls, mw->mons, attr));
    }

    return (0);
}

static const char *_monster_behaviour_names[] = {
    "sleep",
    "wander",
    "seek",
    "flee",
    "cornered",
    "panic",
    "lurk"
};

static beh_type behaviour_by_name(const std::string &name) {
    ASSERT(ARRAYSZ(_monster_behaviour_names) == NUM_BEHAVIOURS);
    for (unsigned i = 0; i < ARRAYSZ(_monster_behaviour_names); ++i)
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
        return (0);

    const char *attr = luaL_checkstring(ls, 2);
    if (!attr)
        return (0);

    if (!strcmp(attr, "beh")) {
        const beh_type beh =
            lua_isnumber(ls, 3) ?
            static_cast<beh_type>(luaL_checkint(ls, 3)) :
            lua_isstring(ls, 3) ? behaviour_by_name(lua_tostring(ls, 3)) :
            NUM_BEHAVIOURS;
        if (beh != NUM_BEHAVIOURS)
            mw->mons->behaviour = beh;
    }

    return (0);
}

static int mons_behaviour(lua_State *ls) {
    if (lua_gettop(ls) < 1)
        return (0);

    if (lua_isnumber(ls, 1)) {
        lua_pushvalue(ls, 1);
        return (1);
    }
    else if (lua_isstring(ls, 1)) {
        const beh_type beh = behaviour_by_name(lua_tostring(ls, 1));
        if (beh != NUM_BEHAVIOURS) {
            lua_pushnumber(ls, beh);
            return (1);
        }
    }
    return (0);
}

static const struct luaL_reg mons_lib[] =
{
    { "behaviour", mons_behaviour },
    { NULL, NULL }
};

void dluaopen_monsters(lua_State *ls)
{
    luaL_newmetatable(ls, MONS_METATABLE);
    lua_pushstring(ls, "__index");
    lua_pushcfunction(ls, monster_get);
    lua_settable(ls, -3);

    lua_pushstring(ls, "__newindex");
    lua_pushcfunction(ls, monster_set);
    lua_settable(ls, -3);

    // Pop the metatable off the stack.
    lua_pop(ls, 1);

    luaL_openlib(ls, "mons", mons_lib, 0);
}
