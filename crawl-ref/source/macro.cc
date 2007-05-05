/*
 *  File:       macro.cc
 *  Summary:    Crude macro-capability
 *  Written by: Juho Snellman <jsnell@lyseo.edu.ouka.fi>
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <3>     6/25/02         JS              Completely rewritten
 *      <2>     6/13/99         BWR             SysEnv.crawl_dir support
 *      <1>     -/--/--         JS              Created
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

#define MACRO_CC
#include "macro.h"

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <deque>
#include <vector>

#include <stdio.h>      // for snprintf
#include <ctype.h>      // for tolower

#include "externs.h"
#include "stuff.h"

// for trim_string:
#include "initfile.h"

typedef std::deque<int> keyseq;
typedef std::deque<int> keybuf;
typedef std::map<keyseq,keyseq> macromap;

static macromap Keymaps[KC_CONTEXT_COUNT];
static macromap Macros;

static macromap *all_maps[] =
{
    &Keymaps[KC_DEFAULT],
    &Keymaps[KC_LEVELMAP],
    &Keymaps[KC_TARGETING],
    &Macros,
};

static keybuf Buffer;

#define USERFUNCBASE -10000
static std::vector<std::string> userfunctions;

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

const char *get_userfunction(int key)
{
    int index = userfunc_index(key);
    return (index == -1? NULL : userfunctions[index].c_str());
}

static const char *get_userfunction(const keyseq &seq)
{
    int index = userfunc_index(seq);
    return (index == -1? NULL : userfunctions[index].c_str());
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

    // Pass 2 to hunt for gaps
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

/* 
 * Returns the name of the file that contains macros.
 */
static std::string get_macro_file() 
{
    std::string dir =
        !Options.macro_dir.empty()? Options.macro_dir :
        !SysEnv.crawl_dir.empty()?  SysEnv.crawl_dir : "";
    
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

/* 
 * Takes as argument a string, and returns a sequence of keys described
 * by the string. Most characters produce their own ASCII code. There
 * are two special cases: 
 *   \\ produces the ASCII code of a single \
 *   \{123} produces 123 (decimal) 
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
    
    for (std::string::iterator i = s.begin(); i != s.end(); i++) 
    {
        char c = *i;
        
        switch (state) 
        {           
        case 0: // Normal state
            if (c == '\\') {
                state = 1;
            } else {
                v.push_back(c);
            }
            break;          

        case 1: // Last char is a '\' 
            if (c == '\\') {
                state = 0;
                v.push_back(c);
            } else if (c == '{') {
                state = 2;
                num = 0;
            } 
            // XXX Error handling
            break;

        case 2: // Inside \{}
            if (c == '}') {
                v.push_back(num);
                state = 0;
            } else if (c >= '0' && c <= '9') {
                num = num * 10 + c - '0';
            } 
            // XXX Error handling
            break;
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
    std::string s;

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
        if (*i <= 32 || *i > 127) {
            char buff[10];

            snprintf( buff, sizeof(buff), "\\{%d}", *i );
            s += std::string( buff );

            // Removing the stringstream code because its highly 
            // non-portable.  For starters, people and compilers 
            // are supposed to be using the <sstream> implementation
            // (with the ostringstream class) not the old <strstream>
            // version, but <sstream> is not as available as it should be.
            //
            // The strstream implementation isn't very standard
            // either:  some compilers require the "ends" marker,
            // others don't (and potentially fatal errors can 
            // happen if you don't have it correct for the system...
            // ie its hard to make portable).  It also isn't a very 
            // good implementation to begin with. 
            //
            // Everyone should have snprintf()... we supply a version
            // in libutil.cc to make sure of that! -- bwr
            //
            // std::ostrstream ss;
            // ss << "\\{" << *i << "}" << ends;
            // s += ss.str();
        } else if (*i == '\\') {
            s += "\\\\";
        } else {
            s += *i;
        }
    }    
    
    return (s);
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
static void macro_buf_add( keyseq actions ) 
{
    for (keyseq::iterator i = actions.begin(); i != actions.end(); i++)
        Buffer.push_back(*i);   
}

/*
 * Adds a single keypress into the internal keybuffer.
 */
void macro_buf_add( int key ) 
{
    Buffer.push_back( key );    
}


/*
 * Adds keypresses from a sequence into the internal keybuffer. Does some
 * O(N^2) analysis to the sequence to replace macros. 
 */
static void macro_buf_add_long( keyseq actions, macromap &keymap = Keymaps[KC_DEFAULT] ) 
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
            if (result.size() > 0) {
                macro_buf_add( result );
                break;
            }
            
            // Didn't find a macro. Remove a key from the end
            // of the sequence, and try again. 
            tmp.pop_back();
        }       
        
        if (tmp.size() == 0) {
            // Didn't find a macro. Add the first keypress of the sequence
            // into the buffer, remove it from the sequence, and try again. 
            macro_buf_add( actions.front() );
            actions.pop_front();

        } else {
            // Found a macro, which has already been added above. Now just
            // remove the macroed keys from the sequence. 
            for (unsigned int i = 0; i < tmp.size(); i++)
                actions.pop_front();
        }
    }
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
        keyseq  result = Macros[tmp]; 

        if (result.size() > 0)
        {
            // Found macro, remove match from front:
            for (unsigned int i = 0; i < tmp.size(); i++)
                Buffer.pop_front();

            // Add macro to front:
            for (keyseq::reverse_iterator k = result.rbegin(); k != result.rend(); k++)
                Buffer.push_front(*k);  

            break;  
        }

        tmp.pop_back();
    }
}

/*
 * Removes the earlies keypress from the keybuffer, and returns its
 * value. If buffer was empty, returns -1;
 */
static int macro_buf_get( void ) 
{
    if (Buffer.size() == 0)
        return (-1);

    int key = Buffer.front();
    Buffer.pop_front();
    
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
void macro_save( void ) 
{
    std::ofstream f;
    f.open( get_macro_file().c_str() );

    f << "# WARNING: This file is entirely auto-generated." << std::endl
      << std::endl << "# Key Mappings:" << std::endl;
    for (int mc = KC_DEFAULT; mc < KC_CONTEXT_COUNT; ++mc) {
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
    int a;
    
    if (!rgetch)
        rgetch = getch;

    keys.push_back( a = rgetch() );
    
    // The a == 0 test is legacy code that I don't dare to remove. I 
    // have a vague recollection of it being a kludge for conio support. 
    while ((kbhit() || a == 0)) {
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
    // ... and add them into the buffer
    macro_buf_add_long( keys, Keymaps[mc] );

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
    if (Options.flush_input[ reason ])
        Buffer.clear();
}

void macro_add_query( void )
{
    unsigned char input;
    bool keymap = false;
    KeymapContext keymc = KC_DEFAULT;

    mesclr();
    mpr( "(m)acro, keymap [(k) default, (x) level-map or (t)argeting], (s)ave?", MSGCH_PROMPT );
    input = getch();
    if (input == 0)
        input = getch();

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

    snprintf( info, INFO_SIZE, "Input %s%s trigger key: ",
               keymap? 
                    (keymc == KC_DEFAULT? "default " :
                     keymc == KC_LEVELMAP? "level-map " :
                                           "targeting ") :
                    "",
              (keymap ? "keymap" : "macro") );

    mpr( info, MSGCH_PROMPT );
    keyseq key = getch_mul();

    cprintf( "%s" EOL, (vtostr( key )).c_str() ); // echo key to screen

    if (mapref[key].size() > 0) 
    {
        snprintf( info, INFO_SIZE, "Current Action: %s", 
                  (vtostr( mapref[key] )).c_str() );

        mpr( info, MSGCH_WARN );
        mpr( "Do you wish to (r)edefine, (c)lear, or (a)bort?", MSGCH_PROMPT );

        input = getch();
        if (input == 0)
            input = getch();

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
int macro_init( void ) 
{
    std::string s;
    std::ifstream f;
    keyseq key, action;
    bool keymap = false;
    KeymapContext keymc = KC_DEFAULT;
      
    f.open( get_macro_file().c_str() );
    
    while (f >> s) 
    {
        trim_string(s);  // remove white space from ends

        if (s[0] == '#') { 
            continue;                   // skip comments

        } else if (s.substr(0, 2) == "K:") {
            key = parse_keyseq(s.substr(2));
            keymap = true;
            keymc  = KC_DEFAULT;
                
        } else if (s.length() >= 3 && s[0] == 'K' && s[2] == ':') {
            keymc  = KeymapContext( KC_DEFAULT + s[1] - '0' );
            if (keymc >= KC_DEFAULT && keymc < KC_CONTEXT_COUNT) {
                key    = parse_keyseq(s.substr(3));
                keymap = true;
            }
        
        } else if (s.substr(0, 2) == "M:") {
            key = parse_keyseq(s.substr(2));
            keymap = false;

        } else if (s.substr(0, 2) == "A:") {
            action = parse_keyseq(s.substr(2));
            macro_add( (keymap ? Keymaps[keymc] : Macros), key, action );
        }
    }
         
    return (0);
}

#ifdef CLUA_BINDINGS
void macro_userfn(const char *keys, const char *regname)
{
    // TODO: Implement.
    // Converting 'keys' to a key sequence is the difficulty. Doing it portably
    // requires a mapping of key names to whatever getch() spits back, unlikely
    // to happen in a hurry.
}
#endif
