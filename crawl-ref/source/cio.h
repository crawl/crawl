/**
 * @file
 * @brief System independent console IO functions
**/

#pragma once

#include <cctype>
#include <string>
#include <vector>

#include "command-type.h"
#include "enum.h"
#include "KeymapContext.h"

#ifdef USE_TILE_LOCAL
 #include "tilebuf.h"
 #include <SDL_keycode.h>
#endif

enum keyfun_action
{
    KEYFUN_PROCESS,
    KEYFUN_IGNORE,
    KEYFUN_CLEAR,
    KEYFUN_BREAK,
};

class input_history
{
public:
    input_history(size_t size);

    void new_input(const string &s);
    void clear();

    const string *prev();
    const string *next();

    void go_end();
private:
    typedef list<string> string_list;

    string_list             history;
    string_list::iterator   pos;
    size_t maxsize;
};

void cursorxy(int x, int y);
static inline void cursorxy(const coord_def& p) { cursorxy(p.x, p.y); }

// Converts a key to a direction key, converting keypad and other sequences
// to vi key sequences (shifted/control key directions are also handled). Non
// direction keys (hopefully) pass through unmangled.
int unmangle_direction_keys(int keyin, KeymapContext keymap = KMC_DEFAULT,
                            bool allow_fake_modifiers = true);

void nowrap_eol_cprintf(PRINTF(0, ));
void wrapcprintf(int wrapcol, const char *s, ...);
void wrapcprintf(const char *s, ...);

// Returns zero if user entered text and pressed Enter, otherwise returns the
// key pressed that caused the exit, usually Escape.
//
// If keyproc is provided, it must return 1 for normal processing, 0 to exit
// normally (pretend the user pressed Enter), or -1 to exit as if the user
// pressed Escape
int cancellable_get_line(char *buf,
                         int len,
                         input_history *mh = nullptr,
                         keyfun_action (*keyproc)(int &c) = nullptr,
                         const string &fill = "",
                         const string &tag = "");

// Do not use this templated function directly. Use the macro below instead.
template<int> static int cancellable_get_line_autohist_temp(char *buf, int len)
{
    static input_history hist(10);
    return cancellable_get_line(buf, len, &hist);
}

// This version of cancellable_get_line will automatically retain its own
// input history, independent of other calls to cancellable_get_line.
#define cancellable_get_line_autohist(buf, len) \
    cancellable_get_line_autohist_temp<__LINE__>(buf, len)

struct c_mouse_event
{
    coord_def pos;
    int       bstate;

    enum button_state_type
    {
        BUTTON1        = 0x1,
        BUTTON1_DBL    = 0x2,
        BUTTON2        = 0x4,
        BUTTON2_DBL    = 0x8,
        BUTTON3        = 0x10,
        BUTTON3_DBL    = 0x20,
        BUTTON4        = 0x40,
        BUTTON4_DBL    = 0x80,
        BUTTON_SCRL_UP = 0x100,
        BUTTON_SCRL_DN = 0x200,
    };

    c_mouse_event() : pos(-1, -1), bstate(0)
    {
    }

    c_mouse_event(const coord_def &c, int state = 0) : pos(c), bstate(state)
    {
    }

    // Returns true for valid events.
    operator bool () const
    {
        return bstate;
    }

    bool left_clicked() const
    {
        return bstate & BUTTON1;
    }

    bool right_clicked() const
    {
        return bstate & BUTTON3;
    }

    bool scroll_up() const
    {
        return bstate & (BUTTON4 | BUTTON4_DBL | BUTTON_SCRL_UP);
    }

    bool scroll_down() const
    {
        return bstate & (BUTTON2 | BUTTON2_DBL | BUTTON_SCRL_DN);
    }
};

c_mouse_event get_mouse_event();
void          new_mouse_event(const c_mouse_event &ce);
void          c_input_reset(bool enable_mouse, bool flush = false);

// Keys that getch() must return for keys Crawl is interested in.
enum KEYS
{
    // Note: keycodes with bit 30 set are reserved for internal SDL keycodes
    // and shouldn't be used for other things

    // We seem to be using the ascii+ characters for SDL.
    // Is this actually correct?
    CK_MAX_NORMAL_CHARACTER_SDL = 255,

    CK_ENTER  = '\r',
    CK_BKSP   = 8,
    CK_TAB    = 9,
    CK_ESCAPE = ESCAPE,

    // note: -256 through approx -600 are reserved for various ncurses keycodes,
    // use at your own risk.
    // For internal keycodes:
    // we use >= -255 as a safe negative range where ascii+ chars are guaranteed
    // to be treated as positive, and <= -1000 as another safe overflow range.
    CK_DELETE = -255,

    // This sequence of enums should not be rearranged.
    CK_UP,
    CK_DOWN,
    CK_LEFT,
    CK_RIGHT,

    CK_INSERT,

    CK_HOME,
    CK_END,
    CK_CLEAR,

    CK_PGUP,
    CK_PGDN,
    CK_TAB_PLACEHOLDER, // unused, here only as an offset

    CK_SHIFT_UP,
    CK_SHIFT_DOWN,
    CK_SHIFT_LEFT,
    CK_SHIFT_RIGHT,

    CK_SHIFT_INSERT,

    CK_SHIFT_HOME,
    CK_SHIFT_END,
    CK_SHIFT_CLEAR,

    CK_SHIFT_PGUP,
    CK_SHIFT_PGDN,
    CK_SHIFT_TAB,

    CK_CTRL_UP,
    CK_CTRL_DOWN,
    CK_CTRL_LEFT,
    CK_CTRL_RIGHT,

    CK_CTRL_INSERT,

    CK_CTRL_HOME,
    CK_CTRL_END,
    CK_CTRL_CLEAR,

    CK_CTRL_PGUP,
    CK_CTRL_PGDN,
    CK_CTRL_TAB,

    CK_CTRL_SHIFT_UP,
    CK_CTRL_SHIFT_DOWN,
    CK_CTRL_SHIFT_LEFT,
    CK_CTRL_SHIFT_RIGHT,

    CK_CTRL_SHIFT_INSERT,

    CK_CTRL_SHIFT_HOME,
    CK_CTRL_SHIFT_END,
    CK_CTRL_SHIFT_CLEAR,

    CK_CTRL_SHIFT_PGUP,
    CK_CTRL_SHIFT_PGDN,
    CK_CTRL_SHIFT_TAB,

    // a bunch of stuff added later than the above list, generalizing on how
    // tab is handled
    CK_ENTER_PLACEHOLDER,
    CK_BKSP_PLACEHOLDER,
    CK_ESCAPE_PLACEHOLDER,
    CK_DELETE_PLACEHOLDER,
    CK_SPACE_PLACEHOLDER,
    CK_SHIFT_ENTER,
    CK_SHIFT_BKSP,
    CK_SHIFT_ESCAPE,
    CK_SHIFT_DELETE,
    CK_SHIFT_SPACE, // probably hard to enter, but still need a placeholder
    CK_CTRL_ENTER,
    CK_CTRL_BKSP,
    CK_CTRL_ESCAPE,
    CK_CTRL_DELETE,
    CK_CTRL_SPACE,
    CK_CTRL_SHIFT_ENTER,
    CK_CTRL_SHIFT_BKSP,
    CK_CTRL_SHIFT_ESCAPE,
    CK_CTRL_SHIFT_DELETE,
    CK_CTRL_SHIFT_SPACE,

    CK_MAX_INTERNAL = CK_CTRL_SHIFT_SPACE,

    // numpad keys are still a mess; see unixcurses_defkeys for the source of
    // some of these bindings. On local console, in my testing, most of the
    // non-numerics still translate as regular versions of their keys.
    CK_NUMPAD_EQUALS   = -1021,
    CK_MIN_INTERNAL = CK_NUMPAD_EQUALS,
    CK_MIN_NUMPAD = CK_NUMPAD_EQUALS,
    CK_NUMPAD_SUBTRACT2 = -1020, // currently used by webtiles only. XX remove..
    CK_NUMPAD_DECIMAL  = -1019,
    CK_NUMPAD_SUBTRACT = -1018,
    CK_NUMPAD_ADD2     = -1017, // mac Terminal.app
    CK_NUMPAD_ADD      = -1016, // normal
    CK_NUMPAD_MULTIPLY = -1015,
    CK_NUMPAD_DIVIDE   = -1012,
    CK_NUMPAD_ENTER    = -1010, // no idea how general this is
    // the numbers themselves are a bit more sane
    CK_NUMPAD_9 = -1009,
    CK_NUMPAD_8,
    CK_NUMPAD_7,
    CK_NUMPAD_6,
    CK_NUMPAD_5,
    CK_NUMPAD_4,
    CK_NUMPAD_3,
    CK_NUMPAD_2,
    CK_NUMPAD_1,
    CK_NUMPAD_0,
    CK_MAX_NUMPAD = CK_NUMPAD_0,

// ugly...
// TODO: should crawl just use one of these internally and convert?
// why stop at F15? (Previously, was F12.) This is what SDL on mac goes up to...
// TODO: maybe useful to extend the ncurses side further, since many terms
// map shifted F-keys to higher values. E.g. mac iTerm (I think this is an
// xterm convention?) maps shift-F1 to F13, etc. But this needs some testing
// on windows as well.
#if defined(TARGET_OS_WINDOWS) // windows console
    CK_F15 = -382, // -(VK_F15 | 256) // XX why...
#else // ncurses console
    CK_F15 = -279, // -KEY_F15 from ncurses
#endif
    CK_F14,
    CK_F13,
    CK_F12,
    CK_F11,
    CK_F10,
    CK_F9,
    CK_F8,
    CK_F7,
    CK_F6,
    CK_F5,
    CK_F4,
    CK_F3,
    CK_F2,
    CK_F1, // (ncurses) -265, aka -KEY_F1
    CK_F0, // is this actually used?

    // SDL only, fairly arbitrary.
    // the whole range from CK_ALT_MIN to CK_ALT_MAX inclusive is reserved
    // for keycodes where the alt key was down
    CK_ALT_BASE = -3000,
    CK_ALT_MAX = CK_ALT_BASE + CK_MAX_NORMAL_CHARACTER_SDL,
    CK_ALT_MIN = CK_ALT_BASE + CK_MIN_INTERNAL,

    // Mouse codes.
    CK_MOUSE_MOVE = -9999,
    CK_MOUSE_CMD,
    CK_MOUSE_B1,
    CK_MOUSE_B2,
    CK_MOUSE_B3,
    CK_MOUSE_B4,
    CK_MOUSE_B5,
    CK_MOUSE_CLICK,
    CK_REDRAW, // no-op to force redraws of things
    CK_RESIZE,

    // so that the handle_mouse loop can be broken from early (for
    // popups), and otherwise for keys to ignore
    CK_NO_KEY,

    // SDL only, fairly arbitrary.
    // the whole range from CK_CMD_MIN to CK_CMD_MAX inclusive is reserved
    // for keycodes where the command key was down
    CK_CMD_BASE = -20000,
    CK_CMD_MAX = CK_CMD_BASE + CK_MAX_NORMAL_CHARACTER_SDL,
    CK_CMD_MIN = CK_CMD_BASE + CK_MIN_INTERNAL,

    // SDL only, fairly arbitrary.
    // the whole range from CK_CMD_ALT_MIN to CK_CMD_ALT_MAX inclusive is
    // reserved for keycodes where the command and alt keys were down
    CK_CMD_ALT_BASE = CK_CMD_BASE + CK_ALT_BASE,
    CK_CMD_ALT_MAX = CK_CMD_ALT_BASE + CK_MAX_NORMAL_CHARACTER_SDL,
    CK_CMD_ALT_MIN = CK_CMD_ALT_BASE + CK_MIN_INTERNAL,

    CK_COMMAND_AS_KEY_MAX = CK_CMD_ALT_MIN - 1,
    CK_COMMAND_AS_KEY_MIN = CK_COMMAND_AS_KEY_MAX - CMD_MAX_CMD - 1,
};
static_assert(CK_ALT_MAX < CK_MIN_INTERNAL, "Keycodes overlap");
static_assert(CK_NO_KEY < CK_ALT_MIN, "Keycodes overlap");
static_assert(CK_CMD_MIN < CK_MOUSE_MOVE, "Keycodes overlap");
static_assert(CK_CMD_ALT_MAX < CK_CMD_MIN, "Keycodes overlap");

// This is an ugly hack to return a command where a keycode is expected
// e.g. when returning mouse input from the getch_ck function
inline int encode_command_as_key(command_type cmd) noexcept
{
    // Don't accept buggy commands
    if (cmd < CMD_NO_CMD || cmd >= CMD_MAX_CMD)
        cmd = CMD_NO_CMD;
    return cmd + CK_COMMAND_AS_KEY_MIN;
}

class cursor_control
{
public:
    cursor_control(bool cursor_enabled)
        : cstate(is_cursor_enabled())
    {
        set_cursor_enabled(cursor_enabled);
    }
    ~cursor_control()
    {
        set_cursor_enabled(cstate);
    }
private:
    bool cstate;
};

enum edit_mode
{
    EDIT_MODE_INSERT,
    EDIT_MODE_OVERWRITE,
};

class draw_colour
{
public:
    draw_colour(COLOURS fg, COLOURS bg);
    ~draw_colour();
    void set();
    void reset();
private:
    COLOURS foreground;
    COLOURS background;
};

// Reads lines of text; used internally by cancellable_get_line.
class line_reader
{
public:
    line_reader(char *buffer, size_t bufsz,
                int wrap_col = get_number_of_cols());
    virtual ~line_reader();

    typedef keyfun_action (*keyproc)(int &key);

    virtual int read_line(bool clear_previous = true, bool reset_cursor = false);
    int read_line(const string &prefill);

    string get_text() const;
    void set_text(string s);

    void set_input_history(input_history *ih);
    void set_keyproc(keyproc fn);

    void set_edit_mode(edit_mode m);
    edit_mode get_edit_mode();

    void set_colour(COLOURS fg, COLOURS bg);
    void set_location(coord_def loc);

    string get_prompt();
    void set_prompt(string p);

    void insert_char_at_cursor(int ch);
    void overwrite_char_at_cursor(int ch);
#ifdef USE_TILE_WEB
    void set_tag(const string &tag);
#endif

protected:
    int read_line_core(bool reset_cursor);
    int process_key_core(int ch);
    virtual void print_segment(int start_point=0, int overprint=0);
    virtual void cursorto(int newcpos);
    virtual int getkey();

    virtual int process_key(int ch);
    void backspace();
    void killword();
    void kill_to_begin();
    void calc_pos();

    bool is_wordchar(char32_t c);

protected:
    char            *buffer;
    size_t          bufsz;
    input_history   *history;
    GotoRegion      region;
    coord_def       start;
    keyproc         keyfn;
    int             wrapcol;
    edit_mode       mode;
    COLOURS         fg_colour;
    COLOURS         bg_colour;
    string          prompt; // currently only used for webtiles input dialogs

#ifdef USE_TILE_WEB
    string          tag; // For identification on the Webtiles client side
#endif

    // These are subject to change during editing.
    char            *cur;
    int             length;
    int             pos;
};

#ifdef USE_TILE_LOCAL
class fontbuf_line_reader : public line_reader
{
public:
    fontbuf_line_reader(char *buffer, size_t bufsz,
            FontBuffer &font_buf,
            int wrap_col = get_number_of_cols());
    int read_line(bool clear_previous = true, bool reset_cursor = false);

protected:
    void print_segment(int start_point=0, int overprint=0);
    void cursorto(int newcpos);
    int getkey();

    FontBuffer& m_font_buf;
};
#endif

class resumable_line_reader : public line_reader
{
public:
    resumable_line_reader(char *buf, size_t sz) : line_reader(buf, sz, sz) { read_line(); };
    int putkey(int ch) { return process_key_core(ch); };
protected:
    virtual int getkey() override { return CK_NO_KEY; };
    virtual int process_key(int ch) override
    {
        return ch == CK_NO_KEY ? CK_NO_KEY : line_reader::process_key(ch);
    }
    virtual void print_segment(int, int) override {};
    void cursorto(int) override {};
    int key;
};

typedef int keycode_type;

keyfun_action keyfun_num_and_char(int &ch);
keycode_type numpad_to_regular(keycode_type key, bool keypad=false);
