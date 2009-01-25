/*
 *  File:       clua.cc
 *  Created by: dshaligram on Wed Aug 2 12:54:15 2006 UTC
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"
REVISION("$Rev$");

#include <algorithm>
#include <sstream>

#include "clua.h"

#include "abl-show.h"
#include "command.h"
#include "chardump.h"
#include "cio.h"
#include "delay.h"
#include "dgnevent.h"
#include "dungeon.h"
#include "files.h"
#include "food.h"
#include "invent.h"
#include "initfile.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "it_use2.h"
#include "libutil.h"
#include "macro.h"
#include "mapdef.h"
#include "message.h"
#include "monstuff.h"
#include "mon-util.h"
#include "newgame.h"
#include "notes.h"
#include "output.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "skills2.h"
#include "spl-util.h"
#include "state.h"
#include "stuff.h"
#include "transfor.h"
#include "travel.h"
#include "view.h"

#include <cstring>
#include <map>
#include <cctype>

#define BUGGY_PCALL_ERROR  "667: Malformed response to guarded pcall."
#define BUGGY_SCRIPT_ERROR "666: Killing badly-behaved Lua script."

#define CL_RESETSTACK_RETURN(ls, oldtop, retval) \
    do \
    {\
        if (oldtop != lua_gettop(ls)) \
        { \
            lua_settop(ls, oldtop); \
        } \
        return (retval); \
    } \
    while (false)

static int  _clua_panic(lua_State *);
static void _clua_throttle_hook(lua_State *, lua_Debug *);
static void *_clua_allocator(void *ud, void *ptr, size_t osize, size_t nsize);
static int  _clua_guarded_pcall(lua_State *);
static int  _clua_require(lua_State *);
static int  _clua_dofile(lua_State *);
static int  _clua_loadfile(lua_State *);

CLua::CLua(bool managed)
    : error(), managed_vm(managed), shutting_down(false),
      throttle_unit_lines(10000),
      throttle_sleep_ms(0), throttle_sleep_start(2),
      throttle_sleep_end(800), n_throttle_sleeps(0), mixed_call_depth(0),
      lua_call_depth(0), max_mixed_call_depth(8),
      max_lua_call_depth(100), memory_used(0),
      _state(NULL), sourced_files(), uniqindex(0L)
{
}

CLua::~CLua()
{
    // Copy the listener vector, because listeners may remove
    // themselves from the listener list when we notify them of a
    // shutdown.
    const std::vector<lua_shutdown_listener*> slisteners = shutdown_listeners;
    for (int i = 0, size = slisteners.size(); i < size; ++i)
        slisteners[i]->shutdown(*this);
    shutting_down = true;
    if (_state)
        lua_close(_state);
}

lua_State *CLua::state()
{
    if (!_state)
        init_lua();
    return _state;
}

void CLua::setglobal(const char *name)
{
    lua_setglobal(state(), name);
}

void CLua::getglobal(const char *name)
{
    lua_getglobal(state(), name);
}

std::string CLua::setuniqregistry()
{
    char name[100];
    snprintf(name, sizeof name, "__cru%lu", uniqindex++);
    lua_pushstring(state(), name);
    lua_insert(state(), -2);
    lua_settable(state(), LUA_REGISTRYINDEX);

    return (name);
}

void CLua::setregistry(const char *name)
{
    lua_pushstring(state(), name);
    // Slide name round before the value
    lua_insert(state(), -2);
    lua_settable(state(), LUA_REGISTRYINDEX);
}

void CLua::_getregistry(lua_State *ls, const char *name)
{
    lua_pushstring(ls, name);
    lua_gettable(ls, LUA_REGISTRYINDEX);
}

void CLua::getregistry(const char *name)
{
    _getregistry(state(), name);
}

void CLua::save(const char *file)
{
    if (!_state)
        return;

    CLuaSave clsave = { file, NULL };
    callfn("c_save", "u", &clsave);
    if (clsave.handle)
        fclose(clsave.handle);
}

int CLua::file_write(lua_State *ls)
{
    if (!lua_islightuserdata(ls, 1))
    {
        luaL_argerror(ls, 1, "Expected filehandle at arg 1");
        return (0);
    }
    CLuaSave *sf = static_cast<CLuaSave *>( lua_touserdata(ls, 1) );
    if (!sf)
        return (0);

    FILE *f = sf->get_file();
    if (!f)
        return (0);

    const char *text = luaL_checkstring(ls, 2);
    if (text)
        fprintf(f, "%s", text);
    return (0);
}

FILE *CLua::CLuaSave::get_file()
{
    if (!handle)
        handle = fopen(filename, "w");

    return (handle);
}

void CLua::set_error(int err, lua_State *ls)
{
    if (!err)
    {
        error.clear();
        return;
    }
    if (!ls && !(ls = _state))
    {
        error = "<LUA not initialized>";
        return;
    }
    const char *serr = lua_tostring(ls, -1);
    lua_pop(ls, 1);
    error = serr? serr : "<Unknown error>";
}

void CLua::init_throttle()
{
    if (!managed_vm)
        return;

    if (throttle_unit_lines <= 0)
        throttle_unit_lines = 500;

    if (throttle_sleep_start < 1)
        throttle_sleep_start = 1;

    if (throttle_sleep_end < throttle_sleep_start)
        throttle_sleep_end = throttle_sleep_start;

    if (!mixed_call_depth)
    {
        lua_sethook(_state, _clua_throttle_hook,
                    LUA_MASKCOUNT, throttle_unit_lines);
        throttle_sleep_ms = 0;
        n_throttle_sleeps = 0;
    }
}

int CLua::loadbuffer(const char *buf, size_t size, const char *context)
{
    const int err = luaL_loadbuffer(state(), buf, size, context);
    set_error(err, state());
    return err;
}

int CLua::loadstring(const char *s, const char *context)
{
    return loadbuffer(s, strlen(s), context);
}

int CLua::execstring(const char *s, const char *context, int nresults)
{
    int err = 0;
    if ((err = loadstring(s, context)))
        return (err);

    lua_State *ls = state();
    lua_call_throttle strangler(this);
    err = lua_pcall(ls, 0, nresults, 0);
    set_error(err, ls);
    return err;
}

bool CLua::is_path_safe(std::string s, bool trusted)
{
    lowercase(s);
    return (s.find("..") == std::string::npos && shell_safe(s.c_str())
            && (trusted || s.find("clua") == std::string::npos));
}

int CLua::loadfile(lua_State *ls, const char *filename, bool trusted,
                   bool die_on_fail)
{
    if (!ls)
        return (-1);

    if (!is_path_safe(filename, trusted))
    {
        lua_pushstring(
            ls,
            make_stringf("invalid filename: %s", filename).c_str());
        return (-1);
    }

    std::string file = datafile_path(filename, die_on_fail);
    if (file.empty())
    {
        lua_pushstring(ls,
                       make_stringf("Can't find \"%s\"", filename).c_str());
        return (-1);
    }
    return (luaL_loadfile(ls, file.c_str()));
}

int CLua::execfile(const char *filename, bool trusted, bool die_on_fail)
{
    if (sourced_files.find(filename) != sourced_files.end())
        return 0;

    sourced_files.insert(filename);

    lua_State *ls = state();
    int err = loadfile(ls, filename, trusted || !managed_vm, die_on_fail);
    lua_call_throttle strangler(this);
    if (!err)
        err = lua_pcall(ls, 0, 0, 0);
    set_error(err);
    if (die_on_fail && !error.empty())
        end(1, false, "Lua execfile error (%s): %s",
            filename, dlua.error.c_str());
    return (err);
}

bool CLua::runhook(const char *hook, const char *params, ...)
{
    error.clear();

    lua_State *ls = state();
    if (!ls)
        return (false);

    // Remember top of stack, for debugging porpoises
    int stack_top = lua_gettop(ls);
    lua_getglobal(ls, hook);
    if (!lua_istable(ls, -1))
    {
        lua_pop(ls, 1);
        CL_RESETSTACK_RETURN( ls, stack_top, false );
    }
    for (int i = 1; ; ++i)
    {
        int currtop = lua_gettop(ls);
        lua_rawgeti(ls, -1, i);
        if (!lua_isfunction(ls, -1))
        {
            lua_pop(ls, 1);
            break;
        }

        // So what's on top *is* a function. Call it with the args we have.
        va_list args;
        va_start(args, params);
        calltopfn(ls, params, args);
        va_end(args);

        lua_settop(ls, currtop);
    }
    CL_RESETSTACK_RETURN( ls, stack_top, true );
}

void CLua::fnreturns(const char *format, ...)
{
    lua_State *ls = _state;

    if (!format || !ls)
        return;

    va_list args;
    va_start(args, format);
    vfnreturns(format, args);
    va_end(args);
}

void CLua::vfnreturns(const char *format, va_list args)
{
    lua_State *ls = _state;
    int nrets = return_count(ls, format);
    int sp = -nrets - 1;

    const char *gs = strchr(format, '>');
    if (gs)
        format = gs + 1;
    else if ((gs = strchr(format, ':')))
        format = gs + 1;

    for (const char *run = format; *run; ++run)
    {
        char argtype = *run;
        ++sp;
        switch (argtype)
        {
        case 'u':
            if (lua_islightuserdata(ls, sp))
                *(va_arg(args, void**)) = lua_touserdata(ls, sp);
            break;
        case 'd':
            if (lua_isnumber(ls, sp))
                *(va_arg(args, int*)) = luaL_checkint(ls, sp);
            break;
        case 'b':
            *(va_arg(args, bool *)) = lua_toboolean(ls, sp);
            break;
        case 's':
            {
                const char *s = lua_tostring(ls, sp);
                if (s)
                    *(va_arg(args, std::string *)) = s;
                break;
            }
        default:
            break;
        }

    }
    // Pop args off the stack
    lua_pop(ls, nrets);
}

static int push_activity_interrupt(lua_State *ls, activity_interrupt_data *t);
int CLua::push_args(lua_State *ls, const char *format, va_list args,
                    va_list *targ)
{
    if (!format)
    {
        if (targ)
            va_copy(*targ, args);
        return (0);
    }

    const char *cs = strchr(format, ':');
    if (cs)
        format = cs + 1;

    int argc = 0;
    for (const char *run = format; *run; run++)
    {
        if (*run == '>')
            break;

        char argtype = *run;
        ++argc;
        switch (argtype)
        {
        case 'u':       // Light userdata
            lua_pushlightuserdata(ls, va_arg(args, void*));
            break;
        case 's':       // String
        {
            const char *s = va_arg(args, const char *);
            if (s)
                lua_pushstring(ls, s);
            else
                lua_pushnil(ls);
            break;
        }
        case 'd':       // Integer
            lua_pushnumber(ls, va_arg(args, int));
            break;
        case 'L':
            lua_pushnumber(ls, va_arg(args, long));
            break;
        case 'b':
            lua_pushboolean(ls, va_arg(args, int));
            break;
        case 'D':
            clua_push_dgn_event(ls, va_arg(args, const dgn_event *));
            break;
        case 'm':
            clua_push_map(ls, va_arg(args, map_def *));
            break;
        case 'M':
            push_monster(ls, va_arg(args, monsters *));
            break;
        case 'A':
            argc += push_activity_interrupt(
                        ls, va_arg(args, activity_interrupt_data *));
            break;
        default:
            --argc;
            break;
        }
    }
    if (targ)
        va_copy(*targ, args);
    return (argc);
}

int CLua::return_count(lua_State *ls, const char *format)
{
    if (!format)
        return (0);

    const char *gs = strchr(format, '>');
    if (gs)
        return (strlen(gs + 1));

    const char *cs = strchr(format, ':');
    if (cs && isdigit(*format))
    {
        char *es = NULL;
        int ci = strtol(format, &es, 10);
        // We're capping return at 10 here, which is arbitrary, but avoids
        // blowing the stack.
        if (ci < 0)
            ci = 0;
        else if (ci > 10)
            ci = 10;
        return (ci);
    }
    return (0);
}

bool CLua::calltopfn(lua_State *ls, const char *params, va_list args,
                     int retc, va_list *copyto)
{
    // We guarantee to remove the function from the stack
    int argc = push_args(ls, params, args, copyto);
    if (retc == -1)
        retc = return_count(ls, params);
    lua_call_throttle strangler(this);
    int err = lua_pcall(ls, argc, retc, 0);
    set_error(err, ls);
    return (!err);
}

bool CLua::callbooleanfn(bool def, const char *fn, const char *params, ...)
{
    error.clear();
    lua_State *ls = state();
    if (!ls)
        return (def);

    int stacktop = lua_gettop(ls);

    lua_getglobal(ls, fn);
    if (!lua_isfunction(ls, -1))
    {
        lua_pop(ls, 1);
        CL_RESETSTACK_RETURN(ls, stacktop, def);
    }

    va_list args;
    va_start(args, params);
    bool ret = calltopfn(ls, params, args, 1);
    if (!ret)
        CL_RESETSTACK_RETURN(ls, stacktop, def);

    def = lua_toboolean(ls, -1);
    CL_RESETSTACK_RETURN(ls, stacktop, def);
}

bool CLua::proc_returns(const char *par) const
{
    return (strchr(par, '>') != NULL);
}

bool CLua::callfn(const char *fn, const char *params, ...)
{
    error.clear();
    lua_State *ls = state();
    if (!ls)
        return (false);

    lua_getglobal(ls, fn);
    if (!lua_isfunction(ls, -1))
    {
        lua_pop(ls, 1);
        return (false);
    }

    va_list args;
    va_list fnret;
    va_start(args, params);
    bool ret = calltopfn(ls, params, args, -1, &fnret);
    if (ret)
    {
        // If we have a > in format, gather return params now.
        if (proc_returns(params))
            vfnreturns(params, fnret);
    }
    va_end(args);
    va_end(fnret);
    return (ret);
}

bool CLua::callfn(const char *fn, int nargs, int nret)
{
    error.clear();
    lua_State *ls = state();
    if (!ls)
        return (false);

    // If a function is not provided on the stack, get the named function.
    if (fn)
    {
        lua_getglobal(ls, fn);
        if (!lua_isfunction(ls, -1))
        {
            lua_settop(ls, -nargs - 2);
            return (false);
        }

        // Slide the function in front of its args and call it.
        if (nargs)
            lua_insert(ls, -nargs - 1);
    }

    lua_call_throttle strangler(this);
    int err = lua_pcall(ls, nargs, nret, 0);
    set_error(err, ls);
    return !err;
}

// Defined in Kills.cc because the kill bindings refer to Kills.cc local
// structs
extern void luaopen_kills(lua_State *ls);

void luaopen_you(lua_State *ls);
void luaopen_item(lua_State *ls);
void luaopen_food(lua_State *ls);
void luaopen_crawl(lua_State *ls);
void luaopen_file(lua_State *ls);
void luaopen_options(lua_State *ls);
void luaopen_monsters(lua_State *ls);
void luaopen_globals(lua_State *ls);

void CLua::init_lua()
{
    if (_state)
        return;

    _state = managed_vm? lua_newstate(_clua_allocator, this) : luaL_newstate();
    if (!_state)
        end(1, false, "Unable to create Lua state.");

    lua_stack_cleaner clean(_state);

    lua_atpanic(_state, _clua_panic);

    luaopen_base(_state);
    luaopen_string(_state);
    luaopen_table(_state);
    luaopen_math(_state);

    // Open Crawl bindings
    luaopen_kills(_state);
    luaopen_you(_state);
    luaopen_item(_state);
    luaopen_food(_state);
    luaopen_crawl(_state);
    luaopen_file(_state);
    luaopen_options(_state);
    luaopen_monsters(_state);

    luaopen_globals(_state);

    load_cmacro();
    load_chooks();

    lua_register(_state, "loadfile", _clua_loadfile);
    lua_register(_state, "dofile", _clua_dofile);

    lua_register(_state, "require", _clua_require);

    execfile("clua/util.lua", true, true);

    if (managed_vm)
    {
        lua_register(_state, "pcall", _clua_guarded_pcall);
        execfile("clua/userbase.lua", true, true);
    }

    lua_pushboolean(_state, managed_vm);
    setregistry("lua_vm_is_managed");

    lua_pushlightuserdata(_state, this);
    setregistry("__clua");
}

CLua &CLua::get_vm(lua_State *ls)
{
    lua_stack_cleaner clean(ls);
    _getregistry(ls, "__clua");
    CLua *vm = util_get_userdata<CLua>(ls, -1);
    if (!vm)
        end(1, false, "Failed to find CLua for lua state %p", ls);
    return (*vm);
}

bool CLua::is_managed_vm(lua_State *ls)
{
    lua_stack_cleaner clean(ls);
    lua_pushstring(ls, "lua_vm_is_managed");
    lua_gettable(ls, LUA_REGISTRYINDEX);
    return (lua_toboolean(ls, -1));
}

void CLua::load_chooks()
{
    // All hook names must be chk_????
    static const char *c_hooks =
        "chk_startgame = { }"
        ;
    execstring(c_hooks, "base");
}

void CLua::load_cmacro()
{
    execfile("clua/macro.lua", true, true);
}

void CLua::add_shutdown_listener(lua_shutdown_listener *listener)
{
    if (std::find(shutdown_listeners.begin(), shutdown_listeners.end(),
                  listener) == shutdown_listeners.end())
        shutdown_listeners.push_back(listener);
}

void CLua::remove_shutdown_listener(lua_shutdown_listener *listener)
{
    std::vector<lua_shutdown_listener*>::iterator i =
        std::find(shutdown_listeners.begin(), shutdown_listeners.end(),
                  listener);
    if (i != shutdown_listeners.end())
        shutdown_listeners.erase(i);
}

/////////////////////////////////////////////////////////////////////

void clua_register_metatable(lua_State *ls, const char *tn,
                             const luaL_reg *lr,
                             int (*gcfn)(lua_State *ls))
{
    lua_stack_cleaner clean(ls);
    luaL_newmetatable(ls, tn);
    lua_pushstring(ls, "__index");
    lua_pushvalue(ls, -2);
    lua_settable(ls, -3);

    if (gcfn)
    {
        lua_pushstring(ls, "__gc");
        lua_pushcfunction(ls, gcfn);
        lua_settable(ls, -3);
    }

    if (lr)
    {
        luaL_openlib(ls, NULL, lr, 0);
    }
}

/////////////////////////////////////////////////////////////////////
// Bindings to get information on the player
//

static const char *transform_name()
{
    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_SPIDER:
        return "spider";
    case TRAN_BAT:
        return "bat";
    case TRAN_BLADE_HANDS:
        return "blade";
    case TRAN_STATUE:
        return "statue";
    case TRAN_ICE_BEAST:
        return "ice";
    case TRAN_DRAGON:
        return "dragon";
    case TRAN_LICH:
        return "lich";
    case TRAN_SERPENT_OF_HELL:
        return "serpent";
    case TRAN_AIR:
        return "air";
    default:
        return "";
    }
}

LUARET1(you_turn_is_over, boolean, you.turn_is_over)
LUARET1(you_name, string, you.your_name)
LUARET1(you_race, string,
        species_name(you.species, you.experience_level).c_str())
LUARET1(you_class, string, get_class_name(you.char_class))
LUARET1(you_god, string, god_name(you.religion).c_str())
LUARET1(you_good_god, boolean,
        lua_isstring(ls, 1) ? is_good_god(str_to_god(lua_tostring(ls, 1)))
        : is_good_god(you.religion))
LUARET1(you_evil_god, boolean,
        lua_isstring(ls, 1) ? is_evil_god(str_to_god(lua_tostring(ls, 1)))
        : is_evil_god(you.religion))
LUARET1(you_god_likes_butchery, boolean,
        lua_isstring(ls, 1) ?
        god_likes_butchery(str_to_god(lua_tostring(ls, 1))) :
        god_likes_butchery(you.religion))
LUARET2(you_hp, number, you.hp, you.hp_max)
LUARET2(you_mp, number, you.magic_points, you.max_magic_points)
LUARET2(you_pos, number, you.pos().x, you.pos().y)
LUARET1(you_hunger, string, hunger_level())
LUARET2(you_strength, number, you.strength, you.max_strength)
LUARET2(you_intelligence, number, you.intel, you.max_intel)
LUARET2(you_dexterity, number, you.dex, you.max_dex)
LUARET1(you_exp, number, you.experience_level)
LUARET1(you_exp_points, number, you.experience)
LUARET1(you_skill, number,
        lua_isstring(ls, 1) ? you.skills[str_to_skill(lua_tostring(ls, 1))]
                            : 0)
LUARET1(you_res_poison, number, player_res_poison(false))
LUARET1(you_res_fire, number, player_res_fire(false))
LUARET1(you_res_cold, number, player_res_cold(false))
LUARET1(you_res_draining, number, player_prot_life(false))
LUARET1(you_res_shock, number, player_res_electricity(false))
LUARET1(you_res_statdrain, number, player_sust_abil(false))
LUARET1(you_res_mutation, number, wearing_amulet(AMU_RESIST_MUTATION, false))
LUARET1(you_res_slowing, number, wearing_amulet(AMU_RESIST_SLOW, false))
LUARET1(you_gourmand, boolean, wearing_amulet(AMU_THE_GOURMAND, false))
LUARET1(you_saprovorous, number, player_mutation_level(MUT_SAPROVOROUS))
LUARET1(you_levitating, boolean, you.flight_mode() == FL_LEVITATE)
LUARET1(you_flying, boolean, you.flight_mode() == FL_FLY)
LUARET1(you_transform, string, transform_name())
LUARET1(you_where, string, level_id::current().describe().c_str())
LUARET1(you_branch, string, level_id::current().describe(false, false).c_str())
LUARET1(you_subdepth, number, level_id::current().depth)
// Increase by 1 because check happens on old level.
LUARET1(you_absdepth, number, you.your_level + 1)
LUAWRAP(you_stop_activity, interrupt_activity(AI_FORCE_INTERRUPT))
LUARET1(you_taking_stairs, boolean,
        current_delay_action() == DELAY_ASCENDING_STAIRS
        || current_delay_action() == DELAY_DESCENDING_STAIRS)
LUARET1(you_turns, number, you.num_turns)
LUARET1(you_see_grid, boolean,
        see_grid(luaL_checkint(ls, 1), luaL_checkint(ls, 2)))
LUARET1(you_see_grid_no_trans, boolean,
        see_grid_no_trans(luaL_checkint(ls, 1), luaL_checkint(ls, 2)))
LUARET1(you_can_smell, boolean, player_can_smell())
LUARET1(you_has_claws, number, you.has_claws(false))

static int _you_gold(lua_State *ls)
{
    if (lua_gettop(ls) >= 1)
    {
        ASSERT_DLUA;
        const int new_gold = luaL_checkint(ls, 1);
        const int old_gold = you.gold;
        you.gold = std::max(new_gold, 0);
        if (new_gold > old_gold)
            you.attribute[ATTR_GOLD_FOUND] += new_gold - old_gold;
        else if (old_gold > new_gold)
            you.attribute[ATTR_MISC_SPENDING] += old_gold - new_gold;
    }
    PLUARET(number, you.gold);
}

void lua_push_floor_items(lua_State *ls);
static int you_floor_items(lua_State *ls)
{
    lua_push_floor_items(ls);
    return (1);
}

static int l_you_spells(lua_State *ls)
{
    lua_newtable(ls);
    int index = 0;
    for (int i = 0; i < 52; ++i)
    {
        const spell_type spell = get_spell_by_letter( index_to_letter(i) );
        if (spell == SPELL_NO_SPELL)
            continue;

        lua_pushstring(ls, spell_title(spell));
        lua_rawseti(ls, -2, ++index);
    }
    return (1);
}

static int l_you_abils(lua_State *ls)
{
    lua_newtable(ls);

    std::vector<const char *>abils = get_ability_names();
    for (int i = 0, size = abils.size(); i < size; ++i)
    {
        lua_pushstring(ls, abils[i]);
        lua_rawseti(ls, -2, i + 1);
    }
    return (1);
}

static int you_can_consume_corpses(lua_State *ls)
{
    lua_pushboolean(ls,
                    can_ingest(OBJ_FOOD, FOOD_CHUNK, true, false, false)
                    || can_ingest(OBJ_CORPSES, CORPSE_BODY, true, false, false)
                    );
    return (1);
}

static const struct luaL_reg you_lib[] =
{
    { "turn_is_over", you_turn_is_over },
    { "turns"       , you_turns },
    { "spells"      , l_you_spells },
    { "abilities"   , l_you_abils },
    { "name"        , you_name },
    { "race"        , you_race },
    { "class"       , you_class },
    { "god"         , you_god },
    { "gold"        , _you_gold },
    { "good_god"    , you_good_god },
    { "evil_god"    , you_evil_god },
    { "hp"          , you_hp },
    { "mp"          , you_mp },
    { "pos"         , you_pos },
    { "hunger"      , you_hunger },
    { "strength"    , you_strength },
    { "intelligence", you_intelligence },
    { "dexterity"   , you_dexterity },
    { "skill"       , you_skill },
    { "xl"          , you_exp },
    { "exp"         , you_exp_points },
    { "res_poison"  , you_res_poison },
    { "res_fire"    , you_res_fire   },
    { "res_cold"    , you_res_cold   },
    { "res_draining", you_res_draining },
    { "res_shock"   , you_res_shock },
    { "res_statdrain", you_res_statdrain },
    { "res_mutation", you_res_mutation },
    { "res_slowing",  you_res_slowing },
    { "saprovorous",  you_saprovorous },
    { "gourmand",     you_gourmand },
    { "levitating",   you_levitating },
    { "flying",       you_flying },
    { "transform",    you_transform },

    { "god_likes_butchery",  you_god_likes_butchery  },
    { "can_consume_corpses", you_can_consume_corpses },

    { "stop_activity", you_stop_activity },
    { "taking_stairs", you_taking_stairs },

    { "floor_items",  you_floor_items },

    { "where",        you_where },
    { "branch",       you_branch },
    { "subdepth",     you_subdepth },
    { "absdepth",     you_absdepth },

    { "see_grid",          you_see_grid },
    { "see_grid_no_trans", you_see_grid_no_trans },
    { "can_smell",         you_can_smell },
    { "has_claws",         you_has_claws },

    { NULL, NULL },
};

void luaopen_you(lua_State *ls)
{
    luaL_openlib(ls, "you", you_lib, 0);
}

/////////////////////////////////////////////////////////////////////
// Bindings to get information on items. We must be extremely careful
// to only hand out information the player already has.
//

static const item_def *excl_item = NULL;

#define LUA_ITEM(name, n) \
    if (!lua_islightuserdata(ls, n)) \
    { \
        luaL_argerror(ls, n, "Unexpected arg type"); \
        return (0); \
    } \
      \
    item_def *name = static_cast<item_def *>( lua_touserdata(ls, n ) ); \
    if (excl_item && name != excl_item) \
    { \
        luaL_argerror(ls, n, "Unexpected item"); \
        return (0); \
    }

void lua_push_inv_items(lua_State *ls);

void lua_set_exclusive_item(const item_def *item)
{
    excl_item = item;
}

void lua_push_items(lua_State *ls, int link)
{
    lua_newtable(ls);
    int index = 0;
    for ( ; link != NON_ITEM; link = mitm[link].link)
    {
        lua_pushlightuserdata(ls, &mitm[link]);
        lua_rawseti(ls, -2, ++index);
    }
}

static int l_item_inventory(lua_State *ls)
{
    lua_push_inv_items(ls);
    return (1);
}

static int l_item_index_to_letter(lua_State *ls)
{
    int index = luaL_checkint(ls, 1);
    char sletter[2] = "?";
    if (index >= 0 && index <= ENDOFPACK)
        *sletter = index_to_letter(index);
    lua_pushstring(ls, sletter);
    return (1);
}

static int l_item_letter_to_index(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);
    if (!s || !*s || s[1])
        return (0);
    lua_pushnumber(ls, letter_to_index(*s));
    return (1);
}

static int l_item_swap_slots(lua_State *ls)
{
    int slot1 = luaL_checkint(ls, 1),
        slot2 = luaL_checkint(ls, 2);
    bool verbose = lua_toboolean(ls, 3);
    if (slot1 < 0 || slot1 >= ENDOFPACK
        || slot2 < 0 || slot2 >= ENDOFPACK
        || slot1 == slot2 || !is_valid_item(you.inv[slot1]))
    {
        return (0);
    }

    swap_inv_slots(slot1, slot2, verbose);

    return (0);
}

static int l_item_wield(lua_State *ls)
{
    if (you.turn_is_over)
        return (0);

    LUA_ITEM(item, 1);
    int slot = -1;
    if (item && is_valid_item(*item) && in_inventory(*item))
        slot = item->link;
    bool res = wield_weapon(true, slot);
    lua_pushboolean(ls, res);
    return (1);
}

static int l_item_wear(lua_State *ls)
{
    if (you.turn_is_over)
        return (0);

    LUA_ITEM(item, 1);
    if (!item || !in_inventory(*item))
        return (0);

    bool success = do_wear_armour(item->link, false);
    lua_pushboolean(ls, success);
    return (1);
}

static int l_item_puton(lua_State *ls)
{
    if (you.turn_is_over)
        return (0);

    LUA_ITEM(item, 1);
    if (!item || !in_inventory(*item))
        return (0);

    lua_pushboolean(ls, puton_ring(item->link, false));
    return (1);
}

static int l_item_remove(lua_State *ls)
{
    if (you.turn_is_over)
    {
        mpr("Turn is over");
        return (0);
    }

    LUA_ITEM(item, 1);
    if (!item || !in_inventory(*item))
    {
        mpr("Bad item");
        return (0);
    }

    int eq = get_equip_slot(item);
    if (eq < 0 || eq >= NUM_EQUIP)
    {
        mpr("Item is not equipped");
        return (0);
    }

    bool result = false;
    if (eq == EQ_WEAPON)
        result = wield_weapon(true, -1);
    else if (eq == EQ_LEFT_RING || eq == EQ_RIGHT_RING || eq == EQ_AMULET)
        result = remove_ring(item->link);
    else
        result = takeoff_armour(item->link);
    lua_pushboolean(ls, result);
    return (1);
}

static int l_item_drop(lua_State *ls)
{
    if (you.turn_is_over)
        return (0);

    LUA_ITEM(item, 1);
    if (!item || !in_inventory(*item))
        return (0);

    int eq = get_equip_slot(item);
    if (eq >= 0 && eq < NUM_EQUIP)
    {
        lua_pushboolean(ls, false);
        lua_pushstring(ls, "Can't drop worn items");
        return (2);
    }

    int qty = item->quantity;
    if (lua_isnumber(ls, 2))
    {
        int q = luaL_checkint(ls, 2);
        if (q >= 1 && q <= item->quantity)
            qty = q;
    }
    lua_pushboolean(ls, drop_item(item->link, qty));
    return (1);
}

static item_def *dmx_get_item(lua_State *ls, int ndx, int subndx)
{
    if (lua_istable(ls, ndx))
    {
        lua_rawgeti(ls, ndx, subndx);
        item_def *item = util_get_userdata<item_def>(ls, -1);
        lua_pop(ls, 1);

        return (item);
    }
    return util_get_userdata<item_def>(ls, ndx);
}

static int dmx_get_qty(lua_State *ls, int ndx, int subndx)
{
    int qty = -1;
    if (lua_istable(ls, ndx))
    {
        lua_rawgeti(ls, ndx, subndx);
        if (lua_isnumber(ls, -1))
            qty = luaL_checkint(ls, -1);
        lua_pop(ls, 1);
    }
    else if (lua_isnumber(ls, ndx))
    {
        qty = luaL_checkint(ls, ndx);
    }
    return (qty);
}

static bool l_item_pickup2(item_def *item, int qty)
{
    if (!item || in_inventory(*item))
        return (false);

    int floor_link = item_on_floor(*item, you.pos());
    if (floor_link == NON_ITEM)
        return (false);

    return pickup_single_item(floor_link, qty);
}

static int l_item_pickup(lua_State *ls)
{
    if (you.turn_is_over)
        return (0);

    if (lua_islightuserdata(ls, 1))
    {
        LUA_ITEM(item, 1);
        int qty = item->quantity;
        if (lua_isnumber(ls, 2))
            qty = luaL_checkint(ls, 2);

        if (l_item_pickup2(item, qty))
            lua_pushnumber(ls, 1);
        else
            lua_pushnil(ls);
        return (1);
    }
    else if (lua_istable(ls, 1))
    {
        int dropped = 0;
        for (int i = 1; ; ++i)
        {
            lua_rawgeti(ls, 1, i);
            item_def *item = dmx_get_item(ls, -1, 1);
            int qty = dmx_get_qty(ls, -1, 2);
            lua_pop(ls, 1);

            if (l_item_pickup2(item, qty))
                dropped++;
            else
            {
                // Yes, we bail out on first failure.
                break;
            }
        }
        if (dropped)
            lua_pushnumber(ls, dropped);
        else
            lua_pushnil(ls);
        return (1);
    }
    return (0);
}

static int l_item_equipped(lua_State *ls)
{
    LUA_ITEM(item, 1);
    if (!item || !in_inventory(*item))
        return (0);

    int eq = get_equip_slot(item);
    if (eq < 0 || eq >= NUM_EQUIP)
        return (0);

    return (1);
}

// Returns item equipped in a slot defined in an argument.
static int l_item_equipped_at(lua_State *ls)
{
    int eq = -1;
    if (lua_isnumber(ls, 1))
        eq = luaL_checkint(ls, 1);
    else if (lua_isstring(ls, 1))
    {
        const char *eqname = lua_tostring(ls, 1);
        if (!eqname)
            return (0);
        eq = equip_name_to_slot(eqname);
    }

    if (eq < 0 || eq >= NUM_EQUIP)
        return (0);

    if (you.equip[eq] != -1)
        lua_pushlightuserdata(ls, &you.inv[you.equip[eq]]);
    else
        lua_pushnil(ls);

    return (1);
}

static int l_item_class(lua_State *ls)
{
    LUA_ITEM(item, 1);
    if (item)
    {
        bool terse = false;
        if (lua_isboolean(ls, 2))
            terse = lua_toboolean(ls, 2);

        std::string s = item_class_name(item->base_type, terse);
        lua_pushstring(ls, s.c_str());
    }
    else
        lua_pushnil(ls);
    return (1);
}

// FIXME: Fold this back into itemname.cc.
static const char *ring_types[] =
{
    "regeneration",
    "protection",
    "protection from fire",
    "poison resistance",
    "protection from cold",
    "strength",
    "slaying",
    "see invisible",
    "invisibility",
    "hunger",
    "teleportation",
    "evasion",
    "sustain abilities",
    "sustenance",
    "dexterity",
    "intelligence",
    "wizardry",
    "magical power",
    "levitation",
    "life protection",
    "protection from magic",
    "fire",
    "ice",
    "teleport control"
};

static const char *amulet_types[] =
{
    "rage", "resist slowing", "clarity", "warding", "resist corrosion",
    "gourmand", "conservation", "controlled flight", "inaccuracy",
    "resist mutation"
};

static int l_item_subtype(lua_State *ls)
{
    LUA_ITEM(item, 1);
    if (item)
    {
        if (item_type_known(*item))
        {
            const char *s = NULL;
            if (item->base_type == OBJ_JEWELLERY)
            {
                if (jewellery_is_amulet(*item))
                    s = amulet_types[ item->sub_type - AMU_RAGE ];
                else
                    s = ring_types[item->sub_type];
            }
            else if (item->base_type == OBJ_POTIONS)
            {
                if (item->sub_type == POT_BLOOD)
                   s = "blood";
                else if (item->sub_type == POT_BLOOD_COAGULATED)
                   s = "coagulated blood";
                else if (item->sub_type == POT_WATER)
                   s = "water";
                else if (item->sub_type == POT_PORRIDGE)
                   s = "porridge";
                else if (item->sub_type == POT_BERSERK_RAGE)
                   s = "berserk";
                else if (item->sub_type == POT_GAIN_STRENGTH
                         || item->sub_type == POT_GAIN_DEXTERITY
                         || item->sub_type == POT_GAIN_INTELLIGENCE)
                {
                   s = "gain ability";
                }
                else if (item->sub_type == POT_CURE_MUTATION)
                   s = "cure mutation";
            }
            else if (item->base_type == OBJ_BOOKS)
            {
                if (item->sub_type == BOOK_MANUAL)
                    s = "manual";
                else
                    s = "spellbook";
            }

            if (s)
                lua_pushstring(ls, s);
            else
                lua_pushnil(ls);

            lua_pushnumber(ls, item->sub_type);
            return (2);
        }
    }

    lua_pushnil(ls);
    lua_pushnil(ls);
    return (2);
}

static int l_item_cursed(lua_State *ls)
{
    LUA_ITEM(item, 1);
    bool cursed = item && item_ident(*item, ISFLAG_KNOW_CURSE)
                       && item_cursed(*item);
    lua_pushboolean(ls, cursed);
    return (1);
}

static int l_item_worn(lua_State *ls)
{
    LUA_ITEM(item, 1);
    int worn = get_equip_slot(item);
    if (worn != -1)
        lua_pushnumber(ls, worn);
    else
        lua_pushnil(ls);
    if (worn != -1)
        lua_pushstring(ls, equip_slot_to_name(worn));
    else
        lua_pushnil(ls);
    return (2);
}

static std::string _item_name(lua_State *ls, item_def* item)
{
    description_level_type ndesc = DESC_PLAIN;
    if (lua_isstring(ls, 2))
        ndesc = description_type_by_name(lua_tostring(ls, 2));
    else if (lua_isnumber(ls, 2))
        ndesc = static_cast<description_level_type>(luaL_checkint(ls, 2));
    bool terse = lua_toboolean(ls, 3);
    return (item->name(ndesc, terse));
}

static int l_item_name(lua_State *ls)
{
    LUA_ITEM(item, 1);
    if (item)
    {
        std::string name = _item_name(ls, item);
        lua_pushstring(ls, name.c_str());
    }
    else
        lua_pushnil(ls);
    return (1);
}

static int l_item_name_coloured(lua_State *ls)
{
    LUA_ITEM(item, 1);
    if (item)
    {
        std::string name   = _item_name(ls, item);
        int         col    = menu_colour(name, menu_colour_item_prefix(*item));
        std::string colstr = colour_to_str(col);

        std::ostringstream out;

        out << "<" << colstr << ">" << name << "</" << colstr << ">";

        lua_pushstring(ls, out.str().c_str());
    }
    else
        lua_pushnil(ls);
    return (1);
}

static int l_item_quantity(lua_State *ls)
{
    LUA_ITEM(item, 1);
    lua_pushnumber(ls, item? item->quantity : 0);
    return (1);
}

static int l_item_inslot(lua_State *ls)
{
    int index = luaL_checkint(ls, 1);
    if (index >= 0 && index < 52 && is_valid_item(you.inv[index]))
        lua_pushlightuserdata(ls, &you.inv[index]);
    else
        lua_pushnil(ls);
    return (1);
}

static int l_item_slot(lua_State *ls)
{
    LUA_ITEM(item, 1);
    if (item)
    {
        int slot = in_inventory(*item) ? item->link
                                       : letter_to_index(item->slot);
        lua_pushnumber(ls, slot);
    }
    else
        lua_pushnil(ls);
    return (1);
}

static int l_item_ininventory(lua_State *ls)
{
    LUA_ITEM(item, 1);
    lua_pushboolean(ls, item && in_inventory(*item));
    return (1);
}

static int l_item_equip_type(lua_State *ls)
{
    LUA_ITEM(item, 1);
    if (!item || !is_valid_item(*item))
        return (0);

    equipment_type eq = EQ_NONE;

    if (item->base_type == OBJ_WEAPONS || item->base_type == OBJ_STAVES)
        eq = EQ_WEAPON;
    else if (item->base_type == OBJ_ARMOUR)
        eq = get_armour_slot(*item);
    else if (item->base_type == OBJ_JEWELLERY)
        eq = item->sub_type >= AMU_RAGE? EQ_AMULET : EQ_RINGS;

    if (eq != EQ_NONE)
    {
        lua_pushnumber(ls, eq);
        lua_pushstring(ls, equip_slot_to_name(eq));
    }
    else
    {
        lua_pushnil(ls);
        lua_pushnil(ls);
    }
    return (2);
}

static int l_item_weap_skill(lua_State *ls)
{
    LUA_ITEM(item, 1);
    if (!item || !is_valid_item(*item))
        return (0);

    int skill = range_skill(*item);
    if (skill == SK_THROWING)
        skill = weapon_skill(*item);
    if (skill == SK_FIGHTING)
        return (0);

    lua_pushstring(ls, skill_name(skill));
    lua_pushnumber(ls, skill);
    return (2);
}

static int l_item_dropped(lua_State *ls)
{
    LUA_ITEM(item, 1);
    if (!item || !is_valid_item(*item))
        return (0);

    lua_pushboolean(ls, item->flags & ISFLAG_DROPPED);

    return (1);
}

static int l_item_can_cut_meat(lua_State *ls)
{
    LUA_ITEM(item, 1);
    if (!item || !is_valid_item(*item))
        return (0);

    lua_pushboolean(ls, can_cut_meat(*item));

    return (1);
}

static int l_item_artefact(lua_State *ls)
{
    LUA_ITEM(item, 1);
    if (!item || !is_valid_item(*item))
        return (0);

    lua_pushboolean(ls, item_ident(*item, ISFLAG_KNOW_PROPERTIES)
                && (is_random_artefact(*item) || is_fixed_artefact(*item)));
    return (1);
}

static int l_item_branded(lua_State *ls)
{
    LUA_ITEM(item, 1);
    if (!item || !is_valid_item(*item) || !item_type_known(*item))
        return (0);

    bool branded = false;
    switch (item->base_type)
    {
    case OBJ_WEAPONS:
        branded = get_weapon_brand(*item) != SPWPN_NORMAL;
        break;
    case OBJ_ARMOUR:
        branded = get_armour_ego_type(*item) != SPARM_NORMAL;
        break;
    case OBJ_MISSILES:
        branded = get_ammo_brand(*item) != SPMSL_NORMAL;
        break;
    default:
        break;
    }
    lua_pushboolean(ls, branded);
    return (1);
}

static const struct luaL_reg item_lib[] =
{
    { "artefact",          l_item_artefact },
    { "branded",           l_item_branded },
    { "class",             l_item_class },
    { "subtype",           l_item_subtype },
    { "cursed",            l_item_cursed },
    { "worn",              l_item_worn },
    { "name",              l_item_name },
    { "name_coloured",     l_item_name_coloured },
    { "quantity",          l_item_quantity },
    { "inslot",            l_item_inslot },
    { "slot",              l_item_slot },
    { "ininventory",       l_item_ininventory },
    { "inventory",         l_item_inventory },
    { "letter_to_index",   l_item_letter_to_index },
    { "index_to_letter",   l_item_index_to_letter },
    { "swap_slots",        l_item_swap_slots },
    { "wield",             l_item_wield },
    { "wear",              l_item_wear },
    { "puton",             l_item_puton },
    { "remove",            l_item_remove },
    { "drop",              l_item_drop },
    { "pickup",            l_item_pickup },
    { "equipped_at",       l_item_equipped_at },
    { "equipped",          l_item_equipped },
    { "equip_type",        l_item_equip_type },
    { "weap_skill",        l_item_weap_skill },
    { "dropped",           l_item_dropped },
    { "can_cut_meat",      l_item_can_cut_meat },

    { NULL, NULL },
};

void luaopen_item(lua_State *ls)
{
    luaL_openlib(ls, "item", item_lib, 0);
}

/////////////////////////////////////////////////////////////////////
// Food information.

static int food_do_eat(lua_State *ls)
{
    bool eaten = false;
    if (!you.turn_is_over)
        eaten = eat_food(-1);
    lua_pushboolean(ls, eaten);
    return (1);
}

static int food_prompt_eat_chunks(lua_State *ls)
{
    int eaten = 0;
    if (!you.turn_is_over)
        eaten = prompt_eat_chunks();

    lua_pushboolean(ls, (eaten != 0));
    return (1);
}

static int food_prompt_floor(lua_State *ls)
{
    int eaten = 0;
    if (!you.turn_is_over)
    {
        eaten = eat_from_floor();
        if (eaten == 1)
            burden_change();
    }
    lua_pushboolean(ls, (eaten != 0));
    return (1);
}

static int food_prompt_inventory(lua_State *ls)
{
    bool eaten = false;
    if (!you.turn_is_over)
        eaten = eat_from_inventory();
    lua_pushboolean(ls, eaten);
    return (1);
}

static int food_prompt_inventory_menu(lua_State *ls)
{
    bool eaten = false;
    if (!you.turn_is_over)
        eaten = prompt_eat_inventory_item();
    lua_pushboolean(ls, eaten);
    return (1);
}

static int food_can_eat(lua_State *ls)
{
    LUA_ITEM(item, 1);
    bool hungercheck = true;

    if (lua_isboolean(ls, 2))
        hungercheck = lua_toboolean(ls, 2);

    bool edible = item && can_ingest(item->base_type,
                                     item->sub_type,
                                     true,
                                     true,
                                     hungercheck);
    lua_pushboolean(ls, edible);
    return (1);
}

static bool eat_item(const item_def &item)
{
    if (in_inventory(item))
    {
        eat_inventory_item(item.link);
        return (true);
    }
    else
    {
        int ilink = item_on_floor(item, you.pos());

        if (ilink != NON_ITEM)
        {
            eat_floor_item(ilink);
            return (true);
        }
        return (false);
    }
}

static int food_eat(lua_State *ls)
{
    LUA_ITEM(item, 1);

    bool eaten = false;
    if (!you.turn_is_over)
    {
        // When we get down to eating, we don't care if the eating is courtesy
        // an un-ided amulet of the gourmand.
        bool edible = item && can_ingest(item->base_type,
                                         item->sub_type,
                                         false,
                                         false);
        if (edible)
            eaten = eat_item(*item);
    }
    lua_pushboolean(ls, eaten);
    return (1);
}

static int food_rotting(lua_State *ls)
{
    LUA_ITEM(item, 1);

    bool rotting = false;
    if (item && item->base_type == OBJ_FOOD && item->sub_type == FOOD_CHUNK)
        rotting = food_is_rotten(*item);

    lua_pushboolean(ls, rotting);
    return (1);
}

static int food_dangerous(lua_State *ls)
{
    LUA_ITEM(item, 1);

    bool dangerous = false;
    if (item)
    {
        dangerous = (is_poisonous(*item) || is_mutagenic(*item)
                     || causes_rot(*item) || is_forbidden_food(*item));
    }
    lua_pushboolean(ls, dangerous);
    return (1);
}

static int food_ischunk(lua_State *ls)
{
    LUA_ITEM(item, 1);
    lua_pushboolean(ls,
            item && item->base_type == OBJ_FOOD
                 && item->sub_type == FOOD_CHUNK);
    return (1);
}

static const struct luaL_reg food_lib[] =
{
    { "do_eat",            food_do_eat },
    { "prompt_eat_chunks", food_prompt_eat_chunks },
    { "prompt_floor",      food_prompt_floor },
    { "prompt_inventory",  food_prompt_inventory },
    { "prompt_inv_menu",   food_prompt_inventory_menu },
    { "can_eat",           food_can_eat },
    { "eat",               food_eat },
    { "rotting",           food_rotting },
    { "dangerous",         food_dangerous },
    { "ischunk",           food_ischunk },
    { NULL, NULL },
};

void luaopen_food(lua_State *ls)
{
    luaL_openlib(ls, "food", food_lib, 0);
}

/////////////////////////////////////////////////////////////////////
// General game bindings.
//

static int crawl_mpr(lua_State *ls)
{
    if (!crawl_state.io_inited)
        return (0);

    const char *message = luaL_checkstring(ls, 1);
    if (!message)
        return (0);

    int ch = MSGCH_PLAIN;
    if (lua_isnumber(ls, 2))
        ch = luaL_checkint(ls, 2);
    else
    {
        const char *channel = lua_tostring(ls, 2);
        if (channel)
            ch = str_to_channel(channel);
    }

    if (ch < 0 || ch >= NUM_MESSAGE_CHANNELS)
        ch = MSGCH_PLAIN;

    mpr(message, static_cast<msg_channel_type>(ch));
    return (0);
}

static int crawl_formatted_mpr(lua_State *ls)
{
    if (!crawl_state.io_inited)
        return (0);

    const char *message = luaL_checkstring(ls, 1);
    if (!message)
        return (0);

    int ch = MSGCH_PLAIN;
    if (lua_isnumber(ls, 2))
        ch = luaL_checkint(ls, 2);
    else
    {
        const char *channel = lua_tostring(ls, 2);
        if (channel)
            ch = str_to_channel(channel);
    }

    if (ch < 0 || ch >= NUM_MESSAGE_CHANNELS)
        ch = MSGCH_PLAIN;

    formatted_mpr(formatted_string::parse_string(message),
                  static_cast<msg_channel_type>(ch));
    return (0);
}

LUAWRAP(crawl_mesclr, mesclr())
LUAWRAP(crawl_redraw_screen, redraw_screen())

static int crawl_input_line(lua_State *ls)
{
    // This is arbitrary, but anybody entering so many characters is psychotic.
    char linebuf[500];

    get_input_line(linebuf, sizeof linebuf);
    lua_pushstring(ls, linebuf);
    return (1);
}

static int crawl_c_input_line(lua_State *ls)
{
    char linebuf[500];

    bool valid = !cancelable_get_line(linebuf, sizeof linebuf);
    if (valid)
        lua_pushstring(ls, linebuf);
    else
        lua_pushnil(ls);
    return (1);
}

LUARET1(crawl_getch, number, getch())
LUARET1(crawl_kbhit, number, kbhit())
LUAWRAP(crawl_flush_input, flush_input_buffer(FLUSH_LUA))

static char _lua_char(lua_State *ls, int ndx, char defval = 0)
{
    return (lua_isnone(ls, ndx) || !lua_isstring(ls, ndx)? defval
            : lua_tostring(ls, ndx)[0]);
}

static int crawl_yesno(lua_State *ls)
{
    const char *prompt = luaL_checkstring(ls, 1);
    const bool safe = lua_toboolean(ls, 2);
    const int safeanswer = _lua_char(ls, 3);
    const bool clear_after =
        lua_isnone(ls, 4) ? true : lua_toboolean(ls, 4);
    const bool interrupt_delays =
        lua_isnone(ls, 5) ? true : lua_toboolean(ls, 5);
    const bool noprompt =
        lua_isnone(ls, 6) ? false : lua_toboolean(ls, 6);

    cursor_control con(true);
    lua_pushboolean(ls, yesno(prompt, safe, safeanswer, clear_after,
                              interrupt_delays, noprompt));
    return (1);
}

static int crawl_yesnoquit(lua_State *ls)
{
    const char *prompt = luaL_checkstring(ls, 1);
    const bool safe = lua_toboolean(ls, 2);
    const int safeanswer = _lua_char(ls, 3);
    const bool allow_all =
        lua_isnone(ls, 4) ? false : lua_toboolean(ls, 4);
    const bool clear_after =
        lua_isnone(ls, 5) ? true : lua_toboolean(ls, 5);

    // Skipping the other params until somebody needs them.

    cursor_control con(true);
    lua_pushnumber(ls, yesnoquit(prompt, safe, safeanswer, allow_all,
                                 clear_after));
    return (1);
}

static void crawl_sendkeys_proc(lua_State *ls, int argi)
{
    if (lua_isstring(ls, argi))
    {
        const char *keys = luaL_checkstring(ls, argi);
        if (!keys)
            return;

        for ( ; *keys; ++keys)
            macro_buf_add(*keys);
    }
    else if (lua_istable(ls, argi))
    {
        for (int i = 1; ; ++i)
        {
            lua_rawgeti(ls, argi, i);
            if (lua_isnil(ls, -1))
            {
                lua_pop(ls, 1);
                return;
            }

            crawl_sendkeys_proc(ls, lua_gettop(ls));
            lua_pop(ls, 1);
        }
    }
    else if (lua_isnumber(ls, argi))
    {
        int key = luaL_checkint(ls, argi);
        macro_buf_add(key);
    }
}

static int crawl_sendkeys(lua_State *ls)
{
    int top = lua_gettop(ls);
    for (int i = 1; i <= top; ++i)
        crawl_sendkeys_proc(ls, i);
    return (0);
}

// Tell Crawl to process one command.
static int crawl_process_command(lua_State *ls)
{
    const bool will_process =
        current_delay_action() == DELAY_MACRO || !you_are_delayed();

    if (will_process)
    {
        // This should only be called from a macro delay, but run_macro
        // may not have started the macro delay; do so now.
        if (!you_are_delayed())
            start_delay(DELAY_MACRO, 1);
        start_delay(DELAY_MACRO_PROCESS_KEY, 1);
    }

    lua_pushboolean(ls, will_process);
    return (1);
}

static int crawl_playsound(lua_State *ls)
{
    const char *sf = luaL_checkstring(ls, 1);
    if (!sf)
        return (0);
    play_sound(sf);
    return (0);
}

static int crawl_runmacro(lua_State *ls)
{
    const char *macroname = luaL_checkstring(ls, 1);
    if (!macroname)
        return (0);
    run_macro(macroname);
    return (0);
}

static int crawl_setopt(lua_State *ls)
{
    if (!lua_isstring(ls, 1))
        return (0);

    const char *s = lua_tostring(ls, 1);
    if (s)
    {
        // Note that the conditional script can contain nested Lua[ ]Lua code.
        read_options(s, true);
    }

    return (0);
}

static int crawl_read_options(lua_State *ls)
{
    if (!lua_isstring(ls, 1))
        return (0);

    const char* filename = lua_tostring(ls, 1);
    Options.include(filename, true, true);
    return (0);
}

static int crawl_bindkey(lua_State *ls)
{
    const char *s = NULL;
    if (lua_isstring(ls, 1))
    {
        s = lua_tostring(ls, 1);
    }

    if (!s || !lua_isfunction(ls, 2) || lua_gettop(ls) != 2)
        return (0);

    lua_pushvalue(ls, 2);
    std::string name = clua.setuniqregistry();
    if (lua_gettop(ls) != 2)
    {
        fprintf(stderr, "Stack top has changed!\n");
        lua_settop(ls, 2);
    }
    macro_userfn(s, name.c_str());
    return (0);
}

static int crawl_msgch_num(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);
    if (!s)
        return (0);
    int ch = str_to_channel(s);
    if (ch == -1)
        return (0);

    lua_pushnumber(ls, ch);
    return (1);
}

static int crawl_msgch_name(lua_State *ls)
{
    int num = luaL_checkint(ls, 1);
    std::string name = channel_to_str(num);
    lua_pushstring(ls, name.c_str());
    return (1);
}

static int crawl_take_note(lua_State *ls)
{
    const char* msg = luaL_checkstring(ls, 1);
    take_note(Note(NOTE_MESSAGE, 0, 0, msg));
    return (0);
}

#define REGEX_METATABLE "crawl.regex"
#define MESSF_METATABLE "crawl.messf"

static int crawl_regex(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);
    if (!s)
        return (0);


    text_pattern **tpudata =
            clua_new_userdata< text_pattern* >(ls, REGEX_METATABLE);
    if (tpudata)
    {
        *tpudata = new text_pattern(s);
        return (1);
    }
    return (0);
}

static int crawl_regex_find(lua_State *ls)
{
    text_pattern **pattern =
            clua_get_userdata< text_pattern* >(ls, REGEX_METATABLE);
    if (!pattern)
        return (0);

    const char *text = luaL_checkstring(ls, -1);
    if (!text)
        return (0);

    lua_pushboolean(ls, (*pattern)->matches(text));
    return (1);
}

static const luaL_reg crawl_regex_ops[] =
{
    { "matches",        crawl_regex_find },
    { NULL, NULL }
};

static int crawl_message_filter(lua_State *ls)
{
    const char *pattern = luaL_checkstring(ls, 1);
    if (!pattern)
        return (0);

    int num = lua_isnumber(ls, 2)? luaL_checkint(ls, 2) : -1;
    message_filter **mf =
            clua_new_userdata< message_filter* >( ls, MESSF_METATABLE );
    if (mf)
    {
        *mf = new message_filter( num, pattern );
        return (1);
    }
    return (0);
}

static int crawl_messf_matches(lua_State *ls)
{
    message_filter **mf =
            clua_get_userdata< message_filter* >(ls, MESSF_METATABLE);
    if (!mf)
        return (0);

    const char *pattern = luaL_checkstring(ls, 2);
    int ch = luaL_checkint(ls, 3);
    if (pattern)
    {
        bool filt = (*mf)->is_filtered(ch, pattern);
        lua_pushboolean(ls, filt);
        return (1);
    }
    return (0);
}

static const luaL_reg crawl_messf_ops[] =
{
    { "matches",        crawl_messf_matches },
    { NULL, NULL }
};

static int crawl_trim(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);
    if (!s)
        return (0);
    std::string text = s;
    trim_string(text);
    lua_pushstring(ls, text.c_str());
    return (1);
}

static int crawl_split(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1),
               *token = luaL_checkstring(ls, 2);
    if (!s || !token)
        return (0);

    std::vector<std::string> segs = split_string(token, s);
    lua_newtable(ls);
    for (int i = 0, count = segs.size(); i < count; ++i)
    {
        lua_pushstring(ls, segs[i].c_str());
        lua_rawseti(ls, -2, i + 1);
    }
    return (1);
}

static int _crawl_grammar(lua_State *ls)
{
    description_level_type ndesc = DESC_PLAIN;
    if (lua_isstring(ls, 2))
        ndesc = description_type_by_name(lua_tostring(ls, 2));
    PLUARET(string,
            thing_do_grammar(ndesc, false,
                             false, luaL_checkstring(ls, 1)).c_str());
}

static int crawl_article_a(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);

    bool lowercase = true;
    if (lua_isboolean(ls, 2))
        lowercase = lua_toboolean(ls, 2);

    lua_pushstring(ls, article_a(s, lowercase).c_str());

    return (1);
}

LUARET1(crawl_game_started, boolean, crawl_state.need_save)
LUARET1(crawl_random2, number, random2( luaL_checkint(ls, 1) ))
LUARET1(crawl_one_chance_in, boolean, one_chance_in( luaL_checkint(ls, 1) ))
LUARET1(crawl_random2avg, number,
        random2avg( luaL_checkint(ls, 1), luaL_checkint(ls, 2) ))
LUARET1(crawl_random_range, number,
        random_range( luaL_checkint(ls, 1), luaL_checkint(ls, 2),
                      lua_isnumber(ls, 3)? luaL_checkint(ls, 3) : 1 ))
LUARET1(crawl_coinflip, boolean, coinflip())
LUARET1(crawl_roll_dice, number,
        lua_gettop(ls) == 1
        ? roll_dice( 1, luaL_checkint(ls, 1) )
        : roll_dice( luaL_checkint(ls, 1), luaL_checkint(ls, 2) ))

static int crawl_random_element(lua_State *ls)
{
    const int table_idx = 1;
    const int value_idx = 2;

    if (lua_gettop(ls) == 0)
    {
        lua_pushnil(ls);
        return 1;
    }

    // Only the first arg does anything now.  Maybe this should
    // select from a variable number of table args?
    lua_pop(ls, lua_gettop(ls) - 1);

    // Keep max value on the stack, as it could be any type of value.
    lua_pushnil(ls);
    int rollsize = 0;

    lua_pushnil(ls);
    while (lua_next(ls, table_idx) != 0)
    {
        const int weight_idx = -1;
        const int key_idx = -2;

        int this_weight = lua_isnil(ls, weight_idx) ?
            1 : (int)lua_tonumber(ls, weight_idx);

        if (rollsize > 0)
        {
            rollsize += this_weight;
            if (x_chance_in_y(this_weight, rollsize))
            {
                lua_pushvalue(ls, key_idx);
                lua_replace(ls, value_idx);
            }
        }
        else
        {
            lua_pushvalue(ls, key_idx);
            lua_replace(ls, value_idx);
            rollsize = this_weight;
        }

        lua_pop(ls, 1);
    }

    lua_pushvalue(ls, value_idx);

    return 1;
}

static int crawl_err_trace(lua_State *ls)
{
    const int nargs = lua_gettop(ls);
    const int err = lua_pcall(ls, nargs - 1, LUA_MULTRET, 0);

    if (err)
    {
        // This code from lua.c:traceback() (mostly)
        const char *errs = lua_tostring(ls, 1);
        std::string errstr = errs? errs : "";
        lua_getfield(ls, LUA_GLOBALSINDEX, "debug");
        if (!lua_istable(ls, -1))
        {
            lua_pop(ls, 1);
            return lua_error(ls);
        }
        lua_getfield(ls, -1, "traceback");
        if (!lua_isfunction(ls, -1))
        {
            lua_pop(ls, 2);
            return lua_error(ls);
        }
        lua_pushvalue(ls, 1);
        lua_pushinteger(ls, 2); // Skip crawl_err_trace and traceback.
        lua_call(ls, 2, 1);

        // What's on top should be the error.
        lua_error(ls);
    }

    return (lua_gettop(ls));
}

static const struct luaL_reg crawl_lib[] =
{
    { "mpr",            crawl_mpr },
    { "formatted_mpr",  crawl_formatted_mpr },
    { "mesclr",         crawl_mesclr },
    { "random2",        crawl_random2 },
    { "one_chance_in",  crawl_one_chance_in },
    { "random2avg"   ,  crawl_random2avg },
    { "coinflip",       crawl_coinflip },
    { "roll_dice",      crawl_roll_dice },
    { "random_range",   crawl_random_range },
    { "random_element", crawl_random_element },
    { "redraw_screen",  crawl_redraw_screen },
    { "input_line",     crawl_input_line },
    { "c_input_line",   crawl_c_input_line},
    { "getch",          crawl_getch },
    { "yesno",          crawl_yesno },
    { "yesnoquit",      crawl_yesnoquit },
    { "kbhit",          crawl_kbhit },
    { "flush_input",    crawl_flush_input },
    { "sendkeys",       crawl_sendkeys },
    { "process_command", crawl_process_command },
    { "playsound",      crawl_playsound },
    { "runmacro",       crawl_runmacro },
    { "bindkey",        crawl_bindkey },
    { "setopt",         crawl_setopt },
    { "read_options",   crawl_read_options },
    { "msgch_num",      crawl_msgch_num },
    { "msgch_name",     crawl_msgch_name },
    { "take_note",      crawl_take_note },

    { "regex",          crawl_regex },
    { "message_filter", crawl_message_filter },
    { "trim",           crawl_trim },
    { "split",          crawl_split },
    { "grammar",        _crawl_grammar },
    { "article_a",      crawl_article_a },
    { "game_started",   crawl_game_started },
    { "err_trace",      crawl_err_trace },

    { NULL, NULL },
};

void luaopen_crawl(lua_State *ls)
{
    clua_register_metatable(ls, REGEX_METATABLE, crawl_regex_ops,
                            lua_object_gc<text_pattern>);
    clua_register_metatable(ls, MESSF_METATABLE, crawl_messf_ops,
                            lua_object_gc<message_filter>);

    luaL_openlib(ls, "crawl", crawl_lib, 0);
}

///////////////////////////////////////////////////////////
// File operations

static const struct luaL_reg file_lib[] =
{
    { "write", CLua::file_write },
    { NULL, NULL },
};

void luaopen_file(lua_State *ls)
{
    luaL_openlib(ls, "file", file_lib, 0);
}


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
        lua_pushboolean(ls, *static_cast<bool*>( data ));
        return (1);
    }
    else
    {
        if (lua_isboolean(ls, 3))
            *static_cast<bool*>( data ) = lua_toboolean(ls, 3);
        return (0);
    }
}

static option_handler handlers[] =
{
    // Boolean options come first
    { "easy_open",     &Options.easy_open, option_hboolean },
    { "colour_map",    &Options.colour_map, option_hboolean },
    { "clean_map",     &Options.clean_map, option_hboolean },
    { "show_uncursed", &Options.show_uncursed, option_hboolean },
    { "easy_open",     &Options.easy_open, option_hboolean },
    { "easy_armour",   &Options.easy_unequip, option_hboolean },
    { "easy_unequip",  &Options.easy_unequip, option_hboolean },
    { "easy_butcher",  &Options.easy_butcher, option_hboolean },
    { "always_confirm_butcher", &Options.always_confirm_butcher, option_hboolean },
    { "default_target",       &Options.default_target, option_hboolean },
    { "autopickup_no_burden", &Options.autopickup_no_burden, option_hboolean },
    { "note_skill_max",       &Options.note_skill_max, option_hboolean },
    { "delay_message_clear",  &Options.delay_message_clear, option_hboolean },
    { "no_dark_brand",   &Options.no_dark_brand, option_hboolean },
    { "auto_list",       &Options.auto_list, option_hboolean },
    { "pickup_thrown",   &Options.pickup_thrown, option_hboolean },
    { "pickup_dropped",  &Options.pickup_dropped, option_hboolean },
    { "show_waypoints",  &Options.show_waypoints, option_hboolean },
    { "item_colour",     &Options.item_colour, option_hboolean },
    { "target_zero_exp", &Options.target_zero_exp, option_hboolean },
    { "target_wrap",     &Options.target_wrap, option_hboolean },
    { "easy_exit_menu",  &Options.easy_exit_menu, option_hboolean },
    { "dos_use_background_intensity", &Options.dos_use_background_intensity,
                                      option_hboolean },
    { "menu_colour_prefix_class", &Options.menu_colour_prefix_class,
                                  option_hboolean }
};

static const option_handler *get_handler(const char *optname)
{
    if (optname)
    {
        for (int i = 0, count = sizeof(handlers) / sizeof(*handlers);
             i < count; ++i)
        {
            if (!strcmp(handlers[i].option, optname))
                return &handlers[i];
        }
    }
    return (NULL);
}

static int option_get(lua_State *ls)
{
    const char *opt = luaL_checkstring(ls, 2);
    if (!opt)
        return (0);

    // Is this a Lua named option?
    game_options::opt_map::iterator i = Options.named_options.find(opt);
    if (i != Options.named_options.end())
    {
        const std::string &ov = i->second;
        lua_pushstring(ls, ov.c_str());
        return (1);
    }

    const option_handler *oh = get_handler(opt);
    if (oh)
        return (oh->handler(ls, opt, oh->data, true));

    return (0);
}

static int option_set(lua_State *ls)
{
    const char *opt = luaL_checkstring(ls, 2);
    if (!opt)
        return (0);

    const option_handler *oh = get_handler(opt);
    if (oh)
        oh->handler(ls, opt, oh->data, false);

    return (0);
}

#define OPT_METATABLE "clua_metatable_optaccess"
void luaopen_options(lua_State *ls)
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
    int *dummy = static_cast<int *>( lua_newuserdata(ls, sizeof(int)) );
    // Mystic number
    *dummy = 42;

    luaL_getmetatable(ls, OPT_METATABLE);
    lua_setmetatable(ls, -2);
    lua_setglobal(ls, "options");
}

/////////////////////////////////////////////////////////////////////
// Monster handling

#define MONS_METATABLE "monster.monsaccess"
struct MonsterWrap
{
    monsters *mons;
    long      turn;
};

#define MDEF(name)                                                      \
    static int l_mons_##name(lua_State *ls, monsters *mons,             \
                             const char *attr)                         \

#define MDEFN(name, closure)                    \
    static int l_mons_##name(lua_State *ls, monsters *mons, const char *attrs) \
    {                                                                   \
    lua_pushlightuserdata(ls, mons);                                    \
    lua_pushcclosure(ls, l_mons_do_dismiss, 1);                         \
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
    "nothing", "eats_items", "open_doors", "starting_equipment",
    "weapons_armour", "magic_items"
};

static const char *_monuse_to_str(mon_itemuse_type ityp)
{
    ASSERT(ARRAYSZ(_monuse_names) == NUM_MONUSE);
    return _monuse_names[ityp];
}

MDEF(muse)
{
    if (const monsterentry *me = mons->find_monsterentry())
    {
        PLUARET(string, _monuse_to_str(me->gmon_use));
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
    { "dismiss", l_mons_dismiss },
    { "experience", l_mons_experience },
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

// We currently permit no set operations on monsters
static int monster_set(lua_State *ls)
{
    return (0);
}

static int push_activity_interrupt(lua_State *ls, activity_interrupt_data *t)
{
    if (!t->data)
    {
        lua_pushnil(ls);
        return 0;
    }

    switch (t->apt)
    {
    case AIP_HP_LOSS:
        {
            const ait_hp_loss *ahl = (const ait_hp_loss *) t->data;
            lua_pushnumber(ls, ahl->hp);
            lua_pushnumber(ls, ahl->hurt_type);
            return 1;
        }
    case AIP_INT:
        lua_pushnumber(ls, *(const int *) t->data);
        break;
    case AIP_STRING:
        lua_pushstring(ls, (const char *) t->data);
        break;
    case AIP_MONSTER:
        // FIXME: We're casting away the const...
        push_monster(ls, (monsters *) t->data);
        break;
    default:
        lua_pushnil(ls);
        break;
    }
    return 0;
}

void push_monster(lua_State *ls, monsters *mons)
{
    MonsterWrap *mw = clua_new_userdata< MonsterWrap >(ls, MONS_METATABLE);
    mw->turn = you.num_turns;
    mw->mons = mons;
}

void clua_push_map(lua_State *ls, map_def *map)
{
    map_def **mapref = clua_new_userdata<map_def *>(ls, MAP_METATABLE);
    *mapref = map;
}

void clua_push_coord(lua_State *ls, const coord_def &c)
{
    lua_pushnumber(ls, c.x);
    lua_pushnumber(ls, c.y);
}

void clua_push_dgn_event(lua_State *ls, const dgn_event *devent)
{
    const dgn_event **de =
        clua_new_userdata<const dgn_event *>(ls, DEVENT_METATABLE);
    *de = devent;
}

void luaopen_monsters(lua_State *ls)
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
}

//////////////////////////////////////////////////////////////////////
// Miscellaneous globals

#define PATTERN_FLUSH_CEILING 100

typedef std::map<std::string, text_pattern> pattern_map;
static pattern_map pattern_cache;

static text_pattern &get_text_pattern(const std::string &s, bool checkcase)
{
    pattern_map::iterator i = pattern_cache.find(s);
    if (i != pattern_cache.end())
        return i->second;

    if (pattern_cache.size() > PATTERN_FLUSH_CEILING)
        pattern_cache.clear();

    pattern_cache[s] = text_pattern(s, !checkcase);
    return (pattern_cache[s]);
}

static int lua_pmatch(lua_State *ls)
{
    const char *pattern = luaL_checkstring(ls, 1);
    if (!pattern)
        return (0);

    const char *text = luaL_checkstring(ls, 2);
    if (!text)
        return (0);

    bool checkcase = true;
    if (lua_isboolean(ls, 3))
        checkcase = lua_toboolean(ls, 3);

    text_pattern &tp = get_text_pattern(pattern, checkcase);
    lua_pushboolean( ls, tp.matches(text) );
    return (1);
}

void luaopen_globals(lua_State *ls)
{
    lua_pushcfunction(ls, lua_pmatch);
    lua_setglobal(ls, "pmatch");
}

////////////////////////////////////////////////////////////////////////
// lua_text_pattern

// We could simplify this a great deal by just using lex and yacc, but I
// don't know if we want to introduce them.

struct lua_pat_op
{
    const char *token;
    const char *luatok;

    bool pretext;       // Does this follow a pattern?
    bool posttext;      // Is this followed by a pattern?
};

static lua_pat_op pat_ops[] =
{
    { "<<", " ( ",   false, true },
    { ">>", " ) ",   true,  false },
    { "!!", " not ", false, true },
    { "==", " == ",  true,  true },
    { "^^", " ~= ",  true,  true },
    { "&&", " and ", true,  true },
    { "||", " or ",  true,  true },
};

unsigned long lua_text_pattern::lfndx = 0L;

bool lua_text_pattern::is_lua_pattern(const std::string &s)
{
    for (int i = 0, size = sizeof(pat_ops) / sizeof(*pat_ops);
            i < size; ++i)
    {
        if (s.find(pat_ops[i].token) != std::string::npos)
            return (true);
    }
    return (false);
}

lua_text_pattern::lua_text_pattern(const std::string &_pattern)
    : translated(false), isvalid(true), pattern(_pattern), lua_fn_name()
{
    lua_fn_name = new_fn_name();
}

lua_text_pattern::~lua_text_pattern()
{
    if (translated && !lua_fn_name.empty())
    {
        lua_State *ls = clua;
        if (ls)
        {
            lua_pushnil(ls);
            clua.setglobal(lua_fn_name.c_str());
        }
    }
}

bool lua_text_pattern::valid() const
{
    return translated? isvalid : translate();
}

bool lua_text_pattern::matches( const std::string &s ) const
{
    if (isvalid && !translated)
        translate();

    if (!isvalid)
        return (false);

    return clua.callbooleanfn(false, lua_fn_name.c_str(), "s", s.c_str());
}

void lua_text_pattern::pre_pattern(std::string &pat, std::string &fn) const
{
    // Trim trailing spaces
    pat.erase( pat.find_last_not_of(" \t\n\r") + 1 );

    fn += " pmatch([[";
    fn += pat;
    fn += "]], text, false) ";

    pat.clear();
}

void lua_text_pattern::post_pattern(std::string &pat, std::string &fn) const
{
    pat.erase( 0, pat.find_first_not_of(" \t\n\r") );

    fn += " pmatch([[";
    fn += pat;
    fn += "]], text, false) ";

    pat.clear();
}

std::string lua_text_pattern::new_fn_name()
{
    char buf[100];
    snprintf(buf, sizeof buf, "__ch_stash_search_%lu", lfndx++);
    return (buf);
}

bool lua_text_pattern::translate() const
{
    if (translated || !isvalid)
        return (false);

    if (pattern.find("]]") != std::string::npos
        || pattern.find("[[") != std::string::npos)
    {
        return (false);
    }

    std::string textp;
    std::string luafn;
    const lua_pat_op *currop = NULL;
    for (std::string::size_type i = 0; i < pattern.length(); ++i)
    {
        bool match = false;
        for (unsigned p = 0; p < sizeof pat_ops / sizeof *pat_ops; ++p)
        {
            const lua_pat_op &lop = pat_ops[p];
            if (pattern.find(lop.token, i) == i)
            {
                match = true;
                if (lop.pretext && (!currop || currop->posttext))
                {
                    if (currop)
                        textp.erase(0, textp.find_first_not_of(" \r\n\t"));
                    pre_pattern(textp, luafn);
                }

                currop = &lop;
                luafn += lop.luatok;

                i += strlen( lop.token ) - 1;

                break;
            }
        }

        if (match)
            continue;

        textp += pattern[i];
    }

    if (currop && currop->posttext)
        post_pattern(textp, luafn);

    luafn = "function " + lua_fn_name + "(text) return " + luafn + " end";

    const_cast<lua_text_pattern *>(this)->translated = true;

    int err = clua.execstring( luafn.c_str(), "stash-search" );
    if (err)
    {
        lua_text_pattern *self = const_cast<lua_text_pattern *>(this);
        self->isvalid = self->translated = false;
    }

    return translated;
}

//////////////////////////////////////////////////////////////////////////

lua_call_throttle::lua_clua_map lua_call_throttle::lua_map;

// A panic function for the Lua interpreter, usually called when it
// runs out of memory when trying to load a file or a chunk of Lua from
// an unprotected Lua op. The only cases of unprotected Lua loads are
// loads of Lua code from .crawlrc, which is read at start of game.
//
// If there's an inordinately large .crawlrc (we're talking seriously
// massive here) that wants more memory than we're willing to give
// Lua, then the game will save and exit until the .crawlrc is fixed.
//
// Lua can also run out of memory during protected script execution,
// such as when running a macro or some other game hook, but in such
// cases the Lua interpreter will throw an exception instead of
// panicking.
//
static int _clua_panic(lua_State *ls)
{
    if (crawl_state.need_save && !crawl_state.saving_game
        && !crawl_state.updating_scores)
    {
        save_game(true);
    }
    return (0);
}

static void *_clua_allocator(void *ud, void *ptr, size_t osize, size_t nsize)
{
    CLua *cl = static_cast<CLua *>( ud );
    cl->memory_used += nsize - osize;

    if (nsize > osize && cl->memory_used >= CLUA_MAX_MEMORY_USE * 1024
        && cl->mixed_call_depth)
    {
        return (NULL);
    }

    if (!nsize)
    {
        free(ptr);
        return (NULL);
    }
    else
        return (realloc(ptr, nsize));
}

static void _clua_throttle_hook(lua_State *ls, lua_Debug *dbg)
{
    CLua *lua = lua_call_throttle::find_clua(ls);

    // Co-routines can create a new Lua state; in such cases, we must
    // fudge it.
    if (!lua)
        lua = &clua;

    if (lua)
    {
        if (!lua->throttle_sleep_ms)
            lua->throttle_sleep_ms = lua->throttle_sleep_start;
        else if (lua->throttle_sleep_ms < lua->throttle_sleep_end)
            lua->throttle_sleep_ms *= 2;

        ++lua->n_throttle_sleeps;

        delay(lua->throttle_sleep_ms);

        // Try to kill the annoying script.
        if (lua->n_throttle_sleeps > CLua::MAX_THROTTLE_SLEEPS)
        {
            lua->n_throttle_sleeps = CLua::MAX_THROTTLE_SLEEPS;
            luaL_error(ls, BUGGY_SCRIPT_ERROR);
        }
    }
}

lua_call_throttle::lua_call_throttle(CLua *_lua)
    : lua(_lua)
{
    lua->init_throttle();
    if (!lua->mixed_call_depth++)
        lua_map[lua->state()] = lua;
}

lua_call_throttle::~lua_call_throttle()
{
    if (!--lua->mixed_call_depth)
        lua_map.erase(lua->state());
}

CLua *lua_call_throttle::find_clua(lua_State *ls)
{
    lua_clua_map::iterator i = lua_map.find(ls);
    return (i != lua_map.end()? i->second : NULL);
}

// This function is a replacement for Lua's in-built pcall function. It behaves
// like pcall in all respects (as documented in the Lua 5.1 reference manual),
// but does not allow the Lua chunk/script to catch errors thrown by the
// Lua-throttling code. This is necessary so that we can interrupt scripts that
// are hogging CPU.
//
// If we did not intercept pcall, the script could do the equivalent
// of this:
//
//    while true do
//      pcall(function () while true do end end)
//    end
//
// And there's a good chance we wouldn't be able to interrupt the
// deadloop because our errors would get caught by the pcall (more
// levels of nesting would just increase the chance of the script
// beating our throttling).
//
static int _clua_guarded_pcall(lua_State *ls)
{
    const int nargs = lua_gettop(ls);
    const int err = lua_pcall(ls, nargs - 1, LUA_MULTRET, 0);

    if (err)
    {
        const char *errs = lua_tostring(ls, 1);
        if (!errs || strstr(errs, BUGGY_SCRIPT_ERROR))
            luaL_error(ls, errs? errs : BUGGY_PCALL_ERROR);
    }

    lua_pushboolean(ls, !err);
    lua_insert(ls, 1);

    return (lua_gettop(ls));
}

static int _clua_loadfile(lua_State *ls)
{
    const char *file = luaL_checkstring(ls, 1);
    if (!file)
        return (0);

    const int err = CLua::loadfile(ls, file, !CLua::is_managed_vm(ls));
    if (err)
    {
        const int place = lua_gettop(ls);
        lua_pushnil(ls);
        lua_insert(ls, place);
        return (2);
    }
    return (1);
}

static int _clua_require(lua_State *ls)
{
    const char *file = luaL_checkstring(ls, 1);
    if (!file)
        return (0);

    CLua &vm(CLua::get_vm(ls));
    if (vm.execfile(file, false, false) != 0)
        luaL_error(ls, vm.error.c_str());

    lua_pushboolean(ls, true);
    return (1);
}

static int _clua_dofile(lua_State *ls)
{
    const char *file = luaL_checkstring(ls, 1);
    if (!file)
        return (0);

    const int err = CLua::loadfile(ls, file, !CLua::is_managed_vm(ls));
    if (err)
        return (lua_error(ls));

    lua_call(ls, 0, LUA_MULTRET);
    return (lua_gettop(ls));
}

std::string quote_lua_string(const std::string &s)
{
    return replace_all_of(replace_all_of(s, "\\", "\\\\"), "\"", "\\\"");
}

/////////////////////////////////////////////////////////////////////

lua_shutdown_listener::~lua_shutdown_listener()
{
}

lua_datum::lua_datum(CLua &_lua, int stackpos, bool pop)
    : lua(_lua), need_cleanup(true)
{
    // Store the datum in the registry indexed by "this".
    lua_pushvalue(lua, stackpos);
    lua_pushlightuserdata(lua, this);
    // Move the key (this) before the value.
    lua_insert(lua, -2);
    lua_settable(lua, LUA_REGISTRYINDEX);

    if (pop && stackpos < 0)
        lua_pop(lua, -stackpos);

    lua.add_shutdown_listener(this);
}

lua_datum::lua_datum(const lua_datum &o)
    : lua(o.lua), need_cleanup(true)
{
    set_from(o);
}

void lua_datum::set_from(const lua_datum &o)
{
    lua_pushlightuserdata(lua, this);
    o.push();
    lua_settable(lua, LUA_REGISTRYINDEX);
    lua.add_shutdown_listener(this);
    need_cleanup = true;
}

const lua_datum &lua_datum::operator = (const lua_datum &o)
{
    if (this != &o)
    {
        cleanup();
        set_from(o);
    }
    return (*this);
}

void lua_datum::push() const
{
    lua_pushlightuserdata(lua, const_cast<lua_datum*>(this));
    lua_gettable(lua, LUA_REGISTRYINDEX);

    // The value we saved is now on top of the Lua stack.
}

lua_datum::~lua_datum()
{
    cleanup();
}

void lua_datum::shutdown(CLua &)
{
    cleanup();
}

void lua_datum::cleanup()
{
    if (need_cleanup)
    {
        need_cleanup = false;
        lua.remove_shutdown_listener(this);

        lua_pushlightuserdata(lua, this);
        lua_pushnil(lua);
        lua_settable(lua, LUA_REGISTRYINDEX);
    }
}

#define LUA_CHECK_TYPE(check) \
    lua_stack_cleaner clean(lua);                               \
    push();                                                     \
    return check(lua, -1)

bool lua_datum::is_table() const
{
    LUA_CHECK_TYPE(lua_istable);
}

bool lua_datum::is_function() const
{
    LUA_CHECK_TYPE(lua_isfunction);
}

bool lua_datum::is_number() const
{
    LUA_CHECK_TYPE(lua_isnumber);
}

bool lua_datum::is_string() const
{
    LUA_CHECK_TYPE(lua_isstring);
}

bool lua_datum::is_udata() const
{
    LUA_CHECK_TYPE(lua_isuserdata);
}

// Can be called from within a debugger to look at the current Lua
// call stack.  (Borrowed from ToME 3)
void print_clua_stack(void)
{
    struct lua_Debug dbg;
    int              i = 0;
    lua_State       *L = clua.state();

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
