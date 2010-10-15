/*
 * File:     l_crawl.cc
 * Summary:  General game bindings.
 */

#include "AppHdr.h"

#include "dlua.h"
#include "cluautil.h"
#include "l_libs.h"

#include "cio.h"
#include "command.h"
#include "delay.h"
#include "directn.h"
#include "format.h"
#include "hiscores.h"
#include "initfile.h"
#include "itemname.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "notes.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "random.h"
#include "religion.h"
#include "stuff.h"
#include "view.h"

#ifdef UNIX
#include <sys/time.h>
#include <time.h>
#endif

/////////////////////////////////////////////////////////////////////
// User accessible
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

// Print to stderr for debugging hooks.
LUAFN(crawl_stderr)
{
    const char *text = luaL_checkstring(ls, 1);
    fprintf(stderr, "%s\n", text);
    return (0);
}

LUAWRAP(crawl_delay, delay(luaL_checkint(ls, 1)))
LUAWRAP(crawl_more, more())
LUAWRAP(crawl_flush_prev_message, flush_prev_message())
LUAWRAP(crawl_mesclr, mesclr(lua_isboolean(ls, 1) ? lua_toboolean(ls, 1) : false))
LUAWRAP(crawl_redraw_screen, redraw_screen())

static int crawl_set_more_autoclear(lua_State *ls)
{
    if (lua_isnone(ls, 1))
    {
        luaL_argerror(ls, 1, "needs a boolean argument");
        return (0);
    }
    set_more_autoclear(lua_toboolean(ls, 1));

    return (0);
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

LUARET1(crawl_getch, number, getchm())
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

        for (; *keys; ++keys)
            macro_sendkeys_end_add_expanded(*keys);
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
        macro_sendkeys_end_add_expanded(luaL_checkint(ls, argi));
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

static int crawl_process_keys(lua_State *ls)
{
    if (you_are_delayed() || you.turn_is_over)
    {
        luaL_error(ls, "Cannot currently process new keys");
        return (0);
    }

    const char* keys = luaL_checkstring(ls, 1);

    if (strlen(keys) == 0)
    {
        luaL_argerror(ls, 1, "Must have at least one key to process.");
        return (0);
    }

    command_type cmd = key_to_command(keys[0], KMC_DEFAULT);

    if (cmd == CMD_NO_CMD)
    {
        luaL_argerror(ls, 1, "First key is invalid command");
        return (0);
    }

    flush_input_buffer(FLUSH_BEFORE_COMMAND);
    for (int i = 1, len = strlen(keys); i < len; i++)
        macro_sendkeys_end_add_expanded(keys[i]);

    process_command(cmd);

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
            clua_new_userdata< message_filter* >(ls, MESSF_METATABLE);
    if (mf)
    {
        *mf = new message_filter(num, pattern);
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
LUARET1(crawl_random2, number, random2(luaL_checkint(ls, 1)))
LUARET1(crawl_one_chance_in, boolean, one_chance_in(luaL_checkint(ls, 1)))
LUARET1(crawl_random2avg, number,
        random2avg(luaL_checkint(ls, 1), luaL_checkint(ls, 2)))
LUARET1(crawl_random_range, number,
        random_range(luaL_checkint(ls, 1), luaL_checkint(ls, 2),
                      lua_isnumber(ls, 3)? luaL_checkint(ls, 3) : 1))
LUARET1(crawl_coinflip, boolean, coinflip())
LUARET1(crawl_roll_dice, number,
        lua_gettop(ls) == 1
        ? roll_dice(1, luaL_checkint(ls, 1))
        : roll_dice(luaL_checkint(ls, 1), luaL_checkint(ls, 2)))

static int crawl_is_tiles(lua_State *ls)
{
#ifdef USE_TILE
    lua_pushboolean(ls, true);
#else
    lua_pushboolean(ls, false);
#endif

    return (1);
}

static int crawl_get_command (lua_State *ls)
{
    if (lua_gettop(ls) == 0)
    {
        lua_pushnil(ls);
        return (1);
    }

    command_type cmd = name_to_command(luaL_checkstring(ls, 1));
    lua_pushstring(ls, command_to_string(cmd).c_str());
    return (1);
}

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

static const struct luaL_reg crawl_clib[] =
{
    { "mpr",            crawl_mpr },
    { "formatted_mpr",  crawl_formatted_mpr },
    { "stderr",  crawl_stderr },
    { "more",           crawl_more },
    { "more_autoclear", crawl_set_more_autoclear },
    { "flush_prev_message", crawl_flush_prev_message },
    { "mesclr",         crawl_mesclr },
    { "delay",          crawl_delay },
    { "random2",        crawl_random2 },
    { "one_chance_in",  crawl_one_chance_in },
    { "random2avg"   ,  crawl_random2avg },
    { "coinflip",       crawl_coinflip },
    { "roll_dice",      crawl_roll_dice },
    { "random_range",   crawl_random_range },
    { "random_element", crawl_random_element },
    { "redraw_screen",  crawl_redraw_screen },
    { "c_input_line",   crawl_c_input_line},
    { "getch",          crawl_getch },
    { "yesno",          crawl_yesno },
    { "yesnoquit",      crawl_yesnoquit },
    { "kbhit",          crawl_kbhit },
    { "flush_input",    crawl_flush_input },
    { "sendkeys",       crawl_sendkeys },
    { "process_command", crawl_process_command },
    { "process_keys",   crawl_process_keys },
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
    { "is_tiles",       crawl_is_tiles },
    { "err_trace",      crawl_err_trace },
    { "get_command",    crawl_get_command },

    { NULL, NULL },
};

void cluaopen_crawl(lua_State *ls)
{
    clua_register_metatable(ls, REGEX_METATABLE, crawl_regex_ops,
                            lua_object_gc<text_pattern>);
    clua_register_metatable(ls, MESSF_METATABLE, crawl_messf_ops,
                            lua_object_gc<message_filter>);

    luaL_openlib(ls, "crawl", crawl_clib, 0);
}

/////////////////////////////////////////////////////////////////////
// Non-user-accessible bindings (dlua).
//

LUAFN(_crawl_args)
{
    return clua_stringtable(ls, SysEnv.cmd_args);
}

LUAFN(_crawl_milestone)
{
    mark_milestone(luaL_checkstring(ls, 1),
                   luaL_checkstring(ls, 2),
                   lua_toboolean(ls, 3));
    return (0);
}

LUAFN(_crawl_redraw_view)
{
    viewwindow();
    return (0);
}

LUAFN(_crawl_redraw_stats)
{
    you.wield_change        = true;
    you.redraw_quiver       = true;
    you.redraw_hit_points   = true;
    you.redraw_magic_points = true;
    you.redraw_stats.init(true);
    you.redraw_experience   = true;
    you.redraw_armour_class = true;
    you.redraw_evasion      = true;
    you.redraw_status_flags = 0xFFFFFFFF;

    print_stats();
    return (0);
}


#ifdef UNIX
LUAFN(_crawl_millis)
{
    struct timeval tv;
    struct timezone tz;
    const int error = gettimeofday(&tv, &tz);
    if (error)
        luaL_error(ls, make_stringf("Failed to get time: %s",
                                    strerror(error)).c_str());

    lua_pushnumber(ls, tv.tv_sec * 1000 + tv.tv_usec / 1000);
    return (1);
}
#endif

static std::string _crawl_make_name(lua_State *ls)
{
    // A quick wrapper around itemname:make_name. Seed is random_int().
    // Possible parameters: all caps, max length, char start. By default
    // these are false, -1, and 0 as per make_name.
    bool all_caps = false;
    int maxlen = -1;
    char start = 0;
    if (lua_gettop(ls) >= 1 && lua_isboolean(ls, 1))
        all_caps = lua_toboolean(ls, 1);
    if (lua_gettop(ls) >= 2 && lua_isnumber(ls, 2))
        maxlen = luaL_checkint(ls, 2);
    if (lua_gettop(ls) >= 3 && lua_isstring(ls, 3))
    {
        const char* s = luaL_checkstring(ls, 3);
        if (s && *s)
            start = *s;
    }
    return make_name(random_int(), all_caps, maxlen, start);
}

LUARET1(crawl_make_name, string, _crawl_make_name(ls).c_str())

static int _crawl_god_speaks(lua_State *ls)
{
    if (!crawl_state.io_inited)
        return (0);

    const char *god_name = luaL_checkstring(ls, 1);
    if (!god_name)
    {
        std::string err = "god_speaks requires a god!";
        return (luaL_argerror(ls, 1, err.c_str()));
    }

    god_type god = str_to_god(god_name);
    if (god == GOD_NO_GOD)
    {
        std::string err = make_stringf("'%s' matches no god.", god_name);
        return (luaL_argerror(ls, 1, err.c_str()));
    }

    const char *message = luaL_checkstring(ls, 2);
    if (!message)
        return (0);

    god_speaks(god, message);
    return (0);
}

static const struct luaL_reg crawl_dlib[] =
{
{ "args", _crawl_args },
{ "mark_milestone", _crawl_milestone },
{ "redraw_view", _crawl_redraw_view },
{ "redraw_stats", _crawl_redraw_stats },
{ "god_speaks", _crawl_god_speaks },
#ifdef UNIX
{ "millis", _crawl_millis },
#endif
{ "make_name", crawl_make_name },
{ NULL, NULL }
};

void dluaopen_crawl(lua_State *ls)
{
    luaL_openlib(ls, "crawl", crawl_dlib, 0);
}
