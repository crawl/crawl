/**
 * @file
 * @brief Functions used to print messages.
**/

#include "AppHdr.h"

#include "message.h"

#include <sstream>

#include "areas.h"
#include "colour.h"
#include "delay.h"
#include "hints.h"
#include "initfile.h"
#include "libutil.h"
#ifdef WIZARD
 #include "luaterp.h"
#endif
#include "menu.h"
#include "monster.h"
#include "mon-util.h"
#include "notes.h"
#include "output.h"
#include "religion.h"
#include "scroller.h"
#include "sound.h"
#include "state.h"
#include "stringutil.h"
#include "tiles-build-specific.h"
#include "unwind.h"
#include "view.h"

static void _mpr(string text, msg_channel_type channel=MSGCH_PLAIN, int param=0,
                 bool nojoin=false, bool cap=true);

void mpr(const string &text)
{
    _mpr(text);
}

void mpr_nojoin(msg_channel_type channel, string text)
{
    _mpr(text, channel, 0, true);
}

static bool _ends_in_punctuation(const string& text)
{
    if (text.size() == 0)
        return false;
    switch (text[text.size() - 1])
    {
    case '.':
    case '!':
    case '?':
    case ',':
    case ';':
    case ':':
        return true;
    default:
        return false;
    }
}

struct message_particle
{
    string text;        /// text of message (tagged string...)
    int repeats;        /// Number of times the message is in succession (x2)

    string pure_text() const
    {
        return formatted_string::parse_string(text).tostring();
    }

    string with_repeats() const
    {
        // TODO: colour the repeats indicator?
        string rep = "";
        if (repeats > 1)
            rep = make_stringf(" x%d", repeats);
        return text + rep;
    }

    string pure_text_with_repeats() const
    {
        string rep = "";
        if (repeats > 1)
            rep = make_stringf(" x%d", repeats);
        return pure_text() + rep;
    }

    /**
     * If this is followed by another message particle on the same line,
     * should there be a semicolon between them?
     */
    bool needs_semicolon() const
    {
        return repeats > 1 || !_ends_in_punctuation(pure_text());
    }
};

struct message_line
{
    msg_channel_type    channel;        // message channel
    int                 param;          // param for channel (god, enchantment)
    vector<message_particle> messages;  // a set of possibly-repeated messages
    int                 turn;
    bool                join;          /// may we merge this message w/others?

    message_line() : channel(NUM_MESSAGE_CHANNELS), param(0), turn(-1),
                     join(true)
    {
    }

    message_line(string msg, msg_channel_type chan, int par, bool jn)
     : channel(chan), param(par), turn(you.num_turns)
    {
        messages = { { msg, 1 } };
        // Don't join long messages.
        join = jn && strwidth(last_msg().pure_text()) < 40;
    }

    // Constructor for restored messages.
    message_line(string text, msg_channel_type chan, int par, int trn)
     : channel(chan), param(par), messages({{ text, 1 }}), turn(trn),
       join(false)
    {
    }

    operator bool() const
    {
        return !messages.empty();
    }

    const message_particle& last_msg() const
    {
        return messages.back();
    }

    // Tries to condense the argument into this message.
    // Either *this needs to be an empty item, or it must be the
    // same as the argument.
    bool merge(const message_line& other)
    {
        if (! *this)
        {
            *this = other;
            return true;
        }
        if (!other)
            return true;


        if (crawl_state.game_is_arena())
            return false; // dangerous for hacky code (looks at EOL for '!'...)
        if (!Options.msg_condense_repeats)
            return false;
        if (other.channel != channel || other.param != param)
            return false;
        if (other.messages.size() > 1)
        {
            return false; // not gonna try to handle this complexity
                          // shouldn't come up...
        }

        if (Options.msg_condense_repeats
            && other.last_msg().text == last_msg().text)
        {
            messages.back().repeats += other.last_msg().repeats;
            return true;
        }
        else if (Options.msg_condense_short
                 && turn == other.turn
                 && join && other.join
                 && _ends_in_punctuation(last_msg().pure_text())
                  == _ends_in_punctuation(other.last_msg().pure_text()))
            // punct check is a hack to avoid pickup messages merging with
            // combat on the same turn - should find a nicer heuristic
        {
            // "; " or " "?
            const int seplen = last_msg().needs_semicolon() ? 2 : 1;
            const int total_len = pure_len() + seplen + other.pure_len();
            if (total_len > (int)msgwin_line_length())
                return false;

            // merge in other's messages; they'll be delimited when printing.
            messages.insert(messages.end(),
                            other.messages.begin(), other.messages.end());
            return true;
        }

        return false;
    }

    /// What's the length of the actual combined text of the particles, not
    /// including non-rendering text (<red> etc)?
    int pure_len() const
    {
        // could we do this more functionally?
        int len = 0;
        for (auto &msg : messages)
        {
            if (len > 0) // not first msg
                len += msg.needs_semicolon() ? 2 : 1; // " " vs "; "
            len += strwidth(msg.pure_text_with_repeats());
        }
        return len;
    }

    /// The full string, with elements joined as appropriate.
    string full_text() const
    {
        string text = "";
        bool needs_semicolon = false;
        for (auto &msg : messages)
        {
            if (!text.empty())
            {
                text += make_stringf("<lightgrey>%s </lightgrey>",
                                     needs_semicolon ? ";" : "");
            }
            text += msg.with_repeats();
            needs_semicolon = msg.needs_semicolon();
        }
        return text;
    }

    string pure_text_with_repeats() const
    {
        return formatted_string::parse_string(full_text()).tostring();
    }
};

static int _mod(int num, int denom)
{
    ASSERT(denom > 0);
    div_t res = div(num, denom);
    return res.rem >= 0 ? res.rem : res.rem + denom;
}

template <typename T, int SIZE>
class circ_vec
{
    T data[SIZE];

    int end;   // first unfilled index
    bool has_circled;
    // TODO: properly track the tail, and make this into a real data
    // structure with an iterator and whatnot

    static void inc(int* index)
    {
        ASSERT_RANGE(*index, 0, SIZE);
        *index = _mod(*index + 1, SIZE);
    }

    static void dec(int* index)
    {
        ASSERT_RANGE(*index, 0, SIZE);
        *index = _mod(*index - 1, SIZE);
    }

public:
    circ_vec() : end(0), has_circled(false) {}

    void clear()
    {
        end = 0;
        has_circled = false;
        for (int i = 0; i < SIZE; ++i)
            data[i] = T();
    }

    int size() const
    {
        return SIZE;
    }

    int filled_size() const
    {
        if (has_circled)
            return SIZE;
        else
            return end;
    }

    T& operator[](int i)
    {
        ASSERT(_mod(i, SIZE) < size());
        return data[_mod(end + i, SIZE)];
    }

    const T& operator[](int i) const
    {
        ASSERT(_mod(i, SIZE) < size());
        return data[_mod(end + i, SIZE)];
    }

    void push_back(const T& item)
    {
        data[end] = item;
        inc(&end);
        if (end == 0)
            has_circled = true;
    }

    void roll_back(int n)
    {
        for (int i = 0; i < n; ++i)
        {
            dec(&end);
            data[end] = T();
        }
        // don't bother to worry about has_circled in this case
        // TODO: properly track the tail
    }

    /**
     * Append the contents of `buf` to the current buffer.
     * If `buf` has cycled, this will overwrite the entire contents of `this`.
     */
    void append(const circ_vec<T, SIZE> buf)
    {
        const int buf_size = buf.filled_size();
        for (int i = 0; i < buf_size; i++)
            push_back(buf[i - buf_size]);
    }
};

static void readkey_more(bool user_forced=false);

// Types of message prefixes.
// Higher values override lower.
enum class prefix_type
{
    none,
    turn_start,
    turn_end,
    new_cmd, // new command, but no new turn
    new_turn,
    full_more,   // single-character more prompt (full window)
    other_more,  // the other type of --more-- prompt
};

// Could also go with coloured glyphs.
static cglyph_t _prefix_glyph(prefix_type p)
{
    cglyph_t g;
    switch (p)
    {
    case prefix_type::turn_start:
        g.ch = Options.show_newturn_mark ? '-' : ' ';
        g.col = LIGHTGRAY;
        break;
    case prefix_type::turn_end:
    case prefix_type::new_turn:
        g.ch = Options.show_newturn_mark ? '_' : ' ';
        g.col = LIGHTGRAY;
        break;
    case prefix_type::new_cmd:
        g.ch = Options.show_newturn_mark ? '_' : ' ';
        g.col = DARKGRAY;
        break;
    case prefix_type::full_more:
        g.ch = '+';
        g.col = channel_to_colour(MSGCH_PROMPT);
        break;
    case prefix_type::other_more:
        g.ch = '+';
        g.col = LIGHTRED;
        break;
    default:
        g.ch = ' ';
        g.col = LIGHTGRAY;
        break;
    }
    return g;
}

static bool _pre_more();

static bool _temporary = false;

class message_window
{
    int next_line;
    int temp_line;     // starting point of temporary messages
    int input_line;    // last line-after-input
    vector<formatted_string> lines;
    prefix_type prompt; // current prefix prompt

    int height() const
    {
        return crawl_view.msgsz.y;
    }

    int use_last_line() const
    {
        return first_col_more();
    }

    int width() const
    {
        return crawl_view.msgsz.x;
    }

    void out_line(const formatted_string& line, int n) const
    {
        cgotoxy(1, n + 1, GOTO_MSG);
        line.display();
        cprintf("%*s", width() - line.width(), "");
    }

    // Place cursor at end of last non-empty line to handle prompts.
    // TODO: might get rid of this by clearing the whole window when writing,
    //       and then just writing the actual non-empty lines.
    void place_cursor()
    {
        // XXX: the screen may have resized since the last time we
        //  called lines.resize(). Consider only the last height()
        //  lines if this has happened.
        const int diff = max(int(lines.size()) - height(), 0);

        int i;
        for (i = lines.size() - 1; i >= diff && lines[i].width() == 0; --i)
            ;
        if (i >= diff)
        {
            // If there was room, put the cursor at the end of that line.
            // Otherwise, put it at the beginning of the next line.
            if ((int) lines[i].width() < crawl_view.msgsz.x)
                cgotoxy(lines[i].width() + 1, i - diff + 1, GOTO_MSG);
            else if (i - diff + 2 <= height())
                cgotoxy(1, i - diff + 2, GOTO_MSG);
            else
            {
                // Scroll to make room for the next line, then redraw.
                scroll(1);
                // Results in a recursive call to place_cursor!  But scroll()
                // made lines[height()] empty, so that recursive call shouldn't
                // hit this case again.
                show();
                return;
            }
        }
        else
        {
            // If there were no lines, put the cursor at the upper left.
            cgotoxy(1, 1, GOTO_MSG);
        }
    }

    // Whether to show msgwin-full more prompts.
    bool more_enabled() const
    {
        return crawl_state.show_more_prompt
               && (Options.clear_messages || Options.show_more);
    }

    int make_space(int n)
    {
        int space = out_height() - next_line;

        if (space >= n)
            return 0;

        int s = 0;
        if (input_line > 0)
        {
            s = min(input_line, n - space);
            scroll(s);
            space += s;
        }

        if (space >= n)
            return s;

        if (more_enabled())
            more(true);

        // We could consider just scrolling off after --more--;
        // that would require marking the last message before
        // the prompt.
        if (!Options.clear_messages && !more_enabled())
        {
            scroll(n - space);
            return s + n - space;
        }
        else
        {
            clear();
            return height();
        }
    }

    void add_line(const formatted_string& line)
    {
        resize(); // TODO: get rid of this
        lines[next_line] = line;
        next_line++;
    }

    void output_prefix(prefix_type p)
    {
        if (!use_first_col())
            return;
        if (p <= prompt)
            return;
        prompt = p;
        if (next_line > 0)
        {
            formatted_string line;
            line.add_glyph(_prefix_glyph(prompt));
            lines[next_line-1].del_char();
            line += lines[next_line-1];
            lines[next_line-1] = line;
        }
        show();
    }

public:
    message_window()
        : next_line(0), temp_line(0), input_line(0), prompt(prefix_type::none)
    {
        clear_lines(); // initialize this->lines
    }

    void resize()
    {
        // XXX: broken (why?)
        lines.resize(height());
    }

    unsigned int out_width() const
    {
        return width() - (use_first_col() ? 1 : 0);
    }

    unsigned int out_height() const
    {
        return height() - (use_last_line() ? 0 : 1);
    }

    void clear_lines()
    {
        lines.clear();
        lines.resize(height());
    }

    bool first_col_more() const
    {
        return Options.small_more;
    }

    bool use_first_col() const
    {
        return !Options.clear_messages;
    }

    void set_starting_line()
    {
        // TODO: start at end (sometimes?)
        next_line = 0;
        input_line = 0;
        temp_line = 0;
    }

    void clear()
    {
        clear_lines();
        set_starting_line();
        show();
    }

    void scroll(int n)
    {
        // We might be asked to scroll off everything by the line reader.
        if (next_line < n)
            n = next_line;

        int i;
        for (i = 0; i < height() - n; ++i)
            lines[i] = lines[i + n];
        for (; i < height(); ++i)
            lines[i].clear();
        next_line -= n;
        temp_line -= n;
        input_line -= n;
    }

    // write to screen (without refresh)
    void show()
    {
        // XXX: this should not be necessary as formatted_string should
        //      already do it
        textcolour(LIGHTGREY);

        // XXX: the screen may have resized since the last time we
        //  called lines.resize(). Consider only the last height()
        //  lines if this has happened.
        const int diff = max(int(lines.size()) - height(), 0);

        for (size_t i = diff; i < lines.size(); ++i)
            out_line(lines[i], i - diff);
        place_cursor();
#ifdef USE_TILE
        tiles.set_need_redraw();
#endif
    }

    // temporary: to be overwritten with next item, e.g. new turn
    //            leading dash or prompt without response
    void add_item(string text, prefix_type first_col = prefix_type::none,
                  bool temporary = false)
    {
        prompt = prefix_type::none; // reset prompt

        vector<formatted_string> newlines;
        linebreak_string(text, out_width());
        formatted_string::parse_string_to_multiple(text, newlines);

        for (const formatted_string &nl : newlines)
        {
            make_space(1);
            formatted_string line;
            if (use_first_col())
                line.add_glyph(_prefix_glyph(first_col));
            line += nl;
            add_line(line);
        }

        if (!temporary)
            reset_temp();

        show();
    }

    void roll_back()
    {
        temp_line = max(temp_line, 0);
        for (int i = temp_line; i < next_line; ++i)
            lines[i].clear();
        next_line = temp_line;
    }

    /**
     * Consider any formerly-temporary messages permanent.
     */
    void reset_temp()
    {
        temp_line = next_line;
    }

    void got_input()
    {
        input_line = next_line;
    }

    void new_cmdturn(bool new_turn)
    {
        output_prefix(new_turn ? prefix_type::new_turn : prefix_type::new_cmd);
    }

    bool any_messages()
    {
        return next_line > input_line;
    }

    /*
     * Handling of more prompts (both types).
     */
    void more(bool full, bool user=false)
    {
        rng::generator rng(rng::UI);

        if (_pre_more())
            return;

        if (you.running)
        {
            mouse_control mc(MOUSE_MODE_MORE);
            redraw_screen();
            update_screen();
        }
        else
        {
            print_stats();
            update_screen();
            show();
        }

        int last_row = crawl_view.msgsz.y;
        if (first_col_more())
        {
            cgotoxy(1, last_row, GOTO_MSG);
            cglyph_t g = _prefix_glyph(full ? prefix_type::full_more : prefix_type::other_more);
            formatted_string f;
            f.add_glyph(g);
            f.display();
            // Move cursor back for nicer display.
            cgotoxy(1, last_row, GOTO_MSG);
            // Need to read_key while cursor_control in scope.
            cursor_control con(true);
            readkey_more();
        }
        else
        {
            cgotoxy(use_first_col() ? 2 : 1, last_row, GOTO_MSG);
            textcolour(channel_to_colour(MSGCH_PROMPT));
            if (crawl_state.game_is_hints())
            {
                string more_str = "--more-- Press Space ";
                if (is_tiles())
                    more_str += "or click ";
                more_str += "to continue. You can later reread messages with "
                            "Ctrl-P.";
                cprintf(more_str.c_str());
            }
            else
                cprintf("--more--");

            readkey_more(user);
        }
    }
};

message_window msgwin;

void display_message_window()
{
    msgwin.show();
}

void clear_message_window()
{
    msgwin = message_window();
}

void scroll_message_window(int n)
{
    msgwin.scroll(n);
    msgwin.show();
}

bool any_messages()
{
    return msgwin.any_messages();
}

typedef circ_vec<message_line, NUM_STORED_MESSAGES> store_t;

class message_store
{
    store_t msgs;
    message_line prev_msg;
    bool last_of_turn;
    int temp; // number of temporary messages

#ifdef USE_TILE_WEB
    int unsent; // number of messages not yet sent to the webtiles client
    int client_rollback;
    bool send_ignore_one;
#endif

public:
    message_store() : last_of_turn(false), temp(0)
#ifdef USE_TILE_WEB
                      , unsent(0), client_rollback(0), send_ignore_one(false)
#endif
    {}

    void add(const message_line& msg)
    {
        string orig_full_text = msg.full_text();

        if (!(msg.channel != MSGCH_PROMPT && prev_msg.merge(msg)))
        {
            flush_prev();
            prev_msg = msg;
            if (msg.channel == MSGCH_PROMPT || _temporary)
                flush_prev();
            }

            // If we play sound, wait until the corresponding message is printed
            // in case we intend on holding up output that comes after.
            //
            // FIXME This doesn't work yet, and causes the game to play the sound,
            // THEN display the text. This appears to only be solvable by reworking
            // the way the game outputs messages, as the game it prints messages
            // one line at a time, not one message at a time.
            //
            // However, it should only print one message at a time when it really
            // needs to, i.e. an sound that interrupts the game. Otherwise it is
            // more efficent to print text together.
#ifdef USE_SOUND
            play_sound(check_sound_patterns(orig_full_text));
#endif
    }

    void store_msg(const message_line& msg)
    {
        prefix_type p = prefix_type::none;
        msgs.push_back(msg);
        if (_temporary)
            temp++;
        else
            reset_temp();
#ifdef USE_TILE_WEB
        // ignore this message until it's actually displayed in case we run out
        // of space and have to display --more-- instead
        unwind_bool dontsend(send_ignore_one, true);
#endif
        if (crawl_state.io_inited && crawl_state.game_started)
            msgwin.add_item(msg.full_text(), p, _temporary);
    }

    void roll_back()
    {
#ifdef USE_TILE_WEB
        client_rollback = max(0, temp - unsent);
        unsent = max(0, unsent - temp);
#endif
        msgs.roll_back(temp);
        temp = 0;
    }

    void reset_temp()
    {
        temp = 0;
    }

    void flush_prev()
    {
        if (!prev_msg)
            return;
        message_line msg = prev_msg;
        // Clear prev_msg before storing it, since
        // writing out to the message window might
        // in turn result in a recursive flush_prev.
        prev_msg = message_line();
#ifdef USE_TILE_WEB
        unsent++;
#endif
        store_msg(msg);
        if (last_of_turn)
        {
            msgwin.new_cmdturn(true);
            last_of_turn = false;
        }
    }

    void new_turn()
    {
        if (prev_msg)
            last_of_turn = true;
        else
            msgwin.new_cmdturn(true);
    }

    // XXX: this should not need to exist
    const store_t& get_store()
    {
        return msgs;
    }

    void append_store(store_t store)
    {
        msgs.append(store);
        const int msgs_to_print = store.filled_size();
#ifdef USE_TILE_WEB
        unwind_bool dontsend(send_ignore_one, true);
#endif
        for (int i = 0; i < msgs_to_print; i++)
            msgwin.add_item(msgs[i - msgs_to_print].full_text(), prefix_type::none, false);
    }

    void clear()
    {
        msgs.clear();
        prev_msg = message_line();
        last_of_turn = false;
        temp = 0;
#ifdef USE_TILE_WEB
        unsent = 0;
#endif
    }

#ifdef USE_TILE_WEB
    void send()
    {
        if (unsent == 0 || (send_ignore_one && unsent == 1)) return;

        if (client_rollback > 0)
        {
            tiles.json_write_int("rollback", client_rollback);
            client_rollback = 0;
        }
        tiles.json_open_array("messages");
        for (int i = -unsent; i < (send_ignore_one ? -1 : 0); ++i)
        {
            message_line& msg = msgs[i];
            tiles.json_open_object();
            tiles.json_write_string("text", msg.full_text());
            tiles.json_write_int("turn", msg.turn);
            tiles.json_write_int("channel", msg.channel);
            tiles.json_close_object();
        }
        tiles.json_close_array();
        unsent = send_ignore_one ? 1 : 0;
    }
#endif
};

// Circular buffer for keeping past messages.
message_store buffer;

#ifdef USE_TILE_WEB
bool _more = false, _last_more = false;

void webtiles_send_messages()
{
    // defer sending any messages to client in this form until a game is
    // started up. It's still possible to send them as a popup. When this is
    // eventually called, it'll send any queued messages.
    if (!crawl_state.io_inited || !crawl_state.game_started)
        return;
    tiles.json_open_object();
    tiles.json_write_string("msg", "msgs");
    tiles.json_treat_as_empty();
    if (_more != _last_more)
    {
        tiles.json_write_bool("more", _more);
        _last_more = _more;
    }
    buffer.send();
    tiles.json_close_object(true);
    tiles.finish_message();
}
#else
void webtiles_send_messages() { }
#endif

static FILE* _msg_dump_file = nullptr;

static msg_colour_type prepare_message(const string& imsg,
                                       msg_channel_type channel,
                                       int param,
                                       bool allow_suppress=true);


namespace msg
{
    static bool suppress_messages = false;
    static unordered_set<tee *> current_message_tees;
    static maybe_bool _msgs_to_stderr = MB_MAYBE;

    static bool _suppressed()
    {
        return suppress_messages;
    }

    /**
     * RAII logic for controlling echoing to stderr.
     * @param f the new state:
     *   MB_TRUE: always echo to stderr (mainly used for debugging)
     *   MB_MAYBE: use default logic, based on mode, io state, etc
     *   MB_FALSE: never echo to stderr (for suppressing error echoing during
     *             startup, e.g. for first-pass initfile processing)
     */
    force_stderr::force_stderr(maybe_bool f)
        : prev_state(_msgs_to_stderr)
    {
        _msgs_to_stderr = f;
    }

    force_stderr::~force_stderr()
    {
        _msgs_to_stderr = prev_state;
    }


    bool uses_stderr(msg_channel_type channel)
    {
        if (_msgs_to_stderr == MB_TRUE)
            return true;
        else if (_msgs_to_stderr == MB_FALSE)
            return false;
        // else, MB_MAYBE:

        if (channel == MSGCH_ERROR)
        {
            return !crawl_state.io_inited // one of these is not like the others
                || crawl_state.test || crawl_state.script
                || crawl_state.build_db
                || crawl_state.map_stat_gen || crawl_state.obj_stat_gen;
        }
        return false;
    }

    tee::tee()
        : target(nullptr)
    {
        current_message_tees.insert(this);
    }

    tee::tee(string &_target)
        : target(&_target)
    {
        current_message_tees.insert(this);
    }

    tee::~tee()
    {
        if (target)
            *target += get_store();
        current_message_tees.erase(this);
    }

    void tee::append(const string &s, msg_channel_type /*ch*/)
    {
        // could use a more c++y external interface -- but that just complicates things
        store << s;
    }

    void tee::append_line(const string &s, msg_channel_type ch)
    {
        append(s + "\n", ch);
    }

    string tee::get_store() const
    {
        return store.str();
    }

    static void _append_to_tees(const string &s, msg_channel_type ch)
    {
        for (auto tee : current_message_tees)
            tee->append(s, ch);
    }

    suppress::suppress()
        : msuppressed(suppress_messages),
          channel(NUM_MESSAGE_CHANNELS),
          prev_colour(MSGCOL_NONE)
    {
        suppress_messages = true;
    }

    // Push useful RAII conditional logic into a constructor
    // Won't override an outer suppressing msg::suppress
    suppress::suppress(bool really_suppress)
        : msuppressed(suppress_messages),
          channel(NUM_MESSAGE_CHANNELS),
          prev_colour(MSGCOL_NONE)
    {
        suppress_messages = suppress_messages || really_suppress;
    }

    // Mute just one channel. Mainly useful for hiding debug spam in various
    // circumstances.
    suppress::suppress(msg_channel_type _channel)
        : msuppressed(suppress_messages),
          channel(_channel),
          prev_colour(Options.channels[channel])
    {
        // don't change global suppress_messages for this case
        ASSERT(channel < NUM_MESSAGE_CHANNELS);
        Options.channels[channel] = MSGCOL_MUTED;
    }

    suppress::~suppress()
    {
        suppress_messages = msuppressed;
        if (channel < NUM_MESSAGE_CHANNELS)
            Options.channels[channel] = prev_colour;
    }
}

msg_colour_type msg_colour(int col)
{
    return static_cast<msg_colour_type>(col);
}

static int colour_msg(msg_colour_type col)
{
    if (col == MSGCOL_MUTED)
        return DARKGREY;
    else
        return static_cast<int>(col);
}

// Returns a colour or MSGCOL_MUTED.
static msg_colour_type channel_to_msgcol(msg_channel_type channel, int param)
{
    msg_colour_type ret;

    switch (Options.channels[channel])
    {
    case MSGCOL_PLAIN:
        // Note that if the plain channel is muted, then we're protecting
        // the player from having that spread to other channels here.
        // The intent of plain is to give non-coloured messages, not to
        // suppress them.
        if (Options.channels[MSGCH_PLAIN] >= MSGCOL_DEFAULT)
            ret = MSGCOL_LIGHTGREY;
        else
            ret = Options.channels[MSGCH_PLAIN];
        break;

    case MSGCOL_DEFAULT:
    case MSGCOL_ALTERNATE:
        switch (channel)
        {
        case MSGCH_GOD:
            ret = (Options.channels[channel] == MSGCOL_DEFAULT)
                   ? msg_colour(god_colour(static_cast<god_type>(param)))
                   : msg_colour(god_message_altar_colour(static_cast<god_type>(param)));
            break;

        case MSGCH_DURATION:
            ret = MSGCOL_LIGHTBLUE;
            break;

        case MSGCH_DANGER:
            ret = MSGCOL_RED;
            break;

        case MSGCH_WARN:
        case MSGCH_ERROR:
            ret = MSGCOL_LIGHTRED;
            break;

        case MSGCH_INTRINSIC_GAIN:
            ret = MSGCOL_GREEN;
            break;

        case MSGCH_RECOVERY:
            ret = MSGCOL_LIGHTGREEN;
            break;

        case MSGCH_TALK:
        case MSGCH_TALK_VISUAL:
        case MSGCH_HELL_EFFECT:
            ret = MSGCOL_WHITE;
            break;

        case MSGCH_MUTATION:
        case MSGCH_MONSTER_WARNING:
            ret = MSGCOL_LIGHTRED;
            break;

        case MSGCH_MONSTER_SPELL:
        case MSGCH_MONSTER_ENCHANT:
        case MSGCH_FRIEND_SPELL:
        case MSGCH_FRIEND_ENCHANT:
            ret = MSGCOL_LIGHTMAGENTA;
            break;

        case MSGCH_TUTORIAL:
        case MSGCH_ORB:
        case MSGCH_BANISHMENT:
            ret = MSGCOL_MAGENTA;
            break;

        case MSGCH_MONSTER_DAMAGE:
            ret =  ((param == MDAM_DEAD)               ? MSGCOL_RED :
                    (param >= MDAM_SEVERELY_DAMAGED)   ? MSGCOL_LIGHTRED :
                    (param >= MDAM_MODERATELY_DAMAGED) ? MSGCOL_YELLOW
                                                       : MSGCOL_LIGHTGREY);
            break;

        case MSGCH_PROMPT:
            ret = MSGCOL_CYAN;
            break;

        case MSGCH_DIAGNOSTICS:
        case MSGCH_MULTITURN_ACTION:
            ret = MSGCOL_DARKGREY; // makes it easier to ignore at times -- bwr
            break;

        case MSGCH_PLAIN:
        case MSGCH_FRIEND_ACTION:
        case MSGCH_ROTTEN_MEAT:
        case MSGCH_EQUIPMENT:
        case MSGCH_EXAMINE:
        case MSGCH_EXAMINE_FILTER:
        case MSGCH_DGL_MESSAGE:
        default:
            ret = param > 0 ? msg_colour(param) : MSGCOL_LIGHTGREY;
            break;
        }
        break;

    case MSGCOL_MUTED:
        ret = MSGCOL_MUTED;
        break;

    default:
        // Setting to a specific colour is handled here, special
        // cases should be handled above.
        if (channel == MSGCH_MONSTER_DAMAGE)
        {
            // A special case right now for monster damage (at least until
            // the init system is improved)... selecting a specific
            // colour here will result in only the death messages coloured.
            if (param == MDAM_DEAD)
                ret = Options.channels[channel];
            else if (Options.channels[MSGCH_PLAIN] >= MSGCOL_DEFAULT)
                ret = MSGCOL_LIGHTGREY;
            else
                ret = Options.channels[MSGCH_PLAIN];
        }
        else
            ret = Options.channels[channel];
        break;
    }

    return ret;
}

int channel_to_colour(msg_channel_type channel, int param)
{
    return colour_msg(channel_to_msgcol(channel, param));
}

void do_message_print(msg_channel_type channel, int param, bool cap,
                             bool nojoin, const char *format, va_list argp)
{
    va_list ap;
    va_copy(ap, argp);
    char buff[200];
    size_t len = vsnprintf(buff, sizeof(buff), format, argp);
    if (len < sizeof(buff))
        _mpr(buff, channel, param, nojoin, cap);
    else
    {
        char *heapbuf = (char*)malloc(len + 1);
        vsnprintf(heapbuf, len + 1, format, ap);
        _mpr(heapbuf, channel, param, nojoin, cap);
        free(heapbuf);
    }
    va_end(ap);
}

void mprf_nocap(msg_channel_type channel, int param, const char *format, ...)
{
    va_list argp;
    va_start(argp, format);
    do_message_print(channel, param, false, false, format, argp);
    va_end(argp);
}

void mprf_nocap(msg_channel_type channel, const char *format, ...)
{
    va_list argp;
    va_start(argp, format);
    do_message_print(channel, channel == MSGCH_GOD ? you.religion : 0,
                     false, false, format, argp);
    va_end(argp);
}

void mprf_nocap(const char *format, ...)
{
    va_list argp;
    va_start(argp, format);
    do_message_print(MSGCH_PLAIN, 0, false, false, format, argp);
    va_end(argp);
}

void mprf(msg_channel_type channel, int param, const char *format, ...)
{
    va_list argp;
    va_start(argp, format);
    do_message_print(channel, param, true, false, format, argp);
    va_end(argp);
}

void mprf(msg_channel_type channel, const char *format, ...)
{
    va_list argp;
    va_start(argp, format);
    do_message_print(channel, channel == MSGCH_GOD ? you.religion : 0,
                     true, false, format, argp);
    va_end(argp);
}

void mprf(const char *format, ...)
{
    va_list argp;
    va_start(argp, format);
    do_message_print(MSGCH_PLAIN, 0, true, false, format, argp);
    va_end(argp);
}

void mprf_nojoin(msg_channel_type channel, const char *format, ...)
{
    va_list argp;
    va_start(argp, format);
    do_message_print(channel, channel == MSGCH_GOD ? you.religion : 0,
                     true, true, format, argp);
    va_end(argp);
}

void mprf_nojoin(const char *format, ...)
{
    va_list argp;
    va_start(argp, format);
    do_message_print(MSGCH_PLAIN, 0, true, true, format, argp);
    va_end(argp);
}

#ifdef DEBUG_DIAGNOSTICS
void dprf(const char *format, ...)
{
    if (Options.quiet_debug_messages[DIAG_NORMAL] || you.suppress_wizard)
        return;

    va_list argp;
    va_start(argp, format);
    do_message_print(MSGCH_DIAGNOSTICS, 0, false, false, format, argp);
    va_end(argp);
}

void dprf(diag_type param, const char *format, ...)
{
    if (Options.quiet_debug_messages[param] || you.suppress_wizard)
        return;

    va_list argp;
    va_start(argp, format);
    do_message_print(MSGCH_DIAGNOSTICS, param, false, false, format, argp);
    va_end(argp);
}
#endif

static bool _updating_view = false;

static bool _check_option(const string& line, msg_channel_type channel,
                          const vector<message_filter>& option)
{
    if (crawl_state.generating_level)
        return false;
    return any_of(begin(option),
                  end(option),
                  bind(mem_fn(&message_filter::is_filtered),
                       placeholders::_1, channel, line));
}

static bool _check_more(const string& line, msg_channel_type channel)
{
    // Try to avoid mores during level excursions, they are glitchy at best.
    // TODO: this is sort of an emergency check, possibly it should
    // crash here in order to find the real bug?
    if (!you.on_current_level)
        return false;
    return _check_option(line, channel, Options.force_more_message);
}

static bool _check_flash_screen(const string& line, msg_channel_type channel)
{
    // absolutely never flash during a level excursion, things will go very
    // badly. TODO: this is sort of an emergency check, possibly it should
    // crash here in order to find the real bug?
    if (!you.on_current_level)
        return false;
    return _check_option(line, channel, Options.flash_screen_message);
}

static bool _check_join(const string& /*line*/, msg_channel_type channel)
{
    switch (channel)
    {
    case MSGCH_EQUIPMENT:
        return false;
    default:
        break;
    }
    return true;
}

static void _debug_channel_arena(msg_channel_type channel)
{
    switch (channel)
    {
    case MSGCH_PROMPT:
    case MSGCH_GOD:
    case MSGCH_DURATION:
    case MSGCH_RECOVERY:
    case MSGCH_INTRINSIC_GAIN:
    case MSGCH_MUTATION:
    case MSGCH_ROTTEN_MEAT:
    case MSGCH_EQUIPMENT:
    case MSGCH_FLOOR_ITEMS:
    case MSGCH_MULTITURN_ACTION:
    case MSGCH_EXAMINE:
    case MSGCH_EXAMINE_FILTER:
    case MSGCH_ORB:
    case MSGCH_TUTORIAL:
        die("Invalid channel '%s' in arena mode",
                 channel_to_str(channel).c_str());
        break;
    default:
        break;
    }
}

bool strip_channel_prefix(string &text, msg_channel_type &channel, bool silence)
{
    string::size_type pos = text.find(":");
    if (pos == string::npos)
        return false;

    string param = text.substr(0, pos);
    bool sound = false;

    if (param == "WARN")
        channel = MSGCH_WARN, sound = true;
    else if (param == "VISUAL WARN")
        channel = MSGCH_WARN;
    else if (param == "SOUND")
        channel = MSGCH_SOUND, sound = true;
    else if (param == "VISUAL")
        channel = MSGCH_TALK_VISUAL;
    else if (param == "SPELL")
        channel = MSGCH_MONSTER_SPELL, sound = true;
    else if (param == "VISUAL SPELL")
        channel = MSGCH_MONSTER_SPELL;
    else if (param == "ENCHANT")
        channel = MSGCH_MONSTER_ENCHANT, sound = true;
    else if (param == "VISUAL ENCHANT")
        channel = MSGCH_MONSTER_ENCHANT;
    else
    {
        param = replace_all(param, " ", "_");
        lowercase(param);
        int ch = str_to_channel(param);
        if (ch == -1)
            return false;
        channel = static_cast<msg_channel_type>(ch);
    }

    if (sound && silence)
        text = "";
    else
        text = text.substr(pos + 1);
    return true;
}

void msgwin_set_temporary(bool temp)
{
    flush_prev_message();
    _temporary = temp;
    if (!temp)
    {
        buffer.reset_temp();
        msgwin.reset_temp();
    }
}

void msgwin_clear_temporary()
{
    buffer.roll_back();
    msgwin.roll_back();
}

static int _last_msg_turn = -1; // Turn of last message.

static void _mpr(string text, msg_channel_type channel, int param, bool nojoin,
                 bool cap)
{
    rng::generator rng(rng::UI);

    if (_msg_dump_file != nullptr)
        fprintf(_msg_dump_file, "%s\n", text.c_str());

    if (crawl_state.game_crashed)
        return;

    if (crawl_state.game_is_valid_type() && crawl_state.game_is_arena())
        _debug_channel_arena(channel);

#ifdef DEBUG_FATAL
    if (channel == MSGCH_ERROR)
        die_noline("%s", text.c_str());
#endif

    if (msg::uses_stderr(channel))
        fprintf(stderr, "%s\n", text.c_str());

    // Flush out any "comes into view" monster announcements before the
    // monster has a chance to give any other messages.
    if (!_updating_view && crawl_state.io_inited)
    {
        _updating_view = true;
        flush_comes_into_view();
        _updating_view = false;
    }

    if (channel == MSGCH_GOD && param == 0)
        param = you.religion;

    // Ugly hack.
    if (channel == MSGCH_DIAGNOSTICS || channel == MSGCH_ERROR)
        cap = false;

    // if the message would be muted, handle any tees before bailing. The
    // actual color for MSGCOL_MUTED ends up as darkgrey in any tees.
    msg_colour_type colour = prepare_message(text, channel, param);

    string col = colour_to_str(colour_msg(colour));
    text = "<" + col + ">" + text + "</" + col + ">"; // XXX

    msg::_append_to_tees(text + "\n", channel);

    if (colour == MSGCOL_MUTED && crawl_state.io_inited)
    {
        if (channel == MSGCH_PROMPT)
            msgwin.show();
        return;
    }

    bool domore = _check_more(text, channel);
    bool do_flash_screen = _check_flash_screen(text, channel);
    bool join = !domore && !nojoin && _check_join(text, channel);

    // Must do this before converting to formatted string and back;
    // that doesn't preserve close tags!

    formatted_string fs = formatted_string::parse_string(text);

    // TODO: this kind of check doesn't really belong in logging code...
    if (you.duration[DUR_QUAD_DAMAGE])
        fs.all_caps(); // No sound, so we simulate the reverb with all caps.
    else if (cap)
        fs.capitalise();
    if (channel != MSGCH_ERROR && channel != MSGCH_DIAGNOSTICS)
        fs.filter_lang();
    text = fs.to_colour_string();

    message_line msg = message_line(text, channel, param, join);
    buffer.add(msg);

    if (!crawl_state.io_inited)
        return;

    _last_msg_turn = msg.turn;

    if (channel == MSGCH_ERROR)
        interrupt_activity(activity_interrupt::force);

    if (channel == MSGCH_PROMPT || channel == MSGCH_ERROR)
        set_more_autoclear(false);

    if (domore)
        more(true);

    if (do_flash_screen)
        flash_view_delay(UA_ALWAYS_ON, YELLOW, 50);

}

static string show_prompt(string prompt)
{
    mprf(MSGCH_PROMPT, "%s", prompt.c_str());

    // FIXME: duplicating mpr code.
    msg_colour_type colour = prepare_message(prompt, MSGCH_PROMPT, 0);
    return colour_string(prompt, colour_msg(colour));
}

static string _prompt;
void msgwin_prompt(string prompt)
{
    msgwin_set_temporary(true);
    _prompt = show_prompt(prompt);
}

void msgwin_reply(string reply)
{
    msgwin_clear_temporary();
    msgwin_set_temporary(false);
    reply = replace_all(reply, "<", "<<");
    mprf(MSGCH_PROMPT, "%s<lightgrey>%s</lightgrey>", _prompt.c_str(), reply.c_str());
    msgwin.got_input();
}

void msgwin_got_input()
{
    msgwin.got_input();
}

int msgwin_get_line(string prompt, char *buf, int len,
                    input_history *mh, const string &fill)
{
#ifdef TOUCH_UI
    bool use_popup = true;
#else
    bool use_popup = !crawl_state.need_save || ui::has_layout();
#endif

    int ret;
    if (use_popup)
    {
        mouse_control mc(MOUSE_MODE_PROMPT);

        linebreak_string(prompt, 79);
        msg_colour_type colour = prepare_message(prompt, MSGCH_PROMPT, 0);
        const auto colour_prompt = formatted_string(prompt, colour_msg(colour));

        bool done = false;
        auto vbox = make_shared<ui::Box>(ui::Widget::VERT);
        auto popup = make_shared<ui::Popup>(vbox);

        vbox->add_child(make_shared<ui::Text>(colour_prompt + "\n"));

        auto input = make_shared<ui::TextEntry>();
        input->set_sync_id("input");
        input->set_text(fill);
        input->set_input_history(mh);
#ifndef USE_TILE_LOCAL
        input->max_size().width = 20;
#endif
        vbox->add_child(input);

        popup->on_hotkey_event([&](const ui::KeyEvent& ev) {
            switch (ev.key())
            {
            CASE_ESCAPE
                ret = CK_ESCAPE;
                return done = true;
            case CK_ENTER:
                ret = 0;
                return done = true;
            default:
                return done = false;
            }
        });

#ifdef USE_TILE_WEB
        tiles.json_open_object();
        tiles.json_write_string("prompt", colour_prompt.to_colour_string());
        tiles.push_ui_layout("msgwin-get-line", 0);
        popup->on_layout_pop([](){ tiles.pop_ui_layout(); });
#endif
        ui::run_layout(move(popup), done, input);

        strncpy(buf, input->get_text().c_str(), len - 1);
        buf[len - 1] = '\0';
    }
    else
    {
        if (!prompt.empty())
            msgwin_prompt(prompt);
        ret = cancellable_get_line(buf, len, mh, nullptr, fill);
        msgwin_reply(buf);
    }

    return ret;
}

void msgwin_new_turn()
{
    buffer.new_turn();
}

void msgwin_new_cmd()
{
#ifndef USE_TILE_LOCAL
    if (crawl_state.smallterm)
        return;
#endif

    flush_prev_message();
    bool new_turn = (you.num_turns > _last_msg_turn);
    msgwin.new_cmdturn(new_turn);
}

unsigned int msgwin_line_length()
{
    return msgwin.out_width();
}

unsigned int msgwin_lines()
{
    return msgwin.out_height();
}

// mpr() an arbitrarily long list of strings without truncation or risk
// of overflow.
void mpr_comma_separated_list(const string &prefix,
                              const vector<string> &list,
                              const string &andc,
                              const string &comma,
                              const msg_channel_type channel,
                              const int param)
{
    string out = prefix;

    for (int i = 0, size = list.size(); i < size; i++)
    {
        out += list[i];

        if (size > 0 && i < (size - 2))
            out += comma;
        else if (i == (size - 2))
            out += andc;
        else if (i == (size - 1))
            out += ".";
    }
    _mpr(out, channel, param);
}

// Checks whether a given message contains patterns relevant for
// notes, stop_running or sounds and handles these cases.
static void mpr_check_patterns(const string& message,
                               msg_channel_type channel,
                               int param)
{
    if (crawl_state.generating_level)
        return;
    for (const text_pattern &pat : Options.note_messages)
    {
        if (channel == MSGCH_EQUIPMENT || channel == MSGCH_FLOOR_ITEMS
            || channel == MSGCH_MULTITURN_ACTION
            || channel == MSGCH_EXAMINE || channel == MSGCH_EXAMINE_FILTER
            || channel == MSGCH_TUTORIAL || channel == MSGCH_DGL_MESSAGE)
        {
            continue;
        }

        if (pat.matches(message))
        {
            take_note(Note(NOTE_MESSAGE, channel, param, message));
            break;
        }
    }

    if (channel != MSGCH_DIAGNOSTICS && channel != MSGCH_EQUIPMENT)
    {
        interrupt_activity(activity_interrupt::message,
                           channel_to_str(channel) + ":" + message);
    }
}

static bool channel_message_history(msg_channel_type channel)
{
    switch (channel)
    {
    case MSGCH_PROMPT:
    case MSGCH_EQUIPMENT:
    case MSGCH_EXAMINE_FILTER:
        return false;
    default:
        return true;
    }
}

// Returns the default colour of the message, or MSGCOL_MUTED if
// the message should be suppressed.
static msg_colour_type prepare_message(const string& imsg,
                                       msg_channel_type channel,
                                       int param,
                                       bool allow_suppress)
{
    if (allow_suppress && msg::_suppressed())
        return MSGCOL_MUTED;

    if (you.num_turns > 0 && silenced(you.pos())
        && (channel == MSGCH_SOUND || channel == MSGCH_TALK))
    {
        return MSGCOL_MUTED;
    }

    msg_colour_type colour = channel_to_msgcol(channel, param);

    if (colour != MSGCOL_MUTED)
        mpr_check_patterns(imsg, channel, param);

    if (!crawl_state.generating_level)
    {
        for (const message_colour_mapping &mcm : Options.message_colour_mappings)
        {
            if (mcm.message.is_filtered(channel, imsg))
            {
                colour = mcm.colour;
                break;
            }
        }
    }

    return colour;
}

void flush_prev_message()
{
    buffer.flush_prev();
}

void clear_messages(bool force)
{
    if (!crawl_state.io_inited)
        return;
    // Unflushed message will be lost with clear_messages,
    // so they shouldn't really exist, but some of the delay
    // code appears to do this intentionally.
    // ASSERT(!buffer.have_prev());
    flush_prev_message();

    msgwin.got_input(); // Consider old messages as read.

    if (Options.clear_messages || force)
        msgwin.clear();

    // TODO: we could indicate indicate clear_messages with a different
    //       leading character than '-'.
}

static bool autoclear_more = false;

void set_more_autoclear(bool on)
{
    autoclear_more = on;
}

static void readkey_more(bool user_forced)
{
    if (autoclear_more)
        return;
    int keypress = 0;
#ifdef USE_TILE_WEB
    unwind_bool unwind_more(_more, true);
#endif
    mouse_control mc(MOUSE_MODE_MORE);

    do
    {
        keypress = getch_ck();
        if (keypress == CK_REDRAW)
        {
            redraw_screen();
            update_screen();
            continue;
        }
    }
    while (keypress != ' ' && keypress != '\r' && keypress != '\n'
           && !key_is_escape(keypress)
#ifdef TOUCH_UI
           && keypress != CK_MOUSE_CLICK);
#else
           && (user_forced || keypress != CK_MOUSE_CLICK));
#endif

    if (key_is_escape(keypress))
        set_more_autoclear(true);
}

/**
 * more() preprocessing.
 *
 * @return Whether the more prompt should be skipped.
 */
static bool _pre_more()
{
    if (crawl_state.game_crashed || crawl_state.seen_hups)
        return true;

#ifdef DEBUG_DIAGNOSTICS
    if (you.running)
        return true;
#endif

    if (crawl_state.game_is_arena())
    {
        delay(Options.view_delay);
        return true;
    }

    if (crawl_state.is_replaying_keys())
        return true;

#ifdef WIZARD
    if (luaterp_running())
        return true;
#endif

    if (!crawl_state.show_more_prompt || msg::_suppressed())
        return true;

    return false;
}

void more(bool user_forced)
{
    rng::generator rng(rng::UI);

    if (!crawl_state.io_inited)
        return;
    flush_prev_message();
    msgwin.more(false, user_forced);
    clear_messages();
}

void canned_msg(canned_message_type which_message)
{
    switch (which_message)
    {
        case MSG_SOMETHING_APPEARS:
            mprf("Something appears %s!",
                 player_has_feet() ? "at your feet" : "before you");
            break;
        case MSG_NOTHING_HAPPENS:
            mpr("Nothing appears to happen.");
            break;
        case MSG_YOU_UNAFFECTED:
            mpr("You are unaffected.");
            break;
        case MSG_YOU_RESIST:
            mpr("You resist.");
            learned_something_new(HINT_YOU_RESIST);
            break;
        case MSG_YOU_PARTIALLY_RESIST:
            mpr("You partially resist.");
            break;
        case MSG_TOO_BERSERK:
            mpr("You are too berserk!");
            crawl_state.cancel_cmd_repeat();
            break;
        case MSG_TOO_CONFUSED:
            mpr("You're too confused!");
            break;
        case MSG_PRESENT_FORM:
            mpr("You can't do that in your present form.");
            crawl_state.cancel_cmd_repeat();
            break;
        case MSG_NOTHING_CARRIED:
            mpr("You aren't carrying anything.");
            crawl_state.cancel_cmd_repeat();
            break;
        case MSG_CANNOT_DO_YET:
            mpr("You can't do that yet.");
            crawl_state.cancel_cmd_repeat();
            break;
        case MSG_OK:
            mprf(MSGCH_PROMPT, "Okay, then.");
            crawl_state.cancel_cmd_repeat();
            break;
        case MSG_UNTHINKING_ACT:
            mpr("Why would you want to do that?");
            crawl_state.cancel_cmd_repeat();
            break;
        case MSG_NOTHING_THERE:
            mpr("There's nothing there!");
            crawl_state.cancel_cmd_repeat();
            break;
        case MSG_NOTHING_CLOSE_ENOUGH:
            mpr("There's nothing close enough!");
            crawl_state.cancel_cmd_repeat();
            break;
        case MSG_SPELL_FIZZLES:
            mpr("The spell fizzles.");
            break;
        case MSG_HUH:
            mprf(MSGCH_EXAMINE_FILTER, "Huh?");
            crawl_state.cancel_cmd_repeat();
            break;
        case MSG_EMPTY_HANDED_ALREADY:
        case MSG_EMPTY_HANDED_NOW:
        {
            const char* when =
            (which_message == MSG_EMPTY_HANDED_ALREADY ? "already" : "now");
            if (you.species == SP_FELID)
                mprf("Your mouth is %s empty.", when);
            else if (you.has_usable_claws(true))
                mprf("You are %s empty-clawed.", when);
            else if (you.has_usable_tentacles(true))
                mprf("You are %s empty-tentacled.", when);
            else
                mprf("You are %s empty-handed.", when);
            break;
        }
        case MSG_YOU_BLINK:
            mpr("You blink.");
            break;
        case MSG_STRANGE_STASIS:
            mpr("You feel a strange sense of stasis.");
            break;
        case MSG_NO_SPELLS:
            mpr("You don't know any spells.");
            break;
        case MSG_MANA_INCREASE:
            mpr("You feel your magic capacity increase.");
            break;
        case MSG_MANA_DECREASE:
            mpr("You feel your magic capacity decrease.");
            break;
        case MSG_DISORIENTED:
            mpr("You feel momentarily disoriented.");
            break;
        case MSG_TOO_HUNGRY:
            mpr("You're too hungry.");
            break;
        case MSG_DETECT_NOTHING:
            mpr("You detect nothing.");
            break;
        case MSG_CALL_DEAD:
            mpr("You call on the dead to rise...");
            break;
        case MSG_ANIMATE_REMAINS:
            mpr("You attempt to give life to the dead...");
            break;
        case MSG_CANNOT_MOVE:
            mpr("You cannot move.");
            break;
        case MSG_YOU_DIE:
            mpr_nojoin(MSGCH_PLAIN, "You die...");
            break;
        case MSG_GHOSTLY_OUTLINE:
            mpr("You see a ghostly outline there, and the spell fizzles.");
            break;
        case MSG_FULL_HEALTH:
            mpr("Your health is already full.");
            break;
        case MSG_FULL_MAGIC:
            mpr("Your reserves of magic are already full.");
            break;
        case MSG_GAIN_HEALTH:
            mpr("You feel better.");
            break;
        case MSG_GAIN_MAGIC:
            mpr("You feel your power returning.");
            break;
        case MSG_MAGIC_DRAIN:
            mprf(MSGCH_WARN, "You suddenly feel drained of magical energy!");
            break;
        case MSG_SOMETHING_IN_WAY:
            mpr("There's something in the way.");
            break;
        case MSG_CANNOT_SEE:
            mpr("You can't see that place.");
            break;
        case MSG_GOD_DECLINES:
            mpr("Your god isn't willing to do this for you now.");
            break;
    }
}

// Note that this function *completely* blocks messaging for monsters
// distant or invisible to the player ... look elsewhere for a function
// permitting output of "It" messages for the invisible {dlb}
// Intentionally avoids info and str_pass now. - bwr
bool simple_monster_message(const monster& mons, const char *event,
                            msg_channel_type channel,
                            int param,
                            description_level_type descrip)
{
    if (you.see_cell(mons.pos())
        && (channel == MSGCH_MONSTER_SPELL || channel == MSGCH_FRIEND_SPELL
            || mons.visible_to(&you)))
    {
        string msg = mons.name(descrip);
        msg += event;

        if (channel == MSGCH_PLAIN && mons.wont_attack())
            channel = MSGCH_FRIEND_ACTION;

        mprf(channel, param, "%s", msg.c_str());
        return true;
    }

    return false;
}

string god_speaker(god_type which_deity)
{
    if (which_deity == GOD_WU_JIAN)
       return "The Council";
    else
       return uppercase_first(god_name(which_deity));
}

// yet another wrapper for mpr() {dlb}:
void simple_god_message(const char *event, god_type which_deity)
{
    string msg = god_speaker(which_deity) + event;

    god_speaks(which_deity, msg.c_str());
}

void wu_jian_sifu_message(const char *event)
{
    string msg;
    msg = uppercase_first(string("Sifu ") + wu_jian_random_sifu_name() + event);
    god_speaks(GOD_WU_JIAN, msg.c_str());
}

static bool is_channel_dumpworthy(msg_channel_type channel)
{
    return channel != MSGCH_EQUIPMENT
           && channel != MSGCH_DIAGNOSTICS
           && channel != MSGCH_TUTORIAL;
}

void clear_message_store()
{
    buffer.clear();
}

string get_last_messages(int mcount, bool full)
{
    flush_prev_message();

    string text;
    // XXX: should use some message_history iterator here
    const store_t& msgs = buffer.get_store();
    // XXX: loop wraps around otherwise. This could be done better.
    mcount = min(mcount, NUM_STORED_MESSAGES);
    for (int i = -1; mcount > 0; --i)
    {
        const message_line msg = msgs[i];
        if (!msg)
            break;
        if (full || is_channel_dumpworthy(msg.channel))
        {
            string line = msg.pure_text_with_repeats();
            string wrapped;
            while (!line.empty())
                wrapped += wordwrap_line(line, 79, false, true) + "\n";
            text = wrapped + text;
        }
        mcount--;
    }

    // An extra line of clearance.
    if (!text.empty())
        text += "\n";
    return text;
}

void get_recent_messages(vector<string> &mess,
                         vector<msg_channel_type> &chan)
{
    flush_prev_message();

    const store_t& msgs = buffer.get_store();
    int mcount = NUM_STORED_MESSAGES;
    for (int i = -1; mcount > 0; --i, --mcount)
    {
        const message_line msg = msgs[i];
        if (!msg)
            break;
        mess.push_back(msg.pure_text_with_repeats());
        chan.push_back(msg.channel);
    }
}

bool recent_error_messages()
{
    // TODO: track whether player has seen error messages so this can be
    // more generally useful?
    flush_prev_message();

    const store_t& msgs = buffer.get_store();
    int mcount = NUM_STORED_MESSAGES;
    for (int i = -1; mcount > 0; --i, --mcount)
    {
        const message_line msg = msgs[i];
        if (!msg)
            break;
        if (msg.channel == MSGCH_ERROR)
            return true;
    }
    return false;
}

// We just write out the whole message store including empty/unused
// messages. They'll be ignored when restoring.
void save_messages(writer& outf)
{
    store_t msgs = buffer.get_store();
    marshallInt(outf, msgs.size());
    for (int i = 0; i < msgs.size(); ++i)
    {
        marshallString4(outf, msgs[i].full_text());
        marshallInt(outf, msgs[i].channel);
        marshallInt(outf, msgs[i].param);
        marshallInt(outf, msgs[i].turn);
    }
}

void load_messages(reader& inf)
{
    unwind_bool save_more(crawl_state.show_more_prompt, false);

    // assumes that the store was cleared at the beginning of _restore_game!
    flush_prev_message();
    store_t load_msgs = buffer.get_store(); // copy of messages during loading
    clear_message_store();

    int num = unmarshallInt(inf);
    for (int i = 0; i < num; ++i)
    {
        string text;
        unmarshallString4(inf, text);

        msg_channel_type channel = (msg_channel_type) unmarshallInt(inf);
        int           param      = unmarshallInt(inf);
#if TAG_MAJOR_VERSION == 34
        if (inf.getMinorVersion() < TAG_MINOR_MESSAGE_REPEATS)
                                   unmarshallInt(inf); // was 'repeats'
#endif
        int           turn       = unmarshallInt(inf);

        message_line msg(message_line(text, channel, param, turn));
        if (msg)
            buffer.store_msg(msg);
    }
    flush_prev_message();
    buffer.append_store(load_msgs);
    clear_messages(); // check for Options.message_clear
}

static void _replay_messages_core(formatted_scroller &hist)
{
    flush_prev_message();

    const store_t msgs = buffer.get_store();
    formatted_string lines;
    for (int i = 0; i < msgs.size(); ++i)
        if (channel_message_history(msgs[i].channel))
        {
            string text = msgs[i].full_text();
            if (!text.size())
                continue;
            linebreak_string(text, cgetsize(GOTO_CRT).x - 1);
            vector<formatted_string> parts;
            formatted_string::parse_string_to_multiple(text, parts, 80);
            for (unsigned int j = 0; j < parts.size(); ++j)
            {
                prefix_type p = prefix_type::none;
                if (j == parts.size() - 1 && i + 1 < msgs.size()
                    && msgs[i+1].turn > msgs[i].turn)
                {
                    p = prefix_type::turn_end;
                }
                if (!lines.empty())
                    lines.add_glyph('\n');
                lines.add_glyph(_prefix_glyph(p));
                lines += parts[j];
            }
        }

    hist.add_formatted_string(lines);
    hist.show();
}

void replay_messages()
{
    formatted_scroller hist(FS_START_AT_END | FS_PREWRAPPED_TEXT);
    hist.set_more();

    _replay_messages_core(hist);
}

void replay_messages_during_startup()
{
    formatted_scroller hist(FS_PREWRAPPED_TEXT);
    hist.set_more();
    hist.set_more(formatted_string::parse_string(
            "<cyan>Press Esc to close, arrows/pgup/pgdn to scroll.</cyan>"));
    hist.set_title(formatted_string::parse_string(recent_error_messages()
        ? "<yellow>Crawl encountered errors during initialization:</yellow>"
        : "<yellow>Initialization log:</yellow>"));
    _replay_messages_core(hist);
}

void set_msg_dump_file(FILE* file)
{
    _msg_dump_file = file;
}

void formatted_mpr(const formatted_string& fs,
                   msg_channel_type channel, int param)
{
    _mpr(fs.to_colour_string(), channel, param);
}
