/*
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

#include "AppHdr.h"

#include "clua.h"

#include "abl-show.h"
#include "command.h"
#include "chardump.h"
#include "delay.h"
#include "food.h"
#include "invent.h"
#include "initfile.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "item_use.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "mon-util.h"
#include "output.h"
#include "player.h"
#include "randart.h"
#include "skills2.h"
#include "spl-util.h"
#include "stuff.h"

#include <cstring>

#ifdef HASH_CONTAINERS
#   include <hash_map>
#   define CHMAP HASH_CONTAINER_NS::hash_map
#else
#   include <map>
#   define CHMAP std::map
#endif

#include <cctype>

#define CL_RESETSTACK_RETURN(ls, oldtop, retval) \
    if (true) \
    {\
        if (oldtop != lua_gettop(ls)) \
        { \
            lua_settop(ls, oldtop); \
        } \
        return (retval); \
    } \
    else

CLua clua;

CLua::CLua() : _state(NULL), sourced_files(), uniqindex(0L)
{
}

CLua::~CLua()
{
    if (_state)
        lua_close(_state);
}

// This has the disadvantage of repeatedly trying init_lua if it fails.
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

void CLua::getregistry(const char *name) 
{
    lua_pushstring(state(), name);
    lua_gettable(state(), LUA_REGISTRYINDEX);
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

int CLua::execstring(const char *s, const char *context)
{
    lua_State *ls = state();
    int err = luaL_loadbuffer(ls, s, strlen(s), context);
    if (err)
    {
        set_error(err, ls);
        return err;
    }
    err = lua_pcall(ls, 0, 0, 0);
    set_error(err, ls);
    return err;
}

int CLua::execfile(const char *filename)
{
    if (sourced_files.find(filename) != sourced_files.end())
        return 0;

    sourced_files.insert(filename);

    FILE *f = fopen(filename, "r");
    if (f)
        fclose(f);
    else
    {
        error = std::string("Can't read ") + filename;
        return -1;
    }

    lua_State *ls = state();
    if (!ls)
        return -1;
    
    int err = luaL_loadfile(ls, filename);
    if (!err)
        err = lua_pcall(ls, 0, 0, 0);
    set_error(err);
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

static void push_monster(lua_State *ls, monsters *mons);
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
    
    lua_getglobal(ls, fn);
    if (!lua_isfunction(ls, -1))
    {
        lua_settop(ls, -nargs - 2);
        return (false);
    }

    // Slide the function in front of its args and call it.
    if (nargs)
        lua_insert(ls, -nargs - 1);
    int err = lua_pcall(ls, nargs, nret, 0);
    set_error(err, ls);
    return !err;
}

// Defined in Kills.cc because the kill bindings refer to Kills.cc local 
// structs
extern void lua_open_kills(lua_State *ls);

void lua_open_you(lua_State *ls);
void lua_open_item(lua_State *ls);
void lua_open_food(lua_State *ls);
void lua_open_crawl(lua_State *ls);
void lua_open_file(lua_State *ls);
void lua_open_options(lua_State *ls);
void lua_open_monsters(lua_State *ls);
void lua_open_globals(lua_State *ls);


void CLua::init_lua()
{
    if (_state)
        return;

    _state = lua_open();
    if (!_state)
        return;
    luaopen_base(_state);
    luaopen_string(_state);
    luaopen_table(_state);

    // Open Crawl bindings
    lua_open_kills(_state);
    lua_open_you(_state);
    lua_open_item(_state);
    lua_open_food(_state);
    lua_open_crawl(_state);
    lua_open_file(_state);
    lua_open_options(_state);
    lua_open_monsters(_state);

    lua_open_globals(_state);

    load_cmacro();
    load_chooks();
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
    static const char *c_macro =
        "function c_macro(fn)"
        "    if fn == nil then"
        "        if c_macro_coroutine ~= nil then"
        "            local coret, mret"
        "            coret, mret = coroutine.resume(c_macro_coroutine)"
        "            if not coret or not mret then"
        "                c_macro_coroutine = nil"
        "                c_macro_name      = nil"
        "            end"
        "            if not coret and mret then"
        "                error(mret)"
        "            end"
        "            return (coret and mret)"
        "        end"
        "        return false"
        "    end"
        "    if _G[fn] == nil or type(_G[fn]) ~= 'function' then"
        "        return false"
        "    end"
        "    c_macro_name = fn"
        "    c_macro_coroutine = coroutine.create(_G[fn]) "
        "    return c_macro() "
        "end";
    execstring(c_macro, "base");
}

/////////////////////////////////////////////////////////////////////

#define LUAWRAP(name, wrapexpr) \
    static int name(lua_State *ls) \
    {   \
        wrapexpr; \
        return (0); \
    }

#define LUARET1(name, type, val) \
    static int name(lua_State *ls) \
    { \
        lua_push##type(ls, val); \
        return (1); \
    }

#define LUARET2(name, type, val1, val2)  \
    static int name(lua_State *ls) \
    { \
        lua_push##type(ls, val1); \
        lua_push##type(ls, val2); \
        return (2); \
    }

template <class T> T *util_get_userdata(lua_State *ls, int ndx)
{
    return (lua_islightuserdata(ls, ndx))?
            static_cast<T *>( lua_touserdata(ls, ndx) )
          : NULL;
}

template <class T> T *clua_get_userdata(lua_State *ls, const char *mt)
{
    return static_cast<T*>( luaL_checkudata( ls, 1, mt ) );
}

static void clua_register_metatable(lua_State *ls, const char *tn, 
                                   const luaL_reg *lr,
                                   int (*gcfn)(lua_State *ls) = NULL)
{
    int top = lua_gettop(ls);
    
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
    
    luaL_openlib(ls, NULL, lr, 0);

    lua_settop(ls, top);
}

template <class T> T *clua_new_userdata(
        lua_State *ls, const char *mt)
{
    void *udata = lua_newuserdata( ls, sizeof(T) );
    luaL_getmetatable(ls, mt);
    lua_setmetatable(ls, -2);
    return static_cast<T*>( udata );
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
LUARET1(you_race, string, species_name(you.species, you.experience_level))
LUARET1(you_class, string, get_class_name(you.char_class))
LUARET2(you_hp, number, you.hp, you.hp_max)
LUARET2(you_mp, number, you.magic_points, you.max_magic_points)
LUARET1(you_hunger, string, hunger_level())
LUARET2(you_strength, number, you.strength, you.max_strength)
LUARET2(you_intelligence, number, you.intel, you.max_intel)
LUARET2(you_dexterity, number, you.dex, you.max_dex)
LUARET1(you_exp, number, you.experience_level)
LUARET1(you_exp_points, number, you.experience)
LUARET1(you_res_poison, number, player_res_poison(false))
LUARET1(you_res_fire, number, player_res_fire(false))
LUARET1(you_res_cold, number, player_res_cold(false))
LUARET1(you_res_draining, number, player_prot_life(false))
LUARET1(you_res_shock, number, player_res_electricity(false))
LUARET1(you_res_statdrain, number, player_sust_abil(false))
LUARET1(you_res_mutation, number, wearing_amulet(AMU_RESIST_MUTATION, false))
LUARET1(you_res_slowing, number, wearing_amulet(AMU_RESIST_SLOW, false))
LUARET1(you_gourmand, boolean, wearing_amulet(AMU_THE_GOURMAND, false))
LUARET1(you_levitating, boolean, 
        player_is_levitating() && !wearing_amulet(AMU_CONTROLLED_FLIGHT))
LUARET1(you_flying, boolean,
        player_is_levitating() && wearing_amulet(AMU_CONTROLLED_FLIGHT))
LUARET1(you_transform, string, transform_name())
LUAWRAP(you_stop_activity, interrupt_activity(AI_FORCE_INTERRUPT))

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

static const struct luaL_reg you_lib[] =
{
    { "turn_is_over", you_turn_is_over },
    { "spells"      , l_you_spells },
    { "abilities"   , l_you_abils },
    { "name"        , you_name },
    { "race"        , you_race },
    { "class"       , you_class },
    { "hp"          , you_hp },
    { "mp"          , you_mp },
    { "hunger"      , you_hunger },
    { "strength"    , you_strength },
    { "intelligence", you_intelligence },
    { "dexterity"   , you_dexterity },
    { "exp"         , you_exp },
    { "exp_points"  , you_exp_points },

    { "res_poison"  , you_res_poison },
    { "res_fire"    , you_res_fire   },
    { "res_cold"    , you_res_cold   },
    { "res_draining", you_res_draining },
    { "res_shock"   , you_res_shock },
    { "res_statdrain", you_res_statdrain },
    { "res_mutation", you_res_mutation },
    { "res_slowing",  you_res_slowing },
    { "gourmand",     you_gourmand },
    { "levitating",   you_levitating },
    { "flying",       you_flying },
    { "transform",    you_transform },

    { "stop_activity", you_stop_activity },

    { "floor_items",  you_floor_items },
    { NULL, NULL },
};

void lua_open_you(lua_State *ls)
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
    if (slot1 < 0 || slot1 >= ENDOFPACK ||
            slot2 < 0 || slot2 >= ENDOFPACK ||
            slot1 == slot2 || !is_valid_item(you.inv[slot1]))
        return (0);
    
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

static int item_on_floor(const item_def &item, int x, int y);

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

    int floor_link = item_on_floor(*item, you.x_pos, you.y_pos);
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
    "teleport control",
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

static description_level_type desc_code(const char *desc)
{
    if (!desc)
        return DESC_PLAIN;
    
    if (!strcmp("The", desc))
        return DESC_CAP_THE;
    else if (!strcmp("the", desc))
        return DESC_NOCAP_THE;
    else if (!strcmp("A", desc))
        return DESC_CAP_A;
    else if (!strcmp("a", desc))
        return DESC_NOCAP_A;
    else if (!strcmp("Your", desc))
        return DESC_CAP_YOUR;
    else if (!strcmp("your", desc))
        return DESC_NOCAP_YOUR;
    else if (!strcmp("its", desc))
        return DESC_NOCAP_ITS;
    else if (!strcmp("worn", desc))
        return DESC_INVENTORY_EQUIP;
    else if (!strcmp("inv", desc))
        return DESC_INVENTORY;

    return DESC_PLAIN;
}

static int l_item_name(lua_State *ls)
{
    LUA_ITEM(item, 1);
    if (item)
    {
        description_level_type ndesc = DESC_PLAIN;
        if (lua_isstring(ls, 2))
            ndesc = desc_code(lua_tostring(ls, 2));
        bool terse = lua_toboolean(ls, 3);
        lua_pushstring(ls, item->name(ndesc, terse).c_str());
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
        int slot = in_inventory(*item)? item->link :
                        letter_to_index(item->slot);
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
    if (skill == SK_RANGED_COMBAT)
        skill = weapon_skill(*item);
    if (skill == SK_FIGHTING)
        return (0);

    lua_pushstring(ls, skill_name(skill));
    lua_pushnumber(ls, skill);
    return (2);
}

static int l_item_artifact(lua_State *ls)
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
    { "artifact",          l_item_artifact },
    { "branded",           l_item_branded },
    { "class",             l_item_class },
    { "subtype",           l_item_subtype },
    { "cursed",            l_item_cursed },
    { "worn",              l_item_worn },
    { "name",              l_item_name },
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
    { "equipped_at",       l_item_equipped },
    { "equip_type",        l_item_equip_type },
    { "weap_skill",        l_item_weap_skill },
    
    { NULL, NULL },
};

void lua_open_item(lua_State *ls)
{
    luaL_openlib(ls, "item", item_lib, 0);
}

/////////////////////////////////////////////////////////////////////
// Food information. Some of this information is spoily (such as whether
// a given chunk is poisonous), but that can't be helped.
//

static int food_do_eat(lua_State *ls)
{
    bool eaten = false;
    if (!you.turn_is_over)
        eaten = eat_food(false);
    lua_pushboolean(ls, eaten);
    return (1);
}

static int food_prompt_floor(lua_State *ls)
{
    bool eaten = false;
    if (!you.turn_is_over && (eaten = eat_from_floor()))
        burden_change();
    lua_pushboolean(ls, eaten);
    return (1);
}

static int food_prompt_inventory(lua_State *ls)
{
    bool eaten = false;
    if (!you.turn_is_over)
        eaten = prompt_eat_from_inventory();
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

static int item_on_floor(const item_def &item, int x, int y)
{
    // Check if the item is on the floor and reachable
    for (int link = igrd[x][y]; link != NON_ITEM; link = mitm[link].link)
    {
        if (&mitm[link] == &item)
            return (link);
    }
    return (NON_ITEM);
}

static bool eat_item(const item_def &item)
{
    if (in_inventory(item))
    {
        eat_from_inventory(item.link);
        burden_change();
        you.turn_is_over = true;

        return (true);
    }
    else
    {
        int ilink =  item_on_floor(item, you.x_pos, you.y_pos);

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
    {
        rotting = item->special < 100;
    }
    lua_pushboolean(ls, rotting);
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
    { "do_eat",         food_do_eat },
    { "prompt_floor",   food_prompt_floor },
    { "prompt_inventory", food_prompt_inventory },
    { "can_eat",        food_can_eat },
    { "eat",            food_eat },
    { "rotting",        food_rotting },
    { "ischunk",        food_ischunk },
    { NULL, NULL },
};

void lua_open_food(lua_State *ls)
{
    luaL_openlib(ls, "food", food_lib, 0);
}

/////////////////////////////////////////////////////////////////////
// General game bindings.
//

static int crawl_mpr(lua_State *ls)
{
    const char *message = luaL_checkstring(ls, 1);
    if (!message)
        return (0);

    int ch = MSGCH_PLAIN;
    const char *channel = lua_tostring(ls, 2);
    if (channel)
        ch = str_to_channel(channel);
    if (ch == -1)
        ch = MSGCH_PLAIN;

    mpr(message, ch);

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

static int crawl_bindkey(lua_State *ls)
{
    const char *s = NULL;
    if (lua_isstring(ls, 1))
    {
        s = lua_tostring(ls, 1);
    }

    if (!s || !lua_isfunction(ls, 2) || !lua_gettop(ls) == 2)
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

static int crawl_regex_gc(lua_State *ls)
{
    text_pattern **pattern = 
            clua_get_userdata< text_pattern* >(ls, REGEX_METATABLE);
    if (pattern)
        delete *pattern;
    return (0);
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

static int crawl_messf_gc(lua_State *ls)
{
    message_filter **pattern = 
            clua_get_userdata< message_filter* >(ls, REGEX_METATABLE);
    if (pattern)
        delete *pattern;
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

static const struct luaL_reg crawl_lib[] =
{
    { "mpr",            crawl_mpr },
    { "mesclr",         crawl_mesclr },
    { "redraw_screen",  crawl_redraw_screen },
    { "input_line",     crawl_input_line },
    { "c_input_line",   crawl_c_input_line},
    { "getch",          crawl_getch },
    { "kbhit",          crawl_kbhit },
    { "flush_input",    crawl_flush_input },
    { "sendkeys",       crawl_sendkeys },
    { "playsound",      crawl_playsound },
    { "runmacro",       crawl_runmacro },
    { "bindkey",        crawl_bindkey },
    { "setopt",         crawl_setopt },
    { "msgch_num",      crawl_msgch_num },
    { "msgch_name",     crawl_msgch_name },

    { "regex",          crawl_regex },
    { "message_filter", crawl_message_filter },
    { "trim",           crawl_trim },
    { "split",          crawl_split },
    { NULL, NULL },
};

void lua_open_crawl(lua_State *ls)
{
    clua_register_metatable(ls, REGEX_METATABLE, crawl_regex_ops,
                            crawl_regex_gc);
    clua_register_metatable(ls, MESSF_METATABLE, crawl_messf_ops,
                            crawl_messf_gc);

    luaL_openlib(ls, "crawl", crawl_lib, 0);
}

///////////////////////////////////////////////////////////
// File operations


static const struct luaL_reg file_lib[] =
{
    { "write", CLua::file_write },
    { NULL, NULL },
};

void lua_open_file(lua_State *ls)
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
    { "easy_open", &Options.easy_open, option_hboolean },
    { "colour_map", &Options.colour_map, option_hboolean },
    { "clean_map", &Options.clean_map, option_hboolean },
    { "show_uncursed", &Options.show_uncursed, option_hboolean },
    { "always_greet", &Options.always_greet, option_hboolean },
    { "easy_open", &Options.easy_open, option_hboolean },
    { "easy_armour", &Options.easy_unequip, option_hboolean },
    { "easy_unequip", &Options.easy_unequip, option_hboolean },
    { "easy_butcher", &Options.easy_butcher, option_hboolean },
    { "terse_hand", &Options.terse_hand, option_hboolean },
    { "increasing_skill_progress", &Options.increasing_skill_progress, option_hboolean },
    { "confirm_self_target", &Options.confirm_self_target, option_hboolean },
    { "default_target", &Options.default_target, option_hboolean },
    { "safe_autopickup", &Options.safe_autopickup, option_hboolean },
    { "autopickup_no_burden", &Options.autopickup_no_burden, option_hboolean },
    { "note_skill_max", &Options.note_skill_max, option_hboolean },
    { "use_notes", &Options.use_notes, option_hboolean },
    { "delay_message_clear", &Options.delay_message_clear, option_hboolean },
    { "no_dark_brand", &Options.no_dark_brand, option_hboolean },
    { "auto_list", &Options.auto_list, option_hboolean },
    { "lowercase_invocations", &Options.lowercase_invocations, 
                    option_hboolean },
    { "pickup_thrown", &Options.pickup_thrown, option_hboolean },
    { "pickup_dropped", &Options.pickup_dropped, option_hboolean },
    { "show_waypoints", &Options.show_waypoints, option_hboolean },
    { "item_colour", &Options.item_colour, option_hboolean },
    { "target_zero_exp", &Options.target_zero_exp, option_hboolean },
    { "safe_zero_exp", &Options.safe_zero_exp, option_hboolean },
    { "target_wrap", &Options.target_wrap, option_hboolean },
    { "easy_exit_menu", &Options.easy_exit_menu, option_hboolean },
    { "dos_use_background_intensity", &Options.dos_use_background_intensity, 
                    option_hboolean },
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

#define OPT_METATABLE "options.optaccess"
void lua_open_options(lua_State *ls)
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

    clua.setglobal("options");
}

/////////////////////////////////////////////////////////////////////
// Monster handling

#define MONS_METATABLE "monster.monsaccess"
struct MonsterWrap
{
    monsters *mons;
    long      turn;
};

static int l_mons_name(lua_State *ls, monsters *mons, const char *attr)
{
    lua_pushstring(ls, mons_type_name(mons->type, DESC_PLAIN).c_str());
    return (1);
}

static int l_mons_x(lua_State *ls, monsters *mons, const char *attr)
{
    lua_pushnumber(ls, int(mons->x) - int(you.x_pos));
    return (1);
}

static int l_mons_y(lua_State *ls, monsters *mons, const char *attr)
{
    lua_pushnumber(ls, int(mons->y) - int(you.y_pos));
    return (1);
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

static void push_monster(lua_State *ls, monsters *mons)
{
    MonsterWrap *mw = clua_new_userdata< MonsterWrap >(ls, MONS_METATABLE);
    mw->turn = you.num_turns;
    mw->mons = mons;
}

void lua_open_monsters(lua_State *ls)
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

typedef CHMAP<std::string, text_pattern> pattern_map;
static pattern_map pattern_cache;

static text_pattern &get_text_pattern(const std::string &s, bool checkcase)
{
    pattern_map::iterator i = pattern_cache.find(s);
    if (i != pattern_cache.end())
        return i->second;

    if (pattern_cache.size() > PATTERN_FLUSH_CEILING)
        pattern_cache.clear();

    pattern_cache[s] = text_pattern(s, !checkcase);
    return pattern_cache[s];
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

void lua_open_globals(lua_State *ls)
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
        return false;

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
