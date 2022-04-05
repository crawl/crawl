#include "AppHdr.h"

#include "clua.h"

#include <algorithm>

#include "cluautil.h"
#include "dlua.h"
#include "end.h"
#include "files.h"
#include "libutil.h"
#include "l-libs.h"
#include "maybe-bool.h"
#include "misc.h" // erase_val
#include "options.h"
#include "state.h"
#include "stringutil.h"
#include "syscalls.h"
#include "unicode.h"
#include "version.h"

#define BUGGY_PCALL_ERROR  "667: Malformed response to guarded pcall."
#define BUGGY_SCRIPT_ERROR "666: Killing badly-behaved Lua script."

// 64-bit luajit does not support custom allocators. Only checking
// TARGET_CPU_X64 because luajit doesn't support other 64-bit archs.
#if defined(USE_LUAJIT) && defined(TARGET_CPU_X64)
#define NO_CUSTOM_ALLOCATOR
#endif

#define CL_RESETSTACK_RETURN(ls, oldtop, retval) \
    do \
    {\
        if (oldtop != lua_gettop(ls)) \
        { \
            lua_settop(ls, oldtop); \
        } \
        return retval; \
    } \
    while (false)

static int  _clua_panic(lua_State *);
static void _clua_throttle_hook(lua_State *, lua_Debug *);
#ifndef NO_CUSTOM_ALLOCATOR
static void *_clua_allocator(void *ud, void *ptr, size_t osize, size_t nsize);
#endif
static int  _clua_guarded_pcall(lua_State *);
static int  _clua_require(lua_State *);
static int  _clua_dofile(lua_State *);
static int  _clua_loadfile(lua_State *);
static string _get_persist_file();

CLua::CLua(bool managed)
    : error(), managed_vm(managed), shutting_down(false),
      throttle_unit_lines(50000),
      throttle_sleep_ms(0), throttle_sleep_start(2),
      throttle_sleep_end(800), n_throttle_sleeps(0), mixed_call_depth(0),
      lua_call_depth(0), max_mixed_call_depth(8),
      max_lua_call_depth(100), memory_used(0),
      _state(nullptr), sourced_files(), uniqindex(0)
{
}

CLua::~CLua()
{
    // Copy the listener vector, because listeners may remove
    // themselves from the listener list when we notify them of a
    // shutdown.
    const vector<lua_shutdown_listener*> slisteners = shutdown_listeners;
    for (lua_shutdown_listener *listener : slisteners)
        listener->shutdown(*this);
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

string CLua::setuniqregistry()
{
    char name[100];
    snprintf(name, sizeof name, "__cru%u", uniqindex++);
    lua_pushstring(state(), name);
    lua_insert(state(), -2);
    lua_settable(state(), LUA_REGISTRYINDEX);

    return name;
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

void CLua::gc()
{
    lua_gc(state(), LUA_GCCOLLECT, 0);
}

void CLua::save(writer &outf)
{
    if (!_state)
        return;

    string res;
    callfn("c_save", ">s", &res);
    outf.write(res.c_str(), res.size());
}

void CLua::save_persist()
{
    string persist;
    // We load persist.lua immediately before calling c_save_persist so
    // that we know that it hasn't been overwritten by a player version.
    execfile("dlua/persist.lua", true, true);
    callfn("c_save_persist", ">s", &persist);
    if (Options.no_save)
        return;

    FILE *f;
    const string persistfile = _get_persist_file();

    // Don't create the file if there's no need to do so.
    if (persist.empty() && !file_exists(persistfile))
        return;

    f = fopen_u(persistfile.c_str(), "w");
    if (!f)
    {
        mprf(MSGCH_ERROR, "Couldn't open %s for writing!", persistfile.c_str());
        return;
    }

    fprintf(f, "-- %s %s persistent clua file\n"
               "-- WARNING: This file is entirely auto-generated.\n"
            "\n",
            OUTS(CRAWL), // ok, localizing the game name is not likely
            OUTS(Version::Long)); // nor the version string
    fprintf(f, "%s", persist.c_str());
    fclose(f);
}

void CLua::load_persist()
{
    if (Options.no_save)
        return;
    string persistfile = _get_persist_file();
    if (!file_exists(persistfile))
        return;
    FileLineInput f(persistfile.c_str());
    string script;
    while (!f.eof())
        script += f.get_line() + "\n";
    execstring(script.c_str());
}

int CLua::file_write(lua_State *ls)
{
    if (!lua_islightuserdata(ls, 1))
    {
        luaL_argerror(ls, 1, "Expected filehandle at arg 1");
        return 0;
    }
    CLuaSave *sf = static_cast<CLuaSave *>(lua_touserdata(ls, 1));
    if (!sf)
        return 0;

    FILE *f = sf->get_file();
    if (!f)
        return 0;

    const char *text = luaL_checkstring(ls, 2);
    if (text)
        fprintf(f, "%s", text);
    return 0;
}

FILE *CLua::CLuaSave::get_file()
{
    if (!handle)
        handle = fopen_u(filename, "w");

    return handle;
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
        error = "<LUA not initialised>";
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

    if (!crawl_state.throttle)
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
        return err;

    lua_State *ls = state();
    lua_call_throttle strangler(this);
    err = lua_pcall(ls, 0, nresults, 0);
    set_error(err, ls);
    return err;
}

bool CLua::is_path_safe(string s, bool trusted)
{
    lowercase(s);
    return s.find("..") == string::npos && shell_safe(s.c_str())
           // loading dlua stuff would spew tons of error messages
           && (trusted || s.find("dlua") != 0);
}

int CLua::loadfile(lua_State *ls, const char *filename, bool trusted,
                   bool die_on_fail)
{
    if (!ls)
        return -1;

    if (!is_path_safe(filename, trusted))
    {
        lua_pushstring(
            ls,
            make_stringf("invalid filename: %s", filename).c_str());
        return -1;
    }

    string file = datafile_path(filename, die_on_fail);
    if (file.empty())
    {
        lua_pushstring(ls,
                       make_stringf("Can't find \"%s\"", filename).c_str());
        return -1;
    }

    FileLineInput f(file.c_str());
    string script;
    while (!f.eof())
        script += f.get_line() + "\n";

    // prefixing with @ stops lua from adding [string "%s"]
    return luaL_loadbuffer(ls, &script[0], script.length(),
                           ("@" + file).c_str());
}

int CLua::execfile(const char *filename, bool trusted, bool die_on_fail,
                   bool force)
{
    if (!force && sourced_files.count(filename))
        return 0;

    lua_State *ls = state();
    int err = loadfile(ls, filename, trusted || !managed_vm, die_on_fail);
    lua_call_throttle strangler(this);
    if (!err)
        err = lua_pcall(ls, 0, 0, 0);
    if (!err)
        sourced_files.insert(filename);
    set_error(err);
    if (die_on_fail && !error.empty())
    {
        end(1, false, "Lua execfile error (%s): %s",
            filename, dlua.error.c_str());
    }
    return err;
}

bool CLua::runhook(const char *hook, const char *params, ...)
{
    error.clear();

    lua_State *ls = state();
    if (!ls)
        return false;

    // Remember top of stack, for debugging porpoises
    int stack_top = lua_gettop(ls);
    pushglobal(hook);
    if (!lua_istable(ls, -1))
    {
        lua_pop(ls, 1);
        CL_RESETSTACK_RETURN(ls, stack_top, false);
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
    CL_RESETSTACK_RETURN(ls, stack_top, true);
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
                *(va_arg(args, int*)) = luaL_safe_checkint(ls, sp);
            break;
        case 'b':
            *(va_arg(args, bool *)) = lua_toboolean(ls, sp);
            break;
        case 's':
            {
                const char *s = lua_tostring(ls, sp);
                if (s)
                    *(va_arg(args, string *)) = s;
                break;
            }
        default:
            break;
        }

    }
    // Pop args off the stack
    lua_pop(ls, nrets);
}

int CLua::push_args(lua_State *ls, const char *format, va_list args,
                    va_list *targ)
{
    if (!format)
    {
        if (targ)
            va_copy(*targ, args);
        return 0;
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
        case 'i':
            clua_push_item(ls, va_arg(args, item_def*));
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
            die("ambiguous long in Lua push_args");
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
            push_monster(ls, va_arg(args, monster*));
            break;
        case 'I':
            lua_push_moninf(ls, va_arg(args, monster_info *));
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
    return argc;
}

int CLua::return_count(lua_State *ls, const char *format)
{
    if (!format)
        return 0;

    const char *gs = strchr(format, '>');
    if (gs)
        return strlen(gs + 1);

    const char *cs = strchr(format, ':');
    if (cs && isdigit(*format))
    {
        char *es = nullptr;
        int ci = strtol(format, &es, 10);
        // We're capping return at 10 here, which is arbitrary, but avoids
        // blowing the stack.
        if (ci < 0)
            ci = 0;
        else if (ci > 10)
            ci = 10;
        return ci;
    }
    return 0;
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
    return !err;
}

maybe_bool CLua::callmbooleanfn(const char *fn, const char *params,
                                va_list args)
{
    error.clear();
    lua_State *ls = state();
    if (!ls)
        return MB_MAYBE;

    int stacktop = lua_gettop(ls);

    pushglobal(fn);
    if (!lua_isfunction(ls, -1))
    {
        lua_pop(ls, 1);
        CL_RESETSTACK_RETURN(ls, stacktop, MB_MAYBE);
    }

    bool ret = calltopfn(ls, params, args, 1);
    if (!ret)
        CL_RESETSTACK_RETURN(ls, stacktop, MB_MAYBE);

    maybe_bool r = frombool(lua_toboolean(ls, -1));
    CL_RESETSTACK_RETURN(ls, stacktop, r);
}

maybe_bool CLua::callmbooleanfn(const char *fn, const char *params, ...)
{
    va_list args;
    va_start(args, params);
    maybe_bool r = callmbooleanfn(fn, params, args);
    va_end(args);
    return r;
}

maybe_bool CLua::callmaybefn(const char *fn, const char *params, va_list args)
{
    error.clear();
    lua_State *ls = state();
    if (!ls)
        return MB_MAYBE;

    int stacktop = lua_gettop(ls);

    pushglobal(fn);
    if (!lua_isfunction(ls, -1))
    {
        lua_pop(ls, 1);
        CL_RESETSTACK_RETURN(ls, stacktop, MB_MAYBE);
    }

    bool ret = calltopfn(ls, params, args, 1);
    if (!ret)
        CL_RESETSTACK_RETURN(ls, stacktop, MB_MAYBE);

    maybe_bool r = lua_isboolean(ls, -1) ? frombool(lua_toboolean(ls, -1))
                                         : MB_MAYBE;
    CL_RESETSTACK_RETURN(ls, stacktop, r);
}

maybe_bool CLua::callmaybefn(const char *fn, const char *params, ...)
{
    va_list args;
    va_start(args, params);
    maybe_bool r = callmaybefn(fn, params, args);
    va_end(args);
    return r;
}

bool CLua::callbooleanfn(bool def, const char *fn, const char *params, ...)
{
    va_list args;
    va_start(args, params);
    maybe_bool r = callmbooleanfn(fn, params, args);
    va_end(args);
    return tobool(r, def);
}

bool CLua::proc_returns(const char *par) const
{
    return strchr(par, '>') != nullptr;
}

// Identical to lua_getglobal for simple names, but will look up
// "a.b.c" names in tables, so you can pushglobal("dgn.point") and get
// _G['dgn']['point'], as expected.
//
// Guarantees to push exactly one value onto the stack.
//
void CLua::pushglobal(const string &name)
{
    vector<string> pieces = split_string(".", name);
    lua_State *ls(state());

    if (pieces.empty())
        lua_pushnil(ls);

    for (unsigned i = 0, size = pieces.size(); i < size; ++i)
    {
        if (!i)
            lua_getglobal(ls, pieces[i].c_str());
        else
        {
            if (lua_istable(ls, -1))
            {
                lua_pushstring(ls, pieces[i].c_str());
                lua_gettable(ls, -2);
                // Swap the value we just found with the table itself.
                lua_insert(ls, -2);
                // And remove the table.
                lua_pop(ls, 1);
            }
            else
            {
                // We expected a table here, but got something else. Fail.
                lua_pop(ls, 1);
                lua_pushnil(ls);
                break;
            }
        }
    }
}

bool CLua::callfn(const char *fn, const char *params, ...)
{
    error.clear();
    lua_State *ls = state();
    if (!ls)
        return false;

    pushglobal(fn);
    if (!lua_isfunction(ls, -1))
    {
        lua_pop(ls, 1);
        return false;
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
    return ret;
}

bool CLua::callfn(const char *fn, int nargs, int nret)
{
    error.clear();
    lua_State *ls = state();
    if (!ls)
        return false;

    // If a function is not provided on the stack, get the named function.
    if (fn)
    {
        pushglobal(fn);
        if (!lua_isfunction(ls, -1))
        {
            lua_settop(ls, -nargs - 2);
            return false;
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

void CLua::init_lua()
{
    if (_state)
        return;

#ifdef NO_CUSTOM_ALLOCATOR
    // If this is likely to be used as a server, warn the builder.
    // NOTE: #warning doesn't work on MSVC, so this will be fatal there
    // (not that webtiles or dgamelaunch are supported on Windows anyway).
# if defined(USE_TILE_WEB) || defined(DGAMELAUNCH)
#   warning Detected 64-bit Luajit, disabling CLua memory throttling.
# endif
    _state = luaL_newstate();
#else
    // Throttle memory usage in managed (clua) VMs
    _state = managed_vm? lua_newstate(_clua_allocator, this) : luaL_newstate();
#endif
    if (!_state)
        end(1, false, "Unable to create Lua state.");

    lua_stack_cleaner clean(_state);

    lua_atpanic(_state, _clua_panic);

#ifdef CLUA_UNRESTRICTED_LIBS
    // open all libs -- this is not safe for public servers or releases!
    // Intended for people writing bots and the like.
    luaL_openlibs(_state);
#else
    // Selectively load some, but not all Lua core libraries.
    //
    // In Lua 5.1, these library setup calls are not supposed to be called
    // directly from C. If the lua version changes, this may need to be changed:
    // recommended practice is (apparently) checking the lua version's linit.cc
    // and seeing how that does the full library setup.
    //
    // This doesn't seem to *obviously* impact the libraries we use by default,
    // but some of the libraries we don't use will panic if not called
    // correctly; since someone writing a bot (for example) might want to
    // expand this, do things "correctly". The core lua libraries in 5.1 we are
    // not loading are:
    //
    // {LUA_LOADLIBNAME, luaopen_package},    // (require etc)
    // {LUA_IOLIBNAME, luaopen_io},           //
    // {LUA_OSLIBNAME, luaopen_os},
    // {LUA_DBLIBNAME, luaopen_debug},
    const vector<pair<string, lua_CFunction>> lua_core_libs =
    {
        {"", luaopen_base}, // XX: why no name? but this is how linit.cc does it
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
    };

    for (auto l : lua_core_libs)
    {
        lua_pushcfunction(_state, l.second);
        lua_pushstring(_state, l.first.c_str());
        lua_call(_state, 1, 0);
    }
#endif

    // Open Crawl bindings
    cluaopen_kills(_state);
    cluaopen_you(_state);
    cluaopen_item(_state);
    cluaopen_food(_state);
    cluaopen_crawl(_state);
    cluaopen_file(_state);
    cluaopen_moninf(_state);
    cluaopen_options(_state);
    cluaopen_travel(_state);
    cluaopen_view(_state);
    cluaopen_spells(_state);

    cluaopen_globals(_state);

    load_cmacro();
    load_chooks();

    lua_register(_state, "loadfile", _clua_loadfile);
    lua_register(_state, "dofile", _clua_dofile);

    lua_register(_state, "crawl_require", _clua_require);

    execfile("dlua/util.lua", true, true);
    execfile("dlua/iter.lua", true, true);
    execfile("dlua/tags.lua", true, true);
    execfile("dlua/init.lua", true, true);

    if (managed_vm)
    {
        lua_register(_state, "pcall", _clua_guarded_pcall);
        execfile("dlua/userbase.lua", true, true);
        execfile("dlua/persist.lua", true, true);
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
    CLua *vm = clua_get_lightuserdata<CLua>(ls, -1);
    if (!vm)
        luaL_error(ls, "Could not find matching clua for lua state");
    return *vm;
}

bool CLua::is_managed_vm(lua_State *ls)
{
    lua_stack_cleaner clean(ls);
    lua_pushstring(ls, "lua_vm_is_managed");
    lua_gettable(ls, LUA_REGISTRYINDEX);
    return lua_toboolean(ls, -1);
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
    execfile("dlua/macro.lua", true, true);
}

void CLua::add_shutdown_listener(lua_shutdown_listener *listener)
{
    if (find(shutdown_listeners.begin(), shutdown_listeners.end(), listener)
        == shutdown_listeners.end())
    {
        shutdown_listeners.push_back(listener);
    }
}

void CLua::remove_shutdown_listener(lua_shutdown_listener *listener)
{
    erase_val(shutdown_listeners, listener);
}

// Can be called from within a debugger to look at the current Lua
// call stack. (Borrowed from ToME 3)
void CLua::print_stack()
{
    struct lua_Debug dbg;
    int              i = 0;
    lua_State       *L = state();

    fprintf(stderr, "\n");
    while (lua_getstack(L, i++, &dbg) == 1)
    {
        lua_getinfo(L, "lnuS", &dbg);

        char* file = strrchr(dbg.short_src, '/');
        if (file == nullptr)
            file = dbg.short_src;
        else
            file++;

        fprintf(stderr, "%s, function %s, line %d\n", file,
                dbg.name, dbg.currentline);
    }

    fprintf(stderr, "\n");
}

// //////////////////////////////////////////////////////////////////////
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

unsigned int lua_text_pattern::lfndx = 0;

bool lua_text_pattern::is_lua_pattern(const string &s)
{
    return any_of(begin(pat_ops), end(pat_ops),
            [&s] (const lua_pat_op &op)
            { return s.find(op.token) != string::npos; });
}

lua_text_pattern::lua_text_pattern(const string &_pattern)
    : translated(false), isvalid(true), pattern(_pattern),
      lua_fn_name(new_fn_name())
{
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

bool lua_text_pattern::matches(const string &s) const
{
    if (isvalid && !translated)
        translate();

    if (!isvalid)
        return false;

    return clua.callbooleanfn(false, lua_fn_name.c_str(), "s", s.c_str());
}

pattern_match lua_text_pattern::match_location(const string &s) const
{
    // lua_text_pattern is only used if a special non-regex op is detected (^F
    // for "armour && ego", for instance), and in those situations, it's
    // unclear what exactly to use for the matched text here (especially in
    // more complicated expressions that include things like <<>>, !!, etc).
    return matches(s)
        ? pattern_match::succeeded(s)
        : pattern_match::failed(s);
}

void lua_text_pattern::pre_pattern(string &pat, string &fn) const
{
    // Trim trailing spaces
    pat.erase(pat.find_last_not_of(" \t\n\r") + 1);

    fn += " pmatch([[";
    fn += pat;
    fn += "]], text, false) ";

    pat.clear();
}

void lua_text_pattern::post_pattern(string &pat, string &fn) const
{
    pat.erase(0, pat.find_first_not_of(" \t\n\r"));

    fn += " pmatch([[";
    fn += pat;
    fn += "]], text, false) ";

    pat.clear();
}

string lua_text_pattern::new_fn_name()
{
    return make_stringf("__ch_stash_search_%u", lfndx++);
}

bool lua_text_pattern::translate() const
{
    if (translated || !isvalid)
        return false;

    if (pattern.find("]]") != string::npos || pattern.find("[[") != string::npos)
        return false;

    string textp;
    string luafn;
    const lua_pat_op *currop = nullptr;
    for (string::size_type i = 0; i < pattern.length(); ++i)
    {
        bool match = false;
        for (unsigned p = 0; p < ARRAYSZ(pat_ops); ++p)
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

                i += strlen(lop.token) - 1;

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

    int err = clua.execstring(luafn.c_str(), "stash-search");
    if (err)
    {
        lua_text_pattern *self = const_cast<lua_text_pattern *>(this);
        self->isvalid = self->translated = false;
    }

    return translated;
}

// ////////////////////////////////////////////////////////////////////////

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
    return 0;
}

#ifndef NO_CUSTOM_ALLOCATOR
static void *_clua_allocator(void *ud, void *ptr, size_t osize, size_t nsize)
{
    CLua *cl = static_cast<CLua *>(ud);
    cl->memory_used += nsize - osize;

    if (nsize > osize && cl->memory_used >= CLUA_MAX_MEMORY_USE * 1024
        && cl->mixed_call_depth)
    {
        return nullptr;
    }

    if (!nsize)
    {
        free(ptr);
        return nullptr;
    }
    else
        return realloc(ptr, nsize);
}
#endif

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
    return lookup(lua_map, ls, nullptr);
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

    return lua_gettop(ls);
}

// Document clua globals here, as they're bound by the interpreter object

/*** Pre-defined globals.
 *
 * *Note:* this is not a real module. All names described here are defined in
 * the global clua namespace.
 * @module Globals
 */

/*** Load the named lua file as a chunk.
 * @tparam string filename
 * @return function chunk or nil,error
 * @function loadfile
 */
static int _clua_loadfile(lua_State *ls)
{
    const char *file = luaL_checkstring(ls, 1);
    if (!file)
        return 0;

    const int err = CLua::loadfile(ls, file, !CLua::is_managed_vm(ls));
    if (err)
    {
        const int place = lua_gettop(ls);
        lua_pushnil(ls);
        lua_insert(ls, place);
        return 2;
    }
    return 1;
}

/*** Load and execute the named lua file.
 * Differs from @{dofile} in that the file is run for its side effects.
 * If the execution has an error we raise that error and exit.
 * @tparam string filename
 * @treturn boolean|nil
 * @function require
 */
static int _clua_require(lua_State *ls)
{
    const char *file = luaL_checkstring(ls, 1);
    if (!file)
        return 0;

    CLua &vm(CLua::get_vm(ls));
    if (vm.execfile(file, false, false) != 0)
        luaL_error(ls, vm.error.c_str());

    lua_pushboolean(ls, true);
    return 1;
}

/*** Load and execute the named luafile, returning the result.
 * Differs from @{require} in that the file is run for a result. Errors
 * come back on the lua stack and can be handled by the caller.
 * @tparam string filename
 * @return whatever is left on the lua stack by filename
 * @function dofile
 */
static int _clua_dofile(lua_State *ls)
{
    const char *file = luaL_checkstring(ls, 1);
    if (!file)
        return 0;

    const int err = CLua::loadfile(ls, file, !CLua::is_managed_vm(ls));
    if (err)
        return lua_error(ls);

    lua_call(ls, 0, LUA_MULTRET);
    return lua_gettop(ls);
}

string quote_lua_string(const string &s)
{
    return replace_all_of(replace_all_of(s, "\\", "\\\\"), "\"", "\\\"");
}

static string _get_persist_file()
{
    return Options.filename + ".persist";
}

// ///////////////////////////////////////////////////////////////////

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
    return *this;
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
