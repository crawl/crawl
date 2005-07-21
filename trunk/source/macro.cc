/*
 *  File:       macro.cc
 *  Summary:    Crude macro-capability
 *  Written by: Juho Snellman <jsnell@lyseo.edu.ouka.fi>
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

#ifdef USE_MACROS
#define MACRO_CC
#include "macro.h"

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <deque>

#include <stdio.h>      // for snprintf
#include <ctype.h>      // for tolower

#include "externs.h"

// for trim_string:
#include "initfile.h"

typedef std::deque<int> keyseq;
typedef std::deque<int> keybuf;
typedef std::map<keyseq,keyseq> macromap;

static macromap Keymaps;
static macromap Macros;

static keybuf Buffer;
  
/* 
 * Returns the name of the file that contains macros.
 */
static std::string get_macro_file() 
{
    std::string s;
    
    if (SysEnv.crawl_dir)
        s = SysEnv.crawl_dir;
    
    return (s + "macro.txt");
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
static std::string vtostr( keyseq v ) 
{
    std::string s;
    
    for (keyseq::iterator i = v.begin(); i != v.end(); i++) 
    {
	if (*i < 32 || *i > 127) {
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
static void macro_buf_add( int key ) 
{
    Buffer.push_back( key );	
}


/*
 * Adds keypresses from a sequence into the internal keybuffer. Does some
 * O(N^2) analysis to the sequence to replace macros. 
 */
static void macro_buf_add_long( keyseq actions ) 
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
	    keyseq result = Keymaps[tmp];
	    
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

/*
 * Saves macros into the macrofile, overwriting the old one.
 */
void macro_save( void ) 
{
    std::ofstream f;
    f.open( get_macro_file().c_str() );

    f << "# WARNING: This file is entirely auto-generated." << std::endl
      << std::endl << "# Key Mappings:" << std::endl;

    for (macromap::iterator i = Keymaps.begin(); i != Keymaps.end(); i++) 
    {
	// Need this check, since empty values are added into the 
	// macro struct for all used keyboard commands.
	if ((*i).second.size() > 0) 
        {
	    f << "K:" << vtostr((*i).first) << std::endl 
              << "A:" << vtostr((*i).second) << std::endl << std::endl;
	}
    }

    f << "# Command Macros:" << std::endl;

    for (macromap::iterator i = Macros.begin(); i != Macros.end(); i++) 
    {
	// Need this check, since empty values are added into the 
	// macro struct for all used keyboard commands.
	if ((*i).second.size() > 0) 
        {
	    f << "M:" << vtostr((*i).first) << std::endl 
              << "A:" << vtostr((*i).second) << std::endl << std::endl;
	}
    }
    
    f.close();
}

/*
 * Reads as many keypresses as are available (waiting for at least one), 
 * and returns them as a single keyseq. 
 */
static keyseq getch_mul( void ) 
{
    keyseq keys; 
    int a;
    
    keys.push_back( a = getch() );
    
    // The a == 0 test is legacy code that I don't dare to remove. I 
    // have a vague recollection of it being a kludge for conio support. 
    while ((kbhit() || a == 0)) {
	keys.push_back( a = getch() );
    }
    
    return (keys);
}

/* 
 * Replacement for getch(). Returns keys from the key buffer if available. 
 * If not, adds some content to the buffer, and returns some of it. 
 */
int getchm( void ) 
{
    int a;
        
    // Got data from buffer. 
    if ((a = macro_buf_get()) != -1)
        return (a);

    // Read some keys...
    keyseq keys = getch_mul();
    // ... and add them into the buffer
    macro_buf_add_long( keys );

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
    }

    // Apply longest matching macro at front of buffer:
    macro_buf_apply_command_macro();
        
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

    mpr( "Command (m)acro or (k)eymap? ", MSGCH_PROMPT );
    input = getch();
    if (input == 0)
        input = getch();

    input = tolower( input );
    if (input == 'k')
        keymap = true;
    else if (input == 'm')
        keymap = false;
    else 
    {
        mpr( "Aborting." );
        return;
    }

    // reference to the appropriate mapping
    macromap &mapref = (keymap ? Keymaps : Macros);

    snprintf( info, INFO_SIZE, "Input %s trigger key: ",
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

    keyseq  act;
    char    buff[4096]; 

    get_input_line( buff, sizeof(buff) );
    
    // convert c_str to keyseq 
    const int len = strlen( buff );
    for (int i = 0; i < len; i++)
        act.push_back( buff[i] );

    macro_add( mapref, key, act );
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
      
    f.open( get_macro_file().c_str() );
    
    while (f >> s) 
    {
        trim_string(s);  // remove white space from ends

        if (s[0] == '#') { 
            continue;                   // skip comments

        } else if (s.substr(0, 2) == "K:") {
	    key = parse_keyseq(s.substr(2));
            keymap = true;

        } else if (s.substr(0, 2) == "M:") {
	    key = parse_keyseq(s.substr(2));
            keymap = false;

	} else if (s.substr(0, 2) == "A:") {
	    action = parse_keyseq(s.substr(2));
            macro_add( (keymap ? Keymaps : Macros), key, action );
	}
    }
         
    return (0);
}

#endif
