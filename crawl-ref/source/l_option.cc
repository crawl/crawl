#include "AppHdr.h"

#include "clua.h"
#include "l_libs.h"

#include "options.h"

////////////////////////////////////////////////////////////////
// Option handling

typedef int (*ohandler)(lua_State *ls, const char *name, void *data, bool get);
struct option_handler
{
    const char *option;
    void       *data;
    ohandler    handler;
};

static int option_hboolean(lua_State *ls, const char *name, void *data,
                           bool get)
{
    if (get)
    {
        lua_pushboolean(ls, *static_cast<bool*>(data));
        return 1;
    }
    else
    {
        if (lua_isboolean(ls, 3))
            *static_cast<bool*>(data) = lua_toboolean(ls, 3);
        return 0;
    }
}

static int option_autopick(lua_State *ls, const char *name, void *data,
                           bool get)
{
    lua_pushboolean(ls, Options.autopickup_on>0);
    return 1;
}

static option_handler handlers[] =
{
    // Boolean options come first
    { "autoswitch",    &Options.auto_switch, option_hboolean },
    { "clean_map",     &Options.clean_map, option_hboolean },
    { "show_uncursed", &Options.show_uncursed, option_hboolean },
    { "easy_open",     &Options.easy_open, option_hboolean },
    { "easy_armour",   &Options.easy_unequip, option_hboolean },
    { "easy_unequip",  &Options.easy_unequip, option_hboolean },
    { "default_target",       &Options.default_target, option_hboolean },
    { "autopickup_no_burden", &Options.autopickup_no_burden, option_hboolean },
    { "note_skill_max",       &Options.note_skill_max, option_hboolean },
    { "clear_messages",  &Options.clear_messages, option_hboolean },
    { "no_dark_brand",   &Options.no_dark_brand, option_hboolean },
    { "auto_list",       &Options.auto_list, option_hboolean },
    { "pickup_thrown",   &Options.pickup_thrown, option_hboolean },
    { "show_waypoints",  &Options.show_waypoints, option_hboolean },
    { "easy_exit_menu",  &Options.easy_exit_menu, option_hboolean },
    { "dos_use_background_intensity", &Options.dos_use_background_intensity,
                                      option_hboolean },
    { "autopick_on", NULL, option_autopick }
};

static const option_handler *get_handler(const char *optname)
{
    if (optname)
    {
        for (int i = 0, count = ARRAYSZ(handlers); i < count; ++i)
        {
            if (!strcmp(handlers[i].option, optname))
                return &handlers[i];
        }
    }
    return NULL;
}

static int option_get(lua_State *ls)
{
    const char *opt = luaL_checkstring(ls, 2);
    if (!opt)
        return 0;

    // Is this a Lua named option?
    game_options::opt_map::iterator i = Options.named_options.find(opt);
    if (i != Options.named_options.end())
    {
        const string &ov = i->second;
        lua_pushstring(ls, ov.c_str());
        return 1;
    }

    const option_handler *oh = get_handler(opt);
    if (oh)
#ifdef DEBUG_GLOBALS
        return oh->handler(ls, opt, (char*)real_Options+(intptr_t)oh->data, true);
#else
        return oh->handler(ls, opt, oh->data, true);
#endif

    return 0;
}

static int option_set(lua_State *ls)
{
    const char *opt = luaL_checkstring(ls, 2);
    if (!opt)
        return 0;

    const option_handler *oh = get_handler(opt);
    if (oh)
        oh->handler(ls, opt, oh->data, false);

    return 0;
}

#define OPT_METATABLE "clua_metatable_optaccess"

void cluaopen_options(lua_State *ls)
{
    int top = lua_gettop(ls);

    luaL_newmetatable(ls, OPT_METATABLE);
    lua_pushstring(ls, "__index");
    lua_pushcfunction(ls, option_get);
    lua_settable(ls, -3);

    luaL_getmetatable(ls, OPT_METATABLE);
    lua_pushstring(ls, "__newindex");
    lua_pushcfunction(ls, option_set);
    lua_settable(ls, -3);

    lua_settop(ls, top);

    // Create dummy userdata to front for our metatable
    int *dummy = static_cast<int *>(lua_newuserdata(ls, sizeof(int)));
    // Mystic number
    *dummy = 42;

    luaL_getmetatable(ls, OPT_METATABLE);
    lua_setmetatable(ls, -2);
    lua_setglobal(ls, "options");
}
