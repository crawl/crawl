/**
 * @file
 * @brief System independent console IO functions
**/

#ifndef CIO_H
#define CIO_H

#include "enum.h"

#include <cctype>
#include <string>
#include <vector>

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

// Read one key, flag it as a mouse event if appropriate, but apply no
// other conversions. Defined in lib$OS.cc, not in cio.cc.
int m_getch();

// Converts a key to a direction key, converting keypad and other sequences
// to vi key sequences (shifted/control key directions are also handled). Non
// direction keys (hopefully) pass through unmangled.
int unmangle_direction_keys(int keyin, KeymapContext keymap = KMC_DEFAULT,
                            bool fake_ctrl = true, bool fake_shift = true);

void nowrap_eol_cprintf(PRINTF(0, ));

// Returns zero if user entered text and pressed Enter, otherwise returns the
// key pressed that caused the exit, usually Escape.
//
// If keyproc is provided, it must return 1 for normal processing, 0 to exit
// normally (pretend the user pressed Enter), or -1 to exit as if the user
// pressed Escape
int cancellable_get_line(char *buf,
                         int len,
                         input_history *mh = NULL,
                         int (*keyproc)(int &c) = NULL,
                         const string &fill = "");

// Do not use this templated function directly.  Use the macro below instead.
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
        return (bstate & (BUTTON4 | BUTTON4_DBL | BUTTON_SCRL_UP));
    }

    bool scroll_down() const
    {
        return (bstate & (BUTTON2 | BUTTON2_DBL | BUTTON_SCRL_DN));
    }
};

c_mouse_event get_mouse_event();
void          new_mouse_event(const c_mouse_event &ce);
void          c_input_reset(bool enable_mouse, bool flush = false);

// Keys that getch() must return for keys Crawl is interested in.
enum KEYS
{
    CK_ENTER  = '\r',
    CK_BKSP   = 8,
    CK_ESCAPE = ESCAPE,

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
    CK_TAB_TILE, // unused

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

#ifdef TOUCH_UI
    // extra numpad keys for zoom
    CK_NUMPAD_PLUS,
    CK_NUMPAD_MINUS,
#endif

    // Mouse codes.
    CK_MOUSE_MOVE  = -10009,
    CK_MOUSE_CMD,
    CK_MOUSE_B1,
    CK_MOUSE_B2,
    CK_MOUSE_B3,
    CK_MOUSE_B4,
    CK_MOUSE_B5,
    CK_MOUSE_CLICK,
    CK_TOUCH_DUMMY, // so a non-event can be passed from handle_mouse to the controlling code
    CK_NO_KEY // so that the handle_mouse loop can be broken from early (for popups)
};

class cursor_control
{
public:
    cursor_control(bool cursor_enabled)
        : cstate(is_cursor_enabled()), smartcstate(is_smart_cursor_enabled())
    {
        enable_smart_cursor(false);
        set_cursor_enabled(cursor_enabled);
    }
    ~cursor_control()
    {
        set_cursor_enabled(cstate);
        enable_smart_cursor(smartcstate);
    }
private:
    bool cstate;
    bool smartcstate;
};

// Reads lines of text; used internally by cancellable_get_line.
class line_reader
{
public:
    line_reader(char *buffer, size_t bufsz,
                int wrap_col = get_number_of_cols());
    virtual ~line_reader();

    typedef int (*keyproc)(int &key);

    int read_line(bool clear_previous = true);
    int read_line(const string &prefill);

    string get_text() const;

    void set_input_history(input_history *ih);
    void set_keyproc(keyproc fn);

protected:
    void cursorto(int newcpos);
    virtual int process_key(int ch);
    void backspace();
    void killword();
    void kill_to_begin();
    void calc_pos();

    bool is_wordchar(ucs_t c);

protected:
    char            *buffer;
    size_t          bufsz;
    input_history   *history;
    GotoRegion      region;
    coord_def       start;
    keyproc         keyfn;
    int             wrapcol;

    string          tag; // For identification on the Webtiles client side

    // These are subject to change during editing.
    char            *cur;
    int             length;
    int             pos;
};

typedef int keycode_type;

#endif
