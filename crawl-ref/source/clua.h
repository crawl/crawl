/*
 *  File:       clua.h
 *  Created by: dshaligram on Wed Aug 2 12:54:15 2006 UTC
 */


#ifndef __CLUA_H__
#define __CLUA_H__

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <cstdio>
#include <cstdarg>
#include <string>
#include <map>
#include <set>

#ifndef CLUA_MAX_MEMORY_USE
#define CLUA_MAX_MEMORY_USE (6 * 1024)
#endif

class CLua;

class lua_stack_cleaner
{
public:
    lua_stack_cleaner(lua_State *_ls) : ls(_ls), top(lua_gettop(_ls)) { }
    ~lua_stack_cleaner() { lua_settop(ls, top); }
private:
    lua_State *ls;
    int top;
};

class lua_call_throttle
{
public:
    lua_call_throttle(CLua *handle);
    ~lua_call_throttle();

    static CLua *find_clua(lua_State *ls);

private:
    CLua *lua;

    typedef std::map<lua_State *, CLua *> lua_clua_map;
    static lua_clua_map lua_map;
};

class lua_shutdown_listener
{
public:
    virtual ~lua_shutdown_listener();
    virtual void shutdown(CLua &lua) = 0;
};

// A convenience class to keep a reference to a lua object on the stack.
// This is useful to hang on to things that cannot be directly retrieved by
// C++ code, such as Lua function references.
class lua_datum : public lua_shutdown_listener
{
public:
    lua_datum(CLua &lua, int stackpos = -1, bool pop = true);
    lua_datum(const lua_datum &other);

    const lua_datum &operator = (const lua_datum &other);

    void shutdown(CLua &lua);

    ~lua_datum();

    // Push the datum onto the Lua stack.
    void push() const;

    bool is_table() const;
    bool is_function() const;
    bool is_number() const;
    bool is_string() const;
    bool is_udata() const;

public:
    CLua &lua;

private:
    bool need_cleanup;

private:
    void set_from(const lua_datum &o);
    void cleanup();
};

class CLua
{
public:
    CLua(bool managed = true);
    ~CLua();

    static CLua &get_vm(lua_State *);

    lua_State *state();

    operator lua_State * ()
    {
        return state();
    }

    void save(writer &outf);
    void gc();

    void setglobal(const char *name);
    void getglobal(const char *name);

    // Assigns the value on top of the stack to a unique name in the registry
    // and returns the name.
    std::string setuniqregistry();

    void setregistry(const char *name);
    void getregistry(const char *name);

    int loadbuffer(const char *buf, size_t size, const char *context);
    int loadstring(const char *str, const char *context);
    int execstring(const char *str, const char *context = "init.txt",
                   int nresults = 0);
    int execfile(const char *filename,
                 bool trusted = false,
                 bool die_on_fail = false,
                 bool force = false);

    void pushglobal(const std::string &name);

    maybe_bool callmbooleanfn(const char *fn, const char *params, ...);
    bool callbooleanfn(bool defval, const char *fn, const char *params, ...);
    bool callfn(const char *fn, int nargs, int nret = 1);
    bool callfn(const char *fn, const char *params, ...);
    void fnreturns(const char *params, ...);
    bool runhook(const char *hook, const char *params, ...);

    void add_shutdown_listener(lua_shutdown_listener *);
    void remove_shutdown_listener(lua_shutdown_listener *);

    static int file_write(lua_State *ls);
    static int loadfile(lua_State *ls, const char *file,
                        bool trusted = false, bool die_on_fail = false);
    static bool is_path_safe(std::string file, bool trusted = false);

    static bool is_managed_vm(lua_State *ls);

    void print_stack();

public:
    std::string error;

    // If managed_vm is set, we have to take steps to control badly-behaved
    // scripts.
    bool managed_vm;
    bool shutting_down;
    int throttle_unit_lines;
    int throttle_sleep_ms;
    int throttle_sleep_start, throttle_sleep_end;
    int n_throttle_sleeps;
    int mixed_call_depth;
    int lua_call_depth;
    int max_mixed_call_depth;
    int max_lua_call_depth;

    long memory_used;

    static const int MAX_THROTTLE_SLEEPS = 100;

private:
    lua_State *_state;
    typedef std::set<std::string> sfset;
    sfset sourced_files;
    unsigned int uniqindex;

    std::vector<lua_shutdown_listener*> shutdown_listeners;

private:
    void init_lua();
    void set_error(int err, lua_State *ls = NULL);
    void load_cmacro();
    void load_chooks();
    void init_throttle();

    static void _getregistry(lua_State *, const char *name);

    void vfnreturns(const char *par, va_list va);

    bool proc_returns(const char *par) const;

    bool calltopfn(lua_State *ls, const char *format, va_list args,
                   int retc = -1, va_list *fnr = NULL);
    maybe_bool callmbooleanfn(const char *fn, const char *params,
                              va_list args);

    int push_args(lua_State *ls, const char *format, va_list args,
                    va_list *cpto = NULL);
    int return_count(lua_State *ls, const char *format);

    struct CLuaSave
    {
        const char *filename;
        FILE *handle;

        FILE *get_file();
    };

    friend class lua_call_throttle;
};

class lua_text_pattern : public base_pattern
{
public:
    lua_text_pattern(const std::string &pattern);
    ~lua_text_pattern();

    bool valid() const;
    bool matches(const std::string &s) const;

    static bool is_lua_pattern(const std::string &s);

private:
    bool        translated;
    bool        isvalid;
    std::string pattern;
    std::string lua_fn_name;

    static unsigned int lfndx;

    bool translate() const;
    void pre_pattern(std::string &pat, std::string &fn) const;
    void post_pattern(std::string &pat, std::string &fn) const;

    static std::string new_fn_name();
};

// Defined in main.cc
#ifdef DEBUG_GLOBALS
#define clua (*real_clua)
#endif
extern CLua clua;

void lua_set_exclusive_item(const item_def *item = NULL);

std::string quote_lua_string(const std::string &s);

#endif
