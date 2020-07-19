/*** General crawl related functions, including interaction.
 * @module crawl
 */

#include "AppHdr.h"

#include "l-libs.h"

#include "branch.h"
#include "chardump.h"
#include "cluautil.h"
#include "command.h"
#include "delay.h"
#include "directn.h"
#include "dlua.h"
#include "end.h"
#include "english.h"
#include "fight.h"
#include "hints.h"
#include "initfile.h"
#include "item-name.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "message.h"
#include "notes.h"
#include "output.h"
#include "perlin.h"
#include "prompt.h"
#include "religion.h"
#include "sound.h"
#include "state.h"
#include "state.h"
#include "stringutil.h"
#include "tutorial.h"
#include "unwind.h"
#include "version.h"
#include "view.h"
#include "worley.h"

#ifdef TARGET_OS_WINDOWS
# include "windows.h"
#else
# include <sys/time.h>
# include <time.h>
#endif

//
// User accessible (clua) functions
//

/*** Print a message.
 * @tparam string message message to print
 * @param channel channel to print on; defaults to 0 (<code>MSGCH_PLAIN</code>)
 * @function mpr
 */
static int crawl_mpr(lua_State *ls)
{
    if (!crawl_state.io_inited)
        return 0;

    const char *message = luaL_checkstring(ls, 1);
    if (!message)
        return 0;

    int ch = MSGCH_PLAIN;
    if (lua_isnumber(ls, 2))
        ch = luaL_safe_checkint(ls, 2);
    else
    {
        const char *channel = lua_tostring(ls, 2);
        if (channel)
            ch = str_to_channel(channel);
    }

    if (ch < 0 || ch >= NUM_MESSAGE_CHANNELS)
        ch = MSGCH_PLAIN;

    mprf(static_cast<msg_channel_type>(ch), "%s", message);
    return 0;
}

/*** Print a formatted message.
 * @tparam string message message to print
 * @param channel channel to print on; defaults to 0 (<code>MSGCH_PLAIN</code>)
 * @function formatted_mpr
 */
static int crawl_formatted_mpr(lua_State *ls)
{
    if (!crawl_state.io_inited)
        return 0;

    const char *message = luaL_checkstring(ls, 1);
    if (!message)
        return 0;

    int ch = MSGCH_PLAIN;
    if (lua_isnumber(ls, 2))
        ch = luaL_safe_checkint(ls, 2);
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
    return 0;
}

/*** Print to stderr for debugging hooks.
 * @tparam string text
 * @function stderr
 */
LUAFN(crawl_stderr)
{
    const char *text = luaL_checkstring(ls, 1);
    fprintf(stderr, "%s\n", text);
    return 0;
}

/*** Print to the debugging channel, which is desplayed only for debug builds.
 * @tparam string text
 * @function dpr
 */
LUAFN(crawl_dpr)
{
#ifdef DEBUG_DIAGNOSTICS
    const char *text = luaL_checkstring(ls, 1);
    if (crawl_state.io_inited)
        dprf("%s", text);
#else
    UNUSED(ls);
#endif
    return 0;
}

/*** Delay the display.
 * @tparam int ms delay in milliseconds
 * @function delay
 */
LUAWRAP(crawl_delay, delay(luaL_safe_checkint(ls, 1)))
/*** Display a `--- more ---` prompt
 * @function more
 */
LUAWRAP(crawl_more, more())
/*** Flush the previous message to the window.
 * @function flush_prev_message
 */
LUAWRAP(crawl_flush_prev_message, flush_prev_message())
/*** Clear the message window.
 * @tparam boolean force
 * @function clear_messages
 * */
LUAWRAP(crawl_clear_messages,
clear_messages(lua_isboolean(ls, 1) ? lua_toboolean(ls, 1) : false))
/*** Redraw the screen.
 * @function redraw_screen */
LUAFN(crawl_redraw_screen)
{
    UNUSED(ls);

    redraw_screen();
    update_screen();
    return 0;
}

/*** Toggle autoclearing of `--- more ---` prompts.
 * @tparam boolean flag
 * @function set_more_autoclear
 */
static int crawl_set_more_autoclear(lua_State *ls)
{
    if (lua_isnone(ls, 1))
    {
        luaL_argerror(ls, 1, "needs a boolean argument");
        return 0;
    }
    set_more_autoclear(lua_toboolean(ls, 1));

    return 0;
}

/*** Toggle the use of `--- more ---` prompts.
 * @tparam boolean flag
 * @function enable_more
 */
static int crawl_enable_more(lua_State *ls)
{
    if (lua_isnone(ls, 1))
    {
        luaL_argerror(ls, 1, "needs a boolean argument");
        return 0;
    }
    crawl_state.show_more_prompt = lua_toboolean(ls, 1);

    return 0;
}

/*** Cancellably prompt the user for a line of input.
 * The line is limited to 500 characters.
 * @treturn string|nil a string if one is input or nil if input is cancelled
 * @function c_input_line
 */
static int crawl_c_input_line(lua_State *ls)
{
    char linebuf[500];

    bool valid = !cancellable_get_line(linebuf, sizeof linebuf);
    if (valid)
        lua_pushstring(ls, linebuf);
    else
        lua_pushnil(ls);
    return 1;
}

/*** Prompt the user to choose a location via the targeting screen.
 * This is useful for scripts that require a user-selected target. For example,
 * one could imagine a "mark dangerous monster" script that would place a large
 * exclusion around a user-chosen monster that would then be deleted if the
 * monster moved or died. This function could be used for the user to select
 * a target monster.
 * @treturn int, int the relative position of the chosen location to the user
 * @function get_target
 */
LUAFN(crawl_get_target) {
    coord_def out;

    if (!get_look_position(&out))
        return 0;

    lua_pushinteger(ls, out.x - you.position.x);
    lua_pushinteger(ls, out.y - you.position.y);

    return 2;
}

/*** Get input key (combo).
 * @treturn int the key (combo) input
 * @function getch */
LUARET1(crawl_getch, number, getchm())
/*** Check for pending input.
 * @return int 1 if there is, 0 otherwise
 * @function kbhit
 */
LUARET1(crawl_kbhit, number, kbhit())
/*** Flush the input buffer (typeahead).
 * @function flush_input
 */
LUAWRAP(crawl_flush_input, flush_input_buffer(FLUSH_LUA))

static char _lua_char(lua_State *ls, int ndx, char defval = 0)
{
    return lua_isnone(ls, ndx) || !lua_isstring(ls, ndx)? defval
           : lua_tostring(ls, ndx)[0];
}

/*** Ask the player a yes/no question.
 * The player is supposed to answer by pressing Y or N.
 * @tparam string prompt question for the user
 * @tparam boolean safe accept lowercase answers
 * @tparam[opt] string|nil safeanswer if a letter, this will be considered a
 * safe default
 * @tparam[optchain=true] boolean clear_after clear the question after the user
 * answers
 * @tparam[optchain=true] boolean interrupt_delays interrupt any ongoing delays
 * to ask the question
 * @tparam[optchain=false] boolean noprompt skip asking the question;
 * just wait for the answer
 * @function yesno
 */
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
    return 1;
}

/*** Ask the player a yes/no/quit question.
 * Mostly like yesno(), but doesn't support as many
 * parameters in this Lua binding.
 * @tparam string prompt question for the user
 * @tparam boolean safe accept lowercase answers
 * @tparam[opt] string|nil safeanswer if a letter, this will be considered a
 * safe default
 * @tparam[optchain=false] boolean allow_all actually ask a yes/no/quit/all
 * question
 * @tparam[optchain=true] boolean clear_after clear the question after the user
 * answers
 * @function yesnoquit
 */
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
    return 1;
}

static void crawl_sendkeys_proc(lua_State *ls, int argi)
{
    if (lua_isstring(ls, argi))
    {
        const char *keys = luaL_checkstring(ls, argi);
        if (!keys)
            return;

        char32_t wc;
        while (int len = utf8towc(&wc, keys))
        {
            macro_sendkeys_end_add_expanded(wc);
            keys += len;
        }
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
        macro_sendkeys_end_add_expanded(luaL_safe_checkint(ls, argi));
}

/*** Send keypresses to crawl.
 * A flexible variadic function to send input to crawl. Will process any number
 * of arguments. Each argument can be a string, keycode number, or array of the
 * previous two types. They are processed in left-to-right order.
 * @tparam string|int|array keys
 * @param[opt] ...
 * @function sendkeys
 */
static int crawl_sendkeys(lua_State *ls)
{
    int top = lua_gettop(ls);
    for (int i = 1; i <= top; ++i)
        crawl_sendkeys_proc(ls, i);
    return 0;
}

/*** Tell crawl to process a current macro delay.
 * @return boolean whether it will actually do so
 * @function process_command
 */
static int crawl_process_command(lua_State *ls)
{
    const bool will_process =
        !you_are_delayed() || current_delay()->is_macro();

    if (will_process)
    {
        // This should only be called from a macro delay, but run_macro
        // may not have started the macro delay; do so now.
        if (!you_are_delayed())
            start_delay<MacroDelay>();
        start_delay<MacroProcessKeyDelay>();
    }

    lua_pushboolean(ls, will_process);
    return 1;
}

static bool _check_can_do_command(lua_State *ls)
{
    auto delay = current_delay();
    if (delay && !delay->is_macro())
    {
        luaL_error(ls, "Cannot currently process new keys (%s delay active)",
                   delay->name());
        return false;
    }

    if (you.turn_is_over)
    {
        luaL_error(ls, "Cannot currently process new keys (turn is over)");
        return false;
    }

    return true;
}

/*** Process a string of input keys
 * The first key of the first argument must be a command key in the default key
 * context. The subsequent keys are processed as in sendkeys().
 * @tparam string keys
 * @tparam[opt=false] boolean hide hide targeter while processing
 * @function process_keys
 */
static int crawl_process_keys(lua_State *ls)
{
    if (!_check_can_do_command(ls))
        return 0;

    // if there's pending input, pushing to the end of the buffer may separate
    // the first element of the sequence from the rest. TODO: should this be
    // changed to push the key sequence to the beginning of the buffer? See
    // crawl_do_commands below.
    if (has_pending_input())
    {
        luaL_error(ls,
                "Cannot currently process new keys (there is pending input)");
        return 0;
    }


    const char* keys = luaL_checkstring(ls, 1);

    if (strlen(keys) == 0)
    {
        luaL_argerror(ls, 1, "Must have at least one key to process.");
        return 0;
    }

    command_type cmd = key_to_command(keys[0], KMC_DEFAULT);

    if (cmd == CMD_NO_CMD)
    {
        luaL_argerror(ls, 1, "First key is invalid command");
        return 0;
    }

    unwind_bool gen(crawl_state.invisible_targeting,
            lua_isboolean(ls, 2) && lua_toboolean(ls, 2));

    flush_input_buffer(FLUSH_BEFORE_COMMAND);
    for (int i = 1, len = strlen(keys); i < len; i++)
        macro_sendkeys_end_add_expanded(keys[i]);

    process_command_on_record(cmd);

    return 0;
}

/*** Enable or disable crashing on an incomplete buffer.
 * Used for tests, not generally useful in real life. Crashing only happens in
 * wizmode.
 * @tparam boolean param
 * @function set_sendkeys_errors
 */
static int crawl_set_sendkeys_errors(lua_State *ls)
{
    const bool errors = lua_toboolean(ls, 1);
    crawl_state.nonempty_buffer_flush_errors = errors;
    return 0;
}

/*** Execute a sequence of named crawl commands
 * The array must be the command names as they appear in cmd-name.h, they are
 * processed in order by the macro internal command buffer. The input buffer is
 * flushed before execution.
 * @tparam array commands
 * @tparam[opt=false] boolean hide hide targeter while processing
 * @function do_commands
 */
static int crawl_do_commands(lua_State *ls)
{
    if (!_check_can_do_command(ls))
        return 0;

    unwind_bool gen(crawl_state.invisible_targeting,
                    lua_isboolean(ls, 2) && lua_toboolean(ls, 2));
    if (lua_isboolean(ls, 2))
        lua_pop(ls, 1);
    if (!lua_istable(ls, 1))
    {
        luaL_argerror(ls, 1, "Must be an array");
        return 0;
    }
    map<int,string> command_map; // ordered by key

    lua_pushnil(ls);
    while (lua_next(ls, 1)) // in general, no guaranteed order for this
    {
        if (!lua_isstring(ls, -1))
        {
            luaL_argerror(ls, 1, "Table contains non-string");
            return 0;
        }
        if (!lua_isnumber(ls, -2))
        {
            luaL_argerror(ls, 1, "Must be an array");
            return 0;
        }
        command_map[lua_tonumber(ls, -2)] = lua_tostring(ls, -1);
        lua_pop(ls, 1);
    }

    bool first = true;
    command_type firstcmd = CMD_NO_CMD;
    deque<command_type> cmd_seq;

    for (const auto& pair : command_map)
    {
        const auto& command = pair.second;
        command_type cmd = name_to_command(command);
        if (cmd == CMD_NO_CMD)
        {
            luaL_argerror(ls, 1, ("Invalid command: " + command).c_str());
            return 0;
        }

        if (first)
        {
            firstcmd = cmd;
            first = false;
        }
        else
            cmd_seq.push_front(cmd); // reverse order for adding below
    }

    flush_input_buffer(FLUSH_BEFORE_COMMAND);

    // insert commands to the front of the macro buffer so that they are
    // guaranteed to be processed adjacent to firstcmd
    for (auto c : cmd_seq)
        macro_buf_add_cmd(c, true);

    process_command_on_record(firstcmd);

    return 0;
}

#ifdef USE_SOUND
/*** Play a sound.
 * Only available when crawl is compiled with sound.
 * @tparam string sf filename of sound to play
 * @function playsound
 */
static int crawl_playsound(lua_State *ls)
{
    const char *sf = luaL_checkstring(ls, 1);
    if (!sf)
        return 0;
    play_sound(sf);
    return 0;
}
#endif

/*** Run a macro.
 * @tparam string macroname name of macro to run
 * @function runmacro
 */
static int crawl_runmacro(lua_State *ls)
{
    const char *macroname = luaL_checkstring(ls, 1);
    if (!macroname)
        return 0;
    run_macro(macroname);
    return 0;
}

/*** Set user options from string.
 * @tparam string opt an option string in the same format as <tt>init.txt</tt>/<tt>.crawlrc</tt>
 * @function setopt
 */
static int crawl_setopt(lua_State *ls)
{
    if (!lua_isstring(ls, 1))
        return 0;

    const char *s = lua_tostring(ls, 1);
    if (s)
    {
        // Note that the conditional script can contain nested Lua[ ]Lua code.
        read_options(s, true);
    }

    return 0;
}

/*** Read options from file.
 * @tparam string filename name of file to read from
 * @function read_options */
static int crawl_read_options(lua_State *ls)
{
    if (!lua_isstring(ls, 1))
        return 0;

    const char* filename = lua_tostring(ls, 1);
    Options.include(filename, true, true);
    return 0;
}

static int crawl_bindkey(lua_State *ls)
{
    const char *s = nullptr;
    if (lua_isstring(ls, 1))
        s = lua_tostring(ls, 1);

    if (!s || !lua_isfunction(ls, 2) || lua_gettop(ls) != 2)
        return 0;

    lua_pushvalue(ls, 2);
    string name = clua.setuniqregistry();
    if (lua_gettop(ls) != 2)
    {
        fprintf(stderr, "Stack top has changed!\n");
        lua_settop(ls, 2);
    }
    // TODO: This function is a stub, so this whole binding is.
    macro_userfn(s, name.c_str());
    return 0;
}

/*** Get the number of a message channel.
 * @tparam string name channel name
 * @treturn int channel number
 * @function msgch_num
 */
static int crawl_msgch_num(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);
    if (!s)
        return 0;
    int ch = str_to_channel(s);
    if (ch == -1)
        return 0;

    lua_pushnumber(ls, ch);
    return 1;
}

/*** Get the name of a message channel.
 * @tparam int num channel number
 * @treturn string channel name
 * @function msgch_name
 */
static int crawl_msgch_name(lua_State *ls)
{
    int num = luaL_safe_checkint(ls, 1);
    string name = channel_to_str(num);
    lua_pushstring(ls, name.c_str());
    return 1;
}

/*** Make a note.
 * @tparam string note
 * @function take_note
 */
static int crawl_take_note(lua_State *ls)
{
    const char* msg = luaL_checkstring(ls, 1);
    take_note(Note(NOTE_MESSAGE, 0, 0, msg));
    return 0;
}

/*** Retrieve the message buffer.
 * @tparam int num how many lines back to go
 * @treturn strong
 * @function messages
 */
static int crawl_messages(lua_State *ls)
{
    const int count = luaL_safe_checkint(ls, 1);
    lua_pushstring(ls, get_last_messages(count).c_str());
    return 1;
}

#define REGEX_METATABLE "crawl.regex"
#define MESSF_METATABLE "crawl.messf"

/*** Compile a regular expression
 * @tparam string pat the pattern string (PCRE)
 * @treturn Regex|nil
 * @function regex
 */
static int crawl_regex(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);
    if (!s)
        return 0;

    text_pattern **tpudata =
            clua_new_userdata< text_pattern* >(ls, REGEX_METATABLE);
    if (tpudata)
    {
        *tpudata = new text_pattern(s);
        return 1;
    }
    return 0;
}


/*** Regular expression objects
 * @type Regex
 */
/*** Test against a string.
 * Returns nil if the pattern object does not exist or if a non-string is
 * passed.
 * @tparam string s
 * @treturn boolean|nil
 * @function Regex:find
 */
static int crawl_regex_find(lua_State *ls)
{
    text_pattern **pattern =
            clua_get_userdata< text_pattern* >(ls, REGEX_METATABLE);
    if (!pattern || !*pattern)
    {
        luaL_argerror(ls, 1, "Invalid regex object");
        return 0;
    }

    const char *text = luaL_checkstring(ls, -1);
    if (!text)
        return 0;

    lua_pushboolean(ls, (*pattern)->matches(text));
    return 1;
}

/*** Test equality
 * Tests if the two lua variables are the same underlying compiled pattern
 * @tparam regex pat
 * @treturn boolean
 * @function Regex:equals
 */
static int crawl_regex_equals(lua_State *ls)
{
    text_pattern **pattern =
            clua_get_userdata< text_pattern* >(ls, REGEX_METATABLE);
    text_pattern **arg =
            clua_get_userdata< text_pattern* >(ls, REGEX_METATABLE, 2);

    if (!pattern || !*pattern || !arg || !*arg)
    {
        // TODO: explain which one
        luaL_error(ls, "Invalid regex object");
        return 0;
    }

    lua_pushboolean(ls, **pattern == **arg);
    return 1;
}
static const luaL_reg crawl_regex_ops[] =
{
    { "matches",        crawl_regex_find },
    { "equals",         crawl_regex_equals },
    { nullptr, nullptr }
};
/*** @section end
 */

/*** Create a message filter
 * @tparam string pat filter pattern
 * @tparam[opt=-1] int ch channel number, -1 for anu
 * @treturn MessageFilter|nil
 * @function message_filter
 */
/*** Message filter object.
 * @type MessageFilter
 */
static int crawl_message_filter(lua_State *ls)
{
    const char *pattern = luaL_checkstring(ls, 1);
    if (!pattern)
        return 0;

    int num = lua_isnumber(ls, 2)? luaL_safe_checkint(ls, 2) : -1;
    message_filter **mf =
            clua_new_userdata< message_filter* >(ls, MESSF_METATABLE);
    if (mf)
    {
        *mf = new message_filter(num, pattern);
        return 1;
    }
    return 0;
}

/*** Check a message against a filter.
 * @tparam string msg
 * @tparam int ch message channel
 * @treturn boolean
 * @function MessageFilter:matches
 */
static int crawl_messf_matches(lua_State *ls)
{
    message_filter **mf =
            clua_get_userdata< message_filter* >(ls, MESSF_METATABLE);
    if (!mf || !*mf)
    {
        luaL_argerror(ls, 1, "Invalid message filter object");
        return 0;
    }

    const char *pattern = luaL_checkstring(ls, 2);
    int ch = luaL_safe_checkint(ls, 3);
    if (pattern)
    {
        bool filt = (*mf)->is_filtered(ch, pattern);
        lua_pushboolean(ls, filt);
        return 1;
    }
    return 0;
}

/*** Test equality
 * Tests if the two lua variables are the same underlying compiled pattern
 * @tparam regex pat
 * @treturn boolean
 * @function MessageFilter:equals
 */
static int crawl_messf_equals(lua_State *ls)
{
    message_filter **mf =
            clua_get_userdata< message_filter* >(ls, MESSF_METATABLE);
    message_filter **arg =
            clua_get_userdata< message_filter* >(ls, MESSF_METATABLE, 2);
    if (!mf || !*mf || !arg || !*arg)
    {
        // TODO: explain which one
        luaL_error(ls, "Invalid message filter object");
        return 0;
    }
    lua_pushboolean(ls, **mf == **arg);
    return 1;
}

static const luaL_reg crawl_messf_ops[] =
{
    { "matches",        crawl_messf_matches },
    { "equals",         crawl_messf_equals },
    { nullptr, nullptr }
};
/*** @section end
 */

/*** Trim newlines and trailing whitespace.
 * @tparam string s
 * @treturn string
 * @function trim
 */
static int crawl_trim(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);
    if (!s)
        return 0;
    string text = s;
    trim_string(text);
    lua_pushstring(ls, text.c_str());
    return 1;
}

/*** Split a string at a token.
 * @tparam string s
 * @tparam string split
 * @treturn {string,...}
 * @function split
 */
static int crawl_split(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1),
               *token = luaL_checkstring(ls, 2);
    if (!s || !token)
        return 0;

    vector<string> segs = split_string(token, s);
    if (lua_isboolean(ls, 3) && lua_toboolean(ls, 3))
        reverse(segs.begin(), segs.end());
    lua_newtable(ls);
    for (int i = 0, count = segs.size(); i < count; ++i)
    {
        lua_pushstring(ls, segs[i].c_str());
        lua_rawseti(ls, -2, i + 1);
    }
    return 1;
}

/*** Compare two strings in a locale-independent way.
 * Lua's built in comparison operations for strings are dependent on locale,
 * which isn't always desireable. This is just a wrapper on
 * std::basic_string::compare.
 *
 * @tparam string s1 the first string.
 * @tparam string s2 the second sring.
 * @treturn -1 if s1 < s2, 1 if s2 < s1, 0 if s1 == s2.
 */
static int crawl_string_compare(lua_State *ls)
{
    const string s1 = luaL_checkstring(ls, 1),
                 s2 = luaL_checkstring(ls, 2);
    lua_pushnumber(ls, s1.compare(s2));
    return 1;
}

/*** Grammatically describe something.
 * Crawl provides the following description types:
 *
 *  - "plain": just give the name
 *  - "the": use the definite article
 *  - "a": use the indefinite article
 *  - "your": use the second person possessive
 *  - "its": use the third person possessive
 *  - "worn": how it is equipped
 *  - "inv": describe something carried
 *  - "none": return the empty string
 *
 * And some specific types for partial item naming:
 *
 *  - "base": base name of the item subtype
 *  - "qualname": name without articles, quantities, or enchantments
 *
 * These are used as the allowable values for how.
 * @tparam string what thing to describe
 * @tparam[opt="plain"] string how crawl description type
 * @treturn string grammatical description
 * @function grammar
 */
static int _crawl_grammar(lua_State *ls)
{
    description_level_type ndesc = DESC_PLAIN;
    if (lua_isstring(ls, 2))
        ndesc = description_type_by_name(lua_tostring(ls, 2));
    PLUARET(string, thing_do_grammar(ndesc, luaL_checkstring(ls, 1)).c_str()); }

/*** Correctly attach the article 'a'.
 * @tparam string s
 * @tparam[opt=true] bool lowercase
 * @treturn string
 * @function article_a
 */
static int crawl_article_a(lua_State *ls)
{
    const char *s = luaL_checkstring(ls, 1);

    bool lowercase = true;
    if (lua_isboolean(ls, 2))
        lowercase = lua_toboolean(ls, 2);

    lua_pushstring(ls, article_a(s, lowercase).c_str());

    return 1;
}

/*** Has the game started?
 * @treturn boolean
 * @function game_started
 */
LUARET1(crawl_game_started, boolean, crawl_state.need_save
                                     || crawl_state.map_stat_gen
                                     || crawl_state.obj_stat_gen
                                     || crawl_state.test)
/*** Is crawl asking us to choose a stat?
 * @treturn boolean
 * @function stat_gain_prompt
 */
LUARET1(crawl_stat_gain_prompt, boolean, crawl_state.stat_gain_prompt)
/*** Return a random number from [0, max).
 * @tparam int max
 * @treturn int
 * @function random2
 * */
LUARET1(crawl_random2, number, random2(luaL_safe_checkint(ls, 1)))
/*** Perform a weighted coinflip.
 * @tparam int in
 * @treturn boolean
 * @function one_chance_in
 */
LUARET1(crawl_one_chance_in, boolean, one_chance_in(luaL_safe_checkint(ls, 1)))
/*** Average num random rolls from [0, max).
 * @tparam int max
 * @tparam int num
 * @treturn int
 * @function random2avg
 */
LUARET1(crawl_random2avg, number,
        random2avg(luaL_safe_checkint(ls, 1), luaL_safe_checkint(ls, 2)))
/*** Random number in a range.
 * @tparam int min
 * @tparam int max
 * @tparam[opt=1] int rolls Average over multiple rolls
 * @function random_range
 */
LUARET1(crawl_random_range, number,
        random_range(luaL_safe_checkint(ls, 1), luaL_safe_checkint(ls, 2),
                      lua_isnumber(ls, 3)? luaL_safe_checkint(ls, 3) : 1))
/*** Flip a coin.
 * @treturn boolean
 * @function coinflip
 */
LUARET1(crawl_coinflip, boolean, coinflip())
/*** Roll dice.
 * @tparam[opt=1] int num_dice
 * @tparam int sides
 * @treturn int
 * @function roll_dice
 */
LUARET1(crawl_roll_dice, number,
        lua_gettop(ls) == 1
        ? roll_dice(1, luaL_safe_checkint(ls, 1))
        : roll_dice(luaL_safe_checkint(ls, 1), luaL_safe_checkint(ls, 2)))
/*** Do a random draw.
 * @tparam int x
 * @tparam int y
 * @treturn boolean
 * @function x_chance_in_y
 */
LUARET1(crawl_x_chance_in_y, boolean, x_chance_in_y(luaL_safe_checkint(ls, 1),
                                                    luaL_safe_checkint(ls, 2)))
/*** Random-round integer division.
 * @tparam int numerator
 * @tparam int denominator
 * @treturn int
 * @function div_rand_round
 */
LUARET1(crawl_div_rand_round, number, div_rand_round(luaL_safe_checkint(ls, 1),
                                                     luaL_safe_checkint(ls, 2)))
/*** A random floating point number in [0,1.0)
 * @treturn number
 * @function random_real
 */
LUARET1(crawl_random_real, number, random_real())
/*** Check if the player really wants to use their weapon.
 * @treturn boolean
 * @function weapon_check
 */
LUARET1(crawl_weapon_check, boolean, wielded_weapon_check(you.weapon()))

/*** Get the full Worley noise datum for a given point
 * @tparam number px
 * @tparam number py
 * @tparam number pz
 * @treturn number distance1
 * @treturn number distance2
 * @treturn number id1
 * @treturn number id2
 * @treturn number id1
 * @treturn number pos1x
 * @treturn number pos1y
 * @treturn number pos1z
 * @treturn number pos2x
 * @treturn number pos2y
 * @treturn number pos2z
 * @function worley
 */
static int crawl_worley(lua_State *ls)
{
    double px = lua_tonumber(ls,1);
    double py = lua_tonumber(ls,2);
    double pz = lua_tonumber(ls,3);

    worley::noise_datum n = worley::noise(px,py,pz);
    lua_pushnumber(ls, n.distance[0]);
    lua_pushnumber(ls, n.distance[1]);
    lua_pushnumber(ls, n.id[0]);
    lua_pushnumber(ls, n.id[1]);
    lua_pushnumber(ls, n.pos[0][0]);
    lua_pushnumber(ls, n.pos[0][1]);
    lua_pushnumber(ls, n.pos[0][2]);
    lua_pushnumber(ls, n.pos[1][0]);
    lua_pushnumber(ls, n.pos[1][1]);
    lua_pushnumber(ls, n.pos[1][2]);
    return 10;
}

/*** Difference between nearest point Worley distances.
 * @tparam number px
 * @tparam number py
 * @tparam number pz
 * @treturn number diff
 * @treturn number id1
 * @function worley_diff
 */
static int crawl_worley_diff(lua_State *ls)
{
    double px = lua_tonumber(ls,1);
    double py = lua_tonumber(ls,2);
    double pz = lua_tonumber(ls,3);

    worley::noise_datum n = worley::noise(px,py,pz);
    lua_pushnumber(ls, n.distance[1]-n.distance[0]);
    lua_pushnumber(ls, n.id[0]);

    return 2;
}

/*** Splits a 32-bit integer into four bytes.
 * This is useful in conjunction with worley ids to get four random numbers
 * instead of one from the current node id.
 * @tparam int num
 * @treturn byte
 * @treturn byte
 * @treturn byte
 * @treturn byte
 * @function split_bytes
 */
static int crawl_split_bytes(lua_State *ls)
{
    uint32_t val = lua_tonumber(ls,1);
    uint8_t bytes[4] =
    {
        (uint8_t)(val >> 24),
        (uint8_t)(val >> 16),
        (uint8_t)(val >> 8),
        (uint8_t)(val)
    };
    lua_pushnumber(ls, bytes[0]);
    lua_pushnumber(ls, bytes[1]);
    lua_pushnumber(ls, bytes[2]);
    lua_pushnumber(ls, bytes[3]);
    return 4;
}

/*** 2D-4D Simplex noise.
 * The first two parameters are required for 2D noise, the next two are
 * optional for and specify 3D or 4D, respectively.
 * @tparam number x
 * @tparam number y
 * @tparam[opt] number z
 * @tparam[optchain] number w
 * @treturn number
 * @function simplex
 */
// TODO: Could support octaves here but maybe it can be handled more
// flexibly in lua
static int crawl_simplex(lua_State *ls)
{
    int dims = 0;
    double vals[4];
    if (lua_isnumber(ls,1))
    {
        vals[dims] = lua_tonumber(ls,1);
        dims++;
    }
    if (lua_isnumber(ls,2))
    {
        vals[dims] = lua_tonumber(ls,2);
        dims++;
    }
    if (lua_isnumber(ls,3))
    {
        vals[dims] = lua_tonumber(ls,3);
        dims++;
    }
    if (lua_isnumber(ls,4))
    {
        vals[dims] = lua_tonumber(ls,4);
        dims++;
    }

    double result;
    switch (dims)
    {
    case 2:
        result = perlin::noise(vals[0],vals[1]);
        break;
    case 3:
        result = perlin::noise(vals[0],vals[1],vals[2]);
        break;
    case 4:
        result = perlin::noise(vals[0],vals[1],vals[2],vals[3]);
        break;
    default:
        return 0; // TODO: Throw error?
    }
    lua_pushnumber(ls, result);

    return 1;
}

/*** Are we running under tiles?
 * @treturn boolean
 * @function is_tiles
 */
static int crawl_is_tiles(lua_State *ls)
{
    lua_pushboolean(ls, is_tiles());

    return 1;
}

/*** Are we running under webtiles?
 * Note: returns true if the crawl binary is a webtiles build, even if the
 * player is currently using console.
 * @treturn boolean
 * @function is_webtiles
 */
static int crawl_is_webtiles(lua_State *ls)
{
#ifdef USE_TILE_WEB
    lua_pushboolean(ls, true);
#else
    lua_pushboolean(ls, false);
#endif

    return 1;
}

/*** Are we using the touch ui?
 * @treturn boolean
 * @function is_touch_ui
 */
static int crawl_is_touch_ui(lua_State *ls)
{
#ifdef TOUCH_UI
    lua_pushboolean(ls, true);
#else
    lua_pushboolean(ls, false);
#endif

    return 1;
}

/*** Look up the current key bound to a command.
 * @tparam string name Name as in cmd-name.h
 * @treturn string|nil
 * @function get_command
 */
static int crawl_get_command(lua_State *ls)
{
    if (lua_gettop(ls) == 0)
    {
        lua_pushnil(ls);
        return 1;
    }

    const command_type cmd = name_to_command(luaL_checkstring(ls, 1));

    string cmd_name = command_to_string(cmd, true);
    if (strcmp(cmd_name.c_str(), "<") == 0)
        cmd_name = "<<";

    lua_pushstring(ls, cmd_name.c_str());
    return 1;
}

LUAWRAP(crawl_endgame, screen_end_game(luaL_checkstring(ls, 1)))
LUAWRAP(crawl_tutorial_skill, set_tutorial_skill(luaL_checkstring(ls, 1), luaL_safe_checkint(ls, 2)))
LUAWRAP(crawl_tutorial_hint, tutorial_init_hint(luaL_checkstring(ls, 1)))
LUAWRAP(crawl_print_hint, print_hint(luaL_checkstring(ls, 1)))

/*** Lua error trace a call
 * Attempts to call-trace a lua function that is producing an error.
 * Returns normally if the function runs normally, otherwise sends
 * @param function
 * @param args
 * @return the result of the call if successful
 * @function err_trace
 */
static int crawl_err_trace(lua_State *ls)
{
    const int nargs = lua_gettop(ls);
    const int err = lua_pcall(ls, nargs - 1, LUA_MULTRET, 0);

    if (err)
    {
        // This code from lua.c:traceback() (mostly)
        (void) lua_tostring(ls, 1);
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

    return lua_gettop(ls);
}

static int crawl_tutorial_msg(lua_State *ls)
{
    const char *key = luaL_checkstring(ls, 1);
    if (!key)
        return 0;
    tutorial_msg(key, lua_isboolean(ls, 2) && lua_toboolean(ls, 2));
    return 0;
}

/*** Produce a character dump
 * @function dump_char
 */
LUAWRAP(crawl_dump_char, dump_char(you.your_name, true))

/*** Call lua in dungeon (dlua) context.
 *
 * @tparam string chunk code run
 * @return a scalar return value from dlua context
 * @function call_dlua
 */
#ifdef WIZARD
static int crawl_call_dlua(lua_State *ls)
{
    if (!you.wizard)
        luaL_error(ls, "This function is wizard mode only.");

    const char* code = luaL_checkstring(ls, 1);
    if (!code)
        return 0;

    luaL_loadbuffer(dlua, code, strlen(code), "call_dlua");
    int status = lua_pcall(dlua, 0, LUA_MULTRET, 0);

    if (status)
    {
        if (!lua_isnil(dlua, -1))
        {
            const char *msg = lua_tostring(dlua, -1);
            if (msg == nullptr)
                msg = "(error object is not a string)";
            mprf(MSGCH_ERROR, "%s", msg);
        }

        lua_settop(dlua, 0); // don't bother unwinding, just nuke the stack
        return 0;
    }

    if (lua_gettop(dlua) > 0)
    {
        // TODO: shuttle things other than a single scalar value
        if (lua_isnil(dlua, -1))
            lua_pushnil(ls);
        else if (lua_isboolean(dlua, -1))
            lua_pushboolean(ls, lua_toboolean(dlua, -1));
        else if (lua_isnumber(dlua, -1))
            lua_pushnumber(ls, lua_tonumber(dlua, -1));
        else if (const char *ret = lua_tostring(dlua, -1))
            lua_pushstring(ls, ret);
        else
        {
            mprf(MSGCH_ERROR, "call_dlua: cannot pass non-scalars yet (TODO)");
            lua_pushnil(ls);
        }

        lua_settop(dlua, 0); // clear the stack
        return 1;
    }

    return 0;
}
#endif

/*** Get the crawl version
 * The optional argument is a string from "long", "major", or "short" to
 * determine which version type to return. The argument * check is
 * case-insensitive.
 * @tparam[opt="long"] string fmt
 * @treturn string
 * @function version
 */
static int crawl_version(lua_State *ls)
{
    string type = "long";
    if (lua_gettop(ls) > 0)
    {
        const char *ltype = luaL_checkstring(ls, 1);
        if (ltype)
            type = lowercase_string(ltype);
    }

    if (type == "long")
        lua_pushstring(ls, Version::Long);
    else if (type == "short")
        lua_pushstring(ls, Version::Short);
    else if (type == "major")
        lua_pushstring(ls, Version::Major);
    else
    {
        luaL_argerror(ls, 1,
                      "must be a string \"long\", \"short\", or \"major\""); return 0;
    }
    return 1;
}

static const struct luaL_reg crawl_clib[] =
{
    { "mpr",                crawl_mpr },
    { "formatted_mpr",      crawl_formatted_mpr },
    { "dpr",                crawl_dpr },
    { "stderr",             crawl_stderr },
    { "more",               crawl_more },
    { "more_autoclear",     crawl_set_more_autoclear },
    { "enable_more",        crawl_enable_more },
    { "flush_prev_message", crawl_flush_prev_message },
    { "clear_messages",     crawl_clear_messages },
    { "delay",              crawl_delay },
    { "random2",            crawl_random2 },
    { "one_chance_in",      crawl_one_chance_in },
    { "random2avg",         crawl_random2avg },
    { "coinflip",           crawl_coinflip },
    { "roll_dice",          crawl_roll_dice },
    { "x_chance_in_y",      crawl_x_chance_in_y },
    { "random_range",       crawl_random_range },
    { "div_rand_round",     crawl_div_rand_round },
    { "random_real",        crawl_random_real },
    { "worley",             crawl_worley },
    { "worley_diff",        crawl_worley_diff },
    { "split_bytes",        crawl_split_bytes },
    { "simplex",            crawl_simplex },

    { "redraw_screen",      crawl_redraw_screen },
    { "c_input_line",       crawl_c_input_line},
    { "get_target",         crawl_get_target },
    { "getch",              crawl_getch },
    { "yesno",              crawl_yesno },
    { "yesnoquit",          crawl_yesnoquit },
    { "kbhit",              crawl_kbhit },
    { "flush_input",        crawl_flush_input },
    { "sendkeys",           crawl_sendkeys },
    { "process_command",    crawl_process_command },
    { "process_keys",       crawl_process_keys },
    { "set_sendkeys_errors", crawl_set_sendkeys_errors },
    { "do_commands",        crawl_do_commands },
#ifdef USE_SOUND
    { "playsound",          crawl_playsound },
#endif
    { "runmacro",           crawl_runmacro },
    { "bindkey",            crawl_bindkey },
    { "setopt",             crawl_setopt },
    { "read_options",       crawl_read_options },
    { "msgch_num",          crawl_msgch_num },
    { "msgch_name",         crawl_msgch_name },
    { "take_note",          crawl_take_note },
    { "messages",           crawl_messages },
    { "regex",              crawl_regex },
    { "message_filter",     crawl_message_filter },
    { "trim",               crawl_trim },
    { "split",              crawl_split },
    { "string_compare",     crawl_string_compare },
    { "grammar",            _crawl_grammar },
    { "article_a",          crawl_article_a },
    { "game_started",       crawl_game_started },
    { "stat_gain_prompt",   crawl_stat_gain_prompt },
    { "is_tiles",           crawl_is_tiles },
    { "is_webtiles",        crawl_is_webtiles },
    { "is_touch_ui",        crawl_is_touch_ui },
    { "err_trace",          crawl_err_trace },
    { "get_command",        crawl_get_command },
    { "endgame",            crawl_endgame },
    { "tutorial_msg",       crawl_tutorial_msg },
    { "dump_char",          crawl_dump_char },
#ifdef WIZARD
    { "call_dlua",          crawl_call_dlua },
#endif
    { "version",            crawl_version },
    { "weapon_check",       crawl_weapon_check},
    { nullptr, nullptr },
};

void cluaopen_crawl(lua_State *ls)
{
    clua_register_metatable(ls, REGEX_METATABLE, crawl_regex_ops,
                            lua_object_gc<text_pattern>);
    clua_register_metatable(ls, MESSF_METATABLE, crawl_messf_ops,
                            lua_object_gc<message_filter>);

    luaL_openlib(ls, "crawl", crawl_clib, 0);
}

//
// Non-user-accessible bindings (dlua).
//

/*** Get the commandline arguments
 * @within dlua
 * @treturn table
 * @function args
 */
LUAFN(_crawl_args)
{
    return clua_stringtable(ls, SysEnv.cmd_args);
}

/*** Mark a milestone
 * Register a dgl milestone. No op if not a dgl game.
 * @within dlua
 * @tparam string type
 * @tparam string milestone
 * @tparam string origin
 * @function milestone
 */
LUAFN(_crawl_milestone)
{
    mark_milestone(luaL_checkstring(ls, 1),
                   luaL_checkstring(ls, 2),
                   luaL_checkstring(ls, 3));
    return 0;
}

/*** Redraw the viewwindow.
 * You probably want @{redraw_screen} unless you specifically want only the
 * view window.
 * @within dlua
 * @function redraw_view
 */
LUAFN(_crawl_redraw_view)
{
    UNUSED(ls);

    viewwindow();
    update_screen();
    return 0;
}

/*** Redraw the player stats.
 * You probably want @{redraw_screen} unless you specifically want only the
 * player stats.
 * @within dlua
 * @function redraw_stats
 */
LUAFN(_crawl_redraw_stats)
{
    UNUSED(ls);

    you.wield_change         = true;
    you.redraw_title         = true;
    you.redraw_quiver        = true;
    you.redraw_hit_points    = true;
    you.redraw_magic_points  = true;
    you.redraw_stats.init(true);
    you.redraw_experience    = true;
    you.redraw_armour_class  = true;
    you.redraw_evasion       = true;
    you.redraw_status_lights = true;

    print_stats();
    update_screen();
    return 0;
}

/*** Current milliseconds.
 * Gives the current milliseconds of the time of day.
 * @within dlua
 * @treturn int
 * @function millis
 */
LUAFN(_crawl_millis)
{
#ifdef TARGET_OS_WINDOWS
    // MSVC has no gettimeofday().
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t tt = ft.dwHighDateTime;
    tt <<= 32;
    tt |= ft.dwLowDateTime;
    tt /= 10000;
    tt -= 11644473600000ULL;
    lua_pushnumber(ls, tt);
#else
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    lua_pushnumber(ls, tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
    return 1;
}
static string _crawl_make_name(lua_State */*ls*/)
{
    // A quick wrapper around itemname:make_name.
    return make_name();
}


/*** Make an item name at random.
 * @within dlua
 * @treturn string
 * @function make_name
 */
LUARET1(crawl_make_name, string, _crawl_make_name(ls).c_str())

/* Check that a Lua argument is a god name, and store that god's enum in
 *  a variable.
 *
 *  @param argno  The Lua argument number of the god name. Evaluated once.
 *  @param godvar An existing writable lvalue to hold the enumeration value
 *                of the god. Evaluated zero or one times.
 *  @param fn     The identifier to use as the function name in an error
 *                message. Should generally be the Lua name of the calling
 *                function. Stringified, not evaluated.
 *
 *  @post If argument (argno) was not a string containing a valid god name,
 *        we returned from the calling function with a Lua argument error.
 *        godvar may or may not have been evaluated and/or assigned to.
 *  @post If argument (argno) was a valid god name, godvar was evaluated
 *        exactly once and assigned that god's enum value.
 */
#define CHECK_GOD_ARG(argno, godvar, fn) do                              \
    {                                                                    \
        int _cg_arg = (argno);                                           \
        const char *_cg_name = luaL_checkstring(ls, _cg_arg);            \
        if (!_cg_name)                                                   \
            return luaL_argerror(ls, _cg_arg, #fn " requires a god!");   \
        if (((godvar) = str_to_god(_cg_name)) == GOD_NO_GOD)             \
        {                                                                \
            return luaL_argerror(ls, _cg_arg,                            \
                       make_stringf("'%s' matches no god.",              \
                                    _cg_name).c_str());                  \
        }                                                                \
    } while (0)

/*** Check if a god is still available.
 * For gods that are no longer appearing in new games, and Jiyva if its been
 * killed by the player.
 * @within dlua
 * @tparam string godname
 * @treturn boolean
 * @function unavailable_god
 */
LUAFN(_crawl_unavailable_god)
{
    god_type god = GOD_NO_GOD;
    CHECK_GOD_ARG(1, god, unavailable_god);
    lua_pushboolean(ls, is_unavailable_god(god));
    return 1;
}

/*** Divine voices.
 * @within dlua
 * @tparam string Name of a current crawl god.
 * @tparam string Speach
 * @function god_speaks
 */
LUAFN(_crawl_god_speaks)
{
    if (!crawl_state.io_inited)
        return 0;

    god_type god = GOD_NO_GOD;
    CHECK_GOD_ARG(1, god, god_speaks);

    const char *message = luaL_checkstring(ls, 2);
    if (!message)
        return 0;

    god_speaks(god, message);
    return 0;
}

/*** Set Max Runes
 * Modify the total number of obtainable runes.
 * @within dlua
 * @tparam int nrune
 * @function set_max_runes
 */
LUAFN(_crawl_set_max_runes)
{
    int max_runes = luaL_safe_checkint(ls, 1);
    if (max_runes < 0 || max_runes > NUM_RUNE_TYPES)
        luaL_error(ls, make_stringf("Bad number of max runes: %d", max_runes).c_str());
    else
        you.obtainable_runes = max_runes;
    return 0;
}

LUAWRAP(_crawl_mark_game_won, crawl_state.mark_last_game_won())

LUAFN(crawl_hints_type)
{
    if (crawl_state.game_is_tutorial())
        lua_pushstring(ls, "tutorial");
    else if (!crawl_state.game_is_hints())
        lua_pushstring(ls, "");
    else
        switch (Hints.hints_type)
        {
        case HINT_BERSERK_CHAR:
            lua_pushstring(ls, "berserk");
            break;
        case HINT_RANGER_CHAR:
            lua_pushstring(ls, "ranger");
            break;
        case HINT_MAGIC_CHAR:
            lua_pushstring(ls, "magic");
            break;
        default:
            die("invalid hints_type");
        }
    return 1;
}

LUAFN(crawl_rng_wrap)
{
    if (!lua_isstring(ls, 2))
        luaL_error(ls, "rng_wrap missing rng name");
    string rng_name = lua_tostring(ls, 2);
    if (!rng_name.size())
        luaL_error(ls, "rng_wrap missing rng name");
    rng::rng_type r = rng::NUM_RNGS;
    if (rng_name == "gameplay")
        r = rng::GAMEPLAY;
    else if (rng_name == "ui")
        r = rng::UI;
    else if (rng_name == "system_specific")
        r = rng::SYSTEM_SPECIFIC;
    else if (rng_name == "subgenerator")
        r = rng::SUB_GENERATOR;
    else
    {
        branch_type b = NUM_BRANCHES;
        if ((b = branch_by_shortname(rng_name)) == NUM_BRANCHES)
            if ((b = branch_by_abbrevname(rng_name)) == NUM_BRANCHES)
                luaL_error(ls, "Unknown rng name %s", rng_name.c_str());
        r = rng::get_branch_generator(b);
    }

    lua_pop(ls, 1); // get rid of the rng name
    if (!lua_isfunction(ls, 1))
        luaL_error(ls, "rng_wrap missing function");
    int result;
    if (r == rng::SUB_GENERATOR)
    {
        rng::subgenerator subgen; // TODO: implement seed + seq?
        result = lua_pcall(ls, 0, LUA_MULTRET, 0);
    }
    else
    {
        rng::generator gen(r); // generator to use
        result = lua_pcall(ls, 0, LUA_MULTRET, 0);
    }
    if (result != 0)
        luaL_error(ls, "Failed to run rng-wrapped function (%d)", result);
    return lua_gettop(ls);
}

static const struct luaL_reg crawl_dlib[] =
{
{ "args", _crawl_args },
{ "mark_milestone", _crawl_milestone },
{ "redraw_view", _crawl_redraw_view },
{ "redraw_stats", _crawl_redraw_stats },
{ "god_speaks", _crawl_god_speaks },
{ "millis", _crawl_millis },
{ "make_name", crawl_make_name },
{ "set_max_runes", _crawl_set_max_runes },
{ "tutorial_skill",  crawl_tutorial_skill },
{ "tutorial_hint",   crawl_tutorial_hint },
{ "print_hint", crawl_print_hint },
{ "mark_game_won", _crawl_mark_game_won },
{ "hints_type", crawl_hints_type },
{ "unavailable_god", _crawl_unavailable_god },
{ "rng_wrap", crawl_rng_wrap },

{ nullptr, nullptr }
};

void dluaopen_crawl(lua_State *ls)
{
    luaL_openlib(ls, "crawl", crawl_dlib, 0);
}
