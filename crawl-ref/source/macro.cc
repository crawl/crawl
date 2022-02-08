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

// TODO: these includes look not very general relative to how other files
// approach it...
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
                const int key = read_key_code(arg);
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

static string _keyseq_desc(const keyseq &key)
{
    string r = keycode_is_printable(key[0])
                ? keycode_to_name(key[0]).c_str()
                : make_stringf("%s (%s)",
                    vtostr(key).c_str(), keycode_to_name(key[0], false).c_str());
    r = replace_all(r, "<", "<<");
    return r;
}

static string _keyseq_action_desc(keyseq &action)
{
    if (action.empty())
        return "<red>[none]</red>";

    string action_str = vtostr(action);
    action_str = replace_all(action_str, "<", "<<");
    return action_str;
}

static string _keyseq_action_desc(const keyseq &key, macromap &mapref)
{
    if (!mapref.count(key))
    {
        keyseq empty;
        return _keyseq_action_desc(empty);
    }
    else
        return _keyseq_action_desc(mapref[key]);
}

class MacroEditMenu : public Menu
{
private:
    const vector<pair<KeymapContext,string>> modes =
        { { KMC_NONE, "macros" }, // not actually what KMC_NONE means
          { KMC_DEFAULT, "default" },
          { KMC_MENU, "menu" },
          { KMC_LEVELMAP, "level" },
          { KMC_TARGETING, "targeting" },
          { KMC_CONFIRM, "confirmation" }
        };

public:
    MacroEditMenu()
        : Menu(MF_SINGLESELECT | MF_ALLOW_FORMATTING
                | MF_ARROWS_SELECT | MF_WRAP),
          selected_new_key(false), keymc(KMC_NONE), edited_keymaps(false)
    {
        set_tag("macros");
#ifdef USE_TILE_LOCAL
        set_min_col_width(MIN_COLS);
#endif
        action_cycle = Menu::CYCLE_NONE;
        menu_action  = Menu::ACT_EXECUTE;
        set_title(new MenuEntry("", MEL_TITLE));
    }

    void fill_entries(int set_hover_keycode=0)
    {
        // TODO: this seems like somehow it should involve ui::Switcher, but I
        // have no idea how to use that class with a Menu
        clear();
        add_entry(new MenuEntry("Create/edit " + mode_name() + " from key", '~',
            [this](const MenuEntry &)
                {
                    edit_mapping(keyseq());
                    return true;
                }));
        if (get_map().size())
        {
            add_entry(new MenuEntry("Clear all " + mode_name() + "s", '-',
                [this](const MenuEntry &)
                    {
                        status_msg = "";
                        update_macro_more();
                        if (item_count() > 0)
                            clear_all();
                        return true;
                    }));
            add_entry(new MenuEntry("Current " + mode_name() + "s", MEL_SUBTITLE));
            for (auto &mapping : get_map())
            {
                // TODO: indicate if macro is from rc file somehow?
                string action_str = vtostr(mapping.second);
                action_str = replace_all(action_str, "<", "<<");
                MenuEntry *me = new MenuEntry(action_str, (int) mapping.first[0],
                    [this](const MenuEntry &item)
                    {
                        if (item.data)
                            edit_mapping(*static_cast<keyseq *>(item.data));
                        return true;
                    });
                me->data = (void *) &mapping.first;
                add_entry(me);
                if (set_hover_keycode == mapping.first[0])
                    last_hovered = item_count() - 1;
            }
        }
        // update more in case menu changes between empty and non-empty
        update_macro_more();
        update_menu(true); // necessary to update webtiles menu
        if (last_hovered == -1)
            cycle_hover();
    }

    void cycle_mode()
    {
        auto it = modes.begin();
        for (; it != modes.end(); it++)
            if (it->first == keymc)
                break;
        if (it == modes.end())
            keymc = KMC_NONE;
        else
        {
            it++;
            if (it == modes.end())
                keymc = KMC_NONE;
            else
                keymc = it->first;
        }
        status_msg = "";
        update_title();
        fill_entries();
    }

    KeymapContext get_mode() const
    {
        return keymc;
    }

    void update_macro_more()
    {
        vector<string> result;
        for (auto m : modes)
            if (keymc == m.first)
                result.push_back("<w>" + m.second + "</w>");
            else
                result.push_back(m.second);

        string hint;
        if (keymc != KMC_NONE)
            edited_keymaps = true;

        // there's much less use-case for editing keymaps in-game, so hide the
        // details by default
        string mode_hint = edited_keymaps
            ? "cycle mode: " + join_strings(result.begin(), result.end(), "|")
            : "edit keymaps";

        set_more(formatted_string::parse_string(
            status_msg + "\n"
            "Arrows/[<w>enter</w>] to select, [<w>bksp</w>] to clear selected, [<w>?</w>] for help\n"
            "[<w>!</w>"
#ifdef USE_TILE_LOCAL
            "/<w>Right-click</w>"
#endif
            "] " + mode_hint));

    }

    string mode_name()
    {
        // XX less stupid implementation
        switch (keymc)
        {
        case KMC_NONE: return "macro";
        case KMC_DEFAULT: return "regular keymap";
        case KMC_MENU: return "menu keymap";
        case KMC_TARGETING: return "targeting keymap";
        case KMC_LEVELMAP: return "level map keymap";
        case KMC_CONFIRM: return "confirmation keymap";
        default: return "buggy keymap";
        }
    }

    virtual formatted_string calc_title() override
    {
        return formatted_string::parse_string(
            "Editing <w>" + mode_name() + "s</w>.");
    }

    void clear_all()
    {
        const string clear_prompt = make_stringf("Really clear all %ss?",
                mode_name().c_str());
        if (yesno(clear_prompt.c_str(), true, 'N'))
        {
            get_map() = macromap();
            status_msg = "All " + mode_name() + "s cleared!";
            crawl_state.unsaved_macros = true;
            fill_entries();
        }
    }

    macromap &get_map()
    {
        return keymc != KMC_NONE ? Keymaps[keymc] : Macros;
    }

    void clear_mapping(keyseq key)
    {
        macromap &mapref = get_map();
        if (!mapref.count(key))
            return;

        const int keycode = key[0];
        string key_str = keycode_is_printable(keycode)
            ? keycode_to_name(keycode, false).c_str()
            : make_stringf("%s (%s)",
                    vtostr(key).c_str(), keycode_to_name(keycode, false).c_str());
        string action_str = vtostr(mapref[key]);

        action_str = replace_all(action_str, "<", "<<");
        key_str = replace_all(key_str, "<", "<<");

        status_msg = make_stringf("Cleared %s '%s' => '%s'.",
                    mode_name().c_str(),
                    key_str.c_str(),
                 action_str.c_str());

        macro_del(mapref, key);
        crawl_state.unsaved_macros = true;
        fill_entries();
    }

    void clear_hovered()
    {
        if (last_hovered < 0)
            return;
        keyseq *_key_chosen = static_cast<keyseq *>(items[last_hovered]->data);
        if (!_key_chosen)
            return;

        // TODO: add a quick undo key?
        clear_mapping(*_key_chosen);
    }

    class MappingEditMenu : public Menu
    {
    public:
        MappingEditMenu(keyseq _key, keyseq _action, MacroEditMenu &_parent)
            : Menu(MF_SINGLESELECT | MF_ALLOW_FORMATTING | MF_ARROWS_SELECT,
                    "", KMC_MENU),
              key(_key), action(_action), abort(false),
              parent(_parent),
              doing_key_input(false), doing_raw_action_input(false)
        {
            set_tag("macro_mapping");
#ifdef USE_TILE_LOCAL
            set_min_col_width(62); // based on `r` more width
#endif
            set_more(string(""));
            if (key.size() == 0)
                initialize_needs_key();
            else if (action.size() == 0)
            {
                reset_key_prompt();
                on_show = [this]()
                {
                    if (edit_action())
                        return false;
                    initialize_with_key();
                    update_menu(true);
                    return true;
                };
            }
            else
                initialize_with_key();
        }

        /// show the menu and edit a mapping
        /// @return whether a mapping was set
        bool input_mapping()
        {
            show();
            return !abort;
        }

        /// Initialize the menu to accept key input immediately on show
        void initialize_needs_key()
        {
            clear();
            key.clear();
            set_more(string(""));
            prompt = make_stringf(
                "Input trigger key to edit or create a %s:",
                parent.mode_name().c_str());
            set_title(new MenuEntry(prompt, MEL_TITLE));
            set_more("<lightgray>([<w>~</w>] to enter by keycode)</lightgray>");
            doing_key_input = true;
        }

        void reset_key_prompt()
        {
            prompt = make_stringf("Current %s for %s: %s",
                        parent.mode_name().c_str(),
                        _keyseq_desc(key).c_str(),
                        _keyseq_action_desc(action).c_str());

            set_title(new MenuEntry(prompt + "\n", MEL_TITLE));
        }

        /// Initialize the menu for key editing, given some key to edit
        void initialize_with_key()
        {
            ASSERT(key.size());
            clear();
            reset_key_prompt();
            set_more(string(""));

            add_entry(new MenuEntry("redefine", 'r',
                [this](const MenuEntry &)
                {
                    set_more("");
                    return !edit_action();
                }));

            add_entry(new MenuEntry("redefine with raw key entry", 'R',
                [this](const MenuEntry &)
                {
                    set_more("");
                    edit_action_raw();
                    return true;
                }));

            if (!action.empty())
            {
                add_entry(new MenuEntry("clear", 'c',
                    [this](const MenuEntry &)
                    {
                        action.clear();
                        return false;
                    }));
            }

            add_entry(new MenuEntry("abort", 'a',
                [this](const MenuEntry &)
                {
                    abort = true;
                    return false;
                }));

            if (last_hovered == -1)
                cycle_hover();
        }

        /// Enter raw input mode for keymaps -- allows mapping to any key
        /// except enter and esc
        void edit_action_raw()
        {
            prompt = make_stringf(
                "<w>%s</w>\nInput (raw) new %s for %s: ",
                        prompt.c_str(),
                        parent.mode_name().c_str(),
                        _keyseq_desc(key).c_str());
            set_title(new MenuEntry(prompt, MEL_TITLE));
            set_more("Raw input: [<w>esc</w>] to abort, [<w>enter</w>] to accept.");
            update_menu(true);
            doing_raw_action_input = true;
        }

        /// edit an action, using title_prompt for text entry
        /// @return true if an action was fully set
        bool edit_action()
        {
            char buff[1024];
            const string edit_prompt = make_stringf("<w>%s</w>\nInput new %s for %s:",
                        prompt.c_str(),
                        parent.mode_name().c_str(),
                        _keyseq_desc(key).c_str());

            int old_last_hovered = last_hovered;
            set_hovered(-1);
            set_more("Input a key sequence. Use <w>\\{n}</w> to enter keycode <w>n</w>. [<w>esc</w>] for menu");
            if (!title_prompt(buff, sizeof(buff), edit_prompt.c_str()))
            {
                set_hovered(old_last_hovered);
                set_more("");
                // line reader success code is 0
                return lastch == 0;
            }

            keyseq new_action = parse_keyseq(buff);
            // it's still possible to get a blank keyseq by having parsing
            // issues with backslashes, for example
            if (!new_action.size())
            {
                set_more(make_stringf("Parsing error in key sequence '%s'", buff));
                return true;
            }
            set_hovered(old_last_hovered);
            action = new_action;
            reset_key_prompt();
            update_menu(true);
            return true;
        }

        bool process_key(int keyin)
        {
            // stateful key processing:
            // * in raw action input mode, fill keys into raw_tmp
            // * in key input mode, fill exactly one key into `key`, either
            //   by key entry or keycode
            // * otherwise, use normal menu key handling
            //
            // regular action input as well as keycode entry use title_prompt,
            // and so are handled in the superclass
            if (doing_raw_action_input)
            {
                if (keyin == ESCAPE || keyin == CONTROL('G'))
                {
                    doing_raw_action_input = false;
                    raw_tmp.clear();
                    set_more("");
                    reset_key_prompt();
                    return true;
                }
                else if (keyin == '\r' || keyin == '\n')
                {
                    doing_raw_action_input = false;
                    if (raw_tmp.size() && raw_tmp[0] != 0)
                        action = raw_tmp;
                    set_more("");
                    reset_key_prompt();
                    return true;
                }
                raw_tmp.push_back(keyin);
                set_title(new MenuEntry(prompt + vtostr(raw_tmp)));
                return true;
            }
            else if (doing_key_input)
            {
                doing_key_input = false;
                if (keyin == ESCAPE || keyin == CONTROL('G') || keyin == CK_MOUSE_B2)
                {
                    abort = true;
                    return false;
                }
                else if (keyin == '~')
                {
                    char buff[10];
                    set_more("[<w>?</w>] Keycode help. "
                        "Quick reference: 8: [<w>bksp</w>], "
                        "9: [<w>tab</w>], 13: [<w>enter</w>], 27: [<w>esc</w>]"
                        );
                    if (!title_prompt(buff, sizeof(buff),
                        "Enter keycode by number:"
#ifndef USE_TILE_LOCAL
                        , "console-keycodes"
#endif
                        ))
                    {
                        abort = true;
                        return false;
                    }
                    keyin = read_key_code(string(buff));
                    if (keyin == 0)
                    {
                        abort = true;
                        return false;
                    }
                }

                // intercept one key, and store it in `key`
                key.push_back(keyin); // TODO: vs _getch_mul?
                // switch to editing state and reinit the menu
                macromap &mapref = parent.get_map();
                if (!mapref.count(key))
                    action.clear();
                else
                    action = mapref[key];
                // if the mapping is new, edit immediately
                if (action.empty())
                {
                    prompt = make_stringf("%s %s", prompt.c_str(),
                                                    _keyseq_desc(key).c_str());
                    if (edit_action())
                        return false;
                }
                // TODO: this drops to the mapping edit menu at this point. It
                // would be faster to go back to the main macro menu, but this
                // allows the player to cancel. Which is better?
                initialize_with_key();
                update_menu(true);
                return true;
            }

            if (keyin == '?')
            {
                show_specific_helps({ "macro-menu"
#ifndef USE_TILE_LOCAL
                    , "console-keycodes"
#endif
                    });
            }
            else if (keyin == 'a')
                return false; // legacy key
            return Menu::process_key(keyin);
        }

        keyseq key;
        keyseq action;
        bool abort;
    protected:
        MacroEditMenu &parent;
        keyseq raw_tmp;
        bool doing_key_input;
        bool doing_raw_action_input;
        string prompt;
    };

    bool edit_mapping(keyseq key)
    {
        status_msg = "";
        update_macro_more();

        const bool existed = get_map().count(key);

        MappingEditMenu pop = MappingEditMenu(key,
            existed ? get_map()[key] : keyseq(), *this);
        if (pop.input_mapping())
        {
            if (pop.action.size()
                && (!get_map().count(pop.key) || get_map()[pop.key] != pop.action))
            {
                macro_add(get_map(), pop.key, pop.action);
                status_msg = make_stringf("%s %s '%s' => '%s'.",
                    existed ? "Redefined" : "Created",
                    mode_name().c_str(),
                    _keyseq_desc(pop.key).c_str(),
                    _keyseq_action_desc(pop.key, get_map()).c_str());
                crawl_state.unsaved_macros = true;
            }
            else if (!pop.action.size())
                clear_mapping(pop.key);
            if (pop.key.size())
                fill_entries(pop.key[0]);
            // else, we aborted
        }

        return false;
    }

    void add_mapping_from_last()
    {
        keyseq key;
        key.push_back(lastch);
        // XX could this jump right into editing?
        edit_mapping(key);
    }

    bool process_key(int keyin) override
    {
        switch (keyin)
        {
        case CK_REDRAW:
        case ' ': case CK_PGDN: case '>': case '+':
        case CK_MOUSE_CLICK:
        case CK_MOUSE_B1:
        case CK_PGUP: case '<':
        case CK_UP:
        case CK_DOWN:
        case CK_HOME:
        case CK_END:
        case CK_ENTER:
            if (item_count() == 0)
                return true; // override weird superclass behavior
            //fallthrough
        case CK_MOUSE_B2:
        CASE_ESCAPE
        case '-':
        case '~':
            return Menu::process_key(keyin);
        case '?':
            show_specific_helps({ "macro-menu"
#ifndef USE_TILE_LOCAL
                    , "console-keycodes"
#endif
                    });
            return true;
        case CK_MOUSE_CMD:
        case '!':
            cycle_mode();
            return true;
        case CK_DELETE:
        case CK_BKSP:
            clear_hovered();
            return true;
        default: // any other key -- no menu item yet
            lastch = keyin;
            add_mapping_from_last();
            return true;
        }
    }
    bool selected_new_key;
    string status_msg;
protected:
    KeymapContext keymc;
    bool edited_keymaps;
};

void macro_quick_add()
{
    MacroEditMenu menu;
    keyseq empty;
    menu.edit_mapping(empty);
    if (menu.status_msg.size())
        mpr(menu.status_msg);
    else
        canned_msg(MSG_OK);
}

void macro_menu()
{
    MacroEditMenu menu;
    menu.fill_entries();

    menu.show();

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
        return "Cannot parse macros += %s , there are only two arguments";

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

#ifdef DEBUG
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
#endif

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
#elif defined(USE_TILE_WEB)
        return keycode >= 32 && keycode < 128;
#else
        return keycode >= 32;
#endif
    }
}

string keycode_to_name(int keycode, bool shorten)
{
    // this is printable, but it's very confusing to try to use ' ' to print it
    // in circumstances where a name is called for
    if (keycode == ' ')
        return "Space";
    // TODO: handling of alt keys in SDL is generally a mess, including here
    // (they are basically just ignored)
    if (keycode_is_printable(keycode))
        return string(1, keycode);

    // shift/ctrl-modified keys aside from shift-tab don't seem to work on mac
    // console, and are somewhat spotty on webtiles.
    const bool shift = (keycode >= CK_SHIFT_UP && keycode <= CK_SHIFT_PGDN);
    const bool ctrl  = (keycode >= CK_CTRL_UP && keycode <= CK_CTRL_PGDN);

    // nb both of these use string literal concatenation
    #define CTRL_DESC(x) (shorten ? ("^" x) : ("Ctrl-" x))
    #define NP_DESC(x) (shorten ? ("NP" x) : ("Numpad " x))

    string prefix = "";

    if (shift)
    {
        keycode -= (CK_SHIFT_UP - CK_UP);
        prefix = "Shift-";
    }
    else if (ctrl)
    {
        keycode -= (CK_CTRL_UP - CK_UP);
        prefix = CTRL_DESC("");
    }

    // placeholder
    switch (keycode)
    {
    case  0: return "NULL";
    case  8: return "Backspace"; // CK_BKSP
    case  9: return "Tab";
    CASE_ESCAPE return "Esc";
    case '\n':
    case '\r': // CK_ENTER
        return "Enter";
    case CK_DELETE: return "Del";
    case CK_UP:     return prefix+"Up";
    case CK_DOWN:   return prefix+"Down";
    case CK_LEFT:   return prefix+"Left";
    case CK_RIGHT:  return prefix+"Right";
    case CK_INSERT: return prefix+"Ins";
    case CK_HOME:   return prefix+"Home";
    case CK_END:    return prefix+"End";
    case CK_CLEAR:  return prefix+"Clear";
    case CK_PGUP:   return prefix+"PgUp";
    case CK_PGDN:   return prefix+"PgDn";
    case CK_SHIFT_TAB:    return "Shift-Tab";
    case CK_CTRL_TAB:     return CTRL_DESC("Tab");
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
#ifndef USE_TILE_LOCAL
    case CK_NUMPAD_0: return NP_DESC("0");
    case CK_NUMPAD_1: return NP_DESC("1");
    case CK_NUMPAD_2: return NP_DESC("2");
    case CK_NUMPAD_3: return NP_DESC("3");
    case CK_NUMPAD_4: return NP_DESC("4");
    case CK_NUMPAD_5: return NP_DESC("5");
    case CK_NUMPAD_6: return NP_DESC("6");
    case CK_NUMPAD_7: return NP_DESC("7");
    case CK_NUMPAD_8: return NP_DESC("8");
    case CK_NUMPAD_9: return NP_DESC("9");
    // many of these may not actually work on any given local console:
    // TODO: confirm the names. Some stuff in libunix.cc appears to have
    // incorrect comments.
    case CK_NUMPAD_MULTIPLY: return NP_DESC("*");
    case CK_NUMPAD_ADD:      return NP_DESC("+");
    case CK_NUMPAD_ADD2:     return NP_DESC("+"); // are there keyboards with both??
    case CK_NUMPAD_SUBTRACT: return NP_DESC("-");
    case CK_NUMPAD_SUBTRACT2: return NP_DESC("-");
    case CK_NUMPAD_DECIMAL:  return NP_DESC(".");
    case CK_NUMPAD_DIVIDE:   return NP_DESC("/");
    case CK_NUMPAD_ENTER:    return NP_DESC("enter");
#endif
    default:
        if (keycode >= CONTROL('A') && keycode <= CONTROL('Z'))
            return make_stringf("%s%c", CTRL_DESC(""), UNCONTROL(keycode));
#ifdef USE_TILE_LOCAL
        // SDL allows control modifiers for some extra punctuation
        else if (keycode < 0 && keycode > SDLK_EXCLAIM - SDLK_a + 1)
            return make_stringf("%s%c", CTRL_DESC(""), (char) (keycode + SDLK_a - 1));

        // SDL uses 1 << 30 to indicate non-printable keys, crawl uses negative
        // numbers; convert back to plain SDL form
        if (keycode < 0)
            keycode = -keycode;
        // SDL_GetKeyName strips capitalization, so we don't want to use it for
        // printable keys.
        return string(SDL_GetKeyName(keycode));
#else
    {
        keyseq v;
        v.push_back(keycode);
        return vtostr(v);
    }
#endif

#undef CTRL_DESC
#undef NP_DESC
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

string command_to_string(command_type cmd, bool tutorial)
{
    const int key = command_to_key(cmd);

    string result;
    if (key >= 32 && key < 256)
    {
        if (tutorial && key >= 'A' && key <= 'Z')
            result = make_stringf("uppercase %c", (char) key);
        else
            result = string(1, (char) key);
    }
    else if (key > 1000 && key <= 1009) // can this be removed?
    {
        const int numpad = (key - 1000);
        result = make_stringf("Numpad %d", numpad);
    }
    else
        result = keycode_to_name(key, false);

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
