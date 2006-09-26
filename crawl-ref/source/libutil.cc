/*
 *  File:       libutil.cc
 *  Summary:    Functions that may be missing from some systems
 *  Written by: ?
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 *      <1> 2001/Nov/01        BWR     Created
 *
 */

#include "AppHdr.h"
#include "defines.h"
#include "libutil.h"
#include "externs.h"
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#ifdef WIN32CONSOLE
    #include <windows.h>

    #ifdef WINMM_PLAY_SOUNDS
        #include <mmsystem.h>
    #endif
#endif

#ifdef REGEX_PCRE
    // Statically link pcre on Windows
    #ifdef WIN32CONSOLE
        #define PCRE_STATIC
    #endif

    #include <pcre.h>
#endif

#ifdef REGEX_POSIX
    // Do we still need to include sys/types.h?
    #include <sys/types.h>
    #include <regex.h>
#endif

// Should return true if the filename contains nothing that
// the shell can do damage with.
bool shell_safe(const char *file)
{
    int match = strcspn(file, "`$*?|><");
    return !(match >= 0 && file[match]);
}

void play_sound( const char *file )
{
#if defined(WINMM_PLAY_SOUNDS)
    // Check whether file exists, is readable, etc.?
    if (file && *file)
        sndPlaySound(file, SND_ASYNC | SND_NODEFAULT);
#elif defined(SOUND_PLAY_COMMAND)
    char command[255];
    command[0] = '\0';
    if (file && *file && (strlen(file) + strlen(SOUND_PLAY_COMMAND) < 255)
            && shell_safe(file))
    {
        snprintf(command, sizeof command, SOUND_PLAY_COMMAND, file);
        system(command);
    }
#endif
}

void get_input_line( char *const buff, int len )
{
    buff[0] = '\0';         // just in case

#if defined(UNIX)
    get_input_line_from_curses( buff, len ); // implemented in libunix.cc
#elif defined(WIN32CONSOLE)
    getstr( buff, len );
#else
    fgets( buff, len, stdin );  // much safer than gets()
#endif

    buff[ len - 1 ] = '\0';  // just in case 

    // Removing white space from the end in order to get rid of any
    // newlines or carriage returns that any of the above might have 
    // left there (ie fgets especially).  -- bwr
    const int end = strlen( buff ); 
    int i; 

    for (i = end - 1; i >= 0; i++) 
    {
        if (isspace( buff[i] ))
            buff[i] = '\0';
        else
            break;
    }
}

#ifdef DOS
static int getch_ck() {
    int c = getch();
    if (!c) {
        switch (c = getch()) {
        case 'O':  return CK_END;
        case 'P':  return CK_DOWN;
        case 'I':  return CK_PGUP;
        case 'H':  return CK_UP;
        case 'G':  return CK_HOME;
        case 'K':  return CK_LEFT;
        case 'Q':  return CK_PGDN;
        case 'M':  return CK_RIGHT;
        case 119:  return CK_CTRL_HOME;
        case 141:  return CK_CTRL_UP;
        case 132:  return CK_CTRL_PGUP;
        case 116:  return CK_CTRL_RIGHT;
        case 118:  return CK_CTRL_PGDN;
        case 145:  return CK_CTRL_DOWN;
        case 117:  return CK_CTRL_END;
        case 115:  return CK_CTRL_LEFT;
        case 'S':  return CK_DELETE;
        }
    }
    return c;
}
#endif

// Hacky wrapper around getch() that returns CK_ codes for keys
// we want to use in cancelable_get_line() and menus.
int c_getch() {
#if defined(DOS) || defined(LINUX) || defined(WIN32CONSOLE)
    return getch_ck();
#else
    return getch();
#endif
}

// cprintf that knows how to wrap down lines (primitive, but what the heck)
int wrapcprintf( int wrapcol, const char *s, ... )
{
    char buf[1000]; // Hard max
    va_list args;
    va_start(args, s);

    // XXX: If snprintf isn't available, vsnprintf probably isn't, either.
    int len = vsnprintf(buf, sizeof buf, s, args);
    int olen = len;
    va_end(args);

    char *run = buf;
    while (len > 0)
    {
        int x = wherex(), y = wherey();

        if (x > wrapcol) // Somebody messed up!
            return 0;
        
        int avail = wrapcol - x + 1;
        int c = 0;
        if (len > avail) {
            c = run[avail];
            run[avail] = 0;
        }
        cprintf("%s", run);
        
        if (len > avail)
            run[avail] = c;

        if ((len -= avail) > 0)
            gotoxy(1, y + 1);
        run += avail;
    }
    return (olen);
}

#define WX(x)   ( ((x) - 1) % maxcol + 1 )
#define WY(x,y) ( (y) + ((x) - 1) / maxcol )
#define WC(x,y) WX(x), WY(x,y)
#define GOTOXY(x,y) gotoxy( WC(x,y) )
bool cancelable_get_line( char *buf, int len, int maxcol,
                          input_history *mh )
{
    if (len <= 0) return false;

    buf[0] = 0;

    char *cur = buf;
    int start = wherex(), line = wherey();
    int length = 0, pos = 0;

    if (mh)
        mh->go_end();

    for ( ; ; ) {
        int ch = c_getch();

        switch (ch) {
        case CK_ESCAPE:
            return false;
        case CK_UP:
        case CK_DOWN:
        {
            if (!mh)
                break;
            const std::string *text = ch == CK_UP? mh->prev() : mh->next();
            if (text)
            {
                int olen = length;
                length = text->length();
                if (length >= len)
                    length = len - 1;
                memcpy(buf, text->c_str(), length);
                buf[length] = 0;
                GOTOXY(start, line);

                int clear = length < olen? olen - length : 0;
                wrapcprintf(maxcol, "%s%*s", buf, clear, "");

                pos = length;
                cur = buf + pos;
                GOTOXY(start + pos, line);
            }
            break;
        }
        case CK_ENTER:
            buf[length] = 0;
            if (mh && length)
                mh->new_input(buf);
            return true;
        case CONTROL('K'):
        {
            // Kill to end of line
            int erase = length - pos;
            if (erase)
            {
                length = pos;
                buf[length] = 0;
                wrapcprintf( maxcol, "%*s", erase, "" );
                GOTOXY(start + pos, line);
            }
            break;
        }
        case CK_DELETE:
            if (pos < length) {
                char *c = cur;
                while (c - buf < length) {
                    *c = c[1];
                    c++;
                }
                --length;

                GOTOXY( start + pos, line );
                buf[length] = 0;
                wrapcprintf( maxcol, "%s ", cur );
                GOTOXY( start + pos, line );
            }
            break;
        case CK_BKSP:
            if (pos) {
                --cur;
                char *c = cur;
                while (*c) {
                    *c = c[1];
                    c++;
                }
                --pos;
                --length;

                GOTOXY( start + pos, line );
                buf[length] = 0;
                wrapcprintf( maxcol, "%s ", cur );
                GOTOXY( start + pos, line );
            }
            break;
        case CK_LEFT:
            if (pos) {
                --pos;
                cur = buf + pos;
                GOTOXY( start + pos, line );
            }
            break;
        case CK_RIGHT:
            if (pos < length) {
                ++pos;
                cur = buf + pos;
                GOTOXY( start + pos, line );
            }
            break;
        case CK_HOME:
        case CONTROL('A'):
            pos = 0;
            cur = buf + pos;
            GOTOXY( start + pos, line );
            break;
        case CK_END:
        case CONTROL('E'):
            pos = length;
            cur = buf + pos;
            GOTOXY( start + pos, line );
            break;
        default:
            if (isprint(ch) && length < len - 1) {
                if (pos < length) {
                    char *c = buf + length - 1;
                    while (c >= cur) {
                        c[1] = *c;
                        c--;
                    }
                }
                *cur++ = (char) ch;
                ++length;
                ++pos;
                putch(ch);
                if (pos < length) {
                    buf[length] = 0;
                    wrapcprintf( maxcol, "%s", cur );
                }
                GOTOXY(start + pos, line);
            }
            break;
        }
    }
}
#undef GOTOXY
#undef WC
#undef WX
#undef WY

// also used with macros
std::string & trim_string( std::string &str )
{
    // OK,  this is really annoying.  Borland C++ seems to define
    // basic_string::erase to take iterators,  and basic_string::remove
    // to take size_t or integer.  This is ass-backwards compared to
    // nearly all other C++ compilers.  Crap.             (GDL)
    //
    // Borland 5.5 does this correctly now... leaving the old code
    // around for now in case anyone needs it.  -- bwr
// #ifdef __BCPLUSPLUS__
//     str.remove( 0, str.find_first_not_of( " \t\n\r" ) );
//     str.remove( str.find_last_not_of( " \t\n\r" ) + 1 );
// #else
    str.erase( 0, str.find_first_not_of( " \t\n\r" ) );
    str.erase( str.find_last_not_of( " \t\n\r" ) + 1 );
// #endif

    return (str);
}

std::vector<std::string> split_string(const char *sep, std::string s)
{
    std::vector<std::string> segments;

    std::string::size_type pos;
    while ((pos = s.find(sep, 0)) != std::string::npos) {
        if (pos > 0)
            segments.push_back(s.substr(0, pos));
        s.erase(0, pos + 1);
    }
    if (s.length() > 0)
        segments.push_back(s);
    
    for (int i = 0, count = segments.size(); i < count; ++i)
        trim_string(segments[i]);
    return segments;
}

// The old school way of doing short delays via low level I/O sync.  
// Good for systems like old versions of Solaris that don't have usleep.
#ifdef NEED_USLEEP

#include <sys/time.h>
#include <sys/types.h>
#include <sys/unistd.h>

void usleep(unsigned long time)
{
    struct timeval timer;

    timer.tv_sec = (time / 1000000L);
    timer.tv_usec = (time % 1000000L);

    select(0, NULL, NULL, NULL, &timer);
}
#endif

// Not the greatest version of snprintf, but a functional one that's 
// a bit safer than raw sprintf().  Note that this doesn't do the
// special behaviour for size == 0, largely because the return value 
// in that case varies depending on which standard is being used (SUSv2 
// returns an unspecified value < 1, whereas C99 allows str == NULL
// and returns the number of characters that would have been written). -- bwr
#ifdef NEED_SNPRINTF

#include <string.h>

int snprintf( char *str, size_t size, const char *format, ... )
{
    va_list argp;
    va_start( argp, format );

    char *buff = new char [ 10 * size ];  // hopefully enough 
    if (!buff)
    {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    vsprintf( buff, format, argp );
    strncpy( str, buff, size );
    str[ size - 1 ] = '\0';

    int ret = strlen( str );  
    if ((unsigned int) ret == size - 1 && strlen( buff ) >= size)
        ret = -1;

    delete [] buff;

    va_end( argp );

    return (ret);
}

#endif

///////////////////////////////////////////////////////////////////////
// Pattern matching

inline int pm_lower(int ch, bool icase) {
    return icase? tolower(ch) : ch;
}

// Determines whether the pattern specified by 'pattern' matches the given
// text. A pattern is a simple glob, with the traditional * and ? wildcards.
static bool glob_match( const char *pattern, const char *text, bool icase )
{
    char p, t;
    bool special;

    for (;;)
    {
        p = pm_lower(*pattern++, icase);
        t = pm_lower(*text++, icase);
        special = true;

        if (!p) return t == 0;
        if (p == '\\' && *pattern)
        {
            p       = pm_lower(*pattern++, icase);
            special = false;
        }

        if (p == '*' && special)
            // Try to match exactly at the current text position...
            return !*pattern || glob_match(pattern, text - 1, icase)? true :
                // Or skip one character in the text and try the wildcard
                // match again. If this is the end of the text, the match has
                // failed.
                t? glob_match(pattern - 1, text, icase) : false;
        else if (!t || (p != t && (p != '?' || !special)))
            return false;
    }
}

#if defined(REGEX_PCRE)
////////////////////////////////////////////////////////////////////
// Perl Compatible Regular Expressions

void *compile_pattern(const char *pattern, bool icase)
{
    const char *error;
    int erroffset;
    int flags = icase? PCRE_CASELESS : 0;
    return pcre_compile(pattern, 
                        flags,
                        &error,
                        &erroffset,
                        NULL);
}

void free_compiled_pattern(void *cp)
{
    if (cp)
        pcre_free(cp);
}

bool pattern_match(void *compiled_pattern, const char *text, int length)
{
    int ovector[42];
    int pcre_rc = pcre_exec(static_cast<pcre *>(compiled_pattern),
              NULL,
              text,
              length,
              0,
              0,
              ovector,
              sizeof(ovector) / sizeof(*ovector));
    return (pcre_rc >= 0);
}

////////////////////////////////////////////////////////////////////
#elif defined(REGEX_POSIX)
////////////////////////////////////////////////////////////////////
// POSIX regular expressions

void *compile_pattern(const char *pattern, bool icase)
{
    regex_t *re = new regex_t;
    if (!re)
        return NULL;

    int flags = REG_EXTENDED | REG_NOSUB;
    if (icase)
        flags |= REG_ICASE;
    int rc = regcomp(re, pattern, flags);
    // Nonzero return code == failure
    if (rc)
    {
        delete re;
        return NULL;
    }
    return re;
}

void free_compiled_pattern(void *cp)
{
    if (cp)
    {
        regex_t *re = static_cast<regex_t *>( cp );
        regfree(re);
        delete re;
    }
}

bool pattern_match(void *compiled_pattern, const char *text, int length)
{
    regex_t *re = static_cast<regex_t *>( compiled_pattern );
    return !regexec(re, text, 0, NULL, 0);
}

////////////////////////////////////////////////////////////////////
#else
////////////////////////////////////////////////////////////////////
// Basic glob

struct glob_info
{
    std::string s;
    bool ignore_case;
};

void *compile_pattern(const char *pattern, bool icase) 
{
    // If we're using simple globs, we need to box the pattern with '*'
    std::string s = std::string("*") + pattern + "*";
    glob_info *gi = new glob_info;
    if (gi)
    {
        gi->s = s;
        gi->ignore_case = icase;
    }
    return gi;
}

void free_compiled_pattern(void *compiled_pattern)
{
    delete static_cast<glob_info *>( compiled_pattern );
}

bool pattern_match(void *compiled_pattern, const char *text, int length)
{
    glob_info *gi = static_cast<glob_info *>( compiled_pattern );
    return glob_match(gi->s.c_str(), text, gi->ignore_case);
}
////////////////////////////////////////////////////////////////////

#endif



////////////////////////////////////////////////////////////////////
// formatted_string
//

formatted_string::formatted_string(const std::string &s)
    : ops()
{
    ops.push_back( s );
}

formatted_string::operator std::string() const
{
    std::string s;
    for (int i = 0, size = ops.size(); i < size; ++i)
    {
        if (ops[i] == FSOP_TEXT)
            s += ops[i].text;
    }
    return s;
}

inline void cap(int &i, int max)
{
    if (i < 0 && -i <= max)
        i += max;
    if (i >= max)
        i = max - 1;
    if (i < 0)
        i = 0;
}

std::string formatted_string::tostring(int s, int e) const
{
    std::string st;
    
    int size = ops.size();
    cap(s, size);    
    cap(e, size);
    
    for (int i = s; i <= e && i < size; ++i)
    {
        if (ops[i] == FSOP_TEXT)
            st += ops[i].text;
    }
    return st;
}

void formatted_string::display(int s, int e) const
{
    int size = ops.size();
    if (!size)
        return ;

    cap(s, size);    
    cap(e, size);
    
    for (int i = s; i <= e && i < size; ++i)
        ops[i].display();
}

void formatted_string::gotoxy(int x, int y)
{
    ops.push_back( fs_op(x, y) );
}

void formatted_string::textcolor(int color)
{
    ops.push_back(color);
}

void formatted_string::cprintf(const char *s, ...)
{
    char buf[1000];
    va_list args;
    va_start(args, s);
    vsnprintf(buf, sizeof buf, s, args);
    va_end(args);

    cprintf(std::string(buf));
}

void formatted_string::cprintf(const std::string &s)
{
    ops.push_back(s);
}

void formatted_string::fs_op::display() const
{
    switch (type)
    {
    case FSOP_CURSOR:
    {
        int cx = (x == -1? wherex() : x);
        int cy = (y == -1? wherey() : y);
        ::gotoxy(cx, cy);
        break;
    }
    case FSOP_COLOUR:
        ::textcolor(x);
        break;
    case FSOP_TEXT:
        ::cprintf("%s", text.c_str());
        break;
    }
}

/////////////////////////////////////////////////////////////
// input_history
//

input_history::input_history(size_t size)
    : history(), pos(), maxsize(size)
{
    if (maxsize < 2)
        maxsize = 2;

    pos = history.end();
}

void input_history::new_input(const std::string &s)
{
    history.remove(s);

    if (history.size() == maxsize)
        history.pop_front();

    history.push_back(s);

    // Force the iterator to the end (also revalidates it)
    go_end();
}

const std::string *input_history::prev()
{
    if (history.empty())
        return NULL;

    if (pos == history.begin())
        pos = history.end();

    return &*--pos;
}

const std::string *input_history::next()
{
    if (history.empty())
        return NULL;

    if (pos == history.end() || ++pos == history.end())
        pos = history.begin();

    return &*pos;
}

void input_history::go_end()
{
    pos = history.end();
}

void input_history::clear()
{
    history.clear();
    go_end();
}
