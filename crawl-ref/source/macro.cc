/*
 *  File:       macro.cc
 *  Summary:    Crude macro-capability
 *  Written by: Juho Snellman <jsnell@lyseo.edu.ouka.fi>
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 */

/*
 * The macro-implementation works like this:
 *   - For generic game code, #define getch() getchm().
 *   - getchm() works by reading characters from an internal
 *     buffer. If none are available, new characters are read into
 *     the buffer with getch_mul().
 *   - getch_mul() reads at least one character, but will read more
 *     if available (determined using kbhit(), which should be defined
 *     in the platform specific libraries).
 *   - Before adding the characters read into the buffer, any macros
 *     in the sequence are replaced (see macro_add_buf_long for the
 *     details).
 *
 * (When the above text mentions characters, it actually means int).
 */

#include "AppHdr.h"
REVISION("$Rev$");

#define MACRO_CC
#include "macro.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <deque>
#include <vector>

#include <cstdio>      // for snprintf
#include <cctype>      // for tolower
#include <cstdlib>

#include "cio.h"
#include "delay.h"
#include "externs.h"
#include "message.h"
#include "state.h"
#include "stuff.h"

// for trim_string:
#include "initfile.h"

typedef std::deque<int> keybuf;
typedef std::map<keyseq,keyseq> macromap;

static macromap Keymaps[KC_CONTEXT_COUNT];
static macromap Macros;

static macromap *all_maps[] =
{
    &Keymaps[KC_DEFAULT],
    &Keymaps[KC_LEVELMAP],
    &Keymaps[KC_TARGETING],
    &Keymaps[KC_CONFIRM],

#ifdef USE_TILE
    &Keymaps[KC_TILE],
#endif

    &Macros,
};

static keybuf Buffer;

#define USERFUNCBASE -10000
static std::vector<std::string> userfunctions;

static std::vector<key_recorder*> recorders;

typedef std::map<std::string, int> name_to_cmd_map;
typedef std::map<int, std::string> cmd_to_name_map;

struct command_name
{
    command_type cmd;
    const char*  name;
};

static command_name _command_name_list[] = {
#include "cmd-name.h"
};

static name_to_cmd_map _names_to_cmds;
static cmd_to_name_map _cmds_to_names;

struct default_binding
{
    int           key;
    command_type  cmd;
};

static default_binding _default_binding_list[] = {
#include "cmd-keys.h"
};

typedef std::map<int, int> key_to_cmd_map;
typedef std::map<int, int> cmd_to_key_map;

static key_to_cmd_map _keys_to_cmds[KC_CONTEXT_COUNT];
static cmd_to_key_map _cmds_to_keys[KC_CONTEXT_COUNT];

inline int userfunc_index(int key)
{
    int index = (key <= USERFUNCBASE? USERFUNCBASE - key : -1);
    return (index < 0 || index >= (int) userfunctions.size()? -1 : index);
}

static int userfunc_index(const keyseq &seq)
{
    if (seq.empty())
        return (-1);

    return userfunc_index(seq.front());
}

bool is_userfunction(int key)
{
    return (userfunc_index(key) != -1);
}

static bool is_userfunction(const keyseq &seq)
{
    return (userfunc_index(seq) != -1);
}

std::string get_userfunction(int key)
{
    int index = userfunc_index(key);
    return (index == -1 ? NULL : userfunctions[index]);
}

static std::string get_userfunction(const keyseq &seq)
{
    int index = userfunc_index(seq);
    return (index == -1 ? NULL : userfunctions[index]);
}

static bool userfunc_referenced(int index, const macromap &mm)
{
    for (macromap::const_iterator i = mm.begin(); i != mm.end(); i++)
    {
        if (userfunc_index(i->second) == index)
            return (true);
    }
    return (false);
}

static bool userfunc_referenced(int index)
{
    for (unsigned i = 0; i < sizeof(all_maps) / sizeof(*all_maps); ++i)
    {
        macromap *m = all_maps[i];
        if (userfunc_referenced(index, *m))
            return (true);
    }
    return (false);
}

// Expensive function to discard unused function names
static void userfunc_collectgarbage(void)
{
    for (int i = userfunctions.size() - 1; i >= 0; --i)
    {
        if (!userfunctions.empty() && !userfunc_referenced(i))
            userfunctions[i].clear();
    }
}

static int userfunc_getindex(const std::string &fname)
{
    if (fname.length() == 0)
        return (-1);

    userfunc_collectgarbage();

    // Pass 1 to see if we have this string already
    for (int i = 0, count = userfunctions.size(); i < count; ++i)
    {
        if (userfunctions[i] == fname)
            return (i);
    }

    // Pass 2 to hunt for gaps.
    for (int i = 0, count = userfunctions.size(); i < count; ++i)
    {
        if (userfunctions[i].empty())
        {
            userfunctions[i] = fname;
            return (i);
        }
    }

    userfunctions.push_back(fname);
    return (userfunctions.size() - 1);
}

// Returns the name of the file that contains macros.
static std::string get_macro_file()
{
    std::string dir = !Options.macro_dir.empty() ? Options.macro_dir :
                      !SysEnv.crawl_dir.empty()  ? SysEnv.crawl_dir : "";

    if (!dir.empty())
    {
#ifndef DGL_MACRO_ABSOLUTE_PATH
        if (dir[dir.length() - 1] != FILE_SEPARATOR)
            dir += FILE_SEPARATOR;
#endif
    }

#if defined(DGL_MACRO_ABSOLUTE_PATH)
    return (dir.empty()? "macro.txt" : dir);
#elif defined(DGL_NAMED_MACRO_FILE)
    return (dir + strip_filename_unsafe_chars(you.your_name) + "-macro.txt");
#else
    return (dir + "macro.txt");
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
            k.push_back( USERFUNCBASE - index );
    }
    else
    {
        const int len = strlen( buff );
        for (int i = 0; i < len; i++)
            k.push_back( buff[i] );
    }
}

static int read_key_code(std::string s)
{
    if (s.empty())
        return (0);

    int base = 10;
    if (s[0] == 'x')
    {
        s = s.substr(1);
        base = 16;
    }
    else if (s[0] == '^')
    {
        // ^A = 1, etc.
        return (1 + toupper(s[1]) - 'A');
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
static keyseq parse_keyseq( std::string s )
{
    int state = 0;
    keyseq v;
    int num;

    if (s.find("===") == 0)
    {
        buf2keyseq(s.c_str(), v);
        return (v);
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
            {
                state = 2;
                num = 0;
            }
            // XXX Error handling
            break;

        case 2: // Inside \{}
        {
            const std::string::size_type clb = s.find('}', i);
            if (clb == std::string::npos)
                break;

            const std::string arg = s.substr(i, clb - i);
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

    return (v);
}

/*
 * Serializes a key sequence into a string of the format described
 * above.
 */
static std::string vtostr( const keyseq &seq )
{
    std::ostringstream s;

    const keyseq *v = &seq;
    keyseq dummy;
    if (is_userfunction(seq))
    {
        // Laboriously reconstruct the original user input
        std::string newseq = std::string("==") + get_userfunction(seq);
        buf2keyseq(newseq.c_str(), dummy);
        dummy.push_front('=');

        v = &dummy;
    }

    for (keyseq::const_iterator i = v->begin(); i != v->end(); i++)
    {
        if (*i <= 32 || *i > 127)
        {
            if (*i == KEY_MACRO_MORE_PROTECT)
                s << "\\{!more}";
            else
            {
                char buff[20];
                snprintf( buff, sizeof(buff), "\\{%d}", *i );
                s << buff;
            }
        }
        else if (*i == '\\')
            s << "\\\\";
        else
            s << static_cast<char>(*i);
    }

    return (s.str());
}

/*
 * Add a macro (suprise, suprise).
 */
static void macro_add( macromap &mapref, keyseq key, keyseq action )
{
    mapref[key] = action;
}

static void macro_add( macromap &mapref, keyseq key, const char *buff )
{
    keyseq  act;
    buf2keyseq(buff, act);
    if (!act.empty())
        macro_add( mapref, key, act );
}

/*
 * Remove a macro.
 */
static void macro_del( macromap &mapref, keyseq key )
{
    mapref.erase( key );
}

/*
 * Adds keypresses from a sequence into the internal keybuffer. Ignores
 * macros.
 */
void macro_buf_add( const keyseq &actions, bool reverse)
{
    keyseq act;
    bool need_more_reset = false;
    for (keyseq::const_iterator i = actions.begin(); i != actions.end();
         ++i)
    {
        int key = *i;
        if (key == KEY_MACRO_MORE_PROTECT)
        {
            key = KEY_MACRO_DISABLE_MORE;
            need_more_reset = true;
        }
        act.push_back(key);
    }
    if (need_more_reset)
        act.push_back(KEY_MACRO_ENABLE_MORE);

    Buffer.insert( reverse? Buffer.begin() : Buffer.end(),
                   act.begin(), act.end() );
}

/*
 * Adds a single keypress into the internal keybuffer.
 */
void macro_buf_add( int key, bool reverse )
{
    if (reverse)
        Buffer.push_front( key );
    else
        Buffer.push_back( key );
}


/*
 * Adds keypresses from a sequence into the internal keybuffer. Does some
 * O(N^2) analysis to the sequence to replace macros.
 */
static void macro_buf_add_long( keyseq actions,
                                macromap &keymap = Keymaps[KC_DEFAULT] )
{
    keyseq tmp;

    // debug << "Adding: " << vtostr(actions) << endl;
    // debug.flush();

    // Check whether any subsequences of the sequence are macros.
    // The matching starts from as early as possible, and is
    // as long as possible given the first constraint. I.e from
    // the sequence "abcdef" and macros "ab", "bcde" and "de"
    // "ab" and "de" are recognized as macros.

    while (actions.size() > 0)
    {
        tmp = actions;

        while (tmp.size() > 0)
        {
            keyseq result = keymap[tmp];

            // Found a macro. Add the expansion (action) of the
            // macro into the buffer.
            if (result.size() > 0)
            {
                macro_buf_add( result );
                break;
            }

            // Didn't find a macro. Remove a key from the end
            // of the sequence, and try again.
            tmp.pop_back();
        }

        if (tmp.size() == 0)
        {
            // Didn't find a macro. Add the first keypress of the sequence
            // into the buffer, remove it from the sequence, and try again.
            macro_buf_add( actions.front() );
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

static int macro_keys_left = -1;

bool is_processing_macro()
{
    return (macro_keys_left >= 0);
}

/*
 * Command macros are only applied from the immediate front of the
 * buffer, and only when the game is expecting a command.
 */
static void macro_buf_apply_command_macro( void )
{
    keyseq  tmp = Buffer;

    // find the longest match from the start of the buffer and replace it
    while (tmp.size() > 0)
    {
        const keyseq &result = Macros[tmp];

        if (result.size() > 0)
        {
            // Found macro, remove match from front:
            for (unsigned int i = 0; i < tmp.size(); i++)
            {
                Buffer.pop_front();
                if (macro_keys_left >= 0)
                    macro_keys_left--;
            }

            if (macro_keys_left == -1)
                macro_keys_left = 0;
            macro_keys_left += result.size();

            macro_buf_add(result, true);

            break;
        }

        tmp.pop_back();
    }
}

/*
 * Removes the earliest keypress from the keybuffer, and returns its
 * value. If buffer was empty, returns -1;
 */
static int macro_buf_get( void )
{
    if (Buffer.size() == 0)
    {
        // If we're trying to fetch a new keystroke, then the processing
        // of the previous keystroke is complete.
        if (macro_keys_left == 0)
            macro_keys_left = -1;

        return (-1);
    }

    int key = Buffer.front();
    Buffer.pop_front();

    if (macro_keys_left >= 0)
        macro_keys_left--;

    for (int i = 0, size_i = recorders.size(); i < size_i; i++)
        recorders[i]->add_key(key);

    return (key);
}

static void write_map(std::ofstream &f, const macromap &mp, const char *key)
{
    for (macromap::const_iterator i = mp.begin(); i != mp.end(); i++)
    {
        // Need this check, since empty values are added into the
        // macro struct for all used keyboard commands.
        if (i->second.size())
        {
            f << key  << vtostr((*i).first) << std::endl
              << "A:" << vtostr((*i).second) << std::endl << std::endl;
        }
    }
}

/*
 * Saves macros into the macrofile, overwriting the old one.
 */
void macro_save()
{
    std::ofstream f;
    const std::string macrofile = get_macro_file();
    f.open(macrofile.c_str());
    if (!f)
    {
        mprf(MSGCH_ERROR, "Couldn't open %s for writing!", macrofile.c_str());
        return;
    }

    f << "# WARNING: This file is entirely auto-generated." << std::endl
      << std::endl << "# Key Mappings:" << std::endl;
    for (int mc = KC_DEFAULT; mc < KC_CONTEXT_COUNT; ++mc)
    {
        char keybuf[30] = "K:";
        if (mc)
            snprintf(keybuf, sizeof keybuf, "K%d:", mc);
        write_map(f, Keymaps[mc], keybuf);
    }

    f << "# Command Macros:" << std::endl;
    write_map(f, Macros, "M:");

    f.close();
}

/*
 * Reads as many keypresses as are available (waiting for at least one),
 * and returns them as a single keyseq.
 */
static keyseq getch_mul( int (*rgetch)() = NULL )
{
    keyseq keys;
    int    a;

    // Something's gone wrong with replaying keys if crawl needs to
    // get new keys from the user.
    if (crawl_state.is_replaying_keys())
    {
        mpr("(Key replay ran out of keys)");
        crawl_state.cancel_cmd_repeat();
        crawl_state.cancel_cmd_again();
    }

    if (!rgetch)
        rgetch = m_getch;

    keys.push_back( a = rgetch() );

    // The a == 0 test is legacy code that I don't dare to remove. I
    // have a vague recollection of it being a kludge for conio support.
    while (kbhit() || a == 0)
    {
        keys.push_back( a = rgetch() );
    }

    return (keys);
}

/*
 * Replacement for getch(). Returns keys from the key buffer if available.
 * If not, adds some content to the buffer, and returns some of it.
 */
int getchm( int (*rgetch)() )
{
    return getchm( KC_DEFAULT, rgetch );
}

int getchm(KeymapContext mc, int (*rgetch)())
{
    int a;

    // Got data from buffer.
    if ((a = macro_buf_get()) != -1)
        return (a);

    // Read some keys...
    keyseq keys = getch_mul(rgetch);
    if (mc == KC_NONE)
        macro_buf_add(keys);
    else
        macro_buf_add_long(keys, Keymaps[mc]);

    return (macro_buf_get());
}

/*
 * Replacement for getch(). Returns keys from the key buffer if available.
 * If not, adds some content to the buffer, and returns some of it.
 */
int getch_with_command_macros( void )
{
    if (Buffer.size() == 0)
    {
        // Read some keys...
        keyseq keys = getch_mul();
        // ... and add them into the buffer (apply keymaps)
        macro_buf_add_long( keys );

        // Apply longest matching macro at front of buffer:
        macro_buf_apply_command_macro();
    }

    return (macro_buf_get());
}

/*
 * Flush the buffer.  Later we'll probably want to give the player options
 * as to when this happens (ex. always before command input, casting failed).
 */
void flush_input_buffer( int reason )
{
    ASSERT(reason != FLUSH_KEY_REPLAY_CANCEL ||
           crawl_state.is_replaying_keys() || crawl_state.cmd_repeat_start);

    ASSERT(reason != FLUSH_ABORT_MACRO || is_processing_macro());

    // Any attempt to flush means that the processing of the previously
    // fetched keystroke is complete.
    if (macro_keys_left == 0)
        macro_keys_left = -1;

    if (crawl_state.is_replaying_keys() && reason != FLUSH_ABORT_MACRO
        && reason != FLUSH_KEY_REPLAY_CANCEL &&
        reason != FLUSH_REPLAY_SETUP_FAILURE)
    {
        return;
    }

    if (Options.flush_input[ reason ] || reason == FLUSH_ABORT_MACRO
        || reason == FLUSH_KEY_REPLAY_CANCEL
        || reason == FLUSH_REPLAY_SETUP_FAILURE)
    {
        while (!Buffer.empty())
        {
            const int key = Buffer.front();
            Buffer.pop_front();
            if (key == KEY_MACRO_ENABLE_MORE)
                Options.show_more_prompt = true;
        }
        macro_keys_left = -1;
    }
}

void macro_add_query( void )
{
    int input;
    bool keymap = false;
    KeymapContext keymc = KC_DEFAULT;

    mesclr();
    mpr("(m)acro, keymap "
        "[(k) default, (x) level-map, (t)argeting, (c)onfirm, m(e)nu], "
        "(s)ave?",
        MSGCH_PROMPT);
    input = m_getch();
    input = tolower( input );
    if (input == 'k')
    {
        keymap = true;
        keymc  = KC_DEFAULT;
    }
    else if (input == 'x')
    {
        keymap = true;
        keymc  = KC_LEVELMAP;
    }
    else if (input == 't')
    {
        keymap = true;
        keymc  = KC_TARGETING;
    }
    else if (input == 'c')
    {
        keymap = true;
        keymc  = KC_CONFIRM;
    }
    else if (input == 'e')
    {
        keymap = true;
        keymc  = KC_MENU;
    }
    else if (input == 'm')
        keymap = false;
    else if (input == 's')
    {
        mpr("Saving macros.");
        macro_save();
        return;
    }
    else
    {
        mpr( "Aborting." );
        return;
    }

    // reference to the appropriate mapping
    macromap &mapref = (keymap ? Keymaps[keymc] : Macros);

    mprf(MSGCH_PROMPT, "Input %s%s trigger key: ",
         keymap ? (keymc == KC_DEFAULT   ? "default " :
                   keymc == KC_LEVELMAP  ? "level-map " :
                   keymc == KC_TARGETING ? "targeting " :
                   keymc == KC_CONFIRM   ? "confirm " :
                   keymc == KC_MENU      ? "menu " :
                   "buggy") : "",
         (keymap ? "keymap" : "macro") );

    keyseq key;
    mouse_control mc(MOUSE_MODE_MACRO);
    key = getch_mul();

    cprintf( "%s" EOL, (vtostr( key )).c_str() ); // echo key to screen

    if (mapref[key].size() > 0)
    {
        mprf(MSGCH_WARN, "Current Action: %s", vtostr(mapref[key]).c_str());
        mpr( "Do you wish to (r)edefine, (c)lear, or (a)bort?", MSGCH_PROMPT );

        input = m_getch();

        input = tolower( input );
        if (input == 'a' || input == ESCAPE)
        {
            mpr( "Aborting." );
            return;
        }
        else if (input == 'c')
        {
            mpr( "Cleared." );
            macro_del( mapref, key );
            return;
        }
    }

    mpr( "Input Macro Action: ", MSGCH_PROMPT );

    // Using getch_mul() here isn't very useful...  We'd like the
    // flexibility to define multicharacter macros without having
    // to resort to editing files and restarting the game.  -- bwr
    // keyseq act = getch_mul();

    char    buff[4096];
    get_input_line(buff, sizeof buff);

    if (Options.macro_meta_entry)
    {
        macro_add( mapref, key, parse_keyseq(buff) );
    }
    else
    {
        macro_add( mapref, key, buff );
    }

    redraw_screen();
}

/*
 * Initializes the macros.
 */
static void _read_macros_from(const char* filename)
{
    std::string s;
    std::ifstream f;
    keyseq key, action;
    bool keymap = false;
    KeymapContext keymc = KC_DEFAULT;

    f.open( filename );

    while (f >> s)
    {
        trim_string(s);  // remove white space from ends

        if (s[0] == '#')
            continue;    // skip comments
        else if (s.substr(0, 2) == "K:")
        {
            key = parse_keyseq(s.substr(2));
            keymap = true;
            keymc  = KC_DEFAULT;
        }
        else if (s.length() >= 3 && s[0] == 'K' && s[2] == ':')
        {
            keymc  = KeymapContext( KC_DEFAULT + s[1] - '0' );
            if (keymc >= KC_DEFAULT && keymc < KC_CONTEXT_COUNT)
            {
                key    = parse_keyseq(s.substr(3));
                keymap = true;
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
            macro_add( (keymap ? Keymaps[keymc] : Macros), key, action );
        }
    }
}

void macro_init()
{
    const std::vector<std::string>& files = Options.additional_macro_files;
    for (std::vector<std::string>::const_iterator it = files.begin();
         it != files.end();
         ++it)
    {
        _read_macros_from(it->c_str());
    }

    _read_macros_from(get_macro_file().c_str());
}


void macro_userfn(const char *keys, const char *regname)
{
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
    case KEY_REPEAT_KEYS:
        return (true);
    default:
        return (false);
    }
}

key_recorder::key_recorder(key_recorder_callback cb, void* cb_data)
    : paused(false), call_back(cb), call_back_data(cb_data)
{
    keys.clear();
    macro_trigger_keys.clear();
}

void key_recorder::add_key(int key, bool reverse)
{
    if (paused)
        return;

    if (call_back)
    {
        // Don't record key if true
        if ((*call_back)(this, key, reverse))
            return;
    }

    if (reverse)
        keys.push_front(key);
    else
        keys.push_back(key);
}

void key_recorder::remove_trigger_keys(int num_keys)
{
    ASSERT(num_keys >= 1);

    if (paused)
        return;

    for (int i = 0; i < num_keys; i++)
    {
        ASSERT(keys.size() >= 1);

        int key = keys[keys.size() - 1];

        if (call_back)
        {
            // Key wasn't recorded in the first place, so no need to remove
            // it
            if ((*call_back)(this, key, true))
                continue;
        }

        macro_trigger_keys.push_front(key);
        keys.pop_back();
    }
}

void key_recorder::clear()
{
    keys.clear();
    macro_trigger_keys.clear();
}

void add_key_recorder(key_recorder* recorder)
{
    for (int i = 0, size = recorders.size(); i < size; i++)
        ASSERT(recorders[i] != recorder);

    recorders.push_back(recorder);
}

void remove_key_recorder(key_recorder* recorder)
{
    std::vector<key_recorder*>::iterator i;

    for (i = recorders.begin(); i != recorders.end(); i++)
        if (*i == recorder)
        {
            recorders.erase(i);
            return;
        }

    end(1, true, "remove_key_recorder(): recorder not found\n");
}

// Add macro trigger keys to beginning of the buffer, then expand
// them.
void insert_macro_into_buff(const keyseq& keys)
{
    for (int i = (int) keys.size() - 1; i >= 0; i--)
        macro_buf_add(keys[i], true);

    macro_buf_apply_command_macro();
}

int get_macro_buf_size()
{
    return (Buffer.size());
}

///////////////////////////////////////////////////////////////
// Keybinding stuff

#define VALID_BIND_COMMAND(cmd) (cmd > CMD_NO_CMD && cmd < CMD_MIN_SYNTHETIC)

void init_keybindings()
{
    int i;

    for (i = 0; _command_name_list[i].cmd != CMD_NO_CMD
             && _command_name_list[i].name != NULL; i++)
    {
        command_name &data = _command_name_list[i];

        ASSERT(VALID_BIND_COMMAND(data.cmd));
        ASSERT(_names_to_cmds.find(data.name) == _names_to_cmds.end());
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

        ASSERT(context < KC_CONTEXT_COUNT);

        key_to_cmd_map &key_map = _keys_to_cmds[context];
        cmd_to_key_map &cmd_map = _cmds_to_keys[context];

        // Only one command per key, but it's okay to have several
        // keys map to the same command.
        ASSERT(key_map.find(data.key) == key_map.end());

        key_map[data.key] = data.cmd;
        cmd_map[data.cmd] = data.key;
    }

    ASSERT(i >= 130);
}

command_type name_to_command(std::string name)
{
    name_to_cmd_map::iterator it = _names_to_cmds.find(name);

    if (it == _names_to_cmds.end())
        return (CMD_NO_CMD);

    return static_cast<command_type>(it->second);
}

std::string command_to_name(command_type cmd)
{
    cmd_to_name_map::iterator it = _cmds_to_names.find(cmd);

    if (it == _cmds_to_names.end())
        return ("CMD_NO_CMD");

    return (it->second);
}

command_type key_to_command(int key, KeymapContext context)
{
    key_to_cmd_map           &key_map = _keys_to_cmds[context];
    key_to_cmd_map::iterator it       = key_map.find(key);

    if (it == key_map.end())
        return CMD_NO_CMD;

    command_type cmd = static_cast<command_type>(it->second);

    ASSERT(context_for_command(cmd) == context);

    return cmd;
}

int command_to_key(command_type cmd)
{
    KeymapContext context = context_for_command(cmd);

    if (context == KC_NONE)
        return ('\0');

    cmd_to_key_map           &cmd_map = _cmds_to_keys[context];
    cmd_to_key_map::iterator it       = cmd_map.find(cmd);

    if (it == cmd_map.end())
        return ('\0');

    return (it->second);
}

KeymapContext context_for_command(command_type cmd)
{
#ifdef USE_TILE
    if (cmd >= CMD_MIN_TILE && cmd <= CMD_MAX_TILE)
        return KC_TILE;
#endif

    if (cmd > CMD_NO_CMD && cmd <= CMD_MAX_NORMAL)
        return KC_DEFAULT;

    if (cmd >= CMD_MIN_OVERMAP && cmd <= CMD_MAX_OVERMAP)
        return KC_LEVELMAP;

    if (cmd >= CMD_MIN_TARGET && cmd <= CMD_MAX_TARGET)
        return KC_TARGETING;

    return KC_NONE;
}

void bind_command_to_key(command_type cmd, int key)
{
    KeymapContext context      = context_for_command(cmd);
    std::string   command_name = command_to_name(cmd);

    if (context == KC_NONE || command_name == "CMD_NO_CMD"
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
        mpr("Cannot bind user function keys to a command.", MSGCH_ERROR);
        return;
    }

    if (is_synthetic_key(key))
    {
        mpr("Cannot bind synthetic keys to a command.", MSGCH_ERROR);
        return;
    }

    // We're good.
    key_to_cmd_map &key_map = _keys_to_cmds[context];
    cmd_to_key_map &cmd_map = _cmds_to_keys[context];

    key_map[key] = cmd;
    cmd_map[cmd] = key;
}
