/**
 * @file
 * @brief Functions that may be missing from some systems
**/

#include "AppHdr.h"

#include "libutil.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sstream>

#include "colour.h"
#include "files.h"
#include "message.h"
#include "sound.h"
#include "state.h"
#include "stringutil.h"
#include "tiles-build-specific.h"
#include "unicode.h"
#include "viewgeom.h"

#ifdef TARGET_OS_WINDOWS
    #undef ARRAYSZ
    #include <windows.h>
    #undef max

    #ifdef WINMM_PLAY_SOUNDS
        #include <mmsystem.h>
    #endif
#endif

#ifdef UNIX
    #include <csignal>
#endif

#ifdef DGL_ENABLE_CORE_DUMP
    #include <sys/time.h>
    #include <sys/resource.h>
#endif

unsigned int isqrt(unsigned int a)
{
    unsigned int rem = 0, root = 0;
    for (int i = 0; i < 16; i++)
    {
        root <<= 1;
        rem = (rem << 2) + (a >> 30);
        a <<= 2;
        if (++root <= rem)
            rem -= root++;
        else
            root--;
    }
    return root >> 1;
}

int isqrt_ceil(int x)
{
    if (x <= 0)
        return 0;
    return isqrt(x - 1) + 1;
}

description_level_type description_type_by_name(const char *desc)
{
    if (!desc)
        return DESC_PLAIN;

    if (!strcmp("The", desc) || !strcmp("the", desc))
        return DESC_THE;
    else if (!strcmp("A", desc) || !strcmp("a", desc))
        return DESC_A;
    else if (!strcmp("Your", desc) || !strcmp("your", desc))
        return DESC_YOUR;
    else if (!strcmp("its", desc))
        return DESC_ITS;
    else if (!strcmp("worn", desc))
        return DESC_INVENTORY_EQUIP;
    else if (!strcmp("inv", desc))
        return DESC_INVENTORY;
    else if (!strcmp("none", desc))
        return DESC_NONE;
    else if (!strcmp("base", desc))
        return DESC_BASENAME;
    else if (!strcmp("qual", desc))
        return DESC_QUALNAME;

    return DESC_PLAIN;
}

// Should return true if the filename contains nothing that
// the shell can do damage with.
bool shell_safe(const char *file)
{
    int match = strcspn(file, "\\`$*?|><&\n!;");
    return match < 0 || !file[match];
}

bool key_is_escape(int key)
{
    switch (key)
    {
    CASE_ESCAPE
        return true;
    default:
        return false;
    }
}

// Returns true if s contains tag 'tag', and strips out tag from s.
bool strip_tag(string &s, const string &tag, bool skip_padding)
{
    if (s == tag)
    {
        s.clear();
        return true;
    }

    string::size_type pos;

    if (skip_padding)
    {
        if ((pos = s.find(tag)) != string::npos)
        {
            s.erase(pos, tag.length());
            trim_string(s);
            return true;
        }
        return false;
    }

    if ((pos = s.find(" " + tag + " ")) != string::npos)
    {
        // Leave one space intact.
        s.erase(pos, tag.length() + 1);
        trim_string(s);
        return true;
    }

    if ((pos = s.find(tag + " ")) == 0
        || ((pos = s.find(" " + tag)) != string::npos
            && pos + tag.length() + 1 == s.length()))
    {
        s.erase(pos, tag.length() + 1);
        trim_string(s);
        return true;
    }

    return false;
}

vector<string> strip_multiple_tag_prefix(string &s, const string &tagprefix)
{
    vector<string> results;

    while (true)
    {
        string this_result = strip_tag_prefix(s, tagprefix);
        if (this_result.empty())
            break;

        results.push_back(this_result);
    }

    return results;
}

string strip_tag_prefix(string &s, const string &tagprefix)
{
    string::size_type pos = s.find(tagprefix);

    while (pos && pos != string::npos && !isspace(s[pos - 1]))
        pos = s.find(tagprefix, pos + 1);

    if (pos == string::npos)
        return "";

    string::size_type ns = s.find(" ", pos);
    if (ns == string::npos)
        ns = s.length();

    const string argument = s.substr(pos + tagprefix.length(),
                                     ns - pos - tagprefix.length());

    s.erase(pos, ns - pos + 1);
    trim_string(s);

    return argument;
}

const string tag_without_prefix(const string &s, const string &tagprefix)
{
    string::size_type pos = s.find(tagprefix);

    while (pos && pos != string::npos && !isspace(s[pos - 1]))
        pos = s.find(tagprefix, pos + 1);

    if (pos == string::npos)
        return "";

    string::size_type ns = s.find(" ", pos);
    if (ns == string::npos)
        ns = s.length();

    return s.substr(pos + tagprefix.length(), ns - pos - tagprefix.length());
}

int strip_number_tag(string &s, const string &tagprefix)
{
    const string num = strip_tag_prefix(s, tagprefix);
    int x;
    if (num.empty() || !parse_int(num.c_str(), x))
        return TAG_UNFOUND;
    return x;
}

set<string> parse_tags(const string &tags)
{
    vector<string> split_tags = split_string(" ", tags);
    set<string> ordered_tags(split_tags.begin(), split_tags.end());
    return ordered_tags;
}

bool parse_int(const char *s, int &i)
{
    if (!s || !*s)
        return false;
    char *err;
    long x = strtol(s, &err, 10);
    if (*err || x < INT_MIN || x > INT_MAX)
        return false;
    i = x;
    return true;
}

/**
 * Compare two strings, sorting integer numeric parts according to their value.
 *
 * "foo123bar" > "foo99bar"
 * "0.10" > "0.9" (version sort)
 *
 * @param a String one.
 * @param b String two.
 * @param limit If passed, comparison ends after X numeric parts.
 * @return As in strcmp().
**/
int numcmp(const char *a, const char *b, int limit)
{
    int res;

    do
    {
        while (*a && *a == *b && !isadigit(*a))
        {
            a++;
            b++;
        }
        if (!isadigit(*a) || !isadigit(*b))
            return (*a < *b) ? -1 : (*a > *b) ? 1 : 0;
        while (*a == '0')
            a++;
        while (*b == '0')
            b++;
        res = 0;
        while (isadigit(*a))
        {
            if (!isadigit(*b))
                return 1;
            if (*a != *b && !res)
                res = (*a < *b) ? -1 : 1;
            a++;
            b++;
        }
        if (isadigit(*b))
            return -1;
        if (res)
            return res;
    } while (--limit > 0);
    return 0;
}

// make STL sort happy
bool numcmpstr(const string& a, const string& b)
{
    return numcmp(a.c_str(), b.c_str()) == -1;
}

bool version_is_stable(const char *v)
{
    // vulnerable to changes in the versioning scheme
    for (;; v++)
    {
        if (*v == '.' || isadigit(*v))
            continue;
        if (*v == '-')
            return isadigit(v[1]);
        return true;
    }
}

static void inline _untag(string &s, const string &pre,
                          const string &post, bool onoff)
{
    size_t p = 0;
    while ((p = s.find(pre, p)) != string::npos)
    {
        size_t q = s.find(post, p);
        if (q == string::npos)
            q = s.length();
        if (onoff)
        {
            s.erase(q, post.length());
            s.erase(p, pre.length());
        }
        else
            s.erase(p, q - p + post.length());
    }
}

string untag_tiles_console(string s)
{
    _untag(s, "<tiles>", "</tiles>", is_tiles());
    _untag(s, "<console>", "</console>", !is_tiles());
#ifdef USE_TILE_WEB
    _untag(s, "<webtiles>", "</webtiles>", true);
#else
    _untag(s, "<webtiles>", "</webtiles>", false);
#endif
#ifdef USE_TILE_LOCAL
    _untag(s, "<localtiles>", "</localtiles>", true);
    _untag(s, "<nomouse>", "</nomouse>", false);
#else
    _untag(s, "<localtiles>", "</localtiles>", false);
    _untag(s, "<nomouse>", "</nomouse>", true);
#endif
    return s;
}

string colour_string(string in, int col)
{
    if (in.empty())
        return in;
    const string cols = colour_to_str(col);
    return "<" + cols + ">" + in + "</" + cols + ">";
}



#ifndef USE_TILE_LOCAL
static coord_def _cgettopleft(GotoRegion region)
{
    switch (region)
    {
    case GOTO_MLIST:
        return crawl_view.mlistp;
    case GOTO_STAT:
        return crawl_view.hudp;
    case GOTO_MSG:
        return crawl_view.msgp;
    case GOTO_DNGN:
        return crawl_view.viewp;
    case GOTO_CRT:
    default:
        return crawl_view.termp;
    }
}

coord_def cgetpos(GotoRegion region)
{
    const coord_def where = coord_def(wherex(), wherey());
    return where - _cgettopleft(region) + coord_def(1, 1);
}

static GotoRegion _current_region = GOTO_CRT;

void cgotoxy(int x, int y, GotoRegion region)
{
    _current_region = region;
    const coord_def tl = _cgettopleft(region);
    const coord_def sz = cgetsize(region);

#ifdef ASSERTS
    if (x < 1 || y < 1 || x > sz.x || y > sz.y)
    {
        save_game(false); // should be safe
        die("screen write out of bounds: (%d,%d) into (%d,%d)", x, y,
            sz.x, sz.y);
    }
#endif

    gotoxy_sys(tl.x + x - 1, tl.y + y - 1);

#ifdef USE_TILE_WEB
    tiles.cgotoxy(x, y, region);
#endif
}

GotoRegion get_cursor_region()
{
    return _current_region;
}

void set_cursor_region(GotoRegion region)
{
    _current_region = region;
}
#endif // !USE_TILE_LOCAL

coord_def cgetsize(GotoRegion region)
{
    switch (region)
    {
    case GOTO_MLIST:
        return crawl_view.mlistsz;
    case GOTO_STAT:
        return crawl_view.hudsz;
    case GOTO_MSG:
        return crawl_view.msgsz;
    case GOTO_DNGN:
        return crawl_view.viewsz;
    case GOTO_CRT:
    default:
        return crawl_view.termsz;
    }
}

void cscroll(int n, GotoRegion region)
{
    // only implemented for the message window right now
    if (region == GOTO_MSG)
        scroll_message_window(n);
}

mouse_mode mouse_control::ms_current_mode = MOUSE_MODE_NORMAL;

mouse_control::mouse_control(mouse_mode mode)
{
    m_previous_mode = ms_current_mode;
    ms_current_mode = mode;

#ifdef USE_TILE_WEB
    if (m_previous_mode != ms_current_mode)
        tiles.update_input_mode(mode);
#endif
}

mouse_control::~mouse_control()
{
#ifdef USE_TILE_WEB
    if (m_previous_mode != ms_current_mode)
        tiles.update_input_mode(m_previous_mode);
#endif
    ms_current_mode = m_previous_mode;
}

string unwrap_desc(string&& desc)
{
    // Don't append a newline to an empty description.
    if (desc.empty())
        return "";

    trim_string_right(desc);

    // Lines beginning with a colon are tags with a full entry scope
    // Only nowrap is supported for now
    while (desc[0] == ':')
    {
        int pos = desc.find("\n");
        string tag = desc.substr(1, pos - 1);
        desc.erase(0, pos + 1);
        if (tag == "nowrap")
            return desc;
        else if (desc.empty())
            return "";
    }

    // An empty line separates paragraphs.
    desc = replace_all(desc, "\n\n", "\\n\\n");
    // Indented lines are pre-formatted.
    desc = replace_all(desc, "\n ", "\\n ");

    // Don't add whitespaces between tags
    desc = replace_all(desc, ">\n<", "><");
    // Newlines are still whitespace.
    desc = replace_all(desc, "\n", " ");
    // Can force a newline with a literal "\n".
    desc = replace_all(desc, "\\n", "\n");

    return desc + "\n";
}

#ifdef TARGET_OS_WINDOWS
// FIXME: This function should detect if aero is running, but the DwmIsCompositionEnabled
// function isn't included in msys, so I don't know how to do that. Instead, I just check
// if we are running vista or higher. -rla
static bool _is_aero()
{
    OSVERSIONINFOEX osvi;
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (GetVersionEx((OSVERSIONINFO *) &osvi))
        return osvi.dwMajorVersion >= 6;
    else
        return false;
}

taskbar_pos get_taskbar_pos()
{
    RECT rect;
    HWND taskbar = FindWindow("Shell_traywnd", nullptr);
    if (taskbar && GetWindowRect(taskbar, &rect))
    {
        if (rect.right - rect.left > rect.bottom - rect.top)
        {
            if (rect.top > 0)
                return TASKBAR_BOTTOM;
            else
                return TASKBAR_TOP;
        }
        else
        {
            if (rect.left > 0)
                return TASKBAR_RIGHT;
            else
                return TASKBAR_LEFT;
        }
    }
    return TASKBAR_NO;
}

int get_taskbar_size()
{
    RECT rect;
    int size;
    taskbar_pos tpos = get_taskbar_pos();
    HWND taskbar = FindWindow("Shell_traywnd", nullptr);

    if (taskbar && GetWindowRect(taskbar, &rect))
    {
        if (tpos & TASKBAR_H)
            size = rect.bottom - rect.top;
        else if (tpos & TASKBAR_V)
            size = rect.right - rect.left;
        else
            return 0;

        if (_is_aero())
            size += 3; // Taskbar behave strangely when aero is active.

        return size;
    }
    return 0;
}

static BOOL WINAPI console_handler(DWORD sig)
{
    switch (sig)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        return true; // block the signal
    default:
        return false;
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        if (crawl_state.seen_hups++)
            return true; // abort immediately

#ifndef USE_TILE_LOCAL
        // Should never happen in tiles -- if it does (Cygwin?), this will
        // kill the game without saving.
        w32_insert_escape();

        Sleep(15000); // allow 15 seconds for shutdown, then kill -9
#endif
        return true;
    }
}

void init_signals()
{
    // If there's no console, this will return an error, which we ignore.
    // For GUI programs there's no controlling terminal, but there might be
    // one on Cygwin.
    SetConsoleCtrlHandler(console_handler, true);
}

void release_cli_signals()
{
    SetConsoleCtrlHandler(nullptr, false);
}

void text_popup(const string& text, const wchar_t *caption)
{
    MessageBoxW(0, OUTW(text), caption, MB_OK);
}
#else
#ifndef USE_TILE_LOCAL // is curses in use?

/* [ds] This SIGHUP handling is primitive and far from safe, but it
 * should be better than nothing. Feel free to get rigorous on this.
 */
static void handle_hangup(int)
{
    if (crawl_state.seen_hups++)
        return;

    // When using Curses, closing stdin will cause any Curses call blocking
    // on key-presses to immediately return, including any call that was
    // still blocking in the main thread when the HUP signal was caught.
    // This should guarantee that the main thread will un-stall and
    // will eventually return to _input() in main.cc, which will then
    // gracefully save the game.

    // SAVE CORRUPTING BUG!!!  We're in a signal handler, calling free()
    // when closing the FILE object is likely to lead to lock-ups, and even
    // if it were a plain kernel-side descriptor, calling functions such
    // as select() or read() is undefined behaviour.
    fclose(stdin);
}
# endif

void init_signals()
{
#ifdef DGAMELAUNCH
    // Force timezone to UTC.
    setenv("TZ", "", 1);
    tzset();
#endif

#ifdef USE_UNIX_SIGNALS
#ifdef SIGQUIT
    signal(SIGQUIT, SIG_IGN);
#endif

#ifdef SIGINT
    signal(SIGINT, SIG_IGN);
#endif

# ifdef USE_TILE_LOCAL
    // Losing the controlling terminal doesn't matter, we continue and will
    // shut down only when the actual window is closed.
    signal(SIGHUP, SIG_IGN);
# else
    signal(SIGHUP, handle_hangup);
# endif
#endif

#ifdef DGL_ENABLE_CORE_DUMP
    rlimit lim;
    if (!getrlimit(RLIMIT_CORE, &lim))
    {
        lim.rlim_cur = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &lim);
    }
#endif
}

void release_cli_signals()
{
#ifdef USE_UNIX_SIGNALS
    signal(SIGQUIT, SIG_DFL);
    signal(SIGINT, SIG_DFL);
#endif
}

#endif
