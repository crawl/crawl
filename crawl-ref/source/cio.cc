/**
 * @file
 * @brief Platform-independent console IO functions.
**/

#include "AppHdr.h"

#include "cio.h"

#include <queue>

#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "options.h"
#include "output.h"
#include "state.h"
#include "stringutil.h"
#include "tiles-build-specific.h"
#include "unicode.h"
#include "viewgeom.h"
#if defined(USE_TILE_LOCAL) && defined(TOUCH_UI)
#include "windowmanager.h"
#endif
#ifdef USE_TILE_LOCAL
#include "tilefont.h"
#endif
#include "ui.h"

// only used in fake shift/ctrl handling, I guess this is to really make sure
// they work?
static keycode_type _numpad2vi(keycode_type key)
{
#if defined(UNIX) && !defined(USE_TILE_LOCAL)
    key = unixcurses_get_vi_key(key);
#endif
    switch (key)
    {
    case CK_UP:    key = 'k'; break;
    case CK_DOWN:  key = 'j'; break;
    case CK_LEFT:  key = 'h'; break;
    case CK_RIGHT: key = 'l'; break;
#if defined(UNIX)
    case CK_NUMPAD_1:    key = 'b'; break;
    case CK_NUMPAD_2:    key = 'j'; break;
    case CK_NUMPAD_3:    key = 'n'; break;
    case CK_NUMPAD_4:    key = 'h'; break;
    case CK_NUMPAD_5:    key = '.'; break;
    case CK_NUMPAD_6:    key = 'l'; break;
    case CK_NUMPAD_7:    key = 'y'; break;
    case CK_NUMPAD_8:    key = 'k'; break;
    case CK_NUMPAD_9:    key = 'u'; break;
#endif
    }
    if (key >= '1' && key <= '9')
    {
        const char *vikeys = "bjnh.lyku";
        return keycode_type(vikeys[key - '1']);
    }
    return key;
}

keycode_type numpad_to_regular(keycode_type key, bool keypad)
{
    switch (key)
    {
    case CK_NUMPAD_SUBTRACT:
    case CK_NUMPAD_SUBTRACT2:
        return '-';
    case CK_NUMPAD_ADD:
    case CK_NUMPAD_ADD2:
        return '+';
    case CK_NUMPAD_DECIMAL:
        return '.';
    case CK_NUMPAD_MULTIPLY:
        return '*';
    case CK_NUMPAD_DIVIDE:
        return '/';
    case CK_NUMPAD_EQUALS:
        return '=';
    case CK_NUMPAD_ENTER:
        return CK_ENTER;
#define KP_PAIR(a, b) (keypad ? static_cast<keycode_type>(a) : static_cast<keycode_type>(b))
    case CK_NUMPAD_7:
        return KP_PAIR(CK_HOME, '7');
    case CK_NUMPAD_8:
        return KP_PAIR(CK_UP, '8');
    case CK_NUMPAD_9:
        return KP_PAIR(CK_PGUP, '9');
    case CK_NUMPAD_4:
        return KP_PAIR(CK_LEFT, '4');
    case CK_NUMPAD_5:
        return '5';
    case CK_NUMPAD_6:
        return KP_PAIR(CK_RIGHT, '6');
    case CK_NUMPAD_1:
        return KP_PAIR(CK_END, '1');
    case CK_NUMPAD_2:
        return KP_PAIR(CK_DOWN, '2');
    case CK_NUMPAD_3:
        return KP_PAIR(CK_PGDN, '3');
    case CK_NUMPAD_0:
        return '0';
#undef KP_PAIR
    default:
        return key;
    }
}

// Save and restore the cursor region.
class unwind_cursor
{
public:
    unwind_cursor()
        : region(get_cursor_region()), pos(cgetpos(get_cursor_region()))
    { }
    unwind_cursor(int x, int y, GotoRegion reg) : unwind_cursor()
    {
        cgotoxy(x, y, reg);
    }
    ~unwind_cursor()
    {
        cgotoxy(pos.x, pos.y, region);
    }
private:
    GotoRegion region;
    coord_def pos;
};

// Convert letters (and 6 other characters) to control codes. Don't change
// anything else.
// Uses int instead of char32_t to match the caller.
static inline int _control_safe(int c)
{
    // XX not strictly accurate for local tiles
    if (c >= 'A'-1 && c < 'A'+' '-1) // ASCII letters and @ [ \ ] ^ _ `
        return CONTROL(c);
    else if (c >= 'a' && c <= 'z') // ASCII letters
        return LC_CONTROL(c);
    else
        return c; // anything else
}

static bool _check_numpad(int key, int tocheck, KeymapContext keymap)
{
    // collapse numpad keys, but only if there is no keybinding for a numpad
    // key specifically.
    // TODO: should this pattern be used basically everywhere?
    const int remapped = numpad_to_regular(key);
    return remapped == tocheck &&
                (remapped == key || key_to_command(key, keymap) == CMD_NO_CMD);
}

int unmangle_direction_keys(int keyin, KeymapContext keymap,
                            bool allow_fake_modifiers)
{
    // Kludging running and opening as two character sequences.
    // This is useful when you can't use control keys (macros, lua) or have
    // them bound to something on your system.

    if (allow_fake_modifiers && Options.use_modifier_prefix_keys)
    {
        /* can we say yuck? -- haranp */
        if (_check_numpad(keyin, '*', keymap))
        {
            unwind_cursor saved(1, crawl_view.msgsz.y, GOTO_MSG);
            cprintf("CTRL");
            webtiles_send_more_text("CTRL");

            keyin = getchm(keymap);
            // return control-key, if there is one.
            keyin = _control_safe(_numpad2vi(keyin));
            webtiles_send_more_text("");
        }
        else if (_check_numpad(keyin, '/', keymap))
        {
            unwind_cursor saved(1, crawl_view.msgsz.y, GOTO_MSG);
            cprintf("SHIFT");
            webtiles_send_more_text("SHIFT");

            keyin = getchm(keymap);
            // return shift-key
            keyin = toupper_safe(_numpad2vi(keyin));
            webtiles_send_more_text("");
        }
    }

    // [dshaligram] More lovely keypad mangling.
    switch (keyin)
    {
#ifdef UNIX
    case '1': return 'b';
    case '2': return 'j';
    case '3': return 'n';
    case '4': return 'h';
    case '6': return 'l';
    case '7': return 'y';
    case '8': return 'k';
    case '9': return 'u';

# ifndef USE_TILE_LOCAL
    default: return unixcurses_get_vi_key(keyin);
# endif

#else
    case '1': return 'B';
    case '2': return 'J';
    case '3': return 'N';
    case '4': return 'H';
    case '6': return 'L';
    case '7': return 'Y';
    case '8': return 'K';
    case '9': return 'U';
#endif
    }

    return keyin;
}

// Wrapper around cgotoxy that can draw a fake cursor for Unix terms where
// cursoring over darkgrey or black causes problems.
void cursorxy(int x, int y)
{
#ifdef USE_TILE_LOCAL
    coord_def ep(x, y);
    coord_def gc = crawl_view.screen2grid(ep);
    tiles.place_cursor(CURSOR_MOUSE, gc);
#endif

#ifndef USE_TILE_LOCAL
#if defined(UNIX)
    if (Options.use_fake_cursor)
        fakecursorxy(x, y);
    else
        cgotoxy(x, y, GOTO_CRT);
#else
    cgotoxy(x, y, GOTO_CRT);
#endif
#endif
}

// cprintf that stops outputting when wrapped
void nowrap_eol_cprintf(const char *s, ...)
{
    const int wrapcol = get_number_of_cols() - 1;

    va_list args;
    va_start(args, s);
    string buf = vmake_stringf(s, args);
    va_end(args);

    cprintf("%s", chop_string(buf, max(wrapcol + 1 - wherex(), 0), false).c_str());
}

/**
 * Print a string wrapped by some value, relative to the current cursor region,
 * potentially skipping some lines. This function truncates if the region has
 * no more space. It is guaranteed to leave the cursor position in a valid spot
 * relative to the region.
 *
 * @param skiplines how many lines of text in `buf` to skip before printing
 * anything. (No output will be displayed for these lines.)
 * @param wrapcol the column to wrap at. The new line will start at the left
 * edge of the current cursor region after wrapping.
 * @param buf the string to print.
 */
static void wrapcprint_skipping(int skiplines, int wrapcol, const string &buf)
{
    ASSERT(skiplines >= 0);

#ifndef USE_TILE_LOCAL
    assert_valid_cursor_pos();
#endif
    const GotoRegion region = get_cursor_region();
    const coord_def sz = cgetsize(region);

    size_t linestart = 0;
    size_t len = buf.length();

    bool linebreak = false;

    while (linestart < len)
    {
        const coord_def pos = cgetpos(region);

        const int avail = wrapcol - pos.x + 1;
        if (avail > 0)
        {
            const string line = chop_string(buf.c_str() + linestart, avail, false);
            if (line.length() == 0)
                linebreak = true; // buf begins with a widechar, cursor is at the edge
            else
            {
                linestart += line.length();
                if (skiplines == 0)
                    cprintf("%s", line.c_str());

                linebreak = skiplines == 0
                            && line.length() >= static_cast<unsigned int>(avail);
            }
        }
        else
            linebreak = true; // cursor started at the end of a line

        // No room for more lines, quit now. As long as a linebreak happens
        // whenever a write fails above, this should prevent infinite loops.
        if (pos.y >= sz.y)
        {
#ifndef USE_TILE_LOCAL
            // leave the cursor at the end of the region to ensure a valid pos.
            // This could happen for example from printing something right up
            // to the end of the mlist.
            if (!valid_cursor_pos(cgetpos(region).x, pos.y, region))
                cgotoxy(sz.x, sz.y, region);
#endif
            break;
        }

        // even if the function returns, this will leave the cursor in a valid
        // position inside the region.
        if (linebreak)
            cgotoxy(1, pos.y + 1, region);

        if (skiplines)
            --skiplines;
    }
}

// cprintf that knows how to wrap down lines
void wrapcprintf(int wrapcol, const char *s, ...)
{
    va_list args;
    va_start(args, s);
    string buf = vmake_stringf(s, args);
    va_end(args);
    wrapcprint_skipping(0, wrapcol, buf);
}

/**
 * Print a string wrapped by some value, relative to the current cursor region,
 * potentially skipping some lines. This function truncates if the region has
 * no more space. It is guaranteed to leave the cursor position in a valid spot
 * relative to the region, and uses the width of the region to determine the
 * wrap column.
 *
 * @param s a format string
 * @param ... formatting parameters
 */
void wrapcprintf(const char *s, ...)
{
    va_list args;
    va_start(args, s);
    string buf = vmake_stringf(s, args);
    va_end(args);
    wrapcprint_skipping(0, cgetsize(get_cursor_region()).x, buf);
}

int cancellable_get_line(char *buf, int len, input_history *mh,
                        keyfun_action (*keyproc)(int &ch), const string &fill,
                        const string &tag)
{
    UNUSED(tag);

    flush_prev_message();

    mouse_control mc(MOUSE_MODE_PROMPT);
    line_reader reader(buf, len, get_number_of_cols());
    reader.set_input_history(mh);
    reader.set_keyproc(keyproc);
#ifdef USE_TILE_WEB
    reader.set_tag(tag);
#endif

    return reader.read_line(fill);
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

void input_history::new_input(const string &s)
{
    history.remove(s);

    if (history.size() == maxsize)
        history.pop_front();

    history.push_back(s);

    // Force the iterator to the end (also revalidates it)
    go_end();
}

const string *input_history::prev()
{
    if (history.empty())
        return nullptr;

    if (pos == history.begin())
        pos = history.end();

    return &*--pos;
}

const string *input_history::next()
{
    if (history.empty())
        return nullptr;

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

keyfun_action keyfun_num_and_char(int &ch)
{
    if (ch == CK_BKSP || isadigit(ch) || (unsigned)ch >= 128)
        return KEYFUN_PROCESS;

    return KEYFUN_BREAK;
}

draw_colour::draw_colour(COLOURS fg, COLOURS bg)
    : foreground(fg), background(bg)
{
    set();
}

draw_colour::~draw_colour()
{
    reset();
}

void draw_colour::reset()
{
    // Assume, following the menu code, that this is always the reset set.
    // TODO: version that saves initial state? this is kind of hard to get, actually.
    if (foreground != COLOUR_INHERIT)
        textcolour(LIGHTGRAY);
    if (background != COLOUR_INHERIT)
        textbackground(BLACK);
}

void draw_colour::set()
{
    if (foreground != COLOUR_INHERIT)
        textcolour(foreground);
    if (background != COLOUR_INHERIT)
        textbackground(background);
}

/////////////////////////////////////////////////////////////////////////
// line_reader

line_reader::line_reader(char *buf, size_t sz, int wrap)
    : buffer(buf), bufsz(sz), history(nullptr), region(GOTO_CRT),
      start(coord_def(-1,-1)), keyfn(nullptr), wrapcol(wrap),
      mode(EDIT_MODE_INSERT), fg_colour(COLOUR_INHERIT),
      bg_colour(COLOUR_INHERIT),
      cur(nullptr), length(0), pos(-1)
{
}

line_reader::~line_reader()
{
}

string line_reader::get_text() const
{
    return buffer;
}

void line_reader::set_text(string text)
{
    snprintf(buffer, bufsz, "%s", text.c_str());
    length = min(text.size(), bufsz);
    cur = buffer + length;
    pos = length;
}

void line_reader::set_input_history(input_history *i)
{
    history = i;
}

void line_reader::set_keyproc(keyproc fn)
{
    keyfn = fn;
}

void line_reader::set_edit_mode(edit_mode m)
{
    mode = m;
}

void line_reader::set_colour(COLOURS fg, COLOURS bg)
{
    fg_colour = fg;
    bg_colour = bg;
}

string line_reader::get_prompt()
{
    return prompt;
}

void line_reader::set_prompt(string p)
{
    prompt = p;
}

void line_reader::set_location(coord_def loc)
{
    start = loc;
}

edit_mode line_reader::get_edit_mode()
{
    return mode;
}

#ifdef USE_TILE_WEB
void line_reader::set_tag(const string &id)
{
    tag = id;
}
#endif

void line_reader::cursorto(int ncx)
{
    int x = (start.x + ncx - 1) % wrapcol + 1;
    int y = start.y + (start.x + ncx - 1) / wrapcol;

    if (y < 1)
    {
        // Cursor would go above the visible area. "Scroll" backwards so that
        // it goes on the top line, and redraw.
        start.y += 1 - y;
        const int skip = max(0, 1 - start.y);
        // If the beginning of the buffer becomes visible, paint over
        // the place where the prompt used to be. FIXME: It would be nice
        // to remember and display the visible part of the prompt.
        if (skip == 0)
        {
            cgotoxy(1, start.y + skip, region);
            cprintf("%*s", start.x - 1, "");
        }
        else
            cgotoxy(start.x, start.y + skip, region);
        wrapcprint_skipping(skip, wrapcol, buffer);
        y = 1;
    }

    int diff = y - cgetsize(region).y;
    if (diff > 0)
    {
        // There's no space left in the region, so we scroll it.
        // XXX: cscroll only implemented for GOTO_MSG.
        cscroll(diff, region);
        start.y -= diff;
        y -= diff;

        const int skip = max(0, 1 - start.y);

        cgotoxy(start.x, start.y + skip, region);
        wrapcprint_skipping(skip, wrapcol, buffer);
    }
    cgotoxy(x, y, region);
}

#ifdef USE_TILE_WEB
static void _webtiles_abort_get_line()
{
    tiles.json_open_object();
    tiles.json_write_string("msg", "close_input");
    tiles.json_close_object();
    tiles.finish_message();
}
#endif

int line_reader::getkey()
{
    return getchm();
}

int line_reader::read_line_core(bool reset_cursor)
{
    length = strlen(buffer);
    int width = strwidth(buffer);

    // Remember the previous cursor position, if valid.
    if (reset_cursor)
        pos = 0;
    else if (pos < 0 || pos > width)
        pos = width;

    cur = buffer;
    // XX shared code with calc_pos
    int cpos = 0;
    while (*cur && cpos < pos)
    {
        char32_t c;
        int s = utf8towc(&c, cur);
        cur += s;
        cpos += wcwidth(c);
    }

    if (length)
        print_segment();

    if (pos != width)
        cursorto(pos);

    if (history)
        history->go_end();

    int ret;
    do
    {
        ret = process_key_core(getkey());
    }
    while (ret == -1);
    return ret;
}

int line_reader::process_key_core(int ch)
{
    // Don't return a partial string if a HUP signal interrupted things
    if (crawl_state.seen_hups)
    {
        buffer[0] = '\0';
        return 0;
    }
    ch = numpad_to_regular(ch); // is this overkill?

    if (keyfn)
    {
        // if you intercept esc, don't forget to provide another way to
        // exit. Processing esc will safely cancel.
        keyfun_action whattodo = (*keyfn)(ch);
        if (whattodo == KEYFUN_CLEAR)
        {
            buffer[length] = 0;
            if (history && length)
                history->new_input(buffer);
            return 0;
        }
        else if (whattodo == KEYFUN_BREAK)
        {
            buffer[length] = 0;
            return ch;
        }
        else if (whattodo == KEYFUN_IGNORE)
            return -1;
        // else case: KEYFUN_PROCESS
    }

    return process_key(ch);
}


int line_reader::read_line(const string &prefill)
{
    strncpy(buffer, prefill.c_str(), bufsz);
    // Just in case it was too long.
    buffer[bufsz - 1] = '\0';
    return read_line(false);
}

/**
 * Read a line of input.
 *
 * @param clear_previous whether to clear the buffer before reading, or not.
 *                       Can be used to set a prefill string.
 * @param reset_cursor if true, start with the cursor at the left edge of the
 *                      buffer string, otherwise put the cursor at the right
 *                      edge or previous position (if any).
 *                      If the buffer is empty or `clear_previous` is true,
 *                      has no impact.
 *                      For webtiles, this corresponds with whether the
 *                      prefill starts selected or not.
 * @return 0 on success, otherwise, the last character read.
 */
int line_reader::read_line(bool clear_previous, bool reset_cursor)
{
    if (bufsz <= 0)
        return false;

    if (clear_previous)
        *buffer = 0;

#if defined(USE_TILE_LOCAL) && defined(TOUCH_UI)
    if (wm)
        wm->show_keyboard();
#endif

#ifdef USE_TILE_WEB
    tiles.redraw();
    tiles.json_open_object();
    tiles.json_write_string("msg", "init_input");
    if (tiles.is_in_crt_menu())
        tiles.json_write_string("type", "generic");
    else
        tiles.json_write_string("type", "messages");
    if (!tag.empty())
        tiles.json_write_string("tag", tag);
    if (history)
    {
        tiles.json_write_string("historyId",
                                make_stringf("%p", (void *)history));
    }
    tiles.json_write_string("prefill", buffer);
    tiles.json_write_bool("select_prefill", reset_cursor);
    if (prompt.length())
        tiles.json_write_string("prompt", prompt);
    tiles.json_write_int("maxlen", (int) bufsz - 1);
    tiles.json_write_int("size", (int) min(bufsz - 1, strlen(buffer) + 15));
    tiles.json_close_object();
    tiles.finish_message();
#endif

    cursor_control con(true);
    draw_colour draw(fg_colour, bg_colour);

    region = get_cursor_region();
    if (start.x < 0)
        start = cgetpos(region); // inherit location from cursor
    else
        cgotoxy(start.x, start.y, region);

    int ret = read_line_core(reset_cursor);

#ifdef USE_TILE_WEB
    _webtiles_abort_get_line();
#endif

    return ret;
}

/**
 * (Re-)print the buffer from start onwards, potentially overprinting
 * with spaces. Does *not* set cursor position.
 *
 * @param start the position in the buffer to print from.
 * @param how many spaces to overprint.
 */
void line_reader::print_segment(int start_point, int overprint)
{
    start_point = min(start_point, length);
    overprint = max(overprint, 0);

    wrapcprintf(wrapcol, "%s%*s", buffer + start_point, overprint, "");
}

void line_reader::backspace()
{
    if (!pos)
        return;

    char *np = prev_glyph(cur, buffer);
    ASSERT(np);
    char32_t ch;
    utf8towc(&ch, np);
    buffer[length] = 0;
    const int glyph_width = wcwidth(ch);
    length -= cur - np;
    char *c = cur;
    cur = np;
    while (*c)
        *np++ = *c++;
    calc_pos();

    cursorto(pos);
    buffer[length] = 0;

    // have to account for double-wide characters here
    // TODO: properly handle the case where deleting moves a
    // double-wide char up a line.
    print_segment(cur - buffer, glyph_width);
    cursorto(pos);
}

bool line_reader::is_wordchar(char32_t c)
{
    return iswalnum(c) || c == '_' || c == '-';
}

void line_reader::kill_to_begin()
{
    if (!pos || cur == buffer)
        return;

    const int rest = length - (cur - buffer);
    const int overwrite_len = pos;

    memmove(buffer, cur, rest);
    length = rest;
    buffer[length] = 0;
    pos = 0;
    cur = buffer;
    // TODO: calculate strwidth of deleted text for more accurate
    // overwriting.
    cursorto(pos);
    print_segment(0, overwrite_len);
    cursorto(pos);
}

void line_reader::killword()
{
    if (cur == buffer)
        return;

    bool foundwc = false;
    char *word = cur;
    int ew = 0;
    while (1)
    {
        char *np = prev_glyph(word, buffer);
        if (!np)
            break;

        char32_t c;
        utf8towc(&c, np);
        if (is_wordchar(c))
            foundwc = true;
        else if (foundwc)
            break;

        word = np;
        ew += wcwidth(c);
    }
    memmove(word, cur, strlen(cur) + 1);
    length -= cur - word;
    cur = word;
    calc_pos();

    cursorto(0);
    print_segment(0, ew);
    cursorto(pos);
}

void line_reader::calc_pos()
{
    int p = 0;
    const char *cp = buffer;
    char32_t c;
    int s;
    int pos_on_line = start.x;
    while (cp < cur && (s = utf8towc(&c, cp)))
    {
        const int c_width = wcwidth(c);
        if (pos_on_line + c_width >= wrapcol + 1)
        {
            if (pos_on_line + c_width > wrapcol + 1)
                p += 1; // account for early wrapping for a wide char
            pos_on_line = 1;
            continue;
        }
        cp += s;
        p += c_width;
        pos_on_line += c_width;
    }
    pos = p;
}

void line_reader::overwrite_char_at_cursor(int ch)
{
    int len = wclen(ch);
    int w = wcwidth(ch);

    if (w >= 0 && cur - buffer + len < static_cast<int>(bufsz))
    {
        bool empty = !*cur;

        wctoutf8(cur, ch);
        cur += len;
        if (empty)
            length += len;
        buffer[length] = 0;
        pos += w;
        cursorto(0);
        print_segment();
        cursorto(pos);
    }
}

void line_reader::insert_char_at_cursor(int ch)
{
    if (wcwidth(ch) >= 0 && length + wclen(ch) < static_cast<int>(bufsz))
    {
        int w = wcwidth(ch);
        int len = wclen(ch);
        if (*cur)
        {
            char *c = buffer + length - 1;
            while (c >= cur)
            {
                c[len] = *c;
                c--;
            }
        }
        wctoutf8(cur, ch);
        cur += len;
        length += len;
        buffer[length] = 0;
        pos += w;
        cursorto(0);
        print_segment();
        calc_pos();
        cursorto(pos);
    }
}

int line_reader::process_key(int ch)
{

    switch (ch)
    {
    CASE_ESCAPE
        return CK_ESCAPE;
    case CK_UP:
    case CONTROL('P'):
    case CK_DOWN:
    case CONTROL('N'):
    {
        if (!history)
            break;

        const string *text = (ch == CK_UP || ch == CONTROL('P'))
                             ? history->prev()
                             : history->next();

        if (text)
        {
            int olen = strwidth(buffer);
            length = text->length();
            if (length >= static_cast<int>(bufsz))
                length = bufsz - 1;
            memcpy(buffer, text->c_str(), length);
            buffer[length] = 0;
            cur = buffer + length;
            calc_pos();
            cursorto(0);

            int clear = pos < olen ? olen - pos : 0;
            print_segment(0, clear);

            calc_pos();
            cursorto(pos);
        }
        break;
    }
    case CK_ENTER:
        buffer[length] = 0;
        if (history && length)
            history->new_input(buffer);
        return 0;

    case CONTROL('K'):
    {
        // Kill to end of line.
        if (*cur)
        {
            int erase = strwidth(cur);
            length = cur - buffer;
            *cur = 0;
            print_segment(length, erase); // only overprint
            calc_pos();
            cursorto(pos);
        }
        break;
    }
    case CK_DELETE:
    case CONTROL('D'):
        // TODO: unify with backspace
        if (*cur)
        {
            const char *np = next_glyph(cur);
            ASSERT(np);
            char32_t ch_at_point;
            utf8towc(&ch_at_point, cur);
            const int glyph_width = wcwidth(ch_at_point);
            const size_t del_bytes = np - cur;
            const size_t follow_bytes = (buffer + length) - np;
            // Copy the NUL too.
            memmove(cur, np, follow_bytes + 1);
            length -= del_bytes;

            cursorto(pos);
            print_segment(cur - buffer, glyph_width);
            calc_pos();
            cursorto(pos);
        }
        break;

    case CK_BKSP:
        backspace();
        break;

    case CONTROL('W'):
        killword();
        break;

    case CONTROL('U'):
        kill_to_begin();
        break;

    case CK_LEFT:
    case CONTROL('B'):
        if (char *np = prev_glyph(cur, buffer))
        {
            cur = np;
            calc_pos();
            cursorto(pos);
        }
        break;
    case CK_RIGHT:
    case CONTROL('F'):
        if (char *np = next_glyph(cur))
        {
            cur = np;
            calc_pos();
            cursorto(pos);
        }
        break;
    case CK_HOME:
    case CONTROL('A'):
        pos = 0;
        cur = buffer;
        calc_pos();
        cursorto(pos);
        break;
    case CK_END:
    case CONTROL('E'):
        cur = buffer + length;
        calc_pos();
        cursorto(pos);
        break;
    case CK_MOUSE_CLICK:
        // FIXME: ought to move cursor to click location, if it's within the input
        return -1;
    case CK_REDRAW:
        redraw_screen();
        update_screen();
        return -1;
    default:
        if (mode == EDIT_MODE_OVERWRITE)
            overwrite_char_at_cursor(ch);
        else // mode == EDIT_MODE_INSERT
            insert_char_at_cursor(ch);

        break;
    }

    return -1;
}

#ifdef USE_TILE_LOCAL
fontbuf_line_reader::fontbuf_line_reader(char *buf, size_t buf_size,
            FontBuffer& font_buf, int wrap_col) :
    line_reader(buf, buf_size, wrap_col), m_font_buf(font_buf)
{
}

int fontbuf_line_reader::getkey()
{
    return ui::getch();
}

int fontbuf_line_reader::read_line(bool clear_previous, bool reset_cursor)
{
    if (bufsz <= 0)
        return false;

    if (clear_previous)
        *buffer = 0;

#if defined(USE_TILE_LOCAL) && defined(TOUCH_UI)
    if (wm)
        wm->show_keyboard();
#endif

    cursor_control con(true);

    m_font_buf.clear();
    m_font_buf.draw();

    return read_line_core(reset_cursor);

}

void fontbuf_line_reader::print_segment(int start_point, int overprint)
{
    start_point = min(start_point, length);
    overprint = max(overprint, 0);

    m_font_buf.clear();
    m_font_buf.add(make_stringf("%s%*s", buffer, overprint, ""),
        term_colours[(fg_colour == COLOUR_INHERIT ? LIGHTGRAY : fg_colour)],
        start.x, start.y);
    m_font_buf.draw();
}


void fontbuf_line_reader::cursorto(int newcpos)
{
    // TODO: does not support multi-line readers
    newcpos = min(max(newcpos, 0), length);

    // TODO: does this technique get flashing on some screens b/c of the double draw?
    print_segment(0, 0);

    char32_t c;
    float pos_x, pos_y;

    if (newcpos >= length)
        c = ' ';
    utf8towc(&c, buffer + newcpos);

    pos_y = start.y;
    string preface(buffer, newcpos);
    pos_x = start.x + m_font_buf.get_font_wrapper().string_width(preface.c_str());

    // redraw with a cursor
    m_font_buf.get_font_wrapper().store(m_font_buf, pos_x, pos_y, c,
        term_colours[LIGHTGRAY], term_colours[DARKGRAY]);
    m_font_buf.draw();
}


#endif // USE_TILE_LOCAL


/////////////////////////////////////////////////////////////////////////////
// Of mice and other mice.

static queue<c_mouse_event> mouse_events;

c_mouse_event get_mouse_event()
{
    if (mouse_events.empty())
        return c_mouse_event();

    c_mouse_event ce = mouse_events.front();
    mouse_events.pop();
    return ce;
}

void new_mouse_event(const c_mouse_event &ce)
{
    mouse_events.push(ce);
}

static void _flush_mouse_events()
{
    while (!mouse_events.empty())
        mouse_events.pop();
}

void c_input_reset(bool enable_mouse, bool flush)
{
    crawl_state.mouse_enabled = (enable_mouse && Options.mouse_input);
    set_mouse_enabled(crawl_state.mouse_enabled);

    if (flush)
        _flush_mouse_events();
}
