/**
 * @file
 * @brief Crude macro-capability
**/

/*
 * The macro-implementation works like this:
 *   - getchm() works by reading characters from an internal
 *     buffer. If none are available, new characters are read into
 *     the buffer with _getch_mul().
 *   - _getch_mul() reads at least one character, but will read more
 *     if available (determined using kbhit(), which should be defined
 *     in the platform specific libraries).
 *   - Before adding the characters read into the buffer, any keybindings
 *     in the sequence are replaced (see macro_add_buf_long for the
 *     details).
 *
 * (When the above text mentions characters, it actually means int).
 */

#include "AppHdr.h"

#include "macro.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifdef USE_TILE_LOCAL
#include <SDL.h>
#include <SDL_keycode.h>
#endif

#include "cio.h"
#include "command.h"
#include "files.h"
#include "initfile.h"
#include "libutil.h"
#include "menu.h"
#include "message.h"
#include "misc.h" // erase_val
#include "options.h"
#include "output.h"
#include "prompt.h"
#include "state.h"
#include "state.h"
#include "stringutil.h"
#include "syscalls.h"
#include "unicode.h"
#include "version.h"

typedef deque<int> keybuf;
typedef map<keyseq,keyseq> macromap;

static macromap Keymaps[KMC_CONTEXT_COUNT];
static macromap Macros;

static macromap *all_maps[] =
{
    &Keymaps[KMC_DEFAULT],
    &Keymaps[KMC_LEVELMAP],
    &Keymaps[KMC_TARGETING],
    &Keymaps[KMC_CONFIRM],

    &Macros,
};

static keybuf Buffer;
static keybuf SendKeysBuffer;

#define USERFUNCBASE -10000
static vector<string> userfunctions;

static vector<key_recorder*> recorders;

typedef map<string, int> name_to_cmd_map;
typedef map<int, string> cmd_to_name_map;

struct command_name
{
    command_type cmd;
    const char*  name;
};

static command_name _command_name_list[] =
{
#include "cmd-name.h"
};

static name_to_cmd_map _names_to_cmds;
static cmd_to_name_map _cmds_to_names;

struct default_binding
{
    int           key;
    command_type  cmd;
};

static default_binding _default_binding_list[] =
{
#include "cmd-keys.h"
};

typedef map<int, int> key_to_cmd_map;
typedef map<int, int> cmd_to_key_map;

static key_to_cmd_map _keys_to_cmds[KMC_CONTEXT_COUNT];
static cmd_to_key_map _cmds_to_keys[KMC_CONTEXT_COUNT];

static inline int userfunc_index(int key)
{
    int index = (key <= USERFUNCBASE ? USERFUNCBASE - key : -1);
    return index < 0 || index >= (int) userfunctions.size()? -1 : index;
}

static int userfunc_index(const keyseq &seq)
{
    if (seq.empty())
        return -1;

    return userfunc_index(seq.front());
}

bool is_userfunction(int key)
{
    return userfunc_index(key) != -1;
}

static bool is_userfunction(const keyseq &seq)
{
    return userfunc_index(seq) != -1;
}

string get_userfunction(int key)
{
    int index = userfunc_index(key);
    return index == -1 ? nullptr : userfunctions[index];
}

static string get_userfunction(const keyseq &seq)
{
    int index = userfunc_index(seq);
    return index == -1 ? nullptr : userfunctions[index];
}

static bool userfunc_referenced(int index, const macromap &mm)
{
    for (const auto &entry : mm)
        if (userfunc_index(entry.second) == index)
            return true;

    return false;
}

static bool userfunc_referenced(int index)
{
    for (auto m : all_maps)
        if (userfunc_referenced(index, *m))
            return true;

    return false;
}

// Expensive function to discard unused function names
static void userfunc_collectgarbage()
{
    for (int i = userfunctions.size() - 1; i >= 0; --i)
    {
        if (!userfunctions.empty() && !userfunc_referenced(i))
            userfunctions[i].clear();
    }
}

static int userfunc_getindex(const string &fname)
{
    if (fname.length() == 0)
        return -1;

    userfunc_collectgarbage();

    // Pass 1 to see if we have this string already
    for (int i = 0, count = userfunctions.size(); i < count; ++i)
    {
        if (userfunctions[i] == fname)
            return i;
    }

    // Pass 2 to hunt for gaps.
    for (int i = 0, count = userfunctions.size(); i < count; ++i)
    {
        if (userfunctions[i].empty())
        {
            userfunctions[i] = fname;
            return i;
        }
    }

    userfunctions.push_back(fname);
    return userfunctions.size() - 1;
}

// Returns the name of the file that contains macros.
static string get_macro_file()
{
    string dir = !Options.macro_dir.empty() ? Options.macro_dir :
                 !SysEnv.crawl_dir.empty()  ? SysEnv.crawl_dir : "";

    if (!dir.empty())
    {
#ifndef DGL_MACRO_ABSOLUTE_PATH
        if (dir[dir.length() - 1] != FILE_SEPARATOR)
            dir += FILE_SEPARATOR;
#endif
    }

#if defined(DGL_MACRO_ABSOLUTE_PATH)
    return dir.empty()? "macro.txt" : dir;
#endif

    check_mkdir("Macro directory", &dir, true);

#if defined(DGL_NAMED_MACRO_FILE)
    return dir + strip_filename_unsafe_chars(you.your_name) + "-macro.txt";
#else
    return dir + "macro.txt";
#endif
}

static void buf2keyseq(const char *buff, keyseq &k)
{
    // Sanity check
    if (!buff)
        return;

    // convert c_str to keyseq

    // Check for user functions
    if (*buff == '=' && buff[1] == '=' && buff[2] == '=' && buff[3])
    {
        int index = userfunc_getindex(buff + 3);
        if (index != -1)
            k.push_back(USERFUNCBASE - index);
    }
    else
    {
        const int len = strlen(buff);
        for (int i = 0; i < len; i++)
            k.push_back(buff[i]);
    }
}

int function_keycode_fixup(int keycode)
{
    // is this harmless on windows console?
#if !defined(USE_TILE_LOCAL) && TAG_MAJOR_VERSION == 34
    // For many years, dcss has (accidentally, it seems) used these keycodes
    // for function keys, because of a patch from 2009 that mapped some common
    // terminal escape codes for F1-F4 to 1011-1014 under the belief (??) that
    // these were used for some numpad keys. In webtiles code, these keycodes
    // are even hardcoded in to the non-versioned part of the js code, so it's
    // extremely hard to change. So we do this somewhat horrible fixup to deal
    // with the complicated history. TODO: remove some day
    switch (keycode)
    {
    case -1011: return CK_F1;
    case -1012: return CK_F2;
    case -1013: return CK_F3;
    case -1014: return CK_F4;
    case -1015: return CK_F5;
    case -1016: return CK_F6;
    case -1017: return CK_F7;
    case -1018: return CK_F8;
    case -1019: return CK_F9;
    case -1020: return CK_F10;
    default:
        return keycode;
    }
#else
    return keycode;
#endif
}

static int read_key_code(string s)
{
    if (s.empty())
        return 0;

    int base = 10;
    if (s[0] == 'x')
    {
        s = s.substr(1);
        base = 16;
    }
    else if (s[0] == '^')
    {
        // ^A = 1, etc.
        return 1 + toupper_safe(s[1]) - 'A';
    }

    char *tail;
    return strtol(s.c_str(), &tail, base);
}

/*
 * Takes as argument a string, and returns a sequence of keys described
 * by the string. Most characters produce their own ASCII code. These
 * are the cases:
 *   \\ produces the ASCII code of a single \
 *   \{123} produces 123 (decimal)
 *   \{^A}  produces 1 (Ctrl-A)
 *   \{x40} produces 64 (hexadecimal code)
 *   \{!more} or \{!m} disables -more- prompt until the end of the macro.
 */
keyseq parse_keyseq(string s)
{
    // TODO parse readable descriptions of special keys, e.g. \{F1} or something
    int state = 0;
    keyseq v;

    if (starts_with(s, "==="))
    {
        buf2keyseq(s.c_str(), v);
        return v;
    }

    bool more_reset = false;
    for (int i = 0, size = s.length(); i < size; ++i)
    {
        char c = s[i];

        switch (state)
        {
        case 0: // Normal state
            if (c == '\\')
                state = 1;
            else
                v.push_back(c);
            break;

        case 1: // Last char is a '\'
            if (c == '\\')
            {
                state = 0;
                v.push_back(c);
            }
            else if (c == '{')
                state = 2;
            // XXX Error handling
            break;

        case 2: // Inside \{}
        {
            const string::size_type clb = s.find('}', i);
            if (clb == string::npos)
                break;

            const string arg = s.substr(i, clb - i);
            if (!more_reset && (arg == "!more" || arg == "!m"))
            {
                more_reset = true;
                v.push_back(KEY_MACRO_MORE_PROTECT);
            }
            else
            {
                const int key = function_keycode_fixup(read_key_code(arg));
                v.push_back(key);
            }

            state = 0;
            i = clb;
            break;
        }
        }
    }

    return v;
}

/*
 * Serialises a key sequence into a string of the format described
 * above.
 */
static string vtostr(const keyseq &seq)
{
    ostringstream s;

    const keyseq *v = &seq;
    keyseq dummy;
    if (is_userfunction(seq))
    {
        // Laboriously reconstruct the original user input
        string newseq = string("==") + get_userfunction(seq);
        buf2keyseq(newseq.c_str(), dummy);
        dummy.push_front('=');

        v = &dummy;
    }

    for (auto key : *v)
    {
        if (key <= 32 || key > 127)
        {
            if (key == KEY_MACRO_MORE_PROTECT)
                s << "\\{!more}";
            else
            {
                char buff[20];
                // TODO: get rid of negative values in user-facing uses of
                // keycodes
                snprintf(buff, sizeof(buff), "\\{%d}", key);
                s << buff;
            }
        }
        else if (key == '\\')
            s << "\\\\";
        else
            s << static_cast<char>(key);
    }

    return s.str();
}

/*
 * Add a macro (surprise, surprise).
 */
static void macro_add(macromap &mapref, keyseq key, keyseq action)
{
    mapref[key] = action;
}

/*
 * Remove a macro.
 */
static bool macro_del(macromap &mapref, keyseq key)
{
    return mapref.erase(key) != 0;
}

static void _register_expanded_keys(int num, bool reverse);

// Safely add macro-expanded keystrokes to the end of Crawl's keyboard
// buffer. Macro-expanded keystrokes will be held in a separate
// keybuffer until the primary keybuffer is empty, at which point
// they'll be added to the primary keybuffer.
void macro_sendkeys_end_add_expanded(int key)
{
    SendKeysBuffer.push_back(key);
}

static void _macro_inject_sent_keys()
{
    if (Buffer.empty() && !SendKeysBuffer.empty())
    {
        // Transfer keys from SendKeysBuffer to the main Buffer as
        // expanded keystrokes and empty SendKeysBuffer.
        macro_buf_add(SendKeysBuffer, false, true);
        SendKeysBuffer.clear();
    }
}

/*
 * Safely add a command to the end of the sendkeys keybuffer.
 */
void macro_sendkeys_end_add_cmd(command_type cmd)
{
    ASSERT_RANGE(cmd, CMD_NO_CMD + 1, CMD_MIN_SYNTHETIC);

    // There should be plenty of room between the synthetic keys
    // (KEY_MACRO_MORE_PROTECT == -10) and USERFUNCBASE (-10000) for
    // command_type to fit (currently 1000 through 2069).
    macro_sendkeys_end_add_expanded(-((int) cmd));
}

/*
 * Adds keypresses from a sequence into the internal keybuffer. Ignores
 * macros.
 */
void macro_buf_add(const keyseq &actions, bool reverse, bool expanded)
{
    keyseq act;
    bool need_more_reset = false;
    for (auto key : actions)
    {
        if (key == KEY_MACRO_MORE_PROTECT)
        {
            key = KEY_MACRO_DISABLE_MORE;
            need_more_reset = true;
        }
        act.push_back(key);
    }
    if (need_more_reset)
        act.push_back(KEY_MACRO_ENABLE_MORE);

    Buffer.insert(reverse? Buffer.begin() : Buffer.end(),
                   act.begin(), act.end());

    if (expanded)
        _register_expanded_keys(act.size(), reverse);
}

/*
 * Adds a single keypress into the internal keybuffer.
 */
void macro_buf_add(int key, bool reverse, bool expanded)
{
    if (reverse)
        Buffer.push_front(key);
    else
        Buffer.push_back(key);
    if (expanded)
        _register_expanded_keys(1, reverse);
}

/*
 * Add a command into the internal keybuffer.
 */
void macro_buf_add_cmd(command_type cmd, bool reverse)
{
    ASSERT_RANGE(cmd, CMD_NO_CMD + 1, CMD_MIN_SYNTHETIC);

    // There should be plenty of room between the synthetic keys
    // (KEY_MACRO_MORE_PROTECT == -10) and USERFUNCBASE (-10000) for
    // command_type to fit (currently 1000 through 2069).
    macro_buf_add(-((int) cmd), reverse, true);
}

/*
 * Adds keypresses from a sequence into the internal keybuffer. Does some
 * O(N^2) analysis to the sequence to apply keymaps.
 */
static void macro_buf_add_long(keyseq actions,
                               macromap &keymap = Keymaps[KMC_DEFAULT])
{
    keyseq tmp;

    // debug << "Adding: " << vtostr(actions) << endl;
    // debug.flush();

    // Check whether any subsequences of the sequence are macros.
    // The matching starts from as early as possible, and is
    // as long as possible given the first constraint. I.e from
    // the sequence "abcdef" and macros "ab", "bcde" and "de"
    // "ab" and "de" are recognised as macros.

    while (!actions.empty())
    {
        tmp = actions;

        while (!tmp.empty())
        {
            auto subst = keymap.find(tmp);
            // Found a macro. Add the expansion (action) of the
            // macro into the buffer.
            if (subst != keymap.end() && !subst->second.empty())
            {
                macro_buf_add(subst->second, false, false);
                break;
            }

            // Didn't find a macro. Remove a key from the end
            // of the sequence, and try again.
            tmp.pop_back();
        }

        if (tmp.empty())
        {
            // Didn't find a macro. Add the first keypress of the sequence
            // into the buffer, remove it from the sequence, and try again.
            macro_buf_add(actions.front(), false, false);
            actions.pop_front();
        }
        else
        {
            // Found a macro, which has already been added above. Now just
            // remove the macroed keys from the sequence.
            for (unsigned int i = 0; i < tmp.size(); i++)
                actions.pop_front();
        }
    }
}

// Number of keys from start of buffer that have already gone through
// macro expansion.
static int expanded_keys_left = 0;
static void _register_expanded_keys(int num, bool reverse)
{
    expanded_keys_left += num;
    if (!reverse)
    {
        // We added at the end of the buffer, so the whole buffer had
        // better be full of expanded keys.
        ASSERT((int)Buffer.size() == expanded_keys_left);
        expanded_keys_left = Buffer.size();
    }
}

static int macro_keys_left = -1;

void macro_clear_buffers()
{
    crawl_state.cancel_cmd_repeat();
    crawl_state.cancel_cmd_again();

    Buffer.clear();
    SendKeysBuffer.clear();
    expanded_keys_left = 0;
    macro_keys_left = -1;

    crawl_state.show_more_prompt = true;
}

bool is_processing_macro()
{
    return macro_keys_left >= 0;
}

bool has_pending_input()
{
    return !Buffer.empty() || !SendKeysBuffer.empty();
}

/*
 * Command macros are only applied from the immediate front of the
 * buffer, and only when the game is expecting a command.
 */
static void macro_buf_apply_command_macro()
{
    if (macro_keys_left > 0 || expanded_keys_left > 0)
        return;

    keyseq tmp = Buffer;

    // find the longest match from the start of the buffer and replace it
    while (!tmp.empty())
    {
        auto expansion = Macros.find(tmp);

        if (expansion != Macros.end() && !expansion->second.empty())
        {
            const keyseq &result = expansion->second;

            // Found macro, remove match from front:
            for (unsigned int i = 0; i < tmp.size(); i++)
                Buffer.pop_front();

            macro_keys_left = result.size();

            macro_buf_add(result, true, true);

            break;
        }

        tmp.pop_back();
    }
}

/*
 * Removes the earliest keypress from the keybuffer, and returns its
 * value. If buffer was empty, returns -1;
 */
int macro_buf_get()
{
    ASSERT(macro_keys_left <= (int)Buffer.size()
           && expanded_keys_left <= (int)Buffer.size());

    _macro_inject_sent_keys();

    if (Buffer.empty())
    {
        // If we're trying to fetch a new keystroke, then the processing
        // of the previous keystroke is complete.
        if (macro_keys_left == 0)
            macro_keys_left = -1;

        return -1;
    }

    int key = Buffer.front();
    Buffer.pop_front();

    if (macro_keys_left >= 0)
        macro_keys_left--;
    if (expanded_keys_left > 0)
        expanded_keys_left--;

    for (key_recorder *recorder : recorders)
        recorder->add_key(key);

    return key;
}

void process_command_on_record(command_type cmd)
{
    const int key = command_to_key(cmd);
    if (key != '\0')
        for (key_recorder *recorder : recorders)
            recorder->add_key(key);
    process_command(cmd);
}

static void write_map(FILE *f, const macromap &mp, const char *key)
{
    for (const auto &entry : mp)
    {
        // Need this check, since empty values are added into the
        // macro struct for all used keyboard commands.
        if (entry.second.size())
        {
            fprintf(f, "%s%s\nA:%s\n\n", OUTS(key),
                OUTS(vtostr(entry.first)), OUTS(vtostr(entry.second)));
        }
    }
}

/*
 * Saves macros into the macrofile, overwriting the old one.
 */
void macro_save()
{
    FILE *f;
    const string macrofile = get_macro_file();
    f = fopen_u(macrofile.c_str(), "w");
    if (!f)
    {
        mprf(MSGCH_ERROR, "Couldn't open %s for writing!", macrofile.c_str());
        return;
    }

    fprintf(f, "# %s %s macro file\n"
               "# WARNING: This file is entirely auto-generated.\n"
               "\n"
               "# Key Mappings:\n",
            OUTS(CRAWL), // ok, localizing the game name is not likely
            OUTS(Version::Long)); // nor the version string
    for (int mc = KMC_DEFAULT; mc < KMC_CONTEXT_COUNT; ++mc)
    {
        char buf[30] = "K:";
        if (mc)
            snprintf(buf, sizeof buf, "K%d:", mc);
        write_map(f, Keymaps[mc], buf);
    }

    fprintf(f, "# Command Macros:\n");
    write_map(f, Macros, "M:");

    crawl_state.unsaved_macros = false;
    fclose(f);
}

/*
 * Reads as many keypresses as are available (waiting for at least one),
 * and returns them as a single keyseq.
 */
static keyseq _getch_mul()
{
    keyseq keys;
    int    a;

    // Something's gone wrong with replaying keys if crawl needs to
    // get new keys from the user.
    if (crawl_state.is_replaying_keys())
    {
        mprf(MSGCH_ERROR, "(Key replay ran out of keys)");
        crawl_state.cancel_cmd_repeat();
        crawl_state.cancel_cmd_again();
    }

    // The a == 0 test is legacy code that I don't dare to remove. I
    // have a vague recollection of it being a kludge for conio support.
    do
    {
        a = getch_ck();
        if (a != CK_NO_KEY)
            keys.push_back(a);
    }
    while (keys.size() == 0 || ((kbhit() || a == 0) && a != CK_REDRAW));

    return keys;
}

/*
 * Replacement for getch(). Returns keys from the key buffer if available.
 * If not, adds some content to the buffer, and returns some of it.
 */
int getchm(KeymapContext mc)
{
    int a;

    // Got data from buffer.
    if ((a = macro_buf_get()) != -1)
        return a;

    // Read some keys...
    keyseq keys = _getch_mul();
    macro_buf_add_with_keymap(keys, mc);
    return macro_buf_get();
}

void macro_buf_add_with_keymap(keyseq keys, KeymapContext mc)
{
    if (mc == KMC_NONE)
        macro_buf_add(keys, false, false);
    else
        macro_buf_add_long(keys, Keymaps[mc]);
}

/**
 * Get a character?
 */
int get_ch()
{
    mouse_control mc(MOUSE_MODE_PROMPT);
    int gotched = getchm();

    if (gotched == 0)
        gotched = getchm();

    return gotched;
}

/*
 * Replacement for getch(). Returns keys from the key buffer if available.
 * If not, adds some content to the buffer, and returns some of it.
 */
int getch_with_command_macros()
{
    _macro_inject_sent_keys();

    if (Buffer.empty())
    {
        keyseq keys = _getch_mul();
        macro_buf_add_with_keymap(keys, KMC_DEFAULT);
    }

    // Apply longest matching macro at front of buffer:
    macro_buf_apply_command_macro();

    return macro_buf_get();
}

static string _buffer_to_string()
{
    string s;
    for (const int k : Buffer)
    {
        if (k > 0 && k <= numeric_limits<unsigned char>::max())
        {
            char c = static_cast<unsigned char>(k);
            if (c == '[' || c == ']')
                s += "\\";
            s += c;
        }
        else
            s += make_stringf("[%d]", k);
    }
    return s;
}

/*
 * Flush the buffer. Later we'll probably want to give the player options
 * as to when this happens (ex. always before command input, casting failed).
 */
void flush_input_buffer(int reason)
{
    ASSERT(reason != FLUSH_KEY_REPLAY_CANCEL
           || crawl_state.is_replaying_keys() || crawl_state.cmd_repeat_start);

    ASSERT(reason != FLUSH_ABORT_MACRO || is_processing_macro());

    // Any attempt to flush means that the processing of the previously
    // fetched keystroke is complete.
    if (macro_keys_left == 0)
        macro_keys_left = -1;

    if (crawl_state.is_replaying_keys() && reason != FLUSH_ABORT_MACRO
        && reason != FLUSH_KEY_REPLAY_CANCEL
        && reason != FLUSH_REPLAY_SETUP_FAILURE
        && reason != FLUSH_ON_FAILURE)
    {
        return;
    }

    if (Options.flush_input[ reason ] || reason == FLUSH_ABORT_MACRO
        || reason == FLUSH_KEY_REPLAY_CANCEL
        || reason == FLUSH_REPLAY_SETUP_FAILURE
        || reason == FLUSH_REPEAT_SETUP_DONE)
    {
        if (crawl_state.nonempty_buffer_flush_errors && !Buffer.empty())
        {
            if (you.wizard) // crash -- intended for tests
            {
                mprf(MSGCH_ERROR,
                    "Flushing non-empty key buffer (Buffer is '%s')",
                    _buffer_to_string().c_str());
                ASSERT(Buffer.empty());
            }
            else
                mprf(MSGCH_ERROR, "Flushing non-empty key buffer");
        }
        while (!Buffer.empty())
        {
            const int key = Buffer.front();
            Buffer.pop_front();
            if (key == KEY_MACRO_ENABLE_MORE)
                crawl_state.show_more_prompt = true;
        }
        macro_keys_left = -1;
        expanded_keys_left = 0;
    }
}

static string _macro_prompt_string(const string &macro_type)
{
    return make_stringf("Input %s action: ", macro_type.c_str());
}

static void _macro_prompt(const string &macro_type)
{
    msgwin_prompt(_macro_prompt_string(macro_type));
}

static void _input_action_raw(const string &macro_type, keyseq* action)
{
    _macro_prompt(macro_type);
    const int x = wherex();
    const int y = wherey();
    bool done = false;

    while (!done)
    {
        cgotoxy(x, y);
        cprintf("%s", vtostr(*action).c_str());

        int input = getch_ck();

        switch (input)
        {
        CASE_ESCAPE
            done = true;
            *action = keyseq();
            break;

        case '\n':
        case '\r':
            done = true;
            break;

        default:
            action->push_back(input);
            break;
        }
    }

    msgwin_reply(vtostr(*action));
}

static void _input_action_text(const string &macro_type, keyseq* action)
{
    char buff[1024];
    msgwin_get_line_autohist(_macro_prompt_string(macro_type),
                             buff, sizeof(buff));
    *action = parse_keyseq(buff);
}

static string _macro_type_name(bool keymap, KeymapContext keymc)
{
    return make_stringf("%s%s",
                        keymap ? (keymc == KMC_DEFAULT    ? "default " :
                                  keymc == KMC_LEVELMAP   ? "level-map " :
                                  keymc == KMC_TARGETING  ? "targeting " :
                                  keymc == KMC_CONFIRM    ? "confirm " :
                                  keymc == KMC_MENU       ? "menu "
                                  : "buggy") : "",
                        (keymap ? "keymap" : "macro"));
}

class MacroEditMenu : public Menu
{
public:
    MacroEditMenu(macromap &_mapref, bool _keymap, KeymapContext _keymc)
        : Menu(MF_SINGLESELECT | MF_ALLOW_FORMATTING),
          selected_new_key(false),
          mapref(_mapref), keymc(_keymc), is_keymap(_keymap)
    {
        set_tag("macros");
        action_cycle = Menu::CYCLE_TOGGLE;
        menu_action  = Menu::ACT_EXECUTE;
    }

    virtual formatted_string calc_title() override
    {
        return formatted_string::parse_string("Editing <w>"
            + _macro_type_name(is_keymap, keymc) + "</w>. <w>[Enter]</w> to add/edit by key, <w>-</w> to clear all, other keys add");
    }

    bool process_key(int keyin) override
    {
        if (keyin == CK_ENTER || keyin == '-')
        {
            lastch = keyin;
            return false;
        }
        switch (keyin)
        {
        case CK_REDRAW:
        case CK_MOUSE_B2:
        case CK_MOUSE_CMD:
        CASE_ESCAPE
        case ' ': case CK_PGDN: case '>': case '+':
        case CK_MOUSE_B1:
        case CK_MOUSE_CLICK:
        case CK_PGUP: case '<':
        case CK_UP:
        case CK_DOWN:
        case CK_HOME:
        case CK_END:
            return Menu::process_key(keyin);
        default:
            selected_new_key = true;
            // fallthrough
        case CK_ENTER:
        case '-':
            lastch = keyin;
            return false;
        }
    }
    bool selected_new_key;
protected:
    macromap &mapref;
    KeymapContext keymc;
    bool is_keymap;
};

void macro_add_query()
{
    int input;
    bool keymap = false;
    KeymapContext keymc = KMC_DEFAULT;

    // TODO: roll this all into a single menu with subsections

    clear_messages();
    // TODO: add a (r)eset option in case you mess up?
    mprf(MSGCH_PROMPT, "(m)acro, keymap "
                       "[(k) default, (x) level-map, (t)argeting, "
                       "(c)onfirm, m(e)nu], (s)ave? ");
    input = getch_ck();
    int low = toalower(input);

    if (low == 'k')
    {
        keymap = true;
        keymc  = KMC_DEFAULT;
    }
    else if (low == 'x')
    {
        keymap = true;
        keymc  = KMC_LEVELMAP;
    }
    else if (low == 't')
    {
        keymap = true;
        keymc  = KMC_TARGETING;
    }
    else if (low == 'c')
    {
        keymap = true;
        keymc  = KMC_CONFIRM;
    }
    else if (low == 'e')
    {
        keymap = true;
        keymc  = KMC_MENU;
    }
    else if (low == 'm')
        keymap = false;
    else if (input == 's')
    {
        mpr("Saving macros.");
        macro_save();
        return;
    }
    else
    {
        mpr("Aborting.");
        return;
    }

    // reference to the appropriate mapping
    macromap &mapref = (keymap ? Keymaps[keymc] : Macros);

    MacroEditMenu menu(mapref, keymap, keymc);
    menu.set_title(new MenuEntry("", MEL_TITLE));
    for (auto &mapping : mapref)
    {
        // TODO: indicate if macro is from rc file somehow?
        string action_str = vtostr(mapping.second);
        action_str = replace_all(action_str, "<", "<<");
        MenuEntry *me = new MenuEntry(action_str,
                                                MEL_ITEM, 1,
                                                (int) mapping.first[0]);
        me->data = (void *) &mapping.first;
        menu.add_entry(me);
    }

    keyseq *key_chosen = nullptr;
    menu.on_single_selection = [&key_chosen](const MenuEntry& item)
    {
        key_chosen = static_cast<keyseq *>(item.data);
        return false;
    };
    menu.show();

    keyseq key;

    mouse_control mc(MOUSE_MODE_MACRO);
    const string macro_type = _macro_type_name(keymap, keymc);

    if (!key_chosen)
    {
        if (menu.getkey() == CK_ENTER)
        {
            const string trigger_prompt = make_stringf(
                "Input %s trigger key (<white>\\</white> to enter by keycode): ",
                macro_type.c_str());
            msgwin_prompt(trigger_prompt);
            key = _getch_mul();
            if (key[0] == '\\')
            {
                msgwin_reply("keycode");
                key.clear();
                char buf[20];
                msgwin_get_line("Input keycode: ", buf, sizeof(buf));
                key.push_back(read_key_code(string(buf)));
                if (key[0] == 0)
                {
                    canned_msg(MSG_OK);
                    return;
                }
            }
            else
                msgwin_reply(vtostr(key));
        }
        else if (menu.getkey() == '-')
        {
            const string clear_prompt = make_stringf("Really clear all %ss?",
                macro_type.c_str());
            if (yesno(clear_prompt.c_str(), false, 'N'))
            {
                mapref = macromap();
                mprf("All %ss cleared!", macro_type.c_str());
                crawl_state.unsaved_macros = true;
            }
            else
                canned_msg(MSG_OK);
            return;
        }
        else if (menu.selected_new_key)
            key.push_back(menu.getkey());
        else
            return;
    }
    else
        key = *key_chosen;

    // TODO: menu-ify (or at least do in a popup) the rest of this stuff:

    string key_str = keycode_is_printable(key[0])
        ? keycode_to_name(key[0]).c_str()
        : make_stringf("%s (%s)",
                vtostr(key).c_str(), keycode_to_name(key[0]).c_str());
    key_str = replace_all(key_str, "<", "<<");

    const bool starts_empty = !mapref.count(key) || mapref[key].empty();
    string action_str;
    if (!starts_empty)
    {
        action_str = vtostr(mapref[key]);
        action_str = replace_all(action_str, "<", "<<");
    }
    else
        action_str = "<red>[none]</red>";

    mprf(MSGCH_WARN, "Current Action for %s: %s",
                                key_str.c_str(), action_str.c_str());
    mprf(MSGCH_PROMPT, "Do you wish to (r)edefine, %s%sor (a)bort? ",
        keymap ? "" : "redefine (R)aw, ",
        starts_empty ? "" : "(c)lear, ");

    input = getch_ck();

    input = toalower(input);
    if (!starts_empty && input == 'c')
    {
        mprf("Cleared %s '%s' => '%s'.",
             macro_type.c_str(),
             key_str.c_str(),
             action_str.c_str());
        macro_del(mapref, key);
        crawl_state.unsaved_macros = true;
        return;
    }
    else if (input != 'r' && input != 'R')
    {
        canned_msg(MSG_OK);
        return;
    }

    keyseq action;
    if (input == 'R' && !keymap) // why isn't raw input mode used for keymaps?
        _input_action_raw(macro_type, &action);
    else
        _input_action_text(macro_type, &action);

    if (action.empty())
    {
        const bool deleted_macro = macro_del(mapref, key);
        if (deleted_macro)
        {
            mprf("Deleted %s for '%s'.",
                 macro_type.c_str(),
                 key_str.c_str());
        }
        else
            canned_msg(MSG_OK);
    }
    else
    {
        string new_action_str = vtostr(action);
        new_action_str = replace_all(new_action_str, "<", "<<");
        macro_add(mapref, key, action);
        mprf("Created %s '%s' => '%s'.",
             macro_type.c_str(), key_str.c_str(), new_action_str.c_str());
    }

    crawl_state.unsaved_macros = true;
    redraw_screen();
    update_screen();
}

/*
 * Initialises the macros.
 */
static void _read_macros_from(const char* filename)
{
    if (!file_exists(filename))
        return;

    string s;
    FileLineInput f(filename);
    keyseq key, action;
    bool keymap = false;
    KeymapContext keymc = KMC_DEFAULT;

    while (!f.eof())
    {
        s = f.get_line();
        trim_string(s);  // remove white space from ends

        if (s.empty() || s[0] == '#')
            continue;    // skip comments
        else if (s.substr(0, 2) == "K:")
        {
            key = parse_keyseq(s.substr(2));
            keymap = true;
            keymc  = KMC_DEFAULT;
        }
        else if (s.length() >= 3 && s[0] == 'K' && s[2] == ':')
        {
            const KeymapContext ctx = KeymapContext(KMC_DEFAULT + s[1] - '0');
            if (ctx >= KMC_DEFAULT && ctx < KMC_CONTEXT_COUNT)
            {
                key    = parse_keyseq(s.substr(3));
                keymap = true;
                keymc  = ctx;
            }
        }
        else if (s.substr(0, 2) == "M:")
        {
            key = parse_keyseq(s.substr(2));
            keymap = false;
        }
        else if (s.substr(0, 2) == "A:")
        {
            action = parse_keyseq(s.substr(2));
            macro_add((keymap ? Keymaps[keymc] : Macros), key, action);
        }
    }
}

/*
 * Based on _read_macros_from(), reads macros or keymaps from rc files.
 *
 * @param field The orig_field (not lowercased) of the macro option (the
 * bit after "macros +=")
 *
 * @return The string of any errors which occurred, or "" if no error.
 * %s is the field argument.
 */

string read_rc_file_macro(const string& field)
{
    const int first_space = field.find(' ');

    if (first_space < 0)
        return "Cannot parse marcos += %s , there is only one argument";

    // Start by deciding what context the macro/keymap is in
    const string context = field.substr(0, first_space);

    bool keymap = false;
    KeymapContext keymc = KMC_DEFAULT;

    if (context == "K")
    {
        keymap = true;
        keymc  = KMC_DEFAULT;
    }
    else if (context.length() >= 2 && context[0] == 'K')
    {
        const KeymapContext ctx =
              KeymapContext(KMC_DEFAULT + context[1] - '0');

        if (ctx >= KMC_DEFAULT && ctx < KMC_CONTEXT_COUNT)
        {
            keymap = true;
            keymc  = ctx;
        }
    }
    else if (context == "M")
        keymap = false;
    else
        return "'" + context
                   + "' is not a valid macro or keymap context (macros += %s)";

    // Now grab the key and action to be performed
    const string key_and_action = field.substr((first_space + 1));

    const int second_space = key_and_action.find(' ');

    if (second_space < 0)
        return "Cannot parse marcos += %s , there are only two arguments";

    const string macro_key_string = key_and_action.substr(0, second_space);
    const string action_string = key_and_action.substr((second_space + 1));


    keyseq key = parse_keyseq(macro_key_string);

    keyseq action = parse_keyseq(action_string);

    macro_add((keymap ? Keymaps[keymc] : Macros), key, action);

    // If we didn't save here, macros in rc files would be saved iff you also
    // changed another macro with cntrl-D and saved at the exit prompt.
    // Consistent behavior works better.
    macro_save();

    return "";
}

// useful for debugging
string keyseq_to_str(const keyseq &seq)
{
    string s = "";
    for (auto k : seq)
    {
        if (k == '\n' || k == '\r')
            s += "newline";
        else if (k == '\t')
            s += "tab";
        else
            s += (char) k;
        s += ", ";
    }
    return s.size() == 0 ? s : s.substr(0, s.size() - 2);

}

bool keycode_is_printable(int keycode)
{
    switch (keycode)
    {
    case  0:
    case  8:
    case  9:
    case 27:
    case '\n':
    case '\r':
        return false;
    default:
#ifdef USE_TILE_LOCAL
        // the upper bound here is based on a comment in
        // windowmanager-sdl.cc:_translate_keysym. It could potentially be
        // applied more generally but I'm concerned about non-US keyboard
        // layouts etc. I'm also not sure how accurate it is for sdl...
        return keycode >= 32 && keycode < 256;
#else
        return keycode >= 32;
#endif
    }
}

string keycode_to_name(int keycode)
{
    // TODO: handling of alt keys in SDL is generally a mess, including here
    // (they are basically just ignored)
    if (keycode_is_printable(keycode))
        return string(1, keycode);

    // placeholder
    switch (keycode)
    {
    case  0: return "NULL";
    case  8: return "Backspace"; // CK_BKSP
    case  9: return "Tab";
    case 27: return "Esc";
    case '\n':
    case '\r': // CK_ENTER
        return "Enter";
    case CK_DELETE: return "Del";
    case CK_UP:     return "Up";
    case CK_DOWN:   return "Down";
    case CK_LEFT:   return "Left";
    case CK_RIGHT:  return "Right";
    case CK_INSERT: return "Ins";
    case CK_HOME:   return "Home";
    case CK_CLEAR:  return "Clear";
    case CK_PGUP:   return "PgUp";
    case CK_PGDN:   return "PgDn";
    // shift/ctrl-modified keys aside from shift-tab don't seem to work on mac
    // console, and are somewhat spotty on webtiles.
    case CK_SHIFT_UP:     return "Shift-Up";
    case CK_SHIFT_DOWN:   return "Shift-Down";
    case CK_SHIFT_LEFT:   return "Shift-Left";
    case CK_SHIFT_RIGHT:  return "Shift-Right";
    case CK_SHIFT_INSERT: return "Shift-Ins";
    case CK_SHIFT_HOME:   return "Shift-Home";
    case CK_SHIFT_CLEAR:  return "Shift-Clear";
    case CK_SHIFT_PGUP:   return "Shift-PgUp";
    case CK_SHIFT_PGDN:   return "Shift-PgDn";
    case CK_SHIFT_TAB:    return "Shift-Tab";
    case CK_CTRL_UP:      return "^Up";
    case CK_CTRL_DOWN:    return "^Down";
    case CK_CTRL_LEFT:    return "^Left";
    case CK_CTRL_RIGHT:   return "^Right";
    case CK_CTRL_INSERT:  return "^Ins";
    case CK_CTRL_HOME:    return "^Home";
    case CK_CTRL_CLEAR:   return "^Clear";
    case CK_CTRL_PGUP:    return "^PgUp";
    case CK_CTRL_PGDN:    return "^PgDn";
    case CK_CTRL_TAB:     return "^Tab";
    case CK_F0:     return "F0";
    case CK_F1:     return "F1";
    case CK_F2:     return "F2";
    case CK_F3:     return "F3";
    case CK_F4:     return "F4";
    case CK_F5:     return "F5";
    case CK_F6:     return "F6";
    case CK_F7:     return "F7";
    case CK_F8:     return "F8";
    case CK_F9:     return "F9";
    case CK_F10:    return "F10";
    case CK_F11:    return "F11";
    case CK_F12:    return "F12";
    default:
#ifdef USE_TILE_LOCAL
        // SDL uses 1 << 30 to indicate non-printable keys, crawl uses negative
        // numbers; convert back to plain SDL form
        if (keycode < 0)
            keycode = -keycode;
        // SDL_GetKeyName strips capitalization, so we don't want to use it for
        // printable keys.
        return string(SDL_GetKeyName(keycode));
#else
    {
        if (keycode >= CONTROL('A') && keycode <= CONTROL('Z'))
            return make_stringf("^%c", UNCONTROL(keycode));

        keyseq v;
        v.push_back(keycode);
        return vtostr(v);
    }
#endif
    }
}

void macro_init()
{
    for (const auto &fn : Options.additional_macro_files)
        _read_macros_from(fn.c_str());

    _read_macros_from(get_macro_file().c_str());
}

void macro_userfn(const char *keys, const char *regname)
{
    UNUSED(keys, regname);
    // TODO: Implement.
    // Converting 'keys' to a key sequence is the difficulty. Doing it portably
    // requires a mapping of key names to whatever getch() spits back, unlikely
    // to happen in a hurry.
}

bool is_synthetic_key(int key)
{
    switch (key)
    {
    case KEY_MACRO_ENABLE_MORE:
    case KEY_MACRO_DISABLE_MORE:
    case KEY_MACRO_MORE_PROTECT:
#ifdef USE_TILE
    case CK_MOUSE_CMD:
    case CK_REDRAW:
#endif
        return true;
    default:
        return false;
    }
}

key_recorder::key_recorder()
    : paused(false)
{
    keys.clear();
}

void key_recorder::add_key(int key, bool reverse)
{
    if (paused)
        return;

    if (reverse)
        keys.push_front(key);
    else
        keys.push_back(key);
}

void key_recorder::clear()
{
    keys.clear();
}

pause_all_key_recorders::pause_all_key_recorders()
{
    for (key_recorder *rec : recorders)
    {
        prev_pause_status.push_back(rec->paused);
        rec->paused = true;
    }
}

pause_all_key_recorders::~pause_all_key_recorders()
{
    for (unsigned int i = 0; i < recorders.size(); i++)
        recorders[i]->paused = prev_pause_status[i];
}

void add_key_recorder(key_recorder* recorder)
{
    ASSERT(find(begin(recorders), end(recorders), recorder) == end(recorders));
    recorders.push_back(recorder);
}

void remove_key_recorder(key_recorder* recorder)
{
    erase_val(recorders, recorder);
}

int get_macro_buf_size()
{
    return Buffer.size();
}

///////////////////////////////////////////////////////////////
// Keybinding stuff

#define VALID_BIND_COMMAND(cmd) (cmd > CMD_NO_CMD && cmd < CMD_MIN_SYNTHETIC)

void init_keybindings()
{
    int i;

    for (i = 0; _command_name_list[i].cmd != CMD_NO_CMD
                && _command_name_list[i].name != nullptr; i++)
    {
        command_name &data = _command_name_list[i];

        ASSERT(VALID_BIND_COMMAND(data.cmd));
        ASSERT(!_names_to_cmds.count(data.name));
        ASSERT(_cmds_to_names.find(data.cmd)  == _cmds_to_names.end());

        _names_to_cmds[data.name] = data.cmd;
        _cmds_to_names[data.cmd]  = data.name;
    }

    ASSERT(i >= 130);

    for (i = 0; _default_binding_list[i].cmd != CMD_NO_CMD
                && _default_binding_list[i].key != '\0'; i++)
    {
        default_binding &data = _default_binding_list[i];
        ASSERT(VALID_BIND_COMMAND(data.cmd));

        KeymapContext context = context_for_command(data.cmd);

        ASSERT(context < KMC_CONTEXT_COUNT);

        key_to_cmd_map &key_map = _keys_to_cmds[context];
        cmd_to_key_map &cmd_map = _cmds_to_keys[context];

        // Only one command per key, but it's okay to have several
        // keys map to the same command.
        ASSERTM(!key_map.count(data.key), "bad mapping for key '%c'", data.key);

        key_map[data.key] = data.cmd;
        cmd_map[data.cmd] = data.key;
    }

    ASSERT(i >= 130);
}

command_type name_to_command(string name)
{
    return static_cast<command_type>(lookup(_names_to_cmds, name, CMD_NO_CMD));
}

string command_to_name(command_type cmd)
{
    return lookup(_cmds_to_names, cmd, "CMD_NO_CMD");
}

command_type key_to_command(int key, KeymapContext context)
{
    if (CMD_NO_CMD < -key && -key < CMD_MIN_SYNTHETIC)
    {
        const auto cmd = static_cast<command_type>(-key);
        const auto cmd_context = context_for_command(cmd);

        if (cmd == CMD_NO_CMD)
            return CMD_NO_CMD;

        if (cmd_context != context)
        {
            mprf(MSGCH_ERROR,
                 "key_to_command(): command '%s' (%d:%d) wrong for desired "
                 "context %d",
                 command_to_name(cmd).c_str(), -key - CMD_NO_CMD,
                 CMD_MAX_CMD + key, (int) context);
            if (is_processing_macro())
                flush_input_buffer(FLUSH_ABORT_MACRO);
            if (crawl_state.is_replaying_keys()
                || crawl_state.cmd_repeat_start)
            {
                flush_input_buffer(FLUSH_KEY_REPLAY_CANCEL);
            }
            flush_input_buffer(FLUSH_BEFORE_COMMAND);
            return CMD_NO_CMD;
        }
        return cmd;
    }

    const auto cmd = static_cast<command_type>(lookup(_keys_to_cmds[context],
                                                      key, CMD_NO_CMD));

    ASSERT(cmd == CMD_NO_CMD || context_for_command(cmd) == context);

    return cmd;
}

int command_to_key(command_type cmd)
{
    KeymapContext context = context_for_command(cmd);

    if (context == KMC_NONE)
        return '\0';

    return lookup(_cmds_to_keys[context], cmd, '\0');
}

KeymapContext context_for_command(command_type cmd)
{
    if (cmd > CMD_NO_CMD && cmd <= CMD_MAX_NORMAL)
        return KMC_DEFAULT;

    if (cmd >= CMD_MIN_OVERMAP && cmd <= CMD_MAX_OVERMAP)
        return KMC_LEVELMAP;

    if (cmd >= CMD_MIN_TARGET && cmd <= CMD_MAX_TARGET)
        return KMC_TARGETING;

#ifdef USE_TILE
    if (cmd >= CMD_MIN_DOLL && cmd <= CMD_MAX_DOLL)
        return KMC_DOLL;
#endif

    return KMC_NONE;
}

void bind_command_to_key(command_type cmd, int key)
{
    KeymapContext context = context_for_command(cmd);
    string   command_name = command_to_name(cmd);

    if (context == KMC_NONE || command_name == "CMD_NO_CMD"
        || !VALID_BIND_COMMAND(cmd))
    {
        if (command_name == "CMD_NO_CMD")
        {
            mprf(MSGCH_ERROR, "Cannot bind command #%d to a key.",
                 (int) cmd);
            return;
        }

        mprf(MSGCH_ERROR, "Cannot bind command '%s' to a key.",
             command_name.c_str());
        return;
    }

    if (is_userfunction(key))
    {
        mprf(MSGCH_ERROR, "Cannot bind user function keys to a command.");
        return;
    }

    if (is_synthetic_key(key))
    {
        mprf(MSGCH_ERROR, "Cannot bind synthetic keys to a command.");
        return;
    }

    // We're good.
    key_to_cmd_map &key_map = _keys_to_cmds[context];
    cmd_to_key_map &cmd_map = _cmds_to_keys[context];

    key_map[key] = cmd;
    cmd_map[cmd] = key;
}

static string _special_keys_to_string(int key)
{
    const bool shift = (key >= CK_SHIFT_UP && key <= CK_SHIFT_PGDN);
    const bool ctrl  = (key >= CK_CTRL_UP && key <= CK_CTRL_PGDN);

    string cmd = "";

    if (shift)
    {
        key -= (CK_SHIFT_UP - CK_UP);
        cmd = "Shift-";
    }
    else if (ctrl)
    {
        key -= (CK_CTRL_UP - CK_UP);
        cmd = "Ctrl-";
    }

    switch (key)
    {
    case CK_ENTER:  cmd += "Enter"; break;
    case CK_BKSP:   cmd += "Backspace"; break;
    CASE_ESCAPE     cmd += "Esc"; break;
    case CK_DELETE: cmd += "Del"; break;
    case CK_UP:     cmd += "Up"; break;
    case CK_DOWN:   cmd += "Down"; break;
    case CK_LEFT:   cmd += "Left"; break;
    case CK_RIGHT:  cmd += "Right"; break;
    case CK_INSERT: cmd += "Ins"; break;
    case CK_HOME:   cmd += "Home"; break;
    case CK_END:    cmd += "End"; break;
    case CK_CLEAR:  cmd += "Clear"; break;
    case CK_PGUP:   cmd += "PgUp"; break;
    case CK_PGDN:   cmd += "PgDn"; break;
    }

    return cmd;
}

string command_to_string(command_type cmd, bool tutorial)
{
    const int key = command_to_key(cmd);

    const string desc = _special_keys_to_string(key);
    if (!desc.empty())
        return desc;

    string result;
    if (key >= 32 && key < 256)
    {
        if (tutorial && key >= 'A' && key <= 'Z')
            result = make_stringf("uppercase %c", (char) key);
        else
            result = string(1, (char) key);
    }
    else if (key > 1000 && key <= 1009)
    {
        const int numpad = (key - 1000);
        result = make_stringf("Numpad %d", numpad);
    }
#ifdef USE_TILE_LOCAL
    // SDL allows control modifiers for some extra punctuation
    else if (key < 0 && key > SDLK_EXCLAIM - SDLK_a + 1)
        result = make_stringf("Ctrl-%c", (char) (key + SDLK_a - 1));
#endif
    else
    {
        const int ch = key + 'A' - 1;
        if (ch >= 'A' && ch <= 'Z')
            result = make_stringf("Ctrl-%c", (char) ch);
        else
            result = to_string(key);
    }

    return result;
}

void insert_commands(string &desc, const vector<command_type> &cmds, bool formatted)
{
    desc = untag_tiles_console(desc);
    for (command_type cmd : cmds)
    {
        const string::size_type found = desc.find("%");
        if (found == string::npos)
            break;

        string command_name = command_to_string(cmd);
        if (formatted && command_name == "<")
            command_name += "<";
        else if (command_name == "%")
            command_name = "percent";

        desc.replace(found, 1, command_name);
    }
    desc = replace_all(desc, "percent", "%");
}
